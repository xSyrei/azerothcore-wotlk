# Client patch: classless "Hero"

The server only lets players create the Hero (see `data/sql/db-world/hero_class.sql` and
`CharacterCreating.Disabled.ClassMask` in `worldserver.conf`). This patch makes the game client
match: the class is called **Hero**, and it is the only class available for every race on the
character creation screen. All other class buttons show as unavailable. It also adds Emberforge's
custom items to `Item.dbc`, so they show their real icon instead of a red "?".

## The patch

A ready-made build is in `dist/patch-E.MPQ`. **Install:** copy it into the client's `Data\`
folder and delete the client's `Cache\` folder. The launcher ships it to players automatically
once it is part of the distributed client (see the launcher's `docs/CLIENT_DISTRIBUTION.md`).

### Rebuilding it

1. **Get the original DBC files.** Your local Docker server already has them extracted:
   ```bash
   docker cp ac-worldserver:/azerothcore/env/dist/data/dbc ./dbc
   ```
   They are also in the `data.zip` of the matching
   [wowgaming/client-data](https://github.com/wowgaming/client-data/releases) release.
2. **Patch them** (`ChrClasses.dbc`, `CharBaseInfo.dbc` and `Item.dbc`):
   ```bash
   python3 patch_hero_class.py ./dbc ./out
   python3 patch_items.py ./dbc ./out
   ```
   New custom items go in the `ITEMS` list of `patch_items.py`.
3. **Pack them into an MPQ** (no MPQ editor needed):
   ```bash
   python3 make_mpq.py dist/patch-E.MPQ "DBFilesClient\ChrClasses.dbc=out/ChrClasses.dbc" "DBFilesClient\CharBaseInfo.dbc=out/CharBaseInfo.dbc" "DBFilesClient\Item.dbc=out/Item.dbc"
   ```
   `make_mpq.py` writes a plain (uncompressed) MPQ with a `(listfile)`, so it also opens in
   Ladik's MPQ Editor.

## Resource bars addon

Heroes use every class resource (mana, rage, energy, runic power and runes), but the player
frame only shows mana. `AddOns/Emberforge_Resources` adds bars for rage, energy and runic power
under the player frame and turns on the rune bar. A bar appears once you know an ability that
uses that resource, or currently have some of it. It also recolors the Hero class from Paladin
pink to ember orange wherever the UI colors names by class.

Install: copy the `Emberforge_Resources` folder into the client's `Interface\AddOns\` folder.
The launcher will ship it automatically for players.

## Notes
- The base class is Paladin (ID 2). To change it, update `HERO_CLASS` in the script, `@HERO` in
  `hero_class.sql` and the class mask in `worldserver.conf` together.
- The class icon on the character creation screen is still the Paladin one; changing it needs
  edited login-screen (GlueXML) files.
