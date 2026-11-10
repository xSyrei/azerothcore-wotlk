// Emberforge Engine: Project Emberforge's own lighting engine for the WoW 3.3.5a client.
//
// It is the game's d3d9.dll: it loads Windows' real one, hands the game a device of its own that passes every call on,
// and so sees all the game draws. The game makes a "pure" device (it answers no questions about its state), so the
// engine keeps its own copy of everything the game sets: render targets, depth, render states, shaders (fingerprinted
// when created), textures, shader constants (the camera), the viewport.
//
// Home opens its panel: what it sees each frame, every look on a slider (kept in Emberforge.ini in the game's folder),
// and "Capture one frame", which writes every draw of the next frame to Emberforge_capture.txt.
//
// The water: the game's depth buffer is swapped for one that can be read (INTZ); at the frame's first water draw the
// engine copies the scene drawn so far and turns the depth into distances (src/distance.hlsl), then draws the water
// with its own shader (src/water.hlsl) in place of the game's.

#include <windows.h>
#include <initguid.h>   // defines the interface IDs d3d9.h declares
#include <d3d9.h>

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <imgui.h>
#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>

#include "forward.h"
#include "known_shaders.h"
#include "shaders.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define EMBERFORGE_ENGINE_VERSION "0.3"

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
        // Appended to: the game loads and unloads d3d9.dll more than once (one log per game start: see DllMain).
        static FILE *file = fopen((GameFolder() + "Emberforge.log").c_str(), "a");
        if (!file)
            return;
        va_list args;
        va_start(args, format);
        vfprintf(file, format, args);
        va_end(args);
        fputc('\n', file);
        fflush(file);
    }

    uint32_t Fnv(const void *data, size_t size);
}

// Every call the game makes on its IDirect3D9 (few, at start-up), with what it returned.
void EngineTrace(const char *call, unsigned long result)
{
    Log("  %s -> %08lx", call, result);
}

namespace
{
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

    // The game's water pixel shader, by what it declares (whatever its exact bytecode, Blizzard's or a patch's): a
    // ps_3_0 shader with cube maps in s0 and s1 (refraction and sky), 2D maps in s2-s5, and the fog as an input.
    bool LooksLikeWater(const DWORD *code)
    {
        if (!code || code[0] != 0xFFFF0300)
            return false;
        int cubes = 0, maps = 0;
        bool fog = false;
        for (size_t i = 1; code[i] != 0x0000FFFF && i < (1u << 20);)
        {
            DWORD token = code[i];
            DWORD op = token & 0xFFFF;
            if (op == 0xFFFE)
            {
                i += 1 + ((token >> 16) & 0x7FFF);
                continue;
            }
            if (op == 31)                                            // dcl: its kind, then the register
            {
                DWORD kind = code[i + 1], reg = code[i + 2];
                DWORD type = ((reg >> 28) & 7) | ((reg >> 8) & 0x18), number = reg & 0x7FF;
                if (type == 10 && ((kind >> 27) & 0xF) == 3 && number <= 1)
                    ++cubes;
                else if (type == 10 && ((kind >> 27) & 0xF) == 2 && number >= 2 && number <= 5)
                    ++maps;
                else if (type == 1 && (kind & 0x1F) == 11)
                    fog = true;
            }
            i += 1 + ((token >> 24) & 0xF);
        }
        return cubes == 2 && maps == 4 && fog;
    }

    const D3DFORMAT kIntz = static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'));

    double Seconds()
    {
        static LARGE_INTEGER frequency = [] {
            LARGE_INTEGER f;
            QueryPerformanceFrequency(&f);
            return f;
        }();
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return static_cast<double>(now.QuadPart) / static_cast<double>(frequency.QuadPart);
    }

    // ---- The looks, on the panel's sliders, kept in Emberforge.ini ----

    struct Settings
    {
        bool water = true, waterCity = true, waterShow = false;
        float tint[3] = {0.03f, 0.11f, 0.13f};
        float tintAmount = 0.6f;
        float hue = 0.0f, saturation = 1.0f, brightness = 0.6f, murk = 0.35f;
        float waveSize = 4.0f, waveHeight = 0.25f, waveSpeed = 0.5f, wind = 0.7f;
        float spread = 0.45f, ripples = 0.35f, detail = 0.3f, detailSize = 6.0f;
        float reflectDown = 0.08f, reflectGlance = 0.9f, reflectBrightness = 0.9f, distortion = 1.0f;
        float skyBrightness = 2.2f, sparkle = 1.0f, sparkleSize = 0.3f;
        float shading = 0.4f, refraction = 1.0f, opacity = 0.85f;

        struct Entry
        {
            const char *name;
            float *number;
            bool *flag;
        };

        template <typename F>
        void Each(F f)
        {
            const Entry entries[] = {
                {"water", nullptr, &water}, {"water_city", nullptr, &waterCity},
                {"water_tint_r", &tint[0]}, {"water_tint_g", &tint[1]}, {"water_tint_b", &tint[2]},
                {"water_tint_amount", &tintAmount}, {"water_hue", &hue}, {"water_saturation", &saturation},
                {"water_brightness", &brightness}, {"water_murk", &murk}, {"wave_size", &waveSize},
                {"wave_height", &waveHeight}, {"wave_speed", &waveSpeed}, {"wave_wind", &wind},
                {"wave_spread", &spread}, {"wave_ripples", &ripples}, {"wave_detail", &detail},
                {"wave_detail_size", &detailSize}, {"reflect_down", &reflectDown},
                {"reflect_glance", &reflectGlance}, {"reflect_brightness", &reflectBrightness},
                {"reflect_distortion", &distortion}, {"sky_brightness", &skyBrightness}, {"sparkle", &sparkle},
                {"sparkle_size", &sparkleSize}, {"wave_shading", &shading}, {"refraction", &refraction},
                {"opacity", &opacity},
            };
            for (const Entry &e : entries)
                f(e);
        }

        static std::string Path()
        {
            return GameFolder() + "Emberforge.ini";
        }

        void Load()
        {
            FILE *file = fopen(Path().c_str(), "r");
            if (!file)
                return;
            char line[256];
            while (fgets(line, sizeof(line), file))
            {
                char *equals = strchr(line, '=');
                if (!equals)
                    continue;
                *equals = 0;
                float value = static_cast<float>(atof(equals + 1));
                Each([&](const Entry &e) {
                    if (strcmp(e.name, line) != 0)
                        return;
                    if (e.number)
                        *e.number = value;
                    else
                        *e.flag = value != 0.0f;
                });
            }
            fclose(file);
        }

        bool Save()
        {
            FILE *file = fopen(Path().c_str(), "w");
            if (!file)
                return false;
            fprintf(file, "; Emberforge Engine settings (the panel's Save writes them; Home opens the panel)\n");
            Each([&](const Entry &e) {
                fprintf(file, "%s=%g\n", e.name, e.number ? *e.number : (*e.flag ? 1.0 : 0.0));
            });
            fclose(file);
            return true;
        }
    };

    // ---- The device the game gets ----

    constexpr int kVertexConstants = 256, kPixelConstants = 32, kRenderStates = 256, kStages = 8;
    constexpr int kSamplers = 16, kSamplerStates = 14;
    constexpr UINT kWaterConstants = 12;                      // the water shader's c12-c24 (src/water.hlsl)
    // Render states an engine pass turns off (and puts back after).
    const D3DRENDERSTATETYPE kPassStates[] = {
        D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ALPHABLENDENABLE, D3DRS_ALPHATESTENABLE, D3DRS_STENCILENABLE,
        D3DRS_SCISSORTESTENABLE, D3DRS_FOGENABLE, D3DRS_SRGBWRITEENABLE, D3DRS_CLIPPLANEENABLE,
        D3DRS_SEPARATEALPHABLENDENABLE,
    };

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
            m_settings.Load();
            Defaults(params);
            Log("device created: window %p, %s", static_cast<void *>(window), m_pure ? "pure" : "not pure");
            CreateShaders();
            CreateTargets(params);
        }

        ~Device() override
        {
            ReleaseTargets();
            if (m_waterPs)
                m_waterPs->Release();
            if (m_distancePs)
                m_distancePs->Release();
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
            // The game's own depth buffer (the automatic one) is swapped for the engine's, which can be read.
            if (surface && surface == m_autoDepth && m_intzSurface)
                surface = m_intzSurface;
            if (!m_recordingBlock)
            {
                m_ds = surface;
                if (m_capture == Capture::Recording)
                    Write("depth = %s", Describe(surface).c_str());
            }
            return real->SetDepthStencilSurface(surface);
        }

        HRESULT STDMETHODCALLTYPE SetVertexDeclaration(IDirect3DVertexDeclaration9 *declaration) override
        {
            if (!m_recordingBlock)
            {
                m_declaration = declaration;
                m_fvfLast = false;
            }
            return real->SetVertexDeclaration(declaration);
        }

        HRESULT STDMETHODCALLTYPE SetStreamSource(UINT stream, IDirect3DVertexBuffer9 *buffer, UINT offset,
                                                  UINT stride) override
        {
            if (!m_recordingBlock && stream == 0)
            {
                m_stream = buffer;
                m_streamOffset = offset;
                m_streamStride = stride;
            }
            return real->SetStreamSource(stream, buffer, offset, stride);
        }

        HRESULT STDMETHODCALLTYPE SetSamplerState(DWORD sampler, D3DSAMPLERSTATETYPE type, DWORD value) override
        {
            if (!m_recordingBlock && sampler < kSamplers && type < kSamplerStates)
                m_ss[sampler][type] = value;
            return real->SetSamplerState(sampler, type, value);
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
            {
                size_t size = BytecodeSize(code);
                m_hashes[*shader] = Fnv(code, size);
                m_code[*shader].assign(code, code + size / sizeof(DWORD));
            }
            return hr;
        }

        HRESULT STDMETHODCALLTYPE CreatePixelShader(const DWORD *code, IDirect3DPixelShader9 **shader) override
        {
            HRESULT hr = real->CreatePixelShader(code, shader);
            if (SUCCEEDED(hr) && code && shader && *shader)
            {
                size_t size = BytecodeSize(code);
                uint32_t hash = Fnv(code, size);
                m_hashes[*shader] = hash;
                m_code[*shader].assign(code, code + size / sizeof(DWORD));
                // A new shader may reuse a freed one's address: it is water or not by its own code.
                if (LooksLikeWater(code) || hash == kWaterShader || hash == kWaterShaderReShade)
                {
                    if (m_waterShaders.insert(*shader).second)
                        Log("water shader found: %08x", hash);
                }
                else
                {
                    m_waterShaders.erase(*shader);
                }
            }
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
            {
                m_fvf = fvf;
                m_fvfLast = true;
            }
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
            bool water = BeginWater();
            HRESULT hr = real->DrawPrimitive(type, start, count);
            if (water)
                EndWater();
            return hr;
        }

        HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE type, INT base, UINT minIndex, UINT vertices,
                                                       UINT start, UINT count) override
        {
            OnDraw("indexed", type, count);
            bool water = BeginWater();
            HRESULT hr = real->DrawIndexedPrimitive(type, base, minIndex, vertices, start, count);
            if (water)
                EndWater();
            return hr;
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
            if (!m_presentedByDevice)
            {
                m_presentedByDevice = true;
                Log("first frame shown (device Present) after %u draws", m_frame.draws);
            }
            OnPresent();
            return real->Present(source, dest, window, dirty);
        }

        // A frame is about to be shown (by the device or a swap chain): the frame ends, the panel is drawn.
        void OnPresent()
        {
            EndFrame();
            DrawPanel();
        }

        // Frames shown through a swap chain come to OnPresent too.
        HRESULT STDMETHODCALLTYPE GetSwapChain(UINT index, IDirect3DSwapChain9 **chain) override;
        HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *params,
                                                            IDirect3DSwapChain9 **chain) override;

        HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS *params) override
        {
            if (m_panelReady)
                ImGui_ImplDX9_InvalidateDeviceObjects();
            ReleaseTargets();                            // a reset fails while video-memory resources live
            HRESULT hr = real->Reset(params);
            if (m_panelReady && SUCCEEDED(hr))
                ImGui_ImplDX9_CreateDeviceObjects();
            Defaults(params);                            // a reset sets every state back to its default
            Log("device reset: %s", SUCCEEDED(hr) ? "ok" : "failed");
            if (SUCCEEDED(hr))
                CreateTargets(params);
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
            for (auto &sampler : m_ss)
            {
                memset(sampler, 0, sizeof(sampler));
                sampler[D3DSAMP_ADDRESSU] = sampler[D3DSAMP_ADDRESSV] = sampler[D3DSAMP_ADDRESSW] = D3DTADDRESS_WRAP;
                sampler[D3DSAMP_MAGFILTER] = sampler[D3DSAMP_MINFILTER] = D3DTEXF_POINT;
                sampler[D3DSAMP_MAXANISOTROPY] = 1;
            }
            m_declaration = nullptr;
            m_fvf = 0;
            m_fvfLast = false;
            m_stream = nullptr;
            m_streamOffset = m_streamStride = 0;
            m_autoDepth = nullptr;
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
                m_ds = m_autoDepth = surface;
                surface->Release();
            }
        }

        // ---- The engine's own resources ----

        void CreateShaders()
        {
            if (FAILED(real->CreatePixelShader(kWaterPixelShader, &m_waterPs)))
                m_waterPs = nullptr;
            if (FAILED(real->CreatePixelShader(kDistancePixelShader, &m_distancePs)))
                m_distancePs = nullptr;
            Log("engine shaders: water %s, distance %s", m_waterPs ? "ok" : "failed", m_distancePs ? "ok" : "failed");
        }

        // The readable depth buffer (in place of the automatic one), the scene's copy and the distances: the size of
        // the back buffer, made again after every reset.
        void CreateTargets(const D3DPRESENT_PARAMETERS *params)
        {
            IDirect3DSurface9 *back = nullptr;
            if (FAILED(real->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) || !back)
                return;
            D3DSURFACE_DESC desc = {};
            back->GetDesc(&desc);
            back->Release();
            m_width = desc.Width;
            m_height = desc.Height;
            if (FAILED(real->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, desc.Format,
                                           D3DPOOL_DEFAULT, &m_scene, nullptr)))
                m_scene = nullptr;
            if (FAILED(real->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F,
                                           D3DPOOL_DEFAULT, &m_distance, nullptr)))
                m_distance = nullptr;
            bool intz = false;
            IDirect3D9 *d3d = nullptr;
            D3DDEVICE_CREATION_PARAMETERS creation = {};
            if (SUCCEEDED(real->GetDirect3D(&d3d)) && d3d)
            {
                real->GetCreationParameters(&creation);
                intz = SUCCEEDED(d3d->CheckDeviceFormat(creation.AdapterOrdinal, creation.DeviceType, D3DFMT_X8R8G8B8,
                                                        D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, kIntz));
                d3d->Release();
            }
            bool plain = desc.MultiSampleType == D3DMULTISAMPLE_NONE;
            if (intz && plain && m_autoDepth && params && params->EnableAutoDepthStencil &&
                SUCCEEDED(real->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_DEPTHSTENCIL, kIntz,
                                              D3DPOOL_DEFAULT, &m_intz, nullptr)))
            {
                m_intz->GetSurfaceLevel(0, &m_intzSurface);
                // Bound now, before the game draws: it starts as the depth buffer the game sees.
                real->SetDepthStencilSurface(m_intzSurface);
                m_ds = m_intzSurface;
                real->Clear(0, nullptr, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
            }
            Log("engine targets %ux%u: scene %s, distance %s, readable depth %s", desc.Width, desc.Height,
                m_scene ? "ok" : "failed", m_distance ? "ok" : "failed",
                m_intz ? "ok" : (!intz ? "not supported" : (!plain ? "off (anti-aliasing)" : "failed")));
        }

        void ReleaseTargets()
        {
            if (m_intzSurface)
            {
                // The automatic depth buffer back in place (a reset expects it bound).
                if (m_autoDepth && m_ds == m_intzSurface)
                    real->SetDepthStencilSurface(m_autoDepth);
                m_intzSurface->Release();
                m_intzSurface = nullptr;
            }
            for (IDirect3DTexture9 **texture : {&m_intz, &m_scene, &m_distance})
            {
                if (*texture)
                    (*texture)->Release();
                *texture = nullptr;
            }
        }

        // ---- The water ----

        // The camera, from the water's vertex shader constants: the projection in c0-c3 (x and y scales, then an
        // infinite far plane: (0, 0, 1, 1) and (0, 0, -near, 0)) and the view in c5-c8 (rows: a world point times
        // them). Checked each frame: false if they are not there.
        bool Camera(float view[4][4], float projection[4]) const
        {
            const float (*c)[4] = m_vsc;
            bool infinite = std::fabs(c[2][2] - 1.0f) < 1e-3f && std::fabs(c[2][3] - 1.0f) < 1e-3f &&
                            std::fabs(c[3][3]) < 1e-4f && c[3][2] < 0.0f && c[0][0] > 0.0f && c[1][1] > 0.0f;
            if (!infinite)
                return false;
            for (int row = 0; row < 3; ++row)
            {
                float length = c[5 + row][0] * c[5 + row][0] + c[5 + row][1] * c[5 + row][1] +
                               c[5 + row][2] * c[5 + row][2];
                if (std::fabs(length - 1.0f) > 0.02f)
                    return false;
            }
            if (std::fabs(c[8][3] - 1.0f) > 1e-3f)
                return false;
            memcpy(view, c[5], sizeof(float) * 16);
            projection[0] = c[0][0];
            projection[1] = c[1][1];
            projection[2] = -c[3][2];
            projection[3] = 1.0f;
            return true;
        }

        // Before a draw: the game's water is drawn with the engine's shader.
        bool BeginWater()
        {
            if (!m_settings.water || !m_waterPs || m_recordingBlock || !IsWater())
                return false;
            // The engine's shader is shader model 3: it works only with a shader model 3 vertex shader.
            if (Version(m_ps) != 0xFFFF0300 || Version(m_vs) != 0xFFFE0300)
            {
                if (!m_waterVersionLogged)
                {
                    m_waterVersionLogged = true;
                    Log("water not replaced: the game's water is pixel %08lx / vertex %08lx (needs 3.0 for both)",
                        static_cast<unsigned long>(Version(m_ps)), static_cast<unsigned long>(Version(m_vs)));
                }
                return false;
            }
            float view[4][4] = {}, projection[4] = {};
            bool camera = Camera(view, projection);
            if (!m_waterPrepared)
            {
                m_waterPrepared = true;
                m_waterCopied = camera && CopyScene(projection[2]);
                if (!m_waterLogged)
                {
                    m_waterLogged = true;
                    Log("first water: camera %s, depth bound %s, scene and depth copied %s",
                        camera ? "found" : "not found", m_ds && m_ds == m_intzSurface ? "the engine's" : "another",
                        m_waterCopied ? "yes" : "no");
                }
            }
            float constants[13][4] = {};
            memcpy(constants[0], view, sizeof(view));
            memcpy(constants[4], projection, sizeof(projection));
            float seconds = static_cast<float>(std::fmod(Seconds() - m_epoch, 3600.0));
            const Settings &s = m_settings;
            float screen[4] = {1.0f / m_width, 1.0f / m_height, seconds, m_waterCopied ? 1.0f : 0.0f};
            float rows[8][4] = {
                {s.tint[0], s.tint[1], s.tint[2], s.tintAmount},
                {s.hue, s.saturation, s.brightness, s.murk},
                {s.waveSize, s.waveHeight, s.waveSpeed, s.wind},
                {s.spread, s.ripples, s.detail, s.detailSize},
                {s.reflectDown, s.reflectGlance, s.reflectBrightness, s.distortion},
                {s.skyBrightness, s.waterCity ? 1.0f : 0.0f, s.sparkle, s.sparkleSize},
                {s.shading, s.refraction, s.opacity, s.waterShow ? 1.0f : 0.0f},
            };
            memcpy(constants[5], screen, sizeof(screen));
            memcpy(constants[6], rows, sizeof(float) * 4 * 7);
            real->SetPixelShader(m_waterPs);
            real->SetPixelShaderConstantF(kWaterConstants, constants[0], 13);
            if (m_waterCopied)
            {
                real->SetTexture(12, m_distance);
                real->SetTexture(13, m_scene);
                for (DWORD sampler : {12u, 13u})
                {
                    DWORD filter = sampler == 12 ? D3DTEXF_POINT : D3DTEXF_LINEAR;
                    real->SetSamplerState(sampler, D3DSAMP_MINFILTER, filter);
                    real->SetSamplerState(sampler, D3DSAMP_MAGFILTER, filter);
                    real->SetSamplerState(sampler, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
                    real->SetSamplerState(sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                    real->SetSamplerState(sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                    real->SetSamplerState(sampler, D3DSAMP_SRGBTEXTURE, FALSE);
                }
            }
            ++m_frame.waterReplaced;
            return true;
        }

        // After it: the game's shader and constants back.
        void EndWater()
        {
            real->SetPixelShader(static_cast<IDirect3DPixelShader9 *>(m_ps));
            real->SetPixelShaderConstantF(kWaterConstants, m_psc[kWaterConstants], 13);
            if (m_waterCopied)
            {
                real->SetTexture(12, nullptr);
                real->SetTexture(13, nullptr);
            }
        }

        // At the frame's first water draw: the scene drawn so far (no water yet), and the depth buffer turned into
        // distances in yards. The game's state is put back as it was after.
        bool CopyScene(float nearPlane)
        {
            if (!m_scene || !m_distance || !m_distancePs || !m_intzSurface || m_ds != m_intzSurface || !m_rt[0])
                return false;
            D3DSURFACE_DESC desc = {};
            m_rt[0]->GetDesc(&desc);
            if (desc.Width != m_width || desc.Height != m_height)
                return false;
            IDirect3DSurface9 *scene = nullptr, *distance = nullptr;
            m_scene->GetSurfaceLevel(0, &scene);
            m_distance->GetSurfaceLevel(0, &distance);
            bool ok = scene && distance && SUCCEEDED(real->StretchRect(m_rt[0], nullptr, scene, nullptr, D3DTEXF_NONE));
            if (ok)
            {
                real->SetRenderTarget(0, distance);
                real->SetDepthStencilSurface(nullptr);
                real->SetVertexShader(nullptr);
                real->SetPixelShader(m_distancePs);
                real->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
                real->SetTexture(0, m_intz);
                real->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
                real->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
                real->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
                real->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                real->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                real->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
                for (D3DRENDERSTATETYPE state : kPassStates)
                    real->SetRenderState(state, FALSE);
                real->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
                real->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
                float params[4] = {nearPlane, m_viewport.MinZ, m_viewport.MaxZ, 0.0f};
                real->SetPixelShaderConstantF(0, params, 1);
                DrawFullScreen();
                RestoreAfterPass();
            }
            if (scene)
                scene->Release();
            if (distance)
                distance->Release();
            return ok;
        }

        void DrawFullScreen()
        {
            // Pixel centres on texel centres (Direct3D 9's half-pixel offset).
            float w = static_cast<float>(m_width) - 0.5f, h = static_cast<float>(m_height) - 0.5f;
            const float quad[4][6] = {
                {-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f}, {w, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f},
                {-0.5f, h, 0.0f, 1.0f, 0.0f, 1.0f}, {w, h, 0.0f, 1.0f, 1.0f, 1.0f},
            };
            real->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
        }

        // Puts back what an engine pass changed, from the state the engine keeps.
        void RestoreAfterPass()
        {
            real->SetRenderTarget(0, m_rt[0]);
            real->SetDepthStencilSurface(m_ds);
            real->SetViewport(&m_viewport);                  // a render target change resets it
            real->SetVertexShader(static_cast<IDirect3DVertexShader9 *>(m_vs));
            real->SetPixelShader(static_cast<IDirect3DPixelShader9 *>(m_ps));
            if (m_fvfLast || !m_declaration)
                real->SetFVF(m_fvf);
            else
                real->SetVertexDeclaration(m_declaration);
            real->SetStreamSource(0, m_stream, m_streamOffset, m_streamStride);   // DrawPrimitiveUP cleared it
            real->SetTexture(0, m_tex[0]);
            for (D3DSAMPLERSTATETYPE type : {D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER,
                                             D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_SRGBTEXTURE})
                real->SetSamplerState(0, type, m_ss[0][type]);
            for (D3DRENDERSTATETYPE state : kPassStates)
                real->SetRenderState(state, m_rs[state]);
            real->SetRenderState(D3DRS_CULLMODE, m_rs[D3DRS_CULLMODE]);
            real->SetRenderState(D3DRS_COLORWRITEENABLE, m_rs[D3DRS_COLORWRITEENABLE]);
            real->SetPixelShaderConstantF(0, m_psc[0], 1);
        }

        struct Counters
        {
            unsigned draws = 0, waterDraws = 0, waterReplaced = 0, targetChanges = 0, depthClears = 0, scenes = 0,
                     stateBlocks = 0;
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
            return m_ps && m_waterShaders.count(m_ps) != 0;
        }

        static bool IsCube(IDirect3DBaseTexture9 *texture)
        {
            return texture && texture->GetType() == D3DRTYPE_CUBETEXTURE;
        }

        // The shader declares cube samplers in s0 and s1.
        bool ReadsCubes(void *shader) const
        {
            auto found = m_code.find(shader);
            if (found == m_code.end())
                return false;
            const std::vector<DWORD> &code = found->second;
            int cubes = 0;
            for (size_t i = 1; i + 2 < code.size() && code[i] != 0x0000FFFF;)
            {
                DWORD token = code[i], op = token & 0xFFFF;
                if (op == 0xFFFE)
                {
                    i += 1 + ((token >> 16) & 0x7FFF);
                    continue;
                }
                DWORD reg = code[i + 2];
                if (op == 31 && (((reg >> 28) & 7) | ((reg >> 8) & 0x18)) == 10 && (reg & 0x7FF) <= 1 &&
                    ((code[i + 1] >> 27) & 0xF) == 3)
                    ++cubes;
                i += 1 + ((token >> 24) & 0xF);
            }
            return cubes == 2;
        }

        DWORD Version(void *shader) const
        {
            auto found = m_code.find(shader);
            return found == m_code.end() || found->second.empty() ? 0 : found->second[0];
        }

        // A shader's bytecode to a file in the game's folder, and its declarations to the log.
        void DumpShader(void *shader, const char *name)
        {
            auto found = shader ? m_code.find(shader) : m_code.end();
            if (found == m_code.end() || found->second.empty())
            {
                Log("%s: no shader bound (fixed function) or its code unknown", name);
                return;
            }
            const std::vector<DWORD> &code = found->second;
            if (FILE *file = fopen((GameFolder() + name).c_str(), "wb"))
            {
                fwrite(code.data(), sizeof(DWORD), code.size(), file);
                fclose(file);
            }
            std::string decls;
            for (size_t i = 1; i < code.size() && code[i] != 0x0000FFFF;)
            {
                DWORD token = code[i], op = token & 0xFFFF;
                if (op == 0xFFFE)
                {
                    i += 1 + ((token >> 16) & 0x7FFF);
                    continue;
                }
                if (op == 31 && i + 2 < code.size())
                {
                    char text[48];
                    DWORD kind = code[i + 1], reg = code[i + 2];
                    DWORD type = ((reg >> 28) & 7) | ((reg >> 8) & 0x18);
                    snprintf(text, sizeof(text), " %s%lu(%lu/%lu)", type == 10 ? "s" : (type == 1 ? "v" : "o"),
                             static_cast<unsigned long>(reg & 0x7FF),
                             static_cast<unsigned long>(type == 10 ? (kind >> 27) & 0xF : kind & 0x1F),
                             static_cast<unsigned long>((kind >> 16) & 0xF));
                    decls += text;
                }
                i += 1 + ((token >> 24) & 0xF);
            }
            Log("%s: hash %08x, version %08lx, %u words, declares%s", name, Hash(shader),
                static_cast<unsigned long>(code[0]), static_cast<unsigned>(code.size()), decls.c_str());
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
            if (!m_firstDraw)
            {
                m_firstDraw = true;
                Log("first draw (%s)", kind);
            }
            // Water by what is bound, whatever else its code says: the game's liquids read cube maps (the
            // reflections) in s0 and s1, with cube maps bound there.
            if (m_ps && !m_recordingBlock && !IsWater() && IsCube(m_tex[0]) && IsCube(m_tex[1]) &&
                ReadsCubes(m_ps))
            {
                m_waterShaders.insert(m_ps);
                Log("water found by its textures: pixel shader %08x, vertex shader %08x", Hash(m_ps), Hash(m_vs));
            }
            if (IsWater())
            {
                ++m_frame.waterDraws;
                if (!m_waterDumped)
                {
                    m_waterDumped = true;
                    DumpShader(m_ps, "Emberforge_water_ps.bin");
                    DumpShader(m_vs, "Emberforge_water_vs.bin");
                }
            }
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
            m_waterPrepared = false;
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
            bool win32 = ImGui_ImplWin32_Init(m_window);
            bool dx9 = win32 && ImGui_ImplDX9_Init(real);
            if (!win32 || !dx9)
            {
                Log("panel: could not start (window %p: %s, Direct3D: %s)", static_cast<void *>(m_window),
                    win32 ? "ok" : "failed", dx9 ? "ok" : "failed");
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
            ImGui::SetNextWindowSize(ImVec2(540, 680), ImGuiCond_FirstUseEver);
            ImGui::Begin("Emberforge Engine " EMBERFORGE_ENGINE_VERSION, &g_panel);
            ImGui::TextUnformatted("What it sees each frame (Home: open/close)");
            ImGui::Separator();
            ImGui::Text("Device: Direct3D 9%s", m_pure ? " (pure: kept by the engine)" : "");
            ImGui::Text("Draws: %u in %u scenes", m_last.draws, m_last.scenes);
            ImGui::Text("Water draws: %u (%u drawn by the engine%s)", m_last.waterDraws, m_last.waterReplaced,
                        m_last.waterReplaced && !m_waterCopied ? ", no depth: plain" : "");
            ImGui::Text("Readable depth: %s", m_intz ? "yes" : "no");
            WaterPanel();
            if (ImGui::CollapsingHeader("Diagnostics"))
            {
                ImGui::Text("Render target changes: %u, depth clears: %u", m_last.targetChanges, m_last.depthClears);
                ImGui::Text("State blocks: %u", m_last.stateBlocks);
                if (ImGui::Button(m_capture == Capture::Idle ? "Capture one frame" : "Capturing..."))
                    RequestCapture();
                if (!m_capturePath.empty())
                    ImGui::TextWrapped("Last capture: %s", m_capturePath.c_str());
            }
            ImGui::Separator();
            if (ImGui::Button("Save"))
                m_settingsSaved = m_settings.Save();
            ImGui::SameLine();
            if (ImGui::Button("Load saved"))
                m_settings.Load();
            ImGui::SameLine();
            if (ImGui::Button("Defaults"))
                m_settings = Settings();
            if (m_settingsSaved)
            {
                ImGui::SameLine();
                ImGui::TextUnformatted("saved to Emberforge.ini");
            }
            ImGui::End();
            ImGui::EndFrame();
            ImGui::Render();
            if (SUCCEEDED(real->BeginScene()))
            {
                ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
                real->EndScene();
            }
        }

        void WaterPanel()
        {
            Settings &s = m_settings;
            if (!ImGui::CollapsingHeader("Water", ImGuiTreeNodeFlags_DefaultOpen))
                return;
            ImGui::PushItemWidth(220);
            ImGui::Checkbox("Engine water", &s.water);
            ImGui::SameLine();
            ImGui::Checkbox("Show it", &s.waterShow);
            ImGui::SeparatorText("Colour");
            ImGui::ColorEdit3("Tint colour", s.tint);
            ImGui::SliderFloat("Tint amount", &s.tintAmount, 0.0f, 1.0f);
            ImGui::SliderAngle("Hue shift", &s.hue, -180.0f, 180.0f);
            ImGui::SliderFloat("Saturation", &s.saturation, 0.0f, 2.0f);
            ImGui::SliderFloat("Brightness", &s.brightness, 0.0f, 2.0f);
            ImGui::SliderFloat("Murkiness", &s.murk, 0.0f, 3.0f, "%.2f per yard");
            ImGui::SliderFloat("Wave shading", &s.shading, 0.0f, 1.0f);
            ImGui::SeparatorText("Waves");
            ImGui::SliderFloat("Wave size", &s.waveSize, 0.5f, 20.0f, "%.1f yards");
            ImGui::SliderFloat("Wave height", &s.waveHeight, 0.0f, 1.0f);
            ImGui::SliderFloat("Wave speed", &s.waveSpeed, 0.0f, 3.0f);
            ImGui::SliderAngle("Wind direction", &s.wind, 0.0f, 360.0f);
            ImGui::SliderFloat("Spread", &s.spread, 0.0f, 1.0f);
            ImGui::SliderFloat("Small ripples", &s.ripples, 0.0f, 1.0f);
            ImGui::SliderFloat("Wave texture", &s.detail, 0.0f, 1.0f);
            ImGui::SliderFloat("Wave texture size", &s.detailSize, 0.5f, 30.0f, "%.1f yards");
            ImGui::SeparatorText("Reflections");
            ImGui::Checkbox("Mirror the city", &s.waterCity);
            ImGui::SliderFloat("Reflectiveness looking down", &s.reflectDown, 0.0f, 1.0f);
            ImGui::SliderFloat("Reflectiveness at a glance", &s.reflectGlance, 0.0f, 1.0f);
            ImGui::SliderFloat("Reflection brightness", &s.reflectBrightness, 0.0f, 2.0f);
            ImGui::SliderFloat("Distortion", &s.distortion, 0.0f, 3.0f);
            ImGui::SliderFloat("Sky brightness", &s.skyBrightness, 0.0f, 4.0f);
            ImGui::SliderFloat("Refraction", &s.refraction, 0.0f, 3.0f);
            ImGui::SliderFloat("Opacity (no depth)", &s.opacity, 0.0f, 1.0f);
            ImGui::SeparatorText("Sparkle");
            ImGui::SliderFloat("Sparkle", &s.sparkle, 0.0f, 3.0f);
            ImGui::SliderFloat("Sparkle size", &s.sparkleSize, 0.0f, 1.0f);
            ImGui::PopItemWidth();
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
        std::unordered_map<void *, std::vector<DWORD>> m_code;
        bool m_waterDumped = false, m_waterVersionLogged = false;
        float m_vsc[kVertexConstants][4] = {};
        float m_psc[kPixelConstants][4] = {};
        D3DVIEWPORT9 m_viewport = {};
        DWORD m_fvf = 0;
        bool m_fvfLast = false;
        IDirect3DVertexDeclaration9 *m_declaration = nullptr;
        IDirect3DVertexBuffer9 *m_stream = nullptr;
        UINT m_streamOffset = 0, m_streamStride = 0;
        DWORD m_ss[kSamplers][kSamplerStates] = {};
        D3DMATRIX m_world = {}, m_view = {}, m_projection = {};
        std::unordered_set<void *> m_waterShaders;

        Settings m_settings;
        bool m_settingsSaved = false;
        IDirect3DSurface9 *m_autoDepth = nullptr;            // the game's automatic depth buffer (no reference kept)
        IDirect3DTexture9 *m_intz = nullptr, *m_scene = nullptr, *m_distance = nullptr;
        IDirect3DSurface9 *m_intzSurface = nullptr;
        IDirect3DPixelShader9 *m_waterPs = nullptr, *m_distancePs = nullptr;
        UINT m_width = 0, m_height = 0;
        bool m_waterPrepared = false, m_waterCopied = false, m_waterLogged = false;
        double m_epoch = Seconds();

        Counters m_frame, m_last;
        Capture m_capture = Capture::Idle;
        FILE *m_file = nullptr;
        std::string m_capturePath;

        bool m_panelReady = false, m_panelFailed = false;
        bool m_homeDown = false;
        bool m_presentedByDevice = false, m_firstDraw = false;
        DWORD m_started = 0;
    };

    // ---- A swap chain: its Present shows a frame too ----

    class SwapChain final : public SwapChain9Forward
    {
    public:
        SwapChain(IDirect3DSwapChain9 *chain, Device *device) : SwapChain9Forward(chain), m_device(device) {}

        ULONG STDMETHODCALLTYPE Release() override
        {
            ULONG refs = real->Release();
            if (refs == 0)
                delete this;
            return refs;
        }

        HRESULT STDMETHODCALLTYPE Present(const RECT *source, const RECT *dest, HWND window, const RGNDATA *dirty,
                                          DWORD flags) override
        {
            if (!m_logged)
            {
                m_logged = true;
                Log("first frame shown (swap chain Present)");
            }
            if (g_device == m_device)
                m_device->OnPresent();
            return real->Present(source, dest, window, dirty, flags);
        }

        HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9 **device) override
        {
            if (!device)
                return D3DERR_INVALIDCALL;
            m_device->AddRef();
            *device = m_device;
            return D3D_OK;
        }

    private:
        Device *m_device;
        bool m_logged = false;
    };

    HRESULT STDMETHODCALLTYPE Device::GetSwapChain(UINT index, IDirect3DSwapChain9 **chain)
    {
        HRESULT hr = real->GetSwapChain(index, chain);
        if (SUCCEEDED(hr) && chain && *chain)
        {
            Log("swap chain %u asked for", index);
            *chain = new SwapChain(*chain, this);
        }
        return hr;
    }

    HRESULT STDMETHODCALLTYPE Device::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *params,
                                                                IDirect3DSwapChain9 **chain)
    {
        HRESULT hr = real->CreateAdditionalSwapChain(params, chain);
        if (SUCCEEDED(hr) && chain && *chain)
        {
            Log("additional swap chain created");
            *chain = new SwapChain(*chain, this);
        }
        return hr;
    }

    // ---- IDirect3D9: hands out our device ----

    class Direct3D9 final : public Direct3D9Forward
    {
    public:
        using Direct3D9Forward::Direct3D9Forward;

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **object) override
        {
            Log("  IDirect3D9::QueryInterface %08lx-%04x", static_cast<unsigned long>(riid.Data1), riid.Data2);
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
            Log("  IDirect3D9::Release -> %lu", static_cast<unsigned long>(refs));
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
                Log("CreateDevice: %ux%u %s, flags %08lx, focus %p, device window %p",
                    params ? params->BackBufferWidth : 0, params ? params->BackBufferHeight : 0,
                    params && params->Windowed ? "windowed" : "full screen", static_cast<unsigned long>(behavior),
                    static_cast<void *>(focus), params ? static_cast<void *>(params->hDeviceWindow) : nullptr);
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

// ---- Loaded ----

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        // The game loads d3d9.dll by name, checks the graphics card through it and frees it (FreeLibrary) before it
        // loads it again to draw: unloaded in between, the engine lost its place. Pinned, it stays for the game's life.
        HMODULE pinned = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           reinterpret_cast<LPCSTR>(&DllMain), &pinned);
        // A fresh log for each game start (the log is appended to while it runs).
        std::string path = GameFolder() + "Emberforge.log";
        if (FILE *fresh = fopen(path.c_str(), "w"))
            fclose(fresh);
        char exe[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exe, MAX_PATH);
        Log("Emberforge Engine %s loaded by %s%s", EMBERFORGE_ENGINE_VERSION, exe, pinned ? " (pinned)" : "");
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        Log(reserved ? "game closed" : "unloaded");
    }
    (void)module;
    return TRUE;
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
        Log("Emberforge Engine %s started (Direct3DCreate9 %u: %s)", EMBERFORGE_ENGINE_VERSION, sdk,
            d3d ? "ok" : "failed");
        return d3d ? new Direct3D9(d3d) : nullptr;
    }

    __declspec(dllexport) HRESULT WINAPI Direct3DCreate9Ex(UINT sdk, IDirect3D9Ex **d3d)
    {
        // Not wrapped: frames drawn this way pass the engine by (the launcher sets the game's gxApi to "d3d9").
        Log("Direct3DCreate9Ex asked for: the game draws past the engine (its gxApi should be \"d3d9\")");
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
