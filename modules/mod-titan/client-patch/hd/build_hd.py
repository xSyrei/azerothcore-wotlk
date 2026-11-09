#!/usr/bin/env python3
"""Builds the HD graphics patch, dist/patch-enUS-6.MPQ: the HD liquids (make_liquids.py) and HD skies
(make_skies.py, when present). Its own archive, beside the gameplay patches (patch-enUS-4/5): rebuilt only when
these change.

Usage: python3 build_hd.py
"""

import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))
import make_liquids  # noqa: E402
import make_mpq  # noqa: E402

OUT = HERE.parent / 'dist' / 'patch-enUS-6.MPQ'


def main():
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        sys.argv = ['make_liquids.py', str(tmp)]
        make_liquids.main()
        if (HERE / 'make_skies.py').exists():
            import make_skies
            make_skies.build(tmp)
        entries = []
        for path in sorted(tmp.rglob('*.blp')) + sorted(tmp.rglob('*.m2')):
            name = str(path.relative_to(tmp)).replace('/', '\\')
            entries.append((name, path.read_bytes()))
        OUT.write_bytes(make_mpq.build(entries))
    print(f'Wrote {OUT} ({len(entries)} files, {OUT.stat().st_size / 2 ** 20:.1f} MB)')


if __name__ == '__main__':
    main()
