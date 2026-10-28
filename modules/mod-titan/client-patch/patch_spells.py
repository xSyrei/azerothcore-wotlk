#!/usr/bin/env python3
"""Add Emberforge's own spells (the Demon Hunter's) to Spell.dbc and SpellIcon.dbc (WoW 3.3.5a),
and their own visuals to SpellVisual.dbc, SpellVisualKit.dbc and SpellVisualEffectName.dbc.

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
    'MaxAffectedTargets': 212, 'DmgClass': 213, 'SchoolMask': 225, 'RuneCostID': 226,
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
VISUAL_KITS = {'precast': 1, 'cast': 2, 'impact': 3}     # SpellVisual columns
KIT_CHEST, KIT_BASE, KIT_LEFT_HAND, KIT_RIGHT_HAND, KIT_LEFT_WEAPON, KIT_RIGHT_WEAPON = 4, 5, 6, 7, 9, 10
HANDS, WEAPONS = (KIT_LEFT_HAND, KIT_RIGHT_HAND), (KIT_LEFT_WEAPON, KIT_RIGHT_WEAPON)
KIT_ANIM = 2
NO_ANIM = 0xFFFFFFFF
BLANK_KIT = 3049    # Sinister Strike's impact: only a chest effect and the hit sound
EFFECTS_DIR = 'Spells\\Emberforge'     # client folder of our effect models

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
AURA_PERIODIC_TRIGGER_SPELL, AURA_MOD_DODGE_PERCENT = 23, 49
TARGET_UNIT_CASTER, TARGET_SRC_CASTER, TARGET_UNIT_SRC_AREA_ENEMY, TARGET_UNIT_CONE_ENEMY = 1, 22, 15, 104
DURATION_1S, DURATION_2S = 36, 39
RADIUS_8, RADIUS_15, RADIUS_20 = 14, 18, 9

# Icons: Interface\Icons\<name>. Ours come from icons/spells/<name>.png (in the client patch); the
# client already has the rest, found by name in SpellIcon.dbc.
ICONS = ['Ability_DemonHunter_ChaosStrike', 'Ability_DemonHunter_BladeDance', 'Ability_DemonHunter_FelRush',
         'Ability_DemonHunter_EyeBeam', 'INV_Weapon_Glave_01']

# Trails: the trail model (spells/<model>.png) and its SpellVisualEffectName id. "shape" picks the
# model: "weapon" (models/spell_effects.weapon_trail, for both weapons) or "body" (body_trail, for
# the chest); "options" go to that function.
TRAILS = {
    'DemonsBite': {'model': 'DH_DemonsBite_Trail', 'effect': 7100},
    'ChaosStrike': {'model': 'DH_ChaosStrike_Trail', 'effect': 7101},
    # Blade Dance: the gray smoke trail, tinted fel green, for the whole spin.
    'BladeDance': {'model': 'DH_BladeDance_Trail', 'effect': 7103,
                   'options': {'duration': 900, 'lifetime': 0.3, 'color': (0.55, 1.0, 0.3)}},
    # Fel Rush: behind the body for the length of the dash.
    'FelRush': {'model': 'DH_FelRush_Trail', 'effect': 7102, 'shape': 'body', 'options': {'lifetime': 0.5}},
}

# Blizzard effect models (SpellVisualEffectName ids) used below.
MANGLE_IMPACT_DEMON = 4697      # Spells\\Mangle_Impact_Demon: a demon's bite
FEL_PRECAST_HAND = 3169         # Spells\\Fel_Fire_Precast_Hand: fel flames on the hands
FEL_FIRE_IMPACT = 3221          # Spells\\Fel_Fire_ImpactDD_High_Chest: a big fel fire hit
FEL_FLAMESHOCK_IMPACT = 3111    # Spells\\Fel_FlameShock_Impact_Chest: a small fel fire hit
FEL_FIREBLAST_IMPACT = 3227     # Spells\\Fel_FireBlast_Impact_Chest
FEL_HELLFIRE_BASE = 3312        # Spells\\Fel_Hellfire_Impact_Caster_Base: fel flames at the feet

# Visuals: the Blizzard visual copied, the new visual's id, and per kit (precast, cast or impact)
# the new kit's id and what goes in its slots, a copy of the copied visual's kit (or of BLANK_KIT
# if it has none). A slot's effect is a Blizzard effect id or one of TRAILS; "anim" sets the kit's
# animation (NO_ANIM: none). "clear" drops kits.
VISUALS = {
    'DemonsBite': {'copy': 253, 'id': 16700, 'kits': {
        'cast': (15600, {WEAPONS: 'DemonsBite'}),
        'impact': (15604, {KIT_CHEST: MANGLE_IMPACT_DEMON})}},
    'ChaosStrike': {'copy': 253, 'id': 16701, 'kits': {
        'cast': (15601, {WEAPONS: 'ChaosStrike', HANDS: FEL_PRECAST_HAND}),
        'impact': (15605, {KIT_CHEST: FEL_FIRE_IMPACT})}},
    # Blade Dance is a Whirlwind; it only targets the Demon Hunter (the dodge), so no impact: its
    # strikes (90107, 90108) bring their own.
    'BladeDance': {'copy': 223, 'id': 16703, 'clear': ('impact',), 'kits': {
        'cast': (15603, {WEAPONS: 'BladeDance', KIT_BASE: FEL_HELLFIRE_BASE})}},
    'BladeDanceStrike': {'copy': 253, 'id': 16704, 'clear': ('cast',), 'kits': {
        'impact': (15606, {KIT_CHEST: FEL_FLAMESHOCK_IMPACT})}},
    'BladeDanceFinalStrike': {'copy': 253, 'id': 16705, 'clear': ('cast',), 'kits': {
        'impact': (15607, {KIT_CHEST: FEL_FIRE_IMPACT})}},
    # Fel Rush was a Blink: fel hands instead of shadow ones, the trail behind the body, and no cast
    # animation (the dash runs). The warlock teleport flame stays where the dash starts.
    'FelRush': {'copy': 10694, 'id': 16702, 'clear': ('precast',), 'kits': {
        'cast': (15602, {KIT_CHEST: 'FelRush', HANDS: FEL_PRECAST_HAND, 'anim': NO_ANIM})}},
    'FelRushHit': {'copy': 253, 'id': 16706, 'clear': ('cast',), 'kits': {
        'impact': (15608, {KIT_CHEST: FEL_FIREBLAST_IMPACT})}},
}

# Spells: id, the Blizzard spell copied, name, tooltip text, aura text, icon, then column changes.
# "two_weapons": needs a one-handed weapon in each hand (the main hand checked here, the off hand by
# the server's spell_titan_dh_dual_weapons, src/demon_hunter.cpp).
# Effects not listed keep the copied spell's; "clear" empties effects (numbers 1-3).
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
                       'Physical damage, then once more for 74.2%, and dodge every attack for $d.',
        'tooltip': 'Dodging all attacks.',
        'set': {'Category': 0, 'CategoryRecoveryTime': 0, 'RecoveryTime': 10000, 'PowerType': FURY,
                'ManaCost': 350, 'MaxAffectedTargets': 0, 'DurationIndex': DURATION_1S, 'BaseLevel': 10,
                'SpellLevel': 10},
        'clear': [1, 2],
        'effects': {
            1: {'Effect': EFFECT_APPLY_AURA, 'Aura': AURA_MOD_DODGE_PERCENT, 'BasePoints': 99, 'DieSides': 1,
                'TargetA': TARGET_UNIT_CASTER},
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
        'two_weapons': True,
        'description': 'Blast all enemies in front of you with fel energy for $d, dealing $90106s1 Chaos damage '
                       'every $t1 sec.',
        'tooltip': 'Channeling Eye Beam.',
        'set': {'PowerType': FURY, 'ManaCost': 300, 'ManaCostPercentage': 0, 'RecoveryTime': 30000,
                'DurationIndex': DURATION_2S, 'BaseLevel': 20, 'SpellLevel': 20, 'MaxLevel': 0,
                'SchoolMask': CHAOS, 'SpellVisual': 8536},
        'clear': [2, 3],
        'effects': {1: {'Amplitude': 200, 'TriggerSpell': 90106}},
    },
    {
        'id': 90106, 'copy': 5857, 'name': 'Eye Beam', 'icon': 'Ability_DemonHunter_EyeBeam',
        'description': '',
        'set': {'SchoolMask': CHAOS, 'SpellVisual': 0, 'BaseLevel': 20, 'SpellLevel': 20, 'MaxLevel': 80},
        'effects': {1: {'BasePoints': 9, 'DieSides': 1, 'RealPointsPerLevel': 3.0, 'TargetA': TARGET_UNIT_CONE_ENEMY,
                        'TargetB': 0, 'RadiusIndex': RADIUS_20, 'BonusMultiplier': 0.0}},
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


def build_visuals(source, out):
    """The trails' effect names, and VISUALS' kits and visuals."""
    visuals = Dbc(source / 'SpellVisual.dbc', VISUAL_FIELDS)
    kits = Dbc(source / 'SpellVisualKit.dbc', KIT_FIELDS)
    names = Dbc(source / 'SpellVisualEffectName.dbc', EFFECT_NAME_FIELDS)
    for key, trail in TRAILS.items():
        model = f'{EFFECTS_DIR}\\{trail["model"]}.mdx'
        names.rows[trail['effect']] = (trail['effect'], names.add_string(f'Emberforge {key} Trail'),
                                       names.add_string(model), raw(0.0), raw(1.0), raw(0.01), raw(100.0))
    for spec in VISUALS.values():
        visual = list(visuals.rows[spec['copy']])
        visual[0] = spec['id']
        for kind in spec.get('clear', ()):
            visual[VISUAL_KITS[kind]] = 0
        for kind, (kit_id, slots) in spec['kits'].items():
            kit = list(kits.rows[visual[VISUAL_KITS[kind]] or BLANK_KIT])
            kit[0] = kit_id
            for where, effect in slots.items():
                if where == 'anim':
                    kit[KIT_ANIM] = effect
                    continue
                for slot in where if isinstance(where, tuple) else (where,):
                    kit[slot] = TRAILS[effect]['effect'] if isinstance(effect, str) else effect
            kits.rows[kit_id] = tuple(kit)
            visual[VISUAL_KITS[kind]] = kit_id
        visuals.rows[spec['id']] = tuple(visual)
    visuals.save(out / 'SpellVisual.dbc')
    kits.save(out / 'SpellVisualKit.dbc')
    names.save(out / 'SpellVisualEffectName.dbc')


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

    build_visuals(source, out)
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
