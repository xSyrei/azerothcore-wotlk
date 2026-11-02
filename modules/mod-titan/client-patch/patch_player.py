#!/usr/bin/env python3
"""Spells every Hero has (whatever they roll): data for patch_spells.py, which builds and writes them with the rest.

Emberforged Hero: the buff that makes every stat count for a classless Hero. The Hero's base class (the Paladin)
already turns Strength into 2 attack power and Agility into 1 ranged attack power; these auras add the rest:

- Agility: 2 attack power (as Strength) and 2 more ranged attack power (3 in all);
- Intellect: 3 ranged attack power, 2 spell power (damage and healing);
- Spirit: 0.25 spell power (damage and healing).

The buff you see (90600) and two hidden ones (90601, 90602: an aura holds at most three effects). The server puts
all three on every Hero at login (hero_resources.cpp).
"""

EFFECT_APPLY_AURA = 6
AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT, AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT = 174, 175
AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT, AURA_MOD_ATTACK_POWER_OF_STAT_PERCENT = 212, 268
STAT_AGILITY, STAT_INTELLECT, STAT_SPIRIT = 1, 3, 4
MAGIC_SCHOOLS = 0x7E
TARGET_UNIT_CASTER = 1
DURATION_INFINITE = 21
SELF_TEMPLATE = 26669           # Evasion: an aura on the caster (the buff you see)
PASSIVE_TEMPLATE = 12320        # Cruelty (Rank 1): a passive, hidden aura
SPELL_ATTR0_NO_AURA_CANCEL = 0x80000000
SPELL_ATTR3_ALLOW_AURA_WHILE_DEAD = 0x100000
ICON = 'INV_Emberforge_Emberspark'


def aura(aura_type, pct, misc, misc_b=0):
    """An aura on the caster: pct% of a stat (misc, or misc_b for spell damage by school)."""
    return {'Effect': EFFECT_APPLY_AURA, 'Aura': aura_type, 'BasePoints': pct - 1, 'DieSides': 1,
            'MiscValue': misc, 'MiscValueB': misc_b, 'TargetA': TARGET_UNIT_CASTER, 'TargetB': 0, 'Amplitude': 0,
            'TriggerSpell': 0, 'RadiusIndex': 0, 'RealPointsPerLevel': 0.0}


# No class's spell family; free, instant, level 1, lasting until removed.
OWN = {'SpellFamilyName': 0, 'SpellFamilyFlags0': 0, 'SpellFamilyFlags1': 0, 'SpellFamilyFlags2': 0,
       'Category': 0, 'CategoryRecoveryTime': 0, 'RecoveryTime': 0, 'StartRecoveryCategory': 0,
       'StartRecoveryTime': 0, 'PowerType': 0, 'ManaCost': 0, 'ManaCostPercentage': 0, 'CastingTimeIndex': 1,
       'SpellVisual': 0, 'RangeIndex': 1, 'BaseLevel': 1, 'SpellLevel': 1, 'MaxLevel': 0,
       'DurationIndex': DURATION_INFINITE, 'ProcFlags': 0, 'ProcChance': 101}

SPELLS = [
    {
        'id': 90600, 'copy': SELF_TEMPLATE, 'name': 'Emberforged Hero', 'icon': ICON,
        'description': 'Strength and Agility each grant 2 attack power. Agility and Intellect each grant 3 ranged '
                       'attack power. Intellect grants 2 spell power, and Spirit 0.25.',
        'tooltip': 'Strength and Agility grant 2 attack power each. Agility and Intellect grant 3 ranged attack power '
                   'each. Intellect grants 2 spell power, Spirit 0.25.',
        'set': {**OWN, 'Attributes': SPELL_ATTR0_NO_AURA_CANCEL, 'AttributesEx': 0,
                'AttributesEx3': SPELL_ATTR3_ALLOW_AURA_WHILE_DEAD},
        'clear': [1, 2, 3],
        'effects': {1: aura(AURA_MOD_ATTACK_POWER_OF_STAT_PERCENT, 200, STAT_AGILITY),
                    2: aura(AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT, 200, STAT_AGILITY),
                    3: aura(AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT, 300, STAT_INTELLECT)},
    },
    {
        'id': 90601, 'copy': PASSIVE_TEMPLATE, 'name': 'Emberforged Hero', 'icon': ICON, 'description': '',
        'set': OWN,
        'clear': [1, 2, 3],
        'effects': {1: aura(AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT, 200, MAGIC_SCHOOLS, STAT_INTELLECT),
                    2: aura(AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT, 25, MAGIC_SCHOOLS, STAT_SPIRIT),
                    3: aura(AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT, 200, STAT_INTELLECT)},
    },
    {
        'id': 90602, 'copy': PASSIVE_TEMPLATE, 'name': 'Emberforged Hero', 'icon': ICON, 'description': '',
        'set': OWN,
        'clear': [1, 2, 3],
        'effects': {1: aura(AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT, 25, STAT_SPIRIT)},
    },
]

ICONS = [ICON]
