#!/usr/bin/env python3
"""Keep only some animations of a wow.export glTF, and the buffers they use (one .bin per animation).

A retail character export holds hundreds of animations; the ones kept are listed as <id> (every variation) or
<id>-<variation>. --skeleton also drops the mesh (and its large .bin): enough for retarget_character.py.

Usage: python3 prune_gltf.py <in.gltf> <out dir> [--skeleton] <id>[-<variation>] ...
"""

import json
import re
import shutil
import sys
from pathlib import Path


def prune(source, out, keep, skeleton=False):
    source, out = Path(source), Path(out)
    out.mkdir(parents=True, exist_ok=True)
    g = json.loads(source.read_text())

    def wanted(animation):
        anim_id, variation = map(int, re.search(r'ID (\d+) variation (\d+)', animation['name']).groups())
        return (anim_id, variation) in keep or (anim_id, None) in keep

    g['animations'] = [a for a in g['animations'] if wanted(a)]
    used = set()
    if skeleton:
        g['meshes'] = []
        for node in g['nodes']:
            node.pop('mesh', None)
            node.pop('skin', None)
        for skin in g['skins']:
            skin.pop('inverseBindMatrices', None)
    for mesh in g['meshes']:
        for primitive in mesh['primitives']:
            used |= set(primitive['attributes'].values())
            used.add(primitive['indices'])
    for skin in g['skins']:
        if 'inverseBindMatrices' in skin:
            used.add(skin['inverseBindMatrices'])
    for animation in g['animations']:
        for sampler in animation['samplers']:
            used |= {sampler['input'], sampler['output']}

    accessors = sorted(used)
    accessor_map = {old: new for new, old in enumerate(accessors)}
    views = sorted({g['accessors'][i]['bufferView'] for i in accessors})
    view_map = {old: new for new, old in enumerate(views)}
    buffers = sorted({g['bufferViews'][i]['buffer'] for i in views})
    buffer_map = {old: new for new, old in enumerate(buffers)}
    g['accessors'] = [dict(g['accessors'][i], bufferView=view_map[g['accessors'][i]['bufferView']]) for i in accessors]
    g['bufferViews'] = [dict(g['bufferViews'][i], buffer=buffer_map[g['bufferViews'][i]['buffer']]) for i in views]
    g['buffers'] = [g['buffers'][i] for i in buffers]
    for mesh in g['meshes']:
        for primitive in mesh['primitives']:
            primitive['attributes'] = {k: accessor_map[v] for k, v in primitive['attributes'].items()}
            primitive['indices'] = accessor_map[primitive['indices']]
    for skin in g['skins']:
        if 'inverseBindMatrices' in skin:
            skin['inverseBindMatrices'] = accessor_map[skin['inverseBindMatrices']]
    for animation in g['animations']:
        for sampler in animation['samplers']:
            sampler['input'] = accessor_map[sampler['input']]
            sampler['output'] = accessor_map[sampler['output']]
    (out / source.name).write_text(json.dumps(g))
    for buffer in g['buffers']:
        shutil.copy(source.parent / buffer['uri'], out / buffer['uri'])
    return len(g['animations']), len(g['buffers'])


def main():
    args = sys.argv[1:]
    skeleton = '--skeleton' in args
    args = [a for a in args if a != '--skeleton']
    if len(args) < 3:
        sys.exit(__doc__)
    keep = {tuple(map(int, k.split('-'))) if '-' in k else (int(k), None) for k in args[2:]}
    animations, buffers = prune(args[0], args[1], keep, skeleton)
    print(f'Wrote {args[1]}: {animations} animations, {buffers} buffers')


if __name__ == '__main__':
    main()
