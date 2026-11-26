#!/usr/bin/env python3
"""Build the Emberforge_Firepit addon's art: AddOns/Emberforge_Firepit/Art/.

- Background.blp: the painting of the ring (icons/ui/FirepitBackground.png, 16:9), squeezed into 1024x512; the
  panel is 16:9 too, so the addon shows all of it.
- Chrome.blp: what of the queue panel does not move, drawn over a transparent 560x315 panel (1024 wide, the top 576
  rows of a 1024x1024 texture): a dark veil over the painting (cut away over the icons and portraits), the thin bronze
  frame, the header line with ember under the title, the sections' underlines, the column and record lines, the
  icons' and places' outlines, the start picker's outline, the bottom line. The layout is the addon's
  (Emberforge_Firepit.lua: the same numbers, in the panel's units).
- Button*.tga (the big button: normal, pushed, highlight, disabled), Arrow*.tga (the picker's, pointing left; the
  addon flips it), Close.tga, Ring.tga (a filled place's ring), Slot.tga (an open place).

Usage: python3 build_firepit_art.py [--preview DIR]   (DIR: the same art as PNGs, for a look outside the game)
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / 'models'))

from PIL import Image, ImageDraw, ImageFilter  # noqa: E402

import blp  # noqa: E402

SOURCE = HERE / 'icons' / 'ui' / 'FirepitBackground.png'
OUT = HERE / 'AddOns' / 'Emberforge_Firepit' / 'Art'

# The panel and its layout, in its own units (Emberforge_Firepit.lua: the same numbers). 16:9, as the painting.
PANEL_W, PANEL_H = 560, 315
BORDER = (7, 5, 553, 310)                   # the thin frame's rectangle
HEADER_LINE = 33
TITLE_BAR_X = 162
UNDERLINES = [(26, 335, 57.6), (26, 241, 156), (26, 326, 216), (358, 537, 57.6), (358, 537, 176.5)]
COLUMN = (343, 43, 271)                     # the column line: x, from y, to y
GRID = (446, 67, 134, 100, 372, 524)        # records: the line down at x (y0-y1), across at y (x0-x1)
REWARD_X, REWARD_Y, REWARD_SIZE, REWARD_STEP = 35, 164, 29, 41.6
SLOT_CX, SLOT_CY, SLOT_R, SLOT_STEP = 40, 236.5, 12.5, 30.6
PICKER = (360, 186, 177, 23)
BOTTOM_LINE = 282

SS = 2                                      # drawn this many times larger, then shrunk (smooth edges)
SCALE = 1024 / PANEL_W
UNIT = SCALE * SS

VEIL = (10, 6, 5)
BRONZE = (140, 104, 76)
GOLD = (214, 170, 110)
CREAM = (236, 214, 180)
EMBER = (255, 128, 40)


def u(value):
    return int(round(value * UNIT))


def box(x, y, w, h):
    return (u(x), u(y), u(x + w), u(y + h))


def faded_line(layer, x0, y0, x1, y1, color, alpha, width, fade=(0.0, 0.25)):
    """A line whose ends fade: fade[0] of its length at the start, fade[1] at the end."""
    draw = ImageDraw.Draw(layer)
    steps = 80
    for i in range(steps):
        t0, t1 = i / steps, (i + 1) / steps
        t = (t0 + t1) / 2
        a = 1.0
        if fade[0] and t < fade[0]:
            a = t / fade[0]
        if fade[1] and t > 1 - fade[1]:
            a = min(a, (1 - t) / fade[1])
        draw.line((u(x0 + (x1 - x0) * t0), u(y0 + (y1 - y0) * t0), u(x0 + (x1 - x0) * t1), u(y0 + (y1 - y0) * t1)),
                  fill=color + (int(alpha * a),), width=width)


def underline(layer, x0, x1, y):
    """A section's underline: ember at its start, cooling to bronze, gone by its end; a soft glow under it."""
    glow = Image.new('RGBA', layer.size, (0, 0, 0, 0))
    faded_line(glow, x0, y, x0 + (x1 - x0) * 0.6, y, EMBER, 255, u(2.4), (0.0, 0.75))
    layer.alpha_composite(glow.filter(ImageFilter.GaussianBlur(u(1.7))))
    faded_line(layer, x0, y, x1, y, BRONZE, 230, max(2, u(0.55)), (0.0, 0.5))
    faded_line(layer, x0, y, x0 + (x1 - x0) * 0.6, y, (255, 160, 80), 255, max(2, u(0.75)), (0.0, 0.85))


def chrome():
    size = (u(PANEL_W), u(PANEL_H))
    layer = Image.new('RGBA', size, VEIL + (168,))

    # Darker toward the edges, and outside the frame's rounded corners.
    vignette = Image.new('L', size, 0)
    ImageDraw.Draw(vignette).rectangle((u(50), u(40), size[0] - u(50), size[1] - u(40)), fill=255)
    vignette = vignette.filter(ImageFilter.GaussianBlur(u(34)))
    edge = Image.new('RGBA', size, VEIL + (255,))
    edge.putalpha(vignette.point(lambda v: int((255 - v) * 0.42)))
    layer.alpha_composite(edge)
    outside = Image.new('L', size, 255)
    ImageDraw.Draw(outside).rounded_rectangle(tuple(u(v) for v in BORDER), u(8), fill=0)
    shade = Image.new('RGBA', size, (6, 4, 3, 240))
    shade.putalpha(outside.point(lambda v: int(v * 0.94)))
    layer.alpha_composite(shade)

    # Holes where the icons and portraits show through.
    holes = Image.new('L', size, 255)
    draw = ImageDraw.Draw(holes)
    for i in range(4):
        draw.rectangle(box(REWARD_X + i * REWARD_STEP, REWARD_Y, REWARD_SIZE, REWARD_SIZE), fill=0)
    for i in range(10):
        cx, cy, r = u(SLOT_CX + i * SLOT_STEP), u(SLOT_CY), u(SLOT_R - 0.6)
        draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=0)
    layer.putalpha(Image.composite(layer.getchannel('A'), holes, holes))

    # The frame: one thin bronze line, a darker one just inside.
    draw = ImageDraw.Draw(layer)
    draw.rounded_rectangle(tuple(u(v) for v in BORDER), u(8), outline=BRONZE + (235,), width=max(2, u(0.6)))
    inner = (BORDER[0] + 1.2, BORDER[1] + 1.2, BORDER[2] - 1.2, BORDER[3] - 1.2)
    draw.rounded_rectangle(tuple(u(v) for v in inner), u(7), outline=(0, 0, 0, 150), width=max(1, u(0.5)))

    # The header: its line, ember under the title, the bar between the title and its subtitle.
    faded_line(layer, 20, HEADER_LINE, 546, HEADER_LINE, BRONZE, 170, max(2, u(0.5)), (0.03, 0.03))
    glow = Image.new('RGBA', size, (0, 0, 0, 0))
    faded_line(glow, 22, HEADER_LINE, 158, HEADER_LINE, EMBER, 255, u(2.6), (0.08, 0.3))
    layer.alpha_composite(glow.filter(ImageFilter.GaussianBlur(u(1.9))))
    faded_line(layer, 22, HEADER_LINE, 158, HEADER_LINE, (255, 176, 96), 255, max(2, u(0.8)), (0.08, 0.3))
    faded_line(layer, TITLE_BAR_X, 15, TITLE_BAR_X, 28, BRONZE, 220, max(2, u(0.6)), (0.0, 0.0))

    for x0, x1, y in UNDERLINES:
        underline(layer, x0, x1, y)

    x, y0, y1 = COLUMN
    faded_line(layer, x, y0, x, y1, BRONZE, 190, max(2, u(0.5)), (0.12, 0.12))
    gx, gy0, gy1, gy, gx0, gx1 = GRID
    faded_line(layer, gx, gy0, gx, gy1, BRONZE, 170, max(2, u(0.5)), (0.18, 0.18))
    faded_line(layer, gx0, gy, gx1, gy, BRONZE, 170, max(2, u(0.5)), (0.12, 0.12))

    draw = ImageDraw.Draw(layer)
    for i in range(4):
        rect = box(REWARD_X + i * REWARD_STEP - 0.8, REWARD_Y - 0.8, REWARD_SIZE + 1.6, REWARD_SIZE + 1.6)
        draw.rounded_rectangle(rect, u(1.2), outline=BRONZE + (230,), width=max(2, u(0.55)))
    for i in range(10):
        cx, cy, r = u(SLOT_CX + i * SLOT_STEP), u(SLOT_CY), u(SLOT_R)
        draw.ellipse((cx - r, cy - r, cx + r, cy + r), outline=BRONZE + (200,), width=max(2, u(0.55)))

    x, y, w, h = PICKER
    fill = Image.new('RGBA', size, (0, 0, 0, 0))
    ImageDraw.Draw(fill).rounded_rectangle(box(x, y, w, h), u(5), fill=(4, 2, 2, 120))
    layer.alpha_composite(fill)
    draw = ImageDraw.Draw(layer)
    draw.rounded_rectangle(box(x, y, w, h), u(5), outline=GOLD + (210,), width=max(2, u(0.6)))

    faded_line(layer, 20, BOTTOM_LINE, 539, BOTTOM_LINE, BRONZE, 190, max(2, u(0.5)), (0.03, 0.03))

    panel = layer.resize((1024, round(PANEL_H * SCALE)), Image.LANCZOS)
    canvas = Image.new('RGBA', (1024, 1024), (0, 0, 0, 0))
    canvas.paste(panel, (0, 0))
    return canvas, panel


def button(state):
    """The big button, 256x64: a deep red rounded plate with a fine lighter edge."""
    s = 4
    w, h = 256 * s, 64 * s
    radius = 9 * s
    if state == 'highlight':
        glow = Image.new('RGBA', (w, h), (0, 0, 0, 0))
        ImageDraw.Draw(glow).rounded_rectangle((3 * s, 3 * s, w - 3 * s, h - 3 * s), radius, fill=(255, 120, 70, 70))
        return glow.filter(ImageFilter.GaussianBlur(4 * s)).resize((256, 64), Image.LANCZOS)
    top, bottom, edge = {
        'normal': ((156, 32, 26), (112, 20, 16), (196, 84, 62)),
        'pushed': ((104, 18, 14), (88, 14, 11), (160, 64, 48)),
        'disabled': ((68, 52, 48), (52, 40, 37), (110, 92, 84)),
    }[state]
    face = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    fd = ImageDraw.Draw(face)
    for y in range(h):
        t = y / (h - 1)
        fd.line((0, y, w, y), fill=tuple(int(a + (b - a) * t) for a, b in zip(top, bottom)) + (255,))
    mask = Image.new('L', (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle((2 * s, 2 * s, w - 2 * s, h - 2 * s), radius, fill=255)
    image = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    image.paste(face, (0, 0), mask)
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((2 * s, 2 * s, w - 2 * s, h - 2 * s), radius, outline=edge + (255,), width=s + 1)
    if state == 'normal':
        draw.line((radius, 4 * s, w - radius, 4 * s), fill=(255, 170, 140, 90), width=s)
    return image.resize((256, 64), Image.LANCZOS)


def chevron(color, flip=False):
    s = 4
    w = 32 * s
    image = Image.new('RGBA', (w, w), (0, 0, 0, 0))
    points = [(20 * s, 7 * s), (11 * s, 16 * s), (20 * s, 25 * s)]
    ImageDraw.Draw(image).line(points, fill=color + (255,), width=3 * s, joint='curve')
    image = image.resize((32, 32), Image.LANCZOS)
    return image.transpose(Image.FLIP_LEFT_RIGHT) if flip else image


def cross(color):
    s = 4
    w = 32 * s
    image = Image.new('RGBA', (w, w), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.line((7 * s, 7 * s, 25 * s, 25 * s), fill=color + (255,), width=3 * s)
    draw.line((25 * s, 7 * s, 7 * s, 25 * s), fill=color + (255,), width=3 * s)
    return image.resize((32, 32), Image.LANCZOS)


def ring():
    """A filled place's ring: gold, over the bronze one."""
    s = 4
    w = 64 * s
    image = Image.new('RGBA', (w, w), (0, 0, 0, 0))
    ImageDraw.Draw(image).ellipse((3 * s, 3 * s, w - 3 * s, w - 3 * s), outline=GOLD + (255,), width=3 * s)
    return image.resize((64, 64), Image.LANCZOS)


def slot():
    """An open place: dark inside its ring."""
    s = 4
    w = 64 * s
    image = Image.new('RGBA', (w, w), (0, 0, 0, 0))
    ImageDraw.Draw(image).ellipse((0, 0, w - 1, w - 1), fill=(14, 9, 7, 200))
    return image.resize((64, 64), Image.LANCZOS)


def main():
    preview = Path(sys.argv[sys.argv.index('--preview') + 1]) if '--preview' in sys.argv else None
    OUT.mkdir(parents=True, exist_ok=True)
    for stale in ('Background.tga', 'Firepit.tga'):
        (OUT / stale).unlink(missing_ok=True)
    pieces = {}
    with Image.open(SOURCE) as source:
        pieces['Background'] = source.convert('RGBA').resize((1024, 512), Image.LANCZOS)
    pieces['Background'].putalpha(255)
    (OUT / 'Background.blp').write_bytes(blp.encode_dxt(pieces['Background']))
    canvas, pieces['Chrome'] = chrome()
    (OUT / 'Chrome.blp').write_bytes(blp.encode_dxt(canvas))
    for state, name in (('normal', 'Button'), ('pushed', 'ButtonPushed'), ('highlight', 'ButtonHighlight'),
                        ('disabled', 'ButtonDisabled')):
        pieces[name] = button(state)
    pieces['Arrow'] = chevron(CREAM)
    pieces['ArrowDisabled'] = chevron((96, 84, 74))
    pieces['Close'] = cross(CREAM)
    pieces['Ring'] = ring()
    pieces['Slot'] = slot()
    for name in ('Button', 'ButtonPushed', 'ButtonHighlight', 'ButtonDisabled', 'Arrow', 'ArrowDisabled', 'Close',
                 'Ring', 'Slot'):
        pieces[name].save(OUT / f'{name}.tga')
    for path in sorted(OUT.iterdir()):
        print(f'  {path.name} {path.stat().st_size // 1024} KB')
    if preview:
        preview.mkdir(parents=True, exist_ok=True)
        for name, image in pieces.items():
            image.save(preview / f'{name}.png')


if __name__ == '__main__':
    main()
