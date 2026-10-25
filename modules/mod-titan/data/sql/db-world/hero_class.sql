-- Classless "Hero": every character is created as the hidden base class (Paladin, ID 2),
-- renamed "Hero" on the client by the client patch (see modules/mod-titan/client-patch).
-- Paladin was already allowed for Human, Dwarf, Blood Elf and Draenei; this opens it to the
-- other six races, starting each in its race's usual starting zone.
SET @HERO := 2;

DELETE FROM `playercreateinfo` WHERE `class` = @HERO AND `race` IN (2, 4, 5, 6, 7, 8);
INSERT INTO `playercreateinfo` (`race`, `class`, `map`, `zone`, `position_x`, `position_y`, `position_z`, `orientation`) VALUES
(2, @HERO, 1, 14, -618.518, -4251.67, 38.718, 0),
(4, @HERO, 1, 141, 10311.3, 832.463, 1326.41, 5.69632),
(5, @HERO, 0, 85, 1676.71, 1678.31, 121.67, 2.70526),
(6, @HERO, 1, 215, -2917.58, -257.98, 52.9968, 0),
(7, @HERO, 0, 1, -6240.32, 331.033, 382.758, 0),
(8, @HERO, 1, 14, -618.518, -4251.67, 38.718, 0);

-- The client's starting outfits only cover the original Paladin races, so the new
-- combinations get a basic kit: shirt, pants, boots, a mace and a Hearthstone.
DELETE FROM `playercreateinfo_item` WHERE `class` = @HERO AND `race` IN (2, 4, 5, 6, 7, 8);
INSERT INTO `playercreateinfo_item` (`race`, `class`, `itemid`, `amount`, `Note`) VALUES
(2, @HERO, 38, 1, 'Hero starter kit: Recruit''s Shirt'),
(2, @HERO, 39, 1, 'Hero starter kit: Recruit''s Pants'),
(2, @HERO, 40, 1, 'Hero starter kit: Recruit''s Boots'),
(2, @HERO, 36, 1, 'Hero starter kit: Worn Mace'),
(2, @HERO, 6948, 1, 'Hero starter kit: Hearthstone'),
(4, @HERO, 38, 1, 'Hero starter kit: Recruit''s Shirt'),
(4, @HERO, 39, 1, 'Hero starter kit: Recruit''s Pants'),
(4, @HERO, 40, 1, 'Hero starter kit: Recruit''s Boots'),
(4, @HERO, 36, 1, 'Hero starter kit: Worn Mace'),
(4, @HERO, 6948, 1, 'Hero starter kit: Hearthstone'),
(5, @HERO, 38, 1, 'Hero starter kit: Recruit''s Shirt'),
(5, @HERO, 39, 1, 'Hero starter kit: Recruit''s Pants'),
(5, @HERO, 40, 1, 'Hero starter kit: Recruit''s Boots'),
(5, @HERO, 36, 1, 'Hero starter kit: Worn Mace'),
(5, @HERO, 6948, 1, 'Hero starter kit: Hearthstone'),
(6, @HERO, 38, 1, 'Hero starter kit: Recruit''s Shirt'),
(6, @HERO, 39, 1, 'Hero starter kit: Recruit''s Pants'),
(6, @HERO, 40, 1, 'Hero starter kit: Recruit''s Boots'),
(6, @HERO, 36, 1, 'Hero starter kit: Worn Mace'),
(6, @HERO, 6948, 1, 'Hero starter kit: Hearthstone'),
(7, @HERO, 38, 1, 'Hero starter kit: Recruit''s Shirt'),
(7, @HERO, 39, 1, 'Hero starter kit: Recruit''s Pants'),
(7, @HERO, 40, 1, 'Hero starter kit: Recruit''s Boots'),
(7, @HERO, 36, 1, 'Hero starter kit: Worn Mace'),
(7, @HERO, 6948, 1, 'Hero starter kit: Hearthstone'),
(8, @HERO, 38, 1, 'Hero starter kit: Recruit''s Shirt'),
(8, @HERO, 39, 1, 'Hero starter kit: Recruit''s Pants'),
(8, @HERO, 40, 1, 'Hero starter kit: Recruit''s Boots'),
(8, @HERO, 36, 1, 'Hero starter kit: Worn Mace'),
(8, @HERO, 6948, 1, 'Hero starter kit: Hearthstone');
