'use strict';
/* =====================================================================================================================
   The Effect Suite's game engine: plays 3.3.5a models (build_assets.py's JSON) in three.js, as the client draws them:
   bone animation (with billboarded bones), skinned meshes, the game's blend modes and render flags, color,
   transparency and texture animation tracks, particle emitters and ribbon emitters.

   A model keeps its own coordinates (X forward, Y left, Z up, yards) under a root turned to the Suite's world
   (Y up). Particles that do not follow their emitter are left in the world where they were born, so a missile's
   trail stays behind it.
   ===================================================================================================================== */
const GameEngine = (() => {
  const b64 = (s, Type) => {
    if (typeof s !== 'string') return s instanceof Type ? s : new Type(s.buffer || s);   // built in the page
    const bin = atob(s), bytes = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
    return new Type(bytes.buffer);
  };
  const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
  const rand = Math.random;
  const ZUP = new THREE.Vector3(0, 0, 1);

  /* ------------------------------------------------------------------------------------------------- tracks ------ */
  // A track's value at time ms of sequence slot s (or its global sequence's loop); fallback when it has no keys.
  function trackValue(track, s, ms, globalMs, gloops, fallback) {
    if (!track) return fallback;
    let keys, t;
    if (track.g >= 0) { keys = track.k[0]; const loop = gloops[track.g] || 1; t = loop ? globalMs % loop : 0; }
    else { keys = track.k[s]; t = ms; }
    if (!keys || !keys[0].length) return fallback;
    const [times, vals] = keys;
    if (times.length === 1 || t <= times[0]) return vals[0];
    if (t >= times[times.length - 1]) return vals[vals.length - 1];
    let i = 1; while (times[i] < t) i++;
    if (track.i === 0) return vals[i - 1];
    const f = (t - times[i - 1]) / Math.max(1, times[i] - times[i - 1]);
    return lerpValue(vals[i - 1], vals[i], f);
  }
  function lerpValue(a, b, f) {
    if (typeof a === 'number') return a + (b - a) * f;
    if (a.length === 4) {                     // a quaternion: shortest way
      let d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3], s = d < 0 ? -1 : 1;
      const q = [0, 1, 2, 3].map((k) => a[k] + (b[k] * s - a[k]) * f);
      const n = Math.hypot(...q) || 1; return q.map((v) => v / n);
    }
    return a.map((v, k) => v + (b[k] - v) * f);
  }
  // An FBlock (particles' color, alpha, scale): its values over a particle's life, times 0..1.
  function fblock(block, f, fallback) {
    const [times, vals] = block || [[], []];
    if (!vals.length) return fallback;
    if (vals.length === 1 || !times.length) return vals[0];
    if (f <= times[0]) return vals[0];
    for (let i = 1; i < times.length; i++) if (f <= times[i]) return lerpValue(vals[i - 1], vals[i], (f - times[i - 1]) / Math.max(1e-6, times[i] - times[i - 1]));
    return vals[vals.length - 1];
  }

  /* ---------------------------------------------------------------------------------------------- materials ----- */
  // The game's blend modes: 0 opaque, 1 alpha key, 2 alpha, 3 add (no alpha), 4 add, 5 mod, 6 mod2x, 7 blend add.
  function applyBlend(m, blend) {
    m.transparent = blend >= 2; m.depthWrite = blend <= 1; m.alphaTest = blend === 1 ? 0.5 : 0;
    m.blending = THREE.CustomBlending; m.blendEquation = THREE.AddEquation;
    const B = {
      0: [THREE.OneFactor, THREE.ZeroFactor], 1: [THREE.OneFactor, THREE.ZeroFactor],
      2: [THREE.SrcAlphaFactor, THREE.OneMinusSrcAlphaFactor], 3: [THREE.OneFactor, THREE.OneFactor],
      4: [THREE.SrcAlphaFactor, THREE.OneFactor], 5: [THREE.DstColorFactor, THREE.ZeroFactor],
      6: [THREE.DstColorFactor, THREE.SrcColorFactor], 7: [THREE.OneFactor, THREE.OneMinusSrcAlphaFactor],
    }[blend] || [THREE.SrcAlphaFactor, THREE.OneFactor];
    if (blend <= 1) m.blending = THREE.NoBlending;
    [m.blendSrc, m.blendDst] = B;
  }

  const textureCache = new Map();
  function loadTexture(entry) {
    if (entry && entry.image) {                 // decoded in the page (gamedata.js): a canvas
      if (!entry.tex) {
        const tex = entry.tex = new THREE.CanvasTexture(entry.image);
        tex.encoding = THREE.sRGBEncoding; tex.flipY = false;
        tex.wrapS = entry.flags & 1 ? THREE.RepeatWrapping : THREE.ClampToEdgeWrapping;
        tex.wrapT = entry.flags & 2 ? THREE.RepeatWrapping : THREE.ClampToEdgeWrapping;
      }
      return entry.tex;
    }
    if (!entry || !entry.png) return null;
    if (textureCache.has(entry.png)) return textureCache.get(entry.png);
    const tex = new THREE.TextureLoader().load(entry.png);
    tex.encoding = THREE.sRGBEncoding;
    tex.wrapS = entry.flags & 1 ? THREE.RepeatWrapping : THREE.ClampToEdgeWrapping;
    tex.wrapT = entry.flags & 2 ? THREE.RepeatWrapping : THREE.ClampToEdgeWrapping;
    tex.flipY = false;
    textureCache.set(entry.png, tex);
    return tex;
  }
  let whiteTex = null;
  function white() {
    if (!whiteTex) {
      const c = document.createElement('canvas'); c.width = c.height = 32; const x = c.getContext('2d');
      const g = x.createRadialGradient(16, 16, 0, 16, 16, 16); g.addColorStop(0, '#fff'); g.addColorStop(1, 'rgba(255,255,255,0)');
      x.fillStyle = g; x.fillRect(0, 0, 32, 32); whiteTex = new THREE.CanvasTexture(c);
    }
    return whiteTex;
  }

  /* ---------------------------------------------------------------------------------------------- the model ----- */
  class GameModel {
    constructor(data, { world, scale = 1, seq = 0 } = {}) {
      this.data = data; this.world = world;
      this.root = new THREE.Group(); this.root.rotation.x = -Math.PI / 2; this.root.scale.setScalar(scale);
      this.time = 0; this.globalTime = 0; this.seqSlot = 0; this.loop = true; this.speed = 1;
      this.brightness = 1;        // the Suite's Brightness: every batch, particle and ribbon color times this
      this.playSeq(seq);
      this.vertexCount = data.vertexCount;
      this.basePos = b64(data.pos, Float32Array); this.baseNrm = b64(data.nrm, Float32Array);
      this.boneIdx = b64(data.bones4, Uint8Array); this.weights = b64(data.weights4, Uint8Array);
      this.boneMats = data.bones.map(() => new THREE.Matrix4());
      this.geo = new THREE.BufferGeometry();
      this.posAttr = new THREE.BufferAttribute(new Float32Array(this.basePos), 3);
      this.nrmAttr = new THREE.BufferAttribute(new Float32Array(this.baseNrm), 3);
      this.geo.setAttribute('position', this.posAttr); this.geo.setAttribute('normal', this.nrmAttr);
      this.geo.setAttribute('uv', new THREE.BufferAttribute(b64(data.uv, Float32Array), 2));
      this.geo.setIndex(new THREE.BufferAttribute(b64(data.index, Uint32Array), 1));
      this.batches = [];
      const sorted = data.batches.map((b, i) => ({ b, i })).sort((x, y) => (x.b.blend > 1) - (y.b.blend > 1) || x.b.prio - y.b.prio || x.b.layer - y.b.layer);
      sorted.forEach(({ b }, order) => {
        this.geo.addGroup(b.start, b.count, order);
        const entry = data.textures[b.tex];
        let map = loadTexture(entry) || (b.blend > 1 ? white() : null);
        if (map && b.uvanim >= 0) { map = map.clone(); map.needsUpdate = true; map.wrapS = map.wrapT = THREE.RepeatWrapping; }
        const unlit = (b.flags & 1) || b.blend > 1;
        const m = unlit ? new THREE.MeshBasicMaterial({ map, color: 0xffffff }) : new THREE.MeshLambertMaterial({ map, color: 0xffffff });
        m.side = b.flags & 4 ? THREE.DoubleSide : THREE.FrontSide;
        applyBlend(m, b.blend); if (b.flags & 16) m.depthWrite = false;
        if (b.flags & 2) m.fog = false;
        m.userData.batch = b; this.batches.push(m);
      });
      this.mesh = new THREE.Mesh(this.geo, this.batches);
      this.mesh.frustumCulled = false; this.root.add(this.mesh);
      // Particles and ribbons: local ones draw under the root, world ones in the world group.
      this.emitters = data.particles.map((p) => new Emitter(this, p));
      this.ribbons = data.ribbons.map((r) => new Ribbon(this, r));
    }
    playSeq(id, loop = true) {
      const slot = this.data.seqs.findIndex((s) => s.id === id);
      this.seqSlot = slot >= 0 ? slot : 0; this.time = 0; this.loop = loop;
      return slot >= 0;
    }
    get duration() { return (this.data.seqs[this.seqSlot] || { dur: 1000 }).dur; }
    // The model's bones at this moment; camPos: the camera in the model's own space (for billboards).
    pose(camPos) {
      const d = this.data, s = this.seqSlot, ms = this.time, g = this.globalTime, gl = d.gloops;
      const T = new THREE.Matrix4(), R = new THREE.Matrix4(), S = new THREE.Matrix4(), P = new THREE.Matrix4(), N = new THREE.Matrix4();
      const q = new THREE.Quaternion(), v = new THREE.Vector3(), pivotW = new THREE.Vector3();
      d.bones.forEach((bone, i) => {
        const t = trackValue(bone.t, s, ms, g, gl, [0, 0, 0]), r = trackValue(bone.r, s, ms, g, gl, [0, 0, 0, 1]), sc = trackValue(bone.s, s, ms, g, gl, [1, 1, 1]);
        const pv = bone.pivot;
        P.makeTranslation(pv[0], pv[1], pv[2]); N.makeTranslation(-pv[0], -pv[1], -pv[2]);
        T.makeTranslation(t[0], t[1], t[2]); R.makeRotationFromQuaternion(q.set(r[0], r[1], r[2], r[3]));
        S.makeScale(sc[0], sc[1], sc[2]);
        const local = new THREE.Matrix4().multiplyMatrices(P, T).multiply(R).multiply(S).multiply(N);
        const m = this.boneMats[i];
        if (bone.parent >= 0) m.multiplyMatrices(this.boneMats[bone.parent], local); else m.copy(local);
        if (bone.flags & 0x8 && camPos) {            // a spherical billboard: its X toward the camera
          pivotW.set(pv[0], pv[1], pv[2]).applyMatrix4(m);
          const sx = new THREE.Vector3().setFromMatrixColumn(m, 0).length();
          const x = v.copy(camPos).sub(pivotW).normalize();
          const z = new THREE.Vector3(0, 0, 1).sub(x.clone().multiplyScalar(x.z)).normalize();
          if (z.lengthSq() < 1e-6) z.set(0, 1, 0);
          const y = new THREE.Vector3().crossVectors(z, x);
          const basis = new THREE.Matrix4().makeBasis(x.clone().multiplyScalar(sx), y.multiplyScalar(sx), z.multiplyScalar(sx));
          m.copy(new THREE.Matrix4().makeTranslation(pivotW.x, pivotW.y, pivotW.z).multiply(basis).multiply(N));
        }
      });
    }
    skin() {
      const pos = this.posAttr.array, nrm = this.nrmAttr.array, bp = this.basePos, bn = this.baseNrm, bi = this.boneIdx, bw = this.weights, M = this.boneMats;
      const nb = this.boneMats.length;
      for (let v = 0; v < this.vertexCount; v++) {
        let x = 0, y = 0, z = 0, nx = 0, ny = 0, nz = 0;
        const px = bp[v * 3], py = bp[v * 3 + 1], pz = bp[v * 3 + 2], qx = bn[v * 3], qy = bn[v * 3 + 1], qz = bn[v * 3 + 2];
        let total = 0;
        for (let k = 0; k < 4; k++) {
          const w = bw[v * 4 + k] / 255; if (!w) continue;
          const b = bi[v * 4 + k]; if (b >= nb) continue;
          const e = M[b].elements; total += w;
          x += (e[0] * px + e[4] * py + e[8] * pz + e[12]) * w; y += (e[1] * px + e[5] * py + e[9] * pz + e[13]) * w; z += (e[2] * px + e[6] * py + e[10] * pz + e[14]) * w;
          nx += (e[0] * qx + e[4] * qy + e[8] * qz) * w; ny += (e[1] * qx + e[5] * qy + e[9] * qz) * w; nz += (e[2] * qx + e[6] * qy + e[10] * qz) * w;
        }
        if (!total) { x = px; y = py; z = pz; nx = qx; ny = qy; nz = qz; }
        pos[v * 3] = x; pos[v * 3 + 1] = y; pos[v * 3 + 2] = z; nrm[v * 3] = nx; nrm[v * 3 + 1] = ny; nrm[v * 3 + 2] = nz;
      }
      this.posAttr.needsUpdate = true; this.nrmAttr.needsUpdate = true;
    }
    // The batches' color, transparency and texture animation.
    shade(opacity) {
      const d = this.data, s = this.seqSlot, ms = this.time, g = this.globalTime, gl = d.gloops;
      for (const m of this.batches) {
        const b = m.userData.batch;
        let a = opacity, rgb = [1, 1, 1];
        if (b.color >= 0 && d.colors[b.color]) { rgb = trackValue(d.colors[b.color].rgb, s, ms, g, gl, rgb); a *= trackValue(d.colors[b.color].a, s, ms, g, gl, 1); }
        if (b.alpha >= 0 && d.alphas[b.alpha]) a *= trackValue(d.alphas[b.alpha], s, ms, g, gl, 1);
        const k = this.brightness; m.color.setRGB(rgb[0] * k, rgb[1] * k, rgb[2] * k); m.opacity = clamp(a, 0, 1);
        if (b.blend <= 1) m.transparent = a < 0.999;
        m.visible = a > 0.003;
        if (b.uvanim >= 0 && m.map && d.uvanims[b.uvanim]) {
          const t = trackValue(d.uvanims[b.uvanim].t, s, ms, g, gl, [0, 0, 0]);
          m.map.offset.set(t[0], t[1]);
        }
      }
    }
    // An attachment point (by id) in the world, after update(); fallback when the model has none.
    attachment(id, out) {
      const a = this.data.attachments.find((x) => x.id === id);
      if (!a) return null;
      out.set(a.pos[0], a.pos[1], a.pos[2]).applyMatrix4(this.boneMats[a.bone] || new THREE.Matrix4());
      return out.applyMatrix4(this.root.matrixWorld);
    }
    // dt seconds; camera: the scene's; opacity: a fade the caller applies (birth, death).
    update(dt, camera, opacity = 1) {
      this.time += dt * 1000 * this.speed; this.globalTime += dt * 1000 * this.speed;
      const dur = this.duration;
      if (this.time > dur) this.time = this.loop ? this.time % Math.max(1, dur) : dur;
      this.posed(camera); this.draw(dt, camera, opacity);
    }
    // Its bones at a sequence (by animation id; another when it lacks that one) and moment, for anchors.
    poseAt(id, ms, camera, loop = true) {
      const slot = this.data.seqs.findIndex((s) => s.id === id);
      this.seqSlot = slot >= 0 ? slot : 0;
      const dur = Math.max(1, this.duration);
      this.time = loop ? ms % dur : Math.min(ms, dur); this.globalTime = ms;
      this.posed(camera);
    }
    posed(camera) {
      this.root.updateMatrixWorld(true);
      const inv = new THREE.Matrix4().copy(this.root.matrixWorld).invert();
      this.pose(camera.position.clone().applyMatrix4(inv));
    }
    draw(dt, camera, opacity = 1) {
      this.skin(); this.shade(opacity);
      for (const e of this.emitters) e.update(dt, camera, opacity);
      for (const r of this.ribbons) r.update(dt, camera, opacity);
    }
    hasSeq(id) { return this.data.seqs.some((s) => s.id === id); }
    get alive() { return this.emitters.some((e) => e.alive) || this.ribbons.some((r) => r.alive); }
    dispose() {
      this.root.parent && this.root.parent.remove(this.root);
      this.geo.dispose(); this.batches.forEach((m) => m.dispose());
      this.emitters.forEach((e) => e.dispose()); this.ribbons.forEach((r) => r.dispose());
    }
  }

  /* ----------------------------------------------------------------------------------------------- particles ----- */
  const MAX_PARTICLES = 400;
  class Emitter {
    constructor(model, p) {
      this.model = model; this.p = p; this.parts = []; this.acc = 0;
      this.local = !!(p.flags & 0x10);
      const geo = new THREE.BufferGeometry();
      this.pos = new Float32Array(MAX_PARTICLES * 12); this.uv = new Float32Array(MAX_PARTICLES * 8); this.col = new Float32Array(MAX_PARTICLES * 16);
      geo.setAttribute('position', new THREE.BufferAttribute(this.pos, 3));
      geo.setAttribute('uv', new THREE.BufferAttribute(this.uv, 2));
      geo.setAttribute('color', new THREE.BufferAttribute(this.col, 4));
      const idx = [];
      for (let i = 0; i < MAX_PARTICLES; i++) idx.push(i * 4, i * 4 + 1, i * 4 + 2, i * 4, i * 4 + 2, i * 4 + 3);
      geo.setIndex(idx);
      const map = loadTexture(model.data.textures[p.tex]) || white();
      const m = new THREE.ShaderMaterial({
        uniforms: { map: { value: map } },
        vertexShader: 'attribute vec4 color; varying vec4 vc; varying vec2 vu; void main(){ vc=color; vu=uv; gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0); }',
        fragmentShader: 'uniform sampler2D map; varying vec4 vc; varying vec2 vu; void main(){ vec4 t=texture2D(map,vu); gl_FragColor=t*vc; if(gl_FragColor.a<0.004) discard; }',
        depthWrite: false, transparent: true, side: THREE.DoubleSide,
      });
      applyBlend(m, p.blend); m.depthWrite = false; m.transparent = true;     // the materials' blend modes
      this.mesh = new THREE.Mesh(geo, m); this.mesh.frustumCulled = false;
      (this.local ? model.root : model.world).add(this.mesh);
      this.geo = geo;
    }
    emitterMatrix() {
      const m = new THREE.Matrix4().copy(this.model.boneMats[this.p.bone] || new THREE.Matrix4());
      return m.multiply(new THREE.Matrix4().makeTranslation(...this.p.pos));
    }
    value(name, fallback) {
      const md = this.model;
      return trackValue(this.p[name], md.seqSlot, md.time, md.globalTime, md.data.gloops, fallback);
    }
    spawn(local) {
      const p = this.p;
      const L = this.value('areaL', 0), W = this.value('areaW', 0), vR = this.value('vRange', 0), hR = this.value('hRange', 0);
      let pos = new THREE.Vector3(), dir;
      if (p.type === 2) {                   // sphere
        const pol = (rand() * 2 - 1) * vR, az = (rand() * 2 - 1) * (hR || Math.PI);
        dir = new THREE.Vector3(Math.sin(pol) * Math.cos(az), Math.sin(pol) * Math.sin(az), Math.cos(pol));
        const r = L + (Math.max(W, L) - L) * rand();
        pos.copy(dir).multiplyScalar(r || L * rand());
        if (!vR && !hR) { dir = new THREE.Vector3(rand() * 2 - 1, rand() * 2 - 1, rand() * 2 - 1).normalize(); pos.copy(dir).multiplyScalar(L * rand()); }
      } else {                               // plane (and spline, as a plane)
        pos.set((rand() - 0.5) * L, (rand() - 0.5) * W, 0);
        const z = this.value('zSource', 0);
        if (z > 0) dir = pos.clone().sub(new THREE.Vector3(0, 0, z)).normalize();
        else {
          const a = (rand() * 2 - 1) * vR, b = (rand() * 2 - 1) * hR;
          dir = new THREE.Vector3(0, 0, 1).applyAxisAngle(new THREE.Vector3(1, 0, 0), a).applyAxisAngle(new THREE.Vector3(0, 0, 1), b);
        }
      }
      const speed = this.value('speed', 0) * (1 + (rand() * 2 - 1) * this.value('speedVar', 0));
      const em = local;
      pos.applyMatrix4(em);
      const vel = dir.transformDirection(em).multiplyScalar(speed * new THREE.Vector3().setFromMatrixColumn(em, 0).length());
      const life = Math.max(0.05, this.value('life', 1) + (rand() * 2 - 1) * (p.lifeVary || 0));
      const vary = p.scaleVary || [0, 0];
      const k = 1 + (rand() * 2 - 1) * vary[0];
      const cells = p.rows * p.cols;
      return { pos, vel, age: 0, life, k, ky: p.flags & 0x80000 ? 1 + (rand() * 2 - 1) * vary[1] : k,
        cell: cells > 1 ? Math.floor(rand() * cells) : 0, rot: (p.baseSpin || 0) + (rand() * 2 - 1) * (p.baseSpinVary || 0),
        spin: (p.spin || 0) + (rand() * 2 - 1) * (p.spinVary || 0) };
    }
    update(dt, camera, opacity) {
      const p = this.p, md = this.model;
      const enabled = this.value('enabled', 1);
      const em = this.emitterMatrix();
      const toWorld = this.local ? null : md.root.matrixWorld;
      if (enabled && md.emitting !== false) {
        this.acc += dt * Math.max(0, this.value('rate', 0) + (rand() * 2 - 1) * (p.rateVary || 0));
        while (this.acc >= 1 && this.parts.length < MAX_PARTICLES) {
          this.acc -= 1;
          const part = this.spawn(em);
          if (toWorld) { const sp = part.vel.length(); part.pos.applyMatrix4(toWorld); part.vel.transformDirection(toWorld).multiplyScalar(sp * md.root.scale.x); }
          this.parts.push(part);
        }
        if (this.acc > 1) this.acc = 1;
      }
      const grav = this.value('gravity', 0);
      const down = this.local ? new THREE.Vector3(0, 0, -1) : new THREE.Vector3(0, -1, 0);
      const scaleK = this.local ? 1 : md.root.scale.x;
      for (let i = this.parts.length - 1; i >= 0; i--) {
        const q = this.parts[i]; q.age += dt;
        if (q.age >= q.life) { this.parts.splice(i, 1); continue; }
        q.vel.addScaledVector(down, grav * dt * scaleK);
        if (p.drag) q.vel.multiplyScalar(Math.max(0, 1 - p.drag * dt));
        q.pos.addScaledVector(q.vel, dt); q.rot += q.spin * dt;
      }
      this.draw(camera, opacity, scaleK);
    }
    draw(camera, opacity, scaleK) {
      const p = this.p, n = this.parts.length;
      // The camera's right and up in the particles' space.
      const space = this.local ? this.model.root.matrixWorld : new THREE.Matrix4();
      const inv = new THREE.Matrix4().copy(space).invert();
      const camR = new THREE.Vector3().setFromMatrixColumn(camera.matrixWorld, 0).transformDirection(inv);
      const camU = new THREE.Vector3().setFromMatrixColumn(camera.matrixWorld, 1).transformDirection(inv);
      const camF = new THREE.Vector3().setFromMatrixColumn(camera.matrixWorld, 2).transformDirection(inv);
      const cols = p.cols, rows = p.rows;
      const R = new THREE.Vector3(), U = new THREE.Vector3(), c = new THREE.Vector3();
      for (let i = 0; i < n; i++) {
        const q = this.parts[i], f = q.age / q.life;
        const s = fblock(p.scale, f, [1, 1]), rgb = fblock(p.color, f, [255, 255, 255]), a = fblock(p.alpha, f, 1);
        const sx = s[0] * q.k * scaleK, sy = s[1] * q.ky * scaleK;        // the game's scale is the half-size
        if (p.headTail === 1 && q.vel.lengthSq() > 1e-6) {       // a tail: stretched along its motion
          U.copy(q.vel).normalize().multiplyScalar(Math.max(sy, q.vel.length() * (p.tailLength || 0.1) * 0.5));
          R.crossVectors(U, camF).normalize().multiplyScalar(sx);
        } else {
          const cr = Math.cos(q.rot), sr = Math.sin(q.rot);
          R.copy(camR).multiplyScalar(cr).addScaledVector(camU, sr).multiplyScalar(sx);
          U.copy(camU).multiplyScalar(cr).addScaledVector(camR, -sr).multiplyScalar(sy);
        }
        c.copy(q.pos);
        const o = i * 12;
        this.pos[o] = c.x - R.x - U.x; this.pos[o + 1] = c.y - R.y - U.y; this.pos[o + 2] = c.z - R.z - U.z;
        this.pos[o + 3] = c.x + R.x - U.x; this.pos[o + 4] = c.y + R.y - U.y; this.pos[o + 5] = c.z + R.z - U.z;
        this.pos[o + 6] = c.x + R.x + U.x; this.pos[o + 7] = c.y + R.y + U.y; this.pos[o + 8] = c.z + R.z + U.z;
        this.pos[o + 9] = c.x - R.x + U.x; this.pos[o + 10] = c.y - R.y + U.y; this.pos[o + 11] = c.z - R.z + U.z;
        let cell = q.cell;
        if (p.headCell && p.headCell[1].length && !(p.flags & 0x10000)) cell = Math.floor(fblock(p.headCell, f, 0));
        const cx = cell % cols, cy = Math.floor(cell / cols), u0 = cx / cols, u1 = (cx + 1) / cols, v0 = cy / rows, v1 = (cy + 1) / rows;
        this.uv.set([u0, v1, u1, v1, u1, v0, u0, v0], i * 8);
        const al = clamp(a * opacity, 0, 1);
        const br = this.model.brightness / 255;
        for (let k = 0; k < 4; k++) this.col.set([rgb[0] * br, rgb[1] * br, rgb[2] * br, al], i * 16 + k * 4);
      }
      this.geo.setDrawRange(0, n * 6);
      this.geo.attributes.position.needsUpdate = true; this.geo.attributes.uv.needsUpdate = true; this.geo.attributes.color.needsUpdate = true;
    }
    get alive() { return this.parts.length; }
    dispose() { this.mesh.parent && this.mesh.parent.remove(this.mesh); this.geo.dispose(); this.mesh.material.dispose(); }
  }

  /* ----------------------------------------------------------------------------------------------- ribbons ------- */
  const MAX_EDGES = 120;
  class Ribbon {
    constructor(model, r) {
      this.model = model; this.r = r; this.edges = []; this.acc = 0;
      const geo = new THREE.BufferGeometry();
      this.pos = new Float32Array(MAX_EDGES * 6); this.uv = new Float32Array(MAX_EDGES * 4); this.col = new Float32Array(MAX_EDGES * 8);
      geo.setAttribute('position', new THREE.BufferAttribute(this.pos, 3)); geo.setAttribute('uv', new THREE.BufferAttribute(this.uv, 2));
      geo.setAttribute('color', new THREE.BufferAttribute(this.col, 4));
      const idx = []; for (let i = 0; i < MAX_EDGES - 1; i++) idx.push(i * 2, i * 2 + 1, i * 2 + 2, i * 2 + 1, i * 2 + 3, i * 2 + 2);
      geo.setIndex(idx);
      const map = loadTexture(model.data.textures[r.tex]) || white();
      const m = new THREE.ShaderMaterial({
        uniforms: { map: { value: map } },
        vertexShader: 'attribute vec4 color; varying vec4 vc; varying vec2 vu; void main(){ vc=color; vu=uv; gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0); }',
        fragmentShader: 'uniform sampler2D map; varying vec4 vc; varying vec2 vu; void main(){ gl_FragColor=texture2D(map,vu)*vc; }',
        transparent: true, depthWrite: false, side: THREE.DoubleSide,
      });
      applyBlend(m, r.mat ? r.mat[1] : 4); m.depthWrite = false; m.transparent = true;
      this.mesh = new THREE.Mesh(geo, m); this.mesh.frustumCulled = false; model.world.add(this.mesh); this.geo = geo;
    }
    value(name, fallback) { const md = this.model; return trackValue(this.r[name], md.seqSlot, md.time, md.globalTime, md.data.gloops, fallback); }
    update(dt, camera, opacity) {
      const r = this.r, md = this.model;
      const m = new THREE.Matrix4().multiplyMatrices(md.root.matrixWorld, md.boneMats[r.bone] || new THREE.Matrix4());
      const at = new THREE.Vector3(...r.pos).applyMatrix4(m);
      const up = new THREE.Vector3(0, 0, 1).transformDirection(m);
      const k = md.root.scale.x;
      if (this.value('visible', 1) && md.emitting !== false) this.edges.push({ p: at, up, above: this.value('above', 0.2) * k, below: this.value('below', 0.2) * k, age: 0 });
      for (const e of this.edges) { e.age += dt; e.p.y -= (r.gravity || 0) * dt * e.age * k; }
      const life = r.edgeLife || 0.5;
      while (this.edges.length && (this.edges[0].age > life || this.edges.length > MAX_EDGES)) this.edges.shift();
      const n = this.edges.length, rgb = this.value('rgb', [1, 1, 1]).map((x) => x * md.brightness), a = this.value('a', 1);
      for (let i = 0; i < n; i++) {
        const e = this.edges[i], f = i / Math.max(1, n - 1);
        this.pos.set([e.p.x + e.up.x * e.above, e.p.y + e.up.y * e.above, e.p.z + e.up.z * e.above,
          e.p.x - e.up.x * e.below, e.p.y - e.up.y * e.below, e.p.z - e.up.z * e.below], i * 6);
        this.uv.set([1 - f, 0, 1 - f, 1], i * 4);
        const al = clamp(a * opacity * (1 - e.age / life), 0, 1);
        this.col.set([rgb[0], rgb[1], rgb[2], al, rgb[0], rgb[1], rgb[2], al], i * 8);
      }
      this.geo.setDrawRange(0, Math.max(0, n - 1) * 6);
      this.geo.attributes.position.needsUpdate = true; this.geo.attributes.uv.needsUpdate = true; this.geo.attributes.color.needsUpdate = true;
    }
    get alive() { return this.edges.length; }
    dispose() { this.mesh.parent && this.mesh.parent.remove(this.mesh); this.geo.dispose(); this.mesh.material.dispose(); }
  }

  /* ------------------------------------------------------------------------------------------------- loading ----- */
  const assets = {};          // name -> JSON
  async function load(base, names) {
    await Promise.all(names.map(async (n) => {
      try {
        const r = await fetch(`${base}${n}.json`);
        if (r.ok) assets[n] = await r.json();
      } catch (e) { /* missing: the stand-in stays */ }
    }));
    return assets;
  }
  // The attachment ids the spell kits use: head, chest, base, the hands, the breath.
  const SLOT_ATTACH = { head: [11, 20, 18], chest: [15, 16, 0], base: [19], lefthand: [21, 2], righthand: [22, 1], breath: [17, 11], world: [19] };

  return { GameModel, assets, load, SLOT_ATTACH, trackValue };
})();
