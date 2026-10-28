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
BLEND_ALPHA, BLEND_ADD = 2, 4
UNFOGGED = 0x2
WRAP_U = 0x1
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
                 edges_per_second=50.0, color=(1.0, 1.0, 1.0)):
    """Write <name>.m2, <name>00.skin and <name>.blp into out_dir.

    above, below: yards the ribbon spans along its bone's Z axis, turned by the quaternion turn.
    duration: milliseconds of each of the two animations; the kit removes the model when they are
    done. lifetime: seconds each stretch of trail lasts before it fades. color: red, green, blue
    (0 to 1) the texture is tinted with, for gray textures.
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
    ribbon += one('3f', tuple(map(float, color))) + one('h', 0x7FFF)       # color, alpha
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


def _soft_strip(texture, size=(64, 64), tint=(0.85, 1.0, 0.7)):
    """A strip as bright as the texture's brightest column all along, fading across like it: a beam
    core that can be stretched to any length."""
    with Image.open(texture) as image:
        alpha = image.convert('RGBA').getchannel('A')
    profile = [max(alpha.getpixel((x, y)) for x in range(alpha.width)) for y in range(alpha.height)]
    strip = Image.new('RGBA', size)
    for y in range(size[1]):
        a = profile[min(alpha.height - 1, y * alpha.height // size[1])]
        color = tuple(int(255 * c) for c in tint) + (a,)
        for x in range(size[0]):
            strip.putpixel((x, y), color)
    return strip


def _soft_edges(texture):
    """The texture with its alpha fading out towards the top and bottom (sides of a beam)."""
    with Image.open(texture) as image:
        image = image.convert('RGBA')
    alpha = Image.new('L', image.size)
    for y in range(image.height):
        edge = 1.0 - abs(2.0 * (y + 0.5) / image.height - 1.0)        # 0 at the edges, 1 in the middle
        alpha.paste(int(255 * min(1.0, edge * 1.6) ** 1.5), (0, y, image.width, y + 1))
    image.putalpha(alpha)
    return image


def beam(flare_texture, flow_texture, core_texture, out_dir, name, client_dir, length=20.0, width=0.9,
         flare_length=2.5, tile=2.5, scroll_ms=600, duration=2000):
    """A beam shot forward (+X) from the attachment it sits on (a kit's breath slot: from the face).

    Three additive layers, each two crossed planes so the beam looks round from any side: a flare
    at the eyes (flare_texture, flare_length yards long), the beam's body (flow_texture with soft
    edges, one copy every tile yards, scrolling outward once every scroll_ms) and a bright core
    (core_texture's brightest column, stretched). duration: milliseconds of each of the two
    animations, the same layout as ribbon_trail().

    Writes <name>.m2, <name>00.skin and one BLP per layer into out_dir.
    """
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    layers = [   # BLP name, image, length, half width, U repeats, scrolls
        (f'{name}_Body', _soft_edges(flow_texture), length, width, length / tile, True),
        (f'{name}_Core', _soft_strip(core_texture), length, width * 0.3, 1.0, False),
        (f'{name}_Flare', Image.open(flare_texture).convert('RGBA'), flare_length, width * 0.9, 1.0, False),
    ]
    for blp_name, image, *_ in layers:
        (out_dir / f'{blp_name}.blp').write_bytes(blp.encode(image))

    # Geometry: per layer, a flat plane (across Y) and an upright one (across Z), x from 0 to length.
    vertices, triangles, sections = b'', [], []
    for _, _, reach, half, repeats, _ in layers:
        start, first = len(triangles), len(vertices) // 48
        for across in ((0.0, 1.0, 0.0), (0.0, 0.0, 1.0)):
            base = len(vertices) // 48
            for x, u in ((0.0, 0.0), (reach, repeats)):
                for side, v in ((1.0, 0.0), (-1.0, 1.0)):
                    pos = (x, across[1] * half * side, across[2] * half * side)
                    vertices += struct.pack('<3f4B4B3f2f2f', *pos, 255, 0, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 1.0,
                                            u, v, 0.0, 0.0)
            triangles += [base, base + 1, base + 2, base + 2, base + 1, base + 3]
        sections.append((first, 8, start, 12, reach, half))
    vertex_count = len(vertices) // 48
    reach = max(length, flare_length)
    lo, hi = (0.0, -width, -width), (reach, width, width)

    blob = Blob(0x130)
    name_ref = blob.add(name.encode() + b'\0', len(name) + 1)
    globals_ref = blob.array('I', [scroll_ms])
    empty_lo, empty_hi = (FLOAT_MAX,) * 3, (-FLOAT_MAX,) * 3
    sequences = blob.add(b''.join(
        struct.pack('<HHIfIhHIII6ffhH', anim, 0, duration, 0.0, SEQUENCE_IN_MODEL, 0x7FFF, 0, 0, 0, 0,
                    *empty_lo, *empty_hi, 0.0, -1, index)
        for index, anim in enumerate((ANIM_STAND, ANIM_BIRTH))), 2)
    bone = struct.pack('<iIhHHH', -1, 0, -1, 0, 0, 0) + empty_track() * 3 + struct.pack('<3f', 0, 0, 0)
    bones = blob.add(bone, 1)
    key_bones = blob.array('h', [-1])
    vertices_ref = blob.add(vertices, vertex_count)

    textures = b''
    for blp_name, _, _, _, _, scrolls in layers:
        path = f'{client_dir}\\{blp_name}.blp'
        ref = blob.add(path.encode() + b'\0', len(path) + 1)
        textures += struct.pack('<II2I', 0, WRAP_U if scrolls else 0, *ref)
    textures_ref = blob.add(textures, len(layers))
    weights = blob.add(track(blob, 0, [([0], [0x7FFF])], 'h'), 1)
    # The body's texture slides outward along the beam, on global sequence 0.
    slide = track(blob, 1, [([0, scroll_ms], [(0.0, 0.0, 0.0), (-1.0, 0.0, 0.0)])], '3f')
    slide = slide[:2] + struct.pack('<h', 0) + slide[4:]
    transforms_ref = blob.add(slide + empty_track() + empty_track(), 1)
    replaceable_lookup = blob.array('h', [-1])
    materials_ref = blob.array('HH', [(UNLIT | UNFOGGED | TWO_SIDED | NO_DEPTH_WRITE, BLEND_ADD)])
    bone_lookup = blob.array('H', [0])
    texture_lookup = blob.array('H', list(range(len(layers))))
    texture_unit_lookup = blob.array('H', [0])
    transparency_lookup = blob.array('H', [0])
    transform_lookup = blob.array('H', [0xFFFF, 0])

    radius = math.dist(lo, hi) / 2
    header = struct.pack('<4sI', b'MD20', 264)
    header += struct.pack('<2I', *name_ref) + struct.pack('<I', 0)
    header += struct.pack('<2I', *globals_ref)
    header += struct.pack('<2I', *sequences) + struct.pack('<2I', 0, 0)            # no sequence lookup
    header += struct.pack('<2I', *bones) + struct.pack('<2I', *key_bones)
    header += struct.pack('<2I', *vertices_ref) + struct.pack('<I', 1)
    header += struct.pack('<2I', 0, 0) + struct.pack('<2I', *textures_ref)          # colors, textures
    header += struct.pack('<2I', *weights) + struct.pack('<2I', *transforms_ref)
    header += struct.pack('<2I', *replaceable_lookup) + struct.pack('<2I', *materials_ref)
    header += struct.pack('<2I', *bone_lookup) + struct.pack('<2I', *texture_lookup)
    header += struct.pack('<2I', *texture_unit_lookup) + struct.pack('<2I', *transparency_lookup)
    header += struct.pack('<2I', *transform_lookup)
    header += struct.pack('<7f', *lo, *hi, radius) + struct.pack('<7f', *[0.0] * 7)
    header += struct.pack('<2I', 0, 0) * 3                                          # collision
    header += struct.pack('<2I', 0, 0) * 2                                          # attachments
    header += struct.pack('<2I', 0, 0) * 2                                          # events, lights
    header += struct.pack('<2I', 0, 0) * 2                                          # cameras
    header += struct.pack('<2I', 0, 0) * 2                                          # ribbons, particles
    assert len(header) == 0x130
    blob.data[:0x130] = header
    (out_dir / f'{name}.m2').write_bytes(bytes(blob.data))

    skin = Blob(48)
    lookup = skin.array('H', list(range(vertex_count)))
    tris = skin.array('H', triangles)
    props = skin.add(b'\0' * 4 * vertex_count, vertex_count)
    section_data, batch_data = b'', b''
    for i, (first, count, start, index_count, reach_i, half) in enumerate(sections):
        center = (reach_i / 2, 0.0, 0.0)
        section_data += struct.pack('<10H3f3ff', 0, 0, first, count, start, index_count, 1, 0, 1, 0,
                                    *center, *center, math.hypot(reach_i / 2, half))
        batch_data += struct.pack('<Bb11H', 16, 0, 0, i, i, 0xFFFF, 0, 0, 1, i, 0, 0,
                                  1 if layers[i][5] else 0)
    sections_ref = skin.add(section_data, len(sections))
    batches_ref = skin.add(batch_data, len(sections))
    skin.data[:48] = struct.pack('<4s2I2I2I2I2II', b'SKIN', *lookup, *tris, *props, *sections_ref, *batches_ref, 21)
    (out_dir / f'{name}00.skin').write_bytes(bytes(skin.data))
    return [f'{name}.m2', f'{name}00.skin'] + [f'{blp_name}.blp' for blp_name, *_ in layers]
