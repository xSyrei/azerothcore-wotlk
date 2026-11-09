/*
 * Emberforge Light Sources: lanterns, torches, campfires, braziers and lit windows light what is around them, with
 * flicker, for the WoW 3.3.5a client, a ReShade effect. The game draws its flames glowing, but they light nothing.
 *
 *  1. Emitters, at an eighth of the screen: what is bright and warm (a flame's colours: red over green over blue),
 *     averaged over 8x8 pixels so thin bright things (text, sparks) do not count, with its depth.
 *  2. Light, at half the screen: each pixel gathers the emitters around it on a 9x9 grid spanning the light's reach,
 *     each by its distance in 3D (from the depth buffer: a lamp lights the wall beside it, not the mountain behind) and
 *     by whether the surface faces it.
 *  3. Applied: the light multiplies the surface's own colour (lighting it, as real light does) plus a little glow in
 *     the air, flickering; less by day.
 * After the water and occlusion, before the fog and bloom (ReShadePreset.ini's order).
 */

#include "ReShade.fxh"

uniform float Intensity <
    ui_type = "slider"; ui_min = 0.0; ui_max = 4.0; ui_step = 0.01;
    ui_category = "Light"; ui_label = "Intensity";
    ui_tooltip = "How strongly lights light their surroundings.";
> = 1.6;

uniform float Reach <
    ui_type = "slider"; ui_min = 1.0; ui_max = 30.0; ui_step = 0.1;
    ui_category = "Light"; ui_label = "Reach (yards)";
    ui_tooltip = "How far a light reaches.";
> = 9.0;

uniform float AirGlow <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Light"; ui_label = "Glow in the air";
    ui_tooltip = "Light added on top (a haze around lights), as well as on the surfaces.";
> = 0.15;

uniform float3 LightTint <
    ui_type = "color";
    ui_category = "Light"; ui_label = "Tint";
    ui_tooltip = "Multiplies the lights' own colour (white: as they are).";
> = float3(1.0, 0.85, 0.65);

uniform float DaylightDimming <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Light"; ui_label = "Dimmer by day";
    ui_tooltip = "How much less lights show when the scene around them is already bright.";
> = 0.6;

uniform float Flicker <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Light"; ui_label = "Flicker";
> = 0.35;

uniform float Threshold <
    ui_type = "slider"; ui_min = 0.3; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Sources"; ui_label = "Brightness threshold";
    ui_tooltip = "Only what is brighter than this gives light.";
> = 0.62;

uniform float Warmth <
    ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
    ui_category = "Sources"; ui_label = "Only warm lights";
    ui_tooltip = "1: only fire-coloured light (flames, lamps); 0: any bright colour (also magic, crystals).";
> = 0.7;

uniform bool ShowLight <
    ui_category = "Sources"; ui_label = "Show the light only";
    ui_tooltip = "Shows the light the sources cast, alone, to check what gives light.";
> = false;

uniform float FieldOfView <
    ui_type = "slider"; ui_min = 60.0; ui_max = 120.0; ui_step = 1.0;
    ui_category = "Other"; ui_label = "Field of view";
    ui_tooltip = "The game camera's diagonal field of view in degrees (90 unless changed).";
> = 90.0;

uniform float Timer < source = "timer"; >;

#define YARDS 1000.0
#define GRID 4                   // taps each way from the centre: (2 * GRID + 1)^2

texture EmitTex { Width = BUFFER_WIDTH / 8; Height = BUFFER_HEIGHT / 8; Format = RGBA16F; };
sampler EmitS { Texture = EmitTex; MinFilter = POINT; MagFilter = POINT; };
texture LightTex { Width = BUFFER_WIDTH / 2; Height = BUFFER_HEIGHT / 2; Format = RGBA16F; };
sampler LightS { Texture = LightTex; };

float Luma(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

float2 TanHalfFov()
{
    float diagonal = tan(radians(FieldOfView) * 0.5);
    float y = diagonal / sqrt(1.0 + BUFFER_ASPECT_RATIO * BUFFER_ASPECT_RATIO);
    return float2(y * BUFFER_ASPECT_RATIO, y);
}

float3 ViewPos(float2 uv, float depth)
{
    float2 t = TanHalfFov();
    return float3((uv.x * 2.0 - 1.0) * t.x, (1.0 - uv.y * 2.0) * t.y, 1.0) * depth * YARDS;
}

float4 PS_Emit(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    // 8x8 pixels as 16 bilinear taps.
    float3 c = 0.0;
    [unroll]
    for (int y = 0; y < 4; y++)
    {
        [unroll]
        for (int x = 0; x < 4; x++)
            c += tex2Dlod(ReShade::BackBuffer, float4(uv + (float2(x, y) - 1.5) * 2.0 * BUFFER_PIXEL_SIZE, 0, 0)).rgb;
    }
    c /= 16.0;
    float depth = ReShade::GetLinearizedDepth(uv);
    if (depth >= 0.9995 || depth <= 0.0001)
        return float4(0.0, 0.0, 0.0, depth);
    float bright = smoothstep(Threshold, Threshold + 0.15, max(c.r, max(c.g, c.b)));
    // Fire: red over green over blue.
    float fire = saturate((c.r - c.b) / max(c.r, 1e-3) * 1.6) * saturate((c.g - c.b * 0.8) / max(c.r, 1e-3) * 2.0);
    float colourOk = lerp(1.0, smoothstep(0.25, 0.6, fire), Warmth);
    return float4(c * bright * colourOk, depth);
}

float4 PS_Gather(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float depth = ReShade::GetLinearizedDepth(uv);
    if (depth >= 0.9995 || depth <= 0.0001)
        return 0.0;
    float3 p = ViewPos(uv, depth);
    // The surface's normal from the depth (the smaller step each way, so an edge does not count).
    float2 px = BUFFER_PIXEL_SIZE * 2.0;
    float3 right = ViewPos(uv + float2(px.x, 0), ReShade::GetLinearizedDepth(uv + float2(px.x, 0))) - p;
    float3 left = p - ViewPos(uv - float2(px.x, 0), ReShade::GetLinearizedDepth(uv - float2(px.x, 0)));
    float3 down = ViewPos(uv + float2(0, px.y), ReShade::GetLinearizedDepth(uv + float2(0, px.y))) - p;
    float3 up = p - ViewPos(uv - float2(0, px.y), ReShade::GetLinearizedDepth(uv - float2(0, px.y)));
    float3 dx = abs(right.z) < abs(left.z) ? right : left;
    float3 dy = abs(down.z) < abs(up.z) ? down : up;
    float3 n = normalize(cross(dy, dx));
    n = dot(n, -p) < 0.0 ? -n : n;

    // The grid spans the light's reach on screen at this depth.
    float2 t = TanHalfFov();
    float2 span = Reach / (p.z * 2.0 * t);                     // the reach as a share of the screen
    float2 step = span / GRID;
    float3 light = 0.0;
    [loop]
    for (int y = -GRID; y <= GRID; y++)
    {
        [loop]
        for (int x = -GRID; x <= GRID; x++)
        {
            float2 at = uv + float2(x, y) * step;
            float4 e = tex2Dlod(EmitS, float4(at, 0, 0));
            if (max(e.r, max(e.g, e.b)) <= 0.0)
                continue;
            float3 d = ViewPos(at, e.a) - p;
            float dist = length(d);
            float fall = saturate(1.0 - dist / Reach);
            float facing = saturate(dot(n, d / max(dist, 1e-3)) * 0.75 + 0.25);
            light += e.rgb * fall * fall * facing;
        }
    }
    // Normalised so a light's strength does not depend on how many grid taps it covers.
    return float4(light * (6.0 / ((2 * GRID + 1) * (2 * GRID + 1))) / max(step.x * step.y * 4000.0, 0.05), 1.0);
}

float3 PS_Apply(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float3 colour = tex2D(ReShade::BackBuffer, uv).rgb;
    float3 light = tex2D(LightS, uv).rgb * LightTint;
    if (!all(light == light))                                  // a NaN from bad depth never shows
        light = 0.0;
    float time = Timer * 0.001;
    float flicker = 1.0 + Flicker * 0.35 * (sin(time * 9.1) * 0.5 + sin(time * 13.7 + 1.3) * 0.3
                                            + sin(time * 23.3 + uv.x * 3.0) * 0.2);
    // By day the scene around is already lit: the lights show less.
    float day = smoothstep(0.15, 0.6, Luma(colour));
    light *= Intensity * flicker * (1.0 - DaylightDimming * day);
    if (ShowLight)
        return light;
    return colour + colour * light + light * AirGlow;
}

technique Emberforge_Lights <
    ui_label = "Emberforge Light Sources";
    ui_tooltip = "Lanterns, torches, campfires and lit windows light what is around them, with flicker.";
>
{
    pass Emit { VertexShader = PostProcessVS; PixelShader = PS_Emit; RenderTarget = EmitTex; }
    pass Gather { VertexShader = PostProcessVS; PixelShader = PS_Gather; RenderTarget = LightTex; }
    pass Apply { VertexShader = PostProcessVS; PixelShader = PS_Apply; }
}
