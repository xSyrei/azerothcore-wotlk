-- Scarlet Benediction (52), redesigned around the Preservation Evoker's Echo (Holy Shock, Power Word: Shield and Renew
-- Echo; a direct heal there heals again and builds Benediction), once (a new file runs once): its picks are cleared,
-- to choose again.
UPDATE `titan_runes` SET `invocation` = 0, `manifestation` = 0, `adaptation` = 0 WHERE `rune` = 52;
UPDATE `titan_runes_second` SET `invocation` = 0, `manifestation` = 0, `adaptation` = 0 WHERE `rune` = 52;
