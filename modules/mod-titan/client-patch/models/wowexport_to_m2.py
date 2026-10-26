#!/usr/bin/env python3
"""Convert a model exported by wow.export (OBJ + JSON + PNG) to a WoW 3.3.5a M2 (version 264).

Rebuilds the mesh, its submeshes and materials (opaque / alpha / additive, unlit, two-sided) on
a single static bone with one Stand animation. Only the first texture of each texture unit is
used. Not carried over: layers with animated color (flashes, pulses; the 3.3.5a client drew them
as solid planes), extra texture layers such as reflections, bone animation, particles and
ribbons (wow.export does not export the last two).

Writes <out dir>/<name>.m2, <name>00.skin and <name>.blp (the main texture).

Usage: python3 wowexport_to_m2.py <export dir> <model base name> <out name> <out dir> <client texture dir>
Example:
  python3 wowexport_to_m2.py veridus/source sword_1h_artifactskywall_d_06 Sword_1H_Veridus out \\
      "Item\\ObjectComponents\\Weapon"
"""

import json
import math
import struct
import sys
from pathlib import Path

from PIL import Image

import blp

M2_VERSION = 264
NONE16 = 0xFFFF
SEQUENCE_IN_MODEL = 0x20


class Blob:
    """Growing byte buffer whose arrays are referenced by (count, offset) pairs."""

    def __init__(self, reserve):
        self.data = bytearray(reserve)

    def add(self, payload, count):
        if not count:
            return 0, 0
        while len(self.data) % 16:
            self.data.append(0)
        offset = len(self.data)
        self.data += payload
        return count, offset

    def array(self, fmt, items):
        return self.add(b''.join(struct.pack('<' + fmt, *(i if isinstance(i, tuple) else (i,))) for i in items),
                        len(items))


def track(blob, interpolation, per_sequence, value_fmt):
    """M2Track: per_sequence is a list (one entry per animation) of (timestamps, values)."""
    stamp_refs = [blob.array('I', ts) for ts, _ in per_sequence]
    value_refs = [blob.array(value_fmt, vs) for _, vs in per_sequence]
    stamps = blob.array('II', stamp_refs)
    values = blob.array('II', value_refs)
    return struct.pack('<HH2I2I', interpolation, NONE16, *stamps, *values)


def empty_track():
    return struct.pack('<HH2I2I', 0, NONE16, 0, 0, 0, 0)


def read_obj(path):
    positions, normals, uvs, groups = [], [], [], []
    for line in Path(path).read_text().splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == 'v':
            x, y, z = map(float, parts[1:4])
            positions.append((x, -z, y))           # OBJ (x, y, z) = M2 (X, Z, -Y)
        elif parts[0] == 'vn':
            x, y, z = map(float, parts[1:4])
            normals.append((x, -z, y))
        elif parts[0] == 'vt':
            u, v = map(float, parts[1:3])
            uvs.append((u, 1.0 - v))                 # OBJ V points up, M2 V points down
        elif parts[0] == 'g':
            groups.append([])
        elif parts[0] == 'f':
            corners = [int(p.split('/')[0]) - 1 for p in parts[1:]]
            groups[-1].append(tuple(corners))
    return positions, normals, uvs, groups


def convert(export_dir, base, name, out_dir, texture_dir):
    export_dir, out_dir = Path(export_dir), Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    meta = json.loads((export_dir / f'{base}.json').read_text())
    positions, normals, uvs, groups = read_obj(export_dir / f'{base}.obj')
    if not len(positions) == len(normals) == len(uvs):
        raise ValueError('expected one normal and UV per vertex (wow.export layout)')

    # Main texture -> BLP
    main_png = export_dir / meta['textures'][0]['fileNameExternal']
    with Image.open(main_png) as image:
        (out_dir / f'{name}.blp').write_bytes(blp.encode(image))
    texture_path = f'{texture_dir}\\{name}.blp'

    # Submeshes: one per OBJ group, in vertex order
    sections = []
    indices = []
    for faces in groups:
        verts = [v for f in faces for v in f]
        first, last = min(verts), max(verts)
        index_start = len(indices)
        for a, b, c in faces:
            indices += [a, b, c]
        sections.append((first, last - first + 1, index_start, len(faces) * 3))

    # Texture units from the export (skinSectionIndex says which submesh each one draws)
    # Texture units whose color pulses or flashes are left out: the 3.3.5a client drew them at full
    # strength all the time (big flat planes) instead of animating them.
    def animated(unit):
        if unit['colorIndex'] >= len(meta['colors']):
            return False
        alpha = meta['colors'][unit['colorIndex']]['alpha']
        values = alpha['values'][0] if alpha['values'] else []
        return any(int(v) != 0x7FFF for v in values)

    batches = []
    dropped = 0
    for unit in meta['skin']['textureUnits']:
        if animated(unit):
            dropped += 1
            continue
        material = meta['materials'][unit['materialIndex']]
        batches.append((material['blendingMode'] != 0, unit))
    batches.sort(key=lambda b: b[0])                # opaque first, then blended layers

    # One looping Stand animation, as long as the longest color animation
    colors = []                                     # only animated layers used colors
    duration = max([max(c['alpha']['timestamps'][0] or [0]) for c in colors if c['alpha']['timestamps']] + [1000])

    lo = [min(p[i] for p in positions) for i in range(3)]
    hi = [max(p[i] for p in positions) for i in range(3)]
    radius = max(math.sqrt(sum(c * c for c in p)) for p in positions)

    blob = Blob(0x130)
    name_ref = blob.add(name.encode() + b'\0', len(name) + 1)
    seq = struct.pack('<HHIfIhHIII6ffhH', 0, 0, duration, 0.0, SEQUENCE_IN_MODEL, 0x7FFF, 0, 0, 0, 150,
                      *lo, *hi, radius, -1, 0)
    sequences = blob.add(seq, 1)
    sequence_lookup = blob.array('H', [0])
    bone = struct.pack('<iIhHHH', -1, 0, -1, 0, 0, 0) + empty_track() * 3 + struct.pack('<3f', 0, 0, 0)
    bones = blob.add(bone, 1)
    vertex_data = b''.join(struct.pack('<3f4B4B3f2f2f', *p, 255, 0, 0, 0, 0, 0, 0, 0, *n, *uv, 0.0, 0.0)
                           for p, n, uv in zip(positions, normals, uvs))
    vertices = blob.add(vertex_data, len(positions))

    color_data = b''
    for c in colors:
        alpha = c['alpha']
        stamps = alpha['timestamps'][0] if alpha['timestamps'] else [0]
        values = alpha['values'][0] if alpha['values'] else [0x7FFF]
        color_data += track(blob, 0, [([0], [(1.0, 1.0, 1.0)])], '3f')
        color_data += track(blob, alpha['interpolation'], [(stamps, [int(v) for v in values])], 'h')
    colors_ref = blob.add(color_data, len(colors))

    texture_name = blob.add(texture_path.encode() + b'\0', len(texture_path) + 1)
    textures = blob.add(struct.pack('<II2I', 0, meta['textures'][0]['flags'], *texture_name), 1)
    weights = blob.add(track(blob, 0, [([0], [0x7FFF])], 'h'), 1)
    replaceable_lookup = blob.array('h', [-1])
    materials = blob.array('HH', [(m['flags'], m['blendingMode']) for m in meta['materials']])
    bone_lookup = blob.array('H', [0])
    texture_lookup = blob.array('H', [0])
    texture_unit_lookup = blob.array('H', [0])
    transparency_lookup = blob.array('H', [0])
    transform_lookup = blob.array('H', [NONE16])

    header = struct.pack('<4sI', b'MD20', M2_VERSION)
    header += struct.pack('<2I', *name_ref) + struct.pack('<I', 0)          # name, global flags
    header += struct.pack('<2I', 0, 0)                                      # global sequences
    header += struct.pack('<2I', *sequences) + struct.pack('<2I', *sequence_lookup)
    header += struct.pack('<2I', *bones) + struct.pack('<2I', 0, 0)         # bones, key bone lookup
    header += struct.pack('<2I', *vertices) + struct.pack('<I', 1)          # vertices, skin profiles
    header += struct.pack('<2I', *colors_ref) + struct.pack('<2I', *textures)
    header += struct.pack('<2I', *weights) + struct.pack('<2I', 0, 0)       # weights, texture transforms
    header += struct.pack('<2I', *replaceable_lookup) + struct.pack('<2I', *materials)
    header += struct.pack('<2I', *bone_lookup) + struct.pack('<2I', *texture_lookup)
    header += struct.pack('<2I', *texture_unit_lookup) + struct.pack('<2I', *transparency_lookup)
    header += struct.pack('<2I', *transform_lookup)
    header += struct.pack('<6ff6ff', *lo, *hi, radius, *lo, *hi, radius)  # bounding / collision boxes
    header += struct.pack('<2I', 0, 0) * 3                                  # collision tris, verts, normals
    header += struct.pack('<2I', 0, 0) * 2                                  # attachments + lookup
    header += struct.pack('<2I', 0, 0) * 2                                  # events, lights
    header += struct.pack('<2I', 0, 0) * 2                                  # cameras + lookup
    header += struct.pack('<2I', 0, 0) * 2                                  # ribbons, particles
    assert len(header) == 0x130
    blob.data[:0x130] = header
    (out_dir / f'{name}.m2').write_bytes(bytes(blob.data))

    # Skin profile
    skin = Blob(48)
    lookup = skin.array('H', list(range(len(positions))))
    tris = skin.array('H', indices)
    props = skin.add(b'\0\0\0\0' * len(positions), len(positions))
    section_data = b''
    for index, (vstart, vcount, istart, icount) in enumerate(sections):
        pts = positions[vstart:vstart + vcount]
        center = tuple(sum(p[i] for p in pts) / len(pts) for i in range(3))
        sort_radius = max(math.dist(center, p) for p in pts)
        section_data += struct.pack('<10H3f3ff', 0, 0, vstart, vcount, istart, icount, 1, 0, 1, 0,
                                    *center, *center, sort_radius)
    sections_ref = skin.add(section_data, len(sections))
    batch_data = b''
    for _, unit in batches:
        color = unit['colorIndex'] if unit['colorIndex'] < len(colors) else NONE16
        batch_data += struct.pack('<Bb11H', 16, unit['priority'], 0, unit['skinSectionIndex'], unit['skinSectionIndex'],
                                  color, unit['materialIndex'], 0, 1, 0, 0, 0, 0)
    batches_ref = skin.add(batch_data, len(batches))
    skin_header = struct.pack('<4s2I2I2I2I2II', b'SKIN', *lookup, *tris, *props, *sections_ref, *batches_ref, 21)
    assert len(skin_header) == 48
    skin.data[:48] = skin_header
    (out_dir / f'{name}00.skin').write_bytes(bytes(skin.data))

    print(f'Wrote {name}.m2 ({len(positions)} vertices, {len(sections)} submeshes, {len(batches)} texture units, '
          f'{dropped} animated layers left out), {name}00.skin and {name}.blp to {out_dir}')


def main():
    if len(sys.argv) != 6:
        sys.exit(__doc__)
    convert(*sys.argv[1:])


if __name__ == '__main__':
    main()
