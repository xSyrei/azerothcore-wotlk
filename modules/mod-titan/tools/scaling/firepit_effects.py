#!/usr/bin/env python3
"""Fireforged bonus effects (The Firepit Ignites): an Epic Fireforged piece carries exactly one, rolled from the
Hero's loot pool (Tank, Healer, Melee, Ranged) and the Hybrid pool, never one their build cannot use.

Each effect is an equip spell (EQUIP_SPELL_FIRST + its index) on the piece through an enchantment, one per item level
tier (ENCHANT_FIRST + index * TIERS + tier) so its tooltip shows the piece's own numbers. Values scale with the
piece's item level: L = clamp(item level / 90, 0.25, 2.0); P = 225 L spell power, A = 335 L attack power,
R = 160 L rating. The server (src/fireforged.cpp) sets the equip auras' amounts from the item, and runs the procs:
15% on their event, 30 sec apart, their buff (BUFF_FIRST on) at the same scale.

Used by client-patch/patch_runes.py (the spells), tools/scaling/firepit_gear.py (the enchantments) and, run as a
script, writes src/fireforged_effects.h. Usage: python3 firepit_effects.py
"""

from pathlib import Path

HERE = Path(__file__).resolve().parent
HEADER = HERE.parents[1] / 'src' / 'fireforged_effects.h'

EQUIP_SPELL_FIRST, BUFF_FIRST, STRIKE_SPELL = 93500, 93560, 93595
ENCHANT_FIRST = 40100

# Aura types and their misc values.
MOD_RESISTANCE, MOD_DAMAGE_DONE, SCHOOL_ABSORB, MOD_POWER_REGEN, MOD_ATTACK_POWER = 22, 13, 69, 85, 99
MOD_POWER_REGEN_PERCENT, MOD_RANGED_ATTACK_POWER, MOD_HEALING_DONE, MOD_RANGED_HASTE = 110, 124, 135, 140
MOD_BASE_RESISTANCE_PCT, MOD_SHIELD_BLOCKVALUE, MOD_RATING, DUMMY = 142, 158, 189, 4
DEFENSE, DODGE, PARRY, BLOCK = 1 << 1, 1 << 2, 1 << 3, 1 << 4
CRIT = (1 << 8) | (1 << 9) | (1 << 10)
HASTE = (1 << 17) | (1 << 18) | (1 << 19)
ARMOR_PEN = 1 << 24
SPELLS_MASK, ALL_MASK, ARMOR = 126, 127, 1
MANA, FOCUS, ENERGY = 0, 2, 3

# Values: (kind, multiplier): 'P' spell power, 'A' attack power, 'R' rating, 'L' 200 x L flat (crit bursts),
# 'C' a constant (percent and the like).
SP = [(MOD_DAMAGE_DONE, SPELLS_MASK), (MOD_HEALING_DONE, ALL_MASK)]
AP = [(MOD_ATTACK_POWER, 0), (MOD_RANGED_ATTACK_POWER, 0)]


def auras(pairs, value):
    return [(aura, misc, value) for aura, misc in pairs]


# Needs (a build without one never rolls the effect).
NEEDS = {'block': 1, 'parry': 2, 'shield': 4, 'mana': 8, 'energy': 16, 'focus': 32, 'heal': 64, 'spell': 128,
         'ranged': 256, 'physical': 512, 'tank': 1024, 'caster_pick': 2048, 'physical_pick': 4096}
# Triggers of a proc.
TRIGGERS = {'': 0, 'taken': 1, 'taken_low': 2, 'blocked': 3, 'melee': 4, 'spell': 5, 'ranged': 6, 'heal': 7,
            'heal_low': 8, 'self_heal': 9, 'direct': 10}
# Specials: a proc that is not a buff of its own value, or an equip the server reads.
SPECIALS = {'': 0, 'armor': 1, 'absorb_self': 2, 'absorb_target': 3, 'mana': 4, 'strike': 5, 'direct_heal': 6,
            'periodic_heal': 7, 'melee_ability': 8, 'auto_attack': 9, 'direct_spell': 10, 'ranged_ability': 11,
            'periodic_spell': 12}

# (name, pool, needs, text, equip auras, trigger, buff auras, buff duration (sec), special, special value)
EFFECTS = [
    # Tank.
    ('Ironbound', 'T', [], 'Increases armor from your items by 2%.',
     [(MOD_BASE_RESISTANCE_PCT, ARMOR, ('C', 2))], '', [], 0, '', 0),
    ('Steadfast', 'T', [], 'Increases defense rating by {R}.', [(MOD_RATING, DEFENSE, ('R', 1))], '', [], 0, '', 0),
    ('Sure Footing', 'T', [], 'Increases dodge rating by {R}.', [(MOD_RATING, DODGE, ('R', 1))], '', [], 0, '', 0),
    ('Stonewall', 'T', ['block'], 'Increases block rating by {R}.', [(MOD_RATING, BLOCK, ('R', 1))], '', [], 0, '',
     0),
    ('Unyielding', 'T', ['parry'], 'Increases parry rating by {R}.', [(MOD_RATING, PARRY, ('R', 1))], '', [], 0, '',
     0),
    ('Shieldbearer', 'T', ['shield', 'block'], 'Increases the block value of your shield by {R75}.',
     [(MOD_SHIELD_BLOCKVALUE, 0, ('R', 0.75))], '', [], 0, '', 0),
    ('Watchful Bastion', 'T', [], 'A damaging hit on you has a chance to grant {R250} dodge rating for 8 sec.',
     [], 'taken', [(MOD_RATING, DODGE, ('R', 2.5))], 8, '', 0),
    ('Tempered Plate', 'T', [], 'A damaging hit on you has a chance to grant armor equal to 8% of this item\'s for 8 '
     'sec.', [], 'taken', [(MOD_RESISTANCE, ARMOR, ('C', 1))], 8, 'armor', 8),
    ('Iron Resolve', 'T', [], 'A damaging hit on you has a chance to grant {R250} defense rating for 8 sec.',
     [], 'taken', [(MOD_RATING, DEFENSE, ('R', 2.5))], 8, '', 0),
    ('Braced Impact', 'T', [], 'A damaging hit on you has a chance to shield you for 2% of your maximum health for 6 '
     'sec.', [], 'taken', [(SCHOOL_ABSORB, ALL_MASK, ('C', 1))], 6, 'absorb_self', 2),
    ('Returning Guard', 'T', ['shield', 'block'], 'Blocking a hit has a chance to grant {R50} block value for 8 sec.',
     [], 'blocked', [(MOD_SHIELD_BLOCKVALUE, 0, ('R', 0.5))], 8, '', 0),
    ('Lasting Stand', 'T', [], 'Taking damage below 40% health has a chance to grant {R250} dodge rating for 8 sec.',
     [], 'taken_low', [(MOD_RATING, DODGE, ('R', 2.5))], 8, '', 0),
    # Healer.
    ('Radiant Wisdom', 'H', [], 'Increases spell power by {P}.', auras(SP, ('P', 1)), '', [], 0, '', 0),
    ('Measured Recovery', 'H', ['mana'], 'Restores {R20} mana every 5 sec.', [(MOD_POWER_REGEN, MANA, ('R', 0.2))],
     '', [], 0, '', 0),
    ('Healing Focus', 'H', ['heal'], 'Increases critical strike rating by {R}.', [(MOD_RATING, CRIT, ('R', 1))], '',
     [], 0, '', 0),
    ('Quickened Prayer', 'H', ['heal'], 'Increases haste rating by {R}.', [(MOD_RATING, HASTE, ('R', 1))], '', [], 0,
     '', 0),
    ('Gentle Remedy', 'H', ['heal'], 'Increases your direct healing by 2%.', [(DUMMY, 0, ('C', 2))], '', [], 0,
     'direct_heal', 2),
    ('Renewing Touch', 'H', ['heal'], 'Increases your healing over time by 2%.', [(DUMMY, 0, ('C', 2))], '', [], 0,
     'periodic_heal', 2),
    ("Mender's Insight", 'H', ['heal'], 'Your direct heals have a chance to grant {P250} spell power for 8 sec.', [],
     'heal', auras(SP, ('P', 2.5)), 8, '', 0),
    ('Healing Rhythm', 'H', ['heal'], 'Your direct heals have a chance to grant {R250} haste rating for 8 sec.', [],
     'heal', [(MOD_RATING, HASTE, ('R', 2.5))], 8, '', 0),
    ('Bright Benediction', 'H', ['heal'], 'Your direct heals have a chance to grant {R250} critical strike rating for '
     '8 sec.', [], 'heal', [(MOD_RATING, CRIT, ('R', 2.5))], 8, '', 0),
    ('Protective Grace', 'H', ['heal'], 'Your direct heals have a chance to shield their target for 1.5% of your '
     'maximum health for 6 sec.', [], 'heal', [(SCHOOL_ABSORB, ALL_MASK, ('C', 1))], 6, 'absorb_target', 1.5),
    ('Restorative Clarity', 'H', ['heal', 'mana'], 'Your direct heals have a chance to restore 1% of your maximum '
     'mana.', [], 'heal', [], 0, 'mana', 1),
    ('Emergency Response', 'H', ['heal'], 'Your direct heals on an ally below 40% health have a chance to grant {P75} '
     'spell power for 8 sec.', [], 'heal_low', auras(SP, ('P', 0.75)), 8, '', 0),
    # Melee.
    ("Veteran's Edge", 'M', [], 'Increases attack power by {A}.', auras(AP, ('A', 1)), '', [], 0, '', 0),
    ('Keen Blades', 'M', [], 'Increases critical strike rating by {R}.', [(MOD_RATING, CRIT, ('R', 1))], '', [], 0,
     '', 0),
    ('Swift Hands', 'M', [], 'Increases haste rating by {R}.', [(MOD_RATING, HASTE, ('R', 1))], '', [], 0, '', 0),
    ('Piercing Force', 'M', ['physical'], 'Increases armor penetration rating by {R}.',
     [(MOD_RATING, ARMOR_PEN, ('R', 1))], '', [], 0, '', 0),
    ('Focused Strikes', 'M', [], 'Increases the damage of your melee abilities by 2%.', [(DUMMY, 0, ('C', 2))], '',
     [], 0, 'melee_ability', 2),
    ('Finely Honed', 'M', [], 'Increases your melee auto-attack damage by 3%.', [(DUMMY, 0, ('C', 3))], '', [], 0,
     'auto_attack', 3),
    ('Bloodrush', 'M', [], 'Your melee hits have a chance to grant {A250} attack power for 8 sec.', [], 'melee',
     auras(AP, ('A', 2.5)), 8, '', 0),
    ('Relentless Tempo', 'M', [], 'Your melee hits have a chance to grant {R250} haste rating for 8 sec.', [],
     'melee', [(MOD_RATING, HASTE, ('R', 2.5))], 8, '', 0),
    ('Deadly Opening', 'M', [], 'Your melee hits have a chance to grant {L200} critical strike rating for 5 sec.', [],
     'melee', [(MOD_RATING, CRIT, ('L', 200))], 5, '', 0),
    ('Combat Readiness', 'M', [], 'Your melee hits have a chance to grant {R250} critical strike rating for 8 sec.',
     [], 'melee', [(MOD_RATING, CRIT, ('R', 2.5))], 8, '', 0),
    ('Thundering Strike', 'M', [], 'Your melee hits have a chance to deal {A150} Physical damage.', [], 'melee', [],
     0, 'strike', 1.5),
    ('Endless Motion', 'M', ['energy'], 'Increases Energy regeneration by 15%.',
     [(MOD_POWER_REGEN_PERCENT, ENERGY, ('C', 15))], '', [], 0, '', 0),
    # Ranged.
    ('Arcane Power', 'R', ['spell'], 'Increases spell power by {P}.', auras(SP, ('P', 1)), '', [], 0, '', 0),
    ('Marksman Power', 'R', ['ranged'], 'Increases attack power by {A}.', auras(AP, ('A', 1)), '', [], 0, '', 0),
    ('Far-Sight', 'R', [], 'Increases critical strike rating by {R}.', [(MOD_RATING, CRIT, ('R', 1))], '', [], 0, '',
     0),
    ('Swift Casting', 'R', ['spell'], 'Increases haste rating by {R}.', [(MOD_RATING, HASTE, ('R', 1))], '', [], 0,
     '', 0),
    ('Steady Aim', 'R', ['ranged'], 'Increases ranged attack speed by 2%.', [(MOD_RANGED_HASTE, 0, ('C', 2))], '',
     [], 0, '', 0),
    ("Spellweaver's Focus", 'R', ['spell'], 'Increases the damage of your direct damaging spells by 2%.',
     [(DUMMY, 0, ('C', 2))], '', [], 0, 'direct_spell', 2),
    ("Hunter's Precision", 'R', ['ranged'], 'Increases the damage of your ranged abilities by 2%.',
     [(DUMMY, 0, ('C', 2))], '', [], 0, 'ranged_ability', 2),
    ('Arcane Surge', 'R', ['spell'], 'Your damaging spells have a chance to grant {P250} spell power for 8 sec.', [],
     'spell', auras(SP, ('P', 2.5)), 8, '', 0),
    ('Trueflight', 'R', ['ranged'], 'Your ranged attacks have a chance to grant {A250} attack power for 8 sec.', [],
     'ranged', auras(AP, ('A', 2.5)), 8, '', 0),
    ('Focused Volley', 'R', ['ranged'], 'Your ranged attacks have a chance to grant {R250} haste rating for 8 sec.',
     [], 'ranged', [(MOD_RATING, HASTE, ('R', 2.5))], 8, '', 0),
    ('Emberflash', 'R', ['spell'], 'Your damaging spells have a chance to grant {L200} critical strike rating for 5 '
     'sec.', [], 'spell', [(MOD_RATING, CRIT, ('L', 200))], 5, '', 0),
    ('Lasting Affliction', 'R', ['spell'], 'Increases the damage of your spells over time by 2%.',
     [(DUMMY, 0, ('C', 2))], '', [], 0, 'periodic_spell', 2),
    # Hybrid and shared.
    ('Adaptive Potency', 'X', ['caster_pick'], 'Increases spell power by {P}.', auras(SP, ('P', 1)), '', [], 0, '',
     0),
    ('Adaptive Potency', 'X', ['physical_pick'], 'Increases attack power by {A}.', auras(AP, ('A', 1)), '', [], 0,
     '', 0),
    ('Keen Instinct', 'X', [], 'Increases critical strike rating by {R}.', [(MOD_RATING, CRIT, ('R', 1))], '', [], 0,
     '', 0),
    ('Quickening', 'X', [], 'Increases haste rating by {R}.', [(MOD_RATING, HASTE, ('R', 1))], '', [], 0, '', 0),
    ('Steady Vitality', 'X', [], 'A damaging hit on you has a chance to shield you for 1% of your maximum health for '
     '6 sec.', [], 'taken', [(SCHOOL_ABSORB, ALL_MASK, ('C', 1))], 6, 'absorb_self', 1),
    ('Arcane Reserve', 'X', ['mana'], 'Increases mana regeneration by 5%.',
     [(MOD_POWER_REGEN_PERCENT, MANA, ('C', 5))], '', [], 0, '', 0),
    ('Relentless Flow', 'X', ['focus'], 'Increases Focus regeneration by 8%.',
     [(MOD_POWER_REGEN_PERCENT, FOCUS, ('C', 8))], '', [], 0, '', 0),
    ('Adaptive Surge', 'X', ['caster_pick'], 'Your direct hits and heals have a chance to grant {P} spell power for '
     '8 sec.', [], 'direct', auras(SP, ('P', 1)), 8, '', 0),
    ('Adaptive Surge', 'X', ['physical_pick'], 'Your direct hits and heals have a chance to grant {A} attack power '
     'for 8 sec.', [], 'direct', auras(AP, ('A', 1)), 8, '', 0),
    ('Reinforced Recovery', 'X', ['tank'], 'Healing yourself has a chance to grant {R250} defense rating for 8 sec.',
     [], 'self_heal', [(MOD_RATING, DEFENSE, ('R', 2.5))], 8, '', 0),
]
POOLS = {'T': 1, 'H': 2, 'M': 4, 'R': 8, 'X': 16}


def scale(item_level):
    return min(2.0, max(0.25, item_level / 90.0))


def value(kind, multiplier, item_level):
    L = scale(item_level)
    base = {'P': 225 * L, 'A': 335 * L, 'R': 160 * L, 'L': L, 'C': 1.0}[kind]
    return round(round(base) * multiplier) if kind in 'PAR' else round(base * multiplier)


def text(effect, item_level):
    """The effect's words for a piece of the item level: its numbers its own."""
    words = effect[3]
    for key, kind, mult in (('{P}', 'P', 1), ('{P250}', 'P', 2.5), ('{P75}', 'P', 0.75), ('{A}', 'A', 1),
                            ('{A250}', 'A', 2.5), ('{A150}', 'A', 1.5), ('{R}', 'R', 1), ('{R250}', 'R', 2.5),
                            ('{R75}', 'R', 0.75), ('{R50}', 'R', 0.5), ('{R20}', 'R', 0.2), ('{L200}', 'L', 200)):
        words = words.replace(key, str(value(kind, mult, item_level)))
    return words


def buff_index():
    """Each proc effect's buff spell (its index among the procs that have one)."""
    out, n = {}, 0
    for i, e in enumerate(EFFECTS):
        if e[5] and e[6]:
            out[i] = BUFF_FIRST + n
            n += 1
    return out


def write_header():
    buffs = buff_index()
    lines = [
        '// Generated by tools/scaling/firepit_effects.py: the Fireforged bonus effects. Do not edit.',
        '#ifndef MOD_TITAN_FIREFORGED_EFFECTS_H',
        '#define MOD_TITAN_FIREFORGED_EFFECTS_H',
        '',
        '#include "Define.h"',
        '',
        'namespace FireforgedEffects',
        '{',
        f'    constexpr uint32 EQUIP_SPELL_FIRST = {EQUIP_SPELL_FIRST}, BUFF_FIRST = {BUFF_FIRST};',
        f'    constexpr uint32 STRIKE_SPELL = {STRIKE_SPELL}, ENCHANT_FIRST = {ENCHANT_FIRST};',
        '    enum Pool : uint8 { ' + ', '.join(f'POOL_{k} = {v}' for k, v in
                                         (('TANK', 1), ('HEALER', 2), ('MELEE', 4), ('RANGED', 8), ('HYBRID', 16)))
        + ' };',
        '    enum Need : uint16',
        '    {',
        *[f'        NEED_{k.upper()} = {v},' for k, v in NEEDS.items()],
        '    };',
        '    enum Trigger : uint8',
        '    {',
        *[f'        TRIGGER_{(k or "none").upper()} = {v},' for k, v in TRIGGERS.items()],
        '    };',
        '    enum Special : uint8',
        '    {',
        *[f'        SPECIAL_{(k or "none").upper()} = {v},' for k, v in SPECIALS.items()],
        '    };',
        '    // A value: its kind (P spell power, A attack power, R rating, L the 200 x L bursts, C a constant) and',
        '    // its multiplier.',
        '    struct Value',
        '    {',
        '        char Kind;',
        '        float Multiplier;',
        '    };',
        '    // An effect: its equip spell, its buff (0: none), pool, needs, trigger, special and its value, and the',
        '    // values of its equip aura\'s and its buff\'s effects (up to two each).',
        '    struct Effect',
        '    {',
        '        uint32 Spell;',
        '        uint32 Buff;',
        '        uint8 Pool;',
        '        uint16 Needs;',
        '        uint8 Trigger;',
        '        uint8 Special;',
        '        float SpecialValue;',
        '        Value Equip[2];',
        '        Value Proc[2];',
        '    };',
        '    constexpr Effect EFFECTS[] = {',
    ]
    for i, e in enumerate(EFFECTS):
        name, pool, needs, words, equip, trigger, buff, duration, special, special_value = e
        eq = [f"{{ '{v[0]}', {float(v[1])}f }}" for _, _, v in equip] + ["{ 'C', 0.0f }"] * 2
        pr = [f"{{ '{v[0]}', {float(v[1])}f }}" for _, _, v in buff] + ["{ 'C', 0.0f }"] * 2
        lines.append(f"        {{ {EQUIP_SPELL_FIRST + i}, {buffs.get(i, 0)}, {POOLS[pool]}, "
                     f"{sum(NEEDS[n] for n in needs)}, {TRIGGERS[trigger]}, {SPECIALS[special]}, "
                     f"{float(special_value)}f, {{ {eq[0]}, {eq[1]} }}, {{ {pr[0]}, {pr[1]} }} }},   // {name}")
    lines += ['    };', '}', '', '#endif', '']
    HEADER.write_text('\n'.join(lines))
    print(f'{len(EFFECTS)} effects, {len(buffs)} buffs: {HEADER.name}')


if __name__ == '__main__':
    write_header()
