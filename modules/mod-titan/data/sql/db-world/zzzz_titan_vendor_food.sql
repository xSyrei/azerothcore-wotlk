-- Food and drink vendors sell only what the player can eat or drink: every food and drink an npc_vendor sells
-- shows once the player reaches its required level (conditions: CONDITION_SOURCE_TYPE_NPC_VENDOR 23,
-- CONDITION_LEVEL 27, at least: COMP_TYPE_HIGH_EQ 3). The Burning Crusade's and Wrath's vendors (those selling food
-- or drink first meant for level 61 or more: titan_squish_item_original keeps the level before the squish) also
-- sell the standard bread and water, levels 1-45, so none of them is empty for a lower player.
-- Named zzzz_ to run after zz_titan_squish.sql (the levels it squished are the ones checked). Safe to run again.

-- The standard bread and water, at the vendors of the later expansions.
INSERT IGNORE INTO `npc_vendor` (`entry`, `slot`, `item`, `maxcount`, `incrtime`, `ExtendedCost`, `VerifiedBuild`)
SELECT DISTINCT v.`entry`, 0, s.`item`, 0, 0, 0, 0
FROM `npc_vendor` v
JOIN `item_template` it ON it.`entry` = v.`item`
LEFT JOIN `titan_squish_item_original` o ON o.`entry` = it.`entry`
CROSS JOIN (SELECT 159 AS `item` UNION SELECT 1179 UNION SELECT 1205 UNION SELECT 1708 UNION SELECT 1645
    UNION SELECT 8766 UNION SELECT 4540 UNION SELECT 4541 UNION SELECT 4542 UNION SELECT 4544 UNION SELECT 4601
    UNION SELECT 8950) s
WHERE it.`class` = 0 AND it.`subclass` = 5 AND COALESCE(o.`RequiredLevel`, it.`RequiredLevel`) >= 61;

-- Each food and drink shown from its required level.
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 23 AND `ConditionTypeOrReference` = 27
    AND `Comment` = 'Emberforge: vendor food from its level';
INSERT IGNORE INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`,
    `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`,
    `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`)
SELECT DISTINCT 23, v.`entry`, v.`item`, 0, 0, 27, 0, it.`RequiredLevel`, 3, 0, 0, 0, 0, '',
    'Emberforge: vendor food from its level'
FROM `npc_vendor` v
JOIN `item_template` it ON it.`entry` = v.`item`
WHERE it.`class` = 0 AND it.`subclass` = 5 AND it.`RequiredLevel` > 1;
