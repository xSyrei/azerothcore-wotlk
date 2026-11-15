// Emberforge Engine atmosphere: the whole world's light, haze and colour, drawn over the world once it is done (just
// before the game draws its interface). Nothing here follows the sun or the moon: every pixel is treated alike, so
// the look is the same whichever way the camera turns. Its inputs are the scene as drawn, its distances, and the
// game's own haze colour (set by zone and hour), which also says how far into the night it is (engine.cpp).
//  Bright: at half the screen, the light in the scene: what shines brighter than a threshold (the sky counts less:
//          it is not a lamp). By night the threshold drops, so lamps, torches and windows stand out.
//  Down:   each smaller level (a quarter, an eighth... down to a 32nd), a 13-tap soft average of the one above.
//  Up:     back up, each level plus the smaller one spread over it: light that reaches further the smaller it went.
//  Final:  over the whole screen: night (the dark darker and cooler, what is lit kept), the lights' spill on the
//          ground and walls around them, haze by distance and height in the haze colour with the lights glowing
//          through it, bloom, and a colour grade (day and night tints, saturation, contrast, vignette).

sampler2D SceneS : register(s0);      // the world as drawn (Bright, Final)
sampler2D DistanceS : register(s1);   // the eye's distance (view z) in yards, 100000 for the sky
sampler2D LevelS : register(s2);      // Down: the level above; Up: this level's own; Final: the bloom
sampler2D WideS : register(s3);       // Up: the smaller level, spread; Final: the wide light (the spill, the halos)

// Bright, Down, Up.
float4 Texel : register(c0);          // the source's texel (x, y), threshold, knee
float4 BrightShape : register(c1);    // how much the sky counts, how much light colour is kept (saturation)

// Final.
float4 Night : register(c0);          // how much it is night (0-1), the dark's brightness by night, keep from, keep to
float4 NightLook : register(c1);      // the dark's loss of colour by night, the night's blue (rgb)
float4 Haze : register(c2);           // haze colour (rgb), density (per 100 yards)
float4 HazeShape : register(c3);      // start (yards), height falloff (per yard), most cover, thickest below the eye
float4 Projection : register(c4);     // x scale, y scale, near plane, the camera known (0/1)
float4 ViewX : register(c5);          // the view's rows (world x, y, z), to turn view directions into the world
float4 ViewY : register(c6);
float4 ViewZ : register(c7);
float4 Eye : register(c8);            // the eye in the world, show (0 all, 1 bloom, 2 haze, 3 night, 4 spill,
                                      // 5 point lights, 6 ambient occlusion)
float4 Light : register(c9);          // bloom, spill, halos in the haze, the haze's own light by night
float4 Tint : register(c10);          // the colour grade's tint (rgb, day and night mixed), its amount
float4 Grade : register(c11);         // saturation, contrast, vignette, exposure
float4 Mist : register(c12);          // drifting mist amount, its size (yards), its speed, seconds
float4 Screen : register(c13);        // 1 / width, 1 / height, width / height, highlights' soft shoulder
// Point lights (street lamps, by now only the panel's test light): how many, their strength, their glow in the air.
// Each: where it is in view space (x, y, z) and how far it reaches (yards); its colour.
static const int kLights = 8;
float4 Lights : register(c14);        // count, strength on surfaces, glow in the air, (by day) how much they show
float4 LightAt[kLights * 2] : register(c15);
float4 Shade : register(c31);         // ambient occlusion: strength, radius (yards), how much by night, compare (0/1)
float4 Split : register(c32);         // by day: warmth of the highlights, coolness of the shadows, daylight's own
                                      // brightening of the air, (spare)

static const float3 kLuma = float3(0.2126, 0.7152, 0.0722);
static const float kSky = 50000.0;

float Distance(float2 uv)
{
    return tex2Dlod(DistanceS, float4(uv, 0, 0)).r;
}

float3 Sample(sampler2D s, float2 uv)
{
    return tex2Dlod(s, float4(uv, 0, 0)).rgb;
}

// A 13-tap soft average around uv (the source's texels): wide, and without the flicker of a plain 2x2.
float3 Soft13(sampler2D s, float2 uv, float2 t)
{
    float3 a = Sample(s, uv + t * float2(-2, -2)), b = Sample(s, uv + t * float2(0, -2));
    float3 c = Sample(s, uv + t * float2(2, -2)), d = Sample(s, uv + t * float2(-1, -1));
    float3 e = Sample(s, uv + t * float2(1, -1)), f = Sample(s, uv + t * float2(-2, 0));
    float3 g = Sample(s, uv), h = Sample(s, uv + t * float2(2, 0));
    float3 i = Sample(s, uv + t * float2(-1, 1)), j = Sample(s, uv + t * float2(1, 1));
    float3 k = Sample(s, uv + t * float2(-2, 2)), l = Sample(s, uv + t * float2(0, 2));
    float3 m = Sample(s, uv + t * float2(2, 2));
    return (d + e + i + j) * 0.125 + (a + c + k + m) * 0.03125 + (b + f + h + l) * 0.0625 + g * 0.125;
}

float4 Bright(float2 uv : TEXCOORD0) : COLOR
{
    float3 colour = Soft13(SceneS, uv, Texel.xy);
    float sky = Distance(uv) >= kSky ? BrightShape.x : 1.0;
    // A soft threshold: nothing below threshold - knee, all of it above threshold + knee.
    float l = dot(colour, kLuma);
    float soft = clamp(l - Texel.z + Texel.w, 0.0, 2.0 * Texel.w);
    soft = soft * soft / (4.0 * Texel.w + 1e-4);
    float kept = max(soft, l - Texel.z) / max(l, 1e-4);
    float3 light = colour * kept * sky;
    // The light's colour a little richer (a lamp's glow is warmer than the white of its flame).
    light = lerp(dot(light, kLuma).xxx, light, BrightShape.y);
    return float4(light, 1.0);
}

float4 Down(float2 uv : TEXCOORD0) : COLOR
{
    return float4(Soft13(LevelS, uv, Texel.xy), 1.0);
}

float4 Up(float2 uv : TEXCOORD0) : COLOR
{
    // The smaller level spread with a 3x3 tent over this one.
    float2 t = Texel.xy;
    float3 tent = Sample(WideS, uv) * 4.0;
    tent += (Sample(WideS, uv + float2(-t.x, 0)) + Sample(WideS, uv + float2(t.x, 0)) +
             Sample(WideS, uv + float2(0, -t.y)) + Sample(WideS, uv + float2(0, t.y))) * 2.0;
    tent += Sample(WideS, uv - t) + Sample(WideS, uv + t) + Sample(WideS, uv + float2(t.x, -t.y)) +
            Sample(WideS, uv + float2(-t.x, t.y));
    return float4(Sample(LevelS, uv) + tent / 16.0 * Texel.z, 1.0);
}

float3 ViewRay(float2 uv)
{
    return float3((uv.x * 2.0 - 1.0) / Projection.x, (1.0 - uv.y * 2.0) / Projection.y, 1.0);
}

// The surface's normal in view space, from the depth (the smaller step each way, so an edge does not count).
float3 Normal(float2 uv, float z)
{
    float2 px = Screen.xy * 1.5;
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

// Ambient occlusion: how much of the open air around a surface point is closed off by what is next to it (12
// points on a turning spiral within the radius, from the depth). Corners, doorways, the ground under eaves and walls
// darken; open ground does not.
float Occlusion(float2 uv, float z, float2 pixel)
{
    float3 p = ViewRay(uv) * z;
    float3 n = Normal(uv, z);
    float radius = Shade.y;
    // The radius on screen at this distance (in uv), never under a few pixels or over a fifth of the screen.
    float2 span = clamp(radius * Projection.xy * 0.5 / z, Screen.xy * 3.0, 0.2);
    float turn = frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715)))) * 6.2831853;
    float closed = 0.0;
    [unroll]
    for (int i = 0; i < 12; i++)
    {
        float t = (i + 0.5) / 12.0;
        float a = turn + i * 2.3999632;                   // the golden angle
        float2 at = uv + float2(cos(a), sin(a)) * span * t;
        float3 q = ViewRay(at) * Distance(at);
        float3 d = q - p;
        float l = length(d);
        float range = saturate(1.0 - l / radius);
        closed += saturate(dot(n, d / max(l, 1e-3)) - 0.15) * range;
    }
    return saturate(1.0 - closed / 12.0 * 2.2);
}

// The point lights: on the surface (by distance and how it faces each), and in the air along the view ray (the
// light scattered toward the eye: the integral of 1 / distance squared along the ray, closed form).
void PointLights(float2 uv, float z, bool sky, float3 albedo, out float3 onSurface, out float3 inAir)
{
    onSurface = 0.0;
    inAir = 0.0;
    float3 ray = ViewRay(uv);
    float3 p = ray * z;
    float3 v = normalize(ray);
    float s = sky ? 300.0 : length(p);
    float3 n = sky ? float3(0, 0, -1) : Normal(uv, z);
    [unroll]
    for (int i = 0; i < kLights; i++)
    {
        if (i < Lights.x)
        {
            float3 at = LightAt[i * 2].xyz;
            float reach = max(LightAt[i * 2].w, 0.5);
            float3 colour = LightAt[i * 2 + 1].rgb;
            if (!sky)
            {
                float3 toLight = at - p;
                float d = length(toLight);
                float fall = saturate(1.0 - (d / reach) * (d / reach));
                fall *= fall;
                float facing = saturate(dot(n, toLight / max(d, 1e-3)) * 0.85 + 0.15);
                onSurface += colour * albedo * fall * facing / (1.0 + d * d * 0.04);
            }
            float tc = dot(at, v);
            float h = sqrt(max(dot(at, at) - tc * tc, 0.04));
            float scatter = (atan((s - tc) / h) - atan(-tc / h)) / h;
            float near = saturate(1.0 - h / reach);
            inAir += colour * scatter * near * near;
        }
    }
}

float3 ToWorld(float3 v)
{
    return float3(dot(ViewX.xyz, v), dot(ViewY.xyz, v), dot(ViewZ.xyz, v));
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

// How much haze lies between the eye and a pixel: by distance (past its start) and, the camera known, by height
// (thicker low down), drifting in wisps.
float HazeAmount(float2 uv, float z, bool sky)
{
    float3 ray = float3((uv.x * 2.0 - 1.0) / Projection.x, (1.0 - uv.y * 2.0) / Projection.y, 1.0);
    float r = sky ? 1500.0 : length(ray) * z;
    float through = max(r - HazeShape.x, 0.0);
    float base = Haze.w * 0.01;
    float depth = base * through;
    float wisp = 1.0;
    if (Projection.w > 0.5)
    {
        float3 dir = normalize(ToWorld(ray));
        float falloff = max(HazeShape.y, 1e-4);
        base *= exp(-falloff * HazeShape.w);
        float dz = dir.z * falloff;
        depth = abs(dz) > 1e-4 ? base * (1.0 - exp(-dz * through)) / dz : base * through;
        float3 p = Eye.xyz + dir * min(r, 300.0);
        float3 drift = float3(0.6, 0.2, 0.05) * Mist.w * Mist.z;
        wisp = 1.0 + (Wisps(p / max(Mist.y, 1.0) + drift) - 0.5) * 2.4 * Mist.x * smoothstep(8.0, 40.0, r);
    }
    return saturate((1.0 - exp(-depth * wisp)) * HazeShape.z);
}

float4 Final(float2 uv : TEXCOORD0, float2 pixel : VPOS) : COLOR
{
    float3 scene = Sample(SceneS, uv);
    // Compare: the left half untouched.
    if (Shade.w > 0.5 && uv.x < 0.5)
        return float4(scene, 1.0);
    float z = Distance(uv);
    bool sky = z >= kSky;
    float3 bloom = Sample(LevelS, uv) / 5.0;          // five levels summed
    float3 wide = Sample(WideS, uv) / 3.0;            // the three widest
    float night = Night.x;

    // Night: the dark parts darker and colder; what is lit (lamps, windows, fire, spells) keeps its light.
    float l = dot(scene, kLuma);
    float keep = smoothstep(Night.z, Night.w, l);
    float3 colour = scene * lerp(lerp(1.0, Night.y, night), 1.0, keep);
    float grey = dot(colour, kLuma);
    colour = lerp(colour, grey * NightLook.yzw, night * NightLook.x * (1.0 - keep));

    // Ambient occlusion (by night less: little ambient light is left to block).
    float ao = 1.0;
    if (!sky && Shade.x > 0.0 && z < 300.0)
    {
        ao = Occlusion(uv, z, pixel);
        ao = lerp(1.0, ao, Shade.x * lerp(1.0, Shade.z, night) * (1.0 - keep));
        colour *= ao;
    }

    // The lights' spill: the wide light falls on the ground and walls about each light, in their own colour.
    float3 spill = sky ? 0.0 : wide * (0.25 + scene * 2.0) * Light.y * (0.35 + 0.65 * night);
    colour += spill;

    // Haze: the game's haze colour (dimmer by night), and the lights glowing through it.
    float haze = HazeAmount(uv, z, sky);
    float3 hazeLight = Haze.rgb * lerp(1.0 + Split.z, Light.w, night);
    colour = lerp(colour, hazeLight, haze);
    float3 halos = wide * haze * Light.z * (0.4 + 0.6 * night) * 4.0;
    colour += halos;

    // The point lights: brighter by night (by day they show by Lights.w).
    float3 lit = 0.0, glow = 0.0;
    if (Lights.x > 0.5)
    {
        PointLights(uv, z, sky, scene * 1.6 + 0.04, lit, glow);
        float shown = lerp(Lights.w, 1.0, night);
        lit *= Lights.y * shown;
        glow *= Lights.z * shown * (0.3 + haze * 2.0) * 0.15;
        colour += lit + glow;
    }

    // Bloom.
    colour += bloom * Light.x;

    if (Eye.w > 0.5)
    {
        if (Eye.w < 1.5)
            return float4(bloom * Light.x, 1.0);
        if (Eye.w < 2.5)
            return float4(lerp(0.0, hazeLight, haze) + halos, 1.0);
        if (Eye.w < 3.5)
            return float4(lerp(float3(1, 1, 1), float3(0.1, 0.15, 0.4), night * (1.0 - keep)), 1.0);
        if (Eye.w < 4.5)
            return float4(spill, 1.0);
        if (Eye.w < 5.5)
            return float4(lit + glow, 1.0);
        return float4(ao.xxx, 1.0);
    }

    // The colour grade: exposure, the tint (keeping brightness), saturation, contrast, a soft shoulder for the
    // highlights the light added, the vignette.
    colour *= Grade.w;
    // By day: warm highlights, cool shadows (the light of a sunny afternoon).
    float day = 1.0 - night;
    float bright = saturate(dot(colour, kLuma) * 1.4);
    float3 warm = float3(1.0 + Split.x, 1.0 + Split.x * 0.35, 1.0 - Split.x * 0.6);
    float3 cool = float3(1.0 - Split.y * 0.5, 1.0 - Split.y * 0.1, 1.0 + Split.y);
    colour *= lerp(1.0, lerp(cool, warm, smoothstep(0.15, 0.75, bright)), day);
    float3 tinted = colour * Tint.rgb / max(dot(Tint.rgb, kLuma), 0.05);
    colour = lerp(colour, tinted, Tint.a);
    grey = dot(colour, kLuma);
    colour = max(lerp(grey.xxx, colour, Grade.x), 0.0);
    colour = max((colour - 0.5) * Grade.y + 0.5, 0.0);
    float shoulder = Screen.w;
    float3 over = max(colour - shoulder, 0.0);
    colour = min(colour, shoulder) + over / (1.0 + over / max(1.0 - shoulder, 1e-3));
    float2 c = (uv - 0.5) * float2(Screen.z, 1.0);
    colour *= 1.0 - Grade.z * smoothstep(0.35, 1.1, length(c));
    return float4(colour, 1.0);
}
