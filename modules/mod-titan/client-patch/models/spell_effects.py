#!/usr/bin/env python3
"""Spell effect models (WoW 3.3.5a M2, version 264) for Emberforge's own spells.

ribbon_trail() writes a trail: a model with no mesh, only a ribbon emitter, that a spell visual
kit attaches to the caster (see patch_spells.py). The ribbon leaves the texture behind as the
attachment moves. weapon_trail() is one for the weapons (SpellVisualKit LeftWeaponEffect /
RightWeaponEffect), body_trail() one for the chest (ChestEffect, like Blizzard's Charge trail).

It is laid out like Blizzard's own Spells\\RibbonTrail.m2 (which the client handles fine): no mesh
and an empty skin, no lookup tables, two short animations (Stand and Birth) with no blend time and
empty bounds, and the bone's turn on global sequence 0. An earlier layout (a sequence lookup, a
150 ms blend time, the bone turn on the animation, a transparent triangle) froze the client the
moment its animation ended.

A ribbon spans its bone's Z axis (heightAbove up, heightBelow down), and the bone is turned to
lay that span where it belongs. Weapon models point their blade along +X from the grip, the way a
kit's weapon effect is placed, so a weapon trail's bone is turned a quarter turn about Y. A body
trail's bone is turned as in Blizzard's RibbonTrail.

Trail textures are drawn running from the weapon (left edge, newest) to the end of the trail
(right edge, oldest), blade tip at the top. The client lays a ribbon's texture the other way round
(U across the blade from the grip, V along the trail; seen in game), so they are turned to match.
Nearly transparent pixels are cleared: across a whole swing their faint color showed as a haze.
"""

import math
import struct
from pathlib import Path

from PIL import Image

import blp
from wowexport_to_m2 import Blob, SEQUENCE_IN_MODEL, empty_track, track

UNLIT, TWO_SIDED, NO_DEPTH_WRITE = 0x1, 0x4, 0x10
BLEND_ALPHA = 2
BONE_TRANSFORMED = 0x200
ANIM_STAND, ANIM_BIRTH = 0, 158
FLOAT_MAX = 3.4028234663852886e+38
QUARTER_TURN_Y = (0.0, math.sqrt(0.5), 0.0, math.sqrt(0.5))      # quaternion x, y, z, w
BLIZZARD_CHEST_TURN = (-0.45452, -0.45452, 0.54164, 0.54164)      # RibbonTrail's ribbon bone
FAINT = 12          # alpha below this is cleared from trail textures


def comp_quat(value):
    """A quaternion component as WotLK stores it (M2CompQuat, read back as (v < 0 ? v + 32768 :
    v - 32767) / 32767)."""
    return round(value * 32767) - 32768 if value >= 0 else round(value * 32767) + 32767


def weapon_trail(texture, out_dir, name, client_dir, length=1.1, **options):
    """A trail behind the blade. length: yards of blade it covers, from the grip."""
    return ribbon_trail(texture, out_dir, name, client_dir, length, 0.0, QUARTER_TURN_Y, **options)


def body_trail(texture, out_dir, name, client_dir, height=0.8, **options):
    """A trail behind the body. height: yards it reaches each way from the chest."""
    return ribbon_trail(texture, out_dir, name, client_dir, height, height, BLIZZARD_CHEST_TURN, **options)


def ribbon_trail(texture, out_dir, name, client_dir, above, below, turn, duration=600, lifetime=0.25,
                 edges_per_second=50.0):
    """Write <name>.m2, <name>00.skin and <name>.blp into out_dir.

    above, below: yards the ribbon spans along its bone's Z axis, turned by the quaternion turn.
    duration: milliseconds of each of the two animations; the kit removes the model when they are
    done. lifetime: seconds each stretch of trail lasts before it fades.
    """
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    with Image.open(texture) as image:
        trail = image.convert('RGBA').transpose(Image.Transpose.TRANSPOSE).transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        alpha = trail.getchannel('A').point(lambda a: 0 if a < FAINT else a)
        trail.putalpha(alpha)
        (out_dir / f'{name}.blp').write_bytes(blp.encode(trail))
    texture_name = f'{client_dir}\\{name}.blp'

    blob = Blob(0x130)
    name_ref = blob.add(name.encode() + b'\0', len(name) + 1)
    globals_ref = blob.array('I', [0])

    # Stand and Birth, as Blizzard's: no blend time, empty bounds, each its own alias.
    empty_lo, empty_hi = (FLOAT_MAX,) * 3, (-FLOAT_MAX,) * 3
    sequences = blob.add(b''.join(
        struct.pack('<HHIfIhHIII6ffhH', anim, 0, duration, 0.0, SEQUENCE_IN_MODEL, 0x7FFF, 0, 0, 0, 0,
                    *empty_lo, *empty_hi, 0.0, -1, index)
        for index, anim in enumerate((ANIM_STAND, ANIM_BIRTH))), 2)

    # Bone 0: still, turned so the ribbon's Z span lies where it belongs.
    rotation = track(blob, 0, [([0], [tuple(comp_quat(v) for v in turn)])], '4h')
    rotation = rotation[:2] + struct.pack('<h', 0) + rotation[4:]          # on global sequence 0
    bone = struct.pack('<iIhHHH', -1, BONE_TRANSFORMED, -1, 0, 0, 0)
    bone += empty_track() + rotation + empty_track() + struct.pack('<3f', 0, 0, 0)
    bones = blob.add(bone, 1)
    key_bones = blob.array('h', [-1])

    ref = blob.add(texture_name.encode() + b'\0', len(texture_name) + 1)
    textures_ref = blob.add(struct.pack('<II2I', 0, 0, *ref), 1)       # clamped: the trail ends at the edges
    replaceable_lookup = blob.array('h', [-1])
    materials_ref = blob.array('HH', [(UNLIT | TWO_SIDED | NO_DEPTH_WRITE, BLEND_ALPHA)])

    def one(fmt, value):
        return track(blob, 0, [([0], [value])], fmt)

    ribbon = struct.pack('<iI3f', -1, 0, 0.0, 0.0, 0.0)
    ribbon += struct.pack('<2I', *blob.array('H', [0])) + struct.pack('<2I', *blob.array('H', [0]))
    ribbon += one('3f', (1.0, 1.0, 1.0)) + one('h', 0x7FFF)                # color, alpha
    ribbon += one('f', above) + one('f', below)
    ribbon += struct.pack('<3f', edges_per_second, lifetime, 0.0)           # edges/s, edge life, gravity
    ribbon += struct.pack('<HH', 1, 1)                                      # texture rows, columns
    ribbon += one('h', 0)                                                   # texture slot
    ribbon += one('B', 1)                                                   # visible
    ribbon += struct.pack('<hBB', 0, 0, 0)
    assert len(ribbon) == 176, len(ribbon)
    ribbons_ref = blob.add(ribbon, 1)

    reach = max(above, below)
    lo, hi = (-reach, -reach, -reach), (reach, reach, reach)
    header = struct.pack('<4sI', b'MD20', 264)
    header += struct.pack('<2I', *name_ref) + struct.pack('<I', 0)
    header += struct.pack('<2I', *globals_ref)
    header += struct.pack('<2I', *sequences) + struct.pack('<2I', 0, 0)            # no sequence lookup
    header += struct.pack('<2I', *bones) + struct.pack('<2I', *key_bones)
    header += struct.pack('<2I', 0, 0) + struct.pack('<I', 1)                       # no vertices, 1 skin
    header += struct.pack('<2I', 0, 0) + struct.pack('<2I', *textures_ref)          # colors, textures
    header += struct.pack('<2I', 0, 0) * 2                                          # transparency, transforms
    header += struct.pack('<2I', *replaceable_lookup) + struct.pack('<2I', *materials_ref)
    header += struct.pack('<2I', 0, 0) * 5                                          # bone .. transform lookups
    header += struct.pack('<7f', *lo, *hi, reach * math.sqrt(3)) + struct.pack('<7f', *[0.0] * 7)
    header += struct.pack('<2I', 0, 0) * 3                                          # collision
    header += struct.pack('<2I', 0, 0) * 2                                          # attachments
    header += struct.pack('<2I', 0, 0) * 2                                          # events, lights
    header += struct.pack('<2I', 0, 0) * 2                                          # cameras
    header += struct.pack('<2I', *ribbons_ref) + struct.pack('<2I', 0, 0)          # ribbons, particles
    assert len(header) == 0x130
    blob.data[:0x130] = header
    (out_dir / f'{name}.m2').write_bytes(bytes(blob.data))

    # Skin: empty, as Blizzard's (the ribbon draws itself).
    (out_dir / f'{name}00.skin').write_bytes(struct.pack('<4s2I2I2I2I2II', b'SKIN', *[0] * 10, 21))
    return [f'{name}.m2', f'{name}00.skin', f'{name}.blp']
