#!/usr/bin/env python3
"""Build the Firepit's danger decals (src/firepit.cpp Telegraph): spells/FP_Decals/*.png, flat ground textures the
decal models (patch_runes.py FIREPIT_DECALS, ground zones) lie on the floor with.

Each shape has four white textures (their opacity the shape), shared by every colour: the decal models tint them
with PALETTES (danger, safe, soak, heal, arcane, frost), so six colours cost no more textures than one.

- <shape>_fill: the area's faint fill, a touch stronger toward its edge;
- <shape>_core and <shape>_glow: its rim (drawn additive), a pale-hot core in the colour's glow;
- <shape>_timer: the fill that grows from the middle (a cone's apex, a line's axis) to the edge as the blow comes
  (the server scales it), with a brighter front;
- <shape>_<style>: a texture laid over the fill (STYLES: runic, a sigil of rune rings and spokes; cracked, molten
  cracks; hazard, warning stripes; swirl, a smoky spiral), drawn additive in the colour's glow, kept off the rim.

Shapes, each 1 yard of radius at 92% of the texture's half (the rest the glow's room), facing +X (the texture's
right: the decal object's facing): circle, ring (its inner edge at half the radius), cone90, cone180 and their large
cuts (cone180L, cone120L: thinner rims for the arena's halves and thirds, 52 yards), line_mid (a line's middle: a
square 2 yards across, its rims along its long sides) and line_cap (its end, rimmed at +X too).

Usage: python3 build_firepit_decals.py [--preview DIR]   (DIR: every shape drawn on the arena's sand, for a look)
"""

import sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
OUT = HERE / 'spells' / 'FP_Decals'
SIZE = 512
REACH = 0.92                                    # the shape's radius, of the texture's half
# Each colour's fill, timer, rim core and rim glow: what an area means at a glance. Danger: get out. Safe: stand here.
# Soak: share it. Heal: a helpful area. Arcane: magic, something special. Frost: slowing, frozen ground.
PALETTES = {
    'danger': ((200, 45, 18), (255, 88, 28), (255, 214, 150), (255, 92, 24)),
    'safe': ((22, 92, 205), (45, 150, 255), (205, 235, 255), (45, 140, 255)),
    'soak': ((190, 140, 20), (255, 196, 40), (255, 245, 195), (255, 186, 30)),
    'heal': ((30, 150, 55), (70, 220, 95), (215, 255, 205), (60, 220, 85)),
    'arcane': ((120, 40, 195), (180, 85, 255), (238, 214, 255), (172, 72, 255)),
    'frost': ((40, 150, 175), (110, 225, 245), (225, 250, 255), (90, 210, 240)),
}
# (name, kind, its rim's width and the fill's edge band, in yards of a 1 yard radius)
SHAPES = [
    ('circle', 'circle', 0.035), ('ring', 'ring', 0.035), ('cone90', ('cone', 45), 0.03),
    ('cone180', ('cone', 90), 0.03), ('cone180L', ('cone', 90), 0.012), ('cone120L', ('cone', 60), 0.012),
    ('line_mid', 'line_mid', 0.07), ('line_cap', 'line_cap', 0.07),
]


def grid():
    """Each pixel's point, in yards of a 1 yard radius (x right, y up)."""
    c = (np.arange(SIZE) + 0.5) / SIZE * 2.0 - 1.0
    x, y = np.meshgrid(c / REACH, -c / REACH)
    return x, y


def inside(kind, x, y):
    """The signed distance to the shape's edge (yards of a 1 yard radius): positive inside."""
    r = np.hypot(x, y)
    if kind == 'circle':
        return 1.0 - r
    if kind == 'ring':
        return np.minimum(1.0 - r, r - 0.5)
    if kind in ('line_mid', 'line_cap'):
        side = 1.0 - np.abs(y)
        if kind == 'line_cap':
            return np.minimum(side, 1.0 - x)
        return side
    half = np.radians(kind[1])
    angle = np.abs(np.arctan2(y, x))
    edge = np.where(angle <= half, r * np.sin(np.clip(half - angle, 0, None)), -r * np.sin(angle - half))
    if kind[1] >= 90:                           # a half: its straight edge through the apex
        edge = x if kind[1] == 90 else edge
    return np.minimum(1.0 - r, edge)


def rgba(color, alpha):
    image = np.zeros((SIZE, SIZE, 4), np.uint8)
    image[..., :3] = np.broadcast_to(np.asarray(color, float), (SIZE, SIZE, 3)).astype(np.uint8) \
        if np.ndim(color) == 1 else color.astype(np.uint8)
    image[..., 3] = np.clip(alpha * 255.0, 0, 255).astype(np.uint8)
    return Image.fromarray(image, 'RGBA')


def smooth(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3 - 2 * t)


WHITE = (255, 255, 255)
# The textures laid over a decal's fill (its "style"): a white pattern, in yards of a 1 yard radius.
STYLES = ('runic', 'cracked', 'hazard', 'swirl')


def pattern(style, x, y, line=False):
    """The style's pattern (0 to 1) at each point (line: a line's piece, its pattern running on from piece to
    piece)."""
    r, angle = np.hypot(x, y), np.arctan2(y, x)
    if style == 'runic' and line:               # a band of runes along the axis between two rails
        rails = np.exp(-((np.abs(y) - 0.62) / 0.02) ** 2) + 0.6 * np.exp(-((np.abs(y) - 0.42) / 0.015) ** 2)
        cell = np.floor((x + 1.0) * 4.0)
        within = (x + 1.0) * 4.0 - cell
        bits = (np.sin((cell + 3) * 12.9898) * 43758.5453) % 1.0
        # each rune: a stem, a bar to the right at one height and one to the left at another, slanting
        h1, h2 = (bits * 7) % 1.0 * 0.5 - 0.25, (bits * 13) % 1.0 * 0.5 - 0.25
        stem = (np.abs(y) < 0.3) & (np.abs(within - 0.5) < 0.045)
        right = (np.abs(y - h1 - (within - 0.5) * 0.5) < 0.04) & (within > 0.5) & (within < 0.82)
        left = (np.abs(y - h2 + (within - 0.5) * 0.5) < 0.04) & (within > 0.18) & (within < 0.5) \
            & ((bits * 29) % 1.0 > 0.3)
        return np.clip(rails + (stem | right | left) * 0.9, 0, 1)
    if style == 'runic':
        line = lambda at, w: np.exp(-((r - at) / w) ** 2)  # noqa: E731
        rings = line(0.86, 0.012) + line(0.72, 0.012) + 0.8 * line(0.46, 0.01) + 0.6 * line(0.2, 0.01)
        cell = np.floor((angle + np.pi) / (2 * np.pi) * 28)
        within = (angle + np.pi) / (2 * np.pi) * 28 - cell
        bits = (np.sin(cell * 12.9898) * 43758.5453) % 1.0
        band = (r > 0.745) & (r < 0.835)
        lower = r < 0.79
        mark = np.where(lower, (bits * 8) % 1.0 > 0.4, (bits * 3) % 1.0 > 0.35)
        glyphs = band & (within > 0.2) & (within < 0.75) & mark
        upright = band & (np.abs(within - 0.47) < 0.06)
        spoke_cell = (angle + np.pi) / (2 * np.pi) * 8
        spokes = np.exp(-(((spoke_cell % 1.0) - 0.5) * r * 6.0 / 0.02) ** 2 / 400) * ((r > 0.46) & (r < 0.72))
        star = np.exp(-(np.abs(np.sin(4 * angle)) * r / 0.015) ** 2) * (r < 0.46) * (r > 0.2) * 0.7
        return np.clip(rings + glyphs * 0.85 + upright * 0.5 + spokes * 0.7 + star, 0, 1)
    if style == 'cracked':
        rng = np.random.default_rng(11)
        step = 0.24
        g = np.arange(-1.3, 1.3 + step, step)
        sx, sy = np.meshgrid(g, g)
        seeds = np.stack([sx.ravel(), sy.ravel()], 1) + rng.uniform(-step * 0.4, step * 0.4, (sx.size, 2))
        best, second = np.full(x.shape, 9.0), np.full(x.shape, 9.0)
        for px, py in seeds:
            d = np.hypot(x - px, y - py)
            second = np.where(d < best, best, np.minimum(second, d))
            best = np.minimum(best, d)
        edge = second - best
        return np.clip(np.exp(-(edge / 0.012) ** 2) + 0.35 * np.exp(-(edge / 0.05) ** 2), 0, 1)
    if style == 'hazard':
        t = ((x + y) * 3.0) % 1.0               # a whole number of stripes a line's piece
        return smooth((np.abs(t - 0.5) - 0.32) / 0.04) * 0.4
    # swirl: arms of smoke spiralling in
    rng = np.random.default_rng(5)
    noise = sum(np.sin(x * f * a + y * f * b + p) / f for f, a, b, p in
                ((3, *rng.uniform(-1, 1, 2), rng.uniform(0, 6)), (7, *rng.uniform(-1, 1, 2), rng.uniform(0, 6)),
                 (13, *rng.uniform(-1, 1, 2), rng.uniform(0, 6))))
    arms = 0.5 + 0.5 * np.sin(5 * angle + 9 * r + noise * 1.5)
    return np.clip(arms ** 3 * (0.4 + 0.6 * smooth(r / 0.5)), 0, 1)


def styled(kind, rim, style):
    """The style's texture for a shape: its pattern inside the shape, kept off the rim."""
    x, y = grid()
    d = inside(kind, x, y)
    keep = smooth((d - rim * 1.5) / (rim * 2.5))
    if kind == 'line_mid':
        keep = keep * (np.abs(x) <= 1.0)
    elif kind == 'line_cap':
        keep = keep * (x >= -1.0)
    return rgba(WHITE, pattern(style, x, y, kind in ('line_mid', 'line_cap')) * keep)


def build(kind, rim):
    x, y = grid()
    d = inside(kind, x, y)
    pixel = 2.0 / SIZE / REACH                  # a pixel's yards, for edges a pixel soft
    covered = smooth(d / pixel + 0.5)
    # A line's pieces lie end to end 2 yards apart: nothing of theirs past that, or the seams would double up.
    cut = np.ones_like(d)
    if kind == 'line_mid':
        cut = smooth((1.0 - np.abs(x)) / pixel + 0.5)
    elif kind == 'line_cap':
        cut = smooth((x + 1.0) / pixel + 0.5)
    covered = covered * cut
    band = smooth(1.0 - d / (rim * 4.0))        # toward the edge
    fill = rgba(WHITE, covered * (0.16 + 0.18 * band))
    core = np.exp(-(d / rim) ** 2) * 0.9
    glow = np.exp(-(d / (rim * 2.5)) ** 2) * np.where(d > 0, 0.35, 0.22)
    core_image = rgba(WHITE, np.clip(core, 0, 1) * cut)
    glow_image = rgba(WHITE, np.clip(glow, 0, 1) * cut)
    front = smooth(1.0 - d / (rim * 2.5))
    timer = rgba(WHITE, covered * (0.30 + 0.30 * front))
    return fill, core_image, glow_image, timer


def tinted(image, colour):
    """A white texture in a colour (as the game tints it), for the preview."""
    data = np.asarray(image, float).copy()
    data[..., :3] = data[..., :3] * np.array(colour, float) / 255.0
    return Image.fromarray(data.astype(np.uint8), 'RGBA')


def preview(out_dir):
    """Every shape in every colour on a sand floor: outline, and the timer half grown."""
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(7)
    for colour, (fill_c, timer_c, core_c, glow_c) in PALETTES.items():
        for name, kind, rim in SHAPES:
            fill, core, glow, timer = build(kind, rim)
            sand = np.full((SIZE, SIZE, 3), (150, 118, 82), float) + rng.normal(0, 9, (SIZE, SIZE, 1))
            base = Image.fromarray(np.clip(sand, 0, 255).astype(np.uint8), 'RGB').convert('RGBA')
            small = tinted(timer, timer_c).resize((SIZE // 2, SIZE // 2), Image.LANCZOS)
            half = Image.new('RGBA', (SIZE, SIZE))
            half.paste(small, (SIZE // 4, SIZE // 4))
            lit = np.asarray(Image.alpha_composite(Image.alpha_composite(base, tinted(fill, fill_c)), half), float)
            for layer, c in ((glow, glow_c), (core, core_c)):
                add = np.asarray(tinted(layer, c), float)
                lit[..., :3] = np.clip(lit[..., :3] + add[..., :3] * add[..., 3:4] / 255.0, 0, 255)
            Image.fromarray(lit.astype(np.uint8), 'RGBA').save(out_dir / f'{colour}_{name}.png')


def main():
    if '--preview' in sys.argv:
        preview(sys.argv[sys.argv.index('--preview') + 1])
        return
    OUT.mkdir(parents=True, exist_ok=True)
    for name, kind, rim in SHAPES:
        for part, image in zip(('fill', 'core', 'glow', 'timer'), build(kind, rim)):
            image.save(OUT / f'{name}_{part}.png')
        for style in STYLES:
            styled(kind, rim, style).save(OUT / f'{name}_{style}.png')
    print(f'{len(SHAPES) * (4 + len(STYLES))} textures in {OUT}')


if __name__ == '__main__':
    main()
