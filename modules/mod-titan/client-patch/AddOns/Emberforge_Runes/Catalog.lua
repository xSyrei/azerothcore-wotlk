-- The Legendary Runes the server builds (src/runes.cpp numbers them; each from its own design doc). The first draft's
-- twenty placeholder runes are gone. Modifiers: 3 per slot, in slot order; keywords: words the panel's search also
-- finds a rune by. retired: taken out while its design is reworked (src/runes.cpp, RETIRED_RUNES): not listed.
EmberforgeRunesCatalog = {
    { id = 21,
      name = "Rune of Fel Commandment",
      retired = true,
      role = "Hybrid (Havoc, Demonology)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_FelCommandment",
      keywords = "demon hunter warlock fel chaos fire shadow demons pet volley glaive",
      slots = { "Invocation", "Manifestation", "Adaptation" },
      builtFrom = "Demon's Bite, Chaos Strike, Shadow Bolt, Incinerate, your demon's attacks",
      spentWith = "Throw Glaive (Chaos Bolt Volley)",
      resource = "Fel Command",
      core = "Demon's Bite, Chaos Strike, Shadow Bolt, Incinerate and your summoned demon's attacks grant Fel " ..
          "Command, up to 5. At 5, Throw Glaive becomes Chaos Bolt Volley: you hurl the glaive and 3 Chaos Bolts at " ..
          "up to 3 enemies, each bolt dealing 60% of a Chaos Bolt's damage. The volley consumes all Fel Command.",
      loop = "Build to 5 Fel Command, then throw your glaive.",
      mods = {
        { name = "Infernal Pact",
          text = "Your summoned demon's attacks grant Fel Command twice as fast." },
        { name = "Demonic Ascension",
          text = "Entering Metamorphosis instantly grants 5 Fel Command." },
        { name = "Dreadfire Discipline",
          text = "Shadow Bolt and Incinerate grant 2 Fel Command each." },
        { name = "Barrage",
          text = "The volley fires 2 more bolts." },
        { name = "Shattered Glaive",
          text = "Each critical bolt takes 3 sec off Throw Glaive's cooldown." },
        { name = "Felguard's Call",
          text = "The volley calls a Felguard to your side for 8 sec." },
        { name = "Metamorphic Bond",
          text = "While you are in Metamorphosis, your demon takes 30% less damage and its attacks cleave." },
        { name = "Imp Legion",
          text = "The volley calls a Fel Imp for each bolt, fighting for 10 sec." },
        { name = "Pact of Ruin",
          text = "The volley fires 2 more bolts, but costs 3% of your maximum health per bolt." },
      },
      builds = {
        { name = "The Fel Storm", mods = { "Dreadfire Discipline", "Barrage", "Pact of Ruin" },
          text = "Seven bolts, every throw." },
        { name = "The Legion's Master", mods = { "Infernal Pact", "Felguard's Call", "Imp Legion" },
          text = "Fight beside an army of demons." },
        { name = "The Demon Within", mods = { "Demonic Ascension", "Shattered Glaive", "Metamorphic Bond" },
          text = "Transform and throw again and again." },
      },
    },
    { id = 22,
      name = "Rune of the Sanctified Paragon",
      role = "Any (Consecration)",
      roles = { "DAMAGER", "HEALER", "TANK" },
      icon = "Ability_Rune_SanctifiedParagon",
      keywords = "paladin holy consecration ground light aura judgment holy shock shield of righteousness charge " ..
          "divine shield flash heal",
      slots = { "Presence", "Manifestation", "Covenant" },
      builtFrom = "Consecration",
      spentWith = "Consecration (on and off)",
      resource = "Mana (1% of base a second)",
      core = "Consecration becomes a toggleable aura that follows you within 8 yards, dealing Holy damage every " ..
          "second at 40% of your normal Consecration damage. While active, it consumes 1% of your base mana each " ..
          "second and ends if you cannot pay it. It replaces your normal Consecration. Judgment on an enemy in the " ..
          "aura adds half a tick of Holy damage, once every 6 sec. Holy Shock healing an injured ally in it heals " ..
          "another injured ally within 8 yards for 15% as much, once every 8 sec. Shield of Righteousness on an " ..
          "enemy in it shields you for 3% of your maximum health for 4 sec, once every 8 sec.",
      loop = "Choose what your sanctuary does: damage, healing or protection, each at the aura's reduced rate.",
      mods = {
        {
          name = "Boundless Sanctity",
          text = "The aura reaches 12 yards instead of 8, but its damage, healing and shields are 25% " ..
              "lower.",
        },
        {
          name = "Hallowed Footsteps",
          text = "The aura shrinks to 4 yards, but moving leaves sanctified footsteps for 2 sec that tick " ..
              "like the aura. An enemy is hit once a tick, however many it stands in.",
        },
        {
          name = "Seat of the Paragon",
          text = "Standing still for 3 sec raises the aura's damage, healing and shields by 10% until you " ..
              "move.",
        },
        { name = "Consecrated Charge",
          text = "After Charge, the aura reaches 4 yards farther for 4 sec and its next tick is 15% stronger, once " ..
              "every 12 sec." },
        { name = "Sanctuary March",
          text = "Divine Shield while the aura is on makes its next 3 seconds cost no mana, once every 60 sec." },
        {
          name = "Sanctified Condemnation",
          text = "Damage: each tick on an enemy raises its damage taken from your aura by 2%, up to 10%. " ..
              "Lost 2 sec after it leaves the aura.",
        },
        {
          name = "Merciful Ground",
          text = "Healing: the aura no longer deals damage. Each second it heals your most injured party " ..
              "member in range.",
        },
        {
          name = "Shelter of the Paragon",
          text = "Protection: the aura no longer deals damage. Every 5 sec it shields a nearby party " ..
              "member for 5 ticks' worth. One shield at a time.",
        },
        { name = "Radiant Judgment",
          text = "Damage: Judgment's bonus in the aura is a whole tick instead of half. The aura keeps dealing " ..
              "damage." },
        { name = "Healing Light",
          text = "Healing: the aura no longer deals damage; each second it heals your most injured party member in " ..
              "range for 45% of a tick, and your Flash Heal on that ally heals 30% more, once every 8 sec." },
        {
          name = "Bastion of Faith",
          text = "The aura's damage generates 50% more threat. Needs a damaging Manifestation (Sanctified " ..
              "Condemnation or Radiant Judgment).",
        },
        {
          name = "Weight of the Unworthy",
          text = "An enemy hit by 3 ticks in a row has its movement speed reduced by 15% while it stays in " ..
              "the aura. Needs a damaging Manifestation (Sanctified Condemnation or Radiant Judgment).",
        },
        {
          name = "Measured Devotion",
          text = "The aura costs 0.75% of your base mana a second instead of 1%, but its damage, healing " ..
              "and shields are 10% lower.",
        },
        { name = "Dawnward",
          text = "Holy Shock's second heal also shields its ally for 20% of that heal for 4 sec, once every 8 sec." },
        { name = "Paragon's Aegis",
          text = "Shield of Righteousness's shield in the aura is 4% of your maximum health instead of 3%." },
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
      role = "DPS (Lightning Bolt, Chain Lightning)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_StormboundConduit",
      keywords = "shaman lightning nature electric arc chain enhancement elemental chain lightning conductive arc " ..
          "earth shock frostbolt flame shock lava lash starfire chain heal thunder clap",
      slots = { "Conductor", "Circuit", "Shield" },
      builtFrom = "Lightning Bolt",
      spentWith = "Chain Lightning",
      resource = "Conductive on your enemies, 6 sec",
      core = "Lightning Bolt leaves Conductive on its target for 6 sec. Chain Lightning hitting a Conductive enemy " ..
          "sends one arc to another enemy within 10 yards for 30% of that Lightning Bolt's damage, once a cast; " ..
          "with no other enemy near, it arcs back into the first for half. A Lightning Shield retaliation arcs to " ..
          "another Conductive enemy within 10 yards for 50% of its damage, once every 3 sec. Earth Shock on a " ..
          "Conductive enemy arcs 20% of your spell power to another (half back into it if alone), once every 8 sec. " ..
          "Arcs never arc again.",
      loop = "Lightning Bolt to make them Conductive; Chain Lightning to arc it.",
      mods = {
        { name = "Persistent Current",
          text = "Conductive lasts 12 sec, and the first Chain Lightning arc on each one deals 15% more damage." },
        { name = "Branching Current",
          text = "Lightning Bolt also leaves Conductive on one enemy within 10 yards of its target." },
        { name = "Live Wire",
          text = "Lightning Shield retaliations deal 15% more damage and leave Conductive on the attacker. Not with " ..
              "Storm Within." },
        { name = "Icy Conductor",
          text = "Frostbolt leaves Conductive too, and deals 15% more to a Conductive enemy." },
        { name = "Flame Conductor",
          text = "Lightning Bolt on your Flame Shock target also leaves Conductive on one enemy within 10 yards and " ..
              "makes Flame Shock last 2 sec longer (up to 6 sec for each cast)." },
        { name = "Forked Tempest",
          text = "Chain Lightning's arc strikes 2 other enemies for 65% each." },
        { name = "Closed Circuit",
          text = "Chain Lightning's arc returns into the Conductive enemy for its whole damage." },
        { name = "Faraday Field",
          text = "Arcs reach 15 yards, and the first Chain Lightning arc every 8 sec deals 20% more damage." },
        { name = "Storm Lash",
          text = "Lava Lash on a Conductive enemy arcs 25% of your attack power as Nature damage to another enemy " ..
              "(half back into it if alone), once every 6 sec." },
        { name = "Starbound Circuit",
          text = "Starfire on a Conductive enemy arcs 25% of your spell power as Arcane damage to another enemy " ..
              "(half back into it if alone), once every 8 sec." },
        { name = "Storm Within",
          text = "Every 3 sec in combat, your Lightning Shield strikes the nearest Conductive enemy within 8 yards, " ..
              "spending a charge. Not with Live Wire." },
        { name = "Violent Reprisal",
          text = "Your Lightning Shield's arcs deal 40% more damage." },
        { name = "Distant Conductor",
          text = "Lightning Bolt also strikes its target with your Lightning Shield, spending a charge, once every " ..
              "3 sec." },
        { name = "Storm's Eye",
          text = "Chain Heal makes your next Lightning Bolt within 6 sec deal 15% more damage." },
        { name = "Shocking Surge",
          text = "Thunder Clap on a Conductive enemy deals 20% of your spell power as Nature damage to up to 5 " ..
              "enemies within 8 yards of it, once every 8 sec." },
      },
      builds = {
        { name = "The Tempest", mods = { "Persistent Current", "Forked Tempest", "Violent Reprisal" },
          text = "Arcs through the whole pack." },
        { name = "The Closed Loop", mods = { "Branching Current", "Closed Circuit", "Distant Conductor" },
          text = "Every bolt comes back for more." },
        { name = "The Wide Storm", mods = { "Live Wire", "Faraday Field", "Shocking Surge" },
          text = "A storm that reaches far." },
      },
    },
    { id = 24,
      name = "Rune of Echoing Thunder",
      role = "Tank (Thunder Clap, Shield Slam)",
      roles = { "TANK" },
      icon = "Ability_Rune_EchoingThunder",
      keywords = "warrior protection thunder physical shield aoe thunderfield revenge earth shock lightning shield " ..
          "consecration flame shock icy touch frost fever earth shield",
      slots = { "Field", "Echo", "Resonance" },
      builtFrom = "Thunder Clap (the Thunderfield)",
      spentWith = "Shield Slam",
      resource = "The Thunderfield, 16 sec",
      core = "Thunder Clap lays a Thunderfield where you stand for 16 sec (8 yards about it). Standing in it, Shield " ..
          "Slam releases a Thunder Echo in a cone 10 yards toward its target: 42% of the slam's damage before armor " ..
          "as Nature damage to up to 5 enemies, once every 2 sec. Revenge in the field makes the next echo deal 20% " ..
          "more (6 sec). Earth Shock on an enemy in the field also deals 20% of your attack power as Nature damage " ..
          "to up to 4 enemies within 6 yards of it, once every 8 sec.",
      loop = "Thunder Clap where you hold the pack; Shield Slam from inside the field.",
      mods = {
        { name = "Lingering Thunder",
          text = "Your Thunderfield lasts 19 sec, and its first echo deals 15% more damage." },
        { name = "March of the Storm",
          text = "Your Thunderfield goes with you, and the echo deals 10% more damage." },
        { name = "Rolling Front",
          text = "Thunder Clap lays the field under your enemy within 10 yards instead, and the echo deals 20% more " ..
              "damage." },
        { name = "Stormheart",
          text = "Your Thunderfield goes with you; with Lightning Shield up, the echo also deals 20% of your spell " ..
              "power as Nature damage to the slam's target." },
        { name = "Consecrated Storm",
          text = "Standing in your Consecration too, the echo deals 20% more damage and 30% more threat." },
        { name = "Reverberating Impact",
          text = "The echo bursts about the slam's target instead: up to 5 enemies within 6 yards of it." },
        { name = "Focused Peal",
          text = "The echo strikes only the slam's target, for 60% more damage." },
        { name = "Distant Rumble",
          text = "The echo's cone reaches 15 yards, and enemies beyond 10 yards take 30% more." },
        { name = "Flame Echo",
          text = "The echo on your Flame Shock target burns it for 30% of the echo's damage over 6 sec." },
        { name = "Frost Echo",
          text = "The echo on your Frost Fever target also deals 20% of your attack power as Frost damage and slows " ..
              "it by 25% for 3 sec." },
        { name = "Commanding Thunder",
          text = "The echo causes twice the threat, and deals 10% more to the slam's target." },
        { name = "Sheltering Reverberation",
          text = "The echo shields you for 3% of your maximum health for each enemy it hits (15% at most) for 6 " ..
              "sec." },
        { name = "Deafening Impact",
          text = "The echo slows its enemies by 30% for 3 sec, and deals 20% more to enemies slowed by your Thunder " ..
              "Clap." },
        { name = "Revenge of the Storm",
          text = "Revenge's echo deals 40% more instead of 20%, and gives you 10 Rage, once every 8 sec." },
        { name = "Tidal Shield",
          text = "An echo while your Earth Shield is on you shields you for 30% of its last heal for 4 sec, once " ..
              "every 8 sec." },
      },
      builds = {
        { name = "The Storm Wall", mods = { "March of the Storm", "Reverberating Impact", "Sheltering Reverberation" },
          text = "The field goes where you hold the line." },
        { name = "The Hammer", mods = { "Lingering Thunder", "Focused Peal", "Commanding Thunder" },
          text = "Every echo on the one that matters." },
        { name = "The Deafening Front", mods = { "Rolling Front", "Distant Rumble", "Deafening Impact" },
          text = "Thunder rolls across the whole pack." },
      },
    },
    { id = 25,
      name = "Rune of the Wellspring",
      role = "Healer (Chain Heal, Earth Shield)",
      roles = { "HEALER" },
      icon = "Ability_Rune_Wellspring",
      keywords = "shaman restoration water nature chain heal earth shield riptide renew holy shock soothing mist " ..
          "power word shield wellspring release",
      slots = { "Spring", "Current", "Undertow" },
      builtFrom = "Chain Heal on your Earth Shield bearer",
      spentWith = "Your next Chain Heal there",
      resource = "The Wellspring, 18 sec",
      core = "Chain Heal healing your Earth Shield bearer lays a Wellspring on them for 18 sec, and heals them 20% " ..
          "more. A later Chain Heal reaching the Wellspring releases one more heal: 40% of what it healed the " ..
          "bearer for, to the most injured other ally within 12.5 yards (if there is none, 20% to the bearer), once " ..
          "a cast. Earth Shield healing the bearer renews the Wellspring; Riptide healing them makes the next " ..
          "release 20% stronger (8 sec).",
      loop = "Earth Shield on the tank; Chain Heal through them twice to release the Wellspring.",
      mods = {
        { name = "Flowing Waters",
          text = "When Earth Shield renews the Wellspring, it also heals the most injured ally within 8 yards of " ..
              "the bearer for 20% of your healing power, once every 6 sec." },
        { name = "Lasting Spring",
          text = "The Wellspring lasts 22 sec and its release heals 10% more." },
        { name = "Widening Waters",
          text = "The release reaches 18 yards, and its ally also heals for 10% of your healing power over 6 sec." },
        { name = "Riptide Spring",
          text = "Riptide on the bearer renews the Wellspring and heals another injured ally within 10 yards for " ..
              "25% of your healing power, once every 8 sec." },
        { name = "Renewing Spring",
          text = "Renew on the bearer renews the Wellspring and makes its next release heal 20% more." },
        { name = "Deep Current",
          text = "The release heals 40% more on an ally below half health, 10% more otherwise." },
        { name = "Split Tributary",
          text = "The release heals 2 injured allies for 65% each (one alone gets it all)." },
        { name = "Concentrated Current",
          text = "The release heals one ally for 30% more." },
        { name = "Tidal Surge",
          text = "The release heals an ally with your Riptide 30% more and makes it last 2 sec longer (up to 6 sec " ..
              "for each cast)." },
        { name = "Guided Current",
          text = "Holy Shock healing the bearer makes the next release within 8 sec also heal one more ally for 40% " ..
              "as much." },
        { name = "Tidal Shelter",
          text = "The release also shields its ally for 25% of its healing for 4 sec." },
        { name = "Long Reach",
          text = "The release reaches 20 yards and heals 10% more." },
        { name = "Renewing Current",
          text = "The release also heals its ally for 25% of its healing over 4 sec." },
        { name = "Jade Current",
          text = "If your Soothing Mist is on the release's ally, it also heals them for 25% of a Soothing Mist " ..
              "tick." },
        { name = "Shielded Spring",
          text = "If your Power Word: Shield is on the release's ally, it also shields them for 15% of its healing " ..
              "for 4 sec." },
      },
      builds = {
        { name = "The Deep Well", mods = { "Lasting Spring", "Deep Current", "Tidal Shelter" },
          text = "A long-lived spring for the ones in danger." },
        { name = "The Wide River", mods = { "Widening Waters", "Split Tributary", "Renewing Current" },
          text = "Spread the release far and wide." },
        { name = "The Tidecaller", mods = { "Riptide Spring", "Tidal Surge", "Long Reach" },
          text = "Riptide feeds the spring." },
      },
    },
    { id = 26,
      name = "Rune of the Wandering Mist",
      role = "Healer (Soothing Mist)",
      roles = { "HEALER" },
      icon = "Ability_Rune_WanderingMist",
      keywords = "monk mistweaver mist nature heal soothing renew riptide holy shock chain heal earth shield",
      slots = { "Mistflow", "Technique", "Approach" },
      builtFrom = "Soothing Mist's ticks",
      spentWith = "Wandering Mist, Enveloping Mist and Vivify",
      resource = "None",
      core = "Each Soothing Mist tick also sends a Wandering Mist to your most injured ally within 20 yards of its " ..
          "target, healing for 25% of the tick. Enveloping Mist and Vivify on your Soothing Mist target are cast at " ..
          "once and keep the channel. Renew: a Wandering Mist on your Renew target heals 15% more.",
      loop = "Channel Soothing Mist on the one who needs it most; the mist finds the next.",
      mods = {
        { name = "Flowing Currents",
          text = "Wandering Mist reaches allies up to 30 yards from your Soothing Mist target instead of " ..
              "20." },
        { name = "Drifting Mist",
          text = "Each Soothing Mist tick sends Wandering Mist to 2 allies, healing 15% of the tick's " ..
              "healing each, instead of 25% to one." },
        { name = "Gathering Mists",
          text = "Wandering Mist heals allies below half health for 25% more." },
        { name = "Renewing Mist",
          text = "Wandering Mist on your Renew target makes it last 2 sec longer (up to 6 sec for each cast), once " ..
              "every 6 sec." },
        { name = "Tidal Mist",
          text = "Wandering Mist on your Riptide target also heals another injured ally within 8 yards for 40% of " ..
              "it, once every 6 sec." },
        { name = "Comforting Veil",
          text = "Enveloping Mist also wraps your most injured nearby ally in a Lesser Enveloping Mist " ..
              "that heals 30% as much." },
        { name = "Vivacious Flow",
          text = "Vivify also heals your most injured nearby ally for 30% of its heal." },
        { name = "Peaceful Focus",
          text = "Soothing Mist heals 15% more on a target with your Enveloping Mist." },
        { name = "Holy Thread",
          text = "Holy Shock healing your Soothing Mist target also heals your most injured nearby ally for 25% of " ..
              "it, once every 8 sec." },
        { name = "Chain of Jade",
          text = "Chain Heal on your Soothing Mist target also sends a Wandering Mist to your most injured nearby " ..
              "ally for 30% of its first heal, once every 8 sec." },
        { name = "Effortless Breathing",
          text = "Soothing Mist costs 25% less mana." },
        { name = "Rolling Mist",
          text = "Moving your Soothing Mist to another ally leaves 2 more ticks over 4 sec on the last one, once " ..
              "every 12 sec." },
        { name = "Jade Shelter",
          text = "Wandering Mist also shields its ally for 30% of its healing for 4 sec, once every 8 sec." },
        { name = "Renewing Breath",
          text = "The third tick of each Soothing Mist makes your Renew on its target last 2 sec longer." },
        { name = "Earthen Cocoon",
          text = "Wandering Mist on your Earth Shield's bearer also shields them for 25% of its healing for 4 sec, " ..
              "once every 8 sec." },
      },
      builds = {
        { name = "The Mistwalker", mods = { "Flowing Currents", "Vivacious Flow", "Rolling Mist" },
          text = "Heal everyone, leave nobody behind." },
        { name = "The Jade Wall", mods = { "Gathering Mists", "Peaceful Focus", "Jade Shelter" },
          text = "Keep the ones in danger standing." },
        { name = "The Gentle Rain", mods = { "Drifting Mist", "Comforting Veil", "Effortless Breathing" },
          text = "Spread the mist thin and wide." },
      },
    },
    { id = 27,
      name = "Rune of the Threefold Grave",
      retired = true,
      role = "Melee DPS or Tank (Icy Touch, Plague Strike, Blood Strike)",
      roles = { "DAMAGER", "TANK" },
      icon = "Ability_Rune_ThreefoldGrave",
      keywords = "death knight blood frost shadow graves undead aoe",
      slots = { "Burial Grounds", "Gravekeeper", "Shared Burial" },
      builtFrom = "Icy Touch, Plague Strike, Blood Strike",
      spentWith = "Your Graves (automatic)",
      resource = "One Grave of each kind",
      core = "Icy Touch lays a Frozen Grave beneath its target (Frost damage and a 30% slow), Plague Strike a " ..
          "Plagued Grave (Shadow damage), Blood Strike a Crimson Grave (damage): one of each, 8 sec. Enemies in two " ..
          "or more take 20% more from your Graves. Every Blood Strike also shields you for 5% of your health, up to " ..
          "15%.",
      loop = "Strike with all three runes on the same enemy and stack your Graves under it.",
      mods = {
        { name = "Far-Reaching Graves",
          text = "Your Graves are 50% wider." },
        { name = "Long Rest",
          text = "Your Graves last 12 sec." },
        { name = "March of the Dead",
          text = "Your Graves follow the enemy they were laid beneath." },
        { name = "Cruel Winter",
          text = "The Frozen Grave slows by 50%, and enemies leaving it stay slowed for 3 sec." },
        { name = "Grave Pestilence",
          text = "The Plagued Grave infects the enemies it damages with a short disease." },
        { name = "Crimson Bastion",
          text = "Blood Strike's shield holds up to 25% of your health." },
        { name = "Deep Burial",
          text = "Enemies in two or more Graves take 35% more instead of 20%." },
        { name = "Grave Hunger",
          text = "Your Graves deal 25% more to enemies below 35% health." },
        { name = "Grave Ward",
          text = "Each second, your Graves heal you for 1% of your health per enemy in them (up to 3%)." },
      },
      builds = {
        { name = "The Necropolis", mods = { "Far-Reaching Graves", "Grave Pestilence", "Deep Burial" },
          text = "Bury the whole pack." },
        { name = "The Hunter of the Dead", mods = { "March of the Dead", "Cruel Winter", "Grave Hunger" },
          text = "Let nothing escape its grave." },
        { name = "The Undying", mods = { "Long Rest", "Crimson Bastion", "Grave Ward" },
          text = "Stand in your own graveyard and never fall." },
      },
    },
    { id = 28,
      name = "Rune of the Unbroken Onslaught",
      retired = true,
      role = "Melee DPS (Mortal Strike or Bloodthirst)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_UnbrokenOnslaught",
      keywords = "warrior arms fury bleed physical cleave wake",
      slots = { "Wake", "Follow-Through", "Pursuit" },
      builtFrom = "Mortal Strike or Bloodthirst",
      spentWith = "Any other melee ability",
      resource = "None: your Weapon Wake",
      core = "Mortal Strike or Bloodthirst cuts a Weapon Wake through its target for 6 sec. Your next " ..
          "other melee ability rips it open: every enemy in it (up to 6) takes 96% of a weapon swing as " ..
          "damage, and its first target takes 25% more damage.",
      loop = "Mortal Strike, then anything else: every strike lands twice.",
      mods = {
        { name = "Executioner's Cut",
          text = "A narrow cleft through your target: the wake's first target takes 50% more damage " ..
              "instead of 25% more." },
        { name = "Whirling Steel",
          text = "The wake is a crescent 8 yards wide in front of you, instead of a line through your " ..
              "target." },
        { name = "Raking Passage",
          text = "The wake reaches 14 yards past your target, hitting the enemies behind it." },
        { name = "Overpowering Echo",
          text = "The wake rips open twice: once for each of your next two melee abilities, 96% of a " ..
              "weapon swing each time." },
        { name = "Bloodletting",
          text = "Enemies the wake hits bleed for 30% of the wake's damage every second for 4 sec (120% of " ..
              "its damage in all)." },
        { name = "Aftershock",
          text = "1.5 sec after the wake rips open, it strikes again for half its damage: 48% of a weapon " ..
              "swing." },
        { name = "Relentless",
          text = "Your wake moves with its first target, instead of staying where it was cut." },
        { name = "Battlefield Claim",
          text = "Enemies the wake hits are claimed for 8 sec: their movement speed is reduced by 30% and " ..
              "they take 10% more damage from you." },
        { name = "Second Wind",
          text = "Each enemy the wake hits heals you for 1% of your maximum health." },
      },
      builds = {
        { name = "The Headsman", mods = { "Executioner's Cut", "Overpowering Echo", "Relentless" },
          text = "Every blow lands twice on the one that matters." },
        { name = "The Whirlwind", mods = { "Whirling Steel", "Aftershock", "Second Wind" },
          text = "Cleave the pack and keep standing." },
        { name = "The Long Road", mods = { "Raking Passage", "Bloodletting", "Battlefield Claim" },
          text = "Cut through the line and let them bleed." },
      },
    },
    { id = 29,
      name = "Rune of the Dawnbound Crusader",
      role = "Healer or Melee DPS (Crusader Strike, Holy Shock)",
      roles = { "HEALER", "DAMAGER" },
      icon = "Ability_Rune_DawnboundCrusader",
      keywords = "paladin retribution holy light crusader echo dawn vow beacon of light judgment mark echo earth " ..
          "shield flame shock frostbolt chain heal renew lava lash",
      slots = { "Doctrine", "Dawn", "Pilgrimage" },
      builtFrom = "Dawn Vow (Beacon of Light), Judgment",
      spentWith = "Crusader Strike on your mark, Holy Shock",
      resource = "Your vow and your mark",
      core = "Beacon of Light becomes Dawn Vow (6 sec, free, off the global cooldown): vow yourself or an ally " ..
          "within 40 yards. Judgment marks its enemy for 10 sec. Crusader Strike on the marked enemy, or Holy " ..
          "Shock, sends a Dawn Echo from your vowed ally (from you if none), once every 3 sec: it heals the most " ..
          "injured ally within 10 yards of them for 35% of your power (the higher of your Holy spell power and half " ..
          "your attack power). Your Doctrine can change what the echo does.",
      loop = "Vow your tank; Judgment, then Crusader Strike or Holy Shock for the echo.",
      mods = {
        { name = "Mercy",
          text = "The echo heals the most injured ally within 10 yards of your vow for 50% of your power." },
        { name = "Retribution",
          text = "The echo strikes your marked enemy within 20 yards of your vow for 45% of your power as Holy " ..
              "damage." },
        { name = "Communion",
          text = "Echoes take turns: a heal for 35% of your power, then a strike for 30%." },
        { name = "Earthen Oath",
          text = "While your Earth Shield is on your vow, the echo heals for 40% of your power and shields your vow " ..
              "for 10% for 4 sec." },
        { name = "Crusader's Flame",
          text = "The echo strikes your marked enemy for 35% of your power as Holy damage, and 15% as Fire if your " ..
              "Flame Shock is on it." },
        { name = "Dawnwave",
          text = "The echo travels from your vow toward your marked enemy, touching up to 3 on the way for half " ..
              "each." },
        { name = "Sunburst",
          text = "The echo also touches up to 2 more nearby for 40% each." },
        { name = "Sacred Ground",
          text = "The echo leaves holy ground 4 yards wide at your vow for 4 sec: each second it heals one injured " ..
              "ally in it for 10% of your power." },
        { name = "Frostfire Dawn",
          text = "Frostbolt on your marked enemy makes your next striking echo within 6 sec also deal 20% of your " ..
              "power as Frost damage." },
        { name = "Chain of Grace",
          text = "Chain Heal on your vow makes your next healing echo within 6 sec also heal one more ally for 40%." },
        { name = "Steadfast",
          text = "Holy Shock leaves an afterimage on your vow for 6 sec: its next echo strikes again at 30%." },
        { name = "Passing the Torch",
          text = "Moving your Dawn Vow to another ally heals them for 30% of your power." },
        { name = "Shared Pilgrimage",
          text = "Once you or your vow move 8 yards, the next echo touches one more for 40%." },
        { name = "Renewed Oath",
          text = "An echo while your Renew is on your vow makes it last 2 sec longer (up to 6 sec for each cast), " ..
              "once every 6 sec." },
        { name = "Searing Vow",
          text = "Lava Lash on your marked enemy sends an echo too (still once every 3 sec)." },
      },
      builds = {
        { name = "The Merciful", mods = { "Mercy", "Sunburst", "Steadfast" },
          text = "Every strike heals the ones beside your vow." },
        { name = "The Crusader", mods = { "Retribution", "Dawnwave", "Shared Pilgrimage" },
          text = "The dawn burns a path to your mark." },
        { name = "The Pilgrim", mods = { "Communion", "Sacred Ground", "Passing the Torch" },
          text = "Heal and strike by turns, wherever you go." },
      },
    },
    { id = 30,
      name = "Rune of the Hollow Congregation",
      role = "Ranged DPS (Mind Flay, Mind Blast)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_HollowCongregation",
      keywords = "priest shadow mind flay echoes spirits corruption moonfire shadow bolt frostbolt drain soul renew " ..
          "dot patch",
      slots = { "Congregation", "Sermon", "Command" },
      builtFrom = "Mind Flay",
      spentWith = "Mind Blast",
      resource = "Hollow Echoes, up to 3",
      core = "Mind Flay calls a Hollow Echo to your side for 10 sec (3 at most): it follows you and channels a " ..
          "weaker Mind Flay on your target (40% of a tick), 5% more for each of your damage over time effects on " ..
          "that target (up to 5). Mind Blast turns every echo on its target in a barrage of 3 bolts, 60% of a tick " ..
          "each. Shadow Word: Pain: a barrage ending on your Pain target leaves a patch dealing 10% of your spell " ..
          "power each second to up to 3 enemies for 3 sec. Corruption: the first bolt of each barrage also strikes " ..
          "another enemy with your Corruption within 8 yards for half.",
      loop = "Flay to gather your echoes, then Mind Blast to unleash them.",
      mods = {
        { name = "Following Shadows",
          text = "Your echoes follow you, and their Mind Flay deals 15% more damage." },
        { name = "Haunted Ground",
          text = "Your echoes stay where they were called, reach 10 yards farther (50 yards), and their Mind Flay " ..
              "deals 20% more damage." },
        { name = "Possession",
          text = "Your echoes follow their target, and their Mind Flay deals 20% more damage. One echo for each " ..
              "enemy: a new one on the same enemy replaces it." },
        { name = "Plague Sermon",
          text = "Barrage bolts deal 20% more damage to your Corruption target." },
        { name = "Moonlit Congregation",
          text = "An echo's Mind Flay on your Moonfire target also deals 10% of your spell power as Arcane damage, " ..
              "once every 2 sec for each echo." },
        { name = "Unison",
          text = "Mind Blast's barrage lands all at once and deals 20% more damage: 72% of a Mind Flay " ..
              "tick's damage per bolt." },
        { name = "Dissonance",
          text = "Your echoes spread out over enemies with your Shadow Word: Pain on them, your target " ..
              "first." },
        { name = "Accusation",
          text = "Instead of bolts, each echo screams through up to 5 enemies on the way to your target, " ..
              "dealing 90% of a Mind Flay tick's damage to each." },
        { name = "Devouring Command",
          text = "A Shadow Bolt hit turns one echo on its target with a Shadow hit of 30% of your spell power, once " ..
              "every 8 sec. No barrage." },
        { name = "Frostbound Sermon",
          text = "A Frostbolt hit on an echo's target makes that echo's next barrage also strike one other enemy " ..
              "within 8 yards with each bolt, for half." },
        { name = "Last Word",
          text = "When a Hollow Echo fades, it strikes its target for 3 Mind Flay ticks at once." },
        { name = "Procession",
          text = "Mind Blast moves your echoes beside its target, and the first bolt of each barrage deals 30% more " ..
              "damage." },
        { name = "Silent Witness",
          text = "While you move, your echoes' Mind Flay deals 25% more damage." },
        { name = "Feeding the Void",
          text = "While you channel Drain Soul on an echo's target, that echo's Mind Flay deals 20% more damage." },
        { name = "Renewed Witness",
          text = "A barrage completed while your Renew is on you heals you for 30% of your spell power, once every " ..
              "8 sec." },
      },
      builds = {
        { name = "The Choir", mods = { "Following Shadows", "Unison", "Procession" },
          text = "Your congregation at your side, one voice." },
        { name = "The Plague of Whispers", mods = { "Possession", "Dissonance", "Silent Witness" },
          text = "Spread your echoes over every enemy you have cursed." },
        { name = "The Last Rites", mods = { "Haunted Ground", "Accusation", "Last Word" },
          text = "Line them up and scream them down." },
      },
    },
    { id = 31,
      name = "Rune of the Shattered Sun",
      role = "Ranged DPS (Fireball, Frostbolt)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_ShatteredSun",
      keywords = "mage fire frost arcane shards shatter explosion fireball frostbolt frostfire sunshard fracture " ..
          "broken core collapse ice lance flame shock moonfire lightning bolt ice barrier",
      slots = { "Sunshard", "Fracture", "Collapse" },
      builtFrom = "Fireball",
      spentWith = "Frostbolt, then Fireball",
      resource = "Sunshards, in up to 2 enemies",
      core = "Fireball embeds a Sunshard in its target for 10 sec, in up to 2 enemies (a new one replaces the " ..
          "oldest). Frostbolt on a shard fractures it: 45% of your spell power as Frostfire damage to its host and " ..
          "25% to up to 4 enemies within 6 yards, and the host's core breaks for 8 sec. Your next Fireball on a " ..
          "Broken Core collapses it for 40% of your spell power. Ice Lance on a Broken Core also deals 20% of your " ..
          "spell power as Frost damage, once a core. A fracture on your Flame Shock target makes it last 2 sec " ..
          "longer (up to 6 sec for each cast).",
      loop = "Fireball, Frostbolt, Fireball: shard, fracture, collapse.",
      mods = {
        { name = "Twin Suns",
          text = "Up to 3 enemies hold a Sunshard, and fractures deal 15% more to their host." },
        { name = "Orbiting Sun",
          text = "A fracture also strikes your selected enemy within 15 yards for 40% of the host's damage." },
        { name = "Solar Anchor",
          text = "A Sunshard stays where its host stood for 10 sec: Frostbolt on any enemy within 6 yards of it " ..
              "fractures it, for 20% more to the host." },
        { name = "Lunar Shard",
          text = "Fireball on your Moonfire target embeds a shard whose fracture also deals 25% of your spell power " ..
              "as Arcane damage to its host." },
        { name = "Storm Shard",
          text = "Lightning Bolt embeds a Sunshard too, once every 8 sec; its fracture also deals 20% of your spell " ..
              "power as Nature damage to its host." },
        { name = "Splinterstorm",
          text = "A fracture bursts in a 12-yard cone away from you instead: up to 5 enemies, the others taking " ..
              "half the host's damage." },
        { name = "Solar Lance",
          text = "A fracture also pierces up to 3 enemies behind its host within 15 yards for 60% of the host's " ..
              "damage." },
        { name = "Scalding Ground",
          text = "A fracture leaves scalding ground 6 yards wide for 4 sec: 10% of your spell power as Frostfire " ..
              "damage each second to up to 5 enemies." },
        { name = "Frozen Flash",
          text = "Ice Lance fractures a Sunshard too, once every 8 sec." },
        { name = "Flame Burst",
          text = "A fracture on your Flame Shock target spreads it to one enemy within 8 yards, once every 8 sec." },
        { name = "Implosion",
          text = "The collapse deals 50% more damage." },
        { name = "Chain Reaction",
          text = "The collapse fractures one other Sunshard within 8 yards, at half damage." },
        { name = "Second Sunrise",
          text = "1 sec after the collapse, it strikes again for half: its host and up to 2 enemies within 6 yards." },
        { name = "Shattering Lance",
          text = "Ice Lance on a Broken Core deals 35% of your spell power, and your next Fireball within 6 sec " ..
              "casts 15% faster." },
        { name = "Icy Shelter",
          text = "A collapse while your Ice Barrier is up shields you for 40% of your spell power for 6 sec, once " ..
              "every 12 sec." },
      },
      builds = {
        { name = "The Twin Suns", mods = { "Twin Suns", "Scalding Ground", "Chain Reaction" },
          text = "Shards everywhere, ground ablaze." },
        { name = "The Lance", mods = { "Orbiting Sun", "Solar Lance", "Implosion" },
          text = "One host, broken hard." },
        { name = "The Frostfire Mage", mods = { "Solar Anchor", "Splinterstorm", "Second Sunrise" },
          text = "Fire and frost across the pack." },
      },
    },
    { id = 32,
      name = "Rune of the Spectral Blade",
      role = "Melee DPS (Rogue)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_SpectralBlade",
      keywords = "rogue phantom shadow assassin finisher combo points eviscerate bleed physical backstab sinister " ..
          "strike shadow word pain icy touch flame shock hunters mark corruption renew rend",
      slots = { "Execution Style", "Timing", "Aftermath" },
      builtFrom = "Your combo points",
      spentWith = "Eviscerate",
      resource = "Phantom strikes",
      core = "Eviscerate calls a phantom assassin behind its victim. 0.6 sec later it strikes for 200% of " ..
          "Eviscerate's damage, and your Energy regenerates 100% faster for 4 sec. The phantom cannot critically " ..
          "strike and triggers nothing, poisons included. More combo points, a bigger Eviscerate, a bigger phantom. " ..
          "Backstab marks its target for 6 sec: the phantom spends the mark for 25% more damage. After the phantom " ..
          "strikes, your next Sinister Strike within 6 sec costs 10 less Energy. Shadow Word: Pain: the phantom on " ..
          "your Pain target leaves Phantom Wound, 20% of its damage over 6 sec.",
      loop = "Build combo points, then Eviscerate: your blow, then the phantom's.",
      mods = {
        { name = "Executioner",
          text = "The phantom deals 40% more damage: 280% of Eviscerate's damage." },
        { name = "Thousand Cuts",
          text = "The phantom strikes 3 times, 0.2 sec apart, for 230% of Eviscerate's damage in all." },
        { name = "Shadow Cleave",
          text = "The phantom also strikes up to 3 other enemies within 8 yards for 100% of Eviscerate's " ..
              "damage each." },
        { name = "Frost Phantom",
          text = "The phantom on your Frost Fever target also deals 20% of your attack power as Frost damage." },
        { name = "Flame Phantom",
          text = "The phantom on your Flame Shock target also deals 20% of your spell power as Fire damage." },
        { name = "Fatal Opening",
          text = "The phantom strikes 0.1 sec after Eviscerate instead of 0.6, and your next Sinister Strike, " ..
              "Backstab, Hemorrhage or Ghostly Strike within 6 sec costs 10 less Energy." },
        { name = "Perfect Ambush",
          text = "The phantom waits 2 sec, then strikes for 280% of Eviscerate's damage." },
        { name = "Open Wound",
          text = "The phantom leaves a bleed instead: 240% of Eviscerate's damage over 4 sec. Refreshed, " ..
              "never stacked." },
        { name = "Marked Execution",
          text = "The phantom on your Hunter's Mark target also strikes one other enemy within 8 yards for half its " ..
              "damage." },
        { name = "Corrupted Blade",
          text = "The phantom on your Corruption target leaves 25% of its damage as Shadow damage over 6 sec." },
        { name = "Death Follows",
          text = "If its victim dies first, the phantom strikes the nearest enemy within 10 yards." },
        { name = "Vanishing Act",
          text = "When the phantom strikes, your movement speed rises by 30% for 3 sec." },
        { name = "Marked for Murder",
          text = "Your Backstab's mark makes the phantom deal 40% more damage instead of 25%." },
        { name = "Restoring Phantom",
          text = "The phantom's strike while your Renew is on you heals you for 30% of your spell power, once every " ..
              "10 sec." },
        { name = "Bleeding Follow-Through",
          text = "The phantom's strike on your Rend target makes your next Sinister Strike within 6 sec deal 20% " ..
              "more damage." },
      },
      builds = {
        { name = "The Headsman", mods = { "Executioner", "Perfect Ambush", "Marked for Murder" },
          text = "One victim, one huge blow." },
        { name = "The Shadow Dance", mods = { "Shadow Cleave", "Fatal Opening", "Death Follows" },
          text = "Cut through a whole pack." },
        { name = "The Bleeding Edge", mods = { "Thousand Cuts", "Open Wound", "Vanishing Act" },
          text = "Bleed them and slip away." },
      },
    },
    { id = 33,
      name = "Rune of the Wild Hunt",
      role = "DPS (Hunter, with a pet)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_WildHunt",
      keywords = "hunter pet beast spectral wolf companion summon kill command bleed physical hunter's mark arcane " ..
          "shot icy touch frost fever lightning bolt mend pet",
      slots = { "Spectral Beast", "The Hunt", "Final Attack" },
      builtFrom = "Your pet",
      spentWith = "Kill Command, Bestial Wrath",
      resource = "Spectral Beasts, up to 8",
      core = "Kill Command unleashes a Spectral Beast in your pet's likeness on its target for 10 sec, as well as " ..
          "empowering your pet. Each beast bites every 1.5 sec for 60% of your pet's attack and goes when its prey " ..
          "dies; up to 8 hunt at once. During Bestial Wrath, Kill Command recovers in 5 sec and a beast joins the " ..
          "hunt every second for 4 sec. Needs a living pet and an enemy. Hunter's Mark: beasts deal 15% more to " ..
          "your marked target. Arcane Shot on a beast's target makes the next bite there deal 20% more (6 sec).",
      loop = "Kill Command whenever it is ready; Bestial Wrath to unleash the whole pack, Kill Command every 5 sec.",
      mods = {
        { name = "Alpha Predator",
          text = "Spectral Beasts bite for 78% of your pet's attack instead of 60%." },
        { name = "Pack Ravager",
          text = "Each Spectral Beast bite also hits up to 2 other enemies within 5 yards for 30% of your " ..
              "pet's attack." },
        { name = "Ghostfang",
          text = "Each bite makes its target bleed for 15% of your pet's attack every second for 4 sec. " ..
              "Refreshed, never stacked." },
        { name = "Marked Alpha",
          text = "A bite on your marked target also bites one other enemy within 5 yards for 50% as much." },
        { name = "Frostfang",
          text = "Bites on your Frost Fever target also deal 10% of your ranged attack power as Frost damage." },
        { name = "Fixed Prey",
          text = "Spectral Beasts hold to their first prey and bite it for 20% more." },
        { name = "Hunter's Command",
          text = "Your Spectral Beasts switch to whatever enemy you target." },
        { name = "Blood Scent",
          text = "When its prey dies, a Spectral Beast hunts on: your target, else the nearest enemy." },
        { name = "Arcane Hunt",
          text = "Arcane Shot's empowered bite deals 40% more instead of 20%." },
        { name = "Storm Hunt",
          text = "Lightning Bolt sends your Spectral Beasts to its target, their next bite adding 20% of your spell " ..
              "power as Nature damage, once every 6 sec." },
        { name = "Final Pounce",
          text = "As a Spectral Beast fades, it pounces for 120% of your pet's attack." },
        { name = "Frenzied Farewell",
          text = "As a Spectral Beast fades, it bites 4 times in a flurry, 36% of your pet's attack each." },
        { name = "Parting Howl",
          text = "As a Spectral Beast fades, your pet deals 30% more damage for 6 sec." },
        { name = "Marked Finale",
          text = "As a Spectral Beast fades, it pounces your marked target for 35% of your ranged attack power, and " ..
              "up to 2 enemies within 5 yards for 40% of that." },
        { name = "Healing Companion",
          text = "As a Spectral Beast fades with your Mend Pet on your pet, it heals your pet for 5% of its health " ..
              "and bites for 25% of your ranged attack power." },
      },
      builds = {
        { name = "The Alpha", mods = { "Alpha Predator", "Fixed Prey", "Final Pounce" },
          text = "One prey, torn apart." },
        { name = "The Pack", mods = { "Pack Ravager", "Blood Scent", "Frenzied Farewell" },
          text = "Tear through every enemy near." },
        { name = "The Huntmaster", mods = { "Ghostfang", "Hunter's Command", "Parting Howl" },
          text = "The beast follows your lead." },
      },
    },
    { id = 34,
      name = "Rune of Arcane Mind",
      role = "Ranged DPS (Mage, Arcane)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_ArcaneMind",
      keywords = "mage arcane blast missiles insight channel mana burst volley lightning bolt starfire frostbolt " ..
          "explosion evocation barrage",
      slots = { "Invocation", "Manifestation", "Adaptation" },
      builtFrom = "Arcane Blast",
      spentWith = "Arcane Missiles",
      resource = "Arcane Insight, up to 4",
      core = "Arcane Blast grants 1 Arcane Insight (15 sec, renewed by each one; up to 4). At 4, your next Arcane " ..
          "Missiles spends it all: it costs no mana and fires 5 Insight missiles with it, each dealing 40% of an " ..
          "Arcane Missile's damage. Interrupting the channel loses the rest of the volley. Arcane Barrage spends " ..
          "up to 2 Insight: 15% more damage for each, and one other enemy within 8 yards hit for 30% of it for " ..
          "each. Finishing an empowered Arcane Missiles makes your next Arcane Blast within 6 sec instant. " ..
          "Lightning Bolt: a hit grants 1 Insight, once every 8 sec.",
      loop = "Arcane Blast until Insight reaches 4, then Arcane Missiles.",
      mods = {
        { name = "Accelerated Thought",
          text = "Every third Arcane Blast that hits grants 2 Insight instead of 1." },
        { name = "Expanding Intellect",
          text = "Arcane Explosion grants Insight too: 2 if it hits 3 or more enemies, else 1. Once a cast." },
        { name = "Patient Calculation",
          text = "Insight stacks to 6 instead of 4. Spending 5 or 6 makes the Insight missiles deal 15% more damage " ..
              "for each one above 4." },
        { name = "Storm Conduit",
          text = "A Lightning Bolt hit on an enemy your Arcane Blast hit in the last 15 sec makes your next " ..
              "Insight volley deal 20% more damage (8 sec)." },
        { name = "Starfall Equation",
          text = "A Starfire hit grants 1 Insight, once every 8 sec, and makes your next Arcane Blast within 6 sec " ..
              "deal 10% more damage." },
        { name = "Focused Intention",
          text = "Insight missiles deal 40% more damage to their target: 56% of an Arcane Missile's damage." },
        { name = "Fractured Thought",
          text = "Each Insight missile also hits up to 2 other enemies within 8 yards of its target for 35% of its " ..
              "damage." },
        { name = "Lingering Equation",
          text = "The Insight missiles leave Arcane Echo on their target: 30% of their damage again, over 6 sec. A " ..
              "new Echo keeps what is left of the last." },
        { name = "Missile Shards",
          text = "A Frostbolt hit readies your next empowered Arcane Missiles (within 15 sec): its first Insight " ..
              "missile also hits 2 other enemies within 8 yards for half its damage." },
        { name = "Arcane Charge",
          text = "Arcane Explosion within 6 sec of finishing an empowered Arcane Missiles bursts for 25% of your " ..
              "spell power on up to 5 enemies within 8 yards." },
        { name = "Unbound Mind",
          text = "An empowered Arcane Missiles does not channel: its missiles fly from you on their own, one every " ..
              "0.5 sec, so you can move and keep casting." },
        { name = "Mental Reserves",
          text = "When an empowered Arcane Missiles completes, you regain 3% of your maximum mana and Arcane Blast " ..
              "costs 20% less mana for 4 sec." },
        { name = "Overclocked Intellect",
          text = "The Insight missiles all fly as the channel starts, 0.15 sec apart, and deal 20% more damage: 48% " ..
              "of an Arcane Missile's damage." },
        { name = "Evocative Mind",
          text = "Evocation begun within 30 sec of an empowered Arcane Missiles restores 2% of your maximum mana " ..
              "more, once every 30 sec." },
        { name = "Shocking Clarity",
          text = "Finishing an empowered Arcane Missiles while your Lightning Shield is up sends a bolt of 25% of " ..
              "your spell power as Nature damage at its target." },
      },
      builds = {
        { name = "The Focus", mods = { "Accelerated Thought", "Focused Intention", "Unbound Mind" },
          text = "Quick Insight, a heavy volley, cast on the move." },
        { name = "The Storm", mods = { "Expanding Intellect", "Fractured Thought", "Mental Reserves" },
          text = "Build on packs, spread the volley." },
        { name = "The Patient", mods = { "Patient Calculation", "Lingering Equation", "Overclocked Intellect" },
          text = "Bank Insight, spend it in one burst." },
      },
    },
    { id = 35,
      name = "Rune of the Splintered Crown",
      role = "Ranged DPS (Mage, Frost)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_SplinteredCrown",
      keywords = "mage frost frostbolt icicle ice shard pierce slow instant crown ice lance cone of cold blizzard " ..
          "hailstorm frostbite fire blast icy touch",
      slots = { "Formation", "Winter", "Aftermath" },
      builtFrom = "Frostbolt",
      spentWith = "The Icicles fire on their own",
      resource = "Icicles, up to 5 waiting",
      core = "A Frostbolt that hits forms an Icicle in the crown above you: 1 sec later it flies at the same enemy " ..
          "for 30% of that Frostbolt's damage. Up to 5 wait at once; an Icicle whose target dies melts. Ice Lance: " ..
          "a hit looses 2 Icicles at its target, 20% of your spell power each. Cone of Cold: an Icicle at each " ..
          "enemy it hits (up to 5), 15% of your spell power. Blizzard: every 2 sec of its channel, an Icicle at the " ..
          "enemy it damaged with the most health, 20% of your spell power. Every Icicle leaves Frostbite for 6 sec: " ..
          "your Frostbolt and Ice Lance deal 8% more to that enemy.",
      loop = "Frostbolt, Ice Lance, Cone of Cold or Blizzard; the Icicles strike on their own.",
      mods = {
        { name = "Glacial Spear",
          text = "Frostbolt's Icicles deal 40% more damage: 42% of the Frostbolt's damage." },
        { name = "Twin Fangs",
          text = "Ice Lance's two Icicles deal 25% of your spell power each instead of 20%. The second also leaves " ..
              "a Frost wound of 10% of your spell power over 4 sec." },
        { name = "Frozen Volley",
          text = "Cone of Cold looses 2 Icicles at each enemy it hits (up to 5), 10% of your spell power each." },
        { name = "Hailstorm Crown",
          text = "Blizzard looses an Icicle every second instead of every 2, each 15% of your spell power instead " ..
              "of 20%." },
        { name = "Splintering Ice",
          text = "Frostbolt's Icicles also hit up to 2 other enemies within 8 yards for half their damage." },
        { name = "Cold Snap",
          text = "When both of Ice Lance's Icicles hit, your next Frostbolt within 6 sec casts 15% faster." },
        { name = "Brittle Impact",
          text = "Cone of Cold's Icicle deals 30% more damage to an enemy already in your Frostbite." },
        { name = "Gathering Blizzard",
          text = "Each Icicle Blizzard looses makes your next Ice Lance deal 5% more damage, up to 20%, for 8 sec." },
        { name = "Frostfire Fracture",
          text = "Fire Blast on an enemy in your Frostbite erupts in an Icicle: 25% of your spell power to it and " ..
              "up to 2 other enemies within 6 yards, once every 8 sec." },
        { name = "Runic Rime",
          text = "After your Icy Touch hits an enemy, your next Frostbolt on it within 6 sec forms one more Icicle " ..
              "at half damage." },
        { name = "Deep Freeze",
          text = "Your Icicles deal 20% more damage to enemies already in your Frostbite." },
        { name = "Glacial Wound",
          text = "Ice Lance's second Icicle leaves Glacial Wound: 20% of your spell power over 6 sec. A new wound " ..
              "keeps what is left of the last." },
        { name = "Permafrost",
          text = "Cone of Cold's Icicles slow their enemies by 20% for 3 sec; your next Frostbolt Icicle on each in " ..
              "that time deals 25% more damage." },
        { name = "Whiteout",
          text = "Blizzard's Icicles also hit up to 2 other enemies within 6 yards for 40% of their damage; with " ..
              "none near, the Icicle deals 20% more." },
        { name = "Cold Front",
          text = "Every third Icicle strike makes your next Frostbolt within 5 sec instant. Ice Lance's two Icicles " ..
              "count as two." },
      },
      builds = {
        { name = "The Lancer", mods = { "Twin Fangs", "Cold Snap", "Glacial Wound" },
          text = "Ice Lance after Ice Lance." },
        { name = "The Storm", mods = { "Hailstorm Crown", "Gathering Blizzard", "Whiteout" },
          text = "Blizzard on the pack, then the lance." },
        { name = "The Frostfire", mods = { "Splintering Ice", "Frostfire Fracture", "Deep Freeze" },
          text = "Frostbolt, Fire Blast, and the crown breaks." },
      },
    },
    { id = 36,
      name = "Rune of Phoenix Wake",
      role = "Ranged DPS (Mage, Fire)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_PhoenixWake",
      keywords = "mage fire pyroblast fire blast phoenix ember bird burn trail flock flame shock lava burst " ..
          "flamestrike scorch fire ward",
      slots = { "Phoenix", "Flight", "Wake" },
      builtFrom = "Pyroblast",
      spentWith = "Fire Blast",
      resource = "A Phoenix Ember, 8 sec",
      core = "Pyroblast leaves a Phoenix Ember on its target for 8 sec (one at a time: a new Pyroblast moves it). " ..
          "Fire Blast on that target calls the phoenix: it dives from the sky above you onto the target at 20 yards " ..
          "a second, burning up to 4 enemies under its path for 25% of that Pyroblast's damage each, and strikes " ..
          "the target for 55% and burns it for 30% of your spell power over 6 sec (Phoenix Burn). Pyroblast deals " ..
          "15% more to a target with your Phoenix Burn. Flame Shock: crossing your Flame Shock target, the " ..
          "phoenix erupts for 20% of your spell power on up to 3 enemies within 6 yards (once a phoenix).",
      loop = "Pyroblast, then Fire Blast the same target; the phoenix burns the enemies between you and it.",
      mods = {
        { name = "Sun Talon",
          text = "The phoenix strikes the Ember's target for 50% more: 82% of the Pyroblast's damage." },
        { name = "Broadwing",
          text = "The phoenix's path is 6 yards wide instead of 3, and enemies in it take 30% of the Pyroblast's " ..
              "damage instead of 25%." },
        { name = "Ashen Flock",
          text = "Three phoenixes dive instead, each with a third of the damage. Each one whose flight ends makes " ..
              "your next Pyroblast cast 5% faster, up to 15%, for 6 sec." },
        { name = "Flamekeeper",
          text = "Releasing the phoenix on your Flame Shock target also deals 20% of your spell power as Fire " ..
              "damage to it." },
        { name = "Lava Flock",
          text = "A Lava Burst hit on the Ember's target adds one more bird to that phoenix, dealing 30% of its " ..
              "damage (one for each Ember)." },
        { name = "Homeward Flame",
          text = "The phoenix dives twice as fast, 40 yards a second. When its flight ends, Fire Blast's cooldown " ..
              "is 2 sec shorter." },
        { name = "Death Spiral",
          text = "After its strike, the phoenix circles the Ember's target for 2 sec, then strikes it again for 25% " ..
              "of the Pyroblast's damage." },
        { name = "Wild Flight",
          text = "After its strike, the phoenix swoops through up to 2 other enemies within 10 yards of the Ember's " ..
              "target, striking each for 25% of the Pyroblast's damage." },
        { name = "Blazing Path",
          text = "The first time a phoenix flies through your Flamestrike, it deals 20% of your spell power as " ..
              "Fire damage to up to 4 enemies in it." },
        { name = "Hot Pursuit",
          text = "A Scorch hit on the Ember's target makes your next phoenix strike its target for 25% more damage " ..
              "(8 sec)." },
        { name = "Scorched Earth",
          text = "The phoenix leaves a trail of fire for 4 sec: up to 5 enemies on it burn for 6% of the " ..
              "Pyroblast's damage every second. Trails don't stack." },
        { name = "Clinging Embers",
          text = "Enemies the phoenix strikes burn for 25% more of that hit over 6 sec. A new burn keeps what is " ..
              "left of the last." },
        { name = "Rebirth",
          text = "When the phoenix's flight ends, it bursts for 35% of the Pyroblast's damage to up to 5 enemies " ..
              "within 6 yards of it. A flock bursts once." },
        { name = "Wildfire Return",
          text = "When the phoenix's flight ends, your Flame Shock on its target lasts 2 sec longer, up to 6 sec " ..
              "for each cast." },
        { name = "Emberguard",
          text = "When the phoenix's flight ends while your Fire Ward is up, you gain an absorb of 40% of your " ..
              "spell power for 6 sec, once every 12 sec." },
      },
      builds = {
        { name = "The Sun", mods = { "Sun Talon", "Homeward Flame", "Scorched Earth" },
          text = "Fast, hard, a road of fire." },
        { name = "The Pyre", mods = { "Broadwing", "Death Spiral", "Clinging Embers" },
          text = "Wide burns that linger." },
        { name = "The Flock", mods = { "Ashen Flock", "Wild Flight", "Rebirth" },
          text = "Three birds, everywhere at once." },
      },
    },
    { id = 37,
      name = "Rune of Astral Horizon",
      role = "Ranged DPS (Mage, Arcane)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_AstralHorizon",
      keywords = "mage arcane barrage orb star bolt cleave pierce explosion field arcane blast arcane explosion " ..
          "starfire lightning bolt moonfire frost nova arcane shot evocation shield",
      slots = { "Orbit", "Discharge", "Ending" },
      builtFrom = "Arcane Barrage",
      spentWith = "The Orb fires on its own",
      resource = "One Arcane Orb, 6 sec",
      core = "Arcane Barrage also sets an Arcane Orb loose, at most once every 12 sec. It flies at your target " ..
          "until it reaches an enemy, then drifts on slowly; for 6 sec it fires a bolt every second at that target " ..
          "(else the nearest enemy) within 10 yards of it, for 20% of the Barrage's damage. As it is set loose it " ..
          "strikes your target for two bolts. A new Orb replaces the last. Arcane Blast on the Orb's target makes " ..
          "its next bolt deal 35% more. Arcane Explosion within 8 yards of the Orb pulses for a bolt on up to 5 " ..
          "enemies near it, once every 4 sec. Starfire on the Orb's target makes its next bolt also hit one other " ..
          "enemy within 8 yards for half.",
      loop = "Arcane Barrage whenever the Orb is ready; fight around its path while it fires.",
      mods = {
        { name = "Wandering Star",
          text = "Each enemy the Orb passes through takes two bolts' damage (40% of the Barrage's damage), once (up " ..
              "to 5)." },
        { name = "Event Horizon",
          text = "The Orb flies to its target's position and stays there. Its bolts deal 50% more damage to enemies " ..
              "within 5 yards of it." },
        { name = "Satellite",
          text = "The Orb circles you, 4 yards out, for 8 sec instead of 6, and its bolts reach 15 yards instead of " ..
              "10." },
        { name = "Orbital Lightning",
          text = "A Lightning Bolt hit on the Orb's target commands an extra bolt of Nature damage, once every 4 " ..
              "sec." },
        { name = "Starward Satellite",
          text = "The Orb's bolts deal 20% more damage to your Moonfire target, and extend that Moonfire by 1 sec " ..
              "(up to 4 sec for each cast), once every 2 sec." },
        { name = "Astral Focus",
          text = "Bolts deal 60% more damage to the Orb's target: 32% of the Barrage's damage." },
        { name = "Constellation",
          text = "Each bolt also strikes up to 2 other enemies in range for 75% of its damage." },
        { name = "Prismatic Lance",
          text = "Each bolt becomes a lance that also pierces up to 3 enemies behind its target, for 80% of its " ..
              "damage." },
        { name = "Shattering Orbit",
          text = "A bolt on your Frost Nova target also strikes one other enemy in your Frost Nova within 8 yards " ..
              "for half its damage." },
        { name = "Distant Spark",
          text = "An Arcane Shot hit on the Orb's target makes its next bolt within 5 sec also pierce one more " ..
              "enemy within 8 yards for half its damage." },
        { name = "Supernova",
          text = "When the Orb's time ends, it explodes for 120% of the Barrage's damage (6 bolts) to up to 5 " ..
              "enemies within 8 yards." },
        { name = "Dying Light",
          text = "When the Orb's time ends, it fires 3 last bolts at your target (else its own), each for 60% of " ..
              "the Barrage's damage." },
        { name = "Astral Remnant",
          text = "When the Orb's time ends, it leaves a field 6 yards wide for 6 sec: every second, a bolt's damage " ..
              "to up to 5 enemies in it (120% of the Barrage's damage in all)." },
        { name = "Evoked Remnant",
          text = "Finishing Evocation while the Orb is out makes it last 2 sec longer, once for each Orb." },
        { name = "Astral Shelter",
          text = "Setting the Orb loose while your Power Word: Shield is on you grants an absorb of 25% of your " ..
              "spell power for 6 sec, once every 12 sec." },
      },
      builds = {
        { name = "The Comet", mods = { "Wandering Star", "Astral Focus", "Supernova" },
          text = "Through the pack, then a nova." },
        { name = "The Star", mods = { "Event Horizon", "Constellation", "Dying Light" },
          text = "A fixed star raining bolts." },
        { name = "The Moon", mods = { "Satellite", "Prismatic Lance", "Astral Remnant" },
          text = "At your side, lancing the line." },
      },
    },
    { id = 38,
      name = "Rune of Guiding Light",
      role = "Healer (Priest)",
      roles = { "HEALER" },
      icon = "Ability_Rune_GuidingLight",
      keywords = "priest holy heal healer prayer of mending flash heal greater heal star shield renew earth shield " ..
          "rejuvenation chain heal holy shock riptide",
      slots = { "Radiance", "Guidance", "Farewell" },
      builtFrom = "Prayer of Mending",
      spentWith = "Flash Heal, Greater Heal",
      resource = "One Guiding Star, 12 sec",
      core = "Prayer of Mending places a Guiding Star on its target instead, for 12 sec: every 2 sec it heals its " ..
          "bearer for a pulse, half a Prayer of Mending. Flash Heal or Greater Heal on another party member moves " ..
          "the star there (at most once every 3 sec), healing them on arrival for a full Prayer of Mending (two " ..
          "pulses). Renew: the star leaving your Renew target leaves a trail healing it for 15% of your spell power " ..
          "every 2 sec for 6 sec. Greater Heal: moving the star shields its new bearer for 30% of your spell power " ..
          "(6 sec). Earth Shield: arriving on your Earth Shield target, the star also heals one injured ally within " ..
          "10 yards for 30% of your spell power.",
      loop = "Prayer of Mending on whoever takes the most damage; Flash Heal or Greater Heal to send the star where " ..
          "it is needed.",
      mods = {
        { name = "Steady Light",
          text = "The star heals its bearer for 50% more: 75% of a Prayer of Mending every 2 sec." },
        { name = "Shared Light",
          text = "Each pulse also heals the most injured other ally within 10 yards of the bearer for 75% of it." },
        { name = "Sheltering Light",
          text = "Instead of healing on arrival, the star shields its new bearer for twice a pulse's healing, for 6 " ..
              "sec. A new shield replaces the last." },
        { name = "Earthen Star",
          text = "Each pulse on your Earth Shield target also heals one injured ally within 10 yards for 25% of " ..
              "your spell power. No Earth Shield charge is used." },
        { name = "Moonlit Star",
          text = "Each pulse on your Rejuvenation target makes that Rejuvenation last 1 sec longer, up to 4 sec " ..
              "for each cast." },
        { name = "Gentle Passage",
          text = "As it travels, the star heals up to 3 injured allies along its way for half a pulse each." },
        { name = "Guardian Star",
          text = "Only Greater Heal moves the star. Each pulse on the same bearer heals 10% more than the last, up " ..
              "to 30%; moving the star starts over." },
        { name = "Seeking Light",
          text = "Every third pulse also heals the most injured ally within 15 yards of the bearer for a pulse. If " ..
              "the bearer dies, the star moves on to that ally." },
        { name = "Chain Guidance",
          text = "Your Chain Heal on another ally moves the star there, with its arrival heal (at most once every " ..
              "3 sec)." },
        { name = "Holy Passage",
          text = "Your Holy Shock on another ally moves the star there, with its arrival heal (at most once every " ..
              "3 sec)." },
        { name = "Final Benediction",
          text = "When the star's 12 sec end, it heals its bearer for 2.5 pulses at once." },
        { name = "Afterglow",
          text = "When the star's 12 sec end, it leaves Afterglow on its bearer: 3.25 pulses of healing over 8 sec." },
        { name = "Parting Grace",
          text = "When the star's 12 sec end, it shields its bearer for 2 pulses (6 sec), and they move 30% faster " ..
              "for 3 sec." },
        { name = "Tidal Farewell",
          text = "When the star fades, if its bearer has your Riptide, it heals them for 60% of your spell power." },
        { name = "Renewed Farewell",
          text = "When the star fades, your Renew on its bearer lasts 4 sec longer (up to 6 sec for each cast), " ..
              "and they are healed for 30% of your spell power." },
      },
      builds = {
        { name = "The Beacon", mods = { "Steady Light", "Gentle Passage", "Final Benediction" },
          text = "One bearer held in the light." },
        { name = "The Guardian", mods = { "Shared Light", "Guardian Star", "Afterglow" },
          text = "A steady star that heals the group." },
        { name = "The Seeker", mods = { "Sheltering Light", "Seeking Light", "Parting Grace" },
          text = "Shields where they are needed." },
      },
    },
    { id = 39,
      name = "Rune of Elune's Guardian",
      role = "Ranged DPS (Druid, Balance)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_ElunesGuardian",
      keywords = "druid balance moonkin starfire wrath owl moon lunar arcane wrath moonfire hunters mark frostbolt " ..
          "arcane shot aimed shot steady shot prey sentinel",
      slots = { "Guardian", "Descent", "Blessing" },
      builtFrom = "Starfire",
      spentWith = "The owls dive on their own",
      resource = "Lunar owls, up to 3 waiting",
      core = "A Starfire that hits calls a lunar owl: 1 sec later it dives on the same enemy from the sky for 37.5% " ..
          "of that Starfire's damage and lays Moonlit Prey for 8 sec. Up to 3 wait at once; an owl whose target " ..
          "dies fades. Wrath: your Wrath on Moonlit Prey calls the owl back through it for 20% of your spell power " ..
          "as Nature damage. Moonfire: a dive on your Moonfire target extends it by 2 sec, up to 6 sec for each " ..
          "cast. Hunter's Mark: owls deal 20% more to your marked target, and the return strike also hits one other " ..
          "enemy within 8 yards for half.",
      loop = "Starfire; the owls dive on their own. Wrath their prey to call them back.",
      mods = {
        { name = "Moonpiercer",
          text = "Owls deal 40% more damage to their target: 52.5% of the Starfire's damage." },
        { name = "Silver Wings",
          text = "Each dive also hits up to 4 other enemies within 6 yards for half its damage." },
        { name = "Lunar Flock",
          text = "Each owl strikes 3 times, 1 sec apart, for 40% of its damage each (120% in all), laying Moonlit " ..
              "Prey once. The last strike makes your next Starfire cost 20% less mana." },
        { name = "Sentinel of the Hunt",
          text = "With your Hunter's Mark on an enemy, Starfire calls a Sentinel instead: it strikes your marked " ..
              "target at once, after 3 sec and after 6 sec, for 45% of an owl's damage each, following your mark. A " ..
              "new Starfire replaces it." },
        { name = "Frostfeather",
          text = "Half of each owl's dive is Frost damage. A Frostbolt hit on an owl's target before it dives " ..
              "bursts for 25% of your spell power on it and up to 2 other enemies within 6 yards, once for each " ..
              "owl." },
        { name = "Swift Judgment",
          text = "Owls dive 0.1 sec after Starfire instead of 1. Each dive makes your next Starfire cast 10% faster." },
        { name = "Patient Vigil",
          text = "Owls wait 3 sec, then dive for 30% more damage." },
        { name = "Lunar Guidance",
          text = "Owls deal 15% more damage to your Moonfire target and extend that Moonfire by 4 sec instead of 2, " ..
              "up to 8 sec for each cast." },
        { name = "Command the Hunt",
          text = "Your Arcane Shot or Aimed Shot on your Hunter's Mark target, while an owl or Sentinel is out, " ..
              "commands it to strike that target for 25% of your spell power, once every 6 sec." },
        { name = "Sun and Moon",
          text = "The return strike also deals 15% of your spell power as Nature damage to up to 4 other enemies " ..
              "within 6 yards." },
        { name = "Lunar Wound",
          text = "An owl's strike wounds its target for as much again over 6 sec. A new wound keeps what is left of " ..
              "the last." },
        { name = "Guiding Wings",
          text = "Every third owl makes your next Starfire within 5 sec instant, and you move 20% faster for 5 sec." },
        { name = "Twilight Hunt",
          text = "The return strike makes your next Wrath on that target within 5 sec deal 25% more damage." },
        { name = "Hunter's Pursuit",
          text = "An owl or Sentinel striking your Hunter's Mark target lays Exposed Prey (6 sec, once every 4 " ..
              "sec): your next Arcane Shot, Aimed Shot or Steady Shot on it deals 25% of your spell power more." },
        { name = "Winter's Blessing",
          text = "An owl diving on a target slowed by your Frostbolt lays Moonfrost (6 sec): your next Frostbolt on " ..
              "it deals 20% more damage and extends your Moonfire there by 2 sec." },
      },
      builds = {
        { name = "The Hunter", mods = { "Moonpiercer", "Swift Judgment", "Lunar Wound" },
          text = "Quick owls, deep wounds." },
        { name = "The Flock", mods = { "Silver Wings", "Patient Vigil", "Guiding Wings" },
          text = "Big dives on packs, on the move." },
        { name = "The Watcher", mods = { "Lunar Flock", "Sun and Moon", "Twilight Hunt" },
          text = "Many strikes, and Wrath to follow." },
      },
    },
    { id = 40,
      name = "Rune of Astral Convergence",
      role = "Ranged DPS (Druid, Balance)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_AstralConvergence",
      keywords = "druid balance moonkin wrath starfire sun moon eclipse sunlight moonlight moonfire insect swarm " ..
          "lightning bolt flame shock frostbolt barkskin",
      slots = { "Collision", "Alignment", "Flare" },
      builtFrom = "Wrath",
      spentWith = "Starfire, then Wrath",
      resource = "Sunlight and Moonlight, 5 enemies",
      core = "Wrath leaves Sunlight on its target for 18 sec. Starfire on it collides with it for 80% of your " ..
          "Arcane spell power, and leaves Moonlight for 18 sec. Your next Wrath on it flares for 40% of your Nature " ..
          "spell power and leaves fresh Sunlight. Up to 5 enemies carry your marks. Moonfire: a collision on your " ..
          "Moonfire target also hits one other enemy within 8 yards for 40% of it. Insect Swarm: each flare on its " ..
          "target extends it by 2 sec, up to 6 sec per cast. Lightning Bolt: on Moonlight, it flares for 75% " ..
          "without leaving Sunlight, once every 6 sec per enemy.",
      loop = "Wrath, Starfire, Wrath on the same enemy: the last Wrath starts it again.",
      mods = {
        { name = "Perfect Alignment",
          text = "The collision deals 40% more damage: 112% of your Arcane spell power." },
        { name = "Converging Skies",
          text = "The collision also hits up to 4 other enemies within 6 yards for half its damage." },
        { name = "Astral Rupture",
          text = "Instead of a hit, the collision ruptures its target for 150% of its damage over 6 sec. A new " ..
              "rupture keeps what is left of the last. A flare on it deals its next tick at once, without adding " ..
              "a tick." },
        { name = "Flame Eclipse",
          text = "A collision on your Flame Shock target also deals 25% of your Fire spell power as Fire damage." },
        { name = "Frozen Eclipse",
          text = "A Frostbolt hit on your Sunlight target makes your next collision deal 25% more damage, once " ..
              "per Sunlight." },
        { name = "Long Twilight",
          text = "Your marks last 22 sec instead of 18. Each 2 sec Sunlight was on its target adds 5% to the " ..
              "collision, up to 20%." },
        { name = "Crossed Horizons",
          text = "A collision also strikes up to 3 other enemies within 12 yards that carry your Moonlight, for 35% " ..
              "of its damage. Their marks stay." },
        { name = "Steady Orbit",
          text = "Each flare makes your next Wrath or Starfire within 5 sec instant." },
        { name = "Tidal Flare",
          text = "A flare from your Lightning Bolt also hits one other enemy within 8 yards for half its damage." },
        { name = "Twin Moons",
          text = "A collision on your Moonfire target spreads that Moonfire, for what is left of it, to one other " ..
              "enemy within 8 yards, once every 8 sec." },
        { name = "Solar Return",
          text = "The flare deals 60% more damage: 64% of your Nature spell power." },
        { name = "Lunar Dominance",
          text = "Each flare makes your next Starfire within 5 sec cast 20% faster and cost 20% less mana." },
        { name = "Eventide",
          text = "Each flare leaves a field 6 yards wide for 4 sec: every second, 10% of your Arcane spell power " ..
              "to up to 5 enemies in it." },
        { name = "Insect Renewal",
          text = "A flare on your Insect Swarm target makes your next Wrath within 5 sec cast 15% faster." },
        { name = "Astral Ward",
          text = "A flare while Barkskin is up shields you for 40% of your spell power for 6 sec, once every " ..
              "12 sec." },
      },
      builds = {
        { name = "The Eclipse", mods = { "Perfect Alignment", "Long Twilight", "Solar Return" },
          text = "Patient, heavy hits." },
        { name = "The Constellation", mods = { "Converging Skies", "Crossed Horizons", "Lunar Dominance" },
          text = "Marks across the pack." },
        { name = "The Wanderer", mods = { "Astral Rupture", "Steady Orbit", "Eventide" },
          text = "Burns and fields, on the move." },
        { name = "The Swarm", mods = { "Astral Rupture", "Twin Moons", "Insect Renewal" },
          text = "Moonfire and Insect Swarm kept alive." },
      },
    },
    { id = 41,
      name = "Rune of Ursoc's Champion",
      role = "Tank (Druid, Guardian)",
      roles = { "TANK" },
      icon = "Ability_Rune_UrsocsChampion",
      keywords = "druid guardian bear tank barkskin maul shield absorb ursoc mangle swipe earth shock earth shield " ..
          "thunder clap rake lifebloom rejuvenation impact",
      slots = { "Impact", "Guard", "Resolve" },
      builtFrom = "Barkskin, in Bear Form",
      spentWith = "Maul",
      resource = "Ursoc's Guard, 16 sec",
      core = "Barkskin in Bear Form also raises Ursoc's Guard for 16 sec, an absorb of 30% of your maximum health, " ..
          "and comes back 30 sec sooner. Maul in it releases the impact in a 10-yard cone in front of you (up to 5 " ..
          "enemies): 75% of your attack power plus all the guard absorbed (that part at most 250% of your attack " ..
          "power). Each Maul in Bear Form brings Barkskin back 3 sec sooner. Leaving Bear Form ends the guard. " ..
          "Mangle in the guard marks its target: the impact there also deals 20% of your attack power to up to 4 " ..
          "enemies near it, with threat. Swipe in the guard makes it last 1 sec longer, once. Earth Shock: your " ..
          "next impact deals 15% more, half of it as Nature damage.",
      loop = "Barkskin in Bear Form, Mangle and Swipe, then Maul for the impact.",
      mods = {
        { name = "Crushing Champion",
          text = "The impact deals 30% more damage." },
        { name = "Packbreaker",
          text = "The impact strikes up to 8 enemies instead of 5." },
        { name = "Unbroken Champion",
          text = "The impact comes when the guard ends instead of with Maul, for 125% of its damage." },
        { name = "Stoneguard",
          text = "Your Earth Shield healing you during the guard makes the impact deal 20% more. No extra charge is " ..
              "used." },
        { name = "Thunderbear",
          text = "Thunder Clap during the guard makes the impact also deal 20% of your attack power as Nature " ..
              "damage to each enemy it hits." },
        { name = "Long Guard",
          text = "The guard lasts 2 sec longer; Swipe no longer lengthens it." },
        { name = "Earthen Hide",
          text = "The guard absorbs 20% more." },
        { name = "Ursoc's Challenge",
          text = "Mangle's mark also makes its target deal 15% less damage for 3 sec." },
        { name = "Rending Maul",
          text = "The impact on your Rake target makes it bleed for 30% of the impact's damage over 6 sec." },
        { name = "Earthshock Roar",
          text = "Earth Shock's empowering also makes the guard last 1 sec longer, once for each guard." },
        { name = "Defiant Roar",
          text = "The impact generates 50% more threat." },
        { name = "Restoring Force",
          text = "The impact heals you for 20% of its damage, up to 3% of your maximum health." },
        { name = "Bear's Momentum",
          text = "After the impact, your next Maul within 6 sec deals 20% more damage." },
        { name = "Lifebloom Hide",
          text = "The impact while your Lifebloom is on you heals you for 40% of your spell power, up to 3% of your " ..
              "maximum health." },
        { name = "Shielding Roots",
          text = "Barkskin while your Rejuvenation is on you also shields you for 3% of your maximum health for 4 " ..
              "sec." },
      },
      builds = {
        { name = "The Bulwark", mods = { "Packbreaker", "Earthen Hide", "Defiant Roar" },
          text = "Hold the pack and hold the line." },
        { name = "The Breaker", mods = { "Crushing Champion", "Long Guard", "Bear's Momentum" },
          text = "Soak, then crush." },
        { name = "The Stoneheart", mods = { "Unbroken Champion", "Ursoc's Challenge", "Restoring Force" },
          text = "Let the guard break on its own." },
      },
    },
    { id = 42,
      name = "Rune of the Duelist",
      role = "Melee DPS (Rogue, Combat)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_Duelist",
      keywords = "rogue combat sinister strike sabre slash pistol shot broadside gun explosion combo point energy " ..
          "eviscerate fire blast lightning shield hunters mark icy touch renew arcane shot powder",
      slots = { "Pistol", "Powder", "Footwork" },
      builtFrom = "Sabre Slash (Sinister Strike)",
      spentWith = "Pistol Shot (Arcane Shot) and Broadside (Rapid Fire)",
      resource = "Opening Shot, one held",
      core = "Sinister Strike becomes Sabre Slash: its damage and combo point, a sabre's look. Every third Sabre " ..
          "Slash that hits readies Opening Shot for 8 sec (one held), for Pistol Shot: 10 Energy, 6 sec cooldown, " ..
          "an enemy within 20 yards, 60% of your attack power as Physical damage and 1 combo point. After a Pistol " ..
          "Shot, your next Sabre Slash within 6 sec deals 15% more and also strikes one enemy nearby for 40%. " ..
          "Eviscerate while Opening Shot is ready makes your next Pistol Shot deal 10% more for each combo point " ..
          "(50% at most). Fire Blast: a hit makes your next Pistol Shot within 8 sec explode for 20% of your attack " ..
          "power plus 20% of your spell power as Fire on up to 3 enemies. Broadside, a finishing move (35 Energy, " ..
          "20 sec cooldown): three pistol blasts, each 30% of your attack power plus 5% per combo point, to its " ..
          "target and up to 7 enemies within 8 yards. Arcane Shot becomes Pistol Shot and Rapid Fire becomes " ..
          "Broadside: each is yours only while you know the ability it replaces. The pistol is conjured.",
      loop = "Sabre Slash three times, Pistol Shot; Eviscerate first for a bigger shot.",
      mods = {
        { name = "Heavy Caliber",
          text = "Pistol Shot deals 40% more damage." },
        { name = "Gunslinger",
          text = "Pistol Shot fires twice, 65% of its damage each; only the first grants a combo point." },
        { name = "Scattershot",
          text = "Pistol Shot spreads in a cone: up to 3 other enemies in front of you, as far as 10 yards past its " ..
              "target, take half its damage." },
        { name = "Flame Pistol",
          text = "Fire Blast's powder explosion deals 35% more damage and hits up to 5 enemies." },
        { name = "Storm Pistol",
          text = "Pistol Shot while your Lightning Shield is up also deals 25% of your spell power as Nature damage " ..
              "to its target." },
        { name = "Powder and Steel",
          text = "The Sabre Slash after a Pistol Shot deals 30% more instead of 15%." },
        { name = "Between the Eyes",
          text = "Eviscerate while Opening Shot is ready makes your next Pistol Shot deal 15% more for each combo " ..
              "point instead of 10% (75% at most)." },
        { name = "Loaded Powder",
          text = "Fire Blast's powder explosion deals 25% more damage and hits up to 5 enemies." },
        { name = "Marked Shot",
          text = "Pistol Shot on your Hunter's Mark target makes your next Sabre Slash within 6 sec deal 25% more " ..
              "damage." },
        { name = "Frozen Powder",
          text = "Pistol Shot on your Frost Fever target bursts for 20% of your attack power as Frost damage on up " ..
              "to 3 enemies within 6 yards, once every 8 sec." },
        { name = "Quick Draw",
          text = "Every second Sabre Slash readies Opening Shot instead of every third." },
        { name = "Cut and Run",
          text = "Pistol Shot makes you move 25% faster for 4 sec." },
        { name = "Efficient Duelist",
          text = "Pistol Shot gives its 10 Energy back when it hits, once every 6 sec." },
        { name = "Healing Powder",
          text = "Pistol Shot while your Renew is on you heals you for 30% of your spell power, once every 10 sec." },
        { name = "Arcane Powder",
          text = "Your Arcane Shot makes your next Pistol Shot within 8 sec deal 25% more damage." },
      },
      builds = {
        { name = "The Gunslinger", mods = { "Gunslinger", "Powder and Steel", "Quick Draw" },
          text = "Shot, slash, shot again." },
        { name = "The Executioner", mods = { "Heavy Caliber", "Between the Eyes", "Efficient Duelist" },
          text = "Eviscerate, then one big shot." },
        { name = "The Scatterer", mods = { "Scattershot", "Loaded Powder", "Cut and Run" },
          text = "Powder and lead over the pack." },
      },
    },
    { id = 43,
      name = "Rune of the Stormreaver",
      role = "Melee DPS (Shaman, Enhancement)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_Stormreaver",
      keywords = "shaman enhancement stormstrike lava lash lightning shield storm thunder nature melee earth shock " ..
          "flame shock hunters mark frost shock shield slam lightning bolt thunder clap",
      slots = { "Storm", "Thunderlash", "Shield" },
      builtFrom = "Stormstrike",
      spentWith = "Lava Lash and your melee abilities",
      resource = "Stormfront, 16 sec",
      core = "Stormstrike raises the Stormfront around you for 16 sec (Stormstrike again refreshes it): every 2 sec " ..
          "it strikes up to 5 enemies within 8 yards for 25% of your attack power as Nature damage. In it, Lava " ..
          "Lash becomes Thunderlash: 60% of your attack power more to its target, and arcs of 30% to 2 more enemies " ..
          "within 8 yards. Your melee abilities discharge Lightning Shield at their target, once a second: its own " ..
          "damage, a third of your spell power and 20% of your attack power, and the shield keeps its charges. " ..
          "Earth Shock in the Stormfront makes its next pulse deal 20% more. Thunderlash on your Flame Shock target " ..
          "makes the Flame Shock last 2 sec longer (up to 6 sec for each cast).",
      loop = "Stormstrike to keep the storm up, Lava Lash in it, and strike to discharge the shield.",
      mods = {
        { name = "Eye of the Tempest",
          text = "The Stormfront deals 120% more damage while only one enemy is in it." },
        { name = "Forked Cataclysm",
          text = "The Stormfront strikes up to 8 enemies within 12 yards." },
        { name = "Rolling Thunder",
          text = "The Stormfront strikes every second instead of every 2, each strike 70% of its damage." },
        { name = "Stormhunter",
          text = "Stormfront pulses deal 35% more to your Hunter's Mark target and also strike one other enemy " ..
              "within 8 yards for 40%." },
        { name = "Frozen Front",
          text = "Frost Shock in the Stormfront leaves a frost trail under its target for 4 sec: 10% of your attack " ..
              "power as Frost damage each second to up to 3 enemies, once every 8 sec." },
        { name = "Thunderhead",
          text = "Thunderlash doesn't arc: 120% of your attack power to its target instead." },
        { name = "Chainbreaker",
          text = "Thunderlash arcs to 4 enemies instead of 2, each for 30% of your attack power." },
        { name = "White-Hot Conductor",
          text = "Thunderlash burns its target for 100% of your attack power over 6 sec instead of its Nature hit; " ..
              "its arcs stay. A new burn keeps what is left of the last." },
        { name = "Stormhammer",
          text = "Shield Slam in the Stormfront sends an arc of 30% of your attack power as Nature damage to its " ..
              "target and one other enemy within 8 yards, once every 6 sec." },
        { name = "Flame Conductor",
          text = "Thunderlash on your Flame Shock target spreads it to one other enemy within 8 yards for what is " ..
              "left of it, once every 10 sec." },
        { name = "Overcharged Core",
          text = "The rune's Lightning Shield discharges deal 50% more damage." },
        { name = "Ride the Lightning",
          text = "Each of the rune's discharges makes you move 30% faster for 3 sec and brings Lava Lash back 1 sec " ..
              "sooner." },
        { name = "Violent Grounding",
          text = "Each of the rune's discharges also strikes up to 3 other enemies within 8 yards of its target for " ..
              "half its damage." },
        { name = "Static Strike",
          text = "Lightning Bolt in the Stormfront makes your next discharge within 6 sec strike again for 40% of " ..
              "its damage." },
        { name = "Warstorm",
          text = "Thunder Clap in the Stormfront also deals 25% of your attack power as Nature damage to up to 5 " ..
              "enemies within 8 yards, once every 8 sec." },
      },
      builds = {
        { name = "The Tempest", mods = { "Eye of the Tempest", "Thunderhead", "Overcharged Core" },
          text = "Focused pressure." },
        { name = "The Cataclysm", mods = { "Forked Cataclysm", "Chainbreaker", "Violent Grounding" },
          text = "Lightning on the pack." },
        { name = "The Rider", mods = { "Rolling Thunder", "White-Hot Conductor", "Ride the Lightning" },
          text = "Mobile pressure." },
      },
    },
    { id = 44,
      name = "Rune of Ember's Wrath",
      role = "Melee DPS (Druid, Feral)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_EmbersWrath",
      keywords = "druid feral cat form fire blast rake rip ferocious bite swipe mangle cinder wound burn bleed fire " ..
          "mage energy flame shock claw blazing strike fiery explosion shaman shred ferocious bite flame shock icy " ..
          "touch pyroblast swipe rejuvenation rake spread",
      slots = { "Ignition", "Claws", "Instinct" },
      builtFrom = "Rake or Rip",
      spentWith = "Fire Blast, in Cat Form too",
      resource = "Cinder Wound, 6 sec",
      core = "Your Fire Blast works in Cat Form, at its own mana cost and cooldown. On an enemy bleeding from your " ..
          "own Rake or Rip it ignites Cinder Wound: 150% of your attack power plus 100% of your spell power as Fire " ..
          "damage over 6 sec, a tick every 2 sec. The bleed stays; a new wound keeps what is left of the last, up " ..
          "to twice a fresh one. Shred on your Cinder Wound adds 10% of your attack power as Fire, once every 3 sec " ..
          "for each enemy. Ferocious Bite on it makes it last 1 sec longer for each combo point (5 sec for each " ..
          "wound). Flame Shock: Fire Blast on your Flame Shock target with your Cinder Wound spreads the wound to " ..
          "one more enemy within 8 yards, once every 8 sec. Flame Shock (in Cat Form too) kindles your next Claw or " ..
          "Swipe within 10 sec: Claw becomes Blazing Strike, its hit again on a second enemy within 5 yards and " ..
          "both bleeding for 100% of your attack power over 6 sec; Swipe becomes Fiery Explosion, 75% of your " ..
          "attack power as Fire damage to up to 8 enemies within 8 yards, each left with half a Cinder Wound.",
      loop = "Rake or Rip, Fire Blast to ignite, Shred and Bite to feed it; Flame Shock, then Claw or Swipe.",
      spells = {
        ["Fire Blast"] = "Usable in Cat Form. On an enemy bleeding from your own Rake or Rip, ignites Cinder " ..
            "Wound: 150% of your attack power plus 100% of your spell power as Fire damage over 6 sec.",
        ["Flame Shock"] = "Usable in Cat Form. Kindles your next Claw or Swipe within 10 sec: Claw becomes " ..
            "Blazing Strike, Swipe becomes Fiery Explosion.",
        ["Claw"] = "Kindled (after Flame Shock): becomes Blazing Strike, its hit again on a second enemy within 5 " ..
            "yards, and both bleed for 100% of your attack power over 6 sec. Fire Blast ignites that bleed too.",
        ["Swipe (Cat)"] = "Kindled (after Flame Shock): becomes Fiery Explosion, 75% of your attack power as " ..
            "Fire damage to up to 8 enemies within 8 yards, each left with half a Cinder Wound.",
        ["Rake"] = "Your bleed lets Fire Blast ignite Cinder Wound on this enemy.",
        ["Rip"] = "Your bleed lets Fire Blast ignite Cinder Wound on this enemy.",
      },
      mods = {
        { name = "Cinderclaw",
          text = "Cinder Wound deals 40% more damage." },
        { name = "Wildfire Claws",
          text = "Blazing Strike hits up to 2 other enemies within 5 yards for half its damage; with none near, its " ..
              "target takes 25% more." },
        { name = "Living Flame",
          text = "Cinder Wound deals 15% more damage." },
        { name = "Flame Shock Claws",
          text = "Blazing Strike on your Flame Shock target deals 25% more as Fire damage." },
        { name = "Frozen Embers",
          text = "Fire Blast on your Frost Fever target also deals 25% of your attack power as Frost damage, once " ..
              "every 6 sec." },
        { name = "Ember Rush",
          text = "Fire Blast makes you move 20% faster for 4 sec, and your next Shred within 6 sec gives 10 Energy " ..
              "back." },
        { name = "Smoldering Bite",
          text = "Ferocious Bite on your Cinder Wound deals its next tick at once, without adding a tick." },
        { name = "Firestorm Pounce",
          text = "Flame Shock's spread reaches 3 enemies instead of 1." },
        { name = "Phoenix Claws",
          text = "Pyroblast on your Cinder Wound target bursts for 40% of your spell power as Fire damage, once " ..
              "every 8 sec." },
        { name = "Wildfire Swipe",
          text = "Swipe spreads half of what is left of a Cinder Wound in its reach to one more enemy within 8 " ..
              "yards, once every 8 sec." },
        { name = "Ashen Renewal",
          text = "Igniting a Cinder Wound gives you 10 Energy, once every 8 sec." },
        { name = "Flame-Tipped Claws",
          text = "Blazing Strike deals 40% more damage to its target, as Fire." },
        { name = "Phoenix Scar",
          text = "When a Cinder Wound runs out, it deals 25% of your attack power as Fire damage to its target." },
        { name = "Healing Flame",
          text = "Fire Blast on a bleeding enemy while your Rejuvenation is on you heals you for 2% of your maximum " ..
              "health, once every 10 sec." },
        { name = "Burning Rake",
          text = "Rake on your Cinder Wound target makes your next Blazing Strike within 6 sec deal 30% more " ..
              "damage." },
      },
      builds = {
        { name = "The Pyre", mods = { "Cinderclaw", "Smoldering Bite", "Phoenix Scar" },
          text = "One deep wound, fed and burst." },
        { name = "The Wildfire", mods = { "Wildfire Claws", "Firestorm Pounce", "Flame-Tipped Claws" },
          text = "Flame Shock and claws through the pack." },
        { name = "The Ember", mods = { "Living Flame", "Ember Rush", "Ashen Renewal" },
          text = "Fast and fed with Energy." },
      },
    },
    { id = 45,
      name = "Rune of Whitefang",
      role = "Melee DPS (Druid, Feral)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_Whitefang",
      keywords = "druid feral cat form icy touch shred mangle ferocious bite frost frozen prey shatter death knight " ..
          "energy frost shock claw frostfang glacial burst shaman rake ferocious bite frostbolt lightning shield " ..
          "flame shock hunters mark moonfire rejuvenation",
      slots = { "Shatter", "Hunt", "Instinct" },
      builtFrom = "Icy Touch, in Cat Form too",
      spentWith = "Shred or Mangle",
      resource = "Frozen Prey, 16 sec",
      core = "Your Icy Touch, in Cat Form or not (its Frost rune, damage and Frost Fever as ever), lays Frozen Prey " ..
          "on the enemy for 6 sec; your next Shred or Mangle on it shatters it for 150% of your attack power as " ..
          "Frost damage. One mark at a time, one shatter a strike. With your Rake on it, the shatter also leaves " ..
          "Frozen Wound: 25% of your attack power as Frost damage over 6 sec; Ferocious Bite on Frozen Wound deals " ..
          "its next tick at once. Frostbolt: a hit on Frozen Prey makes its shatter deal 25% more and the mark last " ..
          "2 sec longer, once for each mark. Frost Shock (in Cat Form too) chills your next Claw or Swipe within 10 " ..
          "sec: Claw becomes Frostfang Strike, its hit again on a second enemy within 5 yards and both frostbitten " ..
          "for 100% of your attack power over 6 sec and 30% slower; Swipe becomes Glacial Burst, 75% of your attack " ..
          "power as Frost damage to up to 8 enemies within 8 yards, each slowed 50% for 4 sec and its Frozen Prey " ..
          "shattered.",
      loop = "Icy Touch, Rake, then Shred or Mangle to shatter; Bite the wound.",
      spells = {
        ["Icy Touch"] = "Usable in Cat Form. Lays Frozen Prey on the enemy for 16 sec; your next Shred or Mangle " ..
            "on it shatters it for 150% of your attack power as Frost damage.",
        ["Frost Shock"] = "Usable in Cat Form. Chills your next Claw or Swipe within 10 sec: Claw becomes " ..
            "Frostfang Strike, Swipe becomes Glacial Burst.",
        ["Claw"] = "Frostbitten (after Frost Shock): becomes Frostfang Strike, its hit again on a second enemy " ..
            "within 5 yards, and both frostbitten for 100% of your attack power over 6 sec and 30% slower.",
        ["Swipe (Cat)"] = "Frostbitten (after Frost Shock): becomes Glacial Burst, 75% of your attack power as " ..
            "Frost damage to up to 8 enemies within 8 yards, each slowed 50% for 4 sec and its Frozen Prey " ..
            "shattered.",
        ["Shred"] = "Shatters your Frozen Prey on this enemy for 150% of your attack power as Frost damage.",
        ["Mangle (Cat)"] = "Shatters your Frozen Prey on this enemy for 150% of your attack power as Frost damage.",
      },
      mods = {
        { name = "Predator's Fang",
          text = "The shatter deals 40% more damage." },
        { name = "Pack of Winter",
          text = "Shattering lays a half-strength Frozen Prey on one other enemy within 8 yards for 6 sec. It " ..
              "doesn't spread again." },
        { name = "Glacial Claws",
          text = "The shatter also hits up to 3 other enemies within 8 yards for half its damage." },
        { name = "Stormfang",
          text = "The shatter while your Lightning Shield is up also deals 20% of your spell power as Nature " ..
              "damage." },
        { name = "Flamefang",
          text = "The shatter on your Flame Shock target also deals 20% of your spell power as Fire damage." },
        { name = "Frozen Wound",
          text = "Frozen Wound deals 50% more damage." },
        { name = "Cold Blood",
          text = "Ferocious Bite on your Frozen Wound also deals 10% of your attack power as Frost damage for each " ..
              "combo point." },
        { name = "Predator's Chill",
          text = "Frostbolt's empowering makes the shatter deal 40% more instead of 25%." },
        { name = "Marked Prey",
          text = "The shatter on your Hunter's Mark target also hits one other enemy within 8 yards for half its " ..
              "damage." },
        { name = "Moonfrost",
          text = "The shatter on your Moonfire target extends that Moonfire by 2 sec, up to 6 sec for each cast." },
        { name = "Swift Hunt",
          text = "Shattering makes you move 20% faster for 4 sec." },
        { name = "Winter's Recovery",
          text = "Shattering gives you 10 Energy, once every 6 sec." },
        { name = "Lingering Frost",
          text = "Shattering slows the enemy by 30% for 3 sec, and your next Shred deals 15% more damage." },
        { name = "Mending Fang",
          text = "Shattering while your Rejuvenation is on you heals you for 2% of your maximum health, once every " ..
              "10 sec." },
        { name = "Clawing Cold",
          text = "Shattering a mark your Frostbolt empowered makes your next Shred within 6 sec cost 10 less " ..
              "Energy." },
      },
      builds = {
        { name = "The Fang", mods = { "Predator's Fang", "Frozen Wound", "Winter's Recovery" },
          text = "One deep bite and a wound." },
        { name = "The Pack", mods = { "Glacial Claws", "Cold Blood", "Swift Hunt" },
          text = "Shatter through a pack." },
        { name = "The Frostcaller", mods = { "Stormfang", "Predator's Chill", "Clawing Cold" },
          text = "Frostbolt the prey, then shatter it." },
      },
    },
    { id = 46,
      name = "Rune of Red Communion",
      role = "Tank (Death Knight, Blood; Warlock, Affliction)",
      roles = { "TANK" },
      icon = "Ability_Rune_RedCommunion",
      keywords = "death knight blood boil blood strike link tether crimson communion sanguine recall absorb shadow " ..
          "warlock affliction tank heal corruption mangle rend earth shield death coil holy shock",
      slots = { "Communion", "Reclamation", "Recall" },
      builtFrom = "Blood Boil (Crimson Communion)",
      spentWith = "Blood Strike and Sanguine Recall (Death Strike)",
      resource = "Crimson Links, 5 at once",
      core = "Blood Boil becomes Crimson Communion (20 Rage, 6 sec cooldown, no rune): as ever, and it links up to " ..
          "5 enemies it hits to you for 10 sec (your selected enemy first, then the nearest). Your Blood Strike on " ..
          "a linked enemy heals you for 1.5% of your maximum health plus 0.15% for each other link within 20 yards " ..
          "(2.1% at most) and pulses 20% of your attack power as Shadow damage to each other link, once every 2 " ..
          "sec. Death Strike becomes Sanguine Recall (18 sec, free, off the global cooldown): each link within 20 " ..
          "yards takes 30% of your attack power as Shadow damage, and you gain an absorb of 3% of your maximum " ..
          "health plus 0.5% for each further link (5% at most) for 5 sec. As every Tank Collection rune: NPC " ..
          "attacks never critically strike you, and your taunt (Taunt, Growl, Dark Command or Righteous Defense) " ..
          "becomes Challenge (free, 30 yards, 8 sec). Corruption: Blood Strike on a linked enemy with your " ..
          "Corruption makes it last 1 sec longer (up to 4 sec for each cast), once every 2 sec.",
      loop = "Blood Boil to link the pull; Blood Strike a linked enemy; Sanguine Recall before a big hit.",
      spells = {
        ["Death Strike"] = "Becomes Sanguine Recall.",
        ["Blood Boil"] = "Crimson Communion: links up to 5 enemies it hits to you for 10 sec. Costs 20 Rage and " ..
            "has a 6 sec cooldown instead of its Blood rune.",
        ["Blood Strike"] = "On a linked enemy: heals you for 1.5% of your maximum health (more for each other link) " ..
            "and pulses 20% of your attack power as Shadow damage to the other links, once every 2 sec.",
      },
      mods = {
        { name = "Sovereign Bond",
          text = "Your selected linked enemy deals 8% less damage to you while within 20 yards." },
        { name = "Bloodbound Court",
          text = "Up to 8 enemies are linked. The heal and the absorb keep their caps." },
        { name = "Long Veins",
          text = "Links last 15 sec and reach 30 yards." },
        { name = "Corrupt Communion",
          text = "Crimson Communion also lays your Corruption on up to 2 enemies it links, once every 12 sec." },
        { name = "Thorned Communion",
          text = "Mangle on a linked enemy also reclaims blood and pulses, as Blood Strike does (sharing its 2 " ..
              "sec)." },
        { name = "Crimson Feast",
          text = "Blood Reclamation heals 2% of your maximum health, 2.6% at most." },
        { name = "Arterial Echo",
          text = "Blood Strike also leaves 35% of your attack power as Shadow damage over 4 sec on its linked " ..
              "victim. A refresh keeps what is unpaid." },
        { name = "Shared Weakness",
          text = "Communion pulses make their enemies deal 8% less magic damage to you for 4 sec." },
        { name = "Rending Communion",
          text = "A reclaiming Blood Strike on your Rend target also deals 25% of your attack power as Physical " ..
              "damage." },
        { name = "Earthbound Blood",
          text = "Reclaiming while your Earth Shield is on you also shields you for 1% of your maximum health for 4 " ..
              "sec, once every 6 sec." },
        { name = "Coagulated Throne",
          text = "Sanguine Recall's absorb starts at 5% of your maximum health, 7% at most." },
        { name = "Rapid Recall",
          text = "Sanguine Recall's cooldown is 14 sec." },
        { name = "Blood for Blood",
          text = "Sanguine Recall also heals you for 2% of your maximum health and deals 45% of your attack " ..
              "power to each link." },
        { name = "Death's Recall",
          text = "Death Coil on a linked enemy brings Sanguine Recall back 2 sec sooner, once every 8 sec." },
        { name = "Holy Communion",
          text = "Holy Shock healing you while you have a link shields you for 2% of your maximum health for 4 sec, " ..
              "once every 10 sec." },
      },
      builds = {
        { name = "Boss Anchor", mods = { "Sovereign Bond", "Crimson Feast", "Coagulated Throne" },
          text = "One enemy held, steady recovery, a large shield." },
        { name = "Distributed Suppression", mods = { "Bloodbound Court", "Shared Weakness", "Rapid Recall" },
          text = "More links, magic suppressed, frequent recalls." },
        { name = "Mobile Recovery", mods = { "Long Veins", "Arterial Echo", "Blood for Blood" },
          text = "Long links, damage over time, healing on Recall." },
      },
    },
    { id = 47,
      name = "Rune of the Primal Worldwarden",
      role = "Tank (Shaman, Enhancement; Druid, Guardian)",
      roles = { "TANK" },
      icon = "Ability_Rune_PrimalWorldwarden",
      keywords = "shaman enhancement earth shock flame shock frost shock stormstrike lava lash magma totem " ..
          "worldheart shield druid guardian tank blood strike judgment consecration earth shield thunder clap death " ..
          "coil",
      slots = { "Shockcraft", "Worldheart", "Enhancement" },
      builtFrom = "Earth, Flame or Frost Shock",
      spentWith = "Stormstrike and Lava Lash (Magma Lash)",
      resource = "The Worldheart Totem's last command, 10 sec",
      core = "Stoneskin Totem becomes Worldheart Totem (6 sec): a totem follows 4 yards behind you for 30 min. With " ..
          "a shield on, it gives you Worldguard: 8% less damage taken and 200% more threat. Your Shocks command it: " ..
          "Earth Shock an absorb of 3% of your maximum health for 4 sec; Flame Shock 25% of your attack power as " ..
          "Fire damage to up to 5 enemies within 8 yards of it; Frost Shock slows up to 5 there by 35% and has them " ..
          "deal 6% less damage to you for 4 sec. Stormstrike repeats your last command at 60%, once every 6 sec. " ..
          "Lava Lash becomes Magma Lash: a magma line from you to the totem for 4 sec, 10% of your attack power as " ..
          "Fire damage each second to up to 5 enemies on it. Totemic Recall becomes Place Worldheart (8 sec), which " ..
          "anchors the totem at your target within 20 yards for 6 sec. NPC attacks never critically strike you, and " ..
          "your taunt becomes Challenge (free, 30 yards, 8 sec).",
      loop = "Worldheart Totem and a shield; a Shock for the command you need; Stormstrike to repeat it.",
      spells = {
        ["Stoneskin Totem"] = "Becomes Worldheart Totem.",
        ["Totemic Recall"] = "Becomes Place Worldheart.",
        ["Earth Shock"] = "Commands the Worldheart: an absorb of 3% of your maximum health for 4 sec.",
        ["Flame Shock"] = "Commands the Worldheart: 25% of your attack power as Fire damage to up to 5 enemies " ..
            "within 8 yards of it.",
        ["Frost Shock"] = "Commands the Worldheart: up to 5 enemies within 8 yards of it slowed 35% and dealing " ..
            "6% less damage to you for 4 sec.",
        ["Stormstrike"] = "Repeats your last Shock command at 60% strength, once every 6 sec.",
        ["Lava Lash"] = "Magma Lash (with the Worldheart out): a magma line from you to the totem for 4 sec.",
      },
      mods = {
        { name = "Earthen Command",
          text = "Earth Shock's command absorbs 5% of your maximum health; its echo 3%." },
        { name = "Flame Dominion",
          text = "Flame's command deals 40% of your attack power; its echo 24%." },
        { name = "Winter Command",
          text = "Frost's command slows by 45% and reduces damage by 9%; its echo 27% and 5.4%." },
        { name = "Bloodstone Command",
          text = "Blood Strike within 6 sec of an Earth command shields you for 2% of your maximum health for 4 " ..
              "sec, once for each command." },
        { name = "Judgment of the Elements",
          text = "Judgment repeats your last Shock command (within 10 sec) at half strength, once every 8 sec." },
        { name = "Deepstone Heart",
          text = "Worldguard reduces damage taken by 11%. A shield is still needed." },
        { name = "Wide Communion",
          text = "Flame and Frost commands reach 12 yards and up to 8 enemies; echoes too." },
        { name = "Striding Heart",
          text = "Place Worldheart's cooldown is 5 sec, and placing it makes you move 25% faster for 3 sec." },
        { name = "Consecrated Heart",
          text = "Standing in your Consecration with Worldguard up shields you for 2% of your maximum health for 4 " ..
              "sec, once every 8 sec." },
        { name = "Earthen Communion",
          text = "Your Earth Shield healing you with Worldguard up makes your next Shock command within 6 sec 20% " ..
              "stronger, once every 8 sec." },
        { name = "Resonant Strikes",
          text = "Stormstrike's command echo can come every 4 sec." },
        { name = "Molten Causeway",
          text = "Magma Lash's line deals 16% of your attack power a tick and is 5 yards wide." },
        { name = "Stoneblood Lash",
          text = "Lava Lash also heals you for 1.5% of your maximum health, once every 4 sec." },
        { name = "Thunderheart",
          text = "Thunder Clap repeats your last Shock command (within 10 sec) at half strength, once every 8 sec." },
        { name = "Runic Renewal",
          text = "Death Coil with Worldguard up heals you for 30% of your spell power, at most 2% of your maximum " ..
              "health, once every 8 sec." },
      },
      builds = {
        { name = "Earth Bulwark", mods = { "Earthen Command", "Deepstone Heart", "Resonant Strikes" },
          text = "Reliable absorbs, constant mitigation, frequent Earth echoes." },
        { name = "Elemental Pack", mods = { "Flame Dominion", "Wide Communion", "Molten Causeway" },
          text = "Large Fire bursts and wider fissures." },
        { name = "Winter Pursuit", mods = { "Winter Command", "Striding Heart", "Stoneblood Lash" },
          text = "Enemies held back, a quick totem, melee healing." },
      },
    },
    { id = 48,
      name = "Rune of the Black Covenant",
      role = "Tank (Warlock; Death Knight, Blood)",
      roles = { "TANK" },
      icon = "Ability_Rune_BlackCovenant",
      keywords = "warlock voidwalker searing pain drain life covenant armor bind voidwalker shadowflame soul siphon " ..
          "intercession curse of weakness curse of tongues demon tank blood strike earth shield corruption icy " ..
          "touch frost fever judgment blood boil",
      slots = { "Covenant Armor", "Siphon", "Intercession" },
      builtFrom = "Bind: Voidwalker (in Summon Voidwalker's place)",
      spentWith = "Covenant Lash, Soul Siphon and Dark Intercession",
      resource = "Covenant Armor, until unbound",
      core = "Summon Voidwalker becomes Bind: Voidwalker (2 sec cast, 10 sec): a Voidwalker binds to you as " ..
          "Covenant Armor until you cast it again: 50% more armor (added to your other armor bonuses, never " ..
          "multiplying them), 15% more maximum health, 200% more threat, 6% less damage taken, and a Voidwalker's " ..
          "purple shadow about you. While bound, Searing Pain becomes Covenant Lash (instant, 10 yards, 8% of base " ..
          "mana: 100% of your spell power as Shadowflame damage, 100% more threat) and, if you know Drain Life, " ..
          "Drain Life becomes Soul Siphon (a 3 sec channel at 70% speed, 6% of base mana, 15 sec cooldown: each " ..
          "second 25% of your spell power as Shadow damage and 1.5% of your maximum health back). Shadow Ward " ..
          "becomes Dark Intercession (30 sec): 15% less damage taken for 6 sec, Covenant Lash meanwhile also " ..
          "hitting up to 4 enemies within 6 yards for 50%. Your Curse of Weakness has its victim deal 6% less " ..
          "Physical damage to you, Curse of Tongues 6% less magic damage. NPC attacks never critically strike you.",
      loop = "Bind your Voidwalker; Covenant Lash for threat, Soul Siphon to recover; Intercession before a " ..
          "big hit.",
      spells = {
        ["Shadow Ward"] = "Becomes Dark Intercession.",
        ["Summon Voidwalker"] = "Becomes Bind: Voidwalker: a Voidwalker binds to you as Covenant Armor.",
        ["Searing Pain"] = "While bound: Covenant Lash, an instant 10-yard lash for 100% of your spell power as " ..
            "Shadowflame damage with 100% more threat.",
        ["Drain Life"] = "While bound: Soul Siphon, a 3 sec channel you can move in (70% speed): 25% of your " ..
            "spell power and 1.5% of your maximum health back each second.",
        ["Curse of Weakness"] = "Its victim deals 6% less Physical damage to you.",
        ["Curse of Tongues"] = "Its victim deals 6% less magic damage to you.",
      },
      mods = {
        { name = "Voidforged Plate",
          text = "Covenant Armor also gives 2,500 armor at level 60 (a share of it at lower levels)." },
        { name = "Nether Insulation",
          text = "While bound, magic damage taken is reduced by a further 8%." },
        { name = "Hungry Covenant",
          text = "Covenant Lash gives back 1% of your base mana, once every 2 sec." },
        { name = "Blood Pact",
          text = "While bound, Blood Strike shields you for 2% of your maximum health for 4 sec, once every 8 sec." },
        { name = "Earthen Pact",
          text = "While bound, your own Earth Shield healing you also shields you for 1% of your maximum health for " ..
              "4 sec, once every 8 sec." },
        { name = "Deep Extraction",
          text = "Soul Siphon heals 2.5% of your maximum health a tick." },
        { name = "Siphoning Ward",
          text = "Each Soul Siphon tick also shields you for 1% of your maximum health for 3 sec, adding to what is " ..
              "left, up to 3%." },
        { name = "Unfettered Drain",
          text = "Soul Siphon no longer slows you, reaches 30 yards instead of 20, and deals 30% of your spell " ..
              "power a tick." },
        { name = "Corrupted Drain",
          text = "Soul Siphon on your Corruption target deals 15% more and makes it last 1 sec longer a tick (up to " ..
              "6 sec for each cast)." },
        { name = "Frostbound Siphon",
          text = "Soul Siphon on your Frost Fever target also deals 10% of your spell power as Frost damage and " ..
              "heals 0.25% more a tick." },
        { name = "Guardian Manifest",
          text = "Dark Intercession reduces damage taken by 22%." },
        { name = "Legion's Rebuke",
          text = "During Intercession, Covenant Lash hits up to 7 more enemies for 75% of its damage." },
        { name = "Frequent Petition",
          text = "Dark Intercession's cooldown is 24 sec, and it heals you for 2% of your maximum health." },
        { name = "Judgment of the Void",
          text = "Judgment during Dark Intercession lashes its target for 25% of your spell power as Shadowflame " ..
              "damage, once every 6 sec." },
        { name = "Blood Intercession",
          text = "While bound, Blood Boil brings Dark Intercession back 2 sec sooner and shields you for 1% of your " ..
              "maximum health for 4 sec, once every 8 sec." },
      },
      builds = {
        { name = "Physical Pact", mods = { "Voidforged Plate", "Deep Extraction", "Guardian Manifest" },
          text = "Armor, deep drains, a stronger defensive." },
        { name = "Magic Endurance", mods = { "Nether Insulation", "Siphoning Ward", "Frequent Petition" },
          text = "Magic mitigation, warding drains, frequent recovery." },
        { name = "Lashing Pack Tank", mods = { "Hungry Covenant", "Unfettered Drain", "Legion's Rebuke" },
          text = "Lash after lash, a Siphon on the move, a wide Intercession cleave." },
      },
    },
    { id = 49,
      name = "Rune of the Hungering Rift",
      role = "Ranged DPS (Warlock, Affliction)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_HungeringRift",
      keywords = "warlock affliction shadow bolt corruption rift lash damage over time drain soul death coil " ..
          "immolate haunt shadow word pain drain life curse of agony",
      slots = { "Maw", "Anchor", "Appetite" },
      builtFrom = "Shadow Bolt on your Corruption target",
      spentWith = "The Rift's lashes (automatic)",
      resource = "The Rift, 6 sec",
      core = "Shadow Bolt on your Corruption target opens a Hungering Rift on it for 6 sec. Every 2 sec it lashes " ..
          "the target for 25% of your spell power as Shadow damage, 5% more for each of your damage over time " ..
          "effects on it (up to 5). Channeling Drain Soul on the target makes it lash every 1.5 sec. Shadow Bolt on " ..
          "the Rift's target makes the next lash also strike another Corruption target within 8 yards for half. " ..
          "Death Coil on a Corruption target opens or renews the Rift.",
      loop = "Corruption and your other damage over time effects, then Shadow Bolt to open the Rift; keep them all " ..
          "up.",
      mods = {
        { name = "Devouring Maw",
          text = "The Rift's lashes deal 35% more damage to its target." },
        { name = "Many Tongues",
          text = "Each lash also strikes up to 2 other Corruption targets within 8 yards for 60%. Shadow Bolt on " ..
              "the target makes the next lash 20% stronger instead." },
        { name = "Black Breath",
          text = "Each lash also strikes up to 4 enemies in a 12-yard cone beyond the target for half." },
        { name = "Hellmaw",
          text = "Lashes on your Immolate target also deal 15% of your spell power as Fire damage." },
        { name = "Haunted Maw",
          text = "Lashes on your Haunt target deal 20% more and make Haunt last 1 sec longer (up to 4 sec for each " ..
              "cast)." },
        { name = "Haunting Rift",
          text = "Once its target has moved 5 yards, the next lash deals 25% more, once every 3 sec." },
        { name = "Open Wound",
          text = "The Rift stays where it opened, reaching its target within 20 yards; each lash in a row deals 10% " ..
              "more (30% at most)." },
        { name = "At Your Back",
          text = "The Rift follows you onto your selected Corruption target within 30 yards; switching targets " ..
              "lashes once for half, once every 4 sec." },
        { name = "Painful Anchoring",
          text = "Lashes on your Shadow Word: Pain target also strike one other Shadow Word: Pain target within 8 " ..
              "yards for half." },
        { name = "Siphon Conduit",
          text = "While you channel Drain Life on the Rift's target, each lash heals you for 25% of its damage (1% " ..
              "of your maximum health a second at most)." },
        { name = "Lingering Torment",
          text = "Each lash leaves 25% of its damage again as Shadow damage over 6 sec." },
        { name = "Last Gasp",
          text = "Every third lash erupts for 30% of your spell power on up to 5 enemies near the target; if the " ..
              "target dies first, it erupts for half." },
        { name = "Endless Appetite",
          text = "Lashes make your Corruption last 1 sec longer (up to 4 sec for each cast); when the target dies, " ..
              "the Rift moves to a Corruption target within 15 yards." },
        { name = "Doom Echo",
          text = "Every third lash on your Curse of Agony target also deals 25% of your spell power as Shadow " ..
              "damage." },
        { name = "Necrotic Appetite",
          text = "Death Coil on the Rift's target makes the Rift last 2 sec longer, once for each Rift." },
      },
      builds = {
        { name = "The Glutton", mods = { "Devouring Maw", "Haunting Rift", "Lingering Torment" },
          text = "One target, eaten alive." },
        { name = "The Many-Mouthed", mods = { "Many Tongues", "At Your Back", "Endless Appetite" },
          text = "Corruption on the pack; the Rift on all of it." },
        { name = "The Black Breath", mods = { "Black Breath", "Open Wound", "Last Gasp" },
          text = "A rift that breathes across the crowd." },
      },
    },
    { id = 50,
      name = "Rune of Howling North",
      role = "Melee DPS (Death Knight, Frost)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_HowlingNorth",
      keywords = "death knight frost howling blast obliterate frost strike northwind blade icy touch chains of ice " ..
          "hungering cold blood plague horn of winter death and decay death strike frost fever",
      slots = { "Blade", "Wind", "Winter" },
      builtFrom = "Howling Blast (Northwind)",
      spentWith = "Obliterate or Frost Strike",
      resource = "2 blades, Northwind on up to 5 enemies for 18 sec",
      core = "Howling Blast marks up to 5 enemies it hits with Northwind for 18 sec and readies 2 Frost blades. " ..
          "Obliterate on a marked enemy looses one for 60% of your attack power as Frost damage; Frost Strike " ..
          "looses one for 35%. Blades come 1.5 sec apart and grow with your Frost damage bonuses. Icy Touch on a " ..
          "marked enemy makes the next blade deal 20% more and pierce one more enemy for half. After a blade, Frost " ..
          "Strike on its target within 6 sec also deals 15% of your attack power as Frost damage. Chains of Ice on " ..
          "a marked enemy makes the next blade leave a frozen field for 4 sec: 10% of your spell power each second " ..
          "to up to 3 enemies.",
      loop = "Howling Blast to mark the pack, then Obliterate and Frost Strike to loose both blades.",
      mods = {
        { name = "Glacial Edge",
          text = "Your Frost blades deal 40% more damage." },
        { name = "Rimeblade",
          text = "Each loosed blade becomes 3 blades for 40% each; Frost Strike's follow-up waits for the last." },
        { name = "Winter's Reach",
          text = "Blades pierce up to 4 enemies behind their target for half." },
        { name = "Shattering Cold",
          text = "Blades deal 50% more to enemies frozen by your Hungering Cold." },
        { name = "Plaguewind",
          text = "A blade on your Blood Plague target also strikes one enemy within 8 yards for half." },
        { name = "Sharpened Wind",
          text = "Icy Touch makes the next blade deal 35% more instead of 20%." },
        { name = "Frozen Ground",
          text = "The frozen field deals 50% more damage and slows its enemies by 25%." },
        { name = "Winter's Return",
          text = "Frost Strike's follow-up deals 25% of your attack power instead of 15%." },
        { name = "Horn of the North",
          text = "Horn of Winter makes your next blade also deal 25% of your attack power as Frost damage." },
        { name = "Decaying Ice",
          text = "A blade on an enemy in your Death and Decay leaves 30% of its damage again as Shadowfrost damage " ..
              "over 6 sec." },
        { name = "Northern Endurance",
          text = "A blade makes you 10% faster for 5 sec, and your next Frost Strike deals 10% more." },
        { name = "Icy Wound",
          text = "Blades leave 30% of their damage again as Frost damage over 6 sec." },
        { name = "Cold Recovery",
          text = "A blade brings Howling Blast back 1 sec sooner, once every 8 sec." },
        { name = "Winter's Mending",
          text = "After a blade, your next Death Strike within 6 sec heals 30% more." },
        { name = "Glacial Flow",
          text = "A blade on your Frost Fever target makes your next Frost Strike within 6 sec deal 20% more." },
      },
      builds = {
        { name = "The Glacier", mods = { "Glacial Edge", "Sharpened Wind", "Icy Wound" },
          text = "Fewer blades, and they hurt." },
        { name = "The Blizzard", mods = { "Winter's Reach", "Frozen Ground", "Cold Recovery" },
          text = "Through the pack and into the ground." },
        { name = "The Frost Striker", mods = { "Rimeblade", "Winter's Return", "Northern Endurance" },
          text = "Three blades, then Frost Strike." },
      },
    },
    { id = 51,
      name = "Rune of Trueshot",
      role = "Ranged DPS (Hunter, Marksmanship)",
      roles = { "DAMAGER" },
      icon = "Ability_Rune_Trueshot",
      keywords = "hunter marksmanship aimed shot trueshot pierce hunters mark arcane shot chimera shot multi shot " ..
          "silencing shot serpent sting concussive shot steady shot mend pet",
      slots = { "Shot", "Mark", "Recovery" },
      builtFrom = "Trueshot (in Aimed Shot's place)",
      spentWith = "Trueshot",
      resource = "None",
      core = "Aimed Shot becomes Trueshot: a 2 sec windup standing still (moving cancels it, at no cost), then your " ..
          "Aimed Shot for 45% more, piercing up to 4 enemies behind its target for 40% of its hit. Its cost and " ..
          "cooldown are Aimed Shot's. On your Hunter's Mark target it leaves Exposed Target for 6 sec: your next " ..
          "Arcane Shot or Chimera Shot there deals 20% more. Multi-Shot makes your next Trueshot within 8 sec " ..
          "pierce wider and 20% harder; Silencing Shot makes it also deal 30% of your ranged attack power as Arcane " ..
          "damage to its target.",
      loop = "Hunter's Mark, then Trueshot from a standstill; Arcane Shot or Chimera Shot to spend Exposed Target.",
      mods = {
        { name = "Perfect Shot",
          text = "Trueshot deals 30% more damage to its target." },
        { name = "Piercing Shot",
          text = "Trueshot's pierce deals 65% of its hit instead of 40%." },
        { name = "Steady Aim",
          text = "Trueshot winds up for 2.5 sec instead, and deals 30% more to its target and 20% more to the " ..
              "pierced." },
        { name = "Venom Arrow",
          text = "Trueshot on your Serpent Sting target also deals 25% of your ranged attack power as Nature " ..
              "damage." },
        { name = "Concussive Pierce",
          text = "Trueshot on your Concussive Shot target also deals 25% of your ranged attack power." },
        { name = "Marked Weakness",
          text = "Exposed Target makes your next Arcane Shot or Chimera Shot deal 35% more instead of 20%." },
        { name = "Wide Trajectory",
          text = "After Multi-Shot, Trueshot pierces up to 2 more enemies." },
        { name = "Arcane Arrow",
          text = "Silencing Shot's payload deals 50% of your ranged attack power instead of 30%." },
        { name = "Arcane Calibration",
          text = "Silencing Shot's payload also strikes up to 2 pierced enemies for 40% of it." },
        { name = "Marked Ricochet",
          text = "Trueshot on your marked target also strikes one enemy within 8 yards for 25% of its hit." },
        { name = "Quick Recovery",
          text = "Trueshot brings Arcane Shot back 2 sec sooner." },
        { name = "Hunter's Mobility",
          text = "After Trueshot you move 25% faster for 4 sec." },
        { name = "Follow-Up Shot",
          text = "After Trueshot, your next Arcane Shot within 6 sec deals 20% more." },
        { name = "Steady Recovery",
          text = "After Trueshot, your next Steady Shot within 6 sec casts 20% faster." },
        { name = "Mending Aim",
          text = "Trueshot while your Mend Pet is on your pet heals it for 5% of its health, once every 10 sec." },
      },
      builds = {
        { name = "The Sniper", mods = { "Perfect Shot", "Marked Weakness", "Follow-Up Shot" },
          text = "Mark, wind up, and finish it." },
        { name = "The Line Breaker", mods = { "Piercing Shot", "Wide Trajectory", "Hunter's Mobility" },
          text = "Everything in the line goes down." },
        { name = "The Arcane Marksman", mods = { "Steady Aim", "Arcane Arrow", "Quick Recovery" },
          text = "Silencing Shot loads the shot." },
      },
    },
    { id = 52,
      name = "Rune of Scarlet Benediction",
      role = "Healer (Priest, Holy)",
      roles = { "HEALER" },
      icon = "Ability_Rune_ScarletBenediction",
      keywords = "priest holy healer renew flash heal greater heal prayer of healing binding heal circle of healing " ..
          "power word shield prayer of mending guardian spirit bloom scarlet",
      slots = { "Bloom", "Tending", "Aftermath" },
      builtFrom = "Renew (a dormant Scarlet Bloom)",
      spentWith = "Flash Heal, Greater Heal, Binding Heal or Prayer of Healing",
      resource = "One Bloom on each ally you Renew",
      core = "Renew plants a dormant Scarlet Bloom on its target for as long as the Renew. Your Flash Heal or " ..
          "Greater Heal on that ally opens it: the most injured other ally within 10 yards is healed for 80% of " ..
          "your spell power; if there is none, the Bloom's ally gets a shield for 40% of your spell power for 6 " ..
          "sec. A Bloom opens once, and that ally takes no new Bloom for 8 sec. A Renew that has ticked 3 times " ..
          "makes the opening 30% stronger. Prayer of Healing opens up to 3 Blooms among those it heals; Binding " ..
          "Heal opens its target's.",
      loop = "Renew the group, then Flash Heal or Greater Heal a Bloom to heal whoever is lowest beside it.",
      mods = {
        { name = "Deep Bloom",
          text = "The opening heals for 110% of your spell power instead of 80%." },
        { name = "Scarlet Garden",
          text = "The opening heals the 2 most injured allies nearby for 60% each." },
        { name = "Bloodrose",
          text = "Allies the opening heals also heal for 30% of your spell power over 6 sec." },
        { name = "Circling Bloom",
          text = "Circle of Healing opens the Blooms of up to 2 allies it heals." },
        { name = "Shielded Bloom",
          text = "Power Word: Shield on a Bloom's ally opens it." },
        { name = "Patient Cultivation",
          text = "A Renew that has ticked 3 times makes the opening 60% stronger instead of 30%." },
        { name = "Swift Blossom",
          text = "An ally can take a new Bloom 4 sec after one opens instead of 8." },
        { name = "Guided Petals",
          text = "The opening reaches the most injured ally within 20 yards instead of 10." },
        { name = "Choral Bloom",
          text = "Prayer of Healing opens up to 5 Blooms, each 15% stronger." },
        { name = "Mending Petals",
          text = "A Bloom opened on an ally carrying your Prayer of Mending heals 30% more." },
        { name = "Second Bloom",
          text = "Each Bloom opens twice before it is spent." },
        { name = "Sheltering Petals",
          text = "When the Bloom opens onto another ally, its own ally is also shielded for 40% of your spell " ..
              "power." },
        { name = "Renewing Flower",
          text = "Opening a Bloom starts its ally's Renew over again." },
        { name = "Guardian Bloom",
          text = "A Bloom opened on your Guardian Spirit target heals 50% more." },
        { name = "Binding Petals",
          text = "Binding Heal also opens your own Bloom." },
      },
      builds = {
        { name = "The Gardener", mods = { "Scarlet Garden", "Patient Cultivation", "Second Bloom" },
          text = "Renew early, open late, twice." },
        { name = "The Choir", mods = { "Bloodrose", "Choral Bloom", "Sheltering Petals" },
          text = "Prayer of Healing blooms the whole party." },
        { name = "The Guardian", mods = { "Deep Bloom", "Guided Petals", "Guardian Bloom" },
          text = "One big Bloom where it is needed most." },
      },
    },
    { id = 53,
      name = "Rune of the Evergrove",
      role = "Healer (Druid, Restoration)",
      roles = { "HEALER" },
      icon = "Ability_Rune_Evergrove",
      keywords = "druid restoration healer rejuvenation stack stacking smart heal over time hot regrowth swiftmend " ..
          "wild growth lifebloom nourish healing touch tranquility natures swiftness innervate thorns grove",
      slots = { "Grove", "Seeking", "Bloom" },
      builtFrom = "Rejuvenation (stacking)",
      spentWith = "Swiftmend",
      resource = "Up to 3 Rejuvenations on each ally",
      core = "Rejuvenation stacks. Cast on your own Rejuvenation, it grows a Second, then a Third Rejuvenation on " ..
          "that ally, each healing as much as your Rejuvenation every 3 sec for 15 sec; cast on a full stack, it " ..
          "renews them all. It seeks: Rejuvenation on an ally at full health, or one already holding all three, " ..
          "grows on the most injured ally within 30 yards without a full stack instead. Swiftmend heals 20% more " ..
          "for each extra Rejuvenation on its target and leaves them there. Wild Growth grows a stack on up to 2 of " ..
          "the allies it heals that carry your Rejuvenation.",
      loop = "Triple Rejuvenation on whoever takes the hits, spread the rest by seeking, Swiftmend the full stacks.",
      mods = {
        { name = "Deep Roots",
          text = "Your extra Rejuvenations heal 20% more." },
        { name = "Fourfold Grove",
          text = "Rejuvenation stacks up to 4 times." },
        { name = "Regrowth's Root",
          text = "Regrowth on an ally with your Rejuvenation grows a stack." },
        { name = "Nourishing Grove",
          text = "Nourish heals 8% more for each of your Rejuvenations on its target." },
        { name = "Thorned Grove",
          text = "Your extra Rejuvenations on an ally with your Thorns heal 25% more." },
        { name = "Wild Seeding",
          text = "Wild Growth grows a stack on up to 4 allies instead of 2." },
        { name = "Gentle Seeking",
          text = "Rejuvenation seeks from allies above 90% health, not only full ones." },
        { name = "Lifebound",
          text = "Lifebloom's bloom grows a stack on its ally, or plants your Rejuvenation there." },
        { name = "Tranquil Grove",
          text = "Tranquility grows a stack on each ally it heals that carries your Rejuvenation, once every 8 sec." },
        { name = "Swift Grove",
          text = "Rejuvenation while Nature's Swiftness is on you grows the full stack at once, spending it." },
        { name = "Verdant Swiftmend",
          text = "Swiftmend heals 40% more for each extra Rejuvenation instead of 20%." },
        { name = "Overgrowth",
          text = "Swiftmend on a full stack also heals up to 3 injured allies within 10 yards for 30% of its heal." },
        { name = "Grove Shelter",
          text = "An ally reaching the full stack is shielded for 40% of your spell power for 6 sec." },
        { name = "Rooted Touch",
          text = "Healing Touch on an ally with your Rejuvenation renews its extra Rejuvenations." },
        { name = "Innervating Grove",
          text = "Innervate on an ally with your Rejuvenation grows it to the full stack." },
      },
      builds = {
        { name = "The Deep Grove", mods = { "Fourfold Grove", "Gentle Seeking", "Verdant Swiftmend" },
          text = "Four deep, everywhere, then Swiftmend." },
        { name = "The Wild Garden", mods = { "Deep Roots", "Wild Seeding", "Overgrowth" },
          text = "Wild Growth spreads the stacks; Swiftmend overflows." },
        { name = "The Patient Root", mods = { "Regrowth's Root", "Swift Grove", "Grove Shelter" },
          text = "Full stacks at once, and a shield with them." },
      },
    },
    { id = 54,
      name = "Rune of Iron Reckoning",
      role = "Tank (Warrior, Protection)",
      roles = { "TANK" },
      icon = "Ability_Rune_IronReckoning",
      keywords = "warrior protection tank shield block shield slam bulwark absorb revenge thunder clap devastate " ..
          "rend shockwave concussion blow last stand shield wall iron reckoning",
      slots = { "Impact", "Empowerment", "Hold" },
      builtFrom = "Shield Block (the Iron Bulwark)",
      spentWith = "Shield Slam (the Iron impact)",
      resource = "What the bulwark absorbed, for 14 sec",
      core = "Shield Block raises an Iron Bulwark before you for 14 sec, absorbing up to 15% of your maximum health " ..
          "from attacks in front of you. Shield Slam in that time releases an Iron impact once: 20% of Shield " ..
          "Slam's hit plus half of what the bulwark absorbed (that part at most half your attack power), to up to 5 " ..
          "enemies in a cone before you; the bulwark stays. In the bulwark, Revenge makes the impact 20% stronger, " ..
          "Thunder Clap makes it a full circle, and Devastate makes it 15% stronger.",
      loop = "Shield Block into the hits, Revenge and Thunder Clap while it holds, then Shield Slam to release it.",
      mods = {
        { name = "Crushing Reprisal",
          text = "The Iron impact deals 30% more damage." },
        { name = "Wide Reckoning",
          text = "The Iron impact hits up to 8 enemies." },
        { name = "Immovable",
          text = "The impact is released when the bulwark ends instead of on Shield Slam, for 125%; Shield Slam in " ..
              "the bulwark still adds its share." },
        { name = "Shockwave Reckoning",
          text = "Shockwave in the bulwark releases the impact once more, for 60%." },
        { name = "Concussive Reckoning",
          text = "Concussion Blow in the bulwark makes the impact also deal 25% of your attack power to its first " ..
              "target." },
        { name = "Retaliation",
          text = "Revenge makes the impact 40% stronger instead of 20%." },
        { name = "Shaking Ground",
          text = "After Thunder Clap, the impact's circle reaches 12 yards." },
        { name = "Sundering Force",
          text = "Devastate makes the impact 30% stronger instead of 15%." },
        { name = "Vengeful Devastation",
          text = "Devastate in the bulwark also makes your next Revenge deal 20% more." },
        { name = "Rending Impact",
          text = "The impact on your Rend target also makes it bleed for 30% of the impact over 6 sec." },
        { name = "Fortified Bulwark",
          text = "The bulwark absorbs 20% more." },
        { name = "Shielded Advance",
          text = "Shield Slam makes you 20% faster and absorbs 3% of your maximum health, for 4 sec." },
        { name = "Commanding Force",
          text = "The impact causes 60% more threat." },
        { name = "Last Bastion",
          text = "Shield Block under Last Stand makes the bulwark absorb 5% more of your maximum health." },
        { name = "Wall of Iron",
          text = "The impact heals you for 2% of your maximum health, 5% under Shield Wall, once every 10 sec." },
      },
      builds = {
        { name = "The Anvil", mods = { "Immovable", "Retaliation", "Fortified Bulwark" },
          text = "Take everything, give it all back at once." },
        { name = "The Earthshaker", mods = { "Wide Reckoning", "Shaking Ground", "Commanding Force" },
          text = "Thunder Clap and hold the whole pack." },
        { name = "The Iron Wall", mods = { "Crushing Reprisal", "Sundering Force", "Wall of Iron" },
          text = "Devastate, slam and stand." },
      },
    },
}
