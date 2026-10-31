-- Monk spell scripts (src/monk.cpp) and the spells' share of spell power.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_monk_vivify', 'spell_titan_monk_renewing_mist',
    'spell_titan_monk_renewing_mist_cast', 'spell_titan_monk_life_cocoon', 'spell_titan_monk_revival',
    'spell_titan_monk_jade_wind_heal');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90301, 'spell_titan_monk_vivify'),
(90302, 'spell_titan_monk_renewing_mist'),
(90302, 'spell_titan_monk_renewing_mist_cast'),
(90306, 'spell_titan_monk_life_cocoon'),
(90307, 'spell_titan_monk_revival'),
(90314, 'spell_titan_monk_jade_wind_heal');

-- Mistweaver: healing grows with spell power (direct heals and each tick of the heals over time); Vivify's
-- splash and Life Cocoon are the scripts' own amounts. Crane's Gust, cast by Chi-Ji for the Monk, grows with the
-- Monk's.
DELETE FROM `spell_bonus_data` WHERE `entry` BETWEEN 90300 AND 90314;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90300, 0, 0.12, 0, 0, 'Soothing Mist'),
(90301, 0.8, 0, 0, 0, 'Vivify'),
(90302, 0, 0.09, 0, 0, 'Renewing Mist'),
(90303, 0, 0.15, 0, 0, 'Enveloping Mist'),
(90305, 0.25, 0, 0, 0, 'Essence Font - bolt'),
(90306, 0, 0, 0, 0, 'Life Cocoon'),
(90307, 0.6, 0, 0, 0, 'Revival'),
(90310, 0, 0, 0, 0, 'Vivify - splash'),
(90312, 0.3, 0, 0, 0, 'Invoke Chi-Ji - Crane''s Gust'),
(90314, 0.07, 0, 0, 0, 'Refreshing Jade Wind - heal');

-- Chi-Ji, the Red Crane: Invoke Chi-Ji's guardian (npc_titan_chi_ji), following the Monk and healing; it never
-- fights. Its own model (display 90205, titan_demon_hunter_displays.sql and the client patch).
DELETE FROM `creature_template` WHERE `entry` = 90311;
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
    `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
    `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`,
    `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
    `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`,
    `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
    `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(90311, 0, 0, 0, 0, 0, 'Chi-Ji', 'The Red Crane', NULL, 0, 1, 80, 2, 35, 0, 1.2, 1.6, 1, 1, 20, 0, 0, 1, 2000, 2000,
    1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 5, 1, 1, 0, 0, 0, 1, 0, 0, 'npc_titan_chi_ji', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` = 90311;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(90311, 0, 90205, 1, 1, 0);
