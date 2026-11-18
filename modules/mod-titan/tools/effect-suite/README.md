# Emberforge Effect Suite

A spell effect engine for designing Emberforge's visual effects: a stage with a Hero stand-in and a training dummy, a
live preview, and a timeline. Open `index.html` in a browser (it loads three.js from jsDelivr); it is one file, ready
to become a tab of the launcher (`titan-launcher`).

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

## The stage

The dummy stands *Distance* yards ahead of the Hero (offset to the side, strafing if set, to test homing). It rocks
back when hit and shows damage numbers. Cameras: Side, Behind Hero, Dummy, Top; drag to orbit, scroll to zoom. The
timeline plays, pauses, loops, scrubs and slows down (0.1× to 2×); every change replays the effect to where it was
(the run is deterministic, stepped at 120 Hz). Work autosaves in the browser; *Save* keeps an effect in *My effects*.

## Next

1. An exporter (`client-patch/`): billboard and ground layers to the `mist` builder's cards
   (`models/spell_effects.py`), particles to M2 particle emitters, a projectile's path to a SpellMissileMotion script
   (3.3.5's scripted missile paths), and the layers to SpellVisual kits (precast, cast, missile, impact).
2. An in-game lab: reserved effect slots the engine DLL reloads from the Data folder, and a `.vfx` command to play one
   on a target, so a change shows in the game without restarting it.
