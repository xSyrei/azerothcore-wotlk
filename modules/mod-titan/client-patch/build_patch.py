#!/usr/bin/env python3
"""Build dist/patch-enUS-4.MPQ: everything Emberforge changes on the game client.

It is a language patch (installed as Data\\enUS\\patch-enUS-4.MPQ): the client reads its DBC
tables from the language archives, which outrank patches placed directly in Data\\.

- DBFilesClient: ChrClasses / CharBaseInfo (Hero class, patch_hero_class.py),
  Item / ItemDisplayInfo (custom items, patch_items.py), Spell / SpellIcon / SpellVisual /
  SpellVisualKit / SpellVisualEffectName (Emberforge's own spells, the Demon Hunter's, patch_spells.py)
- spell effect models (Spells\\Emberforge: trails and the Eye Beam beam, models/spell_effects.py, from spells/*.png)
- spell sounds (Sound\\Spells\\Emberforge, from sounds/*.wav)
- custom models converted from wow.export exports (models/, see MODELS below)
- character models with the Demon Hunter's animations added (models/retarget_character.py, see CHARACTERS below)
- animated creature models from wow.export glTF exports (models/creature_gltf_to_m2.py, see CREATURES below),
  with their CreatureModelData / CreatureDisplayInfo rows (patch_creatures.py)
- the login screen scene, the Seat of the Pantheon (models/build_login_scene.py)
- inventory icons (icons/*.png) and spell icons (icons/spells/*.png)
- the login screen: interface/GlueXML/* (Hero text, retail-style character creation) and its art
  (interface/art/*.png, made by interface/make_art.py)

Usage: python3 build_patch.py <dir with the original 3.3.5a DBCs>
(e.g. `docker cp ac-worldserver:/azerothcore/env/dist/data/dbc ./dbc`)
"""

import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE / 'models'))

from PIL import Image  # noqa: E402

import blp  # noqa: E402
import build_login_scene  # noqa: E402
import creature_gltf_to_m2  # noqa: E402
import make_mpq  # noqa: E402
import retarget_character  # noqa: E402
import patch_spells  # noqa: E402
import spell_effects  # noqa: E402
import wowexport_to_m2  # noqa: E402

# (wow.export folder, exported model name, Emberforge model name, client folder)
MODELS = [
    ('models/veridus/source', 'sword_1h_artifactskywall_d_06', 'Sword_1H_Veridus', 'Item\\ObjectComponents\\Weapon'),
]
# (wow.export glTF folder, Emberforge model name, client folder, options): the Demon Hunter's demon forms, and
# their Blade Dance variants (the dance in every stance and movement animation; the base form's textures)
BLADE_DANCE = {'replace': creature_gltf_to_m2.BLADE_DANCE_EVERYWHERE,
               'move_speeds': creature_gltf_to_m2.BLADE_DANCE_SPEEDS}
CREATURES = [
    ('models/demonhunter_meta/male', 'DHMetaMale', 'Creature\\DemonHunterMeta', {}),
    ('models/demonhunter_meta/female', 'DHMetaFemale', 'Creature\\DemonHunterMeta', {}),
    ('models/demonhunter_meta/male', 'DHMetaMaleBladeDance', 'Creature\\DemonHunterMeta',
     dict(BLADE_DANCE, texture_name='DHMetaMale')),
    ('models/demonhunter_meta/female', 'DHMetaFemaleBladeDance', 'Creature\\DemonHunterMeta',
     dict(BLADE_DANCE, texture_name='DHMetaFemale')),
]
# (retail glTF folder, Blizzard's 3.3.5a model, client path): character models given the Demon Hunter's
# animations (models/retarget_character.py)
CHARACTERS = [
    ('models/characters/nightelf_male/retail', 'models/characters/nightelf_male/NightElfMale.m2',
     'Character\\NightElf\\Male\\NightElfMale.m2'),
    ('models/characters/nightelf_female/retail', 'models/characters/nightelf_female/NightElfFemale.m2',
     'Character\\NightElf\\Female\\NightElfFemale.m2'),
]
DBCS = ['ChrClasses.dbc', 'CharBaseInfo.dbc', 'CharStartOutfit.dbc', 'Item.dbc', 'ItemDisplayInfo.dbc',
        'Spell.dbc', 'SpellIcon.dbc', 'SpellVisual.dbc', 'SpellVisualKit.dbc', 'SpellVisualEffectName.dbc',
        'SoundEntries.dbc', 'CreatureModelData.dbc', 'CreatureDisplayInfo.dbc',
        'AnimationData.dbc']


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    dbc_dir = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        for script in ('patch_hero_class.py', 'patch_items.py', 'patch_spells.py', 'patch_creatures.py'):
            subprocess.run([sys.executable, str(HERE / script), str(dbc_dir), str(tmp / 'dbc')], check=True)
        entries = [(f'DBFilesClient\\{name}', (tmp / 'dbc' / name).read_bytes()) for name in DBCS]

        for source, base, name, folder in MODELS:
            wowexport_to_m2.convert(HERE / source, base, name, tmp / name, folder)
            for suffix in ('.m2', '00.skin', '.blp'):
                entries.append((f'{folder}\\{name}{suffix}', (tmp / name / f'{name}{suffix}').read_bytes()))

        for source, name, folder, options in CREATURES:
            files = creature_gltf_to_m2.convert(HERE / source, HERE / source / 'Metamorphosis.json', name,
                                                tmp / name, folder, **options)
            for f in files:
                entries.append((f'{folder}\\{f}', (tmp / name / f).read_bytes()))

        for retail, model, path in CHARACTERS:
            entries.append((path, retarget_character.build(HERE / retail, HERE / model)))

        for spec in patch_spells.EFFECTS.values():
            shape = spec.get('shape', 'weapon')
            make = {'weapon': spell_effects.weapon_trail, 'body': spell_effects.body_trail,
                    'beam': spell_effects.beam, 'burst': spell_effects.ground_burst}[shape]
            textures = [HERE / 'spells' / f'{t}.png' if t else None for t in spec.get('textures', (spec['model'],))]
            files = make(*textures, tmp / 'effects', spec['model'], patch_spells.EFFECTS_DIR,
                         **spec.get('options', {}))
            for f in files:
                entries.append((f'{patch_spells.EFFECTS_DIR}\\{f}', (tmp / 'effects' / f).read_bytes()))

        for spec in patch_spells.SOUNDS.values():
            for name in spec['files']:
                data = (HERE / 'sounds' / f'{name}.wav').read_bytes()
                # A damaged file (bytes lost or added, e.g. to line-ending conversion) plays as static.
                if data[:4] != b'RIFF' or int.from_bytes(data[4:8], 'little') + 8 != len(data):
                    raise ValueError(f'sounds/{name}.wav: size does not match its RIFF header (damaged?)')
                entries.append((f'{patch_spells.SOUNDS_DIR}\\{name}.wav', data))

        for png in sorted((HERE / 'icons').glob('INV_*.png')) + sorted((HERE / 'icons' / 'spells').glob('*.png')):
            with Image.open(png) as image:
                entries.append((f'Interface\\Icons\\{png.stem}.blp', blp.encode(image)))

        scene_dir = tmp / 'login_scene'
        build_login_scene.build(scene_dir)
        for f in sorted(scene_dir.iterdir()):
            entries.append((f'{build_login_scene.CLIENT_DIR}\\{f.name}', f.read_bytes()))

        for source in sorted((HERE / 'interface' / 'GlueXML').iterdir()):
            entries.append((f'Interface\\GlueXML\\{source.name}', source.read_bytes()))
        for png in sorted((HERE / 'interface' / 'art').glob('*.png')):
            with Image.open(png) as image:
                entries.append((f'Interface\\Glues\\Emberforge\\{png.stem}.blp', blp.encode(image)))

        out = HERE / 'dist' / 'patch-enUS-4.MPQ'
        out.parent.mkdir(exist_ok=True)
        out.write_bytes(make_mpq.build(entries))
        print(f'Wrote {out}:')
        for name, data in entries:
            print(f'  {name} ({len(data):,} bytes)')


if __name__ == '__main__':
    main()
