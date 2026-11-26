-- The Firepit (src/firepit.cpp): each Hero's best run in each bracket (1 solo, 2 duo, 3 trio, 4 a group of 4 to 10),
-- for the group finder's records and leaderboards. The first records (titan_firepit_best, one a Hero) come over.
CREATE TABLE IF NOT EXISTS `titan_firepit_records` (
    `guid` INT UNSIGNED NOT NULL,
    `bracket` TINYINT UNSIGNED NOT NULL,
    `best_wave` INT UNSIGNED NOT NULL DEFAULT 0,
    `group_size` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `kills` INT UNSIGNED NOT NULL DEFAULT 0,
    `run_date` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`, `bracket`),
    KEY `idx_bracket_wave` (`bracket`, `best_wave`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO `titan_firepit_records` (`guid`, `bracket`, `best_wave`, `group_size`, `kills`, `run_date`)
SELECT `guid`, LEAST(GREATEST(`group_size`, 1), 4), `best_wave`, `group_size`, `kills`, `run_date`
FROM `titan_firepit_best`;
