#!/usr/bin/env python3
"""Build dist/patch-E.MPQ: everything Emberforge changes on the game client.

- DBFilesClient: ChrClasses / CharBaseInfo (Hero class, patch_hero_class.py),
  Item / ItemDisplayInfo (custom items, patch_items.py)
- custom models converted from wow.export exports (models/, see MODELS below)
- inventory icons (icons/*.png)

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

import blp  # noqa: E402
import make_mpq  # noqa: E402
import wowexport_to_m2  # noqa: E402

# (wow.export folder, exported model name, Emberforge model name, client folder)
MODELS = [
    ('models/veridus/source', 'sword_1h_artifactskywall_d_06', 'Sword_1H_Veridus', 'Item\\ObjectComponents\\Weapon'),
]
DBCS = ['ChrClasses.dbc', 'CharBaseInfo.dbc', 'Item.dbc', 'ItemDisplayInfo.dbc']


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

        for png in sorted((HERE / 'icons').glob('*.png')):
            from PIL import Image
            with Image.open(png) as image:
                entries.append((f'Interface\\Icons\\{png.stem}.blp', blp.encode(image)))

        out = HERE / 'dist' / 'patch-E.MPQ'
        out.parent.mkdir(exist_ok=True)
        out.write_bytes(make_mpq.build(entries))
        print(f'Wrote {out}:')
        for name, data in entries:
            print(f'  {name} ({len(data):,} bytes)')


if __name__ == '__main__':
    main()
