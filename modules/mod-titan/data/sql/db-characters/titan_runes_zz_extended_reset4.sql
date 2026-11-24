-- The Extended Options, once (a new file runs once): picks whose meaning changed are cleared, to choose again.
-- The Black Covenant (48): Ravenous Siphon and Lashing Rhythm (Siphon, choices 2 and 3) gave way to Siphoning Ward
-- and Unfettered Drain. The Wandering Mist (26): Jade Shelter (Mistflow, choice 3) moved to the Approach, Gathering
-- Mists taking its place; Lingering Mist and Deep Breath (Approach, choices 2 and 3) gave way to Rolling Mist and
-- Jade Shelter. The Wild Hunt (33): Unending Wrath (its second slot, choice 1) gave way to Fixed Prey.
UPDATE `titan_runes` SET `manifestation` = 0 WHERE `rune` = 48 AND `manifestation` IN (2, 3);
UPDATE `titan_runes_second` SET `manifestation` = 0 WHERE `rune` = 48 AND `manifestation` IN (2, 3);
UPDATE `titan_runes` SET `invocation` = 0 WHERE `rune` = 26 AND `invocation` = 3;
UPDATE `titan_runes_second` SET `invocation` = 0 WHERE `rune` = 26 AND `invocation` = 3;
UPDATE `titan_runes` SET `adaptation` = 0 WHERE `rune` = 26 AND `adaptation` IN (2, 3);
UPDATE `titan_runes_second` SET `adaptation` = 0 WHERE `rune` = 26 AND `adaptation` IN (2, 3);
UPDATE `titan_runes` SET `manifestation` = 0 WHERE `rune` = 33 AND `manifestation` = 1;
UPDATE `titan_runes_second` SET `manifestation` = 0 WHERE `rune` = 33 AND `manifestation` = 1;
