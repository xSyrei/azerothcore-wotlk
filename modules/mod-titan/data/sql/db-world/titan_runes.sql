-- Legendary Runes (src/runes.cpp; the spells 90700-91543: client-patch/patch_runes.py, in
-- titan_demon_hunter_spells.sql). The Rune of Fel Commandment's Felguard and Fel Summons' imp, its scripts and
-- its proc.
SET @FELGUARD := 910120;
SET @IMP := 910121;
SET @HOLLOW_ECHO := 910122;
SET @SPECTRAL_BEAST := 910123;
SET @PHANTOM_ASSASSIN := 910124;
DELETE FROM `creature_template` WHERE `entry` IN (@FELGUARD, @IMP, @HOLLOW_ECHO, @SPECTRAL_BEAST,
    @PHANTOM_ASSASSIN);
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
    `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
    `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`,
    `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
    `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`,
    `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
    `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(@FELGUARD, 0, 0, 0, 0, 0, 'Felguard', 'Fel Commandment', NULL, 0, 1, 60, 0, 35, 0, 1, 1.14286, 1, 1, 20, 0, 0, 1,
    2000, 2000, 1, 1, 1, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0,
    'npc_titan_rune_felguard', 0),
(@IMP, 0, 0, 0, 0, 0, 'Fel Imp', 'Fel Commandment', NULL, 0, 1, 60, 0, 35, 0, 1, 1.14286, 1, 1, 20, 0, 0, 1,
    2000, 2000, 1, 1, 8, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0,
    'npc_titan_rune_fel_imp', 0),
(@HOLLOW_ECHO, 0, 0, 0, 0, 0, 'Hollow Echo', 'Hollow Congregation', NULL, 0, 1, 80, 0, 35, 0, 1, 1.14286, 1, 1, 20,
    0, 0, 1, 2000, 2000, 1, 1, 8, 33554434, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 0, 0, 0, 1, 0,
    0, 'npc_titan_rune_hollow_echo', 0),
(@SPECTRAL_BEAST, 0, 0, 0, 0, 0, 'Spectral Beast', 'Wild Hunt', NULL, 0, 1, 80, 0, 35, 0, 1.2, 1.6, 1, 1, 20,
    0, 0, 1, 2000, 2000, 1, 1, 1, 33554434, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 0, 0, 0, 1, 0,
    0, 'npc_titan_rune_spectral_beast', 0),
(@PHANTOM_ASSASSIN, 0, 0, 0, 0, 0, 'Phantom Assassin', 'Spectral Blade', NULL, 0, 1, 80, 0, 35, 0, 1, 1.14286, 1, 1,
    20, 0, 0, 1, 2000, 2000, 1, 1, 4, 33554434, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 0, 0, 0, 1,
    0, 0, 'npc_titan_rune_phantom_assassin', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (@FELGUARD, @IMP, @HOLLOW_ECHO, @SPECTRAL_BEAST,
    @PHANTOM_ASSASSIN);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(@FELGUARD, 0, 14255, 1, 1, 0),
(@IMP, 0, 4449, 1, 1, 0),
(@HOLLOW_ECHO, 0, 11686, 1, 1, 0),
(@SPECTRAL_BEAST, 0, 903, 1, 1, 0),
(@PHANTOM_ASSASSIN, 0, 11686, 1, 1, 0);

-- The rune's aura grants Fel Command from its procs; Throw Glaive fires the volley.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_fel_commandment',
    'spell_titan_rune_throw_glaive');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90700, 'spell_titan_rune_fel_commandment'),
(90111, 'spell_titan_rune_throw_glaive');

-- The rune's procs: melee abilities and harmful spells that hit (normal, crit or absorbed); the script picks the
-- ones that count.
DELETE FROM `spell_proc` WHERE `SpellId` = 90700;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`,
    `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`,
    `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(90700, 0, 0, 0, 0, 0, 65552, 1, 2, 1027, 0, 0, 0, 100, 0, 0);

-- The Rune of the Sanctified Paragon: Consecration (every rank) toggles its aura; the aura's tick deals the ground's
-- damage and runs the modifiers.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_consecration',
    'spell_titan_rune_sanctified_paragon');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(-26573, 'spell_titan_rune_consecration'),
(90731, 'spell_titan_rune_sanctified_paragon');

-- Its damage, heal and shield carry the Hero's power already (runes.cpp ParagonTick: Consecration's 4% of spell
-- power and attack power): none of their own.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (90732, 90734, 90737);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90732, 0, 0, 0, 0, 'Sanctified Paragon - Consecration tick'),
(90734, 0, 0, 0, 0, 'Sanctified Paragon - Merciful Ground'),
(90737, 0, 0, 0, 0, 'Sanctified Paragon - Shelter of the Paragon');

-- The Rune of the Stormbound Conduit: its aura reads Lightning Bolt and Chain Lightning hits; Lightning Shield's
-- script (every rank) does nothing now; its retaliations (every rank, and the rune's own) arc.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_stormbound_conduit',
    'spell_titan_rune_lightning_shield', 'spell_titan_rune_lightning_shield_damage');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90750, 'spell_titan_rune_stormbound_conduit'),
(-324, 'spell_titan_rune_lightning_shield'),
(-26364, 'spell_titan_rune_lightning_shield_damage'),
(90754, 'spell_titan_rune_lightning_shield_damage');

-- Its procs: harmful spells that hit (normal, crit or absorbed); the script picks Lightning Bolt and Chain Lightning.
DELETE FROM `spell_proc` WHERE `SpellId` = 90750;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`,
    `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`,
    `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(90750, 0, 0, 0, 0, 0, 65536, 1, 2, 1027, 0, 0, 0, 100, 0, 0);

-- An arc's damage is the script's (a share of what it comes from, power already in it); the rune's retaliation
-- carries Lightning Shield's spell power share.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (90751, 90752, 90754);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90751, 0, 0, 0, 0, 'Stormbound Conduit - Static Ward'),
(90752, 0, 0, 0, 0, 'Stormbound Conduit - secondary arc'),
(90754, 0.147, 0, 0, 0, 'Stormbound Conduit - Lightning Shield retaliation');

-- The Rune of Echoing Thunder: Thunder Clap (every rank) leaves its Thunderfield; Shield Slam (every rank) landed
-- in it releases a Thunder Echo.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_thunder_clap',
    'spell_titan_rune_shield_slam');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(-6343, 'spell_titan_rune_thunder_clap'),
(-23922, 'spell_titan_rune_shield_slam');

-- The echo's damage is the script's (a share of Shield Slam's, power already in it); the absorb's too.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (90774, 90776);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90774, 0, 0, 0, 0, 'Echoing Thunder - Thunder Echo'),
(90776, 0, 0, 0, 0, 'Echoing Thunder - Sheltering Reverberation');

-- The Rune of the Wellspring: Chain Heal (every rank) jumps once more and favors the Earth Shield wearer; Earth
-- Shield's heal flows on (Flowing Waters).
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_chain_heal',
    'spell_titan_rune_earth_shield_heal');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(-1064, 'spell_titan_rune_chain_heal'),
(379, 'spell_titan_rune_earth_shield_heal');

-- The Rune of the Wandering Mist: Soothing Mist's cast and ticks; Enveloping Mist and Vivify woven into the channel.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_soothing_mist_cast',
    'spell_titan_rune_soothing_mist', 'spell_titan_rune_mist_technique');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90300, 'spell_titan_rune_soothing_mist_cast'),
(90300, 'spell_titan_rune_soothing_mist'),
(90301, 'spell_titan_rune_mist_technique'),
(90303, 'spell_titan_rune_mist_technique');

-- Their heals, heals over time and absorbs are the scripts' (shares of healing done, power already in it).
DELETE FROM `spell_bonus_data` WHERE `entry` IN (90792, 90793, 90794, 90811, 90812, 90813, 90814);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90792, 0, 0, 0, 0, 'Wellspring - extra jump'),
(90793, 0, 0, 0, 0, 'Wellspring - Tidal Shelter'),
(90794, 0, 0, 0, 0, 'Wellspring - Renewing Current'),
(90811, 0, 0, 0, 0, 'Wandering Mist'),
(90812, 0, 0, 0, 0, 'Wandering Mist - lesser Enveloping Mist'),
(90813, 0, 0, 0, 0, 'Wandering Mist - Lingering Mist'),
(90814, 0, 0, 0, 0, 'Wandering Mist - Jade Shelter');

-- The five runes after them. The Threefold Grave: Icy Touch, Plague Strike and Blood Strike (every rank) lay
-- their Graves.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_requiem', 'spell_titan_rune_scar_strike',
    'spell_titan_rune_grave_strike',
    'spell_titan_rune_wake_cut', 'spell_titan_rune_unbroken_onslaught', 'spell_titan_rune_dawn_vow',
    'spell_titan_rune_judgement', 'spell_titan_rune_dawn_echo', 'spell_titan_rune_mind_flay',
    'spell_titan_rune_mind_blast', 'spell_titan_rune_shattered_sun');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(-45477, 'spell_titan_rune_grave_strike'),
(-45462, 'spell_titan_rune_grave_strike'),
(-45902, 'spell_titan_rune_grave_strike'),
-- Unbroken Onslaught: Mortal Strike and Bloodthirst (every rank) cut the wake; the rune's aura reads every other
-- melee ability.
(-12294, 'spell_titan_rune_wake_cut'),
(-23881, 'spell_titan_rune_wake_cut'),
(90860, 'spell_titan_rune_unbroken_onslaught'),
-- The Dawnbound Crusader: Crusader Strike's, Holy Shock's and (Judgment's Echo) the Judgements' echoes; Dawn
-- Vow's script does nothing (the first draft's action, unlearned).
(90891, 'spell_titan_rune_dawn_vow'),
(20271, 'spell_titan_rune_judgement'),
(53407, 'spell_titan_rune_judgement'),
(53408, 'spell_titan_rune_judgement'),
(35395, 'spell_titan_rune_dawn_echo'),
(-25912, 'spell_titan_rune_dawn_echo'),
(-25914, 'spell_titan_rune_dawn_echo'),
-- The Hollow Congregation: Mind Flay calls the echoes, Mind Blast commands them (every rank).
(-15407, 'spell_titan_rune_mind_flay'),
(-8092, 'spell_titan_rune_mind_blast'),
-- The Shattered Sun: its fire spells (Fireball, Pyroblast, Fire Blast, Scorch) and frost spells (Frostbolt, Ice
-- Lance, Frost Nova, Cone of Cold), every rank.
(-133, 'spell_titan_rune_shattered_sun'),
(-11366, 'spell_titan_rune_shattered_sun'),
(-2136, 'spell_titan_rune_shattered_sun'),
(-2948, 'spell_titan_rune_shattered_sun'),
(-116, 'spell_titan_rune_shattered_sun'),
(-30455, 'spell_titan_rune_shattered_sun'),
(-122, 'spell_titan_rune_shattered_sun'),
(-120, 'spell_titan_rune_shattered_sun');

-- Unbroken Onslaught's procs: melee abilities that hit (normal, crit or absorbed); the script picks the ones that
-- reopen the wake.
DELETE FROM `spell_proc` WHERE `SpellId` = 90860;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`,
    `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`,
    `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(90860, 0, 0, 0, 0, 0, 16, 1, 2, 1027, 0, 0, 0, 100, 0, 0);

-- Their damage, heals and absorbs are the scripts' (shares of the Hero's own hits and power, already in them).
DELETE FROM `spell_bonus_data` WHERE `entry` IN (90838, 90840, 90841, 90842, 90843, 90844, 90861, 90862, 90864, 90865,
    90894, 90895, 90900, 90922, 90923, 90924, 90925, 90926, 90953, 90955);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90838, 0, 0, 0, 0, 'Threefold Grave - Frozen Grave'),
(90840, 0, 0, 0, 0, 'Threefold Grave - Plagued Grave'),
(90841, 0, 0, 0, 0, 'Threefold Grave - Grave Pestilence'),
(90842, 0, 0, 0, 0, 'Threefold Grave - Crimson Grave'),
(90843, 0, 0, 0, 0, 'Threefold Grave - Crimson Ward'),
(90844, 0, 0, 0, 0, 'Threefold Grave - Grave Ward'),
(90861, 0, 0, 0, 0, 'Unbroken Onslaught - Weapon Wake'),
(90862, 0, 0, 0, 0, 'Unbroken Onslaught - Bloodletting'),
(90864, 0, 0, 0, 0, 'Unbroken Onslaught - Aftershock'),
(90865, 0, 0, 0, 0, 'Unbroken Onslaught - Second Wind'),
(90894, 0, 0, 0, 0, 'Dawnbound Crusader - Dawn Echo heal'),
(90900, 0, 0, 0, 0, 'Dawnbound Crusader - Radiant Ward'),
(90895, 0, 0, 0, 0, 'Dawnbound Crusader - Dawn Echo damage'),
(90922, 0, 0, 0, 0, 'Hollow Congregation - Hollow Flay'),
(90923, 0, 0, 0, 0, 'Hollow Congregation - barrage'),
(90924, 0, 0, 0, 0, 'Hollow Congregation - Accusation'),
(90925, 0, 0, 0, 0, 'Hollow Congregation - Last Word'),
(90926, 0, 0, 0, 0, 'Hollow Congregation - Shadow Mend'),
(90953, 0, 0, 0, 0, 'Shattered Sun - Shatter'),
(90955, 0, 0, 0, 0, 'Shattered Sun - Second Sunrise');

-- The runes' damage over time (Grave Pestilence, Bloodletting, Hollow Flay): periodic dummies whose ticks the script
-- deals as their own damage spells (90832-90834), the amount kept by the script.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_titan_rune_dot';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90841, 'spell_titan_rune_dot'),
(90862, 'spell_titan_rune_dot'),
(90922, 'spell_titan_rune_dot'),
(90983, 'spell_titan_rune_dot'),
(91133, 'spell_titan_rune_dot');

DELETE FROM `spell_bonus_data` WHERE `entry` IN (90832, 90833, 90834, 90984, 91134);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90832, 0, 0, 0, 0, 'Threefold Grave - Grave Pestilence tick'),
(90833, 0, 0, 0, 0, 'Unbroken Onslaught - Bloodletting tick'),
(90834, 0, 0, 0, 0, 'Hollow Congregation - Hollow Flay tick'),
(90984, 0, 0, 0, 0, 'Spectral Blade - Open Wound tick'),
(91134, 0, 0, 0, 0, 'Wild Hunt - Ghostfang tick');

-- The Spectral Blade: Eviscerate calls the phantom; Sinister Strike, Backstab and Hemorrhage spend Marked for Murder
-- (every rank). The Wild Hunt: Kill Command and Bestial Wrath unleash Spectral Beasts (910123, the Hero's pet's look).
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_eviscerate',
    'spell_titan_rune_marked_strike', 'spell_titan_rune_kill_command', 'spell_titan_rune_bestial_wrath');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(-2098, 'spell_titan_rune_eviscerate'),
(-1752, 'spell_titan_rune_marked_strike'),
(-53, 'spell_titan_rune_marked_strike'),
(-16511, 'spell_titan_rune_marked_strike'),
(34026, 'spell_titan_rune_kill_command'),
(19574, 'spell_titan_rune_bestial_wrath');

-- Their strikes are shares of the Hero's (or their pet's) own hits: no bonuses of their own.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (90981, 91132, 91136);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90981, 0, 0, 0, 0, 'Spectral Blade - Phantom Strike'),
(91132, 0, 0, 0, 0, 'Wild Hunt - Spectral Bite'),
(91136, 0, 0, 0, 0, 'Wild Hunt - Final Pounce');

-- Five more (their own dossiers): Arcane Mind, the Splintered Crown, Phoenix Wake, Astral Horizon, Guiding Light.
-- Phoenix Wake's phoenix and Astral Horizon's Arcane Orb: invisible creatures wearing retail's Phoenix Flames bird
-- and Arcane Orb (their auras' looks, client-patch/patch_runes.py); they fly where the rune sends them, out of reach.
SET @RUNE_PHOENIX := 910125;
SET @RUNE_ARCANE_ORB := 910126;
SET @RUNE_BALL_LIGHTNING := 910128;     -- the Stormbound Conduit's: wearing the Spark of Ionar's crackle (52667)
SET @RUNE_TEMPORAL_ANOMALY := 910157;   -- Scarlet Benediction's bubble: wearing its look (93420)
DELETE FROM `creature_template` WHERE `entry` IN (@RUNE_PHOENIX, @RUNE_ARCANE_ORB, @RUNE_BALL_LIGHTNING,
    @RUNE_TEMPORAL_ANOMALY);
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
    `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
    `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`,
    `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
    `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`,
    `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
    `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(@RUNE_PHOENIX, 0, 0, 0, 0, 0, 'Phoenix', 'Phoenix Wake', NULL, 0, 1, 80, 0, 35, 0, 1, 1.14286, 1, 3, 20, 0, 2, 1,
    2000, 2000, 1, 1, 1, 33554434, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0,
    'npc_titan_rune_flier', 0),
(@RUNE_ARCANE_ORB, 0, 0, 0, 0, 0, 'Arcane Orb', 'Astral Horizon', NULL, 0, 1, 80, 0, 35, 0, 1, 1.14286, 1, 1, 20,
    0, 6, 1, 2000, 2000, 1, 1, 8, 33554434, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 0, 0, 0, 1, 0,
    0, 'npc_titan_rune_flier', 0),
(@RUNE_BALL_LIGHTNING, 0, 0, 0, 0, 0, 'Ball Lightning', 'Stormbound Conduit', NULL, 0, 1, 80, 0, 35, 0, 1, 1.14286,
    1, 1, 20, 0, 3, 1, 2000, 2000, 1, 1, 8, 33554434, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 0, 0,
    0, 1, 0, 0, 'npc_titan_rune_flier', 0),
(@RUNE_TEMPORAL_ANOMALY, 0, 0, 0, 0, 0, 'Temporal Anomaly', 'Scarlet Benediction', NULL, 0, 1, 80, 0, 35, 0, 1,
    1.14286, 1, 1, 20, 0, 1, 1, 2000, 2000, 1, 1, 8, 33554434, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1,
    1, 0, 0, 0, 1, 0, 0, 'npc_titan_rune_flier', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (@RUNE_PHOENIX, @RUNE_ARCANE_ORB, @RUNE_BALL_LIGHTNING,
    @RUNE_TEMPORAL_ANOMALY);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(@RUNE_PHOENIX, 0, 11686, 1, 1, 0),
(@RUNE_ARCANE_ORB, 0, 11686, 1, 1, 0),
(@RUNE_BALL_LIGHTNING, 0, 11686, 1, 1, 0),
(@RUNE_TEMPORAL_ANOMALY, 0, 11686, 1, 1, 0);

-- Arcane Blast and Arcane Explosion grant Insight, Arcane Missiles spends it (its channel's end: Mental Reserves);
-- Frostbolt forms the Icicles; Pyroblast leaves the Ember, Fire Blast wakes the phoenix; Arcane Barrage sets the
-- Orb loose; Prayer of Mending places the Guiding Star, Flash Heal and Greater Heal move it (every rank).
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_arcane_blast',
    'spell_titan_rune_arcane_explosion', 'spell_titan_rune_arcane_missiles', 'spell_titan_rune_arcane_missiles_channel',
    'spell_titan_rune_frostbolt_icicle', 'spell_titan_rune_pyroblast_ember', 'spell_titan_rune_fire_blast_phoenix',
    'spell_titan_rune_arcane_barrage', 'spell_titan_rune_prayer_of_mending', 'spell_titan_rune_guiding_heal');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(-30451, 'spell_titan_rune_arcane_blast'),
(-1449, 'spell_titan_rune_arcane_explosion'),
(-5143, 'spell_titan_rune_arcane_missiles'),
(-5143, 'spell_titan_rune_arcane_missiles_channel'),
(-116, 'spell_titan_rune_frostbolt_icicle'),
(-11366, 'spell_titan_rune_pyroblast_ember'),
(-2136, 'spell_titan_rune_fire_blast_phoenix'),
(-44425, 'spell_titan_rune_arcane_barrage'),
(-33076, 'spell_titan_rune_prayer_of_mending'),
(-2061, 'spell_titan_rune_guiding_heal'),
(-2060, 'spell_titan_rune_guiding_heal');

-- Their damage over time: Arcane Echo, Frost Wound, Clinging Embers.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (91203, 91224, 91246) AND `ScriptName` = 'spell_titan_rune_dot';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(91203, 'spell_titan_rune_dot'),
(91224, 'spell_titan_rune_dot'),
(91246, 'spell_titan_rune_dot');

-- Their strikes, heals and absorbs are shares of the Hero's own hits and heals: no bonuses of their own.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (91202, 91204, 91207, 91221, 91225, 91228, 91242, 91245, 91247, 91248,
    91262, 91263, 91264, 91265, 91267, 91282, 91283, 91285, 91286, 91287, 91288);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(91202, 0, 0, 0, 0, 'Arcane Mind - Insight Missile'),
(91204, 0, 0, 0, 0, 'Arcane Mind - Arcane Echo tick'),
(91207, 0, 0, 0, 0, 'Arcane Mind - Unbound Mind missile'),
(91221, 0, 0, 0, 0, 'Splintered Crown - Icicle'),
(91225, 0, 0, 0, 0, 'Splintered Crown - Frost Wound tick'),
(91228, 0, 0, 0, 0, 'Splintered Crown - Ice Splinter'),
(91242, 0, 0, 0, 0, 'Phoenix Wake - Phoenix Flight'),
(91245, 0, 0, 0, 0, 'Phoenix Wake - Scorched Earth tick'),
(91247, 0, 0, 0, 0, 'Phoenix Wake - Clinging Embers tick'),
(91248, 0, 0, 0, 0, 'Phoenix Wake - Rebirth'),
(91262, 0, 0, 0, 0, 'Astral Horizon - Orb Bolt'),
(91263, 0, 0, 0, 0, 'Astral Horizon - Wandering Star'),
(91264, 0, 0, 0, 0, 'Astral Horizon - Prismatic Lance'),
(91265, 0, 0, 0, 0, 'Astral Horizon - Supernova'),
(91267, 0, 0, 0, 0, 'Astral Horizon - Astral Remnant tick'),
(91282, 0, 0, 0, 0, 'Guiding Light - Guiding Star'),
(91283, 0, 0, 0, 0, 'Guiding Light - arrival'),
(91285, 0, 0, 0, 0, 'Guiding Light - Sheltering Light'),
(91286, 0, 0, 0, 0, 'Guiding Light - Gentle Passage'),
(91287, 0, 0, 0, 0, 'Guiding Light - Afterglow'),
(91288, 0, 0, 0, 0, 'Guiding Light - Parting Grace');

-- Three druid runes (their own dossiers): Elune's Guardian (Starfire calls the owls), Astral Convergence (Wrath and
-- Starfire in turn; Wrath also spends Twilight Hunt's mark), Ursoc's Champion (Barkskin raises the guard, Maul
-- releases it), every rank.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_starfire', 'spell_titan_rune_wrath',
    'spell_titan_rune_barkskin', 'spell_titan_rune_maul');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(-2912, 'spell_titan_rune_starfire'),
(-5176, 'spell_titan_rune_wrath'),
(22812, 'spell_titan_rune_barkskin'),
(-6807, 'spell_titan_rune_maul');

-- The Extended Options' other abilities, every rank: Lightning Bolt, Frostbolt, Lava Burst, Scorch, Arcane Shot,
-- Chain Heal and Holy Shock (their hits and heals, to the runes that answer them); Evocation (Arcane Mind, Astral
-- Horizon). The first, Astral Convergence's, was spell_titan_rune_convergence_hit.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_convergence_hit',
    'spell_titan_rune_extended_hit', 'spell_titan_rune_evocation');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(-403, 'spell_titan_rune_extended_hit'),
(-116, 'spell_titan_rune_extended_hit'),
(-51505, 'spell_titan_rune_extended_hit'),
(-2948, 'spell_titan_rune_extended_hit'),
(-3044, 'spell_titan_rune_extended_hit'),
(-1064, 'spell_titan_rune_extended_hit'),
(-20473, 'spell_titan_rune_extended_hit'),
(-686, 'spell_titan_rune_extended_hit'),
(-19434, 'spell_titan_rune_extended_hit'),
(-56641, 'spell_titan_rune_extended_hit'),
(-30455, 'spell_titan_rune_extended_hit'),
(-120, 'spell_titan_rune_extended_hit'),
(-45477, 'spell_titan_rune_extended_hit'),
(-2136, 'spell_titan_rune_extended_hit'),
(-33878, 'spell_titan_rune_extended_hit'),
(-779, 'spell_titan_rune_extended_hit'),
(-8042, 'spell_titan_rune_extended_hit'),
(-6343, 'spell_titan_rune_extended_hit'),
(379, 'spell_titan_rune_extended_hit'),
(-100, 'spell_titan_rune_extended_hit'),
(642, 'spell_titan_rune_extended_hit'),
(-2061, 'spell_titan_rune_extended_hit'),
(-25914, 'spell_titan_rune_extended_hit'),
(20271, 'spell_titan_rune_extended_hit'),
(53408, 'spell_titan_rune_extended_hit'),
(53407, 'spell_titan_rune_extended_hit'),
(-53600, 'spell_titan_rune_extended_hit'),
(-8056, 'spell_titan_rune_extended_hit'),
(-23922, 'spell_titan_rune_extended_hit'),
(-61295, 'spell_titan_rune_extended_hit'),
(-139, 'spell_titan_rune_extended_hit'),
(-6572, 'spell_titan_rune_extended_hit'),
(-6789, 'spell_titan_rune_extended_hit'),
(-47541, 'spell_titan_rune_extended_hit'),
(-49184, 'spell_titan_rune_extended_hit'),
(-49020, 'spell_titan_rune_extended_hit'),
(-49143, 'spell_titan_rune_extended_hit'),
(-2643, 'spell_titan_rune_extended_hit'),
(-53209, 'spell_titan_rune_extended_hit'),
(-57330, 'spell_titan_rune_extended_hit'),
(45524, 'spell_titan_rune_extended_hit'),
(45470, 'spell_titan_rune_extended_hit'),
(34490, 'spell_titan_rune_extended_hit'),
(-421, 'spell_titan_rune_extended_hit'),
(-60103, 'spell_titan_rune_extended_hit'),
(-2912, 'spell_titan_rune_extended_hit'),
(-2060, 'spell_titan_rune_extended_hit'),
(-596, 'spell_titan_rune_extended_hit'),
(-32546, 'spell_titan_rune_extended_hit'),
(-34861, 'spell_titan_rune_extended_hit'),
(-17, 'spell_titan_rune_extended_hit'),
(-774, 'spell_titan_rune_extended_hit'),
(-8936, 'spell_titan_rune_extended_hit'),
(18562, 'spell_titan_rune_extended_hit'),
(-48438, 'spell_titan_rune_extended_hit'),
(50464, 'spell_titan_rune_extended_hit'),
(-5185, 'spell_titan_rune_extended_hit'),
(-44203, 'spell_titan_rune_extended_hit'),
(29166, 'spell_titan_rune_extended_hit'),
(2565, 'spell_titan_rune_extended_hit'),
(-20243, 'spell_titan_rune_extended_hit'),
(46968, 'spell_titan_rune_extended_hit'),
(12809, 'spell_titan_rune_extended_hit'),
(12051, 'spell_titan_rune_evocation');

-- Blizzard's damage (every rank's) with the Splintered Crown: Hailstorm's victims.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_titan_rune_blizzard_hit';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(42208, 'spell_titan_rune_blizzard_hit'), (42209, 'spell_titan_rune_blizzard_hit'),
(42210, 'spell_titan_rune_blizzard_hit'), (42211, 'spell_titan_rune_blizzard_hit'),
(42212, 'spell_titan_rune_blizzard_hit'), (42213, 'spell_titan_rune_blizzard_hit'),
(42198, 'spell_titan_rune_blizzard_hit'), (42937, 'spell_titan_rune_blizzard_hit'),
(42938, 'spell_titan_rune_blizzard_hit');

-- The Extended Options' damage over time: Phoenix Burn, Phantom Wound, Corrupted Blade.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (91635, 91735, 91739, 91911)
    AND `ScriptName` = 'spell_titan_rune_dot';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(91635, 'spell_titan_rune_dot'),
(91911, 'spell_titan_rune_dot'),
(91735, 'spell_titan_rune_dot'),
(91739, 'spell_titan_rune_dot'),
(93218, 'spell_titan_rune_dot'),
(93318, 'spell_titan_rune_dot'),
(93349, 'spell_titan_rune_dot'),
(93351, 'spell_titan_rune_dot'),
(93468, 'spell_titan_rune_dot');

-- Trueshot (both windups): released, the Hero's Aimed Shot.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_titan_rune_trueshot';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(93376, 'spell_titan_rune_trueshot'),
(93377, 'spell_titan_rune_trueshot');

-- The Evergrove's Lifebound (Lifebloom's bloom, its druid the original caster); Iron Reckoning's bulwark.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_lifebloom_bloom',
    'spell_titan_rune_iron_bulwark');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(33778, 'spell_titan_rune_lifebloom_bloom'),
(93466, 'spell_titan_rune_iron_bulwark');

-- The Extended Options' strikes, heals and shields: shares of the Hero's power and hits, no bonuses of their own.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (91636, 91637, 91638, 91639, 91641, 91660, 91661, 91662, 91663,
    91688, 91689, 91710, 91711, 91736, 91737, 91738, 91740, 91741, 91761, 91762, 91786, 91787, 91788, 91837, 91838,
    91840, 91842, 91860, 91861, 91862, 91863, 91864, 91885, 91886, 91889, 91912, 91914, 91916, 91917, 91919, 93015,
    93016, 93017, 93040, 93041, 93042, 93065, 93066, 93067, 93068, 93092, 93093, 93094, 93115, 93116, 93117, 93118,
    93140, 93141, 93142, 93165, 93166, 93167, 93191, 93192, 93193, 93194, 93195, 93216, 93217, 93219, 93220, 93222,
    93241, 93242, 93266, 93267, 93268, 93270, 93291, 93292, 93317, 93319, 93320, 93321, 93322, 93323, 93342, 93343,
    93345, 93347, 93348, 93350, 93352, 93354, 93379, 93380, 93381, 93382, 93385, 93388, 93390, 93392, 93394,
    93395, 93397, 93399, 93417, 93418, 93419, 93422, 93423, 93441, 93442, 93443, 93444, 93445, 93466, 93467, 93469,
    93471, 93472);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(91636, 0, 0, 0, 0, 'Phoenix Wake - Phoenix Burn tick'),
(91637, 0, 0, 0, 0, 'Phoenix Wake - Phoenix Eruption'),
(91638, 0, 0, 0, 0, 'Phoenix Wake - Flamekeeper'),
(91639, 0, 0, 0, 0, 'Phoenix Wake - Blazing Path'),
(91641, 0, 0, 0, 0, 'Phoenix Wake - Emberguard'),
(91660, 0, 0, 0, 0, 'Astral Horizon - Arcane Orb launch'),
(91661, 0, 0, 0, 0, 'Astral Horizon - Orb Pulse'),
(91662, 0, 0, 0, 0, 'Astral Horizon - Orbital Lightning'),
(91663, 0, 0, 0, 0, 'Astral Horizon - Astral Shelter'),
(91688, 0, 0, 0, 0, 'Arcane Mind - Arcane Charge'),
(91689, 0, 0, 0, 0, 'Arcane Mind - Shocking Clarity'),
(91710, 0, 0, 0, 0, 'Guiding Light - Guiding Trail'),
(91711, 0, 0, 0, 0, 'Guiding Light - Guiding Ward'),
(91736, 0, 0, 0, 0, 'Spectral Blade - Phantom Wound tick'),
(91737, 0, 0, 0, 0, 'Spectral Blade - Frost Phantom'),
(91738, 0, 0, 0, 0, 'Spectral Blade - Flame Phantom'),
(91740, 0, 0, 0, 0, 'Spectral Blade - Corrupted Blade tick'),
(91741, 0, 0, 0, 0, 'Spectral Blade - Restoring Phantom'),
(91761, 0, 0, 0, 0, 'Elune''s Guardian - Owl''s Return'),
(91762, 0, 0, 0, 0, 'Elune''s Guardian - Frostfeather'),
(91786, 0, 0, 0, 0, 'Hollow Congregation - Hollow Patch tick'),
(91787, 0, 0, 0, 0, 'Hollow Congregation - Moonlit Congregation'),
(91788, 0, 0, 0, 0, 'Hollow Congregation - Devouring Command'),
(91837, 0, 0, 0, 0, 'Duelist - Powder Blast'),
(91838, 0, 0, 0, 0, 'Duelist - Storm Pistol'),
(91840, 0, 0, 0, 0, 'Duelist - Frozen Powder'),
(91842, 0, 0, 0, 0, 'Duelist - Healing Powder'),
(91860, 0, 0, 0, 0, 'Ember''s Wrath - Searing Shred'),
(91861, 0, 0, 0, 0, 'Ember''s Wrath - Frozen Embers'),
(91862, 0, 0, 0, 0, 'Ember''s Wrath - Phoenix Claws'),
(91863, 0, 0, 0, 0, 'Ember''s Wrath - Phoenix Scar'),
(91864, 0, 0, 0, 0, 'Ember''s Wrath - Healing Flame'),
(91885, 0, 0, 0, 0, 'Whitefang - Stormfang'),
(91886, 0, 0, 0, 0, 'Whitefang - Flamefang'),
(91889, 0, 0, 0, 0, 'Whitefang - Mending Fang'),
(91912, 0, 0, 0, 0, 'Ursoc''s Champion - Rending Maul tick'),
(91914, 0, 0, 0, 0, 'Ursoc''s Champion - Thunderbear'),
(91916, 0, 0, 0, 0, 'Ursoc''s Champion - Ursoc''s Mend'),
(91917, 0, 0, 0, 0, 'Ursoc''s Champion - Shielding Roots'),
(91919, 0, 0, 0, 0, 'Ursoc''s Champion - Ursoc''s Impact (Nature)'),
(93015, 0, 0, 0, 0, 'Red Communion - Rending Communion'),
(93016, 0, 0, 0, 0, 'Red Communion - Earthbound Blood'),
(93017, 0, 0, 0, 0, 'Red Communion - Holy Communion'),
(93040, 0, 0, 0, 0, 'Primal Worldwarden - Bloodstone Command'),
(93041, 0, 0, 0, 0, 'Primal Worldwarden - Consecrated Heart'),
(93042, 0, 0, 0, 0, 'Primal Worldwarden - Runic Renewal'),
(93065, 0, 0, 0, 0, 'Sanctified Paragon - Judgment in the aura'),
(93066, 0, 0, 0, 0, 'Sanctified Paragon - Paragon''s Mend'),
(93067, 0, 0, 0, 0, 'Sanctified Paragon - Dawnward'),
(93068, 0, 0, 0, 0, 'Sanctified Paragon - Righteous Ward'),
(93092, 0, 0, 0, 0, 'Stormreaver - Frozen Front tick'),
(93093, 0, 0, 0, 0, 'Stormreaver - Stormhammer'),
(93094, 0, 0, 0, 0, 'Stormreaver - Warstorm'),
(93115, 0, 0, 0, 0, 'Black Covenant - Blood Pact'),
(93116, 0, 0, 0, 0, 'Black Covenant - Earthen Pact'),
(93117, 0, 0, 0, 0, 'Black Covenant - Frostbound Siphon'),
(93118, 0, 0, 0, 0, 'Black Covenant - Blood Intercession'),
(93140, 0, 0, 0, 0, 'Wandering Mist - Tidal Mist'),
(93141, 0, 0, 0, 0, 'Wandering Mist - Holy Thread'),
(93142, 0, 0, 0, 0, 'Wandering Mist - Earthen Cocoon'),
(93165, 0, 0, 0, 0, 'Wild Hunt - Frostfang'),
(93166, 0, 0, 0, 0, 'Wild Hunt - Storm Hunt'),
(93167, 0, 0, 0, 0, 'Wild Hunt - Healing Companion'),
(93191, 0, 0, 0, 0, 'Wellspring - Flowing Waters'),
(93192, 0, 0, 0, 0, 'Wellspring - Widening Waters'),
(93193, 0, 0, 0, 0, 'Wellspring - Riptide Spring'),
(93194, 0, 0, 0, 0, 'Wellspring - Jade Current'),
(93195, 0, 0, 0, 0, 'Wellspring - Shielded Spring'),
(93216, 0, 0, 0, 0, 'Echoing Thunder - Thunder Burst'),
(93217, 0, 0, 0, 0, 'Echoing Thunder - Stormheart'),
(93219, 0, 0, 0, 0, 'Echoing Thunder - Flame Echo tick'),
(93220, 0, 0, 0, 0, 'Echoing Thunder - Frost Echo'),
(93222, 0, 0, 0, 0, 'Echoing Thunder - Tidal Shield'),
(93241, 0, 0, 0, 0, 'Stormbound Conduit - Starbound Circuit'),
(93242, 0, 0, 0, 0, 'Stormbound Conduit - Shocking Surge'),
(93266, 0, 0, 0, 0, 'Shattered Sun - Lunar Shard'),
(93267, 0, 0, 0, 0, 'Shattered Sun - Storm Shard'),
(93268, 0, 0, 0, 0, 'Shattered Sun - Broken Core (Ice Lance)'),
(93270, 0, 0, 0, 0, 'Shattered Sun - Icy Shelter'),
(93291, 0, 0, 0, 0, 'Dawnbound Crusader - Crusader''s Flame'),
(93292, 0, 0, 0, 0, 'Dawnbound Crusader - Frostfire Dawn'),
(93317, 0, 0, 0, 0, 'Hungering Rift - Rift Lash'),
(93319, 0, 0, 0, 0, 'Hungering Rift - Lingering Torment tick'),
(93320, 0, 0, 0, 0, 'Hungering Rift - Last Gasp'),
(93321, 0, 0, 0, 0, 'Hungering Rift - Plague Maw'),
(93322, 0, 0, 0, 0, 'Hungering Rift - Doom Echo'),
(93323, 0, 0, 0, 0, 'Hungering Rift - Siphon Conduit'),
(93342, 0, 0, 0, 0, 'Howling North - Frost Blade'),
(93343, 0, 0, 0, 0, 'Howling North - Frost Strike follow-up'),
(93345, 0, 0, 0, 0, 'Howling North - the field'),
(93347, 0, 0, 0, 0, 'Howling North - Flame of Winter'),
(93348, 0, 0, 0, 0, 'Howling North - Stormblade'),
(93350, 0, 0, 0, 0, 'Howling North - Rending Ice tick'),
(93352, 0, 0, 0, 0, 'Howling North - Icy Wound tick'),
(93354, 0, 0, 0, 0, 'Howling North - Winter''s Renewal'),
(93379, 0, 0, 0, 0, 'Trueshot - pierce'),
(93380, 0, 0, 0, 0, 'Trueshot - Arcane payload'),
(93381, 0, 0, 0, 0, 'Trueshot - Flame Arrow'),
(93382, 0, 0, 0, 0, 'Trueshot - Frostpiercer'),
(93385, 0, 0, 0, 0, 'Trueshot - Renewed Aim'),
(93388, 0, 0, 0, 0, 'Scarlet Benediction - Crimson Burst'),
(93390, 0, 0, 0, 0, 'Scarlet Benediction - Verdant Embrace'),
(93392, 0, 0, 0, 0, 'Scarlet Benediction - Reversion'),
(93394, 0, 0, 0, 0, 'Scarlet Benediction - Dream Breath'),
(93395, 0, 0, 0, 0, 'Scarlet Benediction - Dream Breath over time'),
(93397, 0, 0, 0, 0, 'Scarlet Benediction - Spiritbloom (as Greater Heal)'),
(93399, 0, 0, 0, 0, 'Scarlet Benediction - Scarlet Bloom'),
(93417, 0, 0, 0, 0, 'Scarlet Benediction - Echo'),
(93418, 0, 0, 0, 0, 'Scarlet Benediction - Temporal Anomaly'),
(93419, 0, 0, 0, 0, 'Scarlet Benediction - Lingering Light'),
(93422, 0, 0, 0, 0, 'Scarlet Benediction - Spiritbloom (as Holy Light)'),
(93423, 0, 0, 0, 0, 'Scarlet Benediction - Reversion surge'),
(93441, 0, 0, 0, 0, 'Evergrove - Second Rejuvenation'),
(93442, 0, 0, 0, 0, 'Evergrove - Third Rejuvenation'),
(93443, 0, 0, 0, 0, 'Evergrove - Fourth Rejuvenation'),
(93444, 0, 0, 0, 0, 'Evergrove - Grove Shelter'),
(93445, 0, 0, 0, 0, 'Evergrove - Overgrowth'),
(93466, 0, 0, 0, 0, 'Iron Reckoning - Iron Bulwark'),
(93467, 0, 0, 0, 0, 'Iron Reckoning - impact'),
(93469, 0, 0, 0, 0, 'Iron Reckoning - Rending Impact tick'),
(93471, 0, 0, 0, 0, 'Iron Reckoning - Shielded Advance'),
(93472, 0, 0, 0, 0, 'Iron Reckoning - Wall of Iron');

-- Their damage over time: Lunar Wound, Astral Rupture.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (91302, 91325) AND `ScriptName` = 'spell_titan_rune_dot';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(91302, 'spell_titan_rune_dot'),
(91325, 'spell_titan_rune_dot');

-- Their strikes and shields are shares of the Hero's own hits, health, attack and spell power: no bonuses of their
-- own.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (91301, 91303, 91323, 91324, 91326, 91339, 91341, 91342, 91346, 91615,
    91618);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(91301, 0, 0, 0, 0, 'Elune''s Guardian - Lunar Owl'),
(91303, 0, 0, 0, 0, 'Elune''s Guardian - Lunar Wound tick'),
(91323, 0, 0, 0, 0, 'Astral Convergence - Lunar Collision'),
(91324, 0, 0, 0, 0, 'Astral Convergence - Solar Flare'),
(91326, 0, 0, 0, 0, 'Astral Convergence - Astral Rupture tick'),
(91339, 0, 0, 0, 0, 'Astral Convergence - Eventide'),
(91615, 0, 0, 0, 0, 'Astral Convergence - Flame Eclipse'),
(91618, 0, 0, 0, 0, 'Astral Convergence - Astral Ward'),
(91341, 0, 0, 0, 0, 'Ursoc''s Champion - Ursoc''s Guard'),
(91342, 0, 0, 0, 0, 'Ursoc''s Champion - Ursoc''s Blow'),
(91346, 0, 0, 0, 0, 'Ursoc''s Champion - Enduring Champion');

-- The Duelist (Sinister Strike readies Opening Shot; the rune teaches Pistol Shot) and the Stormreaver
-- (Stormstrike Stormstrikes its target, its hits counted; Lava Lash erupts; its aura's procs discharge Lightning
-- Shield; Thunderbomb and Magma Breaker, the abilities it teaches). The Stormbound Conduit's: Ball Lightning and
-- Storm Lance.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_sinister_strike',
    'spell_titan_rune_flintlock_shot', 'spell_titan_rune_pistol_shot', 'spell_titan_rune_stormstrike',
    'spell_titan_rune_lava_lash', 'spell_titan_rune_stormreaver', 'spell_titan_rune_stormstrike_hit',
    'spell_titan_rune_thunderbomb', 'spell_titan_rune_magma_breaker', 'spell_titan_rune_ball_lightning',
    'spell_titan_rune_storm_lance');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(-1752, 'spell_titan_rune_sinister_strike'),
(91382, 'spell_titan_rune_pistol_shot'),
(17364, 'spell_titan_rune_stormstrike'),
(32175, 'spell_titan_rune_stormstrike_hit'),
(32176, 'spell_titan_rune_stormstrike_hit'),
(60103, 'spell_titan_rune_lava_lash'),
(91400, 'spell_titan_rune_stormreaver'),
(93091, 'spell_titan_rune_thunderbomb'),
(93093, 'spell_titan_rune_magma_breaker'),
(93241, 'spell_titan_rune_ball_lightning'),
(93244, 'spell_titan_rune_storm_lance');

-- Their damage over time: Serrated Edge, White-Hot Conductor.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (91384, 91405) AND `ScriptName` = 'spell_titan_rune_dot';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(91384, 'spell_titan_rune_dot'),
(91405, 'spell_titan_rune_dot');

-- The Stormreaver's procs: every melee ability that hits.
DELETE FROM `spell_proc` WHERE `SpellId` = 91400;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`,
    `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`,
    `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(91400, 0, 0, 0, 0, 0, 16, 1, 2, 1027, 0, 0, 0, 100, 0, 0);

-- Their strikes are shares of the Hero's attack power and hits: no bonuses of their own.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (91383, 91385, 91399, 91402, 91403, 91404, 91406, 91407);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(91383, 0, 0, 0, 0, 'Duelist - Pistol Shot'),
(91385, 0, 0, 0, 0, 'Duelist - Serrated Edge tick'),
(91399, 0, 0, 0, 0, 'Duelist - Sabre Slash'),
(91402, 0, 0, 0, 0, 'Stormreaver - Stormfront pulse'),
(91403, 0, 0, 0, 0, 'Stormreaver - Thunderlash'),
(91404, 0, 0, 0, 0, 'Stormreaver - Thunderlash arc'),
(91406, 0, 0, 0, 0, 'Stormreaver - White-Hot Conductor tick'),
(91407, 0, 0, 0, 0, 'Stormreaver - Lightning Shield discharge');

-- The Duelist's Sabre Slash (its Sinister Strike) and Broadside (its finishing move).
DELETE FROM `spell_script_names` WHERE `spell_id` = 91370 AND `ScriptName` = 'spell_titan_rune_sinister_strike';
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_titan_rune_broadside';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(91370, 'spell_titan_rune_sinister_strike'),
(91371, 'spell_titan_rune_broadside');
DELETE FROM `spell_bonus_data` WHERE `entry` = 91372;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(91372, 0, 0, 0, 0, 'Duelist - Broadside blast');

-- The Duelist's Footwork modifiers (Advance, Disengaging Shot, Press the Advantage) gave way to Cannon-Fire;
-- 91386 is Rapid Shots now, 91387-91389 retired.
DELETE FROM `spell_dbc` WHERE `ID` IN (91387, 91388, 91389);

-- Ember's Wrath (the Hero's Fire Blast, in Cat Form too, ignites their Rake or Rip: Cinder Wound) and Whitefang
-- (their Icy Touch lays Frozen Prey; Shred or Mangle shatters it): the spells they reach into. The Cat Form copies
-- they first taught (91420, 91440) are gone.
DELETE FROM `spell_dbc` WHERE `ID` IN (91420, 91440);
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_cat_fire_blast',
    'spell_titan_rune_cat_icy_touch', 'spell_titan_rune_embers_fire_blast', 'spell_titan_rune_whitefang_icy_touch',
    'spell_titan_rune_cat_strike', 'spell_titan_rune_ferocious_bite', 'spell_titan_rune_cat_swipe',
    'spell_titan_rune_cat_shock', 'spell_titan_rune_cat_form');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(-2136, 'spell_titan_rune_embers_fire_blast'),
(-45477, 'spell_titan_rune_whitefang_icy_touch'),
(-8050, 'spell_titan_rune_cat_shock'),
(-8056, 'spell_titan_rune_cat_shock'),
(-1822, 'spell_titan_rune_cat_strike'),
(-1082, 'spell_titan_rune_cat_strike'),
(-33876, 'spell_titan_rune_cat_strike'),
(-5221, 'spell_titan_rune_cat_strike'),
(-22568, 'spell_titan_rune_ferocious_bite'),
(62078, 'spell_titan_rune_cat_swipe'),
(768, 'spell_titan_rune_cat_form');

-- Their damage over time: Cinder Wound (and its spread), Winter Wound, Blazing Bleed, Frostbite.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (91421, 91423, 91444, 91465, 91470)
    AND `ScriptName` = 'spell_titan_rune_dot';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(91421, 'spell_titan_rune_dot'),
(91423, 'spell_titan_rune_dot'),
(91444, 'spell_titan_rune_dot'),
(91465, 'spell_titan_rune_dot'),
(91470, 'spell_titan_rune_dot');

-- Their strikes are shares of the Hero's power: no bonuses of their own.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (91420, 91422, 91424, 91425, 91426, 91440, 91443, 91445, 91460, 91464,
    91466, 91467, 91469, 91471, 91473);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(91422, 0, 0, 0, 0, 'Ember''s Wrath - Cinder Wound tick'),
(91424, 0, 0, 0, 0, 'Ember''s Wrath - Wild Embers'),
(91425, 0, 0, 0, 0, 'Ember''s Wrath - Blazing Rake'),
(91426, 0, 0, 0, 0, 'Ember''s Wrath - Molten Bite'),
(91443, 0, 0, 0, 0, 'Whitefang - Shatter'),
(91445, 0, 0, 0, 0, 'Whitefang - Winter Wound tick'),
(91460, 0, 0, 0, 0, 'Whitefang - Frozen Trail'),
(91464, 0, 0, 0, 0, 'Ember''s Wrath - Blazing Strike'),
(91466, 0, 0, 0, 0, 'Ember''s Wrath - Blazing Bleed tick'),
(91467, 0, 0, 0, 0, 'Ember''s Wrath - Fiery Explosion'),
(91469, 0, 0, 0, 0, 'Whitefang - Frostfang Strike'),
(91471, 0, 0, 0, 0, 'Whitefang - Frostbite tick'),
(91473, 0, 0, 0, 0, 'Whitefang - Glacial Burst');

-- The Tank Collection (src/tank_runes.cpp): the Worldheart Totem (Primal Worldwarden's: an earth totem, larger,
-- following its Hero, out of reach), and the scripts on Blood Boil, Blood Strike, the Shocks, Stormstrike, Lava Lash,
-- the taught actions and Soul Siphon.
SET @WORLDHEART := 910127;
DELETE FROM `creature_template` WHERE `entry` = @WORLDHEART;
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
    `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
    `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`,
    `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
    `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`,
    `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
    `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(@WORLDHEART, 0, 0, 0, 0, 0, 'Worldheart Totem', 'Primal Worldwarden', NULL, 0, 1, 80, 0, 35, 0, 1, 1.6, 1, 1, 20,
    0, 0, 1, 2000, 2000, 1, 1, 1, 33554434, 0, 0, 0, 11, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 0, 0, 0, 1, 0,
    0, '', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` = @WORLDHEART;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(@WORLDHEART, 0, 4588, 1.6, 1, 0);

DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_tank_blood_boil',
    'spell_titan_tank_blood_strike', 'spell_titan_tank_shock', 'spell_titan_tank_stormstrike',
    'spell_titan_tank_lava_lash', 'spell_titan_tank_action', 'spell_titan_tank_siphon',
    'spell_titan_tank_siphon_cast');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(-48721, 'spell_titan_tank_blood_boil'),
(-45902, 'spell_titan_tank_blood_strike'),
(-8042, 'spell_titan_tank_shock'),
(-8050, 'spell_titan_tank_shock'),
(-8056, 'spell_titan_tank_shock'),
(17364, 'spell_titan_tank_stormstrike'),
(60103, 'spell_titan_tank_lava_lash'),
(91482, 'spell_titan_tank_action'),
(91500, 'spell_titan_tank_action'),
(91501, 'spell_titan_tank_action'),
(91520, 'spell_titan_tank_action'),
(91523, 'spell_titan_tank_action'),
(91529, 'spell_titan_tank_action'),
(91544, 'spell_titan_tank_action'),
(91525, 'spell_titan_tank_siphon'),
(91525, 'spell_titan_tank_siphon_cast');

-- The tank runes' Extended Options: their other abilities (Mangle (Bear), Death Coil, Holy Shock's heal, Earth
-- Shield's heal, the Judgements, Thunder Clap), every rank.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_titan_tank_extended';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(-33878, 'spell_titan_tank_extended'),
(-47541, 'spell_titan_tank_extended'),
(-6789, 'spell_titan_tank_extended'),
(-25914, 'spell_titan_tank_extended'),
(379, 'spell_titan_tank_extended'),
(20271, 'spell_titan_tank_extended'),
(53408, 'spell_titan_tank_extended'),
(53407, 'spell_titan_tank_extended'),
(-6343, 'spell_titan_tank_extended');

-- The Stormreaver's and the Stormbound Conduit's replacements (Thunderbomb, Magma Breaker, Ball Lightning, Storm
-- Lance): their hits run the scripts the abilities they replace run (their main target's hit counting as that
-- ability: runes.cpp CountHitAs; their tier_hit rows in titan_rune_effects.sql). Their hits and Fulmination take the
-- Hero's modifiers (as their bases' Shaman family: talents and glyphs raise them), no spell power of their own.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (93092, 93094, 93243, 93245) AND `ScriptName` IN
    ('spell_titan_rune_extended_hit', 'spell_titan_tank_stormstrike', 'spell_titan_tank_lava_lash',
    'spell_titan_tank_shock');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(93092, 'spell_titan_tank_stormstrike'),
(93094, 'spell_titan_rune_extended_hit'),
(93094, 'spell_titan_tank_lava_lash'),
(93243, 'spell_titan_rune_extended_hit'),
(93245, 'spell_titan_rune_extended_hit'),
(93245, 'spell_titan_tank_shock');
DELETE FROM `spell_bonus_data` WHERE `entry` IN (93092, 93094, 93240, 93242, 93243, 93245);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(93092, 0, 0, 0, 0, 'Thunderbomb - burst'),
(93094, 0, 0, 0, 0, 'Magma Breaker - hit'),
(93240, 0, 0, 0, 0, 'Fulmination'),
(93242, 0, 0, 0, 0, 'Ball Lightning - zap'),
(93243, 0, 0, 0, 0, 'Ball Lightning - burst'),
(93245, 0, 0, 0, 0, 'Storm Lance - hit');

-- Scarlet Benediction's redesign: Verdant Embrace, Reversion, Dream Breath and Spiritbloom (the abilities it teaches in
-- Binding Heal's or Divine Favor's, Renew's, Prayer of Healing's and Greater Heal's or Holy Light's places) and
-- Reversion's heal over time (its second tick below half health). Holy Light, Flash of Light and Prayer of Mending's
-- landing answer the rune too, as does Spiritbloom's heal (as Greater Heal or Holy Light: runes.cpp CountHitAs).
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_scarlet_ability',
    'spell_titan_rune_reversion_hot');
DELETE FROM `spell_script_names` WHERE `spell_id` IN (-635, -19750, 41635, 48110, 48111, 93397, 93422)
    AND `ScriptName` = 'spell_titan_rune_extended_hit';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(93389, 'spell_titan_rune_scarlet_ability'),
(93391, 'spell_titan_rune_scarlet_ability'),
(93393, 'spell_titan_rune_scarlet_ability'),
(93396, 'spell_titan_rune_scarlet_ability'),
(93392, 'spell_titan_rune_reversion_hot'),
(-635, 'spell_titan_rune_extended_hit'),
(-19750, 'spell_titan_rune_extended_hit'),
(41635, 'spell_titan_rune_extended_hit'),
(48110, 'spell_titan_rune_extended_hit'),
(48111, 'spell_titan_rune_extended_hit'),
(93397, 'spell_titan_rune_extended_hit'),
(93422, 'spell_titan_rune_extended_hit');
