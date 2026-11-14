-- The Legendary Runes, generated from the "Emberforge Legendary Runes" design doc (tools/runes/make_catalog.py),
-- then rune 22 (the Sanctified Paragon, its own doc; slots: its own names) added by hand. src/runes.cpp numbers them
-- in this order; the server builds runes 21 (Fel Commandment) and 22 so far. Modifiers: 3 per slot, in slot order.
EmberforgeRunesCatalog = {
    { id = 1,
      name = "Rune of the Unbroken Wall",
      role = "Tank",
      icon = "Ability_Warrior_ShieldMastery",
      builtFrom = "Shield Block, Revenge",
      spentWith = "Shield Slam",
      resource = "Bulwark",
      core = "Blocking an attack or casting Revenge grants 1 Bulwark, up to 5. Shield Slam consumes all " ..
          "Bulwark and grants an absorb shield equal to 6% of your maximum health per stack.",
      loop = "Block and Revenge to build, Shield Slam to cash in before a big hit.",
      mods = {
        {
          name = "Braced Stance",
          text = "Shield Block grants 2 Bulwark when cast.",
        },
        {
          name = "Retaliation",
          text = "Revenge critical strikes grant 1 additional Bulwark.",
        },
        {
          name = "Iron Patience",
          text = "Bulwark stacks to 8, but you gain it only from blocks.",
        },
        {
          name = "Shield Wall Echo",
          text = "The absorb shield also reduces all damage you take by 10% while it holds.",
        },
        {
          name = "Shrapnel",
          text = "When the absorb shield breaks, it deals 40% of its remaining value to enemies within 8 " ..
              "yards.",
        },
        {
          name = "Rallying Bulwark",
          text = "30% of the absorb is also given to the nearest injured party member.",
        },
        {
          name = "Unmoving",
          text = "Each Bulwark stack increases your threat by 5%.",
        },
        {
          name = "Second Wind",
          text = "Shield Slam with 5 Bulwark generates 15 rage.",
        },
        {
          name = "Overloaded Wall",
          text = "The absorb shield is 50% larger, but Shield Slam's cooldown is 3 sec longer.",
        },
      },
      builds = {
        {
          name = "Raid wall",
          mods = { "Iron Patience", "Shield Wall Echo", "Overloaded Wall" },
        },
        {
          name = "Dungeon puller",
          mods = { "Retaliation", "Shrapnel", "Unmoving" },
        },
        {
          name = "Group anchor",
          mods = { "Braced Stance", "Rallying Bulwark", "Second Wind" },
        },
      },
    },
    { id = 2,
      name = "Rune of the Bloodbound",
      role = "Tank",
      icon = "Spell_Deathknight_RuneTap",
      builtFrom = "Heart Strike, Death Strike",
      spentWith = "Rune Tap",
      resource = "Vitae",
      core = "Heart Strike and Death Strike grant 1 Vitae, up to 5. Rune Tap consumes all Vitae, healing you " ..
          "for an extra 4% of maximum health per stack and leaving a Blood Shield for half that.",
      loop = "Strike to build, Rune Tap to heal through spikes.",
      mods = {
        {
          name = "Sanguine Strikes",
          text = "Heart Strike grants 1 additional Vitae when it hits 3 or more enemies.",
        },
        {
          name = "Bitter Harvest",
          text = "Death Strike grants 2 Vitae if you took damage in the last 2 sec.",
        },
        {
          name = "Coagulation",
          text = "Vitae stacks to 8 and decays 1 stack every 10 sec out of combat only.",
        },
        {
          name = "Crimson Bloom",
          text = "The Blood Shield lasts 6 sec longer.",
        },
        {
          name = "Bloodburst",
          text = "Rune Tap with 5 Vitae deals 30% of its healing as Shadow damage to enemies within 10 yards.",
        },
        {
          name = "Shared Blood",
          text = "25% of Rune Tap's healing also heals up to 3 nearby allies.",
        },
        {
          name = "Thick Blood",
          text = "Each Vitae stack reduces magic damage taken by 2%.",
        },
        {
          name = "Runic Hunger",
          text = "Rune Tap refunds 10 runic power per Vitae consumed.",
        },
        {
          name = "Blood Price",
          text = "Rune Tap heals 40% more, but costs 5% of your current health to cast.",
        },
      },
      builds = {
        {
          name = "Spike soaker",
          mods = { "Bitter Harvest", "Crimson Bloom", "Thick Blood" },
        },
        {
          name = "AoE tank",
          mods = { "Sanguine Strikes", "Bloodburst", "Runic Hunger" },
        },
        {
          name = "High-risk",
          mods = { "Coagulation", "Shared Blood", "Blood Price" },
        },
      },
    },
    { id = 3,
      name = "Rune of the Soulbrand",
      role = "Tank",
      icon = "Ability_DemonHunter_SoulCleave",
      builtFrom = "Shear",
      spentWith = "Soul Cleave",
      resource = "Soul Fragments",
      core = "Shear shatters a Soul Fragment (up to 5). Soul Cleave consumes all fragments, healing you for " ..
          "3% of maximum health each and branding the target: it deals 8% less damage to you per fragment for " ..
          "6 sec.",
      loop = "Shear to shatter souls, Soul Cleave to heal and weaken the target.",
      mods = {
        {
          name = "Fracture",
          text = "Shear shatters 2 fragments against a target below 50% health.",
        },
        {
          name = "Searing Souls",
          text = "Immolation Aura shatters 1 fragment every 3 sec.",
        },
        {
          name = "Soul Hoard",
          text = "Fragments stack to 8; Soul Cleave's brand lasts 1 sec longer per fragment beyond 5.",
        },
        {
          name = "Brand of Ruin",
          text = "The brand also makes the target take 10% more Fire damage from you.",
        },
        {
          name = "Soul Rend",
          text = "Soul Cleave strikes all enemies in front of you and brands each of them.",
        },
        {
          name = "Spectral Ward",
          text = "Each fragment consumed grants a 2% absorb shield for 8 sec.",
        },
        {
          name = "Fel Defiance",
          text = "Demon Spikes lasts 2 sec longer per fragment consumed by your last Soul Cleave.",
        },
        {
          name = "Hungering Soul",
          text = "Soul Cleave with 5 fragments refunds 20 rage.",
        },
        {
          name = "Shattered Will",
          text = "Soul Cleave heals 50% more, but Shear deals 20% less damage.",
        },
      },
      builds = {
        {
          name = "Boss tank",
          mods = { "Fracture", "Brand of Ruin", "Fel Defiance" },
        },
        {
          name = "Trash puller",
          mods = { "Searing Souls", "Soul Rend", "Hungering Soul" },
        },
        {
          name = "Self-sustain",
          mods = { "Soul Hoard", "Spectral Ward", "Shattered Will" },
        },
      },
    },
    { id = 4,
      name = "Rune of the Sentinel",
      role = "Tank",
      icon = "Spell_Holy_AvengersShield",
      builtFrom = "Holy Shield blocks",
      spentWith = "Avenger's Shield",
      resource = "Sanctity",
      core = "Each attack blocked while Holy Shield is active grants 1 Sanctity, up to 4. Avenger's Shield " ..
          "consumes all Sanctity and strikes 1 extra target per stack, each struck enemy taking 20% less " ..
          "damage from Holy Shield's next block.",
      loop = "Block with Holy Shield to build, throw Avenger's Shield to spread it.",
      mods = {
        {
          name = "Consecrated Ground",
          text = "Enemies standing in your Consecration grant Sanctity when they attack you.",
        },
        {
          name = "Righteous Guard",
          text = "Holy Shield's first block each cast grants 2 Sanctity.",
        },
        {
          name = "Vigil",
          text = "Sanctity stacks to 6; Avenger's Shield's damage rises 6% per stack beyond 4.",
        },
        {
          name = "Shield of Judgment",
          text = "Avenger's Shield applies Judgement of Light to every enemy it strikes.",
        },
        {
          name = "Sacred Bounce",
          text = "Avenger's Shield silences each struck target for 1 sec per Sanctity.",
        },
        {
          name = "Aegis",
          text = "Avenger's Shield grants you 3% of maximum health as an absorb per enemy struck.",
        },
        {
          name = "Devoted Guardian",
          text = "Each Sanctity increases Holy Shield's block value by 5%.",
        },
        {
          name = "Divine Return",
          text = "Avenger's Shield with 4 Sanctity restores 4% of your maximum mana.",
        },
        {
          name = "Zealous Sentry",
          text = "Avenger's Shield's cooldown is 6 sec shorter, but Holy Shield has 2 fewer charges.",
        },
      },
      builds = {
        {
          name = "Caster pack control",
          mods = { "Righteous Guard", "Sacred Bounce", "Divine Return" },
        },
        {
          name = "Sustained boss tank",
          mods = { "Vigil", "Aegis", "Devoted Guardian" },
        },
        {
          name = "Fast puller",
          mods = { "Consecrated Ground", "Shield of Judgment", "Zealous Sentry" },
        },
      },
    },
    { id = 5,
      name = "Rune of the Lightbringer",
      role = "Healer",
      icon = "Spell_Holy_SearingLight",
      builtFrom = "Flash of Light, Holy Light",
      spentWith = "Holy Shock",
      resource = "Radiance",
      core = "Flash of Light and Holy Light grant 1 Radiance, up to 3. At 3, your next Holy Shock is free and " ..
          "also heals the 2 most injured allies within 30 yards for 50% of its amount.",
      loop = "Cast direct heals to build, Holy Shock to splash healing.",
      mods = {
        {
          name = "Gleaming Hands",
          text = "Flash of Light critical heals grant 1 additional Radiance.",
        },
        {
          name = "Beacon's Light",
          text = "Healing your Beacon of Light target grants Radiance as well.",
        },
        {
          name = "Patient Prayer",
          text = "Radiance stacks to 5; the empowered Holy Shock heals 10% more per stack beyond 3.",
        },
        {
          name = "Dawnbreak",
          text = "The splash heals 4 allies instead of 2, at 35% of Holy Shock's amount.",
        },
        {
          name = "Sacred Ward",
          text = "The empowered Holy Shock leaves an absorb for 30% of its healing on its target.",
        },
        {
          name = "Purifying Light",
          text = "The empowered Holy Shock also removes 1 harmful magic effect from each ally it heals.",
        },
        {
          name = "Lightwalker",
          text = "The empowered Holy Shock grants 30% movement speed for 4 sec.",
        },
        {
          name = "Font of Mana",
          text = "The empowered Holy Shock refunds 6% of your maximum mana.",
        },
        {
          name = "Overflowing Light",
          text = "Direct heals cost 10% more mana, but each heals 8% more.",
        },
      },
      builds = {
        {
          name = "Raid healer",
          mods = { "Patient Prayer", "Dawnbreak", "Font of Mana" },
        },
        {
          name = "Tank healer",
          mods = { "Beacon's Light", "Sacred Ward", "Overflowing Light" },
        },
        {
          name = "Dungeon healer",
          mods = { "Gleaming Hands", "Purifying Light", "Lightwalker" },
        },
      },
    },
    { id = 6,
      name = "Rune of the Penitent",
      role = "Healer",
      icon = "Spell_Holy_PowerWordShield",
      builtFrom = "Smite, Holy Fire",
      spentWith = "Power Word: Shield",
      resource = "Atonement",
      core = "Smite and Holy Fire grant 1 Atonement for each enemy hit, up to 5. Power Word: Shield consumes " ..
          "all Atonement; the shield is 10% larger per stack and heals its target for 20% of its value when " ..
          "it breaks.",
      loop = "Deal damage to build, shield the ally about to take a hit.",
      mods = {
        {
          name = "Zealous Smite",
          text = "Smite critical strikes grant 2 Atonement.",
        },
        {
          name = "Searing Faith",
          text = "Holy Fire's damage over time grants 1 Atonement each time it ticks.",
        },
        {
          name = "Contrition",
          text = "Atonement stacks to 8 but decays 1 stack every 5 sec.",
        },
        {
          name = "Shared Penance",
          text = "The empowered shield is also cast on 1 nearby ally at 50% strength.",
        },
        {
          name = "Burning Rebuke",
          text = "When the empowered shield breaks, it deals its break heal as Holy damage to the attacker.",
        },
        {
          name = "Steadfast Ward",
          text = "The empowered shield also reduces its target's damage taken by 8% while it holds.",
        },
        {
          name = "Swift Absolution",
          text = "The empowered shield grants its target 20% movement speed for 6 sec.",
        },
        {
          name = "Mindful Offering",
          text = "Consuming 5 Atonement restores 5% of your maximum mana.",
        },
        {
          name = "Martyr's Price",
          text = "The empowered shield is 30% larger, but Weakened Soul lasts 50% longer.",
        },
      },
      builds = {
        {
          name = "Tank shielder",
          mods = { "Zealous Smite", "Steadfast Ward", "Martyr's Price" },
        },
        {
          name = "Group shielder",
          mods = { "Searing Faith", "Shared Penance", "Mindful Offering" },
        },
        {
          name = "Damage-leaning",
          mods = { "Contrition", "Burning Rebuke", "Swift Absolution" },
        },
      },
    },
    { id = 7,
      name = "Rune of the Tidecaller",
      role = "Healer",
      icon = "Spell_Nature_HealingWaveGreater",
      builtFrom = "Healing Wave, Lesser Healing Wave",
      spentWith = "Chain Heal",
      resource = "Tidal Surge",
      core = "Healing Wave and Lesser Healing Wave grant 1 Tidal Surge, up to 3. At 3, your next Chain Heal " ..
          "is instant, jumps to 2 extra targets and loses no healing per jump.",
      loop = "Single-target waves to build, Chain Heal to sweep the group.",
      mods = {
        {
          name = "Rising Tide",
          text = "Healing a target below 50% health grants 1 additional Tidal Surge.",
        },
        {
          name = "Earthen Current",
          text = "Your Earth Shield's heals grant Tidal Surge.",
        },
        {
          name = "Deep Waters",
          text = "Tidal Surge stacks to 5; the empowered Chain Heal jumps once more per stack beyond 3.",
        },
        {
          name = "Riptide Wake",
          text = "Each target of the empowered Chain Heal heals for 15% more over 6 sec.",
        },
        {
          name = "Crashing Wave",
          text = "The empowered Chain Heal's final jump heals for 50% more.",
        },
        {
          name = "Cleansing Tide",
          text = "The empowered Chain Heal removes 1 poison or disease from each target.",
        },
        {
          name = "Spirit Walk",
          text = "You can cast the empowered Chain Heal while moving.",
        },
        {
          name = "Water Shield Echo",
          text = "The empowered Chain Heal restores 2% of your maximum mana per target.",
        },
        {
          name = "Storm Tide",
          text = "The empowered Chain Heal heals 25% more, but Healing Wave's cast time rises by 0.3 sec.",
        },
      },
      builds = {
        {
          name = "Raid sweeper",
          mods = { "Deep Waters", "Riptide Wake", "Water Shield Echo" },
        },
        {
          name = "Mobile healer",
          mods = { "Rising Tide", "Cleansing Tide", "Spirit Walk" },
        },
        {
          name = "Burst healer",
          mods = { "Earthen Current", "Crashing Wave", "Storm Tide" },
        },
      },
    },
    { id = 8,
      name = "Rune of the Grove",
      role = "Healer",
      icon = "Spell_Nature_Rejuvenation",
      builtFrom = "Rejuvenation ticks",
      spentWith = "Swiftmend",
      resource = "Verdancy",
      core = "Each Rejuvenation tick grants 1 Verdancy, up to 10. Swiftmend consumes all Verdancy and seeds a " ..
          "grove under its target that heals up to 5 allies within 8 yards for 2% of their maximum health per " ..
          "stack over 6 sec.",
      loop = "Spread Rejuvenation to build, Swiftmend to plant a healing grove.",
      mods = {
        {
          name = "Lifebinder",
          text = "Lifebloom's bloom grants 3 Verdancy.",
        },
        {
          name = "Nature's Bounty",
          text = "Rejuvenation critical ticks grant 2 Verdancy.",
        },
        {
          name = "Ancient Roots",
          text = "Verdancy stacks to 15, but Swiftmend needs at least 8 to seed a grove.",
        },
        {
          name = "Overgrowth",
          text = "The grove also applies a 6 sec Rejuvenation to each ally it heals.",
        },
        {
          name = "Thornwood",
          text = "The grove deals Nature damage to enemies inside it equal to 20% of its healing.",
        },
        {
          name = "Sanctuary",
          text = "Allies inside the grove take 6% less damage.",
        },
        {
          name = "Wild Stride",
          text = "Allies leaving the grove gain 30% movement speed for 3 sec.",
        },
        {
          name = "Living Mana",
          text = "The grove restores 1% of your maximum mana each time it heals 5 allies.",
        },
        {
          name = "Hungry Roots",
          text = "The grove heals 40% more, but Swiftmend's cooldown is 5 sec longer.",
        },
      },
      builds = {
        {
          name = "Raid grove",
          mods = { "Ancient Roots", "Sanctuary", "Hungry Roots" },
        },
        {
          name = "Dungeon roamer",
          mods = { "Nature's Bounty", "Overgrowth", "Wild Stride" },
        },
        {
          name = "Lifebloom tank healer",
          mods = { "Lifebinder", "Thornwood", "Living Mana" },
        },
      },
    },
    { id = 9,
      name = "Rune of the Jade Serpent",
      role = "Healer",
      icon = "Ability_Monk_SoothingMists",
      builtFrom = "Soothing Mist",
      spentWith = "Enveloping Mist",
      resource = "Jade Essence",
      core = "Each second you channel Soothing Mist grants 1 Jade Essence, up to 5. Enveloping Mist consumes " ..
          "all Essence, casts instantly, and heals 12% more per stack.",
      loop = "Channel Soothing Mist to build, Enveloping Mist to finish the heal.",
      mods = {
        {
          name = "Serpent's Breath",
          text = "Vivify cast during Soothing Mist grants 1 Jade Essence.",
        },
        {
          name = "Mistwalker",
          text = "Renewing Mist jumping to a new ally grants 1 Jade Essence.",
        },
        {
          name = "Still Waters",
          text = "Jade Essence stacks to 8 while you stand still.",
        },
        {
          name = "Jade Bloom",
          text = "The empowered Enveloping Mist also heals 2 nearby allies for 30% of its amount.",
        },
        {
          name = "Cocooned",
          text = "If its target falls below 30% health, the empowered Enveloping Mist triggers a Life Cocoon " ..
              "at 40% strength.",
        },
        {
          name = "Essence Burst",
          text = "The empowered Enveloping Mist heals for 25% more over its duration.",
        },
        {
          name = "Drifting Mist",
          text = "You can move while channeling Soothing Mist.",
        },
        {
          name = "Tea Ceremony",
          text = "The empowered Enveloping Mist reduces Thunder Focus Tea's cooldown by 2 sec per stack.",
        },
        {
          name = "Overbrewed",
          text = "The empowered Enveloping Mist costs no mana, but Soothing Mist costs 20% more.",
        },
      },
      builds = {
        {
          name = "Raid channeler",
          mods = { "Still Waters", "Jade Bloom", "Overbrewed" },
        },
        {
          name = "Tank healer",
          mods = { "Serpent's Breath", "Cocooned", "Tea Ceremony" },
        },
        {
          name = "Mobile healer",
          mods = { "Mistwalker", "Essence Burst", "Drifting Mist" },
        },
      },
    },
    { id = 10,
      name = "Rune of the Arcane Mind",
      role = "Ranged",
      icon = "Spell_Arcane_Blast",
      builtFrom = "Arcane Blast",
      spentWith = "Arcane Missiles",
      resource = "Arcane Insight",
      core = "Casting Arcane Blast generates Arcane Insight. At 4 stacks, your next Arcane Missiles consumes " ..
          "all Insight, requires no mana, and fires an additional volley.",
      loop = "Build with Arcane Blast, spend with Arcane Missiles.",
      mods = {
        {
          name = "Accelerated Thought",
          text = "Arcane Blast critical strikes generate 1 additional Insight.",
        },
        {
          name = "Expanding Intellect",
          text = "Arcane Explosion generates 1 Insight when it hits at least 3 enemies. Once per cast.",
        },
        {
          name = "Patient Calculation",
          text = "Insight's maximum rises to 6. Empowered Arcane Missiles consumes all stacks and deals 8% " ..
              "more damage per stack beyond 4.",
        },
        {
          name = "Focused Intention",
          text = "The additional volley deals 50% increased damage to your primary target.",
        },
        {
          name = "Fractured Thought",
          text = "The additional volley also strikes up to 3 nearby enemies for 40% of its damage.",
        },
        {
          name = "Lingering Equation",
          text = "Empowered Arcane Missiles leaves an Arcane Echo, dealing 20% of the channel's damage over 6 " ..
              "sec.",
        },
        {
          name = "Unbound Mind",
          text = "You can move while channeling empowered Arcane Missiles.",
        },
        {
          name = "Mental Reserves",
          text = "Completing empowered Arcane Missiles restores 3% of your maximum mana.",
        },
        {
          name = "Overclocked Intellect",
          text = "Empowered Arcane Missiles channels 30% faster, but the Arcane Blasts that generate Insight " ..
              "cost 15% more mana.",
        },
      },
      builds = {
        {
          name = "Burst caster",
          mods = { "Accelerated Thought", "Focused Intention", "Overclocked Intellect" },
        },
        {
          name = "Dungeon cleave",
          mods = { "Expanding Intellect", "Fractured Thought", "Mental Reserves" },
        },
        {
          name = "Mobile sustained damage",
          mods = { "Patient Calculation", "Lingering Equation", "Unbound Mind" },
        },
      },
    },
    { id = 11,
      name = "Rune of the Pyre",
      role = "Ranged",
      icon = "Spell_Fire_Fireball02",
      builtFrom = "Fireball, Scorch critical strikes",
      spentWith = "Pyroblast",
      resource = "Kindling",
      core = "Fireball and Scorch critical strikes grant 1 Kindling, up to 3. At 3, your next Pyroblast is " ..
          "instant, consumes all Kindling and erupts for 30% of its damage to enemies within 8 yards of its " ..
          "target.",
      loop = "Crit with Fireball and Scorch, then an instant Pyroblast.",
      mods = {
        {
          name = "Searing Focus",
          text = "Scorch grants 1 Kindling on a target below 35% health, crit or not.",
        },
        {
          name = "Wildfire",
          text = "Living Bomb's explosion grants 1 Kindling per 2 enemies hit.",
        },
        {
          name = "Slow Burn",
          text = "Kindling stacks to 5; the empowered Pyroblast deals 10% more damage per stack beyond 3.",
        },
        {
          name = "Meteor Heart",
          text = "The empowered Pyroblast deals 40% more damage to its primary target and no splash.",
        },
        {
          name = "Firestorm",
          text = "The eruption's radius doubles and it leaves burning ground for 30% of its damage over 4 " ..
              "sec.",
        },
        {
          name = "Ember Brand",
          text = "The empowered Pyroblast's damage over time lasts 6 sec longer.",
        },
        {
          name = "Blazing Speed",
          text = "The empowered Pyroblast grants 30% movement speed for 3 sec.",
        },
        {
          name = "Ashen Reserves",
          text = "The empowered Pyroblast refunds 50% of its mana cost.",
        },
        {
          name = "Combustion Engine",
          text = "Kindling's maximum drops to 2, but your fire spells cost 10% more mana.",
        },
      },
      builds = {
        {
          name = "Single-target burst",
          mods = { "Slow Burn", "Meteor Heart", "Ashen Reserves" },
        },
        {
          name = "Dungeon AoE",
          mods = { "Wildfire", "Firestorm", "Blazing Speed" },
        },
        {
          name = "Fast execute",
          mods = { "Searing Focus", "Ember Brand", "Combustion Engine" },
        },
      },
    },
    { id = 12,
      name = "Rune of the Void",
      role = "Ranged",
      icon = "Spell_Shadow_UnholyFrenzy",
      builtFrom = "Mind Flay ticks",
      spentWith = "Mind Blast",
      resource = "Shadow Orbs",
      core = "Each Mind Flay tick grants 1 Shadow Orb, up to 3. Mind Blast consumes all orbs, dealing 25% " ..
          "more damage per orb and extending your Shadow Word: Pain by 3 sec per orb.",
      loop = "Channel Mind Flay to build, Mind Blast to spend.",
      mods = {
        {
          name = "Whispering Dark",
          text = "Shadow Word: Pain critical ticks grant 1 Shadow Orb.",
        },
        {
          name = "Insanity",
          text = "Mind Sear grants 1 orb per 3 enemies it hits each tick.",
        },
        {
          name = "Abyssal Patience",
          text = "Orbs stack to 5, and they no longer fade out of combat.",
        },
        {
          name = "Void Eruption",
          text = "Mind Blast with 3 or more orbs also hits 2 nearby enemies for 50% damage.",
        },
        {
          name = "Devouring Mind",
          text = "The empowered Mind Blast heals you for 15% of its damage.",
        },
        {
          name = "Psychic Scream",
          text = "The empowered Mind Blast slows its target by 40% for 4 sec.",
        },
        {
          name = "Shadow Step",
          text = "Consuming 3 orbs grants 20% movement speed for 4 sec.",
        },
        {
          name = "Mana Leech",
          text = "The empowered Mind Blast restores 2% of your maximum mana per orb.",
        },
        {
          name = "Madness",
          text = "Each orb raises your damage by 3% but your damage taken by 2%.",
        },
      },
      builds = {
        {
          name = "Boss damage",
          mods = { "Abyssal Patience", "Void Eruption", "Madness" },
        },
        {
          name = "Dungeon AoE",
          mods = { "Insanity", "Devouring Mind", "Mana Leech" },
        },
        {
          name = "Solo leveling",
          mods = { "Whispering Dark", "Psychic Scream", "Shadow Step" },
        },
      },
    },
    { id = 13,
      name = "Rune of the Storm Caller",
      role = "Ranged",
      icon = "Spell_Nature_Lightning",
      builtFrom = "Lightning Bolt",
      spentWith = "Earth Shock",
      resource = "Static Charge",
      core = "Lightning Bolt grants 1 Static Charge, up to 5. Earth Shock consumes all charges, dealing 20% " ..
          "more damage per charge and chaining to 1 extra enemy per 2 charges.",
      loop = "Lightning Bolt to charge, Earth Shock to discharge.",
      mods = {
        {
          name = "Overcharge",
          text = "Lightning Bolt critical strikes grant 2 charges.",
        },
        {
          name = "Chain Reaction",
          text = "Chain Lightning grants 1 charge per enemy it hits after the first.",
        },
        {
          name = "Eye of the Storm",
          text = "Lava Burst grants 2 charges, but Lightning Bolt grants none.",
        },
        {
          name = "Thunderclap",
          text = "The empowered Earth Shock knocks back enemies within 6 yards of its target.",
        },
        {
          name = "Molten Earth",
          text = "The empowered Earth Shock makes your next Lava Burst instant.",
        },
        {
          name = "Conductive",
          text = "Each enemy struck by the empowered Earth Shock takes 10% more Nature damage from you for 6 " ..
              "sec.",
        },
        {
          name = "Spiritwalker",
          text = "You can cast Lightning Bolt while moving after an empowered Earth Shock, for 4 sec.",
        },
        {
          name = "Elemental Focus",
          text = "The empowered Earth Shock restores 1% of your maximum mana per charge.",
        },
        {
          name = "Storm Unleashed",
          text = "Charges stack to 8, but at 8 you take 5% of your health as Nature damage.",
        },
      },
      builds = {
        {
          name = "Burst caster",
          mods = { "Overcharge", "Molten Earth", "Storm Unleashed" },
        },
        {
          name = "Cleave caster",
          mods = { "Chain Reaction", "Conductive", "Elemental Focus" },
        },
        {
          name = "Mobile caster",
          mods = { "Eye of the Storm", "Thunderclap", "Spiritwalker" },
        },
      },
    },
    { id = 14,
      name = "Rune of the Soulreaper",
      role = "Ranged",
      icon = "Spell_Shadow_LifeDrain02",
      builtFrom = "Corruption, Curse of Agony ticks",
      spentWith = "Drain Life",
      resource = "Harvested Souls",
      core = "Each tick of Corruption or Curse of Agony has a 20% chance to grant 1 Harvested Soul, up to 5. " ..
          "Drain Life consumes all souls; each soul makes it tick 15% harder and spreads Corruption to 1 " ..
          "nearby enemy.",
      loop = "Keep your damage over time up to harvest souls, Drain Life to reap them.",
      mods = {
        {
          name = "Grim Reach",
          text = "Corruption's chance to grant a soul rises to 30%.",
        },
        {
          name = "Soul Siphon",
          text = "Killing an enemy afflicted by your Corruption grants 2 souls.",
        },
        {
          name = "Malediction",
          text = "Unstable Affliction's ticks also harvest souls.",
        },
        {
          name = "Soul Rot",
          text = "The empowered Drain Life heals you for 50% more.",
        },
        {
          name = "Epidemic",
          text = "The spread Corruption also applies Curse of Agony.",
        },
        {
          name = "Haunted Drain",
          text = "The empowered Drain Life makes its target take 10% more damage from your spells for 8 sec.",
        },
        {
          name = "Fel Armor Link",
          text = "Each soul consumed grants your pet 5% more damage for 10 sec.",
        },
        {
          name = "Life Tap Echo",
          text = "The empowered Drain Life restores 2% of your maximum mana per soul.",
        },
        {
          name = "Dark Bargain",
          text = "Souls stack to 8, but the empowered Drain Life costs 5% of your health per second.",
        },
      },
      builds = {
        {
          name = "Spreading plague",
          mods = { "Grim Reach", "Epidemic", "Life Tap Echo" },
        },
        {
          name = "Solo drainer",
          mods = { "Soul Siphon", "Soul Rot", "Fel Armor Link" },
        },
        {
          name = "Boss reaper",
          mods = { "Malediction", "Haunted Drain", "Dark Bargain" },
        },
      },
    },
    { id = 15,
      name = "Rune of the Deadeye",
      role = "Ranged",
      icon = "Ability_Hunter_AimedShot",
      builtFrom = "Steady Shot",
      spentWith = "Aimed Shot",
      resource = "Steady Aim",
      core = "Steady Shot grants 1 Steady Aim, up to 3. At 3, your next Aimed Shot is instant, consumes all " ..
          "Steady Aim and ignores 30% of its target's armor.",
      loop = "Steady Shot to line up, Aimed Shot to land the big hit.",
      mods = {
        {
          name = "Trick Shots",
          text = "Multi-Shot grants 1 Steady Aim when it hits 3 or more enemies.",
        },
        {
          name = "Careful Aim",
          text = "Steady Shot grants 2 Steady Aim against targets above 80% health.",
        },
        {
          name = "Lock and Load",
          text = "Arcane Shot has a 30% chance to grant 1 Steady Aim.",
        },
        {
          name = "Piercing Round",
          text = "The empowered Aimed Shot passes through and hits every enemy in a line.",
        },
        {
          name = "Kill Zone",
          text = "The empowered Aimed Shot critical strikes deal 40% more damage.",
        },
        {
          name = "Bleeding Wound",
          text = "The empowered Aimed Shot causes its target to bleed for 30% of its damage over 6 sec.",
        },
        {
          name = "Disengage Reset",
          text = "The empowered Aimed Shot resets Disengage's cooldown.",
        },
        {
          name = "Focused Aspect",
          text = "The empowered Aimed Shot restores 4% of your maximum mana.",
        },
        {
          name = "Overdraw",
          text = "The empowered Aimed Shot deals 30% more damage, but Steady Shot's cast time rises by 0.4 " ..
              "sec.",
        },
      },
      builds = {
        {
          name = "Sniper",
          mods = { "Careful Aim", "Kill Zone", "Overdraw" },
        },
        {
          name = "Pack clearer",
          mods = { "Trick Shots", "Piercing Round", "Focused Aspect" },
        },
        {
          name = "Kiter",
          mods = { "Lock and Load", "Bleeding Wound", "Disengage Reset" },
        },
      },
    },
    { id = 16,
      name = "Rune of the Warbringer",
      role = "Melee",
      icon = "Ability_Warrior_SavageBlow",
      builtFrom = "Overpower, Slam",
      spentWith = "Mortal Strike",
      resource = "Battle Trance",
      core = "Overpower and Slam grant 1 Battle Trance, up to 4. Mortal Strike consumes all stacks, dealing " ..
          "15% more damage per stack; at 4 it also makes your next Execute usable at any health.",
      loop = "Overpower and Slam to build, Mortal Strike to cash in, then a free Execute.",
      mods = {
        {
          name = "Tactical Mastery",
          text = "Overpower grants 2 stacks.",
        },
        {
          name = "Whirling Steel",
          text = "Whirlwind grants 1 stack when it hits 3 or more enemies.",
        },
        {
          name = "Bloodlust Trance",
          text = "Battle Trance stacks to 6, and taking damage grants 1 stack every 6 sec.",
        },
        {
          name = "Colossus Smash",
          text = "The empowered Mortal Strike ignores 20% of its target's armor for 6 sec.",
        },
        {
          name = "Cleaving Strikes",
          text = "The empowered Mortal Strike also hits 2 nearby enemies for 50% damage.",
        },
        {
          name = "Deep Wounds",
          text = "The empowered Mortal Strike makes its target bleed for 30% of its damage over 6 sec.",
        },
        {
          name = "Juggernaut",
          text = "The empowered Mortal Strike resets Charge's cooldown.",
        },
        {
          name = "Battle Fury",
          text = "The empowered Mortal Strike generates 5 rage per stack consumed.",
        },
        {
          name = "Reckless Trance",
          text = "Each stack raises your damage by 4% but your damage taken by 3%.",
        },
      },
      builds = {
        {
          name = "Executioner",
          mods = { "Tactical Mastery", "Colossus Smash", "Battle Fury" },
        },
        {
          name = "Dungeon cleaver",
          mods = { "Whirling Steel", "Cleaving Strikes", "Juggernaut" },
        },
        {
          name = "Berserker",
          mods = { "Bloodlust Trance", "Deep Wounds", "Reckless Trance" },
        },
      },
    },
    { id = 17,
      name = "Rune of the Shadowblade",
      role = "Melee",
      icon = "Ability_Backstab",
      builtFrom = "Finishing moves",
      spentWith = "Backstab",
      resource = "Shadow Edge",
      core = "Each finishing move grants 1 Shadow Edge, up to 3. At 3, your next Backstab can be used from " ..
          "any side, consumes all Shadow Edge and awards 2 extra combo points.",
      loop = "Finish to build, an empowered Backstab to refill combo points.",
      mods = {
        {
          name = "Ruthless Edge",
          text = "Finishers at 5 combo points grant 2 Shadow Edge.",
        },
        {
          name = "Slice and Shadow",
          text = "Refreshing Slice and Dice grants 1 Shadow Edge.",
        },
        {
          name = "Patient Blade",
          text = "Shadow Edge stacks to 5; the empowered Backstab deals 10% more per stack beyond 3.",
        },
        {
          name = "Hemorrhage Edge",
          text = "The empowered Backstab makes its target take 10% more physical damage for 8 sec.",
        },
        {
          name = "Fan of Shadows",
          text = "The empowered Backstab also strikes 2 nearby enemies.",
        },
        {
          name = "Ambusher",
          text = "The empowered Backstab always critically strikes.",
        },
        {
          name = "Shadowstep Echo",
          text = "The empowered Backstab resets Shadowstep's cooldown.",
        },
        {
          name = "Relentless Energy",
          text = "The empowered Backstab refunds 25 energy.",
        },
        {
          name = "Glass Dagger",
          text = "The empowered Backstab deals 40% more damage, but you take 10% more damage for 4 sec after.",
        },
      },
      builds = {
        {
          name = "Assassin",
          mods = { "Ruthless Edge", "Ambusher", "Glass Dagger" },
        },
        {
          name = "Combat sustain",
          mods = { "Slice and Shadow", "Hemorrhage Edge", "Relentless Energy" },
        },
        {
          name = "Skirmisher",
          mods = { "Patient Blade", "Fan of Shadows", "Shadowstep Echo" },
        },
      },
    },
    { id = 18,
      name = "Rune of the Crusade",
      role = "Melee",
      icon = "Spell_Holy_CrusaderStrike",
      builtFrom = "Crusader Strike",
      spentWith = "Judgement",
      resource = "Zeal",
      core = "Crusader Strike grants 1 Zeal, up to 3. Judgement consumes all Zeal; each stack makes it deal " ..
          "20% more damage and grants 3% attack speed for 10 sec.",
      loop = "Crusader Strike to build, Judgement to unleash and speed up your swings.",
      mods = {
        {
          name = "Art of War",
          text = "Melee critical strikes have a 30% chance to grant 1 Zeal.",
        },
        {
          name = "Consecrated Zeal",
          text = "Consecration grants 1 Zeal when it damages 3 or more enemies.",
        },
        {
          name = "Righteous Vengeance",
          text = "Zeal stacks to 5, but decays 1 stack every 8 sec.",
        },
        {
          name = "Hammer of Light",
          text = "The empowered Judgement also hits 2 nearby enemies for 50% damage.",
        },
        {
          name = "Sacred Seal",
          text = "The empowered Judgement doubles your active Seal's effect for 6 sec.",
        },
        {
          name = "Exorcist",
          text = "The empowered Judgement makes your next Exorcism instant.",
        },
        {
          name = "Divine Steed",
          text = "The empowered Judgement grants 30% movement speed for 3 sec.",
        },
        {
          name = "Judgements of the Wise",
          text = "The empowered Judgement restores 4% of your maximum mana.",
        },
        {
          name = "Fervor",
          text = "The attack speed bonus doubles, but Judgement's cooldown is 2 sec longer.",
        },
      },
      builds = {
        {
          name = "Seal twister",
          mods = { "Art of War", "Sacred Seal", "Judgements of the Wise" },
        },
        {
          name = "AoE crusader",
          mods = { "Consecrated Zeal", "Hammer of Light", "Divine Steed" },
        },
        {
          name = "Burst",
          mods = { "Righteous Vengeance", "Exorcist", "Fervor" },
        },
      },
    },
    { id = 19,
      name = "Rune of the Fel Hunger",
      role = "Melee",
      icon = "Ability_DemonHunter_EyeBeam",
      builtFrom = "Demon's Bite, Chaos Strike",
      spentWith = "Eye Beam",
      resource = "Hunger",
      core = "Demon's Bite and Chaos Strike grant 1 Hunger, up to 5. Eye Beam consumes all Hunger; each stack " ..
          "makes it deal 12% more damage and last 0.2 sec longer.",
      loop = "Bite and Chaos Strike to feed, Eye Beam to unleash.",
      mods = {
        {
          name = "Ravenous",
          text = "Chaos Strike critical strikes grant 2 Hunger.",
        },
        {
          name = "Blade Frenzy",
          text = "Blade Dance grants 1 Hunger per 2 enemies it hits.",
        },
        {
          name = "Starving Demon",
          text = "Hunger stacks to 8, and Fel Rush grants 1 stack.",
        },
        {
          name = "Demonic Gaze",
          text = "The empowered Eye Beam leaves you in Metamorphosis for 4 sec.",
        },
        {
          name = "Chaos Burn",
          text = "Enemies hit by the empowered Eye Beam burn for 20% of its damage over 6 sec.",
        },
        {
          name = "Felblade Echo",
          text = "The empowered Eye Beam resets Felblade's cooldown.",
        },
        {
          name = "Vengeful Glide",
          text = "You can move at 50% speed while channeling the empowered Eye Beam.",
        },
        {
          name = "Fury Feast",
          text = "The empowered Eye Beam refunds 8 rage per stack consumed.",
        },
        {
          name = "Insatiable",
          text = "The empowered Eye Beam deals 30% more damage, but you take 10% more damage while " ..
              "channeling.",
        },
      },
      builds = {
        {
          name = "Pack shredder",
          mods = { "Blade Frenzy", "Chaos Burn", "Vengeful Glide" },
        },
        {
          name = "Burst demon",
          mods = { "Ravenous", "Demonic Gaze", "Insatiable" },
        },
        {
          name = "Endless fury",
          mods = { "Starving Demon", "Felblade Echo", "Fury Feast" },
        },
      },
    },
    { id = 20,
      name = "Rune of the Fierce Tiger",
      role = "Melee",
      icon = "Ability_Monk_RisingSunKick",
      builtFrom = "Tiger Palm",
      spentWith = "Rising Sun Kick",
      resource = "Tiger Spirit",
      core = "Tiger Palm grants 1 Tiger Spirit, up to 4. Rising Sun Kick consumes all stacks; each stack " ..
          "makes it deal 15% more damage, and at 4 it resets Fists of Fury's cooldown.",
      loop = "Tiger Palm to build, Rising Sun Kick, then a free Fists of Fury.",
      mods = {
        {
          name = "Combo Strikes",
          text = "Blackout Kick grants 1 Tiger Spirit if it follows a different ability.",
        },
        {
          name = "Crane's Whirl",
          text = "Spinning Crane Kick grants 1 stack per 3 enemies hit.",
        },
        {
          name = "Inner Peace",
          text = "Tiger Spirit stacks to 6; Rising Sun Kick's damage rises 8% per stack beyond 4.",
        },
        {
          name = "Rising Fury",
          text = "The empowered Rising Sun Kick makes its target take 10% more damage from you for 6 sec.",
        },
        {
          name = "Whirling Dragon",
          text = "The empowered Rising Sun Kick sends a shockwave hitting 3 nearby enemies for 40% damage.",
        },
        {
          name = "Touch of the Tiger",
          text = "The empowered Rising Sun Kick heals you for 20% of its damage.",
        },
        {
          name = "Flying Serpent",
          text = "The empowered Rising Sun Kick grants 40% movement speed for 3 sec.",
        },
        {
          name = "Ki Flow",
          text = "The empowered Rising Sun Kick refunds 10 energy per stack consumed.",
        },
        {
          name = "Tiger's Gambit",
          text = "The empowered Rising Sun Kick deals 25% more damage, but Tiger Palm costs 10 more energy.",
        },
      },
      builds = {
        {
          name = "Single-target striker",
          mods = { "Inner Peace", "Rising Fury", "Tiger's Gambit" },
        },
        {
          name = "Dungeon whirlwind",
          mods = { "Crane's Whirl", "Whirling Dragon", "Ki Flow" },
        },
        {
          name = "Self-sufficient",
          mods = { "Combo Strikes", "Touch of the Tiger", "Flying Serpent" },
        },
      },
    },
    { id = 21,
      name = "Rune of Fel Commandment",
      role = "Hybrid (Havoc, Demonology)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_FelCommandment",
      builtFrom = "Demon's Bite, Chaos Strike, Shadow Bolt, Incinerate, your demon's attacks",
      spentWith = "Throw Glaive (Chaos Bolt Volley)",
      resource = "Fel Command",
      core = "Demon's Bite, Chaos Strike, Shadow Bolt, Incinerate and your summoned demon's attacks grant " ..
          "Fel Command, up to 5. At 5, " ..
          "Throw Glaive becomes Chaos Bolt Volley: you hurl the glaive and 3 Chaos Bolts at up to 3 enemies, " ..
          "each bolt dealing 60% of a Chaos Bolt's damage. The volley consumes all Fel Command. In " ..
          "Metamorphosis, it fires 5 bolts and calls a Felguard to your side for 8 sec.",
      loop = "Fight beside your demon to build Fel Command, unleash the volley, and save a full volley for " ..
          "Metamorphosis to call in the Felguard.",
      mods = {
        {
          name = "Infernal Pact",
          text = "Your summoned demon's attacks grant Fel Command twice as fast.",
        },
        {
          name = "Demonic Ascension",
          text = "Entering Metamorphosis instantly grants 5 Fel Command.",
        },
        {
          name = "Dreadfire Discipline",
          text = "Shadow Bolt and Incinerate grant 2 Fel Command each; Demon's Bite grants none.",
        },
        {
          name = "Imp Legion",
          text = "Every Throw Glaive spends your Fel Command: a Fel Imp for each stack, fighting for 10 sec. " ..
              "Each volley bolt deals 10% more for every demon you have out.",
        },
        {
          name = "Shattered Glaive",
          text = "The glaive ricochets between the volley's targets, and each critical bolt reduces Throw " ..
              "Glaive's cooldown by 3 sec.",
        },
        {
          name = "Fel Host",
          text = "Every Throw Glaive spends your Fel Command: a demon for each stack, for 10 sec: Felguards " ..
              "in Metamorphosis, Fel Imps outside it. Each volley bolt deals 10% more for every demon you " ..
              "have out.",
        },
        {
          name = "Metamorphic Bond",
          text = "While you are in Metamorphosis, your demon takes 30% less damage and its attacks cleave.",
        },
        {
          name = "Fel Rift",
          text = "For 3 sec after the volley, Fel Rush carries you to the glaive's first target.",
        },
        {
          name = "Pact of Ruin",
          text = "The volley fires 2 extra bolts, but costs 3% of your maximum health per bolt.",
        },
      },
      builds = {
        {
          name = "Demon commander",
          mods = { "Infernal Pact", "Fel Host", "Metamorphic Bond" },
          text = "A host of demons answers every throw, and Metamorphosis makes them Felguards.",
        },
        {
          name = "Chaos artillery",
          mods = { "Dreadfire Discipline", "Shattered Glaive", "Pact of Ruin" },
          text = "A caster who builds with Shadow Bolt and fires a seven-bolt volley.",
        },
        {
          name = "Fel legion",
          mods = { "Demonic Ascension", "Imp Legion", "Fel Rift" },
          text = "Pop Metamorphosis, fire a five-bolt volley with a Felguard and imps behind it, and dash in.",
        },
      },
    },
    { id = 22,
      name = "Rune of the Sanctified Paragon",
      role = "Any (Consecration)",
      roles = { "DAMAGER", "HEALER", "TANK" },
      icon = "Ability_Rune_SanctifiedParagon",
      slots = { "Presence", "Manifestation", "Covenant" },
      builtFrom = "Consecration",
      spentWith = "Consecration (on and off)",
      resource = "Mana (1% of base a second)",
      core = "Consecration becomes a toggleable aura that follows you within 8 yards, dealing Holy damage every " ..
          "second at 40% of your normal Consecration damage. While active, it consumes 1% of your base mana " ..
          "each second and ends if you cannot pay it. It replaces your normal Consecration.",
      loop = "Choose what your sanctuary does: damage, healing or protection, each at the aura's reduced rate.",
      mods = {
        {
          name = "Boundless Sanctity",
          text = "Radius increases to 12 yards, but damage, healing and absorption are reduced by 25%.",
        },
        {
          name = "Hallowed Footsteps",
          text = "Aura radius becomes 4 yards. Moving leaves trails lasting 2 sec; they share the aura's ticks " ..
              "and never strike an enemy twice.",
        },
        {
          name = "Seat of the Paragon",
          text = "After standing still for 3 sec, the aura's output increases by 10%. Moving removes the bonus.",
        },
        {
          name = "Sanctified Condemnation",
          text = "Damage. Consecutive ticks increase that enemy's damage taken from your Consecration by 2%, up " ..
              "to 10%. Stacks expire after 2 sec outside the aura.",
        },
        {
          name = "Merciful Ground",
          text = "Healing. Consecration no longer damages enemies: each pulse heals the single most injured party " ..
              "member within range.",
        },
        {
          name = "Shelter of the Paragon",
          text = "Protection. Consecration no longer damages enemies: every 5 sec it shields one nearby party " ..
              "member. Only one shield from your aura at a time.",
        },
        {
          name = "Bastion of Faith",
          text = "The aura generates 50% more threat. Only with Sanctified Condemnation.",
        },
        {
          name = "Weight of the Unworthy",
          text = "After 3 consecutive damaging ticks, an enemy is slowed by 15% until it leaves the aura. Only " ..
              "with Sanctified Condemnation.",
        },
        {
          name = "Measured Devotion",
          text = "Reduces the aura's mana cost by 25%, but reduces its output by 10%.",
        },
      },
      builds = {
        {
          name = "The Living Sanctuary",
          mods = { "Boundless Sanctity", "Merciful Ground", "Measured Devotion" },
          text = "Wider single-target healing with reduced output and lower upkeep.",
        },
        {
          name = "The Immovable Paragon",
          mods = { "Seat of the Paragon", "Sanctified Condemnation", "Bastion of Faith" },
          text = "Stationary damage and increased threat; no passive mitigation.",
        },
        {
          name = "The Hallowed March",
          mods = { "Hallowed Footsteps", "Sanctified Condemnation", "Weight of the Unworthy" },
          text = "Narrower coverage, short trails, and a delayed slow.",
        },
      },
    },
    { id = 23,
      name = "Rune of the Stormbound Conduit",
      role = "DPS (Lightning Bolt)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_StormboundConduit",
      slots = { "Arcs", "Lightning Shield", "Surge" },
      builtFrom = "Lightning Bolt",
      spentWith = "Arcs (automatic)",
      resource = "None",
      core = "Lightning Bolt arcs to 2 enemies within 10 yards of its target for 30% of its damage each. While " ..
          "Lightning Shield is up, its retaliations arc to 1 nearby enemy for 50% of their damage. Arcs never arc " ..
          "again.",
      loop = "Cast Lightning Bolt. The storm does the rest.",
      mods = {
        { name = "Forked Tempest",
          text = "Lightning Bolt arcs to 3 enemies." },
        { name = "Closed Circuit",
          text = "Lightning Bolt's arc strikes its own target again for 60% instead." },
        { name = "Faraday Field",
          text = "Arcs reach 15 yards and deal 10% more." },
        { name = "Live Wire",
          text = "Lightning Shield's retaliations arc to 2 enemies." },
        { name = "Storm Within",
          text = "Lightning Shield strikes the nearest enemy within 8 yards every 3 sec in combat." },
        { name = "Distant Conductor",
          text = "Lightning Bolt also fires a Lightning Shield retaliation at its target." },
        { name = "Chain Surge",
          text = "Each Chain Lightning hit arcs to 1 more enemy for 20% of it." },
        { name = "Static Ward",
          text = "Each arc shields you for 1% of your maximum health, up to 10%, for 8 sec." },
        { name = "Overcharge",
          text = "Arcs from a critical strike deal 50% more." },
      },
      builds = {
        { name = "The Thunderhead", mods = { "Closed Circuit", "Distant Conductor", "Overcharge" },
          text = "Pour every bolt into one enemy." },
        { name = "The Wildfire Storm", mods = { "Forked Tempest", "Live Wire", "Chain Surge" },
          text = "Light up the whole pack." },
        { name = "The Lightning Rod", mods = { "Faraday Field", "Storm Within", "Static Ward" },
          text = "Fight up close, shielded by the storm." },
      },
    },
    { id = 24,
      name = "Rune of Echoing Thunder",
      role = "Tank (Thunder Clap, Shield Slam)",
      roles = { "TANK" },
      icon = "Ability_Rune_EchoingThunder",
      slots = { "Charge", "Echo", "Resonance" },
      builtFrom = "Thunder Clap",
      spentWith = "Shield Slam",
      resource = "Thunder Charge",
      core = "Thunder Clap charges you with thunder for 8 sec. Your next Shield Slam releases a Thunder Echo: 30% " ..
          "of its damage as Nature damage to up to 5 enemies within 6 yards of its target. Needs a shield.",
      loop = "Thunder Clap, then Shield Slam. Every time.",
      mods = {
        { name = "Double Charge",
          text = "The charge holds 2 Thunder Echoes." },
        { name = "Lingering Charge",
          text = "The charge lasts 20 sec." },
        { name = "Commanding Thunder",
          text = "Thunder Echo generates twice the threat." },
        { name = "Focused Peal",
          text = "Thunder Echo strikes only Shield Slam's target, for 60% more." },
        { name = "Distant Rumble",
          text = "Thunder Echo reaches 10 yards around the target." },
        { name = "Deafening Impact",
          text = "Enemies hit by Thunder Echo are slowed by 30% for 3 sec." },
        { name = "Sheltering Reverberation",
          text = "Thunder Echo shields you for 3% of your maximum health per enemy hit (up to 15%) for 6 sec." },
        { name = "Battle Rhythm",
          text = "Thunder Echo grants 2 rage per enemy hit." },
        { name = "Rolling Thunder",
          text = "Each enemy Thunder Echo hits takes 1 sec off Thunder Clap's cooldown." },
      },
      builds = {
        { name = "The Raid Wall", mods = { "Double Charge", "Focused Peal", "Sheltering Reverberation" },
          text = "Hit one boss hard and stay shielded." },
        { name = "The Pack Puller", mods = { "Commanding Thunder", "Distant Rumble", "Rolling Thunder" },
          text = "Grab every enemy and keep the thunder rolling." },
        { name = "The Storm Engine", mods = { "Lingering Charge", "Deafening Impact", "Battle Rhythm" },
          text = "Slow everything and never run out of rage." },
      },
    },
    { id = 25,
      name = "Rune of the Wellspring",
      role = "Healer (Chain Heal)",
      roles = { "HEALER" },
      icon = "Ability_Rune_Wellspring",
      slots = { "Spring", "Earth", "Undertow" },
      builtFrom = "Chain Heal",
      spentWith = "The extra jump (automatic)",
      resource = "None",
      core = "Chain Heal jumps once more from its last target, to the most injured ally within 12.5 yards it did " ..
          "not heal, for 40% of its first heal. Chain Heal also heals the wearer of your Earth Shield for 20% more.",
      loop = "Cast Chain Heal: it always reaches one more ally.",
      mods = {
        { name = "Split Tributary",
          text = "The extra jump heals 2 allies, 25% each." },
        { name = "Deep Current",
          text = "The extra jump heals 40% more an ally below half health." },
        { name = "Concentrated Current",
          text = "The extra jump heals 20% more." },
        { name = "Flowing Waters",
          text = "Earth Shield's heals also heal the most injured ally near its wearer for half as much." },
        { name = "Bedrock",
          text = "Chain Heal heals your Earth Shield wearer for 40% more instead of 20%." },
        { name = "Renewed Shield",
          text = "Chain Heal on your Earth Shield wearer restores a charge." },
        { name = "Tidal Shelter",
          text = "The extra jump also shields its ally for 20% of the heal for 4 sec." },
        { name = "Renewing Current",
          text = "The extra jump also heals its ally for 20% more over 4 sec." },
        { name = "Long Reach",
          text = "The extra jump reaches 20 yards." },
      },
      builds = {
        { name = "The Flood", mods = { "Split Tributary", "Flowing Waters", "Renewing Current" },
          text = "Wash over the whole group." },
        { name = "The Lifeline", mods = { "Deep Current", "Bedrock", "Tidal Shelter" },
          text = "Keep the tank and the one in trouble up." },
        { name = "The Long River", mods = { "Concentrated Current", "Renewed Shield", "Long Reach" },
          text = "Reach anyone, anywhere." },
      },
    },
    { id = 26,
      name = "Rune of the Wandering Mist",
      role = "Healer (Soothing Mist)",
      roles = { "HEALER" },
      icon = "Ability_Rune_WanderingMist",
      slots = { "Mistflow", "Technique", "Approach" },
      builtFrom = "Soothing Mist's ticks",
      spentWith = "Wandering Mist, Enveloping Mist and Vivify",
      resource = "None",
      core = "Each Soothing Mist tick also sends a Wandering Mist to your most injured ally within 20 yards of its " ..
          "target, healing for 25% of the tick. Enveloping Mist and Vivify on your Soothing Mist target are cast at " ..
          "once and keep the channel.",
      loop = "Channel Soothing Mist on the one who needs it most; the mist finds the next.",
      mods = {
        { name = "Flowing Currents",
          text = "Wandering Mist reaches 30 yards." },
        { name = "Drifting Mist",
          text = "Wandering Mist heals 2 allies, 15% each." },
        { name = "Jade Shelter",
          text = "Wandering Mist also shields its ally for 10% of the heal." },
        { name = "Comforting Veil",
          text = "Enveloping Mist also wraps your most injured ally nearby in a lesser one, 30% as strong." },
        { name = "Vivacious Flow",
          text = "Vivify also heals your most injured ally nearby for 30% of it." },
        { name = "Peaceful Focus",
          text = "Soothing Mist heals 15% more on a target with your Enveloping Mist." },
        { name = "Effortless Breathing",
          text = "Soothing Mist costs 25% less mana." },
        { name = "Lingering Mist",
          text = "When your channel ends, its target is healed for 2 more ticks over 4 sec." },
        { name = "Deep Breath",
          text = "Each Wandering Mist restores 0.3% of your maximum mana." },
      },
      builds = {
        { name = "The Mistwalker", mods = { "Flowing Currents", "Vivacious Flow", "Deep Breath" },
          text = "Heal everyone, never run dry." },
        { name = "The Jade Wall", mods = { "Jade Shelter", "Peaceful Focus", "Lingering Mist" },
          text = "Keep one ally standing no matter what." },
        { name = "The Gentle Rain", mods = { "Drifting Mist", "Comforting Veil", "Effortless Breathing" },
          text = "Spread the mist thin and wide." },
      },
    },
    { id = 27,
      name = "Rune of the Threefold Grave",
      role = "Melee DPS or Tank (Icy Touch, Plague Strike, Blood Strike)",
      roles = { "DAMAGER", "TANK" },
      icon = "Ability_Rune_ThreefoldGrave",
      slots = { "Burial Grounds", "Gravekeeper", "Shared Burial" },
      builtFrom = "Icy Touch, Plague Strike and Blood Strike",
      spentWith = "Your Graves",
      resource = "None: one Grave of each kind",
      core = "Icy Touch lays a Frozen Grave beneath its target (Frost damage and a 30% slow), Plague Strike a " ..
          "Plagued Grave (Shadow damage), Blood Strike a Crimson Grave beneath you (damage): one of each, 8 sec; " ..
          "another replaces it. Enemies standing in two or more take 20% more from your Graves (Shared Burial). " ..
          "Blood Strike landed inside your Crimson Grave keeps it where it is and grants a shield of 5% of your " ..
          "health, up to 15% (once every 3 sec; the strike that lays the Grave grants it at once).",
      loop = "Against a boss, set your Crimson Grave with Blood Strike, then lay the Frozen and Plagued Graves " ..
          "beneath it and fight from the overlap. Against a pack, frost across its approach, disease where it " ..
          "gathers, blood where you stand.",
      mods = {
        { name = "Gathered Dead",
          text = "Icy Touch and Plague Strike lay their Graves on your Crimson Grave when it is within 10 yards of " ..
              "the struck enemy." },
        { name = "March of the Dead",
          text = "Your Crimson Grave follows you. Frozen and Plagued Graves stay where they are laid." },
        { name = "Far-Reaching Graves", text = "Frozen and Plagued Graves are 50% wider." },
        { name = "Cruel Winter",
          text = "The Frozen Grave slows by 50%, and enemies leaving it stay slowed for 3 sec." },
        { name = "Grave Pestilence",
          text = "The Plagued Grave infects the enemies it damages with a short disease; further hits refresh it." },
        { name = "Crimson Bastion",
          text = "The Crimson Grave's shield holds up to 25% of your health, and stays when you leave the Grave." },
        { name = "Deep Burial", text = "Shared Burial raises your Graves' damage by 35% instead of 20%." },
        { name = "Spreading Death",
          text = "Enemies leaving an overlap keep Shared Burial for 3 sec in any of your Graves." },
        { name = "Hallowed by Nothing",
          text = "Blood Strike landed while you stand in your Crimson Grave and another Grave extends your Graves " ..
              "by 2 sec, each once." },
      },
      builds = {
        { name = "The Killing Floor", mods = { "Gathered Dead", "Grave Pestilence", "Deep Burial" },
          text = "Stack every Grave on one spot and keep the boss buried in it." },
        { name = "The Moving Crypt", mods = { "March of the Dead", "Crimson Bastion", "Spreading Death" },
          text = "Carry your circle through the fight and keep your shield with you." },
        { name = "The Long Vigil", mods = { "Far-Reaching Graves", "Cruel Winter", "Hallowed by Nothing" },
          text = "Hold wide ground, slow what comes and keep your Graves standing." },
      },
    },
    { id = 28,
      name = "Rune of the Unbroken Onslaught",
      role = "Melee DPS (Mortal Strike or Bloodthirst)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_UnbrokenOnslaught",
      slots = { "Wake", "Follow-Through", "Pursuit" },
      builtFrom = "Mortal Strike or Bloodthirst",
      spentWith = "Any other melee ability",
      resource = "None: your Weapon Wake",
      core = "Mortal Strike or Bloodthirst cuts a Weapon Wake through its target: a short slash left in the " ..
          "battlefield for 6 sec. Your next different melee ability striking an enemy in that wake reopens it: " ..
          "another attack along its shape a moment later, 60% of a weapon swing to each enemy in it (up to 6), " ..
          "its first target 25% more. A wake reopens once; a new one replaces it, and the wake's own attacks " ..
          "never cut or reopen one.",
      loop = "Weave different melee attacks through your weapon's path: aim a narrow wake at a priority enemy, " ..
          "sweep a pack with a crescent, or lay a ground slash for enemies to walk into.",
      mods = {
        { name = "Executioner's Cut",
          text = "A narrow cleft through your target: its first target takes 50% more instead of 25%." },
        { name = "Whirling Steel", text = "A broad crescent in front of you, 8 yards, for cleaving nearby enemies." },
        { name = "Raking Passage",
          text = "A longer ground slash, reaching 14 yards past your target to the enemies behind it." },
        { name = "Overpowering Echo", text = "Reopening the wake delivers its attack at once." },
        { name = "Bloodletting",
          text = "Enemies the wake's attack hits bleed for 30% of it over 6 sec, refreshed, never more than 60% " ..
              "of a swing." },
        { name = "Aftershock",
          text = "Reopening also sets off a second impact in the same place 1.5 sec later, at half strength." },
        { name = "Relentless", text = "Your wake follows its first target." },
        { name = "Battlefield Claim", text = "Your wake stays where it was cut and lasts 10 sec instead of 6." },
        { name = "Changing Prey",
          text = "Striking another enemy through the wake gives it the first target's bonus." },
      },
      builds = {
        { name = "The Headsman", mods = { "Executioner's Cut", "Overpowering Echo", "Relentless" },
          text = "Keep a narrow wake on one enemy and cash it in at once." },
        { name = "The Reaping Line", mods = { "Whirling Steel", "Bloodletting", "Changing Prey" },
          text = "Sweep a pack and leave every enemy in it bleeding." },
        { name = "The Prepared Field", mods = { "Raking Passage", "Aftershock", "Battlefield Claim" },
          text = "Cut a long slash ahead of the pull and let the enemies come to it." },
      },
    },
    { id = 29,
      name = "Rune of the Dawnbound Crusader",
      role = "Healer or Melee DPS (Crusader Strike, Holy Shock)",
      roles = { "HEALER", "DAMAGER" },
      icon = "Ability_Rune_DawnboundCrusader",
      slots = { "Echo", "Light", "Grace" },
      builtFrom = "Crusader Strike, Holy Shock",
      spentWith = "Dawn Echoes (automatic)",
      resource = "None",
      core = "Crusader Strike and Holy Shock send a Dawn Echo a moment later: half of their damage again on the " ..
          "same enemy, or half of Holy Shock's healing on your most injured ally within 15 yards. A Healer's " ..
          "Crusader Strike echoes as a heal.",
      loop = "Use Crusader Strike and Holy Shock on cooldown: every one lands twice.",
      mods = {
        { name = "Sunburst",
          text = "Each echo also splashes up to 5 others within 8 yards for 40% of it." },
        { name = "Focused Dawn",
          text = "Your echoes are 40% stronger." },
        { name = "Communion",
          text = "Every echo both strikes and heals, at 35% each." },
        { name = "Sacred Ground",
          text = "Each echo leaves consecrated ground for 4 sec, a quarter of it each second." },
        { name = "Steadfast",
          text = "Holy Shock echoes twice; the second at half strength." },
        { name = "Judgment's Echo",
          text = "Judgement sends a Dawn Echo too." },
        { name = "Dawn's Mercy",
          text = "Your damaging echoes heal you for 20% of their damage." },
        { name = "Swift Dawn",
          text = "Each echo takes 0.5 sec off Crusader Strike and Holy Shock's cooldowns." },
        { name = "Radiant Ward",
          text = "Your healing echoes also shield their target for 25% of the heal for 8 sec." },
      },
      builds = {
        { name = "The Dawnblade", mods = { "Focused Dawn", "Judgment's Echo", "Swift Dawn" },
          text = "Hit twice, hit often." },
        { name = "The Morning Prayer", mods = { "Sunburst", "Steadfast", "Radiant Ward" },
          text = "Heal the whole group with every Holy Shock." },
        { name = "The Crusader's Path", mods = { "Communion", "Sacred Ground", "Dawn's Mercy" },
          text = "Fight and heal at once." },
      },
    },
    { id = 30,
      name = "Rune of the Hollow Congregation",
      role = "Ranged DPS (Mind Flay, Mind Blast)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_HollowCongregation",
      slots = { "Congregation", "Sermon", "Command" },
      builtFrom = "Mind Flay",
      spentWith = "Mind Blast",
      resource = "Hollow Echoes, up to 3",
      core = "Mind Flay calls a Hollow Echo to your side for 10 sec (3 at most): it follows you and channels a " ..
          "weaker Mind Flay on your target (40% of a tick). Mind Blast turns every echo on its target in a barrage " ..
          "of 3 bolts, 60% of a tick each.",
      loop = "Flay to gather your echoes, then Mind Blast to unleash them.",
      mods = {
        { name = "Gathering",
          text = "You can have 4 Hollow Echoes." },
        { name = "Lasting Echoes",
          text = "Your echoes last 15 sec." },
        { name = "Last Word",
          text = "An echo fading away strikes its target for three ticks at once." },
        { name = "Unison",
          text = "The barrage lands all together and deals 20% more." },
        { name = "Dissonance",
          text = "Your echoes turn on separate enemies with your Shadow Word: Pain, your target first." },
        { name = "Accusation",
          text = "The barrage becomes a scream from each echo through up to 5 enemies on the way." },
        { name = "Fervor",
          text = "Your echoes deal 25% more while you channel Mind Flay." },
        { name = "Shadow Mend",
          text = "Your echoes heal you for 10% of the damage they deal." },
        { name = "Swift Command",
          text = "Mind Blast comes back 0.5 sec sooner for each echo it commands." },
      },
      builds = {
        { name = "The Choir", mods = { "Gathering", "Unison", "Swift Command" },
          text = "Four echoes, one voice, again and again." },
        { name = "The Plague of Whispers", mods = { "Lasting Echoes", "Dissonance", "Shadow Mend" },
          text = "Spread your echoes over every enemy you have cursed." },
        { name = "The Last Rites", mods = { "Last Word", "Accusation", "Fervor" },
          text = "Line them up and scream them down." },
      },
    },
    { id = 31,
      name = "Rune of the Shattered Sun",
      role = "Ranged DPS (fire and frost spells)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_ShatteredSun",
      slots = { "Formation", "Shatter", "Sunfall" },
      builtFrom = "Fireball, Pyroblast, Fire Blast or Scorch",
      spentWith = "Frostbolt, Ice Lance, Frost Nova or Cone of Cold",
      resource = "Sunshards in your target, up to 5",
      core = "Your fire spells embed a burning Sunshard in their target, up to 5. Your next frost spell on it " ..
          "shatters them all: a burst of 25% of its damage per shard to every enemy within 8 yards.",
      loop = "Fire, fire, fire, then frost: the more shards you build, the bigger the burst.",
      mods = {
        { name = "Wildfire",
          text = "Fireball and Pyroblast embed 2 shards." },
        { name = "Spreading Sun",
          text = "Fireball and Pyroblast also embed a shard in 2 enemies near their target." },
        { name = "Heavy Sun",
          text = "Your target can hold up to 8 shards." },
        { name = "Wide Burst",
          text = "The burst reaches 12 yards." },
        { name = "Scalding Ground",
          text = "The burst leaves burning ground for 4 sec, 15% of it each second." },
        { name = "Chain Reaction",
          text = "Shards in other enemies within 10 yards shatter too, at half strength." },
        { name = "Implosion",
          text = "The burst deals 40% more to your target." },
        { name = "Second Sunrise",
          text = "The burst flares again 2 sec later for 40% of it." },
        { name = "Frozen Core",
          text = "Enemies hit by the burst are slowed by 40% for 3 sec." },
      },
      builds = {
        { name = "The Dying Star", mods = { "Heavy Sun", "Wide Burst", "Implosion" },
          text = "Stack 8 shards and break them on one foe." },
        { name = "The Burning Field", mods = { "Spreading Sun", "Chain Reaction", "Second Sunrise" },
          text = "Seed a whole pack and set it off at once." },
        { name = "The Cold Sun", mods = { "Wildfire", "Scalding Ground", "Frozen Core" },
          text = "Burn and slow everything near you." },
      },
    },
}
