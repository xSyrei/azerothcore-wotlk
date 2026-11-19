#!/usr/bin/env python3
"""Dungeon Scaled gear: the list of the 5-player dungeons' own items, which drop at the party's level.

Dungeons are fought at the party's level (src/dungeon_scaling.cpp); their own items drop at it too. The server makes
each listed item's copies for levels 1-60 at start-up (src/dungeon_loot.cpp: the item's stats, armor, damage and
block scaled from its own item level to the new one; entry FIRST_ITEM + index x LEVELS + level - 1). An item is a
dungeon's own when it is uncommon to epic gear that drops from a creature spawned in a 5-player dungeon (its heroic
version too) or from a chest there, and from no creature in the open world (raids aside): the world's random greens
that also drop in dungeons are not (they become Leveling Scaled gear, as before).

The list only grows: an item's index is its copies' entries, and players keep those in their bags. A run keeps the
list in src/dungeon_gear.h and appends what is new.

Writes:
- src/dungeon_gear.h: the items' entries, in order;
- client-patch/dungeon_gear.json: each item's Item.dbc row (class, subclass, sound, material, display, slot, sheath),
  which client-patch/patch_items.py copies for every level's entry (without it the client shows a red "?").

Usage: python3 dungeon_gear.py
"""

import json
import re
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
MODULE = HERE.parent.parent
BASE = MODULE.parent.parent / 'data' / 'sql' / 'base' / 'db_world'
HEADER = MODULE / 'src' / 'dungeon_gear.h'
CLIENT = MODULE / 'client-patch' / 'dungeon_gear.json'

FIRST_ITEM = 920000
LEVELS = 60

# Raids and battlegrounds among instance_template's maps; the rest are 5-player dungeons.
RAIDS = {249, 309, 409, 469, 509, 531, 532, 533, 534, 544, 548, 550, 564, 565, 568, 580, 603, 615, 616, 624, 631, 649,
         724}
BATTLEGROUNDS = {30, 489, 529, 559, 562, 566, 572, 607, 617, 618, 628}

TOKEN = re.compile(r"'(?:[^'\\]|\\.)*'|-?[\d.]+(?:e-?\d+)?|NULL")
ROW = re.compile(r"\(((?:'(?:[^'\\]|\\.)*'|[^()'])*)\)")


def table(name, columns):
    text = (BASE / f'{name}.sql').read_text(encoding='utf8')
    head = text[text.index('CREATE TABLE'):text.index(') ENGINE')]
    names = re.findall(r'^\s+`(\w+)`', head, re.M)
    index = [names.index(c) for c in columns]
    rows = []
    for match in ROW.finditer(text[text.index('INSERT INTO'):]):
        values = TOKEN.findall(match.group(1))
        if len(values) == len(names):
            rows.append([values[i] for i in index])
    return rows


def loot(name):
    entries = defaultdict(list)
    for entry, item, reference in table(name, ['Entry', 'Item', 'Reference']):
        entries[int(entry)].append((int(item), int(reference)))
    return entries


def dungeon_items():
    dungeons = {int(m) for (m,) in table('instance_template', ['map'])} - RAIDS - BATTLEGROUNDS
    references = loot('reference_loot_template')

    def expand(rows, seen):
        items = set()
        for item, reference in rows:
            if not reference:
                items.add(item)
            elif reference not in seen:
                seen.add(reference)
                items |= expand(references[reference], seen)
        return items

    creatures = {int(e): (int(l), int(h)) for e, l, h in
                 table('creature_template', ['entry', 'lootid', 'difficulty_entry_1'])}
    spawned = defaultdict(set)
    for entry, game_map in table('creature', ['id1', 'map']):
        spawned[int(entry)].add(int(game_map))
    creature_loot = loot('creature_loot_template')
    dungeon, world = set(), set()
    for entry, (loot_id, heroic) in creatures.items():
        maps = spawned.get(entry)
        if not maps or not loot_id:
            continue
        items = expand(creature_loot[loot_id], set())
        if maps & dungeons:
            dungeon |= items
            if heroic in creatures and creatures[heroic][0]:
                dungeon |= expand(creature_loot[creatures[heroic][0]], set())
        if maps - dungeons - RAIDS:
            world |= items
    chests = {int(e): (int(t), int(l)) for e, t, l in table('gameobject_template', ['entry', 'type', 'Data1'])}
    placed = defaultdict(set)
    for entry, game_map in table('gameobject', ['id', 'map']):
        placed[int(entry)].add(int(game_map))
    chest_loot = loot('gameobject_loot_template')
    for entry, (kind, loot_id) in chests.items():
        if kind == 3 and loot_id and placed.get(entry, set()) & dungeons:
            dungeon |= expand(chest_loot[loot_id], set())
    return dungeon - world


def main():
    columns = ['entry', 'class', 'subclass', 'SoundOverrideSubclass', 'Material', 'displayid', 'InventoryType',
               'sheath', 'Quality', 'name']
    templates = {int(r[0]): r for r in table('item_template', columns)}

    def gear(entry):
        row = templates.get(entry)
        return row and int(row[1]) in (2, 4) and int(row[6]) > 0 and 2 <= int(row[8]) <= 4

    known = []
    if HEADER.exists():
        known = [int(x) for x in re.findall(r'^\s+(\d+),', HEADER.read_text(), re.M)]
    new = sorted(e for e in dungeon_items() if gear(e) and e not in set(known))
    entries = known + new

    lines = [
        '// Generated by tools/scaling/dungeon_gear.py: the 5-player dungeons\' own items (Dungeon Scaled gear). Do not',
        '// edit. The list only grows: an item\'s copies are FIRST + index x LEVELS + level - 1, kept in players\' bags.',
        '#ifndef MOD_TITAN_DUNGEON_GEAR_H',
        '#define MOD_TITAN_DUNGEON_GEAR_H',
        '',
        '#include "Define.h"',
        '',
        'namespace DungeonGear',
        '{',
        f'    constexpr uint32 FIRST = {FIRST_ITEM};',
        f'    constexpr uint32 LEVELS = {LEVELS};',
        '',
        '    constexpr uint32 ITEMS[] = {',
    ]
    for entry in entries:
        name = templates[entry][9][1:-1].replace("\\'", "'") if entry in templates else '?'
        lines.append(f'        {entry},   // {name}')
    lines += ['    };', '}', '', '#endif', '']
    HEADER.write_text('\n'.join(lines))

    rows = []
    for index, entry in enumerate(entries):
        r = templates[entry]
        rows.append([entry, index] + [int(r[i]) for i in range(1, 8)])
    CLIENT.write_text(json.dumps({'first': FIRST_ITEM, 'levels': LEVELS, 'items': rows}, separators=(',', ':')) + '\n')
    print(f'{len(entries)} dungeon items ({len(new)} new): {HEADER.name}, {CLIENT.name}')


if __name__ == '__main__':
    main()
