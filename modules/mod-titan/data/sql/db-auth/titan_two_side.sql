-- Both factions as one: every player (the Player role, 199) may mail (27), see in /who (28), add as a friend (29) and
-- trade with (51) the other faction. These are RBAC permissions, not worldserver.conf settings, and the stock data
-- gives them only to staff roles. Chat, channels and groups across factions are settings (docker-compose.yml).
DELETE FROM `rbac_linked_permissions` WHERE `id` = 199 AND `linkedId` IN (27, 28, 29, 51);
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(199, 27),
(199, 28),
(199, 29),
(199, 51);
