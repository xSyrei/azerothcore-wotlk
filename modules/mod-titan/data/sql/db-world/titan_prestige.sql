-- Prestige (src/prestige.cpp): the Keeper of the Ember, who forges level 60 Heroes anew at level 1. Its menu is the
-- script's (npc_titan_prestige): the texts below, the prestige with its confirmation, the Emberspark balance and the
-- Hero's prestige, this run's time and the fastest. A dark iron dwarf of the Thorium Brotherhood's look (display
-- 14666, Lokhtos Darkbargainer's), in Stormwind's Trade District and Orgrimmar's Valley of Strength.
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

-- At the cap, below it, and what is lost, kept and given ($B: a line break, $n: the player's name).
DELETE FROM `npc_text` WHERE `ID` IN (910100, 910101, 910102);
INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `Probability0`) VALUES
(910100, 'The forge burns for those who reach the summit, $n. Prestige, and you begin again at level 1: new abilities and talents, every quest to earn anew.$B$BYour gear waits in your bags and goes back on as soon as you reach its level. Your gold and your Emberspark stay yours.$B$BEvery prestige brings 500 Emberspark, 2 Titan Souls of Rested Experience and Prestigious: 10% more experience until level 60. Reach level 60 again for 5,000 Emberspark more.',
    'The forge burns for those who reach the summit, $n. Prestige, and you begin again at level 1: new abilities and talents, every quest to earn anew.$B$BYour gear waits in your bags and goes back on as soon as you reach its level. Your gold and your Emberspark stay yours.$B$BEvery prestige brings 500 Emberspark, 2 Titan Souls of Rested Experience and Prestigious: 10% more experience until level 60. Reach level 60 again for 5,000 Emberspark more.', 1),
(910101, 'Not yet, $n. Only those who have reached level 60 may be forged anew.$B$BWhen you do, come back to me. You will begin again at level 1 with new abilities and talents, keep your gear and your currency, and earn Emberspark for the journey.',
    'Not yet, $n. Only those who have reached level 60 may be forged anew.$B$BWhen you do, come back to me. You will begin again at level 1 with new abilities and talents, keep your gear and your currency, and earn Emberspark for the journey.', 1),
(910102, 'YOU LOSE$B- Your level: back to 1.$B- Your Wildcard abilities and talents, rerolled. The starting tomes come back.$B- Your quests: every one can be done, and rewarded, again.$B$BYOU KEEP$B- All your gear, back on once you reach its level. What does not fit in your bags comes by mail.$B- Gold and Emberspark.$B- Your role, locked until you reach level 60 again.$B- Professions, mounts, reputation and achievements.$B$BYOU GET$B- 500 Emberspark at once.$B- 2 Titan Souls of Rested Experience.$B- Prestigious: 10% more experience from everything, until level 60.$B- 5,000 Emberspark when you reach level 60 again.',
    'YOU LOSE$B- Your level: back to 1.$B- Your Wildcard abilities and talents, rerolled. The starting tomes come back.$B- Your quests: every one can be done, and rewarded, again.$B$BYOU KEEP$B- All your gear, back on once you reach its level. What does not fit in your bags comes by mail.$B- Gold and Emberspark.$B- Your role, locked until you reach level 60 again.$B- Professions, mounts, reputation and achievements.$B$BYOU GET$B- 500 Emberspark at once.$B- 2 Titan Souls of Rested Experience.$B- Prestigious: 10% more experience from everything, until level 60.$B- 5,000 Emberspark when you reach level 60 again.', 1);

-- Stormwind, Trade District; Orgrimmar, Valley of Strength (beside the holiday spawns there).
DELETE FROM `creature` WHERE `id1` = @NPC AND `guid` IN (@GUID, @GUID + 1);
INSERT INTO `creature` (`guid`, `id1`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`,
    `orientation`, `spawntimesecs`, `Comment`) VALUES
(@GUID, @NPC, 0, 1, 1, -8832.6, 643.6, 94.6, 4.22, 300, 'Keeper of the Ember (Prestige), Stormwind'),
(@GUID + 1, @NPC, 1, 1, 1, 1627.5, -4412.4, 16.2, 0.44, 300, 'Keeper of the Ember (Prestige), Orgrimmar');
