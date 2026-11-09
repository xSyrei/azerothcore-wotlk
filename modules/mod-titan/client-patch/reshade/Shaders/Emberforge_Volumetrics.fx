/*
 * Emberforge Volumetrics: light shafts and atmospheric fog for the WoW 3.3.5a client, a ReShade effect.
 *
 * The client draws its world itself, so the lighting is a screen-space layer over it:
 *  1. Sky: the bright sky (pixels at the far plane) at an eighth of the screen.
 *  2. Sun: where that light gathers (the brightest sky, weighted toward its hot spot), its colour and how much of it
 *     shows, eased from frame to frame so the shafts do not jump.
 *  3. Shafts: the sky's light smeared toward the sun, so it streams around trees, roofs and mountains in front of it.
 *  4. Fog: distance fog from the depth buffer, tinted with the sky's colour and brighter toward the sun (light
 *     scattering in the air); the sky itself is left alone.
 *  5. Composite.
 *
 * Depth: WoW's buffer is not reversed (ReShade.ini sets RESHADE_DEPTH_INPUT_IS_REVERSED=0). Without depth access
 * (some drivers) the effect fades out to nothing rather than fogging the screen.
 */

#include "ReShade.fxh"

// Settings ---------------------------------------------------------------------------------------------------------

uniform float ShaftIntensity <
    ui_type = "slider"; ui_min = 0.0; ui_max = 2.0; ui_step = 0.01;
    ui_category = "Light shafts"; ui_label = "Intensity";
    ui_tooltip = "How bright the rays streaming from the sun are.";
> = 0.65;

uniform float ShaftLength <
    ui_type = "slider"; ui_min = 0.5; ui_max = 1.0; ui_step = 0.005;
    ui_category = "Light shafts"; ui_label = "Length";
    ui_tooltip = "How far the rays reach from the sun across the screen.";
> = 0.93;

uniform float SkyThreshold <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Light shafts"; ui_label = "Sky brightness threshold";
    ui_tooltip = "Only sky brighter than this casts rays (raise it if overcast skies glow).";
> = 0.55;

uniform float FogDensity <
    ui_type = "slider"; ui_min = 0.0; ui_max = 4.0; ui_step = 0.01;
    ui_category = "Fog"; ui_label = "Density";
    ui_tooltip = "How thick the air gets with distance.";
> = 1.1;

uniform float FogStart <
    ui_type = "slider"; ui_min = 0.0; ui_max = 0.5; ui_step = 0.005;
    ui_category = "Fog"; ui_label = "Start distance";
    ui_tooltip = "How far away the fog begins (share of the view distance).";
> = 0.04;

uniform float FogAmount <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Fog"; ui_label = "Strength";
    ui_tooltip = "The most the fog may cover the far distance.";
> = 0.45;

uniform float SunScatter <
    ui_type = "slider"; ui_min = 0.0; ui_max = 2.0; ui_step = 0.01;
    ui_category = "Fog"; ui_label = "Sun glow in the fog";
    ui_tooltip = "How much the fog lights up toward the sun.";
> = 0.8;

uniform float FrameTime < source = "frametime"; >;

// Textures ---------------------------------------------------------------------------------------------------------

#define SKY_WIDTH  (BUFFER_WIDTH / 8)
#define SKY_HEIGHT (BUFFER_HEIGHT / 8)
#define SUN_GRID_X 32
#define SUN_GRID_Y 18
#define SHAFT_SAMPLES 56

texture SkyTex { Width = SKY_WIDTH; Height = SKY_HEIGHT; Format = RGBA16F; MipLevels = 4; };
sampler SkySampler { Texture = SkyTex; AddressU = BORDER; AddressV = BORDER; };

// x, y: the sun on screen; z: how much sky light shows (0-1); w: unused.
texture SunTex { Width = 1; Height = 1; Format = RGBA32F; };
sampler SunSampler { Texture = SunTex; MinFilter = POINT; MagFilter = POINT; };
// The sun's (and sky's) colour.
texture SunColorTex { Width = 1; Height = 1; Format = RGBA16F; };
sampler SunColorSampler { Texture = SunColorTex; MinFilter = POINT; MagFilter = POINT; };
// Last frame's, eased toward this frame's.
texture SunPrevTex { Width = 1; Height = 1; Format = RGBA32F; };
sampler SunPrevSampler { Texture = SunPrevTex; MinFilter = POINT; MagFilter = POINT; };
texture SunColorPrevTex { Width = 1; Height = 1; Format = RGBA16F; };
sampler SunColorPrevSampler { Texture = SunColorPrevTex; MinFilter = POINT; MagFilter = POINT; };

texture ShaftTex { Width = BUFFER_WIDTH / 2; Height = BUFFER_HEIGHT / 2; Format = RGBA16F; };
sampler ShaftSampler { Texture = ShaftTex; };

// Helpers ----------------------------------------------------------------------------------------------------------

float Luma(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

// The far plane: sky, and nothing drawn in front of it.
float IsSky(float2 uv)
{
    return step(0.9995, ReShade::GetLinearizedDepth(uv));
}

// Pass 1: sky ------------------------------------------------------------------------------------------------------

float4 PS_Sky(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float3 color = tex2D(ReShade::BackBuffer, uv).rgb;
    float bright = smoothstep(SkyThreshold, 1.0, Luma(color));
    return float4(color * bright * IsSky(uv), IsSky(uv));
}

// Pass 2: sun ------------------------------------------------------------------------------------------------------

void PS_Sun(float4 pos : SV_Position, float2 uv : TEXCOORD, out float4 sun : SV_Target0, out float4 tint : SV_Target1)
{
    float2 centre = 0.0;
    float weight = 0.0, peak = 0.0, sky = 0.0;
    float3 color = 0.0;
    [loop]
    for (int y = 0; y < SUN_GRID_Y; y++)
    {
        [loop]
        for (int x = 0; x < SUN_GRID_X; x++)
        {
            float2 at = (float2(x, y) + 0.5) / float2(SUN_GRID_X, SUN_GRID_Y);
            float4 s = tex2Dlod(SkySampler, float4(at, 0, 2));
            float l = Luma(s.rgb);
            // Weighted toward the hot spot: the sun's disc outweighs a bright cloud.
            float w = l * l * l * l;
            centre += at * w;
            weight += w;
            peak = max(peak, l);
            // The sky's own colour, for the fog (all of the sky, not only its bright part).
            float isSky = IsSky(at);
            sky += isSky;
            color += tex2Dlod(ReShade::BackBuffer, float4(at, 0, 0)).rgb * isSky;
        }
    }
    float2 at = weight > 1e-5 ? centre / weight : float2(0.5, 0.0);
    // How much sun shows: by the brightest sky on screen (none at night, under a roof, or looking away).
    float shown = smoothstep(0.15, 0.6, peak);
    float4 now = float4(at, shown, 0.0);
    float3 hue = sky > 0.5 ? color / sky : 0.6;

    // Eased: about a third of a second to follow.
    float ease = saturate(FrameTime / 300.0);
    float4 prev = tex2Dfetch(SunPrevSampler, int2(0, 0));
    float4 prevTint = tex2Dfetch(SunColorPrevSampler, int2(0, 0));
    bool first = prev.w < 0.5;
    sun = first ? float4(now.xyz, 1.0) : float4(lerp(prev.xyz, now.xyz, ease), 1.0);
    tint = first ? float4(hue, 1.0) : float4(lerp(prevTint.rgb, hue, ease), 1.0);
}

void PS_KeepSun(float4 pos : SV_Position, float2 uv : TEXCOORD, out float4 sun : SV_Target0,
    out float4 tint : SV_Target1)
{
    sun = tex2Dfetch(SunSampler, int2(0, 0));
    tint = tex2Dfetch(SunColorSampler, int2(0, 0));
}

// Pass 3: shafts ---------------------------------------------------------------------------------------------------

float4 PS_Shafts(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float4 sun = tex2Dfetch(SunSampler, int2(0, 0));
    if (sun.z < 0.01)
        return 0.0;

    // Toward the sun, in screen space corrected for the aspect so the rays fan out evenly.
    float2 delta = (sun.xy - uv) / SHAFT_SAMPLES;
    float2 at = uv;
    float decay = 1.0;
    float3 sum = 0.0;
    [loop]
    for (int i = 0; i < SHAFT_SAMPLES; i++)
    {
        at += delta;
        sum += tex2Dlod(SkySampler, float4(at, 0, 0)).rgb * decay;
        decay *= ShaftLength;
    }
    sum /= SHAFT_SAMPLES * 0.25;
    return float4(sum * sun.z, 1.0);
}

// Pass 4 and 5: fog and composite ----------------------------------------------------------------------------------

float3 PS_Composite(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float3 color = tex2D(ReShade::BackBuffer, uv).rgb;
    float4 sun = tex2Dfetch(SunSampler, int2(0, 0));
    float3 tint = tex2Dfetch(SunColorSampler, int2(0, 0)).rgb;
    float depth = ReShade::GetLinearizedDepth(uv);
    float sky = IsSky(uv);

    // No depth buffer to read (cleared to one value everywhere): nothing to do.
    float valid = 1.0 - step(0.9999, ReShade::GetLinearizedDepth(float2(0.5, 0.75)))
        * step(0.9999, ReShade::GetLinearizedDepth(float2(0.5, 0.95)));

    // Fog: exponential with distance past its start; never on the sky.
    float distance = max(depth - FogStart, 0.0);
    float fog = (1.0 - exp(-distance * FogDensity * 3.0)) * FogAmount * (1.0 - sky) * valid;
    float2 toSun = (uv - sun.xy) * float2(BUFFER_ASPECT_RATIO, 1.0);
    float glow = exp(-dot(toSun, toSun) * 6.0) * SunScatter * sun.z;
    float3 fogColor = tint * (0.85 + glow);
    color = lerp(color, fogColor, saturate(fog));

    // Light shafts over everything but the sky's own pixels (they are the source).
    float3 shafts = tex2D(ShaftSampler, uv).rgb * ShaftIntensity * valid;
    color += shafts * (1.0 - sky * 0.6);
    return color;
}

technique Emberforge_Volumetrics <
    ui_label = "Emberforge Volumetrics";
    ui_tooltip = "Light shafts from the sun and atmospheric fog lit by it.";
>
{
    pass Sky { VertexShader = PostProcessVS; PixelShader = PS_Sky; RenderTarget = SkyTex; GenerateMipMaps = true; }
    pass Sun { VertexShader = PostProcessVS; PixelShader = PS_Sun; RenderTarget0 = SunTex; RenderTarget1 = SunColorTex; }
    pass KeepSun
    {
        VertexShader = PostProcessVS; PixelShader = PS_KeepSun;
        RenderTarget0 = SunPrevTex; RenderTarget1 = SunColorPrevTex;
    }
    pass Shafts { VertexShader = PostProcessVS; PixelShader = PS_Shafts; RenderTarget = ShaftTex; }
    pass Composite { VertexShader = PostProcessVS; PixelShader = PS_Composite; }
}
