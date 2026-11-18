'use strict';
/* =====================================================================================================================
   The Effect Suite's reader of the game's own files, for the in-client Suite (.ees): what build_assets.py does, in
   the page, on demand. A client is { read(path) -> Promise<ArrayBuffer|null> } (the launcher's archive helper).

   - dbc(buffer): a client table's rows (by id) and strings.
   - blp(buffer): a texture as RGBA pixels (BLP2: palettized, DXT1/3/5, raw).
   - buildModel(client, path, options): a 3.3.5a model (M2 264, its skins and .anim files, its textures) as the
     engine's model data (game.js, GameModel), the same shape build_assets.py writes.
   - Library(client): the spells and effect models the client has, and any spell as a template (its SpellVisual kits).
   ===================================================================================================================== */
const GameData = (() => {
  const td = new TextDecoder('latin1');

  /* ------------------------------------------------------------------------------------------------- tables ----- */
  function dbc(buffer) {
    const v = new DataView(buffer);
    const n = v.getUint32(4, true), f = v.getUint32(8, true), rs = v.getUint32(12, true), ss = v.getUint32(16, true);
    const strings = new Uint8Array(buffer, 20 + n * rs, ss);
    const rows = new Map();
    for (let i = 0; i < n; i++) {
      const o = 20 + i * rs, row = new Uint32Array(f);
      for (let k = 0; k < f; k++) row[k] = v.getUint32(o + k * 4, true);
      rows.set(row[0], row);
    }
    const str = (o) => { let e = o; while (e < strings.length && strings[e]) e++; return td.decode(strings.subarray(o, e)); };
    const float = (x) => { const b = new DataView(new ArrayBuffer(4)); b.setUint32(0, x, true); return b.getFloat32(0, true); };
    return { rows, str, float, fields: f };
  }

  /* ----------------------------------------------------------------------------------------------- textures ----- */
  function rgb565(c) { return [((c >> 11) & 31) * 255 / 31, ((c >> 5) & 63) * 255 / 63, (c & 31) * 255 / 31]; }
  function dxt(src, w, h, kind) {          // kind: 1, 3, 5
    const out = new Uint8ClampedArray(w * h * 4), v = new DataView(src.buffer, src.byteOffset, src.byteLength);
    const block = kind === 1 ? 8 : 16;
    let o = 0;
    for (let by = 0; by < Math.max(1, h / 4); by++) for (let bx = 0; bx < Math.max(1, w / 4); bx++, o += block) {
      if (o + block > src.length) break;
      const a = new Uint8Array(16).fill(255);
      let c0 = o;
      if (kind === 3) {
        for (let i = 0; i < 8; i++) { const b = src[o + i]; a[i * 2] = (b & 15) * 17; a[i * 2 + 1] = (b >> 4) * 17; }
        c0 = o + 8;
      } else if (kind === 5) {
        const a0 = src[o], a1 = src[o + 1], table = [a0, a1];
        if (a0 > a1) for (let i = 1; i < 7; i++) table.push(((7 - i) * a0 + i * a1) / 7);
        else { for (let i = 1; i < 5; i++) table.push(((5 - i) * a0 + i * a1) / 5); table.push(0, 255); }
        let bits = 0n; for (let i = 0; i < 6; i++) bits |= BigInt(src[o + 2 + i]) << BigInt(8 * i);
        for (let i = 0; i < 16; i++) a[i] = table[Number((bits >> BigInt(3 * i)) & 7n)];
        c0 = o + 8;
      }
      const col0 = v.getUint16(c0, true), col1 = v.getUint16(c0 + 2, true), idx = v.getUint32(c0 + 4, true);
      const p0 = rgb565(col0), p1 = rgb565(col1), pal = [p0, p1];
      if (kind !== 1 || col0 > col1) { pal.push(p0.map((x, k) => (2 * x + p1[k]) / 3), p0.map((x, k) => (x + 2 * p1[k]) / 3)); }
      else { pal.push(p0.map((x, k) => (x + p1[k]) / 2), [0, 0, 0]); }
      for (let i = 0; i < 16; i++) {
        const x = bx * 4 + (i & 3), y = by * 4 + (i >> 2); if (x >= w || y >= h) continue;
        const ci = (idx >> (2 * i)) & 3, p = pal[ci], d = (y * w + x) * 4;
        out[d] = p[0]; out[d + 1] = p[1]; out[d + 2] = p[2];
        out[d + 3] = kind === 1 ? (col0 <= col1 && ci === 3 ? 0 : 255) : a[i];
      }
    }
    return out;
  }
  function blp(buffer) {
    const v = new DataView(buffer), b = new Uint8Array(buffer);
    if (td.decode(b.subarray(0, 4)) !== 'BLP2') return null;
    const enc = b[8], adepth = b[9], aenc = b[10], w = v.getUint32(12, true), h = v.getUint32(16, true);
    const off = v.getUint32(20, true), size = v.getUint32(84, true);
    const src = b.subarray(off, off + size);
    let rgba;
    if (enc === 2) rgba = dxt(src, w, h, aenc === 7 ? 5 : aenc === 1 ? 3 : 1);
    else if (enc === 1) {
      rgba = new Uint8ClampedArray(w * h * 4);
      for (let i = 0; i < w * h; i++) {
        const p = 148 + src[i] * 4;
        rgba[i * 4] = b[p + 2]; rgba[i * 4 + 1] = b[p + 1]; rgba[i * 4 + 2] = b[p];
        let a = 255;
        if (adepth === 8) a = src[w * h + i];
        else if (adepth === 1) a = (src[w * h + (i >> 3)] >> (i & 7)) & 1 ? 255 : 0;
        else if (adepth === 4) a = ((src[w * h + (i >> 1)] >> ((i & 1) * 4)) & 15) * 17;
        rgba[i * 4 + 3] = a;
      }
    } else {
      rgba = new Uint8ClampedArray(w * h * 4);
      for (let i = 0; i < w * h; i++) { rgba[i * 4] = src[i * 4 + 2]; rgba[i * 4 + 1] = src[i * 4 + 1]; rgba[i * 4 + 2] = src[i * 4]; rgba[i * 4 + 3] = src[i * 4 + 3]; }
    }
    return { width: w, height: h, rgba };
  }

  /* -------------------------------------------------------------------------------------------------- models ----- */
  const FIXED16 = 32767;
  const quat = (x) => (x < 0 ? x + 32768 : x - 32767) / 32767;
  const SIZE = { f: 4, h: 2, H: 2, B: 1, I: 4 };
  function reader(buffer) {
    const v = new DataView(buffer);
    const get = (t, o) => t === 'f' ? v.getFloat32(o, true) : t === 'h' ? v.getInt16(o, true) : t === 'H' ? v.getUint16(o, true)
      : t === 'B' ? v.getUint8(o) : t === 'i' ? v.getInt32(o, true) : v.getUint32(o, true);
    return { v, get, len: buffer.byteLength, arr: (o) => [v.getUint32(o, true), v.getUint32(o + 4, true)] };
  }
  function normalizePath(p) { return p.replace(/\//g, '\\').replace(/\.(mdx|mdl)$/i, '.m2'); }

  // The model data game.js plays. options: sequences (animation ids), geosets (submesh ids to show, or null),
  // replaceable (texture type -> client path), textureSize (largest texture side).
  async function buildModel(client, path, { sequences = [0], geosets = null, replaceable = {}, textureSize = 256 } = {}) {
    path = normalizePath(path);
    const buf = await client.read(path);
    if (!buf) throw new Error(`${path}: not in the client`);
    const R = reader(buf), d = R.v;
    if (td.decode(new Uint8Array(buf, 0, 4)) !== 'MD20' || d.getUint32(4, true) !== 264) throw new Error(`${path}: not a 3.3.5a model`);
    const base = path.slice(0, -3);
    const seqs = [];
    { const [n, o] = R.arr(0x1C); for (let i = 0; i < n; i++) { const s = o + i * 64; seqs.push({ id: d.getUint16(s, true), var: d.getUint16(s + 2, true), dur: d.getUint32(s + 4, true), flags: d.getUint32(s + 12, true), index: i }); } }
    let seqIndex = [];
    for (const id of sequences) { const s = seqs.find((x) => x.id === id && x.var === 0); if (s) seqIndex.push(s.index); }
    if (!seqIndex.length) seqIndex = [0];
    // The .anim files of the sequences kept (their keys are not in the model).
    const anims = new Map();
    await Promise.all(seqIndex.map(async (si) => {
      const s = seqs[si]; if (!s || s.flags & 0x20) return;
      const name = `${base}${String(s.id).padStart(4, '0')}-${String(s.var).padStart(2, '0')}.anim`;
      const a = await client.read(name); if (a) anims.set(si, reader(a));
    }));
    const gl = R.arr(0x14), gloops = [];
    for (let i = 0; i < gl[0]; i++) gloops.push(d.getUint32(gl[1] + i * 4, true));

    function track(off, fmt, conv) {
      const interp = d.getUint16(off, true), gseq = d.getInt16(off + 2, true);
      const tn = d.getUint32(off + 4, true), to = d.getUint32(off + 8, true), vn = d.getUint32(off + 12, true), vo = d.getUint32(off + 16, true);
      const size = fmt.split('').reduce((s, c) => s + SIZE[c], 0), stride = interp >= 2 ? size * 3 : size;
      const out = { i: interp, g: gseq, k: [] }; let any = false;
      for (const si of gseq >= 0 ? [0] : seqIndex) {
        if (si >= tn) { out.k.push([[], []]); continue; }
        const src = gseq >= 0 || (seqs[si] && seqs[si].flags & 0x20) ? R : anims.get(si);
        const n1 = d.getUint32(to + si * 8, true), o1 = d.getUint32(to + si * 8 + 4, true);
        const n2 = d.getUint32(vo + si * 8, true), o2 = d.getUint32(vo + si * 8 + 4, true);
        if (!src || !n1 || n1 !== n2 || o1 + n1 * 4 > src.len || o2 + n2 * stride > src.len) { out.k.push([[], []]); continue; }
        const times = [], vals = [];
        for (let j = 0; j < n1; j++) times.push(src.v.getUint32(o1 + j * 4, true));
        for (let j = 0; j < n2; j++) {
          let p = o2 + j * stride; const parts = [];
          for (const c of fmt) { parts.push(src.get(c, p)); p += SIZE[c]; }
          vals.push(conv ? conv(parts) : parts.length > 1 ? parts : parts[0]);
        }
        out.k.push([times, vals]); any = true;
      }
      return any ? out : null;
    }
    function fblock(off, fmt, conv) {
      const tn = d.getUint32(off, true), to = d.getUint32(off + 4, true), vn = d.getUint32(off + 8, true), vo = d.getUint32(off + 12, true);
      const size = fmt.split('').reduce((s, c) => s + SIZE[c], 0), times = [], vals = [];
      for (let j = 0; j < tn; j++) times.push(d.getUint16(to + j * 2, true) / FIXED16);
      for (let j = 0; j < vn; j++) {
        let p = vo + j * size; const parts = [];
        for (const c of fmt) { parts.push(R.get(c, p)); p += SIZE[c]; }
        vals.push(conv ? conv(parts) : parts.length > 1 ? parts : parts[0]);
      }
      return [times, vals];
    }
    const fixed = (p) => p[0] / FIXED16;

    // Textures (replaceable ones from options), decoded to RGBA.
    const textures = [];
    { const [n, o] = R.arr(0x50);
      await Promise.all(Array.from({ length: n }, async (_, i) => {
        const t = o + i * 16, type = d.getUint32(t, true), flags = d.getUint32(t + 4, true), ln = d.getUint32(t + 8, true), lo = d.getUint32(t + 12, true);
        const tpath = ln > 1 ? td.decode(new Uint8Array(buf, lo, ln - 1)) : (replaceable[type] || '');
        let image = null;
        if (tpath) { const tb = await client.read(tpath); if (tb) image = shrink(blp(tb), textureSize); }
        textures[i] = { name: tpath.split('\\').pop(), flags, type, image };
      })); }

    // Vertices.
    const [vc, vo] = R.arr(0x3C);
    const pos = new Float32Array(vc * 3), nrm = new Float32Array(vc * 3), uv = new Float32Array(vc * 2), bones4 = new Uint8Array(vc * 4), weights4 = new Uint8Array(vc * 4);
    for (let i = 0; i < vc; i++) {
      const o = vo + i * 48;
      for (let k = 0; k < 3; k++) { pos[i * 3 + k] = d.getFloat32(o + k * 4, true); nrm[i * 3 + k] = d.getFloat32(o + 20 + k * 4, true); }
      for (let k = 0; k < 4; k++) { weights4[i * 4 + k] = d.getUint8(o + 12 + k); bones4[i * 4 + k] = d.getUint8(o + 16 + k); }
      uv[i * 2] = d.getFloat32(o + 32, true); uv[i * 2 + 1] = d.getFloat32(o + 36, true);
    }
    const skinBuf = await client.read(base + '00.skin');
    if (!skinBuf) throw new Error(`${base}00.skin: not in the client`);
    const S = reader(skinBuf), s = S.v;
    const [ln, lo] = S.arr(0x04), [inn, ino] = S.arr(0x0C), [sn, so] = S.arr(0x1C), [bn, bo] = S.arr(0x24);
    const subs = [];
    for (let i = 0; i < sn; i++) { const o = so + i * 48; subs.push({ id: s.getUint16(o, true), istart: s.getUint16(o + 8, true) + (s.getUint16(o + 2, true) << 16), icount: s.getUint16(o + 10, true) }); }
    const mats = []; { const [n, o] = R.arr(0x70); for (let i = 0; i < n; i++) mats.push([d.getUint16(o + i * 4, true), d.getUint16(o + i * 4 + 2, true)]); }
    const lookup16 = (off, signed) => { const [n, o] = R.arr(off), a = []; for (let i = 0; i < n; i++) a.push(signed ? d.getInt16(o + i * 2, true) : d.getUint16(o + i * 2, true)); return a; };
    const texLookup = lookup16(0x80), transLookup = lookup16(0x90), uvLookup = lookup16(0x98, true);
    const index = [], batches = [];
    for (let i = 0; i < bn; i++) {
      const o = bo + i * 24, prio = s.getInt8(o + 1), section = s.getUint16(o + 4, true), color = s.getUint16(o + 8, true), mat = s.getUint16(o + 10, true);
      const layer = s.getUint16(o + 12, true), combo = s.getUint16(o + 16, true), weight = s.getUint16(o + 20, true), transform = s.getUint16(o + 22, true);
      const sub = subs[section]; if (!sub || (geosets && !geosets.has(sub.id))) continue;
      const start = index.length;
      for (let t = sub.istart; t < sub.istart + sub.icount; t++) index.push(s.getUint16(lo + s.getUint16(ino + t * 2, true) * 2, true));
      const [rflags, blend] = mats[mat] || [0, 0];
      batches.push({ start, count: sub.icount, tex: texLookup[combo] ?? 0, blend, flags: rflags, prio, layer, geoset: sub.id,
        color: color === 0xFFFF ? -1 : color, alpha: transLookup[weight] ?? -1, uvanim: uvLookup[transform] ?? -1 });
    }

    // Bones, attachments, colors, transparencies, texture animations.
    const bones = []; { const [n, o] = R.arr(0x2C);
      for (let i = 0; i < n; i++) { const b = o + i * 88; bones.push({ parent: d.getInt16(b + 8, true), flags: d.getUint32(b + 4, true), key: d.getInt32(b, true),
        pivot: [d.getFloat32(b + 76, true), d.getFloat32(b + 80, true), d.getFloat32(b + 84, true)],
        t: track(b + 16, 'fff'), r: track(b + 36, 'hhhh', (p) => p.map(quat)), s: track(b + 56, 'fff') }); } }
    const attachments = []; { const [n, o] = R.arr(0xF0); for (let i = 0; i < n; i++) { const a = o + i * 40;
      attachments.push({ id: d.getUint32(a, true), bone: d.getUint16(a + 4, true), pos: [d.getFloat32(a + 8, true), d.getFloat32(a + 12, true), d.getFloat32(a + 16, true)] }); } }
    const colors = []; { const [n, o] = R.arr(0x48); for (let i = 0; i < n; i++) colors.push({ rgb: track(o + i * 40, 'fff'), a: track(o + i * 40 + 20, 'h', fixed) }); }
    const alphas = []; { const [n, o] = R.arr(0x58); for (let i = 0; i < n; i++) alphas.push(track(o + i * 20, 'h', fixed)); }
    const uvanims = []; { const [n, o] = R.arr(0x60); for (let i = 0; i < n; i++) uvanims.push({ t: track(o + i * 60, 'fff'), r: track(o + i * 60 + 20, 'ffff'), s: track(o + i * 60 + 40, 'fff') }); }

    // Particle emitters (476 bytes) and ribbons (176).
    const particles = []; { const [n, o] = R.arr(0x128);
      for (let i = 0; i < n; i++) {
        const b = o + i * 476, f = (x) => d.getFloat32(b + x, true);
        const at = { speed: 0x34, speedVar: 0x48, vRange: 0x5C, hRange: 0x70, gravity: 0x84, life: 0x98, rate: 0xB0, areaL: 0xC8, areaW: 0xDC, zSource: 0xF0 };
        const p = { flags: d.getUint32(b + 4, true), pos: [f(8), f(12), f(16)], bone: d.getUint16(b + 0x14, true), tex: d.getUint16(b + 0x16, true),
          blend: d.getUint8(b + 0x28), type: d.getUint8(b + 0x29), ptype: d.getUint8(b + 0x2C), headTail: d.getUint8(b + 0x2D),
          rows: Math.max(1, d.getUint16(b + 0x30, true)), cols: Math.max(1, d.getUint16(b + 0x32, true)),
          lifeVary: f(0xAC), rateVary: f(0xC4),
          color: fblock(b + 0x104, 'fff'), alpha: fblock(b + 0x114, 'h', fixed), scale: fblock(b + 0x124, 'ff'), scaleVary: [f(0x134), f(0x138)],
          headCell: fblock(b + 0x13C, 'H'), tailCell: fblock(b + 0x14C, 'H'), tailLength: f(0x15C), burst: f(0x170), drag: f(0x174),
          baseSpin: f(0x178), baseSpinVary: f(0x17C), spin: f(0x180), spinVary: f(0x184), enabled: track(b + 0x1C8, 'B') };
        for (const [k, x] of Object.entries(at)) p[k] = track(b + x, 'f');
        particles.push(p);
      } }
    const ribbons = []; { const [n, o] = R.arr(0x120);
      for (let i = 0; i < n; i++) {
        const b = o + i * 176, tc = d.getUint32(b + 20, true), toff = d.getUint32(b + 24, true), mc = d.getUint32(b + 28, true), moff = d.getUint32(b + 32, true);
        ribbons.push({ bone: d.getUint32(b + 4, true), pos: [d.getFloat32(b + 8, true), d.getFloat32(b + 12, true), d.getFloat32(b + 16, true)],
          tex: tc ? d.getUint16(toff, true) : 0, mat: mc ? mats[d.getUint16(moff, true)] : [0, 4],
          rgb: track(b + 36, 'fff'), a: track(b + 56, 'h', fixed), above: track(b + 76, 'f'), below: track(b + 96, 'f'),
          edgesPerSec: d.getFloat32(b + 116, true), edgeLife: d.getFloat32(b + 120, true), gravity: d.getFloat32(b + 124, true), visible: track(b + 152, 'B') });
      } }
    const bounds = []; for (let k = 0; k < 6; k++) bounds.push(d.getFloat32(0xA0 + k * 4, true));
    return { name: path.split('\\').pop().replace(/\.m2$/i, ''), path, seqs: seqIndex.map((i) => ({ id: seqs[i].id, dur: seqs[i].dur })), gloops, bounds,
      vertexCount: vc, pos, nrm, uv, bones4, weights4, index: new Uint32Array(index), batches, textures, bones, attachments, colors, alphas, uvanims, particles, ribbons };
  }
  // A texture no larger than most (the engine draws it; the client's own sizes are fine for a preview).
  function shrink(img, most) {
    if (!img || typeof document === 'undefined') return img;
    const c = document.createElement('canvas'); c.width = img.width; c.height = img.height;
    c.getContext('2d').putImageData(new ImageData(img.rgba, img.width, img.height), 0, 0);
    if (Math.max(img.width, img.height) <= most) return c;
    const k = most / Math.max(img.width, img.height), o = document.createElement('canvas');
    o.width = Math.max(1, Math.round(img.width * k)); o.height = Math.max(1, Math.round(img.height * k));
    o.getContext('2d').drawImage(c, 0, 0, o.width, o.height);
    return o;
  }

  /* ------------------------------------------------------------------------------------------------- library ----- */
  const KIT_SLOTS = { 3: 'head', 4: 'chest', 5: 'base', 6: 'lefthand', 7: 'righthand', 8: 'breath', 14: 'world' };
  class Library {
    constructor(client) { this.client = client; this.tables = {}; this.models = new Map(); }
    async table(name) {
      if (!this.tables[name]) this.tables[name] = this.client.read('DBFilesClient\\' + name).then((b) => (b ? dbc(b) : null));
      return this.tables[name];
    }
    // Every spell with a visual: { id, name, rank, visual }.
    async spells() {
      if (this._spells) return this._spells;
      const s = await this.table('Spell.dbc'), out = [];
      for (const [id, r] of s.rows) if (r[131]) out.push({ id, name: s.str(r[136]), rank: s.str(r[153]), visual: r[131] });
      return (this._spells = out);
    }
    // Every effect model the spell visuals name: { id, name, path }.
    async effects() {
      if (this._effects) return this._effects;
      const e = await this.table('SpellVisualEffectName.dbc'), out = [], seen = new Set();
      for (const [id, r] of e.rows) {
        const path = normalizePath(e.str(r[2])); if (!path || seen.has(path.toLowerCase())) continue;
        seen.add(path.toLowerCase()); out.push({ id, name: e.str(r[1]) || path.split('\\').pop(), path, scale: e.float(r[4]) });
      }
      return (this._effects = out);
    }
    // A spell's visual as a template: its kits' effect models by slot, its missile, its speed, Chain Lightning's beam.
    async template(spellId) {
      const [spell, vis, kit, eff, chain] = await Promise.all(['Spell.dbc', 'SpellVisual.dbc', 'SpellVisualKit.dbc', 'SpellVisualEffectName.dbc', 'SpellChainEffects.dbc'].map((n) => this.table(n)));
      const r = spell.rows.get(spellId); if (!r) return null;
      const v = vis.rows.get(r[131]); if (!v) return null;
      const model = (id) => { const e = eff.rows.get(id); return e ? normalizePath(eff.str(e[2])) : null; };
      const kitOf = async (id) => {
        const k = kit.rows.get(id); if (!k) return null;
        const slots = {};
        for (const [col, slot] of Object.entries(KIT_SLOTS)) { const m = model(k[col]); if (m) slots[slot] = m; }
        let beam = null;
        if (k[17] === 0 && chain) {
          const c = chain.rows.get(Math.round(kit.float(k[21])));
          if (c) {
            const tb = await this.client.read(chain.str(c[7])), img = tb && shrink(blp(tb), 256);
            if (img && img.toDataURL) beam = { texture: img.toDataURL() };
          }
        }
        const anim = k[2] === 0xFFFFFFFF ? null : k[2], sound = k[15] || null;     // SoundEntries
        return Object.keys(slots).length || beam || anim !== null || sound ? { slots, beam, anim, sound } : null;
      };
      return { spell: spellId, name: spell.str(r[136]), speed: spell.float(r[47]),
        precast: await kitOf(v[1]), cast: await kitOf(v[2]), impact: (await kitOf(v[3])) || (await kitOf(v[15])),
        state: await kitOf(v[4]), missile: v[8] ? model(v[8]) : null, missileSound: v[11] || null };
    }
    // Every sound of SoundEntries with files: { id, name, files: [client paths], volume }.
    async sounds() {
      if (this._sounds) return this._sounds;
      const t = await this.table('SoundEntries.dbc'), out = [];
      if (t) for (const [id, r] of t.rows) {
        const dir = t.str(r[23]), files = [];
        for (let k = 3; k < 13; k++) { const f = r[k] && t.str(r[k]); if (f) files.push(dir ? `${dir}\\${f}` : f); }
        if (files.length) out.push({ id, name: t.str(r[2]) || files[0], files, volume: t.float(r[24]) || 1 });
      }
      out.sort((a, b) => a.name.localeCompare(b.name));
      return (this._sounds = out);
    }
    async sound(id) { return (await this.sounds()).find((x) => x.id === id) || null; }
    // The Human (HumanMale with NPC display extra 8177's look and hair) and the training dummy (WoodenDummy).
    async human() {
      let hair = 1;
      const g = await this.table('CharHairGeosets.dbc');
      if (g) for (const r of g.rows.values()) if (r[1] === 1 && r[2] === 0 && r[3] === 15) hair = r[4] || 1;
      return this.model('Character\\Human\\Male\\HumanMale.m2', { sequences: [0, 51, 53, 9, 26], textureSize: 512,
        geosets: [0, hair, 101, 201, 301, 401, 501, 702, 902, 1002, 1301],
        replaceable: { 1: 'Textures\\BakedNpcTextures\\CreatureDisplayExtra-08177.blp', 6: 'Character\\Human\\Hair02_00.blp' } });
    }
    dummy() {
      return this.model('Creature\\Object\\WoodenDummy.m2', { sequences: [0, 9, 10], textureSize: 512, replaceable: { 11: 'Creature\\Object\\WoodenDummy.blp' } });
    }
  }
  // Geosets arrive as an array in options (JSON-friendly); buildModel wants a Set.
  const build = buildModel;
  async function buildModelFromOptions(client, path, options = {}) {
    return build(client, path, { ...options, geosets: options.geosets ? new Set(options.geosets) : null });
  }
  Library.prototype.model = function (path, options) {
    const key = normalizePath(path).toLowerCase() + JSON.stringify(options || {});
    if (!this.models.has(key)) this.models.set(key, buildModelFromOptions(this.client, path, options).catch((e) => { console.warn(e.message); return null; }));
    return this.models.get(key);
  };

  return { dbc, blp, buildModel: buildModelFromOptions, Library, normalizePath };
})();
if (typeof module !== 'undefined') module.exports = GameData;
