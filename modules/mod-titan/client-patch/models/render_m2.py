#!/usr/bin/env python3
"""Preview a WoW 3.3.5a M2 (version 264) model: parse it back and software-render it.

Reads the .m2, its 00.skin and the palettized BLP textures written by blp.py, straight from the
file layout (no code shared with the converter), so a broken offset, vertex layout, UV mapping or
triangle winding shows up in the picture. Also used to render the inventory icon.

Layers with an animated color (flashes, pulses) are left out: they are invisible most of the time.

Usage: python3 render_m2.py <model.m2> <texture dir> <out.png> [size] [angle degrees] [background RRGGBB]
"""

import math
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image


def arr(data, offset):
    return struct.unpack_from('<II', data, offset)


def read_blp(path):
    data = Path(path).read_bytes()
    magic, _, encoding, alpha_depth, _, _, width, height = struct.unpack_from('<4sIBBBBII', data)
    if magic != b'BLP2' or encoding != 1:
        raise ValueError(f'{path}: only palettized BLP2 is supported by this previewer')
    offset = struct.unpack_from('<I', data, 20)[0]
    palette = np.frombuffer(data, np.uint8, 1024, 148).reshape(256, 4)
    pixels = width * height
    idx = np.frombuffer(data, np.uint8, pixels, offset)
    rgb = palette[idx][:, [2, 1, 0]].astype(np.float32) / 255
    alpha = (np.frombuffer(data, np.uint8, pixels, offset + pixels).astype(np.float32) / 255
             if alpha_depth == 8 else np.ones(pixels, np.float32))
    return np.dstack([rgb.reshape(height, width, 3), alpha.reshape(height, width, 1)])


def load(m2_path, texture_dir):
    m2 = Path(m2_path).read_bytes()
    skin = Path(m2_path).with_name(Path(m2_path).stem + '00.skin').read_bytes()
    assert m2[:4] == b'MD20' and struct.unpack_from('<I', m2, 4)[0] == 264, 'not a 3.3.5a M2'
    assert skin[:4] == b'SKIN', 'not a skin file'

    count, offset = arr(m2, 0x3C)
    verts = np.frombuffer(m2, np.dtype([('pos', '<f4', 3), ('w', 'u1', 4), ('b', 'u1', 4), ('n', '<f4', 3),
                                         ('uv', '<f4', 2), ('uv2', '<f4', 2)]), count, offset)
    textures = []
    count, offset = arr(m2, 0x50)
    for i in range(count):
        _, _, n_len, n_off = struct.unpack_from('<IIII', m2, offset + i * 16)
        name = m2[n_off:n_off + n_len - 1].decode().split('\\')[-1]
        textures.append(read_blp(Path(texture_dir) / name))
    count, offset = arr(m2, 0x70)
    materials = [struct.unpack_from('<HH', m2, offset + i * 4) for i in range(count)]
    count, offset = arr(m2, 0x80)
    texture_lookup = struct.unpack_from(f'<{count}H', m2, offset)

    count, offset = arr(skin, 0x04)
    lookup = np.frombuffer(skin, '<u2', count, offset)
    count, offset = arr(skin, 0x0C)
    indices = np.frombuffer(skin, '<u2', count, offset)
    count, offset = arr(skin, 0x1C)
    sections = [struct.unpack_from('<10H', skin, offset + i * 48) for i in range(count)]
    count, offset = arr(skin, 0x24)
    batches = [struct.unpack_from('<Bb11H', skin, offset + i * 24) for i in range(count)]
    return verts, textures, materials, texture_lookup, lookup, indices, sections, batches


def render(m2_path, texture_dir, size=512, angle=45.0, background=None):
    verts, textures, materials, texture_lookup, lookup, indices, sections, batches = load(m2_path, texture_dir)
    pos, nrm, uv = verts['pos'], verts['n'], verts['uv']

    # Look along -Y (the flat of the blade), rotate in the X/Z plane, fit to the image.
    a = math.radians(angle)
    sx = pos[:, 0] * math.cos(a) - pos[:, 2] * math.sin(a)
    sy = pos[:, 0] * math.sin(a) + pos[:, 2] * math.cos(a)
    depth = pos[:, 1]
    span = max(sx.max() - sx.min(), sy.max() - sy.min()) * 1.06
    px = (sx - (sx.max() + sx.min()) / 2) / span * size + size / 2
    py = size / 2 - (sy - (sy.max() + sy.min()) / 2) / span * size
    light = np.clip(np.abs(nrm @ np.array([0.3, -0.8, 0.5]) / np.linalg.norm([0.3, -0.8, 0.5])), 0.25, 1)

    image = np.zeros((size, size, 3), np.float32)
    if background is not None:
        image[:] = np.array(background, np.float32) / 255
    zbuf = np.full((size, size), np.inf, np.float32)
    coverage = np.zeros((size, size), np.float32)
    facing = [0, 0]

    for flags, _, _, section, _, color, material, _, _, combo, _, _, _ in batches:
        if color != 0xFFFF:
            continue
        mat_flags, blend = materials[material]
        tex = textures[texture_lookup[combo]]
        th, tw = tex.shape[:2]
        _, _, vstart, vcount, istart, icount = sections[section][:6]
        for t in range(istart, istart + icount, 3):
            ids = lookup[indices[t:t + 3]]
            x, y = px[ids], py[ids]
            area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0])
            if blend == 0:
                facing[int(area > 0)] += 1
            if abs(area) < 1e-9:
                continue
            x0, x1 = int(max(0, math.floor(x.min()))), int(min(size - 1, math.ceil(x.max())))
            y0, y1 = int(max(0, math.floor(y.min()))), int(min(size - 1, math.ceil(y.max())))
            if x0 > x1 or y0 > y1:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            w0 = ((x[1] - gx) * (y[2] - gy) - (x[2] - gx) * (y[1] - gy)) / area
            w1 = ((x[2] - gx) * (y[0] - gy) - (x[0] - gx) * (y[2] - gy)) / area
            w2 = 1 - w0 - w1
            inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
            if not inside.any():
                continue
            z = w0 * depth[ids[0]] + w1 * depth[ids[1]] + w2 * depth[ids[2]]
            u = w0 * uv[ids[0], 0] + w1 * uv[ids[1], 0] + w2 * uv[ids[2], 0]
            v = w0 * uv[ids[0], 1] + w1 * uv[ids[1], 1] + w2 * uv[ids[2], 1]
            texel = tex[(np.mod(v, 1) * (th - 1)).astype(int), (np.mod(u, 1) * (tw - 1)).astype(int)]
            shade = w0 * light[ids[0]] + w1 * light[ids[1]] + w2 * light[ids[2]]
            region = (slice(y0, y1 + 1), slice(x0, x1 + 1))
            if blend == 0:
                visible = inside & (z < zbuf[region])
                zbuf[region][visible] = z[visible]
                lit = texel[..., :3] * (1 if mat_flags & 1 else shade[..., None])
                image[region][visible] = lit[visible]
                coverage[region][visible] = 1
            else:
                visible = inside & (z <= zbuf[region] + 1e-3)
                add = texel[..., :3] * (texel[..., 3:] if blend == 2 else 1)
                image[region][visible] = np.clip(image[region][visible] + add[visible], 0, 1)
                coverage[region][visible] = np.maximum(coverage[region][visible], add[visible].max(axis=-1))
    rgba = np.dstack([image, coverage if background is None else np.ones_like(coverage)])
    return Image.fromarray((rgba * 255).astype(np.uint8), 'RGBA'), facing


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    size = int(sys.argv[4]) if len(sys.argv) > 4 else 512
    angle = float(sys.argv[5]) if len(sys.argv) > 5 else 45.0
    background = tuple(int(sys.argv[6][i:i + 2], 16) for i in (0, 2, 4)) if len(sys.argv) > 6 else None
    image, facing = render(sys.argv[1], sys.argv[2], size * 2, angle, background)
    image.resize((size, size), Image.Resampling.LANCZOS).save(sys.argv[3])
    print(f'Wrote {sys.argv[3]}; opaque triangles facing the camera: {facing[1]}, facing away: {facing[0]}')


if __name__ == '__main__':
    main()
