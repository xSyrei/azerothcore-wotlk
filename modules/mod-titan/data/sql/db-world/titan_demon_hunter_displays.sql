-- Emberforge creature displays (the Demon Hunter's demon forms, Chi-Ji): written by
-- client-patch/patch_creatures.py --sql, which puts the same rows in the client patch.
DELETE FROM `creaturemodeldata_dbc` WHERE `ID` IN (3501, 3502, 3503, 3504, 3505);
INSERT INTO `creaturemodeldata_dbc` VALUES
(3501,0,'Creature\\DemonHunterMeta\\DHMetaMale.mdx',0,1.0,3,4,18.0,12.0,1.0,0,0,0,2655,0.6111,2.031,0.0,-3.87,-1.33,-0.01,0.79,1.33,6.7,0.1,1.0,0.0,0.0,0.0),
(3502,0,'Creature\\DemonHunterMeta\\DHMetaFemale.mdx',0,1.0,3,4,18.0,12.0,1.0,0,0,0,2655,0.6111,2.031,0.0,-4.43,-1.64,0.0,0.5,1.64,7.03,0.1,1.0,0.0,0.0,0.0),
(3503,0,'Creature\\DemonHunterMeta\\DHMetaMaleBladeDance.mdx',0,1.0,3,4,18.0,12.0,1.0,0,0,0,2655,0.6111,2.031,0.0,-3.87,-1.33,-0.01,0.79,1.33,6.7,0.1,1.0,0.0,0.0,0.0),
(3504,0,'Creature\\DemonHunterMeta\\DHMetaFemaleBladeDance.mdx',0,1.0,3,4,18.0,12.0,1.0,0,0,0,2655,0.6111,2.031,0.0,-4.43,-1.64,0.0,0.5,1.64,7.03,0.1,1.0,0.0,0.0,0.0),
(3505,0,'Creature\\ChiJi\\ChiJi.mdx',3,1.0,-1,-1,18.0,12.0,1.0,0,0,0,2243,0.6111,2.031,1.56762,-1.34,-0.51,0.0,1.47,0.51,3.23,0.6,1.0,0.0,0.0,0.0);

DELETE FROM `creaturedisplayinfo_dbc` WHERE `ID` IN (90201, 90202, 90203, 90204, 90205);
INSERT INTO `creaturedisplayinfo_dbc` VALUES
(90201,3501,0,0,0.6,255,'','','','',-1,0,0,0,0,0),
(90202,3502,0,0,0.6,255,'','','','',-1,0,0,0,0,0),
(90203,3503,0,0,0.6,255,'','','','',-1,0,0,0,0,0),
(90204,3504,0,0,0.6,255,'','','','',-1,0,0,0,0,0),
(90205,3505,0,0,0.8,255,'','','','',1,0,0,0,0,0);

DELETE FROM `creature_model_info` WHERE `DisplayID` IN (90201, 90202, 90203, 90204, 90205);
INSERT INTO `creature_model_info` (`DisplayID`, `BoundingRadius`, `CombatReach`, `Gender`, `DisplayID_Other_Gender`) VALUES
(90201,0.6,2.0,0,90202),
(90202,0.6,2.0,1,90201),
(90203,0.6,2.0,0,90204),
(90204,0.6,2.0,1,90203),
(90205,0.6,2.0,2,0);
