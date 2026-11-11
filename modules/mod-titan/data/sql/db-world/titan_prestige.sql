-- Prestige (src/prestige.cpp): the Keeper of the Ember, who forges level 60 Heroes anew at level 1. Talking to it
-- opens the Prestige window (npc_titan_prestige, the Emberforge_Prestige addon). A grey-bearded dwarf in a black
-- tuxedo and top hat (display 90206: client-patch/patch_creatures.py and bake_npc.py), in Stormwind's Trade
-- District and Orgrimmar's Valley of Strength.
SET @NPC := 910100;
SET @GUID := 9101000;
SET @QUEST := 910100;

DELETE FROM `creature_template` WHERE `entry` = @NPC;
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
    `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
    `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`,
    `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
    `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`,
    `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
    `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(@NPC, 0, 0, 0, 0, 0, 'Keeper of the Ember', 'Prestige', NULL, 0, 60, 60, 0, 35, 3, 1, 1.14286, 1, 1, 20, 0, 0, 1,
    2000, 2000, 1, 1, 1, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 10, 1, 1, 0, 0, 0, 1, 0, 0,
    'npc_titan_prestige', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` = @NPC;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(@NPC, 0, 90206, 1, 1, 0);

-- Its menu texts from before the window: gone.
DELETE FROM `npc_text` WHERE `ID` IN (910100, 910101, 910102);

-- Stormwind, Trade District; Orgrimmar, Valley of Strength (beside the holiday spawns there).
DELETE FROM `creature` WHERE `id` = @NPC AND `guid` IN (@GUID, @GUID + 1);
INSERT INTO `creature` (`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`,
    `orientation`, `spawntimesecs`, `Comment`) VALUES
(@GUID, @NPC, 0, 1, 1, -8832.6, 643.6, 94.6, 4.22, 300, 'Keeper of the Ember (Prestige), Stormwind'),
(@GUID + 1, @NPC, 1, 1, 1, 1627.5, -4412.4, 16.2, 0.44, 300, 'Keeper of the Ember (Prestige), Orgrimmar');

-- Trial of Ember: Prestigious, given at every prestige (src/prestige.cpp): done on reaching the level cap again (an
-- event the script sets: SpecialFlags 2), turned in to the Keeper (npcflag 3: gossip and quest giver) for 5,000
-- Emberspark (OnQuestReward). No experience or money of its own.
DELETE FROM `quest_template` WHERE `ID` = @QUEST;
INSERT INTO `quest_template` (`ID`, `QuestType`, `QuestLevel`, `MinLevel`, `QuestSortID`, `RewardXPDifficulty`,
    `Flags`, `LogTitle`, `LogDescription`, `QuestDescription`, `QuestCompletionLog`, `ObjectiveText1`) VALUES
(@QUEST, 2, 60, 1, 0, 0, 0, 'Trial of Ember: Prestigious',
    'Reach level 60 again, then return to the Keeper of the Ember.',
    'You have given everything back to the Ember, $n: your level, your abilities, your deeds. Now prove that the '
    'forge was right to remake you. Climb to the summit once more, and when you stand there again, come back to me. '
    'The Ember rewards those who endure it.',
    'Return to the Keeper of the Ember in Stormwind or Orgrimmar.', 'Reach level 60 again');

DELETE FROM `quest_template_addon` WHERE `ID` = @QUEST;
INSERT INTO `quest_template_addon` (`ID`, `SpecialFlags`) VALUES
(@QUEST, 2);

DELETE FROM `quest_request_items` WHERE `ID` = @QUEST;
INSERT INTO `quest_request_items` (`ID`, `EmoteOnComplete`, `EmoteOnIncomplete`, `CompletionText`) VALUES
(@QUEST, 1, 1, 'Have you reached the summit again, $n?');

DELETE FROM `quest_offer_reward` WHERE `ID` = @QUEST;
INSERT INTO `quest_offer_reward` (`ID`, `Emote1`, `RewardText`) VALUES
(@QUEST, 4, 'The Ember burns brighter for you, $n. You have walked the whole road again and stand at its top. Take '
    'what you have earned: 5,000 Emberspark, and the right to be forged anew whenever you wish.');

DELETE FROM `creature_questender` WHERE `id` = @NPC AND `quest` = @QUEST;
INSERT INTO `creature_questender` (`id`, `quest`) VALUES
(@NPC, @QUEST);

-- Prestige Gear Cache: every piece worn at a prestige is packed into one (src/prestige.cpp, titan_prestige_gear in
-- db-characters); used, it gives the gear back. Soulbound, unsellable. Spell 3361 only makes the item usable in the
-- 3.3.5 client; the script handles the use.
SET @CACHE := 910021;
DELETE FROM `item_template` WHERE `entry` = @CACHE;
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`,
    `Flags`, `BuyPrice`, `SellPrice`, `ItemLevel`, `RequiredLevel`, `stackable`, `bonding`, `spellid_1`,
    `spelltrigger_1`, `spellcharges_1`, `description`, `ScriptName`) VALUES
(@CACHE, 15, 0, -1, 'Prestige Gear Cache', 9515, 4, 0, 0, 0, 1, 0, 1, 1, 3361, 0, 0,
    'The gear you wore when you were forged anew. Use it to take it back. Destroying it destroys the gear inside.',
    'item_titan_prestige_cache');
