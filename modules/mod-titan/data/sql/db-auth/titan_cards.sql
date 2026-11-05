-- Skill and Talent Cards (src/cards.cpp): each account's collection, owned forever by every character on it. kind:
-- 0 Skill Card (card_id: the ability's first rank), 1 Talent Card (card_id: the talent; count: ranks owned), 2 and 3
-- their Golden Cards. The tallies: duplicates opened (every 10th a free pack, every 50th a Golden Pack) and Skill
-- Packs bought with gold this week.
CREATE TABLE IF NOT EXISTS `titan_cards` (
    `account_id` INT UNSIGNED NOT NULL,
    `kind` TINYINT UNSIGNED NOT NULL,
    `card_id` INT UNSIGNED NOT NULL,
    `count` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`account_id`, `kind`, `card_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `titan_card_tallies` (
    `account_id` INT UNSIGNED NOT NULL,
    `duplicates` INT UNSIGNED NOT NULL DEFAULT 0,
    `gold_week` INT UNSIGNED NOT NULL DEFAULT 0,
    `gold_bought` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
