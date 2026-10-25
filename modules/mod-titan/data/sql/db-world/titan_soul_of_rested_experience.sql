-- Titan Soul of Rested Experience: grants 4 levels of rested experience.
-- The effect is handled by the item script; spell 3361 only makes the item usable
-- in the 3.3.5 client and is never cast. Replace it with a dedicated spell from the
-- client patch to get a proper "Use:" tooltip line.
SET @ENTRY := 910001;
SET @CARRIER_SPELL := 3361;

DELETE FROM `item_template` WHERE `entry` = @ENTRY;
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyPrice`, `SellPrice`, `ItemLevel`, `RequiredLevel`, `stackable`, `bonding`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `description`, `ScriptName`) VALUES
(@ENTRY, 0, 8, -1, 'Titan Soul of Rested Experience', 6689, 4, 0, 0, 0, 1, 1, 20, 1, @CARRIER_SPELL, 0, -1, 'Grants 4 levels of rested experience.', 'item_titan_soul_of_rested_experience');
