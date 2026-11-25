-- The Firepit (src/firepit.cpp): each Hero's best run, for the Keeper's leaderboard.
CREATE TABLE IF NOT EXISTS `titan_firepit_best` (
    `guid` INT UNSIGNED NOT NULL,
    `best_wave` INT UNSIGNED NOT NULL DEFAULT 0,
    `group_size` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `kills` INT UNSIGNED NOT NULL DEFAULT 0,
    `run_date` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`),
    KEY `idx_best_wave` (`best_wave`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
