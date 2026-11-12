-- Wildcard's reroll blacklist (src/wildcard.cpp): a name rerolled away (an ability's, or a talent's: every talent of
-- that name, Toughness of any tree) is not rolled again until the Hero's reroll count reaches `until_reroll`
-- (25 rerolls after it). Level-up rolls do not count down.
CREATE TABLE IF NOT EXISTS `titan_reroll_blacklist` (
    `guid` INT UNSIGNED NOT NULL,
    `name` VARCHAR(100) NOT NULL,
    `until_reroll` INT UNSIGNED NOT NULL,
    PRIMARY KEY (`guid`, `name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
