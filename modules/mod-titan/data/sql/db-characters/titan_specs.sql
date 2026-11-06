-- Specializations (src/specs.cpp): a Hero's four loadouts of Wildcard abilities and talents, their names, icons and
-- locks (titan_specs), and what each holds while inactive, with its locked abilities and talents (titan_spec_entries:
-- kind 0 an ability by its first rank, 1 a talent with its ranks). The active one's rows are its locks only.
CREATE TABLE IF NOT EXISTS `titan_specs` (
    `guid` INT UNSIGNED NOT NULL,
    `spec` TINYINT UNSIGNED NOT NULL,
    `name` VARCHAR(24) NOT NULL DEFAULT '',
    `icon` VARCHAR(64) NOT NULL DEFAULT '',
    `owned` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `locked` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `used` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `granted` INT UNSIGNED NOT NULL DEFAULT 0,
    `picks` INT UNSIGNED NOT NULL DEFAULT 0,
    `capstone` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`, `spec`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `titan_spec_entries` (
    `guid` INT UNSIGNED NOT NULL,
    `spec` TINYINT UNSIGNED NOT NULL,
    `kind` TINYINT UNSIGNED NOT NULL,
    `id` INT UNSIGNED NOT NULL,
    `rank` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `locked` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`, `spec`, `kind`, `id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
