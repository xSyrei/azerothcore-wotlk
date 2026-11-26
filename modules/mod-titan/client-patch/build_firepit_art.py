#!/usr/bin/env python3
"""Build the Emberforge_Firepit addon's art: AddOns/Emberforge_Firepit/Art/.

- Background.blp: the painting of the ring (icons/ui/FirepitBackground.png, 16:9), squeezed into 1024x512; the addon
  draws it at the panel's own shape again.
- Chrome.blp: everything of the queue panel that does not move, drawn here over a transparent 560x344 panel (1024
  wide, the top 629 rows of a 1024x1024 texture): the iron frame with its ember edge and corners, the title plate,
  the column rail, the dark wash under the left column's words, the section dividers, the reward and portrait frames,
  the record plaques, the start picker's slot, the bottom strip. The layout is the addon's (Emberforge_Firepit.lua:
  the same numbers, in the panel's units).
- Button*.tga (the big button: normal, pushed, highlight, disabled), Arrow*.tga (the picker's, pointing left; the
  addon flips it), Slot.tga (an empty portrait).

Usage: python3 build_firepit_art.py [--preview DIR]   (DIR: the same art as PNGs, for a look outside the game)
"""

import math
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / 'models'))

from PIL import Image, ImageDraw, ImageFilter  # noqa: E402

import blp  # noqa: E402

SOURCE = HERE / 'icons' / 'ui' / 'FirepitBackground.png'
OUT = HERE / 'AddOns' / 'Emberforge_Firepit' / 'Art'

# The panel and its layout, in its own units (Emberforge_Firepit.lua).
PANEL_W, PANEL_H = 560, 344
FRAME = 11                                  # the iron frame's width
COLUMN_X = 334                              # the rail between the columns
TITLE_BOTTOM = 58
REWARDS_DIVIDER, GROUP_DIVIDER = 168, 238
REWARD_Y, REWARD_X, REWARD_SIZE, REWARD_STEP = 176, 24, 38, 48
SLOT_Y, SLOT_X, SLOT_SIZE, SLOT_STEP = 246, 24, 26, 30
PLAQUES = [(344, 22), (446, 22), (344, 84), (446, 84)]
PLAQUE_W, PLAQUE_H = 94, 54
PICKER = (344, 196, 196, 26)
STRIP_TOP = 304

SS = 2                                      # drawn this many times larger, then shrunk (smooth edges)
SCALE = 1024 / PANEL_W
UNIT = SCALE * SS

IRON = (38, 29, 26)
IRON_LIGHT = (104, 80, 64)
IRON_DARK = (12, 8, 7)
GOLD = (214, 160, 82)
EMBER = (255, 118, 28)
EMBER_HOT = (255, 196, 96)
random.seed(1170)


def u(value):
    return int(round(value * UNIT))


def box(x, y, w, h):
    return (u(x), u(y), u(x + w), u(y + h))


_IRON = {}


def iron_texture(size):
    """Dark iron: a base, mottled, with fine scratches (one for the whole panel, reused)."""
    if size in _IRON:
        return _IRON[size]
    w, h = size
    image = Image.new('RGBA', size, IRON + (255,))
    noise = Image.new('L', (max(1, w // 6), max(1, h // 6)))
    noise.putdata([random.randint(0, 255) for _ in range(noise.width * noise.height)])
    noise = noise.resize(size, Image.BICUBIC).filter(ImageFilter.GaussianBlur(3))
    light = Image.new('RGBA', size, (70, 54, 46, 255))
    image = Image.composite(light, image, noise.point(lambda v: int(v * 0.45)))
    draw = ImageDraw.Draw(image)
    for _ in range(w * h // 900):
        x, y = random.randrange(w), random.randrange(h)
        length = random.randint(4, 18)
        angle = random.uniform(-0.4, 0.4)
        draw.line((x, y, x + length * math.cos(angle), y + length * math.sin(angle)),
                  fill=(90, 72, 62, 90), width=1)
    _IRON[size] = image
    return image


def glow_line(layer, points, width, color, blur):
    glow = Image.new('RGBA', layer.size, (0, 0, 0, 0))
    ImageDraw.Draw(glow).line(points, fill=color, width=width)
    return Image.alpha_composite(layer, glow.filter(ImageFilter.GaussianBlur(blur)))


def rivet(draw, cx, cy, r):
    draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=IRON_DARK + (255,))
    draw.ellipse((cx - r + 2, cy - r + 2, cx + r - 2, cy + r - 2), fill=(96, 76, 62, 255))
    draw.ellipse((cx - r * 0.35 - 1, cy - r * 0.55, cx + r * 0.1, cy - r * 0.1), fill=(190, 160, 130, 255))


def bevel_rect(draw, rect, width, light, dark):
    x0, y0, x1, y1 = rect
    for i in range(width):
        draw.line((x0 + i, y1 - i, x0 + i, y0 + i, x1 - i, y0 + i), fill=light)
        draw.line((x1 - i, y0 + i, x1 - i, y1 - i, x0 + i, y1 - i), fill=dark)


def masked(texture, mask):
    out = Image.new('RGBA', texture.size, (0, 0, 0, 0))
    out.paste(texture, (0, 0), mask)
    return out


def frame_mask(size, rect, inner):
    mask = Image.new('L', size, 0)
    draw = ImageDraw.Draw(mask)
    draw.rectangle(rect, fill=255)
    draw.rectangle(inner, fill=0)
    return mask


def recolor(image, color):
    solid = Image.new('RGBA', image.size, color + (255,))
    solid.putalpha(image.getchannel('A'))
    return solid


def divider(layer, x0, x1, y):
    """A gold line with an ember glow and a diamond at its start, fading toward its end."""
    line = Image.new('RGBA', layer.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(line)
    steps = 64
    for i in range(steps):
        t = i / steps
        xa = u(x0) + (u(x1) - u(x0)) * t
        xb = u(x0) + (u(x1) - u(x0)) * (i + 1) / steps
        draw.line((xa, u(y), xb, u(y)), fill=GOLD + (int(255 * (1 - t) ** 1.4),), width=max(2, u(0.9)))
    layer = Image.alpha_composite(layer, recolor(line.filter(ImageFilter.GaussianBlur(u(1.6))), EMBER))
    layer = Image.alpha_composite(layer, line)
    draw = ImageDraw.Draw(layer)
    cx, cy, r = u(x0), u(y), u(3.2)
    draw.polygon([(cx - r, cy), (cx, cy - r), (cx + r, cy), (cx, cy + r)], fill=EMBER_HOT + (255,),
                 outline=IRON_DARK + (255,))
    return layer


def chrome():
    size = (u(PANEL_W), u(PANEL_H))
    layer = Image.new('RGBA', size, (0, 0, 0, 0))

    # The dark wash under the left column, and a vignette all round: the painting shows on the right.
    wash = Image.new('RGBA', size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(wash)
    reach = u(COLUMN_X + 30)
    for i in range(reach):
        t = i / reach
        draw.line((i, 0, i, size[1]), fill=(9, 5, 4, int(195 * (1 - t ** 3))))
    layer = Image.alpha_composite(layer, wash)
    vignette = Image.new('L', size, 0)
    ImageDraw.Draw(vignette).rectangle((u(40), u(40), size[0] - u(40), size[1] - u(40)), fill=255)
    vignette = vignette.filter(ImageFilter.GaussianBlur(u(30)))
    dark = Image.new('RGBA', size, (6, 3, 2, 200))
    dark.putalpha(vignette.point(lambda v: int((255 - v) * 0.6)))
    layer = Image.alpha_composite(layer, dark)
    # Holes where the icons and portraits show through: nothing over them.
    holes = Image.new('L', size, 255)
    draw = ImageDraw.Draw(holes)
    for i in range(4):
        draw.rectangle(box(REWARD_X + i * REWARD_STEP, REWARD_Y, REWARD_SIZE, REWARD_SIZE), fill=0)
    for i in range(10):
        cx, cy, r = u(SLOT_X + i * SLOT_STEP + SLOT_SIZE / 2), u(SLOT_Y + SLOT_SIZE / 2), u(SLOT_SIZE / 2)
        draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=0)
    alpha = layer.getchannel('A')
    layer.putalpha(Image.composite(alpha, holes, holes))

    # The title plate: a darker band with an ember underline.
    plate = Image.new('RGBA', size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(plate)
    for y in range(u(FRAME), u(TITLE_BOTTOM)):
        t = (y - u(FRAME)) / (u(TITLE_BOTTOM) - u(FRAME))
        draw.line((u(FRAME), y, u(COLUMN_X), y), fill=(20, 9, 6, int(150 + 70 * t)))
    layer = Image.alpha_composite(layer, plate)
    layer = divider(layer, FRAME + 6, COLUMN_X - 8, TITLE_BOTTOM)
    layer = divider(layer, 16, COLUMN_X - 12, REWARDS_DIVIDER)
    layer = divider(layer, 16, COLUMN_X - 12, GROUP_DIVIDER)

    # The rail between the columns.
    rail_rect = box(COLUMN_X - 2, FRAME, 4, STRIP_TOP - FRAME)
    rail_mask = Image.new('L', size, 0)
    ImageDraw.Draw(rail_mask).rectangle(rail_rect, fill=255)
    layer = glow_line(layer, [(u(COLUMN_X), u(FRAME)), (u(COLUMN_X), u(STRIP_TOP))], u(3), EMBER + (170,), u(3))
    layer = Image.alpha_composite(layer, masked(iron_texture(size), rail_mask))
    draw = ImageDraw.Draw(layer)
    bevel_rect(draw, rail_rect, max(1, u(0.6)), IRON_LIGHT + (255,), IRON_DARK + (255,))

    # The reward frames.
    for i in range(4):
        x, y = REWARD_X + i * REWARD_STEP, REWARD_Y
        outer = box(x - 4, y - 4, REWARD_SIZE + 8, REWARD_SIZE + 8)
        inner = box(x, y, REWARD_SIZE, REWARD_SIZE)
        layer = Image.alpha_composite(layer, masked(iron_texture(size), frame_mask(size, outer, inner)))
        draw = ImageDraw.Draw(layer)
        bevel_rect(draw, outer, max(1, u(0.8)), GOLD + (255,), (90, 58, 30, 255))
        bevel_rect(draw, (inner[0] - 2, inner[1] - 2, inner[2] + 2, inner[3] + 2), 2, IRON_DARK + (255,),
                   (140, 100, 60, 255))

    # The portrait rings.
    for i in range(10):
        cx = u(SLOT_X + i * SLOT_STEP + SLOT_SIZE / 2)
        cy = u(SLOT_Y + SLOT_SIZE / 2)
        r_out, r_in = u(SLOT_SIZE / 2 + 2.6), u(SLOT_SIZE / 2 - 0.4)
        ring = Image.new('L', size, 0)
        ImageDraw.Draw(ring).ellipse((cx - r_out, cy - r_out, cx + r_out, cy + r_out), fill=255)
        ImageDraw.Draw(ring).ellipse((cx - r_in, cy - r_in, cx + r_in, cy + r_in), fill=0)
        layer = Image.alpha_composite(layer, masked(iron_texture(size), ring))
        draw = ImageDraw.Draw(layer)
        width = max(1, u(0.7))
        draw.arc((cx - r_out, cy - r_out, cx + r_out, cy + r_out), 140, 320, fill=GOLD + (255,), width=width)
        draw.arc((cx - r_out, cy - r_out, cx + r_out, cy + r_out), 320, 140, fill=(70, 46, 26, 255), width=width)
        draw.ellipse((cx - r_in, cy - r_in, cx + r_in, cy + r_in), outline=IRON_DARK + (255,), width=2)

    # The record plaques: iron edged, dark glass, an ember band along the top.
    for x, y in PLAQUES:
        outer = box(x, y, PLAQUE_W, PLAQUE_H)
        inner = box(x + 3, y + 3, PLAQUE_W - 6, PLAQUE_H - 6)
        glass = Image.new('RGBA', size, (0, 0, 0, 0))
        ImageDraw.Draw(glass).rectangle(inner, fill=(10, 5, 4, 190))
        layer = Image.alpha_composite(layer, glass)
        band = Image.new('RGBA', size, (0, 0, 0, 0))
        draw = ImageDraw.Draw(band)
        for yy in range(inner[1], inner[1] + u(15)):
            t = (yy - inner[1]) / u(15)
            draw.line((inner[0], yy, inner[2], yy), fill=(120, 34, 10, int(200 * (1 - t))))
        layer = Image.alpha_composite(layer, band)
        layer = Image.alpha_composite(layer, masked(iron_texture(size), frame_mask(size, outer, inner)))
        draw = ImageDraw.Draw(layer)
        bevel_rect(draw, outer, max(1, u(0.7)), GOLD + (255,), (80, 52, 28, 255))
        for rx, ry in ((outer[0], outer[1]), (outer[2], outer[1]), (outer[0], outer[3]), (outer[2], outer[3])):
            rivet(draw, rx, ry, u(2.6))

    # The start picker's recessed slot.
    x, y, w, h = PICKER
    outer = box(x, y, w, h)
    inner = box(x + 3, y + 3, w - 6, h - 6)
    sunk = Image.new('RGBA', size, (0, 0, 0, 0))
    ImageDraw.Draw(sunk).rectangle(inner, fill=(6, 3, 2, 225))
    layer = Image.alpha_composite(layer, sunk)
    layer = Image.alpha_composite(layer, masked(iron_texture(size), frame_mask(size, outer, inner)))
    draw = ImageDraw.Draw(layer)
    bevel_rect(draw, outer, max(1, u(0.7)), (70, 52, 42, 255), GOLD + (255,))

    # The bottom strip.
    strip = box(FRAME, STRIP_TOP, PANEL_W - 2 * FRAME, PANEL_H - FRAME - STRIP_TOP)
    strip_mask = Image.new('L', size, 0)
    ImageDraw.Draw(strip_mask).rectangle(strip, fill=255)
    dim = Image.new('RGBA', size, (0, 0, 0, 0))
    ImageDraw.Draw(dim).rectangle(strip, fill=(8, 4, 3, 120))
    layer = Image.alpha_composite(layer, masked(iron_texture(size), strip_mask))
    layer = Image.alpha_composite(layer, dim)
    layer = glow_line(layer, [(strip[0], strip[1]), (strip[2], strip[1])], u(2), EMBER + (230,), u(2.2))
    draw = ImageDraw.Draw(layer)
    draw.line((strip[0], strip[1], strip[2], strip[1]), fill=EMBER_HOT + (255,), width=max(1, u(0.6)))

    # The frame: iron all round, an ember edge inside, riveted corner plates.
    outer = (0, 0, size[0] - 1, size[1] - 1)
    inner = box(FRAME, FRAME, PANEL_W - 2 * FRAME, PANEL_H - 2 * FRAME)
    layer = glow_line(layer, [(inner[0], inner[1]), (inner[2], inner[1]), (inner[2], inner[3]), (inner[0], inner[3]),
                              (inner[0], inner[1])], u(2.5), EMBER + (255,), u(3.5))
    layer = Image.alpha_composite(layer, masked(iron_texture(size), frame_mask(size, outer, inner)))
    draw = ImageDraw.Draw(layer)
    bevel_rect(draw, outer, max(2, u(1.2)), IRON_LIGHT + (255,), IRON_DARK + (255,))
    bevel_rect(draw, (inner[0] - u(1.5), inner[1] - u(1.5), inner[2] + u(1.5), inner[3] + u(1.5)), max(1, u(0.8)),
               IRON_DARK + (255,), (130, 96, 70, 255))
    draw.rectangle(inner, outline=EMBER_HOT + (255,), width=max(1, u(0.5)))
    corner = 30
    for cx, cy in ((0, 0), (PANEL_W - corner, 0), (0, PANEL_H - corner), (PANEL_W - corner, PANEL_H - corner)):
        plate = box(cx, cy, corner, corner)
        plate_mask = Image.new('L', size, 0)
        ImageDraw.Draw(plate_mask).rectangle(plate, fill=255)
        layer = Image.alpha_composite(layer, masked(iron_texture(size), plate_mask))
        draw = ImageDraw.Draw(layer)
        bevel_rect(draw, plate, max(2, u(1.1)), (128, 98, 78, 255), IRON_DARK + (255,))
        mx, my = (plate[0] + plate[2]) / 2, (plate[1] + plate[3]) / 2
        r = u(8)
        draw.polygon([(mx - r, my), (mx, my - r), (mx + r, my), (mx, my + r)], fill=(26, 18, 15, 255),
                     outline=GOLD + (255,))
        r2 = u(3.4)
        draw.polygon([(mx - r2, my), (mx, my - r2), (mx + r2, my), (mx, my + r2)], fill=EMBER + (255,))
        for rx, ry in ((plate[0] + u(5), plate[1] + u(5)), (plate[2] - u(5), plate[1] + u(5)),
                       (plate[0] + u(5), plate[3] - u(5)), (plate[2] - u(5), plate[3] - u(5))):
            rivet(draw, rx, ry, u(2.4))
    for x in range(60, PANEL_W - 50, 64):
        for y in (FRAME / 2, PANEL_H - FRAME / 2):
            rivet(draw, u(x), u(y), u(2.2))

    panel = layer.resize((1024, round(PANEL_H * SCALE)), Image.LANCZOS)
    canvas = Image.new('RGBA', (1024, 1024), (0, 0, 0, 0))
    canvas.paste(panel, (0, 0))
    return canvas, panel


def button(state):
    """The big button, 256x64: a crimson plate in a gold-edged iron rim."""
    s = 4
    w, h = 256 * s, 64 * s
    radius = 10 * s
    if state == 'highlight':
        glow = Image.new('RGBA', (w, h), (0, 0, 0, 0))
        ImageDraw.Draw(glow).rounded_rectangle((6 * s, 6 * s, w - 6 * s, h - 6 * s), radius, fill=(255, 150, 50, 150))
        return glow.filter(ImageFilter.GaussianBlur(5 * s)).resize((256, 64), Image.LANCZOS)
    image = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    rim = (150, 150, 150) if state == 'disabled' else GOLD
    draw.rounded_rectangle((2 * s, 2 * s, w - 2 * s, h - 2 * s), radius, fill=IRON_DARK + (255,))
    draw.rounded_rectangle((4 * s, 4 * s, w - 4 * s, h - 4 * s), radius - s, fill=rim + (255,))
    draw.rounded_rectangle((7 * s, 7 * s, w - 7 * s, h - 7 * s), radius - 2 * s, fill=(40, 22, 14, 255))
    top, bottom = {
        'normal': ((168, 34, 16), (74, 10, 5)),
        'pushed': ((96, 16, 8), (60, 8, 4)),
        'disabled': ((70, 62, 58), (40, 36, 34)),
    }[state]
    face = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    fd = ImageDraw.Draw(face)
    for y in range(10 * s, h - 10 * s):
        t = (y - 10 * s) / (h - 20 * s)
        fd.line((10 * s, y, w - 10 * s, y), fill=tuple(int(a + (b - a) * t) for a, b in zip(top, bottom)) + (255,))
    mask = Image.new('L', (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle((10 * s, 10 * s, w - 10 * s, h - 10 * s), radius - 3 * s, fill=255)
    image.paste(face, (0, 0), mask)
    draw = ImageDraw.Draw(image)
    if state == 'normal':
        draw.line((18 * s, 12 * s, w - 18 * s, 12 * s), fill=(255, 190, 120, 160), width=2 * s)
        ember = Image.new('RGBA', (w, h), (0, 0, 0, 0))
        ImageDraw.Draw(ember).ellipse((w * 0.2, h * 0.55, w * 0.8, h * 1.15), fill=(255, 110, 30, 110))
        image = Image.alpha_composite(image, masked(ember.filter(ImageFilter.GaussianBlur(10 * s)), mask))
    elif state == 'pushed':
        shade = Image.new('RGBA', (w, h), (0, 0, 0, 0))
        ImageDraw.Draw(shade).rounded_rectangle((10 * s, 10 * s, w - 10 * s, 22 * s), radius, fill=(0, 0, 0, 120))
        image = Image.alpha_composite(image, masked(shade.filter(ImageFilter.GaussianBlur(3 * s)), mask))
    return image.resize((256, 64), Image.LANCZOS)


def arrow(disabled=False):
    s = 4
    w = 32 * s
    image = Image.new('RGBA', (w, w), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    color = (110, 100, 94) if disabled else EMBER_HOT
    points = [(21 * s, 6 * s), (9 * s, 16 * s), (21 * s, 26 * s)]
    draw.line(points, fill=IRON_DARK + (255,), width=8 * s, joint='curve')
    draw.line(points, fill=color + (255,), width=4 * s, joint='curve')
    return image.resize((32, 32), Image.LANCZOS)


def slot():
    s = 4
    w = 64 * s
    image = Image.new('RGBA', (w, w), (0, 0, 0, 0))
    ImageDraw.Draw(image).ellipse((0, 0, w - 1, w - 1), fill=(10, 6, 5, 235))
    inner = Image.new('RGBA', (w, w), (0, 0, 0, 0))
    ImageDraw.Draw(inner).ellipse((w * 0.2, w * 0.2, w * 0.8, w * 0.8), fill=(120, 40, 12, 90))
    image = Image.alpha_composite(image, inner.filter(ImageFilter.GaussianBlur(8 * s)))
    return image.resize((64, 64), Image.LANCZOS)


def main():
    preview = Path(sys.argv[sys.argv.index('--preview') + 1]) if '--preview' in sys.argv else None
    OUT.mkdir(parents=True, exist_ok=True)
    for stale in ('Background.tga', 'Firepit.tga'):
        (OUT / stale).unlink(missing_ok=True)
    pieces = {}
    with Image.open(SOURCE) as source:
        pieces['Background'] = source.convert('RGBA').resize((1024, 512), Image.LANCZOS)
    (OUT / 'Background.blp').write_bytes(blp.encode_dxt(pieces['Background']))
    canvas, pieces['Chrome'] = chrome()
    (OUT / 'Chrome.blp').write_bytes(blp.encode_dxt(canvas))
    for state, name in (('normal', 'Button'), ('pushed', 'ButtonPushed'), ('highlight', 'ButtonHighlight'),
                        ('disabled', 'ButtonDisabled')):
        pieces[name] = button(state)
        pieces[name].save(OUT / f'{name}.tga')
    pieces['Arrow'] = arrow()
    pieces['Arrow'].save(OUT / 'Arrow.tga')
    pieces['ArrowDisabled'] = arrow(True)
    pieces['ArrowDisabled'].save(OUT / 'ArrowDisabled.tga')
    pieces['Slot'] = slot()
    pieces['Slot'].save(OUT / 'Slot.tga')
    for path in sorted(OUT.iterdir()):
        print(f'  {path.name} {path.stat().st_size // 1024} KB')
    if preview:
        preview.mkdir(parents=True, exist_ok=True)
        for name, image in pieces.items():
            image.save(preview / f'{name}.png')


if __name__ == '__main__':
    main()
