#!/usr/bin/env python3
"""Generate the Classless window's art, in the character creation screen's style (make_art.py).

Writes AddOns/Emberforge_Classless/Art/*.tga: addons load TGA from their own folder, so the window
needs no client patch. Pieces are white where the addon tints them (panel fills and edges, icon
frames, arrows); the rest carry their own colors.
Usage: python3 make_classless_art.py
"""

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

from make_art import HERE, SS, divider, radial

OUT = HERE.parent / 'AddOns' / 'Emberforge_Classless' / 'Art'


def save(rgba, name):
    img = Image.fromarray((np.clip(rgba, 0, 1) * 255).astype(np.uint8), 'RGBA')
    img = img.resize((img.width // SS, img.height // SS), Image.Resampling.LANCZOS)
    img.save(OUT / f'{name}.tga')


def white(alpha):
    return np.dstack([np.ones_like(alpha), np.ones_like(alpha), np.ones_like(alpha), alpha])


def corner_sdf(size, radius):
    """Signed distance to a rounded panel's top-left corner (negative inside), in texels."""
    n = size * SS
    y, x = (np.mgrid[0:n, 0:n] + 0.5) / SS
    c = 1 + radius
    qx, qy = np.maximum(c - x, 0), np.maximum(c - y, 0)
    return np.where((x < c) & (y < c), np.hypot(qx, qy) - radius, np.maximum(1 - x, 1 - y))


def edge_sdf(size, vertical):
    n = size * SS
    y, x = (np.mgrid[0:n, 0:n] + 0.5) / SS
    return 1 - (x if vertical else y)


def fill(d):
    return white(np.clip(-d * 1.5, 0, 1))


def line(d, width):
    return white(np.clip(1 - np.abs(d + width) / width, 0, 1))


def icon_frame(size=64, radius=6, width=1.6):
    """Rounded square outline around an icon (tinted gold); transparent inside and out."""
    n = size * SS
    y, x = (np.mgrid[0:n, 0:n] + 0.5) / SS
    h = size / 2
    qx, qy = np.abs(x - h) - (h - 2 - radius), np.abs(y - h) - (h - 2 - radius)
    d = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - radius
    return line(d, width)


def icon_glow(size=64):
    """Additive ember glow around a square icon (black adds nothing)."""
    n = size * SS
    y, x = (np.mgrid[0:n, 0:n] + 0.5) / n * 2 - 1
    d = (np.abs(x) ** 4 + np.abs(y) ** 4) ** 0.25          # a square with rounded corners
    g = np.exp(-((d - 0.58) / 0.2) ** 2) * 0.5 * np.clip((1 - d) / 0.25, 0, 1)
    return np.dstack([g, g * 0.5, g * 0.12, np.ones_like(g)])


def arrow(size=32, right=False):
    """Soft-edged white arrowhead pointing down (or right)."""
    n = size * SS
    y, x = (np.mgrid[0:n, 0:n] + 0.5) / n * 2 - 1
    if right:
        x, y = y, x
    # triangle: top edge at y=-0.45, tip at y=0.55
    d = np.maximum(-0.45 - y, np.abs(x) * 1.1 + (y - 0.55) * 0.95)
    return white(np.clip(-d / 0.06, 0, 1))


def close_button(size=64):
    """Round dark button with a gold ring and X, like the creation screen's chevrons."""
    n = size * SS
    y, x = (np.mgrid[0:n, 0:n] + 0.5) / n * 2 - 1
    r = np.hypot(x, y)
    disc = np.clip((0.86 - r) / 0.04, 0, 1)
    rim = np.clip(1 - np.abs(r - 0.83) / 0.06, 0, 1)
    d = np.minimum(np.abs(x - y), np.abs(x + y)) / np.sqrt(2)
    cross = np.clip(1 - (d - 0.06) / 0.05, 0, 1) * np.clip((0.36 - np.maximum(np.abs(x), np.abs(y))) / 0.04, 0, 1)
    rgb = np.zeros((n, n, 3)) + np.array([0.09, 0.06, 0.05])
    rgb = rgb * (1 - rim[..., None]) + np.array([0.86, 0.64, 0.32]) * rim[..., None]
    rgb = rgb * (1 - cross[..., None]) + np.array([1.0, 0.85, 0.5]) * cross[..., None]
    return np.dstack([rgb, np.maximum(disc, rim)])


def magnifier(size=32):
    n = size * SS
    y, x = (np.mgrid[0:n, 0:n] + 0.5) / n * 2 - 1
    lens = np.clip(1 - np.abs(np.hypot(x + 0.15, y + 0.15) - 0.42) / 0.1, 0, 1)
    t = np.clip(((x - 0.18) + (y - 0.18)) / 2, 0, 0.5)
    handle = np.clip(1 - (np.hypot(x - 0.18 - t, y - 0.18 - t) - 0.06) / 0.06, 0, 1)
    return white(np.maximum(lens, handle))


def header_glow(width=256, height=64):
    """Warm light falling from the top edge of the window (additive)."""
    y, x = (np.mgrid[0:height * SS, 0:width * SS] + 0.5) / np.array([height * SS, width * SS])[:, None, None]
    g = np.exp(-((x - 0.5) / 0.3) ** 2) * np.exp(-(y / 0.55) ** 2) * 0.5
    return np.dstack([g, g * 0.42, g * 0.12, np.ones_like(g)])


def shade(size=128):
    """Dark vignette over the talent tree art: clear in the middle, dark at the edges."""
    r, _ = radial(size)
    n = size * SS
    y, x = (np.mgrid[0:n, 0:n] + 0.5) / n * 2 - 1
    d = np.maximum(np.abs(x), np.abs(y)) * 0.6 + r * 0.4
    a = np.clip((d - 0.55) / 0.45, 0, 1) ** 1.4 * 0.85
    return np.dstack([np.full_like(a, 0.02), np.full_like(a, 0.015), np.full_like(a, 0.01), a])


def thin_ring(size=128, inner=0.90, outer=0.965):
    """Slim gold portrait ring (the creation screen's ring, a third as thick)."""
    r, v = radial(size)
    band = np.clip(np.minimum((r - inner) / 0.012, (outer - r) / 0.012), 0, 1)
    t = np.clip((r - inner) / (outer - inner), 0, 1)
    bevel = 0.7 + 0.3 * np.cos((t - 0.4) * np.pi)
    shade = 1.0 - 0.4 * v
    gold = np.stack([np.ones_like(r), 0.74 + 0.1 * (1 - v), 0.34 + 0.1 * (1 - v)], -1)
    return np.dstack([gold * (bevel * shade)[..., None], band])


def ring_glow(size=128, strength=1.0, color=(1.0, 0.45, 0.08)):
    """Additive ember glow hugging the thin ring (peaks at 0.8 of the texture's radius)."""
    r, _ = radial(size)
    g = np.exp(-((r - 0.8) / 0.09) ** 2) * strength * np.clip((1 - r) / 0.1, 0, 1)
    return np.dstack([g * color[0], g * color[1], g * color[2], np.ones_like(r)])


def emblem(size=128):
    """The Emberforge W on a dark disc inside the thin ring."""
    r, _ = radial(size)
    n = size * SS
    warm = np.exp(-(r / 0.55) ** 2) * 0.35
    disc = np.dstack([0.07 + warm, 0.045 + warm * 0.35, np.full_like(r, 0.03), np.clip((0.93 - r) / 0.01, 0, 1)])
    logo = Image.open(HERE.parent / 'icons' / 'emberforge-w.png').convert('RGBA')
    logo.thumbnail((int(n * 0.6), int(n * 0.6)), Image.Resampling.LANCZOS)
    canvas = Image.new('RGBA', (n, n))
    canvas.paste(logo, ((n - logo.width) // 2, (n - logo.height) // 2 + n // 40), logo)
    lw = np.asarray(canvas).astype(np.float64) / 255
    rgb = disc[..., :3] * (1 - lw[..., 3:]) + lw[..., :3] * lw[..., 3:]
    out = np.dstack([rgb, np.maximum(disc[..., 3], lw[..., 3])])
    rg = thin_ring(size)
    out[..., :3] = out[..., :3] * (1 - rg[..., 3:]) + rg[..., :3] * rg[..., 3:]
    out[..., 3] = np.maximum(out[..., 3], rg[..., 3])
    return out


def rays(size=256, count=14):
    """Light burst for the jackpot (additive): soft rays from the center, gone before the edge so the
    texture can spin."""
    n = size * SS
    y, x = (np.mgrid[0:n, 0:n] + 0.5) / n * 2 - 1
    r, a = np.hypot(x, y), np.arctan2(y, x)
    beams = np.abs(np.cos(a * count / 2)) ** 10
    fade = np.clip(1 - r / 0.95, 0, 1) ** 1.6
    g = (beams * 0.75 + 0.25) * fade + np.exp(-(r / 0.22) ** 2) * 0.6
    return np.dstack([g, g * 0.62, g * 0.25, np.ones_like(g)])


def spark(size=32):
    """Soft ember spark (additive)."""
    r, _ = radial(size)
    g = np.exp(-(r / 0.3) ** 2) * np.clip(1 - r, 0, 1)
    return np.dstack([g, g * 0.7, g * 0.35, np.ones_like(g)])


# The Wildcard die: the project's ember cube (icons/wildcard-die.png, keyed out of its background). It
# cracks along jagged lines from its center gem (ORIGIN) and bursts into SHARDS pieces.
DIE_SIZE = 256
SHARDS = 6
ORIGIN = (0.5, 0.49)


def die(size=DIE_SIZE):
    n = size * SS
    with Image.open(HERE.parent / 'icons' / 'wildcard-die.png') as art:
        art = art.convert('RGBA').resize((n, n), Image.Resampling.LANCZOS)
    return np.asarray(art).astype(np.float64) / 255


def shard_lines(n, seed=11):
    """Jagged crack lines from the front corner out past the edge, one per shard boundary (pixels)."""
    rng = np.random.default_rng(seed)
    ox, oy = ORIGIN[0] * n, ORIGIN[1] * n
    lines = []
    for k in range(SHARDS):
        base = np.radians(-75 + 360 / SHARDS * k)
        points = [(ox, oy)]
        for radius in (0.12, 0.25, 0.4, 0.55, 0.75, 1.4):
            angle = base + rng.uniform(-0.18, 0.18) * (radius < 1)
            points.append((ox + radius * n * np.cos(angle), oy + radius * n * np.sin(angle)))
        lines.append(points)
    return lines


def shard(index, size=DIE_SIZE):
    """One piece of the die: the die inside the wedge between two crack lines."""
    n = size * SS
    lines = shard_lines(n)
    mask = Image.new('L', (n, n))
    wedge = lines[index] + lines[(index + 1) % SHARDS][::-1]
    ImageDraw.Draw(mask).polygon(wedge, fill=255)
    out = die(size)
    out[..., 3] *= np.asarray(mask).astype(np.float64) / 255
    return out


def cracks(size=DIE_SIZE):
    """Glowing ember cracks along the shard lines (additive), only on the die, drawn as it shakes."""
    n = size * SS
    img = Image.new('L', (n, n))
    draw = ImageDraw.Draw(img)
    for points in shard_lines(n):
        draw.line(points, fill=255, width=max(2, n // 110), joint='curve')
    inside = Image.fromarray((die(size)[..., 3] * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(9))
    inside = np.asarray(inside).astype(np.float64) / 255
    core = np.asarray(img).astype(np.float64) / 255 * inside
    halo = np.asarray(Image.fromarray((core * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(n / 60)))
    g = np.clip(core + halo.astype(np.float64) / 255 * 2.4, 0, 1)
    return np.dstack([g, g * (0.55 + 0.4 * core), g * (0.15 + 0.6 * core), np.ones_like(g)])


# Class portraits the client lacks (it has no Demon Hunter or Monk): the art inside its own rim,
# made round for the window's gold ring. Source, center and outer rim radius in source pixels.
CLASS_PORTRAITS = {
    'ClassDemonHunter': ('class-demonhunter.jpg', 125, 125, 116 * 0.82),
    'ClassMonk': ('class-monk.png', 127.5, 128, 109.5 * 0.88),
}


def class_portrait(source, cx, cy, radius, size=128):
    with Image.open(HERE.parent / 'icons' / source) as art:
        art = art.convert('RGBA').crop((round(cx - radius), round(cy - radius), round(cx + radius),
                                        round(cy + radius))).resize((size, size), Image.Resampling.LANCZOS)
    n = size * SS
    mask = Image.new('L', (n, n))
    ImageDraw.Draw(mask).ellipse((SS, SS, n - SS - 1, n - SS - 1), fill=255)
    art.putalpha(mask.resize((size, size), Image.Resampling.LANCZOS))
    return art


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    # Panel 9-slices: large (16 px corners, for panels) and small (8 px, for cells, tabs, rows).
    # Shown at half their texel size; the small set's line is thicker to stay one pixel wide.
    for suffix, size, radius, width in (('', 32, 10, 0.6), ('Small', 16, 6, 1.0)):
        save(fill(corner_sdf(size, radius)), f'FillCorner{suffix}')
        save(line(corner_sdf(size, radius), width), f'LineCorner{suffix}')
        save(line(edge_sdf(size, False), width), f'LineEdge{suffix}')
        save(line(edge_sdf(size, True), width), f'LineEdgeV{suffix}')
    save(thin_ring(), 'Ring')
    save(ring_glow(), 'RingSelected')
    save(ring_glow(strength=0.45, color=(1.0, 0.75, 0.4)), 'RingHover')
    save(emblem(), 'HeroEmblem')
    save(rays(), 'Rays')
    save(spark(), 'Spark')
    save(die(), 'Die')
    save(cracks(), 'DieCracks')
    for index in range(SHARDS):
        save(shard(index), f'DieShard{index + 1}')
    save(divider(), 'Divider')
    save(icon_frame(), 'IconFrame')
    save(icon_glow(), 'IconGlow')
    save(arrow(), 'ArrowDown')
    save(arrow(right=True), 'ArrowRight')
    save(close_button(), 'Close')
    save(magnifier(), 'Search')
    save(header_glow(), 'HeaderGlow')
    save(shade(), 'Shade')
    for name, (source, cx, cy, radius) in CLASS_PORTRAITS.items():
        class_portrait(source, cx, cy, radius).save(OUT / f'{name}.tga')
    print('Wrote', ', '.join(sorted(p.name for p in OUT.glob('*.tga'))))


if __name__ == '__main__':
    main()
