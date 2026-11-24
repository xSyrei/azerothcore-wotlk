-- The Extended Options, once (a new file runs once): the Splintered Crown (35), the Duelist (42) and Ember's Wrath
-- (44) were redesigned; their picks are cleared, to choose again.
UPDATE `titan_runes` SET `invocation` = 0, `manifestation` = 0, `adaptation` = 0 WHERE `rune` IN (35, 42, 44);
UPDATE `titan_runes_second` SET `invocation` = 0, `manifestation` = 0, `adaptation` = 0 WHERE `rune` IN (35, 42, 44);
