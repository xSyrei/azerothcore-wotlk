# mod-titan

Project Emberforge custom content for AzerothCore.

## Classless Hero
Every character is created as the **Hero**: the hidden base class (Paladin, ID 2) renamed on the
client. `data/sql/db-world/hero_class.sql` opens it to all 10 races with each race's usual start
zone, `hero_outfit.sql` gives every Hero the Paladin starting outfit (the one the client's creation
preview shows), and `CharacterCreating.Disabled.ClassMask = 1533` in
`worldserver.conf` blocks every other class (GM accounts can still create any class for testing).
The matching client patch is built with `client-patch/` (see its README).

Heroes use **every class resource**: mana, rage, energy, runic power and runes, with combo points
on any target. `src/hero_resources.cpp` turns these on through AzerothCore's classless hooks
(`OnPlayerHasActivePowerType`, and `OnPlayerIsClass` for ability mechanics only). Starting zone,
stats and equipment still follow the real base class. The `Emberforge_Resources` addon (in
`client-patch/AddOns/`) shows the extra bars in game and colors Heroes ember orange.

Nothing else comes from the base class (`data/sql/db-world/hero_cleanup.sql`):
- no starting class spells (the Paladin skill lines are removed) and no Paladin action buttons;
- Paladin trainers no longer train, and Paladin-only class quests are disabled;
- no talent points to spend: Wildcard grants talents (below).

## Wildcard
How a Hero progresses (`src/wildcard.cpp`), fully random for now:
- **Abilities**: one random ability per level from level 1, drawn from every class trainer's list
  (all 10 classes; passives, talent abilities and hunter pet handling are left out). Owned
  abilities upgrade to the best rank for the Hero's level by themselves, no trainer needed.
- **Talents**: from level 10, one random talent rank per level from any class's talent trees
  (tier and prerequisite rules do not apply). Heroes have no talent points to spend in the UI.
- Grants are caught up at login and on every level change, so multi-level jumps and characters
  made before Wildcard get everything they are owed. Each grant is announced in chat.

It needs two settings in `worldserver.conf` (set in the `.dist`; an existing config needs them by
hand, and the server log says so at the first Hero login when they are missing):
- `ValidateSkillLearnedBySpells = 0`, or other classes' spells are deleted at every login;
- `EnablePlayerSettings = 1`, where each Hero's count of granted abilities is kept.

## Items

| Entry | Item | Effect |
|---|---|---|
| 910001 | Titan Soul of Rested Experience | Grants 4 levels of rested experience (kill XP doubled across the next 4 levels' worth of XP). Can exceed the normal `Rate.Rest.MaxBonus` cap. Does nothing at max level. |
| 910002 | Veridus | Epic one-handed sword with a custom model (display 90001) converted from a Legion artifact export. Placeholder stats for testing. |

Get one in game with `.additem 910001` (or `910002`).

The item SQL in `data/sql/db-world/` is applied automatically by the database updater.

### Client patch
New item entries also need a row in the client's `Item.dbc`, otherwise players see a red "?"
icon: add them to `client-patch/patch_items.py` and rebuild `patch-enUS-4.MPQ` (see
`client-patch/README.md`, which also covers custom models). The item borrows spell 3361 only so the
3.3.5 client lets it be used; the script intercepts the use and the spell is never cast.
A dedicated spell in the client patch gives it a proper "Use:" tooltip line.

## Core changes this module relies on
- `Player::SetRestBonus(float, bool allowAboveCap)`: lets items grant rested experience above
  the cap, and keeps natural resting from trimming it back down.
