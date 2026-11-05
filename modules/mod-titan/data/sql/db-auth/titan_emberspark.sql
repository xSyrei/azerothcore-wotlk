-- Emberspark: Project Emberforge's server currency, one balance per account (src/emberspark.cpp). Earned in game
-- (prestige) and, later, bought and spent in the shop. Every change is logged.
CREATE TABLE IF NOT EXISTS `titan_emberspark` (
    `account_id` INT UNSIGNED NOT NULL,
    `balance` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `titan_emberspark_log` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `account_id` INT UNSIGNED NOT NULL,
    `amount` INT NOT NULL,
    `reason` VARCHAR(64) NOT NULL,
    `time` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    KEY `idx_account` (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
