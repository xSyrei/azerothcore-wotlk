-- Every Hero starts in the Paladin starting outfit, the same one the client's character creation
-- preview shows (client-patch/patch_hero_class.py adds it to CharStartOutfit.dbc for the races
-- that had none). Replaces the basic kit hero_class.sql gave the newly opened races.
SET @HERO := 2;

DELETE FROM `playercreateinfo_item` WHERE `class` = @HERO AND `race` IN (2, 4, 5, 6, 7, 8);
INSERT INTO `playercreateinfo_item` (`race`, `class`, `itemid`, `amount`, `Note`) VALUES
(2, @HERO, 45, 1, 'Hero starter kit: Squire''s Shirt'),
(2, @HERO, 44, 1, 'Hero starter kit: Squire''s Pants'),
(2, @HERO, 43, 1, 'Hero starter kit: Squire''s Boots'),
(2, @HERO, 2361, 1, 'Hero starter kit: Battleworn Hammer'),
(2, @HERO, 6948, 1, 'Hero starter kit: Hearthstone'),
(4, @HERO, 45, 1, 'Hero starter kit: Squire''s Shirt'),
(4, @HERO, 44, 1, 'Hero starter kit: Squire''s Pants'),
(4, @HERO, 43, 1, 'Hero starter kit: Squire''s Boots'),
(4, @HERO, 2361, 1, 'Hero starter kit: Battleworn Hammer'),
(4, @HERO, 6948, 1, 'Hero starter kit: Hearthstone'),
(5, @HERO, 45, 1, 'Hero starter kit: Squire''s Shirt'),
(5, @HERO, 44, 1, 'Hero starter kit: Squire''s Pants'),
(5, @HERO, 43, 1, 'Hero starter kit: Squire''s Boots'),
(5, @HERO, 2361, 1, 'Hero starter kit: Battleworn Hammer'),
(5, @HERO, 6948, 1, 'Hero starter kit: Hearthstone'),
(6, @HERO, 45, 1, 'Hero starter kit: Squire''s Shirt'),
(6, @HERO, 44, 1, 'Hero starter kit: Squire''s Pants'),
(6, @HERO, 43, 1, 'Hero starter kit: Squire''s Boots'),
(6, @HERO, 2361, 1, 'Hero starter kit: Battleworn Hammer'),
(6, @HERO, 6948, 1, 'Hero starter kit: Hearthstone'),
(7, @HERO, 45, 1, 'Hero starter kit: Squire''s Shirt'),
(7, @HERO, 44, 1, 'Hero starter kit: Squire''s Pants'),
(7, @HERO, 43, 1, 'Hero starter kit: Squire''s Boots'),
(7, @HERO, 2361, 1, 'Hero starter kit: Battleworn Hammer'),
(7, @HERO, 6948, 1, 'Hero starter kit: Hearthstone'),
(8, @HERO, 45, 1, 'Hero starter kit: Squire''s Shirt'),
(8, @HERO, 44, 1, 'Hero starter kit: Squire''s Pants'),
(8, @HERO, 43, 1, 'Hero starter kit: Squire''s Boots'),
(8, @HERO, 2361, 1, 'Hero starter kit: Battleworn Hammer'),
(8, @HERO, 6948, 1, 'Hero starter kit: Hearthstone');
