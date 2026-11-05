-- Prestige (src/prestige.cpp): the Keeper of the Ember, who forges level 60 Heroes anew at level 1. Talking to it
-- opens the Prestige window (npc_titan_prestige, the Emberforge_Prestige addon). A dark iron dwarf of the Thorium
-- Brotherhood's look (display 14666, Lokhtos Darkbargainer's), in Stormwind's Trade District and Orgrimmar's
-- Valley of Strength.
SET @NPC := 910100;
SET @GUID := 9101000;

DELETE FROM `creature_template` WHERE `entry` = @NPC;
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
    `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
    `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`,
    `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
    `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`,
    `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
    `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(@NPC, 0, 0, 0, 0, 0, 'Keeper of the Ember', 'Prestige', NULL, 0, 60, 60, 0, 35, 1, 1, 1.14286, 1, 1, 20, 0, 0, 1,
    2000, 2000, 1, 1, 1, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 10, 1, 1, 0, 0, 0, 1, 0, 0,
    'npc_titan_prestige', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` = @NPC;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(@NPC, 0, 14666, 1, 1, 0);

-- Its menu texts from before the window: gone.
DELETE FROM `npc_text` WHERE `ID` IN (910100, 910101, 910102);

-- Stormwind, Trade District; Orgrimmar, Valley of Strength (beside the holiday spawns there).
DELETE FROM `creature` WHERE `id` = @NPC AND `guid` IN (@GUID, @GUID + 1);
INSERT INTO `creature` (`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`,
    `orientation`, `spawntimesecs`, `Comment`) VALUES
(@GUID, @NPC, 0, 1, 1, -8832.6, 643.6, 94.6, 4.22, 300, 'Keeper of the Ember (Prestige), Stormwind'),
(@GUID + 1, @NPC, 1, 1, 1, 1627.5, -4412.4, 16.2, 0.44, 300, 'Keeper of the Ember (Prestige), Orgrimmar');
