#!/usr/bin/env python3
"""The Rune of the Unbroken Onslaught's wake art (spells/WAR_UnbrokenOnslaught/, see patch_runes.EFFECTS): white on
black (drawn additive, tinted blood red by the model), drawn here.

- wake_gash.png: a few crossed blade cuts torn into the ground, glowing at their middles, with flecks thrown off
  them, fading at the rim. Patches of it lie along the wake, so it reads the same from any side.

Usage: python3 build_wake_art.py
"""

import math
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent
OUT = HERE / 'spells' / 'WAR_UnbrokenOnslaught'
SIZE = 256


def cut(draw, rng, angle, length, width):
    """A blade cut through the middle: widest at its center, thin at both ends, slightly curved."""
    middle = SIZE / 2
    bend = rng.uniform(-0.25, 0.25)
    steps = 24
    for i in range(steps):
        t0, t1 = i / steps - 0.5, (i + 1) / steps - 0.5
        points = []
        for t in (t0, t1):
            a = angle + bend * t
            points.append((middle + t * length * math.cos(a) + rng.uniform(-1, 1),
                           middle + t * length * math.sin(a) + rng.uniform(-1, 1)))
        w = max(1, int(width * (1.0 - (2 * abs((t0 + t1) / 2)) ** 1.5)))
        draw.line(points, fill=255, width=w)


def gash():
    rng = random.Random(12294)
    image = Image.new('L', (SIZE, SIZE), 0)
    draw = ImageDraw.Draw(image)
    base = rng.uniform(0, math.pi)
    for i in range(3):
        cut(draw, rng, base + i * math.pi / 3 + rng.uniform(-0.2, 0.2), SIZE * rng.uniform(0.7, 0.85),
            rng.uniform(9, 13))
    middle = SIZE / 2
    for _ in range(40):
        a, r = rng.uniform(0, 2 * math.pi), rng.uniform(0.15, 0.42) * SIZE
        x, y = middle + r * math.cos(a), middle + r * math.sin(a)
        s = rng.uniform(1.0, 3.0)
        draw.ellipse([x - s, y - s, x + s, y + s], fill=rng.randint(120, 255))
    soft = image.filter(ImageFilter.GaussianBlur(5))
    glowing = Image.blend(soft, image, 0.5).point(lambda v: min(255, int(v * 1.9)))
    fade = Image.new('L', (SIZE, SIZE), 0)
    pixels = fade.load()
    for y in range(SIZE):
        for x in range(SIZE):
            d = math.hypot(x - middle, y - middle) / middle
            pixels[x, y] = int(255 * max(0.0, min(1.0, (1.0 - d) / 0.2)))
    gray = Image.composite(glowing, Image.new('L', (SIZE, SIZE), 0), fade)
    return Image.merge('RGBA', (gray, gray, gray, Image.new('L', gray.size, 255)))


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    gash().save(OUT / 'wake_gash.png')
    print(f'Wrote {OUT}/wake_gash.png')


if __name__ == '__main__':
    main()
