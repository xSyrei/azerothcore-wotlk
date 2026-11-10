// Emberforge Engine: Project Emberforge's own lighting engine for the WoW 3.3.5a client.
//
// It is the game's d3d9.dll: it loads Windows' real one, hands the game a device of its own that passes every call on,
// and so sees all the game draws. The game makes a "pure" device (it answers no questions about its state), so the
// engine keeps its own copy of everything the game sets: render targets, depth, render states, shaders (fingerprinted
// when created), textures, shader constants (the camera), the viewport.
//
// This version learns how the client draws: Home opens its panel (what it sees each frame), and "Capture one frame"
// writes every draw of the next frame to Emberforge_capture.txt in the game's folder. The lighting effects are rebuilt
// on those facts next.

#include <windows.h>
#include <initguid.h>   // defines the interface IDs d3d9.h declares
#include <d3d9.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

#include <imgui.h>
#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>

#include "forward.h"
#include "known_shaders.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define EMBERFORGE_ENGINE_VERSION "0.1"

namespace
{
    // ---- Windows' real d3d9.dll ----

    HMODULE RealModule()
    {
        static HMODULE module = [] {
            char path[MAX_PATH] = {};
            GetSystemDirectoryA(path, MAX_PATH);   // SysWOW64 for the 32-bit game on 64-bit Windows
            strcat(path, "\\d3d9.dll");
            return LoadLibraryA(path);
        }();
        return module;
    }

    template <typename F>
    F Real(const char *name)
    {
        HMODULE module = RealModule();
        return module ? reinterpret_cast<F>(reinterpret_cast<void *>(GetProcAddress(module, name))) : nullptr;
    }

    std::string GameFolder()
    {
        char path[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string folder = path;
        size_t slash = folder.find_last_of("\\/");
        return slash == std::string::npos ? std::string() : folder.substr(0, slash + 1);
    }

    void Log(const char *format, ...)
    {
        static FILE *file = fopen((GameFolder() + "Emberforge.log").c_str(), "w");
        if (!file)
            return;
        va_list args;
        va_start(args, format);
        vfprintf(file, format, args);
        va_end(args);
        fputc('\n', file);
        fflush(file);
    }

    uint32_t Fnv(const void *data, size_t size)
    {
        const uint8_t *bytes = static_cast<const uint8_t *>(data);
        uint32_t hash = 2166136261u;
        for (size_t i = 0; i < size; ++i)
            hash = (hash ^ bytes[i]) * 16777619u;
        return hash;
    }

    // A shader's bytecode length: tokens up to and including the end token (0x0000FFFF), skipping comments' contents.
    size_t BytecodeSize(const DWORD *code)
    {
        size_t i = 1;                                   // the version token
        while (code[i] != 0x0000FFFF)
        {
            DWORD token = code[i];
            if ((token & 0xFFFF) == 0xFFFE)             // a comment: its length in bits 16-30
                i += 1 + ((token >> 16) & 0x7FFF);
            else
                ++i;
            if (i > (1u << 20))                         // never runs away on bad data
                return 0;
        }
        return (i + 1) * sizeof(DWORD);
    }

    // ---- The device the game gets ----

    constexpr int kVertexConstants = 256, kPixelConstants = 32, kRenderStates = 256, kStages = 8;

    // How many of count registers from start fit below limit.
    UINT Clamp(UINT start, UINT count, int limit)
    {
        return start + count > static_cast<UINT>(limit) ? limit - start : count;
    }

    class Device;
    Device *g_device = nullptr;
    WNDPROC g_gameWndProc = nullptr;
    bool g_panel = false;

    LRESULT CALLBACK PanelWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        // Home belongs to the engine (read in Present): the game does not see it.
        if ((msg == WM_KEYDOWN || msg == WM_KEYUP) && wParam == VK_HOME)
            return 0;
        if (g_panel && ImGui::GetCurrentContext())
        {
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
            const ImGuiIO &io = ImGui::GetIO();
            bool mouse = msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST;
            bool keys = msg >= WM_KEYFIRST && msg <= WM_KEYLAST;
            if ((mouse && io.WantCaptureMouse) || (keys && io.WantCaptureKeyboard))
                return 0;
        }
        return CallWindowProcA(g_gameWndProc, hwnd, msg, wParam, lParam);
    }

    class Device final : public Device9Forward
    {
    public:
        Device(IDirect3DDevice9 *device, HWND window, DWORD behavior, const D3DPRESENT_PARAMETERS *params)
            : Device9Forward(device), m_window(window)
        {
            m_pure = (behavior & D3DCREATE_PUREDEVICE) != 0;
            g_device = this;
            Defaults(params);
            Log("device created: window %p, %s", static_cast<void *>(window), m_pure ? "pure" : "not pure");
        }

        ~Device() override
        {
            ShutdownPanel();
            if (g_device == this)
                g_device = nullptr;
        }

        // ---- Lifetime: our own count, so the panel's resources on the real device do not keep it alive ----

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **object) override
        {
            if (riid == IID_IUnknown || riid == IID_IDirect3DDevice9)
            {
                AddRef();
                *object = this;
                return S_OK;
            }
            return real->QueryInterface(riid, object);
        }

        ULONG STDMETHODCALLTYPE AddRef() override
        {
            return ++m_refs;
        }

        ULONG STDMETHODCALLTYPE Release() override
        {
            ULONG refs = --m_refs;
            if (refs == 0)
            {
                IDirect3DDevice9 *device = real;
                delete this;                             // frees the panel's resources first
                device->Release();
            }
            return refs;
        }

        // ---- State the game sets, kept here (a pure device answers no Get* calls) ----

        HRESULT STDMETHODCALLTYPE SetRenderTarget(DWORD index, IDirect3DSurface9 *surface) override
        {
            if (!m_recordingBlock && index < 4)
            {
                m_rt[index] = surface;
                ++m_frame.targetChanges;
                if (m_capture == Capture::Recording)
                    Write("target %lu = %s", static_cast<unsigned long>(index), Describe(surface).c_str());
            }
            return real->SetRenderTarget(index, surface);
        }

        HRESULT STDMETHODCALLTYPE SetDepthStencilSurface(IDirect3DSurface9 *surface) override
        {
            if (!m_recordingBlock)
            {
                m_ds = surface;
                if (m_capture == Capture::Recording)
                    Write("depth = %s", Describe(surface).c_str());
            }
            return real->SetDepthStencilSurface(surface);
        }

        HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE state, DWORD value) override
        {
            if (!m_recordingBlock && state < kRenderStates)
                m_rs[state] = value;
            return real->SetRenderState(state, value);
        }

        HRESULT STDMETHODCALLTYPE SetTexture(DWORD stage, IDirect3DBaseTexture9 *texture) override
        {
            if (!m_recordingBlock && stage < kStages)
                m_tex[stage] = texture;
            return real->SetTexture(stage, texture);
        }

        HRESULT STDMETHODCALLTYPE SetVertexShader(IDirect3DVertexShader9 *shader) override
        {
            if (!m_recordingBlock)
                m_vs = shader;
            return real->SetVertexShader(shader);
        }

        HRESULT STDMETHODCALLTYPE SetPixelShader(IDirect3DPixelShader9 *shader) override
        {
            if (!m_recordingBlock)
                m_ps = shader;
            return real->SetPixelShader(shader);
        }

        HRESULT STDMETHODCALLTYPE CreateVertexShader(const DWORD *code, IDirect3DVertexShader9 **shader) override
        {
            HRESULT hr = real->CreateVertexShader(code, shader);
            if (SUCCEEDED(hr) && code && shader && *shader)
                m_hashes[*shader] = Fnv(code, BytecodeSize(code));
            return hr;
        }

        HRESULT STDMETHODCALLTYPE CreatePixelShader(const DWORD *code, IDirect3DPixelShader9 **shader) override
        {
            HRESULT hr = real->CreatePixelShader(code, shader);
            if (SUCCEEDED(hr) && code && shader && *shader)
                m_hashes[*shader] = Fnv(code, BytecodeSize(code));
            return hr;
        }

        HRESULT STDMETHODCALLTYPE SetVertexShaderConstantF(UINT start, const float *data, UINT count) override
        {
            if (!m_recordingBlock && data && start < kVertexConstants)
                memcpy(m_vsc[start], data, sizeof(float) * 4 * Clamp(start, count, kVertexConstants));
            return real->SetVertexShaderConstantF(start, data, count);
        }

        HRESULT STDMETHODCALLTYPE SetPixelShaderConstantF(UINT start, const float *data, UINT count) override
        {
            if (!m_recordingBlock && data && start < kPixelConstants)
                memcpy(m_psc[start], data, sizeof(float) * 4 * Clamp(start, count, kPixelConstants));
            return real->SetPixelShaderConstantF(start, data, count);
        }

        HRESULT STDMETHODCALLTYPE SetViewport(const D3DVIEWPORT9 *viewport) override
        {
            if (!m_recordingBlock && viewport)
                m_viewport = *viewport;
            return real->SetViewport(viewport);
        }

        HRESULT STDMETHODCALLTYPE SetFVF(DWORD fvf) override
        {
            if (!m_recordingBlock)
                m_fvf = fvf;
            return real->SetFVF(fvf);
        }

        HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE type, const D3DMATRIX *matrix) override
        {
            if (!m_recordingBlock && matrix)
            {
                if (type == D3DTS_VIEW)
                    m_view = *matrix;
                else if (type == D3DTS_PROJECTION)
                    m_projection = *matrix;
                else if (type == D3DTS_WORLD)
                    m_world = *matrix;
            }
            return real->SetTransform(type, matrix);
        }

        // State blocks set state without the calls above: counted, so a capture shows whether the game uses them.
        HRESULT STDMETHODCALLTYPE BeginStateBlock() override
        {
            m_recordingBlock = true;
            return real->BeginStateBlock();
        }

        HRESULT STDMETHODCALLTYPE EndStateBlock(IDirect3DStateBlock9 **block) override
        {
            m_recordingBlock = false;
            ++m_frame.stateBlocks;
            return real->EndStateBlock(block);
        }

        HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE type, IDirect3DStateBlock9 **block) override
        {
            ++m_frame.stateBlocks;
            return real->CreateStateBlock(type, block);
        }

        // ---- Frame events ----

        HRESULT STDMETHODCALLTYPE Clear(DWORD count, const D3DRECT *rects, DWORD flags, D3DCOLOR color, float z,
                                        DWORD stencil) override
        {
            if (flags & D3DCLEAR_ZBUFFER)
                ++m_frame.depthClears;
            if (m_capture == Capture::Recording)
                Write("clear (before draw %u)%s%s%s colour %08lx depth %.3f, target %s, depth %s", m_frame.draws,
                        (flags & D3DCLEAR_TARGET) ? " target" : "", (flags & D3DCLEAR_ZBUFFER) ? " depth" : "",
                        (flags & D3DCLEAR_STENCIL) ? " stencil" : "", static_cast<unsigned long>(color), z,
                        Describe(m_rt[0]).c_str(), Describe(m_ds).c_str());
            return real->Clear(count, rects, flags, color, z, stencil);
        }

        HRESULT STDMETHODCALLTYPE BeginScene() override
        {
            ++m_frame.scenes;
            if (m_capture == Capture::Recording)
                Write("begin scene (before draw %u)", m_frame.draws);
            return real->BeginScene();
        }

        HRESULT STDMETHODCALLTYPE EndScene() override
        {
            if (m_capture == Capture::Recording)
                Write("end scene (after draw %u)", m_frame.draws);
            return real->EndScene();
        }

        HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE type, UINT start, UINT count) override
        {
            OnDraw("draw", type, count);
            return real->DrawPrimitive(type, start, count);
        }

        HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE type, INT base, UINT minIndex, UINT vertices,
                                                       UINT start, UINT count) override
        {
            OnDraw("indexed", type, count);
            return real->DrawIndexedPrimitive(type, base, minIndex, vertices, start, count);
        }

        HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(D3DPRIMITIVETYPE type, UINT count, const void *data,
                                                  UINT stride) override
        {
            OnDraw("draw-up", type, count);
            return real->DrawPrimitiveUP(type, count, data, stride);
        }

        HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE type, UINT minIndex, UINT vertices,
                                                         UINT count, const void *indices, D3DFORMAT format,
                                                         const void *data, UINT stride) override
        {
            OnDraw("indexed-up", type, count);
            return real->DrawIndexedPrimitiveUP(type, minIndex, vertices, count, indices, format, data, stride);
        }

        HRESULT STDMETHODCALLTYPE Present(const RECT *source, const RECT *dest, HWND window,
                                          const RGNDATA *dirty) override
        {
            EndFrame();
            DrawPanel();
            return real->Present(source, dest, window, dirty);
        }

        HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS *params) override
        {
            if (m_panelReady)
                ImGui_ImplDX9_InvalidateDeviceObjects();
            HRESULT hr = real->Reset(params);
            if (m_panelReady && SUCCEEDED(hr))
                ImGui_ImplDX9_CreateDeviceObjects();
            Defaults(params);                            // a reset sets every state back to its default
            Log("device reset: %s", SUCCEEDED(hr) ? "ok" : "failed");
            return hr;
        }

        // ---- The panel's requests ----

        void RequestCapture()
        {
            if (m_capture == Capture::Idle)
                m_capture = Capture::Armed;
        }

    private:
        enum class Capture { Idle, Armed, Recording };

        // Direct3D's state before the game sets any: the back buffer and the automatic depth buffer bound, the
        // default render states, a viewport over the whole back buffer.
        void Defaults(const D3DPRESENT_PARAMETERS *params)
        {
            memset(m_rs, 0, sizeof(m_rs));
            memset(m_tex, 0, sizeof(m_tex));
            memset(m_rt, 0, sizeof(m_rt));
            m_vs = m_ps = nullptr;
            m_ds = nullptr;
            bool depth = params && params->EnableAutoDepthStencil;
            m_rs[D3DRS_ZENABLE] = depth ? D3DZB_TRUE : D3DZB_FALSE;
            m_rs[D3DRS_ZWRITEENABLE] = TRUE;
            m_rs[D3DRS_ZFUNC] = D3DCMP_LESSEQUAL;
            m_rs[D3DRS_SRCBLEND] = D3DBLEND_ONE;
            m_rs[D3DRS_DESTBLEND] = D3DBLEND_ZERO;
            m_rs[D3DRS_CULLMODE] = D3DCULL_CCW;
            m_rs[D3DRS_COLORWRITEENABLE] = 0xF;
            m_rs[D3DRS_LIGHTING] = TRUE;
            // The surfaces are only pointers for telling targets apart: no reference kept.
            IDirect3DSurface9 *surface = nullptr;
            if (SUCCEEDED(real->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &surface)) && surface)
            {
                m_rt[0] = surface;
                D3DSURFACE_DESC desc = {};
                surface->GetDesc(&desc);
                m_viewport = {0, 0, desc.Width, desc.Height, 0.0f, 1.0f};
                surface->Release();
            }
            if (depth && SUCCEEDED(real->GetDepthStencilSurface(&surface)) && surface)
            {
                m_ds = surface;
                surface->Release();
            }
        }

        struct Counters
        {
            unsigned draws = 0, waterDraws = 0, targetChanges = 0, depthClears = 0, scenes = 0, stateBlocks = 0;
        };

        uint32_t Hash(void *shader) const
        {
            if (!shader)
                return 0;
            auto found = m_hashes.find(shader);
            return found == m_hashes.end() ? 1 : found->second;
        }

        bool IsWater() const
        {
            uint32_t hash = Hash(m_ps);
            return hash == kWaterShader || hash == kWaterShaderReShade;
        }

        static std::string Describe(IDirect3DSurface9 *surface)
        {
            if (!surface)
                return "none";
            D3DSURFACE_DESC desc = {};
            surface->GetDesc(&desc);
            char text[96];
            snprintf(text, sizeof(text), "%p:%ux%u:fmt%u:ms%u", static_cast<void *>(surface), desc.Width,
                     desc.Height, static_cast<unsigned>(desc.Format), static_cast<unsigned>(desc.MultiSampleType));
            return text;
        }

        void Write(const char *format, ...)
        {
            if (!m_file)
                return;
            va_list args;
            va_start(args, format);
            vfprintf(m_file, format, args);
            va_end(args);
            fputc('\n', m_file);
        }

        void OnDraw(const char *kind, D3DPRIMITIVETYPE type, UINT count)
        {
            if (IsWater())
                ++m_frame.waterDraws;
            if (m_capture == Capture::Recording)
                WriteDraw(kind, type, count);
            ++m_frame.draws;
        }

        void WriteDraw(const char *kind, D3DPRIMITIVETYPE type, UINT count)
        {
            FILE *f = m_file;
            if (!f)
                return;
            fprintf(f, "draw %u %s type%d n=%u rt=%s ds=%s", m_frame.draws, kind, static_cast<int>(type), count,
                    Describe(m_rt[0]).c_str(), Describe(m_ds).c_str());
            static const struct
            {
                int state;
                const char *name;
            } states[] = {
                {D3DRS_ZENABLE, "z"}, {D3DRS_ZWRITEENABLE, "zw"}, {D3DRS_ZFUNC, "zf"},
                {D3DRS_ALPHABLENDENABLE, "blend"}, {D3DRS_SRCBLEND, "src"}, {D3DRS_DESTBLEND, "dst"},
                {D3DRS_ALPHATESTENABLE, "atest"}, {D3DRS_CULLMODE, "cull"}, {D3DRS_FOGENABLE, "fog"},
                {D3DRS_COLORWRITEENABLE, "cw"}, {D3DRS_STENCILENABLE, "stencil"}, {D3DRS_LIGHTING, "light"},
            };
            for (const auto &s : states)
                fprintf(f, " %s=%lu", s.name, static_cast<unsigned long>(m_rs[s.state]));
            uint32_t vs = Hash(m_vs), ps = Hash(m_ps);
            fprintf(f, " vs=%08x ps=%08x fvf=%lx vp=%lu,%lu,%lux%lu,%.2f-%.2f", vs, ps,
                    static_cast<unsigned long>(m_fvf), static_cast<unsigned long>(m_viewport.X),
                    static_cast<unsigned long>(m_viewport.Y), static_cast<unsigned long>(m_viewport.Width),
                    static_cast<unsigned long>(m_viewport.Height), m_viewport.MinZ, m_viewport.MaxZ);
            if (IsWater())
                fprintf(f, " WATER");
            for (int stage = 0; stage < kStages; ++stage)
            {
                IDirect3DBaseTexture9 *tex = m_tex[stage];
                if (!tex)
                    continue;
                if (tex->GetType() == D3DRTYPE_TEXTURE)
                {
                    D3DSURFACE_DESC desc = {};
                    static_cast<IDirect3DTexture9 *>(tex)->GetLevelDesc(0, &desc);
                    fprintf(f, " t%d=%p:%ux%u:fmt%u%s", stage, static_cast<void *>(tex), desc.Width, desc.Height,
                            static_cast<unsigned>(desc.Format), (desc.Usage & D3DUSAGE_RENDERTARGET) ? ":rt" : "");
                }
                else
                {
                    fprintf(f, " t%d=%p:type%u", stage, static_cast<void *>(tex),
                            static_cast<unsigned>(tex->GetType()));
                }
            }
            fputc('\n', f);
            if (vs)
            {
                fprintf(f, "  vc:");
                for (int i = 0; i < 16; ++i)
                    fprintf(f, "%s %.4g,%.4g,%.4g,%.4g", i ? " |" : "", m_vsc[i][0], m_vsc[i][1], m_vsc[i][2],
                            m_vsc[i][3]);
                fputc('\n', f);
            }
            else
            {
                const D3DMATRIX *matrices[] = {&m_world, &m_view, &m_projection};
                const char *names[] = {"world", "view", "proj"};
                for (int t = 0; t < 3; ++t)
                {
                    fprintf(f, "  %s:", names[t]);
                    for (int i = 0; i < 16; ++i)
                        fprintf(f, "%s%.4g", i % 4 == 0 ? (i ? " | " : " ") : ",", (&matrices[t]->_11)[i]);
                    fputc('\n', f);
                }
            }
            if (IsWater())
            {
                fprintf(f, "  pc:");
                for (int i = 0; i < 12; ++i)
                    fprintf(f, "%s %.4g,%.4g,%.4g,%.4g", i ? " |" : "", m_psc[i][0], m_psc[i][1], m_psc[i][2],
                            m_psc[i][3]);
                fputc('\n', f);
            }
        }

        void EndFrame()
        {
            if (m_capture == Capture::Recording)
            {
                Write("present after %u draws (%u water, %u target changes, %u depth clears, %u scenes, %u state "
                        "blocks)", m_frame.draws, m_frame.waterDraws, m_frame.targetChanges, m_frame.depthClears,
                        m_frame.scenes, m_frame.stateBlocks);
                fclose(m_file);
                m_file = nullptr;
                m_capture = Capture::Idle;
            }
            else if (m_capture == Capture::Armed)
            {
                m_capturePath = GameFolder() + "Emberforge_capture.txt";
                m_file = fopen(m_capturePath.c_str(), "w");
                if (m_file)
                {
                    IDirect3DSurface9 *back = nullptr;
                    real->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back);
                    Write("Emberforge Engine %s frame capture. pure device: %s. back buffer: %s. water shaders: "
                            "%08x %08x", EMBERFORGE_ENGINE_VERSION, m_pure ? "yes" : "no", Describe(back).c_str(),
                            kWaterShader, kWaterShaderReShade);
                    if (back)
                        back->Release();
                    m_capture = Capture::Recording;
                }
                else
                {
                    m_capturePath = "could not write " + m_capturePath;
                    m_capture = Capture::Idle;
                }
            }
            m_last = m_frame;
            m_frame = Counters();
        }

        // ---- The panel (Home) ----

        bool InitPanel()
        {
            if (m_panelReady)
                return true;
            if (m_panelFailed || !m_window)
                return false;
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO &io = ImGui::GetIO();
            io.IniFilename = nullptr;
            ImGui::StyleColorsDark();
            if (!ImGui_ImplWin32_Init(m_window) || !ImGui_ImplDX9_Init(real))
            {
                Log("panel: could not start");
                ImGui::DestroyContext();
                m_panelFailed = true;
                return false;
            }
            g_gameWndProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrA(m_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(PanelWndProc)));
            m_panelReady = true;
            Log("panel ready");
            return true;
        }

        void ShutdownPanel()
        {
            if (!m_panelReady)
                return;
            if (g_gameWndProc)
                SetWindowLongPtrA(m_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_gameWndProc));
            ImGui_ImplDX9_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            m_panelReady = false;
        }

        void DrawPanel()
        {
            if (!InitPanel())
                return;
            // Home, read from the keyboard directly (the game may not pass every key to its window), while the game's
            // window is in front.
            bool home = (GetAsyncKeyState(VK_HOME) & 0x8000) != 0;
            if (home && !m_homeDown && GetForegroundWindow() == m_window)
                g_panel = !g_panel;
            m_homeDown = home;
            // For the first seconds of play, a note that the engine is on.
            if (m_started == 0)
                m_started = GetTickCount();
            bool note = GetTickCount() - m_started < 10000;
            if (!g_panel && !note)
                return;
            ImGui_ImplDX9_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            if (!g_panel)
            {
                ImGui::SetNextWindowPos(ImVec2(12, 12));
                ImGui::SetNextWindowBgAlpha(0.6f);
                ImGui::Begin("##note", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing);
                ImGui::TextUnformatted("Emberforge Engine " EMBERFORGE_ENGINE_VERSION " on. Home: its panel.");
                ImGui::End();
                ImGui::EndFrame();
                ImGui::Render();
                if (SUCCEEDED(real->BeginScene()))
                {
                    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
                    real->EndScene();
                }
                return;
            }
            ImGui::GetIO().MouseDrawCursor = true;
            ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
            ImGui::Begin("Emberforge Engine " EMBERFORGE_ENGINE_VERSION, &g_panel);
            ImGui::TextUnformatted("What it sees each frame (Home: open/close)");
            ImGui::Separator();
            ImGui::Text("Device: Direct3D 9%s", m_pure ? " (pure: kept by the engine)" : "");
            ImGui::Text("Draws: %u in %u scenes", m_last.draws, m_last.scenes);
            ImGui::Text("Water draws: %u", m_last.waterDraws);
            ImGui::Text("Render target changes: %u, depth clears: %u", m_last.targetChanges, m_last.depthClears);
            ImGui::Text("State blocks: %u", m_last.stateBlocks);
            ImGui::Spacing();
            if (ImGui::Button(m_capture == Capture::Idle ? "Capture one frame" : "Capturing..."))
                RequestCapture();
            if (!m_capturePath.empty())
                ImGui::TextWrapped("Last capture: %s", m_capturePath.c_str());
            ImGui::End();
            ImGui::EndFrame();
            ImGui::Render();
            if (SUCCEEDED(real->BeginScene()))
            {
                ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
                real->EndScene();
            }
        }

        HWND m_window;
        bool m_pure = false;
        ULONG m_refs = 1;
        bool m_recordingBlock = false;

        IDirect3DSurface9 *m_rt[4] = {};
        IDirect3DSurface9 *m_ds = nullptr;
        DWORD m_rs[kRenderStates] = {};
        IDirect3DBaseTexture9 *m_tex[kStages] = {};
        void *m_vs = nullptr;
        void *m_ps = nullptr;
        std::unordered_map<void *, uint32_t> m_hashes;
        float m_vsc[kVertexConstants][4] = {};
        float m_psc[kPixelConstants][4] = {};
        D3DVIEWPORT9 m_viewport = {};
        DWORD m_fvf = 0;
        D3DMATRIX m_world = {}, m_view = {}, m_projection = {};

        Counters m_frame, m_last;
        Capture m_capture = Capture::Idle;
        FILE *m_file = nullptr;
        std::string m_capturePath;

        bool m_panelReady = false, m_panelFailed = false;
        bool m_homeDown = false;
        DWORD m_started = 0;
    };

    // ---- IDirect3D9: hands out our device ----

    class Direct3D9 final : public Direct3D9Forward
    {
    public:
        using Direct3D9Forward::Direct3D9Forward;

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **object) override
        {
            if (riid == IID_IUnknown || riid == IID_IDirect3D9)
            {
                AddRef();
                *object = this;
                return S_OK;
            }
            return real->QueryInterface(riid, object);
        }

        ULONG STDMETHODCALLTYPE Release() override
        {
            ULONG refs = real->Release();
            if (refs == 0)
                delete this;
            return refs;
        }

        HRESULT STDMETHODCALLTYPE CreateDevice(UINT adapter, D3DDEVTYPE type, HWND focus, DWORD behavior,
                                               D3DPRESENT_PARAMETERS *params, IDirect3DDevice9 **device) override
        {
            HRESULT hr = real->CreateDevice(adapter, type, focus, behavior, params, device);
            if (SUCCEEDED(hr) && device && *device)
            {
                HWND window = params && params->hDeviceWindow ? params->hDeviceWindow : focus;
                *device = new Device(*device, window, behavior, params);
            }
            else
            {
                Log("CreateDevice failed: %08lx", static_cast<unsigned long>(hr));
            }
            return hr;
        }
    };
}

// ---- What the game imports from d3d9.dll ----

extern "C"
{
    __declspec(dllexport) IDirect3D9 *WINAPI Direct3DCreate9(UINT sdk)
    {
        auto create = Real<IDirect3D9 *(WINAPI *)(UINT)>("Direct3DCreate9");
        if (!create)
        {
            Log("Windows' d3d9.dll not found");
            return nullptr;
        }
        IDirect3D9 *d3d = create(sdk);
        Log("Emberforge Engine %s started", EMBERFORGE_ENGINE_VERSION);
        return d3d ? new Direct3D9(d3d) : nullptr;
    }

    __declspec(dllexport) HRESULT WINAPI Direct3DCreate9Ex(UINT sdk, IDirect3D9Ex **d3d)
    {
        auto create = Real<HRESULT(WINAPI *)(UINT, IDirect3D9Ex **)>("Direct3DCreate9Ex");
        return create ? create(sdk, d3d) : E_NOTIMPL;
    }

    __declspec(dllexport) int WINAPI D3DPERF_BeginEvent(D3DCOLOR color, LPCWSTR name)
    {
        auto f = Real<int(WINAPI *)(D3DCOLOR, LPCWSTR)>("D3DPERF_BeginEvent");
        return f ? f(color, name) : 0;
    }

    __declspec(dllexport) int WINAPI D3DPERF_EndEvent()
    {
        auto f = Real<int(WINAPI *)()>("D3DPERF_EndEvent");
        return f ? f() : 0;
    }

    __declspec(dllexport) DWORD WINAPI D3DPERF_GetStatus()
    {
        auto f = Real<DWORD(WINAPI *)()>("D3DPERF_GetStatus");
        return f ? f() : 0;
    }

    __declspec(dllexport) BOOL WINAPI D3DPERF_QueryRepeatFrame()
    {
        auto f = Real<BOOL(WINAPI *)()>("D3DPERF_QueryRepeatFrame");
        return f ? f() : FALSE;
    }

    __declspec(dllexport) void WINAPI D3DPERF_SetMarker(D3DCOLOR color, LPCWSTR name)
    {
        auto f = Real<void(WINAPI *)(D3DCOLOR, LPCWSTR)>("D3DPERF_SetMarker");
        if (f)
            f(color, name);
    }

    __declspec(dllexport) void WINAPI D3DPERF_SetOptions(DWORD options)
    {
        auto f = Real<void(WINAPI *)(DWORD)>("D3DPERF_SetOptions");
        if (f)
            f(options);
    }

    __declspec(dllexport) void WINAPI D3DPERF_SetRegion(D3DCOLOR color, LPCWSTR name)
    {
        auto f = Real<void(WINAPI *)(D3DCOLOR, LPCWSTR)>("D3DPERF_SetRegion");
        if (f)
            f(color, name);
    }
}
