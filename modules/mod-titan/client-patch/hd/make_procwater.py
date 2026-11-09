#!/usr/bin/env python3
"""Experimental reflective water: dist/patch-enUS-7.MPQ.

The 3.3.5a client ships a full procedural water shader it never uses (psLiquidProcWater / vsLiquidProcWater,
Liquid::CMaterialProcWater): two scrolling normal maps, Fresnel, a specular sun highlight, reflections and the
game's fog. Only LiquidType.dbc's "Basic Procedural Water" (100, liquid material 3) asks for it, and its textures
were never shipped. This patch:
- moves every water and ocean liquid type onto that material, with its settings (LiquidType 100's material,
  textures and shader parameters, fields 14-44);
- makes its missing textures: XTextures\\procWater\\basicWaterHeightTex_<0-30>.blp, tiling, looping wave normal maps
  (the shader reads one normal from red/green and another, at other coordinates, from alpha/blue: both hold the
  same normal), and basicReflectionMap.blp, a sky over a horizon.

An experiment: whether the client feeds this shader everything it needs can only be seen in game. To try it, put
the archive in the client's Data\\enUS\\ and play; to undo it, delete it.

Usage: python3 make_procwater.py <dir with original DBCs (LiquidType.dbc)>
"""

import struct
import sys
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE.parent / 'models'))
sys.path.insert(0, str(HERE))
import blp  # noqa: E402
import make_mpq  # noqa: E402
from make_liquids import periodic_noise, SIZE, FRAMES  # noqa: E402

OUT = HERE.parent / 'dist' / 'patch-enUS-7.MPQ'
PROCEDURAL = 100
# Water and ocean (normal, slow, fast, WMO, interior, the raid and zone waters).
WATERS = [1, 2, 5, 6, 9, 10, 13, 14, 17, 41, 61, 81]
MATERIAL_FIELDS = range(14, 45)
# Read from Wow.exe (Liquid::CMaterialProcWater): the liquid's texture slots bind 0, 1 -> s0, s1 (the reflection
# cube maps), 4 -> s2 (the engine's depth texture: colour and opacity by depth, from the zone's light), 5 -> s3 (its
# alpha, times 4, masks the specular highlight), 2, 3 -> s4, s5 (the wave normal maps). Its floats from field 23:
# 0-3 scroll speeds, 4-7 their angles, 11-17 the pixel shader's c10 (normal fade, refraction ratio, reflection share
# and its Fresnel part) and c11 (edge opacity, specular share and its Fresnel part).
SPEC_MASK = 'XTextures\\procWater\\basicSpecMask_%d.blp'
SPEC_SLOT = 20              # texture slot 5
REFRACTION_SLOT = 15        # texture slot 0: s0, the cube added to the water's own colour (what is under it)
REFRACTION_MAP = 'XTextures\\procWater\\basicRefractionMap.blp'
EDGE_OPACITY = 40           # float 17: c11.x, the opacity Fresnel adds at a glance
REFLECTION_BASE = 36        # float 13: c10.z, the share of reflection looking straight down
REFLECTION_FRESNEL = 37     # float 14: c10.w, the share Fresnel adds at a glance


def liquid_types(source):
    data = bytearray(Path(source, 'LiquidType.dbc').read_bytes())
    count, fields, size, _ = struct.unpack_from('<4I', data, 4)
    rows = {}
    for i in range(count):
        offset = 20 + i * size
        rows[struct.unpack_from('<I', data, offset)[0]] = offset
    procedural = list(struct.unpack_from(f'<{fields}I', data, rows[PROCEDURAL]))
    # The specular mask in slot 5 (the ocean's glint texture there was almost transparent: no highlight), as a new
    # string at the end of the string block.
    strings_at = 20 + count * size
    procedural[SPEC_SLOT] = len(data) - strings_at
    data += SPEC_MASK.encode('ascii') + b'\0'
    # Its own refraction cube in slot 0 (it shared the sky's, which washed the water out).
    procedural[REFRACTION_SLOT] = len(data) - strings_at
    data += REFRACTION_MAP.encode('ascii') + b'\0'
    struct.pack_into('<I', data, 16, len(data) - strings_at)
    # More translucent at a glance (Fresnel adds 0.12 opacity, not 0.2).
    procedural[EDGE_OPACITY] = struct.unpack('<I', struct.pack('<f', 0.12))[0]
    # Mostly the water's own colour looking down, more sky across a lake; enough looking down that the waves show
    # (at 0.05 the surface read as flat glass).
    procedural[REFLECTION_BASE] = struct.unpack('<I', struct.pack('<f', 0.12))[0]
    procedural[REFLECTION_FRESNEL] = struct.unpack('<I', struct.pack('<f', 0.45))[0]
    for water in WATERS:
        if water in rows:
            for field in MATERIAL_FIELDS:
                struct.pack_into('<I', data, rows[water] + field * 4, procedural[field])
    return bytes(data)


def spec_frames():
    """The specular mask: alpha 120-255 (x4 in the shader), glittering finely and looping, so the sun's highlight
    sparkles across the waves rather than shining as one flat sheet."""
    rng = np.random.default_rng(32)
    frames = []
    for field in periodic_noise(rng, 60, 240, 1.0, 2.0):
        alpha = np.clip(190 + field * 40, 120, 255)
        rgba = np.dstack([np.full_like(alpha, 255), np.full_like(alpha, 255), np.full_like(alpha, 255), alpha])
        frames.append(Image.fromarray(np.round(rgba).astype(np.uint8), 'RGBA'))
    return frames


def normal_frames():
    rng = np.random.default_rng(31)
    # Rolling waves with ripples over them, steep enough to catch the light (the first try was too flat to see).
    heights = periodic_noise(rng, 3, 140, 2.2, 1.2)
    frames = []
    for h in heights:
        gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
        gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
        n = np.stack([-gx, -gy, np.full_like(h, 0.22)], -1)   # steep enough to read from above
        n /= np.linalg.norm(n, axis=-1, keepdims=True)
        x, y = (n[..., 0] * 0.5 + 0.5) * 255, (n[..., 1] * 0.5 + 0.5) * 255
        rgba = np.stack([x, y, y, x], -1)            # red/green and alpha/blue: the same normal
        frames.append(Image.fromarray(np.clip(np.round(rgba), 0, 255).astype(np.uint8), 'RGBA'))
    return frames


def reflection_map():
    """The sky the water mirrors, as the client keeps cube maps (TILESET\\Terrain Cube Maps): one strip of six square
    faces, the four sides, then the top and the bottom. A muted blue-grey sky (it is fixed, so it must not glow at
    night) shading paler toward the horizon, soft clouds, and dark below the horizon: the shader also looks there,
    for what it adds under the surface. On a flat image the cube came out black: no reflections at all."""
    face = 256
    rng = np.random.default_rng(33)
    k1 = np.fft.fftfreq(face) * face
    kx, ky = np.meshgrid(k1, k1)
    k = np.hypot(kx, ky)
    spectrum = np.where((k >= 2) & (k <= 40), np.maximum(k, 1) ** -2.0, 0) * np.exp(1j * rng.uniform(0, 6.3, k.shape))
    clouds = np.real(np.fft.ifft2(spectrum))
    # Faint, soft clouds: strong ones broke every wave into white blobs.
    clouds = np.clip((clouds - clouds.mean()) / clouds.std() * 0.4 + 0.15, 0, 1)
    # Below the horizon stays a soft horizon tone: tilted waves look there, and black beside the bright sky read as
    # camouflage.
    # A hazy teal-grey sky, darker than a sky looks, as water mirrors it (a pale one made it milky, a blue one dark),
    # dark overhead and bright at the horizon: that contrast is what each tilted wave catches, the surface's life.
    zenith, horizon, below = np.array([24, 50, 60]), np.array([118, 138, 136]), np.array([34, 62, 62])
    cloud = np.array([116, 130, 130])
    v = np.linspace(0, 1, face)[:, None] * np.ones((1, face))     # 0 top of a side face, 1 its bottom
    t = np.clip(v / 0.5, 0, 1)[..., None]
    sky = zenith * (1 - t) + horizon * t
    sky = sky * (1 - clouds[..., None] * 0.6 * (1 - t)) + cloud * clouds[..., None] * 0.6 * (1 - t)
    side = np.where(v[..., None] < 0.5, sky, below + (horizon - below) * np.exp(-(v[..., None] - 0.5) * 6))
    top = zenith * (1 - clouds[..., None] * 0.6) + cloud * clouds[..., None] * 0.6
    bottom = np.broadcast_to(below, (face, face, 3))
    faces = [np.roll(side, i * face // 4, axis=1) for i in range(4)] + [top, bottom]
    strip = np.concatenate(faces, axis=1)
    rgba = np.dstack([np.clip(strip, 0, 255), np.full(strip.shape[:2], 255)])
    return Image.fromarray(np.round(rgba).astype(np.uint8), 'RGBA')


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    entries = [('DBFilesClient\\LiquidType.dbc', liquid_types(sys.argv[1]))]
    frames = normal_frames()
    for i in range(FRAMES + 1):
        entries.append((f'XTextures\\procWater\\basicWaterHeightTex_{i}.blp',
                        blp.encode_dxt(frames[(i - 1) % FRAMES])))
    entries.append(('XTextures\\procWater\\basicReflectionMap.blp', blp.encode_dxt(reflection_map())))
    dark = np.zeros((128, 768, 4), np.uint8)
    dark[...] = (6, 14, 13, 255)          # a hint of teal from the depths
    entries.append((REFRACTION_MAP, blp.encode_dxt(Image.fromarray(dark, 'RGBA'))))
    masks = spec_frames()
    for i in range(FRAMES + 1):
        entries.append((SPEC_MASK.replace('%d', str(i)), blp.encode_dxt(masks[(i - 1) % FRAMES])))
    OUT.write_bytes(make_mpq.build(entries))
    print(f'Wrote {OUT} ({len(entries)} files, {OUT.stat().st_size / 2 ** 20:.1f} MB)')


if __name__ == '__main__':
    main()
