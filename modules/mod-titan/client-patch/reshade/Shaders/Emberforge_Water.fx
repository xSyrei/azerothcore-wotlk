/*
 * Emberforge Water: the water drawn by ReShade, every part of its look on a slider, for the WoW 3.3.5a client.
 *
 * While Lighting is on, the launcher installs a variant of Emberforge's water shader (Water-ReShade.MPQ, as
 * patch-enUS-9.MPQ) that hands this effect what it needs instead of a finished picture, in a checkerboard:
 * - even pixels (x + y even): the water's own colour (by depth, this zone and hour, fogged); the lowest bit of red,
 *   green and blue is 1, 0, 1;
 * - odd pixels: where it is in the world, x and y modulo 128 yards in 2048 steps: red and green the top 8 bits, blue's
 *   bits 1-3 and 4-6 the low 3 of x and y, blue's bit 0 is 0 and bit 7 is 1.
 * A flat colour can never be both, so streets and floors never pass for water. All water is handed over, made
 * opaque, however shallow.
 *
 *  1. Classify: how many of each pixel's 3x3 fit the checkerboard, in either phase.
 *  2. Decode: the colour and world position (a pass each) at every pixel of water (and at its edge pixels).
 *  3. Fill: pixels inside the water that lost it (a raindrop, a spell, a thin line drawn over it) take it from around.
 *  4. Up and sky: the water's normal from the depth buffer, and the sky's colour, each averaged over the screen.
 *  5. Shade: waves from the world position (so they stay put as the camera moves), colour grading, the city mirrored
 *     by a ray marched through the depth buffer (bent by the waves), the sky where it hits nothing, and sparkle.
 * First in ReShadePreset.ini's order: every later effect changes the colours and would wipe what the water hands over.
 * The game's Full Screen Glow repaints every pixel, so the launcher turns it off.
 */

#include "ReShade.fxh"

// ---- Colour ----
uniform float HueShift <
    ui_type = "slider"; ui_min = -180.0; ui_max = 180.0; ui_step = 1.0;
    ui_category = "Water colour"; ui_label = "Hue shift";
    ui_tooltip = "Turns the water's colour around the colour wheel (negative: toward green, positive: toward purple).";
> = 0.0;

uniform float Saturation <
    ui_type = "slider"; ui_min = 0.0; ui_max = 2.0; ui_step = 0.01;
    ui_category = "Water colour"; ui_label = "Saturation";
> = 1.0;

uniform float Brightness <
    ui_type = "slider"; ui_min = 0.0; ui_max = 2.0; ui_step = 0.01;
    ui_category = "Water colour"; ui_label = "Brightness";
> = 0.8;

uniform float3 TintColour <
    ui_type = "color";
    ui_category = "Water colour"; ui_label = "Tint colour";
> = float3(0.08, 0.32, 0.36);

uniform float TintAmount <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Water colour"; ui_label = "Tint amount";
    ui_tooltip = "How much the tint colour replaces the zone's own water colour (kept dimmed at night).";
> = 0.25;

// ---- Waves ----
uniform float WaveSize <
    ui_type = "slider"; ui_min = 0.5; ui_max = 20.0; ui_step = 0.1;
    ui_category = "Waves"; ui_label = "Wave size (yards)";
    ui_tooltip = "The length of the largest waves; the others are shorter.";
> = 4.0;

uniform float WaveHeight <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Waves"; ui_label = "Wave height";
    ui_tooltip = "How steep the waves are: 0 is a mirror.";
> = 0.25;

uniform float WaveSpeed <
    ui_type = "slider"; ui_min = 0.0; ui_max = 3.0; ui_step = 0.01;
    ui_category = "Waves"; ui_label = "Wave speed";
    ui_tooltip = "1 is real water's speed for waves that long.";
> = 0.5;

uniform float WindDirection <
    ui_type = "slider"; ui_min = 0.0; ui_max = 360.0; ui_step = 1.0;
    ui_category = "Waves"; ui_label = "Wind direction";
> = 40.0;

uniform float WaveSpread <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Waves"; ui_label = "Spread";
    ui_tooltip = "0: every wave runs with the wind (long parallel swells); 1: waves from every side (choppy).";
> = 0.45;

uniform float Ripples <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Waves"; ui_label = "Small ripples";
    ui_tooltip = "Fine ripples over the waves.";
> = 0.35;

uniform float WaveShading <
    ui_type = "slider"; ui_min = 0.0; ui_max = 2.0; ui_step = 0.01;
    ui_category = "Waves"; ui_label = "Wave shading";
    ui_tooltip = "How much the waves' faces turned to the light are brighter and those turned away darker.";
> = 0.4;

uniform float LightDirection <
    ui_type = "slider"; ui_min = 0.0; ui_max = 360.0; ui_step = 1.0;
    ui_category = "Waves"; ui_label = "Light direction";
    ui_tooltip = "Where the light comes from, for the wave shading and the sparkle.";
> = 220.0;

// ---- Reflections ----
uniform float ReflectionDown <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Reflections"; ui_label = "Reflectiveness looking down";
> = 0.3;

uniform float ReflectionGlance <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Reflections"; ui_label = "Reflectiveness at a glance";
    ui_tooltip = "Looking across the water (real water: almost a mirror).";
> = 0.9;

uniform float ReflectionBrightness <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.5; ui_step = 0.01;
    ui_category = "Reflections"; ui_label = "Reflection brightness";
> = 0.85;

uniform float Distortion <
    ui_type = "slider"; ui_min = 0.0; ui_max = 3.0; ui_step = 0.01;
    ui_category = "Reflections"; ui_label = "Distortion";
    ui_tooltip = "How much the waves break the reflection up.";
> = 1.0;

uniform float SkyReflection <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.5; ui_step = 0.01;
    ui_category = "Reflections"; ui_label = "Sky in the water";
    ui_tooltip = "The sky's colour where the reflection finds nothing on screen.";
> = 0.8;

// ---- Sparkle ----
uniform float Sparkle <
    ui_type = "slider"; ui_min = 0.0; ui_max = 3.0; ui_step = 0.01;
    ui_category = "Sparkle"; ui_label = "Sparkle";
    ui_tooltip = "Glints of light on the wave crests (follows the sky's brightness: none at night).";
> = 0.6;

uniform float SparkleSize <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Sparkle"; ui_label = "Sparkle size";
> = 0.3;

// ---- Other ----
uniform float FieldOfView <
    ui_type = "slider"; ui_min = 60.0; ui_max = 120.0; ui_step = 1.0;
    ui_category = "Other"; ui_label = "Field of view";
    ui_tooltip = "The game camera's diagonal field of view in degrees (90 unless changed). Reflections that sit too\n"
                 "high or too low mean it is off.";
> = 90.0;

uniform bool ShowWater <
    ui_category = "Other"; ui_label = "Show the water it found";
    ui_tooltip = "Tints in red what it treats as water.";
> = false;

uniform float Timer < source = "timer"; >;

#define MARCH_STEPS 32
#define YARDS 1000.0       // ReShade's linearised depth is a share of the far plane; times this it is roughly yards
#define PERIOD 128.0       // the world position wraps every 128 yards

texture EfWaterClassTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RG16F; };
sampler EfWaterClassS { Texture = EfWaterClassTex; MinFilter = POINT; MagFilter = POINT; };
texture EfWaterColourTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
sampler EfWaterColourS { Texture = EfWaterColourTex; MinFilter = POINT; MagFilter = POINT; };
texture EfWaterWorldTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RG32F; };
sampler EfWaterWorldS { Texture = EfWaterWorldTex; MinFilter = POINT; MagFilter = POINT; };
texture EfWaterColourFillTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
sampler EfWaterColourFillS { Texture = EfWaterColourFillTex; MinFilter = POINT; MagFilter = POINT; };
texture EfWaterWorldFillTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RG32F; };
sampler EfWaterWorldFillS { Texture = EfWaterWorldFillTex; MinFilter = POINT; MagFilter = POINT; };
// Sums over a grid whose smallest mip is the screen's average: the water's normal, and the sky's colour.
texture EfWaterUpTex { Width = 256; Height = 256; Format = RGBA16F; MipLevels = 9; };
sampler EfWaterUpS { Texture = EfWaterUpTex; };
texture EfWaterSkyTex { Width = 128; Height = 128; Format = RGBA16F; MipLevels = 8; };
sampler EfWaterSkyS { Texture = EfWaterSkyTex; };

// ---------------------------------------------------------------------------------------------------------------
// Decoding

float Odd(float n)
{
    return n - 2.0 * floor(n * 0.5);
}

float3 Bytes(int2 p)
{
    return floor(tex2Dfetch(ReShade::BackBuffer, p).rgb * 255.0 + 0.5);
}

// Whether a pixel fits its role: colour (even) or position (odd).
bool Fits(float3 b, bool even)
{
    if (even)
        return Odd(b.r) > 0.5 && Odd(b.g) < 0.5 && Odd(b.b) > 0.5;
    return Odd(b.b) < 0.5 && b.b >= 128.0;
}

// Even in phase 0 when x + y is even; phase 1 the other way round.
bool IsEven(int2 p, float phase)
{
    return Odd(float(p.x + p.y) + phase) < 0.5;
}

float2 Position(float3 b)
{
    float low = b.b - 128.0;                                    // 16 y + 2 x (+ 0)
    float lowY = floor(low / 16.0);
    float lowX = floor((low - lowY * 16.0) / 2.0);
    return float2(b.r * 8.0 + lowX, b.g * 8.0 + lowY) / 16.0;   // yards, 0-128
}

float2 Unwrap(float2 d)
{
    return d - PERIOD * floor(d / PERIOD + 0.5);
}

// x: how many of the 3x3 fit in phase 0, y: in phase 1 (each over 9).
float2 PS_Classify(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    int2 p = int2(pos.xy);
    float2 count = 0.0;
    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            int2 q = p + int2(x, y);
            float3 b = Bytes(q);
            count.x += Fits(b, IsEven(q, 0.0)) ? 1.0 : 0.0;
            count.y += Fits(b, IsEven(q, 1.0)) ? 1.0 : 0.0;
        }
    }
    return count / 9.0;
}

// Whether this pixel is water (7 of 9 fit; or at its edge: it fits and a neighbour is water), and its phase.
bool IsWater(int2 p, out float phase)
{
    float2 own = tex2Dfetch(EfWaterClassS, p).xy;
    phase = own.y > own.x ? 1.0 : 0.0;
    bool water = max(own.x, own.y) >= 6.5 / 9.0;
    bool fits = Fits(Bytes(p), IsEven(p, phase));
    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            float2 c = tex2Dfetch(EfWaterClassS, p + int2(x, y)).xy;
            water = water || (fits && (phase > 0.5 ? c.y : c.x) >= 6.5 / 9.0);
        }
    }
    return water;
}

// The water's colour: the 3x3's colour pixels averaged (a: 1 where water).
float4 PS_DecodeColour(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    int2 p = int2(pos.xy);
    float phase;
    if (!IsWater(p, phase))
        return 0.0;
    float3 sum = 0.0;
    float count = 0.0;
    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            int2 q = p + int2(x, y);
            float3 b = Bytes(q);
            bool even = IsEven(q, phase);
            if (even && Fits(b, even))
            {
                sum += b / 255.0;
                count += 1.0;
            }
        }
    }
    return count > 0.0 ? float4(sum / count, 1.0) : 0.0;
}

// Where the water is in the world: the 3x3's position pixels averaged (around the first, across the wrap).
float4 PS_DecodeWorld(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    int2 p = int2(pos.xy);
    float phase;
    if (!IsWater(p, phase))
        return 0.0;
    float2 reference = 0.0, offsets = 0.0;
    float count = 0.0;
    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            int2 q = p + int2(x, y);
            float3 b = Bytes(q);
            bool even = IsEven(q, phase);
            if (!even && Fits(b, even))
            {
                float2 w = Position(b);
                reference = count == 0.0 ? w : reference;
                offsets += Unwrap(w - reference);
                count += 1.0;
            }
        }
    }
    return count > 0.0 ? float4(reference + offsets / count, 0.0, 1.0) : 0.0;
}

// Inside the water, pixels something was drawn over take the water from around (8 directions, 2 pixels away: 6 of
// them must be water, so the water never grows over the shore or a boat).
static const float2 AROUND[8] = {
    float2(2, 0), float2(-2, 0), float2(0, 2), float2(0, -2),
    float2(2, 2), float2(-2, -2), float2(2, -2), float2(-2, 2) };

float4 PS_FillColour(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    int2 p = int2(pos.xy);
    float4 own = tex2Dfetch(EfWaterColourS, p);
    if (own.a > 0.5)
        return own;
    float4 sum = 0.0;
    [unroll]
    for (int i = 0; i < 8; i++)
    {
        float4 c = tex2Dfetch(EfWaterColourS, p + int2(AROUND[i]));
        sum += c.a > 0.5 ? float4(c.rgb, 1.0) : 0.0;
    }
    return sum.a >= 6.0 ? float4(sum.rgb / sum.a, 1.0) : own;
}

float4 PS_FillWorld(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    int2 p = int2(pos.xy);
    float4 own = tex2Dfetch(EfWaterWorldS, p);
    if (tex2Dfetch(EfWaterColourS, p).a > 0.5)
        return own;
    float2 reference = 0.0, offsets = 0.0;
    float count = 0.0;
    [unroll]
    for (int i = 0; i < 8; i++)
    {
        int2 q = p + int2(AROUND[i]);
        float2 w = tex2Dfetch(EfWaterWorldS, q).xy;
        if (tex2Dfetch(EfWaterColourS, q).a > 0.5)
        {
            reference = count == 0.0 ? w : reference;
            offsets += Unwrap(w - reference);
            count += 1.0;
        }
    }
    return count >= 6.0 ? float4(reference + offsets / count, 0.0, 1.0) : own;
}

// ---------------------------------------------------------------------------------------------------------------
// The view

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

float4 PS_Up(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float water = tex2Dlod(EfWaterColourFillS, float4(uv, 0, 0)).a;
    float depth = ReShade::GetLinearizedDepth(uv);
    if (water < 0.5 || depth <= 0.0001 || depth >= 0.9995)
        return 0.0;
    float2 px = BUFFER_PIXEL_SIZE * 3.0;
    float3 p = ViewPos(uv);
    float3 n = normalize(cross(ViewPos(uv + float2(0.0, px.y)) - p, ViewPos(uv + float2(px.x, 0.0)) - p));
    n = dot(n, -p) < 0.0 ? -n : n;
    if (!(n.y > 0.0))
        return 0.0;
    return float4(n, 1.0);
}

float4 PS_Sky(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    // The upper part of the screen, where it is sky (the far plane).
    float2 at = uv * float2(1.0, 0.6);
    float depth = ReShade::GetLinearizedDepth(at);
    if (depth < 0.9995)
        return 0.0;
    return float4(tex2Dlod(ReShade::BackBuffer, float4(at, 0, 0)).rgb, 1.0);
}

// ---------------------------------------------------------------------------------------------------------------
// Waves: a sum of travelling waves over the world position (yards), their wave numbers whole turns per 128 yards so
// the pattern wraps where the position does. Returns the slope (dh/dx, dh/dy).

float2 Waves(float2 w, float time, float footprint)
{
    static const float4 waves[8] = {
        // x: length share of WaveSize, y: angle from the wind (times Spread), z: height share, w: is a ripple
        float4(1.00, 0.0, 1.00, 0.0), float4(0.73, 0.9, 0.80, 0.0), float4(0.58, -0.7, 0.70, 0.0),
        float4(0.41, 2.0, 0.45, 0.0), float4(0.31, -1.7, 0.40, 0.0), float4(0.12, 0.4, 0.60, 1.0),
        float4(0.085, -2.3, 0.50, 1.0), float4(0.06, 1.4, 0.45, 1.0) };
    float wind = radians(WindDirection);
    float2 slope = 0.0;
    [unroll]
    for (int i = 0; i < 8; i++)
    {
        float wavelength = WaveSize * waves[i].x;
        float angle = wind + waves[i].y * WaveSpread * 1.6;
        float2 k = float2(cos(angle), sin(angle)) * (6.2831853 / wavelength);
        k = round(k * PERIOD / 6.2831853) * (6.2831853 / PERIOD);       // whole turns per period
        float kk = max(length(k), 1e-4);
        float omega = sqrt(9.8 * kk) * WaveSpeed;
        // Steepness from Wave height (ripples from Small ripples), faded where a wave is shorter than a few pixels.
        float steep = (waves[i].w > 0.5 ? Ripples * 0.35 : WaveHeight * 0.3) * waves[i].z;
        float visible = saturate(6.2831853 / kk / (footprint * 6.0) - 1.0);
        slope += k / kk * steep * cos(dot(k, w) - omega * time) * visible * (length(k) > 1e-4 ? 1.0 : 0.0);
    }
    return slope;
}

// ---------------------------------------------------------------------------------------------------------------
// Shade

float3 Grade(float3 c)
{
    // Hue: a turn around the grey axis.
    float a = radians(HueShift);
    float3 k = 0.57735;
    c = c * cos(a) + cross(k, c) * sin(a) + k * dot(k, c) * (1.0 - cos(a));
    float l = dot(c, float3(0.2126, 0.7152, 0.0722));
    c = lerp(l.xxx, c, Saturation);
    // The tint keeps the zone's brightness (dim at night).
    float tl = dot(TintColour, float3(0.2126, 0.7152, 0.0722));
    c = lerp(c, TintColour * (l / max(tl, 0.01)), TintAmount);
    return max(c * Brightness, 0.0);
}

float3 PS_Shade(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float3 original = tex2D(ReShade::BackBuffer, uv).rgb;
    int2 p = int2(pos.xy);
    float4 base = tex2Dfetch(EfWaterColourFillS, p);
    if (base.a < 0.5)
        return original;

    float2 w = tex2Dfetch(EfWaterWorldFillS, p).xy;
    // Yards per pixel across and down (for fading waves too fine to show and for bending the reflection).
    float4 right = tex2Dfetch(EfWaterColourFillS, p + int2(1, 0));
    float4 down = tex2Dfetch(EfWaterColourFillS, p + int2(0, 1));
    float2 wx = right.a > 0.5 ? Unwrap(tex2Dfetch(EfWaterWorldFillS, p + int2(1, 0)).xy - w) : 0.0;
    float2 wy = down.a > 0.5 ? Unwrap(tex2Dfetch(EfWaterWorldFillS, p + int2(0, 1)).xy - w) : 0.0;
    float footprint = max(max(length(wx), length(wy)), 0.002);

    float2 slope = Waves(w, Timer * 0.001, footprint);
    float light = radians(LightDirection);
    float2 toLight = float2(cos(light), sin(light));

    float3 colour = Grade(base.rgb);
    colour *= 1.0 + WaveShading * clamp(dot(slope, toLight) * 3.0, -0.6, 0.6);

    // The view and the water's up, for the Fresnel and the reflection.
    float4 upSum = tex2Dlod(EfWaterUpS, float4(0.5, 0.5, 0, 8));
    float3 up = upSum.w > 1e-5 ? normalize(upSum.xyz / upSum.w) : float3(0.0, 1.0, 0.0);
    float3 at = ViewPos(uv);
    float3 v = normalize(at);
    float fresnel = pow(1.0 - saturate(dot(-v, up)), 5.0);
    float share = lerp(ReflectionDown, ReflectionGlance, fresnel);

    // The sky's colour (or, with no sky on screen, the water's own a little brighter).
    float4 skySum = tex2Dlod(EfWaterSkyS, float4(0.5, 0.5, 0, 7));
    float3 sky = skySum.w > 1e-4 ? skySum.rgb / skySum.w : base.rgb * 1.4;
    float3 mirrored = sky * SkyReflection;

    // The ray, mirrored about the flat water.
    float3 r = reflect(v, up);
    r = normalize(r + up * max(0.0, 0.03 - dot(r, up)));
    float dist = 0.4;
    [loop]
    for (int i = 0; i < MARCH_STEPS; i++)
    {
        float3 q = at + r * dist;
        if (q.z <= 0.1)
            break;
        float2 quv = Project(q);
        if (any(quv < 0.0) || any(quv > 1.0))
            break;
        float scene = ReShade::GetLinearizedDepth(quv) * YARDS;
        float behind = q.z - scene;
        if (behind > 0.0 && behind < max(1.5, dist * 0.25) && scene < YARDS * 0.999)
        {
            float lo = dist / 1.22 - 0.25, hi = dist;
            [unroll]
            for (int j = 0; j < 5; j++)
            {
                float mid = (lo + hi) * 0.5;
                float3 m = at + r * mid;
                if (m.z > ReShade::GetLinearizedDepth(Project(m)) * YARDS)
                    hi = mid;
                else
                    lo = mid;
            }
            float2 hit = Project(at + r * hi);
            // Bent by the waves: the slope as a shift in the world, turned into pixels (inverse of the yards per
            // pixel), at most 40 pixels.
            float det = wx.x * wy.y - wx.y * wy.x;
            float2 shift = 0.0;
            if (abs(det) > 1e-8)
            {
                float2 world = slope * Distortion * 0.6;
                shift = float2(wy.y * world.x - wy.x * world.y, -wx.y * world.x + wx.x * world.y) / det;
                shift *= min(1.0, 40.0 / max(length(shift), 1e-4));
            }
            hit += shift * BUFFER_PIXEL_SIZE;
            float2 edge = saturate(min(hit, 1.0 - hit) * 10.0);
            float onWater = tex2Dlod(EfWaterColourFillS, float4(hit, 0, 0)).a;
            float found = edge.x * edge.y * (1.0 - onWater);
            mirrored = lerp(mirrored, tex2Dlod(ReShade::BackBuffer, float4(hit, 0, 0)).rgb, found);
            break;
        }
        dist = dist * 1.22 + 0.25;
    }

    float3 result = lerp(colour, mirrored * ReflectionBrightness, saturate(share));

    // Sparkle: crests turned toward the light, as bright as the sky is.
    float3 n = normalize(float3(-slope, 1.0));
    float3 h = normalize(float3(toLight * 0.6, 1.0));
    float shininess = lerp(400.0, 30.0, SparkleSize);
    float skyLight = dot(sky, float3(0.2126, 0.7152, 0.0722));
    result += pow(saturate(dot(n, h)), shininess) * Sparkle * skyLight * sky * 2.0;

    if (ShowWater)
        result = lerp(result, float3(1.0, 0.1, 0.1), 0.5);
    return result;
}

technique Emberforge_Water <
    ui_label = "Emberforge Water";
    ui_tooltip = "The water drawn by ReShade: colour, waves, reflections of the city and sparkle, all adjustable.\n"
                 "Needs Lighting on in the launcher (it installs the water this reads) and the game's Full Screen\n"
                 "Glow off.";
>
{
    pass Classify { VertexShader = PostProcessVS; PixelShader = PS_Classify; RenderTarget = EfWaterClassTex; }
    pass DecodeColour { VertexShader = PostProcessVS; PixelShader = PS_DecodeColour; RenderTarget = EfWaterColourTex; }
    pass DecodeWorld { VertexShader = PostProcessVS; PixelShader = PS_DecodeWorld; RenderTarget = EfWaterWorldTex; }
    pass FillColour
    {
        VertexShader = PostProcessVS; PixelShader = PS_FillColour; RenderTarget = EfWaterColourFillTex;
    }
    pass FillWorld { VertexShader = PostProcessVS; PixelShader = PS_FillWorld; RenderTarget = EfWaterWorldFillTex; }
    pass Up { VertexShader = PostProcessVS; PixelShader = PS_Up; RenderTarget = EfWaterUpTex; }
    pass Sky { VertexShader = PostProcessVS; PixelShader = PS_Sky; RenderTarget = EfWaterSkyTex; }
    pass Shade { VertexShader = PostProcessVS; PixelShader = PS_Shade; }
}
