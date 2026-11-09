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
EDGE_OPACITY = 40           # float 17: c11.x, the opacity Fresnel adds at a glance


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
    struct.pack_into('<I', data, 16, len(data) - strings_at)
    # More translucent at a glance (Fresnel adds 0.12 opacity, not 0.2).
    procedural[EDGE_OPACITY] = struct.unpack('<I', struct.pack('<f', 0.12))[0]
    for water in WATERS:
        if water in rows:
            for field in MATERIAL_FIELDS:
                struct.pack_into('<I', data, rows[water] + field * 4, procedural[field])
    return bytes(data)


def spec_frames():
    """The specular mask: alpha 90-200 (x4 in the shader), glittering finely and looping, so the sun's highlight
    sparkles across the waves rather than shining as one flat sheet."""
    rng = np.random.default_rng(32)
    frames = []
    for field in periodic_noise(rng, 60, 240, 1.0, 2.0):
        alpha = np.clip(145 + field * 32, 90, 200)
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
        n = np.stack([-gx, -gy, np.full_like(h, 0.2)], -1)
        n /= np.linalg.norm(n, axis=-1, keepdims=True)
        x, y = (n[..., 0] * 0.5 + 0.5) * 255, (n[..., 1] * 0.5 + 0.5) * 255
        rgba = np.stack([x, y, y, x], -1)            # red/green and alpha/blue: the same normal
        frames.append(Image.fromarray(np.clip(np.round(rgba), 0, 255).astype(np.uint8), 'RGBA'))
    return frames


def reflection_map():
    """A sky over a horizon: deep blue overhead, pale toward the horizon, the dark sea below it."""
    size = 256
    v = np.linspace(0, 1, size)[:, None] * np.ones((1, size))
    sky_top, sky_low, sea = np.array([60, 105, 170]), np.array([190, 210, 230]), np.array([30, 50, 60])
    t = np.clip(v / 0.5, 0, 1)[..., None]
    below = sea + (sky_low - sea) * np.exp(-(v[..., None] - 0.5) * 12)
    color = np.where(v[..., None] < 0.5, sky_top * (1 - t) + sky_low * t, below)
    return Image.fromarray(np.dstack([np.clip(color, 0, 255), np.full((size, size), 255)]).astype(np.uint8), 'RGBA')


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    entries = [('DBFilesClient\\LiquidType.dbc', liquid_types(sys.argv[1]))]
    frames = normal_frames()
    for i in range(FRAMES + 1):
        entries.append((f'XTextures\\procWater\\basicWaterHeightTex_{i}.blp',
                        blp.encode_dxt(frames[(i - 1) % FRAMES])))
    entries.append(('XTextures\\procWater\\basicReflectionMap.blp', blp.encode_dxt(reflection_map())))
    masks = spec_frames()
    for i in range(FRAMES + 1):
        entries.append((SPEC_MASK.replace('%d', str(i)), blp.encode_dxt(masks[(i - 1) % FRAMES])))
    OUT.write_bytes(make_mpq.build(entries))
    print(f'Wrote {OUT} ({len(entries)} files, {OUT.stat().st_size / 2 ** 20:.1f} MB)')


if __name__ == '__main__':
    main()
