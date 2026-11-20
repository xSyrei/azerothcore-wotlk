-- Riding past the level cap (60): every riding upgrade and mount that asked for a higher level asks for 60.
-- The trainers' Artisan Riding (70) and Cold Weather Flying (77); the mounts (epic flyers 70, a few 77 and 80) and
-- riding items (Riding Crop and the like, 68). Their skill requirements stay. Runs any number of times.
UPDATE `trainer_spell` SET `ReqLevel` = 60
WHERE `ReqSkillLine` = 762 AND `ReqLevel` > 60;

UPDATE `item_template` SET `RequiredLevel` = 60
WHERE `RequiredLevel` > 60 AND ((`class` = 15 AND `subclass` = 5) OR `RequiredSkill` = 762);
