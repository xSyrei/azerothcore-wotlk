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

from PIL import Image, ImageChops, ImageDraw, ImageFilter  # noqa: E402

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

    # The beam out of a torn pack: from the gap (the bottom edge, half the width) widening upward to the whole width,
    # brightest at its foot and fading as it rises.
    beam = Image.new('L', (128, 256), 0)
    for y in range(256):
        rise = (255 - y) / 255                  # 0 at the foot, 1 at the top
        half = 0.525 + 0.475 * rise             # half-width, as a share of the texture's
        for x in range(128):
            d = abs(x - 63.5) / 64 / half
            beam.putpixel((x, y), int(255 * max(0.0, 1 - d) ** 1.2 * (1 - rise) ** 1.3))
    save_tga(Image.merge('RGBA', (Image.new('L', (128, 256), 255),) * 3 + (beam,)), 'Beam')

    # A legendary's pillar of light: a soft vertical band, fading at both ends.
    pillar = Image.new('L', (64, 256), 0)
    for y in range(256):
        v = math.sin(math.pi * y / 255)
        for x in range(64):
            pillar.putpixel((x, y), int(255 * max(0.0, 1 - abs(x - 31.5) / 32) ** 1.4 * v))
    save_tga(Image.merge('RGBA', (Image.new('L', (64, 256), 255),) * 3 + (pillar,)), 'Pillar')

    # The slash across a pack's top: a bright streak, soft at both ends.
    slash = Image.new('RGBA', (256, 32), (0, 0, 0, 0))
    for x in range(256):
        a = math.sin(math.pi * x / 255) ** 0.6
        for y in range(32):
            v = max(0, 1 - abs(y - 15.5) / 16) ** 3
            slash.putpixel((x, y), (255, 248, 220, int(255 * a * v)))
    save_tga(slash.filter(ImageFilter.GaussianBlur(0.6)), 'Slash')


# The icon's circle on each card front (fractions of the card: center x, y, radius of the width), as the addon's LAYOUT.
# behind: the icon shows through the frame's own window (the Talent Cards', whose ornaments reach into the circle), so
# inside the circle the ring keeps the frame as it is; else the circle is cleared (the Skill Cards' frame is solid
# there and the icon goes on top).
RINGS = [('front', 'CardSkillRing', 0.5, 0.287, 0.25, False),
         ('front_gold', 'CardSkillGoldenRing', 0.5, 0.287, 0.25, False),
         ('tfront', 'CardTalentRing', 0.5, 0.268, 0.285, True),
         ('tfront_gold', 'CardTalentGoldenRing', 0.5, 0.268, 0.285, True)]
RING_SPAN = 2.4     # the ring's square: this many radii across


def rings():
    """The frame around each card's icon circle, see-through inside it and solid outside: drawn over the icon (a
    square, as the game draws icons), it hides the icon's corners and its border, whatever the frame's window."""
    for source, name, cx, cy, r, behind in RINGS:
        with Image.open(SOURCE / f'{source}.webp') as image:
            card = image.convert('RGBA')
        w, h = card.size
        radius, half = r * w, r * w * RING_SPAN / 2
        box = (round(w * cx - half), round(h * cy - half), round(w * cx + half), round(h * cy + half))
        piece = card.crop(box)
        solid = Image.new('RGBA', piece.size, (14, 8, 6, 255))
        solid.alpha_composite(piece)
        scale = 4
        mask = Image.new('L', (piece.width * scale, piece.height * scale), 255)
        c, rr = piece.width * scale / 2, (radius - 1) * scale
        ImageDraw.Draw(mask).ellipse((c - rr, c - rr, c + rr, c + rr), fill=0)
        outside = mask.resize(piece.size, Image.Resampling.LANCZOS)
        if behind:
            outside = ImageChops.lighter(outside, piece.getchannel('A'))
        solid.putalpha(outside)
        save_blp(solid, name, (256, 256))


# The burn, baked: for each card front (its frame and ring, all that is the same on every card of it) 32 frames of
# the mockup's burn: noise climbs through the card (from the bottom, from its left), behind it the card is gone, along
# it a rim of cream-white. In the addon one texture steps through them (and mirrored, so cards differ); the icon, in
# tiles under it, and the text go by the same noise (BurnNoise.lua: the noise at 4 px, two digits a cell).
BURNS = [('front', 'BurnSkill', 0.5, 0.287, 0.25, False),
         ('front_gold', 'BurnSkillGolden', 0.5, 0.287, 0.25, False),
         ('tfront', 'BurnTalent', 0.5, 0.268, 0.285, True),
         ('tfront_gold', 'BurnTalentGolden', 0.5, 0.268, 0.285, True)]
BURN_FRAMES, BURN_COLUMNS = 32, 4           # the atlas: 4 across, 8 down
BURN_CELL = (128, 256)                      # each frame's cell; the card fills its top 128x192
BURN_SIZE = (256, 384)                      # baked at twice the size it is drawn, then brought down
BURN_REACH, BURN_EDGE = 1.08, 0.035         # the burn runs p from 0 to BURN_REACH; the rim is BURN_EDGE deep
NOISE_GRID = (32, 48)


def burn_noise():
    """The mockup's noise map: mostly from the bottom, from a point at the left, three octaves of value noise."""
    import numpy as np
    rng = np.random.default_rng(20261106)
    w, h = BURN_SIZE

    def octave(cell):
        cw, ch = w // cell + 3, h // cell + 3
        grid = rng.random((ch, cw))
        ys, xs = np.mgrid[0:h, 0:w] / cell
        x0, y0 = xs.astype(int), ys.astype(int)
        tx, ty = xs - x0, ys - y0
        tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
        top = grid[y0, x0] + (grid[y0, x0 + 1] - grid[y0, x0]) * tx
        bottom = grid[y0 + 1, x0] + (grid[y0 + 1, x0 + 1] - grid[y0 + 1, x0]) * tx
        return top + (bottom - top) * ty

    ys, xs = np.mgrid[0:h, 0:w]
    start = 0.3 * w
    octaves = octave(100) * 0.55 + octave(40) * 0.3 + octave(16) * 0.15
    n = (1 - ys / h) * 0.55 + np.abs(xs - start) / w * 0.25 + octaves * 0.5
    return (n - n.min()) / (n.max() - n.min())


def burns():
    import numpy as np
    noise = burn_noise()
    w, h = BURN_SIZE
    cw, ch = BURN_CELL
    rows = BURN_FRAMES // BURN_COLUMNS
    cream, gold = np.array([255, 252, 240], float), np.array([255, 214, 140], float)
    for source, name, cx, cy, r, behind in BURNS:
        with Image.open(SOURCE / f'{source}.webp') as image:
            card = np.asarray(blp.resize_straight(image.convert('RGBA'), (w, h)), float)
        ys, xs = np.mgrid[0:h, 0:w]
        circle = (xs - cx * w) ** 2 + (ys - cy * h) ** 2 < (r * w - 1) ** 2
        frame = card.copy()
        if not behind:
            frame[circle, 3] = 0                    # the icon shows through here (it is drawn under)
        present = (card[..., 3] > 8) | circle      # what burns: the card, its icon's circle too
        atlas = Image.new('RGBA', (cw * BURN_COLUMNS, ch * rows), (0, 0, 0, 0))
        for index in range(BURN_FRAMES):
            p = index / (BURN_FRAMES - 1) * BURN_REACH
            ahead = noise - p
            out = frame.copy()
            out[present & (ahead < 0), 3] = 0
            rim = present & (ahead >= 0) & (ahead < BURN_EDGE)
            k = (ahead[rim] / BURN_EDGE)[:, None]
            out[rim, :3] = cream + (gold - cream) * k
            out[rim, 3] = 255
            cell = blp.resize_straight(Image.fromarray(out.astype(np.uint8), 'RGBA'), (cw, ch * 3 // 4))
            atlas.paste(cell, ((index % BURN_COLUMNS) * cw, (index // BURN_COLUMNS) * ch))
        (OUT / f'{name}.blp').write_bytes(blp.encode_dxt(atlas))
        print(f'  {name}.blp {atlas.width}x{atlas.height} ({BURN_FRAMES} frames)')
    gw, gh = NOISE_GRID
    small = Image.fromarray((noise * 255).astype(np.uint8), 'L').resize((gw, gh), Image.Resampling.BOX)
    digits = ''.join(f'{min(99, small.getpixel((x, y)) * 100 // 256):02d}' for y in range(gh) for x in range(gw))
    (OUT.parent / 'BurnNoise.lua').write_text(
        '-- Generated by client-patch/build_darkmoon_art.py: the cards\' burn noise (0-99, two digits a cell, '
        'rows from\n'
        f'-- the top), {gw}x{gh} cells; the baked burns (Art/Burn*.blp) run it from 0 to {BURN_REACH}.\n'
        f'EmberforgeDarkmoon_BurnNoise = "{digits}"\n')
    print('  BurnNoise.lua')


# The cards of "Open all"'s fountain spin: each card's art centered in a transparent square with room to turn (its
# diagonal fits), so the addon can rotate it by its texture coordinates (the 3.3.5 client turns nothing else).
SPINS = [('back', 'SpinBackSkill'), ('back_gold', 'SpinBackSkillGolden'), ('tback', 'SpinBackTalent'),
         ('tback_gold', 'SpinBackTalentGolden'), ('front', 'SpinCardSkill'), ('front_gold', 'SpinCardSkillGolden'),
         ('tfront', 'SpinCardTalent'), ('tfront_gold', 'SpinCardTalentGolden')]
SPIN_SIZE = 256
SPIN_CARD_H = 210                   # the card's height in the square: sqrt(2^2 + 3^2) = 3.61 heights of 3 across


def spins():
    for source, name in SPINS:
        with Image.open(SOURCE / f'{source}.webp') as image:
            card = blp.resize_straight(image.convert('RGBA'), (SPIN_CARD_H * 2 // 3, SPIN_CARD_H))
        square = Image.new('RGBA', (SPIN_SIZE, SPIN_SIZE), (0, 0, 0, 0))
        square.alpha_composite(card, ((SPIN_SIZE - card.width) // 2, (SPIN_SIZE - card.height) // 2))
        (OUT / f'{name}.blp').write_bytes(blp.encode_dxt(square))
        print(f'  {name}.blp {SPIN_SIZE}x{SPIN_SIZE}')


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for source, name, size in PAINTED:
        with Image.open(SOURCE / f'{source}.webp') as image:
            save_blp(image, name, size)
    border()
    rings()
    pieces()
    burns()
    spins()


if __name__ == '__main__':
    main()
