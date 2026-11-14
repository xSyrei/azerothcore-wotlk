-- Legendary Runes (src/runes.cpp; the spells 90700-91038: client-patch/patch_runes.py, in
-- titan_demon_hunter_spells.sql). The Rune of Fel Commandment's Felguard and Fel Summons' imp, its scripts and
-- its proc.
SET @FELGUARD := 910120;
SET @IMP := 910121;
SET @HOLLOW_ECHO := 910122;
SET @SPECTRAL_BEAST := 910123;
DELETE FROM `creature_template` WHERE `entry` IN (@FELGUARD, @IMP, @HOLLOW_ECHO, @SPECTRAL_BEAST);
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
    0, 'npc_titan_rune_spectral_beast', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (@FELGUARD, @IMP, @HOLLOW_ECHO, @SPECTRAL_BEAST);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(@FELGUARD, 0, 14255, 1, 1, 0),
(@IMP, 0, 4449, 1, 1, 0),
(@HOLLOW_ECHO, 0, 11686, 1, 1, 0),
(@SPECTRAL_BEAST, 0, 903, 1, 1, 0);

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
(91013, 'spell_titan_rune_dot');

DELETE FROM `spell_bonus_data` WHERE `entry` IN (90832, 90833, 90834, 90984, 91014);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90832, 0, 0, 0, 0, 'Threefold Grave - Grave Pestilence tick'),
(90833, 0, 0, 0, 0, 'Unbroken Onslaught - Bloodletting tick'),
(90834, 0, 0, 0, 0, 'Hollow Congregation - Hollow Flay tick'),
(90984, 0, 0, 0, 0, 'Spectral Blade - Open Wound tick'),
(91014, 0, 0, 0, 0, 'Wild Hunt - Ghostfang tick');

-- The Spectral Blade: Eviscerate calls the phantom; Sinister Strike, Backstab and Hemorrhage spend Marked for Murder
-- (every rank). The Wild Hunt: Kill Command unleashes the Spectral Beast (910123, the Hero's pet's look).
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_eviscerate',
    'spell_titan_rune_marked_strike', 'spell_titan_rune_kill_command');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(-2098, 'spell_titan_rune_eviscerate'),
(-1752, 'spell_titan_rune_marked_strike'),
(-53, 'spell_titan_rune_marked_strike'),
(-16511, 'spell_titan_rune_marked_strike'),
(34026, 'spell_titan_rune_kill_command');

-- Their strikes are shares of the Hero's (or their pet's) own hits: no bonuses of their own.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (90981, 91012, 91016);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90981, 0, 0, 0, 0, 'Spectral Blade - Phantom Strike'),
(91012, 0, 0, 0, 0, 'Wild Hunt - Spectral Bite'),
(91016, 0, 0, 0, 0, 'Wild Hunt - Final Pounce');
