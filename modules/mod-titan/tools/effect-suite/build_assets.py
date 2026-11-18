#!/usr/bin/env python3
"""The Effect Suite's game assets: 3.3.5a models (M2 version 264, their skins, .anim files and BLP textures) as JSON
the Suite's engine plays (index.html, GameModel), and spell templates from the game's own spell visuals.

    python3 build_assets.py <extracted client folder> [out folder: assets/] [--dbc <DBFilesClient folder>]

The client folder is EmberforgeExtract's output (patterns/effect-suite.txt), its files at their client paths in any
letter case. Writes assets/<name>.json for each model below and assets/templates.json; models whose files are not
there are skipped (said so), so the Suite falls back to its stand-ins.

A model's JSON: its vertices (position, normal, texture coordinates, four bone indices and weights; base64), the
triangles of each batch (texture, blend, render flags, color, transparency and texture animation), the visible
geosets, bones (parent, pivot, flags, translation / rotation / scale tracks), attachments, colors, transparencies,
texture animations, particle emitters and ribbon emitters; its sequences (the ones asked for); its textures as PNG
data URIs. Coordinates stay the model's own (X forward, Y left, Z up): the Suite turns them to its world.
"""

import base64
import io
import json
import math
import struct
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent


# ---------------------------------------------------------------------------------------------------- client files --
class Client:
    """The extracted files, found by their client path in any letter case."""

    def __init__(self, root):
        self.root = Path(root)
        self.index = {}
        for p in self.root.rglob('*'):
            if p.is_file():
                self.index[str(p.relative_to(self.root)).replace('/', '\\').lower()] = p

    def find(self, name):
        name = name.replace('/', '\\').lower()
        if name.endswith('.mdx') or name.endswith('.mdl'):
            name = name[:-4] + '.m2'
        return self.index.get(name)

    def read(self, name):
        p = self.find(name)
        return p.read_bytes() if p else None


def png_uri(image, most=256):
    image = image.convert('RGBA')
    if max(image.size) > most:
        k = most / max(image.size)
        image = image.resize((max(1, round(image.width * k)), max(1, round(image.height * k))), Image.LANCZOS)
    out = io.BytesIO()
    image.save(out, 'PNG', optimize=True)
    return 'data:image/png;base64,' + base64.b64encode(out.getvalue()).decode()


def b64(data):
    return base64.b64encode(bytes(data)).decode()


# ------------------------------------------------------------------------------------------------------- M2 reading --
FIXED16 = 32767.0


class M2:
    """A 3.3.5a model (MD20, version 264) and what its tracks read from its .anim files."""

    def __init__(self, client, name):
        if name.lower().endswith(('.mdx', '.mdl')):
            name = name[:-4] + '.m2'        # the spell tables' old names for the same files
        self.client, self.name = client, name
        self.d = client.read(name)
        if not self.d or self.d[:4] != b'MD20':
            raise FileNotFoundError(name)
        self.version = struct.unpack_from('<I', self.d, 4)[0]
        if self.version != 264:
            raise ValueError(f'{name}: version {self.version}, not 3.3.5a')
        self.base = name[:-3] if name.lower().endswith('.m2') else name
        self.seqs = [self.seq(i) for i in range(self.arr(0x1C)[0])]
        self.anims = {}

    def arr(self, off, d=None):
        return struct.unpack_from('<II', d or self.d, off)

    def seq(self, i):
        o = self.arr(0x1C)[1] + i * 64
        sid, var, dur, speed, flags, freq = struct.unpack_from('<HHIfIh', self.d, o)
        alias = struct.unpack_from('<H', self.d, o + 62)[0]
        return {'id': sid, 'var': var, 'dur': dur, 'flags': flags, 'freq': freq, 'alias': alias, 'index': i}

    def anim_data(self, seq_index):
        """Where a sequence's keys live: this model, or its .anim file (sequence flags without 0x20)."""
        s = self.seqs[seq_index]
        if s['flags'] & 0x20:
            return self.d
        if seq_index not in self.anims:
            data = self.client.read(f"{self.base}{s['id']:04d}-{s['var']:02d}.anim")
            self.anims[seq_index] = data
        return self.anims[seq_index]

    def track(self, off, fmt, size, seqs, convert=None):
        """An M2Track: interpolation, global sequence, and per exported sequence its times (ms) and values."""
        interp, gseq, tn, to, vn, vo = struct.unpack_from('<HhIIII', self.d, off)
        conv = convert or (lambda v: list(v) if len(v) > 1 else v[0])
        out = {'i': interp, 'g': gseq, 'k': []}
        if gseq >= 0:
            seq_list = [0]          # a global sequence: the first sub-array, played on its own loop
        else:
            seq_list = seqs
        any_key = False
        for si in seq_list:
            if si >= tn:
                out['k'].append([[], []])
                continue
            data = self.d if gseq >= 0 else self.anim_data(si)
            n1, o1 = struct.unpack_from('<II', self.d, to + si * 8)
            n2, o2 = struct.unpack_from('<II', self.d, vo + si * 8)
            if not data or n1 == 0 or n1 != n2 or o1 + n1 * 4 > len(data):
                out['k'].append([[], []])
                continue
            times = list(struct.unpack_from(f'<{n1}I', data, o1))
            # Hermite and Bezier keys (2, 3) hold the value and its two tangents: the value is kept.
            stride = size * 3 if interp >= 2 else size
            if o2 + n2 * stride > len(data):
                out['k'].append([[], []])
                continue
            vals = [conv(struct.unpack_from('<' + fmt, data, o2 + j * stride)) for j in range(n2)]
            out['k'].append([times, vals])
            any_key = True
        return out if any_key else None

    def fblock(self, off, fmt, size, convert=None):
        tn, to, vn, vo = struct.unpack_from('<IIII', self.d, off)
        conv = convert or (lambda v: list(v) if len(v) > 1 else v[0])
        times = [t / FIXED16 for t in struct.unpack_from(f'<{tn}H', self.d, to)] if tn else []
        vals = [conv(struct.unpack_from('<' + fmt, self.d, vo + j * size)) for j in range(vn)]
        return [times, vals]

    def texture_names(self):
        c, o = self.arr(0x50)
        out = []
        for i in range(c):
            t, f, ln, lo = struct.unpack_from('<IIII', self.d, o + i * 16)
            out.append((t, f, self.d[lo:lo + ln - 1].decode('latin-1') if ln > 1 else ''))
        return out


def quat(v):
    return [(x + 32768 if x < 0 else x - 32767) / 32767.0 for x in v]


def fixed(v):
    return v[0] / FIXED16


# -------------------------------------------------------------------------------------------------- model to JSON --
def build_model(client, name, sequences=(0,), geosets=None, replaceable=None, texture_size=256):
    """name: the model's client path; sequences: animation ids to keep (each's first variation; ids not there are
    left out); geosets: the submesh ids to show (None: all); replaceable: texture type -> client texture path."""
    m = M2(client, name)
    d = m.d
    seq_index = []
    for sid in sequences:
        for s in m.seqs:
            if s['id'] == sid and s['var'] == 0:
                seq_index.append(s['index'])
                break
    if not seq_index:
        seq_index = [0]
    seqs = [{'id': m.seqs[i]['id'], 'dur': m.seqs[i]['dur']} for i in seq_index]
    gloops = list(struct.unpack_from(f'<{m.arr(0x14)[0]}I', d, m.arr(0x14)[1]))

    # Textures (replaceable ones from the caller's list), each a PNG data URI.
    textures = []
    for ttype, flags, tname in m.texture_names():
        path = tname or (replaceable or {}).get(ttype, '')
        data = client.read(path) if path else None
        uri = None
        if data:
            try:
                uri = png_uri(Image.open(io.BytesIO(data)), texture_size)
            except Exception as e:      # noqa: BLE001 - an unreadable texture is drawn white
                print(f'  {path}: {e}')
        if path and not data:
            print(f'  missing texture {path}')
        textures.append({'name': path.split('\\')[-1], 'flags': flags, 'type': ttype, 'png': uri})

    # Vertices: 48 bytes (position, 4 weights, 4 bones, normal, 2 UV sets).
    vc, vo = m.arr(0x3C)
    pos, nrm, uv, bones, weights = [], [], [], [], []
    for i in range(vc):
        p = struct.unpack_from('<3f4B4B3f2f', d, vo + i * 48)
        pos += p[0:3]; weights += p[3:7]; bones += p[7:11]; nrm += p[11:14]; uv += p[14:16]

    skin = client.read(m.base + '00.skin')
    if not skin or skin[:4] != b'SKIN':
        raise FileNotFoundError(m.base + '00.skin')
    ln, lo = m.arr(0x04, skin)
    lookup = struct.unpack_from(f'<{ln}H', skin, lo)
    inn, ino = m.arr(0x0C, skin)
    tris = struct.unpack_from(f'<{inn}H', skin, ino)
    sn, so = m.arr(0x1C, skin)
    subs = []
    for i in range(sn):
        sid, level, vs, vcount, istart, icount = struct.unpack_from('<6H', skin, so + i * 48)
        subs.append({'id': sid, 'istart': istart + (level << 16), 'icount': icount})
    bn, bo = m.arr(0x24, skin)
    mats = [struct.unpack_from('<HH', d, m.arr(0x70)[1] + i * 4) for i in range(m.arr(0x70)[0])]
    tex_lookup = struct.unpack_from(f'<{m.arr(0x80)[0]}H', d, m.arr(0x80)[1])
    trans_lookup = struct.unpack_from(f'<{m.arr(0x90)[0]}H', d, m.arr(0x90)[1])
    uv_lookup = struct.unpack_from(f'<{m.arr(0x98)[0]}h', d, m.arr(0x98)[1])
    index_out, batches = [], []
    for i in range(bn):
        flags, prio, shader, section, geoset, color, mat, layer, tcount, combo, coord, weight, transform = \
            struct.unpack_from('<Bb11H', skin, bo + i * 24)
        sub = subs[section]
        if geosets is not None and sub['id'] not in geosets:
            continue
        start = len(index_out)
        for t in range(sub['istart'], sub['istart'] + sub['icount']):
            index_out.append(lookup[tris[t]])
        rflags, blend = mats[mat]
        batches.append({'start': start, 'count': sub['icount'], 'tex': tex_lookup[combo] if combo < len(tex_lookup) else 0,
                        'blend': blend, 'flags': rflags, 'prio': prio, 'layer': layer, 'geoset': sub['id'],
                        'color': -1 if color == 0xFFFF else color,
                        'alpha': trans_lookup[weight] if weight < len(trans_lookup) else -1,
                        'uvanim': uv_lookup[transform] if transform < len(uv_lookup) else -1})

    # Bones: 88 bytes.
    bcount, boff = m.arr(0x2C)
    bone_out = []
    for i in range(bcount):
        o = boff + i * 88
        key, flags, parent = struct.unpack_from('<iIh', d, o)
        bone_out.append({'parent': parent, 'flags': flags, 'key': key,
                         'pivot': list(struct.unpack_from('<3f', d, o + 76)),
                         't': m.track(o + 16, '3f', 12, seq_index),
                         'r': m.track(o + 36, '4h', 8, seq_index, quat),
                         's': m.track(o + 56, '3f', 12, seq_index)})

    # Attachments: 40 bytes (id, bone, position).
    an, ao = m.arr(0xF0)
    attachments = [{'id': struct.unpack_from('<I', d, ao + i * 40)[0], 'bone': struct.unpack_from('<H', d, ao + i * 40 + 4)[0],
                    'pos': list(struct.unpack_from('<3f', d, ao + i * 40 + 8))} for i in range(an)]
    cn, co = m.arr(0x48)
    colors = [{'rgb': m.track(co + i * 40, '3f', 12, seq_index), 'a': m.track(co + i * 40 + 20, 'h', 2, seq_index, fixed)}
              for i in range(cn)]
    tn, to = m.arr(0x58)
    alphas = [m.track(to + i * 20, 'h', 2, seq_index, fixed) for i in range(tn)]
    un, uo = m.arr(0x60)
    uvanims = [{'t': m.track(uo + i * 60, '3f', 12, seq_index), 'r': m.track(uo + i * 60 + 20, '4f', 16, seq_index),
                's': m.track(uo + i * 60 + 40, '3f', 12, seq_index)} for i in range(un)]

    # Particle emitters: 476 bytes each.
    pn, po = m.arr(0x128)
    particles = []
    for i in range(pn):
        b = po + i * 476
        pid, flags = struct.unpack_from('<iI', d, b)
        position = list(struct.unpack_from('<3f', d, b + 8))
        bone, tex = struct.unpack_from('<HH', d, b + 0x14)
        blend, etype, _ = struct.unpack_from('<BBH', d, b + 0x28)
        ptype, head_tail, tile_rot, rows, cols = struct.unpack_from('<BBhHH', d, b + 0x2C)
        # Its tracks (3.3.5a: a variation float after the lifespan's and the emission rate's).
        at = {'speed': 0x34, 'speedVar': 0x48, 'vRange': 0x5C, 'hRange': 0x70, 'gravity': 0x84, 'life': 0x98,
              'rate': 0xB0, 'areaL': 0xC8, 'areaW': 0xDC, 'zSource': 0xF0}
        tracks = {n: m.track(b + off, 'f', 4, seq_index) for n, off in at.items()}
        life_vary, rate_vary = struct.unpack_from('<f', d, b + 0xAC)[0], struct.unpack_from('<f', d, b + 0xC4)[0]
        particles.append({
            'flags': flags, 'pos': position, 'bone': bone, 'tex': tex, 'blend': blend, 'type': etype,
            'ptype': ptype, 'headTail': head_tail, 'rows': max(1, rows), 'cols': max(1, cols), **tracks,
            'lifeVary': life_vary, 'rateVary': rate_vary,
            'color': m.fblock(b + 0x104, '3f', 12), 'alpha': m.fblock(b + 0x114, 'h', 2, fixed),
            'scale': m.fblock(b + 0x124, '2f', 8), 'scaleVary': list(struct.unpack_from('<2f', d, b + 0x134)),
            'headCell': m.fblock(b + 0x13C, 'H', 2), 'tailCell': m.fblock(b + 0x14C, 'H', 2),
            'tailLength': struct.unpack_from('<f', d, b + 0x15C)[0],
            'burst': struct.unpack_from('<f', d, b + 0x170)[0], 'drag': struct.unpack_from('<f', d, b + 0x174)[0],
            'baseSpin': struct.unpack_from('<f', d, b + 0x178)[0], 'baseSpinVary': struct.unpack_from('<f', d, b + 0x17C)[0],
            'spin': struct.unpack_from('<f', d, b + 0x180)[0], 'spinVary': struct.unpack_from('<f', d, b + 0x184)[0],
            'enabled': m.track(b + 0x1C8, 'B', 1, seq_index),
        })

    # Ribbon emitters: 176 bytes each.
    rn, ro = m.arr(0x120)
    ribbons = []
    for i in range(rn):
        b = ro + i * 176
        rid, bone = struct.unpack_from('<iI', d, b)
        tcount, toff = struct.unpack_from('<II', d, b + 20)
        mcount, moff = struct.unpack_from('<II', d, b + 28)
        ribbons.append({
            'bone': bone, 'pos': list(struct.unpack_from('<3f', d, b + 8)),
            'tex': struct.unpack_from('<H', d, toff)[0] if tcount else 0,
            'mat': mats[struct.unpack_from('<H', d, moff)[0]] if mcount else (0, 4),
            'rgb': m.track(b + 36, '3f', 12, seq_index), 'a': m.track(b + 56, 'h', 2, seq_index, fixed),
            'above': m.track(b + 76, 'f', 4, seq_index), 'below': m.track(b + 96, 'f', 4, seq_index),
            'edgesPerSec': struct.unpack_from('<f', d, b + 116)[0], 'edgeLife': struct.unpack_from('<f', d, b + 120)[0],
            'gravity': struct.unpack_from('<f', d, b + 124)[0],
            'visible': m.track(b + 152, 'B', 1, seq_index),
        })

    bb = list(struct.unpack_from('<6f', d, 0xA0))
    return {
        'name': name.split('\\')[-1].rsplit('.', 1)[0], 'seqs': seqs, 'gloops': gloops, 'bounds': bb,
        'vertexCount': vc, 'pos': b64(struct.pack(f'<{len(pos)}f', *pos)), 'nrm': b64(struct.pack(f'<{len(nrm)}f', *nrm)),
        'uv': b64(struct.pack(f'<{len(uv)}f', *uv)), 'bones4': b64(bones), 'weights4': b64(weights),
        'index': b64(struct.pack(f'<{len(index_out)}I', *index_out)), 'batches': batches, 'textures': textures,
        'bones': bone_out, 'attachments': attachments, 'colors': colors, 'alphas': alphas, 'uvanims': uvanims,
        'particles': particles, 'ribbons': ribbons,
    }


# ------------------------------------------------------------------------------------------------ the stage's cast --
# The Human: Stand, ReadySpellDirected (51), SpellCastDirected (53), its two-handed ready and a wound; the visible
# geosets: the body, hair style 15's, the default face, gloves, boots, ears and legs (Human male display extra 8177).
HUMAN_ANIMS = (0, 51, 53, 9, 26)
DUMMY_ANIMS = (0, 9, 10)
SPELL_ANIMS = (0, 158, 159)


def human_geosets(client, model_path):
    """Hair style 15 (CharHairGeosets) for a Human male; the other groups' defaults."""
    hair = 1
    data = client.read('DBFilesClient\\CharHairGeosets.dbc')
    if data:
        n, f, rs = struct.unpack_from('<III', data, 4)
        for i in range(n):
            r = struct.unpack_from(f'<{f}I', data, 20 + i * rs)
            if r[1] == 1 and r[2] == 0 and r[3] == 15:
                hair = r[4] or 1
    return {0, hair, 101, 201, 301, 401, 501, 702, 902, 1002, 1301}


def build_cast(client, out):
    made = {}
    cast = [
        ('human', 'Character\\Human\\Male\\HumanMale.m2', HUMAN_ANIMS,
         {1: 'Textures\\BakedNpcTextures\\CreatureDisplayExtra-08177.blp', 6: 'Character\\Human\\Hair02_00.blp'}),
        ('dummy', 'Creature\\Object\\WoodenDummy.m2', DUMMY_ANIMS, {11: 'Creature\\Object\\WoodenDummy.blp'}),
    ]
    for key, path, anims, replace in cast:
        if not client.find(path):
            print(f'{key}: {path} not extracted; the Suite keeps its stand-in')
            continue
        geosets = human_geosets(client, path) if key == 'human' else None
        model = build_model(client, path, anims, geosets, replace, 512)
        (out / f'{key}.json').write_text(json.dumps(model, separators=(',', ':')))
        made[key] = path
        print(f'{key}: {path}, {model["vertexCount"]} vertices, {len(model["bones"])} bones, '
              f'{len(model["particles"])} particle emitters')
    return made


# ----------------------------------------------------------------------------------------------- spell templates ---
SPELLS = {30451: 'Arcane Blast', 116: 'Frostbolt', 133: 'Fireball', 403: 'Lightning Bolt', 421: 'Chain Lightning'}
KIT_SLOTS = {3: 'head', 4: 'chest', 5: 'base', 6: 'lefthand', 7: 'righthand', 8: 'breath', 14: 'world'}


def dbc(client, dbc_dir, name):
    data = client.read('DBFilesClient\\' + name) or ((Path(dbc_dir) / name).read_bytes()
                                                     if dbc_dir and (Path(dbc_dir) / name).exists() else None)
    if not data:
        return None, None
    n, f, rs, ss = struct.unpack_from('<IIII', data, 4)
    rows = {}
    for i in range(n):
        r = struct.unpack_from(f'<{f}I', data, 20 + i * rs)
        rows[r[0]] = r
    strings = data[20 + n * rs:]
    return rows, (lambda o: strings[o:strings.index(b'\0', o)].decode('latin-1'))


def build_templates(client, out, dbc_dir):
    spell, ss = dbc(client, dbc_dir, 'Spell.dbc')
    vis, _ = dbc(client, dbc_dir, 'SpellVisual.dbc')
    kit, _ = dbc(client, dbc_dir, 'SpellVisualKit.dbc')
    eff, es = dbc(client, dbc_dir, 'SpellVisualEffectName.dbc')
    chain, cs = dbc(client, dbc_dir, 'SpellChainEffects.dbc')
    if not spell:
        print('templates: Spell.dbc not found (give --dbc <DBFilesClient folder>)')
        return []
    models = {}

    def model(effect_id):
        if not effect_id or effect_id not in eff:
            return None
        path = es(eff[effect_id][2])
        key = path.split('\\')[-1].rsplit('.', 1)[0]
        if key not in models:
            if not client.find(path):
                print(f'  {path} not extracted')
                models[key] = None
            else:
                data = build_model(client, path, SPELL_ANIMS)
                (out / f'fx_{key}.json').write_text(json.dumps(data, separators=(',', ':')))
                models[key] = f'fx_{key}'
        return models[key]

    def kit_of(kit_id):
        k = kit.get(kit_id)
        if not k:
            return None
        slots = {}
        for col, slot in KIT_SLOTS.items():
            asset = model(k[col])
            if asset:
                slots[slot] = asset
        beam = None
        if k[17] == 0 and chain:
            cid = int(struct.unpack('<f', struct.pack('<I', k[21]))[0])
            if cid in chain:
                tex = cs(chain[cid][7])
                data = client.read(tex)
                if data:
                    beam = {'texture': png_uri(Image.open(io.BytesIO(data)), 256)}
        anim = k[2] if k[2] != 0xFFFFFFFF else None
        return {'slots': slots, 'anim': anim, 'beam': beam} if slots or beam or anim is not None else None

    templates = []
    for sid, name in SPELLS.items():
        r = spell.get(sid)
        v = vis.get(r[131]) if r else None
        if not v:
            continue
        speed = struct.unpack('<f', struct.pack('<I', r[47]))[0]
        t = {'spell': sid, 'name': name, 'speed': speed, 'precast': kit_of(v[1]), 'cast': kit_of(v[2]),
             'impact': kit_of(v[3]) or kit_of(v[15]), 'state': kit_of(v[4]), 'missile': model(v[8]) if v[7] or v[8] else None}
        if not t['missile'] and not any(t[k] and (t[k]['slots'] or t[k]['beam']) for k in ('precast', 'cast', 'impact', 'state')):
            print(f'template {name}: none of its models extracted, left out')
            continue
        templates.append(t)
        print(f'template {name}: missile {t["missile"]}, impact {t["impact"] and t["impact"]["slots"]}')
    (out / 'templates.json').write_text(json.dumps(templates, indent=1))
    return templates


def main():
    args = sys.argv[1:]
    dbc_dir = None
    if '--dbc' in args:
        i = args.index('--dbc'); dbc_dir = args[i + 1]; del args[i:i + 2]
    if not args:
        print(__doc__); return 1
    client = Client(args[0])
    out = Path(args[1]) if len(args) > 1 else HERE / 'assets'
    out.mkdir(parents=True, exist_ok=True)
    build_cast(client, out)
    build_templates(client, out, dbc_dir)
    # What is there: the Suite loads these and nothing else.
    models = sorted(p.stem for p in out.glob('*.json') if p.stem not in ('manifest', 'templates'))
    (out / 'manifest.json').write_text(json.dumps({'models': models,
                                                   'templates': (out / 'templates.json').exists()}, indent=1))
    print(f'{out}: {len(models)} models')
    return 0


if __name__ == '__main__':
    sys.exit(main())
