#!/usr/bin/env python3
"""Legendary Runes' spells (src/runes.cpp): data for patch_spells.py, which builds and writes them with the rest.

The Rune of Fel Commandment (the design: the "Emberforge Legendary Runes" doc, its Fel Commandment tab), 90700 on:

- 90700 the rune: a buff the Hero wears while their active specialization's rune is Fel Commandment; its procs
  (Demon's Bite, Chaos Strike; Shadow Bolt and Incinerate with Dreadfire Discipline) grant Fel Command
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
               'Demon\'s Bite, Chaos Strike and your demon\'s attacks grant Fel Command, up to 5. At 5, Throw Glaive '
               'becomes Chaos Bolt Volley.', DURATION_INFINITE, {1: aura(AURA_DUMMY, 1, 0)}),
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
# low wall at the aura's edge with the middle left clear (hollow, its edge faded in; stretch: the Hero and what is
# about them stay in sight), faint as a mist; flakes of light tumbling up through the aura (retail's flipping snowflake, an 8-frame flipbook), embers and
# sparks; a crown of light above the head; a shockwave and flash as it wakes (90749). No floor. The sigil and glyph of Seat of the Paragon and Hallowed Footsteps are ours (build_paragon_art.py).
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
                              {'source': 0, 'mesh': 2, 'texture': 0, 'tint': GOLD, 'alpha': 0.07,
                               'turn': 1.6, 'scroll': (-0.6, 0.0), 'hollow': 1.8, 'stretch': (2.8, 2.8, 0.42)},
                              {'source': 0, 'mesh': 2, 'texture': 0, 'tint': DEEP_GOLD, 'alpha': 0.06, 'turn': 2.4,
                               'scroll': (0.4, 0.1), 'hollow': 1.8, 'stretch': (2.6, 2.6, 0.38)},
                              # blades of light whipping round that wall
                              {'source': 0, 'mesh': 2, 'texture': 1, 'tint': PALE_GOLD, 'alpha': 0.12, 'turn': 1.3,
                               'hollow': 1.8, 'stretch': (2.7, 2.7, 0.4)},
                              {'source': 0, 'mesh': 2, 'texture': 1, 'tint': GOLD, 'alpha': 0.09, 'turn': -1.9,
                               'flicker': (0.45, 0.3), 'hollow': 1.8, 'stretch': (2.5, 2.5, 0.35)},
                              # light rising at the boundary (aura01's funnel, its outer part, low)
                              {'source': 1, 'mesh': 0, 'texture': 2, 'tint': GOLD, 'alpha': 0.07, 'turn': 5.0,
                               'scroll': (0.0, -0.8), 'hollow': 2.6, 'stretch': (1.85, 1.85, 0.35)})}},
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
}

ICONS = [RUNE_ICON, PARAGON_ICON, 'Spell_Holy_InnerFire', 'Spell_Holy_RighteousFury', 'Spell_Holy_HealingAura',
         'Spell_Holy_DevotionAura', 'Spell_Holy_SealOfWrath', 'Spell_Holy_PowerWordShield', 'Spell_Holy_HolyGuidance',
         'Spell_Fire_FelFlameRing', 'Ability_Warlock_ChaosBolt', 'Spell_Shadow_SummonFelGuard',
         'Spell_Shadow_SummonImp', 'Ability_Warlock_DemonicEmpowerment', 'Spell_Shadow_DemonForm',
         'Spell_Shadow_DemonicCircleTeleport', 'Ability_Warrior_Cleave']
assert patch_player  # the helpers' module
