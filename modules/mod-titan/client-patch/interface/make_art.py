#!/usr/bin/env python3
"""Generate the Emberforge login-screen art (PNG) used by the custom GlueXML.

build_patch.py turns interface/art/*.png into Interface\\Glues\\Emberforge\\<name>.blp.
Usage: python3 make_art.py
"""

from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
ART = HERE / 'art'
SS = 4  # supersampling


def radial(size):
    n = size * SS
    y, x = np.mgrid[0:n, 0:n] + 0.5
    c = n / 2
    return np.hypot(x - c, y - c) / c, (y / n)


def save(rgba, name):
    img = Image.fromarray((np.clip(rgba, 0, 1) * 255).astype(np.uint8), 'RGBA')
    img = img.resize((img.width // SS, img.height // SS), Image.Resampling.LANCZOS)
    img.save(ART / f'{name}.png')


def ring(size=128):
    """Gold-ember portrait ring: bright top, darker bottom, transparent inside and out."""
    r, v = radial(size)
    inner, outer = 0.80, 0.97
    band = np.clip(np.minimum((r - inner) / 0.02, (outer - r) / 0.02), 0, 1)
    t = np.clip((r - inner) / (outer - inner), 0, 1)
    bevel = 0.65 + 0.35 * np.cos((t - 0.35) * np.pi)          # lighter ridge in the band
    shade = 1.0 - 0.45 * v                                      # light from above
    gold = np.stack([1.0 * np.ones_like(r), 0.72 + 0.1 * (1 - v), 0.30 + 0.1 * (1 - v)], -1)
    rgb = gold * (bevel * shade)[..., None]
    edge = np.clip(1 - np.minimum(np.abs(r - inner), np.abs(r - outer)) / 0.012, 0, 1)
    rgb = rgb * (1 - 0.6 * edge[..., None])                     # dark rims
    return np.dstack([rgb, band])


def glow(size=128, strength=1.0, color=(1.0, 0.45, 0.08)):
    """Additive ember glow around the ring (black = nothing added)."""
    r, _ = radial(size)
    g = np.exp(-((r - 0.86) / 0.07) ** 2) * strength
    rgb = np.stack([g * color[0], g * color[1], g * color[2]], -1)
    return np.dstack([rgb, np.ones_like(r)])


def hero_emblem(size=128):
    """The Emberforge W in a dark disc with the gold ring."""
    r, v = radial(size)
    disc = np.dstack([np.full_like(r, 0.07), np.full_like(r, 0.045), np.full_like(r, 0.03),
                      np.clip((0.86 - r) / 0.01, 0, 1)])
    warm = np.exp(-(r / 0.55) ** 2) * 0.35
    disc[..., 0] += warm
    disc[..., 1] += warm * 0.35
    n = size * SS
    logo_path = HERE.parent / 'icons' / 'emberforge-w.png'
    logo = Image.open(logo_path).convert('RGBA')
    logo.thumbnail((int(n * 0.62), int(n * 0.62)), Image.Resampling.LANCZOS)
    canvas = Image.new('RGBA', (n, n))
    canvas.paste(logo, ((n - logo.width) // 2, (n - logo.height) // 2 + n // 40), logo)
    lw = np.asarray(canvas).astype(np.float32) / 255
    rgb = disc[..., :3] * (1 - lw[..., 3:]) + lw[..., :3] * lw[..., 3:]
    alpha = np.maximum(disc[..., 3], lw[..., 3])
    out = np.dstack([rgb, alpha])
    rg = ring(size)
    out[..., :3] = out[..., :3] * (1 - rg[..., 3:]) + rg[..., :3] * rg[..., 3:]
    out[..., 3] = np.maximum(out[..., 3], rg[..., 3])
    return out


def vignette(width=16, height=256):
    """Vertical fade, transparent at the top to dark at the bottom."""
    v = (np.mgrid[0:height * SS, 0:width * SS][0] + 0.5) / (height * SS)
    a = np.clip(v, 0, 1) ** 1.6 * 0.9
    return np.dstack([np.full_like(v, 0.03), np.full_like(v, 0.02), np.full_like(v, 0.015), a])


def divider(width=256, height=16):
    """Thin ember line that fades out at both ends, with a bright center gem."""
    y, x = np.mgrid[0:height * SS, 0:width * SS] + 0.5
    u = x / (width * SS) * 2 - 1
    w = y / (height * SS) * 2 - 1
    line = np.exp(-(w / 0.12) ** 2) * (1 - np.abs(u) ** 2)
    gem = np.clip(1 - (np.abs(u) * 6 + np.abs(w) * 1.1), 0, 1)
    a = np.clip(np.maximum(line, gem), 0, 1)
    rgb = np.stack([np.ones_like(u), 0.62 + 0.3 * gem, 0.25 + 0.4 * gem], -1)
    return np.dstack([rgb, a])


PANEL_FILL = (0.045, 0.032, 0.026, 0.9)
PANEL_LINE = (0.86, 0.64, 0.32)


def rounded_piece(size, sdf):
    """Panel 9-slice piece from a signed distance function (negative inside the panel)."""
    n = size * SS
    y, x = (np.mgrid[0:n, 0:n] + 0.5) / SS
    d = sdf(x, y)
    fill = np.clip(-d * 1.5, 0, 1)
    line = np.clip(1 - np.abs(d + 0.6) / 0.7, 0, 1)
    rgb = np.empty((n, n, 3))
    for i in range(3):
        rgb[..., i] = PANEL_FILL[i] * (1 - line) + PANEL_LINE[i] * line
    alpha = np.maximum(fill * PANEL_FILL[3], line)
    return np.dstack([rgb, alpha])


def panel_corner(size=32, radius=10):
    """Top-left corner of a rounded panel; other corners are this one flipped."""
    c = 1 + radius

    def sdf(x, y):
        qx, qy = np.maximum(c - x, 0), np.maximum(c - y, 0)
        return np.where((x < c) & (y < c), np.hypot(qx, qy) - radius, np.maximum(1 - x, 1 - y))
    return rounded_piece(size, sdf)


def panel_edge(size=32, vertical=False):
    """Top edge (or left edge) of a rounded panel, stretched along its length."""
    return rounded_piece(size, (lambda x, y: 1 - x) if vertical else (lambda x, y: 1 - y))


def chevron(size=32, glow_only=False):
    """Round dark button with a gold left-pointing chevron (the right one is flipped)."""
    n = size * SS
    y, x = (np.mgrid[0:n, 0:n] + 0.5) / n * 2 - 1
    r = np.hypot(x, y)
    if glow_only:
        g = np.exp(-((r - 0.75) / 0.25) ** 2) * 0.8
        return np.dstack([g, g * 0.6, g * 0.25, np.ones_like(r)])
    disc = np.clip((0.86 - r) / 0.04, 0, 1)
    ring = np.clip(1 - np.abs(r - 0.83) / 0.06, 0, 1)
    # chevron: two strokes meeting at the left
    def seg(px, py, ax, ay, bx, by):
        vx, vy = bx - ax, by - ay
        t = np.clip(((px - ax) * vx + (py - ay) * vy) / (vx * vx + vy * vy), 0, 1)
        return np.hypot(px - (ax + t * vx), py - (ay + t * vy))
    d = np.minimum(seg(x, y, -0.22, 0.0, 0.18, -0.38), seg(x, y, -0.22, 0.0, 0.18, 0.38))
    chev = np.clip(1 - (d - 0.07) / 0.05, 0, 1)
    gold = np.array(PANEL_LINE)
    rgb = np.zeros((n, n, 3)) + np.array([0.09, 0.06, 0.05])
    rgb = rgb * (1 - ring[..., None]) + gold * ring[..., None]
    rgb = rgb * (1 - chev[..., None]) + np.array([1.0, 0.85, 0.5]) * chev[..., None]
    return np.dstack([rgb, np.maximum(disc, ring)])


def star_field(width=1024, height=512, seed=7):
    """Night sky for the login scene's dome: violet-blue nebula clouds and stars. Tiles both ways."""
    rng = np.random.default_rng(seed)
    y, x = np.mgrid[0:height, 0:width] / np.array([height, width])[:, None, None]

    def noise(scale, octaves):
        out = np.zeros((height, width))
        for o in range(octaves):
            f = scale * 2 ** o
            rows, cols = int(f), int(f * 2)
            g = rng.random((rows, cols))
            gy, gx = y * rows, x * cols
            iy, ix = gy.astype(int) % rows, gx.astype(int) % cols
            fy, fx = gy - np.floor(gy), gx - np.floor(gx)
            fy, fx = fy * fy * (3 - 2 * fy), fx * fx * (3 - 2 * fx)
            iy1, ix1 = (iy + 1) % rows, (ix + 1) % cols
            v = (g[iy, ix] * (1 - fx) + g[iy, ix1] * fx) * (1 - fy) + (g[iy1, ix] * (1 - fx) + g[iy1, ix1] * fx) * fy
            out += v / 2 ** o
        return out / (2 - 2 ** (1 - octaves))

    cloud = np.clip((noise(3, 5) - 0.45) * 2.2, 0, 1) ** 1.5
    # Break the clouds up with the Argus skybox's cloud noise (tiled; the dome repeats the texture).
    with Image.open(HERE.parent / 'models' / 'pantheon' / 'sky' / 'Nebula_Noise.png') as tile:
        tile = np.asarray(tile.convert('L').resize((width // 2, height // 2)), np.float64) / 255
    cloud = cloud * (0.35 + 1.3 * np.tile(tile, (2, 2)))
    tint = noise(2, 3)
    rgb = np.zeros((height, width, 3))
    rgb += np.array([0.02, 0.02, 0.06])
    rgb += cloud[..., None] * (np.array([0.22, 0.08, 0.32]) * tint[..., None] + np.array([0.05, 0.12, 0.30]) * (1 - tint[..., None]))
    for count, size, bright in ((2600, 0.6, 0.55), (420, 1.0, 0.85), (60, 1.6, 1.0)):
        sy, sx = rng.random(count) * height, rng.random(count) * width
        for cy, cx in zip(sy, sx):
            y0, y1, x0, x1 = int(cy - 3), int(cy + 4), int(cx - 3), int(cx + 4)
            yy, xx = np.mgrid[y0:y1, x0:x1]
            g = np.exp(-(((yy - cy) ** 2 + (xx - cx) ** 2) / (2 * size ** 2))) * bright * rng.uniform(0.6, 1.0)
            col = np.array([0.85, 0.9, 1.0]) if rng.random() < 0.8 else np.array([1.0, 0.85, 0.7])
            rgb[yy % height, xx % width] += g[..., None] * col
    return np.dstack([np.clip(rgb, 0, 1), np.ones((height, width))])


def gem_glow(size=128):
    """Additive glow for the scene's gems: a bright disc (the lit gem, white-hot at the center) inside
    a cyan halo fading to nothing at the edge."""
    r, _ = radial(size)
    fade = np.clip(1 - r, 0, 1) ** 1.5
    core = np.clip((0.30 - r) / 0.10, 0, 1) * (0.75 + 0.25 * np.exp(-(r / 0.12) ** 2))
    inner = np.exp(-(r / 0.38) ** 2) * 0.8
    halo = np.exp(-(r / 0.62) ** 2) * 0.55 * fade
    rgb = (core[..., None] * np.array([1.0, 1.0, 1.0]) + inner[..., None] * np.array([0.45, 0.85, 1.0])
           + halo[..., None] * np.array([0.10, 0.50, 1.0]))
    return np.dstack([np.clip(rgb, 0, 1), np.ones_like(r)])


def planet(size=256):
    """Distant rust-colored planet for the sky, lit from the upper right with a faint cyan rim."""
    r, _ = radial(size)
    n = size * SS
    y, x = (np.mgrid[0:n, 0:n] + 0.5) / (n / 2) - 1
    inside = r < 0.94
    z = np.sqrt(np.clip(0.94 ** 2 - x * x - y * y, 0, None)) / 0.94
    light = np.clip((x * 0.55 - y * 0.45 + z * 0.7) / 0.94, 0, 1)
    rng = np.random.default_rng(3)
    bands = 0.85 + 0.15 * np.sin(y * 9 + np.sin(x * 4) * 0.8 + rng.random() * 6)
    rgb = np.array([0.40, 0.24, 0.18]) * (light * bands)[..., None] * 0.8 + 0.02
    rim = np.clip((r - 0.82) / 0.12, 0, 1) * inside
    rgb += rim[..., None] * np.array([0.10, 0.25, 0.35])
    alpha = np.clip((0.94 - r) / 0.01, 0, 1) * 0.8         # a little of the nebula shows through
    return np.dstack([np.clip(rgb, 0, 1), alpha])


def mote(size=64):
    """Soft round particle for the scene's floating motes (additive: white center, fading out)."""
    r, _ = radial(size)
    g = np.exp(-(r / 0.35) ** 2) * np.clip(1 - r, 0, 1)
    return np.dstack([np.stack([g, g, g], -1), np.ones_like(r)])


def save_login(rgba, name):
    img = Image.fromarray((np.clip(rgba, 0, 1) * 255).astype(np.uint8), 'RGBA')
    img = img.resize((img.width // SS, img.height // SS), Image.Resampling.LANCZOS)
    img.save(HERE / 'login' / f'{name}.png')


def main():
    ART.mkdir(exist_ok=True)
    save(ring(), 'RaceRing')
    save(glow(strength=1.0), 'RaceRingSelected')
    save(glow(strength=0.45, color=(1.0, 0.75, 0.4)), 'RaceRingHover')
    save(hero_emblem(), 'HeroEmblem')
    save(vignette(), 'Vignette')
    save(divider(), 'Divider')
    save(panel_corner(), 'PanelCorner')
    save(panel_edge(), 'PanelEdge')
    save(panel_edge(vertical=True), 'PanelEdgeV')
    save(chevron(), 'Chevron')
    save(chevron(glow_only=True), 'ChevronHighlight')
    img = Image.fromarray((star_field() * 255).astype(np.uint8), 'RGBA')
    img.save(HERE / 'login' / 'StarField.png')
    save_login(gem_glow(), 'GemGlow')
    save_login(planet(), 'Planet')
    save_login(mote(), 'Mote')
    print('Wrote', ', '.join(sorted(p.name for p in ART.glob('*.png'))))


if __name__ == '__main__':
    main()
