#!/usr/bin/env python3
"""Build the Legendary Rune panel's interface art (AddOns/Emberforge_Runes/Art/Ui*.tga): the pieces the redesigned
panel draws with, from the "Recommended for you", "Browse legendary runes" and rune build mockups. White pieces are
tinted in the game (SetVertexColor); the gold ring is painted. Each is drawn four times larger and brought down, so
its edges are smooth.

Usage: python3 build_rune_ui.py
"""

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

OUT = Path(__file__).resolve().parent / 'AddOns' / 'Emberforge_Runes' / 'Art'
S = 4                                                   # supersampling


def canvas(w, h):
    return Image.new('RGBA', (w * S, h * S), (0, 0, 0, 0))


def save(image, name, w, h):
    image.resize((w, h), Image.LANCZOS).save(OUT / f'{name}.tga')


def white_shape(w, h, draw_fn, blur=0):
    mask = Image.new('L', (w * S, h * S), 0)
    draw_fn(ImageDraw.Draw(mask))
    if blur:
        mask = mask.filter(ImageFilter.GaussianBlur(blur * S))
    image = Image.new('RGBA', mask.size, (255, 255, 255, 0))
    image.putalpha(mask)
    return image


def ring_gold():
    """The medallion's gold ring (256): a bright gold band with a diamond at its top and bottom, a soft glow."""
    size = 256
    c = size * S / 2
    r_out, r_in = 118 * S, 113 * S
    image = canvas(size, size)
    glow = white_shape(size, size, lambda d: d.ellipse((c - r_out - 4 * S, c - r_out - 4 * S, c + r_out + 4 * S,
                                                         c + r_out + 4 * S), outline=255, width=8 * S), blur=4)
    glow_px = Image.new('RGBA', glow.size, (255, 196, 90, 0))
    glow_px.putalpha(glow.getchannel('A').point(lambda v: int(v * 0.45)))
    image.alpha_composite(glow_px)
    band = Image.new('L', image.size, 0)
    d = ImageDraw.Draw(band)
    d.ellipse((c - r_out, c - r_out, c + r_out, c + r_out), fill=255)
    d.ellipse((c - r_in, c - r_in, c + r_in, c + r_in), fill=0)
    # Gold, lighter at the top.
    grad = Image.new('RGBA', image.size)
    gp = grad.load()
    for y in range(image.size[1]):
        k = y / image.size[1]
        col = (int(255 - 40 * k), int(222 - 70 * k), int(130 - 80 * k), 255)
        for x in range(image.size[0]):
            gp[x, y] = col
    grad.putalpha(band)
    image.alpha_composite(grad)
    # The diamonds, top and bottom.
    for cy in (c - (r_out + r_in) / 2, c + (r_out + r_in) / 2):
        dm = 9 * S
        pts = [(c, cy - dm), (c + dm * 0.8, cy), (c, cy + dm), (c - dm * 0.8, cy)]
        ImageDraw.Draw(image).polygon(pts, fill=(255, 226, 140, 255), outline=(120, 80, 20, 255))
    save(image, 'UiRing', size, size)


def ring_thin():
    size = 128
    c = size * S / 2
    image = white_shape(size, size, lambda d: d.ellipse((c - 58 * S, c - 58 * S, c + 58 * S, c + 58 * S),
                                                        outline=255, width=4 * S))
    save(image, 'UiRingThin', size, size)


def disc():
    size = 128
    c = size * S / 2
    save(white_shape(size, size, lambda d: d.ellipse((c - 60 * S, c - 60 * S, c + 60 * S, c + 60 * S), fill=255)),
         'UiDisc', size, size)


def pill():
    """Capsules (256 x 64): the fill, the edge (3 px) and a soft glow (256 x 128, the capsule in its middle)."""
    w, h = 256, 64
    r = h * S / 2
    box = (1 * S, 1 * S, w * S - 1 * S, h * S - 1 * S)
    save(white_shape(w, h, lambda d: d.rounded_rectangle(box, radius=r - S, fill=255)), 'UiPillFill', w, h)
    save(white_shape(w, h, lambda d: d.rounded_rectangle(box, radius=r - S, outline=255, width=3 * S)),
         'UiPillEdge', w, h)
    gw, gh = 256, 128
    save(white_shape(gw, gh, lambda d: d.rounded_rectangle((24 * S, 32 * S, (gw - 24) * S, (gh - 32) * S),
                                                           radius=32 * S, fill=200), blur=10),
         'UiPillGlow', gw, gh)


def box():
    """A rounded box (64): fill and a 2 px edge, cut nine ways at 16 px."""
    size = 64
    rect = (S, S, size * S - S, size * S - S)
    save(white_shape(size, size, lambda d: d.rounded_rectangle(rect, radius=14 * S, fill=255)), 'UiBoxFill', size,
         size)
    save(white_shape(size, size, lambda d: d.rounded_rectangle(rect, radius=14 * S, outline=255, width=2 * S)),
         'UiBoxEdge', size, size)


def lines():
    """A line fading at both ends (256 x 8), and one fading in from its left (the eyebrow's)."""
    w, h = 256, 8
    for name, alpha in (('UiLine', lambda k: math.sin(math.pi * k) ** 0.6),
                        ('UiLineFade', lambda k: k ** 1.2)):
        image = Image.new('RGBA', (w, h), (255, 255, 255, 0))
        px = image.load()
        for x in range(w):
            a = alpha(x / (w - 1))
            for y in range(h):
                edge = 1 - abs(y - (h - 1) / 2) / (h / 2)
                px[x, y] = (255, 255, 255, int(255 * a * min(1, edge * 2.2)))
        image.save(OUT / f'{name}.tga')


def marks():
    size = 64
    c = size * S / 2
    # A diamond's outline, and filled.
    pts = [(c, 6 * S), (size * S - 14 * S, c), (c, size * S - 6 * S), (14 * S, c)]
    save(white_shape(size, size, lambda d: d.polygon(pts, outline=255, width=4 * S)), 'UiDiamond', size, size)
    save(white_shape(size, size, lambda d: d.polygon(pts, fill=255)), 'UiDiamondFill', size, size)
    # The check, the X, the lens.
    save(white_shape(size, size, lambda d: d.line([(14 * S, 33 * S), (27 * S, 46 * S), (51 * S, 18 * S)],
                                                  fill=255, width=6 * S, joint='curve')), 'UiCheck', size, size)
    save(white_shape(size, size, lambda d: (d.line([(12 * S, 12 * S), (52 * S, 52 * S)], fill=255, width=3 * S),
                                            d.line([(52 * S, 12 * S), (12 * S, 52 * S)], fill=255, width=3 * S))),
         'UiX', size, size)
    save(white_shape(size, size, lambda d: (d.ellipse((10 * S, 10 * S, 40 * S, 40 * S), outline=255, width=5 * S),
                                            d.line([(37 * S, 37 * S), (54 * S, 54 * S)], fill=255, width=6 * S))),
         'UiLens', size, size)


def vignette():
    """The dimmed world behind the panel: dark, darker toward the edges (256)."""
    size = 256
    image = Image.new('RGBA', (size, size))
    px = image.load()
    for y in range(size):
        for x in range(size):
            dx, dy = (x - size / 2) / (size / 2), (y - size / 2) / (size / 2)
            k = min(1, math.sqrt(dx * dx * 0.8 + dy * dy))
            px[x, y] = (4, 3, 8, int(255 * (0.42 + 0.4 * k ** 1.8)))
    image.save(OUT / 'UiVignette.tga')


if __name__ == '__main__':
    ring_gold()
    ring_thin()
    disc()
    pill()
    box()
    lines()
    marks()
    vignette()
    print('built', ', '.join(sorted(p.name for p in OUT.glob('Ui*.tga'))))
