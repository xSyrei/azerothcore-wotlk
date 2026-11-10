/*
 * Emberforge Bloom & Grade: glow around bright light and a colour grade for the WoW 3.3.5a client, a ReShade effect.
 *
 *  1. Bloom: what is brighter than the threshold (spells, torches, the sun on water, glowing runes), blurred wide
 *     over three sizes (a quarter, an eighth and a sixteenth of the screen) and added back softly.
 *  2. Sharpen: detail lifted where the picture is flat (contrast-adaptive, so edges do not ring).
 *  3. Grade: vibrance (lifts the dull colours more than the vivid ones, so skin and spells do not burn), a gentle
 *     S-curve for contrast, warmth, and a light vignette.
 * Last in ReShadePreset.ini's order, over the light shafts and fog.
 */

#include "ReShade.fxh"
#include "Emberforge.fxh"

uniform float BloomIntensity <
    ui_type = "slider"; ui_min = 0.0; ui_max = 2.0; ui_step = 0.01;
    ui_category = "Bloom"; ui_label = "Intensity";
> = 1.2;

uniform float BloomThreshold <
    ui_type = "slider"; ui_min = 0.3; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Bloom"; ui_label = "Threshold";
    ui_tooltip = "Only light brighter than this glows.";
> = 0.5;

uniform float Sharpen <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.5; ui_step = 0.01;
    ui_category = "Grade"; ui_label = "Sharpen";
    ui_tooltip = "Crisper textures (the old game's are soft).";
> = 0.4;

uniform float Vibrance <
    ui_type = "slider"; ui_min = -1.0; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Grade"; ui_label = "Vibrance";
> = 0.33;

uniform float Contrast <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Grade"; ui_label = "Contrast";
> = 0.59;

uniform float Warmth <
    ui_type = "slider"; ui_min = -1.0; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Grade"; ui_label = "Warmth";
> = 0.6;

uniform float Vignette <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Grade"; ui_label = "Vignette";
> = 0.17;

texture Bloom1 { Width = BUFFER_WIDTH / 4; Height = BUFFER_HEIGHT / 4; Format = RGBA16F; };
texture Bloom1b { Width = BUFFER_WIDTH / 4; Height = BUFFER_HEIGHT / 4; Format = RGBA16F; };
texture Bloom2 { Width = BUFFER_WIDTH / 8; Height = BUFFER_HEIGHT / 8; Format = RGBA16F; };
texture Bloom2b { Width = BUFFER_WIDTH / 8; Height = BUFFER_HEIGHT / 8; Format = RGBA16F; };
texture Bloom3 { Width = BUFFER_WIDTH / 16; Height = BUFFER_HEIGHT / 16; Format = RGBA16F; };
texture Bloom3b { Width = BUFFER_WIDTH / 16; Height = BUFFER_HEIGHT / 16; Format = RGBA16F; };
sampler Bloom1S { Texture = Bloom1; };
sampler Bloom1bS { Texture = Bloom1b; };
sampler Bloom2S { Texture = Bloom2; };
sampler Bloom2bS { Texture = Bloom2b; };
sampler Bloom3S { Texture = Bloom3; };
sampler Bloom3bS { Texture = Bloom3b; };

float Luma(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

// The bright part, with a soft knee (no hard edge where the threshold cuts).
float4 PS_Bright(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float3 c = 0.0;
    [unroll]
    for (int y = -1; y <= 1; y++)
        [unroll]
        for (int x = -1; x <= 1; x++)
            c += tex2D(ReShade::BackBuffer, uv + float2(x, y) * BUFFER_PIXEL_SIZE * 1.5).rgb;
    c /= 9.0;
    float l = Luma(c);
    float knee = smoothstep(BloomThreshold - 0.1, BloomThreshold + 0.15, l);
    return float4(c * knee, 1.0);
}

float4 Blur(sampler s, float2 uv, float2 step)
{
    // 9 taps, Gaussian weights.
    static const float w[5] = { 0.2270270, 0.1945946, 0.1216216, 0.0540540, 0.0162162 };
    float3 c = tex2D(s, uv).rgb * w[0];
    [unroll]
    for (int i = 1; i < 5; i++)
        c += (tex2D(s, uv + step * i).rgb + tex2D(s, uv - step * i).rgb) * w[i];
    return float4(c, 1.0);
}

float4 PS_Blur1X(float4 p : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    return Blur(Bloom1S, uv, float2(4.0 * BUFFER_RCP_WIDTH, 0));
}

float4 PS_Blur1Y(float4 p : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    return Blur(Bloom1bS, uv, float2(0, 4.0 * BUFFER_RCP_HEIGHT));
}

float4 PS_Down2(float4 p : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    return tex2D(Bloom1S, uv);
}

float4 PS_Blur2X(float4 p : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    return Blur(Bloom2S, uv, float2(8.0 * BUFFER_RCP_WIDTH, 0));
}

float4 PS_Blur2Y(float4 p : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    return Blur(Bloom2bS, uv, float2(0, 8.0 * BUFFER_RCP_HEIGHT));
}

float4 PS_Down3(float4 p : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    return tex2D(Bloom2S, uv);
}

float4 PS_Blur3X(float4 p : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    return Blur(Bloom3S, uv, float2(16.0 * BUFFER_RCP_WIDTH, 0));
}

float4 PS_Blur3Y(float4 p : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    return Blur(Bloom3bS, uv, float2(0, 16.0 * BUFFER_RCP_HEIGHT));
}

float3 PS_Combine(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float3 color = tex2D(ReShade::BackBuffer, uv).rgb;
    // Only in the world, and never on the marker's pixels (Emberforge.fxh).
    if (EmberforgeInWorld() < 0.5 || EmberforgeMarkerPixel(pos))
        return color;

    // Sharpen: the difference from the four neighbours, less where they already differ a lot (an edge).
    float3 n = tex2D(ReShade::BackBuffer, uv + float2(0, -BUFFER_RCP_HEIGHT)).rgb;
    float3 s = tex2D(ReShade::BackBuffer, uv + float2(0, BUFFER_RCP_HEIGHT)).rgb;
    float3 e = tex2D(ReShade::BackBuffer, uv + float2(BUFFER_RCP_WIDTH, 0)).rgb;
    float3 w = tex2D(ReShade::BackBuffer, uv + float2(-BUFFER_RCP_WIDTH, 0)).rgb;
    float3 low = min(min(min(n, s), min(e, w)), color), high = max(max(max(n, s), max(e, w)), color);
    float adapt = saturate(1.0 - (high.g - low.g) * 2.5);
    color = clamp(color + (color * 4.0 - n - s - e - w) * 0.25 * Sharpen * adapt, low, high);

    float3 bloom = tex2D(Bloom1S, uv).rgb * 0.5 + tex2D(Bloom2S, uv).rgb * 0.3 + tex2D(Bloom3S, uv).rgb * 0.2;
    // Screen blend: bright areas glow without clipping to white.
    color = 1.0 - (1.0 - color) * (1.0 - saturate(bloom * BloomIntensity));

    // Vibrance: more for the dull colours.
    float l = Luma(color);
    float sat = max(color.r, max(color.g, color.b)) - min(color.r, min(color.g, color.b));
    color = lerp(l.xxx, color, 1.0 + Vibrance * (1.0 - sat));

    // Contrast: an S-curve around the middle grey.
    float3 curved = color * color * (3.0 - 2.0 * color);
    color = lerp(color, curved, Contrast);

    // Warmth: a little more red, a little less blue in the light.
    color *= float3(1.0 + Warmth * 0.06, 1.0 + Warmth * 0.01, 1.0 - Warmth * 0.07);

    // Vignette.
    float2 d = (uv - 0.5) * float2(BUFFER_ASPECT_RATIO, 1.0);
    color *= 1.0 - Vignette * smoothstep(0.35, 1.0, length(d));
    return saturate(color);
}

technique Emberforge_Bloom <
    ui_label = "Emberforge Bloom & Grade";
    ui_tooltip = "Glow around bright light, vibrance, contrast, warmth and a light vignette.";
>
{
    pass Bright { VertexShader = PostProcessVS; PixelShader = PS_Bright; RenderTarget = Bloom1; }
    pass Blur1X { VertexShader = PostProcessVS; PixelShader = PS_Blur1X; RenderTarget = Bloom1b; }
    pass Blur1Y { VertexShader = PostProcessVS; PixelShader = PS_Blur1Y; RenderTarget = Bloom1; }
    pass Down2 { VertexShader = PostProcessVS; PixelShader = PS_Down2; RenderTarget = Bloom2; }
    pass Blur2X { VertexShader = PostProcessVS; PixelShader = PS_Blur2X; RenderTarget = Bloom2b; }
    pass Blur2Y { VertexShader = PostProcessVS; PixelShader = PS_Blur2Y; RenderTarget = Bloom2; }
    pass Down3 { VertexShader = PostProcessVS; PixelShader = PS_Down3; RenderTarget = Bloom3; }
    pass Blur3X { VertexShader = PostProcessVS; PixelShader = PS_Blur3X; RenderTarget = Bloom3b; }
    pass Blur3Y { VertexShader = PostProcessVS; PixelShader = PS_Blur3Y; RenderTarget = Bloom3; }
    pass Combine { VertexShader = PostProcessVS; PixelShader = PS_Combine; }
}
