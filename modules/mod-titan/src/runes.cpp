/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "runes.h"
#include "specs.h"
#include "wildcard.h"
#include "CellImpl.h"
#include "Chat.h"
#include "Containers.h"
#include "DatabaseEnv.h"
#include "DynamicObject.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "StringFormat.h"
#include "TemporarySummon.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <algorithm>
#include <array>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

// Legendary Runes (the design: the "Emberforge Legendary Runes" doc). Twenty-one runes, each a loop (a builder
// makes a resource, a spender cashes it in) and three modifier slots (Invocation, Manifestation, Adaptation), one
// pick each, a 22nd, the Rune of the Sanctified Paragon (its own concept doc; slots Presence, Manifestation,
// Covenant), a 23rd, the Rune of the Stormbound Conduit (its own doc; slots Conductivity, Arc Pattern, Lightning
// Shield), and a 24th, the Rune of Echoing Thunder (its own doc; slots Thunderfield, Echo Pattern, Shield
// Resonance), a 25th, the Rune of the Wellspring (slots Wellspring, Current, Undertow) and a 26th, the Rune of the
// Wandering Mist (slots Mistflow, Technique, Mistweaver's Approach), each its own doc, then five more, each its own
// doc too: 27 the Threefold Grave, 28 the Unbroken Onslaught, 29 the Dawnbound Crusader, 30 the Hollow Congregation,
// 31 the Shattered Sun. Built so far: those eleven; the panel lists the doc's others as coming.
//
// - Unlocked at level 5: the panel (the Emberforge_Runes addon, addon whispers with prefix EFRN) opens by itself
//   while the active specialization has no rune, suggesting up to 3 the Hero's abilities can use, with every rune
//   under Browse all (one missing a required ability says so; it can still be bound after a warning naming what it
//   lacks), each with a Preview of its build that binds nothing.
// - One rune per specialization (titan_runes, db-characters), chosen once: only a prestige clears it. Modifiers
//   change freely out of combat.
// - The rune works through auras: its own while it is the active spec's (90700 Fel Commandment, 90730 the
//   Sanctified Paragon, 90750 the Stormbound Conduit, 90770 Echoing Thunder, 90790 the Wellspring, 90810 the
//   Wandering Mist, 90830 / 90860 / 90890 / 90920 / 90950 the five after), and one marker per picked modifier
//   (90710 / 90740 / 90760 / 90780 / 90800 / 90820, and 20 on from each of the five's first, + slot * 3 + choice);
//   scripts look for them. Two teach an action while bound: Requiem (the Threefold Grave) and Dawn Vow.
//
// Protocol (fields by ','; the client sends STATE, PICK,<rune>[,1], MOD,<slot 1-3>,<choice 1-3>; PICK's 1: warned
// of the abilities the rune needs and lacks, the Hero binds it anyway):
//   S,<level>,<unlock level>,<spec>,<rune>,<m1>,<m2>,<m3>,<offers a.b.c or ->,<prompt 0/1>
//   R,<rune>,<missing>      an implemented rune and the abilities it lacks (groups by '/', one of by '+'; '-' none)
//   N,<rune>,<slot>,<choice>,<missing>      a modifier with needs of its own
//   E,<text>   O (open the panel)
namespace
{
    constexpr char const* PREFIX = "EFRN";
    constexpr uint8 UNLOCK_LEVEL = 5;
    constexpr uint8 MAX_SPECS = 4;
    constexpr uint8 SLOTS = 3;
    constexpr uint8 CHOICES = 3;
    constexpr uint8 MAX_OFFERS = 3;

    constexpr uint8 RUNE_FEL_COMMANDMENT = 21;      // the doc's order: the 20 class runes, then the hybrid
    constexpr uint8 RUNE_SANCTIFIED_PARAGON = 22;   // its own doc
    constexpr uint8 RUNE_STORMBOUND_CONDUIT = 23;   // its own doc
    constexpr uint8 RUNE_ECHOING_THUNDER = 24;      // its own doc
    constexpr uint8 RUNE_WELLSPRING = 25;           // its own doc
    constexpr uint8 RUNE_WANDERING_MIST = 26;       // its own doc
    constexpr uint8 RUNE_THREEFOLD_GRAVE = 27;      // its own doc, and the four after it
    constexpr uint8 RUNE_UNBROKEN_ONSLAUGHT = 28;
    constexpr uint8 RUNE_DAWNBOUND_CRUSADER = 29;
    constexpr uint8 RUNE_HOLLOW_CONGREGATION = 30;
    constexpr uint8 RUNE_SHATTERED_SUN = 31;

    enum Spells : uint32
    {
        SPELL_RUNE_FEL_COMMANDMENT  = 90700,
        SPELL_FEL_COMMAND           = 90701,
        SPELL_VOLLEY_BOLT           = 90702,
        SPELL_FELGUARD_GUARDIAN     = 90703,
        SPELL_FEL_IMP_GUARDIAN      = 90704,
        SPELL_COMMANDERS_FURY       = 90705,
        SPELL_METAMORPHIC_BOND      = 90706,
        SPELL_FEL_RIFT              = 90707,
        SPELL_FELSTORM              = 90708,
        SPELL_METAMORPHIC_CLEAVE    = 90709,
        SPELL_MODIFIER_FIRST        = 90710,        // + slot * 3 + choice

        SPELL_RUNE_SANCTIFIED_PARAGON   = 90730,
        SPELL_SANCTIFIED_PARAGON        = 90731,    // the aura (Consecration turned on)
        SPELL_PARAGON_TICK              = 90732,
        SPELL_CONDEMNATION              = 90733,
        SPELL_MERCIFUL_GROUND           = 90734,
        SPELL_BASTION_OF_FAITH          = 90735,
        SPELL_WEIGHT_OF_THE_UNWORTHY    = 90736,
        SPELL_SHELTER_OF_THE_PARAGON    = 90737,
        SPELL_SEAT_OF_THE_PARAGON       = 90738,
        SPELL_HALLOWED_FOOTSTEPS        = 90739,
        SPELL_PARAGON_MODIFIER_FIRST    = 90740,    // + slot * 3 + choice
        SPELL_PARAGON_AWAKEN            = 90749,    // its look as it turns on (a shockwave and a flash)

        SPELL_RUNE_STORMBOUND_CONDUIT   = 90750,
        SPELL_CONDUCTIVE                = 90751,
        SPELL_STORMBOUND_ARC            = 90752,    // a secondary arc's damage
        SPELL_STORMBOUND_ARC_VISUAL     = 90753,    // its bolt, cast by the enemy it leaves
        SPELL_STORMBOUND_RETALIATION    = 90754,    // a Lightning Shield retaliation the rune makes
        SPELL_STORM_MODIFIER_FIRST      = 90760,    // + slot * 3 + choice

        SPELL_RUNE_ECHOING_THUNDER      = 90770,
        SPELL_THUNDERFIELD              = 90771,    // the storm on the ground (a dynamic object)
        SPELL_THUNDERFIELD_MARCH        = 90772,    // March of the Storm's: on the Hero
        SPELL_THUNDER_ECHO_WAVE         = 90773,    // the echo's cone, its look
        SPELL_THUNDER_ECHO              = 90774,
        SPELL_THUNDER_ECHO_BURST        = 90775,    // a burst about the Shield Slam target, its look
        SPELL_SHELTERING_REVERBERATION  = 90776,
        SPELL_DEAFENING_IMPACT          = 90777,
        SPELL_THUNDER_MODIFIER_FIRST    = 90780,    // + slot * 3 + choice

        SPELL_RUNE_WELLSPRING           = 90790,
        SPELL_WELLSPRING                = 90791,    // on the Earth Shield wearer
        SPELL_WELLSPRING_HEAL           = 90792,    // the additional jump
        SPELL_TIDAL_SHELTER             = 90793,
        SPELL_RENEWING_CURRENT          = 90794,
        SPELL_WELLSPRING_JUMP           = 90795,    // the jump's bolt, its look
        SPELL_WELLSPRING_MODIFIER_FIRST = 90800,    // + slot * 3 + choice
        SPELL_RUNE_WANDERING_MIST       = 90810,
        SPELL_WANDERING_MIST            = 90811,
        SPELL_LESSER_ENVELOPING_MIST    = 90812,
        SPELL_ROLLING_MIST              = 90813,
        SPELL_JADE_SHELTER              = 90814,
        SPELL_MIST_MODIFIER_FIRST       = 90820,    // + slot * 3 + choice

        SPELL_RUNE_THREEFOLD_GRAVE      = 90830,
        SPELL_REQUIEM                   = 90831,    // the action the rune teaches: the next Dominion
        SPELL_REQUIEM_FIRST             = 90832,    // Chosen Ending's three: + Dominion
        SPELL_DOMINION_FIRST            = 90835,    // the Hero's Dominion: + Dominion
        SPELL_RIME                      = 90838,
        SPELL_RIME_CHILL                = 90839,
        SPELL_BLIGHT                    = 90840,
        SPELL_RESIDUAL_INFECTION        = 90841,
        SPELL_GORE                      = 90842,
        SPELL_GRAVE_WARD                = 90843,
        SPELL_RIME_FAREWELL             = 90844,    // its look: a frost nova about each enemy
        SPELL_SCAR_FIRST                = 90845,    // the scars on the ground: + Scar
        SPELL_BLOOD_CIRCLE_MARCH        = 90848,    // March of the Dead's: on the Hero
        SPELL_CRUEL_WINTER              = 90849,
        SPELL_GRAVE_MODIFIER_FIRST      = 90850,    // + slot * 3 + choice

        SPELL_RUNE_UNBROKEN_ONSLAUGHT   = 90860,
        SPELL_WEAPON_WAKE               = 90861,    // the wake's attack
        SPELL_WAKE_BLEED                = 90862,
        SPELL_WAKE_GROUND               = 90863,    // the wake on the ground
        SPELL_WAKE_AFTERSHOCK           = 90864,
        SPELL_ONSLAUGHT_MODIFIER_FIRST  = 90880,    // + slot * 3 + choice

        SPELL_RUNE_DAWNBOUND_CRUSADER   = 90890,
        SPELL_DAWN_VOW                  = 90891,    // the action the rune teaches
        SPELL_DAWN_VOWED                = 90892,    // on the ally
        SPELL_DAWN_MARK                 = 90893,
        SPELL_DAWN_ECHO_HEAL            = 90894,
        SPELL_DAWN_ECHO_DAMAGE          = 90895,
        SPELL_DAWN_BOLT                 = 90896,    // its look from the vow: Holy Shock's, harmful
        SPELL_DAWN_BOLT_HEAL            = 90897,    // and helpful
        SPELL_SUNBURST                  = 90898,    // its look about the vow
        SPELL_SACRED_GROUND             = 90899,
        SPELL_DAWN_AFTERIMAGE           = 90900,
        SPELL_DAWN_MODIFIER_FIRST       = 90910,    // + slot * 3 + choice

        SPELL_RUNE_HOLLOW_CONGREGATION  = 90920,
        SPELL_HOLLOW_ECHO               = 90921,    // on the echo: its look, no threat
        SPELL_HOLLOW_FLAY               = 90922,
        SPELL_HOLLOW_BOLT               = 90923,
        SPELL_HOLLOW_SCREAM             = 90924,
        SPELL_HOLLOW_LAST_WORD          = 90925,
        SPELL_HOLLOW_MODIFIER_FIRST     = 90940,    // + slot * 3 + choice

        SPELL_RUNE_SHATTERED_SUN        = 90950,
        SPELL_SUNSHARD                  = 90951,
        SPELL_BROKEN_CORE               = 90952,
        SPELL_FRACTURE                  = 90953,
        SPELL_FRACTURE_LOOK             = 90954,    // a ring of ice about where it breaks
        SPELL_COLLAPSE                  = 90955,
        SPELL_COLLAPSE_LOOK             = 90956,
        SPELL_SCALDING_GROUND           = 90957,
        SPELL_SOLAR_ANCHOR              = 90958,    // a shard left on the ground
        SPELL_SUN_MODIFIER_FIRST        = 90970,    // + slot * 3 + choice

        SPELL_ICY_TOUCH             = 45477,        // first ranks
        SPELL_PLAGUE_STRIKE         = 45462,
        SPELL_DEATH_STRIKE          = 49998,
        SPELL_MORTAL_STRIKE         = 12294,
        SPELL_BLOODTHIRST           = 23881,
        SPELL_CRUSADER_STRIKE       = 35395,
        SPELL_HOLY_SHOCK            = 20473,
        SPELL_JUDGEMENT_OF_LIGHT    = 20271,
        SPELL_JUDGEMENT_OF_WISDOM   = 53408,
        SPELL_JUDGEMENT_OF_JUSTICE  = 53407,
        SPELL_MIND_FLAY             = 15407,
        SPELL_MIND_BLAST            = 8092,
        SPELL_SHADOW_WORD_PAIN      = 589,
        SPELL_CLONE_ME              = 45204,        // a copy of the caster's look (Mirror Image's)
        SPELL_FIREBALL              = 133,
        SPELL_FROSTBOLT             = 116,

        SPELL_CONSECRATION          = 26573,        // first rank
        SPELL_CHAIN_HEAL            = 1064,         // first ranks
        SPELL_EARTH_SHIELD          = 974,
        SPELL_EARTH_SHIELD_HEAL     = 379,
        SPELL_SOOTHING_MIST         = 90300,        // the Monk's (patch_monk.py)
        SPELL_VIVIFY                = 90301,
        SPELL_ENVELOPING_MIST       = 90303,
        SPELL_THUNDER_CLAP          = 6343,         // first ranks
        SPELL_SHIELD_SLAM           = 23922,
        SPELL_LIGHTNING_BOLT        = 403,          // first ranks
        SPELL_CHAIN_LIGHTNING       = 421,
        SPELL_LIGHTNING_SHIELD      = 324,
        SPELL_LIGHTNING_SHIELD_DAMAGE = 26364,

        SPELL_DEMONS_BITE           = 90100,
        SPELL_CHAOS_STRIKE          = 90101,
        SPELL_THROW_GLAIVE          = 90111,
        SPELL_METAMORPHOSIS_DH      = 90116,
        SPELL_METAMORPHOSIS_WARLOCK = 47241,
        SPELL_SHADOW_BOLT           = 686,
        SPELL_INCINERATE            = 29722,
        SPELL_SUMMON_IMP            = 688,
        SPELL_SUMMON_VOIDWALKER     = 697,
        SPELL_SUMMON_SUCCUBUS       = 712,
        SPELL_SUMMON_FELHUNTER      = 691,
        SPELL_SUMMON_FELGUARD       = 30146,
        SPELL_FIREBOLT              = 3110,
        SPELL_FELGUARD_CLEAVE       = 30213,
    };

    // Fel Commandment's modifiers: slot (0 Invocation, 1 Manifestation, 2 Adaptation) x 3 + choice.
    enum Modifier : uint8
    {
        MOD_INFERNAL_PACT, MOD_DEMONIC_ASCENSION, MOD_DREADFIRE_DISCIPLINE,
        MOD_IMP_LEGION, MOD_SHATTERED_GLAIVE, MOD_FEL_HOST,
        MOD_METAMORPHIC_BOND, MOD_FEL_RIFT, MOD_PACT_OF_RUIN,
    };

    // The Sanctified Paragon's modifiers: slot (0 Presence, 1 Manifestation, 2 Covenant) x 3 + choice.
    enum ParagonModifier : uint8
    {
        MOD_BOUNDLESS_SANCTITY, MOD_HALLOWED_FOOTSTEPS, MOD_SEAT_OF_THE_PARAGON,
        MOD_SANCTIFIED_CONDEMNATION, MOD_MERCIFUL_GROUND, MOD_SHELTER_OF_THE_PARAGON,
        MOD_BASTION_OF_FAITH, MOD_WEIGHT_OF_THE_UNWORTHY, MOD_MEASURED_DEVOTION,
    };

    // The Stormbound Conduit's modifiers: slot (0 Conductivity, 1 Arc Pattern, 2 Lightning Shield) x 3 + choice.
    enum StormModifier : uint8
    {
        MOD_PERSISTENT_CURRENT, MOD_BRANCHING_CURRENT, MOD_LIVE_WIRE,
        MOD_FORKED_TEMPEST, MOD_CLOSED_CIRCUIT, MOD_FARADAY_FIELD,
        MOD_STORM_WITHIN, MOD_VIOLENT_REPRISAL, MOD_DISTANT_CONDUCTOR,
    };

    // The Stormbound Conduit's tuning (the draft's initial values; the arc range, Storm Within's pulse and the
    // Lightning Bolt reference are ours to tune).
    constexpr int32 CONDUCTIVE_MS = 6000, PERSISTENT_CONDUCTIVE_MS = 12000;
    constexpr float ARC_RANGE = 8.0f;                           // a secondary arc's reach from the enemy it leaves
    constexpr float BRANCHING_RANGE = 8.0f, BRANCHING_ARCS = 0.8f;
    constexpr float CHAIN_ARC_SHARE = 0.15f;                    // of Lightning Bolt's damage
    constexpr float SHIELD_ARC_SHARE = 0.25f;                   // of the retaliation's damage
    constexpr std::size_t FORKED_TARGETS = 2;
    constexpr float CLOSED_CIRCUIT_SHARE = 0.5f;
    constexpr float FARADAY_REACH = 1.5f, FARADAY_ARCS = 0.85f;
    constexpr uint32 STORM_WITHIN_MS = 3000, STORM_CHECK_MS = 250;
    constexpr float STORM_WITHIN_RANGE = 8.0f;
    constexpr float VIOLENT_ARCS = 1.4f, VIOLENT_RETALIATION = 0.9f;
    constexpr uint32 LIGHTNING_SHIELD_FAMILY_FLAG = 0x400;      // SpellFamilyFlags0

    // Echoing Thunder's modifiers: slot (0 Thunderfield, 1 Echo Pattern, 2 Shield Resonance) x 3 + choice.
    enum ThunderModifier : uint8
    {
        MOD_LINGERING_THUNDER, MOD_MARCH_OF_THE_STORM, MOD_ROLLING_FRONT,
        MOD_REVERBERATING_IMPACT, MOD_FOCUSED_PEAL, MOD_DISTANT_RUMBLE,
        MOD_COMMANDING_THUNDER, MOD_SHELTERING_REVERBERATION, MOD_DEAFENING_IMPACT,
    };

    // Echoing Thunder's tuning (the revised draft's initial values: modifiers give benefits, no damage penalties;
    // the field's radius, the cone's reach and angle are ours: Thunder Clap's 8 yards, 10 yards, 90 degrees).
    constexpr uint32 THUNDERFIELD_MS = 4000, LINGERING_MS = 6000;
    constexpr float THUNDERFIELD_RADIUS = 8.0f, ROLLING_RANGE = 10.0f;
    constexpr float ECHO_SHARE = 0.15f;                         // of Shield Slam's damage before mitigation
    constexpr uint32 ECHO_INTERVAL_MS = 2000;
    constexpr std::size_t ECHO_TARGETS = 4;
    constexpr float ECHO_REACH = 10.0f, ECHO_ARC = float(M_PI) / 2.0f, REVERBERATING_RADIUS = 6.0f;
    constexpr float ROLLING_ECHO = 1.1f, FOCUSED_ECHO = 1.4f;
    constexpr float DISTANT_REACH = 1.5f, DISTANT_FAR_ECHO = 1.2f;       // beyond the cone's own reach
    constexpr float COMMANDING_THREAT = 0.5f;                           // 50% more threat
    constexpr float SHELTERING_SHARE = 0.05f;                           // of block value
    constexpr uint32 SHELTERING_INTERVAL_MS = 6000;
    constexpr float DEAFENING_ECHO = 1.1f;                              // on an enemy under the Hero's Thunder Clap

    // The Wellspring's modifiers: slot (0 Wellspring, 1 Current, 2 Undertow) x 3 + choice.
    enum WellspringModifier : uint8
    {
        MOD_FLOWING_WATERS, MOD_LASTING_SPRING, MOD_WIDENING_WATERS,
        MOD_DEEP_CURRENT, MOD_SPLIT_TRIBUTARY, MOD_CONCENTRATED_CURRENT,
        MOD_TIDAL_SHELTER, MOD_LONG_REACH, MOD_RENEWING_CURRENT,
    };

    // The Wellspring's tuning (the draft's initial values; the jump's range is Chain Heal's own).
    constexpr int32 WELLSPRING_MS = 6000, LASTING_SPRING_MS = 10000;
    constexpr float WELLSPRING_SHARE = 0.3f;                    // of the healing the Wellspring received
    constexpr float CHAIN_HEAL_JUMP = 12.5f, WIDENING_REACH = 1.5f, LONG_REACH = 5.0f;
    constexpr float DEEP_CURRENT = 1.25f, DEEP_CURRENT_BELOW_PCT = 50.0f, CONCENTRATED_CURRENT = 1.15f;
    constexpr std::size_t SPLIT_ALLIES = 2;
    constexpr float UNDERTOW_SHARE = 0.2f;                      // of the jump's effective healing
    constexpr int32 UNDERTOW_TICKS = 4;

    // The Wandering Mist's modifiers: slot (0 Mistflow, 1 Technique, 2 Mistweaver's Approach) x 3 + choice.
    enum MistModifier : uint8
    {
        MOD_FLOWING_CURRENTS, MOD_GATHERING_MISTS, MOD_DRIFTING_MIST,
        MOD_COMFORTING_VEIL, MOD_VIVACIOUS_FLOW, MOD_PEACEFUL_FOCUS,
        MOD_ROLLING_MIST, MOD_JADE_SHELTER, MOD_EFFORTLESS_BREATHING,
    };

    // The Wandering Mist's tuning (the draft's initial values).
    constexpr uint32 WANDER_CHANCE = 25, WANDER_INTERVAL_MS = 2000;
    constexpr float WANDER_SHARE = 0.3f, WANDER_RANGE = 20.0f, FLOWING_RANGE = 30.0f;
    constexpr float VEIL_SHARE = 0.15f, VIVACIOUS_SHARE = 0.2f, SECONDARY_RANGE = 20.0f, PEACEFUL_PCT = 10.0f;
    constexpr uint32 ROLLING_INTERVAL_MS = 12000, ROLLING_SWITCH_MS = 1500;
    constexpr int32 ROLLING_TICKS = 4;
    constexpr uint32 JADE_SHELTER_INTERVAL_MS = 8000;
    constexpr float JADE_SHELTER_SHARE = 0.2f;
    constexpr uint32 EFFORTLESS_AFTER_MS = 3000;
    constexpr float EFFORTLESS_SHARE = 0.15f;
    constexpr int32 SOOTHING_MIST_TICKS = 8;

    // The Threefold Grave's modifiers: slot (0 Inscription, 1 Dominion, 2 Requiem) x 3 + choice.
    enum GraveModifier : uint8
    {
        MOD_CONVERGING_GRAVES, MOD_MARCH_OF_THE_DEAD, MOD_LONG_BURIAL,
        MOD_CRUEL_WINTER, MOD_GRAVE_PESTILENCE, MOD_CRIMSON_BASTION,
        MOD_REVERSE_THE_DIRGE, MOD_LINGERING_DOMINION, MOD_CHOSEN_ENDING,
    };

    // The scars (Icy Touch's frozen line, Plague Strike's diseased patch, Death Strike's blood circle) and the
    // Dominions Requiem turns through.
    enum Scar : uint8 { SCAR_FROZEN_LINE, SCAR_DISEASED_PATCH, SCAR_BLOOD_CIRCLE, SCARS };
    enum Dominion : uint8 { DOMINION_RIME, DOMINION_BLIGHT, DOMINION_GORE, DOMINIONS };

    // The Threefold Grave's tuning (the concept draft leaves it to us). A scar's tick: the Hero's level and 6% of
    // their attack power, each enemy once a second however many scars it stands in.
    constexpr uint32 SCAR_MS = 12000, LONG_BURIAL_MS = 20000, GRAVE_TICK_MS = 1000;
    constexpr float LINE_HALF_LENGTH = 5.0f, LINE_HALF_WIDTH = 1.5f, PATCH_RADIUS = 5.0f, CIRCLE_RADIUS = 6.0f;
    constexpr float SCAR_LEVEL = 2.0f, SCAR_AP = 0.06f;
    constexpr float RIME_SHARE = 0.8f, BLIGHT_SHARE = 1.0f, GORE_SHARE = 0.9f;
    constexpr int32 RIME_SLOW = -30, CRUEL_WINTER_SLOW = -50;
    constexpr float PESTILENCE = 1.3f;                          // Blight's ticks with Grave Pestilence
    constexpr float RESIDUAL_SHARE = 0.5f;                      // a residual infection's tick, of a scar tick
    constexpr int32 RESIDUAL_MS = 3000, PESTILENCE_RESIDUAL_MS = 6000;
    constexpr float FAREWELL_BUDGET = 3.0f;                     // scar ticks, shared between the scars
    constexpr float CONVERGING_TWO = 1.3f, CONVERGING_THREE = 1.5f;
    constexpr float GRAVE_WARD_SHARE = 0.25f, GRAVE_WARD_CAP_PCT = 10.0f;
    constexpr float BASTION_WARD_SHARE = 0.4f, BASTION_WARD_CAP_PCT = 15.0f;
    constexpr int32 GRAVE_WARD_MS = 8000, PRESERVED_WARD_MS = 5000;
    constexpr float PRESERVED_WARD = 0.5f;                      // of the ward kept as Gore ends (all: Crimson Bastion)
    constexpr uint32 LINGERING_DOMINION_MS = 3000;
    constexpr uint32 CHOSEN_ENDING_MS = 20000;              // Requiem: 8 sec (its data)
    constexpr float LINGERING_OUTPUT = 0.5f;
    constexpr std::size_t SCAR_TARGETS = 10;

    // Unbroken Onslaught's modifiers: slot (0 Wake, 1 Follow-Through, 2 Pursuit) x 3 + choice.
    enum OnslaughtModifier : uint8
    {
        MOD_EXECUTIONERS_CUT, MOD_WHIRLING_STEEL, MOD_RAKING_PASSAGE,
        MOD_OVERPOWERING_ECHO, MOD_BLOODLETTING, MOD_AFTERSHOCK,
        MOD_RELENTLESS, MOD_BATTLEFIELD_CLAIM, MOD_CHANGING_PREY,
    };

    // Unbroken Onslaught's tuning: the wake's attack is 60% of a normalized main-hand swing to each enemy along it
    // (up to 6), its primary target 25% more (50% with Executioner's Cut).
    constexpr uint32 WAKE_MS = 6000, CLAIM_MS = 10000, WAKE_DELAY_MS = 600, AFTERSHOCK_MS = 1500;
    constexpr float WAKE_SHARE = 0.6f, WAKE_PRIMARY = 1.25f, EXECUTIONER_PRIMARY = 1.5f, AFTERSHOCK_SHARE = 0.5f;
    constexpr float WAKE_HALF_LENGTH = 4.0f, WAKE_HALF_WIDTH = 1.5f, CUT_HALF_WIDTH = 1.0f;
    constexpr float CRESCENT_RADIUS = 8.0f, CRESCENT_ARC = float(M_PI) / 2.0f;     // each side of the facing
    constexpr float RAKE_BEHIND = 2.0f, RAKE_BEYOND = 14.0f;
    constexpr float BLEED_SHARE = 0.3f, BLEED_CAP = 0.6f;       // of the wake's hit; at most, of a swing
    constexpr int32 BLEED_TICKS = 6;
    constexpr std::size_t WAKE_TARGETS = 6;

    // The Dawnbound Crusader's modifiers: slot (0 Doctrine, 1 Manifestation, 2 Oath) x 3 + choice.
    enum DawnModifier : uint8
    {
        MOD_MERCY, MOD_RETRIBUTION, MOD_COMMUNION,
        MOD_DAWNWAVE, MOD_SUNBURST, MOD_SACRED_GROUND,
        MOD_STEADFAST, MOD_PASSING_THE_TORCH, MOD_SHARED_PILGRIMAGE,
    };

    // The Dawnbound Crusader's tuning: an echo is half of what set it off (Crusader Strike's damage, Holy Shock's
    // damage or healing), released from the vow 0.75 sec later.
    constexpr float DAWN_SHARE = 0.5f, COMMUNION_OUTPUT = 0.85f, AFTERIMAGE_SHARE = 0.5f;
    constexpr uint32 DAWN_PENDING_MS = 750, DAWN_MARK_MS = 15000;
    constexpr float DAWN_REACH = 15.0f, PILGRIMAGE_CAP = 15.0f, PILGRIMAGE_PER_YARD = 0.5f;
    constexpr float DAWNWAVE_WIDTH = 3.0f, DAWNWAVE_SHARE = 0.6f, SUNBURST_RADIUS = 8.0f, SUNBURST_SHARE = 0.5f;
    constexpr float SACRED_RADIUS = 8.0f, SACRED_SHARE = 0.25f;     // Consecration's ground
    constexpr uint32 SACRED_MS = 4000;
    constexpr int32 AFTERIMAGE_MS = 6000;
    constexpr std::size_t DAWN_TARGETS = 5;

    // The Hollow Congregation's modifiers: slot (0 Congregation, 1 Sermon, 2 Command) x 3 + choice.
    enum HollowModifier : uint8
    {
        MOD_FOLLOWING_SHADOWS, MOD_HAUNTED_GROUND, MOD_POSSESSION,
        MOD_UNISON, MOD_DISSONANCE, MOD_ACCUSATION,
        MOD_LAST_WORD, MOD_PROCESSION, MOD_SILENT_WITNESS,
    };

    // The Hollow Congregation's tuning: an echo's channel ticks for 40% of the Hero's Mind Flay tick; a barrage is 3
    // bolts each, 0.4 sec apart, 60% of a tick each.
    constexpr uint32 FLAY_ECHO_AFTER_MS = 2000, HOLLOW_ECHO_MS = 10000, BARRAGE_SPACING_MS = 400;
    constexpr std::size_t MAX_HOLLOW_ECHOES = 2;
    constexpr uint32 BARRAGE_BOLTS = 3;
    constexpr float HOLLOW_SHARE = 0.4f, BARRAGE_SHARE = 0.6f, UNISON_BARRAGE = 1.2f, SCREAM_SHARE = 1.5f;
    constexpr float HOLLOW_RANGE = 30.0f, HAUNTED_RANGE = 40.0f, SCREAM_WIDTH = 2.0f;
    constexpr float LAST_WORD_TICKS = 3.0f, SILENT_WITNESS = 1.3f;
    constexpr std::size_t SCREAM_TARGETS = 5;

    // The Shattered Sun's modifiers: slot (0 Formation, 1 Fracture, 2 Collapse) x 3 + choice.
    enum SunModifier : uint8
    {
        MOD_TWIN_SUNS, MOD_ORBITING_SUN, MOD_SOLAR_ANCHOR,
        MOD_SPLINTERSTORM, MOD_SOLAR_LANCE, MOD_SCALDING_GROUND,
        MOD_IMPLOSION, MOD_CHAIN_REACTION, MOD_SECOND_SUNRISE,
    };

    // The Shattered Sun's tuning: a fracture is 60% of the Frostbolt's damage to each enemy in the ring (8 yards),
    // a collapse the Fireball's damage again on its host.
    constexpr std::size_t MAX_SHARDS = 2, TWIN_SUNS_SHARDS = 3, FRACTURE_TARGETS = 8;
    constexpr int32 SUNSHARD_MS = 15000, BROKEN_CORE_MS = 6000;
    constexpr float FRACTURE_SHARE = 0.6f, FRACTURE_RADIUS = 8.0f, COLLAPSE_SHARE = 1.0f, IMPLOSION = 1.4f;
    constexpr float SPLINTER_REACH = 12.0f, SPLINTER_ARC = float(M_PI) / 4.0f, LANCE_REACH = 20.0f;
    constexpr float LANCE_HALF_WIDTH = 1.5f, SCALDING_RADIUS = 6.0f, SCALDING_SHARE = 0.15f;
    constexpr uint32 SCALDING_MS = 4000, SUNRISE_DELAY_MS = 2000;
    constexpr float SUNRISE_SHARE = 0.4f, SUNRISE_PER_ENEMY = 0.1f, SUNRISE_CAP = 0.5f, CHAIN_RANGE = 15.0f;
    constexpr float CHAIN_SHARE = 0.5f, ORBIT_RANGE = 40.0f;

    // The Sanctified Paragon's tuning (the revised balance draft's initial values).
    constexpr float PARAGON_SHARE = 0.4f;                       // of a Consecration tick, a second
    constexpr float UPKEEP_PCT = 1.0f;                          // of base mana, a second
    constexpr float PARAGON_RADIUS = 8.0f, BOUNDLESS_RADIUS = 12.0f, BOUNDLESS_OUTPUT = 0.75f;
    constexpr float SUPPORT_COEFFICIENT = 0.08f;                // healing power, a heal's or shield's tick
    constexpr float FOOTSTEPS_RADIUS = 4.0f;                    // the aura's own with Hallowed Footsteps
    constexpr float TRAIL_RADIUS = 4.0f, TRAIL_SPACING = 3.0f;  // a footstep's patch; one at most every 3 yards
    constexpr uint32 TRAIL_MS = 2000;
    constexpr float MOVED = 0.3f;                               // yards between ticks that count as moving
    constexpr uint32 SEAT_MS = 3000;
    constexpr float SEAT_OUTPUT = 1.1f;
    constexpr uint32 CONDEMNATION_PCT = 2, CONDEMNATION_STACKS = 5;
    constexpr float MERCIFUL_SHARE = 1.0f;                      // of the aura's tick, healed (to tune)
    constexpr uint32 SHELTER_MS = 5000;
    constexpr float SHELTER_TICKS = 5.0f;                       // a shield of five ticks (to tune)
    constexpr float BASTION_THREAT = 0.5f;
    constexpr uint8 WEIGHT_TICKS = 3;
    constexpr float DEVOTION_UPKEEP = 0.75f, DEVOTION_OUTPUT = 0.9f;

    constexpr uint32 NPC_RUNE_FELGUARD = 910120;
    constexpr uint32 NPC_RUNE_FEL_IMP = 910121;
    constexpr uint32 NPC_HOLLOW_ECHO = 910122;
    // SummonProperties 61: category ally, type guardian. A pet-category row (67) would make a controllable guardian
    // that takes the pet slot from the Hero's own demon.
    constexpr uint32 SUMMON_PROPERTIES_GUARDIAN = 61;
    // The warlock's demons (creature entries of their summons), whose hits feed the rune.
    constexpr std::array<uint32, 5> DEMONS = { 416, 1860, 1863, 417, 17252 };

    // Tuning (the doc's table).
    constexpr uint8 MAX_STACKS = 5;
    constexpr uint32 DEMON_STACK_INTERVAL_MS = 4000;
    constexpr uint32 INFERNAL_PACT_INTERVAL_MS = 2000;
    constexpr uint32 BOLTS = 3, BOLTS_EMPOWERED = 5, PACT_OF_RUIN_BOLTS = 2;
    constexpr Milliseconds BOLT_SPACING = 150ms;
    constexpr float BOLT_RANGE = 10.0f;             // the volley's other targets, around the glaive's
    constexpr float BOLT_SHARE = 0.6f;              // of a Chaos Bolt
    constexpr float CHAOS_BOLT_COEFFICIENT = 0.7143f;
    constexpr float CHAOS_BOLT_RANK1_AVERAGE = 949.0f;      // 50796: 837 to 1061, level 60
    constexpr uint8 CHAOS_BOLT_RANK1_LEVEL = 60;
    constexpr uint32 FELGUARD_DURATION_MS = 8000;
    constexpr uint32 FEL_IMP_DURATION_MS = 10000;
    // Imp Legion and Fel Host: every Throw Glaive spends the Fel Command, a demon (10 sec) for each stack, no limit
    // to how many are out; each bolt of a volley deals 10% more for every one of the Hero's demons out.
    constexpr uint32 LEGION_DURATION_MS = 10000;
    constexpr uint32 BOLT_PCT_PER_DEMON = 10;
    constexpr int32 SHATTER_CUT_MS = 3000, SHATTER_BUDGET_MS = 9000;
    constexpr uint32 PACT_OF_RUIN_PCT = 3;
    constexpr float BOND_CLEAVE_RANGE = 5.0f;
    constexpr uint32 BOND_CLEAVE_TARGETS = 2;
    constexpr uint32 BOND_CHECK_MS = 500;

    // A rune: what it needs to be offered (each group: one of its spells, by first rank) and the roles it is
    // suggested to.
    struct RuneDef
    {
        uint8 Id;
        uint8 Roles;
        std::vector<std::vector<uint32>> Requires;
        uint32 Aura;                // worn while it is the active spec's rune
        uint32 MarkerFirst;         // its modifiers' markers: + slot * 3 + choice
    };

    std::vector<RuneDef> const& Catalog()
    {
        static std::vector<RuneDef> const runes = {
            { RUNE_FEL_COMMANDMENT,
              uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE) | Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              // Throw Glaive (it spends the stacks) and any one generator.
              { { SPELL_THROW_GLAIVE },
                { SPELL_DEMONS_BITE, SPELL_CHAOS_STRIKE, SPELL_SHADOW_BOLT, SPELL_INCINERATE } },
              SPELL_RUNE_FEL_COMMANDMENT, SPELL_MODIFIER_FIRST },
            // Every role: a damage aura, a defensive zone or a source of healing, as its modifiers make it.
            { RUNE_SANCTIFIED_PARAGON,
              uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE) | Wildcard::RoleMask(Wildcard::ROLE_RANGED)
                  | Wildcard::RoleMask(Wildcard::ROLE_HEALER) | Wildcard::RoleMask(Wildcard::ROLE_TANK)),
              { { SPELL_CONSECRATION } }, SPELL_RUNE_SANCTIFIED_PARAGON, SPELL_PARAGON_MODIFIER_FIRST },
            // DPS only: Lightning Bolt makes the connections, Lightning Shield's retaliations (and Chain Lightning,
            // if known) arc through them.
            { RUNE_STORMBOUND_CONDUIT,
              uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE) | Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_LIGHTNING_BOLT }, { SPELL_LIGHTNING_SHIELD } }, SPELL_RUNE_STORMBOUND_CONDUIT,
              SPELL_STORM_MODIFIER_FIRST },
            // Tanks: Thunder Clap places the storm, Shield Slam echoes it (and needs the shield the rune asks for).
            { RUNE_ECHOING_THUNDER, uint8(Wildcard::RoleMask(Wildcard::ROLE_TANK)),
              { { SPELL_THUNDER_CLAP }, { SPELL_SHIELD_SLAM } }, SPELL_RUNE_ECHOING_THUNDER,
              SPELL_THUNDER_MODIFIER_FIRST },
            // Healers: Chain Heal through the Earth Shield wearer; Soothing Mist's channel.
            { RUNE_WELLSPRING, uint8(Wildcard::RoleMask(Wildcard::ROLE_HEALER)),
              { { SPELL_CHAIN_HEAL }, { SPELL_EARTH_SHIELD } }, SPELL_RUNE_WELLSPRING,
              SPELL_WELLSPRING_MODIFIER_FIRST },
            { RUNE_WANDERING_MIST, uint8(Wildcard::RoleMask(Wildcard::ROLE_HEALER)),
              { { SPELL_SOOTHING_MIST } }, SPELL_RUNE_WANDERING_MIST, SPELL_MIST_MODIFIER_FIRST },
            // DPS or Tank: Death Strike's blood circle and one of the other two scars.
            { RUNE_THREEFOLD_GRAVE,
              uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE) | Wildcard::RoleMask(Wildcard::ROLE_TANK)),
              { { SPELL_DEATH_STRIKE }, { SPELL_ICY_TOUCH, SPELL_PLAGUE_STRIKE } }, SPELL_RUNE_THREEFOLD_GRAVE,
              SPELL_GRAVE_MODIFIER_FIRST },
            // Melee DPS: Mortal Strike or Bloodthirst cuts the wake.
            { RUNE_UNBROKEN_ONSLAUGHT, uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE)),
              { { SPELL_MORTAL_STRIKE, SPELL_BLOODTHIRST } }, SPELL_RUNE_UNBROKEN_ONSLAUGHT,
              SPELL_ONSLAUGHT_MODIFIER_FIRST },
            // Healer or melee DPS: Crusader Strike (on the Judgment target) or Holy Shock makes the echoes.
            { RUNE_DAWNBOUND_CRUSADER,
              uint8(Wildcard::RoleMask(Wildcard::ROLE_HEALER) | Wildcard::RoleMask(Wildcard::ROLE_MELEE)),
              { { SPELL_CRUSADER_STRIKE, SPELL_HOLY_SHOCK } }, SPELL_RUNE_DAWNBOUND_CRUSADER,
              SPELL_DAWN_MODIFIER_FIRST },
            // Ranged DPS: Mind Flay leaves the echoes, Mind Blast commands them.
            { RUNE_HOLLOW_CONGREGATION, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_MIND_FLAY }, { SPELL_MIND_BLAST } }, SPELL_RUNE_HOLLOW_CONGREGATION,
              SPELL_HOLLOW_MODIFIER_FIRST },
            // Ranged DPS: Fireball embeds and collapses, Frostbolt fractures.
            { RUNE_SHATTERED_SUN, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_FIREBALL }, { SPELL_FROSTBOLT } }, SPELL_RUNE_SHATTERED_SUN, SPELL_SUN_MODIFIER_FIRST },
        };
        return runes;
    }

    RuneDef const* FindRune(uint8 id)
    {
        for (RuneDef const& rune : Catalog())
            if (rune.Id == id)
                return &rune;
        return nullptr;
    }

    // A modifier's own needs (Dreadfire Discipline: Shadow Bolt or Incinerate; Infernal Pact and Metamorphic Bond:
    // a demon to summon; Closed Circuit: Chain Lightning).
    std::vector<std::vector<uint32>> ModifierNeeds(uint8 rune, uint8 slot, uint8 choice)
    {
        uint8 modifier = slot * CHOICES + choice;
        if (rune == RUNE_STORMBOUND_CONDUIT && modifier == MOD_CLOSED_CIRCUIT)
            return { { SPELL_CHAIN_LIGHTNING } };
        if (rune == RUNE_WANDERING_MIST && (modifier == MOD_COMFORTING_VEIL || modifier == MOD_PEACEFUL_FOCUS))
            return { { SPELL_ENVELOPING_MIST } };
        if (rune == RUNE_WANDERING_MIST && modifier == MOD_VIVACIOUS_FLOW)
            return { { SPELL_VIVIFY } };
        if (rune == RUNE_DAWNBOUND_CRUSADER && modifier == MOD_STEADFAST)
            return { { SPELL_HOLY_SHOCK } };
        if (rune == RUNE_HOLLOW_CONGREGATION && modifier == MOD_DISSONANCE)
            return { { SPELL_SHADOW_WORD_PAIN } };
        if (rune != RUNE_FEL_COMMANDMENT)
            return {};
        if (modifier == MOD_DREADFIRE_DISCIPLINE)
            return { { SPELL_SHADOW_BOLT, SPELL_INCINERATE } };
        if (modifier == MOD_INFERNAL_PACT || modifier == MOD_METAMORPHIC_BOND)
            return { { SPELL_SUMMON_IMP, SPELL_SUMMON_VOIDWALKER, SPELL_SUMMON_SUCCUBUS, SPELL_SUMMON_FELHUNTER,
                       SPELL_SUMMON_FELGUARD } };
        return {};
    }

    struct Choice
    {
        uint8 Rune = 0;
        std::array<uint8, SLOTS> Mods = {};         // 1 to 3; 0 none yet
    };

    struct State
    {
        std::array<Choice, MAX_SPECS> Choices;
        uint32 LastDemonStack = 0;                  // game ms
        uint32 BondTimer = 0;
        int32 ShatterBudget = 0;
        ObjectGuid Felguard;
        std::vector<ObjectGuid> Imps;
        ObjectGuid RiftTarget;

        // The Sanctified Paragon.
        Position LastPosition;
        bool HasLastPosition = false;
        uint32 StillSince = 0;                      // game ms
        uint32 LastShelter = 0;
        ObjectGuid Sheltered;                                       // the one ally holding its shield
        std::vector<std::pair<Position, uint32>> Trail;             // footsteps: where, when
        std::unordered_map<ObjectGuid, uint8> Weighed;              // enemy -> damaging ticks in a row

        // The Stormbound Conduit.
        uint32 StormTimer = 0;                      // ms since Storm Within's last pulse
        bool Pulsing = false;                       // a Storm Within pulse is landing: it never arcs

        // Echoing Thunder.
        uint32 ThunderMap = 0;
        Position ThunderPos;
        uint32 ThunderUntil = 0;                    // game ms
        bool ThunderFollows = false;                // March of the Storm
        uint32 LastEcho = 0;
        uint32 LastSheltering = 0;

        // The Wellspring.
        ObjectGuid Wellspring;

        // The Wandering Mist.
        ObjectGuid MistTarget;                      // Soothing Mist's channel target
        uint32 MistSince = 0;                       // the channel's start, game ms
        uint32 MistLastTick = 0;
        int32 MistTickAmount = 0;
        uint32 LastWander = 0, LastRolling = 0, LastJadeShelter = 0;
        ObjectGuid LastWanderTarget;                // the most recently healed Wandering Mist target
        ObjectGuid LesserTarget;                    // the lesser Enveloping Mist's (Comforting Veil)

        // The Threefold Grave: each scar a line From..To or a circle at From (Until 0: none).
        struct ScarAt
        {
            uint32 Map = 0;
            Position From, To;
            uint32 Until = 0;                       // game ms
        };
        std::array<ScarAt, SCARS> Scars;
        uint8 Dominion = DOMINION_RIME;
        uint8 Lingering = DOMINIONS;                // Lingering Dominion: the last one, until LingerUntil
        uint32 LingerUntil = 0;
        uint32 GraveTimer = 0;
        std::set<ObjectGuid> InBlight;              // enemies Blight touched on the last tick

        // Unbroken Onslaught: the wake, a line From..To or a crescent at From facing Facing.
        bool Wake = false, WakeCrescent = false;
        uint32 WakeMap = 0;
        Position WakeFrom, WakeTo;
        float WakeFacing = 0.0f, WakeHalfWidth = 0.0f;
        uint32 WakeUntil = 0, WakeCreator = 0, WakeTimer = 0;
        ObjectGuid WakePrimary;
        Position WakeOffset;                        // Relentless: from its target, as cut

        // The Dawnbound Crusader.
        ObjectGuid Vow, Marked;
        bool Pending = false;
        uint32 PendingAt = 0;                       // game ms the pending echo releases
        float PendingAmount = 0.0f;
        bool PendingHeal = false;
        uint32 Echoes = 0;                          // echoes released (Communion alternates)
        Position HeroWas, VowWas;
        float Pilgrimage = 0.0f;                    // Shared Pilgrimage's yards
        uint32 DawnTimer = 0;
        Position SacredAt;                          // Sacred Ground's patch (SacredUntil 0: none)
        uint32 SacredMap = 0, SacredUntil = 0, SacredTimer = 0;
        float SacredTick = 0.0f;
        bool SacredHeal = false;

        // The Hollow Congregation.
        std::vector<ObjectGuid> HollowEchoes;       // oldest first
        uint32 FlayStart = 0;
        bool FlayEchoed = true;
        ObjectGuid FlayTarget;
        std::unordered_map<ObjectGuid, ObjectGuid> EchoHosts;      // Possession: echo -> its enemy

        // The Shattered Sun: the enemies holding a shard, oldest first, and Solar Anchor's places.
        std::vector<ObjectGuid> Shards;
        std::unordered_map<ObjectGuid, Position> Anchors;
        Position ScaldAt;                           // Scalding Ground (ScaldUntil 0: none)
        uint32 ScaldMap = 0, ScaldUntil = 0, ScaldTimer = 0;
        float ScaldTick = 0.0f;
    };

    std::unordered_map<ObjectGuid::LowType, State> states;

    State& StateOf(Player* player)
    {
        return states[player->GetGUID().GetCounter()];
    }

    Choice& ActiveChoice(Player* player)
    {
        return StateOf(player).Choices[Specs::ActiveSpec(player)];
    }

    // The spell or any of its ranks.
    bool Knows(Player* player, uint32 firstRank)
    {
        for (uint32 id = firstRank; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (player->HasSpell(id))
                return true;
        return false;
    }

    // The groups the player meets none of, as the protocol writes them ('-' when none).
    std::string Missing(Player* player, std::vector<std::vector<uint32>> const& groups)
    {
        std::string out;
        for (std::vector<uint32> const& group : groups)
        {
            if (std::any_of(group.begin(), group.end(), [player](uint32 id) { return Knows(player, id); }))
                continue;
            std::string ids;
            for (uint32 id : group)
                ids += (ids.empty() ? "" : "+") + std::to_string(id);
            out += (out.empty() ? "" : "/") + ids;
        }
        return out.empty() ? "-" : out;
    }

    bool HasMarker(Unit* unit, uint8 modifier)
    {
        return unit && unit->HasAura(SPELL_MODIFIER_FIRST + modifier);
    }

    bool HasParagonMarker(Unit* unit, uint8 modifier)
    {
        return unit && unit->HasAura(SPELL_PARAGON_MODIFIER_FIRST + modifier);
    }

    bool InMetamorphosis(Unit* unit)
    {
        return unit->HasAura(SPELL_METAMORPHOSIS_DH) || unit->HasAura(SPELL_METAMORPHOSIS_WARLOCK);
    }

    void Send(Player* player, std::string const& payload)
    {
        WorldPacket data;
        std::string const message = std::string(PREFIX) + '\t' + payload;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, message);
        player->SendDirectMessage(&data);
    }

    // ---- Fel Commandment ------------------------------------------------------------------------------------

    uint8 Stacks(Player* player)
    {
        Aura* aura = player->GetAura(SPELL_FEL_COMMAND);
        return aura ? aura->GetStackAmount() : 0;
    }

    // Fel Command gained: up to 5; every new stack restarts the 20 sec, stacks past 5 are wasted.
    void Gain(Player* player, uint8 count)
    {
        if (!count || !player->HasAura(SPELL_RUNE_FEL_COMMANDMENT))
            return;
        uint8 have = Stacks(player);
        if (have >= MAX_STACKS)
            return;
        Aura* aura = player->GetAura(SPELL_FEL_COMMAND);
        if (!aura)
            aura = player->AddAura(SPELL_FEL_COMMAND, player);
        if (!aura)
            return;
        aura->SetStackAmount(std::min<uint8>(MAX_STACKS, have + count));
        aura->RefreshDuration();
    }

    // A Chaos Bolt of the Hero's level (rank 1's, scaled by a creature's damage at that level below 60) x 0.6, plus
    // the higher of spell power and half the attack power x Chaos Bolt's coefficient x 0.6.
    int32 BoltDamage(Player* player)
    {
        uint8 level = std::min<uint8>(player->GetLevel(), CHAOS_BOLT_RANK1_LEVEL);
        float scale = 1.0f;
        CreatureBaseStats const* there = sObjectMgr->GetCreatureBaseStats(level, CLASS_WARRIOR);
        CreatureBaseStats const* top = sObjectMgr->GetCreatureBaseStats(CHAOS_BOLT_RANK1_LEVEL, CLASS_WARRIOR);
        if (there && top && top->BaseDamage[0] > 0.0f)
            scale = there->BaseDamage[0] / top->BaseDamage[0];
        float power = std::max(float(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE)),
            player->GetTotalAttackPowerValue(BASE_ATTACK) / 2.0f);
        float damage = (CHAOS_BOLT_RANK1_AVERAGE * scale + power * CHAOS_BOLT_COEFFICIENT) * BOLT_SHARE;
        return std::max<int32>(1, int32(damage));
    }

    // Enemies within range of a point the player may hit: in sight, attackable, not crowd-controlled, nearest first.
    std::vector<Unit*> EnemiesAround(Player* player, WorldObject* center, float range)
    {
        std::list<Unit*> found;
        Acore::AnyUnfriendlyUnitInObjectRangeCheck check(center, player, range);
        Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(center, found, check);
        Cell::VisitObjects(center, searcher, range);
        std::vector<Unit*> enemies;
        for (Unit* unit : found)
            if (unit->IsAlive() && player->IsValidAttackTarget(unit) && !unit->HasBreakableByDamageCrowdControlAura()
                && player->IsWithinLOSInMap(unit))
                enemies.push_back(unit);
        std::sort(enemies.begin(), enemies.end(), [center](Unit* a, Unit* b)
        {
            return center->GetExactDist(a) < center->GetExactDist(b);
        });
        return enemies;
    }

    Creature* Felguard(Player* player)
    {
        State& state = StateOf(player);
        Creature* felguard = state.Felguard ? ObjectAccessor::GetCreature(*player, state.Felguard) : nullptr;
        return felguard && felguard->IsAlive() ? felguard : nullptr;
    }

    // The player's warlock demon (its hits feed the rune), if out.
    Pet* Demon(Player* player)
    {
        Pet* pet = player->GetPet();
        return pet && pet->IsAlive() && std::find(DEMONS.begin(), DEMONS.end(), pet->GetEntry()) != DEMONS.end()
            ? pet : nullptr;
    }

    void SummonFelguard(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        if (Creature* felguard = Felguard(player))
        {
            if (TempSummon* summon = felguard->ToTempSummon())
                summon->SetTimer(FELGUARD_DURATION_MS);         // one at a time: the next resets its 8 sec
            return;
        }
        Position pos = player->GetNearPosition(2.0f, float(M_PI));
        TempSummon* felguard = player->GetMap()->SummonCreature(NPC_RUNE_FELGUARD, pos,
            sSummonPropertiesStore.LookupEntry(SUMMON_PROPERTIES_GUARDIAN), FELGUARD_DURATION_MS, player);
        if (!felguard)
            return;
        state.Felguard = felguard->GetGUID();
        if (felguard->IsAIEnabled && target)
            felguard->AI()->AttackStart(target);
    }

    bool Legion(Player* player)
    {
        return HasMarker(player, MOD_IMP_LEGION) || HasMarker(player, MOD_FEL_HOST);
    }

    // The legion's demons still out (Imp Legion's imps, Fel Host's Felguards), the dead ones dropped.
    std::size_t LegionOut(Player* player)
    {
        State& state = StateOf(player);
        state.Imps.erase(std::remove_if(state.Imps.begin(), state.Imps.end(), [player](ObjectGuid guid)
        {
            Creature* demon = ObjectAccessor::GetCreature(*player, guid);
            return !demon || !demon->IsAlive();
        }), state.Imps.end());
        return state.Imps.size();
    }

    // One of the legion's demons by the target (an imp; a Felguard with felguard), for LEGION_DURATION_MS.
    void SummonDemon(Player* player, Unit* target, bool felguard)
    {
        State& state = StateOf(player);
        Position pos = target->GetNearPosition(frand(3.0f, 6.0f), target->GetAngle(player) + frand(-1.2f, 1.2f));
        TempSummon* demon = player->GetMap()->SummonCreature(felguard ? NPC_RUNE_FELGUARD : NPC_RUNE_FEL_IMP, pos,
            sSummonPropertiesStore.LookupEntry(SUMMON_PROPERTIES_GUARDIAN), LEGION_DURATION_MS, player);
        if (!demon)
            return;
        state.Imps.push_back(demon->GetGUID());
        if (demon->IsAIEnabled)
            demon->AI()->AttackStart(target);
    }

    // Every demon of the Hero's out: the legion's, the volley's Felguard and their own warlock demon.
    std::size_t DemonsOut(Player* player)
    {
        return LegionOut(player) + (Felguard(player) ? 1 : 0) + (Demon(player) ? 1 : 0);
    }

    void ClearLegends(Player* player);

    // Stacks, the Felguard and the imps gone (death, prestige, a spec or modifier change).
    void ClearCombat(Player* player)
    {
        ClearLegends(player);
        player->RemoveAurasDueToSpell(SPELL_FEL_COMMAND);
        player->RemoveAurasDueToSpell(SPELL_FEL_RIFT);
        State& state = StateOf(player);
        std::vector<ObjectGuid> summons = state.Imps;
        summons.push_back(state.Felguard);
        for (ObjectGuid guid : summons)
            if (Creature* creature = guid ? ObjectAccessor::GetCreature(*player, guid) : nullptr)
                if (TempSummon* summon = creature->ToTempSummon())
                    summon->UnSummon();
        state.Imps.clear();
        state.Felguard.Clear();
        state.RiftTarget.Clear();
        for (uint32 spell : { SPELL_SANCTIFIED_PARAGON, SPELL_SEAT_OF_THE_PARAGON, SPELL_BASTION_OF_FAITH })
            player->RemoveAurasDueToSpell(spell);
        state.Trail.clear();
        state.Weighed.clear();
        state.HasLastPosition = false;
        if (Pet* demon = player->GetPet())
        {
            demon->RemoveAurasDueToSpell(SPELL_METAMORPHIC_BOND);
            demon->RemoveAurasDueToSpell(SPELL_COMMANDERS_FURY);
        }
    }

    struct Volley
    {
        ObjectGuid Primary;
        std::vector<ObjectGuid> Targets;            // one per bolt, in order
        std::vector<Position> LastSeen;
        std::set<ObjectGuid> Hit;
        bool Stopped = false;
    };

    void FireBolt(Player* player, std::shared_ptr<Volley> volley, std::size_t index)
    {
        if (volley->Stopped || !player->IsInWorld() || !player->IsAlive())
            return;
        Unit* target = ObjectAccessor::GetUnit(*player, volley->Targets[index]);
        if (!target || !target->IsAlive() || !player->IsValidAttackTarget(target))
        {
            // Its target died: the nearest other enemy within 10 yards of where it was, else the bolt fizzles.
            Position const& where = volley->LastSeen[index];
            target = nullptr;
            float best = BOLT_RANGE;
            for (Unit* enemy : EnemiesAround(player, player, player->GetExactDist(&where) + BOLT_RANGE))
                if (float dist = enemy->GetExactDist(&where); dist <= best)
                {
                    best = dist;
                    target = enemy;
                }
            if (!target)
                return;
        }

        if (HasMarker(player, MOD_PACT_OF_RUIN))
        {
            uint32 cost = player->CountPctFromMaxHealth(PACT_OF_RUIN_PCT);
            if (player->GetHealth() <= cost)
            {
                volley->Stopped = true;             // never kills: this bolt and the rest are not fired
                return;
            }
            player->ModifyHealth(-int32(cost));
        }

        int32 damage = BoltDamage(player);
        if (Legion(player))
            damage += damage * int32(BOLT_PCT_PER_DEMON * DemonsOut(player)) / 100;
        player->CastCustomSpell(target, SPELL_VOLLEY_BOLT, &damage, nullptr, nullptr, true);

        if (!volley->Hit.insert(target->GetGUID()).second)
            return;
        if (HasMarker(player, MOD_SHATTERED_GLAIVE) && target->GetGUID() != volley->Primary)
            player->CastSpell(target, SPELL_THROW_GLAIVE, true);    // the glaive bounces to it, full damage
    }

    // Chaos Bolt Volley: at 5 Fel Command, Throw Glaive also fires 3 bolts (5 in Metamorphosis, then the Felguard;
    // Pact of Ruin 2 more), 0.15 sec apart: the first at the glaive's target, each next at another enemy within
    // 10 yards of it, nearest first; extra bolts go to the target.
    void FireVolley(Player* player, Unit* primary)
    {
        player->RemoveAurasDueToSpell(SPELL_FEL_COMMAND);
        bool empowered = InMetamorphosis(player);
        uint32 bolts = (empowered ? BOLTS_EMPOWERED : BOLTS)
            + (HasMarker(player, MOD_PACT_OF_RUIN) ? PACT_OF_RUIN_BOLTS : 0);

        auto volley = std::make_shared<Volley>();
        volley->Primary = primary->GetGUID();
        std::vector<Unit*> others = EnemiesAround(player, primary, BOLT_RANGE);
        others.erase(std::remove(others.begin(), others.end(), primary), others.end());
        for (uint32 i = 0; i < bolts; ++i)
        {
            Unit* target = (i > 0 && i - 1 < others.size()) ? others[i - 1] : primary;
            volley->Targets.push_back(target->GetGUID());
            volley->LastSeen.push_back(target->GetPosition());
        }

        State& state = StateOf(player);
        state.ShatterBudget = SHATTER_BUDGET_MS;
        for (std::size_t i = 0; i < volley->Targets.size(); ++i)
            player->m_Events.AddEventAtOffset([player, volley, i]() { FireBolt(player, volley, i); },
                Milliseconds(BOLT_SPACING.count() * i));

        if (empowered)
            SummonFelguard(player, primary);
        if (HasMarker(player, MOD_FEL_RIFT))
        {
            state.RiftTarget = primary->GetGUID();
            player->AddAura(SPELL_FEL_RIFT, player);
        }
    }

    // ---- The Sanctified Paragon -------------------------------------------------------------------------------

    // The Hero's best Consecration rank they know, else 0.
    uint32 ConsecrationRank(Player* player)
    {
        uint32 best = 0;
        for (uint32 id = SPELL_CONSECRATION; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (player->HasSpell(id))
                best = id;
        return best;
    }

    // A tick of the Hero's Consecration before the target's mitigation, as the core makes one: their rank's tick
    // with Consecration's share of spell power and attack power (spell_bonus_data) and every talent, glyph and
    // bonus that raises Consecration or Holy damage. The damage, heal and shield spells add no power of their own
    // (spell_bonus_data 0): the modifiers scale all of it.
    float ParagonTick(Player* player)
    {
        SpellInfo const* rank = sSpellMgr->GetSpellInfo(ConsecrationRank(player));
        if (!rank)
            return 1.0f;
        int32 points = std::max(1, rank->Effects[EFFECT_0].CalcValue(player));
        return float(player->SpellDamageBonusDone(player, rank, uint32(points), DOT, EFFECT_0));
    }

    // Merciful Ground's heal and Shelter of the Paragon's shield: the tick (its talents and power), and the Hero's
    // healing power on top (SUPPORT_COEFFICIENT a tick). A shield takes their healing done bonuses here (an absorb
    // gets none from the core); the heal gets them as it lands.
    float ParagonSupport(Player* player, float output, bool shield)
    {
        float healing = float(player->SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_HOLY)) * SUPPORT_COEFFICIENT
            * PARAGON_SHARE;
        float done = shield ? player->GetTotalAuraMultiplier(SPELL_AURA_MOD_HEALING_DONE_PERCENT) : 1.0f;
        return (output + healing) * done;
    }

    // Allies of the Hero's within range (themselves first, then their group's, nearest first), alive.
    std::vector<Unit*> AlliesAround(Player* player, float range)
    {
        std::vector<Unit*> allies = { player };
        if (Group* group = player->GetGroup())
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                if (Player* member = ref->GetSource())
                    if (member != player && member->IsInMap(player) && member->IsAlive()
                        && member->IsWithinDistInMap(player, range))
                        allies.push_back(member);
        std::sort(allies.begin() + 1, allies.end(), [player](Unit* a, Unit* b)
        {
            return player->GetExactDist(a) < player->GetExactDist(b);
        });
        return allies;
    }

    // The party members within range (the Hero first), alive, most injured first (by share of health).
    std::vector<Unit*> MostInjured(Player* player, float range)
    {
        std::vector<Unit*> allies = AlliesAround(player, range);
        std::stable_sort(allies.begin(), allies.end(), [](Unit* a, Unit* b)
        {
            return a->GetHealthPct() < b->GetHealthPct();
        });
        return allies;
    }

    // A second of the aura (90731's tick), the revised balance draft's: its upkeep (1% of base mana, 0.75% with
    // Measured Devotion; the aura ends when it cannot be paid), then one pulse at 40% of a Consecration tick, through
    // the Manifestation picked: damage to each enemy within 8 yards (12 with Boundless Sanctity, 4 and the
    // footsteps' patches with Hallowed Footsteps; an enemy once a pulse however many it stands in), or a heal on the
    // single most injured party member (Merciful Ground), or a shield every 5 sec on one (Shelter of the Paragon).
    // Nothing a modifier makes sets off another; toggling neither pulses at once nor resets their timers.
    // false: the upkeep could not be paid (the aura's script ends it).
    bool SanctifiedGround(Player* player)
    {
        if (!player->IsAlive() || !ConsecrationRank(player))
            return true;
        State& state = StateOf(player);
        uint32 now = GameTime::GetGameTimeMS().count();

        bool devotion = HasParagonMarker(player, MOD_MEASURED_DEVOTION);
        int32 upkeep = std::max<int32>(1, int32(player->GetCreateMana() * UPKEEP_PCT / 100.0f
            * (devotion ? DEVOTION_UPKEEP : 1.0f)));
        if (player->GetPower(POWER_MANA) < upkeep)
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cffa335eeSanctified Paragon:|r out of mana.");
            return false;
        }
        player->ModifyPower(POWER_MANA, -upkeep);

        bool boundless = HasParagonMarker(player, MOD_BOUNDLESS_SANCTITY);
        bool footsteps = HasParagonMarker(player, MOD_HALLOWED_FOOTSTEPS);
        float radius = boundless ? BOUNDLESS_RADIUS : footsteps ? FOOTSTEPS_RADIUS : PARAGON_RADIUS;

        // Moving or still (Seat of the Paragon, Hallowed Footsteps).
        Position here = player->GetPosition();
        bool moved = state.HasLastPosition && state.LastPosition.GetExactDist(&here) > MOVED;
        if (!state.HasLastPosition || moved)
            state.StillSince = now;
        if (moved && footsteps
            && (state.Trail.empty() || state.Trail.back().first.GetExactDist(&state.LastPosition) >= TRAIL_SPACING))
        {
            state.Trail.emplace_back(state.LastPosition, now);
            player->CastSpell(state.LastPosition.GetPositionX(), state.LastPosition.GetPositionY(),
                state.LastPosition.GetPositionZ(), SPELL_HALLOWED_FOOTSTEPS, true);
        }
        state.LastPosition = here;
        state.HasLastPosition = true;
        state.Trail.erase(std::remove_if(state.Trail.begin(), state.Trail.end(),
            [now](auto const& step) { return now - step.second >= TRAIL_MS; }), state.Trail.end());

        bool seated = HasParagonMarker(player, MOD_SEAT_OF_THE_PARAGON) && now - state.StillSince >= SEAT_MS;
        if (seated && !player->HasAura(SPELL_SEAT_OF_THE_PARAGON))
            player->AddAura(SPELL_SEAT_OF_THE_PARAGON, player);
        else if (!seated)
            player->RemoveAurasDueToSpell(SPELL_SEAT_OF_THE_PARAGON);

        float output = ParagonTick(player) * PARAGON_SHARE * (boundless ? BOUNDLESS_OUTPUT : 1.0f)
            * (seated ? SEAT_OUTPUT : 1.0f) * (devotion ? DEVOTION_OUTPUT : 1.0f);

        // Merciful Ground: no damage; the single most injured party member within range healed.
        if (HasParagonMarker(player, MOD_MERCIFUL_GROUND))
        {
            for (Unit* ally : MostInjured(player, radius))
                if (!ally->IsFullHealth())
                {
                    int32 heal = std::max<int32>(1, int32(ParagonSupport(player, output, false) * MERCIFUL_SHARE));
                    player->CastCustomSpell(ally, SPELL_MERCIFUL_GROUND, &heal, nullptr, nullptr, true);
                    break;
                }
            return true;
        }

        // Shelter of the Paragon: no damage; every 5 sec one party member within range (the most injured) shielded,
        // one shield of the aura's at a time (the last one's goes), none on an ally another Paragon shields.
        if (HasParagonMarker(player, MOD_SHELTER_OF_THE_PARAGON))
        {
            if (now - state.LastShelter < SHELTER_MS)
                return true;
            for (Unit* ally : MostInjured(player, radius))
            {
                if (ally->HasAura(SPELL_SHELTER_OF_THE_PARAGON) && ally->GetGUID() != state.Sheltered)
                    continue;
                if (Unit* last = state.Sheltered ? ObjectAccessor::GetUnit(*player, state.Sheltered) : nullptr)
                    last->RemoveAurasDueToSpell(SPELL_SHELTER_OF_THE_PARAGON, player->GetGUID());
                int32 shield = std::max<int32>(1, int32(ParagonSupport(player, output, true) * SHELTER_TICKS));
                player->CastCustomSpell(ally, SPELL_SHELTER_OF_THE_PARAGON, &shield, nullptr, nullptr, true);
                state.Sheltered = ally->GetGUID();
                state.LastShelter = now;
                break;
            }
            return true;
        }

        // Damage (Sanctified Condemnation, or no Manifestation yet): the enemies sanctified this second, the aura's
        // then the footsteps' (each once).
        std::vector<Unit*> enemies = EnemiesAround(player, player, radius);
        if (footsteps)
            for (auto const& step : state.Trail)
                for (Unit* enemy : EnemiesAround(player, player, player->GetExactDist(&step.first) + TRAIL_RADIUS))
                    if (enemy->GetExactDist(&step.first) <= TRAIL_RADIUS
                        && std::find(enemies.begin(), enemies.end(), enemy) == enemies.end())
                        enemies.push_back(enemy);

        bool condemnation = HasParagonMarker(player, MOD_SANCTIFIED_CONDEMNATION);
        bool weight = condemnation && HasParagonMarker(player, MOD_WEIGHT_OF_THE_UNWORTHY);
        bool bastion = condemnation && HasParagonMarker(player, MOD_BASTION_OF_FAITH);
        std::unordered_map<ObjectGuid, uint8> weighed;
        for (Unit* enemy : enemies)
        {
            float amount = output;
            if (condemnation)
                if (Aura* condemned = enemy->GetAura(SPELL_CONDEMNATION, player->GetGUID()))
                    amount *= 1.0f + CONDEMNATION_PCT * condemned->GetStackAmount() / 100.0f;
            int32 damage = std::max<int32>(1, int32(amount));
            player->CastCustomSpell(enemy, SPELL_PARAGON_TICK, &damage, nullptr, nullptr, true);
            if (!enemy->IsAlive())
                continue;
            if (condemnation)
            {
                Aura* condemned = enemy->GetAura(SPELL_CONDEMNATION, player->GetGUID());
                if (!condemned)
                    condemned = player->AddAura(SPELL_CONDEMNATION, enemy);
                else if (condemned->GetStackAmount() < CONDEMNATION_STACKS)
                    condemned->ModStackAmount(1);
                if (condemned)
                    condemned->RefreshDuration();
            }
            // Weight of the Unworthy: slowed from the third damaging tick in a row, while it stays in the aura.
            if (weight)
            {
                auto itr = state.Weighed.find(enemy->GetGUID());
                uint8 ticks = uint8(std::min<uint32>((itr != state.Weighed.end() ? itr->second : 0) + 1, 255));
                weighed[enemy->GetGUID()] = ticks;
                if (ticks >= WEIGHT_TICKS)
                    player->CastSpell(enemy, SPELL_WEIGHT_OF_THE_UNWORTHY, true);
            }
            if (bastion && enemy->CanHaveThreatList())
                enemy->AddThreat(player, float(damage) * BASTION_THREAT, SPELL_SCHOOL_MASK_HOLY);
        }
        // Those not struck this second lose the count and the slow.
        for (auto const& [guid, ticks] : state.Weighed)
            if (!weighed.count(guid))
                if (Unit* enemy = ObjectAccessor::GetUnit(*player, guid))
                    enemy->RemoveAurasDueToSpell(SPELL_WEIGHT_OF_THE_UNWORTHY, player->GetGUID());
        state.Weighed = std::move(weighed);
        return true;
    }

    // ---- The Stormbound Conduit -------------------------------------------------------------------------------

    bool HasStormMarker(Unit* unit, uint8 modifier)
    {
        return unit && unit->HasAura(SPELL_STORM_MODIFIER_FIRST + modifier);
    }

    bool IsConductive(Player* player, Unit* unit)
    {
        return unit->HasAura(SPELL_CONDUCTIVE, player->GetGUID());
    }

    // Conductive on the enemy: 6 sec (12 with Persistent Current), refreshed by a new one; never stacks.
    void MakeConductive(Player* player, Unit* target)
    {
        player->CastSpell(target, SPELL_CONDUCTIVE, true);
        if (Aura* aura = target->GetAura(SPELL_CONDUCTIVE, player->GetGUID()))
        {
            int32 ms = HasStormMarker(player, MOD_PERSISTENT_CURRENT) ? PERSISTENT_CONDUCTIVE_MS : CONDUCTIVE_MS;
            aura->SetMaxDuration(ms);
            aura->SetDuration(ms);
        }
    }

    // The player's Lightning Shield (any rank): its proc effect.
    AuraEffect* Shield(Player* player)
    {
        return player->GetAuraEffect(SPELL_AURA_PROC_TRIGGER_SPELL, SPELLFAMILY_SHAMAN, LIGHTNING_SHIELD_FAMILY_FLAG, 0,
            0, player->GetGUID());
    }

    // The damage the player's best Lightning Bolt would deal the target (its rank's, with their spell power).
    float LightningBoltDamage(Player* player, Unit* target)
    {
        SpellInfo const* best = nullptr;
        for (uint32 id = SPELL_LIGHTNING_BOLT; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (player->HasSpell(id))
                best = sSpellMgr->GetSpellInfo(id);
        if (!best)
            return 0.0f;
        int32 base = std::max<int32>(0, best->Effects[EFFECT_0].CalcValue(player));
        return float(player->SpellDamageBonusDone(target, best, uint32(base), SPELL_DIRECT_DAMAGE, EFFECT_0));
    }

    // Secondary arcs leaving source: Chain Lightning's to any enemy near it (Closed Circuit: back into source at
    // half), Lightning Shield's to a Conductive one; Forked Tempest splits one between two. An arc never arcs again
    // (its damage is a spell of its own).
    void Arc(Player* player, Unit* source, float damage, bool shield)
    {
        float range = ARC_RANGE;
        if (HasStormMarker(player, MOD_BRANCHING_CURRENT))
            damage *= BRANCHING_ARCS;
        if (HasStormMarker(player, MOD_FARADAY_FIELD))
        {
            range *= FARADAY_REACH;
            damage *= FARADAY_ARCS;
        }
        if (shield && HasStormMarker(player, MOD_VIOLENT_REPRISAL))
            damage *= VIOLENT_ARCS;

        std::vector<Unit*> targets;
        if (!shield && HasStormMarker(player, MOD_CLOSED_CIRCUIT))
        {
            targets.push_back(source);
            damage *= CLOSED_CIRCUIT_SHARE;
        }
        else
        {
            for (Unit* enemy : EnemiesAround(player, source, range))
                if (enemy != source && (!shield || IsConductive(player, enemy)))
                    targets.push_back(enemy);
            std::size_t most = HasStormMarker(player, MOD_FORKED_TEMPEST) ? FORKED_TARGETS : 1;
            if (targets.size() > most)
                targets.resize(most);
        }
        if (targets.empty() || damage < 1.0f)
            return;
        int32 each = std::max<int32>(1, int32(damage / targets.size()));
        for (Unit* target : targets)
        {
            if (target != source)
                source->CastSpell(target, SPELL_STORMBOUND_ARC_VISUAL, true);
            player->CastCustomSpell(target, SPELL_STORMBOUND_ARC, &each, nullptr, nullptr, true);
        }
    }

    // A Lightning Shield retaliation the rune makes (Distant Conductor, Storm Within): the shield's rank's damage at
    // the target, a charge spent. A pulse never arcs.
    void Retaliate(Player* player, Unit* target, AuraEffect* shield, bool pulse)
    {
        uint32 damageSpell = sSpellMgr->GetSpellWithRank(SPELL_LIGHTNING_SHIELD_DAMAGE,
            shield->GetSpellInfo()->GetRank());
        SpellInfo const* info = sSpellMgr->GetSpellInfo(damageSpell);
        if (!info)
            return;
        int32 damage = std::max<int32>(1, info->Effects[EFFECT_0].CalcValue(player));
        Aura* aura = shield->GetBase();
        State& state = StateOf(player);
        state.Pulsing = pulse;
        player->CastCustomSpell(target, SPELL_STORMBOUND_RETALIATION, &damage, nullptr, nullptr, true);
        state.Pulsing = false;
        aura->DropCharge();
    }

    // Storm Within: every 3 sec, a pulse at the nearest Conductive enemy within 8 yards (checked every 0.25 sec
    // until there is one).
    void StormPulse(Player* player, uint32 diff)
    {
        if (!HasStormMarker(player, MOD_STORM_WITHIN))
            return;
        State& state = StateOf(player);
        state.StormTimer += diff;
        if (state.StormTimer < STORM_WITHIN_MS)
            return;
        AuraEffect* shield = Shield(player);
        Unit* target = nullptr;
        if (shield && player->IsInCombat())
            for (Unit* enemy : EnemiesAround(player, player, STORM_WITHIN_RANGE))
                if (IsConductive(player, enemy))
                {
                    target = enemy;
                    break;
                }
        if (!target)
        {
            state.StormTimer = STORM_WITHIN_MS - STORM_CHECK_MS;
            return;
        }
        state.StormTimer = 0;
        Retaliate(player, target, shield, true);
    }

    // ---- Echoing Thunder ---------------------------------------------------------------------------------------

    bool HasThunderMarker(Unit* unit, uint8 modifier)
    {
        return unit && unit->HasAura(SPELL_THUNDER_MODIFIER_FIRST + modifier);
    }

    // Thunder Clap's storm: one at a time (a new one replaces it), 4 sec (6 with Lingering Thunder); beneath the
    // Hero, or their hostile target within 10 yards (Rolling Front), or following them (March of the Storm).
    void PlaceThunderfield(Player* player)
    {
        State& state = StateOf(player);
        uint32 now = GameTime::GetGameTimeMS().count();
        uint32 ms = HasThunderMarker(player, MOD_LINGERING_THUNDER) ? LINGERING_MS : THUNDERFIELD_MS;
        state.ThunderUntil = now + ms;
        state.ThunderFollows = HasThunderMarker(player, MOD_MARCH_OF_THE_STORM);
        player->RemoveAurasDueToSpell(SPELL_THUNDERFIELD_MARCH);
        player->RemoveDynObject(SPELL_THUNDERFIELD);
        if (state.ThunderFollows)
        {
            if (Aura* aura = player->AddAura(SPELL_THUNDERFIELD_MARCH, player))
            {
                aura->SetMaxDuration(int32(ms));
                aura->SetDuration(int32(ms));
            }
            return;
        }
        Position center = player->GetPosition();
        if (HasThunderMarker(player, MOD_ROLLING_FRONT))
            if (Unit* target = player->GetSelectedUnit())
                if (target->IsAlive() && player->IsValidAttackTarget(target)
                    && player->IsWithinDistInMap(target, ROLLING_RANGE))
                    center = target->GetPosition();
        state.ThunderMap = player->GetMapId();
        state.ThunderPos = center;
        player->CastSpell(center.GetPositionX(), center.GetPositionY(), center.GetPositionZ(), SPELL_THUNDERFIELD,
            true);
        if (DynamicObject* field = player->GetDynObject(SPELL_THUNDERFIELD))
            field->SetDuration(int32(ms));
    }

    bool InThunderfield(Player* player)
    {
        State& state = StateOf(player);
        if (GameTime::GetGameTimeMS().count() >= state.ThunderUntil)
            return false;
        if (state.ThunderFollows)
            return true;
        return player->GetMapId() == state.ThunderMap
            && player->GetExactDist(&state.ThunderPos) <= THUNDERFIELD_RADIUS;
    }

    // Under the Hero's Thunder Clap (any rank's slow).
    bool ThunderClapped(Player* player, Unit* enemy)
    {
        for (uint32 id = SPELL_THUNDER_CLAP; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (enemy->HasAura(id, player->GetGUID()))
                return true;
        return false;
    }

    // Thunder Echo: once every 2 sec at most, 15% of Shield Slam's damage before mitigation, as Nature damage that
    // never crits on its own, to each of up to 4 enemies once: in a 90 degree cone toward the target, 10 yards (15
    // with Distant Rumble, 20% more beyond 10); or within 6 yards of the target (Reverberating Impact); or the target
    // alone (Focused Peal, 40% more). Rolling Front: 10% more; Deafening Impact: 10% more on an enemy under the Hero's
    // Thunder Clap. Its damage never echoes again.
    void ThunderEcho(Player* player, Unit* target, int32 slamDamage)
    {
        State& state = StateOf(player);
        uint32 now = GameTime::GetGameTimeMS().count();
        if (now - state.LastEcho < ECHO_INTERVAL_MS || slamDamage <= 0)
            return;
        state.LastEcho = now;

        float damage = slamDamage * ECHO_SHARE;
        bool reverberating = HasThunderMarker(player, MOD_REVERBERATING_IMPACT);
        bool focused = HasThunderMarker(player, MOD_FOCUSED_PEAL);
        bool distant = HasThunderMarker(player, MOD_DISTANT_RUMBLE);
        if (HasThunderMarker(player, MOD_ROLLING_FRONT))
            damage *= ROLLING_ECHO;
        if (focused)
            damage *= FOCUSED_ECHO;

        std::vector<Unit*> targets;
        if (target->IsAlive())
            targets.push_back(target);
        if (!focused)
        {
            float reach = reverberating ? REVERBERATING_RADIUS : ECHO_REACH * (distant ? DISTANT_REACH : 1.0f);
            float facing = player->GetAngle(target);
            for (Unit* enemy : EnemiesAround(player, reverberating ? target : static_cast<WorldObject*>(player),
                reach))
            {
                if (targets.size() >= ECHO_TARGETS)
                    break;
                if (enemy == target)
                    continue;
                if (!reverberating)
                {
                    float diff = std::fabs(Position::NormalizeOrientation(player->GetAngle(enemy) - facing));
                    if (std::min(diff, float(2 * M_PI) - diff) > ECHO_ARC / 2.0f)
                        continue;
                }
                targets.push_back(enemy);
            }
        }

        // Its look: the wave from the Hero toward the target, or a burst about the target.
        if (reverberating || focused)
            target->CastSpell(target, SPELL_THUNDER_ECHO_BURST, true);
        else
            player->CastSpell(player, SPELL_THUNDER_ECHO_WAVE, true);

        bool commanding = HasThunderMarker(player, MOD_COMMANDING_THUNDER);
        bool deafening = HasThunderMarker(player, MOD_DEAFENING_IMPACT);
        for (Unit* enemy : targets)
        {
            float amount = damage;
            if (distant && !reverberating && player->GetExactDist(enemy) > ECHO_REACH)
                amount *= DISTANT_FAR_ECHO;
            if (deafening && ThunderClapped(player, enemy))
                amount *= DEAFENING_ECHO;
            int32 each = std::max<int32>(1, int32(amount));
            player->CastCustomSpell(enemy, SPELL_THUNDER_ECHO, &each, nullptr, nullptr, true);
            if (!enemy->IsAlive())
                continue;
            if (commanding && enemy->CanHaveThreatList())
                enemy->AddThreat(player, float(each) * COMMANDING_THREAT, SPELL_SCHOOL_MASK_NATURE);
            if (deafening)
                player->CastSpell(enemy, SPELL_DEAFENING_IMPACT, true);
        }

        // Sheltering Reverberation: an absorb of 5% of block value for 3 sec, once every 6 sec, never stacked.
        if (HasThunderMarker(player, MOD_SHELTERING_REVERBERATION)
            && now - state.LastSheltering >= SHELTERING_INTERVAL_MS)
        {
            state.LastSheltering = now;
            player->RemoveAurasDueToSpell(SPELL_SHELTERING_REVERBERATION);
            int32 absorb = std::max<int32>(1, int32(player->GetShieldBlockValue() * SHELTERING_SHARE));
            player->CastCustomSpell(player, SPELL_SHELTERING_REVERBERATION, &absorb, nullptr, nullptr, true);
        }
    }

    // ---- Healing runes: the Wellspring, the Wandering Mist ------------------------------------------------------

    bool HasWellMarker(Unit* unit, uint8 modifier)
    {
        return unit && unit->HasAura(SPELL_WELLSPRING_MODIFIER_FIRST + modifier);
    }

    bool HasMistMarker(Unit* unit, uint8 modifier)
    {
        return unit && unit->HasAura(SPELL_MIST_MODIFIER_FIRST + modifier);
    }

    // The Hero and their party or raid members alive and injured within range of center, but except, most injured
    // first.
    std::vector<Unit*> InjuredAllies(Player* player, WorldObject* center, float range, Unit* except)
    {
        std::vector<Unit*> allies;
        auto consider = [&](Player* ally)
        {
            if (ally != except && ally->IsAlive() && !ally->IsFullHealth() && ally->IsInMap(player)
                && ally->IsWithinDistInMap(center, range))
                allies.push_back(ally);
        };
        consider(player);
        if (Group* group = player->GetGroup())
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                if (Player* member = ref->GetSource(); member && member != player)
                    consider(member);
        std::sort(allies.begin(), allies.end(), [](Unit* a, Unit* b) { return a->GetHealthPct() < b->GetHealthPct(); });
        return allies;
    }

    // A heal's effective share on the ally now (no overhealing).
    int32 Effective(Unit* ally, int32 amount)
    {
        return std::max<int32>(0, std::min<int32>(amount, int32(ally->GetMaxHealth() - ally->GetHealth())));
    }

    // An absorb or heal over time of the Hero's on the ally, refreshed rather than stacked.
    void Refresh(Player* player, Unit* ally, uint32 spellId, int32 amount)
    {
        if (amount <= 0)
            return;
        ally->RemoveAurasDueToSpell(spellId, player->GetGUID());
        player->CastCustomSpell(ally, spellId, &amount, nullptr, nullptr, true);
    }

    bool HasEarthShield(Player* player, Unit* unit)
    {
        for (uint32 id = SPELL_EARTH_SHIELD; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (unit->HasAura(id, player->GetGUID()))
                return true;
        return false;
    }

    bool IsWellspring(Player* player, Unit* unit)
    {
        return StateOf(player).Wellspring == unit->GetGUID() && unit->HasAura(SPELL_WELLSPRING, player->GetGUID());
    }

    // The Wellspring on the unit (one at a time: the last one's goes), 6 sec (10 with Lasting Spring), or refreshed.
    void MakeWellspring(Player* player, Unit* unit)
    {
        State& state = StateOf(player);
        if (state.Wellspring && state.Wellspring != unit->GetGUID())
            if (Unit* last = ObjectAccessor::GetUnit(*player, state.Wellspring))
                last->RemoveAurasDueToSpell(SPELL_WELLSPRING, player->GetGUID());
        state.Wellspring = unit->GetGUID();
        int32 ms = HasWellMarker(player, MOD_LASTING_SPRING) ? LASTING_SPRING_MS : WELLSPRING_MS;
        Aura* aura = unit->GetAura(SPELL_WELLSPRING, player->GetGUID());
        if (!aura)
            aura = player->AddAura(SPELL_WELLSPRING, unit);
        if (aura)
        {
            aura->SetMaxDuration(ms);
            aura->SetDuration(ms);
        }
    }

    // The Wellspring's additional jump: 30% of the healing it received to the most injured ally within Chain Heal's
    // jump range of it (50% farther with Widening Waters, 5 yards more with Long Reach): Split Tributary shares it
    // between two, Concentrated Current gives one 15% more, Deep Current 25% more to one below half health. Tidal
    // Shelter and Renewing Current follow up for 20% of each one's effective healing. It never jumps again.
    void WellspringJump(Player* player, Unit* holder, int32 received)
    {
        float range = CHAIN_HEAL_JUMP * (HasWellMarker(player, MOD_WIDENING_WATERS) ? WIDENING_REACH : 1.0f)
            + (HasWellMarker(player, MOD_LONG_REACH) ? LONG_REACH : 0.0f);
        std::vector<Unit*> allies = InjuredAllies(player, holder, range, holder);
        if (allies.empty() || received <= 0)
            return;
        bool split = HasWellMarker(player, MOD_SPLIT_TRIBUTARY);
        if (allies.size() > (split ? SPLIT_ALLIES : 1))
            allies.resize(split ? SPLIT_ALLIES : 1);
        float base = received * WELLSPRING_SHARE / float(allies.size());
        if (HasWellMarker(player, MOD_CONCENTRATED_CURRENT))
            base *= CONCENTRATED_CURRENT;
        for (Unit* ally : allies)
        {
            float amount = base;
            if (HasWellMarker(player, MOD_DEEP_CURRENT) && ally->GetHealthPct() < DEEP_CURRENT_BELOW_PCT)
                amount *= DEEP_CURRENT;
            int32 heal = std::max<int32>(1, int32(amount));
            int32 effective = Effective(ally, heal);
            holder->CastSpell(ally, SPELL_WELLSPRING_JUMP, true);
            player->CastCustomSpell(ally, SPELL_WELLSPRING_HEAL, &heal, nullptr, nullptr, true);
            if (HasWellMarker(player, MOD_TIDAL_SHELTER))
                Refresh(player, ally, SPELL_TIDAL_SHELTER, int32(effective * UNDERTOW_SHARE));
            if (HasWellMarker(player, MOD_RENEWING_CURRENT))
                Refresh(player, ally, SPELL_RENEWING_CURRENT, int32(effective * UNDERTOW_SHARE / UNDERTOW_TICKS));
        }
    }

    // Soothing Mist's channel target, while the Hero channels it.
    Unit* MistChannelTarget(Player* player)
    {
        Spell* channel = player->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (!channel || channel->m_spellInfo->Id != SPELL_SOOTHING_MIST)
            return nullptr;
        return ObjectAccessor::GetUnit(*player, player->GetGuidValue(UNIT_FIELD_CHANNEL_OBJECT));
    }

    // The most recently healed Wandering Mist target, still alive, injured and within 20 yards of the channel target.
    Unit* SecondaryAlly(Player* player, Unit* channelTarget)
    {
        State& state = StateOf(player);
        Unit* ally = state.LastWanderTarget ? ObjectAccessor::GetUnit(*player, state.LastWanderTarget) : nullptr;
        if (!ally || ally == channelTarget || !ally->IsAlive() || ally->IsFullHealth()
            || !ally->IsWithinDistInMap(channelTarget, SECONDARY_RANGE))
            return nullptr;
        return ally;
    }

    // A Wandering Mist: to an injured ally within 20 yards of the channel target (30 with Flowing Currents), at random
    // (the most injured with Gathering Mists), or shared between two (Drifting Mist). Jade Shelter: once every 8 sec
    // an absorb of 20% of the healing delivered. It never makes another.
    void Wander(Player* player, Unit* channelTarget, float heal)
    {
        State& state = StateOf(player);
        float range = HasMistMarker(player, MOD_FLOWING_CURRENTS) ? FLOWING_RANGE : WANDER_RANGE;
        std::vector<Unit*> allies = InjuredAllies(player, channelTarget, range, channelTarget);
        if (allies.empty() || heal < 1.0f)
            return;
        bool drifting = HasMistMarker(player, MOD_DRIFTING_MIST);
        if (!HasMistMarker(player, MOD_GATHERING_MISTS))
            Acore::Containers::RandomShuffle(allies);
        if (allies.size() > (drifting ? 2u : 1u))
            allies.resize(drifting ? 2 : 1);
        uint32 now = GameTime::GetGameTimeMS().count();
        for (Unit* ally : allies)
        {
            int32 amount = std::max<int32>(1, int32(heal / float(allies.size())));
            int32 effective = Effective(ally, amount);
            player->CastCustomSpell(ally, SPELL_WANDERING_MIST, &amount, nullptr, nullptr, true);
            state.LastWanderTarget = ally->GetGUID();
            if (HasMistMarker(player, MOD_JADE_SHELTER) && now - state.LastJadeShelter >= JADE_SHELTER_INTERVAL_MS
                && effective > 0)
            {
                state.LastJadeShelter = now;
                Refresh(player, ally, SPELL_JADE_SHELTER, int32(effective * JADE_SHELTER_SHARE));
            }
        }
    }

    // ---- Shared by the runes below ----------------------------------------------------------------------------

    uint32 Now()
    {
        return GameTime::GetGameTimeMS().count();
    }

    bool Marked(Unit* unit, uint32 first, uint8 modifier)
    {
        return unit && unit->HasAura(first + modifier);
    }

    Position Toward(Position const& from, float angle, float distance)
    {
        return Position(from.GetPositionX() + distance * std::cos(angle),
            from.GetPositionY() + distance * std::sin(angle), from.GetPositionZ(), angle);
    }

    // In the plane: from p to the segment a..b.
    float SegmentDist(Position const& p, Position const& a, Position const& b)
    {
        float dx = b.GetPositionX() - a.GetPositionX(), dy = b.GetPositionY() - a.GetPositionY();
        float length = dx * dx + dy * dy;
        float t = length > 0.0f ? ((p.GetPositionX() - a.GetPositionX()) * dx
            + (p.GetPositionY() - a.GetPositionY()) * dy) / length : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);
        float x = a.GetPositionX() + t * dx - p.GetPositionX(), y = a.GetPositionY() + t * dy - p.GetPositionY();
        return std::sqrt(x * x + y * y);
    }

    bool InArc(Position const& center, float facing, Position const& p, float halfArc)
    {
        float diff = std::fabs(Position::NormalizeOrientation(center.GetAngle(&p) - facing));
        return std::min(diff, float(2 * M_PI) - diff) <= halfArc;
    }

    // A stretch of ground: a line From..To HalfWidth either side, or a cone at From, Radius long, Arc either side of
    // Facing (a circle at Arc pi).
    struct Shape
    {
        bool Cone = false;
        Position From, To;
        float Facing = 0.0f, HalfWidth = 0.0f, Radius = 0.0f, Arc = float(M_PI);

        bool Holds(Unit* unit) const
        {
            float reach = unit->GetCombatReach() * 0.5f;
            Position at = unit->GetPosition();
            if (Cone)
                return From.GetExactDist2d(&at) <= Radius + reach && InArc(From, Facing, at, Arc);
            return SegmentDist(at, From, To) <= HalfWidth + reach;
        }

        float Extent() const
        {
            return Cone ? Radius : From.GetExactDist2d(&To) + HalfWidth;
        }

        // Where its look lies: patches along the line, or across the cone.
        std::vector<Position> Points(float spacing) const
        {
            std::vector<Position> points;
            if (Cone)
            {
                if (Arc >= float(M_PI) - 0.01f)
                    return { From };
                for (int32 i = -2; i <= 2; ++i)
                    points.push_back(Toward(From, Facing + Arc * i / 2.5f, Radius * 0.6f));
                return points;
            }
            float length = From.GetExactDist2d(&To);
            uint32 count = std::max<uint32>(1, uint32(length / spacing) + 1);
            float angle = From.GetAngle(&To);
            for (uint32 i = 0; i < count; ++i)
                points.push_back(Toward(From, angle, count > 1 ? length * i / (count - 1) : 0.0f));
            return points;
        }
    };

    Shape Circle(Position const& at, float radius)
    {
        Shape shape;
        shape.Cone = true;
        shape.From = at;
        shape.Radius = radius;
        return shape;
    }

    // The enemies the Hero may hit within the shape, nearest the Hero first, up to cap.
    std::vector<Unit*> EnemiesIn(Player* player, Shape const& shape, std::size_t cap)
    {
        float reach = player->GetExactDist2d(&shape.From) + shape.Extent() + 5.0f;
        std::vector<Unit*> found;
        for (Unit* enemy : EnemiesAround(player, player, reach))
        {
            if (found.size() >= cap)
                break;
            if (shape.Holds(enemy))
                found.push_back(enemy);
        }
        return found;
    }

    // A look on the ground: spellId's patches at each point (its earlier ones gone), each on the ground there.
    void PlaceDecals(Player* player, uint32 spellId, std::vector<Position> const& points)
    {
        player->RemoveDynObject(spellId);
        for (Position const& point : points)
        {
            float z = point.GetPositionZ();
            player->UpdateGroundPositionZ(point.GetPositionX(), point.GetPositionY(), z);
            player->CastSpell(point.GetPositionX(), point.GetPositionY(), z, spellId, true);
        }
    }

    // A spell of the Hero's on the unit for ms (its own duration the longest it can last).
    void Lasting(Player* player, Unit* unit, uint32 spellId, int32 ms, int32 const* amount = nullptr)
    {
        unit->RemoveAurasDueToSpell(spellId, player->GetGUID());
        if (amount)
            player->CastCustomSpell(unit, spellId, amount, nullptr, nullptr, true);
        else
            player->CastSpell(unit, spellId, true);
        if (Aura* aura = unit->GetAura(spellId, player->GetGUID()))
        {
            aura->SetMaxDuration(ms);
            aura->SetDuration(ms);
        }
    }

    bool Valid(Player* player, Unit* enemy)
    {
        return enemy && enemy->IsAlive() && enemy->IsInMap(player) && player->IsValidAttackTarget(enemy);
    }

    // ---- The Threefold Grave -----------------------------------------------------------------------------------

    bool HasGraveMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_GRAVE_MODIFIER_FIRST, modifier);
    }

    // A scar's tick: the Hero's level and their attack power.
    float ScarTick(Player* player)
    {
        return SCAR_LEVEL * player->GetLevel() + player->GetTotalAttackPowerValue(BASE_ATTACK) * SCAR_AP;
    }

    bool ScarActive(Player* player, uint8 scar)
    {
        State::ScarAt const& at = StateOf(player).Scars[scar];
        return at.Until > Now() && at.Map == player->GetMapId();
    }

    // The scar's ground: the frozen line, the diseased patch, the blood circle (on the Hero with March of the Dead).
    Shape ScarShape(Player* player, uint8 scar)
    {
        State::ScarAt const& at = StateOf(player).Scars[scar];
        if (scar == SCAR_FROZEN_LINE)
        {
            Shape line;
            line.From = at.From;
            line.To = at.To;
            line.HalfWidth = LINE_HALF_WIDTH;
            return line;
        }
        if (scar == SCAR_DISEASED_PATCH)
            return Circle(at.From, PATCH_RADIUS);
        return Circle(HasGraveMarker(player, MOD_MARCH_OF_THE_DEAD) ? player->GetPosition() : at.From,
            CIRCLE_RADIUS);
    }

    uint8 ScarsHolding(Player* player, Unit* unit)
    {
        uint8 count = 0;
        for (uint8 scar = 0; scar < SCARS; ++scar)
            if (ScarActive(player, scar) && ScarShape(player, scar).Holds(unit))
                ++count;
        return count;
    }

    // Icy Touch, Plague Strike, Death Strike: their scar, moved here (12 sec, 20 with Long Burial).
    void Inscribe(Player* player, uint8 scar, Unit* target)
    {
        State::ScarAt& at = StateOf(player).Scars[scar];
        at.Map = player->GetMapId();
        uint32 ms = HasGraveMarker(player, MOD_LONG_BURIAL) ? LONG_BURIAL_MS : SCAR_MS;
        at.Until = Now() + ms;
        uint32 look = SPELL_SCAR_FIRST + scar;
        if (scar == SCAR_FROZEN_LINE)
        {
            float facing = player->GetAngle(target);
            at.From = Toward(target->GetPosition(), facing + float(M_PI), LINE_HALF_LENGTH);
            at.To = Toward(target->GetPosition(), facing, LINE_HALF_LENGTH);
            PlaceDecals(player, look, ScarShape(player, scar).Points(2.5f));
        }
        else if (scar == SCAR_DISEASED_PATCH)
        {
            at.From = target->GetPosition();
            PlaceDecals(player, look, { at.From });
        }
        else if (HasGraveMarker(player, MOD_MARCH_OF_THE_DEAD))
        {
            at.From = player->GetPosition();
            player->RemoveDynObject(look);
            Lasting(player, player, SPELL_BLOOD_CIRCLE_MARCH, int32(ms));
        }
        else
        {
            at.From = player->GetPosition();
            PlaceDecals(player, look, { at.From });
        }
    }

    void ClearScar(Player* player, uint8 scar)
    {
        StateOf(player).Scars[scar].Until = 0;
        player->RemoveDynObject(SPELL_SCAR_FIRST + scar);
        if (scar == SCAR_BLOOD_CIRCLE)
            player->RemoveAurasDueToSpell(SPELL_BLOOD_CIRCLE_MARCH);
    }

    // A residual infection: a scar's half tick every second, 3 sec (6 with Grave Pestilence).
    void Infect(Player* player, Unit* enemy)
    {
        bool pestilence = HasGraveMarker(player, MOD_GRAVE_PESTILENCE);
        int32 tick = std::max<int32>(1, int32(ScarTick(player) * RESIDUAL_SHARE * (pestilence ? PESTILENCE : 1.0f)));
        Lasting(player, enemy, SPELL_RESIDUAL_INFECTION, pestilence ? PESTILENCE_RESIDUAL_MS : RESIDUAL_MS, &tick);
    }

    // A Dominion's tick on an enemy in the scars: Rime's frost and slow, Blight's disease, Gore's blood.
    void Dominate(Player* player, Unit* enemy, uint8 dominion, float scale)
    {
        float tick = ScarTick(player) * scale;
        if (dominion == DOMINION_RIME)
        {
            int32 damage = std::max<int32>(1, int32(tick * RIME_SHARE));
            player->CastCustomSpell(enemy, SPELL_RIME, &damage, nullptr, nullptr, true);
            int32 slow = HasGraveMarker(player, MOD_CRUEL_WINTER) ? CRUEL_WINTER_SLOW : RIME_SLOW;
            if (enemy->IsAlive())
                player->CastCustomSpell(enemy, SPELL_RIME_CHILL, &slow, nullptr, nullptr, true);
        }
        else if (dominion == DOMINION_BLIGHT)
        {
            float share = BLIGHT_SHARE * (HasGraveMarker(player, MOD_GRAVE_PESTILENCE) ? PESTILENCE : 1.0f);
            int32 damage = std::max<int32>(1, int32(tick * share));
            player->CastCustomSpell(enemy, SPELL_BLIGHT, &damage, nullptr, nullptr, true);
        }
        else
        {
            int32 damage = std::max<int32>(1, int32(tick * GORE_SHARE));
            player->CastCustomSpell(enemy, SPELL_GORE, &damage, nullptr, nullptr, true);
        }
    }

    // Leaving a Dominion, its farewell, all of it within one budget (three scar ticks) shared between the scars:
    // Rime a last burst of slowing frost (rooting with Cruel Winter), Blight every residual infection at once, Gore
    // half the Grave Ward kept a few seconds (all of it with Crimson Bastion).
    void Farewell(Player* player, uint8 dominion)
    {
        float budget = ScarTick(player) * FAREWELL_BUDGET;
        if (dominion == DOMINION_RIME)
        {
            uint8 scars = 0;
            for (uint8 scar = 0; scar < SCARS; ++scar)
                scars += ScarActive(player, scar) ? 1 : 0;
            if (!scars)
                return;
            std::size_t hit = 0;
            for (Unit* enemy : EnemiesAround(player, player, 40.0f))
            {
                uint8 count = ScarsHolding(player, enemy);
                if (!count)
                    continue;
                if (++hit > SCAR_TARGETS)
                    break;
                int32 damage = std::max<int32>(1, int32(budget * count / scars));
                enemy->CastSpell(enemy, SPELL_RIME_FAREWELL, true);
                player->CastCustomSpell(enemy, SPELL_RIME, &damage, nullptr, nullptr, true);
                if (!enemy->IsAlive())
                    continue;
                if (HasGraveMarker(player, MOD_CRUEL_WINTER))
                    player->CastSpell(enemy, SPELL_CRUEL_WINTER, true);
                int32 slow = CRUEL_WINTER_SLOW;
                player->CastCustomSpell(enemy, SPELL_RIME_CHILL, &slow, nullptr, nullptr, true);
            }
        }
        else if (dominion == DOMINION_BLIGHT)
        {
            std::vector<std::pair<Unit*, float>> infected;
            float total = 0.0f;
            for (Unit* enemy : EnemiesAround(player, player, 60.0f))
                if (Aura* aura = enemy->GetAura(SPELL_RESIDUAL_INFECTION, player->GetGUID()))
                    if (AuraEffect* effect = aura->GetEffect(EFFECT_0))
                    {
                        float left = effect->GetAmount() * std::ceil(aura->GetDuration() / 1000.0f);
                        infected.emplace_back(enemy, left);
                        total += left;
                    }
            float scale = total > budget ? budget / total : 1.0f;
            for (auto const& [enemy, left] : infected)
            {
                enemy->RemoveAurasDueToSpell(SPELL_RESIDUAL_INFECTION, player->GetGUID());
                int32 damage = std::max<int32>(1, int32(left * scale));
                player->CastCustomSpell(enemy, SPELL_BLIGHT, &damage, nullptr, nullptr, true);
            }
        }
        else if (Aura* ward = player->GetAura(SPELL_GRAVE_WARD))
        {
            if (!HasGraveMarker(player, MOD_CRIMSON_BASTION))
                if (AuraEffect* effect = ward->GetEffect(EFFECT_0))
                    effect->ChangeAmount(std::max<int32>(1, int32(effect->GetAmount() * PRESERVED_WARD)));
            if (ward->GetDuration() > PRESERVED_WARD_MS)
                ward->SetDuration(PRESERVED_WARD_MS);
        }
    }

    // The Hero's Dominion (its buff and look); farewell: the last one's farewell, and Lingering Dominion's 3 sec.
    void SetDominion(Player* player, uint8 dominion, bool farewell)
    {
        State& state = StateOf(player);
        uint8 last = state.Dominion;
        if (farewell && last != dominion)
        {
            Farewell(player, last);
            if (HasGraveMarker(player, MOD_LINGERING_DOMINION))
            {
                state.Lingering = last;
                state.LingerUntil = Now() + LINGERING_DOMINION_MS;
            }
        }
        state.Dominion = dominion;
        state.InBlight.clear();
        for (uint8 other = 0; other < DOMINIONS; ++other)
            if (other != dominion)
                player->RemoveAurasDueToSpell(SPELL_DOMINION_FIRST + other);
        player->CastSpell(player, SPELL_DOMINION_FIRST + dominion, true);
    }

    // Requiem's next Dominion: Rime, Blight, Gore (Gore, Blight, Rime with Reverse the Dirge).
    uint8 NextDominion(Player* player)
    {
        uint8 step = HasGraveMarker(player, MOD_REVERSE_THE_DIRGE) ? DOMINIONS - 1 : 1;
        return uint8((StateOf(player).Dominion + step) % DOMINIONS);
    }

    bool GoreActive(Player* player)
    {
        State& state = StateOf(player);
        return state.Dominion == DOMINION_GORE
            || (state.Lingering == DOMINION_GORE && Now() < state.LingerUntil);
    }

    // Death Strike connecting while the Hero stands in a scar under Gore: a Grave Ward of a quarter of its damage
    // (40% with Crimson Bastion), added up to 10% of their health (15%), 8 sec.
    void GraveWard(Player* player, int32 damage)
    {
        if (damage <= 0 || !GoreActive(player) || !ScarsHolding(player, player))
            return;
        bool bastion = HasGraveMarker(player, MOD_CRIMSON_BASTION);
        int32 cap = int32(player->CountPctFromMaxHealth(int32(bastion ? BASTION_WARD_CAP_PCT : GRAVE_WARD_CAP_PCT)));
        int32 have = 0;
        if (AuraEffect* ward = player->GetAuraEffect(SPELL_GRAVE_WARD, EFFECT_0))
            have = ward->GetAmount();
        int32 amount = std::min(cap, have + int32(damage * (bastion ? BASTION_WARD_SHARE : GRAVE_WARD_SHARE)));
        if (amount > 0)
            Lasting(player, player, SPELL_GRAVE_WARD, GRAVE_WARD_MS, &amount);
    }

    // Every second: the scars' Dominion (and Lingering Dominion's last one, at half) on each enemy in them, once
    // however many scars it stands in (more with Converging Graves), up to 10; enemies leaving Blight infected.
    void GraveTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        state.GraveTimer += diff;
        if (state.GraveTimer < GRAVE_TICK_MS)
            return;
        state.GraveTimer = 0;
        uint32 now = Now();
        bool any = false;
        for (uint8 scar = 0; scar < SCARS; ++scar)
        {
            State::ScarAt const& at = state.Scars[scar];
            if (at.Until && (at.Until <= now || at.Map != player->GetMapId()))
                ClearScar(player, scar);
            any = any || state.Scars[scar].Until;
        }
        if (!any || !player->IsAlive())
        {
            state.InBlight.clear();
            return;
        }
        bool lingering = state.Lingering < DOMINIONS && now < state.LingerUntil;
        bool converging = HasGraveMarker(player, MOD_CONVERGING_GRAVES);
        std::set<ObjectGuid> blighted;
        std::size_t hit = 0;
        for (Unit* enemy : EnemiesAround(player, player, 40.0f))
        {
            uint8 count = ScarsHolding(player, enemy);
            if (!count)
                continue;
            if (++hit > SCAR_TARGETS)
                break;
            float scale = !converging || count < 2 ? 1.0f : count >= 3 ? CONVERGING_THREE : CONVERGING_TWO;
            Dominate(player, enemy, state.Dominion, scale);
            if (lingering && enemy->IsAlive())
                Dominate(player, enemy, state.Lingering, scale * LINGERING_OUTPUT);
            if (state.Dominion == DOMINION_BLIGHT || (lingering && state.Lingering == DOMINION_BLIGHT))
                blighted.insert(enemy->GetGUID());
        }
        for (ObjectGuid guid : state.InBlight)
            if (!blighted.count(guid))
                if (Unit* enemy = ObjectAccessor::GetUnit(*player, guid); Valid(player, enemy))
                    Infect(player, enemy);
        state.InBlight = std::move(blighted);
    }

    // ---- Unbroken Onslaught ------------------------------------------------------------------------------------

    bool HasOnslaughtMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_ONSLAUGHT_MODIFIER_FIRST, modifier);
    }

    // The wake through the target, as the Wake slot cuts it, from the Hero's angle to the target.
    Shape WakeShape(Player* player, Unit* target)
    {
        Shape shape;
        float facing = player->GetAngle(target);
        Position at = target->GetPosition();
        if (HasOnslaughtMarker(player, MOD_WHIRLING_STEEL))
        {
            shape.Cone = true;
            shape.From = player->GetPosition();
            shape.Facing = facing;
            shape.Radius = CRESCENT_RADIUS;
            shape.Arc = CRESCENT_ARC;
            return shape;
        }
        bool raking = HasOnslaughtMarker(player, MOD_RAKING_PASSAGE);
        shape.From = Toward(at, facing + float(M_PI), raking ? RAKE_BEHIND : WAKE_HALF_LENGTH);
        shape.To = Toward(at, facing, raking ? RAKE_BEYOND : WAKE_HALF_LENGTH);
        shape.HalfWidth = HasOnslaughtMarker(player, MOD_EXECUTIONERS_CUT) ? CUT_HALF_WIDTH : WAKE_HALF_WIDTH;
        return shape;
    }

    void EndWake(Player* player)
    {
        StateOf(player).Wake = false;
        player->RemoveDynObject(SPELL_WAKE_GROUND);
    }

    // The wake's attack: 60% of a normalized swing to each enemy along it (up to 6), its primary target more;
    // Bloodletting: a bleed of 30% of the hit over 6 sec, refreshed, never above 60% of a swing.
    void WakeStrike(Player* player, Shape const& shape, ObjectGuid primary, float share)
    {
        if (!player->IsInWorld() || !player->IsAlive())
            return;
        float swing = float(player->CalculateDamage(BASE_ATTACK, true, true));
        float bonus = HasOnslaughtMarker(player, MOD_EXECUTIONERS_CUT) ? EXECUTIONER_PRIMARY : WAKE_PRIMARY;
        bool bleed = HasOnslaughtMarker(player, MOD_BLOODLETTING);
        for (Unit* enemy : EnemiesIn(player, shape, WAKE_TARGETS))
        {
            float amount = swing * WAKE_SHARE * share * (enemy->GetGUID() == primary ? bonus : 1.0f);
            int32 damage = std::max<int32>(1, int32(amount));
            player->CastCustomSpell(enemy, share < 1.0f ? SPELL_WAKE_AFTERSHOCK : SPELL_WEAPON_WAKE, &damage,
                nullptr, nullptr, true);
            if (!bleed || !enemy->IsAlive())
                continue;
            int32 tick = int32(damage * BLEED_SHARE / BLEED_TICKS);
            if (AuraEffect const* last = enemy->GetAuraEffect(SPELL_WAKE_BLEED, EFFECT_0, player->GetGUID()))
                tick = std::max(tick, last->GetAmount());
            tick = std::max<int32>(1, std::min(tick, int32(swing * BLEED_CAP / BLEED_TICKS)));
            Lasting(player, enemy, SPELL_WAKE_BLEED, BLEED_TICKS * 1000, &tick);
        }
    }

    // A different melee ability through the wake reopens it, once: its attack along the wake a moment later (at
    // once with Overpowering Echo), Aftershock a second impact there 1.5 sec on at half. Changing Prey: the one
    // struck takes the primary target's bonus.
    void ReopenWake(Player* player, Unit* struck)
    {
        State& state = StateOf(player);
        Shape shape;
        shape.Cone = state.WakeCrescent;
        shape.From = state.WakeFrom;
        shape.To = state.WakeTo;
        shape.Facing = state.WakeFacing;
        shape.HalfWidth = state.WakeHalfWidth;
        shape.Radius = CRESCENT_RADIUS;
        shape.Arc = CRESCENT_ARC;
        ObjectGuid primary = HasOnslaughtMarker(player, MOD_CHANGING_PREY) ? struck->GetGUID() : state.WakePrimary;
        EndWake(player);
        uint32 delay = HasOnslaughtMarker(player, MOD_OVERPOWERING_ECHO) ? 0 : WAKE_DELAY_MS;
        if (!delay)
            WakeStrike(player, shape, primary, 1.0f);
        else
            player->m_Events.AddEventAtOffset([player, shape, primary]() { WakeStrike(player, shape, primary, 1.0f); },
                Milliseconds(delay));
        if (HasOnslaughtMarker(player, MOD_AFTERSHOCK))
            player->m_Events.AddEventAtOffset([player, shape, primary]()
            {
                WakeStrike(player, shape, primary, AFTERSHOCK_SHARE);
            }, Milliseconds(delay + AFTERSHOCK_MS));
    }

    // Mortal Strike or Bloodthirst: a new wake through the target (the last one gone), 6 sec (10 with Battlefield
    // Claim), fixed where it was cut (following the target with Relentless).
    void CutWake(Player* player, Unit* target, uint32 creator)
    {
        State& state = StateOf(player);
        Shape shape = WakeShape(player, target);
        state.Wake = true;
        state.WakeCrescent = shape.Cone;
        state.WakeFrom = shape.From;
        state.WakeTo = shape.To;
        state.WakeFacing = shape.Facing;
        state.WakeHalfWidth = shape.HalfWidth;
        state.WakeMap = player->GetMapId();
        state.WakeCreator = creator;
        state.WakePrimary = target->GetGUID();
        state.WakeOffset = target->GetPosition();
        state.WakeUntil = Now() + (HasOnslaughtMarker(player, MOD_BATTLEFIELD_CLAIM) ? CLAIM_MS : WAKE_MS);
        PlaceDecals(player, SPELL_WAKE_GROUND, shape.Points(2.5f));
    }

    bool InWake(Player* player, Unit* unit)
    {
        State& state = StateOf(player);
        if (!state.Wake || state.WakeMap != player->GetMapId() || Now() >= state.WakeUntil)
            return false;
        Shape shape;
        shape.Cone = state.WakeCrescent;
        shape.From = state.WakeFrom;
        shape.To = state.WakeTo;
        shape.Facing = state.WakeFacing;
        shape.HalfWidth = state.WakeHalfWidth;
        shape.Radius = CRESCENT_RADIUS;
        shape.Arc = CRESCENT_ARC;
        return shape.Holds(unit);
    }

    // The wake's end; Relentless: it follows its target (moved with it, twice a second).
    void WakeTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        if (!state.Wake)
            return;
        if (Now() >= state.WakeUntil || state.WakeMap != player->GetMapId())
            return EndWake(player);
        state.WakeTimer += diff;
        if (state.WakeTimer < 500 || !HasOnslaughtMarker(player, MOD_RELENTLESS))
            return;
        state.WakeTimer = 0;
        Unit* target = ObjectAccessor::GetUnit(*player, state.WakePrimary);
        if (!target || !target->IsAlive() || target->GetExactDist2d(&state.WakeOffset) < 1.0f)
            return;
        float dx = target->GetPositionX() - state.WakeOffset.GetPositionX();
        float dy = target->GetPositionY() - state.WakeOffset.GetPositionY();
        for (Position* point : { &state.WakeFrom, &state.WakeTo })
            point->Relocate(point->GetPositionX() + dx, point->GetPositionY() + dy, target->GetPositionZ());
        state.WakeOffset = target->GetPosition();
        Shape shape;
        shape.Cone = state.WakeCrescent;
        shape.From = state.WakeFrom;
        shape.To = state.WakeTo;
        shape.Facing = state.WakeFacing;
        shape.Radius = CRESCENT_RADIUS;
        shape.Arc = CRESCENT_ARC;
        PlaceDecals(player, SPELL_WAKE_GROUND, shape.Points(2.5f));
    }

    // ---- The Dawnbound Crusader --------------------------------------------------------------------------------

    bool HasDawnMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_DAWN_MODIFIER_FIRST, modifier);
    }

    // The vowed ally, or the Hero themselves while there is none (gone, dead, elsewhere).
    Unit* VowOf(Player* player)
    {
        State& state = StateOf(player);
        Unit* vow = state.Vow ? ObjectAccessor::GetUnit(*player, state.Vow) : nullptr;
        if (!vow || !vow->IsAlive() || !vow->IsInMap(player) || !vow->HasAura(SPELL_DAWN_VOWED, player->GetGUID()))
            return player;
        return vow;
    }

    void DropVow(Player* player)
    {
        State& state = StateOf(player);
        if (state.Vow)
            if (Unit* vow = ObjectAccessor::GetUnit(*player, state.Vow))
                vow->RemoveAurasDueToSpell(SPELL_DAWN_VOWED, player->GetGUID());
        player->RemoveAurasDueToSpell(SPELL_DAWN_VOWED, player->GetGUID());
        state.Vow.Clear();
    }

    Unit* JudgmentTarget(Player* player)
    {
        State& state = StateOf(player);
        Unit* marked = state.Marked ? ObjectAccessor::GetUnit(*player, state.Marked) : nullptr;
        return Valid(player, marked) && marked->HasAura(SPELL_DAWN_MARK, player->GetGUID()) ? marked : nullptr;
    }

    // The Doctrine: Mercy heals, Retribution strikes, Communion turns between them (a Healer's echoes heal and
    // anyone else's strike until a Doctrine is chosen).
    bool EchoHeals(Player* player)
    {
        State& state = StateOf(player);
        if (HasDawnMarker(player, MOD_MERCY))
            return true;
        if (HasDawnMarker(player, MOD_RETRIBUTION))
            return false;
        if (HasDawnMarker(player, MOD_COMMUNION))
            return state.Echoes % 2 == 0;
        return Wildcard::GetRole(player) == Wildcard::ROLE_HEALER;
    }

    void EchoOn(Player* player, Unit* unit, float amount, bool heal)
    {
        int32 points = std::max<int32>(1, int32(amount));
        player->CastCustomSpell(unit, heal ? SPELL_DAWN_ECHO_HEAL : SPELL_DAWN_ECHO_DAMAGE, &points, nullptr,
            nullptr, true);
    }

    // A Dawn Echo from the vow: on one (the most injured ally within reach of it, or the Judgment target, else the
    // nearest enemy), or along the way to the Judgment target (Dawnwave), or all about the vow (Sunburst), or as a
    // patch of consecrated ground there (Sacred Ground: a quarter a second for 4 sec, the last patch replaced).
    void Emit(Player* player, Unit* vow, float amount, bool heal, float reach)
    {
        State& state = StateOf(player);
        Unit* marked = JudgmentTarget(player);
        auto single = [&]() -> Unit*
        {
            if (heal)
            {
                std::vector<Unit*> allies = InjuredAllies(player, vow, reach, nullptr);
                return allies.empty() ? nullptr : allies.front();
            }
            if (marked && marked->IsWithinDistInMap(vow, reach))
                return marked;
            std::vector<Unit*> enemies = EnemiesAround(player, vow, reach);
            return enemies.empty() ? nullptr : enemies.front();
        };
        uint32 look = heal ? SPELL_DAWN_BOLT_HEAL : SPELL_DAWN_BOLT;

        if (HasDawnMarker(player, MOD_DAWNWAVE))
        {
            Unit* toward = marked && marked->IsWithinDistInMap(vow, reach * 2.0f) ? marked : single();
            if (!toward)
                return;
            Shape line;
            line.From = vow->GetPosition();
            line.To = Toward(line.From, vow->GetAngle(toward), vow->GetExactDist2d(toward) + 2.0f);
            line.HalfWidth = DAWNWAVE_WIDTH / 2.0f;
            vow->CastSpell(toward, look, true);
            std::vector<Unit*> touched;
            if (heal)
            {
                for (Unit* ally : InjuredAllies(player, vow, vow->GetExactDist2d(toward) + 5.0f, nullptr))
                    if (touched.size() < DAWN_TARGETS && line.Holds(ally))
                        touched.push_back(ally);
            }
            else
                touched = EnemiesIn(player, line, DAWN_TARGETS);
            for (Unit* unit : touched)
                EchoOn(player, unit, amount * DAWNWAVE_SHARE, heal);
            return;
        }
        if (HasDawnMarker(player, MOD_SUNBURST))
        {
            vow->CastSpell(vow, SPELL_SUNBURST, true);
            std::vector<Unit*> touched = heal ? InjuredAllies(player, vow, SUNBURST_RADIUS, nullptr)
                : EnemiesAround(player, vow, SUNBURST_RADIUS);
            if (touched.size() > DAWN_TARGETS)
                touched.resize(DAWN_TARGETS);
            for (Unit* unit : touched)
                EchoOn(player, unit, amount * SUNBURST_SHARE, heal);
            return;
        }
        if (HasDawnMarker(player, MOD_SACRED_GROUND))
        {
            state.SacredAt = vow->GetPosition();
            state.SacredMap = player->GetMapId();
            state.SacredUntil = Now() + SACRED_MS;
            state.SacredTick = amount * SACRED_SHARE;
            state.SacredHeal = heal;
            state.SacredTimer = 0;
            PlaceDecals(player, SPELL_SACRED_GROUND, { state.SacredAt });
            return;
        }
        if (Unit* unit = single())
        {
            vow->CastSpell(unit, look, true);
            EchoOn(player, unit, amount, heal);
        }
    }

    // The pending echo, released: from the vow, its reach 15 yards and Shared Pilgrimage's yards; Steadfast's
    // afterimage repeats it at half.
    void ReleaseEcho(Player* player)
    {
        State& state = StateOf(player);
        if (!state.Pending)
            return;
        state.Pending = false;
        Unit* vow = VowOf(player);
        bool heal = EchoHeals(player);
        float amount = state.PendingAmount * (HasDawnMarker(player, MOD_COMMUNION) ? COMMUNION_OUTPUT : 1.0f);
        ++state.Echoes;
        float reach = DAWN_REACH;
        if (HasDawnMarker(player, MOD_SHARED_PILGRIMAGE))
            reach += std::min(PILGRIMAGE_CAP, state.Pilgrimage);
        state.Pilgrimage = 0.0f;
        Emit(player, vow, amount, heal, reach);
        if (vow->HasAura(SPELL_DAWN_AFTERIMAGE, player->GetGUID()))
        {
            vow->RemoveAurasDueToSpell(SPELL_DAWN_AFTERIMAGE, player->GetGUID());
            Emit(player, vow, amount * AFTERIMAGE_SHARE, heal, reach);
        }
    }

    // An echo pending: half of what set it off, released 0.75 sec on (another one meanwhile joins it).
    void QueueEcho(Player* player, float amount)
    {
        State& state = StateOf(player);
        if (amount <= 0.0f)
            return;
        state.PendingAmount = state.Pending ? state.PendingAmount + amount * DAWN_SHARE : amount * DAWN_SHARE;
        state.Pending = true;
        state.PendingAt = Now() + DAWN_PENDING_MS;
    }

    // The vow moved to the ally (one at a time); Passing the Torch: a pending echo released from them at once.
    void Vow(Player* player, Unit* ally)
    {
        State& state = StateOf(player);
        if (state.Vow && state.Vow != ally->GetGUID())
            if (Unit* last = ObjectAccessor::GetUnit(*player, state.Vow))
                last->RemoveAurasDueToSpell(SPELL_DAWN_VOWED, player->GetGUID());
        state.Vow = ally->GetGUID();
        if (!ally->HasAura(SPELL_DAWN_VOWED, player->GetGUID()))
            player->AddAura(SPELL_DAWN_VOWED, ally);
        state.VowWas = ally->GetPosition();
        if (state.Pending && HasDawnMarker(player, MOD_PASSING_THE_TORCH))
            ReleaseEcho(player);
    }

    void Mark(Player* player, Unit* enemy)
    {
        State& state = StateOf(player);
        if (state.Marked && state.Marked != enemy->GetGUID())
            if (Unit* last = ObjectAccessor::GetUnit(*player, state.Marked))
                last->RemoveAurasDueToSpell(SPELL_DAWN_MARK, player->GetGUID());
        state.Marked = enemy->GetGUID();
        Lasting(player, enemy, SPELL_DAWN_MARK, int32(DAWN_MARK_MS));
    }

    // Four times a second: the pending echo's release, Shared Pilgrimage's yards, Sacred Ground's ticks.
    void DawnTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        state.DawnTimer += diff;
        if (state.DawnTimer < 250)
            return;
        uint32 elapsed = state.DawnTimer;
        state.DawnTimer = 0;
        uint32 now = Now();
        if (state.Pending && now >= state.PendingAt)
            ReleaseEcho(player);

        Unit* vow = VowOf(player);
        Position hero = player->GetPosition(), there = vow->GetPosition();
        if (state.HeroWas.GetExactDist2d(&hero) < 10.0f)
            state.Pilgrimage += state.HeroWas.GetExactDist2d(&hero) * PILGRIMAGE_PER_YARD;
        if (vow != player && state.VowWas.GetExactDist2d(&there) < 10.0f)
            state.Pilgrimage += state.VowWas.GetExactDist2d(&there) * PILGRIMAGE_PER_YARD;
        state.HeroWas = hero;
        state.VowWas = there;

        if (!state.SacredUntil)
            return;
        if (now >= state.SacredUntil || state.SacredMap != player->GetMapId())
        {
            state.SacredUntil = 0;
            player->RemoveDynObject(SPELL_SACRED_GROUND);
            return;
        }
        state.SacredTimer += elapsed;
        if (state.SacredTimer < 1000)
            return;
        state.SacredTimer = 0;
        Shape patch = Circle(state.SacredAt, SACRED_RADIUS);
        if (state.SacredHeal)
        {
            std::size_t healed = 0;
            for (Unit* ally : InjuredAllies(player, player, 60.0f, nullptr))
                if (healed < DAWN_TARGETS && patch.Holds(ally))
                {
                    ++healed;
                    EchoOn(player, ally, state.SacredTick, true);
                }
        }
        else
            for (Unit* enemy : EnemiesIn(player, patch, DAWN_TARGETS))
                EchoOn(player, enemy, state.SacredTick, false);
    }

    // ---- The Hollow Congregation -------------------------------------------------------------------------------

    bool HasHollowMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_HOLLOW_MODIFIER_FIRST, modifier);
    }

    // A tick of the Hero's best Mind Flay: its rank's, a quarter of spell power, and their Shadow damage bonuses.
    float FlayTick(Player* player)
    {
        uint32 best = 0;
        for (uint32 id = SPELL_MIND_FLAY; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (player->HasSpell(id))
                best = id;
        SpellInfo const* rank = sSpellMgr->GetSpellInfo(best ? best : uint32(SPELL_MIND_FLAY));
        float points = rank ? float(std::max(1, rank->Effects[EFFECT_2].CalcValue(player))) : 1.0f;
        float power = float(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SHADOW)) * 0.257f;
        return (points + power)
            * player->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, SPELL_SCHOOL_MASK_SHADOW);
    }

    std::vector<Creature*> LiveEchoes(Player* player)
    {
        State& state = StateOf(player);
        std::vector<Creature*> echoes;
        std::vector<ObjectGuid> kept;
        for (ObjectGuid guid : state.HollowEchoes)
            if (Creature* echo = ObjectAccessor::GetCreature(*player, guid); echo && echo->IsAlive())
            {
                echoes.push_back(echo);
                kept.push_back(guid);
            }
        state.HollowEchoes = kept;
        return echoes;
    }

    void DismissEchoes(Player* player)
    {
        for (Creature* echo : LiveEchoes(player))
            if (TempSummon* summon = echo->ToTempSummon())
                summon->UnSummon();
        StateOf(player).HollowEchoes.clear();
        StateOf(player).EchoHosts.clear();
    }

    // Two seconds into Mind Flay: a Hollow Echo where the Hero stands (on the target with Possession, one an
    // enemy), 10 sec; a third replaces the oldest.
    void LeaveEcho(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        std::vector<Creature*> echoes = LiveEchoes(player);
        if (echoes.size() >= MAX_HOLLOW_ECHOES)
            if (TempSummon* oldest = echoes.front()->ToTempSummon())
            {
                oldest->UnSummon();
                state.HollowEchoes.erase(state.HollowEchoes.begin());
            }
        Unit* host = nullptr;
        if (HasHollowMarker(player, MOD_POSSESSION))
        {
            auto hosted = [&](Unit* enemy)
            {
                for (Creature* echo : LiveEchoes(player))
                    if (auto host = state.EchoHosts.find(echo->GetGUID());
                        host != state.EchoHosts.end() && host->second == enemy->GetGUID())
                        return true;
                return false;
            };
            std::vector<Unit*> candidates = { target };
            for (Unit* enemy : EnemiesAround(player, player, HOLLOW_RANGE))
                candidates.push_back(enemy);
            for (Unit* enemy : candidates)
                if (Valid(player, enemy) && !hosted(enemy))
                {
                    host = enemy;
                    break;
                }
        }
        Position pos = host ? host->GetPosition() : player->GetNearPosition(1.5f, frand(0.0f, 2.0f * float(M_PI)));
        TempSummon* echo = player->GetMap()->SummonCreature(NPC_HOLLOW_ECHO, pos,
            sSummonPropertiesStore.LookupEntry(SUMMON_PROPERTIES_GUARDIAN), HOLLOW_ECHO_MS, player);
        if (!echo)
            return;
        state.HollowEchoes.push_back(echo->GetGUID());
        if (echo->IsAIEnabled)
        {
            echo->AI()->SetGUID(target ? target->GetGUID() : ObjectGuid::Empty, 0);
            if (host)
                echo->AI()->SetGUID(host->GetGUID(), 1);
        }
        if (host)
            state.EchoHosts[echo->GetGUID()] = host->GetGUID();
    }

    // Mind Blast's command: every echo turns on its target (Dissonance: each on another enemy with the Hero's
    // Shadow Word: Pain, the target first), beside it with Procession, in a barrage of 3 bolts (Unison: together,
    // 20% more) or a piercing scream along the way (Accusation); then their channels resume on it.
    void CommandEchoes(Player* player, Unit* target)
    {
        std::vector<Creature*> echoes = LiveEchoes(player);
        if (echoes.empty())
            return;
        std::vector<Unit*> victims;
        if (HasHollowMarker(player, MOD_DISSONANCE))
        {
            auto pained = [player](Unit* enemy)
            {
                for (uint32 id = SPELL_SHADOW_WORD_PAIN; id; id = sSpellMgr->GetNextSpellInChain(id))
                    if (enemy->HasAura(id, player->GetGUID()))
                        return true;
                return false;
            };
            if (pained(target))
                victims.push_back(target);
            for (Unit* enemy : EnemiesAround(player, player, HAUNTED_RANGE))
                if (enemy != target && pained(enemy))
                    victims.push_back(enemy);
        }
        bool unison = HasHollowMarker(player, MOD_UNISON), accusation = HasHollowMarker(player, MOD_ACCUSATION);
        float tick = FlayTick(player);
        int32 bolt = std::max<int32>(1, int32(tick * BARRAGE_SHARE * (unison ? UNISON_BARRAGE : 1.0f)));
        ObjectGuid owner = player->GetGUID();
        for (std::size_t i = 0; i < echoes.size(); ++i)
        {
            Creature* echo = echoes[i];
            Unit* victim = i < victims.size() ? victims[i] : target;
            ObjectGuid victimGuid = victim->GetGUID();
            echo->InterruptNonMeleeSpells(false);
            if (HasHollowMarker(player, MOD_PROCESSION))
            {
                Position spot = victim->GetNearPosition(3.0f, float(M_PI) / 2.0f + float(M_PI) * i);
                echo->NearTeleportTo(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(),
                    echo->GetAngle(victim));
            }
            if (echo->IsAIEnabled)
            {
                echo->AI()->SetGUID(victimGuid, 0);
                echo->AI()->SetData(0, BARRAGE_BOLTS * BARRAGE_SPACING_MS + 300);
            }
            if (accusation)
            {
                int32 scream = std::max<int32>(1, int32(tick * BARRAGE_SHARE * SCREAM_SHARE));
                echo->m_Events.AddEventAtOffset([echo, owner, victimGuid, scream]()
                {
                    Player* hero = ObjectAccessor::GetPlayer(*echo, owner);
                    Unit* at = ObjectAccessor::GetUnit(*echo, victimGuid);
                    if (!hero || !at || !at->IsAlive())
                        return;
                    Shape line;
                    line.From = echo->GetPosition();
                    line.To = Toward(line.From, echo->GetAngle(at), echo->GetExactDist2d(at) + 3.0f);
                    line.HalfWidth = SCREAM_WIDTH;
                    for (Unit* enemy : EnemiesIn(hero, line, SCREAM_TARGETS))
                        echo->CastCustomSpell(enemy, SPELL_HOLLOW_SCREAM, &scream, nullptr, nullptr, true);
                }, Milliseconds(unison ? 0 : 150 * i));
                continue;
            }
            for (uint32 b = 0; b < BARRAGE_BOLTS; ++b)
                echo->m_Events.AddEventAtOffset([echo, victimGuid, bolt]()
                {
                    Unit* at = ObjectAccessor::GetUnit(*echo, victimGuid);
                    if (at && at->IsAlive())
                        echo->CastCustomSpell(at, SPELL_HOLLOW_BOLT, &bolt, nullptr, nullptr, true);
                }, Milliseconds(b * BARRAGE_SPACING_MS + (unison ? 0 : 150 * i)));
        }
    }

    // Mind Flay held two seconds leaves its echo.
    void HollowTick(Player* player)
    {
        State& state = StateOf(player);
        if (state.FlayEchoed || Now() - state.FlayStart < FLAY_ECHO_AFTER_MS)
            return;
        Spell* channel = player->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (!channel || channel->m_spellInfo->GetFirstRankSpell()->Id != SPELL_MIND_FLAY)
        {
            state.FlayEchoed = true;
            return;
        }
        state.FlayEchoed = true;
        Unit* target = ObjectAccessor::GetUnit(*player, state.FlayTarget);
        LeaveEcho(player, Valid(player, target) ? target : nullptr);
    }

    // ---- The Shattered Sun ---------------------------------------------------------------------------------------

    bool HasSunMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_SUN_MODIFIER_FIRST, modifier);
    }

    bool HasShard(Player* player, Unit* unit)
    {
        return unit->HasAura(SPELL_SUNSHARD, player->GetGUID());
    }

    // The shards still held, and Solar Anchor's left on the ground where theirs were embedded.
    void PruneShards(Player* player)
    {
        State& state = StateOf(player);
        std::vector<ObjectGuid> kept;
        for (ObjectGuid guid : state.Shards)
            if (Unit* host = ObjectAccessor::GetUnit(*player, guid); host && host->IsAlive() && HasShard(player, host))
                kept.push_back(guid);
            else
                state.Anchors.erase(guid);
        bool changed = kept.size() != state.Shards.size();
        state.Shards = kept;
        if (!changed)
            return;
        std::vector<Position> anchors;
        for (auto const& [guid, at] : state.Anchors)
            anchors.push_back(at);
        PlaceDecals(player, SPELL_SOLAR_ANCHOR, anchors);
    }

    // Fireball: a Sunshard in the target (refreshed if it has one), two held at most (three with Twin Suns): the
    // oldest goes. Solar Anchor: the shard stays where it went in.
    void Embed(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        PruneShards(player);
        bool anchor = HasSunMarker(player, MOD_SOLAR_ANCHOR);
        if (!HasShard(player, target))
        {
            std::size_t most = HasSunMarker(player, MOD_TWIN_SUNS) ? TWIN_SUNS_SHARDS : MAX_SHARDS;
            while (state.Shards.size() >= most)
            {
                if (Unit* oldest = ObjectAccessor::GetUnit(*player, state.Shards.front()))
                    oldest->RemoveAurasDueToSpell(SPELL_SUNSHARD, player->GetGUID());
                state.Anchors.erase(state.Shards.front());
                state.Shards.erase(state.Shards.begin());
            }
            state.Shards.push_back(target->GetGUID());
        }
        Lasting(player, target, SPELL_SUNSHARD, SUNSHARD_MS);
        if (!anchor)
            return;
        state.Anchors[target->GetGUID()] = target->GetPosition();
        std::vector<Position> anchors;
        for (auto const& [guid, at] : state.Anchors)
            anchors.push_back(at);
        PlaceDecals(player, SPELL_SOLAR_ANCHOR, anchors);
    }

    // Frostbolt on a shard's host: it breaks, 60% of the bolt's damage to each enemy within 8 yards (up to 8): about
    // the host, or its anchor (Solar Anchor), or the Hero's own target (Orbiting Sun); a cone away from the Hero
    // (Splinterstorm), a line on through it (Solar Lance); Scalding Ground leaves a hazard. A Broken Core stays.
    void Fracture(Player* player, Unit* host, float base)
    {
        State& state = StateOf(player);
        host->RemoveAurasDueToSpell(SPELL_SUNSHARD, player->GetGUID());
        Position at = host->GetPosition();
        if (auto anchor = state.Anchors.find(host->GetGUID()); anchor != state.Anchors.end())
            at = anchor->second;
        Unit* look = host;
        if (HasSunMarker(player, MOD_ORBITING_SUN))
            if (Unit* aimed = player->GetSelectedUnit(); Valid(player, aimed) && aimed != host
                && player->IsWithinDistInMap(aimed, ORBIT_RANGE))
            {
                at = aimed->GetPosition();
                look = aimed;
            }
        PruneShards(player);

        Shape shape = Circle(at, FRACTURE_RADIUS);
        float away = player->GetAngle(&at);
        if (HasSunMarker(player, MOD_SPLINTERSTORM))
        {
            shape.Facing = away;
            shape.Radius = SPLINTER_REACH;
            shape.Arc = SPLINTER_ARC;
        }
        else if (HasSunMarker(player, MOD_SOLAR_LANCE))
        {
            shape = Shape();
            shape.From = at;
            shape.To = Toward(at, away, LANCE_REACH);
            shape.HalfWidth = LANCE_HALF_WIDTH;
        }
        look->CastSpell(look, SPELL_FRACTURE_LOOK, true);
        int32 damage = std::max<int32>(1, int32(base * FRACTURE_SHARE));
        for (Unit* enemy : EnemiesIn(player, shape, FRACTURE_TARGETS))
            player->CastCustomSpell(enemy, SPELL_FRACTURE, &damage, nullptr, nullptr, true);
        if (host->IsAlive())
            Lasting(player, host, SPELL_BROKEN_CORE, BROKEN_CORE_MS);
        if (HasSunMarker(player, MOD_SCALDING_GROUND))
        {
            state.ScaldAt = at;
            state.ScaldMap = player->GetMapId();
            state.ScaldUntil = Now() + SCALDING_MS;
            state.ScaldTick = base * SCALDING_SHARE;
            state.ScaldTimer = 0;
            PlaceDecals(player, SPELL_SCALDING_GROUND, { at });
        }
    }

    // Fireball on a Broken Core: the inward collapse, the Fireball's damage again on it (40% more with Implosion);
    // Chain Reaction breaks one other shard within 15 yards; Second Sunrise pulses there 2 sec on, 40% to each
    // enemy within 8 yards, 10% more for each other one there (50% at most).
    void Collapse(Player* player, Unit* host, float fireball)
    {
        host->RemoveAurasDueToSpell(SPELL_BROKEN_CORE, player->GetGUID());
        host->CastSpell(host, SPELL_COLLAPSE_LOOK, true);
        int32 damage = std::max<int32>(1, int32(fireball * COLLAPSE_SHARE
            * (HasSunMarker(player, MOD_IMPLOSION) ? IMPLOSION : 1.0f)));
        Position at = host->GetPosition();
        player->CastCustomSpell(host, SPELL_COLLAPSE, &damage, nullptr, nullptr, true);

        if (HasSunMarker(player, MOD_CHAIN_REACTION))
        {
            PruneShards(player);
            for (ObjectGuid guid : StateOf(player).Shards)
                if (Unit* other = ObjectAccessor::GetUnit(*player, guid); other && other != host
                    && other->GetExactDist2d(&at) <= CHAIN_RANGE)
                {
                    Fracture(player, other, fireball * CHAIN_SHARE);
                    break;
                }
        }
        if (HasSunMarker(player, MOD_SECOND_SUNRISE))
            player->m_Events.AddEventAtOffset([player, at, fireball]()
            {
                if (!player->IsInWorld() || !player->IsAlive())
                    return;
                std::vector<Unit*> enemies = EnemiesIn(player, Circle(at, FRACTURE_RADIUS), FRACTURE_TARGETS);
                if (enemies.empty())
                    return;
                enemies.front()->CastSpell(enemies.front(), SPELL_COLLAPSE_LOOK, true);
                float bonus = std::min(SUNRISE_CAP, SUNRISE_PER_ENEMY * float(enemies.size() - 1));
                int32 pulse = std::max<int32>(1, int32(fireball * SUNRISE_SHARE * (1.0f + bonus)));
                for (Unit* enemy : enemies)
                    player->CastCustomSpell(enemy, SPELL_COLLAPSE, &pulse, nullptr, nullptr, true);
            }, Milliseconds(SUNRISE_DELAY_MS));
    }

    // Scalding Ground's ticks.
    void SunTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        if (!state.ScaldUntil)
            return;
        if (Now() >= state.ScaldUntil || state.ScaldMap != player->GetMapId())
        {
            state.ScaldUntil = 0;
            player->RemoveDynObject(SPELL_SCALDING_GROUND);
            return;
        }
        state.ScaldTimer += diff;
        if (state.ScaldTimer < 1000)
            return;
        state.ScaldTimer = 0;
        int32 damage = std::max<int32>(1, int32(state.ScaldTick));
        for (Unit* enemy : EnemiesIn(player, Circle(state.ScaldAt, SCALDING_RADIUS), FRACTURE_TARGETS))
            player->CastCustomSpell(enemy, SPELL_FRACTURE, &damage, nullptr, nullptr, true);
    }

    // What the five leave behind (death, logout, a spec or modifier change): scars, the wake, a pending echo and
    // its ground, the echoes, the shards. The vow and the Dominion stay.
    void ClearLegends(Player* player)
    {
        State& state = StateOf(player);
        for (uint8 scar = 0; scar < SCARS; ++scar)
            ClearScar(player, scar);
        state.InBlight.clear();
        state.LingerUntil = 0;
        EndWake(player);
        state.Pending = false;
        state.SacredUntil = 0;
        player->RemoveDynObject(SPELL_SACRED_GROUND);
        DismissEchoes(player);
        state.FlayEchoed = true;
        for (ObjectGuid guid : state.Shards)
            if (Unit* host = ObjectAccessor::GetUnit(*player, guid))
                host->RemoveAurasDueToSpell(SPELL_SUNSHARD, player->GetGUID());
        state.Shards.clear();
        state.Anchors.clear();
        player->RemoveDynObject(SPELL_SOLAR_ANCHOR);
        state.ScaldUntil = 0;
        player->RemoveDynObject(SPELL_SCALDING_GROUND);
    }

    // The actions the rune teaches while it is the active spec's (taught again at each login, never saved):
    // Requiem (Chosen Ending: its three), Dawn Vow; the Dominion's buff; the vow on the Hero until they choose.
    void ApplyLegend(Player* player, uint8 rune)
    {
        bool grave = rune == RUNE_THREEFOLD_GRAVE;
        bool chosen = grave && HasGraveMarker(player, MOD_CHOSEN_ENDING);
        auto teach = [player](uint32 spellId, bool wanted)
        {
            if (wanted && !player->HasSpell(spellId))
                player->learnSpell(spellId, true);
            else if (!wanted && player->HasSpell(spellId))
                player->removeSpell(spellId, SPEC_MASK_ALL, true);
        };
        teach(SPELL_REQUIEM, grave && !chosen);
        for (uint8 dominion = 0; dominion < DOMINIONS; ++dominion)
            teach(SPELL_REQUIEM_FIRST + dominion, chosen);
        teach(SPELL_DAWN_VOW, rune == RUNE_DAWNBOUND_CRUSADER);

        State& state = StateOf(player);
        if (grave)
        {
            if (!player->HasAura(SPELL_DOMINION_FIRST + state.Dominion))
                SetDominion(player, state.Dominion, false);
        }
        else
            for (uint8 dominion = 0; dominion < DOMINIONS; ++dominion)
                player->RemoveAurasDueToSpell(SPELL_DOMINION_FIRST + dominion);
        if (rune != RUNE_DAWNBOUND_CRUSADER)
            DropVow(player);
        else if (VowOf(player) == player)
            Vow(player, player);
    }

    // ---- The rune's auras ------------------------------------------------------------------------------------

    std::vector<uint32> RuneAuras()
    {
        std::vector<uint32> auras;
        for (RuneDef const& rune : Catalog())
        {
            auras.push_back(rune.Aura);
            for (uint32 i = 0; i < SLOTS * CHOICES; ++i)
                auras.push_back(rune.MarkerFirst + i);
        }
        return auras;
    }

    // The active spec's rune and modifiers as auras (none below UNLOCK_LEVEL).
    void Apply(Player* player)
    {
        for (uint32 spell : RuneAuras())
            player->RemoveAurasDueToSpell(spell);
        Choice const& choice = ActiveChoice(player);
        RuneDef const* rune = FindRune(choice.Rune);
        if (player->GetLevel() < UNLOCK_LEVEL || !rune)
        {
            ClearCombat(player);
            ApplyLegend(player, 0);
            return;
        }
        if (rune->Id != RUNE_SANCTIFIED_PARAGON)
            player->RemoveAurasDueToSpell(SPELL_SANCTIFIED_PARAGON);
        player->AddAura(rune->Aura, player);
        for (uint8 slot = 0; slot < SLOTS; ++slot)
            if (uint8 pick = choice.Mods[slot])
                player->AddAura(rune->MarkerFirst + slot * CHOICES + pick - 1, player);
        ApplyLegend(player, rune->Id);
    }

    void Save(Player* player, uint8 spec)
    {
        Choice const& choice = StateOf(player).Choices[spec];
        CharacterDatabase.Execute(Acore::StringFormat("REPLACE INTO `titan_runes` (`guid`, `spec`, `rune`, "
            "`invocation`, `manifestation`, `adaptation`) VALUES ({}, {}, {}, {}, {}, {})",
            player->GetGUID().GetCounter(), spec, choice.Rune, choice.Mods[0], choice.Mods[1], choice.Mods[2]));
    }

    void Load(Player* player)
    {
        State& state = StateOf(player);
        state = State();
        if (QueryResult result = CharacterDatabase.Query("SELECT `spec`, `rune`, `invocation`, `manifestation`, "
            "`adaptation` FROM `titan_runes` WHERE `guid` = {}", player->GetGUID().GetCounter()))
        {
            do
            {
                Field* fields = result->Fetch();
                uint8 spec = fields[0].Get<uint8>();
                if (spec >= MAX_SPECS)
                    continue;
                Choice& choice = state.Choices[spec];
                choice.Rune = fields[1].Get<uint8>();
                for (uint8 slot = 0; slot < SLOTS; ++slot)
                    choice.Mods[slot] = std::min<uint8>(fields[2 + slot].Get<uint8>(), CHOICES);
            } while (result->NextRow());
        }
    }

    // ---- The panel ---------------------------------------------------------------------------------------------

    bool Prompt(Player* player)
    {
        return Wildcard::IsHero(player) && player->GetLevel() >= UNLOCK_LEVEL && !ActiveChoice(player).Rune
            && !Specs::IsDormant(player);
    }

    void SendState(Player* player)
    {
        Choice const& choice = ActiveChoice(player);
        uint8 role = Wildcard::GetRole(player);
        // Suggested: runes the Hero meets, their role's first (a Tank or Healer sees a hybrid only in Browse all).
        std::vector<RuneDef const*> offers;
        for (RuneDef const& rune : Catalog())
            if (Missing(player, rune.Requires) == "-"
                && (role == Wildcard::ROLE_NONE || rune.Roles & Wildcard::RoleMask(Wildcard::Role(role))))
                offers.push_back(&rune);
        if (offers.size() > MAX_OFFERS)
            offers.resize(MAX_OFFERS);
        std::string offerList;
        for (RuneDef const* rune : offers)
            offerList += (offerList.empty() ? "" : ".") + std::to_string(rune->Id);

        for (RuneDef const& rune : Catalog())
        {
            Send(player, Acore::StringFormat("R,{},{}", rune.Id, Missing(player, rune.Requires)));
            for (uint8 slot = 0; slot < SLOTS; ++slot)
                for (uint8 pick = 0; pick < CHOICES; ++pick)
                {
                    std::vector<std::vector<uint32>> needs = ModifierNeeds(rune.Id, slot, pick);
                    if (!needs.empty())
                        Send(player, Acore::StringFormat("N,{},{},{},{}", rune.Id, slot + 1, pick + 1,
                            Missing(player, needs)));
                }
        }
        Send(player, Acore::StringFormat("S,{},{},{},{},{},{},{},{},{}", player->GetLevel(), UNLOCK_LEVEL,
            Specs::ActiveSpec(player) + 1, choice.Rune, choice.Mods[0], choice.Mods[1], choice.Mods[2],
            offerList.empty() ? "-" : offerList, Prompt(player) ? 1 : 0));
    }

    void Refuse(Player* player, std::string const& why)
    {
        Send(player, "E," + why);
        ChatHandler(player->GetSession()).PSendSysMessage("|cffa335eeLegendary Rune:|r {}", why);
    }

    void OnPick(Player* player, uint8 runeId, bool anyway)
    {
        Choice& choice = ActiveChoice(player);
        RuneDef const* rune = FindRune(runeId);
        if (player->GetLevel() < UNLOCK_LEVEL)
            return Refuse(player, Acore::StringFormat("Legendary Runes unlock at level {}.", UNLOCK_LEVEL));
        if (choice.Rune)
            return Refuse(player, "This specialization already has its rune: only a prestige changes it.");
        if (Specs::IsDormant(player))
            return Refuse(player, "This specialization is empty until you prestige in it.");
        if (!rune)
            return Refuse(player, "That rune is not available yet.");
        if (Missing(player, rune->Requires) != "-" && !anyway)
            return Refuse(player, "You lack an ability this rune needs.");
        choice = Choice();
        choice.Rune = runeId;
        Save(player, Specs::ActiveSpec(player));
        Apply(player);
        ChatHandler(player->GetSession()).PSendSysMessage("|cffa335eeLegendary Rune:|r bound to this "
            "specialization until your next prestige. Choose its modifiers in the rune panel.");
        SendState(player);
    }

    void OnModifier(Player* player, uint8 slot, uint8 pick)
    {
        Choice& choice = ActiveChoice(player);
        if (!choice.Rune || slot < 1 || slot > SLOTS || pick < 1 || pick > CHOICES)
            return;
        if (player->IsInCombat())
            return Refuse(player, "Modifiers can't be changed in combat.");
        if (Missing(player, ModifierNeeds(choice.Rune, slot - 1, pick - 1)) != "-")
            return Refuse(player, "You lack an ability this modifier needs.");
        // The Sanctified Paragon: Bastion of Faith and Weight of the Unworthy work on its damage only (Sanctified
        // Condemnation); another Manifestation lets go of either.
        if (choice.Rune == RUNE_SANCTIFIED_PARAGON)
        {
            uint8 condemnation = MOD_SANCTIFIED_CONDEMNATION % CHOICES + 1;
            auto damageOnly = [](uint8 covenant) { return covenant == MOD_BASTION_OF_FAITH % CHOICES + 1
                || covenant == MOD_WEIGHT_OF_THE_UNWORTHY % CHOICES + 1; };
            if (slot == 3 && damageOnly(pick) && choice.Mods[1] != condemnation)
                return Refuse(player, "That modifier needs Sanctified Condemnation as your Manifestation.");
            if (slot == 2 && pick != condemnation && damageOnly(choice.Mods[2]))
            {
                choice.Mods[2] = 0;
                ChatHandler(player->GetSession()).SendSysMessage("|cffa335eeLegendary Rune:|r your Covenant needs "
                    "Sanctified Condemnation: choose it again.");
            }
        }
        if (choice.Mods[slot - 1] == pick)
            return SendState(player);
        choice.Mods[slot - 1] = pick;
        Save(player, Specs::ActiveSpec(player));
        ClearCombat(player);
        Apply(player);
        SendState(player);
    }

    void OnMessage(Player* player, std::string_view message)
    {
        std::vector<std::string_view> parts;
        for (std::size_t start = 0; start <= message.size();)
        {
            std::size_t end = message.find(',', start);
            if (end == std::string_view::npos)
                end = message.size();
            parts.push_back(message.substr(start, end - start));
            start = end + 1;
        }
        auto number = [&parts](std::size_t i) -> uint32
        {
            return i < parts.size() ? Acore::StringTo<uint32>(parts[i]).value_or(0) : 0;
        };
        if (!Wildcard::IsHero(player) || parts.empty())
            return;
        if (parts[0] == "STATE")
            SendState(player);
        else if (parts[0] == "PICK")
            OnPick(player, uint8(number(1)), number(2) == 1);
        else if (parts[0] == "MOD")
            OnModifier(player, uint8(number(1)), uint8(number(2)));
    }
}

namespace LegendaryRunes
{
    void OnSpecChanged(Player* player)
    {
        ClearCombat(player);
        Apply(player);
        SendState(player);
        if (Prompt(player))
            Send(player, "O");
    }

    void OnPrestige(Player* player)
    {
        CharacterDatabase.Execute(Acore::StringFormat("DELETE FROM `titan_runes` WHERE `guid` = {}",
            player->GetGUID().GetCounter()));
        State& state = StateOf(player);
        state.Choices = {};
        ClearCombat(player);
        Apply(player);
    }

    Unit* FelRiftTarget(Player* player)
    {
        if (!player->HasAura(SPELL_FEL_RIFT))
            return nullptr;
        State& state = StateOf(player);
        Unit* target = state.RiftTarget ? ObjectAccessor::GetUnit(*player, state.RiftTarget) : nullptr;
        return target && target->IsAlive() && player->IsValidAttackTarget(target) ? target : nullptr;
    }
}

// The rune's aura (90700): Demon's Bite, Chaos Strike, Shadow Bolt and Incinerate hits grant Fel Command (a
// Chaos Strike crit 2); with Dreadfire Discipline, Shadow Bolt and Incinerate 2 and Demon's Bite none. A volley
// bolt's crit takes 3 sec off Throw Glaive with Shattered Glaive (9 sec a volley at most).
class spell_titan_rune_fel_commandment : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_fel_commandment);

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        SpellInfo const* spell = eventInfo.GetSpellInfo();
        Player* player = GetTarget()->ToPlayer();
        if (!spell || !player || !(eventInfo.GetHitMask() & (PROC_HIT_NORMAL | PROC_HIT_CRITICAL | PROC_HIT_ABSORB)))
            return false;
        bool crit = eventInfo.GetHitMask() & PROC_HIT_CRITICAL;
        uint32 first = spell->GetFirstRankSpell()->Id;

        if (first == SPELL_VOLLEY_BOLT)
        {
            State& state = StateOf(player);
            if (crit && HasMarker(player, MOD_SHATTERED_GLAIVE) && state.ShatterBudget > 0)
            {
                player->ModifySpellCooldown(SPELL_THROW_GLAIVE, -SHATTER_CUT_MS);
                state.ShatterBudget -= SHATTER_CUT_MS;
            }
            return false;
        }

        bool dreadfire = HasMarker(player, MOD_DREADFIRE_DISCIPLINE);
        if (first == SPELL_CHAOS_STRIKE)
            Gain(player, crit ? 2 : 1);
        else if (first == SPELL_DEMONS_BITE && !dreadfire)
            Gain(player, 1);
        else if (first == SPELL_SHADOW_BOLT || first == SPELL_INCINERATE)
            Gain(player, dreadfire ? 2 : 1);
        return false;                               // the stacks are granted here; nothing else to proc
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_titan_rune_fel_commandment::CheckProc);
    }
};

// Throw Glaive (90111) with the rune at 5 Fel Command: Chaos Bolt Volley. Not when the only enemy about is
// crowd-controlled (the volley would break it). A triggered throw (Shattered Glaive's bounce) never fires one.
class spell_titan_rune_throw_glaive : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_throw_glaive);

    bool Armed()
    {
        Player* player = GetCaster()->ToPlayer();
        return player && !GetSpell()->IsTriggered() && player->HasAura(SPELL_RUNE_FEL_COMMANDMENT)
            && Stacks(player) >= MAX_STACKS;
    }

    SpellCastResult CheckTarget()
    {
        Unit* target = GetExplTargetUnit();
        if (!Armed() || !target || !target->HasBreakableByDamageCrowdControlAura())
            return SPELL_CAST_OK;
        Player* player = GetCaster()->ToPlayer();
        return EnemiesAround(player, target, BOLT_RANGE).empty() ? SPELL_FAILED_BAD_TARGETS : SPELL_CAST_OK;
    }

    void Volley()
    {
        Unit* target = GetExplTargetUnit();
        Player* player = GetCaster()->ToPlayer();
        if (!target || !player || GetSpell()->IsTriggered() || !player->HasAura(SPELL_RUNE_FEL_COMMANDMENT))
            return;
        // Imp Legion / Fel Host: every throw spends the Fel Command, a demon for each stack (Felguards in
        // Metamorphosis with Fel Host), before the volley so its bolts count them.
        uint8 stacks = Stacks(player);
        bool armed = Armed();
        if (Legion(player) && stacks)
        {
            bool felguards = HasMarker(player, MOD_FEL_HOST) && InMetamorphosis(player);
            for (uint8 i = 0; i < stacks; ++i)
                SummonDemon(player, target, felguards);
            if (!armed)
                player->RemoveAurasDueToSpell(SPELL_FEL_COMMAND);
        }
        if (armed)
            FireVolley(player, target);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_titan_rune_throw_glaive::CheckTarget);
        AfterCast += SpellCastFn(spell_titan_rune_throw_glaive::Volley);
    }
};

// Consecration (every rank) with the Sanctified Paragon: no ground left behind; the cast turns the aura on (paying
// Consecration's cost and starting its cooldown), and a cast while it is on turns it off (free, no cooldown).
class spell_titan_rune_consecration : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_consecration);

    SpellCastResult Toggle()
    {
        Unit* caster = GetCaster();
        if (!caster->HasAura(SPELL_RUNE_SANCTIFIED_PARAGON) || !caster->HasAura(SPELL_SANCTIFIED_PARAGON))
            return SPELL_CAST_OK;
        caster->RemoveAurasDueToSpell(SPELL_SANCTIFIED_PARAGON);
        return SPELL_FAILED_DONT_REPORT;
    }

    void Sanctify(SpellEffIndex effIndex)
    {
        Unit* caster = GetCaster();
        if (!caster->HasAura(SPELL_RUNE_SANCTIFIED_PARAGON))
            return;
        PreventHitDefaultEffect(effIndex);
        caster->AddAura(SPELL_SANCTIFIED_PARAGON, caster);
        caster->CastSpell(caster, SPELL_PARAGON_AWAKEN, true);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_titan_rune_consecration::Toggle);
        OnEffectHit += SpellEffectFn(spell_titan_rune_consecration::Sanctify, EFFECT_0,
            SPELL_EFFECT_PERSISTENT_AREA_AURA);
    }
};

// Sanctified Paragon (90731): a second of sanctified ground each tick (SanctifiedGround); its bonuses go with it.
class spell_titan_rune_sanctified_paragon : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_sanctified_paragon);

    void Tick(AuraEffect const* /*aurEff*/)
    {
        if (Player* player = GetTarget()->ToPlayer())
        {
            if (!player->HasAura(SPELL_RUNE_SANCTIFIED_PARAGON) || !SanctifiedGround(player))
                return Remove();
        }
    }

    void Gone(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        target->RemoveAurasDueToSpell(SPELL_SEAT_OF_THE_PARAGON);
        target->RemoveAurasDueToSpell(SPELL_BASTION_OF_FAITH);
        if (Player* player = target->ToPlayer())
        {
            State& state = StateOf(player);
            state.Trail.clear();
            state.HasLastPosition = false;
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_titan_rune_sanctified_paragon::Tick, EFFECT_0,
            SPELL_AURA_PERIODIC_DUMMY);
        AfterEffectRemove += AuraEffectRemoveFn(spell_titan_rune_sanctified_paragon::Gone, EFFECT_0,
            SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// The Stormbound Conduit's aura (90750): a Lightning Bolt hit makes its target Conductive (Branching Current: one
// more enemy within 8 yards; Distant Conductor: a Lightning Shield retaliation at it, a charge each); a Chain
// Lightning hit on a Conductive enemy (each jump's) arcs for 15% of Lightning Bolt's damage.
class spell_titan_rune_stormbound_conduit : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_stormbound_conduit);

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        SpellInfo const* spell = eventInfo.GetSpellInfo();
        Player* player = GetTarget()->ToPlayer();
        Unit* target = eventInfo.GetActionTarget();
        if (!spell || !player || !target || target == player || !target->IsAlive()
            || !(eventInfo.GetHitMask() & (PROC_HIT_NORMAL | PROC_HIT_CRITICAL | PROC_HIT_ABSORB)))
            return false;
        uint32 first = spell->GetFirstRankSpell()->Id;
        if (first == SPELL_LIGHTNING_BOLT)
        {
            MakeConductive(player, target);
            if (HasStormMarker(player, MOD_BRANCHING_CURRENT))
                for (Unit* enemy : EnemiesAround(player, target, BRANCHING_RANGE))
                    if (enemy != target)
                    {
                        MakeConductive(player, enemy);
                        break;
                    }
            if (HasStormMarker(player, MOD_DISTANT_CONDUCTOR))
                if (AuraEffect* shield = Shield(player))
                    Retaliate(player, target, shield, false);
        }
        else if (first == SPELL_CHAIN_LIGHTNING && IsConductive(player, target))
            Arc(player, target, LightningBoltDamage(player, target) * CHAIN_ARC_SHARE, false);
        return false;                               // the rune's work is done here; nothing else to proc
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_titan_rune_stormbound_conduit::CheckProc);
    }
};

// Lightning Shield (every rank) with the Stormbound Conduit: Storm Within and Distant Conductor replace the
// retaliation when struck.
class spell_titan_rune_lightning_shield : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_lightning_shield);

    bool CheckProc(ProcEventInfo& /*eventInfo*/)
    {
        Player* player = GetTarget()->ToPlayer();
        return !player || !player->HasAura(SPELL_RUNE_STORMBOUND_CONDUIT)
            || (!HasStormMarker(player, MOD_STORM_WITHIN) && !HasStormMarker(player, MOD_DISTANT_CONDUCTOR));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_titan_rune_lightning_shield::CheckProc);
    }
};

// A Lightning Shield retaliation (its damage, every rank, and the rune's own 90754) with the Stormbound Conduit:
// Violent Reprisal takes 10% off it; it arcs to a Conductive enemy near the target for 25% of its damage (40% more
// with Violent Reprisal). A Storm Within pulse never arcs.
class spell_titan_rune_lightning_shield_damage : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_lightning_shield_damage);

    void Strike()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_STORMBOUND_CONDUIT))
            return;
        int32 damage = GetHitDamage();
        if (HasStormMarker(player, MOD_VIOLENT_REPRISAL))
        {
            damage = int32(damage * VIOLENT_RETALIATION);
            SetHitDamage(damage);
        }
        if (StateOf(player).Pulsing)
            return;
        Arc(player, target, damage * SHIELD_ARC_SHARE, true);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_titan_rune_lightning_shield_damage::Strike);
    }
};

// Thunder Clap (every rank) with Echoing Thunder: its storm (PlaceThunderfield).
class spell_titan_rune_thunder_clap : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_thunder_clap);

    void Storm()
    {
        if (Player* player = GetCaster()->ToPlayer())
            if (player->HasAura(SPELL_RUNE_ECHOING_THUNDER))
                PlaceThunderfield(player);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_rune_thunder_clap::Storm);
    }
};

// Shield Slam (every rank) with Echoing Thunder: landed from inside the Thunderfield, a Thunder Echo (its damage
// before the target's mitigation).
class spell_titan_rune_shield_slam : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_shield_slam);

    void Echo()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target && player->HasAura(SPELL_RUNE_ECHOING_THUNDER) && InThunderfield(player))
            ThunderEcho(player, target, GetHitDamage());
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_titan_rune_shield_slam::Echo);
    }
};

// Chain Heal (every rank) with the Wellspring: healing the Earth Shield wearer makes them the Wellspring; reaching
// the Wellspring (not one this cast made) sends its additional jump and refreshes it while they wear the shield.
// One jump a cast.
class spell_titan_rune_chain_heal : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_chain_heal);

    void Flow()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_WELLSPRING))
            return;
        bool shielded = HasEarthShield(player, target);
        if (IsWellspring(player, target) && !_jumped && target->GetGUID() != _made)
        {
            _jumped = true;
            WellspringJump(player, target, GetHitHeal());
            if (shielded)
                MakeWellspring(player, target);
            return;
        }
        if (shielded && !IsWellspring(player, target))
        {
            MakeWellspring(player, target);
            _made = target->GetGUID();
        }
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_chain_heal::Flow);
    }

    bool _jumped = false;
    ObjectGuid _made;
};

// Earth Shield's heal (every rank) with Flowing Waters: healing its wearer refreshes their Wellspring to 6 sec.
class spell_titan_rune_earth_shield_heal : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_earth_shield_heal);

    void Flow()
    {
        Unit* origin = GetOriginalCaster() ? GetOriginalCaster() : GetCaster();
        Player* player = origin ? origin->ToPlayer() : nullptr;
        Unit* target = GetHitUnit();
        if (!player || !target || !HasWellMarker(player, MOD_FLOWING_WATERS) || !IsWellspring(player, target))
            return;
        Aura* aura = target->GetAura(SPELL_WELLSPRING, player->GetGUID());
        if (aura && aura->GetDuration() < WELLSPRING_MS)
        {
            aura->SetMaxDuration(std::max(aura->GetMaxDuration(), WELLSPRING_MS));
            aura->SetDuration(WELLSPRING_MS);
        }
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_earth_shield_heal::Flow);
    }
};

// Soothing Mist's cast with the Wandering Mist: the channel's start (Effortless Breathing), and Rolling Mist when it
// moves straight from one ally to another: a lingering mist on the last, a tick over 4 sec, once every 12 sec.
class spell_titan_rune_soothing_mist_cast : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_soothing_mist_cast);

    void Start()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetExplTargetUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_WANDERING_MIST))
            return;
        State& state = StateOf(player);
        uint32 now = GameTime::GetGameTimeMS().count();
        if (HasMistMarker(player, MOD_ROLLING_MIST) && state.MistTarget && state.MistTarget != target->GetGUID()
            && now - state.MistLastTick <= ROLLING_SWITCH_MS && now - state.LastRolling >= ROLLING_INTERVAL_MS)
            if (Unit* last = ObjectAccessor::GetUnit(*player, state.MistTarget); last && last->IsAlive())
            {
                state.LastRolling = now;
                Refresh(player, last, SPELL_ROLLING_MIST, std::max<int32>(1, state.MistTickAmount / ROLLING_TICKS));
            }
        state.MistTarget = target->GetGUID();
        state.MistSince = now;
    }

    void Register() override
    {
        OnCast += SpellCastFn(spell_titan_rune_soothing_mist_cast::Start);
    }
};

// Soothing Mist's ticks with the Wandering Mist: a 25% chance, once every 2 sec, of a Wandering Mist for 30% of the
// tick's effective healing; with Effortless Breathing, from 3 sec into the channel, 15% of a tick's mana back.
class spell_titan_rune_soothing_mist : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_soothing_mist);

    void Tick(AuraEffect const* aurEff)
    {
        Unit* caster = GetCaster();
        Player* player = caster ? caster->ToPlayer() : nullptr;
        Unit* target = GetTarget();
        if (!player || !player->HasAura(SPELL_RUNE_WANDERING_MIST))
            return;
        State& state = StateOf(player);
        uint32 now = GameTime::GetGameTimeMS().count();
        state.MistTarget = target->GetGUID();
        state.MistLastTick = now;
        state.MistTickAmount = aurEff->GetAmount();

        if (HasMistMarker(player, MOD_EFFORTLESS_BREATHING) && now - state.MistSince >= EFFORTLESS_AFTER_MS)
        {
            int32 cost = int32(GetSpellInfo()->ManaCostPercentage * player->GetCreateMana() / 100);
            if (int32 back = int32(cost * EFFORTLESS_SHARE / SOOTHING_MIST_TICKS); back > 0)
                player->ModifyPower(POWER_MANA, back);
        }

        int32 effective = Effective(target, aurEff->GetAmount());
        if (effective > 0 && now - state.LastWander >= WANDER_INTERVAL_MS && roll_chance_i(WANDER_CHANCE))
        {
            state.LastWander = now;
            Wander(player, target, effective * WANDER_SHARE);
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_titan_rune_soothing_mist::Tick, EFFECT_0,
            SPELL_AURA_PERIODIC_HEAL);
    }
};

// Enveloping Mist and Vivify with the Wandering Mist: on the Soothing Mist target, cast at once (their mana and
// cooldowns as ever) without breaking the channel; then Comforting Veil (Enveloping Mist: a lesser one on the most
// recent Wandering Mist target, 15% of it) or Vivacious Flow (Vivify: 20% of its healing to that ally).
class spell_titan_rune_mist_technique : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_mist_technique);

    SpellCastResult Weave()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetExplTargetUnit();
        if (!player || !target || GetSpell()->IsTriggered() || !player->HasAura(SPELL_RUNE_WANDERING_MIST)
            || MistChannelTarget(player) != target)
            return SPELL_CAST_OK;
        player->CastSpell(target, GetSpellInfo()->Id, TriggerCastFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS
            | TRIGGERED_CAST_DIRECTLY));
        return SPELL_FAILED_DONT_REPORT;
    }

    void Share()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_WANDERING_MIST) || MistChannelTarget(player) != target)
            return;
        Unit* ally = SecondaryAlly(player, target);
        if (!ally)
            return;
        State& state = StateOf(player);
        uint32 first = GetSpellInfo()->GetFirstRankSpell()->Id;
        if (first == SPELL_ENVELOPING_MIST && HasMistMarker(player, MOD_COMFORTING_VEIL))
        {
            AuraEffect const* mist = target->GetAuraEffect(SPELL_ENVELOPING_MIST, EFFECT_0, player->GetGUID());
            if (!mist)
                return;
            if (state.LesserTarget && state.LesserTarget != ally->GetGUID())
                if (Unit* last = ObjectAccessor::GetUnit(*player, state.LesserTarget))
                    last->RemoveAurasDueToSpell(SPELL_LESSER_ENVELOPING_MIST, player->GetGUID());
            state.LesserTarget = ally->GetGUID();
            Refresh(player, ally, SPELL_LESSER_ENVELOPING_MIST,
                std::max<int32>(1, int32(mist->GetAmount() * VEIL_SHARE)));
        }
        else if (first == SPELL_VIVIFY && HasMistMarker(player, MOD_VIVACIOUS_FLOW))
        {
            int32 heal = std::max<int32>(1, int32(GetHitHeal() * VIVACIOUS_SHARE));
            player->CastCustomSpell(ally, SPELL_WANDERING_MIST, &heal, nullptr, nullptr, true);
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_titan_rune_mist_technique::Weave);
        AfterHit += SpellHitFn(spell_titan_rune_mist_technique::Share);
    }
};

// Requiem with the Threefold Grave: the next Dominion; Chosen Ending's three: theirs.
class spell_titan_rune_requiem : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_requiem);

    void Turn()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player || !player->HasAura(SPELL_RUNE_THREEFOLD_GRAVE))
            return;
        uint32 id = GetSpellInfo()->Id;
        SetDominion(player, id == SPELL_REQUIEM ? NextDominion(player) : uint8(id - SPELL_REQUIEM_FIRST), true);
        if (id == SPELL_REQUIEM)
            return;
        // Chosen Ending: the three share their cooldown (each its own in the data, the others set here).
        WorldPacket data(SMSG_SPELL_COOLDOWN, 8 + 1 + 2 * 8);
        data << player->GetGUID();
        data << uint8(0);
        for (uint8 dominion = 0; dominion < DOMINIONS; ++dominion)
            if (uint32 other = SPELL_REQUIEM_FIRST + dominion; other != id)
            {
                player->AddSpellCooldown(other, 0, CHOSEN_ENDING_MS);
                data << uint32(other) << uint32(CHOSEN_ENDING_MS);
            }
        player->SendDirectMessage(&data);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_rune_requiem::Turn);
    }
};

// Icy Touch, Plague Strike and Death Strike (every rank) with the Threefold Grave: their scar; Death Strike's
// Grave Ward first (standing in a scar as it lands).
class spell_titan_rune_scar_strike : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_scar_strike);

    void Inscribe()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_THREEFOLD_GRAVE) || _done)
            return;
        _done = true;
        uint32 first = GetSpellInfo()->GetFirstRankSpell()->Id;
        if (first == SPELL_DEATH_STRIKE)
        {
            GraveWard(player, GetHitDamage());
            ::Inscribe(player, SCAR_BLOOD_CIRCLE, target);
        }
        else
            ::Inscribe(player, first == SPELL_ICY_TOUCH ? SCAR_FROZEN_LINE : SCAR_DISEASED_PATCH, target);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_scar_strike::Inscribe);
    }

    bool _done = false;
};

// Mortal Strike and Bloodthirst (every rank) with Unbroken Onslaught: the other one through the wake reopens it,
// then either cuts a new wake through its target.
class spell_titan_rune_wake_cut : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_wake_cut);

    void Cut()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_UNBROKEN_ONSLAUGHT))
            return;
        uint32 first = GetSpellInfo()->GetFirstRankSpell()->Id;
        if (StateOf(player).WakeCreator != first && InWake(player, target))
            ReopenWake(player, target);
        if (target->IsAlive())
            CutWake(player, target, first);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_wake_cut::Cut);
    }
};

// Unbroken Onslaught's aura: any other melee ability striking an enemy in the wake reopens it.
class spell_titan_rune_unbroken_onslaught : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_unbroken_onslaught);

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Player* player = GetTarget()->ToPlayer();
        SpellInfo const* spell = eventInfo.GetSpellInfo();
        Unit* struck = eventInfo.GetActionTarget();
        if (!player || !spell || !struck || struck == player || spell->DmgClass != SPELL_DAMAGE_CLASS_MELEE)
            return false;
        uint32 first = spell->GetFirstRankSpell()->Id;
        if (first == SPELL_MORTAL_STRIKE || first == SPELL_BLOODTHIRST
            || (spell->Id >= SPELL_RUNE_UNBROKEN_ONSLAUGHT && spell->Id < SPELL_RUNE_DAWNBOUND_CRUSADER))
            return false;
        if (InWake(player, struck))
            ReopenWake(player, struck);
        return false;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_titan_rune_unbroken_onslaught::CheckProc);
    }
};

// Dawn Vow: the vow moves to the ally.
class spell_titan_rune_dawn_vow : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_dawn_vow);

    void Swear()
    {
        Player* player = GetCaster()->ToPlayer();
        if (Unit* ally = GetHitUnit(); player && ally && player->HasAura(SPELL_RUNE_DAWNBOUND_CRUSADER))
            Vow(player, ally);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_dawn_vow::Swear);
    }
};

// A Judgement with the Dawnbound Crusader: the Dawn Mark on its target (one at a time).
class spell_titan_rune_judgement : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_judgement);

    void Mark()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target && target->IsAlive() && player->HasAura(SPELL_RUNE_DAWNBOUND_CRUSADER))
            ::Mark(player, target);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_judgement::Mark);
    }
};

// Crusader Strike on the Dawn Mark's enemy: a Dawn Echo pending. Holy Shock's damage or heal (every rank): an echo
// at once, then Steadfast's afterimage on the vow.
class spell_titan_rune_dawn_echo : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_dawn_echo);

    void Echo()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_DAWNBOUND_CRUSADER))
            return;
        if (GetSpellInfo()->GetFirstRankSpell()->Id == SPELL_CRUSADER_STRIKE)
        {
            if (target->HasAura(SPELL_DAWN_MARK, player->GetGUID()))
                QueueEcho(player, float(GetHitDamage()));
            return;
        }
        QueueEcho(player, float(GetHitDamage() + GetHitHeal()));
        ReleaseEcho(player);
        if (!HasDawnMarker(player, MOD_STEADFAST))
            return;
        Unit* vow = VowOf(player);
        vow->RemoveAurasDueToSpell(SPELL_DAWN_AFTERIMAGE, player->GetGUID());
        if (Aura* afterimage = player->AddAura(SPELL_DAWN_AFTERIMAGE, vow))
        {
            afterimage->SetMaxDuration(AFTERIMAGE_MS);
            afterimage->SetDuration(AFTERIMAGE_MS);
        }
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_dawn_echo::Echo);
    }
};

// Mind Flay (every rank) with the Hollow Congregation: its start, for the echo two seconds in.
class spell_titan_rune_mind_flay : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_mind_flay);

    void Start()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player || !player->HasAura(SPELL_RUNE_HOLLOW_CONGREGATION))
            return;
        State& state = StateOf(player);
        state.FlayStart = Now();
        state.FlayEchoed = false;
        state.FlayTarget = GetExplTargetUnit() ? GetExplTargetUnit()->GetGUID() : ObjectGuid::Empty;
    }

    void Register() override
    {
        OnCast += SpellCastFn(spell_titan_rune_mind_flay::Start);
    }
};

// Mind Blast (every rank) with the Hollow Congregation: the echoes' command.
class spell_titan_rune_mind_blast : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_mind_blast);

    void Command()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target && target->IsAlive() && player->HasAura(SPELL_RUNE_HOLLOW_CONGREGATION))
            CommandEchoes(player, target);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_mind_blast::Command);
    }
};

// Fireball and Frostbolt (every rank) with the Shattered Sun: a Fireball collapses a Broken Core or embeds a shard,
// a Frostbolt fractures a shard.
class spell_titan_rune_shattered_sun : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_shattered_sun);

    void Strike()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_SHATTERED_SUN))
            return;
        float damage = float(std::max(1, GetHitDamage()));
        if (GetSpellInfo()->GetFirstRankSpell()->Id == SPELL_FROSTBOLT)
        {
            if (HasShard(player, target))
                Fracture(player, target, damage);
            return;
        }
        if (target->HasAura(SPELL_BROKEN_CORE, player->GetGUID()))
            Collapse(player, target, damage);
        else if (target->IsAlive())
            Embed(player, target);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_shattered_sun::Strike);
    }
};

// A Hollow Echo (910122): the Hero's look, hollowed; for 10 sec a weaker Mind Flay on its enemy (the one it was left
// for, then the Hero's), 30 yards (40 with Haunted Ground), drifting after the Hero (Following Shadows) or riding
// its enemy (Possession); quiet during a barrage. Last Word: a last, stronger attack as it fades. Silent Witness:
// stronger while the Hero moves and does not channel.
struct npc_titan_rune_hollow_echo : public ScriptedAI
{
    npc_titan_rune_hollow_echo(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        Player* owner = summoner ? summoner->ToPlayer() : nullptr;
        if (!owner)
            return;
        _owner = owner->GetGUID();
        owner->CastSpell(me, SPELL_CLONE_ME, true);
        me->AddAura(SPELL_HOLLOW_ECHO, me);
        me->SetReactState(REACT_PASSIVE);
        if (HasHollowMarker(owner, MOD_FOLLOWING_SHADOWS))
            me->GetMotionMaster()->MoveFollow(owner, 2.5f, frand(1.5f, 4.8f));
        scheduler.Schedule(200ms, [this](TaskContext channel)
        {
            Channel();
            channel.Repeat(500ms);
        });
        scheduler.Schedule(Milliseconds(HOLLOW_ECHO_MS - 400), [this](TaskContext)
        {
            Player* hero = Owner();
            Unit* target = Target(hero);
            if (!hero || !target || !HasHollowMarker(hero, MOD_LAST_WORD))
                return;
            int32 damage = std::max<int32>(1, int32(FlayTick(hero) * HOLLOW_SHARE * LAST_WORD_TICKS));
            me->CastCustomSpell(target, SPELL_HOLLOW_LAST_WORD, &damage, nullptr, nullptr, true);
        });
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id == 0)
            _target = guid;
        else
        {
            _host = guid;
            if (Unit* host = ObjectAccessor::GetUnit(*me, guid))
                me->GetMotionMaster()->MoveFollow(host, 0.5f, frand(0.0f, 2.0f * float(M_PI)));
        }
    }

    void SetData(uint32 /*id*/, uint32 value) override
    {
        _quietUntil = Now() + value;
    }

    void AttackStart(Unit* /*who*/) override { }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }

private:
    Player* Owner() const
    {
        return ObjectAccessor::GetPlayer(*me, _owner);
    }

    // Its enemy: the one it was given (its host with Possession), else the Hero's, else the nearest to it.
    Unit* Target(Player* hero)
    {
        if (!hero)
            return nullptr;
        float range = HasHollowMarker(hero, MOD_HAUNTED_GROUND) ? HAUNTED_RANGE : HOLLOW_RANGE;
        auto fits = [&](Unit* unit)
        {
            return Valid(hero, unit) && me->IsWithinDistInMap(unit, range) && me->IsWithinLOSInMap(unit);
        };
        for (ObjectGuid guid : { _host, _target })
            if (Unit* unit = guid ? ObjectAccessor::GetUnit(*me, guid) : nullptr; fits(unit))
                return unit;
        if (Unit* chosen = hero->GetSelectedUnit(); fits(chosen))
        {
            _target = chosen->GetGUID();
            return chosen;
        }
        for (Unit* enemy : EnemiesAround(hero, me, range))
            if (fits(enemy))
            {
                _target = enemy->GetGUID();
                return enemy;
            }
        return nullptr;
    }

    void Channel()
    {
        if (Now() < _quietUntil || me->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            return;
        Player* hero = Owner();
        Unit* target = Target(hero);
        if (!target)
            return;
        float tick = FlayTick(hero) * HOLLOW_SHARE;
        if (HasHollowMarker(hero, MOD_SILENT_WITNESS) && hero->isMoving()
            && !hero->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            tick *= SILENT_WITNESS;
        int32 damage = std::max<int32>(1, int32(tick));
        me->SetFacingToObject(target);
        me->CastCustomSpell(target, SPELL_HOLLOW_FLAY, &damage, nullptr, nullptr, true);
    }

    ObjectGuid _owner, _target, _host;
    uint32 _quietUntil = 0;
};

// The rune's Felguard (910120): 8 sec at the Hero's side, on the Hero's target (the next when it dies). Felstorm as
// it arrives (a 2 sec spin: 4 ticks), then Cleave and melee. 50% of the Hero's power (spell power or attack power,
// the higher) in its weapon, 30% of their health; no threat.
struct npc_titan_rune_felguard : public ScriptedAI
{
    npc_titan_rune_felguard(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        Player* owner = summoner ? summoner->ToPlayer() : nullptr;
        if (!owner)
            return;
        me->AddAura(SPELL_FELGUARD_GUARDIAN, me);
        uint32 health = std::max<uint32>(1, owner->CountPctFromMaxHealth(30));
        me->SetMaxHealth(health);
        me->SetHealth(health);
        float power = std::max(float(owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_ALL)),
            owner->GetTotalAttackPowerValue(BASE_ATTACK)) * 0.5f;
        float swing = (BoltDamage(owner) * 0.25f) + power / 14.0f * (me->GetAttackTime(BASE_ATTACK) / 1000.0f);
        me->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, swing * 0.9f);
        me->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, swing * 1.1f);
        me->UpdateDamagePhysical(BASE_ATTACK);
        me->SetReactState(REACT_AGGRESSIVE);
        for (uint32 tick = 0; tick < 4; ++tick)
            scheduler.Schedule(Milliseconds(500 * tick), [this](TaskContext) { DoCastSelf(SPELL_FELSTORM, true); });
        scheduler.Schedule(3s, [this](TaskContext cleave)
        {
            if (me->GetVictim())
                DoCastVictim(SPELL_FELGUARD_CLEAVE, true);
            cleave.Repeat(6s);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
        Unit* victim = me->GetVictim();
        if (!victim || !victim->IsAlive())
            if (Unit* owner = me->GetOwner())
                if (Unit* next = owner->GetVictim())
                    AttackStart(next);
        DoMeleeAttackIfReady();
    }
};

// Imp Legion's imp (910121): 10 sec, Firebolt at its enemy every 2 sec from where it stands (a quarter of a volley
// bolt), then the Hero's target; no threat.
struct npc_titan_rune_fel_imp : public ScriptedAI
{
    npc_titan_rune_fel_imp(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        me->AddAura(SPELL_FEL_IMP_GUARDIAN, me);
        me->SetReactState(REACT_AGGRESSIVE);
        scheduler.Schedule(500ms, [this](TaskContext bolt)
        {
            Unit* victim = me->GetVictim();
            if (!victim || !victim->IsAlive())
                if (Unit* owner = me->GetOwner())
                    victim = owner->GetVictim();
            Player* owner = me->GetOwner() ? me->GetOwner()->ToPlayer() : nullptr;
            if (victim && owner)
            {
                int32 damage = std::max<int32>(1, BoltDamage(owner) / 4);
                me->CastCustomSpell(victim, SPELL_FIREBOLT, &damage, nullptr, nullptr, true);
            }
            bolt.Repeat(2s);
        });
    }

    void AttackStart(Unit* who) override
    {
        AttackStartCaster(who, 25.0f);
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }
};

class titan_runes_player : public PlayerScript
{
public:
    titan_runes_player() : PlayerScript("titan_runes_player", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_LEVEL_CHANGED,
        PLAYERHOOK_ON_PLAYER_JUST_DIED,
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_DELETE,
        PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT
    }) { }

    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 language, std::string& msg,
        Player* receiver) override
    {
        std::string const prefix = std::string(PREFIX) + '\t';
        if (language != LANG_ADDON || receiver != player || msg.compare(0, prefix.size(), prefix) != 0)
            return true;
        OnMessage(player, std::string_view(msg).substr(prefix.size()));
        return false;
    }

    void OnPlayerLogin(Player* player) override
    {
        Load(player);
        if (Wildcard::IsHero(player))
            Apply(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        ClearCombat(player);
        DropVow(player);
        states.erase(player->GetGUID().GetCounter());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        CharacterDatabase.Execute(Acore::StringFormat("DELETE FROM `titan_runes` WHERE `guid` = {}",
            guid.GetCounter()));
    }

    // UNLOCK_LEVEL: the panel opens while the active spec has no rune.
    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (!Wildcard::IsHero(player))
            return;
        Apply(player);
        if (oldLevel < UNLOCK_LEVEL && player->GetLevel() >= UNLOCK_LEVEL)
        {
            SendState(player);
            if (Prompt(player))
                Send(player, "O");
        }
    }

    void OnPlayerJustDied(Player* player) override
    {
        ClearCombat(player);
    }

    // Metamorphic Bond on the Hero's demon while they are in Metamorphosis; Storm Within's pulses; the five
    // newest runes' timers.
    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (player->HasAura(SPELL_RUNE_STORMBOUND_CONDUIT))
            StormPulse(player, diff);
        if (player->HasAura(SPELL_RUNE_THREEFOLD_GRAVE))
            GraveTick(player, diff);
        if (player->HasAura(SPELL_RUNE_UNBROKEN_ONSLAUGHT))
            WakeTick(player, diff);
        if (player->HasAura(SPELL_RUNE_DAWNBOUND_CRUSADER))
            DawnTick(player, diff);
        if (player->HasAura(SPELL_RUNE_HOLLOW_CONGREGATION))
            HollowTick(player);
        if (player->HasAura(SPELL_RUNE_SHATTERED_SUN))
            SunTick(player, diff);
        if (!player->HasAura(SPELL_RUNE_FEL_COMMANDMENT))
            return;
        State& state = StateOf(player);
        state.BondTimer += diff;
        if (state.BondTimer < BOND_CHECK_MS)
            return;
        state.BondTimer = 0;
        Pet* demon = Demon(player);
        if (!demon)
            return;
        bool bond = HasMarker(player, MOD_METAMORPHIC_BOND) && InMetamorphosis(player);
        if (bond && !demon->HasAura(SPELL_METAMORPHIC_BOND))
            demon->AddAura(SPELL_METAMORPHIC_BOND, demon);
        else if (!bond && demon->HasAura(SPELL_METAMORPHIC_BOND))
            demon->RemoveAurasDueToSpell(SPELL_METAMORPHIC_BOND);
    }
};

// Live Wire (the Stormbound Conduit): melee attackers become Conductive.
// The Hero's demon: a hit grants Fel Command, once every 4 sec (2 with Infernal Pact); with Metamorphic Bond, in
// Metamorphosis, it strikes 2 more enemies within 5 yards for half. Entering Metamorphosis with Demonic Ascension:
// 5 Fel Command.
class titan_runes_unit : public UnitScript
{
public:
    titan_runes_unit() : UnitScript("titan_runes_unit", true, { UNITHOOK_ON_DAMAGE, UNITHOOK_ON_AURA_APPLY,
        UNITHOOK_MODIFY_HEAL_RECEIVED }) { }

    // Peaceful Focus (the Wandering Mist): Soothing Mist 10% stronger on a target wearing the Hero's Enveloping Mist.
    void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* spellInfo) override
    {
        if (target && healer && spellInfo && spellInfo->Id == SPELL_SOOTHING_MIST && healer->IsPlayer()
            && HasMistMarker(healer, MOD_PEACEFUL_FOCUS) && healer->HasAura(SPELL_RUNE_WANDERING_MIST)
            && target->HasAura(SPELL_ENVELOPING_MIST, healer->GetGUID()))
            heal = uint32(heal * (1.0f + PEACEFUL_PCT / 100.0f));
    }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        // Live Wire: an enemy that strikes the Hero in melee range becomes Conductive (whatever their shield does).
        if (attacker && victim && damage && attacker != victim && victim->IsPlayer())
        {
            Player* hero = victim->ToPlayer();
            if (HasStormMarker(hero, MOD_LIVE_WIRE) && hero->HasAura(SPELL_RUNE_STORMBOUND_CONDUIT)
                && attacker->IsAlive() && !IsConductive(hero, attacker) && hero->IsWithinMeleeRange(attacker)
                && hero->IsValidAttackTarget(attacker))
                MakeConductive(hero, attacker);
        }

        static bool cleaving = false;
        if (!attacker || !victim || !damage || cleaving || !attacker->IsPet())
            return;
        Player* owner = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (!owner || owner == attacker || !owner->HasAura(SPELL_RUNE_FEL_COMMANDMENT) || Demon(owner) != attacker)
            return;

        State& state = StateOf(owner);
        uint32 now = GameTime::GetGameTimeMS().count();
        uint32 interval = HasMarker(owner, MOD_INFERNAL_PACT) ? INFERNAL_PACT_INTERVAL_MS : DEMON_STACK_INTERVAL_MS;
        if (now - state.LastDemonStack >= interval)
        {
            state.LastDemonStack = now;
            Gain(owner, 1);
        }

        if (HasMarker(owner, MOD_METAMORPHIC_BOND) && InMetamorphosis(owner))
        {
            std::vector<Unit*> enemies = EnemiesAround(owner, victim, BOND_CLEAVE_RANGE);
            enemies.erase(std::remove(enemies.begin(), enemies.end(), victim), enemies.end());
            if (enemies.size() > BOND_CLEAVE_TARGETS)
                enemies.resize(BOND_CLEAVE_TARGETS);
            int32 share = std::max<int32>(1, int32(damage / 2));
            cleaving = true;
            for (Unit* enemy : enemies)
                attacker->CastCustomSpell(enemy, SPELL_METAMORPHIC_CLEAVE, &share, nullptr, nullptr, true);
            cleaving = false;
        }
    }

    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        uint32 id = aura->GetId();
        if (id != SPELL_METAMORPHOSIS_DH && id != SPELL_METAMORPHOSIS_WARLOCK)
            return;
        if (Player* player = unit->ToPlayer())
            if (HasMarker(player, MOD_DEMONIC_ASCENSION))
                Gain(player, MAX_STACKS);
    }
};

void AddSC_runes()
{
    RegisterSpellScript(spell_titan_rune_fel_commandment);
    RegisterSpellScript(spell_titan_rune_throw_glaive);
    RegisterSpellScript(spell_titan_rune_consecration);
    RegisterSpellScript(spell_titan_rune_sanctified_paragon);
    RegisterSpellScript(spell_titan_rune_stormbound_conduit);
    RegisterSpellScript(spell_titan_rune_lightning_shield);
    RegisterSpellScript(spell_titan_rune_lightning_shield_damage);
    RegisterSpellScript(spell_titan_rune_thunder_clap);
    RegisterSpellScript(spell_titan_rune_shield_slam);
    RegisterSpellScript(spell_titan_rune_chain_heal);
    RegisterSpellScript(spell_titan_rune_earth_shield_heal);
    RegisterSpellScript(spell_titan_rune_soothing_mist_cast);
    RegisterSpellScript(spell_titan_rune_soothing_mist);
    RegisterSpellScript(spell_titan_rune_mist_technique);
    RegisterSpellScript(spell_titan_rune_requiem);
    RegisterSpellScript(spell_titan_rune_scar_strike);
    RegisterSpellScript(spell_titan_rune_wake_cut);
    RegisterSpellScript(spell_titan_rune_unbroken_onslaught);
    RegisterSpellScript(spell_titan_rune_dawn_vow);
    RegisterSpellScript(spell_titan_rune_judgement);
    RegisterSpellScript(spell_titan_rune_dawn_echo);
    RegisterSpellScript(spell_titan_rune_mind_flay);
    RegisterSpellScript(spell_titan_rune_mind_blast);
    RegisterSpellScript(spell_titan_rune_shattered_sun);
    RegisterCreatureAI(npc_titan_rune_hollow_echo);
    RegisterCreatureAI(npc_titan_rune_felguard);
    RegisterCreatureAI(npc_titan_rune_fel_imp);
    new titan_runes_player();
    new titan_runes_unit();
}
