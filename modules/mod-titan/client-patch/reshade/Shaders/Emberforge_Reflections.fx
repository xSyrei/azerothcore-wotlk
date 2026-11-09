/*
 * Emberforge Water Reflections: the city mirrored in the water (lamps, lit windows, walls), rippled, for the WoW
 * 3.3.5a client, a ReShade effect. The game's water can only mirror a fixed sky picture.
 *
 *  1. Where the water is: Emberforge's water shader (patch-enUS-7, psLiquidProcWater) signs every pixel of opaque
 *     water: the lowest bit of red, green and blue is 1, 0, 1 where x + y is even and 0, 1, 0 where it is odd (or the
 *     other way round: the phase is not assumed). A pixel is water when at least 8 of its 3x3 neighbourhood carry it:
 *     a flat colour can never match both phases, and noise one time in billions, so streets and floors stay matte.
 *     The game's Full Screen Glow repaints every pixel and wipes the signature (the launcher turns it off; this
 *     effect's bloom replaces it).
 *  2. Which way is up: the water's surface normal, from the depth buffer, averaged over all the water on screen.
 *  3. The reflection: from each water pixel a ray mirrored about that normal (bent by moving ripples) is marched
 *     through the depth buffer; where it passes behind something, that is what the water mirrors. Rays that leave the
 *     screen or reach the sky keep the water's own colour (its sky reflection).
 * First in ReShadePreset.ini's order: every later effect changes the colours and would wipe the signature.
 */

#include "ReShade.fxh"

uniform float Strength <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
    ui_label = "Strength";
    ui_tooltip = "How much the water mirrors what is around it.";
> = 0.85;

uniform float Ripples <
    ui_type = "slider"; ui_min = 0.0; ui_max = 2.0; ui_step = 0.01;
    ui_label = "Ripples";
    ui_tooltip = "How much the moving ripples break the reflection up.";
> = 1.0;

uniform float FieldOfView <
    ui_type = "slider"; ui_min = 60.0; ui_max = 120.0; ui_step = 1.0;
    ui_label = "Field of view";
    ui_tooltip = "The game camera's diagonal field of view in degrees (90 unless changed). Reflections that sit too\n"
                 "high or too low mean it is off.";
> = 90.0;

uniform bool ShowWater <
    ui_label = "Show the water it found";
    ui_tooltip = "Tints in red what it treats as water, to check it finds the water and only the water.";
> = false;

uniform float Timer < source = "timer"; >;

#define MARCH_STEPS 32
// ReShade's linearised depth is a share of the far plane; times this it is roughly yards.
#define YARDS 1000.0

texture WaterMaskTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = R8; };
sampler WaterMask { Texture = WaterMaskTex; };
// The surface normal times the water's share, over a grid; its smallest mip is the average over the screen.
texture WaterUpTex { Width = 256; Height = 256; Format = RGBA16F; MipLevels = 9; };
sampler WaterUp { Texture = WaterUpTex; };

float2 TanHalfFov()
{
    float diagonal = tan(radians(FieldOfView) * 0.5);
    float y = diagonal / sqrt(1.0 + BUFFER_ASPECT_RATIO * BUFFER_ASPECT_RATIO);
    return float2(y * BUFFER_ASPECT_RATIO, y);
}

// View space (x right, y up, z forward) in yards.
float3 ViewPos(float2 uv)
{
    float z = ReShade::GetLinearizedDepth(uv) * YARDS;
    float2 t = TanHalfFov();
    return float3((uv.x * 2.0 - 1.0) * t.x, (1.0 - uv.y * 2.0) * t.y, 1.0) * z;
}

float2 Project(float3 p)
{
    float2 t = TanHalfFov();
    return float2(p.x / (p.z * t.x) * 0.5 + 0.5, 0.5 - p.y / (p.z * t.y) * 0.5);
}

// 1 for odd whole numbers, 0 for even.
float Odd(float n)
{
    return n - 2.0 * floor(n * 0.5);
}

float3 Odd(float3 n)
{
    return n - 2.0 * floor(n * 0.5);
}

// How many of the 3x3 pixels carry the signature in one phase (parity 0 or 1 for the even pixels).
float Signed(int2 p, float phase)
{
    float count = 0.0;
    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            float3 c = tex2Dfetch(ReShade::BackBuffer, p + int2(x, y)).rgb;
            float3 bits = Odd(floor(c * 255.0 + 0.5));
            float odd = Odd(float(p.x + x + p.y + y) + phase);
            float3 expected = odd > 0.5 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 1.0);
            count += all(bits == expected) ? 1.0 : 0.0;
        }
    }
    return count;
}

float PS_Mask(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    int2 p = int2(pos.xy);
    return max(Signed(p, 0.0), Signed(p, 1.0)) >= 8.0 ? 1.0 : 0.0;
}

float3 SurfaceNormal(float2 uv)
{
    float2 px = BUFFER_PIXEL_SIZE * 3.0;
    float3 p = ViewPos(uv);
    float3 dx = ViewPos(uv + float2(px.x, 0.0)) - p;
    float3 dy = ViewPos(uv + float2(0.0, px.y)) - p;
    float3 n = normalize(cross(dy, dx));
    return dot(n, -p) < 0.0 ? -n : n;
}

float4 PS_Up(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float water = tex2Dlod(WaterMask, float4(uv, 0, 0)).x;
    float depth = ReShade::GetLinearizedDepth(uv);
    if (water < 0.5 || depth <= 0.0001 || depth >= 0.9995)
        return 0.0;
    float3 n = SurfaceNormal(uv);
    if (!(n.y > 0.0))                  // a NaN, or a face no water has
        return 0.0;
    return float4(n, 1.0);
}

// The ripples' slope on the water's plane (coordinates in yards), from a few travelling waves.
float2 RippleSlope(float2 w, float time)
{
    static const float3 waves[4] = {
        float3(0.80, 0.60, 4.1), float3(-0.45, 0.89, 6.3), float3(0.20, -0.98, 9.7), float3(-0.93, -0.37, 14.2) };
    float2 slope = 0.0;
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        float2 d = waves[i].xy;
        float k = waves[i].z;
        float speed = sqrt(9.8 / k) * 1.4;        // deep water: longer waves run faster
        slope += d * cos(dot(w, d) * k - time * speed * k) * (0.05 / sqrt(k)) * k * 0.25;
    }
    return slope;
}

float3 PS_Reflect(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float3 color = tex2D(ReShade::BackBuffer, uv).rgb;
    // A soft edge: the share of water around the pixel.
    float2 px = BUFFER_PIXEL_SIZE * 3.0;
    float water = (tex2D(WaterMask, uv).x * 2.0 + tex2D(WaterMask, uv + float2(px.x, 0)).x
        + tex2D(WaterMask, uv - float2(px.x, 0)).x + tex2D(WaterMask, uv + float2(0, px.y)).x
        + tex2D(WaterMask, uv - float2(0, px.y)).x) / 6.0;
    float3 result = color;
    float4 sum = tex2Dlod(WaterUp, float4(0.5, 0.5, 0, 8));
    if (water > 0.01 && sum.w > 1e-5)
    {
        float3 up = normalize(sum.xyz / sum.w);
        float3 p = ViewPos(uv);
        float3 v = normalize(p);

        // Ripples, on the plane's own axes, fading with distance (beyond, they would only shimmer).
        float3 t = normalize(cross(up, abs(up.z) < 0.95 ? float3(0, 0, 1) : float3(1, 0, 0)));
        float3 b = cross(t, up);
        float2 slope = RippleSlope(float2(dot(p, t), dot(p, b)), Timer * 0.001);
        slope *= Ripples * saturate(1.0 - p.z / 150.0);
        float3 n = normalize(up - slope.x * t - slope.y * b);

        float3 r = reflect(v, n);
        r = normalize(r + up * max(0.0, 0.03 - dot(r, up)));   // never down into the water

        float hit = 0.0;
        float2 hitUv = uv;
        float dist = 0.4;
        [loop]
        for (int i = 0; i < MARCH_STEPS; i++)
        {
            float3 q = p + r * dist;
            if (q.z <= 0.1)
                break;
            float2 quv = Project(q);
            if (any(quv < 0.0) || any(quv > 1.0))
                break;
            float scene = ReShade::GetLinearizedDepth(quv) * YARDS;
            float behind = q.z - scene;
            if (behind > 0.0 && behind < max(1.5, dist * 0.25) && scene < YARDS * 0.999)
            {
                // Refine between this step and the last.
                float lo = dist / 1.22 - 0.25, hi = dist;
                [unroll]
                for (int j = 0; j < 5; j++)
                {
                    float mid = (lo + hi) * 0.5;
                    float3 m = p + r * mid;
                    float2 muv = Project(m);
                    if (m.z > ReShade::GetLinearizedDepth(muv) * YARDS)
                        hi = mid;
                    else
                        lo = mid;
                }
                hitUv = Project(p + r * hi);
                hit = 1.0;
                break;
            }
            dist = dist * 1.22 + 0.25;
        }

        if (hit > 0.0)
        {
            float3 mirrored = tex2Dlod(ReShade::BackBuffer, float4(hitUv, 0, 0)).rgb;
            // Fades: toward the screen's edges (where the ray ran out of picture) and onto other water.
            float2 edge = saturate(min(hitUv, 1.0 - hitUv) * 10.0);
            float other = tex2Dlod(WaterMask, float4(hitUv, 0, 0)).x;
            float fresnel = 0.02 + 0.98 * pow(1.0 - saturate(dot(-v, n)), 5.0);
            float share = lerp(0.45, 1.0, fresnel) * edge.x * edge.y * (1.0 - other) * water * Strength;
            result = lerp(color, mirrored * 0.9, saturate(share));
        }
    }
    if (ShowWater)
        result = lerp(result, float3(1.0, 0.1, 0.1), water * 0.5);
    return result;
}

technique Emberforge_Reflections <
    ui_label = "Emberforge Water Reflections";
    ui_tooltip = "Mirrors the city, lamps and lights in the water, broken up by moving ripples. Needs Emberforge's\n"
                 "water (patch-enUS-7) and the game's Full Screen Glow off.";
>
{
    pass Mask { VertexShader = PostProcessVS; PixelShader = PS_Mask; RenderTarget = WaterMaskTex; }
    pass Up { VertexShader = PostProcessVS; PixelShader = PS_Up; RenderTarget = WaterUpTex; }
    pass Reflect { VertexShader = PostProcessVS; PixelShader = PS_Reflect; }
}
