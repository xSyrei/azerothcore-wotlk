-- Emberforge creature displays (the Demon Hunter's demon forms): written by
-- client-patch/patch_creatures.py --sql, which puts the same rows in the client patch.
DELETE FROM `creaturemodeldata_dbc` WHERE `ID` IN (3501, 3502);
INSERT INTO `creaturemodeldata_dbc` VALUES
(3501,20,'Creature\\DemonHunterMeta\\DHMetaMale.mdx',0,1.0,3,4,18.0,12.0,1.0,0,0,0,2655,0.6111,2.031,0.0,-3.87,-1.33,-0.01,0.79,1.33,6.7,0.1,1.0,0.0,0.0,0.0),
(3502,20,'Creature\\DemonHunterMeta\\DHMetaFemale.mdx',0,1.0,3,4,18.0,12.0,1.0,0,0,0,2655,0.6111,2.031,0.0,-4.43,-1.64,0.0,0.5,1.64,7.03,0.1,1.0,0.0,0.0,0.0);

DELETE FROM `creaturedisplayinfo_dbc` WHERE `ID` IN (90201, 90202);
INSERT INTO `creaturedisplayinfo_dbc` VALUES
(90201,3501,0,0,0.6,255,'','','','',-1,0,0,0,0,0),
(90202,3502,0,0,0.6,255,'','','','',-1,0,0,0,0,0);

DELETE FROM `creature_model_info` WHERE `DisplayID` IN (90201, 90202);
INSERT INTO `creature_model_info` (`DisplayID`, `BoundingRadius`, `CombatReach`, `Gender`, `DisplayID_Other_Gender`) VALUES
(90201,0.6,2.0,0,90202),
(90202,0.6,2.0,1,90201);
