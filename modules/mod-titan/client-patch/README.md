# Client patch: classless "Hero"

The server only lets players create the Hero (see `data/sql/db-world/hero_class.sql` and
`CharacterCreating.Disabled.ClassMask` in `worldserver.conf`). This patch makes the game client
match: the class is called **Hero**, and it is the only class available for every race on the
character creation screen. All other class buttons show as unavailable. It also adds Emberforge's
custom items (`Item.dbc`, `ItemDisplayInfo.dbc`) with their models and icons.

## The patch

A ready-made build is in `dist/patch-enUS-4.MPQ`. **Install:** copy it into the client's
`Data\enUS\` folder and delete the client's `Cache\` folder. It must be a language patch: the
client reads its tables (`DBFilesClient`) from the language archives, which take priority over
patches placed directly in `Data\`, so a `Data\patch-X.MPQ` would be ignored for class and item
data. The launcher ships it to players automatically
once it is part of the distributed client (see the launcher's `docs/CLIENT_DISTRIBUTION.md`).

### Rebuilding it

1. **Get the original DBC files.** Your local Docker server already has them extracted:
   ```bash
   docker cp ac-worldserver:/azerothcore/env/dist/data/dbc ./dbc
   ```
   They are also in the `data.zip` of the matching
   [wowgaming/client-data](https://github.com/wowgaming/client-data/releases) release. Use the
   DBCs of the same language as the client (enUS).
2. **Build** (Python 3 with Pillow and numpy):
   ```bash
   python3 build_patch.py ./dbc
   ```
   This patches the DBCs (ChrClasses, CharBaseInfo and CharStartOutfit with `patch_hero_class.py`;
   Item and ItemDisplayInfo with `patch_items.py`; Spell, SpellIcon and the spell visual tables with
   `patch_spells.py`), builds the spell effect models, converts the custom models, turns `icons/*.png` and `icons/spells/*.png` into BLP icons and packs
   everything into `dist/patch-enUS-4.MPQ` with `make_mpq.py` (zlib-compressed sectors, as Blizzard's
   patches; Spell.dbc alone is 48 MB uncompressed, and a `(listfile)` so it opens in Ladik's MPQ
   Editor).

## Custom items and models

- **Items:** add the item's `item_template` SQL under `data/sql/db-world/`, and its row to `ITEMS`
  in `patch_items.py` (otherwise the client shows a red "?").
- **Models:** export the model from modern WoW with [wow.export](https://www.kruithne.net/wow.export/)
  as OBJ (it writes the `.obj`, `.mtl`, `.json` and PNG textures), put the folder in `models/`,
  add it to `MODELS` in `build_patch.py` and a display row to `DISPLAYS` in `patch_items.py`.
  `models/wowexport_to_m2.py` rebuilds it as a 3.3.5a M2: mesh, materials (opaque, alpha,
  additive, unlit, two-sided) on one static bone. Not carried over: layers with animated color
  (flashes and pulses showed as solid planes in game), bone animation, particles and ribbons
  (wow.export does not export them) and extra texture layers such as reflections.
- **Preview:** `python3 models/render_m2.py <model.m2> <texture dir> preview.png` renders a model
  from the converted files. That is how the icons in `icons/` were made.
- **Icons:** 64x64 PNGs in `icons/`, named like the `DISPLAYS` icon (`INV_Sword_1H_Veridus.png`).

## Custom spells (Demon Hunter)

`patch_spells.py` defines Emberforge's own spells, each a copy of a Blizzard spell (attributes,
visuals, targeting) with some columns changed: the Demon Hunter's, ids 90100 and up. It writes them
to the client's Spell.dbc and SpellIcon.dbc (new icons from `icons/spells/`, by their Blizzard
names; icons the client already has are found in SpellIcon.dbc) and, with `--sql`, the same rows
for the server's `spell_dbc` table:

```bash
python3 patch_spells.py ./dbc /tmp/out --sql ../data/sql/db-world/titan_demon_hunter_spells.sql
```

After changing a spell, regenerate that SQL **and** rebuild the patch: client and server must agree.
Fury is the client's Runic Power, renamed (every Hero has it). The Wildcard pool and the
Classless window get the spells from `CUSTOM_ABILITIES` in `src/wildcard.cpp` (spec tabs Havoc and Vengeance).

### Trails and visuals

Demon's Bite, Chaos Strike and Blade Dance leave a trail behind the blade, Fel Rush one behind the
body, and Eye Beam shoots a beam from the face (`EFFECTS` in `patch_spells.py`). Each is its own
effect model, `Spells\Emberforge\<model>.m2`, built by `models/spell_effects.py`, laid out like
Blizzard's own `Spells\RibbonTrail.m2`. A trail (from `spells/<model>.png`) has no mesh, only a
ribbon emitter that follows the weapon or the chest. The beam is additive layers of crossed planes
20 yards long, after Legion's Eye Beam: a faint green glow, two fel flows (the fel swirl smeared
into streaks) scrolling outward fast at different speeds, a bright yellow-white core, and the orb
texture as a flare at the eyes, on the head attachment (where the mage's Dragon's Breath sits), so
it turns with the Demon Hunter. It bursts out of the eyes to full length in 0.15 sec flaring wide,
flickers and jitters in width while it lasts, tapers to a fading point at its far end, and narrows
and fades out over the channel's last 0.3 sec. (`DH_EyeBeam_Streak.png` is not used.)

Each spell's look is one of `VISUALS`: a copy of a Blizzard visual whose kits (precast, cast,
impact) carry our trails and Blizzard's own fel effect models (fel flames on the hands, fel fire
hits on targets, a demon's bite), as new rows in SpellVisual, SpellVisualKit and
SpellVisualEffectName.

Their sounds are `SOUNDS`: `sounds/*.wav`, converted from retail's `.ogg` (the 3.3.5a client plays
`.wav` and `.mp3`) with `ffmpeg -i x.ogg -af volume=<gain>dB -map_metadata -1 -fflags +bitexact
-acodec pcm_s16le x.wav`, the gain bringing the peak to -1 dB (retail's files peak above full scale
and would clip, crackling). They are binary in `.gitattributes`: git's line-ending conversion
would cut bytes out of them, which plays as static. Eye Beam's channel sounds (`*_1shot_*`, 4 to 5
sec in retail, which cuts them off when the beam stops) are cut to its 2 sec channel with
`-t 2.0 -af volume=<gain>dB,afade=t=out:st=1.7:d=0.3`. Packed under
`Sound\\Spells\\Emberforge`, as new SoundEntries rows. An entry holds up to 10 files and the game
plays one at random each time; a kit's sound plays when the kit does (cast, channel start, end).

Trail textures run from the weapon (left edge) to the end of the trail (right edge), blade tip at
the top; the build turns them the way the client lays them. `weapon_trail()` sets how much of the
blade the trail covers (`length`, yards from the grip), `body_trail()` how far it reaches above and
below the chest (`height`); both how long the model lives (`duration`) and how fast a stretch of
trail fades (`lifetime`).

The other `spells/DH_FelRush_*.png` (layers of retail's Fel
Rush dash) are not used yet.

## Animated creature models (Demon Hunter demon forms)

The Demon Hunter's Metamorphosis forms (retail `creature/dhmaledps` and `dhfemaledps`) are in
`models/demonhunter_meta/male` and `female`: wow.export **glTF** exports (`Metamorphosis.gltf`, its
`.bin` buffers, one per animation), the export's `Metamorphosis.json` and the textures in `textures/`.
Only the animations the converter uses are kept (see `ANIMATIONS` in the converter).

`models/creature_gltf_to_m2.py` rebuilds each as an animated 3.3.5a M2: the skinned mesh (split into
submeshes of at most 53 bones), all bones with their keyframes (wow.export writes the M2 bone tracks
unchanged, only with Y up, so nothing is resampled), the sequences, key bones and attachments (weapons
in the hands, spell hands, breath, name plate, on the retail helper bones). Retail animation ids
above 505 do not exist in 3.3.5a and get free slots instead:

| 3.3.5a animation | From retail |
|---|---|
| AttackThrown (107) | GlvThrowMain (Throw Glaive) |
| CustomSpell01 (213) | FelRushLoop |
| CustomSpell02 to 04 (214 to 216), 3 variations each | DHBladeDance1 to 3 |
| CustomSpell05, 06 (217, 218) | BackFlipStart, BackFlipLoop (Vengeful Retreat) |

Missing ones are filled in from similar ones (the male export has no Attack1H, the female no
AttackOff; Dead is Death's last frame, held). The tattoo glow is drawn additive on the second UV set.
Not carried over: particles (eye glow, smoke), ribbons, events, cameras.

`patch_creatures.py` adds their CreatureModelData and CreatureDisplayInfo rows (copies of the warlock's
Metamorphosis demon: its sounds, blood, footprints) and, with `--sql`, the same rows for the server:

```bash
python3 patch_creatures.py ./dbc /tmp/out --sql ../data/sql/db-world/titan_demon_hunter_displays.sql
```

| Display | Model |
|---|---|
| 90201 | `Creature\DemonHunterMeta\DHMetaMale.m2` |
| 90202 | `Creature\DemonHunterMeta\DHMetaFemale.m2` |

The Demon Form aura (spell 90115, hidden) puts the character in the one of its gender: Eye Beam wears it
for its channel, Chaos Nova for a second (`src/demon_hunter.cpp`). In game, `.morph 90201` or
`.morph 90202` shows one on your character (`.demorph` to undo). Their size
is `CreatureModelScale` in `CREATURES`.

**Preview:** `python3 models/render_creature.py <model.m2> <texture dir> out.png <anim id>[/variation]
[frames] [angle] [size]` reads a converted model back and renders frames of one of its animations.

## Demon Hunter animations on the character models

Retail Demon Hunters have their own animations (Blade Dance's three dashes, Fel Rush, the backflip, the glaive
throw and strikes). `models/retarget_character.py` adds them to Blizzard's 3.3.5a character models: it takes
them from a retail character exported by wow.export as glTF (`models/characters/<race>_<gender>/retail`,
pruned to the animations used) and patches the 3.3.5a model from the client (`<Race><Gender>.m2`, original),
written to the patch at its own path (`CHARACTERS` in `build_patch.py`).

M2 bones have no rest rotation and the retail skeletons keep the old joints in nearly the same places, so each
old bone is matched to the retail bone at its place and takes its orientation; the hips also take its
movement. Swaying bones (hair, cloth) keep swaying. The model is patched in place: the new sequences and their
data go after the old, nothing old moves (the `.anim` files still match).

The animations go in slots no Blizzard model uses, the same in the demon forms, and the spell visuals play them
(`ANIM_*` in `patch_spells.py`):

| Slot | Animation | Played by |
|---|---|---|
| CustomSpell03 (215) | DHBladeDance 1, 2 and 3 in a row (3 variations) | Blade Dance |
| CustomSpell04 (216) | FelRushLoop | Fel Rush |
| CustomSpell05, 06 (217, 218) | BackFlipStart, BackFlipLoop | Vengeful Retreat (start) |
| CustomSpell07 (219) | GlvThrowMain, GlvThrownOff | Throw Glaive |
| CustomSpell08 (220) | CombatAbilityGlv01, GlvOff01 | Demon's Bite |
| CustomSpell09 (221) | CombatAbilityGlvBig01 to 03 | Chaos Strike |
| CustomSpell10 (222), Custom1 (154) | FelRushEnd, DHMeteorStrike | (not used yet) |

Done: Night Elf male. A race not converted yet stands still for those abilities.

## Login screen (GlueXML)

`interface/GlueXML/` replaces Blizzard's login-screen files of the same name (the originals can be
extracted from a client with the launcher's `tools/extract-mpq.js`):

- `GlueStrings.lua`: the class panel describes the Hero and Wildcard instead of the Paladin.
- `CharacterCreate.xml` / `.lua`: a retail-style character creation in two steps. **Race:** Alliance
  races down the left, Horde down the right, gender at the top, the Hero card at the bottom and the
  race / Hero details in side panels. **Customize:** appearance options on the right, name and
  "Create Hero" at the bottom.
- Art in `interface/art/` (portrait rings and glows, Hero emblem, divider, vignette) is generated by
  `interface/make_art.py` and packed as `Interface\Glues\Emberforge\*.blp`.

### Login scene: Seat of the Pantheon

`AccountLogin.lua` is Blizzard's file with one change: the background model is
`Interface\Glues\Models\UI_Emberforge\UI_Emberforge.m2`, the Seat of the Pantheon from Antorus.
`models/build_login_scene.py` builds it on every patch build from the wow.export OBJ exports in
`models/pantheon/` (the room WMO plus the throne and waterfall doodads, placed from the
`_ModelPlacementInformation.csv`), a star dome (`interface/login/StarField.png`) and camera 0,
which the login screen looks through. `models/wowexport_scene_to_m2.py` does the conversion:
static and unlit, textures capped at 512px, waterfalls keep their scrolling texture transforms.
The 3.3.5a client draws at most 65535 triangle indices per model and nothing beyond its 1277-yard
draw distance, so the build keeps only what camera 0 can see, drops the smallest slivers until the
scene fits, and centers the star dome on the camera. Moving the camera re-runs all of that.

- Move the camera: edit `CAMERA` (position, target, field of view) and preview it with
  `python3 models/build_login_scene.py out` then
  `python3 models/render_scene.py out/UI_Emberforge.m2 out view.png`. A trial camera can be passed
  instead: `build_login_scene.py out 280,0,5 -89,0,60 1.4`.
- Sky: the star dome (`interface/login/StarField.png` from `make_art.py`: nebula with dust lanes and
  stars) with layers of Argus's Legion skybox textures (`models/pantheon/sky/`, some mirrored, edges
  feathered so they blend into the dome) and a violet glow in front of it, placed by screen
  position in `SKY`, plus a planet (`PLANET`). Keep scene textures at 1024 or less: with a 2048x1024
  dome texture the client dropped the whole scene (black login screen).
- Glows and colors: `GLOW_MODELS` (gem glows at Blizzard's glow doodad spots) and the `*_TINT` /
  `SHEEN` colors in `build_login_scene.py`.
- Lighting (`LIT`, `LIGHTS`): one directional M2 light, set up like the light in Blizzard's own login
  model (`UI_MainMenu_Northrend.m2`); the login screen drew lit materials black under point lights.
- Particles (`PARTICLES`): emitters written field for field like a known-good one from that model
  (its floating glow motes), so only placement, look and rates differ.
- Not included yet: light rays (wow.export does not export particles; the rays need their models).

Check changes offline with `sh interface/tests/run.sh` (anchor order plus a full creation run
against a mocked login API) before testing in game.

The client only loads modified GlueXML with a `Wow.exe` that skips Blizzard's interface signature
check; the Grimfall build in use does (see the launcher's `docs/CLIENT_DISTRIBUTION.md`).

## Resource bars addon

Heroes use every class resource (mana, rage, energy, Fury and runes), but the player
frame only shows mana. `AddOns/Emberforge_Resources` adds bars for rage, energy and Fury
under the player frame and turns on the rune bar. A bar appears once you know an ability that
uses that resource, or currently have some of it. It also shows the ammo slot (the character
sheet hides it for a Paladin, which has a relic slot) and recolors the Hero class from Paladin pink
to ember orange wherever the UI colors names by class.

Emberforge has no Runic Power: it is Fury. `patch_spells.py` renames it in every spell's name and
text in `Spell.dbc`, and the addon renames it in every UI string (spell costs, combat text, errors)
and colors its bars Fury purple.

Install: copy the `Emberforge_Resources` folder into the client's `Interface\AddOns\` folder.
The launcher will ship it automatically for players.

## Classless window addon

`AddOns/Emberforge_Classless` is the Classless window (N, the minimap button or `/classless`): every
class's abilities and talent trees by spec, your build, search, the Tome of Selection's picks and
the Wildcard roll (a reel that lands on each level-up ability or talent). N is bound once per
character if it is free; rebind it under Key Bindings > Emberforge. It is styled like the character
creation screen; its art (`Emberforge_Classless/Art/*.tga`, loaded straight from the addon folder)
comes from `interface/make_classless_art.py`; the Wildcard die is the project's ember cube
(`icons/wildcard-die.png`), which the script also splits into shards and cracks. Demon Hunter and
Monk (not in the client) have their own portraits (`icons/class-demonhunter.jpg`,
`icons/class-monk.png`), also made round by the script. Its data comes from the server
(`src/classless.cpp`). Check changes offline with `cd AddOns/tests && lua5.1 classless_test.lua`,
which plays a session against a mocked WoW API. Install: copy the folder into `Interface\AddOns\`
(the launcher ships `Emberforge_*` addons).

## Notes
- The base class is Paladin (ID 2). To change it, update `HERO_CLASS` in the script, `@HERO` in
  `hero_class.sql` and the class mask in `worldserver.conf` together.
- The class icon on the character creation screen is still the Paladin one; changing it needs
  edited login-screen (GlueXML) files.
