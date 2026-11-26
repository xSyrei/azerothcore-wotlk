#!/usr/bin/env python3
"""Atmosphere and living water: dist/patch-enUS-8.MPQ, the client's lighting tables (every zone, every hour).

- Atmosphere: outdoor fog (LightFloatBand band 0, its end distance) comes FOG_END times nearer, and starts FOG_START
  sooner (band 1, its start as a share of the end), so the distance hazes into each zone's own fog and sky colour as
  real air does. Indoors and dungeons (short fog already: under FOG_MIN_END) keep theirs.
- Translucent water: the water's opacity (LightParams: lakes' and oceans' shallow and deep alpha) is lowered, the
  shallows most, so the ground shows through near the shore and deep water still reads deep.
- Colour-matched water: each zone's water colours (LightIntBand 14-17: river shallow, river deep, ocean shallow,
  ocean deep) move toward clear sea teal (TEAL_SHARE) and take a little of that zone's sky at the same hour (band 6,
  the horizon, for the shallows; band 5 for the deep), so a dusk lake still goes warm, a stormy sea grey.

Bands are per LightParams: LightIntBand id = params * 18 - 17 + band, LightFloatBand id = params * 6 - 5 + band; each
holds up to 16 (time of day, value) keys.

- The Firepit's own light (Blackrock Depths' Ring of Law, map 230: the dungeon has none of its own, so the global
  default's dim grey filled it): a Light there (FIREPIT_AT, FIREPIT_REACH) with its own LightParams and bands, all day
  the same: warm ambient and diffuse light, an ember fog that starts near and thickens to orange, a red cavern sky.

Usage: python3 make_atmosphere.py <dir with original DBCs (Light, LightParams, LightIntBand, LightFloatBand)>
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
SHALLOW_ALPHA = 0.95    # shallow water's opacity, times this (at 0.7 a pale canal floor shone through: glass)
DEEP_ALPHA = 1.0        # deep water's
SKY_IN_SHALLOWS = 0.15  # the share of the sky's horizon colour in the shallows
SKY_IN_DEEP = 0.1       # and of its lower sky in the deep
# Clear sea water's own teal (the reference look), blended into every zone's colours, which keep the rest.
TEAL_SHALLOW, TEAL_DEEP = (70, 136, 124), (20, 72, 74)
TEAL_SHARE = 0.25      # (0.45 was bright: the reflections show on darker water)

BAND_SKY_LOW, BAND_HORIZON = 5, 6

# The Firepit's light: the ring's centre (world x, y, z), where it is whole and where it is gone (yards).
FIREPIT_MAP, FIREPIT_AT, FIREPIT_REACH = 230, (596.48, -187.91, -54.14), (60.0, 95.0)
FIREPIT_TEMPLATE_PARAMS = 12            # the global default's clear-weather params: the rest of its bands kept
FIREPIT_COLOURS = {                     # LightIntBand band: colour (all day)
    0: (138, 72, 46),                   # ambient: warm, so faces and armour read
    1: (255, 156, 92),                  # diffuse
    2: (46, 12, 6), 3: (72, 20, 8), 4: (104, 30, 10), 5: (140, 44, 14), 6: (176, 62, 18),   # the sky, red to ember
    7: (150, 52, 18),                   # fog: ember
    9: (255, 178, 104), 10: (255, 120, 50),     # sun and its halo (where the sky shows)
}
FIREPIT_FOG_END, FIREPIT_FOG_START = 160.0 * 36, 0.2    # the fog's end (yards x 36) and where it starts (its share)
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
        shallow = band in (14, 16)
        share = SKY_IN_SHALLOWS if shallow else SKY_IN_DEEP
        teal = TEAL_SHALLOW if shallow else TEAL_DEEP
        own = 1 - share - TEAL_SHARE
        blended = []
        for i in range(count):
            water, tint = unpack(values[i]), at_time(sky, times[i])
            # The teal dims with the zone's own water (night, dark zones): scaled by its brightness, up to 1.
            light = min(1.0, max(water) / 160.0)
            blended.append(pack(tuple(water[c] * own + tint[c] * share + teal[c] * light * TEAL_SHARE
                                      for c in range(3))))
        struct.pack_into(f'<{count}I', dbc.data, offset + 72, *blended)
    return bytes(dbc.data)


def appended(data, rows):
    """A DBC with rows added at its end (before its strings): the count raised."""
    data = bytearray(data)
    count, fields, size, strings = struct.unpack_from('<4I', data, 4)
    body = data[20:20 + count * size]
    for row in rows:
        assert len(row) == size
        body += row
    struct.pack_into('<I', data, 4, count + len(rows))
    return bytes(data[:20] + body + data[20 + count * size:])


def firepit_light(source, params_data, int_data, float_data):
    """The Firepit's Light, its LightParams (a copy of the default's, a new id past every band's) and their bands."""
    light = Dbc(Path(source, 'Light.dbc'))
    params, ints, floats = Dbc.__new__(Dbc), Dbc.__new__(Dbc), Dbc.__new__(Dbc)
    for dbc, data in ((params, params_data), (ints, int_data), (floats, float_data)):
        dbc.data = bytearray(data)
        dbc.count, dbc.fields, dbc.size, _ = struct.unpack_from('<4I', dbc.data, 4)
    ids = lambda dbc: [struct.unpack_from('<I', dbc.data, o)[0] for o in dbc.rows()]
    new_params = max(max(ids(params)), -(-max(ids(ints)) // 18), -(-max(ids(floats)) // 6)) + 1
    template = next(o for o in params.rows() if struct.unpack_from('<I', params.data, o)[0] == FIREPIT_TEMPLATE_PARAMS)
    params_row = bytearray(params.data[template:template + params.size])
    struct.pack_into('<I', params_row, 0, new_params)

    old_ints, old_floats = bands(ints, 'I'), bands(floats, 'f')
    int_rows, float_rows = [], []
    for band in range(18):
        source_band = old_ints[FIREPIT_TEMPLATE_PARAMS * 18 - 17 + band]
        row = bytearray(ints.data[source_band[0]:source_band[0] + ints.size])
        struct.pack_into('<I', row, 0, new_params * 18 - 17 + band)
        if band in FIREPIT_COLOURS:
            struct.pack_into('<I', row, 4, 1)
            struct.pack_into('<16I', row, 8, *([0] * 16))
            struct.pack_into('<16I', row, 72, *([pack(FIREPIT_COLOURS[band])] * 16))
        int_rows.append(bytes(row))
    for band in range(6):
        source_band = old_floats[FIREPIT_TEMPLATE_PARAMS * 6 - 5 + band]
        row = bytearray(floats.data[source_band[0]:source_band[0] + floats.size])
        struct.pack_into('<I', row, 0, new_params * 6 - 5 + band)
        if band in (0, 1):
            struct.pack_into('<I', row, 4, 1)
            struct.pack_into('<16I', row, 8, *([0] * 16))
            struct.pack_into('<16f', row, 72, *([FIREPIT_FOG_END if band == 0 else FIREPIT_FOG_START] * 16))
        float_rows.append(bytes(row))

    # Light: id, map, position (x = 17066.666 - world y, y = world z, z = 17066.666 - world x; all x 36), falloff
    # start and end (x 36), then its params: clear, clear underwater, storm, storm underwater, death, unused.
    light_ids = [struct.unpack_from('<I', light.data, o)[0] for o in light.rows()]
    wx, wy, wz = FIREPIT_AT
    default = light.data[light.rows()[0] + 28:light.rows()[0] + 60]
    underwater, _, storm_underwater, death = struct.unpack_from('<4I', default, 4)
    light_row = struct.pack('<2I5f8I', max(light_ids) + 1, FIREPIT_MAP, (17066.666 - wy) * 36, wz * 36,
                            (17066.666 - wx) * 36, FIREPIT_REACH[0] * 36, FIREPIT_REACH[1] * 36, new_params,
                            underwater, new_params, storm_underwater, death, 0, 0, 0)
    return (appended(bytes(light.data), [light_row]), appended(bytes(params.data), [bytes(params_row)]),
            appended(bytes(ints.data), int_rows), appended(bytes(floats.data), float_rows), new_params)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    source = sys.argv[1]
    floats, fogs = float_bands(source)
    light, params, ints, floats, firepit = firepit_light(source, light_params(source), int_bands(source), floats)
    entries = [
        ('DBFilesClient\\Light.dbc', light),
        ('DBFilesClient\\LightParams.dbc', params),
        ('DBFilesClient\\LightFloatBand.dbc', floats),
        ('DBFilesClient\\LightIntBand.dbc', ints),
    ]
    print(f'The Firepit\'s light: LightParams {firepit}')
    OUT.write_bytes(make_mpq.build(entries))
    print(f'Wrote {OUT} (fog pulled in for {fogs} outdoor lights, {OUT.stat().st_size / 2 ** 20:.1f} MB)')


if __name__ == '__main__':
    main()
