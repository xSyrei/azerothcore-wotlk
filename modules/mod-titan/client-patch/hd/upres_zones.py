#!/usr/bin/env python3
"""Upscale the textures of a few zones (terrain, nature, buildings) into an HD patch for the 3.3.5a client.

Runs on the computer with the game client and a GPU (Windows): Real-ESRGAN upscales on the GPU through
Vulkan (any recent NVIDIA, AMD or Intel card). Reads the client's Blizzard archives only, never writes
them (point it at a clean copy of the client anyway), and writes one new archive, patch-H.MPQ, to put
in the game's Data folder.

1. Zones: the terrain tiles (World\\Maps\\Azeroth\\Azeroth_X_Y.adt) any of whose chunks lie in the zones
   (ZONES and every subzone under them, from AreaTable.dbc).
2. Textures: those tiles' ground textures; the textures of the doodads placed on them (trees, bushes,
   rocks: M2 models) and of their buildings (WMOs), and of the doodads inside the buildings (lamps,
   crates, furniture). Only World\\ and Tileset\\ textures: no creatures, characters, items or spells.
3. Upscaling: each texture SCALE times (a Real-ESRGAN model at 4x, then brought down to SCALE with
   Lanczos, which is sharper than a 2x model), at most MAX_SIZE pixels a side. The model is the anime one by
   default: the photo one (realesrgan-x4plus) made WoW's painted grass into gritty dark clumps. The result is
   blended with the texture simply enlarged (Lanczos), STRENGTH of it the model's, to keep invented detail
   down. Ground and building
   textures repeat across surfaces, so they are padded with their own opposite edges first (and the
   padding cut off after) to stay seamless; the rest are padded by repeating their edges. Alpha is scaled
   with Lanczos and kept 1-bit where it was (leaf and fence cut-outs stay crisp).
4. Encoding: DirectX's texconv compresses each into DXT1 (no alpha or 1-bit alpha) or DXT5 (smooth
   alpha), with every mip level, written as BLP2 the client reads.

Usage (Python 3.10+, Pillow: `py -m pip install pillow`):
  py upres_zones.py <client folder> <work folder> [--model <name>] [--strength <0 to 1>]
  e.g. py upres_zones.py E:\\Emberforge\\client E:\\Emberforge\\hd-work --strength 0.4
Models: realesrgan-x4plus-anime (default), realesrgan-x4plus; strength 0.5 by default (1: the model's alone,
0: only enlarged).
Real-ESRGAN (BSD 3-clause) and texconv (MIT) are downloaded into the work folder the first time.
Re-running reuses what is done (delete the work folder to start over). The archive ends up as
<work folder>\\patch-H.MPQ.
"""

import argparse
import io
import struct
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
sys.path.insert(0, str(Path(__file__).resolve().parent))

import make_mpq  # noqa: E402
from mpq_read import Archives  # noqa: E402

ZONES = {12, 1519}          # Elwynn Forest (Northshire Valley is one of its subzones), Stormwind City
MAP_ID, MAP_NAME = 0, 'Azeroth'
SCALE, MODEL_SCALE, MAX_SIZE = 2, 4, 1024
MODEL, STRENGTH = 'realesrgan-x4plus-anime', 0.5
PAD = 8                     # pixels of padding around a texture before upscaling (at its original size)
KEEP = ('TILESET\\', 'WORLD\\')
REAL_ESRGAN = ('https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/'
               'realesrgan-ncnn-vulkan-20220424-windows.zip')
TEXCONV = 'https://github.com/microsoft/DirectXTex/releases/latest/download/texconv.exe'
OUT_ARCHIVE = 'patch-H.MPQ'


# --- Client data ------------------------------------------------------------------------------------

def chunks(data):
    """(id, payload) of an ADT or WMO file's chunks (ids are stored reversed: 'XETM' is MTEX)."""
    offset = 0
    while offset + 8 <= len(data):
        magic, size = struct.unpack_from('<4sI', data, offset)
        yield magic[::-1].decode('ascii', 'replace'), data[offset + 8:offset + 8 + size]
        offset += 8 + size


def names(payload):
    return [n.decode('latin-1') for n in payload.split(b'\0') if n]


def zone_areas(archives):
    """The ids of ZONES and every area under them, on the map."""
    data = archives.read('DBFilesClient\\AreaTable.dbc')
    _, count, fields, size, _ = struct.unpack_from('<4s4I', data)
    parent = {}
    for i in range(count):
        area_id, map_id, parent_id = struct.unpack_from('<3I', data, 20 + i * size)
        if map_id == MAP_ID:
            parent[area_id] = parent_id
    areas = set()
    for area_id in parent:
        seen, current = set(), area_id
        while current in parent and current not in seen:
            if current in ZONES:
                areas.add(area_id)
                break
            seen.add(current)
            current = parent[current]
    return areas


def model_textures(data):
    """An M2's own texture files (its textures of type 0: files named in the model)."""
    if not data or data[:4] != b'MD20':
        return []
    count, offset = struct.unpack_from('<II', data, 0x50)
    out = []
    for i in range(count):
        kind, _, length, name_offset = struct.unpack_from('<IIII', data, offset + i * 16)
        if kind == 0 and length > 1:
            out.append(data[name_offset:name_offset + length - 1].decode('latin-1'))
    return out


def m2_name(name):
    return name[:-4] + '.m2' if name.lower().endswith(('.mdx', '.mdl')) else name


def find_textures(archives):
    areas = zone_areas(archives)
    print(f'{len(areas)} areas in the zones')
    textures, models, buildings = set(), set(), set()
    tiles = 0
    for x in range(64):
        for y in range(64):
            data = archives.read(f'World\\Maps\\{MAP_NAME}\\{MAP_NAME}_{x}_{y}.adt')
            if not data:
                continue
            parts = list(chunks(data))
            # MCNK header: area id at offset 0x34
            if not any(cid == 'MCNK' and struct.unpack_from('<I', payload, 0x34)[0] in areas
                       for cid, payload in parts):
                continue
            tiles += 1
            for cid, payload in parts:
                if cid == 'MTEX':
                    textures.update(names(payload))
                elif cid == 'MMDX':
                    models.update(m2_name(n) for n in names(payload))
                elif cid == 'MWMO':
                    buildings.update(names(payload))
    print(f'{tiles} terrain tiles, {len(models)} doodads, {len(buildings)} buildings')
    for building in buildings:
        for cid, payload in chunks(archives.read(building) or b''):
            if cid == 'MOTX':
                textures.update(names(payload))
            elif cid == 'MODN':
                models.update(m2_name(n) for n in names(payload))
    for model in models:
        textures.update(model_textures(archives.read(model)))
    kept = {t.replace('/', '\\') for t in textures if t.replace('/', '\\').upper().startswith(KEEP)}
    # Ground textures' specular maps (<name>_s.blp), which the client loads beside them.
    kept |= {t[:-4] + '_s.blp' for t in kept if t.upper().startswith('TILESET\\') and archives.has(t[:-4] + '_s.blp')}
    return sorted(kept, key=str.upper)


# --- Tools ------------------------------------------------------------------------------------------

def tools(work):
    """Real-ESRGAN's and texconv's executables, downloaded into the work folder if not there yet."""
    folder = work / 'tools'
    folder.mkdir(parents=True, exist_ok=True)
    esrgan = folder / 'realesrgan-ncnn-vulkan.exe'
    if not esrgan.exists():
        print('Downloading Real-ESRGAN...')
        with zipfile.ZipFile(io.BytesIO(urllib.request.urlopen(REAL_ESRGAN).read())) as archive:
            archive.extractall(folder)
    texconv = folder / 'texconv.exe'
    if not texconv.exists():
        print('Downloading texconv...')
        texconv.write_bytes(urllib.request.urlopen(TEXCONV).read())
    return esrgan, texconv


# --- Textures ---------------------------------------------------------------------------------------

def alpha_depth(blp):
    """The alpha bits of a BLP2 (0, 1, 4 or 8)."""
    return blp[9] if blp[:4] == b'BLP2' else 8


def pad(image, wrap):
    """The image with PAD pixels more on every side: its opposite edges (wrap) or its own edges repeated."""
    w, h = image.size
    out = Image.new(image.mode, (w + 2 * PAD, h + 2 * PAD))
    if wrap:
        for dx in (-w, 0, w):
            for dy in (-h, 0, h):
                out.paste(image, (PAD + dx, PAD + dy))
    else:
        out.paste(image.resize((w + 2 * PAD, h + 2 * PAD), Image.NEAREST), (0, 0))
        out.paste(image, (PAD, PAD))
    return out


def unpad_and_scale(upscaled, original, scale):
    """The upscaled image's middle (padding cut off), brought down to the original size times scale."""
    factor = upscaled.width // (original.width + 2 * PAD)
    edge = PAD * factor
    middle = upscaled.crop((edge, edge, upscaled.width - edge, upscaled.height - edge))
    return middle.resize((original.width * scale, original.height * scale), Image.LANCZOS)


def write_blp(dds, depth):
    """A BLP2 (DXT, with its mips) from a DDS texconv wrote."""
    height, width = struct.unpack_from('<II', dds, 12)
    (mips,) = struct.unpack_from('<I', dds, 28)
    fourcc = dds[84:88]
    start = 128 + (20 if fourcc == b'DX10' else 0)
    block = 8 if depth <= 1 else 16
    offsets, sizes, data, offset = [], [], b'', 1172
    w, h = width, height
    for _ in range(min(max(mips, 1), 16)):
        size = max(1, (w + 3) // 4) * max(1, (h + 3) // 4) * block
        offsets.append(offset)
        sizes.append(size)
        data += dds[start:start + size]
        start += size
        offset += size
        w, h = max(1, w // 2), max(1, h // 2)
    offsets += [0] * (16 - len(offsets))
    sizes += [0] * (16 - len(sizes))
    alpha_bits = 0 if depth == 0 else (1 if depth == 1 else 8)
    alpha_encoding = 7 if depth > 1 else 0     # 0: DXT1, 7: DXT5
    header = struct.pack('<4sIBBBBII', b'BLP2', 1, 2, alpha_bits, alpha_encoding, 1, width, height)
    header += struct.pack('<16I', *offsets) + struct.pack('<16I', *sizes) + b'\0' * 1024
    return header + data


def run(command):
    result = subprocess.run([str(c) for c in command], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f'{command[0]} failed:\n{result.stdout}\n{result.stderr}')


def main():
    parser = argparse.ArgumentParser(usage=__doc__)
    parser.add_argument('client')
    parser.add_argument('work')
    parser.add_argument('--model', default=MODEL)
    parser.add_argument('--strength', type=float, default=STRENGTH)
    args = parser.parse_args()
    client, work = Path(args.client), Path(args.work)
    archives = Archives(client / 'Data')
    esrgan, texconv = tools(work)
    source, padded, final, dds_dir = (work / n for n in ('source', 'padded', 'final', 'dds'))
    upscaled = work / f'upscaled-{args.model}'     # each model's own results, reused on the next run
    for folder in (source, padded, upscaled, final, dds_dir):
        folder.mkdir(parents=True, exist_ok=True)

    print('Finding the zones\' textures...')
    jobs = {}       # file stem -> (archive path, alpha depth); the original is source\\<stem>.png
    for index, path in enumerate(find_textures(archives)):
        data = archives.read(path)
        if not data:
            continue
        try:
            image = Image.open(io.BytesIO(data))
            image.load()
        except Exception as error:      # a format Pillow cannot read: left as it is
            print(f'  skipped {path}: {error}')
            continue
        if max(image.size) * SCALE > MAX_SIZE or min(image.size) < 8:
            continue
        stem = f'{index:05d}'
        image = image.convert('RGBA')
        jobs[stem] = (path, alpha_depth(data))
        if not (padded / f'{stem}.png').exists():
            image.save(source / f'{stem}.png')
            pad(image.convert('RGB'), path.upper().startswith(('TILESET\\', 'WORLD\\WMO\\'))).save(
                padded / f'{stem}.png')
    print(f'{len(jobs)} textures to upscale')

    todo = [s for s in jobs if not (upscaled / f'{s}.png').exists()]
    if todo:
        print(f'Upscaling {len(todo)} on the GPU (Real-ESRGAN)...')
        batch = work / 'batch'
        batch.mkdir(exist_ok=True)
        for old in batch.glob('*.png'):
            old.unlink()
        for stem in todo:
            (batch / f'{stem}.png').write_bytes((padded / f'{stem}.png').read_bytes())
        run([esrgan, '-i', batch, '-o', upscaled, '-n', args.model, '-s', MODEL_SCALE, '-f', 'png'])

    print('Compressing (texconv)...')
    groups = {'BC1_UNORM': [], 'BC1_UNORM:alpha': [], 'BC3_UNORM': []}
    for stem, (path, depth) in jobs.items():
        image = Image.open(source / f'{stem}.png')
        model = unpad_and_scale(Image.open(upscaled / f'{stem}.png').convert('RGB'), image, SCALE)
        enlarged = image.convert('RGB').resize(model.size, Image.LANCZOS)
        rgb = Image.blend(enlarged, model, args.strength)
        alpha = image.getchannel('A').resize(rgb.size, Image.LANCZOS)
        if depth == 1:
            alpha = alpha.point(lambda a: 255 if a >= 128 else 0)
        out = rgb.copy()
        out.putalpha(alpha)
        out.save(final / f'{stem}.png')
        groups['BC1_UNORM' if depth == 0 else 'BC1_UNORM:alpha' if depth == 1 else 'BC3_UNORM'].append(stem)
    for group, stems in groups.items():
        fmt = group.split(':')[0]
        extra = ['-at', '0.5'] if group.endswith(':alpha') else []
        for i in range(0, len(stems), 100):
            run([texconv, '-nologo', '-y', '-f', fmt, '-m', '0', *extra, '-o', dds_dir,
                 *[final / f'{s}.png' for s in stems[i:i + 100]]])

    entries = []
    for stem, (path, depth) in jobs.items():
        entries.append((path, write_blp((dds_dir / f'{stem}.dds').read_bytes(), depth)))
    out = work / OUT_ARCHIVE
    out.write_bytes(make_mpq.build(entries))
    print(f'Wrote {out} ({len(entries)} textures). Copy it into the game\'s Data folder.')


if __name__ == '__main__':
    main()
