#!/usr/bin/env python3
"""The Dungeon Journal's art and texts, from a Mists of Pandaria client (tools/extract/EmberforgeExtract.exe).

Reads the client's Journal tables (DBFilesClient: JournalInstance, JournalEncounter, JournalEncounterCreature,
JournalEncounterSection, with FileData for their art) and writes, for the instances the 3.3.5a game has
(Classic, the Burning Crusade, Wrath of the Lich King, and the old Zul'Aman, Deadmines and Shadowfang Keep):

- client-patch/AddOns/Emberforge_Journal/Data.lua:
    EmberforgeJournalArt[map]        { button = tile art, lore = the instance's picture } (files in Art/), or
                                     { screen = its loading screen } (the game's own)
    EmberforgeJournalLore[map]       the instance's description
    EmberforgeJournalBosses[map]     { [boss key] = { d = description, p = portrait, s = { sections } } }
                                     sections: { t = title, b = body, i = icon (Interface\\Icons\\),
                                                 f = icon flags, l = depth }
    EmberforgeJournalPortraits[key]  every boss portrait, by its boss's key (the art's own name)
  A boss key is its name, lowercase, letters and digits only; the addon matches the server's boss names to it.
- client-patch/AddOns/Emberforge_Journal/Art/: the art those name (the client's own BLPs, as they are; the 3.3.5a
  client reads them), UI-EJ-Icons (the ability flags' icons, re-encoded) and UI-EJ-LOREBG-Frame (the pictures'
  frame, empty: a loading screen goes in it).

The abilities' numbers ($12345s1, $d, ...) are written out from the spell tables: the 3.3.5a client's Spell.dbc
when the spell is there (the game's own values), else Mists of Pandaria's.

The instances Cataclysm rebuilt in place (Deadmines, Shadowfang Keep) keep their art; their bosses and texts are
Cataclysm's, so they are left out. Raids the Journal of that client does not have get their loading screens.

Usage: python3 mop_journal.py <EmberforgeExtract folder> <3.3.5a DBC folder>
(the extract of patterns/journal-ui.txt merged into it: its Interface\\FrameGeneral art)
(the 3.3.5a DBCs: `docker cp ac-worldserver:/azerothcore/env/dist/data/dbc ./dbc`)
"""

import re
import shutil
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'client-patch' / 'models'))

from PIL import Image  # noqa: E402

import blp  # noqa: E402

ADDON = ROOT / 'client-patch' / 'AddOns' / 'Emberforge_Journal'

# Maps Cataclysm added (the Journal's Cataclysm instances that are not old maps): not in the game yet.
CATACLYSM_MAPS = {643, 644, 645, 657, 669, 670, 671, 720, 725, 754, 755, 757, 859, 938, 939, 940, 967}
# Rebuilt by Cataclysm on the same map: art only.
REBUILT = {33, 36}
# Old maps whose Journal entry moved to a rebuilt map: (old map, the Journal's art name).
OLD_ART = {289: 'SCHOLOMANCE', 309: 'ZULGURUB'}
# Old maps whose story is their rebuilt version's (the old one's is a placeholder, or missing): old map -> map.
OLD_STORY = {189: 1004, 289: 1007}
# Raids the Journal does not have: their loading screen (Interface\Glues\LoadingScreens\<name>, the 3.3.5a client's
# own, not shipped), framed by the Journal's picture frame (UI-EJ-LOREBG-Frame) on the instance's page.
LOADING_SCREENS = {
    249: 'LoadScreenRaid', 409: 'LoadScreenMoltenCore', 469: 'LoadScreenBlackWingLair',
    509: 'LoadScreenAhnQiraj20man', 531: 'LoadScreenAhnQiraj40man', 532: 'LoadScreenKarazhan',
    533: 'LoadScreenNaxxramas', 534: 'LoadScreenCavernsTime', 544: 'LoadScreenHellfireCitadelRaid',
    548: 'LoadScreenCoilFang', 550: 'LoadScreenTempestKeep', 564: 'LoadScreenBlackTemple',
    565: 'LoadScreenGruulsLair', 580: 'LoadScreenSunwell', 603: 'LoadScreenUlduarRaid',
    615: 'LoadScreenChamberBlack', 616: 'LoadScreenMalygos', 624: 'LoadScreenWintergrasp',
    631: 'LoadScreenIcecrownCitadel', 649: 'LoadScreenArgentRaid', 724: 'LoadScreenRubySanctum',
}
# The Journal's picture frame: an instance's picture (its burnt edge and gilt border; the same on all, tinted), the
# picture inside cut out (left, top, right, bottom, in the 512 by 512 file).
FRAME_SOURCE, FRAME_HOLE = 'ui-ej-lorebg-utgardekeep.blp', (27, 39, 362, 293)
SKIP_TIERS = {'Mists of Pandaria'}
# The game's boss names (DungeonEncounter.dbc) that differ from the Journal's: map -> {game's: Journal's}.
ALIASES = {
    209: {"Ghaz'rilla": "Gahz'rilla"},
    329: {'Cannon Master Willey': 'Willey Hopebreaker', 'Malor the Zealous': 'Commander Malor',
          'Archivist Galford': 'Instructor Galford', 'Ramnstein the Gorger': 'Ramstein the Gorger',
          'Baron Rivendare': 'Lord Aurius Rivendare'},
    574: {'Skarvold & Dalronn': 'Skarvald & Dalronn'},
    576: {'Ormrok the Tree-Shaper': 'Ormorok the Tree-Shaper', 'Frozen Commander': 'Commander Stoutbeard'},
    595: {'Salram the Fleshcrafter': 'Salramm the Fleshcrafter'},
    599: {'Tribunal of Ages': 'Tribunal of the Ages'},
    658: {'Overlord Tyrannus': 'Scourgelord Tyrannus'},
}


class Dbc:
    def __init__(self, path):
        data = path.read_bytes()
        _, count, fields, size, _ = struct.unpack('<4s4I', data[:20])
        self.strings = data[20 + count * size:]
        self.rows = [struct.unpack_from(f'<{fields}i', data, 20 + i * size) for i in range(count)]
        self.by_id = {row[0]: row for row in self.rows}

    def text(self, offset):
        if offset <= 0 or offset >= len(self.strings):
            return ''
        end = self.strings.find(b'\0', offset)
        return self.strings[offset:end].decode('utf8', 'replace')


def real(value):
    return struct.unpack('<f', struct.pack('<i', value))[0]


def key(name):
    return re.sub(r'[^a-z0-9]', '', name.lower())


class Spells:
    """A spell's numbers for the texts' $ variables: 3.3.5a's when it has the spell, else Mists of Pandaria's."""

    def __init__(self, mop, wotlk):
        self.old = Dbc(wotlk / 'Spell.dbc')
        self.old_duration = Dbc(wotlk / 'SpellDuration.dbc').by_id
        self.old_radius = Dbc(wotlk / 'SpellRadius.dbc').by_id
        self.old_icons = Dbc(wotlk / 'SpellIcon.dbc')
        self.mop = Dbc(mop / 'Spell.dbc').by_id
        self.misc = Dbc(mop / 'SpellMisc.dbc').by_id
        self.duration = Dbc(mop / 'SpellDuration.dbc').by_id
        self.radius = Dbc(mop / 'SpellRadius.dbc').by_id
        self.effects = {}
        for row in Dbc(mop / 'SpellEffect.dbc').rows:
            if row[1] == 0:     # the normal difficulty's
                self.effects.setdefault(row[27], {})[row[28]] = row
        self.icon_paths = {self.old_icons.text(row[1]).lower(): self.old_icons.text(row[1])
                           for row in self.old_icons.rows}

    def icon(self, spell):
        row = self.old.by_id.get(spell)
        icon = row and self.old_icons.by_id.get(row[133])
        return self.old_icons.text(icon[1]).split('\\')[-1] if icon else None

    def has_icon(self, path):
        found = self.icon_paths.get(path.lower())
        return found.split('\\')[-1] if found else None

    def value(self, spell, kind, index):
        """The text of $<spell><kind><index>, or None."""
        index = max(1, index or 1) - 1
        old = self.old.by_id.get(spell)
        if old:
            base, die = old[80 + index] + 0, old[74 + index]
            duration = self.old_duration.get(old[40])
            duration = duration[1] if duration else 0
            period = old[98 + index]
            radius = self.old_radius.get(old[92 + index])
            radius = real(radius[1]) if radius else 0
            chain = old[104 + index]
            extra = {'u': old[49], 'n': old[36], 'h': old[35]}
        else:
            effect = self.effects.get(spell, {}).get(index)
            row = self.mop.get(spell)
            if not effect and not row:
                return None
            misc = self.misc.get(row[24]) if row else None
            duration = self.duration.get(misc[18]) if misc else None
            duration = duration[1] if duration else 0
            base, die = (effect[6], effect[10]) if effect else (0, 0)
            period = effect[5] if effect else 0
            radius = self.radius.get(effect[16] or effect[17]) if effect else None
            radius = real(radius[1]) if radius else 0
            chain = effect[9] if effect else 0
            extra = {}
        low = base if die == 0 else base + 1
        high = base + max(die, 1)
        kind = kind.lower()
        if kind == 's':
            return f'{abs(low)}' if high <= low else f'{abs(low)} to {abs(high)}'
        if kind == 'm':
            return f'{abs(low)}'
        if kind == 'd':
            return seconds(duration / 1000) if duration > 0 else None
        if kind == 't':
            return number(period / 1000) if period else None
        if kind == 'a':
            return number(radius) if radius else None
        if kind == 'x':
            return str(chain) if chain else None
        if kind == 'o':
            if period and duration > 0:
                return str(abs(low) * int(duration / period))
            return None
        if kind in extra and extra[kind]:
            return str(extra[kind])
        return None


def read_bgra(path):
    """A BLP2 of uncompressed BGRA pixels (encoding 3, which PIL does not read): its first image."""
    data = path.read_bytes()
    width, height = struct.unpack_from('<II', data, 12)
    offset = struct.unpack_from('<I', data, 20)[0]
    b, g, r, a = Image.frombytes('RGBA', (width, height), data[offset:offset + width * height * 4]).split()
    return Image.merge('RGBA', (r, g, b, a))


def number(value):
    return str(int(value)) if value == int(value) else f'{value:g}'


def seconds(value):
    if value >= 3600 and value % 3600 == 0:
        return f'{int(value // 3600)} hr'
    if value >= 60 and value % 60 == 0:
        return f'{int(value // 60)} min'
    return f'{number(value)} sec'


TOKEN = re.compile(r'\$(\d*)([sSmMdDtTaAxXoOuUnNhHrRfFcC])(\d?)')


def resolve(text, spells):
    """The text with its $ variables written out (those that cannot be are dropped)."""
    text = text.replace('\r\n', '\n')
    text = re.sub(r'\$\[[\d, ]*', '', text).replace('$]', '')     # difficulty-only parts: shown, they say so
    first = re.search(r'\$(\d+)', text)
    owner = int(first.group(1)) if first else 0

    def token(match):
        spell = int(match.group(1)) if match.group(1) else owner
        value = spells.value(spell, match.group(2), int(match.group(3) or 1)) if spell else None
        return value if value is not None else ''

    def formula(match):
        inner = TOKEN.sub(lambda m: (token(m) or '0').split(' ')[0], match.group(1))
        if re.fullmatch(r'[\d.+\-*/() ]+', inner):
            try:
                return number(round(eval(inner), 1))   # noqa: S307 - digits and operators only
            except (SyntaxError, ZeroDivisionError):
                pass
        return ''

    text = re.sub(r'\$\{([^}]*)\}', formula, text)
    text = TOKEN.sub(token, text)
    text = re.sub(r'[ \t]+([.,%])', r'\1', text)
    text = re.sub(r'[ \t]{2,}', ' ', text)
    return re.sub(r'\n{3,}', '\n\n', text).strip()


def lua_string(text):
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'


def lua(value, indent=0):
    """Lua source for a value, its lines under 120 columns once indented by `indent`."""
    pad = ' ' * indent
    if isinstance(value, str):
        if len(value) + indent <= 100:
            return lua_string(value)
        pieces = [piece for piece in re.findall(r'.{1,76}(?:\s+|$)|.{1,76}', value, re.S) if piece]
        return '\n' + ' ..\n'.join(pad + '    ' + lua_string(piece) for piece in pieces)
    if isinstance(value, (int, float)):
        return number(value) if isinstance(value, float) else str(value)
    fields = [(name, item) for name, item in value.items() if item not in (None, '', 0)]
    line = '{ ' + ', '.join(f'{name} = {lua(item, 120)}' for name, item in fields) + ' }'
    if '\n' not in line and len(line) + indent <= 116:
        return line
    parts = []
    for name, item in fields:
        text = lua(item, indent + 4)
        parts.append(f'{pad}    {name} =' + (text if text.startswith('\n') else ' ' + text) + ',')
    return '{\n' + '\n'.join(parts) + '\n' + pad + '}'


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    extract, wotlk = Path(sys.argv[1]), Path(sys.argv[2])
    dbc = extract / 'DBFilesClient'
    art_files = {}      # lowercase file name -> its path in the extract
    for path in extract.rglob('*'):
        if path.suffix.lower() == '.blp' and path.parent.name.lower() in ('encounterjournal', 'lfgframe',
                                                                          'framegeneral'):
            art_files[path.name.lower()] = path

    file_data = Dbc(dbc / 'FileData.dbc')
    files = {row[0]: file_data.text(row[1]) for row in file_data.rows}
    tiers = Dbc(dbc / 'JournalTier.dbc')
    tier_of = {}
    for tier, instance in Dbc(dbc / 'JournalTierXInstance.dbc').rows:
        tier_of[instance] = tiers.text(tiers.by_id[tier][1])
    spells = Spells(dbc, wotlk)

    used = set()        # art file names (lowercase) to copy

    def art(name):
        if name and name.lower() in art_files:
            used.add(name.lower())
            return art_files[name.lower()].stem
        return None

    instances = Dbc(dbc / 'JournalInstance.dbc')
    maps = {}           # Journal instance -> map, for the ones written
    art_by_map, lore_by_map = {}, {}
    stories = {row[1]: resolve(instances.text(row[7]), spells) for row in instances.rows}
    for row in instances.rows:
        instance, map_id = row[0], row[1]
        # Cataclysm's new maps and Mists of Pandaria's (its rebuilt Scholomance and Scarlet Monastery among them)
        if map_id in CATACLYSM_MAPS or map_id >= 900 or tier_of.get(instance) in SKIP_TIERS:
            continue
        button, lore = files.get(row[3], ''), files.get(row[5], '')
        art_by_map[map_id] = {'button': art(button), 'lore': art(lore)}
        story = resolve(instances.text(row[7]), spells)
        if map_id not in REBUILT and len(story) > 20:       # not a placeholder ("PH")
            lore_by_map[map_id] = story
            maps[instance] = map_id
    for map_id, source in OLD_STORY.items():
        if map_id not in lore_by_map and len(stories.get(source, '')) > 20:
            lore_by_map[map_id] = stories[source]
    for map_id, name in OLD_ART.items():
        art_by_map[map_id] = {'button': art(f'UI-EJ-DUNGEONBUTTON-{name}.blp'),
                              'lore': art(f'UI-EJ-LOREBG-{name}.blp')}
    for map_id, name in LOADING_SCREENS.items():
        art_by_map[map_id] = {'screen': name}

    # Bosses: each encounter's description, its first creature's portrait, its sections in order (depth first).
    encounters = Dbc(dbc / 'JournalEncounter.dbc')
    creatures = Dbc(dbc / 'JournalEncounterCreature.dbc')
    sections = Dbc(dbc / 'JournalEncounterSection.dbc')
    portraits_of, names_of = {}, {}
    for row in sorted(creatures.rows, key=lambda r: (r[1], r[3])):
        portraits_of.setdefault(row[1], files.get(row[4], ''))
        names_of.setdefault(row[1], []).append(creatures.text(row[5]))
    children = {}
    for row in sections.rows:
        children.setdefault((row[1], row[4]), []).append(row)

    def walk(encounter, parent, depth, out):
        for row in sorted(children.get((encounter, parent), []), key=lambda r: r[5]):
            title, body = sections.text(row[9]), resolve(sections.text(row[10]), spells)
            icon = files.get(row[13], '')
            icon_path = spells.has_icon('Interface\\Icons\\' + icon.rsplit('.', 1)[0]) if icon else None
            if not icon_path:
                spell = re.search(r'\$(\d+)', sections.text(row[10]))
                icon_path = spells.icon(int(spell.group(1))) if spell else None
            out.append({'t': title, 'b': body, 'i': icon_path, 'f': row[8], 'l': depth})
            walk(encounter, row[0], depth + 1, out)
        return out

    bosses = {}
    for row in encounters.rows:
        map_id = maps.get(row[6])
        if map_id is None:
            continue
        name = encounters.text(row[9])
        boss = {'d': resolve(encounters.text(row[10]), spells), 'p': art(portraits_of.get(row[0])),
                's': walk(row[0], 0, 0, []), 'o': row[7]}
        for alias in [name] + names_of.get(row[0], []):
            bosses.setdefault(map_id, {}).setdefault(key(alias), boss)

    for map_id, names in ALIASES.items():
        for game, journal in names.items():
            if key(journal) in bosses.get(map_id, {}):
                bosses[map_id].setdefault(key(game), bosses[map_id][key(journal)])

    # Every portrait (but Mists of Pandaria's), by the name in its file.
    mop_portraits = {portraits_of.get(row[0], '').lower() for row in encounters.rows
                     if tier_of.get(row[6]) in SKIP_TIERS}
    portraits = {}
    for name, path in sorted(art_files.items()):
        if name.startswith('ui-ej-boss-') and name not in mop_portraits and name != 'ui-ej-boss-default.blp':
            portraits[key(path.stem[len('UI-EJ-BOSS-'):])] = art(path.name)

    art('UI-EJ-BOSS-Default.blp')       # the portrait of a boss the Journal has none for
    # The Journal's frame art: its book, its buttons and bars (one sheet), and each expansion's home background.
    for name in ('UI-EJ-JournalBG', 'UI-ENCOUNTERJOURNALTEXTURES', 'UI-EJ-Classic',
                 'UI-EJ-BurningCrusade', 'UI-EJ-WrathoftheLichKing', 'UI-EJ-PortraitIcon',
                 'UI-EncounterJournalTextures_Tile',
                 # The standard window frame the Journal sits in (Interface\\FrameGeneral): its border, portrait ring,
                 # title bar, rock background and the inset's marble.
                 'UI-Frame', '!UI-Frame', '_UI-Frame', 'UI-Background-Rock', 'UI-Background-Marble'):
        art(name + '.blp')
    out = ADDON / 'Art'
    out.mkdir(parents=True, exist_ok=True)
    for old in out.iterdir():           # the Journal's art (portraits.py's head shots, EF-*, stay)
        if not old.name.startswith('EF-'):
            old.unlink()
    for name in sorted(used):
        shutil.copyfile(art_files[name], out / (art_files[name].stem + '.blp'))     # .blp: binary to git
    (out / 'UI-EJ-Icons.blp').write_bytes(blp.encode(read_bgra(art_files['ui-ej-icons.blp'])))
    frame = Image.open(art_files[FRAME_SOURCE]).convert('RGBA')
    left, top, right, bottom = FRAME_HOLE
    frame.paste((0, 0, 0, 0), (left, top, right, bottom))
    (out / 'UI-EJ-LOREBG-Frame.blp').write_bytes(blp.encode_dxt(frame))

    lines = ['-- The Dungeon Journal\'s art and texts, from the Mists of Pandaria client\'s Journal. Written by',
             '-- modules/mod-titan/tools/journal/mop_journal.py; edit that, not this file.',
             'EmberforgeJournalArt = {']
    lines += [f'    [{m}] = {lua(a)},' for m, a in sorted(art_by_map.items())]
    lines += ['}', 'EmberforgeJournalLore = {']
    lines += [f'    [{m}] ={lua(text, 4)},' for m, text in sorted(lore_by_map.items())]
    lines += ['}', 'EmberforgeJournalPortraits = {']
    lines += [f'    [{lua(k)}] = {lua(p)},' for k, p in sorted(portraits.items())]
    lines += ['}', 'EmberforgeJournalBosses = {']
    for map_id, entries in sorted(bosses.items()):
        lines.append(f'    [{map_id}] = {{')
        for name, boss in sorted(entries.items()):
            lines.append(f'        [{lua(name)}] = {{')
            for field in ('d', 'p', 'o'):
                if boss[field]:
                    text = lua(boss[field], 12)
                    lines.append(f'            {field} =' + (text if text.startswith('\n') else ' ' + text) + ',')
            lines.append('            s = {')
            lines += [f'                {lua(section, 16)},' for section in boss['s']]
            lines.append('            },')
            lines.append('        },')
        lines.append('    },')
    lines.append('}')
    (ADDON / 'Data.lua').write_text('\n'.join(lines) + '\n', encoding='utf8')
    size = sum(f.stat().st_size for f in out.iterdir())
    print(f'{len(art_by_map)} instances with art, {sum(len(b) for b in bosses.values())} boss keys, '
          f'{len(portraits)} portraits, {len(used) + 1} art files ({size // 1024} KB)')


if __name__ == '__main__':
    main()
