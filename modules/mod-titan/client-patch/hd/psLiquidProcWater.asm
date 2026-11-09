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
; - The sky picture is tinted by the hour's fog colour (c4): a fixed daytime sky made night water glow pale.
; - The wave maps are read 5 and 3.5 times larger (two scales, so the tiling does not show).
; - Deep water is opaque (its opacity ramps to 1 from 0.6 to 0.85) and signed for Emberforge_Reflections.fx (ReShade's
;   screen-space reflections of the city in the water): there, the lowest bit of red, green and blue is 1, 0, 1 on
;   pixels where x + y is even and 0, 1, 0 where it is odd. Invisible, and nothing else on screen makes that
;   checkerboard, so the effect finds exactly the water (floors and streets stay matte).
;
; Inputs: v0-v2 the surface's frame (tangent, bitangent, normal), v3 and v4 the near and far wave maps' coordinates
; (two sets each: .xy and .wz), v5 the depth texture's (.xy) and the glitter mask's (.wz), v6 the world position, v7
; the fog. Constants: c4 fog colour, c5 camera, c6 the sun's direction for the highlight, c9 highlight colour and
; exponent, c10 (normal fade, -, reflection base, reflection Fresnel), c11 (edge opacity, glitter base, glitter
; Fresnel). Samplers: s1 sky cube, s2 depth colour and opacity, s3 glitter mask, s4 and s5 wave normal maps.
ps_3_0
def c0, 0, 2, -1, 1
def c1, 0.5, 4, 0, 0
def c12, 3, 0.12, -0.5, 1         ; wave light: tilt gain, glow, darkest, brightest (subtle: 0.3 read as marble)
def c17, 2.2, 2.2, 2.2, 0          ; the sky picture times the hour's fog colour, times this
def c14, 0.6, 0.85, 4, 127.5        ; opaque from: start, end, 1 / (end - start); half of 255
def c15, -1, 1, -1, 0.00392157      ; the signature: odd minus even bits; 1 / 255
def c16, 1.25, 0.25, 1.25, 0        ; even pixels' bits (+ 0.25 / 255, so rounding or truncation keeps the bit)
def c13, 0.2, 0.29, 0.08, 0          ; the wave maps' scale: waves 5 and 3.5 times larger (at 1, a yard: marbling);
                                   ; the lowest a reflection looks (a crest's back face mirrored the dark below)
dcl_texcoord4 v0.xyz
dcl_texcoord5 v1.xyz
dcl_texcoord6 v2.xyz
dcl_texcoord0 v3
dcl_texcoord1 v4
dcl_texcoord2 v5
dcl_texcoord3 v6.xyz
dcl_fog v7.x
dcl_vpos vPos.xy
dcl_cube s0
dcl_cube s1
dcl_2d s2
dcl_2d s3
dcl_2d s4
dcl_2d s5
; Near waves: the map at two coordinates (red/green, then alpha/blue), z rebuilt, summed.
mul r9, v3, c13.x
texld r0, r9, s4
mad r0.xy, r0, c0.y, c0.z
dp2add r0.w, r0, -r0, c0.w
rsq r0.w, r0.w
rcp r0.z, r0.w
mul r9, v3, c13.y
texld r1, r9.wzzw, s4
mad r1.xy, r1.wzzw, c0.y, c0.z
dp2add r0.w, r1, -r1, c0.w
rsq r0.w, r0.w
rcp r1.z, r0.w
add r0.xyz, r0, r1
nrm r0.xyz, r0
; Far waves, the same from the second map.
mul r9, v4, c13.x
texld r1, r9, s5
mad r1.xy, r1, c0.y, c0.z
dp2add r1.w, r1, -r1, c0.w
rsq r1.w, r1.w
rcp r1.z, r1.w
mul r9, v4, c13.y
texld r2, r9.wzzw, s5
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
; Opacity: the depth's, plus the edge's at a glance; deep water ramps to opaque (r10.x: 1 where fully).
texld r3, v5, s2
mad r10.w, c11.x, r8.x, r3.w
add r10.x, r10.w, -c14.x
mul_sat r10.x, r10.x, c14.z
lrp r10.w, r10.x, c0.w, r10.w
add r10.y, r10.x, -c0.w
; The sky the wave mirrors.
dp3 r1.w, r6, r5
add r1.w, r1.w, r1.w
mad r4.xyz, r5.xzyw, -r1.w, r6.xzyw
max r4.y, r4.y, c13.z
texld r2, r4, s1
mul r2.xyz, r2, c4
mul r2.xyz, r2, c17
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
mad r0.xyz, v7.x, r0, c4
; The signature where the water is opaque: each channel to an even step of 255, plus its bit.
mov_sat r11.xyz, r0
mul r11.xyz, r11, c14.w
frc r12.xyz, r11
add r11.xyz, r11, -r12
add r12.w, vPos.x, vPos.y
mul r12.w, r12.w, c1.x
frc r12.w, r12.w
add r12.w, r12.w, r12.w
mad r12.xyz, c15, r12.w, c16
mad r11.xyz, r11, c0.y, r12
mul r11.xyz, r11, c15.w
cmp oC0.xyz, r10.y, r11, r0
mov oC0.w, r10.w
