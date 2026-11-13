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
}
