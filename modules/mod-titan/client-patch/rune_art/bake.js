// The Legendary Rune panel's art, drawn with the "Legendary Rune Panel" mockup's own canvas code (its paintCard, its
// medallion, its glows) in headless Chromium, one PNG a piece. build_rune_art.py writes the job (job.json: the fonts,
// the runes, the themes) and turns the PNGs into the addon's BLPs and TGAs.
//
// Usage: node bake.js <job.json> <out dir>
'use strict';
const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright');

const PAGE = String.raw`
<!doctype html><html><head><style>
@font-face { font-family: 'Cinzel'; font-weight: 700; src: url('FONT_CINZEL_700'); }
@font-face { font-family: 'Cinzel'; font-weight: 900; src: url('FONT_CINZEL_900'); }
@font-face { font-family: 'Alegreya Sans'; font-weight: 400; src: url('FONT_ALEGREYA_400'); }
@font-face { font-family: 'Alegreya Sans'; font-weight: 500; src: url('FONT_ALEGREYA_500'); }
@font-face { font-family: 'Alegreya Sans'; font-weight: 700; src: url('FONT_ALEGREYA_700'); }
@font-face { font-family: 'Alegreya Sans'; font-style: italic; font-weight: 400; src: url('FONT_ALEGREYA_ITALIC'); }
</style></head><body></body></html>`;

// --- the mockup's helpers, as it has them ------------------------------------------------------------------------
const DRAW = String.raw`
// Seeded, so each build bakes the same stars and nebulae (mulberry32).
Math.random = (() => { let a = 0x52554e45; return () => { a |= 0; a = a + 0x6D2B79F5 | 0;
  let t = Math.imul(a ^ a >>> 15, 1 | a); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
  return ((t ^ t >>> 14) >>> 0) / 4294967296; }; })();
const rand = (a, b) => a + Math.random() * (b - a);
function roundRect(g, x, y, w, h, r) {
  g.beginPath(); g.moveTo(x + r, y); g.arcTo(x + w, y, x + w, y + h, r); g.arcTo(x + w, y + h, x, y + h, r);
  g.arcTo(x, y + h, x, y, r); g.arcTo(x, y, x + w, y, r); g.closePath();
}
function wrap(g, text, x, y, maxW, lh) {
  const words = text.split(' '); let line = '', yy = y;
  for (const w of words) {
    const t = line ? line + ' ' + w : w;
    if (g.measureText(t).width > maxW && line) { g.fillText(line, x, yy); line = w; yy += lh; } else line = t;
  }
  if (line) g.fillText(line, x, yy);
  return yy;
}
function canvas(w, h) { const c = document.createElement('canvas'); c.width = w; c.height = h; return c; }
const GOLD_STOPS = [[0, '#fbe3a6'], [0.35, '#b07a34'], [0.6, '#f6d48d'], [1, '#7a4f1e']];

// The mockup's paintCard: the card at CS pixels a point, into a w x h canvas (its top-left corner).
function paintCard(r, CS, w, h) {
  const CW = 300, CH = 440;
  const c = canvas(w, h);
  const g = c.getContext('2d'); g.scale(CS, CS);
  roundRect(g, 0, 0, CW, CH, 20); g.save(); g.clip();
  const bg = g.createLinearGradient(0, 0, 0, CH); bg.addColorStop(0, r.deep); bg.addColorStop(1, '#030207');
  g.fillStyle = bg; g.fillRect(0, 0, CW, CH);
  for (let i = 0; i < 5; i++) {
    const x = rand(40, CW - 40), y = rand(90, 230), rr = rand(60, 140);
    const n = g.createRadialGradient(x, y, 0, x, y, rr); n.addColorStop(0, r.glow + '0.20)'); n.addColorStop(1, r.glow + '0)');
    g.fillStyle = n; g.fillRect(0, 0, CW, CH);
  }
  for (let i = 0; i < 70; i++) { g.fillStyle = 'rgba(255,255,255,' + rand(0.05, 0.5) + ')'; g.fillRect(rand(10, CW - 10), rand(10, 260), 1, 1); }
  const pl = g.createLinearGradient(0, 250, 0, CH); pl.addColorStop(0, 'rgba(0,0,0,0)'); pl.addColorStop(0.25, 'rgba(4,2,10,0.75)');
  g.fillStyle = pl; g.fillRect(0, 240, CW, CH);
  g.restore();
  const gold = g.createLinearGradient(0, 0, CW, CH);
  for (const [at, colour] of GOLD_STOPS) gold.addColorStop(at, colour);
  g.lineWidth = 4; g.strokeStyle = gold; roundRect(g, 2, 2, CW - 4, CH - 4, 18); g.stroke();
  g.lineWidth = 1.2; g.strokeStyle = r.hue; g.globalAlpha = 0.8; roundRect(g, 9, 9, CW - 18, CH - 18, 13); g.stroke(); g.globalAlpha = 1;
  for (const [x, y] of [[14, 14], [CW - 14, 14], [14, CH - 14], [CW - 14, CH - 14]]) {
    g.save(); g.translate(x, y); g.rotate(Math.PI / 4); g.fillStyle = gold; g.fillRect(-5, -5, 10, 10);
    g.fillStyle = r.hue; g.fillRect(-2, -2, 4, 4); g.restore();
  }
  g.font = '700 12px Cinzel'; g.textAlign = 'center'; g.fillStyle = '#ffb35c';
  g.letterSpacing = '4px'; g.fillText('LEGENDARY', CW / 2, 38); g.letterSpacing = '0px';
  const ex = CW / 2, ey = 146;
  g.lineWidth = 3; g.strokeStyle = gold; g.beginPath(); g.arc(ex, ey, 66, 0, Math.PI * 2); g.stroke();
  g.lineWidth = 1; g.strokeStyle = r.hue; g.beginPath(); g.arc(ex, ey, 58, 0, Math.PI * 2); g.stroke();
  for (let i = 0; i < 24; i++) { const a = i / 24 * Math.PI * 2; g.fillStyle = i % 6 ? 'rgba(232,194,122,0.5)' : gold;
    g.beginPath(); g.arc(ex + Math.cos(a) * 66, ey + Math.sin(a) * 66, i % 6 ? 1.2 : 2.6, 0, Math.PI * 2); g.fill(); }
  const well = g.createRadialGradient(ex, ey, 0, ex, ey, 56); well.addColorStop(0, r.glow + '0.35)'); well.addColorStop(1, '#05020b');
  g.fillStyle = well; g.beginPath(); g.arc(ex, ey, 56, 0, Math.PI * 2); g.fill();
  g.save(); g.translate(ex - 36, ey - 36); g.scale(3, 3); g.lineWidth = 1.4; g.lineCap = 'round'; g.lineJoin = 'round';
  g.shadowColor = r.hue; g.shadowBlur = 8; g.strokeStyle = '#fff'; g.stroke(new Path2D(r.glyph));
  g.shadowBlur = 0; g.lineWidth = 0.8; g.strokeStyle = r.hue; g.stroke(new Path2D(r.glyph)); g.restore();
  g.fillStyle = '#ffffff'; g.font = '700 22px Cinzel'; g.shadowColor = '#000'; g.shadowBlur = 4;
  let y = wrap(g, r.name, CW / 2, 254, CW - 50, 27); g.shadowBlur = 0;
  g.font = '500 14px "Alegreya Sans"'; g.fillStyle = r.hue; g.letterSpacing = '2px';
  g.fillText(r.school.toUpperCase(), CW / 2, y + 26); g.letterSpacing = '0px';
  g.font = '400 16px "Alegreya Sans"'; g.fillStyle = r.theme.card; y = wrap(g, r.text, CW / 2, y + 54, CW - 54, 21);
  g.font = 'italic 400 13px "Alegreya Sans"'; g.fillStyle = r.theme.from; wrap(g, 'From your ' + r.from, CW / 2, CH - 34, CW - 54, 17);
  return c;
}

// The mockup's sprite: a soft round glow, white at its heart.
function sprite(colour, size) {
  const s = canvas(size, size), g = s.getContext('2d'), h = size / 2;
  const gr = g.createRadialGradient(h, h, 0, h, h, h);
  gr.addColorStop(0, 'rgba(255,255,255,1)'); gr.addColorStop(0.18, colour); gr.addColorStop(1, 'rgba(0,0,0,0)');
  g.fillStyle = gr; g.fillRect(0, 0, size, size); return s;
}
// A glow with no white heart, white (the addon tints it): auras, halos.
function soft(size) {
  const s = canvas(size, size), g = s.getContext('2d'), h = size / 2;
  const gr = g.createRadialGradient(h, h, 0, h, h, h);
  gr.addColorStop(0, 'rgba(255,255,255,1)'); gr.addColorStop(1, 'rgba(255,255,255,0)');
  g.fillStyle = gr; g.fillRect(0, 0, size, size); return s;
}

// The medallion (R 94 points), in a square of SPAN points about its centre: its core and glyph, its ring's segment
// (the one facing right, gold), the segment's inner line and glow (white: tinted), the inner rings.
const R = 94, SPAN = 260;
function square(size) { const c = canvas(size, size), g = c.getContext('2d'); g.translate(size / 2, size / 2);
  g.scale(size / SPAN, size / SPAN); return [c, g]; }
function core(theme, glyph, size) {
  const [c, g] = square(size);
  const grad = g.createRadialGradient(0, 0, 0, 0, 0, R * 0.78);
  grad.addColorStop(0, theme.core); grad.addColorStop(1, '#07030f');
  g.fillStyle = grad; g.beginPath(); g.arc(0, 0, R * 0.78, 0, Math.PI * 2); g.fill();
  g.strokeStyle = 'rgba(232,194,122,0.8)'; g.lineWidth = 2; g.stroke();
  const gs = R * 0.055; g.translate(-12 * gs, -12 * gs); g.scale(gs, gs);
  g.lineCap = 'round'; g.lineJoin = 'round'; g.shadowColor = theme.hue; g.shadowBlur = 14 * size / SPAN;
  g.lineWidth = 1.5; g.strokeStyle = '#ffffff'; g.stroke(new Path2D(glyph));
  return c;
}
function segGold(size) {
  const [c, g] = square(size);
  const gold = g.createLinearGradient(-R, -R, R, R); gold.addColorStop(0, '#fbe3a6'); gold.addColorStop(0.5, '#b07a34');
  gold.addColorStop(1, '#f6d48d');
  g.lineWidth = 9; g.strokeStyle = gold; g.beginPath(); g.arc(0, 0, R, -0.95, 0.95); g.stroke(); return c;
}
function segLine(size) {
  const [c, g] = square(size);
  g.lineWidth = 2; g.strokeStyle = '#fff'; g.beginPath(); g.arc(0, 0, R - 11, -0.88, 0.88); g.stroke(); return c;
}
function segGlow(size) {
  const [c, g] = square(size);
  g.filter = 'blur(' + (9 * size / SPAN) + 'px)';
  g.lineWidth = 12; g.strokeStyle = '#fff'; g.beginPath(); g.arc(0, 0, R, -0.95, 0.95); g.stroke(); return c;
}
function ringGold(size) {
  const [c, g] = square(size);
  g.lineWidth = 1.5; g.strokeStyle = 'rgba(232,194,122,0.7)'; g.beginPath(); g.arc(0, 0, R * 0.92, 0, Math.PI * 2);
  g.stroke(); return c;
}
function ringDash(size) {
  const [c, g] = square(size);
  g.setLineDash([2, 7]); g.lineWidth = 1; g.strokeStyle = '#fff'; g.beginPath(); g.arc(0, 0, R * 1.02, 0, Math.PI * 2);
  g.stroke(); return c;
}
// The mockup's rays (drawRays), white, at spin 0: 18 wedges fading out.
function rays(size) {
  const c = canvas(size, size), g = c.getContext('2d'), r = size / 2;
  g.translate(r, r);
  for (let i = 0; i < 18; i++) {
    const a = i / 18 * Math.PI * 2, w = 0.05 + 0.04 * Math.sin(i * 7.3), len = r * (0.65 + 0.35 * Math.sin(i * 3.1));
    const gr = g.createLinearGradient(0, 0, Math.cos(a) * len, Math.sin(a) * len);
    gr.addColorStop(0, 'rgba(255,255,255,0.9)'); gr.addColorStop(1, 'rgba(255,255,255,0)');
    g.fillStyle = gr; g.beginPath(); g.moveTo(0, 0); g.arc(0, 0, len, a - w, a + w); g.closePath(); g.fill();
  }
  return c;
}
// A shockwave at its radius (the size's half): the band and its thin line.
function shockBand(size) {
  const c = canvas(size, size), g = c.getContext('2d'), r = size / 2;
  const band = g.createRadialGradient(r, r, r * 0.82, r, r, r);
  band.addColorStop(0, 'rgba(255,255,255,0)'); band.addColorStop(0.85, 'rgba(255,255,255,1)');
  band.addColorStop(1, 'rgba(255,255,255,0)');
  g.fillStyle = band; g.beginPath(); g.arc(r, r, r, 0, Math.PI * 2); g.fill(); return c;
}
function shockLine(size) {
  const c = canvas(size, size), g = c.getContext('2d'), r = size / 2;
  g.lineWidth = 1.5 * size / 256; g.strokeStyle = '#fff'; g.beginPath(); g.arc(r, r, r * 0.97 - 1, 0, Math.PI * 2);
  g.stroke(); return c;
}
// The mockup's #dim: the world darkened toward the edges.
function dim(size) {
  const c = canvas(size, size), g = c.getContext('2d'), r = size / 2;
  const gr = g.createRadialGradient(r, r * 0.9, 0, r, r * 0.9, r * 1.25);
  gr.addColorStop(0, 'rgba(5,3,10,0.15)'); gr.addColorStop(1, 'rgba(5,3,10,0.62)');
  g.fillStyle = gr; g.fillRect(0, 0, size, size); return c;
}

// The option card (.opt: 340 x 172, radius 16) with its glows, at SCALE pixels a point, MARGIN points around it.
function option(theme, hover, scale, margin, size) {
  const c = canvas(size[0], size[1]), g = c.getContext('2d');
  g.scale(scale, scale); g.translate(margin, margin);
  const W = 340, H = 172, acc = theme.rgb, hot = theme.hot;
  roundRect(g, 0, 0, W, H, 16);
  g.shadowColor = 'rgba(' + acc + ',' + (hover ? 0.6 : 0.25) + ')'; g.shadowBlur = hover ? 40 : 22;
  g.fillStyle = 'rgba(14,7,26,0.72)'; g.fill(); g.shadowBlur = 0;
  g.save(); roundRect(g, 0, 0, W, H, 16); g.clip();
  g.shadowColor = 'rgba(' + acc + ',' + (hover ? 0.22 : 0.12) + ')'; g.shadowBlur = hover ? 36 : 30;
  g.lineWidth = 30; g.strokeStyle = 'rgba(0,0,0,1)'; roundRect(g, -15, -15, W + 30, H + 30, 30); g.stroke();
  g.restore();
  g.lineWidth = 1; g.strokeStyle = hover ? 'rgba(' + hot + ',0.95)' : 'rgba(' + acc + ',0.55)';
  roundRect(g, 0.5, 0.5, W - 1, H - 1, 16); g.stroke();
  return c;
}
// The pill button (46 points tall, radius 23), 106 points wide: its ends and a middle to stretch.
function pill(theme, hover, scale, margin, size) {
  const c = canvas(size[0], size[1]), g = c.getContext('2d');
  g.scale(scale, scale); g.translate(margin, margin);
  const W = 106 - 2 * margin, H = 46, acc = theme.rgb;
  roundRect(g, 0, 0, W, H, 23);
  g.shadowColor = 'rgba(' + acc + ',' + (hover ? 0.55 : 0.25) + ')'; g.shadowBlur = hover ? 30 : 18;
  g.fillStyle = hover ? 'rgba(40,20,70,0.75)' : 'rgba(16,8,30,0.62)'; g.fill(); g.shadowBlur = 0;
  g.lineWidth = 1; g.strokeStyle = 'rgba(' + acc + ',0.7)'; roundRect(g, 0.5, 0.5, W - 1, H - 1, 23); g.stroke();
  return c;
}
// A slot's number disc (.num, 54 points): plain, active (its glow beats in the addon), done.
function num(theme, kind, scale, size) {
  const c = canvas(size, size), g = c.getContext('2d');
  g.translate(size / 2, size / 2); g.scale(scale, scale);
  const acc = theme.rgb, hot = theme.hot;
  g.beginPath(); g.arc(0, 0, 26, 0, Math.PI * 2);
  if (kind === 'plain') {
    g.fillStyle = 'rgba(10,5,18,0.8)'; g.fill();
    g.lineWidth = 2; g.strokeStyle = 'rgba(' + acc + ',0.3)'; g.stroke();
  } else if (kind === 'active') {
    g.shadowColor = 'rgba(' + acc + ',0.8)'; g.shadowBlur = 22; g.fillStyle = 'rgba(10,5,18,0.8)'; g.fill();
    g.shadowBlur = 0; g.save(); g.clip(); g.shadowColor = 'rgba(' + acc + ',0.5)'; g.shadowBlur = 12;
    g.lineWidth = 12; g.strokeStyle = '#000'; g.beginPath(); g.arc(0, 0, 32, 0, Math.PI * 2); g.stroke(); g.restore();
    g.lineWidth = 2; g.strokeStyle = 'rgb(' + acc + ')'; g.beginPath(); g.arc(0, 0, 26, 0, Math.PI * 2); g.stroke();
  } else {
    g.shadowColor = 'rgba(' + hot + ',0.9)'; g.shadowBlur = 26; g.fillStyle = 'rgb(' + hot + ')'; g.fill();
    g.shadowBlur = 0; g.lineWidth = 2; g.strokeStyle = '#fff'; g.stroke();
  }
  return c;
}
// A modifier's icon (the mockup's 48-point disc, its line drawing inside).
function icon(theme, d, size) {
  const c = canvas(size, size), g = c.getContext('2d'); g.scale(size / 48, size / 48);
  g.beginPath(); g.arc(24, 24, 22, 0, Math.PI * 2); g.fillStyle = '#0b0514'; g.fill();
  g.lineWidth = 1.5; g.strokeStyle = theme.hue; g.stroke();
  g.translate(12, 12); g.lineWidth = 2; g.lineCap = 'round'; g.lineJoin = 'round'; g.strokeStyle = theme.inner;
  g.stroke(new Path2D(d)); return c;
}
// A line of the mockup's lettering, as CSS draws it (letter spacing, glow, shadow): white where the addon tints it.
function label(spec, scale) {
  const probe = canvas(8, 8).getContext('2d');
  probe.font = spec.font; probe.letterSpacing = spec.spacing || '0px';
  const width = Math.ceil(probe.measureText(spec.text).width) + 2 * spec.pad, height = spec.height + 2 * spec.pad;
  const c = canvas(Math.ceil(width * scale), Math.ceil(height * scale)), g = c.getContext('2d');
  g.scale(scale, scale); g.font = spec.font; g.letterSpacing = spec.spacing || '0px';
  g.textBaseline = 'middle'; g.textAlign = 'center';
  const x = width / 2, y = height / 2;
  for (const glow of spec.glows || []) { g.shadowColor = glow[0]; g.shadowBlur = glow[1]; g.fillStyle = spec.color;
    g.fillText(spec.text, x, y); }
  g.shadowColor = spec.shadow || 'rgba(0,0,0,0)'; g.shadowBlur = spec.shadowBlur || 0; g.shadowOffsetY = spec.shadowY || 0;
  g.fillStyle = spec.color; g.fillText(spec.text, x, y);
  return { canvas: c, width, height };
}
`;

async function main() {
    const [jobPath, outDir] = process.argv.slice(2);
    const job = JSON.parse(fs.readFileSync(jobPath, 'utf8'));
    fs.mkdirSync(outDir, { recursive: true });
    let html = PAGE;
    for (const [key, file] of Object.entries(job.fonts)) {
        html = html.replace(key, 'data:font/ttf;base64,' + fs.readFileSync(file).toString('base64'));
    }
    const browser = await chromium.launch();
    const page = await browser.newPage();
    await page.setContent(html);
    await page.addScriptTag({ content: DRAW });
    await page.evaluate(() => Promise.all([document.fonts.load('700 22px Cinzel'), document.fonts.load('900 22px Cinzel'),
        document.fonts.load('400 16px "Alegreya Sans"'), document.fonts.load('500 14px "Alegreya Sans"'),
        document.fonts.load('700 14px "Alegreya Sans"'), document.fonts.load('italic 400 13px "Alegreya Sans"')]));
    const pieces = await page.evaluate((job) => {
        const out = {};
        const put = (name, c) => { out[name] = c.toDataURL('image/png'); };
        for (const r of job.runes) {
            put('Card' + r.id, paintCard(r, job.card.scale, job.card.size, job.card.size));
            if (r.built) put('Core' + r.id, core(r.theme, r.glyph, job.core));
        }
        for (const [name, theme] of Object.entries(job.themes)) {
            put('Sprite' + name, sprite('rgb(' + theme.rgb + ')', 64));
            put('SpriteHot' + name, sprite('rgb(' + theme.hot + ')', 64));
            put('Option' + name, option(theme, false, job.option.scale, job.option.margin, job.option.size));
            put('OptionHot' + name, option(theme, true, job.option.scale, job.option.margin, job.option.size));
            put('Pill' + name, pill(theme, false, job.pill.scale, job.pill.margin, job.pill.size));
            put('PillHot' + name, pill(theme, true, job.pill.scale, job.pill.margin, job.pill.size));
            for (const kind of ['plain', 'active', 'done']) {
                put('Num' + kind[0].toUpperCase() + kind.slice(1) + name, num(theme, kind, job.num.scale, job.num.size));
            }
        }
        for (const r of job.runes) {
            (r.icons || []).forEach((d, i) => put('Icon' + r.id + '_' + (i + 1), icon(r.theme, d, job.icon)));
        }
        for (const [name, colour] of Object.entries(job.sprites)) put(name, sprite(colour, 64));
        put('Soft', soft(128));
        put('SegGold', segGold(job.seg)); put('SegLine', segLine(job.seg)); put('SegGlow', segGlow(job.seg));
        put('RingGold', ringGold(job.seg)); put('RingDash', ringDash(job.seg));
        put('Rays', rays(512)); put('ShockBand', shockBand(256)); put('ShockLine', shockLine(256)); put('Dim', dim(256));
        const sizes = {};
        for (const [name, spec] of Object.entries(job.labels)) {
            const made = label(spec, job.labelScale);
            put(name, made.canvas); sizes[name] = [made.width, made.height];
        }
        out.__labels = JSON.stringify(sizes);
        return out;
    }, job);
    for (const [name, url] of Object.entries(pieces)) {
        if (name === '__labels') {
            fs.writeFileSync(path.join(outDir, 'labels.json'), url);
            continue;
        }
        fs.writeFileSync(path.join(outDir, name + '.png'), Buffer.from(url.split(',')[1], 'base64'));
    }
    await browser.close();
    console.log('baked ' + (Object.keys(pieces).length - 1) + ' pieces');
}

main().catch((error) => { console.error(error); process.exit(1); });
