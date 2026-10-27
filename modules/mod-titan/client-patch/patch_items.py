#!/usr/bin/env python3
"""Add Emberforge's custom items to the client's Item.dbc and ItemDisplayInfo.dbc (WoW 3.3.5a).

Without a row in Item.dbc the client shows a custom item with a red "?" icon. Each row mirrors
the item's `item_template` values (see data/sql/db-world/). Items with their own model also need
an ItemDisplayInfo row naming the model, its texture and the inventory icon.

Usage: python3 patch_items.py <dir with original DBCs> <output dir>
"""

import struct
import sys
from pathlib import Path

# (entry, class, subclass, SoundOverrideSubclass, Material, displayid, InventoryType, sheath)
ITEMS = [
    (910001, 0, 8, -1, 1, 6689, 0, 0),     # Titan Soul of Rested Experience
    (910002, 2, 7, -1, 1, 90001, 13, 1),   # Veridus (one-handed sword)
    (910003, 0, 8, -1, 1, 1103, 0, 0),     # Titan Tome of Randomization (book, like Blizzard's tomes)
    (910004, 0, 8, -1, 1, 1103, 0, 0),     # Titan Tome of Selection
]

# Custom displays: (id, model file, model texture, inventory icon, item group sound)
# The model lives in Item\ObjectComponents\Weapon\ (weapons), the icon in Interface\Icons\.
DISPLAYS = [
    (90001, 'Sword_1H_Veridus.mdx', 'Sword_1H_Veridus', 'INV_Sword_1H_Veridus', 8),
]

ITEM_FIELDS = 8
DISPLAY_FIELDS = 25


class Dbc:
    def __init__(self, path, fields):
        data = Path(path).read_bytes()
        magic, count, n_fields, record_size, string_size = struct.unpack_from('<4s4I', data)
        if magic != b'WDBC' or n_fields != fields or record_size != fields * 4:
            raise ValueError(f'{path}: unexpected layout ({n_fields} fields / {record_size} bytes). '
                             'Is this a 3.3.5a file?')
        self.fields = fields
        self.rows = {}
        for i in range(count):
            row = struct.unpack_from(f'<{fields}I', data, 20 + i * record_size)
            self.rows[row[0]] = row
        start = 20 + count * record_size
        self.strings = bytearray(data[start:start + string_size])

    def add_string(self, text):
        if not text:
            return 0
        encoded = text.encode('utf-8') + b'\0'
        found = self.strings.find(b'\0' + encoded)
        if found != -1:
            return found + 1
        offset = len(self.strings)
        self.strings += encoded
        return offset

    def save(self, path):
        body = b''.join(struct.pack(f'<{self.fields}I', *self.rows[key]) for key in sorted(self.rows))
        header = struct.pack('<4s4I', b'WDBC', len(self.rows), self.fields, self.fields * 4, len(self.strings))
        Path(path).write_bytes(header + body + self.strings)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    source, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)

    items = Dbc(source / 'Item.dbc', ITEM_FIELDS)
    for row in ITEMS:
        items.rows[row[0]] = tuple(v & 0xFFFFFFFF for v in row)
    items.save(out / 'Item.dbc')
    print(f'Wrote {out / "Item.dbc"} ({len(ITEMS)} custom items)')

    displays = Dbc(source / 'ItemDisplayInfo.dbc', DISPLAY_FIELDS)
    for display_id, model, texture, icon, group_sound in DISPLAYS:
        row = [0] * DISPLAY_FIELDS
        row[0] = display_id
        row[1] = displays.add_string(model)         # ModelName[0]
        row[3] = displays.add_string(texture)       # ModelTexture[0]
        row[5] = displays.add_string(icon)          # InventoryIcon[0]
        row[12] = group_sound                       # GroupSoundIndex
        displays.rows[display_id] = tuple(row)
    displays.save(out / 'ItemDisplayInfo.dbc')
    print(f'Wrote {out / "ItemDisplayInfo.dbc"} ({len(DISPLAYS)} custom displays)')


if __name__ == '__main__':
    main()
