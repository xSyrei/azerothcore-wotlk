// Emberforge Engine atmosphere: light shafts from the sun and fog lit by it, drawn over the world once it is done (just
// before the game draws its interface). Three passes:
//  Sky:       at half the screen, the sky's light around the sun (the far plane, brighter than a threshold, fading
//             away from the sun: only there do rays come from) plus a soft sun disc.
//  Shafts:    at half the screen, that light smeared toward the sun: it streams past roofs, trees and mountains.
//  Composite: over the whole screen, fog by the true distance and height (thicker low, drifting in wisps), lit by the
//             sun toward it and by the shafts through it, and the shafts themselves.
// The sun's direction and colour, and the camera, come from the game's own vertex shader constants (engine.cpp).

sampler2D SceneS : register(s0);      // the world as drawn
sampler2D DistanceS : register(s1);   // the eye's distance (view z) in yards, 100000 for the sky
sampler2D SkyS : register(s2);        // Sky's output (Shafts reads it)
sampler2D ShaftS : register(s3);      // Shafts' output (Composite reads it)

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
float4 Mist : register(c11);         // drifting mist amount, its size (yards), its speed

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

    // The fog's light: its own colour, brighter toward the sun (forward scattering) and where the shafts cross it.
    float toward = saturate(dot(dir, ToSun.xyz));
    // Close around the sun (a wide glow washed the whole view in its colour).
    float glow = (pow(toward, 48.0) * 0.35 + pow(toward, 400.0)) * FogShape.w * saturate(ToSun.z * 5.0 + 0.5);
    float3 shafts = tex2Dlod(ShaftS, float4(uv, 0, 0)).rgb;
    float3 fogLight = Fog.rgb + SunColour.rgb * glow + shafts * Shafts.z;
    if (Projection.w > 0.5)
        return float4(shafts * SunColour.a + fogLight * fog, 1.0);
    colour = lerp(colour, fogLight, fog);
    colour += shafts * SunColour.a * (sky ? 0.15 : 1.0);
    return float4(colour, 1.0);
}
