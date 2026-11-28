# Changelog

Player-facing changes to Emberforge, newest first. Each entry is also posted as a patch note on the website.

## 2026-11-27: The Firepit Ignites

- **A new Firepit loading screen**: the arena's gate opening on a packed, burning ring.
- **The cache is the only loot**: enemies and bosses in the Firepit no longer drop items, coin or skins; everything
  you earn waits in the end-of-run cache.
- **A new end-of-run cache**: the Spoils of the Firepit now come in a grand challenge chest, which opens as you
  loot it.
- **Potential Rewards, corrected**: the Firepit panel now shows the real rewards with fitting icons: the Spoils
  chest, Rare and Epic Fireforged gear, and Titanforged (the leaderboard isn't a reward, so it's gone from there).
- **The bosses speak**: every boss has its own words as it enters the Ring and its own last words as it falls (Grizzle,
  Lord Roccor, Bael'Gar, Emperor Thaurissan and the rest), and the five major bosses also call out their phases and
  their enrage.
- **Clear floor warnings**: every attack's area now shows on the arena floor as a glowing decal, a faint fill with a
  bright rim, with an inner fill that grows to the rim as the blow comes (it lands when it reaches the edge). Circles,
  rings, cones, lines and the arena's halves and thirds all have their own shape, and the decals draw over the floor
  and props so nothing hides them. They come in six colours for what an area means: red-orange to get out of, blue to
  stand in (the Ring of Embers' safe middle), gold to share, green for helpful areas, violet for special mechanics
  and icy blue for frozen ground.
- **Ladder bosses' attacks restored**: from wave 15, the ladder's bosses use their eruptions and novas again (adding
  their dialogue had switched them off).
- **Enemy Forces tracker**: the tracker shows the living enemies by role (Melee, Ranged, Healers, Boss), the
  reinforcements waiting (INCOMING when the field is empty but more are coming) and a badge for Bolstered or Mending
  enemies.
- **Fewer specials at once**: across overlapping waves, at most 1 healer alive to wave 15 (2 after) and 2 ranged enemies
  to wave 15 (3 to wave 30, 4 after); extra picks become frontliners.
- **The stands are full**: a crowd of the dungeons' creatures watches from the seats, cheering each wave, roaring as
  bosses enter and fall, getting louder in Carnage's last 30 seconds, and laughing at a wipe. Purely cosmetic.
- **Fireforged gear, reforged**: the spoils of waves 15 and 30 are **Rare**; from wave 45 they are **Epic**, each with
  exactly **one bonus effect** chosen for your role and build (never one you can't use: no block bonus without a shield,
  no mana bonus without mana).
- **58 bonus effects** across Tank, Healer, Melee, Ranged and Hybrid pools: steady stats (spell power, attack power,
  ratings, block value, armor, regeneration), small percentage boosts (direct heals, heals over time, melee abilities,
  auto-attacks, direct spells, ranged abilities, spells over time) and procs (15% chance, 30 sec apart: bursts of power,
  haste, crit, dodge or defense, absorb shields, mana, a Thundering Strike). Every number scales with the piece's item
  level and shows on its tooltip.
- **Titanforged** pieces now carry a **prismatic socket** instead of doubled stats. Old Fireforged pieces keep what they
  have; the old famous-proc modifiers no longer roll.
- **Marked for Carnage**: at waves 45, 60, 75, 90, 100, 105 and every 15 after, the boss must die within 5 minutes or
  the arena detonates. The tracker shows the clock, with warnings at 2:00, 1:00, 0:30 and a final countdown.
- **Five new major bosses**, each with telegraphed attacks and health phases:
  - **Wave 45, the Ashfleet Captain**: Powder Lines and Cutlass Sweeps; at 70% and 35% a Trial by Fire (cannon volleys
    while it takes half damage); a Final Stand at 30%.
  - **Wave 60, the Ironbound Colossus**: Ironbreaker cones and Molten Ruptures; Shatter the Core at 70% and 35% leaves
    its core exposed (20% more damage taken); its forge grows unstable at 25%.
  - **Wave 75, the Cinder Matron**: Scorching Spit, Rending Mandibles and Cinderweb lines held by Web Anchors you can
    break; Webstorm at 65% and 30% takes her out of reach.
  - **Wave 90, the Furnace Warden**: Warden's Crush, Furnace Vents and Divided Furnace (half the arena burns,
    alternating sides); Furnace Cycle at 70% and 35%; Meltdown at 25%.
  - **Wave 100, the Firepit Champion**: cleaves, Blazing Shots and Challenge the Crowd; changes weapons at 75%, 50% and
    25% (Ironbreaker lines, the Ashen Glaive's ring, the Executioner's arc); at 10% the arena's edge ignites.
- After wave 105 the five return in turn on every Carnage wave.
- **World affixes**: the arena itself changes as you climb. A new affix enters at waves 10, 20, 30 and 40; from wave 50,
  every 10 waves the oldest gives way to a new one (four at most). The order is the same for everyone each week, and the
  tracker lists the active ones.
  - **Molten Ground**: every 20 sec the floor erupts under up to 2 Heroes (12% of max health within 4 yards) and leaves
    a burning pool for 8 sec.
  - **Cinder Winds**: every 35 sec a burning wind sweeps a lane across the arena after a 5 sec warning: 10% and slowed
    by 30% for 3 sec. Step out of the lane.
  - **Searing Brand**: every 30 sec a Hero burns for 2% a second for 8 sec. Get 8 yards away from everyone (solo: from
    where it struck) for 3 sec to put it out.
  - **Emberfall**: every 40 sec embers fall on 3 spots; 5 sec later each deals 15% within 4 yards.
  - Two arena dangers never go off within 3 seconds of each other, and the revival spot stays safe.
- **Ten more affixes join the rotation**:
  - **Bolstering**: a wave may bring a Bolstered enemy; while it lives, the others take 25% less damage.
  - **Mending Embers**: every 30 sec an enemy heals its most injured neighbour for 12 sec. Kill it.
  - **Cinder Shield**: each wave one enemy wears a shield of 20% of its health and hits 20% harder while it holds.
  - **Ashen Echo**: every 40 sec an Echo at the edge channels into an enemy for 8 sec; if it finishes, that enemy
    attacks and casts 15% faster. Kill it, or stun, silence or fear it.
  - **Volatile Remains**: fallen enemies leave embers that explode 4 sec later (12% within 5 yards).
  - **Shackling Chains**: every 35 sec two Heroes are chained; over 12 yards apart they burn. Stay together for 3 sec to
    break it (solo: stay near your point). Never active with Searing Brand.
  - **Burning Pursuit**: every 35 sec an enemy hunts a random Hero for 6 sec, 30% faster.
  - **Cracked Earth**: every 35 sec a ring of ground cracks; 5 sec later it erupts (15%) and slows anyone on it for 10
    sec.
  - **Vengeful Flame**: when an enemy falls, its nearest ally deals 5% more for 10 sec (up to three times).
  - **Flamebound**: each wave two enemies are bound; within 8 yards of each other they deal 15% more (10% solo).
- **Boss waves give you room**: before a boss, the next wave waits until the field is clear, then you get 20 seconds to
  prepare (the boss is named in advance).
- **The fallen rise after a boss**: clear a boss wave with anyone still standing and every fallen Hero comes back at the
  edge of the pit with half their health and mana, protected for 5 seconds (attacking ends it).
- **Records and checkpoints count cleared waves**: your best is the highest wave your group fully cleared, and beating a
  boss and clearing its wave unlocks that checkpoint for everyone in the run, fallen or not.
- **Tank priority**: when a wave starts, every enemy turns to the party's tank with a 50% threat lead (taunts
  and normal threat still work). With no tank, enemies go for the nearest player.
- **Thunderbomb's burst no longer leaves a big white square on the ground.**
- **Rune of Scarlet Benediction, redesigned** (Holy Paladin, Holy Priest, and some Preservation Evoker): Holy Shock,
  Power Word: Shield and Renew place a **Scarlet Echo** on their target (up to 4). Your next Holy Light, Flash of Light,
  Flash Heal or Greater Heal there heals them again for 50% and grants a **Benediction**; at 5, your next of those heals
  releases a **Crimson Burst** on up to 5 injured allies nearby (60% of it). Judgement Echoes your Beacon of Light's
  ally. New options include four Evoker abilities, each taking a base ability's place: **Verdant Embrace** (Binding
  Heal or Divine Favor: leap to an ally, heal and Echo them), **Reversion** (Renew: a heal over time that ticks twice
  below half health), **Dream Breath** (Prayer of Healing: a cone of healing that spends 5 Benediction to Echo
  everyone) and **Spiritbloom** (Greater Heal or Holy Light: a heal that splits to the 3 most injured nearby). Also
  **Temporal Anomaly** (a drifting bubble that shields and Echoes), **Stasis** (3 instant heals) and more, with the
  Evoker's own looks and icons. Scarlet Benediction's picks are cleared so you can choose again.
- **A smoother difficulty curve**: enemy health now grows gently and steadily with each wave (wave 15 about 1.6x a
  dungeon enemy's, wave 45 about 4.5x, wave 90 about 11.5x) instead of compounding 10% a wave, and waves no longer raise
  enemy damage. Each extra player adds 55% health.
- **Calmer pacing**: a wave every 75 seconds, or 10 seconds after you clear the field. Waves bring 3 to 12 enemies as
  they climb, plus one for every two extra players (20 at most).
- **Boss waves are a duel**: every 5th wave is one boss alone, and no new wave comes while it lives; you get 20 seconds
  after it falls. Bosses have 2x health and 1.1x damage early, rising to 2.5x and 1.2x from wave 45.
- **No more instant wipes from the cap**: the field holds 8 enemies plus 4 per player. Extra enemies wait as
  reinforcements and enter as room opens. If the field stays full for 30 seconds, **Pressure** builds: enemies deal 5%
  more damage, up to 20%. Get under the cap for 10 seconds and it's gone. The tracker shows Pressure and how many
  reinforcements are waiting.

## 2026-11-23: Card slots

### Stormreaver and the Stormbound Conduit, rebuilt

Both runes now play through their own kits, all the time. **Your picks for these two runes are cleared: choose
them again.**

- **Stormreaver**: your swings (Windfury's too) have a 20% chance to **reset Stormstrike** and make it free.
  Stormstrike makes your next Lava Lash within 8 sec deal **40% more**, and your **Flame Shock ticks can reset Lava
  Lash**. Lava Lash on an enemy you just Stormstruck **erupts** for 50% of your attack power on every enemy within 8
  yards. Your melee abilities always discharge Lightning Shield now; the Stormfront is an option.
- **New Stormreaver options**: **Thunderbomb** (Stormstrike becomes a thrown lightning bomb), **Magma Breaker** (Lava
  Lash becomes a fiery cone that spreads Flame Shock), Static Strikes, Echoing Strike, Overcharge, Molten Spread, Hot
  Hand, Cinder Echo, Tempo, Ride the Lightning (now 20% melee haste) and Maelstrom (resets give Maelstrom Weapon).
- **Stormbound Conduit** is a caster's rune, and a hybrid's through Maelstrom Weapon: Lightning Bolt and Chain
  Lightning **charge your Lightning Shield** (up to 9; an instant bolt adds 2), and **Earth Shock releases** every
  charge above 3 as **Fulmination**, scaling with your spell power or attack power, whichever is higher. Lightning
  Bolt arcs to a second enemy, and a full shield makes your next Earth Shock free.
- **New Conduit options**: **Ball Lightning** (Lightning Bolt becomes a slow orb that zaps enemies on its way, then
  bursts), **Storm Lance** (Earth Shock becomes a lance through every enemy in its line), Overload, Forked Conduit,
  Supercharge, Maelstrom Conduit, Static Field, Reflux, Shared Current (with Stormreaver), Galvanize, Charged Blades,
  Chain Fulmination, Thunderstorm Release, Overflow and Static Surge.
- **The replacements count as what they replace**: Thunderbomb, Magma Breaker, Ball Lightning and Storm Lance now get
  every talent, glyph and bonus that raises Stormstrike, Lava Lash, Lightning Bolt or Earth Shock (damage, critical
  strikes, Lightning Overload), and on their main target they trigger everything that reacts to those abilities (other
  runes and the runes' wildcard effects). Thunderbomb puts Stormstrike's Nature mark on everything it hits, and Magma
  Breaker gets Lava Lash's Flametongue bonus. Fulmination gets Earth Shock's bonuses too.
- **Thunderbomb has its own look**: a new icon, a lightning bomb you can see flying to its target, and a blue burst
  where it lands.

### Fireforged gear and a Firepit that marches through every dungeon

- **The Firepit's spoils are Fireforged gear, made for you**: each piece goes to one of your weakest slots and is
  always well above what you wear there (9 item levels or more), epic, with stats for your role: a caster never gets
  strength, a fighter never intellect, a tank gets tank stats.
- **Titanforged Fireforged**: a very rare roll doubles every stat on the piece. The whole run hears about it.
- **Deep runs drop crazy gear**: from wave 30 a piece may carry a famous proc or equip effect (Hand of Justice,
  Dragonspine Trophy, Thunderfury, Darkmoon Cards, Crusader, Berserking and more, chosen for your role); from wave 90
  it can carry two. Every spoil is certain to have one by wave 90 and two by wave 135.
- **The roster marches through every dungeon**: every 5 waves the enemies of a new dungeon pour in, from Ragefire
  Chasm through the Deadmines, Scholomance, Outland and Northrend to the Halls of Reflection (49 dungeons, 189 kinds of
  enemies). Healers stay capped: one a wave, two alive at most.

### Longer windows for Legendary Runes

- The windows you build up in last **10 seconds longer**: the Iron Bulwark (14 sec), Ursoc's Guard (16), the
  Thunderfield (16, 19 with Lingering Thunder), the Wellspring (18, 22 with Lasting Spring), Northwind (18), Sunlight
  and Moonlight (18, 22 with Long Twilight), Frozen Prey (16) and the Stormfront (16).
- **Iron Reckoning**: the Iron impact now hits **every enemy within 8 yards**, no target limit (it was 5 in a cone
  before you); Thunder Clap in the bulwark makes it reach 10 yards (Shaking Ground 12). **Wide Reckoning** now makes it
  deal 30% more to every enemy past its first. A Shield Slam that kills its target still releases the impact.

### Damage fixes

- **Your damage numbers are real now**: dungeons and the Firepit made monsters tougher by *dividing your damage*
  (so a 400 Stormstrike showed as 100 and damage meters lied). Monsters now get the extra health themselves: you deal
  and see your full damage, and their health bars are bigger to match. How fast things die is unchanged. (A Hero
  above the group's lowest level is still matched down to it, as level sync does.)

- **Molten Armor and other damage shields** were scaled as a monster's blow on you instead of your damage on them:
  in dungeons and the Firepit they hit far harder than everything you cast and topped your meter. They now scale
  like the rest of your damage.
- **The Firepit no longer buries you**: past 150 enemies alive, every enemy that could not spawn made all the others
  3% stronger, without end. A group that fell behind met enemies with ten times the health.
- **Overwhelmed**: the Firepit now holds at most 30 enemies plus 10 for each Hero (50 for a duo). Reach it and you are
  **Overwhelmed**: a warning, and a red 60 sec countdown on the wave tracker. Get back under the cap in time and it
  ends; if not, the Firepit kills the whole team.

### Firepit spoils, fairer

- **One great chest**: at the end of a run a single, big chest stands by the pit; every Hero opens it for their own
  spoils.
- **Spoils are earned by killing a 15th wave's boss**, and everyone still in the run gets a piece, standing or fallen
  (a duo partner who fell no longer goes without). Surviving into the next wave no longer counts.
- **Starting a run at a 15th wave** (a checkpoint) gives no spoils for that wave.
- **Fireforged item level follows your level**: at most your level + 30 for a 15th wave (3 more for each 15 waves
  deeper), so it no longer climbs on its own run after run; heirlooms count as gear of your level. Slots still below
  that are picked first (necks included).

### Fixes

- **Monster casters** no longer hit far harder than their swings: a non-boss monster's spell on you deals at most 3
  of its own swings (a tick at most 1). At level 15 in the Firepit they were hitting for 1,200-1,500 against 100.
- **Horn of Winter** now lasts 30 minutes, as its tooltip says.
- **After a prestige** the rune window opens on Rune I: left on Rune II, it showed a locked slot you could not leave
  until level 30.
- **Tundra Stalker** gives its own 3-15%: Magic Suppression's absorb bonus was being added to it.
- **Call of the Elements** (and Ancestors, Spirits) works without a totem bar: each empty slot drops your best totem
  of its element (Strength of Earth, Searing, Healing Stream, Windfury, or their alternatives).
- **The Stormbound Conduit has no Conductive mark any more**: Chain Lightning arcs 30% of its own hit, and your
  Lightning Shield, Earth Shock, Lava Lash, Starfire and Thunder Clap arc on any enemy, so it no longer fights the
  Stormreaver. Its options that laid Conductive now arc themselves (Branching Current, Flame Conductor) or simply
  deal more (Persistent Current: arcs 15% more; Icy Conductor: Frostbolt 15% more).

### Your damage up, runes rebalanced, sturdier gear

- **All your damage against monsters is 30% higher**: your abilities, your swings and your pet's.
- **Every Legendary Rune's own damage is twice what it was** (a first pass had it at 3.5 times, which drowned out
  everything else you do); **its healing and shields 3 times**. What a rune adds to your own abilities is unchanged.
- **Every piece of Emberforge gear has stamina now** (Leveling Scaled and Fireforged alike), the tank stat mixes the
  most (40% more stamina), and **every stat on them is 15% higher**. Gear you already have updates by itself.
- **Iron Reckoning, rebuilt for survival**: the Iron Bulwark absorbs **45% of your maximum health from every side**
  (it was 15%, from the front only); each enemy the impact hits **mends you for 6%** of your maximum health (30% at
  most); and **Shield Block comes round far sooner**: Shield Slam takes 4 sec off its cooldown, Revenge 2, Devastate 1.

### The Firepit spares your gear

- **No durability loss in the Firepit**: dying or getting hit there never wears your gear.

### No more chain crowd control from monsters

- **Diminishing returns now apply to monsters' crowd control on you.** Every stun, fear, sleep, silence, polymorph,
  horror and the like a monster lands shares one diminishing returns: full length, then half, then a quarter, then
  you are immune until 15 sec after the last one ended. Roots and disarms diminish on their own.
- **Resolve**: for 3 sec after a monster's crowd control on you ends, no other can land.
- A monster's crowd control lasts **6 sec at most**. Raid bosses' are unchanged.

### Echoing Thunder: a field you can see through

- The **Thunderfield** is now faint: its cracks, ring of lightning and bolts are about a third as opaque, its pulse
  barely there. The storm cloud over your head (March of the Storm, Stormheart) is gone; the lightning in your hands
  stays.

### Enemies have no resistances or immunities

- **Nothing resists you any more**: no partial resists, no "Resist" on your spells, and no enemy is immune to stuns,
  fears, roots, slows, silences, interrupts, taunts, knockbacks, disarms, bleeds or a school of magic, bosses
  included. Only an encounter's full invulnerability (a boss's untouchable phase) and immunities to one particular
  spell remain. A plain miss from a level gap still happens.

### The Firepit

- **A new endless wave arena** with a map of its own, from level 10. Queue from the **group finder**: its new
  **The Firepit** tab (beside Dungeons and Player vs. Player) shows the rules and rewards, your group, your best wave
  solo, as a duo, a trio and a group (hover one for its leaderboard), how much tougher your group size makes it, and
  where to start: wave 1, or any 5th wave everyone in your group has reached. Alone you go straight in; a group's leader queues and everyone
  gets an Accept prompt, then the whole group goes in from wherever they are (up to 10). The **Firepit Keeper**
  beside Silas Darkmoon in Stormwind and Orgrimmar opens it too.
- Waves come every **60 seconds** whether you are ready or not, and the ones you don't kill stay. Wave 1 is easy;
  every wave is **10% stronger** than the last, and every extra player makes them tougher and more numerous. Every
  5th wave is a **boss wave**: the boss alone (the Ring of Law's champions, then Blackrock Depths' own).
- **From wave 15 the bosses fight back**: **Molten Eruption** puts rings of flame under you that erupt 3 seconds
  later (step out!); from wave 30 a boss roots itself and gathers a **Searing Nova** (run away from it); from wave
  45 a boss **enrages** under 30% health. Every warning shows on the wave tracker.
- **Fewer Medics**: at most one Anvilrage Medic joins a wave, and never more than two are alive at once.
- **Starting from a later wave works**, and so does the group scaling: both were lost on the way in.
- **The Firepit has its own music**: *Pressure Point* plays while you're inside.
- You fight until everyone falls. A groupmate's battle resurrection can bring you back.
- Then everyone stands again, and each Hero gets **their own chest**: a piece of gear at your level for every 15 waves
  you lived through (epic from wave 45).
- **The Firepit is its own place**: the great arena of the Argent Tournament's coliseum, open to a burning red sky,
  lit warm and ember-orange with a smoky haze, its own loading screen and name. Each run lights it up: a fire pit in
  the middle, braziers along the walls, embers rising and lava at the gates. Arriving shows the Firepit's own title.
- **The Firepit is a dungeon like any other**: the dungeon finder's **eye** shows on your minimap while you're
  inside, and its menu's **Teleport out** takes you back to where you came in from (mid-run, you fall and the
  others fight on). Enemies now come in all around the arena floor's edge, never stuck behind its gates.
- **The wave tracker** sits on the right under the minimap: the wave, its time running out on a bar, enemies alive,
  Heroes standing, and the run's news (a boss coming in red, with a warning sound).
- **The recap** shows the run in big numbers (wave, time, kills, Heroes, enemy health), who stood out, and each
  Hero's peak wave, the enemies they faced, damage, healing, damage taken, kills, biggest hits, falls and more; the
  best in each column lit. `/firepit` shows it again.
- **Leaderboard**: the Keeper keeps your best wave, solo and in groups.

### Fix: rune damage behind you

- **Sanctified Paragon**'s aura now damages enemies all around you, not only those in front. The same fix reaches
  every rune's own strikes (splashes, pierces, pulses, impacts, damage over time): they no longer fail on enemies
  behind you.

### Cross-faction guilds

- Guilds take **both factions**: invite anyone, sign any guild charter, and guild chat reaches every member in a
  language they read.

### Class buffs last 30 minutes

- Every class buff now lasts **30 minutes**: Battle Shout, Commanding Shout, Horn of Winter, the Blessings, Earth
  and Lightning Shield, Water Shield, Thorns, Amplify and Dampen Magic, Shadow Protection, Unending Breath and the
  like (from 2 or 10 min). Buffs that already lasted 30 min (the Armors, Inner Fire, Fortitude, Arcane Intellect,
  Mark of the Wild, the Seals) are unchanged; the hour-long group buffs (Prayer of Fortitude, Gift of the Wild,
  Arcane Brilliance, Prayer of Spirit) are 30 min too, like the rest.

### Wildcard window: drag and link

- **Drag** a known ability from the Wildcard window (the ability grid, the Spells list, or a talent that teaches an
  ability) straight onto your action bars.
- **Shift-click** any ability or talent there to link it in chat: abilities at the rank you know, talents as talent
  links with your rank. Talents in the Talents list and under Your Talents link too.

### New Legendary Runes: Scarlet Benediction, the Evergrove, Iron Reckoning

- **Rune of Scarlet Benediction** (Healer; Renew, Flash Heal or Greater Heal): Renew plants a dormant Scarlet Bloom.
  Your Flash Heal or Greater Heal on that ally opens it: the most injured other ally within 10 yards is healed for
  80% of your spell power, or the Bloom's ally is shielded for 40% if no one is hurt. A Renew that has ticked 3 times
  makes it 30% stronger. Prayer of Healing opens up to 3 Blooms, Binding Heal one. Choices: Deep Bloom, Scarlet
  Garden, Bloodrose, Circling Bloom, Shielded Bloom; Patient Cultivation, Swift Blossom, Guided Petals, Choral Bloom,
  Mending Petals; Second Bloom, Sheltering Petals, Renewing Flower, Guardian Bloom, Binding Petals.
- **Rune of the Evergrove** (Healer; Rejuvenation): Rejuvenation stacks. Cast it again on the same ally for a Second,
  then a Third Rejuvenation, each healing as much as yours every 3 sec. It seeks: on an ally at full health, or one
  already at three, it grows on the most injured ally within 30 yards instead. Swiftmend heals 20% more for each
  extra Rejuvenation and leaves them; Wild Growth adds a stack to up to 2 allies. Choices: Deep Roots, Fourfold
  Grove, Regrowth's Root, Nourishing Grove, Thorned Grove; Wild Seeding, Gentle Seeking, Lifebound, Tranquil Grove,
  Swift Grove; Verdant Swiftmend, Overgrowth, Grove Shelter, Rooted Touch, Innervating Grove.
- **Rune of Iron Reckoning** (Tank; Shield Block, Shield Slam): Shield Block raises an Iron Bulwark in front of you
  for 4 sec, absorbing up to 15% of your maximum health. Shield Slam in that time releases an Iron impact: 20% of its
  hit plus half of what the bulwark absorbed (that part at most half your attack power), to up to 5 enemies in a
  cone. Revenge (20%), Thunder Clap (a full circle) and Devastate (15%) make it stronger. Choices: Crushing Reprisal,
  Wide Reckoning, Immovable, Shockwave Reckoning, Concussive Reckoning; Retaliation, Shaking Ground, Sundering Force,
  Vengeful Devastation, Rending Impact; Fortified Bulwark, Shielded Advance, Commanding Force, Last Bastion, Wall of
  Iron.

### The new runes: their own classes' abilities, and Howling North sped up

- **Howling North** is faster: Howling Blast now readies **2 blades** (Northwind lasts 8 sec), and **Obliterate or
  Frost Strike** on a marked enemy looses one: Obliterate's for **60%** of your attack power (was 45%), Frost
  Strike's for 35%. Blades come 1.5 sec apart (was 3).
- Their options use their own class's abilities now, and **their picks are cleared**: choose again.
  - **Howling North**: Chains of Ice (the frozen field, Frozen Ground), **Shattering Cold** (Hungering Cold),
    **Plaguewind** (Blood Plague), **Horn of the North** (Horn of Winter), **Decaying Ice** (Death and Decay),
    **Winter's Mending** (Death Strike); Glacial Flow works on your Frost Fever target.
  - **Trueshot**: **Venom Arrow** (Serpent Sting), **Concussive Pierce** (Concussive Shot), Silencing Shot powers the
    payload (Arcane Arrow, Arcane Calibration), **Mending Aim** (Mend Pet).
  - **Hungering Rift**: **Hellmaw** (Immolate), **Haunted Maw** (Haunt).
- **Damage bonuses**: rune hits that come from your attack power (or raw spell power) now grow with your damage
  bonuses of their school, as your spells do: the Frost blade with your Frost bonuses, Broadside with your Fire,
  the Stormfront with your Nature, and the same across the runes.

### New Legendary Runes: Hungering Rift, Howling North, Trueshot

- **Rune of the Hungering Rift** (Ranged DPS; Shadow Bolt, Corruption): Shadow Bolt on your Corruption target opens
  a Rift that lashes it every 2 sec for 25% of your spell power, 5% more for each of your damage over time effects on
  it. Drain Soul speeds it up; Shadow Bolt and Death Coil feed it. Choices: Devouring Maw, Many Tongues, Black Breath,
  Plague Maw, Moon-Eater; Haunting Rift, Open Wound, At Your Back, Painful Anchoring, Siphon Conduit; Lingering
  Torment, Last Gasp, Endless Appetite, Doom Echo, Necrotic Appetite.
- **Rune of Howling North** (Melee DPS; Howling Blast, Obliterate): Howling Blast marks up to 5 enemies with
  Northwind; your next Obliterate on one looses a Frost blade (45% of your attack power). Icy Touch, Frost Strike and
  Frostbolt add to it. Choices: Glacial Edge, Rimeblade, Winter's Reach, Flame of Winter, Marked North; Sharpened
  Wind, Frozen Ground, Winter's Return, Stormblade, Rending Ice; Northern Endurance, Icy Wound, Cold Recovery,
  Winter's Renewal, Glacial Flow.
- **Rune of Trueshot** (Ranged DPS; Aimed Shot): Aimed Shot becomes Trueshot, a 2 sec windup standing still for 45%
  more damage, piercing up to 4 enemies behind its target. Hunter's Mark, Multi-Shot and Arcane Blast add to it.
  Choices: Perfect Shot, Piercing Shot, Steady Aim, Flame Arrow, Frostpiercer; Marked Weakness, Wide Trajectory,
  Arcane Arrow, Arcane Calibration, Marked Ricochet; Quick Recovery, Hunter's Mobility, Follow-Up Shot, Steady
  Recovery, Renewed Aim.

### Legendary Runes: visual effects

- About 140 of the runes' hits, damage and healing over time, slows, marks and buffs that showed nothing now have a
  look, borrowed from Blizzard's own spells: Flame Shock, Corruption, Rupture, Moonfire and Chains of Ice for damage
  over time; Renew and Rejuvenation for healing over time; Fire Blast, Ice Lance, Mind Blast, Earth Shock, Holy
  Shock and Hemorrhage for hits; Hunter's Mark, Living Bomb and Stormstrike for marks; Sprint, Clearcasting and
  Sacred Shield for buffs.

### Extended Options: Wellspring, Echoing Thunder, Stormbound Conduit, Shattered Sun, Dawnbound Crusader

- The last five Legendary Runes are **redesigned** with their Extended Options: five choices in each slot. **Your
  picks for these five runes are cleared**: choose again.
- **Wellspring** (now needs Earth Shield too)
  - Chain Heal on your Earth Shield bearer lays a **Wellspring** on them (8 sec); your next Chain Heal reaching it
    releases 40% of that heal to another ally within 12.5 yards. Earth Shield's heal renews it; Riptide on the bearer
    makes the next release 20% stronger.
  - Choices: Flowing Waters, Lasting Spring, Widening Waters, Riptide Spring, Renewing Spring; Deep Current, Split
    Tributary, Concentrated Current, Tidal Surge, Guided Current; Tidal Shelter, Long Reach, Renewing Current, Jade
    Current, Shielded Spring.
- **Echoing Thunder**
  - Thunder Clap lays a **Thunderfield** (6 sec). Standing in it, Shield Slam releases a **Thunder Echo** in a cone
    toward its target: 42% of its damage to up to 5 enemies, once every 2 sec. **Revenge** in the field strengthens
    the next echo; **Earth Shock** on an enemy in it bursts.
  - Choices: Lingering Thunder, March of the Storm, Rolling Front, Stormheart, Consecrated Storm; Reverberating
    Impact, Focused Peal, Distant Rumble, Flame Echo, Frost Echo; Commanding Thunder, Sheltering Reverberation,
    Deafening Impact, Revenge of the Storm, Tidal Shield.
- **Stormbound Conduit** (now needs Chain Lightning too)
  - Lightning Bolt leaves **Conductive**; **Chain Lightning** on a Conductive enemy arcs 30% of that bolt to another.
    Lightning Shield retaliations and **Earth Shock** arc from Conductive enemies too. Lightning Bolt no longer arcs
    on its own.
  - Choices: Persistent Current, Branching Current, Live Wire, Icy Conductor, Flame Conductor; Forked Tempest, Closed
    Circuit, Faraday Field, Storm Lash, Starbound Circuit; Storm Within, Violent Reprisal, Distant Conductor, Storm's
    Eye, Shocking Surge. Live Wire and Storm Within can't be chosen together.
- **Shattered Sun** (now Fireball and Frostbolt)
  - **Fireball** embeds a Sunshard (up to 2 enemies); **Frostbolt** fractures it (45% of your spell power as
    Frostfire to the host, 25% around it) and breaks its core; your next Fireball on a **Broken Core** collapses it.
    **Ice Lance** on a Broken Core adds Frost damage; a fracture extends your **Flame Shock**.
  - Choices: Twin Suns, Orbiting Sun, Solar Anchor, Lunar Shard, Storm Shard; Splinterstorm, Solar Lance, Scalding
    Ground, Frozen Flash, Flame Burst; Implosion, Chain Reaction, Second Sunrise, Shattering Lance, Icy Shelter.
- **Dawnbound Crusader** (now needs Judgment too)
  - **Beacon of Light** becomes **Dawn Vow**: vow yourself or an ally. **Judgment** marks an enemy; Crusader Strike on
    it, or Holy Shock, sends a **Dawn Echo** from your vow (once every 3 sec) that heals the most injured ally near
    them. Your Doctrine changes what it does.
  - Choices: Mercy, Retribution, Communion, Earthen Oath, Crusader's Flame; Dawnwave, Sunburst, Sacred Ground,
    Frostfire Dawn, Chain of Grace; Steadfast, Passing the Torch, Shared Pilgrimage, Renewed Oath, Searing Vow.

### Black Covenant

- **Bind: Voidwalker** needs **Summon Voidwalker**: only a Hero who knows it gets Bind: Voidwalker (in its place on
  your bars), and forgetting it unbinds you.
- **Covenant Armor** now gives **50% more armor** (was 400% more armor from items). It is added on top of your other
  armor bonuses, never multiplying them, and follows your armor as it changes.
- Covenant Armor's **15% more maximum health** is back (the armor change had dropped it).

### Fixes

- The rune build's description no longer runs under the rune's medallion: it stays on one line (a little smaller
  when it is long).

### Legendary Runes: taught abilities replace yours

- Every ability a Legendary Rune teaches now **needs an ability you know and takes its place** on your bars, as
  Sabre Slash does with Sinister Strike. Without that ability, the rune's ability is not taught; learn it and it is.
  - **Duelist**: Arcane Shot becomes **Pistol Shot**, Rapid Fire becomes **Broadside**.
  - **Red Communion**: Death Strike becomes **Sanguine Recall**.
  - **Primal Worldwarden**: Stoneskin Totem becomes the **Worldheart Totem** (the rune now needs Stoneskin Totem),
    Totemic Recall becomes **Place Worldheart**.
  - **Black Covenant**: Shadow Ward becomes **Dark Intercession**.
  - **Challenge** (Red Communion, Primal Worldwarden) takes the place of your taunt: Taunt, Growl, Dark Command or
    Righteous Defense.

### Extended Options: Black Covenant, Wandering Mist, Wild Hunt

- Three more Legendary Runes get their **Extended Options** (from the updated compendium): five choices in each slot.
  A few old choices left or moved; **those picks are cleared**: choose again (listed below). Other picks keep their
  meaning.
- **Black Covenant**
  - New choices: **Blood Pact** (Blood Strike), **Earthen Pact** (Earth Shield), **Siphoning Ward**, **Unfettered
    Drain**, **Corrupted Drain** (Corruption), **Frostbound Siphon** (Icy Touch), **Judgment of the Void**
    (Judgment), **Blood Intercession** (Blood Boil).
  - Ravenous Siphon and Lashing Rhythm are gone (Siphon picks 2 and 3 cleared).
- **Wandering Mist**
  - A Wandering Mist on your **Renew** target heals 15% more.
  - New choices: **Gathering Mists**, **Renewing Mist** (Renew), **Tidal Mist** (Riptide), **Holy Thread** (Holy
    Shock), **Chain of Jade** (Chain Heal), **Rolling Mist**, **Renewing Breath** (Renew), **Earthen Cocoon** (Earth
    Shield). **Jade Shelter** moved to the Approach slot (30% of a Mist as a shield, once every 8 sec).
  - Lingering Mist and Deep Breath are gone (Mistflow pick 3 and Approach picks 2 and 3 cleared).
- **Wild Hunt**
  - Spectral Beasts deal 15% more to your **Hunter's Mark** target; **Arcane Shot** on a beast's target makes its
    next bite there deal 20% more.
  - The second slot is now **The Hunt**. New choices: **Marked Alpha** (Hunter's Mark), **Frostfang** (Icy Touch),
    **Fixed Prey**, **Arcane Hunt** (Arcane Shot), **Storm Hunt** (Lightning Bolt), **Marked Finale** (Hunter's
    Mark), **Healing Companion** (Mend Pet).
  - Unending Wrath is gone (its pick cleared).

### Extended Options: Sanctified Paragon, Stormreaver, Red Communion, Primal Worldwarden

- Four more Legendary Runes get their **Extended Options** (from the updated compendium): five choices in each slot.
  Your current picks keep their meaning.
- **Sanctified Paragon**
  - **Judgment** on an enemy in the aura adds half a tick of Holy damage; **Holy Shock** healing an ally in it heals
    another nearby; **Shield of Righteousness** on an enemy in it shields you.
  - New choices: **Consecrated Charge** (Charge), **Sanctuary March** (Divine Shield), **Radiant Judgment**
    (Judgment), **Healing Light** (Flash Heal), **Dawnward** (Holy Shock), **Paragon's Aegis** (Shield of
    Righteousness). Bastion of Faith and Weight of the Unworthy now work with any damaging Manifestation.
- **Stormreaver**
  - **Earth Shock** in the Stormfront makes its next pulse stronger; Thunderlash on your **Flame Shock** target makes
    it last longer.
  - New choices: **Stormhunter** (Hunter's Mark), **Frozen Front** (Frost Shock), **Stormhammer** (Shield Slam),
    **Flame Conductor** (Flame Shock), **Static Strike** (Lightning Bolt), **Warstorm** (Thunder Clap).
- **Red Communion**
  - Blood Strike on a linked enemy with your **Corruption** makes it last longer.
  - New choices: **Corrupt Communion** (Corruption), **Thorned Communion** (Mangle), **Rending Communion** (Rend),
    **Earthbound Blood** (Earth Shield), **Death's Recall** (Death Coil), **Holy Communion** (Holy Shock).
- **Primal Worldwarden**
  - New choices: **Bloodstone Command** (Blood Strike), **Judgment of the Elements** (Judgment), **Consecrated Heart**
    (Consecration), **Earthen Communion** (Earth Shield), **Thunderheart** (Thunder Clap), **Runic Renewal** (Death
    Coil).

### Extended Options: Whitefang, Ursoc's Champion

- Two more Legendary Runes are **redesigned** with their Extended Options: five choices in each slot. **Your picks
  for these two runes are cleared**: choose again.
- **Whitefang**
  - With your **Rake** on the enemy, the shatter also leaves **Frozen Wound** (25% of your attack power over 6 sec);
    **Ferocious Bite** on Frozen Wound deals its next tick at once. A **Frostbolt** hit on Frozen Prey makes its
    shatter deal 25% more and the mark last 2 sec longer.
  - Choices: Predator's Fang, Pack of Winter, Glacial Claws, Stormfang, Flamefang; Frozen Wound, Cold Blood,
    Predator's Chill, Marked Prey, Moonfrost; Swift Hunt, Winter's Recovery, Lingering Frost, Mending Fang, Clawing
    Cold.
- **Ursoc's Champion**
  - **Mangle** in the guard marks its target: the impact there also strikes up to 4 enemies near it. **Swipe** in the
    guard makes it last 1 sec longer. **Earth Shock** makes your next impact 15% stronger, half of it Nature.
  - Choices: Crushing Champion, Packbreaker, Unbroken Champion, Stoneguard, Thunderbear; Long Guard, Earthen Hide,
    Ursoc's Challenge, Rending Maul, Earthshock Roar; Defiant Roar, Restoring Force, Bear's Momentum, Lifebloom Hide,
    Shielding Roots. The slots are now Impact, Guard and Resolve.

### Extended Options: Splintered Crown, Duelist, Ember's Wrath

- Three more Legendary Runes get their **Extended Options**, each **redesigned** around the document: five new
  choices in each slot. **Your picks for these three runes are cleared**: choose again.
- **Splintered Crown**
  - **Ice Lance** looses 2 Icicles at its target, **Cone of Cold** one at each enemy it hits (up to 5), and
    **Blizzard** one every 2 sec of its channel. Every Icicle leaves **Frostbite** (6 sec): your Frostbolt and Ice
    Lance deal 8% more there.
  - Choices: Glacial Spear, Twin Fangs, Frozen Volley, Hailstorm Crown, Splintering Ice; Cold Snap, Brittle Impact,
    Gathering Blizzard, Frostfire Fracture, Runic Rime; Deep Freeze, Glacial Wound, Permafrost, Whiteout, Cold Front.
- **Duelist**
  - Opening Shot now comes every **third** Sabre Slash, and **Pistol Shot costs 10 Energy** (was 20; still 60% of
    your attack power). After a Pistol Shot, your next Sabre Slash deals 15% more and strikes a second enemy.
    **Eviscerate** with Opening Shot ready loads your next Pistol Shot; **Fire Blast** makes it explode. Broadside
    stays.
  - Choices: Heavy Caliber, Gunslinger, Scattershot, Flame Pistol, Storm Pistol; Powder and Steel, Between the Eyes,
    Loaded Powder, Marked Shot, Frozen Powder; Quick Draw, Cut and Run, Efficient Duelist, Healing Powder, Arcane
    Powder. Rapid Shots and the Cannon-Fire choices are gone.
- **Ember's Wrath**
  - **Shred** on your Cinder Wound adds Fire damage; **Ferocious Bite** makes it last longer; **Fire Blast** on your
    Flame Shock target spreads it.
  - Choices: Cinderclaw, Wildfire Claws, Living Flame, Flame Shock Claws, Frozen Embers; Ember Rush, Smoldering Bite,
    Firestorm Pounce, Phoenix Claws, Wildfire Swipe; Ashen Renewal, Flame-Tipped Claws, Phoenix Scar, Healing Flame,
    Burning Rake.

### Extended Options: Spectral Blade, Elune's Guardian, Hollow Congregation

- Three more Legendary Runes get their **Extended Options**: five choices in each slot.
- **Spectral Blade**
  - **Backstab** marks its target for 6 sec: your phantom spends the mark for 25% more damage (**Marked for Murder**
    now raises that to 40%). After the phantom strikes, your next **Sinister Strike** costs 10 less Energy. On your
    **Shadow Word: Pain** target, the phantom leaves Phantom Wound.
  - Quick Death is now **Fatal Opening** (it also makes your next builder 10 Energy cheaper); Patient Killer is now
    **Perfect Ambush**.
  - New choices: **Frost Phantom** (Icy Touch), **Flame Phantom** (Flame Shock), **Marked Execution** (Hunter's
    Mark), **Corrupted Blade** (Corruption), **Restoring Phantom** (Renew), **Bleeding Follow-Through** (Rend).
- **Elune's Guardian**
  - Owls dive for **37.5%** of the Starfire's damage (was 30%) and lay **Moonlit Prey**: your **Wrath** on it calls
    the owl back for a Nature strike. Dives extend your **Moonfire**, and deal 20% more to your **Hunter's Mark**
    target.
  - Watchful Flight is now **Lunar Flock**. **Twilight Hunt** now comes from the return strike.
  - New choices: **Sentinel of the Hunt**, **Frostfeather**, **Lunar Guidance** (replaces Moonlit Pursuit: if you had
    it, choose again), **Command the Hunt**, **Sun and Moon**, **Hunter's Pursuit**, **Winter's Blessing**.
- **Hollow Congregation** (redesigned)
  - Echoes deal 5% more for each of your damage over time effects on their target (up to 5). A barrage ending on
    your **Shadow Word: Pain** target leaves a patch; one bolt of each barrage also strikes another of your
    **Corruption** targets.
  - New first slot (where the echoes stand): **Following Shadows**, **Haunted Ground**, **Possession**, **Plague
    Sermon**, **Moonlit Congregation**. New third slot: **Last Word**, **Procession**, **Silent Witness**, **Feeding
    the Void**, **Renewed Witness**. The middle slot gains **Devouring Command** and **Frostbound Sermon**.
  - Gathering, Lasting Echoes, Fervor, Shadow Mend and Swift Command are gone: **your first and third picks for this
    rune are cleared**, choose again.

### Fixes

- **Phoenix Wake's phoenix** was a totem: it now flies as the phoenix again. (The Primal Worldwarden's totem had taken
  its place.)

### Commands

- **.emberspark** works for everyone: on its own it shows your account's Emberspark; with an amount it adds that much
  to your own account. Only GMs can use it on another player. (**.unbindrune** already worked for everyone, on
  yourself.)

### Extended Options: Arcane Mind, Phoenix Wake, Astral Horizon, Guiding Light

- Four more Legendary Runes get their **Extended Options**: five choices in each slot. Your current picks keep their
  meaning.
- **Arcane Mind**
  - **Arcane Barrage** spends up to 2 Insight: 15% more damage for each, and one other enemy within 8 yards hit for
    30% of it for each.
  - Finishing an empowered **Arcane Missiles** makes your next **Arcane Blast** within 6 sec instant.
  - **Lightning Bolt** grants 1 Insight, once every 8 sec.
  - New choices: **Storm Conduit** (Lightning Bolt), **Starfall Equation** (Starfire), **Missile Shards**
    (Frostbolt), **Arcane Charge** (Arcane Explosion), **Evocative Mind** (Evocation), **Shocking Clarity**
    (Lightning Shield).
- **Phoenix Wake**
  - The phoenix strikes its target for **55%** of the Pyroblast's damage (was 50%) and burns it for 30% of your
    spell power over 6 sec (**Phoenix Burn**). Pyroblast deals 15% more to a target with your Phoenix Burn.
  - **Flame Shock**: crossing your Flame Shock target, the phoenix erupts for 20% of your spell power on up to 3
    enemies.
  - New choices: **Flamekeeper** (Flame Shock), **Lava Flock** (Lava Burst), **Blazing Path** (Flamestrike), **Hot
    Pursuit** (Scorch), **Wildfire Return** (Flame Shock), **Emberguard** (Fire Ward).
- **Astral Horizon**
  - The Orb strikes your target for **two bolts** as it is set loose.
  - **Arcane Blast** on the Orb's target makes its next bolt deal 35% more; **Arcane Explosion** near the Orb pulses
    for a bolt on up to 5 enemies (once every 4 sec); **Starfire** on its target makes the next bolt hit one more
    enemy for half.
  - New choices: **Orbital Lightning** (Lightning Bolt), **Starward Satellite** (Moonfire), **Shattering Orbit**
    (Frost Nova), **Distant Spark** (Arcane Shot), **Evoked Remnant** (Evocation), **Astral Shelter** (Power Word:
    Shield).
- **Guiding Light**
  - The star now heals for a full Prayer of Mending (two pulses) when it arrives (was 75%).
  - **Renew**: leaving your Renew target, the star leaves a trail healing it every 2 sec for 6 sec. **Greater
    Heal**: moving the star shields its new bearer. **Earth Shield**: arriving on your Earth Shield target, the star
    also heals one injured ally nearby.
  - New choices: **Earthen Star** (Earth Shield), **Moonlit Star** (Rejuvenation), **Chain Guidance** (Chain Heal),
    **Holy Passage** (Holy Shock), **Tidal Farewell** (Riptide), **Renewed Farewell** (Renew).

### Astral Convergence: Extended Options

- **Astral Convergence** is the first Legendary Rune with its **Extended Options**: each of its three slots now has
  **five** choices (two rows in the build view). Your current picks keep their meaning.
- **Its core now scales with spell power**: the collision deals **80% of your Arcane spell power** and the flare
  **40% of your Nature spell power** (never less than your Starfire's own damage while your gear is light). The flare
  now leaves **fresh Sunlight**, so the cycle keeps going.
- **Ability interactions** (always on, with the ability):
  - **Moonfire**: a collision on your Moonfire target also hits one other enemy within 8 yards for 40%.
  - **Insect Swarm**: each flare on its target extends it by 2 sec, up to 6 sec per cast.
  - **Lightning Bolt**: on Moonlight, it flares for 75% without leaving Sunlight, once every 6 sec per enemy.
- **New choices**:
  - Collision: **Flame Eclipse** (Flame Shock: a collision on your Flame Shock target adds 25% of your Fire spell
    power) and **Frozen Eclipse** (Frostbolt: a hit on your Sunlight target makes the next collision 25% stronger).
  - Alignment: **Tidal Flare** (Lightning Bolt: its flare also hits one other enemy for half) and **Twin Moons**
    (Moonfire: a collision spreads your Moonfire to one other enemy within 8 yards, once every 8 sec).
  - Flare: **Insect Renewal** (Insect Swarm: the next Wrath casts 15% faster) and **Astral Ward** (Barkskin: a flare
    while it is up shields you for 40% of your spell power, once every 12 sec).
- **Astral Rupture**: a flare on the rupture's target deals its next tick at once. **Eventide** now deals 10% of
  your spell power each second.

### Cat and Bear Form

- In **Cat Form** you can use your **Energy** abilities from every class (a Rogue's Sinister Strike, a Monk's
  strikes...), and in **Bear** or **Dire Bear Form** your **Rage** abilities (a Warrior's Heroic Strike and Execute, a
  Death Knight's). They still work out of form, as before.

### Wildcard rolls

- Abilities whose spell shows no damage, healing or threat no longer go to every role: Execute, Bloodrage,
  Demoralizing Shout and the like (Rage, Warrior) go to Melee and Tank; the Rogue's and anything spending Energy to
  Melee; Desperate Prayer and other heals on yourself to Healer and Ranged. Buffs and utility of the casters still
  suit everyone.
- **Healers roll heals**: 7 rolls in 10 of a Healer's are a healing ability (while any is left to learn) and 7 talent
  rolls in 10 a healing talent (healing stats, all healing, or one improving a heal you know); the rest are anything
  a Healer takes, as before.
- **Hunters are Ranged**: the Hunter's shots, stings, aspects, traps and cooldowns (Rapid Fire, Readiness,
  Concussive Shot...) and ranged attack power talents roll for the **Ranged** role, not Melee. The Hunter's melee
  strikes (Raptor Strike, Mongoose Bite, Wing Clip, Counterattack) stay Melee.
- Talents that improve one ability need it: Improved Hamstring (Hamstring), Improved Kick (Kick), Desolation (Blood
  Strike), Master of Deception and Master of Subtlety (Stealth), Spirit Bond (a pet), Feral Swiftness and Survival
  Instincts (Cat or Bear Form), Tactical Mastery (Defensive Stance), Shatter (a frost spell), Ignite (a fire spell),
  Ancestral Healing (a shaman heal). Talents the game triggers only from certain spells need one of those spells.
- Fire Nova needs a fire totem, and Savage Roar needs Cat Form.
- Survival Instincts is a talent (a Talent Card), and Cleanse Spirit an ability (a Skill Card, for Healers).

### Legendary Rune panel

- **A new look**, from the redesign's mockups:
  - **Recommended for you**: up to three runes as gold-ringed medallions, each with its name, roles, what it does and
    **Preview rune**; **Browse all runes** below.
  - **Browse legendary runes**: a search by name, ability or keyword, **All / Damage / Healing / Tank**, a **Usable
    now** switch, and every rune in a list with where it stands (Usable now, Requires ..., Your cards bring ...),
    **Preview** and **Choose** (or **Learn Available**).
  - **The rune's build**: its medallion with its three slots around it (a slot opens once the one before has its
    modifier), **Choose your ...** and the slot's three modifiers. Click one to select it, then **Confirm modifier**.
    A preview ends with **Choose this rune**, and the modifiers you tried come with it.
- Fixed: long words in the rune panel were cut off: a modifier's description ran past its column (the columns now
  grow to the longest, their words a little smaller when they must), and Browse cut each rune's description to one
  line with "..." (now two lines).
- Fixed: **tooltips stopped working** after the rune panel had been open (until a /reload). The panel now has its
  own tooltip and its own questions, and leaves the game's alone.
- Legendary Runes can be seen and bound **from level 1** (/runes or the rune button). The panel only opens by itself
  from level 5, while you haven't chosen one.
- Fixed: the rune panel's Lua error ("attempt to index global 'alone'") every frame while it was open.
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
  gold badge on the icon's corner with its level and name beneath; the lists show rarity as gems and the unlock level
  beside each card, with scroll bars, and sort by rarity or by level.
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
