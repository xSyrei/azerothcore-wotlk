#!/usr/bin/env python3
"""The Dungeon Journal's boss abilities: writes data/sql/db-world/titan_journal_abilities.sql.

A boss scripted in C++ registers its AI under the name creature_template.ScriptName holds (RegisterCreatureAI,
RegisterIcecrownCitadelCreatureAI..., new boss_x()), and its file lists its spells as SPELL_NAME = id. This
reads every script under src/server/scripts that registers a creature AI and keeps, per script name, the spells of
its file (several bosses in one file share them; the journal shows those with a description). Bosses scripted in
SmartAI are read from smart_scripts by the server (src/journal.cpp).

Usage: python3 abilities.py
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SCRIPTS = ROOT / 'src/server/scripts'
OUT = ROOT / 'modules/mod-titan/data/sql/db-world/titan_journal_abilities.sql'
REGISTER = re.compile(r'\bRegister\w*CreatureAI\(\s*(\w+)\s*\)|\bnew\s+(boss_\w+|npc_\w+)\s*\(\s*\)')
SPELL = re.compile(r'\bSPELL_\w+\s*=\s*(\d+)')


def main():
    rows = set()
    for path in sorted(SCRIPTS.rglob('*.cpp')):
        text = path.read_text(encoding='utf8', errors='replace')
        names = {a or b for a, b in REGISTER.findall(text)}
        names = {name for name in names if name.startswith('boss_')}
        spells = sorted({int(spell) for spell in SPELL.findall(text) if 0 < int(spell) < 100000})
        for name in names:
            for spell in spells:
                rows.add((name, spell))
    values = sorted(rows)
    lines = [
        '-- The Dungeon Journal\'s boss abilities (src/journal.cpp): the spells of each C++ boss script\'s file, by',
        '-- script name. Written by modules/mod-titan/tools/journal/abilities.py; edit that, not this file.',
        'CREATE TABLE IF NOT EXISTS `titan_journal_abilities` (`script` VARCHAR(64) NOT NULL,',
        '    `spell` INT UNSIGNED NOT NULL, PRIMARY KEY (`script`, `spell`));',
        'DELETE FROM `titan_journal_abilities`;',
        'INSERT INTO `titan_journal_abilities` (`script`, `spell`) VALUES',
    ]
    chunks = [f"('{name}', {spell})" for name, spell in values]
    lines.append(',\n'.join(', '.join(chunks[i:i + 3]) for i in range(0, len(chunks), 3)) + ';')
    OUT.write_text('\n'.join(lines) + '\n')
    print(f'Wrote {OUT}: {len(values)} spells, {len({n for n, _ in values})} bosses')


if __name__ == '__main__':
    main()
