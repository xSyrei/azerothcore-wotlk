-- Legendary Runes (src/runes.cpp): each specialization's second rune, from level 30 (0: none yet), and its modifiers
-- (1 to 3 per slot; 0: none). The first is in titan_runes. A prestige deletes a Hero's rows in both.
CREATE TABLE IF NOT EXISTS `titan_runes_second` (
    `guid` INT UNSIGNED NOT NULL,
    `spec` TINYINT UNSIGNED NOT NULL,
    `rune` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `invocation` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `manifestation` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `adaptation` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`, `spec`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
