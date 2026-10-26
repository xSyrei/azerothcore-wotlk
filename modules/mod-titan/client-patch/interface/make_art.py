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
    g = np.exp(-((r - 0.88) / 0.10) ** 2) * strength
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


def main():
    ART.mkdir(exist_ok=True)
    save(ring(), 'RaceRing')
    save(glow(strength=1.0), 'RaceRingSelected')
    save(glow(strength=0.45, color=(1.0, 0.75, 0.4)), 'RaceRingHover')
    save(hero_emblem(), 'HeroEmblem')
    save(vignette(), 'Vignette')
    save(divider(), 'Divider')
    print('Wrote', ', '.join(sorted(p.name for p in ART.glob('*.png'))))


if __name__ == '__main__':
    main()
