#!/usr/bin/env python3
"""Bake a humanoid NPC's skin as the 3.3.5a client wants it (Textures\\BakedNpcTextures\\*.blp, 256 x 256): its race's
skin, face, facial hair and scalp (CharSections), then each worn item's body textures (ItemDisplayInfo's texture
components) in the order the client draws them, into the character texture's regions.

Used for Emberforge's own NPCs (NPCS: the Keeper of the Ember): the baked PNG is written to textures/BakedNpcTextures/,
which the patch builds into a BLP; their display rows are patch_creatures.py's.

The skins and item textures come from the 3.3.5a client (tools/extract/EmberforgeExtract.exe with
tools/extract/patterns/prestige-keeper.txt, and the portraits' patterns for the character textures).

Usage: python3 bake_npc.py <dir(s) with the client files, ':'-separated>
"""

import struct
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
OUT = HERE / 'textures' / 'BakedNpcTextures'

# Regions of the 256 x 256 character texture (CharComponentTextureSections, 3.3.5a): x, y, width, height.
ARM_UPPER, ARM_LOWER, HAND, FACE_UPPER, FACE_LOWER = (0, 0, 128, 64), (0, 64, 128, 64), (0, 128, 128, 32), \
    (0, 160, 128, 32), (0, 192, 128, 64)
TORSO_UPPER, TORSO_LOWER, LEG_UPPER, LEG_LOWER, FOOT = (128, 0, 128, 64), (128, 64, 128, 32), (128, 96, 128, 64), \
    (128, 160, 128, 64), (128, 224, 128, 32)
# ItemDisplayInfo's texture fields (15-22), in order, with their region and client folder.
ITEM_REGIONS = [(ARM_UPPER, 'ArmUpperTexture'), (ARM_LOWER, 'ArmLowerTexture'), (HAND, 'HandTexture'),
                (TORSO_UPPER, 'TorsoUpperTexture'), (TORSO_LOWER, 'TorsoLowerTexture'),
                (LEG_UPPER, 'LegUpperTexture'), (LEG_LOWER, 'LegLowerTexture'), (FOOT, 'FootTexture')]
# Slots in CreatureDisplayInfoExtra's order (head, shoulder, shirt, chest, belt, legs, feet, wrist, hands, tabard,
# cape), and the order the client paints the body: shirt, then wrist, legs, feet, chest, hands, belt, tabard.
PAINT_ORDER = [2, 7, 5, 6, 3, 8, 4, 9]
CHAR_SKIN, CHAR_FACE, CHAR_FACIAL_HAIR, CHAR_HAIR, CHAR_UNDERWEAR = 0, 1, 2, 3, 4
RACE_PREFIX = {1: 'Human', 3: 'Dwarf', 4: 'NightElf', 7: 'Gnome'}

# name: race, sex (0 male), skin, face, hair style, hair color, facial hair, items by slot (ItemDisplayInfo ids).
NPCS = {
    # The Keeper of the Ember (prestige.cpp): a bearded dwarf in the Black Tuxedo (6834), its pants (6835), Dress Shoes
    # (6836) and the Gilneas top hat (its display: patch_creatures.py's).
    'EF-Keeper': dict(race=3, sex=0, skin=2, face=4, hair_style=4, hair_color=7, facial=6,
                      items={0: 90002, 3: 13116, 5: 13117, 6: 16368}),
}


class Dbc:
    def __init__(self, path):
        data = path.read_bytes()
        count, fields, size, _ = struct.unpack('<4I', data[4:20])
        self.rows = [struct.unpack_from(f'<{fields}I', data, 20 + i * size) for i in range(count)]
        self.strings = data[20 + count * size:]
        self.by_id = {row[0]: row for row in self.rows}

    def text(self, offset):
        return self.strings[offset:self.strings.index(b'\0', offset)].decode()


class Files:
    def __init__(self, roots):
        self.paths = {}
        for root in roots:
            for path in root.rglob('*'):
                if path.is_file():
                    self.paths[str(path.relative_to(root)).replace('/', '\\').lower()] = path

    def image(self, name, opaque=False):
        name = name if name.lower().endswith('.blp') else name + '.blp'
        found = self.paths.get(name.lower())
        if not found:
            return None
        image = Image.open(found).convert('RGBA')
        # An item texture that says it has alpha but holds none (all zero; Cloth_B_08Black_Chest_TU) is opaque in
        # game. (A scalp's empty alpha is real: nothing to draw.)
        if opaque and image.getchannel('A').getextrema()[1] == 0:
            image.putalpha(255)
        return image

    def dbc(self, name):
        return Dbc(self.paths['dbfilesclient\\' + name.lower()])


def paste(canvas, image, region):
    if image is None:
        return
    x, y, w, h = region
    canvas.alpha_composite(image.resize((w, h), Image.LANCZOS), (x, y))


def bake(files, npc):
    sections, items = files.dbc('CharSections.dbc'), files.dbc('ItemDisplayInfo.dbc')

    def section(kind, variation, color):
        for row in sections.rows:
            if row[1] == npc['race'] and row[2] == npc['sex'] and row[3] == kind and row[8] == variation \
                    and row[9] == color:
                return [files.image(sections.text(row[i])) if row[i] else None for i in (4, 5, 6)]
        raise KeyError(f'no CharSections row {kind} {variation} {color}')

    canvas = Image.new('RGBA', (256, 256), (0, 0, 0, 255))
    skin = section(CHAR_SKIN, 0, npc['skin'])[0]
    canvas.alpha_composite(skin.resize((256, 256), Image.LANCZOS))
    lower, upper, _ = section(CHAR_FACE, npc['face'], npc['skin'])
    paste(canvas, lower, FACE_LOWER)
    paste(canvas, upper, FACE_UPPER)
    lower, upper, _ = section(CHAR_FACIAL_HAIR, npc['facial'], npc['hair_color'])
    paste(canvas, lower, FACE_LOWER)
    paste(canvas, upper, FACE_UPPER)
    _, lower, upper = section(CHAR_HAIR, npc['hair_style'], npc['hair_color'])
    paste(canvas, lower, FACE_LOWER)
    paste(canvas, upper, FACE_UPPER)
    suffix = 'F' if npc['sex'] else 'M'
    for slot in PAINT_ORDER:
        display = items.by_id.get(npc['items'].get(slot, 0))
        if not display:
            continue
        for index, (region, folder) in enumerate(ITEM_REGIONS):
            name = items.text(display[15 + index])
            if not name:
                continue
            base = f'Item\\TextureComponents\\{folder}\\{name}'
            image = files.image(f'{base}_{suffix}', True) or files.image(f'{base}_U', True) or files.image(base, True)
            if image is None:
                raise FileNotFoundError(base)
            paste(canvas, image, region)
    return canvas


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    files = Files([Path(p) for p in sys.argv[1].split(':')])
    OUT.mkdir(parents=True, exist_ok=True)
    for name, npc in NPCS.items():
        bake(files, npc).save(OUT / f'{name}.png')
        print(f'Baked {OUT / name}.png')


if __name__ == '__main__':
    main()
