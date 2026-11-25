-- The new runes' options, once (a new file runs once): the Hungering Rift (49), Howling North (50) and Trueshot (51)
-- traded their other classes' abilities for their own (Howling North's Frostbolt, Flame Shock, Hunter's Mark,
-- Lightning Bolt, Rend and Renew options; Trueshot's Flame Shock, Frostbolt, Arcane Blast and Renew ones; the
-- Rift's Icy Touch and Moonfire ones); their picks are cleared, to choose again.
UPDATE `titan_runes` SET `invocation` = 0, `manifestation` = 0, `adaptation` = 0 WHERE `rune` IN (49, 50, 51);
UPDATE `titan_runes_second` SET `invocation` = 0, `manifestation` = 0, `adaptation` = 0 WHERE `rune` IN (49, 50, 51);
