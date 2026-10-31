#!/usr/bin/env python3
"""The Monk's spells: data for patch_spells.py, which builds and writes them with the Demon Hunter's.

Mistweaver first (the Monk's healer, on mana): each spell a copy of a Blizzard healing spell, with retail's
icons and its own look (EFFECTS, VISUALS): mists (green, Enveloping Mist's yellow), Soothing Mist's beam, Life
Cocoon's cocoon and Revival's burst. Essence Font, Thunder Focus Tea and Detox keep the copied spell's look
(Tranquility, Nature's Swiftness, Cleanse) until theirs are in. The server's
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
    90311: 0x800,   # Invoke Chi-Ji, the Red Crane
    90312: 0x1000,  # Crane's Gust (Chi-Ji's heal)
    90313: 0x2000,  # Refreshing Jade Wind
    90314: 0x4000,  # Refreshing Jade Wind's heal
    90320: 0x8000,      # Tiger Palm
    90321: 0x10000,     # Blackout Kick
    90322: 0x20000,     # Rising Sun Kick
    90323: 0x40000,     # Fists of Fury
    90324: 0x80000,     # Fists of Fury's strikes
    90325: 0x100000,    # Spinning Crane Kick
    90326: 0x200000,    # Spinning Crane Kick's strikes
    90327: 0x400000,    # Whirling Dragon Punch
    90328: 0x800000,    # Whirling Dragon Punch's strikes
}
VIVIFY, ENVELOPING_MIST = 0x2, 0x8

# Spell.dbc values used below (as in patch_spells.py).
EFFECT_HEAL, EFFECT_APPLY_AURA = 10, 6
AURA_PERIODIC_HEAL, AURA_SCHOOL_ABSORB, AURA_ADD_PCT_MODIFIER = 8, 69, 108
SPELLMOD_CASTING_TIME, SPELLMOD_COST = 10, 14
TARGET_UNIT_CASTER, TARGET_UNIT_TARGET_ALLY, TARGET_UNIT_CASTER_AREA_RAID = 1, 21, 56
CAST_INSTANT, CAST_1_5S, CAST_2S = 1, 16, 5
DURATION_3S, DURATION_6S, DURATION_8S, DURATION_12S, DURATION_20S, DURATION_30S = 27, 32, 31, 29, 18, 9
DURATION_25S = 63
RANGE_40, RANGE_100 = 5, 6
EFFECT_SUMMON, SUMMON_GUARDIAN = 28, 1161      # Feral Spirit's summon properties: a guardian at the caster's side
NPC_CHI_JI = 90311
AURA_PERIODIC_TRIGGER_SPELL = 23
TARGET_SRC_CASTER, TARGET_UNIT_SRC_AREA_ALLY = 22, 30
RANGE_SELF, RADIUS_10 = 1, 13

# Windwalker: Energy and combo points. Tiger Palm builds them; the finishers (spell_titan_monk_finisher, src/monk.cpp)
# take them all, from 1 to 5, and hit as hard as the points spent: their values here are the 5-point ones.
ENERGY, PHYSICAL, DMG_CLASS_MELEE = 3, 0x1, 2
EFFECT_WEAPON_PERCENT_DAMAGE, EFFECT_ADD_COMBO_POINTS, EFFECT_NORMALIZED_WEAPON_DMG = 31, 80, 121
AURA_MOD_HEALING_PCT = 118
TARGET_UNIT_TARGET_ENEMY, TARGET_UNIT_SRC_AREA_ENEMY, TARGET_UNIT_CONE_ENEMY = 6, 15, 24
RADIUS_8 = 14
DURATION_1S, DURATION_1_5S, DURATION_4S, DURATION_10S = 36, 65, 35, 1
ANIM_ATTACK_UNARMED, ANIM_KICK = 16, 95
WHIRLWIND_VISUAL = 11756        # Whirlwind's (50622) hit visual
GLOBAL_COOLDOWN = 133
NATURE, ALL_SCHOOLS = 0x8, 0x7F

# Visuals (as in patch_spells.py, which builds them with the Demon Hunter's).
KIT_HEAD, KIT_CHEST, KIT_BASE, KIT_LEFT_HAND, KIT_RIGHT_HAND, KIT_WORLD = 3, 4, 5, 6, 7, 14
HANDS = (KIT_LEFT_HAND, KIT_RIGHT_HAND)
ANIM_SPELL_CAST_OMNI = 54
NO_ANIM = 0xFFFFFFFF
VISUAL_MISSILE_MODEL, VISUAL_MISSILE_SOUND, VISUAL_MISSILE_MOTION = 8, 11, 21
VISUAL_CHANNEL = 6              # SpellVisual column: the channel kit
BLEND_ADD = 4
JADE = (0.45, 1.0, 0.8)
GREEN = (0.35, 1.0, 0.45)
GOLD = (1.0, 0.82, 0.3)
CRANE_RED, CRANE_GOLD = (1.0, 0.32, 0.18), (1.0, 0.68, 0.3)
HEALING_BEAM_KIT = 9818         # the Healing Beam's channel kit (48775: channeling, a beam from hands to target)
GREEN_BEAM = 269                # SpellChainEffects: Green Beam (33346), a smooth wavy beam, white (its texture's color)

# Our beams (see patch_spells.CHAIN_EFFECTS): Soothing Mist's, the Green Beam twice as wide, its texture jade puffs and
# wisps along it (spells/MONK_SoothingMist_Beam.png, tiling end to end, fading to its edges).
CHAIN_EFFECTS = {
    'SoothingMist': {'id': 1200, 'copy': GREEN_BEAM, 'texture': 'MONK_SoothingMist_Beam', 'set': {'Width': 0.5}},
}
# Our textures (spells/): white, tinted per spell. Puffs, Wisps and Clouds: four variants each (2x2), a puff each;
# Anim: a 4x4 flipbook of a puff billowing out and fading; Glow: a soft round light; Flare: a small star (motes).
GLOW, FLARE = 'MONK_Glow', 'MONK_Flare'
PUFFS, WISPS, CLOUDS, ANIM = 'MONK_Mist_Puffs', 'MONK_Mist_Wisps', 'MONK_Mist_Clouds', 'MONK_Mist_Anim'
MOTE = (0.85, 1.0, 0.9)
CRANE_MOTE = (1.0, 0.85, 0.6)


def ring(count, out, up, across, step=0.0):
    """count puffs out yards from the middle, up yards high (each step higher than the last), across yards wide."""
    return tuple((out, up + i * step, across) for i in range(count))


# Our effect models (see patch_spells.EFFECTS): no Blizzard druid or priest look on the Monk.
EFFECTS = {
    # The jade on the Monk's hands while casting: a soft light, wisps circling it, smoke billowing up off it and
    # motes of light rising.
    'JadePrecastHand': {'model': 'MONK_JadePrecastHand', 'effect': 7122, 'shape': 'mist',
                        'textures': (GLOW, WISPS, ANIM, FLARE),
                        'options': {'layers': (
                            {'texture': 0, 'tint': JADE, 'alpha': 0.45, 'pulse': 0.8, 'cards': ring(1, 0.0, 0.0, 0.5)},
                            {'texture': 1, 'sheet': (2, 2), 'tint': JADE, 'alpha': 0.8, 'turn': 0.9, 'pulse': 0.9,
                             'cards': ring(3, 0.1, 0.0, 0.45)},
                            {'texture': 2, 'sheet': (4, 4), 'flip': 1.0, 'tint': JADE, 'alpha': 0.6, 'turn': -1.4,
                             'pulse': 0, 'rise': (0.25, 1.0), 'cards': ring(2, 0.06, 0.05, 0.45)},
                            {'texture': 3, 'tint': MOTE, 'alpha': 0.9, 'turn': 1.6, 'pulse': 0.5,
                             'cards': ring(4, 0.2, -0.05, 0.12, 0.04)})}},
    # The jade let go from the hands as the spell goes off: a flash, a puff of smoke and motes, gone in 0.7 sec.
    'JadeCastHand': {'model': 'MONK_JadeCastHand', 'effect': 7123, 'shape': 'mist',
                     'textures': (GLOW, ANIM, WISPS, FLARE),
                     'options': {'duration': 700, 'fade': (0, 400), 'layers': (
                         {'texture': 0, 'tint': JADE, 'alpha': 0.6, 'pulse': 0, 'cards': ring(1, 0.0, 0.0, 0.8)},
                         {'texture': 1, 'sheet': (4, 4), 'flip': 0.7, 'tint': JADE, 'alpha': 0.8, 'pulse': 0,
                          'rise': (0.4, 0.7), 'cards': ring(2, 0.08, 0.05, 0.8)},
                         {'texture': 2, 'sheet': (2, 2), 'tint': JADE, 'alpha': 0.7, 'turn': 0.8, 'pulse': 0,
                          'cards': ring(2, 0.15, 0.0, 0.6)},
                         {'texture': 3, 'tint': MOTE, 'alpha': 1.0, 'turn': 1.2, 'pulse': 0, 'rise': (0.5, 0.7),
                          'cards': ring(5, 0.25, 0.0, 0.14, 0.05)})}},
    # A jade heal on the healed (Renewing Mist's, Essence Font's, Revival's, Detox's...): a light at the chest, smoke
    # billowing up around the body with wisps, motes of light rising past the head, gone in 1.4 sec.
    'JadeHeal': {'model': 'MONK_JadeHeal', 'effect': 7124, 'shape': 'mist', 'textures': (GLOW, ANIM, WISPS, FLARE),
                 'options': {'duration': 1400, 'fade': (100, 500), 'layers': (
                     {'texture': 0, 'tint': JADE, 'alpha': 0.3, 'pulse': 0, 'cards': ring(1, 0.0, 1.0, 1.6)},
                     {'texture': 1, 'sheet': (4, 4), 'flip': 1.4, 'tint': JADE, 'alpha': 0.75, 'turn': 3.0,
                      'pulse': 0, 'rise': (1.0, 1.4), 'cards': ring(4, 0.45, 0.3, 1.3, 0.25)},
                     {'texture': 2, 'sheet': (2, 2), 'tint': JADE, 'alpha': 0.6, 'turn': -2.0, 'pulse': 0,
                      'rise': (1.2, 1.4), 'cards': ring(3, 0.6, 0.4, 1.0, 0.3)},
                     {'texture': 3, 'tint': MOTE, 'alpha': 1.0, 'turn': 2.0, 'pulse': 0.35, 'rise': (1.6, 1.4),
                      'cards': ring(6, 0.7, 0.2, 0.18, 0.12)})}},
    # The mists on the healed, for as long as the spell lasts: puffs circling the body, facing the camera, in layers
    # (each puff its own variant, fading in and out a beat behind the last), a soft light and twinkling motes.
    # Soothing Mist's, jade wisps over soft puffs where its beam flows in.
    'SoothingMist': {'model': 'MONK_SoothingMist', 'effect': 7116, 'shape': 'mist',
                     'textures': (PUFFS, WISPS, GLOW, FLARE),
                     'options': {'layers': (
                         {'texture': 2, 'tint': JADE, 'alpha': 0.35, 'pulse': 1.2, 'cards': ring(1, 0.0, 1.1, 2.0)},
                         {'texture': 0, 'sheet': (2, 2), 'tint': JADE, 'alpha': 0.4, 'turn': 6.0,
                          'cards': ring(3, 0.4, 0.8, 1.3, 0.25)},
                         {'texture': 1, 'sheet': (2, 2), 'tint': JADE, 'alpha': 0.65, 'turn': -4.0, 'pulse': 1.2,
                          'cards': ring(3, 0.5, 0.7, 1.1, 0.35)},
                         {'texture': 3, 'tint': MOTE, 'alpha': 0.9, 'turn': 3.0, 'pulse': 0.6,
                          'cards': ring(5, 0.6, 0.4, 0.15, 0.3)})}},
    # Renewing Mist's, green puffs and wisps around the body with green motes.
    'RenewingMist': {'model': 'MONK_RenewingMist', 'effect': 7119, 'shape': 'mist',
                     'textures': (PUFFS, WISPS, FLARE),
                     'options': {'layers': (
                         {'texture': 0, 'sheet': (2, 2), 'tint': GREEN, 'alpha': 0.4, 'turn': 5.0, 'pulse': 2.0,
                          'cards': ring(4, 0.6, 0.5, 1.3, 0.45)},
                         {'texture': 1, 'sheet': (2, 2), 'tint': GREEN, 'alpha': 0.55, 'turn': -7.0, 'pulse': 1.4,
                          'cards': ring(3, 0.7, 0.7, 1.1, 0.55)},
                         {'texture': 2, 'tint': (0.7, 1.0, 0.75), 'alpha': 0.9, 'turn': 3.5, 'pulse': 0.7,
                          'cards': ring(6, 0.75, 0.3, 0.14, 0.3)})}},
    # Enveloping Mist's, thick gold clouds wrapping the body with gold wisps through them, a warm light and motes.
    'EnvelopingMist': {'model': 'MONK_EnvelopingMist', 'effect': 7120, 'shape': 'mist',
                       'textures': (CLOUDS, WISPS, GLOW, FLARE),
                       'options': {'layers': (
                           {'texture': 2, 'tint': GOLD, 'alpha': 0.3, 'pulse': 2.5, 'cards': ring(1, 0.0, 1.1, 2.4)},
                           {'texture': 0, 'sheet': (2, 2), 'tint': GOLD, 'alpha': 0.9, 'turn': -4.0, 'pulse': 2.4,
                            'cards': ring(4, 0.5, 0.4, 1.7, 0.5)},
                           {'texture': 1, 'sheet': (2, 2), 'tint': GOLD, 'alpha': 0.6, 'turn': 5.0, 'pulse': 1.6,
                            'cards': ring(3, 0.6, 0.7, 1.2, 0.55)},
                           {'texture': 3, 'tint': (1.0, 0.95, 0.7), 'alpha': 0.9, 'turn': -3.0, 'pulse': 0.6,
                            'cards': ring(6, 0.7, 0.3, 0.15, 0.3)})}},
    # Vivify's, a burst of green smoke billowing up the healed (the flipbook, once) with wisps, a light and motes.
    'VivifyMist': {'model': 'MONK_VivifyMist', 'effect': 7121, 'shape': 'mist', 'textures': (ANIM, WISPS, GLOW, FLARE),
                   'options': {'duration': 1200, 'fade': (100, 400), 'layers': (
                       {'texture': 2, 'tint': GREEN, 'alpha': 0.35, 'pulse': 0, 'cards': ring(1, 0.0, 1.0, 1.8)},
                       {'texture': 0, 'sheet': (4, 4), 'flip': 1.2, 'tint': GREEN, 'alpha': 0.9, 'turn': 2.0,
                        'pulse': 0, 'rise': (0.8, 1.2), 'cards': ring(3, 0.4, 0.5, 1.7, 0.5)},
                       {'texture': 1, 'sheet': (2, 2), 'tint': GREEN, 'alpha': 0.6, 'turn': -2.0, 'pulse': 0,
                        'rise': (1.0, 1.2), 'cards': ring(3, 0.6, 0.6, 1.2, 0.4)},
                       {'texture': 3, 'tint': MOTE, 'alpha': 1.0, 'turn': 1.5, 'pulse': 0.3, 'rise': (1.5, 1.2),
                        'cards': ring(6, 0.7, 0.3, 0.16, 0.15)})}},
    # Life Cocoon: retail's cocoon (monk_chicocoon_state), 80% as big, its smoke strips scrolling, its flares
    # pulsing and its lightning flickering as retail's Stand does; its glow and lightning cards face the camera.
    'LifeCocoon': {'model': 'MONK_LifeCocoon', 'effect': 7117, 'shape': 'model', 'textures': (),
                   'options': {'export': 'MONK_LifeCocoon', 'base': 'monk_chicocoon_state', 'sequence': 1,
                               'duration': 1934, 'max_texture': 512, 'scale': 0.8,
                               'billboard': (2, 8, 9, 10, 11, 12, 13)}},
    # Revival: a ring of mist (puffs and wisps, spells/MONK_Revival_Ring.png) bursting out over the ground around the
    # Monk, a jade flare at its center...
    'RevivalBurst': {'model': 'MONK_Revival_Burst', 'effect': 7118, 'shape': 'burst',
                     'textures': ('MONK_Revival_Ring', FLARE, None),
                     'options': {'size': 30.0, 'ring_tint': JADE, 'ring_blend': BLEND_ADD, 'flash_sheet': False,
                                 'flash_tint': JADE, 'duration': 1200,
                                 'ring': {'scale': ([0, 600, 1200], [2.0, 28.0, 30.0]),
                                          'fade': ([0, 300, 1200], [0x7FFF, 0x7FFF, 0])}}},
    # ... and a column of jade mist, wisps and motes rising up around the Monk (1.8 sec).
    'RevivalMist': {'model': 'MONK_RevivalMist', 'effect': 7125, 'shape': 'mist',
                    'textures': (ANIM, WISPS, FLARE, GLOW),
                    'options': {'duration': 1800, 'fade': (100, 700), 'layers': (
                        {'texture': 3, 'tint': JADE, 'alpha': 0.35, 'pulse': 0, 'cards': ring(1, 0.0, 1.2, 2.6)},
                        {'texture': 0, 'sheet': (4, 4), 'flip': 1.8, 'tint': JADE, 'alpha': 0.85, 'turn': 3.0,
                         'pulse': 0, 'rise': (2.0, 1.8), 'cards': ring(6, 1.2, 0.3, 2.2, 0.15)},
                        {'texture': 1, 'sheet': (2, 2), 'tint': JADE, 'alpha': 0.7, 'turn': -2.5, 'pulse': 0,
                         'rise': (2.5, 1.8), 'cards': ring(6, 1.6, 0.5, 1.6, 0.2)},
                        {'texture': 2, 'tint': MOTE, 'alpha': 1.0, 'turn': 2.0, 'pulse': 0.4, 'rise': (3.0, 1.8),
                         'cards': ring(10, 1.4, 0.2, 0.2, 0.12)})}},
    # Chi-Ji, the Red Crane: arriving, a burst of red and gold mist and embers rising around the Monk (1.6 sec)...
    'ChiJiArrival': {'model': 'MONK_ChiJiArrival', 'effect': 7126, 'shape': 'mist',
                     'textures': (ANIM, WISPS, FLARE, GLOW),
                     'options': {'duration': 1600, 'fade': (100, 600), 'layers': (
                         {'texture': 3, 'tint': CRANE_RED, 'alpha': 0.4, 'pulse': 0, 'cards': ring(1, 0.0, 1.0, 2.6)},
                         {'texture': 0, 'sheet': (4, 4), 'flip': 1.6, 'tint': CRANE_RED, 'alpha': 0.85, 'turn': 2.5,
                          'pulse': 0, 'rise': (1.6, 1.6), 'cards': ring(5, 0.9, 0.3, 1.9, 0.2)},
                         {'texture': 1, 'sheet': (2, 2), 'tint': CRANE_GOLD, 'alpha': 0.7, 'turn': -2.0,
                          'pulse': 0, 'rise': (2.0, 1.6), 'cards': ring(5, 1.2, 0.5, 1.4, 0.2)},
                         {'texture': 2, 'tint': CRANE_MOTE, 'alpha': 1.0, 'turn': 1.8, 'pulse': 0.35,
                          'rise': (2.6, 1.6), 'cards': ring(9, 1.0, 0.2, 0.2, 0.12)})}},
    # ... Crane's Gust flying from Chi-Ji to the healed: a ball of red mist trailing embers...
    'CranesGustMissile': {'model': 'MONK_CranesGustMissile', 'effect': 7127, 'shape': 'mist',
                          'textures': (GLOW, WISPS, FLARE),
                          'options': {'layers': (
                              {'texture': 0, 'tint': CRANE_GOLD, 'alpha': 0.7, 'pulse': 0.4,
                               'cards': ring(1, 0.0, 0.0, 0.7)},
                              {'texture': 1, 'sheet': (2, 2), 'tint': CRANE_RED, 'alpha': 0.8, 'turn': 0.6,
                               'pulse': 0.5, 'cards': ring(3, 0.12, 0.0, 0.6)},
                              {'texture': 2, 'tint': CRANE_MOTE, 'alpha': 1.0, 'turn': -0.8, 'pulse': 0.3,
                               'cards': ring(4, 0.25, -0.1, 0.12, 0.06)})}},
    # ... and its heal: the jade heal in Chi-Ji's red and gold.
    'CraneHeal': {'model': 'MONK_CraneHeal', 'effect': 7128, 'shape': 'mist', 'textures': (GLOW, ANIM, WISPS, FLARE),
                  'options': {'duration': 1200, 'fade': (100, 450), 'layers': (
                      {'texture': 0, 'tint': CRANE_GOLD, 'alpha': 0.3, 'pulse': 0, 'cards': ring(1, 0.0, 1.0, 1.6)},
                      {'texture': 1, 'sheet': (4, 4), 'flip': 1.2, 'tint': CRANE_RED, 'alpha': 0.75, 'turn': 3.0,
                       'pulse': 0, 'rise': (0.9, 1.2), 'cards': ring(3, 0.45, 0.3, 1.3, 0.3)},
                      {'texture': 2, 'sheet': (2, 2), 'tint': CRANE_GOLD, 'alpha': 0.55, 'turn': -2.0, 'pulse': 0,
                       'rise': (1.1, 1.2), 'cards': ring(3, 0.6, 0.4, 1.0, 0.3)},
                      {'texture': 3, 'tint': CRANE_MOTE, 'alpha': 1.0, 'turn': 2.0, 'pulse': 0.35, 'rise': (1.5, 1.2),
                       'cards': ring(5, 0.7, 0.2, 0.18, 0.12)})}},
    # Refreshing Jade Wind: retail's whirlwind around the Monk (monk_rushingjadewind_missile's meshes, spells/
    # MONK_JadeWind): the tornado twice, turning and its wind flowing at two speeds, and the streak rings at the
    # waist...
    'JadeWind': {'model': 'MONK_JadeWind', 'effect': 7129, 'shape': 'spun',
                 'textures': ('MONK_JadeWind/shaman_windwalk_wind1', 'MONK_JadeWind/streak_halfcircle_01'),
                 'options': {'export': 'MONK_JadeWind/monk_rushingjadewind_missile.gltf', 'layers': (
                     {'mesh': 1, 'texture': 0, 'tint': JADE, 'alpha': 1.0, 'turn': 1.0, 'scroll': (-0.6, 0.0)},
                     {'mesh': 1, 'texture': 0, 'tint': MOTE, 'alpha': 0.7, 'turn': 1.7, 'scroll': (0.35, 0.25)},
                     {'mesh': 0, 'texture': 1, 'tint': JADE, 'alpha': 0.9, 'turn': 0.5},
                     {'mesh': 0, 'texture': 1, 'tint': MOTE, 'alpha': 0.5, 'turn': 0.8})}},
    # ... and its particles as puffs (on the chest, about 1.1 yards up): jade smoke billowing up the funnel, white
    # feathers whirling up it and wind swirls (spells/MONK_Feather.png, MONK_Swirl.png).
    'JadeWindMist': {'model': 'MONK_JadeWindMist', 'effect': 7130, 'shape': 'mist',
                     'textures': (ANIM, 'MONK_Feather', 'MONK_Swirl'),
                     'options': {'layers': (
                         {'texture': 0, 'sheet': (4, 4), 'flip': 1.2, 'tint': JADE, 'alpha': 0.7, 'turn': 1.1,
                          'pulse': 0, 'rise': (2.4, 1.2), 'cards': ring(6, 1.5, -0.9, 1.4, 0.1)},
                         {'texture': 1, 'tint': (0.85, 1.0, 0.92), 'alpha': 1.0, 'turn': 0.9, 'pulse': 1.5,
                          'rise': (2.2, 1.5), 'cards': ring(8, 1.7, -0.8, 0.45, 0.25)},
                         {'texture': 2, 'tint': JADE, 'alpha': 0.8, 'turn': 1.3, 'pulse': 0.9,
                          'cards': ring(4, 1.3, -0.1, 1.3, 0.5)})}},
    # Its heal: a small jade glint on each ally healed (six every 0.75 sec: kept light).
    'JadeWindHeal': {'model': 'MONK_JadeWindHeal', 'effect': 7131, 'shape': 'mist', 'textures': (GLOW, FLARE),
                     'options': {'duration': 600, 'fade': (50, 300), 'layers': (
                         {'texture': 0, 'tint': JADE, 'alpha': 0.4, 'pulse': 0, 'cards': ring(1, 0.0, 1.0, 1.2)},
                         {'texture': 1, 'tint': MOTE, 'alpha': 1.0, 'turn': 1.0, 'pulse': 0, 'rise': (0.8, 0.6),
                          'cards': ring(3, 0.4, 0.6, 0.18, 0.3)})}},
}

# Sounds (see patch_spells.SOUNDS): retail's Monk sounds (sounds/spell_mk_*); loops copy a Blizzard looping entry.
SOUNDS = {
    'MonkPrecast': {'id': 18120, 'copy': 27, 'files': ['spell_mk_jade_precast_low_loop']},
    'MonkPrecastHigh': {'id': 18121, 'copy': 27, 'files': ['spell_mk_jade_precast_high_loop']},
    'MonkChannel': {'id': 18122, 'copy': 3087, 'files': ['spell_mk_jadecast_low_loop']},
    'SoothingMistState': {'id': 18123, 'copy': 3087, 'files': ['spell_mk_jadebeam_player_state_loop']},
    'MonkHeal': {'id': 18124, 'copy': 1482, 'files': ['spell_mk_jade_heal_low_base']},
    'Revival': {'id': 18125, 'copy': 1433, 'files': [f'spell_mk_revival_0{n}' for n in range(1, 6)]},
    'RenewingMist': {'id': 18126, 'copy': 2561, 'files': ['spell_mk_renewingmist_cast']},
    'Vivify': {'id': 18127, 'copy': 2561, 'files': ['spell_mk_vivify_cast']},
    'JadeWind': {'id': 18129, 'copy': 2561, 'files': [f'spell_mk_jadewind_cast_0{n}' for n in range(2, 6)]},
    'CranesGust': {'id': 18128, 'copy': 1482, 'files': [f'spell_mk_invoke_chi_ji_heal_0{n}' for n in range(1, 4)]},
}

# The Monk's casting, on every visual: the jade on the hands and its loop while casting, let go on the cast.
PRECAST = {HANDS: 'JadePrecastHand', 'sound': 'MonkPrecast'}
CAST = {HANDS: 'JadeCastHand', 'sound': None}

# SpellVisual (and kit) ids, a Blizzard visual copied (its animations, timing) and every kit's look ours.
VISUALS = {
    # Soothing Mist: channeled as the Healing Beam, our mist beam from the Monk's hands flowing into the target, the
    # jade on the hands and the jade cast loop; the mist on the target with the beam's loop while it lasts.
    'SoothingMist': {'copy': 1283, 'id': 16730, 'clear': ('impact',), 'set': {VISUAL_CHANNEL: HEALING_BEAM_KIT},
                     'kits': {
        'precast': (15661, PRECAST),
        'cast': (15662, {**CAST, 'sound': 'MonkChannel'}),
        'channel': (15656, {HANDS: 'JadePrecastHand', 'beam': 'SoothingMist', 'sound': None}),
        'state': (15650, {KIT_CHEST: 0, KIT_BASE: 'SoothingMist', 'sound': 'SoothingMistState', 'procs': False})}},
    # Renewing Mist: its sound on the cast, a jade heal on the target, the green mist while it lasts.
    'RenewingMist': {'copy': 32, 'id': 16733, 'kits': {
        'precast': (15663, PRECAST),
        'cast': (15664, {**CAST, 'sound': 'RenewingMist'}),
        'impact': (15665, {KIT_BASE: 'JadeHeal', 'sound': 'MonkHeal'}),
        'state': (15657, {KIT_CHEST: 0, KIT_BASE: 'RenewingMist', 'sound': None, 'procs': False})}},
    # Enveloping Mist: the higher jade loop while casting, the gold mist on the target while it lasts.
    'EnvelopingMist': {'copy': 8145, 'id': 16734, 'kits': {
        'precast': (15666, {**PRECAST, 'sound': 'MonkPrecastHigh'}),
        'cast': (15667, {**CAST, 'sound': 'MonkHeal'}),
        'state': (15658, {KIT_HEAD: 0, KIT_BASE: 'EnvelopingMist', 'sound': None, 'procs': False})}},
    # Vivify: its sound on the cast, a burst of green mist on the healed; its splash the burst alone.
    'Vivify': {'copy': 58, 'id': 16735, 'kits': {
        'precast': (15668, PRECAST),
        'cast': (15669, {**CAST, 'sound': 'Vivify'}),
        'impact': (15659, {KIT_BASE: 'VivifyMist', 'sound': 'MonkHeal'})}},
    'VivifySplash': {'copy': 58, 'id': 16736, 'clear': ('precast', 'cast'), 'kits': {
        'impact': (15660, {KIT_BASE: 'VivifyMist', 'sound': None})}},
    # Life Cocoon: Power Word: Shield's sound on the cocoon around the target.
    'LifeCocoon': {'copy': 784, 'id': 16731, 'kits': {
        'precast': (15651, PRECAST),
        'cast': (15652, CAST),
        'state': (15653, {KIT_BASE: 'LifeCocoon', 'procs': False})}},
    # Revival: one of its sounds, the ring bursting out over the ground and the mist rising around the Monk; a jade
    # heal on each ally healed.
    'Revival': {'copy': 13751, 'id': 16732, 'clear': ('state',), 'kits': {
        'cast': (15654, {**CAST, KIT_CHEST: 0, KIT_WORLD: 'RevivalBurst', KIT_BASE: 'RevivalMist',
                         'anim': ANIM_SPELL_CAST_OMNI, 'sound': 'Revival'}),
        'impact': (15655, {KIT_BASE: 'JadeHeal', 'sound': None})}},
    # Essence Font: Tranquility's channeling with the jade on the hands and the jade cast loop (not its rain of
    # light); each bolt a jade heal on its ally (90305).
    'EssenceFont': {'copy': 1283, 'id': 16737, 'clear': ('impact',), 'kits': {
        'precast': (15670, PRECAST),
        'cast': (15671, {HANDS: 'JadePrecastHand', 'sound': None}),
        'channel': (15679, {HANDS: 'JadePrecastHand', KIT_BASE: 0, KIT_WORLD: 0, 'sound': 'MonkChannel'})}},
    'EssenceFontBolt': {'copy': 58, 'id': 16738, 'clear': ('precast', 'cast'), 'kits': {
        'impact': (15672, {KIT_BASE: 'JadeHeal', 'sound': None})}},
    # Thunder Focus Tea: Nature's Swiftness's timing, the jade let go from the hands and a jade heal on the Monk.
    'ThunderFocusTea': {'copy': 4040, 'id': 16739, 'kits': {
        'precast': (15673, PRECAST),
        'cast': (15674, {**CAST, 'sound': 'MonkHeal'}),
        'impact': (15675, {KIT_CHEST: 0, KIT_BASE: 'JadeHeal', 'sound': None})}},
    # Detox: Cleanse's timing, the jade from the hands and a jade heal on the cleansed.
    # Invoke Chi-Ji: Feral Spirit's timing, the jade from the hands, Revival's sound and Chi-Ji's red mist arriving.
    'InvokeChiJi': {'copy': 13077, 'id': 16741, 'clear': ('precast', 'impact', 'state', 'statedone'), 'kits': {
        'cast': (15680, {**CAST, KIT_CHEST: 0, KIT_BASE: 'ChiJiArrival', 'anim': ANIM_SPELL_CAST_OMNI,
                         'sound': 'Revival'})}},
    # Crane's Gust: Frozen Orb's missile visual (from the caster's middle to the target, straight), its missile our
    # ball of red mist; no cast animation on Chi-Ji; the crane's heal on the healed, with one of its three sounds.
    'CranesGust': {'copy': 15082, 'id': 16742, 'clear': ('precast', 'cast', 'impact', 'state', 'statedone', 'area'),
                   'set': {VISUAL_MISSILE_MODEL: 'CranesGustMissile', VISUAL_MISSILE_SOUND: 0,
                           VISUAL_MISSILE_MOTION: 0}, 'kits': {
        'cast': (15681, {KIT_CHEST: 0, 'anim': NO_ANIM, 'sound': None}),
        'impact': (15682, {KIT_BASE: 'CraneHeal', KIT_CHEST: 0, 'sound': 'CranesGust'})}},
    # Refreshing Jade Wind: Rejuvenation's timing, the jade let go from the hands and one of its sounds; the
    # whirlwind and its puffs around the Monk while it lasts; a jade glint on each ally healed.
    'RefreshingJadeWind': {'copy': 32, 'id': 16743, 'clear': ('precast', 'impact'), 'kits': {
        'cast': (15683, {**CAST, 'sound': 'JadeWind'}),
        'state': (15684, {KIT_BASE: 'JadeWind', KIT_CHEST: 'JadeWindMist', 'sound': None,
                          'procs': False})}},
    'JadeWindHeal': {'copy': 58, 'id': 16744, 'clear': ('precast', 'cast'), 'kits': {
        'impact': (15685, {KIT_BASE: 'JadeWindHeal', 'sound': None})}},
    # Windwalker, for now Blizzard's looks with fitting animations: Tiger Palm a punch (Sinister Strike's hit), the
    # kicks a kick, Fists of Fury Tranquility's channel punching on and on with the jade on the hands (not its rain
    # of light); Spinning Crane Kick and Whirling Dragon Punch keep Bladestorm's spin (their own visual).
    'TigerPalm': {'copy': 253, 'id': 16745, 'kits': {
        'cast': (15686, {'anim': ANIM_ATTACK_UNARMED})}},
    'BlackoutKick': {'copy': 253, 'id': 16746, 'kits': {
        'cast': (15687, {'anim': ANIM_KICK})}},
    'RisingSunKick': {'copy': 253, 'id': 16747, 'kits': {
        'cast': (15688, {'anim': ANIM_KICK, HANDS: 'JadeCastHand'})}},
    'FistsOfFury': {'copy': 1283, 'id': 16748, 'clear': ('precast', 'impact'), 'kits': {
        'cast': (15689, {HANDS: 'JadePrecastHand', 'anim': ANIM_ATTACK_UNARMED, 'sound': None}),
        'channel': (15690, {HANDS: 'JadePrecastHand', KIT_BASE: 0, KIT_WORLD: 0, 'anim': ANIM_ATTACK_UNARMED,
                            'sound': None})}},
    'Detox': {'copy': 337, 'id': 16740, 'kits': {
        'precast': (15676, PRECAST),
        'cast': (15677, CAST),
        'impact': (15678, {KIT_BASE: 'JadeHeal', 'sound': 'MonkHeal'})}},
}

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
        'id': 90300, 'copy': 740, 'name': 'Soothing Mist', 'icon': 'Ability_Monk_SoothingMists',
        'visual': 'SoothingMist',
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
        'id': 90301, 'copy': 8936, 'name': 'Vivify', 'icon': 'Ability_Monk_Vivify', 'visual': 'Vivify',
        'description': 'Heals the target for $s1, and every ally with your Renewing Mist for half as much.',
        'set': monk(90301, 1, CastingTimeIndex=CAST_1_5S, ManaCostPercentage=15, RangeIndex=RANGE_40),
        'clear': [2, 3],
        'effects': {1: {'Effect': EFFECT_HEAL, 'TargetA': TARGET_UNIT_TARGET_ALLY, **heal(40, 18.0)}},
    },
    {
        # Rejuvenation: healing every 2 sec for 20 sec; spell_titan_monk_renewing_mist moves it to an injured
        # ally once its target is at full health.
        'id': 90302, 'copy': 774, 'name': 'Renewing Mist', 'icon': 'Ability_Monk_RenewingMists',
        'visual': 'RenewingMist',
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
        'id': 90303, 'copy': 33763, 'name': 'Enveloping Mist', 'icon': 'Spell_Monk_EnvelopingMist',
        'visual': 'EnvelopingMist',
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
        'id': 90304, 'copy': 740, 'name': 'Essence Font', 'icon': 'Ability_Monk_EssenceFont', 'visual': 'EssenceFont',
        'description': 'Unleashes a rapid twirl of healing bolts at up to 6 allies within 30 yards every sec '
                       'for $d, each healing for $90305s1.',
        'set': monk(90304, 20, RecoveryTime=12000, ManaCostPercentage=25, DurationIndex=DURATION_3S),
        'clear': [1, 3],
        'effects': {2: {'Amplitude': 1000, 'TriggerSpell': 90305}},
    },
    {
        # Tranquility's healing pulse: up to 6 allies around the Monk.
        'id': 90305, 'copy': 44203, 'name': 'Essence Font', 'icon': 'Ability_Monk_EssenceFont',
        'visual': 'EssenceFontBolt', 'description': '',
        'set': monk(90305, 20, MaxAffectedTargets=6),
        'effects': {1: heal(30, 6.0)},
    },
    {
        # Power Word: Shield: a shield as big as 30% of the Monk's maximum health (spell_titan_monk_life_cocoon)
        # for 12 sec.
        'id': 90306, 'copy': 17, 'name': 'Life Cocoon', 'icon': 'Ability_Monk_ChiCocoon',
        'visual': 'LifeCocoon',
        'description': 'Encases the target in a cocoon of jade mist for $d, absorbing damage equal to 30% of '
                       'your maximum health.',
        'tooltip': 'Absorbing damage.',
        'set': monk(90306, 18, RecoveryTime=120000, ManaCostPercentage=10, RangeIndex=RANGE_40,
                    DurationIndex=DURATION_12S),
        'effects': {1: {'BasePoints': 0, 'DieSides': 1, 'MiscValue': ALL_SCHOOLS}},
    },
    {
        # Divine Hymn's heal, on the whole raid around the Monk at once; spell_titan_monk_revival also clears
        # one Magic, Poison and Disease effect from each.
        'id': 90307, 'copy': 64844, 'name': 'Revival', 'icon': 'Spell_Monk_Revival',
        'visual': 'Revival',
        'description': 'Heals all party and raid members within 40 yards for $s1 and clears them of a Magic, '
                       'Poison and Disease effect.',
        'set': monk(90307, 26, RecoveryTime=180000, ManaCostPercentage=35, MaxAffectedTargets=0,
                    StartRecoveryCategory=GLOBAL_COOLDOWN, StartRecoveryTime=1500),
        'clear': [2, 3],
        'effects': {1: {'TargetA': TARGET_UNIT_CASTER_AREA_RAID, 'TargetB': 0, **heal(100, 25.0)}},
    },
    {
        # Nature's Swiftness: the next Vivify is free and the next Enveloping Mist instant (one charge).
        'id': 90308, 'copy': 17116, 'name': 'Thunder Focus Tea', 'icon': 'Ability_Monk_ThunderFocusTea',
        'visual': 'ThunderFocusTea',
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
        'visual': 'Detox',
        'description': 'Removes 1 Magic, 1 Poison and 1 Disease effect from a friendly target.',
        'set': monk(90309, 8, RecoveryTime=8000, ManaCostPercentage=8, RangeIndex=RANGE_40),
    },
    {
        # Vivify's splash: a heal of the script's amount (half of Vivify's).
        'id': 90310, 'copy': 8936, 'name': 'Vivify', 'icon': 'Ability_Monk_Vivify', 'visual': 'VivifySplash',
        'description': '',
        'set': monk(90310, 1, CastingTimeIndex=CAST_INSTANT, ManaCostPercentage=0, ManaCost=0, RangeIndex=RANGE_40,
                    StartRecoveryCategory=0, StartRecoveryTime=0),
        'clear': [2, 3],
        'effects': {1: {'Effect': EFFECT_HEAL, 'TargetA': TARGET_UNIT_TARGET_ALLY, 'BasePoints': 0, 'DieSides': 1,
                        'RealPointsPerLevel': 0.0}},
    },
]

SPELLS += [
    {
        # Feral Spirit's summon, one guardian: Chi-Ji (npc_titan_chi_ji, src/monk.cpp) at the Monk's side for 25 sec,
        # healing with Crane's Gust; meanwhile Enveloping Mist costs half and casts twice as fast. The capstone.
        'id': 90311, 'copy': 51533, 'name': 'Invoke Chi-Ji, the Red Crane', 'icon': 'Monk_Ability_ChiJi',
        'visual': 'InvokeChiJi',
        'description': 'Summon an effigy of Chi-Ji, the Red Crane, for $d. Every 2 sec Chi-Ji heals the 2 most injured '
                       'allies within 40 yards with Crane\'s Gust, and once more each time you cast a healing spell. '
                       'While Chi-Ji is with you, Enveloping Mist costs 50% less mana and casts 50% faster.',
        'tooltip': 'Chi-Ji is at your side. Enveloping Mist costs 50% less mana and casts 50% faster.',
        'set': monk(90311, 30, RecoveryTime=180000, ManaCostPercentage=5, ManaCost=0, DurationIndex=DURATION_25S,
                    StartRecoveryCategory=GLOBAL_COOLDOWN, StartRecoveryTime=1500),
        'effects': {
            1: {'Effect': EFFECT_SUMMON, 'BasePoints': 0, 'DieSides': 1, 'MiscValue': NPC_CHI_JI,
                'MiscValueB': SUMMON_GUARDIAN},
            2: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_ADD_PCT_MODIFIER, 'BasePoints': -51, 'DieSides': 1,
                'MiscValue': SPELLMOD_COST, 'Mask': ENVELOPING_MIST, 'TargetA': TARGET_UNIT_CASTER, 'TargetB': 0,
                'RadiusIndex': 0},
            3: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_ADD_PCT_MODIFIER, 'BasePoints': -51, 'DieSides': 1,
                'MiscValue': SPELLMOD_CASTING_TIME, 'Mask': ENVELOPING_MIST, 'TargetA': TARGET_UNIT_CASTER,
                'TargetB': 0, 'RadiusIndex': 0},
        },
    },
    {
        # Chi-Ji's heal, cast by Chi-Ji for the Monk (its healing power, crits): a ball of red mist flying to the
        # healed.
        'id': 90312, 'copy': 8936, 'name': "Crane's Gust", 'icon': 'Monk_Ability_ChiJi', 'visual': 'CranesGust',
        'description': 'Heals the target for $s1.',
        'set': monk(90312, 30, CastingTimeIndex=CAST_INSTANT, ManaCostPercentage=0, ManaCost=0, RangeIndex=RANGE_100,
                    StartRecoveryCategory=0, StartRecoveryTime=0, Speed=20.0),
        'clear': [2, 3],
        'effects': {1: {'Effect': EFFECT_HEAL, 'TargetA': TARGET_UNIT_TARGET_ALLY, **heal(20, 6.0)}},
    },
]

SPELLS += [
    {
        # Rejuvenation's aura on the Monk, triggering the heal (90314) every 0.75 sec for 6 sec.
        'id': 90313, 'copy': 774, 'name': 'Refreshing Jade Wind', 'icon': 'Ability_Monk_RushingJadeWind',
        'visual': 'RefreshingJadeWind',
        'description': 'Summon a whirling tornado around you, healing up to 6 injured allies within 10 yards for '
                       '$90314s1 every 0.75 sec for $d.',
        'tooltip': 'Healing up to 6 injured allies within 10 yards every 0.75 sec.',
        'set': monk(90313, 22, RecoveryTime=45000, ManaCostPercentage=10, RangeIndex=RANGE_SELF,
                    DurationIndex=DURATION_6S),
        'clear': [2, 3],
        'effects': {1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_PERIODIC_TRIGGER_SPELL, 'Amplitude': 750,
                        'TriggerSpell': 90314, 'BasePoints': 0, 'DieSides': 1, 'TargetA': TARGET_UNIT_CASTER,
                        'TargetB': 0}},
    },
    {
        # The heal: the allies within 10 yards of the Monk, the 6 most injured (spell_titan_monk_jade_wind_heal).
        'id': 90314, 'copy': 44203, 'name': 'Refreshing Jade Wind', 'icon': 'Ability_Monk_RushingJadeWind',
        'visual': 'JadeWindHeal', 'description': '',
        'set': monk(90314, 22, MaxAffectedTargets=0, RangeIndex=RANGE_SELF),
        'effects': {1: {'TargetA': TARGET_SRC_CASTER, 'TargetB': TARGET_UNIT_SRC_AREA_ALLY, 'RadiusIndex': RADIUS_10,
                        **heal(8, 1.6)}},
    },
]



def windwalker(spell_id, level, **fields):
    """A Windwalker strike: melee, physical, on Energy (the finishers cost none: they take combo points), with or
    without a weapon (fists)."""
    return monk(spell_id, level, **{'SchoolMask': PHYSICAL, 'DmgClass': DMG_CLASS_MELEE, 'PowerType': ENERGY,
                                    'ManaCost': 0, 'ManaCostPercentage': 0, 'EquippedItemClass': -1,
                                    'EquippedItemSubClassMask': 0, 'EquippedItemInventoryTypeMask': 0, **fields})


def strikes(spell_id, level, name, icon, weapon_pct, target_a, target_b=0, radius=RADIUS_8):
    """A channel's or spin's strikes (Whirlwind's hit): weapon_pct% weapon damage to every enemy in its area."""
    return {'id': spell_id, 'copy': 50622, 'name': name, 'icon': icon, 'description': '',
            'set': windwalker(spell_id, level, MaxAffectedTargets=0, SpellVisual=WHIRLWIND_VISUAL),
            'clear': [2, 3],
            'effects': {1: {'Effect': EFFECT_WEAPON_PERCENT_DAMAGE, 'BasePoints': weapon_pct - 1, 'DieSides': 1,
                            'TargetA': target_a, 'TargetB': target_b, 'RadiusIndex': radius}}}


SPELLS += [
    {
        # Sinister Strike: a punch for weapon damage, 2 combo points.
        'id': 90320, 'copy': 1752, 'name': 'Tiger Palm', 'icon': 'Ability_Warrior_PunishingBlow', 'visual': 'TigerPalm',
        'description': 'Strike with the palm of an open hand for weapon damage plus $s1. Awards 2 combo points.',
        'set': windwalker(90320, 1, ManaCost=50, AttributesEx=0),
        'unset': {'AttributesEx3': 0x400},
        'effects': {1: {'Effect': EFFECT_NORMALIZED_WEAPON_DMG, 'BasePoints': 4, 'DieSides': 1,
                        'RealPointsPerLevel': 1.0},
                    2: {'Effect': EFFECT_ADD_COMBO_POINTS, 'BasePoints': 1, 'DieSides': 1}},
    },
    {
        # Sinister Strike's hit as a kick, a finisher (60% weapon damage a combo point); spell_titan_monk_blackout_kick
        # cools the two big ones down.
        'id': 90321, 'copy': 1752, 'name': 'Blackout Kick', 'icon': 'Ability_Kick',
        'visual': 'BlackoutKick',
        'description': 'Finishing move that kicks with a blast of energy, dealing 60% weapon damage per combo point '
                       '($s1% at 5) and reducing the cooldowns of Rising Sun Kick and Fists of Fury by 1 sec.',
        'set': windwalker(90321, 3, AttributesEx=0),
        'unset': {'AttributesEx3': 0x400},
        'clear': [2],
        'effects': {1: {'Effect': EFFECT_WEAPON_PERCENT_DAMAGE, 'BasePoints': 299, 'DieSides': 1}},
    },
    {
        # Mortal Strike: a finishing kick (90% weapon damage a combo point), healing taken -25% for 10 sec.
        'id': 90322, 'copy': 12294, 'name': 'Rising Sun Kick', 'icon': 'Ability_Warrior_Revenge',
        'visual': 'RisingSunKick',
        'description': 'Finishing move that kicks upwards, dealing 90% weapon damage per combo point ($s2% at 5) '
                       'and inflicting Mortal Wounds, reducing healing the target receives by 25% for $d.',
        'tooltip': 'Healing received reduced by 25%.',
        'set': windwalker(90322, 10, RecoveryTime=10000, DurationIndex=DURATION_10S, AttributesEx=0),
        'unset': {'AttributesEx3': 0x400},
        'effects': {1: {'BasePoints': -26, 'DieSides': 1},
                    2: {'Effect': EFFECT_WEAPON_PERCENT_DAMAGE, 'BasePoints': 449, 'DieSides': 1}},
    },
    {
        # Hellfire's channel (standing still), a finisher: striking (90324, 20% weapon damage a combo point) in a cone
        # every sec for 4 sec (spell_titan_monk_finisher_channel).
        'id': 90323, 'copy': 1949, 'name': 'Fists of Fury', 'icon': 'Ability_Hunter_SwiftStrike',
        'visual': 'FistsOfFury',
        'description': 'Finishing move that pummels all targets in front of you for 20% weapon damage per combo '
                       'point ($90324s1% at 5) every sec for $d.',
        'tooltip': 'Pummeling all targets in front.',
        'set': windwalker(90323, 16, RecoveryTime=24000, DurationIndex=DURATION_4S),
        'clear': [2, 3],
        'effects': {1: {'Amplitude': 1000, 'TriggerSpell': 90324}},
    },
    strikes(90324, 16, 'Fists of Fury', 'Ability_Hunter_SwiftStrike', 100, TARGET_UNIT_CONE_ENEMY),
    {
        # Bladestorm's spin (moving allowed), striking (90326) everything within 8 yards every 0.5 sec for 1.5 sec;
        # none of Bladestorm's immunities; a finisher (10% weapon damage a combo point, each strike).
        'id': 90325, 'copy': 46924, 'name': 'Spinning Crane Kick', 'icon': 'Ability_Whirlwind',
        'description': 'Finishing move that spins while kicking in the air, dealing 10% weapon damage per combo point '
                       '($90326s1% at 5) to all enemies within 8 yards every 0.5 sec for $d. You can move.',
        'tooltip': 'Spinning, kicking all enemies within 8 yards.',
        'set': windwalker(90325, 12, RecoveryTime=0, DurationIndex=DURATION_1_5S),
        'clear': [2, 3],
        'effects': {1: {'Amplitude': 500, 'TriggerSpell': 90326}},
    },
    strikes(90326, 12, 'Spinning Crane Kick', 'Ability_Whirlwind', 50, TARGET_SRC_CASTER,
            TARGET_UNIT_SRC_AREA_ENEMY),
    {
        # Bladestorm's spin, three strikes (90328) in a second; only while Rising Sun Kick and Fists of Fury are
        # cooling down (spell_titan_monk_whirling_dragon_punch).
        'id': 90327, 'copy': 46924, 'name': 'Whirling Dragon Punch', 'icon': 'Ability_Warrior_Bladestorm',
        'description': 'Erupt into a whirling punch, striking all enemies within 8 yards for $90328s1% weapon damage '
                       'three times. Only usable while Rising Sun Kick and Fists of Fury are on cooldown.',
        'tooltip': 'Whirling.',
        'set': windwalker(90327, 24, RecoveryTime=24000, DurationIndex=DURATION_1S),
        'clear': [2, 3],
        'effects': {1: {'Amplitude': 333, 'TriggerSpell': 90328}},
    },
    strikes(90328, 24, 'Whirling Dragon Punch', 'Ability_Warrior_Bladestorm', 90, TARGET_SRC_CASTER,
            TARGET_UNIT_SRC_AREA_ENEMY),
]

ICONS = sorted({spell['icon'] for spell in SPELLS})
