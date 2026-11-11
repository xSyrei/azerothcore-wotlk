-- Prestige Gear Caches (src/prestige.cpp): the gear packed into each, by the cache's item guid. The items themselves
-- stay in item_instance (as mailed ones do), out of every inventory until the cache gives them back.
CREATE TABLE IF NOT EXISTS `titan_prestige_gear` (
    `cache_guid` INT UNSIGNED NOT NULL,
    `item_guid` INT UNSIGNED NOT NULL,
    `slot` TINYINT UNSIGNED NOT NULL,
    PRIMARY KEY (`item_guid`),
    KEY `cache_guid` (`cache_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
