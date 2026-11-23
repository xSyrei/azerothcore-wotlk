-- The Extended Options, once (a new file runs once): picks whose meaning changed are cleared, to choose again.
-- The Hollow Congregation (30) was redesigned: its Congregation and Command slots hold new options. Elune's
-- Guardian (39): Moonlit Pursuit (Descent, choice 3) gave way to Lunar Guidance.
UPDATE `titan_runes` SET `invocation` = 0, `adaptation` = 0 WHERE `rune` = 30;
UPDATE `titan_runes_second` SET `invocation` = 0, `adaptation` = 0 WHERE `rune` = 30;
UPDATE `titan_runes` SET `manifestation` = 0 WHERE `rune` = 39 AND `manifestation` = 3;
UPDATE `titan_runes_second` SET `manifestation` = 0 WHERE `rune` = 39 AND `manifestation` = 3;
