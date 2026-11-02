-- Smoldering Adventurer's Cache (src/smoldering_cache.cpp): a Hero gets one for every level gained and every quest
-- turned in; opening one gives a piece of gear for their level in their lowest item level slot, picked for their role.
-- Stacks to 200. Spell 3361 only makes the item usable in the 3.3.5 client; the script handles the use.
SET @ENTRY := 910005;
SET @CARRIER_SPELL := 3361;
DELETE FROM `item_template` WHERE `entry` = @ENTRY;
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyPrice`, `SellPrice`, `ItemLevel`, `RequiredLevel`, `stackable`, `bonding`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `description`, `ScriptName`) VALUES
(@ENTRY, 15, 0, -1, 'Smoldering Adventurer''s Cache', 12644, 2, 0, 0, 0, 1, 1, 200, 1, @CARRIER_SPELL, 0, 0, 'Still warm. Holds a piece of gear for your level, for the slot you need it most.', 'item_titan_smoldering_cache');
