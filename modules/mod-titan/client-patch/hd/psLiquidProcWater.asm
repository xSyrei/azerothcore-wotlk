; Emberforge water: the client's procedural water pixel shader (Shaders\Pixel\ps_3_0\psLiquidProcWater.bls),
; rewritten. Same inputs and constants as the game's; assembled by make_procwater.py (d3d9asm.py).
;
; What changed from the game's:
; - Fresnel: its view vector was divided by the horizontal distance only, so looking down at the water its dot
;   product saturated: no reflection and no change across the waves (flat glass). Here it is a real unit vector, and
;   the reflection grows with (1 - cos)^4 at a glance.
; - The water's own colour is lit by the waves: faces turned to the eye glow (light through the wave), faces turned
;   away go darker, so every wave shows its shape, not only where it mirrors the sky.
; - No refraction cube (it added one flat colour).
;
; Inputs: v0-v2 the surface's frame (tangent, bitangent, normal), v3 and v4 the near and far wave maps' coordinates
; (two sets each: .xy and .wz), v5 the depth texture's (.xy) and the glitter mask's (.wz), v6 the world position, v7
; the fog. Constants: c4 fog colour, c5 camera, c6 the sun's direction for the highlight, c9 highlight colour and
; exponent, c10 (normal fade, -, reflection base, reflection Fresnel), c11 (edge opacity, glitter base, glitter
; Fresnel). Samplers: s1 sky cube, s2 depth colour and opacity, s3 glitter mask, s4 and s5 wave normal maps.
ps_3_0
def c0, 0, 2, -1, 1
def c1, 0.5, 4, 0, 0
def c12, 3, 0.4, -0.5, 1          ; wave light: tilt gain, glow, darkest, brightest
dcl_texcoord4 v0.xyz
dcl_texcoord5 v1.xyz
dcl_texcoord6 v2.xyz
dcl_texcoord0 v3
dcl_texcoord1 v4
dcl_texcoord2 v5
dcl_texcoord3 v6.xyz
dcl_fog v7.x
dcl_cube s0
dcl_cube s1
dcl_2d s2
dcl_2d s3
dcl_2d s4
dcl_2d s5
; Near waves: the map at two coordinates (red/green, then alpha/blue), z rebuilt, summed.
texld r0, v3, s4
mad r0.xy, r0, c0.y, c0.z
dp2add r0.w, r0, -r0, c0.w
rsq r0.w, r0.w
rcp r0.z, r0.w
texld r1, v3.wzzw, s4
mad r1.xy, r1.wzzw, c0.y, c0.z
dp2add r0.w, r1, -r1, c0.w
rsq r0.w, r0.w
rcp r1.z, r0.w
add r0.xyz, r0, r1
nrm r0.xyz, r0
; Far waves, the same from the second map.
texld r1, v4, s5
mad r1.xy, r1, c0.y, c0.z
dp2add r1.w, r1, -r1, c0.w
rsq r1.w, r1.w
rcp r1.z, r1.w
texld r2, v4.wzzw, s5
mad r2.xy, r2.wzzw, c0.y, c0.z
dp2add r1.w, r2, -r2, c0.w
rsq r1.w, r1.w
rcp r2.z, r1.w
add r1.xyz, r1, r2
nrm r2.xyz, r1
; Near to far with the horizontal distance (as the game's).
add r1.xyz, v6, -c5
dp2add r0.w, r1, r1, c0.x
rsq r0.w, r0.w
rcp r1.w, r0.w
mov r3.x, c10.x
mad_sat r1.w, r1.w, r3.x, c1.x
add r0.xyz, r0, -r2
mad r0.xyz, r1.w, r0, r2
; The normal in the world.
dp3 r2.x, v0, r0
dp3 r2.y, v1, r0
dp3 r2.z, v2, r0
nrm r5.xyz, r2
; The sun's highlight, as the game's (its half vector).
mad r3.xyz, r1, r0.w, c6
nrm r4.xyz, r3
dp3 r3.w, r4, r5
max r3.w, r3.w, c0.x
pow r3.w, r3.w, c9.w
mul r7.xyz, r3.w, c9
; Fresnel from the true view direction.
nrm r6.xyz, r1
dp3_sat r0.w, -r6, r5
add r0.w, -r0.w, c0.w
mul r8.x, r0.w, r0.w
mul r8.y, r8.x, r8.x
; Opacity: the depth's, plus the edge's at a glance.
texld r3, v5, s2
mad oC0.w, c11.x, r8.x, r3.w
; The sky the wave mirrors.
dp3 r1.w, r6, r5
add r1.w, r1.w, r1.w
mad r4.xyz, r5.xzyw, -r1.w, r6.xzyw
texld r2, r4, s1
; The water's colour lit by the wave: the slope toward the eye, scaled, kept within darkest..brightest.
dp2add r8.z, r5, -r6, c0.x
mul r8.z, r8.z, c12.x
max r8.z, r8.z, c12.z
min r8.z, r8.z, c12.w
mad r8.z, r8.z, c12.y, c0.w
mul r3.xyz, r3, r8.z
; Colour and sky by the reflection's share (its base, plus its Fresnel part).
mad r1.w, c10.w, r8.y, c10.z
lrp r0.xyz, r1.w, r2, r3
; The glitter: highlight times the mask's alpha, x4, by its share.
texld r2, v5.wzzw, s3
mul r7.xyz, r7, r2.w
mul r7.xyz, r7, c1.y
mad r1.w, c11.z, r8.x, c11.y
mad r0.xyz, r7, r1.w, r0
; Fog.
add r0.xyz, r0, -c4
mad oC0.xyz, v7.x, r0, c4
