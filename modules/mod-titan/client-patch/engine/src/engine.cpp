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
#include "spellmods.h"
namespace Suite
{
    void Tick();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define EMBERFORGE_ENGINE_VERSION "0.23"

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
        // The defaults: the look tuned in game at Stormwind's canals (deep blue, clear, long slow waves).
        float tint[3] = {0.0f, 0.1333f, 0.4118f};
        float tintAmount = 1.0f;
        float hue = 1.5359f, saturation = 1.716f, brightness = 0.188f, murk = 0.04f;
        float waveSize = 20.0f, waveHeight = 0.309f, waveSpeed = 0.603f, wind = 0.8029f;
        float spread = 0.053f, ripples = 0.242f, detail = 0.634f, detailSize = 11.1f;
        float reflectDown = 0.217f, reflectGlance = 0.9f, reflectBrightness = 0.9f, distortion = 1.0f;
        float skyBrightness = 2.2f, sparkle = 0.397f, sparkleSize = 0.093f;
        float sunAzimuth = 6.2134f, sunHeight = 0.4712f;
        float shading = 1.0f, refraction = 3.0f, opacity = 0.85f;
        // The atmosphere: the whole world's light, haze and colour (src/atmosphere.hlsl), alike in every direction
        // (nothing follows the sun or the moon). Its keys are new (atmosphere_*), so an older Emberforge.ini's
        // light-shaft settings are left behind.
        // Off for now (issues left to fix); its key is new, so an Emberforge.ini that saved it on does not turn it
        // back on. The panel still can, to test it.
        bool atmosphere = false, nightAuto = true, hazeOwn = false;
        float show = 0.0f;                                     // 0 all, 1 bloom, 2 haze, 3 night, 4 spill
        // Bloom: what shines brighter than the threshold (lower by night, so lamps and windows stand out).
        float bloom = 1.0f, bloomThreshold = 0.7f, bloomNightThreshold = 0.42f, bloomKnee = 0.2f;
        float bloomSky = 0.12f, bloomColour = 1.25f, bloomReach = 0.9f;
        // Night: by the brightness of the game's haze colour (set by zone and hour): day above nightFrom, full night
        // below nightTo. The dark gets darker and colder; what is brighter than keepTo keeps its light.
        float nightForce = 0.0f, nightFrom = 0.32f, nightTo = 0.12f, nightDark = 0.55f, keepFrom = 0.3f,
              keepTo = 0.7f, nightDesaturate = 0.55f;
        float nightBlue[3] = {0.62f, 0.76f, 1.0f};
        // The same mood by day, milder: the shade darker and cool, what the sun lights kept bright and warm.
        float dayDark = 0.7f, dayKeepFrom = 0.32f, dayKeepTo = 0.78f, dayDesaturate = 0.12f;
        float dayShade[3] = {0.86f, 0.93f, 1.06f};
        // The scene's own lamps (everything that shines on screen: lamps, windows, fire, spells) lighting what is
        // about them in 3D: past a threshold (lower by night) and standing out from what is about them, within a
        // reach. Its keys, the spill's and the night bloom's were renamed when they were calmed (engine 0.23), so
        // an Emberforge.ini that saved the old, glaring values does not bring them back.
        bool lamps = true;
        float lampStrength = 1.0f, lampReach = 10.0f, lampStep = 1.0f, lampThreshold = 0.92f,
              lampNightThreshold = 0.6f;
        // The lights by night: their spill on the ground and walls around them, their halos in the haze.
        float spill = 0.6f, halos = 0.6f, hazeNightLight = 0.45f;
        // Haze: subtle by day (colours nearby kept), in the game's colour.
        float hazeDensity = 0.25f, hazeStart = 20.0f, hazeFalloff = 0.03f, hazeMost = 0.6f, hazeBelow = 0.0f;
        // By day its own: thick, starting close, the air lit (the night's above; mixed by the night amount).
        float dayHazeDensity = 0.75f, dayHazeStart = 8.0f, dayHazeFalloff = 0.018f, dayHazeMost = 0.82f,
              dayHazeGlow = 1.3f;
        float hazeColour[3] = {0.55f, 0.62f, 0.7f};
        float mist = 0.45f, mistSize = 25.0f, mistSpeed = 1.0f;
        // The colour grade: a warm day, a cool night.
        float dayTint[3] = {1.0f, 0.95f, 0.86f}, nightTint[3] = {0.72f, 0.84f, 1.0f};
        float gradeTint = 0.35f, gradeSaturation = 1.15f, contrast = 1.1f, vignette = 0.25f, exposure = 1.0f,
              shoulder = 0.82f;
        // Point lights (street lamps once the engine knows them; the test light ahead of the camera): how strongly
        // they light the surfaces about them and glow in the air, how far they reach, how much they show by day.
        bool lights = true, testLight = false;
        float lightStrength = 1.4f, lightGlow = 0.8f, lightReach = 12.0f, lightsByDay = 0.15f;
        float lightColour[3] = {1.0f, 0.7f, 0.38f};
        // Ambient occlusion (soft contact shadows from the depth); by day warm highlights, cool shadows, and the air
        // a little brighter (sunlit).
        bool compare = false;
        float ao = 0.65f, aoRadius = 2.5f, aoNight = 0.4f, warmth = 0.08f, coolness = 0.08f, dayAir = 0.35f;

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
                {"sparkle_size", &sparkleSize}, {"sun_azimuth", &sunAzimuth}, {"sun_height", &sunHeight},
                {"wave_shading", &shading}, {"refraction", &refraction},
                {"opacity", &opacity}, {"atmosphere_paused", nullptr, &atmosphere},
                {"atmosphere_bloom", &bloom}, {"atmosphere_bloom_threshold", &bloomThreshold},
                {"atmosphere_bloom_threshold_night", &bloomNightThreshold}, {"atmosphere_bloom_knee", &bloomKnee},
                {"atmosphere_bloom_sky", &bloomSky}, {"atmosphere_bloom_colour", &bloomColour},
                {"atmosphere_bloom_reach", &bloomReach}, {"atmosphere_night_auto", nullptr, &nightAuto},
                {"atmosphere_night_force", &nightForce}, {"atmosphere_night_from", &nightFrom},
                {"atmosphere_night_to", &nightTo}, {"atmosphere_night_dark", &nightDark},
                {"atmosphere_keep_from", &keepFrom}, {"atmosphere_keep_to", &keepTo},
                {"atmosphere_night_desaturate", &nightDesaturate}, {"atmosphere_night_blue_r", &nightBlue[0]},
                {"atmosphere_night_blue_g", &nightBlue[1]}, {"atmosphere_night_blue_b", &nightBlue[2]},
                {"atmosphere_day_dark", &dayDark}, {"atmosphere_day_keep_from", &dayKeepFrom},
                {"atmosphere_day_keep_to", &dayKeepTo}, {"atmosphere_day_desaturate", &dayDesaturate},
                {"atmosphere_day_shade_r", &dayShade[0]}, {"atmosphere_day_shade_g", &dayShade[1]},
                {"atmosphere_day_shade_b", &dayShade[2]}, {"lamps", nullptr, &lamps},
                {"atmosphere_lamp_strength", &lampStrength}, {"lamp_reach", &lampReach}, {"lamp_step", &lampStep},
                {"atmosphere_lamp_threshold", &lampThreshold},
                {"atmosphere_lamp_threshold_night", &lampNightThreshold}, {"atmosphere_spill_light", &spill},
                {"atmosphere_halos", &halos},
                {"atmosphere_haze_night_light", &hazeNightLight}, {"atmosphere_haze_density", &hazeDensity},
                {"atmosphere_haze_start", &hazeStart}, {"atmosphere_haze_falloff", &hazeFalloff},
                {"atmosphere_haze_most", &hazeMost}, {"atmosphere_haze_below", &hazeBelow},
                {"atmosphere_day_haze_density", &dayHazeDensity}, {"atmosphere_day_haze_start", &dayHazeStart},
                {"atmosphere_day_haze_falloff", &dayHazeFalloff}, {"atmosphere_day_haze_most", &dayHazeMost},
                {"atmosphere_day_haze_glow", &dayHazeGlow},
                {"atmosphere_haze_own", nullptr, &hazeOwn}, {"atmosphere_haze_r", &hazeColour[0]},
                {"atmosphere_haze_g", &hazeColour[1]}, {"atmosphere_haze_b", &hazeColour[2]},
                {"atmosphere_mist", &mist}, {"atmosphere_mist_size", &mistSize},
                {"atmosphere_mist_speed", &mistSpeed}, {"atmosphere_day_tint_r", &dayTint[0]},
                {"atmosphere_day_tint_g", &dayTint[1]}, {"atmosphere_day_tint_b", &dayTint[2]},
                {"atmosphere_night_tint_r", &nightTint[0]}, {"atmosphere_night_tint_g", &nightTint[1]},
                {"atmosphere_night_tint_b", &nightTint[2]}, {"atmosphere_tint", &gradeTint},
                {"atmosphere_saturation", &gradeSaturation}, {"atmosphere_contrast", &contrast},
                {"atmosphere_vignette", &vignette}, {"atmosphere_exposure", &exposure},
                {"atmosphere_shoulder", &shoulder}, {"lights", nullptr, &lights},
                {"light_strength", &lightStrength}, {"light_glow", &lightGlow}, {"light_reach", &lightReach},
                {"lights_by_day", &lightsByDay}, {"light_colour_r", &lightColour[0]},
                {"light_colour_g", &lightColour[1]}, {"light_colour_b", &lightColour[2]},
                {"atmosphere_ao", &ao}, {"atmosphere_ao_radius", &aoRadius}, {"atmosphere_ao_night", &aoNight},
                {"atmosphere_warmth", &warmth}, {"atmosphere_coolness", &coolness},
                {"atmosphere_day_air", &dayAir},
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

    constexpr int kVertexConstants = 256, kPixelConstants = 64, kRenderStates = 256, kStages = 8;
    constexpr int kSamplers = 16, kSamplerStates = 14;
    constexpr DWORD kPassStages = 5;                          // the stages an engine pass binds (and puts back)
    constexpr UINT kPassConstants = 34;                       // the pixel constants it sets (c0-c33)
    constexpr int kLights = 8;                                // point lights a frame (src/atmosphere.hlsl)
    constexpr float kLampGain = 9.0f;                         // the scene's lamps' light, as gathered (calibrated)
    constexpr UINT kWaterConstants = 12, kWaterConstantCount = 16;   // the water shader's c12-c27 (src/water.hlsl)
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
            for (IDirect3DPixelShader9 *shader : {m_distancePs, m_brightPs, m_downPs, m_upPs, m_finalPs, m_emitPs,
                                                  m_wherePs, m_gatherPs})
                if (shader)
                    shader->Release();
            if (m_passVs)
                m_passVs->Release();
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
                if (index == 0)
                    TargetSize(surface);
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
            BeforeDraw();
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
            BeforeDraw();
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
            BeforeDraw();
            OnDraw("draw-up", type, count);
            return real->DrawPrimitiveUP(type, count, data, stride);
        }

        HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE type, UINT minIndex, UINT vertices,
                                                         UINT count, const void *indices, D3DFORMAT format,
                                                         const void *data, UINT stride) override
        {
            BeforeDraw();
            OnDraw("indexed-up", type, count);
            return real->DrawIndexedPrimitiveUP(type, minIndex, vertices, count, indices, format, data, stride);
        }

        HRESULT STDMETHODCALLTYPE Present(const RECT *source, const RECT *dest, HWND window,
                                          const RGNDATA *dirty) override
        {
            SpellMods::Tick();
            Suite::Tick();
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
                TargetSize(surface);
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
            // The engine's passes draw with ps_3_0, which Direct3D 9 defines only beside a vs_3_0 vertex shader.
            // NVIDIA's driver also runs it after the fixed-function pipeline (pre-transformed vertices); AMD's and
            // Intel's do not (nothing, or garbage, was drawn), so every pass has this vertex shader: the position
            // (already in clip space) and the texture coordinate passed through.
            static const DWORD kPassVertexShader[] = {
                0xFFFE0300,                                  // vs_3_0
                0x0200001F, 0x80000000, 0x900F0000,          // dcl_position v0
                0x0200001F, 0x80000005, 0x900F0001,          // dcl_texcoord v1
                0x0200001F, 0x80000000, 0xE00F0000,          // dcl_position o0
                0x0200001F, 0x80000005, 0xE00F0001,          // dcl_texcoord o1
                0x02000001, 0xE00F0000, 0x90E40000,          // mov o0, v0
                0x02000001, 0xE00F0001, 0x90E40001,          // mov o1, v1
                0x0000FFFF,                                  // end
            };
            if (FAILED(real->CreateVertexShader(kPassVertexShader, &m_passVs)))
                m_passVs = nullptr;
            if (FAILED(real->CreatePixelShader(kWaterPixelShader, &m_waterPs)))
                m_waterPs = nullptr;
            if (FAILED(real->CreatePixelShader(kDistancePixelShader, &m_distancePs)))
                m_distancePs = nullptr;
            bool bright = SUCCEEDED(real->CreatePixelShader(kAtmosphereBrightPixelShader, &m_brightPs));
            bool down = SUCCEEDED(real->CreatePixelShader(kAtmosphereDownPixelShader, &m_downPs));
            bool up = SUCCEEDED(real->CreatePixelShader(kAtmosphereUpPixelShader, &m_upPs));
            bool finished = SUCCEEDED(real->CreatePixelShader(kAtmosphereFinalPixelShader, &m_finalPs));
            if (FAILED(real->CreatePixelShader(kAtmosphereEmitPixelShader, &m_emitPs)) ||
                FAILED(real->CreatePixelShader(kAtmosphereWherePixelShader, &m_wherePs)) ||
                FAILED(real->CreatePixelShader(kAtmosphereGatherPixelShader, &m_gatherPs)))
                m_emitPs = m_wherePs = m_gatherPs = nullptr;
            if (!bright || !down || !up || !finished)
                m_brightPs = m_downPs = m_upPs = m_finalPs = nullptr;
            if (!m_passVs)                                   // no pass can draw without it
                for (IDirect3DPixelShader9 **shader : {&m_distancePs, &m_brightPs, &m_downPs, &m_upPs, &m_finalPs,
                                                       &m_emitPs, &m_wherePs, &m_gatherPs})
                    if (*shader)
                    {
                        (*shader)->Release();
                        *shader = nullptr;
                    }
            Log("engine shaders: water %s, distance %s, atmosphere %s, passes' vertex shader %s",
                m_waterPs ? "ok" : "failed", m_distancePs ? "ok" : "failed", m_finalPs ? "ok" : "failed",
                m_passVs ? "ok" : "failed");
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
            // The light's levels (a half, a quarter... a 32nd of the screen), down and back up: half floats, or 8
            // bits where not.
            for (int level = 0; level < kLevels; ++level)
            {
                m_levelWidth[level] = std::max(1u, desc.Width >> (level + 1));
                m_levelHeight[level] = std::max(1u, desc.Height >> (level + 1));
                for (IDirect3DTexture9 **texture : {&m_down[level], &m_up[level]})
                {
                    if (texture == &m_up[level] && level == kLevels - 1)
                        continue;                            // the smallest has no way up into it
                    if (FAILED(real->CreateTexture(m_levelWidth[level], m_levelHeight[level], 1,
                                                   D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT,
                                                   texture, nullptr)) &&
                        FAILED(real->CreateTexture(m_levelWidth[level], m_levelHeight[level], 1,
                                                   D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, texture,
                                                   nullptr)))
                        *texture = nullptr;
                }
            }
            // The scene's lamps: 64-pixel cells (their light; where it is), and the light they cast at a quarter:
            // half floats only (the distances do not fit in 8 bits).
            m_emitWidth = std::max(1u, desc.Width / 64);
            m_emitHeight = std::max(1u, desc.Height / 64);
            for (IDirect3DTexture9 **texture : {&m_emit, &m_where})
                if (FAILED(real->CreateTexture(m_emitWidth, m_emitHeight, 1, D3DUSAGE_RENDERTARGET,
                                               D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, texture, nullptr)))
                    *texture = nullptr;
            if (FAILED(real->CreateTexture(std::max(1u, desc.Width / 4), std::max(1u, desc.Height / 4), 1,
                                           D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &m_gather,
                                           nullptr)))
                m_gather = nullptr;
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
            for (IDirect3DTexture9 **texture : {&m_intz, &m_scene, &m_distance, &m_emit, &m_where, &m_gather})
            {
                if (*texture)
                    (*texture)->Release();
                *texture = nullptr;
            }
            for (int level = 0; level < kLevels; ++level)
                for (IDirect3DTexture9 **texture : {&m_down[level], &m_up[level]})
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
            // The engine's shader is shader model 3: it needs a shader model 3 vertex shader (the game's water has
            // one, whether its pixel shader is 2.0 or 3.0).
            if (Version(m_vs) != 0xFFFE0300)
            {
                if (!m_waterVersionLogged)
                {
                    m_waterVersionLogged = true;
                    Log("water not replaced: its vertex shader is %08lx (needs 3.0)",
                        static_cast<unsigned long>(Version(m_vs)));
                }
                return false;
            }
            float view[4][4] = {}, projection[4] = {};
            bool camera = Camera(view, projection);
            if (!m_waterPrepared)
            {
                m_waterPrepared = true;
                m_waterCopied = camera && CopyScene(projection[2], m_viewport.MinZ, m_viewport.MaxZ);
                if (!m_waterLogged)
                {
                    m_waterLogged = true;
                    Log("first water: camera %s, depth bound %s, scene and depth copied %s",
                        camera ? "found" : "not found", m_ds && m_ds == m_intzSurface ? "the engine's" : "another",
                        m_waterCopied ? "yes" : "no");
                }
            }
            float constants[kWaterConstantCount][4] = {};
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
            // The fog colour the game set for its fixed-function fog (the 2.0 water's), the sun from the panel,
            // and the glint: white by day, fading with the fog's brightness at night.
            DWORD fog = m_rs[D3DRS_FOGCOLOR];
            float fr = ((fog >> 16) & 0xFF) / 255.0f, fg = ((fog >> 8) & 0xFF) / 255.0f, fb = (fog & 0xFF) / 255.0f;
            float day = std::fmin(1.0f, (0.2126f * fr + 0.7152f * fg + 0.0722f * fb) * 2.5f);
            float ch = std::cos(s.sunHeight);
            float sun[3][4] = {
                {fr, fg, fb, 1.0f},
                {ch * std::cos(s.sunAzimuth), ch * std::sin(s.sunAzimuth), std::sin(s.sunHeight), 0.0f},
                {day * (0.8f + 0.2f * fr), day * (0.8f + 0.2f * fg), day * (0.8f + 0.2f * fb), 1.0f},
            };
            memcpy(constants[13], sun, sizeof(sun));
            real->SetPixelShader(m_waterPs);
            real->SetPixelShaderConstantF(kWaterConstants, constants[0], kWaterConstantCount);
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
            real->SetPixelShaderConstantF(kWaterConstants, m_psc[kWaterConstants], kWaterConstantCount);
            if (m_waterCopied)
            {
                real->SetTexture(12, nullptr);
                real->SetTexture(13, nullptr);
            }
        }

        // At the frame's first water draw: the scene drawn so far (no water yet), and the depth buffer turned into
        // distances in yards. The game's state is put back as it was after.
        bool CopyScene(float nearPlane, float minZ, float maxZ)
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
                real->SetVertexShader(m_passVs);
                real->SetPixelShader(m_distancePs);
                real->SetFVF(D3DFVF_XYZW | D3DFVF_TEX1);
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
                float params[4] = {nearPlane, minZ, maxZ, 0.0f};
                real->SetPixelShaderConstantF(0, params, 1);
                DrawFullScreen(m_width, m_height);
                RestoreAfterPass();
            }
            if (scene)
                scene->Release();
            if (distance)
                distance->Release();
            return ok;
        }

        void DrawFullScreen(UINT width, UINT height)
        {
            // In clip space, moved half a pixel up and left: pixel centres on texel centres (Direct3D 9's
            // half-pixel offset).
            float x = 1.0f / static_cast<float>(width), y = 1.0f / static_cast<float>(height);
            const float quad[4][6] = {
                {-1.0f - x, 1.0f + y, 0.0f, 1.0f, 0.0f, 0.0f}, {1.0f - x, 1.0f + y, 0.0f, 1.0f, 1.0f, 0.0f},
                {-1.0f - x, -1.0f + y, 0.0f, 1.0f, 0.0f, 1.0f}, {1.0f - x, -1.0f + y, 0.0f, 1.0f, 1.0f, 1.0f},
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
            for (DWORD stage = 0; stage < kPassStages; ++stage)
            {
                real->SetTexture(stage, m_tex[stage]);
                for (D3DSAMPLERSTATETYPE type : {D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER,
                                                 D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_SRGBTEXTURE})
                    real->SetSamplerState(stage, type, m_ss[stage][type]);
            }
            for (D3DRENDERSTATETYPE state : kPassStates)
                real->SetRenderState(state, m_rs[state]);
            real->SetRenderState(D3DRS_CULLMODE, m_rs[D3DRS_CULLMODE]);
            real->SetRenderState(D3DRS_COLORWRITEENABLE, m_rs[D3DRS_COLORWRITEENABLE]);
            real->SetPixelShaderConstantF(0, m_psc[0], kPassConstants);
        }

        // A texture for an engine pass, read with clamping, filtered or not.
        void Bind(DWORD stage, IDirect3DTexture9 *texture, bool filtered)
        {
            DWORD filter = filtered ? D3DTEXF_LINEAR : D3DTEXF_POINT;
            real->SetTexture(stage, texture);
            real->SetSamplerState(stage, D3DSAMP_MINFILTER, filter);
            real->SetSamplerState(stage, D3DSAMP_MAGFILTER, filter);
            real->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            real->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            real->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            real->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
        }

        // The state every engine pass draws with (no depth, blending, tests or clipping).
        void PassState()
        {
            real->SetDepthStencilSurface(nullptr);
            real->SetVertexShader(m_passVs);
            real->SetFVF(D3DFVF_XYZW | D3DFVF_TEX1);
            for (D3DRENDERSTATETYPE state : kPassStates)
                real->SetRenderState(state, FALSE);
            real->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            real->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        }

        void DrawInto(IDirect3DTexture9 *target, IDirect3DPixelShader9 *shader, UINT width, UINT height)
        {
            IDirect3DSurface9 *surface = nullptr;
            if (target && SUCCEEDED(target->GetSurfaceLevel(0, &surface)) && surface)
            {
                real->SetRenderTarget(0, surface);
                surface->Release();
            }
            else
            {
                real->SetRenderTarget(0, m_rt[0]);
            }
            real->SetPixelShader(shader);
            DrawFullScreen(width, height);
        }

        // ---- The atmosphere ----

        // The camera, from the game's vertex shader constants as each draw goes by: the terrain's and the
        // buildings' shaders hold the view in c0-c3 (rows: a world point times them) and the projection in c4-c7
        // (infinite: (0, 0, 1, 1) and (0, 0, -near, 0)). Only for the haze's height: the rest needs no camera.
        void WatchConstants()
        {
            const float (*c)[4] = m_vsc;
            auto near0 = [](float a, float b) { return std::fabs(a - b) < 1e-3f; };
            auto unit = [](const float *v) {
                return std::fabs(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] - 1.0f) < 0.02f;
            };
            if (near0(c[6][2], 1.0f) && near0(c[6][3], 1.0f) && near0(c[7][3], 0.0f) && c[7][2] < 0.0f &&
                c[4][0] > 0.0f && c[5][1] > 0.0f && near0(c[3][3], 1.0f) && unit(c[0]) && unit(c[1]) && unit(c[2]))
            {
                memcpy(m_camera, c[0], sizeof(m_camera));
                m_cameraProjection[0] = c[4][0];
                m_cameraProjection[1] = c[5][1];
                m_cameraProjection[2] = -c[7][2];
                m_cameraSeen = m_cameraEver = true;
            }
        }

        // The sky pass (fixed function, the depth range at 1): its dome is drawn with the camera's rotation (and no
        // translation), kept for frames that draw no terrain or buildings (looking up, or at water and models only).
        void WatchSky(UINT count)
        {
            if (m_viewport.MinZ < 0.999f || count <= 100 || m_rs[D3DRS_ALPHABLENDENABLE])
                return;
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    m_skyRotation[i][j] = (&m_view._11)[i * 4 + j];
            m_skyProjection[0] = m_projection._11;
            m_skyProjection[1] = m_projection._22;
            m_skySeen = true;
        }

        // A frame with no terrain or buildings drawn: the camera from the sky dome's rotation and the last position.
        bool CameraFromSky()
        {
            if (!m_skySeen || !m_cameraEver)
                return false;
            float eye[3];
            for (int i = 0; i < 3; ++i)
                eye[i] = -(m_camera[i][0] * m_camera[3][0] + m_camera[i][1] * m_camera[3][1] +
                           m_camera[i][2] * m_camera[3][2]);
            for (int i = 0; i < 3; ++i)
            {
                for (int j = 0; j < 3; ++j)
                    m_camera[i][j] = m_skyRotation[i][j];
                m_camera[i][3] = 0.0f;
            }
            for (int j = 0; j < 3; ++j)
                m_camera[3][j] = -(eye[0] * m_skyRotation[0][j] + eye[1] * m_skyRotation[1][j] +
                                   eye[2] * m_skyRotation[2][j]);
            m_camera[3][3] = 1.0f;
            m_cameraProjection[0] = m_skyProjection[0];
            m_cameraProjection[1] = m_skyProjection[1];
            return true;
        }

        // Once the world is drawn and the interface is about to be: the atmosphere over the world.
        void BeforeDraw()
        {
            if (m_recordingBlock || m_atmosphereDone)
                return;
            bool screen = m_rt0Width == m_width && m_rt0Height == m_height && m_width != 0;
            if (!screen)
                return;
            if (m_rs[D3DRS_ZFUNC] == D3DCMP_ALWAYS)
            {
                if (!m_worldDrawn)
                    return;
                m_atmosphereDone = true;
                if (!m_cameraSeen)
                    m_cameraSeen = CameraFromSky();
                UpdateNight();
                if (m_settings.atmosphere && m_finalPs && LevelsReady())
                    Atmosphere();
                return;
            }
            // The world: drawn with depth into the engine's depth buffer, in its depth range (the sky is drawn at 1).
            // Its depth range is the widest any of it used (one odd draw does not skew the distances).
            if (m_rs[D3DRS_ZENABLE] && m_rs[D3DRS_ZWRITEENABLE] && m_ds && m_ds == m_intzSurface &&
                m_viewport.MaxZ < 0.999f)
            {
                if (!m_worldDrawn)
                {
                    m_worldMinZ = m_viewport.MinZ;
                    m_worldMaxZ = m_viewport.MaxZ;
                }
                m_worldDrawn = true;
                m_worldMinZ = std::fmin(m_worldMinZ, m_viewport.MinZ);
                m_worldMaxZ = std::fmax(m_worldMaxZ, m_viewport.MaxZ);
            }
            // The fog's colour from opaque fogged draws only (the sky dome): the game sets it black or grey for
            // blended draws (particles, glows) so they are not fogged, which washed the view in the wrong colour.
            // Kept from frame to frame (indoors no sky is drawn).
            if (m_rs[D3DRS_FOGENABLE] && !m_rs[D3DRS_ALPHABLENDENABLE])
                m_worldFog = m_rs[D3DRS_FOGCOLOR];
        }

        bool LevelsReady() const
        {
            for (int level = 0; level < kLevels; ++level)
                if (!m_down[level] || (level < kLevels - 1 && !m_up[level]))
                    return false;
            return true;
        }

        // How far into the night it is, from the brightness of the game's haze colour (the game sets it by zone and
        // hour, the same whichever way the camera turns), eased over a couple of seconds.
        void UpdateNight()
        {
            const Settings &s = m_settings;
            double now = Seconds();
            float dt = m_nightTime > 0.0 ? static_cast<float>(std::fmin(now - m_nightTime, 1.0)) : 1.0f;
            m_nightTime = now;
            DWORD fog = m_worldFog;
            float r = ((fog >> 16) & 0xFF) / 255.0f, g = ((fog >> 8) & 0xFF) / 255.0f, b = (fog & 0xFF) / 255.0f;
            m_hazeBrightness = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            float target = s.nightForce;
            if (s.nightAuto && !m_worldFog)
                target = 0.0f;                                   // no haze seen yet (the login screen): day
            else if (s.nightAuto)
            {
                float span = std::fmax(s.nightFrom - s.nightTo, 0.01f);
                float t = std::fmin(1.0f, std::fmax(0.0f, (s.nightFrom - m_hazeBrightness) / span));
                target = t * t * (3.0f - 2.0f * t);
            }
            m_night += (target - m_night) * std::fmin(1.0f, dt * 1.5f);
        }

        void Atmosphere()
        {
            const Settings &s = m_settings;
            // The distances need the near plane: the camera's, the last seen, or the game's usual.
            float nearPlane = m_cameraEver ? m_cameraProjection[2] : 0.2f;
            if (!CopyScene(nearPlane, m_worldMinZ, m_worldMaxZ))
                return;
            const float night = m_night;
            PassState();
            // The light in the scene, at half the screen.
            float threshold = s.bloomThreshold + (s.bloomNightThreshold - s.bloomThreshold) * night;
            float bright[2][4] = {
                {1.0f / m_width, 1.0f / m_height, threshold, std::fmax(s.bloomKnee, 0.01f)},
                {s.bloomSky, s.bloomColour, 0.0f, 0.0f},
            };
            real->SetPixelShaderConstantF(0, bright[0], 2);
            Bind(0, m_scene, true);
            Bind(1, m_distance, false);
            DrawInto(m_down[0], m_brightPs, m_levelWidth[0], m_levelHeight[0]);
            // Down the levels.
            for (int level = 1; level < kLevels; ++level)
            {
                float texel[4] = {1.0f / m_levelWidth[level - 1], 1.0f / m_levelHeight[level - 1], 0.0f, 0.0f};
                real->SetPixelShaderConstantF(0, texel, 1);
                Bind(2, m_down[level - 1], true);
                DrawInto(m_down[level], m_downPs, m_levelWidth[level], m_levelHeight[level]);
            }
            // And back up: each level plus the smaller one spread over it.
            for (int level = kLevels - 2; level >= 0; --level)
            {
                IDirect3DTexture9 *smaller = level == kLevels - 2 ? m_down[level + 1] : m_up[level + 1];
                float texel[4] = {1.0f / m_levelWidth[level + 1], 1.0f / m_levelHeight[level + 1], s.bloomReach,
                                  0.0f};
                real->SetPixelShaderConstantF(0, texel, 1);
                Bind(2, m_down[level], true);
                Bind(3, smaller, true);
                DrawInto(m_up[level], m_upPs, m_levelWidth[level], m_levelHeight[level]);
            }
            // Over the screen.
            const float (*view)[4] = m_camera;
            float eye[3];
            for (int i = 0; i < 3; ++i)
                eye[i] = -(view[i][0] * view[3][0] + view[i][1] * view[3][1] + view[i][2] * view[3][2]);
            DWORD fog = m_worldFog;
            float tint[3];
            for (int i = 0; i < 3; ++i)
                tint[i] = s.dayTint[i] + (s.nightTint[i] - s.dayTint[i]) * night;
            float seconds = static_cast<float>(std::fmod(Seconds() - m_epoch, 3600.0));
            auto mix = [night](float day, float dark) { return day + (dark - day) * night; };
            float constants[kPassConstants][4] = {
                {night, mix(s.dayDark, s.nightDark), mix(s.dayKeepFrom, s.keepFrom),
                 std::fmax(mix(s.dayKeepTo, s.keepTo), mix(s.dayKeepFrom, s.keepFrom) + 0.01f)},
                {mix(s.dayDesaturate, s.nightDesaturate), mix(s.dayShade[0], s.nightBlue[0]),
                 mix(s.dayShade[1], s.nightBlue[1]), mix(s.dayShade[2], s.nightBlue[2])},
                {s.hazeOwn ? s.hazeColour[0] : ((fog >> 16) & 0xFF) / 255.0f,
                 s.hazeOwn ? s.hazeColour[1] : ((fog >> 8) & 0xFF) / 255.0f,
                 s.hazeOwn ? s.hazeColour[2] : (fog & 0xFF) / 255.0f, mix(s.dayHazeDensity, s.hazeDensity)},
                {mix(s.dayHazeStart, s.hazeStart), mix(s.dayHazeFalloff, s.hazeFalloff),
                 mix(s.dayHazeMost, s.hazeMost), s.hazeBelow},
                {m_cameraProjection[0], m_cameraProjection[1], nearPlane, m_cameraSeen ? 1.0f : 0.0f},
                {view[0][0], view[0][1], view[0][2], 0.0f},
                {view[1][0], view[1][1], view[1][2], 0.0f},
                {view[2][0], view[2][1], view[2][2], 0.0f},
                {eye[0], eye[1], eye[2], s.show},
                {s.bloom, s.spill, s.halos, s.hazeNightLight},
                {tint[0], tint[1], tint[2], s.gradeTint},
                {s.gradeSaturation, s.contrast, s.vignette, s.exposure},
                {s.mist, s.mistSize, s.mistSpeed, seconds},
                {1.0f / m_width, 1.0f / m_height, static_cast<float>(m_width) / m_height, s.shoulder},
            };
            if (!m_cameraSeen)                                   // a sane projection for the rays' lengths
                constants[4][0] = constants[4][1] = 1.0f;
            // The point lights, in view space: the test light ahead of the camera, low (where the path is).
            int count = 0;
            auto add = [&](const float at[3], float reach) {
                if (count >= kLights)
                    return;
                float *row = constants[15 + count * 2];
                row[0] = at[0];
                row[1] = at[1];
                row[2] = at[2];
                row[3] = reach;
                float *colour = constants[16 + count * 2];
                colour[0] = s.lightColour[0];
                colour[1] = s.lightColour[1];
                colour[2] = s.lightColour[2];
                colour[3] = 0.0f;
                ++count;
            };
            if (s.lights && s.testLight)
            {
                const float ahead[3] = {0.0f, -1.2f, 9.0f};
                add(ahead, s.lightReach);
            }
            constants[14][0] = static_cast<float>(count);
            constants[14][1] = s.lightStrength;
            constants[14][2] = s.lightGlow;
            constants[14][3] = s.lightsByDay;
            constants[31][0] = s.ao;
            constants[31][1] = std::fmax(s.aoRadius, 0.25f);
            constants[31][2] = s.aoNight;
            constants[31][3] = s.compare ? 1.0f : 0.0f;
            constants[32][0] = s.warmth;
            constants[32][1] = s.coolness;
            constants[32][2] = s.dayAir;
            constants[32][3] = s.dayHazeGlow;
            bool lamps = s.lamps && m_emitPs && m_wherePs && m_gatherPs && m_emit && m_where && m_gather;
            constants[33][0] = s.lampStrength;
            constants[33][1] = s.lightsByDay;
            constants[33][2] = lamps ? 1.0f : 0.0f;
            constants[33][3] = 0.0f;
            // The scene's lamps: where they shine (a 16th of the screen), then the light they cast (half).
            if (lamps)
            {
                real->SetPixelShaderConstantF(0, constants[0], kPassConstants);
                float threshold = s.lampThreshold + (s.lampNightThreshold - s.lampThreshold) * night;
                float emit[4] = {1.0f / m_width, 1.0f / m_height, threshold, 0.12f};
                real->SetPixelShaderConstantF(0, emit, 1);
                Bind(0, m_scene, true);                      // filtered: each tap the average of 2 x 2 pixels
                Bind(1, m_distance, false);
                DrawInto(m_emit, m_emitPs, m_emitWidth, m_emitHeight);
                Bind(2, m_emit, false);                      // Where reads each cell's mean brightness
                DrawInto(m_where, m_wherePs, m_emitWidth, m_emitHeight);
                float gather[4] = {1.0f / m_emitWidth, 1.0f / m_emitHeight, kLampGain, std::fmax(s.lampReach, 1.0f)};
                real->SetPixelShaderConstantF(0, gather, 1);
                Bind(2, m_emit, false);
                Bind(3, m_where, false);
                DrawInto(m_gather, m_gatherPs, std::max(1u, m_width / 4), std::max(1u, m_height / 4));
            }
            real->SetPixelShaderConstantF(0, constants[0], kPassConstants);
            Bind(0, m_scene, false);
            Bind(1, m_distance, false);
            Bind(2, m_up[0], true);
            Bind(3, m_up[2], true);
            Bind(4, lamps ? m_gather : nullptr, true);
            DrawInto(nullptr, m_finalPs, m_width, m_height);
            RestoreAfterPass();
            ++m_frame.atmosphere;
        }

        struct Counters
        {
            unsigned draws = 0, waterDraws = 0, waterReplaced = 0, targetChanges = 0, depthClears = 0, scenes = 0,
                     stateBlocks = 0, atmosphere = 0;
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

        void TargetSize(IDirect3DSurface9 *surface)
        {
            D3DSURFACE_DESC desc = {};
            if (surface)
                surface->GetDesc(&desc);
            m_rt0Width = desc.Width;
            m_rt0Height = desc.Height;
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
                    const char *prefix = type == 10 ? "s" : (type == 1 ? "v" : (type == 3 ? "t" : "o"));
                    snprintf(text, sizeof(text), " %s%lu(%lu/%lu)", prefix,
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
            if (m_vs && !m_recordingBlock)
                WatchConstants();
            else if (!m_vs && !m_recordingBlock)
                WatchSky(count);
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
                    fprintf(f, " t%d=%p:%ux%u:fmt%u%s:fp%08x", stage, static_cast<void *>(tex), desc.Width,
                            desc.Height, static_cast<unsigned>(desc.Format),
                            (desc.Usage & D3DUSAGE_RENDERTARGET) ? ":rt" : "",
                            Fingerprint(static_cast<IDirect3DTexture9 *>(tex)));
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
                // Every other constant that changed since the last draw written (the models' bones live there).
                bool any = false;
                for (int i = 16; i < kVertexConstants; ++i)
                {
                    if (memcmp(m_vsc[i], m_vscWritten[i], sizeof(m_vsc[i])) == 0)
                        continue;
                    fprintf(f, "%s c%d=%.5g,%.5g,%.5g,%.5g", any ? "" : "  vc+:", i, m_vsc[i][0], m_vsc[i][1],
                            m_vsc[i][2], m_vsc[i][3]);
                    memcpy(m_vscWritten[i], m_vsc[i], sizeof(m_vsc[i]));
                    any = true;
                }
                if (any)
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

        // A texture's fingerprint for a capture: a hash of its smallest level, read where the texture is
        // readable (the managed ones; 0 otherwise). Kept per texture.
        uint32_t Fingerprint(IDirect3DTexture9 *texture)
        {
            auto found = m_fingerprints.find(texture);
            if (found != m_fingerprints.end())
                return found->second;
            uint32_t hash = 0;
            DWORD levels = texture->GetLevelCount();
            D3DSURFACE_DESC desc = {};
            D3DLOCKED_RECT locked = {};
            if (levels && SUCCEEDED(texture->GetLevelDesc(levels - 1, &desc)) && desc.Pool == D3DPOOL_MANAGED &&
                SUCCEEDED(texture->LockRect(levels - 1, &locked, nullptr, D3DLOCK_READONLY | D3DLOCK_NOSYSLOCK)))
            {
                bool blocks = desc.Format == D3DFMT_DXT1 || desc.Format == D3DFMT_DXT3 || desc.Format == D3DFMT_DXT5;
                UINT rows = blocks ? (desc.Height + 3) / 4 : desc.Height;
                hash = Fnv(locked.pBits, static_cast<size_t>(locked.Pitch) * rows) ^ (desc.Width << 16) ^ desc.Height;
                texture->UnlockRect(levels - 1);
            }
            m_fingerprints[texture] = hash;
            return hash;
        }

        void EndFrame()
        {
            if (m_capture == Capture::Recording)
            {
                m_fingerprints.clear();
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
                    memset(m_vscWritten, 0, sizeof(m_vscWritten));
                }
                else
                {
                    m_capturePath = "could not write " + m_capturePath;
                    m_capture = Capture::Idle;
                }
            }
            m_last = m_frame;
            m_cameraSeenLast = m_cameraSeen;
            // Why the atmosphere was or was not drawn this frame, for the panel.
            if (!m_settings.atmosphere)
                m_atmosphereWhy = "off";
            else if (m_last.atmosphere)
                m_atmosphereWhy = "drawn";
            else if (!m_finalPs)
                m_atmosphereWhy = "not drawn: its shaders failed";
            else if (!LevelsReady())
                m_atmosphereWhy = "not drawn: its targets failed";
            else if (!m_worldDrawn)
                m_atmosphereWhy = "not drawn: no world seen (depth not the engine's?)";
            else if (!m_atmosphereDone)
                m_atmosphereWhy = "not drawn: the interface never started";
            else
                m_atmosphereWhy = "not drawn: the scene copy failed";
            m_frame = Counters();
            m_waterPrepared = false;
            m_worldDrawn = m_atmosphereDone = m_cameraSeen = false;
            m_skySeen = false;
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
            ImGui::Text("Atmosphere: %s; camera %s (the haze's height)", m_atmosphereWhy,
                        m_cameraSeenLast ? "found" : "not found");
            WaterPanel();
            AtmospherePanel();
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

        void AtmospherePanel()
        {
            Settings &s = m_settings;
            if (!ImGui::CollapsingHeader("Atmosphere", ImGuiTreeNodeFlags_DefaultOpen))
                return;
            ImGui::PushItemWidth(220);
            ImGui::Checkbox("Engine atmosphere", &s.atmosphere);
            int show = static_cast<int>(s.show + 0.5f);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150);
            if (ImGui::Combo("Show", &show, "Everything\0Bloom alone\0Haze alone\0Night (blue: darkened)\0"
                                             "Lights' spill alone\0Point lights alone\0"
                                             "Ambient occlusion alone\0"))
                s.show = static_cast<float>(show);
            ImGui::Checkbox("Compare: the left half untouched", &s.compare);
            ImGui::SeparatorText("Day and night");
            ImGui::Text("The game's haze brightness now: %.3f; night %.0f%%", m_hazeBrightness, m_night * 100.0f);
            ImGui::Checkbox("Night by the haze's brightness", &s.nightAuto);
            if (!s.nightAuto)
                ImGui::SliderFloat("Night (by hand)", &s.nightForce, 0.0f, 1.0f);
            ImGui::SliderFloat("Day above", &s.nightFrom, 0.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Full night below", &s.nightTo, 0.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Day's shade", &s.dayDark, 0.05f, 1.0f, "%.2f (1: none)");
            ImGui::SliderFloat("Sunlit kept from", &s.dayKeepFrom, 0.0f, 1.0f);
            ImGui::SliderFloat("Sunlit kept fully at", &s.dayKeepTo, 0.0f, 1.0f);
            ImGui::SliderFloat("Day shade's colour loss", &s.dayDesaturate, 0.0f, 1.0f);
            ImGui::ColorEdit3("Day shade's colour", s.dayShade, ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Night's darkness", &s.nightDark, 0.05f, 1.0f, "%.2f (1: none)");
            ImGui::SliderFloat("Lights kept from", &s.keepFrom, 0.0f, 1.0f);
            ImGui::SliderFloat("Lights kept fully at", &s.keepTo, 0.0f, 1.0f);
            ImGui::SliderFloat("Night's colour loss", &s.nightDesaturate, 0.0f, 1.0f);
            ImGui::ColorEdit3("Night's blue", s.nightBlue, ImGuiColorEditFlags_NoInputs);
            ImGui::SeparatorText("Lights and bloom");
            ImGui::SliderFloat("Bloom", &s.bloom, 0.0f, 3.0f);
            ImGui::SliderFloat("Bloom threshold by day", &s.bloomThreshold, 0.0f, 1.0f);
            ImGui::SliderFloat("Bloom threshold by night", &s.bloomNightThreshold, 0.0f, 1.0f);
            ImGui::SliderFloat("Threshold softness", &s.bloomKnee, 0.01f, 0.5f);
            ImGui::SliderFloat("Sky's bloom", &s.bloomSky, 0.0f, 1.0f);
            ImGui::SliderFloat("Glow colour", &s.bloomColour, 0.0f, 2.0f);
            ImGui::SliderFloat("Glow reach", &s.bloomReach, 0.3f, 1.5f);
            ImGui::SliderFloat("Lights on the paths", &s.spill, 0.0f, 3.0f);
            ImGui::SliderFloat("Lights' halos in the haze", &s.halos, 0.0f, 3.0f);
            ImGui::SeparatorText("The scene's lamps (lamps, windows, fire, spells light what is about them)");
            ImGui::Checkbox("Lamps light their surroundings", &s.lamps);
            if (!m_emit || !m_where || !m_gather || !m_emitPs)
                ImGui::TextUnformatted("(not available: this card has no half-float targets)");
            ImGui::SliderFloat("Lamp light", &s.lampStrength, 0.0f, 5.0f);
            ImGui::SliderFloat("Lamp reach", &s.lampReach, 2.0f, 30.0f, "%.0f yards");
            ImGui::SliderFloat("Lamp threshold by day", &s.lampThreshold, 0.0f, 1.0f);
            ImGui::SliderFloat("Lamp threshold by night", &s.lampNightThreshold, 0.0f, 1.0f);
            ImGui::SeparatorText("Point lights (test)");
            ImGui::TextWrapped("Street lamps are found once a capture beside one shows their draw; the test light "
                               "stands ahead of the camera to judge the look.");
            ImGui::Checkbox("Point lights", &s.lights);
            ImGui::SameLine();
            ImGui::Checkbox("Test light ahead", &s.testLight);
            ImGui::ColorEdit3("Lamp colour", s.lightColour, ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Light on surfaces", &s.lightStrength, 0.0f, 5.0f);
            ImGui::SliderFloat("Glow in the air", &s.lightGlow, 0.0f, 5.0f);
            ImGui::SliderFloat("Reach", &s.lightReach, 2.0f, 40.0f, "%.0f yards");
            ImGui::SliderFloat("Shown by day", &s.lightsByDay, 0.0f, 1.0f);
            ImGui::SeparatorText("Haze");
            float game[3] = {((m_worldFog >> 16) & 0xFF) / 255.0f, ((m_worldFog >> 8) & 0xFF) / 255.0f,
                             (m_worldFog & 0xFF) / 255.0f};
            ImGui::ColorEdit3("The game's haze colour now", game, ImGuiColorEditFlags_NoInputs |
                              ImGuiColorEditFlags_NoPicker);
            ImGui::Checkbox("Own haze colour", &s.hazeOwn);
            if (s.hazeOwn)
            {
                ImGui::SameLine();
                ImGui::ColorEdit3("##hazecolour", s.hazeColour, ImGuiColorEditFlags_NoInputs);
            }
            ImGui::TextUnformatted("By day");
            ImGui::SliderFloat("Day haze density", &s.dayHazeDensity, 0.0f, 3.0f, "%.2f per 100 yards");
            ImGui::SliderFloat("Day haze start", &s.dayHazeStart, 0.0f, 200.0f, "%.0f yards");
            ImGui::SliderFloat("Day haze most", &s.dayHazeMost, 0.0f, 1.0f);
            ImGui::SliderFloat("Day height falloff", &s.dayHazeFalloff, 0.0f, 0.2f, "%.3f per yard");
            ImGui::SliderFloat("Light glowing through the day haze", &s.dayHazeGlow, 0.0f, 4.0f);
            ImGui::TextUnformatted("By night");
            ImGui::SliderFloat("Haze density", &s.hazeDensity, 0.0f, 2.0f, "%.2f per 100 yards");
            ImGui::SliderFloat("Haze start", &s.hazeStart, 0.0f, 200.0f, "%.0f yards");
            ImGui::SliderFloat("Haze most", &s.hazeMost, 0.0f, 1.0f);
            ImGui::SliderFloat("Height falloff", &s.hazeFalloff, 0.0f, 0.2f, "%.3f per yard");
            ImGui::SliderFloat("Thickest below the eye", &s.hazeBelow, -50.0f, 100.0f, "%.0f yards");
            ImGui::SliderFloat("Haze's light by night", &s.hazeNightLight, 0.0f, 1.0f);
            ImGui::SliderFloat("Drifting mist", &s.mist, 0.0f, 1.0f);
            ImGui::SliderFloat("Mist size", &s.mistSize, 2.0f, 100.0f, "%.0f yards");
            ImGui::SliderFloat("Mist speed", &s.mistSpeed, 0.0f, 5.0f);
            ImGui::SeparatorText("Shading");
            ImGui::SliderFloat("Ambient occlusion", &s.ao, 0.0f, 1.5f);
            ImGui::SliderFloat("Its radius", &s.aoRadius, 0.5f, 8.0f, "%.1f yards");
            ImGui::SliderFloat("Its share by night", &s.aoNight, 0.0f, 1.0f);
            ImGui::SliderFloat("Warm highlights by day", &s.warmth, 0.0f, 0.3f);
            ImGui::SliderFloat("Cool shadows by day", &s.coolness, 0.0f, 0.3f);
            ImGui::SliderFloat("Sunlit air by day", &s.dayAir, 0.0f, 1.0f);
            ImGui::SeparatorText("Colour");
            ImGui::ColorEdit3("Day tint", s.dayTint, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine();
            ImGui::ColorEdit3("Night tint", s.nightTint, ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Tint amount", &s.gradeTint, 0.0f, 1.0f);
            ImGui::SliderFloat("Saturation", &s.gradeSaturation, 0.0f, 2.0f);
            ImGui::SliderFloat("Contrast", &s.contrast, 0.5f, 1.5f);
            ImGui::SliderFloat("Exposure", &s.exposure, 0.5f, 2.0f);
            ImGui::SliderFloat("Highlights roll off from", &s.shoulder, 0.5f, 1.0f);
            ImGui::SliderFloat("Vignette", &s.vignette, 0.0f, 1.0f);
            ImGui::PopItemWidth();
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
            ImGui::SliderAngle("Sun direction", &s.sunAzimuth, 0.0f, 360.0f);
            ImGui::SliderAngle("Sun height", &s.sunHeight, 5.0f, 90.0f);
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
        float m_vscWritten[kVertexConstants][4] = {};        // a capture's: the constants as last written
        std::unordered_map<void *, uint32_t> m_fingerprints;
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
        IDirect3DPixelShader9 *m_brightPs = nullptr, *m_downPs = nullptr, *m_upPs = nullptr, *m_finalPs = nullptr;
        IDirect3DPixelShader9 *m_emitPs = nullptr, *m_wherePs = nullptr, *m_gatherPs = nullptr;
        IDirect3DVertexShader9 *m_passVs = nullptr;           // every pass's (CreateShaders)
        IDirect3DTexture9 *m_emit = nullptr, *m_where = nullptr, *m_gather = nullptr;
        UINT m_emitWidth = 1, m_emitHeight = 1;
        static constexpr int kLevels = 5;                    // the light's levels: a half of the screen to a 32nd
        IDirect3DTexture9 *m_down[kLevels] = {}, *m_up[kLevels] = {};
        UINT m_levelWidth[kLevels] = {}, m_levelHeight[kLevels] = {};
        float m_night = 0.0f, m_hazeBrightness = 0.0f;
        double m_nightTime = 0.0;
        UINT m_rt0Width = 0, m_rt0Height = 0;
        bool m_worldDrawn = false, m_atmosphereDone = false, m_cameraSeen = false;
        bool m_cameraSeenLast = false;
        const char *m_atmosphereWhy = "not drawn yet";
        bool m_skySeen = false, m_cameraEver = false;
        float m_skyRotation[3][3] = {}, m_skyProjection[2] = {};
        float m_worldMinZ = 0.0f, m_worldMaxZ = 1.0f;
        DWORD m_worldFog = 0;
        float m_camera[4][4] = {}, m_cameraProjection[3] = {1.0f, 1.0f, 0.2f};

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
            SpellMods::Tick();
            Suite::Tick();
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

// ---- Spell modifiers by class family (a classless Hero's talents in the client's costs and cooldowns) ----

#include "spellmods.cpp"

// ---- The Emberforge Effect Suite (.ees: the launcher's Suite window) ----

#include "suite.cpp"

// ---- Loaded ----

// The client keeps what the server told it about items, creatures and quests in Cache\WDB and never asks again: an
// answer from before an update (the Hearthstone's 30 min cooldown) outlived it. When the client patch changes
// (Data\enUS\patch-enUS-4.MPQ: its size and write time, kept in Cache\Emberforge.patch), Cache\WDB is cleared once,
// before the game reads it; the client asks the server afresh.
namespace
{
    void ClearFolder(const std::string &folder)
    {
        WIN32_FIND_DATAA found;
        HANDLE search = FindFirstFileA((folder + "\\*").c_str(), &found);
        if (search == INVALID_HANDLE_VALUE)
            return;
        do
        {
            std::string name = found.cFileName;
            if (name == "." || name == "..")
                continue;
            std::string path = folder + "\\" + name;
            if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                ClearFolder(path);
            else
                DeleteFileA(path.c_str());
        } while (FindNextFileA(search, &found));
        FindClose(search);
    }

    void FreshCacheOnNewPatch()
    {
        std::string game = GameFolder();
        WIN32_FILE_ATTRIBUTE_DATA info;
        if (!GetFileAttributesExA((game + "Data\\enUS\\patch-enUS-4.MPQ").c_str(), GetFileExInfoStandard, &info))
            return;
        char stamp[64];
        snprintf(stamp, sizeof(stamp), "%lu %lu %lu", info.nFileSizeLow, info.ftLastWriteTime.dwLowDateTime,
                 info.ftLastWriteTime.dwHighDateTime);
        std::string marker = game + "Cache\\Emberforge.patch";
        char last[64] = {};
        if (FILE *file = fopen(marker.c_str(), "r"))
        {
            if (!fgets(last, sizeof(last), file))
                last[0] = 0;
            fclose(file);
        }
        if (strcmp(last, stamp) == 0)
            return;
        ClearFolder(game + "Cache\\WDB");
        CreateDirectoryA((game + "Cache").c_str(), nullptr);
        if (FILE *file = fopen(marker.c_str(), "w"))
        {
            fputs(stamp, file);
            fclose(file);
        }
        Log("new client patch: Cache\\WDB cleared (the client asks the server afresh)");
    }
}

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
        // Once: the game loads d3d9.dll more than once, but pinned, this runs at the first load only.
        static bool spellModsInstalled = false;
        if (!spellModsInstalled)
        {
            spellModsInstalled = true;
            FreshCacheOnNewPatch();
            SpellMods::Install();
        }
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
