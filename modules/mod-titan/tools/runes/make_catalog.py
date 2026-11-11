#!/usr/bin/env python3
"""Emberforge_Runes/Catalog.lua from the "Emberforge Legendary Runes" design doc (claude.ai doc S8Hd5pfSTncJWYMxvsAvMR):
its first tab exported as plain text (headings as "## "/"### " lines, table cells as " | "-separated lines), every
rune with its core, loop, nine modifiers and three builds, in the doc's order (src/runes.cpp numbers them so).

Usage: python3 make_catalog.py <exported tab .txt> <out Catalog.lua>
"""
import re, sys
L=[l.strip() for l in open(sys.argv[1]).read().split('\n')]
ov=[]
k=L.index('Resource')+1
while not L[k].startswith('##'):
    if L[k]=='|':
        ov.append(dict(name=L[k+1].rstrip(' |'),role=L[k+2].rstrip(' |'),built=L[k+3].rstrip(' |'),
                       spent=L[k+4].rstrip(' |'),res=L[k+5])); k+=6
    else: k+=1
assert len(ov)==21
icons={'Rune of the Unbroken Wall':'Ability_Warrior_ShieldMastery','Rune of the Bloodbound':'Spell_Deathknight_RuneTap',
'Rune of the Soulbrand':'Ability_DemonHunter_SoulCleave','Rune of the Sentinel':'Spell_Holy_AvengersShield',
'Rune of the Lightbringer':'Spell_Holy_SearingLight','Rune of the Penitent':'Spell_Holy_PowerWordShield',
'Rune of the Tidecaller':'Spell_Nature_HealingWaveGreater',
'Rune of the Grove':'Spell_Nature_Rejuvenation',
'Rune of the Jade Serpent':'Ability_Monk_SoothingMists','Rune of the Arcane Mind':'Spell_Arcane_Blast',
'Rune of the Pyre':'Spell_Fire_Fireball02','Rune of the Void':'Spell_Shadow_UnholyFrenzy',
'Rune of the Storm Caller':'Spell_Nature_Lightning','Rune of the Soulreaper':'Spell_Shadow_LifeDrain02',
'Rune of the Deadeye':'Ability_Hunter_AimedShot','Rune of the Warbringer':'Ability_Warrior_SavageBlow',
'Rune of the Shadowblade':'Ability_Backstab','Rune of the Crusade':'Spell_Holy_CrusaderStrike',
'Rune of the Fel Hunger':'Ability_DemonHunter_EyeBeam','Rune of the Fierce Tiger':'Ability_Monk_RisingSunKick',
'Rune of Fel Commandment':'Spell_Shadow_DemonicTactics'}
runes=[]
for r in ov:
    s=L.index('### '+r['name']); e=s+1
    while e<len(L) and not L[e].startswith('##'): e+=1
    blk=L[s+1:e]
    core=next(x for x in blk if x.startswith('Core.'))[5:].strip()
    loop=next(x for x in blk if x.startswith('Loop.'))[5:].strip()
    mods=[]; j=blk.index('Effect')+1
    while j<len(blk):
        if blk[j]=='|' and j+3<len(blk) and blk[j+1].rstrip(' |') in ('Invocation','Manifestation','Adaptation'):
            mods.append((blk[j+2].rstrip(' |'),blk[j+3])); j+=4; continue
        j+=1
    assert len(mods)==9
    builds=[]; bl=[x for x in blk if x.startswith('Builds.')]
    if len(bl[0])>8:
        for a,b in re.findall(r'([A-Z][^:.]*?): ([^.]+)\.', bl[0][7:]):
            builds.append((a.strip(),[m.strip() for m in b.split('+')],''))
    else:
        for x in blk[blk.index('Builds.')+1:]:
            m=re.match(r'([^.]+)\. ([^.]+?)\. (.*)',x)
            if m and '+' in m.group(2):
                builds.append((m.group(1), [y.strip() for y in m.group(2).split('+')], m.group(3)))
    assert len(builds)==3
    runes.append(dict(r,core=core,loop=loop,mods=mods,builds=builds,icon=icons[r['name']]))

def lit(s): return '"'+s.replace('\\','\\\\').replace('"','\\"')+'"'
def field(ind, key, text, comma=','):
    """key = "text", wrapped into concatenated pieces under 120 columns."""
    head=f'{ind}{key} = '
    words=text.split(' '); lines=[]; cur=''; prefix=head; width=114
    for w in words:
        cand=(cur+' '+w) if cur else w
        if len(prefix)+len(cand)+6>width and cur:
            lines.append(prefix+lit(cur+' ')+' ..'); prefix=ind+'    '; cur=w
        else: cur=cand
    lines.append(prefix+lit(cur)+comma)
    return lines
out=['-- The Legendary Runes, generated from the "Emberforge Legendary Runes" design doc. src/runes.cpp numbers them '
     'in',
     '-- this order; the server builds rune 21 (Fel Commandment) so far. Modifiers: 3 per slot, in slot order.',
     'EmberforgeRunesCatalog = {']
for i,r in enumerate(runes,1):
    out.append(f'    {{ id = {i},')
    for key in ('name','role','icon'): out += field('      ', key, r[key])
    out += field('      ','builtFrom',r['built']); out += field('      ','spentWith',r['spent'])
    out += field('      ', 'resource', r['res'])
    out += field('      ', 'core', r['core'])
    out += field('      ', 'loop', r['loop'])
    out.append('      mods = {')
    for name,text in r['mods']:
        out.append('        {')
        out += field('          ', 'name', name)
        out += field('          ', 'text', text)
        out.append('        },')
    out.append('      },')
    out.append('      builds = {')
    for name,mods,desc in r['builds']:
        out.append('        {'); out += field('          ','name',name)
        out.append('          mods = { '+', '.join(lit(m) for m in mods)+' },')
        if desc: out += field('          ','text',desc)
        out.append('        },')
    out.append('      },')
    out.append('    },')
out.append('}')
open(sys.argv[2],'w').write('\n'.join(out)+'\n')
print(len(runes))
