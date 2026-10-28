-- Demon Hunter abilities need a one-handed weapon in each hand: the off hand is checked by
-- spell_titan_dh_dual_weapons (src/demon_hunter.cpp).
DELETE FROM `spell_script_names` WHERE `spell_id` IN (90100, 90101, 90102, 90103, 90105);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90100, 'spell_titan_dh_dual_weapons'),
(90101, 'spell_titan_dh_dual_weapons'),
(90102, 'spell_titan_dh_dual_weapons'),
(90103, 'spell_titan_dh_dual_weapons'),
(90105, 'spell_titan_dh_dual_weapons');
