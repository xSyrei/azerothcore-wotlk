#!/usr/bin/env python3
"""Add Emberforge's custom creature models to CreatureModelData.dbc and CreatureDisplayInfo.dbc (WoW 3.3.5a).

A creature display (what a creature, or a player under a transform, looks like) names a CreatureModelData
row, which names the model file. Each new row copies a Blizzard row (sounds, blood, footprints) and changes
the model, its size and its id. The server needs the same rows (spells, scripts and `.morph` check them):
--sql writes them for `creaturemodeldata_dbc`, `creaturedisplayinfo_dbc` and `creature_model_info`.

Usage: python3 patch_creatures.py <dir with original DBCs> <output dir> [--sql <out.sql>]
"""

import struct
import sys
from pathlib import Path

from patch_items import Dbc

MODEL_FIELDS = 28
DISPLAY_FIELDS = 16

# The Demon Hunter's demon forms (Metamorphosis), converted by models/creature_gltf_to_m2.py. They copy the
# warlock's Metamorphosis demon (model 2927, display 25277): its creature sounds, blood and footprints.
# (display id, model id, model file, template model, template display, display scale, gender, geo box)
CREATURES = [
    (90201, 3501, 'Creature\\DemonHunterMeta\\DHMetaMale.mdx', 2927, 25277, 0.6, 0,
     (-3.87, -1.33, -0.01, 0.79, 1.33, 6.7)),
    (90202, 3502, 'Creature\\DemonHunterMeta\\DHMetaFemale.mdx', 2927, 25277, 0.6, 1,
     (-4.43, -1.64, 0.0, 0.5, 1.64, 7.03)),
    # Their Blade Dance variants, shown for the length of the dance (spell_titan_dh_blade_dance)
    (90203, 3503, 'Creature\\DemonHunterMeta\\DHMetaMaleBladeDance.mdx', 2927, 25277, 0.6, 0,
     (-3.87, -1.33, -0.01, 0.79, 1.33, 6.7)),
    (90204, 3504, 'Creature\\DemonHunterMeta\\DHMetaFemaleBladeDance.mdx', 2927, 25277, 0.6, 1,
     (-4.43, -1.64, 0.0, 0.5, 1.64, 7.03)),
]
OTHER_GENDER = {90201: 90202, 90202: 90201, 90203: 90204, 90204: 90203}
BOUNDING_RADIUS = 0.6
COMBAT_REACH = 2.0


def as_float(value):
    return struct.unpack('<f', struct.pack('<I', value))[0]


def as_bits(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]


def patch(source, out):
    models = Dbc(source / 'CreatureModelData.dbc', MODEL_FIELDS)
    displays = Dbc(source / 'CreatureDisplayInfo.dbc', DISPLAY_FIELDS)
    for display_id, model_id, model_file, template_model, template_display, scale, _, box in CREATURES:
        row = list(models.rows[template_model])
        row[0] = model_id
        row[1] = 0                  # Flags: the warlock demon's (0x4, 0x10) hide the weapons; Illidan's model has none
        row[2] = models.add_string(model_file)              # ModelName
        row[17:23] = [as_bits(v) for v in box]              # GeoBoxMin / GeoBoxMax
        models.rows[model_id] = tuple(row)
        row = list(displays.rows[template_display])
        row[0] = display_id
        row[1] = model_id
        row[4] = as_bits(scale)                             # CreatureModelScale
        row[6:9] = [0, 0, 0]                                # no texture variations: the model names its textures
        displays.rows[display_id] = tuple(row)
    out.mkdir(parents=True, exist_ok=True)
    models.save(out / 'CreatureModelData.dbc')
    displays.save(out / 'CreatureDisplayInfo.dbc')
    return models, displays


def sql_value(value, is_float):
    return repr(round(as_float(value), 6)) if is_float else str(struct.unpack('<i', struct.pack('<I', value))[0])


def write_sql(models, displays, path):
    model_floats = {4, 7, 8, 9, 14, 15, 16} | set(range(17, 28))
    display_floats = {4}
    model_ids = ', '.join(str(c[1]) for c in CREATURES)
    display_ids = ', '.join(str(c[0]) for c in CREATURES)
    lines = ['-- Emberforge creature displays (the Demon Hunter\'s demon forms): written by',
             '-- client-patch/patch_creatures.py --sql, which puts the same rows in the client patch.',
             f'DELETE FROM `creaturemodeldata_dbc` WHERE `ID` IN ({model_ids});',
             'INSERT INTO `creaturemodeldata_dbc` VALUES']
    rows = []
    for c in CREATURES:
        row = models.rows[c[1]]
        values = [f"'{c[2].replace(chr(92), chr(92) * 2)}'" if i == 2 else sql_value(v, i in model_floats)
                  for i, v in enumerate(row)]
        rows.append('(' + ','.join(values) + ')')
    lines.append(',\n'.join(rows) + ';')
    lines += ['', f'DELETE FROM `creaturedisplayinfo_dbc` WHERE `ID` IN ({display_ids});',
              'INSERT INTO `creaturedisplayinfo_dbc` VALUES']
    rows = []
    for c in CREATURES:
        row = displays.rows[c[0]]
        values = ["''" if i in (6, 7, 8, 9) else sql_value(v, i in display_floats) for i, v in enumerate(row)]
        rows.append('(' + ','.join(values) + ')')
    lines.append(',\n'.join(rows) + ';')
    lines += ['', f'DELETE FROM `creature_model_info` WHERE `DisplayID` IN ({display_ids});',
              'INSERT INTO `creature_model_info` (`DisplayID`, `BoundingRadius`, `CombatReach`, `Gender`, '
              '`DisplayID_Other_Gender`) VALUES']
    lines.append(',\n'.join(f'({c[0]},{BOUNDING_RADIUS},{COMBAT_REACH},{c[6]},{OTHER_GENDER[c[0]]})'
                            for c in CREATURES) + ';')
    Path(path).write_text('\n'.join(lines) + '\n')


def main():
    args = sys.argv[1:]
    sql = None
    if '--sql' in args:
        sql = args[args.index('--sql') + 1]
        del args[args.index('--sql'):args.index('--sql') + 2]
    if len(args) != 2:
        sys.exit(__doc__)
    source, out = Path(args[0]), Path(args[1])
    models, displays = patch(source, out)
    print(f'Wrote {out / "CreatureModelData.dbc"} and {out / "CreatureDisplayInfo.dbc"} ({len(CREATURES)} displays)')
    if sql:
        write_sql(models, displays, sql)
        print(f'Wrote {sql}')


if __name__ == '__main__':
    main()
