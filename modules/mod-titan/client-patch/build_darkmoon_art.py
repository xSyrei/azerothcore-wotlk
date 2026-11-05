#!/usr/bin/env python3
"""Build the Emberforge_Darkmoon addon's art: AddOns/Emberforge_Darkmoon/Art/.

The painted art (darkmoon_art/*.webp: the cards' fronts and backs, the packs, the window's border and background) is
brought to power-of-two sizes (the 3.3.5a client loads nothing else; the addon draws each at its own aspect again)
and written as DXT BLPs (models/blp.py). The border is cut into its corners, crest and rails, the way the window
pieces it to its size. The small pieces drawn here (rounded panels and buttons, the Eye of Unlearning, the bar's
diamonds and orbs, glows) are uncompressed TGAs, sharp at any tint.

Usage: python3 build_darkmoon_art.py
"""

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / 'models'))

from PIL import Image, ImageDraw, ImageFilter  # noqa: E402

import blp  # noqa: E402

SOURCE = HERE / 'darkmoon_art'
OUT = HERE / 'AddOns' / 'Emberforge_Darkmoon' / 'Art'

# Painted art: (source, out name, power-of-two size)
PAINTED = [
    ('front', 'CardSkill', (512, 1024)), ('front_gold', 'CardSkillGolden', (512, 1024)),
    ('tfront', 'CardTalent', (512, 1024)), ('tfront_gold', 'CardTalentGolden', (512, 1024)),
    ('back', 'BackSkill', (256, 512)), ('back_gold', 'BackSkillGolden', (256, 512)),
    ('tback', 'BackTalent', (256, 512)), ('tback_gold', 'BackTalentGolden', (256, 512)),
    ('pack', 'PackSkill', (256, 512)), ('pack_gold', 'PackSkillGolden', (256, 512)),
    ('tpack', 'PackTalent', (256, 512)), ('tpack_gold', 'PackTalentGolden', (256, 512)),
    ('bg', 'Background', (1024, 1024)), ('orn_gem', 'Gem', (64, 64)),
]

# The border's pieces (source pixels): corners C square, the crest CREST wide in the top's middle, rails between.
C, CREST = 180, 380


def save_blp(image, name, size):
    image = blp.resize_straight(image.convert('RGBA'), size)
    (OUT / f'{name}.blp').write_bytes(blp.encode_dxt(image))
    print(f'  {name}.blp {size[0]}x{size[1]}')


def save_tga(image, name):
    image.save(OUT / f'{name}.tga')
    print(f'  {name}.tga {image.width}x{image.height}')


def border():
    with Image.open(SOURCE / 'border.webp') as source:
        src = source.convert('RGBA')
    w, h = src.size
    cx = (w - CREST) // 2
    pieces = {
        'BorderTL': ((0, 0, C, C), (256, 256)), 'BorderTR': ((w - C, 0, w, C), (256, 256)),
        'BorderBL': ((0, h - C, C, h), (256, 256)), 'BorderBR': ((w - C, h - C, w, h), (256, 256)),
        'BorderCrest': ((cx, 0, cx + CREST, C), (512, 256)),
        'BorderTopL': ((C, 0, cx, C), (512, 256)), 'BorderTopR': ((cx + CREST, 0, w - C, C), (512, 256)),
        'BorderBottom': ((C, h - C, w - C, h), (1024, 256)),
        'BorderLeft': ((0, C, C, h - C), (256, 1024)), 'BorderRight': ((w - C, C, w, h - C), (256, 1024)),
    }
    for name, (box, size) in pieces.items():
        save_blp(src.crop(box), name, size)


def rounded(size, radius, fill, edge, width=2, inner=None, scale=4):
    """A rounded box: fill a color or a (top, bottom) gradient, an edge line; inner: a second line inside."""
    s = size * scale
    image = Image.new('RGBA', (s, s), (0, 0, 0, 0))
    if isinstance(fill, tuple) and isinstance(fill[0], tuple):
        top, bottom = fill
        gradient = Image.new('RGBA', (1, s))
        for y in range(s):
            t = y / (s - 1)
            gradient.putpixel((0, y), tuple(round(a + (b - a) * t) for a, b in zip(top, bottom)))
        gradient = gradient.resize((s, s))
        mask = Image.new('L', (s, s), 0)
        ImageDraw.Draw(mask).rounded_rectangle((0, 0, s - 1, s - 1), radius * scale, fill=255)
        image.paste(gradient, (0, 0), mask)
    else:
        ImageDraw.Draw(image).rounded_rectangle((0, 0, s - 1, s - 1), radius * scale, fill=fill)
    draw = ImageDraw.Draw(image)
    half = width * scale // 2
    draw.rounded_rectangle((half, half, s - 1 - half, s - 1 - half), radius * scale - half, outline=edge,
                           width=width * scale)
    if inner:
        o = (width + 2) * scale
        draw.rounded_rectangle((o, o, s - 1 - o, s - 1 - o), radius * scale - o, outline=inner, width=scale)
    return image.resize((size, size), Image.Resampling.LANCZOS)


def pieces():
    # Panels and buttons: 64x64, sliced 9 ways by the addon (16 px corners). White fills are tinted there.
    save_tga(rounded(64, 12, ((30, 19, 14, 236), (11, 7, 6, 236)), (138, 95, 38, 255)), 'Panel')
    save_tga(rounded(64, 12, ((40, 20, 12, 242), (11, 7, 6, 242)), (224, 168, 76, 255)), 'PanelGold')
    save_tga(rounded(64, 10, ((36, 23, 15, 255), (16, 9, 6, 255)), (154, 106, 42, 255)), 'Button')
    save_tga(rounded(64, 10, ((255, 154, 58, 255), (194, 65, 15, 255)), (255, 208, 138, 255)), 'ButtonHot')
    save_tga(rounded(64, 10, ((138, 21, 24, 255), (74, 7, 9, 255)), (240, 192, 96, 255)), 'ButtonRed')
    save_tga(rounded(64, 10, ((13, 9, 7, 255), (13, 9, 7, 255)), (58, 44, 34, 255)), 'ButtonOff')
    save_tga(rounded(64, 10, ((34, 21, 15, 255), (11, 6, 5, 255)), (240, 192, 96, 255), 2, (106, 66, 20, 255)),
             'Pill')
    save_tga(rounded(64, 12, (0, 0, 0, 0), (107, 74, 32, 255), 1), 'Slot')
    save_tga(rounded(64, 12, (255, 255, 255, 255), (255, 255, 255, 255), 1), 'Solid')

    # The Eye of Unlearning: a gold almond and a fel-green iris.
    s = 512
    eye = Image.new('RGBA', (s, s), (0, 0, 0, 0))
    draw = ImageDraw.Draw(eye)
    almond = [(s * 0.06 + (s * 0.88) * t, s / 2 - math.sin(math.pi * t) * s * 0.36) for t in
              [i / 60 for i in range(61)]]
    almond += [(x, s - y) for x, y in reversed(almond)]
    draw.polygon(almond, fill=(10, 6, 4, 255))
    iris = Image.new('RGBA', (s, s), (0, 0, 0, 0))
    for r in range(int(s * 0.2), 0, -1):
        t = r / (s * 0.2)
        color = (int(234 - 221 * t), int(255 - 181 * t), int(216 - 200 * t), 255)
        ImageDraw.Draw(iris).ellipse((s / 2 - r, s / 2 - r, s / 2 + r, s / 2 + r), fill=color)
    mask = Image.new('L', (s, s), 0)
    ImageDraw.Draw(mask).polygon(almond, fill=255)
    eye.paste(iris, (0, 0), Image.composite(iris.getchannel('A'), Image.new('L', (s, s), 0), mask))
    draw.ellipse((s / 2 - s * 0.04, s / 2 - s * 0.13, s / 2 + s * 0.04, s / 2 + s * 0.13), fill=(4, 16, 4, 255))
    draw.line(almond + [almond[0]], fill=(214, 156, 64, 255), width=int(s * 0.045), joint='curve')
    for k in (-1, 1):
        x = s / 2 + k * s * 0.44
        draw.polygon([(x, s / 2), (x + k * s * 0.06, s / 2 - s * 0.05), (x + k * s * 0.06, s / 2 + s * 0.05)],
                     fill=(214, 156, 64, 255))
    save_tga(eye.resize((128, 128), Image.Resampling.LANCZOS), 'Eye')

    # A soft round glow, an orb, a diamond and a ring (white, tinted by the addon).
    glow = Image.new('L', (64, 64), 0)
    for y in range(64):
        for x in range(64):
            d = math.hypot(x - 31.5, y - 31.5) / 32
            glow.putpixel((x, y), int(255 * max(0, 1 - d) ** 2))
    save_tga(Image.merge('RGBA', (Image.new('L', (64, 64), 255),) * 3 + (glow,)), 'Glow')

    orb = Image.new('RGBA', (256, 256), (0, 0, 0, 0))
    draw = ImageDraw.Draw(orb)
    draw.ellipse((0, 0, 255, 255), fill=(106, 22, 6, 255))
    for r in range(116, 0, -1):
        t = r / 116
        color = (int(255 - 43 * t ** 2), int(251 - 179 * t), int(230 - 215 * t), 255)
        draw.ellipse((102 - r * 0.9, 90 - r * 0.9, 102 + r * 1.1, 90 + r * 1.1), fill=color)
    draw.ellipse((6, 6, 250, 250), outline=(240, 192, 96, 255), width=10)
    mask = Image.new('L', (256, 256), 0)
    ImageDraw.Draw(mask).ellipse((2, 2, 254, 254), fill=255)
    orb.putalpha(mask)
    save_tga(orb.resize((64, 64), Image.Resampling.LANCZOS), 'Orb')

    diamond = Image.new('RGBA', (128, 128), (0, 0, 0, 0))
    points = [(64, 8), (120, 64), (64, 120), (8, 64)]
    ImageDraw.Draw(diamond).polygon(points, fill=(255, 255, 255, 255), outline=(0, 0, 0, 255))
    inner = [(64, 26), (102, 64), (64, 102), (26, 64)]
    ImageDraw.Draw(diamond).line(inner + [inner[0]], fill=(255, 255, 255, 120), width=4)
    ImageDraw.Draw(diamond).line(points + [points[0]], fill=(30, 18, 8, 255), width=8)
    save_tga(diamond.resize((32, 32), Image.Resampling.LANCZOS), 'Diamond')

    ring = Image.new('RGBA', (256, 256), (0, 0, 0, 0))
    ImageDraw.Draw(ring).ellipse((8, 8, 248, 248), fill=(26, 12, 8, 255), outline=(255, 255, 255, 255), width=14)
    save_tga(ring.resize((64, 64), Image.Resampling.LANCZOS), 'Ring')

    # Infinity, for the end of the Eyes' track (the game's fonts have no such glyph).
    inf = Image.new('RGBA', (512, 256), (0, 0, 0, 0))
    curve = [(256 + 190 * math.cos(t) / (1 + math.sin(t) ** 2), 128 + 190 * math.sin(t) * math.cos(t) /
              (1 + math.sin(t) ** 2)) for t in [i / 200 * 2 * math.pi for i in range(201)]]
    ImageDraw.Draw(inf).line(curve, fill=(255, 255, 255, 255), width=40, joint='curve')
    save_tga(inf.resize((128, 64), Image.Resampling.LANCZOS), 'Infinity')

    # The slash across a pack's top: a bright streak, soft at both ends.
    slash = Image.new('RGBA', (256, 32), (0, 0, 0, 0))
    for x in range(256):
        a = math.sin(math.pi * x / 255) ** 0.6
        for y in range(32):
            v = max(0, 1 - abs(y - 15.5) / 16) ** 3
            slash.putpixel((x, y), (255, 248, 220, int(255 * a * v)))
    save_tga(slash.filter(ImageFilter.GaussianBlur(0.6)), 'Slash')


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for source, name, size in PAINTED:
        with Image.open(SOURCE / f'{source}.webp') as image:
            save_blp(image, name, size)
    border()
    pieces()


if __name__ == '__main__':
    main()
