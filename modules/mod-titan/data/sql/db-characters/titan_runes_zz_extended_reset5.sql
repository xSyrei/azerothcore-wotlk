-- The Extended Options, once (a new file runs once): the Stormbound Conduit (23), Echoing Thunder (24), the
-- Wellspring (25), the Dawnbound Crusader (29) and the Shattered Sun (31) were redesigned; their picks are cleared, to
-- choose again.
UPDATE `titan_runes` SET `invocation` = 0, `manifestation` = 0, `adaptation` = 0 WHERE `rune` IN (23, 24, 25, 29, 31);
UPDATE `titan_runes_second` SET `invocation` = 0, `manifestation` = 0, `adaptation` = 0
    WHERE `rune` IN (23, 24, 25, 29, 31);
