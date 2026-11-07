-- The dungeon finder from level 10 (DungeonFinder.MinLevel = 10, docker-compose.yml; src/server/game/DungeonFinding/
-- LFGMgr.cpp): every normal 5-player dungeon queues from level 10 to the cap, scaled by mod-autobalance, every heroic
-- at 60. Their entrances follow, with no level cap and no gear score. Raids keep their own; the retired dungeons
-- (titan_retired_dungeons.sql) stay closed by their disables.
UPDATE `dungeon_access_template` SET `min_level` = 10, `max_level` = 0, `min_avg_item_level` = 0
WHERE `difficulty` = 0 AND `map_id` IN (33, 34, 36, 43, 47, 48, 70, 90, 109, 129, 189, 209, 229, 230, 269, 289, 329,
    349, 389, 429, 540, 542, 543, 545, 546, 547, 552, 553, 554, 555, 556, 557, 558, 560, 574, 575, 576, 578, 585, 595,
    599, 600, 601, 602, 604, 608, 619, 632, 650, 658, 668);
UPDATE `dungeon_access_template` SET `min_level` = 60, `max_level` = 0, `min_avg_item_level` = 0
WHERE `difficulty` = 1 AND `map_id` IN (33, 34, 36, 43, 47, 48, 70, 90, 109, 129, 189, 209, 229, 230, 269, 289, 329,
    349, 389, 429, 540, 542, 543, 545, 546, 547, 552, 553, 554, 555, 556, 557, 558, 560, 574, 575, 576, 578, 585, 595,
    599, 600, 601, 602, 604, 608, 619, 632, 650, 658, 668);
