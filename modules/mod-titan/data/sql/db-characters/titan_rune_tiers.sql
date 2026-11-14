-- Runes of Enhancement (src/rune_tiers.cpp; the catalog: db-world titan_rune_catalog). A prestige keeps them all.
-- The runes a Hero holds: an Epic or Rare in its socket (1-5, 1-10), an Uncommon in none (0).
CREATE TABLE IF NOT EXISTS `titan_rune_owned` (
    `guid` INT UNSIGNED NOT NULL,
    `rune` INT UNSIGNED NOT NULL,
    `socket` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`, `rune`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Offers not chosen yet (a choice of 3), oldest first.
CREATE TABLE IF NOT EXISTS `titan_rune_offers` (
    `guid` INT UNSIGNED NOT NULL,
    `seq` INT UNSIGNED NOT NULL,
    `level` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `rune1` INT UNSIGNED NOT NULL DEFAULT 0,
    `rune2` INT UNSIGNED NOT NULL DEFAULT 0,
    `rune3` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`, `seq`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- The highest level offered this run (every 5 levels): a prestige's level 1 starts it again.
CREATE TABLE IF NOT EXISTS `titan_rune_state` (
    `guid` INT UNSIGNED NOT NULL,
    `offered_level` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
