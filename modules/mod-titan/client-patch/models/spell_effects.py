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

import json
import math
import struct
from pathlib import Path

from PIL import Image, ImageChops

import blp
from wowexport_to_m2 import Blob, SEQUENCE_IN_MODEL, empty_track, read_obj, track

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


def _ribbon_record(blob, bone, position, above, below, edges_per_second, lifetime, color, texture, material):
    """M2Ribbon (WotLK, 176 bytes) as Blizzard's RibbonTrail writes it: every track one key in one
    sequence, always visible. above / below: yards it spans along the bone's Z axis; color: red,
    green, blue (0 to 1); texture, material: indices into the model's textures and materials."""
    def one(fmt, value):
        return track(blob, 0, [([0], [value])], fmt)

    ribbon = struct.pack('<iI3f', -1, bone, *position)
    ribbon += struct.pack('<2I', *blob.array('H', [texture])) + struct.pack('<2I', *blob.array('H', [material]))
    ribbon += one('3f', tuple(map(float, color))) + one('h', 0x7FFF)       # color, alpha
    ribbon += one('f', above) + one('f', below)
    ribbon += struct.pack('<3f', edges_per_second, lifetime, 0.0)           # edges/s, edge life, gravity
    ribbon += struct.pack('<HH', 1, 1)                                      # texture rows, columns
    ribbon += one('h', 0)                                                   # texture slot
    ribbon += one('B', 1)                                                   # visible
    ribbon += struct.pack('<hBB', 0, 0, 0)
    assert len(ribbon) == 176, len(ribbon)
    return ribbon


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

    ribbon = _ribbon_record(blob, 0, (0.0, 0.0, 0.0), above, below, edges_per_second, lifetime, color, 0, 0)
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


def _glow_strip(sigma, color, peak=1.0, size=(64, 64)):
    """A strip glowing in color along its length, fading across it like a bell curve (sigma: its
    width, as a share of the strip's half height), peak: its brightest alpha (0 to 1)."""
    import numpy as np
    height, width = size
    y = (np.arange(height) + 0.5) / height * 2 - 1
    alpha = np.exp(-(y / sigma) ** 2)
    alpha = (alpha - alpha.min()) / (alpha.max() - alpha.min()) * peak
    rgba = np.zeros((height, width, 4))
    rgba[..., :3] = np.array(color) * 255
    rgba[..., 3] = alpha[:, None] * 255
    return Image.fromarray(rgba.astype('uint8'))


def _streaky(texture, blur=48, gain=1.25, sigma=0.45):
    """The texture smeared along its length (a wrap-around blur, so it still tiles): flowing
    streaks instead of its strokes, brightened by gain and fading across like _glow_strip."""
    import numpy as np
    with Image.open(texture) as image:
        rgb = np.asarray(image.convert('RGB')).astype(float)
    kernel = np.fft.fft(np.pad(np.ones(blur) / blur, (0, rgb.shape[1] - blur)))
    for _ in range(2):
        rgb = np.real(np.fft.ifft(np.fft.fft(rgb, axis=1) * kernel[None, :, None], axis=1))
    rgb = np.clip(rgb * gain, 0, 255)
    y = (np.arange(rgb.shape[0]) + 0.5) / rgb.shape[0] * 2 - 1
    alpha = np.exp(-(y / sigma) ** 2)
    alpha = (alpha - alpha.min()) / (alpha.max() - alpha.min())
    rgba = np.dstack([rgb, np.repeat((alpha * 255)[:, None], rgb.shape[1], 1)])
    return Image.fromarray(rgba.astype('uint8'))


def _fade_tip(image, start=0.7):
    """The image with its alpha fading to nothing along its last stretch (from start, a share of
    its width): the far end of a beam."""
    image = image.copy()
    alpha = image.getchannel('A')
    for x in range(int(image.width * start), image.width):
        keep = 1.0 - (x - image.width * start) / (image.width * (1.0 - start))
        alpha.paste(alpha.crop((x, 0, x + 1, image.height)).point(lambda a, k=keep: int(a * k)),
                    (x, 0))
    image.putalpha(alpha)
    return image


def beam(flare_texture, flow_texture, out_dir, name, client_dir, length=20.0, width=0.9, flare_length=2.5,
         duration=2000, burst_ms=150, settle_ms=320, fade_ms=300, taper=0.8):
    """A beam shot forward (+X) from the attachment it sits on (a kit's head slot: from the face).

    Additive layers after Legion's Eye Beam, each two crossed planes so the beam looks round from
    any side: a wide faint green glow, two fel flows (flow_texture smeared into streaks along the
    beam) scrolling outward fast at different speeds so they churn against each other, a bright
    yellow-white core, and a flare at the eyes (flare_texture, flare_length yards long). It looks
    unstable: the glow and core flicker, and the flows and core jitter in width.

    The beam bursts out: it shoots from the eyes to full length in burst_ms, flaring wide, settles
    by settle_ms, and over the last fade_ms of its duration (milliseconds: the channel) narrows to
    nothing and fades out. Its far end tapers to a point from taper (a share of the length) on, the
    glow and core fading there too.

    Same layout as ribbon_trail() (two animations of duration milliseconds), with the mesh's
    bounds and the bones' animation on them, as Blizzard's mesh effects (e.g. Mage_Dragons_Breath).
    Writes <name>.m2, <name>00.skin and one BLP per layer into out_dir.
    """
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    flow = _streaky(flow_texture)
    # Global sequences (milliseconds): the two flows' scrolls, the flicker and the width jitter.
    SCROLL_A, SCROLL_B, FLICKER, JITTER = 0, 1, 2, 3
    global_sequences = [330, 210, 230, 170]
    layers = [   # BLP name, image, length, half width, U repeats, scrolling global sequence, flickers, jitters
        (f'{name}_Glow', _fade_tip(_glow_strip(0.5, (0.35, 0.9, 0.15), peak=0.45)), length, width * 1.9, 1.0,
         None, True, False),
        (f'{name}_Body', flow, length, width, length / 6.0, SCROLL_A, False, True),
        (f'{name}_Churn', flow.transpose(Image.Transpose.FLIP_TOP_BOTTOM), length, width * 0.8, length / 4.1,
         SCROLL_B, False, True),
        (f'{name}_Core', _fade_tip(_glow_strip(0.35, (1.0, 1.0, 0.55))), length, width * 0.4, 1.0, None, True,
         True),
        (f'{name}_Flare', Image.open(flare_texture).convert('RGBA'), flare_length, width * 0.9, 1.0, None, True,
         False),
    ]
    for blp_name, image, *_ in layers:
        (out_dir / f'{blp_name}.blp').write_bytes(blp.encode(image))

    # Geometry: per layer, a flat plane (across Y) and an upright one (across Z), x from 0 to its
    # length; the long layers narrow to a point over their last stretch. Jittering layers hang on
    # bone 1, the rest on bone 0.
    vertices, triangles, sections = b'', [], []
    for _, _, reach, half, repeats, _, _, jitters in layers:
        bone = 1 if jitters else 0
        start, first = len(triangles), len(vertices) // 48
        stations = [(0.0, 1.0), (reach * taper, 1.0), (reach, 0.0)] if reach == length else [(0.0, 1.0), (reach, 1.0)]
        for across in ((0.0, 1.0, 0.0), (0.0, 0.0, 1.0)):
            base = len(vertices) // 48
            for x, scale in stations:
                for side, v in ((1.0, 0.0), (-1.0, 1.0)):
                    pos = (x, across[1] * half * scale * side, across[2] * half * scale * side)
                    vertices += struct.pack('<3f4B4B3f2f2f', *pos, 255, 0, 0, 0, bone, 0, 0, 0, 0.0, 0.0, 1.0,
                                            x / reach * repeats, v, 0.0, 0.0)
            for n in range(len(stations) - 1):
                q = base + 2 * n
                triangles += [q, q + 1, q + 2, q + 2, q + 1, q + 3]
        sections.append((first, len(vertices) // 48 - first, start, len(triangles) - start, reach, half, bone))
    vertex_count = len(vertices) // 48
    reach = max(length, flare_length)
    side = max(layer[3] for layer in layers) * 2.2          # the burst flares it 2.2 times as wide
    lo, hi = (0.0, -side, -side), (reach, side, side)

    blob = Blob(0x130)
    name_ref = blob.add(name.encode() + b'\0', len(name) + 1)
    globals_ref = blob.array('I', global_sequences)
    # Unlike a ribbon model's, the animations carry the mesh's real bounds (as Blizzard's mesh
    # effects do, e.g. Mage_Dragons_Breath): with empty ones the client culls the mesh, unseen.
    radius = math.dist(lo, hi) / 2
    sequences = blob.add(b''.join(
        struct.pack('<HHIfIhHIII6ffhH', anim, 0, duration, 0.0, SEQUENCE_IN_MODEL, 0x7FFF, 0, 0, 0, 0,
                    *lo, *hi, radius, -1, index)
        for index, anim in enumerate((ANIM_STAND, ANIM_BIRTH))), 2)

    def on_global(packed, index):
        return packed[:2] + struct.pack('<h', index) + packed[4:]

    # Bone 0: scaled from the eyes, the same keys in both animations. Length (X) from almost nothing
    # to full in burst_ms, flaring to 2.2 and 1.6 times as wide (Y, Z), back to normal by
    # settle_ms, then narrowing to nothing over the last fade_ms.
    end_fade = duration - fade_ms
    scale_keys = ([0, burst_ms, settle_ms, end_fade, duration],
                  [(0.02, 2.2, 2.2), (1.0, 1.6, 1.6), (1.0, 1.0, 1.0), (1.0, 1.0, 1.0), (1.0, 0.0, 0.0)])
    bone0 = struct.pack('<iIhHHH', -1, BONE_TRANSFORMED, -1, 0, 0, 0)
    bone0 += empty_track() + empty_track() + track(blob, 1, [scale_keys, scale_keys], '3f')
    bone0 += struct.pack('<3f', 0, 0, 0)
    # Bone 1, under bone 0: its width jitters, on its own global sequence.
    jitter = track(blob, 1, [([0, 40, 85, 125, 170], [(1.0, 1.0, 1.0), (1.0, 1.18, 0.86), (1.0, 0.88, 1.14),
                                                      (1.0, 1.1, 0.95), (1.0, 1.0, 1.0)])], '3f')
    bone1 = struct.pack('<iIhHHH', -1, BONE_TRANSFORMED, 0, 0, 0, 0)
    bone1 += empty_track() + empty_track() + on_global(jitter, JITTER) + struct.pack('<3f', 0, 0, 0)
    bones = blob.add(bone0 + bone1, 2)
    key_bones = blob.array('h', [-1])
    vertices_ref = blob.add(vertices, vertex_count)

    # Color 0 (the flickering layers): white, its alpha flickering on its own global sequence.
    flicker = track(blob, 1, [([0, 35, 70, 115, 150, 190, 230],
                               [0x7FFF, 0x5000, 0x7FFF, 0x6400, 0x7FFF, 0x4C00, 0x7FFF])], 'h')
    white = on_global(track(blob, 0, [([0], [(1.0, 1.0, 1.0)])], '3f'), FLICKER)
    colors_ref = blob.add(white + on_global(flicker, FLICKER), 1)

    textures = b''
    for blp_name, _, _, _, _, scroll, _, _ in layers:
        path = f'{client_dir}\\{blp_name}.blp'
        ref = blob.add(path.encode() + b'\0', len(path) + 1)
        textures += struct.pack('<II2I', 0, WRAP_U if scroll is not None else 0, *ref)
    textures_ref = blob.add(textures, len(layers))
    fade_keys = ([0, end_fade, duration], [0x7FFF, 0x7FFF, 0])          # fading out with the narrowing
    weights = blob.add(track(blob, 1, [fade_keys, fade_keys], 'h'), 1)
    # The flows' textures slide outward along the beam, each on its own global sequence.
    scrolling = [layer[5] for layer in layers if layer[5] is not None]
    transforms = b''
    for index in scrolling:
        slide = track(blob, 1, [([0, global_sequences[index]], [(0.0, 0.0, 0.0), (-1.0, 0.0, 0.0)])], '3f')
        transforms += on_global(slide, index) + empty_track() + empty_track()
    transforms_ref = blob.add(transforms, len(scrolling))
    replaceable_lookup = blob.array('h', [-1])
    materials_ref = blob.array('HH', [(UNLIT | UNFOGGED | TWO_SIDED | NO_DEPTH_WRITE, BLEND_ADD)])
    bone_lookup = blob.array('H', [0, 1])
    texture_lookup = blob.array('H', list(range(len(layers))))
    texture_unit_lookup = blob.array('H', [0])
    transparency_lookup = blob.array('H', [0])
    transform_lookup = blob.array('H', [0xFFFF] + list(range(len(scrolling))))

    header = struct.pack('<4sI', b'MD20', 264)
    header += struct.pack('<2I', *name_ref) + struct.pack('<I', 0)
    header += struct.pack('<2I', *globals_ref)
    header += struct.pack('<2I', *sequences) + struct.pack('<2I', 0, 0)            # no sequence lookup
    header += struct.pack('<2I', *bones) + struct.pack('<2I', *key_bones)
    header += struct.pack('<2I', *vertices_ref) + struct.pack('<I', 1)
    header += struct.pack('<2I', *colors_ref) + struct.pack('<2I', *textures_ref)
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

    # Skin: per layer a section on its bone (through the bone lookup) and a batch.
    skin = Blob(48)
    lookup = skin.array('H', list(range(vertex_count)))
    tris = skin.array('H', triangles)
    props = skin.add(b'\0' * 4 * vertex_count, vertex_count)
    section_data, batch_data = b'', b''
    for i, (first, count, start, index_count, reach_i, half, bone) in enumerate(sections):
        center = (reach_i / 2, 0.0, 0.0)
        section_data += struct.pack('<10H3f3ff', 0, 0, first, count, start, index_count, 1, bone, 1, bone,
                                    *center, *center, math.hypot(reach_i / 2, half))
        scroll, flickers = layers[i][5], layers[i][6]
        transform = 1 + scrolling.index(scroll) if scroll is not None else 0
        batch_data += struct.pack('<Bb11H', 16, 0, 0, i, i, 0 if flickers else 0xFFFF, 0, 0, 1, i, 0, 0, transform)
    sections_ref = skin.add(section_data, len(sections))
    batches_ref = skin.add(batch_data, len(sections))
    skin.data[:48] = struct.pack('<4s2I2I2I2I2II', b'SKIN', *lookup, *tris, *props, *sections_ref, *batches_ref, 21)
    (out_dir / f'{name}00.skin').write_bytes(bytes(skin.data))
    return [f'{name}.m2', f'{name}00.skin'] + [f'{blp_name}.blp' for blp_name, *_ in layers]


def _turn_z(angle):
    """A quaternion (x, y, z, w) turning angle radians about Z."""
    return (0.0, 0.0, math.sin(angle / 2), math.cos(angle / 2))


VENGEFUL_RETREAT_RING = {   # ground_burst's ring timing, after cfx_demonhunter_vengefulretreat_impactbase
    'scale': ([0, 450, 900], [0.35, 18.0, 18.0]),                       # yards across
    'fade': ([0, 167, 233, 300, 500, 900], [0x7FFF, 0x7FFF, 7638, 1783, 0, 0]),
}


def ground_burst(ring_texture, flash_texture, trail_texture, out_dir, name, client_dir, size=18.0,
                 ring_tint=(1.0, 0.835, 0.886), trail_tint=(1.0, 0.3, 0.45), duration=900,
                 ring=VENGEFUL_RETREAT_RING, ring_blend=BLEND_ALPHA, flash_sheet=True, flash_tint=None):
    """A burst left on the ground where it is placed (a kit's world slot), after Legion's Vengeful
    Retreat (cfx_demonhunter_vengefulretreat_impactbase and its spiral ribbon):

    - a shockwave ring on the ground (ring_texture tinted ring_tint, blended with ring_blend)
      growing and fading on ring's keys (milliseconds: yards across, transparency); by default
      Vengeful Retreat's, from a third of a yard to size yards in 0.45 sec, whole until 0.17 sec,
      gone by 0.5;
    - a flash at its center (flash_texture, or its first quarter if flash_sheet: a 2x2 sheet; its
      colors multiplied by flash_tint if given) popping up and gone by 0.3 sec;
    - unless trail_texture is None, a ribbon (trail_texture, tinted trail_tint) spun around the
      center while rising, leaving a corkscrew.

    Same layout as beam(): two animations of duration milliseconds, the bones' animation on them,
    the mesh's bounds. Writes <name>.m2, <name>00.skin and its BLPs into out_dir.
    """
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    flash = Image.open(flash_texture).convert('RGBA')
    if flash_tint:
        r, g, b, a = flash.split()
        r, g, b = (band.point(lambda v, k=k: int(v * k)) for band, k in zip((r, g, b), flash_tint))
        flash = Image.merge('RGBA', (r, g, b, a))
    images = [(f'{name}_Ring', Image.open(ring_texture).convert('RGBA')), (f'{name}_Flash', flash)]
    if trail_texture is not None:
        with Image.open(trail_texture) as image:
            trail = image.convert('RGBA').transpose(Image.Transpose.TRANSPOSE)
            trail = trail.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            trail.putalpha(trail.getchannel('A').point(lambda a: 0 if a < FAINT else a))
        images.append((f'{name}_Trail', trail))
    for blp_name, image in images:
        (out_dir / f'{blp_name}.blp').write_bytes(blp.encode(image))
    RING, FLASH, TRAIL = 0, 1, 2              # textures; also bones 0 (ring), 1 (flash), 2 (spiral)

    # Geometry: two flat squares just above the ground, the ring (1 yard across, scaled by bone 0)
    # and the flash (3 yards, bone 1, showing the sheet's first quarter).
    vertices, triangles, sections = b'', [], []
    for bone, half, uv_max in ((RING, 0.5, 1.0), (FLASH, 1.5, 0.5 if flash_sheet else 1.0)):
        first, start = len(vertices) // 48, len(triangles)
        for x, u in ((-half, 0.0), (half, uv_max)):
            for y, v in ((-half, uv_max), (half, 0.0)):
                vertices += struct.pack('<3f4B4B3f2f2f', x, y, 0.15, 255, 0, 0, 0, bone, 0, 0, 0, 0.0, 0.0, 1.0,
                                        u, v, 0.0, 0.0)
        triangles += [first, first + 1, first + 2, first + 2, first + 1, first + 3]
        sections.append((first, 4, start, 6, half, bone))
    vertex_count = len(vertices) // 48
    lo, hi = (-size / 2, -size / 2, 0.0), (size / 2, size / 2, 3.0)
    radius = math.dist(lo, hi) / 2

    blob = Blob(0x130)
    name_ref = blob.add(name.encode() + b'\0', len(name) + 1)
    sequences = blob.add(b''.join(
        struct.pack('<HHIfIhHIII6ffhH', anim, 0, duration, 0.0, SEQUENCE_IN_MODEL, 0x7FFF, 0, 0, 0, 0,
                    *lo, *hi, radius, -1, index)
        for index, anim in enumerate((ANIM_STAND, ANIM_BIRTH))), 2)

    def keyed(interpolation, keys, fmt):
        return track(blob, interpolation, [keys, keys], fmt)          # the same keys in both animations

    def bone(scaling=None, rotation=None, translation=None):
        record = struct.pack('<iIhHHH', -1, BONE_TRANSFORMED, -1, 0, 0, 0)
        record += translation or empty_track()
        record += rotation or empty_track()
        record += scaling or empty_track()
        return record + struct.pack('<3f', 0, 0, 0)

    stamps, across = ring['scale']
    ring_scale = keyed(1, (stamps, [(yards, yards, 1.0) for yards in across]), '3f')
    flash_scale = keyed(1, ([0, 120, 300], [(0.3, 0.3, 1.0), (1.2, 1.2, 1.0), (1.5, 1.5, 1.0)]), '3f')
    # The spiral: a turn and a half in 0.6 sec (keys every quarter turn), rising 2.5 yards.
    steps = 7
    spin = keyed(1, ([int(600 * i / (steps - 1)) for i in range(steps)],
                     [tuple(comp_quat(c) for c in _turn_z(3 * math.pi * i / (steps - 1))) for i in range(steps)]),
                 '4h')
    rise = keyed(1, ([0, 600], [(0.0, 0.0, 0.2), (0.0, 0.0, 2.7)]), '3f')
    bones = blob.add(bone(scaling=ring_scale) + bone(scaling=flash_scale) + bone(rotation=spin, translation=rise), 3)
    # (bone 2 spins with nothing on it when there is no trail: cheaper than renumbering)
    key_bones = blob.array('h', [-1])
    vertices_ref = blob.add(vertices, vertex_count)

    # Color 0: the ring's tint.
    tint = track(blob, 0, [([0], [tuple(map(float, ring_tint))])], '3f') + track(blob, 0, [([0], [0x7FFF])], 'h')
    colors_ref = blob.add(tint, 1)
    textures = b''
    for blp_name, _ in images:
        path = f'{client_dir}\\{blp_name}.blp'
        ref = blob.add(path.encode() + b'\0', len(path) + 1)
        textures += struct.pack('<II2I', 0, 0, *ref)
    textures_ref = blob.add(textures, len(images))
    ring_fade = keyed(1, ring['fade'], 'h')
    flash_fade = keyed(1, ([0, 120, 300, duration], [0x7FFF, 0x5000, 0, 0]), 'h')
    weights = blob.add(ring_fade + flash_fade, 2)
    replaceable_lookup = blob.array('h', [-1])
    # Materials: the ring alpha-blended (as retail's), the flash and the ribbon additive.
    materials_ref = blob.array('HH', [(UNLIT | TWO_SIDED | NO_DEPTH_WRITE, ring_blend),
                                      (UNLIT | UNFOGGED | TWO_SIDED | NO_DEPTH_WRITE, BLEND_ADD)])
    bone_lookup = blob.array('H', [RING, FLASH])
    texture_lookup = blob.array('H', [RING, FLASH])
    texture_unit_lookup = blob.array('H', [0])
    transparency_lookup = blob.array('H', [0, 1])
    transform_lookup = blob.array('H', [0xFFFF])
    if trail_texture is not None:
        ribbon = _ribbon_record(blob, 2, (1.1, 0.0, 0.0), 0.35, 0.35, 50.0, 0.45, trail_tint, TRAIL, 1)
        ribbons_ref = blob.add(ribbon, 1)
    else:
        ribbons_ref = (0, 0)

    header = struct.pack('<4sI', b'MD20', 264)
    header += struct.pack('<2I', *name_ref) + struct.pack('<I', 0)
    header += struct.pack('<2I', 0, 0)                                              # global sequences
    header += struct.pack('<2I', *sequences) + struct.pack('<2I', 0, 0)            # no sequence lookup
    header += struct.pack('<2I', *bones) + struct.pack('<2I', *key_bones)
    header += struct.pack('<2I', *vertices_ref) + struct.pack('<I', 1)
    header += struct.pack('<2I', *colors_ref) + struct.pack('<2I', *textures_ref)
    header += struct.pack('<2I', *weights) + struct.pack('<2I', 0, 0)               # no texture transforms
    header += struct.pack('<2I', *replaceable_lookup) + struct.pack('<2I', *materials_ref)
    header += struct.pack('<2I', *bone_lookup) + struct.pack('<2I', *texture_lookup)
    header += struct.pack('<2I', *texture_unit_lookup) + struct.pack('<2I', *transparency_lookup)
    header += struct.pack('<2I', *transform_lookup)
    header += struct.pack('<7f', *lo, *hi, radius) + struct.pack('<7f', *[0.0] * 7)
    header += struct.pack('<2I', 0, 0) * 3                                          # collision
    header += struct.pack('<2I', 0, 0) * 2                                          # attachments
    header += struct.pack('<2I', 0, 0) * 2                                          # events, lights
    header += struct.pack('<2I', 0, 0) * 2                                          # cameras
    header += struct.pack('<2I', *ribbons_ref) + struct.pack('<2I', 0, 0)          # ribbons, particles
    assert len(header) == 0x130
    blob.data[:0x130] = header
    (out_dir / f'{name}.m2').write_bytes(bytes(blob.data))

    # Skin: the ring (tinted by color 0, fading by weight 0) and the flash (weight 1).
    skin = Blob(48)
    lookup = skin.array('H', list(range(vertex_count)))
    tris = skin.array('H', triangles)
    props = skin.add(b'\0' * 4 * vertex_count, vertex_count)
    section_data, batch_data = b'', b''
    for i, (first, count, start, index_count, half, bone_index) in enumerate(sections):
        section_data += struct.pack('<10H3f3ff', 0, 0, first, count, start, index_count, 1, bone_index, 1,
                                    bone_index, 0.0, 0.0, 0.15, 0.0, 0.0, 0.15, half * 1.5)
        color, material = (0, 0) if i == RING else (0xFFFF, 1)
        batch_data += struct.pack('<Bb11H', 16, 0, 0, i, i, color, material, 0, 1, i, 0, i, 0)
    sections_ref = skin.add(section_data, len(sections))
    batches_ref = skin.add(batch_data, len(sections))
    skin.data[:48] = struct.pack('<4s2I2I2I2I2II', b'SKIN', *lookup, *tris, *props, *sections_ref, *batches_ref, 21)
    (out_dir / f'{name}00.skin').write_bytes(bytes(skin.data))
    return [f'{name}.m2', f'{name}00.skin'] + [f'{blp_name}.blp' for blp_name, _ in images]


def _round_off(image, inner=0.55):
    """The image faded out toward a circle's edge (from inner of its radius on): no square corners."""
    width, height = image.size

    def fade(x, y):
        distance = math.hypot((x + 0.5) / width - 0.5, (y + 0.5) / height - 0.5) * 2
        return round(255 * min(1.0, max(0.0, (1.0 - distance) / (1.0 - inner))))

    mask = Image.new('L', image.size)
    mask.putdata([fade(x, y) for y in range(height) for x in range(width)])
    out = image.copy()
    out.putalpha(ImageChops.multiply(image.getchannel('A'), mask))
    return out


def ground_zone(*args, layers=()):
    """A zone lying on the ground for as long as the kit keeps it (a persistent area's world slot),
    after Legion's Darkness (cfx_demonhunter_darkness_state): flat squares just above the ground,
    one per layer, each a dict of

    - texture: index into the textures given; size: yards across; height: yards above the ground;
    - tint: red, green, blue (0 to 1) the texture is multiplied by; alpha: its opacity (0 to 1);
    - turn: seconds per turn about the center (negative: the other way; 0: still);
    - round: fade the texture out toward a circle's edge (for tiling textures, square to the edge);
    - blend: BLEND_ALPHA (the default) or BLEND_ADD.

    All unlit. Called as ground_zone(texture, ..., out_dir, name, client_dir,
    layers=...); one looping Stand. Writes <name>.m2, <name>00.skin and its BLPs into out_dir.
    """
    *textures, out_dir, name, client_dir = args
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    images = []
    for index, layer in enumerate(layers):
        with Image.open(textures[layer['texture']]) as image:
            image = image.convert('RGBA')
        if layer.get('round'):
            image = _round_off(image)
        blp_name = f'{name}_{index}'
        (out_dir / f'{blp_name}.blp').write_bytes(blp.encode(image))
        images.append(blp_name)

    vertices, triangles, sections = b'', [], []
    for bone, layer in enumerate(layers):
        half, z = layer['size'] / 2, layer.get('height', 0.1)
        first, start = len(vertices) // 48, len(triangles)
        for x, u in ((-half, 0.0), (half, 1.0)):
            for y, v in ((-half, 1.0), (half, 0.0)):
                vertices += struct.pack('<3f4B4B3f2f2f', x, y, z, 255, 0, 0, 0, bone, 0, 0, 0, 0.0, 0.0, 1.0,
                                        u, v, 0.0, 0.0)
        triangles += [first, first + 1, first + 2, first + 2, first + 1, first + 3]
        sections.append((first, start, half, z))
    vertex_count = len(vertices) // 48
    reach = max(layer['size'] for layer in layers) / 2
    lo, hi = (-reach, -reach, 0.0), (reach, reach, 3.0)
    radius = math.dist(lo, hi) / 2
    duration = 1000

    blob = Blob(0x130)
    name_ref = blob.add(name.encode() + b'\0', len(name) + 1)
    turning = [layer for layer in layers if layer.get('turn')]
    globals_ref = blob.array('I', [int(abs(layer['turn']) * 1000) for layer in turning])
    sequences = blob.add(struct.pack('<HHIfIhHIII6ffhH', ANIM_STAND, 0, duration, 0.0, SEQUENCE_IN_MODEL, 0x7FFF,
                                     0, 0, 0, 0, *lo, *hi, radius, -1, 0), 1)

    bones = b''
    for layer in layers:
        rotation = empty_track()
        if layer.get('turn'):
            period = int(abs(layer['turn']) * 1000)
            way = 1 if layer['turn'] > 0 else -1
            rotation = track(blob, 1, [([period * i // 4 for i in range(5)],
                                        [tuple(comp_quat(c) for c in _turn_z(way * math.pi * i / 2))
                                         for i in range(5)])], '4h')
            rotation = rotation[:2] + struct.pack('<h', turning.index(layer)) + rotation[4:]
        bones += struct.pack('<iIhHHH', -1, BONE_TRANSFORMED, -1, 0, 0, 0)
        bones += empty_track() + rotation + empty_track() + struct.pack('<3f', 0, 0, 0)
    bones_ref = blob.add(bones, len(layers))
    key_bones = blob.array('h', [-1])
    vertices_ref = blob.add(vertices, vertex_count)

    colors = b''
    for layer in layers:
        colors += track(blob, 0, [([0], [tuple(map(float, layer.get('tint', (1.0, 1.0, 1.0))))])], '3f')
        colors += track(blob, 0, [([0], [round(layer.get('alpha', 1.0) * 0x7FFF)])], 'h')
    colors_ref = blob.add(colors, len(layers))
    textures_blob = b''
    for blp_name in images:
        path = f'{client_dir}\\{blp_name}.blp'
        ref = blob.add(path.encode() + b'\0', len(path) + 1)
        textures_blob += struct.pack('<II2I', 0, 0, *ref)
    textures_ref = blob.add(textures_blob, len(images))
    weights = blob.add(track(blob, 0, [([0], [0x7FFF])], 'h'), 1)
    replaceable_lookup = blob.array('h', [-1])
    materials_ref = blob.array('HH', [(UNLIT | TWO_SIDED | NO_DEPTH_WRITE, layer.get('blend', BLEND_ALPHA))
                                      for layer in layers])
    bone_lookup = blob.array('H', list(range(len(layers))))
    texture_lookup = blob.array('H', list(range(len(layers))))
    texture_unit_lookup = blob.array('H', [0])
    transparency_lookup = blob.array('H', [0])
    transform_lookup = blob.array('H', [0xFFFF])

    header = struct.pack('<4sI', b'MD20', 264)
    header += struct.pack('<2I', *name_ref) + struct.pack('<I', 0)
    header += struct.pack('<2I', *globals_ref)
    header += struct.pack('<2I', *sequences) + struct.pack('<2I', 0, 0)            # no sequence lookup
    header += struct.pack('<2I', *bones_ref) + struct.pack('<2I', *key_bones)
    header += struct.pack('<2I', *vertices_ref) + struct.pack('<I', 1)
    header += struct.pack('<2I', *colors_ref) + struct.pack('<2I', *textures_ref)
    header += struct.pack('<2I', *weights) + struct.pack('<2I', 0, 0)               # no texture transforms
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

    # Skin: one section and batch per layer, drawn bottom first.
    skin = Blob(48)
    lookup = skin.array('H', list(range(vertex_count)))
    tris = skin.array('H', triangles)
    props = skin.add(b'\0' * 4 * vertex_count, vertex_count)
    section_data, batch_data = b'', b''
    for i, (first, start, half, z) in enumerate(sections):
        section_data += struct.pack('<10H3f3ff', 0, 0, first, 4, start, 6, 1, i, 1, i, 0.0, 0.0, z, 0.0, 0.0, z,
                                    half * 1.5)
        batch_data += struct.pack('<Bb11H', 16, i, 0, i, i, i, i, 0, 1, i, 0, 0, 0)
    sections_ref = skin.add(section_data, len(sections))
    batches_ref = skin.add(batch_data, len(sections))
    skin.data[:48] = struct.pack('<4s2I2I2I2I2II', b'SKIN', *lookup, *tris, *props, *sections_ref, *batches_ref, 21)
    (out_dir / f'{name}00.skin').write_bytes(bytes(skin.data))
    return [f'{name}.m2', f'{name}00.skin'] + [f'{blp_name}.blp' for blp_name in images]


def exported_model(*args, export, base, size=None, scale=1.0, center=False, offset=(0.0, 0.0, 0.0), turn=0.0,
                   trail_height=0.3, lifetime=0.4, duration=1000, fade=None, two_sided=False, max_texture=None,
                   sequence=None):
    """A spell effect model from a wow.export model (OBJ + JSON + PNGs in the folder export): a spell
    missile (SpellVisual's missile model) or a model a kit attaches (chest, head...).

    Called as exported_model([trail texture,] out_dir, name, client_dir, export=..., base=...):

    - the model scaled to size yards across (or by scale), its middle moved to the origin if center, then
      moved by offset (yards: x forward, y left, z up), turning about Z every turn seconds (0: still); each
      texture unit with its first texture (at most max_texture pixels a side), its material (retail's blend
      mode 7 drawn additive; two_sided: both faces), its tint and its scrolling (texture transforms, on
      global sequences; their lengths are the last key's time, wow.export not exporting them);
    - with a trail texture, a ribbon (laid out as ribbon_trail's) spanning trail_height yards above and
      below its center, each stretch fading after lifetime seconds.

    One Stand animation of duration milliseconds (a kit removes a model once it has played); fade: the
    milliseconds over which it fades out from the start (None: whole all along). Layers' own fades in
    and out (per-sequence alpha) are left out, unless sequence names the export's animation (its index)
    whose layer alpha (flashes, pulses) and texture weights the Stand plays. Writes <name>.m2,
    <name>00.skin and its BLPs into out_dir.
    """
    *textures, out_dir, name, client_dir = args
    trail_texture = textures[0] if textures else None
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    source = Path(export)
    info = json.loads((source / f'{base}.json').read_text())
    positions, normals, uvs, groups = read_obj(source / f'{base}.obj')
    lo = [min(p[i] for p in positions) for i in range(3)]
    hi = [max(p[i] for p in positions) for i in range(3)]
    factor = size / max(h - l for l, h in zip(lo, hi)) if size else scale
    middle = [(l + h) / 2 if center else 0.0 for l, h in zip(lo, hi)]
    positions = [tuple((c - m) * factor + o for c, m, o in zip(p, middle, offset)) for p in positions]

    # Textures: the ones the texture units draw, then the trail.
    units = info['skin']['textureUnits']
    used = sorted({info['textureCombos'][unit['textureComboIndex']] for unit in units})
    blp_names, texture_index = [], {}
    for texture in used:
        png = info['textures'][texture]['fileNameExternal']
        blp_name = f'{name}_{len(blp_names)}'
        with Image.open(source / png) as image:
            image = image.convert('RGBA')
            if max_texture and max(image.size) > max_texture:
                ratio = max_texture / max(image.size)
                image = image.resize((int(image.width * ratio), int(image.height * ratio)), Image.LANCZOS)
            (out_dir / f'{blp_name}.blp').write_bytes(blp.encode(image))
        texture_index[texture] = len(blp_names)
        blp_names.append(blp_name)
    trail_index = None
    if trail_texture:
        with Image.open(trail_texture) as image:
            trail = image.convert('RGBA').transpose(Image.Transpose.TRANSPOSE)
            trail = trail.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            trail.putalpha(trail.getchannel('A').point(lambda a: 0 if a < FAINT else a))
        trail_index = len(blp_names)
        blp_names.append(f'{name}_Trail')
        (out_dir / f'{blp_names[-1]}.blp').write_bytes(blp.encode(trail))

    blob = Blob(0x130)
    name_ref = blob.add(name.encode() + b'\0', len(name) + 1)
    transforms = info['textureTransforms']
    lengths = [max(1, t['translation']['timestamps'][0][-1]) for t in transforms] + [int(abs(turn) * 1000) or 1]
    globals_ref = blob.array('I', lengths)
    SPIN_SEQUENCE = len(transforms)

    reach = max(max(abs(c) for c in p) for p in positions) + (trail_height if trail_texture else 0.0)
    box_lo, box_hi = (-reach,) * 3, (reach,) * 3
    radius = reach * math.sqrt(3)
    sequences = blob.add(struct.pack('<HHIfIhHIII6ffhH', ANIM_STAND, 0, duration, 0.0, SEQUENCE_IN_MODEL,
                                     0x7FFF, 0, 0, 0, 0, *box_lo, *box_hi, radius, -1, 0), 1)

    # Bones: 0 the model, turning (or still); 1 the trail, still.
    spin = empty_track()
    if turn:
        spin = track(blob, 1, [([lengths[SPIN_SEQUENCE] * i // 4 for i in range(5)],
                                [tuple(comp_quat(c) for c in _turn_z(math.pi * i / 2)) for i in range(5)])], '4h')
        spin = spin[:2] + struct.pack('<h', SPIN_SEQUENCE) + spin[4:]
    bones = b''
    for rotation in (spin, empty_track()):
        bones += struct.pack('<iIhHHH', -1, BONE_TRANSFORMED, -1, 0, 0, 0)
        bones += empty_track() + rotation + empty_track() + struct.pack('<3f', 0, 0, 0)
    bones_ref = blob.add(bones, 2)
    key_bones = blob.array('h', [-1])

    vertices = b''
    for p, n, uv in zip(positions, normals, uvs):
        vertices += struct.pack('<3f4B4B3f2f2f', *p, 255, 0, 0, 0, 0, 0, 0, 0, *n, *uv, 0.0, 0.0)
    vertices_ref = blob.add(vertices, len(positions))

    colors = b''
    for color in info['colors']:
        colors += track(blob, 0, [([0], [tuple(map(float, color['color']['values'][0][0]))])], '3f')
        alpha = color['alpha']
        keys = ([0], [0x7FFF])
        if sequence is not None and alpha['timestamps'][sequence]:
            keys = (alpha['timestamps'][sequence], [max(0, v) for v in alpha['values'][sequence]])
        colors += track(blob, alpha['interpolation'] if len(keys[0]) > 1 else 0, [keys], 'h')
    colors_ref = blob.add(colors, len(info['colors']))
    textures_blob = b''
    for i, blp_name in enumerate(blp_names):
        path = f'{client_dir}\\{blp_name}.blp'
        ref = blob.add(path.encode() + b'\0', len(path) + 1)
        textures_blob += struct.pack('<II2I', 0, 0 if i == trail_index else WRAP_U | 0x2, *ref)
    textures_ref = blob.add(textures_blob, len(blp_names))
    levels = [0x7FFF]
    if sequence is not None:
        levels = [weight['values'][0][0] for weight in info['textureWeights']]
    weights = b''.join(track(blob, 1, [([0, fade], [level, 0]) if fade else ([0], [level])], 'h')
                       for level in levels)
    weights = blob.add(weights, len(levels))
    transform_records = b''
    for index, transform in enumerate(transforms):
        move = transform['translation']
        translation = track(blob, move['interpolation'], [(move['timestamps'][0],
                                                            [tuple(v) for v in move['values'][0]])], '3f')
        transform_records += translation[:2] + struct.pack('<h', index) + translation[4:]
        transform_records += empty_track() + empty_track()
    transforms_ref = blob.add(transform_records, len(transforms))
    replaceable_lookup = blob.array('h', [-1])
    materials = [((m['flags'] & 0x1F) | (TWO_SIDED if two_sided else 0),
                  BLEND_ADD if m['blendingMode'] > 6 else m['blendingMode']) for m in info['materials']]
    trail_material = len(materials)
    materials.append((UNLIT | TWO_SIDED | NO_DEPTH_WRITE, BLEND_ALPHA))
    materials_ref = blob.array('HH', materials)
    bone_lookup = blob.array('H', [0])
    texture_lookup = blob.array('H', [texture_index.get(t, 0) for t in info['textureCombos']])
    texture_unit_lookup = blob.array('H', [0])
    transparency_lookup = blob.array('H', info['transparencyLookup'] if sequence is not None else [0])
    transform_lookup = blob.array('H', info['textureTransformsLookup'])
    ribbons_ref = (0, 0)
    if trail_texture:
        ribbon = _ribbon_record(blob, 1, (0.0, 0.0, 0.0), trail_height, trail_height, 50.0, lifetime,
                                (1.0, 1.0, 1.0), trail_index, trail_material)
        ribbons_ref = blob.add(ribbon, 1)

    header = struct.pack('<4sI', b'MD20', 264)
    header += struct.pack('<2I', *name_ref) + struct.pack('<I', 0)
    header += struct.pack('<2I', *globals_ref)
    header += struct.pack('<2I', *sequences) + struct.pack('<2I', 0, 0)            # no sequence lookup
    header += struct.pack('<2I', *bones_ref) + struct.pack('<2I', *key_bones)
    header += struct.pack('<2I', *vertices_ref) + struct.pack('<I', 1)
    header += struct.pack('<2I', *colors_ref) + struct.pack('<2I', *textures_ref)
    header += struct.pack('<2I', *weights) + struct.pack('<2I', *transforms_ref)
    header += struct.pack('<2I', *replaceable_lookup) + struct.pack('<2I', *materials_ref)
    header += struct.pack('<2I', *bone_lookup) + struct.pack('<2I', *texture_lookup)
    header += struct.pack('<2I', *texture_unit_lookup) + struct.pack('<2I', *transparency_lookup)
    header += struct.pack('<2I', *transform_lookup)
    header += struct.pack('<7f', *box_lo, *box_hi, radius) + struct.pack('<7f', *[0.0] * 7)
    header += struct.pack('<2I', 0, 0) * 3                                          # collision
    header += struct.pack('<2I', 0, 0) * 2                                          # attachments
    header += struct.pack('<2I', 0, 0) * 2                                          # events, lights
    header += struct.pack('<2I', 0, 0) * 2                                          # cameras
    header += struct.pack('<2I', *ribbons_ref) + struct.pack('<2I', 0, 0)          # ribbons, particles
    assert len(header) == 0x130
    blob.data[:0x130] = header
    (out_dir / f'{name}.m2').write_bytes(bytes(blob.data))

    # Skin: the export's submeshes (one per OBJ group) and texture units.
    skin = Blob(48)
    lookup = skin.array('H', list(range(len(positions))))
    triangles, section_data = [], b''
    for submesh, faces in zip(info['skin']['subMeshes'], groups):
        start = len(triangles)
        for face in faces:
            triangles += face
        center = tuple(c * factor for c in submesh['centerPosition'])
        section_data += struct.pack('<10H3f3ff', 0, 0, submesh['vertexStart'], submesh['vertexCount'], start,
                                    len(triangles) - start, 1, 0, 1, 0, *center, *center,
                                    submesh['sortRadius'] * factor)
    tris = skin.array('H', triangles)
    props = skin.add(b'\0' * 4 * len(positions), len(positions))
    batch_data = b''
    for unit in units:
        batch_data += struct.pack('<Bb11H', 16, unit['priority'], 0, unit['skinSectionIndex'],
                                  unit['skinSectionIndex'], unit['colorIndex'], unit['materialIndex'], 0, 1,
                                  unit['textureComboIndex'], 0,
                                  unit['textureWeightComboIndex'] if sequence is not None else 0,
                                  unit['textureTransformComboIndex'])
    sections_ref = skin.add(section_data, len(groups))
    batches_ref = skin.add(batch_data, len(units))
    skin.data[:48] = struct.pack('<4s2I2I2I2I2II', b'SKIN', *lookup, *tris, *props, *sections_ref, *batches_ref, 21)
    (out_dir / f'{name}00.skin').write_bytes(bytes(skin.data))
    return [f'{name}.m2', f'{name}00.skin'] + [f'{blp_name}.blp' for blp_name in blp_names]
