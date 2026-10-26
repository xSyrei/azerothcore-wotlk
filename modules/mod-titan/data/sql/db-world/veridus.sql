-- Veridus: one-handed sword with a custom model (display 90001, shipped in client-patch/dist/patch-enUS-4.MPQ).
-- Placeholder stats for testing; tune before it drops anywhere.
SET @ENTRY := 910002;

DELETE FROM `item_template` WHERE `entry` = @ENTRY;
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `maxcount`, `stackable`, `stat_type1`, `stat_value1`, `stat_type2`, `stat_value2`, `stat_type3`, `stat_value3`, `dmg_min1`, `dmg_max1`, `dmg_type1`, `delay`, `bonding`, `description`, `Material`, `sheath`, `MaxDurability`) VALUES
(@ENTRY, 2, 7, -1, 'Veridus', 90001, 4, 0, 0, 25000, 13, -1, -1, 60, 1, 1, 1, 4, 12, 3, 12, 7, 15, 42, 79, 0, 2600, 1, 'Lightning still crackles along the blade.', 1, 1, 100);
