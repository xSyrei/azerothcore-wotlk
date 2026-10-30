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

-- Chaos Nova, Blur and Consume Magic: both weapons needed; Chaos Nova's share of attack power.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (90112, 90113, 90114) AND `ScriptName` = 'spell_titan_dh_dual_weapons';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90112, 'spell_titan_dh_dual_weapons'),
(90113, 'spell_titan_dh_dual_weapons'),
(90114, 'spell_titan_dh_dual_weapons');
DELETE FROM `spell_bonus_data` WHERE `entry` = 90112;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90112, 0, 0, 0.4, 0, 'Chaos Nova');

-- Demon Form (90115): the demon shape (spell_titan_dh_demon_form), worn by Eye Beam for its channel
-- and by Chaos Nova for a moment; Metamorphosis (90116) wears it for its 30 sec.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_dh_demon_form', 'spell_titan_dh_eye_beam',
    'spell_titan_dh_chaos_nova');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90115, 'spell_titan_dh_demon_form'),
(90116, 'spell_titan_dh_demon_form'),
(90105, 'spell_titan_dh_eye_beam'),
(90112, 'spell_titan_dh_chaos_nova');

-- Felblade, Darkness and Immolation Aura: both weapons needed; Felblade's and Immolation Aura's pulses'
-- share of attack power; Darkness's chance to avoid a hit (spell_titan_dh_darkness).
DELETE FROM `spell_script_names` WHERE `spell_id` IN (90117, 90118, 90119)
    AND `ScriptName` = 'spell_titan_dh_dual_weapons';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90117, 'spell_titan_dh_dual_weapons'),
(90118, 'spell_titan_dh_dual_weapons'),
(90119, 'spell_titan_dh_dual_weapons');
DELETE FROM `spell_script_names` WHERE `spell_id` = 90118 AND `ScriptName` = 'spell_titan_dh_darkness';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90118, 'spell_titan_dh_darkness');
DELETE FROM `spell_bonus_data` WHERE `entry` IN (90117, 90120);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90117, 0, 0, 0.5, 0, 'Felblade'),
(90120, 0, 0, 0.2, 0, 'Immolation Aura - pulse');

-- Fel Barrage: both weapons needed; its Fury drain (spell_titan_dh_fel_barrage); each pulse's share of attack
-- power, and the fel balls raining down with it (spell_titan_dh_fel_barrage_pulse).
DELETE FROM `spell_script_names` WHERE `spell_id` = 90121 AND `ScriptName` IN ('spell_titan_dh_dual_weapons',
    'spell_titan_dh_fel_barrage');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90121, 'spell_titan_dh_dual_weapons'),
(90121, 'spell_titan_dh_fel_barrage');
DELETE FROM `spell_bonus_data` WHERE `entry` IN (90122, 90123);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90122, 0, 0, 0.15, 0, 'Fel Barrage - pulse');
DELETE FROM `spell_script_names` WHERE `spell_id` = 90122 AND `ScriptName` = 'spell_titan_dh_fel_barrage_pulse';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90122, 'spell_titan_dh_fel_barrage_pulse');

-- Havoc talents (client-patch/patch_talents.py): the abilities whose scripts apply them, and the talents' own
-- procs (Desperate Instincts, Soul Rending).
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_dh_chaos_strike',
    'spell_titan_dh_vengeful_retreat', 'spell_titan_dh_immolation_aura', 'spell_titan_dh_throw_glaive',
    'spell_titan_dh_blade_dance_final_strike', 'spell_titan_dh_desperate_instincts', 'spell_titan_dh_soul_rending');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90101, 'spell_titan_dh_chaos_strike'),
(90109, 'spell_titan_dh_vengeful_retreat'),
(90119, 'spell_titan_dh_immolation_aura'),
(90111, 'spell_titan_dh_throw_glaive'),
(90108, 'spell_titan_dh_blade_dance_final_strike'),
(90218, 'spell_titan_dh_desperate_instincts'),
(90219, 'spell_titan_dh_desperate_instincts'),
(90241, 'spell_titan_dh_soul_rending'),
(90242, 'spell_titan_dh_soul_rending'),
(90243, 'spell_titan_dh_soul_rending');
-- Demon Blades' hit takes 20% of attack power; the helpers whose amount the scripts set (a share of a hit,
-- healing) take nothing more.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (90152, 90155, 90156, 90157, 90158);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90152, 0, 0, 0, 0, 'Bloodlet'),
(90155, 0, 0, 0, 0, 'Chaos Cleave'),
(90156, 0, 0, 0.2, 0, 'Demon Blades'),
(90157, 0, 0, 0, 0, 'Trail of Ruin'),
(90158, 0, 0, 0, 0, 'Soul Rending');

-- Vengeance: Shear, Soul Cleave, Demon Spikes, Fiery Brand and Torment need both weapons; Soul Cleave eats Soul
-- Fragments (spell_titan_dh_soul_cleave); Fiery Brand's share of attack power.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (90124, 90125, 90127, 90128, 90129)
    AND `ScriptName` = 'spell_titan_dh_dual_weapons';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90124, 'spell_titan_dh_dual_weapons'),
(90125, 'spell_titan_dh_dual_weapons'),
(90127, 'spell_titan_dh_dual_weapons'),
(90128, 'spell_titan_dh_dual_weapons'),
(90129, 'spell_titan_dh_dual_weapons');
DELETE FROM `spell_script_names` WHERE `spell_id` = 90125 AND `ScriptName` = 'spell_titan_dh_soul_cleave';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90125, 'spell_titan_dh_soul_cleave');
DELETE FROM `spell_bonus_data` WHERE `entry` = 90128;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90128, 0, 0, 0.5, 0, 'Fiery Brand');

-- Spirit Bomb, Soul Barrier and Sigil of Chains: both weapons needed; their scripts (Soul Fragments eaten, Frailty's
-- healing, the sigil's pull); the explosion's and the healing's amounts are the scripts' own.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (90132, 90135, 90138)
    AND `ScriptName` = 'spell_titan_dh_dual_weapons';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90132, 'spell_titan_dh_dual_weapons'),
(90135, 'spell_titan_dh_dual_weapons'),
(90138, 'spell_titan_dh_dual_weapons');
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_dh_spirit_bomb', 'spell_titan_dh_frailty',
    'spell_titan_dh_soul_barrier', 'spell_titan_dh_sigil_of_chains');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90135, 'spell_titan_dh_spirit_bomb'),
(90137, 'spell_titan_dh_frailty'),
(90138, 'spell_titan_dh_soul_barrier'),
(90132, 'spell_titan_dh_sigil_of_chains');
DELETE FROM `spell_bonus_data` WHERE `entry` IN (90136, 90139);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90136, 0, 0, 0, 0, 'Spirit Bomb - explosion'),
(90139, 0, 0, 0, 0, 'Frailty - healing');
