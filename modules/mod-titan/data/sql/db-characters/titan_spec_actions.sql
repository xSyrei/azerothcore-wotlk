-- Specializations' action bars (src/specs.cpp): what each inactive spec had on every action button (button 0-143,
-- the action: a spell, macro or item id, and its type), put back when the spec is activated again.
CREATE TABLE IF NOT EXISTS `titan_spec_actions` (
    `guid` INT UNSIGNED NOT NULL,
    `spec` TINYINT UNSIGNED NOT NULL,
    `button` TINYINT UNSIGNED NOT NULL,
    `action` INT UNSIGNED NOT NULL,
    `type` TINYINT UNSIGNED NOT NULL,
    PRIMARY KEY (`guid`, `spec`, `button`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
