#!/usr/bin/env python3
"""Add Leveling Scaled gear's suffixes to the client's ItemRandomSuffix.dbc (WoW 3.3.5a), and Fireforged gear's
Titanforged suffixes and modifiers (SpellItemEnchantment.dbc: firepit_gear.json, tools/scaling/firepit_gear.py).

The server reads the same rows from the world DB (itemrandomsuffix_dbc in data/sql/db-world/titan_scaled_gear.sql);
the client needs them in its own file to name the suffix ("(Leveling Scaled)") and show its stats in the tooltip.
They come from scaled_gear.json, which tools/scaling/scaled_gear.py writes along with the SQL. The pool's Item.dbc
rows (a template per piece and level) are added by patch_items.py.

Usage: python3 patch_scaled_gear.py <dir with original DBCs> <output dir>
"""

import json
import sys
from pathlib import Path

from patch_items import Dbc

HERE = Path(__file__).resolve().parent
SUFFIX_FIELDS = 29        # ID, Name_Lang[16] + mask, InternalName, Enchantment[5], AllocationPct[5]
ENCHANT_FIELDS = 38       # SpellItemEnchantment: ID, Charges, Effect, Min, Max, Arg (3 each), Name_Lang[16]...
LANG_MASK = 16712190


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    source, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    data = json.loads((HERE / 'scaled_gear.json').read_text())
    # Fireforged gear's Titanforged suffixes (tools/scaling/firepit_gear.py): every stat doubled.
    data['suffixes'] += json.loads((HERE / 'firepit_gear.json').read_text())['suffixes']

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

    # Fireforged gear's modifiers (firepit_gear.json): famous procs as enchantments, named for the tooltip.
    enchants = Dbc(source / 'SpellItemEnchantment.dbc', ENCHANT_FIELDS)
    ours = json.loads((HERE / 'firepit_gear.json').read_text())['enchants']
    for enchant_id, effect, amount, spell, name in ours:
        row = [0] * ENCHANT_FIELDS
        row[0] = enchant_id
        row[2] = effect                         # Effect[0]
        row[5] = row[8] = amount                # EffectPointsMin[0], Max[0]: a combat spell's chance on hit
        row[11] = spell                         # EffectArg[0]
        row[14] = enchants.add_string(name)     # Name_Lang enUS
        row[30] = LANG_MASK
        enchants.rows[enchant_id] = tuple(row)
    enchants.save(out / 'SpellItemEnchantment.dbc')
    print(f'Wrote {out / "SpellItemEnchantment.dbc"} ({len(ours)} Fireforged modifiers)')


if __name__ == '__main__':
    main()
