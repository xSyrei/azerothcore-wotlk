#!/usr/bin/env python3
"""Add Emberforge's custom items to the client's Item.dbc (WoW 3.3.5a, build 12340).

Without a row in Item.dbc the client shows a custom item with a red "?" icon. Each row mirrors
the item's `item_template` values (see data/sql/db-world/).

Usage: python3 patch_items.py <dir with original Item.dbc> <output dir>
"""

import struct
import sys
from pathlib import Path

# (entry, class, subclass, SoundOverrideSubclass, Material, displayid, InventoryType, sheath)
ITEMS = [
    (910001, 0, 8, -1, 1, 6689, 0, 0),  # Titan Soul of Rested Experience
]

FIELDS = 8
RECORD = '<8i'


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    source, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)

    data = (source / 'Item.dbc').read_bytes()
    magic, count, fields, record_size, string_size = struct.unpack_from('<4s4I', data)
    if magic != b'WDBC' or fields != FIELDS or record_size != FIELDS * 4:
        sys.exit(f'Item.dbc: unexpected layout ({fields} fields / {record_size} bytes). Is this a 3.3.5a file?')

    records = {}
    for i in range(count):
        row = struct.unpack_from(RECORD, data, 20 + i * record_size)
        records[row[0]] = row
    for row in ITEMS:
        records[row[0]] = row
    strings = data[20 + count * record_size:20 + count * record_size + string_size]

    body = b''.join(struct.pack(RECORD, *records[entry]) for entry in sorted(records))
    header = struct.pack('<4s4I', b'WDBC', len(records), FIELDS, record_size, len(strings))
    (out / 'Item.dbc').write_bytes(header + body + strings)
    print(f'Wrote {out / "Item.dbc"} ({len(ITEMS)} custom items)')


if __name__ == '__main__':
    main()
