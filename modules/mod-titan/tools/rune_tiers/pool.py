"""Runes of Enhancement: the Wildcard ability pool (class trainers' spells, active talents, our Demon Hunter and
Monk spells) and, for each combat ability, the Uncommon prefixes it can take (run by generate.py).

Prefixes: Rapid (it has a cast time), Aggressive (it deals damage), Critical (a direct hit or heal that can crit),
Refreshing (a cooldown over the global one), Provoking (it deals damage or forces threat: offered to tanks only).
Mending: it heals or shields. Utility (conjures, teleports, portals, long buffs, stances, forms, aspects, tracking,
pets' upkeep, resurrection) is out.
"""
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
CP = ROOT / 'modules/mod-titan/client-patch'
sys.path.insert(0, str(CP))
from patch_spells import Dbc, SPELL_FIELDS, FIELDS, EFFECT_FIELDS, NAME  # noqa: E402

SQL = ROOT / 'data/sql/base/db_world'
PATCHED, DBC = Path(sys.argv[1]), Path(sys.argv[2])         # patch_spells.py's output, the original DBCs
spells = Dbc(PATCHED / 'Spell.dbc', SPELL_FIELDS)            # patched: our own spells included
CLASSES = {1: 'Warrior', 2: 'Paladin', 3: 'Hunter', 4: 'Rogue', 5: 'Priest', 6: 'Death Knight', 7: 'Shaman',
           8: 'Mage', 9: 'Warlock', 11: 'Druid'}


def rows(path):
    text = path.read_text(errors='replace')
    return [tuple(v.strip("'") for v in re.findall(r"'(?:[^'\\]|\\.)*'|[^,]+", t))
            for t in re.findall(r'\(([^()]*)\)', text.split('INSERT INTO', 1)[1])]


def f(row, name, n=None):
    if n is None:
        return row[FIELDS[name]]
    col, flt = EFFECT_FIELDS[name]
    v = row[col + n]
    return struct.unpack('<f', struct.pack('<I', v))[0] if flt else (v - (1 << 32) if v >= 1 << 31 else v)


def name(sid):
    return spells.string(spells.rows[sid][NAME]) if sid in spells.rows else ''


# Ranks.
first_of, chain = {}, {}
for first, sid, rank in ((int(a), int(b), int(c)) for a, b, c in rows(SQL / 'spell_ranks.sql')):
    first_of[sid] = first
    chain.setdefault(first, []).append((rank, sid))
for k in chain:
    chain[k] = [s for _, s in sorted(chain[k])]


def first(sid):
    return first_of.get(sid, sid)


def taught(sid):
    row = spells.rows.get(sid)
    if not row:
        return []
    out = [f(row, 'TriggerSpell', n) for n in range(3) if f(row, 'Effect', n) == 36 and f(row, 'TriggerSpell', n)]
    return out or [sid]


# The pool: class trainers' spells, by class.
trainer_class = {int(r[0]): int(r[2]) for r in rows(SQL / 'trainer.sql') if int(r[1]) == 0}
pool = {}
for r in rows(SQL / 'trainer_spell.sql'):
    tid, sid = int(r[0]), int(r[1])
    if tid in trainer_class:
        for s in taught(sid):
            pool.setdefault(first(s), trainer_class[tid])
# Active talents (the Wildcard teaches them as abilities).
talents = Dbc(DBC / 'Talent.dbc', 23)
tabs = Dbc(DBC / 'TalentTab.dbc', 24)
tab_class = {}
for tab in tabs.rows.values():
    mask = tab[20]
    for cid in CLASSES:
        if mask == 1 << (cid - 1):
            tab_class[tab[0]] = cid
for t in talents.rows.values():
    for sid in t[4:9]:
        if sid and t[1] in tab_class:
            for s in taught(sid):
                pool.setdefault(first(s), tab_class[t[1]])
# Our Demon Hunter (901xx) and Monk (903xx) spells.
for sid, row in spells.rows.items():
    if 90100 <= sid < 90200:
        pool.setdefault(first(sid), 'DH')
    elif 90300 <= sid < 90400:
        pool.setdefault(first(sid), 'MONK')

PASSIVE, HIDDEN_CLIENT = 0x40, 0x80
DAMAGE_EFFECTS = {2, 9, 17, 31, 58, 121, 7}              # school, leech, weapon (+%, normalized), environmental
HEAL_EFFECTS = {10, 67, 136}
THREAT_EFFECTS = {63, 114, 25}                           # threat, attack me (taunt)
DAMAGE_AURAS = {3, 53, 89}                               # periodic damage, leech, damage percent
HEAL_AURAS = {8, 69, 20}                                 # periodic heal, absorb, heal percent
UTILITY_NAMES = re.compile(r'^(Conjure|Teleport|Portal|Ritual|Summon (?!Felguard)|Track|Find |Sense |Detect|Eagle Eye|'
                           r'Feed Pet|Revive Pet|Dismiss|Call Pet|Tame|Beast (Lore|Training)|Mend Pet|Create|Enchant|'
                           r'Resurrect|Revive|Redemption|Ancestral Spirit|Rebirth|Raise Ally|Soulstone|Hearthstone|'
                           r'Mount|Ghost Wolf|Travel Form|Aquatic Form|Flight Form|Swift Flight|'
                           r'Water (Walking|Breathing)|'
                           r'Unending Breath|Levitate|Slow Fall|Feather|Pick |Disarm Trap|Remove Insignia|Ritual|'
                           r'Dalaran|Arcane Brilliance|Arcane Intellect|Mark of the Wild|Gift of the Wild|'
                           r'Power Word: Fortitude|Prayer of|Divine Spirit|Shadow Protection|Blessing|Greater Blessing|'
                           r'Thorns|Amplify|Dampen|Detect|Sense|Fishing|Cooking|First Aid|Seal of |Aura$|Presence|'
                           r'Aspect|Stance|Form$|Totem of Wrath$|Horn of Winter|Battle Shout|Commanding Shout|'
                           r'Path of Frost|Death Gate|Soulshatter|Lesser Invisibility|Fade|Feint|Distract|Sprint|'
                           r'Cat Form|Bear Form|Dire Bear|Moonkin|Tree of Life|Shadowform|Demon (Armor|Skin)|Fel Armor|'
                           r'Frost Armor|Ice Armor|Mage Armor|Molten Armor|Inner Fire|Lightning Shield$|Water Shield|'
                           r'Earth Shield$|Righteous Fury|Trueshot Aura|Hunter\'s Mark|Scorpid Sting|Shadowmeld|'
                           r'Stealth|Prowl|Vanish|Hysteria|Rune Tap$|Summon$)')
KEEP = {'Lightning Shield', 'Earth Shield', 'Hunter\'s Mark', 'Rune Tap', 'Vanish', 'Feint', 'Mend Pet'}

cast_times = {r[0]: r[1] for r in Dbc(DBC / 'SpellCastTimes.dbc', 4).rows.values()}
durations = {r[0]: r[1] if r[1] < 1 << 31 else -1 for r in Dbc(DBC / 'SpellDuration.dbc', 4).rows.values()}


def info(sid):
    """The whole chain's facts (an effect in any rank counts), read from its top rank for times."""
    ids = chain.get(sid, [sid])
    top = spells.rows[ids[-1]]
    effects, auras = set(), set()
    for s in ids:
        row = spells.rows.get(s)
        if not row:
            continue
        for n in range(3):
            e, a = f(row, 'Effect', n), f(row, 'Aura', n)
            if e:
                effects.add(e)
            if e in (6, 35, 65, 119, 128, 129, 143) and a:
                auras.add(a)
            trig = f(row, 'TriggerSpell', n)
            if (e == 64 or a in (23, 227)) and trig in spells.rows:          # trigger spell: its effects count
                t = spells.rows[trig]
                for m in range(3):
                    if f(t, 'Effect', m):
                        effects.add(f(t, 'Effect', m))
                    if f(t, 'Aura', m):
                        auras.add(f(t, 'Aura', m))
    cast = cast_times.get(f(top, 'CastingTimeIndex'), 0)
    cast = cast if cast < 1 << 31 else 0
    cd = max(f(top, 'RecoveryTime'), f(top, 'CategoryRecoveryTime'))
    dur = durations.get(f(top, 'DurationIndex'), 0)
    channel = bool(top[FIELDS['AttributesEx']] & (0x4 | 0x40))
    return {'effects': effects, 'auras': auras, 'cast': cast, 'cd': cd, 'dur': dur, 'channel': channel,
            'passive': bool(top[FIELDS['Attributes']] & PASSIVE), 'dmgclass': f(top, 'DmgClass')}


ORDER = ['Rapid', 'Aggressive', 'Mending', 'Critical', 'Refreshing', 'Provoking']
A, C, P, R, F, M = 'Aggressive', 'Critical', 'Provoking', 'Refreshing', 'Rapid', 'Mending'
# Combat abilities the data hides (scripted damage, procs, totems, heals over time): their prefixes by hand.
FORCE = {
    'Spirit Bomb': [A, C, P], 'Death Coil': [A, M, C, P], 'Summon Gargoyle': [R], 'Bone Shield': [R],
    'Rejuvenation': [M], 'Lifebloom': [M, C], 'Faerie Fire (Feral)': [R, P], 'Cyclone': [F], 'Hibernate': [F],
    "Nature's Swiftness": [R], 'Wing Clip': [A, C, P], 'Distracting Shot': [R, P], 'Feign Death': [R],
    'Mend Pet': [M], 'Soothing Mist': [M], 'Combustion': [R], 'Polymorph': [F], 'Presence of Mind': [R],
    'Summon Water Elemental': [R], 'Divine Favor': [R], 'Sacred Shield': [M], 'Seal of Righteousness': [A, P],
    'Seal of Command': [A, C, P], 'Seal of Vengeance': [A, P], 'Seal of Corruption': [A, P],
    'Renew': [M], 'Prayer of Healing': [F, M, C], 'Prayer of Mending': [M, C, R], 'Inner Focus': [R],
    'Fade': [R], 'Mind Control': [F], 'Shackle Undead': [F], 'Shiv': [A, C, P], 'Cold Blood': [R],
    'Earth Shield': [M], 'Lightning Shield': [A, P], 'Searing Totem': [A, C, P], 'Magma Totem': [A, P],
    'Healing Stream Totem': [M], 'Fear': [F], 'Banish': [F], 'Execute': [A, C, P], 'Slam': [F, A, C, P],
    'Hamstring': [A, C, P], 'Blade Dance': [A, C, R, P], 'Starfall': [A, C, R, P], 'Penance': [A, M, C, R, P],
}
# Passives, procs and pieces of others (not buttons a player presses).
DROP = {'Demonic Appetite', 'Feast of Souls', 'Frailty', 'Soul Rending', 'Bloodlet', 'Trail of Ruin', 'Demon Blades',
        'Chaos Cleave', 'Thunderfist', 'Fire Spirit', 'Earth Spirit'}
catalog = {}
skipped = []
for sid, cls in pool.items():
    if sid not in spells.rows:
        continue
    nm = name(sid)
    if not nm or nm in {n for c in catalog.values() for n in c}:
        continue
    i = info(sid)
    if i['passive']:
        continue
    damage = bool(i['effects'] & DAMAGE_EFFECTS or i['auras'] & DAMAGE_AURAS)
    heal = bool(i['effects'] & HEAL_EFFECTS or i['auras'] & HEAL_AURAS)
    threat = bool(i['effects'] & THREAT_EFFECTS)
    direct = bool(i['effects'] & (DAMAGE_EFFECTS | HEAL_EFFECTS))
    long_buff = i['dur'] < 0 or i['dur'] >= 5 * 60 * 1000
    combat = damage or heal or threat or (i['cd'] >= 3000 and not long_buff)
    if nm in DROP:
        continue
    if nm in FORCE:
        catalog.setdefault(CLASSES.get(cls, {'DH': 'Demon Hunter', 'MONK': 'Monk'}.get(cls, cls)), {})[nm] = {
            'spell': sid, 'prefixes': FORCE[nm], 'heal': M in FORCE[nm] and A not in FORCE[nm], 'cast': i['cast'],
            'cd': i['cd']}
        continue
    if (UTILITY_NAMES.search(nm) and nm not in KEEP) or not combat or (long_buff and not damage and not heal):
        skipped.append((CLASSES.get(cls, cls), nm))
        continue
    prefixes = []
    if i['cast'] > 0 and not i['channel']:
        prefixes.append('Rapid')
    if damage:
        prefixes.append('Aggressive')
    if heal:
        prefixes.append('Mending')
    if direct and (damage or heal):
        prefixes.append('Critical')
    if i['cd'] > 1500:
        prefixes.append('Refreshing')
    if damage or threat:
        prefixes.append('Provoking')
    if not prefixes:
        skipped.append((CLASSES.get(cls, cls), nm))
        continue
    prefixes = sorted(set(prefixes), key=ORDER.index)
    catalog.setdefault(CLASSES.get(cls, {'DH': 'Demon Hunter', 'MONK': 'Monk'}.get(cls, cls)), {})[nm] = {
        'spell': sid, 'prefixes': prefixes, 'heal': heal and not damage, 'cast': i['cast'], 'cd': i['cd']}

print({k: len(v) for k, v in catalog.items()}, 'total', sum(len(v) for v in catalog.values()), 'skipped', len(skipped))
