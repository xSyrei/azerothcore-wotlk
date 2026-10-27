#!/usr/bin/env python3
"""Build a static WoW 3.3.5a M2 scene (login screen backdrop) from wow.export OBJ exports.

The scene is one WMO (the room) plus M2 doodads placed with the WMO's doodad placement CSV, a star
sky dome around everything, and a camera. Everything is self-lit (unlit materials) because login
scenes get no world lighting. Kept per material: the first texture, the blend mode, two-sided and
no-depth-write flags, and scrolling texture animations (waterfalls). Not kept: particles, ribbons,
bone animation, extra texture layers (reflections, shimmer masks).

The scene is described in Python (see build_patch.py, LOGIN_SCENE) and written by build().
"""

import csv
import json
import math
import struct
from pathlib import Path

import numpy as np
from PIL import Image

import blp
from wowexport_to_m2 import Blob, NONE16, SEQUENCE_IN_MODEL, empty_track, track

UNLIT, TWO_SIDED, NO_DEPTH_WRITE = 0x1, 0x4, 0x10
# WMO blend modes -> M2 blend modes
WMO_BLEND = {0: 0, 1: 1, 2: 2, 3: 4, 4: 5, 5: 6, 6: 2, 7: 2}


class Part:
    """A batch of triangles with one material."""

    def __init__(self, positions, normals, uvs, triangles, texture, blend, flags, scroll=None, tint=None,
                 max_size=None):
        self.positions, self.normals, self.uvs = positions, normals, uvs
        self.triangles = triangles          # (n, 3) indices into this part's vertices
        self.texture, self.blend, self.flags, self.scroll = texture, blend, flags, scroll
        self.tint = tint                    # RGB multiplier baked into the texture (stands in for lighting)
        self.max_size = max_size            # texture size cap overriding build()'s default


def read_obj(path):
    """Positions/normals in M2 axes (wow.export OBJ is x, z, -y), UVs with V flipped back, the
    faces of each OBJ group (as global vertex indices) and each group's material name."""
    pos, nrm, uv, groups, names = [], [], [], [], []
    for line in Path(path).read_text().splitlines():
        p = line.split()
        if not p:
            continue
        if p[0] == 'v':
            x, y, z = map(float, p[1:4])
            pos.append((x, -z, y))
        elif p[0] == 'vn':
            x, y, z = map(float, p[1:4])
            nrm.append((x, -z, y))
        elif p[0] == 'vt':
            uv.append((float(p[1]), 1.0 - float(p[2])))
        elif p[0] == 'g':
            groups.append([])
            names.append(None)
        elif p[0] == 'usemtl':
            names[-1] = p[1]
        elif p[0] == 'f':
            groups[-1].append([int(c.split('/')[0]) - 1 for c in p[1:4]])
    return np.array(pos, np.float64), np.array(nrm, np.float64), np.array(uv, np.float64), groups, names


def part_from_faces(pos, nrm, uv, faces, **material):
    faces = np.array(faces, np.int64)
    used, local = np.unique(faces, return_inverse=True)
    return Part(pos[used], nrm[used], uv[used], local.reshape(-1, 3), **material)


def quat_matrix(w, x, y, z):
    return np.array([
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
    ])


def wmo_parts(folder, name, texture_root):
    """The room: one part per OBJ group (= WMO render batch)."""
    folder = Path(folder)
    meta = json.loads((folder / f'{name}.json').read_text())
    pos, nrm, uv, groups, names = read_obj(folder / f'{name}.obj')
    textures = {t['fileDataID']: t['fileNameExternal'].replace('\\', '/') for t in meta['textures']}
    # wow.export leaves the render batches' material IDs at 0, so match each OBJ group to a
    # material through its usemtl name, which is "mat_<texture file name>".
    by_name = {}
    for m in meta['materials']:
        by_name.setdefault('mat_' + Path(textures[m['texture1']]).stem, m)
    parts = []
    for faces, mtl in zip(groups, names):
        m = by_name[mtl]
        texture = Path(texture_root) / textures[m['texture1']].split('textures/', 1)[1]
        flags = UNLIT | (TWO_SIDED if m['flags'] & 0x4 else 0)
        blend = WMO_BLEND.get(m['blendMode'], 0)
        if blend:
            flags |= NO_DEPTH_WRITE
        parts.append(part_from_faces(pos, nrm, uv, faces, texture=texture, blend=blend, flags=flags))
    return parts


def m2_parts(folder, name):
    """A doodad model: one part per texture unit, using the unit's first texture."""
    folder = Path(folder)
    meta = json.loads((folder / f'{name}.json').read_text())
    pos, nrm, uv, groups, _ = read_obj(folder / f'{name}.obj')
    parts = []
    for unit in meta['skin']['textureUnits']:
        material = meta['materials'][unit['materialIndex']]
        tex_index = meta['textureCombos'][unit['textureComboIndex']]
        texture = folder / meta['textures'][tex_index]['fileNameExternal']
        flags = UNLIT | (material['flags'] & (TWO_SIDED | NO_DEPTH_WRITE))
        scroll = None
        lookup = meta.get('textureTransformsLookup') or []
        ti = unit['textureTransformComboIndex']
        if ti < len(lookup) and lookup[ti] != NONE16:
            scroll = meta['textureTransforms'][lookup[ti]]['translation']
        parts.append(part_from_faces(pos, nrm, uv, groups[unit['skinSectionIndex']], texture=texture,
                                     blend=material['blendingMode'], flags=flags, scroll=scroll))
    return parts


def placed(parts, position, rotation_wxyz, scale):
    """Copies of a doodad's parts moved to a placement (rotation, then scale, then position)."""
    rot = quat_matrix(*rotation_wxyz)
    out = []
    for p in parts:
        q = Part(p.positions @ rot.T * scale + np.array(position), p.normals @ rot.T, p.uvs, p.triangles,
                 p.texture, p.blend, p.flags, p.scroll, p.tint, p.max_size)
        out.append(q)
    return out


def placements(csv_path, model_file):
    rows = csv.DictReader(open(csv_path), delimiter=';')
    for r in rows:
        if r['ModelFile'].replace('\\', '/').split('/')[-1] == f'{model_file}.obj':
            yield ((float(r['PositionX']), float(r['PositionY']), float(r['PositionZ'])),
                   tuple(float(r[k]) for k in ('RotationW', 'RotationX', 'RotationY', 'RotationZ')),
                   float(r['ScaleFactor']))


def sky_dome(center, radius, texture, rings=16, segments=32, repeat=(4, 2)):
    """Inside-facing UV sphere for the star field."""
    verts, uvs = [], []
    for i in range(rings + 1):
        theta = math.pi * i / rings
        for j in range(segments + 1):
            phi = 2 * math.pi * j / segments
            verts.append((math.sin(theta) * math.cos(phi), math.sin(theta) * math.sin(phi), math.cos(theta)))
            uvs.append((j / segments * repeat[0], i / rings * repeat[1]))
    tris = []
    for i in range(rings):
        for j in range(segments):
            a, b = i * (segments + 1) + j, (i + 1) * (segments + 1) + j
            tris += [(a, a + 1, b), (a + 1, b + 1, b)]
    v = np.array(verts)
    return Part(v * radius + np.array(center), -v, np.array(uvs), np.array(tris), texture, 0, UNLIT | TWO_SIDED | 0x2,
                max_size=1024)


def tinted(image, tint):
    """Darken / color a texture to stand in for the lighting the scene does not get, keeping
    glowing details (bright, saturated blue-white pixels) at full brightness."""
    a = np.asarray(image.convert('RGBA')).astype(np.float32) / 255
    rgb, alpha = a[..., :3], a[..., 3:]
    lum = rgb.max(-1, keepdims=True)
    glow = np.clip((rgb[..., 2:3] - rgb[..., 0:1] - 0.15) * 4, 0, 1) * np.clip((lum - 0.55) * 4, 0, 1)
    out = rgb * np.array(tint) * (1 - glow) + rgb * glow
    return Image.fromarray((np.dstack([np.clip(out, 0, 1), alpha]) * 255).astype(np.uint8), 'RGBA')


def look_at_track_key(value):
    return struct.pack('<3f3f3f', *value, 0, 0, 0, 0, 0, 0)        # M2SplineKey<C3Vector>


def build(parts, out_dir, name, client_dir, camera, max_texture=512):
    """Write <name>.m2, <name>00.skin and one BLP per texture into out_dir."""
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    # Textures: one BLP each, at most max_texture pixels on a side.
    # A texture key is (file, tint): the same file with two tints becomes two textures.
    texture_files, specs = [], {}
    for p in parts:
        key = (str(p.texture), p.tint)
        if key not in specs:
            specs[key] = p
            texture_files.append(key)
    texture_names = []
    for key in texture_files:
        path, tint = key
        spec = specs[key]
        stem = Path(path).stem + ('' if tint is None else '_t' + ''.join(f'{int(c * 100):02d}' for c in tint))
        limit = spec.max_size or max_texture
        with Image.open(path) as image:
            if tint is not None:
                image = tinted(image, tint)
            if max(image.size) > limit:
                f = limit / max(image.size)
                image = image.resize((max(1, int(image.width * f)), max(1, int(image.height * f))), Image.Resampling.LANCZOS)
            (out_dir / f'{stem}.blp').write_bytes(blp.encode(image))
        texture_names.append(f'{client_dir}\\{stem}.blp')

    # Opaque parts first, then blended ones (drawn in this order).
    parts = sorted(parts, key=lambda p: p.blend != 0)

    vertices, sections, indices = [], [], []
    vbase = 0
    for p in parts:
        n = len(p.positions)
        istart = len(indices)
        indices.extend((p.triangles + vbase).reshape(-1).tolist())
        sections.append((vbase, n, istart, len(p.triangles) * 3, p))
        vertices.append(p)
        vbase += n
    if vbase > 0xFFFF:
        raise ValueError(f'{vbase} vertices: a 3.3.5a skin holds at most 65535')

    allpos = np.concatenate([p.positions for p in parts])
    lo, hi = allpos.min(0), allpos.max(0)
    radius = float(np.linalg.norm(allpos - (lo + hi) / 2, axis=1).max())

    scrolls = [p.scroll for p in parts if p.scroll]
    global_seqs = sorted({max(s['timestamps'][0]) for s in scrolls}) if scrolls else []

    blob = Blob(0x130)
    name_ref = blob.add(name.encode() + b'\0', len(name) + 1)
    globals_ref = blob.array('I', global_seqs)
    seq = struct.pack('<HHIfIhHIII6ffhH', 0, 0, 10000, 0.0, SEQUENCE_IN_MODEL, 0x7FFF, 0, 0, 0, 150,
                      *lo, *hi, radius, -1, 0)
    sequences = blob.add(seq, 1)
    sequence_lookup = blob.array('H', [0])
    bone = struct.pack('<iIhHHH', -1, 0, -1, 0, 0, 0) + empty_track() * 3 + struct.pack('<3f', 0, 0, 0)
    bones = blob.add(bone, 1)
    vdata = bytearray()
    for p in parts:
        for pos, nrm, uv in zip(p.positions, p.normals, p.uvs):
            vdata += struct.pack('<3f4B4B3f2f2f', *pos, 255, 0, 0, 0, 0, 0, 0, 0, *nrm, *uv, 0.0, 0.0)
    vertices_ref = blob.add(bytes(vdata), vbase)

    tex_data = b''
    for tname in texture_names:
        ref = blob.add(tname.encode() + b'\0', len(tname) + 1)
        tex_data += struct.pack('<II2I', 0, 3, *ref)                     # wrap U and V
    textures_ref = blob.add(tex_data, len(texture_names))
    weights = blob.add(track(blob, 0, [([0], [0x7FFF])], 'h'), 1)

    # Scrolling textures: one transform per distinct scroll, on its global sequence.
    transforms, transform_of = [], {}
    tt_data = b''
    for p in parts:
        if p.scroll and id(p.scroll) not in transform_of:
            s = p.scroll
            stamps, values = s['timestamps'][0], [tuple(v) for v in s['values'][0]]
            gs = global_seqs.index(max(stamps))
            translation = track(blob, s['interpolation'], [(stamps, values)], '3f')
            translation = translation[:2] + struct.pack('<H', gs) + translation[4:]
            tt_data += translation + empty_track() + empty_track()
            transform_of[id(p.scroll)] = len(transforms)
            transforms.append(s)
    transforms_ref = blob.add(tt_data, len(transforms))
    transform_lookup = blob.array('H', [NONE16] + list(range(len(transforms))))

    materials = []
    for p in parts:
        key = (p.flags, p.blend)
        if key not in materials:
            materials.append(key)
    materials_ref = blob.array('HH', materials)
    bone_lookup = blob.array('H', [0])
    texture_lookup = blob.array('H', list(range(len(texture_names))))
    texture_unit_lookup = blob.array('H', [0])
    transparency_lookup = blob.array('H', [0])
    replaceable_lookup = blob.array('h', [-1])

    # Camera 0, the one glue screens use (SetCamera(0)). Positions are absolute (base left at 0).
    cam_pos, cam_target, fov = camera
    cam = struct.pack('<Ifff', 0, fov, 4000.0, 1.0)
    cam += struct.pack('<HH2I2I', 0, NONE16, *blob.array('II', [blob.array('I', [0])]),
                       *blob.array('II', [blob.add(look_at_track_key(cam_pos), 1)]))
    cam += struct.pack('<3f', 0, 0, 0)
    cam += struct.pack('<HH2I2I', 0, NONE16, *blob.array('II', [blob.array('I', [0])]),
                       *blob.array('II', [blob.add(look_at_track_key(cam_target), 1)]))
    cam += struct.pack('<3f', 0, 0, 0)
    cam += struct.pack('<HH2I2I', 0, NONE16, *blob.array('II', [blob.array('I', [0])]),
                       *blob.array('II', [blob.add(struct.pack('<3f', 0, 0, 0), 1)]))
    assert len(cam) == 100
    cameras_ref = blob.add(cam, 1)
    camera_lookup = blob.array('H', [0])

    header = struct.pack('<4sI', b'MD20', 264)
    header += struct.pack('<2I', *name_ref) + struct.pack('<I', 0)
    header += struct.pack('<2I', *globals_ref)
    header += struct.pack('<2I', *sequences) + struct.pack('<2I', *sequence_lookup)
    header += struct.pack('<2I', *bones) + struct.pack('<2I', 0, 0)
    header += struct.pack('<2I', *vertices_ref) + struct.pack('<I', 1)
    header += struct.pack('<2I', 0, 0) + struct.pack('<2I', *textures_ref)          # colors, textures
    header += struct.pack('<2I', *weights) + struct.pack('<2I', *transforms_ref)
    header += struct.pack('<2I', *replaceable_lookup) + struct.pack('<2I', *materials_ref)
    header += struct.pack('<2I', *bone_lookup) + struct.pack('<2I', *texture_lookup)
    header += struct.pack('<2I', *texture_unit_lookup) + struct.pack('<2I', *transparency_lookup)
    header += struct.pack('<2I', *transform_lookup)
    header += struct.pack('<6ff6ff', *lo, *hi, radius, *lo, *hi, radius)
    header += struct.pack('<2I', 0, 0) * 3
    header += struct.pack('<2I', 0, 0) * 2                                          # attachments
    header += struct.pack('<2I', 0, 0) * 2                                          # events, lights
    header += struct.pack('<2I', *cameras_ref) + struct.pack('<2I', *camera_lookup)
    header += struct.pack('<2I', 0, 0) * 2                                          # ribbons, particles
    assert len(header) == 0x130
    blob.data[:0x130] = header
    (out_dir / f'{name}.m2').write_bytes(bytes(blob.data))

    # Skin: identity vertex lookup; index starts above 65535 go in the section's Level field.
    skin = Blob(48)
    lookup = skin.array('H', list(range(vbase)))
    tris = skin.add(np.array(indices, '<u2').tobytes(), len(indices))
    props = skin.add(b'\0\0\0\0' * vbase, vbase)
    section_data, batch_data = b'', b''
    for i, (vstart, vcount, istart, icount, p) in enumerate(sections):
        center = p.positions.mean(0)
        r = float(np.linalg.norm(p.positions - center, axis=1).max())
        level, istart_low = istart >> 16, istart & 0xFFFF
        section_data += struct.pack('<10H3f3ff', 0, level, vstart, vcount, istart_low, icount, 1, 0, 1, 0,
                                    *center, *center, r)
        transform = 1 + transform_of[id(p.scroll)] if p.scroll else 0
        batch_data += struct.pack('<Bb11H', 16, 0, 0, i, i, NONE16, materials.index((p.flags, p.blend)), 0, 1,
                                  texture_files.index((str(p.texture), p.tint)), 0, 0, transform)
    sections_ref = skin.add(section_data, len(sections))
    batches_ref = skin.add(batch_data, len(sections))
    skin.data[:48] = struct.pack('<4s2I2I2I2I2II', b'SKIN', *lookup, *tris, *props, *sections_ref, *batches_ref, 21)
    (out_dir / f'{name}00.skin').write_bytes(bytes(skin.data))
    return {'vertices': vbase, 'triangles': len(indices) // 3, 'parts': len(parts), 'textures': len(texture_names),
            'levels': max(s[2] >> 16 for s in sections)}
