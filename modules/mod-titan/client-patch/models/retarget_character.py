#!/usr/bin/env python3
"""Add retail animations to a WoW 3.3.5a character model (version 264), retargeted from a retail character.

Source: a retail character exported by wow.export as glTF (e.g. character/nightelf/male/nightelfmale_hd.m2),
read with creature_gltf_to_m2.Export. Target: Blizzard's 3.3.5a model of a race and gender, from the client.

M2 bones carry no rest rotation (only a pivot), and the retail skeletons keep the old ones' joints in nearly
the same places, so an old bone takes the retail pose bone by bone:
- each old bone is matched to the retail bone at the same place (by its pivot, among the descendants of
  the retail bone its parent matched; key bones by name first), scaled by the models' heights;
- a matched bone gets the retail bone's orientation in the model (its world rotation); its own rotation
  is that relative to its parent's. Unmatched bones follow their parent;
- the root bones (the model origin and the hips) also take the retail ones' movement, scaled.
Bones that sway on their own (global sequences: hair, cloth) and everything else that is animated per
sequence (bones not matched, colors, attachments) play the model's Stand in the new sequences; events
(footsteps) have none.

The model is patched, not rebuilt: the new sequences go after the existing ones, and every per-sequence
track gets one more entry per new sequence, written after the old data. Nothing already in the file moves,
so the sequences kept in .anim files still find their data.

Usage: python3 retarget_character.py <retail glTF dir> <3.3.5a model.m2> <out.m2>
"""

import struct
import sys
from pathlib import Path

import numpy as np

from creature_gltf_to_m2 import Export, comp_quat, pose

NONE16 = 0xFFFF
SEQUENCE_IN_MODEL = 0x20
SEQUENCE_ONCE = 0x1                     # plays once (Blizzard's flag on attacks, casts, jump start and end);
                                        # without it the client treats a sequence as a loop, and running replaces it
BONE_TRANSFORMED = 0x200
SAMPLE_MS = 33                          # keyframe spacing of the retargeted animations
MATCH_DISTANCE = 0.06                   # yards (after scaling): farther pivots are not the same joint
ROOT_BONES = 2                          # bones 0 (origin) and 1 (hips) also move

# New sequences: (3.3.5a animation id, retail animations as (id, variation), one per variation). The ids
# are AnimationData rows no character model uses; spell visual kits play them.
# A tuple of retail animations plays them one after the other. The same slots hold the same animations in the
# demon forms (creature_gltf_to_m2.py), so one spell visual plays them on both.
ANIMATIONS = [
    (215, [((1056, v), (1058, v), (1060, v)) for v in range(3)]),  # CustomSpell03 <- DHBladeDance 1, 2, 3
    (216, [(1042, 0)]),                             # CustomSpell04 <- FelRushLoop
    (217, [(1038, 0)]),                             # CustomSpell05 <- BackFlipStart (Vengeful Retreat)
    (218, [(1040, 0)]),                             # CustomSpell06 <- BackFlipLoop
    (219, [(1008, 0), (1010, 0)]),                  # CustomSpell07 <- GlvThrowMain, GlvThrownOff (Throw Glaive)
    (220, [(1014, 0), (1018, 0)]),                  # CustomSpell08 <- CombatAbilityGlv01, GlvOff01 (Demon's Bite)
    (221, [(1022, 0), (1024, 0), (1028, 0)]),       # CustomSpell09 <- CombatAbilityGlvBig01 to 03 (Chaos Strike)
    (222, [(1044, 0)]),                             # CustomSpell10 <- FelRushEnd
    (154, [(1062, 0)]),                             # Custom1 <- DHMeteorStrike
]
LOOPS = {216, 218}                                  # the rest play once
# Animations whose turning and travel go on the model's origin bone (0) instead of the hips, as Blizzard's
# Whirlwind does: a moving character's hips and legs play the movement animation, the origin bone still plays
# the action, so the whole character turns and dashes while running. Standing still it looks the same.
ROOT_MOTION = {215}

KEY_BONES = ['ArmL', 'ArmR', 'ShoulderL', 'ShoulderR', 'SpineLow', 'Waist', 'Head', 'Jaw',
             'IndexFingerR', 'MiddleFingerR', 'PinkyFingerR', 'RingFingerR', 'ThumbR',
             'IndexFingerL', 'MiddleFingerL', 'PinkyFingerL', 'RingFingerL', 'ThumbL',
             '$BTH', '$CSR', '$CSL', '_Breath', '_Name', '_NameMount', '$CHD', '$CCH', 'Root']

SEQUENCE = struct.Struct('<HHIfIhHIII6ffhH')
TRACK = struct.Struct('<Hh2I2I')
TRACK_BASE = struct.Struct('<Hh2I')


def arr(data, offset):
    return struct.unpack_from('<II', data, offset)


class Model:
    """The 3.3.5a model's bones and the place of every per-sequence track in the file."""

    def __init__(self, data):
        self.data = bytearray(data)
        if self.data[:4] != b'MD20' or struct.unpack_from('<I', self.data, 4)[0] != 264:
            raise ValueError('not a 3.3.5a M2')
        count, offset = arr(self.data, 0x1C)
        self.sequences = [SEQUENCE.unpack_from(self.data, offset + 64 * i) for i in range(count)]
        count, self.bones_at = arr(self.data, 0x2C)
        self.parents, self.pivots, self.key_bones, self.flags = [], [], [], []
        for i in range(count):
            key_bone, flags, parent = struct.unpack_from('<iIh', self.data, self.bones_at + 88 * i)
            self.parents.append(parent)
            self.key_bones.append(key_bone)
            self.flags.append(flags)
            self.pivots.append(np.array(struct.unpack_from('<3f', self.data, self.bones_at + 88 * i + 76)))
        self.pivots = np.array(self.pivots)

    def bone_track(self, bone, kind):
        """File offset of a bone's translation (0), rotation (1) or scale (2) track."""
        return self.bones_at + 88 * bone + 16 + 20 * kind

    def other_tracks(self):
        """(offset, has values) of every other track: colors, texture weights and transforms, attachments,
        events (timestamps only), cameras."""
        found = []
        layouts = [(0x48, 40, (0, 20)), (0x58, 20, (0,)), (0x60, 60, (0, 20, 40)), (0xF0, 40, (20,)),
                   (0x110, 100, (16, 48, 80))]
        for header, size, fields in layouts:
            count, offset = arr(self.data, header)
            found += [(offset + size * i + f, True) for i in range(count) for f in fields]
        count, offset = arr(self.data, 0x100)
        found += [(offset + 36 * i + 24, False) for i in range(count)]
        for header in (0x108, 0x120, 0x128):
            if arr(self.data, header)[0]:
                raise ValueError('lights, ribbons and particles are not handled')
        return found

    def append(self, payload):
        while len(self.data) % 16:
            self.data.append(0)
        offset = len(self.data)
        self.data += payload
        return offset


def orthonormal(m):
    u, _, vt = np.linalg.svd(m)
    r = u @ vt
    if np.linalg.det(r) < 0:
        u[:, -1] *= -1
        r = u @ vt
    return r


def matrix_quat(m):
    """(x, y, z, w) of a rotation matrix."""
    w = np.sqrt(max(0.0, 1 + m[0, 0] + m[1, 1] + m[2, 2])) / 2
    x = np.sqrt(max(0.0, 1 + m[0, 0] - m[1, 1] - m[2, 2])) / 2
    y = np.sqrt(max(0.0, 1 - m[0, 0] + m[1, 1] - m[2, 2])) / 2
    z = np.sqrt(max(0.0, 1 - m[0, 0] - m[1, 1] + m[2, 2])) / 2
    x = np.copysign(x, m[2, 1] - m[1, 2])
    y = np.copysign(y, m[0, 2] - m[2, 0])
    z = np.copysign(z, m[1, 0] - m[0, 1])
    q = np.array([x, y, z, w])
    return q / np.linalg.norm(q)


def descendants(parents, root):
    children = {}
    for i, p in enumerate(parents):
        children.setdefault(p, []).append(i)
    found, todo = [], [root]
    while todo:
        b = todo.pop()
        found.append(b)
        todo += children.get(b, [])
    return found


def match_bones(model, export):
    """Old bone -> retail bone (or None: it follows its parent), for every bone under the model's root; and the
    bones that sway on their own (global sequences: hair, cloth), which keep doing so."""
    # Sized by the head: the models' highest bones are helpers (the name plate), not the body.
    old_head = model.pivots[model.key_bones.index(KEY_BONES.index('Head')), 2]
    scale = old_head / export.pivots[export.names.index('Head'), 2]
    retail_pivots = export.pivots * scale
    # Not body joints: attachment helpers, the cape, the face (eyes, lids, jaw: an old face bone follows the head)
    skip = {i for i, n in enumerate(export.names)
            if n[0] in '$_' or n.startswith('EXP_C1_Cape') or 'Eye' in n or n == 'Jaw'}
    has_children = set(export.parents)
    match, sway = {}, set()
    tracked = set(descendants(model.parents, 0))
    for b in range(len(model.parents)):
        if b not in tracked:
            continue
        rotation = TRACK.unpack_from(model.data, model.bone_track(b, 1))
        if rotation[1] != -1:
            sway.add(b)
            match[b] = None
            continue
        parent = model.parents[b]
        if parent < 0:
            match[b] = 0
            continue
        if match.get(parent) is None:
            match[b] = None
            continue
        key = KEY_BONES[model.key_bones[b]] if 0 <= model.key_bones[b] < len(KEY_BONES) else None
        if key in export.names and np.linalg.norm(retail_pivots[export.names.index(key)] - model.pivots[b]) < 0.15:
            match[b] = export.names.index(key)
            continue
        candidates = [r for r in descendants(export.parents, match[parent]) if r not in skip]
        # Named joints first, then joints with children (the limb, not a twist or helper bone beside it)
        cost = [(np.linalg.norm(retail_pivots[r] - model.pivots[b])
                 + (0.004 if export.names[r].startswith('EXP') or export.names[r].isdigit() else 0)
                 + (0 if r in has_children else 0.01), r)
                for r in candidates]
        best = min(cost)
        match[b] = best[1] if best[0] < MATCH_DISTANCE else None
    return match, sway, scale


def retarget(model, export, match, sway, scale, key, root_motion=False):
    """Per old bone: (times, rotations as quaternions or None, translations or None) for one animation.
    root_motion: the hips' heading and ground travel go on the origin bone (ROOT_MOTION)."""
    duration = export.animations[key][0]
    times = list(range(0, duration, SAMPLE_MS)) + [duration]
    order = sorted(match)                                  # parents come before their children
    rotations = {b: [] for b in match if b not in sway}
    translations = {b: [] for b in match if b < ROOT_BONES}
    for t in times:
        retail = pose(export, key, t)
        world = {}
        for b in order:
            parent = model.parents[b]
            parent_world = world[parent] if parent >= 0 else np.eye(4)
            r = match[b]
            if b == 0 and root_motion:
                hips = match[1]
                hips_rotation = orthonormal(retail[hips][:3, :3])
                heading = hips_rotation @ np.array([1.0, 0.0, 0.0])
                yaw = np.arctan2(heading[1], heading[0])
                rotation = np.array([[np.cos(yaw), -np.sin(yaw), 0], [np.sin(yaw), np.cos(yaw), 0], [0, 0, 1]])
                position = (retail[hips] @ np.append(export.pivots[hips], 1))[:3] * scale
                travel = position - rotation @ model.pivots[1]
                translation = np.array([travel[0], travel[1], 0.0])
                translations[b].append(translation)
                local = np.eye(4)
                local[:3, :3] = rotation
                local[:3, 3] = translation
                world[b] = local                            # the origin bone's pivot is the model's origin
                q = matrix_quat(rotation)
                if rotations[b] and np.dot(q, rotations[b][-1]) < 0:
                    q = -q
                rotations[b].append(q)
                continue
            rotation = orthonormal(retail[r][:3, :3]) if r is not None else parent_world[:3, :3]
            local_rotation = parent_world[:3, :3].T @ rotation
            local = np.eye(4)
            local[:3, :3] = local_rotation
            translation = np.zeros(3)
            if b in translations and r is not None:
                target = retail[r] @ np.append(export.pivots[r], 1)
                target[:3] *= scale
                translation = (np.linalg.inv(parent_world) @ target)[:3] - model.pivots[b]
                translations[b].append(translation)
            to_pivot, back = np.eye(4), np.eye(4)
            to_pivot[:3, 3] = model.pivots[b] + translation
            back[:3, 3] = -model.pivots[b]
            world[b] = parent_world @ to_pivot @ local @ back
            if b in rotations:
                q = matrix_quat(local_rotation)
                if rotations[b] and np.dot(q, rotations[b][-1]) < 0:
                    q = -q                                  # keep neighbouring keys on the same side
                rotations[b].append(q)
    return duration, times, rotations, translations


def patch(model, export, sources):
    """Adds the sequences in `sources` [(anim id, [(duration, times, rotations, translations), ...])]."""
    data = model.data
    old_count = len(model.sequences)
    stand = next(i for i, s in enumerate(model.sequences) if s[0] == 0 and s[1] == 0)
    count, lookup_at = arr(data, 0x24)
    lookup = list(struct.unpack_from(f'<{count}H', data, lookup_at))

    new_sequences = []
    for anim_id, variations in sources:
        if anim_id >= len(lookup) or lookup[anim_id] != NONE16:
            raise ValueError(f'animation {anim_id} is taken (or past the lookup table) in this model')
        first = old_count + len(new_sequences)
        lookup[anim_id] = first
        for v, (duration, _, _, _) in enumerate(variations):
            index = old_count + len(new_sequences)
            following = index + 1 if v + 1 < len(variations) else -1
            s = model.sequences[stand]
            flags = SEQUENCE_IN_MODEL | (0 if anim_id in LOOPS else SEQUENCE_ONCE)
            new_sequences.append(SEQUENCE.pack(anim_id, v, duration, 0.0, flags,
                                               0x7FFF // len(variations), 0, 0, 0, 150, *s[10:17], following, index))
    struct.pack_into(f'<{len(lookup)}H', data, lookup_at, *lookup)
    flat = [v for _, variations in sources for v in variations]

    def extend(offset, entries, values=True):
        """Gives a per-sequence track `entries` [(stamps ref, values ref)] for the new sequences."""
        if values:
            interpolation, global_seq, n_stamps, o_stamps, n_values, o_values = TRACK.unpack_from(data, offset)
        else:
            interpolation, global_seq, n_stamps, o_stamps = TRACK_BASE.unpack_from(data, offset)
        if global_seq != -1:
            return
        stamps = [arr(data, o_stamps + 8 * i) for i in range(n_stamps)]
        stamps += [(0, 0)] * (old_count - len(stamps))
        stamps += [e[0] for e in entries]
        o_stamps = model.append(b''.join(struct.pack('<II', *e) for e in stamps))
        if values:
            vals = [arr(data, o_values + 8 * i) for i in range(n_values)]
            vals += [(0, 0)] * (old_count - len(vals))
            vals += [e[1] for e in entries]
            o_values = model.append(b''.join(struct.pack('<II', *e) for e in vals))
            TRACK.pack_into(data, offset, interpolation, global_seq, len(stamps), o_stamps, len(vals), o_values)
        else:
            TRACK_BASE.pack_into(data, offset, interpolation, global_seq, len(stamps), o_stamps)

    def stand_entry(offset):
        _, global_seq, n_stamps, o_stamps, n_values, o_values = TRACK.unpack_from(data, offset)
        if global_seq != -1 or stand >= n_stamps:
            return (0, 0), (0, 0)
        return arr(data, o_stamps + 8 * stand), arr(data, o_values + 8 * stand)

    for b in range(len(model.parents)):
        retargeted = [flat[0][2].get(b) is not None, b in flat[0][3]]
        for kind in range(3):
            offset = model.bone_track(b, kind)
            if kind == 1 and b in flat[0][2] and flat[0][2][b]:
                entries = []
                for _, times, rotations, _ in flat:
                    quats = [tuple(comp_quat(c) for c in q) for q in rotations[b]]
                    entries.append(((len(times), model.append(struct.pack(f'<{len(times)}I', *times))),
                                     (len(quats), model.append(b''.join(struct.pack('<4h', *q) for q in quats)))))
                interpolation = TRACK.unpack_from(data, offset)
                TRACK.pack_into(data, offset, 1, -1, *interpolation[2:])
                extend(offset, entries)
                model.flags[b] |= BONE_TRANSFORMED
            elif kind == 0 and retargeted[1]:
                entries = []
                for _, times, _, translations in flat:
                    entries.append(((len(times), model.append(struct.pack(f'<{len(times)}I', *times))),
                                    (len(times), model.append(b''.join(struct.pack('<3f', *v)
                                                                       for v in translations[b])))))
                interpolation = TRACK.unpack_from(data, offset)
                TRACK.pack_into(data, offset, 1, -1, *interpolation[2:])
                extend(offset, entries)
            elif kind == 0 and b in flat[0][2]:
                extend(offset, [((0, 0), (0, 0))] * len(flat))        # matched: no stretch
            else:
                extend(offset, [stand_entry(offset)] * len(flat))
        struct.pack_into('<I', data, model.bones_at + 88 * b + 4, model.flags[b])
    for offset, values in model.other_tracks():
        if values:
            extend(offset, [stand_entry(offset)] * len(flat))
        else:
            extend(offset, [((0, 0), (0, 0))] * len(flat), values=False)

    count, offset = arr(data, 0x1C)
    sequences = bytes(data[offset:offset + 64 * count]) + b''.join(new_sequences)
    struct.pack_into('<2I', data, 0x1C, count + len(new_sequences), model.append(sequences))
    return len(new_sequences)


def build(retail_dir, model_path):
    """The model at `model_path` with ANIMATIONS added, retargeted from the retail glTF in `retail_dir`."""
    export = Export(retail_dir, skeleton_only=True)
    model = Model(Path(model_path).read_bytes())
    match, sway, scale = match_bones(model, export)
    assert all(model.parents[b] < b for b in match if model.parents[b] >= 0)
    sources = []
    for anim_id, keys in ANIMATIONS:
        keys = [export.concatenate(k) if isinstance(k[0], tuple) else k for k in keys
                if (all(p in export.animations for p in k) if isinstance(k[0], tuple) else k in export.animations)]
        found = [retarget(model, export, match, sway, scale, k, anim_id in ROOT_MOTION) for k in keys]
        if found:
            sources.append((anim_id, found))
    added = patch(model, export, sources)
    print(f'{Path(model_path).name}: {sum(r is not None for r in match.values())} of {len(match)} bones matched '
          f'(scale {scale:.3f}), {added} sequences added')
    return bytes(model.data)


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    data = build(sys.argv[1], sys.argv[2])
    Path(sys.argv[3]).write_bytes(data)
    print(f'Wrote {sys.argv[3]} ({len(data):,} bytes)')


if __name__ == '__main__':
    main()
