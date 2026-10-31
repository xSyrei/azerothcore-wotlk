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

import patch_monk_talents

# Spell.dbc values used below (as in patch_spells.py).
FURY = 6
CHAOS, PHYSICAL, SHADOW = 0x7F, 0x1, 0x20
EFFECT_SCHOOL_DAMAGE, EFFECT_APPLY_AURA, EFFECT_HEAL, EFFECT_ENERGIZE, EFFECT_HEAL_PCT = 2, 6, 10, 30, 136
AURA_DUMMY, AURA_PERIODIC_DAMAGE, AURA_PROC_TRIGGER_SPELL = 4, 3, 42
AURA_PERIODIC_ENERGIZE, AURA_MOD_DECREASE_SPEED, AURA_MOD_DAMAGE_PERCENT_DONE = 24, 33, 79
AURA_ADD_FLAT_MODIFIER, AURA_ADD_PCT_MODIFIER = 107, 108
SPELLMOD_DAMAGE, SPELLMOD_DURATION, SPELLMOD_CRITICAL_CHANCE, SPELLMOD_COOLDOWN, SPELLMOD_COST = 0, 1, 7, 11, 14
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
    90124: 0x800000,    # Shear
    90125: 0x1000000,   # Soul Cleave
    90127: 0x2000000,   # Demon Spikes
    90128: 0x4000000,   # Fiery Brand
    90129: 0x8000000,   # Torment
    90132: 0x10000000,  # Sigil of Chains
    90135: 0x20000000,  # Spirit Bomb
    90136: 0x40000000,  # Spirit Bomb's explosion
    90138: 0x80000000,  # Soul Barrier
}
DEMONS_BITE, CHAOS_STRIKE, BLADE_DANCE_STRIKES, FEL_RUSH_HIT = 0x1, 0x2, 0x8, 0x20
EYE_BEAM_BEAM, THROW_GLAIVE, CHAOS_NOVA = 0x80, 0x100, 0x200
IMMOLATION_PULSE, SHEAR, SOUL_CLEAVE, DEMON_SPIKES = 0x10000, 0x800000, 0x1000000, 0x2000000
SIGIL_OF_CHAINS = 0x10000000
AURA_SCHOOL_ABSORB, AURA_PERIODIC_DUMMY, AURA_PERIODIC_HEAL, AURA_DAMAGE_SHIELD = 69, 226, 8, 15
AURA_MOD_RESISTANCE_PCT, AURA_MOD_DAMAGE_PERCENT_TAKEN = 101, 87
FIRE = 0x4
DURATION_12S, DURATION_3MIN = 29, 25
# Direct Chaos damage, and every direct damage spell (strikes, hits, pulses).
CHAOS_DAMAGE = CHAOS_STRIKE | FEL_RUSH_HIT | EYE_BEAM_BEAM | CHAOS_NOVA | 0x40000
ALL_DAMAGE = (DEMONS_BITE | CHAOS_DAMAGE | BLADE_DANCE_STRIKES | THROW_GLAIVE | 0x2000 | 0x10000 | 0x80000
              | 0x100000 | 0x200000 | 0x800000 | 0x1000000 | 0x4000000)

TALENT_CLASS_MASK = 1 << 11     # the Demon Hunter, class 12
HAVOC_TAB, VENGEANCE_TAB = 412, 413
TABS = [
    # id, name, order, icon (one of patch_spells.ICONS), class mask; the Monk's trees (patch_monk_talents.py) after
    (HAVOC_TAB, 'Havoc', 0, 'Ability_DemonHunter_ChaosStrike', TALENT_CLASS_MASK),
    (VENGEANCE_TAB, 'Vengeance', 1, 'Ability_DemonHunter_SoulCleave', TALENT_CLASS_MASK),
] + patch_monk_talents.TABS
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


# The Havoc tree: (name, row, column, icon, prerequisite talent or None, ranks, description, effects, set), four
# columns as the game's trees. Their order is their ids (talent and rank spells): only ever add at the end.
# description and effects take the rank (1 up); set is extra Spell.dbc columns (procs).
HAVOC = [
    ('Blind Fury', 0, 1, 'Ability_DemonHunter_EyeBeam', None, 3,
     lambda r: f'Eye Beam generates {r + 1} Fury every time it strikes.',
     lambda r: {1: dummy(r + 1)}, {}),
    ('Improved Chaos Strike', 0, 2, 'Ability_DemonHunter_ChaosStrike', None, 2,
     lambda r: f'Chaos Strike deals {10 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, CHAOS_STRIKE, 10 * r)}, {}),
    ('Demonic Appetite', 2, 2, 'Spell_Shadow_SoulLeech_3', None, 3,
     lambda r: f'Chaos Strike has a {10 * r}% chance to feed your hunger, healing you for 5% of your maximum '
               'health and generating 20 Fury.',
     lambda r: {1: dummy(10 * r)}, {}),
    ('Prepared', 1, 3, 'Ability_DemonHunter_VengefulRetreat', None, 2,
     lambda r: f'Vengeful Retreat generates {10 * r} Fury over 5 sec.',
     lambda r: {1: dummy(2 * r)}, {}),
    ('Bloodlet', 2, 0, 'Ability_DemonHunter_Bloodlet', 'Serrated Glaive', 3,
     lambda r: f'Throw Glaive causes enemies to bleed for {20 * r}% of the damage it dealt over 6 sec.',
     lambda r: {1: dummy(20 * r)}, {}),
    ('Serrated Glaive', 1, 0, 'Ability_DemonHunter_ThrowGlaive', None, 2,
     lambda r: f'Throw Glaive deals {15 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, THROW_GLAIVE, 15 * r)}, {}),
    ('Dancing with Fate', 3, 2, 'Ability_DemonHunter_BladeDance', None, 2,
     lambda r: f'Blade Dance\'s strikes deal {10 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, BLADE_DANCE_STRIKES, 10 * r)}, {}),
    ('Master of the Glaive', 3, 0, 'INV_Weapon_Glave_01', None, 1,
     lambda r: 'Throw Glaive\'s cooldown is 3 sec shorter, and it slows its targets by 50% for 6 sec.',
     lambda r: {1: mod(SPELLMOD_COOLDOWN, THROW_GLAIVE, -3000, flat=True), 2: dummy(50)}, {}),
    ('Desperate Instincts', 4, 1, 'Ability_DemonHunter_Blur', None, 2,
     lambda r: f'Taking damage that leaves you below 35% health makes you Blur. Cannot happen more than once '
               f'every {(120, 60)[r - 1]} sec.',
     lambda r: {1: dummy((120, 60)[r - 1])}, {'ProcFlags': PROC_FLAG_TAKEN_DAMAGE, 'ProcChance': 100}),
    ('Critical Chaos', 1, 2, 'Ability_CriticalStrike', 'Improved Chaos Strike', 2,
     lambda r: f'Chaos Strike and Blade Dance\'s strikes have a {5 * r}% higher chance to be a critical strike.',
     lambda r: {1: mod(SPELLMOD_CRITICAL_CHANCE, CHAOS_STRIKE | BLADE_DANCE_STRIKES, 5 * r, flat=True)}, {}),
    ('Unleashed Power', 4, 0, 'Spell_Fire_FelFireNova', 'Master of the Glaive', 3,
     lambda r: f'Chaos Nova costs {(33, 66, 100)[r - 1]}% less Fury and its cooldown is {10 * r} sec shorter.',
     lambda r: {1: mod(SPELLMOD_COST, CHAOS_NOVA, -(33, 66, 100)[r - 1]),
                2: mod(SPELLMOD_COOLDOWN, CHAOS_NOVA, -10000 * r, flat=True)}, {}),
    ('Momentum', 2, 3, 'Ability_Rogue_Sprint', 'Prepared', 2,
     lambda r: f'Fel Rush and Vengeful Retreat increase all damage you deal by {5 * r}% for 6 sec.',
     lambda r: {1: dummy(5 * r)}, {}),
    ('Chaos Cleave', 5, 2, 'Ability_Warrior_Cleave', None, 3,
     lambda r: f'Chaos Strike also deals {10 * r}% of its damage as Chaos to up to 3 other enemies within 8 '
               'yards of its target.',
     lambda r: {1: dummy(10 * r)}, {}),
    ('Demon Blades', 6, 1, 'Spell_Shadow_DemonicTactics', None, 1,
     lambda r: 'Your auto attacks deal an additional 20% of your attack power as Shadow damage and generate '
               '12 Fury.',
     lambda r: {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_PROC_TRIGGER_SPELL, 'TriggerSpell': 90156,
                    'TargetA': TARGET_UNIT_CASTER}},
     {'ProcFlags': PROC_FLAG_DONE_MELEE_AUTO_ATTACK, 'ProcChance': 100}),
    ('Fel Mastery', 3, 3, 'Ability_DemonHunter_FelRush', None, 2,
     lambda r: f'Fel Rush deals {15 * r}% more damage and generates {10 * r} Fury when it hits.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, FEL_RUSH_HIT, 15 * r), 2: dummy(10 * r)}, {}),
    ('Any Means Necessary', 7, 0, 'Ability_Warlock_ChaosBolt', None, 3,
     lambda r: f'Your Chaos damage abilities deal {4 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, CHAOS_DAMAGE, 4 * r)}, {}),
    ('Cycle of Hatred', 7, 1, 'Ability_DemonHunter_CycleOfHatred', None, 3,
     lambda r: f'Chaos Strike reduces the remaining cooldown of Eye Beam by {r} sec.',
     lambda r: {1: dummy(r)}, {}),
    ('Trail of Ruin', 4, 2, 'Spell_Fire_FelFlameRing', 'Dancing with Fate', 2,
     lambda r: f'The last strike of Blade Dance burns its targets for {20 * r}% of its damage as Chaos over '
               '4 sec.',
     lambda r: {1: dummy(20 * r)}, {}),
    ('Soul Rending', 8, 0, 'Spell_Shadow_LifeDrain', None, 3,
     lambda r: f'You heal for {4 * r}% of the damage you deal, twice as much in Metamorphosis.',
     lambda r: {1: dummy(4 * r)}, {'ProcFlags': PROC_FLAGS_DONE_DAMAGE, 'ProcChance': 100}),
    ('Know Your Enemy', 8, 2, 'Ability_Hunter_MasterMarksman', None, 2,
     lambda r: f'Your Demon Hunter abilities deal {10 * r}% more damage with critical strikes.',
     lambda r: {1: mod(SPELLMOD_CRIT_DAMAGE_BONUS, ALL_DAMAGE, 10 * r)}, {}),
    ('Unbound Chaos', 5, 3, 'Ability_DemonHunter_UnboundChaos', None, 2,
     lambda r: f'Immolation Aura makes your next Fel Rush within 20 sec deal {25 * r}% more damage.',
     lambda r: {1: dummy(25 * r)}, {}),
    ('Demonic', 9, 1, 'Spell_Shadow_DemonForm', 'Cycle of Hatred', 1,
     lambda r: 'Eye Beam transforms you into Metamorphosis for 8 sec after it ends, if you are not in it.',
     lambda r: {1: dummy(8)}, {}),
    # Follow-ups: a talent's power split with the one it leads to.
    ('Looks Can Kill', 1, 1, 'Spell_Shadow_Charm', 'Blind Fury', 2,
     lambda r: f'Eye Beam deals {10 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, EYE_BEAM_BEAM, 10 * r)}, {}),
    ('Eternal Reaping', 6, 2, 'Ability_Warrior_Bladestorm', 'Chaos Cleave', 1,
     lambda r: 'Chaos Cleave strikes every other enemy within 8 yards of the target, not just 3.',
     lambda r: {1: dummy(1)}, {}),
    ('Inertia', 6, 3, 'Ability_Warrior_DecisiveStrike', 'Unbound Chaos', 1,
     lambda r: 'A Fel Rush empowered by Unbound Chaos also increases all damage you deal by 10% for 5 sec.',
     lambda r: {1: dummy(10)}, {}),
]

# The Vengeance tree, as HAVOC, spread over the 11 rows: each root branches to its follow-ups below and to the side
# (every arrow over empty cells only).
VENGEANCE = [
    ('Improved Shear', 0, 0, 'Ability_DemonHunter_Shear', None, 2,
     lambda r: f'Shear deals {15 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, SHEAR, 15 * r)}, {}),
    ('Shear Fury', 2, 0, 'Ability_Warlock_DemonicEmpowerment', 'Improved Shear', 1,
     lambda r: 'Shear generates 10 more Fury.',
     lambda r: {1: dummy(10)}, {}),
    ('Agonizing Flames', 4, 0, 'Spell_Fire_Incinerate', None, 3,
     lambda r: f'Immolation Aura deals {10 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, IMMOLATION_PULSE, 10 * r)}, {}),
    ('Infernal Armor', 6, 0, 'Spell_Fire_FelImmolation', 'Agonizing Flames', 2,
     lambda r: f'While Immolation Aura burns, your armor is increased by {10 * r}% and enemies striking you in melee '
               f'take {10 * r}% of your attack power as Fire damage.',
     lambda r: {1: dummy(10 * r)}, {}),
    ('Razor Spikes', 1, 2, 'Ability_DemonHunter_DemonSpikes', 'Demonic Wards', 3,
     lambda r: f'Demon Spikes also increases your Physical damage by {5 * r}%.',
     lambda r: {1: dummy(5 * r)}, {}),
    ('Painbringer', 3, 2, 'Ability_DemonHunter_Painbringer', 'Razor Spikes', 2,
     lambda r: f'Consuming Soul Fragments reduces all damage you take by {5 * r}% for 6 sec.',
     lambda r: {1: dummy(5 * r)}, {}),
    ('Quickened Sigils', 4, 3, 'Ability_DemonHunter_QuickenedSigils', None, 2,
     lambda r: f'Sigil of Chains arms {r * 0.5:g} sec faster and its cooldown is {10 * r} sec shorter.',
     lambda r: {1: dummy(500 * r), 2: mod(SPELLMOD_COOLDOWN, SIGIL_OF_CHAINS, -10000 * r, flat=True)}, {}),
    ('Concentrated Sigils', 6, 3, 'Ability_DemonHunter_SigilOfChains', 'Quickened Sigils', 1,
     lambda r: 'Sigil of Chains pulls enemies from 12 yards away instead of 8.',
     lambda r: {1: dummy(12)}, {}),
    ('Focused Cleave', 1, 1, 'Ability_DemonHunter_SoulCleave', 'Improved Shear', 2,
     lambda r: f'Soul Cleave deals {10 * r}% more damage.',
     lambda r: {1: mod(SPELLMOD_DAMAGE, SOUL_CLEAVE, 10 * r)}, {}),
    ('Charred Flesh', 7, 1, 'Ability_DemonHunter_FieryBrand', None, 3,
     lambda r: f'Immolation Aura\'s pulses extend Fiery Brand on the enemies they hit by {r * 0.25:g} sec.',
     lambda r: {1: dummy(250 * r)}, {}),
    ('Fallout', 7, 2, 'Spell_Shadow_SoulGem', None, 2,
     lambda r: f'Each Immolation Aura pulse that hits an enemy has a {15 * r}% chance to shatter a Soul Fragment.',
     lambda r: {1: dummy(15 * r)}, {}),
    ('Cover of Darkness', 2, 3, 'Ability_DemonHunter_Darkness', 'Demonic Wards', 2,
     lambda r: f'Darkness gives a {20 + 5 * r}% chance to avoid all damage from an attack.',
     lambda r: {1: dummy(5 * r)}, {}),
    ('Burning Alive', 8, 1, 'Spell_Fire_FelFlameRing', 'Charred Flesh', 1,
     lambda r: 'Every 2 sec, Fiery Brand spreads to a nearby enemy that is not branded.',
     lambda r: {1: dummy(1)}, {}),
    ('Feed the Demon', 8, 2, 'Spell_Shadow_SacrificialShield', 'Fallout', 2,
     lambda r: f'Each Soul Fragment you consume reduces the remaining cooldown of Demon Spikes by {r * 0.5:g} sec.',
     lambda r: {1: dummy(500 * r)}, {}),
    ('Feast of Souls', 3, 1, 'Spell_Shadow_SoulLeech_3', 'Focused Cleave', 2,
     lambda r: f'Soul Cleave heals you for an additional {8 * r}% of your maximum health over 6 sec.',
     lambda r: {1: dummy(8 * r)}, {}),
    ('Fiery Demise', 9, 1, 'Spell_Fire_FelFire', 'Burning Alive', 2,
     lambda r: f'Enemies branded by your Fiery Brand take {15 * r}% more Fire damage from you.',
     lambda r: {1: dummy(15 * r)}, {}),
    ('Last Resort', 10, 2, 'INV_Misc_Bone_HumanSkull_01', 'Feed the Demon', 1,
     lambda r: 'Damage that would kill you instead leaves you at 40% health. Cannot happen more than once every '
               '3 min.',
     lambda r: {1: dummy(40)}, {}),
    ('Ruinous Bulwark', 9, 3, 'Ability_DemonHunter_SoulBarrier', None, 2,
     lambda r: f'Soul Barrier absorbs {15 * r}% more damage.',
     lambda r: {1: dummy(15 * r)}, {}),
    ('Void Reaver', 5, 1, 'Spell_Shadow_ShadowWordPain', 'Feast of Souls', 3,
     lambda r: f'Enemies struck by Soul Cleave deal {3 * r}% less damage to you for 12 sec.',
     lambda r: {1: dummy(3 * r)}, {}),
    ('Vulnerability', 10, 1, 'Ability_DemonHunter_SpiritBomb', None, 2,
     lambda r: f'Enemies afflicted by Frailty take {3 * r}% more damage from you.',
     lambda r: {1: dummy(3 * r)}, {}),
    # More to survive with.
    ('Demonic Wards', 0, 3, 'Spell_Shadow_AntiMagicShell', None, 3,
     lambda r: f'Reduces all damage you take by {(3, 6, 10)[r - 1]}%.',
     lambda r: {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_MOD_DAMAGE_PERCENT_TAKEN,
                    'BasePoints': -(3, 6, 10)[r - 1] - 1, 'DieSides': 1, 'MiscValue': CHAOS,
                    'TargetA': TARGET_UNIT_CASTER}}, {}),
    ('Fel Flame Fortification', 8, 0, 'Spell_Fire_FireArmor', 'Infernal Armor', 2,
     lambda r: f'While Immolation Aura burns, you take {5 * r}% less damage.',
     lambda r: {1: dummy(5 * r)}, {}),
    ('Calcified Spikes', 5, 2, 'Ability_Warrior_ShieldWall', 'Painbringer', 2,
     lambda r: f'Demon Spikes lasts {2 * r} sec longer.',
     lambda r: {1: mod(SPELLMOD_DURATION, DEMON_SPIKES, 2000 * r, flat=True)}, {}),
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
        # Damage %: the script's value (Inertia's).
        'id': 90160, 'copy': SELF_TEMPLATE, 'name': 'Inertia', 'icon': 'Ability_Warrior_DecisiveStrike',
        'description': '', 'tooltip': 'All damage dealt increased by $s1%.', 'clear': [1, 2, 3],
        'duration': DURATION_5S,
        'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_MOD_DAMAGE_PERCENT_DONE, 'BasePoints': 9,
                        'DieSides': 1, 'MiscValue': CHAOS, 'TargetA': TARGET_UNIT_CASTER}},
    },
    {
        # Armor % (effect 1) and the Fire damage melee attackers take (effect 2): the script's values (Infernal
        # Armor's rank, a share of attack power), for as long as Immolation Aura.
        'id': 90161, 'copy': SELF_TEMPLATE, 'name': 'Infernal Armor', 'icon': 'Spell_Fire_FelImmolation',
        'description': '', 'tooltip': 'Armor increased by $s1%. Melee attackers take $s2 Fire damage.',
        'clear': [1, 2, 3], 'duration': DURATION_6S, 'set': {'SchoolMask': FIRE},
        'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_MOD_RESISTANCE_PCT, 'BasePoints': 9, 'DieSides': 1,
                        'MiscValue': 1, 'TargetA': TARGET_UNIT_CASTER},
                    2: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_DAMAGE_SHIELD, 'BasePoints': 0, 'DieSides': 1,
                        'TargetA': TARGET_UNIT_CASTER}},
    },
    {
        # Healing per 2 sec: the script's value (a share of maximum health, 3 ticks).
        'id': 90162, 'copy': SELF_TEMPLATE, 'name': 'Feast of Souls', 'icon': 'Spell_Shadow_SoulLeech_3',
        'description': '', 'tooltip': 'Healing every 2 sec.', 'clear': [1, 2, 3], 'duration': DURATION_6S,
        'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_PERIODIC_HEAL, 'BasePoints': 0, 'DieSides': 1,
                        'Amplitude': 2000, 'TargetA': TARGET_UNIT_CASTER}},
    },
    {
        # Damage dealt -%: the script's value (Void Reaver's rank).
        'id': 90163, 'copy': HIT_TEMPLATE, 'name': 'Void Reaver', 'icon': 'Spell_Shadow_ShadowWordPain',
        'description': '', 'tooltip': 'Damage dealt reduced by $s1%.', 'clear': [1, 2, 3], 'duration': DURATION_12S,
        'set': {'SchoolMask': SHADOW},
        'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_MOD_DAMAGE_PERCENT_DONE, 'BasePoints': -3,
                        'DieSides': 1, 'MiscValue': CHAOS, 'TargetA': TARGET_UNIT_TARGET_ENEMY}},
    },
    {
        # Physical damage %: the script's value (Razor Spikes' rank), for as long as Demon Spikes.
        'id': 90164, 'copy': SELF_TEMPLATE, 'name': 'Razor Spikes', 'icon': 'Ability_DemonHunter_DemonSpikes',
        'description': '', 'tooltip': 'Physical damage dealt increased by $s1%.', 'clear': [1, 2, 3],
        'duration': DURATION_6S,
        'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_MOD_DAMAGE_PERCENT_DONE, 'BasePoints': 4,
                        'DieSides': 1, 'MiscValue': PHYSICAL, 'TargetA': TARGET_UNIT_CASTER}},
    },
    {
        # Damage taken -%: the script's value (Painbringer's rank).
        'id': 90165, 'copy': SELF_TEMPLATE, 'name': 'Painbringer', 'icon': 'Ability_DemonHunter_Painbringer',
        'description': '', 'tooltip': 'Damage taken reduced by $s1%.', 'clear': [1, 2, 3], 'duration': DURATION_6S,
        'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_MOD_DAMAGE_PERCENT_TAKEN, 'BasePoints': -4,
                        'DieSides': 1, 'MiscValue': CHAOS, 'TargetA': TARGET_UNIT_CASTER}},
    },
    {
        # Damage taken -%: the script's value (Fel Flame Fortification's rank), for as long as Immolation Aura.
        'id': 90166, 'copy': SELF_TEMPLATE, 'name': 'Fel Flame Fortification', 'icon': 'Spell_Fire_FireArmor',
        'description': '', 'tooltip': 'Damage taken reduced by $s1%.', 'clear': [1, 2, 3], 'duration': DURATION_6S,
        'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_MOD_DAMAGE_PERCENT_TAKEN, 'BasePoints': -6,
                        'DieSides': 1, 'MiscValue': CHAOS, 'TargetA': TARGET_UNIT_CASTER}},
    },
    {
        # Last Resort's cooldown, shown on the Demon Hunter (the server's titan_dh_damage_taken checks it).
        'id': 90167, 'copy': SELF_TEMPLATE, 'name': 'Uncontained Fel', 'icon': 'INV_Misc_Bone_HumanSkull_01',
        'description': '', 'tooltip': 'Last Resort cannot save you again yet.', 'clear': [1, 2, 3],
        'duration': DURATION_3MIN,
        'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_DUMMY, 'BasePoints': 0, 'DieSides': 1,
                        'TargetA': TARGET_UNIT_CASTER}},
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


# The trees in id order, each a run of talent ids and rank spells of its own: Havoc's first (its ids came first),
# then Vengeance's, both of the Demon Hunter's family (build_spells' default: None); then the Monk's, of its family.
MONK_FAMILY = patch_monk_talents.patch_monk.MONK_FAMILY
TREES = [
    # tab, talents, first talent id, first rank spell, spell family
    (HAVOC_TAB, HAVOC, FIRST_TALENT_ID, FIRST_RANK_SPELL, None),
    (VENGEANCE_TAB, VENGEANCE, FIRST_TALENT_ID + len(HAVOC), FIRST_RANK_SPELL + sum(t[5] for t in HAVOC), None),
    (patch_monk_talents.MISTWEAVER_TAB, patch_monk_talents.MISTWEAVER, patch_monk_talents.FIRST_TALENT_ID,
     patch_monk_talents.FIRST_RANK_SPELL, MONK_FAMILY),
    (patch_monk_talents.WINDWALKER_TAB, patch_monk_talents.WINDWALKER,
     patch_monk_talents.FIRST_TALENT_ID + len(patch_monk_talents.MISTWEAVER),
     patch_monk_talents.FIRST_RANK_SPELL + sum(t[5] for t in patch_monk_talents.MISTWEAVER), MONK_FAMILY),
]
ALL_TALENTS = [(tab, talent) for tab, tree, *_ in TREES for talent in tree]


def ranked(tree, first_rank):
    """Each talent of a tree with its rank spell ids."""
    spell_id = first_rank
    for talent in tree:
        yield talent, list(range(spell_id, spell_id + talent[5]))
        spell_id += talent[5]


def talents():
    """[(talent id, tab, row, column, rank spell ids, prerequisite talent id)] for TALENT rows."""
    rows = []
    for tab, tree, first_talent, first_rank, _ in TREES:
        ids = {talent[0]: talent_id for talent_id, talent in enumerate(tree, first_talent)}
        for talent_id, (talent, ranks) in enumerate(ranked(tree, first_rank), first_talent):
            _, row, column, _, requires, *_ = talent
            rows.append((talent_id, tab, row, column, ranks, ids[requires] if requires else 0))
    return rows


def spells():
    """The rank spells and helper spells, as patch_spells.py's SPELLS entries."""
    out = []
    for _, tree, _, first_rank, family in TREES:
        own = {'SpellFamilyName': family, 'SpellFamilyFlags0': 0} if family else {}
        for (name, _, _, icon, _, _, description, effects, extra), ranks in ranked(tree, first_rank):
            for rank, spell_id in enumerate(ranks, 1):
                out.append({
                    'id': spell_id, 'copy': TALENT_TEMPLATE, 'name': name, 'rank': f'Rank {rank}', 'icon': icon,
                    'description': description(rank), 'clear': [1, 2, 3],
                    'set': {'BaseLevel': 1, 'SpellLevel': 1, 'MaxLevel': 0, 'ProcFlags': 0, 'ProcChance': 101,
                            **own, **extra},
                    'effects': effects(rank),
                })
    for helper in HELPERS + patch_monk_talents.HELPERS:
        spell = {key: value for key, value in helper.items() if key != 'duration'}
        spell['set'] = {**HELPER_SET, 'DurationIndex': helper['duration'], **helper.get('set', {})}
        out.append(spell)
    return out


SPELLS = spells()
ICONS = sorted({talent[3] for _, talent in ALL_TALENTS} | {tab[3] for tab in TABS}
               | {h['icon'] for h in HELPERS + patch_monk_talents.HELPERS})
