-- Dungeons taken out of the game: closed (normal and heroic: flags 1 | 2), so out of the dungeon finder and,
-- the Journal listing only open instances (src/journal.cpp), out of the Dungeon Journal.
DELETE FROM `disables` WHERE `sourceType` = 2 AND `entry` IN (560, 585, 632, 650, 658, 668);
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES
(2, 560, 3, '', '', 'Emberforge: Escape from Durnholde (Old Hillsbrad Foothills) retired'),
(2, 585, 3, '', '', 'Emberforge: Magisters'' Terrace retired'),
(2, 632, 3, '', '', 'Emberforge: The Forge of Souls retired'),
(2, 650, 3, '', '', 'Emberforge: Trial of the Champion retired'),
(2, 658, 3, '', '', 'Emberforge: Pit of Saron retired'),
(2, 668, 3, '', '', 'Emberforge: Halls of Reflection retired');
