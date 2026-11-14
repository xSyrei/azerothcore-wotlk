#!/usr/bin/env python3
"""Legendary Runes' spells (src/runes.cpp): data for patch_spells.py, which builds and writes them with the rest.

The Rune of Fel Commandment (the design: the "Emberforge Legendary Runes" doc, its Fel Commandment tab), 90700 on:

- 90700 the rune: a buff the Hero wears while their active specialization's rune is Fel Commandment; its procs
  (Demon's Bite, Chaos Strike, Shadow Bolt, Incinerate) grant Fel Command
  (runes.cpp: an AuraScript reads the spell and the crit);
- 90701 Fel Command: up to 5 stacks, 20 sec (each new stack restarts it);
- 90702 a Chaos Bolt Volley's bolt: Chaos Bolt's flight and look, instant, free, off the global cooldown, never
  resisted, its damage set by the script (no caster modifiers: the script has them);
- 90703 the Felguard's guardian aura and 90704 the imps': no threat (they never pull a dungeon's creatures off the
  tank), hidden;
- 90705 Commander's Fury (no longer given: Fel Host took its place; kept for the auras players may still have);
- 90706 Metamorphic Bond (on your demon while you are in Metamorphosis): 30% less damage taken;
- 90707 Fel Rift: 3 sec after a volley, Fel Rush carries you to the glaive's first target;
- 90708 Felstorm: the Felguard's spin, an NPC Whirlwind's tick (weapon damage around it), 4 ticks over 2 sec;
- 90709 Metamorphic Cleave: your demon's share on the enemies beside its target (Metamorphic Bond);
- 90710-90718 the nine modifiers' markers (Invocation, Manifestation, Adaptation: 3 each), hidden.

The Rune of the Sanctified Paragon (the "Rune of the Sanctified Paragon" concept doc), 90730 on:

- 90730 the rune: a buff the Hero wears while their active specialization's rune is the Sanctified Paragon;
- 90731 Sanctified Paragon: Consecration as a lasting aura around the Hero (cast Consecration to turn it on, again
  to turn it off), a tick every second (runes.cpp: the AuraScript deals it); its look: retail's Remorseless Winter
  (spells/PAL_SanctifiedParagon: its meshes, gold and see-through) with a gold ring of glyphs and embers rising;
- 90732 Consecration: a tick's Holy damage (its amount the script's: the Hero's Consecration rank's with its power
  and bonuses, and the aura's own share of spell power; the Hero's modifiers not again);
- 90733 Condemnation (Sanctified Condemnation): 2% a stack, up to 5, on an enemy, 2 sec;
- 90734 Merciful Ground: a heal (its amount set by the script);
- 90735 Bastion of Faith's former damage reduction (no longer given: the revised Bastion is threat only);
- 90736 Weight of the Unworthy: 15% slower, while the enemy stays in the aura;
- 90737 Shelter of the Paragon: an absorb (its amount set by the script), 6 sec;
- 90738 Seat of the Paragon: 10% more output from the aura while still (a larger sigil beneath you);
- 90739 Hallowed Footsteps: a patch of sanctified ground left behind (fading glyphs), 2 sec;
- 90740-90748 its nine modifiers' markers (Presence, Manifestation, Covenant: 3 each), hidden;
- 90749 its waking: the shockwave and flash as the aura turns on (a hidden 3 sec aura, its look only).

The Rune of the Stormbound Conduit (the "Rune of the Stormbound Conduit" concept doc), 90750 on:

- 90750 the rune: a buff the Hero wears while their active specialization's rune is the Stormbound Conduit; its
  procs (Lightning Bolt and Chain Lightning hits) arc (runes.cpp);
- 90751 Static Ward: an absorb the arcs build (its amount and time set by the script), lightning crackling on it;
- 90752 Stormbound Arc: a secondary arc's Nature damage (its amount set by the script, no caster modifiers), a
  strike of lightning where it lands;
- 90753 its look: Chain Lightning's bolt from the enemy it leaves to the next (cast by that enemy, no effect);
- 90754 Lightning Shield: a retaliation the rune makes (Distant Conductor, Storm Within): the Hero's Lightning
  Shield rank's damage, its spell power share through spell_bonus_data; a bolt from the sky;
- 90760-90768 its nine modifiers' markers (Arcs, Lightning Shield, Surge: 3 each), hidden.

The Rune of Echoing Thunder (the "Rune of Echoing Thunder" concept doc), 90770 on:

- 90770 the rune: a buff the Hero wears while their active specialization's rune is Echoing Thunder;
- 90772 Thunder Charge: Thunder Clap's charge on the Hero (a small storm about them; the script sets its time);
- 90774 Thunder Echo: Nature damage (its amount set by the script, never a critical strike of its own);
- 90775 its burst about the Shield Slam target: Thunder Clap's look, cast by the target (no effect);
- 90776 Sheltering Reverberation: an absorb (its amount and time set by the script);
- 90777 Deafening Impact: movement speed 30% slower (the script sets its time);
- 90780-90788 its nine modifiers' markers (Charge, Echo, Resonance: 3 each), hidden.

The Rune of the Wellspring (the "Rune of the Wellspring" concept doc), 90790 on:

- 90790 the rune (90791, the first draft's Wellspring, is gone);
- 90792 Wellspring: the extra jump's heal (its amount set by the script), Chain Heal's bolt and its look;
- 90793 Tidal Shelter: an absorb, 4 sec; 90794 Renewing Current: healing every sec for 4 sec;
- 90795 the jump's bolt from the ally it leaves to the next (cast by that ally, no effect);
- 90800-90808 its nine modifiers' markers (Spring, Earth, Undertow: 3 each), hidden.

The Rune of the Wandering Mist (the "Rune of the Wandering Mist" concept doc), 90810 on:

- 90810 the rune; 90811 Wandering Mist: a ribbon of jade mist's heal (its amount set by the script), Vivify's
  splash look;
- 90812 lesser Enveloping Mist (Comforting Veil): healing every sec for 6 sec, Enveloping Mist's look;
- 90813 Lingering Mist: healing every sec for 4 sec;
- 90814 Jade Shelter: an absorb, 4 sec;
- 90820-90828 its nine modifiers' markers (Mistflow, Technique, Approach: 3 each), hidden.

The five after them (their own concept docs), each with its markers 20 on from its first:

- the Threefold Grave, 90830 (its revised draft): Shared Burial 90831, the Frozen Grave's damage 90838 and slow
  90839, the Plagued Grave's 90840, Grave Pestilence 90841, the Crimson Grave's 90842, Crimson Ward 90843, the
  Graves' grounds 90845-90847 (March of the Dead's on the Hero 90848); 90832-90837, 90844 and 90849 were the first
  draft's (Requiem, the Dominions);
- the Unbroken Onslaught, 90860 (its procs: melee abilities): Weapon Wake 90861, Bloodletting 90862, the wake's
  ground 90863, Aftershock 90864, Second Wind 90865;
- the Dawnbound Crusader, 90890: the first draft's Dawn Vow 90891 (on the ally 90892; unlearned), the echo's heal
  and damage 90894-90895, their looks 90896-90897, Sunburst's 90898, Sacred Ground 90899, Radiant Ward 90900;
- the Hollow Congregation, 90920: the echo's look 90921, Hollow Flay 90922, the barrage's bolt 90923, Accusation
  90924, Last Word 90925, Shadow Mend 90926;
- the Shattered Sun, 90950: Sunshard 90951 (stacks), Frozen Core 90952, Shatter 90953 and its look 90954, Second
  Sunrise 90955 and its look 90956, Scalding Ground 90957.
"""

import math

import patch_player
from patch_player import aura, OWN, HIDDEN, SELF_TEMPLATE, SPELL_ATTR0_NO_AURA_CANCEL, SPELL_ATTR3_ALLOW_AURA_WHILE_DEAD

AURA_DUMMY, AURA_MOD_THREAT = 4, 10
AURA_MOD_DAMAGE_PERCENT_DONE, AURA_MOD_DAMAGE_PERCENT_TAKEN, AURA_MOD_MELEE_HASTE = 79, 87, 138
ALL_SCHOOLS = 0x7F
EFFECT_SCHOOL_DAMAGE = 2
TARGET_UNIT_TARGET_ENEMY = 6
DURATION_10S, DURATION_20S, DURATION_3S, DURATION_INFINITE = 1, 18, 27, 21
RANGE_VISION = 6                                    # 100 yards: a bolt flies to whoever the script names
PROC_DONE_MELEE_SPELL, PROC_DONE_MAGIC_SPELL_NEGATIVE = 0x10, 0x10000

CHAOS_BOLT = 50796
NPC_WHIRLWIND = 15578                               # weapon damage to enemies around the caster
FIREBOLT = 3110
SPELL_ATTR3_ALWAYS_HIT, SPELL_ATTR3_IGNORE_CASTER_MODIFIERS = 0x40000, 0x20000000
SPELL_ATTR4_CANNOT_BE_RESISTED, SPELL_ATTR4_NO_PARTIAL_IMMUNITY = 0x1, 0x800

RUNE_ICON = 'Ability_Rune_FelCommandment'          # icons/spells/: the rune's own
PARAGON_ICON = 'Ability_Rune_SanctifiedParagon'    # icons/spells/: the Sanctified Paragon's own
STORM_ICON = 'Ability_Rune_StormboundConduit'     # icons/spells/: the Stormbound Conduit's own
THUNDER_ICON = 'Ability_Rune_EchoingThunder'       # icons/spells/: Echoing Thunder's own
WELLSPRING_ICON = 'Ability_Rune_Wellspring'        # icons/spells/: the Wellspring's own
MIST_ICON = 'Ability_Rune_WanderingMist'           # icons/spells/: the Wandering Mist's own
CHAIN_HEAL, VISUAL_RIPTIDE = 1064, 12994
AURA_PERIODIC_HEAL, DURATION_4S = 8, 35
SHOCKWAVE, THUNDER_CLAP = 46968, 6343
VISUAL_SHOCKWAVE, VISUAL_THUNDER_BURST, VISUAL_THUNDER_HIT = 10703, 7449, 145
SPELL_ATTR2_CANT_CRIT = 0x20000000
DURATION_6S_INDEX = 32
LIGHTNING_BOLT, CHAIN_LIGHTNING, LIGHTNING_SHIELD_DAMAGE = 403, 421, 26364
# Client SpellVisuals the Stormbound Conduit borrows (impact or state only: nothing on the Hero): a crackle looping
# on the chest (Shredder Aura's), a bolt from the sky (Storm's Fury's Call Lightning), and Chain Lightning's strike
# with a bolt from the sky.
VISUAL_CRACKLE, VISUAL_SKY_STRIKE, VISUAL_ARC_STRIKE = 10778, 12145, 12082
NATURE = 0x8
EFFECT_DUMMY, TARGET_UNIT_TARGET_ANY = 3, 25
SPELL_ATTR0_ALLOW_CAST_WHILE_DEAD = 0x800000
CONSECRATION = 26573
EXORCISM = 879                                      # a direct Holy damage spell
CRIPPLING_POISON = 3409                             # a harmful aura on an enemy (slow)
LESSER_HEAL = 2050
POWER_WORD_SHIELD = 17
HOLY = 0x2
EFFECT_HEAL, EFFECT_PERSISTENT_AREA_AURA = 10, 27
AURA_SCHOOL_ABSORB, AURA_MOD_DECREASE_SPEED, AURA_PERIODIC_DUMMY = 69, 33, 226
AURA_MOD_HEALING_DONE_PERCENT = 136
TARGET_UNIT_TARGET_ALLY, TARGET_DEST_CASTER, TARGET_UNIT_DEST_AREA_ENEMY = 21, 18, 16
# The script lays its grounds where it chooses (CastSpell at x, y, z): at the caster (18) would put every one at the
# Hero's feet.
TARGET_DEST_DEST = 87
RADIUS_8 = 14
DURATION_2S, DURATION_6S = 39, 32
SHOWN = {**OWN, 'Attributes': SPELL_ATTR0_NO_AURA_CANCEL, 'AttributesEx': 0,
         'AttributesEx3': SPELL_ATTR3_ALLOW_AURA_WHILE_DEAD}
# A helper the script casts: free, instant, off the global cooldown, no cooldown of its own.
FREE = {'Category': 0, 'CategoryRecoveryTime': 0, 'RecoveryTime': 0, 'StartRecoveryCategory': 0,
        'StartRecoveryTime': 0, 'PowerType': 0, 'ManaCost': 0, 'ManaCostPercentage': 0, 'CastingTimeIndex': 1,
        'BaseLevel': 1, 'SpellLevel': 1, 'MaxLevel': 0}


def buff(spell_id, name, icon, text, duration, effects, hidden=False, stacks=0):
    return {'id': spell_id, 'copy': SELF_TEMPLATE, 'name': name, 'icon': icon,
            'description': '' if hidden else text, 'tooltip': '' if hidden else text,
            'set': {**(HIDDEN if hidden else SHOWN), 'DurationIndex': duration, 'StackAmount': stacks},
            'clear': [1, 2, 3], 'effects': effects}


SPELLS = [
    {
        **buff(90700, 'Rune of Fel Commandment', RUNE_ICON,
               'Demon\'s Bite, Chaos Strike, Shadow Bolt, Incinerate and your demon\'s attacks grant Fel Command, '
               'up to 5. At 5, Throw Glaive becomes Chaos Bolt Volley.', DURATION_INFINITE,
               {1: aura(AURA_DUMMY, 1, 0)}),
    },
    buff(90701, 'Fel Command', 'Spell_Fire_FelFlameRing',
         'Fel Command. At 5, your next Throw Glaive fires a Chaos Bolt Volley.', DURATION_20S,
         {1: aura(AURA_DUMMY, 1, 0)}, stacks=5),
    {
        'id': 90702, 'copy': CHAOS_BOLT, 'name': 'Chaos Bolt Volley', 'icon': 'Ability_Warlock_ChaosBolt',
        'description': 'A Chaos Bolt of the volley.', 'tooltip': '',
        'set': {**FREE, 'RangeIndex': RANGE_VISION, 'SpellFamilyName': 0, 'SpellFamilyFlags0': 0,
                'SpellFamilyFlags1': 0, 'SpellFamilyFlags2': 0,
                'AttributesEx3': SPELL_ATTR3_ALWAYS_HIT | SPELL_ATTR3_IGNORE_CASTER_MODIFIERS,
                'AttributesEx4': SPELL_ATTR4_NO_PARTIAL_IMMUNITY | SPELL_ATTR4_CANNOT_BE_RESISTED},
        'effects': {1: {'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0}},
    },
    buff(90703, 'Felguard', 'Spell_Shadow_SummonFelGuard', '', DURATION_INFINITE,
         {1: aura(AURA_MOD_THREAT, -100, ALL_SCHOOLS)}, hidden=True),
    buff(90704, 'Fel Imp', 'Spell_Shadow_SummonImp', '', DURATION_INFINITE,
         {1: aura(AURA_MOD_THREAT, -100, ALL_SCHOOLS)}, hidden=True),
    buff(90705, "Commander's Fury", 'Ability_Warlock_DemonicEmpowerment',
         'Damage increased by 30% and attack speed by 20%.', DURATION_10S,
         {1: aura(AURA_MOD_DAMAGE_PERCENT_DONE, 30, ALL_SCHOOLS), 2: aura(AURA_MOD_MELEE_HASTE, 20, 0)}),
    buff(90706, 'Metamorphic Bond', 'Spell_Shadow_DemonForm',
         'Damage taken reduced by 30%, and attacks strike nearby enemies.', DURATION_INFINITE,
         {1: aura(AURA_MOD_DAMAGE_PERCENT_TAKEN, -30, ALL_SCHOOLS)}),
    buff(90707, 'Fel Rift', 'Spell_Shadow_DemonicCircleTeleport',
         'Fel Rush carries you to the glaive\'s first target.', DURATION_3S, {1: aura(AURA_DUMMY, 1, 0)}),
    {
        'id': 90708, 'copy': NPC_WHIRLWIND, 'name': 'Felstorm', 'icon': 'Ability_Warrior_Cleave',
        'description': '', 'tooltip': '', 'set': {**FREE, 'SpellFamilyName': 0},
        'effects': {1: {'BasePoints': 0, 'DieSides': 1}},
    },
    {
        'id': 90709, 'copy': FIREBOLT, 'name': 'Metamorphic Cleave', 'icon': 'Spell_Shadow_DemonForm',
        'description': '', 'tooltip': '',
        'set': {**FREE, 'SpellVisual': 0, 'Speed': 0.0, 'SchoolMask': 1, 'SpellFamilyName': 0,
                'AttributesEx3': SPELL_ATTR3_ALWAYS_HIT | SPELL_ATTR3_IGNORE_CASTER_MODIFIERS},
        'effects': {1: {'Effect': EFFECT_SCHOOL_DAMAGE, 'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0,
                        'TargetA': TARGET_UNIT_TARGET_ENEMY}},
    },
]

# The rune's procs: the spells that grant Fel Command land as melee abilities (Demon's Bite, Chaos Strike) or harmful
# spells (Shadow Bolt, Incinerate); the script picks them out.
SPELLS[0]['set'] = {**SPELLS[0]['set'], 'ProcFlags': PROC_DONE_MELEE_SPELL | PROC_DONE_MAGIC_SPELL_NEGATIVE,
                    'ProcChance': 101}

# The modifiers' markers: (slot, choice) -> 90710 + slot * 3 + choice, hidden.
MODIFIERS = [
    ('Infernal Pact', 'Demonic Ascension', 'Dreadfire Discipline'),
    ('Imp Legion', 'Shattered Glaive', 'Fel Host'),
    ('Metamorphic Bond', 'Fel Rift', 'Pact of Ruin'),
]
SPELLS += [buff(90710 + slot * 3 + choice, name, RUNE_ICON, '', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)},
                hidden=True)
           for slot, names in enumerate(MODIFIERS) for choice, name in enumerate(names)]

# The Rune of the Sanctified Paragon (90730 on; runes.cpp's Sanctified Paragon section).
HARMFUL = {**FREE, 'SpellFamilyName': 0, 'SpellFamilyFlags0': 0, 'SpellFamilyFlags1': 0, 'SpellFamilyFlags2': 0,
           'RangeIndex': RANGE_VISION, 'SpellVisual': 0, 'ProcFlags': 0, 'ProcChance': 101}
SPELLS += [
    buff(90730, 'Rune of the Sanctified Paragon', PARAGON_ICON,
         'Consecration becomes Sanctified Paragon: an aura that follows you, sanctifying the ground within 8 yards at '
         '40% of Consecration\'s damage, for 1% of your base mana a second. Cast Consecration to turn it on or off.',
         DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    {
        **buff(90731, 'Sanctified Paragon', 'Spell_Holy_InnerFire',
               'Sanctifying the ground about you every second, for 1% of your base mana.',
               DURATION_INFINITE, {1: {**aura(AURA_PERIODIC_DUMMY, 1, 0), 'Amplitude': 1000}}),
        'visual': 'SanctifiedParagon',
    },
    {
        'id': 90732, 'copy': EXORCISM, 'name': 'Consecration', 'icon': 'Spell_Holy_InnerFire',
        'description': 'Sanctified ground.', 'tooltip': '',
        'set': {**HARMFUL, 'SchoolMask': HOLY, 'AttributesEx3': SPELL_ATTR3_IGNORE_CASTER_MODIFIERS},
        'effects': {1: {'Effect': EFFECT_SCHOOL_DAMAGE, 'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0,
                        'TargetA': TARGET_UNIT_TARGET_ENEMY, 'TargetB': 0}},
        'clear': [2, 3],
    },
    {
        'id': 90733, 'copy': CRIPPLING_POISON, 'name': 'Condemnation', 'icon': 'Spell_Holy_RighteousFury',
        'description': 'Taking 2% more damage from Consecration a stack.',
        'tooltip': 'Taking $s1% more damage from the Paragon\'s Consecration a stack.',
        'set': {**HARMFUL, 'DurationIndex': DURATION_2S, 'StackAmount': 5, 'SchoolMask': HOLY},
        'effects': {1: {**aura(AURA_DUMMY, 2, 0), 'TargetA': TARGET_UNIT_TARGET_ENEMY}},
        'clear': [2, 3],
    },
    {
        'id': 90734, 'copy': LESSER_HEAL, 'name': 'Merciful Ground', 'icon': 'Spell_Holy_HealingAura',
        'description': 'Sanctified ground mends you.', 'tooltip': '',
        'set': {**FREE, 'SpellFamilyName': 0, 'SpellFamilyFlags0': 0, 'SpellFamilyFlags1': 0, 'SpellFamilyFlags2': 0,
                'RangeIndex': RANGE_VISION, 'SchoolMask': HOLY},
        'effects': {1: {'Effect': EFFECT_HEAL, 'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0,
                        'TargetA': TARGET_UNIT_TARGET_ALLY, 'TargetB': 0}},
        'clear': [2, 3],
    },
    buff(90735, 'Bastion of Faith', 'Spell_Holy_DevotionAura',
         'Damage taken reduced by $s1%: an enemy in your Consecration a stack.', DURATION_2S,
         {1: aura(AURA_MOD_DAMAGE_PERCENT_TAKEN, -1, ALL_SCHOOLS)}, stacks=5),
    {
        'id': 90736, 'copy': CRIPPLING_POISON, 'name': 'Weight of the Unworthy', 'icon': 'Spell_Holy_SealOfWrath',
        'description': 'Movement speed reduced by 15%.', 'tooltip': 'Movement speed reduced by $s1%.',
        'set': {**HARMFUL, 'DurationIndex': DURATION_2S, 'SchoolMask': HOLY},
        'effects': {1: {**aura(AURA_MOD_DECREASE_SPEED, -15, 0), 'TargetA': TARGET_UNIT_TARGET_ENEMY}},
        'clear': [2, 3],
    },
    {
        'id': 90737, 'copy': POWER_WORD_SHIELD, 'name': 'Shelter of the Paragon', 'icon': 'Spell_Holy_PowerWordShield',
        'description': 'Absorbs damage.', 'tooltip': 'Absorbs $s1 damage.',
        'set': {**FREE, 'SpellFamilyName': 0, 'SpellFamilyFlags0': 0, 'SpellFamilyFlags1': 0, 'SpellFamilyFlags2': 0,
                'RangeIndex': RANGE_VISION, 'DurationIndex': DURATION_6S, 'SchoolMask': HOLY},
        'effects': {1: {'Effect': 6, 'Aura': AURA_SCHOOL_ABSORB, 'BasePoints': 0, 'DieSides': 1,
                        'RealPointsPerLevel': 0.0, 'MiscValue': ALL_SCHOOLS, 'TargetA': TARGET_UNIT_TARGET_ALLY,
                        'TargetB': 0}},
        'clear': [2, 3],
    },
    {
        **buff(90738, 'Seat of the Paragon', 'Spell_Holy_HolyGuidance',
               'Standing your ground: your Sanctified Paragon\'s output is 10% higher.', DURATION_INFINITE,
               {1: aura(AURA_DUMMY, 10, 0)}),
        'visual': 'SeatOfTheParagon',
    },
    {
        'id': 90739, 'copy': CONSECRATION, 'name': 'Hallowed Footsteps', 'icon': 'Spell_Holy_InnerFire',
        'description': '', 'tooltip': '', 'visual': 'HallowedFootsteps',
        'set': {**FREE, 'SpellFamilyName': 0, 'SpellFamilyFlags0': 0, 'SpellFamilyFlags1': 0, 'SpellFamilyFlags2': 0,
                'DurationIndex': DURATION_2S, 'RangeIndex': RANGE_VISION},
        'effects': {1: {'Effect': EFFECT_PERSISTENT_AREA_AURA, 'Aura': AURA_DUMMY, 'BasePoints': 0, 'DieSides': 1,
                        'RealPointsPerLevel': 0.0, 'Amplitude': 0, 'TargetA': TARGET_DEST_DEST,
                        'TargetB': TARGET_UNIT_DEST_AREA_ENEMY, 'RadiusIndex': RADIUS_8}},
    },
]
SPELLS += [{
    'id': 90749, 'copy': SELF_TEMPLATE, 'name': 'Sanctified Paragon', 'icon': 'Spell_Holy_InnerFire',
    'description': '', 'tooltip': '', 'visual': 'SanctifiedParagonAwaken',
    'set': {**HIDDEN, 'DurationIndex': DURATION_3S}, 'clear': [1, 2, 3], 'effects': {1: aura(AURA_DUMMY, 1, 0)},
}]
PARAGON_MODIFIERS = [
    ('Boundless Sanctity', 'Hallowed Footsteps', 'Seat of the Paragon'),
    ('Sanctified Condemnation', 'Merciful Ground', 'Shelter of the Paragon'),
    ('Bastion of Faith', 'Weight of the Unworthy', 'Measured Devotion'),
]
SPELLS += [buff(90740 + slot * 3 + choice, name, PARAGON_ICON, '', DURATION_INFINITE,
                {1: aura(AURA_DUMMY, 1, 0)}, hidden=True)
           for slot, names in enumerate(PARAGON_MODIFIERS) for choice, name in enumerate(names)]

# Its look (patch_spells.EFFECTS / VISUALS): a storm of Holy light about the Hero, from two retail Remorseless Winter
# exports (spells/PAL_SanctifiedParagon, wow.export glTFs; every texture made gray so the tint is the color):
# deathknight_remorselesswinter_state's storm (mesh 2, its own smoke band) with blades of light (the crane kick's
# streak) whipping round it and cfx_deathknight_remorselesswinter_aura01's rising funnel (mesh 0), all spread out to a
# low wall at the aura's edge with the middle left clear (hollow, its edge faded in; fade_top: its top fading out;
# stretch: the Hero and what is about them stay in sight), faint as a mist; flakes of light tumbling up through the
# aura (retail's flipping snowflake, an 8-frame flipbook), embers and sparks; a crown of light above the head; a
# shockwave and flash as it wakes (90749). No floor. The sigil and glyph of Seat of the Paragon and Hallowed Footsteps
# are ours (build_paragon_art.py).
PARAGON = 'PAL_SanctifiedParagon'
GOLD, PALE_GOLD, WHITE_GOLD = (1.0, 0.72, 0.25), (1.0, 0.86, 0.5), (1.0, 0.96, 0.82)
DEEP_GOLD = (1.0, 0.55, 0.12)                       # the mist's warmest: vibrant, not white (additive)
STATE = f'{PARAGON}/deathknight_remorselesswinter_state.gltf'
AURA = f'{PARAGON}/cfx_deathknight_remorselesswinter_aura01.gltf'
EFFECTS = {
    'SanctifiedParagon': {'model': 'PAL_SanctifiedParagon', 'effect': 7151, 'shape': 'spun',
                          'textures': (f'{PARAGON}/beam_smokegrey', f'{PARAGON}/streak_halfcircle_01',
                                       f'{PARAGON}/7fx_wavey_mask_02_vert'),
                          'options': {'export': (STATE, AURA), 'layers': (
                              # the storm, spread out to the aura's edge: retail's swirl without its middle
                              # (hollow), stretched to a low wall about 6 to 8 yards out, under 2 yards high
                              {'source': 0, 'mesh': 2, 'texture': 0, 'tint': GOLD, 'alpha': 0.1,
                               'turn': 1.6, 'scroll': (-0.6, 0.0), 'hollow': 1.8, 'fade_top': 0.45,
                               'stretch': (2.8, 2.8, 0.42)},
                              {'source': 0, 'mesh': 2, 'texture': 0, 'tint': DEEP_GOLD, 'alpha': 0.085, 'turn': 2.4,
                               'scroll': (0.4, 0.1), 'hollow': 1.8, 'fade_top': 0.45, 'stretch': (2.6, 2.6, 0.38)},
                              # blades of light whipping round that wall
                              {'source': 0, 'mesh': 2, 'texture': 1, 'tint': PALE_GOLD, 'alpha': 0.17, 'turn': 1.3,
                               'hollow': 1.8, 'fade_top': 0.45, 'stretch': (2.7, 2.7, 0.4)},
                              {'source': 0, 'mesh': 2, 'texture': 1, 'tint': GOLD, 'alpha': 0.13, 'turn': -1.9,
                               'flicker': (0.45, 0.3), 'hollow': 1.8, 'fade_top': 0.45, 'stretch': (2.5, 2.5, 0.35)},
                              # light rising at the boundary (aura01's funnel, its outer part, low)
                              {'source': 1, 'mesh': 0, 'texture': 2, 'tint': GOLD, 'alpha': 0.1, 'turn': 5.0,
                               'scroll': (0.0, -0.8), 'hollow': 2.6, 'fade_top': 0.45,
                               'stretch': (1.85, 1.85, 0.35)})}},
    # Light about the Hero, spread out through the aura (the chest slot): flakes of light tumbling up and spiralling
    # with the storm (retail's flipping snowflake: 8 frames), embers and sparks; the middle left clear.
    'SanctifiedParagonEmbers': {'model': 'PAL_SanctifiedParagonEmbers', 'effect': 7152, 'shape': 'mist',
                                'textures': (f'{PARAGON}/snowflake_flipping_01', f'{PARAGON}/ember_offset_grey',
                                             'MONK_Flare'),
                                'options': {'layers': (
                                    {'texture': 0, 'sheet': (8, 1), 'flip': 0.8, 'tint': WHITE_GOLD, 'alpha': 1.0,
                                     'turn': 3.0, 'pulse': 0, 'rise': (3.0, 1.8),
                                     'cards': tuple((r, -1.0 + 0.15 * (i % 4), 0.32) for i, r in enumerate(
                                         (3.0, 4.2, 5.4, 6.6, 7.4, 3.6, 4.8, 6.0, 7.0, 3.3, 5.1, 6.3, 7.6, 4.5)))},
                                    {'texture': 1, 'tint': WHITE_GOLD, 'alpha': 1.0, 'turn': -3.4, 'pulse': 0.7,
                                     'rise': (3.4, 1.4), 'cards': tuple((r, -1.0, 0.22) for r in
                                                                        (2.8, 3.6, 4.4, 5.2, 6.0, 6.8, 7.6, 3.2, 5.6,
                                                                         4.0, 6.4, 7.2))},
                                    {'texture': 2, 'tint': WHITE_GOLD, 'alpha': 1.0, 'turn': 4.0, 'pulse': 0.5,
                                     'rise': (2.6, 1.2), 'cards': tuple((r, -0.8, 0.16) for r in
                                                                        (3.0, 4.6, 6.2, 7.4, 3.8, 5.4, 7.0, 5.0))})}},
    # A crown of light above the head: motes of light circling a soft halo.
    'SanctifiedParagonCrown': {'model': 'PAL_SanctifiedParagonCrown', 'effect': 7155, 'shape': 'mist',
                               'textures': ('MONK_Flare', 'MONK_Glow'),
                               'options': {'layers': (
                                   {'texture': 1, 'tint': GOLD, 'alpha': 0.5, 'pulse': 1.6,
                                    'cards': ((0.0, 0.45, 0.9),)},
                                   {'texture': 0, 'tint': WHITE_GOLD, 'alpha': 1.0, 'turn': 2.5, 'pulse': 0,
                                    'cards': tuple((0.38, 0.45, 0.16) for _ in range(8))},
                                   {'texture': 0, 'tint': GOLD, 'alpha': 0.9, 'turn': -3.5, 'pulse': 0.8,
                                    'cards': tuple((0.24, 0.55, 0.12) for _ in range(5))})}},
    # Waking (90749, cast as the aura turns on): a golden shockwave rolling out and a flash of light.
    'SanctifiedParagonAwaken': {'model': 'PAL_SanctifiedParagonAwaken', 'effect': 7156, 'shape': 'burst',
                                'textures': (f'{PARAGON}/shockwavewater1extreme', 'MONK_Glow', None),
                                'options': {'size': 16.0, 'ring_tint': GOLD, 'ring_blend': 4, 'flash_sheet': False,
                                            'flash_tint': WHITE_GOLD,
                                            'ring': {'scale': ([0, 250, 650], [1.0, 16.0, 17.0]),
                                                     'fade': ([0, 150, 550, 800], [0x7FFF, 0x7FFF, 0, 0])}}},
    # Seat of the Paragon: a larger holy sigil beneath the Hero while the bonus is on.
    'SeatOfTheParagon': {'model': 'PAL_SeatOfTheParagon', 'effect': 7153, 'shape': 'zone',
                         'textures': (f'{PARAGON}/holy_sigil', f'{PARAGON}/holy_inscriptions'),
                         'options': {'layers': (
                             {'texture': 0, 'size': 12.0, 'height': 0.12, 'tint': GOLD, 'alpha': 0.6, 'turn': 24.0,
                              'blend': 4},
                             {'texture': 1, 'size': 12.5, 'height': 0.16, 'tint': PALE_GOLD, 'alpha': 0.7,
                              'blend': 4})}},
    # Hallowed Footsteps: fading golden inscriptions where the Hero walked.
    'HallowedFootsteps': {'model': 'PAL_HallowedFootsteps', 'effect': 7154, 'shape': 'zone',
                          'textures': (f'{PARAGON}/holy_glyph',),
                          'options': {'layers': (
                              {'texture': 0, 'size': 8.0, 'height': 0.1, 'tint': GOLD, 'alpha': 0.5, 'turn': 18.0,
                               'blend': 4},)}},
}
# The Rune of the Stormbound Conduit (90750 on; runes.cpp's Stormbound Conduit section).
STORM_RUNE = buff(90750, 'Rune of the Stormbound Conduit', STORM_ICON,
                  'Lightning Bolt arcs to 2 enemies within 10 yards of its target for 30% of its damage. Your '
                  'Lightning Shield\'s retaliations arc to 1 nearby enemy for 50% of theirs.',
                  DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)})
# Its procs: Lightning Bolt and Chain Lightning land as harmful spells; the script picks them out.
STORM_RUNE['set'] = {**STORM_RUNE['set'], 'ProcFlags': PROC_DONE_MAGIC_SPELL_NEGATIVE, 'ProcChance': 101}
SPELLS += [
    STORM_RUNE,
    {
        'id': 90751, 'copy': POWER_WORD_SHIELD, 'name': 'Static Ward', 'icon': 'Spell_Nature_LightningOverload',
        'description': 'Absorbs damage.', 'tooltip': 'Absorbs $s1 damage.',
        'set': {**FREE, 'SpellFamilyName': 0, 'SpellFamilyFlags0': 0, 'SpellFamilyFlags1': 0, 'SpellFamilyFlags2': 0,
                'RangeIndex': 1, 'DurationIndex': DURATION_6S, 'SchoolMask': NATURE, 'SpellVisual': VISUAL_CRACKLE},
        'effects': {1: {'Effect': 6, 'Aura': AURA_SCHOOL_ABSORB, 'BasePoints': 0, 'DieSides': 1,
                        'RealPointsPerLevel': 0.0, 'MiscValue': ALL_SCHOOLS, 'TargetA': 1, 'TargetB': 0}},
        'clear': [2, 3],
    },
    {
        'id': 90752, 'copy': LIGHTNING_BOLT, 'name': 'Stormbound Arc', 'icon': 'Spell_Nature_ChainLightning',
        'description': 'A secondary arc of lightning.', 'tooltip': '',
        'set': {**HARMFUL, 'Speed': 0.0, 'SchoolMask': NATURE, 'SpellVisual': VISUAL_ARC_STRIKE,
                'AttributesEx3': SPELL_ATTR3_ALWAYS_HIT | SPELL_ATTR3_IGNORE_CASTER_MODIFIERS,
                'AttributesEx4': SPELL_ATTR4_NO_PARTIAL_IMMUNITY | SPELL_ATTR4_CANNOT_BE_RESISTED},
        'effects': {1: {'Effect': EFFECT_SCHOOL_DAMAGE, 'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0,
                        'ChainTarget': 0, 'TargetA': TARGET_UNIT_TARGET_ENEMY, 'TargetB': 0}},
        'clear': [2, 3],
    },
    {
        'id': 90753, 'copy': CHAIN_LIGHTNING, 'name': 'Stormbound Arc', 'icon': 'Spell_Nature_ChainLightning',
        'description': '', 'tooltip': '',
        'set': {**FREE, 'SpellFamilyName': 0, 'SpellFamilyFlags0': 0, 'SpellFamilyFlags1': 0,
                'SpellFamilyFlags2': 0, 'RangeIndex': RANGE_VISION, 'Speed': 0.0, 'ProcFlags': 0,
                'Attributes': SPELL_ATTR0_ALLOW_CAST_WHILE_DEAD},
        'effects': {1: {'Effect': EFFECT_DUMMY, 'Aura': 0, 'BasePoints': 0, 'DieSides': 1,
                        'RealPointsPerLevel': 0.0, 'ChainTarget': 0, 'TargetA': TARGET_UNIT_TARGET_ANY,
                        'TargetB': 0}},
        'clear': [2, 3],
    },
    {
        'id': 90754, 'copy': LIGHTNING_SHIELD_DAMAGE, 'name': 'Lightning Shield',
        'icon': 'Spell_Nature_LightningShield',
        'description': 'A Lightning Shield retaliation.', 'tooltip': '',
        'set': {**HARMFUL, 'Speed': 0.0, 'SchoolMask': NATURE, 'SpellVisual': VISUAL_SKY_STRIKE},
        'effects': {1: {'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0, 'ChainTarget': 0,
                        'TargetA': TARGET_UNIT_TARGET_ENEMY, 'TargetB': 0}},
        'clear': [2, 3],
    },
]
STORM_MODIFIERS = [
    ('Forked Tempest', 'Closed Circuit', 'Faraday Field'),
    ('Live Wire', 'Storm Within', 'Distant Conductor'),
    ('Chain Surge', 'Static Ward', 'Overcharge'),
]
SPELLS += [buff(90760 + slot * 3 + choice, name, STORM_ICON, '', DURATION_INFINITE,
                {1: aura(AURA_DUMMY, 1, 0)}, hidden=True)
           for slot, names in enumerate(STORM_MODIFIERS) for choice, name in enumerate(names)]

# The Rune of Echoing Thunder (90770 on; runes.cpp's Echoing Thunder section).
THUNDER = 'WAR_EchoingThunder'
STORM_BLUE, PALE_BLUE = (0.45, 0.7, 1.0), (0.75, 0.88, 1.0)
SPELLS += [
    buff(90770, 'Rune of Echoing Thunder', THUNDER_ICON,
         'Thunder Clap charges you with thunder. Your next Shield Slam releases a Thunder Echo: 30% of its damage '
         'to up to 5 enemies within 6 yards of its target.',
         DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    {
        **buff(90772, 'Thunder Charge', 'Spell_Nature_ThunderClap',
               'Charged: your next Shield Slam releases a Thunder Echo.', DURATION_INFINITE,
               {1: aura(AURA_DUMMY, 1, 0)}),
        'visual': 'ThunderfieldMarch',
    },
    {
        'id': 90774, 'copy': LIGHTNING_BOLT, 'name': 'Thunder Echo', 'icon': 'Spell_Nature_ThunderClap',
        'description': 'A shockwave of thunder.', 'tooltip': '',
        'set': {**HARMFUL, 'Speed': 0.0, 'SchoolMask': NATURE, 'SpellVisual': VISUAL_THUNDER_HIT,
                'AttributesEx2': SPELL_ATTR2_CANT_CRIT,
                'AttributesEx3': SPELL_ATTR3_ALWAYS_HIT | SPELL_ATTR3_IGNORE_CASTER_MODIFIERS,
                'AttributesEx4': SPELL_ATTR4_NO_PARTIAL_IMMUNITY | SPELL_ATTR4_CANNOT_BE_RESISTED},
        'effects': {1: {'Effect': EFFECT_SCHOOL_DAMAGE, 'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0,
                        'ChainTarget': 0, 'TargetA': TARGET_UNIT_TARGET_ENEMY, 'TargetB': 0}},
        'clear': [2, 3],
    },
    {
        'id': 90775, 'copy': SELF_TEMPLATE, 'name': 'Thunder Echo', 'icon': 'Spell_Nature_ThunderClap',
        'description': '', 'tooltip': '',
        'set': {**HIDDEN, 'Attributes': SPELL_ATTR0_ALLOW_CAST_WHILE_DEAD, 'SpellVisual': VISUAL_THUNDER_BURST,
                'DurationIndex': 0},
        'effects': {1: {'Effect': EFFECT_DUMMY, 'Aura': 0, 'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0,
                        'TargetA': 1, 'TargetB': 0}},
        'clear': [2, 3],
    },
    {
        'id': 90776, 'copy': POWER_WORD_SHIELD, 'name': 'Sheltering Reverberation',
        'icon': 'Ability_Warrior_ShieldWall',
        'description': 'Absorbs damage.', 'tooltip': 'Absorbs $s1 damage.',
        'set': {**FREE, 'SpellFamilyName': 0, 'SpellFamilyFlags0': 0, 'SpellFamilyFlags1': 0, 'SpellFamilyFlags2': 0,
                'RangeIndex': 1, 'DurationIndex': DURATION_3S, 'SchoolMask': 1},
        'effects': {1: {'Effect': 6, 'Aura': AURA_SCHOOL_ABSORB, 'BasePoints': 0, 'DieSides': 1,
                        'RealPointsPerLevel': 0.0, 'MiscValue': ALL_SCHOOLS, 'TargetA': 1, 'TargetB': 0}},
        'clear': [2, 3],
    },
    {
        'id': 90777, 'copy': CRIPPLING_POISON, 'name': 'Deafening Impact', 'icon': 'Spell_Nature_ThunderClap',
        'description': 'Movement speed reduced by 30%.', 'tooltip': 'Movement speed reduced by $s1%.',
        'set': {**HARMFUL, 'DurationIndex': DURATION_6S_INDEX, 'SchoolMask': NATURE},
        'effects': {1: {**aura(AURA_MOD_DECREASE_SPEED, -30, 0), 'TargetA': TARGET_UNIT_TARGET_ENEMY}},
        'clear': [2, 3],
    },
]
THUNDER_MODIFIERS = [
    ('Double Charge', 'Lingering Charge', 'Commanding Thunder'),
    ('Focused Peal', 'Distant Rumble', 'Deafening Impact'),
    ('Sheltering Reverberation', 'Battle Rhythm', 'Rolling Thunder'),
]
SPELLS += [buff(90780 + slot * 3 + choice, name, THUNDER_ICON, '', DURATION_INFINITE,
                {1: aura(AURA_DUMMY, 1, 0)}, hidden=True)
           for slot, names in enumerate(THUNDER_MODIFIERS) for choice, name in enumerate(names)]

# The Rune of the Wellspring (90790 on) and of the Wandering Mist (90810 on; runes.cpp's sections of theirs).
HEAL_SET = {**FREE, 'SpellFamilyName': 0, 'SpellFamilyFlags0': 0, 'SpellFamilyFlags1': 0, 'SpellFamilyFlags2': 0,
            'RangeIndex': RANGE_VISION, 'SchoolMask': NATURE, 'AttributesEx2': SPELL_ATTR2_CANT_CRIT,
            'AttributesEx3': SPELL_ATTR3_IGNORE_CASTER_MODIFIERS, 'ProcFlags': 0}


def periodic_heal(spell_id, name, icon, text, duration, visual=None):
    """A heal over time the script casts (its tick set by the script), every sec."""
    spell = {'id': spell_id, 'copy': LESSER_HEAL, 'name': name, 'icon': icon, 'description': text,
             'tooltip': 'Healing every 1 sec.', 'set': {**HEAL_SET, 'DurationIndex': duration},
             'effects': {1: {'Effect': 6, 'Aura': AURA_PERIODIC_HEAL, 'Amplitude': 1000, 'BasePoints': 0,
                             'DieSides': 1, 'RealPointsPerLevel': 0.0, 'TargetA': TARGET_UNIT_TARGET_ALLY,
                             'TargetB': 0}},
             'clear': [2, 3]}
    if visual:
        spell['visual'] = visual
    return spell


def absorb(spell_id, name, icon, duration):
    return {'id': spell_id, 'copy': POWER_WORD_SHIELD, 'name': name, 'icon': icon, 'description': 'Absorbs damage.',
            'tooltip': 'Absorbs $s1 damage.', 'set': {**HEAL_SET, 'DurationIndex': duration},
            'effects': {1: {'Effect': 6, 'Aura': AURA_SCHOOL_ABSORB, 'BasePoints': 0, 'DieSides': 1,
                            'RealPointsPerLevel': 0.0, 'MiscValue': ALL_SCHOOLS, 'TargetA': TARGET_UNIT_TARGET_ALLY,
                            'TargetB': 0}},
            'clear': [2, 3]}


SPELLS += [
    buff(90790, 'Rune of the Wellspring', WELLSPRING_ICON,
         'Chain Heal jumps once more from its last target, to the most injured ally nearby, for 40% of its first '
         'heal. It heals the wearer of your Earth Shield for 20% more.',
         DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    {
        'id': 90792, 'copy': LESSER_HEAL, 'name': 'Wellspring', 'icon': 'Spell_Nature_HealingWaveGreater',
        'description': 'A current of the Wellspring.', 'tooltip': '',
        'set': {**HEAL_SET, 'SpellVisual': 3659},
        'effects': {1: {'Effect': EFFECT_HEAL, 'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0,
                        'TargetA': TARGET_UNIT_TARGET_ALLY, 'TargetB': 0}},
        'clear': [2, 3],
    },
    absorb(90793, 'Tidal Shelter', 'Spell_Frost_SummonWaterElemental', DURATION_4S),
    periodic_heal(90794, 'Renewing Current', 'Spell_Nature_Rejuvenation', 'Healing over time.', DURATION_4S),
    {
        'id': 90795, 'copy': CHAIN_HEAL, 'name': 'Wellspring', 'icon': 'Spell_Nature_HealingWaveGreater',
        'description': '', 'tooltip': '',
        'set': {**FREE, 'SpellFamilyName': 0, 'SpellFamilyFlags0': 0, 'SpellFamilyFlags1': 0,
                'SpellFamilyFlags2': 0, 'RangeIndex': RANGE_VISION, 'Speed': 0.0, 'ProcFlags': 0,
                'Attributes': SPELL_ATTR0_ALLOW_CAST_WHILE_DEAD},
        'effects': {1: {'Effect': EFFECT_DUMMY, 'Aura': 0, 'BasePoints': 0, 'DieSides': 1,
                        'RealPointsPerLevel': 0.0, 'ChainTarget': 0, 'TargetA': TARGET_UNIT_TARGET_ANY,
                        'TargetB': 0}},
        'clear': [2, 3],
    },
    buff(90810, 'Rune of the Wandering Mist', MIST_ICON,
         'Each Soothing Mist tick also sends a Wandering Mist to your most injured ally within 20 yards of its target, '
         'healing for 25% of the tick. Enveloping Mist and Vivify on your Soothing Mist target are cast at once, '
         'without breaking the channel.',
         DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    {
        'id': 90811, 'copy': LESSER_HEAL, 'name': 'Wandering Mist', 'icon': 'Ability_Monk_SoothingMists',
        'description': 'A ribbon of jade mist.', 'tooltip': '', 'visual': 'VivifySplash',
        'set': {**HEAL_SET},
        'effects': {1: {'Effect': EFFECT_HEAL, 'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0,
                        'TargetA': TARGET_UNIT_TARGET_ALLY, 'TargetB': 0}},
        'clear': [2, 3],
    },
    periodic_heal(90812, 'Enveloping Mist', 'Spell_Monk_EnvelopingMist', 'A lesser Enveloping Mist.', DURATION_6S,
                  'EnvelopingMist'),
    periodic_heal(90813, 'Lingering Mist', 'Ability_Monk_SoothingMists', 'A lingering mist.', DURATION_4S),
    absorb(90814, 'Jade Shelter', 'Ability_Monk_SoothingMists', DURATION_4S),
]
WELLSPRING_MODIFIERS = [
    ('Split Tributary', 'Deep Current', 'Concentrated Current'),
    ('Flowing Waters', 'Bedrock', 'Renewed Shield'),
    ('Tidal Shelter', 'Renewing Current', 'Long Reach'),
]
MIST_MODIFIERS = [
    ('Flowing Currents', 'Drifting Mist', 'Jade Shelter'),
    ('Comforting Veil', 'Vivacious Flow', 'Peaceful Focus'),
    ('Effortless Breathing', 'Lingering Mist', 'Deep Breath'),
]
SPELLS += [buff(first + slot * 3 + choice, name, icon, '', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)},
                hidden=True)
           for first, icon, modifiers in ((90800, WELLSPRING_ICON, WELLSPRING_MODIFIERS),
                                          (90820, MIST_ICON, MIST_MODIFIERS))
           for slot, names in enumerate(modifiers) for choice, name in enumerate(names)]

# ---- The five runes after them (runes.cpp's sections of theirs): 90830 the Threefold Grave, 90860 Unbroken
# Onslaught, 90890 the Dawnbound Crusader, 90920 the Hollow Congregation, 90950 the Shattered Sun; each one's
# modifiers' markers 20 on from its first.
GRAVE_ICON = 'Ability_Rune_ThreefoldGrave'
ONSLAUGHT_ICON = 'Ability_Rune_UnbrokenOnslaught'
DAWN_ICON = 'Ability_Rune_DawnboundCrusader'
HOLLOW_ICON = 'Ability_Rune_HollowCongregation'
SUN_ICON = 'Ability_Rune_ShatteredSun'
PHYSICAL, FIRE, FROST, SHADOW, FROSTFIRE = 0x1, 0x4, 0x10, 0x20, 0x14
AURA_PERIODIC_DAMAGE, AURA_MOD_ROOT = 3, 26
TARGET_UNIT_CASTER = 1
DURATION_8S, DURATION_12S, DURATION_15S = 31, 29, 8
RANGE_40 = 5
MIND_FLAY, SHADOW_BOLT, HOLY_SHOCK_DAMAGE = 15407, 686, 25912
VISUAL_FROST_NOVA, VISUAL_BLAST_WAVE, VISUAL_HOLY_NOVA, VISUAL_MIND_BLAST = 17, 963, 3643, 3057
VISUAL_SHADOW_BOLT, VISUAL_HOLY_SHOCK, VISUAL_HOLY_SHOCK_HEAL, VISUAL_BEACON = 64, 128, 135, 11876
NO_FAMILY = {'SpellFamilyName': 0, 'SpellFamilyFlags0': 0, 'SpellFamilyFlags1': 0, 'SpellFamilyFlags2': 0}
SCRIPTED = {'AttributesEx3': SPELL_ATTR3_IGNORE_CASTER_MODIFIERS}


def action(spell_id, name, icon, text, cooldown, target=TARGET_UNIT_CASTER, range_index=1, visual=0):
    """An action a rune teaches while it is bound: instant, free, off the global cooldown; the script does it."""
    return {'id': spell_id, 'copy': SELF_TEMPLATE, 'name': name, 'icon': icon, 'description': text,
            'tooltip': '', 'set': {**FREE, **NO_FAMILY, 'RecoveryTime': cooldown, 'RangeIndex': range_index,
                                   'DurationIndex': 0, 'SpellVisual': visual, 'ProcFlags': 0},
            'effects': {1: {'Effect': EFFECT_DUMMY, 'Aura': 0, 'BasePoints': 0, 'DieSides': 1,
                            'RealPointsPerLevel': 0.0, 'TargetA': target, 'TargetB': 0}},
            'clear': [2, 3]}


def strike(spell_id, name, icon, school, visual=0, text='', key=None, modifiers=False):
    """Damage on an enemy (its amount set by the script: a share of a hit, the Hero's bonuses already in it; or,
    modifiers, raw power the Hero's damage bonuses and talents then raise as the core applies them)."""
    spell = {'id': spell_id, 'copy': LIGHTNING_BOLT, 'name': name, 'icon': icon, 'description': text,
             'tooltip': '', 'set': {**HARMFUL, 'Speed': 0.0, 'SchoolMask': school, 'SpellVisual': visual,
                                    'AttributesEx3': SPELL_ATTR3_ALWAYS_HIT
                                    | (0 if modifiers else SPELL_ATTR3_IGNORE_CASTER_MODIFIERS),
                                    'AttributesEx4': SPELL_ATTR4_NO_PARTIAL_IMMUNITY | SPELL_ATTR4_CANNOT_BE_RESISTED},
             'effects': {1: {'Effect': EFFECT_SCHOOL_DAMAGE, 'BasePoints': 0, 'DieSides': 1,
                             'RealPointsPerLevel': 0.0, 'ChainTarget': 0, 'TargetA': TARGET_UNIT_TARGET_ENEMY,
                             'TargetB': 0}},
             'clear': [2, 3]}
    if key:
        spell['visual'] = key
    return spell


def harmful(spell_id, name, icon, text, duration, effect, school, key=None):
    """An aura on an enemy: effect (an aura() dict), its tick set by the script for damage over time."""
    spell = {'id': spell_id, 'copy': CRIPPLING_POISON, 'name': name, 'icon': icon, 'description': text,
             'tooltip': text, 'set': {**HARMFUL, 'DurationIndex': duration, 'SchoolMask': school, **SCRIPTED},
             'effects': {1: {**effect, 'TargetA': TARGET_UNIT_TARGET_ENEMY}}, 'clear': [2, 3]}
    if key:
        spell['visual'] = key
    return spell


def dot(spell_id, name, icon, school, duration, text):
    """Damage over time: a periodic dummy each second, its damage the script's own spell (spell_titan_rune_dot)."""
    return harmful(spell_id, name, icon, text, duration,
                   {**aura(AURA_PERIODIC_DUMMY, 1, 0), 'Amplitude': 1000}, school)


def ground(spell_id, name, icon, key, duration):
    """A patch of ground the script lays (a dynamic object: its look only)."""
    return {'id': spell_id, 'copy': CONSECRATION, 'name': name, 'icon': icon, 'description': '', 'tooltip': '',
            'visual': key, 'set': {**FREE, **NO_FAMILY, 'DurationIndex': duration, 'RangeIndex': RANGE_VISION},
            'effects': {1: {'Effect': EFFECT_PERSISTENT_AREA_AURA, 'Aura': AURA_DUMMY, 'BasePoints': 0, 'DieSides': 1,
                            'RealPointsPerLevel': 0.0, 'Amplitude': 0, 'TargetA': TARGET_DEST_DEST,
                            'TargetB': TARGET_UNIT_DEST_AREA_ENEMY, 'RadiusIndex': RADIUS_8}}}


def look(spell_id, name, icon, visual, target=1):
    """A look only: cast by a unit on itself (target 1) or at another (TARGET_UNIT_TARGET_ANY), no effect."""
    return {'id': spell_id, 'copy': SELF_TEMPLATE, 'name': name, 'icon': icon, 'description': '', 'tooltip': '',
            'set': {**HIDDEN, 'Attributes': SPELL_ATTR0_ALLOW_CAST_WHILE_DEAD, 'SpellVisual': visual,
                    'DurationIndex': 0, 'RangeIndex': RANGE_VISION},
            'effects': {1: {'Effect': EFFECT_DUMMY, 'Aura': 0, 'BasePoints': 0, 'DieSides': 1,
                            'RealPointsPerLevel': 0.0, 'TargetA': target, 'TargetB': 0}},
            'clear': [2, 3]}


def heal(spell_id, name, icon, text):
    return {'id': spell_id, 'copy': LESSER_HEAL, 'name': name, 'icon': icon, 'description': text, 'tooltip': '',
            'set': {**HEAL_SET, 'SchoolMask': HOLY, 'AttributesEx2': 0},
            'effects': {1: {'Effect': EFFECT_HEAL, 'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0,
                            'TargetA': TARGET_UNIT_TARGET_ALLY, 'TargetB': 0}},
            'clear': [2, 3]}


def shown(spell_id, name, icon, text, duration, key=None, effects=None):
    spell = buff(spell_id, name, icon, text, duration, effects or {1: aura(AURA_DUMMY, 1, 0)})
    if key:
        spell['visual'] = key
    return spell


ONSLAUGHT_RUNE = buff(90860, 'Rune of the Unbroken Onslaught', ONSLAUGHT_ICON,
                      'Mortal Strike and Bloodthirst cut a Weapon Wake through their target for 6 sec. Your next '
                      'other melee ability rips it open: 60% of a weapon swing to every enemy in it, its first '
                      'target 25% more.',
                      DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)})
# Its procs: every melee ability that hits; the script picks the ones that reopen the wake.
ONSLAUGHT_RUNE['set'] = {**ONSLAUGHT_RUNE['set'], 'ProcFlags': PROC_DONE_MELEE_SPELL, 'ProcChance': 101}
SPELLS += [
    buff(90830, 'Rune of the Threefold Grave', GRAVE_ICON,
         'Icy Touch lays a Frozen Grave beneath its target, Plague Strike a Plagued Grave, Blood Strike a Crimson '
         'Grave beneath you: one of each, 8 sec. Enemies in two or more take 20% more from your Graves. Blood Strike '
         'in your Crimson Grave grants a shield.', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    harmful(90831, 'Shared Burial', 'Spell_Shadow_DeathPact', 'Taking more damage from the Threefold Grave.',
            DURATION_2S, aura(AURA_DUMMY, 1, 0), SHADOW),
    strike(90832, 'Grave Pestilence', 'Spell_Shadow_CreepingPlague', SHADOW, modifiers=True),
    strike(90833, 'Bloodletting', 'Ability_Gouge', PHYSICAL, modifiers=True),
    strike(90834, 'Hollow Flay', 'Spell_Shadow_SiphonMana', SHADOW),
    strike(90838, 'Frozen Grave', 'Spell_Deathknight_IceTouch', FROST, key='GraveRimeHit', modifiers=True),
    harmful(90839, 'Frozen Grave', 'Spell_Frost_ChillingBlast', 'Movement speed reduced by $s1%.', DURATION_6S,
            aura(AURA_MOD_DECREASE_SPEED, -30, 0), FROST),
    strike(90840, 'Plagued Grave', 'Spell_Shadow_PlagueCloud', SHADOW, key='GraveBlightHit', modifiers=True),
    dot(90841, 'Grave Pestilence', 'Spell_Shadow_CreepingPlague', SHADOW, DURATION_6S, 'Shadow damage every 1 sec.'),
    strike(90842, 'Crimson Grave', 'Spell_Deathknight_BloodBoil', SHADOW, key='GraveGoreHit', modifiers=True),
    {**absorb(90843, 'Crimson Ward', 'Spell_Deathknight_DeathStrike', DURATION_10S),
     'set': {**HEAL_SET, 'DurationIndex': DURATION_10S, 'SchoolMask': SHADOW}},
    ground(90845, 'Frozen Grave', 'Spell_Deathknight_IceTouch', 'GraveFrozenLine', DURATION_20S),
    ground(90846, 'Plagued Grave', 'Spell_Shadow_PlagueCloud', 'GraveDiseasedPatch', DURATION_20S),
    ground(90847, 'Crimson Grave', 'Spell_Deathknight_BloodBoil', 'GraveBloodCircle', DURATION_20S),
    shown(90848, 'Crimson Grave', 'Spell_Deathknight_BloodBoil', 'Your Crimson Grave follows you.', DURATION_20S,
          key='GraveBloodMarch'),

    ONSLAUGHT_RUNE,
    strike(90861, 'Weapon Wake', 'Ability_Warrior_Cleave', PHYSICAL, key='WakeHit', modifiers=True),
    dot(90862, 'Bloodletting', 'Ability_Gouge', PHYSICAL, DURATION_6S, 'Bleeding every 1 sec.'),
    ground(90863, 'Weapon Wake', 'Ability_Warrior_Cleave', 'OnslaughtWake', DURATION_10S),
    strike(90864, 'Aftershock', 'Ability_Warrior_Shockwave', PHYSICAL, VISUAL_THUNDER_HIT, modifiers=True),
    {**heal(90865, 'Second Wind', 'Ability_Hunter_Harass', 'The onslaught carries you on.'),
     'set': {**HEAL_SET, 'SchoolMask': PHYSICAL, 'AttributesEx2': 0, 'SpellVisual': 0}},

    buff(90890, 'Rune of the Dawnbound Crusader', DAWN_ICON,
         'Crusader Strike and Holy Shock send a Dawn Echo a moment later: half of their damage again, or half of '
         'Holy Shock\'s healing on your most injured ally nearby.',
         DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    action(90891, 'Dawn Vow', 'Ability_Paladin_BeaconofLight',
           'Swear your Dawn Vow to an ally, or yourself: your Dawn Echoes rise from them.', 10000,
           target=TARGET_UNIT_TARGET_ALLY, range_index=RANGE_40, visual=VISUAL_BEACON),
    shown(90892, 'Dawn Vow', 'Ability_Paladin_BeaconofLight', 'A Dawnbound Crusader\'s echoes rise from you.',
          DURATION_INFINITE, key='DawnVow'),
    heal(90894, 'Dawn Echo', 'Spell_Holy_SearingLight', 'An echo of the dawn.'),
    strike(90895, 'Dawn Echo', 'Spell_Holy_SearingLight', HOLY),
    look(90896, 'Dawn Echo', 'Spell_Holy_SearingLight', VISUAL_HOLY_SHOCK, TARGET_UNIT_TARGET_ANY),
    look(90897, 'Dawn Echo', 'Spell_Holy_SearingLight', VISUAL_HOLY_SHOCK_HEAL, TARGET_UNIT_TARGET_ANY),
    look(90898, 'Sunburst', 'Spell_Holy_HolyBolt', VISUAL_HOLY_NOVA),
    # Sacred Ground: Consecration's own ground.
    {key: value for key, value in ground(90899, 'Sacred Ground', 'Spell_Holy_InnerFire', None, DURATION_6S).items()
     if key != 'visual'},
    {'id': 90900, 'copy': POWER_WORD_SHIELD, 'name': 'Radiant Ward', 'icon': 'Spell_Holy_AuraMastery',
     'description': 'Absorbs damage.', 'tooltip': 'Absorbs $s1 damage.',
     'set': {**FREE, **NO_FAMILY, 'RangeIndex': RANGE_40, 'DurationIndex': DURATION_8S, 'SchoolMask': HOLY},
     'effects': {1: {'Effect': 6, 'Aura': AURA_SCHOOL_ABSORB, 'BasePoints': 0, 'DieSides': 1,
                     'RealPointsPerLevel': 0.0, 'MiscValue': ALL_SCHOOLS, 'TargetA': TARGET_UNIT_TARGET_ALLY,
                     'TargetB': 0}},
     'clear': [2, 3]},

    buff(90920, 'Rune of the Hollow Congregation', HOLLOW_ICON,
         'Mind Flay calls a Hollow Echo to your side for 10 sec (3 at most): it follows you and flays your target. '
         'Mind Blast turns every echo on its target in a short barrage.',
         DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    {**shown(90921, 'Hollow Echo', 'Spell_Shadow_Shadowform', '', DURATION_INFINITE, key='HollowEcho',
             effects={1: aura(AURA_MOD_THREAT, -100, ALL_SCHOOLS)}),
     'set': {**HIDDEN, 'DurationIndex': DURATION_INFINITE}},
    {
        'id': 90922, 'copy': MIND_FLAY, 'name': 'Hollow Flay', 'icon': 'Spell_Shadow_SiphonMana',
        'description': 'A hollow echo of Mind Flay.', 'tooltip': 'Shadow damage every 1 sec.',
        'set': {**FREE, **NO_FAMILY, 'RangeIndex': RANGE_VISION, 'SchoolMask': SHADOW, 'ProcFlags': 0,
                'InterruptFlags': 0, 'AuraInterruptFlags': 0, 'ChannelInterruptFlags': 0, **SCRIPTED},
        'effects': {1: {**aura(AURA_PERIODIC_DUMMY, 1, 0), 'Amplitude': 1000,
                        'TargetA': TARGET_UNIT_TARGET_ENEMY}},
        'clear': [2, 3],
    },
    # The barrage's bolt: Shadow Bolt's, flying.
    {**strike(90923, 'Hollow Barrage', 'Spell_Shadow_UnholyFrenzy', SHADOW, VISUAL_SHADOW_BOLT),
     'copy': SHADOW_BOLT},
    strike(90924, 'Accusation', 'Spell_Shadow_PsychicScream', SHADOW, VISUAL_MIND_BLAST),
    strike(90925, 'Last Word', 'Spell_Shadow_DemonicFortitude', SHADOW, VISUAL_MIND_BLAST),
    {**heal(90926, 'Shadow Mend', 'Spell_Shadow_ShadowMend', 'Your Hollow Echoes mend you.'),
     'set': {**HEAL_SET, 'SchoolMask': SHADOW, 'AttributesEx2': 0, 'SpellVisual': 0}},

    buff(90950, 'Rune of the Shattered Sun', SUN_ICON,
         'Your fire spells embed burning Sunshards in their target, up to 5. A frost spell shatters every shard in its '
         'target: a burst of 25% of its damage per shard to each enemy within 8 yards.',
         DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    {**harmful(90951, 'Sunshard', 'Ability_Mage_LivingBomb', 'Burning Sunshards: a frost spell shatters them.',
               DURATION_15S, aura(AURA_DUMMY, 1, 0), FIRE, key='Sunshard')},
    harmful(90952, 'Frozen Core', 'Ability_Mage_FrostFireBolt', 'Movement speed reduced by 40%.',
            DURATION_6S, aura(AURA_MOD_DECREASE_SPEED, -40, 0), FROSTFIRE, key='BrokenCore'),
    strike(90953, 'Shatter', 'Spell_Frost_FrostNova', FROSTFIRE),
    look(90954, 'Shatter', 'Spell_Frost_FrostNova', VISUAL_FROST_NOVA),
    strike(90955, 'Second Sunrise', 'Spell_Fire_SoulBurn', FROSTFIRE),
    look(90956, 'Second Sunrise', 'Spell_Fire_SoulBurn', VISUAL_BLAST_WAVE),
    ground(90957, 'Scalding Ground', 'Spell_Fire_Incinerate', 'ScaldingGround', DURATION_6S),
]
LEGEND_MODIFIERS = [
    (90850, GRAVE_ICON, [('Gathered Dead', 'March of the Dead', 'Far-Reaching Graves'),
                         ('Cruel Winter', 'Grave Pestilence', 'Crimson Bastion'),
                         ('Deep Burial', 'Spreading Death', 'Hallowed by Nothing')]),
    (90880, ONSLAUGHT_ICON, [("Executioner's Cut", 'Whirling Steel', 'Raking Passage'),
                             ('Overpowering Echo', 'Bloodletting', 'Aftershock'),
                             ('Relentless', 'Battlefield Claim', 'Second Wind')]),
    (90910, DAWN_ICON, [('Sunburst', 'Focused Dawn', 'Communion'),
                        ('Sacred Ground', 'Steadfast', "Judgment's Echo"),
                        ("Dawn's Mercy", 'Swift Dawn', 'Radiant Ward')]),
    (90940, HOLLOW_ICON, [('Gathering', 'Lasting Echoes', 'Last Word'),
                          ('Unison', 'Dissonance', 'Accusation'),
                          ('Fervor', 'Shadow Mend', 'Swift Command')]),
    (90970, SUN_ICON, [('Wildfire', 'Spreading Sun', 'Heavy Sun'),
                       ('Wide Burst', 'Scalding Ground', 'Chain Reaction'),
                       ('Implosion', 'Second Sunrise', 'Frozen Core')]),
]
next(spell for spell in SPELLS if spell['id'] == 90923)['set']['Speed'] = 20.0
next(spell for spell in SPELLS if spell['id'] == 90951)['set']['StackAmount'] = 8
SPELLS += [buff(first + slot * 3 + choice, name, icon, '', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)},
                hidden=True)
           for first, icon, modifiers in LEGEND_MODIFIERS
           for slot, names in enumerate(modifiers) for choice, name in enumerate(names)]

# Their looks on the ground (ground_zone): the scars (Echoing Thunder's cracks iced over, the monk's mist gone sickly
# green, the Illidari sigil ring in blood), the wake's gash (build_wake_art.py), Scalding Ground's frostfire, a shard
# anchored to the ground.
ICE, PLAGUE, BLOOD, WAKE_RED = (0.55, 0.85, 1.0), (0.4, 0.95, 0.2), (0.85, 0.05, 0.05), (1.0, 0.18, 0.1)
FROSTFIRE_TINT, SUN_GOLD = (0.75, 0.45, 1.0), (1.0, 0.6, 0.15)
CRACKS, CLOUDS = f'{THUNDER}/thunder_cracks', 'MONK_Mist_Clouds'
SIGIL, RING, GLYPH = 'DH_SigilOfChains_Sigil', 'DH_SigilOfChains_Ring', 'DH_SigilOfChains_Glyph'


# The Threefold Grave's grounds (mist() with flat layers: rune circles and cracks lying on the ground, animated
# flipbooks of smoke on it, motes rising off it; spells/RUNE_VFX).
VFX = 'RUNE_VFX'


def ring(radius, count, height, size, turned=0.0):
    """count spots evenly around a ring, each turned to face along it."""
    return tuple((radius * math.cos(2 * math.pi * k / count), radius * math.sin(2 * math.pi * k / count), height,
                  size, turned + 360.0 * k / count) for k in range(count))


def grave(model, effect, circle, inner, across, tint, deep, extra):
    """A Grave: a turning rune circle and an inner one turning the other way, its own life on top (extra)."""
    return {'model': model, 'effect': effect, 'shape': 'mist',
            'textures': (f'{VFX}/{circle}', f'{VFX}/{inner}') + tuple(f'{VFX}/{t}' for t, _ in extra),
            'options': {'layers': (
                {'texture': 0, 'flat': True, 'spots': ((0.0, 0.0, 0.08, across, 0.0),), 'tint': tint, 'alpha': 0.95,
                 'turn': 40.0, 'pulse': 0},
                {'texture': 1, 'flat': True, 'spots': ((0.0, 0.0, 0.06, across * 0.86, 0.0),), 'tint': deep,
                 'alpha': 0.7, 'turn': -70.0, 'pulse': 3.0},
            ) + tuple({**layer, 'texture': 2 + k} for k, (_, layer) in enumerate(extra))}}


FROST_TINT, FROST_DEEP = (0.55, 0.85, 1.0), (0.35, 0.6, 1.0)
PLAGUE_TINT, PLAGUE_DEEP = (0.45, 1.0, 0.25), (0.25, 0.7, 0.1)
BLOOD_TINT, BLOOD_DEEP = (1.0, 0.12, 0.08), (0.65, 0.02, 0.02)
FROZEN = grave('DK_GraveFrozen', 7159, 'MagicCircle4', 'Circle_Cracks_01', 10.5, FROST_TINT, FROST_DEEP, (
    ('ice_smoke_1_w_sheet', {'flat': True, 'spots': ring(2.6, 5, 0.12, 4.2), 'sheet': (4, 4), 'flip': 1.8,
                             'tint': FROST_TINT, 'alpha': 0.55, 'pulse': 0, 'turn': 25.0}),
    ('ice_pieces_2_white_sheet', {'spots': ring(3.6, 8, 0.3, 1.3), 'sheet': (4, 4), 'flip': 1.2,
                                  'tint': (0.8, 0.94, 1.0), 'alpha': 0.85, 'rise': (2.2, 2.4), 'pulse': 0}),
))
PLAGUED = grave('DK_GravePlagued', 7160, 'MagicCircle9', 'magic_debuff_circle', 10.5, PLAGUE_TINT, PLAGUE_DEEP, (
    ('generic_smoke_1_sheet_bw', {'flat': True, 'spots': ring(2.4, 6, 0.12, 4.0), 'sheet': (4, 4), 'flip': 2.2,
                                  'tint': PLAGUE_TINT, 'alpha': 0.5, 'pulse': 0, 'turn': -30.0}),
    ('poison_dot_1_sheet', {'spots': ring(3.0, 6, 0.4, 1.5), 'sheet': (4, 4), 'flip': 1.0, 'alpha': 0.9,
                            'rise': (1.2, 1.0), 'pulse': 0}),
    ('skullflip', {'spots': ring(1.8, 2, 0.6, 1.6), 'sheet': (4, 4), 'flip': 1.6, 'tint': PLAGUE_TINT,
                   'alpha': 0.55, 'rise': (2.4, 1.6), 'pulse': 0, 'turn': 6.0}),
))


def crimson(model, effect):
    return grave(model, effect, 'Circle_DragonLord_01', 'MagicCircle1', 12.5, BLOOD_TINT, BLOOD_DEEP, (
        ('blood_explosion_ground_sheet', {'flat': True, 'spots': ring(3.4, 4, 0.1, 3.6, 30.0), 'sheet': (2, 2),
                                          'tint': (0.7, 0.05, 0.05), 'alpha': 0.75, 'pulse': 0, 'blend': 2}),
        ('new_blood_1_sheet', {'spots': ring(4.2, 6, 0.5, 1.4), 'sheet': (4, 4), 'flip': 0.9, 'alpha': 0.9,
                               'pulse': 0, 'blend': 2}),
    ))


EFFECTS.update({
    'GraveFrozenLine': FROZEN,
    'GraveDiseasedPatch': PLAGUED,
    'GraveBloodCircle': crimson('DK_GraveCrimson', 7161),
    'GraveBloodMarch': crimson('DK_GraveCrimsonMarch', 7162),
})

# Echoing Thunder, the Unbroken Onslaught and the Shattered Sun, rebuilt on the same layered looks: grounds of
# turning rune rings and cracks with flipbooks lying on them and billboards over them; one-shot bursts and hits
# (fade: played once) for the moments that matter.
def burst(model, effect, layers, duration=900):
    return {'model': model, 'effect': effect, 'shape': 'mist', 'textures': tuple(f'{VFX}/{t}' for t, _ in layers),
            'options': {'layers': tuple({**layer, 'texture': k} for k, (_, layer) in enumerate(layers)),
                        'duration': duration, 'fade': (0, 300)}}


def looping(model, effect, layers):
    return {'model': model, 'effect': effect, 'shape': 'mist', 'textures': tuple(f'{VFX}/{t}' for t, _ in layers),
            'options': {'layers': tuple({**layer, 'texture': k} for k, (_, layer) in enumerate(layers))}}


def thunderfield(model, effect, across):
    k = across / 16.0
    return looping(model, effect, (
        ('Circle_Cracks_02', {'flat': True, 'spots': ((0.0, 0.0, 0.08, across, 0.0),), 'tint': STORM_BLUE,
                              'alpha': 0.9, 'turn': 60.0, 'pulse': 0}),
        ('Lightning_Circular_01', {'flat': True, 'spots': ((0.0, 0.0, 0.12, across * 0.94, 0.0),), 'tint': PALE_BLUE,
                                   'alpha': 0.85, 'turn': -18.0, 'pulse': 0.7}),
        ('lightning_bolt_4x4', {'spots': ring(5.2 * k, 4, 1.7 * k, 3.4 * k), 'sheet': (4, 4), 'flip': 0.8,
                                'max_size': 1024, 'tint': (0.82, 0.92, 1.0), 'alpha': 0.95, 'pulse': 0}),
        ('Cast_Shockwave_01', {'flat': True, 'spots': ((0.0, 0.0, 0.1, across * 0.3, 0.0),), 'tint': PALE_BLUE,
                               'alpha': 0.55, 'scale': [(0.0, 0.3), (1.0, 3.2)], 'scale_beat': 1.6, 'pulse': 1.6}),
    ))


SLASH_RED, EMBER = (1.0, 0.16, 0.1), (1.0, 0.55, 0.2)
SUN_RAYS, SUN_ORANGE = (1.0, 0.72, 0.28), (1.0, 0.45, 0.1)
WHITE_HOT, ICE_GLINT = (1.0, 0.93, 0.75), (0.65, 0.88, 1.0)
EFFECTS.update({
    'Thunderfield': thunderfield('WAR_Thunderfield', 7157, 16.0),
    'ThunderfieldMarch': thunderfield('WAR_ThunderfieldMarch', 7158, 6.0),
    # A Thunder Echo landing: a bolt from the sky and a ring of sparks at the feet.
    'ThunderEchoHit': burst('WAR_ThunderEchoHit', 7166, (
        ('lightning_bolt_4x4', {'spots': ((0.0, 0.0, 2.3, 4.6, 0.0),), 'sheet': (4, 4), 'flip': 0.7,
                                'max_size': 1024, 'tint': (0.8, 0.92, 1.0), 'alpha': 1.0, 'pulse': 0}),
        ('sonic_impact_white_sheet', {'flat': True, 'spots': ((0.0, 0.0, 0.1, 3.0, 0.0),), 'sheet': (4, 2),
                                      'flip': 0.6, 'tint': PALE_BLUE, 'alpha': 0.95, 'pulse': 0}),
    ), 700),
    # Shield Slam's burst about its target (Reverberating Impact, Focused Peal): a ring rushing out and bolts.
    'ThunderBurst': burst('WAR_ThunderBurst', 7167, (
        ('Cast_Shockwave_01', {'flat': True, 'spots': ((0.0, 0.0, 0.1, 2.0, 0.0),), 'tint': PALE_BLUE, 'alpha': 0.9,
                               'scale': [(0.0, 0.3), (0.6, 5.5), (1.0, 6.5)], 'pulse': 0}),
        ('Lightning_Circular_01', {'flat': True, 'spots': ((0.0, 0.0, 0.14, 9.0, 0.0),), 'tint': STORM_BLUE,
                                   'alpha': 0.9, 'scale': [(0.0, 0.2), (0.5, 1.0), (1.0, 1.1)], 'pulse': 0}),
        ('lightning_bolt_4x4', {'spots': ring(2.5, 3, 1.4, 3.0), 'sheet': (4, 4), 'flip': 0.8,
                                'max_size': 1024, 'tint': (0.8, 0.92, 1.0), 'alpha': 1.0, 'pulse': 0}),
    )),
    # The Weapon Wake: crossed blade cuts glowing red on the ground, a soft red glow under them.
    'OnslaughtWake': looping('WAR_OnslaughtWake', 7163, (
        ('preparation_circle_a', {'flat': True, 'spots': ((0.0, 0.0, 0.06, 3.6, 0.0),), 'tint': (0.8, 0.05, 0.03),
                                  'alpha': 0.6, 'pulse': 1.2}),
        ('DaggerSlashSpriteSheet', {'flat': True, 'spots': ((0.0, 0.0, 0.1, 4.6, 35.0), (0.0, 0.0, 0.12, 4.0, -55.0),
                                                            (0.0, 0.0, 0.14, 3.4, 100.0)),
                                    'sheet': (4, 4), 'tint': (1.0, 0.4, 0.3), 'alpha': 1.0, 'pulse': 0.9}),
    )),
    # Reopening the wake: red slashes and sparks on each enemy along it.
    'WakeSlash': burst('WAR_WakeSlash', 7168, (
        ('new_cleave_a_1_sheet', {'spots': ((0.0, 0.0, 0.0, 3.6, 20.0), (0.0, 0.0, 0.2, 3.0, -40.0)), 'sheet': (4, 2),
                                  'flip': 0.45, 'alpha': 1.0, 'pulse': 0, 'blend': 2}),
        ('impact_new_1_a_new_sheet', {'spots': ((0.0, 0.0, 0.1, 2.6, 0.0),), 'sheet': (3, 2), 'flip': 0.4,
                                      'tint': SLASH_RED, 'alpha': 1.0, 'pulse': 0}),
    ), 600),
    # Aftershock: the ground jolting where the wake was.
    'WakeAftershock': burst('WAR_WakeAftershock', 7169, (
        ('sonic_impact_white_sheet', {'flat': True, 'spots': ((0.0, 0.0, 0.1, 3.6, 0.0),), 'sheet': (4, 2),
                                      'flip': 0.5, 'tint': EMBER, 'alpha': 1.0, 'pulse': 0}),
        ('Cast_Shockwave_01', {'flat': True, 'spots': ((0.0, 0.0, 0.08, 1.5, 0.0),), 'tint': SLASH_RED,
                               'alpha': 0.8, 'scale': [(0.0, 0.4), (1.0, 3.5)], 'pulse': 0}),
    ), 600),
    # A Sunshard: a small sun burning in the enemy, rays turning about a white-hot glint.
    'SunshardGlow': looping('MAGE_SunshardGlow', 7170, (
        ('sun_rays', {'spots': ((0.0, 0.0, 0.0, 3.0, 0.0),), 'tint': SUN_RAYS, 'alpha': 0.85, 'pulse': 1.2}),
        ('sun_glint', {'spots': ((0.0, 0.0, 0.0, 1.5, 0.0),), 'tint': WHITE_HOT, 'alpha': 1.0, 'pulse': 0.6}),
    )),
    # A fracture: frost cracks the shard: an icy glint, the sun bursting out in flame, a ring of fire rushing out.
    'SunFracture': burst('MAGE_SunFracture', 7171, (
        ('sun_star', {'spots': ((0.0, 0.0, 1.0, 3.6, 0.0),), 'tint': ICE_GLINT, 'alpha': 1.0, 'pulse': 0,
                      'scale': [(0.0, 0.2), (0.12, 1.3), (0.4, 0.5), (1.0, 0.0)]}),
        ('spritesheet_4', {'spots': ((0.0, 0.0, 1.0, 3.0, 0.0),), 'sheet': (2, 2), 'flip': 0.3,
                           'tint': ICE_GLINT, 'alpha': 0.9, 'pulse': 0}),
        ('sun_fireburst_4x4', {'spots': ring(0.8, 3, 1.0, 2.8), 'sheet': (4, 4), 'tint': WHITE_HOT, 'alpha': 1.0,
                               'pulse': 0}),
        ('sun_ring', {'flat': True, 'spots': ((0.0, 0.0, 0.1, 7.0, 0.0),), 'tint': SUN_ORANGE, 'alpha': 0.9,
                      'pulse': 0, 'scale': [(0.0, 0.15), (0.45, 1.0), (1.0, 1.15)]}),
    ), 550),
    # A collapse: the sun drawn in to a point, then bursting: a corona flaring out, flame and a ring of fire.
    'SunCollapse': burst('MAGE_SunCollapse', 7172, (
        ('sun_ring', {'flat': True, 'spots': ((0.0, 0.0, 0.1, 6.0, 0.0),), 'tint': SUN_ORANGE, 'alpha': 0.9,
                      'pulse': 0, 'scale': [(0.0, 1.0), (0.25, 0.05), (0.3, 0.05), (0.7, 1.4), (1.0, 1.6)]}),
        ('sun_corona', {'spots': ((0.0, 0.0, 1.2, 5.0, 0.0),), 'tint': SUN_RAYS, 'alpha': 1.0, 'pulse': 0,
                        'scale': [(0.0, 0.0), (0.25, 0.1), (0.4, 1.2), (1.0, 1.5)]}),
        ('sun_glint', {'spots': ((0.0, 0.0, 1.2, 3.0, 0.0),), 'tint': WHITE_HOT, 'alpha': 1.0, 'pulse': 0,
                       'scale': [(0.0, 0.6), (0.25, 0.15), (0.35, 1.4), (1.0, 0.2)]}),
        ('sun_fireburst_4x4', {'spots': ((0.0, 0.0, 1.2, 4.5, 0.0),), 'sheet': (4, 4), 'tint': WHITE_HOT,
                               'alpha': 1.0, 'pulse': 0}),
    ), 750),
    # Scalding Ground: cracked earth glowing with sunfire, flames licking up about it, a heat haze pulsing.
    'ScaldingGround': looping('MAGE_ScaldingGround', 7164, (
        ('Circle_Cracks_02', {'flat': True, 'spots': ((0.0, 0.0, 0.08, 12.0, 0.0),), 'tint': SUN_ORANGE,
                              'alpha': 0.9, 'turn': 70.0, 'pulse': 0}),
        ('sun_rays', {'flat': True, 'spots': ((0.0, 0.0, 0.1, 11.0, 0.0),), 'tint': SUN_ORANGE, 'alpha': 0.35,
                      'turn': -40.0, 'pulse': 1.6}),
        ('sun_flames_4x4', {'spots': ring(3.2, 7, 1.2, 2.4), 'sheet': (4, 4), 'flip': 0.9, 'tint': (1.0, 0.8, 0.55),
                            'alpha': 0.95, 'pulse': 0}),
        ('ice_pieces_2_white_sheet', {'spots': ring(3.6, 8, 0.3, 1.0), 'sheet': (4, 4), 'flip': 1.0,
                                      'tint': (1.0, 0.6, 0.2), 'alpha': 0.9, 'rise': (2.0, 1.8), 'pulse': 0}),
    )),
})

KIT_HEAD, KIT_CHEST, KIT_BASE, KIT_WORLD = 3, 4, 5, 14
VISUALS = {
    'SanctifiedParagon': {'copy': 32, 'id': 16759, 'clear': ('precast', 'cast', 'impact'), 'kits': {
        'state': (15708, {KIT_BASE: 'SanctifiedParagon', KIT_CHEST: 'SanctifiedParagonEmbers',
                          KIT_HEAD: 'SanctifiedParagonCrown', 'sound': None, 'procs': False})}},
    'SanctifiedParagonAwaken': {'copy': 58, 'id': 16762, 'clear': ('precast', 'cast'), 'kits': {
        'impact': (15711, {KIT_BASE: 'SanctifiedParagonAwaken', KIT_CHEST: 0, 'sound': None})}},
    'SeatOfTheParagon': {'copy': 32, 'id': 16760, 'clear': ('precast', 'cast', 'impact'), 'kits': {
        'state': (15709, {KIT_BASE: 'SeatOfTheParagon', 'sound': None, 'procs': False})}},
    'HallowedFootsteps': {'copy': 9735, 'id': 16761, 'clear': ('precast', 'state', 'area'), 'kits': {
        'persistent': (15710, {KIT_WORLD: 'HallowedFootsteps', 'procs': False})}},
    'Thunderfield': {'copy': 9735, 'id': 16763, 'clear': ('precast', 'state', 'area'), 'kits': {
        'persistent': (15712, {KIT_WORLD: 'Thunderfield', 'procs': False})}},
    'ThunderfieldMarch': {'copy': 32, 'id': 16764, 'clear': ('precast', 'cast', 'impact'), 'kits': {
        'state': (15713, {KIT_BASE: 'ThunderfieldMarch', 'sound': None, 'procs': False})}},
}
# The five runes': their grounds (the persistent world slot; March of the Dead's on the Hero), and Blizzard looks cut
# to the part we want (a state only, an impact only).
VISUALS.update({
    'GraveFrozenLine': {'copy': 9735, 'id': 16765, 'clear': ('precast', 'state', 'area'), 'kits': {
        'persistent': (15714, {KIT_WORLD: 'GraveFrozenLine', 'procs': False})}},
    'GraveDiseasedPatch': {'copy': 9735, 'id': 16766, 'clear': ('precast', 'state', 'area'), 'kits': {
        'persistent': (15715, {KIT_WORLD: 'GraveDiseasedPatch', 'procs': False})}},
    'GraveBloodCircle': {'copy': 9735, 'id': 16767, 'clear': ('precast', 'state', 'area'), 'kits': {
        'persistent': (15716, {KIT_WORLD: 'GraveBloodCircle', 'procs': False})}},
    'GraveBloodMarch': {'copy': 32, 'id': 16768, 'clear': ('precast', 'cast', 'impact'), 'kits': {
        'state': (15717, {KIT_BASE: 'GraveBloodMarch', 'sound': None, 'procs': False})}},
    'OnslaughtWake': {'copy': 9735, 'id': 16769, 'clear': ('precast', 'state', 'area'), 'kits': {
        'persistent': (15718, {KIT_WORLD: 'OnslaughtWake', 'procs': False})}},
    'ScaldingGround': {'copy': 9735, 'id': 16770, 'clear': ('precast', 'state', 'area'), 'kits': {
        'persistent': (15719, {KIT_WORLD: 'ScaldingGround', 'procs': False})}},
    # Icy Touch's, Plague Strike's and Blood Boil's hits without their casts on the Hero; Cleave's hit.
    'GraveRimeHit': {'copy': 11152, 'id': 16772, 'clear': ('precast', 'cast'), 'kits': {}},
    'GraveBlightHit': {'copy': 11624, 'id': 16773, 'clear': ('precast', 'cast', 'state'), 'kits': {}},
    'GraveGoreHit': {'copy': 11117, 'id': 16774, 'clear': ('precast', 'cast'), 'kits': {}},
    'WakeHit': {'copy': 219, 'id': 16775, 'clear': ('precast', 'cast'), 'kits': {
        'impact': (15723, {KIT_CHEST: 'WakeSlash'})}},
    # The new hits and bursts (one-shot looks in an impact kit).
    'ThunderEchoHitLook': {'copy': 145, 'id': 16784, 'clear': ('precast', 'cast'), 'kits': {
        'impact': (15721, {KIT_BASE: 'ThunderEchoHit'})}},
    'ThunderBurstLook': {'copy': 7449, 'id': 16785, 'clear': ('precast', 'cast'), 'kits': {
        'impact': (15722, {KIT_BASE: 'ThunderBurst'})}},
    'WakeAftershockLook': {'copy': 145, 'id': 16786, 'clear': ('precast', 'cast'), 'kits': {
        'impact': (15724, {KIT_BASE: 'WakeAftershock'})}},
    'SunFractureLook': {'copy': 17, 'id': 16787, 'clear': ('precast', 'cast', 'state'), 'kits': {
        'impact': (15726, {KIT_BASE: 'SunFracture'})}},
    'SunCollapseLook': {'copy': 963, 'id': 16788, 'clear': ('precast', 'cast', 'state'), 'kits': {
        'impact': (15727, {KIT_BASE: 'SunCollapse'})}},
    # States: Power Infusion's glow on the vow, Hand of Freedom's on its afterimage, the NPC Shadowform on a Hollow
    # Echo, Living Bomb's on a Sunshard, Deep Freeze's on a Broken Core.
    'DawnVow': {'copy': 7553, 'id': 16779, 'clear': ('precast', 'cast', 'impact'), 'kits': {}},
    'DawnAfterimage': {'copy': 4050, 'id': 16780, 'clear': ('precast', 'cast', 'impact'), 'kits': {}},
    'HollowEcho': {'copy': 3619, 'id': 16781, 'clear': ('precast', 'cast', 'impact'), 'kits': {}},
    'Sunshard': {'copy': 32, 'id': 16782, 'clear': ('precast', 'cast', 'impact'), 'kits': {
        'state': (15725, {KIT_CHEST: 'SunshardGlow', 'sound': None, 'procs': False})}},
    'BrokenCore': {'copy': 9963, 'id': 16783, 'clear': ('precast', 'cast', 'impact'), 'kits': {}},
})

ICONS = [RUNE_ICON, PARAGON_ICON, STORM_ICON, THUNDER_ICON, WELLSPRING_ICON, MIST_ICON,
         GRAVE_ICON, ONSLAUGHT_ICON, DAWN_ICON, HOLLOW_ICON, SUN_ICON, 'Spell_Shadow_DeathPact',
         'Spell_Deathknight_ClassIcon', 'Spell_Deathknight_FrostPresence', 'Spell_Deathknight_UnholyPresence',
         'Spell_Deathknight_BloodPresence', 'Spell_Deathknight_IceTouch', 'Spell_Frost_ChillingBlast',
         'Spell_Shadow_PlagueCloud', 'Spell_Shadow_CreepingPlague', 'Spell_Deathknight_BloodBoil',
         'Spell_Deathknight_DeathStrike', 'Spell_Frost_FrostNova', 'Ability_Gouge', 'Ability_Warrior_Shockwave',
         'Ability_Paladin_BeaconofLight', 'Spell_Holy_SearingLight', 'Spell_Holy_HolyBolt',
         'Spell_Holy_AuraMastery', 'Spell_Shadow_Shadowform', 'Spell_Shadow_SiphonMana', 'Spell_Shadow_UnholyFrenzy',
         'Spell_Shadow_PsychicScream', 'Spell_Shadow_DemonicFortitude', 'Ability_Mage_LivingBomb',
         'Ability_Mage_FrostFireBolt', 'Spell_Fire_SoulBurn', 'Spell_Fire_Incinerate',
         'Spell_Nature_HealingWaveGreater', 'Spell_Frost_SummonWaterElemental', 'Spell_Nature_Rejuvenation',
         'Ability_Monk_SoothingMists', 'Spell_Monk_EnvelopingMist', 'Spell_Nature_ThunderClap',
         'Ability_Warrior_ShieldWall',
         'Spell_Nature_LightningOverload', 'Spell_Nature_ChainLightning',
         'Spell_Nature_LightningShield', 'Spell_Holy_InnerFire', 'Spell_Holy_RighteousFury', 'Spell_Holy_HealingAura',
         'Spell_Holy_DevotionAura', 'Spell_Holy_SealOfWrath', 'Spell_Holy_PowerWordShield', 'Spell_Holy_HolyGuidance',
         'Spell_Fire_FelFlameRing', 'Ability_Warlock_ChaosBolt', 'Spell_Shadow_SummonFelGuard',
         'Spell_Shadow_SummonImp', 'Ability_Warlock_DemonicEmpowerment', 'Spell_Shadow_DemonForm',
         'Spell_Shadow_DemonicCircleTeleport', 'Ability_Warrior_Cleave']
assert patch_player  # the helpers' module

# The runes' own auras stay off the buff bar (the rune button shows the bound rune): hidden, their words kept.
RUNE_AURAS = {90700, 90730, 90750, 90770, 90790, 90810, 90830, 90860, 90890, 90920, 90950}
for _spell in SPELLS:
    if _spell['id'] in RUNE_AURAS:
        _spell['set'] = {**_spell['set'], 'Attributes': HIDDEN['Attributes']}

# The spells that wear the new hits and bursts.
for _spell in SPELLS:
    _key = {90774: 'ThunderEchoHitLook', 90775: 'ThunderBurstLook', 90864: 'WakeAftershockLook',
            90954: 'SunFractureLook', 90956: 'SunCollapseLook'}.get(_spell['id'])
    if _key:
        _spell['visual'] = _key
