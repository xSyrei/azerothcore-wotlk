# Changelog

Player-facing changes to Emberforge, newest first. Each entry is also posted as a patch note on the website.

## 2026-11-23: Card slots

### Legendary Rune panel

- The rune panel **hides the rest of the interface** while it is open, and opens only out of combat (asked for in a
  fight, it opens when the fight ends; a fight starting closes it). Escape or Alt+Z closes it.
- With nothing to suggest, it opens straight to the **search**.
- Its suggestions count the **Skill Cards you have slotted** too: a rune your cards will complete is suggested, and
  says "Your cards bring ..." for what they will give you.
- **Unlearn** a bound rune in the panel **until level 8**, and choose another.
- **Learn Available**: while none of the runes is usable, a rune whose abilities you can get shows Learn Available.
  It learns each ability you can learn now with your kit picks, and slots the Skill Card of each you can't yet. Before
  anything changes it lists what it will do and, in red, what it replaces (an ability in your kit, a card in a slot).
  It works until level 8, when your abilities and cards lock.

### Eyes of Unlearning

- **Leveling Eyes you hadn't claimed are kept when you prestige**: claim them at Silas Darkmoon afterwards, with the
  new ones. (Eyes lost to a prestige before this patch can't be restored.)
- The Specializations page shows its card slots as an even row: each card's icon in its rarity's color, its Golden
  Card in the corner, and the level it comes at beneath.

### Card slots

- Each specialization has **8 card slots**. A slot holds one of your cards (a Skill or Talent Card) and one Golden
  Card.
- On Silas Darkmoon's Specializations page, **Edit Cards** opens the card editor: your 8 slots above every card you
  own, with Abilities and Talents side by side and one search bar for both. The Golden tab shows your Golden Cards.
- Click a slot to choose it, then click a card to put it there; the next empty slot is chosen for you. Right-click a
  slot to empty it. A card is in one slot at a time: slot it again and it moves.
- **Your slotted cards are your level-ups.** Every card shows the level it unlocks at. Each level-up that gives an
  ability (levels 9, 11, 13...) gives your next slotted Skill Card unlocked by then instead of a random one, and each
  one that gives a talent (8, 10, 12...) your next Talent Card. Several unlocked at once come one after another: three
  Skill Cards unlocked at 9 arrive at 9, 11 and 13. Golden Cards count too. Each slot shows the level its card comes
  at.
- Cards **lock at level 8**: from then they can't be swapped until you prestige. A specialization you haven't built
  yet stays open until the prestige that builds it.
- An ability that needs a form, stance or pet you don't have yet waits until you do.
- Each specialization keeps its own cards, and they stay when you prestige.
- The card editor's new look: each slot shows its card's icon and name and the level it comes at, its Golden Card a
  gold badge on the icon's corner with its level and name beneath; the lists show rarity as gems and the unlock level beside each card, with scroll bars, and sort by
  rarity or by level.
- **Search by what a card does**: the search bar (the editor's and the Cards tab's) looks through each card's text
  too. Every word you type must be in it, in any order: "% spell haste", "shadow damage", "stun", "heal over time". Put
  a phrase in quotes to find it exactly.
- **Filters** in the card editor: Skill Cards by **Role** (Melee, Ranged, Healer, Tank, or Utility for those every role
  uses) and by **School** (Physical, Holy, Fire, Nature, Frost, Shadow, Arcane), and both lists by **Rarity**. The
  lists count what is shown ("37 of 443").

## 2026-11-19: The Duelist's Cannon-Fire

- Rune of the Duelist: **Rapid Shots** replaces Piercing Round. Each combo point your finishing moves spend has a
  10% chance to reset Pistol Shot's cooldown and ready Opening Shot; your next Pistol Shot within 10 sec then grants
  50 Energy instead of costing it.
- The third slot, Footwork, is now **Cannon-Fire**, with Broadside modifiers: Heavy Cannonade (Broadside's blasts
  deal 40% more damage), Full Broadside (a fourth blast) and Quick Reload (each combo point your finishing moves
  spend takes 1 sec off Broadside's cooldown). Advance, Disengaging Shot and Press the Advantage are gone; a choice
  in that slot carries over to the new modifier in its place.
- The Fencer build is now The Quickdraw: Serrated Edge, Rapid Shots and Quick Reload.

### ElvUI

- Heroes playing with ElvUI see all their resources: Rage and Energy bars and the six runes under ElvUI's player
  frame, in ElvUI's look (move them with /moveui, "Hero Resources"), and their combo points on ElvUI's target
  frame combo bar.
- ElvUI's config (/ec) has an Emberforge page: the Hero Resources bars (width, heights, spacing, text and its size,
  which resources show), the target frame's combo bar (detach it to move it on its own), and combo points on the
  target's nameplate. Move frames opens ElvUI's movers.

### Dungeon loot

- Dungeons drop **their own items at your level** again: a dungeon's blues and epics (Vendetta, Butcher's Cleaver...)
  drop as that same item made for the party's level (its lowest Hero's), up or down, its stats, armor and damage
  scaled to it. They are 15 item levels above what that level's gear is (a level 20's dungeon piece: item level
  40). Only the world's random greens that also drop in dungeons still become Ember gear. Bonuses an item
  gives through an effect (spell power, hit, procs) keep their amounts.

### Dungeon bosses

- Bosses no longer take less damage from Heroes: their extra health (4.5 times) is gone, and they show their real
  health.
- Every dungeon boss kill gives 75% of your level's experience, whichever boss: some gave a level's worth, others
  barely a trash pack's. Optional bosses count too.
- Bosses (and what they summon) deal 30% more damage to tanks and 15% less to everyone else. A boss mechanic one
  player deals to those around them (a pulse from a tank's debuff) always deals the reduced amount.

### Tuning

- Rune of the Duelist: Broadside's blasts now deal 30% of your attack power plus 5% per combo point each (55% at
  five, was 150%). Its tooltip shows each combo point's exact damage.
- Flamestrike deals 15% more damage (its blast and its burning ground).
- Every Feral ability (Cat and Bear: Shred, Mangle, Rake, Rip, Ferocious Bite, Swipe, Maul, Lacerate...) deals 50%
  more damage, on top of its earlier 25%.
- Spells a Hero has below their level (Greater Heal, Mind Blast, Consecration, Stormstrike and every other ability
  Wildcard gives from level 1) are scaled to the Hero's level: (level + 2) / (spell level + 2) of their damage and
  healing. A level 10 Hero's Greater Heal (a level 40 spell) heals 29% of its tooltip, a level 30's 76%; from the
  spell's own level, all of it.

### Fixes

- Smoldering Adventurer's Caches fill your hands for your role: a Tank gets a one-hand weapon and a shield, a Melee
  Hero a weapon in each hand (none in the off hand without dual wielding), a Ranged Hero an off-hand book, a Healer
  a book or a shield. Tanks rarely got shields (off-hand weapons and two-handers scored as well) and Melee Heroes
  often did.
- Riding needs no level past 60: Artisan Riding and Cold Weather Flying, epic flying mounts and every other mount or
  riding item that asked for 68 to 80 now ask for 60 (their riding skill still applies).
- Dungeon bosses always drop at least one piece of gear (their own items first). World gear that would become an
  Ember piece in a dungeon does so far less often (30% of the time; otherwise it does not drop).
- Bosses deal 2.25 times their damage to tanks (125% more; it was 30% more).
- Rune of Red Communion: Blood Boil costs 20 Rage and has a 6 sec cooldown instead of its Blood rune. While the
  rune is bound, your Blood Boil buttons become the rune's own Blood Boil, so it no longer says "Not enough runes".
- Rune of the Black Covenant: Voidforged Plate now gives 2,500 armor at level 60 (a share of it at lower levels) on
  top of Covenant Armor, instead of raising its armor from items to 500%.
- Blood Boil no longer needs Icy Touch or Plague Strike to be offered by Wildcard.
- You can heal (and buff) groupmates of the other faction in dungeons and raids: inside an instance, a mixed group
  wears its leader's faction, so the game sees friends. Outside you are your own faction again.
- Healing between Heroes of different levels in a dungeon works the right way round: a heal on a higher-level Hero
  is made larger for their bigger health, not smaller (the game handed every direct heal's healer and target the
  wrong way round to the scaling).
- Creatures notice you from 30% less far.
- The Legendary Rune window's modifier cards show only the modifier's name, without the small icon beside it.
- Orc Heroes have Blood Fury (attack power and spell power), and Night Elf Heroes Elusiveness: the game kept them
  from both, since it treats Heroes as Paladins.

### Tank Legendary Runes

Three new Legendary Runes for tanks, from the Tank Collection. Each also stops NPC attacks from critically striking
you; Red Communion and the Primal Worldwarden also teach Challenge, a free 30-yard taunt (8 sec cooldown, off the
global cooldown).

- **Rune of Red Communion** (Blood Boil): Blood Boil links up to 5 enemies it hits to you for 10 sec. Blood Strike on
  a linked enemy heals you (1.5% of your maximum health, more for each other link) and pulses Shadow damage to the
  rest. Sanguine Recall strikes every link and shields you. Modifiers: Sovereign Bond, Bloodbound Court, Long Veins;
  Crimson Feast, Arterial Echo, Shared Weakness; Coagulated Throne, Rapid Recall, Blood for Blood.
- **Rune of the Primal Worldwarden** (Earth, Flame or Frost Shock): a Worldheart Totem follows you and, with a shield
  on, gives you Worldguard (8% less damage taken, 200% more threat). Earth Shock commands an absorb, Flame Shock a Fire
  burst around the totem, Frost Shock a slow that weakens enemies; Stormstrike repeats your last command, Lava Lash
  draws a magma line to the totem, and Place Worldheart anchors it. Modifiers: Earthen Command, Flame Dominion,
  Winter Command; Deepstone Heart, Wide Communion, Striding Heart; Resonant Strikes, Molten Causeway, Stoneblood Lash.
- **Rune of the Black Covenant** (Summon Voidwalker and Searing Pain): Summon Voidwalker becomes **Bind:
  Voidwalker**, which binds a Voidwalker to you as Covenant Armor (more armor, health and threat, less damage taken,
  and a Voidwalker's purple shadow about you). While bound, Searing Pain becomes **Covenant Lash** (an instant lash
  for 100% of your spell power as Shadowflame damage, with extra threat; 8% of base mana) and, if you know Drain Life,
  Drain Life becomes **Soul Siphon** (a channel you can move in that heals you; 15 sec cooldown). Dark Intercession is
  your defensive, and your Curses of Weakness and Tongues weaken their victims' hits on you. Your taunt comes from
  Wildcard. Modifiers: Voidforged Plate, Nether Insulation, Hungry Covenant; Deep Extraction (more Siphon healing),
  Ravenous Siphon (more Siphon damage), Lashing Rhythm (each Covenant Lash takes 1 sec off Soul Siphon); Guardian
  Manifest, Legion's Rebuke, Frequent Petition.
- The Worldheart Totem and Place Worldheart are in the Shaman tab of the spellbook, Sanguine Recall in the Death
  Knight's, the Black Covenant's abilities in the Warlock's.

### Graphics engine

- The engine now works on AMD and Intel graphics cards, not only NVIDIA: its passes drew nothing, or garbage, there.
- The engine turns the game's anti-aliasing (Multisampling) off: with it on, the water and the atmosphere could not
  run (the engine cannot read a multisampled depth buffer). The engine panel (Home) can keep it, without them.
- The scene's lamps are calmer and steady: they no longer flicker or jump as the camera moves, a roof or wall lit by
  the moon or a lamp no longer glows as if it were a lamp itself, and lamps light their surroundings more softly
  (never blinding, however many or near). Lamps leaving the screen fade out instead of popping.

### Talent glyphs

- A talent's glyph is now part of the talent: knowing Vigor gives Glyph of Vigor's 10 extra maximum Energy, Aimed
  Shot has Glyph of Aimed Shot's shorter cooldown, and so on for 75 talents of every class. Their tooltips say so.
  Glyphs that trade something away (Insect Swarm, Avenger's Shield, Blast Wave, Thunderstorm, Typhoon) are left out.

### Fixes

- Combo points no longer vanish a few seconds after an enemy dodges. A dodge readied Overpower by granting a combo
  point and then cleared all of them; it now readies Overpower on its own, and Overpower no longer spends combo
  points.
- Rune of the Shattered Sun: a Frostbolt (or any frost spell) that kills a target holding Sunshards still shatters
  them.
- Rune of the Shattered Sun, Spreading Sun: a Fireball or Pyroblast that kills its target still spreads Sunshards to
  2 enemies nearby.

## 2026-11-16: Legendary Rune balance and bug fixes

### Three new Legendary Runes

- Rune of Elune's Guardian (Starfire, Ranged): every Starfire that hits calls a lunar owl. A second later it dives
  on the same enemy for 30% of the Starfire's damage. Up to 3 wait at once.
- Rune of Astral Convergence (Wrath and Starfire, Ranged): Wrath leaves Sunlight on its target. Starfire on it
  collides for 40% of its damage and leaves Moonlight; your next Wrath on that flares for 80% of its damage.
- Elune's Guardian's Lunar Wound now deals damage: as much again as the owl's strike, over 6 sec. The runes'
  debuffs (Lunar Wound, Sunlight, Moonlight and the rest) can no longer miss or be resisted.
- Rune of Ursoc's Champion (Bear Form, Barkskin and Maul, Tank): Barkskin in Bear Form raises Ursoc's Guard, an
  absorb of 30% of your maximum health for 6 sec, and comes back 30 sec sooner; each Maul in Bear Form brings it
  back 3 sec sooner still. Maul releases the guard as a blow in front of you: 75% of your attack power plus all the
  guard absorbed (up to 250% of your attack power more).
- Each has three slots of three options, like the others.

### The Duelist looks and sounds new

- Sabre Slash's hit is new: a gold slash flashing across the target, a spray of sparks through it and a soft glow,
  with its own slash sound. The first effect made in the Emberforge Effect Suite.
- Broadside fires cannonballs: each blast lobs one at every enemy it hits, bursting in rings of fire with smoke,
  sparks and a scorch on the ground, and a cannon's boom.

### Two feral Legendary Runes

- Rune of Ember's Wrath (Cat Form, Fire Blast, Rake or Rip, Melee): your own Fire Blast works in Cat Form, at its
  own mana cost and cooldown. On an enemy bleeding from your own Rake or Rip it ignites Cinder Wound: 150% of your
  attack power plus 100% of your spell power as Fire damage over 6 sec.
- Rune of Whitefang (Cat Form, Icy Touch, Shred or Mangle, Melee): your own Icy Touch, in Cat Form too (its Frost
  rune and Frost Fever as ever), lays Frozen Prey; your next Shred or Mangle on that enemy shatters it for 150% of
  your attack power as Frost damage.
- Flame Shock (Ember's Wrath) and Frost Shock (Whitefang) work in Cat Form too, and empower your next Claw or Swipe
  within 10 sec. Flame Shock: Claw becomes Blazing Strike (its hit again on a second enemy, and both bleed: a bleed
  Fire Blast ignites); Swipe becomes Fiery Explosion (Fire damage to up to 8 enemies around you, each left burning
  with half a Cinder Wound). Frost Shock: Claw becomes Frostfang Strike (a second enemy, both frostbitten and
  slowed); Swipe becomes Glacial Burst (Frost damage to up to 8 enemies, slowed 50%, every Frozen Prey among them
  shattered).
- Blazing Bleed burns: flames stream from the bleeding enemy for as long as it lasts.
- Fiery Explosion and Glacial Burst look new: flames (or frost) burst on every enemy the empowered Swipe hits, and
  sparks fly from the cat, with an impact sound.
- The spells these runes reach into say so on their tooltips: Flame Shock, Fire Blast, Claw, Swipe, Rake and Rip
  with Ember's Wrath bound; Frost Shock, Icy Touch, Claw, Swipe, Shred and Mangle with Whitefang.
- With either rune bound, your Cat Form takes a new look: a Flame Cat wreathed in fire (Ember's Wrath) or an Ice
  Cat with frost wisps (Whitefang).
- Each has three slots of three options, like the others.

### Caster balance

- Ranged and Healer's spell haste is 60% (was 110%): cast times are 37.5% shorter than usual instead of 52% (a 3 sec
  Fireball casts in 1.9 sec).
- Mana costs are a little higher: 35% of the usual (was 30%).

### Fixes

- The training dummies in the starting zones can no longer be pushed around by knockbacks.
- Hunter abilities and Auto Shot work again with a bow, gun or crossbow: they failed with "no ammo". The arrows or
  bullets of your level now sit in your ammo slot (a stack in your bags) and never run out.

### Two more Legendary Runes

- Rune of the Duelist (Sinister Strike, Melee): Sinister Strike becomes Sabre Slash (its own icon and slash; your
  Sinister Strike buttons change to it). Every second Sabre Slash that hits readies Opening Shot for Pistol Shot (20
  Energy, 6 sec cooldown, 20 yards): a conjured pistol shot for 60% of your attack power and a combo point, no ranged
  weapon needed. Broadside, a finishing move (35 Energy, 20 sec cooldown): three exploding pistol blasts, each 30%
  of your attack power per combo point to the target and up to 7 enemies around it. All three are in the Rogue tab
  of your spellbook.
- Rune of the Stormreaver (Stormstrike, Lava Lash and Lightning Shield, Melee): Stormstrike raises a Stormfront
  around you for 6 sec that strikes up to 5 enemies every 2 sec for 25% of your attack power. In it, Lava Lash
  becomes Thunderlash (60% of your attack power more, arcing to 2 more enemies), and your melee abilities discharge
  Lightning Shield once a second without using its charges.

### Rune of Astral Horizon

- The Orb flies to its target much faster, then slows to a drift once it reaches an enemy (whatever the options).
- It hovers above the ground instead of sinking into it, and looks 25% bigger.
- Stronger options:
  - Wandering Star: the enemies it passes through take 2 bolts' damage, up from 1.
  - Event Horizon: bolts deal 50% more, up from 20%.
  - Satellite: lasts 8 sec (up from 6) and reaches 15 yards (up from 12).
  - Astral Focus: bolts on the Orb's target deal 60% more, up from 40%.
  - Constellation: 75% of the bolt to the others, up from 50%.
  - Prismatic Lance: 80% of the bolt, up from 60%.
  - Supernova: 6 bolts' damage, up from 4.
  - Dying Light: 3 bolts' damage each, up from 2.
  - Astral Remnant: strikes every second, up from every 2.

### A second Legendary Rune

- From level 30, every specialization gets a second Legendary Rune, at full power with its own three options. The
  rune panel's medallion is split between your two runes: click a half to see that rune. The panel opens on your
  second rune when you reach 30.
- The two runes can't be the same rune. `.unbindrune 1` or `.unbindrune 2` lets go of one; `.unbindrune` of both.

### Five new Legendary Runes

- Rune of Arcane Mind (Arcane Blast and Arcane Missiles): Arcane Blast builds Arcane Insight. At 4, your next Arcane
  Missiles is free and fires 5 Insight missiles with it, each 40% of a missile's damage.
- Rune of the Splintered Crown (Frostbolt): every Frostbolt that hits forms an Icicle above you; a second later it
  strikes the same enemy for 30% of the Frostbolt's damage. Up to 5 wait at once.
- Rune of Phoenix Wake (Pyroblast and Fire Blast): Pyroblast leaves a Phoenix Ember on its target. Fire Blast on it
  calls a phoenix that dives from above you onto it, burning the enemies under its path, and strikes it for 50% of
  the Pyroblast's damage.
- Rune of Astral Horizon (Arcane Barrage): Arcane Barrage sets an Arcane Orb loose (every 12 sec at most). For 6
  sec it drifts on and fires a bolt every second for 20% of the Barrage's damage.
- Rune of Guiding Light (Prayer of Mending, healers): Prayer of Mending places a Guiding Star that heals its bearer
  every 2 sec for 12 sec; Flash Heal or Greater Heal on another ally sends it there.
- Each has three slots of three options, like the others.
- Their looks: modern WoW's Icicles for the Splintered Crown, the Arcane Orb for Astral Horizon, and the Phoenix
  Flames phoenix. The Icicles and the phoenix both launch from above you down to their target.

### Legendary Runes balance

- Rune of Echoing Thunder: Thunder Echo deals 40% more damage (42% of Shield Slam's damage, up from 30%).
- Rune of the Unbroken Onslaught: the wake deals 60% more damage (96% of a weapon swing, up from 60%; its bleed and
  Aftershock grow with it).
  - Overpowering Echo (redesigned): the wake rips open twice, once for each of your next two melee abilities.
  - Battlefield Claim (redesigned): enemies the wake hits are claimed for 8 sec, slowed by 30% and taking 10% more
    damage from you.
  - New look for the Weapon Wake: a burning red fissure on the ground, and a flashier rip (a crossed slash, steel
    shards, blood and a shockwave).
- Rune of the Spectral Blade: the phantom strikes for 200% of Eviscerate's damage (up from 50%), and each phantom
  strike makes your Energy regenerate 100% faster for 4 sec (Phantom Rush).
- Every Legendary Rune option now says exactly what it does, with its numbers.
- New looks for Echoing Thunder, the Shattered Sun and the Spectral Blade, made from the game's own spell effects:
  a storm cloud over a charged warrior, Living Bomb's burning Sunshards and explosion, Flamestrike's burning ground,
  and a real Phantom Assassin: your shadowy double steps out of the shadows behind your target, strikes and vanishes.
- Uncommon, Rare and Epic runes are turned off for now: only the Legendary Runes are in play. Runes you already hold
  are kept for when they return.
- Rune of Fel Commandment, Rune of the Threefold Grave and Rune of the Unbroken Onslaught are temporarily removed
  while their designs are reworked. If you had one bound, it is released when you log in: choose another in the rune panel.
- Rune debuffs show their real kind: Bloodletting, Open Wound and Ghostfang are Bleeds, Grave Pestilence is a
  Disease, the rest are Magic. They all showed as Poisons before, and counted as slows.

### Combat and classes

- Energy regenerates 25% faster.
- Sinister Strike costs 30 Energy, down from 45.

- Enemy casters keep casting after an interrupt. A kicked caster used to run into melee and stay there; it now goes
  back to range once its spells are usable again (dungeons and the open world).
- Hunters no longer need ammo. Bows, guns and crossbows shoot without arrows or bullets, and every shot carries the
  damage of the ammo for your level. Leftover ammo is removed from your bags, and vendors no longer sell it.
- Combo points no longer build up on their own. Honor Among Thieves now counts only your party's critical strikes,
  not your own (a Hero crits with every class's spells, so it filled your combo points by itself).
- Death Knight abilities dim when you run out of runes.
- Holy Light and Seal of Righteousness can be picked again, and they stay after you log out.
- Shattered Sun: each player's shards are their own and no longer replace another player's.
- Warrior stances can be left: cast the stance you are in again (its buff is hidden, so there was no way out).

### Pets

- Tame Beast now comes with Revive Pet (from level 1).
- Fixed being stuck without a pet after a prestige: your tamed pet comes back as your current pet, so Call Pet works
  and Tame Beast is no longer refused with "too many pets".

### Specializations

- Changing specialization keeps the buffs, pet and totems that both specializations share. Anything the new
  specialization doesn't have is removed.

### Items and vendors

- Dungeon drops now match your level. Gear above the party's level (the lowest player's) drops as a piece of the
  same slot and armor or weapon type at that level, so you can wear it right away.
- Food and drink vendors only show what you can use at your level. Burning Crusade and Wrath vendors also sell the
  standard bread and water.

### Wildcard

- Talents that work with a resource (Energy, Rage, mana, combo points, a pet's Focus) are only rolled for Heroes who
  use it: no more Energy talents on a caster.
- Travel Form, Aquatic Form and Flight Form are no longer rolled.
- Buffs, auras and totems for one kind of fighting now count for the roles they serve: Devotion Aura is no longer
  rolled for a Ranged Hero.

### Interface

- The Talents button opens the Talents list.
- The spellbook keeps working after learning abilities or swapping Legendary Runes (the rune addon kept refreshing
  it, which left its buttons unusable).
- Throw is no longer put on your action bar at every login and prestige.
