#!/usr/bin/env python3
"""Ranks of the Demon Hunter's and Monk's abilities: data for patch_spells.py, which builds and writes them.

Every ability whose own numbers grow with level gets a rank every 10 levels from the level it is learned at (Tiger
Palm: 1, 11, 21 ... 71; Rising Sun Kick: 10, 20 ... 80), as Blizzard's class spells do. Each rank is a copy of the
ability that starts at its level with the value the ability had there, grows the same per level until the next
rank, and stops (MaxLevel) where the next rank takes over, so the damage and healing curve is the one the ability
has always had. Wildcard learns the best rank for the Hero's level by itself (wildcard.cpp).

Abilities whose strength is weapon damage, attack power or a script's (Chaos Strike, Blade Dance, Fists of Fury...)
and the utility ones (Fel Rush, Blur, Detox...) keep a single rank.

The server needs the chains too: write_sql() adds them to `spell_ranks`. Scripts are bound to every rank (negative
ids in titan_*_scripts.sql), and spell_bonus_data on the first rank covers them all.
"""

import copy

RANK_STEP = 10
MAX_LEVEL = 80

# First rank -> the first id of its next ranks (block of 10 ids each).
RANKED = {
    90100: 91000,   # Demon's Bite
    90124: 91010,   # Shear
    90125: 91020,   # Soul Cleave
    90300: 91030,   # Soothing Mist
    90301: 91040,   # Vivify
    90302: 91050,   # Renewing Mist
    90303: 91060,   # Enveloping Mist
    90307: 91070,   # Revival
    90320: 91080,   # Tiger Palm
    90321: 91090,   # Blackout Kick
    90322: 91100,   # Rising Sun Kick
}


def rank_levels(first_level):
    """The level each rank is learned at."""
    return list(range(first_level, MAX_LEVEL + 1, RANK_STEP))


def ranks(spells):
    """Sets each ranked ability's first rank in spells (rank text, MaxLevel) and returns its later ranks, and the
    chains: {first rank: [every rank id]}."""
    by_id = {spell['id']: spell for spell in spells}
    added, chains = [], {}
    for first_id, next_id in RANKED.items():
        first = by_id[first_id]
        start = first['set']['SpellLevel']
        levels = rank_levels(start)
        if len(levels) - 1 > RANK_STEP:
            raise ValueError(f'{first_id}: {len(levels)} ranks do not fit a block of {RANK_STEP} ids')
        chain = [first_id]
        for number, level in enumerate(levels, 1):
            spell = first if number == 1 else copy.deepcopy(first)
            if number > 1:
                spell['id'] = next_id + number - 2
                chain.append(spell['id'])
                for effect in spell.get('effects', {}).values():
                    per_level = effect.get('RealPointsPerLevel')
                    if per_level:
                        effect['BasePoints'] += round(per_level * (level - start))
                added.append(spell)
            spell['rank'] = f'Rank {number}'
            spell['set'] = {**spell['set'], 'BaseLevel': level, 'SpellLevel': level,
                            'MaxLevel': levels[number] - 1 if number < len(levels) else MAX_LEVEL}
        chains[first_id] = chain
    return added, chains


def sql(chains):
    """`spell_ranks` rows for the chains."""
    firsts = ', '.join(str(first) for first in chains)
    rows = ',\n'.join(f'({first}, {spell_id}, {number})' for first, chain in chains.items()
                      for number, spell_id in enumerate(chain, 1))
    return ['',
            '-- Ranks of the Demon Hunter\'s and Monk\'s abilities (client-patch/patch_ranks.py).',
            f'DELETE FROM `spell_ranks` WHERE `first_spell_id` IN ({firsts});',
            'INSERT INTO `spell_ranks` (`first_spell_id`, `spell_id`, `rank`) VALUES',
            rows + ';']
