-- Legendary Runes (src/runes.cpp): each specialization's rune (0: none yet; 21: Fel Commandment) and its modifiers
-- (1 to 3 per slot; 0: none). A prestige deletes a Hero's rows: the runes are chosen again from level 10.
CREATE TABLE IF NOT EXISTS `titan_runes` (
    `guid` INT UNSIGNED NOT NULL,
    `spec` TINYINT UNSIGNED NOT NULL,
    `rune` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `invocation` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `manifestation` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `adaptation` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`, `spec`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
