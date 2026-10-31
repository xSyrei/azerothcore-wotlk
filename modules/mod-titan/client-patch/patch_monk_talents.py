#!/usr/bin/env python3
"""The Monk's talent trees (Mistweaver, Windwalker): data for patch_talents.py, which builds them with the Demon
Hunter's.

As the Demon Hunter's, talents only amplify: spell modifiers aimed at the Monk's spells by their flag bit in the
Monk's spell family (patch_monk.FAMILY_FLAGS), and dummy auras holding a number for the server's scripts
(src/monk.cpp), with the helper spells those scripts cast. Each tree spreads over the 11 rows, every arrow over empty
cells only.

The trees belong to the Monk, class 10 (ClassMask 1 << 9). Their talent ids start at 2400 and their rank spells at
90400 (the Demon Hunter's end below both); their order is their ids: only ever add at the end of a tree.
"""

import patch_monk
from patch_monk import FAMILY_FLAGS

# Spell.dbc values used below (as in patch_talents.py).
EFFECT_APPLY_AURA, EFFECT_SCHOOL_DAMAGE = 6, 2
AURA_DUMMY, AURA_ADD_FLAT_MODIFIER, AURA_ADD_PCT_MODIFIER = 4, 107, 108
AURA_MOD_CASTING_SPEED, AURA_MOD_MELEE_HASTE = 65, 138
SPELLMOD_DAMAGE, SPELLMOD_DURATION, SPELLMOD_CRITICAL_CHANCE, SPELLMOD_CASTING_TIME = 0, 1, 7, 10
SPELLMOD_COOLDOWN, SPELLMOD_COST, SPELLMOD_CRIT_DAMAGE_BONUS, SPELLMOD_DOT = 11, 14, 15, 22
TARGET_UNIT_CASTER, TARGET_UNIT_TARGET_ENEMY = 1, 6
NATURE = 0x8
DURATION_10S = 1

MONK_CLASS_MASK = 1 << 9        # the Monk, class 10
MISTWEAVER_TAB, WINDWALKER_TAB = 415, 416       # (414: Brewmaster's, to come, first in the Monk's order)
TABS = [
    # id, name, order, icon (one of patch_spells.ICONS), class mask
    (MISTWEAVER_TAB, 'Mistweaver', 1, 'Ability_Monk_SoothingMists', MONK_CLASS_MASK),
    (WINDWALKER_TAB, 'Windwalker', 2, 'Ability_Monk_RisingSunKick', MONK_CLASS_MASK),
]
FIRST_TALENT_ID = 2400
FIRST_RANK_SPELL = 90400


def flag(*spell_ids):
    """The Monk spells' family flags, together."""
    bits = 0
    for spell_id in spell_ids:
        bits |= FAMILY_FLAGS[spell_id]
    return bits


SOOTHING_MIST, VIVIFY, RENEWING_MIST, ENVELOPING_MIST = flag(90300), flag(90301), flag(90302), flag(90303)
ESSENCE_FONT, ESSENCE_FONT_BOLT, LIFE_COCOON, REVIVAL = flag(90304), flag(90305), flag(90306), flag(90307)
THUNDER_FOCUS_TEA, VIVIFY_SPLASH, CHI_JI, CRANES_GUST = flag(90308), flag(90310), flag(90311), flag(90312)
JADE_WIND, JADE_WIND_HEAL = flag(90313), flag(90314)
HEALS = flag(90300, 90301, 90302, 90303, 90305, 90307, 90310, 90312, 90314)
TIGER_PALM, BLACKOUT_KICK, RISING_SUN_KICK, FISTS_OF_FURY = flag(90320), flag(90321), flag(90322), flag(90323)
FISTS_OF_FURY_STRIKES, SPINNING_CRANE_KICK, CRANE_KICK_STRIKES = flag(90324), flag(90325), flag(90326)
DRAGON_PUNCH_STRIKES, STORM_EARTH_AND_FIRE, SPIRIT_ECHOES = flag(90328), flag(90329), flag(90330, 90331)
STRIKES = flag(90320, 90321, 90322, 90324, 90326, 90328)


def mod(op, mask, value, flat=False):
    """A spell modifier effect: op (SPELLMOD_*) on the Monk spells in mask, value % (or flat)."""
    return {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_ADD_FLAT_MODIFIER if flat else AURA_ADD_PCT_MODIFIER,
            'BasePoints': value - 1, 'DieSides': 1, 'MiscValue': op, 'Mask': mask, 'TargetA': TARGET_UNIT_CASTER}


def dummy(value):
    """A number for the server's scripts (aura amount)."""
    return {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_DUMMY, 'BasePoints': value - 1, 'DieSides': 1,
            'TargetA': TARGET_UNIT_CASTER}


# The Mistweaver tree, as patch_talents.HAVOC: (name, row, column, icon, prerequisite or None, ranks, description,
# effects, set). Heals over time grow by SPELLMOD_DOT, direct heals by SPELLMOD_DAMAGE.
MISTWEAVER = [
    ('Grace of the Crane', 0, 0, 'Spell_Nature_Rejuvenation', None, 3,
     lambda r: f'All your healing spells heal for {2 * r}% more.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, HEALS, 2 * r), 2: mod(SPELLMOD_DOT, HEALS, 2 * r)}, {}),
    ('Improved Vivify', 0, 2, 'Ability_Monk_Vivify', None, 3,
     lambda r: f'Vivify heals for {5 * r}% more.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, VIVIFY | VIVIFY_SPLASH, 5 * r)}, {}),
    ('Rapid Diffusion', 1, 0, 'Ability_Monk_RenewingMists', 'Grace of the Crane', 2,
     lambda r: f'Reduces the cooldown of Renewing Mist by {1.5 * r:g} sec.',
     lambda r: {1: mod(SPELLMOD_COOLDOWN, RENEWING_MIST, -1500 * r, flat=True)}, {}),
    ('Lifecycles', 1, 3, 'Spell_Nature_LightningShield', None, 2,
     lambda r: f'Reduces the mana cost of Vivify and Enveloping Mist by {10 * r}%.',
     lambda r: {1: mod(SPELLMOD_COST, VIVIFY | ENVELOPING_MIST, -10 * r)}, {}),
    ('Soothing Breath', 2, 1, 'Ability_Monk_SoothingMists', None, 2,
     lambda r: f'Soothing Mist heals for {15 * r}% more.',
     lambda r: {1: mod(SPELLMOD_DOT, SOOTHING_MIST, 15 * r)}, {}),
    ('Vivacious Vivification', 2, 2, 'Spell_Nature_Invisibilty', 'Improved Vivify', 1,
     lambda r: 'Reduces the cast time of Vivify by 0.5 sec.',
     lambda r: {1: mod(SPELLMOD_CASTING_TIME, VIVIFY, -500, flat=True)}, {}),
    ('Rising Mist', 3, 0, 'Spell_Nature_HealingWaveGreater', 'Rapid Diffusion', 3,
     lambda r: f'Vivify extends your Renewing Mist on each ally it heals by {r} sec, up to its full duration.',
     lambda r: {1: dummy(r)}, {}),
    ('Mist Wrap', 3, 3, 'Spell_Monk_EnvelopingMist', None, 2,
     lambda r: f'Increases the duration of Enveloping Mist by {r} sec.',
     lambda r: {1: mod(SPELLMOD_DURATION, ENVELOPING_MIST, 1000 * r, flat=True)}, {}),
    ('Upwelling', 4, 1, 'Ability_Monk_EssenceFont', None, 3,
     lambda r: f'Essence Font\'s bolts heal for {8 * r}% more.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, ESSENCE_FONT_BOLT, 8 * r)}, {}),
    ('Enveloping Breath', 4, 3, 'Spell_Nature_ResistNature', 'Mist Wrap', 2,
     lambda r: f'Enveloping Mist heals for {10 * r}% more.',
     lambda r: {1: mod(SPELLMOD_DOT, ENVELOPING_MIST, 10 * r)}, {}),
    ('Font of Life', 5, 1, 'Spell_Nature_HealingWay', 'Upwelling', 1,
     lambda r: 'Reduces the cooldown of Essence Font by 3 sec.',
     lambda r: {1: mod(SPELLMOD_COOLDOWN, ESSENCE_FONT, -3000, flat=True)}, {}),
    ('Focused Thunder', 5, 2, 'Ability_Monk_ThunderFocusTea', None, 1,
     lambda r: 'Reduces the cooldown of Thunder Focus Tea by 10 sec.',
     lambda r: {1: mod(SPELLMOD_COOLDOWN, THUNDER_FOCUS_TEA, -10000, flat=True)}, {}),
    ('Chrysalis', 6, 0, 'Ability_Monk_ChiCocoon', None, 2,
     lambda r: f'Reduces the cooldown of Life Cocoon by {15 * r} sec.',
     lambda r: {1: mod(SPELLMOD_COOLDOWN, LIFE_COCOON, -15000 * r, flat=True)}, {}),
    ('Cocooned in Light', 6, 1, 'Spell_Holy_PowerWordShield', 'Chrysalis', 3,
     lambda r: f'Life Cocoon absorbs {10 * r}% more damage.',
     lambda r: {1: dummy(10 * r)}, {}),
    ('Invigorating Mists', 7, 0, 'Ability_Monk_RenewingMists', None, 2,
     lambda r: f'Renewing Mist heals for {10 * r}% more.',
     lambda r: {1: mod(SPELLMOD_DOT, RENEWING_MIST, 10 * r)}, {}),
    ('Secret Infusion', 7, 2, 'INV_Drink_04', 'Focused Thunder', 2,
     lambda r: f'Thunder Focus Tea grants you {5 * r}% haste for 10 sec.',
     lambda r: {1: dummy(5 * r)}, {}),
    ('Tear of Morning', 7, 3, 'Spell_Holy_SurgeOfLight', None, 2,
     lambda r: f'Vivify\'s healing of allies with your Renewing Mist is {15 * r}% stronger.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, VIVIFY_SPLASH, 15 * r)}, {}),
    ('Dancing Mists', 8, 0, 'Spell_Nature_Tranquility', 'Invigorating Mists', 3,
     lambda r: f'Renewing Mist has a {10 * r}% chance to also settle on an injured ally within 20 yards of its '
               'target.',
     lambda r: {1: dummy(10 * r)}, {}),
    ('Jade Bond', 8, 2, 'Ability_Monk_RushingJadeWind', None, 2,
     lambda r: f'Refreshing Jade Wind and Chi-Ji\'s Crane\'s Gust heal for {10 * r}% more.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, JADE_WIND_HEAL | CRANES_GUST, 10 * r)}, {}),
    ('Uplifted Spirits', 8, 3, 'Spell_Monk_Revival', None, 2,
     lambda r: f'Reduces the cooldown of Revival by {20 * r} sec.',
     lambda r: {1: mod(SPELLMOD_COOLDOWN, REVIVAL, -20000 * r, flat=True)}, {}),
    ('Clouded Focus', 9, 1, 'Spell_Nature_Sleep', None, 3,
     lambda r: f'Increases the critical effect chance of Vivify and Enveloping Mist by {3 * r}%.',
     lambda r: {1: mod(SPELLMOD_CRITICAL_CHANCE, VIVIFY | VIVIFY_SPLASH | ENVELOPING_MIST, 3 * r, flat=True)}, {}),
    ('Ancient Teachings', 9, 2, 'Monk_Ability_ChiJi', 'Jade Bond', 1,
     lambda r: 'Chi-Ji\'s Crane\'s Gust heals 1 more injured ally every 2 sec.',
     lambda r: {1: dummy(1)}, {}),
    ('Refreshing Breeze', 10, 0, 'Spell_Nature_Cyclone', None, 1,
     lambda r: 'Increases the duration of Refreshing Jade Wind by 1.5 sec.',
     lambda r: {1: mod(SPELLMOD_DURATION, JADE_WIND, 1500, flat=True)}, {}),
    ('Gift of the Celestials', 10, 2, 'Spell_Holy_Ashestoashes', 'Ancient Teachings', 1,
     lambda r: 'Chi-Ji stays 5 sec longer.',
     lambda r: {1: mod(SPELLMOD_DURATION, CHI_JI, 5000, flat=True)}, {}),
]

# The Windwalker tree. Its finishers' damage grows by SPELLMOD_DAMAGE before the combo points scale it.
WINDWALKER = [
    ('Improved Tiger Palm', 0, 1, 'Ability_Monk_TigerPalm', None, 3,
     lambda r: f'Tiger Palm deals {8 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, TIGER_PALM, 8 * r)}, {}),
    ('Iron Fists', 0, 2, 'Ability_Monk_RoundhouseKick', None, 2,
     lambda r: f'Blackout Kick deals {8 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, BLACKOUT_KICK, 8 * r)}, {}),
    ('Swift Palm', 1, 1, 'Ability_Rogue_Sprint', 'Improved Tiger Palm', 2,
     lambda r: f'Reduces the Energy cost of Tiger Palm by {5 * r}.',
     lambda r: {1: mod(SPELLMOD_COST, TIGER_PALM, -5 * r, flat=True)}, {}),
    ('Teachings of the Monastery', 1, 2, 'Spell_Holy_InnerFire', 'Iron Fists', 3,
     lambda r: f'Blackout Kick has a {10 * r}% chance to reset the cooldown of Rising Sun Kick.',
     lambda r: {1: dummy(10 * r)}, {}),
    ('Rising Star', 2, 0, 'Ability_Monk_RisingSunKick', None, 3,
     lambda r: f'Rising Sun Kick deals {6 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, RISING_SUN_KICK, 6 * r)}, {}),
    ('Fast Feet', 2, 3, 'Ability_Monk_CraneKick_New', None, 2,
     lambda r: f'Spinning Crane Kick deals {10 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, CRANE_KICK_STRIKES, 10 * r)}, {}),
    ('Rising Sun\'s Fury', 3, 0, 'Spell_Fire_SunKey', 'Rising Star', 2,
     lambda r: f'Increases the critical strike chance of Rising Sun Kick by {8 * r}%.',
     lambda r: {1: mod(SPELLMOD_CRITICAL_CHANCE, RISING_SUN_KICK, 8 * r, flat=True)}, {}),
    ('Open Palm Strikes', 3, 1, 'Monk_Ability_FistOfFury', None, 2,
     lambda r: f'Fists of Fury deals {10 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, FISTS_OF_FURY_STRIKES, 10 * r)}, {}),
    ('Spiritual Focus', 3, 3, 'Spell_Nature_GiftoftheWild', 'Fast Feet', 2,
     lambda r: f'Reduces the cooldown of Storm, Earth, and Fire by {10 * r} sec.',
     lambda r: {1: mod(SPELLMOD_COOLDOWN, STORM_EARTH_AND_FIRE, -10000 * r, flat=True)}, {}),
    ('Flying Fists', 4, 1, 'Ability_Hunter_SwiftStrike', 'Open Palm Strikes', 1,
     lambda r: 'Reduces the cooldown of Fists of Fury by 4 sec.',
     lambda r: {1: mod(SPELLMOD_COOLDOWN, FISTS_OF_FURY, -4000, flat=True)}, {}),
    ('Combo Breaker', 4, 2, 'Ability_Rogue_SliceDice', None, 3,
     lambda r: f'Tiger Palm has a {6 * r}% chance to award 2 more combo points.',
     lambda r: {1: dummy(6 * r)}, {}),
    ('Dance of Chi-Ji', 5, 0, 'Ability_Whirlwind', None, 2,
     lambda r: f'Spinning Crane Kick deals {15 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, CRANE_KICK_STRIKES, 15 * r)}, {}),
    ('Hardened Soles', 5, 3, 'INV_Boots_Plate_03', None, 3,
     lambda r: f'Increases the critical strike damage of Blackout Kick and Rising Sun Kick by {10 * r}%.',
     lambda r: {1: mod(SPELLMOD_CRIT_DAMAGE_BONUS, BLACKOUT_KICK | RISING_SUN_KICK, 10 * r)}, {}),
    ('Whirling Dragon', 6, 1, 'Ability_Monk_HurricaneStrike', None, 2,
     lambda r: f'Whirling Dragon Punch deals {15 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, DRAGON_PUNCH_STRIKES, 15 * r)}, {}),
    ('Energy Burst', 6, 2, 'Spell_Shadow_ShadowWordDominate', 'Combo Breaker', 2,
     lambda r: f'Your finishing moves restore {15 * r} Energy when spending 5 combo points.',
     lambda r: {1: dummy(15 * r)}, {}),
    ('Crane Vortex', 7, 0, 'Spell_Nature_Cyclone', 'Dance of Chi-Ji', 1,
     lambda r: 'Spinning Crane Kick lasts 0.5 sec longer.',
     lambda r: {1: mod(SPELLMOD_DURATION, SPINNING_CRANE_KICK, 500, flat=True)}, {}),
    ('Thunderfist', 7, 3, 'Spell_Nature_ThunderClap', 'Hardened Soles', 3,
     lambda r: f'Rising Sun Kick also strikes for {15 * r}% of its damage as Nature.',
     lambda r: {1: dummy(15 * r)}, {}),
    ('Touch of the Tiger', 8, 1, 'Ability_Druid_Rake', None, 3,
     lambda r: f'Your Windwalker strikes deal {2 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, STRIKES, 2 * r)}, {}),
    ('Storm\'s Fury', 8, 2, 'Spell_Fire_Elemental_Totem', None, 2,
     lambda r: f'Storm, Earth, and Fire\'s spirits echo your strikes for {10 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, SPIRIT_ECHOES, 10 * r)}, {}),
    ('Xuen\'s Battlegear', 9, 1, 'INV_Chest_Leather_09', 'Touch of the Tiger', 2,
     lambda r: f'Reduces the cooldown of Rising Sun Kick by {r} sec.',
     lambda r: {1: mod(SPELLMOD_COOLDOWN, RISING_SUN_KICK, -1000 * r, flat=True)}, {}),
    ('Elemental Fury', 9, 2, 'Spell_Fire_Volcano', 'Storm\'s Fury', 1,
     lambda r: 'Storm, Earth, and Fire lasts 5 sec longer.',
     lambda r: {1: mod(SPELLMOD_DURATION, STORM_EARTH_AND_FIRE, 5000, flat=True)}, {}),
    ('Transfer the Power', 10, 1, 'Ability_Warrior_Rampage', 'Xuen\'s Battlegear', 3,
     lambda r: f'Increases the critical strike chance of Fists of Fury and Whirling Dragon Punch by {5 * r}%.',
     lambda r: {1: mod(SPELLMOD_CRITICAL_CHANCE, FISTS_OF_FURY_STRIKES | DRAGON_PUNCH_STRIKES, 5 * r, flat=True)},
     {}),
]

# The scripts' helper spells: what the talents cast. Monk spells (their family), no flag bit.
HELPERS = [
    {
        # Secret Infusion: the haste (both values the talent's %, set by spell_titan_monk_thunder_focus_tea).
        'id': 90360, 'copy': 26669, 'name': 'Secret Infusion', 'icon': 'INV_Drink_04',
        'description': '', 'tooltip': 'Haste increased by $s1%.', 'duration': DURATION_10S,
        'set': {'SpellFamilyName': patch_monk.MONK_FAMILY, 'SpellFamilyFlags0': 0, 'RangeIndex': 1},
        'clear': [3],
        'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_MOD_CASTING_SPEED, 'BasePoints': 0, 'DieSides': 1,
                        'TargetA': TARGET_UNIT_CASTER, 'TargetB': 0},
                    2: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_MOD_MELEE_HASTE, 'BasePoints': 0, 'DieSides': 1,
                        'TargetA': TARGET_UNIT_CASTER, 'TargetB': 0}},
    },
    {
        # Thunderfist: Rising Sun Kick's Nature strike (the script's amount).
        'id': 90361, 'copy': 5857, 'name': 'Thunderfist', 'icon': 'Spell_Nature_ThunderClap', 'description': '',
        'duration': 0,
        'set': {'SpellFamilyName': patch_monk.MONK_FAMILY, 'SpellFamilyFlags0': 0, 'SchoolMask': NATURE,
                'MaxAffectedTargets': 0},
        'effects': {1: {'Effect': EFFECT_SCHOOL_DAMAGE, 'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0,
                        'TargetA': TARGET_UNIT_TARGET_ENEMY, 'TargetB': 0, 'RadiusIndex': 0}},
    },
]
