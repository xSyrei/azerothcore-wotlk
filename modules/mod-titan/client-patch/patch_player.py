#!/usr/bin/env python3
"""Spells every Hero has (whatever they roll): data for patch_spells.py, which builds and writes them with the rest.

Emberforged Hero: the buff that makes every stat count for a classless Hero. The Hero's base class (the Paladin)
already turns Strength into 2 attack power and Agility into 1 ranged attack power; these auras add the rest:

- Agility: 2 attack power (as Strength) and 2 more ranged attack power (3 in all);
- Intellect: 3 ranged attack power, 2 spell power (damage and healing);
- Spirit: 0.25 spell power (damage and healing).

The buff you see (90600) and two hidden ones (90601, 90602: an aura holds at most three effects). The server puts
all three on every Hero at login (hero_resources.cpp).

Roles (90603-90606), chosen in the Classless window (wildcard.cpp): a buff each, changing the threat the Hero causes
(all schools): Melee and Ranged -50%, Tank +200%, Healer unchanged. Tank also takes 5% less damage (all schools) and
has 5% more armor.

Prestigious (90607): the buff of a prestige run, until level 60 (prestige.cpp).
"""

EFFECT_APPLY_AURA = 6
AURA_DUMMY, AURA_MOD_THREAT = 4, 10
AURA_MOD_DAMAGE_PERCENT_TAKEN, AURA_MOD_RESISTANCE_PCT = 87, 101
ALL_SCHOOLS = 0x7F
SCHOOL_MASK_NORMAL = 0x1        # resistance of the physical school: armor
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
ICON = 'Spell_Fire_Fire'           # the client's own fire icon


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


def role(spell_id, name, icon, threat, text, extra=()):
    """A role's buff: threat% more threat from all schools (0: a dummy, nothing changed), then the extra effects."""
    effect = aura(AURA_MOD_THREAT, threat, ALL_SCHOOLS) if threat else aura(AURA_DUMMY, 1, 0)
    return {'id': spell_id, 'copy': SELF_TEMPLATE, 'name': f'Role: {name}', 'icon': icon,
            'description': f'{text} Chosen in the Classless window.', 'tooltip': text,
            'set': {**OWN, 'Attributes': SPELL_ATTR0_NO_AURA_CANCEL, 'AttributesEx': 0,
                    'AttributesEx3': SPELL_ATTR3_ALLOW_AURA_WHILE_DEAD},
            'clear': [1, 2, 3],
            'effects': {1: effect, **{index + 2: extra_effect for index, extra_effect in enumerate(extra)}}}


# (id, name, icon, threat %): the order of Wildcard::Role (wildcard.h), from 1.
ROLES = [
    (90603, 'Melee', 'Ability_DualWield', -50),
    (90604, 'Ranged', 'Spell_Fire_FlameBolt', -50),
    (90605, 'Healer', 'Spell_Holy_FlashHeal', 0),
    (90606, 'Tank', 'Ability_Warrior_DefensiveStance', 200),
]
# The Tank's more: 5% less damage taken, 5% more armor.
TANK_EXTRA = (aura(AURA_MOD_DAMAGE_PERCENT_TAKEN, -5, ALL_SCHOOLS),
              aura(AURA_MOD_RESISTANCE_PCT, 5, SCHOOL_MASK_NORMAL))
SPELLS += [role(spell_id, name, icon, threat,
                (f'Threat caused {"reduced" if threat < 0 else "increased"} by {abs(threat)}%.' if threat
                 else 'Threat caused unchanged.')
                + (' Damage taken reduced by 5%, armor increased by 5%.' if name == 'Tank' else ''),
                TANK_EXTRA if name == 'Tank' else ())
           for spell_id, name, icon, threat in ROLES]

# Prestigious (90607): during a prestige run, until level 60 (prestige.cpp, which grants the bonus itself: every
# experience source, OnPlayerGiveXP). The aura only shows it.
PRESTIGE_ICON = 'INV_Emberforge_Emberspark'
SPELLS.append({
    'id': 90607, 'copy': SELF_TEMPLATE, 'name': 'Prestigious', 'icon': PRESTIGE_ICON,
    'description': 'Experience gained increased by 10% until level 60. Granted by a prestige.',
    'tooltip': 'Experience gained increased by 10% until level 60.',
    'set': {**OWN, 'Attributes': SPELL_ATTR0_NO_AURA_CANCEL, 'AttributesEx': 0,
            'AttributesEx3': SPELL_ATTR3_ALLOW_AURA_WHILE_DEAD},
    'clear': [1, 2, 3],
    'effects': {1: aura(AURA_DUMMY, 10, 0)},
})

ICONS = [ICON] + [icon for _, _, icon, _ in ROLES] + [PRESTIGE_ICON]
