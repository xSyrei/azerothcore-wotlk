-- The Firepit (src/firepit.cpp): an endless wave arena on a map of its own. The Firepit Keeper (Grimstone's look)
-- stands beside Silas Darkmoon in Stormwind and Orgrimmar, and is summoned in the ring when a run ends to lead the
-- Heroes out. The run's spoils chest (Arena Spoils' look, three times its size) gives each Hero their own.
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

-- A boss's Molten Eruption (src/firepit.cpp, from wave 15): an invisible source, its Fire Nova only a look.
SET @ERUPTION := 910141;
DELETE FROM `creature_template` WHERE `entry` = @ERUPTION;
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
    `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
    `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`,
    `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
    `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`,
    `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
    `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(@ERUPTION, 0, 0, 0, 0, 0, 'Molten Eruption', '', NULL, 0, 60, 60, 0, 35, 0, 1, 1.14286, 1, 1, 20,
    0, 2, 1, 2000, 2000, 1, 1, 1, 33554434, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, '', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` = @ERUPTION;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(@ERUPTION, 0, 11686, 1, 1, 0);

-- The Ashen Echo (src/firepit.cpp, the Ashen Echo affix): a fire spirit at the arena's edge channelling into an
-- enemy; attackable, it never fights back.
SET @ASHEN_ECHO := 910129;
DELETE FROM `creature_template` WHERE `entry` = @ASHEN_ECHO;
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
    `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
    `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`,
    `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
    `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`,
    `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
    `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(@ASHEN_ECHO, 0, 0, 0, 0, 0, 'Ashen Echo', 'The Firepit', NULL, 0, 60, 60, 0, 14, 0, 1, 1.14286, 1, 1, 20,
    0, 2, 1, 2000, 2000, 1, 1, 8, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, '', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` = @ASHEN_ECHO;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(@ASHEN_ECHO, 0, 4607, 1, 1, 0);

-- The major bosses (src/firepit.cpp, Marked for Carnage: waves 45, 60, 75, 90, 100, then every 15 from 105): elite,
-- Gorosh the Dervish's strength; their encounters are the Firepit's own. The Matron's Web Anchors hold her webs.
SET @ASHFLEET := 910150;
SET @COLOSSUS := 910151;
SET @MATRON := 910152;
SET @WARDEN := 910153;
SET @CHAMPION := 910154;
SET @WEB_ANCHOR := 910155;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 910150 AND 910155;
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
    `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
    `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`,
    `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
    `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`,
    `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
    `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(@ASHFLEET, 0, 0, 0, 0, 0, 'Ashfleet Captain', 'The Firepit', NULL, 0, 60, 60, 0, 14, 0, 1, 1.14286, 1, 1, 20,
    1, 0, 7.5, 1800, 2000, 1, 1, 1, 0, 2048, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 6, 2, 1, 1, 0, 0, 1,
    0, 0, '', 0),
(@COLOSSUS, 0, 0, 0, 0, 0, 'Ironbound Colossus', 'The Firepit', NULL, 0, 60, 60, 0, 14, 0, 1, 1.14286, 1, 1, 20,
    1, 0, 7.5, 1800, 2000, 1, 1, 1, 0, 2048, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 6, 2, 1, 1, 0, 0, 1,
    0, 0, '', 0),
(@MATRON, 0, 0, 0, 0, 0, 'Cinder Matron', 'The Firepit', NULL, 0, 60, 60, 0, 14, 0, 1, 1.14286, 1, 1, 20,
    1, 0, 7.5, 1800, 2000, 1, 1, 1, 0, 2048, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 6, 2, 1, 1, 0, 0, 1,
    0, 0, '', 0),
(@WARDEN, 0, 0, 0, 0, 0, 'Furnace Warden', 'The Firepit', NULL, 0, 60, 60, 0, 14, 0, 1, 1.14286, 1, 1, 20,
    1, 0, 7.5, 1800, 2000, 1, 1, 1, 0, 2048, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 6, 2, 1, 1, 0, 0, 1,
    0, 0, '', 0),
(@CHAMPION, 0, 0, 0, 0, 0, 'Firepit Champion', 'The Firepit', NULL, 0, 60, 60, 0, 14, 0, 1, 1.14286, 1, 1, 20,
    1, 0, 7.5, 1800, 2000, 1, 1, 1, 0, 2048, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 6, 2, 1, 1, 0, 0, 1,
    0, 0, '', 0),
(@WEB_ANCHOR, 0, 0, 0, 0, 0, 'Web Anchor', 'The Firepit', NULL, 0, 60, 60, 0, 14, 0, 1, 1.14286, 1, 1, 20,
    0, 0, 1, 2000, 2000, 1, 1, 1, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 0, '', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 910150 AND 910155;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(@ASHFLEET, 0, 7113, 1.5, 1, 0),
(@COLOSSUS, 0, 26154, 1.2, 1, 0),
(@MATRON, 0, 15928, 0.6, 1, 0),
(@WARDEN, 0, 29185, 0.45, 1, 0),
(@CHAMPION, 0, 8760, 1.4, 1, 0),
(@WEB_ANCHOR, 0, 16213, 1, 1, 0);

-- The Spectators (src/firepit.cpp): the stands' crowd, dressed as the roster's enemies as they are seated; never
-- selectable, never fighting.
SET @SPECTATOR := 910156;
DELETE FROM `creature_template` WHERE `entry` = @SPECTATOR;
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
    `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
    `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`,
    `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
    `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`,
    `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
    `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(@SPECTATOR, 0, 0, 0, 0, 0, 'Spectator', 'The Firepit', NULL, 0, 60, 60, 0, 35, 0, 1, 1.14286, 1, 1, 20,
    0, 0, 1, 2000, 2000, 1, 1, 1, 33555202, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    2, '', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` = @SPECTATOR;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(@SPECTATOR, 0, 11686, 1, 1, 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` = @KEEPER;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(@KEEPER, 0, 9329, 1, 1, 0);

DELETE FROM `creature` WHERE `id` = @KEEPER AND `guid` IN (@GUID, @GUID + 1);
INSERT INTO `creature` (`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`,
    `orientation`, `spawntimesecs`, `Comment`) VALUES
(@GUID, @KEEPER, 0, 1, 1, -8823.5, 643.0, 94.45, 3.9, 300, 'Firepit Keeper, Stormwind'),
(@GUID + 1, @KEEPER, 1, 1, 1, 1633.5, -4409.5, 16.4, 0.44, 300, 'Firepit Keeper, Orgrimmar');

-- The spoils' great chest: retail's challenge chest (display 9700, client-patch/patch_creatures.py GAMEOBJECTS).
DELETE FROM `gameobject_template` WHERE `entry` = @SPOILS;
INSERT INTO `gameobject_template` (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`,
    `size`, `ScriptName`, `VerifiedBuild`) VALUES
(@SPOILS, 3, 9700, 'Spoils of the Firepit', '', '', '', 1.0, 'go_titan_firepit_spoils', 0);

-- The ring dressed for each run (src/firepit.cpp, DECOR_RINGS): the client's own fire models as plain, unselectable
-- objects (type 5): Blackrock's Dark Iron brazier, a midsummer bonfire, a large fire pit, a lava pillar, a ring of
-- flame, rising embers and smoke, a fire rift.
DELETE FROM `gameobject_template` WHERE `entry` BETWEEN 910141 AND 910147;
INSERT INTO `gameobject_template` (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`,
    `size`, `ScriptName`, `VerifiedBuild`) VALUES
(910141, 5, 3411, 'Dark Iron Brazier', '', '', '', 1.3, '', 0),
(910142, 5, 6754, 'Firepit Bonfire', '', '', '', 1.0, '', 0),
(910143, 5, 307, 'The Firepit', '', '', '', 1.0, '', 0),
(910144, 5, 205, 'Lava Pillar', '', '', '', 0.7, '', 0),
(910145, 5, 410, 'Ring of Flame', '', '', '', 2.2, '', 0),
(910146, 5, 4452, 'Rising Embers', '', '', '', 1.6, '', 0),
(910147, 5, 6333, 'Fire Rift', '', '', '', 0.9, '', 0);

DELETE FROM `gameobject_template_addon` WHERE `entry` BETWEEN 910141 AND 910147;
INSERT INTO `gameobject_template_addon` (`entry`, `faction`, `flags`, `mingold`, `maxgold`) VALUES
(910141, 0, 16, 0, 0), (910142, 0, 16, 0, 0), (910143, 0, 16, 0, 0), (910144, 0, 16, 0, 0),
(910145, 0, 16, 0, 0), (910146, 0, 16, 0, 0), (910147, 0, 16, 0, 0);

-- The area decals (src/firepit.cpp Telegraph; client-patch build_firepit_decals.py), in six colours (danger, safe,
-- soak, heal, arcane, frost), 16 a colour from 910160 (displays 9701 on): its 8 shapes' outlines, then their timers;
-- flat on the floor, unselectable. The server sizes each to its area and grows the timer to its edge as the blow
-- comes.
DELETE FROM `gameobject_template` WHERE `entry` BETWEEN 910160 AND 910255;
INSERT INTO `gameobject_template` (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`,
    `size`, `ScriptName`, `VerifiedBuild`) VALUES
(910160, 5, 9701, 'Firepit danger: circle', '', '', '', 1.0, '', 0),
(910161, 5, 9702, 'Firepit danger: ring', '', '', '', 1.0, '', 0),
(910162, 5, 9703, 'Firepit danger: 90 degree cone', '', '', '', 1.0, '', 0),
(910163, 5, 9704, 'Firepit danger: half (10 yards)', '', '', '', 1.0, '', 0),
(910164, 5, 9705, 'Firepit danger: half (arena)', '', '', '', 1.0, '', 0),
(910165, 5, 9706, 'Firepit danger: third (arena)', '', '', '', 1.0, '', 0),
(910166, 5, 9707, 'Firepit danger: line', '', '', '', 1.0, '', 0),
(910167, 5, 9708, 'Firepit danger: line end', '', '', '', 1.0, '', 0),
(910168, 5, 9709, 'Firepit danger timer: circle', '', '', '', 1.0, '', 0),
(910169, 5, 9710, 'Firepit danger timer: ring', '', '', '', 1.0, '', 0),
(910170, 5, 9711, 'Firepit danger timer: 90 degree cone', '', '', '', 1.0, '', 0),
(910171, 5, 9712, 'Firepit danger timer: half (10 yards)', '', '', '', 1.0, '', 0),
(910172, 5, 9713, 'Firepit danger timer: half (arena)', '', '', '', 1.0, '', 0),
(910173, 5, 9714, 'Firepit danger timer: third (arena)', '', '', '', 1.0, '', 0),
(910174, 5, 9715, 'Firepit danger timer: line', '', '', '', 1.0, '', 0),
(910175, 5, 9716, 'Firepit danger timer: line end', '', '', '', 1.0, '', 0),
(910176, 5, 9717, 'Firepit safe: circle', '', '', '', 1.0, '', 0),
(910177, 5, 9718, 'Firepit safe: ring', '', '', '', 1.0, '', 0),
(910178, 5, 9719, 'Firepit safe: 90 degree cone', '', '', '', 1.0, '', 0),
(910179, 5, 9720, 'Firepit safe: half (10 yards)', '', '', '', 1.0, '', 0),
(910180, 5, 9721, 'Firepit safe: half (arena)', '', '', '', 1.0, '', 0),
(910181, 5, 9722, 'Firepit safe: third (arena)', '', '', '', 1.0, '', 0),
(910182, 5, 9723, 'Firepit safe: line', '', '', '', 1.0, '', 0),
(910183, 5, 9724, 'Firepit safe: line end', '', '', '', 1.0, '', 0),
(910184, 5, 9725, 'Firepit safe timer: circle', '', '', '', 1.0, '', 0),
(910185, 5, 9726, 'Firepit safe timer: ring', '', '', '', 1.0, '', 0),
(910186, 5, 9727, 'Firepit safe timer: 90 degree cone', '', '', '', 1.0, '', 0),
(910187, 5, 9728, 'Firepit safe timer: half (10 yards)', '', '', '', 1.0, '', 0),
(910188, 5, 9729, 'Firepit safe timer: half (arena)', '', '', '', 1.0, '', 0),
(910189, 5, 9730, 'Firepit safe timer: third (arena)', '', '', '', 1.0, '', 0),
(910190, 5, 9731, 'Firepit safe timer: line', '', '', '', 1.0, '', 0),
(910191, 5, 9732, 'Firepit safe timer: line end', '', '', '', 1.0, '', 0),
(910192, 5, 9733, 'Firepit soak: circle', '', '', '', 1.0, '', 0),
(910193, 5, 9734, 'Firepit soak: ring', '', '', '', 1.0, '', 0),
(910194, 5, 9735, 'Firepit soak: 90 degree cone', '', '', '', 1.0, '', 0),
(910195, 5, 9736, 'Firepit soak: half (10 yards)', '', '', '', 1.0, '', 0),
(910196, 5, 9737, 'Firepit soak: half (arena)', '', '', '', 1.0, '', 0),
(910197, 5, 9738, 'Firepit soak: third (arena)', '', '', '', 1.0, '', 0),
(910198, 5, 9739, 'Firepit soak: line', '', '', '', 1.0, '', 0),
(910199, 5, 9740, 'Firepit soak: line end', '', '', '', 1.0, '', 0),
(910200, 5, 9741, 'Firepit soak timer: circle', '', '', '', 1.0, '', 0),
(910201, 5, 9742, 'Firepit soak timer: ring', '', '', '', 1.0, '', 0),
(910202, 5, 9743, 'Firepit soak timer: 90 degree cone', '', '', '', 1.0, '', 0),
(910203, 5, 9744, 'Firepit soak timer: half (10 yards)', '', '', '', 1.0, '', 0),
(910204, 5, 9745, 'Firepit soak timer: half (arena)', '', '', '', 1.0, '', 0),
(910205, 5, 9746, 'Firepit soak timer: third (arena)', '', '', '', 1.0, '', 0),
(910206, 5, 9747, 'Firepit soak timer: line', '', '', '', 1.0, '', 0),
(910207, 5, 9748, 'Firepit soak timer: line end', '', '', '', 1.0, '', 0),
(910208, 5, 9749, 'Firepit heal: circle', '', '', '', 1.0, '', 0),
(910209, 5, 9750, 'Firepit heal: ring', '', '', '', 1.0, '', 0),
(910210, 5, 9751, 'Firepit heal: 90 degree cone', '', '', '', 1.0, '', 0),
(910211, 5, 9752, 'Firepit heal: half (10 yards)', '', '', '', 1.0, '', 0),
(910212, 5, 9753, 'Firepit heal: half (arena)', '', '', '', 1.0, '', 0),
(910213, 5, 9754, 'Firepit heal: third (arena)', '', '', '', 1.0, '', 0),
(910214, 5, 9755, 'Firepit heal: line', '', '', '', 1.0, '', 0),
(910215, 5, 9756, 'Firepit heal: line end', '', '', '', 1.0, '', 0),
(910216, 5, 9757, 'Firepit heal timer: circle', '', '', '', 1.0, '', 0),
(910217, 5, 9758, 'Firepit heal timer: ring', '', '', '', 1.0, '', 0),
(910218, 5, 9759, 'Firepit heal timer: 90 degree cone', '', '', '', 1.0, '', 0),
(910219, 5, 9760, 'Firepit heal timer: half (10 yards)', '', '', '', 1.0, '', 0),
(910220, 5, 9761, 'Firepit heal timer: half (arena)', '', '', '', 1.0, '', 0),
(910221, 5, 9762, 'Firepit heal timer: third (arena)', '', '', '', 1.0, '', 0),
(910222, 5, 9763, 'Firepit heal timer: line', '', '', '', 1.0, '', 0),
(910223, 5, 9764, 'Firepit heal timer: line end', '', '', '', 1.0, '', 0),
(910224, 5, 9765, 'Firepit arcane: circle', '', '', '', 1.0, '', 0),
(910225, 5, 9766, 'Firepit arcane: ring', '', '', '', 1.0, '', 0),
(910226, 5, 9767, 'Firepit arcane: 90 degree cone', '', '', '', 1.0, '', 0),
(910227, 5, 9768, 'Firepit arcane: half (10 yards)', '', '', '', 1.0, '', 0),
(910228, 5, 9769, 'Firepit arcane: half (arena)', '', '', '', 1.0, '', 0),
(910229, 5, 9770, 'Firepit arcane: third (arena)', '', '', '', 1.0, '', 0),
(910230, 5, 9771, 'Firepit arcane: line', '', '', '', 1.0, '', 0),
(910231, 5, 9772, 'Firepit arcane: line end', '', '', '', 1.0, '', 0),
(910232, 5, 9773, 'Firepit arcane timer: circle', '', '', '', 1.0, '', 0),
(910233, 5, 9774, 'Firepit arcane timer: ring', '', '', '', 1.0, '', 0),
(910234, 5, 9775, 'Firepit arcane timer: 90 degree cone', '', '', '', 1.0, '', 0),
(910235, 5, 9776, 'Firepit arcane timer: half (10 yards)', '', '', '', 1.0, '', 0),
(910236, 5, 9777, 'Firepit arcane timer: half (arena)', '', '', '', 1.0, '', 0),
(910237, 5, 9778, 'Firepit arcane timer: third (arena)', '', '', '', 1.0, '', 0),
(910238, 5, 9779, 'Firepit arcane timer: line', '', '', '', 1.0, '', 0),
(910239, 5, 9780, 'Firepit arcane timer: line end', '', '', '', 1.0, '', 0),
(910240, 5, 9781, 'Firepit frost: circle', '', '', '', 1.0, '', 0),
(910241, 5, 9782, 'Firepit frost: ring', '', '', '', 1.0, '', 0),
(910242, 5, 9783, 'Firepit frost: 90 degree cone', '', '', '', 1.0, '', 0),
(910243, 5, 9784, 'Firepit frost: half (10 yards)', '', '', '', 1.0, '', 0),
(910244, 5, 9785, 'Firepit frost: half (arena)', '', '', '', 1.0, '', 0),
(910245, 5, 9786, 'Firepit frost: third (arena)', '', '', '', 1.0, '', 0),
(910246, 5, 9787, 'Firepit frost: line', '', '', '', 1.0, '', 0),
(910247, 5, 9788, 'Firepit frost: line end', '', '', '', 1.0, '', 0),
(910248, 5, 9789, 'Firepit frost timer: circle', '', '', '', 1.0, '', 0),
(910249, 5, 9790, 'Firepit frost timer: ring', '', '', '', 1.0, '', 0),
(910250, 5, 9791, 'Firepit frost timer: 90 degree cone', '', '', '', 1.0, '', 0),
(910251, 5, 9792, 'Firepit frost timer: half (10 yards)', '', '', '', 1.0, '', 0),
(910252, 5, 9793, 'Firepit frost timer: half (arena)', '', '', '', 1.0, '', 0),
(910253, 5, 9794, 'Firepit frost timer: third (arena)', '', '', '', 1.0, '', 0),
(910254, 5, 9795, 'Firepit frost timer: line', '', '', '', 1.0, '', 0),
(910255, 5, 9796, 'Firepit frost timer: line end', '', '', '', 1.0, '', 0);

DELETE FROM `gameobject_template_addon` WHERE `entry` BETWEEN 910160 AND 910255;
INSERT INTO `gameobject_template_addon` (`entry`, `faction`, `flags`, `mingold`, `maxgold`) VALUES
(910160, 0, 16, 0, 0), (910161, 0, 16, 0, 0), (910162, 0, 16, 0, 0), (910163, 0, 16, 0, 0),
(910164, 0, 16, 0, 0), (910165, 0, 16, 0, 0), (910166, 0, 16, 0, 0), (910167, 0, 16, 0, 0),
(910168, 0, 16, 0, 0), (910169, 0, 16, 0, 0), (910170, 0, 16, 0, 0), (910171, 0, 16, 0, 0),
(910172, 0, 16, 0, 0), (910173, 0, 16, 0, 0), (910174, 0, 16, 0, 0), (910175, 0, 16, 0, 0),
(910176, 0, 16, 0, 0), (910177, 0, 16, 0, 0), (910178, 0, 16, 0, 0), (910179, 0, 16, 0, 0),
(910180, 0, 16, 0, 0), (910181, 0, 16, 0, 0), (910182, 0, 16, 0, 0), (910183, 0, 16, 0, 0),
(910184, 0, 16, 0, 0), (910185, 0, 16, 0, 0), (910186, 0, 16, 0, 0), (910187, 0, 16, 0, 0),
(910188, 0, 16, 0, 0), (910189, 0, 16, 0, 0), (910190, 0, 16, 0, 0), (910191, 0, 16, 0, 0),
(910192, 0, 16, 0, 0), (910193, 0, 16, 0, 0), (910194, 0, 16, 0, 0), (910195, 0, 16, 0, 0),
(910196, 0, 16, 0, 0), (910197, 0, 16, 0, 0), (910198, 0, 16, 0, 0), (910199, 0, 16, 0, 0),
(910200, 0, 16, 0, 0), (910201, 0, 16, 0, 0), (910202, 0, 16, 0, 0), (910203, 0, 16, 0, 0),
(910204, 0, 16, 0, 0), (910205, 0, 16, 0, 0), (910206, 0, 16, 0, 0), (910207, 0, 16, 0, 0),
(910208, 0, 16, 0, 0), (910209, 0, 16, 0, 0), (910210, 0, 16, 0, 0), (910211, 0, 16, 0, 0),
(910212, 0, 16, 0, 0), (910213, 0, 16, 0, 0), (910214, 0, 16, 0, 0), (910215, 0, 16, 0, 0),
(910216, 0, 16, 0, 0), (910217, 0, 16, 0, 0), (910218, 0, 16, 0, 0), (910219, 0, 16, 0, 0),
(910220, 0, 16, 0, 0), (910221, 0, 16, 0, 0), (910222, 0, 16, 0, 0), (910223, 0, 16, 0, 0),
(910224, 0, 16, 0, 0), (910225, 0, 16, 0, 0), (910226, 0, 16, 0, 0), (910227, 0, 16, 0, 0),
(910228, 0, 16, 0, 0), (910229, 0, 16, 0, 0), (910230, 0, 16, 0, 0), (910231, 0, 16, 0, 0),
(910232, 0, 16, 0, 0), (910233, 0, 16, 0, 0), (910234, 0, 16, 0, 0), (910235, 0, 16, 0, 0),
(910236, 0, 16, 0, 0), (910237, 0, 16, 0, 0), (910238, 0, 16, 0, 0), (910239, 0, 16, 0, 0),
(910240, 0, 16, 0, 0), (910241, 0, 16, 0, 0), (910242, 0, 16, 0, 0), (910243, 0, 16, 0, 0),
(910244, 0, 16, 0, 0), (910245, 0, 16, 0, 0), (910246, 0, 16, 0, 0), (910247, 0, 16, 0, 0),
(910248, 0, 16, 0, 0), (910249, 0, 16, 0, 0), (910250, 0, 16, 0, 0), (910251, 0, 16, 0, 0),
(910252, 0, 16, 0, 0), (910253, 0, 16, 0, 0), (910254, 0, 16, 0, 0), (910255, 0, 16, 0, 0);

-- The area decals' textured overlays (runic, cracked, hazard, swirl), 48 a style from 910256 (displays 9797 on), 8 a
-- colour: laid over a decal's fill, unselectable.
DELETE FROM `gameobject_template` WHERE `entry` BETWEEN 910256 AND 910447;
INSERT INTO `gameobject_template` (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`,
    `size`, `ScriptName`, `VerifiedBuild`) VALUES
(910256, 5, 9797, 'Firepit danger runic: circle', '', '', '', 1.0, '', 0),
(910257, 5, 9798, 'Firepit danger runic: ring', '', '', '', 1.0, '', 0),
(910258, 5, 9799, 'Firepit danger runic: 90 degree cone', '', '', '', 1.0, '', 0),
(910259, 5, 9800, 'Firepit danger runic: half (10 yards)', '', '', '', 1.0, '', 0),
(910260, 5, 9801, 'Firepit danger runic: half (arena)', '', '', '', 1.0, '', 0),
(910261, 5, 9802, 'Firepit danger runic: third (arena)', '', '', '', 1.0, '', 0),
(910262, 5, 9803, 'Firepit danger runic: line', '', '', '', 1.0, '', 0),
(910263, 5, 9804, 'Firepit danger runic: line end', '', '', '', 1.0, '', 0),
(910264, 5, 9805, 'Firepit safe runic: circle', '', '', '', 1.0, '', 0),
(910265, 5, 9806, 'Firepit safe runic: ring', '', '', '', 1.0, '', 0),
(910266, 5, 9807, 'Firepit safe runic: 90 degree cone', '', '', '', 1.0, '', 0),
(910267, 5, 9808, 'Firepit safe runic: half (10 yards)', '', '', '', 1.0, '', 0),
(910268, 5, 9809, 'Firepit safe runic: half (arena)', '', '', '', 1.0, '', 0),
(910269, 5, 9810, 'Firepit safe runic: third (arena)', '', '', '', 1.0, '', 0),
(910270, 5, 9811, 'Firepit safe runic: line', '', '', '', 1.0, '', 0),
(910271, 5, 9812, 'Firepit safe runic: line end', '', '', '', 1.0, '', 0),
(910272, 5, 9813, 'Firepit soak runic: circle', '', '', '', 1.0, '', 0),
(910273, 5, 9814, 'Firepit soak runic: ring', '', '', '', 1.0, '', 0),
(910274, 5, 9815, 'Firepit soak runic: 90 degree cone', '', '', '', 1.0, '', 0),
(910275, 5, 9816, 'Firepit soak runic: half (10 yards)', '', '', '', 1.0, '', 0),
(910276, 5, 9817, 'Firepit soak runic: half (arena)', '', '', '', 1.0, '', 0),
(910277, 5, 9818, 'Firepit soak runic: third (arena)', '', '', '', 1.0, '', 0),
(910278, 5, 9819, 'Firepit soak runic: line', '', '', '', 1.0, '', 0),
(910279, 5, 9820, 'Firepit soak runic: line end', '', '', '', 1.0, '', 0),
(910280, 5, 9821, 'Firepit heal runic: circle', '', '', '', 1.0, '', 0),
(910281, 5, 9822, 'Firepit heal runic: ring', '', '', '', 1.0, '', 0),
(910282, 5, 9823, 'Firepit heal runic: 90 degree cone', '', '', '', 1.0, '', 0),
(910283, 5, 9824, 'Firepit heal runic: half (10 yards)', '', '', '', 1.0, '', 0),
(910284, 5, 9825, 'Firepit heal runic: half (arena)', '', '', '', 1.0, '', 0),
(910285, 5, 9826, 'Firepit heal runic: third (arena)', '', '', '', 1.0, '', 0),
(910286, 5, 9827, 'Firepit heal runic: line', '', '', '', 1.0, '', 0),
(910287, 5, 9828, 'Firepit heal runic: line end', '', '', '', 1.0, '', 0),
(910288, 5, 9829, 'Firepit arcane runic: circle', '', '', '', 1.0, '', 0),
(910289, 5, 9830, 'Firepit arcane runic: ring', '', '', '', 1.0, '', 0),
(910290, 5, 9831, 'Firepit arcane runic: 90 degree cone', '', '', '', 1.0, '', 0),
(910291, 5, 9832, 'Firepit arcane runic: half (10 yards)', '', '', '', 1.0, '', 0),
(910292, 5, 9833, 'Firepit arcane runic: half (arena)', '', '', '', 1.0, '', 0),
(910293, 5, 9834, 'Firepit arcane runic: third (arena)', '', '', '', 1.0, '', 0),
(910294, 5, 9835, 'Firepit arcane runic: line', '', '', '', 1.0, '', 0),
(910295, 5, 9836, 'Firepit arcane runic: line end', '', '', '', 1.0, '', 0),
(910296, 5, 9837, 'Firepit frost runic: circle', '', '', '', 1.0, '', 0),
(910297, 5, 9838, 'Firepit frost runic: ring', '', '', '', 1.0, '', 0),
(910298, 5, 9839, 'Firepit frost runic: 90 degree cone', '', '', '', 1.0, '', 0),
(910299, 5, 9840, 'Firepit frost runic: half (10 yards)', '', '', '', 1.0, '', 0),
(910300, 5, 9841, 'Firepit frost runic: half (arena)', '', '', '', 1.0, '', 0),
(910301, 5, 9842, 'Firepit frost runic: third (arena)', '', '', '', 1.0, '', 0),
(910302, 5, 9843, 'Firepit frost runic: line', '', '', '', 1.0, '', 0),
(910303, 5, 9844, 'Firepit frost runic: line end', '', '', '', 1.0, '', 0),
(910304, 5, 9845, 'Firepit danger cracked: circle', '', '', '', 1.0, '', 0),
(910305, 5, 9846, 'Firepit danger cracked: ring', '', '', '', 1.0, '', 0),
(910306, 5, 9847, 'Firepit danger cracked: 90 degree cone', '', '', '', 1.0, '', 0),
(910307, 5, 9848, 'Firepit danger cracked: half (10 yards)', '', '', '', 1.0, '', 0),
(910308, 5, 9849, 'Firepit danger cracked: half (arena)', '', '', '', 1.0, '', 0),
(910309, 5, 9850, 'Firepit danger cracked: third (arena)', '', '', '', 1.0, '', 0),
(910310, 5, 9851, 'Firepit danger cracked: line', '', '', '', 1.0, '', 0),
(910311, 5, 9852, 'Firepit danger cracked: line end', '', '', '', 1.0, '', 0),
(910312, 5, 9853, 'Firepit safe cracked: circle', '', '', '', 1.0, '', 0),
(910313, 5, 9854, 'Firepit safe cracked: ring', '', '', '', 1.0, '', 0),
(910314, 5, 9855, 'Firepit safe cracked: 90 degree cone', '', '', '', 1.0, '', 0),
(910315, 5, 9856, 'Firepit safe cracked: half (10 yards)', '', '', '', 1.0, '', 0),
(910316, 5, 9857, 'Firepit safe cracked: half (arena)', '', '', '', 1.0, '', 0),
(910317, 5, 9858, 'Firepit safe cracked: third (arena)', '', '', '', 1.0, '', 0),
(910318, 5, 9859, 'Firepit safe cracked: line', '', '', '', 1.0, '', 0),
(910319, 5, 9860, 'Firepit safe cracked: line end', '', '', '', 1.0, '', 0),
(910320, 5, 9861, 'Firepit soak cracked: circle', '', '', '', 1.0, '', 0),
(910321, 5, 9862, 'Firepit soak cracked: ring', '', '', '', 1.0, '', 0),
(910322, 5, 9863, 'Firepit soak cracked: 90 degree cone', '', '', '', 1.0, '', 0),
(910323, 5, 9864, 'Firepit soak cracked: half (10 yards)', '', '', '', 1.0, '', 0),
(910324, 5, 9865, 'Firepit soak cracked: half (arena)', '', '', '', 1.0, '', 0),
(910325, 5, 9866, 'Firepit soak cracked: third (arena)', '', '', '', 1.0, '', 0),
(910326, 5, 9867, 'Firepit soak cracked: line', '', '', '', 1.0, '', 0),
(910327, 5, 9868, 'Firepit soak cracked: line end', '', '', '', 1.0, '', 0),
(910328, 5, 9869, 'Firepit heal cracked: circle', '', '', '', 1.0, '', 0),
(910329, 5, 9870, 'Firepit heal cracked: ring', '', '', '', 1.0, '', 0),
(910330, 5, 9871, 'Firepit heal cracked: 90 degree cone', '', '', '', 1.0, '', 0),
(910331, 5, 9872, 'Firepit heal cracked: half (10 yards)', '', '', '', 1.0, '', 0),
(910332, 5, 9873, 'Firepit heal cracked: half (arena)', '', '', '', 1.0, '', 0),
(910333, 5, 9874, 'Firepit heal cracked: third (arena)', '', '', '', 1.0, '', 0),
(910334, 5, 9875, 'Firepit heal cracked: line', '', '', '', 1.0, '', 0),
(910335, 5, 9876, 'Firepit heal cracked: line end', '', '', '', 1.0, '', 0),
(910336, 5, 9877, 'Firepit arcane cracked: circle', '', '', '', 1.0, '', 0),
(910337, 5, 9878, 'Firepit arcane cracked: ring', '', '', '', 1.0, '', 0),
(910338, 5, 9879, 'Firepit arcane cracked: 90 degree cone', '', '', '', 1.0, '', 0),
(910339, 5, 9880, 'Firepit arcane cracked: half (10 yards)', '', '', '', 1.0, '', 0),
(910340, 5, 9881, 'Firepit arcane cracked: half (arena)', '', '', '', 1.0, '', 0),
(910341, 5, 9882, 'Firepit arcane cracked: third (arena)', '', '', '', 1.0, '', 0),
(910342, 5, 9883, 'Firepit arcane cracked: line', '', '', '', 1.0, '', 0),
(910343, 5, 9884, 'Firepit arcane cracked: line end', '', '', '', 1.0, '', 0),
(910344, 5, 9885, 'Firepit frost cracked: circle', '', '', '', 1.0, '', 0),
(910345, 5, 9886, 'Firepit frost cracked: ring', '', '', '', 1.0, '', 0),
(910346, 5, 9887, 'Firepit frost cracked: 90 degree cone', '', '', '', 1.0, '', 0),
(910347, 5, 9888, 'Firepit frost cracked: half (10 yards)', '', '', '', 1.0, '', 0),
(910348, 5, 9889, 'Firepit frost cracked: half (arena)', '', '', '', 1.0, '', 0),
(910349, 5, 9890, 'Firepit frost cracked: third (arena)', '', '', '', 1.0, '', 0),
(910350, 5, 9891, 'Firepit frost cracked: line', '', '', '', 1.0, '', 0),
(910351, 5, 9892, 'Firepit frost cracked: line end', '', '', '', 1.0, '', 0),
(910352, 5, 9893, 'Firepit danger hazard: circle', '', '', '', 1.0, '', 0),
(910353, 5, 9894, 'Firepit danger hazard: ring', '', '', '', 1.0, '', 0),
(910354, 5, 9895, 'Firepit danger hazard: 90 degree cone', '', '', '', 1.0, '', 0),
(910355, 5, 9896, 'Firepit danger hazard: half (10 yards)', '', '', '', 1.0, '', 0),
(910356, 5, 9897, 'Firepit danger hazard: half (arena)', '', '', '', 1.0, '', 0),
(910357, 5, 9898, 'Firepit danger hazard: third (arena)', '', '', '', 1.0, '', 0),
(910358, 5, 9899, 'Firepit danger hazard: line', '', '', '', 1.0, '', 0),
(910359, 5, 9900, 'Firepit danger hazard: line end', '', '', '', 1.0, '', 0),
(910360, 5, 9901, 'Firepit safe hazard: circle', '', '', '', 1.0, '', 0),
(910361, 5, 9902, 'Firepit safe hazard: ring', '', '', '', 1.0, '', 0),
(910362, 5, 9903, 'Firepit safe hazard: 90 degree cone', '', '', '', 1.0, '', 0),
(910363, 5, 9904, 'Firepit safe hazard: half (10 yards)', '', '', '', 1.0, '', 0),
(910364, 5, 9905, 'Firepit safe hazard: half (arena)', '', '', '', 1.0, '', 0),
(910365, 5, 9906, 'Firepit safe hazard: third (arena)', '', '', '', 1.0, '', 0),
(910366, 5, 9907, 'Firepit safe hazard: line', '', '', '', 1.0, '', 0),
(910367, 5, 9908, 'Firepit safe hazard: line end', '', '', '', 1.0, '', 0),
(910368, 5, 9909, 'Firepit soak hazard: circle', '', '', '', 1.0, '', 0),
(910369, 5, 9910, 'Firepit soak hazard: ring', '', '', '', 1.0, '', 0),
(910370, 5, 9911, 'Firepit soak hazard: 90 degree cone', '', '', '', 1.0, '', 0),
(910371, 5, 9912, 'Firepit soak hazard: half (10 yards)', '', '', '', 1.0, '', 0),
(910372, 5, 9913, 'Firepit soak hazard: half (arena)', '', '', '', 1.0, '', 0),
(910373, 5, 9914, 'Firepit soak hazard: third (arena)', '', '', '', 1.0, '', 0),
(910374, 5, 9915, 'Firepit soak hazard: line', '', '', '', 1.0, '', 0),
(910375, 5, 9916, 'Firepit soak hazard: line end', '', '', '', 1.0, '', 0),
(910376, 5, 9917, 'Firepit heal hazard: circle', '', '', '', 1.0, '', 0),
(910377, 5, 9918, 'Firepit heal hazard: ring', '', '', '', 1.0, '', 0),
(910378, 5, 9919, 'Firepit heal hazard: 90 degree cone', '', '', '', 1.0, '', 0),
(910379, 5, 9920, 'Firepit heal hazard: half (10 yards)', '', '', '', 1.0, '', 0),
(910380, 5, 9921, 'Firepit heal hazard: half (arena)', '', '', '', 1.0, '', 0),
(910381, 5, 9922, 'Firepit heal hazard: third (arena)', '', '', '', 1.0, '', 0),
(910382, 5, 9923, 'Firepit heal hazard: line', '', '', '', 1.0, '', 0),
(910383, 5, 9924, 'Firepit heal hazard: line end', '', '', '', 1.0, '', 0),
(910384, 5, 9925, 'Firepit arcane hazard: circle', '', '', '', 1.0, '', 0),
(910385, 5, 9926, 'Firepit arcane hazard: ring', '', '', '', 1.0, '', 0),
(910386, 5, 9927, 'Firepit arcane hazard: 90 degree cone', '', '', '', 1.0, '', 0),
(910387, 5, 9928, 'Firepit arcane hazard: half (10 yards)', '', '', '', 1.0, '', 0),
(910388, 5, 9929, 'Firepit arcane hazard: half (arena)', '', '', '', 1.0, '', 0),
(910389, 5, 9930, 'Firepit arcane hazard: third (arena)', '', '', '', 1.0, '', 0),
(910390, 5, 9931, 'Firepit arcane hazard: line', '', '', '', 1.0, '', 0),
(910391, 5, 9932, 'Firepit arcane hazard: line end', '', '', '', 1.0, '', 0),
(910392, 5, 9933, 'Firepit frost hazard: circle', '', '', '', 1.0, '', 0),
(910393, 5, 9934, 'Firepit frost hazard: ring', '', '', '', 1.0, '', 0),
(910394, 5, 9935, 'Firepit frost hazard: 90 degree cone', '', '', '', 1.0, '', 0),
(910395, 5, 9936, 'Firepit frost hazard: half (10 yards)', '', '', '', 1.0, '', 0),
(910396, 5, 9937, 'Firepit frost hazard: half (arena)', '', '', '', 1.0, '', 0),
(910397, 5, 9938, 'Firepit frost hazard: third (arena)', '', '', '', 1.0, '', 0),
(910398, 5, 9939, 'Firepit frost hazard: line', '', '', '', 1.0, '', 0),
(910399, 5, 9940, 'Firepit frost hazard: line end', '', '', '', 1.0, '', 0),
(910400, 5, 9941, 'Firepit danger swirl: circle', '', '', '', 1.0, '', 0),
(910401, 5, 9942, 'Firepit danger swirl: ring', '', '', '', 1.0, '', 0),
(910402, 5, 9943, 'Firepit danger swirl: 90 degree cone', '', '', '', 1.0, '', 0),
(910403, 5, 9944, 'Firepit danger swirl: half (10 yards)', '', '', '', 1.0, '', 0),
(910404, 5, 9945, 'Firepit danger swirl: half (arena)', '', '', '', 1.0, '', 0),
(910405, 5, 9946, 'Firepit danger swirl: third (arena)', '', '', '', 1.0, '', 0),
(910406, 5, 9947, 'Firepit danger swirl: line', '', '', '', 1.0, '', 0),
(910407, 5, 9948, 'Firepit danger swirl: line end', '', '', '', 1.0, '', 0),
(910408, 5, 9949, 'Firepit safe swirl: circle', '', '', '', 1.0, '', 0),
(910409, 5, 9950, 'Firepit safe swirl: ring', '', '', '', 1.0, '', 0),
(910410, 5, 9951, 'Firepit safe swirl: 90 degree cone', '', '', '', 1.0, '', 0),
(910411, 5, 9952, 'Firepit safe swirl: half (10 yards)', '', '', '', 1.0, '', 0),
(910412, 5, 9953, 'Firepit safe swirl: half (arena)', '', '', '', 1.0, '', 0),
(910413, 5, 9954, 'Firepit safe swirl: third (arena)', '', '', '', 1.0, '', 0),
(910414, 5, 9955, 'Firepit safe swirl: line', '', '', '', 1.0, '', 0),
(910415, 5, 9956, 'Firepit safe swirl: line end', '', '', '', 1.0, '', 0),
(910416, 5, 9957, 'Firepit soak swirl: circle', '', '', '', 1.0, '', 0),
(910417, 5, 9958, 'Firepit soak swirl: ring', '', '', '', 1.0, '', 0),
(910418, 5, 9959, 'Firepit soak swirl: 90 degree cone', '', '', '', 1.0, '', 0),
(910419, 5, 9960, 'Firepit soak swirl: half (10 yards)', '', '', '', 1.0, '', 0),
(910420, 5, 9961, 'Firepit soak swirl: half (arena)', '', '', '', 1.0, '', 0),
(910421, 5, 9962, 'Firepit soak swirl: third (arena)', '', '', '', 1.0, '', 0),
(910422, 5, 9963, 'Firepit soak swirl: line', '', '', '', 1.0, '', 0),
(910423, 5, 9964, 'Firepit soak swirl: line end', '', '', '', 1.0, '', 0),
(910424, 5, 9965, 'Firepit heal swirl: circle', '', '', '', 1.0, '', 0),
(910425, 5, 9966, 'Firepit heal swirl: ring', '', '', '', 1.0, '', 0),
(910426, 5, 9967, 'Firepit heal swirl: 90 degree cone', '', '', '', 1.0, '', 0),
(910427, 5, 9968, 'Firepit heal swirl: half (10 yards)', '', '', '', 1.0, '', 0),
(910428, 5, 9969, 'Firepit heal swirl: half (arena)', '', '', '', 1.0, '', 0),
(910429, 5, 9970, 'Firepit heal swirl: third (arena)', '', '', '', 1.0, '', 0),
(910430, 5, 9971, 'Firepit heal swirl: line', '', '', '', 1.0, '', 0),
(910431, 5, 9972, 'Firepit heal swirl: line end', '', '', '', 1.0, '', 0),
(910432, 5, 9973, 'Firepit arcane swirl: circle', '', '', '', 1.0, '', 0),
(910433, 5, 9974, 'Firepit arcane swirl: ring', '', '', '', 1.0, '', 0),
(910434, 5, 9975, 'Firepit arcane swirl: 90 degree cone', '', '', '', 1.0, '', 0),
(910435, 5, 9976, 'Firepit arcane swirl: half (10 yards)', '', '', '', 1.0, '', 0),
(910436, 5, 9977, 'Firepit arcane swirl: half (arena)', '', '', '', 1.0, '', 0),
(910437, 5, 9978, 'Firepit arcane swirl: third (arena)', '', '', '', 1.0, '', 0),
(910438, 5, 9979, 'Firepit arcane swirl: line', '', '', '', 1.0, '', 0),
(910439, 5, 9980, 'Firepit arcane swirl: line end', '', '', '', 1.0, '', 0),
(910440, 5, 9981, 'Firepit frost swirl: circle', '', '', '', 1.0, '', 0),
(910441, 5, 9982, 'Firepit frost swirl: ring', '', '', '', 1.0, '', 0),
(910442, 5, 9983, 'Firepit frost swirl: 90 degree cone', '', '', '', 1.0, '', 0),
(910443, 5, 9984, 'Firepit frost swirl: half (10 yards)', '', '', '', 1.0, '', 0),
(910444, 5, 9985, 'Firepit frost swirl: half (arena)', '', '', '', 1.0, '', 0),
(910445, 5, 9986, 'Firepit frost swirl: third (arena)', '', '', '', 1.0, '', 0),
(910446, 5, 9987, 'Firepit frost swirl: line', '', '', '', 1.0, '', 0),
(910447, 5, 9988, 'Firepit frost swirl: line end', '', '', '', 1.0, '', 0);

DELETE FROM `gameobject_template_addon` WHERE `entry` BETWEEN 910256 AND 910447;
INSERT INTO `gameobject_template_addon` (`entry`, `faction`, `flags`, `mingold`, `maxgold`) VALUES
(910256, 0, 16, 0, 0), (910257, 0, 16, 0, 0), (910258, 0, 16, 0, 0), (910259, 0, 16, 0, 0),
(910260, 0, 16, 0, 0), (910261, 0, 16, 0, 0), (910262, 0, 16, 0, 0), (910263, 0, 16, 0, 0),
(910264, 0, 16, 0, 0), (910265, 0, 16, 0, 0), (910266, 0, 16, 0, 0), (910267, 0, 16, 0, 0),
(910268, 0, 16, 0, 0), (910269, 0, 16, 0, 0), (910270, 0, 16, 0, 0), (910271, 0, 16, 0, 0),
(910272, 0, 16, 0, 0), (910273, 0, 16, 0, 0), (910274, 0, 16, 0, 0), (910275, 0, 16, 0, 0),
(910276, 0, 16, 0, 0), (910277, 0, 16, 0, 0), (910278, 0, 16, 0, 0), (910279, 0, 16, 0, 0),
(910280, 0, 16, 0, 0), (910281, 0, 16, 0, 0), (910282, 0, 16, 0, 0), (910283, 0, 16, 0, 0),
(910284, 0, 16, 0, 0), (910285, 0, 16, 0, 0), (910286, 0, 16, 0, 0), (910287, 0, 16, 0, 0),
(910288, 0, 16, 0, 0), (910289, 0, 16, 0, 0), (910290, 0, 16, 0, 0), (910291, 0, 16, 0, 0),
(910292, 0, 16, 0, 0), (910293, 0, 16, 0, 0), (910294, 0, 16, 0, 0), (910295, 0, 16, 0, 0),
(910296, 0, 16, 0, 0), (910297, 0, 16, 0, 0), (910298, 0, 16, 0, 0), (910299, 0, 16, 0, 0),
(910300, 0, 16, 0, 0), (910301, 0, 16, 0, 0), (910302, 0, 16, 0, 0), (910303, 0, 16, 0, 0),
(910304, 0, 16, 0, 0), (910305, 0, 16, 0, 0), (910306, 0, 16, 0, 0), (910307, 0, 16, 0, 0),
(910308, 0, 16, 0, 0), (910309, 0, 16, 0, 0), (910310, 0, 16, 0, 0), (910311, 0, 16, 0, 0),
(910312, 0, 16, 0, 0), (910313, 0, 16, 0, 0), (910314, 0, 16, 0, 0), (910315, 0, 16, 0, 0),
(910316, 0, 16, 0, 0), (910317, 0, 16, 0, 0), (910318, 0, 16, 0, 0), (910319, 0, 16, 0, 0),
(910320, 0, 16, 0, 0), (910321, 0, 16, 0, 0), (910322, 0, 16, 0, 0), (910323, 0, 16, 0, 0),
(910324, 0, 16, 0, 0), (910325, 0, 16, 0, 0), (910326, 0, 16, 0, 0), (910327, 0, 16, 0, 0),
(910328, 0, 16, 0, 0), (910329, 0, 16, 0, 0), (910330, 0, 16, 0, 0), (910331, 0, 16, 0, 0),
(910332, 0, 16, 0, 0), (910333, 0, 16, 0, 0), (910334, 0, 16, 0, 0), (910335, 0, 16, 0, 0),
(910336, 0, 16, 0, 0), (910337, 0, 16, 0, 0), (910338, 0, 16, 0, 0), (910339, 0, 16, 0, 0),
(910340, 0, 16, 0, 0), (910341, 0, 16, 0, 0), (910342, 0, 16, 0, 0), (910343, 0, 16, 0, 0),
(910344, 0, 16, 0, 0), (910345, 0, 16, 0, 0), (910346, 0, 16, 0, 0), (910347, 0, 16, 0, 0),
(910348, 0, 16, 0, 0), (910349, 0, 16, 0, 0), (910350, 0, 16, 0, 0), (910351, 0, 16, 0, 0),
(910352, 0, 16, 0, 0), (910353, 0, 16, 0, 0), (910354, 0, 16, 0, 0), (910355, 0, 16, 0, 0),
(910356, 0, 16, 0, 0), (910357, 0, 16, 0, 0), (910358, 0, 16, 0, 0), (910359, 0, 16, 0, 0),
(910360, 0, 16, 0, 0), (910361, 0, 16, 0, 0), (910362, 0, 16, 0, 0), (910363, 0, 16, 0, 0),
(910364, 0, 16, 0, 0), (910365, 0, 16, 0, 0), (910366, 0, 16, 0, 0), (910367, 0, 16, 0, 0),
(910368, 0, 16, 0, 0), (910369, 0, 16, 0, 0), (910370, 0, 16, 0, 0), (910371, 0, 16, 0, 0),
(910372, 0, 16, 0, 0), (910373, 0, 16, 0, 0), (910374, 0, 16, 0, 0), (910375, 0, 16, 0, 0),
(910376, 0, 16, 0, 0), (910377, 0, 16, 0, 0), (910378, 0, 16, 0, 0), (910379, 0, 16, 0, 0),
(910380, 0, 16, 0, 0), (910381, 0, 16, 0, 0), (910382, 0, 16, 0, 0), (910383, 0, 16, 0, 0),
(910384, 0, 16, 0, 0), (910385, 0, 16, 0, 0), (910386, 0, 16, 0, 0), (910387, 0, 16, 0, 0),
(910388, 0, 16, 0, 0), (910389, 0, 16, 0, 0), (910390, 0, 16, 0, 0), (910391, 0, 16, 0, 0),
(910392, 0, 16, 0, 0), (910393, 0, 16, 0, 0), (910394, 0, 16, 0, 0), (910395, 0, 16, 0, 0),
(910396, 0, 16, 0, 0), (910397, 0, 16, 0, 0), (910398, 0, 16, 0, 0), (910399, 0, 16, 0, 0),
(910400, 0, 16, 0, 0), (910401, 0, 16, 0, 0), (910402, 0, 16, 0, 0), (910403, 0, 16, 0, 0),
(910404, 0, 16, 0, 0), (910405, 0, 16, 0, 0), (910406, 0, 16, 0, 0), (910407, 0, 16, 0, 0),
(910408, 0, 16, 0, 0), (910409, 0, 16, 0, 0), (910410, 0, 16, 0, 0), (910411, 0, 16, 0, 0),
(910412, 0, 16, 0, 0), (910413, 0, 16, 0, 0), (910414, 0, 16, 0, 0), (910415, 0, 16, 0, 0),
(910416, 0, 16, 0, 0), (910417, 0, 16, 0, 0), (910418, 0, 16, 0, 0), (910419, 0, 16, 0, 0),
(910420, 0, 16, 0, 0), (910421, 0, 16, 0, 0), (910422, 0, 16, 0, 0), (910423, 0, 16, 0, 0),
(910424, 0, 16, 0, 0), (910425, 0, 16, 0, 0), (910426, 0, 16, 0, 0), (910427, 0, 16, 0, 0),
(910428, 0, 16, 0, 0), (910429, 0, 16, 0, 0), (910430, 0, 16, 0, 0), (910431, 0, 16, 0, 0),
(910432, 0, 16, 0, 0), (910433, 0, 16, 0, 0), (910434, 0, 16, 0, 0), (910435, 0, 16, 0, 0),
(910436, 0, 16, 0, 0), (910437, 0, 16, 0, 0), (910438, 0, 16, 0, 0), (910439, 0, 16, 0, 0),
(910440, 0, 16, 0, 0), (910441, 0, 16, 0, 0), (910442, 0, 16, 0, 0), (910443, 0, 16, 0, 0),
(910444, 0, 16, 0, 0), (910445, 0, 16, 0, 0), (910446, 0, 16, 0, 0), (910447, 0, 16, 0, 0);

-- The Ashfleet Captain's dressing (src/firepit.cpp RunOutTheGuns): a pirate cannon and a fused powder keg, run out
-- round the rim for his fight; unselectable.
DELETE FROM `gameobject_template` WHERE `entry` IN (910448, 910449);
INSERT INTO `gameobject_template` (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`,
    `size`, `ScriptName`, `VerifiedBuild`) VALUES
(910448, 5, 245, 'Ashfleet Cannon', '', '', '', 1.6, '', 0),
(910449, 5, 112, 'Ashfleet Powder Keg', '', '', '', 1.4, '', 0);

DELETE FROM `gameobject_template_addon` WHERE `entry` IN (910448, 910449);
INSERT INTO `gameobject_template_addon` (`entry`, `faction`, `flags`, `mingold`, `maxgold`) VALUES
(910448, 0, 16, 0, 0), (910449, 0, 16, 0, 0);

-- The Firepit's map (760): Trial of the Champion's arena (map 650's Directory, so its terrain) under its own name and
-- sky (the client patch's Map.dbc row, client-patch/hd/make_atmosphere.py). Its terrain files are map 650's, copied by
-- the ac-firepit-map service (tools/firepit-map.sh). A dungeon, 5 Heroes by the DBC (the Firepit's own core hook lets
-- 10 in), entered only by the Firepit's teleport; a dead Hero's corpse goes to Northrend, as at Trial of the Champion.
SET @MAP := 760;
DELETE FROM `map_dbc` WHERE `ID` = @MAP;
INSERT INTO `map_dbc` (`ID`, `Directory`, `InstanceType`, `Flags`, `PVP`, `MapName_Lang_enUS`, `MapName_Lang_Mask`,
    `AreaTableID`, `LoadingScreenID`, `MinimapIconScale`, `CorpseMapID`, `CorpseX`, `CorpseY`, `TimeOfDayOverride`,
    `ExpansionID`, `RaidOffset`, `MaxPlayers`) VALUES
(@MAP, 'ArgentTournamentDungeon', 1, 29, 0, 'The Firepit', 16712190, 0, 255, 1, 571, 8571.43, 792.22, -1, 2, 0, 0);

DELETE FROM `mapdifficulty_dbc` WHERE `MapID` = @MAP;
INSERT INTO `mapdifficulty_dbc` (`ID`, `MapID`, `Difficulty`, `RaidDuration`, `MaxPlayers`, `Difficultystring`) VALUES
(754, @MAP, 0, 0, 5, 'DUNGEON_DIFFICULTY_5PLAYER'),
(755, @MAP, 1, 86400, 5, 'DUNGEON_DIFFICULTY_5PLAYER_HEROIC');

DELETE FROM `instance_template` WHERE `map` = @MAP;
INSERT INTO `instance_template` (`map`, `parent`, `script`, `allowMount`) VALUES
(@MAP, 0, '', 0);

-- The Fireforged bonus effects' equip spells (tools/scaling/firepit_effects.py, src/fireforged.cpp): their amounts
-- from their piece's item level.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_titan_fireforged_equip';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(93500, 'spell_titan_fireforged_equip'),
(93501, 'spell_titan_fireforged_equip'),
(93502, 'spell_titan_fireforged_equip'),
(93503, 'spell_titan_fireforged_equip'),
(93504, 'spell_titan_fireforged_equip'),
(93505, 'spell_titan_fireforged_equip'),
(93506, 'spell_titan_fireforged_equip'),
(93507, 'spell_titan_fireforged_equip'),
(93508, 'spell_titan_fireforged_equip'),
(93509, 'spell_titan_fireforged_equip'),
(93510, 'spell_titan_fireforged_equip'),
(93511, 'spell_titan_fireforged_equip'),
(93512, 'spell_titan_fireforged_equip'),
(93513, 'spell_titan_fireforged_equip'),
(93514, 'spell_titan_fireforged_equip'),
(93515, 'spell_titan_fireforged_equip'),
(93516, 'spell_titan_fireforged_equip'),
(93517, 'spell_titan_fireforged_equip'),
(93518, 'spell_titan_fireforged_equip'),
(93519, 'spell_titan_fireforged_equip'),
(93520, 'spell_titan_fireforged_equip'),
(93521, 'spell_titan_fireforged_equip'),
(93522, 'spell_titan_fireforged_equip'),
(93523, 'spell_titan_fireforged_equip'),
(93524, 'spell_titan_fireforged_equip'),
(93525, 'spell_titan_fireforged_equip'),
(93526, 'spell_titan_fireforged_equip'),
(93527, 'spell_titan_fireforged_equip'),
(93528, 'spell_titan_fireforged_equip'),
(93529, 'spell_titan_fireforged_equip'),
(93530, 'spell_titan_fireforged_equip'),
(93531, 'spell_titan_fireforged_equip'),
(93532, 'spell_titan_fireforged_equip'),
(93533, 'spell_titan_fireforged_equip'),
(93534, 'spell_titan_fireforged_equip'),
(93535, 'spell_titan_fireforged_equip'),
(93536, 'spell_titan_fireforged_equip'),
(93537, 'spell_titan_fireforged_equip'),
(93538, 'spell_titan_fireforged_equip'),
(93539, 'spell_titan_fireforged_equip'),
(93540, 'spell_titan_fireforged_equip'),
(93541, 'spell_titan_fireforged_equip'),
(93542, 'spell_titan_fireforged_equip'),
(93543, 'spell_titan_fireforged_equip'),
(93544, 'spell_titan_fireforged_equip'),
(93545, 'spell_titan_fireforged_equip'),
(93546, 'spell_titan_fireforged_equip'),
(93547, 'spell_titan_fireforged_equip'),
(93548, 'spell_titan_fireforged_equip'),
(93549, 'spell_titan_fireforged_equip'),
(93550, 'spell_titan_fireforged_equip'),
(93551, 'spell_titan_fireforged_equip'),
(93552, 'spell_titan_fireforged_equip'),
(93553, 'spell_titan_fireforged_equip'),
(93554, 'spell_titan_fireforged_equip'),
(93555, 'spell_titan_fireforged_equip'),
(93556, 'spell_titan_fireforged_equip'),
(93557, 'spell_titan_fireforged_equip');
