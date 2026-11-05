#!/usr/bin/env python3
"""Turn a static glTF model exported by wow.export (one mesh, one texture: a helm, a doodad) into the OBJ + JSON
layout wowexport_to_m2.py reads, so it converts the same way.

The glTF's positions are model space (Y up, as wow.export's OBJ). With --pivot they are moved by its skeleton's
root bone pivot, so the model's origin is the point it hangs from: a helm then sits on the head, not a pivot's
height above it, as the 3.3.5a client draws it (a doodad's pivot is anywhere: none). Its V runs down, the OBJ's up.
Every primitive is a submesh with one opaque, lit texture unit.

Usage: python3 gltf_to_wowexport.py <model.gltf> <out dir> <base name> [--pivot]
"""

import json
import struct
import sys
from pathlib import Path

COMPONENTS = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}
FORMATS = {5121: 'B', 5123: 'H', 5125: 'I', 5126: 'f'}


def accessor(gltf, data, index):
    acc = gltf['accessors'][index]
    view = gltf['bufferViews'][acc['bufferView']]
    width = COMPONENTS[acc['type']]
    fmt = FORMATS[acc['componentType']]
    start = view.get('byteOffset', 0) + acc.get('byteOffset', 0)
    values = struct.unpack_from(f'<{acc["count"] * width}{fmt}', data, start)
    return [values[i:i + width] for i in range(0, len(values), width)] if width > 1 else list(values)


def convert(source, out_dir, base, use_pivot=False):
    source, out_dir = Path(source), Path(out_dir)
    gltf = json.loads(source.read_text())
    data = (source.parent / gltf['buffers'][0]['uri']).read_bytes()
    pivot = (0.0, 0.0, 0.0)
    if use_pivot and gltf.get('skins'):
        joint = gltf['skins'][0]['joints'][0]
        parent = next((node for node in gltf['nodes'] if joint in node.get('children', [])), None)
        pivot = tuple((parent or {}).get('translation', pivot))
    lines, units, offset = [], [], 0
    for mesh in gltf['meshes']:
        for primitive in mesh['primitives']:
            attributes = primitive['attributes']
            positions = accessor(gltf, data, attributes['POSITION'])
            normals = accessor(gltf, data, attributes['NORMAL'])
            uvs = accessor(gltf, data, attributes['TEXCOORD_0'])
            indices = accessor(gltf, data, primitive['indices'])
            lines += [f'v {x - pivot[0]} {y - pivot[1]} {z - pivot[2]}' for x, y, z in positions]
            lines += [f'vn {x} {y} {z}' for x, y, z in normals]
            lines += [f'vt {u} {1.0 - v}' for u, v in uvs]
            lines.append(f'g geoset{len(units)}')
            for i in range(0, len(indices), 3):
                a, b, c = (offset + indices[i + k] + 1 for k in range(3))
                lines.append(f'f {a}/{a}/{a} {b}/{b}/{b} {c}/{c}/{c}')
            units.append({'flags': 16, 'priority': 0, 'shaderID': 0, 'skinSectionIndex': len(units), 'colorIndex': 0,
                          'materialIndex': 0, 'textureCount': 1})
            offset += len(positions)
    (out_dir / f'{base}.obj').write_text('\n'.join(lines) + '\n')
    texture = gltf['images'][0]['uri']
    meta = {'textures': [{'fileNameExternal': texture, 'flags': 0}], 'materials': [{'flags': 0, 'blendingMode': 0}],
            'colors': [], 'skin': {'textureUnits': units}}
    (out_dir / f'{base}.json').write_text(json.dumps(meta, indent=1))
    print(f'Wrote {base}.obj ({offset} vertices, {len(units)} submeshes) and {base}.json to {out_dir}')


if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if a != '--pivot']
    if len(args) != 3:
        sys.exit(__doc__)
    convert(*args, use_pivot='--pivot' in sys.argv)
