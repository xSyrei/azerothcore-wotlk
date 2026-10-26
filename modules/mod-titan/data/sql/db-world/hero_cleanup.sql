-- Strip what Heroes inherit from the hidden base class (Paladin, ID 2): Wildcard grants their
-- abilities and talents instead. Paladin can no longer be created by players (see
-- CharacterCreating.Disabled.ClassMask), so these rows only ever applied to Heroes.
SET @HERO := 2;

-- Starting spells: Holy Light and Seal of Righteousness come from the Paladin skill lines.
DELETE FROM `playercreateinfo_skills` WHERE `raceMask` = 0 AND `classMask` = @HERO AND `skill` IN (184, 267, 594);

-- Action bars: drop the Paladin spells, and give the races added in hero_class.sql an
-- Auto Attack button like everyone else.
DELETE FROM `playercreateinfo_action` WHERE `class` = @HERO AND `action` IN (635, 21084);
DELETE FROM `playercreateinfo_action` WHERE `class` = @HERO AND `race` IN (2, 4, 5, 6, 7, 8);
INSERT INTO `playercreateinfo_action` (`race`, `class`, `button`, `action`, `type`) VALUES
(2, @HERO, 0, 6603, 0),
(4, @HERO, 0, 6603, 0),
(5, @HERO, 0, 6603, 0),
(6, @HERO, 0, 6603, 0),
(7, @HERO, 0, 6603, 0),
(8, @HERO, 0, 6603, 0);

-- Paladin trainers no longer train: their trainer and class-trainer NPC flags are removed.
UPDATE `creature_template` SET `npcflag` = `npcflag` & ~(16 | 32)
WHERE `entry` IN (SELECT `CreatureId` FROM `creature_default_trainer` WHERE `TrainerId` IN (SELECT `Id` FROM `trainer` WHERE `Type` = 0 AND `Requirement` = @HERO));

-- Paladin-only class quests are disabled.
DELETE FROM `disables` WHERE `sourceType` = 1 AND `entry` IN (SELECT `ID` FROM `quest_template_addon` WHERE `AllowableClasses` = @HERO);
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `comment`)
SELECT 1, `ID`, 0, 'Emberforge: Paladin class quest (Heroes are classless)' FROM `quest_template_addon` WHERE `AllowableClasses` = @HERO;
