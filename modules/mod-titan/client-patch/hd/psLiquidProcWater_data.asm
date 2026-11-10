; The ReShade water's variant of psLiquidProcWater.asm's tail (make_procwater.py splices it in from the "tail" line):
; all water, made opaque, hands Emberforge_Water.fx what it needs to draw the water itself, in a checkerboard (vPos):
; - even pixels (x + y even): the water's own colour by depth, this zone and hour, fogged, without waves or
;   reflections; the lowest bit of red, green and blue is 1, 0, 1;
; - odd pixels: where it is in the world, x and y modulo 128 yards in 1024 steps (an eighth of a yard): red and
;   green the top 8 bits, blue's bits 1-2 and 3-4 the low 2 of x and y, bit 5 a check (red + green odd), bit 0 is 0,
;   bit 6 is 0 and bit 7 is 1 (a sky's blues passed fewer tag bits by chance).
; A flat colour can never be both (blue's lowest bit 1 and 0), so nothing else on screen passes for water. Without
; ReShade the checkerboard would show: the launcher installs this variant only while Lighting is on.
; The water's colour, fogged.
texld r13, v5, s2
add r13.xyz, r13, -c4
mad r13.xyz, v7.x, r13, c4
mov_sat r11.xyz, r13
mul r11.xyz, r11, c14.w
frc r12.xyz, r11
add r11.xyz, r11, -r12
mad r11.xyz, r11, c0.y, c16
mul r11.xyz, r11, c15.w
; Where it is: q = 0-1023 per axis, its top 8 bits and low 2, and the check.
mul r14.xy, v6, c18.x
frc r14.xy, r14
mul r14.xy, r14, c18.y
frc r15.xy, r14
add r14.xy, r14, -r15
mul r15.xy, r14, c18.z
frc r16.xy, r15
add r15.xy, r15, -r16
mul r16.xy, r16, c18.w
add r17.w, r15.x, r15.y
mul r17.w, r17.w, c1.x
frc r17.w, r17.w
mov r17.xy, r15
mad r17.z, r16.x, c0.y, c19.x
mad r17.z, r16.y, c19.y, r17.z
mad r17.z, r17.w, c19.w, r17.z
add r17.xyz, r17, c19.z
mul r17.xyz, r17, c15.w
; Even or odd pixel.
add r12.w, vPos.x, vPos.y
mul r12.w, r12.w, c1.x
frc r12.w, r12.w
add r12.w, r12.w, -c1.x
; Every pixel of water, however shallow (only opaque water keeps the bits exact, and Stormwind's canals never got
; deep enough): ReShade draws all of it.
cmp oC0.xyz, r12.w, r17, r11
mov oC0.w, c0.w
