#!/usr/bin/env python3
"""Build the Emberforge_Runes addon's art and its Art.lua, from the "Legendary Rune Panel" mockup.

The mockup draws everything on a canvas; rune_art/bake.js runs its own drawing code (paintCard, the medallion, the
glows, its lettering) in headless Chromium, one PNG a piece, and this script brings each to a power-of-two size (the
3.3.5a client loads nothing else) as a BLP (large, painted: the cards, the medallion cores, the labels) or a TGA
(small or sharp: the glows, the ring pieces, the buttons). Art.lua tells the addon each rune's colours and words and
where each piece sits in its texture.

The runes' card words and glyphs are here (RUNES): the mockup has three (the Arcane Mind, Fel Commandment, and a
Frozen Heart that is not one of the 21); the rest are written to match. Fonts: Cinzel and Alegreya Sans (SIL Open
Font License, AddOns/Emberforge_Runes/Fonts/).

Usage: python3 build_rune_art.py
"""

import base64
import json
import math
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / 'models'))

from PIL import Image  # noqa: E402

import blp  # noqa: E402

ADDON = HERE / 'AddOns' / 'Emberforge_Runes'
OUT = ADDON / 'Art'
FONTS = ADDON / 'Fonts'

# A theme from one colour, the way the mockup's Fel and Arcane themes sit about theirs.
def mix(rgb, to, k):
    return tuple(round(c + (t - c) * k) for c, t in zip(rgb, to))


def css(rgb):
    return ','.join(str(c) for c in rgb)


def hexa(rgb):
    return '#' + ''.join(f'{c:02x}' for c in rgb)


def theme(rgb):
    hue = tuple(min(255, round(c * 1.12 + 20)) for c in rgb)
    return {'rgb': css(rgb), 'hot': css(mix(rgb, (255, 255, 255), 0.65)), 'hue': hexa(hue),
            'inner': hexa(mix(rgb, (255, 255, 255), 0.82)),
            'core': f'rgba({css(tuple(round(c * 0.5) for c in rgb))},0.9)',
            'ink': css(mix(rgb, (255, 255, 255), 0.9)), 'muted': css(mix(rgb, (215, 210, 220), 0.6)),
            'card': hexa(mix(rgb, (232, 232, 232), 0.75)), 'from': hexa(mix(rgb, (160, 160, 160), 0.6)),
            'deep': hexa(tuple(round(c * 0.07) for c in rgb))}


# The mockup's own two themes.
ARCANE = {'rgb': '185,140,255', 'hot': '235,215,255', 'hue': '#c79bff', 'inner': '#efe0ff',
          'core': 'rgba(120,70,220,0.9)', 'ink': '241,230,255', 'muted': '203,184,230', 'card': '#ddd0f0',
          'from': '#a996c8', 'deep': '#12062a'}
FEL = {'rgb': '110,235,80', 'hot': '214,255,196', 'hue': '#8cff6a', 'inner': '#e2ffd6', 'core': 'rgba(40,150,30,0.9)',
       'ink': '236,255,228', 'muted': '184,222,170', 'card': '#d6f0cc', 'from': '#94bd88', 'deep': '#061a0a'}

# Each rune: (id, colour or theme, school, card text, "From your ...", glyph: a 24-point line drawing).
RUNES = [
    (1, (120, 160, 255), 'Protection · Shield', 'Blocks and Revenge build Bulwark. Shield Slam turns it into a '
     'shield.', 'Shield Block and Revenge', 'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6ZM8 10h8M8 14h8'),
    (2, (240, 60, 80), 'Blood · Death Knight', 'Your strikes gather Vitae. Rune Tap drinks it to heal through '
     'spikes.', 'Heart Strike and Death Strike',
     'M12 3c3 4 6 7 6 11a6 6 0 0 1-12 0c0-4 3-7 6-11ZM9 15h6M12 12v6'),
    (3, (255, 110, 200), 'Vengeance · Demon Hunter', 'Shear shatters souls. Soul Cleave devours them to heal and '
     'weaken.', 'Shear and Soul Cleave',
     'M12 2c2 3 5 5 5 9a5 5 0 0 1-10 0c0-2 1-3 2-4 0 2 1 3 2 3 0-3-1-5 1-8ZM6 19h12M9 22h6'),
    (4, (255, 210, 110), 'Holy · Protection', 'Holy Shield blocks build Sanctity. Avenger\'s Shield carries it '
     'down the pack.', 'Holy Shield and Avenger\'s Shield',
     'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6ZM12 8l1.4 2.9 3.2.5-2.3 2.2.5 3.1L12 15.2l-2.8 1.5.5-3.1'
     '-2.3-2.2 3.2-.5Z'),
    (5, (255, 232, 150), 'Holy · Paladin', 'Direct heals build Radiance. Holy Shock bursts it across your '
     'allies.', 'Flash of Light and Holy Light',
     'M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8ZM12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9 7 7M17 17l2.1 2.1M4.9 19.1 7 17'
     'M17 7l2.1-2.1'),
    (6, (255, 200, 150), 'Discipline · Priest', 'Your Smites build Atonement. Power Word: Shield spends it on '
     'whoever needs it.', 'Smite and Holy Fire', 'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18ZM12 7v10M8 11h8'),
    (7, (70, 190, 255), 'Restoration · Shaman', 'Healing Waves raise a Tidal Surge. Chain Heal sweeps it through '
     'the group.', 'Healing Wave and Lesser Healing Wave',
     'M3 9c2-2 4-2 6 0s4 2 6 0 4-2 6 0M3 14c2-2 4-2 6 0s4 2 6 0 4-2 6 0M3 19c2-2 4-2 6 0s4 2 6 0 4-2 6 0M12 2v4'),
    (8, (160, 230, 100), 'Restoration · Druid', 'Rejuvenation grows Verdancy. Swiftmend plants a healing '
     'grove.', 'Rejuvenation',
     'M12 21V10M12 14c-4 0-7-3-7-7 4 0 7 3 7 7ZM12 11c3 0 6-2 6-6-3 0-6 2-6 6ZM8 21h8'),
    (9, (90, 235, 175), 'Mistweaver · Monk', 'Soothing Mist gathers Jade Essence. Enveloping Mist finishes the '
     'heal.', 'Soothing Mist', 'M17 4c-3-1-7 0-7 3s7 3 7 7-4 6-8 5M17 4l2 1M7 19l-2 1M9 20c1 1 3 1 4 0'),
    (10, ARCANE, 'Arcane · Mage', 'Arcane Blast builds Arcane Insight. At 4, your next Arcane Missiles is free and '
     'fires an extra volley.', 'Arcane Blast and Arcane Missiles', 'M12 2 4 7v10l8 5 8-5V7ZM12 2v20M4 7l16 10M20 7 4 17'),
    (11, (255, 125, 45), 'Fire · Mage', 'Fireball and Scorch criticals stoke Kindling, then an instant '
     'Pyroblast.', 'Fireball and Scorch',
     'M12 2c1 4 6 6 6 12a6 6 0 0 1-12 0c0-3 2-5 3-6 0 2 1 4 3 4 0-4-2-6 0-10ZM12 15a2 2 0 0 0 2 2'),
    (12, (140, 90, 255), 'Shadow · Priest', 'Mind Flay gathers Shadow Orbs. Mind Blast spends them.',
     'Mind Flay and Mind Blast', 'M2 12s4-6 10-6 10 6 10 6-4 6-10 6S2 12 2 12ZM12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6Z'),
    (13, (110, 215, 255), 'Elemental · Shaman', 'Lightning Bolt builds Static Charge. Earth Shock discharges '
     'it.', 'Lightning Bolt and Earth Shock', 'M13 2 5 13h6l-1 9 8-12h-6l1-8ZM4 6l2 2M20 6l-2 2'),
    (14, (200, 80, 230), 'Affliction · Warlock', 'Your curses harvest souls. Drain Life reaps them.',
     'Corruption and Curse of Agony', 'M5 21 15 5M15 5c3 0 6 2 6 6-2-2-4-3-7-3M8 16l3 3'),
    (15, (225, 215, 120), 'Marksmanship · Hunter', 'Steady Shot lines up Steady Aim. Aimed Shot lands the big '
     'hit.', 'Steady Shot and Aimed Shot',
     'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18ZM12 1v6M12 17v6M1 12h6M17 12h6M12 11v2'),
    (16, (255, 95, 60), 'Arms · Warrior', 'Overpower and Slam build Battle Trance. Mortal Strike cashes it in, '
     'then a free Execute.', 'Overpower and Slam',
     'M4 4l12 12M20 4 8 16M14 18l4-4M6 14l4 4M16 16l3 3M8 16l-3 3'),
    (17, (200, 205, 235), 'Subtlety · Rogue', 'Finishers sharpen your Shadow Edge. An empowered Backstab refills '
     'your combo points.', 'finishing moves and Backstab', 'M12 2l2 4v9h-4V6ZM8 15h8M12 15v4M10 21h4'),
    (18, (255, 190, 90), 'Retribution · Paladin', 'Crusader Strike builds Zeal. Judgement unleashes it and '
     'speeds your swings.', 'Crusader Strike and Judgement', 'M12 3v18M7 8h10M9 21h6M8 3h8'),
    (19, (190, 255, 70), 'Havoc · Demon Hunter', 'Demon\'s Bite and Chaos Strike feed your Hunger. Eye Beam '
     'unleashes it.', 'Demon\'s Bite and Chaos Strike',
     'M3 12c3-4 6-6 9-6s6 2 9 6c-3 4-6 6-9 6s-6-2-9-6ZM11 12l1-3 1 3-1 3Z'),
    (20, (255, 155, 70), 'Windwalker · Monk', 'Tiger Palm builds Tiger Spirit. Rising Sun Kick, then a free Fists '
     'of Fury.', 'Tiger Palm and Rising Sun Kick', 'M6 4c2 5 2 11 0 16M12 3c2 6 2 12 0 18M18 4c2 5 2 11 0 16'),
    (21, FEL, 'Fel · Havoc & Demonology', 'Your strikes and your demon build Fel Command. At 5, Throw Glaive hurls '
     'a volley of Chaos Bolts.', 'Throw Glaive, Chaos Strike and your demon',
     'M3 3c0 6 3.5 9 9 9s9-3 9-9M5.5 16c2 2.6 4.2 3.8 6.5 3.8s4.5-1.2 6.5-3.8c-2-2.4-4.2-3.6-6.5-3.6S7.5 13.6 5.5 '
     '16ZM12 14.6v2.8'),
    (22, (255, 222, 140), 'Holy · Consecration', 'Consecration becomes a sanctuary that follows you: a damage aura, '
     'a defensive zone or a source of healing.', 'Consecration',
     'M12 3v4M12 17v4M3 12h4M17 12h4M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8ZM5.6 5.6l2.1 2.1M16.3 16.3l2.1 2.1'
     'M5.6 18.4l2.1-2.1M16.3 7.7l2.1-2.1'),
]

# The runes the panel builds (their build view: the medallion, the slots, the options): the mockup's words for them.
# core: the build view's line (**bold** in the rune's ink); asks: each slot's question; texts and abilities: each
# option's words and its small line; icons: each option's drawing (the mockup's).
BUILT = {
    10: {
        'core': 'Casting Arcane Blast generates **Arcane Insight**. At 4 stacks, your next Arcane Missiles consumes all '
                'Insight, requires no mana, and fires an additional volley.',
        'asks': ['How you build Arcane Insight', 'What your empowered Arcane Missiles does',
                 'Utility, or a new tradeoff'],
        'texts': ['Arcane Blast critical strikes generate 1 additional Insight.',
                  'Arcane Explosion generates 1 Insight when it hits at least 3 enemies. Once per cast.',
                  'Insight stacks to 6. Empowered Arcane Missiles consumes them all and deals 8% more damage per '
                  'stack beyond 4.',
                  'The additional volley deals 50% increased damage to your primary target.',
                  'The additional volley also strikes up to 3 nearby enemies for 40% of its damage.',
                  'Leaves an Arcane Echo that deals 20% of the channel\'s damage over 6 sec.',
                  'You can move while channeling empowered Arcane Missiles.',
                  'Completing empowered Arcane Missiles restores 3% of your maximum mana.',
                  'Empowered Arcane Missiles channels 30% faster, but Arcane Blasts that build Insight cost 15% more '
                  'mana.'],
        'abilities': ['Arcane Blast', 'Arcane Explosion', 'Arcane Insight', 'Additional volley', 'Additional volley',
                      'Empowered Arcane Missiles', 'Mobility', 'Mana', 'Risk and reward'],
        'icons': ['M13 2 4 14h7l-1 8 9-12h-7z',
                  'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
                  'M12 7v5l3 3M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18Z',
                  'M12 2v4M12 18v4M2 12h4M18 12h4M12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6Z',
                  'M12 3v6M5 21l7-12 7 12M3 12h4M17 12h4', 'M4 12c3-6 5-6 8 0s5 6 8 0',
                  'M5 19 19 5M9 5h10v10', 'M12 3c3 4 6 6 6 10a6 6 0 0 1-12 0c0-4 3-6 6-10Z',
                  'M12 2 3 7v10l9 5 9-5V7Z M12 8v5M12 16v.5'],
    },
    21: {
        'core': 'Your strikes and your demon build **Fel Command**. At 5, Throw Glaive becomes **Chaos Bolt Volley**: '
                'the glaive and 3 Chaos Bolts, or 5 and a Felguard in Metamorphosis.',
        'asks': ['How you build Fel Command', 'What your Chaos Bolt Volley does', 'How you survive and move'],
        'texts': ['Your demon grants Fel Command twice as often: once every 2 sec instead of 4.',
                  'Entering Metamorphosis grants 5 Fel Command, so your first throw is an empowered volley.',
                  'Shadow Bolt and Incinerate grant 1 Fel Command each. Demon\'s Bite grants none.',
                  'Every throw spends your Fel Command: a Fel Imp for each stack, for 10 sec. Volley bolts deal 10% '
                  'more per demon you have out.',
                  'The glaive bounces to every enemy hit. Each critical bolt takes 3 sec off Throw Glaive.',
                  'Every throw spends your Fel Command: a demon for each stack, for 10 sec, Felguards in '
                  'Metamorphosis. Volley bolts deal 10% more per demon out.',
                  'In Metamorphosis, your demon takes 30% less damage and its attacks cleave.',
                  'For 3 sec after a volley, Fel Rush carries you to the glaive\'s first target.',
                  'The volley fires 2 extra bolts, but each bolt costs 3% of your maximum health.'],
        'abilities': ['Your demon', 'Metamorphosis', 'Shadow Bolt · Incinerate', 'Throw Glaive', 'Throw Glaive',
                      'Throw Glaive · Metamorphosis', 'Metamorphosis', 'Fel Rush', 'Risk and reward'],
        'icons': ['M4 5c0 6 3 9 8 9s8-3 8-9M8 18h8M10 21h4', 'M3 3c0 6 3.5 9 9 9s9-3 9-9M12 12v9',
                  'M13 2 4 14h7l-1 8 9-12h-7z',
                  'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M9 10h.5M14.5 10h.5M9 15c2 1.5 4 1.5 6 0',
                  'M12 2 15 9 22 12 15 15 12 22 9 15 2 12 9 9Z', 'M5 19 12 5l7 14M8 13h8',
                  'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z', 'M5 19 19 5M9 5h10v10',
                  'M12 3c3 4 6 6 6 10a6 6 0 0 1-12 0c0-4 3-6 6-10Z'],
    },
    22: {
        'core': 'Consecration becomes **Sanctified Paragon**: a permanent aura that follows you, sanctifying the ground '
                'within 8 yards. Cast Consecration to turn it on or off.',
        'slots': ['Presence', 'Manifestation', 'Covenant'],
        'asks': ['How your Consecration occupies the battlefield', 'What happens to those within your sanctuary',
                 'How Consecration supports your survival or control'],
        'texts': ['Increases Consecration\'s radius to 12 yards, but reduces its damage by 15%.',
                  'Moving leaves sanctified ground behind you for 3 sec. An enemy is struck once a tick, however '
                  'many patches it stands in.',
                  'Remaining still for 3 sec increases Consecration\'s damage and healing by 20%, until you move.',
                  'Each tick applies Condemnation: 3% more damage from your Consecration a stack, up to 10. Stacks '
                  'fade 3 sec outside it.',
                  'Each second, Consecration heals you and up to 4 injured allies for 25% of a tick\'s damage.',
                  'Each enemy\'s first tick deals 150% more damage, once every 12 sec per enemy.',
                  'Each enemy in your Consecration grants you 1% damage reduction, up to 5%. Consecration '
                  'generates 50% more threat.',
                  'Enemies damaged by Consecration are slowed by 20% for 2 sec, refreshed by each tick.',
                  'Every 5 sec, you and up to 4 nearby allies gain a shield absorbing 50% of a tick, for 6 sec.'],
        'abilities': ['Radius', 'Footsteps', 'Standing still', 'Condemnation', 'Healing', 'First tick',
                      'Damage reduction · Threat', 'Slow', 'Shields'],
        'icons': ['M12 12m-9 0a9 9 0 1 0 18 0a9 9 0 1 0-18 0M12 12m-4 0a4 4 0 1 0 8 0a4 4 0 1 0-8 0',
                  'M8 20c-2 0-3-2-2-4l2-4M16 12c2 0 3-2 2-4l-2-4M9 9h.5M15 15h.5',
                  'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6ZM9 12h6',
                  'M12 2v20M8 6h8M6 12h12',
                  'M12 21s-7-4.5-7-10a4 4 0 0 1 7-2.6A4 4 0 0 1 19 11c0 5.5-7 10-7 10ZM12 9v6M9 12h6',
                  'M12 2 15 9 22 12 15 15 12 22 9 15 2 12 9 9Z',
                  'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
                  'M4 12h10M10 8l4 4-4 4M18 5v14',
                  'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18ZM12 7v10M7 12h10'],
    },
}

# Sizes. The card (300 x 440 points) at CARD_SCALE pixels a point fills the top-left of a 512 square; the medallion's
# pieces span 260 points in a SEG square; the option card (340 x 172) and its glow, the pill and the slot disc.
CARD = {'scale': 512 / 440, 'size': 512}
SEG, CORE, ICON = 256, 256, 64
OPTION = {'scale': 1.1, 'margin': 24, 'size': [512, 256]}
PILL = {'scale': 2, 'margin': 20, 'size': [256, 256]}
NUM = {'scale': 1.6, 'size': 128}

SPRITES = {'SpriteEmber': 'rgb(255,170,70)', 'SpriteCream': 'rgb(255,228,165)', 'SpriteGold': 'rgb(255,180,90)'}

# The mockup's lettering that never changes (scale: pixels a point).
LABELS = {
    'Title': {'text': 'Legendary Runic Enhancements', 'font': '900 44px Cinzel', 'spacing': '6px', 'color': '#ffe2a8',
              'glows': [['rgba(255,150,60,0.55)', 28]], 'shadow': '#000', 'shadowY': 2, 'height': 56, 'pad': 30,
              'scale': 1.0},
    'Eyebrow': {'text': 'LEGENDARY RUNE', 'font': '700 14px Cinzel', 'spacing': '6px', 'color': '#ffb35c',
                'glows': [['rgba(255,140,40,0.6)', 12]], 'height': 18, 'pad': 14, 'scale': 2},
    'DoneEyebrow': {'text': 'YOUR RUNE IS COMPLETE', 'font': '700 14px Cinzel', 'spacing': '5px', 'color': '#ffb35c',
                    'height': 18, 'pad': 4, 'scale': 2},
}
# Every slot name a built rune uses (the doc's three; the Sanctified Paragon's Presence and Covenant), by name.
for _name in ['Invocation', 'Manifestation', 'Adaptation', 'Presence', 'Covenant']:
    LABELS[f'Choose{_name}'] = {'text': 'Choose your ' + _name, 'font': '700 28px Cinzel', 'spacing': '3px',
                                'color': '#ffe2a8', 'glows': [['rgba(255,150,60,0.55)', 20]], 'shadow': '#000',
                                'shadowY': 2, 'height': 36, 'pad': 24, 'scale': 1.5}
    LABELS[f'Slot{_name}'] = {'text': _name.upper(), 'font': '400 13px "Alegreya Sans"', 'spacing': '3px',
                              'color': '#ffffff', 'shadow': '#000', 'shadowBlur': 4, 'height': 16, 'pad': 6,
                              'scale': 2}

# Pieces up to SMALL pixels a side are TGAs (sharp); the larger are DXT BLPs (a quarter of the size or less).
SMALL = 64


def pow2(n):
    return 1 << max(0, math.ceil(math.log2(n)))


def save(image, name):
    """The piece at the top-left of a power-of-two canvas; returns the share of it the piece fills (u, v)."""
    w, h = image.size
    size = (pow2(w), pow2(h))
    if size != (w, h):
        canvas = Image.new('RGBA', size, (0, 0, 0, 0))
        canvas.paste(image, (0, 0))
        image = canvas
    if max(size) > SMALL:
        (OUT / f'{name}.blp').write_bytes(blp.encode_dxt(image))
    else:
        image.save(OUT / f'{name}.tga')
    return w / size[0], h / size[1]


def key(k):
    if isinstance(k, int):
        return f'[{k}]'
    return k if k.isidentifier() else '[' + json.dumps(k) + ']'


def lua(value, indent=0):
    pad = '    ' * indent
    if isinstance(value, dict):
        lines = [f'{pad}    {key(k)} = {lua(v, indent + 1).lstrip()},' for k, v in value.items()]
        return pad + '{\n' + '\n'.join(lines) + '\n' + pad + '}'
    if isinstance(value, list):
        if all(isinstance(v, (int, float)) for v in value):
            return pad + '{ ' + ', '.join(lua(v) for v in value) + ' }'
        return pad + '{\n' + '\n'.join(lua(v, indent + 1) + ',' for v in value) + '\n' + pad + '}'
    if isinstance(value, bool):
        return pad + ('true' if value else 'false')
    if isinstance(value, float):
        return pad + f'{value:.4f}'.rstrip('0').rstrip('.')
    if isinstance(value, int):
        return pad + str(value)
    # Long words: in pieces joined with .., so no line passes 120 columns.
    words, pieces, line = value.split(' '), [], ''
    for word in words:
        candidate = (line + ' ' + word) if line else word
        if len(candidate) > 70 and line:
            pieces.append(line + ' ')
            line = word
        else:
            line = candidate
    pieces.append(line)
    joiner = ' ..\n' + pad + '        '
    return pad + joiner.join(json.dumps(piece, ensure_ascii=False) for piece in pieces)


def floats(rgb_text):
    return [round(int(c) / 255, 3) for c in rgb_text.split(',')]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    runes, themes = [], {'Arcane': ARCANE}
    for rune_id, colour, school, text, source, glyph in RUNES:
        th = colour if isinstance(colour, dict) else theme(colour)
        entry = {'id': rune_id, 'theme': th, 'school': school, 'text': text, 'from': source, 'glyph': glyph,
                 'deep': th['deep'], 'glow': f'rgba({th["rgb"]},', 'hue': th['hue']}
        if rune_id in BUILT:
            entry['icons'] = BUILT[rune_id]['icons']
            entry['built'] = True
            if th is not ARCANE:
                themes[str(rune_id)] = th
        runes.append(entry)
    names = {}
    catalog = (ADDON / 'Catalog.lua').read_text()
    for rune_id in [r[0] for r in RUNES]:
        start = catalog.index(f'{{ id = {rune_id},')
        names[rune_id] = catalog[catalog.index('name = "', start) + 8:catalog.index('",', catalog.index('name = "',
                                                                                                         start))]
    for entry in runes:
        entry['name'] = names[entry['id']]
        # A rune with its own icon (icons/spells/, as its catalog entry names it) shows it in place of its glyph.
        start = catalog.index(f'{{ id = {entry["id"]},')
        icon = catalog[catalog.index('icon = "', start) + 8:catalog.index('",', catalog.index('icon = "', start))]
        png = HERE / 'icons' / 'spells' / f'{icon}.png'
        if png.exists():
            entry['icon'] = 'data:image/png;base64,' + base64.b64encode(png.read_bytes()).decode()
    fonts = {'FONT_CINZEL_700': FONTS / 'Cinzel-Bold.ttf', 'FONT_CINZEL_900': FONTS / 'Cinzel-Black.ttf',
             'FONT_ALEGREYA_400': FONTS / 'AlegreyaSans-Regular.ttf',
             'FONT_ALEGREYA_500': FONTS / 'AlegreyaSans-Medium.ttf',
             'FONT_ALEGREYA_700': FONTS / 'AlegreyaSans-Bold.ttf',
             'FONT_ALEGREYA_ITALIC': FONTS / 'AlegreyaSans-Italic.ttf'}
    job = {'fonts': {k: str(v) for k, v in fonts.items()}, 'runes': runes, 'themes': themes, 'card': CARD,
           'core': CORE, 'seg': SEG, 'icon': ICON, 'option': OPTION, 'pill': PILL, 'num': NUM, 'sprites': SPRITES,
           'labels': LABELS, 'labelScale': 2}
    for spec in LABELS.values():
        spec.setdefault('scale', 2)
    with tempfile.TemporaryDirectory() as work:
        work = Path(work)
        (work / 'job.json').write_text(json.dumps(job))
        # bake.js reads labelScale per label: one bake per scale.
        shares = {}
        for scale in sorted({spec['scale'] for spec in LABELS.values()}):
            job['labelScale'] = scale
            job['labels'] = {k: v for k, v in LABELS.items() if v['scale'] == scale}
            (work / 'job.json').write_text(json.dumps(job))
            subprocess.run(['node', str(HERE / 'rune_art' / 'bake.js'), str(work / 'job.json'), str(work / 'png')],
                           check=True, cwd=HERE / 'rune_art')
            sizes = json.loads((work / 'png' / 'labels.json').read_text())
            for name, size in sizes.items():
                shares[name] = size
        labels = {}
        for png in sorted((work / 'png').glob('*.png')):
            with Image.open(png) as image:
                u, v = save(image.convert('RGBA'), png.stem)
            if png.stem in shares:
                labels[png.stem] = {'w': shares[png.stem][0], 'h': shares[png.stem][1], 'u': round(u, 4),
                                    'v': round(v, 4)}
    print(f'  {len(list(OUT.iterdir()))} files in {OUT.relative_to(HERE)}')

    art = {'card': {'u': round(300 * CARD['scale'] / CARD['size'], 4), 'v': round(440 * CARD['scale'] / CARD['size'],
                                                                                  4)},
           'option': {'u': round((340 + 2 * OPTION['margin']) * OPTION['scale'] / OPTION['size'][0], 4),
                      'v': round((172 + 2 * OPTION['margin']) * OPTION['scale'] / OPTION['size'][1], 4),
                      'margin': OPTION['margin']},
           'pill': {'u': round(106 * PILL['scale'] / PILL['size'][0], 4),
                    'v': round((46 + 2 * PILL['margin']) * PILL['scale'] / PILL['size'][1], 4),
                    'margin': PILL['margin'], 'width': 106},
           'num': {'span': NUM['size'] / NUM['scale']}, 'seg': {'span': 260}, 'labels': labels, 'runes': {}}
    for entry in runes:
        th = entry['theme']
        rune = {'rgb': floats(th['rgb']), 'hot': floats(th['hot']), 'ink': floats(th['ink']),
                'muted': floats(th['muted']), 'hue': floats(css(tuple(int(th['hue'][i:i + 2], 16) for i in (1, 3, 5)))),
                'inner': floats(css(tuple(int(th['inner'][i:i + 2], 16) for i in (1, 3, 5))))}
        if entry['id'] in BUILT:
            built = BUILT[entry['id']]
            rune.update({'built': True, 'core': built['core'], 'asks': built['asks'], 'abilities': built['abilities'],
                         'texts': built['texts'],
                         'slots': built.get('slots', ['Invocation', 'Manifestation', 'Adaptation'])})
        art['runes'][entry['id']] = rune
    art['arcane'] = {'rgb': floats(ARCANE['rgb']), 'hot': floats(ARCANE['hot']), 'ink': floats(ARCANE['ink']),
                     'muted': floats(ARCANE['muted'])}
    text = ('-- Generated by client-patch/build_rune_art.py: the rune panel\'s art (Art/), each rune\'s colours and the '
            'words of\n-- the runes the panel builds (the "Legendary Rune Panel" mockup\'s).\nEmberforgeRunesArt = '
            + lua(art).lstrip() + '\n')
    (ADDON / 'Art.lua').write_text(text)
    print('  Art.lua')


if __name__ == '__main__':
    main()
