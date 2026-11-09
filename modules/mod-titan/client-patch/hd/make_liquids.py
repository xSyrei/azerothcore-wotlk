#!/usr/bin/env python3
"""HD liquids: new animated textures for the client's water, ocean, fast water, lava, slime, green lava and orange
slime (LiquidType.dbc's XTextures\\...\\<name>.<1-30>.blp), at 512x512 where the originals are 256x256.

Made, not upscaled: every liquid is time-periodic Fourier noise (each wave a whole number of times across the tile
and a whole number of times around the 30 frames), so it tiles seamlessly and loops without a seam. Its colours and
alpha are then given the original's distribution, by rank (liquid_stats.json: 256 quantiles of each original's
colour, sorted by brightness, and of its alpha; statistics, not the textures), so the engine blends them as it did:
- lake_a, ocean_h: the glint masks over the water colour the game draws itself, which lays their alpha over its
  water as light. Made for calm, realistic water (not the originals' distribution: large patches of alpha read as
  stones in game): fine wind ripples in a low, even haze and tiny, sharp sun glints, nothing larger (the texture
  repeats across a lake, so a larger shape repeats as a grid). The ocean's ripples run with the wind.
- fast_a: grey foam streaks scrolling a whole tile per loop.
- lava, lavagreen: glowing liquid around dark crust, warped; slime, LavaOrange: smooth slow swirls.

Usage: python3 make_liquids.py <out dir> [--preview]   (writes XTextures/... BLPs; --preview: PNG sheets only)
"""

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'models'))
import blp  # noqa: E402

SIZE = 512
FRAMES = 30
STATS = json.loads((HERE / 'liquid_stats.json').read_text())

# Name -> (archive path pattern, kind, parameters).
LIQUIDS = {
    'lake_a': ('XTextures\\river\\lake_a.{}.blp', 'glint', dict(kmin=36, kmax=230, beta=1.7, speed=1.6, seed=11)),
    'ocean_h': ('XTextures\\ocean\\ocean_h.{}.blp', 'glint',
                dict(kmin=24, kmax=230, beta=1.8, speed=1.4, seed=12, stretch=(1.0, 1.8), angle=35, haze=26.0,
                     ripple=11.0, glints=0.006)),
    'fast_a': ('XTextures\\river\\fast_a.{}.blp', 'flow', dict(kmin=6, kmax=220, beta=1.25, speed=2.0, seed=13)),
    'lava': ('XTextures\\lava\\lava.{}.blp', 'crust', dict(kmin=3, kmax=40, beta=2.0, speed=0.8, seed=14)),
    'lavagreen': ('XTextures\\LavaGreen\\lavagreen.{}.blp', 'crust',
                  dict(kmin=4, kmax=50, beta=2.2, speed=0.8, seed=15, islands=True, grain_amount=0.03)),
    'slime': ('XTextures\\slime\\slime.{}.blp', 'swirl', dict(kmin=2, kmax=30, beta=2.4, speed=0.7, seed=16)),
    'LavaOrange': ('XTEXTURES\\LavaOrange\\LavaOrange.{}.blp', 'swirl',
                   dict(kmin=2, kmax=30, beta=2.4, speed=0.7, seed=17)),
}


def periodic_noise(rng, kmin, kmax, beta, speed, stretch=(1.0, 1.0), scroll=0, angle=0.0):
    """FRAMES fields of tileable noise looping over the frames: a power-law band of waves, each turning a whole
    number of times per loop (faster for shorter waves), plus scroll whole tiles per loop along y."""
    k1 = np.fft.fftfreq(SIZE) * SIZE
    kx, ky = np.meshgrid(k1, k1)
    c, s = np.cos(np.radians(angle)), np.sin(np.radians(angle))
    k = np.hypot((kx * c + ky * s) * stretch[0], (-kx * s + ky * c) * stretch[1])
    amp = np.where((k >= kmin) & (k <= kmax), np.maximum(k, 1) ** -beta, 0.0)
    phase = rng.uniform(0, 2 * np.pi, amp.shape)
    turns = np.maximum(1, np.round(speed * np.sqrt(k / max(kmin, 1)))) * rng.choice([-1, 1], amp.shape)
    turns = turns + scroll * ky
    fields = []
    for t in range(FRAMES):
        spectrum = amp * np.exp(1j * (phase + 2 * np.pi * turns * t / FRAMES))
        field = np.real(np.fft.ifft2(spectrum))
        fields.append((field - field.mean()) / (field.std() + 1e-9))
    return fields


def warp(field, dx, dy, strength):
    """field sampled at (x + dx, y + dy) * strength, wrapping (bilinear)."""
    y, x = np.mgrid[0:SIZE, 0:SIZE].astype(float)
    sx, sy = (x + dx * strength) % SIZE, (y + dy * strength) % SIZE
    x0, y0 = np.floor(sx).astype(int), np.floor(sy).astype(int)
    fx, fy = sx - x0, sy - y0
    x1, y1 = (x0 + 1) % SIZE, (y0 + 1) % SIZE
    return (field[y0, x0] * (1 - fx) * (1 - fy) + field[y0, x1] * fx * (1 - fy)
            + field[y1, x0] * (1 - fx) * fy + field[y1, x1] * fx * fy)


def rank(field):
    """Each pixel's place in the field, 0-1."""
    flat = field.ravel()
    order = np.argsort(flat, kind='stable')
    ranks = np.empty(flat.size)
    ranks[order] = np.linspace(0, 1, flat.size)
    return ranks.reshape(field.shape)


def by_rank(ranks, quantiles):
    """The original's distribution at these ranks (its 256 quantiles, interpolated)."""
    q = np.asarray(quantiles, dtype=float)
    pos = ranks * (len(q) - 1)
    lo = np.floor(pos).astype(int)
    hi = np.minimum(lo + 1, len(q) - 1)
    f = (pos - lo)[..., None] if q.ndim > 1 else pos - lo
    return q[lo] * (1 - f) + q[hi] * f


def glint_frames(name, kmin, kmax, beta, speed, seed, stretch=(1.0, 1.0), angle=0.0, haze=22.0, ripple=17.0,
                 glints=0.006):
    """Calm, realistic water over the colour the game draws: the engine lays this texture's alpha over its water as
    light, so big patches of it read as stones. Fine wind ripples in a low, even haze (haze +- ripple alpha, out of
    255), and sun glints only on the steepest facets: tiny, sparse and sharp (the brightest glints share of the
    pixels). Nothing larger than a few ripples: the texture repeats across a lake, and a larger shape would repeat as
    a grid. Lakes ripple every way; the ocean's ripples run with the wind."""
    rng = np.random.default_rng(seed)
    ripples = periodic_noise(rng, kmin, kmax, beta, speed, stretch, angle=angle)
    sun = np.array([0.35, -0.25, 0.9]); sun /= np.linalg.norm(sun)
    half = sun + np.array([0, 0, 1.0]); half /= np.linalg.norm(half)
    frames = []
    for h in ripples:
        # No shape larger than a few ripples: the game repeats the texture across a lake, and any larger shape repeats
        # with it as a grid of squares.
        height = h
        gx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * 0.5
        gy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * 0.5
        n = np.stack([-gx, -gy, np.full_like(height, 0.55)], -1)
        n /= np.linalg.norm(n, axis=-1, keepdims=True)
        shade = n @ sun
        shade = (shade - shade.mean()) / (shade.std() + 1e-9)
        spec = n @ half
        # The glints: the given share of the brightest facets, sharpened to points.
        cut = np.quantile(spec, 1.0 - glints)
        glint = np.clip((spec - cut) / (spec.max() - cut + 1e-9), 0, 1) ** 0.5
        alpha = np.clip(haze + ripple * np.tanh(shade * 0.8) + 225.0 * glint, 0, 255)
        lum = np.clip(4.0 + 3.0 * np.tanh(shade * 0.8) + 200.0 * glint, 0, 255)
        frames.append(np.dstack([lum, lum, lum, alpha]))
    return frames


def color_frames(name, kind, kmin, kmax, beta, speed, seed, islands=False, grain_amount=0.14):
    rng = np.random.default_rng(seed)
    scroll = 1 if kind == 'flow' else 0
    stretch = (1.0, 1.3) if kind == 'flow' else (1.0, 1.0)
    base = periodic_noise(rng, kmin, kmax, beta, speed, stretch, scroll)
    if islands:
        # The larger of two fields: dark only where both are low, so the crust breaks into separate islands.
        other = periodic_noise(rng, kmin, kmax, beta, speed, stretch, scroll)
        base = [np.maximum(a, b) for a, b in zip(base, other)]
    # Fine grain over the shapes (the glow's texture), scrolling with them.
    grain = periodic_noise(rng, 30, 180, 1.4, 2.5, scroll=scroll)
    wx = periodic_noise(rng, 1, 10, 2.5, 0.6)
    wy = periodic_noise(rng, 1, 10, 2.5, 0.6)
    strength = {'crust': 10.0, 'swirl': 28.0, 'flow': 4.0}[kind]
    palette = STATS[name]['palette']
    frames = []
    for f, g, dx, dy in zip(base, grain, wx, wy):
        field = warp(f, dx, dy, strength)
        if kind == 'crust':
            # Crust: dark blobs with a crisp edge (the low values), the glow between them grainy.
            crust = np.tanh(field * 1.1)
            glow = np.clip((crust + 0.3) / 0.8, 0, 1)
            field = crust + grain_amount * g * glow * glow
        elif kind == 'swirl':
            # Swirls: the bright ridge lines where the warped field crosses zero.
            field = -np.abs(field) * 0.55 + 0.45 * f + 0.05 * g
        else:
            field = field + 0.45 * g
        rgb = by_rank(rank(field), palette)
        frames.append(np.dstack([rgb, np.full((SIZE, SIZE), 255.0)]))
    return frames


def frames_for(name):
    _, kind, params = LIQUIDS[name]
    if kind == 'glint':
        return glint_frames(name, **params)
    return color_frames(name, kind, **params)


def to_image(frame):
    return Image.fromarray(np.clip(np.round(frame), 0, 255).astype(np.uint8), 'RGBA')


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    preview = '--preview' in sys.argv
    for name, (pattern, kind, _) in LIQUIDS.items():
        frames = frames_for(name)
        if preview:
            show = []
            for frame in (frames[0], frames[10], frames[20]):
                img = frame.copy()
                if kind == 'glint':  # brighten the mask to see it: colour x4 | alpha
                    img = np.concatenate([np.clip(img[..., :3] * 4, 0, 255),
                                          np.repeat(img[..., 3:4], 3, -1)], 1)
                    img = np.dstack([img, np.full(img.shape[:2], 255.0)])
                show.append(img)
            sheet = np.concatenate(show, 0)
            Image.fromarray(np.clip(sheet, 0, 255).astype(np.uint8), 'RGBA').save(out / f'{name}.png')
            continue
        for i, frame in enumerate(frames, 1):
            path = out / pattern.format(i).replace('\\', '/')
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(blp.encode_dxt(to_image(frame)))
        print(f'{name}: {FRAMES} frames, {SIZE}x{SIZE}')


if __name__ == '__main__':
    main()
