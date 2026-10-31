#!/usr/bin/env python3
"""Build dist/patch-enUS-5.MPQ: Emberforge's retextures of the game's own textures.

Every PNG under textures/ replaces the client's texture at the same path (textures/Tileset/Elwynn/
ElwynnGrassBase.png is Tileset\\Elwynn\\ElwynnGrassBase.blp), as a DXT-compressed BLP2 with all its mip
levels (models/blp.py's encode_dxt: DXT1 when opaque, DXT5 with the alpha otherwise; terrain textures'
alpha is their specular). They are an archive of their own, beside patch-enUS-4.MPQ, so that rebuilding
the main patch does not carry them again: run this only when textures/ changes.

Usage: python3 build_textures.py
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / 'models'))

from PIL import Image  # noqa: E402

import blp  # noqa: E402
import make_mpq  # noqa: E402

SOURCE = HERE / 'textures'
OUT = HERE / 'dist' / 'patch-enUS-5.MPQ'


def main():
    entries = []
    for png in sorted(SOURCE.rglob('*.png')):
        path = '\\'.join(png.relative_to(SOURCE).with_suffix('.blp').parts)
        with Image.open(png) as image:
            entries.append((path, blp.encode_dxt(image)))
        print(f'  {path}')
    OUT.parent.mkdir(exist_ok=True)
    OUT.write_bytes(make_mpq.build(entries))
    print(f'Wrote {OUT} ({len(entries)} textures, {OUT.stat().st_size:,} bytes)')


if __name__ == '__main__':
    main()
