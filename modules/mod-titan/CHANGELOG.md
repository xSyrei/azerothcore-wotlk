# Changelog

Player-facing changes to Emberforge, newest first. Each entry is also posted as a patch note on the website.

## 2026-11-16: Bug fixes

### Combat and classes

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
