// Emberforge Engine water: replaces the game's water pixel shader (psLiquidProcWater) while the game draws its water.
// Compiled when the game starts (d3dcompiler_47.dll, ps_3_0); build.sh embeds this file in the engine.
//
// From the game, as its own shader has them: the water's world position (TEXCOORD3), the depth colour's coordinates
// (TEXCOORD2.xy), the fog (FOG), the sky cube (s1), the depth colour and opacity ramp (s2), the wave normal map (s4),
// the fog colour (c4), the sun's direction (c6), the highlight colour (c9).
// From the engine: the scene behind the water (s13) and its distance from the eye in yards (s12), both copied just
// before the first water draw; the camera (c12-c16); the screen and time (c17); the panel's sliders (c18-c24).
//
// The water is drawn opaque over the copied scene: the bottom shows through shallow water and fades into the water's
// own colour as it deepens, the sky and the city (marched through the depth) are mirrored by the Fresnel, waves of the
// panel's size, height and speed move over the world position, and the sun glints on them.

samplerCUBE SkyS : register(s1);
sampler2D DepthColourS : register(s2);
sampler2D WaveS : register(s4);
sampler2D DistanceS : register(s12);
sampler2D SceneS : register(s13);

float4 FogColour : register(c4);
float4 SunDirection : register(c6);
float4 Highlight : register(c9);

float4 ViewX : register(c12);        // the view matrix's rows (a world point times it: x, y, z, 1)
float4 ViewY : register(c13);
float4 ViewZ : register(c14);
float4 ViewT : register(c15);
float4 Projection : register(c16);   // x scale, y scale, near plane, the camera is known (1) or not (0)
float4 Screen : register(c17);       // 1 / width, 1 / height, seconds, the scene and depth are copied (1) or not (0)
float4 Tint : register(c18);         // tint colour, tint amount
float4 Grade : register(c19);        // hue (radians), saturation, brightness, murkiness (per yard)
float4 WaveA : register(c20);        // size (yards), height, speed, wind (radians)
float4 WaveB : register(c21);        // spread, ripples, detail strength, detail size (yards)
float4 Mirror : register(c22);       // reflectiveness looking down, at a glance, reflection brightness, distortion
float4 Sky : register(c23);          // sky brightness, city reflections (0/1), sparkle, sparkle size
float4 Other : register(c24);        // wave shading, refraction, opacity without depth, show the water (0/1)

struct Input
{
    float4 depthUv : TEXCOORD2;
    float3 world : TEXCOORD3;
    float fog : FOG;
    float2 pixel : VPOS;
};

static const float3 kLuma = float3(0.2126, 0.7152, 0.0722);

// A sum of travelling waves over the world position (yards). Returns the slope (dh/dx, dh/dy).
float2 Waves(float2 w, float time, float footprint)
{
    static const float4 waves[8] = {
        // x: length share of the wave size, y: angle from the wind (times spread), z: height share, w: a ripple
        float4(1.00, 0.0, 1.00, 0.0), float4(0.73, 0.9, 0.80, 0.0), float4(0.58, -0.7, 0.70, 0.0),
        float4(0.41, 2.0, 0.45, 0.0), float4(0.31, -1.7, 0.40, 0.0), float4(0.12, 0.4, 0.60, 1.0),
        float4(0.085, -2.3, 0.50, 1.0), float4(0.06, 1.4, 0.45, 1.0) };
    float2 slope = 0.0;
    [unroll]
    for (int i = 0; i < 8; i++)
    {
        float wavelength = max(WaveA.x * waves[i].x, 0.05);
        float angle = WaveA.w + waves[i].y * WaveB.x * 1.6;
        float k = 6.2831853 / wavelength;
        float2 dir = float2(cos(angle), sin(angle));
        float omega = sqrt(9.8 * k) * WaveA.z;
        float steep = (waves[i].w > 0.5 ? WaveB.y * 0.35 : WaveA.y * 0.3) * waves[i].z;
        // Waves shorter than a few pixels fade instead of shimmering.
        float visible = saturate(wavelength / (footprint * 6.0) - 1.0);
        slope += dir * steep * cos(dot(dir, w) * k - omega * time) * visible;
    }
    return slope;
}

float3 GradeColour(float3 c)
{
    // Hue: a turn around the grey axis.
    float a = Grade.x;
    float3 k = 0.57735;
    c = c * cos(a) + cross(k, c) * sin(a) + k * dot(k, c) * (1.0 - cos(a));
    float l = dot(c, kLuma);
    c = lerp(l.xxx, c, Grade.y);
    // The tint keeps the zone's brightness (dim at night).
    float tl = dot(Tint.rgb, kLuma);
    c = lerp(c, Tint.rgb * (l / max(tl, 0.01)), Tint.a);
    return max(c * Grade.z, 0.0);
}

float3 ToView(float3 w)
{
    return w.x * ViewX.xyz + w.y * ViewY.xyz + w.z * ViewZ.xyz + ViewT.xyz;
}

float3 DirectionToView(float3 d)
{
    return d.x * ViewX.xyz + d.y * ViewY.xyz + d.z * ViewZ.xyz;
}

float2 ToScreen(float3 v)
{
    return float2(0.5 + 0.5 * Projection.x * v.x / v.z, 0.5 - 0.5 * Projection.y * v.y / v.z);
}

float Distance(float2 uv)
{
    return tex2Dlod(DistanceS, float4(uv, 0, 0)).r;
}

// The city in the water: the reflected ray marched through the copied depth. found: 0 (missed) to 1. (No break out of
// the loop: a flag ends it, which every driver runs the same.)
float3 March(float3 p, float3 r, out float found)
{
    found = 0.0;
    float3 colour = 0.0;
    float dist = 0.4, previous = 0.0;
    float done = 0.0;
    [loop]
    for (int s = 0; s < 28; s++)
    {
        if (done < 0.5)
        {
            float3 q = p + r * dist;
            float2 quv = ToScreen(q);
            if (q.z <= Projection.z || any(quv < 0.0) || any(quv > 1.0))
            {
                done = 1.0;
            }
            else
            {
                float behind = q.z - Distance(quv);
                if (behind > 0.0 && behind < max(1.5, dist * 0.3))
                {
                    float lo = previous, hi = dist;
                    [unroll]
                    for (int j = 0; j < 5; j++)
                    {
                        float mid = (lo + hi) * 0.5;
                        float3 m = p + r * mid;
                        if (m.z > Distance(ToScreen(m)))
                            hi = mid;
                        else
                            lo = mid;
                    }
                    float2 hit = ToScreen(p + r * hi);
                    float2 edge = saturate(min(hit, 1.0 - hit) * 12.0);
                    found = edge.x * edge.y;
                    colour = tex2Dlod(SceneS, float4(hit, 0, 0)).rgb;
                    done = 1.0;
                }
                previous = dist;
                dist = dist * 1.22 + 0.3;
            }
        }
    }
    return colour;
}

float4 main(Input i) : COLOR
{
    float3 w = i.world;
    float2 uv = (i.pixel + 0.5) * Screen.xy;
    float footprint = max(max(length(ddx(w.xy)), length(ddy(w.xy))), 0.002);

    // Waves: the panel's, plus the game's own wave map for detail.
    float2 slope = Waves(w.xy, Screen.z, footprint);
    float size = max(WaveB.w, 0.1);
    float4 a = tex2D(WaveS, w.xy / size);
    float4 b = tex2D(WaveS, w.yx / (size * 0.7));
    slope += ((a.xy * 2.0 - 1.0) + (b.wz * 2.0 - 1.0)) * 0.5 * WaveB.z;

    bool camera = Projection.w > 0.5;
    float3 eye = -float3(dot(ViewX.xyz, ViewT.xyz), dot(ViewY.xyz, ViewT.xyz), dot(ViewZ.xyz, ViewT.xyz));
    float3 n = normalize(float3(-slope, 1.0));
    float3 v = camera ? normalize(w - eye) : float3(0.0, 0.0, -1.0);
    bool under = camera && eye.z < w.z;
    if (under)
        n = -n;

    // The sun, from above (the game's direction may point either way).
    float3 sun = normalize(SunDirection.xyz);
    sun = sun.z < 0.0 ? -sun : sun;

    // The water's own colour (the game's for this zone and hour), graded, lit by the waves.
    float4 game = tex2D(DepthColourS, i.depthUv.xy);
    float3 body = GradeColour(game.rgb);
    float2 toSun = normalize(sun.xy + 1e-4);
    body *= 1.0 + Other.x * clamp(dot(slope, toSun) * 3.0, -0.6, 0.6);

    if (under)
    {
        float3 c = lerp(FogColour.rgb, body, i.fog);
        return float4(c, saturate(game.a * Other.z + 0.3));
    }

    // Reflection: the sky (in the hour's colour), and the city where the ray finds it.
    float fresnel = pow(1.0 - saturate(dot(-v, n)), 5.0);
    float share = saturate(lerp(Mirror.x, Mirror.y, fresnel));
    float3 r = reflect(v, n);
    float3 sky = texCUBE(SkyS, float3(r.x, max(r.z, 0.02), r.y)).rgb * FogColour.rgb * Sky.x;
    float3 mirrored = sky;
    bool copied = Screen.w > 0.5 && camera;
    float3 p = ToView(w);
    if (copied && Sky.y > 0.5)
    {
        float3 bent = normalize(float3(-slope * Mirror.w, 1.0));
        // Never below the water (a steep wave would mirror the bottom under it).
        float3 rw = reflect(v, bent);
        rw = normalize(float3(rw.xy, max(rw.z, 0.03)));
        float3 rv = normalize(DirectionToView(rw));
        float found;
        float3 city = March(p, rv, found);
        mirrored = lerp(mirrored, city, found);
    }
    mirrored *= Mirror.z;

    // The sun's glint on the waves, in the game's highlight colour.
    float3 h = normalize(sun - v);
    float shininess = lerp(600.0, 40.0, saturate(Sky.w));
    float3 glint = pow(saturate(dot(n, h)), shininess) * Highlight.rgb * Sky.z * 4.0;

    float4 result;
    if (copied)
    {
        // The bottom through the water, bent by the waves where it is still under the water.
        float2 offset = slope * Other.y * 0.03 * saturate(12.0 / max(p.z, 1.0));
        float2 ruv = uv + offset;
        float bottom = Distance(ruv);
        if (bottom < p.z)
        {
            ruv = uv;
            bottom = Distance(uv);
        }
        float depth = max(bottom - p.z, 0.0);
        float3 below = tex2Dlod(SceneS, float4(ruv, 0, 0)).rgb;
        float murk = 1.0 - exp(-depth * Grade.w);
        share *= saturate(depth * 2.0);                    // the shore's edge is not a hard line
        float cover = 1.0 - (1.0 - murk) * (1.0 - share);
        float3 surface = (body * murk * (1.0 - share) + mirrored * share) / max(cover, 1e-4) + glint;
        surface = lerp(FogColour.rgb, surface, i.fog);
        result = float4(lerp(below, surface, cover), 1.0);
    }
    else
    {
        float3 surface = lerp(body, mirrored, share) + glint;
        result = float4(lerp(FogColour.rgb, surface, i.fog), saturate(lerp(game.a * Other.z, 1.0, share)));
    }
    if (Other.w > 0.5)
        result.rgb = lerp(result.rgb, float3(1.0, 0.1, 0.1), 0.5);
    return result;
}
