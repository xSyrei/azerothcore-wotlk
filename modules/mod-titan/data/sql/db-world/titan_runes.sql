-- Legendary Runes (src/runes.cpp; the spells 90700-90828: client-patch/patch_runes.py, in
-- titan_demon_hunter_spells.sql). The Rune of Fel Commandment's Felguard and Fel Summons' imp, its scripts and
-- its proc.
SET @FELGUARD := 910120;
SET @IMP := 910121;
DELETE FROM `creature_template` WHERE `entry` IN (@FELGUARD, @IMP);
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
    'npc_titan_rune_fel_imp', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (@FELGUARD, @IMP);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(@FELGUARD, 0, 14255, 1, 1, 0),
(@IMP, 0, 4449, 1, 1, 0);

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

-- The Rune of the Stormbound Conduit: its aura reads Lightning Bolt and Chain Lightning hits; Lightning Shield
-- (every rank) gives way to Storm Within and Distant Conductor; its retaliations (every rank, and the rune's own)
-- arc and take its modifiers.
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
DELETE FROM `spell_bonus_data` WHERE `entry` IN (90752, 90754);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
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

-- The Rune of the Wellspring: Chain Heal (every rank) makes and reaches the Wellspring; Earth Shield's heal refreshes
-- it (Flowing Waters).
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
(90792, 0, 0, 0, 0, 'Wellspring - additional jump'),
(90793, 0, 0, 0, 0, 'Wellspring - Tidal Shelter'),
(90794, 0, 0, 0, 0, 'Wellspring - Renewing Current'),
(90811, 0, 0, 0, 0, 'Wandering Mist'),
(90812, 0, 0, 0, 0, 'Wandering Mist - lesser Enveloping Mist'),
(90813, 0, 0, 0, 0, 'Wandering Mist - Rolling Mist'),
(90814, 0, 0, 0, 0, 'Wandering Mist - Jade Shelter');
