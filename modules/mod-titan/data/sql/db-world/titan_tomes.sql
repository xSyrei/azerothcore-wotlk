-- Starting-kit tomes (src/item_titan_tomes.cpp): every Hero starts with both. They can be read any
-- number of times until level 9, when the abilities lock and the tomes vanish.
-- Titan Tome of Randomization: rerolls the Hero's 4 starting abilities.
-- Titan Tome of Selection: clears them for 4 picks in the Classless window.
-- Spell 3361 only makes the items usable in the 3.3.5 client; the scripts handle the use and it is
-- never cast. Kept in its own file after hero_outfit.sql, which rewrites whole races' start items.
SET @HERO := 2;
SET @RANDOMIZATION := 910003;
SET @SELECTION := 910004;
SET @CARRIER_SPELL := 3361;
SET @TOME_DISPLAY := 1103;

DELETE FROM `item_template` WHERE `entry` IN (@RANDOMIZATION, @SELECTION);
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyPrice`, `SellPrice`, `ItemLevel`, `RequiredLevel`, `stackable`, `bonding`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `description`, `ScriptName`) VALUES
(@RANDOMIZATION, 0, 8, -1, 'Titan Tome of Randomization', @TOME_DISPLAY, 4, 0, 0, 0, 1, 1, 1, 1, @CARRIER_SPELL, 0, 0, 'Rerolls your 4 starting abilities at random. Usable until level 9.', 'item_titan_tome_of_randomization'),
(@SELECTION, 0, 8, -1, 'Titan Tome of Selection', @TOME_DISPLAY, 4, 0, 0, 0, 1, 1, 1, 1, @CARRIER_SPELL, 0, 0, 'Clears your abilities so you can choose 4 in the Classless window. Usable until level 9.', 'item_titan_tome_of_selection');

DELETE FROM `playercreateinfo_item` WHERE `class` = @HERO AND `itemid` IN (@RANDOMIZATION, @SELECTION);
INSERT INTO `playercreateinfo_item` (`race`, `class`, `itemid`, `amount`, `Note`) VALUES
(1, @HERO, @RANDOMIZATION, 1, 'Titan Tome of Randomization'),
(1, @HERO, @SELECTION, 1, 'Titan Tome of Selection'),
(2, @HERO, @RANDOMIZATION, 1, 'Titan Tome of Randomization'),
(2, @HERO, @SELECTION, 1, 'Titan Tome of Selection'),
(3, @HERO, @RANDOMIZATION, 1, 'Titan Tome of Randomization'),
(3, @HERO, @SELECTION, 1, 'Titan Tome of Selection'),
(4, @HERO, @RANDOMIZATION, 1, 'Titan Tome of Randomization'),
(4, @HERO, @SELECTION, 1, 'Titan Tome of Selection'),
(5, @HERO, @RANDOMIZATION, 1, 'Titan Tome of Randomization'),
(5, @HERO, @SELECTION, 1, 'Titan Tome of Selection'),
(6, @HERO, @RANDOMIZATION, 1, 'Titan Tome of Randomization'),
(6, @HERO, @SELECTION, 1, 'Titan Tome of Selection'),
(7, @HERO, @RANDOMIZATION, 1, 'Titan Tome of Randomization'),
(7, @HERO, @SELECTION, 1, 'Titan Tome of Selection'),
(8, @HERO, @RANDOMIZATION, 1, 'Titan Tome of Randomization'),
(8, @HERO, @SELECTION, 1, 'Titan Tome of Selection'),
(10, @HERO, @RANDOMIZATION, 1, 'Titan Tome of Randomization'),
(10, @HERO, @SELECTION, 1, 'Titan Tome of Selection'),
(11, @HERO, @RANDOMIZATION, 1, 'Titan Tome of Randomization'),
(11, @HERO, @SELECTION, 1, 'Titan Tome of Selection');
