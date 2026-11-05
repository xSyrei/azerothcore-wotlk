-- Silas Darkmoon (src/cards.cpp): his Darkmoon shop (npc_titan_silas, the Emberforge_Darkmoon addon) sells Skill and
-- Talent Packs and any card for Emberspark, Skill Packs for gold, and Golden Packs. His own look (display 14855),
-- beside the Keeper of the Ember in Stormwind's Trade District and Orgrimmar's Valley of Strength.
-- The packs: opened from the bags (item_titan_*_pack), the cards are the account's. Spell 3361 only makes them usable
-- in the 3.3.5 client; the scripts handle the use.
SET @NPC := 910110;
SET @GUID := 9101010;
SET @SKILL_PACK := 910010;
SET @TALENT_PACK := 910011;
SET @GOLDEN_PACK := 910012;
SET @CARRIER_SPELL := 3361;

DELETE FROM `creature_template` WHERE `entry` = @NPC;
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
    `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
    `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`,
    `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
    `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`,
    `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
    `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(@NPC, 0, 0, 0, 0, 0, 'Silas Darkmoon', 'Cards & Specializations', NULL, 0, 60, 60, 0, 35, 1, 1, 1.14286, 1, 1, 20,
    0, 0, 1, 2000, 2000, 1, 1, 1, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 10, 1, 1, 0, 0, 0, 1, 0, 0,
    'npc_titan_silas', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` = @NPC;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(@NPC, 0, 14855, 1, 1, 0);

DELETE FROM `creature` WHERE `id` = @NPC AND `guid` IN (@GUID, @GUID + 1);
INSERT INTO `creature` (`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`,
    `orientation`, `spawntimesecs`, `Comment`) VALUES
(@GUID, @NPC, 0, 1, 1, -8826.5, 640.5, 94.45, 3.9, 300, 'Silas Darkmoon (cards), Stormwind'),
(@GUID + 1, @NPC, 1, 1, 1, 1631.0, -4411.5, 16.4, 0.44, 300, 'Silas Darkmoon (cards), Orgrimmar');

DELETE FROM `item_template` WHERE `entry` IN (@SKILL_PACK, @TALENT_PACK, @GOLDEN_PACK);
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`,
    `Flags`, `BuyPrice`, `SellPrice`, `ItemLevel`, `RequiredLevel`, `stackable`, `bonding`, `spellid_1`,
    `spelltrigger_1`, `spellcharges_1`, `description`, `ScriptName`) VALUES
(@SKILL_PACK, 15, 0, -1, 'Skill Card Pack', 31755, 3, 0, 0, 0, 1, 1, 200, 1, @CARRIER_SPELL, 0, 0,
    'Three Skill Cards, for your account''s collection.', 'item_titan_skill_pack'),
(@TALENT_PACK, 15, 0, -1, 'Talent Card Pack', 44704, 3, 0, 0, 0, 1, 1, 200, 1, @CARRIER_SPELL, 0, 0,
    'Three Talent Cards, for your account''s collection.', 'item_titan_talent_pack'),
(@GOLDEN_PACK, 15, 0, -1, 'Golden Card Pack', 56181, 5, 0, 0, 0, 1, 1, 200, 1, @CARRIER_SPELL, 0, 0,
    'Three Golden Cards: one more of a card, past your slots.', 'item_titan_golden_pack');
