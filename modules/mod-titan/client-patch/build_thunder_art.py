#!/usr/bin/env python3
"""The Rune of Echoing Thunder's Thunderfield art (spells/WAR_EchoingThunder/, see patch_runes.EFFECTS): white on
black (drawn additive, tinted blue-white by the model), drawn here.

- thunder_cracks.png: cracked earth with lightning running through it: jagged cracks branching out from the middle
  to a ring of fractures at the edge, glowing, fading at the rim.

Usage: python3 build_thunder_art.py
"""

import math
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent
OUT = HERE / 'spells' / 'WAR_EchoingThunder'
SIZE = 512


def crack(draw, rng, x, y, angle, length, width, depth):
    """A jagged crack from (x, y), forking as it goes."""
    steps = max(3, int(length / 14))
    for _ in range(steps):
        angle += rng.uniform(-0.45, 0.45)
        step = length / steps
        nx, ny = x + step * math.cos(angle), y + step * math.sin(angle)
        draw.line([(x, y), (nx, ny)], fill=255, width=max(1, int(width)))
        x, y = nx, ny
        width *= 0.9
        if depth > 0 and rng.random() < 0.28:
            crack(draw, rng, x, y, angle + rng.choice((-1, 1)) * rng.uniform(0.5, 1.0), length * 0.45, width * 0.8,
                  depth - 1)


def cracks():
    rng = random.Random(23922)
    image = Image.new('L', (SIZE, SIZE), 0)
    draw = ImageDraw.Draw(image)
    middle = SIZE / 2
    for i in range(11):
        angle = i * 2 * math.pi / 11 + rng.uniform(-0.2, 0.2)
        crack(draw, rng, middle + 10 * math.cos(angle), middle + 10 * math.sin(angle), angle,
              rng.uniform(150, 230), 4.5, 3)
    # A ring of short fractures toward the edge.
    for i in range(36):
        angle = i * 2 * math.pi / 36 + rng.uniform(-0.05, 0.05)
        r = middle * rng.uniform(0.72, 0.86)
        crack(draw, rng, middle + r * math.cos(angle), middle + r * math.sin(angle), angle + math.pi / 2,
              rng.uniform(25, 45), 2.5, 1)
    soft = image.filter(ImageFilter.GaussianBlur(6))
    glowing = Image.blend(soft, image, 0.55).point(lambda v: min(255, int(v * 1.8)))
    # Fading at the rim, so the field has no hard edge.
    fade = Image.new('L', (SIZE, SIZE), 0)
    pixels = fade.load()
    for y in range(SIZE):
        for x in range(SIZE):
            d = math.hypot(x - middle, y - middle) / middle
            pixels[x, y] = int(255 * max(0.0, min(1.0, (1.0 - d) / 0.15)))
    gray = Image.composite(glowing, Image.new('L', (SIZE, SIZE), 0), fade)
    return Image.merge('RGBA', (gray, gray, gray, Image.new('L', gray.size, 255)))


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    cracks().save(OUT / 'thunder_cracks.png')
    print(f'Wrote {OUT}/thunder_cracks.png')


if __name__ == '__main__':
    main()
