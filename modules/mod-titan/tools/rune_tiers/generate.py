#!/usr/bin/env python3
"""Runes of Enhancement: the catalog the server and the Halo window share.

Epic and Rare runes are written by hand (runes.py); Uncommon runes are every combat ability of the Wildcard pool with
the prefixes it can take (pool.py, from the spell data). Writes:
- data/sql/db-world/titan_rune_catalog.sql: id, rarity, home ability (first rank), prefix (src/rune_tiers.cpp);
- client-patch/AddOns/Emberforge_Runes/Tiers.lua: the same with names and words, for the window;
- .agents/plans/rune-tiers/rune-tiers.CATALOG.md (with --doc): the planning catalog.

Ids never change once given: Epics 1001 on and Rares 2001 on, in runes.py's order (append, never insert or remove);
an Uncommon is 1000000 + its ability's first rank x 8 + its prefix (PREFIXES' index).

Usage: python3 generate.py <patch_spells.py output dir> <original DBC dir> [--doc]
"""

import runpy
import sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(HERE))
import runes  # noqa: E402

PREFIXES = ['Rapid', 'Aggressive', 'Mending', 'Critical', 'Refreshing', 'Provoking']
EFFECT = {'Rapid': 'cast time 10% shorter', 'Aggressive': '10% more damage', 'Mending': '10% more healing',
          'Critical': 'critical strikes deal 20% more', 'Refreshing': 'cooldown 10% shorter',
          'Provoking': '50% more threat (offered to tanks)'}
CLASSES = ['Warrior', 'Paladin', 'Hunter', 'Rogue', 'Priest', 'Death Knight', 'Shaman', 'Mage', 'Warlock', 'Druid',
           'Demon Hunter', 'Monk']
UNCOMMON, RARE, EPIC = 1, 2, 3


def lua(text):
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"') + '"'


def main():
    doc = '--doc' in sys.argv
    args = [a for a in sys.argv[1:] if a != '--doc']
    sys.argv = [str(HERE / 'pool.py')] + args
    catalog = runpy.run_path(str(HERE / 'pool.py'), run_name='pool')['catalog']
    spell_of = {(cls, name): v['spell'] for cls, abilities in catalog.items() for name, v in abilities.items()}
    by_name = {name: v['spell'] for abilities in catalog.values() for name, v in abilities.items()}

    rows = []           # (id, rarity, ability spell, prefix)
    lua_runes = []
    for i, (cls, spec, home, name, text) in enumerate(runes.EPICS):
        rid, spell = 1001 + i, spell_of.get((cls, home), by_name[home])
        rows.append((rid, EPIC, spell, 0))
        lua_runes.append(f'    [{rid}] = {{ 3, {spell}, {lua(name)}, {lua(text)}, {lua(cls)}, {lua(spec)} }},')
    for i, (cls, home, name, text) in enumerate(runes.RARES):
        rid, spell = 2001 + i, spell_of.get((cls, home), by_name[home])
        rows.append((rid, RARE, spell, 0))
        lua_runes.append(f'    [{rid}] = {{ 2, {spell}, {lua(name)}, {lua(text)}, {lua(cls)} }},')
    lua_abilities = []
    for cls in CLASSES:
        for name, v in sorted(catalog.get(cls, {}).items()):
            mask = 0
            for prefix in v['prefixes']:
                p = PREFIXES.index(prefix)
                mask |= 1 << p
                rows.append((1000000 + v['spell'] * 8 + p, UNCOMMON, v['spell'], p))
            lua_abilities.append(f'    [{v["spell"]}] = {{ {lua(name)}, {lua(cls)}, {mask} }},')

    sql = ['-- Runes of Enhancement (src/rune_tiers.cpp): every Epic, Rare and Uncommon rune, its home ability (first',
           '-- rank) and, an Uncommon, its prefix (0 Rapid, 1 Aggressive, 2 Mending, 3 Critical, 4 Refreshing,',
           '-- 5 Provoking). Written by modules/mod-titan/tools/rune_tiers/generate.py; edit that, not this file.',
           'CREATE TABLE IF NOT EXISTS `titan_rune_catalog` (`id` INT UNSIGNED NOT NULL, `rarity` TINYINT UNSIGNED '
           'NOT NULL,', '    `ability` INT UNSIGNED NOT NULL, `prefix` TINYINT UNSIGNED NOT NULL DEFAULT 0, '
           'PRIMARY KEY (`id`));',
           'DELETE FROM `titan_rune_catalog`;',
           'INSERT INTO `titan_rune_catalog` (`id`, `rarity`, `ability`, `prefix`) VALUES']
    line = ''
    tuples = [f'({r[0]}, {r[1]}, {r[2]}, {r[3]})' for r in rows]
    for n, t in enumerate(tuples):
        piece = t + (';' if n == len(tuples) - 1 else ',')
        if len(line) + len(piece) + 1 > 118:
            sql.append(line.rstrip())
            line = ''
        line += piece + ' '
    sql.append(line.rstrip())
    out = ROOT / 'modules/mod-titan/data/sql/db-world/titan_rune_catalog.sql'
    out.write_text('\n'.join(sql) + '\n')

    text = ['-- Runes of Enhancement: the catalog (written by tools/rune_tiers/generate.py; edit that, not this file).',
            '-- runes: id = { rarity (2 Rare, 3 Epic), home ability (first rank), name, effect, class[, spec] }.',
            '-- abilities: first rank = { name, class, prefix mask }: its Uncommons, id 1000000 + rank x 8 + prefix.',
            'EmberforgeRuneTiers = {',
            '  prefixes = { ' + ', '.join(lua(p) for p in PREFIXES) + ' },',
            '  effects = { ' + ', '.join(lua(EFFECT[p]) for p in PREFIXES) + ' },',
            '  runes = {'] + lua_runes + ['  },', '  abilities = {'] + lua_abilities + ['  },', '}']
    (ROOT / 'modules/mod-titan/client-patch/AddOns/Emberforge_Runes/Tiers.lua').write_text('\n'.join(text) + '\n')
    print(f'{len(rows)} runes: {len(runes.EPICS)} Epic, {len(runes.RARES)} Rare, '
          f'{len(rows) - len(runes.EPICS) - len(runes.RARES)} Uncommon')

    if doc:
        write_doc(catalog)


def write_doc(d):
    total = sum(len(v['prefixes']) for c in d.values() for v in c.values())
    epics, rares = defaultdict(list), defaultdict(list)
    for c, sp, h, n, t in runes.EPICS:
        epics[c].append((sp, h, n, t))
    for c, h, n, t in runes.RARES:
        rares[c].append((h, n, t))
    out = ['# Runes of Enhancement: the launch catalog', '',
           'Written by modules/mod-titan/tools/rune_tiers/generate.py --doc. Numbers are first-pass tuning.', '',
           '## The rules', '',
           '- **Sockets.** 5 Epic, 10 Rare. Uncommons: any number, on at most **10 abilities** (the first Uncommon '
           'on an ability claims one of its 10 ability slots).',
           '- **Offers.** Every 5 levels, choose 1 of 3: Uncommon 60%, Rare 30%, Epic 10%. With all 10 ability slots '
           'claimed, Uncommons roll only on those; none left: the offer is a Rare.',
           '- **Home ability.** Runes on an ability the Hero no longer knows go **dormant** (and say why); they wake '
           'when it is learned again.',
           '- **Releasing** a rune, or an ability slot with its Uncommons, is free. **Prestige** keeps every rune.',
           '- **Resonance.** An Epic, a Rare and an Uncommon on one ability: it is **10% stronger**.',
           '- **Utility is out:** conjures, teleports, long buffs, stances, aspects, tracking, mounts, '
           'resurrection.', '',
           '## Uncommon prefixes', '', '| Prefix | Effect |', '|---|---|']
    out += [f'| **{p}** | {EFFECT[p]} |' for p in PREFIXES]
    out += ['', f'**{sum(len(v) for v in d.values())} abilities, {total} Uncommon runes, '
            f'{len(runes.EPICS)} Epic and {len(runes.RARES)} Rare runes.**', '', '## Epic runes', '',
            'The big upgrades: one idea each, two for every spec, simple on purpose (the complex, build-defining '
            'effects belong to the Legendary Runes).', '']
    for c in CLASSES:
        out += [f'### {c}', '', '| Spec | Rune | Ability | Effect |', '|---|---|---|---|']
        out += [f'| {sp} | **{n}** | {h} | {t} |' for sp, h, n, t in epics[c]] + ['']
    out += ['## Rare runes', '', 'Smaller upgrades and interactions between abilities.', '']
    for c in CLASSES:
        out += [f'### {c}', '', '| Rune | Ability | Effect |', '|---|---|---|']
        out += [f'| **{n}** | {h} | {t} |' for h, n, t in rares[c]] + ['']
    out += ['## Uncommon runes', '', 'Every combat ability and the prefixes it can take.', '']
    for c in CLASSES:
        abilities = d.get(c, {})
        out += [f'### {c} ({len(abilities)} abilities, {sum(len(v["prefixes"]) for v in abilities.values())} runes)',
                '', '| Ability | Runes |', '|---|---|']
        for name in sorted(abilities):
            out.append(f'| {name} | ' + ', '.join(f'{p} {name}' for p in abilities[name]['prefixes']) + ' |')
        out.append('')
    path = ROOT / '.agents/plans/rune-tiers/rune-tiers.CATALOG.md'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('\n'.join(out) + '\n')


if __name__ == '__main__':
    main()
