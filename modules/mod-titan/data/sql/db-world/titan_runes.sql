-- Legendary Runes (src/runes.cpp; the spells 90700-90718: client-patch/patch_runes.py, in
-- titan_demon_hunter_spells.sql). The Rune of Fel Commandment's Felguard and Fel Summons' imp, its scripts and
-- its proc.
SET @FELGUARD := 910120;
SET @IMP := 910121;
DELETE FROM `creature_template` WHERE `entry` IN (@FELGUARD, @IMP);
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
    `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
    `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`,
    `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
    `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`,
    `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
    `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(@FELGUARD, 0, 0, 0, 0, 0, 'Felguard', 'Fel Commandment', NULL, 0, 1, 60, 0, 35, 0, 1, 1.14286, 1, 1, 20, 0, 0, 1,
    2000, 2000, 1, 1, 1, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0,
    'npc_titan_rune_felguard', 0),
(@IMP, 0, 0, 0, 0, 0, 'Fel Imp', 'Fel Commandment', NULL, 0, 1, 60, 0, 35, 0, 1, 1.14286, 1, 1, 20, 0, 0, 1,
    2000, 2000, 1, 1, 8, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0,
    'npc_titan_rune_fel_imp', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (@FELGUARD, @IMP);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(@FELGUARD, 0, 14255, 1, 1, 0),
(@IMP, 0, 4449, 1, 1, 0);

-- The rune's aura grants Fel Command from its procs; Throw Glaive fires the volley.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_fel_commandment',
    'spell_titan_rune_throw_glaive');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90700, 'spell_titan_rune_fel_commandment'),
(90111, 'spell_titan_rune_throw_glaive');

-- The rune's procs: melee abilities and harmful spells that hit (normal, crit or absorbed); the script picks the
-- ones that count.
DELETE FROM `spell_proc` WHERE `SpellId` = 90700;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`,
    `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`,
    `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(90700, 0, 0, 0, 0, 0, 65552, 1, 2, 1027, 0, 0, 0, 100, 0, 0);

-- The Rune of the Sanctified Paragon: Consecration (every rank) toggles its aura; the aura's tick deals the ground's
-- damage and runs the modifiers.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_titan_rune_consecration',
    'spell_titan_rune_sanctified_paragon');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(-26573, 'spell_titan_rune_consecration'),
(90731, 'spell_titan_rune_sanctified_paragon');

-- Its damage, heal and shield carry the Hero's power already (runes.cpp ParagonTick: Consecration's 4% of spell
-- power and attack power): none of their own.
DELETE FROM `spell_bonus_data` WHERE `entry` IN (90732, 90734, 90737);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90732, 0, 0, 0, 0, 'Sanctified Paragon - Consecration tick'),
(90734, 0, 0, 0, 0, 'Sanctified Paragon - Merciful Ground'),
(90737, 0, 0, 0, 0, 'Sanctified Paragon - Shelter of the Paragon');
