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
#include <map>
#include <tuple>
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
// - One rune per specialization (titan_runes, db-characters), chosen once: a prestige clears it, or .unbindrune
//   (titan_commands.cpp) lets go of the active one to bind another. Modifiers
//   change freely out of combat.
// - The rune works through auras: its own while it is the active spec's (90700 Fel Commandment, 90730 the
//   Sanctified Paragon, 90750 the Stormbound Conduit, 90770 Echoing Thunder, 90790 the Wellspring, 90810 the
//   Wandering Mist, 90830 / 90860 / 90890 / 90920 / 90950 the five after), and one marker per picked modifier
//   (90710 / 90740 / 90760 / 90780 / 90800 / 90820, and 20 on from each of the five's first, + slot * 3 + choice);
//   scripts look for them.
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
        SPELL_STATIC_WARD               = 90751,    // Static Ward's absorb
        SPELL_STORMBOUND_ARC            = 90752,    // a secondary arc's damage
        SPELL_STORMBOUND_ARC_VISUAL     = 90753,    // its bolt, cast by the enemy it leaves
        SPELL_STORMBOUND_RETALIATION    = 90754,    // a Lightning Shield retaliation the rune makes
        SPELL_STORM_MODIFIER_FIRST      = 90760,    // + slot * 3 + choice

        SPELL_RUNE_ECHOING_THUNDER      = 90770,
        SPELL_THUNDERFIELD_MARCH        = 90772,    // the charge: a small storm about the Hero
        SPELL_THUNDER_ECHO              = 90774,
        SPELL_THUNDER_ECHO_BURST        = 90775,    // a burst about the Shield Slam target, its look
        SPELL_SHELTERING_REVERBERATION  = 90776,
        SPELL_DEAFENING_IMPACT          = 90777,
        SPELL_THUNDER_MODIFIER_FIRST    = 90780,    // + slot * 3 + choice

        SPELL_RUNE_WELLSPRING           = 90790,
        SPELL_WELLSPRING_HEAL           = 90792,    // the additional jump
        SPELL_TIDAL_SHELTER             = 90793,
        SPELL_RENEWING_CURRENT          = 90794,
        SPELL_WELLSPRING_JUMP           = 90795,    // the jump's bolt, its look
        SPELL_WELLSPRING_MODIFIER_FIRST = 90800,    // + slot * 3 + choice
        SPELL_RUNE_WANDERING_MIST       = 90810,
        SPELL_WANDERING_MIST            = 90811,
        SPELL_LESSER_ENVELOPING_MIST    = 90812,
        SPELL_LINGERING_MIST            = 90813,
        SPELL_JADE_SHELTER              = 90814,
        SPELL_MIST_MODIFIER_FIRST       = 90820,    // + slot * 3 + choice

        SPELL_RUNE_THREEFOLD_GRAVE      = 90830,
        SPELL_SHARED_BURIAL             = 90831,    // on an enemy in two Graves or more
        SPELL_GRAVE_PESTILENCE_TICK     = 90832,    // the damage over time's ticks (spell_titan_rune_dot)
        SPELL_WAKE_BLEED_TICK           = 90833,
        SPELL_HOLLOW_FLAY_TICK          = 90834,
        SPELL_FROZEN_GRAVE              = 90838,    // the Graves' damage
        SPELL_GRAVE_CHILL               = 90839,
        SPELL_PLAGUED_GRAVE             = 90840,
        SPELL_GRAVE_PESTILENCE          = 90841,
        SPELL_CRIMSON_GRAVE             = 90842,
        SPELL_CRIMSON_WARD              = 90843,
        SPELL_GRAVE_WARD                = 90844,    // Grave Ward's heal
        SPELL_GRAVE_GROUND_FIRST        = 90845,    // the Graves on the ground: + Grave
        SPELL_CRIMSON_MARCH             = 90848,    // the first draft's March of the Dead (on the Hero): removed
        SPELL_GRAVE_MODIFIER_FIRST      = 90850,    // + slot * 3 + choice

        SPELL_RUNE_UNBROKEN_ONSLAUGHT   = 90860,
        SPELL_WEAPON_WAKE               = 90861,    // the wake's attack
        SPELL_WAKE_BLEED                = 90862,
        SPELL_WAKE_GROUND               = 90863,    // the wake on the ground
        SPELL_WAKE_AFTERSHOCK           = 90864,
        SPELL_SECOND_WIND               = 90865,
        SPELL_ONSLAUGHT_MODIFIER_FIRST  = 90880,    // + slot * 3 + choice

        SPELL_RUNE_DAWNBOUND_CRUSADER   = 90890,
        SPELL_DAWN_VOW                  = 90891,    // the first draft's action and vow: unlearned, removed
        SPELL_DAWN_VOWED                = 90892,
        SPELL_DAWN_ECHO_HEAL            = 90894,
        SPELL_DAWN_ECHO_DAMAGE          = 90895,
        SPELL_DAWN_BOLT                 = 90896,    // its look: Holy Shock's, harmful
        SPELL_DAWN_BOLT_HEAL            = 90897,    // and helpful
        SPELL_SUNBURST                  = 90898,    // Sunburst's look
        SPELL_SACRED_GROUND             = 90899,
        SPELL_RADIANT_WARD              = 90900,
        SPELL_DAWN_MODIFIER_FIRST       = 90910,    // + slot * 3 + choice

        SPELL_RUNE_HOLLOW_CONGREGATION  = 90920,
        SPELL_HOLLOW_ECHO               = 90921,    // on the echo: its look, no threat
        SPELL_HOLLOW_FLAY               = 90922,
        SPELL_HOLLOW_BOLT               = 90923,
        SPELL_HOLLOW_SCREAM             = 90924,
        SPELL_HOLLOW_LAST_WORD          = 90925,
        SPELL_HOLLOW_MEND               = 90926,    // Shadow Mend's heal
        SPELL_HOLLOW_MODIFIER_FIRST     = 90940,    // + slot * 3 + choice

        SPELL_RUNE_SHATTERED_SUN        = 90950,
        SPELL_SUNSHARD                  = 90951,
        SPELL_FROZEN_CORE               = 90952,    // Frozen Core's slow
        SPELL_FRACTURE                  = 90953,
        SPELL_FRACTURE_LOOK             = 90954,    // a ring of ice about where it breaks
        SPELL_COLLAPSE                  = 90955,
        SPELL_COLLAPSE_LOOK             = 90956,
        SPELL_SCALDING_GROUND           = 90957,
        SPELL_SUN_MODIFIER_FIRST        = 90970,    // + slot * 3 + choice

        SPELL_ICY_TOUCH             = 45477,        // first ranks
        SPELL_PLAGUE_STRIKE         = 45462,
        SPELL_BLOOD_STRIKE          = 45902,
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
        SPELL_PYROBLAST             = 11366,
        SPELL_FIRE_BLAST            = 2136,
        SPELL_SCORCH                = 2948,
        SPELL_ICE_LANCE             = 30455,
        SPELL_FROST_NOVA            = 122,
        SPELL_CONE_OF_COLD          = 120,

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

    // The Stormbound Conduit's modifiers: slot (0 Arcs, 1 Lightning Shield, 2 Surge) x 3 + choice.
    enum StormModifier : uint8
    {
        MOD_FORKED_TEMPEST, MOD_CLOSED_CIRCUIT, MOD_FARADAY_FIELD,
        MOD_LIVE_WIRE, MOD_STORM_WITHIN, MOD_DISTANT_CONDUCTOR,
        MOD_CHAIN_SURGE, MOD_STATIC_WARD, MOD_OVERCHARGE,
    };

    // The Stormbound Conduit's tuning: Lightning Bolt arcs to 2 enemies within 10 yards of its target for 30% of its
    // damage each; a Lightning Shield retaliation to 1 for 50%. An arc never arcs again.
    constexpr float ARC_RANGE = 10.0f, FARADAY_RANGE = 15.0f, FARADAY_ARCS = 1.1f;
    constexpr float BOLT_ARC_SHARE = 0.3f, SHIELD_ARC_SHARE = 0.5f, CHAIN_ARC_SHARE = 0.2f;
    constexpr std::size_t BOLT_ARCS = 2, FORKED_ARCS = 3, SHIELD_ARCS = 1, LIVE_WIRE_ARCS = 2;
    constexpr float CLOSED_CIRCUIT_SHARE = 0.6f, OVERCHARGE = 1.5f;
    constexpr uint32 STORM_WITHIN_MS = 3000;
    constexpr float STORM_WITHIN_RANGE = 8.0f;
    constexpr float STATIC_WARD_PCT = 1.0f, STATIC_WARD_CAP_PCT = 10.0f;     // of maximum health
    constexpr int32 STATIC_WARD_MS = 8000;
    constexpr uint32 LIGHTNING_SHIELD_FAMILY_FLAG = 0x400;      // SpellFamilyFlags0

    // Echoing Thunder's modifiers: slot (0 Charge, 1 Echo, 2 Resonance) x 3 + choice.
    enum ThunderModifier : uint8
    {
        MOD_DOUBLE_CHARGE, MOD_LINGERING_CHARGE, MOD_COMMANDING_THUNDER,
        MOD_FOCUSED_PEAL, MOD_DISTANT_RUMBLE, MOD_DEAFENING_IMPACT,
        MOD_SHELTERING_REVERBERATION, MOD_BATTLE_RHYTHM, MOD_ROLLING_THUNDER,
    };

    // Echoing Thunder's tuning: Thunder Clap charges the Hero for 8 sec (20 with Lingering Charge); the next Shield
    // Slam releases the echo: 30% of its damage before mitigation to each enemy within 6 yards of its target (up to
    // 5).
    constexpr int32 CHARGE_MS = 8000, LINGERING_CHARGE_MS = 20000;
    constexpr uint8 DOUBLE_CHARGES = 2;
    constexpr float ECHO_SHARE = 0.3f, ECHO_RADIUS = 6.0f, DISTANT_RADIUS = 10.0f, FOCUSED_ECHO = 1.6f;
    constexpr std::size_t ECHO_TARGETS = 5;
    constexpr float COMMANDING_THREAT = 1.0f;                           // twice the threat
    constexpr float SHELTERING_PCT = 3.0f, SHELTERING_CAP_PCT = 15.0f;  // of maximum health, an enemy hit
    constexpr int32 SHELTERING_MS = 6000, DEAFENING_MS = 3000;
    constexpr int32 RHYTHM_RAGE = 20;                                   // 2 rage an enemy hit
    constexpr int32 ROLLING_MS = 1000;                                  // off Thunder Clap's cooldown, an enemy hit

    // The Wellspring's modifiers: slot (0 Spring, 1 Earth, 2 Undertow) x 3 + choice.
    enum WellspringModifier : uint8
    {
        MOD_SPLIT_TRIBUTARY, MOD_DEEP_CURRENT, MOD_CONCENTRATED_CURRENT,
        MOD_FLOWING_WATERS, MOD_BEDROCK, MOD_RENEWED_SHIELD,
        MOD_TIDAL_SHELTER, MOD_RENEWING_CURRENT, MOD_LONG_REACH,
    };

    // The Wellspring's tuning: Chain Heal jumps once more from its last target, 40% of its first heal, to the most
    // injured ally within 12.5 yards; it heals the Hero's Earth Shield wearer 20% more.
    constexpr float WELLSPRING_SHARE = 0.4f, SPLIT_SHARE = 0.25f;
    constexpr float CHAIN_HEAL_JUMP = 12.5f, LONG_REACH = 20.0f;
    constexpr float DEEP_CURRENT = 1.4f, DEEP_CURRENT_BELOW_PCT = 50.0f, CONCENTRATED_CURRENT = 1.2f;
    constexpr float WEARER_BONUS = 1.2f, BEDROCK_BONUS = 1.4f, FLOWING_SHARE = 0.5f;
    constexpr std::size_t SPLIT_ALLIES = 2;
    constexpr float UNDERTOW_SHARE = 0.2f;                      // of the jump's effective healing
    constexpr int32 UNDERTOW_TICKS = 4;
    constexpr uint32 JUMP_DELAY_MS = 100;                       // after Chain Heal's first hit: its chain done

    // The Wandering Mist's modifiers: slot (0 Mistflow, 1 Technique, 2 Approach) x 3 + choice.
    enum MistModifier : uint8
    {
        MOD_FLOWING_CURRENTS, MOD_DRIFTING_MIST, MOD_JADE_SHELTER,
        MOD_COMFORTING_VEIL, MOD_VIVACIOUS_FLOW, MOD_PEACEFUL_FOCUS,
        MOD_EFFORTLESS_BREATHING, MOD_LINGERING_MIST, MOD_DEEP_BREATH,
    };

    // The Wandering Mist's tuning: each Soothing Mist tick also heals the most injured ally within 20 yards of its
    // target for 25% of it.
    constexpr float WANDER_SHARE = 0.25f, DRIFTING_SHARE = 0.15f, WANDER_RANGE = 20.0f, FLOWING_RANGE = 30.0f;
    constexpr float JADE_SHELTER_SHARE = 0.1f, VEIL_SHARE = 0.3f, VIVACIOUS_SHARE = 0.3f, SECONDARY_RANGE = 20.0f;
    constexpr float PEACEFUL_PCT = 15.0f, EFFORTLESS_SHARE = 0.25f, DEEP_BREATH_PCT = 0.3f;
    constexpr int32 SOOTHING_MIST_TICKS = 8, LINGERING_TICKS = 2;

    // The Threefold Grave's modifiers: slot (0 Burial Grounds, 1 Gravekeeper, 2 Shared Burial) x 3 + choice.
    enum GraveModifier : uint8
    {
        MOD_FAR_REACHING_GRAVES, MOD_LONG_REST, MOD_MARCH_OF_THE_DEAD,
        MOD_CRUEL_WINTER, MOD_GRAVE_PESTILENCE, MOD_CRIMSON_BASTION,
        MOD_DEEP_BURIAL, MOD_GRAVE_HUNGER, MOD_GRAVE_WARD,
    };

    // The Graves: Icy Touch's Frozen Grave, Plague Strike's Plagued Grave and Blood Strike's Crimson Grave, each
    // beneath its target (a Frost, an Unholy and a Blood rune's strike).
    enum Grave : uint8 { GRAVE_FROZEN, GRAVE_PLAGUED, GRAVE_CRIMSON, GRAVES };

    // The Threefold Grave's tuning. A Grave's tick: the Hero's level x 2 and 6% of their attack power, with their
    // damage bonuses for its school; each Grave its own tick on every enemy in it (up to 10), 20% more on one in two
    // or three (Shared Burial; 35% with Deep Burial).
    constexpr uint32 GRAVE_MS = 8000, LONG_REST_MS = 12000, GRAVE_TICK_MS = 1000;
    constexpr float GRAVE_RADIUS = 5.0f, FAR_REACHING = 1.5f;
    constexpr float GRAVE_LEVEL = 2.0f, GRAVE_AP = 0.06f;
    constexpr float FROZEN_SHARE = 0.8f, PLAGUED_SHARE = 1.0f, CRIMSON_SHARE = 0.9f;
    constexpr int32 GRAVE_SLOW = -30, CRUEL_WINTER_SLOW = -50;
    constexpr int32 CHILL_MS = 1200, CRUEL_WINTER_MS = 3000;    // the slow outlasts a tick; Cruel Winter's lingers
    constexpr float PESTILENCE_SHARE = 0.4f;                    // Grave Pestilence's disease tick, of a Grave tick
    constexpr int32 PESTILENCE_MS = 4000;
    constexpr float SHARED_BURIAL = 1.2f, DEEP_BURIAL = 1.35f, GRAVE_HUNGER = 1.25f, HUNGER_BELOW_PCT = 35.0f;
    constexpr float CRIMSON_WARD_PCT = 5.0f, CRIMSON_WARD_CAP_PCT = 15.0f, BASTION_WARD_CAP_PCT = 25.0f;
    constexpr int32 CRIMSON_WARD_MS = 8000;
    constexpr float GRAVE_WARD_PCT = 1.0f;                      // of maximum health, an enemy in a Grave, a tick
    constexpr std::size_t GRAVE_TARGETS = 10, GRAVE_WARD_ENEMIES = 3;

    // Unbroken Onslaught's modifiers: slot (0 Wake, 1 Follow-Through, 2 Pursuit) x 3 + choice.
    enum OnslaughtModifier : uint8
    {
        MOD_EXECUTIONERS_CUT, MOD_WHIRLING_STEEL, MOD_RAKING_PASSAGE,
        MOD_OVERPOWERING_ECHO, MOD_BLOODLETTING, MOD_AFTERSHOCK,
        MOD_RELENTLESS, MOD_BATTLEFIELD_CLAIM, MOD_SECOND_WIND,
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
    constexpr float SECOND_WIND_PCT = 1.0f;                     // of maximum health, an enemy the wake hits

    // The Dawnbound Crusader's modifiers: slot (0 Echo, 1 Light, 2 Grace) x 3 + choice.
    enum DawnModifier : uint8
    {
        MOD_SUNBURST, MOD_FOCUSED_DAWN, MOD_COMMUNION,
        MOD_SACRED_GROUND, MOD_STEADFAST, MOD_JUDGMENTS_ECHO,
        MOD_DAWNS_MERCY, MOD_SWIFT_DAWN, MOD_RADIANT_WARD,
    };

    // The Dawnbound Crusader's tuning: an echo is half of what set it off, 0.75 sec later.
    constexpr float DAWN_SHARE = 0.5f, FOCUSED_DAWN = 1.4f, COMMUNION_SHARE = 0.7f;   // Communion: 35% each way
    constexpr uint32 DAWN_DELAY_MS = 750;
    constexpr float DAWN_REACH = 15.0f, SUNBURST_RADIUS = 8.0f, SUNBURST_SHARE = 0.4f;
    constexpr float SACRED_RADIUS = 8.0f, SACRED_SHARE = 0.25f;     // Consecration's ground
    constexpr uint32 SACRED_MS = 4000;
    constexpr float STEADFAST_SHARE = 0.5f, MERCY_SHARE = 0.2f, RADIANT_SHARE = 0.25f, JUDGMENT_LEVEL = 5.0f;
    constexpr int32 SWIFT_DAWN_MS = 500, RADIANT_MS = 8000;
    constexpr std::size_t DAWN_TARGETS = 5;

    // The Hollow Congregation's modifiers: slot (0 Congregation, 1 Sermon, 2 Command) x 3 + choice.
    enum HollowModifier : uint8
    {
        MOD_GATHERING, MOD_LASTING_ECHOES, MOD_LAST_WORD,
        MOD_UNISON, MOD_DISSONANCE, MOD_ACCUSATION,
        MOD_FERVOR, MOD_SHADOW_MEND, MOD_SWIFT_COMMAND,
    };

    // The Hollow Congregation's tuning: an echo's channel ticks for 40% of the Hero's Mind Flay tick; a barrage is 3
    // bolts each, 0.4 sec apart, 60% of a tick each.
    constexpr uint32 HOLLOW_ECHO_MS = 10000, LASTING_ECHO_MS = 15000, BARRAGE_SPACING_MS = 400;
    constexpr std::size_t MAX_HOLLOW_ECHOES = 3, GATHERING_ECHOES = 4;
    constexpr uint32 BARRAGE_BOLTS = 3;
    constexpr float HOLLOW_SHARE = 0.4f, BARRAGE_SHARE = 0.6f, UNISON_BARRAGE = 1.2f, SCREAM_SHARE = 1.5f;
    constexpr float HOLLOW_RANGE = 40.0f, SCREAM_WIDTH = 2.0f;
    constexpr float LAST_WORD_TICKS = 3.0f, FERVOR = 1.25f, MEND_SHARE = 0.1f, FLAY_CHANNEL_TICKS = 3.0f;
    constexpr int32 SWIFT_COMMAND_MS = 500;
    constexpr std::size_t SCREAM_TARGETS = 5;

    // The Shattered Sun's modifiers: slot (0 Formation, 1 Fracture, 2 Collapse) x 3 + choice.
    enum SunModifier : uint8
    {
        MOD_WILDFIRE, MOD_SPREADING_SUN, MOD_HEAVY_SUN,
        MOD_WIDE_BURST, MOD_SCALDING_GROUND, MOD_CHAIN_REACTION,
        MOD_IMPLOSION, MOD_SECOND_SUNRISE, MOD_FROZEN_CORE,
    };

    // The Shattered Sun's tuning: a fire spell embeds a Sunshard in its target, stacking to 5 (8 with Heavy Sun); a
    // frost spell on it shatters them all, 25% of its damage per shard (the Hero's floor at least: level x 4 and 40%
    // of their spell power) to each enemy within 8 yards.
    constexpr uint8 MAX_SHARDS = 5, HEAVY_SHARDS = 8;
    constexpr std::size_t SPREAD = 2, FRACTURE_TARGETS = 10;
    constexpr float SPREAD_RANGE = 8.0f, SUN_LEVEL = 4.0f, SUN_POWER = 0.4f, SHARD_SHARE = 0.25f;
    constexpr int32 SUNSHARD_MS = 15000, FROZEN_CORE_MS = 3000;
    constexpr float FRACTURE_RADIUS = 8.0f, WIDE_RADIUS = 12.0f, CHAIN_RADIUS = 10.0f, CHAIN_SHARE = 0.5f;
    constexpr float IMPLOSION = 1.4f, SCALDING_RADIUS = 6.0f, SCALDING_SHARE = 0.15f, SUNRISE_SHARE = 0.4f;
    constexpr uint32 SCALDING_MS = 4000, SUNRISE_DELAY_MS = 2000;

    // The Sanctified Paragon's tuning (the revised balance draft's initial values).
    constexpr float PARAGON_SHARE = 0.4f;                       // of a Consecration tick, a second
    constexpr float PARAGON_POWER = 0.15f;                      // spell power in a tick: 6% of it a second
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
              { { SPELL_LIGHTNING_BOLT } }, SPELL_RUNE_STORMBOUND_CONDUIT,
              SPELL_STORM_MODIFIER_FIRST },
            // Tanks: Thunder Clap places the storm, Shield Slam echoes it (and needs the shield the rune asks for).
            { RUNE_ECHOING_THUNDER, uint8(Wildcard::RoleMask(Wildcard::ROLE_TANK)),
              { { SPELL_THUNDER_CLAP }, { SPELL_SHIELD_SLAM } }, SPELL_RUNE_ECHOING_THUNDER,
              SPELL_THUNDER_MODIFIER_FIRST },
            // Healers: Chain Heal through the Earth Shield wearer; Soothing Mist's channel.
            { RUNE_WELLSPRING, uint8(Wildcard::RoleMask(Wildcard::ROLE_HEALER)),
              { { SPELL_CHAIN_HEAL } }, SPELL_RUNE_WELLSPRING,
              SPELL_WELLSPRING_MODIFIER_FIRST },
            { RUNE_WANDERING_MIST, uint8(Wildcard::RoleMask(Wildcard::ROLE_HEALER)),
              { { SPELL_SOOTHING_MIST } }, SPELL_RUNE_WANDERING_MIST, SPELL_MIST_MODIFIER_FIRST },
            // DPS or Tank: Blood Strike's blood circle and one of the other two scars.
            { RUNE_THREEFOLD_GRAVE,
              uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE) | Wildcard::RoleMask(Wildcard::ROLE_TANK)),
              { { SPELL_BLOOD_STRIKE, SPELL_ICY_TOUCH, SPELL_PLAGUE_STRIKE } }, SPELL_RUNE_THREEFOLD_GRAVE,
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
            // Ranged DPS: a fire spell embeds and collapses, a frost spell fractures.
            { RUNE_SHATTERED_SUN, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_FIREBALL, SPELL_PYROBLAST, SPELL_FIRE_BLAST, SPELL_SCORCH },
                { SPELL_FROSTBOLT, SPELL_ICE_LANCE, SPELL_FROST_NOVA, SPELL_CONE_OF_COLD } }, SPELL_RUNE_SHATTERED_SUN,
              SPELL_SUN_MODIFIER_FIRST },
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
        if (rune == RUNE_STORMBOUND_CONDUIT && modifier == MOD_CHAIN_SURGE)
            return { { SPELL_CHAIN_LIGHTNING } };
        if (rune == RUNE_STORMBOUND_CONDUIT
            && (modifier == MOD_LIVE_WIRE || modifier == MOD_STORM_WITHIN || modifier == MOD_DISTANT_CONDUCTOR))
            return { { SPELL_LIGHTNING_SHIELD } };
        if (rune == RUNE_WELLSPRING
            && (modifier == MOD_FLOWING_WATERS || modifier == MOD_BEDROCK || modifier == MOD_RENEWED_SHIELD))
            return { { SPELL_EARTH_SHIELD } };
        if (rune == RUNE_WANDERING_MIST && (modifier == MOD_COMFORTING_VEIL || modifier == MOD_PEACEFUL_FOCUS))
            return { { SPELL_ENVELOPING_MIST } };
        if (rune == RUNE_WANDERING_MIST && modifier == MOD_VIVACIOUS_FLOW)
            return { { SPELL_VIVIFY } };
        if (rune == RUNE_DAWNBOUND_CRUSADER && modifier == MOD_STEADFAST)
            return { { SPELL_HOLY_SHOCK } };
        if (rune == RUNE_DAWNBOUND_CRUSADER && modifier == MOD_JUDGMENTS_ECHO)
            return { { SPELL_JUDGEMENT_OF_LIGHT, SPELL_JUDGEMENT_OF_WISDOM, SPELL_JUDGEMENT_OF_JUSTICE } };
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

        // Echoing Thunder.
        uint8 ThunderCharges = 0;                   // echoes left in the charge


        // The Wandering Mist.
        int32 MistTickAmount = 0;                   // Soothing Mist's last tick (Lingering Mist)
        ObjectGuid LesserTarget;                    // the lesser Enveloping Mist's (Comforting Veil)

        // The Threefold Grave: each Grave a circle at From (Until 0: none).
        struct GraveAt
        {
            uint32 Map = 0;
            Position From;
            ObjectGuid Host;                        // the enemy it was laid beneath (March of the Dead)
            uint32 Until = 0;                       // game ms
        };
        std::array<GraveAt, GRAVES> Graves;
        uint32 GraveTimer = 0;

        // Unbroken Onslaught: the wake, a line From..To or a crescent at From facing Facing.
        bool Wake = false, WakeCrescent = false;
        uint32 WakeMap = 0;
        Position WakeFrom, WakeTo;
        float WakeFacing = 0.0f, WakeHalfWidth = 0.0f;
        uint32 WakeUntil = 0, WakeCreator = 0, WakeTimer = 0;
        ObjectGuid WakePrimary;
        Position WakeOffset;                        // Relentless: from its target, as cut

        // The Dawnbound Crusader.
        Position SacredAt;                          // Sacred Ground's patch (SacredUntil 0: none)
        uint32 SacredMap = 0, SacredUntil = 0, SacredTimer = 0;
        float SacredTick = 0.0f;
        bool SacredHeal = false;

        // The Hollow Congregation.
        std::vector<ObjectGuid> HollowEchoes;       // oldest first

        // The Shattered Sun: the enemies holding shards.
        std::vector<ObjectGuid> Shards;
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
    // the higher of spell power and half the attack power x Chaos Bolt's coefficient x 0.6, with the Hero's Fire
    // damage bonuses (the bolt ignores their modifiers).
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
        float damage = (CHAOS_BOLT_RANK1_AVERAGE * scale + power * CHAOS_BOLT_COEFFICIENT) * BOLT_SHARE
            * player->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, SPELL_SCHOOL_MASK_FIRE);
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
    // with Consecration's share of spell power and attack power (spell_bonus_data, 4% each) and every talent, glyph
    // and bonus that raises Consecration or Holy damage; and the aura's own power on top, PARAGON_POWER of the
    // higher of the Hero's Holy spell power and half their attack power, with their Holy damage bonuses. The damage,
    // heal and shield spells add nothing of their own (spell_bonus_data 0, caster modifiers ignored): this is all.
    float ParagonTick(Player* player)
    {
        SpellInfo const* rank = sSpellMgr->GetSpellInfo(ConsecrationRank(player));
        if (!rank)
            return 1.0f;
        int32 points = std::max(1, rank->Effects[EFFECT_0].CalcValue(player));
        float tick = float(player->SpellDamageBonusDone(player, rank, uint32(points), DOT, EFFECT_0));
        float power = std::max(float(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_HOLY)),
            player->GetTotalAttackPowerValue(BASE_ATTACK) / 2.0f);
        return tick + power * PARAGON_POWER
            * player->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, SPELL_SCHOOL_MASK_HOLY);
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

    // The player's Lightning Shield (any rank): its proc effect.
    AuraEffect* Shield(Player* player)
    {
        return player->GetAuraEffect(SPELL_AURA_PROC_TRIGGER_SPELL, SPELLFAMILY_SHAMAN, LIGHTNING_SHIELD_FAMILY_FLAG, 0,
            0, player->GetGUID());
    }

    // Static Ward: each arc adds 1% of maximum health to one absorb, up to 10%, renewed to 8 sec.
    void StaticWard(Player* player)
    {
        int32 step = std::max<int32>(1, int32(player->GetMaxHealth() * STATIC_WARD_PCT / 100.0f));
        int32 cap = std::max<int32>(1, int32(player->GetMaxHealth() * STATIC_WARD_CAP_PCT / 100.0f));
        if (AuraEffect* ward = player->GetAuraEffect(SPELL_STATIC_WARD, EFFECT_0, player->GetGUID()))
        {
            ward->ChangeAmount(std::min(cap, ward->GetAmount() + step));
            ward->GetBase()->SetDuration(STATIC_WARD_MS);
            return;
        }
        player->CastCustomSpell(player, SPELL_STATIC_WARD, &step, nullptr, nullptr, true);
        if (Aura* aura = player->GetAura(SPELL_STATIC_WARD, player->GetGUID()))
        {
            aura->SetMaxDuration(STATIC_WARD_MS);
            aura->SetDuration(STATIC_WARD_MS);
        }
    }

    // Arcs leaving source for share of damage each: to count enemies near it (10 yards, 15 with Faraday Field, 10%
    // more), or (Closed Circuit, a Lightning Bolt's) back into source once for 60%. An arc never arcs again (its
    // damage is a spell of its own).
    void Arc(Player* player, Unit* source, float damage, float share, std::size_t count, bool bolt)
    {
        bool faraday = HasStormMarker(player, MOD_FARADAY_FIELD);
        std::vector<Unit*> targets;
        if (bolt && HasStormMarker(player, MOD_CLOSED_CIRCUIT))
        {
            if (source->IsAlive())
                targets.push_back(source);
            share = CLOSED_CIRCUIT_SHARE;
        }
        else
            for (Unit* enemy : EnemiesAround(player, source, faraday ? FARADAY_RANGE : ARC_RANGE))
                if (enemy != source && targets.size() < count)
                    targets.push_back(enemy);
        int32 each = int32(damage * share * (faraday ? FARADAY_ARCS : 1.0f));
        if (targets.empty() || each < 1)
            return;
        for (Unit* target : targets)
        {
            if (target != source)
                source->CastSpell(target, SPELL_STORMBOUND_ARC_VISUAL, true);
            player->CastCustomSpell(target, SPELL_STORMBOUND_ARC, &each, nullptr, nullptr, true);
            if (HasStormMarker(player, MOD_STATIC_WARD))
                StaticWard(player);
        }
    }

    // A Lightning Shield retaliation the rune makes (Distant Conductor, Storm Within): the shield's rank's damage at
    // the target; no charge used.
    void Retaliate(Player* player, Unit* target, AuraEffect* shield)
    {
        uint32 damageSpell = sSpellMgr->GetSpellWithRank(SPELL_LIGHTNING_SHIELD_DAMAGE,
            shield->GetSpellInfo()->GetRank());
        SpellInfo const* info = sSpellMgr->GetSpellInfo(damageSpell);
        if (!info)
            return;
        int32 damage = std::max<int32>(1, info->Effects[EFFECT_0].CalcValue(player));
        player->CastCustomSpell(target, SPELL_STORMBOUND_RETALIATION, &damage, nullptr, nullptr, true);
    }

    // Storm Within: every 3 sec in combat, a retaliation at the nearest enemy within 8 yards.
    void StormPulse(Player* player, uint32 diff)
    {
        if (!HasStormMarker(player, MOD_STORM_WITHIN))
            return;
        State& state = StateOf(player);
        state.StormTimer += diff;
        if (state.StormTimer < STORM_WITHIN_MS)
            return;
        AuraEffect* shield = Shield(player);
        if (!shield || !player->IsInCombat())
            return;
        std::vector<Unit*> enemies = EnemiesAround(player, player, STORM_WITHIN_RANGE);
        if (enemies.empty())
            return;
        state.StormTimer = 0;
        Retaliate(player, enemies.front(), shield);
    }

    // ---- Echoing Thunder ---------------------------------------------------------------------------------------

    bool HasThunderMarker(Unit* unit, uint8 modifier)
    {
        return unit && unit->HasAura(SPELL_THUNDER_MODIFIER_FIRST + modifier);
    }

    // Thunder Clap: the Hero is charged (the small storm about them), one echo (two with Double Charge).
    void ChargeThunder(Player* player)
    {
        int32 ms = HasThunderMarker(player, MOD_LINGERING_CHARGE) ? LINGERING_CHARGE_MS : CHARGE_MS;
        StateOf(player).ThunderCharges = HasThunderMarker(player, MOD_DOUBLE_CHARGE) ? DOUBLE_CHARGES : 1;
        player->RemoveAurasDueToSpell(SPELL_THUNDERFIELD_MARCH);
        if (Aura* aura = player->AddAura(SPELL_THUNDERFIELD_MARCH, player))
        {
            aura->SetMaxDuration(ms);
            aura->SetDuration(ms);
        }
    }

    // Shield Slam while charged: the echo bursts about its target, 30% of the slam's damage before mitigation to
    // each enemy within 6 yards (10 with Distant Rumble; the target alone, 60% more, with Focused Peal), up to 5.
    void ThunderEcho(Player* player, Unit* target, int32 slamDamage)
    {
        State& state = StateOf(player);
        if (!player->HasAura(SPELL_THUNDERFIELD_MARCH) || !state.ThunderCharges || slamDamage <= 0)
            return;
        if (!--state.ThunderCharges)
            player->RemoveAurasDueToSpell(SPELL_THUNDERFIELD_MARCH);

        bool focused = HasThunderMarker(player, MOD_FOCUSED_PEAL);
        float damage = slamDamage * ECHO_SHARE * (focused ? FOCUSED_ECHO : 1.0f);
        std::vector<Unit*> targets;
        if (target->IsAlive())
            targets.push_back(target);
        if (!focused)
            for (Unit* enemy : EnemiesAround(player, target,
                HasThunderMarker(player, MOD_DISTANT_RUMBLE) ? DISTANT_RADIUS : ECHO_RADIUS))
                if (enemy != target && targets.size() < ECHO_TARGETS)
                    targets.push_back(enemy);
        target->CastSpell(target, SPELL_THUNDER_ECHO_BURST, true);

        int32 each = std::max<int32>(1, int32(damage));
        for (Unit* enemy : targets)
        {
            player->CastCustomSpell(enemy, SPELL_THUNDER_ECHO, &each, nullptr, nullptr, true);
            if (!enemy->IsAlive())
                continue;
            if (HasThunderMarker(player, MOD_COMMANDING_THUNDER) && enemy->CanHaveThreatList())
                enemy->AddThreat(player, float(each) * COMMANDING_THREAT, SPELL_SCHOOL_MASK_NATURE);
            if (!HasThunderMarker(player, MOD_DEAFENING_IMPACT))
                continue;
            player->CastSpell(enemy, SPELL_DEAFENING_IMPACT, true);
            if (Aura* slow = enemy->GetAura(SPELL_DEAFENING_IMPACT, player->GetGUID()))
            {
                slow->SetMaxDuration(DEAFENING_MS);
                slow->SetDuration(DEAFENING_MS);
            }
        }

        float hit = float(targets.size());
        if (HasThunderMarker(player, MOD_SHELTERING_REVERBERATION) && hit > 0)
        {
            player->RemoveAurasDueToSpell(SPELL_SHELTERING_REVERBERATION);
            int32 absorb = std::max<int32>(1, int32(player->GetMaxHealth()
                * std::min(SHELTERING_CAP_PCT, SHELTERING_PCT * hit) / 100.0f));
            player->CastCustomSpell(player, SPELL_SHELTERING_REVERBERATION, &absorb, nullptr, nullptr, true);
            if (Aura* aura = player->GetAura(SPELL_SHELTERING_REVERBERATION))
            {
                aura->SetMaxDuration(SHELTERING_MS);
                aura->SetDuration(SHELTERING_MS);
            }
        }
        if (HasThunderMarker(player, MOD_BATTLE_RHYTHM))
            player->ModifyPower(POWER_RAGE, RHYTHM_RAGE * int32(targets.size()));
        if (HasThunderMarker(player, MOD_ROLLING_THUNDER))
            for (uint32 id = SPELL_THUNDER_CLAP; id; id = sSpellMgr->GetNextSpellInChain(id))
                if (player->HasSpell(id))
                    player->ModifySpellCooldown(id, -ROLLING_MS * int32(targets.size()));
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

    // Chain Heal's extra jump from its last target: 40% of its first heal to the most injured ally within 12.5 yards
    // (20 with Long Reach) it did not heal; Split Tributary: two, 25% each; Concentrated Current 20% more, Deep
    // Current 40% more below half health; Tidal Shelter and Renewing Current follow up for 20% of each one's effective
    // healing. It never jumps again.
    void WellspringJump(Player* player, Unit* from, int32 first, std::set<ObjectGuid> const& healed)
    {
        float range = HasWellMarker(player, MOD_LONG_REACH) ? LONG_REACH : CHAIN_HEAL_JUMP;
        std::vector<Unit*> allies;
        bool split = HasWellMarker(player, MOD_SPLIT_TRIBUTARY);
        for (Unit* ally : InjuredAllies(player, from, range, nullptr))
            if (!healed.count(ally->GetGUID()) && allies.size() < (split ? SPLIT_ALLIES : 1))
                allies.push_back(ally);
        if (allies.empty() || first <= 0)
            return;
        float base = first * (split ? SPLIT_SHARE : WELLSPRING_SHARE);
        if (HasWellMarker(player, MOD_CONCENTRATED_CURRENT))
            base *= CONCENTRATED_CURRENT;
        for (Unit* ally : allies)
        {
            float amount = base;
            if (HasWellMarker(player, MOD_DEEP_CURRENT) && ally->GetHealthPct() < DEEP_CURRENT_BELOW_PCT)
                amount *= DEEP_CURRENT;
            int32 heal = std::max<int32>(1, int32(amount));
            int32 effective = Effective(ally, heal);
            from->CastSpell(ally, SPELL_WELLSPRING_JUMP, true);
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

    // The most injured ally within 20 yards of the channel target, not the target.
    Unit* SecondaryAlly(Player* player, Unit* channelTarget)
    {
        std::vector<Unit*> allies = InjuredAllies(player, channelTarget, SECONDARY_RANGE, channelTarget);
        return allies.empty() ? nullptr : allies.front();
    }

    // A Wandering Mist: to the most injured ally within 20 yards of the channel target (30 with Flowing Currents), or
    // to two at 15% each (Drifting Mist). Jade Shelter: an absorb of 10% of it too; Deep Breath: 0.3% of the Hero's
    // mana back.
    void Wander(Player* player, Unit* channelTarget, float tick)
    {
        float range = HasMistMarker(player, MOD_FLOWING_CURRENTS) ? FLOWING_RANGE : WANDER_RANGE;
        std::vector<Unit*> allies = InjuredAllies(player, channelTarget, range, channelTarget);
        bool drifting = HasMistMarker(player, MOD_DRIFTING_MIST);
        if (allies.size() > (drifting ? 2u : 1u))
            allies.resize(drifting ? 2 : 1);
        for (Unit* ally : allies)
        {
            int32 amount = std::max<int32>(1, int32(tick * (drifting ? DRIFTING_SHARE : WANDER_SHARE)));
            int32 effective = Effective(ally, amount);
            player->CastCustomSpell(ally, SPELL_WANDERING_MIST, &amount, nullptr, nullptr, true);
            if (HasMistMarker(player, MOD_JADE_SHELTER) && effective > 0)
                Refresh(player, ally, SPELL_JADE_SHELTER, std::max<int32>(1, int32(amount * JADE_SHELTER_SHARE)));
            if (HasMistMarker(player, MOD_DEEP_BREATH))
                player->ModifyPower(POWER_MANA, int32(player->GetMaxPower(POWER_MANA) * DEEP_BREATH_PCT / 100.0f));
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

    // The runes' damage over time: a periodic dummy whose tick (keyed by target, spell and caster) the script keeps
    // and deals each second as its own spell's damage (spell_titan_rune_dot), not the aura's amount.
    using DotKey = std::tuple<ObjectGuid, uint32, ObjectGuid>;
    std::map<DotKey, int32> dotTicks;

    uint32 DotDamageSpell(uint32 spellId)
    {
        switch (spellId)
        {
            case SPELL_GRAVE_PESTILENCE: return SPELL_GRAVE_PESTILENCE_TICK;
            case SPELL_WAKE_BLEED: return SPELL_WAKE_BLEED_TICK;
            case SPELL_HOLLOW_FLAY: return SPELL_HOLLOW_FLAY_TICK;
            default: return 0;
        }
    }

    void SetDotTick(Unit* caster, Unit* target, uint32 spellId, int32 tick)
    {
        dotTicks[{ target->GetGUID(), spellId, caster->GetGUID() }] = std::max(1, tick);
    }

    // A spell of the Hero's on the unit for ms (its own duration the longest it can last). A damage over time
    // already there keeps ticking on its own time: its tick and duration are renewed.
    void Lasting(Player* player, Unit* unit, uint32 spellId, int32 ms, int32 const* amount = nullptr)
    {
        if (DotDamageSpell(spellId) && amount)
        {
            SetDotTick(player, unit, spellId, *amount);
            if (Aura* aura = unit->GetAura(spellId, player->GetGUID()))
            {
                aura->SetMaxDuration(std::max(aura->GetMaxDuration(), ms));
                aura->SetDuration(ms);
                return;
            }
        }
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

    // A Grave's tick: the Hero's level and their attack power; their damage bonuses and talents raise it as the core
    // applies them to the Graves' damage spells.
    float GravePower(Player* player)
    {
        return GRAVE_LEVEL * player->GetLevel() + player->GetTotalAttackPowerValue(BASE_ATTACK) * GRAVE_AP;
    }

    bool GraveActive(Player* player, uint8 grave)
    {
        State::GraveAt const& at = StateOf(player).Graves[grave];
        return at.Until > Now() && at.Map == player->GetMapId();
    }

    // Its ground: 5 yards (7.5 with Far-Reaching Graves) about where it lies.
    Shape GraveShape(Player* player, uint8 grave)
    {
        return Circle(StateOf(player).Graves[grave].From,
            GRAVE_RADIUS * (HasGraveMarker(player, MOD_FAR_REACHING_GRAVES) ? FAR_REACHING : 1.0f));
    }

    bool InGrave(Player* player, uint8 grave, Unit* unit)
    {
        return GraveActive(player, grave) && GraveShape(player, grave).Holds(unit);
    }

    uint8 GravesHolding(Player* player, Unit* unit)
    {
        uint8 count = 0;
        for (uint8 grave = 0; grave < GRAVES; ++grave)
            count += InGrave(player, grave, unit) ? 1 : 0;
        return count;
    }

    void ClearGrave(Player* player, uint8 grave)
    {
        StateOf(player).Graves[grave].Until = 0;
        player->RemoveDynObject(SPELL_GRAVE_GROUND_FIRST + grave);
    }

    // A Grave beneath the target (the last of its kind gone), 8 sec (12 with Long Rest).
    void LayGrave(Player* player, uint8 grave, Unit* target)
    {
        State::GraveAt& at = StateOf(player).Graves[grave];
        at.Map = player->GetMapId();
        at.From = target->GetPosition();
        at.Host = target->GetGUID();
        at.Until = Now() + (HasGraveMarker(player, MOD_LONG_REST) ? LONG_REST_MS : GRAVE_MS);
        PlaceDecals(player, SPELL_GRAVE_GROUND_FIRST + grave, { at.From });
    }

    // Blood Strike's shield: 5% of the Hero's health added, up to 15% (25% with Crimson Bastion), 8 sec.
    void CrimsonWard(Player* player)
    {
        float cap = HasGraveMarker(player, MOD_CRIMSON_BASTION) ? BASTION_WARD_CAP_PCT : CRIMSON_WARD_CAP_PCT;
        int32 have = 0;
        if (AuraEffect* ward = player->GetAuraEffect(SPELL_CRIMSON_WARD, EFFECT_0))
            have = ward->GetAmount();
        int32 amount = std::min(int32(player->CountPctFromMaxHealth(int32(cap))),
            have + int32(player->CountPctFromMaxHealth(int32(CRIMSON_WARD_PCT))));
        if (amount > 0)
            Lasting(player, player, SPELL_CRIMSON_WARD, CRIMSON_WARD_MS, &amount);
    }

    // Every second: March of the Dead moves each Grave to its enemy; each Grave's tick on each enemy in it (up to
    // 10), Shared Burial's 20% (35%) on one in two or more, Grave Hunger's 25% below 35% health, the Frozen Grave's
    // slow, Grave Pestilence's disease; Grave Ward's healing.
    void GraveTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        state.GraveTimer += diff;
        if (state.GraveTimer < GRAVE_TICK_MS)
            return;
        state.GraveTimer = 0;
        uint32 now = Now();
        bool any = false;
        bool marching = HasGraveMarker(player, MOD_MARCH_OF_THE_DEAD);
        for (uint8 grave = 0; grave < GRAVES; ++grave)
        {
            State::GraveAt& at = state.Graves[grave];
            if (at.Until && (at.Until <= now || at.Map != player->GetMapId()))
                ClearGrave(player, grave);
            if (!at.Until)
                continue;
            any = true;
            if (!marching)
                continue;
            Unit* host = ObjectAccessor::GetUnit(*player, at.Host);
            if (host && host->IsAlive() && host->GetExactDist2d(&at.From) > 1.0f)
            {
                at.From = host->GetPosition();
                PlaceDecals(player, SPELL_GRAVE_GROUND_FIRST + grave, { at.From });
            }
        }
        if (!any || !player->IsAlive())
            return;

        float burial = HasGraveMarker(player, MOD_DEEP_BURIAL) ? DEEP_BURIAL : SHARED_BURIAL;
        std::size_t hit = 0;
        for (Unit* enemy : EnemiesAround(player, player, 50.0f))
        {
            uint8 count = GravesHolding(player, enemy);
            if (!count)
                continue;
            if (++hit > GRAVE_TARGETS)
                break;
            float scale = count >= 2 ? burial : 1.0f;
            if (count >= 2)
                Lasting(player, enemy, SPELL_SHARED_BURIAL, 1500);
            if (HasGraveMarker(player, MOD_GRAVE_HUNGER) && enemy->GetHealthPct() < HUNGER_BELOW_PCT)
                scale *= GRAVE_HUNGER;
            float power = GravePower(player);
            for (uint8 grave = 0; grave < GRAVES && enemy->IsAlive(); ++grave)
            {
                if (!InGrave(player, grave, enemy))
                    continue;
                if (grave == GRAVE_FROZEN)
                {
                    int32 damage = std::max<int32>(1, int32(power * FROZEN_SHARE * scale));
                    player->CastCustomSpell(enemy, SPELL_FROZEN_GRAVE, &damage, nullptr, nullptr, true);
                    bool cruel = HasGraveMarker(player, MOD_CRUEL_WINTER);
                    int32 slow = cruel ? CRUEL_WINTER_SLOW : GRAVE_SLOW;
                    if (enemy->IsAlive())
                        Lasting(player, enemy, SPELL_GRAVE_CHILL, cruel ? CRUEL_WINTER_MS : CHILL_MS, &slow);
                }
                else if (grave == GRAVE_PLAGUED)
                {
                    int32 damage = std::max<int32>(1, int32(power * PLAGUED_SHARE * scale));
                    player->CastCustomSpell(enemy, SPELL_PLAGUED_GRAVE, &damage, nullptr, nullptr, true);
                    if (HasGraveMarker(player, MOD_GRAVE_PESTILENCE) && enemy->IsAlive())
                    {
                        int32 tick = std::max<int32>(1, int32(power * PESTILENCE_SHARE));
                        Lasting(player, enemy, SPELL_GRAVE_PESTILENCE, PESTILENCE_MS, &tick);
                    }
                }
                else
                {
                    int32 damage = std::max<int32>(1, int32(power * CRIMSON_SHARE * scale));
                    player->CastCustomSpell(enemy, SPELL_CRIMSON_GRAVE, &damage, nullptr, nullptr, true);
                }
            }
        }
        if (HasGraveMarker(player, MOD_GRAVE_WARD) && hit && !player->IsFullHealth())
        {
            int32 heal = std::max<int32>(1, int32(player->GetMaxHealth() * GRAVE_WARD_PCT / 100.0f
                * float(std::min(hit, GRAVE_WARD_ENEMIES))));
            player->CastCustomSpell(player, SPELL_GRAVE_WARD, &heal, nullptr, nullptr, true);
        }
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
    // Bloodletting: a bleed of 30% of the hit over 6 sec, refreshed, never above 60% of a swing; Second Wind: 1% of
    // the Hero's health back for each enemy hit.
    void WakeStrike(Player* player, Shape const& shape, ObjectGuid primary, float share)
    {
        if (!player->IsInWorld() || !player->IsAlive())
            return;
        float swing = float(player->CalculateDamage(BASE_ATTACK, true, true));
        float bonus = HasOnslaughtMarker(player, MOD_EXECUTIONERS_CUT) ? EXECUTIONER_PRIMARY : WAKE_PRIMARY;
        bool bleed = HasOnslaughtMarker(player, MOD_BLOODLETTING);
        std::vector<Unit*> enemies = EnemiesIn(player, shape, WAKE_TARGETS);
        if (HasOnslaughtMarker(player, MOD_SECOND_WIND) && !enemies.empty())
        {
            int32 heal = std::max<int32>(1, int32(player->GetMaxHealth() * SECOND_WIND_PCT / 100.0f
                * float(enemies.size())));
            player->CastCustomSpell(player, SPELL_SECOND_WIND, &heal, nullptr, nullptr, true);
        }
        for (Unit* enemy : enemies)
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

    // The next different melee ability (on any enemy) reopens the wake, once: its attack along the wake a moment
    // later (at once with Overpowering Echo), Aftershock a second impact there 1.5 sec on at half.
    void ReopenWake(Player* player)
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
        ObjectGuid primary = state.WakePrimary;
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

    bool WakeOpen(Player* player)
    {
        State& state = StateOf(player);
        return state.Wake && state.WakeMap == player->GetMapId() && Now() < state.WakeUntil;
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

    void EchoOn(Player* player, Unit* unit, float amount, bool heal)
    {
        int32 points = std::max<int32>(1, int32(amount));
        player->CastSpell(unit, heal ? SPELL_DAWN_BOLT_HEAL : SPELL_DAWN_BOLT, true);
        player->CastCustomSpell(unit, heal ? SPELL_DAWN_ECHO_HEAL : SPELL_DAWN_ECHO_DAMAGE, &points, nullptr,
            nullptr, true);
        if (heal && HasDawnMarker(player, MOD_RADIANT_WARD))
        {
            int32 ward = std::max<int32>(1, int32(amount * RADIANT_SHARE));
            unit->RemoveAurasDueToSpell(SPELL_RADIANT_WARD, player->GetGUID());
            player->CastCustomSpell(unit, SPELL_RADIANT_WARD, &ward, nullptr, nullptr, true);
            if (Aura* aura = unit->GetAura(SPELL_RADIANT_WARD, player->GetGUID()))
            {
                aura->SetMaxDuration(RADIANT_MS);
                aura->SetDuration(RADIANT_MS);
            }
        }
        if (!heal && HasDawnMarker(player, MOD_DAWNS_MERCY))
        {
            int32 mercy = std::max<int32>(1, int32(amount * MERCY_SHARE));
            player->CastCustomSpell(player, SPELL_DAWN_ECHO_HEAL, &mercy, nullptr, nullptr, true);
        }
    }

    // One echo landing: a strike on the enemy (the nearest one within 15 yards if it is gone) or a heal on the most
    // injured ally within 15 yards of the Hero; Sunburst splashes it about there, Sacred Ground leaves a patch.
    void Echo(Player* player, ObjectGuid enemyGuid, float amount, bool heal)
    {
        Unit* unit = nullptr;
        if (heal)
        {
            std::vector<Unit*> allies = InjuredAllies(player, player, DAWN_REACH, nullptr);
            unit = allies.empty() ? player : allies.front();
        }
        else
        {
            unit = ObjectAccessor::GetUnit(*player, enemyGuid);
            if (!Valid(player, unit) || !unit->IsWithinDistInMap(player, DAWN_REACH * 2.0f))
            {
                std::vector<Unit*> enemies = EnemiesAround(player, player, DAWN_REACH);
                unit = enemies.empty() ? nullptr : enemies.front();
            }
        }
        if (!unit)
            return;
        EchoOn(player, unit, amount, heal);
        if (HasDawnMarker(player, MOD_SUNBURST))
        {
            unit->CastSpell(unit, SPELL_SUNBURST, true);
            std::vector<Unit*> touched = heal ? InjuredAllies(player, unit, SUNBURST_RADIUS, unit)
                : EnemiesAround(player, unit, SUNBURST_RADIUS);
            std::size_t count = 0;
            for (Unit* other : touched)
                if (other != unit && count++ < DAWN_TARGETS)
                    EchoOn(player, other, amount * SUNBURST_SHARE, heal);
        }
        if (HasDawnMarker(player, MOD_SACRED_GROUND))
        {
            State& state = StateOf(player);
            state.SacredAt = unit->GetPosition();
            state.SacredMap = player->GetMapId();
            state.SacredUntil = Now() + SACRED_MS;
            state.SacredTick = amount * SACRED_SHARE;
            state.SacredHeal = heal;
            state.SacredTimer = 0;
            PlaceDecals(player, SPELL_SACRED_GROUND, { state.SacredAt });
        }
    }

    // A Dawn Echo of what was dealt or healed: half of it 0.75 sec later (40% more with Focused Dawn). A damage echo
    // heals instead for a Healer; Communion makes every echo both, at 35% each. Swift Dawn: Crusader Strike and Holy
    // Shock come back 0.5 sec sooner.
    void QueueEcho(Player* player, Unit* target, float amount, bool heal, float share = DAWN_SHARE)
    {
        if (amount <= 0.0f)
            return;
        amount *= share * (HasDawnMarker(player, MOD_FOCUSED_DAWN) ? FOCUSED_DAWN : 1.0f);
        bool communion = HasDawnMarker(player, MOD_COMMUNION);
        if (!heal && Wildcard::GetRole(player) == Wildcard::ROLE_HEALER)
            heal = true;
        ObjectGuid enemy = target && !heal ? target->GetGUID() : ObjectGuid::Empty;
        if (communion && target && player->IsValidAttackTarget(target))
            enemy = target->GetGUID();
        player->m_Events.AddEventAtOffset([player, enemy, amount, heal, communion]()
        {
            if (!player->IsInWorld() || !player->IsAlive())
                return;
            if (communion)
            {
                Echo(player, enemy, amount * COMMUNION_SHARE, false);
                Echo(player, enemy, amount * COMMUNION_SHARE, true);
            }
            else
                Echo(player, enemy, amount, heal);
        }, Milliseconds(DAWN_DELAY_MS));
        if (HasDawnMarker(player, MOD_SWIFT_DAWN))
            for (uint32 first : { SPELL_CRUSADER_STRIKE, SPELL_HOLY_SHOCK })
                for (uint32 id = first; id; id = sSpellMgr->GetNextSpellInChain(id))
                    if (player->HasSpell(id))
                        player->ModifySpellCooldown(id, -SWIFT_DAWN_MS);
    }

    // Sacred Ground's ticks.
    void DawnTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        if (!state.SacredUntil)
            return;
        if (Now() >= state.SacredUntil || state.SacredMap != player->GetMapId())
        {
            state.SacredUntil = 0;
            player->RemoveDynObject(SPELL_SACRED_GROUND);
            return;
        }
        state.SacredTimer += diff;
        if (state.SacredTimer < 1000)
            return;
        state.SacredTimer = 0;
        Shape patch = Circle(state.SacredAt, SACRED_RADIUS);
        int32 tick = std::max<int32>(1, int32(state.SacredTick));
        if (state.SacredHeal)
        {
            std::size_t healed = 0;
            for (Unit* ally : InjuredAllies(player, player, 60.0f, nullptr))
                if (healed < DAWN_TARGETS && patch.Holds(ally))
                {
                    ++healed;
                    player->CastCustomSpell(ally, SPELL_DAWN_ECHO_HEAL, &tick, nullptr, nullptr, true);
                }
        }
        else
            for (Unit* enemy : EnemiesIn(player, patch, DAWN_TARGETS))
                player->CastCustomSpell(enemy, SPELL_DAWN_ECHO_DAMAGE, &tick, nullptr, nullptr, true);
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
    }

    // Mind Flay: a Hollow Echo beside the Hero, following them, 10 sec (15 with Lasting Echoes); a fourth (fifth
    // with Gathering) replaces the oldest.
    void CallEcho(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        std::vector<Creature*> echoes = LiveEchoes(player);
        std::size_t most = HasHollowMarker(player, MOD_GATHERING) ? GATHERING_ECHOES : MAX_HOLLOW_ECHOES;
        if (echoes.size() >= most)
            if (TempSummon* oldest = echoes.front()->ToTempSummon())
            {
                oldest->UnSummon();
                state.HollowEchoes.erase(state.HollowEchoes.begin());
            }
        uint32 ms = HasHollowMarker(player, MOD_LASTING_ECHOES) ? LASTING_ECHO_MS : HOLLOW_ECHO_MS;
        Position pos = player->GetNearPosition(1.5f, frand(0.0f, 2.0f * float(M_PI)));
        TempSummon* echo = player->GetMap()->SummonCreature(NPC_HOLLOW_ECHO, pos,
            sSummonPropertiesStore.LookupEntry(SUMMON_PROPERTIES_GUARDIAN), ms, player);
        if (!echo)
            return;
        state.HollowEchoes.push_back(echo->GetGUID());
        if (echo->IsAIEnabled)
            echo->AI()->SetGUID(target ? target->GetGUID() : ObjectGuid::Empty, 0);
    }

    // Mind Blast's command: every echo turns on its target (Dissonance: each on another enemy with the Hero's
    // Shadow Word: Pain, the target first), in a barrage of 3 bolts (Unison: together, 20% more) or a piercing
    // scream along the way (Accusation); then their channels resume on it. Swift Command: Mind Blast 0.5 sec sooner
    // for each.
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
            for (Unit* enemy : EnemiesAround(player, player, HOLLOW_RANGE))
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
        if (HasHollowMarker(player, MOD_SWIFT_COMMAND))
            for (uint32 id = SPELL_MIND_BLAST; id; id = sSpellMgr->GetNextSpellInChain(id))
                if (player->HasSpell(id))
                    player->ModifySpellCooldown(id, -SWIFT_COMMAND_MS * int32(echoes.size()));
    }

    // ---- The Shattered Sun ---------------------------------------------------------------------------------------

    bool HasSunMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_SUN_MODIFIER_FIRST, modifier);
    }

    uint8 Shards(Player* player, Unit* unit)
    {
        Aura* aura = unit->GetAura(SPELL_SUNSHARD, player->GetGUID());
        return aura ? aura->GetStackAmount() : 0;
    }

    // count Sunshards in the target (up to 5, 8 with Heavy Sun), all renewed to 15 sec.
    void Embed(Player* player, Unit* target, uint8 count)
    {
        uint8 most = HasSunMarker(player, MOD_HEAVY_SUN) ? HEAVY_SHARDS : MAX_SHARDS;
        uint8 shards = std::min<uint8>(most, Shards(player, target) + count);
        Lasting(player, target, SPELL_SUNSHARD, SUNSHARD_MS);
        if (Aura* aura = target->GetAura(SPELL_SUNSHARD, player->GetGUID()))
            aura->SetStackAmount(shards);
        State& state = StateOf(player);
        if (std::find(state.Shards.begin(), state.Shards.end(), target->GetGUID()) == state.Shards.end())
            state.Shards.push_back(target->GetGUID());
    }

    // The Hero's floor for a shatter's base: level x 4 and 40% of their spell power (Fire or Frost, the higher), with
    // their Frostfire damage bonuses; a weak frost spell (Frost Nova, Ice Lance) still shatters well.
    float SunPower(Player* player)
    {
        float power = float(std::max(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE),
            player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FROST)));
        return (SUN_LEVEL * player->GetLevel() + power * SUN_POWER) * player->GetTotalAuraMultiplierByMiscMask(
            SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, SPELL_SCHOOL_MASK_FIRE | SPELL_SCHOOL_MASK_FROST);
    }

    // The shards' burst about their host: each enemy within 8 yards (12 with Wide Burst) takes the burst, the host
    // 40% more with Implosion; Frozen Core slows them, Scalding Ground leaves burning ground, Second Sunrise pulses
    // again.
    void Burst(Player* player, Unit* host, float burst)
    {
        Position at = host->GetPosition();
        float radius = HasSunMarker(player, MOD_WIDE_BURST) ? WIDE_RADIUS : FRACTURE_RADIUS;
        host->CastSpell(host, SPELL_FRACTURE_LOOK, true);
        for (Unit* enemy : EnemiesIn(player, Circle(at, radius), FRACTURE_TARGETS))
        {
            float share = enemy == host && HasSunMarker(player, MOD_IMPLOSION) ? IMPLOSION : 1.0f;
            int32 damage = std::max<int32>(1, int32(burst * share));
            player->CastCustomSpell(enemy, SPELL_FRACTURE, &damage, nullptr, nullptr, true);
            if (HasSunMarker(player, MOD_FROZEN_CORE) && enemy->IsAlive())
                Lasting(player, enemy, SPELL_FROZEN_CORE, FROZEN_CORE_MS);
        }
        if (HasSunMarker(player, MOD_SCALDING_GROUND))
        {
            State& state = StateOf(player);
            state.ScaldAt = at;
            state.ScaldMap = player->GetMapId();
            state.ScaldUntil = Now() + SCALDING_MS;
            state.ScaldTick = burst * SCALDING_SHARE;
            state.ScaldTimer = 0;
            PlaceDecals(player, SPELL_SCALDING_GROUND, { at });
        }
        if (HasSunMarker(player, MOD_SECOND_SUNRISE))
            player->m_Events.AddEventAtOffset([player, at, radius, burst]()
            {
                if (!player->IsInWorld() || !player->IsAlive())
                    return;
                std::vector<Unit*> enemies = EnemiesIn(player, Circle(at, radius), FRACTURE_TARGETS);
                if (enemies.empty())
                    return;
                enemies.front()->CastSpell(enemies.front(), SPELL_COLLAPSE_LOOK, true);
                int32 pulse = std::max<int32>(1, int32(burst * SUNRISE_SHARE));
                for (Unit* enemy : enemies)
                    player->CastCustomSpell(enemy, SPELL_COLLAPSE, &pulse, nullptr, nullptr, true);
            }, Milliseconds(SUNRISE_DELAY_MS));
    }

    // A frost spell on a host: every shard in it shatters, 25% of the spell's damage each (the Hero's floor at
    // least). Chain Reaction: shards in other enemies within 10 yards shatter too, at half (once, no further chain).
    void Shatter(Player* player, Unit* host, float frost, bool chained = false)
    {
        uint8 shards = Shards(player, host);
        if (!shards)
            return;
        host->RemoveAurasDueToSpell(SPELL_SUNSHARD, player->GetGUID());
        float base = std::max(frost, SunPower(player));
        Burst(player, host, base * SHARD_SHARE * shards * (chained ? CHAIN_SHARE : 1.0f));
        if (chained || !HasSunMarker(player, MOD_CHAIN_REACTION))
            return;
        for (Unit* other : EnemiesAround(player, host, CHAIN_RADIUS))
            if (other != host && Shards(player, other))
                Shatter(player, other, frost, true);
    }

    // Scalding Ground's ticks; the shards' hosts kept for clearing.
    void SunTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        state.Shards.erase(std::remove_if(state.Shards.begin(), state.Shards.end(), [player](ObjectGuid guid)
        {
            Unit* host = ObjectAccessor::GetUnit(*player, guid);
            return !host || !Shards(player, host);
        }), state.Shards.end());
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

    // What the five leave behind (death, logout, a spec or modifier change): the Graves and their shield, the wake,
    // Sacred Ground, the echoes, the shards.
    void ClearLegends(Player* player)
    {
        State& state = StateOf(player);
        for (uint8 grave = 0; grave < GRAVES; ++grave)
            ClearGrave(player, grave);
        player->RemoveAurasDueToSpell(SPELL_CRIMSON_WARD);
        EndWake(player);
        state.SacredUntil = 0;
        player->RemoveDynObject(SPELL_SACRED_GROUND);
        DismissEchoes(player);
        for (ObjectGuid guid : state.Shards)
            if (Unit* host = ObjectAccessor::GetUnit(*player, guid))
                host->RemoveAurasDueToSpell(SPELL_SUNSHARD, player->GetGUID());
        state.Shards.clear();
        state.ScaldUntil = 0;
        player->RemoveDynObject(SPELL_SCALDING_GROUND);
    }

    // At each login and rune change: what earlier drafts taught or left on the Hero, gone.
    void ApplyLegend(Player* player, uint8 /*rune*/)
    {
        auto unlearn = [player](uint32 spellId)
        {
            if (player->HasSpell(spellId))
                player->removeSpell(spellId, SPEC_MASK_ALL, true);
        };
        // The Threefold Grave's first draft taught Requiem (90831) and three more (90832-90834), and wore a
        // Dominion (90835-90837): gone with it.
        for (uint32 old = 90831; old <= 90834; ++old)
            unlearn(old);
        for (uint32 old = 90835; old <= 90837; ++old)
            player->RemoveAurasDueToSpell(old);
        player->RemoveAurasDueToSpell(SPELL_CRIMSON_MARCH);
        // The Dawnbound Crusader's first draft taught Dawn Vow and vowed the Hero: gone with it.
        unlearn(SPELL_DAWN_VOW);
        player->RemoveAurasDueToSpell(SPELL_DAWN_VOWED);
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
            return Refuse(player, "This specialization already has its rune: .unbindrune lets go of it.");
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

    bool Unbind(Player* player)
    {
        Choice& choice = ActiveChoice(player);
        if (!choice.Rune)
            return false;
        choice = Choice();
        Save(player, Specs::ActiveSpec(player));
        ClearCombat(player);
        Apply(player);
        ChatHandler(player->GetSession()).SendSysMessage("|cffa335eeLegendary Rune:|r unbound. Choose another in the "
            "rune panel.");
        SendState(player);
        Send(player, "O");
        return true;
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

// The Stormbound Conduit's aura (90750): a Lightning Bolt hit arcs (Overcharge: 50% more on a critical strike;
// Distant Conductor: a Lightning Shield retaliation at its target); Chain Surge: each Chain Lightning hit arcs once.
class spell_titan_rune_stormbound_conduit : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_stormbound_conduit);

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        SpellInfo const* spell = eventInfo.GetSpellInfo();
        Player* player = GetTarget()->ToPlayer();
        Unit* target = eventInfo.GetActionTarget();
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!spell || !player || !target || target == player || !damageInfo
            || !(eventInfo.GetHitMask() & (PROC_HIT_NORMAL | PROC_HIT_CRITICAL | PROC_HIT_ABSORB)))
            return false;
        float damage = float(damageInfo->GetDamage() + damageInfo->GetAbsorb());
        if ((eventInfo.GetHitMask() & PROC_HIT_CRITICAL) && HasStormMarker(player, MOD_OVERCHARGE))
            damage *= OVERCHARGE;
        uint32 first = spell->GetFirstRankSpell()->Id;
        if (first == SPELL_LIGHTNING_BOLT)
        {
            Arc(player, target, damage, BOLT_ARC_SHARE,
                HasStormMarker(player, MOD_FORKED_TEMPEST) ? FORKED_ARCS : BOLT_ARCS, true);
            if (HasStormMarker(player, MOD_DISTANT_CONDUCTOR) && target->IsAlive())
                if (AuraEffect* shield = Shield(player))
                    Retaliate(player, target, shield);
        }
        else if (first == SPELL_CHAIN_LIGHTNING && HasStormMarker(player, MOD_CHAIN_SURGE))
            Arc(player, target, damage, CHAIN_ARC_SHARE, 1, false);
        return false;                               // the rune's work is done here; nothing else to proc
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_titan_rune_stormbound_conduit::CheckProc);
    }
};

// Lightning Shield (every rank): the first draft's script (it gave way to Storm Within); nothing now.
class spell_titan_rune_lightning_shield : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_lightning_shield);

    void Register() override { }
};

// A Lightning Shield retaliation (its damage, every rank, and the rune's own 90754) with the Stormbound Conduit:
// it arcs to an enemy near the target for 50% of its damage (two with Live Wire).
class spell_titan_rune_lightning_shield_damage : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_lightning_shield_damage);

    void Strike()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_STORMBOUND_CONDUIT))
            return;
        Arc(player, target, float(GetHitDamage()), SHIELD_ARC_SHARE,
            HasStormMarker(player, MOD_LIVE_WIRE) ? LIVE_WIRE_ARCS : SHIELD_ARCS, false);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_titan_rune_lightning_shield_damage::Strike);
    }
};

// Thunder Clap (every rank) with Echoing Thunder: the charge (ChargeThunder).
class spell_titan_rune_thunder_clap : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_thunder_clap);

    void Storm()
    {
        if (Player* player = GetCaster()->ToPlayer())
            if (player->HasAura(SPELL_RUNE_ECHOING_THUNDER))
                ChargeThunder(player);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_rune_thunder_clap::Storm);
    }
};

// Shield Slam (every rank) with Echoing Thunder: while charged, a Thunder Echo (its damage before the target's
// mitigation).
class spell_titan_rune_shield_slam : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_shield_slam);

    void Echo()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target && player->HasAura(SPELL_RUNE_ECHOING_THUNDER))
            ThunderEcho(player, target, GetHitDamage());
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_titan_rune_shield_slam::Echo);
    }
};

// Chain Heal (every rank) with the Wellspring: 20% more on the Hero's Earth Shield wearer (40% with Bedrock;
// Renewed Shield: a charge back); a moment after its first hit, its extra jump from its last target.
class spell_titan_rune_chain_heal : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_chain_heal);

    struct Chain
    {
        int32 First = 0;
        ObjectGuid Last;
        std::set<ObjectGuid> Healed;
    };

    void Flow()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_WELLSPRING))
            return;
        if (HasEarthShield(player, target))
        {
            SetHitHeal(int32(GetHitHeal() * (HasWellMarker(player, MOD_BEDROCK) ? BEDROCK_BONUS : WEARER_BONUS)));
            if (HasWellMarker(player, MOD_RENEWED_SHIELD))
                for (uint32 id = SPELL_EARTH_SHIELD; id; id = sSpellMgr->GetNextSpellInChain(id))
                    if (Aura* shield = target->GetAura(id, player->GetGUID()))
                        if (shield->GetCharges() < shield->CalcMaxCharges())
                            shield->ModCharges(1);
        }
        if (!_chain)
        {
            _chain = std::make_shared<Chain>();
            _chain->First = GetHitHeal();
            std::shared_ptr<Chain> chain = _chain;
            player->m_Events.AddEventAtOffset([player, chain]()
            {
                if (!player->IsInWorld() || !player->IsAlive())
                    return;
                if (Unit* last = ObjectAccessor::GetUnit(*player, chain->Last))
                    WellspringJump(player, last, chain->First, chain->Healed);
            }, Milliseconds(JUMP_DELAY_MS));
        }
        _chain->Last = target->GetGUID();
        _chain->Healed.insert(target->GetGUID());
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_titan_rune_chain_heal::Flow);
    }

    std::shared_ptr<Chain> _chain;
};

// Earth Shield's heal (every rank) with Flowing Waters: half of it again on the most injured ally within 12.5 yards
// of the wearer.
class spell_titan_rune_earth_shield_heal : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_earth_shield_heal);

    void Flow()
    {
        Unit* origin = GetOriginalCaster() ? GetOriginalCaster() : GetCaster();
        Player* player = origin ? origin->ToPlayer() : nullptr;
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_WELLSPRING) || !HasWellMarker(player, MOD_FLOWING_WATERS))
            return;
        std::vector<Unit*> allies = InjuredAllies(player, target, CHAIN_HEAL_JUMP, target);
        if (allies.empty())
            return;
        int32 heal = std::max<int32>(1, int32(GetHitHeal() * FLOWING_SHARE));
        target->CastSpell(allies.front(), SPELL_WELLSPRING_JUMP, true);
        player->CastCustomSpell(allies.front(), SPELL_WELLSPRING_HEAL, &heal, nullptr, nullptr, true);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_earth_shield_heal::Flow);
    }
};

// Soothing Mist's cast with the Wandering Mist: nothing more now (the first draft's Rolling Mist was here).
class spell_titan_rune_soothing_mist_cast : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_soothing_mist_cast);

    void Register() override { }
};

// Soothing Mist's ticks with the Wandering Mist: a Wandering Mist each tick, 25% of it; Effortless Breathing: 25%
// of a tick's mana back. Lingering Mist: the channel's end leaves two ticks over 4 sec on its target.
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
        StateOf(player).MistTickAmount = aurEff->GetAmount();
        if (HasMistMarker(player, MOD_EFFORTLESS_BREATHING))
        {
            int32 cost = int32(GetSpellInfo()->ManaCostPercentage * player->GetCreateMana() / 100);
            if (int32 back = int32(cost * EFFORTLESS_SHARE / SOOTHING_MIST_TICKS); back > 0)
                player->ModifyPower(POWER_MANA, back);
        }
        Wander(player, target, float(aurEff->GetAmount()));
    }

    void Gone(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* caster = GetCaster();
        Player* player = caster ? caster->ToPlayer() : nullptr;
        Unit* target = GetTarget();
        if (!player || !target->IsAlive() || !player->HasAura(SPELL_RUNE_WANDERING_MIST)
            || !HasMistMarker(player, MOD_LINGERING_MIST))
            return;
        int32 tick = StateOf(player).MistTickAmount;
        if (tick > 0)
            Refresh(player, target, SPELL_LINGERING_MIST, std::max<int32>(1, tick * LINGERING_TICKS / 2));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_titan_rune_soothing_mist::Tick, EFFECT_0,
            SPELL_AURA_PERIODIC_HEAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_titan_rune_soothing_mist::Gone, EFFECT_0,
            SPELL_AURA_PERIODIC_HEAL, AURA_EFFECT_HANDLE_REAL);
    }
};

// Enveloping Mist and Vivify with the Wandering Mist: on the Soothing Mist target, cast at once (their mana and
// cooldowns as ever) without breaking the channel; then Comforting Veil (Enveloping Mist: a lesser one on the most
// injured ally near the target, 30% of it) or Vivacious Flow (Vivify: 30% of its healing to that ally).
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

// Icy Touch, Plague Strike and Blood Strike (every rank) with the Threefold Grave: their Grave.
class spell_titan_rune_grave_strike : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_grave_strike);

    void Lay()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_THREEFOLD_GRAVE) || _done)
            return;
        _done = true;
        uint32 first = GetSpellInfo()->GetFirstRankSpell()->Id;
        if (first == SPELL_BLOOD_STRIKE)
            CrimsonWard(player);
        if (target->IsAlive())
            LayGrave(player, first == SPELL_ICY_TOUCH ? GRAVE_FROZEN : first == SPELL_PLAGUE_STRIKE ? GRAVE_PLAGUED
                : GRAVE_CRIMSON, target);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_grave_strike::Lay);
    }

    bool _done = false;
};

// Mortal Strike and Bloodthirst (every rank) with Unbroken Onslaught: the other one reopens the wake, then either
// cuts a new wake through its target.
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
        if (StateOf(player).WakeCreator != first && WakeOpen(player))
            ReopenWake(player);
        if (target->IsAlive())
            CutWake(player, target, first);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_wake_cut::Cut);
    }
};

// Unbroken Onslaught's aura: any other melee ability that strikes an enemy reopens the wake.
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
        if (WakeOpen(player))
            ReopenWake(player);
        return false;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_titan_rune_unbroken_onslaught::CheckProc);
    }
};

// Dawn Vow: the first draft's action (unlearned); nothing.
class spell_titan_rune_dawn_vow : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_dawn_vow);

    void Register() override { }
};

// A Judgement with the Dawnbound Crusader and Judgment's Echo: a Dawn Echo of its damage.
class spell_titan_rune_judgement : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_judgement);

    void Echo()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_DAWNBOUND_CRUSADER)
            || !HasDawnMarker(player, MOD_JUDGMENTS_ECHO))
            return;
        // A Judgement's own cast deals nothing (its seal's spell does): the echo is the Hero's level x 5 and the
        // higher of their Holy spell power or half their attack power.
        float power = std::max(float(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_HOLY)),
            player->GetTotalAttackPowerValue(BASE_ATTACK) / 2.0f);
        QueueEcho(player, target, JUDGMENT_LEVEL * player->GetLevel() + power, false);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_judgement::Echo);
    }
};

// Crusader Strike and Holy Shock's damage or heal (every rank): a Dawn Echo of it; Steadfast: Holy Shock's twice,
// the second at half.
class spell_titan_rune_dawn_echo : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_dawn_echo);

    void Echo()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_DAWNBOUND_CRUSADER))
            return;
        bool heal = GetHitHeal() > 0;
        float amount = float(heal ? GetHitHeal() : GetHitDamage());
        QueueEcho(player, target, amount, heal);
        if (GetSpellInfo()->GetFirstRankSpell()->Id != SPELL_CRUSADER_STRIKE && HasDawnMarker(player, MOD_STEADFAST))
            QueueEcho(player, target, amount, heal, DAWN_SHARE * STEADFAST_SHARE);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_dawn_echo::Echo);
    }
};

// Mind Flay (every rank) with the Hollow Congregation: a Hollow Echo called.
class spell_titan_rune_mind_flay : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_mind_flay);

    void Start()
    {
        Player* player = GetCaster()->ToPlayer();
        if (player && player->HasAura(SPELL_RUNE_HOLLOW_CONGREGATION))
            CallEcho(player, GetExplTargetUnit());
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

// The Shattered Sun's fire spells (Fireball, Pyroblast, Fire Blast, Scorch; every rank) collapse a Broken Core or
// embed a shard (Fireball and Pyroblast spread it); its frost spells (Frostbolt, Ice Lance, Frost Nova, Cone of Cold)
// fracture a shard.
class spell_titan_rune_shattered_sun : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_shattered_sun);

    // A frost spell shatters the target's shards; a fire spell embeds one (Wildfire: Fireball and Pyroblast two;
    // Spreading Sun: they also embed one in 2 more enemies within 8 yards).
    void Strike()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_SHATTERED_SUN))
            return;
        uint32 first = GetSpellInfo()->GetFirstRankSpell()->Id;
        if (first == SPELL_FROSTBOLT || first == SPELL_ICE_LANCE || first == SPELL_FROST_NOVA
            || first == SPELL_CONE_OF_COLD)
        {
            Shatter(player, target, float(std::max(0, GetHitDamage())));
            return;
        }
        if (!target->IsAlive())
            return;
        bool big = first == SPELL_FIREBALL || first == SPELL_PYROBLAST;
        Embed(player, target, big && HasSunMarker(player, MOD_WILDFIRE) ? 2 : 1);
        if (!big || !HasSunMarker(player, MOD_SPREADING_SUN))
            return;
        std::size_t count = SPREAD;
        for (Unit* enemy : EnemiesAround(player, target, SPREAD_RANGE))
            if (enemy != target && count)
            {
                Embed(player, enemy, 1);
                --count;
            }
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_shattered_sun::Strike);
    }
};

// The runes' damage over time (Grave Pestilence, Bloodletting, Hollow Flay): each second its kept tick, dealt as
// its own damage spell by whoever put it there.
class spell_titan_rune_dot : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_dot);

    void Tick(AuraEffect const* /*aurEff*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        if (!caster || !target || !target->IsAlive())
            return;
        auto tick = dotTicks.find({ target->GetGUID(), GetId(), caster->GetGUID() });
        if (tick == dotTicks.end())
            return;
        int32 damage = tick->second;
        caster->CastCustomSpell(target, DotDamageSpell(GetId()), &damage, nullptr, nullptr, true);
    }

    void Gone(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetCasterGUID() && !GetTarget()->HasAura(GetId(), GetCasterGUID()))
            dotTicks.erase({ GetTarget()->GetGUID(), GetId(), GetCasterGUID() });
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_titan_rune_dot::Tick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        AfterEffectRemove += AuraEffectRemoveFn(spell_titan_rune_dot::Gone, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY,
            AURA_EFFECT_HANDLE_REAL);
    }
};

// A Hollow Echo (910122): the Hero's look, hollowed, following them; for 10 sec (15 with Lasting Echoes) a weaker
// Mind Flay on the Hero's target (else the one it was called for, else the nearest), 40 yards; quiet during a
// barrage. Last Word: a last, stronger attack as it fades. Fervor: stronger while the Hero channels Mind Flay.
// Shadow Mend: each channel heals the Hero for 10% of its damage.
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
        me->GetMotionMaster()->MoveFollow(owner, 2.5f, frand(1.5f, 4.8f));
        scheduler.Schedule(200ms, [this](TaskContext channel)
        {
            Channel();
            channel.Repeat(500ms);
        });
        uint32 ms = HasHollowMarker(owner, MOD_LASTING_ECHOES) ? LASTING_ECHO_MS : HOLLOW_ECHO_MS;
        scheduler.Schedule(Milliseconds(ms - 400), [this](TaskContext)
        {
            Player* hero = Owner();
            Unit* target = Target(hero);
            if (!hero || !target || !HasHollowMarker(hero, MOD_LAST_WORD))
                return;
            int32 damage = std::max<int32>(1, int32(FlayTick(hero) * HOLLOW_SHARE * LAST_WORD_TICKS));
            me->CastCustomSpell(target, SPELL_HOLLOW_LAST_WORD, &damage, nullptr, nullptr, true);
        });
    }

    void SetGUID(ObjectGuid const& guid, int32 /*id*/) override
    {
        _target = guid;
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

    // Its enemy: the one Mind Blast gave it during a barrage, else the Hero's target, else the one it was called
    // for, else the nearest to it.
    Unit* Target(Player* hero)
    {
        if (!hero)
            return nullptr;
        auto fits = [&](Unit* unit)
        {
            return Valid(hero, unit) && me->IsWithinDistInMap(unit, HOLLOW_RANGE) && me->IsWithinLOSInMap(unit);
        };
        if (Unit* chosen = hero->GetSelectedUnit(); Now() >= _quietUntil && fits(chosen))
        {
            _target = chosen->GetGUID();
            return chosen;
        }
        if (Unit* unit = _target ? ObjectAccessor::GetUnit(*me, _target) : nullptr; fits(unit))
            return unit;
        for (Unit* enemy : EnemiesAround(hero, me, HOLLOW_RANGE))
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
        if (HasHollowMarker(hero, MOD_FERVOR))
            if (Spell* flay = hero->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
                flay && flay->m_spellInfo->GetFirstRankSpell()->Id == SPELL_MIND_FLAY)
                tick *= FERVOR;
        int32 damage = std::max<int32>(1, int32(tick));
        me->SetFacingToObject(target);
        SetDotTick(me, target, SPELL_HOLLOW_FLAY, damage);
        me->CastCustomSpell(target, SPELL_HOLLOW_FLAY, &damage, nullptr, nullptr, true);
        if (!HasHollowMarker(hero, MOD_SHADOW_MEND))
            return;
        SpellInfo const* flay = sSpellMgr->GetSpellInfo(SPELL_HOLLOW_FLAY);
        float ticks = flay && flay->GetMaxDuration() > 0 ? flay->GetMaxDuration() / 1000.0f : FLAY_CHANNEL_TICKS;
        int32 mend = std::max<int32>(1, int32(damage * ticks * MEND_SHARE));
        hero->CastCustomSpell(hero, SPELL_HOLLOW_MEND, &mend, nullptr, nullptr, true);
    }

    ObjectGuid _owner, _target;
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

    // The panel's state goes out a few seconds in as well, without waiting to be asked: a request sent while the
    // client was still loading, or its answer, can be lost.
    void OnPlayerLogin(Player* player) override
    {
        Load(player);
        if (!Wildcard::IsHero(player))
            return;
        Apply(player);
        ObjectGuid guid = player->GetGUID();
        player->m_Events.AddEventAtOffset([player, guid]()
        {
            if (player->IsInWorld() && player->GetGUID() == guid)
                SendState(player);
        }, 5s);
    }

    void OnPlayerLogout(Player* player) override
    {
        ClearCombat(player);
        states.erase(player->GetGUID().GetCounter());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        CharacterDatabase.Execute(Acore::StringFormat("DELETE FROM `titan_runes` WHERE `guid` = {}",
            guid.GetCounter()));
    }

    // Every level: the panel's state (its level); at UNLOCK_LEVEL the panel opens while the active spec has no rune.
    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (!Wildcard::IsHero(player))
            return;
        Apply(player);
        SendState(player);
        if (oldLevel < UNLOCK_LEVEL && player->GetLevel() >= UNLOCK_LEVEL && Prompt(player))
            Send(player, "O");
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
    RegisterSpellScript(spell_titan_rune_grave_strike);
    RegisterSpellScript(spell_titan_rune_dot);
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
