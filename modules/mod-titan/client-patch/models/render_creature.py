#!/usr/bin/env python3
"""Preview an animated WoW 3.3.5a M2 (version 264): pose it at moments of its sequences and software-render it.

Reads the .m2, its 00.skin and the palettized BLPs back from the file layout (no code shared with the
converter): bones and their keyframes, sequences, the skin's bone palettes, texture units and their UV
sets. A wrong pivot, track, quaternion, bone palette or UV set shows up in the picture.

Usage: python3 render_creature.py <model.m2> <texture dir> <out.png> <anim id>[/variation] [frames] [angle] [size]
Renders `frames` moments of the sequence side by side (default 4), seen from `angle` degrees around the model
(0 = from the front).
"""

import math
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image

from render_m2 import read_blp


def arr(data, offset):
    return struct.unpack_from('<II', data, offset)


class Model:
    def __init__(self, m2_path, texture_dir):
        m2 = self.m2 = Path(m2_path).read_bytes()
        skin = Path(m2_path).with_name(Path(m2_path).stem + '00.skin').read_bytes()
        assert m2[:4] == b'MD20' and struct.unpack_from('<I', m2, 4)[0] == 264, 'not a 3.3.5a M2'

        count, offset = arr(m2, 0x1C)
        self.sequences = [struct.unpack_from('<HHIfIhHIII6ffhH', m2, offset + 64 * i) for i in range(count)]
        count, offset = arr(m2, 0x2C)
        self.bones = []
        for i in range(count):
            base = offset + 88 * i
            _, _, parent = struct.unpack_from('<iIh', m2, base)
            tracks = [self.track(base + 16 + 20 * k, fmt) for k, fmt in enumerate(('3f', '4h', '3f'))]
            pivot = np.array(struct.unpack_from('<3f', m2, base + 76))
            self.bones.append((parent, tracks, pivot))
        count, offset = arr(m2, 0x3C)
        self.verts = np.frombuffer(m2, np.dtype([('pos', '<f4', 3), ('w', 'u1', 4), ('b', 'u1', 4), ('n', '<f4', 3),
                                                 ('uv', '<f4', 2), ('uv2', '<f4', 2)]), count, offset)
        self.textures = []
        count, offset = arr(m2, 0x50)
        for i in range(count):
            _, _, n_len, n_off = struct.unpack_from('<IIII', m2, offset + i * 16)
            name = m2[n_off:n_off + n_len - 1].decode().split('\\')[-1]
            self.textures.append(read_blp(Path(texture_dir) / name))
        count, offset = arr(m2, 0x70)
        self.materials = [struct.unpack_from('<HH', m2, offset + i * 4) for i in range(count)]
        count, offset = arr(m2, 0x78)
        self.combos = struct.unpack_from(f'<{count}H', m2, offset)
        count, offset = arr(m2, 0x80)
        self.texture_lookup = struct.unpack_from(f'<{count}H', m2, offset)
        count, offset = arr(m2, 0x88)
        self.uv_lookup = struct.unpack_from(f'<{count}h', m2, offset)

        count, offset = arr(skin, 0x04)
        self.lookup = np.frombuffer(skin, '<u2', count, offset).astype(np.int64)
        count, offset = arr(skin, 0x0C)
        self.indices = np.frombuffer(skin, '<u2', count, offset).astype(np.int64)
        count, offset = arr(skin, 0x14)
        self.props = np.frombuffer(skin, 'u1', count * 4, offset).reshape(count, 4).astype(np.int64)
        count, offset = arr(skin, 0x1C)
        self.sections = [struct.unpack_from('<10H', skin, offset + i * 48) for i in range(count)]
        count, offset = arr(skin, 0x24)
        self.batches = [struct.unpack_from('<Bb11H', skin, offset + i * 24) for i in range(count)]

    def track(self, offset, fmt):
        interpolation, global_seq, n_stamps, o_stamps, n_values, o_values = struct.unpack_from('<Hh4I', self.m2, offset)
        per_sequence = []
        size = struct.calcsize('<' + fmt)
        for s in range(n_stamps):
            count, at = struct.unpack_from('<II', self.m2, o_stamps + 8 * s)
            times = np.array(struct.unpack_from(f'<{count}I', self.m2, at))
            count, at = struct.unpack_from('<II', self.m2, o_values + 8 * s)
            values = np.array([struct.unpack_from('<' + fmt, self.m2, at + size * k) for k in range(count)], float)
            if fmt == '4h' and count:
                values = np.where(values < 0, values + 32768, values - 32767) / 32767
            per_sequence.append((times, values))
        return per_sequence

    def sequence(self, anim_id, variation=0):
        for index, s in enumerate(self.sequences):
            if s[0] == anim_id and s[1] == variation:
                return index
        raise KeyError(f'no sequence {anim_id}/{variation}')

    def bone_matrices(self, sequence, t):
        mats = []
        for parent, tracks, pivot in self.bones:
            local = np.eye(4)
            values = []
            for times, vals in (tr[sequence] if sequence < len(tr) else ([], []) for tr in tracks):
                if len(times) == 0:
                    values.append(None)
                    continue
                if t <= times[0] or len(times) == 1:
                    values.append(vals[0])
                elif t >= times[-1]:
                    values.append(vals[-1])
                else:
                    i = int(np.searchsorted(times, t, side='right')) - 1
                    f = (t - times[i]) / (times[i + 1] - times[i])
                    a, b = vals[i], vals[i + 1]
                    if len(a) == 4 and np.dot(a, b) < 0:
                        b = -b
                    values.append(a + (b - a) * f)
            translation, rotation, scale = values
            if rotation is not None:
                x, y, z, w = rotation / np.linalg.norm(rotation)
                local[:3, :3] = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                                 [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                                 [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
            if scale is not None:
                local[:3, :3] *= scale
            if translation is not None:
                local[:3, 3] = translation
            to_pivot, back = np.eye(4), np.eye(4)
            to_pivot[:3, 3], back[:3, 3] = pivot, -pivot
            m = to_pivot @ local @ back
            mats.append(mats[parent] @ m if parent >= 0 else m)
        return np.array(mats)

    def skinned(self, sequence, t):
        """Skinned positions and normals per skin vertex, through the skin's bone palettes."""
        mats = self.bone_matrices(sequence, t)
        pos = np.zeros((len(self.lookup), 3))
        nrm = np.zeros((len(self.lookup), 3))
        for section in self.sections:
            vstart, vcount, combo_start = section[2], section[3], section[7]
            rows = np.arange(vstart, vstart + vcount)
            v = self.verts[self.lookup[rows]]
            bones = np.array(self.combos)[combo_start + self.props[rows]]
            w = v['w'].astype(float) / 255
            p = np.append(v['pos'], np.ones((vcount, 1)), axis=1)
            for k in range(4):
                m = mats[bones[:, k]]
                pos[rows] += w[:, k:k + 1] * np.einsum('nij,nj->ni', m, p)[:, :3]
                nrm[rows] += w[:, k:k + 1] * np.einsum('nij,nj->ni', m[:, :3, :3], v['n'])
        return pos, nrm


def render_frame(model, pos, nrm, angle, size, view):
    a = math.radians(angle)
    right = np.array([-math.sin(a), math.cos(a), 0])
    toward = np.array([math.cos(a), math.sin(a), 0])
    sx, sy, depth = pos @ right, pos[:, 2], pos @ toward
    center, span = view
    px = (sx - center[0]) / span * size + size / 2
    py = size / 2 - (sy - center[1]) / span * size
    lnorm = nrm / np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-6)
    light = np.clip(lnorm @ (toward * 0.6 + np.array([0, 0, 0.8])) / 1.0, 0.25, 1)

    image = np.zeros((size, size, 3))
    image[:] = (0.16, 0.17, 0.2)
    zbuf = np.full((size, size), -np.inf)
    order = sorted(model.batches, key=lambda b: model.materials[b[6]][1] > 1)
    for batch in order:
        section = model.sections[batch[3]]
        flags, blend = model.materials[batch[6]]
        tex = model.textures[model.texture_lookup[batch[9]]]
        uv = model.verts['uv2' if model.uv_lookup[batch[10]] == 1 else 'uv'][model.lookup]
        th, tw = tex.shape[:2]
        istart, icount = section[4], section[5]
        for t in range(istart, istart + icount, 3):
            ids = model.indices[t:t + 3]
            x, y = px[ids], py[ids]
            area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0])
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
            shade = (w0 * light[ids[0]] + w1 * light[ids[1]] + w2 * light[ids[2]])[..., None]
            region = (slice(y0, y1 + 1), slice(x0, x1 + 1))
            if blend in (0, 1):
                visible = inside & (z > zbuf[region])
                if blend == 1:
                    visible &= texel[..., 3] > 0.5
                zbuf[region][visible] = z[visible]
                image[region][visible] = (texel[..., :3] * (1 if flags & 1 else shade))[visible]
            else:
                visible = inside & (z >= zbuf[region] - 1e-3)
                add = texel[..., :3] * texel[..., 3:]
                if blend == 2:
                    image[region][visible] = (image[region] * (1 - texel[..., 3:]) + add)[visible]
                else:
                    image[region][visible] = np.clip(image[region] + add, 0, 1)[visible]
    return Image.fromarray((np.clip(image, 0, 1) * 255).astype(np.uint8))


def render(m2_path, texture_dir, anim, frames=4, angle=20.0, size=384):
    model = Model(m2_path, texture_dir)
    anim_id, _, variation = anim.partition('/')
    sequence = model.sequence(int(anim_id), int(variation or 0))
    duration = model.sequences[sequence][2]
    times = [duration * i / max(frames - 1, 1) for i in range(frames)] if frames > 1 else [0]
    posed = [model.skinned(sequence, t) for t in times]
    a = math.radians(angle)
    right = np.array([-math.sin(a), math.cos(a), 0])
    every = np.concatenate([p for p, _ in posed])
    sx, sy = every @ right, every[:, 2]
    view = ((sx.min() + sx.max()) / 2, (sy.min() + sy.max()) / 2), max(np.ptp(sx), np.ptp(sy)) * 1.08
    sheet = Image.new('RGB', (size * len(times), size))
    for i, (pos, nrm) in enumerate(posed):
        sheet.paste(render_frame(model, pos, nrm, angle, size * 2, view).resize((size, size), Image.LANCZOS),
                    (size * i, 0))
    return sheet


def main():
    if len(sys.argv) < 5:
        sys.exit(__doc__)
    frames = int(sys.argv[5]) if len(sys.argv) > 5 else 4
    angle = float(sys.argv[6]) if len(sys.argv) > 6 else 20.0
    size = int(sys.argv[7]) if len(sys.argv) > 7 else 384
    render(sys.argv[1], sys.argv[2], sys.argv[4], frames, angle, size).save(sys.argv[3])
    print(f'Wrote {sys.argv[3]}')


if __name__ == '__main__':
    main()
