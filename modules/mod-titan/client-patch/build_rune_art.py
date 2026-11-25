#!/usr/bin/env python3
"""Build the Emberforge_Runes addon's Art.lua (each rune's colours and words) and the pieces of the first design's art
still drawn (KEPT: the Soft glow; the rune button's ring), from the "Legendary Rune Panel" mockup. The redesigned
panel's own pieces are build_rune_ui.py's (Art/Ui*.tga); the bake below still paints the first design's cards,
medallions and labels, but only KEPT is written.

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
     'Your Fire Blast, in Cat Form too, ignites your bleeds into Cinder Wounds.',
     'Cat Form, Fire Blast, Rake or Rip',
     'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11ZM9 15l2-3 2 3'),
    (45, (140, 210, 245), 'Frost · Whitefang',
     'Your Icy Touch, in Cat Form too, marks the prey; Shred or Mangle shatters it.',
     'Cat Form, Icy Touch, Shred or Mangle',
     'M12 2v20M4 6l16 12M20 6 4 18M9 3l3 3 3-3M9 21l3-3 3 3'),
    (46, (225, 60, 70), 'Shadow · Red Communion',
     'Blood Boil links the pack to you; Blood Strike and Sanguine Recall take their blood back.',
     'Blood Boil and Blood Strike',
     'M12 3c3 5 6 8 6 11a6 6 0 0 1-12 0c0-3 3-6 6-11ZM4 20h16'),
    (47, (215, 165, 75), 'Nature · Primal Worldwarden',
     'A Worldheart Totem follows you; your Shocks command it, Stormstrike echoes them.',
     'Earth, Flame or Frost Shock',
     'M8 21h8M9 21V5h6v16M7 5h10M10 9h4M10 13h4'),
    (48, (170, 110, 240), 'Shadowflame · Black Covenant',
     'Your Voidwalker becomes Covenant Armor; Covenant Lash and Soul Siphon hold the line.',
     'Summon Voidwalker and Searing Pain',
     'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6ZM12 8v8M8 12h8'),
    (49, (150, 90, 220), 'Shadow · Hungering Rift',
     'Shadow Bolt on your Corruption opens a Rift that feeds on your damage over time.',
     'Shadow Bolt and Corruption',
     'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18ZM12 7a5 5 0 1 0 0 10 5 5 0 0 0 0-10ZM12 11v2'),
    (50, (130, 200, 255), 'Frost · Howling North',
     'Howling Blast marks the pack; Obliterate and Frost Strike loose Frost blades.',
     'Howling Blast, Obliterate and Frost Strike',
     'M3 12h12M3 7h8M3 17h8M15 12l6-6M15 12l6 6'),
    (51, (240, 200, 110), 'Marksmanship · Trueshot',
     'Aimed Shot becomes Trueshot: wind up, and pierce the line.',
     'Aimed Shot',
     'M3 12h18M17 8l4 4-4 4M8 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0'),
    (52, (225, 70, 80), 'Holy · Scarlet Benediction',
     'Renew plants the Bloom; your direct heals open it onto the injured.',
     'Renew, Flash Heal and Greater Heal',
     'M12 20v-6M12 14c-4 0-6-3-6-7 3 0 6 2 6 5 0-3 3-5 6-5 0 4-2 7-6 7z'),
    (53, (110, 220, 120), 'Restoration · Evergrove',
     'Rejuvenation stacks three deep, and finds the injured on its own.',
     'Rejuvenation, Swiftmend and Wild Growth',
     'M12 21V9M12 13l-5-4M12 11l5-5M7 9a3 3 0 1 1 0-.1M17 6a3 3 0 1 1 0-.1M12 9a3 3 0 1 1 0-.1'),
    (54, (170, 180, 200), 'Protection · Iron Reckoning',
     'Shield Block takes the blows; Shield Slam gives them back.',
     'Shield Block and Shield Slam',
     'M12 3l7 3v6c0 5-3 8-7 9-4-1-7-4-7-9V6zM8 12h8M12 8v8'),
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
            'The aura shrinks to 4 yards, but moving leaves sanctified footsteps for 2 sec that tick like the aura. An '
            'enemy is hit once a tick, however many it stands in.',
            "Standing still for 3 sec raises the aura's damage, healing and shields by 10% until you move.",
            'After Charge, the aura reaches 4 yards farther for 4 sec and its next tick is 15% stronger, once every 12 '
            'sec.',
            'Divine Shield while the aura is on makes its next 3 seconds cost no mana, once every 60 sec.',
            'Damage: each tick on an enemy raises its damage taken from your aura by 2%, up to 10%. Lost 2 sec after '
            'it leaves the aura.',
            'Healing: the aura no longer deals damage. Each second it heals your most injured party member in range.',
            "Protection: the aura no longer deals damage. Every 5 sec it shields a nearby party member for 5 ticks' "
            "worth. One shield at a time.",
            "Damage: Judgment's bonus in the aura is a whole tick instead of half. The aura keeps dealing damage.",
            'Healing: the aura no longer deals damage; each second it heals your most injured party member in range '
            'for 45% of a tick, and your Flash Heal on that ally heals 30% more, once every 8 sec.',
            "The aura's damage generates 50% more threat. Needs a damaging Manifestation (Sanctified Condemnation "
            'or Radiant Judgment).',
            'An enemy hit by 3 ticks in a row has its movement speed reduced by 15% while it stays in the aura. Needs '
            'a damaging Manifestation (Sanctified Condemnation or Radiant Judgment).',
            'The aura costs 0.75% of your base mana a second instead of 1%, but its damage, healing and shields are '
            '10% lower.',
            "Holy Shock's second heal also shields its ally for 20% of that heal for 4 sec, once every 8 sec.",
            "Shield of Righteousness's shield in the aura is 4% of your maximum health instead of 3%.",
        ],
        'abilities': [
            'Radius',
            'Footsteps',
            'Standing still',
            'Charge',
            'Divine Shield',
            'Condemnation',
            'Healing',
            'Shields',
            'Judgment',
            'Flash Heal',
            'Threat',
            'Slow',
            'Mana',
            'Holy Shock',
            'Shield of Righteousness',
        ],
        'icons': [
            'M12 12m-9 0a9 9 0 1 0 18 0a9 9 0 1 0-18 0M12 12m-4 0a4 4 0 1 0 8 0a4 4 0 1 0-8 0',
            'M8 20c-2 0-3-2-2-4l2-4M16 12c2 0 3-2 2-4l-2-4M9 9h.5M15 15h.5',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6ZM9 12h6',
            'M12 12m-9 0a9 9 0 1 0 18 0a9 9 0 1 0-18 0M12 12m-4 0a4 4 0 1 0 8 0a4 4 0 1 0-8 0',
            'M12 12m-9 0a9 9 0 1 0 18 0a9 9 0 1 0-18 0M12 12m-4 0a4 4 0 1 0 8 0a4 4 0 1 0-8 0',
            'M12 2v20M8 6h8M6 12h12',
            'M12 21s-7-4.5-7-10a4 4 0 0 1 7-2.6A4 4 0 0 1 19 11c0 5.5-7 10-7 10ZM12 9v6M9 12h6',
            'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18ZM12 7v10M7 12h10',
            'M12 2v20M8 6h8M6 12h12',
            'M12 2v20M8 6h8M6 12h12',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
            'M4 12h10M10 8l4 4-4 4M18 5v14',
            'M12 3c3 4 6 6 6 10a6 6 0 0 1-12 0c0-4 3-6 6-10Z',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
        ],
    },
    23: {
        'core': '**Lightning Bolt** leaves **Conductive**; **Chain Lightning** on a Conductive enemy arcs 30% of that '
                'bolt to another.',
        'slots': ['Conductor', 'Circuit', 'Shield'],
        'asks': ['What carries the charge', 'How the current arcs', 'What the shield adds'],
        'texts': [
            'Conductive lasts 12 sec, and the first Chain Lightning arc on each one deals 15% more damage.',
            'Lightning Bolt also leaves Conductive on one enemy within 10 yards of its target.',
            'Lightning Shield retaliations deal 15% more damage and leave Conductive on the attacker. Not with Storm '
            'Within.',
            'Frostbolt leaves Conductive too, and deals 15% more to a Conductive enemy.',
            'Lightning Bolt on your Flame Shock target also leaves Conductive on one enemy within 10 yards and makes '
            'Flame Shock last 2 sec longer (up to 6 sec for each cast).',
            "Chain Lightning's arc strikes 2 other enemies for 65% each.",
            "Chain Lightning's arc returns into the Conductive enemy for its whole damage.",
            'Arcs reach 15 yards, and the first Chain Lightning arc every 8 sec deals 20% more damage.',
            'Lava Lash on a Conductive enemy arcs 25% of your attack power as Nature damage to another enemy (half '
            'back into it if alone), once every 6 sec.',
            'Starfire on a Conductive enemy arcs 25% of your spell power as Arcane damage to another enemy (half back '
            'into it if alone), once every 8 sec.',
            'Every 3 sec in combat, your Lightning Shield strikes the nearest Conductive enemy within 8 yards, '
            'spending a charge. Not with Live Wire.',
            "Your Lightning Shield's arcs deal 40% more damage.",
            'Lightning Bolt also strikes its target with your Lightning Shield, spending a charge, once every 3 sec.',
            'Chain Heal makes your next Lightning Bolt within 6 sec deal 15% more damage.',
            'Thunder Clap on a Conductive enemy deals 20% of your spell power as Nature damage to up to 5 enemies '
            'within 8 yards of it, once every 8 sec.',
        ],
        'abilities': [
            '12 sec',
            '2 enemies',
            'Lightning Shield',
            'Frostbolt',
            'Flame Shock',
            '2 arcs',
            'Back in',
            '15 yards',
            'Lava Lash',
            'Starfire',
            'Lightning Shield',
            'Lightning Shield',
            'Lightning Shield',
            'Chain Heal',
            'Thunder Clap',
        ],
        'icons': [
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18Z',
            'M12 3v6M5 21l7-12 7 12M3 12h4M17 12h4',
            'M4 12c3-6 5-6 8 0s5 6 8 0',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18Z',
            'M12 3v6M5 21l7-12 7 12M3 12h4M17 12h4',
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 4v4l3-2',
            'M12 12m-9 0a9 9 0 1 0 18 0a9 9 0 1 0-18 0M12 12m-4 0a4 4 0 1 0 8 0a4 4 0 1 0-8 0',
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 4v4l3-2',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6ZM12 8v4M12 15v.5',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6ZM13 7l-4 6h4l-2 5',
            'M13 2 4 14h7l-1 8 9-12h-7z',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6ZM12 8v4M12 15v.5',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6ZM13 7l-4 6h4l-2 5',
        ],
    },
    24: {
        'core': '**Thunder Clap** lays a **Thunderfield**; standing in it, **Shield Slam** releases a **Thunder Echo** '
                'in a cone toward its target.',
        'slots': ['Field', 'Echo', 'Resonance'],
        'asks': ['Where the field stands', 'How the echo strikes', 'What the thunder leaves'],
        'texts': [
            'Your Thunderfield lasts 9 sec, and its first echo deals 15% more damage.',
            'Your Thunderfield goes with you, and the echo deals 10% more damage.',
            'Thunder Clap lays the field under your enemy within 10 yards instead, and the echo deals 20% more damage.',
            "Your Thunderfield goes with you; with Lightning Shield up, the echo also deals 20% of your spell power as "
            "Nature damage to the slam's target.",
            'Standing in your Consecration too, the echo deals 20% more damage and 30% more threat.',
            "The echo bursts about the slam's target instead: up to 5 enemies within 6 yards of it.",
            "The echo strikes only the slam's target, for 60% more damage.",
            "The echo's cone reaches 15 yards, and enemies beyond 10 yards take 30% more.",
            "The echo on your Flame Shock target burns it for 30% of the echo's damage over 6 sec.",
            'The echo on your Frost Fever target also deals 20% of your attack power as Frost damage and slows it by '
            '25% for 3 sec.',
            "The echo causes twice the threat, and deals 10% more to the slam's target.",
            'The echo shields you for 3% of your maximum health for each enemy it hits (15% at most) for 6 sec.',
            'The echo slows its enemies by 30% for 3 sec, and deals 20% more to enemies slowed by your Thunder Clap.',
            "Revenge's echo deals 40% more instead of 20%, and gives you 10 Rage, once every 8 sec.",
            'An echo while your Earth Shield is on you shields you for 30% of its last heal for 4 sec, once every 8 '
            'sec.',
        ],
        'abilities': [
            '9 sec',
            'Follows you',
            'Under the enemy',
            'Lightning Shield',
            'Consecration',
            'Burst',
            'One target',
            '15 yards',
            'Flame Shock',
            'Icy Touch',
            'Threat',
            'Shield',
            'Slow',
            'Revenge',
            'Earth Shield',
        ],
        'icons': [
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18Z',
            'M5 19 19 5M9 5h10v10',
            'M4 12h10M10 8l4 4-4 4M18 5v14',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18Z',
            'M5 19 19 5M9 5h10v10',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M12 2v4M12 18v4M2 12h4M18 12h4M12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6Z',
            'M3 12h14M13 7l5 5-5 5M21 7v10',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M12 2v4M12 18v4M2 12h4M18 12h4M12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6Z',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6ZM9 12h6',
            'M4 12c3-6 5-6 8 0s5 6 8 0',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6ZM9 12h6',
        ],
    },
    25: {
        'core': '**Chain Heal** on your **Earth Shield** bearer lays a **Wellspring**; your next Chain Heal there '
                'releases 40% of it to another ally.',
        'slots': ['Spring', 'Current', 'Undertow'],
        'asks': ['How the spring holds', 'Where the current runs', 'What the undertow leaves'],
        'texts': [
            'When Earth Shield renews the Wellspring, it also heals the most injured ally within 8 yards of the bearer '
            'for 20% of your healing power, once every 6 sec.',
            'The Wellspring lasts 12 sec and its release heals 10% more.',
            'The release reaches 18 yards, and its ally also heals for 10% of your healing power over 6 sec.',
            'Riptide on the bearer renews the Wellspring and heals another injured ally within 10 yards for 25% of '
            'your healing power, once every 8 sec.',
            'Renew on the bearer renews the Wellspring and makes its next release heal 20% more.',
            'The release heals 40% more on an ally below half health, 10% more otherwise.',
            'The release heals 2 injured allies for 65% each (one alone gets it all).',
            'The release heals one ally for 30% more.',
            'The release heals an ally with your Riptide 30% more and makes it last 2 sec longer (up to 6 sec for each '
            'cast).',
            'Holy Shock healing the bearer makes the next release within 8 sec also heal one more ally for 40% as '
            'much.',
            'The release also shields its ally for 25% of its healing for 4 sec.',
            'The release reaches 20 yards and heals 10% more.',
            'The release also heals its ally for 25% of its healing over 4 sec.',
            "If your Soothing Mist is on the release's ally, it also heals them for 25% of a Soothing Mist tick.",
            "If your Power Word: Shield is on the release's ally, it also shields them for 15% of its healing for 4 "
            "sec.",
        ],
        'abilities': [
            'Earth Shield',
            '12 sec',
            '18 yards',
            'Riptide',
            'Renew',
            'Low health',
            '2 allies',
            'One ally',
            'Riptide',
            'Holy Shock',
            'Shield',
            '20 yards',
            'Over time',
            'Soothing Mist',
            'Power Word: Shield',
        ],
        'icons': [
            'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18ZM12 7v10M7 12h10',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18Z',
            'M5 19 19 5M9 5h10v10',
            'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18ZM12 7v10M7 12h10',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18Z',
            'M12 21s-7-4.5-7-10a4 4 0 0 1 7-2.6A4 4 0 0 1 19 11c0 5.5-7 10-7 10ZM12 9v6M9 12h6',
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M12 2v4M12 18v4M2 12h4M18 12h4M12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6Z',
            'M12 21s-7-4.5-7-10a4 4 0 0 1 7-2.6A4 4 0 0 1 19 11c0 5.5-7 10-7 10ZM12 9v6M9 12h6',
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
            'M4 12h10M10 8l4 4-4 4M18 5v14',
            'M12 3c3 4 6 6 6 10a6 6 0 0 1-12 0c0-4 3-6 6-10Z',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
            'M4 12h10M10 8l4 4-4 4M18 5v14',
        ],
    },
    26: {
        'core': 'Each **Soothing Mist** tick also sends a **Wandering Mist** to your most injured ally nearby, for '
                '25% of the tick.',
        'slots': ['Mistflow', 'Technique', 'Approach'],
        'asks': ['Where your mist wanders', 'How your techniques share', 'How you sustain it'],
        'texts': [
            'Wandering Mist reaches allies up to 30 yards from your Soothing Mist target instead of 20.',
            "Each Soothing Mist tick sends Wandering Mist to 2 allies, healing 15% of the tick's healing each, instead "
            "of 25% to one.",
            'Wandering Mist heals allies below half health for 25% more.',
            'Wandering Mist on your Renew target makes it last 2 sec longer (up to 6 sec for each cast), once every 6 '
            'sec.',
            'Wandering Mist on your Riptide target also heals another injured ally within 8 yards for 40% of it, once '
            'every 6 sec.',
            'Enveloping Mist also wraps your most injured nearby ally in a Lesser Enveloping Mist that heals 30% as '
            'much.',
            'Vivify also heals your most injured nearby ally for 30% of its heal.',
            'Soothing Mist heals 15% more on a target with your Enveloping Mist.',
            'Holy Shock healing your Soothing Mist target also heals your most injured nearby ally for 25% of it, once '
            'every 8 sec.',
            'Chain Heal on your Soothing Mist target also sends a Wandering Mist to your most injured nearby ally for '
            '30% of its first heal, once every 8 sec.',
            'Soothing Mist costs 25% less mana.',
            'Moving your Soothing Mist to another ally leaves 2 more ticks over 4 sec on the last one, once every 12 '
            'sec.',
            'Wandering Mist also shields its ally for 30% of its healing for 4 sec, once every 8 sec.',
            'The third tick of each Soothing Mist makes your Renew on its target last 2 sec longer.',
            "Wandering Mist on your Earth Shield's bearer also shields them for 25% of its healing for 4 sec, once "
            "every 8 sec.",
        ],
        'abilities': [
            '30 yards',
            '2 allies',
            'Below half',
            'Renew',
            'Riptide',
            'Enveloping Mist',
            'Vivify',
            'Enveloping Mist',
            'Holy Shock',
            'Chain Heal',
            'Mana',
            'Rolling',
            'Shield',
            'Renew',
            'Earth Shield',
        ],
        'icons': [
            'M5 19 19 5M9 5h10v10',
            'M12 21s-7-4.5-7-10a4 4 0 0 1 7-2.6A4 4 0 0 1 19 11c0 5.5-7 10-7 10Z',
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M5 19 19 5M9 5h10v10',
            'M5 19 19 5M9 5h10v10',
            'M4 9c3-3 6-3 8 0s5 3 8 0M4 15c3-3 6-3 8 0s5 3 8 0',
            'M13 2 4 14h7l-1 8 9-12h-7z',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M4 9c3-3 6-3 8 0s5 3 8 0M4 15c3-3 6-3 8 0s5 3 8 0',
            'M4 9c3-3 6-3 8 0s5 3 8 0M4 15c3-3 6-3 8 0s5 3 8 0',
            'M4 12h10M10 8l4 4-4 4M18 5v14',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
            'M12 3c3 4 6 6 6 10a6 6 0 0 1-12 0c0-4 3-6 6-10Z',
            'M4 12h10M10 8l4 4-4 4M18 5v14',
            'M4 12h10M10 8l4 4-4 4M18 5v14',
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
        'core': "**Dawn Vow** takes Beacon of Light's place; **Crusader Strike** on your **Judgment** target, or "
                "**Holy Shock**, sends a **Dawn Echo** from your vowed ally.",
        'slots': ['Doctrine', 'Dawn', 'Pilgrimage'],
        'asks': ['What the echo does', 'How the dawn spreads', 'What the vow carries'],
        'texts': [
            'The echo heals the most injured ally within 10 yards of your vow for 50% of your power.',
            'The echo strikes your marked enemy within 20 yards of your vow for 45% of your power as Holy damage.',
            'Echoes take turns: a heal for 35% of your power, then a strike for 30%.',
            'While your Earth Shield is on your vow, the echo heals for 40% of your power and shields your vow for 10% '
            'for 4 sec.',
            'The echo strikes your marked enemy for 35% of your power as Holy damage, and 15% as Fire if your Flame '
            'Shock is on it.',
            'The echo travels from your vow toward your marked enemy, touching up to 3 on the way for half each.',
            'The echo also touches up to 2 more nearby for 40% each.',
            'The echo leaves holy ground 4 yards wide at your vow for 4 sec: each second it heals one injured ally in '
            'it for 10% of your power.',
            'Frostbolt on your marked enemy makes your next striking echo within 6 sec also deal 20% of your power as '
            'Frost damage.',
            'Chain Heal on your vow makes your next healing echo within 6 sec also heal one more ally for 40%.',
            'Holy Shock leaves an afterimage on your vow for 6 sec: its next echo strikes again at 30%.',
            'Moving your Dawn Vow to another ally heals them for 30% of your power.',
            'Once you or your vow move 8 yards, the next echo touches one more for 40%.',
            'An echo while your Renew is on your vow makes it last 2 sec longer (up to 6 sec for each cast), once '
            'every 6 sec.',
            'Lava Lash on your marked enemy sends an echo too (still once every 3 sec).',
        ],
        'abilities': [
            'Heal',
            'Strike',
            'Both',
            'Earth Shield',
            'Flame Shock',
            'Path',
            '2 more',
            'Ground',
            'Frostbolt',
            'Chain Heal',
            'Holy Shock',
            'Vow',
            '8 yards',
            'Renew',
            'Lava Lash',
        ],
        'icons': [
            'M12 21s-7-4.5-7-10a4 4 0 0 1 7-2.6A4 4 0 0 1 19 11c0 5.5-7 10-7 10Z',
            'M13 2 4 14h7l-1 8 9-12h-7z',
            'M4 8h14l-4-4M20 16H6l4 4',
            'M12 21s-7-4.5-7-10a4 4 0 0 1 7-2.6A4 4 0 0 1 19 11c0 5.5-7 10-7 10Z',
            'M13 2 4 14h7l-1 8 9-12h-7z',
            'M4 12h10M10 8l4 4-4 4M18 5v14',
            'M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8ZM12 2v3M12 19v3M2 12h3M19 12h3',
            'M3 18h18M6 14h12M9 10h6',
            'M4 12h10M10 8l4 4-4 4M18 5v14',
            'M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8ZM12 2v3M12 19v3M2 12h3M19 12h3',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
            'M4 12a8 8 0 0 1 14-5l2-2v6h-6l2-2a5 5 0 0 0-9 3',
            'M8 4c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4ZM16 10c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4Z',
            'M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6Z',
            'M4 12a8 8 0 0 1 14-5l2-2v6h-6l2-2a5 5 0 0 0-9 3',
        ],
    },
    30: {
        'core': '**Mind Flay** calls a **Hollow Echo** to your side; it flays your target, stronger for each of your '
            'damage over time effects there. **Mind Blast** turns every echo on its target in a barrage.',
        'slots': ['Congregation', 'Sermon', 'Command'],
        'asks': ['Where your echoes stand', 'How they answer Mind Blast', 'What else they give'],
        'texts': [
            'Your echoes follow you, and their Mind Flay deals 15% more damage.',
            'Your echoes stay where they were called, reach 10 yards farther (50 yards), and their Mind Flay deals 20% '
            'more damage.',
            'Your echoes follow their target, and their Mind Flay deals 20% more damage. One echo for each enemy: a '
            'new one on the same enemy replaces it.',
            'Barrage bolts deal 20% more damage to your Corruption target.',
            "An echo's Mind Flay on your Moonfire target also deals 10% of your spell power as Arcane damage, once "
            "every 2 sec for each echo.",
            "Mind Blast's barrage lands all at once and deals 20% more damage: 72% of a Mind Flay tick's damage per "
            'bolt.',
            'Your echoes spread out over enemies with your Shadow Word: Pain on them, your target first.',
            'Instead of bolts, each echo screams through up to 5 enemies on the way to your target, dealing 90% of a '
            "Mind Flay tick's damage to each.",
            'A Shadow Bolt hit turns one echo on its target with a Shadow hit of 30% of your spell power, once every 8 '
            'sec. No barrage.',
            "A Frostbolt hit on an echo's target makes that echo's next barrage also strike one other enemy within 8 "
            "yards with each bolt, for half.",
            'When a Hollow Echo fades, it strikes its target for 3 Mind Flay ticks at once.',
            'Mind Blast moves your echoes beside its target, and the first bolt of each barrage deals 30% more damage.',
            "While you move, your echoes' Mind Flay deals 25% more damage.",
            "While you channel Drain Soul on an echo's target, that echo's Mind Flay deals 20% more damage.",
            'A barrage completed while your Renew is on you heals you for 30% of your spell power, once every 8 sec.',
        ],
        'abilities': [
            'Follow',
            'Haunt',
            'Possess',
            'Corruption',
            'Moonfire',
            'Together',
            'Spread',
            'Line',
            'Shadow Bolt',
            'Frostbolt',
            'Fade',
            'Procession',
            'Witness',
            'Drain Soul',
            'Renew',
        ],
        'icons': [
            'M8 4c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4ZM16 10c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4Z',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M13 2 4 14h7l-1 8 9-12h-7z',
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M13 2 4 14h7l-1 8 9-12h-7z',
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M4 20 20 4',
            'M8 4c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4ZM16 10c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4Z',
            'M8 4c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4ZM16 10c1.5 0 2 2 2 4s-1 3-2 3-2-1-2-3 .5-4 2-4Z',
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M13 2 4 14h7l-1 8 9-12h-7z',
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M4 20 20 4',
        ],
    },
    31: {
        'core': "**Fireball** embeds a **Sunshard**; **Frostbolt** fractures it and breaks its host's core; your next "
                "Fireball there collapses it.",
        'slots': ['Sunshard', 'Fracture', 'Collapse'],
        'asks': ['Where the shards lodge', 'How the fracture breaks', 'What the collapse brings'],
        'texts': [
            'Up to 3 enemies hold a Sunshard, and fractures deal 15% more to their host.',
            "A fracture also strikes your selected enemy within 15 yards for 40% of the host's damage.",
            'A Sunshard stays where its host stood for 10 sec: Frostbolt on any enemy within 6 yards of it fractures '
            'it, for 20% more to the host.',
            'Fireball on your Moonfire target embeds a shard whose fracture also deals 25% of your spell power as '
            'Arcane damage to its host.',
            'Lightning Bolt embeds a Sunshard too, once every 8 sec; its fracture also deals 20% of your spell power '
            'as Nature damage to its host.',
            "A fracture bursts in a 12-yard cone away from you instead: up to 5 enemies, the others taking half the "
            "host's damage.",
            "A fracture also pierces up to 3 enemies behind its host within 15 yards for 60% of the host's damage.",
            'A fracture leaves scalding ground 6 yards wide for 4 sec: 10% of your spell power as Frostfire damage '
            'each second to up to 5 enemies.',
            'Ice Lance fractures a Sunshard too, once every 8 sec.',
            'A fracture on your Flame Shock target spreads it to one enemy within 8 yards, once every 8 sec.',
            'The collapse deals 50% more damage.',
            'The collapse fractures one other Sunshard within 8 yards, at half damage.',
            '1 sec after the collapse, it strikes again for half: its host and up to 2 enemies within 6 yards.',
            'Ice Lance on a Broken Core deals 35% of your spell power, and your next Fireball within 6 sec casts 15% '
            'faster.',
            'A collapse while your Ice Barrier is up shields you for 40% of your spell power for 6 sec, once every 12 '
            'sec.',
        ],
        'abilities': [
            '3 shards',
            'Your target',
            'Ground',
            'Moonfire',
            'Lightning Bolt',
            'Cone',
            'Pierce',
            'Burning ground',
            'Ice Lance',
            'Flame Shock',
            'Host',
            'Chain',
            'Again',
            'Ice Lance',
            'Ice Barrier',
        ],
        'icons': [
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0',
            'M3 18h18M6 14h12M9 10h6',
            'M12 3v8M12 11 5 21M12 11l7 10',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0',
            'M12 20 4 6h16Z',
            'M4 20 20 4',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-3 2-5 3-7 1 2 2 3 3 3 0-3-1-5 0-7Z',
            'M12 20 4 6h16Z',
            'M4 20 20 4',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M4 12a8 8 0 0 1 14-5l2-2v6h-6l2-2a5 5 0 0 0-9 3',
            'M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8ZM12 2v3M12 19v3M2 12h3M19 12h3',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M4 12a8 8 0 0 1 14-5l2-2v6h-6l2-2a5 5 0 0 0-9 3',
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
            'The phantom on your Frost Fever target also deals 20% of your attack power as Frost damage.',
            'The phantom on your Flame Shock target also deals 20% of your spell power as Fire damage.',
            'The phantom strikes 0.1 sec after Eviscerate instead of 0.6, and your next Sinister Strike, Backstab, '
            'Hemorrhage or Ghostly Strike within 6 sec costs 10 less Energy.',
            "The phantom waits 2 sec, then strikes for 280% of Eviscerate's damage.",
            "The phantom leaves a bleed instead: 240% of Eviscerate's damage over 4 sec. Refreshed, never stacked.",
            "The phantom on your Hunter's Mark target also strikes one other enemy within 8 yards for half its damage.",
            'The phantom on your Corruption target leaves 25% of its damage as Shadow damage over 6 sec.',
            'If its victim dies first, the phantom strikes the nearest enemy within 10 yards.',
            'When the phantom strikes, your movement speed rises by 30% for 3 sec.',
            "Your Backstab's mark makes the phantom deal 40% more damage instead of 25%.",
            "The phantom's strike while your Renew is on you heals you for 30% of your spell power, once every 10 sec.",
            "The phantom's strike on your Rend target makes your next Sinister Strike within 6 sec deal 20% more "
            "damage.",
        ],
        'abilities': [
            'Execute',
            'Cuts',
            'Cleave',
            'Icy Touch',
            'Flame Shock',
            'Fatal',
            'Ambush',
            'Bleed',
            "Hunter's Mark",
            'Corruption',
            'Follow',
            'Speed',
            'Backstab',
            'Renew',
            'Rend',
        ],
        'icons': [
            'M4 20 20 4M14 4h6v6',
            'M5 19 9 15M9 19l4-4M13 19l4-4',
            'M4 20 15 9M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0',
            'M13 2 4 14h7l-1 8 9-12h-7Z',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 7v5l3 2',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M4 12h10M10 6l6 6-6 6M18 6v12',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-2 0a2 2 0 1 0 4 0a2 2 0 1 0-4 0',
            'M4 20 20 4M14 4h6v6',
            'M4 12h10M10 6l6 6-6 6M18 6v12',
            'M3 12h4M5 8h6M5 16h6M13 12h8',
            'M13 2 4 14h7l-1 8 9-12h-7Z',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 7v5l3 2',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
        ],
    },
    33: {
        'core': "**Kill Command** unleashes a **Spectral Beast** in your pet's likeness. During **Bestial Wrath**, "
                'Kill Command recovers in 5 sec and a beast joins the hunt every second.',
        'slots': ['Spectral Beast', 'The Hunt', 'Final Attack'],
        'asks': ['How the beasts bite', 'How the pack hunts', 'How a beast says farewell'],
        'texts': [
            "Spectral Beasts bite for 78% of your pet's attack instead of 60%.",
            "Each Spectral Beast bite also hits up to 2 other enemies within 5 yards for 30% of your pet's attack.",
            "Each bite makes its target bleed for 15% of your pet's attack every second for 4 sec. Refreshed, never "
            "stacked.",
            'A bite on your marked target also bites one other enemy within 5 yards for 50% as much.',
            'Bites on your Frost Fever target also deal 10% of your ranged attack power as Frost damage.',
            'Spectral Beasts hold to their first prey and bite it for 20% more.',
            'Your Spectral Beasts switch to whatever enemy you target.',
            'When its prey dies, a Spectral Beast hunts on: your target, else the nearest enemy.',
            "Arcane Shot's empowered bite deals 40% more instead of 20%.",
            'Lightning Bolt sends your Spectral Beasts to its target, their next bite adding 20% of your spell power '
            'as Nature damage, once every 6 sec.',
            "As a Spectral Beast fades, it pounces for 120% of your pet's attack.",
            "As a Spectral Beast fades, it bites 4 times in a flurry, 36% of your pet's attack each.",
            'As a Spectral Beast fades, your pet deals 30% more damage for 6 sec.',
            'As a Spectral Beast fades, it pounces your marked target for 35% of your ranged attack power, and up to 2 '
            'enemies within 5 yards for 40% of that.',
            'As a Spectral Beast fades with your Mend Pet on your pet, it heals your pet for 5% of its health and '
            'bites for 25% of your ranged attack power.',
        ],
        'abilities': [
            'Alpha',
            'Ravage',
            'Bleed',
            "Hunter's Mark",
            'Icy Touch',
            'Fixed',
            'Command',
            'Scent',
            'Arcane Shot',
            'Lightning Bolt',
            'Pounce',
            'Flurry',
            'Howl',
            "Hunter's Mark",
            'Mend Pet',
        ],
        'icons': [
            'M12 3l3 6 6 1-4 4 1 6-6-3-6 3 1-6-4-4 6-1Z',
            'M3 12h6M15 12h6M12 3v6M12 15v6',
            'M6 4l2 8-2 8M12 4l2 8-2 8M18 4l2 8-2 8',
            'M12 3l3 6 6 1-4 4 1 6-6-3-6 3 1-6-4-4 6-1Z',
            'M12 3l3 6 6 1-4 4 1 6-6-3-6 3 1-6-4-4 6-1Z',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M4 20 20 4M14 4h6v6',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M4 18 12 4l8 14',
            'M5 19 9 15M9 19l4-4M13 19l4-4',
            'M6 9h4l5-4v14l-5-4H6Z',
            'M4 18 12 4l8 14',
            'M4 18 12 4l8 14',
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
            'A Lightning Bolt hit on an enemy your Arcane Blast hit in the last 15 sec makes your next Insight volley '
            'deal 20% more damage (8 sec).',
            'A Starfire hit grants 1 Insight, once every 8 sec, and makes your next Arcane Blast within 6 sec deal '
            '10% more damage.',
            "Insight missiles deal 40% more damage to their target: 56% of an Arcane Missile's damage.",
            'Each Insight missile also hits up to 2 other enemies within 8 yards of its target for 35% of its damage.',
            'The Insight missiles leave Arcane Echo on their target: 30% of their damage again, over 6 sec. A new '
            'Echo keeps what is left of the last.',
            'A Frostbolt hit readies your next empowered Arcane Missiles (within 15 sec): its first Insight missile '
            'also hits 2 other enemies within 8 yards for half its damage.',
            'Arcane Explosion within 6 sec of finishing an empowered Arcane Missiles bursts for 25% of your spell '
            'power on up to 5 enemies within 8 yards.',
            'An empowered Arcane Missiles does not channel: its missiles fly from you on their own, one every 0.5 '
            'sec, so you can move and keep casting.',
            'When an empowered Arcane Missiles completes, you regain 3% of your maximum mana and Arcane Blast costs '
            '20% less mana for 4 sec.',
            'The Insight missiles all fly as the channel starts, 0.15 sec apart, and deal 20% more damage: 48% of an '
            "Arcane Missile's damage.",
            'Evocation begun within 30 sec of an empowered Arcane Missiles restores 2% of your maximum mana more, '
            'once every 30 sec.',
            'Finishing an empowered Arcane Missiles while your Lightning Shield is up sends a bolt of 25% of your '
            'spell power as Nature damage at its target.',
        ],
        'abilities': [
            'Accelerate',
            'Expand',
            'Patience',
            'Lightning Bolt',
            'Starfire',
            'Focus',
            'Fracture',
            'Echo',
            'Frostbolt',
            'Arcane Explosion',
            'Unbound',
            'Reserves',
            'Overclock',
            'Evocation',
            'Lightning Shield',
        ],
        'icons': [
            'M13 2 4 14h7l-1 8 9-12h-7Z',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12h.01',
            'M5 4h14M5 20h14M7 4c0 5 10 11 10 16M17 4c0 5-10 11-10 16',
            'M13 2 4 14h7l-1 8 9-12h-7Z',
            'M12 3l2 6 6 2-6 2-2 6-2-6-6-2 6-2Z',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M12 3v6M12 15v6M3 12h6M15 12h6M6 6l3 3M15 15l3 3',
            'M3 12h4l3-7 4 14 3-7h4',
            'M12 2v20M4 7l16 10M20 7 4 17',
            'M12 12m-9 0a9 9 0 1 0 18 0a9 9 0 1 0-18 0M12 12h.01',
            'M4 18l6-6-6-6M12 18h8',
            'M6 4h12v4l-4 4 4 4v4H6v-4l4-4-4-4Z',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
            'M3 12c3-6 15-6 18 0M3 12c3 6 15 6 18 0M12 9v6',
            'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6ZM13 7l-3 5h4l-3 5',
        ],
    },
    35: {
        'core': 'Each **Frostbolt** that hits forms an **Icicle**: **30%** of its damage, 1 sec later. **Ice Lance**, '
            '**Cone of Cold** and **Blizzard** loose Icicles too; each leaves **Frostbite**.',
        'slots': ['Formation', 'Winter', 'Aftermath'],
        'asks': ['What the Icicles are', "Winter's interaction", 'What they leave behind'],
        'texts': [
            "Frostbolt's Icicles deal 40% more damage: 42% of the Frostbolt's damage.",
            "Ice Lance's two Icicles deal 25% of your spell power each instead of 20%. The second also leaves a Frost "
            "wound of 10% of your spell power over 4 sec.",
            'Cone of Cold looses 2 Icicles at each enemy it hits (up to 5), 10% of your spell power each.',
            'Blizzard looses an Icicle every second instead of every 2, each 15% of your spell power instead of 20%.',
            "Frostbolt's Icicles also hit up to 2 other enemies within 8 yards for half their damage.",
            "When both of Ice Lance's Icicles hit, your next Frostbolt within 6 sec casts 15% faster.",
            "Cone of Cold's Icicle deals 30% more damage to an enemy already in your Frostbite.",
            'Each Icicle Blizzard looses makes your next Ice Lance deal 5% more damage, up to 20%, for 8 sec.',
            'Fire Blast on an enemy in your Frostbite erupts in an Icicle: 25% of your spell power to it and up to 2 '
            'other enemies within 6 yards, once every 8 sec.',
            'After your Icy Touch hits an enemy, your next Frostbolt on it within 6 sec forms one more Icicle at half '
            'damage.',
            'Your Icicles deal 20% more damage to enemies already in your Frostbite.',
            "Ice Lance's second Icicle leaves Glacial Wound: 20% of your spell power over 6 sec. A new wound keeps "
            "what is left of the last.",
            "Cone of Cold's Icicles slow their enemies by 20% for 3 sec; your next Frostbolt Icicle on each in that "
            "time deals 25% more damage.",
            "Blizzard's Icicles also hit up to 2 other enemies within 6 yards for 40% of their damage; with none near, "
            "the Icicle deals 20% more.",
            "Every third Icicle strike makes your next Frostbolt within 5 sec instant. Ice Lance's two Icicles count "
            "as two.",
        ],
        'abilities': [
            'Spear',
            'Ice Lance',
            'Cone of Cold',
            'Blizzard',
            'Splinter',
            'Ice Lance',
            'Cone of Cold',
            'Blizzard',
            'Fire Blast',
            'Icy Touch',
            'Freeze',
            'Ice Lance',
            'Cone of Cold',
            'Blizzard',
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
            'M4 20 20 4M14 4h6v6',
            'M6 18 18 6M4 12l4-4M12 20l4-4',
            'M12 2v20M2 12h20M5 5l14 14M19 5 5 19',
            'M2 12h20M16 7l5 5-5 5',
            'M4 18c4-10 12-10 16-12M16 4l4 2-2 4',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
        ],
    },
    36: {
        'core': '**Pyroblast** leaves an **Ember**; **Fire Blast** on it calls a **phoenix** diving from above you '
            "onto it: **55%** of the Pyroblast's damage, and it burns.",
        'slots': ['Phoenix', 'Flight', 'Wake'],
        'asks': ['What wakes in the Ember', 'How it flies', 'What it leaves behind'],
        'texts': [
            "The phoenix strikes the Ember's target for 50% more: 82% of the Pyroblast's damage.",
            "The phoenix's path is 6 yards wide instead of 3, and enemies in it take 30% of the Pyroblast's damage "
            'instead of 25%.',
            'Three phoenixes dive instead, each with a third of the damage. Each one whose flight ends makes your '
            'next Pyroblast cast 5% faster, up to 15%, for 6 sec.',
            'Releasing the phoenix on your Flame Shock target also deals 20% of your spell power as Fire damage to '
            'it.',
            "A Lava Burst hit on the Ember's target adds one more bird to that phoenix, dealing 30% of its damage "
            "(one for each Ember).",
            "The phoenix dives twice as fast, 40 yards a second. When its flight ends, Fire Blast's cooldown is 2 sec "
            'shorter.',
            "After its strike, the phoenix circles the Ember's target for 2 sec, then strikes it again for 25% of the "
            "Pyroblast's damage.",
            "After its strike, the phoenix swoops through up to 2 other enemies within 10 yards of the Ember's "
            "target, striking each for 25% of the Pyroblast's damage.",
            'The first time a phoenix flies through your Flamestrike, it deals 20% of your spell power as Fire damage '
            'to up to 4 enemies in it.',
            "A Scorch hit on the Ember's target makes your next phoenix strike its target for 25% more damage (8 "
            "sec).",
            "The phoenix leaves a trail of fire for 4 sec: up to 5 enemies on it burn for 6% of the Pyroblast's "
            "damage every second. Trails don't stack.",
            'Enemies the phoenix strikes burn for 25% more of that hit over 6 sec. A new burn keeps what is left of '
            'the last.',
            "When the phoenix's flight ends, it bursts for 35% of the Pyroblast's damage to up to 5 enemies within 6 "
            'yards of it. A flock bursts once.',
            "When the phoenix's flight ends, your Flame Shock on its target lasts 2 sec longer, up to 6 sec for each "
            "cast.",
            "When the phoenix's flight ends while your Fire Ward is up, you gain an absorb of 40% of your spell power "
            "for 6 sec, once every 12 sec.",
        ],
        'abilities': [
            'Talon',
            'Broadwing',
            'Flock',
            'Flame Shock',
            'Lava Burst',
            'Homeward',
            'Spiral',
            'Wild',
            'Flamestrike',
            'Scorch',
            'Scorch',
            'Embers',
            'Rebirth',
            'Flame Shock',
            'Fire Ward',
        ],
        'icons': [
            'M4 4l6 6M10 4v6H4M20 20l-6-6M14 20v-6h6',
            'M2 12c5-6 15-6 20 0M2 12c5 6 15 6 20 0',
            'M4 18l4-6 4 6 4-6 4 6',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11ZM12 21v-6',
            'M4 18c4-6 12-6 16 0M8 10l4-6 4 6',
            'M4 12h14M14 7l5 5-5 5',
            'M12 12m-8 0a8 8 0 1 0 16 0M12 12m-4 0a4 4 0 1 0 8 0',
            'M4 18c4-10 6-10 8-4s4 6 8-6',
            'M3 20h18M7 20l3-8 4 4 3-8',
            'M4 12h14M14 7l5 5-5 5M4 6h6',
            'M3 20h18M6 20c0-5 3-6 3-10M12 20c0-6 3-7 3-12M18 20c0-4 2-5 2-8',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M12 2v4M12 18v4M2 12h4M18 12h4M5 5l3 3M16 16l3 3',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
            'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6Z',
        ],
    },
    37: {
        'core': '**Arcane Barrage** sets an **Arcane Orb** loose (every 12 sec at most): it flies to your enemies, '
            "strikes for **2 bolts**, then for 6 sec fires a bolt a second for **20%** of the Barrage's damage.",
        'slots': ['Orbit', 'Discharge', 'Ending'],
        'asks': ['Where the Orb goes', 'How it fires', 'How it ends'],
        'texts': [
            "Each enemy the Orb passes through takes two bolts' damage (40% of the Barrage's damage), once (up to 5).",
            "The Orb flies to its target's position and stays there. Its bolts deal 50% more damage to enemies within "
            '5 yards of it.',
            'The Orb circles you, 4 yards out, for 8 sec instead of 6, and its bolts reach 15 yards instead of 10.',
            "A Lightning Bolt hit on the Orb's target commands an extra bolt of Nature damage, once every 4 sec.",
            "The Orb's bolts deal 20% more damage to your Moonfire target, and extend that Moonfire by 1 sec (up to 4 "
            "sec for each cast), once every 2 sec.",
            "Bolts deal 60% more damage to the Orb's target: 32% of the Barrage's damage.",
            'Each bolt also strikes up to 2 other enemies in range for 75% of its damage.',
            'Each bolt becomes a lance that also pierces up to 3 enemies behind its target, for 80% of its damage.',
            'A bolt on your Frost Nova target also strikes one other enemy in your Frost Nova within 8 yards for half '
            'its damage.',
            "An Arcane Shot hit on the Orb's target makes its next bolt within 5 sec also pierce one more enemy "
            "within 8 yards for half its damage.",
            "When the Orb's time ends, it explodes for 120% of the Barrage's damage (6 bolts) to up to 5 enemies "
            'within 8 yards.',
            "When the Orb's time ends, it fires 3 last bolts at your target (else its own), each for 60% of the "
            "Barrage's damage.",
            "When the Orb's time ends, it leaves a field 6 yards wide for 6 sec: every second, a bolt's damage to up "
            "to 5 enemies in it (120% of the Barrage's damage in all).",
            'Finishing Evocation while the Orb is out makes it last 2 sec longer, once for each Orb.',
            'Setting the Orb loose while your Power Word: Shield is on you grants an absorb of 25% of your spell '
            'power for 6 sec, once every 12 sec.',
        ],
        'abilities': [
            'Wander',
            'Horizon',
            'Satellite',
            'Lightning Bolt',
            'Moonfire',
            'Focus',
            'Constellation',
            'Lance',
            'Frost Nova',
            'Arcane Shot',
            'Supernova',
            'Dying',
            'Remnant',
            'Evocation',
            'Power Word: Shield',
        ],
        'icons': [
            'M4 18 20 6M14 6h6v6',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12h.01',
            'M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0M12 3a9 9 0 0 1 9 9',
            'M13 2 4 14h7l-1 8 9-12h-7Z',
            'M12 3a9 9 0 1 0 9 9 7 7 0 0 1-9-9Z',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M5 5h.01M19 5h.01M12 12h.01M5 19h.01M19 19h.01M5 5l7 7 7-7M5 19l7-7 7 7',
            'M2 12h20M16 7l5 5-5 5',
            'M12 2v20M4 7l16 10M20 7 4 17',
            'M2 12h20M16 7l5 5-5 5M6 7v10',
            'M12 2v4M12 18v4M2 12h4M18 12h4M5 5l3 3M16 16l3 3M19 5l-3 3M8 16l-3 3',
            'M12 4v12M8 12l4 4 4-4M6 20h12',
            'M3 17c3-3 6-3 9 0s6 3 9 0M12 4v8',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
            'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6Z',
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
            'Each pulse on your Earth Shield target also heals one injured ally within 10 yards for 25% of your spell '
            'power. No Earth Shield charge is used.',
            'Each pulse on your Rejuvenation target makes that Rejuvenation last 1 sec longer, up to 4 sec for each '
            'cast.',
            'As it travels, the star heals up to 3 injured allies along its way for half a pulse each.',
            'Only Greater Heal moves the star. Each pulse on the same bearer heals 10% more than the last, up to 30%; '
            'moving the star starts over.',
            'Every third pulse also heals the most injured ally within 15 yards of the bearer for a pulse. If the '
            'bearer dies, the star moves on to that ally.',
            'Your Chain Heal on another ally moves the star there, with its arrival heal (at most once every 3 sec).',
            'Your Holy Shock on another ally moves the star there, with its arrival heal (at most once every 3 sec).',
            "When the star's 12 sec end, it heals its bearer for 2.5 pulses at once.",
            "When the star's 12 sec end, it leaves Afterglow on its bearer: 3.25 pulses of healing over 8 sec.",
            "When the star's 12 sec end, it shields its bearer for 2 pulses (6 sec), and they move 30% faster for 3 "
            'sec.',
            'When the star fades, if its bearer has your Riptide, it heals them for 60% of your spell power.',
            'When the star fades, your Renew on its bearer lasts 4 sec longer (up to 6 sec for each cast), and they '
            'are healed for 30% of your spell power.',
        ],
        'abilities': [
            'Steady',
            'Shared',
            'Shelter',
            'Earth Shield',
            'Rejuvenation',
            'Passage',
            'Guardian',
            'Seeking',
            'Chain Heal',
            'Holy Shock',
            'Benediction',
            'Afterglow',
            'Grace',
            'Riptide',
            'Renew',
        ],
        'icons': [
            'M12 2l2.5 7.5L22 12l-7.5 2.5L12 22l-2.5-7.5L2 12l7.5-2.5Z',
            'M8 12m-5 0a5 5 0 1 0 10 0a5 5 0 1 0-10 0M16 12m-5 0a5 5 0 1 0 10 0a5 5 0 1 0-10 0',
            'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6Z',
            'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6ZM12 8v8M8 12h8',
            'M12 3a9 9 0 1 0 9 9 7 7 0 0 1-9-9Z',
            'M4 18c4-10 12-10 16-12M16 4l4 2-2 4',
            'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6ZM12 8v8M8 12h8',
            'M11 11m-7 0a7 7 0 1 0 14 0a7 7 0 1 0-14 0M21 21l-5-5',
            'M5 12h4l2-4 2 8 2-4h4',
            'M13 2 4 14h7l-1 8 9-12h-7Z',
            'M12 4v16M4 12h16',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M4 12h14M14 7l5 5-5 5',
            'M3 17c3-3 6-3 9 0s6 3 9 0M12 4v8',
            'M20 12a8 8 0 1 1-3-6.2M20 4v5h-5',
        ],
    },
    39: {
        'core': 'Each **Starfire** that hits calls a **lunar owl**: 1 sec later it dives on the same enemy for '
            "**37.5%** of the Starfire's damage; **Wrath** on its prey calls it back.",
        'slots': ['Guardian', 'Descent', 'Blessing'],
        'asks': ['What the owl is', 'When it dives', 'What it leaves behind'],
        'texts': [
            "Owls deal 40% more damage to their target: 52.5% of the Starfire's damage.",
            'Each dive also hits up to 4 other enemies within 6 yards for half its damage.',
            'Each owl strikes 3 times, 1 sec apart, for 40% of its damage each (120% in all), laying Moonlit Prey '
            'once. The last strike makes your next Starfire cost 20% less mana.',
            "With your Hunter's Mark on an enemy, Starfire calls a Sentinel instead: it strikes your marked target at "
            "once, after 3 sec and after 6 sec, for 45% of an owl's damage each, following your mark. A new Starfire "
            "replaces it.",
            "Half of each owl's dive is Frost damage. A Frostbolt hit on an owl's target before it dives bursts for "
            "25% of your spell power on it and up to 2 other enemies within 6 yards, once for each owl.",
            'Owls dive 0.1 sec after Starfire instead of 1. Each dive makes your next Starfire cast 10% faster.',
            'Owls wait 3 sec, then dive for 30% more damage.',
            'Owls deal 15% more damage to your Moonfire target and extend that Moonfire by 4 sec instead of 2, up to 8 '
            'sec for each cast.',
            "Your Arcane Shot or Aimed Shot on your Hunter's Mark target, while an owl or Sentinel is out, commands it "
            "to strike that target for 25% of your spell power, once every 6 sec.",
            'The return strike also deals 15% of your spell power as Nature damage to up to 4 other enemies within 6 '
            'yards.',
            "An owl's strike wounds its target for as much again over 6 sec. A new wound keeps what is left of the "
            'last.',
            'Every third owl makes your next Starfire within 5 sec instant, and you move 20% faster for 5 sec.',
            'The return strike makes your next Wrath on that target within 5 sec deal 25% more damage.',
            "An owl or Sentinel striking your Hunter's Mark target lays Exposed Prey (6 sec, once every 4 sec): your "
            "next Arcane Shot, Aimed Shot or Steady Shot on it deals 25% of your spell power more.",
            'An owl diving on a target slowed by your Frostbolt lays Moonfrost (6 sec): your next Frostbolt on it '
            'deals 20% more damage and extends your Moonfire there by 2 sec.',
        ],
        'abilities': [
            'Pierce',
            'Wings',
            'Flock',
            "Hunter's Mark",
            'Frostbolt',
            'Swift',
            'Vigil',
            'Moonfire',
            'Arcane Shot',
            'Wrath',
            'Wound',
            'Guide',
            'Wrath',
            "Hunter's Mark",
            'Frostbolt',
        ],
        'icons': [
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M2 12c5-6 15-6 20 0M2 12c5 6 15 6 20 0',
            'M5 19 9 15M9 19l4-4M13 19l4-4',
            'M13 2 4 14h7l-1 8 9-12h-7Z',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
            'M13 2 4 14h7l-1 8 9-12h-7Z',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
            'M4 12h14M14 7l5 5-5 5',
            'M11 11m-7 0a7 7 0 1 0 14 0a7 7 0 1 0-14 0M21 21l-5-5',
            'M12 12m-8 0a8 8 0 1 0 16 0a8 8 0 1 0-16 0M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M4 12h14M14 7l5 5-5 5',
            'M13 2 4 14h7l-1 8 9-12h-7Z',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
            'M4 18c4-10 12-10 16-12M16 4l4 2-2 4',
        ],
    },
    40: {
        'core': '**Wrath** leaves **Sunlight**; **Starfire** on it collides for **80%** of your spell power and '
            'leaves **Moonlight**; **Wrath** on that flares for **40%** and leaves Sunlight again.',
        'slots': ['Collision', 'Alignment', 'Flare'],
        'asks': ['How the energies collide', 'How the marks align', 'How the sequence ends'],
        'texts': [
            'The collision deals 40% more damage: 112% of your Arcane spell power.',
            'The collision also hits up to 4 other enemies within 6 yards for half its damage.',
            'Instead of a hit, the collision ruptures its target for 150% of its damage over 6 sec. A new rupture '
            'keeps what is left of the last. A flare on it deals its next tick at once, without adding a tick.',
            'A collision on your Flame Shock target also deals 25% of your Fire spell power as Fire damage.',
            'A Frostbolt hit on your Sunlight target makes your next collision deal 25% more damage, once per '
            'Sunlight.',
            'Your marks last 12 sec instead of 8. Each 2 sec Sunlight was on its target adds 5% to the collision, up '
            'to 20%.',
            'A collision also strikes up to 3 other enemies within 12 yards that carry your Moonlight, for 35% of its '
            'damage. Their marks stay.',
            'Each flare makes your next Wrath or Starfire within 5 sec instant.',
            'A flare from your Lightning Bolt also hits one other enemy within 8 yards for half its damage.',
            'A collision on your Moonfire target spreads that Moonfire, for what is left of it, to one other enemy '
            'within 8 yards, once every 8 sec.',
            'The flare deals 60% more damage: 64% of your Nature spell power.',
            'Each flare makes your next Starfire within 5 sec cast 20% faster and cost 20% less mana.',
            'Each flare leaves a field 6 yards wide for 4 sec: every second, 10% of your Arcane spell power to up to '
            '5 enemies in it.',
            'A flare on your Insect Swarm target makes your next Wrath within 5 sec cast 15% faster.',
            'A flare while Barkskin is up shields you for 40% of your spell power for 6 sec, once every 12 sec.',
        ],
        'abilities': [
            'Align',
            'Skies',
            'Rupture',
            'Flame Shock',
            'Frostbolt',
            'Twilight',
            'Crossed',
            'Orbit',
            'Lightning Bolt',
            'Moonfire',
            'Return',
            'Dominance',
            'Eventide',
            'Insect Swarm',
            'Barkskin',
        ],
        'icons': [
            'M12 2v4M12 18v4M2 12h4M18 12h4M5 5l3 3M16 16l3 3',
            'M5 5h.01M19 5h.01M12 12h.01M5 19h.01M19 19h.01M5 5l7 7 7-7M5 19l7-7 7 7',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11ZM12 21v-6',
            'M12 2v20M4 7l16 10M20 7 4 17',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
            'M2 12h20M16 7l5 5-5 5',
            'M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0M12 3a9 9 0 0 1 9 9',
            'M13 2 4 14h7l-1 8 9-12h-7Z',
            'M9 3a9 9 0 1 0 9 9 7 7 0 0 1-9-9ZM18 4a4 4 0 1 0 2 6',
            'M12 12m-4 0a4 4 0 1 0 8 0a4 4 0 1 0-8 0M12 2v3M12 19v3M2 12h3M19 12h3',
            'M12 3a9 9 0 1 0 9 9 7 7 0 0 1-9-9Z',
            'M3 17c3-3 6-3 9 0s6 3 9 0M12 4v8',
            'M6 8h.01M12 5h.01M18 8h.01M8 14h.01M16 14h.01M12 19h.01',
            'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6Z',
        ],
    },
    41: {
        'core': "**Barkskin** in Bear Form raises **Ursoc's Guard** (30% of your health); **Maul** releases the "
            '**impact**: **75%** of your attack power plus all it absorbed.',
        'slots': ['Impact', 'Guard', 'Resolve'],
        'asks': ['How the impact lands', 'How the guard holds', 'What it leaves behind'],
        'texts': [
            'The impact deals 30% more damage.',
            'The impact strikes up to 8 enemies instead of 5.',
            'The impact comes when the guard ends instead of with Maul, for 125% of its damage.',
            'Your Earth Shield healing you during the guard makes the impact deal 20% more. No extra charge is used.',
            'Thunder Clap during the guard makes the impact also deal 20% of your attack power as Nature damage to '
            'each enemy it hits.',
            'The guard lasts 2 sec longer; Swipe no longer lengthens it.',
            'The guard absorbs 20% more.',
            "Mangle's mark also makes its target deal 15% less damage for 3 sec.",
            "The impact on your Rake target makes it bleed for 30% of the impact's damage over 6 sec.",
            "Earth Shock's empowering also makes the guard last 1 sec longer, once for each guard.",
            'The impact generates 50% more threat.',
            'The impact heals you for 20% of its damage, up to 3% of your maximum health.',
            'After the impact, your next Maul within 6 sec deals 20% more damage.',
            'The impact while your Lifebloom is on you heals you for 40% of your spell power, up to 3% of your maximum '
            'health.',
            'Barkskin while your Rejuvenation is on you also shields you for 3% of your maximum health for 4 sec.',
        ],
        'abilities': [
            'Crush',
            'Pack',
            'Maul',
            'Earth Shield',
            'Thunder Clap',
            'Swipe',
            'Hide',
            'Mangle',
            'Rake',
            'Earth Shock',
            'Roar',
            'Force',
            'Maul',
            'Lifebloom',
            'Rejuvenation',
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
            'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6Z',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
            'M4 12h14M14 7l5 5-5 5',
            'M4 4l6 6M10 4v6H4M20 20l-6-6M14 20v-6h6',
            'M2 12c5-6 15-6 20 0M2 12c5 6 15 6 20 0',
            'M3 20h18M6 20 12 4l6 16',
        ],
    },
    42: {
        'core': '**Sinister Strike** becomes **Sabre Slash**, **Arcane Shot** becomes **Pistol Shot** and **Rapid '
            'Fire** becomes **Broadside**; every third Slash readies **Opening Shot** for the pistol.',
        'slots': ['Pistol', 'Powder', 'Footwork'],
        'asks': ['How the pistol fires', 'What the powder adds', 'How you move'],
        'texts': [
            'Pistol Shot deals 40% more damage.',
            'Pistol Shot fires twice, 65% of its damage each; only the first grants a combo point.',
            'Pistol Shot spreads in a cone: up to 3 other enemies in front of you, as far as 10 yards past its target, '
            'take half its damage.',
            "Fire Blast's powder explosion deals 35% more damage and hits up to 5 enemies.",
            'Pistol Shot while your Lightning Shield is up also deals 25% of your spell power as Nature damage to its '
            'target.',
            'The Sabre Slash after a Pistol Shot deals 30% more instead of 15%.',
            'Eviscerate while Opening Shot is ready makes your next Pistol Shot deal 15% more for each combo point '
            'instead of 10% (75% at most).',
            "Fire Blast's powder explosion deals 25% more damage and hits up to 5 enemies.",
            "Pistol Shot on your Hunter's Mark target makes your next Sabre Slash within 6 sec deal 25% more damage.",
            'Pistol Shot on your Frost Fever target bursts for 20% of your attack power as Frost damage on up to 3 '
            'enemies within 6 yards, once every 8 sec.',
            'Every second Sabre Slash readies Opening Shot instead of every third.',
            'Pistol Shot makes you move 25% faster for 4 sec.',
            'Pistol Shot gives its 10 Energy back when it hits, once every 6 sec.',
            'Pistol Shot while your Renew is on you heals you for 30% of your spell power, once every 10 sec.',
            'Your Arcane Shot makes your next Pistol Shot within 8 sec deal 25% more damage.',
        ],
        'abilities': [
            'Caliber',
            'Gunslinger',
            'Scatter',
            'Fire Blast',
            'Lightning Shield',
            'Steel',
            'Eviscerate',
            'Fire Blast',
            "Hunter's Mark",
            'Icy Touch',
            'Draw',
            'Run',
            'Efficient',
            'Renew',
            'Arcane Shot',
        ],
        'icons': [
            'M4 20 20 4M14 4h6v6',
            'M2 12c5-6 15-6 20 0M2 12c5 6 15 6 20 0',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M12 12m-5 0a5 5 0 1 0 10 0a5 5 0 1 0-10 0',
            'M4 12 20 4M4 12l16 0M4 12l16 8',
            'M3 12h6M3 7h9M3 17h9M15 12l6 0M18 9l3 3-3 3',
            'M7 9h10a4 4 0 0 1 0 8H7a4 4 0 0 1 0-8ZM17 13h4M5 17l-2 4M11 17l-2 4',
            'M4 6h4M10 6h4M16 6h4M4 12h4M10 12h4M16 12h4',
            'M20 12a8 8 0 1 1-3-6.2M20 4v5h-5',
            'M4 20 20 4M14 4h6v6',
            'M2 12c5-6 15-6 20 0M2 12c5 6 15 6 20 0',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M12 12m-5 0a5 5 0 1 0 10 0a5 5 0 1 0-10 0',
            'M4 12 20 4M4 12l16 0M4 12l16 8',
            'M3 12h6M3 7h9M3 17h9M15 12l6 0M18 9l3 3-3 3',
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
            "Stormfront pulses deal 35% more to your Hunter's Mark target and also strike one other enemy within 8 "
            "yards for 40%.",
            'Frost Shock in the Stormfront leaves a frost trail under its target for 4 sec: 10% of your attack power '
            'as Frost damage each second to up to 3 enemies, once every 8 sec.',
            "Thunderlash doesn't arc: 120% of your attack power to its target instead.",
            'Thunderlash arcs to 4 enemies instead of 2, each for 30% of your attack power.',
            'Thunderlash burns its target for 100% of your attack power over 6 sec instead of its Nature hit; its arcs '
            'stay. A new burn keeps what is left of the last.',
            'Shield Slam in the Stormfront sends an arc of 30% of your attack power as Nature damage to its target and '
            'one other enemy within 8 yards, once every 6 sec.',
            'Thunderlash on your Flame Shock target spreads it to one other enemy within 8 yards for what is left of '
            'it, once every 10 sec.',
            "The rune's Lightning Shield discharges deal 50% more damage.",
            "Each of the rune's discharges makes you move 30% faster for 3 sec and brings Lava Lash back 1 sec sooner.",
            "Each of the rune's discharges also strikes up to 3 other enemies within 8 yards of its target for half "
            "its damage.",
            'Lightning Bolt in the Stormfront makes your next discharge within 6 sec strike again for 40% of its '
            'damage.',
            'Thunder Clap in the Stormfront also deals 25% of your attack power as Nature damage to up to 5 enemies '
            'within 8 yards, once every 8 sec.',
        ],
        'abilities': [
            'Eye',
            'Forked',
            'Rolling',
            "Hunter's Mark",
            'Frost Shock',
            'Head',
            'Chain',
            'White-Hot',
            'Shield Slam',
            'Flame Shock',
            'Overcharge',
            'Ride',
            'Ground',
            'Lightning Bolt',
            'Thunder Clap',
        ],
        'icons': [
            'M11 11m-7 0a7 7 0 1 0 14 0a7 7 0 1 0-14 0M11 11h.01',
            'M5 5l3 3M16 16l3 3M12 2v4M12 18v4',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
            'M11 11m-7 0a7 7 0 1 0 14 0a7 7 0 1 0-14 0M11 11h.01',
            'M11 11m-7 0a7 7 0 1 0 14 0a7 7 0 1 0-14 0M11 11h.01',
            'M13 2 4 14h7l-1 8 9-12h-7Z',
            'M5 5h.01M19 5h.01M12 12h.01M5 19h.01M19 19h.01M5 5l7 7 7-7M5 19l7-7 7 7',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M13 2 4 14h7l-1 8 9-12h-7Z',
            'M13 2 4 14h7l-1 8 9-12h-7Z',
            'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6Z',
            'M4 12h14M14 7l5 5-5 5',
            'M3 20h18M12 4v12M8 12l4 4 4-4',
            'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6Z',
            'M12 3 4 6v6c0 5 4 8 8 9 4-1 8-4 8-9V6Z',
        ],
    },
    44: {
        'core': 'Your **Fire Blast**, in Cat Form too, ignites your **Rake** or **Rip** victim: **Cinder Wound**, '
            '150% of your attack power plus 100% of your spell power over 6 sec.',
        'slots': ['Ignition', 'Claws', 'Instinct'],
        'asks': ['How the wound burns', 'What your claws add', 'What the hunt gives'],
        'texts': [
            'Cinder Wound deals 40% more damage.',
            'Blazing Strike hits up to 2 other enemies within 5 yards for half its damage; with none near, its target '
            'takes 25% more.',
            'Cinder Wound deals 15% more damage.',
            'Blazing Strike on your Flame Shock target deals 25% more as Fire damage.',
            'Fire Blast on your Frost Fever target also deals 25% of your attack power as Frost damage, once every 6 '
            'sec.',
            'Fire Blast makes you move 20% faster for 4 sec, and your next Shred within 6 sec gives 10 Energy back.',
            'Ferocious Bite on your Cinder Wound deals its next tick at once, without adding a tick.',
            "Flame Shock's spread reaches 3 enemies instead of 1.",
            'Pyroblast on your Cinder Wound target bursts for 40% of your spell power as Fire damage, once every 8 '
            'sec.',
            'Swipe spreads half of what is left of a Cinder Wound in its reach to one more enemy within 8 yards, once '
            'every 8 sec.',
            'Igniting a Cinder Wound gives you 10 Energy, once every 8 sec.',
            'Blazing Strike deals 40% more damage to its target, as Fire.',
            'When a Cinder Wound runs out, it deals 25% of your attack power as Fire damage to its target.',
            'Fire Blast on a bleeding enemy while your Rejuvenation is on you heals you for 2% of your maximum health, '
            'once every 10 sec.',
            'Rake on your Cinder Wound target makes your next Blazing Strike within 6 sec deal 30% more damage.',
        ],
        'abilities': [
            'Cinder',
            'Flame Shock',
            'Living',
            'Flame Shock',
            'Icy Touch',
            'Shred',
            'Bite',
            'Flame Shock',
            'Pyroblast',
            'Swipe',
            'Renewal',
            'Claws',
            'Scar',
            'Rejuvenation',
            'Rake',
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
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M12 12m-5 0a5 5 0 1 0 10 0a5 5 0 1 0-10 0M4 4l3 3M20 4l-3 3M4 20l3-3M20 20l-3-3',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
            'M5 19 9 15M9 19l4-4M13 19l4-4',
            'M4 14c4-6 12-6 16 0M8 14v3M12 14v4M16 14v3',
            'M2 12c5-6 15-6 20 0M2 12c5 6 15 6 20 0',
        ],
    },
    45: {
        'core': 'Your **Icy Touch**, in Cat Form too, lays **Frozen Prey**; your next **Shred** or **Mangle** '
            'shatters it for 150% of your attack power as Frost damage.',
        'slots': ['Shatter', 'Hunt', 'Instinct'],
        'asks': ['How the prey shatters', 'How you mark the hunt', 'What the shatter leaves'],
        'texts': [
            'The shatter deals 40% more damage.',
            "Shattering lays a half-strength Frozen Prey on one other enemy within 8 yards for 6 sec. It doesn't "
            "spread again.",
            'The shatter also hits up to 3 other enemies within 8 yards for half its damage.',
            'The shatter while your Lightning Shield is up also deals 20% of your spell power as Nature damage.',
            'The shatter on your Flame Shock target also deals 20% of your spell power as Fire damage.',
            'Frozen Wound deals 50% more damage.',
            'Ferocious Bite on your Frozen Wound also deals 10% of your attack power as Frost damage for each combo '
            'point.',
            "Frostbolt's empowering makes the shatter deal 40% more instead of 25%.",
            "The shatter on your Hunter's Mark target also hits one other enemy within 8 yards for half its damage.",
            'The shatter on your Moonfire target extends that Moonfire by 2 sec, up to 6 sec for each cast.',
            'Shattering makes you move 20% faster for 4 sec.',
            'Shattering gives you 10 Energy, once every 6 sec.',
            'Shattering slows the enemy by 30% for 3 sec, and your next Shred deals 15% more damage.',
            'Shattering while your Rejuvenation is on you heals you for 2% of your maximum health, once every 10 sec.',
            'Shattering a mark your Frostbolt empowered makes your next Shred within 6 sec cost 10 less Energy.',
        ],
        'abilities': [
            'Fang',
            'Pack',
            'Claws',
            'Lightning Shield',
            'Flame Shock',
            'Rake',
            'Bite',
            'Frostbolt',
            "Hunter's Mark",
            'Moonfire',
            'Swift',
            'Recovery',
            'Shred',
            'Rejuvenation',
            'Frostbolt',
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
            'M8 3l4 18 4-18',
            'M4 20 12 12M12 12l8-8M12 12l8 2M12 12l2 8',
            'M12 3c2 4 6 6 6 11a6 6 0 0 1-12 0c0-5 4-7 6-11Z',
            'M12 7v5l3 3M12 3a9 9 0 1 0 0 18',
            'M4 12h14M14 7l5 5-5 5',
            'M6 12h.01M12 6h.01M18 12h.01M12 18h.01M12 12m-2 0a2 2 0 1 0 4 0a2 2 0 1 0-4 0',
        ],
    },
}

BUILT.update({
    46: {
        'core': '**Blood Boil** links up to 5 enemies to you; **Blood Strike** on a link heals you and pulses to '
            'the rest; **Death Strike** becomes **Sanguine Recall**: it strikes every link and shields you.',
        'slots': ['Communion', 'Reclamation', 'Recall'],
        'asks': ['How the links hold', 'What Blood Strike takes back', 'What the Recall gives'],
        'texts': [
            'Your selected linked enemy deals 8% less damage to you while within 20 yards.',
            'Up to 8 enemies are linked. The heal and the absorb keep their caps.',
            'Links last 15 sec and reach 30 yards.',
            'Crimson Communion also lays your Corruption on up to 2 enemies it links, once every 12 sec.',
            'Mangle on a linked enemy also reclaims blood and pulses, as Blood Strike does (sharing its 2 sec).',
            'Blood Reclamation heals 2% of your maximum health, 2.6% at most.',
            'Blood Strike also leaves 35% of your attack power as Shadow damage over 4 sec on its linked victim.',
            'Communion pulses make their enemies deal 8% less magic damage to you for 4 sec.',
            'A reclaiming Blood Strike on your Rend target also deals 25% of your attack power as Physical damage.',
            'Reclaiming while your Earth Shield is on you also shields you for 1% of your maximum health for 4 sec, '
            'once every 6 sec.',
            "Sanguine Recall's absorb starts at 5% of your maximum health, 7% at most.",
            "Sanguine Recall's cooldown is 14 sec.",
            'Sanguine Recall also heals you for 2% of your maximum health and deals 45% of your attack power.',
            'Death Coil on a linked enemy brings Sanguine Recall back 2 sec sooner, once every 8 sec.',
            'Holy Shock healing you while you have a link shields you for 2% of your maximum health for 4 sec, once '
            'every 10 sec.',
        ],
        'abilities': [
            'Bond',
            'Court',
            'Veins',
            'Corruption',
            'Mangle',
            'Feast',
            'Echo',
            'Weakness',
            'Rend',
            'Earth Shield',
            'Throne',
            'Rapid',
            'Blood',
            'Death Coil',
            'Holy Shock',
        ],
        'icons': [],
    },
    47: {
        'core': '**Stoneskin Totem** becomes the **Worldheart Totem**, which follows you; your **Shocks** command it, '
            '**Stormstrike** echoes the last command and **Lava Lash** draws a magma line to it.',
        'slots': ['Shockcraft', 'Worldheart', 'Enhancement'],
        'asks': ['Which command grows', 'What the totem gives', 'What your strikes add'],
        'texts': [
            "Earth Shock's command absorbs 5% of your maximum health; its echo 3%.",
            "Flame's command deals 40% of your attack power; its echo 24%.",
            "Frost's command slows by 45% and reduces damage by 9%; its echo 27% and 5.4%.",
            'Blood Strike within 6 sec of an Earth command shields you for 2% of your maximum health for 4 sec, once '
            'for each command.',
            'Judgment repeats your last Shock command (within 10 sec) at half strength, once every 8 sec.',
            'Worldguard reduces damage taken by 11%. A shield is still needed.',
            'Flame and Frost commands reach 12 yards and up to 8 enemies; echoes too.',
            "Place Worldheart's cooldown is 5 sec, and placing it makes you move 25% faster for 3 sec.",
            'Standing in your Consecration with Worldguard up shields you for 2% of your maximum health for 4 sec, '
            'once every 8 sec.',
            'Your Earth Shield healing you with Worldguard up makes your next Shock command within 6 sec 20% stronger, '
            'once every 8 sec.',
            "Stormstrike's command echo can come every 4 sec.",
            "Magma Lash's line deals 16% of your attack power a tick and is 5 yards wide.",
            'Lava Lash also heals you for 1.5% of your maximum health, once every 4 sec.',
            'Thunder Clap repeats your last Shock command (within 10 sec) at half strength, once every 8 sec.',
            'Death Coil with Worldguard up heals you for 30% of your spell power, at most 2% of your maximum health, '
            'once every 8 sec.',
        ],
        'abilities': [
            'Earth',
            'Flame',
            'Winter',
            'Blood Strike',
            'Judgment',
            'Deepstone',
            'Wide',
            'Striding',
            'Consecration',
            'Earth Shield',
            'Resonant',
            'Molten',
            'Stone',
            'Thunder Clap',
            'Death Coil',
        ],
        'icons': [],
    },
    48: {
        'core': '**Bind: Voidwalker** binds a Voidwalker to you as Covenant Armor; **Covenant Lash** and **Soul '
            'Siphon** replace Searing Pain and Drain Life; **Shadow Ward** becomes **Dark Intercession**.',
        'slots': ['Covenant Armor', 'Siphon', 'Intercession'],
        'asks': ['What the armor holds', 'What the siphon draws', 'How the guardian intercedes'],
        'texts': [
            'Covenant Armor also gives 2,500 armor at level 60 (a share of it at lower levels).',
            'While bound, magic damage taken is reduced by a further 8%.',
            'Covenant Lash gives back 1% of your base mana, once every 2 sec.',
            'While bound, Blood Strike shields you for 2% of your maximum health for 4 sec, once every 8 sec.',
            'While bound, your own Earth Shield healing you also shields you for 1% of your maximum health for 4 sec, '
            'once every 8 sec.',
            'Soul Siphon heals 2.5% of your maximum health a tick.',
            'Each Soul Siphon tick also shields you for 1% of your maximum health for 3 sec, adding to what is left, '
            'up to 3%.',
            'Soul Siphon no longer slows you, reaches 30 yards instead of 20, and deals 30% of your spell power a '
            'tick.',
            'Soul Siphon on your Corruption target deals 15% more and makes it last 1 sec longer a tick (up to 6 sec '
            'for each cast).',
            'Soul Siphon on your Frost Fever target also deals 10% of your spell power as Frost damage and heals 0.25% '
            'more a tick.',
            'Dark Intercession reduces damage taken by 22%.',
            'During Intercession, Covenant Lash hits up to 7 more enemies for 75% of its damage.',
            "Dark Intercession's cooldown is 24 sec, and it heals you for 2% of your maximum health.",
            'Judgment during Dark Intercession lashes its target for 25% of your spell power as Shadowflame damage, '
            'once every 6 sec.',
            'While bound, Blood Boil brings Dark Intercession back 2 sec sooner and shields you for 1% of your maximum '
            'health for 4 sec, once every 8 sec.',
        ],
        'abilities': [
            'Plate',
            'Nether',
            'Hunger',
            'Blood Strike',
            'Earth Shield',
            'Deep',
            'Ward',
            'Unfettered',
            'Corruption',
            'Icy Touch',
            'Manifest',
            'Legion',
            'Petition',
            'Judgment',
            'Blood Boil',
        ],
        'icons': [],
    },
    49: {
        'core': '**Shadow Bolt** on your **Corruption** target opens a **Hungering Rift** that lashes it every 2 sec, '
            'harder for every damage over time effect on it.',
        'slots': ['Maw', 'Anchor', 'Appetite'],
        'asks': ['How the rift bites', 'Where the rift holds', 'What the rift feeds on'],
        'texts': [
            "The Rift's lashes deal 35% more damage to its target.",
            'Each lash also strikes up to 2 other Corruption targets within 8 yards for 60%. Shadow Bolt on the target '
            'makes the next lash 20% stronger instead.',
            'Each lash also strikes up to 4 enemies in a 12-yard cone beyond the target for half.',
            'Lashes on your Immolate target also deal 15% of your spell power as Fire damage.',
            'Lashes on your Haunt target deal 20% more and make Haunt last 1 sec longer (up to 4 sec for each cast).',
            'Once its target has moved 5 yards, the next lash deals 25% more, once every 3 sec.',
            'The Rift stays where it opened, reaching its target within 20 yards; each lash in a row deals 10% more '
            '(30% at most).',
            'The Rift follows you onto your selected Corruption target within 30 yards; switching targets lashes once '
            'for half, once every 4 sec.',
            'Lashes on your Shadow Word: Pain target also strike one other Shadow Word: Pain target within 8 yards for '
            'half.',
            "While you channel Drain Life on the Rift's target, each lash heals you for 25% of its damage (1% of your "
            "maximum health a second at most).",
            'Each lash leaves 25% of its damage again as Shadow damage over 6 sec.',
            'Every third lash erupts for 30% of your spell power on up to 5 enemies near the target; if the target '
            'dies first, it erupts for half.',
            'Lashes make your Corruption last 1 sec longer (up to 4 sec for each cast); when the target dies, the Rift '
            'moves to a Corruption target within 15 yards.',
            'Every third lash on your Curse of Agony target also deals 25% of your spell power as Shadow damage.',
            "Death Coil on the Rift's target makes the Rift last 2 sec longer, once for each Rift.",
        ],
        'abilities': [
            'Bite',
            'Corruption',
            'Cone',
            'Immolate',
            'Haunt',
            'Moving prey',
            'Stays put',
            'Your target',
            'Shadow Word: Pain',
            'Drain Life',
            'Over time',
            'Every third',
            'Corruption',
            'Curse of Agony',
            'Death Coil',
        ],
        'icons': [],
    },
    50: {
        'core': '**Howling Blast** marks the pack and readies 2 **Frost blades**; **Obliterate** or **Frost Strike** '
            'on a marked enemy looses one.',
        'slots': ['Blade', 'Wind', 'Winter'],
        'asks': ['How the blade cuts', 'What the wind carries', 'What winter leaves'],
        'texts': [
            'Your Frost blades deal 40% more damage.',
            "Each loosed blade becomes 3 blades for 40% each; Frost Strike's follow-up waits for the last.",
            'Blades pierce up to 4 enemies behind their target for half.',
            'Blades deal 50% more to enemies frozen by your Hungering Cold.',
            'A blade on your Blood Plague target also strikes one enemy within 8 yards for half.',
            'Icy Touch makes the next blade deal 35% more instead of 20%.',
            'The frozen field deals 50% more damage and slows its enemies by 25%.',
            "Frost Strike's follow-up deals 25% of your attack power instead of 15%.",
            'Horn of Winter makes your next blade also deal 25% of your attack power as Frost damage.',
            'A blade on an enemy in your Death and Decay leaves 30% of its damage again as Shadowfrost damage over 6 '
            'sec.',
            'A blade makes you 10% faster for 5 sec, and your next Frost Strike deals 10% more.',
            'Blades leave 30% of their damage again as Frost damage over 6 sec.',
            'A blade brings Howling Blast back 1 sec sooner, once every 8 sec.',
            'After a blade, your next Death Strike within 6 sec heals 30% more.',
            'A blade on your Frost Fever target makes your next Frost Strike within 6 sec deal 20% more.',
        ],
        'abilities': [
            'Sharper',
            'Three blades',
            'Pierce',
            'Hungering Cold',
            'Plague Strike',
            'Icy Touch',
            'Chains of Ice',
            'Frost Strike',
            'Horn of Winter',
            'Death and Decay',
            'Frost Strike',
            'Over time',
            'Howling Blast',
            'Death Strike',
            'Frost Strike',
        ],
        'icons': [],
    },
    51: {
        'core': '**Aimed Shot** becomes **Trueshot**: a 2 sec windup for 45% more, piercing up to 4 enemies behind its '
            'target.',
        'slots': ['Shot', 'Mark', 'Recovery'],
        'asks': ['How the shot lands', 'What the mark adds', 'What follows the shot'],
        'texts': [
            'Trueshot deals 30% more damage to its target.',
            "Trueshot's pierce deals 65% of its hit instead of 40%.",
            'Trueshot winds up for 2.5 sec instead, and deals 30% more to its target and 20% more to the pierced.',
            'Trueshot on your Serpent Sting target also deals 25% of your ranged attack power as Nature damage.',
            'Trueshot on your Concussive Shot target also deals 25% of your ranged attack power.',
            'Exposed Target makes your next Arcane Shot or Chimera Shot deal 35% more instead of 20%.',
            'After Multi-Shot, Trueshot pierces up to 2 more enemies.',
            "Silencing Shot's payload deals 50% of your ranged attack power instead of 30%.",
            "Silencing Shot's payload also strikes up to 2 pierced enemies for 40% of it.",
            'Trueshot on your marked target also strikes one enemy within 8 yards for 25% of its hit.',
            'Trueshot brings Arcane Shot back 2 sec sooner.',
            'After Trueshot you move 25% faster for 4 sec.',
            'After Trueshot, your next Arcane Shot within 6 sec deals 20% more.',
            'After Trueshot, your next Steady Shot within 6 sec casts 20% faster.',
            'Trueshot while your Mend Pet is on your pet heals it for 5% of its health, once every 10 sec.',
        ],
        'abilities': [
            'Harder',
            'Pierce',
            '2.5 sec',
            'Serpent Sting',
            'Concussive Shot',
            "Hunter's Mark",
            'Multi-Shot',
            'Silencing Shot',
            'Silencing Shot',
            "Hunter's Mark",
            'Arcane Shot',
            'Speed',
            'Arcane Shot',
            'Steady Shot',
            'Mend Pet',
        ],
        'icons': [],
    },
    52: {
        'core': '**Renew** plants a **Scarlet Bloom**; your **Flash Heal** or **Greater Heal** there opens it onto the '
            'most injured ally nearby.',
        'slots': ['Bloom', 'Tending', 'Aftermath'],
        'asks': ['How the Bloom opens', 'How you tend it', 'What it leaves behind'],
        'texts': [
            'The opening heals for 110% of your spell power instead of 80%.',
            'The opening heals the 2 most injured allies nearby for 60% each.',
            'Allies the opening heals also heal for 30% of your spell power over 6 sec.',
            'Circle of Healing opens the Blooms of up to 2 allies it heals.',
            "Power Word: Shield on a Bloom's ally opens it.",
            'A Renew that has ticked 3 times makes the opening 60% stronger instead of 30%.',
            'An ally can take a new Bloom 4 sec after one opens instead of 8.',
            'The opening reaches the most injured ally within 20 yards instead of 10.',
            'Prayer of Healing opens up to 5 Blooms, each 15% stronger.',
            'A Bloom opened on an ally carrying your Prayer of Mending heals 30% more.',
            'Each Bloom opens twice before it is spent.',
            'When the Bloom opens onto another ally, its own ally is also shielded for 40% of your spell power.',
            "Opening a Bloom starts its ally's Renew over again.",
            'A Bloom opened on your Guardian Spirit target heals 50% more.',
            'Binding Heal also opens your own Bloom.',
        ],
        'abilities': [
            'Stronger',
            'Two allies',
            'Heal over time',
            'Circle of Healing',
            'Power Word: Shield',
            'Ripeness',
            '4 sec',
            '20 yards',
            'Prayer of Healing',
            'Prayer of Mending',
            'Twice',
            'Shield',
            'Renew',
            'Guardian Spirit',
            'Binding Heal',
        ],
        'icons': [],
    },
    53: {
        'core': '**Rejuvenation** stacks up to 3 times on one ally, and on a healthy or full ally it **seeks** the '
            'most injured instead.',
        'slots': ['Grove', 'Seeking', 'Bloom'],
        'asks': ['How deep the grove grows', 'How it seeks the injured', 'What the full grove does'],
        'texts': [
            'Your extra Rejuvenations heal 20% more.',
            'Rejuvenation stacks up to 4 times.',
            'Regrowth on an ally with your Rejuvenation grows a stack.',
            'Nourish heals 8% more for each of your Rejuvenations on its target.',
            'Your extra Rejuvenations on an ally with your Thorns heal 25% more.',
            'Wild Growth grows a stack on up to 4 allies instead of 2.',
            'Rejuvenation seeks from allies above 90% health, not only full ones.',
            "Lifebloom's bloom grows a stack on its ally, or plants your Rejuvenation there.",
            'Tranquility grows a stack on each ally it heals that carries your Rejuvenation, once every 8 sec.',
            "Rejuvenation while Nature's Swiftness is on you grows the full stack at once, spending it.",
            'Swiftmend heals 40% more for each extra Rejuvenation instead of 20%.',
            'Swiftmend on a full stack also heals up to 3 injured allies within 10 yards for 30% of its heal.',
            'An ally reaching the full stack is shielded for 40% of your spell power for 6 sec.',
            'Healing Touch on an ally with your Rejuvenation renews its extra Rejuvenations.',
            'Innervate on an ally with your Rejuvenation grows it to the full stack.',
        ],
        'abilities': [
            'Stronger',
            'Four',
            'Regrowth',
            'Nourish',
            'Thorns',
            'Wild Growth',
            '90% health',
            'Lifebloom',
            'Tranquility',
            "Nature's Swiftness",
            'Swiftmend',
            'Swiftmend',
            'Shield',
            'Healing Touch',
            'Innervate',
        ],
        'icons': [],
    },
    54: {
        'core': '**Shield Block** raises an **Iron Bulwark**; **Shield Slam** releases what it held as an **Iron '
            'impact**.',
        'slots': ['Impact', 'Empowerment', 'Hold'],
        'asks': ['How the impact lands', 'What makes it stronger', 'What the bulwark gives you'],
        'texts': [
            'The Iron impact deals 30% more damage.',
            'The Iron impact hits up to 8 enemies.',
            'The impact is released when the bulwark ends instead of on Shield Slam, for 125%; Shield Slam in the '
            'bulwark still adds its share.',
            'Shockwave in the bulwark releases the impact once more, for 60%.',
            'Concussion Blow in the bulwark makes the impact also deal 25% of your attack power to its first target.',
            'Revenge makes the impact 40% stronger instead of 20%.',
            "After Thunder Clap, the impact's circle reaches 12 yards.",
            'Devastate makes the impact 30% stronger instead of 15%.',
            'Devastate in the bulwark also makes your next Revenge deal 20% more.',
            'The impact on your Rend target also makes it bleed for 30% of the impact over 6 sec.',
            'The bulwark absorbs 20% more.',
            'Shield Slam makes you 20% faster and absorbs 3% of your maximum health, for 4 sec.',
            'The impact causes 60% more threat.',
            'Shield Block under Last Stand makes the bulwark absorb 5% more of your maximum health.',
            'The impact heals you for 2% of your maximum health, 5% under Shield Wall, once every 10 sec.',
        ],
        'abilities': [
            'Stronger',
            'Eight',
            'Shield Slam',
            'Shockwave',
            'Concussion Blow',
            'Revenge',
            'Thunder Clap',
            'Devastate',
            'Devastate',
            'Rend',
            'Stronger',
            'Shield Slam',
            'Threat',
            'Last Stand',
            'Shield Wall',
        ],
        'icons': [],
    },
})

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


# The baked pieces still drawn (the panel's and the rune button's glow); the rest of the first design is not written.
KEPT = {'Soft'}


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
            if png.stem not in KEPT:
                continue
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
