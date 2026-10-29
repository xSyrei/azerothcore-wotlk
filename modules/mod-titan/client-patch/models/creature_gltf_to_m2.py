#!/usr/bin/env python3
"""Convert an animated creature exported by wow.export (glTF 2.0 + its JSON) to a WoW 3.3.5a M2 (version 264).

wow.export writes each M2 bone as a pivot node (`bone_X_p`, translation = pivot - parent pivot) with the
animated joint node under it (`bone_X`), and the joint's translation / rotation / scale channels are the
M2 bone tracks themselves (coordinates swapped to glTF's Y-up). So the bones and their keyframes are
carried over one to one: no resampling, only the axis swap back.

Carried over: the skinned mesh (split into submeshes of at most BONES_PER_SECTION bones), every bone and its
animation, the sequences listed in ANIMATIONS (retail IDs above 505 moved to 3.3.5a ones), texture units
(first texture only; a fel scroll layer keeps its tattoo mask, drawn additive on the second UV set),
key bones and attachments (weapons, spell hands, breath, name plate) placed on the retail helper bones.
Not carried over: particles, ribbons, events, cameras, bones on global sequences (wow.export drops them).

Writes <out dir>/<name>.m2, <name>00.skin and one BLP per texture (<name>_<texture>.blp).

Usage: python3 creature_gltf_to_m2.py <glTF export dir> <wow.export json> <name> <out dir> <client dir>
"""

import json
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image

import blp
from spell_effects import comp_quat
from wowexport_to_m2 import Blob

M2_VERSION = 264
NONE16 = 0xFFFF
SEQUENCE_IN_MODEL = 0x20
BONE_TRANSFORMED = 0x200
BONES_PER_SECTION = 53
LOOKUP_SIZE = 506                       # one entry per 3.3.5a AnimationData row

# 3.3.5a key bones (KeyBoneId -> wow.export bone name)
KEY_BONES = ['ArmL', 'ArmR', 'ShoulderL', 'ShoulderR', 'SpineLow', 'Waist', 'Head', 'Jaw',
             'IndexFingerR', 'MiddleFingerR', 'PinkyFingerR', 'RingFingerR', 'ThumbR',
             'IndexFingerL', 'MiddleFingerL', 'PinkyFingerL', 'RingFingerL', 'ThumbL',
             '$BTH', '$CSR', '$CSL', '_Breath', '_Name', '_NameMount', '$CHD', '$CCH', 'Root']

# Attachment id -> (bone, offset from its pivot in M2 axes: +X forward, +Z up)
ATTACHMENTS = {
    0: ('WeaponL', (0, 0, 0)),          # shield
    1: ('WeaponR', (0, 0, 0)),          # main hand
    2: ('WeaponL', (0, 0, 0)),          # off hand
    5: ('ShoulderR', (0, 0, 0)),
    6: ('ShoulderL', (0, 0, 0)),
    11: ('Head', (0, 0, 0.15)),         # helm
    12: ('Chest', (-0.35, 0, 0)),       # back
    15: ('Chest', (0.3, 0, 0)),         # chest, front
    16: ('Chest', (-0.3, 0, 0)),        # chest, back
    17: ('_Breath', (0, 0, 0)),
    18: ('_Name', (0, 0, 0)),
    19: ('Main', (0, 0, 0)),            # base
    20: ('Head', (0, 0, 0)),
    21: ('SpellHandL', (0, 0, 0)),
    22: ('SpellHandR', (0, 0, 0)),
    23: ('Chest', (0, 0, 0)),
    24: ('Chest', (0, 0, 0)),
    25: ('Chest', (0, 0, 0)),
    26: ('Chest', (-0.35, 0, 0.1)),     # sheathed main hand
    27: ('Chest', (-0.35, 0, 0.1)),     # sheathed off hand
}

# 3.3.5a sequences: (animation id, source animations as (retail id, variation), ...). Several sources make
# variations; a tuple of sources plays them one after the other. A source missing from the export is skipped;
# a sequence with none left is left out.
ANIMATIONS = [
    (0, [(0, 0)]),                      # Stand
    (1, [(1, 0)]),                      # Death
    (4, [(4, 0)]),                      # Walk
    (5, [(5, 0)]),                      # Run
    (8, [(8, 0)]),                      # StandWound
    (10, [(10, 0)]),                    # CombatCritical
    (11, [(11, 0)]),                    # ShuffleLeft
    (12, [(12, 0)]),                    # ShuffleRight
    (13, [(13, 0)]),                    # Walkbackwards
    (14, [(14, 0)]),                    # Stun
    (15, [(15, 0)]),                    # HandsClosed
    (16, [(16, 0), (16, 1)]),           # AttackUnarmed
    (17, [(17, 0), (87, 0)]),           # Attack1H (the male export has only AttackOff)
    (18, [(17, 0), (87, 0)]),           # Attack2H
    (19, [(17, 0), (87, 0)]),           # Attack2HL
    (20, [(20, 0)]),                    # ParryUnarmed
    (21, [(21, 0)]),                    # Parry1H
    (22, [(21, 0)]),                    # Parry2H
    (23, [(21, 0)]),                    # Parry2HL
    (25, [(25, 0)]),                    # ReadyUnarmed
    (26, [(26, 0)]),                    # Ready1H
    (27, [(26, 0)]),                    # Ready2H
    (28, [(26, 0)]),                    # Ready2HL
    (30, [(30, 0)]),                    # Dodge
    (2, [(54, 0)]),                     # Spell
    (31, [(52, 0)]),                    # SpellPrecast
    (32, [(54, 0)]),                    # SpellCast
    (33, [(54, 0)]),                    # SpellCastArea
    (37, [(37, 0)]),                    # JumpStart
    (38, [(38, 0)]),                    # Jump
    (39, [(39, 0)]),                    # JumpEnd
    (40, [(40, 0)]),                    # Fall
    (41, [(41, 0)]),                    # SwimIdle
    (42, [(42, 0)]),                    # Swim
    (43, [(43, 0)]),                    # SwimLeft
    (44, [(44, 0)]),                    # SwimRight
    (45, [(45, 0)]),                    # SwimBackwards
    (51, [(51, 0)]),                    # ReadySpellDirected
    (52, [(52, 0)]),                    # ReadySpellOmni
    (53, [(53, 0)]),                    # SpellCastDirected
    (54, [(54, 0)]),                    # SpellCastOmni
    (55, [(55, 0)]),                    # BattleRoar
    (57, [(57, 0)]),                    # Special1H
    (87, [(87, 0), (16, 1)]),           # AttackOff
    (117, [(16, 1)]),                   # AttackUnarmedOff
    (107, [(1008, 0)]),                 # AttackThrown <- GlvThrowMain (Throw Glaive)
    (124, [(124, 0)]),                  # ChannelCastDirected
    (125, [(125, 0)]),                  # ChannelCastOmni
    (143, [(143, 0)]),                  # Sprint
    (187, [(187, 0)]),                  # JumpLandRun
    # The Demon Hunter's ability animations, in the slots the character models use too
    # (retarget_character.py), so one spell visual plays them on both:
    (215, [((1056, v), (1058, v), (1060, v)) for v in range(3)]),  # CustomSpell03 <- DHBladeDance 1, 2, 3
    (216, [(1042, 0)]),                 # CustomSpell04 <- FelRushLoop
    (217, [(1038, 0)]),                 # CustomSpell05 <- BackFlipStart (Vengeful Retreat)
    (218, [(1040, 0)]),                 # CustomSpell06 <- BackFlipLoop
    (219, [(1008, 0)]),                 # CustomSpell07 <- GlvThrowMain (Throw Glaive)
    (220, [(16, 0), (16, 1)]),          # CustomSpell08: Demon's Bite (the export has no glaive attacks)
    (221, [(17, 0), (87, 0)]),          # CustomSpell09: Chaos Strike
]
DEAD = (6, 1)                           # Dead: the last frame of Death, held
# Fraction of a cycle a foot is planted, to turn foot travel into the speed a movement animation matches.
# Strafing (ShuffleLeft / Right) moves sideways at walking speed.
MOVING = {4: 0.6, 5: 0.4, 13: 0.6, 143: 0.35}
SAME_SPEED = {11: 4, 12: 4}
# How far the mesh reaches past its bones (wing tips, horns), for the sequence bounds.
MESH_MARGIN = 2.5


# glTF (Y up) -> M2 (Z up): wow.export wrote M2 (x, y, z) as (x, z, -y).
def to_m2(v):
    v = np.asarray(v, np.float64)
    return np.stack([v[..., 0], -v[..., 2], v[..., 1]], axis=-1)


def quat_to_m2(q):
    q = np.asarray(q, np.float64)
    return np.stack([q[..., 0], -q[..., 2], q[..., 1], q[..., 3]], axis=-1)


def scale_to_m2(s):
    s = np.asarray(s, np.float64)
    return np.stack([s[..., 0], s[..., 2], s[..., 1]], axis=-1)


class Export:
    """The glTF as numpy arrays in M2 axes."""

    def __init__(self, export_dir):
        self.dir = Path(export_dir)
        gltf_path = next(self.dir.glob('*.gltf'))
        self.gltf = json.loads(gltf_path.read_text())
        self._buffers = {}
        g = self.gltf
        nodes = g['nodes']
        joints = g['skins'][0]['joints']
        joint_index = {node: i for i, node in enumerate(joints)}
        parent = {c: i for i, n in enumerate(nodes) for c in n.get('children', [])}

        self.names = [nodes[j]['name'].removeprefix('bone_') for j in joints]
        self.parents = []
        for j in joints:
            prefix = parent.get(j)                          # bone_X_p
            above = parent.get(prefix) if prefix is not None else None
            self.parents.append(joint_index.get(above, -1))
        ibm = self.accessor(g['skins'][0]['inverseBindMatrices']).reshape(-1, 4, 4)
        self.pivots = to_m2(-ibm[:, 3, :3])                # glTF matrices are column major

        # One vertex pool; each mesh draws a contiguous part of it.
        self.meshes = []
        attributes = None
        for node in nodes:
            if 'mesh' not in node:
                continue
            primitive = g['meshes'][node['mesh']]['primitives'][0]
            attributes = primitive['attributes']
            self.meshes.append((g['materials'][primitive['material']]['name'],
                                self.accessor(primitive['indices']).ravel().astype(np.int64)))
        self.positions = to_m2(self.accessor(attributes['POSITION']))
        self.normals = to_m2(self.accessor(attributes['NORMAL']))
        self.uv0 = self.accessor(attributes['TEXCOORD_0']).astype(np.float64)
        self.uv1 = self.accessor(attributes['TEXCOORD_1']).astype(np.float64)
        self.bone_ids = self.accessor(attributes['JOINTS_0']).astype(np.int64)
        self.weights = self.accessor(attributes['WEIGHTS_0']).astype(np.int64)

        # Animations: {(id, variation): (duration ms, {(joint, path): (times ms, values)})}
        self.animations = {}
        convert = {'translation': to_m2, 'rotation': quat_to_m2, 'scale': scale_to_m2}
        for anim in g['animations']:
            head = anim['name'].rsplit('(ID ', 1)[1].rstrip(')').split(' variation ')
            key = (int(head[0]), int(head[1]))
            channels = {}
            duration = 0
            for channel in anim['channels']:
                sampler = anim['samplers'][channel['sampler']]
                times = np.rint(self.accessor(sampler['input']).ravel() * 1000).astype(np.int64)
                path = channel['target']['path']
                channels[(joint_index[channel['target']['node']], path)] = (
                    times, convert[path](self.accessor(sampler['output'])))
                duration = max(duration, int(round(g['accessors'][sampler['input']]['max'][0] * 1000)))
            self.animations[key] = (max(duration, 1), channels)

    def concatenate(self, parts):
        """A new animation playing `parts` one after the other (1 ms apart). A bone a part does not animate
        keeps its rest pose through that part."""
        key = tuple(parts)
        if key in self.animations:
            return key
        rest = {'translation': np.zeros(3), 'rotation': np.array([0.0, 0.0, 0.0, 1.0]), 'scale': np.ones(3)}
        tracks = set().union(*(self.animations[part][1] for part in parts))
        pieces = {track: ([], []) for track in tracks}
        offset = 0
        for part in parts:
            duration, channels = self.animations[part]
            for track in tracks:
                times, values = channels.get(track, (np.array([0, duration]), np.array([rest[track[1]]] * 2)))
                pieces[track][0].append(times + offset)
                pieces[track][1].append(values)
            offset += duration + 1
        self.animations[key] = (offset - 1, {track: (np.concatenate(t), np.concatenate(v))
                                             for track, (t, v) in pieces.items()})
        return key

    def accessor(self, index):
        g = self.gltf
        a = g['accessors'][index]
        view = g['bufferViews'][a['bufferView']]
        uri = g['buffers'][view['buffer']]['uri']
        if uri not in self._buffers:
            self._buffers[uri] = (self.dir / uri).read_bytes()
        width = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}[a['type']]
        dtype = {5126: np.float32, 5123: np.uint16, 5125: np.uint32, 5121: np.uint8}[a['componentType']]
        offset = view.get('byteOffset', 0) + a.get('byteOffset', 0)
        return np.frombuffer(self._buffers[uri], dtype, a['count'] * width, offset).reshape(a['count'], width)


def sample(times, values, t, rotation=False):
    """Linear interpolation of one track at time t (ms); quaternions by normalized lerp."""
    if t <= times[0]:
        return values[0]
    if t >= times[-1]:
        return values[-1]
    i = int(np.searchsorted(times, t, side='right')) - 1
    f = (t - times[i]) / max(times[i + 1] - times[i], 1)
    a, b = values[i], values[i + 1]
    if rotation:
        if np.dot(a, b) < 0:
            b = -b
        q = a * (1 - f) + b * f
        return q / np.linalg.norm(q)
    return a * (1 - f) + b * f


def quat_matrix(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def pose(export, key, t):
    """Model-space 4x4 bone matrices of animation `key` at time t, as the client builds them:
    parent * T(pivot) * T(translation) * R * S * T(-pivot)."""
    _, channels = export.animations[key]
    mats = np.zeros((len(export.names), 4, 4))
    for b in range(len(export.names)):
        m = np.eye(4)
        tr = channels.get((b, 'translation'))
        rot = channels.get((b, 'rotation'))
        sc = channels.get((b, 'scale'))
        local = np.eye(4)
        if rot is not None:
            local[:3, :3] = quat_matrix(sample(*rot, t, rotation=True))
        if sc is not None:
            local[:3, :3] = local[:3, :3] * sample(*sc, t)
        if tr is not None:
            local[:3, 3] = sample(*tr, t)
        pivot = export.pivots[b]
        m[:3, 3] = pivot
        m = m @ local
        back = np.eye(4)
        back[:3, 3] = -pivot
        m = m @ back
        p = export.parents[b]
        mats[b] = (mats[p] @ m) if p >= 0 else m           # parents come before children
    return mats


def move_speed(export, key, planted):
    """Yards per second a walk / run cycle covers, from how far its feet slide back while planted."""
    duration, _ = export.animations[key]
    feet = [export.names.index(n) for n in ('FootL', 'FootR') if n in export.names]
    if not feet:
        return 0.0
    travel = []
    for foot in feet:
        xs = [(pose(export, key, t)[foot] @ np.append(export.pivots[foot], 1))[0]
              for t in np.linspace(0, duration, 24)]
        travel.append(max(xs) - min(xs))
    return float(max(travel) / (planted * duration / 1000))


def convert(export_dir, json_path, name, out_dir, client_dir):
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    export = Export(export_dir)
    meta = json.loads(Path(json_path).read_text())
    texture_files = [Path(t['fileNameInternal']).stem for t in meta['textures']]
    bone_count = len(export.names)
    if bone_count > 255:
        raise ValueError(f'{bone_count} bones: M2 vertices index at most 256')

    # Texture units: first texture of each; a scrolling fel layer keeps its tattoo mask on UV set 2.
    units = []
    for unit in meta['skin']['textureUnits']:
        combo = meta['textureCombos'][unit['textureComboIndex']:unit['textureComboIndex'] + unit['textureCount']]
        texture, uv_set = texture_files[combo[0]], 0
        if 'scrollfx' in texture and len(combo) > 1:
            texture, uv_set = texture_files[combo[1]], 1
        units.append((unit['skinSectionIndex'], texture, unit['materialIndex'], uv_set, unit['priority']))
    used_textures = sorted({u[1] for u in units}, key=lambda t: texture_files.index(t))
    for texture in used_textures:
        with Image.open(Path(export_dir) / 'textures' / f'{texture}.png') as image:
            (out_dir / f'{name}_{texture}.blp').write_bytes(blp.encode(image))
    texture_paths = [f'{client_dir}\\{name}_{t}.blp' for t in used_textures]

    # Vertices: the used part of the pool, in mesh order.
    used = np.unique(np.concatenate([idx for _, idx in export.meshes]))
    remap = np.full(len(export.positions), -1, np.int64)
    remap[used] = np.arange(len(used))
    positions, normals = export.positions[used], export.normals[used]
    uv0, uv1 = export.uv0[used], export.uv1[used]
    bone_ids, weights = export.bone_ids[used], export.weights[used].copy()
    weights[weights.sum(axis=1) == 0, 0] = 255
    fix = weights.sum(axis=1) != 255                       # make weights add up to 255 exactly
    weights[fix, np.argmax(weights[fix], axis=1)] += 255 - weights[fix].sum(axis=1)
    bone_ids[weights == 0] = 0

    # Submeshes: each mesh split so no part uses more than BONES_PER_SECTION bones.
    sections = []                                          # (mesh, skin vertices, triangles, palette)
    for mesh_index, (_, idx) in enumerate(export.meshes):
        tris = remap[idx].reshape(-1, 3)
        current, palette = [], set()
        for tri in tris:
            need = {int(b) for v in tri for b, w in zip(bone_ids[v], weights[v]) if w}
            if len(palette | need) > BONES_PER_SECTION:
                sections.append((mesh_index, current, sorted(palette)))
                current, palette = [], set()
            current.append(tri)
            palette |= need
        if current:
            sections.append((mesh_index, current, sorted(palette)))

    skin_vertices, skin_props, skin_indices, bone_combos, section_records, mesh_sections = [], [], [], [], [], {}
    for mesh_index, tris, palette in sections:
        local = {b: i for i, b in enumerate(palette)}
        order = sorted({int(v) for tri in tris for v in tri})
        vstart = len(skin_vertices)
        position_of = {v: vstart + i for i, v in enumerate(order)}
        skin_vertices += order
        skin_props += [tuple(local[int(b)] if w else 0 for b, w in zip(bone_ids[v], weights[v])) for v in order]
        istart = len(skin_indices)
        skin_indices += [position_of[int(v)] for tri in tris for v in tri]
        influence = max(int((weights[v] > 0).sum()) for v in order)
        pts = positions[order]
        center = pts.mean(axis=0)
        per_bone = np.zeros(bone_count)
        np.add.at(per_bone, bone_ids[order].ravel(), weights[order].ravel())
        section_records.append(struct.pack('<10H3f3ff', 0, 0, vstart, len(order), istart,
                                           len(tris) * 3, len(palette), len(bone_combos), influence,
                                           int(per_bone.argmax()), *center, *center,
                                           float(np.linalg.norm(pts - center, axis=1).max())))
        mesh_sections.setdefault(mesh_index, []).append(len(section_records) - 1)
        bone_combos += palette
    if len(skin_vertices) > 0xFFFF or len(skin_indices) > 0xFFFF:
        raise ValueError(f'{len(skin_vertices)} skin vertices / {len(skin_indices)} indices: over 65535')

    # Sequences
    sequences = []                                         # (id, source key or DEAD, variation)
    for anim_id, sources in ANIMATIONS:
        found = [export.concatenate(s) if isinstance(s[0], tuple) else s for s in sources
                 if (all(p in export.animations for p in s) if isinstance(s[0], tuple) else s in export.animations)]
        for variation, source in enumerate(found):
            sequences.append((anim_id, source, variation))
    if (1, 0) in export.animations:
        sequences.append((DEAD[0], 'dead', 0))
    sequences.sort(key=lambda s: (s[0], s[2]))

    blob = Blob(0x130)
    name_ref = blob.add(name.encode() + b'\0', len(name) + 1)

    # Bone tracks: one key array per source animation, shared by every sequence made from it.
    def channel(key, bone, path):
        if key == 'dead':
            times, values = export.animations[(1, 0)][1].get((bone, path), (None, None))
            return None if times is None else (np.array([0]), values[-1:])
        return export.animations[key][1].get((bone, path))

    value_format = {'translation': '3f', 'rotation': '4h', 'scale': '3f'}
    cache = {}

    def key_arrays(key, bone, path):
        if (key, bone, path) not in cache:
            data = channel(key, bone, path)
            if data is None:
                cache[(key, bone, path)] = ((0, 0), (0, 0))
            else:
                times, values = data
                if path == 'rotation':
                    values = [tuple(comp_quat(c) for c in q / np.linalg.norm(q)) for q in values]
                else:
                    values = [tuple(float(c) for c in v) for v in values]
                cache[(key, bone, path)] = (blob.array('I', [int(t) for t in times]),
                                            blob.array(value_format[path], values))
        return cache[(key, bone, path)]

    bone_data = b''
    animated = []
    for b in range(bone_count):
        tracks = b''
        moving = False
        for path in ('translation', 'rotation', 'scale'):
            refs = [key_arrays(source, b, path) for _, source, _ in sequences]
            moving |= any(r[0][0] for r in refs)
            stamps = blob.array('II', [r[0] for r in refs])
            values = blob.array('II', [r[1] for r in refs])
            tracks += struct.pack('<HH2I2I', 1, NONE16, *stamps, *values)
        animated.append(moving)
        key_bone = KEY_BONES.index(export.names[b]) if export.names[b] in KEY_BONES else -1
        bone_data += struct.pack('<iIhHHH', key_bone, BONE_TRANSFORMED if moving else 0, export.parents[b], 0, 0, 0)
        bone_data += tracks + struct.pack('<3f', *export.pivots[b])
    bones_ref = blob.add(bone_data, bone_count)
    key_lookup = [export.names.index(n) if n in export.names else -1 for n in KEY_BONES]
    key_lookup_ref = blob.array('h', key_lookup)

    # Bounds: the mesh at rest, and per sequence every bone through a few of its frames plus MESH_MARGIN
    # (Blade Dance carries the model several yards from where it stands).
    reach = float(np.linalg.norm(positions, axis=1).max())
    lo = positions.min(axis=0)
    hi = positions.max(axis=0)
    speeds = {anim_id: move_speed(export, (anim_id, 0), planted)
              for anim_id, planted in MOVING.items() if (anim_id, 0) in export.animations}
    speeds.update({anim_id: speeds[like] for anim_id, like in SAME_SPEED.items() if like in speeds})
    homogeneous = np.append(export.pivots, np.ones((bone_count, 1)), axis=1)
    sequence_data = b''
    first_of = {}
    for index, (anim_id, source, variation) in enumerate(sequences):
        first_of.setdefault(anim_id, index)
        key = (1, 0) if source == 'dead' else source
        duration = 1000 if source == 'dead' else export.animations[key][0]
        speed = speeds.get(anim_id, 0.0) if source == (anim_id, 0) or anim_id in SAME_SPEED else 0.0
        points = np.concatenate([np.einsum('bij,bj->bi', pose(export, key, t), homogeneous)[:, :3]
                                 for t in np.linspace(0, duration, 6)] + [positions])
        variations = [s for s in sequences if s[0] == anim_id]
        following = [i for i, s in enumerate(sequences) if s[0] == anim_id and s[2] == variation + 1]
        frequency = 0x7FFF // len(variations)
        seq_lo = points.min(axis=0) - MESH_MARGIN
        seq_hi = points.max(axis=0) + MESH_MARGIN
        seq_radius = float(np.linalg.norm(points, axis=1).max()) + MESH_MARGIN
        sequence_data += struct.pack('<HHIfIhHIII3f3ffhH', anim_id, variation, duration, speed, SEQUENCE_IN_MODEL,
                                     frequency, 0, 0, 0, 150, *seq_lo, *seq_hi, seq_radius,
                                     following[0] if following else -1, index)
    sequences_ref = blob.add(sequence_data, len(sequences))
    lookup = [NONE16] * LOOKUP_SIZE
    for anim_id, index in first_of.items():
        lookup[anim_id] = index
    lookup_ref = blob.array('H', lookup)

    vertex_data = b''.join(struct.pack('<3f4B4B3f2f2f', *positions[i], *weights[i], *bone_ids[i], *normals[i],
                                       *uv0[i], *uv1[i]) for i in range(len(positions)))
    vertices_ref = blob.add(vertex_data, len(positions))

    texture_data = b''
    for path in texture_paths:
        ref = blob.add(path.encode() + b'\0', len(path) + 1)
        texture_data += struct.pack('<II2I', 0, 0, *ref)
    textures_ref = blob.add(texture_data, len(texture_paths))
    weights_ref = blob.add(struct.pack('<HH2I2I', 0, NONE16, *blob.array('II', [blob.array('I', [0])]),
                                       *blob.array('II', [blob.array('h', [0x7FFF])])), 1)
    replaceable_ref = blob.array('h', [-1])
    materials_ref = blob.array('HH', [(m['flags'], m['blendingMode']) for m in meta['materials']])
    combos_ref = blob.array('H', bone_combos)
    texture_lookup_ref = blob.array('H', list(range(len(texture_paths))))
    uv_lookup_ref = blob.array('h', [0, 1])                # texture coordinate combos: UV set 1, UV set 2
    transparency_lookup_ref = blob.array('H', [0])
    transform_lookup_ref = blob.array('H', [NONE16])

    attachment_data = b''
    attachment_ids = []
    for attachment_id, (bone_name, offset) in sorted(ATTACHMENTS.items()):
        if bone_name not in export.names:
            continue
        bone = export.names.index(bone_name)
        position = export.pivots[bone] + np.array(offset)
        visible = struct.pack('<HH2I2I', 0, NONE16, *blob.array('II', [blob.array('I', [0])]),
                              *blob.array('II', [blob.array('B', [1])]))
        attachment_data += struct.pack('<IHH3f', attachment_id, bone, 0, *position) + visible
        attachment_ids.append(attachment_id)
    attachments_ref = blob.add(attachment_data, len(attachment_ids))
    attachment_lookup = [-1] * (max(attachment_ids) + 1)
    for index, attachment_id in enumerate(attachment_ids):
        attachment_lookup[attachment_id] = index
    attachment_lookup_ref = blob.array('h', attachment_lookup)

    collision_lo = meta['collisionBox']['min']
    collision_hi = meta['collisionBox']['max']
    header = struct.pack('<4sI', b'MD20', M2_VERSION)
    header += struct.pack('<2I', *name_ref) + struct.pack('<I', 0)          # name, global flags
    header += struct.pack('<2I', 0, 0)                                      # global sequences
    header += struct.pack('<2I', *sequences_ref) + struct.pack('<2I', *lookup_ref)
    header += struct.pack('<2I', *bones_ref) + struct.pack('<2I', *key_lookup_ref)
    header += struct.pack('<2I', *vertices_ref) + struct.pack('<I', 1)      # vertices, skin profiles
    header += struct.pack('<2I', 0, 0) + struct.pack('<2I', *textures_ref)  # colors, textures
    header += struct.pack('<2I', *weights_ref) + struct.pack('<2I', 0, 0)   # weights, texture transforms
    header += struct.pack('<2I', *replaceable_ref) + struct.pack('<2I', *materials_ref)
    header += struct.pack('<2I', *combos_ref) + struct.pack('<2I', *texture_lookup_ref)
    header += struct.pack('<2I', *uv_lookup_ref) + struct.pack('<2I', *transparency_lookup_ref)
    header += struct.pack('<2I', *transform_lookup_ref)
    header += struct.pack('<6ff', *lo, *hi, reach)
    header += struct.pack('<6ff', *collision_lo, *collision_hi, meta['collisionSphereRadius'])
    header += struct.pack('<2I', 0, 0) * 3                                  # collision tris, verts, normals
    header += struct.pack('<2I', *attachments_ref) + struct.pack('<2I', *attachment_lookup_ref)
    header += struct.pack('<2I', 0, 0) * 2                                  # events, lights
    header += struct.pack('<2I', 0, 0) * 2                                  # cameras + lookup
    header += struct.pack('<2I', 0, 0) * 2                                  # ribbons, particles
    assert len(header) == 0x130
    blob.data[:0x130] = header
    (out_dir / f'{name}.m2').write_bytes(bytes(blob.data))

    # Skin profile
    skin = Blob(48)
    lookup_ref = skin.array('H', skin_vertices)
    tris_ref = skin.array('H', skin_indices)
    props_ref = skin.array('4B', skin_props)
    sections_ref = skin.add(b''.join(section_records), len(section_records))
    batch_data = b''
    batch_count = 0
    for section, texture, material, uv_set, priority in sorted(units, key=lambda u: meta['materials'][u[2]]
                                                               ['blendingMode'] not in (0, 1)):
        for part in mesh_sections.get(section, []):
            batch_data += struct.pack('<Bb11H', 16, priority, 0, part, part, NONE16, material, 0, 1,
                                      used_textures.index(texture), uv_set, 0, 0)
            batch_count += 1
    batches_ref = skin.add(batch_data, batch_count)
    skin.data[:48] = struct.pack('<4s2I2I2I2I2II', b'SKIN', *lookup_ref, *tris_ref, *props_ref, *sections_ref,
                                 *batches_ref, BONES_PER_SECTION)
    (out_dir / f'{name}00.skin').write_bytes(bytes(skin.data))

    files = [f'{name}.m2', f'{name}00.skin'] + [f'{name}_{t}.blp' for t in used_textures]
    print(f'Wrote {name}: {len(positions)} vertices, {len(skin_indices) // 3} triangles in {len(section_records)} '
          f'submeshes, {bone_count} bones ({sum(animated)} animated), {len(sequences)} sequences, '
          f'{len(attachment_ids)} attachments, {len(used_textures)} textures, {len(blob.data):,} bytes')
    return files


def main():
    if len(sys.argv) != 6:
        sys.exit(__doc__)
    convert(*sys.argv[1:])


if __name__ == '__main__':
    main()
