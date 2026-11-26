-- The Stormbound Conduit (23) and the Stormreaver (43), redesigned around their own kits (the Conduit charges
-- Lightning Shield and Earth Shock releases it; the Stormreaver's Stormstrike and Lava Lash feed each other, its
-- Stormfront an option now), once (a new file runs once): their picks are cleared, to choose again.
UPDATE `titan_runes` SET `invocation` = 0, `manifestation` = 0, `adaptation` = 0 WHERE `rune` IN (23, 43);
UPDATE `titan_runes_second` SET `invocation` = 0, `manifestation` = 0, `adaptation` = 0 WHERE `rune` IN (23, 43);
