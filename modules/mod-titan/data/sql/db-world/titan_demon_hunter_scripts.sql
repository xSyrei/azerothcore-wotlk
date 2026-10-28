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

-- Fel Rush: its damage (90104) only hits the enemies in the dash's path.
DELETE FROM `spell_script_names` WHERE `spell_id` = 90104 AND `ScriptName` = 'spell_titan_dh_fel_rush_damage';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90104, 'spell_titan_dh_fel_rush_damage');

-- Eye Beam: its damage (90106) only hits the enemies along the beam.
DELETE FROM `spell_script_names` WHERE `spell_id` = 90106 AND `ScriptName` = 'spell_titan_dh_eye_beam_damage';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90106, 'spell_titan_dh_eye_beam_damage');

-- Fel Rush: its dash runs at its own, faster speed.
DELETE FROM `spell_script_names` WHERE `spell_id` = 90103 AND `ScriptName` = 'spell_titan_dh_fel_rush';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90103, 'spell_titan_dh_fel_rush');

-- Vengeful Retreat and Throw Glaive: both weapons needed; their hits' share of attack power; Throw
-- Glaive's ricochet reaches 10 yards (a ranged spell's chain would reach 7.5).
DELETE FROM `spell_script_names` WHERE `spell_id` IN (90109, 90111) AND `ScriptName` = 'spell_titan_dh_dual_weapons';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90109, 'spell_titan_dh_dual_weapons'),
(90111, 'spell_titan_dh_dual_weapons');
DELETE FROM `spell_bonus_data` WHERE `entry` IN (90110, 90111);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90110, 0, 0, 0.4, 0, 'Vengeful Retreat'),
(90111, 0, 0, 0.45, 0, 'Throw Glaive');
DELETE FROM `spell_jump_distance` WHERE `ID` = 90111;
INSERT INTO `spell_jump_distance` (`ID`, `JumpDistance`) VALUES
(90111, 10);
