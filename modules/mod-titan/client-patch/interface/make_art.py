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


def star_field(width=2048, height=1024, seed=7):
    """Night sky for the login scene's dome, after Argus's skybox: a deep blue-violet nebula with
    brighter cores and dark dust lanes, many fine dim stars and a few bright ones. Tiles both ways."""
    rng = np.random.default_rng(seed)
    y, x = np.mgrid[0:height, 0:width] / np.array([height, width])[:, None, None]

    def noise(scale, octaves, gain=0.5):
        out, total = np.zeros((height, width)), 0.0
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
            out += v * gain ** o
            total += gain ** o
        return out / total

    clouds = np.clip((noise(2, 7) * 0.75 + noise(4, 5) * 0.45 - 0.48) * 2.2, 0, 1) ** 1.3
    hue = np.clip((noise(2, 3) - 0.35) * 2.5, 0, 1)
    # Dark dust lanes from the Argus skybox's cloud noise (tiled to keep the sky seamless).
    with Image.open(HERE.parent / 'models' / 'pantheon' / 'sky' / 'Nebula_Noise.png') as tile:
        tile = np.asarray(tile.convert('L').resize((width // 4, height // 2)), np.float64) / 255
    dust = np.clip((0.52 - np.tile(tile, (2, 4))) * 3.0, 0, 1) * np.clip(noise(3, 3) * 1.6 - 0.3, 0, 1)

    blue, violet, magenta = np.array([0.10, 0.17, 0.48]), np.array([0.30, 0.11, 0.46]), np.array([0.45, 0.10, 0.34])
    color = blue * (1 - hue[..., None]) + violet * hue[..., None]
    color += magenta * np.clip((hue - 0.75) * 3, 0, 1)[..., None] * 0.6
    rgb = np.array([0.012, 0.016, 0.05]) + clouds[..., None] * color * (1 - 0.75 * dust[..., None])
    core = np.clip((clouds - 0.72) * 3.5, 0, 1) * (1 - dust)
    rgb += core[..., None] * np.array([0.18, 0.32, 0.55]) * 0.6

    def star(cy, cx, sigma, bright, col, halo=0.0):
        reach = int(np.ceil(sigma * 3 + (12 if halo else 0)))
        yy, xx = np.mgrid[int(cy) - reach:int(cy) + reach + 1, int(cx) - reach:int(cx) + reach + 1]
        d2 = (yy - cy) ** 2 + (xx - cx) ** 2
        g = np.exp(-d2 / (2 * sigma ** 2)) * bright
        if halo:
            g = g + np.exp(-d2 / (2 * (sigma * 5) ** 2)) * halo
        rgb[yy % height, xx % width] += g[..., None] * col

    dim = 1 - 0.65 * dust
    for count, sigma, (lo, hi), halo in ((4200, 0.55, (0.15, 0.45), 0.0), (380, 0.8, (0.45, 0.8), 0.0),
                                         (28, 1.1, (0.9, 1.0), 0.12)):
        for cy, cx in zip(rng.random(count) * height, rng.random(count) * width):
            col = (np.array([0.80, 0.88, 1.0]) if rng.random() < 0.85
                   else np.array([1.0, 0.85, 0.7]) if rng.random() < 0.6 else np.array([0.75, 0.7, 1.0]))
            star(cy, cx, sigma, rng.uniform(lo, hi) * dim[int(cy), int(cx)], col, halo)
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
    """Distant rust-colored planet for the sky: a crescent lit from the lower left (where the nebula's
    bright burst is), its night side faint, with a thin glowing atmosphere around the lit limb."""
    n = size * SS
    y, x = (np.mgrid[0:n, 0:n] + 0.5) / (n / 2) - 1
    R = 0.78
    r = np.hypot(x, y)
    z = np.sqrt(np.clip(R * R - x * x - y * y, 0, None)) / R
    light_dir = np.array([-0.65, 0.45, 0.62])
    light_dir /= np.linalg.norm(light_dir)
    lambert = np.clip((x / R) * light_dir[0] + (y / R) * light_dir[1] + z * light_dir[2], 0, 1)
    rng = np.random.default_rng(3)
    phase = rng.random() * 6
    bands = 0.82 + 0.18 * np.sin(y * 11 + np.sin(x * 5 + phase) * 1.2)
    surface = np.array([0.55, 0.30, 0.20]) * bands[..., None]
    rgb = surface * (lambert ** 0.8)[..., None] + np.array([0.025, 0.02, 0.04])
    inside = np.clip((R - r) / 0.006, 0, 1)
    limb = np.clip(1 - np.abs(r - R) / 0.07, 0, 1) ** 2
    lit_side = np.clip((x * light_dir[0] + y * light_dir[1]) / (r + 1e-6) * 0.8 + 0.5, 0, 1)
    glow = limb * lit_side
    rgb = rgb * inside[..., None] + glow[..., None] * np.array([0.35, 0.55, 0.95])
    alpha = np.clip(inside * 0.92 + glow * 0.8, 0, 1)
    return np.dstack([np.clip(rgb, 0, 1), alpha])


def violet_glow(size=256):
    """Soft violet light for the sky (additive), balancing the blue burst."""
    r, _ = radial(size)
    g = np.exp(-(r / 0.45) ** 2) * np.clip(1 - r, 0, 1) ** 1.5
    return np.dstack([g[..., None] * np.array([0.42, 0.16, 0.62]), np.ones_like(r)])


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
    save_login(violet_glow(), 'VioletGlow')
    print('Wrote', ', '.join(sorted(p.name for p in ART.glob('*.png'))))


if __name__ == '__main__':
    main()
