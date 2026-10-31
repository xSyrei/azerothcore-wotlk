# mod-titan

Project Emberforge custom content for AzerothCore.

## Classless Hero
Every character is created as the **Hero**: the hidden base class (Paladin, ID 2) renamed on the
client. `data/sql/db-world/hero_class.sql` opens it to all 10 races with each race's usual start
zone, `hero_outfit.sql` gives every Hero the Paladin starting outfit (the one the client's creation
preview shows), and `CharacterCreating.Disabled.ClassMask = 1533` in
`worldserver.conf` blocks every other class (GM accounts can still create any class for testing).
The matching client patch is built with `client-patch/` (see its README).

Heroes use **every class resource**: mana, rage, energy, Fury and runes, with combo points
on any target. `src/hero_resources.cpp` turns these on through AzerothCore's classless hooks
(`OnPlayerHasActivePowerType`, and `OnPlayerIsClass` for ability mechanics only). Starting zone
and stats still follow the real base class. The `Emberforge_Resources` addon (in
`client-patch/AddOns/`) shows the extra bars in game and colors Heroes ember orange.

Heroes **equip anything**: every weapon type (one- and two-handed, daggers, fist weapons, staves,
polearms, bows, guns, crossbows, thrown, wands), every armor type (cloth, leather, mail, plate,
shields, and librams, idols, totems and sigils in the relic slot), a weapon in each hand (Dual
Wield) and a ranged weapon with ammo. `src/hero_equipment.cpp` teaches the proficiencies and sets
the weapon and armor skills at every login and level (the core drops the ones a Paladin cannot
have); `Emberforge_Resources` shows the ammo slot, which the character sheet hides for a Paladin.
Fishing poles still need the Fishing profession.

Every character (Hero or not) keeps its weapon skills, Defense and Unarmed at the cap for its
level: `src/max_skills.cpp` fills them at login and after every level up.

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
all fades away; with **Instant rolls** (a check box in the window, or `/cl instant`) the track opens
straight on the prize. Catching up at login grants without rolling. It is the `Emberforge_Classless` addon
(`client-patch/AddOns/`); the server side is `src/classless.cpp`,
talking to it in addon whispers (prefix `EFCL`, protocol in both files).

It needs two settings in `worldserver.conf` (set in the `.dist`; an existing config needs them by
hand, and the server log says so at the first Hero login when they are missing):
- `ValidateSkillLearnedBySpells = 0`, or other classes' spells are deleted at every login;
- `EnablePlayerSettings = 1`, where each Hero's count of granted abilities is kept.

## Demon Hunter

The Demon Hunter is one of Wildcard's classes, with spells of Emberforge's own (the game has no
Demon Hunter): spell ids 90100 and up in `data/sql/db-world/titan_demon_hunter_spells.sql`, the same
spells in the client patch (`client-patch/patch_spells.py`, see its README). They join the Wildcard
pool (`CUSTOM_ABILITIES` in `src/wildcard.cpp`) and the Classless window's Havoc and Vengeance tabs.
Emberforge has no Runic Power: the client's power type 6 is renamed Fury everywhere (spell texts
in the client patch, UI strings and bar color in `Emberforge_Resources`). Death Knight abilities that
built or spent Runic Power build and spend Fury, the same pool as the Demon Hunter's.

| Spell | Ability | Level | Notes |
|---|---|---|---|
| 90100 | Demon's Bite | 1 | Weapon strike; generates 20-30 Fury |
| 90101 | Chaos Strike | 1 | 200% weapon damage as Chaos; costs 40 |
| 90103 | Fel Rush | 1 | Dashes 15 yards forward along the ground in about 0.08 sec (180 yards/sec, `spell_titan_dh_fel_rush`), Chaos damage to enemies in the path (90104); 10 sec cooldown |
| 90111 | Throw Glaive | 4 | 30-yard throw of the main-hand weapon (Heroic Throw's visual) for 45% of attack power, ricochets to 1 more enemy within 10 yards; 9 sec cooldown |
| 90102 | Blade Dance | 10 | Four strikes within 8 yards over 0.75 sec: three for 24.75% of attack power, the last for 74.2% (90107, 90108); dodges all attacks for 1 sec; costs 35 |
| 90114 | Consume Magic | 12 | 20-yard interrupt (3 sec school lockout, Counterspell) that also removes 1 magic buff and generates 20 Fury; 15 sec cooldown |
| 90109 | Vengeful Retreat | 14 | Clears snares and leaps back (Disengage; a hunter-family spell, as the core only leaps hunter spells backward); enemies within 8 yards take 40% of attack power and are slowed by 70% for 3 sec (90110); 25 sec cooldown |
| 90113 | Blur | 18 | 50% dodge and 35% less damage taken for 10 sec (Evasion); 60 sec cooldown, off the global cooldown |
| 90105 | Eye Beam | 20 | 2 sec channel, Chaos damage every 0.2 sec to enemies along the beam (20 yards, 2.5 each side; 90106); can move at 5% speed while channeling; costs 30 |
| 90112 | Chaos Nova | 24 | 40% of attack power as Chaos damage to enemies within 8 yards, stunned 2 sec; costs 30, 60 sec cooldown |
| 90117 | Felblade | 16 (Vengeance) | Charges a target up to 15 yards away; on arrival 50% of attack power as Fire damage and 40 Fury; 15 sec cooldown |
| 90118 | Darkness | 26 (Vengeance) | 8-yard zone for 8 sec: allies in it avoid all damage from a hit 20% of the time (`spell_titan_dh_darkness`); 3 min cooldown |
| 90116 | Metamorphosis | 30 | Demon form for 30 sec: 25% attack speed, 20% damage; 3 min cooldown |
| 90121 | Fel Barrage | 30 | Havoc's capstone: for 3 sec, every 0.25 sec (90122) enemies within 8 yards take 15% of attack power as Chaos damage while fel balls rain down on the ground around the Demon Hunter (90123, only a look, `spell_titan_dh_fel_barrage_pulse`); each pulse drains 5 Fury, ending it when Fury runs out (`spell_titan_dh_fel_barrage`); costs 10, 90 sec cooldown |
| 90119 | Immolation Aura | 1 (Vengeance) | 20% of attack power as Fire damage to enemies within 8 yards at once and every sec for 6 sec (90120); 20 Fury; 15 sec cooldown |
| 90124 | Shear | 1 (Vengeance) | Weapon strike; 10 Fury and 2 combo points (Vengeance's generator; Soul Fragments, 90126, are retired) |
| 90125 | Soul Cleave | 1 (Vengeance) | Finisher: 60% weapon damage a combo point to up to 5 enemies in front, heals 8% of maximum health plus 3% a point (90131, `spell_titan_dh_soul_cleave`); no Fury |
| 90129 | Torment | 6 (Vengeance) | Taunt, 30 yards, 8 sec cooldown |
| 90127 | Demon Spikes | 10 (Vengeance) | 20% parry and 30% more armor for 6 sec, off the global cooldown; costs 20, 15 sec cooldown |
| 90128 | Fiery Brand | 20 (Vengeance) | 50% of attack power as Fire damage, and the target deals 40% less damage for 8 sec; 60 sec cooldown |
| 90135 | Spirit Bomb | 30 (Vengeance) | Finisher: spends every combo point for 30% of attack power each as Fire damage within 8 yards (90136) and Frailty for 8 sec (90137: damage you deal them heals you for 8%, `spell_titan_dh_frailty`); no Fury |
| 90132 | Sigil of Chains | 16 (Vengeance) | Ground-targeted sigil, 30 yards; after 1.5 sec pulls every enemy within 8 yards to its center (90133, the core's pull to a point) and slows them 70% for 6 sec (`spell_titan_dh_sigil_of_chains`); 60 sec cooldown |
| 90138 | Soul Barrier | 24 (Vengeance) | Absorbs 20% of maximum health for 12 sec, 5% more per combo point spent (all, `spell_titan_dh_soul_barrier`); 30 sec cooldown |

Every ability above needs a one-handed weapon in each hand (two one-handed weapons, or a main-hand
and an off-hand weapon; not a shield or held-in-off-hand item). The main hand is checked by the
spells themselves, the off hand by `spell_titan_dh_dual_weapons` (`src/demon_hunter.cpp`, bound in
`titan_demon_hunter_scripts.sql`). Every Hero can dual wield (see Equipment).

Learn one in game with `.learn 90100` (needs the client patch for its name and icon).

### Havoc talents

The Demon Hunter's talent tree (`client-patch/patch_talents.py`): TalentTab 412 (ClassMask `1 << 11`, the
Demon Hunter) with talents 2300-2324, ranks 90200-90252, in the client's Talent.dbc / TalentTab.dbc and the
server's `talent_dbc` / `talenttab_dbc` (written with the spells into `titan_demon_hunter_spells.sql`). Wildcard
grants their ranks like any class's talents and the Classless window shows the tree. Talents only amplify:
nothing in the tree is an ability to press.

The Demon Hunter's spells are spell family 14 (unused by the game), each with a flag bit
(`FAMILY_FLAGS`), so damage, cost, cooldown and critical strike talents are the game's own spell
modifiers aimed at them (tooltips in game show the change). The rest are dummy auras holding a number,
read by `src/demon_hunter.cpp` (Blind Fury, Demonic Appetite, Prepared, Bloodlet, Master of the Glaive's
slow, Desperate Instincts, Momentum, Chaos Cleave, Fel Mastery's Fury, Cycle of Hatred, Trail of Ruin,
Soul Rending, Unbound Chaos, Demonic, Eternal Reaping, Inertia), with helper spells 90150-90160; Demon Blades is
a proc the game runs. Some talents lead to a follow-up that takes part of their power (Blind Fury to Looks Can
Kill, Chaos Cleave to Eternal Reaping, Unbound Chaos to Inertia); the tree's arrows show what needs what.

### Vengeance talents

TalentTab 413, talents 2325-2347, ranks 90253-90299, built the same way (`VENGEANCE` in `patch_talents.py`), one
theme a column, row after row: strikes (Improved Shear, Shear Fury, Focused Cleave, Feast of Souls, Void Reaver),
fire (Agonizing Flames, Infernal Armor, Fel Flame Fortification, Charred Flesh, Burning Alive, Fiery Demise,
Vulnerability under Spirit Bomb, Vengeance's capstone at level 30), souls and spikes (Demonic Wards, Razor Spikes,
Painbringer, Calcified Spikes, Fallout, Feed the Demon, Last Resort), sigils and shields (Quickened and
Concentrated Sigils, Cover of Darkness, Ruinous Bulwark). Fiery Demise, Vulnerability and Last Resort (a killing
blow leaves 40% health, then Uncontained Fel for 3 min) are a UnitScript's (`titan_dh_damage_taken`); helpers
90161-90167.

**Never delete a talent or a rank spell players have**: the server asserts on loading a character with a
talent spell it does not know. Retire one by leaving its rows and taking it out of the tree another way.

## Monk

The Monk (class 10, spec tabs Brewmaster, Mistweaver and Windwalker: skill lines 2002-2004) is built spec by
spec, Mistweaver first: spells 90300 and up (`client-patch/patch_monk.py`, a spell family of its own, 16),
copies of Blizzard healing spells (their timing and animations) with retail's icons and sounds and jade effects
of our own (no Druid or Priest look left), with what the copies cannot do in
`src/monk.cpp` (bound in `titan_monk_scripts.sql`, with the spells' share of spell power).
Windwalker's effects are retail's (`client-patch/spells/WW_*`, built by `spell_effects.retail_effect`: the glTF's
meshes and bone animation, textures recolored), in each strike's color, with retail's sounds and icons.
Mistweaver runs on mana; Windwalker on Energy and combo points: Tiger Palm awards them, the finishers take them all
(from 1 to 5) and hit as hard as the points spent (`spell_titan_monk_finisher`, `_finisher_channel`). The Monk has no
Chi.

| Spell | Ability | Level | Notes |
|---|---|---|---|
| 90300 | Soothing Mist | 1 | Channeled heal on an ally, every sec for 8 sec |
| 90301 | Vivify | 1 | 1.5 sec heal; every ally with your Renewing Mist is healed for half as much (90310) |
| 90302 | Renewing Mist | 4 | Heal over 20 sec; jumps to the most injured ally within 20 yards once its target is at full health; 9 sec cooldown |
| 90309 | Detox | 8 | Removes a Magic, Poison and Disease effect; 8 sec cooldown |
| 90303 | Enveloping Mist | 10 | 2 sec cast, heal over 6 sec; your healing on the target is 30% stronger meanwhile (`titan_monk_healing`) |
| 90308 | Thunder Focus Tea | 14 | The next Vivify is free or the next Enveloping Mist instant; 30 sec cooldown |
| 90306 | Life Cocoon | 18 | Shield of 30% of your maximum health for 12 sec; 2 min cooldown |
| 90304 | Essence Font | 20 | 3 sec channel: heals up to 6 allies within 30 yards every sec (90305); 12 sec cooldown |
| 90313 | Refreshing Jade Wind | 22 | A whirlwind around the Monk for 6 sec: every 0.75 sec (90314) heals the 6 most injured allies within 10 yards (`spell_titan_monk_jade_wind_heal`); retail's whirlwind (`client-patch/spells/MONK_JadeWind`); 45 sec cooldown |
| 90307 | Revival | 26 | Heals the raid within 40 yards and clears a Magic, Poison and Disease effect from each; 3 min cooldown |
| 90311 | Invoke Chi-Ji, the Red Crane | 30 | Mistweaver's capstone: Chi-Ji (creature 90311, `npc_titan_chi_ji`) follows the Monk for 25 sec, healing the 2 most injured allies within 40 yards every 2 sec with Crane's Gust (90312), and 1 more on each healing spell the Monk casts (any class's, `titan_monk_chi_ji_casts`); Enveloping Mist costs 50% less and casts 50% faster meanwhile; 3 min cooldown. Chi-Ji is retail's crane (`client-patch/models/chiji`, display 90205) |
| 90320 | Tiger Palm | 1 | Windwalker: weapon damage plus a little, awards 2 combo points; 50 Energy |
| 90321 | Blackout Kick | 3 | Finisher: 60% weapon damage a combo point, Rising Sun Kick's and Fists of Fury's cooldowns 1 sec shorter (`spell_titan_monk_blackout_kick`) |
| 90322 | Rising Sun Kick | 10 | Finisher: 90% weapon damage a combo point and Mortal Wounds (healing taken -25%, 10 sec); 10 sec cooldown |
| 90325 | Spinning Crane Kick | 12 | Finisher: 1.5 sec spin, moving allowed: 10% weapon damage a combo point to all enemies within 8 yards every 0.5 sec (90326) |
| 90323 | Fists of Fury | 16 | Finisher: 4 sec channel: 20% weapon damage a combo point to all enemies in a cone in front every sec (90324); 24 sec cooldown |
| 90327 | Whirling Dragon Punch | 24 | Only while Rising Sun Kick and Fists of Fury cool down (`spell_titan_monk_whirling_dragon_punch`): 90% weapon damage to all enemies within 8 yards three times in a second (90328); 24 sec cooldown |
| 90329 | Storm, Earth, and Fire | 30 | Windwalker's capstone: an Earth and a Fire spirit (creatures 90330, 90331, `npc_titan_monk_sef_spirit`, the Monk's look cloned onto them) fight at the Monk's side for 15 sec, each echoing every Windwalker strike the Monk lands for 35% of its damage (90330, 90331: Nature, Fire; `spell_titan_monk_sef_echo`); 90 sec cooldown |

### Monk talents

The Mistweaver and Windwalker trees (`client-patch/patch_monk_talents.py`, built with the Demon Hunter's by
`patch_talents.py`): TalentTabs 415 and 416 (ClassMask `1 << 9`, the Monk; 414 left for Brewmaster), talents
2400-2445, ranks 90400-90497, of the Monk's spell family so their spell modifiers aim at its spells. As the Demon
Hunter's, they only amplify, spread over the 11 rows. Nine are scripted in `src/monk.cpp` (dummy auras read by
`TalentValue`):

| Talent | Tree | Script |
|---|---|---|
| Rising Mist | Mistweaver | Vivify extends your Renewing Mists (`spell_titan_monk_vivify`) |
| Cocooned in Light | Mistweaver | Life Cocoon absorbs more (`spell_titan_monk_life_cocoon`) |
| Secret Infusion | Mistweaver | Thunder Focus Tea grants haste (90360, `spell_titan_monk_thunder_focus_tea`) |
| Dancing Mists | Mistweaver | Renewing Mist may settle on a second ally (`spell_titan_monk_renewing_mist_cast`) |
| Celestial Flock | Mistweaver | Chi-Ji's Crane's Gust heals one more ally (`npc_titan_chi_ji`) |
| Teachings of the Monastery | Windwalker | Blackout Kick may reset Rising Sun Kick (`spell_titan_monk_blackout_kick`) |
| Combo Breaker | Windwalker | Tiger Palm may award 2 more combo points (`spell_titan_monk_tiger_palm`) |
| Energy Burst | Windwalker | 5-point finishers restore Energy (`spell_titan_monk_finisher`) |
| Thunderfist | Windwalker | Rising Sun Kick adds a Nature strike (90361, `spell_titan_monk_thunderfist`) |
| Ancient Teachings | Windwalker | Any class's finishers may refund the combo points spent (`src/combo_points.cpp`) |

Each tree also has a few broad talents, WotLK's kind (Agility, Stamina or Intellect %, physical or all damage, healing
done, melee crit, casting speed, armor, dodge, mana regeneration while casting). They and Vengeance's Shattered Souls
(the same refund as Ancient Teachings; the two do not add up, the better chance counts: `src/combo_points.cpp`, for
every finisher, the game's own too) came after the trees, so they
have id runs of their own (talents 2350-2363 and 2450-2463, ranks 90500-90531 and 90550-90581: `patch_talents.TREES`):
a talent's ids never change once players may have it.

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
