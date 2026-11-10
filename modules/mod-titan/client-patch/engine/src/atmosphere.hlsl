// Emberforge Engine atmosphere: the sun (or moon) lighting the whole world and the air, drawn over the world once it
// is done (just before the game draws its interface). Where the body stands in the sky only sets the light's
// direction: every surface facing it is lit, every one turned away or in a shadow is not, and the air glows where the
// light reaches it. Four passes:
//  Sky:       at half the screen, the sky's light around the sun (the far plane, brighter than a threshold, fading
//             away from the sun: only there do rays come from) plus a soft sun disc.
//  Shafts:    at half the screen, that light smeared toward the sun: it streams past roofs, trees and mountains.
//  Scatter:   at half the screen, along each view ray, how much of the air sees the sun (16 points, each checked for
//             something in the way toward the sun through the depth), and whether the surface itself does.
//  Composite: over the whole screen, the surfaces lit by the sun by how they face it (normals from the depth) and
//             whether they see it, fog by the true distance and height (thicker low, drifting in wisps), lit by the
//             sun toward it and by the shafts through it, and the shafts themselves.
// The sun's direction and colour, and the camera, come from the game's own vertex shader constants (engine.cpp).

sampler2D SceneS : register(s0);      // the world as drawn
sampler2D DistanceS : register(s1);   // the eye's distance (view z) in yards, 100000 for the sky
sampler2D SkyS : register(s2);        // Sky's output (Shafts reads it)
sampler2D ShaftS : register(s3);      // Shafts' output (Composite reads it)
sampler2D ScatterS : register(s4);    // Scatter's output: the lit share of the air, the surface's sight of the sun

float4 Sun : register(c0);           // the sun on screen (uv), how much of it shows (0-1)
float4 SunColour : register(c1);     // its colour, the shafts' intensity
float4 Fog : register(c2);           // fog colour, density (per 100 yards)
float4 FogShape : register(c3);      // start (yards), height falloff (per yard), most cover, sun glow in the fog
float4 Shafts : register(c4);        // decay per sample, sky threshold, shafts in the fog, sun disc size
float4 Projection : register(c5);    // x scale, y scale, near plane, show the light alone (0/1)
float4 ViewX : register(c6);         // the view's rows (world x, y, z), to turn view directions into the world
float4 ViewY : register(c7);
float4 ViewZ : register(c8);
float4 Eye : register(c9);           // the eye in the world, how far below it the fog is thickest (yards)
float4 ToSun : register(c10);        // toward the sun in the world, seconds
float4 Mist : register(c11);         // drifting mist amount, its size (yards), its speed, 1 / the screen's height
float4 SunView : register(c12);      // toward the sun in view space, the light's strength (by day 1, by night less)
float4 Light : register(c13);        // sunlight on surfaces, sunlight in the air, how far the air is lit (yards),
                                     // 1 / the screen's width

static const float3 kLuma = float3(0.2126, 0.7152, 0.0722);
static const float kSky = 50000.0;

float Distance(float2 uv)
{
    return tex2Dlod(DistanceS, float4(uv, 0, 0)).r;
}

// The view ray through a pixel (view space, z = 1).
float3 ViewRay(float2 uv)
{
    return float3((uv.x * 2.0 - 1.0) / Projection.x, (1.0 - uv.y * 2.0) / Projection.y, 1.0);
}

float3 ToWorld(float3 v)
{
    return float3(dot(ViewX.xyz, v), dot(ViewY.xyz, v), dot(ViewZ.xyz, v));
}

float4 Sky(float2 uv : TEXCOORD0) : COLOR
{
    if (Distance(uv) < kSky)
        return 0.0;
    float3 colour = tex2Dlod(SceneS, float4(uv, 0, 0)).rgb;
    float bright = smoothstep(Shafts.y, 1.0, dot(colour, kLuma));
    // The sun's disc, where the game's own may be faint, and the sky glowing around it.
    float3 dir = normalize(ToWorld(ViewRay(uv)));
    float toward = saturate(dot(dir, ToSun.xyz));
    float disc = pow(toward, lerp(4000.0, 200.0, Shafts.w));
    return float4(colour * bright * pow(toward, 12.0) + SunColour.rgb * disc * 2.0, 1.0);
}

float4 ShaftsPass(float2 uv : TEXCOORD0) : COLOR
{
    if (Sun.z < 0.01)
        return 0.0;
    const int samples = 48;
    float2 step = (Sun.xy - uv) / samples;
    float2 at = uv;
    float decay = 1.0, weight = 0.0;
    float3 sum = 0.0;
    [unroll]
    for (int i = 0; i < samples; i++)
    {
        at += step;
        sum += tex2Dlod(SkyS, float4(at, 0, 0)).rgb * decay;
        weight += decay;
        decay *= Shafts.x;
    }
    // The light along the way, averaged (nearer samples count more: the shaft fades away from the sun).
    return float4(sum / weight * 2.0 * Sun.z, 1.0);
}

float2 ToScreen(float3 v)
{
    return float2(0.5 + 0.5 * Projection.x * v.x / v.z, 0.5 - 0.5 * Projection.y * v.y / v.z);
}

// Whether a point (view space) sees the sun: a few steps toward it, growing, each checked against the depth there.
float SeesSun(float3 p, float start)
{
    float seen = 1.0;
    float dist = start;
    [unroll]
    for (int j = 0; j < 6; j++)
    {
        float3 q = p + SunView.xyz * dist;
        float2 quv = ToScreen(q);
        if (q.z > Projection.z && all(quv > 0.0) && all(quv < 1.0))
        {
            float surface = Distance(quv);
            // Behind something (not far behind: that is a gap behind a thin object, not a wall).
            float behind = q.z - surface;
            if (behind > 0.15 && behind < 4.0 + dist)
                seen = 0.0;
        }
        dist *= 2.1;
    }
    return seen;
}

float4 Scatter(float2 uv : TEXCOORD0, float2 pixel : VPOS) : COLOR
{
    float z = Distance(uv);
    float3 ray = ViewRay(uv);
    float far = min(z, Light.z);
    // Interleaved noise for each pixel's starting point: 16 steps look like 64 once blurred by the half size.
    float jitter = frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
    float lit = 0.0;
    [loop]
    for (int i = 0; i < 16; i++)
    {
        float t = far * (i + jitter) / 16.0;
        lit += SeesSun(ray * t, 0.6);
    }
    float surface = z < kSky ? SeesSun(ray * z, 0.3) : 1.0;
    return float4(lit / 16.0, surface, 0.0, 1.0);
}

// The surface's normal in view space, from the depth (the smaller step each way, so an edge does not count).
float3 Normal(float2 uv, float z)
{
    float2 px = float2(Light.w, Mist.w) * 1.5;
    float3 p = ViewRay(uv) * z;
    float3 r = ViewRay(uv + float2(px.x, 0)) * Distance(uv + float2(px.x, 0)) - p;
    float3 l = p - ViewRay(uv - float2(px.x, 0)) * Distance(uv - float2(px.x, 0));
    float3 d = ViewRay(uv + float2(0, px.y)) * Distance(uv + float2(0, px.y)) - p;
    float3 u = p - ViewRay(uv - float2(0, px.y)) * Distance(uv - float2(0, px.y));
    float3 dx = abs(r.z) < abs(l.z) ? r : l;
    float3 dy = abs(d.z) < abs(u.z) ? d : u;
    float3 n = normalize(cross(dy, dx));
    return dot(n, -p) < 0.0 ? -n : n;
}

// Smooth 3D value noise, three octaves: the mist's wisps.
float Hash(float3 p)
{
    p = frac(p * 0.3183099 + 0.1);
    p *= 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float Noise(float3 x)
{
    float3 i = floor(x);
    float3 f = frac(x);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(lerp(Hash(i), Hash(i + float3(1, 0, 0)), f.x),
                     lerp(Hash(i + float3(0, 1, 0)), Hash(i + float3(1, 1, 0)), f.x), f.y),
                lerp(lerp(Hash(i + float3(0, 0, 1)), Hash(i + float3(1, 0, 1)), f.x),
                     lerp(Hash(i + float3(0, 1, 1)), Hash(i + float3(1, 1, 1)), f.x), f.y), f.z);
}

float Wisps(float3 p)
{
    return Noise(p) * 0.55 + Noise(p * 2.03 + 7.1) * 0.3 + Noise(p * 4.1 + 13.7) * 0.15;
}

float4 Composite(float2 uv : TEXCOORD0) : COLOR
{
    float3 colour = tex2Dlod(SceneS, float4(uv, 0, 0)).rgb;
    float z = Distance(uv);
    bool sky = z >= kSky;
    float3 ray = ViewRay(uv);
    float3 dir = normalize(ToWorld(ray));
    // How far the ray goes through the air: to the surface, or (the sky) far enough to haze the horizon.
    float r = sky ? 1500.0 : length(ray) * z;
    float through = max(r - FogShape.x, 0.0);

    // Height fog: density D * exp(-falloff * (height - (eye - offset))) integrated along the ray.
    float falloff = max(FogShape.y, 1e-4);
    float base = Fog.w * 0.01 * exp(-falloff * Eye.w);
    float dz = dir.z * falloff;
    float depth = abs(dz) > 1e-4 ? base * (1.0 - exp(-dz * through)) / dz : base * through;
    // Drifting mist: the fog thins and thickens between 0.4 and 1.6 times, by where the ray ends (not right at the
    // eye: there it reads as smudges on the screen).
    float3 p = Eye.xyz + dir * min(r, 300.0);
    float3 drift = float3(0.6, 0.2, 0.05) * ToSun.w * Mist.z;
    float wisp = 1.0 + (Wisps(p / max(Mist.y, 1.0) + drift) - 0.5) * 2.4 * Mist.x * smoothstep(8.0, 40.0, r);
    float fog = saturate((1.0 - exp(-depth * wisp)) * FogShape.z);

    float4 scatter = tex2Dlod(ScatterS, float4(uv, 0, 0));

    // The surfaces lit by the sun: brighter and in its colour where they face it and see it, darker turned away or
    // in a shadow. The sky is the light's source, not lit.
    float3 sunTint = SunColour.rgb / max(dot(SunColour.rgb, kLuma), 0.05);
    if (!sky)
    {
        float3 n = Normal(uv, z);
        float facing = saturate(dot(n, SunView.xyz));
        float direct = facing * scatter.g;
        float strength = Light.x * SunView.w;
        colour *= 1.0 + strength * (direct * 1.1 - 0.35);
        colour *= lerp(1.0, sunTint, saturate(strength * direct * 0.5));
    }

    // The fog's light: its own colour, and the sun's where it reaches the air (more toward the sun: light scatters
    // forward), brighter close around the sun and where the screen's shafts cross it.
    float toward = saturate(dot(dir, ToSun.xyz));
    float cosine = dot(dir, ToSun.xyz);
    float g = 0.55;
    float phase = (1.0 - g * g) / pow(1.0 + g * g - 2.0 * g * cosine, 1.5) * (1.0 - g) * (1.0 - g) / (1.0 + g);
    // Close around the sun (a wide glow washed the whole view in its colour).
    float glow = (pow(toward, 48.0) * 0.35 + pow(toward, 400.0)) * FogShape.w * saturate(ToSun.z * 5.0 + 0.5);
    float3 shafts = tex2Dlod(ShaftS, float4(uv, 0, 0)).rgb;
    float air = Light.y * SunView.w * scatter.r * (0.2 + phase);
    float3 fogLight = Fog.rgb + SunColour.rgb * (glow + air) + shafts * Shafts.z;
    // Lit air shows even where the fog is thin (a sunbeam through clear air).
    colour += SunColour.rgb * air * 0.08 * (1.0 - fog);
    if (Projection.w > 0.5)
        return float4(shafts * SunColour.a + fogLight * fog, 1.0);
    colour = lerp(colour, fogLight, fog);
    colour += shafts * SunColour.a * (sky ? 0.15 : 1.0);
    return float4(colour, 1.0);
}
