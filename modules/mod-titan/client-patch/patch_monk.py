#!/usr/bin/env python3
"""The Monk's spells: data for patch_spells.py, which builds and writes them with the Demon Hunter's.

Mistweaver first (the Monk's healer, on mana): each spell a copy of a Blizzard healing spell whose look
fits until the Monk's own visuals, icons and sounds are in (Tranquility's green channel, Regrowth,
Rejuvenation, Lifebloom, Power Word: Shield, Divine Hymn, Nature's Swiftness, Cleanse). The server's
scripts (src/monk.cpp) do what the copies cannot: Vivify's splash on Renewing Mist targets, Renewing Mist
jumping off a healed target, Enveloping Mist's extra healing, Life Cocoon's size and Revival's cleanse.

The Monk's spells are a spell family of their own (MONK_FAMILY, unused by the game), each with a flag bit
(FAMILY_FLAGS), for its talents to aim spell modifiers at, as the Demon Hunter's.

Healing is a base amount growing with level plus a share of spell power (spell_bonus_data, in
titan_monk_scripts.sql); costs are a share of base mana.
"""

MONK_FAMILY = 16
FAMILY_FLAGS = {
    90300: 0x1,     # Soothing Mist
    90301: 0x2,     # Vivify
    90302: 0x4,     # Renewing Mist
    90303: 0x8,     # Enveloping Mist
    90304: 0x10,    # Essence Font
    90305: 0x20,    # Essence Font's bolts
    90306: 0x40,    # Life Cocoon
    90307: 0x80,    # Revival
    90308: 0x100,   # Thunder Focus Tea
    90309: 0x200,   # Detox
    90310: 0x400,   # Vivify's splash
}
VIVIFY, ENVELOPING_MIST = 0x2, 0x8

# Spell.dbc values used below (as in patch_spells.py).
EFFECT_HEAL, EFFECT_APPLY_AURA = 10, 6
AURA_PERIODIC_HEAL, AURA_SCHOOL_ABSORB, AURA_ADD_PCT_MODIFIER = 8, 69, 108
SPELLMOD_CASTING_TIME, SPELLMOD_COST = 10, 14
TARGET_UNIT_CASTER, TARGET_UNIT_TARGET_ALLY, TARGET_UNIT_CASTER_AREA_RAID = 1, 21, 56
CAST_INSTANT, CAST_1_5S, CAST_2S = 1, 16, 5
DURATION_3S, DURATION_6S, DURATION_8S, DURATION_12S, DURATION_20S, DURATION_30S = 27, 32, 31, 29, 18, 9
RANGE_40 = 5
GLOBAL_COOLDOWN = 133
NATURE, ALL_SCHOOLS = 0x8, 0x7F


def heal(base, per_level):
    """A heal (or heal over time tick): base + per_level a level (above 1), plus spell power."""
    return {'BasePoints': base - 1, 'DieSides': 1, 'RealPointsPerLevel': per_level}


def monk(spell_id, level, **fields):
    """The columns every Monk spell sets: its family and flag, no Blizzard cooldown category, its level."""
    return {'SpellFamilyName': MONK_FAMILY, 'SpellFamilyFlags0': FAMILY_FLAGS.get(spell_id, 0),
            'SpellFamilyFlags1': 0, 'SpellFamilyFlags2': 0, 'Category': 0, 'CategoryRecoveryTime': 0,
            'BaseLevel': level, 'SpellLevel': level, 'MaxLevel': 80, 'SchoolMask': NATURE, **fields}


SPELLS = [
    {
        # Tranquility's channel, on one ally: healing every sec for 8 sec.
        'id': 90300, 'copy': 740, 'name': 'Soothing Mist', 'icon': 'Spell_Nature_Tranquility',
        'description': 'Heals the target for $o1 over $d. Channeled; moving ends it.',
        'tooltip': 'Healing every 1 sec.',
        'set': monk(90300, 1, RecoveryTime=0, ManaCostPercentage=4, RangeIndex=RANGE_40, DurationIndex=DURATION_8S),
        'clear': [1, 2, 3],
        'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_PERIODIC_HEAL, 'Amplitude': 1000,
                        'TargetA': TARGET_UNIT_TARGET_ALLY, **heal(15, 2.0)}},
    },
    {
        # Regrowth's cast, a single heal; spell_titan_monk_vivify also heals every ally with the Monk's
        # Renewing Mist for half as much (90310).
        'id': 90301, 'copy': 8936, 'name': 'Vivify', 'icon': 'Ability_Druid_Nourish',
        'description': 'Heals the target for $s1, and every ally with your Renewing Mist for half as much.',
        'set': monk(90301, 1, CastingTimeIndex=CAST_1_5S, ManaCostPercentage=15, RangeIndex=RANGE_40),
        'clear': [2, 3],
        'effects': {1: {'Effect': EFFECT_HEAL, 'TargetA': TARGET_UNIT_TARGET_ALLY, **heal(40, 18.0)}},
    },
    {
        # Rejuvenation: healing every 2 sec for 20 sec; spell_titan_monk_renewing_mist moves it to an injured
        # ally once its target is at full health.
        'id': 90302, 'copy': 774, 'name': 'Renewing Mist', 'icon': 'Spell_Nature_Rejuvenation',
        'description': 'Surrounds the target with healing mists, restoring $o1 health over $d. When the target '
                       'is at full health, the mist jumps to an injured ally within 20 yards.',
        'tooltip': 'Healing every 2 sec.',
        'set': monk(90302, 4, RecoveryTime=9000, ManaCostPercentage=10, RangeIndex=RANGE_40,
                    DurationIndex=DURATION_20S),
        'effects': {1: {'Amplitude': 2000, **heal(10, 1.4)}},
    },
    {
        # Lifebloom's heal over time, cast: every sec for 6 sec, and the Monk's own healing on the target is
        # 30% stronger meanwhile (titan_monk_healing).
        'id': 90303, 'copy': 33763, 'name': 'Enveloping Mist', 'icon': 'Ability_Druid_Flourish',
        'description': 'Wraps the target in healing mists, restoring $o1 health over $d and increasing the '
                       'healing they receive from you by 30%.',
        'tooltip': 'Healing every 1 sec. Healing received from the Monk increased by 30%.',
        'set': monk(90303, 10, CastingTimeIndex=CAST_2S, ManaCostPercentage=20, RangeIndex=RANGE_40,
                    DurationIndex=DURATION_6S),
        'clear': [2, 3],
        'effects': {1: {'Amplitude': 1000, **heal(20, 3.0)}},
    },
    {
        # Tranquility's channel for 3 sec: a burst of bolts (90305) every sec.
        'id': 90304, 'copy': 740, 'name': 'Essence Font', 'icon': 'Spell_Nature_HealingWay',
        'description': 'Unleashes a rapid twirl of healing bolts at up to 6 allies within 30 yards every sec '
                       'for $d, each healing for $90305s1.',
        'set': monk(90304, 20, RecoveryTime=12000, ManaCostPercentage=25, DurationIndex=DURATION_3S),
        'clear': [1, 3],
        'effects': {2: {'Amplitude': 1000, 'TriggerSpell': 90305}},
    },
    {
        # Tranquility's healing pulse: up to 6 allies around the Monk.
        'id': 90305, 'copy': 44203, 'name': 'Essence Font', 'icon': 'Spell_Nature_HealingWay', 'description': '',
        'set': monk(90305, 20, MaxAffectedTargets=6),
        'effects': {1: heal(30, 6.0)},
    },
    {
        # Power Word: Shield: a shield as big as 30% of the Monk's maximum health (spell_titan_monk_life_cocoon)
        # for 12 sec.
        'id': 90306, 'copy': 17, 'name': 'Life Cocoon', 'icon': 'Spell_Nature_ResistNature',
        'description': 'Encases the target in a cocoon of Chi energy for $d, absorbing damage equal to 30% of '
                       'your maximum health.',
        'tooltip': 'Absorbing damage.',
        'set': monk(90306, 18, RecoveryTime=120000, ManaCostPercentage=10, RangeIndex=RANGE_40,
                    DurationIndex=DURATION_12S),
        'effects': {1: {'BasePoints': 0, 'DieSides': 1, 'MiscValue': ALL_SCHOOLS}},
    },
    {
        # Divine Hymn's heal, on the whole raid around the Monk at once; spell_titan_monk_revival also clears
        # one Magic, Poison and Disease effect from each.
        'id': 90307, 'copy': 64844, 'name': 'Revival', 'icon': 'Spell_Holy_DivineHymn',
        'description': 'Heals all party and raid members within 40 yards for $s1 and clears them of a Magic, '
                       'Poison and Disease effect.',
        'set': monk(90307, 30, RecoveryTime=180000, ManaCostPercentage=35, MaxAffectedTargets=0,
                    StartRecoveryCategory=GLOBAL_COOLDOWN, StartRecoveryTime=1500),
        'clear': [2, 3],
        'effects': {1: {'TargetA': TARGET_UNIT_CASTER_AREA_RAID, 'TargetB': 0, **heal(100, 25.0)}},
    },
    {
        # Nature's Swiftness: the next Vivify is free and the next Enveloping Mist instant (one charge).
        'id': 90308, 'copy': 17116, 'name': 'Thunder Focus Tea', 'icon': 'INV_Drink_04',
        'description': 'Your next Vivify costs no mana, or your next Enveloping Mist is instant.',
        'tooltip': 'Next Vivify free, or next Enveloping Mist instant.',
        'set': monk(90308, 14, RecoveryTime=30000, DurationIndex=DURATION_30S, ProcCharges=1),
        'clear': [1, 2, 3],
        'effects': {
            1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_ADD_PCT_MODIFIER, 'BasePoints': -101, 'DieSides': 1,
                'MiscValue': SPELLMOD_COST, 'Mask': VIVIFY, 'TargetA': TARGET_UNIT_CASTER},
            2: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_ADD_PCT_MODIFIER, 'BasePoints': -101, 'DieSides': 1,
                'MiscValue': SPELLMOD_CASTING_TIME, 'Mask': ENVELOPING_MIST, 'TargetA': TARGET_UNIT_CASTER},
        },
    },
    {
        # Cleanse: one Magic, one Poison and one Disease effect.
        'id': 90309, 'copy': 4987, 'name': 'Detox', 'icon': 'Spell_Nature_NullifyPoison_02',
        'description': 'Removes 1 Magic, 1 Poison and 1 Disease effect from a friendly target.',
        'set': monk(90309, 8, RecoveryTime=8000, ManaCostPercentage=8, RangeIndex=RANGE_40),
    },
    {
        # Vivify's splash: a heal of the script's amount (half of Vivify's).
        'id': 90310, 'copy': 8936, 'name': 'Vivify', 'icon': 'Ability_Druid_Nourish', 'description': '',
        'set': monk(90310, 1, CastingTimeIndex=CAST_INSTANT, ManaCostPercentage=0, ManaCost=0, RangeIndex=RANGE_40,
                    StartRecoveryCategory=0, StartRecoveryTime=0),
        'clear': [2, 3],
        'effects': {1: {'Effect': EFFECT_HEAL, 'TargetA': TARGET_UNIT_TARGET_ALLY, 'BasePoints': 0, 'DieSides': 1,
                        'RealPointsPerLevel': 0.0}},
    },
]

ICONS = sorted({spell['icon'] for spell in SPELLS})
