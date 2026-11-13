-- Every starting area (where a new Hero appears: playercreateinfo, hero_class.sql) has Silas Darkmoon (cards,
-- titan_darkmoon.sql), the Keeper of the Ember (prestige, titan_prestige.sql) and two training dummies a few yards
-- ahead of the Hero's first step. The positions follow the start points (their height theirs): move one in game
-- (.npc move) if it stands in the ground.
-- The dummy (910130, src/starter_zones.cpp): the Expert's Training Dummy's look and flags, never damaged, its
-- level matching whoever engages it (a level 1 Hero does not miss a level 60 dummy).
SET @SILAS := 910110;
SET @KEEPER := 910100;
SET @DUMMY := 910130;
SET @GUID := 9101100;

DELETE FROM `creature_template` WHERE `entry` = @DUMMY;
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
    `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
    `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`,
    `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
    `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`,
    `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
    `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(@DUMMY, 0, 0, 0, 0, 0, 'Training Dummy', 'Emberforge', '', 0, 1, 1, 0, 7, 0, 1, 1, 1, 1, 20, 0, 0, 1, 2000,
    2000, 1, 1, 1, 131072, 2048, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 262144,
    'npc_titan_training_dummy', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` = @DUMMY;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(@DUMMY, 0, 28047, 1, 1, 0);

DELETE FROM `creature` WHERE `guid` BETWEEN @GUID AND @GUID + 99;
INSERT INTO `creature` (`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`,
    `orientation`, `spawntimesecs`, `Comment`) VALUES
(@GUID + 0, @SILAS, 0, 1, 1, -8943.95, -129.99, 83.53, 3.14, 300, 'Silas Darkmoon, Northshire Valley'),
(@GUID + 1, @KEEPER, 0, 1, 1, -8943.95, -134.99, 83.53, 3.14, 300, 'Keeper of the Ember, Northshire Valley'),
(@GUID + 2, @DUMMY, 0, 1, 1, -8939.95, -127.99, 83.53, 3.14, 300, 'Training Dummy, Northshire Valley'),
(@GUID + 3, @DUMMY, 0, 1, 1, -8939.95, -136.99, 83.53, 3.14, 300, 'Training Dummy, Northshire Valley'),
(@GUID + 4, @SILAS, 1, 1, 1, -612.52, -4249.17, 38.72, 3.14, 300, 'Silas Darkmoon, Valley of Trials'),
(@GUID + 5, @KEEPER, 1, 1, 1, -612.52, -4254.17, 38.72, 3.14, 300, 'Keeper of the Ember, Valley of Trials'),
(@GUID + 6, @DUMMY, 1, 1, 1, -608.52, -4247.17, 38.72, 3.14, 300, 'Training Dummy, Valley of Trials'),
(@GUID + 7, @DUMMY, 1, 1, 1, -608.52, -4256.17, 38.72, 3.14, 300, 'Training Dummy, Valley of Trials'),
(@GUID + 8, @SILAS, 0, 1, 1, -6234.09, 332.88, 382.76, 3.04, 300, 'Silas Darkmoon, Coldridge Valley'),
(@GUID + 9, @KEEPER, 0, 1, 1, -6234.62, 327.91, 382.76, 3.04, 300, 'Keeper of the Ember, Coldridge Valley'),
(@GUID + 10, @DUMMY, 0, 1, 1, -6229.90, 334.45, 382.76, 3.04, 300, 'Training Dummy, Coldridge Valley'),
(@GUID + 11, @DUMMY, 0, 1, 1, -6230.85, 325.50, 382.76, 3.04, 300, 'Training Dummy, Coldridge Valley'),
(@GUID + 12, @SILAS, 1, 1, 1, 10317.68, 831.22, 1326.41, 2.55, 300, 'Silas Darkmoon, Shadowglen'),
(@GUID + 13, @KEEPER, 1, 1, 1, 10314.91, 827.06, 1326.41, 2.55, 300, 'Keeper of the Ember, Shadowglen'),
(@GUID + 14, @DUMMY, 1, 1, 1, 10322.12, 830.67, 1326.41, 2.55, 300, 'Training Dummy, Shadowglen'),
(@GUID + 15, @DUMMY, 1, 1, 1, 10317.13, 823.18, 1326.41, 2.55, 300, 'Training Dummy, Shadowglen'),
(@GUID + 16, @SILAS, 0, 1, 1, 1670.22, 1678.58, 121.67, 5.85, 300, 'Silas Darkmoon, Deathknell'),
(@GUID + 17, @KEEPER, 0, 1, 1, 1672.33, 1683.11, 121.67, 5.85, 300, 'Keeper of the Ember, Deathknell'),
(@GUID + 18, @DUMMY, 0, 1, 1, 1665.75, 1678.46, 121.67, 5.85, 300, 'Training Dummy, Deathknell'),
(@GUID + 19, @DUMMY, 0, 1, 1, 1669.55, 1686.61, 121.67, 5.85, 300, 'Training Dummy, Deathknell'),
(@GUID + 20, @SILAS, 1, 1, 1, -2911.58, -255.48, 53.00, 3.14, 300, 'Silas Darkmoon, Camp Narache'),
(@GUID + 21, @KEEPER, 1, 1, 1, -2911.58, -260.48, 53.00, 3.14, 300, 'Keeper of the Ember, Camp Narache'),
(@GUID + 22, @DUMMY, 1, 1, 1, -2907.58, -253.48, 53.00, 3.14, 300, 'Training Dummy, Camp Narache'),
(@GUID + 23, @DUMMY, 1, 1, 1, -2907.58, -262.48, 53.00, 3.14, 300, 'Training Dummy, Camp Narache'),
(@GUID + 24, @SILAS, 530, 1, 1, 10355.06, -6360.81, 33.40, 2.17, 300, 'Silas Darkmoon, Sunstrider Isle'),
(@GUID + 25, @KEEPER, 530, 1, 1, 10350.95, -6363.65, 33.40, 2.17, 300, 'Keeper of the Ember, Sunstrider Isle'),
(@GUID + 26, @DUMMY, 530, 1, 1, 10358.98, -6362.97, 33.40, 2.17, 300, 'Training Dummy, Sunstrider Isle'),
(@GUID + 27, @DUMMY, 530, 1, 1, 10351.57, -6368.08, 33.40, 2.17, 300, 'Training Dummy, Sunstrider Isle'),
(@GUID + 28, @SILAS, 530, 1, 1, -3966.76, -13927.20, 100.61, 5.23, 300, 'Silas Darkmoon, Ammen Vale'),
(@GUID + 29, @KEEPER, 530, 1, 1, -3962.41, -13924.75, 100.61, 5.23, 300, 'Keeper of the Ember, Ammen Vale'),
(@GUID + 30, @DUMMY, 530, 1, 1, -3970.47, -13924.69, 100.61, 5.23, 300, 'Training Dummy, Ammen Vale'),
(@GUID + 31, @DUMMY, 530, 1, 1, -3962.63, -13920.28, 100.61, 5.23, 300, 'Training Dummy, Ammen Vale');
