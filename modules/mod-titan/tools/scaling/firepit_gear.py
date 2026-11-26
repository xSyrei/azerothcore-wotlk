#!/usr/bin/env python3
"""Fireforged gear: the Firepit's spoils (src/firepit.cpp), always better than what the Hero wears.

Each spoil is made for the Hero who earned it: the slot they are weakest in, a piece some item levels above what is
there, with stats for their role. The 3.3.5 client caches an item's level, armor and damage per template, so the
pieces (the Leveling Scaled pool's slots and looks: scaled_gear.py) have a template per item level tier
(FIRST_ITEM_LEVEL, then every ITEM_LEVEL_STEP, TIERS of them), epic, soulbound on pickup, required level 10 (the
Firepit's). Stats come from a random suffix, as Leveling Scaled gear's: the same stat mixes (its suffixes 2000-2008),
their amounts scaled by the core from the template's item level and quality. A Titanforged Fireforged piece (a rare
roll) is the same piece with every stat doubled: a template of its own (its tag) and suffixes of its own (TITAN_SUFFIX,
each mix's allocations x2).

Modifiers: from wave 30 a spoil may carry a chance-on-hit or equip effect, from wave 90 a second (src/firepit.cpp
says how likely): the procs of the game's famous weapons and trinkets (Hand of Justice, Dragonspine Trophy,
Thunderfury, Darkmoon Cards ...) and its weapon enchantments, in the item's free random-property enchantment slots
(PROP_ENCHANTMENT_SLOT_3 and 4: the suffix uses the first three). Each is for some roles (a caster never gets a melee
proc), from some level (the Burning Crusade's from 40, Wrath's from 50: their ratings are for those levels), and a
weapon's or any piece's. Ours are SpellItemEnchantment rows (MODIFIER_FIRST on) naming the item's proc spell; the
game's own enchantments are used as they are.

Writes:
- data/sql/db-world/titan_firepit_gear.sql: the templates, the Titanforged suffixes (itemrandomsuffix_dbc) and the
  modifiers (spellitemenchantment_dbc);
- src/firepit_gear.h: the pieces, tiers and suffixes for src/firepit.cpp;
- client-patch/firepit_gear.json: the same rows for the client patch (patch_items.py, patch_scaled_gear.py).

Usage: python3 firepit_gear.py
"""

import json
import sys
from pathlib import Path

import scaled_gear as scaled

HERE = Path(__file__).resolve().parent
MODULE = HERE.parent.parent
SQL = MODULE / 'data' / 'sql' / 'db-world' / 'titan_firepit_gear.sql'
HEADER = MODULE / 'src' / 'firepit_gear.h'
CLIENT = MODULE / 'client-patch' / 'firepit_gear.json'

FIRST_ITEM = 916000             # Fireforged: + piece * TIERS + tier; Titanforged: + TITAN_OFFSET
FIRST_ITEM_LEVEL, ITEM_LEVEL_STEP, TIERS = 12, 3, 37        # item levels 12-120
REQUIRED_LEVEL = 10             # the Firepit's
QUALITY_EPIC = 4
BIND_ON_PICKUP = 1
ARMOR_FACTOR, DPS_FACTOR = 1.2, 1.25    # an epic's armor and weapon damage over a green's at the same item level
TITAN_SUFFIX = 22000            # + mix index (2000-21999 are titan_scaled_gear.sql's to clear)
TITAN_STATS = 2                 # a Titanforged piece's stats: twice a Fireforged one's
SUFFIX_GROUP = scaled.SUFFIX_GROUP

# Modifiers. Roles: P physical (melee, tanks, hunters), C caster, H healer, T tank. Era: from level 10 (0), 40 (1),
# 50 (2). Kind: W a weapon's (chance on hit: a combat spell, its chance in percent), E any piece's (an equip spell).
MODIFIER_FIRST = 40000
COMBAT_SPELL, EQUIP_SPELL = 1, 3
MODIFIERS = [
    # The game's own weapon enchantments (id, name, roles, era, kind).
    (1900, None, 'PT', 0, 'W'), (803, None, 'PT', 0, 'W'), (1898, None, 'PT', 0, 'W'), (1894, None, 'PT', 0, 'W'),
    (1899, None, 'P', 0, 'W'), (2673, None, 'P', 1, 'W'), (3225, None, 'P', 1, 'W'), (3789, None, 'P', 2, 'W'),
    (3273, None, 'P', 2, 'W'), (3241, None, 'PT', 2, 'W'), (3368, None, 'PT', 2, 'W'), (3239, None, 'P', 2, 'W'),
    (2674, None, 'CH', 1, 'E'), (3790, None, 'C', 2, 'E'), (2791, None, 'T', 1, 'E'), (205, None, 'T', 0, 'E'),
    (1704, None, 'T', 0, 'E'), (2714, None, 'T', 1, 'E'),
    # Ours: (spell, name, roles, era, kind, chance on hit %).
    ('new', 15494, "Ironfoe's Fury", 'P', 0, 'W', 3), ('new', 21165, 'Empyrean Haste', 'P', 0, 'W', 5),
    ('new', 21992, 'Thunderfury', 'PT', 0, 'W', 6), ('new', 23605, 'Nightfall', 'PT', 0, 'W', 8),
    ('new', 15600, 'Hand of Justice', 'PT', 0, 'E', 0), ('new', 7598, "Blackhand's Breadth", 'P', 0, 'E', 0),
    ('new', 39438, 'Darkmoon Crusade', 'P', 1, 'E', 0), ('new', 34774, 'Dragonspine Trophy', 'P', 1, 'E', 0),
    ('new', 42083, 'Tsunami', 'P', 1, 'E', 0), ('new', 40475, 'Madness of the Betrayer', 'P', 1, 'E', 0),
    ('new', 39442, 'Darkmoon Wrath', 'PC', 1, 'E', 0), ('new', 57345, 'Darkmoon Greatness', 'P', 2, 'E', 0),
    ('new', 33648, 'Mirror of Truth', 'P', 2, 'E', 0), ('new', 60436, 'Grim Toll', 'P', 2, 'E', 0),
    ('new', 60313, 'Fury of the Five Flights', 'P', 2, 'E', 0), ('new', 57352, 'Darkmoon Death', 'PC', 2, 'E', 0),
    ('new', 60301, 'Meteorite Whetstone', 'P', 2, 'E', 0), ('new', 65013, 'Pyrite Infusion', 'P', 2, 'E', 0),
    ('new', 60442, "Bandit's Insignia", 'P', 2, 'E', 0), ('new', 67702, "Death's Choice", 'P', 2, 'E', 0),
    ('new', 71519, "Deathbringer's Will", 'P', 2, 'E', 0),
    ('new', 39440, 'Darkmoon Crusade', 'CH', 1, 'E', 0), ('new', 33297, "Quagmirran's Eye", 'C', 1, 'E', 0),
    ('new', 38347, 'Sextant of Unstable Currents', 'C', 1, 'E', 0),
    ('new', 34749, 'Eye of Magtheridon', 'C', 1, 'E', 0),
    ('new', 34320, "Shiffar's Nexus-Horn", 'C', 1, 'E', 0), ('new', 60490, 'Embrace of the Spider', 'CH', 2, 'E', 0),
    ('new', 60485, 'Illustration of the Dragon Soul', 'CH', 2, 'E', 0), ('new', 60473, 'Forge Ember', 'CH', 2, 'E', 0),
    ('new', 60063, 'Sundial of the Exiled', 'C', 2, 'E', 0), ('new', 60493, 'Dying Curse', 'C', 2, 'E', 0),
    ('new', 60487, 'Extract of Necromantic Power', 'C', 2, 'E', 0),
    ('new', 64714, 'Flare of the Heavens', 'C', 2, 'E', 0),
    ('new', 65007, 'Eye of the Broodmother', 'CH', 2, 'E', 0),
    ('new', 23688, 'Darkmoon Blue Dragon', 'HC', 0, 'E', 0), ('new', 60537, 'Soul of the Dead', 'HC', 2, 'E', 0),
    ('new', 60524, 'Majestic Dragon Figurine', 'H', 2, 'E', 0), ('new', 60519, 'Spark of Life', 'H', 2, 'E', 0),
    ('new', 49622, "Je'Tze's Bell", 'H', 2, 'E', 0), ('new', 33953, 'Egg of Mortal Essence', 'H', 2, 'E', 0),
    ('new', 60221, 'Essence of Gossamer', 'T', 2, 'E', 0), ('new', 45057, "Commendation of Kael'thas", 'T', 1, 'E', 0),
    ('new', 39444, 'Darkmoon Vengeance', 'T', 1, 'E', 0),
]
ROLE_BITS = {'P': 1, 'C': 2, 'H': 4, 'T': 8}


def modifiers():
    """The modifiers: (enchantment id, roles mask, era, weapon only, our row or None)."""
    out, next_id = [], MODIFIER_FIRST
    for m in MODIFIERS:
        if m[0] == 'new':
            _, spell, name, roles, era, kind, chance = m
            row = {'id': next_id, 'name': name, 'type': COMBAT_SPELL if kind == 'W' else EQUIP_SPELL,
                   'amount': chance, 'spell': spell}
            out.append((next_id, roles, era, kind == 'W', row))
            next_id += 1
        else:
            enchant, _, roles, era, kind = m
            out.append((enchant, roles, era, kind == 'W', None))
    return out


# The names: the Leveling Scaled pool's, Ember made Pyre (the tag under the name says Fireforged).
PREFIXES = {'Emberweave': 'Pyreweave', 'Emberhide': 'Pyrehide', 'Emberlink': 'Pyrelink', 'Emberforged': 'Pyreforged',
            'Ember': 'Pyre'}


def name_of(name):
    first, _, rest = name.partition(' ')
    return f'{PREFIXES[first]} {rest}'


def item_level(tier):
    return FIRST_ITEM_LEVEL + tier * ITEM_LEVEL_STEP


def build():
    pieces, _, base_suffixes = scaled.build()
    titan_offset = len(pieces) * TIERS
    templates = []
    for index, piece in enumerate(pieces):
        piece['fire_first'] = FIRST_ITEM + index * TIERS
        piece['titan_first'] = FIRST_ITEM + titan_offset + index * TIERS
        for titan in (False, True):
            first = piece['titan_first'] if titan else piece['fire_first']
            for tier in range(TIERS):
                il = item_level(tier)
                armor = 0
                if piece['armor']:
                    armor = round(scaled.ARMOR_PER_LEVEL[piece['armor']] * il * piece['share'] * ARMOR_FACTOR)
                low = high = 0
                if piece['weapon']:
                    average = (scaled.DPS_PER_LEVEL[piece['weapon']] * DPS_FACTOR * il
                               * scaled.SPEED[piece['weapon']] / 1000)
                    low = max(1, round(average * (1 - scaled.DAMAGE_SPREAD)))
                    high = max(2, round(average * (1 + scaled.DAMAGE_SPREAD)))
                templates.append({**piece, 'name': name_of(piece['name']), 'entry': first + tier, 'item_level': il,
                                  'armor_value': armor, 'damage': (low, high),
                                  'delay': scaled.SPEED[piece['weapon']] if piece['weapon'] else 0})
    titan = [{'id': TITAN_SUFFIX + i, 'mix': x['mix'], 'name': '', 'internal': f"titanforged_{x['mix']}",
              'enchants': x['enchants'], 'alloc': [a * TITAN_STATS for a in x['alloc']]}
             for i, x in enumerate(base_suffixes)]
    return pieces, templates, base_suffixes, titan, titan_offset


def sql_text(text):
    return "'" + text.replace("'", "''") + "'"


def write_sql(templates, titan):
    last = FIRST_ITEM + len(templates) - 1
    lines = [
        '-- Generated by tools/scaling/firepit_gear.py: Fireforged gear, the Firepit\'s spoils (src/firepit.cpp).',
        '-- Do not edit by hand; see the script for how it scales.',
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
        ammo = scaled.AMMO_TYPE.get(t['subclass'], 0) if t['class'] == 2 else 0
        rows.append(f"({t['entry']}, {t['class']}, {t['subclass']}, {sql_text(t['name'])}, {t['display']}, "
                    f"{QUALITY_EPIC}, {t['item_level'] * 4}, {t['inventory']}, {t['item_level']}, "
                    f"{REQUIRED_LEVEL}, 1, {t['armor_value']}, {t['damage'][0]}, {t['damage'][1]}, {t['delay']}, "
                    f"{ammo}, {scaled.RANGED_RANGE if ranged else 0}, {BIND_ON_PICKUP}, {t['material']}, "
                    f"{t['sheath']}, {t['durability']}, {SUFFIX_GROUP}, 0)")
    lines.append(',\n'.join(rows) + ';')
    lines += [
        '',
        '-- The Titanforged suffixes: each stat mix\'s, every stat doubled (nameless: the tag is under the item name).',
        f'DELETE FROM `itemrandomsuffix_dbc` WHERE `ID` BETWEEN {TITAN_SUFFIX} AND {TITAN_SUFFIX + 99};',
        'INSERT INTO `itemrandomsuffix_dbc` (`ID`, `Name_Lang_enUS`, `Name_Lang_Mask`, `InternalName`,',
        '    `Enchantment_1`, `Enchantment_2`, `Enchantment_3`, `Enchantment_4`, `Enchantment_5`, `AllocationPct_1`,',
        '    `AllocationPct_2`, `AllocationPct_3`, `AllocationPct_4`, `AllocationPct_5`) VALUES',
    ]
    rows = []
    for x in titan:
        e = x['enchants'] + [0] * (5 - len(x['enchants']))
        a = x['alloc'] + [0] * (5 - len(x['alloc']))
        rows.append(f"({x['id']}, {sql_text(x['name'])}, 16712190, {sql_text(x['internal'])}, "
                    + ', '.join(map(str, e)) + ', ' + ', '.join(map(str, a)) + ')')
    lines.append(',\n'.join(rows) + ';')
    ours = [m[4] for m in modifiers() if m[4]]
    lines += [
        '',
        '-- The modifiers: famous procs as enchantments (their name is the item\'s; the client shows it on the piece).',
        f'DELETE FROM `spellitemenchantment_dbc` WHERE `ID` BETWEEN {MODIFIER_FIRST} AND {MODIFIER_FIRST + 999};',
        'INSERT INTO `spellitemenchantment_dbc` (`ID`, `Charges`, `Effect_1`, `EffectPointsMin_1`,',
        '    `EffectPointsMax_1`, `EffectArg_1`, `Name_Lang_enUS`, `Name_Lang_Mask`) VALUES',
        ',\n'.join(f"({x['id']}, 0, {x['type']}, {x['amount']}, {x['amount']}, {x['spell']}, {sql_text(x['name'])}, "
                   "16712190)" for x in ours) + ';',
        '',
    ]
    SQL.write_text('\n'.join(lines))


def write_header(pieces, base, titan, titan_offset):
    mixes = [x['mix'] for x in base]
    lines = [
        '// Generated by tools/scaling/firepit_gear.py: Fireforged gear, the Firepit\'s spoils. Do not edit.',
        '#ifndef MOD_TITAN_FIREPIT_GEAR_H',
        '#define MOD_TITAN_FIREPIT_GEAR_H',
        '',
        '#include "Define.h"',
        '',
        'namespace FirepitGear',
        '{',
        f'    constexpr uint32 FIRST_ITEM = {FIRST_ITEM};',
        f'    constexpr uint32 TITAN_OFFSET = {titan_offset};     // a Titanforged piece: its Fireforged entry + this',
        f'    constexpr uint32 TIERS = {TIERS};',
        f'    constexpr uint32 FIRST_ITEM_LEVEL = {FIRST_ITEM_LEVEL}, ITEM_LEVEL_STEP = {ITEM_LEVEL_STEP};',
        '',
        '    // The stat mixes (ItemRandomSuffix: Fireforged, and Titanforged, every stat doubled).',
        '    enum Mix : uint8',
        '    {',
    ]
    lines += [f'        MIX_{m.upper()},' for m in mixes]
    lines += [
        '        MAX_MIXES',
        '    };',
        f'    constexpr uint16 SUFFIX[MAX_MIXES] = {{ {", ".join(str(x["id"]) for x in base)} }};',
        f'    constexpr uint16 TITAN_SUFFIX[MAX_MIXES] = {{ {", ".join(str(x["id"]) for x in titan)} }};',
        '',
        '    // A piece: its first tier\'s Fireforged template (tier t: First + t), its item class and subclass,',
        '    // inventory type and armor type (ITEM_SUBCLASS_ARMOR_CLOTH..PLATE for the armor sets, 0 otherwise).',
        '    struct Piece',
        '    {',
        '        uint32 First;',
        '        uint8 Class;',
        '        uint8 SubClass;',
        '        uint8 InventoryType;',
        '        uint8 ArmorType;',
        '    };',
        '',
        '    constexpr Piece PIECES[] = {',
    ]
    for piece in pieces:
        lines.append(f"        {{ {piece['fire_first']}, {piece['class']}, {piece['subclass']}, {piece['inventory']}, "
                     f"{piece['armor_type']} }},   // {name_of(piece['name'])}")
    lines += [
        '    };',
        '',
        '    // The modifiers (tools/scaling/firepit_gear.py MODIFIERS): an enchantment, the roles it is for',
        '    // (ROLE_* bits), the era it comes from (0: level 10, 1: level 40, 2: level 50), a weapon\'s only.',
        '    enum ModifierRole : uint8 { ROLE_PHYSICAL = 1, ROLE_CASTER = 2, ROLE_HEALER = 4, ROLE_TANK = 8 };',
        '    struct Modifier',
        '    {',
        '        uint32 Enchantment;',
        '        uint8 Roles;',
        '        uint8 Era;',
        '        bool WeaponOnly;',
        '    };',
        '    constexpr uint8 ERA_LEVEL[] = { 10, 40, 50 };',
        '',
        '    constexpr Modifier MODIFIERS[] = {',
        *[f"        {{ {e}, {sum(ROLE_BITS[c] for c in roles)}, {era}, {'true' if weapon else 'false'} }},"
          + (f"   // {row['name']}" if row else '') for e, roles, era, weapon, row in modifiers()],
        '    };',
        '',
        '    // The tier of an item level: the lowest at or above it (the last if none).',
        '    inline uint32 TierAtLeast(uint32 itemLevel)',
        '    {',
        '        if (itemLevel <= FIRST_ITEM_LEVEL)',
        '            return 0;',
        '        uint32 tier = (itemLevel - FIRST_ITEM_LEVEL + ITEM_LEVEL_STEP - 1) / ITEM_LEVEL_STEP;',
        '        return tier < TIERS ? tier : TIERS - 1;',
        '    }',
        '}',
        '',
        '#endif',
        '',
    ]
    HEADER.write_text('\n'.join(lines))


def write_client(templates, titan):
    CLIENT.write_text(json.dumps({
        'items': [[t['entry'], t['class'], t['subclass'], -1, t['material'], t['display'], t['inventory'],
                   t['sheath']] for t in templates],
        'suffixes': [[x['id'], x['name'], x['internal'], x['enchants'], x['alloc']] for x in titan],
        'enchants': [[m[4]['id'], m[4]['type'], m[4]['amount'], m[4]['spell'], m[4]['name']]
                     for m in modifiers() if m[4]],
    }, separators=(',', ':')))


def main():
    if len(sys.argv) != 1:
        sys.exit(__doc__)
    pieces, templates, base, titan, titan_offset = build()
    write_sql(templates, titan)
    write_header(pieces, base, titan, titan_offset)
    write_client(templates, titan)
    print(f'{len(pieces)} pieces, {len(templates)} templates ({FIRST_ITEM}-{FIRST_ITEM + len(templates) - 1}), '
          f'{len(titan)} Titanforged suffixes: {SQL.name}, {HEADER.name}, {CLIENT.name}')


if __name__ == '__main__':
    main()
