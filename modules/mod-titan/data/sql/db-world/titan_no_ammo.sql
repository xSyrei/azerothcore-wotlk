-- No ammo: a Hero's bow, crossbow or gun needs no arrows or bullets, and its shots carry the damage of their level's
-- ammo (src/hero_equipment.cpp). Arrows and bullets (item class 6, Projectile: subclasses 2 and 3) are no longer sold.
DELETE v FROM `npc_vendor` v
JOIN `item_template` it ON it.`entry` = v.`item`
WHERE it.`class` = 6 AND it.`subclass` IN (2, 3);
