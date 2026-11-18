# Changelog

Player-facing changes to Emberforge, newest first. Each entry is also posted as a patch note on the website.

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

- Rune of Ember's Wrath (Cat Form, Fire Blast, Rake or Rip, Melee): Fire Blast becomes usable in Cat Form for 30
  Energy (6 sec cooldown), with your Fire Blast's damage; the rune puts it on your Cat Form bar. On an enemy bleeding
  from your own Rake or Rip it ignites Cinder Wound: 30% of your attack power plus 30% of your spell power as Fire
  damage over 6 sec.
- Rune of Whitefang (Cat Form, Icy Touch, Shred or Mangle, Melee): Icy Touch becomes usable in Cat Form for 25 Energy
  and no runes, Frost Fever and all. It lays Frozen Prey; your next Shred or Mangle on that enemy shatters it for 30%
  of your attack power as Frost damage.
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
