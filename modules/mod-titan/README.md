# mod-titan

Project Emberforge custom content for AzerothCore.

## Classless Hero
Every character is created as the **Hero**: the hidden base class (Paladin, ID 2) renamed on the
client. `data/sql/db-world/hero_class.sql` opens it to all 10 races with each race's usual start
zone, `hero_outfit.sql` gives every Hero the Paladin starting outfit (the one the client's creation
preview shows) plus a Recruit's Tunic for the chest, and `CharacterCreating.Disabled.ClassMask = 1533` in
`worldserver.conf` blocks every other class (GM accounts can still create any class for testing).
The matching client patch is built with `client-patch/` (see its README).

Heroes use **every class resource**: mana, rage, energy and runes, with combo points
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
Fishing poles still need the Fishing profession. Runes recharge on the server, but the client only shows a
real Death Knight's recharging, so `hero_runes` (`src/hero_resources.cpp`) sends a Hero's rune state while any is
cooling down (every half second, and at once when one is spent or ready) and each rune as it comes back. Death Knight abilities come
from level 1 but hit like a level-55 class's, so below level 40 a Hero's Death Knight abilities deal a share of their
damage (`titan_dk_low_level`, same file): the spells (Icy Touch, Death Coil, Death and Decay) 15% at level 1, 25% at
10, 45% at 20, 70% at 30; the weapon strikes (Plague Strike, Blood Strike...) 35%, 50%, 70%, 85%; all from 40. The
diseases (their ticks are attack power's) and the heals stay whole. Unholy Presence's 0.5 sec off the global cooldown
reaches the client through the spell modifiers by family (below). Stances, presences and forms (every
class's, Stealth and Shadowform too) toggle off for a Hero, in or out of combat: casting one again while in it, or
right-clicking its buff, has the Classless addon ask the server to leave it (`CANCEL`, `classless.cpp`), since the
client (a Paladin's) neither toggles another class's forms nor lets a stance or presence go. Tuning by ability
(`titan_hero_damage_tuning`, same file): Soul Cleave deals 25% less, Blade Dance's strikes 15% less, every Feral Combat
ability 25% more and every Rogue ability 30% more; Thunder Clap costs 10 Rage and the Hearthstone cools down in 5
minutes (server and client patch: `hero_tuning`). Spell modifiers by family: the client applies a
spell modifier (talents' cost, cooldown, cast time, range cuts) only to its own class's spells, and a Hero is a
Paladin to it. The server sends every modifier the player has, totalled by class family, on opcode 0x51A whenever they
change (`Player::SendSpellModifiersByFamily`), and the Emberforge engine (`client-patch/engine/src/spellmods.cpp`)
replaces the client's lookup with one by the spell's own family: tooltips, buttons, the client's resource check and
its cooldowns show the talents. Talents made global on the server ("all means all") are not mirrored. As a fallback
for clients without the engine, the server tells the client a cooldown once more just after a cast when a talent
shortened it (`Player.cpp`, `AddSpellAndCategoryCooldowns`). At first login a Hero also finds one of each weapon kind
in the bags: a Battleworn Claymore, two Shortswords and two Worn Daggers (all one-hand, to dual wield either), a Bent Staff, a Worn Wooden Shield,
a Worn Shortbow. Ammo comes by itself: with a bow, crossbow or gun, the arrows or bullets of the Hero's level are put
in the ammo slot (Rough, Sharp, Razor, Jagged and Wicked Arrows; Light, Heavy and Solid Shot, Accurate Slugs, Impact
Shot: levels 1, 10, 25, 40, 55) at login, level-up and when a ranged weapon goes on, and never run out
(`InfiniteAmmo.Enabled`, docker-compose.yml). Better ammo the Hero put there stays.

**No reagents**: class abilities need no reagents, tools or totems (Rune of Portals, Soul Shards, Ankh, a Fire
Totem...), on the server (`hero_resources.cpp`) and in the client patch (`patch_spells.py`, `drop_class_reagents`).
Professions keep theirs.

Every Hero wears four **Emberforged Satchels** (item 910020, 30 slots, `data/sql/db-world/titan_satchels.sql`): at
every login `src/hero_equipment.cpp` puts one in each empty bag slot, and replaces any other bag: its contents go
into the backpack and the satchels, and the old bag comes by mail from the Keeper of the Ember. A bag whose contents
do not fit stays until a later login. The client knows the item from `client-patch/patch_items.py`.

Every character (Hero or not) keeps its weapon skills, Defense and Unarmed at the cap for its
level: `src/max_skills.cpp` fills them at login and after every level up.

Nothing else comes from the base class (`data/sql/db-world/hero_cleanup.sql`):
- no starting class spells (the Paladin skill lines are removed) and no Paladin action buttons;
- Paladin trainers no longer train, and Paladin-only class quests are disabled;
- no talent points to spend: Wildcard grants talents (below).

## Wildcard
How a Hero progresses (`src/wildcard.cpp`): random, steered so a build takes shape (the decisions:
`.agents/plans/wildcard-pass/`).
- **Starting kit (levels 1-7)**: every Hero starts with 4 random abilities and two tomes in its
  bags (`data/sql/db-world/titan_tomes.sql`, `src/item_titan_tomes.cpp`), both usable any number of
  times through level 7:
  - **Titan Tome of Randomization** (910003) rerolls the 4 abilities;
  - **Titan Tome of Selection** (910004) clears them and opens the Classless window with 4 picks.
  At **level 8 the kit locks**: picks never spent are rolled at random and both tomes vanish.
- **From level 8 the level-up rolls alternate**: a random talent at 8, a random ability at 9, a
  talent at 10, an ability at 11, and so on. Abilities come from every class trainer's list (all 10
  classes; passives and hunter pet handling are left out); talents from any class's talent trees, each at its top
  rank (talents rolled at a lower rank before go to their top rank at login). Tier rules do not apply; a talent
  building on another (Eternal Reaping on Chaos Cleave) only comes once that one is known. Heroes have no talent points to spend in the UI. Characters that already had
  more than this keep them, and roll again once their level catches up.
- **Talent abilities are abilities** (`TALENT_ABILITIES`, and any talent among `LEVEL_ONE_ABILITIES`: Crusader
  Strike, Mutilate, Stormstrike; Mortal Strike, Shadowform, Mangle, Titan's Grip...): about
  130 talents are learned as abilities, rolled with the ability rolls from their tier's level (row 1: 10, row 11:
  60), never by a talent roll. They stay on their talent tree in the Classless window, lit when known. Ones rolled
  as talents before are kept, and their points no longer count, so another talent is owed.
- **Every ability by level 59**: one first learned past level 60 in the game (the Burning Crusade's and Wrath's:
  Obliterate at 61, Kill Shot at 71, Mirror Image at 80) comes at 40 plus its level past 60 (41-59), one at 60 at
  59 (60's roll is a capstone). Later ranks keep their levels. A talent's other spells come with its ability
  (Metamorphosis: Shadow Cleave), and faction pairs are one ability, the Hero's faction's (Bloodlust for the Horde,
  Heroism for the Alliance; Teleport: Shattrath).
- **Capstones at level 60** (`CAPSTONE_ABILITIES`, `CUSTOM_CAPSTONES`: Bladestorm, Divine Storm, Fel Barrage,
  Storm, Earth, and Fire...): rolled only by level 60's roll, which is a capstone of the Hero's role instead of a
  talent rank (Tank: Dancing Rune Weapon, Howling Blast, Hammer of the Righteous, Shockwave, Summon Gargoyle, Spirit
  Bomb; Healer: Wild Growth, Invoke Chi-Ji, Beacon of Light, Penance, Guardian Spirit, Riptide; Ranged and Melee:
  the rest, Howling Blast and Summon Gargoyle Melee's too). No other roll or pick gives one.
- **Rolls follow the role**: abilities are tagged by what they do (melee or ranged physical damage: Melee; magic
  damage: Ranged; heals and shields on others: Healer; taunts and threat: Tank; `ROLE_ABILITIES` by hand for forms,
  stances and scripted spells), and a level-up or kit roll only gives one suiting the Hero's role: Melee takes
  Melee, Ranged takes Ranged, Healer takes Healer and Ranged, Tank takes Tank and Melee. Untagged abilities (buffs,
  utility, crowd control) suit every role; with no role, or nothing left that suits, anything goes. Tome of
  Selection picks are free.
- **Ability rolls are steered** (`GrantRandom`): one pet family (once a Hero summons a hunter pet, a demon or a
  ghoul, the other families' summons stay out); the kit (before level 8) first gets 2 abilities doing the role's
  work (Healer: heals; Tank: a threat ability, then a melee one; no role: damage); after 2 rolls in a row gave a
  cooldown of 10 sec or more, the next is one to press on repeat; and the pick is weighted toward the damage schools
  the Hero's abilities already deal (+25% a matching ability, up to 4) and the resources they spend (+40%).
  Abilities that only work with another wait for it (`PAIRED_ABILITIES`: Conflagrate for Immolate, Ice Lance for a
  Frost chill, Obliterate and Scourge Strike for a disease, Swiftmend for a heal over time...).
- **Talents**: about 70% of the talent rolls improve an ability the Hero knows (one its class masks modify; a
  scripted talent: one of its class's abilities), the rest are talents for everyone suiting the role (stats, all
  damage or healing, armor and avoidance, talents teaching an ability for the role); either pool stands in for the
  other. **"All means all"**: a talent whose words reach every spell of a kind ("all your healing spells", "your
  spells", "your Frost spells": 44 of the game's talents, `src/global_talents.h` from
  `tools/wildcard/global_talents.py`) applies to every class's spells of that kind (`titan_global_talents`), and is
  rolled for a Hero knowing one; Grace of the Crane (all healing), Any Means Necessary (all Shadow damage) and
  Demonic Wards (all damage taken) too. A talent's changed cooldown is told to the client (Player.cpp), which
  applies spell modifiers only to its own class's spells.
- **Reroll blacklist**: a name rerolled away (an ability, or a talent of that name in every tree: Toughness) is not
  rolled again for 25 rerolls (`titan_reroll_blacklist`, db-characters).
- **Roles**: Melee, Ranged, Healer or Tank, picked in the Classless window under the classes (not in
  combat; kept per character). Each is a buff (90603-90606, `client-patch/patch_player.py`): Melee and
  Ranged cause 50% less threat, Tank 200% more (and takes 5% less damage, with 5% more armor), Healer
  unchanged.
- Owned abilities upgrade to the best rank for the Hero's level by themselves, no trainer needed: every rank up to it
  is learned, as a trainer teaches them, and after every learn (a roll, reroll, pick, spec switch or talent) all the
  Hero's abilities are checked for ranks still pending, so none is left behind. Group buffs come the
  same way (`GROUP_VERSIONS`): Arcane Brilliance with Arcane Intellect, Prayer of Fortitude, Spirit and Shadow
  Protection with their single-target buffs, Gift of the Wild with Mark of the Wild, Dalaran Brilliance with Dalaran
  Intellect, the Greater Blessings with the Blessings, each once the Hero's level reaches it; never rolled on their
  own, and gone with the buff.
- **Requirements**: an ability cast only in a form (Swipe (Cat): Cat Form; a Warrior's abilities need no stance:
  `hero_resources.cpp` and `free_warrior_stances`, `patch_spells.py`), only in stealth (Ambush: Stealth; Pounce: Prowl) or on a pet (Mend Pet, Kill Command: Tame
  Beast; Health Funnel: a demon; Death Pact: Raise Dead) is rolled or picked only once the Hero knows an
  ability giving it. Forms, stances and stealth are read from the spells; pets from their targets and
  `PET_ABILITIES`. The Classless window shows them on the ability ("Requires Cat Form", red until met).
  A finisher (it spends combo points: the game's own by their spell, the Monk's and Demon Hunter's by name,
  `SCRIPTED_FINISHERS`) needs an ability awarding combo points (by its spell's effect, or `CODE_BUILDERS`: Mangle
  (Cat), Hemorrhage); Overpower is not one (its point comes from a dodge). The window says "Requires an ability that
  awards combo points".
- **Ability lists** (`src/wildcard.cpp`, by spell name, every rank and faction version; a name that
  matches nothing is logged at startup):
  - `BANNED_ABILITIES`: taken out of the game for Heroes (class mounts, tracking and scouting,
    Runeforging and every rune, Ritual of Refreshment, Divine Intervention, Lesser Heal, Disarm Trap,
    Create Firestone). Never granted or listed; a Hero who has one loses it at login and gets a
    random replacement. Everything in the Runeforging skill line is banned too.
  - `LEVEL_ONE_ABILITIES`: available from level 1 (first rank; later ranks keep their trainer
    levels), including talent and starting abilities such as Mutilate, Stormstrike, Crusader Strike
    and Feral Charge. Everything else unlocks at its trainer level.
  - Tame Beast also teaches Call Pet, Dismiss Pet, Revive Pet, Mend Pet and Feed Pet (all at level 1;
    unlearning Tame Beast unlearns them), and Heroes count as Hunters for pets (taming, resummoning, stables;
    `src/hero_resources.cpp`).
  - A Hero's pets and demons know all their abilities from level 1 (each one's first rank; later ranks at their
    levels; `src/pets.cpp`). Unlearning or rerolling the ability that summoned a pet dismisses it (a tamed beast
    once Call Pet is gone; it stays in the stable).
- **Pick Lock for everyone**: every player always knows Pick Lock, with Lockpicking at its maximum
  for their level (5 per level). The core drops the Rogue-only Lockpicking skill at each login for
  other classes ("invalid for the race/class" in the log), so it is set again every login.
- Grants are caught up at login and on every level change, so multi-level jumps and characters
  made before Wildcard get everything they are owed. Each grant is announced in chat.

**Classless window** (**N**, the minimap button, `/classless` or `/cl`): every class's abilities by
spec with the level each needs, each spec's talent tree beside them (your ranks lit, the rest
greyed, prerequisite arrows drawn), an Abilities view of everything you own, your spells, talents and
specs on the right, a search across all classes, and the picks after a Tome of Selection. Each random
ability and talent rank from a level-up (level 8 on) is rolled on screen: a die shatters, the track
opens between two gryphons and the icons spin down onto the prize, then the die re-forms and it
all fades away; with **Instant rolls** (a check box in the window, or `/cl instant`) the track opens
straight on the prize. Catching up at login grants without rolling. It is the `Emberforge_Classless` addon
(`client-patch/AddOns/`); the server side is `src/classless.cpp`,
talking to it in addon whispers (prefix `EFCL`, protocol in both files).

**GM commands** (`src/titan_commands.cpp`, GM accounts):
- `.spellids [on|off]`: shows the spell id on every spell tooltip (the Classless window's abilities and talent
  ranks, the spellbook, action bars, buffs; the talent frame shows its talent id). Kept per character.
- `.emberspark [amount]`: adds Emberspark to the selected player's (or your) account; without an amount, shows its
  balance (see Emberspark).
- `.daytime [hour]`: your own client's time of day, 0-23 (only yours; the clock runs on from there), to look at the
  lighting, water and fog at any hour. Without an hour: back to the real time.
- `.runes`: the selected Hero's (or your) runes as the server sees them, each one's type and time until ready. If
  the server says a rune is ready but the game still refuses a rune spell, the problem is on the client.
- `.learnability <spell id or link>`: teaches the selected player (or yourself) that ability the way Wildcard
  does (best rank for the level, with its companion spells), or that talent rank as a talent (plain `.learn`
  cannot give a Hero talents). Requirements, role and level are not checked.

It needs two settings in `worldserver.conf` (set in the `.dist`, and by `docker-compose.yml`'s environment, which
wins over an existing config; elsewhere by hand, and the server log says so at the first Hero login when missing):
- `ValidateSkillLearnedBySpells = 0`, or other classes' spells are deleted at every login;
- `EnablePlayerSettings = 1`, where each Hero's count of granted abilities is kept.

## Demon Hunter

The Demon Hunter is one of Wildcard's classes, with spells of Emberforge's own (the game has no
Demon Hunter): spell ids 90100 and up in `data/sql/db-world/titan_demon_hunter_spells.sql`, the same
spells in the client patch (`client-patch/patch_spells.py`, see its README). They join the Wildcard
pool (`CUSTOM_ABILITIES` in `src/wildcard.cpp`) and the Classless window's Havoc and Vengeance tabs.
Emberforge has no Runic Power: every spell that cost or gave it costs or gives Rage (the same tenths), on the
client (`convert_runic_power` in `client-patch/patch_spells.py`) and the server (`titan_runic_power_to_rage` in
`src/hero_resources.cpp`), and every text that named it says Rage. Death Knight abilities and the Demon Hunter's
(Fury before) build and spend Rage, the Warrior's pool. The Demon Hunter's damage is Shadow (Chaos, every school,
before): it grows with Shadow spell power and Shadow damage bonuses.

| Spell | Ability | Level | Notes |
|---|---|---|---|
| 90100 | Demon's Bite | 1 | Weapon strike; generates 20-30 Rage |
| 90101 | Chaos Strike | 1 | 200% weapon damage as Shadow; costs 40 |
| 90103 | Fel Rush | 1 | Dashes 15 yards forward along the ground in about 0.08 sec (180 yards/sec, `spell_titan_dh_fel_rush`), Shadow damage to enemies in the path (90104); 10 sec cooldown |
| 90111 | Throw Glaive | 4 | 30-yard throw of the main-hand weapon (Heroic Throw's visual) for 45% of attack power, ricochets to 1 more enemy within 10 yards; 9 sec cooldown |
| 90102 | Blade Dance | 10 | Four strikes within 8 yards over 0.75 sec: three for 24.75% of attack power, the last for 74.2% (90107, 90108); dodges all attacks for 1 sec; costs 35 |
| 90114 | Consume Magic | 12 | 20-yard interrupt (3 sec school lockout, Counterspell) that also removes 1 magic buff and generates 20 Rage; 15 sec cooldown |
| 90109 | Vengeful Retreat | 14 | Clears snares and leaps back (Disengage; a hunter-family spell, as the core only leaps hunter spells backward); enemies within 8 yards take 40% of attack power and are slowed by 70% for 3 sec (90110); 25 sec cooldown |
| 90113 | Blur | 18 | 50% dodge and 35% less damage taken for 10 sec (Evasion); 60 sec cooldown, off the global cooldown |
| 90105 | Eye Beam | 20 | 2 sec channel, Shadow damage every 0.2 sec to enemies along the beam (20 yards, 2.5 each side; 90106); can move at 5% speed while channeling; costs 30 |
| 90112 | Chaos Nova | 24 | 40% of attack power as Shadow damage to enemies within 8 yards, stunned 2 sec; costs 30, 60 sec cooldown |
| 90117 | Felblade | 16 (Vengeance) | Charges a target up to 15 yards away; on arrival 50% of attack power as Fire damage and 40 Rage; 15 sec cooldown |
| 90118 | Darkness | 26 (Vengeance) | 8-yard zone for 8 sec: allies in it avoid all damage from a hit 20% of the time (`spell_titan_dh_darkness`); 3 min cooldown |
| 90116 | Metamorphosis | 30 | Demon form for 30 sec: 25% attack speed, 20% damage; 3 min cooldown |
| 90121 | Fel Barrage | 30 | Havoc's capstone: for 3 sec, every 0.25 sec (90122) enemies within 8 yards take 15% of attack power as Shadow damage while fel balls rain down on the ground around the Demon Hunter (90123, only a look, `spell_titan_dh_fel_barrage_pulse`); each pulse drains 5 Rage, ending it when Rage runs out (`spell_titan_dh_fel_barrage`); costs 10, 90 sec cooldown |
| 90119 | Immolation Aura | 1 (Vengeance) | 20% of attack power as Fire damage to enemies within 8 yards at once and every sec for 6 sec (90120); 20 Rage; 15 sec cooldown |
| 90124 | Shear | 1 (Vengeance) | Weapon strike; 10 Rage and 1 combo point (Vengeance's generator; Soul Fragments, 90126, are retired) |
| 90125 | Soul Cleave | 1 (Vengeance) | Finisher: a base and 15% of attack power a combo point to up to 5 enemies in front, heals 8% of maximum health plus 3% a point (90131, `spell_titan_dh_soul_cleave`); no Rage |
| 90129 | Torment | 6 (Vengeance) | Taunt, 30 yards, 8 sec cooldown |
| 90127 | Demon Spikes | 10 (Vengeance) | 20% parry and 30% more armor for 6 sec, off the global cooldown; costs 20, 15 sec cooldown |
| 90128 | Fiery Brand | 20 (Vengeance) | 50% of attack power as Fire damage, and the target deals 40% less damage for 8 sec; 60 sec cooldown |
| 90135 | Spirit Bomb | 30 (Vengeance) | Finisher: spends every combo point for 30% of attack power each as Fire damage within 8 yards (90136) and Frailty for 8 sec (90137: damage you deal them heals you for 8%, `spell_titan_dh_frailty`); no Rage |
| 90132 | Sigil of Chains | 16 (Vengeance) | Ground-targeted sigil, 30 yards; after 1.5 sec pulls every enemy within 8 yards to its center (90133, the core's pull to a point) and slows them 70% for 6 sec (`spell_titan_dh_sigil_of_chains`); 60 sec cooldown |
| 90138 | Soul Barrier | 24 (Vengeance) | Absorbs 20% of maximum health for 12 sec, 5% more per combo point spent (all, `spell_titan_dh_soul_barrier`); 30 sec cooldown |

Every attack above needs a one-handed weapon in each hand (two one-handed weapons, or a main-hand
and an off-hand weapon; not a shield or held-in-off-hand item); Fel Rush, Vengeful Retreat, Chaos Nova, Blur,
Consume Magic, Darkness, Demon Spikes, Fiery Brand, Torment and Sigil of Chains need no weapon. The main hand is checked by the
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
Vulnerability under Spirit Bomb, Vengeance's capstone at level 60), souls and spikes (Demonic Wards, Razor Spikes,
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
Mistweaver runs on mana; Windwalker on Energy and combo points: Tiger Palm awards them, Spinning Crane Kick (the
finisher; Rising Sun Kick and Fists of Fury are cooldowns, no points) takes them all
(from 1 to 5) and hits as hard as the points spent (`spell_titan_monk_finisher`, `_finisher_channel`). The Monk has no
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
| 90321 | Blackout Kick | 3 | No cost, 4 sec cooldown: a base and 40% of attack power, Rising Sun Kick's and Fists of Fury's cooldowns 1 sec shorter (`spell_titan_monk_blackout_kick`) |
| 90322 | Rising Sun Kick | 10 | No combo points: a base and 140% of attack power, and Mortal Wounds (healing taken -25%, 10 sec); 10 sec cooldown |
| 90325 | Spinning Crane Kick | 12 | Finisher: 1.5 sec spin, moving allowed: a base and 4.5% of attack power a combo point to all enemies within 8 yards every 0.5 sec (90326, `spell_titan_monk_finisher_strike`) |
| 90323 | Fists of Fury | 16 | No combo points: 4 sec channel: a base and 30% of attack power to all enemies in a cone in front every sec (90324); 24 sec cooldown |
| 90327 | Whirling Dragon Punch | 24 | 90% weapon damage to all enemies within 8 yards three times in a second (90328); 24 sec cooldown, no other requirement |
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

### Ability ranks
The Demon Hunter's and Monk's abilities whose own numbers grow with level have a rank every 10 levels
from the level they are learned at, like the game's class spells (`client-patch/patch_ranks.py`):
Demon's Bite, Shear, Soul Cleave, Soothing Mist, Vivify, Renewing Mist, Enveloping Mist, Revival, Tiger
Palm, Blackout Kick and Rising Sun Kick (Tiger Palm: ranks at 1, 11, 21 ... 71; Revival: 26 ... 76).
Each rank starts with the value the ability had at its level and grows the same per level up to the
next one, so damage and healing follow the curve they always had; Wildcard learns the best rank by
itself. The rank spells are 91000-91106, chained in `spell_ranks` (written with the spells to
`titan_demon_hunter_spells.sql`); their scripts are bound to every rank (negative ids). Abilities that
hit for weapon damage, attack power or a script's amount, and the utility ones, keep one rank.

## Smoldering Adventurer's Cache
A Hero gets a **Smoldering Adventurer's Cache** (910005, stacks to 200; `src/smoldering_cache.cpp`,
`data/sql/db-world/titan_smoldering_cache.sql`) for every level gained and every quest turned in (by mail when the
bags are full). Opening one gives a piece of **Leveling Scaled** gear for the Hero's level in the slot of their lowest
item level piece (an empty slot counts as 0; a scaled piece counts at the level it was made for):
- the cache has its own pool (no Blizzard gear; `src/scaled_gear.h`, `data/sql/db-world/titan_scaled_gear.sql`, both
  made by `tools/scaling/scaled_gear.py`): a cloth, leather, mail and plate set (Emberweave, Emberhide, Emberlink,
  Emberforged), a cloak, neck, ring, trinket, shield, off-hand and one weapon of each kind, uncommon and soulbound;
- every piece has a template per level (912000 + piece x 60 + level - 1): item level `level + 5` (level 60's: 65),
  required level `level`, and a green's armor and weapon damage at that item level, so the tooltip and the Character
  window show the piece's real item level. Its stats come from a nameless suffix, one per stat mix (Bear, Monkey,
  Eagle, Owl, Tiger, Sorcerer, Elder, Champion, Hunt; suffixes 2000-2008, the client has them too:
  `client-patch/patch_scaled_gear.py`), scaled by the template's item level as any green's; a suffix that would show
  a "+0" line is not given (a green's stat budget is too small at the lowest levels: those pieces have armor and
  damage only). The tooltip tags the piece "Leveling Scaled" in green under its name, above its item level (as a
  tier piece's tag; the `Emberforge_Resources` addon, by item id);
- scored for the role: Melee strength or agility (whichever the Hero's melee abilities use: a warrior's, paladin's or
  death knight's strength, the others' agility), attack power and weapon damage; Ranged intellect and spell power;
  Healer intellect, spirit, spell power and mana regeneration; Tank stamina, strength, armor and defense; no role a
  bit of everything. Armor type is whatever scores best among the role's two: Tank plate or mail, Melee leather or
  mail, Ranged cloth or mail, Healer leather or plate. A random pick within 90% of the best is given. The main hand
  gets a one-hand weapon or a two-hander, half the time each (two-handers would always score higher).

**Empty slots fill themselves**: gear a Hero gets (loot, quest rewards, purchases, crafting, caches) is put on at
once when its slot is empty and the Hero can wear it.

## Prestige
At level 60 a Hero can start again from level 1 at the **Keeper of the Ember** (creature 910100, "Prestige": Stormwind's
Trade District and Orgrimmar's Valley of Strength; `src/prestige.cpp`, `data/sql/db-world/titan_prestige.sql`).
Talking to it opens the **Prestige window** (the `Emberforge_Prestige` addon, in the Classless window's look; addon
whispers with prefix `EFPR`, protocol in both files): the Emberspark balance, the Hero's prestige, a level ring
(a dot a level, filling with their experience to the cap), this run's
time and the fastest run, what a prestige takes, keeps and gives, and level 60's reward. Its Prestige button asks for
confirmation; the server checks again that the Hero is beside the Keeper (15 yards), alive, out of combat and not in
a dungeon, battleground or queue.
- **Lost:** the level (back to 1, at the race's starting point); every Wildcard ability and talent (a new random kit
  and both tomes, as a new Hero: `Wildcard::Reset`); every quest, in the log or done (dailies, weeklies and monthlies
  too), so all can be done and rewarded again (but the game's hidden tracking quests).
- **Kept:** gear, gold, Emberspark, professions, mounts, reputation, achievements and the role. Every piece worn is
  packed into a **Prestige Gear Cache** (910021, `titan_prestige.sql`; one bag slot, by mail when the bags are full):
  the very items stay in the database (`item_instance`, listed by the cache in `titan_prestige_gear`,
  `data/sql/db-characters/`). Used, the cache gives them back into the bags (what does not fit stays in it; empty, it
  is gone), and each piece goes back on in its old slot as soon as its level allows, whatever is worn there then (to
  the bags). Destroying the cache destroys the gear in it (its tooltip says so).
  A piece's level (`Prestige::EquipLevel`) is its required level, or for a weapon or armor (uncommon or better)
  with none (the squish gives most one: below) its item level less 10, up to item level 70 (none when soulbound:
  dungeon drops and rewards come at the Hero's own level), else the level cap. A
  Hero cannot put on gear above their level, and gear worn above it at login comes off into the bags (ratings are
  worth far more at low levels: TBC rewards on a level 1 Hero gave hundreds of percent of critical strike).
- **Given:** a new Hero's gear (the race's starting outfit, worn, and the starter weapons in the bags,
  `HeroEquipment::StarterBagKit`), 500 Emberspark, 2 Titan Souls of Rested Experience (910001), **Prestigious**
  (90607: 10% more experience from everything, until level 60; the bonus is `OnPlayerGiveXP`'s, the buff shows
  it) and the quest **Trial of Ember: Prestigious** (910100, `titan_prestige.sql`). Reaching level 60 again ends
  the run (the fastest run is kept) and completes the quest; turned in to the Keeper (who offers it before the window), it gives 5,000
  Emberspark. A run started before the quest existed gets them at level 60. Both are announced to the realm.
- **Specialization locked** during a run (`specs.cpp`); the role can change.

It is all kept in the character's settings (`mod-titan-prestige`: rank, run, its start, the fastest run, the stashed
gear by slot). Still to come: the rank on the name or tooltip, Runic Enhancements carried over.

The Keeper is a grey-bearded dwarf in the Black Tuxedo, its pants, Dress Shoes and a Gilneas top hat (display 90206):
its skin is baked by `client-patch/bake_npc.py` (the dwarf's skin, face, beard and the clothes' textures, from the
3.3.5a client) into `textures/BakedNpcTextures/EF-Keeper.png`; the display rows are `patch_creatures.py`'s
(`HUMANOIDS`, with `creaturedisplayinfoextra_dbc` for the server); the hat is retail's dwarf male model
(`models/gilneashat`, made static by `models/gltf_to_wowexport.py --pivot`), item display 90002 (`patch_items.py`).

## Specializations
A Hero has four loadouts of Wildcard abilities and talents (`src/specs.cpp`; characters database: `titan_specs`,
`titan_spec_entries`, `data/sql/db-characters/titan_specs.sql`). Specialization I and II are free; III (25,000
Emberspark) and IV (50,000) are bought from Silas Darkmoon, in the Darkmoon window.
- **Switching** (out of combat, not in a battleground): the Classless window's Specs tab, right-click a spec,
  Activate (or the Darkmoon window's Specializations page). The active spec's abilities, talents and action bars are
  kept and the other's put back, with what its level is owed since. A spec never built is empty (dormant: no
  abilities, talents, rune or bars, nothing rolled into it, no picks, `Specs::IsDormant`) until the Hero prestiges
  while it is active: the prestige builds it from level 1 and the other specs keep their builds. Action bars:
  `titan_spec_actions`
  (`data/sql/db-characters/titan_spec_actions.sql`).
- **Names and icons**: click a spec's icon in the Specs tab (or right-click, Rename / icon): a name and any of the
  macro icons.
- **Rerolls**: right-click one of your spells or talents, Reroll: an Eye of Unlearning for another ability the Hero can
  learn (a capstone for another capstone), or the talent for another talent. Before the kit locks (level 8) an
  ability is **unlearned for free** instead (Unlearn): its pick comes back, to pick another.
- **Locks**: right-click, Lock: a locked ability or talent is never rerolled, a locked spec's none are; padlocks show
  on them. Nothing locked can be prestiged: unlock first. A prestige starts the active spec over (a dormant one is built
  by it); the other specs keep their builds (names, icons and the ones bought stay).

## Legendary Runes
From level 10 a Hero binds a **Legendary Rune** to each specialization (`src/runes.cpp`; the design: the
"Emberforge Legendary Runes" doc). A rune is a loop: a builder makes a resource, a spender cashes it in with an
empowered effect; three modifier slots (Invocation: how you build; Manifestation: what the spender does; Adaptation:
utility or a tradeoff) take one pick each, from three.
- **The panel** (`Emberforge_Runes`, `/runes`; addon whispers with prefix `EFRN`, protocol in both files) opens by
  itself at level 10, and at login while the active specialization has no rune: up to three runes suggested from
  the Hero's abilities and role, and Browse all for every rune, three a page (one missing a required ability or not
  built yet shakes and says so). Binding asks first. It is the "Legendary Rune Panel" mockup over the game world: the
  chosen card erupts and the rest burn away, it condenses into the rune's medallion, which breaks into the three
  slots, and each slot's three options rise in (a done slot can be chosen again). Its art is baked from the mockup's
  own drawing code by `client-patch/build_rune_art.py` (`rune_art/bake.js` in headless Chromium: `Art/`, `Art.lua`);
  its fonts are Cinzel and Alegreya Sans (`Fonts/`, SIL Open Font License).
- **One rune per specialization**, chosen once: only a prestige clears them (`titan_runes`, db-characters; chosen
  again from level 10). Modifiers change freely out of combat. A new specialization picks its own.
- **Built so far: the Rune of Fel Commandment** (rune 21; the other 20 are listed). Needs Throw Glaive, Demon's Bite
  or Chaos Strike, and a warlock demon summon. Demon's Bite, Chaos Strike (2 on a crit) and the Hero's demon (once
  every 4 sec) grant **Fel Command** (90701, up to 5, 20 sec). At 5, Throw Glaive also fires a **Chaos Bolt Volley**:
  3 bolts 0.15 sec apart, the first at the target, the next at other enemies within 10 yards (nearest first,
  crowd-controlled ones skipped), each 60% of a Chaos Bolt of the Hero's level plus the higher of spell power or
  half attack power (Fire, never resisted). In Metamorphosis (the Demon Hunter's or Demonology's) 5 bolts and a
  **Felguard** (910120, 8 sec: Felstorm, Cleave, no threat; another volley resets it). Its modifiers: Infernal Pact,
  Demonic Ascension, Dreadfire Discipline (Shadow Bolt or Incinerate build instead of Demon's Bite; offered only to a
  Hero with one); Imp Legion, Shattered Glaive, Fel Host (both legions: every Throw Glaive spends the Fel Command,
  a demon for each stack for 10 sec with no limit to how many are out, Fel Imps 910121, or Felguards in
  Metamorphosis with Fel Host; each volley bolt deals 10% more for every demon out, the Hero's own included);
  Metamorphic Bond, Fel Rift (Fel Rush to the volley's target, `demon_hunter.cpp`), Pact of Ruin. Numbers: the
  doc's tuning table, in `runes.cpp`.
- Spells 90700-90718 (`client-patch/patch_runes.py`, in `titan_demon_hunter_spells.sql` and the client patch): the
  rune's aura (its procs grant the stacks), Fel Command, the bolt, the summons' no-threat auras, Commander's Fury (no
  longer given), Metamorphic Bond, Fel Rift, Felstorm, Metamorphic Cleave, and one hidden marker per modifier. The panel's texts:
  `Emberforge_Runes/Catalog.lua`, made from the doc by `tools/runes/make_catalog.py`. Throw Glaive's button glows
  and shows Chaos Bolt at 5 Fel Command (a Felguard mark in Metamorphosis).
- **The Rune of the Sanctified Paragon** (rune 22, its own concept doc and its revised balance draft; its slots
  Presence, Manifestation, Covenant, the panel's labels follow a rune's own `slots` in `Art.lua`). Needs Consecration;
  suggested to every role. Consecration becomes **Sanctified Paragon** (90731): cast it to turn a lasting aura on
  (Consecration's mana and cooldown; no ground left behind), again to turn it off (free). It costs 1% of base mana a
  second and ends when that cannot be paid. Every second one pulse at 40% of a Consecration tick (the Hero's rank's
  plus 4% of spell power and attack power), through the Manifestation: Sanctified Condemnation (damage to each enemy
  within 8 yards, 90732; 2% more a tick in a row, up to 10%, 90733) or none, Merciful Ground (no damage: the single
  most injured party member healed, 90734) or Shelter of the Paragon (no damage: every 5 sec one party member
  shielded for five ticks, 90737, one shield of the aura's at a time). Presence: Boundless Sanctity (12 yards, 25%
  less), Hallowed Footsteps (4 yards, trails of 4 yards for 2 sec, 90739; an enemy once a pulse), Seat of the Paragon
  (still 3 sec: 10% more, 90738). Covenant: Bastion of Faith (50% more threat) and Weight of the Unworthy (15% slower
  from the third damaging tick in a row while it stays, 90736), both only with Sanctified Condemnation (the panel
  refuses them otherwise), Measured Devotion (25% less upkeep, 10% less output). Spells 90730-90749
  (`patch_runes.py`); its look: retail's Remorseless Winter (`client-patch/spells/PAL_SanctifiedParagon`, two
  wow.export glTFs): the state's storm in its own smoke, blades of light and aura01's rising light, spread out to a low
  wall at the aura's edge with the middle clear, gold and see-through, no floor; flakes of light tumbling up through
  the aura (retail's flipping snowflake), embers and sparks, a crown of light above the head, a golden shockwave and
  flash as it turns on (90749); Seat's sigil and the footsteps' glyphs are ours (`client-patch/build_paragon_art.py`).

## Cards and Silas Darkmoon
Every Wildcard ability but the capstones (they stay the level 60 roll's, something to chase) is a **Skill Card**, and
every talent a **Talent Card** (the talent at its top rank: one card, bought or opened, and it is complete), each
Common, Rare, Epic or Legendary (Skill Cards by their power: the fight-changing cooldowns, forms and summons
Legendary, Metamorphosis, Icy Veins, Power Infusion, Bloodlust and the like, and each spec's signature (Frost Strike,
Bloodthirst, Mortal Strike, Holy Shock...); strong cooldowns, defensives and talent
abilities Epic; abilities from level 30 Rare. Talent Cards the same way: Legendary the talents a spec is built
around (Hot Streak, The Art of War, Sudden Death...), the rest by their tree row: 0-2 Common, 3-5 Rare, deeper Epic.
The lists are in `src/cards.cpp`; a talent that is a Skill Card already is no Talent Card). The versions of one ability are one card: a talent's (Mangle (Cat) and (Bear)), a form's (Swipe), a
faction's (Bloodlust and Heroism). A card opened is the **account's**
forever, for every character on it (`titan_cards` in the auth database, `data/sql/db-auth/titan_cards.sql`;
`src/cards.cpp`). Slotting cards into Specializations, loadouts the level-up rolls follow, comes next.
- **Packs** (items: Skill Card Pack 910010, Talent Card Pack 910011, Golden Skill Card Pack 910012, Golden Talent
  Card Pack 910013) hold 5 cards of their kind: a rarity rolled for each (Common 70%, Rare 24%, Epic 5%, Legendary 1%),
  then a card of it, 3 times in 4 one suiting the Hero's role. Golden Packs hold Golden Cards only, one the account has
  no Golden copy of when it can.
- **Duplicates** (a card already owned) pay Emberspark back: 40, 120, 400 or 1,200 by rarity, 2,000 for
  a Golden Card. Every 10th duplicate of the account gives a free pack of the same kind, every 50th a Golden Pack of it
  and 2,000 Emberspark (`titan_card_tallies`).
- **Eyes of Unlearning** are the character's (its settings, `mod-titan-darkmoon`): one for every level from 8 to 60, four
  at the milestones (8, 10, 20, 30, 40, 50, 60), 74 in all, claimed in the window; past that two at a time for
  Emberspark, at level 60 only, on the climbing price (100, 125 ...). A prestige lets the levels be claimed again and
  starts the price over. Spending them (rerolling an ability, a talent or a card just opened) comes with the
  Specializations.
- **Silas Darkmoon** (creature 910110, "Cards & Specializations", beside the Keeper of the Ember;
  `data/sql/db-world/titan_darkmoon.sql`) opens the **Darkmoon window** (the `Emberforge_Darkmoon` addon, prefix `EFDM`;
  `/darkmoon` opens it anywhere to look):
  - **Specializations**, the first page: the loadouts (Specialization I, the others to come), the Eyes' leveling track
    and the Eyes for sale.
  - **Packs**: the duplicate tallies; the packs in the bags, opened here (the pack rips open and its five cards turn
    over; "Open all" opens up to 50 at once and lists the new cards) or from the bags; Skill and Talent Packs for
    Emberspark, at prices that climb with each purchase (1,000, 1,250, 1,500 ... 20,000, then 2,500 more each) and
    start over at a prestige; Skill Packs for 75 gold too (5 a week an account); Golden Packs for 20,000.
  - **Cards**: every card (by kind, rarity, not owned, Golden, a search) with what the account owns, any one but a
    Golden one bought at a fixed price: Common 500, Rare 5,000, Epic 12,000, Legendary 30,000.

  Its art (card frames and backs, packs, the border and background: `client-patch/darkmoon_art/`) is in the addon,
  `Art/`, made by `client-patch/build_darkmoon_art.py`.

## Emberspark
Project Emberforge's server currency, one balance per **account** (`titan_emberspark` in the auth database,
`data/sql/db-auth/titan_emberspark.sql`; `src/emberspark.cpp`), so the shop can sell and spend it too. Every change is
one atomic statement, logged in `titan_emberspark_log` with its reason. Earned by prestiges for now. GMs:
`.emberspark [amount]` adds to the selected player's (or their own) account, or shows its balance.


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

## Emberforged Hero

Every Hero wears the Emberforged Hero buff (90600, with two hidden halves 90601 and 90602:
`client-patch/patch_player.py`), put on at login and after a resurrection (`hero_emberforged` in
`src/hero_resources.cpp`). It makes every stat count whatever the Hero rolled. On top of the Paladin base class's own
Strength to 2 attack power and Agility to 1 ranged attack power:

- Agility: 2 attack power, and 2 more ranged attack power (3 in all);
- Intellect: 3 ranged attack power, 2 spell power (damage and healing);
- Spirit: 0.25 spell power (damage and healing).

## Dungeon Journal
`/journal` or `/dj` (the `Emberforge_Journal` addon, `client-patch/AddOns/`; server side `src/journal.cpp`, addon
whispers with prefix `EFDJ`), laid out as Cataclysm's: a home page per expansion (its name the title, Dungeons and
Raids tabs, a search box, a grid of tiles), then an open book per instance (left: its plate and details, creature
levels as the world database has them after the level squish, map, each difficulty with its size, queue levels and
dungeon ID; right, by the tabs on the book's edge: its bosses, each with its creature's model as portrait, or all
its loot), and a page per boss (left: its model, level, classification and health; right: its abilities or its
loot). Abilities are the spells of the boss's C++ script file (`titan_journal_abilities`, written by
`tools/journal/abilities.py`) and its SmartAI casts, shown with the client's own descriptions; loot is its creature
and reference loot, uncommon gear and up. The server sends every boss's creature query response with the list, so
the client can draw the models.

The art and texts are the Mists of Pandaria client's own Journal (`Emberforge_Journal/Data.lua` and `Art/`, written by
`tools/journal/mop_journal.py` from what `tools/extract/EmberforgeExtract.exe` takes out of that client): each
dungeon's tile art, picture and story, each boss's portrait and story, and its abilities as the Journal has them
(headings, icons, flags such as tank, heroic or interruptible, their numbers from the spell tables, 3.3.5a's where the
spell is there). The addon matches the server's boss names to the Journal's; a boss it has no entry for keeps its
model and the spells of its script. Raids that Journal does not have (Classic's, the Burning Crusade's, Wrath's) get
the dungeon finder's art; Deadmines and Shadowfang Keep, rebuilt by Cataclysm, keep only their art. The bosses the
Journal has no portrait for get head shots in its style (`Art/EF-BOSS-*`, `Headshots.lua`), rendered by
`tools/journal/portraits.py` from the 3.3.5a client's own models: a creature's model, or a humanoid's character model
with its baked skin, hair, beard, helmet and shoulders, posed at the start of its Stand animation.

## Level and power squish
The game ends on Classic's numbers: the new level 60 is today's level 60 in power (`MaxPlayerLevel = 60`). Classic
stays as it is, 1-60; the Burning Crusade and Wrath both squish into 40-60, beside Classic's own 40-60
(`data/sql/db-world/zz_titan_squish.sql`, written by `tools/squish/squish.py`, which says how):

| Old levels | New levels | Content |
|---|---|---|
| 1-60 | 1-60 | Classic (unchanged) |
| 58-70 | 40-60 | Burning Crusade |
| 68-80 | 40-60 | Wrath |

- **Items** (weapons and armor): Classic's unchanged; the others' required level by that map, their item level onto
  Classic's curve there (a level 60 green is item level 65 whatever its expansion), stats and block by the stat budget ratio, armor and weapon
  damage by the item level ratio; random "of the ..." stats follow the item level.
  Items with no required level (many quest rewards: the quest has it) get one from where they come from: the
  lowest of the quests that reward them (a Burning Crusade or Wrath quest's level: Icecrown's rewards land at 57-60;
  a Classic quest's minimum level), else, past Classic's item levels, where their expansion's curve reaches their item
  level. They are then squished as the others; Classic's keep their power and get the required level.
- **Creatures**: their level by the map (the level decides the band, their expansion only where bands overlap), and
  Classic's stats rows at the new level: a Northrend mob at 55 is as strong as a Classic level 55 mob.
- **Quests**: their levels by the map (where bands overlap, by their zone's expansion).

It keeps every original value in `titan_squish_*_original` the first time and always computes from those, so it can
run any number of times. After an update puts original values back (a core update, another module's SQL), bump
`VERSION` in the script and regenerate: the updater runs a changed file again. Not squished yet: spells (ranks above
60 are simply out of reach; Death Knight abilities learned above 60 too), creature spells with fixed damage, items
with no required level, and dungeon level ranges (with the dungeon scaling).

## Dungeon scaling
Every 5-player dungeon is fought at every Hero's own level, Dragonflight's way (`src/dungeon_scaling.cpp`; it
replaced mod-autobalance):
- **The creatures are at the group's lowest level**, bosses 2 over (`BOSS_LEVELS`), with the core's own stats for that
  level (`Creature::SelectLevel`, through `OnBeforeCreatureSelectLevel`: health, mana, armor, melee damage). A creature
  gets its level as it spawns or respawns; whenever a player enters, every creature out of combat is set again to the
  lowest level of those in the dungeon. Their spells carry their native level's amounts (the template's level), so
  damage (their own buffs' too: Lightning Shield, damage shields) is scaled by their class damage at the level they
  are at over the native one, healing by their health; spells by a share of health stay. A dungeon is as strong as
  for 5 players, whoever comes. It follows the lowest level as players level up too (creatures out of combat).
- **Each player sees the creatures at their own level**: their level, health and maximum health are patched per
  receiver (`OnPatchValuesUpdate`): the level the player meets them at, and the health their damage takes to empty
  (a level 60 with a level 25 sees level 60 creatures with a level 60's health). Sent again when someone levels.
  Combat text still shows the damage actually dealt (smaller than the health bar's drop for the higher player).
- **Boss mechanics players deal each other** (a spell they don't know: Krystallus' Shatter, cast by each stoned
  player on those around) scale from the dungeon's level (its Dungeon Finder target level) to the one hit's, with the
  ease. Triggers and creatures more than 10 levels from the dungeon's level (invisible stalkers, often level 1 or 80)
  have their spells scaled from the dungeon's level, not their own.
- **Creature damage multipliers are capped** at 3 (bosses 5: `MAX_DAMAGE_MODIFIER`, `MAX_BOSS_DAMAGE_MODIFIER`), melee
  and spells alike: each expansion tuned its own `DamageModifier` (dungeon elites: Classic's about 4.6, Wrath's 7.5 to
  15), so a level 20 pack of Wrath trash hit for hundreds.
- **Every player meets the creatures at their own level**: a creature counts as at the player's level, with its offset
  from the group's lowest. The damage a player (and their pets) deals is scaled by the creature's health at its level
  over its health at that one, the damage they take by its damage there over its own (`creature_classlevelstats`, its
  class): a level 45 with a level 12 deals about 16% and takes about 4 times as much, as against level 45 creatures.
  Heals between players of different levels go by their health the same way.
- **Easier the lower the Hero**: at level 10 (and below) creatures have half their health (the Hero's damage doubled)
  and deal 75% of their damage, evenly up to the intended difficulty at level 60. Heroics are level 60: as intended.
- **Experience** for a kill is each member's own: what `Acore::XP::Gain` gives for the creature at their level, split
  evenly over the group's members in range (the core's, nothing for a creature gray to them, is replaced). Prestige's
  bonus and experience auras apply.
- Not converted: absorb shields across levels (a level 45's shield on a level 12 is a level 45's; a creature's absorb
  shields keep their native amounts), and the level
  difference in hit, dodge and crit chances (the higher players fight creatures below them: a small edge).

## Core changes this module relies on
- `LFGMgr::LoadLFGDungeons`, `DungeonFinder.MinLevel` and `DungeonFinder.OneRandom` (`docker-compose.yml`): every
  normal dungeon can be queued for from level 10 to the cap and every heroic at 60, in one Random Dungeon and one
  Random Heroic Dungeon (raids keep their levels; the retired dungeons stay closed); `zzz_titan_dungeon_finder.sql`
  opens their entrances the same, with no gear score. Both factions queue together (`AllowTwoSide.Interaction.Group`).
- Both factions as one: chat, channels and groups across factions are settings (`AllowTwoSide.Interaction.*`,
  `docker-compose.yml`); mail, `/who`, friends and trade across factions are RBAC permissions, which the stock data
  gives only to staff, so `data/sql/db-auth/titan_two_side.sql` links them (27, 28, 29, 51) to the Player role (199).
  The client's own level filters are lifted by `Emberforge_Journal/MicroMenu.lua`. Finishing a random dungeon gives
  100 Emberspark (500 for a heroic), and its experience is a quarter of what the Hero's level takes, times
  `Rate.XP.Quest`
  (`src/dungeon_rewards.cpp`; `LFGHandler.cpp` shows the dungeon finder that amount, asking the quest experience
  hook). Every other quest gives 3 times its experience on turn-in, on top of `Rate.XP.Quest` (5), shown so in the
  quest window too (`QUEST_XP_MULTIPLIER`, same file).
  The dungeon finder gives no Deserter (`DungeonFinder.CastDeserter` 0, `docker-compose.yml`) and no Random Dungeon
  Cooldown (`LFGMgr.cpp`): the next random can be queued at once (both auras, still given elsewhere, last 5 minutes:
  same file). A teleport in refused for the player's state (in combat as the group formed, dead, falling) is tried
  again every 2 sec for 2 min; back in a dungeon finder group after a relog or a restart, the client is told again
  (its eye offers the teleport in: `LFGScripts.cpp`), and such a group may walk into any dungeon (`MapMgr.cpp`).
- `Player::SetRestBonus(float, bool allowAboveCap)`: lets items grant rested experience above
  the cap, and keeps natural resting from trimming it back down.
- `Spell::TakeRunePower`: runes give Rage instead of Runic Power (Emberforge has none; see Demon Hunter).
- Universal combo points (`Unit::RetargetComboPoints`, `Unit::AddComboPoints`, `Unit::ClearComboPoints`,
  `Unit::ClearComboPointHolders`, `Player::SetSelection`): a player's combo points are their own, not their
  target's. Gaining points on a new target adds to them; changing target moves them onto it (shown there, its
  finishers using them); the target dying keeps them for the next one. Pets and creatures are unchanged.
