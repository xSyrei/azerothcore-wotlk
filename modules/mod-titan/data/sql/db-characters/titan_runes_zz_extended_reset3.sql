-- The Extended Options, once (a new file runs once): Ursoc's Champion (41) and Whitefang (45) were redesigned;
-- their picks are cleared, to choose again.
UPDATE `titan_runes` SET `invocation` = 0, `manifestation` = 0, `adaptation` = 0 WHERE `rune` IN (41, 45);
UPDATE `titan_runes_second` SET `invocation` = 0, `manifestation` = 0, `adaptation` = 0 WHERE `rune` IN (41, 45);
