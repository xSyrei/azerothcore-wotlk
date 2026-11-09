/*
 * Emberforge Ambient Occlusion: soft contact shadows for the WoW 3.3.5a client, a ReShade effect.
 *
 * The old engine lights a corner the same as an open wall, so props float and rooms look flat. This darkens what the
 * depth buffer shows to be tucked in (creases, the foot of a wall, under a table, between rocks):
 *  1. Occlusion at half resolution: around each pixel, samples on a turning spiral; a sample nearer the eye than the
 *     pixel (within the radius: not a distant wall behind a silhouette) hides part of its sky.
 *  2. A blur that keeps to the same depth, so the shading does not bleed across edges.
 *  3. Multiplied into the picture; never on the sky.
 * Before the light shafts and fog (ReShadePreset.ini's order), so the fog still covers distant shading.
 */

#include "ReShade.fxh"

uniform float Strength <
    ui_type = "slider"; ui_min = 0.0; ui_max = 2.0; ui_step = 0.01;
    ui_label = "Strength";
    ui_tooltip = "How dark the occluded corners get.";
> = 1.25;

uniform float Radius <
    ui_type = "slider"; ui_min = 0.2; ui_max = 4.0; ui_step = 0.01;
    ui_label = "Radius";
    ui_tooltip = "How far a crease reaches out (larger: broader, softer shading).";
> = 1.4;

uniform float FadeDistance <
    ui_type = "slider"; ui_min = 0.02; ui_max = 1.0; ui_step = 0.01;
    ui_label = "Fade distance";
    ui_tooltip = "Beyond this share of the view distance the shading fades out.";
> = 0.25;

#define AO_SAMPLES 12

// 1 while the depth buffer looks real, 0 on a frame where ReShade picked an empty or wrong one (all far plane, all
// zero, or one value everywhere): the occlusion is skipped then, not drawn black.
texture AOValidTex { Width = 1; Height = 1; Format = R16F; };
sampler AOValidSampler { Texture = AOValidTex; MinFilter = POINT; MagFilter = POINT; };

texture AOTex { Width = BUFFER_WIDTH / 2; Height = BUFFER_HEIGHT / 2; Format = RG16F; };
sampler AOSampler { Texture = AOTex; };
texture AOBlurTex { Width = BUFFER_WIDTH / 2; Height = BUFFER_HEIGHT / 2; Format = RG16F; };
sampler AOBlurSampler { Texture = AOBlurTex; };

float Depth(float2 uv)
{
    return ReShade::GetLinearizedDepth(uv);
}

float PS_Check(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float far = 0.0, zero = 0.0, low = 1.0, high = 0.0;
    [loop]
    for (int y = 0; y < 8; y++)
    {
        [loop]
        for (int x = 0; x < 8; x++)
        {
            float d = Depth((float2(x, y) + 0.5) / 8.0);
            far += step(0.9995, d);
            zero += step(d, 0.0001);
            low = min(low, d);
            high = max(high, d);
        }
    }
    return (far < 56.0 && zero < 32.0 && high - low > 0.001) ? 1.0 : 0.0;
}

// x: how occluded (0-1); y: the pixel's depth, for the blur.
float2 PS_Occlusion(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float depth = Depth(uv);
    // The sky, and no depth at all (an empty buffer: dividing by it made black pixels, the flashing).
    if (depth >= 0.9995 || depth < 0.0001)
        return float2(0.0, depth);

    // The radius on screen shrinks with distance; a turn per pixel (interleaved) hides the sample pattern.
    float2 radius = min(Radius * 0.001 / max(depth, 0.005), 0.12) * float2(1.0 / BUFFER_ASPECT_RATIO, 1.0);
    float turn = frac(sin(dot(floor(pos.xy), float2(12.9898, 78.233))) * 43758.5453) * 6.2831853;
    // The surface's own slope on screen (the smaller step each way, so an edge does not count as slope): a sample is
    // only occluding when it stands out of that plane toward the eye. Without it a flat wall or floor seen at an
    // angle darkened in blotches, half its samples always being nearer.
    float2 pixel = BUFFER_PIXEL_SIZE * 2.0;
    float right = Depth(uv + float2(pixel.x, 0)) - depth, left = depth - Depth(uv - float2(pixel.x, 0));
    float down = Depth(uv + float2(0, pixel.y)) - depth, up = depth - Depth(uv - float2(0, pixel.y));
    float2 slope = float2(abs(right) < abs(left) ? right : left, abs(down) < abs(up) ? down : up) / pixel;
    float occlusion = 0.0;
    [unroll]
    for (int i = 0; i < AO_SAMPLES; i++)
    {
        float t = (i + 0.5) / AO_SAMPLES;
        float angle = turn + i * 2.3999632;        // the golden angle
        float2 offset = float2(cos(angle), sin(angle)) * radius * sqrt(t);
        float sampled = Depth(uv + offset);
        float expected = depth + dot(offset, slope);
        float nearer = (expected - sampled) / depth - 0.004;  // how much nearer the eye than the plane, relative
        // Nearer, but not by so much it is another object in front (a fade over the range).
        occlusion += saturate(nearer * 40.0) * saturate(1.0 - (nearer - 0.08) * 8.0);
    }
    occlusion /= AO_SAMPLES;
    float fade = saturate(1.0 - depth / FadeDistance);
    return float2(occlusion * fade, depth);
}

float2 Blur(sampler source, float2 uv, float2 direction)
{
    float2 centre = tex2D(source, uv).xy;
    float sum = centre.x, weight = 1.0;
    [unroll]
    for (int i = -4; i <= 4; i++)
    {
        if (i == 0)
            continue;
        float2 s = tex2D(source, uv + direction * i * 2.0).xy;
        // Only from the same surface: weights fall off with the depth difference.
        float w = exp(-abs(s.y - centre.y) / max(centre.y, 1e-4) * 60.0) * exp(-i * i / 18.0);
        sum += s.x * w;
        weight += w;
    }
    return float2(sum / weight, centre.y);
}

float2 PS_BlurX(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    return Blur(AOSampler, uv, float2(BUFFER_RCP_WIDTH, 0.0));
}

float2 PS_BlurY(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    return Blur(AOBlurSampler, uv, float2(0.0, BUFFER_RCP_HEIGHT));
}

float3 PS_Apply(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float3 color = tex2D(ReShade::BackBuffer, uv).rgb;
    float occlusion = tex2D(AOSampler, uv).x * tex2Dfetch(AOValidSampler, int2(0, 0)).x;
    // A bad value (a NaN from bad depth) must never darken the picture.
    if (!(occlusion >= 0.0))
        occlusion = 0.0;
    return color * (1.0 - saturate(occlusion * Strength));
}

technique Emberforge_AO <
    ui_label = "Emberforge Ambient Occlusion";
    ui_tooltip = "Soft contact shadows in creases, corners and under objects.";
>
{
    pass Check { VertexShader = PostProcessVS; PixelShader = PS_Check; RenderTarget = AOValidTex; }
    pass Occlusion { VertexShader = PostProcessVS; PixelShader = PS_Occlusion; RenderTarget = AOTex; }
    pass BlurX { VertexShader = PostProcessVS; PixelShader = PS_BlurX; RenderTarget = AOBlurTex; }
    pass BlurY { VertexShader = PostProcessVS; PixelShader = PS_BlurY; RenderTarget = AOTex; }
    pass Apply { VertexShader = PostProcessVS; PixelShader = PS_Apply; }
}
