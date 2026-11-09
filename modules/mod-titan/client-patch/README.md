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
The Demon Hunter's abilities cost and generate Rage (Fury, the client's Runic Power, before) and deal
Shadow damage (Chaos, every school, before). The Wildcard pool and the
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

Darkness lies on the ground (`ground_zone()`): flat layers just above it, each
a texture tinted, faded and slowly turning (on global sequences, so it never restarts), rounded off
at the edges when the texture tiles. Darkness is retail's dark purple ground (from
`cfx_demonhunter_darkness_state`) under two layers of its shadow smoke turning opposite ways, shown
by the persistent area for its 8 sec (the visual's persistent-area kit, Death and Decay's world
slot; none of Death and Decay's own ground decal, tint or sound). Immolation Aura casts a fel
shockwave (`ground_burst()`), then burns the Demon Hunter while it lasts with Blizzard's fel flames
(the Burning Crusade demons' Fel Flames around the body, fel fire on the hands). Retail's particles
(smoke, flames) are not in wow.export's exports: only their textures (`spells/DH_Darkness_*`,
`spells/DH_ImmolationAura_*`).

Demon Spikes' spikes (on the back) and Fiery Brand's sigil (over the target's head) are retail models a kit
attaches (`exported_model()`, from `spells/DH_DemonSpikes/` and `spells/DH_FieryBrand/`).

Fel Barrage's fel ball is a spell missile (`exported_model()` with a trail): retail's `fel_balllightning_creature`
(wow.export's OBJ, JSON and textures in `spells/DH_FelBarrage_Ball/`), scaled to 1.2 yards, turning,
each layer with its own texture, tint and scrolling, and Fel Rush's trail as a ribbon behind it. It
is the missile of a copy of Frozen Orb's visual (`set` in `VISUALS`), dropped from 10 yards above the
Demon Hunter onto spots on the ground around them, a fel fire burst where each lands.

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

**Blade Dance in demon form:** while a character moves, the client always gives its legs to the movement
animation, so a spell animation plays on the upper body only. Each demon form therefore has a Blade Dance variant
(`DHMetaMaleBladeDance`, `DHMetaFemaleBladeDance`, displays 90203 and 90204, the base form's textures) in which
every stance and movement animation is the dance (`BLADE_DANCE_EVERYWHERE`, with move speeds that keep its pace
at walking and running speed). The server shows it for the dance's 0.9 sec (`spell_titan_dh_blade_dance`), in
or out of demon form: it is the only way the dance plays on the whole body while moving (nothing in the model
or AnimationData did it: whole-body rows, play-once or looping sequences, Whirlwind's behavior or root bone).

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

Their rows in the client's AnimationData.dbc are patched (`ANIMATION_ROWS` in `patch_spells.py`) to copy the
Blizzard animation each stands in for (Whirlwind, Attack1H, Special1H, AttackThrown, JumpStart, Jump): the same
weapon and body flags make them upper-body actions that play over running and over the idle and ready stances,
and that animation is the fallback a model without the new one plays.

Done: Night Elf male and female. Other races play the fallback (Whirlwind for Blade Dance, and so on).

The retail export is pruned to the skeleton and these animations (`models/prune_gltf.py --skeleton`; the bone
pivots are read from the glTF's nodes, so its mesh `.bin` is not needed):

```bash
python3 models/prune_gltf.py <export>/nightelffemale_hd.gltf models/characters/nightelf_female/retail --skeleton \
    0-0 1008 1010 1014 1018 1022 1024 1028 1038 1040 1042 1044 1056 1058 1060 1062
```

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

Heroes use every class resource (mana, rage, energy and runes), but the player
frame only shows mana. `AddOns/Emberforge_Resources` adds bars for rage and energy
under the player frame and turns on the rune bar. A bar appears once you know an ability that
uses that resource, or currently have some of it. It also shows the ammo slot (the character
sheet hides it for a Paladin, which has a relic slot) and recolors the Hero class from Paladin pink
to ember orange wherever the UI colors names by class.

Emberforge has no Runic Power: `patch_spells.py` turns every spell costing or giving it into Rage in
`Spell.dbc` (costs, energize and drain effects, power auras, texts), and the addon renames it to Rage
in every UI string left (combat text, errors).

Rune costs are left to the server: `patch_spells.py` clears `RuneCostID` in the client's `Spell.dbc` (every spell
but our own, 393). The client only recharges a real Death Knight's runes for casting, so a Hero's rune spell stayed
refused ("Not enough runes") once its runes were spent, until another rune spell's cast told the client they were
back. The server's copy keeps the costs: it checks and spends the runes, and `hero_runes` shows them on the rune bar.
So the client no longer knows a rune spell's cost: `Emberforge_Resources` (`Runes.lua`) dims its action button
while its runes are not ready (blue, like a spell short of mana; a Death rune stands in for any) and puts its runes on
the tooltip. The costs come from `RuneCosts.lua`, which `patch_spells.py` writes from the client's `SpellRuneCost.dbc`
(in the DBC folder it is given; without it the file is left as it is).

**Combo points**: the client keeps them only for a Rogue or Druid. The server sends a Hero's (`hero_resources.cpp`,
prefix EFCP) and `Emberforge_Resources` (`Combo.lua`) answers `GetComboPoints` with them, so the combo frame by the
target shows them; a finisher (`ComboFinishers.lua`, written by `patch_spells.py` from the finishing-move attribute,
plus our own: Soul Cleave, Spirit Bomb, Rising Sun Kick, Fists of Fury, Spinning Crane Kick) looks unusable while
there are none. The pet frame moves down below the resource bars and runes (outside combat). The dungeon finder
window (it holds the PvP tab) stays shut below level 10, by its key too.

**World chat**: `Emberforge_Resources` (`WorldChat.lua`) joins everyone to the World channel a few seconds after
login and shows it in the main chat window; `/world <message>` talks in it. Both factions share it
(`AllowTwoSide.Interaction.Channel`, docker-compose.yml).

**Publishing to players:** `publish-client.ps1` (on the PC with rclone set up for the CDN) uploads the two patch MPQs
and the `Emberforge_*` addons where they changed, updates the launcher's manifest and raises its version. Purge
Cloudflare's cache after.

It also shows every piece of gear's item level on its tooltip ("Item Level 63" in gold under the name, as from
Cataclysm on), for every class, and the character window's stats Cataclysm style (`StatPanel.lua`): a column on
its right with the average item level of what you wear on top, then two sections whose titles are filters you click
to choose Attributes, Melee, Ranged, Spell or Defenses (Blizzard's own stat lines and tooltips; kept per character),
fixed in size so nothing scrolls, and an arrow at the bottom of the model to hide or show the column. The old stat
area under the model is hidden. `CharacterSkin.lua` makes the Character tab one window around the model and that
column: one border and background, a title bar across the top with the name and the close button at its right end,
a dark inset behind the model (the resistances in a row across its top, the rotation arrows in its bottom left
corner, the weapons centered at its bottom, the character between them), a gold ring around the portrait, and a
frame behind each gear slot edged in its item's quality color; the stats' title bars are rounded gold-edged boxes
like the Classless window's (`Box.lua`), and the tabs (Blizzard's) join the window's bottom edge (the other tabs
keep their own look).

Install: copy the `Emberforge_Resources` folder into the client's `Interface\AddOns\` folder.
The launcher will ship it automatically for players.

## Retextures (patch-enUS-5.MPQ)

`textures/` holds HD retextures of the game's own textures, each PNG at the client path it replaces
(`textures/Tileset/Elwynn/ElwynnGrassBase.png` is `Tileset\Elwynn\ElwynnGrassBase.blp`): for now Elwynn
Forest's 46 ground textures at 1024x1024, with their specular maps (`_s`, specular in the alpha) and cut-out
variants. `build_textures.py` turns them into DXT-compressed BLP2s with every mip level (color and alpha resized
apart: Pillow's RGBA resize blackened the color wherever the specular alpha was 0) (`encode_dxt` in
`models/blp.py`: DXT1 when opaque, DXT5 with the alpha) in an archive of their own, `dist/patch-enUS-5.MPQ`,
installed beside patch-enUS-4.MPQ. Run it only when `textures/` changes: the main patch's rebuilds leave it
alone.

## Spellbook class tabs

A Hero learns every class's abilities but none of their skill lines, so the client files them all under
General. `interface/FrameXML/SpellBookFrame.lua` and `.xml` (the 3.3.5a originals, patched; packed as
`Interface\FrameXML\`) make the tabs General and one per class the Hero has abilities of (up to 12 tabs: the
frame's 8 and 4 more, closer together), each with its class icon. The spells of the client's tabs are filed by
`EMBERFORGE_SPELL_CLASSES` (spell id -> class), which `interface/spell_classes.py` writes into
`EmberforgeSpellClasses.lua` at build time from SkillLineAbility.dbc (every spell of each class's spec skill
lines) and the Demon Hunter's spells. Being Blizzard's own interface code, the book still casts in combat.
The order of spells within a tab is the client's.

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

## HD liquids (patch-enUS-6.MPQ)

`hd/make_liquids.py` makes new animated textures for every liquid the client draws (LiquidType.dbc:
`XTextures\river\lake_a`, `fast_a`, `ocean\ocean_h`, `lava\lava`, `slime\slime`, `LavaGreen\lavagreen`,
`LavaOrange\LavaOrange`, frames 1-30) at 512x512, four times the originals' pixels. They are made, not upscaled:
time-periodic Fourier noise (each wave a whole number of times across the tile and around the 30 frames), so every
frame tiles and the loop has no seam; then each texture takes the original's colour and alpha distribution by rank
(`hd/liquid_stats.json`: 256 quantiles each, statistics only), so the engine blends it as before. Lakes and oceans
are made for calm, realistic water instead (the game lays their alpha over its water as light, so the originals'
large patches read as stones): fine wind ripples in a low, even haze and tiny sun glints gathering where the swell
faces the sun, the ocean's ripples running with the wind; fast water is foam scrolling a tile per loop; lava has
dark crust islands in grainy
glow; slime and orange slime are slow ridged swirls. `hd/build_hd.py` writes them to `dist/patch-enUS-6.MPQ`, an
archive of its own (29 MB), published with the others.

## Reflective water (patch-enUS-7.MPQ)

The client ships a full procedural water shader it never uses (`psLiquidProcWater` / `vsLiquidProcWater`,
`Liquid::CMaterialProcWater`: two scrolling normal maps, Fresnel, a specular sun highlight, reflections, the game's
fog); only LiquidType 100, "Basic Procedural Water", asks for it, and its textures were never shipped.
`hd/make_procwater.py` moves every water and ocean liquid type onto that material (LiquidType 100's fields 14-44)
and makes its textures: `XTextures\procWater\basicWaterHeightTex_<0-30>.blp` (looping wave normal maps, in
red/green and again in alpha/blue, as the shader reads them) and `basicReflectionMap.blp` (a sky over a horizon).
Whether the client feeds the shader all it needs shows only in game: try it by putting the archive in Data\enUS\,
undo it by deleting it. `publish-client.ps1` leaves it out until it is known to work.

How the client drives it (read from Wow.exe, `Liquid::CMaterialProcWater`): the liquid's texture slots bind 0 and 1
to the reflection cube maps, 4 to the engine's depth texture (the water's colour and opacity by depth, from the
zone's light), 5 to the specular mask (its alpha, times 4) and 2 and 3 to the wave normal maps; its floats from field
23 are the scroll speeds and angles and, from float 11, the pixel shader's c10 (normal fade, refraction ratio,
reflection share) and c11 (edge opacity, specular share). So slot 5 gets its own glittering specular mask
(`basicSpecMask_<0-30>.blp`, a string added to LiquidType.dbc) and the edge opacity is 0.12. The reflection map is a
real cube map, as the client keeps them (`TILESET\\Terrain Cube Maps`: a strip of six square faces, the four sides,
the top, the bottom; a flat image came out black, so the water reflected nothing): a sky darker and deeper than a
sky looks, as water mirrors it (a pale one made the water milky), with faint clouds, paler at the horizon and a soft
tone below it (tilted waves look there: black beside the sky read as camouflage). Slot 0, the refraction cube added to the water's own colour, is its own near-black map
(`basicRefractionMap.blp`; sharing the sky's washed the water out). The waves' normals have up 0.28 and the
reflection's share is 0.05 looking straight down, rising to 0.45 at a glance (c10.z, c10.w). `models/blp.py` encodes
cube strips (width six times a power-of-two height).

## Atmosphere and water light (patch-enUS-8.MPQ)

`hd/make_atmosphere.py` rewrites the client's lighting tables (every zone, every hour):
- **Atmosphere**: outdoor fog (LightFloatBand band 0, longer than 83 yards) ends at 72% of its distance and starts
  0.15 sooner (band 1), so the distance hazes into each zone's fog and sky colour. Indoors keep theirs.
- **Translucent water**: LightParams' shallow water opacity times 0.85, deep times 0.95 (lakes and oceans; 0.6 read
  as tinted glass).
- **Colour-matched water**: each zone's water colours (LightIntBand 14-17) take 30% (shallows) and 20% (deep) of
  that zone's sky at the same hour (the horizon, band 6, and the lower sky, band 5), so water mirrors its sky.

## Volumetric lighting (`reshade/`)

**Emberforge Volumetrics** (`reshade/Shaders/Emberforge_Volumetrics.fx`): light shafts and atmospheric fog, a
ReShade effect over the client's Direct3D 9 picture (the client draws its world itself, so this is a screen-space
layer):

1. **Sky**: the bright sky (pixels at the far plane, brighter than *Sky brightness threshold*), at an eighth of the
   screen.
2. **Sun**: where that light gathers (weighted to its hot spot), how much of it shows (by the brightest sky: none at
   night, indoors or looking away) and the sky's colour, eased over about a third of a second.
3. **Light shafts**: the sky's light smeared toward the sun at half resolution (56 samples), so it streams around
   trees, roofs and mountains in front of it.
4. **Volumetric fog**: from the depth buffer, exponential with distance, in the sky's colour and brighter toward the
   sun and where the shafts cross it (light scattered in the air); thicker low, below the eye line, and broken into
   slow drifting wisps (three octaves of 3D noise in view space, with time). Never on the sky; without depth access it
   turns itself off rather than fog the screen.

Two more effects, in this order around it (`ReShadePreset.ini`):
- **Emberforge Ambient Occlusion** (`Emberforge_AO.fx`), first: soft contact shadows in creases, corners and under
  objects, from the depth buffer at half resolution (12 samples on a turning spiral, a depth-aware blur), fading out
  with distance; never on the sky.
- **Emberforge Bloom & Grade** (`Emberforge_Bloom.fx`), last: glow around bright light over three sizes (screen
  blended, no clipping), then vibrance, a gentle S-curve, warmth and a light vignette.

Their settings (Home in game: ReShade's window): shafts (intensity, length, sky threshold), fog (density, start,
strength, sun glow, drifting mist, low-lying, shafts in the fog), occlusion (strength, radius, fade distance), bloom
(intensity, threshold) and grade (vibrance, contrast, warmth, vignette). The UI is drawn into the same picture, so the
effects touch it a little.

The client ships **ReShade 6.8.0** (`ReShade32.dll`, from the official installer at reshade.me,
`ReShade_Setup_6.8.0.exe` SHA-256 `207aea16205fbf952bc8fe1879966672454cf04002e7ad34237c7990a5b3c0b4`; BSD-3-Clause,
`ReShade-LICENSE.md`),
`ReShade.fxh` (CC0), and its settings as `ReShade.default.ini` / `ReShadePreset.default.ini`: WoW's depth is not
reversed, depth copied before clears, the tutorial skipped, Home opens the window, Scroll Lock turns the effects off
and on. The launcher's **Lighting: On/Off** puts ReShade in as `d3d9.dll` when the game starts (or takes it out; a
`d3d9.dll` that is not ReShade stays), and copies the default settings once.

To check the shader, ReShade's own compiler (`tools/fxc.cpp` in crosire/reshade) builds on Linux from its effect
sources; it compiles for Shader Model 3 (Direct3D 9) and GLSL without errors.
