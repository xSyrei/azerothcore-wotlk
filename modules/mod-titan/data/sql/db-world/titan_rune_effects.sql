-- Runes of Enhancement: the Epic and Rare runes' effects (src/rune_effects.cpp; spells 91171-91181:
-- client-patch/patch_runes.py, in titan_demon_hunter_spells.sql). Batch one: Warrior, Paladin, Rogue, Priest, Mage.

-- On-hit extras, by the spell's first rank (negative: every rank).
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_tier_hit', 'spell_titan_rune_tier_spread',
    'spell_titan_rune_tier_ward', 'spell_titan_rune_tier_quick_recovery');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(7384, 'spell_titan_rune_tier_hit'),
(-6572, 'spell_titan_rune_tier_hit'),
(-845, 'spell_titan_rune_tier_hit'),
(-23922, 'spell_titan_rune_tier_hit'),
(-6343, 'spell_titan_rune_tier_hit'),
(-635, 'spell_titan_rune_tier_hit'),
(-879, 'spell_titan_rune_tier_hit'),
(35395, 'spell_titan_rune_tier_hit'),
(-1329, 'spell_titan_rune_tier_hit'),
(22482, 'spell_titan_rune_tier_hit'),
(-16511, 'spell_titan_rune_tier_hit'),
(-1752, 'spell_titan_rune_tier_hit'),
(-8676, 'spell_titan_rune_tier_hit'),
(-8092, 'spell_titan_rune_tier_hit'),
(-2061, 'spell_titan_rune_tier_hit'),
(-44425, 'spell_titan_rune_tier_hit'),
(-133, 'spell_titan_rune_tier_hit'),
(-116, 'spell_titan_rune_tier_hit'),
(-120, 'spell_titan_rune_tier_hit'),
(-34861, 'spell_titan_rune_tier_hit'),
-- Living Bomb and Vampiric Touch spread; Power Word: Shield heals when broken; Quick Recovery's dodge.
(-44457, 'spell_titan_rune_tier_spread'),
(-34914, 'spell_titan_rune_tier_spread'),
(-17, 'spell_titan_rune_tier_ward'),
(91181, 'spell_titan_rune_tier_quick_recovery');

-- Quick Recovery: a melee attack dodged.
DELETE FROM `spell_proc` WHERE `SpellId` = 91181;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`,
    `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`,
    `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(91181, 0, 0, 0, 0, 0, 8, 0, 0, 16, 0, 0, 0, 100, 0, 0);

-- The strikes and the heal are shares of the Hero's own hits, their bonuses in them already.
DELETE FROM `spell_bonus_data` WHERE `entry` BETWEEN 91171 AND 91178;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(91171, 0, 0, 0, 0, 'Runes of Enhancement - physical strike'),
(91172, 0, 0, 0, 0, 'Runes of Enhancement - arcane strike'),
(91173, 0, 0, 0, 0, 'Runes of Enhancement - fire strike'),
(91174, 0, 0, 0, 0, 'Runes of Enhancement - frost strike'),
(91175, 0, 0, 0, 0, 'Runes of Enhancement - shadow strike'),
(91176, 0, 0, 0, 0, 'Runes of Enhancement - holy strike'),
(91177, 0, 0, 0, 0, 'Runes of Enhancement - Open Vein'),
(91178, 0, 0, 0, 0, 'Runes of Enhancement - Rune Mend');
