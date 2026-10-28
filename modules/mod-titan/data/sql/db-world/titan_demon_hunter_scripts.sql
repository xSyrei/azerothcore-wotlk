-- Demon Hunter spell scripts (src/demon_hunter.cpp). Every ability needs a one-handed weapon in
-- each hand: the off hand is checked by spell_titan_dh_dual_weapons.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (90100, 90101, 90102, 90103, 90105);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90100, 'spell_titan_dh_dual_weapons'),
(90101, 'spell_titan_dh_dual_weapons'),
(90102, 'spell_titan_dh_dual_weapons'),
(90103, 'spell_titan_dh_dual_weapons'),
(90105, 'spell_titan_dh_dual_weapons');

-- Blade Dance: its four strikes (spell_titan_dh_blade_dance), and their share of attack power.
DELETE FROM `spell_script_names` WHERE `spell_id` = 90102 AND `ScriptName` = 'spell_titan_dh_blade_dance';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90102, 'spell_titan_dh_blade_dance');
DELETE FROM `spell_bonus_data` WHERE `entry` IN (90107, 90108);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90107, 0, 0, 0.2475, 0, 'Blade Dance - strike'),
(90108, 0, 0, 0.742, 0, 'Blade Dance - final strike');
