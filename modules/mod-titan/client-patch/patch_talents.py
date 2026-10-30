#!/usr/bin/env python3
"""The Demon Hunter's talent trees: data for patch_spells.py, which builds and writes everything.

Talents only amplify: nothing in a tree is an ability to press. Each talent is a row of the client's
Talent.dbc (and the server's `talent_dbc`) in a tree of TalentTab.dbc (`talenttab_dbc`), its ranks
passive spells (SPELLS, built by patch_spells.py like every other Emberforge spell):

- spell modifiers (damage, cost, cooldown, critical strike chance and damage of chosen Demon Hunter
  spells): the Demon Hunter's spells are a spell family of their own (DH_FAMILY, unused by the game)
  with a flag bit each (FAMILY_FLAGS), so the game's own modifiers aim at them, tooltips included;
- dummy auras holding a number, read by the server's scripts (src/demon_hunter.cpp), with the
  helper spells those scripts cast (heals, Fury, bleeds, buffs);
- one proc (Demon Blades) that the game runs by itself.

The trees belong to the Demon Hunter, class 12 (ClassMask 1 << 11): the server's Wildcard grants
their ranks like any class's talents and the Classless window draws them (classless.cpp).

Never delete a talent row or a rank spell once players have it: the server asserts on loading a
character with a talent spell it does not know.
"""

# Spell.dbc values used below (as in patch_spells.py).
FURY = 6
CHAOS, PHYSICAL, SHADOW = 0x7F, 0x1, 0x20
EFFECT_SCHOOL_DAMAGE, EFFECT_APPLY_AURA, EFFECT_HEAL, EFFECT_ENERGIZE, EFFECT_HEAL_PCT = 2, 6, 10, 30, 136
AURA_DUMMY, AURA_PERIODIC_DAMAGE, AURA_PROC_TRIGGER_SPELL = 4, 3, 42
AURA_PERIODIC_ENERGIZE, AURA_MOD_DECREASE_SPEED, AURA_MOD_DAMAGE_PERCENT_DONE = 24, 33, 79
AURA_ADD_FLAT_MODIFIER, AURA_ADD_PCT_MODIFIER = 107, 108
SPELLMOD_DAMAGE, SPELLMOD_CRITICAL_CHANCE, SPELLMOD_COOLDOWN, SPELLMOD_COST = 0, 7, 11, 14
SPELLMOD_CRIT_DAMAGE_BONUS = 15
TARGET_UNIT_CASTER, TARGET_UNIT_TARGET_ENEMY = 1, 6
PROC_FLAG_DONE_MELEE_AUTO_ATTACK, PROC_FLAG_TAKEN_DAMAGE = 0x4, 0x100000
PROC_FLAGS_DONE_DAMAGE = 0x4 | 0x10 | 0x1000 | 0x10000    # auto attacks and melee, none and magic damage spells
MECHANIC_SNARE, MECHANIC_BLEED = 11, 15
DMG_CLASS_MAGIC, DMG_CLASS_MELEE = 1, 2
DURATION_4S, DURATION_5S, DURATION_6S, DURATION_20S = 35, 28, 32, 18
RANGE_30 = 4
TALENT_TEMPLATE = 12320         # Cruelty (Rank 1): a passive, hidden talent aura
SELF_TEMPLATE = 26669           # Evasion: an aura on the caster
HIT_TEMPLATE = 5857             # Hellfire Effect: an instant damage spell

# The Demon Hunter's spell family, and each spell's bit in it (SpellFamilyFlags, the first word).
DH_FAMILY = 14
FAMILY_FLAGS = {
    90100: 0x1,         # Demon's Bite
    90101: 0x2,         # Chaos Strike
    90102: 0x4,         # Blade Dance
    90107: 0x8,         # Blade Dance's strikes
    90108: 0x8,
    90103: 0x10,        # Fel Rush
    90104: 0x20,        # Fel Rush's hit
    90105: 0x40,        # Eye Beam
    90106: 0x80,        # Eye Beam's beam
    90111: 0x100,       # Throw Glaive
    90112: 0x200,       # Chaos Nova
    90113: 0x400,       # Blur
    90114: 0x800,       # Consume Magic
    90116: 0x1000,      # Metamorphosis
    90117: 0x2000,      # Felblade
    90118: 0x4000,      # Darkness
    90119: 0x8000,      # Immolation Aura
    90120: 0x10000,     # Immolation Aura's pulse
    90121: 0x20000,     # Fel Barrage
    90122: 0x40000,     # Fel Barrage's pulse
    90110: 0x80000,     # Vengeful Retreat's hit (Vengeful Retreat itself is a hunter spell)
    90155: 0x100000,    # Chaos Cleave's hit
    90156: 0x200000,    # Demon Blades' hit
    90157: 0x400000,    # Trail of Ruin
}
DEMONS_BITE, CHAOS_STRIKE, BLADE_DANCE_STRIKES, FEL_RUSH_HIT = 0x1, 0x2, 0x8, 0x20
EYE_BEAM_BEAM, THROW_GLAIVE, CHAOS_NOVA = 0x80, 0x100, 0x200
# Direct Chaos damage, and every direct damage spell (strikes, hits, pulses).
CHAOS_DAMAGE = CHAOS_STRIKE | FEL_RUSH_HIT | EYE_BEAM_BEAM | CHAOS_NOVA | 0x40000
ALL_DAMAGE = (DEMONS_BITE | CHAOS_DAMAGE | BLADE_DANCE_STRIKES | THROW_GLAIVE | 0x2000 | 0x10000 | 0x80000
              | 0x100000 | 0x200000)

TALENT_CLASS_MASK = 1 << 11     # the Demon Hunter, class 12
HAVOC_TAB, VENGEANCE_TAB = 412, 413
TABS = [
    # id, name, order, icon (one of patch_spells.ICONS)
    (HAVOC_TAB, 'Havoc', 0, 'Ability_DemonHunter_ChaosStrike'),
]
FIRST_TALENT_ID = 2300
FIRST_RANK_SPELL = 90200


def mod(op, mask, value, flat=False):
    """A spell modifier effect: op (SPELLMOD_*) on the Demon Hunter spells in mask, value % (or flat)."""
    return {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_ADD_FLAT_MODIFIER if flat else AURA_ADD_PCT_MODIFIER,
            'BasePoints': value - 1, 'DieSides': 1, 'MiscValue': op, 'Mask': mask, 'TargetA': TARGET_UNIT_CASTER}


def dummy(value):
    """A number for the server's scripts (aura amount)."""
    return {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_DUMMY, 'BasePoints': value - 1, 'DieSides': 1,
            'TargetA': TARGET_UNIT_CASTER}


# The Havoc tree: (name, row, column, icon, prerequisite talent or None, ranks, description, effects, set).
# description and effects take the rank (1 up); set is extra Spell.dbc columns (procs).
HAVOC = [
    ('Blind Fury', 0, 0, 'Ability_DemonHunter_EyeBeam', None, 3,
     lambda r: f'Eye Beam deals {10 * r}% more damage and generates {r + 1} Fury every time it strikes.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, EYE_BEAM_BEAM, 10 * r), 2: dummy(r + 1)}, {}),
    ('Improved Chaos Strike', 0, 1, 'Ability_DemonHunter_ChaosStrike', None, 2,
     lambda r: f'Chaos Strike deals {10 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, CHAOS_STRIKE, 10 * r)}, {}),
    ('Demonic Appetite', 0, 2, 'Spell_Shadow_SoulLeech_3', None, 3,
     lambda r: f'Chaos Strike has a {10 * r}% chance to feed your hunger, healing you for 5% of your maximum '
               'health and generating 20 Fury.',
     lambda r: {1: dummy(10 * r)}, {}),
    ('Prepared', 1, 0, 'Ability_DemonHunter_VengefulRetreat', None, 2,
     lambda r: f'Vengeful Retreat generates {10 * r} Fury over 5 sec.',
     lambda r: {1: dummy(2 * r)}, {}),
    ('Bloodlet', 1, 1, 'Ability_DemonHunter_Bloodlet', None, 3,
     lambda r: f'Throw Glaive causes enemies to bleed for {(33, 66, 100)[r - 1]}% of the damage it dealt '
               'over 6 sec.',
     lambda r: {1: dummy((33, 66, 100)[r - 1])}, {}),
    ('Serrated Glaive', 1, 2, 'Ability_DemonHunter_ThrowGlaive', None, 2,
     lambda r: f'Throw Glaive deals {15 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, THROW_GLAIVE, 15 * r)}, {}),
    ('Dancing with Fate', 2, 0, 'Ability_DemonHunter_BladeDance', None, 2,
     lambda r: f'Blade Dance\'s strikes deal {10 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, BLADE_DANCE_STRIKES, 10 * r)}, {}),
    ('Master of the Glaive', 2, 1, 'INV_Weapon_Glave_01', None, 1,
     lambda r: 'Throw Glaive\'s cooldown is 3 sec shorter, and it slows its targets by 50% for 6 sec.',
     lambda r: {1: mod(SPELLMOD_COOLDOWN, THROW_GLAIVE, -3000, flat=True), 2: dummy(50)}, {}),
    ('Desperate Instincts', 2, 2, 'Ability_DemonHunter_Blur', None, 2,
     lambda r: f'Taking damage that leaves you below 35% health makes you Blur. Cannot happen more than once '
               f'every {(120, 60)[r - 1]} sec.',
     lambda r: {1: dummy((120, 60)[r - 1])}, {'ProcFlags': PROC_FLAG_TAKEN_DAMAGE, 'ProcChance': 100}),
    ('Critical Chaos', 3, 0, 'Ability_CriticalStrike', None, 2,
     lambda r: f'Chaos Strike and Blade Dance\'s strikes have a {5 * r}% higher chance to be a critical strike.',
     lambda r: {1: mod(SPELLMOD_CRITICAL_CHANCE, CHAOS_STRIKE | BLADE_DANCE_STRIKES, 5 * r, flat=True)}, {}),
    ('Unleashed Power', 3, 1, 'Spell_Fire_FelFireNova', 'Master of the Glaive', 3,
     lambda r: f'Chaos Nova costs {(33, 66, 100)[r - 1]}% less Fury and its cooldown is {10 * r} sec shorter.',
     lambda r: {1: mod(SPELLMOD_COST, CHAOS_NOVA, -(33, 66, 100)[r - 1]),
                2: mod(SPELLMOD_COOLDOWN, CHAOS_NOVA, -10000 * r, flat=True)}, {}),
    ('Momentum', 3, 2, 'Ability_Rogue_Sprint', None, 2,
     lambda r: f'Fel Rush and Vengeful Retreat increase all damage you deal by {5 * r}% for 6 sec.',
     lambda r: {1: dummy(5 * r)}, {}),
    ('Chaos Cleave', 4, 0, 'Ability_Warrior_Cleave', None, 3,
     lambda r: f'Chaos Strike also deals {10 * r}% of its damage as Chaos to every other enemy within 8 yards '
               'of its target.',
     lambda r: {1: dummy(10 * r)}, {}),
    ('Demon Blades', 4, 1, 'Spell_Shadow_DemonicTactics', None, 1,
     lambda r: 'Your auto attacks deal an additional 20% of your attack power as Shadow damage and generate '
               '12 Fury.',
     lambda r: {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_PROC_TRIGGER_SPELL, 'TriggerSpell': 90156,
                    'TargetA': TARGET_UNIT_CASTER}},
     {'ProcFlags': PROC_FLAG_DONE_MELEE_AUTO_ATTACK, 'ProcChance': 100}),
    ('Fel Mastery', 4, 2, 'Ability_DemonHunter_FelRush', None, 2,
     lambda r: f'Fel Rush deals {15 * r}% more damage and generates {10 * r} Fury when it hits.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, FEL_RUSH_HIT, 15 * r), 2: dummy(10 * r)}, {}),
    ('Any Means Necessary', 5, 0, 'Ability_Warlock_ChaosBolt', None, 3,
     lambda r: f'Your Chaos damage abilities deal {4 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, CHAOS_DAMAGE, 4 * r)}, {}),
    ('Cycle of Hatred', 5, 1, 'Ability_DemonHunter_CycleOfHatred', None, 3,
     lambda r: f'Chaos Strike reduces the remaining cooldown of Eye Beam by {r} sec.',
     lambda r: {1: dummy(r)}, {}),
    ('Trail of Ruin', 5, 2, 'Spell_Fire_FelFlameRing', None, 2,
     lambda r: f'The last strike of Blade Dance burns its targets for {20 * r}% of its damage as Chaos over '
               '4 sec.',
     lambda r: {1: dummy(20 * r)}, {}),
    ('Soul Rending', 6, 0, 'Spell_Shadow_LifeDrain', None, 3,
     lambda r: f'You heal for {4 * r}% of the damage you deal, twice as much in Metamorphosis.',
     lambda r: {1: dummy(4 * r)}, {'ProcFlags': PROC_FLAGS_DONE_DAMAGE, 'ProcChance': 100}),
    ('Know Your Enemy', 6, 1, 'Ability_Hunter_MasterMarksman', None, 2,
     lambda r: f'Your Demon Hunter abilities deal {10 * r}% more damage with critical strikes.',
     lambda r: {1: mod(SPELLMOD_CRIT_DAMAGE_BONUS, ALL_DAMAGE, 10 * r)}, {}),
    ('Unbound Chaos', 6, 2, 'Ability_DemonHunter_UnboundChaos', None, 2,
     lambda r: f'Immolation Aura makes your next Fel Rush within 20 sec deal {50 * r}% more damage.',
     lambda r: {1: dummy(50 * r)}, {}),
    ('Demonic', 7, 1, 'Spell_Shadow_DemonForm', None, 1,
     lambda r: 'Eye Beam transforms you into Metamorphosis for 8 sec after it ends, if you are not in it.',
     lambda r: {1: dummy(8)}, {}),
]

# The scripts' helper spells: what the talents cast (heals, Fury, bleeds, buffs, hits).
HELPERS = [
    {
        'id': 90150, 'copy': SELF_TEMPLATE, 'name': 'Demonic Appetite', 'icon': 'Spell_Shadow_SoulLeech_3',
        'description': '', 'clear': [1, 2, 3], 'duration': 0,
        'effects': {1: {'Effect': EFFECT_HEAL_PCT, 'BasePoints': 4, 'DieSides': 1, 'TargetA': TARGET_UNIT_CASTER},
                    2: {'Effect': EFFECT_ENERGIZE, 'BasePoints': 199, 'DieSides': 1, 'MiscValue': FURY,
                        'TargetA': TARGET_UNIT_CASTER}},
    },
    {
        # Fury per second: the script's value (Prepared's rank).
        'id': 90151, 'copy': SELF_TEMPLATE, 'name': 'Prepared', 'icon': 'Ability_DemonHunter_VengefulRetreat',
        'description': '', 'tooltip': 'Generating Fury.', 'clear': [1, 2, 3], 'duration': DURATION_5S,
        'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_PERIODIC_ENERGIZE, 'BasePoints': 19,
                        'DieSides': 1, 'Amplitude': 1000, 'MiscValue': FURY, 'TargetA': TARGET_UNIT_CASTER}},
    },
    {
        # Damage per 2 sec: the script's value (a share of Throw Glaive's hit).
        'id': 90152, 'copy': HIT_TEMPLATE, 'name': 'Bloodlet', 'icon': 'Ability_DemonHunter_Bloodlet',
        'description': '', 'tooltip': 'Bleeding.', 'clear': [1, 2, 3], 'duration': DURATION_6S,
        'set': {'SchoolMask': PHYSICAL, 'DmgClass': DMG_CLASS_MELEE},
        'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_PERIODIC_DAMAGE, 'BasePoints': 0, 'DieSides': 1,
                        'Amplitude': 2000, 'Mechanic': MECHANIC_BLEED, 'TargetA': TARGET_UNIT_TARGET_ENEMY}},
    },
    {
        'id': 90153, 'copy': HIT_TEMPLATE, 'name': 'Master of the Glaive', 'icon': 'INV_Weapon_Glave_01',
        'description': '', 'tooltip': 'Movement speed reduced by 50%.', 'clear': [1, 2, 3],
        'duration': DURATION_6S, 'set': {'SchoolMask': PHYSICAL},
        'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_MOD_DECREASE_SPEED, 'BasePoints': -51,
                        'DieSides': 1, 'Mechanic': MECHANIC_SNARE, 'TargetA': TARGET_UNIT_TARGET_ENEMY}},
    },
    {
        # Damage %: the script's value (Momentum's rank).
        'id': 90154, 'copy': SELF_TEMPLATE, 'name': 'Momentum', 'icon': 'Ability_Rogue_Sprint',
        'description': '', 'tooltip': 'All damage dealt increased by $s1%.', 'clear': [1, 2, 3],
        'duration': DURATION_6S,
        'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_MOD_DAMAGE_PERCENT_DONE, 'BasePoints': 4,
                        'DieSides': 1, 'MiscValue': CHAOS, 'TargetA': TARGET_UNIT_CASTER}},
    },
    {
        # Damage: the script's value (a share of Chaos Strike's).
        'id': 90155, 'copy': HIT_TEMPLATE, 'name': 'Chaos Cleave', 'icon': 'Ability_Warrior_Cleave',
        'description': '', 'clear': [1, 2, 3], 'duration': 0, 'set': {'SchoolMask': CHAOS},
        'effects': {1: {'Effect': EFFECT_SCHOOL_DAMAGE, 'BasePoints': 0, 'DieSides': 1,
                        'TargetA': TARGET_UNIT_TARGET_ENEMY}},
    },
    {
        # 1 damage plus attack power (spell_bonus_data), and 12 Fury.
        'id': 90156, 'copy': HIT_TEMPLATE, 'name': 'Demon Blades', 'icon': 'Spell_Shadow_DemonicTactics',
        'description': '', 'clear': [1, 2, 3], 'duration': 0, 'set': {'SchoolMask': SHADOW},
        'effects': {1: {'Effect': EFFECT_SCHOOL_DAMAGE, 'BasePoints': 0, 'DieSides': 1,
                        'TargetA': TARGET_UNIT_TARGET_ENEMY},
                    2: {'Effect': EFFECT_ENERGIZE, 'BasePoints': 119, 'DieSides': 1, 'MiscValue': FURY,
                        'TargetA': TARGET_UNIT_CASTER}},
    },
    {
        # Damage per sec: the script's value (a share of the last Blade Dance strike).
        'id': 90157, 'copy': HIT_TEMPLATE, 'name': 'Trail of Ruin', 'icon': 'Spell_Fire_FelFlameRing',
        'description': '', 'tooltip': 'Burning.', 'clear': [1, 2, 3], 'duration': DURATION_4S,
        'set': {'SchoolMask': CHAOS},
        'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_PERIODIC_DAMAGE, 'BasePoints': 0, 'DieSides': 1,
                        'Amplitude': 1000, 'TargetA': TARGET_UNIT_TARGET_ENEMY}},
    },
    {
        # Healing: the script's value (a share of the damage dealt).
        'id': 90158, 'copy': SELF_TEMPLATE, 'name': 'Soul Rending', 'icon': 'Spell_Shadow_LifeDrain',
        'description': '', 'clear': [1, 2, 3], 'duration': 0,
        'effects': {1: {'Effect': EFFECT_HEAL, 'BasePoints': 0, 'DieSides': 1, 'TargetA': TARGET_UNIT_CASTER}},
    },
    {
        # Damage %: the script's value (Unbound Chaos's rank); the next Fel Rush uses it up.
        'id': 90159, 'copy': SELF_TEMPLATE, 'name': 'Unbound Chaos', 'icon': 'Ability_DemonHunter_UnboundChaos',
        'description': '', 'tooltip': 'Your next Fel Rush deals $s1% more damage.', 'clear': [1, 2, 3],
        'duration': DURATION_20S,
        'effects': {1: mod(SPELLMOD_DAMAGE, FEL_RUSH_HIT, 50)},
    },
]

# Free, instant, off every cooldown, level 1 (scaling with the caster's level up to 80), reaching 30 yards.
HELPER_SET = {'Category': 0, 'CategoryRecoveryTime': 0, 'RecoveryTime': 0, 'StartRecoveryCategory': 0,
              'StartRecoveryTime': 0, 'PowerType': 0, 'ManaCost': 0, 'ManaCostPercentage': 0, 'CastingTimeIndex': 1,
              'SpellVisual': 0, 'RangeIndex': RANGE_30, 'BaseLevel': 1, 'SpellLevel': 1, 'MaxLevel': 80,
              'ProcFlags': 0, 'ProcChance': 101}


def talents():
    """[(talent id, tab, row, column, rank spell ids, prerequisite talent id)] for TALENT rows."""
    rows, ids, spell_id = [], {}, FIRST_RANK_SPELL
    for talent_id, (name, row, column, _, requires, ranks, *_) in enumerate(HAVOC, FIRST_TALENT_ID):
        ids[name] = talent_id
        rows.append((talent_id, HAVOC_TAB, row, column, list(range(spell_id, spell_id + ranks)),
                     ids[requires] if requires else 0))
        spell_id += ranks
    return rows


def spells():
    """The rank spells and helper spells, as patch_spells.py's SPELLS entries."""
    out, spell_id = [], FIRST_RANK_SPELL
    for name, _, _, icon, _, ranks, description, effects, extra in HAVOC:
        for rank in range(1, ranks + 1):
            out.append({
                'id': spell_id, 'copy': TALENT_TEMPLATE, 'name': name, 'rank': f'Rank {rank}', 'icon': icon,
                'description': description(rank), 'clear': [1, 2, 3],
                'set': {'BaseLevel': 1, 'SpellLevel': 1, 'MaxLevel': 0, 'ProcFlags': 0, 'ProcChance': 101,
                        **extra},
                'effects': effects(rank),
            })
            spell_id += 1
    for helper in HELPERS:
        spell = {key: value for key, value in helper.items() if key != 'duration'}
        spell['set'] = {**HELPER_SET, 'DurationIndex': helper['duration'], **helper.get('set', {})}
        out.append(spell)
    return out


SPELLS = spells()
ICONS = sorted({talent[3] for talent in HAVOC} | {tab[3] for tab in TABS} | {h['icon'] for h in HELPERS})
