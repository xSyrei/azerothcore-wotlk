# Emberforge Effect Suite

A spell effect engine for designing Emberforge's visual effects: a stage with a Hero stand-in and a training dummy, a
live preview, and a timeline. Open `index.html` in a browser (it loads three.js from jsDelivr), or in the game with
`.ees` (below).

## What an effect is

An effect spec (`"format": "emberforge-effect"`, exported as JSON) has:

- **Layers**, each fired at one of three moments: **on cast** (from the start), **on the projectile** (riding it for
  its whole flight), or **on impact** (where each projectile lands; without a projectile, at the effect's *Impact at*
  time). Each is anchored to the Hero (feet, chest, above the head, casting hand), the dummy (feet, chest, above the
  head), the impact point or the projectile.
  - **Billboard**: camera-facing cards (copies in a ring, orbit, spin, rise, stretch, flipbook, facing the motion).
  - **Ground**: flat decals on the floor (rings, sigils, scorch marks).
  - **Particles**: a burst or a stream (direction, cone, speed, gravity, drag, life, size, inherit the projectile's
    speed).
  - **Beam**: a textured band between two anchors (tiling, scroll, flicker).
  Every layer has a texture, blend (additive or normal), a color that fades to a second, opacity with fade in and out,
  a delay and how long it lasts.
- **A projectile**: launch time, from and to anchors (with offsets), speed and easing, homing, and its path: **Arc**
  (yards up), **Side** (yards right, or left when negative), **Peak** (where along the way the bend is widest), a
  **spiral** and a **wobble**; a **volley** (how many, time between, fan spacing, every other mirrored) and a **trail**.
  The orange dashes on the stage show the path as it is set.
- **Textures**: built-in white textures (glow, flare, spark, ring, shockwave, streak, slash, star, swirl, rune circle,
  smoke, fire and lightning flipbooks, a beam band) tinted by the layer's color, plus any PNG added in the texture
  picker (kept in the spec, with its flipbook columns and rows).

## Sound, light and placement

- **Sound**: a Sound layer plays on cast, on the projectile (a loop flies with it until it lands) or on impact, in
  3D from its place, with volume, pitch and a little variation each play. The projectile has its own Launch, In
  flight and Impact sounds. Sounds are built in (whoosh, fire crackle, arcane hum, charge, explosion, zap, thunder
  crack, chime, frost shatter, thud), your own files (kept inside the effect, under 4 MB each), or in the client any of
  the game's (SoundEntries: the Library searches them, and a spell's template brings its kits' sounds and its missile
  loop). They play while the effect plays, not while scrubbing or below half speed; the Sound button mutes them.
- **Lighting**: every layer has Brightness, Scene light (how much the stage's light shades it: 0 glows on its own)
  and Casts light (a point light with color, intensity, reach and flicker that lights the Hero, the dummy and the
  ground); the projectile has Trail brightness and its own light. Effect & stage sets the stage's mood (Studio,
  Daylight, Dusk, Night, Dungeon) or its ambient, sun, sun angle, exposure and sky, and scales every effect light.
- **Placement**: every layer has an Offset (X toward the dummy, Y up, Z right; or the projectile's flight), a Rotation
  (degrees: roll, turn, tilt) and a Scale; Effect & stage's Whole effect moves, turns and sizes all layers at once.
  The handles on the stage do the same by dragging (W move, E rotate, R scale, Q world or local axes, S snap to
  0.25 yd / 15° / 0.1×, G hide, F focus, Reset), on the selected layer, the projectile's launch point or the whole
  effect; the arrow keys and PgUp / PgDn nudge it (Shift: five times as far).

## The stage

The dummy stands *Distance* yards ahead of the Hero (offset to the side, strafing if set, to test homing). It rocks
back when hit and shows damage numbers. Cameras: Side, Behind Hero, Dummy, Top; drag to orbit, scroll to zoom. The
timeline plays, pauses, loops, scrubs and slows down (0.1× to 2×); every change replays the effect to where it was
(the run is deterministic, stepped at 120 Hz). Work autosaves in the browser; *Save* keeps an effect in *My effects*.

## The game's own models

`assets/` holds models from the 3.3.5a client, made by `build_assets.py` (JSON the Suite's engine, `game.js`, plays:
skinned bone animation with billboarded bones, the game's blend modes and render flags, color, transparency and
texture animation, particle emitters and ribbons). When `assets/manifest.json` lists them:

- **human**: the Hero is a Human male (HumanMale, with the baked look of an NPC, display extra 8177, and its hair),
  playing ReadySpellDirected while casting, SpellCastDirected as the spell leaves, and Stand. Its hand, chest and head
  attachments are the anchors.
- **dummy**: the training dummy (Creature\Object\WoodenDummy, display 28047), its chest and head the targets'.
- **fx_\***: spell effect models, used by **Game model** layers (anchored like any layer; one fired on the projectile
  rides it, facing where it flies, so the path sliders shape a real missile's flight).
- **templates.json**: the game's spells (Arcane Blast, Frostbolt, Fireball, Lightning Bolt, Chain Lightning) in the
  Library's *Game spells*: their SpellVisual kits as layers (precast in the hands, cast, missile, impact on the
  dummy, lingering over its head) and Chain Lightning's arc as a beam.

To make them: extract the files with `tools/extract/EmberforgeExtract.exe` and `patterns/effect-suite.txt` (each model
brings its skins, .anim files and textures), then

```sh
python3 build_assets.py <EmberforgeExtract folder> assets --dbc <a folder with Spell.dbc, SpellVisual*.dbc,
    SpellVisualEffectName.dbc, SpellChainEffects.dbc>
```

The Suite fetches `assets/`, so open it through a web server (`python3 -m http.server` in this folder), the launcher
or the published page; opened as a file it keeps its stand-ins.

## In the game: .ees

`.ees` (Administrator accounts only, `titan_commands.cpp`) opens the Suite on the staff member's own computer with the
whole game behind it:

1. The server sends opcode 0x51B (a client-to-server opcode the client never handles). The Emberforge Engine
   (`client-patch/engine/src/suite.cpp`, the launcher's d3d9.dll: Lighting on) listens for it, as for the spell
   modifiers' 0x51A, and runs the launcher named in `<install>\Emberforge\launcher.path` (the launcher writes it at
   each start) with `--ees`.
2. The launcher (`titan-launcher/ees.js`, one launcher at a time: a running one gets the request) opens its Suite
   window: this page (`npm run sync-ees` copies it to `renderer/ees/`, three.js vendored) with `window.ees`.
3. `window.ees.read` reads any file of the client through `EmberforgeArchive.exe` (`tools/extract/`, StormLib, the
   archives opened as the game opens them), and `gamedata.js` turns the client's own files into what the engine
   plays, as `build_assets.py` does: models (M2, skins, .anim files, BLP textures), the spell tables, the Human and
   the dummy. So the Library searches every spell (its SpellVisual kits as a template: precast, cast, missile,
   impact, Chain Lightning's arc) and every effect model of SpellVisualEffectName (added as a layer, built when
   used), and a layer's Model field takes any of them.
4. Save, Import and Export use `<install>\Emberforge\Effects\`: Save writes `<name>.json` there (My effects lists
   that folder), Export writes `Exports\<name>.json` and shows it, Import opens a file from there.

In the client a game model's key is its client path (`Spells\Fireball_Missile_Low.m2`); the web Suite's `fx_<name>`
keys still open (found under `Spells\`).

## Next

1. An exporter (`client-patch/`): billboard and ground layers to the `mist` builder's cards
   (`models/spell_effects.py`), particles to M2 particle emitters, a projectile's path to a SpellMissileMotion script
   (3.3.5's scripted missile paths), and the layers to SpellVisual kits (precast, cast, missile, impact).
2. An in-game lab: reserved effect slots the engine DLL reloads from the Data folder, and a `.vfx` command to play one
   on a target, so a change shows in the game without restarting it.
