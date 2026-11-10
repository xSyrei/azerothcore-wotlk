// Emberforge Engine: the depth buffer (INTZ, as the game drew it) to the distance from the eye in yards, for the water
// and the effects after it. The game's projection is infinite with its near plane n, and draws the world into the
// depth range MinZ..MaxZ (the sky at 1): the distance is n / (1 - d), d the depth within that range.

sampler2D DepthS : register(s0);
float4 Params : register(c0);   // near plane, MinZ, MaxZ

float4 main(float2 uv : TEXCOORD0) : COLOR
{
    float d = (tex2D(DepthS, uv).r - Params.y) / max(Params.z - Params.y, 1e-6);
    float z = d >= 0.99999 ? 100000.0 : Params.x / max(1.0 - d, 1e-6);
    return float4(z, z, z, 1.0);
}
