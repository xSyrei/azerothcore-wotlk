#!/usr/bin/env python3
"""Add Leveling Scaled gear's suffixes and weapon damage enchantments to the client's DBCs (WoW 3.3.5a).

The server reads the same rows from the world DB (itemrandomsuffix_dbc, spellitemenchantment_dbc in
data/sql/db-world/titan_scaled_gear.sql); the client needs them in its own files to name the suffix
("(Leveling Scaled 42)") and show its stats in the tooltip. Both come from scaled_gear.json, which
tools/scaling/scaled_gear.py writes along with the SQL. The pool's Item.dbc rows are added by patch_items.py.

Usage: python3 patch_scaled_gear.py <dir with original DBCs> <output dir>
"""

import json
import sys
from pathlib import Path

from patch_items import Dbc

HERE = Path(__file__).resolve().parent
SUFFIX_FIELDS = 29        # ID, Name_Lang[16] + mask, InternalName, Enchantment[5], AllocationPct[5]
ENCHANT_FIELDS = 38       # ID, Charges, Effect[3], PointsMin[3], PointsMax[3], Arg[3], Name_Lang[16] + mask, ...
LANG_MASK = 16712190
ENCHANT_TYPE_DAMAGE = 2


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    source, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    data = json.loads((HERE / 'scaled_gear.json').read_text())

    enchants = Dbc(source / 'SpellItemEnchantment.dbc', ENCHANT_FIELDS)
    for enchant_id, amount, name in data['enchants']:
        row = [0] * ENCHANT_FIELDS
        row[0] = enchant_id
        row[2] = ENCHANT_TYPE_DAMAGE          # Effect[0]
        row[5] = amount                       # EffectPointsMin[0]
        row[8] = amount                       # EffectPointsMax[0]
        row[14] = enchants.add_string(name)   # Name_Lang enUS
        row[30] = LANG_MASK
        enchants.rows[enchant_id] = tuple(row)
    enchants.save(out / 'SpellItemEnchantment.dbc')
    print(f'Wrote {out / "SpellItemEnchantment.dbc"} ({len(data["enchants"])} damage enchantments)')

    suffixes = Dbc(source / 'ItemRandomSuffix.dbc', SUFFIX_FIELDS)
    for suffix_id, name, internal, enchant_ids, allocations in data['suffixes']:
        row = [0] * SUFFIX_FIELDS
        row[0] = suffix_id
        row[1] = suffixes.add_string(name)    # Name_Lang enUS
        row[17] = LANG_MASK
        row[18] = suffixes.add_string(internal)
        row[19:19 + len(enchant_ids)] = enchant_ids
        row[24:24 + len(allocations)] = allocations
        suffixes.rows[suffix_id] = tuple(row[:SUFFIX_FIELDS])
    suffixes.save(out / 'ItemRandomSuffix.dbc')
    print(f'Wrote {out / "ItemRandomSuffix.dbc"} ({len(data["suffixes"])} suffixes)')


if __name__ == '__main__':
    main()
