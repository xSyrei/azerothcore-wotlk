#!/usr/bin/env python3
"""Head shots for the Dungeon Journal's bosses that have no Journal portrait, made as Blizzard made theirs.

Each boss's own model (the 3.3.5a client's: its creature model, or for a humanoid its race's character model with
its baked skin, hair, beard, helmet and shoulders) is posed at the first moment of its Stand animation, lit from the
front, seen from a little to its left, and its head and shoulders cut out as a 128 x 64 picture on a clear
background, fading out at the bottom (UI-EJ-BOSS-*'s size and look). Written as
client-patch/AddOns/Emberforge_Journal/Art/EF-BOSS-<key>.blp, with Headshots.lua naming them by boss key (see
mop_journal.py; the addon uses them where the Journal has no portrait).

The bosses: the game's encounters (instance_encounters, the world database's base data) whose boss has no Journal
portrait in Data.lua; each one's creature (its credit creature, else the creature of its name spawned on its map)
and that creature's first model.

The 3.3.5a files come from the client with tools/extract/EmberforgeExtract.exe and patterns/portraits-1.txt and
portraits-2.txt.

Usage: python3 portraits.py <EmberforgeExtract folder> [preview.png]
"""

import math
import re
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
MODULE = HERE.parents[1]
ROOT = MODULE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(MODULE / 'client-patch' / 'models'))

import blp  # noqa: E402
from mop_journal import Dbc, key  # noqa: E402

ADDON = MODULE / 'client-patch' / 'AddOns' / 'Emberforge_Journal'
SQL = ROOT / 'data' / 'sql' / 'base' / 'db_world'
WIDTH, HEIGHT, SCALE = 128, 64, 4           # the picture, and how much larger it is drawn first
ANGLE = 22.0                                # degrees: seen from a little to its left
KEY_HEAD, KEY_SHOULDER_L, KEY_SHOULDER_R = 6, 2, 3
ATTACH_SHOULDER_R, ATTACH_SHOULDER_L, ATTACH_HELM = 5, 6, 11
# Encounters that are not one creature (or credited by a spell): the creature that stands for them.
STAND_INS = {
    'The Four Horsemen': 30549,         # Baron Rivendare
    'Icecrown Gunship Battle': 36948,   # Muradin Bronzebeard
    'Escaped from Arthas': 36954,       # the Lich King
    'Faction Champions': 11699,         # Varian Wrynn
    'Northrend Beasts': 34796,          # Gormok the Impaler
    "Val'kyr Twins": 34497,             # Fjola Lightbane
    'The Iron Council': 32867,          # Steelbreaker
    'Reliquary of Souls': 23418,        # the Essence of Suffering
    'Opera Event': 17521,               # the Big Bad Wolf
}


class Files:
    """The extract's files by their path in any case (the client's paths are not case-consistent)."""

    def __init__(self, root):
        self.paths = {str(p.relative_to(root)).replace('/', '\\').lower(): p for p in root.rglob('*') if p.is_file()}

    def get(self, path):
        return self.paths.get(path.replace('/', '\\').lower())

    def read(self, path):
        found = self.get(path)
        return found.read_bytes() if found else None


def texture(files, path):
    """A BLP as float RGBA rows, or None."""
    found = files.get(path if path.lower().endswith('.blp') else path + '.blp')
    if not found:
        return None
    with Image.open(found) as image:
        return np.asarray(image.convert('RGBA'), np.float32) / 255


def arr(data, offset):
    return struct.unpack_from('<II', data, offset)


class Model:
    """A 3.3.5a M2 (version 264) with its first skin, posed at the start of its Stand animation."""

    def __init__(self, files, path):
        self.path = path.rsplit('.', 1)[0] + '.m2'
        m2 = self.m2 = files.read(self.path)
        if not m2 or m2[:4] != b'MD20':
            raise FileNotFoundError(self.path)
        skin = files.read(self.path[:-3] + '00.skin')
        if not skin:
            raise FileNotFoundError(self.path[:-3] + '00.skin')
        self.files = files
        count, offset = arr(m2, 0x1C)
        self.sequences = [struct.unpack_from('<HHIfI', m2, offset + 64 * i) for i in range(count)]
        self.stand = next((i for i, s in enumerate(self.sequences) if s[0] == 0), 0)
        self.anim = None
        sequence = self.sequences[self.stand] if self.sequences else None
        if sequence and not sequence[4] & 0x20:      # its keyframes in their own file
            self.anim = files.read(f'{self.path[:-3]}{sequence[0]:04d}-{sequence[1]:02d}.anim')

        count, offset = arr(m2, 0x2C)
        self.bones = []
        for i in range(count):
            base = offset + 88 * i
            parent = struct.unpack_from('<h', m2, base + 8)[0]
            tracks = [self.track(base + 16 + 20 * k, fmt) for k, fmt in enumerate(('3f', '4h', '3f'))]
            self.bones.append((parent, tracks, np.array(struct.unpack_from('<3f', m2, base + 76))))
        count, offset = arr(m2, 0x34)
        self.key_bones = struct.unpack_from(f'<{count}h', m2, offset)
        count, offset = arr(m2, 0x3C)
        self.verts = np.frombuffer(m2, np.dtype([('pos', '<f4', 3), ('w', 'u1', 4), ('b', 'u1', 4), ('n', '<f4', 3),
                                                 ('uv', '<f4', 2), ('uv2', '<f4', 2)]), count, offset)
        count, offset = arr(m2, 0x50)
        self.textures = []
        for i in range(count):
            kind, _, n_len, n_off = struct.unpack_from('<IIII', m2, offset + i * 16)
            self.textures.append((kind, m2[n_off:n_off + n_len - 1].decode('latin-1') if n_len > 1 else ''))
        self.weights = self.alphas(0x58)
        count, offset = arr(m2, 0x70)
        self.materials = [struct.unpack_from('<HH', m2, offset + i * 4) for i in range(count)]
        count, offset = arr(m2, 0x78)
        self.combos = np.array(struct.unpack_from(f'<{count}H', m2, offset), np.int64)
        count, offset = arr(m2, 0x80)
        self.texture_lookup = struct.unpack_from(f'<{count}H', m2, offset)
        count, offset = arr(m2, 0x88)
        self.uv_lookup = struct.unpack_from(f'<{count}h', m2, offset)
        count, offset = arr(m2, 0x90)
        self.weight_lookup = struct.unpack_from(f'<{count}H', m2, offset)
        count, offset = arr(m2, 0xF0)
        self.attachments = {}
        for i in range(count):
            ident, bone = struct.unpack_from('<IH', m2, offset + 40 * i)
            self.attachments[ident] = (bone, np.array(struct.unpack_from('<3f', m2, offset + 40 * i + 8)))

        count, offset = arr(skin, 0x04)
        self.lookup = np.frombuffer(skin, '<u2', count, offset).astype(np.int64)
        count, offset = arr(skin, 0x0C)
        self.indices = np.frombuffer(skin, '<u2', count, offset).astype(np.int64)
        count, offset = arr(skin, 0x14)
        self.props = np.frombuffer(skin, 'u1', count * 4, offset).reshape(count, 4).astype(np.int64)
        count, offset = arr(skin, 0x1C)
        self.sections = [struct.unpack_from('<10H', skin, offset + i * 48) for i in range(count)]
        count, offset = arr(skin, 0x24)
        self.batches = [struct.unpack_from('<Bb11H', skin, offset + i * 24) for i in range(count)]
        self.matrices = self.pose()

    def track(self, offset, fmt):
        """A track's first keyframe in Stand (else its global or first one), or None."""
        _, global_seq, n_stamps, _, n_values, o_values = struct.unpack_from('<Hh4I', self.m2, offset)
        size = struct.calcsize('<' + fmt)
        index = self.stand if self.stand < n_values else (0 if n_values else None)
        if index is None:
            return None
        count, at = struct.unpack_from('<II', self.m2, o_values + 8 * index)
        source = self.m2
        if index == self.stand and self.anim is not None and global_seq < 0:
            source = self.anim
        if not count or at + size > len(source):
            return None
        value = np.array(struct.unpack_from('<' + fmt, source, at), float)
        if fmt == '4h':
            value = np.where(value < 0, value + 32768, value - 32767) / 32767
        return value

    def alphas(self, header):
        count, offset = arr(self.m2, header)
        values = []
        for i in range(count):
            value = self.track(offset + 20 * i, 'h')
            values.append(1.0 if value is None else float(value[0]) / 32767)
        return values

    def pose(self):
        mats = []
        for parent, (translation, rotation, scale), pivot in self.bones:
            local = np.eye(4)
            if rotation is not None and np.linalg.norm(rotation) > 0:
                x, y, z, w = rotation / np.linalg.norm(rotation)
                local[:3, :3] = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                                 [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                                 [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
            if scale is not None:
                local[:3, :3] *= scale
            if translation is not None:
                local[:3, 3] = translation
            to_pivot, back = np.eye(4), np.eye(4)
            to_pivot[:3, 3], back[:3, 3] = pivot, -pivot
            m = to_pivot @ local @ back
            mats.append(mats[parent] @ m if 0 <= parent < len(mats) else m)
        return np.array(mats) if mats else np.zeros((0, 4, 4))

    def point(self, bone, position):
        """Where a point of a bone (its pivot, or an attachment's position) is in the pose."""
        if bone < 0 or bone >= len(self.bones):
            return None
        local = self.bones[bone][2] if position is None else position
        return (self.matrices[bone] @ np.append(local, 1))[:3]

    def key_point(self, key_bone):
        if key_bone < len(self.key_bones) and self.key_bones[key_bone] >= 0:
            return self.point(self.key_bones[key_bone], None)
        return None

    def attachment(self, ident):
        """An attachment point's matrix in the pose, or None."""
        if ident not in self.attachments:
            return None
        bone, position = self.attachments[ident]
        offset = np.eye(4)
        offset[:3, 3] = position
        return (self.matrices[bone] if bone < len(self.matrices) else np.eye(4)) @ offset

    def rows(self, section):
        """A section's skin vertices (those the files really have)."""
        rows = np.arange(section[2], section[2] + section[3])
        rows = rows[rows < len(self.lookup)]
        return rows[self.lookup[rows] < len(self.verts)]

    def dominant_bones(self):
        """Each skin vertex's main bone (-1: none)."""
        result = np.full(len(self.lookup), -1)
        for section in self.sections:
            rows = self.rows(section)
            if len(rows) == 0 or len(self.combos) == 0:
                continue
            v = self.verts[self.lookup[rows]]
            main = np.argmax(v['w'], axis=1)
            slot = np.minimum(section[7] + self.props[rows, main], len(self.combos) - 1)
            result[rows] = self.combos[slot]
        return result

    def descendants(self, bone):
        found = {bone}
        for index, (parent, _, _) in enumerate(self.bones):
            if parent in found:
                found.add(index)
        return found

    def skinned(self, transform=None):
        pos = np.zeros((len(self.lookup), 3))
        nrm = np.zeros((len(self.lookup), 3))
        for section in self.sections:
            combo_start = section[7]
            rows = self.rows(section)
            vcount = len(rows)
            if vcount == 0:
                continue
            v = self.verts[self.lookup[rows]]
            p = np.append(v['pos'], np.ones((vcount, 1)), axis=1)
            if len(self.matrices) == 0:
                pos[rows], nrm[rows] = v['pos'], v['n']
                continue
            bones = self.combos[np.minimum(combo_start + self.props[rows], len(self.combos) - 1)]
            bones = np.minimum(bones, len(self.matrices) - 1)
            w = v['w'].astype(float) / 255
            for k in range(4):
                m = self.matrices[bones[:, k]]
                pos[rows] += w[:, k:k + 1] * np.einsum('nij,nj->ni', m, p)[:, :3]
                nrm[rows] += w[:, k:k + 1] * np.einsum('nij,nj->ni', m[:, :3, :3], v['n'])
        if transform is not None:
            pos = pos @ transform[:3, :3].T + transform[:3, 3]
            nrm = nrm @ transform[:3, :3].T
        return pos, nrm


class Part:
    """A model to draw: its posed vertices, and its batches with their textures (those it shows)."""

    def __init__(self, model, textures, geosets=None, transform=None):
        self.model = model
        self.pos, self.nrm = model.skinned(transform)
        self.draws = []
        for batch in model.batches:
            section = model.sections[batch[3]]
            if geosets is not None and not geosets(section[0]):
                continue
            if batch[11] < len(model.weight_lookup) and model.weights:
                index = model.weight_lookup[batch[11]]
                if index < len(model.weights) and model.weights[index] <= 0.01:
                    continue
            kind, name = model.textures[model.texture_lookup[batch[9]]]
            image = textures(kind, name)
            if image is None:
                continue
            flags, blend = model.materials[batch[6]]
            uv_set = 'uv2' if batch[10] < len(model.uv_lookup) and model.uv_lookup[batch[10]] == 1 else 'uv'
            self.draws.append((section, image, flags, blend, uv_set, batch[1]))


def project(pos, angle):
    a = math.radians(angle)
    right = np.array([-math.sin(a), math.cos(a), 0])
    toward = np.array([math.cos(a), math.sin(a), 0])
    return pos @ right, pos[:, 2], pos @ toward, toward


def draw(parts, view, angle):
    """The parts drawn into the view ((left, top) and units a pixel): RGBA."""
    (left, top), unit = view
    width, height = WIDTH * SCALE, HEIGHT * SCALE
    image = np.zeros((height, width, 4))
    zbuf = np.full((height, width), -np.inf)
    light_dir = None
    queue = []
    for part in parts:
        sx, sy, depth, toward = project(part.pos, angle)
        light_dir = toward * 0.55 + np.array([0, 0, 0.55]) + np.cross(toward, [0, 0, 1]) * -0.25
        light_dir /= np.linalg.norm(light_dir)
        normals = part.nrm / np.maximum(np.linalg.norm(part.nrm, axis=1, keepdims=True), 1e-6)
        light = 0.5 + 0.65 * np.clip(normals @ light_dir, 0, 1)
        px, py = (sx - left) / unit, (top - sy) / unit
        for section, tex, flags, blend, uv_set, priority in part.draws:
            queue.append((blend > 1, priority, part, px, py, depth, light, section, tex, flags, blend, uv_set))
    queue.sort(key=lambda q: (q[0], q[1]))
    for _, _, part, px, py, depth, light, section, tex, flags, blend, uv_set in queue:
        model = part.model
        uv = model.verts[uv_set][model.lookup]
        th, tw = tex.shape[:2]
        istart = section[4] + (section[1] << 16)
        for t in range(istart, istart + section[5], 3):
            ids = model.indices[t:t + 3]
            if len(ids) < 3:
                break
            if ids.max() >= len(px):
                continue
            x, y = px[ids], py[ids]
            area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0])
            if abs(area) < 1e-9:
                continue
            x0, x1 = int(max(0, math.floor(x.min()))), int(min(width - 1, math.ceil(x.max())))
            y0, y1 = int(max(0, math.floor(y.min()))), int(min(height - 1, math.ceil(y.max())))
            if x0 > x1 or y0 > y1:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            w0 = ((x[1] - gx) * (y[2] - gy) - (x[2] - gx) * (y[1] - gy)) / area
            w1 = ((x[2] - gx) * (y[0] - gy) - (x[0] - gx) * (y[2] - gy)) / area
            w2 = 1 - w0 - w1
            inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
            if not inside.any():
                continue
            z = w0 * depth[ids[0]] + w1 * depth[ids[1]] + w2 * depth[ids[2]]
            u = w0 * uv[ids[0], 0] + w1 * uv[ids[1], 0] + w2 * uv[ids[2], 0]
            v = w0 * uv[ids[0], 1] + w1 * uv[ids[1], 1] + w2 * uv[ids[2], 1]
            texel = tex[(np.mod(v, 1) * (th - 1)).astype(int), (np.mod(u, 1) * (tw - 1)).astype(int)]
            shade = (w0 * light[ids[0]] + w1 * light[ids[1]] + w2 * light[ids[2]])[..., None]
            color = texel[..., :3] * (1 if flags & 1 else shade)
            region = (slice(y0, y1 + 1), slice(x0, x1 + 1))
            if blend in (0, 1):
                visible = inside & (z > zbuf[region])
                if blend == 1:
                    visible &= texel[..., 3] > 0.5
                zbuf[region][visible] = z[visible]
                rgba = np.concatenate([color, np.ones_like(color[..., :1])], axis=-1)
                image[region][visible] = rgba[visible]
            else:
                visible = inside & (z >= zbuf[region] - 1e-3)
                alpha = texel[..., 3:]
                target = image[region]
                if blend == 2:
                    mixed = np.concatenate([target[..., :3] * (1 - alpha) + color * alpha,
                                            target[..., 3:] + (1 - target[..., 3:]) * alpha], axis=-1)
                else:
                    mixed = np.concatenate([np.clip(target[..., :3] + color * alpha, 0, 1), target[..., 3:]],
                                           axis=-1)
                image[region][visible] = mixed[visible]
    return np.clip(image, 0, 1)


def frame(parts, model, angle):
    """The view, 2:1: the head (the vertices its bone and the bones under it move) and the shoulders, the picture
    2.3 times the head's height, the head's top a little under the picture's."""
    body = parts[0]
    sx, sy, _, _ = project(body.pos, angle)
    every = np.concatenate([p.pos for p in parts])
    ex, ey, _, _ = project(every, angle)
    height = np.ptp(ey)
    head = model.key_bones[KEY_HEAD] if KEY_HEAD < len(model.key_bones) else -1
    used = np.zeros(len(body.pos), bool)
    for section, *_ in body.draws:
        used[model.rows(section)] = True
    mask = np.zeros(len(body.pos), bool)
    if head >= 0:
        mask = np.isin(model.dominant_bones(), list(model.descendants(head))) & used
    pivot = model.key_point(KEY_HEAD)
    if mask.sum() >= 8 and pivot is not None:
        # The head's height: from its bone's pivot (the neck) to its top, which counts what sits on it (a helmet,
        # horns, a crest: the other parts near it).
        px, pz, _, _ = project(pivot[None], angle)
        hx, pz = float(px[0]), float(pz[0])
        top = float(sy[mask].max())
        width = max(float(np.ptp(sx[mask])), 1e-3)
        for part in parts[1:]:
            ox, oy, _, _ = project(part.pos, angle)
            near = np.abs(ox - hx) < width
            if near.any():
                top = max(top, float(oy[near].max()))
        size = max(top - pz, height * 0.05)
        span = size * 2.3
        # With shoulders: three and a half times the neck's drop to them (head and shoulders), when that is a
        # sensible share of the model.
        shoulders = [p for p in (model.key_point(KEY_SHOULDER_L), model.key_point(KEY_SHOULDER_R)) if p is not None]
        least = height * 0.32       # no shoulders to go by: a third of the model at least
        if shoulders:
            drop = pz - float(np.mean([project(p[None], angle)[1][0] for p in shoulders]))
            if 0.04 * height < drop * 3.4 < 0.75 * height:
                span, least = max(drop * 3.4, (top - pz) * 1.6), height * 0.22
        span = min(max(span, least), height * 0.8)
    else:
        # No head: the model's highest part, a third of its height.
        hx, top = float(np.median(ex)), float(ey.max())
        span = height * 0.45
    unit = span / (HEIGHT * SCALE)
    return (hx - WIDTH * SCALE * unit / 2, top + span * 0.06), unit


def finish(image):
    """Down to its size, faded out at the bottom (the last quarter)."""
    picture = Image.fromarray((image * 255).astype(np.uint8), 'RGBA').resize((WIDTH, HEIGHT), Image.LANCZOS)
    rgba = np.asarray(picture).astype(np.float32)
    fade = np.clip((HEIGHT - 1 - np.arange(HEIGHT)) / (HEIGHT * 0.25), 0, 1)
    rgba[..., 3] *= fade[:, None]
    return Image.fromarray(rgba.astype(np.uint8), 'RGBA')


# --- The bosses -------------------------------------------------------------------------------------------------

def sql_rows(name):
    for line in (SQL / name).open(encoding='utf8'):
        if line.startswith('('):
            yield line


def bosses():
    """(map, name, creature entry or None, display) of every encounter, in the game's order: its creature is its
    stand-in (STAND_INS), else its credit creature, else the creature of its name (spawned on its map, if any).
    Bosses the game summons have no spawn: their map is -1 (head shots go by name)."""
    encounters = re.findall(r"^\((\d+),(\d+),(\d+),\d+,'((?:[^'\\]|\\.)*)'\)",
                            (SQL / 'instance_encounters.sql').read_text(encoding='utf8'), re.M)
    names = {}
    for line in sql_rows('creature_template.sql'):
        match = re.match(r"\((\d+),\d+,\d+,\d+,\d+,\d+,'((?:[^'\\]|\\.)*)'", line)
        if match:
            names.setdefault(match.group(2).replace("\\'", "'").lower(), []).append(int(match.group(1)))
    models = {}
    for line in sql_rows('creature_template_model.sql'):
        for entry, index, display in re.findall(r'\((\d+),(\d+),(\d+),', line):
            if index == '0':
                models[int(entry)] = int(display)
    spawn_map, spawned = {}, set()
    for line in sql_rows('creature.sql'):
        for entry, map_id in re.findall(r'\(\d+,(\d+),\d+,\d+,(\d+),', line):
            spawn_map.setdefault(int(entry), int(map_id))
            spawned.add((int(map_id), int(entry)))
    result, seen = [], set()
    for _, credit_type, credit, name in encounters:
        name = name.replace("\\'", "'")
        credit = int(credit)
        if name in STAND_INS:
            entry = STAND_INS[name]
        elif credit_type == '0' and credit in models:
            entry = credit
        else:
            candidates = names.get(name.lower(), [])
            entry = next((e for e in candidates if e in spawn_map), candidates[0] if candidates else None)
        map_id = spawn_map.get(entry, spawn_map.get(credit, -1) if credit_type == '0' else -1)
        if (map_id, key(name)) in seen:
            continue
        seen.add((map_id, key(name)))
        result.append((map_id, name, entry, models.get(entry)))
    return result


def journal_portraits():
    """The bosses Data.lua has a portrait for: (map, key) and keys."""
    script = ('dofile(arg[1]) for m, b in pairs(EmberforgeJournalBosses) do for k, v in pairs(b) do '
              'if v.p then print(m, k) end end end for k in pairs(EmberforgeJournalPortraits) do print("P", k) end')
    out = subprocess.run(['lua5.1', '-', str(ADDON / 'Data.lua')], input=script, capture_output=True, text=True,
                         check=True).stdout
    by_map, keys = set(), set()
    for line in out.splitlines():
        a, b = line.split('\t')
        if a == 'P':
            keys.add(b)
        else:
            by_map.add((int(a), b))
    return by_map, keys


# --- Models -----------------------------------------------------------------------------------------------------

class Client:
    def __init__(self, root):
        self.files = Files(root)
        dbc = root / 'DBFilesClient'
        self.display = Dbc(dbc / 'CreatureDisplayInfo.dbc')
        self.model_data = Dbc(dbc / 'CreatureModelData.dbc')
        self.extra = Dbc(dbc / 'CreatureDisplayInfoExtra.dbc')
        self.items = Dbc(dbc / 'ItemDisplayInfo.dbc')
        self.races = Dbc(dbc / 'ChrRaces.dbc')
        self.sections = Dbc(dbc / 'CharSections.dbc')
        self.hair = Dbc(dbc / 'CharHairGeosets.dbc')
        self.facial = Dbc(dbc / 'CharacterFacialHairStyles.dbc')
        self.helmet_vis = Dbc(dbc / 'HelmetGeosetVisData.dbc')

    def creature(self, display_id):
        """The parts of a creature display, and its main model (for the framing)."""
        display = self.display.by_id[display_id]
        model_path = self.model_data.text(self.model_data.by_id[display[1]][2])
        if display[3]:
            return self.humanoid(display, model_path)
        model = Model(self.files, model_path)
        folder = model_path.rsplit('\\', 1)[0]
        variations = [self.display.text(display[6 + i]) for i in range(3)]

        def textures(kind, name):
            if kind == 0:
                return texture(self.files, name)
            if 11 <= kind <= 13 and variations[kind - 11]:
                return texture(self.files, f'{folder}\\{variations[kind - 11]}')
            return None
        return [Part(model, textures)], model

    def humanoid(self, display, model_path):
        extra = self.extra.by_id[display[3]]
        race, sex, skin, face, hair_style, hair_color, facial = extra[1:8]
        items = extra[8:19]
        model = Model(self.files, model_path)
        bake = texture(self.files, 'Textures\\BakedNpcTextures\\' + self.extra.text(extra[20]))
        if bake is None:
            raise FileNotFoundError('its baked skin ' + self.extra.text(extra[20]))
        hair_texture = None
        for row in self.sections.rows:
            if row[1] == race and row[2] == sex and row[3] == 3 and row[8] == hair_style and row[9] == hair_color:
                hair_texture = texture(self.files, self.sections.text(row[4]))
        skin_extra = None
        for row in self.sections.rows:
            if row[1] == race and row[2] == sex and row[3] == 0 and row[9] == skin and row[5]:
                skin_extra = texture(self.files, self.sections.text(row[5]))

        hair_geoset = next((row[4] for row in self.hair.rows if row[1] == race and row[2] == sex
                            and row[3] == hair_style), 1)
        facial_row = next((row for row in self.facial.rows if row[0] == race and row[1] == sex
                           and row[2] == facial), None)
        shown = {0, 401, 501, 702, 801, 901, 1001, 1101, 1301}
        if hair_geoset:
            shown.add(hair_geoset)
        if facial_row:
            shown |= {100 + facial_row[3], 300 + facial_row[4], 200 + facial_row[5]}
        else:
            shown |= {101, 201, 301}
        helm = self.items.by_id.get(items[0])
        if helm:
            vis = self.helmet_vis.by_id.get(helm[13 + sex])
            if vis:
                race_bit = 1 << race
                if vis[1] & race_bit:
                    shown = {g for g in shown if g >= 100 or g == 0}
                for column, group in ((2, 1), (3, 2), (4, 3), (5, 7)):
                    if vis[column] & race_bit:
                        shown = {g for g in shown if g // 100 != group}

        def textures(kind, name):
            if kind == 0:
                return texture(self.files, name)
            if kind == 1:
                return bake
            if kind == 6:
                return hair_texture
            if kind == 8:
                return skin_extra
            return None
        parts = [Part(model, textures, lambda g: g in shown)]

        prefix = self.races.text(self.races.by_id[race][6]) if race in self.races.by_id else ''
        suffix = f'_{prefix}{"F" if sex else "M"}'
        if helm:
            parts += self.item(model, helm, 'Head', ATTACH_HELM, 0, suffix)
        shoulders = self.items.by_id.get(items[1])
        if shoulders:
            parts += self.item(model, shoulders, 'Shoulder', ATTACH_SHOULDER_L, 0, '')
            parts += self.item(model, shoulders, 'Shoulder', ATTACH_SHOULDER_R, 1, '')
        return parts, model

    def item(self, body, item, folder, attachment, side, suffix):
        name = self.items.text(item[1 + side])
        where = body.attachment(attachment)
        if not name or where is None:
            return []
        stem = name.rsplit('.', 1)[0]
        path = f'Item\\ObjectComponents\\{folder}\\{stem}{suffix}.m2'
        if not self.files.get(path):
            path = f'Item\\ObjectComponents\\{folder}\\{stem}.m2'
        try:
            model = Model(self.files, path)
        except FileNotFoundError:
            return []
        skin = self.items.text(item[3 + side])
        image = texture(self.files, f'Item\\ObjectComponents\\{folder}\\{skin}') if skin else None

        def textures(kind, name):
            if kind == 0:
                return texture(self.files, name)
            if kind == 2:
                return image
            return None
        return [Part(model, textures, transform=where)]


def render(client, display_id):
    parts, model = client.creature(display_id)
    view = frame(parts, model, ANGLE)
    return finish(draw(parts, view, ANGLE))


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    client = Client(Path(sys.argv[1]))
    by_map, keys = journal_portraits()
    art = ADDON / 'Art'
    for old in art.glob('EF-BOSS-*.blp'):     # made again below
        old.unlink()
    written, failed, sheet = {}, [], []
    for map_id, name, entry, display in bosses():
        boss_key = key(name)
        if (map_id, boss_key) in by_map or boss_key in keys or boss_key in written:
            continue
        if not display or display not in client.display.by_id:
            failed.append(f'{name} (no model)')
            continue
        try:
            picture = render(client, display)
        except (FileNotFoundError, KeyError, ValueError, IndexError, struct.error) as error:
            failed.append(f'{name} ({type(error).__name__}: {error})')
            continue
        if np.asarray(picture)[..., 3].max() < 16:
            failed.append(f'{name} (nothing drawn)')
            continue
        file = f'EF-BOSS-{boss_key}'
        (art / f'{file}.blp').write_bytes(blp.encode(picture))
        written[boss_key] = file
        sheet.append((name, picture))
        print(f'{name}: {file}')
    lines = ['-- The Dungeon Journal\'s head shots of the bosses that have no Journal portrait (Art/EF-BOSS-*), by',
             '-- boss key. Written by modules/mod-titan/tools/journal/portraits.py; edit that, not this file.',
             'EmberforgeJournalHeadshots = {']
    lines += [f'    ["{k}"] = "{f}",' for k, f in sorted(written.items())]
    lines.append('}')
    (ADDON / 'Headshots.lua').write_text('\n'.join(lines) + '\n', encoding='utf8')
    print(f'{len(written)} head shots; not made: {len(failed)}')
    for line in failed:
        print('  ' + line)
    if len(sys.argv) > 2 and sheet:
        from PIL import ImageDraw
        columns, cell_w, cell_h = 6, WIDTH * 2 + 8, HEIGHT * 2 + 20
        rows = math.ceil(len(sheet) / columns)
        board = Image.new('RGBA', (columns * cell_w, rows * cell_h), (30, 24, 20, 255))
        pen = ImageDraw.Draw(board)
        for index, (name, picture) in enumerate(sheet):
            x, y = (index % columns) * cell_w, (index // columns) * cell_h
            board.alpha_composite(picture.resize((WIDTH * 2, HEIGHT * 2)), (x, y))
            pen.text((x + 4, y + HEIGHT * 2 + 4), name[:40], fill=(230, 210, 160, 255))
        board.save(sys.argv[2])


if __name__ == '__main__':
    main()
