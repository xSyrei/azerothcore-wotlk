-- Monk spell scripts (src/monk.cpp) and the spells' share of spell power.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_monk_vivify', 'spell_titan_monk_renewing_mist',
    'spell_titan_monk_life_cocoon', 'spell_titan_monk_revival');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90301, 'spell_titan_monk_vivify'),
(90302, 'spell_titan_monk_renewing_mist'),
(90306, 'spell_titan_monk_life_cocoon'),
(90307, 'spell_titan_monk_revival');

-- Mistweaver: healing grows with spell power (direct heals and each tick of the heals over time); Vivify's
-- splash and Life Cocoon are the scripts' own amounts.
DELETE FROM `spell_bonus_data` WHERE `entry` BETWEEN 90300 AND 90310;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90300, 0, 0.12, 0, 0, 'Soothing Mist'),
(90301, 0.8, 0, 0, 0, 'Vivify'),
(90302, 0, 0.09, 0, 0, 'Renewing Mist'),
(90303, 0, 0.15, 0, 0, 'Enveloping Mist'),
(90305, 0.25, 0, 0, 0, 'Essence Font - bolt'),
(90306, 0, 0, 0, 0, 'Life Cocoon'),
(90307, 0.6, 0, 0, 0, 'Revival'),
(90310, 0, 0, 0, 0, 'Vivify - splash');
