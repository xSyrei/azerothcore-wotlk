#!/usr/bin/env python3
"""Add Emberforge's custom creature models to CreatureModelData.dbc and CreatureDisplayInfo.dbc (WoW 3.3.5a), and our
dressed humanoids (HUMANOIDS: a race's own model, a CreatureDisplayInfoExtra row with its look, worn items and baked
skin, bake_npc.py's) to CreatureDisplayInfo.dbc and CreatureDisplayInfoExtra.dbc.

A creature display (what a creature, or a player under a transform, looks like) names a CreatureModelData
row, which names the model file. Each new row copies a Blizzard row (sounds, blood, footprints) and changes
the model, its size and its id. The server needs the same rows (spells, scripts and `.morph` check them):
--sql writes them for `creaturemodeldata_dbc`, `creaturedisplayinfo_dbc` and `creature_model_info`.

Usage: python3 patch_creatures.py <dir with original DBCs> <output dir> [--sql <out.sql>]
"""

import struct
import sys
from pathlib import Path

import bake_npc
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
    # Chi-Ji, the Red Crane (Invoke Chi-Ji's guardian, creature 90311), copying the phoenix's sounds (model 2384,
    # display 19682), at 80%.
    (90205, 3505, 'Creature\\ChiJi\\ChiJi.mdx', 2384, 19682, 0.8, 2, (-1.34, -0.51, 0.0, 1.47, 0.51, 3.23)),
    # The Flame Cat and the Ice Cat (a Hero's Cat Form with Ember's Wrath or Whitefang, runes.cpp), copying the Night
    # Elf Cat Form's sounds (model 1231, display 892), at 80%.
    (90207, 3507, 'Creature\\FlameCat\\FlameCat.mdx', 1231, 892, 0.8, 2, (-2.67, -1.03, -0.16, 1.33, 1.03, 1.72)),
    (90208, 3508, 'Creature\\FlameCat\\IceCat.mdx', 1231, 892, 0.8, 2, (-2.67, -1.03, -0.16, 1.33, 1.03, 1.72)),
]
OTHER_GENDER = {90201: 90202, 90202: 90201, 90203: 90204, 90204: 90203, 90205: 0, 90207: 0, 90208: 0}
BOUNDING_RADIUS = 0.6
COMBAT_REACH = 2.0
EXTRA_FIELDS = 21

# Our gameobject models (GameObjectDisplayInfo.dbc): (display id, model file, a Blizzard display to copy (its sounds),
# geo box). The Firepit's end-of-run cache (retail's challenge chest, models/firepit_cache), copying the chest
# Chest01's (display 10: its open and close sounds).
GAMEOBJECT_FIELDS = 19
GAMEOBJECTS = [
    (9700, 'World\\Emberforge\\FirepitCache.mdx', 10, (-1.29, -1.85, -0.03, 1.2, 1.86, 2.41)),
    # The Firepit's area decals (patch_runes.FIREPIT_DECALS), 16 a colour (danger, safe, soak, heal, arcane, frost):
    # its outlines (9701 + 16 x colour on) and its timers (8 on), flat on the floor, a yard of radius (the server
    # scales each).
    *[(9701 + 16 * c + 8 * timer + i,
       f'Spells\\Emberforge\\FP_{"Timer" if timer else "Decal"}_{colour}_{shape}.mdx', 10,
       (-1.1, -1.1, 0.0, 1.1, 1.1, 0.3))
      for c, colour in enumerate(('danger', 'safe', 'soak', 'heal', 'arcane', 'frost')) for timer in (0, 1)
      for i, shape in enumerate(('circle', 'ring', 'cone90', 'cone180', 'cone180L', 'cone120L', 'line_mid',
                                 'line_cap'))],
    # Their textured overlays (patch_runes.DECAL_STYLES: runic, cracked, hazard, swirl), 48 a style from 9797 (8 a
    # colour).
    *[(9797 + 48 * st + 8 * c + i, f'Spells\\Emberforge\\FP_Style_{style}_{colour}_{shape}.mdx', 10,
       (-1.1, -1.1, 0.0, 1.1, 1.1, 0.3))
      for st, style in enumerate(('runic', 'cracked', 'hazard', 'swirl'))
      for c, colour in enumerate(('danger', 'safe', 'soak', 'heal', 'arcane', 'frost'))
      for i, shape in enumerate(('circle', 'ring', 'cone90', 'cone180', 'cone180L', 'cone120L', 'line_mid',
                                 'line_cap'))],
]

# (display id, a Blizzard display of the race and sex to copy (its model and sounds), bake_npc.NPCS's name, bounding
# radius, combat reach): the extra row takes the display's id.
HUMANOIDS = [
    # The Keeper of the Ember (prestige.cpp): a dwarf male (display 825's model, 53), in a tuxedo and top hat
    (90206, 825, 'EF-Keeper', 0.347, 1.5),
]


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
    extras = Dbc(source / 'CreatureDisplayInfoExtra.dbc', EXTRA_FIELDS)
    for display_id, template_display, name, _, _ in HUMANOIDS:
        npc = bake_npc.NPCS[name]
        row = list(displays.rows[template_display])
        row[0], row[3] = display_id, display_id         # ID, ExtendedDisplayInfoID
        row[6:9] = [0, 0, 0]
        displays.rows[display_id] = tuple(row)
        items = [npc['items'].get(slot, 0) for slot in range(11)]
        extras.rows[display_id] = (display_id, npc['race'], npc['sex'], npc['skin'], npc['face'], npc['hair_style'],
                                   npc['hair_color'], npc['facial'], *items, 0, extras.add_string(f'{name}.blp'))
    objects = Dbc(source / 'GameObjectDisplayInfo.dbc', GAMEOBJECT_FIELDS)
    for display_id, model_file, template, box in GAMEOBJECTS:
        row = list(objects.rows[template])
        row[0] = display_id
        row[1] = objects.add_string(model_file)
        row[12:18] = [as_bits(v) for v in box]              # GeoBoxMin / GeoBoxMax
        objects.rows[display_id] = tuple(row)
    out.mkdir(parents=True, exist_ok=True)
    objects.save(out / 'GameObjectDisplayInfo.dbc')
    patch.objects = objects
    models.save(out / 'CreatureModelData.dbc')
    displays.save(out / 'CreatureDisplayInfo.dbc')
    extras.save(out / 'CreatureDisplayInfoExtra.dbc')
    return models, displays


def sql_value(value, is_float):
    return repr(round(as_float(value), 6)) if is_float else str(struct.unpack('<i', struct.pack('<I', value))[0])


def write_sql(models, displays, path):
    model_floats = {4, 7, 8, 9, 14, 15, 16} | set(range(17, 28))
    display_floats = {4}
    model_ids = ', '.join(str(c[1]) for c in CREATURES)
    display_ids = ', '.join(str(c[0]) for c in CREATURES + HUMANOIDS)
    lines = ['-- Emberforge creature displays (the Demon Hunter\'s demon forms, Chi-Ji, the Keeper of the Ember):',
             '-- written by client-patch/patch_creatures.py --sql, which puts the same rows in the client patch.',
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
    for c in CREATURES + HUMANOIDS:
        row = displays.rows[c[0]]
        values = ["''" if i in (6, 7, 8, 9) else sql_value(v, i in display_floats) for i, v in enumerate(row)]
        rows.append('(' + ','.join(values) + ')')
    lines.append(',\n'.join(rows) + ';')
    humanoid_ids = ', '.join(str(h[0]) for h in HUMANOIDS)
    lines += ['', f'DELETE FROM `creaturedisplayinfoextra_dbc` WHERE `ID` IN ({humanoid_ids});',
              'INSERT INTO `creaturedisplayinfoextra_dbc` VALUES']
    rows = []
    for display_id, _, name, _, _ in HUMANOIDS:
        npc = bake_npc.NPCS[name]
        items = [npc['items'].get(slot, 0) for slot in range(11)]
        values = [display_id, npc['race'], npc['sex'], npc['skin'], npc['face'], npc['hair_style'], npc['hair_color'],
                  npc['facial'], *items, 0]
        rows.append('(' + ','.join(str(v) for v in values) + f",'{name}.blp')")
    lines.append(',\n'.join(rows) + ';')
    lines += ['', f'DELETE FROM `creature_model_info` WHERE `DisplayID` IN ({display_ids});',
              'INSERT INTO `creature_model_info` (`DisplayID`, `BoundingRadius`, `CombatReach`, `Gender`, '
              '`DisplayID_Other_Gender`) VALUES']
    lines.append(',\n'.join([f'({c[0]},{BOUNDING_RADIUS},{COMBAT_REACH},{c[6]},{OTHER_GENDER[c[0]]})'
                              for c in CREATURES] + [f'({h[0]},{h[3]},{h[4]},{bake_npc.NPCS[h[2]]["sex"]},0)'
                                                     for h in HUMANOIDS]) + ';')
    objects = patch.objects
    object_ids = ', '.join(str(g[0]) for g in GAMEOBJECTS)
    lines += ['', f'DELETE FROM `gameobjectdisplayinfo_dbc` WHERE `ID` IN ({object_ids});',
              'INSERT INTO `gameobjectdisplayinfo_dbc` VALUES']
    rows = []
    for g in GAMEOBJECTS:
        row = objects.rows[g[0]]
        values = [f"'{g[1].replace(chr(92), chr(92) * 2)}'" if i == 1 else sql_value(v, 12 <= i < 18)
                  for i, v in enumerate(row)]
        rows.append('(' + ','.join(values) + ')')
    lines.append(',\n'.join(rows) + ';')
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
    print(f'Wrote {out / "CreatureModelData.dbc"}, {out / "CreatureDisplayInfo.dbc"} and '
          f'{out / "CreatureDisplayInfoExtra.dbc"} ({len(CREATURES)} model displays, {len(HUMANOIDS)} humanoids)')
    if sql:
        write_sql(models, displays, sql)
        print(f'Wrote {sql}')


if __name__ == '__main__':
    main()
