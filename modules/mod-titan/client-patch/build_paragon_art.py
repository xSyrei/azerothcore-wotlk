#!/usr/bin/env python3
"""The Rune of the Sanctified Paragon's holy ground art (spells/PAL_SanctifiedParagon/, see patch_runes.EFFECTS):
white on black (drawn additive, tinted gold by the models), made here rather than taken from a demon's sigil.

- holy_inscriptions.png: a band of inscriptions (Cinzel capitals, the Paragon's words, crosses between) between two
  thin circles, the outer one at the texture's edge: the ring of holy writing about Seat of the Paragon's sigil;
- holy_sigil.png: Seat of the Paragon's larger sigil (rings, rays and an eight-pointed star);
- holy_glyph.png: Hallowed Footsteps' small glyph (a ring and a four-pointed star).

Usage: python3 build_paragon_art.py
"""

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = Path(__file__).resolve().parent
OUT = HERE / 'spells' / 'PAL_SanctifiedParagon'
FONT = HERE / 'AddOns' / 'Emberforge_Runes' / 'Fonts' / 'Cinzel-Bold.ttf'
WORDS = 'SANCTUS LUMEN PARAGON FIDES CUSTOS LUX AETERNA'
SIZE = 512


def glow(image, radius, strength=0.7):
    """The drawing with a soft glow around it (additive: light spills off the lines)."""
    soft = image.filter(ImageFilter.GaussianBlur(radius))
    return Image.blend(soft, image, 1 - strength).point(lambda v: min(255, int(v * 1.6)))


def as_additive(gray):
    """White on black, as RGBA with a full alpha (the models draw it additive: black adds nothing)."""
    return Image.merge('RGBA', (gray, gray, gray, Image.new('L', gray.size, 255)))


def circle(draw, middle, radius, width):
    draw.ellipse([middle - radius, middle - radius, middle + radius, middle + radius], outline=255, width=width)


def inscriptions():
    image = Image.new('L', (SIZE, SIZE), 0)
    draw = ImageDraw.Draw(image)
    middle = SIZE / 2
    circle(draw, middle, middle - 3, 3)
    circle(draw, middle, middle - 40, 2)
    font = ImageFont.truetype(str(FONT), 22)
    text = (WORDS + ' + ') * 2
    radius = middle - 22
    step = 2 * math.pi / len(text)
    for i, letter in enumerate(text):
        angle = i * step
        if letter == ' ':
            continue
        cell = Image.new('L', (32, 32), 0)
        ImageDraw.Draw(cell).text((16, 16), letter, font=font, fill=255, anchor='mm')
        cell = cell.rotate(-math.degrees(angle) - 90, resample=Image.BICUBIC)
        x = middle + radius * math.cos(angle) - 16
        y = middle + radius * math.sin(angle) - 16
        image.paste(255, (int(x), int(y)), cell)
    return as_additive(glow(image, 3))


def star(draw, middle, points, outer, inner, turn=0.0):
    corners = []
    for i in range(points * 2):
        r = outer if i % 2 == 0 else inner
        a = turn + i * math.pi / points
        corners.append((middle + r * math.cos(a), middle + r * math.sin(a)))
    draw.polygon(corners, outline=255, width=3)


def sigil():
    image = Image.new('L', (SIZE, SIZE), 0)
    draw = ImageDraw.Draw(image)
    middle = SIZE / 2
    for radius, width in ((middle - 4, 3), (middle - 26, 2), (middle * 0.52, 2), (middle * 0.18, 2)):
        circle(draw, middle, radius, width)
    for i in range(24):
        a = i * math.pi / 12
        inner, outer = middle * 0.56, middle - 30 if i % 2 == 0 else middle * 0.8
        draw.line([(middle + inner * math.cos(a), middle + inner * math.sin(a)),
                   (middle + outer * math.cos(a), middle + outer * math.sin(a))], fill=200, width=2)
    star(draw, middle, 8, middle * 0.5, middle * 0.24)
    star(draw, middle, 4, middle * 0.34, middle * 0.08, math.pi / 4)
    return as_additive(glow(image, 4))


def glyph():
    size = 256
    image = Image.new('L', (size, size), 0)
    draw = ImageDraw.Draw(image)
    middle = size / 2
    circle(draw, middle, middle - 6, 3)
    circle(draw, middle, middle * 0.62, 2)
    star(draw, middle, 4, middle * 0.82, middle * 0.16)
    return as_additive(glow(image, 3))


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    inscriptions().save(OUT / 'holy_inscriptions.png')
    sigil().save(OUT / 'holy_sigil.png')
    glyph().save(OUT / 'holy_glyph.png')
    print(f'Wrote {OUT}/holy_inscriptions.png, holy_sigil.png, holy_glyph.png')


if __name__ == '__main__':
    main()
