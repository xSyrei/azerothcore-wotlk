#!/usr/bin/env python3
"""Build the client DBC files for the classless "Hero" (WoW 3.3.5a, build 12340).

Reads the original ChrClasses.dbc, CharBaseInfo.dbc and CharStartOutfit.dbc and writes patched
copies:
  - ChrClasses.dbc: the hidden base class (Paladin) is renamed "Hero".
  - CharBaseInfo.dbc: Hero is the only class every race can pick on the
    character creation screen; all other classes show as unavailable.
  - CharStartOutfit.dbc: races that could never be Paladins get the Paladin starting outfit, so
    they are not shown naked on the creation screen (matches data/sql/db-world/hero_outfit.sql).

Usage: python3 patch_hero_class.py <dir with original DBCs> <output dir>
"""

import struct
import sys
from pathlib import Path

HERO_CLASS = 2        # Paladin; must match @HERO in data/sql/db-world/hero_class.sql
HERO_NAME = "Hero"
PLAYABLE_RACES = [1, 2, 3, 4, 5, 6, 7, 8, 10, 11]

# ChrClasses.dbc (3.3.5a): 60 uint32 fields. Name blocks are 16 locale string
# offsets plus a flags field: base name 4-19, female 21-36, male 38-53.
CHRCLASSES_FIELDS = 60
CHRCLASSES_NAME_FIELDS = [*range(4, 20), *range(21, 37), *range(38, 54)]
CHRCLASSES_ENUS_NAME = 4

# CharBaseInfo.dbc (3.3.5a): one byte race, one byte class per record.
CHARBASEINFO_FIELDS = 2
CHARBASEINFO_RECORD_SIZE = 2


class Dbc:
    def __init__(self, path):
        data = Path(path).read_bytes()
        magic, self.count, self.fields, self.record_size, string_size = struct.unpack_from("<4s4I", data)
        if magic != b"WDBC":
            raise ValueError(f"{path}: not a WDBC file")
        start = 20
        end = start + self.count * self.record_size
        self.records = [bytearray(data[start + i * self.record_size:start + (i + 1) * self.record_size])
                        for i in range(self.count)]
        self.strings = bytearray(data[end:end + string_size])

    def add_string(self, text):
        encoded = text.encode("utf-8") + b"\0"
        found = self.strings.find(b"\0" + encoded)
        if found != -1:
            return found + 1
        offset = len(self.strings)
        self.strings += encoded
        return offset

    def save(self, path):
        header = struct.pack("<4s4I", b"WDBC", len(self.records), self.fields, self.record_size, len(self.strings))
        Path(path).write_bytes(header + b"".join(self.records) + self.strings)


def require_layout(dbc, name, fields, record_size):
    if dbc.fields != fields or dbc.record_size != record_size:
        raise ValueError(f"{name}: expected {fields} fields / {record_size}-byte records, "
                         f"got {dbc.fields} / {dbc.record_size}. Is this a 3.3.5a (12340) file?")


def patch_chr_classes(dbc):
    require_layout(dbc, "ChrClasses.dbc", CHRCLASSES_FIELDS, CHRCLASSES_FIELDS * 4)
    hero = dbc.add_string(HERO_NAME)
    for record in dbc.records:
        if struct.unpack_from("<I", record, 0)[0] != HERO_CLASS:
            continue
        for field in CHRCLASSES_NAME_FIELDS:
            offset = struct.unpack_from("<I", record, field * 4)[0]
            if offset or field == CHRCLASSES_ENUS_NAME:
                struct.pack_into("<I", record, field * 4, hero)
        return
    raise ValueError(f"ChrClasses.dbc: class {HERO_CLASS} not found")


def patch_char_base_info(dbc):
    require_layout(dbc, "CharBaseInfo.dbc", CHARBASEINFO_FIELDS, CHARBASEINFO_RECORD_SIZE)
    dbc.records = [bytearray([race, HERO_CLASS]) for race in PLAYABLE_RACES]


# CharStartOutfit.dbc (3.3.5a): ID, then race / class / sex / outfit as four bytes, then 24 item
# IDs, 24 display IDs and 24 inventory types.
OUTFIT_RECORD = struct.Struct('<I4B72i')
OUTFIT_FIELDS = 77
OUTFIT_TEMPLATE_RACE = 1   # Human: copy its Paladin outfit to the races that had none


def patch_char_start_outfit(dbc):
    require_layout(dbc, "CharStartOutfit.dbc", OUTFIT_FIELDS, OUTFIT_RECORD.size)
    rows = [list(OUTFIT_RECORD.unpack(bytes(r))) for r in dbc.records]
    have = {(r[1], r[3]) for r in rows if r[2] == HERO_CLASS}
    templates = {r[3]: r for r in rows if r[1] == OUTFIT_TEMPLATE_RACE and r[2] == HERO_CLASS}
    next_id = max(r[0] for r in rows) + 1
    added = 0
    for race in PLAYABLE_RACES:
        for sex in (0, 1):
            if (race, sex) in have:
                continue
            row = list(templates[sex])
            row[0], row[1] = next_id, race
            next_id += 1
            rows.append(row)
            added += 1
    dbc.records = [bytearray(OUTFIT_RECORD.pack(*r)) for r in rows]
    return added


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    source, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)

    classes = Dbc(source / "ChrClasses.dbc")
    patch_chr_classes(classes)
    classes.save(out / "ChrClasses.dbc")

    base_info = Dbc(source / "CharBaseInfo.dbc")
    patch_char_base_info(base_info)
    base_info.save(out / "CharBaseInfo.dbc")

    outfits = Dbc(source / "CharStartOutfit.dbc")
    added = patch_char_start_outfit(outfits)
    outfits.save(out / "CharStartOutfit.dbc")

    print(f"Wrote {out / 'ChrClasses.dbc'} (class {HERO_CLASS} renamed to {HERO_NAME})")
    print(f"Wrote {out / 'CharStartOutfit.dbc'} ({added} {HERO_NAME} outfits added)")
    print(f"Wrote {out / 'CharBaseInfo.dbc'} ({HERO_NAME} only, {len(PLAYABLE_RACES)} races)")


if __name__ == "__main__":
    main()
