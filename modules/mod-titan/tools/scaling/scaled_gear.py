#!/usr/bin/env python3
"""Leveling Scaled gear: the Smoldering Adventurer's Cache's own small pool of items, each copy fixed at the level it
was opened at.

How it scales (no core change): the 3.3.5 client shows per-copy stats only through enchantments, and a random suffix
("of the Bear") is the one an item keeps per copy: its stats are AllocationPct * the suffix factor / 10000. The factor
comes from the item template (its item level, quality and slot: GenerateEnchSuffixFactor, RandPropPoints.dbc) and is
recomputed at every load, so it cannot carry a level; the suffix id is saved with the copy (item_instance's
randomPropertyId) and can. So there is a suffix for every (stat mix, item kind, level), its AllocationPct the level's:
- stats: Blizzard's own mixes (the Wrath "of the Bear" kind) scaled by RandPropPoints(green, item level of the level)
  over the template's (item level 65, the squish's level 60 green);
- armor: Blizzard's "+$i Armor" (2800) in the suffix, by armor type and the exact slot (a green's armor per item level,
  the slot's share of the chest's);
- weapon damage: enchantments take no suffix amount, so one "+X Weapon Damage" enchantment per level and weapon speed
  (a green's damage per second at that item level).
The item level of player level L is L + 5 (60: 65).

Writes:
- data/sql/db-world/titan_scaled_gear.sql: the pool's item_template rows, itemrandomsuffix_dbc and
  spellitemenchantment_dbc (the server reads its DBCs and these rows over them), item_enchantment_template;
- src/scaled_gear.h: the pool and its suffix sets for src/smoldering_cache.cpp;
- client-patch/scaled_gear.json: the same rows for the client patch (client-patch/patch_scaled_gear.py, patch_items.py).

Usage: python3 scaled_gear.py <dir with the 3.3.5a RandPropPoints.dbc>
"""

import json
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
MODULE = HERE.parent.parent
SQL = MODULE / 'data' / 'sql' / 'db-world' / 'titan_scaled_gear.sql'
HEADER = MODULE / 'src' / 'scaled_gear.h'
CLIENT = MODULE / 'client-patch' / 'scaled_gear.json'

FIRST_ITEM = 911000
SUFFIX_BASE = 2000              # Blizzard's suffixes end below 300
ENCHANT_BASE = 30000            # the damage enchantments (Blizzard's end below 4000)
SUFFIX_GROUP = 9110             # item_enchantment_template group the templates name (a GM's .additem rolls one)
LEVELS = 60
TEMPLATE_ITEM_LEVEL = 65
QUALITY_UNCOMMON = 2


def item_level(level):
    return min(level, LEVELS) + 5


# Blizzard's generic "+$i" enchantments, their amount from the suffix (SpellItemEnchantment.dbc).
AGI, STA, INT, STR, SPI, SP, AP, DEF, MP5, ARMOR = 2802, 2803, 2804, 2805, 2806, 2824, 2825, 2813, 2816, 2800

# Stat mixes: Wrath's suffixes' allocations (ItemRandomSuffix.dbc 68 Bear, 78 Monkey, 69 Eagle, 9 Owl, 14 Tiger,
# 85 Sorcerer, 73 Elder, 75 Champion, 50 Hunt).
MIXES = {
    'Bear': [(STR, 6666), (STA, 6666)],
    'Monkey': [(AGI, 6666), (STA, 6666)],
    'Eagle': [(STA, 6666), (INT, 6666)],
    'Owl': [(INT, 6666), (SPI, 6666)],
    'Tiger': [(AGI, 6666), (STR, 6666)],
    'Sorcerer': [(STA, 5259), (INT, 5259), (SP, 6153)],
    'Elder': [(STA, 5259), (INT, 5259), (MP5, 2104)],
    'Champion': [(STR, 5259), (STA, 5259), (DEF, 5259)],
    'Hunt': [(AP, 14532), (AGI, 4106), (INT, 3193)],
}
ALL_MIXES = list(MIXES)

# Armor per item level, a green's chest (median over Blizzard's greens), and each slot's share of the chest's.
ARMOR_PER_LEVEL = {'cloth': 1.32, 'leather': 2.84, 'mail': 5.92, 'plate': 9.53, 'shield': 21.24, 'cloak': 0.67}
SLOT_ARMOR = {1: 0.8125, 3: 0.75, 5: 1.0, 20: 1.0, 9: 0.4375, 10: 0.625, 6: 0.5625, 7: 0.875, 8: 0.6875,
              14: 1.0, 16: 1.0}
# Damage per second per item level, a green's (median), and the speeds the pool's weapons are made at.
DPS_PER_LEVEL = {'one-hand': 0.518, 'dagger': 0.518, 'two-hand': 0.677, 'ranged': 0.555, 'wand': 0.807}
SPEED = {'one-hand': 2600, 'dagger': 1800, 'two-hand': 3400, 'ranged': 2800, 'wand': 1500}
BASE_DAMAGE = (1, 2)            # the templates' own damage; the enchantment adds the rest

ARMOR_MIXES = {'cloth': ['Owl', 'Sorcerer'], 'leather': ['Monkey', 'Owl'],
               'mail': ['Hunt', 'Elder', 'Monkey', 'Bear'], 'plate': ['Bear', 'Champion', 'Sorcerer']}
ARMOR_SUBCLASS = {'cloth': 1, 'leather': 2, 'mail': 3, 'plate': 4}

# The armor sets: (inventory type, name), the look of a Blizzard green family's piece (entry, displayid, material,
# durability): cloth Mystical, leather Wanderer's, mail Engraved, plate Templar.
ARMOR_SETS = {
    'cloth': ('Emberweave', [
        (1, 'Hood', 44717, 7, 45), (3, 'Mantle', 28109, 7, 45), (20, 'Robe', 28112, 7, 70),
        (9, 'Bracers', 28082, 7, 25), (10, 'Gloves', 28083, 7, 25), (6, 'Sash', 28079, 7, 25),
        (7, 'Leggings', 28084, 7, 55),
        (8, 'Slippers', 28080, 7, 35)]),
    'leather': ('Emberhide', [
        (1, 'Cap', 28590, 8, 50), (3, 'Shoulders', 27733, 8, 50), (5, 'Tunic', 27716, 8, 85),
        (9, 'Bracers', 27719, 8, 30), (10, 'Gloves', 27726, 8, 30), (6, 'Belt', 27717, 8, 30),
        (7, 'Pants', 27731, 8, 65), (8, 'Boots', 27718, 8, 45)]),
    'mail': ('Emberlink', [
        (1, 'Coif', 26274, 5, 60), (3, 'Pauldrons', 26273, 5, 60), (5, 'Hauberk', 26267, 5, 100),
        (9, 'Bracers', 26265, 5, 35), (10, 'Gauntlets', 26269, 5, 35), (6, 'Girdle', 26270, 5, 35),
        (7, 'Legguards', 26272, 5, 75), (8, 'Boots', 26264, 5, 50)]),
    'plate': ('Emberforged', [
        (1, 'Helm', 27408, 6, 70), (3, 'Pauldrons', 27414, 6, 70), (5, 'Breastplate', 27407, 6, 115),
        (9, 'Bracers', 27406, 6, 40), (10, 'Gauntlets', 29014, 6, 40), (6, 'Girdle', 27411, 6, 40),
        (7, 'Legplates', 27413, 6, 85), (8, 'Sabatons', 27405, 6, 55)]),
}

# Everything else: (name, class, subclass, inventory type, displayid, material, sheath, durability, kind, mixes).
# kind: 'cloak' / 'shield' (armor by their share), 'none' (no armor), or a weapon speed class.
OTHERS = [
    ('Ember Cloak', 4, 1, 16, 23081, 7, 0, 0, 'cloak', ALL_MIXES),
    ('Ember Pendant', 4, 0, 2, 15420, 3, 0, 0, 'none', ALL_MIXES),
    ('Ember Band', 4, 0, 11, 9846, 5, 0, 0, 'none', ALL_MIXES),
    ('Ember Charm', 4, 0, 12, 6499, -1, 0, 0, 'none', ALL_MIXES),
    ('Ember Bulwark', 4, 6, 14, 18451, 1, 4, 85, 'shield', ['Champion', 'Sorcerer', 'Elder']),
    ('Ember Tome', 4, 0, 23, 6098, -1, 7, 0, 'none', ['Sorcerer', 'Owl', 'Elder', 'Eagle']),
    ('Ember Blade', 2, 7, 13, 26572, 1, 3, 75, 'one-hand', ['Bear', 'Monkey', 'Tiger', 'Champion']),
    ('Ember Mace', 2, 4, 13, 19743, 2, 3, 75, 'one-hand', ['Bear', 'Champion', 'Sorcerer', 'Elder']),
    ('Ember Axe', 2, 0, 13, 19129, 1, 3, 75, 'one-hand', ['Bear', 'Monkey', 'Tiger']),
    ('Ember Dagger', 2, 15, 13, 20573, 1, 3, 55, 'dagger', ['Monkey', 'Tiger', 'Sorcerer']),
    ('Ember Claw', 2, 13, 21, 23270, 1, 7, 65, 'one-hand', ['Monkey', 'Tiger']),
    ('Ember Greatsword', 2, 8, 17, 26586, 1, 1, 85, 'two-hand', ['Bear', 'Tiger', 'Champion']),
    ('Ember Maul', 2, 5, 17, 5232, 2, 1, 85, 'two-hand', ['Bear', 'Tiger', 'Champion']),
    ('Ember Greataxe', 2, 1, 17, 5128, 1, 1, 85, 'two-hand', ['Bear', 'Monkey', 'Tiger', 'Champion']),
    ('Ember Staff', 2, 10, 17, 20357, 2, 2, 85, 'two-hand', ['Owl', 'Sorcerer', 'Elder', 'Monkey']),
    ('Ember Halberd', 2, 6, 17, 22234, 1, 2, 100, 'two-hand', ['Bear', 'Monkey', 'Tiger']),
    ('Ember Longbow', 2, 2, 15, 4426, 2, 0, 65, 'ranged', ['Hunt', 'Monkey']),
    ('Ember Rifle', 2, 3, 26, 6612, 1, 0, 65, 'ranged', ['Hunt', 'Monkey']),
    ('Ember Wand', 2, 19, 26, 20815, 2, 0, 55, 'wand', ['Sorcerer', 'Owl', 'Elder', 'Eagle']),
]
WEAPON_KINDS = list(SPEED)


def rand_prop_points(dbc_dir):
    """RandPropPoints.dbc: item level -> the uncommon (Good) points of its 5 slot categories."""
    data = (Path(dbc_dir) / 'RandPropPoints.dbc').read_bytes()
    count, fields, size, _ = struct.unpack_from('<4I', data, 4)
    points = {}
    for i in range(count):
        row = struct.unpack_from(f'<{fields}I', data, 20 + i * size)
        points[row[0]] = row[11:16]
    return points


def category(inventory_type):
    """GenerateEnchSuffixFactor's slot category (ItemEnchantmentMgr.cpp)."""
    if inventory_type in (1, 4, 5, 7, 17, 20):
        return 0
    if inventory_type in (3, 6, 8, 10, 12):
        return 1
    if inventory_type in (2, 9, 11, 14, 16, 23):
        return 2
    if inventory_type in (13, 21, 22):
        return 3
    return 4


def build(dbc_dir):
    points = rand_prop_points(dbc_dir)
    template = points[TEMPLATE_ITEM_LEVEL]

    # Items, each with the suffix sets it can take.
    items, sets = [], []                     # sets: (key, mix, armor kind, armor share, weapon kind, category)
    set_index = {}

    def set_for(mix, armor, share, weapon, cat):
        key = (mix, armor, share, weapon, cat)
        if key not in set_index:
            set_index[key] = len(sets)
            sets.append(key)
        return set_index[key]

    entry = FIRST_ITEM
    for armor, (prefix, pieces) in ARMOR_SETS.items():
        for inventory_type, name, display, material, durability in pieces:
            cat = category(inventory_type)
            items.append({'entry': entry, 'name': f'{prefix} {name}', 'class': 4, 'subclass': ARMOR_SUBCLASS[armor],
                          'inventory': inventory_type, 'display': display, 'material': material, 'sheath': 0,
                          'durability': durability, 'delay': 0, 'armor_kind': armor,
                          'sets': [set_for(mix, armor, SLOT_ARMOR[inventory_type], None, cat)
                                   for mix in ARMOR_MIXES[armor]]})
            entry += 1
    for name, cls, sub, inventory_type, display, material, sheath, durability, kind, mixes in OTHERS:
        cat = category(inventory_type)
        weapon = kind if kind in WEAPON_KINDS else None
        armor = kind if kind in ('cloak', 'shield') else None
        items.append({'entry': entry, 'name': name, 'class': cls, 'subclass': sub, 'inventory': inventory_type,
                      'display': display, 'material': material, 'sheath': sheath, 'durability': durability,
                      'delay': SPEED[weapon] if weapon else 0, 'armor_kind': armor or '',
                      'sets': [set_for(mix, armor, SLOT_ARMOR.get(inventory_type, 1.0) if armor else 0, weapon, cat)
                               for mix in mixes]})
        entry += 1

    # Weapon damage enchantments: one per weapon kind and level.
    enchants = []
    damage_id = {}
    for k, weapon in enumerate(WEAPON_KINDS):
        for level in range(1, LEVELS + 1):
            per_hit = DPS_PER_LEVEL[weapon] * item_level(level) * SPEED[weapon] / 1000
            amount = max(1, round(per_hit - sum(BASE_DAMAGE) / 2))
            enchant_id = ENCHANT_BASE + k * LEVELS + level - 1
            damage_id[(weapon, level)] = enchant_id
            enchants.append({'id': enchant_id, 'amount': amount, 'name': f'+{amount} Weapon Damage'})

    # Suffixes: one per set and level.
    suffixes = []
    for index, (mix, armor, share, weapon, cat) in enumerate(sets):
        for level in range(1, LEVELS + 1):
            il = item_level(level)
            scale = points[il][0] / template[0]
            parts = [(enchant, round(alloc * scale)) for enchant, alloc in MIXES[mix]]
            if armor:
                armor_value = ARMOR_PER_LEVEL[armor] * il * share
                parts.append((ARMOR, round(armor_value * 10000 / template[cat])))
            if weapon:
                parts.append((damage_id[(weapon, level)], 0))
            suffixes.append({'id': SUFFIX_BASE + index * LEVELS + level - 1,
                             'name': f'(Leveling Scaled {level})', 'internal': f'{mix}_{index}_{level}',
                             'enchants': [p[0] for p in parts], 'alloc': [p[1] for p in parts]})
    return items, sets, enchants, suffixes


def sql_text(text):
    return "'" + text.replace("'", "''") + "'"


def write_sql(items, enchants, suffixes):
    first, last = SUFFIX_BASE, suffixes[-1]['id']
    lines = [
        '-- Generated by tools/scaling/scaled_gear.py: Leveling Scaled gear, the pool of the Smoldering',
        '-- Adventurer\'s Cache (src/smoldering_cache.cpp). Do not edit by hand; see the script for how it scales.',
        f'DELETE FROM `item_template` WHERE `entry` BETWEEN {FIRST_ITEM} AND {FIRST_ITEM + 999};',
        'INSERT INTO `item_template` (`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`,',
        '    `Quality`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`,',
        '    `stackable`, `dmg_min1`, `dmg_max1`, `delay`, `bonding`, `Material`, `sheath`, `MaxDurability`,',
        '    `RandomSuffix`, `VerifiedBuild`) VALUES',
    ]
    rows = []
    for item in items:
        weapon = item['delay'] > 0
        rows.append(f"({item['entry']}, {item['class']}, {item['subclass']}, -1, {sql_text(item['name'])}, "
                    f"{item['display']}, {QUALITY_UNCOMMON}, 1, {item['inventory']}, -1, -1, "
                    f"{TEMPLATE_ITEM_LEVEL}, 0, 1, {BASE_DAMAGE[0] if weapon else 0}, "
                    f"{BASE_DAMAGE[1] if weapon else 0}, {item['delay']}, 1, {item['material']}, {item['sheath']}, "
                    f"{item['durability']}, {SUFFIX_GROUP}, 0)")
    lines.append(',\n'.join(rows) + ';')
    lines += [
        '',
        '-- The weapon damage enchantments (+X Weapon Damage, one per weapon speed and level).',
        f'DELETE FROM `spellitemenchantment_dbc` WHERE `ID` BETWEEN {ENCHANT_BASE} AND {ENCHANT_BASE + 9999};',
        'INSERT INTO `spellitemenchantment_dbc` (`ID`, `Charges`, `Effect_1`, `Effect_2`, `Effect_3`,',
        '    `EffectPointsMin_1`, `EffectPointsMin_2`, `EffectPointsMin_3`, `EffectPointsMax_1`, `EffectPointsMax_2`,',
        '    `EffectPointsMax_3`, `EffectArg_1`, `EffectArg_2`, `EffectArg_3`, `Name_Lang_enUS`, `Name_Lang_Mask`,',
        '    `ItemVisual`, `Flags`, `Src_ItemID`, `Condition_Id`, `RequiredSkillID`, `RequiredSkillRank`,',
        '    `MinLevel`) VALUES',
    ]
    lines.append(',\n'.join(f"({e['id']}, 0, 2, 0, 0, {e['amount']}, 0, 0, {e['amount']}, 0, 0, 0, 0, 0, "
                            f"{sql_text(e['name'])}, 16712190, 0, 0, 0, 0, 0, 0, 0)" for e in enchants) + ';')
    lines += [
        '',
        '-- The suffixes: one per stat mix, item kind and level (its allocations the level\'s).',
        f'DELETE FROM `itemrandomsuffix_dbc` WHERE `ID` BETWEEN {first} AND {first + 19999};',
        'INSERT INTO `itemrandomsuffix_dbc` (`ID`, `Name_Lang_enUS`, `Name_Lang_Mask`, `InternalName`,',
        '    `Enchantment_1`, `Enchantment_2`, `Enchantment_3`, `Enchantment_4`, `Enchantment_5`, `AllocationPct_1`,',
        '    `AllocationPct_2`, `AllocationPct_3`, `AllocationPct_4`, `AllocationPct_5`) VALUES',
    ]
    rows = []
    for s in suffixes:
        e = s['enchants'] + [0] * (5 - len(s['enchants']))
        a = s['alloc'] + [0] * (5 - len(s['alloc']))
        rows.append(f"({s['id']}, {sql_text(s['name'])}, 16712190, {sql_text(s['internal'])}, "
                    + ', '.join(map(str, e)) + ', ' + ', '.join(map(str, a)) + ')')
    lines.append(',\n'.join(rows) + ';')
    level60 = [s for s in suffixes if s['name'].endswith(f' {LEVELS})')][:1]
    lines += [
        '',
        f'-- The group the templates name (RandomSuffix {SUFFIX_GROUP}): only for items made without the cache (a',
        '-- GM\'s .additem), a level 60 piece; the cache gives the suffix itself.',
        f'DELETE FROM `item_enchantment_template` WHERE `entry` = {SUFFIX_GROUP};',
        'INSERT INTO `item_enchantment_template` (`entry`, `ench`, `chance`) VALUES',
        f"({SUFFIX_GROUP}, {level60[0]['id']}, 100);",
        '',
    ]
    SQL.write_text('\n'.join(lines))


def write_header(items, sets):
    lines = [
        '// Generated by tools/scaling/scaled_gear.py: the Leveling Scaled pool and its suffix sets. Do not edit.',
        '#ifndef MOD_TITAN_SCALED_GEAR_H',
        '#define MOD_TITAN_SCALED_GEAR_H',
        '',
        '#include "Define.h"',
        '#include <array>',
        '',
        'namespace ScaledGear',
        '{',
        f'    constexpr uint32 SUFFIX_BASE = {SUFFIX_BASE};',
        f'    constexpr uint32 LEVELS = {LEVELS};',
        f'    constexpr uint32 SETS = {len(sets)};',
        f'    constexpr uint32 FIRST_ITEM = {FIRST_ITEM};',
        f'    constexpr uint32 LAST_ITEM = {items[-1]["entry"]};',
        '',
        '    // A pool item and the suffix sets it can take (set: SUFFIX_BASE + set * LEVELS + level - 1).',
        '    struct Item',
        '    {',
        '        uint32 Entry;',
        '        uint8 ArmorType;            // ITEM_SUBCLASS_ARMOR_CLOTH..PLATE for the armor sets, 0 otherwise',
        '        uint8 SetCount;',
        '        std::array<uint16, 9> Sets;',
        '    };',
        '',
        '    constexpr Item ITEMS[] = {',
    ]
    for item in items:
        armor = ARMOR_SUBCLASS.get(item['armor_kind'], 0)
        padded = item['sets'] + [0] * (9 - len(item['sets']))
        sets = ', '.join(map(str, padded))
        lines.append(f"        {{ {item['entry']}, {armor}, {len(item['sets'])}, {{ {sets} }} }},"
                     f"   // {item['name']}")
    lines += [
        '    };',
        '',
        '    inline uint32 Suffix(uint16 set, uint8 level)',
        '    {',
        '        return SUFFIX_BASE + set * LEVELS + (level ? level : 1) - 1;',
        '    }',
        '',
        '    // The level a Leveling Scaled suffix was made at, or 0 when it is not one.',
        '    inline uint8 LevelOf(int32 randomPropertyId)',
        '    {',
        '        if (randomPropertyId >= 0 || uint32(-randomPropertyId) < SUFFIX_BASE',
        '            || uint32(-randomPropertyId) >= SUFFIX_BASE + SETS * LEVELS)',
        '            return 0;',
        '        return uint8((uint32(-randomPropertyId) - SUFFIX_BASE) % LEVELS + 1);',
        '    }',
        '',
        '    // Its item level (a green\'s at that level: level + 5).',
        '    inline uint32 ItemLevelOf(uint8 level)',
        '    {',
        f'        return (level < LEVELS ? level : LEVELS) + 5;',
        '    }',
        '}',
        '',
        '#endif',
        '',
    ]
    HEADER.write_text('\n'.join(lines))


def write_client(items, enchants, suffixes):
    CLIENT.write_text(json.dumps({
        'items': [[i['entry'], i['class'], i['subclass'], -1, i['material'], i['display'], i['inventory'],
                   i['sheath']] for i in items],
        'enchants': [[e['id'], e['amount'], e['name']] for e in enchants],
        'suffixes': [[s['id'], s['name'], s['internal'], s['enchants'], s['alloc']] for s in suffixes],
    }, separators=(',', ':')))


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    items, sets, enchants, suffixes = build(sys.argv[1])
    write_sql(items, enchants, suffixes)
    write_header(items, sets)
    write_client(items, enchants, suffixes)
    print(f'{len(items)} items, {len(sets)} suffix sets, {len(suffixes)} suffixes, {len(enchants)} damage '
          f'enchantments: {SQL.name}, {HEADER.name}, {CLIENT.name}')


if __name__ == '__main__':
    main()
