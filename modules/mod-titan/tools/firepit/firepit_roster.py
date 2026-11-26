#!/usr/bin/env python3
"""The Firepit's roster (src/firepit.cpp): its waves march through the game's dungeons, in the order a Hero levels
through them, each dungeon a chapter of CHAPTER_WAVES waves (its boss wave the last).

Each dungeon brings up to KINDS_A_DUNGEON of its own enemies: the kinds spawned most often in it (creature.sql),
ordinary ones only, that need nothing of their dungeon: no C++ script (SmartAI is fine), not a boss, not a trigger,
civilian, vendor, vehicle, critter or totem, nothing unattackable or unselectable. The kind joins one wave apart from
the chapter's first. Healers (by their name: a Medic, a Priest ...) are marked: the Firepit sends few of them.

Writes src/firepit_roster.h. Usage: python3 firepit_roster.py
"""

import re
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
BASE = ROOT / 'data' / 'sql' / 'base' / 'db_world'
HEADER = HERE.parents[1] / 'src' / 'firepit_roster.h'

CHAPTER_WAVES = 5
KINDS_A_DUNGEON = 4
MIN_SPAWNS = 3

# The dungeons, in leveling order (map, name).
DUNGEONS = [
    (389, 'Ragefire Chasm'), (36, 'the Deadmines'), (43, 'Wailing Caverns'), (33, 'Shadowfang Keep'),
    (48, 'Blackfathom Deeps'), (34, 'the Stockade'), (90, 'Gnomeregan'), (47, 'Razorfen Kraul'),
    (189, 'the Scarlet Monastery'), (129, 'Razorfen Downs'), (70, 'Uldaman'), (209, "Zul'Farrak"),
    (349, 'Maraudon'), (109, 'the Sunken Temple'), (230, 'Blackrock Depths'), (229, 'Blackrock Spire'),
    (429, 'Dire Maul'), (289, 'Scholomance'), (329, 'Stratholme'),
    (543, 'Hellfire Ramparts'), (542, 'the Blood Furnace'), (547, 'the Slave Pens'), (546, 'the Underbog'),
    (557, 'the Mana-Tombs'), (558, 'the Auchenai Crypts'), (560, 'Old Hillsbrad'), (556, 'the Sethekk Halls'),
    (545, 'the Steamvault'), (555, 'the Shadow Labyrinth'), (540, 'the Shattered Halls'), (269, 'the Black Morass'),
    (553, 'the Botanica'), (554, 'the Mechanar'), (552, 'the Arcatraz'), (585, "Magisters' Terrace"),
    (574, 'Utgarde Keep'), (576, 'the Nexus'), (601, 'Azjol-Nerub'), (619, "Ahn'kahet"), (600, "Drak'Tharon Keep"),
    (608, 'the Violet Hold'), (604, 'Gundrak'), (599, 'the Halls of Stone'), (602, 'the Halls of Lightning'),
    (595, 'the Culling of Stratholme'), (575, 'Utgarde Pinnacle'), (578, 'the Oculus'), (632, 'the Forge of Souls'),
    (658, 'the Pit of Saron'), (668, 'the Halls of Reflection'),
]

NOT_SELECTABLE, NON_ATTACKABLE, IMMUNE_TO_PC, NOT_ATTACKABLE_1, PACIFIED = 0x2000000, 0x2, 0x100, 0x80, 0x20000
BAD_UNIT_FLAGS = NOT_SELECTABLE | NON_ATTACKABLE | IMMUNE_TO_PC | NOT_ATTACKABLE_1 | PACIFIED
TRIGGER, CIVILIAN = 0x80, 0x2
BAD_TYPES = {8, 10, 11, 12, 13}         # critter, not specified, totem, non-combat pet, gas cloud
BAD_NAMES = re.compile(r'trigger|bunny|invisible|dummy|stalker|\[|credit|totem|target|controller|portal|'
                       r'image of|spirit of|vehicle|cannon|prisoner|captive|slave\b|kidnapped|spectator', re.I)
HEALER_NAMES = re.compile(r'medic|healer|priest|chaplain|cleric|mender|oracle|mystic|shaman|witch doctor|abbot',
                          re.I)


def columns(text):
    return re.findall(r'^\s+`(\w+)`', text[text.index('CREATE TABLE'):text.index(') ENGINE')], re.M)


def rows(text):
    for line in text.splitlines():
        if line.startswith('('):
            yield re.findall(r"'(?:[^'\\]|\\.)*'|[^,()]+", line.rstrip(',;').strip('()'))


def main():
    template_text = (BASE / 'creature_template.sql').read_text(encoding='utf8')
    names = columns(template_text)
    col = {n: i for i, n in enumerate(names)}
    templates = {}
    for values in rows(template_text):
        if len(values) != len(names):
            continue
        templates[int(values[col['entry']])] = values

    spawn_text = (BASE / 'creature.sql').read_text(encoding='utf8')
    spawn_cols = {n: i for i, n in enumerate(columns(spawn_text))}
    counts = {}
    for values in rows(spawn_text):
        try:
            counts.setdefault(int(values[spawn_cols['map']]), Counter())[int(values[spawn_cols['id1']])] += 1
        except (ValueError, IndexError):
            continue

    def usable(entry):
        t = templates.get(entry)
        if not t:
            return False
        name = t[col['name']].strip("'")
        return (t[col['ScriptName']] == "''" and int(t[col['rank']]) in (0, 1) and int(t[col['npcflag']]) == 0
                and int(t[col['VehicleId']]) == 0 and not int(t[col['unit_flags']]) & BAD_UNIT_FLAGS
                and not int(t[col['flags_extra']]) & (TRIGGER | CIVILIAN) and int(t[col['type']]) not in BAD_TYPES
                and not BAD_NAMES.search(name))

    seen = set()
    chapters = []
    for map_id, dungeon in DUNGEONS:
        kinds = []
        for entry, spawns in counts.get(map_id, Counter()).most_common():
            if len(kinds) == KINDS_A_DUNGEON or spawns < MIN_SPAWNS:
                break
            if entry in seen or not usable(entry):
                continue
            seen.add(entry)
            name = templates[entry][col['name']].strip("'").replace("\\'", "'")
            kinds.append((entry, name, bool(HEALER_NAMES.search(name))))
        if kinds:
            chapters.append((dungeon, kinds))

    lines = [
        '// Generated by tools/firepit/firepit_roster.py: the Firepit\'s roster, a chapter per dungeon. Do not edit.',
        '#ifndef MOD_TITAN_FIREPIT_ROSTER_H',
        '#define MOD_TITAN_FIREPIT_ROSTER_H',
        '',
        '#include "Define.h"',
        '',
        'namespace FirepitRoster',
        '{',
        f'    constexpr uint32 CHAPTER_WAVES = {CHAPTER_WAVES};',
        '',
        '    // A kind: the wave it joins at, its entry, whether it heals (the Firepit sends few healers).',
        '    struct Kind',
        '    {',
        '        uint32 Wave;',
        '        uint32 Entry;',
        '        bool Healer;',
        '    };',
        '',
        '    // A chapter: the dungeon its kinds are from, named as the news says it ("the foes of ... pour in").',
        f'    constexpr char const* CHAPTERS[] = {{',
    ]
    for dungeon, _ in chapters:
        lines.append(f'        "{dungeon}",')
    lines += ['    };', '', '    constexpr Kind KINDS[] = {']
    for index, (dungeon, kinds) in enumerate(chapters):
        lines.append(f'        // {dungeon}')
        for k, (entry, name, healer) in enumerate(kinds):
            wave = index * CHAPTER_WAVES + 1 + k
            lines.append(f"        {{ {wave}, {entry}, {'true' if healer else 'false'} }},   // {name}")
    lines += ['    };', '}', '', '#endif', '']
    HEADER.write_text('\n'.join(lines))
    total = sum(len(k) for _, k in chapters)
    print(f'{len(chapters)} dungeons, {total} kinds, {sum(h for _, k in chapters for *_, h in k)} healers: '
          f'{HEADER.name} (waves 1-{len(chapters) * CHAPTER_WAVES})')


if __name__ == '__main__':
    main()
