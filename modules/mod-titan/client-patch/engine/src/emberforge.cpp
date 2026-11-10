// Emberforge Engine: Project Emberforge's own lighting engine for the WoW 3.3.5a client, a ReShade add-on
// (Emberforge.addon32, beside ReShade32.dll as d3d9.dll; needs ReShade's build with add-on support).
//
// The ReShade effects alone only ever see the finished picture and whatever depth buffer ReShade guesses at, so they
// had to guess where the water is, whether the world is on screen, and how the camera sits, and they did so unstably
// (stale depth burned into occlusion, the sky taken for water). This add-on sits inside the game's own Direct3D 9
// drawing, where those are facts: which draw is the water, the depth when the world is done, the interface starting.
//
// This first version learns how the client draws: an "Emberforge" tab in ReShade's window shows what the engine sees
// each frame, and "Capture one frame" writes every draw of the next frame (render targets, depth, blending, shaders,
// textures, the vertex shader's first constants: the camera) to Emberforge_capture.txt in the game's folder.
// The effects are then rebuilt on those facts.

// Windows and Direct3D first: they declare what ReShade's headers use (__uuidof under MinGW).
#include <windows.h>
#include <d3d9.h>

#define ImTextureID ImU64
#include <imgui.h>
#include <reshade.hpp>

#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>

#include "known_shaders.h"

extern "C"
{
    __declspec(dllexport) extern const char *NAME;
    __declspec(dllexport) extern const char *DESCRIPTION;
}
const char *NAME = "Emberforge Engine";
const char *DESCRIPTION =
    "Project Emberforge's lighting engine: water, light, fog and shading from inside the game's own drawing.";

namespace
{
    IDirect3DDevice9 *g_device = nullptr;

    // Per frame.
    unsigned g_draws = 0, g_waterDraws = 0, g_targetChanges = 0, g_depthClears = 0;
    // The last whole frame's, for the overlay.
    unsigned g_lastDraws = 0, g_lastWaterDraws = 0, g_lastTargetChanges = 0, g_lastDepthClears = 0;
    bool g_pure = false;

    // Capture: requested, then the whole next frame (present to present) is written.
    enum class Capture { Idle, Armed, Recording } g_capture = Capture::Idle;
    FILE *g_file = nullptr;
    std::string g_lastCapture;

    // Shaders: the 32-bit FNV-1a of their bytecode, cached by pointer.
    std::unordered_map<void *, uint32_t> g_shaderHashes;

    uint32_t Fnv(const uint8_t *data, size_t size)
    {
        uint32_t hash = 2166136261u;
        for (size_t i = 0; i < size; ++i)
            hash = (hash ^ data[i]) * 16777619u;
        return hash;
    }

    template <typename Shader>
    uint32_t ShaderHash(Shader *shader)
    {
        if (shader == nullptr)
            return 0;
        auto found = g_shaderHashes.find(shader);
        if (found != g_shaderHashes.end())
            return found->second;
        UINT size = 0;
        uint32_t hash = 1;
        if (SUCCEEDED(shader->GetFunction(nullptr, &size)) && size > 0)
        {
            std::string code(size, '\0');
            if (SUCCEEDED(shader->GetFunction(code.data(), &size)))
                hash = Fnv(reinterpret_cast<const uint8_t *>(code.data()), size);
        }
        g_shaderHashes.emplace(shader, hash);
        return hash;
    }

    std::string GameFolder()
    {
        char path[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string folder = path;
        size_t slash = folder.find_last_of("\\/");
        return slash == std::string::npos ? std::string() : folder.substr(0, slash + 1);
    }

    void Surface(FILE *f, const char *name, IDirect3DSurface9 *surface)
    {
        if (surface == nullptr)
        {
            fprintf(f, " %s=none", name);
            return;
        }
        D3DSURFACE_DESC desc = {};
        surface->GetDesc(&desc);
        fprintf(f, " %s=%p:%ux%u:fmt%u:ms%u", name, static_cast<void *>(surface), desc.Width, desc.Height,
                static_cast<unsigned>(desc.Format), static_cast<unsigned>(desc.MultiSampleType));
    }

    // One draw's state, in one line (then constants on the next lines).
    void WriteDraw(const char *kind, uint32_t count)
    {
        IDirect3DDevice9 *d = g_device;
        FILE *f = g_file;
        fprintf(f, "draw %u %s n=%u", g_draws, kind, count);

        IDirect3DSurface9 *rt = nullptr, *ds = nullptr;
        d->GetRenderTarget(0, &rt);
        d->GetDepthStencilSurface(&ds);
        Surface(f, "rt", rt);
        Surface(f, "ds", ds);
        if (rt)
            rt->Release();
        if (ds)
            ds->Release();

        static const struct
        {
            D3DRENDERSTATETYPE state;
            const char *name;
        } states[] = {
            {D3DRS_ZENABLE, "z"}, {D3DRS_ZWRITEENABLE, "zw"}, {D3DRS_ZFUNC, "zf"},
            {D3DRS_ALPHABLENDENABLE, "blend"}, {D3DRS_SRCBLEND, "src"}, {D3DRS_DESTBLEND, "dst"},
            {D3DRS_ALPHATESTENABLE, "atest"}, {D3DRS_CULLMODE, "cull"}, {D3DRS_FOGENABLE, "fog"},
            {D3DRS_COLORWRITEENABLE, "cw"}, {D3DRS_STENCILENABLE, "stencil"}, {D3DRS_SCISSORTESTENABLE, "scissor"},
            {D3DRS_LIGHTING, "light"},
        };
        for (const auto &s : states)
        {
            DWORD value = 0;
            if (SUCCEEDED(d->GetRenderState(s.state, &value)))
                fprintf(f, " %s=%lu", s.name, static_cast<unsigned long>(value));
        }

        IDirect3DVertexShader9 *vs = nullptr;
        IDirect3DPixelShader9 *ps = nullptr;
        d->GetVertexShader(&vs);
        d->GetPixelShader(&ps);
        uint32_t vsHash = ShaderHash(vs), psHash = ShaderHash(ps);
        fprintf(f, " vs=%08x ps=%08x", vsHash, psHash);
        if (psHash == kWaterShader || psHash == kWaterShaderReShade)
            fprintf(f, " WATER");
        if (vs)
            vs->Release();
        if (ps)
            ps->Release();
        DWORD fvf = 0;
        if (SUCCEEDED(d->GetFVF(&fvf)))
            fprintf(f, " fvf=%lx", static_cast<unsigned long>(fvf));

        D3DVIEWPORT9 vp = {};
        if (SUCCEEDED(d->GetViewport(&vp)))
            fprintf(f, " vp=%lu,%lu,%lux%lu,%.3f-%.3f", static_cast<unsigned long>(vp.X),
                    static_cast<unsigned long>(vp.Y), static_cast<unsigned long>(vp.Width),
                    static_cast<unsigned long>(vp.Height), vp.MinZ, vp.MaxZ);

        for (DWORD stage = 0; stage < 6; ++stage)
        {
            IDirect3DBaseTexture9 *tex = nullptr;
            if (FAILED(d->GetTexture(stage, &tex)) || tex == nullptr)
                continue;
            if (tex->GetType() == D3DRTYPE_TEXTURE)
            {
                D3DSURFACE_DESC desc = {};
                static_cast<IDirect3DTexture9 *>(tex)->GetLevelDesc(0, &desc);
                fprintf(f, " t%lu=%p:%ux%u:fmt%u%s", static_cast<unsigned long>(stage), static_cast<void *>(tex),
                        desc.Width, desc.Height, static_cast<unsigned>(desc.Format),
                        (desc.Usage & D3DUSAGE_RENDERTARGET) ? ":rt" : "");
            }
            else
            {
                fprintf(f, " t%lu=%p:type%u", static_cast<unsigned long>(stage), static_cast<void *>(tex),
                        static_cast<unsigned>(tex->GetType()));
            }
            tex->Release();
        }
        fprintf(f, "\n");

        // The vertex shader's first constants (where the camera's matrices usually are); with no vertex shader, the
        // fixed-function transforms.
        if (vsHash != 0)
        {
            float c[16 * 4] = {};
            if (SUCCEEDED(d->GetVertexShaderConstantF(0, c, 16)))
            {
                fprintf(f, "  vc:");
                for (int i = 0; i < 16 * 4; ++i)
                    fprintf(f, "%s%.4g", i % 4 == 0 ? (i ? " | " : " ") : ",", c[i]);
                fprintf(f, "\n");
            }
        }
        else
        {
            static const D3DTRANSFORMSTATETYPE transforms[] = {D3DTS_WORLD, D3DTS_VIEW, D3DTS_PROJECTION};
            static const char *names[] = {"world", "view", "proj"};
            for (int t = 0; t < 3; ++t)
            {
                D3DMATRIX m = {};
                if (SUCCEEDED(d->GetTransform(transforms[t], &m)))
                {
                    fprintf(f, "  %s:", names[t]);
                    for (int i = 0; i < 16; ++i)
                        fprintf(f, "%s%.4g", i % 4 == 0 ? (i ? " | " : " ") : ",", (&m._11)[i]);
                    fprintf(f, "\n");
                }
            }
        }
        if (psHash == kWaterShader || psHash == kWaterShaderReShade)
        {
            float c[12 * 4] = {};
            if (SUCCEEDED(d->GetPixelShaderConstantF(0, c, 12)))
            {
                fprintf(f, "  pc:");
                for (int i = 0; i < 12 * 4; ++i)
                    fprintf(f, "%s%.4g", i % 4 == 0 ? (i ? " | " : " ") : ",", c[i]);
                fprintf(f, "\n");
            }
        }
    }

    bool IsWaterDraw()
    {
        IDirect3DPixelShader9 *ps = nullptr;
        if (FAILED(g_device->GetPixelShader(&ps)) || ps == nullptr)
            return false;
        uint32_t hash = ShaderHash(ps);
        ps->Release();
        return hash == kWaterShader || hash == kWaterShaderReShade;
    }

    void OnDraw(const char *kind, uint32_t count)
    {
        if (g_device == nullptr)
            return;
        if (IsWaterDraw())
            ++g_waterDraws;
        if (g_capture == Capture::Recording && g_file)
            WriteDraw(kind, count);
        ++g_draws;
    }

    void on_init_device(reshade::api::device *device)
    {
        if (device->get_api() != reshade::api::device_api::d3d9)
            return;
        g_device = reinterpret_cast<IDirect3DDevice9 *>(device->get_native());
        D3DDEVICE_CREATION_PARAMETERS params = {};
        if (SUCCEEDED(g_device->GetCreationParameters(&params)))
            g_pure = (params.BehaviorFlags & D3DCREATE_PUREDEVICE) != 0;
    }

    void on_destroy_device(reshade::api::device *device)
    {
        if (g_device == reinterpret_cast<IDirect3DDevice9 *>(device->get_native()))
            g_device = nullptr;
        g_shaderHashes.clear();
    }

    bool on_draw(reshade::api::command_list *, uint32_t vertices, uint32_t, uint32_t, uint32_t)
    {
        OnDraw("draw", vertices);
        return false;
    }

    bool on_draw_indexed(reshade::api::command_list *, uint32_t indices, uint32_t, uint32_t, int32_t, uint32_t)
    {
        OnDraw("indexed", indices);
        return false;
    }

    void on_bind_targets(reshade::api::command_list *, uint32_t count, const reshade::api::resource_view *rtvs,
                         reshade::api::resource_view dsv)
    {
        ++g_targetChanges;
        if (g_capture == Capture::Recording && g_file)
            fprintf(g_file, "targets (before draw %u) count=%u rt0=%llx ds=%llx\n", g_draws, count,
                    static_cast<unsigned long long>(count ? rtvs[0].handle : 0),
                    static_cast<unsigned long long>(dsv.handle));
    }

    bool on_clear_depth(reshade::api::command_list *, reshade::api::resource_view dsv, const float *depth,
                        const uint8_t *, uint32_t, const reshade::api::rect *)
    {
        ++g_depthClears;
        if (g_capture == Capture::Recording && g_file)
            fprintf(g_file, "clear depth (before draw %u) ds=%llx to %.3f\n", g_draws,
                    static_cast<unsigned long long>(dsv.handle), depth ? *depth : -1.0f);
        return false;
    }

    bool on_clear_target(reshade::api::command_list *, reshade::api::resource_view rtv, const float color[4],
                         uint32_t, const reshade::api::rect *)
    {
        if (g_capture == Capture::Recording && g_file)
            fprintf(g_file, "clear target (before draw %u) rt=%llx to %.3f,%.3f,%.3f,%.3f\n", g_draws,
                    static_cast<unsigned long long>(rtv.handle), color[0], color[1], color[2], color[3]);
        return false;
    }

    void on_present(reshade::api::command_queue *, reshade::api::swapchain *, const reshade::api::rect *,
                    const reshade::api::rect *, uint32_t, const reshade::api::rect *)
    {
        if (g_capture == Capture::Recording)
        {
            if (g_file)
            {
                fprintf(g_file, "present after %u draws (%u water, %u target changes, %u depth clears)\n", g_draws,
                        g_waterDraws, g_targetChanges, g_depthClears);
                fclose(g_file);
                g_file = nullptr;
            }
            g_capture = Capture::Idle;
        }
        else if (g_capture == Capture::Armed)
        {
            g_lastCapture = GameFolder() + "Emberforge_capture.txt";
            g_file = fopen(g_lastCapture.c_str(), "w");
            if (g_file)
            {
                fprintf(g_file, "Emberforge Engine frame capture. pure device: %s. water shaders: %08x %08x\n",
                        g_pure ? "yes" : "no", kWaterShader, kWaterShaderReShade);
                g_capture = Capture::Recording;
            }
            else
            {
                g_lastCapture = "could not write " + g_lastCapture;
                g_capture = Capture::Idle;
            }
        }
        g_lastDraws = g_draws;
        g_lastWaterDraws = g_waterDraws;
        g_lastTargetChanges = g_targetChanges;
        g_lastDepthClears = g_depthClears;
        g_draws = g_waterDraws = g_targetChanges = g_depthClears = 0;
    }

    void draw_overlay(reshade::api::effect_runtime *)
    {
        ImGui::TextUnformatted("Emberforge Engine: what it sees each frame");
        ImGui::Separator();
        ImGui::Text("Device: %s%s", g_device ? "Direct3D 9" : "none", g_pure ? " (pure: state not readable)" : "");
        ImGui::Text("Draws: %u", g_lastDraws);
        ImGui::Text("Water draws (Emberforge water shader): %u", g_lastWaterDraws);
        ImGui::Text("Render target changes: %u, depth clears: %u", g_lastTargetChanges, g_lastDepthClears);
        ImGui::Spacing();
        if (ImGui::Button(g_capture == Capture::Idle ? "Capture one frame" : "Capturing..."))
        {
            if (g_capture == Capture::Idle)
                g_capture = Capture::Armed;
        }
        if (!g_lastCapture.empty())
            ImGui::TextWrapped("Last capture: %s", g_lastCapture.c_str());
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        if (!reshade::register_addon(module))
            return FALSE;
        reshade::register_event<reshade::addon_event::init_device>(on_init_device);
        reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
        reshade::register_event<reshade::addon_event::draw>(on_draw);
        reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
        reshade::register_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(on_bind_targets);
        reshade::register_event<reshade::addon_event::clear_depth_stencil_view>(on_clear_depth);
        reshade::register_event<reshade::addon_event::clear_render_target_view>(on_clear_target);
        reshade::register_event<reshade::addon_event::present>(on_present);
        reshade::register_overlay("Emberforge", draw_overlay);
        break;
    case DLL_PROCESS_DETACH:
        reshade::unregister_addon(module);
        break;
    }
    return TRUE;
}
