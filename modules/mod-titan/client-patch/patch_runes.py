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
- 90732 Consecration: a tick's Holy damage (its amount the Hero's Consecration rank's; spell power and attack
  power through spell_bonus_data, as Consecration's);
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
  procs (Lightning Bolt and Chain Lightning hits) make enemies Conductive and arc (runes.cpp);
- 90751 Conductive: on an enemy, 6 sec (12 with Persistent Current, set by the script), one per Hero; lightning
  crackling on it;
- 90752 Stormbound Arc: a secondary arc's Nature damage (its amount set by the script, no caster modifiers), a
  strike of lightning where it lands;
- 90753 its look: Chain Lightning's bolt from the Conductive enemy to the next (cast by that enemy, no effect);
- 90754 Lightning Shield: a retaliation the rune makes (Distant Conductor, Storm Within): the Hero's Lightning
  Shield rank's damage, its spell power share through spell_bonus_data; a bolt from the sky;
- 90760-90768 its nine modifiers' markers (Conductivity, Arc Pattern, Lightning Shield: 3 each), hidden.

The Rune of Echoing Thunder (the "Rune of Echoing Thunder" concept doc), 90770 on:

- 90770 the rune: a buff the Hero wears while their active specialization's rune is Echoing Thunder;
- 90771 Thunderfield: where Thunder Clap leaves its storm (a patch of ground, 8 yards, 4 sec; the script places it
  and sets its time); its look: cracked earth with lightning running through it (build_thunder_art.py);
- 90772 Thunderfield (March of the Storm): the storm following the Hero, 5 yards (a buff with the same ground);
- 90773 Thunder Echo's wave: Shockwave's look from the Hero toward the target (no effect);
- 90774 Thunder Echo: Nature damage (its amount set by the script, never a critical strike of its own);
- 90775 its burst about the Shield Slam target (Reverberating Impact, Focused Peal): Thunder Clap's look, cast by
  the target (no effect);
- 90776 Sheltering Reverberation: an absorb (its amount set by the script), 3 sec;
- 90777 Deafening Impact: movement speed 15% slower, 2 sec;
- 90780-90788 its nine modifiers' markers (Thunderfield, Echo Pattern, Shield Resonance: 3 each), hidden.

The Rune of the Wellspring (the "Rune of the Wellspring" concept doc), 90790 on:

- 90790 the rune; 90791 Wellspring: on the Earth Shield wearer Chain Heal made one, 6 sec (10 with Lasting Spring),
  Riptide's swirl of water about them;
- 90792 Wellspring: the additional jump's heal (its amount set by the script), Chain Heal's bolt and its look;
- 90793 Tidal Shelter: an absorb, 4 sec; 90794 Renewing Current: healing every sec for 4 sec;
- 90795 the jump's bolt from the Wellspring to the ally (cast by the Wellspring, no effect);
- 90800-90808 its nine modifiers' markers (Wellspring, Current, Undertow: 3 each), hidden.

The Rune of the Wandering Mist (the "Rune of the Wandering Mist" concept doc), 90810 on:

- 90810 the rune; 90811 Wandering Mist: a ribbon of jade mist's heal (its amount set by the script), Vivify's
  splash look;
- 90812 lesser Enveloping Mist (Comforting Veil): healing every sec for 6 sec, Enveloping Mist's look;
- 90813 lingering mist (Rolling Mist): healing every sec for 4 sec;
- 90814 Jade Shelter: an absorb, 4 sec;
- 90820-90828 its nine modifiers' markers (Mistflow, Technique, Mistweaver's Approach: 3 each), hidden.

The five after them (their own concept docs), each with its markers 20 on from its first:

- the Threefold Grave, 90830: Requiem 90831 (Chosen Ending's three 90832-90834), the Dominions' buffs 90835-90837,
  Rime 90838 and its slow 90839, Blight 90840, Residual Infection 90841, Gore 90842, Grave Ward 90843, Rime's
  farewell look 90844, the scars' grounds 90845-90847 (March of the Dead's on the Hero 90848), Cruel Winter 90849;
- the Unbroken Onslaught, 90860 (its procs: melee abilities): Weapon Wake 90861, Bloodletting 90862, the wake's
  ground 90863, Aftershock 90864;
- the Dawnbound Crusader, 90890: Dawn Vow 90891 (on the ally 90892), Dawn Mark 90893, the echo's heal and damage
  90894-90895, its looks from the vow 90896-90897, Sunburst's 90898, Sacred Ground 90899, Afterimage 90900;
- the Hollow Congregation, 90920: the echo's look 90921, Hollow Flay 90922, the barrage's bolt 90923, Accusation
  90924, Last Word 90925;
- the Shattered Sun, 90950: Sunshard 90951, Broken Core 90952, Fracture 90953 and its look 90954, Collapse 90955 and
  its look 90956, Scalding Ground 90957, Solar Anchor 90958.
"""

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
        'set': {**HARMFUL, 'SchoolMask': HOLY},
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
                        'RealPointsPerLevel': 0.0, 'Amplitude': 0, 'TargetA': TARGET_DEST_CASTER,
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
                  'Lightning Bolt makes its target Conductive for 6 sec. Chain Lightning striking a Conductive enemy '
                  'arcs to one nearby enemy for 15% of Lightning Bolt\'s damage, and your Lightning Shield\'s '
                  'retaliations arc to one nearby Conductive enemy for 25% of their damage.',
                  DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)})
# Its procs: Lightning Bolt and Chain Lightning land as harmful spells; the script picks them out.
STORM_RUNE['set'] = {**STORM_RUNE['set'], 'ProcFlags': PROC_DONE_MAGIC_SPELL_NEGATIVE, 'ProcChance': 101}
SPELLS += [
    STORM_RUNE,
    {
        'id': 90751, 'copy': CRIPPLING_POISON, 'name': 'Conductive', 'icon': 'Spell_Nature_LightningOverload',
        'description': 'Conductive: the Stormbound Conduit\'s lightning arcs through it.',
        'tooltip': 'Conductive: lightning arcs through it.',
        'set': {**HARMFUL, 'DurationIndex': DURATION_6S, 'SchoolMask': NATURE, 'SpellVisual': VISUAL_CRACKLE},
        'effects': {1: {**aura(AURA_DUMMY, 1, 0), 'TargetA': TARGET_UNIT_TARGET_ENEMY}},
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
    ('Persistent Current', 'Branching Current', 'Live Wire'),
    ('Forked Tempest', 'Closed Circuit', 'Faraday Field'),
    ('Storm Within', 'Violent Reprisal', 'Distant Conductor'),
]
SPELLS += [buff(90760 + slot * 3 + choice, name, STORM_ICON, '', DURATION_INFINITE,
                {1: aura(AURA_DUMMY, 1, 0)}, hidden=True)
           for slot, names in enumerate(STORM_MODIFIERS) for choice, name in enumerate(names)]

# The Rune of Echoing Thunder (90770 on; runes.cpp's Echoing Thunder section).
THUNDER = 'WAR_EchoingThunder'
STORM_BLUE, PALE_BLUE = (0.45, 0.7, 1.0), (0.75, 0.88, 1.0)
EFFECTS.update({
    # The Thunderfield: cracked earth with lightning running through it, slowly turning, a paler layer over it.
    'Thunderfield': {'model': 'WAR_Thunderfield', 'effect': 7157, 'shape': 'zone',
                     'textures': (f'{THUNDER}/thunder_cracks',),
                     'options': {'layers': (
                         {'texture': 0, 'size': 16.0, 'height': 0.1, 'tint': STORM_BLUE, 'alpha': 0.8, 'turn': 40.0,
                          'blend': 4},
                         {'texture': 0, 'size': 15.0, 'height': 0.14, 'tint': PALE_BLUE, 'alpha': 0.45,
                          'turn': -55.0, 'blend': 4})}},
    # March of the Storm: the same ground, 5 yards, under the Hero.
    'ThunderfieldMarch': {'model': 'WAR_ThunderfieldMarch', 'effect': 7158, 'shape': 'zone',
                          'textures': (f'{THUNDER}/thunder_cracks',),
                          'options': {'layers': (
                              {'texture': 0, 'size': 10.0, 'height': 0.1, 'tint': STORM_BLUE, 'alpha': 0.8,
                               'turn': 40.0, 'blend': 4},
                              {'texture': 0, 'size': 9.4, 'height': 0.14, 'tint': PALE_BLUE, 'alpha': 0.45,
                               'turn': -55.0, 'blend': 4})}},
})
SPELLS += [
    buff(90770, 'Rune of Echoing Thunder', THUNDER_ICON,
         'Thunder Clap leaves a Thunderfield for 4 sec. Shield Slam landed while you stand in it releases a Thunder '
         'Echo toward your target: Nature damage to up to 4 enemies, 15% of Shield Slam\'s damage.',
         DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    {
        'id': 90771, 'copy': CONSECRATION, 'name': 'Thunderfield', 'icon': 'Spell_Nature_ThunderClap',
        'description': '', 'tooltip': '', 'visual': 'Thunderfield',
        'set': {**FREE, 'SpellFamilyName': 0, 'SpellFamilyFlags0': 0, 'SpellFamilyFlags1': 0, 'SpellFamilyFlags2': 0,
                'DurationIndex': DURATION_6S_INDEX, 'RangeIndex': RANGE_VISION},
        'effects': {1: {'Effect': EFFECT_PERSISTENT_AREA_AURA, 'Aura': AURA_DUMMY, 'BasePoints': 0, 'DieSides': 1,
                        'RealPointsPerLevel': 0.0, 'Amplitude': 0, 'TargetA': TARGET_DEST_CASTER,
                        'TargetB': TARGET_UNIT_DEST_AREA_ENEMY, 'RadiusIndex': RADIUS_8}},
    },
    {
        **buff(90772, 'Thunderfield', 'Spell_Nature_ThunderClap',
               'A Thunderfield follows you: Shield Slam releases a Thunder Echo.', DURATION_6S_INDEX,
               {1: aura(AURA_DUMMY, 1, 0)}),
        'visual': 'ThunderfieldMarch',
    },
    {
        'id': 90773, 'copy': SHOCKWAVE, 'name': 'Thunder Echo', 'icon': 'Spell_Nature_ThunderClap',
        'description': '', 'tooltip': '',
        'set': {**FREE, 'SpellFamilyName': 0, 'SpellFamilyFlags0': 0, 'SpellFamilyFlags1': 0, 'SpellFamilyFlags2': 0,
                'SpellVisual': VISUAL_SHOCKWAVE, 'RangeIndex': 1, 'ProcFlags': 0},
        'effects': {1: {'Effect': EFFECT_DUMMY, 'Aura': 0, 'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0,
                        'TargetA': 1, 'TargetB': 0, 'RadiusIndex': 0}},
        'clear': [2, 3],
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
        'description': 'Movement speed reduced by 15%.', 'tooltip': 'Movement speed reduced by $s1%.',
        'set': {**HARMFUL, 'DurationIndex': DURATION_2S, 'SchoolMask': NATURE},
        'effects': {1: {**aura(AURA_MOD_DECREASE_SPEED, -15, 0), 'TargetA': TARGET_UNIT_TARGET_ENEMY}},
        'clear': [2, 3],
    },
]
THUNDER_MODIFIERS = [
    ('Lingering Thunder', 'March of the Storm', 'Rolling Front'),
    ('Reverberating Impact', 'Focused Peal', 'Distant Rumble'),
    ('Commanding Thunder', 'Sheltering Reverberation', 'Deafening Impact'),
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
         'When Chain Heal heals the wearer of your Earth Shield, they become a Wellspring for 6 sec. Your next Chain '
         'Heal to reach them jumps once more, to an injured ally, for 30% of the healing they received.',
         DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    {
        **buff(90791, 'Wellspring', 'Spell_Nature_HealingWaveGreater',
               'A Wellspring: your Chain Heal reaching it flows on to another ally.', DURATION_10S,
               {1: aura(AURA_DUMMY, 1, 0)}),
        'set': {**SHOWN, 'DurationIndex': DURATION_10S, 'SpellVisual': VISUAL_RIPTIDE},
    },
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
         'While you channel Soothing Mist, its healing ticks have a 25% chance to send a Wandering Mist to another '
         'injured ally within 20 yards of your channel target, healing for 30% of the tick, once every 2 sec. '
         'Enveloping Mist and Vivify on your Soothing Mist target are cast at once, without breaking the channel.',
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
    periodic_heal(90813, 'Rolling Mist', 'Ability_Monk_SoothingMists', 'A lingering mist.', DURATION_4S),
    absorb(90814, 'Jade Shelter', 'Ability_Monk_SoothingMists', DURATION_4S),
]
WELLSPRING_MODIFIERS = [
    ('Flowing Waters', 'Lasting Spring', 'Widening Waters'),
    ('Deep Current', 'Split Tributary', 'Concentrated Current'),
    ('Tidal Shelter', 'Long Reach', 'Renewing Current'),
]
MIST_MODIFIERS = [
    ('Flowing Currents', 'Gathering Mists', 'Drifting Mist'),
    ('Comforting Veil', 'Vivacious Flow', 'Peaceful Focus'),
    ('Rolling Mist', 'Jade Shelter', 'Effortless Breathing'),
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


def strike(spell_id, name, icon, school, visual=0, text='', key=None):
    """Damage on an enemy (its amount set by the script, the Hero's power already in it)."""
    spell = {'id': spell_id, 'copy': LIGHTNING_BOLT, 'name': name, 'icon': icon, 'description': text,
             'tooltip': '', 'set': {**HARMFUL, 'Speed': 0.0, 'SchoolMask': school, 'SpellVisual': visual,
                                    'AttributesEx3': SPELL_ATTR3_ALWAYS_HIT | SPELL_ATTR3_IGNORE_CASTER_MODIFIERS,
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
    return harmful(spell_id, name, icon, text, duration,
                   {**aura(AURA_PERIODIC_DAMAGE, 1, 0), 'Amplitude': 1000}, school)


def ground(spell_id, name, icon, key, duration):
    """A patch of ground the script lays (a dynamic object: its look only)."""
    return {'id': spell_id, 'copy': CONSECRATION, 'name': name, 'icon': icon, 'description': '', 'tooltip': '',
            'visual': key, 'set': {**FREE, **NO_FAMILY, 'DurationIndex': duration, 'RangeIndex': RANGE_VISION},
            'effects': {1: {'Effect': EFFECT_PERSISTENT_AREA_AURA, 'Aura': AURA_DUMMY, 'BasePoints': 0, 'DieSides': 1,
                            'RealPointsPerLevel': 0.0, 'Amplitude': 0, 'TargetA': TARGET_DEST_CASTER,
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
                      'Mortal Strike and Bloodthirst cut a Weapon Wake through their target for 6 sec. The next '
                      'different melee ability to strike an enemy in the wake reopens it: another attack along it, '
                      '60% of a weapon swing to each enemy, its first target 25% more.',
                      DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)})
# Its procs: every melee ability that hits; the script picks the ones that reopen the wake.
ONSLAUGHT_RUNE['set'] = {**ONSLAUGHT_RUNE['set'], 'ProcFlags': PROC_DONE_MELEE_SPELL, 'ProcChance': 101}
DOMINIONS = (('Rime', 'Spell_Deathknight_FrostPresence', 'Your scars freeze and slow the enemies in them.'),
             ('Blight', 'Spell_Deathknight_UnholyPresence', 'Your scars sicken the enemies in them.'),
             ('Gore', 'Spell_Deathknight_BloodPresence',
              'Your scars bleed the enemies in them; Death Strike in a scar grants a Grave Ward.'))
SPELLS += [
    buff(90830, 'Rune of the Threefold Grave', GRAVE_ICON,
         'Icy Touch inscribes a frozen line beneath its target, Plague Strike a diseased patch, Death Strike a blood '
         'circle beneath you: one of each, for 12 sec. Requiem turns your Dominion, Rime to Blight to Gore, and '
         'every scar changes with it.', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    action(90831, 'Requiem', 'Spell_Deathknight_ClassIcon',
           'Turn your Dominion: Rime to Blight to Gore. The Dominion you leave bids its farewell.', 8000),
    *[{**action(90832 + i, f'Requiem: {name}', icon, f'Your Dominion becomes {name}. The one you leave bids its '
                'farewell. Shares a 20 sec cooldown with the other Requiems.', 20000)}
      for i, (name, icon, _) in enumerate(DOMINIONS)],
    *[shown(90835 + i, f'Dominion of {name}', icon, text, DURATION_INFINITE, key=f'Dominion{name}')
      for i, (name, icon, text) in enumerate(DOMINIONS)],
    strike(90838, 'Rime', 'Spell_Deathknight_IceTouch', FROST, key='GraveRimeHit'),
    harmful(90839, 'Rime', 'Spell_Frost_ChillingBlast', 'Movement speed reduced by $s1%.', DURATION_2S,
            aura(AURA_MOD_DECREASE_SPEED, -30, 0), FROST),
    strike(90840, 'Blight', 'Spell_Shadow_PlagueCloud', SHADOW, key='GraveBlightHit'),
    dot(90841, 'Residual Infection', 'Spell_Shadow_CreepingPlague', SHADOW, DURATION_6S,
        'Shadow damage every 1 sec.'),
    strike(90842, 'Gore', 'Spell_Deathknight_BloodBoil', SHADOW, key='GraveGoreHit'),
    {**absorb(90843, 'Grave Ward', 'Spell_Deathknight_DeathStrike', DURATION_10S),
     'set': {**HEAL_SET, 'DurationIndex': DURATION_10S, 'SchoolMask': SHADOW}},
    look(90844, 'Rime', 'Spell_Frost_FrostNova', VISUAL_FROST_NOVA),
    ground(90845, 'Frozen Line', 'Spell_Deathknight_IceTouch', 'GraveFrozenLine', DURATION_20S),
    ground(90846, 'Diseased Patch', 'Spell_Shadow_PlagueCloud', 'GraveDiseasedPatch', DURATION_20S),
    ground(90847, 'Blood Circle', 'Spell_Deathknight_BloodBoil', 'GraveBloodCircle', DURATION_20S),
    shown(90848, 'Blood Circle', 'Spell_Deathknight_BloodBoil', 'Your blood circle follows you.', DURATION_20S,
          key='GraveBloodMarch'),
    harmful(90849, 'Cruel Winter', 'Spell_Frost_FrostNova', 'Frozen in place.', DURATION_2S,
            aura(AURA_MOD_ROOT, 1, 0), FROST),

    ONSLAUGHT_RUNE,
    strike(90861, 'Weapon Wake', 'Ability_Warrior_Cleave', PHYSICAL, key='WakeHit'),
    dot(90862, 'Bloodletting', 'Ability_Gouge', PHYSICAL, DURATION_6S, 'Bleeding every 1 sec.'),
    ground(90863, 'Weapon Wake', 'Ability_Warrior_Cleave', 'OnslaughtWake', DURATION_10S),
    strike(90864, 'Aftershock', 'Ability_Warrior_Shockwave', PHYSICAL, VISUAL_THUNDER_HIT),

    buff(90890, 'Rune of the Dawnbound Crusader', DAWN_ICON,
         'Dawn Vow binds an ally (or you) as the point your power rises from. Judgment marks an enemy: Crusader '
         'Strike on it sends a Dawn Echo from your vow, half of its damage, and Holy Shock one at once.',
         DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    action(90891, 'Dawn Vow', 'Ability_Paladin_BeaconofLight',
           'Swear your Dawn Vow to an ally, or yourself: your Dawn Echoes rise from them.', 10000,
           target=TARGET_UNIT_TARGET_ALLY, range_index=RANGE_40, visual=VISUAL_BEACON),
    shown(90892, 'Dawn Vow', 'Ability_Paladin_BeaconofLight', 'A Dawnbound Crusader\'s echoes rise from you.',
          DURATION_INFINITE, key='DawnVow'),
    harmful(90893, 'Dawn Mark', 'Spell_Holy_RighteousFury', 'Marked by Judgment: Crusader Strike sends Dawn Echoes.',
            DURATION_15S, aura(AURA_DUMMY, 1, 0), HOLY),
    heal(90894, 'Dawn Echo', 'Spell_Holy_SearingLight', 'An echo of the dawn.'),
    strike(90895, 'Dawn Echo', 'Spell_Holy_SearingLight', HOLY),
    look(90896, 'Dawn Echo', 'Spell_Holy_SearingLight', VISUAL_HOLY_SHOCK, TARGET_UNIT_TARGET_ANY),
    look(90897, 'Dawn Echo', 'Spell_Holy_SearingLight', VISUAL_HOLY_SHOCK_HEAL, TARGET_UNIT_TARGET_ANY),
    look(90898, 'Sunburst', 'Spell_Holy_HolyBolt', VISUAL_HOLY_NOVA),
    # Sacred Ground: Consecration's own ground.
    {key: value for key, value in ground(90899, 'Sacred Ground', 'Spell_Holy_InnerFire', None, DURATION_6S).items()
     if key != 'visual'},
    shown(90900, 'Afterimage', 'Spell_Holy_AuraMastery', 'Your next Dawn Echo repeats at half strength.',
          DURATION_6S, key='DawnAfterimage'),

    buff(90920, 'Rune of the Hollow Congregation', HOLLOW_ICON,
         'Two seconds into Mind Flay, a Hollow Echo stays where you stand and channels a weaker Mind Flay for 10 '
         'sec (two at most). Mind Blast turns every echo on its target in a short barrage; then they channel again.',
         DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    {**shown(90921, 'Hollow Echo', 'Spell_Shadow_Shadowform', '', DURATION_INFINITE, key='HollowEcho',
             effects={1: aura(AURA_MOD_THREAT, -100, ALL_SCHOOLS)}),
     'set': {**HIDDEN, 'DurationIndex': DURATION_INFINITE}},
    {
        'id': 90922, 'copy': MIND_FLAY, 'name': 'Hollow Flay', 'icon': 'Spell_Shadow_SiphonMana',
        'description': 'A hollow echo of Mind Flay.', 'tooltip': 'Shadow damage every 1 sec.',
        'set': {**FREE, **NO_FAMILY, 'RangeIndex': RANGE_VISION, 'SchoolMask': SHADOW, 'ProcFlags': 0,
                'InterruptFlags': 0, 'AuraInterruptFlags': 0, 'ChannelInterruptFlags': 0, **SCRIPTED},
        'effects': {1: {**aura(AURA_PERIODIC_DAMAGE, 1, 0), 'Amplitude': 1000,
                        'TargetA': TARGET_UNIT_TARGET_ENEMY}},
        'clear': [2, 3],
    },
    # The barrage's bolt: Shadow Bolt's, flying.
    {**strike(90923, 'Hollow Barrage', 'Spell_Shadow_UnholyFrenzy', SHADOW, VISUAL_SHADOW_BOLT),
     'copy': SHADOW_BOLT},
    strike(90924, 'Accusation', 'Spell_Shadow_PsychicScream', SHADOW, VISUAL_MIND_BLAST),
    strike(90925, 'Last Word', 'Spell_Shadow_DemonicFortitude', SHADOW, VISUAL_MIND_BLAST),

    buff(90950, 'Rune of the Shattered Sun', SUN_ICON,
         'Fireball embeds a burning Sunshard in its target (two at most). Frostbolt on it fractures the shard into a '
         'ring of burning ice; the enemy keeps a Broken Core, and your next Fireball on it collapses it inward.',
         DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
    harmful(90951, 'Sunshard', 'Ability_Mage_LivingBomb', 'A burning Sunshard: Frostbolt fractures it.',
            DURATION_15S, aura(AURA_DUMMY, 1, 0), FIRE, key='Sunshard'),
    harmful(90952, 'Broken Core', 'Ability_Mage_FrostFireBolt', 'A Broken Core: Fireball collapses it.',
            DURATION_6S, aura(AURA_DUMMY, 1, 0), FROSTFIRE, key='BrokenCore'),
    strike(90953, 'Fracture', 'Spell_Frost_FrostNova', FROSTFIRE),
    look(90954, 'Fracture', 'Spell_Frost_FrostNova', VISUAL_FROST_NOVA),
    strike(90955, 'Collapse', 'Spell_Fire_SoulBurn', FROSTFIRE),
    look(90956, 'Collapse', 'Spell_Fire_SoulBurn', VISUAL_BLAST_WAVE),
    ground(90957, 'Scalding Ground', 'Spell_Fire_Incinerate', 'ScaldingGround', DURATION_6S),
    ground(90958, 'Solar Anchor', 'Ability_Mage_LivingBomb', 'SolarAnchor', DURATION_15S),
]
LEGEND_MODIFIERS = [
    (90850, GRAVE_ICON, [('Converging Graves', 'March of the Dead', 'Long Burial'),
                         ('Cruel Winter', 'Grave Pestilence', 'Crimson Bastion'),
                         ('Reverse the Dirge', 'Lingering Dominion', 'Chosen Ending')]),
    (90880, ONSLAUGHT_ICON, [("Executioner's Cut", 'Whirling Steel', 'Raking Passage'),
                             ('Overpowering Echo', 'Bloodletting', 'Aftershock'),
                             ('Relentless', 'Battlefield Claim', 'Changing Prey')]),
    (90910, DAWN_ICON, [('Mercy', 'Retribution', 'Communion'),
                        ('Dawnwave', 'Sunburst', 'Sacred Ground'),
                        ('Steadfast', 'Passing the Torch', 'Shared Pilgrimage')]),
    (90940, HOLLOW_ICON, [('Following Shadows', 'Haunted Ground', 'Possession'),
                          ('Unison', 'Dissonance', 'Accusation'),
                          ('Last Word', 'Procession', 'Silent Witness')]),
    (90970, SUN_ICON, [('Twin Suns', 'Orbiting Sun', 'Solar Anchor'),
                       ('Splinterstorm', 'Solar Lance', 'Scalding Ground'),
                       ('Implosion', 'Chain Reaction', 'Second Sunrise')]),
]
next(spell for spell in SPELLS if spell['id'] == 90923)['set']['Speed'] = 20.0
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


def blood_circle(model, effect):
    return {'model': model, 'effect': effect, 'shape': 'zone', 'textures': (RING, SIGIL),
            'options': {'layers': (
                {'texture': 0, 'size': 12.0, 'height': 0.1, 'tint': BLOOD, 'alpha': 0.9, 'blend': 4},
                {'texture': 1, 'size': 11.5, 'height': 0.14, 'tint': BLOOD, 'alpha': 0.55, 'turn': 30.0,
                 'blend': 4})}}


EFFECTS.update({
    'GraveFrozenLine': {'model': 'DK_GraveFrozenLine', 'effect': 7159, 'shape': 'zone', 'textures': (CRACKS, CLOUDS),
                        'options': {'layers': (
                            {'texture': 0, 'size': 4.5, 'height': 0.1, 'tint': ICE, 'alpha': 0.85, 'blend': 4},
                            {'texture': 1, 'size': 4.0, 'height': 0.16, 'tint': ICE, 'alpha': 0.4, 'turn': 30.0,
                             'round': True, 'blend': 4})}},
    'GraveDiseasedPatch': {'model': 'DK_GraveDiseasedPatch', 'effect': 7160, 'shape': 'zone',
                           'textures': (CLOUDS, CRACKS),
                           'options': {'layers': (
                               {'texture': 0, 'size': 10.0, 'height': 0.12, 'tint': PLAGUE, 'alpha': 0.6,
                                'turn': 40.0, 'round': True, 'blend': 4},
                               {'texture': 1, 'size': 10.0, 'height': 0.1, 'tint': PLAGUE, 'alpha': 0.5,
                                'turn': -60.0, 'blend': 4})}},
    'GraveBloodCircle': blood_circle('DK_GraveBloodCircle', 7161),
    'GraveBloodMarch': blood_circle('DK_GraveBloodMarch', 7162),
    'OnslaughtWake': {'model': 'WAR_OnslaughtWake', 'effect': 7163, 'shape': 'zone',
                      'textures': ('WAR_UnbrokenOnslaught/wake_gash',),
                      'options': {'layers': (
                          {'texture': 0, 'size': 3.4, 'height': 0.12, 'tint': WAKE_RED, 'alpha': 0.95,
                           'blend': 4},)}},
    'ScaldingGround': {'model': 'MAGE_ScaldingGround', 'effect': 7164, 'shape': 'zone', 'textures': (CRACKS, CLOUDS),
                       'options': {'layers': (
                           {'texture': 0, 'size': 12.0, 'height': 0.1, 'tint': FROSTFIRE_TINT, 'alpha': 0.8,
                            'turn': 50.0, 'blend': 4},
                           {'texture': 1, 'size': 11.0, 'height': 0.16, 'tint': (1.0, 0.4, 0.6), 'alpha': 0.5,
                            'turn': -25.0, 'round': True, 'blend': 4})}},
    'SolarAnchor': {'model': 'MAGE_SolarAnchor', 'effect': 7165, 'shape': 'zone', 'textures': (GLYPH,),
                    'options': {'layers': (
                        {'texture': 0, 'size': 3.0, 'height': 0.15, 'tint': SUN_GOLD, 'turn': 8.0, 'blend': 4},)}},
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
    'SolarAnchor': {'copy': 9735, 'id': 16771, 'clear': ('precast', 'state', 'area'), 'kits': {
        'persistent': (15720, {KIT_WORLD: 'SolarAnchor', 'procs': False})}},
    # Icy Touch's, Plague Strike's and Blood Boil's hits without their casts on the Hero; Cleave's hit.
    'GraveRimeHit': {'copy': 11152, 'id': 16772, 'clear': ('precast', 'cast'), 'kits': {}},
    'GraveBlightHit': {'copy': 11624, 'id': 16773, 'clear': ('precast', 'cast', 'state'), 'kits': {}},
    'GraveGoreHit': {'copy': 11117, 'id': 16774, 'clear': ('precast', 'cast'), 'kits': {}},
    'WakeHit': {'copy': 219, 'id': 16775, 'clear': ('precast', 'cast'), 'kits': {}},
    # The Dominions: each presence's flash as it turns.
    'DominionRime': {'copy': 11115, 'id': 16776, 'clear': ('precast', 'cast'), 'kits': {}},
    'DominionBlight': {'copy': 11116, 'id': 16777, 'clear': ('precast', 'cast'), 'kits': {}},
    'DominionGore': {'copy': 11114, 'id': 16778, 'clear': ('precast', 'cast'), 'kits': {}},
    # States: Power Infusion's glow on the vow, Hand of Freedom's on its afterimage, the NPC Shadowform on a Hollow
    # Echo, Living Bomb's on a Sunshard, Deep Freeze's on a Broken Core.
    'DawnVow': {'copy': 7553, 'id': 16779, 'clear': ('precast', 'cast', 'impact'), 'kits': {}},
    'DawnAfterimage': {'copy': 4050, 'id': 16780, 'clear': ('precast', 'cast', 'impact'), 'kits': {}},
    'HollowEcho': {'copy': 3619, 'id': 16781, 'clear': ('precast', 'cast', 'impact'), 'kits': {}},
    'Sunshard': {'copy': 10692, 'id': 16782, 'clear': ('precast', 'cast', 'impact'), 'kits': {}},
    'BrokenCore': {'copy': 9963, 'id': 16783, 'clear': ('precast', 'cast', 'impact'), 'kits': {}},
})

ICONS = [RUNE_ICON, PARAGON_ICON, STORM_ICON, THUNDER_ICON, WELLSPRING_ICON, MIST_ICON,
         GRAVE_ICON, ONSLAUGHT_ICON, DAWN_ICON, HOLLOW_ICON, SUN_ICON,
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
