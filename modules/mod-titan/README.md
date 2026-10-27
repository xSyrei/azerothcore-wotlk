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
- **Starting kit (levels 1-8)**: every Hero starts with 4 random abilities and two tomes in its
  bags (`data/sql/db-world/titan_tomes.sql`, `src/item_titan_tomes.cpp`), both usable any number of
  times until level 9:
  - **Titan Tome of Randomization** (910003) rerolls the 4 abilities;
  - **Titan Tome of Selection** (910004) clears them and opens the Classless window with 4 picks.
  At **level 9 the kit locks**: picks never spent are rolled at random and both tomes vanish.
- **From level 10**: one random ability and one random talent rank per level. Abilities come from
  every class trainer's list (all 10 classes; passives, talent abilities and hunter pet handling
  are left out); talents from any class's talent trees (tier and prerequisite rules do not apply).
  Heroes have no talent points to spend in the UI.
- Owned abilities upgrade to the best rank for the Hero's level by themselves, no trainer needed.
- **Ability lists** (`src/wildcard.cpp`, by spell name, every rank and faction version; a name that
  matches nothing is logged at startup):
  - `BANNED_ABILITIES`: taken out of the game for Heroes (class mounts, tracking and scouting,
    Runeforging and every rune, Ritual of Refreshment, Divine Intervention, Lesser Heal, Disarm Trap,
    Create Firestone). Never granted or listed; a Hero who has one loses it at login and gets a
    random replacement. Everything in the Runeforging skill line is banned too.
  - `LEVEL_ONE_ABILITIES`: available from level 1 (first rank; later ranks keep their trainer
    levels), including talent and starting abilities such as Mutilate, Stormstrike, Crusader Strike
    and Feral Charge. Everything else unlocks at its trainer level.
  - Tame Beast also teaches Call Pet, Dismiss Pet, Revive Pet and Feed Pet, and Heroes count as
    Hunters for pets (taming, resummoning, stables; `src/hero_resources.cpp`).
- **Pick Lock for everyone**: every player always knows Pick Lock, with Lockpicking at its maximum
  for their level (5 per level). The core drops the Rogue-only Lockpicking skill at each login for
  other classes ("invalid for the race/class" in the log), so it is set again every login.
- Grants are caught up at login and on every level change, so multi-level jumps and characters
  made before Wildcard get everything they are owed. Each grant is announced in chat.

**Classless window** (**N**, the minimap button, `/classless` or `/cl`): every class's abilities by
spec with the level each needs, each spec's talent tree beside them (your ranks lit, the rest
greyed, prerequisite arrows drawn), an Abilities view of everything you own, your spells, talents and
specs on the right, a search across all classes, and the picks after a Tome of Selection. Each random
ability and talent rank from a level-up (level 10 on) is rolled on screen: a die shatters, the track
opens between two gryphons and the icons spin down onto the prize, then the die re-forms and it
all fades away. Catching up at login grants without rolling. It is the `Emberforge_Classless` addon
(`client-patch/AddOns/`); the server side is `src/classless.cpp`,
talking to it in addon whispers (prefix `EFCL`, protocol in both files).

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
