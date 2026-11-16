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

### Interface

- The Talents button opens the Talents list.

### Still under investigation

- Legendary Rune window errors.
- Battle Stance can't be left once it's on.
- Throw Glaive is put on the action bar at login and after a prestige.
- Wildcard sometimes rolls talents or abilities your build gets nothing from.
