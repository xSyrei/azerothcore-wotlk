-- Specializations' card slots (src/specs.cpp): eight a specialization, each holding a card of the account's
-- collection (kind 0 a Skill Card by its first rank, 1 a Talent Card by its talent) and a Golden Card (golden 1).
CREATE TABLE IF NOT EXISTS `titan_spec_cards` (
    `guid` INT UNSIGNED NOT NULL,
    `spec` TINYINT UNSIGNED NOT NULL,
    `slot` TINYINT UNSIGNED NOT NULL,
    `golden` TINYINT UNSIGNED NOT NULL,
    `kind` TINYINT UNSIGNED NOT NULL,
    `card_id` INT UNSIGNED NOT NULL,
    PRIMARY KEY (`guid`, `spec`, `slot`, `golden`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
