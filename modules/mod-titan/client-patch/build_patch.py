#!/usr/bin/env python3
"""Build dist/patch-enUS-4.MPQ: everything Emberforge changes on the game client.

It is a language patch (installed as Data\\enUS\\patch-enUS-4.MPQ): the client reads its DBC
tables from the language archives, which outrank patches placed directly in Data\\.

- DBFilesClient: ChrClasses / CharBaseInfo (Hero class, patch_hero_class.py),
  Item / ItemDisplayInfo (custom items, patch_items.py)
- custom models converted from wow.export exports (models/, see MODELS below)
- the login screen scene, the Seat of the Pantheon (models/build_login_scene.py)
- inventory icons (icons/*.png)
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
import make_mpq  # noqa: E402
import wowexport_to_m2  # noqa: E402

# (wow.export folder, exported model name, Emberforge model name, client folder)
MODELS = [
    ('models/veridus/source', 'sword_1h_artifactskywall_d_06', 'Sword_1H_Veridus', 'Item\\ObjectComponents\\Weapon'),
]
DBCS = ['ChrClasses.dbc', 'CharBaseInfo.dbc', 'CharStartOutfit.dbc', 'Item.dbc', 'ItemDisplayInfo.dbc']


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    dbc_dir = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        for script in ('patch_hero_class.py', 'patch_items.py'):
            subprocess.run([sys.executable, str(HERE / script), str(dbc_dir), str(tmp / 'dbc')], check=True)
        entries = [(f'DBFilesClient\\{name}', (tmp / 'dbc' / name).read_bytes()) for name in DBCS]

        for source, base, name, folder in MODELS:
            wowexport_to_m2.convert(HERE / source, base, name, tmp / name, folder)
            for suffix in ('.m2', '00.skin', '.blp'):
                entries.append((f'{folder}\\{name}{suffix}', (tmp / name / f'{name}{suffix}').read_bytes()))

        for png in sorted((HERE / 'icons').glob('INV_*.png')):
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
