#!/usr/bin/env python3
"""Spell effect models (WoW 3.3.5a M2, version 264) for Emberforge's own spells.

weapon_trail() writes a weapon trail: a model with no mesh, only a ribbon emitter, that a spell
visual kit attaches to the caster's weapons (SpellVisualKit LeftWeaponEffect / RightWeaponEffect,
see patch_spells.py). The ribbon leaves the texture behind the blade as the swing moves it.

Weapon models point their blade along +X from the grip, the way a kit's weapon effect is placed.
A ribbon spans its bone's Z axis (heightAbove up, heightBelow down), so the bone is turned a
quarter turn about Y to lay that span along the blade.

The model also carries one tiny triangle drawn fully transparent: models without any mesh are not
something the client's own effect models do, so this one has the usual vertex buffer and skin.

Trail textures are drawn running from the weapon (left edge, newest) to the end of the trail
(right edge, oldest), blade tip at the top. The client lays a ribbon's texture the other way round
(U across the blade from the tip, V along the trail; seen in game), so they are transposed.
"""

import math
import struct
from pathlib import Path

from PIL import Image

import blp
from wowexport_to_m2 import Blob, NONE16, SEQUENCE_IN_MODEL, empty_track, track

UNLIT, TWO_SIDED, NO_DEPTH_WRITE = 0x1, 0x4, 0x10
BLEND_ALPHA = 2
BONE_TRANSFORMED = 0x200


def comp_quat(value):
    """A quaternion component as WotLK stores it (M2CompQuat, read back as (v < 0 ? v + 32768 :
    v - 32767) / 32767)."""
    return round(value * 32767) - 32768 if value >= 0 else round(value * 32767) + 32767


def weapon_trail(texture, out_dir, name, client_dir, length=1.1, duration=600, lifetime=0.25,
                 edges_per_second=60.0, sequence_length=30000, with_ribbon=True):
    """Write <name>.m2, <name>00.skin and <name>.blp into out_dir.

    length: yards of blade the trail covers, from the grip. duration: milliseconds the trail is
    drawn after the kit starts (the swing), None to draw it as long as the model lives; it then
    fades out over lifetime seconds.

    sequence_length: milliseconds of the one animation. It must outlast the model: the client froze
    a moment after a ribbon model's animation started over (tested in game with a 1.75 sec
    animation; 30 sec never froze, the kit removes the model well before).
    """
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    with Image.open(texture) as image:
        trail = image.convert('RGBA').transpose(Image.Transpose.TRANSPOSE)
        (out_dir / f'{name}.blp').write_bytes(blp.encode(trail))
    texture_name = f'{client_dir}\\{name}.blp'
    lo, hi = (-length, -length, -length), (length, length, length)
    radius = length * math.sqrt(3)

    blob = Blob(0x130)
    name_ref = blob.add(name.encode() + b'\0', len(name) + 1)
    seq = struct.pack('<HHIfIhHIII6ffhH', 0, 0, sequence_length, 0.0, SEQUENCE_IN_MODEL, 0x7FFF, 0, 0, 0, 150,
                      *lo, *hi, radius, -1, 0)
    sequences = blob.add(seq, 1)
    sequence_lookup = blob.array('H', [0])

    # Bone 0: still, turned a quarter turn about Y so the ribbon's Z span lies along the blade (+X).
    half = math.sqrt(0.5)
    rotation = track(blob, 1, [([0], [tuple(comp_quat(v) for v in (0.0, half, 0.0, half))])], '4h')
    bone = struct.pack('<iIhHHH', -1, BONE_TRANSFORMED, -1, 0, 0, 0)
    bone += empty_track() + rotation + empty_track() + struct.pack('<3f', 0, 0, 0)
    bones = blob.add(bone, 1)

    ref = blob.add(texture_name.encode() + b'\0', len(texture_name) + 1)
    textures_ref = blob.add(struct.pack('<II2I', 0, 0, *ref), 1)       # clamped: the trail ends at the edges
    weights = blob.add(track(blob, 0, [([0], [0])], 'h'), 1)               # the triangle: never seen
    # Color 0, which the ribbon's color index points at: white, opaque.
    colors_ref = blob.add(track(blob, 0, [([0], [(1.0, 1.0, 1.0)])], '3f') + track(blob, 0, [([0], [0x7FFF])], 'h'), 1)
    materials_ref = blob.array('HH', [(UNLIT | TWO_SIDED | NO_DEPTH_WRITE, BLEND_ALPHA)])
    bone_lookup = blob.array('H', [0])
    texture_lookup = blob.array('H', [0])
    texture_unit_lookup = blob.array('H', [0])
    transparency_lookup = blob.array('H', [0])
    replaceable_lookup = blob.array('h', [-1])
    transform_lookup = blob.array('H', [NONE16])                           # no texture animation

    def one(fmt, value):
        return track(blob, 0, [([0], [value])], fmt)

    ribbon = struct.pack('<iI3f', -1, 0, 0.0, 0.0, 0.0)
    ribbon += struct.pack('<2I', *blob.array('H', [0])) + struct.pack('<2I', *blob.array('H', [0]))
    ribbon += one('3f', (1.0, 1.0, 1.0)) + one('h', 0x7FFF)                # color, alpha
    ribbon += one('f', length) + one('f', 0.0)                             # height above, below
    ribbon += struct.pack('<3f', edges_per_second, lifetime, 0.0)           # edges/s, edge life, gravity
    ribbon += struct.pack('<HH', 1, 1)                                      # texture rows, columns
    ribbon += one('h', 0)                                                   # texture slot
    shown = ([0, duration], [1, 0]) if duration else ([0], [1])
    ribbon += track(blob, 0, [shown], 'B')                                  # drawn during the swing
    ribbon += struct.pack('<hBB', 0, 0, 0)          # priority plane, color 0, texture transform lookup 0
    assert len(ribbon) == 176, len(ribbon)
    ribbons_ref = blob.add(ribbon, 1) if with_ribbon else (0, 0)

    header = struct.pack('<4sI', b'MD20', 264)
    header += struct.pack('<2I', *name_ref) + struct.pack('<I', 0)
    header += struct.pack('<2I', 0, 0)                                              # global sequences
    header += struct.pack('<2I', *sequences) + struct.pack('<2I', *sequence_lookup)
    header += struct.pack('<2I', *bones) + struct.pack('<2I', 0, 0)
    vertices = b''.join(struct.pack('<3f4B4B3f2f2f', *pos, 255, 0, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0)
                        for pos in ((0.0, 0.0, 0.0), (0.001, 0.0, 0.0), (0.0, 0.001, 0.0)))
    vertices_ref = blob.add(vertices, 3)
    header += struct.pack('<2I', *vertices_ref) + struct.pack('<I', 1)
    header += struct.pack('<2I', *colors_ref) + struct.pack('<2I', *textures_ref)
    header += struct.pack('<2I', *weights) + struct.pack('<2I', 0, 0)               # no texture transforms
    header += struct.pack('<2I', *replaceable_lookup) + struct.pack('<2I', *materials_ref)
    header += struct.pack('<2I', *bone_lookup) + struct.pack('<2I', *texture_lookup)
    header += struct.pack('<2I', *texture_unit_lookup) + struct.pack('<2I', *transparency_lookup)
    header += struct.pack('<2I', *transform_lookup)
    header += struct.pack('<6ff6ff', *lo, *hi, radius, *lo, *hi, radius)
    header += struct.pack('<2I', 0, 0) * 3                                          # collision
    header += struct.pack('<2I', 0, 0) * 2                                          # attachments
    header += struct.pack('<2I', 0, 0) * 2                                          # events, lights
    header += struct.pack('<2I', 0, 0) * 2                                          # cameras
    header += struct.pack('<2I', *ribbons_ref) + struct.pack('<2I', 0, 0)          # ribbons, particles
    assert len(header) == 0x130
    blob.data[:0x130] = header
    (out_dir / f'{name}.m2').write_bytes(bytes(blob.data))

    # Skin: the transparent triangle (the ribbon draws itself).
    skin = Blob(48)
    lookup = skin.array('H', [0, 1, 2])
    triangles = skin.array('H', [0, 1, 2])
    props = skin.add(b'\0' * 12, 3)
    sections = skin.add(struct.pack('<10H3f3ff', 0, 0, 0, 3, 0, 3, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0.001), 1)
    batches = skin.add(struct.pack('<Bb11H', 16, 0, 0, 0, 0, NONE16, 0, 0, 1, 0, 0, 0, 0), 1)
    skin.data[:48] = struct.pack('<4s2I2I2I2I2II', b'SKIN', *lookup, *triangles, *props, *sections, *batches, 21)
    (out_dir / f'{name}00.skin').write_bytes(bytes(skin.data))
    return [f'{name}.m2', f'{name}00.skin', f'{name}.blp']
