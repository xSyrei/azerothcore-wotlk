#!/usr/bin/env python3
"""Leveling Scaled gear: the Smoldering Adventurer's Cache's own small pool of items, each copy made for the level it
was opened at.

The 3.3.5 client caches an item's level, armor and damage per template, so every piece has a template per level
(1-60): item level L + 5, required level L, a green's armor and weapon damage at that item level (Blizzard's greens'
medians per item level, the slot's share of the chest's armor). Stats come from a random suffix, "(Leveling Scaled)":
one per stat mix (Blizzard's Wrath "of the Bear" kind), whose amounts the core scales by the template's item level,
quality and slot (GenerateEnchSuffixFactor, RandPropPoints.dbc), as for any green. Below item level 10 a green has no
stat budget: the first few levels' pieces have armor and damage only.

Writes:
- data/sql/db-world/titan_scaled_gear.sql: the templates, itemrandomsuffix_dbc (the server reads its DBCs and these
  rows over them) and item_enchantment_template (a GM's .additem rolls one of the suffixes);
- src/scaled_gear.h: the pool and its suffixes for src/smoldering_cache.cpp;
- client-patch/scaled_gear.json: the same rows for the client patch (client-patch/patch_scaled_gear.py, patch_items.py).

Usage: python3 scaled_gear.py
"""

import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
MODULE = HERE.parent.parent
SQL = MODULE / 'data' / 'sql' / 'db-world' / 'titan_scaled_gear.sql'
HEADER = MODULE / 'src' / 'scaled_gear.h'
CLIENT = MODULE / 'client-patch' / 'scaled_gear.json'

FIRST_ITEM = 912000             # + piece * LEVELS + level - 1
OLD_ITEMS = (911000, 911999)    # the first version's templates (one per piece, the level in the suffix): removed
SUFFIX_BASE = 2000              # Blizzard's suffixes end below 300
OLD_SUFFIXES = 19999            # the first version's suffixes went up to SUFFIX_BASE + this
OLD_ENCHANTS = (30000, 39999)   # and its weapon damage enchantments
SUFFIX_GROUP = 9110             # item_enchantment_template group the templates name (a GM's .additem rolls one)
LEVELS = 60
QUALITY_UNCOMMON = 2


def item_level(level):
    return min(level, LEVELS) + 5


# Blizzard's generic "+$i" enchantments, their amount from the suffix (SpellItemEnchantment.dbc).
AGI, STA, INT, STR, SPI, SP, AP, DEF, MP5 = 2802, 2803, 2804, 2805, 2806, 2824, 2825, 2813, 2816

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
# Ranged weapons, as Blizzard's: their range (RangedModRange 100: the client computes a shot's range from it, 0 is out
# of range everywhere) and ammo (bows arrows 2, guns bullets 3; wands none).
RANGED_RANGE = 100
AMMO_TYPE = {2: 2, 3: 3, 18: 2}
DAMAGE_SPREAD = 0.3             # minimum and maximum damage: the average less and plus this share

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


def build():
    """The pieces (each with its mixes) and the templates (a piece at each level)."""
    pieces = []
    for armor, (prefix, parts) in ARMOR_SETS.items():
        for inventory_type, name, display, material, durability in parts:
            pieces.append({'name': f'{prefix} {name}', 'class': 4, 'subclass': ARMOR_SUBCLASS[armor],
                           'inventory': inventory_type, 'display': display, 'material': material, 'sheath': 0,
                           'durability': durability, 'armor': armor, 'share': SLOT_ARMOR[inventory_type],
                           'weapon': None, 'armor_type': ARMOR_SUBCLASS[armor], 'mixes': ARMOR_MIXES[armor]})
    for name, cls, sub, inventory_type, display, material, sheath, durability, kind, mixes in OTHERS:
        armor = kind if kind in ('cloak', 'shield') else None
        pieces.append({'name': name, 'class': cls, 'subclass': sub, 'inventory': inventory_type, 'display': display,
                       'material': material, 'sheath': sheath, 'durability': durability, 'armor': armor,
                       'share': SLOT_ARMOR.get(inventory_type, 1.0), 'weapon': kind if kind in WEAPON_KINDS else None,
                       'armor_type': 0, 'mixes': mixes})

    templates = []
    for index, piece in enumerate(pieces):
        piece['first'] = FIRST_ITEM + index * LEVELS
        for level in range(1, LEVELS + 1):
            il = item_level(level)
            armor = round(ARMOR_PER_LEVEL[piece['armor']] * il * piece['share']) if piece['armor'] else 0
            low = high = 0
            if piece['weapon']:
                average = DPS_PER_LEVEL[piece['weapon']] * il * SPEED[piece['weapon']] / 1000
                low, high = max(1, round(average * (1 - DAMAGE_SPREAD))), max(2, round(average * (1 + DAMAGE_SPREAD)))
            templates.append({**piece, 'entry': piece['first'] + level - 1, 'level': level, 'item_level': il,
                              'armor_value': armor, 'damage': (low, high),
                              'delay': SPEED[piece['weapon']] if piece['weapon'] else 0})

    suffixes = [{'id': SUFFIX_BASE + i, 'mix': mix, 'name': '(Leveling Scaled)', 'internal': f'scaled_{mix}',
                 'enchants': [e for e, _ in MIXES[mix]], 'alloc': [a for _, a in MIXES[mix]]}
                for i, mix in enumerate(MIXES)]
    return pieces, templates, suffixes


def sql_text(text):
    return "'" + text.replace("'", "''") + "'"


def write_sql(templates, suffixes):
    last = templates[-1]['entry']
    lines = [
        '-- Generated by tools/scaling/scaled_gear.py: Leveling Scaled gear, the pool of the Smoldering',
        '-- Adventurer\'s Cache (src/smoldering_cache.cpp). Do not edit by hand; see the script for how it scales.',
        '-- The first version (one template per piece, the level in the suffix) is removed: its copies are gone.',
        f'DELETE FROM `item_template` WHERE `entry` BETWEEN {OLD_ITEMS[0]} AND {OLD_ITEMS[1]};',
        f'DELETE FROM `spellitemenchantment_dbc` WHERE `ID` BETWEEN {OLD_ENCHANTS[0]} AND {OLD_ENCHANTS[1]};',
        f'DELETE FROM `itemrandomsuffix_dbc` WHERE `ID` BETWEEN {SUFFIX_BASE} AND {SUFFIX_BASE + OLD_SUFFIXES};',
        '',
        '-- A template per piece and level: item level = level + 5, a green\'s armor and damage there.',
        f'DELETE FROM `item_template` WHERE `entry` BETWEEN {FIRST_ITEM} AND {last};',
        '-- (SoundOverrideSubclass, AllowableClass and AllowableRace: their default, -1.)',
        'INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`,',
        '    `Quality`, `SellPrice`, `InventoryType`, `ItemLevel`, `RequiredLevel`,',
        '    `stackable`, `armor`, `dmg_min1`, `dmg_max1`, `delay`, `ammo_type`, `RangedModRange`, `bonding`,',
        '    `Material`, `sheath`, `MaxDurability`, `RandomSuffix`, `VerifiedBuild`) VALUES',
    ]
    rows = []
    for t in templates:
        ranged = t['weapon'] in ('ranged', 'wand')
        ammo = AMMO_TYPE.get(t['subclass'], 0) if t['class'] == 2 else 0
        rows.append(f"({t['entry']}, {t['class']}, {t['subclass']}, {sql_text(t['name'])}, {t['display']}, "
                    f"{QUALITY_UNCOMMON}, {t['item_level']}, {t['inventory']}, {t['item_level']}, "
                    f"{t['level']}, 1, {t['armor_value']}, {t['damage'][0]}, {t['damage'][1]}, {t['delay']}, "
                    f"{ammo}, {RANGED_RANGE if ranged else 0}, 1, {t['material']}, {t['sheath']}, "
                    f"{t['durability']}, {SUFFIX_GROUP}, 0)")
    lines.append(',\n'.join(rows) + ';')
    lines += [
        '',
        '-- The suffixes: one per stat mix, "(Leveling Scaled)".',
        'INSERT INTO `itemrandomsuffix_dbc` (`ID`, `Name_Lang_enUS`, `Name_Lang_Mask`, `InternalName`,',
        '    `Enchantment_1`, `Enchantment_2`, `Enchantment_3`, `Enchantment_4`, `Enchantment_5`, `AllocationPct_1`,',
        '    `AllocationPct_2`, `AllocationPct_3`, `AllocationPct_4`, `AllocationPct_5`) VALUES',
    ]
    rows = []
    for x in suffixes:
        e = x['enchants'] + [0] * (5 - len(x['enchants']))
        a = x['alloc'] + [0] * (5 - len(x['alloc']))
        rows.append(f"({x['id']}, {sql_text(x['name'])}, 16712190, {sql_text(x['internal'])}, "
                    + ', '.join(map(str, e)) + ', ' + ', '.join(map(str, a)) + ')')
    lines.append(',\n'.join(rows) + ';')
    lines += [
        '',
        f'-- The group the templates name (RandomSuffix {SUFFIX_GROUP}): for items made without the cache (a GM\'s',
        '-- .additem), any suffix; the cache picks its own.',
        f'DELETE FROM `item_enchantment_template` WHERE `entry` = {SUFFIX_GROUP};',
        'INSERT INTO `item_enchantment_template` (`entry`, `ench`, `chance`) VALUES',
        ',\n'.join(f"({SUFFIX_GROUP}, {x['id']}, {100 / len(suffixes):.2f})" for x in suffixes) + ';',
        '',
    ]
    SQL.write_text('\n'.join(lines))


def write_header(pieces, suffixes):
    mix_id = {x['mix']: x['id'] for x in suffixes}
    lines = [
        '// Generated by tools/scaling/scaled_gear.py: the Leveling Scaled pool and its suffixes. Do not edit.',
        '#ifndef MOD_TITAN_SCALED_GEAR_H',
        '#define MOD_TITAN_SCALED_GEAR_H',
        '',
        '#include "Define.h"',
        '#include <array>',
        '',
        'namespace ScaledGear',
        '{',
        f'    constexpr uint32 LEVELS = {LEVELS};',
        '',
        '    // A piece: its level 1 template (level L: First + L - 1) and the suffixes it can take.',
        '    struct Item',
        '    {',
        '        uint32 First;',
        '        uint8 ArmorType;            // ITEM_SUBCLASS_ARMOR_CLOTH..PLATE for the armor sets, 0 otherwise',
        '        uint8 SuffixCount;',
        '        std::array<uint16, 9> Suffixes;',
        '    };',
        '',
        '    constexpr Item ITEMS[] = {',
    ]
    for piece in pieces:
        ids = [mix_id[m] for m in piece['mixes']]
        padded = ', '.join(map(str, ids + [0] * (9 - len(ids))))
        lines.append(f"        {{ {piece['first']}, {piece['armor_type']}, {len(ids)}, {{ {padded} }} }},"
                     f"   // {piece['name']}")
    lines += [
        '    };',
        '',
        '    inline uint32 Entry(Item const& item, uint8 level)',
        '    {',
        '        return item.First + (level < 1 ? 1 : level > LEVELS ? LEVELS : level) - 1;',
        '    }',
        '}',
        '',
        '#endif',
        '',
    ]
    HEADER.write_text('\n'.join(lines))


def write_client(templates, suffixes):
    CLIENT.write_text(json.dumps({
        'items': [[t['entry'], t['class'], t['subclass'], -1, t['material'], t['display'], t['inventory'],
                   t['sheath']] for t in templates],
        'suffixes': [[x['id'], x['name'], x['internal'], x['enchants'], x['alloc']] for x in suffixes],
    }, separators=(',', ':')))


def main():
    if len(sys.argv) != 1:
        sys.exit(__doc__)
    pieces, templates, suffixes = build()
    write_sql(templates, suffixes)
    write_header(pieces, suffixes)
    write_client(templates, suffixes)
    print(f'{len(pieces)} pieces, {len(templates)} templates, {len(suffixes)} suffixes: '
          f'{SQL.name}, {HEADER.name}, {CLIENT.name}')


if __name__ == '__main__':
    main()
