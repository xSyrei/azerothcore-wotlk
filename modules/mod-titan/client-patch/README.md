# Client patch: classless "Hero"

The server only lets players create the Hero (see `data/sql/db-world/hero_class.sql` and
`CharacterCreating.Disabled.ClassMask` in `worldserver.conf`). This patch makes the game client
match: the class is called **Hero**, and it is the only class available for every race on the
character creation screen. All other class buttons show as unavailable.

## Build the patch

1. **Get the original DBC files.** Your local Docker server already has them extracted:
   ```bash
   docker cp ac-worldserver:/azerothcore/env/dist/data/dbc ./dbc
   ```
   (Or extract `DBFilesClient\ChrClasses.dbc` and `DBFilesClient\CharBaseInfo.dbc` from your
   3.3.5a client's MPQs with Ladik's MPQ Editor.)
2. **Patch them:**
   ```bash
   python3 patch_hero_class.py ./dbc ./out
   ```
3. **Pack them into an MPQ** with Ladik's MPQ Editor (Windows, free):
   - *MPQ → New* → name it `patch-E.MPQ` (default settings are fine).
   - Add `out/ChrClasses.dbc` and `out/CharBaseInfo.dbc` with the archive path
     `DBFilesClient\` (so they end up as `DBFilesClient\ChrClasses.dbc`, etc.).
   - Save and close.
4. **Install:** copy `patch-E.MPQ` into the client's `Data\` folder and delete the client's
   `Cache\` folder. For players, the launcher will ship this file automatically.

## Resource bars addon

Heroes use every class resource (mana, rage, energy, runic power and runes), but the player
frame only shows mana. `AddOns/Emberforge_Resources` adds bars for rage, energy and runic power
under the player frame and turns on the rune bar. A bar appears once you know an ability that
uses that resource, or currently have some of it.

Install: copy the `Emberforge_Resources` folder into the client's `Interface\AddOns\` folder.
The launcher will ship it automatically for players.

## Notes
- The base class is Paladin (ID 2). To change it, update `HERO_CLASS` in the script, `@HERO` in
  `hero_class.sql` and the class mask in `worldserver.conf` together.
- The class color is still Paladin pink in the UI; recoloring it is a separate interface patch.
