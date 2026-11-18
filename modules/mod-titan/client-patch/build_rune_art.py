#!/usr/bin/env python3
"""Build the Emberforge_Runes addon's art and its Art.lua, from the "Legendary Rune Panel" mockup.

The mockup draws everything on a canvas; rune_art/bake.js runs its own drawing code (paintCard, the medallion, the
glows, its lettering) in headless Chromium, one PNG a piece, and this script brings each to a power-of-two size (the
3.3.5a client loads nothing else) as a BLP (large, painted: the cards, the medallion cores, the labels) or a TGA
(small or sharp: the glows, the ring pieces, the buttons). Art.lua tells the addon each rune's colours and words and
where each piece sits in its texture.

The runes' card words and glyphs are here (RUNES): the runes the server builds (the first draft's twenty placeholders
are gone). Fonts: Cinzel and Alegreya Sans (SIL Open
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
    (21, FEL, 'Fel · Havoc & Demonology', 'Your strikes and your demon build Fel Command. At 5, Throw Glaive hurls '
     'a volley of Chaos Bolts.', 'Throw Glaive, Chaos Strike and your demon',
     'M3 3c0 6 3.5 9 9 9s9-3 9-9M5.5 16c2 2.6 4.2 3.8 6.5 3.8s4.5-1.2 6.5-3.8c-2-2.4-4.2-3.6-6.5-3.6S7.5 13.6 5.5 '
     '16ZM12 14.6v2.8'),
    (22, (255, 222, 140), 'Holy · Consecration', 'Consecration becomes a sanctuary that follows you: a damage aura, '
     'a defensive zone or a source of healing.', 'Consecration',
     'M12 3v4M12 17v4M3 12h4M17 12h4M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8ZM5.6 5.6l2.1 2.1M16.3 16.3l2.1 2.1'
     'M5.6 18.4l2.1-2.1M16.3 7.7l2.1-2.1'),
    (23, (95, 170, 255), 'Lightning · Stormbound',
     'Lightning Bolt arcs through nearby enemies.',
     'Lightning Bolt and Lightning Shield',
     'M13 2 5 13h6l-1 9 8-12h-6l1-8ZM3 8l3 2-3 3M21 8l-3 2 3 3'),
    (24, (120, 160, 255), 'Protection · Thunder',
     'Thunder Clap charges you; Shield Slam echoes it.',
     'Thunder Clap and Shield Slam',
     'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6ZM13 7l-4 6h4l-2 5'),
    (25, (70, 215, 210), 'Restoration · Wellspring',
     'Chain Heal always reaches one more ally.',
     'Chain Heal and Earth Shield',
     'M12 3c3 4 6 6 6 10a6 6 0 0 1-12 0c0-4 3-6 6-10ZM8 14c2 2 6 2 8 0'),
    (26, (90, 225, 140), 'Mistweaver · Jade Mist',
     'Soothing Mist always reaches a second ally.',
     'Soothing Mist',
     'M4 9c3-3 6-3 8 0s5 3 8 0M4 15c3-3 6-3 8 0s5 3 8 0'),
    (27, (150, 200, 235), 'Frost, Unholy & Blood · Grave',
     'Three strikes lay three Graves beneath your enemy.',
     'Icy Touch, Plague Strike and Blood Strike',
     'M12 3a6 6 0 0 0-6 6c0 2.4 1.2 3.8 2 4.6V17h8v-3.4c.8-.8 2-2.2 2-4.6a6 6 0 0 0-6-6ZM10 10h.01M14 10h.01M9 21h6M1'
     '0 17v4M14 17v4'),
    (28, (235, 70, 60), 'Arms & Fury · Onslaught',
     'Mortal Strike cuts a wake; your next strike rips it open.',
     'Mortal Strike or Bloodthirst',
     'M4 20 20 4M14 4h6v6M4 14l6 6M7 11l6 6'),
    (29, (255, 200, 90), 'Holy & Retribution · Dawn',
     'Crusader Strike and Holy Shock land twice.',
     'Crusader Strike and Holy Shock',
     'M3 18h18M6 18a6 6 0 0 1 12 0M12 6V3M5.6 9.6 3.5 7.5M18.4 9.6l2.1-2.1M12 12v3'),
    (30, (170, 110, 235), 'Shadow · Congregation',
     'Mind Flay calls hollow echoes; Mind Blast commands them.',
     'Mind Flay and Mind Blast',
     'M12 3c-3 0-5 2.5-5 6v10l2-2 3 3 3-3 2 2V9c0-3.5-2-6-5-6ZM10 10h.01M14 10h.01'),
    (31, (120, 200, 240), 'Fire & Frost · Shattered Sun',
     'Fire embeds burning shards; frost shatters them in a burst.',
     'Your fire and frost spells',
     'M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8ZM12 2v3M12 19v3M2 12h3M19 12h3M5 5l2 2M17 17l2 2M5 19l2-2M17 7l2-2'),
    (32, (170, 100, 255), 'Rogue · Spectral Blade',
     'Eviscerate calls a phantom assassin that strikes again.',
     'Eviscerate and your combo points',
     'M4 20 15 9M15 9l5-5-1 6-4-1ZM6 14l4 4'),
    (33, (110, 220, 120), 'Hunter · Wild Hunt',
     'Kill Command and Bestial Wrath unleash spectral copies of your pet.',
     'Kill Command, Bestial Wrath and your pet',
     'M4 18l3-9 3 3 2-6 2 6 3-3 3 9Z'),
    (34, (120, 150, 255), 'Arcane · Arcane Mind',
     'Arcane Blast builds Insight; Arcane Missiles spends it in a volley.',
     'Arcane Blast and Arcane Missiles',
     'M2 12s4-7 10-7 10 7 10 7-4 7-10 7S2 12 2 12ZM12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6Z'),
    (35, (130, 205, 255), 'Frost · Splintered Crown',
     'Frostbolt forms Icicles that strike on their own.',
     'Frostbolt',
     'M4 20 14 10M14 10l6-6-2 7-4-1ZM8 4l2 5M16 16l4 2'),
    (36, (255, 140, 60), 'Fire · Phoenix Wake',
     'Pyroblast leaves an Ember; Fire Blast calls a phoenix diving onto it.',
     'Pyroblast and Fire Blast',
     'M3 10c4 0 6 2 9 6 3-4 5-6 9-6-2 4-5 8-9 10-4-2-7-6-9-10ZM12 4v5'),
    (37, (205, 120, 255), 'Arcane · Astral Horizon',
     'Arcane Barrage sets an Arcane Orb loose that fires on its own.',
     'Arcane Barrage',
     'M12 12m-5 0a5 5 0 1 0 10 0a5 5 0 1 0-10 0M2 15c6-5 14-9 20-5'),
    (38, (255, 205, 110), 'Holy · Guiding Light',
     'Prayer of Mending places a healing star you move with your heals.',
     'Prayer of Mending, Flash Heal and Greater Heal',
     'M12 2l2.5 7.5L22 12l-7.5 2.5L12 22l-2.5-7.5L2 12l7.5-2.5Z'),
    (39, (150, 160, 255), "Arcane · Elune's Guardian",
     'Starfire calls lunar owls that dive on your enemies.',
     'Starfire',
     'M12 3a9 9 0 1 0 9 9 7 7 0 0 1-9-9ZM9 11h.01M15 11h.01'),
    (40, (255, 170, 90), 'Nature & Arcane · Convergence',
     'Wrath and Starfire in turn: Sunlight, a collision, Moonlight, a flare.',
     'Wrath and Starfire',
     'M12 12m-4 0a4 4 0 1 0 8 0a4 4 0 1 0-8 0M12 2v3M12 19v3M2 12h3M19 12h3'),
    (41, (230, 180, 90), "Physical · Ursoc's Champion",
     'Barkskin in Bear Form raises a guard; Maul answers with what it took.',
     'Bear Form, Barkskin and Maul',
     'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6ZM9 10h.01M15 10h.01'),
    (42, (230, 185, 95), 'Physical · Duelist',
     'Sabre Slash, Pistol Shot and Broadside: steel, shot and explosions.',
     'Sinister Strike',
     'M4 20 16 8M14 6l4 4M18 4l2 2M6 12h6l2-2'),
    (43, (90, 200, 255), 'Nature · Stormreaver',
     'Stormstrike tears open a storm; Lava Lash and Lightning Shield strike through it.',
     'Stormstrike, Lava Lash and Lightning Shield',
     'M13 2 4 14h7l-1 8 9-12h-7Z'),
    (44, (255, 125, 55), "Fire · Ember's Wrath",
     'Fire Blast in Cat Form ignites your bleeds into Cinder Wounds.',
     'Cat Form, Fire Blast, Rake or Rip',
     'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11ZM9 15l2-3 2 3'),
    (45, (140, 210, 245), 'Frost · Whitefang',
     'Icy Touch in Cat Form marks the prey; Shred or Mangle shatters it.',
     'Cat Form, Icy Touch, Shred or Mangle',
     'M12 2v20M4 6l16 12M20 6 4 18M9 3l3 3 3-3M9 21l3-3 3 3'),
]

# The runes the panel builds (their build view: the medallion, the slots, the options): the mockup's words for them.
# core: the build view's line (**bold** in the rune's ink); asks: each slot's question; texts and abilities: each
# option's words and its small line; icons: each option's drawing (the mockup's).
BUILT = {
    21: {
        'core': 'Your strikes and your demon build **Fel Command**. At 5, Throw Glaive becomes **Chaos Bolt Volley**: '
                'the glaive and 3 Chaos Bolts.',
        'slots': ['Invocation', 'Manifestation', 'Adaptation'],
        'asks': ['How you build Fel Command', 'What your Chaos Bolt Volley does', 'What your pact brings'],
        'texts': [
            'Your demon grants Fel Command twice as often.',
            'Metamorphosis grants 5 Fel Command.',
            'Shadow Bolt and Incinerate grant 2 each.',
            'The volley fires 2 more bolts.',
            "Critical bolts cut Throw Glaive's cooldown.",
            'The volley calls a Felguard for 8 sec.',
            'In Metamorphosis your demon is tougher and cleaves.',
            'The volley calls a Fel Imp for each bolt.',
            '2 more bolts, 3% of your health each.',
        ],
        'abilities': [
            'Your demon',
            'Metamorphosis',
            'Shadow Bolt · Incinerate',
            'Throw Glaive',
            'Throw Glaive',
            'Felguard',
            'Your demon',
            'Imps',
            'Health',
        ],
        'icons': [
            'M4 5c0 6 3 9 8 9s8-3 8-9M8 18h8M10 21h4',
            'M3 3c0 6 3.5 9 9 9s9-3 9-9M12 12v9',
            'M13 2 4 14h7l-1 8 9-12h-7z',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M9 10h.5M14.5 10h.5M9 15c2 1.5 4 1.5 6 0',
            'M12 2 15 9 22 12 15 15 12 22 9 15 2 12 9 9Z',
            'M5 19 12 5l7 14M8 13h8',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
            'M5 19 19 5M9 5h10v10',
            'M12 3c3 4 6 6 6 10a6 6 0 0 1-12 0c0-4 3-6 6-10Z',
        ],
    },
    22: {
        'core': 'Consecration becomes **Sanctified Paragon**: an aura that follows you within 8 yards at 40% of '
                'Consecration\'s damage, for 1% of your base mana a second. Cast Consecration to turn it on or off.',
        'slots': ['Presence', 'Manifestation', 'Covenant'],
        'asks': ['How your sanctuary covers the ground', 'Damage, healing or protection',
                 'Threat, control or upkeep'],
        'texts': [
            'The aura reaches 12 yards instead of 8, but its damage, healing and shields are 25% lower.',
            'The aura shrinks to 4 yards, but moving leaves sanctified footsteps for 2 sec that tick like the aura. '
            'An enemy is hit once a tick, however many it stands in.',
            "Standing still for 3 sec raises the aura's damage, healing and shields by 10% until you move.",
            'Damage: each tick on an enemy raises its damage taken from your aura by 2%, up to 10%. Lost 2 sec after '
            'it leaves the aura.',
            'Healing: the aura no longer deals damage. Each second it heals your most injured party member in range.',
            "Protection: the aura no longer deals damage. Every 5 sec it shields a nearby party member for 5 ticks' "
            'worth. One shield at a time.',
            "The aura's damage generates 50% more threat. Needs Sanctified Condemnation.",
            'An enemy hit by 3 ticks in a row has its movement speed reduced by 15% while it stays in the aura. Needs '
            'Sanctified Condemnation.',
            'The aura costs 0.75% of your base mana a second instead of 1%, but its damage, healing and shields are '
            '10% lower.',
        ],
        'abilities': ['Radius', 'Footsteps', 'Standing still', 'Condemnation', 'Healing', 'Shields',
                      'Threat', 'Slow', 'Mana'],
        'icons': ['M12 12m-9 0a9 9 0 1 0 18 0a9 9 0 1 0-18 0M12 12m-4 0a4 4 0 1 0 8 0a4 4 0 1 0-8 0',
                  'M8 20c-2 0-3-2-2-4l2-4M16 12c2 0 3-2 2-4l-2-4M9 9h.5M15 15h.5',
                  'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6ZM9 12h6',
                  'M12 2v20M8 6h8M6 12h12',
                  'M12 21s-7-4.5-7-10a4 4 0 0 1 7-2.6A4 4 0 0 1 19 11c0 5.5-7 10-7 10ZM12 9v6M9 12h6',
                  'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18ZM12 7v10M7 12h10',
                  'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
                  'M4 12h10M10 8l4 4-4 4M18 5v14',
                  'M12 3c3 4 6 6 6 10a6 6 0 0 1-12 0c0-4 3-6 6-10Z'],
    },
    23: {
        'core': "**Lightning Bolt** arcs to 2 nearby enemies for 30% of its damage. Your **Lightning Shield**'s "
                'retaliations arc too.',
        'slots': ['Arcs', 'Lightning Shield', 'Surge'],
        'asks': ['Where your arcs go', 'What your Lightning Shield does', 'What the storm adds'],
        'texts': [
            'Lightning Bolt arcs to 3 enemies instead of 2, for 30% of its damage each.',
            'Lightning Bolt no longer arcs: it strikes its own target again for 60% of its damage.',
            'Arcs reach enemies 15 yards away instead of 10, and deal 10% more damage.',
            "Lightning Shield's retaliations arc to 2 enemies instead of 1, for 50% of the retaliation each.",
            'In combat, Lightning Shield strikes the nearest enemy within 8 yards every 3 sec on its own.',
            'Lightning Bolt also fires a Lightning Shield retaliation at its target, which arcs as usual.',
            'Each Chain Lightning hit arcs to 1 more enemy for 20% of that hit.',
            'Each arc shields you for 1% of your maximum health for 8 sec, stacking up to 10% of your maximum health.',
            'Arcs from a critical Lightning Bolt or retaliation deal 50% more damage.',
        ],
        'abilities': [
            '3 arcs',
            'Single target',
            '15 yards',
            '2 arcs',
            'Pulse',
            'Retaliation',
            'Chain Lightning',
            'Shield',
            'Critical',
        ],
        'icons': [
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18Z',
            'M12 3v6M5 21l7-12 7 12M3 12h4M17 12h4',
            'M4 12c3-6 5-6 8 0s5 6 8 0',
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 4v4l3-2',
            'M12 12m-9 0a9 9 0 1 0 18 0a9 9 0 1 0-18 0M12 12m-4 0a4 4 0 1 0 8 0a4 4 0 1 0-8 0',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6ZM12 8v4M12 15v.5',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6ZM13 7l-4 6h4l-2 5',
            'M13 2 4 14h7l-1 8 9-12h-7z',
        ],
    },
    24: {
        'core': '**Thunder Clap** charges you with thunder. Your next **Shield Slam** releases a **Thunder Echo** '
                'around its target.',
        'slots': ['Charge', 'Echo', 'Resonance'],
        'asks': ['How you hold the charge', 'How the echo strikes', 'What the echo gives you'],
        'texts': [
            "Thunder Clap's charge holds 2 Thunder Echoes: your next 2 Shield Slams each release one.",
            "Thunder Clap's charge lasts 20 sec instead of 8.",
            'Thunder Echo generates twice the threat of its damage.',
            "Thunder Echo hits only Shield Slam's target, for 67% of Shield Slam's damage instead of 42%.",
            "Thunder Echo hits enemies within 10 yards of Shield Slam's target instead of 6.",
            'Enemies hit by Thunder Echo have their movement speed reduced by 30% for 3 sec.',
            'Thunder Echo shields you for 3% of your maximum health per enemy hit, up to 15% of your maximum health, '
            'for 6 sec.',
            'Thunder Echo grants you 2 Rage for each enemy it hits.',
            "Each enemy Thunder Echo hits takes 1 sec off Thunder Clap's cooldown.",
        ],
        'abilities': [
            '2 echoes',
            '20 sec',
            'Threat',
            'Focus',
            '10 yards',
            'Slow',
            'Shield',
            'Rage',
            'Cooldown',
        ],
        'icons': [
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18Z',
            'M5 19 19 5M9 5h10v10',
            'M4 12h10M10 8l4 4-4 4M18 5v14',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M12 2v4M12 18v4M2 12h4M18 12h4M12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6Z',
            'M3 12h14M13 7l5 5-5 5M21 7v10',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6ZM9 12h6',
            'M4 12c3-6 5-6 8 0s5 6 8 0',
        ],
    },
    25: {
        'core': '**Chain Heal** jumps once more, for 40% of its first heal, and heals your **Earth Shield** wearer '
                'for 20% more.',
        'slots': ['Spring', 'Earth', 'Undertow'],
        'asks': ['Where the extra jump goes', 'What your Earth Shield adds', 'What the jump leaves behind'],
        'texts': [
            "The extra jump heals 2 allies instead of 1, for 25% of Chain Heal's first heal each (instead of 40%).",
            "The extra jump heals 40% more on an ally below half health: 56% of Chain Heal's first heal instead of "
            '40%.',
            "The extra jump heals for 48% of Chain Heal's first heal instead of 40%.",
            'Each Earth Shield heal also heals the most injured ally near its wearer for half as much.',
            'Chain Heal heals your Earth Shield wearer for 40% more healing instead of 20% more.',
            'Chain Heal on your Earth Shield wearer restores 1 Earth Shield charge.',
            "The extra jump also shields its ally for 20% of the jump's heal for 4 sec.",
            "The extra jump also heals its ally for 20% of the jump's heal over 4 sec.",
            'The extra jump can reach an ally 20 yards away instead of 12.5.',
        ],
        'abilities': [
            '2 allies',
            'Low health',
            'Power',
            'Earth Shield',
            'Wearer',
            'Charges',
            'Shield',
            'Over time',
            '20 yards',
        ],
        'icons': [
            'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18ZM12 7v10M7 12h10',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18Z',
            'M5 19 19 5M9 5h10v10',
            'M12 21s-7-4.5-7-10a4 4 0 0 1 7-2.6A4 4 0 0 1 19 11c0 5.5-7 10-7 10ZM12 9v6M9 12h6',
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M12 2v4M12 18v4M2 12h4M18 12h4M12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6Z',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
            'M4 12h10M10 8l4 4-4 4M18 5v14',
            'M12 3c3 4 6 6 6 10a6 6 0 0 1-12 0c0-4 3-6 6-10Z',
        ],
    },
    26: {
        'core': 'Each **Soothing Mist** tick also sends a **Wandering Mist** to your most injured ally nearby, for '
                '25% of the tick.',
        'slots': ['Mistflow', 'Technique', 'Approach'],
        'asks': ['Where your mist wanders', 'How your techniques share', 'How you sustain it'],
        'texts': [
            'Wandering Mist reaches allies up to 30 yards from your Soothing Mist target instead of 20.',
            "Each Soothing Mist tick sends Wandering Mist to 2 allies, healing 15% of the tick's healing each, "
            'instead of 25% to one.',
            'Wandering Mist also shields its ally for 10% of its heal.',
            'Enveloping Mist also wraps your most injured nearby ally in a Lesser Enveloping Mist that heals 30% as '
            'much.',
            'Vivify also heals your most injured nearby ally for 30% of its heal.',
            'Soothing Mist heals 15% more on a target with your Enveloping Mist.',
            'Soothing Mist costs 25% less mana.',
            'When your Soothing Mist ends, its target is healed for 2 more ticks over 4 sec.',
            'Each Wandering Mist restores 0.3% of your maximum mana.',
        ],
        'abilities': [
            '30 yards',
            '2 allies',
            'Shield',
            'Enveloping Mist',
            'Vivify',
            'Enveloping Mist',
            'Mana',
            'Lingering',
            'Mana',
        ],
        'icons': [
            'M5 19 19 5M9 5h10v10',
            'M12 21s-7-4.5-7-10a4 4 0 0 1 7-2.6A4 4 0 0 1 19 11c0 5.5-7 10-7 10Z',
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M4 9c3-3 6-3 8 0s5 3 8 0M4 15c3-3 6-3 8 0s5 3 8 0',
            'M13 2 4 14h7l-1 8 9-12h-7z',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M4 12h10M10 8l4 4-4 4M18 5v14',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
            'M12 3c3 4 6 6 6 10a6 6 0 0 1-12 0c0-4 3-6 6-10Z',
        ],
    },
    27: {
        'core': '**Icy Touch**, **Plague Strike** and **Blood Strike** each lay their **Grave** beneath their target; '
                'enemies in two take more. Blood Strike shields you.',
        'slots': ['Burial Grounds', 'Gravekeeper', 'Shared Burial'],
        'asks': ['How your Graves lie', 'What each Grave keeps', 'What a shared burial gives'],
        'texts': [
            'Your Graves are 50% wider.',
            'Your Graves last 12 sec.',
            'Your Graves follow their enemy.',
            'The Frozen Grave slows by 50%.',
            'The Plagued Grave spreads a disease.',
            "Blood Strike's shield holds up to 25%.",
            'Two or more Graves: 35% more.',
            'Graves deal 25% more below 35% health.',
            'Your Graves heal you each second.',
        ],
        'abilities': [
            'Wider',
            '12 sec',
            'Follow',
            'Frozen Grave',
            'Plagued Grave',
            'Blood Strike',
            'Shared',
            'Execute',
            'Heal',
        ],
        'icons': [
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M8 4c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4ZM16 10c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4Z',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0',
            'M12 2v20M2 12h20M5 5l14 14M19 5 5 19',
            'M12 3c3 4 6 6 6 10a6 6 0 0 1-12 0c0-4 3-6 6-10Z',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
            'M6 6h.01M12 6h.01M18 6h.01M6 12h12M6 18h12',
            'M12 12m-9 0a9 9 0 1 0 18 0a9 9 0 1 0-18 0M12 7v5l3 2',
            'M4 12a8 8 0 0 1 14-5l2-2v6h-6l2-2a5 5 0 0 0-9 3',
        ],
    },
    28: {
        'core': '**Mortal Strike** or **Bloodthirst** cuts a **Weapon Wake**; your next other melee ability rips it '
                'open.',
        'slots': ['Wake', 'Follow-Through', 'Pursuit'],
        'asks': ['The shape of your wake', 'How it rips open', 'What it gives you'],
        'texts': [
            "A narrow cleft through your target: the wake's first target takes 50% more damage instead of 25% more.",
            'The wake is a crescent 8 yards wide in front of you, instead of a line through your target.',
            'The wake reaches 14 yards past your target, hitting the enemies behind it.',
            'The wake rips open twice: once for each of your next two melee abilities, 96% of a weapon swing each '
            'time.',
            "Enemies the wake hits bleed for 30% of the wake's damage every second for 4 sec (120% of its damage in "
            'all).',
            '1.5 sec after the wake rips open, it strikes again for half its damage: 48% of a weapon swing.',
            'Your wake moves with its first target, instead of staying where it was cut.',
            'Enemies the wake hits are claimed for 8 sec: their movement speed is reduced by 30% and they take 10% '
            'more damage from you.',
            'Each enemy the wake hits heals you for 1% of your maximum health.',
        ],
        'abilities': [
            'Cleft',
            'Crescent',
            'Long slash',
            'Twice',
            'Bleed',
            'Aftershock',
            'Follow',
            'Claim',
            'Heal',
        ],
        'icons': [
            'M4 20 20 4',
            'M3 16a9 9 0 0 1 18 0',
            'M4 12h10M10 8l4 4-4 4M18 5v14',
            'M13 2 4 14h7l-1 8 9-12h-7z',
            'M12 3c3 4 6 6 6 10a6 6 0 0 1-12 0c0-4 3-6 6-10Z',
            'M3 18h18M6 14h12M9 10h6',
            'M8 4c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4ZM16 10c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4Z',
            'M12 12m-9 0a9 9 0 1 0 18 0a9 9 0 1 0-18 0M12 7v5l3 2',
            'M4 8h14l-4-4M20 16H6l4 4',
        ],
    },
    29: {
        'core': '**Crusader Strike** and **Holy Shock** send a **Dawn Echo** a moment later: half of their damage or '
                'healing again.',
        'slots': ['Echo', 'Light', 'Grace'],
        'asks': ['What your echoes become', 'What light they leave', 'What grace they give'],
        'texts': [
            "Each Dawn Echo also strikes (or heals) up to 5 others within 8 yards for 40% of the echo's damage or "
            'healing.',
            "Dawn Echoes deal 40% more damage and healing: 70% of the strike's damage or the heal instead of 50%.",
            "Every Dawn Echo both strikes the enemy and heals your most injured ally, each for 35% of the original's "
            'damage or healing.',
            "Each Dawn Echo consecrates the ground for 4 sec, dealing 25% of the echo's damage every second to "
            'enemies within 8 yards.',
            'Holy Shock sends 2 Dawn Echoes; the second deals half the damage or healing.',
            'Judgement sends a Dawn Echo too.',
            'Your damaging Dawn Echoes heal you for 20% of their damage.',
            "Each Dawn Echo takes 0.5 sec off Crusader Strike's and Holy Shock's cooldowns.",
            'Your healing Dawn Echoes also shield their target for 25% of the heal for 8 sec.',
        ],
        'abilities': [
            'Splash',
            'Power',
            'Both',
            'Ground',
            'Holy Shock',
            'Judgement',
            'Self-heal',
            'Cooldowns',
            'Shield',
        ],
        'icons': [
            'M12 21s-7-4.5-7-10a4 4 0 0 1 7-2.6A4 4 0 0 1 19 11c0 5.5-7 10-7 10Z',
            'M13 2 4 14h7l-1 8 9-12h-7z',
            'M4 8h14l-4-4M20 16H6l4 4',
            'M4 12h10M10 8l4 4-4 4M18 5v14',
            'M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8ZM12 2v3M12 19v3M2 12h3M19 12h3',
            'M3 18h18M6 14h12M9 10h6',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
            'M4 12a8 8 0 0 1 14-5l2-2v6h-6l2-2a5 5 0 0 0-9 3',
            'M8 4c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4ZM16 10c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4Z',
        ],
    },
    30: {
        'core': '**Mind Flay** calls a **Hollow Echo** to your side; it follows you and flays your target. **Mind '
                'Blast** turns every echo on its target in a barrage.',
        'slots': ['Congregation', 'Sermon', 'Command'],
        'asks': ['How many echoes you keep', 'How they answer Mind Blast', 'What else they give'],
        'texts': [
            'You can have 4 Hollow Echoes at once instead of 3.',
            'Your Hollow Echoes last 15 sec instead of 10.',
            'When a Hollow Echo fades, it strikes its target for 3 Mind Flay ticks at once.',
            "Mind Blast's barrage lands all at once and deals 20% more damage: 72% of a Mind Flay tick's damage per "
            'bolt.',
            'Your echoes spread out over enemies with your Shadow Word: Pain on them, your target first.',
            'Instead of bolts, each echo screams through up to 5 enemies on the way to your target, dealing 90% of a '
            "Mind Flay tick's damage to each.",
            'Your Hollow Echoes deal 25% more damage while you channel Mind Flay.',
            'Your Hollow Echoes heal you for 10% of the damage they deal.',
            "Mind Blast's cooldown is 0.5 sec shorter for each echo it commands.",
        ],
        'abilities': [
            '4 echoes',
            '15 sec',
            'Fade',
            'Together',
            'Spread',
            'Line',
            'Fervor',
            'Self-heal',
            'Cooldown',
        ],
        'icons': [
            'M8 4c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4ZM16 10c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4Z',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M13 2 4 14h7l-1 8 9-12h-7z',
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M4 20 20 4',
            'M12 12m-9 0a9 9 0 1 0 18 0a9 9 0 1 0-18 0M12 7v5l3 2',
            'M4 12h10M10 8l4 4-4 4M18 5v14',
            'M8 4c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4ZM16 10c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4Z',
        ],
    },
    31: {
        'core': 'Your fire spells embed **Sunshards** in their target, up to 5. Your next **frost spell** on it '
                'shatters them all in a burst around it.',
        'slots': ['Formation', 'Shatter', 'Sunfall'],
        'asks': ['How you embed shards', 'How the burst spreads', 'What the burst adds'],
        'texts': [
            'Fireball and Pyroblast embed 2 Sunshards instead of 1.',
            'Fireball and Pyroblast also embed a Sunshard in up to 2 enemies within 8 yards of their target.',
            'An enemy can hold up to 8 Sunshards instead of 5.',
            'The burst hits enemies within 12 yards instead of 8.',
            "The burst leaves burning ground for 4 sec, dealing 15% of the burst's damage every second to enemies "
            'within 6 yards.',
            'The burst also shatters the Sunshards in other enemies within 10 yards, for half the damage.',
            "The burst deals 40% more damage to the frost spell's target.",
            '2 sec after the burst, it flares again for 40% of its damage.',
            'Enemies hit by the burst have their movement speed reduced by 40% for 3 sec.',
        ],
        'abilities': [
            'Wildfire',
            'Spread',
            '8 shards',
            '12 yards',
            'Ground',
            'Chain',
            'Implosion',
            'Pulse',
            'Slow',
        ],
        'icons': [
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0',
            'M3 18h18M6 14h12M9 10h6',
            'M12 20 4 6h16Z',
            'M4 20 20 4',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-3 2-5 3-7 1 2 2 3 3 3 0-3-1-5 0-7Z',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M4 12a8 8 0 0 1 14-5l2-2v6h-6l2-2a5 5 0 0 0-9 3',
            'M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8ZM12 2v3M12 19v3M2 12h3M19 12h3',
        ],
    },
    32: {
        'core': "**Eviscerate** calls a **phantom assassin**: 0.6 sec later it strikes for **200%** of Eviscerate's "
            'damage, and your Energy regenerates 100% faster for 4 sec.',
        'slots': ['Execution Style', 'Timing', 'Aftermath'],
        'asks': ['How the phantom strikes', 'When the phantom strikes', 'What follows its blow'],
        'texts': [
            "The phantom deals 40% more damage: 280% of Eviscerate's damage.",
            "The phantom strikes 3 times, 0.2 sec apart, for 230% of Eviscerate's damage in all.",
            "The phantom also strikes up to 3 other enemies within 8 yards for 100% of Eviscerate's damage each.",
            'The phantom strikes 0.1 sec after Eviscerate instead of 0.6.',
            "The phantom waits 2 sec, then strikes for 280% of Eviscerate's damage.",
            "The phantom leaves a bleed instead: 240% of Eviscerate's damage over 4 sec. Refreshed, never stacked.",
            'If its victim dies first, the phantom strikes the nearest enemy within 10 yards.',
            'When the phantom strikes, your movement speed rises by 30% for 3 sec.',
            'The phantom marks its victim for 10 sec: your next Sinister Strike, Backstab or Hemorrhage on it deals '
            '30% more.',
        ],
        'abilities': [
            'Execute',
            'Cuts',
            'Cleave',
            'Instant',
            '2 sec',
            'Bleed',
            'Follow',
            'Speed',
            'Mark',
        ],
        'icons': [
            'M4 20 20 4M14 4h6v6',
            'M5 19 9 15M9 19l4-4M13 19l4-4',
            'M4 20 15 9M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0',
            'M13 2 4 14h7l-1 8 9-12h-7Z',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 7v5l3 2',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M4 12h10M10 6l6 6-6 6M18 6v12',
            'M3 12h4M5 8h6M5 16h6M13 12h8',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-2 0a2 2 0 1 0 4 0a2 2 0 1 0-4 0',
        ],
    },
    33: {
        'core': "**Kill Command** unleashes a **Spectral Beast** in your pet's likeness. During **Bestial Wrath**, "
                'Kill Command recovers in 5 sec and a beast joins the hunt every second.',
        'slots': ['Spectral Beast', 'The Wrath', 'Final Attack'],
        'asks': ['How the beasts bite', 'How the pack hunts', 'How a beast says farewell'],
        'texts': [
            "Spectral Beasts bite for 78% of your pet's attack instead of 60%.",
            "Each Spectral Beast bite also hits up to 2 other enemies within 5 yards for 30% of your pet's attack.",
            "Each bite makes its target bleed for 15% of your pet's attack every second for 4 sec. Refreshed, never "
            'stacked.',
            'Each Kill Command during Bestial Wrath makes Bestial Wrath last 2 sec longer, up to 10 sec longer.',
            'Your Spectral Beasts switch to whatever enemy you target.',
            'When its prey dies, a Spectral Beast hunts on: your target, else the nearest enemy.',
            "As a Spectral Beast fades, it pounces for 120% of your pet's attack.",
            "As a Spectral Beast fades, it bites 4 times in a flurry, 36% of your pet's attack each.",
            'As a Spectral Beast fades, your pet deals 30% more damage for 6 sec.',
        ],
        'abilities': [
            'Alpha',
            'Ravage',
            'Bleed',
            'Wrath',
            'Command',
            'Scent',
            'Pounce',
            'Flurry',
            'Howl',
        ],
        'icons': [
            'M12 3l3 6 6 1-4 4 1 6-6-3-6 3 1-6-4-4 6-1Z',
            'M3 12h6M15 12h6M12 3v6M12 15v6',
            'M6 4l2 8-2 8M12 4l2 8-2 8M18 4l2 8-2 8',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M4 20 20 4M14 4h6v6',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M4 18 12 4l8 14',
            'M5 19 9 15M9 19l4-4M13 19l4-4',
            'M6 9h4l5-4v14l-5-4H6Z',
        ],
    },
    34: {
        'core': '**Arcane Blast** grants **Insight**. At 4, your next **Arcane Missiles** is free and fires 5 Insight '
            'missiles with it, **40%** of a missile each.',
        'slots': ['Invocation', 'Manifestation', 'Adaptation'],
        'asks': ['How Insight builds', 'How the volley strikes', 'How you cast it'],
        'texts': [
            'Every third Arcane Blast that hits grants 2 Insight instead of 1.',
            'Arcane Explosion grants Insight too: 2 if it hits 3 or more enemies, else 1. Once a cast.',
            'Insight stacks to 6 instead of 4. Spending 5 or 6 makes the Insight missiles deal 15% more damage for '
            'each one above 4.',
            "Insight missiles deal 40% more damage to their target: 56% of an Arcane Missile's damage.",
            'Each Insight missile also hits up to 2 other enemies within 8 yards of its target for 35% of its damage.',
            'The Insight missiles leave Arcane Echo on their target: 30% of their damage again, over 6 sec. A new '
            'Echo keeps what is left of the last.',
            'An empowered Arcane Missiles does not channel: its missiles fly from you on their own, one every 0.5 '
            'sec, so you can move and keep casting.',
            'When an empowered Arcane Missiles completes, you regain 3% of your maximum mana and Arcane Blast costs '
            '20% less mana for 4 sec.',
            'The Insight missiles all fly as the channel starts, 0.15 sec apart, and deal 20% more damage: 48% of an '
            "Arcane Missile's damage.",
        ],
        'abilities': [
            'Accelerate',
            'Expand',
            'Patience',
            'Focus',
            'Fracture',
            'Echo',
            'Unbound',
            'Reserves',
            'Overclock',
        ],
        'icons': [
            'M13 2 4 14h7l-1 8 9-12h-7Z',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12h.01',
            'M5 4h14M5 20h14M7 4c0 5 10 11 10 16M17 4c0 5-10 11-10 16',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M12 3v6M12 15v6M3 12h6M15 12h6M6 6l3 3M15 15l3 3',
            'M3 12h4l3-7 4 14 3-7h4',
            'M4 18l6-6-6-6M12 18h8',
            'M6 4h12v4l-4 4 4 4v4H6v-4l4-4-4-4Z',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
        ],
    },
    35: {
        'core': 'Each **Frostbolt** that hits forms an **Icicle** above you: 1 sec later it strikes the same enemy '
            "for **30%** of the Frostbolt's damage.",
        'slots': ['Formation', 'Flight', 'Aftermath'],
        'asks': ['What the Icicle is', 'Where it flies', 'What it leaves behind'],
        'texts': [
            "Icicles deal 40% more damage: 42% of the Frostbolt's damage.",
            "Each Icicle breaks into 3 shards, 0.2 sec apart, each 40% of an Icicle's damage (120% in all). The last "
            'one makes your next Frostbolt cast 10% faster.',
            'Each Icicle also hits up to 4 other enemies within 6 yards of its target for half its damage.',
            'Icicles and their shards pierce up to 3 enemies within 15 yards behind their target, for 40% of their '
            'damage.',
            'As an Icicle lands, a splinter flies on to another enemy within 10 yards for 30% of its damage; with '
            'none near, it strikes the same enemy for 15%.',
            'Every third Frostbolt that hits makes an Icicle that deals 60% more damage (its shards too).',
            "Every third Icicle attack wounds its target: 80% of an Icicle's damage over 6 sec. A shattered Icicle "
            'counts once.',
            'Icicles slow their targets by 25% for 3 sec, and deal 15% more damage to an enemy they slowed already.',
            'Every third Icicle attack makes your next Frostbolt within 5 sec instant. A shattered Icicle counts once.',
        ],
        'abilities': [
            'Spear',
            'Splinter',
            'Shatter',
            'Impale',
            'Seek',
            'Focus',
            'Wound',
            'Grasp',
            'Front',
        ],
        'icons': [
            'M4 20 20 4M14 4h6v6',
            'M6 18 18 6M4 12l4-4M12 20l4-4',
            'M12 2v20M2 12h20M5 5l14 14M19 5 5 19',
            'M2 12h20M16 7l5 5-5 5',
            'M4 18c4-10 12-10 16-12M16 4l4 2-2 4',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M4 12h16M8 8l-4 4 4 4M16 8l4 4-4 4',
            'M3 17c3-3 6-3 9 0s6 3 9 0M3 11c3-3 6-3 9 0s6 3 9 0',
        ],
    },
    36: {
        'core': '**Pyroblast** leaves an **Ember**; **Fire Blast** on it calls a **phoenix** diving from above you '
            "onto it: **50%** of the Pyroblast's damage.",
        'slots': ['Phoenix', 'Flight', 'Wake'],
        'asks': ['What wakes in the Ember', 'How it flies', 'What it leaves behind'],
        'texts': [
            "The phoenix strikes the Ember's target for 50% more: 75% of the Pyroblast's damage.",
            "The phoenix's path is 6 yards wide instead of 3, and enemies in it take 30% of the Pyroblast's damage "
            'instead of 25%.',
            'Three phoenixes dive instead, each with a third of the damage. Each one whose flight ends makes your '
            'next Pyroblast cast 5% faster, up to 15%, for 6 sec.',
            "The phoenix dives twice as fast, 40 yards a second. When its flight ends, Fire Blast's cooldown is 2 sec "
            'shorter.',
            "After its strike, the phoenix circles the Ember's target for 2 sec, then strikes it again for 25% of the "
            "Pyroblast's damage.",
            "After its strike, the phoenix swoops through up to 2 other enemies within 10 yards of the Ember's "
            "target, striking each for 25% of the Pyroblast's damage.",
            "The phoenix leaves a trail of fire for 4 sec: up to 5 enemies on it burn for 6% of the Pyroblast's "
            "damage every second. Trails don't stack.",
            'Enemies the phoenix strikes burn for 25% more of that hit over 6 sec. A new burn keeps what is left of '
            'the last.',
            "When the phoenix's flight ends, it bursts for 35% of the Pyroblast's damage to up to 5 enemies within 6 "
            'yards of it. A flock bursts once.',
        ],
        'abilities': [
            'Talon',
            'Broadwing',
            'Flock',
            'Homeward',
            'Spiral',
            'Wild',
            'Scorch',
            'Embers',
            'Rebirth',
        ],
        'icons': [
            'M4 4l6 6M10 4v6H4M20 20l-6-6M14 20v-6h6',
            'M2 12c5-6 15-6 20 0M2 12c5 6 15 6 20 0',
            'M4 18l4-6 4 6 4-6 4 6',
            'M4 12h14M14 7l5 5-5 5',
            'M12 12m-8 0a8 8 0 1 0 16 0M12 12m-4 0a4 4 0 1 0 8 0',
            'M4 18c4-10 6-10 8-4s4 6 8-6',
            'M3 20h18M6 20c0-5 3-6 3-10M12 20c0-6 3-7 3-12M18 20c0-4 2-5 2-8',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M12 2v4M12 18v4M2 12h4M18 12h4M5 5l3 3M16 16l3 3',
        ],
    },
    37: {
        'core': '**Arcane Barrage** sets an **Arcane Orb** loose (every 12 sec at most): it flies to your enemies and '
            "for 6 sec fires a bolt a second for **20%** of the Barrage's damage.",
        'slots': ['Orbit', 'Discharge', 'Ending'],
        'asks': ['Where the Orb goes', 'How it fires', 'How it ends'],
        'texts': [
            "Each enemy the Orb passes through takes two bolts' damage (40% of the Barrage's damage), once (up to 5).",
            "The Orb flies to its target's position and stays there. Its bolts deal 50% more damage to enemies within "
            '5 yards of it.',
            'The Orb circles you, 4 yards out, for 8 sec instead of 6, and its bolts reach 15 yards instead of 10.',
            "Bolts deal 60% more damage to the Orb's target: 32% of the Barrage's damage.",
            'Each bolt also strikes up to 2 other enemies in range for 75% of its damage.',
            'Each bolt becomes a lance that also pierces up to 3 enemies behind its target, for 80% of its damage.',
            "When the Orb's time ends, it explodes for 120% of the Barrage's damage (6 bolts) to up to 5 enemies "
            'within 8 yards.',
            "When the Orb's time ends, it fires 3 last bolts at your target (else its own), each for 60% of the "
            "Barrage's damage.",
            "When the Orb's time ends, it leaves a field 6 yards wide for 6 sec: every second, a bolt's damage to up "
            "to 5 enemies in it (120% of the Barrage's damage in all).",
        ],
        'abilities': [
            'Wander',
            'Horizon',
            'Satellite',
            'Focus',
            'Constellation',
            'Lance',
            'Supernova',
            'Dying',
            'Remnant',
        ],
        'icons': [
            'M4 18 20 6M14 6h6v6',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12h.01',
            'M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0M12 3a9 9 0 0 1 9 9',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M5 5h.01M19 5h.01M12 12h.01M5 19h.01M19 19h.01M5 5l7 7 7-7M5 19l7-7 7 7',
            'M2 12h20M16 7l5 5-5 5',
            'M12 2v4M12 18v4M2 12h4M18 12h4M5 5l3 3M16 16l3 3M19 5l-3 3M8 16l-3 3',
            'M12 4v12M8 12l4 4 4-4M6 20h12',
            'M3 17c3-3 6-3 9 0s6 3 9 0M12 4v8',
        ],
    },
    38: {
        'core': '**Prayer of Mending** places a **Guiding Star**: every 2 sec it heals its bearer for half a Prayer '
            'of Mending. **Flash Heal** or **Greater Heal** moves it.',
        'slots': ['Radiance', 'Guidance', 'Farewell'],
        'asks': ['How the star heals', 'How it travels', 'How it says farewell'],
        'texts': [
            'The star heals its bearer for 50% more: 75% of a Prayer of Mending every 2 sec.',
            'Each pulse also heals the most injured other ally within 10 yards of the bearer for 75% of it.',
            "Instead of healing on arrival, the star shields its new bearer for twice a pulse's healing, for 6 sec. A "
            'new shield replaces the last.',
            'As it travels, the star heals up to 3 injured allies along its way for half a pulse each.',
            'Only Greater Heal moves the star. Each pulse on the same bearer heals 10% more than the last, up to 30%; '
            'moving the star starts over.',
            'Every third pulse also heals the most injured ally within 15 yards of the bearer for a pulse. If the '
            'bearer dies, the star moves on to that ally.',
            "When the star's 12 sec end, it heals its bearer for 2.5 pulses at once.",
            "When the star's 12 sec end, it leaves Afterglow on its bearer: 3.25 pulses of healing over 8 sec.",
            "When the star's 12 sec end, it shields its bearer for 2 pulses (6 sec), and they move 30% faster for 3 "
            'sec.',
        ],
        'abilities': [
            'Steady',
            'Shared',
            'Shelter',
            'Passage',
            'Guardian',
            'Seeking',
            'Benediction',
            'Afterglow',
            'Grace',
        ],
        'icons': [
            'M12 2l2.5 7.5L22 12l-7.5 2.5L12 22l-2.5-7.5L2 12l7.5-2.5Z',
            'M8 12m-5 0a5 5 0 1 0 10 0a5 5 0 1 0-10 0M16 12m-5 0a5 5 0 1 0 10 0a5 5 0 1 0-10 0',
            'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6Z',
            'M4 18c4-10 12-10 16-12M16 4l4 2-2 4',
            'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6ZM12 8v8M8 12h8',
            'M11 11m-7 0a7 7 0 1 0 14 0a7 7 0 1 0-14 0M21 21l-5-5',
            'M12 4v16M4 12h16',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M4 12h14M14 7l5 5-5 5',
        ],
    },
    39: {
        'core': 'Each **Starfire** that hits calls a **lunar owl**: 1 sec later it dives on the same enemy for '
            "**30%** of the Starfire's damage.",
        'slots': ['Guardian', 'Descent', 'Blessing'],
        'asks': ['What the owl is', 'When it dives', 'What it leaves behind'],
        'texts': [
            "Owls deal 40% more damage to their target: 42% of the Starfire's damage.",
            'Each dive also hits up to 4 other enemies within 6 yards for half its damage.',
            'Each owl strikes 3 times, 1 sec apart, for 40% of its damage each (120% in all). The last strike makes '
            'your next Starfire cost 20% less mana.',
            'Owls dive 0.1 sec after Starfire instead of 1. Each dive makes your next Starfire cast 10% faster.',
            'Owls wait 3 sec, then dive for 30% more damage.',
            'If its target dies first, the owl dives on the nearest enemy within 15 yards instead. Each owl also '
            'strikes again 1 sec later for 20% of its damage.',
            "An owl's strike wounds its target for as much again over 6 sec. A new wound keeps what is left of the "
            'last.',
            'Every third owl makes your next Starfire within 5 sec instant, and you move 20% faster for 5 sec.',
            "An owl's target takes 25% more damage from your next Wrath within 5 sec.",
        ],
        'abilities': [
            'Pierce',
            'Wings',
            'Watch',
            'Swift',
            'Vigil',
            'Pursuit',
            'Wound',
            'Guide',
            'Hunt',
        ],
        'icons': [
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M2 12c5-6 15-6 20 0M2 12c5 6 15 6 20 0',
            'M5 19 9 15M9 19l4-4M13 19l4-4',
            'M13 2 4 14h7l-1 8 9-12h-7Z',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
            'M4 18c4-10 12-10 16-12M16 4l4 2-2 4',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M4 12h14M14 7l5 5-5 5',
            'M11 11m-7 0a7 7 0 1 0 14 0a7 7 0 1 0-14 0M21 21l-5-5',
        ],
    },
    40: {
        'core': '**Wrath** leaves **Sunlight**; **Starfire** on it collides for **40%** of its damage and leaves '
            '**Moonlight**; **Wrath** on that flares for **80%**.',
        'slots': ['Collision', 'Alignment', 'Flare'],
        'asks': ['How the energies collide', 'How the marks align', 'How the sequence ends'],
        'texts': [
            "The collision deals 40% more damage: 56% of the Starfire's damage.",
            'The collision also hits up to 4 other enemies within 6 yards for half its damage.',
            'Instead of a hit, the collision ruptures its target for 150% of its damage over 6 sec. A new rupture '
            'keeps what is left of the last.',
            'Your marks last 12 sec instead of 8. Each 2 sec Sunlight was on its target adds 5% to the collision, up '
            'to 20%.',
            'A collision also strikes up to 3 other enemies within 12 yards that carry your Moonlight, for 35% of its '
            'damage. Their marks stay.',
            'Each flare makes your next Wrath or Starfire within 5 sec instant.',
            "The flare deals 60% more damage: 128% of the Wrath's damage.",
            'Each flare makes your next Starfire within 5 sec cast 20% faster and cost 20% less mana.',
            "Each flare leaves a field 6 yards wide for 4 sec: every second, a quarter of the flare's damage to up to "
            '5 enemies in it.',
        ],
        'abilities': [
            'Align',
            'Skies',
            'Rupture',
            'Twilight',
            'Crossed',
            'Orbit',
            'Return',
            'Dominance',
            'Eventide',
        ],
        'icons': [
            'M12 2v4M12 18v4M2 12h4M18 12h4M5 5l3 3M16 16l3 3',
            'M5 5h.01M19 5h.01M12 12h.01M5 19h.01M19 19h.01M5 5l7 7 7-7M5 19l7-7 7 7',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
            'M2 12h20M16 7l5 5-5 5',
            'M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0M12 3a9 9 0 0 1 9 9',
            'M12 12m-4 0a4 4 0 1 0 8 0a4 4 0 1 0-8 0M12 2v3M12 19v3M2 12h3M19 12h3',
            'M12 3a9 9 0 1 0 9 9 7 7 0 0 1-9-9Z',
            'M3 17c3-3 6-3 9 0s6 3 9 0M12 4v8',
        ],
    },
    41: {
        'core': "**Barkskin** in Bear Form raises **Ursoc's Guard** (30% of your health); **Maul** releases it: "
            '**75%** of your attack power plus all it absorbed.',
        'slots': ['Guard', 'Answer', 'Resolve'],
        'asks': ['How the guard holds', 'How you answer', 'What it leaves behind'],
        'texts': [
            'The guard absorbs 45% of your maximum health instead of 30%.',
            'The guard lasts 10 sec instead of 6, and you receive 25% more healing meanwhile.',
            'You move 40% faster while the guard holds. Once you have moved 5 yards, it absorbs 10% of your maximum '
            'health more (once a guard).',
            "The blow strikes only Maul's target, for double damage.",
            'The blow strikes in a half circle in front of you, up to 10 enemies.',
            'The blow runs 25 yards ahead of you in a line (up to 5 enemies); those past 10 yards take 50% more.',
            'All the guard had left stays with you as a shield for 8 sec after the blow.',
            'Enemies struck deal 20% less damage for 6 sec, and the blow causes double threat.',
            'Maul releases 75% of your attack power and the guard stays up: up to 3 blows a guard.',
        ],
        'abilities': [
            'Hide',
            'Stand',
            'Mountain',
            'Bite',
            'Roar',
            'Break',
            'Endure',
            'Challenge',
            'Unbroken',
        ],
        'icons': [
            'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6Z',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
            'M4 12h14M14 7l5 5-5 5',
            'M4 4l6 6M10 4v6H4M20 20l-6-6M14 20v-6h6',
            'M2 12c5-6 15-6 20 0M2 12c5 6 15 6 20 0',
            'M3 20h18M6 20 12 4l6 16',
            'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6ZM12 8v8M8 12h8',
            'M6 9h4l5-4v14l-5-4H6Z',
            'M12 2v20M2 12h20',
        ],
    },
    42: {
        'core': '**Sabre Slash** readies **Opening Shot**: **Pistol Shot** for **60%** of your attack power; '
            '**Broadside** spends combo points in pistol blasts.',
        'slots': ['Sabre', 'Pistol', 'Footwork'],
        'asks': ['How the blade cuts', 'How the shot flies', 'What you do after'],
        'texts': [
            'Sabre Slash deals 25% more damage.',
            'Sabre Slash also hits up to 3 other enemies within 5 yards of its target for half its damage.',
            'Sabre Slash makes its target bleed for 40% more of its damage over 6 sec. A new bleed keeps what is left '
            'of the last.',
            'Pistol Shot deals 60% more damage: 96% of your attack power.',
            'Pistol Shot spreads in a cone: up to 4 other enemies in front of you, as far as 10 yards past its '
            'target, take 75% of its damage.',
            'Pistol Shot pierces up to 3 enemies within 15 yards behind its target, for 80% of its damage.',
            'After firing, you move 40% faster for 3 sec, and your next Sabre Slash on that enemy within 5 sec deals '
            '30% more damage.',
            'After firing, you move 40% faster for 3 sec, and your next Pistol Shot within 8 sec gives 10 Energy back.',
            'After firing, your next Sabre Slash within 5 sec strikes again a moment later for half its damage.',
        ],
        'abilities': [
            'Cut',
            'Sweep',
            'Serrate',
            'Ball',
            'Scatter',
            'Pierce',
            'Advance',
            'Disengage',
            'Press',
        ],
        'icons': [
            'M4 20 20 4M14 4h6v6',
            'M2 12c5-6 15-6 20 0M2 12c5 6 15 6 20 0',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M12 12m-5 0a5 5 0 1 0 10 0a5 5 0 1 0-10 0',
            'M4 12 20 4M4 12l16 0M4 12l16 8',
            'M2 12h20M16 7l5 5-5 5',
            'M4 12h14M14 7l5 5-5 5',
            'M20 12H6M10 7l-5 5 5 5',
            'M5 19 9 15M9 19l4-4M13 19l4-4',
        ],
    },
    43: {
        'core': '**Stormstrike** raises the **Stormfront** (25% of your attack power every 2 sec); **Lava Lash** '
            'becomes **Thunderlash**; your strikes discharge **Lightning Shield**.',
        'slots': ['Storm', 'Thunderlash', 'Shield'],
        'asks': ['How the storm strikes', 'How Thunderlash lands', 'How the shield fights'],
        'texts': [
            'The Stormfront deals 120% more damage while only one enemy is in it.',
            'The Stormfront strikes up to 8 enemies within 12 yards.',
            'The Stormfront strikes every second instead of every 2, each strike 70% of its damage.',
            "Thunderlash doesn't arc: 120% of your attack power to its target instead.",
            'Thunderlash arcs to 4 enemies instead of 2, each for 30% of your attack power.',
            'Thunderlash burns its target for 100% of your attack power over 6 sec instead of its Nature hit; its '
            'arcs stay. A new burn keeps what is left of the last.',
            "The rune's Lightning Shield discharges deal 50% more damage.",
            "Each of the rune's discharges makes you move 30% faster for 3 sec and brings Lava Lash back 1 sec sooner.",
            "Each of the rune's discharges also strikes up to 3 other enemies within 8 yards of its target for half "
            'its damage.',
        ],
        'abilities': [
            'Eye',
            'Forked',
            'Rolling',
            'Head',
            'Chain',
            'White-Hot',
            'Overcharge',
            'Ride',
            'Ground',
        ],
        'icons': [
            'M11 11m-7 0a7 7 0 1 0 14 0a7 7 0 1 0-14 0M11 11h.01',
            'M5 5l3 3M16 16l3 3M12 2v4M12 18v4',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
            'M13 2 4 14h7l-1 8 9-12h-7Z',
            'M5 5h.01M19 5h.01M12 12h.01M5 19h.01M19 19h.01M5 5l7 7 7-7M5 19l7-7 7 7',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6Z',
            'M4 12h14M14 7l5 5-5 5',
            'M3 20h18M12 4v12M8 12l4 4 4-4',
        ],
    },
    44: {
        'core': '**Fire Blast** in Cat Form ignites your **Rake** or **Rip** victim: **Cinder Wound**, 30% of your '
            'attack power plus 30% of your spell power over 6 sec.',
        'slots': ['Ignition', 'Claws', 'Instinct'],
        'asks': ['How the wound burns', 'What your claws add', 'What the hunt gives'],
        'texts': [
            'Cinder Wound deals 35% more damage.',
            'Each ignition also burns up to 4 other enemies within 6 yards of its victim for 10% of your attack power '
            'plus 10% of your spell power as Fire damage.',
            'Cinder Wound lasts 10 sec and deals 30% more damage over that time.',
            'Rake on your Cinder Wound victim also deals 15% of your attack power as Fire damage, once every 3 sec.',
            'Ferocious Bite on your Cinder Wound victim also deals 5% of your attack power per combo point spent as '
            'Fire damage.',
            'Swipe spreads Cinder Wound, at half its damage, to up to 3 bleeding enemies within its reach. Spread '
            "wounds don't spread again.",
            'Fire Blast makes you move 30% faster for 3 sec, and your next Rake or Mangle within 5 sec gives 10 '
            'Energy back.',
            "Cinder Wound's last tick deals twice its damage. A new wound keeps its tick timer running.",
            'Your first Fire Blast on each new enemy deals 20% more damage, once every 6 sec; that enemy dying resets '
            'the wait.',
        ],
        'abilities': [
            'Sear',
            'Embers',
            'Smolder',
            'Rake',
            'Bite',
            'Swipe',
            'Bound',
            'Patience',
            'Pursuit',
        ],
        'icons': [
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M12 12m-5 0a5 5 0 1 0 10 0a5 5 0 1 0-10 0M4 4l3 3M20 4l-3 3M4 20l3-3M20 20l-3-3',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
            'M5 19 9 15M9 19l4-4M13 19l4-4',
            'M4 14c4-6 12-6 16 0M8 14v3M12 14v4M16 14v3',
            'M2 12c5-6 15-6 20 0M2 12c5 6 15 6 20 0',
            'M4 12h14M14 7l5 5-5 5',
            'M12 3v9l5 5M12 21a9 9 0 1 0 0-18',
            'M2 12h20M16 7l5 5-5 5',
        ],
    },
    45: {
        'core': '**Icy Touch** in Cat Form lays **Frozen Prey**; your next **Shred** or **Mangle** shatters it for '
            '30% of your attack power as Frost damage.',
        'slots': ['Shatter', 'Hunt', 'Instinct'],
        'asks': ['How the prey shatters', 'How you mark the hunt', 'What the shatter leaves'],
        'texts': [
            'The shatter deals 40% more damage.',
            'The shatter also hits up to 4 enemies in an 8-yard cone in front of you for half its damage.',
            'Instead of its hit, the shatter leaves 150% of its damage as Frost damage over 6 sec. A new wound keeps '
            'what is left of the last.',
            'Frozen Prey lasts 10 sec; shattered after at least 3 sec, it deals 20% more damage.',
            'Laying Frozen Prey makes you move 30% faster for 3 sec; the Shred or Mangle that shatters it in that '
            'time gives 10 Energy back.',
            'Shattering lays a 60%-strength Frozen Prey on up to 2 other enemies within 8 yards for 6 sec. Those '
            "marks don't spread again.",
            'Shattering makes your next Ferocious Bite on that enemy within 5 sec deal 20% more damage.',
            'Shattering slows enemies within 6 yards by 30% for 3 sec and raises your dodge chance by 15% for 3 sec.',
            'Shattering leaves a 6-yard patch of frost for 4 sec: up to 5 enemies in it move 30% slower and take 5% '
            'of your attack power as Frost damage every second.',
        ],
        'abilities': [
            'Fang',
            'Spray',
            'Wound',
            'Winter',
            'Chase',
            'Pack',
            'Blood',
            'Snow',
            'Trail',
        ],
        'icons': [
            'M8 3l4 18 4-18',
            'M4 20 12 12M12 12l8-8M12 12l8 2M12 12l2 8',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
            'M4 12h14M14 7l5 5-5 5',
            'M6 12h.01M12 6h.01M18 12h.01M12 18h.01M12 12m-2 0a2 2 0 1 0 4 0a2 2 0 1 0-4 0',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11ZM12 10v5',
            'M12 2v20M4 6l16 12M20 6 4 18',
            'M5 19 9 15M9 19l4-4M13 19l4-4',
        ],
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
# Every slot name a built rune uses (the doc's three; the Sanctified Paragon's Presence and Covenant; the Stormbound
# Conduit's, Echoing Thunder's, the Wellspring's, the Wandering Mist's and the five after them), by name (the key its
# letters and digits only: the panel does the same).
for _name in dict.fromkeys(['Invocation', 'Manifestation', 'Adaptation']
                          + [name for built in BUILT.values() for name in built.get('slots', [])]):
    _key = ''.join(c for c in _name if c.isalnum())
    LABELS[f'Choose{_key}'] = {'text': 'Choose your ' + _name, 'font': '700 28px Cinzel', 'spacing': '3px',
                                'color': '#ffe2a8', 'glows': [['rgba(255,150,60,0.55)', 20]], 'shadow': '#000',
                                'shadowY': 2, 'height': 36, 'pad': 24, 'scale': 1.5}
    LABELS[f'Slot{_key}'] = {'text': _name.upper(), 'font': '400 13px "Alegreya Sans"', 'spacing': '3px',
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
        # A larger copy (icons/spells/large/, not in the client patch) for the card, when there is one.
        png = HERE / 'icons' / 'spells' / 'large' / f'{icon}.png'
        if not png.exists():
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
    # The rune button's ring (rune_art/rune_ring.png, the gold silhouette): 256 pixels a side, uncompressed (DXT's
    # blocks broke its thin gold rim up into pixels).
    with Image.open(HERE / 'rune_art' / 'rune_ring.png') as ring:
        (OUT / 'RuneRing.blp').write_bytes(blp.encode_raw(ring.convert('RGBA').resize((256, 256), Image.LANCZOS)))
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
            rune.update({'built': True, 'core': built['core'], 'asks': built['asks'],
                         'abilities': built['abilities'],
                         'texts': built['texts'],
                         'slots': built.get('slots', ['Invocation', 'Manifestation', 'Adaptation'])})
        art['runes'][entry['id']] = rune
    art['arcane'] = {'rgb': floats(ARCANE['rgb']), 'hot': floats(ARCANE['hot']), 'ink': floats(ARCANE['ink']),
                     'muted': floats(ARCANE['muted']),
                     'hue': floats(css(tuple(int(ARCANE['hue'][i:i + 2], 16) for i in (1, 3, 5)))),
                     'inner': floats(css(tuple(int(ARCANE['inner'][i:i + 2], 16) for i in (1, 3, 5))))}
    text = ('-- Generated by client-patch/build_rune_art.py: the rune panel\'s art (Art/), each rune\'s colours and the '
            'words of\n-- the runes the panel builds (the "Legendary Rune Panel" mockup\'s).\nEmberforgeRunesArt = '
            + lua(art).lstrip() + '\n')
    (ADDON / 'Art.lua').write_text(text)
    print('  Art.lua')


if __name__ == '__main__':
    main()
