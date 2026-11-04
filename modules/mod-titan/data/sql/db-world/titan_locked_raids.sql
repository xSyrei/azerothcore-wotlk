-- Every raid closed for now (`disables`, a map each), so also out of the Dungeon Journal (src/journal.cpp). The flags
-- are the map's own difficulties only (more are refused): 1 the one size; 3 10 and 25 players; 15 with heroics.
-- To open them again, keep only the DELETE.
DELETE FROM `disables` WHERE `sourceType` = 2 AND `entry` IN (249, 309, 409, 469, 509, 531, 532, 533, 534, 544, 548,
    550, 564, 565, 568, 580, 603, 615, 616, 624, 631, 649, 724);
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES
(2, 249, 3, '', '', 'Emberforge: raids closed - Onyxia''s Lair'),
(2, 309, 1, '', '', 'Emberforge: raids closed - Zul''Gurub'),
(2, 409, 1, '', '', 'Emberforge: raids closed - Molten Core'),
(2, 469, 1, '', '', 'Emberforge: raids closed - Blackwing Lair'),
(2, 509, 1, '', '', 'Emberforge: raids closed - Ruins of Ahn''Qiraj'),
(2, 531, 1, '', '', 'Emberforge: raids closed - Temple of Ahn''Qiraj'),
(2, 532, 1, '', '', 'Emberforge: raids closed - Karazhan'),
(2, 533, 3, '', '', 'Emberforge: raids closed - Naxxramas'),
(2, 534, 1, '', '', 'Emberforge: raids closed - Hyjal Summit'),
(2, 544, 1, '', '', 'Emberforge: raids closed - Magtheridon''s Lair'),
(2, 548, 1, '', '', 'Emberforge: raids closed - Serpentshrine Cavern'),
(2, 550, 1, '', '', 'Emberforge: raids closed - Tempest Keep'),
(2, 564, 1, '', '', 'Emberforge: raids closed - Black Temple'),
(2, 565, 1, '', '', 'Emberforge: raids closed - Gruul''s Lair'),
(2, 568, 1, '', '', 'Emberforge: raids closed - Zul''Aman'),
(2, 580, 1, '', '', 'Emberforge: raids closed - Sunwell Plateau'),
(2, 603, 3, '', '', 'Emberforge: raids closed - Ulduar'),
(2, 615, 3, '', '', 'Emberforge: raids closed - The Obsidian Sanctum'),
(2, 616, 3, '', '', 'Emberforge: raids closed - The Eye of Eternity'),
(2, 624, 3, '', '', 'Emberforge: raids closed - Vault of Archavon'),
(2, 631, 15, '', '', 'Emberforge: raids closed - Icecrown Citadel'),
(2, 649, 15, '', '', 'Emberforge: raids closed - Trial of the Crusader'),
(2, 724, 15, '', '', 'Emberforge: raids closed - The Ruby Sanctum');
