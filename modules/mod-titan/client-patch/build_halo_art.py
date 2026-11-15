#!/usr/bin/env python3
"""Bake the Halo window's art (Emberforge_Runes, Halo.lua) into AddOns/Emberforge_Runes/Art/Halo*.tga.

Each piece is an SVG rendered by headless Chromium (Playwright, via node) at its power-of-two size, transparent
around it. White and grey pieces are tinted by the addon (rings, sockets, gems, rune marks); the Legendary's
segmented ring is gold already.

Usage: python3 build_halo_art.py
"""

import json
import subprocess
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
OUT = HERE / 'AddOns' / 'Emberforge_Runes' / 'Art'

GOLD = ('<linearGradient id="g" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#fbe3a6"/>'
        '<stop offset=".35" stop-color="#b07a34"/><stop offset=".6" stop-color="#f6d48d"/>'
        '<stop offset="1" stop-color="#7a4f1e"/></linearGradient>')
# The rune marks: line drawings on a 24 grid (the Halo mockup's).
GLYPHS = [
    'M12 3v18M7 7l5 4 5-4M7 17l5-4 5 4',
    'M6 4l12 16M18 4L6 20M12 2v4',
    'M12 3l7 9-7 9-7-9zM12 8v8',
    'M5 20L12 4l7 16M8 14h8',
    'M7 3v18M17 3v18M7 9l10 6',
    'M12 3a6 6 0 0 1 0 12M12 3a6 6 0 0 0 0 12M12 15v6',
    'M4 12h16M12 4v16M7 7l10 10',
    'M12 3l8 6-3 11H7L4 9z',
    'M6 20V8l6-5 6 5v12M9 14h6',
    'M5 6h14M5 12h14M12 6v14',
]


def svg(size, body, defs=''):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 {size} {size}">'
            f'<defs>{defs}</defs>{body}</svg>')


def tall(width, height, view_w, view_h, body, defs=''):
    """A non-square piece: drawn on a view_w x view_h canvas, stretched to a power-of-two texture; the addon draws
    it back at view_w x view_h, so its proportions come back."""
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
            f'viewBox="0 0 {view_w} {view_h}" preserveAspectRatio="none"><defs>{defs}</defs>{body}</svg>')


# The offer card: 240 x 360, corners cut by 16.
CARD = 'M16 1H224L239 16V344L224 359H16L1 344V16Z'
CARD_INNER = 'M19 8H221L232 19V341L221 352H19L8 341V19Z'


def card_pieces(out):
    # The frame (tinted by rarity): the cut outline, a faint inner line, crest and foot diamonds, corner marks.
    marks = ''.join(f'<path d="{d}" fill="none" stroke="#fff" stroke-width="1.6" stroke-opacity=".8"/>' for d in (
        'M4 40V22L22 4H40', 'M200 4H218L236 22V40', 'M4 320V338L22 356H40', 'M200 356H218L236 338V320'))
    out['HaloCardFrame'] = tall(256, 512, 240, 360,
                                f'<path d="{CARD}" fill="none" stroke="#fff" stroke-width="2"/>'
                                f'<path d="{CARD_INNER}" fill="none" stroke="#fff" stroke-width="1" '
                                f'stroke-opacity=".28"/>{marks}'
                                '<path d="M120 -6L127 1L120 8L113 1Z" fill="#fff"/>'
                                '<path d="M120 352L127 359L120 366L113 359Z" fill="#fff"/>')
    # The body (untinted): dark, lighter at the top, a darker foot.
    out['HaloCardFill'] = tall(256, 512, 240, 360,
                               f'<path d="{CARD}" fill="url(#v)"/><path d="{CARD}" fill="url(#t)"/>',
                               '<linearGradient id="v" x1="0" y1="0" x2="0" y2="1">'
                               '<stop offset="0" stop-color="#1f1a26"/><stop offset=".55" stop-color="#110f16"/>'
                               '<stop offset="1" stop-color="#08070b"/></linearGradient>'
                               '<radialGradient id="t" cx=".5" cy=".18" r=".55">'
                               '<stop offset="0" stop-color="#ffffff" stop-opacity=".07"/>'
                               '<stop offset="1" stop-color="#ffffff" stop-opacity="0"/></radialGradient>')
    # The light behind the rune (tinted, added): an ellipse at the top, and a band along the frame.
    out['HaloCardGlow'] = tall(256, 512, 240, 360,
                               '<ellipse cx="120" cy="86" rx="120" ry="110" fill="url(#r)"/>'
                               f'<path d="{CARD}" fill="none" stroke="url(#e)" stroke-width="10"/>',
                               '<radialGradient id="r"><stop offset="0" stop-color="#fff" stop-opacity=".9"/>'
                               '<stop offset=".45" stop-color="#fff" stop-opacity=".25"/>'
                               '<stop offset="1" stop-color="#fff" stop-opacity="0"/></radialGradient>'
                               '<linearGradient id="e" x1="0" y1="0" x2="0" y2="1">'
                               '<stop offset="0" stop-color="#fff" stop-opacity=".5"/>'
                               '<stop offset="1" stop-color="#fff" stop-opacity=".08"/></linearGradient>')
    # A divider: a line fading out both ways, a diamond in the middle.
    out['HaloDivider'] = tall(256, 32, 240, 16,
                              '<rect x="0" y="7.4" width="240" height="1.2" fill="url(#l)"/>'
                              '<path d="M120 2L126 8L120 14L114 8Z" fill="none" stroke="#fff" stroke-width="1.2"/>'
                              '<path d="M120 5.5L122.5 8L120 10.5L117.5 8Z" fill="#fff"/>',
                              '<linearGradient id="l"><stop offset="0" stop-color="#fff" stop-opacity="0"/>'
                              '<stop offset=".5" stop-color="#fff"/><stop offset="1" stop-color="#fff" '
                              'stop-opacity="0"/></linearGradient>')


def pieces():
    out = {}
    out['HaloRing'] = svg(512, '<circle cx="256" cy="256" r="252" fill="none" stroke="#fff" stroke-width="3"/>')
    ticks = ''.join(f'<line x1="256" y1="4" x2="256" y2="{14 if i % 6 else 22}" stroke="#fff" stroke-width="2" '
                    f'transform="rotate({i * 5} 256 256)"/>' for i in range(72))
    out['HaloTicks'] = svg(512, ticks)
    out['HaloFill'] = svg(128, '<circle cx="64" cy="64" r="61" fill="url(#f)"/>',
                          '<radialGradient id="f" cx=".5" cy=".38" r=".62"><stop offset="0" stop-color="#2a2430"/>'
                          '<stop offset="1" stop-color="#07060b"/></radialGradient>')
    out['HaloSocketRing'] = svg(128, '<circle cx="64" cy="64" r="60" fill="none" stroke="#fff" stroke-width="4"/>')
    out['HaloSocketDash'] = svg(128, '<circle cx="64" cy="64" r="60" fill="none" stroke="#fff" stroke-width="3" '
                                     'stroke-dasharray="7 8"/>')
    seg = 2 * 3.14159 * 118 / 3 - 14
    out['HaloSegments'] = svg(256, f'<circle cx="128" cy="128" r="118" fill="none" stroke="url(#g)" '
                                   f'stroke-width="7" stroke-linecap="round" stroke-dasharray="{seg:.1f} 14" '
                                   f'transform="rotate(-86 128 128)"/>', GOLD)
    out['HaloGold'] = svg(256, '<circle cx="128" cy="128" r="124" fill="none" stroke="url(#g)" stroke-width="5"/>',
                          GOLD)
    facets = ('<linearGradient id="a" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#ffffff"/>'
              '<stop offset=".5" stop-color="#cfcfcf"/><stop offset="1" stop-color="#8a8a8a"/></linearGradient>'
              '<linearGradient id="b" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#a8a8a8"/>'
              '<stop offset="1" stop-color="#2e2e2e"/></linearGradient>')
    out['HaloGemHex'] = svg(128, '<g transform="translate(4 4) scale(3)">'
                                 '<polygon points="20,2 36,11 36,29 20,38 4,29 4,11" fill="url(#b)" stroke="#fff" '
                                 'stroke-opacity=".5" stroke-width=".6"/>'
                                 '<polygon points="20,10 29,15 29,25 20,30 11,25 11,15" fill="url(#a)"/>'
                                 '<path d="M20 2v8M36 11l-7 4M36 29l-7-4M20 38v-8M4 29l7-4M4 11l7 4" stroke="#fff" '
                                 'stroke-opacity=".35" stroke-width=".6"/>'
                                 '<polygon points="11,15 20,10 20,14 14,17" fill="#fff" fill-opacity=".7"/></g>',
                            facets)
    out['HaloGemDiamond'] = svg(128, '<g transform="translate(4 4) scale(3)">'
                                     '<polygon points="20,2 34,20 20,38 6,20" fill="url(#b)" stroke="#fff" '
                                     'stroke-opacity=".5" stroke-width=".6"/>'
                                     '<polygon points="20,10 27,20 20,30 13,20" fill="url(#a)"/>'
                                     '<path d="M20 2v8M34 20h-7M20 38v-8M6 20h7" stroke="#fff" stroke-opacity=".35" '
                                     'stroke-width=".6"/><polygon points="13,20 20,10 20,15 16,20" fill="#fff" '
                                     'fill-opacity=".7"/></g>', facets)
    for i, d in enumerate(GLYPHS, 1):
        out[f'HaloGlyph{i}'] = svg(64, f'<g transform="translate(4 4) scale(2.333)"><path d="{d}" fill="none" '
                                       f'stroke="#fff" stroke-width="1.7" stroke-linecap="round" '
                                       f'stroke-linejoin="round"/></g>')
    out['HaloPlus'] = svg(32, '<path d="M16 6v20M6 16h20" stroke="#fff" stroke-width="3" stroke-linecap="round"/>')
    out['HaloCorner'] = svg(64, '<path d="M2 62V2h60" fill="none" stroke="#fff" stroke-width="3"/>')
    card_pieces(out)
    return out


RENDER = r"""
const { chromium } = require('playwright');
const fs = require('fs');
(async () => {
  const jobs = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
  const browser = await chromium.launch();
  const page = await browser.newPage();
  for (const job of jobs) {
    await page.setViewportSize({ width: job.width, height: job.height });
    await page.setContent(`<html><body style="margin:0;background:transparent">${job.svg}</body></html>`);
    await page.screenshot({ path: job.out, omitBackground: true,
      clip: { x: 0, y: 0, width: job.width, height: job.height } });
  }
  await browser.close();
})();
"""


def main():
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        jobs = []
        for name, text in pieces().items():
            width = int(text.split('width="', 1)[1].split('"', 1)[0])
            height = int(text.split('height="', 1)[1].split('"', 1)[0])
            jobs.append({'name': name, 'width': width, 'height': height, 'svg': text,
                         'out': str(tmp / f'{name}.png')})
        (tmp / 'jobs.json').write_text(json.dumps(jobs))
        (tmp / 'render.js').write_text(RENDER)
        subprocess.run(['node', str(tmp / 'render.js'), str(tmp / 'jobs.json')], check=True, cwd=tmp)
        for job in jobs:
            Image.open(job['out']).convert('RGBA').save(OUT / f'{job["name"]}.tga')
            print(f'  {job["name"]}.tga ({job["width"]} x {job["height"]})')


if __name__ == '__main__':
    main()
