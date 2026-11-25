-- The Firepit (src/firepit.cpp): an endless wave arena in a private Ring of Law. The Firepit Keeper (Grimstone's look)
-- stands beside Silas Darkmoon in Stormwind and Orgrimmar, and is summoned in the ring when a run ends to lead the
-- Heroes out. Each Hero's spoils chest (Arena Spoils' look) opens for its owner alone.
SET @KEEPER := 910140;
SET @GUID := 9101400;
SET @SPOILS := 910140;

DELETE FROM `creature_template` WHERE `entry` = @KEEPER;
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
    `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
    `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`,
    `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
    `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`,
    `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
    `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(@KEEPER, 0, 0, 0, 0, 0, 'Firepit Keeper', 'The Firepit', NULL, 0, 60, 60, 0, 35, 1, 1, 1.14286, 1, 1, 20,
    0, 0, 1, 2000, 2000, 1, 1, 1, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 10, 1, 1, 0, 0, 0, 1, 0, 0,
    'npc_titan_firepit_keeper', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` = @KEEPER;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(@KEEPER, 0, 9329, 1, 1, 0);

DELETE FROM `creature` WHERE `id` = @KEEPER AND `guid` IN (@GUID, @GUID + 1);
INSERT INTO `creature` (`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`,
    `orientation`, `spawntimesecs`, `Comment`) VALUES
(@GUID, @KEEPER, 0, 1, 1, -8823.5, 643.0, 94.45, 3.9, 300, 'Firepit Keeper, Stormwind'),
(@GUID + 1, @KEEPER, 1, 1, 1, 1633.5, -4409.5, 16.4, 0.44, 300, 'Firepit Keeper, Orgrimmar');

DELETE FROM `gameobject_template` WHERE `entry` = @SPOILS;
INSERT INTO `gameobject_template` (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`,
    `size`, `ScriptName`, `VerifiedBuild`) VALUES
(@SPOILS, 3, 10, 'Spoils of the Firepit', '', '', '', 1.2, 'go_titan_firepit_spoils', 0);
