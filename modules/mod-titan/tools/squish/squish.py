#!/usr/bin/env python3
"""The level and power squish (option B, .agents/plans/phase-0): writes data/sql/db-world/zz_titan_squish.sql.

The game ends on Classic's numbers: the new level 60 is today's level 60 in power. Classic stays as it is, 1-60;
the Burning Crusade and Wrath are squished into 40-60, beside Classic's own 40-60 and each other:

    Classic         1-60  -> 1-60    unchanged
    Burning Crusade 58-70 -> 40-60   new = 40 + (old - 58) x 20/12
    Wrath           68-80 -> 40-60   new = 40 + (old - 68) x 20/12

- Items (weapons and armor, uncommon to epic and their poor and common kin): Classic's as they are; the
  others' required level by that map, their item level onto Classic's curve at the new required level (what a
  Classic item of its quality has there, read from the Classic items themselves), its height above its own
  expansion's curve shrunk with the band; stats and block by the stat budget ratio (RandPropPoints.dbc), armor and
  weapon damage by the item level ratio. Random "of the ..." stats follow the item level by themselves.
  Heirlooms and legendaries are left alone. An item with no required level (uncommon to epic: many quest rewards,
  the quest has the level) gets one from where it comes from: the lowest level of the quests that reward it (a Burning
  Crusade or Wrath quest's level, a Classic quest's minimum level; their zone's expansion), else, past Classic's item levels, the required
  level where its expansion's curve reaches its item level; then it is squished as the others (Classic's keep
  their power and get that required level). Classic's with no quest stay without (mod-titan's
  Prestige::EquipLevel judges them by item level).
- Creatures: their level by the map (the level decides the band; creature_template.exp only where bands overlap,
  58-63 and 68-70: many Outland and Northrend creatures are marked 0; Classic's unchanged), and exp 0, so their
  health, damage and armor come from Classic's stats rows at the new level.
- Quests: their level, minimum and maximum level by the map (where bands overlap, by their zone's expansion:
  AreaTable.dbc, Outland and its dungeons, Northrend and its dungeons; the Blood Elf and Draenei zones are
  Classic's).

The SQL can run any number of times: the first run keeps every original value in titan_squish_*_original (rows
added to the game later are kept when first met), and every run computes from those. To apply it again after an
update put the original values back (a core update, another module's SQL), change this script's VERSION and
regenerate: the updater runs a changed file again.

Usage: python3 squish.py <RandPropPoints.dbc> <AreaTable.dbc> [item_template.sql]
"""

import re
import statistics
import struct
import sys
from collections import defaultdict
from pathlib import Path

VERSION = 3
ROOT = Path(__file__).resolve().parents[4]
ITEMS = ROOT / 'data/sql/base/db_world/item_template.sql'
OUT = ROOT / 'modules/mod-titan/data/sql/db-world/zz_titan_squish.sql'

CLASSIC, TBC, WRATH = 0, 1, 2
# Instance maps of the Burning Crusade and Wrath (dungeons and raids); their continents are 530 and 571.
TBC_MAPS = {269, 532, 534, 540, 542, 543, 544, 545, 546, 547, 548, 550, 552, 553, 554, 555, 556, 557, 558, 560, 564,
            565, 568, 580, 585}
WRATH_MAPS = {533, 571, 574, 575, 576, 578, 595, 599, 600, 601, 602, 603, 604, 608, 615, 616, 619, 624, 631, 632, 649,
              650, 658, 668, 724}
# Map 530 zones that are Classic's: the Blood Elf and Draenei starting lands.
CLASSIC_530 = {3430, 3433, 3487, 3524, 3525, 3557}


def dbc(path):
    data = Path(path).read_bytes()
    _, count, fields, size, _ = struct.unpack_from('<4s4I', data)
    return {row[0]: row for row in (struct.unpack_from(f'<{fields}I', data, 20 + i * size) for i in range(count))}


def zone_expansions(path):
    """Every area id -> its expansion, by its top zone's map."""
    areas = dbc(path)
    result = {}
    for area, row in areas.items():
        top = row
        for _ in range(8):
            if not top[2] or top[2] not in areas:
                break
            top = areas[top[2]]
        game_map = top[1]
        if game_map in WRATH_MAPS:
            result[area] = WRATH
        elif game_map in TBC_MAPS or (game_map == 530 and top[0] not in CLASSIC_530):
            result[area] = TBC
        else:
            result[area] = CLASSIC
    return result


def read_items(path):
    txt = Path(path).read_text(encoding='utf8')
    head = txt[txt.index('CREATE TABLE'):txt.index(') ENGINE')]
    cols = re.findall(r'^\s+`(\w+)`', head, re.M)
    ix = {c: i for i, c in enumerate(cols)}
    tok = re.compile(r"'(?:[^'\\]|\\.)*'|-?[\d.]+(?:e-?\d+)?|NULL")
    items = []
    for line in txt.splitlines():
        if not line.startswith('('):
            continue
        v = tok.findall(line)
        if len(v) < len(cols):
            continue
        num = lambda c: float(v[ix[c]]) if v[ix[c]] not in ('NULL',) and not v[ix[c]].startswith("'") else 0
        if int(num('class')) in (2, 4) and int(num('RequiredLevel')) > 0:
            items.append((int(num('Quality')), int(num('RequiredLevel')), int(num('ItemLevel'))))
    return items


def expansion(quality, req, ilvl):
    """An item's expansion by its required and item level (items have no expansion field)."""
    if req <= 57:
        return CLASSIC
    if req <= 60:
        return CLASSIC if ilvl <= req + 10 or (quality >= 4 and ilvl <= 92) else TBC
    if req <= 67:
        return TBC
    if req <= 70:
        return TBC if ilvl < 130 else WRATH
    return WRATH


def curves(items):
    """Median item level by (expansion, quality, required level), gaps filled from the nearest required level."""
    med = defaultdict(list)
    for quality, req, ilvl in items:
        if quality in (2, 3, 4):
            med[(expansion(quality, req, ilvl), quality, req)].append(ilvl)

    def curve(exp, quality, req):
        for dr in range(0, 12):
            for rr in (req - dr, req + dr):
                vals = med.get((exp, quality, rr))
                if vals and len(vals) >= 3:
                    return statistics.median(vals) + (req - rr)
        return req + 5
    table = {(e, q, r): curve(e, q, r) for e in (CLASSIC, TBC, WRATH) for q in (2, 3, 4) for r in range(1, 81)}
    span = {CLASSIC: (1, 60, 1, 40), TBC: (58, 70, 40, 60), WRATH: (68, 80, 40, 60)}
    spread = {}
    for e, (a, b, na, nb) in span.items():
        for q in (2, 3, 4):
            old = table[(e, q, b)] - table[(e, q, a)]
            new = table[(CLASSIC, q, nb)] - table[(CLASSIC, q, na)]
            spread[(e, q)] = round(new / old, 4) if old > 0 else 1
    return table, spread


def level_sql(level, exp):
    """SQL for a level's new value. The level decides the band; exp (a SQL expression: the creature's exp, the quest's
    zone's expansion, the item's) only where bands overlap: 58-60 the Burning Crusade's if so marked, else Classic;
    61-63 Classic's if so marked (its raid bosses and elites), else the Burning Crusade's; 64-67 the Burning
    Crusade's; 68-70 Wrath's if so marked, else the Burning Crusade's; 71 and up Wrath's. Above 83: placeholders,
    kept. Signed: the columns are unsigned, and level - 58 would not be."""
    lvl = f'CAST({level} AS SIGNED)'
    tbc = f'ROUND(40 + ({lvl} - 58) * 20 / 12)'
    wrath = f'ROUND(40 + ({lvl} - 68) * 20 / 12)'
    classic = lvl
    return (f"(CASE WHEN {lvl} > 83 THEN {lvl} WHEN {lvl} >= 71 THEN {wrath} "
            f"WHEN {lvl} >= 68 THEN IF({exp} = 2, {wrath}, {tbc}) WHEN {lvl} >= 64 THEN {tbc} "
            f"WHEN {lvl} >= 61 THEN IF({exp} = 0, {classic}, {tbc}) "
            f"WHEN {lvl} >= 58 THEN IF({exp} = 1, {tbc}, {classic}) ELSE {classic} END)")


# The level an item rewarded by a quest is for: a Burning Crusade or Wrath quest's level (its zone's: Icecrown's 78-80,
# squished to 57-60), a Classic quest's minimum level (wearable when the quest can be done).
QUEST_REQ = ('LEAST(80, IF(q.`expansion` > 0 AND q.`QuestLevel` > 0, q.`QuestLevel`, '
             'IF(q.`MinLevel` > 0, q.`MinLevel`, q.`QuestLevel`)))')


def values(rows, per_line=8):
    lines = [', '.join(rows[i:i + per_line]) for i in range(0, len(rows), per_line)]
    return ',\n'.join(lines) + ';'


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    points = dbc(sys.argv[1])
    zones = zone_expansions(sys.argv[2])
    table, spread = curves(read_items(sys.argv[3] if len(sys.argv) > 3 else ITEMS))
    stats = range(1, 11)
    out = [
        f'-- The level and power squish (option B), version {VERSION}: written by',
        '-- modules/mod-titan/tools/squish/squish.py, which says what it does. Edit the script, not this file.',
        '-- Safe to run again: it computes from the original values it keeps in titan_squish_*_original.',
        '-- Named zz_ to run after every other world SQL of the module.',
        '',
        '-- Stat budget per item level and quality (RandPropPoints.dbc: 2 uncommon, 3 rare, 4 epic).',
        'DROP TABLE IF EXISTS `titan_squish_points`;',
        'CREATE TABLE `titan_squish_points` (`ilvl` SMALLINT NOT NULL, `quality` TINYINT NOT NULL, '
        '`points` INT NOT NULL, PRIMARY KEY (`ilvl`, `quality`));',
        'INSERT INTO `titan_squish_points` (`ilvl`, `quality`, `points`) VALUES',
        values([f'({i}, {q}, {points[i][col]})' for i in sorted(points) for q, col in ((4, 1), (3, 6), (2, 11))]),
        '',
        '-- Median item level by expansion (0 Classic, 1 Burning Crusade, 2 Wrath), quality and required level.',
        'DROP TABLE IF EXISTS `titan_squish_curve`;',
        'CREATE TABLE `titan_squish_curve` (`expansion` TINYINT NOT NULL, `quality` TINYINT NOT NULL, '
        '`req` TINYINT NOT NULL, `ilvl` FLOAT NOT NULL, PRIMARY KEY (`expansion`, `quality`, `req`));',
        'INSERT INTO `titan_squish_curve` (`expansion`, `quality`, `req`, `ilvl`) VALUES',
        values([f'({e}, {q}, {r}, {v:g})' for (e, q, r), v in sorted(table.items())]),
        '',
        '-- How much an item\'s height above its curve shrinks: the band\'s span, new over old.',
        'DROP TABLE IF EXISTS `titan_squish_spread`;',
        'CREATE TABLE `titan_squish_spread` (`expansion` TINYINT NOT NULL, `quality` TINYINT NOT NULL, '
        '`factor` FLOAT NOT NULL, PRIMARY KEY (`expansion`, `quality`));',
        'INSERT INTO `titan_squish_spread` (`expansion`, `quality`, `factor`) VALUES',
        values([f'({e}, {q}, {f:g})' for (e, q), f in sorted(spread.items())]),
        '',
        '-- Every area\'s expansion (AreaTable.dbc), for quests by their zone.',
        'DROP TABLE IF EXISTS `titan_squish_zone`;',
        'CREATE TABLE `titan_squish_zone` (`zone` INT NOT NULL, `expansion` TINYINT NOT NULL, PRIMARY KEY (`zone`));',
        'INSERT INTO `titan_squish_zone` (`zone`, `expansion`) VALUES',
        values([f'({a}, {e})' for a, e in sorted(zones.items())], 12),
        '',
        '-- Quests: the originals, kept once (here: items with no required level take their quests\' levels).',
        'CREATE TABLE IF NOT EXISTS `titan_squish_quest_original` (`ID` INT UNSIGNED NOT NULL, `QuestLevel` SMALLINT '
        'NOT NULL, `MinLevel` TINYINT UNSIGNED NOT NULL, `MaxLevel` TINYINT UNSIGNED NOT NULL, `expansion` TINYINT '
        'NOT NULL, PRIMARY KEY (`ID`));',
        'INSERT IGNORE INTO `titan_squish_quest_original` SELECT q.`ID`, q.`QuestLevel`, q.`MinLevel`, '
        'COALESCE(a.`MaxLevel`, 0),',
        '    COALESCE(z.`expansion`, IF(q.`QuestLevel` > 70, 2, IF(q.`QuestLevel` > 60, 1, 0)))',
        '    FROM `quest_template` q LEFT JOIN `quest_template_addon` a ON a.`ID` = q.`ID`',
        '    LEFT JOIN `titan_squish_zone` z ON q.`QuestSortID` > 0 AND z.`zone` = q.`QuestSortID`;',
        '',
        '-- Items: the originals, kept once.',
        'CREATE TABLE IF NOT EXISTS `titan_squish_item_original` (`entry` INT UNSIGNED NOT NULL, '
        '`Quality` TINYINT UNSIGNED NOT NULL, `RequiredLevel` TINYINT UNSIGNED NOT NULL, '
        '`ItemLevel` SMALLINT UNSIGNED NOT NULL, `armor` SMALLINT UNSIGNED NOT NULL, `block` INT UNSIGNED NOT NULL, '
        '`dmg_min1` FLOAT NOT NULL, `dmg_max1` FLOAT NOT NULL, `dmg_min2` FLOAT NOT NULL, `dmg_max2` FLOAT NOT NULL, '
        + ', '.join(f'`stat_value{i}` INT NOT NULL' for i in stats) + ', PRIMARY KEY (`entry`));',
        'INSERT IGNORE INTO `titan_squish_item_original` SELECT `entry`, `Quality`, `RequiredLevel`, `ItemLevel`, '
        '`armor`, `block`, `dmg_min1`, `dmg_max1`, `dmg_min2`, `dmg_max2`, '
        + ', '.join(f'`stat_value{i}`' for i in stats) + ' FROM `item_template`',
        '    WHERE `class` IN (2, 4) AND `Quality` <= 4 AND `ScalingStatDistribution` = 0',
        '        AND (`RequiredLevel` > 0 OR (`Quality` >= 2 AND `ItemLevel` > 1 AND `entry` < 900000));',
        '',
        '-- Items with no required level: where they come from. The quests that reward them (their lowest level)...',
        'DROP TABLE IF EXISTS `titan_squish_item_source`;',
        'CREATE TABLE `titan_squish_item_source` (`entry` INT UNSIGNED NOT NULL, `req` TINYINT UNSIGNED NOT NULL, '
        '`expansion` TINYINT NOT NULL, PRIMARY KEY (`entry`));',
        'INSERT INTO `titan_squish_item_source` (`entry`, `req`, `expansion`)',
        'SELECT o.`entry`, MIN(' + QUEST_REQ + '), MAX(q.`expansion`)',
        '    FROM `titan_squish_item_original` o',
        '    JOIN `quest_template` t ON o.`entry` IN (t.`RewardItem1`, t.`RewardItem2`, t.`RewardItem3`, '
        't.`RewardItem4`,',
        '        t.`RewardChoiceItemID1`, t.`RewardChoiceItemID2`, t.`RewardChoiceItemID3`, t.`RewardChoiceItemID4`,',
        '        t.`RewardChoiceItemID5`, t.`RewardChoiceItemID6`)',
        '    JOIN `titan_squish_quest_original` q ON q.`ID` = t.`ID`',
        '    WHERE o.`RequiredLevel` = 0 AND (q.`MinLevel` > 0 OR q.`QuestLevel` > 0)',
        '    GROUP BY o.`entry`',
        '    -- past Classic\'s item levels only from Burning Crusade or Wrath quests (not a level 1 holiday quest\'s)',
        '    HAVING MAX(o.`ItemLevel`) <= IF(MAX(o.`Quality`) >= 4, 92, 76)',
        '        OR (MIN(' + QUEST_REQ + ') >= 58 AND MAX(q.`expansion`) > 0);',
        '-- ... else, past Classic\'s item levels (green and blue 76, epic 92), where its expansion\'s curve reaches it',
        '-- (Wrath\'s from item level 130, epics 175).',
        'INSERT IGNORE INTO `titan_squish_item_source` (`entry`, `req`, `expansion`)',
        'SELECT x.`entry`, COALESCE(',
        '        (SELECT MIN(c.`req`) FROM `titan_squish_curve` c WHERE c.`expansion` = x.`e` AND c.`quality` = x.`q`',
        '            AND c.`req` BETWEEN x.`low` AND x.`low` + 12 AND c.`ilvl` >= x.`ItemLevel`), x.`low` + 12), x.`e`',
        '    FROM (SELECT `entry`, `ItemLevel`, LEAST(4, GREATEST(2, `Quality`)) AS `q`,',
        '            IF(`ItemLevel` >= IF(`Quality` >= 4, 175, 130), 2, 1) AS `e`,',
        '            IF(`ItemLevel` >= IF(`Quality` >= 4, 175, 130), 68, 58) AS `low`',
        '        FROM `titan_squish_item_original`',
        '        WHERE `RequiredLevel` = 0 AND `ItemLevel` > IF(`Quality` >= 4, 92, 76)) x;',
        '',
        '-- Items: their expansion, new required level, new item level and the factors, from the originals.',
        'DROP TABLE IF EXISTS `titan_squish_item_work`;',
        'CREATE TABLE `titan_squish_item_work` (`entry` INT UNSIGNED NOT NULL, `expansion` TINYINT NOT NULL, '
        '`q` TINYINT NOT NULL, `old_req` TINYINT UNSIGNED NOT NULL, `old_ilvl` SMALLINT UNSIGNED NOT NULL, '
        '`new_req` TINYINT UNSIGNED NOT NULL DEFAULT 1, `new_ilvl` SMALLINT UNSIGNED NOT NULL DEFAULT 1, '
        '`stat_factor` FLOAT NOT NULL DEFAULT 1, `level_factor` FLOAT NOT NULL DEFAULT 1, PRIMARY KEY (`entry`));',
        'INSERT INTO `titan_squish_item_work` (`entry`, `expansion`, `q`, `old_req`, `old_ilvl`)',
        'SELECT `entry`,',
        '    CASE WHEN `RequiredLevel` <= 57 THEN 0',
        '        WHEN `RequiredLevel` <= 60 THEN IF(`ItemLevel` <= `RequiredLevel` + 10 OR (`Quality` >= 4 AND '
        '`ItemLevel` <= 92), 0, 1)',
        '        WHEN `RequiredLevel` <= 67 THEN 1',
        '        WHEN `RequiredLevel` <= 70 THEN IF(`ItemLevel` < 130, 1, 2)',
        '        ELSE 2 END,',
        '    LEAST(4, GREATEST(2, `Quality`)), `RequiredLevel`, `ItemLevel`',
        'FROM `titan_squish_item_original` WHERE `RequiredLevel` > 0',
        'UNION ALL SELECT o.`entry`, s.`expansion`, LEAST(4, GREATEST(2, o.`Quality`)), s.`req`, o.`ItemLevel`',
        '    FROM `titan_squish_item_original` o JOIN `titan_squish_item_source` s ON s.`entry` = o.`entry`',
        '    WHERE o.`RequiredLevel` = 0;',
        'UPDATE `titan_squish_item_work` SET `new_req` = ' + level_sql('`old_req`', '`expansion`') + ';',
        'UPDATE `titan_squish_item_work` w',
        '    JOIN `titan_squish_curve` o ON o.`expansion` = w.`expansion` AND o.`quality` = w.`q` AND '
        'o.`req` = LEAST(80, w.`old_req`)',
        '    JOIN `titan_squish_curve` n ON n.`expansion` = 0 AND n.`quality` = w.`q` AND '
        'n.`req` = LEAST(60, w.`new_req`)',
        '    JOIN `titan_squish_spread` s ON s.`expansion` = w.`expansion` AND s.`quality` = w.`q`',
        '    SET w.`new_ilvl` = GREATEST(1, LEAST(w.`old_ilvl`, '
        'ROUND(n.`ilvl` + (CAST(w.`old_ilvl` AS SIGNED) - o.`ilvl`) * s.`factor`)));',
        'UPDATE `titan_squish_item_work` w',
        '    LEFT JOIN `titan_squish_points` po ON po.`ilvl` = LEAST(299, w.`old_ilvl`) AND po.`quality` = w.`q`',
        '    LEFT JOIN `titan_squish_points` pn ON pn.`ilvl` = LEAST(299, w.`new_ilvl`) AND pn.`quality` = w.`q`',
        '    SET w.`level_factor` = w.`new_ilvl` / GREATEST(1, w.`old_ilvl`),',
        '        w.`stat_factor` = LEAST(1, IF(po.`points` > 0 AND pn.`points` > 0, pn.`points` / po.`points`, '
        'w.`new_ilvl` / GREATEST(1, w.`old_ilvl`)));',
        '-- Classic\'s items keep (or get back) their own values.',
        'UPDATE `titan_squish_item_work` SET `new_req` = `old_req`, `new_ilvl` = `old_ilvl`, `level_factor` = 1, '
        '`stat_factor` = 1 WHERE `expansion` = 0;',
        'UPDATE `item_template` t',
        '    JOIN `titan_squish_item_work` w ON w.`entry` = t.`entry`',
        '    JOIN `titan_squish_item_original` o ON o.`entry` = t.`entry`',
        '    SET t.`RequiredLevel` = w.`new_req`, t.`ItemLevel` = w.`new_ilvl`,',
        '        t.`armor` = ROUND(o.`armor` * w.`level_factor`), t.`block` = ROUND(o.`block` * w.`stat_factor`),',
        '        t.`dmg_min1` = o.`dmg_min1` * w.`level_factor`, t.`dmg_max1` = o.`dmg_max1` * w.`level_factor`,',
        '        t.`dmg_min2` = o.`dmg_min2` * w.`level_factor`, t.`dmg_max2` = o.`dmg_max2` * w.`level_factor`,',
        ',\n'.join(f'        t.`stat_value{i}` = IF(o.`stat_value{i}` > 0, GREATEST(1, ROUND(o.`stat_value{i}` * '
                   f'w.`stat_factor`)), ROUND(o.`stat_value{i}` * w.`stat_factor`))' for i in stats) + ';',
        'DROP TABLE `titan_squish_item_work`;',
        'DROP TABLE `titan_squish_item_source`;',
        '',
        '-- Creatures: their level by their expansion, and Classic\'s stats rows (exp 0).',
        'CREATE TABLE IF NOT EXISTS `titan_squish_creature_original` (`entry` INT UNSIGNED NOT NULL, '
        '`minlevel` TINYINT UNSIGNED NOT NULL, `maxlevel` TINYINT UNSIGNED NOT NULL, `exp` SMALLINT NOT NULL, '
        'PRIMARY KEY (`entry`));',
        'INSERT IGNORE INTO `titan_squish_creature_original` SELECT `entry`, `minlevel`, `maxlevel`, `exp` '
        'FROM `creature_template`;',
        'UPDATE `creature_template` t JOIN `titan_squish_creature_original` o ON o.`entry` = t.`entry`',
        '    SET t.`minlevel` = ' + level_sql('o.`minlevel`', 'o.`exp`') + ',',
        '        t.`maxlevel` = ' + level_sql('o.`maxlevel`', 'o.`exp`') + ', t.`exp` = 0',
        '    WHERE o.`maxlevel` <= 83;',
        '',
        '-- Quests: their levels; where bands overlap, by their zone\'s expansion (originals: above).',
        'UPDATE `quest_template` t JOIN `titan_squish_quest_original` o ON o.`ID` = t.`ID`',
        '    SET t.`QuestLevel` = IF(o.`QuestLevel` > 0, ' + level_sql('o.`QuestLevel`', 'o.`expansion`')
        + ', o.`QuestLevel`),',
        '        t.`MinLevel` = IF(o.`MinLevel` > 0, ' + level_sql('o.`MinLevel`', 'o.`expansion`') + ', 0);',
        'UPDATE `quest_template_addon` t JOIN `titan_squish_quest_original` o ON o.`ID` = t.`ID`',
        '    SET t.`MaxLevel` = ' + level_sql('o.`MaxLevel`', 'o.`expansion`') + '',
        '    WHERE o.`MaxLevel` > 0;',
    ]
    OUT.write_text('\n'.join(out) + '\n')
    print(f'Wrote {OUT}')


if __name__ == '__main__':
    main()
