#!/usr/bin/env python3
"""Effects designed in the Emberforge Effect Suite (tools/effect-suite) as 3.3.5a models (M2 264).

suite_effect() turns the impact layers of a Suite spec (its Export/Save .json) into one model for a spell visual kit's
slot, played once from the moment it lands: slot 'chest' (KIT_CHEST) takes the layers at the target's chest or the
impact point, slot 'base' (KIT_BASE) those at its feet (ground decals lie on the ground there):

- a Ground layer is a card lying on the ground, its turn and spin about the vertical;
- a Billboard layer is a camera-facing card per copy, on its own bone: its size over its time (the Suite's ease out),
  its colors and fades in its color track, its flipbook frame by frame in its texture transform, its turn in the card;
- a Particles layer is a real particle emitter (a plane emitter on a bone turned to the layer's direction): its burst
  (or stream) over the layer's time, speeds, cone, gravity, drag, life, sizes and colors over a particle's life;
- Casts light is a point light (the client may not light the world with a spell model's lights: a soft extra);
- Scene light has no equivalent (the game's spell cards are unlit) and is left out; brightness above 1 is too.

Model space: X forward (toward the caster, the target's facing), Y left, Z up; the Suite's stage is X toward the dummy,
Y up, Z to the Hero's right, and its target.chest anchor 0.3 yd in front of the chest, so a stage point (x, y, z) from
the chest is (0.3 - x, z, y) here (from the feet: (-x, z, y); a decal's height is from the ground). Layers on other anchors or triggers are skipped with a note. Textures are the spec's
own (data URIs) or the Suite's built-in ones (spells/suite, exported from the Suite).
"""

import base64
import io
import json
import math
import struct
from pathlib import Path

from PIL import Image

import blp
from spell_effects import (ANIM_STAND, BLEND_ADD, BLEND_ALPHA, BONE_BILLBOARD, BONE_TRANSFORMED, NO_DEPTH_WRITE,
                           TWO_SIDED, UNLIT, comp_quat)
from wowexport_to_m2 import SEQUENCE_IN_MODEL, Blob, empty_track, track

BUILTIN = Path(__file__).resolve().parent.parent / 'spells' / 'suite'
FIXED = 0x7FFF
FRONT = 0.3
SLOTS = {'chest': ({'target.chest', 'impact'}, ('sprite', 'particles')),
         'base': ({'target.base'}, ('ground', 'sprite', 'particles'))}
SAMPLES = 12                    # keys over a layer's time (its size, colors and fades)
PARTICLE_FLAGS = 0x40029        # as Blizzard's impact sparks (Ice_ImpactDD_Med_Chest)
EMITTER_PLANE = 1
INTERP_NONE, INTERP_LINEAR = 0, 1


def _euler_yzx(degrees):
    """three.js's quaternion (x, y, z, w) of Euler angles in degrees, order YZX (the Suite's rotations)."""
    x, y, z = (math.radians(a) / 2 for a in degrees)
    c1, c2, c3, s1, s2, s3 = math.cos(x), math.cos(y), math.cos(z), math.sin(x), math.sin(y), math.sin(z)
    return (s1 * c2 * c3 + c1 * s2 * s3, c1 * s2 * c3 + s1 * c2 * s3, c1 * c2 * s3 - s1 * s2 * c3,
            c1 * c2 * c3 - s1 * s2 * s3)


def _qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def _rotate(q, v):
    x, y, z, w = _qmul(_qmul(q, (*v, 0.0)), (-q[0], -q[1], -q[2], q[3]))
    return (x, y, z)


def _to_model(v):
    return (-v[0], v[2], v[1])


def _z_to(direction):
    """The quaternion turning +Z to direction (a unit vector)."""
    dx, dy, dz = direction
    dot = dz
    if dot < -0.9999:
        return (1.0, 0.0, 0.0, 0.0)
    axis = (-dy, dx, 0.0)                       # (0, 0, 1) x direction
    w = 1.0 + dot
    n = math.sqrt(axis[0] ** 2 + axis[1] ** 2 + w * w)
    return (axis[0] / n, axis[1] / n, 0.0, w / n)


def _color(hex_color):
    h = hex_color.lstrip('#')
    return tuple(int(h[i:i + 2], 16) / 255 for i in (0, 2, 4))


def _fade(layer, f):
    fi, fo, a = layer.get('fadeIn', 0) or 0, layer.get('fadeOut', 0) or 0, 1.0
    if fi > 0 and f < fi:
        a = f / fi
    if fo > 0 and f > 1 - fo:
        a = min(a, (1 - f) / fo)
    return max(0.0, min(1.0, a))


def _texture(spec, name):
    """(image, columns, rows) of a texture the spec uses: its own, else the Suite's built-in one."""
    own = (spec.get('textures') or {}).get(name)
    if own:
        image = Image.open(io.BytesIO(base64.b64decode(own['data'].split(',', 1)[1])))
        return image.convert('RGBA'), own.get('cols', 1), own.get('rows', 1)
    meta = json.loads((BUILTIN / 'textures.json').read_text()).get(name)
    if not meta:
        raise FileNotFoundError(f'{name}: not in the spec and not a built-in Suite texture')
    with Image.open(BUILTIN / f'{name}.png') as image:
        return image.convert('RGBA'), meta['cols'], meta['rows']


def _blp(image, most=1024):
    if max(image.size) > most:
        k = most / max(image.size)
        image = image.resize((max(1, int(image.width * k)), max(1, int(image.height * k))), Image.Resampling.LANCZOS)
    pow2 = tuple(1 << (n - 1).bit_length() for n in image.size)
    if pow2 != image.size:
        image = image.resize(pow2, Image.Resampling.LANCZOS)
    return blp.encode(image)


def _z_turn(degrees):
    a = math.radians(degrees) / 2
    return (0.0, 0.0, math.sin(a), math.cos(a))


def suite_effect(out_dir, name, client_dir, spec, slot='chest'):
    """spec: the Suite's .json (a path). Writes <name>.m2, <name>00.skin and its BLPs into out_dir; returns their
    names. Called as an EFFECTS entry's builder (shape 'suite', options {'spec': ..., 'slot': 'chest' or 'base'})."""
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    data = json.loads(Path(spec).read_text())
    xf = data.get('transform') or {}
    turn_all = _euler_yzx(xf.get('rotation') or (0, 0, 0))
    shift_all, size_all = xf.get('offset') or (0, 0, 0), xf.get('scale', 1) or 1

    anchors, kinds = SLOTS[slot]
    layers = []
    for layer in data['layers']:
        if layer.get('enabled') is False or layer['type'] == 'sound':
            continue
        kind = layer['type']
        if layer.get('trigger') != 'impact' or layer.get('anchor') not in anchors or kind not in kinds:
            print(f'  {name} ({slot}): "{layer.get("name")}" ({kind}, {layer.get("trigger")}, {layer.get("anchor")}) '
                  f'left out: this model takes the impact {", ".join(kinds)} at {", ".join(sorted(anchors))}')
            continue
        layers.append(layer)
    if not layers:
        raise ValueError(f'{spec}: no impact layers for the {slot}')

    def place(layer):
        """The layer's origin in model space, from the chest attachment."""
        turn = turn_all
        offset = _rotate(turn, layer.get('offset') or (0, 0, 0))
        stage = tuple(shift_all[i] + offset[i] for i in range(3))
        x, y, z = _to_model(stage)
        if layer['type'] == 'ground':               # on the ground: its height is the offset's
            return (x, y, 0.02 + stage[1])
        return (x + (FRONT if slot == 'chest' else 0.0), y, z)

    def orient(layer):
        return _qmul(turn_all, _euler_yzx(layer.get('orient') or (0, 0, 0)))

    duration = 0.0
    for layer in layers:
        end = layer.get('delay', 0) + layer.get('life', 0.5)
        if layer['type'] == 'particles':
            end += layer.get('partLifeMax', 0.6)
        duration = max(duration, end)
    duration_ms = int(math.ceil(duration * 1000)) + 50

    # Textures: one per name used, in order.
    texture_names, textures = [], {}
    for layer in layers:
        if layer['texture'] not in textures:
            textures[layer['texture']] = _texture(data, layer['texture'])
            texture_names.append(layer['texture'])
    files, particle_only = [], {layer['texture'] for layer in layers if layer['type'] == 'particles'} - {
        layer['texture'] for layer in layers if layer['type'] != 'particles'}
    for index, tex in enumerate(texture_names):
        image, columns, rows = textures[tex]
        most = 256 if tex in particle_only or columns * rows == 1 else min(1024, 256 * max(columns, rows))  # a cell
        (out_dir / f'{name}_{index}.blp').write_bytes(_blp(image, most))
        files.append(f'{name}_{index}.blp')

    blob = Blob(0x130)
    bones = struct.pack('<iIhHHH', -1, 0, -1, 0, 0, 0) + empty_track() * 3 + struct.pack('<3f', 0, 0, 0)   # root
    bone_count = 1
    vertices, triangles = b'', []
    cards = []                       # (layer, bone, texture index, pivot, half extents)
    colors, transforms, emitters, lights = b'', b'', b'', b''
    color_count = transform_count = emitter_count = light_count = 0
    lo, hi = [-1.0, -1.0, -1.0], [1.0, 1.0, 1.0]

    def stretch_box(p, r):
        for i in range(3):
            lo[i], hi[i] = min(lo[i], p[i] - r), max(hi[i], p[i] + r)

    def keyed(start, life, value):
        """Keys over a layer's time: value(f) for f in 0..1 at its samples, 0-sized / clear before and after."""
        s, e = int(round(start * 1000)), int(round((start + life) * 1000))
        times, values = [], []
        if s > 0:
            times.append(max(0, s - 1))
            values.append(None)
        for i in range(SAMPLES + 1):
            times.append(s + round((e - s) * i / SAMPLES))
            values.append(value(i / SAMPLES))
        times.append(e + 1)
        values.append(None)
        pairs, seen = [], set()
        for t, v in zip(times, values):
            if t not in seen:
                seen.add(t)
                pairs.append((t, v))
        return pairs

    for layer in layers:
        start, life = layer.get('delay', 0) or 0, max(0.02, layer.get('life', 0.5) or 0.5)
        k = (layer.get('scale', 1) or 1) * size_all
        opacity, bright = layer.get('opacity', 1), min(1.0, layer.get('brightness', 1) or 1)
        c1, c2 = _color(layer.get('color', '#ffffff')), _color(layer.get('color2') or layer.get('color', '#ffffff'))
        blend = BLEND_ALPHA if layer.get('blend') == 'normal' else BLEND_ADD
        tex_index = texture_names.index(layer['texture'])
        image, columns, rows = textures[layer['texture']]
        origin, q = place(layer), orient(layer)

        if layer['type'] in ('sprite', 'ground'):
            flat = layer['type'] == 'ground'
            aspect = (image.width / columns) / (image.height / rows)
            ratio = (layer.get('stretch', 1) or 1) * aspect
            roll = math.radians(layer.get('rotation', 0) or 0)
            count = max(1, int(layer.get('count', 1) or 1))
            frames = columns * rows
            for copy in range(count):
                a = 2 * math.pi * copy / count
                ring = _to_model(_rotate(q, (math.cos(a) * (layer.get('radius', 0) or 0) * k, 0.0,
                                             math.sin(a) * (layer.get('radius', 0) or 0) * k)))
                pivot = tuple(origin[i] + ring[i] for i in range(3))
                bone = bone_count
                first = len(vertices) // 48
                for y, u in ((-ratio / 2, 0.0), (ratio / 2, 1.0 / columns)):
                    for z, v in ((-0.5, 1.0 / rows), (0.5, 0.0)):
                        yy = y * math.cos(roll) - z * math.sin(roll)        # Y screen right, Z up: counterclockwise
                        zz = y * math.sin(roll) + z * math.cos(roll)
                        at = (pivot[0] + yy, pivot[1] + zz, pivot[2] + copy * 0.002) if flat else (
                            pivot[0], pivot[1] + yy, pivot[2] + zz)
                        vertices += struct.pack('<3f4B4B3f2f2f', *at, 255, 0, 0, 0, bone, 0, 0, 0,
                                                *((0.0, 0.0, 1.0) if flat else (1.0, 0.0, 0.0)), u, v, 0.0, 0.0)
                triangles += [first, first + 1, first + 2, first + 2, first + 1, first + 3]

                def size_at(f):
                    eased = 1 - (1 - f) ** 2
                    return (layer.get('sizeStart', 1) + (layer.get('sizeEnd', 1) - layer.get('sizeStart', 1)) * eased) * k
                pairs = keyed(start, life, size_at)
                scaling = track(blob, INTERP_LINEAR, [([t for t, _ in pairs],
                                                       [(0.0,) * 3 if v is None else (float(v),) * 3 for _, v in pairs])], '3f')
                rise = 0 if flat else layer.get('rise', 0) or 0
                moving = empty_track()
                if rise:
                    up = _to_model(_rotate(q, (0.0, rise * k, 0.0)))
                    pairs_r = keyed(start, life, lambda f: f)
                    moving = track(blob, INTERP_LINEAR, [([t for t, _ in pairs_r],
                                                          [tuple(up[i] * (v or 0.0) for i in range(3)) for _, v in pairs_r])], '3f')
                spinning = empty_track()
                if flat and layer.get('spin'):              # degrees a second about the vertical, over its time
                    pairs_s = keyed(start, life, lambda f: f)
                    spinning = track(blob, INTERP_LINEAR, [([t for t, _ in pairs_s], [
                        tuple(comp_quat(c) for c in _z_turn(layer['spin'] * life * (v or 0.0))) for _, v in pairs_s])], '4h')
                bones += struct.pack('<iIhHHH', -1, BONE_TRANSFORMED | (0 if flat else BONE_BILLBOARD), 0, 0, 0, 0)
                bones += moving + spinning + scaling + struct.pack('<3f', *pivot)
                bone_count += 1
                reach = max(layer.get('sizeStart', 1), layer.get('sizeEnd', 1)) * k * max(1.0, ratio)
                stretch_box(pivot, reach + abs(rise))

                # Its color and fades; its flipbook.
                pairs = keyed(start, life, lambda f: f)
                rgb = [(0.0, 0.0, 0.0) if v is None else tuple(min(1.0, (c1[i] + (c2[i] - c1[i]) * v) * bright)
                                                               for i in range(3)) for _, v in pairs]
                alpha = [0 if v is None else round(FIXED * max(0.0, min(1.0, opacity * _fade(layer, v)))) for _, v in pairs]
                times = [t for t, _ in pairs]
                colors += track(blob, INTERP_LINEAR, [(times, rgb)], '3f') + track(blob, INTERP_LINEAR, [(times, alpha)], 'h')
                color_count += 1

                def cell(frame):
                    frame %= frames
                    return ((frame % columns) / columns, (frame // columns) / rows, 0.0)
                if frames > 1:
                    s = int(round(start * 1000))
                    if layer.get('flipOnce'):
                        keys = [(s + round(life * 1000 * i / frames), cell(i)) for i in range(frames)]
                    else:
                        fps = max(1.0, layer.get('fps', 16) or 16)
                        n = max(1, int(life * fps) + 1)
                        keys = [(s + round(1000 * i / fps), cell(i + copy * 3)) for i in range(n)]
                    if keys[0][0] > 0:
                        keys.insert(0, (0, keys[0][1]))
                    flip = track(blob, INTERP_NONE, [([t for t, _ in keys], [c for _, c in keys])], '3f')
                else:
                    flip = track(blob, INTERP_NONE, [([0], [cell(0)])], '3f')
                transforms += flip + empty_track() + empty_track()
                transform_count += 1
                cards.append((layer, bone, tex_index, blend, pivot, max(ratio, 1.0) / 2))
        else:
            # Particles: a plane emitter on a bone turned so its +Z is the layer's aim.
            base = {'up': (0, 1, 0), 'down': (0, -1, 0), 'forward': (1, 0, 0), 'back': (-1, 0, 0),
                    'ring': (0, 1, 0)}.get(layer.get('shape'), (0, 1, 0))
            aim = _to_model(_rotate(q, base))
            n = math.sqrt(sum(c * c for c in aim)) or 1.0
            aim = tuple(c / n for c in aim)
            cone = math.pi if layer.get('shape', 'sphere') == 'sphere' else (
                math.pi / 2 if layer.get('shape') == 'ring' else math.radians(layer.get('spread', 30) or 30))
            bone = bone_count
            rotation = track(blob, INTERP_NONE, [([0], [tuple(comp_quat(c) for c in _z_to(aim))])], '4h')
            bones += struct.pack('<iIhHHH', -1, BONE_TRANSFORMED, 0, 0, 0, 0)
            bones += empty_track() + rotation + empty_track() + struct.pack('<3f', *origin)
            bone_count += 1
            lo_s, hi_s = layer.get('speedMin', 3) or 0, layer.get('speedMax', 8) or 0
            speed = (lo_s + hi_s) / 2
            vary = (hi_s - lo_s) / 2 / speed if speed else 0.0
            lo_l, hi_l = layer.get('partLifeMin', 0.4), layer.get('partLifeMax', 0.9)
            p_life = (lo_l + hi_l) / 2
            s, e = int(round(start * 1000)), int(round((start + life) * 1000))
            if layer.get('mode', 'burst') == 'burst':
                burst_ms = 40
                rate = (layer.get('count', 30) or 30) / (burst_ms / 1000)
                rate_keys = ([0, s, s + burst_ms] if s else [0, burst_ms], [0.0, rate, 0.0] if s else [rate, 0.0])
            else:
                rate = layer.get('rate', 40) or 40
                rate_keys = ([0, s, e] if s else [0, e], [0.0, rate, 0.0] if s else [rate, 0.0])

            def ft(value):
                return track(blob, INTERP_NONE, [([0], [float(value)])], 'f')

            def fblock(fmt, times, values):
                t = blob.array('H', [round(x * FIXED) for x in times])
                v = blob.array(fmt, values)
                return struct.pack('<2I2I', *t, *v)
            area = (layer.get('emitRadius', 0) or 0) * k * 1.6
            frames = columns * rows
            fades = [0.0, 0.5, 1.0] if not (layer.get('fadeIn') or layer.get('fadeOut')) else sorted(
                {0.0, 1.0, layer.get('fadeIn', 0) or 0.0, 1 - (layer.get('fadeOut', 0) or 0.0)})
            emitter = struct.pack('<iI3fHH', -1, PARTICLE_FLAGS, *origin, bone, tex_index)
            emitter += struct.pack('<4I', 0, 0, 0, 0)
            emitter += struct.pack('<BBhBBhHH', 4 if blend == BLEND_ADD else 2, EMITTER_PLANE, 0, 0, 0, 0, rows, columns)
            assert len(emitter) == 0x34
            emitter += ft(speed) + ft(vary) + ft(cone) + ft(math.pi) + ft(-(layer.get('gravity', 0) or 0)) + ft(p_life)
            emitter += struct.pack('<f', (hi_l - lo_l) / 2 / p_life if p_life else 0.0)
            emitter += track(blob, INTERP_NONE, [rate_keys], 'f') + struct.pack('<f', 0.0)
            emitter += ft(area) + ft(area) + ft(0.0)
            assert len(emitter) == 0x104
            emitter += fblock('3f', [0.0, 1.0], [tuple(c * 255 * bright for c in c1), tuple(c * 255 * bright for c in c2)])
            emitter += fblock('h', fades, [round(FIXED * max(0.0, min(1.0, opacity * _fade(layer, f)))) for f in fades])
            half0, half1 = layer.get('sizeStart', 0.3) * k / 2, layer.get('sizeEnd', 0.1) * k / 2
            emitter += fblock('2f', [0.0, 1.0], [(half0, half0), (half1, half1)]) + struct.pack('<2f', 0.3, 0.3)
            emitter += (fblock('H', [0.0, 1.0], [0, frames - 1]) if frames > 1 else struct.pack('<4I', 0, 0, 0, 0))
            emitter += struct.pack('<4I', 0, 0, 0, 0)
            emitter += struct.pack('<5f', 0.1, 10.0, 1.0, 1.0, 1.0)                  # tail length, twinkle as Blizzard's
            emitter += struct.pack('<6f', 1.0, layer.get('drag', 0) or 0.0, 0.0, math.pi,
                                   math.radians(layer.get('spin', 0) or 0), 0.0)
            emitter += struct.pack('<14f', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2.5, 0.7, 7.0, 0.9)
            emitter += struct.pack('<2I', 0, 0)
            emitter += track(blob, INTERP_NONE, [([0], [1])], 'B')
            assert len(emitter) == 476
            emitters += emitter
            emitter_count += 1
            reach = hi_s * hi_l + area
            stretch_box(origin, reach)

        if layer.get('glow'):
            lc = _color(layer.get('lightColor') or layer.get('color', '#ffffff'))
            pairs = keyed(start, life, lambda f: f)
            times = [t for t, _ in pairs]
            power = [0.0 if v is None else (layer.get('lightIntensity', 2) or 0) * 0.4 * opacity * _fade(layer, v)
                     for _, v in pairs]
            reach = layer.get('lightRange', 6) or 6
            lights += struct.pack('<hh3f', 1, 0, *origin)
            lights += track(blob, INTERP_NONE, [([0], [(0.0, 0.0, 0.0)])], '3f') + track(blob, INTERP_NONE, [([0], [0.0])], 'f')
            lights += track(blob, INTERP_NONE, [([0], [lc])], '3f') + track(blob, INTERP_LINEAR, [(times, power)], 'f')
            lights += track(blob, INTERP_NONE, [([0], [reach * 0.1])], 'f') + track(blob, INTERP_NONE, [([0], [reach])], 'f')
            lights += track(blob, INTERP_NONE, [(times, [0 if v is None else 1 for _, v in pairs])], 'B')
            light_count += 1

    radius = math.dist(lo, hi) / 2
    name_ref = blob.add(name.encode() + b'\0', len(name) + 1)
    globals_ref = blob.array('I', [1])
    sequences = blob.add(struct.pack('<HHIfIhHIII6ffhH', ANIM_STAND, 0, duration_ms, 0.0, SEQUENCE_IN_MODEL, 0x7FFF,
                                     0, 0, 0, 0, *lo, *hi, radius, -1, 0), 1)
    bones_ref = blob.add(bones, bone_count)
    key_bones = blob.array('h', [-1])
    vertex_count = len(vertices) // 48
    vertices_ref = blob.add(vertices, vertex_count)
    colors_ref = blob.add(colors, color_count)
    textures_blob = b''
    for index in range(len(texture_names)):
        path = f'{client_dir}\\{name}_{index}.blp'
        ref = blob.add(path.encode() + b'\0', len(path) + 1)
        textures_blob += struct.pack('<II2I', 0, 0, *ref)
    textures_ref = blob.add(textures_blob, len(texture_names))
    weights = blob.add(track(blob, INTERP_NONE, [([0], [FIXED])], 'h'), 1)
    transforms_ref = blob.add(transforms, transform_count)
    replaceable_lookup = blob.array('h', [-1])
    materials_ref = blob.array('HH', [(UNLIT | TWO_SIDED | NO_DEPTH_WRITE, card[3]) for card in cards] or [(UNLIT, BLEND_ADD)])
    bone_lookup = blob.array('H', list(range(bone_count)))
    texture_lookup = blob.array('H', list(range(len(texture_names))))
    texture_unit_lookup = blob.array('H', [0])
    transparency_lookup = blob.array('H', [0])
    transform_lookup = blob.array('H', list(range(max(1, transform_count))))
    lights_ref = blob.add(lights, light_count)
    emitters_ref = blob.add(emitters, emitter_count)

    header = struct.pack('<4sI', b'MD20', 264)
    header += struct.pack('<2I', *name_ref) + struct.pack('<I', 0)
    header += struct.pack('<2I', *globals_ref)
    header += struct.pack('<2I', *sequences) + struct.pack('<2I', 0, 0)
    header += struct.pack('<2I', *bones_ref) + struct.pack('<2I', *key_bones)
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
    header += struct.pack('<2I', 0, 0) + struct.pack('<2I', *lights_ref)            # events, lights
    header += struct.pack('<2I', 0, 0) * 2                                          # cameras
    header += struct.pack('<2I', 0, 0) + struct.pack('<2I', *emitters_ref)          # ribbons, particles
    assert len(header) == 0x130
    blob.data[:0x130] = header
    (out_dir / f'{name}.m2').write_bytes(bytes(blob.data))

    # Skin: a section and a batch per card, on its bone, with its texture, material, color and flipbook.
    skin = Blob(48)
    lookup = skin.array('H', list(range(vertex_count)))
    tris = skin.array('H', triangles)
    props = skin.add(b'\0' * 4 * vertex_count, vertex_count)
    section_data, batch_data = b'', b''
    for number, (_, bone, tex_index, _, pivot, half) in enumerate(cards):
        section_data += struct.pack('<10H3f3ff', 0, 0, number * 4, 4, number * 6, 6, 1, bone, 1, bone, *pivot, *pivot,
                                    half * 2)
        batch_data += struct.pack('<Bb11H', 16, number, 0, number, number, number, number, 0, 1, tex_index, 0, 0, number)
    sections_ref = skin.add(section_data, len(cards))
    batches_ref = skin.add(batch_data, len(cards))
    skin.data[:48] = struct.pack('<4s2I2I2I2I2II', b'SKIN', *lookup, *tris, *props, *sections_ref, *batches_ref, 21)
    (out_dir / f'{name}00.skin').write_bytes(bytes(skin.data))
    return [f'{name}.m2', f'{name}00.skin'] + files
