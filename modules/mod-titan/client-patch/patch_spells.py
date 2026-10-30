#!/usr/bin/env python3
"""Add Emberforge's own spells (the Demon Hunter's) to Spell.dbc and SpellIcon.dbc (WoW 3.3.5a),
their own visuals to SpellVisual.dbc, SpellVisualKit.dbc and SpellVisualEffectName.dbc, and their own
sounds to SoundEntries.dbc.

Each spell is a copy of a Blizzard spell (its attributes, visuals, targeting) with some fields
changed, so it behaves and looks like a real ability. The same rows go to the client (the patched
DBCs, in the client patch) and to the server (`spell_dbc` rows, written with --sql to
data/sql/db-world/): both must agree on every spell.

Resources: Emberforge has no Runic Power: its power type (6) is Fury, which every Hero has
(src/hero_resources.cpp). Every client spell text that says Runic Power says Fury instead (the
Emberforge_Resources addon renames it in the UI).

Visuals: a spell with a "visual" gets one of VISUALS, a copy of a Blizzard visual whose kits carry
our trails and Blizzard's own effect models. The trail models are built by models/spell_effects.py
from spells/<model>.png (build_patch.py puts them in the client patch under Spells\\Emberforge).

Usage: python3 patch_spells.py <dir with original DBCs> <output dir> [--sql <out.sql>]
"""

import re
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SPELL_FIELDS = 234
ICON_FIELDS = 2
FIRST_ICON_ID = 4400            # SpellIcon.dbc ends at 4375

# Spell.dbc columns (3.3.5a, as AzerothCore's SpellEntry / spell_dbc), by name. Effects are 1-3.
FIELDS = {
    'Category': 1, 'Attributes': 4, 'AttributesEx': 5, 'AttributesEx2': 6, 'AttributesEx3': 7,
    'AttributesEx4': 8, 'AttributesEx5': 9, 'AttributesEx6': 10, 'AttributesEx7': 11, 'Stances': 12, 'StancesNot': 14,
    'CastingTimeIndex': 28, 'RecoveryTime': 29, 'CategoryRecoveryTime': 30, 'InterruptFlags': 31,
    'AuraInterruptFlags': 32, 'ChannelInterruptFlags': 33, 'MaxLevel': 37, 'BaseLevel': 38,
    'SpellLevel': 39, 'DurationIndex': 40, 'PowerType': 41, 'ManaCost': 42, 'RangeIndex': 46,
    'EquippedItemClass': 68, 'EquippedItemSubClassMask': 69,
    'EquippedItemInventoryTypeMask': 70, 'SpellVisual': 131, 'SpellIconID': 133,
    'ActiveIconID': 134, 'ManaCostPercentage': 204, 'StartRecoveryCategory': 205, 'StartRecoveryTime': 206,
    'SpellFamilyName': 208, 'SpellFamilyFlags0': 209, 'SpellFamilyFlags1': 210, 'SpellFamilyFlags2': 211,
    'MaxAffectedTargets': 212, 'DmgClass': 213, 'Speed': 47, 'SchoolMask': 225, 'RuneCostID': 226,
}
EFFECT_FIELDS = {       # name: (first column, is float); effect n is first column + n - 1
    'Effect': (71, False), 'DieSides': (74, False), 'RealPointsPerLevel': (77, True), 'BasePoints': (80, False),
    'Mechanic': (83, False), 'TargetA': (86, False), 'TargetB': (89, False), 'RadiusIndex': (92, False),
    'Aura': (95, False), 'Amplitude': (98, False), 'ValueMultiplier': (101, True), 'ChainTarget': (104, False),
    'ItemType': (107, False), 'MiscValue': (110, False), 'MiscValueB': (113, False), 'TriggerSpell': (116, False),
    'PointsPerComboPoint': (119, True), 'DamageMultiplier': (216, True), 'BonusMultiplier': (229, True),
}
CLASS_MASK = 122        # 3 x 3 columns: the effects' spell class masks
NAME, RANK, DESCRIPTION, TOOLTIP = 136, 153, 170, 187     # 16 locales each, then a flags column
FLOAT_COLUMNS = {47} | {first + n for first, flt in EFFECT_FIELDS.values() if flt for n in range(3)}
STRING_COLUMNS = {base + n for base in (NAME, RANK, DESCRIPTION, TOOLTIP) for n in range(16)}

# SpellVisual.dbc (32 columns), SpellVisualKit.dbc (38), SpellVisualEffectName.dbc (7: id, name,
# file, area size, scale, min scale, max scale).
VISUAL_FIELDS, KIT_FIELDS, EFFECT_NAME_FIELDS = 32, 38, 7
# SpellVisual columns; 'persistent' is the kit a persistent area (a dynamic object) shows for as long as it lasts,
# 'area' the one it shows (a sound) as it appears.
VISUAL_MISSILE_MODEL, VISUAL_MISSILE_SOUND = 8, 11
VISUAL_KITS = {'precast': 1, 'cast': 2, 'impact': 3, 'state': 4, 'statedone': 5, 'area': 23, 'persistent': 25}
KIT_HEAD, KIT_CHEST, KIT_BASE, KIT_LEFT_HAND, KIT_RIGHT_HAND, KIT_BREATH = 3, 4, 5, 6, 7, 8
KIT_LEFT_WEAPON, KIT_RIGHT_WEAPON, KIT_WORLD = 9, 10, 14
HANDS, WEAPONS = (KIT_LEFT_HAND, KIT_RIGHT_HAND), (KIT_LEFT_WEAPON, KIT_RIGHT_WEAPON)
KIT_ANIM = 2
KIT_PROCS, KIT_PROC_PARAMS = range(17, 21), range(21, 37)   # character effects (tints, ground decals)
NO_ANIM = 0xFFFFFFFF
# The Demon Hunter's own animations, in slots no Blizzard model uses: the retail ones, added to the character
# models (models/retarget_character.py) and the demon forms (models/creature_gltf_to_m2.py). A model without
# them (a race not converted yet) stands still instead.
ANIM_BLADE_DANCE, ANIM_FEL_RUSH, ANIM_BACK_FLIP, ANIM_GLAIVE_THROW = 215, 216, 217, 219
ANIM_GLAIVE_STRIKE, ANIM_GLAIVE_BIG_STRIKE = 220, 221
# Their AnimationData rows (the client's) are made like the Blizzard animation each stands in for, which is also
# the fallback a model without the new one plays: its weapon and body flags make it an action that plays over
# the idle and ready stances (a custom spell row has none: full body, lowest priority). Body flags, as Blizzard
# uses them: 0x20 plays on the upper body over running legs (attacks, specials), 0x8 lets a hit's flinch
# (CombatWound) cut it short (attacks, casts), 0x100 an action. A moving character's legs always play the movement
# animation: without 0x20 an action does not play at all while moving (tried for Blade Dance: a whole-body 0x100,
# and Knockdown's 0x180, which also stops the player in place). Blade Dance's full body while moving is the demon
# form's Blade Dance variant (models/creature_gltf_to_m2.py).
# (slot, Blizzard animation copied, body flags or None to copy them too)
BODY_UPPER = 0x120
ANIMATION_ROWS = [
    (ANIM_BLADE_DANCE, 126, None),              # Whirlwind: upper body over running legs
    (ANIM_FEL_RUSH, 126, None),                 # Whirlwind's flags; no fallback (Fel Rush had no animation)
    (ANIM_BACK_FLIP, 37, None),                 # JumpStart
    (ANIM_BACK_FLIP + 1, 38, None),             # Jump
    (ANIM_GLAIVE_THROW, 107, BODY_UPPER),       # AttackThrown, not cut by hits
    (ANIM_GLAIVE_STRIKE, 17, BODY_UPPER),       # Attack1H, not cut by hits
    (ANIM_GLAIVE_BIG_STRIKE, 57, None),         # Special1H (not cut by hits already)
]
ANIMATION_FIELDS = 8
ANIM_WEAPON_FLAGS, ANIM_BODY_FLAGS, ANIM_FALLBACK = 2, 3, 5   # weapon, body, flags, fallback: columns 2 to 5
BLANK_KIT = 3049    # Sinister Strike's impact: a chest effect and its hit sound (set them to 0 / None)
EFFECTS_DIR = 'Spells\\Emberforge'     # client folder of our effect models
# SoundEntries.dbc (30 columns): id, type, name, 10 files, 10 weights, folder, volume, flags, min
# distance, distance cutoff, EAX, advanced id.
SOUND_FIELDS = 30
SOUND_NAME, SOUND_FILES, SOUND_WEIGHTS, SOUND_FOLDER = 2, 3, 13, 23
SOUNDS_DIR = 'Sound\\Spells\\Emberforge'     # client folder of our sounds (from sounds/*.wav)
KIT_SOUND = 15

# Values used below.
CHAOS = 0x7F                    # every school
PHYSICAL = 0x1
DMG_CLASS_MELEE = 2
FURY = 6                        # the client's Runic Power, renamed
ONE_HANDED_WEAPONS = 0xA091     # one-handed axes, maces and swords, fist weapons, daggers
ITEM_CLASS_WEAPON = 2
SPELL_ATTR3_REQUIRES_MAIN_HAND_WEAPON = 0x400
EFFECT_SCHOOL_DAMAGE, EFFECT_APPLY_AURA, EFFECT_ENERGIZE = 2, 6, 30
EFFECT_WEAPON_PERCENT_DAMAGE, EFFECT_TRIGGER_SPELL, EFFECT_NORMALIZED_WEAPON_DMG = 31, 64, 121
EFFECT_CHARGE_DEST = 149
AURA_PERIODIC_TRIGGER_SPELL, AURA_MOD_DECREASE_SPEED, AURA_MOD_DODGE_PERCENT = 23, 33, 49
INTERRUPT_MOVEMENT = 0x1        # InterruptFlags: casting is stopped by moving
CHANNEL_INTERRUPT_MOVE = 0x8    # ChannelInterruptFlags: the channel is broken by moving
TARGET_UNIT_CASTER, TARGET_SRC_CASTER, TARGET_UNIT_SRC_AREA_ENEMY, TARGET_UNIT_CONE_ENEMY = 1, 22, 15, 104
TARGET_UNIT_TARGET_ENEMY = 6
DURATION_1S, DURATION_2S, DURATION_3S = 36, 39, 27
RANGE_30 = 4
GLOBAL_COOLDOWN = 133           # StartRecoveryCategory of spells on the global cooldown
EFFECT_DISPEL_MECHANIC, EFFECT_LEAP_BACK = 108, 138
MECHANIC_SNARE = 11
SPELLFAMILY_HUNTER = 9
BLEND_ADD = 4                   # M2 blend mode: additive
DURATION_10S, RANGE_20 = 1, 3
AURA_STUN, AURA_MOD_DAMAGE_PERCENT_TAKEN = 12, 87
AURA_MOD_ATTACKER_MELEE_HIT_CHANCE, AURA_MOD_ATTACKER_RANGED_HIT_CHANCE = 184, 185
AURA_DUMMY, AURA_MOD_DAMAGE_PERCENT_DONE, AURA_MOD_MELEE_RANGED_HASTE = 4, 79, 192
CAST_INSTANT = 1
ATTR0_DO_NOT_DISPLAY, ATTR0_DO_NOT_LOG = 0x80, 0x100
EFFECT_DISPEL, EFFECT_INTERRUPT_CAST = 38, 68
DISPEL_MAGIC = 1
ALL_SCHOOLS = 0x7F
RADIUS_8, RADIUS_15, RADIUS_20 = 14, 18, 9
FIRE, SHADOW = 0x4, 0x20
EFFECT_CHARGE, EFFECT_PERSISTENT_AREA_AURA, EFFECT_DUMMY = 96, 27, 3
AURA_SCHOOL_ABSORB = 69
TARGET_DEST_CASTER, TARGET_UNIT_DEST_AREA_ENEMY, TARGET_UNIT_DEST_AREA_ALLY = 18, 16, 31
RANGE_15, DURATION_6S, DURATION_8S = 11, 32, 31

# Icons: Interface\Icons\<name>. Ours come from icons/spells/<name>.png (in the client patch); the
# client already has the rest, found by name in SpellIcon.dbc.
ICONS = ['Ability_DemonHunter_ChaosStrike', 'Ability_DemonHunter_BladeDance', 'Ability_DemonHunter_FelRush',
         'Ability_DemonHunter_EyeBeam', 'INV_Weapon_Glave_01', 'Ability_DemonHunter_VengefulRetreat',
         'Ability_DemonHunter_ThrowGlaive', 'Spell_Fire_FelFireNova', 'Ability_DemonHunter_Blur',
         'Ability_DemonHunter_ConsumeMagic', 'Spell_Shadow_DemonForm',
         'Ability_DemonHunter_Metamorphosis', 'Ability_DemonHunter_Felblade', 'Ability_DemonHunter_Darkness',
         'Spell_Fire_Incinerate', 'INV_FelBarrage']

# Our effect models and their SpellVisualEffectName ids. "shape" picks the builder in
# models/spell_effects.py: "weapon" (weapon_trail, for both weapons; the default) or "body"
# (body_trail, for the chest), from spells/<model>.png, or "beam" (beam, for the head slot),
# "burst" (ground_burst, for the world slot) or "zone" (ground_zone, lasting on the ground) from the
# spells/<texture>.png in "textures" (beam: flare, flow; burst: ring, flash, trail or None; zone: those
# its layers name) or "missile" (missile, a spell missile from a wow.export model in spells/<export>, with a trail
# from the spells/<texture>.png in "textures"). "options" go to that builder.
EFFECTS = {
    'DemonsBite': {'model': 'DH_DemonsBite_Trail', 'effect': 7100},
    'ChaosStrike': {'model': 'DH_ChaosStrike_Trail', 'effect': 7101},
    # Blade Dance: the gray smoke trail, tinted fel green, for the whole spin.
    'BladeDance': {'model': 'DH_BladeDance_Trail', 'effect': 7103,
                   'options': {'duration': 900, 'lifetime': 0.3, 'color': (0.55, 1.0, 0.3)}},
    # Fel Rush: behind the body for the length of the dash.
    'FelRush': {'model': 'DH_FelRush_Trail', 'effect': 7102, 'shape': 'body', 'options': {'lifetime': 0.5}},
    # Vengeful Retreat: the burst left where the Demon Hunter leaps from. The trail is Blade Dance's
    # gray smoke tinted red until retail's (weapontrail_deathknight_marrowrend) is here.
    'VengefulRetreatBurst': {'model': 'DH_VengefulRetreat_Burst', 'effect': 7105, 'shape': 'burst',
                             'textures': ('DH_VengefulRetreat_Shockwave', 'DH_VengefulRetreat_FlowerStar',
                                          'DH_BladeDance_Trail')},
    # Chaos Nova: the void eruption swirl (retail's cfx_demonhunter_chaosnova_impact) on the ground,
    # tinted fel green, fading as retail's, with a fel flare at its center.
    'ChaosNovaBurst': {'model': 'DH_ChaosNova_Burst', 'effect': 7106, 'shape': 'burst',
                       'textures': ('DH_ChaosNova_Eruption', 'DH_ChaosNova_Flare', None),
                       'options': {'size': 16.0, 'ring_tint': (0.45, 1.0, 0.25),
                                   'ring_blend': BLEND_ADD, 'flash_sheet': False, 'flash_tint': (0.6, 1.0, 0.4),
                                   'ring': {'scale': ([0, 134, 850], [6.0, 16.0, 17.0]),
                                            'fade': ([0, 34, 134, 367, 634, 834, 900],
                                                     [0, 0, 18149, 18149, 4658, 0, 0])}}},
    # Felblade: a fel slash trail on both weapons (retail's texture).
    'Felblade': {'model': 'DH_Felblade_Trail', 'effect': 7107},
    # Darkness: retail's (cfx_demonhunter_darkness_state, its dark purple ground at 90%) under two slow,
    # opposite turning layers of its shadow smoke (rounded off) instead of the smoke particles.
    'Darkness': {'model': 'DH_Darkness', 'effect': 7108, 'shape': 'zone',
                 'textures': ('DH_Darkness_Base', 'DH_Darkness_Smoke'),
                 'options': {'layers': (
                     {'texture': 0, 'size': 17.0, 'height': 0.1, 'tint': (0.055, 0.0, 0.078), 'alpha': 0.9},
                     {'texture': 1, 'size': 16.0, 'height': 0.3, 'alpha': 0.75, 'turn': 24.0, 'round': True},
                     {'texture': 1, 'size': 12.0, 'height': 0.6, 'alpha': 0.6, 'turn': -16.0, 'round': True})}},
    # Immolation Aura: the cast's fel shockwave and flash (retail's cfx_demonhunter_immolationaura_castbase
    # textures).
    'ImmolationAuraBurst': {'model': 'DH_ImmolationAura_Burst', 'effect': 7109, 'shape': 'burst',
                            'textures': ('DH_ImmolationAura_Shockwave', 'DH_ImmolationAura_Flash', None),
                            'options': {'size': 16.0, 'ring_tint': (1.0, 1.0, 1.0), 'ring_blend': BLEND_ADD,
                                        'flash_sheet': False,
                                        'ring': {'scale': ([0, 300, 600], [1.0, 16.0, 17.0]),
                                                 'fade': ([0, 200, 600, 900], [0x7FFF, 0x7FFF, 0, 0])}}},
    # Fel Barrage: retail's fel ball (fel_balllightning_creature) as each pulse's missile, with Fel Rush's trail.
    'FelBarrageBall': {'model': 'DH_FelBarrage_Ball', 'effect': 7111, 'shape': 'missile',
                       'textures': ('DH_FelRush_Trail',),
                       'options': {'export': 'DH_FelBarrage_Ball', 'base': 'fel_balllightning_creature', 'size': 1.2,
                                   'trail_height': 0.3, 'lifetime': 0.4}},
    # Eye Beam: from the face, as long as the channel and its 20-yard reach.
    'EyeBeam': {'model': 'DH_EyeBeam', 'effect': 7104, 'shape': 'beam',
                'textures': ('DH_EyeBeam_Beam', 'DH_EyeBeam_Fel')},
}

# Sounds: SoundEntries ids, a Blizzard entry copied (volume, distances), and the files (sounds/,
# without .wav); the game plays one of them at random each time.
SOUNDS = {
    'BladeDance': {'id': 18100, 'copy': 5674, 'files': [f'spell_dh_bladedance_cast_0{n}' for n in range(1, 5)]},
    'ChaosStrike': {'id': 18101, 'copy': 3091, 'files': [f'spell_dh_chaos_strike_cast_0{n}' for n in range(1, 6)]},
    'FelRush': {'id': 18102, 'copy': 5674, 'files': [f'spell_dh_felrush_cast_0{n}' for n in range(1, 4)]},
    'DemonsBite': {'id': 18103, 'copy': 3091, 'files': [f'spell_dh_demonbite_cast_0{n}' for n in range(1, 6)]},
    'EyeBeamCast': {'id': 18104, 'copy': 5674, 'files': [f'spell_dh_player_eyebeam_cast_0{n}' for n in range(1, 5)]},
    'ThrowGlaiveImpact': {'id': 18107, 'copy': 5530,
                          'files': [f'spell_dh_throwglaives_impact_0{n}' for n in range(1, 6)]},
    'ThrowGlaiveCast': {'id': 18108, 'copy': 5530,
                        'files': [f'spell_dh_throw_glaives_cast_0{n}' for n in range(1, 5)]},
    'VengefulRetreat': {'id': 18109, 'copy': 5674,
                        'files': [f'spell_dh_vengefullretreat_cast_0{n}' for n in range(1, 4)]},
    'EyeBeamChannel': {'id': 18106, 'copy': 5674,
                       'files': [f'spell_dh_player_eyebeam_1shot_0{n}' for n in range(1, 4)]},
    'Metamorphosis': {'id': 18110, 'copy': 5674,
                      'files': [f'spell_dh_metamorph_jumpland_0{n}' for n in range(1, 5)]},
    'FelBarrage': {'id': 18111, 'copy': 5674, 'files': [f'spell_dh_fel_barrage_comp_cast_0{n}' for n in (5, 6)]},
    'EyeBeamEnd': {'id': 18105, 'copy': 5674, 'files': [f'spell_dh_player_eyebeam_end_0{n}' for n in range(1, 5)]},
}

# Blizzard effect models (SpellVisualEffectName ids) used below.
CONSUME_MAGIC_IMPACT = 3196     # Spells\\Consume_Magic_Impact: the Burning Crusade original, unused by any spell
MANGLE_IMPACT_DEMON = 4697      # Spells\\Mangle_Impact_Demon: a demon's bite
FEL_PRECAST_HAND = 3169         # Spells\\Fel_Fire_Precast_Hand: fel flames on the hands
FEL_FIRE_IMPACT = 3221          # Spells\\Fel_Fire_ImpactDD_High_Chest: a big fel fire hit
FEL_FLAMESHOCK_IMPACT = 3111    # Spells\\Fel_FlameShock_Impact_Chest: a small fel fire hit
FEL_FIREBLAST_IMPACT = 3227     # Spells\\Fel_FireBlast_Impact_Chest
FEL_HELLFIRE_BASE = 3312        # Spells\\Fel_Hellfire_Impact_Caster_Base: fel flames at the feet
XPLOSION_FEL_IMPACT = 4714      # spells\\Xplosion_Fel_Impact: a fel explosion
FEL_IMMOLATE_STATE = 3231       # Spells\\Fel_Immolate_State_Base: fel flames rising around the body

# Visuals: the Blizzard visual copied, the new visual's id, and per kit (precast, cast or impact)
# the new kit's id and what goes in its slots, a copy of the copied visual's kit (or of BLANK_KIT
# if it has none). A slot's effect is a Blizzard effect id (0: none) or one of EFFECTS; "anim" sets the kit's
# animation (NO_ANIM: none), "sound" its sound (one of SOUNDS, or None: silent), "procs": False drops the copied kit's
# character effects (its tints and ground decals). "clear" drops kits; "set" sets visual columns (a missile model: one
# of EFFECTS).
VISUALS = {
    'DemonsBite': {'copy': 253, 'id': 16700, 'kits': {
        'cast': (15600, {WEAPONS: 'DemonsBite', 'anim': ANIM_GLAIVE_STRIKE, 'sound': 'DemonsBite'}),
        'impact': (15604, {KIT_CHEST: MANGLE_IMPACT_DEMON, 'sound': None})}},
    'ChaosStrike': {'copy': 253, 'id': 16701, 'kits': {
        'cast': (15601, {WEAPONS: 'ChaosStrike', HANDS: FEL_PRECAST_HAND, 'anim': ANIM_GLAIVE_BIG_STRIKE,
                         'sound': 'ChaosStrike'}),
        'impact': (15605, {KIT_CHEST: FEL_FIRE_IMPACT, 'sound': None})}},
    # Blade Dance is a Whirlwind; it only targets the Demon Hunter (the dodge), so no impact: its
    # strikes (90107, 90108) bring their own.
    'BladeDance': {'copy': 223, 'id': 16703, 'clear': ('impact',), 'kits': {
        'cast': (15603, {WEAPONS: 'BladeDance', KIT_BASE: FEL_HELLFIRE_BASE, 'anim': ANIM_BLADE_DANCE,
                         'sound': 'BladeDance'})}},
    # The strikes of Blade Dance (and Vengeful Retreat's and Fel Rush's hits) are melee spells cast on their own:
    # without an animation of their own the client stabs with the weapon each time, over Blade Dance. Their cast
    # kit says no animation (Sinister Strike's swing is gone with its kit; the blank kit's effect and sound too).
    'BladeDanceStrike': {'copy': 253, 'id': 16704, 'clear': ('cast',), 'kits': {
        'cast': (15620, {KIT_CHEST: 0, 'anim': NO_ANIM, 'sound': None}),
        'impact': (15606, {KIT_CHEST: FEL_FLAMESHOCK_IMPACT, 'sound': None})}},
    'BladeDanceFinalStrike': {'copy': 253, 'id': 16705, 'clear': ('cast',), 'kits': {
        'cast': (15621, {KIT_CHEST: 0, 'anim': NO_ANIM, 'sound': None}),
        'impact': (15607, {KIT_CHEST: FEL_FIRE_IMPACT, 'sound': None})}},
    # Fel Rush was a Blink: fel hands instead of shadow ones, the trail behind the body, and the retail
    # dash pose. The warlock teleport flame stays where the dash starts.
    'FelRush': {'copy': 10694, 'id': 16702, 'clear': ('precast',), 'kits': {
        'cast': (15602, {KIT_CHEST: 'FelRush', HANDS: FEL_PRECAST_HAND, 'anim': ANIM_FEL_RUSH, 'sound': 'FelRush'})}},
    # Eye Beam is a Fel Breath; its channel is an aura on the Demon Hunter, kept for the channel
    # ("state"): the beam from the face, on the head (characters have no breath attachment; the
    # mage's Dragon's Breath does the same), instead of the breath's flames, and the channel sound
    # (cut to the 2 sec channel), over the cast sound. Its end ("statedone") plays the end sound.
    'EyeBeam': {'copy': 8536, 'id': 16707, 'kits': {
        'cast': (15609, {KIT_BREATH: 0, 'sound': 'EyeBeamCast'}),
        'state': (15611, {KIT_HEAD: 'EyeBeam', 'sound': 'EyeBeamChannel'}),
        'statedone': (15610, {KIT_CHEST: 0, 'sound': 'EyeBeamEnd'})}},
    # Throw Glaive is Heroic Throw's visual: the same throw animations as the night elf glaive throw,
    # but no missile model of its own, so the client throws (and spins) the caster's main-hand
    # weapon. Its sound on the throw, the impact sound (instead of Heroic Throw's) on each enemy.
    'ThrowGlaive': {'copy': 13222, 'id': 16708, 'kits': {
        'cast': (15613, {'anim': ANIM_GLAIVE_THROW, 'sound': 'ThrowGlaiveCast'}),
        'impact': (15612, {'sound': 'ThrowGlaiveImpact'})}},
    # Vengeful Retreat is Disengage (its leap animation is the precast kit); the cast kit, new,
    # carries the sound, Fel Rush's smoke trail behind the body for the leap, and the burst on the
    # ground where it starts.
    'VengefulRetreat': {'copy': 12305, 'id': 16709, 'kits': {
        'cast': (15614, {KIT_CHEST: 'FelRush', KIT_WORLD: 'VengefulRetreatBurst', 'anim': ANIM_BACK_FLIP,
                         'sound': 'VengefulRetreat'})}},
    # Chaos Nova is the Burning Crusade demons' (fel hands, the fel fire nova, fel fire on the
    # stunned), with our fel eruption left on the ground.
    'ChaosNova': {'copy': 8451, 'id': 16710, 'kits': {
        'cast': (15615, {KIT_WORLD: 'ChaosNovaBurst'})}},
    # Blur is Evasion with fel hands.
    'Blur': {'copy': 72, 'id': 16711, 'kits': {
        'cast': (15616, {HANDS: FEL_PRECAST_HAND})}},
    # Consume Magic is Counterspell with fel hands, and the original Consume Magic impact.
    'ConsumeMagic': {'copy': 239, 'id': 16712, 'kits': {
        'cast': (15617, {HANDS: FEL_PRECAST_HAND}),
        'impact': (15618, {KIT_CHEST: CONSUME_MAGIC_IMPACT})}},
    # Metamorphosis is the warlock's: its transformation burst (no shadow hands: it is instant), with
    # the Demon Hunter's landing roar instead of its sound.
    'Metamorphosis': {'copy': 12118, 'id': 16713, 'clear': ('precast',), 'kits': {
        'impact': (15619, {'sound': 'Metamorphosis'})}},
    # Felblade is Intercept's charge (its sound, the dust where it lands), with Fel Rush's trail behind the body,
    # the fel slash trail on the weapons and a big fel fire hit.
    'Felblade': {'copy': 29, 'id': 16714, 'kits': {
        'cast': (15623, {KIT_CHEST: 'FelRush', WEAPONS: 'Felblade', HANDS: FEL_PRECAST_HAND}),
        'impact': (15624, {KIT_CHEST: FEL_FIRE_IMPACT, 'sound': None})}},
    # Darkness is Death and Decay's shadow hands and its persistent area, the runes on the ground now the darkness;
    # none of the rest of Death and Decay (its ground decal, the tint on those in it, its area sound).
    'Darkness': {'copy': 9735, 'id': 16715, 'clear': ('precast', 'state', 'area'), 'kits': {
        'persistent': (15625, {KIT_WORLD: 'Darkness', 'procs': False})}},
    # Immolation Aura is the Metamorphosis warlock's: fel hands and the fel shockwave on the cast; while it lasts
    # (instead of its embers) the Demon Hunter burns: fel flames rising around the body (the Burning Crusade
    # demons' Fel Flames) and fel fire on the hands. Each pulse's hit is a small fel fire one.
    'ImmolationAura': {'copy': 12038, 'id': 16716, 'kits': {
        'cast': (15626, {HANDS: FEL_PRECAST_HAND, KIT_WORLD: 'ImmolationAuraBurst'}),
        'state': (15627, {KIT_HEAD: 0, KIT_CHEST: 0, KIT_BASE: FEL_IMMOLATE_STATE, HANDS: FEL_PRECAST_HAND})}},
    'ImmolationAuraPulse': {'copy': 11111, 'id': 16717, 'kits': {
        'impact': (15628, {KIT_BASE: 0, KIT_CHEST: FEL_FLAMESHOCK_IMPACT})}},
    # Fel Barrage: fel fire on the hands while it lasts, and one of its two sounds on the cast. Each pulse (no
    # animation: a triggered spell's own would stab with the weapon) throws a fel ball at every enemy, which
    # explodes on it: the Burning Crusade demons' Fel Fireball visual, with our ball as its missile (silent: a
    # dozen pulses at once) and a fel explosion as its hit.
    'FelBarrage': {'copy': 12038, 'id': 16718, 'kits': {
        'cast': (15629, {HANDS: FEL_PRECAST_HAND, 'sound': 'FelBarrage'}),
        'state': (15630, {KIT_HEAD: 0, KIT_CHEST: 0, KIT_BASE: 0, HANDS: FEL_PRECAST_HAND})}},
    'FelBarragePulse': {'copy': 253, 'id': 16719, 'clear': ('cast', 'impact'), 'kits': {
        'cast': (15631, {KIT_CHEST: 0, 'anim': NO_ANIM, 'sound': None})}},
    'FelBarrageBolt': {'copy': 10018, 'id': 16720, 'clear': ('precast', 'state'),
                       'set': {VISUAL_MISSILE_MODEL: 'FelBarrageBall', VISUAL_MISSILE_SOUND: 0}, 'kits': {
        'cast': (15632, {KIT_BREATH: 0, 'anim': NO_ANIM, 'sound': None}),
        'impact': (15633, {KIT_CHEST: XPLOSION_FEL_IMPACT})}},
    'FelRushHit': {'copy': 253, 'id': 16706, 'clear': ('cast',), 'kits': {
        'cast': (15622, {KIT_CHEST: 0, 'anim': NO_ANIM, 'sound': None}),
        'impact': (15608, {KIT_CHEST: FEL_FIREBLAST_IMPACT, 'sound': None})}},
}

# Spells: id, the Blizzard spell copied, name, tooltip text, aura text, icon, then column changes.
# "two_weapons": needs a one-handed weapon in each hand (the main hand checked here, the off hand by
# the server's spell_titan_dh_dual_weapons, src/demon_hunter.cpp).
# Effects not listed keep the copied spell's; "clear" empties effects (numbers 1-3); "unset" clears
# bits of the copied spell's flag columns.
SPELLS = [
    {
        'id': 90100, 'copy': 1752, 'name': "Demon's Bite", 'icon': 'INV_Weapon_Glave_01',
        'visual': 'DemonsBite', 'two_weapons': True,
        'description': 'Quickly attack for your weapon damage plus $s1 Physical damage. '
                       'Generates 20 to 30 Fury.',
        'set': {'Category': 0, 'AttributesEx': 0, 'PowerType': FURY, 'ManaCost': 0,
                'StartRecoveryTime': 1500, 'BaseLevel': 1, 'SpellLevel': 1, 'MaxLevel': 80},
        'effects': {
            1: {'Effect': EFFECT_NORMALIZED_WEAPON_DMG, 'BasePoints': 4, 'DieSides': 1, 'RealPointsPerLevel': 1.0},
            2: {'Effect': EFFECT_ENERGIZE, 'BasePoints': 199, 'DieSides': 101, 'MiscValue': FURY,
                'TargetA': TARGET_UNIT_CASTER, 'TargetB': 0},
        },
    },
    {
        'id': 90101, 'copy': 1752, 'name': 'Chaos Strike', 'icon': 'Ability_DemonHunter_ChaosStrike',
        'visual': 'ChaosStrike', 'two_weapons': True,
        'description': 'Slice your target for $s1% weapon damage as Chaos.',
        'set': {'Category': 0, 'AttributesEx': 0, 'PowerType': FURY, 'ManaCost': 400,
                'StartRecoveryTime': 1500, 'BaseLevel': 1, 'SpellLevel': 1, 'SchoolMask': CHAOS},
        'clear': [2],
        'effects': {1: {'Effect': EFFECT_WEAPON_PERCENT_DAMAGE, 'BasePoints': 199, 'DieSides': 1}},
    },
    {
        # The strikes are 90107 (three) and 90108 (the last), cast on a timer by the server's
        # spell_titan_dh_blade_dance; their attack power share is in spell_bonus_data.
        'id': 90102, 'copy': 1680, 'name': 'Blade Dance', 'icon': 'Ability_DemonHunter_BladeDance',
        'two_weapons': True, 'visual': 'BladeDance',
        'description': 'Strike all enemies within 8 yards three times for 24.75% of your attack power as '
                       'Physical damage, then once more for 74.2%, and evade every attack for $d.',
        'tooltip': 'Evading all attacks.',
        'set': {'Category': 0, 'CategoryRecoveryTime': 0, 'RecoveryTime': 10000, 'PowerType': FURY,
                'ManaCost': 350, 'MaxAffectedTargets': 0, 'DurationIndex': DURATION_1S, 'BaseLevel': 10,
                'SpellLevel': 10},
        'clear': [1, 2],
        # Every attack misses (100% more chance to miss it) rather than being dodged: a dodge plays the Dodge
        # animation, which cut the spin short every time an enemy swung.
        'effects': {
            1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_MOD_ATTACKER_MELEE_HIT_CHANCE, 'BasePoints': -101,
                'DieSides': 1, 'TargetA': TARGET_UNIT_CASTER},
            2: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_MOD_ATTACKER_RANGED_HIT_CHANCE, 'BasePoints': -101,
                'DieSides': 1, 'TargetA': TARGET_UNIT_CASTER},
        },
    },
    {
        'id': 90103, 'copy': 1953, 'name': 'Fel Rush', 'icon': 'Ability_DemonHunter_FelRush',
        'two_weapons': True, 'visual': 'FelRush',
        'description': 'Rush forward $a1 yards in a burst of fel energy, dealing $90104s1 Chaos damage to '
                       'enemies in your path.',
        'set': {'Category': 0, 'CategoryRecoveryTime': 0, 'RecoveryTime': 10000, 'ManaCostPercentage': 0,
                'SchoolMask': CHAOS, 'BaseLevel': 1, 'SpellLevel': 1},
        'clear': [2, 3],
        # Blink's leap is now a charge: the Demon Hunter dashes along the ground to the point Blink
        # would land on; 90104 hits the enemies in the way (spell_titan_dh_fel_rush_damage).
        'effects': {
            1: {'Effect': EFFECT_CHARGE_DEST, 'RadiusIndex': RADIUS_15},
            2: {'Effect': EFFECT_TRIGGER_SPELL, 'TargetA': TARGET_UNIT_CASTER, 'TriggerSpell': 90104},
        },
    },
    {
        'id': 90104, 'copy': 5857, 'name': 'Fel Rush', 'icon': 'Ability_DemonHunter_FelRush',
        'description': '',
        'visual': 'FelRushHit',
        'set': {'SchoolMask': CHAOS, 'BaseLevel': 1, 'SpellLevel': 1, 'MaxLevel': 80},
        'effects': {1: {'BasePoints': 9, 'DieSides': 1, 'RealPointsPerLevel': 2.5, 'TargetA': TARGET_SRC_CASTER,
                        'TargetB': TARGET_UNIT_SRC_AREA_ENEMY, 'RadiusIndex': RADIUS_15,
                        'BonusMultiplier': 0.0}},
    },
    {
        'id': 90105, 'copy': 1949, 'name': 'Eye Beam', 'icon': 'Ability_DemonHunter_EyeBeam',
        'two_weapons': True, 'visual': 'EyeBeam',
        'description': 'Blast all enemies in a line in front of you with fel energy for $d, dealing $90106s1 '
                       'Chaos damage every $t1 sec. You can move while channeling, at 5% speed.',
        'tooltip': 'Channeling Eye Beam. Movement speed slowed by 95%.',
        # Hellfire's channel, but cast and kept while moving (turning was already allowed), with the
        # Demon Hunter slowed by 95% (effect 2) for as long as it lasts.
        'set': {'PowerType': FURY, 'ManaCost': 300, 'ManaCostPercentage': 0, 'RecoveryTime': 30000,
                'DurationIndex': DURATION_2S, 'BaseLevel': 20, 'SpellLevel': 20, 'MaxLevel': 0,
                'SchoolMask': CHAOS},
        'unset': {'InterruptFlags': INTERRUPT_MOVEMENT, 'ChannelInterruptFlags': CHANNEL_INTERRUPT_MOVE},
        'clear': [2, 3],
        'effects': {1: {'Amplitude': 200, 'TriggerSpell': 90106},
                    2: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_MOD_DECREASE_SPEED, 'BasePoints': -96,
                        'DieSides': 1, 'TargetA': TARGET_UNIT_CASTER}},
    },
    {
        'id': 90106, 'copy': 5857, 'name': 'Eye Beam', 'icon': 'Ability_DemonHunter_EyeBeam',
        'description': '',
        'set': {'SchoolMask': CHAOS, 'SpellVisual': 0, 'BaseLevel': 20, 'SpellLevel': 20, 'MaxLevel': 80},
        # Everything within 20 yards, cut to the beam's lane by spell_titan_dh_eye_beam_damage.
        'effects': {1: {'BasePoints': 9, 'DieSides': 1, 'RealPointsPerLevel': 3.0, 'TargetA': TARGET_SRC_CASTER,
                        'TargetB': TARGET_UNIT_SRC_AREA_ENEMY, 'RadiusIndex': RADIUS_20, 'BonusMultiplier': 0.0}},
    },
    {
        # Blade Dance: the three quick strikes. 1 damage plus attack power (spell_bonus_data).
        'id': 90107, 'copy': 5857, 'name': 'Blade Dance', 'icon': 'Ability_DemonHunter_BladeDance',
        'visual': 'BladeDanceStrike',
        'description': '',
        'set': {'SchoolMask': PHYSICAL, 'DmgClass': DMG_CLASS_MELEE, 'MaxAffectedTargets': 0,
                'BaseLevel': 10, 'SpellLevel': 10, 'MaxLevel': 80},
        'effects': {1: {'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0, 'TargetA': TARGET_SRC_CASTER,
                        'TargetB': TARGET_UNIT_SRC_AREA_ENEMY, 'RadiusIndex': RADIUS_8, 'BonusMultiplier': 0.0}},
    },
    {
        # Blade Dance: the final strike. 1 damage plus attack power (spell_bonus_data).
        'id': 90108, 'copy': 5857, 'name': 'Blade Dance', 'icon': 'Ability_DemonHunter_BladeDance',
        'visual': 'BladeDanceFinalStrike',
        'description': '',
        'set': {'SchoolMask': PHYSICAL, 'DmgClass': DMG_CLASS_MELEE, 'MaxAffectedTargets': 0,
                'BaseLevel': 10, 'SpellLevel': 10, 'MaxLevel': 80},
        'effects': {1: {'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0, 'TargetA': TARGET_SRC_CASTER,
                        'TargetB': TARGET_UNIT_SRC_AREA_ENEMY, 'RadiusIndex': RADIUS_8, 'BonusMultiplier': 0.0}},
    },
    {
        # Disengage's backward leap (farther), clearing snares; 90110 hits and slows the enemies left behind.
        'id': 90109, 'copy': 781, 'name': 'Vengeful Retreat', 'icon': 'Ability_DemonHunter_VengefulRetreat',
        'two_weapons': True, 'visual': 'VengefulRetreat',
        'description': 'Remove all snares and vault away. Nearby enemies take 40% of your attack power as '
                       'Physical damage and have their movement speed reduced by 70% for 3 sec.',
        # A hunter-family spell (no class flags, so no hunter talent or glyph touches it): the core
        # leaps backward only for hunter spells (Spell::EffectLeapBack), forward for any other.
        'set': {'RecoveryTime': 25000, 'ManaCostPercentage': 0, 'BaseLevel': 14, 'SpellLevel': 14,
                'SpellFamilyName': SPELLFAMILY_HUNTER, 'SpellFamilyFlags0': 0, 'SpellFamilyFlags1': 0,
                'SpellFamilyFlags2': 0},
        'effects': {
            1: {'BasePoints': 79, 'MiscValue': 220},       # leap back: up speed 8, back speed 22
            2: {'Effect': EFFECT_TRIGGER_SPELL, 'BasePoints': 0, 'DieSides': 0, 'TargetA': TARGET_UNIT_CASTER,
                'TriggerSpell': 90110},
            3: {'Effect': EFFECT_DISPEL_MECHANIC, 'BasePoints': 0, 'DieSides': 0, 'TargetA': TARGET_UNIT_CASTER,
                'MiscValue': MECHANIC_SNARE},
        },
    },
    {
        # Vengeful Retreat's hit: everything within 8 yards, 1 damage plus attack power (spell_bonus_data),
        # slowed by 70% for 3 sec.
        'id': 90110, 'copy': 5857, 'name': 'Vengeful Retreat', 'icon': 'Ability_DemonHunter_VengefulRetreat',
        'visual': 'BladeDanceStrike', 'description': '', 'tooltip': 'Movement speed reduced by 70%.',
        'set': {'SchoolMask': PHYSICAL, 'DmgClass': DMG_CLASS_MELEE, 'MaxAffectedTargets': 0,
                'DurationIndex': DURATION_3S, 'BaseLevel': 14, 'SpellLevel': 14, 'MaxLevel': 80},
        'clear': [2, 3],
        'effects': {
            1: {'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0, 'TargetA': TARGET_SRC_CASTER,
                'TargetB': TARGET_UNIT_SRC_AREA_ENEMY, 'RadiusIndex': RADIUS_8, 'BonusMultiplier': 0.0},
            2: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_MOD_DECREASE_SPEED, 'BasePoints': -71, 'DieSides': 1,
                'TargetA': TARGET_SRC_CASTER, 'TargetB': TARGET_UNIT_SRC_AREA_ENEMY, 'RadiusIndex': RADIUS_8},
        },
    },
    {
        # The night elf "Glaive" throw, with Heroic Throw's visual (the main-hand weapon flies),
        # bouncing once (10 yards, spell_jump_distance); 1 damage plus attack power (spell_bonus_data).
        'id': 90111, 'copy': 36500, 'name': 'Throw Glaive', 'icon': 'Ability_DemonHunter_ThrowGlaive',
        'two_weapons': True, 'visual': 'ThrowGlaive',
        'description': 'Throw a demonic glaive at the target for 45% of your attack power as Physical damage. '
                       'The glaive ricochets to 1 additional enemy within 10 yards.',
        'set': {'RecoveryTime': 9000, 'RangeIndex': RANGE_30, 'StartRecoveryCategory': GLOBAL_COOLDOWN,
                'StartRecoveryTime': 1500, 'BaseLevel': 4, 'SpellLevel': 4, 'MaxLevel': 80,
                'SchoolMask': PHYSICAL},
        'clear': [2],
        'effects': {1: {'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0, 'ChainTarget': 2,
                        'BonusMultiplier': 0.0, 'DamageMultiplier': 1.0}},
    },
    {
        # The Burning Crusade demons' Chaos Nova: everything within 8 yards takes 1 damage plus attack
        # power (spell_bonus_data) and is stunned for 2 sec.
        'id': 90112, 'copy': 36225, 'name': 'Chaos Nova', 'icon': 'Spell_Fire_FelFireNova',
        'two_weapons': True, 'visual': 'ChaosNova',
        'description': 'Unleash an eruption of fel energy, dealing 40% of your attack power as Chaos damage '
                       'and stunning all enemies within 8 yards for 2 sec.',
        'tooltip': 'Stunned.',
        'set': {'PowerType': FURY, 'ManaCost': 300, 'RecoveryTime': 60000, 'DurationIndex': DURATION_2S,
                'CastingTimeIndex': CAST_INSTANT, 'StartRecoveryCategory': GLOBAL_COOLDOWN, 'StartRecoveryTime': 1500,
                'SchoolMask': CHAOS, 'DmgClass': DMG_CLASS_MELEE, 'MaxAffectedTargets': 0, 'BaseLevel': 24,
                'SpellLevel': 24, 'MaxLevel': 80},
        'clear': [3],
        'effects': {
            1: {'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0, 'RadiusIndex': RADIUS_8,
                'BonusMultiplier': 0.0},
            2: {'Aura': AURA_STUN, 'BasePoints': 0, 'DieSides': 0, 'RadiusIndex': RADIUS_8},
        },
    },
    {
        # Evasion's dodge, and 35% less damage taken instead of its ranged miss chance.
        'id': 90113, 'copy': 26669, 'name': 'Blur', 'icon': 'Ability_DemonHunter_Blur',
        'two_weapons': True, 'visual': 'Blur',
        'description': 'Increases your chance to dodge by 50% and reduces all damage taken by 35% for $d.',
        'tooltip': 'Dodge chance increased by 50%. Damage taken reduced by 35%.',
        'set': {'Category': 0, 'CategoryRecoveryTime': 0, 'RecoveryTime': 60000, 'DurationIndex': DURATION_10S,
                'PowerType': 0, 'ManaCost': 0, 'BaseLevel': 18, 'SpellLevel': 18},
        'effects': {2: {'Aura': AURA_MOD_DAMAGE_PERCENT_TAKEN, 'BasePoints': -36, 'DieSides': 1,
                        'MiscValue': ALL_SCHOOLS}},
    },
    {
        # Demon Form: the Demon Hunter's demon shape for as long as it lasts (male or female, by the
        # character), hidden from the buff bar. Eye Beam holds it for its channel, Chaos Nova for a moment
        # (spell_titan_dh_demon_form and the scripts of both).
        'id': 90115, 'copy': 26669, 'name': 'Demon Form', 'icon': 'Spell_Shadow_DemonForm',
        'description': '', 'tooltip': '',
        'set': {'Attributes': ATTR0_DO_NOT_DISPLAY | ATTR0_DO_NOT_LOG, 'Category': 0, 'CategoryRecoveryTime': 0,
                'RecoveryTime': 0, 'StartRecoveryCategory': 0, 'StartRecoveryTime': 0, 'PowerType': 0,
                'ManaCost': 0, 'DurationIndex': DURATION_1S, 'SpellVisual': 0, 'BaseLevel': 1, 'SpellLevel': 1},
        'clear': [2],
        'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_DUMMY, 'BasePoints': 0, 'DieSides': 0,
                        'TargetA': TARGET_UNIT_CASTER}},
    },
    {
        # The warlock's Metamorphosis, without the shapeshift: the demon shape (effect 1, a dummy aura the
        # server's spell_titan_dh_demon_form turns into the model), 25% haste (effect 2, instead of its
        # armor) and its 20% damage (effect 3).
        'id': 90116, 'copy': 47241, 'name': 'Metamorphosis', 'icon': 'Ability_DemonHunter_Metamorphosis',
        'visual': 'Metamorphosis',
        'description': 'Transform into a demon for $d, increasing your attack speed by $s2% and all damage '
                       'you deal by $s3%.',
        'tooltip': 'Demon form. Attack speed increased by $s2%. Damage dealt increased by $s3%.',
        'set': {'Category': 0, 'CategoryRecoveryTime': 0, 'RecoveryTime': 180000, 'PowerType': 0, 'ManaCost': 0,
                'ManaCostPercentage': 0, 'BaseLevel': 30, 'SpellLevel': 30, 'MaxLevel': 0},
        'effects': {
            1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_DUMMY, 'BasePoints': 0, 'DieSides': 0, 'MiscValue': 0},
            2: {'Aura': AURA_MOD_MELEE_RANGED_HASTE, 'BasePoints': 24, 'DieSides': 1, 'MiscValue': 0},
            3: {'Aura': AURA_MOD_DAMAGE_PERCENT_DONE, 'BasePoints': 19, 'DieSides': 1},
        },
    },
    {
        # Intercept's charge: the hit (effect 2) and the Fury (effect 3) land as the Demon Hunter arrives (a
        # charge of no class family travels at charge speed, Spell::EffectCharge). 1 damage plus attack power
        # (spell_bonus_data).
        'id': 90117, 'copy': 20252, 'name': 'Felblade', 'icon': 'Ability_DemonHunter_Felblade',
        'two_weapons': True, 'visual': 'Felblade',
        'description': 'Charge to your target and deal 50% of your attack power as Fire damage. Generates 40 Fury.',
        'set': {'RecoveryTime': 15000, 'RangeIndex': RANGE_15, 'PowerType': 0, 'ManaCost': 0,
                'ManaCostPercentage': 0, 'StartRecoveryCategory': GLOBAL_COOLDOWN, 'StartRecoveryTime': 1500,
                'SchoolMask': FIRE, 'DmgClass': DMG_CLASS_MELEE, 'BaseLevel': 16, 'SpellLevel': 16, 'MaxLevel': 80},
        'clear': [2, 3],
        'effects': {
            2: {'Effect': EFFECT_SCHOOL_DAMAGE, 'BasePoints': 0, 'DieSides': 1, 'TargetA': TARGET_UNIT_TARGET_ENEMY,
                'BonusMultiplier': 0.0},
            3: {'Effect': EFFECT_ENERGIZE, 'BasePoints': 399, 'DieSides': 1, 'TargetA': TARGET_UNIT_CASTER,
                'MiscValue': FURY},
        },
    },
    {
        # Consecration's persistent area, on allies: a shield of no size (-1) that spell_titan_dh_darkness turns
        # into a 20% chance to take nothing from each hit.
        'id': 90118, 'copy': 26573, 'name': 'Darkness', 'icon': 'Ability_DemonHunter_Darkness',
        'two_weapons': True, 'visual': 'Darkness',
        'description': 'Summon darkness around you in an 8 yard radius for $d. Allies in it have a 20% chance to '
                       'avoid all damage from an attack.',
        'tooltip': '20% chance to avoid all damage from an attack.',
        'set': {'RecoveryTime': 180000, 'DurationIndex': DURATION_8S, 'PowerType': 0, 'ManaCost': 0,
                'ManaCostPercentage': 0, 'StartRecoveryCategory': GLOBAL_COOLDOWN, 'StartRecoveryTime': 1500,
                'SchoolMask': SHADOW, 'DmgClass': 0, 'BaseLevel': 26, 'SpellLevel': 26, 'MaxLevel': 0},
        'effects': {1: {'Effect': EFFECT_PERSISTENT_AREA_AURA, 'Aura': AURA_SCHOOL_ABSORB, 'BasePoints': -2,
                        'DieSides': 1, 'TargetA': TARGET_DEST_CASTER, 'TargetB': TARGET_UNIT_DEST_AREA_ALLY,
                        'RadiusIndex': RADIUS_8, 'Amplitude': 0, 'MiscValue': ALL_SCHOOLS}},
    },
    {
        # The Metamorphosis warlock's: a pulse (90120) every second and one at once (effect 3, instead of its
        # immunity), and 20 Fury (effect 2).
        'id': 90119, 'copy': 50589, 'name': 'Immolation Aura', 'icon': 'Spell_Fire_Incinerate',
        'two_weapons': True, 'visual': 'ImmolationAura',
        'description': 'Engulf yourself in flames for $d, dealing 20% of your attack power as Fire damage to '
                       'enemies within 8 yards at once and every sec. Generates 20 Fury.',
        'tooltip': 'Burning nearby enemies every sec.',
        'set': {'RecoveryTime': 15000, 'DurationIndex': DURATION_6S, 'PowerType': 0, 'ManaCost': 0,
                'ManaCostPercentage': 0, 'StartRecoveryCategory': GLOBAL_COOLDOWN, 'StartRecoveryTime': 1500,
                'BaseLevel': 1, 'SpellLevel': 1, 'MaxLevel': 0},
        'clear': [2, 3],
        'effects': {
            1: {'TriggerSpell': 90120},
            2: {'Effect': EFFECT_ENERGIZE, 'BasePoints': 199, 'DieSides': 1, 'TargetA': TARGET_UNIT_CASTER,
                'MiscValue': FURY},
            3: {'Effect': EFFECT_TRIGGER_SPELL, 'TargetA': TARGET_UNIT_CASTER, 'TriggerSpell': 90120},
        },
    },
    {
        # Immolation Aura's pulse: everything within 8 yards, 1 damage plus attack power (spell_bonus_data).
        'id': 90120, 'copy': 50590, 'name': 'Immolation Aura', 'icon': 'Spell_Fire_Incinerate',
        'visual': 'ImmolationAuraPulse', 'description': '',
        'set': {'SchoolMask': FIRE, 'MaxAffectedTargets': 0, 'BaseLevel': 1, 'SpellLevel': 1, 'MaxLevel': 80},
        'effects': {1: {'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0, 'TargetA': TARGET_DEST_CASTER,
                        'TargetB': TARGET_UNIT_DEST_AREA_ENEMY, 'RadiusIndex': RADIUS_8, 'BonusMultiplier': 0.0}},
    },
    {
        # Havoc's capstone (Dragonflight's): Immolation Aura's pulsing on the Demon Hunter, a pulse (90122, a fel ball
        # at every enemy in range) every 0.25 sec for 3 sec, each draining 5 Fury; it ends when the Fury runs out
        # (spell_titan_dh_fel_barrage).
        'id': 90121, 'copy': 50589, 'name': 'Fel Barrage', 'icon': 'INV_FelBarrage',
        'two_weapons': True, 'visual': 'FelBarrage',
        'description': 'Unleash a torrent of Fel energy over $d, hurling a fel ball at every enemy within 8 '
                       'yards every 0.25 sec, each exploding for 15% of your attack power as Chaos damage. Each '
                       'burst drains 5 Fury; it ends when your Fury runs out.',
        'tooltip': 'Unleashing Fel energy on nearby enemies.',
        'set': {'RecoveryTime': 90000, 'DurationIndex': DURATION_3S, 'PowerType': FURY, 'ManaCost': 100,
                'ManaCostPercentage': 0, 'StartRecoveryCategory': GLOBAL_COOLDOWN, 'StartRecoveryTime': 1500,
                'SchoolMask': CHAOS, 'BaseLevel': 30, 'SpellLevel': 30, 'MaxLevel': 0},
        'clear': [2, 3],
        'effects': {1: {'Amplitude': 250, 'TriggerSpell': 90122}},
    },
    {
        # Fel Barrage's pulse: finds every enemy within 8 yards; spell_titan_dh_fel_barrage_pulse throws a
        # ball (90123) at each.
        'id': 90122, 'copy': 50590, 'name': 'Fel Barrage', 'icon': 'INV_FelBarrage',
        'visual': 'FelBarragePulse', 'description': '',
        'set': {'SchoolMask': CHAOS, 'MaxAffectedTargets': 0, 'BaseLevel': 30, 'SpellLevel': 30, 'MaxLevel': 80},
        'effects': {1: {'Effect': EFFECT_DUMMY, 'BasePoints': 0, 'DieSides': 0, 'RealPointsPerLevel': 0.0,
                        'TargetA': TARGET_DEST_CASTER, 'TargetB': TARGET_UNIT_DEST_AREA_ENEMY,
                        'RadiusIndex': RADIUS_8, 'BonusMultiplier': 0.0}},
    },
    {
        # Fel Barrage's ball: flies to its enemy (28 yards a second) and explodes, 1 damage plus attack power
        # (spell_bonus_data).
        'id': 90123, 'copy': 50590, 'name': 'Fel Barrage', 'icon': 'INV_FelBarrage',
        'visual': 'FelBarrageBolt', 'description': '',
        'set': {'SchoolMask': CHAOS, 'Speed': 28.0, 'RangeIndex': RANGE_20, 'MaxAffectedTargets': 0,
                'BaseLevel': 30, 'SpellLevel': 30, 'MaxLevel': 80},
        'effects': {1: {'BasePoints': 0, 'DieSides': 1, 'RealPointsPerLevel': 0.0,
                        'TargetA': TARGET_UNIT_TARGET_ENEMY, 'TargetB': 0, 'RadiusIndex': 0,
                        'BonusMultiplier': 0.0}},
    },
    {
        # Counterspell: interrupts and locks the school out for 3 sec; also removes one magic buff
        # from the target and gives 20 Fury.
        'id': 90114, 'copy': 2139, 'name': 'Consume Magic', 'icon': 'Ability_DemonHunter_ConsumeMagic',
        'two_weapons': True, 'visual': 'ConsumeMagic',
        'description': "Interrupt the target's spellcasting, preventing any spell in that school from being "
                       'cast for $d, and consume 1 beneficial Magic effect from it. Generates 20 Fury.',
        'set': {'Category': 0, 'CategoryRecoveryTime': 0, 'RecoveryTime': 15000, 'RangeIndex': RANGE_20,
                'DurationIndex': DURATION_3S, 'ManaCostPercentage': 0, 'SchoolMask': CHAOS, 'BaseLevel': 12,
                'SpellLevel': 12},
        'effects': {
            2: {'Effect': EFFECT_DISPEL, 'BasePoints': 0, 'DieSides': 1, 'TargetA': TARGET_UNIT_TARGET_ENEMY,
                'MiscValue': DISPEL_MAGIC},
            3: {'Effect': EFFECT_ENERGIZE, 'BasePoints': 199, 'DieSides': 1, 'TargetA': TARGET_UNIT_CASTER,
                'MiscValue': FURY},
        },
    },
]


class Dbc:
    def __init__(self, path, fields):
        data = Path(path).read_bytes()
        magic, count, n_fields, record_size, string_size = struct.unpack_from('<4s4I', data)
        if magic != b'WDBC' or n_fields != fields or record_size != fields * 4:
            raise ValueError(f'{path}: unexpected layout ({n_fields} fields / {record_size} bytes). '
                             'Is this a 3.3.5a file?')
        self.fields = fields
        self.rows = {}
        for i in range(count):
            row = struct.unpack_from(f'<{fields}I', data, 20 + i * record_size)
            self.rows[row[0]] = row
        start = 20 + count * record_size
        self.strings = bytearray(data[start:start + string_size])

    def string(self, offset):
        return self.strings[offset:self.strings.index(b'\0', offset)].decode('utf-8')

    def add_string(self, text):
        if not text:
            return 0
        encoded = text.encode('utf-8') + b'\0'
        found = self.strings.find(b'\0' + encoded)
        if found != -1:
            return found + 1
        offset = len(self.strings)
        self.strings += encoded
        return offset

    def save(self, path):
        body = b''.join(struct.pack(f'<{self.fields}I', *self.rows[key]) for key in sorted(self.rows))
        header = struct.pack('<4s4I', b'WDBC', len(self.rows), self.fields, self.fields * 4, len(self.strings))
        Path(path).write_bytes(header + body + self.strings)


def raw(value):
    """A column value as the DBC stores it (floats by their bits, negatives as two's complement)."""
    if isinstance(value, float):
        return struct.unpack('<I', struct.pack('<f', value))[0]
    return value & 0xFFFFFFFF


def icon_ids(icons):
    """SpellIcon.dbc id for every icon: the client's own if it has one, else a new row."""
    ids, next_id = {}, FIRST_ICON_ID
    for path, icon_id in ((icons.string(row[1]).lower(), key) for key, row in icons.rows.items()):
        ids.setdefault(path, icon_id)
    result = {}
    for name in ICONS:
        path = f'interface\\icons\\{name}'.lower()
        if path not in ids:
            icons.rows[next_id] = (next_id, icons.add_string(f'Interface\\Icons\\{name}'))
            ids[path] = next_id
            next_id += 1
        result[name] = ids[path]
    return result


def build_sounds(source, out):
    """SOUNDS' entries, copies of Blizzard's with our files."""
    sounds = Dbc(source / 'SoundEntries.dbc', SOUND_FIELDS)
    for key, spec in SOUNDS.items():
        if len(spec['files']) > 10:
            raise ValueError(f'{key}: a sound entry holds at most 10 files')
        row = list(sounds.rows[spec['copy']])
        row[0] = spec['id']
        row[SOUND_NAME] = sounds.add_string(f'Emberforge {key}')
        for n in range(10):
            has = n < len(spec['files'])
            row[SOUND_FILES + n] = sounds.add_string(f'{spec["files"][n]}.wav') if has else 0
            row[SOUND_WEIGHTS + n] = 1 if has else 0
        row[SOUND_FOLDER] = sounds.add_string(SOUNDS_DIR)
        sounds.rows[spec['id']] = tuple(row)
    sounds.save(out / 'SoundEntries.dbc')


def build_visuals(source, out):
    """The trails' effect names, and VISUALS' kits and visuals."""
    visuals = Dbc(source / 'SpellVisual.dbc', VISUAL_FIELDS)
    kits = Dbc(source / 'SpellVisualKit.dbc', KIT_FIELDS)
    names = Dbc(source / 'SpellVisualEffectName.dbc', EFFECT_NAME_FIELDS)
    for key, effect in EFFECTS.items():
        model = f'{EFFECTS_DIR}\\{effect["model"]}.mdx'
        names.rows[effect['effect']] = (effect['effect'], names.add_string(f'Emberforge {key}'),
                                       names.add_string(model), raw(0.0), raw(1.0), raw(0.01), raw(100.0))
    for spec in VISUALS.values():
        visual = list(visuals.rows[spec['copy']])
        visual[0] = spec['id']
        for kind in spec.get('clear', ()):
            visual[VISUAL_KITS[kind]] = 0
        for column, value in spec.get('set', {}).items():
            visual[column] = EFFECTS[value]['effect'] if isinstance(value, str) else value
        for kind, (kit_id, slots) in spec['kits'].items():
            kit = list(kits.rows[visual[VISUAL_KITS[kind]] or BLANK_KIT])
            kit[0] = kit_id
            for where, effect in slots.items():
                if where == 'anim':
                    kit[KIT_ANIM] = effect
                    continue
                if where == 'procs':
                    kit[KIT_PROCS.start:KIT_PROCS.stop] = [0xFFFFFFFF] * len(KIT_PROCS)
                    kit[KIT_PROC_PARAMS.start:KIT_PROC_PARAMS.stop] = [0] * len(KIT_PROC_PARAMS)
                    continue
                if where == 'sound':
                    kit[KIT_SOUND] = SOUNDS[effect]['id'] if effect else 0
                    continue
                for slot in where if isinstance(where, tuple) else (where,):
                    kit[slot] = EFFECTS[effect]['effect'] if isinstance(effect, str) else effect
            kits.rows[kit_id] = tuple(kit)
            visual[VISUAL_KITS[kind]] = kit_id
        visuals.rows[spec['id']] = tuple(visual)
    visuals.save(out / 'SpellVisual.dbc')
    kits.save(out / 'SpellVisualKit.dbc')
    names.save(out / 'SpellVisualEffectName.dbc')


def build_animations(source, out):
    """AnimationData rows of the Demon Hunter's animation slots (ANIMATION_ROWS)."""
    animations = Dbc(source / 'AnimationData.dbc', ANIMATION_FIELDS)
    for slot, like, body in ANIMATION_ROWS:
        row = list(animations.rows[slot])
        row[ANIM_WEAPON_FLAGS:ANIM_FALLBACK] = animations.rows[like][ANIM_WEAPON_FLAGS:ANIM_FALLBACK]
        if body is not None:
            row[ANIM_BODY_FLAGS] = body
        row[ANIM_FALLBACK] = 0 if slot == ANIM_FEL_RUSH else like
        animations.rows[slot] = tuple(row)
    animations.save(out / 'AnimationData.dbc')


def build_spells(spells, icon_id):
    for spell in SPELLS:
        row = list(spells.rows[spell['copy']])
        row[0] = spell['id']
        for n in spell.get('clear', []):
            for name, (first, is_float) in EFFECT_FIELDS.items():
                row[first + n - 1] = raw(1.0 if name == 'DamageMultiplier' else 0.0) if is_float else 0
            for word in range(3):
                row[CLASS_MASK + (n - 1) * 3 + word] = 0
        for name, value in spell.get('set', {}).items():
            row[FIELDS[name]] = raw(value)
        for name, bits in spell.get('unset', {}).items():
            row[FIELDS[name]] &= ~bits
        for n, changes in spell.get('effects', {}).items():
            for name, value in changes.items():
                first, is_float = EFFECT_FIELDS[name]
                row[first + n - 1] = raw(float(value) if is_float else value)
        # Our own spells: no class family (no class talent or glyph touches them), one rank.
        if 'SpellFamilyName' not in spell.get('set', {}):
            for name in ('SpellFamilyName', 'SpellFamilyFlags0', 'SpellFamilyFlags1', 'SpellFamilyFlags2'):
                row[FIELDS[name]] = 0
        if spell.get('two_weapons'):
            row[FIELDS['EquippedItemClass']] = ITEM_CLASS_WEAPON
            row[FIELDS['EquippedItemSubClassMask']] = ONE_HANDED_WEAPONS
            row[FIELDS['EquippedItemInventoryTypeMask']] = 0
            row[FIELDS['AttributesEx3']] |= SPELL_ATTR3_REQUIRES_MAIN_HAND_WEAPON
        if 'visual' in spell:
            row[FIELDS['SpellVisual']] = VISUALS[spell['visual']]['id']
        # No stance or form needed (or forbidden): Blade Dance's Whirlwind wanted Berserker Stance.
        for column in range(FIELDS['Stances'], FIELDS['StancesNot'] + 2):
            row[column] = 0
        row[FIELDS['SpellIconID']] = icon_id[spell['icon']]
        row[FIELDS['ActiveIconID']] = 0
        for base, text in ((NAME, spell['name']), (RANK, ''), (DESCRIPTION, spell.get('description', '')),
                           (TOOLTIP, spell.get('tooltip', ''))):
            row[base] = spells.add_string(text)     # enUS; no text in the other locales
            for locale in range(1, 16):
                row[base + locale] = 0
        spells.rows[spell['id']] = tuple(row)


def rename_runic_power(spells):
    """Runic Power is Fury on Emberforge: rename it in every spell's name, rank, description and tooltip."""
    pattern = re.compile(r'runic power', re.I)
    renamed = 0
    for key, row in spells.rows.items():
        row = list(row)
        for base in (NAME, RANK, DESCRIPTION, TOOLTIP):
            if row[base] and pattern.search(spells.string(row[base])):
                row[base] = spells.add_string(pattern.sub('Fury', spells.string(row[base])))
                renamed += 1
        spells.rows[key] = tuple(row)
    return renamed


def sql_columns():
    """spell_dbc's columns and their types, from the base schema."""
    schema = (HERE.parents[2] / 'data' / 'sql' / 'base' / 'db_world' / 'spell_dbc.sql').read_text()
    table = schema[schema.index('CREATE TABLE `spell_dbc`'):]
    return re.findall(r'^\s+`(\w+)` (\w+)( unsigned)?', table[:table.index('PRIMARY KEY')], re.M)


def write_sql(spells, path):
    columns = sql_columns()
    if len(columns) != SPELL_FIELDS:
        raise ValueError(f'spell_dbc has {len(columns)} columns, expected {SPELL_FIELDS}')
    ids = ', '.join(str(spell['id']) for spell in SPELLS)
    lines = ['-- Emberforge spells (Demon Hunter): written by client-patch/patch_spells.py --sql, which also',
             '-- puts the same rows in the client patch. Edit the definitions there, not this file.',
             f'DELETE FROM `spell_dbc` WHERE `ID` IN ({ids});',
             'INSERT INTO `spell_dbc` VALUES']
    values = []
    for spell in SPELLS:
        row = spells.rows[spell['id']]
        out = []
        for column, ((_, kind, unsigned), value) in enumerate(zip(columns, row)):
            if column in STRING_COLUMNS:
                out.append("'" + spells.string(value).replace('\\', '\\\\').replace("'", "''") + "'"
                           if value else "''")
            elif kind == 'float':
                out.append(repr(round(struct.unpack('<f', struct.pack('<I', value))[0], 6)))
            elif unsigned or value < 0x80000000:
                out.append(str(value))
            else:
                out.append(str(value - 0x100000000))
        values.append('(' + ','.join(out) + ')')
    lines.append(',\n'.join(values) + ';')
    Path(path).write_text('\n'.join(lines) + '\n')


def main():
    args = sys.argv[1:]
    sql = None
    if '--sql' in args:
        sql = args[args.index('--sql') + 1]
        del args[args.index('--sql'):args.index('--sql') + 2]
    if len(args) != 2:
        sys.exit(__doc__)
    source, out = Path(args[0]), Path(args[1])
    out.mkdir(parents=True, exist_ok=True)

    icons = Dbc(source / 'SpellIcon.dbc', ICON_FIELDS)
    icon_id = icon_ids(icons)
    icons.save(out / 'SpellIcon.dbc')

    build_sounds(source, out)
    build_visuals(source, out)
    build_animations(source, out)
    spells = Dbc(source / 'Spell.dbc', SPELL_FIELDS)
    build_spells(spells, icon_id)
    renamed = rename_runic_power(spells)
    spells.save(out / 'Spell.dbc')
    print(f'Wrote {out / "Spell.dbc"} ({len(SPELLS)} spells, Runic Power renamed in {renamed} texts) '
          f'and {out / "SpellIcon.dbc"}')
    if sql:
        write_sql(spells, sql)
        print(f'Wrote {sql}')


if __name__ == '__main__':
    main()
