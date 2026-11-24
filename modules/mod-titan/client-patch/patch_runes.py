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
- 90813 Rolling Mist: healing every sec for 4 sec;
- 90814 Jade Shelter: an absorb, 4 sec;
- 90820-90828 its first nine markers, retired (its Extended Options' block: 93125-93139, hidden).

The five after them (their own concept docs), each with its markers 20 on from its first:

- the Threefold Grave, 90830 (its revised draft): Shared Burial 90831, the Frozen Grave's damage 90838 and slow
  90839, the Plagued Grave's 90840, Grave Pestilence 90841, the Crimson Grave's 90842, Crimson Ward 90843, the
  Graves' grounds 90845-90847, Grave Ward 90844; 90848 was the first March of the Dead's (on the Hero), 90832-90837
  and 90849 the first draft's (Requiem, the Dominions);
- the Unbroken Onslaught, 90860 (its procs: melee abilities): Weapon Wake 90861, Bloodletting 90862, the wake's
  ground 90863, Aftershock 90864, Second Wind 90865;
- the Dawnbound Crusader, 90890: the first draft's Dawn Vow 90891 (on the ally 90892; unlearned), the echo's heal
  and damage 90894-90895, their looks 90896-90897, Sunburst's 90898, Sacred Ground 90899, Radiant Ward 90900;
- the Hollow Congregation, 90920: the echo's look 90921, Hollow Flay 90922, the barrage's bolt 90923, Accusation
  90924, Last Word 90925, Shadow Mend 90926;
- the Shattered Sun, 90950: Sunshard 90951 (stacks), Frozen Core 90952, Shatter 90953 and its look 90954, Second
  Sunrise 90955 and its look 90956, Scalding Ground 90957;
- the Spectral Blade, 90980: Phantom Strike 90981, Open Wound 90983 and its tick 90984, Vanishing Act 90985, Marked
  for Murder 90986; modifiers' markers 91120-91128;
- Runes of Enhancement: 91170, the source of the Uncommons' spell modifiers (family 99); the Epics' and Rares'
  strikes 91171-91177, heal 91178, Grasping Mind 91179, Phoenix 91180, Quick Recovery 91181, Alpha 91182, Hunter's
  Leap 91183, Earthen Grip 91184;
- the Wild Hunt, 91130: the Spectral Beast's look 91131, Spectral Bite 91132, Ghostfang 91133 and its tick 91134,
  Parting Howl (on the Hero's pet) 91135, Final Pounce 91136; markers 91150-91158 retired (now 93150-93164);
- five from their own dossiers, each one's markers 10 on from its first: Arcane Mind 91200 (Insight 91201, Insight
  Missile 91202, Arcane Echo 91203-91204, Mind Unbound 91205, Mental Reserves 91206, Unbound Mind's missile 91207),
  the Splintered Crown 91220 (Icicle 91221, the crown 91222, Splinter Volley 91223, Frost Wound 91224-91225, Winter's
  Grasp 91226, Cold Front 91227, Ice Splinter 91228), Phoenix Wake 91240 (Ember 91241, flight 91242, the phoenix's
  aura 91243, Scorched Earth 91244-91245, Clinging Embers 91246-91247, Rebirth 91248 and its look 91259, Ashen Flock
  91249), Astral Horizon 91260 (the Orb's aura 91261, Orb Bolt 91262, Wandering Star 91263, Prismatic Lance 91264,
  Supernova 91265 and its look 91269, Astral Remnant 91266-91267), Guiding Light 91280 (the star 91281, its pulse
  91282 and arrival 91283, its flight's look 91284, Sheltering Light 91285, Gentle Passage 91286, Afterglow 91287,
  Parting Grace 91288-91289).
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
AURA_MOD_DAMAGE_FROM_CASTER = 271           # more damage taken from the aura's caster, %
AURA_MOD_POWER_REGEN_PERCENT, POWER_ENERGY = 110, 3     # % faster regeneration of a power
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
    ('Barrage', 'Shattered Glaive', "Felguard's Call"),
    ('Metamorphic Bond', 'Imp Legion', 'Pact of Ruin'),
]
SPELLS += [buff(90710 + slot * 3 + choice, name, RUNE_ICON, '', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)},
                hidden=True)
           for slot, names in enumerate(MODIFIERS) for choice, name in enumerate(names)]

# The Rune of the Sanctified Paragon (90730 on; runes.cpp's Sanctified Paragon section).
# A debuff's kind (Spell.dbc Dispel, Mechanic): Magic, not the Crippling Poison it is copied from (every rune debuff
# showed as a poison); the physical damage over time a Bleed instead (harmful).
DISPEL_NONE, DISPEL_MAGIC, DISPEL_DISEASE = 0, 1, 3
MECHANIC_BLEED = 15
HARMFUL = {**FREE, 'SpellFamilyName': 0, 'SpellFamilyFlags0': 0, 'SpellFamilyFlags1': 0, 'SpellFamilyFlags2': 0,
           'RangeIndex': RANGE_VISION, 'SpellVisual': 0, 'ProcFlags': 0, 'ProcChance': 101,
           'Dispel': DISPEL_MAGIC, 'SpellMechanic': 0}
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
        'description': 'Absorbs damage.', 'tooltip': 'Absorbs damage.',
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
# Its markers: the Extended Options' block (93050, below); 90740-90748 retired.

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
        'description': 'Absorbs damage.', 'tooltip': 'Absorbs damage.',
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
         'Thunder Clap charges you with thunder. Your next Shield Slam releases a Thunder Echo: 42% of its damage '
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
        'description': 'Absorbs damage.', 'tooltip': 'Absorbs damage.',
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
             'tooltip': 'Healing every second.', 'set': {**HEAL_SET, 'DurationIndex': duration},
             'effects': {1: {'Effect': 6, 'Aura': AURA_PERIODIC_HEAL, 'Amplitude': 1000, 'BasePoints': 0,
                             'DieSides': 1, 'RealPointsPerLevel': 0.0, 'TargetA': TARGET_UNIT_TARGET_ALLY,
                             'TargetB': 0}},
             'clear': [2, 3]}
    if visual:
        spell['visual'] = visual
    return spell


def absorb(spell_id, name, icon, duration):
    return {'id': spell_id, 'copy': POWER_WORD_SHIELD, 'name': name, 'icon': icon, 'description': 'Absorbs damage.',
            'tooltip': 'Absorbs damage.', 'set': {**HEAL_SET, 'DurationIndex': duration},
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
    periodic_heal(90813, 'Rolling Mist', 'Ability_Monk_SoothingMists', 'A rolling mist.', DURATION_4S),
    absorb(90814, 'Jade Shelter', 'Ability_Monk_SoothingMists', DURATION_4S),
]
WELLSPRING_MODIFIERS = [
    ('Split Tributary', 'Deep Current', 'Concentrated Current'),
    ('Flowing Waters', 'Bedrock', 'Renewed Shield'),
    ('Tidal Shelter', 'Renewing Current', 'Long Reach'),
]
# The Wandering Mist's markers: the Extended Options' block (93125, below); 90820-90828 retired.
SPELLS += [buff(first + slot * 3 + choice, name, icon, '', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)},
                hidden=True)
           for first, icon, modifiers in ((90800, WELLSPRING_ICON, WELLSPRING_MODIFIERS),)
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
SPELL_ATTR3_DOT_STACKING = 0x80          # stacks separately for each caster
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
    """An aura on an enemy: effect (an aura() dict), its tick set by the script for damage over time. Each Hero's
    own (stacks separately for each caster): the same aura of two Heroes on one enemy replaced the first's (a
    second Shattered Sun player's Sunshards took the first's)."""
    spell = {'id': spell_id, 'copy': CRIPPLING_POISON, 'name': name, 'icon': icon, 'description': text,
             'tooltip': text, 'set': {**HARMFUL, 'DurationIndex': duration, 'SchoolMask': school, **SCRIPTED,
                                      'AttributesEx3': SPELL_ATTR3_IGNORE_CASTER_MODIFIERS | SPELL_ATTR3_DOT_STACKING
                                      | SPELL_ATTR3_ALWAYS_HIT,      # a rune's own debuff never misses
                                      'AttributesEx4': SPELL_ATTR4_NO_PARTIAL_IMMUNITY
                                      | SPELL_ATTR4_CANNOT_BE_RESISTED},
             'effects': {1: {**effect, 'TargetA': TARGET_UNIT_TARGET_ENEMY}}, 'clear': [2, 3]}
    if key:
        spell['visual'] = key
    return spell


def dot(spell_id, name, icon, school, duration, text):
    """Damage over time: a periodic dummy each second, its damage the script's own spell (spell_titan_rune_dot).
    Physical damage over time is a Bleed: no dispel type, the bleed mechanic (shown so, and bleed effects count it)."""
    spell = harmful(spell_id, name, icon, text, duration, {**aura(AURA_PERIODIC_DUMMY, 1, 0), 'Amplitude': 1000},
                    school)
    if school == PHYSICAL:
        spell['set'].update({'Dispel': DISPEL_NONE, 'SpellMechanic': MECHANIC_BLEED})
    return spell


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
                      'other melee ability rips it open: 96% of a weapon swing to every enemy in it, its first '
                      'target 25% more.',
                      DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)})
# Its procs: every melee ability that hits; the script picks the ones that reopen the wake.
ONSLAUGHT_RUNE['set'] = {**ONSLAUGHT_RUNE['set'], 'ProcFlags': PROC_DONE_MELEE_SPELL, 'ProcChance': 101}
SPELLS += [
    buff(90830, 'Rune of the Threefold Grave', GRAVE_ICON,
         'Icy Touch lays a Frozen Grave beneath its target, Plague Strike a Plagued Grave, Blood Strike a Crimson '
         'Grave: one of each, 8 sec. Enemies in two or more take 20% more from your Graves. Blood Strike also '
         'shields you.', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    harmful(90831, 'Shared Burial', 'Spell_Shadow_DeathPact', 'Taking more damage from the Threefold Grave.',
            DURATION_2S, aura(AURA_DUMMY, 1, 0), SHADOW),
    strike(90832, 'Grave Pestilence', 'Spell_Shadow_CreepingPlague', SHADOW, modifiers=True),
    strike(90833, 'Bloodletting', 'Ability_Gouge', PHYSICAL, modifiers=True),
    strike(90834, 'Hollow Flay', 'Spell_Shadow_SiphonMana', SHADOW),
    strike(90838, 'Frozen Grave', 'Spell_Deathknight_IceTouch', FROST, key='GraveRimeHit', modifiers=True),
    harmful(90839, 'Frozen Grave', 'Spell_Frost_ChillingBlast', 'Movement speed reduced.', DURATION_6S,
            aura(AURA_MOD_DECREASE_SPEED, -30, 0), FROST),
    strike(90840, 'Plagued Grave', 'Spell_Shadow_PlagueCloud', SHADOW, key='GraveBlightHit', modifiers=True),
    dot(90841, 'Grave Pestilence', 'Spell_Shadow_CreepingPlague', SHADOW, DURATION_6S, 'Shadow damage every second.'),
    strike(90842, 'Crimson Grave', 'Spell_Deathknight_BloodBoil', SHADOW, key='GraveGoreHit', modifiers=True),
    {**absorb(90843, 'Crimson Ward', 'Spell_Deathknight_DeathStrike', DURATION_10S),
     'set': {**HEAL_SET, 'DurationIndex': DURATION_10S, 'SchoolMask': SHADOW}},
    ground(90845, 'Frozen Grave', 'Spell_Deathknight_IceTouch', 'GraveFrozenLine', DURATION_20S),
    ground(90846, 'Plagued Grave', 'Spell_Shadow_PlagueCloud', 'GraveDiseasedPatch', DURATION_20S),
    ground(90847, 'Crimson Grave', 'Spell_Deathknight_BloodBoil', 'GraveBloodCircle', DURATION_20S),
    shown(90848, 'Crimson Grave', 'Spell_Deathknight_BloodBoil', 'Your Crimson Grave follows you.', DURATION_20S,
          key='GraveBloodMarch'),
    {**heal(90844, 'Grave Ward', 'Spell_Deathknight_BloodBoil', 'The dead mend you.'),
     'set': {**HEAL_SET, 'SchoolMask': SHADOW, 'AttributesEx2': 0, 'SpellVisual': 0}},

    ONSLAUGHT_RUNE,
    strike(90861, 'Weapon Wake', 'Ability_Warrior_Cleave', PHYSICAL, key='WakeHit', modifiers=True),
    dot(90862, 'Bloodletting', 'Ability_Gouge', PHYSICAL, DURATION_6S, 'Bleeding every second.'),
    ground(90863, 'Weapon Wake', 'Ability_Warrior_Cleave', 'OnslaughtWake', DURATION_10S),
    strike(90864, 'Aftershock', 'Ability_Warrior_Shockwave', PHYSICAL, VISUAL_THUNDER_HIT, modifiers=True),
    {**heal(90865, 'Second Wind', 'Ability_Warrior_Cleave', 'The onslaught carries you on.'),
     'set': {**HEAL_SET, 'SchoolMask': PHYSICAL, 'AttributesEx2': 0, 'SpellVisual': 0}},
    # Battlefield Claim: slowed, and more damage taken from the Hero (the aura counts the caster's damage only).
    {**(claim := harmful(90866, 'Battlefield Claim', 'Ability_Warrior_Rampage',
                         'Movement speed reduced by 30%. Taking 10% more damage from the Hero who claimed you.',
                         DURATION_8S, aura(AURA_MOD_DECREASE_SPEED, -30, 0), PHYSICAL)),
     'set': {**claim['set'], 'Dispel': DISPEL_NONE},
     'effects': {**claim['effects'],
                 2: {**aura(AURA_MOD_DAMAGE_FROM_CASTER, 10, 0), 'TargetA': TARGET_UNIT_TARGET_ENEMY}},
     'clear': [3]},

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
     'description': 'Absorbs damage.', 'tooltip': 'Absorbs damage.',
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
        'description': 'A hollow echo of Mind Flay.', 'tooltip': 'Shadow damage every second.',
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
    {**heal(90926, 'Renewed Witness', 'Spell_Shadow_SiphonMana', 'Your Hollow Echoes mend you.'),
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
# ---- The Spectral Blade (90980; markers 91120 on) and the Wild Hunt (91130; markers 91150 on): 91000-91106 are
# patch_ranks.py's.
BLADE_ICON = 'Ability_Rune_SpectralBlade'
HUNT_ICON = 'Ability_Rune_WildHunt'
AURA_MOD_INCREASE_SPEED = 31
VISUAL_GHOSTLY_STRIKE, VISUAL_GHOST, VISUAL_KILL_COMMAND, VISUAL_DASH = 4159, 886, 8138, 2276
VISUAL_VANISH = 255                         # Vanish: its puff of smoke (the cast kit)
SPELLS += [
    buff(90980, 'Rune of the Spectral Blade', BLADE_ICON,
         'Eviscerate calls a phantom assassin behind its victim: a moment later it strikes for 200% of '
         'Eviscerate\'s damage, and your Energy regenerates 100% faster for 4 sec.', DURATION_INFINITE,
         {1: aura(AURA_DUMMY, 1, 0)}),
    {**strike(90981, 'Phantom Strike', 'Ability_Rogue_Eviscerate', SHADOW, VISUAL_GHOSTLY_STRIKE),
     'visual': 'PhantomStrikeLook'},
    shown(90988, 'Phantom Form', BLADE_ICON, '', DURATION_INFINITE, key='PhantomForm'),
    look(90989, 'Phantom Vanish', BLADE_ICON, VISUAL_VANISH),
    dot(90983, 'Open Wound', 'Ability_Rogue_Eviscerate', PHYSICAL, DURATION_4S, 'Bleeding every second.'),
    strike(90984, 'Open Wound', 'Ability_Rogue_Eviscerate', PHYSICAL),
    {**buff(90985, 'Vanishing Act', 'Ability_Rogue_Sprint', 'Movement speed increased by 30%.', DURATION_3S,
            {1: aura(AURA_MOD_INCREASE_SPEED, 30, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_3S, 'SpellVisual': VISUAL_DASH}},
    {**buff(90987, 'Phantom Rush', 'Ability_Rogue_Eviscerate', 'Energy regeneration increased by 100%.',
            DURATION_4S, {1: aura(AURA_MOD_POWER_REGEN_PERCENT, 100, POWER_ENERGY)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_4S}},
    harmful(90986, 'Marked for Murder', 'Ability_Rogue_FindWeakness',
            'Your next phantom on this target deals more damage.', DURATION_10S,
            aura(AURA_DUMMY, 1, 0), SHADOW),

    buff(91130, 'Rune of the Wild Hunt', HUNT_ICON,
         'Kill Command unleashes a Spectral Beast in your pet\'s likeness on its target for 10 sec: it bites for '
         'part of your pet\'s damage. During Bestial Wrath, Kill Command recovers in 5 sec and a beast joins the '
         'hunt every second.', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    {**shown(91131, 'Spectral Beast', HUNT_ICON, '', DURATION_INFINITE,
             effects={1: aura(AURA_MOD_THREAT, -100, ALL_SCHOOLS)}),
     'set': {**HIDDEN, 'DurationIndex': DURATION_INFINITE, 'SpellVisual': VISUAL_GHOST}},
    strike(91132, 'Spectral Bite', 'Ability_Hunter_KillCommand', PHYSICAL),
    dot(91133, 'Ghostfang', 'Ability_Druid_Rake', PHYSICAL, DURATION_4S, 'Bleeding every second.'),
    strike(91134, 'Ghostfang', 'Ability_Druid_Rake', PHYSICAL),
    {**buff(91135, 'Parting Howl', 'Ability_Hunter_KillCommand', 'Damage done increased by 30%.', DURATION_6S,
            {1: aura(AURA_MOD_DAMAGE_PERCENT_DONE, 30, ALL_SCHOOLS)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_6S, 'SpellVisual': VISUAL_KILL_COMMAND}},
    strike(91136, 'Final Pounce', 'Ability_Hunter_KillCommand', PHYSICAL, VISUAL_KILL_COMMAND),
]
next(spell for spell in SPELLS if spell['id'] == 90981)['set']['AttributesEx2'] = SPELL_ATTR2_CANT_CRIT
# The bleeds' ticks: armor never reduces them (Unit::IsDamageReducedByArmor).
for _bleed in (90984, 91134):
    next(spell for spell in SPELLS if spell['id'] == _bleed)['effects'][1]['Mechanic'] = 15

LEGEND_MODIFIERS = [
    (90850, GRAVE_ICON, [('Far-Reaching Graves', 'Long Rest', 'March of the Dead'),
                         ('Cruel Winter', 'Grave Pestilence', 'Crimson Bastion'),
                         ('Deep Burial', 'Grave Hunger', 'Grave Ward')]),
    (90880, ONSLAUGHT_ICON, [("Executioner's Cut", 'Whirling Steel', 'Raking Passage'),
                             ('Overpowering Echo', 'Bloodletting', 'Aftershock'),
                             ('Relentless', 'Battlefield Claim', 'Second Wind')]),
    (90910, DAWN_ICON, [('Sunburst', 'Focused Dawn', 'Communion'),
                        ('Sacred Ground', 'Steadfast', "Judgment's Echo"),
                        ("Dawn's Mercy", 'Swift Dawn', 'Radiant Ward')]),
    (90970, SUN_ICON, [('Wildfire', 'Spreading Sun', 'Heavy Sun'),
                       ('Wide Burst', 'Scalding Ground', 'Chain Reaction'),
                       ('Implosion', 'Second Sunrise', 'Frozen Core')]),
]
next(spell for spell in SPELLS if spell['id'] == 90923)['set']['Speed'] = 20.0
next(spell for spell in SPELLS if spell['id'] == 90951)['set']['StackAmount'] = 8
# Grave Pestilence is a disease.
next(spell for spell in SPELLS if spell['id'] == 90841)['set']['Dispel'] = DISPEL_DISEASE
# Bloodletting's tick is a bleed: armor never reduces it (Unit::IsDamageReducedByArmor).
next(spell for spell in SPELLS if spell['id'] == 90833)['effects'][1]['Mechanic'] = 15
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


def upright(spots):
    """Spots for camera-facing flames: unturned in their own plane (ring() rolls each one about its middle)."""
    return tuple((x, y, z, size, 0.0) for x, y, z, size, _ in spots)


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
SUN_ORANGE = (1.0, 0.45, 0.1)
FLAME_ORANGE, FLAME_GOLD = (1.0, 0.42, 0.12), (1.0, 0.72, 0.32)


def flames(spots, alpha=0.9, flicker=0.32):
    """Fire standing at spots (upright, base on the ground): soft wisps of flame flickering between their four
    shapes, thin tongues licking up through them. Both sheets' cells fade out before their edges."""
    tongues = tuple((x, y, z + size * 0.1, size * 0.8, 0.0) for x, y, z, size, _ in spots)
    return (
        ('fire_wisps_2x2', {'spots': tuple((x, y, z + size * 0.15, size * 1.35, 0.0) for x, y, z, size, _ in spots),
                            'sheet': (2, 2), 'flip': flicker, 'tint': FLAME_ORANGE,
                            'alpha': alpha, 'pulse': 0}),
        ('fire_tongues_4x4', {'spots': tongues, 'sheet': (4, 4), 'flip': 0.9, 'tint': FLAME_GOLD,
                              'alpha': alpha, 'pulse': 0}),
    )
WHITE_HOT, ICE_GLINT = (1.0, 0.93, 0.75), (0.65, 0.88, 1.0)
EFFECTS.update({
    'Thunderfield': thunderfield('WAR_Thunderfield', 7157, 16.0),
})

KIT_HEAD, KIT_CHEST, KIT_BASE, KIT_WORLD = 3, 4, 5, 14
KIT_LEFT_HAND, KIT_RIGHT_HAND = 6, 7
# Blizzard effect models (SpellVisualEffectName ids, the ones their own spells use) for the runes' looks: Echoing
# Thunder's, the Shattered Sun's and the Spectral Blade's are the game's own, not our sheets.
FX_LIGHTNING_CLOUD_LOW = 3792       # Spells\\LightningStorm_CloudLow_State: a small storm cloud overhead
FX_LIGHTNING_HAND = 88              # Spells\\Lightning_PreCast_Low_Hand: lightning crackling in the hands
FX_LIGHTNING_BOLT_IMPACT = 281      # Spells\\LightningBolt_Impact_Chest
FX_LIGHTNING_SHIELD_IMPACT = 173    # Spells\\LightningShield_Impact_Base: sparks at the feet
FX_THUNDERCLAP = 349                # Spells\\ThunderClap_Cast_Base: Thunder Clap's ring of dust and thunder
FX_THUNDERSTORM = 4405              # spells\\shaman_thunder: Thunderstorm's blue shockwave
FX_LIVING_BOMB_STATE = 4048         # spells\\mage_livingbomb_state: Living Bomb's burning bomb on the target
FX_LIVING_BOMB_AREA = 4049          # spells\\mage_livingbomb_area: Living Bomb's explosion
FX_ICE_LANCE_IMPACT = 3075          # spells\\ice_lance_impact: shards of ice
FX_FIRE_NOVA = 714                  # Spells\\FireNova_Area: Blast Wave's ring of fire
FX_FIRE_UBER_IMPACT = 220           # Spells\\Fire_ImpactDD_Uber_Chest: Pyroblast's sized fire hit
FX_FLAMESTRIKE_GROUND = 420         # Spells\\Flamestrike_Impact_Base: Flamestrike's burning ground
FX_SHADOW_DANCE = 4585              # spells\\shadowdance_state: Shadow Dance's purple wisps about the body
FX_SHADOW_BASE = 1703               # Spells\\Shadow_Precast_Med_Base: a pool of shadow at the feet
FX_SHADOW_HIGH_IMPACT = 624         # Spells\\Shadow_ImpactDD_High_Chest: a heavy shadow hit
FX_SHADOWSTEPS = 4185               # spells\\shadowsteps_fx: Shadowstep's smoke
FX_GROUND_RUPTURE = 3696            # spells\\ground_rupture: the ground torn open
FX_CLEAVE_IMPACT = 458              # Spells\\Cleave_Impact_Chest: Cleave's hit
FX_CLEAVE_ARC = 1643                # Spells\\Cleave_Cast_Base: Cleave's swing, a slash across the ground
FX_SHOCKWAVE_IMPACT = 4057          # spells\\warrior_shockwave_impact: Shockwave's hit at the feet
FX_SLAM_IMPACT = 1034               # Spells\\Slam_Impact_Chest: Slam's hit
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
    # Thunder Clap's charge: a small storm cloud over the Hero, lightning in their hands.
    'ThunderfieldMarch': {'copy': 32, 'id': 16764, 'clear': ('precast', 'cast', 'impact'), 'kits': {
        'state': (15713, {KIT_HEAD: FX_LIGHTNING_CLOUD_LOW, KIT_LEFT_HAND: FX_LIGHTNING_HAND,
                          KIT_RIGHT_HAND: FX_LIGHTNING_HAND, KIT_BASE: 0, KIT_CHEST: 0, 'sound': None,
                          'procs': False})}},
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
    # The Weapon Wake: the ground torn open along it. Its rip, on each enemy: Cleave's hit and swing. Aftershock:
    # Shockwave's hit and Slam's.
    'OnslaughtWake': {'copy': 9735, 'id': 16769, 'clear': ('precast', 'cast', 'state', 'area'), 'kits': {
        'persistent': (15718, {KIT_BASE: FX_GROUND_RUPTURE, KIT_WORLD: 0, 'procs': False})}},
    # Flamestrike's burning ground.
    'ScaldingGround': {'copy': 9735, 'id': 16770, 'clear': ('precast', 'cast', 'state', 'area'), 'kits': {
        'persistent': (15719, {KIT_BASE: FX_FLAMESTRIKE_GROUND, KIT_WORLD: 0, 'procs': False})}},
    # Icy Touch's, Plague Strike's and Blood Boil's hits without their casts on the Hero; Cleave's hit.
    'GraveRimeHit': {'copy': 11152, 'id': 16772, 'clear': ('precast', 'cast'), 'kits': {}},
    'GraveBlightHit': {'copy': 11624, 'id': 16773, 'clear': ('precast', 'cast', 'state'), 'kits': {}},
    'GraveGoreHit': {'copy': 11117, 'id': 16774, 'clear': ('precast', 'cast'), 'kits': {}},
    'WakeHit': {'copy': 219, 'id': 16775, 'clear': ('precast', 'cast'), 'kits': {
        'impact': (15723, {KIT_CHEST: FX_CLEAVE_IMPACT, KIT_BASE: FX_CLEAVE_ARC})}},
    # The new hits and bursts (one-shot looks in an impact kit).
    # A Thunder Echo's hit: Lightning Bolt's impact, sparks at the feet. Its burst: Thunder Clap's ring and
    # Thunderstorm's shockwave about Shield Slam's target.
    'ThunderEchoHitLook': {'copy': 145, 'id': 16784, 'clear': ('precast', 'cast', 'state'), 'kits': {
        'impact': (15721, {KIT_CHEST: FX_LIGHTNING_BOLT_IMPACT, KIT_BASE: FX_LIGHTNING_SHIELD_IMPACT})}},
    'ThunderBurstLook': {'copy': 7449, 'id': 16785, 'clear': ('precast', 'cast', 'state'), 'kits': {
        'impact': (15722, {KIT_BASE: FX_THUNDERCLAP, KIT_WORLD: FX_THUNDERSTORM, KIT_CHEST: 0})}},
    'WakeAftershockLook': {'copy': 145, 'id': 16786, 'clear': ('precast', 'cast', 'state'), 'kits': {
        'impact': (15724, {KIT_BASE: FX_SHOCKWAVE_IMPACT, KIT_CHEST: FX_SLAM_IMPACT})}},
    # The shards shattering: Living Bomb's explosion with Ice Lance's shards. Second Sunrise: Blast Wave's ring
    # of fire and a great fire hit.
    'SunFractureLook': {'copy': 17, 'id': 16787, 'clear': ('precast', 'cast', 'state'), 'kits': {
        'impact': (15726, {KIT_WORLD: FX_LIVING_BOMB_AREA, KIT_CHEST: FX_ICE_LANCE_IMPACT, KIT_BASE: 0})}},
    'SunCollapseLook': {'copy': 963, 'id': 16788, 'clear': ('precast', 'cast', 'state'), 'kits': {
        'impact': (15727, {KIT_WORLD: FX_FIRE_NOVA, KIT_CHEST: FX_FIRE_UBER_IMPACT, KIT_BASE: 0})}},
    # States: Power Infusion's glow on the vow, Hand of Freedom's on its afterimage, the NPC Shadowform on a Hollow
    # Echo, Living Bomb's on a Sunshard, Deep Freeze's on a Broken Core.
    'DawnVow': {'copy': 7553, 'id': 16779, 'clear': ('precast', 'cast', 'impact'), 'kits': {}},
    'DawnAfterimage': {'copy': 4050, 'id': 16780, 'clear': ('precast', 'cast', 'impact'), 'kits': {}},
    'HollowEcho': {'copy': 3619, 'id': 16781, 'clear': ('precast', 'cast', 'impact'), 'kits': {}},
    'Sunshard': {'copy': 32, 'id': 16782, 'clear': ('precast', 'cast', 'impact'), 'kits': {
        'state': (15725, {KIT_BASE: FX_LIVING_BOMB_STATE, KIT_CHEST: 0, 'sound': None, 'procs': False})}},
    # The Spectral Blade's phantom: Shadow Dance's purple wisps and a pool of shadow on the Hero's double; its blow:
    # a heavy shadow hit and Shadowstep's smoke.
    'PhantomForm': {'copy': 32, 'id': 16789, 'clear': ('precast', 'cast', 'impact'), 'kits': {
        'state': (15728, {KIT_CHEST: FX_SHADOW_DANCE, KIT_BASE: FX_SHADOW_BASE, 'sound': None, 'procs': False})}},
    'PhantomStrikeLook': {'copy': 4159, 'id': 16790, 'clear': ('precast', 'cast'), 'kits': {
        'impact': (15729, {KIT_CHEST: FX_SHADOW_HIGH_IMPACT, KIT_BASE: FX_SHADOWSTEPS})}},
    'BrokenCore': {'copy': 9963, 'id': 16783, 'clear': ('precast', 'cast', 'impact'), 'kits': {}},
})

ICONS = [RUNE_ICON, PARAGON_ICON, STORM_ICON, THUNDER_ICON, WELLSPRING_ICON, MIST_ICON,
         GRAVE_ICON, ONSLAUGHT_ICON, DAWN_ICON, HOLLOW_ICON, SUN_ICON, BLADE_ICON, HUNT_ICON, 'Spell_Shadow_DeathPact',
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
         'Spell_Shadow_DemonicCircleTeleport', 'Ability_Warrior_Cleave', 'Ability_Rogue_Eviscerate',
         'Ability_Rogue_Sprint', 'Ability_Rogue_FindWeakness', 'Ability_Hunter_KillCommand', 'Ability_Druid_Rake',
         'Spell_Nature_StarFall', 'Spell_Fire_FlameBolt', 'Spell_Frost_FrostBolt02', 'Spell_Shadow_ShadowBolt',
         'Spell_Shadow_LifeDrain', 'Spell_Holy_FlashHeal', 'Spell_Fire_SealOfFire', 'Spell_Shadow_ShadowWard',
         'Ability_Devour', 'Ability_Rogue_Feint', 'Spell_Nature_EarthBind']
assert patch_player  # the helpers' module

# The runes' own auras stay off the buff bar (the rune button shows the bound rune): hidden, their words kept.
# Runes of Enhancement's Epic and Rare effects (src/rune_effects.cpp): strikes and a heal of an amount the script
# sets (shares of the Hero's own hits, their bonuses in it already), Grasping Mind (Mind Flay: -5% damage done),
# Phoenix (Combustion: fire spells always strike critically, 6 sec), Quick Recovery (Evasion: heals on a dodge; its
# procs in titan_rune_effects.sql).
ARCANE = 0x40
AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, PROC_TAKEN_MELEE_AUTO_ATTACK = 71, 0x8
SPELLS += [
    strike(91171, 'Rune Strike', 'Ability_Warrior_Cleave', PHYSICAL),
    strike(91172, 'Rune Strike', 'Spell_Nature_StarFall', ARCANE),
    strike(91173, 'Rune Strike', 'Spell_Fire_FlameBolt', FIRE),
    strike(91174, 'Rune Strike', 'Spell_Frost_FrostBolt02', FROST),
    strike(91175, 'Rune Strike', 'Spell_Shadow_ShadowBolt', SHADOW),
    strike(91176, 'Rune Strike', 'Spell_Holy_HolyBolt', HOLY),
    strike(91177, 'Open Vein', 'Spell_Shadow_LifeDrain', PHYSICAL),
    {**heal(91178, 'Rune Mend', 'Spell_Holy_FlashHeal', 'A rune mends you.'),
     'set': {**HEAL_SET, 'SchoolMask': HOLY, 'AttributesEx2': 0, 'SpellVisual': 0}},
    harmful(91179, 'Grasping Mind', 'Spell_Shadow_SiphonMana', 'Damage done reduced by 5%.', DURATION_4S,
            aura(AURA_MOD_DAMAGE_PERCENT_DONE, -5, ALL_SCHOOLS), SHADOW),
    buff(91180, 'Phoenix', 'Spell_Fire_SealOfFire', 'Your fire spells always strike critically.', DURATION_6S,
         {1: aura(AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, 100, FIRE)}),
    buff(91181, 'Quick Recovery', 'Spell_Shadow_ShadowWard', '', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)},
         hidden=True),
]
SPELLS[-1]['set'] = {**SPELLS[-1]['set'], 'ProcFlags': PROC_TAKEN_MELEE_AUTO_ATTACK, 'ProcChance': 101}
# Batch two: Alpha (Intimidation: the pet +30% damage, 10 sec), Hunter's Leap (Disengage: +30% speed, 4 sec),
# Earthen Grip (Earth Shock: -50% speed, 3 sec).
SPELLS += [
    buff(91182, 'Alpha', 'Ability_Devour', 'Damage done increased by 30%.', DURATION_10S,
         {1: aura(AURA_MOD_DAMAGE_PERCENT_DONE, 30, ALL_SCHOOLS)}),
    buff(91183, "Hunter's Leap", 'Ability_Rogue_Feint', 'Movement speed increased by 30%.', DURATION_4S,
         {1: aura(AURA_MOD_INCREASE_SPEED, 30, 0)}),
    harmful(91184, 'Earthen Grip', 'Spell_Nature_EarthBind', 'Movement speed reduced by 50%.', DURATION_3S,
            aura(AURA_MOD_DECREASE_SPEED, -50, 0), NATURE),
]
next(spell for spell in SPELLS if spell['id'] == 91177)['effects'][1]['Mechanic'] = 15     # a bleed: no armor

# Runes of Enhancement (src/rune_tiers.cpp): the source its Uncommons' spell modifiers name. Family 99 (no spell's)
# so the core matches it to nothing; the server's own check matches each modifier to its rune's ability.
SPELLS.append({**buff(91170, 'Runes of Enhancement', BLADE_ICON, '', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)},
                      hidden=True)})
SPELLS[-1]['set'] = {**SPELLS[-1]['set'], 'SpellFamilyName': 99, 'SpellFamilyFlags0': 0, 'SpellFamilyFlags1': 0,
                     'SpellFamilyFlags2': 0}

# ---- Five more (their own dossiers): Arcane Mind 91200, the Splintered Crown 91220, Phoenix Wake 91240, Astral
# Horizon 91260, Guiding Light 91280; each one's markers 10 on from its first (+ slot * 3 + choice).
MIND_ICON, CROWN_ICON = 'Ability_Rune_ArcaneMind', 'Ability_Rune_SplinteredCrown'
PHOENIX_ICON, HORIZON_ICON, GUIDING_ICON = 'Ability_Rune_PhoenixWake', 'Ability_Rune_AstralHorizon', \
    'Ability_Rune_GuidingLight'
AURA_ADD_PCT_MODIFIER, SPELLMOD_CASTING_TIME, SPELLMOD_COST = 108, 10, 14
SPELLMOD_DAMAGE = 0
MAGE_FAMILY = 3
ARCANE_BLAST_FLAG, FROSTBOLT_FLAG, PYROBLAST_FLAG = 0x20000000, 0x20, 0x400000
DURATION_5S, DURATION_8S, DURATION_12S, DURATION_15S = 28, 31, 29, 8
VISUAL_ARCANE_MISSILE, VISUAL_ICE_LANCE, VISUAL_FIRE_BLAST = 268, 7906, 143
VISUAL_ARCANE_BARRAGE, VISUAL_ARCANE_HIT, VISUAL_PRAYER_JUMP = 9947, 13635, 9425
FX_ICE_CROWN = 54                       # Spells\\Ice_Precast_Uber_Head: Frostbolt's ice about the head
FX_MAGIC_HAND = 272                     # Spells\\Magic_PreCast_Hand: arcane light in the hands
FX_ARCANE_POWER_CHEST = 2122            # spells\\arcanepower_state_chest: Arcane Power's swirl
FX_ARCANE_EXPLOSION_BASE = 734          # Spells\\ArcaneExplosion_Base: Arcane Explosion's ring
FX_HOLY_LIGHT_HEAD = 244                # Spells\\HolyLight_Low_Head: holy light about the head
FX_HOLY_HIGH_IMPACT = 125               # Spells\\Holy_ImpactDD_High_Chest


def missile(spell_id, name, icon, school, visual, speed):
    """A strike that flies (its amount set by the script)."""
    spell = strike(spell_id, name, icon, school, visual)
    spell['set'] = {**spell['set'], 'Speed': speed, 'AttributesEx2': SPELL_ATTR2_CANT_CRIT}
    return spell


def quickening(spell_id, name, icon, text, duration, flag, pct, stacks=0, charges=0):
    """A mage spell's cast time cut by pct% (a family 3 spell modifier: the engine makes the client honour it)."""
    spell = buff(spell_id, name, icon, text, duration,
                 {1: {**aura(AURA_ADD_PCT_MODIFIER, -pct, SPELLMOD_CASTING_TIME), 'Mask': flag}}, stacks=stacks)
    spell['set'] = {**spell['set'], 'SpellFamilyName': MAGE_FAMILY, 'ProcCharges': charges}
    return spell


SPELLS += [
    # Arcane Mind.
    buff(91200, 'Rune of Arcane Mind', MIND_ICON,
         'Arcane Blast grants Arcane Insight. At 4, your next Arcane Missiles spends it all: free, and each missile '
         'is joined by an Insight missile.', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    buff(91201, 'Arcane Insight', 'Spell_Arcane_MindMastery',
         'At 4 or more, your next Arcane Missiles spends all your Insight: it costs no mana and each missile is '
         'joined by an Insight missile.', DURATION_15S, {1: aura(AURA_DUMMY, 1, 0)}, stacks=6),
    missile(91202, 'Insight Missile', 'Spell_Nature_StarFall', ARCANE, VISUAL_ARCANE_MISSILE, 24.0),
    dot(91203, 'Arcane Echo', 'Spell_Arcane_StarFire', ARCANE, DURATION_6S, 'Arcane damage every second.'),
    strike(91204, 'Arcane Echo', 'Spell_Arcane_StarFire', ARCANE),
    shown(91205, 'Mind Unbound', 'Spell_Arcane_MindMastery',
          'Your next Arcane Missiles spends your Arcane Insight.', DURATION_15S, key='MindUnbound'),
    {**buff(91206, 'Mental Reserves', 'Spell_Shadow_ManaBurn', 'Arcane Blast costs 20% less mana.', DURATION_4S,
            {1: {**aura(AURA_ADD_PCT_MODIFIER, -20, SPELLMOD_COST), 'Mask': ARCANE_BLAST_FLAG}})},
    {**missile(91207, 'Arcane Missiles', 'Spell_Nature_StarFall', ARCANE, VISUAL_ARCANE_MISSILE, 24.0)},
    # The Splintered Crown.
    buff(91220, 'Rune of the Splintered Crown', CROWN_ICON,
         'A Frostbolt that hits forms an Icicle above you: 1 sec later it strikes the same enemy for 30% of the '
         'Frostbolt\'s damage. Up to 5 wait at once.', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    missile(91221, 'Icicle', 'Spell_Frost_IceShard', FROST, VISUAL_ICE_LANCE, 30.0),
    shown(91222, 'Splintered Crown', CROWN_ICON, 'Icicles waiting to strike.', DURATION_INFINITE, key='FrostCrown'),
    quickening(91223, 'Splinter Volley', 'Spell_Frost_FrostBolt02', 'Your next Frostbolt casts 10% faster.',
               DURATION_5S, FROSTBOLT_FLAG, 10, charges=1),
    dot(91224, 'Frost Wound', 'Spell_Frost_IceStorm', FROST, DURATION_6S, 'Frost damage every second.'),
    strike(91225, 'Frost Wound', 'Spell_Frost_IceStorm', FROST),
    harmful(91226, 'Permafrost', 'Spell_Frost_FrostShock', 'Movement speed reduced by 20%.', DURATION_3S,
            aura(AURA_MOD_DECREASE_SPEED, -20, 0), FROST),
    quickening(91227, 'Cold Front', 'Spell_Frost_ColdHearted', 'Your next Frostbolt is instant.', DURATION_5S,
               FROSTBOLT_FLAG, 100, charges=1),
    missile(91228, 'Ice Splinter', 'Spell_Frost_IceShard', FROST, VISUAL_ICE_LANCE, 36.0),
    # Phoenix Wake.
    buff(91240, 'Rune of Phoenix Wake', PHOENIX_ICON,
         'Pyroblast leaves a Phoenix Ember on its target for 8 sec. Fire Blast on it calls a phoenix diving from '
         'above you onto it, through your enemies.', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    harmful(91241, 'Phoenix Ember', 'Spell_Fire_Burnout', 'A phoenix sleeps in this ember. Fire Blast wakes it.',
            DURATION_8S, aura(AURA_DUMMY, 1, 0), FIRE, key='Sunshard'),
    strike(91242, 'Phoenix Flight', 'Spell_Fire_Burnout', FIRE, VISUAL_FIRE_BLAST),
    # The phoenix's and the Orb's looks are their auras' states: shown auras (a hidden one's state is not drawn).
    {**shown(91243, 'Phoenix', PHOENIX_ICON, '', DURATION_INFINITE,
             effects={1: aura(AURA_MOD_THREAT, -100, ALL_SCHOOLS)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_INFINITE}},
    ground(91244, 'Scorched Earth', 'Spell_Fire_Fire', 'ScaldingGround', DURATION_4S),
    strike(91245, 'Scorched Earth', 'Spell_Fire_Fire', FIRE),
    dot(91246, 'Clinging Embers', 'Spell_Fire_Immolation', FIRE, DURATION_6S, 'Fire damage every second.'),
    strike(91247, 'Clinging Embers', 'Spell_Fire_Immolation', FIRE),
    strike(91248, 'Rebirth', 'Spell_Fire_SelfDestruct', FIRE),
    quickening(91249, 'Ashen Flock', 'Spell_Fire_Burnout', 'Your next Pyroblast casts 5% faster a stack.',
               DURATION_6S, PYROBLAST_FLAG, 5, stacks=3),
    look(91259, 'Rebirth', 'Spell_Fire_SelfDestruct', 0),
    # Astral Horizon.
    buff(91260, 'Rune of Astral Horizon', HORIZON_ICON,
         'Arcane Barrage also sets an Arcane Orb loose, at most once every 12 sec: it flies to your enemies, then '
         'for 6 sec fires a bolt every second for 20% of the Barrage\'s damage.', DURATION_INFINITE,
         {1: aura(AURA_DUMMY, 1, 0)}),
    {**shown(91261, 'Arcane Orb', HORIZON_ICON, '', DURATION_INFINITE,
             effects={1: aura(AURA_MOD_THREAT, -100, ALL_SCHOOLS)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_INFINITE}},
    missile(91262, 'Orb Bolt', 'Spell_Arcane_Arcane04', ARCANE, VISUAL_ARCANE_BARRAGE, 22.0),
    strike(91263, 'Wandering Star', 'Spell_Arcane_Arcane04', ARCANE, VISUAL_ARCANE_HIT),
    missile(91264, 'Prismatic Lance', 'Spell_Arcane_PrismaticCloak', ARCANE, VISUAL_ARCANE_BARRAGE, 45.0),
    strike(91265, 'Supernova', 'Spell_Arcane_Blast', ARCANE, VISUAL_ARCANE_HIT),
    ground(91266, 'Astral Remnant', 'Spell_Arcane_Arcane04', 'AstralRemnant', DURATION_6S),
    strike(91267, 'Astral Remnant', 'Spell_Arcane_Arcane04', ARCANE),
    {**buff(91268, 'Arcane Orb', HORIZON_ICON, 'An Arcane Orb circles you, firing at your enemies.', DURATION_6S,
            {1: aura(AURA_DUMMY, 1, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_6S}},
    look(91269, 'Supernova', 'Spell_Arcane_Blast', 0),
    # Guiding Light.
    buff(91280, 'Rune of Guiding Light', GUIDING_ICON,
         'Prayer of Mending places a Guiding Star on its target instead: for 12 sec it heals its bearer every 2 sec. '
         'Flash Heal or Greater Heal on another ally moves it there.', DURATION_INFINITE,
         {1: aura(AURA_DUMMY, 1, 0)}),
    {**shown(91281, 'Guiding Star', GUIDING_ICON, 'Healed every 2 sec by a Guiding Star.', DURATION_12S,
             key='GuidingStar'),
     'set': {**SHOWN, 'DurationIndex': DURATION_12S}},
    heal(91282, 'Guiding Star', GUIDING_ICON, 'The star\'s light.'),
    {**heal(91283, 'Guiding Star', GUIDING_ICON, 'The star arrives.'),
     'set': {**HEAL_SET, 'SchoolMask': HOLY, 'AttributesEx2': 0, 'SpellVisual': VISUAL_HOLY_SHOCK_HEAL}},
    look(91284, 'Guiding Star', GUIDING_ICON, VISUAL_PRAYER_JUMP, target=TARGET_UNIT_TARGET_ANY),
    absorb(91285, 'Sheltering Light', 'Spell_Holy_PowerWordShield', DURATION_6S),
    heal(91286, 'Gentle Passage', 'Spell_Holy_FlashHeal', 'The star passes by.'),
    periodic_heal(91287, 'Afterglow', 'Spell_Holy_Renew', 'Healing every second.', DURATION_8S),
    absorb(91288, 'Parting Grace', 'Spell_Holy_DivineSpirit', DURATION_6S),
    {**buff(91289, 'Parting Grace', 'Spell_Holy_DivineSpirit', 'Movement speed increased by 30%.', DURATION_3S,
            {1: aura(AURA_MOD_INCREASE_SPEED, 30, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_3S, 'SpellVisual': VISUAL_DASH}},
]
# Their Arcane Echo's, Frost Wound's and Clinging Embers' ticks, their strikes: no crits of their own.
for _spell in SPELLS:
    if _spell['id'] in (91204, 91225, 91245, 91247, 91263, 91267):
        _spell['set'] = {**_spell['set'], 'AttributesEx2': SPELL_ATTR2_CANT_CRIT}
# The crown shows how many Icicles wait (up to 5).
next(spell for spell in SPELLS if spell['id'] == 91222)['set']['StackAmount'] = 5
# Unbound Mind's missiles strike critically as Arcane Missiles' own do.
next(spell for spell in SPELLS if spell['id'] == 91207)['set']['AttributesEx2'] = 0
# Mental Reserves: a mage spell's modifier (its family's).
next(spell for spell in SPELLS if spell['id'] == 91206)['set']['SpellFamilyName'] = MAGE_FAMILY

NEW_MODIFIERS = [
]
SPELLS += [buff(first + slot * 3 + choice, name, icon, '', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)},
                hidden=True)
           for first, icon, modifiers in NEW_MODIFIERS
           for slot, names in enumerate(modifiers) for choice, name in enumerate(names)]
ICONS += [MIND_ICON, CROWN_ICON, PHOENIX_ICON, HORIZON_ICON, GUIDING_ICON, 'Spell_Arcane_MindMastery',
          'Spell_Nature_StarFall', 'Spell_Arcane_StarFire', 'Spell_Shadow_ManaBurn', 'Spell_Frost_IceShard',
          'Spell_Frost_FrostBolt02', 'Spell_Frost_IceStorm', 'Spell_Frost_FrostShock', 'Spell_Frost_ColdHearted',
          'Spell_Fire_Burnout', 'Spell_Fire_Fire', 'Spell_Fire_Immolation', 'Spell_Fire_SelfDestruct',
          'Spell_Arcane_Arcane04', 'Spell_Arcane_PrismaticCloak', 'Spell_Arcane_Blast',
          'Spell_Holy_PowerWordShield', 'Spell_Holy_FlashHeal', 'Spell_Holy_Renew', 'Spell_Holy_DivineSpirit']

VISUALS.update({
    # Insight full: Arcane Power's swirl and arcane light in the hands.
    'MindUnbound': {'copy': 32, 'id': 16791, 'clear': ('precast', 'cast', 'impact'), 'kits': {
        'state': (15730, {KIT_CHEST: FX_ARCANE_POWER_CHEST, KIT_LEFT_HAND: FX_MAGIC_HAND,
                          KIT_RIGHT_HAND: FX_MAGIC_HAND, KIT_BASE: 0, 'sound': None, 'procs': False})}},
    # Icicles waiting: Frostbolt's ice about the head (the crown).
    'FrostCrown': {'copy': 32, 'id': 16792, 'clear': ('precast', 'cast', 'impact'), 'kits': {
        'state': (15731, {KIT_HEAD: FX_ICE_CROWN, KIT_CHEST: 0, KIT_BASE: 0, 'sound': None, 'procs': False})}},
    # The Orb's remnant: Arcane Explosion's ring, lingering.
    'AstralRemnant': {'copy': 9735, 'id': 16793, 'clear': ('precast', 'cast', 'state', 'area'), 'kits': {
        'persistent': (15732, {KIT_BASE: FX_ARCANE_EXPLOSION_BASE, KIT_WORLD: 0, 'procs': False})}},
    # The Guiding Star on its bearer: holy light about the head.
    'GuidingStar': {'copy': 32, 'id': 16794, 'clear': ('precast', 'cast', 'impact'), 'kits': {
        'state': (15733, {KIT_HEAD: FX_HOLY_LIGHT_HEAD, KIT_CHEST: 0, KIT_BASE: 0, 'sound': None,
                          'procs': False})}},
    # Rebirth and Supernova: Living Bomb's explosion with a great fire hit; Arcane Explosion's ring.
    'RebirthLook': {'copy': 963, 'id': 16795, 'clear': ('precast', 'cast', 'state'), 'kits': {
        'impact': (15734, {KIT_WORLD: FX_LIVING_BOMB_AREA, KIT_CHEST: FX_FIRE_UBER_IMPACT, KIT_BASE: 0})}},
    'SupernovaLook': {'copy': 965, 'id': 16796, 'clear': ('precast', 'state'), 'kits': {
        'cast': (15735, {KIT_BASE: FX_ARCANE_EXPLOSION_BASE, KIT_LEFT_HAND: 0, KIT_RIGHT_HAND: 0})}},
})
# Retail's own (wow.export glTF, their bone animation kept; spell_effects.retail_effect): Icicle Mastery's icicle as
# the Icicles' missile, Arcane Orb's orb on the Orb, Phoenix Flames' phoenix on the phoenix (both on an invisible
# creature: the look follows it, the phoenix facing where it flies).
VISUAL_MISSILE_MODEL, VISUAL_MISSILE_SOUND, VISUAL_MISSILE_CAST_OFFSET = 8, 11, 26     # SpellVisual's columns
VISUAL_MISSILE_MOTION = 21
FROZEN_ORB_BOLT, ICE_LANCE_MISSILE_SOUND, ICICLE_DROP = 15082, 3256, 3.0
EFFECTS.update({
    # The icicle as a missile: exported_model's (the Fel Barrage ball's path, which flies; a missile built by the
    # creature converter showed nothing), from its glTF's mesh (gltf_to_wowexport.py) and a sidecar written for it.
    'IcicleMissile': {'model': 'MAGE_Icicle', 'effect': 7157, 'shape': 'missile', 'textures': (),
                      'options': {'export': 'MAGE_IcicleObj', 'base': 'mage_icicle', 'size': 1.3, 'center': True,
                                  'two_sided': True}},
    'ArcaneOrbRetail': {'model': 'MAGE_ArcaneOrb', 'effect': 7158, 'shape': 'retail', 'textures': (),
                        'options': {'export': 'MAGE_ArcaneOrb/mage_arcaneorb_missile_fallback.gltf', 'scale': 0.44,
                                    'center': True, 'spin': 3.0, 'lift': 1.2}},
    # Satellite's Orb: the same, circling whatever wears it 4 yards out (a turn every 4 sec), drawn by the client
    # itself (a creature steered around the Hero by the server stuttered).
    'ArcaneOrbSatellite': {'model': 'MAGE_ArcaneOrbSatellite', 'effect': 7160, 'shape': 'retail', 'textures': (),
                           'options': {'export': 'MAGE_ArcaneOrb/mage_arcaneorb_missile_fallback.gltf',
                                       'scale': 0.375, 'center': True, 'spin': 4.0, 'orbit': 4.0}},
    'PhoenixRetail': {'model': 'MAGE_PhoenixFlames', 'effect': 7159, 'shape': 'retail', 'textures': (),
                      'options': {'export': 'MAGE_PhoenixFlames/7fx_mage_phoenixsflames_missile.gltf', 'scale': 0.5,
                                  'center': True}},
})
VISUALS.update({
    # The Icicles: Fel Barrage's bolt (Frozen Orb's visual, flying straight), from just above the Hero's head (3
    # yards up, straight over them) down to their target, Ice Lance's sound and shards where they land.
    'IcicleBolt': {'copy': FROZEN_ORB_BOLT, 'id': 16797,
                   'clear': ('precast', 'cast', 'impact', 'state', 'statedone', 'area'),
                   'set': {VISUAL_MISSILE_MODEL: 'IcicleMissile', VISUAL_MISSILE_SOUND: ICE_LANCE_MISSILE_SOUND,
                           VISUAL_MISSILE_MOTION: 0, VISUAL_MISSILE_CAST_OFFSET: 0.0,
                           VISUAL_MISSILE_CAST_OFFSET + 1: 0.0, VISUAL_MISSILE_CAST_OFFSET + 2: ICICLE_DROP}, 'kits': {
                       'impact': (15738, {KIT_CHEST: FX_ICE_LANCE_IMPACT, 'sound': None})}},
    'ArcaneOrbForm': {'copy': 32, 'id': 16798, 'clear': ('precast', 'cast', 'impact'), 'kits': {
        'state': (15736, {KIT_BASE: 'ArcaneOrbRetail', KIT_CHEST: 0, 'sound': None, 'procs': False})}},
    'SatelliteForm': {'copy': 32, 'id': 16800, 'clear': ('precast', 'cast', 'impact'), 'kits': {
        'state': (15739, {KIT_CHEST: 'ArcaneOrbSatellite', KIT_BASE: 0, 'sound': None, 'procs': False})}},
    'PhoenixForm': {'copy': 32, 'id': 16799, 'clear': ('precast', 'cast', 'impact'), 'kits': {
        'state': (15737, {KIT_BASE: 'PhoenixRetail', KIT_CHEST: 0, 'sound': None, 'procs': False})}},
})
for _spell in SPELLS:
    _key = {91221: 'IcicleBolt', 91228: 'IcicleBolt', 91243: 'PhoenixForm', 91261: 'ArcaneOrbForm',
            91268: 'SatelliteForm'}.get(_spell['id'])
    if _key:
        _spell['visual'] = _key
for _spell in SPELLS:
    _key = {91259: 'RebirthLook', 91269: 'SupernovaLook'}.get(_spell['id'])
    if _key:
        _spell['visual'] = _key

# ---- Three druid runes (their own dossiers): Elune's Guardian 91300, Astral Convergence 91320, Ursoc's Champion
# 91340; each one's markers 10 on from its first (+ slot * 3 + choice). Astral Convergence's are the Extended Options'
# (91600 on, below); 91330-91338, its first nine, are retired.
ELUNE_ICON, CONVERGENCE_ICON = 'Ability_Rune_ElunesGuardian', 'Ability_Rune_AstralConvergence'
URSOC_ICON = 'Ability_Rune_UrsocsChampion'
DRUID_FAMILY = 7
WRATH_FLAG, STARFIRE_FLAG = 0x1, 0x4
AURA_MOD_HEALING_PCT = 118                  # healing taken, %
VISUAL_STARFALL_STAR, VISUAL_LUNAR_ECLIPSE, VISUAL_SOLAR_ECLIPSE = 11040, 11567, 12705
VISUAL_DEMORALIZING_ROAR = 3949


def druid_mod(spell_id, name, icon, text, duration, flag, mods, charges=0):
    """A druid spell's modifiers (family 7: the engine makes the client honour a Hero's): mods, (op, pct) pairs."""
    effects = {n + 1: {**aura(AURA_ADD_PCT_MODIFIER, -pct, op), 'Mask': flag} for n, (op, pct) in enumerate(mods)}
    spell = buff(spell_id, name, icon, text, duration, effects)
    spell['set'] = {**spell['set'], 'SpellFamilyName': DRUID_FAMILY, 'ProcCharges': charges}
    return spell


SPELLS += [
    # Elune's Guardian.
    buff(91300, "Rune of Elune's Guardian", ELUNE_ICON,
         'A Starfire that hits calls a lunar owl: 1 sec later it dives on the same enemy for 30% of the Starfire\'s '
         'damage. Up to 3 wait at once.', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    strike(91301, 'Lunar Owl', 'Ability_Hunter_Pet_Owl', ARCANE, VISUAL_STARFALL_STAR),
    dot(91302, 'Lunar Wound', 'Spell_Nature_Moonglow', ARCANE, DURATION_6S, 'Arcane damage every second.'),
    strike(91303, 'Lunar Wound', 'Spell_Nature_Moonglow', ARCANE),
    druid_mod(91304, 'Swift Judgment', ELUNE_ICON, 'Your next Starfire casts 10% faster.', DURATION_5S,
              STARFIRE_FLAG, [(SPELLMOD_CASTING_TIME, 10)], charges=1),
    druid_mod(91305, 'Lunar Flock', ELUNE_ICON, 'Your next Starfire costs 20% less mana.', DURATION_5S,
              STARFIRE_FLAG, [(SPELLMOD_COST, 20)], charges=1),
    druid_mod(91306, 'Guiding Wings', ELUNE_ICON, 'Your next Starfire is instant.', DURATION_5S, STARFIRE_FLAG,
              [(SPELLMOD_CASTING_TIME, 100)], charges=1),
    {**buff(91307, 'Guiding Wings', 'Ability_Druid_Dash', 'Movement speed increased by 20%.', DURATION_5S,
            {1: aura(AURA_MOD_INCREASE_SPEED, 20, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_5S}},
    harmful(91308, 'Twilight Hunt', 'Ability_Druid_TwilightsWrath',
            'Your next Wrath on this target deals 25% more damage.', DURATION_5S, aura(AURA_DUMMY, 1, 0), ARCANE),
    # Astral Convergence.
    buff(91320, 'Rune of Astral Convergence', CONVERGENCE_ICON,
         'Wrath leaves Sunlight; Starfire on it collides for 80% of your spell power and leaves Moonlight; Wrath on '
         'that flares for 40% and leaves Sunlight again.', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    harmful(91321, 'Sunlight', 'Ability_Druid_EclipseOrange', 'Your Starfire on this target collides with it.',
            DURATION_8S, aura(AURA_DUMMY, 1, 0), NATURE),
    harmful(91322, 'Moonlight', 'Ability_Druid_Eclipse', 'Your Wrath on this target flares.', DURATION_8S,
            aura(AURA_DUMMY, 1, 0), ARCANE),
    strike(91323, 'Lunar Collision', 'Ability_Druid_Eclipse', ARCANE, VISUAL_LUNAR_ECLIPSE),
    strike(91324, 'Solar Flare', 'Ability_Druid_EclipseOrange', NATURE, VISUAL_SOLAR_ECLIPSE),
    dot(91325, 'Astral Rupture', 'Ability_Druid_Eclipse', ARCANE, DURATION_6S, 'Arcane damage every second.'),
    strike(91326, 'Astral Rupture', 'Ability_Druid_Eclipse', ARCANE),
    druid_mod(91327, 'Steady Orbit', CONVERGENCE_ICON, 'Your next Wrath or Starfire is instant.', DURATION_5S,
              WRATH_FLAG | STARFIRE_FLAG, [(SPELLMOD_CASTING_TIME, 100)], charges=1),
    druid_mod(91328, 'Lunar Dominance', CONVERGENCE_ICON,
              'Your next Starfire casts 20% faster and costs 20% less mana.', DURATION_5S, STARFIRE_FLAG,
              [(SPELLMOD_CASTING_TIME, 20), (SPELLMOD_COST, 20)], charges=1),
    ground(91329, 'Eventide', 'Spell_Arcane_StarFire', 'AstralRemnant', DURATION_4S),
    strike(91339, 'Eventide', 'Spell_Arcane_StarFire', ARCANE),
    look(91360, 'Astral Rupture', 'Ability_Druid_Eclipse', VISUAL_LUNAR_ECLIPSE),
    # Ursoc's Champion.
    buff(91340, "Rune of Ursoc's Champion", URSOC_ICON,
         'Barkskin in Bear Form adds Ursoc\'s Guard: an absorb of 30% of your maximum health for 6 sec, and comes '
         'back 30 sec sooner. Maul in it releases a blow of 75% of your attack power and all it absorbed.',
         DURATION_INFINITE,
         {1: aura(AURA_DUMMY, 1, 0)}),
    absorb(91341, "Ursoc's Guard", URSOC_ICON, DURATION_6S),
    {**strike(91342, "Ursoc's Blow", 'Ability_Druid_Maul', PHYSICAL), 'visual': 'WakeAftershockLook'},
    look(91343, "Ursoc's Blow", URSOC_ICON, VISUAL_SHOCKWAVE),
    {**buff(91344, 'Lasting Stand', 'Spell_Nature_StoneClawTotem', 'Healing received increased by 25%.', DURATION_10S,
            {1: aura(AURA_MOD_HEALING_PCT, 25, ALL_SCHOOLS)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_10S}},
    {**buff(91345, 'Moving Mountain', 'Ability_Druid_Dash', 'Movement speed increased by 40%.', DURATION_6S,
            {1: aura(AURA_MOD_INCREASE_SPEED, 40, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_6S}},
    absorb(91346, 'Enduring Champion', URSOC_ICON, DURATION_8S),
    harmful(91347, "Challenger's Roar", 'Ability_Druid_ChallangingRoar', 'Damage done reduced by 20%.', DURATION_6S,
            aura(AURA_MOD_DAMAGE_PERCENT_DONE, -20, ALL_SCHOOLS), PHYSICAL),
    look(91359, "Mountain's Roar", 'Ability_Druid_DemoralizingRoar', VISUAL_DEMORALIZING_ROAR),
]
for _spell in SPELLS:
    if _spell['id'] in (91301, 91303, 91323, 91324, 91326, 91339, 91342):
        _spell['set'] = {**_spell['set'], 'AttributesEx2': SPELL_ATTR2_CANT_CRIT}

DRUID_MODIFIERS = [
]
SPELLS += [buff(first + slot * 3 + choice, name, icon, '', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)},
                hidden=True)
           for first, icon, modifiers in DRUID_MODIFIERS
           for slot, names in enumerate(modifiers) for choice, name in enumerate(names)]
ICONS += [ELUNE_ICON, CONVERGENCE_ICON, URSOC_ICON, 'Ability_Hunter_Pet_Owl', 'Spell_Nature_Moonglow',
          'Ability_Druid_Dash', 'Ability_Druid_TwilightsWrath', 'Ability_Druid_EclipseOrange', 'Ability_Druid_Eclipse',
          'Ability_Druid_Maul', 'Spell_Nature_StoneClawTotem', 'Ability_Druid_ChallangingRoar',
          'Ability_Druid_DemoralizingRoar']

# ---- The Duelist 91380 and the Stormreaver 91400 (their own dossiers); each one's markers 10 on from its first.
DUELIST_ICON, STORMREAVER_ICON = 'Ability_Rune_Duelist', 'Ability_Rune_Stormreaver'
ROGUE_FAMILY = 8
SPELL_ATTR1_FINISHING_MOVE_DAMAGE = 0x00100000      # a finishing move: needs combo points and spends them
# The spellbook tab of the abilities a rune teaches (build_patch.py: our spells are the Demon Hunter's otherwise).
SPELL_CLASSES = {91370: 'ROGUE', 91371: 'ROGUE', 91382: 'ROGUE'}
# Sabre Slash's hit: designed in the Emberforge Effect Suite (spells/ROGUE_SabreSlash/Sabre_Slash.json, built by
# models/suite_effect.py): a gold slash flashing across the target, a spray of sparks through it, a soft glow; and
# retail's Sabre Slash sound (sounds/spell_ro_saberslash_cast_01).
EFFECTS.update({
    'SabreSlashImpact': {'model': 'ROGUE_SabreSlashImpact', 'effect': 7170, 'shape': 'suite', 'textures': (),
                         'options': {'spec': 'ROGUE_SabreSlash/Sabre_Slash.json'}},
})
SOUNDS = {
    'SabreSlash': {'id': 18136, 'copy': 3091, 'files': ['spell_ro_saberslash_cast_01']},
    # Explosive Shot's impact entry (13590) with the Suite design's explosion (its volume 0.77 and pitch 1.2 baked in).
    'BroadsideBlast': {'id': 18137, 'copy': 13590, 'files': ['fx_grenade_explosion01']},
}
# Broadside's blasts: a cannonball lobbed at each enemy (Blizzard's catapult ball missile at 0.35 and its arcing
# missile motion, as their cannons fire it), bursting as designed in the Effect Suite
# (spells/ROGUE_Broadside/Broadside_Impact.json): fire rings and a scorch on the ground, smoke, sparks and debris.
CANNONBALL_MISSILE, CANNON_LOB = 3778, 701
EFFECTS.update({
    'BroadsideImpact': {'model': 'ROGUE_BroadsideImpact', 'effect': 7171, 'shape': 'suite', 'textures': (),
                        'options': {'spec': 'ROGUE_Broadside/Broadside_Impact.json', 'slot': 'chest'}},
    'BroadsideGround': {'model': 'ROGUE_BroadsideGround', 'effect': 7172, 'shape': 'suite', 'textures': (),
                        'options': {'spec': 'ROGUE_Broadside/Broadside_Impact.json', 'slot': 'base'}},
})
VISUALS.update({
    'BroadsideBlast': {'copy': 11828, 'id': 16802, 'set': {VISUAL_MISSILE_MODEL: CANNONBALL_MISSILE,
                                                         VISUAL_MISSILE_MOTION: CANNON_LOB}, 'kits': {
        'impact': (15741, {KIT_CHEST: 'BroadsideImpact', KIT_BASE: 'BroadsideGround', 'sound': 'BroadsideBlast'})}},
    'SabreSlashHit': {'copy': 253, 'id': 16801, 'clear': ('state', 'statedone', 'area'), 'kits': {
        'impact': (15740, {KIT_CHEST: 'SabreSlashImpact', 'sound': 'SabreSlash'})}},
})
VISUAL_GUNSHOT, VISUAL_SINISTER_STRIKE, VISUAL_THUNDERSTORM, VISUAL_LIGHTNING_BOLT = 8113, 253, 11302, 173
RANGE_20, GCD_CATEGORY = 3, 133
PISTOL_ICON = 'Ability_Rogue_PistolShot'
PISTOL_SHOT = action(91382, 'Pistol Shot', PISTOL_ICON,
                   'Fire a conjured pistol at an enemy within 20 yards: 60% of your attack power as Physical damage '
                   'and 1 combo point. Needs Opening Shot (every third Sabre Slash).', 6000,
                   target=TARGET_UNIT_TARGET_ENEMY, range_index=RANGE_20)
PISTOL_SHOT['set'] = {**PISTOL_SHOT['set'], 'PowerType': POWER_ENERGY, 'ManaCost': 10,
                    'StartRecoveryCategory': GCD_CATEGORY, 'StartRecoveryTime': 1000, 'CasterAuraSpell': 91381}
PISTOL_SHOT['tooltip'] = PISTOL_SHOT['description']
PISTOL_SHOT['unset'] = {'Attributes': 0x10000}      # Evasion's "not while shapeshifted": usable in stealth or a form
BROADSIDE = action(91371, 'Broadside', 'INV_Weapon_Rifle_01',
                   'Finishing move: three pistol blasts explode on an enemy within 20 yards, each dealing Fire damage '
                   '(30% of your attack power, plus 5% per combo point) to it and up to 7 other enemies within 8 '
                   'yards. A blast deals:\r\n'
                   '   1 point  : ${$AP*35/100} damage\r\n'
                   '   2 points: ${$AP*40/100} damage\r\n'
                   '   3 points: ${$AP*45/100} damage\r\n'
                   '   4 points: ${$AP*50/100} damage\r\n'
                   '   5 points: ${$AP*55/100} damage',
                   20000, target=TARGET_UNIT_TARGET_ENEMY, range_index=RANGE_20)
BROADSIDE['set'] = {**BROADSIDE['set'], 'PowerType': POWER_ENERGY, 'ManaCost': 35,
                    'StartRecoveryCategory': GCD_CATEGORY, 'StartRecoveryTime': 1000,
                    'AttributesEx': SPELL_ATTR1_FINISHING_MOVE_DAMAGE}
BROADSIDE['tooltip'] = BROADSIDE['description']
BROADSIDE['unset'] = {'Attributes': 0x10000}
VISUAL_EXPLOSIVE_SHOT, VISUAL_SAPPER_CHARGE, SINISTER_STRIKE = 11828, 2899, 1752
SPELLS += [
    # The Duelist: Sabre Slash (Sinister Strike's first rank, its own look; the server adds the known rank's bonus),
    # Broadside and its blasts.
    {'id': 91370, 'copy': SINISTER_STRIKE, 'name': 'Sabre Slash', 'icon': 'Ability_Rogue_SabreSlash',
     'description': 'A sabre slash: your Sinister Strike\'s damage and 1 combo point. Every third hit readies '
                    'Opening Shot.',
     'tooltip': '', 'visual': 'SabreSlashHit', 'set': {'SpellFamilyName': ROGUE_FAMILY}},
    BROADSIDE,
    {**strike(91372, 'Broadside', 'INV_Weapon_Rifle_01', FIRE, VISUAL_EXPLOSIVE_SHOT), 'visual': 'BroadsideBlast',
     'set': {**strike(91372, '', '', FIRE, VISUAL_EXPLOSIVE_SHOT)['set'], 'Speed': 30.0}},
    look(91373, 'Broadside', 'INV_Weapon_Rifle_01', VISUAL_SAPPER_CHARGE),
    # The Duelist.
    buff(91380, 'Rune of the Duelist', DUELIST_ICON,
         'Sinister Strike becomes Sabre Slash. Every second Sabre Slash that hits readies Opening Shot: your Pistol '
         'Shot, 60% of your attack power and a combo point. Broadside spends combo points in pistol blasts.',
         DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    {**buff(91381, 'Opening Shot', PISTOL_ICON, 'Pistol Shot is ready.', DURATION_8S,
            {1: aura(AURA_DUMMY, 1, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_8S}},
    PISTOL_SHOT,
    {**strike(91383, 'Pistol Shot', PISTOL_ICON, PHYSICAL, VISUAL_GUNSHOT)},
    dot(91384, 'Serrated Edge', 'Ability_Rogue_Rupture', PHYSICAL, DURATION_6S, 'Bleeding every second.'),
    strike(91385, 'Serrated Edge', 'Ability_Rogue_Rupture', PHYSICAL),
    # Rapid Shots (91386; Advance, Disengaging Shot and Press the Advantage, 91386-91389, retired with Footwork).
    {**buff(91386, 'Rapid Shots', PISTOL_ICON, 'Your next Pistol Shot grants 50 Energy instead of costing it.',
            DURATION_10S, {1: aura(AURA_DUMMY, 1, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_10S}},
    {**strike(91399, 'Sabre Slash', 'Ability_Rogue_SabreSlash', PHYSICAL), 'visual': 'SabreSlashHit'},
    # The Stormreaver.
    buff(91400, 'Rune of the Stormreaver', STORMREAVER_ICON,
         'Stormstrike raises the Stormfront for 6 sec: every 2 sec, 25% of your attack power as Nature damage to up '
         'to 5 enemies within 8 yards. In it, Lava Lash becomes Thunderlash and your melee abilities discharge '
         'Lightning Shield.', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    {**buff(91401, 'Stormfront', 'Spell_Shaman_ThunderStorm',
            'A storm strikes the enemies around you. Lava Lash is Thunderlash; your melee abilities discharge '
            'Lightning Shield, which keeps its charges.', DURATION_6S, {1: aura(AURA_DUMMY, 1, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_6S, 'SpellVisual': VISUAL_THUNDERSTORM}},
    strike(91402, 'Stormfront', 'Spell_Shaman_ThunderStorm', NATURE, VISUAL_LIGHTNING_BOLT),
    strike(91403, 'Thunderlash', 'Ability_Shaman_LavaLash', NATURE, VISUAL_THUNDER_HIT),
    strike(91404, 'Thunderlash', 'Spell_Nature_ChainLightning', NATURE, VISUAL_THUNDER_HIT),
    dot(91405, 'White-Hot Conductor', 'Spell_Fire_Immolation', FIRE, DURATION_6S, 'Fire damage every second.'),
    strike(91406, 'White-Hot Conductor', 'Spell_Fire_Immolation', FIRE),
    strike(91407, 'Lightning Shield', 'Spell_Nature_LightningShield', NATURE, VISUAL_THUNDER_HIT),
    {**buff(91408, 'Ride the Lightning', 'Spell_Nature_Lightning', 'Movement speed increased by 30%.', DURATION_3S,
            {1: aura(AURA_MOD_INCREASE_SPEED, 30, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_3S}},
]
for _spell in SPELLS:
    if _spell['id'] == 91383:
        _spell['set'] = {**_spell['set'], 'Speed': 60.0}
    if _spell['id'] in (91372, 91383, 91385, 91399, 91402, 91403, 91404, 91406, 91407):
        _spell['set'] = {**_spell['set'], 'AttributesEx2': SPELL_ATTR2_CANT_CRIT}
# Its procs: every melee ability that hits; the script discharges the shield (once a second).
for _spell in SPELLS:
    if _spell['id'] == 91400:
        _spell['set'] = {**_spell['set'], 'ProcFlags': PROC_DONE_MELEE_SPELL, 'ProcChance': 101}
# The Stormreaver's markers: the Extended Options' block (93075, below); 91410-91418 retired.
ICONS += ['Ability_Rogue_SabreSlash', PISTOL_ICON, DUELIST_ICON, STORMREAVER_ICON, 'INV_Weapon_Rifle_01',
          'Ability_Rogue_Rupture',
          'Ability_Hunter_RunningShot',
          'Ability_Rogue_SinisterCalling', 'Spell_Shaman_ThunderStorm', 'Ability_Shaman_LavaLash',
          'Spell_Fire_Immolation', 'Spell_Nature_LightningShield', 'Spell_Nature_Lightning']

# ---- Ember's Wrath 91430 and Whitefang 91450 (their own dossiers), their markers 1 on. The Hero's own Fire Blast
# and Icy Touch work in Cat Form at their own costs (Fire Blast's form rule: patch_spells.RUNE_FREED, the server
# keeping it for everyone but a Cat with the rune); 91420 and 91440, the Cat Form copies they first taught, are gone.
EMBERS_ICON, WHITEFANG_ICON = 'Ability_Rune_EmbersWrath', 'Ability_Rune_Whitefang'
VISUAL_FIRE_BLAST, VISUAL_SCORCH, VISUAL_ICE_LANCE, VISUAL_FROST_TRAP = 143, 945, 7906, 3759
AURA_MOD_DODGE_PERCENT = 49
_SPEED = {1: aura(AURA_MOD_INCREASE_SPEED, 30, 0)}
SPELLS += [
    {**dot(91421, 'Cinder Wound', 'Spell_Fire_Incinerate', FIRE, DURATION_10S, 'Fire damage every 2 sec.')},
    strike(91422, 'Cinder Wound', 'Spell_Fire_Incinerate', FIRE),
    {**dot(91423, 'Cinder Wound', 'Spell_Fire_Incinerate', FIRE, DURATION_10S, 'Fire damage every 2 sec.')},
    strike(91424, 'Wild Embers', 'Spell_Fire_Fireball', FIRE, VISUAL_FIRE_BLAST),
    strike(91425, 'Blazing Rake', 'Ability_Druid_Disembowel', FIRE, VISUAL_SCORCH),
    strike(91426, 'Molten Bite', 'Ability_Druid_FerociousBite', FIRE, VISUAL_SCORCH),
    {**buff(91427, 'Ember Rush', 'Spell_Fire_BurningSpeed', 'Movement speed increased by 20%.', DURATION_3S,
            {1: aura(AURA_MOD_INCREASE_SPEED, 20, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_3S}},
    {**buff(91428, 'Ember Rush', 'Spell_Fire_BurningSpeed', 'Your next Shred gives 10 Energy back.',
            DURATION_5S, {1: aura(AURA_DUMMY, 1, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_5S}},
    buff(91430, "Rune of Ember's Wrath", EMBERS_ICON,
         'Your Fire Blast works in Cat Form. On your Rake or Rip victim it ignites Cinder Wound: 150% of your attack '
         'power plus 100% of your spell power as Fire damage over 6 sec.', DURATION_INFINITE,
         {1: aura(AURA_DUMMY, 1, 0)}),
    harmful(91441, 'Frozen Prey', 'Spell_Frost_FrostShock', 'Marked: the next Shred or Mangle shatters it.',
            DURATION_10S, aura(AURA_DUMMY, 1, 0), FROST),
    harmful(91442, 'Frozen Prey', 'Spell_Frost_FrostShock', 'Marked: the next Shred or Mangle shatters it.',
            DURATION_6S, aura(AURA_DUMMY, 1, 0), FROST),
    strike(91443, 'Shatter', 'Spell_Frost_FrostBlast', FROST, VISUAL_ICE_LANCE),
    dot(91444, 'Winter Wound', 'Spell_Frost_ChainsOfIce', FROST, DURATION_6S, 'Frost damage every second.'),
    strike(91445, 'Winter Wound', 'Spell_Frost_ChainsOfIce', FROST),
    {**buff(91446, 'Swift Hunt', 'Spell_Frost_ArcticWinds', 'Movement speed increased by 20%.', DURATION_3S,
            {1: aura(AURA_MOD_INCREASE_SPEED, 20, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_3S}},
    harmful(91447, 'Snowblind', 'Spell_Frost_FrostNova', 'Movement slowed by 30%.', DURATION_3S,
            aura(AURA_MOD_DECREASE_SPEED, -30, 0), FROST),
    {**buff(91448, 'Snowblind', 'Spell_Frost_FrostNova', 'Dodge chance increased by 15%.', DURATION_3S,
            {1: aura(AURA_MOD_DODGE_PERCENT, 15, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_3S}},
    buff(91450, 'Rune of Whitefang', WHITEFANG_ICON,
         'Your Icy Touch, in Cat Form too, lays Frozen Prey: your next Shred or Mangle on the victim shatters it for '
         '150% of your attack power as Frost damage.', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    strike(91460, 'Frozen Trail', 'Spell_Frost_FrostNova', FROST),
    harmful(91461, 'Frozen Trail', 'Spell_Frost_FrostNova', 'Movement slowed by 30%.', DURATION_2S,
            aura(AURA_MOD_DECREASE_SPEED, -30, 0), FROST),
    {**buff(91462, 'Cold Blood', 'Spell_Frost_ColdHearted',
            'Your next Ferocious Bite on the shattered enemy deals 20% more damage.', DURATION_5S,
            {1: aura(AURA_DUMMY, 1, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_5S}},
]
# The shocks' empowered strikes: Flame Shock's Kindled (Blazing Strike, its bleed; Fiery Explosion, a Fire Nova),
# Frost Shock's Frostbitten (Frostfang Strike, Frostbite and its slow; Glacial Burst, a Frost Nova, and its slow).
VISUAL_FROST_SHOCK, VISUAL_FIRE_NOVA, VISUAL_FROST_NOVA = 144, 14908, 17
SPELLS += [
    {**buff(91463, 'Kindled', 'Spell_Fire_FlameShock',
            'Your next Claw is a Blazing Strike; your next Swipe a Fiery Explosion.', DURATION_10S,
            {1: aura(AURA_DUMMY, 1, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_10S}},
    strike(91464, 'Blazing Strike', 'Spell_Fire_FlameShock', FIRE, VISUAL_FIRE_BLAST),
    dot(91465, 'Blazing Bleed', 'Ability_Druid_Rake', PHYSICAL, DURATION_6S, 'Bleeding every second.'),
    strike(91466, 'Blazing Bleed', 'Ability_Druid_Rake', PHYSICAL),
    strike(91467, 'Fiery Explosion', 'Spell_Fire_SealOfFire', FIRE, VISUAL_SCORCH),
    {**buff(91468, 'Frostbitten', 'Spell_Frost_FrostShock',
            'Your next Claw is a Frostfang Strike; your next Swipe a Glacial Burst.', DURATION_10S,
            {1: aura(AURA_DUMMY, 1, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_10S}},
    strike(91469, 'Frostfang Strike', 'Spell_Frost_FrostShock', FROST, VISUAL_FROST_SHOCK),
    dot(91470, 'Frostbite', 'Spell_Frost_FrostArmor', FROST, DURATION_6S, 'Frost damage every second.'),
    strike(91471, 'Frostbite', 'Spell_Frost_FrostArmor', FROST),
    harmful(91472, 'Frostbite', 'Spell_Frost_FrostArmor', 'Movement slowed by 30%.', DURATION_6S,
            aura(AURA_MOD_DECREASE_SPEED, -30, 0), FROST),
    strike(91473, 'Glacial Burst', 'Spell_Frost_FrostNova', FROST, VISUAL_ICE_LANCE),
    harmful(91474, 'Glacial Burst', 'Spell_Frost_FrostNova', 'Movement slowed by 50%.', DURATION_4S,
            aura(AURA_MOD_DECREASE_SPEED, -50, 0), FROST),
    look(91475, 'Fiery Explosion', 'Spell_Fire_SealOfFire', VISUAL_FIRE_NOVA),
    look(91476, 'Glacial Burst', 'Spell_Frost_FrostNova', VISUAL_FROST_NOVA),
]
for _spell in SPELLS:
    if _spell['id'] in (91464, 91466, 91467, 91469, 91471, 91473):
        _spell['set'] = {**_spell['set'], 'AttributesEx2': SPELL_ATTR2_CANT_CRIT}
ICONS += ['Spell_Fire_FlameShock', 'Spell_Fire_SealOfFire', 'Spell_Frost_FrostArmor', 'Ability_Druid_Rake']
# The empowered Swipes' looks, designed in the Emberforge Effect Suite (spells/DRUID_CatSwipe: Swipe (Fire Cat),
# Swipe (Frost Cat)): their flames and bits on each enemy Fiery Explosion or Glacial Burst hits (slot 'chest'), and
# their sparks at the cat's own feet with the impact sound (slot 'caster', on the look the cat casts on itself). The
# Swipe's own arc (SwipeCaster) plays as ever; its tint in the Suite is a Game model layer, not carried yet.
EFFECTS.update({
    'FieryExplosionImpact': {'model': 'DRUID_FieryExplosionImpact', 'effect': 7173, 'shape': 'suite', 'textures': (),
                             'options': {'spec': 'DRUID_CatSwipe/Swipe_Fire_Cat.json'}},
    'GlacialBurstImpact': {'model': 'DRUID_GlacialBurstImpact', 'effect': 7174, 'shape': 'suite', 'textures': (),
                           'options': {'spec': 'DRUID_CatSwipe/Swipe_Frost_Cat.json'}},
    'FieryExplosionCaster': {'model': 'DRUID_FieryExplosionCaster', 'effect': 7175, 'shape': 'suite', 'textures': (),
                             'options': {'spec': 'DRUID_CatSwipe/Swipe_Fire_Cat.json', 'slot': 'caster'}},
    'GlacialBurstCaster': {'model': 'DRUID_GlacialBurstCaster', 'effect': 7176, 'shape': 'suite', 'textures': (),
                           'options': {'spec': 'DRUID_CatSwipe/Swipe_Frost_Cat.json', 'slot': 'caster'}},
})
# Blazing Bleed's look (spells/DRUID_CatSwipe/Blazing_Bleed.json): flames streaming from the bleeding enemy's
# chest, looping for as long as the bleed lasts (its state kit).
EFFECTS['BlazingBleedState'] = {
    'model': 'DRUID_BlazingBleedState', 'effect': 7177, 'shape': 'suite', 'textures': (),
    'options': {'spec': 'DRUID_CatSwipe/Blazing_Bleed.json', 'loop': True}}
VISUALS['BlazingBleedBurn'] = {
    'copy': 253, 'id': 16807, 'clear': ('precast', 'cast', 'impact', 'statedone', 'area'),
    'kits': {'state': (15746, {KIT_CHEST: 'BlazingBleedState', 'sound': None, 'procs': False})}}
SWIPE_IMPACT_SOUND = 3134                      # the Suite effects' impact sound (SoundEntries)
_LOOK_CLEAR = ('precast', 'cast', 'state', 'statedone', 'area')
VISUALS.update({
    'FieryExplosionHit': {'copy': 253, 'id': 16803, 'clear': _LOOK_CLEAR, 'kits': {
        'impact': (15742, {KIT_CHEST: 'FieryExplosionImpact', 'sound': None})}},
    'GlacialBurstHit': {'copy': 253, 'id': 16804, 'clear': _LOOK_CLEAR, 'kits': {
        'impact': (15743, {KIT_CHEST: 'GlacialBurstImpact', 'sound': None})}},
    'FieryExplosionCast': {'copy': 253, 'id': 16805, 'clear': _LOOK_CLEAR, 'kits': {
        'impact': (15744, {KIT_BASE: 'FieryExplosionCaster', KIT_CHEST: 0, 'sound': SWIPE_IMPACT_SOUND})}},
    'GlacialBurstCast': {'copy': 253, 'id': 16806, 'clear': _LOOK_CLEAR, 'kits': {
        'impact': (15745, {KIT_BASE: 'GlacialBurstCaster', KIT_CHEST: 0, 'sound': SWIPE_IMPACT_SOUND})}},
})
for _spell in SPELLS:
    _look = {91465: 'BlazingBleedBurn', 91467: 'FieryExplosionHit', 91473: 'GlacialBurstHit',
             91475: 'FieryExplosionCast', 91476: 'GlacialBurstCast'}.get(_spell['id'])
    if _look:
        _spell['visual'] = _look
# Frozen Trail's patch: Frost Trap's frost on the ground.
_trail = ground(91449, 'Frozen Trail', 'Spell_Frost_FrostNova', None, DURATION_4S)
_trail.pop('visual')
_trail['set']['SpellVisual'] = VISUAL_FROST_TRAP
SPELLS.append(_trail)
for _spell in SPELLS:
    if _spell['id'] in (91421, 91423):          # Cinder Wound ticks every 2 sec
        _spell['effects'][1]['Amplitude'] = 2000
    if _spell['id'] in (91422, 91424, 91425, 91426, 91443, 91445, 91460):
        _spell['set'] = {**_spell['set'], 'AttributesEx2': SPELL_ATTR2_CANT_CRIT}
ICONS += [EMBERS_ICON, WHITEFANG_ICON, 'Spell_Fire_Fireball', 'Spell_Fire_Incinerate', 'Ability_Druid_Disembowel',
          'Ability_Druid_FerociousBite', 'Spell_Fire_BurningSpeed', 'Spell_DeathKnight_IceTouch',
          'Spell_Frost_FrostShock', 'Spell_Frost_FrostBlast', 'Spell_Frost_ChainsOfIce', 'Spell_Frost_ArcticWinds',
          'Spell_Frost_FrostNova', 'Spell_Frost_ColdHearted']

RUNE_AURAS = {90700, 90730, 90750, 90770, 90790, 90810, 90830, 90860, 90890, 90920, 90950, 90980, 91130, 91200, 91220,
              91240, 91260, 91280, 91300, 91320, 91340, 91380, 91400, 91430, 91450}
for _spell in SPELLS:
    if _spell['id'] in RUNE_AURAS:
        _spell['set'] = {**_spell['set'], 'Attributes': HIDDEN['Attributes']}

# The spells that wear the new hits and bursts.
for _spell in SPELLS:
    _key = {90774: 'ThunderEchoHitLook', 90775: 'ThunderBurstLook', 90864: 'WakeAftershockLook',
            90954: 'SunFractureLook', 90956: 'SunCollapseLook'}.get(_spell['id'])
    if _key:
        _spell['visual'] = _key

# ---- The Tank Collection (its own compendium): Red Communion 91490, Primal Worldwarden 91510, Black Covenant 91530,
# their markers 1 on; the collection's Tank Foundation 91543 (NPC attacks never critically strike) and Challenge 91542.
COMMUNION_ICON, WORLDWARDEN_ICON, COVENANT_ICON = ('Ability_Rune_RedCommunion', 'Ability_Rune_PrimalWorldwarden',
                                                   'Ability_Rune_BlackCovenant')
TAUNT = 355
LASH_ICON, SIPHON_ICON = 'Ability_Covenant_Lash', 'Ability_Covenant_SoulSiphon'
DRAIN_LIFE = 689
RANGE_10, RANGE_30 = 7, 4
DURATION_30MIN, DURATION_30S = 30, 9
CAST_2S = 5
AURA_MOD_INCREASE_HEALTH_PERCENT, AURA_MOD_BASE_RESISTANCE_PCT, AURA_MOD_RESISTANCE = 133, 142, 22
AURA_MOD_ATTACKER_SPELL_CRIT, AURA_MOD_ATTACKER_MELEE_CRIT, AURA_MOD_ATTACKER_RANGED_CRIT = 179, 187, 188
SPELL_ATTR5_ALLOW_ACTION_DURING_CHANNEL = 0x1
SHADOWFLAME = SHADOW | FIRE
MAGIC_SCHOOLS = 0x7E
EFFECT_APPLY_AURA = 6
VISUAL_SHADOW_BOLT_HIT, VISUAL_BLOOD_BOIL = 64, 13005


def tank_action(spell_id, name, icon, text, cooldown, gcd=False, cast=1, target=TARGET_UNIT_CASTER, range_index=1):
    spell = action(spell_id, name, icon, text, cooldown, target, range_index)
    spell['set'] = {**spell['set'], 'CastingTimeIndex': cast}
    if gcd:
        spell['set'].update({'StartRecoveryCategory': GCD_CATEGORY, 'StartRecoveryTime': 1500})
    spell['tooltip'] = ''
    # Usable in any form or stance (the template's "not while shapeshifted" kept a tank in Defensive Stance or Bear
    # Form from casting them at all; Bind: Voidwalker refuses a form itself, on the server).
    spell['unset'] = {'Attributes': 0x10000}
    return spell


SPELLS += [
    # Red Communion: the link, its pulse, Sanguine Recall and its blow, ward and heal, Shared Weakness, Arterial Echo.
    harmful(91480, 'Crimson Link', 'Spell_DeathKnight_BloodBoil', 'Linked by blood.', DURATION_15S,
            aura(AURA_DUMMY, 1, 0), SHADOW),
    strike(91481, 'Communion Pulse', 'Spell_DeathKnight_BloodBoil', SHADOW, VISUAL_SHADOW_BOLT_HIT),
    tank_action(91482, 'Sanguine Recall', COMMUNION_ICON,
                'Each enemy linked to you within 20 yards takes 30% of your attack power as Shadow damage, and you '
                'gain an absorb of 3% of your maximum health plus 0.5% for each further enemy, up to 5%, for 5 sec. '
                'Needs a linked enemy in range.', 18000),
    strike(91483, 'Sanguine Recall', COMMUNION_ICON, SHADOW, VISUAL_SHADOW_BOLT_HIT),
    absorb(91484, 'Sanguine Ward', COMMUNION_ICON, DURATION_5S),
    heal(91485, 'Blood Reclamation', 'Spell_DeathKnight_DeathStrike', ''),
    harmful(91486, 'Shared Weakness', 'Spell_Shadow_ShadowWordPain', 'Magic damage dealt to the warden reduced.',
            DURATION_4S, aura(AURA_DUMMY, 1, 0), SHADOW),
    harmful(91487, 'Arterial Echo', 'Spell_DeathKnight_BloodPlague', 'Shadow damage every second.', DURATION_4S,
            {**aura(AURA_PERIODIC_DAMAGE, 0, 0), 'Amplitude': 1000}, SHADOW),
    # Blood Boil while bound: the client checks a Blood rune itself ("Not enough runes") whatever the server
    # waives, so the rune puts this in its place: Rage and a cooldown, the script casting the Hero's Blood Boil.
    tank_action(91544, 'Blood Boil', 'Spell_DeathKnight_BloodBoil',
                'Boils the blood of all enemies within 10 yards (your Blood Boil), linking up to 5 of them to you. '
                'Costs Rage instead of a Blood rune.', 6000, gcd=True),
    buff(91490, 'Rune of Red Communion', COMMUNION_ICON,
         'Blood Boil links up to 5 enemies to you; Blood Strike on a linked enemy heals you and pulses to the rest.',
         DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    # Primal Worldwarden: the totem and its anchor, Worldguard, the three commands, Magma Lash, Striding Heart and
    # Stoneblood Lash.
    tank_action(91500, 'Worldheart Totem', WORLDWARDEN_ICON,
                'Summons the Worldheart Totem for 30 min: it follows 4 yards behind you. With a shield on, it gives '
                'you Worldguard: 8% less damage taken and 200% more threat. Your Earth, Flame and Frost Shocks '
                'command it.', 6000, gcd=True),
    tank_action(91501, 'Place Worldheart', 'Spell_Nature_StoneClawTotem',
                'Moves the Worldheart Totem to your target, or to you, within 20 yards and anchors it there for 6 '
                'sec.', 8000),
    {**buff(91502, 'Worldguard', WORLDWARDEN_ICON, 'Damage taken reduced; threat increased.', DURATION_INFINITE,
            {1: aura(AURA_MOD_THREAT, 200, ALL_SCHOOLS), 2: aura(AURA_MOD_DAMAGE_PERCENT_TAKEN, -8, ALL_SCHOOLS)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_INFINITE}},
    absorb(91503, 'Earthen Command', 'Spell_Nature_EarthShock', DURATION_4S),
    strike(91504, 'Flame Command', 'Spell_Fire_FlameShock', FIRE, VISUAL_FIRE_NOVA),
    harmful(91505, 'Frost Command', 'Spell_Frost_FrostShock', 'Movement slowed; damage to the warden reduced.',
            DURATION_4S, aura(AURA_MOD_DECREASE_SPEED, -35, 0), FROST),
    strike(91506, 'Magma Lash', 'Ability_Shaman_LavaLash', FIRE, VISUAL_SCORCH),
    {**buff(91508, 'Striding Heart', 'Spell_Nature_EarthBindTotem', 'Movement speed increased by 25%.', DURATION_3S,
            {1: aura(AURA_MOD_INCREASE_SPEED, 25, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_3S}},
    heal(91509, 'Stoneblood Lash', 'Ability_Shaman_LavaLash', ''),
    buff(91510, 'Rune of the Primal Worldwarden', WORLDWARDEN_ICON,
         'The Worldheart Totem follows you; your Shocks command it, Stormstrike echoes the last command and Lava Lash '
         'draws a magma line to it.', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    # Black Covenant: Bind Guardian, Covenant Armor and its pact, Covenant Lash, Soul Siphon, its ward and slow,
    # Dark Intercession.
    tank_action(91520, 'Bind: Voidwalker', 'Spell_Shadow_SummonVoidWalker',
                'Binds a Voidwalker to you as Covenant Armor until you cast this again: armor from items 400% higher, '
                '15% more maximum health, 200% more threat, 6% less damage taken. Searing Pain becomes Covenant Lash '
                'and Drain Life Soul Siphon.', 10000, cast=CAST_2S),
    {**buff(91521, 'Covenant Armor', COVENANT_ICON,
            'Armor from items, maximum health and threat increased; damage taken reduced.', DURATION_INFINITE,
            {1: aura(AURA_MOD_BASE_RESISTANCE_PCT, 400, 1), 2: aura(AURA_MOD_INCREASE_HEALTH_PERCENT, 15, 0),
             3: aura(AURA_MOD_DAMAGE_PERCENT_TAKEN, -6, ALL_SCHOOLS)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_INFINITE}},
    buff(91522, 'Covenant Pact', COVENANT_ICON, '', DURATION_INFINITE,
         {1: aura(AURA_MOD_THREAT, 200, ALL_SCHOOLS), 2: aura(AURA_MOD_DAMAGE_PERCENT_TAKEN, 0, MAGIC_SCHOOLS),
          3: aura(AURA_MOD_RESISTANCE, 0, 1)},          # Voidforged Plate's armor (the script's amount)
         hidden=True),
    tank_action(91523, 'Covenant Lash', LASH_ICON,
                'An instant lash: 100% of your spell power as Shadowflame damage, with 100% more threat.', 0,
                gcd=True, target=TARGET_UNIT_TARGET_ENEMY, range_index=RANGE_10),
    strike(91524, 'Covenant Lash', LASH_ICON, SHADOWFLAME, VISUAL_SHADOW_BOLT_HIT),
    {'id': 91525, 'copy': DRAIN_LIFE, 'name': 'Soul Siphon', 'icon': SIPHON_ICON,
     'description': 'Channels for 3 sec, moving at 70% speed: each second 25% of your spell power as Shadow damage '
                    'and a heal of 1.5% of your maximum health.', 'tooltip': '',
     'set': {**NO_FAMILY, 'DurationIndex': DURATION_3S, 'RangeIndex': RANGE_30, 'RecoveryTime': 15000,
             'ManaCost': 0, 'ManaCostPercentage': 6, 'ChannelInterruptFlags': 0,
             'AttributesEx5': SPELL_ATTR5_ALLOW_ACTION_DURING_CHANNEL, 'BaseLevel': 1, 'SpellLevel': 1, 'MaxLevel': 0},
     'unset': {'Attributes': 0x10000},
     'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_PERIODIC_DUMMY, 'Amplitude': 1000, 'BasePoints': 0,
                     'DieSides': 1, 'RealPointsPerLevel': 0.0, 'TargetA': TARGET_UNIT_TARGET_ENEMY, 'TargetB': 0}},
     'clear': [2, 3]},
    strike(91526, 'Soul Siphon', SIPHON_ICON, SHADOW),
    heal(91527, 'Soul Siphon', SIPHON_ICON, ''),
    absorb(91528, 'Siphoning Ward', 'Spell_Shadow_LifeDrain02', DURATION_3S),
    tank_action(91529, 'Dark Intercession', 'Spell_Shadow_AntiShadow',
                '15% less damage taken for 6 sec; Covenant Lash also strikes up to 4 enemies within 6 yards of its '
                'victim for 50% of its damage.', 30000),
    buff(91530, 'Rune of the Black Covenant', COVENANT_ICON,
         'Bind Guardian merges your Voidwalker into Covenant Armor; Covenant Lash and Soul Siphon replace Searing '
         'Pain and Drain Life while bound.', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    {**buff(91540, 'Dark Intercession', 'Spell_Shadow_AntiShadow', 'Damage taken reduced.', DURATION_6S,
            {1: aura(AURA_MOD_DAMAGE_PERCENT_TAKEN, -15, ALL_SCHOOLS)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_6S}},
    buff(91541, 'Soul Siphon', 'Spell_Shadow_LifeDrain02', '', DURATION_3S,
         {1: aura(AURA_MOD_DECREASE_SPEED, -30, 0)}, hidden=True),
    # The collection's foundation: Challenge (a free 30-yard taunt, off the global cooldown, 8 sec) and the passive.
    {'id': 91542, 'copy': TAUNT, 'name': 'Challenge', 'icon': 'Spell_Nature_Reincarnation',
     'description': 'Taunts an enemy within 30 yards to attack you for 3 sec.', 'tooltip': '',
     'set': {**NO_FAMILY, 'Stances': 0, 'StancesNot': 0, 'RangeIndex': RANGE_30, 'RecoveryTime': 8000,
             'ManaCost': 0, 'PowerType': 0, 'StartRecoveryCategory': 0, 'StartRecoveryTime': 0,
             'BaseLevel': 1, 'SpellLevel': 1, 'MaxLevel': 0}},
    buff(91543, 'Tank Foundation', COVENANT_ICON, '', DURATION_INFINITE,
         {1: aura(AURA_MOD_ATTACKER_MELEE_CRIT, -100, 0), 2: aura(AURA_MOD_ATTACKER_RANGED_CRIT, -100, 0),
          3: aura(AURA_MOD_ATTACKER_SPELL_CRIT, -100, ALL_SCHOOLS)}, hidden=True),
]
# The Worldheart's magma line: Flamestrike's burning ground, laid along it by the script.
_magma = ground(91507, 'Magma Lash', 'Ability_Shaman_LavaLash', None, DURATION_4S)
_magma.pop('visual')
_magma['set']['SpellVisual'] = FX_FLAMESTRIKE_GROUND
SPELLS.append(_magma)
for _spell in SPELLS:
    if _spell['id'] in (91481, 91483, 91504, 91506, 91524, 91526):
        _spell['set'] = {**_spell['set'], 'AttributesEx2': SPELL_ATTR2_CANT_CRIT}
    if _spell['id'] == 91544:       # 20 Rage (tenths)
        _spell['set'] = {**_spell['set'], 'PowerType': 1, 'ManaCost': 200}
    if _spell['id'] == 91523:       # 8% of base mana, on the global cooldown
        _spell['set'] = {**_spell['set'], 'PowerType': 0, 'ManaCostPercentage': 8}
    if _spell['id'] == 91521:       # bound: Shadowform's look in the Voidwalker's purple
        _spell['visual'] = 'CovenantBound'
# The Black Covenant's markers: the Extended Options' block (93100, below); 91531-91539 retired.
for _spell in SPELLS:
    if _spell['id'] in (91490, 91510, 91530):
        _spell['set'] = {**_spell['set'], 'Attributes': HIDDEN['Attributes']}
ICONS += [COMMUNION_ICON, WORLDWARDEN_ICON, COVENANT_ICON, 'Spell_DeathKnight_BloodBoil',
          'Spell_DeathKnight_DeathStrike', 'Spell_Shadow_ShadowWordPain', 'Spell_DeathKnight_BloodPlague',
          'Spell_Nature_StoneClawTotem', 'Spell_Nature_EarthShock', 'Spell_Nature_EarthBindTotem',
          'Spell_Fire_SoulBurn', 'Spell_Shadow_LifeDrain02', 'Spell_Shadow_AntiShadow', 'Spell_Nature_Reincarnation',
          LASH_ICON, SIPHON_ICON, 'Spell_Shadow_SummonVoidWalker']
# The spellbook's class tabs for what the Tank Collection teaches (the default for our own spells is the Demon
# Hunter's): the Worldheart's Shaman, Sanguine Recall Death Knight, the Covenant's Warlock, Challenge Warrior.
SPELL_CLASSES.update({91482: 'DEATHKNIGHT', 91544: 'DEATHKNIGHT', 91500: 'SHAMAN', 91501: 'SHAMAN',
                      91520: 'WARLOCK', 91523: 'WARLOCK', 91525: 'WARLOCK', 91529: 'WARLOCK', 91542: 'WARRIOR'})
# Covenant Armor's look: Shadowform's (its darkening character effect and the shadow at the feet) tinted the
# Voidwalker's purple, with Shadow Dance's purple wisps about the body.
COVENANT_TINT = 0x6A5010A0                      # ARGB: Shadowform's own is 0x4A1C0108, near-black red
KIT_TINT = 21                                   # the kit's first character effect's first parameter: its colour
VISUALS['CovenantBound'] = {'copy': 3619, 'id': 16808, 'kits': {
    'state': (15747, {KIT_TINT: COVENANT_TINT, KIT_CHEST: FX_SHADOW_DANCE})}}

AURA_ADD_FLAT_MODIFIER = 107
SHRED_FLAG, MAUL_FLAG = 0x8000, 0x800
SINISTER_STRIKE_FLAG = 0x2
BUILDER_FLAGS = 0x2 | 0x4 | 0x2000000 | 0x4000000    # Sinister Strike, Backstab, Hemorrhage, Ghostly Strike


def family_mod(spell_id, name, icon, text, duration, family, flag, aura_type, op, value):
    """A spell modifier of a class's (its family: the engine makes the client honour a Hero's), one charge."""
    spell = buff(spell_id, name, icon, text, duration, {1: {**aura(aura_type, value, op), 'Mask': flag}})
    spell['set'] = {**spell['set'], 'SpellFamilyName': family, 'ProcCharges': 1}
    return spell


# ---- The Extended Options (5 choices a slot): 20 ids a rune from 91600, its 15 markers (+ slot * 5 + choice) and
# then its new spells.
EXTENDED_MODIFIERS = [
    (91600, CONVERGENCE_ICON, [('Perfect Alignment', 'Converging Skies', 'Astral Rupture', 'Flame Eclipse',
                                'Frozen Eclipse'),
                               ('Long Twilight', 'Crossed Horizons', 'Steady Orbit', 'Tidal Flare', 'Twin Moons'),
                               ('Solar Return', 'Lunar Dominance', 'Eventide', 'Insect Renewal', 'Astral Ward')]),
    (91620, PHOENIX_ICON, [('Sun Talon', 'Broadwing', 'Ashen Flock', 'Flamekeeper', 'Lava Flock'),
                           ('Homeward Flame', 'Death Spiral', 'Wild Flight', 'Blazing Path', 'Hot Pursuit'),
                           ('Scorched Earth', 'Clinging Embers', 'Rebirth', 'Wildfire Return', 'Emberguard')]),
    (91645, HORIZON_ICON, [('Wandering Star', 'Event Horizon', 'Satellite', 'Orbital Lightning',
                            'Starward Satellite'),
                           ('Astral Focus', 'Constellation', 'Prismatic Lance', 'Shattering Orbit', 'Distant Spark'),
                           ('Supernova', 'Dying Light', 'Astral Remnant', 'Evoked Remnant', 'Astral Shelter')]),
    (91670, MIND_ICON, [('Accelerated Thought', 'Expanding Intellect', 'Patient Calculation', 'Storm Conduit',
                         'Starfall Equation'),
                        ('Focused Intention', 'Fractured Thought', 'Lingering Equation', 'Missile Shards',
                         'Arcane Charge'),
                        ('Unbound Mind', 'Mental Reserves', 'Overclocked Intellect', 'Evocative Mind',
                         'Shocking Clarity')]),
    (91695, GUIDING_ICON, [('Steady Light', 'Shared Light', 'Sheltering Light', 'Earthen Star', 'Moonlit Star'),
                           ('Gentle Passage', 'Guardian Star', 'Seeking Light', 'Chain Guidance', 'Holy Passage'),
                           ('Final Benediction', 'Afterglow', 'Parting Grace', 'Tidal Farewell',
                            'Renewed Farewell')]),
    (91720, BLADE_ICON, [('Executioner', 'Thousand Cuts', 'Shadow Cleave', 'Frost Phantom', 'Flame Phantom'),
                         ('Fatal Opening', 'Perfect Ambush', 'Open Wound', 'Marked Execution', 'Corrupted Blade'),
                         ('Death Follows', 'Vanishing Act', 'Marked for Murder', 'Restoring Phantom',
                          'Bleeding Follow-Through')]),
    (91745, ELUNE_ICON, [('Moonpiercer', 'Silver Wings', 'Lunar Flock', 'Sentinel of the Hunt', 'Frostfeather'),
                         ('Swift Judgment', 'Patient Vigil', 'Lunar Guidance', 'Command the Hunt', 'Sun and Moon'),
                         ('Lunar Wound', 'Guiding Wings', 'Twilight Hunt', "Hunter's Pursuit", "Winter's Blessing")]),
    (91795, CROWN_ICON, [('Glacial Spear', 'Twin Fangs', 'Frozen Volley', 'Hailstorm Crown', 'Splintering Ice'),
                         ('Cold Snap', 'Brittle Impact', 'Gathering Blizzard', 'Frostfire Fracture', 'Runic Rime'),
                         ('Deep Freeze', 'Glacial Wound', 'Permafrost', 'Whiteout', 'Cold Front')]),
    (91820, DUELIST_ICON, [('Heavy Caliber', 'Gunslinger', 'Scattershot', 'Flame Pistol', 'Storm Pistol'),
                           ('Powder and Steel', 'Between the Eyes', 'Loaded Powder', 'Marked Shot', 'Frozen Powder'),
                           ('Quick Draw', 'Cut and Run', 'Efficient Duelist', 'Healing Powder', 'Arcane Powder')]),
    (91870, WHITEFANG_ICON, [('Predator\'s Fang', 'Pack of Winter', 'Glacial Claws', 'Stormfang', 'Flamefang'),
                             ('Frozen Wound', 'Cold Blood', "Predator's Chill", 'Marked Prey', 'Moonfrost'),
                             ('Swift Hunt', "Winter's Recovery", 'Lingering Frost', 'Mending Fang', 'Clawing Cold')]),
    (91895, URSOC_ICON, [('Crushing Champion', 'Packbreaker', 'Unbroken Champion', 'Stoneguard', 'Thunderbear'),
                         ('Long Guard', 'Earthen Hide', "Ursoc's Challenge", 'Rending Maul', 'Earthshock Roar'),
                         ('Defiant Roar', 'Restoring Force', "Bear's Momentum", 'Lifebloom Hide', 'Shielding Roots')]),
    (93000, COMMUNION_ICON, [('Sovereign Bond', 'Bloodbound Court', 'Long Veins', 'Corrupt Communion',
                              'Thorned Communion'),
                             ('Crimson Feast', 'Arterial Echo', 'Shared Weakness', 'Rending Communion',
                              'Earthbound Blood'),
                             ('Coagulated Throne', 'Rapid Recall', 'Blood for Blood', "Death's Recall",
                              'Holy Communion')]),
    (93025, WORLDWARDEN_ICON, [('Earthen Command', 'Flame Dominion', 'Winter Command', 'Bloodstone Command',
                                'Judgment of the Elements'),
                               ('Deepstone Heart', 'Wide Communion', 'Striding Heart', 'Consecrated Heart',
                                'Earthen Communion'),
                               ('Resonant Strikes', 'Molten Causeway', 'Stoneblood Lash', 'Thunderheart',
                                'Runic Renewal')]),
    (93050, PARAGON_ICON, [('Boundless Sanctity', 'Hallowed Footsteps', 'Seat of the Paragon', 'Consecrated Charge',
                            'Sanctuary March'),
                           ('Sanctified Condemnation', 'Merciful Ground', 'Shelter of the Paragon', 'Radiant Judgment',
                            'Healing Light'),
                           ('Bastion of Faith', 'Weight of the Unworthy', 'Measured Devotion', 'Dawnward',
                            "Paragon's Aegis")]),
    (93100, COVENANT_ICON, [('Voidforged Plate', 'Nether Insulation', 'Hungry Covenant', 'Blood Pact', 'Earthen Pact'),
                            ('Deep Extraction', 'Siphoning Ward', 'Unfettered Drain', 'Corrupted Drain',
                             'Frostbound Siphon'),
                            ('Guardian Manifest', "Legion's Rebuke", 'Frequent Petition', 'Judgment of the Void',
                             'Blood Intercession')]),
    (93125, MIST_ICON, [('Flowing Currents', 'Drifting Mist', 'Gathering Mists', 'Renewing Mist', 'Tidal Mist'),
                        ('Comforting Veil', 'Vivacious Flow', 'Peaceful Focus', 'Holy Thread', 'Chain of Jade'),
                        ('Effortless Breathing', 'Rolling Mist', 'Jade Shelter', 'Renewing Breath',
                         'Earthen Cocoon')]),
    (93150, HUNT_ICON, [('Alpha Predator', 'Pack Ravager', 'Ghostfang', 'Marked Alpha', 'Frostfang'),
                        ('Fixed Prey', "Hunter's Command", 'Blood Scent', 'Arcane Hunt', 'Storm Hunt'),
                        ('Final Pounce', 'Frenzied Farewell', 'Parting Howl', 'Marked Finale', 'Healing Companion')]),
    (93075, STORMREAVER_ICON, [('Eye of the Tempest', 'Forked Cataclysm', 'Rolling Thunder', 'Stormhunter',
                                'Frozen Front'),
                               ('Thunderhead', 'Chainbreaker', 'White-Hot Conductor', 'Stormhammer',
                                'Flame Conductor'),
                               ('Overcharged Core', 'Ride the Lightning', 'Violent Grounding', 'Static Strike',
                                'Warstorm')]),
    (91845, EMBERS_ICON, [('Cinderclaw', 'Wildfire Claws', 'Living Flame', 'Flame Shock Claws', 'Frozen Embers'),
                          ('Ember Rush', 'Smoldering Bite', 'Firestorm Pounce', 'Phoenix Claws', 'Wildfire Swipe'),
                          ('Ashen Renewal', 'Flame-Tipped Claws', 'Phoenix Scar', 'Healing Flame', 'Burning Rake')]),
    (91770, HOLLOW_ICON, [('Following Shadows', 'Haunted Ground', 'Possession', 'Plague Sermon',
                           'Moonlit Congregation'),
                          ('Unison', 'Dissonance', 'Accusation', 'Devouring Command', 'Frostbound Sermon'),
                          ('Last Word', 'Procession', 'Silent Witness', 'Feeding the Void', 'Renewed Witness')]),
]
SPELLS += [buff(first + slot * 5 + choice, name, icon, '', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)},
                hidden=True)
           for first, icon, modifiers in EXTENDED_MODIFIERS
           for slot, names in enumerate(modifiers) for choice, name in enumerate(names)]
SPELLS += [
    # Astral Convergence.
    strike(91615, 'Flame Eclipse', 'Spell_Fire_FlameShock', FIRE, VISUAL_FIRE_BLAST),
    buff(91616, 'Frozen Eclipse', 'Spell_Frost_FrostBolt02', 'Your next Lunar Collision deals 25% more damage.',
         DURATION_12S, {1: aura(AURA_DUMMY, 1, 0)}),
    druid_mod(91617, 'Insect Renewal', 'Spell_Nature_InsectSwarm', 'Your next Wrath casts 15% faster.', DURATION_5S,
              WRATH_FLAG, [(SPELLMOD_CASTING_TIME, 15)], charges=1),
    absorb(91618, 'Astral Ward', 'Spell_Nature_StoneClawTotem', DURATION_6S),
    # Phoenix Wake.
    dot(91635, 'Phoenix Burn', 'Spell_Fire_Fire', FIRE, DURATION_6S, 'Fire damage every second.'),
    strike(91636, 'Phoenix Burn', 'Spell_Fire_Fire', FIRE),
    strike(91637, 'Phoenix Eruption', 'Spell_Fire_SelfDestruct', FIRE, VISUAL_FIRE_NOVA),
    strike(91638, 'Flamekeeper', 'Spell_Fire_FlameShock', FIRE, VISUAL_FIRE_BLAST),
    strike(91639, 'Blazing Path', 'Spell_Fire_SelfDestruct', FIRE, VISUAL_FIRE_BLAST),
    buff(91640, 'Hot Pursuit', 'Spell_Fire_SoulBurn', "Your next phoenix's strike on its host deals 25% more damage.",
         DURATION_8S, {1: aura(AURA_DUMMY, 1, 0)}),
    absorb(91641, 'Emberguard', 'Spell_Fire_FireArmor', DURATION_6S),
    # Astral Horizon.
    strike(91660, 'Arcane Orb', 'Spell_Arcane_Arcane04', ARCANE, VISUAL_ARCANE_HIT),
    strike(91661, 'Orb Pulse', 'Spell_Nature_WispSplode', ARCANE, VISUAL_ARCANE_HIT),
    strike(91662, 'Orbital Lightning', 'Spell_Nature_Lightning', NATURE, VISUAL_LIGHTNING_BOLT),
    absorb(91663, 'Astral Shelter', 'Spell_Holy_PowerWordShield', DURATION_6S),
    # Arcane Mind.
    quickening(91685, 'Mind Flow', MIND_ICON, 'Your next Arcane Blast is instant.', DURATION_6S, ARCANE_BLAST_FLAG,
               100, charges=1),
    buff(91686, 'Storm Conduit', 'Spell_Nature_Lightning', 'Your next Insight volley deals 20% more damage.',
         DURATION_8S, {1: aura(AURA_DUMMY, 1, 0)}),
    buff(91687, 'Starfall Equation', 'Spell_Arcane_StarFire', 'Your next Arcane Blast deals 10% more damage.',
         DURATION_6S, {1: {**aura(AURA_ADD_PCT_MODIFIER, 10, SPELLMOD_DAMAGE), 'Mask': ARCANE_BLAST_FLAG}}),
    strike(91688, 'Arcane Charge', 'Spell_Nature_WispSplode', ARCANE, VISUAL_ARCANE_HIT),
    strike(91689, 'Shocking Clarity', 'Spell_Nature_LightningShield', NATURE, VISUAL_LIGHTNING_BOLT),
    # Guiding Light.
    periodic_heal(91710, 'Guiding Trail', 'Spell_Holy_Renew', 'Healing every 2 sec.', DURATION_6S),
    absorb(91711, 'Guiding Ward', 'Spell_Holy_PowerWordShield', DURATION_6S),
    # The Spectral Blade.
    dot(91735, 'Phantom Wound', 'Ability_Rogue_FindWeakness', SHADOW, DURATION_6S, 'Shadow damage every second.'),
    strike(91736, 'Phantom Wound', 'Ability_Rogue_FindWeakness', SHADOW),
    strike(91737, 'Frost Phantom', 'Spell_DeathKnight_IceTouch', FROST, VISUAL_ICE_LANCE),
    strike(91738, 'Flame Phantom', 'Spell_Fire_FlameShock', FIRE, VISUAL_FIRE_BLAST),
    dot(91739, 'Corrupted Blade', 'Spell_Shadow_AbominationExplosion', SHADOW, DURATION_6S,
        'Shadow damage every second.'),
    strike(91740, 'Corrupted Blade', 'Spell_Shadow_AbominationExplosion', SHADOW),
    heal(91741, 'Restoring Phantom', 'Spell_Holy_Renew', 'The phantom restores you.'),
    family_mod(91742, 'Phantom Opening', BLADE_ICON, 'Your next Sinister Strike costs 10 less Energy.', DURATION_6S,
               ROGUE_FAMILY, SINISTER_STRIKE_FLAG, AURA_ADD_FLAT_MODIFIER, SPELLMOD_COST, -10),
    family_mod(91743, 'Fatal Opening', BLADE_ICON, 'Your next combo point builder costs 10 less Energy.',
               DURATION_6S, ROGUE_FAMILY, BUILDER_FLAGS, AURA_ADD_FLAT_MODIFIER, SPELLMOD_COST, -10),
    family_mod(91744, 'Bleeding Follow-Through', 'Ability_Gouge', 'Your next Sinister Strike deals 20% more damage.',
               DURATION_6S, ROGUE_FAMILY, SINISTER_STRIKE_FLAG, AURA_ADD_PCT_MODIFIER, SPELLMOD_DAMAGE, 20),
    # Elune's Guardian.
    harmful(91760, 'Moonlit Prey', 'Spell_Nature_Moonglow', 'Your Wrath on this target calls the owl back.',
            DURATION_8S, aura(AURA_DUMMY, 1, 0), ARCANE),
    strike(91761, "Owl's Return", 'Ability_Hunter_Pet_Owl', NATURE, VISUAL_SOLAR_ECLIPSE),
    strike(91762, 'Frostfeather', 'Spell_Frost_FrostBolt02', FROST, VISUAL_ICE_LANCE),
    harmful(91763, 'Moonfrost', 'Spell_Frost_ChillingBlast', 'Your next Frostbolt on this target deals 20% more.',
            DURATION_6S, aura(AURA_DUMMY, 1, 0), FROST),
    harmful(91764, 'Exposed Prey', 'Ability_Hunter_SniperShot', 'Your next shot on this target spends it.',
            DURATION_6S, aura(AURA_DUMMY, 1, 0), ARCANE),
    # The Hollow Congregation.
    ground(91785, 'Hollow Patch', 'Spell_Shadow_ShadowWordPain', 'GraveDiseasedPatch', DURATION_3S),
    strike(91786, 'Hollow Patch', 'Spell_Shadow_ShadowWordPain', SHADOW),
    strike(91787, 'Moonlit Congregation', 'Spell_Nature_StarFall', ARCANE, VISUAL_ARCANE_HIT),
    strike(91788, 'Devouring Command', 'Spell_Shadow_ShadowBolt', SHADOW, VISUAL_SHADOW_BOLT),
    # The Splintered Crown.
    harmful(91810, 'Frostbite', 'Spell_Frost_FrostArmor', 'Your Frostbolt and Ice Lance deal 8% more to this target.',
            DURATION_6S, aura(AURA_DUMMY, 1, 0), FROST),
    quickening(91811, 'Cold Snap', 'Spell_Frost_IceShard', 'Your next Frostbolt casts 15% faster.', DURATION_6S,
               FROSTBOLT_FLAG, 15, charges=1),
    buff(91812, 'Gathering Blizzard', 'Spell_Frost_IceStorm', 'Your next Ice Lance deals 5% more damage a stack.',
         DURATION_8S, {1: aura(AURA_DUMMY, 1, 0)}, stacks=4),
    # The Duelist.
    buff(91835, 'Loaded Steel', 'Ability_Rogue_SabreSlash',
         'Your next Sabre Slash deals 15% more and strikes one more enemy.', DURATION_6S, {1: aura(AURA_DUMMY, 1, 0)}),
    buff(91836, 'Loaded Powder', 'Spell_Fire_Fireball', 'Your next Pistol Shot explodes.', DURATION_8S,
         {1: aura(AURA_DUMMY, 1, 0)}),
    strike(91837, 'Powder Blast', 'Spell_Fire_Fireball', FIRE, VISUAL_FIRE_BLAST),
    strike(91838, 'Storm Pistol', 'Spell_Nature_LightningShield', NATURE, VISUAL_LIGHTNING_BOLT),
    buff(91839, 'Marked Shot', 'Ability_Hunter_SniperShot', 'Your next Sabre Slash deals 25% more damage.',
         DURATION_6S, {1: aura(AURA_DUMMY, 1, 0)}),
    strike(91840, 'Frozen Powder', 'Spell_Frost_FrostShock', FROST, VISUAL_ICE_LANCE),
    {**buff(91841, 'Cut and Run', 'Ability_Rogue_Sprint', 'Movement speed increased by 25%.', DURATION_4S,
            {1: aura(AURA_MOD_INCREASE_SPEED, 25, 0)}),
     'set': {**SHOWN, 'DurationIndex': DURATION_4S}},
    heal(91842, 'Healing Powder', 'Spell_Holy_Renew', 'Your powder mends you.'),
    buff(91843, 'Arcane Powder', 'Spell_Arcane_StarFire', 'Your next Pistol Shot deals 25% more damage.',
         DURATION_8S, {1: aura(AURA_DUMMY, 1, 0)}),
    buff(91844, 'Dead Aim', PISTOL_ICON, 'Your next Pistol Shot deals 5% more damage a stack.', DURATION_8S,
         {1: aura(AURA_DUMMY, 1, 0)}, stacks=15),
    # Ember's Wrath.
    strike(91860, 'Searing Shred', 'Spell_Fire_Incinerate', FIRE, VISUAL_SCORCH),
    strike(91861, 'Frozen Embers', 'Spell_Frost_FrostShock', FROST, VISUAL_ICE_LANCE),
    strike(91862, 'Phoenix Claws', 'Spell_Fire_Burnout', FIRE, VISUAL_FIRE_BLAST),
    strike(91863, 'Phoenix Scar', 'Spell_Fire_SelfDestruct', FIRE, VISUAL_SCORCH),
    heal(91864, 'Healing Flame', 'Spell_Nature_Rejuvenation', 'The flame mends you.'),
    buff(91865, 'Burning Rake', 'Ability_Druid_Disembowel', 'Your next Blazing Strike deals 30% more damage.',
         DURATION_6S, {1: aura(AURA_DUMMY, 1, 0)}),
    # Whitefang.
    strike(91885, 'Stormfang', 'Spell_Nature_LightningShield', NATURE, VISUAL_LIGHTNING_BOLT),
    strike(91886, 'Flamefang', 'Spell_Fire_FlameShock', FIRE, VISUAL_FIRE_BLAST),
    family_mod(91887, 'Lingering Frost', 'Spell_Frost_ArcticWinds', 'Your next Shred deals 15% more damage.',
               DURATION_10S, DRUID_FAMILY, SHRED_FLAG, AURA_ADD_PCT_MODIFIER, SPELLMOD_DAMAGE, 15),
    family_mod(91888, 'Clawing Cold', 'Spell_Frost_ChillingBlast', 'Your next Shred costs 10 less Energy.',
               DURATION_6S, DRUID_FAMILY, SHRED_FLAG, AURA_ADD_FLAT_MODIFIER, SPELLMOD_COST, -10),
    heal(91889, 'Mending Fang', 'Spell_Nature_Rejuvenation', 'The hunt mends you.'),
    # Ursoc's Champion.
    harmful(91910, "Ursoc's Mark", 'Ability_Druid_Mangle2', 'Ursoc\'s impact here strikes those near it too.',
            DURATION_6S, aura(AURA_DUMMY, 1, 0), PHYSICAL),
    dot(91911, 'Rending Maul', 'Ability_Druid_Maul', PHYSICAL, DURATION_6S, 'Bleeding every second.'),
    strike(91912, 'Rending Maul', 'Ability_Druid_Maul', PHYSICAL),
    buff(91913, 'Earthen Fury', 'Spell_Nature_EarthShock', "Your next impact deals 15% more, half of it Nature.",
         DURATION_10S, {1: aura(AURA_DUMMY, 1, 0)}),
    strike(91914, 'Thunderbear', 'Spell_Nature_ThunderClap', NATURE, VISUAL_THUNDER_HIT),
    family_mod(91915, "Bear's Momentum", URSOC_ICON, 'Your next Maul deals 20% more damage.', DURATION_6S,
               DRUID_FAMILY, MAUL_FLAG, AURA_ADD_PCT_MODIFIER, SPELLMOD_DAMAGE, 20),
    heal(91916, "Ursoc's Mend", URSOC_ICON, 'The impact mends you.'),
    absorb(91917, 'Shielding Roots', 'Spell_Nature_StoneClawTotem', DURATION_4S),
    harmful(91918, "Ursoc's Challenge", 'Ability_Druid_ChallangingRoar', 'Damage done reduced by 15%.', DURATION_3S,
            aura(AURA_MOD_DAMAGE_PERCENT_DONE, -15, ALL_SCHOOLS), PHYSICAL),
    strike(91919, "Ursoc's Impact", URSOC_ICON, NATURE),
    # Red Communion.
    strike(93015, 'Rending Communion', 'Ability_Gouge', PHYSICAL),
    absorb(93016, 'Earthbound Blood', 'Spell_Nature_SkinofEarth', DURATION_4S),
    absorb(93017, 'Holy Communion', 'Spell_Holy_SearingLight', DURATION_4S),
    # The Primal Worldwarden.
    absorb(93040, 'Bloodstone Command', 'Spell_Deathknight_BloodPresence', DURATION_4S),
    absorb(93041, 'Consecrated Heart', 'Spell_Holy_InnerFire', DURATION_4S),
    heal(93042, 'Runic Renewal', 'Spell_Shadow_DeathCoil', 'The Worldheart renews you.'),
    # The Sanctified Paragon.
    strike(93065, 'Sanctified Judgment', 'Spell_Holy_RighteousFury', HOLY, VISUAL_HOLY_SHOCK),
    heal(93066, "Paragon's Mend", 'Spell_Holy_SearingLight', 'The aura mends you.'),
    absorb(93067, 'Dawnward', 'Spell_Holy_PowerWordShield', DURATION_4S),
    absorb(93068, 'Righteous Ward', 'Ability_Paladin_ShieldofVengeance', DURATION_4S),
    # The Stormreaver.
    buff(93090, 'Storm Surge', 'Spell_Nature_EarthShock', 'Your next Stormfront pulse deals 20% more damage.',
         DURATION_6S, {1: aura(AURA_DUMMY, 1, 0)}),
    ground(93091, 'Frozen Front', 'Spell_Frost_FrostShock', 'GraveFrozenLine', DURATION_4S),
    strike(93092, 'Frozen Front', 'Spell_Frost_FrostShock', FROST),
    strike(93093, 'Stormhammer', 'Spell_Nature_Lightning', NATURE, VISUAL_LIGHTNING_BOLT),
    strike(93094, 'Warstorm', 'Spell_Nature_ThunderClap', NATURE, VISUAL_THUNDER_HIT),
    buff(93095, 'Static Strike', 'Spell_Nature_LightningShield', 'Your next discharge strikes twice.', DURATION_6S,
         {1: aura(AURA_DUMMY, 1, 0)}),
    # The Black Covenant.
    absorb(93115, 'Blood Pact', 'Spell_Deathknight_BloodPresence', DURATION_4S),
    absorb(93116, 'Earthen Pact', 'Spell_Nature_SkinofEarth', DURATION_4S),
    strike(93117, 'Frostbound Siphon', 'Spell_DeathKnight_IceTouch', FROST, VISUAL_ICE_LANCE),
    absorb(93118, 'Blood Intercession', 'Spell_Shadow_AntiShadow', DURATION_4S),
    # The Wandering Mist.
    heal(93140, 'Tidal Mist', 'Spell_Nature_HealingWaveGreater', 'The tide carries the mist.'),
    heal(93141, 'Holy Thread', 'Spell_Holy_HolyBolt', 'A thread of holy light.'),
    absorb(93142, 'Earthen Cocoon', 'Spell_Nature_SkinofEarth', DURATION_4S),
    # The Wild Hunt.
    strike(93165, 'Frostfang', 'Spell_DeathKnight_IceTouch', FROST, VISUAL_ICE_LANCE),
    strike(93166, 'Storm Hunt', 'Spell_Nature_Lightning', NATURE, VISUAL_LIGHTNING_BOLT),
    heal(93167, 'Healing Companion', HUNT_ICON, 'The spectral beast mends your pet.'),
]
for _spell in SPELLS:
    if _spell['id'] in (93015, 93065, 93092, 93093, 93094, 93117, 93165, 93166):
        _spell['set'] = {**_spell['set'], 'AttributesEx2': SPELL_ATTR2_CANT_CRIT}
for _spell in SPELLS:
    if _spell['id'] in (91885, 91886, 91912, 91914, 91919):
        _spell['set'] = {**_spell['set'], 'AttributesEx2': SPELL_ATTR2_CANT_CRIT}
for _spell in SPELLS:
    if _spell['id'] in (91615, 91636, 91637, 91638, 91639, 91660, 91661, 91662, 91688, 91689, 91736, 91737, 91738,
                        91740, 91761, 91762, 91786, 91787, 91788, 91837, 91838, 91840, 91860, 91861, 91862, 91863):
        _spell['set'] = {**_spell['set'], 'AttributesEx2': SPELL_ATTR2_CANT_CRIT}
    if _spell['id'] == 91687:
        _spell['set'] = {**_spell['set'], 'SpellFamilyName': MAGE_FAMILY, 'ProcCharges': 1}
    if _spell['id'] == 91710:
        _spell['effects'][1]['Amplitude'] = 2000
        _spell['tooltip'] = 'Healing every 2 sec.'
ICONS += ['Spell_Nature_InsectSwarm', 'Spell_Fire_SoulBurn', 'Spell_Fire_FireArmor', 'Spell_Arcane_Arcane04',
          'Spell_Nature_WispSplode', 'Spell_Nature_Lightning', 'Spell_Nature_LightningShield',
          'Spell_Shadow_AbominationExplosion', 'Spell_Frost_ChillingBlast', 'Ability_Hunter_SniperShot',
          'Ability_Gouge', 'Spell_Shadow_ShadowWordPain', 'Spell_Shadow_ShadowBolt', 'Spell_Nature_StarFall',
          'Spell_DeathKnight_IceTouch', 'Spell_Fire_FlameShock', 'Spell_Holy_Renew', 'Ability_Rogue_FindWeakness',
          'Spell_Nature_Moonglow', 'Ability_Hunter_Pet_Owl', 'Spell_Frost_FrostBolt02', 'Spell_Frost_FrostArmor',
          'Spell_Frost_IceShard', 'Spell_Frost_IceStorm', 'Ability_Rogue_SabreSlash', 'Spell_Fire_Fireball',
          'Spell_Frost_FrostShock', 'Ability_Rogue_Sprint', 'Spell_Arcane_StarFire', 'Spell_Fire_Incinerate',
          'Spell_Fire_Burnout', 'Spell_Fire_SelfDestruct', 'Spell_Nature_Rejuvenation', 'Ability_Druid_Disembowel',
          PISTOL_ICON, 'Ability_Druid_Mangle2', 'Ability_Druid_Maul', 'Spell_Nature_EarthShock',
          'Spell_Nature_ThunderClap', 'Ability_Druid_ChallangingRoar', 'Spell_Frost_ArcticWinds',
          'Spell_Nature_SkinofEarth', 'Spell_Holy_SearingLight', 'Spell_Deathknight_BloodPresence',
          'Spell_Holy_InnerFire', 'Spell_Shadow_DeathCoil', 'Spell_Holy_RighteousFury',
          'Ability_Paladin_ShieldofVengeance']
