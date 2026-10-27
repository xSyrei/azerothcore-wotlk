#!/usr/bin/env python3
"""Preview a login scene M2 through its own camera 0 (what the login screen shows).

Parses the .m2 / 00.skin back from the file layout (sections with their Level field, texture
lookups, materials, the camera's position and target keys) and renders it with perspective,
self-lit like the scene materials. Slow but dependency-light (numpy + Pillow).

Usage: python3 render_scene.py <scene.m2> <texture dir> <out.png> [width] [height]
"""

import math
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image

from render_m2 import arr, read_blp


def load(m2_path, texture_dir):
    m2 = Path(m2_path).read_bytes()
    skin = Path(m2_path).with_name(Path(m2_path).stem + '00.skin').read_bytes()
    n, o = arr(m2, 0x3C)
    verts = np.frombuffer(m2, np.dtype([('pos', '<f4', 3), ('w', 'u1', 4), ('b', 'u1', 4), ('n', '<f4', 3),
                                         ('uv', '<f4', 2), ('uv2', '<f4', 2)]), n, o)
    n, o = arr(m2, 0x50)
    textures = []
    for i in range(n):
        _, _, ln, lo = struct.unpack_from('<IIII', m2, o + i * 16)
        textures.append(read_blp(Path(texture_dir) / m2[lo:lo + ln - 1].decode().split('\\')[-1]))
    n, o = arr(m2, 0x70)
    materials = [struct.unpack_from('<HH', m2, o + i * 4) for i in range(n)]
    n, o = arr(m2, 0x80)
    texture_lookup = struct.unpack_from(f'<{n}H', m2, o)

    # Camera 0: position / target = first key of each track + base.
    n, o = arr(m2, 0x110)
    assert n >= 1, 'scene has no camera'
    _, fov, far, near = struct.unpack_from('<Ifff', m2, o)

    def first_key(track_off, base_off):
        _, _, _, _, vn, vo = struct.unpack_from('<HHIIII', m2, track_off)
        inner_n, inner_o = struct.unpack_from('<II', m2, vo)
        value = np.array(struct.unpack_from('<3f', m2, inner_o))
        return value + np.array(struct.unpack_from('<3f', m2, base_off))
    cam_pos = first_key(o + 16, o + 36)
    cam_target = first_key(o + 48, o + 68)

    n, o = arr(skin, 0x04)
    lookup = np.frombuffer(skin, '<u2', n, o)
    n, o = arr(skin, 0x0C)
    indices = np.frombuffer(skin, '<u2', n, o)
    n, o = arr(skin, 0x1C)
    sections = [struct.unpack_from('<10H', skin, o + i * 48) for i in range(n)]
    n, o = arr(skin, 0x24)
    batches = [struct.unpack_from('<Bb11H', skin, o + i * 24) for i in range(n)]
    return verts, textures, materials, texture_lookup, lookup, indices, sections, batches, (cam_pos, cam_target, fov, near)


def render(m2_path, texture_dir, width=960, height=540):
    verts, textures, materials, tex_lookup, lookup, indices, sections, batches, camera = load(m2_path, texture_dir)
    cam_pos, cam_target, fov, near = camera
    f = cam_target - cam_pos
    f /= np.linalg.norm(f)
    r = np.cross(f, [0, 0, 1])
    r /= np.linalg.norm(r)
    u = np.cross(r, f)
    aspect = width / height
    vfov = fov / math.sqrt(1 + aspect * aspect)          # M2 camera FOV is diagonal
    th = math.tan(vfov / 2)

    d = verts['pos'].astype(np.float64) - cam_pos
    x, y, z = d @ r, d @ u, d @ f
    with np.errstate(divide='ignore', invalid='ignore'):
        sx = width / 2 + x / (z * th * aspect) * width / 2
        sy = height / 2 - y / (z * th) * height / 2
    uv = verts['uv']

    image = np.zeros((height, width, 3), np.float32)
    zbuf = np.full((height, width), np.inf, np.float32)
    for flags, _, _, section, _, color, material, _, _, combo, _, _, _ in batches:
        mflags, blend = materials[material]
        tex = textures[tex_lookup[combo]]
        th_, tw = tex.shape[:2]
        _, level, vstart, vcount, istart, icount = sections[section][:6]
        istart += level << 16
        for t in range(istart, istart + icount, 3):
            ids = lookup[indices[t:t + 3]]
            if (z[ids] < near).any():
                continue
            px, py = sx[ids], sy[ids]
            area = (px[1] - px[0]) * (py[2] - py[0]) - (px[2] - px[0]) * (py[1] - py[0])
            if abs(area) < 1e-6:
                continue
            x0, x1 = int(max(0, math.floor(px.min()))), int(min(width - 1, math.ceil(px.max())))
            y0, y1 = int(max(0, math.floor(py.min()))), int(min(height - 1, math.ceil(py.max())))
            if x0 > x1 or y0 > y1:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            w0 = ((px[1] - gx) * (py[2] - gy) - (px[2] - gx) * (py[1] - gy)) / area
            w1 = ((px[2] - gx) * (py[0] - gy) - (px[0] - gx) * (py[2] - gy)) / area
            w2 = 1 - w0 - w1
            inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
            if not inside.any():
                continue
            # perspective-correct interpolation
            iz = w0 / z[ids[0]] + w1 / z[ids[1]] + w2 / z[ids[2]]
            depth = 1 / iz
            tu = (w0 * uv[ids[0], 0] / z[ids[0]] + w1 * uv[ids[1], 0] / z[ids[1]] + w2 * uv[ids[2], 0] / z[ids[2]]) * depth
            tv = (w0 * uv[ids[0], 1] / z[ids[0]] + w1 * uv[ids[1], 1] / z[ids[1]] + w2 * uv[ids[2], 1] / z[ids[2]]) * depth
            texel = tex[(np.mod(tv, 1) * (th_ - 1)).astype(int), (np.mod(tu, 1) * (tw - 1)).astype(int)]
            region = (slice(y0, y1 + 1), slice(x0, x1 + 1))
            zb = zbuf[region]
            if blend == 0 or blend == 1:
                vis = inside & (depth < zb)
                if blend == 1:
                    vis &= texel[..., 3] > 0.5
                zb[vis] = depth[vis]
                image[region][vis] = texel[..., :3][vis]
            else:
                vis = inside & (depth < zb)
                a = texel[..., 3:] if blend in (2, 4) else 1.0
                src = texel[..., :3] * a
                if blend == 2:
                    image[region][vis] = (image[region] * (1 - a) + src)[vis]
                else:
                    image[region][vis] = np.clip(image[region] + src, 0, 1)[vis]
    return Image.fromarray((np.clip(image, 0, 1) * 255).astype(np.uint8), 'RGB')


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    w = int(sys.argv[4]) if len(sys.argv) > 4 else 960
    h = int(sys.argv[5]) if len(sys.argv) > 5 else 540
    render(sys.argv[1], sys.argv[2], w, h).save(sys.argv[3])
    print('Wrote', sys.argv[3])


if __name__ == '__main__':
    main()
