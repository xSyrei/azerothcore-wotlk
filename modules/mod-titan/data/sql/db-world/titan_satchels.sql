-- Emberforged Satchel: a 30-slot bag. Every Hero wears four, put in the bag slots at login (src/hero_equipment.cpp).
-- Soulbound, unique (4), worth nothing to a vendor. The client's copy: client-patch/patch_items.py.
SET @SATCHEL := 910020;

DELETE FROM `item_template` WHERE `entry` = @SATCHEL;
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`,
    `Flags`, `BuyPrice`, `SellPrice`, `InventoryType`, `ItemLevel`, `RequiredLevel`, `maxcount`, `stackable`,
    `ContainerSlots`, `bonding`, `Material`, `description`) VALUES
(@SATCHEL, 1, 0, -1, 'Emberforged Satchel', 34796, 3, 0, 0, 0, 18, 1, 1, 4, 1, 30, 1, 7,
    'Forged for every Hero of Emberforge.');
