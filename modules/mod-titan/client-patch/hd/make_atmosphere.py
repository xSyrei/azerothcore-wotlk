#!/usr/bin/env python3
"""Atmosphere and living water: dist/patch-enUS-8.MPQ, the client's lighting tables (every zone, every hour).

- Atmosphere: outdoor fog (LightFloatBand band 0, its end distance) comes FOG_END times nearer, and starts FOG_START
  sooner (band 1, its start as a share of the end), so the distance hazes into each zone's own fog and sky colour as
  real air does. Indoors and dungeons (short fog already: under FOG_MIN_END) keep theirs.
- Translucent water: the water's opacity (LightParams: lakes' and oceans' shallow and deep alpha) is lowered, the
  shallows most, so the ground shows through near the shore and deep water still reads deep.
- Colour-matched water: each zone's water colours (LightIntBand 14-17: river shallow, river deep, ocean shallow,
  ocean deep) take on that zone's sky at the same hour (band 6, the horizon, for the shallows; band 5 for the deep),
  as real water mirrors the sky over it: a dusk lake goes warm, a stormy sea grey.

Bands are per LightParams: LightIntBand id = params * 18 - 17 + band, LightFloatBand id = params * 6 - 5 + band; each
holds up to 16 (time of day, value) keys.

Usage: python3 make_atmosphere.py <dir with original DBCs (LightParams, LightIntBand, LightFloatBand)>
"""

import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import make_mpq  # noqa: E402

OUT = HERE.parent / 'dist' / 'patch-enUS-8.MPQ'

FOG_END = 0.72          # outdoor fog ends at 72% of the distance it did
FOG_START = 0.15        # and starts this much sooner (its start share lowered)
FOG_MIN_END = 3000.0    # shorter fog (indoors, dungeons, under 83 yards) is left alone
SHALLOW_ALPHA = 0.6     # shallow water's opacity, times this
DEEP_ALPHA = 0.85       # deep water's
SKY_IN_SHALLOWS = 0.3   # the share of the sky's horizon colour in the shallows
SKY_IN_DEEP = 0.2       # and of its lower sky in the deep

BAND_SKY_LOW, BAND_HORIZON = 5, 6
WATER_BANDS = {14: BAND_HORIZON, 15: BAND_SKY_LOW, 16: BAND_HORIZON, 17: BAND_SKY_LOW}


class Dbc:
    def __init__(self, path):
        self.data = bytearray(Path(path).read_bytes())
        self.count, self.fields, self.size, _ = struct.unpack_from('<4I', self.data, 4)

    def offset(self, i):
        return 20 + i * self.size

    def rows(self):
        return [self.offset(i) for i in range(self.count)]


def bands(dbc, fmt):
    """id -> (row offset, number of keys, times, values)."""
    out = {}
    for offset in dbc.rows():
        ident, count = struct.unpack_from('<2I', dbc.data, offset)
        times = struct.unpack_from('<16I', dbc.data, offset + 8)
        values = struct.unpack_from('<16' + fmt, dbc.data, offset + 72)
        out[ident] = (offset, count, times, values)
    return out


def at_time(band, time):
    """A colour band's value at a time of day (0-2880, half-minutes), interpolated around the clock."""
    _, count, times, values = band
    if count <= 1:
        return unpack(values[0])
    keys = sorted(zip(times[:count], values[:count]))
    keys.append((keys[0][0] + 2880, keys[0][1]))
    t = time if time >= keys[0][0] else time + 2880
    for (t0, v0), (t1, v1) in zip(keys, keys[1:]):
        if t0 <= t <= t1:
            f = (t - t0) / max(t1 - t0, 1)
            a, b = unpack(v0), unpack(v1)
            return tuple(a[i] + (b[i] - a[i]) * f for i in range(3))
    return unpack(keys[-2][1])


def unpack(v):
    return ((v >> 16) & 255, (v >> 8) & 255, v & 255)


def pack(rgb):
    r, g, b = (max(0, min(255, round(c))) for c in rgb)
    return (r << 16) | (g << 8) | b


def light_params(source):
    dbc = Dbc(Path(source, 'LightParams.dbc'))
    for offset in dbc.rows():
        # fields 5-8: water shallow, water deep, ocean shallow, ocean deep alpha
        values = list(struct.unpack_from('<4f', dbc.data, offset + 20))
        values = [values[0] * SHALLOW_ALPHA, values[1] * DEEP_ALPHA, values[2] * SHALLOW_ALPHA, values[3] * DEEP_ALPHA]
        struct.pack_into('<4f', dbc.data, offset + 20, *values)
    return bytes(dbc.data)


def float_bands(source):
    dbc = Dbc(Path(source, 'LightFloatBand.dbc'))
    all_bands = bands(dbc, 'f')
    changed = 0
    for ident, (offset, count, _, values) in all_bands.items():
        band = (ident - 1) % 6
        if band != 0 or not count or max(values[:count]) < FOG_MIN_END:
            continue
        struct.pack_into(f'<{count}f', dbc.data, offset + 72, *[v * FOG_END for v in values[:count]])
        start = all_bands.get(ident + 1)
        if start:
            s_offset, s_count, _, s_values = start
            struct.pack_into(f'<{s_count}f', dbc.data, s_offset + 72, *[v - FOG_START for v in s_values[:s_count]])
        changed += 1
    return bytes(dbc.data), changed


def int_bands(source):
    dbc = Dbc(Path(source, 'LightIntBand.dbc'))
    all_bands = bands(dbc, 'I')
    for ident, (offset, count, times, values) in all_bands.items():
        band = (ident - 1) % 18
        if band not in WATER_BANDS or not count:
            continue
        sky = all_bands.get(ident - band + WATER_BANDS[band])
        if not sky or not sky[1]:
            continue
        share = SKY_IN_SHALLOWS if band in (14, 16) else SKY_IN_DEEP
        blended = []
        for i in range(count):
            water, tint = unpack(values[i]), at_time(sky, times[i])
            blended.append(pack(tuple(water[c] * (1 - share) + tint[c] * share for c in range(3))))
        struct.pack_into(f'<{count}I', dbc.data, offset + 72, *blended)
    return bytes(dbc.data)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    source = sys.argv[1]
    floats, fogs = float_bands(source)
    entries = [
        ('DBFilesClient\\LightParams.dbc', light_params(source)),
        ('DBFilesClient\\LightFloatBand.dbc', floats),
        ('DBFilesClient\\LightIntBand.dbc', int_bands(source)),
    ]
    OUT.write_bytes(make_mpq.build(entries))
    print(f'Wrote {OUT} (fog pulled in for {fogs} outdoor lights, {OUT.stat().st_size / 2 ** 20:.1f} MB)')


if __name__ == '__main__':
    main()
