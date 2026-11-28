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
