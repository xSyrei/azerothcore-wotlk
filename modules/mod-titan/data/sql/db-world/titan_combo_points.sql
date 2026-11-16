-- Honor Among Thieves: only a party member's critical strikes award a Hero a combo point (src/combo_points.cpp).
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_titan_honor_among_thieves';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(51698, 'spell_titan_honor_among_thieves'),
(51700, 'spell_titan_honor_among_thieves'),
(51701, 'spell_titan_honor_among_thieves');
