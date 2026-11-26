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
#include "cards.h"
#include "tank_runes.h"
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
// 31 the Shattered Sun; then 32 the Spectral Blade (its doc: Phantom Edge) and 33 the Wild Hunt; then five from their
// own dossiers: 34 Arcane Mind, 35 the Splintered Crown, 36 Phoenix Wake, 37 Astral Horizon, 38 Guiding Light; and
// three druid runes: 39 Elune's Guardian, 40 Astral Convergence, 41 Ursoc's Champion.
//
// - Unlocked from level 1 (UNLOCK_LEVEL): /runes and the rune button open the panel (the Emberforge_Runes addon,
//   addon whispers with prefix EFRN) and a rune can be bound; from level 5 (PROMPT_LEVEL) it opens by itself while
//   the active specialization has no rune, suggesting up to 3 the Hero's abilities can use, with every rune
//   under Browse all (one missing a required ability says so; it can still be bound after a warning naming what it
//   lacks), each with a Preview of its build that binds nothing.
// - Two runes per specialization, each with its own three modifiers: the first from level 1 (titan_runes,
//   db-characters), the second from level 30 at full power (titan_runes_second). Each is chosen once: a prestige
//   clears both, or .unbindrune (titan_commands.cpp) lets go of the active spec's (one or both) to bind again.
//   Modifiers change freely out of combat.
// - The rune works through auras: its own while it is the active spec's (90700 Fel Commandment, 90730 the
//   Sanctified Paragon, 90750 the Stormbound Conduit, 90770 Echoing Thunder, 90790 the Wellspring, 90810 the
//   Wandering Mist, 90830 / 90860 / 90890 / 90920 / 90950 the five after, 90980 the Spectral Blade, 91130 the
//   Wild Hunt), and one marker per picked modifier
//   (90710 / 90740 / 90760 / 90780 / 90800 / 90820, and 20 on from each of the five's first, + slot * 3 + choice);
//   scripts look for them.
//
// - Before Wildcard::LOCK_LEVEL (8) a bound rune can be unlearned in the panel (UNBIND), out of combat, and the
//   panel's suggestions count the abilities the Hero's slotted Skill Cards (specs.cpp) will bring. While none of the
//   runes is usable, Learn Available gets a rune's abilities: each one learnable now with a kit pick (or in place of
//   a kit ability the rune does not need), else its Skill Card slotted (in an empty slot, or in place of a card the
//   rune does not need); PLAN asks what it would do (L), APPLY does it.
//
// Protocol (fields by ','; the client sends STATE, PICK,<rune>,<anyway 0/1>,<rune slot 1-2>, MOD,<slot 1-3>,
// <choice 1-3>,<rune slot 1-2>, UNBIND,<rune slot 1-2>, PLAN,<rune>, APPLY,<rune>; anyway 1: warned of the
// abilities the rune needs and lacks, the Hero binds it anyway; no rune slot: the first):
//   S,<level>,<unlock level>,<spec>,<rune>,<m1>,<m2>,<m3>,<offers a.b.c or ->,<prompt 0/1>,<second rune>,<m1>,<m2>,
//     <m3>,<second unlock level>      (offers: up to 4, the panel leaving out the other slot's rune)
//   R,<rune>,<missing>,<missing with cards>,<learn available 0/1>      an implemented rune, the abilities it lacks
//     (groups by '/', one of by '+'; '-' none), those the slotted Skill Cards will not bring either, and whether
//     Learn Available can get them
//   L,<rune>,<learn>,<replaced>,<slot>,<card replaced>      Learn Available's plan: abilities (first ranks, by '.';
//     '-' none) learned now and the kit abilities each replaces (0: a pick), cards slotted and the slot each takes
//     (<slot>:<kind>.<id> replaced, or <slot>:- an empty one)
//   N,<rune>,<slot>,<choice>,<missing>      a modifier with needs of its own
//   E,<text>   O (open the panel)
namespace
{
    constexpr char const* PREFIX = "EFRN";
    constexpr uint8 UNLOCK_LEVEL = 1;               // a rune can be seen and bound from the start
    constexpr uint8 PROMPT_LEVEL = 5;               // the panel opens by itself from here while Rune I is unbound
    constexpr uint8 RUNE_SLOTS = 2;                 // two Legendary Runes a specialization, each with its modifiers
    constexpr uint8 SECOND_UNLOCK_LEVEL = 30;       // the second, at full power
    constexpr uint8 MAX_SPECS = 4;
    constexpr uint8 SLOTS = 3;
    constexpr uint8 CHOICES = 3;                    // a slot's choices, unless its rune says more (RuneDef::Choices)
    constexpr uint8 MAX_CHOICES = 5;                // the Extended Options' (5 a slot, as each rune is extended)
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
    constexpr uint8 RUNE_SPECTRAL_BLADE = 32;      // its doc: Phantom Edge
    constexpr uint8 RUNE_WILD_HUNT = 33;
    constexpr uint8 RUNE_ARCANE_MIND = 34;          // its own dossier, and the four after it
    constexpr uint8 RUNE_SPLINTERED_CROWN = 35;
    constexpr uint8 RUNE_PHOENIX_WAKE = 36;
    constexpr uint8 RUNE_ASTRAL_HORIZON = 37;
    constexpr uint8 RUNE_GUIDING_LIGHT = 38;
    constexpr uint8 RUNE_ELUNES_GUARDIAN = 39;      // its own dossier, and the two after it
    constexpr uint8 RUNE_ASTRAL_CONVERGENCE = 40;
    constexpr uint8 RUNE_URSOCS_CHAMPION = 41;
    constexpr uint8 RUNE_DUELIST = 42;              // its own dossier, and the Stormreaver's
    constexpr uint8 RUNE_STORMREAVER = 43;
    constexpr uint8 RUNE_EMBERS_WRATH = 44;         // its own dossier, and Whitefang's
    constexpr uint8 RUNE_WHITEFANG = 45;
    constexpr uint8 RUNE_RED_COMMUNION = 46;        // the Tank Collection's (src/tank_runes.cpp)
    constexpr uint8 RUNE_PRIMAL_WORLDWARDEN = 47;
    constexpr uint8 RUNE_BLACK_COVENANT = 48;
    constexpr uint8 RUNE_HUNGERING_RIFT = 49;       // the updated compendium's (no earlier draft)
    constexpr uint8 RUNE_HOWLING_NORTH = 50;
    constexpr uint8 RUNE_TRUESHOT = 51;
    constexpr uint8 RUNE_SCARLET_BENEDICTION = 52;  // the updated compendium's
    constexpr uint8 RUNE_EVERGROVE = 53;            // Emberforge's own (Restoration)
    constexpr uint8 RUNE_IRON_RECKONING = 54;       // the updated compendium's

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
        // 90740-90748: its first 9 markers, retired (the Extended Options' block, 93050)
        SPELL_PARAGON_AWAKEN            = 90749,    // its look as it turns on (a shockwave and a flash)

        SPELL_RUNE_STORMBOUND_CONDUIT   = 90750,
        SPELL_STATIC_WARD               = 90751,    // Static Ward's absorb
        SPELL_STORMBOUND_ARC            = 90752,    // a secondary arc's damage
        SPELL_STORMBOUND_ARC_VISUAL     = 90753,    // its bolt, cast by the enemy it leaves
        SPELL_STORMBOUND_RETALIATION    = 90754,    // a Lightning Shield retaliation the rune makes
        SPELL_STORM_MODIFIER_FIRST      = 93225,    // + slot * 5 + choice (90760-90768 retired)
        SPELL_CONDUCTIVE                = 93240,    // on an enemy: the Lightning Bolt hit's damage its amount
        SPELL_STARBOUND_CIRCUIT         = 93241,
        SPELL_SHOCKING_SURGE            = 93242,

        SPELL_RUNE_ECHOING_THUNDER      = 90770,
        SPELL_THUNDERFIELD_MARCH        = 90772,    // the charge: a small storm about the Hero
        SPELL_THUNDER_ECHO              = 90774,
        SPELL_THUNDER_ECHO_BURST        = 90775,    // a burst about the Shield Slam target, its look
        SPELL_SHELTERING_REVERBERATION  = 90776,
        SPELL_DEAFENING_IMPACT          = 90777,
        SPELL_THUNDER_MODIFIER_FIRST    = 93200,    // + slot * 5 + choice (90780-90788 retired)
        SPELL_THUNDERFIELD              = 93215,    // the field laid on the ground (its look)
        SPELL_THUNDER_BURST             = 93216,    // Earth Shock in the field
        SPELL_STORMHEART                = 93217,
        SPELL_FLAME_ECHO                = 93218,    // its tick 93219
        SPELL_FLAME_ECHO_TICK           = 93219,
        SPELL_FROST_ECHO                = 93220,
        SPELL_FROST_ECHO_SLOW           = 93221,
        SPELL_TIDAL_SHIELD              = 93222,

        SPELL_RUNE_WELLSPRING           = 90790,
        SPELL_WELLSPRING_HEAL           = 90792,    // the additional jump
        SPELL_TIDAL_SHELTER             = 90793,
        SPELL_RENEWING_CURRENT          = 90794,
        SPELL_WELLSPRING_JUMP           = 90795,    // the jump's bolt, its look
        SPELL_WELLSPRING_MODIFIER_FIRST = 93175,    // + slot * 5 + choice (90800-90808 retired)
        SPELL_WELLSPRING_MARK           = 93190,    // the Wellspring on the Hero's Earth Shield bearer
        SPELL_FLOWING_WATERS            = 93191,
        SPELL_WIDENING_WATERS           = 93192,
        SPELL_RIPTIDE_SPRING            = 93193,
        SPELL_JADE_CURRENT              = 93194,
        SPELL_SHIELDED_SPRING           = 93195,
        SPELL_RUNE_WANDERING_MIST       = 90810,
        SPELL_WANDERING_MIST            = 90811,
        SPELL_LESSER_ENVELOPING_MIST    = 90812,
        SPELL_ROLLING_MIST              = 90813,
        SPELL_JADE_SHELTER              = 90814,
        SPELL_MIST_MODIFIER_FIRST       = 93125,    // + slot * 5 + choice (90820-90828 retired)
        SPELL_TIDAL_MIST                = 93140,
        SPELL_HOLY_THREAD               = 93141,
        SPELL_EARTHEN_COCOON            = 93142,

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
        SPELL_BATTLEFIELD_CLAIM         = 90866,    // Battlefield Claim's mark: slowed, more damage from the Hero
        SPELL_ONSLAUGHT_MODIFIER_FIRST  = 90880,    // + slot * 3 + choice

        SPELL_RUNE_DAWNBOUND_CRUSADER   = 90890,
        SPELL_DAWN_VOW                  = 90891,    // the vow (taught in Beacon of Light's place)
        SPELL_DAWN_VOWED                = 90892,    // on the vowed ally
        SPELL_DAWN_ECHO_HEAL            = 90894,
        SPELL_DAWN_ECHO_DAMAGE          = 90895,
        SPELL_DAWN_BOLT                 = 90896,    // its look: Holy Shock's, harmful
        SPELL_DAWN_BOLT_HEAL            = 90897,    // and helpful
        SPELL_SUNBURST                  = 90898,    // Sunburst's look
        SPELL_SACRED_GROUND             = 90899,
        SPELL_RADIANT_WARD              = 90900,
        SPELL_DAWN_MODIFIER_FIRST       = 93275,    // + slot * 5 + choice (90910-90918 retired)
        SPELL_DAWN_MARK                 = 93290,    // Judgment's mark
        SPELL_CRUSADERS_FLAME           = 93291,
        SPELL_FROSTFIRE_DAWN            = 93292,
        SPELL_DAWN_AFTERIMAGE           = 93293,    // Steadfast, on the vowed ally
        SPELL_BEACON_OF_LIGHT           = 53563,

        SPELL_RUNE_HOLLOW_CONGREGATION  = 90920,
        SPELL_HOLLOW_ECHO               = 90921,    // on the echo: its look, no threat
        SPELL_HOLLOW_FLAY               = 90922,
        SPELL_HOLLOW_BOLT               = 90923,
        SPELL_HOLLOW_SCREAM             = 90924,
        SPELL_HOLLOW_LAST_WORD          = 90925,
        SPELL_HOLLOW_MEND               = 90926,    // Renewed Witness's heal (Shadow Mend's, retired)
        // 90940-90948: its first 9 markers, retired (the Extended Options' block, below)

        SPELL_RUNE_SHATTERED_SUN        = 90950,
        SPELL_SUNSHARD                  = 90951,
        SPELL_FROZEN_CORE               = 90952,    // Frozen Core's slow
        SPELL_FRACTURE                  = 90953,
        SPELL_FRACTURE_LOOK             = 90954,    // a ring of ice about where it breaks
        SPELL_COLLAPSE                  = 90955,
        SPELL_COLLAPSE_LOOK             = 90956,
        SPELL_SCALDING_GROUND           = 90957,
        SPELL_SUN_MODIFIER_FIRST        = 93250,    // + slot * 5 + choice (90970-90978 retired)
        SPELL_BROKEN_CORE               = 93265,
        SPELL_LUNAR_SHARD               = 93266,
        SPELL_STORM_SHARD               = 93267,
        SPELL_CORE_LANCE                = 93268,    // Ice Lance on a Broken Core
        SPELL_SHATTERING_LANCE          = 93269,    // the next Fireball's cast 15% quicker
        SPELL_ICY_SHELTER               = 93270,
        SPELL_SOLAR_ANCHOR              = 93271,    // the anchored shards' ground
        SPELL_ICE_BARRIER               = 11426,    // first rank

        SPELL_RUNE_SPECTRAL_BLADE       = 90980,
        SPELL_PHANTOM_STRIKE            = 90981,    // cannot crit
        SPELL_OPEN_WOUND                = 90983,
        SPELL_OPEN_WOUND_TICK           = 90984,
        SPELL_VANISHING_ACT             = 90985,
        SPELL_MARKED_FOR_MURDER         = 90986,
        SPELL_PHANTOM_RUSH              = 90987,    // Energy regenerating twice as fast, 4 sec
        SPELL_PHANTOM_FORM              = 90988,    // on the phantom: Shadow Dance's wisps, a shadow at its feet
        SPELL_PHANTOM_VANISH            = 90989,    // the phantom arriving and leaving: Vanish's puff
        // 91120-91128: its first 9 markers, retired (the Extended Options' block, below)

        SPELL_RUNE_WILD_HUNT            = 91130,
        SPELL_SPECTRAL_BEAST            = 91131,    // on the beast: its look, no threat
        SPELL_SPECTRAL_BITE             = 91132,
        SPELL_GHOSTFANG                 = 91133,
        SPELL_GHOSTFANG_TICK            = 91134,
        SPELL_PARTING_HOWL              = 91135,    // on the Hero's pet
        SPELL_FINAL_POUNCE              = 91136,
        SPELL_HUNT_MODIFIER_FIRST       = 93150,    // + slot * 5 + choice (91150-91158 retired)
        SPELL_FROSTFANG                 = 93165,
        SPELL_STORM_HUNT                = 93166,
        SPELL_HEALING_COMPANION         = 93167,
        SPELL_MEND_PET                  = 136,      // first rank

        SPELL_RUNE_ARCANE_MIND          = 91200,
        SPELL_ARCANE_INSIGHT            = 91201,
        SPELL_INSIGHT_MISSILE           = 91202,
        SPELL_ARCANE_ECHO               = 91203,
        SPELL_ARCANE_ECHO_TICK          = 91204,
        SPELL_MIND_UNBOUND              = 91205,    // Insight 4 or more: its look
        SPELL_MENTAL_RESERVES           = 91206,
        SPELL_UNBOUND_MISSILE           = 91207,    // Unbound Mind: an Arcane Missile without its channel
        // 91210-91218: its first 9 markers, retired (the Extended Options' block, below)

        SPELL_RUNE_SPLINTERED_CROWN     = 91220,
        SPELL_ICICLE                    = 91221,
        SPELL_FROST_CROWN               = 91222,    // on the Hero: a stack an Icicle waiting
        // 91223: Splinter Volley's quickening, retired
        SPELL_FROST_WOUND               = 91224,
        SPELL_FROST_WOUND_TICK          = 91225,
        SPELL_PERMAFROST                = 91226,    // Permafrost's slow (Winter's Grasp's, retired)
        SPELL_COLD_FRONT                = 91227,
        SPELL_ICE_SPLINTER              = 91228,    // a fragment, a pierce, a seeking shard
        // 91230-91238: its first 9 markers, retired (the Extended Options' block, below)

        SPELL_RUNE_PHOENIX_WAKE         = 91240,
        SPELL_PHOENIX_EMBER             = 91241,
        SPELL_PHOENIX_FLIGHT            = 91242,
        SPELL_PHOENIX_FORM              = 91243,    // on the phoenix: no threat
        SPELL_SCORCHED_EARTH            = 91244,
        SPELL_SCORCHED_EARTH_TICK       = 91245,
        SPELL_CLINGING_EMBERS           = 91246,
        SPELL_CLINGING_EMBERS_TICK      = 91247,
        SPELL_REBIRTH                   = 91248,
        SPELL_ASHEN_FLOCK               = 91249,
        // 91250-91258: its first 9 markers, retired (the Extended Options' block, below)
        SPELL_REBIRTH_LOOK              = 91259,

        SPELL_RUNE_ASTRAL_HORIZON       = 91260,
        SPELL_ORB_FORM                  = 91261,    // on the Orb: no threat
        SPELL_ORB_BOLT                  = 91262,
        SPELL_WANDERING_STAR            = 91263,
        SPELL_PRISMATIC_LANCE           = 91264,
        SPELL_SUPERNOVA                 = 91265,
        SPELL_ASTRAL_REMNANT            = 91266,
        SPELL_ASTRAL_REMNANT_TICK       = 91267,
        SPELL_ORB_SATELLITE             = 91268,    // Satellite: the Orb circling the Hero (its look)
        SPELL_SUPERNOVA_LOOK            = 91269,
        // 91270-91278: its first 9 markers, retired (the Extended Options' block, below)

        SPELL_RUNE_GUIDING_LIGHT        = 91280,
        SPELL_GUIDING_STAR              = 91281,
        SPELL_STAR_PULSE                = 91282,
        SPELL_STAR_ARRIVAL              = 91283,
        SPELL_STAR_FLIGHT               = 91284,    // its look on the way, from one bearer to the next
        SPELL_SHELTERING_LIGHT          = 91285,
        SPELL_GENTLE_PASSAGE            = 91286,
        SPELL_AFTERGLOW                 = 91287,
        SPELL_PARTING_GRACE             = 91288,
        SPELL_PARTING_GRACE_SPEED       = 91289,
        // 91290-91298: its first 9 markers, retired (the Extended Options' block, below)

        SPELL_RUNE_ELUNES_GUARDIAN      = 91300,
        SPELL_OWL_DIVE                  = 91301,
        SPELL_LUNAR_WOUND               = 91302,
        SPELL_LUNAR_WOUND_TICK          = 91303,
        SPELL_SWIFT_JUDGMENT            = 91304,    // the next Starfire 10% faster
        SPELL_WATCHFUL_FLIGHT           = 91305,    // the next Starfire 20% cheaper
        SPELL_GUIDING_WINGS             = 91306,    // the next Starfire instant
        SPELL_GUIDING_WINGS_SPEED       = 91307,
        SPELL_TWILIGHT_HUNT             = 91308,    // on the owl's victim: the next Wrath 25% more
        // 91310-91318: its first 9 markers, retired (the Extended Options' block, below)

        SPELL_RUNE_ASTRAL_CONVERGENCE   = 91320,
        SPELL_SUNLIGHT                  = 91321,
        SPELL_MOONLIGHT                 = 91322,
        SPELL_LUNAR_COLLISION           = 91323,
        SPELL_SOLAR_FLARE               = 91324,
        SPELL_ASTRAL_RUPTURE            = 91325,
        SPELL_ASTRAL_RUPTURE_TICK       = 91326,
        SPELL_STEADY_ORBIT              = 91327,    // the next Wrath or Starfire instant
        SPELL_LUNAR_DOMINANCE           = 91328,    // the next Starfire 20% faster and cheaper
        SPELL_EVENTIDE                  = 91329,
        SPELL_EVENTIDE_TICK             = 91339,    // (91330-91338: its first 9 markers, retired)

        SPELL_RUNE_URSOCS_CHAMPION      = 91340,
        SPELL_URSOCS_GUARD              = 91341,
        SPELL_URSOC_BLOW                = 91342,
        SPELL_URSOC_BLOW_LOOK           = 91343,
        SPELL_LASTING_STAND             = 91344,
        SPELL_MOVING_MOUNTAIN           = 91345,
        SPELL_ENDURING_CHAMPION         = 91346,
        SPELL_CHALLENGERS_ROAR          = 91347,
        // 91350-91358: its first 9 markers, retired (the Extended Options' block, below)
        SPELL_URSOC_ROAR_LOOK           = 91359,
        SPELL_COLLISION_LOOK            = 91360,    // Astral Rupture: the collision's look, its damage over time

        // The Extended Options (91600 on: 15 markers a rune, + slot * 5 + choice; then its new spells; Astral
        // Convergence 20 ids, the others 25).
        SPELL_CONVERGENCE_MODIFIER_FIRST = 91600,
        SPELL_FLAME_ECLIPSE             = 91615,    // Flame Eclipse's Fire damage
        SPELL_FROZEN_ECLIPSE            = 91616,    // the next Lunar Collision 25% more
        SPELL_INSECT_RENEWAL            = 91617,    // the next Wrath 15% faster
        SPELL_ASTRAL_WARD               = 91618,    // the absorb
        SPELL_PHOENIX_MODIFIER_FIRST    = 91620,
        SPELL_PHOENIX_BURN              = 91635,    // its tick 91636
        SPELL_PHOENIX_BURN_TICK         = 91636,
        SPELL_PHOENIX_ERUPTION          = 91637,    // crossing the Hero's Flame Shock target
        SPELL_FLAMEKEEPER               = 91638,
        SPELL_BLAZING_PATH              = 91639,
        SPELL_HOT_PURSUIT               = 91640,    // the next phoenix's host hit 25% more
        SPELL_EMBERGUARD                = 91641,    // the absorb
        SPELL_HORIZON_MODIFIER_FIRST    = 91645,
        SPELL_ORB_LAUNCH                = 91660,    // the Orb's hit as it is set loose
        SPELL_ORB_PULSE                 = 91661,    // Arcane Explosion near it
        SPELL_ORBITAL_LIGHTNING         = 91662,
        SPELL_ASTRAL_SHELTER            = 91663,    // the absorb
        SPELL_MIND_MODIFIER_FIRST       = 91670,
        SPELL_MIND_FLOW                 = 91685,    // after an empowered channel: the next Arcane Blast instant
        SPELL_STORM_CONDUIT_CHARGE      = 91686,    // the next Insight volley 20% more
        SPELL_STARFALL_EQUATION         = 91687,    // the next Arcane Blast 10% more
        SPELL_ARCANE_CHARGE             = 91688,
        SPELL_SHOCKING_CLARITY          = 91689,
        SPELL_GUIDING_MODIFIER_FIRST    = 91695,
        SPELL_GUIDING_TRAIL             = 91710,    // the heal over time the star leaves on a Renew target
        SPELL_GUIDING_WARD              = 91711,    // Greater Heal's redirect: the absorb
        SPELL_BLADE_MODIFIER_FIRST      = 91720,
        SPELL_PHANTOM_WOUND             = 91735,    // its tick 91736
        SPELL_PHANTOM_WOUND_TICK        = 91736,
        SPELL_FROST_PHANTOM             = 91737,
        SPELL_FLAME_PHANTOM             = 91738,
        SPELL_CORRUPTED_BLADE           = 91739,    // its tick 91740
        SPELL_CORRUPTED_BLADE_TICK      = 91740,
        SPELL_RESTORING_PHANTOM         = 91741,
        SPELL_PHANTOM_OPENING           = 91742,    // the next Sinister Strike 10 Energy cheaper
        SPELL_FATAL_OPENING             = 91743,    // the next builder 10 Energy cheaper
        SPELL_BLEEDING_FOLLOW           = 91744,    // the next Sinister Strike 20% more
        SPELL_OWL_MODIFIER_FIRST        = 91745,
        SPELL_MOONLIT_PREY              = 91760,
        SPELL_OWL_RETURN                = 91761,
        SPELL_FROSTFEATHER              = 91762,    // the dive's Frost half, and the burst
        SPELL_MOONFROST                 = 91763,
        SPELL_EXPOSED_PREY              = 91764,
        SPELL_HOLLOW_MODIFIER_FIRST     = 91770,
        SPELL_CROWN_MODIFIER_FIRST      = 91795,
        SPELL_CROWN_FROSTBITE           = 91810,
        SPELL_COLD_SNAP                 = 91811,    // the next Frostbolt 15% faster
        SPELL_GATHERING_BLIZZARD        = 91812,    // the next Ice Lance 5% more a stack
        SPELL_DUELIST_MODIFIER_FIRST    = 91820,
        SPELL_LOADED_STEEL              = 91835,    // the next Sabre Slash 15% more and a second enemy
        SPELL_LOADED_POWDER             = 91836,    // the next Pistol Shot explodes
        SPELL_POWDER_BLAST              = 91837,
        SPELL_STORM_PISTOL              = 91838,
        SPELL_MARKED_SHOT               = 91839,    // the next Sabre Slash 25% more
        SPELL_FROZEN_POWDER             = 91840,
        SPELL_CUT_AND_RUN               = 91841,
        SPELL_HEALING_POWDER            = 91842,
        SPELL_ARCANE_POWDER             = 91843,    // the next Pistol Shot 25% more
        SPELL_DEAD_AIM                  = 91844,    // Eviscerate's: the next Pistol Shot more (a stack a 5%)
        SPELL_EMBERS_MODIFIER_FIRST     = 91845,
        SPELL_SEARING_SHRED             = 91860,
        SPELL_FROZEN_EMBERS             = 91861,
        SPELL_PHOENIX_CLAWS             = 91862,
        SPELL_PHOENIX_SCAR              = 91863,
        SPELL_HEALING_FLAME             = 91864,
        SPELL_WHITEFANG_MODIFIER_FIRST  = 91870,
        SPELL_STORMFANG                 = 91885,
        SPELL_FLAMEFANG                 = 91886,
        SPELL_LINGERING_FROST           = 91887,    // the next Shred 15% more
        SPELL_CLAWING_COLD              = 91888,    // the next Shred 10 Energy cheaper
        SPELL_MENDING_FANG              = 91889,
        SPELL_URSOC_MODIFIER_FIRST      = 91895,
        SPELL_URSOCS_MARK               = 91910,
        SPELL_RENDING_MAUL              = 91911,    // its tick 91912
        SPELL_RENDING_MAUL_TICK         = 91912,
        SPELL_EARTHEN_FURY              = 91913,    // Earth Shock's: the next impact
        SPELL_THUNDERBEAR               = 91914,
        SPELL_BEARS_MOMENTUM            = 91915,    // the next Maul 20% more
        SPELL_URSOC_MEND                = 91916,
        SPELL_SHIELDING_ROOTS           = 91917,
        SPELL_URSOCS_CHALLENGE          = 91918,
        SPELL_URSOC_NATURE              = 91919,    // the impact's Nature half
        // The Extended Options continue at 93000 (91920 on is full): 25 ids a rune, Red Communion 93000 and the
        // Primal Worldwarden 93025 (tank_runes.cpp), the Sanctified Paragon 93050, the Stormreaver 93075.
        SPELL_PARAGON_MODIFIER_FIRST    = 93050,
        SPELL_PARAGON_JUDGMENT          = 93065,    // Judgment in the aura: a share of its tick
        SPELL_PARAGON_MEND              = 93066,    // Holy Shock's second heal, Healing Light's Flash Heal
        SPELL_DAWNWARD                  = 93067,
        SPELL_RIGHTEOUS_WARD            = 93068,    // Shield of Righteousness in the aura: the absorb
        SPELL_STORMREAVER_MODIFIER_FIRST = 93075,
        SPELL_STORM_SURGE               = 93090,    // Earth Shock's: the next pulse 20% more
        SPELL_FROZEN_FRONT              = 93091,    // its look; its tick 93092
        SPELL_FROZEN_FRONT_TICK         = 93092,
        SPELL_STORMHAMMER               = 93093,
        SPELL_WARSTORM                  = 93094,
        SPELL_STATIC_STRIKE             = 93095,    // the next discharge strikes twice
        SPELL_BURNING_RAKE              = 91865,    // the next Burning Claws 30% more
        SPELL_HOLLOW_PATCH              = 91785,    // its tick 91786
        SPELL_HOLLOW_PATCH_TICK         = 91786,
        SPELL_MOONLIT_CONGREGATION      = 91787,
        SPELL_DEVOURING_COMMAND         = 91788,

        // The Duelist (91370 on: its taught abilities first) and the Stormreaver (91400 on); their markers 10 on
        // from each rune.
        SPELL_SABRE_SLASH               = 91370,    // Sinister Strike while the Duelist is bound
        SPELL_BROADSIDE                 = 91371,    // the finisher it teaches
        SPELL_BROADSIDE_BLAST           = 91372,
        SPELL_BROADSIDE_LOOK            = 91373,    // the last blast's great explosion, on its target
        SPELL_RUNE_DUELIST              = 91380,
        SPELL_OPENING_SHOT              = 91381,    // Pistol Shot ready (8 sec)
        SPELL_PISTOL_SHOT            = 91382,    // the action the rune teaches
        SPELL_PISTOL_DAMAGE          = 91383,
        SPELL_SERRATED_EDGE             = 91384,    // its bleed; 91385 the tick
        SPELL_SERRATED_EDGE_TICK        = 91385,
        // 91386: Rapid Shots, retired (91387-91389 too)
        // 91390-91398: its first 9 markers, retired (the Extended Options' block, below)
        SPELL_SABRE_STRIKE              = 91399,    // Sweeping Steel's and Press the Advantage's hits
        SPELL_RUNE_STORMREAVER          = 91400,
        SPELL_STORMFRONT                = 91401,
        SPELL_STORMFRONT_PULSE          = 91402,
        SPELL_THUNDERLASH               = 91403,
        SPELL_THUNDERLASH_ARC           = 91404,
        SPELL_WHITE_HOT_BURN            = 91405,    // its tick 91406
        SPELL_WHITE_HOT_TICK            = 91406,
        SPELL_STORM_DISCHARGE           = 91407,
        SPELL_RIDE_THE_LIGHTNING        = 91408,
        // 91410-91418: its first 9 markers, retired (the Extended Options' block, 93075)
        SPELL_STORMSTRIKE               = 17364,
        SPELL_LAVA_LASH                 = 60103,

        // Ember's Wrath (91420 on) and Whitefang (91440 on); their markers 11 on from each first. 91420 and 91440,
        // the Cat Form copies of Fire Blast and Icy Touch they first taught, are retired: the Hero's own spells work
        // in Cat Form now, at their own costs.
        SPELL_CAT_FIRE_BLAST            = 91420,
        SPELL_CINDER_WOUND              = 91421,    // its tick 91422, every 2 sec
        SPELL_CINDER_WOUND_TICK         = 91422,
        SPELL_CINDER_WOUND_SPREAD       = 91423,    // Cinder Swipe's: half a wound, never spread again
        SPELL_WILD_EMBERS               = 91424,
        SPELL_BLAZING_RAKE              = 91425,
        SPELL_MOLTEN_BITE               = 91426,
        SPELL_FLAMEBOUND                = 91427,    // speed
        SPELL_FLAMEBOUND_DISCOUNT       = 91428,    // the next Rake or Mangle 10 Energy back
        SPELL_RUNE_EMBERS_WRATH         = 91430,
        // 91431-91439: its first 9 markers, retired (the Extended Options' block, below)
        SPELL_CAT_ICY_TOUCH             = 91440,
        SPELL_FROZEN_PREY               = 91441,    // the primary mark
        SPELL_FROZEN_PREY_PACK          = 91442,    // Pack Frost's: 60% and never spread again
        SPELL_FANG_SHATTER              = 91443,
        SPELL_WINTER_WOUND              = 91444,    // its tick 91445
        SPELL_WINTER_WOUND_TICK         = 91445,
        SPELL_PURSUING_CHILL            = 91446,    // speed
        SPELL_SNOWBLIND_SLOW            = 91447,
        SPELL_SNOWBLIND                 = 91448,    // dodge
        SPELL_FROZEN_TRAIL              = 91449,    // the patch's look
        SPELL_RUNE_WHITEFANG            = 91450,

        // The Hungering Rift (93300 on: the rune, its 15 markers, its spells).
        SPELL_RUNE_HUNGERING_RIFT       = 93300,
        SPELL_RIFT_MODIFIER_FIRST       = 93301,    // + slot * 5 + choice
        SPELL_HUNGERING_RIFT            = 93316,    // on its host
        SPELL_RIFT_LASH                 = 93317,
        SPELL_LINGERING_TORMENT         = 93318,    // its tick 93319
        SPELL_LINGERING_TORMENT_TICK    = 93319,
        SPELL_LAST_GASP                 = 93320,
        SPELL_HELLMAW                   = 93321,
        SPELL_DOOM_ECHO                 = 93322,
        SPELL_SIPHON_CONDUIT            = 93323,
        // Howling North (93325 on).
        SPELL_RUNE_HOWLING_NORTH        = 93325,
        SPELL_NORTH_MODIFIER_FIRST      = 93326,
        SPELL_NORTHWIND                 = 93341,    // the mark
        SPELL_FROST_BLADE               = 93342,
        SPELL_NORTH_FOLLOW_UP           = 93343,    // Frost Strike's added Frost
        SPELL_NORTH_FIELD               = 93344,    // the field's ground
        SPELL_NORTH_FIELD_TICK          = 93345,
        SPELL_FROZEN_GROUND             = 93346,    // its slow
        SPELL_SHATTERING_COLD           = 93347,
        SPELL_HORN_OF_THE_NORTH         = 93348,
        SPELL_DECAYING_ICE              = 93349,    // its tick 93350
        SPELL_DECAYING_ICE_TICK         = 93350,
        SPELL_ICY_WOUND                 = 93351,    // its tick 93352
        SPELL_ICY_WOUND_TICK            = 93352,
        SPELL_NORTHERN_ENDURANCE        = 93353,
        SPELL_WINTERS_MENDING           = 93354,    // unused: Death Strike's own heal grows
        // Trueshot (93360 on).
        SPELL_RUNE_TRUESHOT             = 93360,
        SPELL_TRUESHOT_MODIFIER_FIRST   = 93361,
        SPELL_TRUESHOT                  = 93376,    // in Aimed Shot's place, 2 sec
        SPELL_TRUESHOT_STEADY           = 93377,    // Steady Aim's, 2.5 sec
        SPELL_EXPOSED_TARGET            = 93378,
        SPELL_TRUESHOT_PIERCE           = 93379,
        SPELL_ARCANE_PAYLOAD            = 93380,
        SPELL_VENOM_ARROW               = 93381,
        SPELL_CONCUSSIVE_PIERCE         = 93382,
        SPELL_HUNTERS_MOBILITY          = 93383,
        SPELL_STEADY_RECOVERY           = 93384,
        SPELL_MENDING_AIM               = 93385,    // the pet's heal
        // Scarlet Benediction (93400 on).
        SPELL_RUNE_SCARLET_BENEDICTION  = 93400,
        SPELL_SCARLET_MODIFIER_FIRST    = 93401,
        SPELL_SCARLET_BLOOM             = 93416,    // dormant on Renew's target
        SPELL_SCARLET_OPENING           = 93417,    // the heal
        SPELL_SCARLET_PETALS            = 93418,    // the absorb
        SPELL_BLOODROSE                 = 93419,    // heal over time
        // The Evergrove (93425 on).
        SPELL_RUNE_EVERGROVE            = 93425,
        SPELL_GROVE_MODIFIER_FIRST      = 93426,
        SPELL_SECOND_REJUVENATION       = 93441,    // then Third, Fourth
        SPELL_GROVE_SHELTER             = 93444,
        SPELL_OVERGROWTH                = 93445,
        // Iron Reckoning (93450 on).
        SPELL_RUNE_IRON_RECKONING       = 93450,
        SPELL_IRON_MODIFIER_FIRST       = 93451,
        SPELL_IRON_BULWARK              = 93466,
        SPELL_IRON_IMPACT               = 93467,
        SPELL_RENDING_IMPACT            = 93468,    // its tick 93469
        SPELL_RENDING_IMPACT_TICK       = 93469,
        SPELL_SHIELDED_ADVANCE          = 93470,
        SPELL_SHIELDED_ADVANCE_WARD     = 93471,
        SPELL_IRON_MENDING              = 93472,
        SPELL_PRAYER_OF_HEALING         = 596,
        SPELL_BINDING_HEAL              = 32546,
        SPELL_PRAYER_OF_MENDING_BUFF    = 41635,    // its ranks 48110, 48111
        SPELL_HEALING_TOUCH             = 5185,
        SPELL_NOURISH                   = 50464,
        SPELL_TRANQUILITY               = 740,
        SPELL_TRANQUILITY_HEAL          = 44203,
        SPELL_NATURES_SWIFTNESS_DRUID   = 17116,
        SPELL_THORNS                    = 467,
        SPELL_INNERVATE                 = 29166,
        SPELL_LIFEBLOOM_BLOOM           = 33778,
        SPELL_SHIELD_BLOCK              = 2565,
        SPELL_DEVASTATE                 = 20243,
        SPELL_CONCUSSION_BLOW           = 12809,
        SPELL_LAST_STAND                = 12975,
        SPELL_LAST_STAND_AURA           = 12976,
        SPELL_SHIELD_WALL               = 871,
        SPELL_SHOCKWAVE                 = 46968,
        SPELL_CIRCLE_OF_HEALING         = 34861,
        SPELL_GUARDIAN_SPIRIT           = 47788,
        SPELL_REGROWTH                  = 8936,
        SPELL_SWIFTMEND                 = 18562,
        SPELL_WILD_GROWTH               = 48438,
        SPELL_IMMOLATE                  = 348,
        SPELL_HAUNT                     = 48181,
        SPELL_CHAINS_OF_ICE             = 45524,
        SPELL_HUNGERING_COLD            = 49203,
        SPELL_HUNGERING_COLD_FREEZE     = 51209,
        SPELL_BLOOD_PLAGUE              = 55078,
        SPELL_HORN_OF_WINTER            = 57330,
        SPELL_DEATH_AND_DECAY           = 43265,
        SPELL_DEATH_STRIKE              = 49998,
        SPELL_DEATH_STRIKE_HEAL         = 45470,
        SPELL_SERPENT_STING             = 1978,
        SPELL_CONCUSSIVE_SHOT           = 5116,
        SPELL_SILENCING_SHOT            = 34490,
        SPELL_CURSE_OF_AGONY            = 980,
        SPELL_HOWLING_BLAST             = 49184,
        SPELL_OBLITERATE                = 49020,
        SPELL_FROST_STRIKE              = 49143,
        SPELL_MULTI_SHOT                = 2643,
        SPELL_CHIMERA_SHOT              = 53209,
        // 91451-91459: its first 9 markers, retired (the Extended Options' block, below)
        SPELL_FROZEN_TRAIL_TICK         = 91460,
        SPELL_FROZEN_TRAIL_SLOW         = 91461,
        SPELL_FANG_COLD_BLOOD           = 91462,    // the next Ferocious Bite on the victim 20% more
        // The shocks' empowered next strike: Flame Shock's Kindled (Claw: Blazing Strike; Swipe: Fiery Explosion),
        // Frost Shock's Frostbitten (Claw: Frostfang Strike; Swipe: Glacial Burst).
        SPELL_KINDLED                   = 91463,
        SPELL_BLAZING_STRIKE            = 91464,
        SPELL_BLAZING_BLEED             = 91465,    // its tick 91466
        SPELL_BLAZING_BLEED_TICK        = 91466,
        SPELL_FIERY_EXPLOSION           = 91467,
        SPELL_FROSTBITTEN               = 91468,
        SPELL_FROSTFANG_STRIKE          = 91469,
        SPELL_FROSTBITE                 = 91470,    // its tick 91471
        SPELL_FROSTBITE_TICK            = 91471,
        SPELL_FROSTBITE_SLOW            = 91472,
        SPELL_GLACIAL_BURST             = 91473,
        SPELL_GLACIAL_SLOW              = 91474,
        SPELL_FIERY_EXPLOSION_LOOK      = 91475,
        SPELL_GLACIAL_BURST_LOOK        = 91476,
        SPELL_FLAME_SHOCK               = 8050,     // first ranks
        SPELL_FROST_SHOCK               = 8056,
        SPELL_CLAW                      = 1082,
        SPELL_CAT_FORM                  = 768,
        SPELL_RAKE                      = 1822,     // first ranks
        SPELL_RIP                       = 1079,
        SPELL_FEROCIOUS_BITE            = 22568,
        SPELL_MANGLE_CAT                = 33876,
        SPELL_SHRED                     = 5221,
        SPELL_SWIPE_CAT                 = 62078,

        SPELL_EVISCERATE            = 2098,         // first ranks
        SPELL_SINISTER_STRIKE       = 1752,
        SPELL_BACKSTAB              = 53,
        SPELL_HEMORRHAGE            = 16511,
        SPELL_KILL_COMMAND          = 34026,
        SPELL_BESTIAL_WRATH         = 19574,
        SPELL_ARCANE_BLAST          = 30451,        // first ranks
        SPELL_ARCANE_MISSILES       = 5143,
        SPELL_ARCANE_EXPLOSION      = 1449,
        SPELL_ARCANE_BARRAGE        = 44425,
        SPELL_PRAYER_OF_MENDING     = 33076,
        SPELL_PRAYER_OF_MENDING_HEAL = 33110,
        SPELL_FLASH_HEAL            = 2061,
        SPELL_GREATER_HEAL          = 2060,
        SPELL_STARFIRE              = 2912,         // first ranks
        SPELL_WRATH                 = 5176,
        SPELL_BARKSKIN              = 22812,
        SPELL_MOONFIRE              = 8921,
        SPELL_INSECT_SWARM          = 5570,
        SPELL_REJUVENATION          = 774,
        SPELL_EVOCATION             = 12051,
        SPELL_FIRE_WARD             = 543,
        SPELL_FLAMESTRIKE           = 2120,
        SPELL_LAVA_BURST            = 51505,
        SPELL_ARCANE_SHOT           = 3044,
        SPELL_RAPID_FIRE            = 3045,
        SPELL_POWER_WORD_SHIELD     = 17,
        SPELL_RENEW                 = 139,
        SPELL_RIPTIDE               = 61295,
        SPELL_HUNTERS_MARK          = 1130,
        SPELL_CORRUPTION            = 172,
        SPELL_REND                  = 772,
        SPELL_FROST_FEVER           = 55095,
        SPELL_AIMED_SHOT            = 19434,
        SPELL_STEADY_SHOT           = 56641,
        SPELL_DRAIN_SOUL            = 1120,
        SPELL_MANGLE_BEAR           = 33878,
        SPELL_SWIPE_BEAR            = 779,
        SPELL_LIFEBLOOM             = 33763,
        SPELL_CHARGE                = 100,
        SPELL_DEATH_COIL_DK         = 47541,
        SPELL_DEATH_COIL_WARLOCK    = 6789,
        SPELL_DIVINE_SHIELD         = 642,
        SPELL_SHIELD_OF_RIGHTEOUSNESS = 53600,
        SPELL_HOLY_SHOCK_HEAL       = 25914,
        SPELL_BLIZZARD              = 10,
        SPELL_MAUL                  = 6807,
        SPELL_BEAR_FORM             = 5487,
        SPELL_DIRE_BEAR_FORM        = 9634,

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
        SPELL_REVENGE               = 6572,
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
        MOD_BARRAGE, MOD_SHATTERED_GLAIVE, MOD_FELGUARDS_CALL,
        MOD_METAMORPHIC_BOND, MOD_IMP_LEGION, MOD_PACT_OF_RUIN,
    };

    // The Sanctified Paragon's modifiers: slot (0 Presence, 1 Manifestation, 2 Covenant) x 3 + choice.
    // (Its Extended Options: slot x 5 + choice, the first three of each the ones it had.)
    enum ParagonModifier : uint8
    {
        MOD_BOUNDLESS_SANCTITY, MOD_HALLOWED_FOOTSTEPS, MOD_SEAT_OF_THE_PARAGON, MOD_CONSECRATED_CHARGE,
        MOD_SANCTUARY_MARCH,
        MOD_SANCTIFIED_CONDEMNATION, MOD_MERCIFUL_GROUND, MOD_SHELTER_OF_THE_PARAGON, MOD_RADIANT_JUDGMENT,
        MOD_HEALING_LIGHT,
        MOD_BASTION_OF_FAITH, MOD_WEIGHT_OF_THE_UNWORTHY, MOD_MEASURED_DEVOTION, MOD_DAWNWARD, MOD_PARAGONS_AEGIS,
    };

    // The Sanctified Paragon's Extended Options. Its interactions: Judgment on an enemy in the aura, 50% of a tick
    // more (6 sec apart; Radiant Judgment 100%); a Holy Shock heal on an injured ally in it, 15% of it to another
    // within 8 yards (8 sec apart); Shield of Righteousness on an enemy in it, an absorb of 3% of maximum health for
    // 4 sec (8 sec apart; Paragon's Aegis 4%). Consecrated Charge: after Charge, 4 yards more for 4 sec and the next
    // tick 15% more (12 sec apart); Sanctuary March: Divine Shield, 3 ticks free (60 sec apart); Healing Light: the
    // lowest ally healed for 45% of a tick a second, a Flash Heal on them 30% more (8 sec apart); Dawnward: Holy
    // Shock's second heal also shields for 20% of it (4 sec).
    constexpr uint32 PARAGON_JUDGMENT_EVERY_MS = 6000, PARAGON_SHOCK_EVERY_MS = 8000, RIGHTEOUS_EVERY_MS = 8000;
    constexpr uint32 CHARGE_EVERY_MS = 12000, PARAGON_CHARGE_MS = 4000, MARCH_EVERY_MS = 60000, LIGHT_EVERY_MS = 8000;
    constexpr uint32 DAWNWARD_EVERY_MS = 8000;
    constexpr uint8 MARCH_TICKS = 3;
    constexpr float PARAGON_JUDGMENT = 0.5f, RADIANT_JUDGMENT = 1.0f, PARAGON_SHOCK_SHARE = 0.15f;
    constexpr float PARAGON_SHOCK_RANGE = 8.0f, RIGHTEOUS_PCT = 3.0f, AEGIS_PCT = 4.0f, CHARGE_RADIUS = 4.0f;
    constexpr float CHARGE_TICK = 1.15f, HEALING_LIGHT_SHARE = 0.75f, LIGHT_FLASH = 0.3f, DAWNWARD_SHARE = 0.2f;

    // The Stormbound Conduit's modifiers: slot (0 Arcs, 1 Lightning Shield, 2 Surge) x 3 + choice.
    enum StormModifier : uint8
    {
        MOD_PERSISTENT_CURRENT, MOD_BRANCHING_CURRENT, MOD_LIVE_WIRE, MOD_ICY_CONDUCTOR, MOD_STORM_FLAME_CONDUCTOR,
        MOD_FORKED_TEMPEST, MOD_CLOSED_CIRCUIT, MOD_FARADAY_FIELD, MOD_STORM_LASH, MOD_STARBOUND_CIRCUIT,
        MOD_STORM_WITHIN, MOD_VIOLENT_REPRISAL, MOD_DISTANT_CONDUCTOR, MOD_STORMS_EYE, MOD_SHOCKING_SURGE,
    };

    // The Stormbound Conduit's tuning (its Extended Options, a redesign): Lightning Bolt (the Hero's own) leaves
    // Conductive on its target for 6 sec, remembering the hit. Chain Lightning hitting a Conductive enemy sends one
    // arc (once a cast) to another enemy within 10 yards for 30% of that Lightning Bolt hit; none: back into it for
    // half. A Lightning Shield retaliation arcs to another Conductive enemy within 10 yards for 50% of it (3 sec
    // apart); Earth Shock on a Conductive enemy arcs 20% of spell power to another (half into it alone; 8 sec apart).
    // An arc never arcs again.
    constexpr float ARC_RANGE = 10.0f, FARADAY_RANGE = 15.0f, CHAIN_ARC_SHARE = 0.3f, ALONE_SHARE = 0.5f;
    constexpr float SHIELD_ARC_SHARE = 0.5f, EARTH_ARC_POWER = 0.2f;
    constexpr int32 CONDUCTIVE_MS = 6000, PERSISTENT_MS = 12000;
    constexpr uint32 CHAIN_ARC_EVERY_MS = 1000, SHIELD_ARC_EVERY_MS = 3000, EARTH_ARC_EVERY_MS = 8000;
    // Its options: Persistent Current (12 sec, an application's first arc 15% more), Branching Current (the bolt
    // also on one enemy within 10 yards), Live Wire (retaliations 15% more, Conductive on the attacker; not with
    // Storm Within), Icy Conductor (Frostbolt leaves it too, 15% more on a Conductive enemy), Flame Conductor
    // (Lightning Bolt on their Flame Shock target: one more enemy, Flame Shock 2 sec longer, 6 sec an application);
    // Forked Tempest (2 enemies, 65% each), Closed Circuit (back into it, whole), Faraday Field (15 yards, the first
    // arc each 8 sec 20% more), Storm Lash (Lava Lash: 25% of attack power, 6 sec apart), Starbound Circuit
    // (Starfire: 25% of spell power as Arcane, 8 sec apart); Storm Within (a retaliation at the nearest Conductive
    // enemy within 8 yards every 3 sec in combat, a charge each), Violent Reprisal (the shield's arcs 40% more),
    // Distant Conductor (Lightning Bolt: a retaliation at its target, a charge, 3 sec apart), Storm's Eye (Chain Heal:
    // the next Lightning Bolt within 6 sec 15% more), Shocking Surge (Thunder Clap on a Conductive enemy: 20% of
    // spell power to up to 5 within 8 yards, 8 sec apart).
    constexpr float PERSISTENT_FIRST = 1.15f, LIVE_WIRE_MORE = 1.15f, ICY_CONDUCTOR = 1.15f, FORKED_SHARE = 0.65f;
    constexpr float FARADAY_MORE = 1.2f, STORM_LASH_AP = 0.25f, STARBOUND_POWER = 0.25f, VIOLENT_REPRISAL = 1.4f;
    constexpr float STORMS_EYE = 1.15f, SHOCKING_POWER = 0.2f, SHOCKING_RADIUS = 8.0f, STORM_WITHIN_RANGE = 8.0f;
    constexpr int32 FLAME_CONDUCTOR_MS = 2000, FLAME_CONDUCTOR_MOST_MS = 6000, STORMS_EYE_MS = 6000;
    constexpr std::size_t FORKED_ARCS = 2, SHOCKING_TARGETS = 5;
    constexpr uint32 FARADAY_EVERY_MS = 8000, STORM_LASH_EVERY_MS = 6000, STARBOUND_EVERY_MS = 8000;
    constexpr uint32 STORM_WITHIN_MS = 3000, DISTANT_EVERY_MS = 3000, SHOCKING_EVERY_MS = 8000;
    constexpr uint32 LIGHTNING_SHIELD_FAMILY_FLAG = 0x400;      // SpellFamilyFlags0

    // Echoing Thunder's modifiers (its Extended Options, a redesign): slot (0 Field, 1 Echo, 2 Resonance) x 5 +
    // choice.
    enum ThunderModifier : uint8
    {
        MOD_LINGERING_THUNDER, MOD_MARCH_OF_THE_STORM, MOD_ROLLING_FRONT, MOD_STORMHEART, MOD_CONSECRATED_STORM,
        MOD_REVERBERATING_IMPACT, MOD_FOCUSED_PEAL, MOD_DISTANT_RUMBLE, MOD_FLAME_ECHO, MOD_FROST_ECHO,
        MOD_COMMANDING_THUNDER, MOD_SHELTERING_REVERBERATION, MOD_DEAFENING_IMPACT, MOD_REVENGE_OF_THE_STORM,
        MOD_TIDAL_SHIELD,
    };

    // Echoing Thunder's tuning: Thunder Clap lays a Thunderfield where the Hero stands for 16 sec (8 yards about).
    // Standing in it, Shield Slam releases a Thunder Echo in a 90 degree cone 10 yards toward its target: 42% of the
    // slam's damage before mitigation, Nature, to up to 5 enemies; 2 sec apart. Revenge in the field: the next echo
    // 20% more (6 sec); Earth Shock on an enemy in it: 20% of attack power as Nature to up to 4 within 6 yards of
    // it, 8 sec apart.
    constexpr int32 THUNDERFIELD_MS = 16000, LINGERING_FIELD_MS = 19000, REVENGE_CHARGE_MS = 6000;
    constexpr float THUNDERFIELD_RADIUS = 8.0f, ECHO_SHARE = 0.42f, ECHO_CONE = 10.0f, ECHO_HALF_ARC = float(M_PI) / 4;
    constexpr std::size_t ECHO_TARGETS = 5, THUNDER_BURST_TARGETS = 4;
    constexpr uint32 ECHO_EVERY_MS = 2000, THUNDER_BURST_EVERY_MS = 8000;
    constexpr float REVENGE_ECHO = 1.2f, THUNDER_BURST_AP = 0.2f, THUNDER_BURST_RADIUS = 6.0f;
    // Its options: Lingering Thunder (9 sec, the field's first echo 15% more), March of the Storm (the field goes
    // with the Hero, 10% more), Rolling Front (laid under their enemy within 10 yards, 20% more), Stormheart (goes
    // with them; with Lightning Shield, 20% of spell power as Nature to the slam's target), Consecrated Storm (in
    // their Consecration too: 20% more, 30% more threat); Reverberating Impact (a 6 yard burst about the target),
    // Focused Peal (the target alone, 60% more), Distant Rumble (15 yards, 30% more beyond 10), Flame Echo (their
    // Flame Shock target: 30% of it over 6 sec), Frost Echo (their Frost Fever target: 20% of attack power as Frost,
    // 25% slower for 3 sec); Commanding Thunder (twice the threat, the target 10% more), Sheltering Reverberation (3%
    // of maximum health an enemy hit, 15% at most, 6 sec), Deafening Impact (30% slower for 3 sec; their Thunder
    // Clap's victims 20% more), Revenge of the Storm (Revenge's echo 40% more, and 10 Rage, 8 sec apart), Tidal
    // Shield (their Earth Shield on them: 30% of its last heal as a 4 sec absorb, 8 sec apart).
    constexpr float LINGERING_ECHO = 1.15f, MARCH_ECHO = 1.1f, ROLLING_FRONT_ECHO = 1.2f, ROLLING_FRONT_RANGE = 10.0f;
    constexpr float STORMHEART_POWER = 0.2f, CONSECRATED_ECHO = 1.2f, CONSECRATED_THREAT = 0.3f;
    constexpr float REVERBERATING_RADIUS = 6.0f, FOCUSED_ECHO = 1.6f, DISTANT_CONE = 15.0f, DISTANT_MORE = 1.3f;
    constexpr float FLAME_ECHO_SHARE = 0.3f, FROST_ECHO_AP = 0.2f, COMMANDING_THREAT = 1.0f, COMMANDING_MORE = 1.1f;
    constexpr float SHELTERING_PCT = 3.0f, SHELTERING_CAP_PCT = 15.0f, DEAFENED_MORE = 1.2f;
    constexpr float STORM_REVENGE_ECHO = 1.4f, TIDAL_SHIELD_SHARE = 0.3f;
    constexpr int32 SHELTERING_MS = 6000, DEAFENING_MS = 3000, FLAME_ECHO_MS = 6000, FROST_ECHO_MS = 3000;
    constexpr int32 STORM_REVENGE_RAGE = 100, TIDAL_SHIELD_MS = 4000;      // Rage in tenths
    constexpr uint32 STORM_REVENGE_EVERY_MS = 8000, TIDAL_SHIELD_EVERY_MS = 8000;

    // The Wellspring's modifiers (its Extended Options, a redesign): slot (0 Spring, 1 Current, 2 Undertow) x 5 +
    // choice.
    enum WellspringModifier : uint8
    {
        MOD_FLOWING_WATERS, MOD_LASTING_SPRING, MOD_WIDENING_WATERS, MOD_RIPTIDE_SPRING, MOD_RENEWING_SPRING,
        MOD_DEEP_CURRENT, MOD_SPLIT_TRIBUTARY, MOD_CONCENTRATED_CURRENT, MOD_TIDAL_SURGE, MOD_GUIDED_CURRENT,
        MOD_TIDAL_SHELTER, MOD_LONG_REACH, MOD_RENEWING_CURRENT, MOD_JADE_CURRENT, MOD_SHIELDED_SPRING,
    };

    // The Wellspring's tuning: Chain Heal (the Hero's own cast) healing their Earth Shield bearer lays a Wellspring
    // on them for 18 sec; a later Chain Heal reaching it releases one more heal, 40% of what the bearer was healed
    // (before overhealing), to the most injured other ally within 12.5 yards (none: 20% to the bearer); once a cast,
    // never the Wellspring it just laid. Chain Heal heals the bearer 20% more. Earth Shield's heal on the bearer
    // renews the Wellspring; a Riptide heal on them makes the next release 20% stronger (8 sec).
    constexpr float WELLSPRING_SHARE = 0.4f, BEARER_SHARE = 0.2f, WEARER_BONUS = 1.2f, WELL_RIPTIDE = 1.2f;
    constexpr float WELL_RANGE = 12.5f, WIDENING_RANGE = 18.0f, LONG_REACH = 20.0f;
    constexpr int32 WELL_MS = 18000, LASTING_WELL_MS = 22000, WELL_CHARGE_MS = 8000;
    // Its options: Flowing Waters (the shield's renewal: 20% of healing power to the most injured within 8 yards, 6
    // sec apart), Lasting Spring (12 sec, 10% more), Widening Waters (18 yards, and 10% of healing power over 6 sec),
    // Riptide Spring (Riptide on the bearer renews it and heals another within 10 yards for 25%, 8 sec apart),
    // Renewing Spring (Renew on the bearer renews it, the next release 20% more); Deep Current (40% more below half
    // health, else 10%), Split Tributary (2 allies, 65% each), Concentrated Current (one, 30% more), Tidal Surge (on
    // their Riptide: 30% more, Riptide 2 sec longer, 6 sec an application), Guided Current (Holy Shock's heal on the
    // bearer: the next release within 8 sec one more ally for 40%); Tidal Shelter (25% as a 4 sec absorb), Long Reach
    // (20 yards, 10% more), Renewing Current (25% over 4 sec), Jade Current (their Soothing Mist there: 25% of a
    // tick), Shielded Spring (their Power Word: Shield there: 15% as a 4 sec absorb).
    constexpr float FLOWING_POWER = 0.2f, FLOWING_RANGE_WELL = 8.0f, LASTING_SPRING = 1.1f, WIDENING_POWER = 0.1f;
    constexpr float RIPTIDE_SPRING_POWER = 0.25f, RIPTIDE_SPRING_RANGE = 10.0f, RENEWING_SPRING = 1.2f;
    constexpr float DEEP_CURRENT = 1.4f, DEEP_CURRENT_ABOVE = 1.1f, DEEP_CURRENT_BELOW_PCT = 50.0f;
    constexpr float SPLIT_SHARE = 0.65f, CONCENTRATED_CURRENT = 1.3f, TIDAL_SURGE = 1.3f, GUIDED_SHARE = 0.4f;
    constexpr float UNDERTOW_SHARE = 0.25f, LONG_REACH_MORE = 1.1f, JADE_CURRENT_SHARE = 0.25f, SHIELDED_SHARE = 0.15f;
    constexpr std::size_t SPLIT_ALLIES = 2;
    constexpr int32 UNDERTOW_TICKS = 4, WIDENING_TICKS = 6, TIDAL_SURGE_MS = 2000, TIDAL_SURGE_MOST_MS = 6000;
    constexpr uint32 FLOWING_EVERY_MS = 6000, RIPTIDE_SPRING_EVERY_MS = 8000, GUIDED_MS = 8000;

    // The Wandering Mist's modifiers: slot (0 Mistflow, 1 Technique, 2 Approach) x 5 + choice (its Extended Options:
    // Jade Shelter moved to the third slot, Lingering Mist and Deep Breath gone).
    enum MistModifier : uint8
    {
        MOD_FLOWING_CURRENTS, MOD_DRIFTING_MIST, MOD_GATHERING_MISTS, MOD_RENEWING_MIST, MOD_TIDAL_MIST,
        MOD_COMFORTING_VEIL, MOD_VIVACIOUS_FLOW, MOD_PEACEFUL_FOCUS, MOD_HOLY_THREAD, MOD_CHAIN_OF_JADE,
        MOD_EFFORTLESS_BREATHING, MOD_ROLLING_MIST, MOD_JADE_SHELTER, MOD_RENEWING_BREATH, MOD_EARTHEN_COCOON,
    };

    // The Wandering Mist's tuning: each Soothing Mist tick also heals the most injured ally within 20 yards of its
    // target for 25% of it.
    constexpr float WANDER_SHARE = 0.25f, DRIFTING_SHARE = 0.15f, WANDER_RANGE = 20.0f, FLOWING_RANGE = 30.0f;
    constexpr float JADE_SHELTER_SHARE = 0.3f, VEIL_SHARE = 0.3f, VIVACIOUS_SHARE = 0.3f, SECONDARY_RANGE = 20.0f;
    constexpr float PEACEFUL_PCT = 15.0f, EFFORTLESS_SHARE = 0.25f;
    constexpr int32 SOOTHING_MIST_TICKS = 8, ROLLING_TICKS = 2;
    // Its interaction and Extended Options: a Mist on the Hero's Renew target 15% more; Gathering Mists (below half
    // health: 25% more), Renewing Mist (their Renew 2 sec longer, 6 sec an application, 6 sec apart), Tidal Mist
    // (their Riptide target: 40% of its healing to another injured ally within 8 yards, 6 sec apart); Holy Thread
    // (Holy Shock's heal on the channel target: 25% to the Mist's ally, 8 sec apart), Chain of Jade (Chain Heal's
    // first: a Mist of 30% of it, 8 sec apart); Rolling Mist (a switch leaves two ticks over 4 sec on the last
    // target, 12 sec apart), Jade Shelter (30% of a Mist as a 4 sec absorb, 8 sec apart), Renewing Breath (the
    // channel's third tick: their Renew there 2 sec longer), Earthen Cocoon (their Earth Shield's bearer: 25% as an
    // absorb, 8 sec apart).
    constexpr float MIST_RENEW = 1.15f, GATHERING_MISTS = 1.25f, GATHERING_BELOW_PCT = 50.0f, TIDAL_MIST_SHARE = 0.4f;
    constexpr float TIDAL_MIST_RANGE = 8.0f, HOLY_THREAD_SHARE = 0.25f, CHAIN_OF_JADE_SHARE = 0.3f;
    constexpr float EARTHEN_COCOON_SHARE = 0.25f;
    constexpr int32 MIST_RENEW_MS = 2000, MIST_RENEW_MOST_MS = 6000, MIST_WATCH_MS = 4000, ROLLING_SWITCH_MS = 1000;
    constexpr uint32 RENEWING_MIST_EVERY_MS = 6000, TIDAL_MIST_EVERY_MS = 6000, HOLY_THREAD_EVERY_MS = 8000;
    constexpr uint32 CHAIN_OF_JADE_EVERY_MS = 8000, ROLLING_EVERY_MS = 12000, JADE_SHELTER_EVERY_MS = 8000;
    constexpr uint32 EARTHEN_COCOON_EVERY_MS = 8000, RENEWING_BREATH_TICK = 3;

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

    // Unbroken Onslaught's tuning: the wake's attack is 96% of a normalized main-hand swing to each enemy along it
    // (up to 6), its primary target 25% more (50% with Executioner's Cut). Overpowering Echo: the wake rips open
    // twice (the next two melee abilities). Battlefield Claim: enemies it hits are marked 8 sec (slowed 30%, 10% more
    // damage from the Hero: the mark's own spell data).
    constexpr uint32 WAKE_MS = 6000, WAKE_DELAY_MS = 600, AFTERSHOCK_MS = 1500, RIP_GAP_MS = 300;
    constexpr uint8 ECHO_RIPS = 2;
    constexpr float WAKE_SHARE = 0.96f, WAKE_PRIMARY = 1.25f, EXECUTIONER_PRIMARY = 1.5f, AFTERSHOCK_SHARE = 0.5f;
    constexpr float WAKE_HALF_LENGTH = 4.0f, WAKE_HALF_WIDTH = 1.5f, CUT_HALF_WIDTH = 1.0f;
    constexpr float CRESCENT_RADIUS = 8.0f, CRESCENT_ARC = float(M_PI) / 2.0f;     // each side of the facing
    constexpr float RAKE_BEHIND = 2.0f, RAKE_BEYOND = 14.0f;
    constexpr float BLEED_SHARE = 0.3f;                         // of the wake's hit, every second
    constexpr int32 BLEED_TICKS = 4;
    constexpr std::size_t WAKE_TARGETS = 6;
    constexpr float SECOND_WIND_PCT = 1.0f;                     // of maximum health, an enemy the wake hits

    // The Dawnbound Crusader's modifiers: slot (0 Echo, 1 Light, 2 Grace) x 3 + choice.
    enum DawnModifier : uint8
    {
        MOD_MERCY, MOD_RETRIBUTION, MOD_COMMUNION, MOD_EARTHEN_OATH, MOD_CRUSADERS_FLAME,
        MOD_DAWNWAVE, MOD_SUNBURST, MOD_SACRED_GROUND, MOD_FROSTFIRE_DAWN, MOD_CHAIN_OF_GRACE,
        MOD_STEADFAST, MOD_PASSING_THE_TORCH, MOD_SHARED_PILGRIMAGE, MOD_RENEWED_OATH, MOD_SEARING_VOW,
    };

    // The Dawnbound Crusader's tuning (its Extended Options, a redesign): Dawn Vow (in Beacon of Light's place; 6
    // sec) vows the Hero or an ally within 40 yards (none: the Hero). Judgment marks its enemy for 10 sec. Crusader
    // Strike on the marked enemy, or Holy Shock, sends a Dawn Echo from the vowed ally (3 sec apart): it heals the
    // most injured ally within 10 yards of them for 35% of the Hero's power (the higher of their Holy spell power
    // and half their attack power), unless a Doctrine (the first slot) says otherwise. An echo never echoes.
    constexpr float DAWN_POWER = 0.35f, DAWN_REACH = 10.0f, DAWN_STRIKE_REACH = 20.0f;
    constexpr int32 DAWN_MARK_MS = 10000;
    constexpr uint32 DAWN_EVERY_MS = 3000;
    // Its options: Mercy (heals 50%), Retribution (strikes the marked enemy within 20 yards of the vow for 45%),
    // Communion (by turns a 35% heal and a 30% strike), Earthen Oath (their Earth Shield on the vow: heals 40% and
    // shields the vow for 10%, 4 sec), Crusader's Flame (strikes the marked enemy for 35%; their Flame Shock on it,
    // 15% as Fire too); Dawnwave (from the vow toward the marked enemy: up to 3 on the way, half each), Sunburst (2
    // more within 8 yards of the vow, 40%), Sacred Ground (4 yards at the vow, 4 sec: one injured ally healed for 10%
    // a second), Frostfire Dawn (Frostbolt on the marked enemy: the next strike 20% as Frost, 6 sec), Chain of Grace
    // (Chain Heal on the vow: the next heal one more ally for 40%, 6 sec); Steadfast (Holy Shock: the vow's
    // afterimage, 6 sec: their next echo again at 30%), Passing the Torch (a new vow healed for 30%), Shared
    // Pilgrimage (the Hero or the vow 8 yards on: the next echo one more for 40%), Renewed Oath (an echo with their
    // Renew on the vow: 2 sec longer, 6 sec an application, 6 sec apart), Searing Vow (Lava Lash on the marked enemy
    // sends an echo).
    constexpr float MERCY_POWER = 0.5f, RETRIBUTION_POWER = 0.45f, COMMUNION_HEAL = 0.35f, COMMUNION_STRIKE = 0.3f;
    constexpr float EARTHEN_OATH_POWER = 0.4f, EARTHEN_OATH_WARD = 0.1f, CRUSADERS_FLAME_POWER = 0.35f;
    constexpr float CRUSADERS_FIRE_POWER = 0.15f, DAWNWAVE_SHARE = 0.5f, DAWNWAVE_HALF_WIDTH = 3.0f;
    constexpr float SUNBURST_RADIUS = 8.0f, SUNBURST_SHARE = 0.4f, SACRED_RADIUS = 4.0f, SACRED_POWER = 0.1f;
    constexpr float FROSTFIRE_DAWN_POWER = 0.2f, DAWN_GRACE_SHARE = 0.4f, STEADFAST_SHARE = 0.3f, TORCH_POWER = 0.3f;
    constexpr float PILGRIMAGE_SHARE = 0.4f, PILGRIMAGE_DISTANCE = 8.0f;
    constexpr std::size_t DAWNWAVE_TARGETS = 3, SUNBURST_TARGETS = 2;
    constexpr int32 EARTHEN_OATH_MS = 4000, AFTERIMAGE_MS = 6000, OATH_MS = 2000, OATH_MOST_MS = 6000;
    constexpr uint32 SACRED_MS = 4000, DAWN_CHARGE_MS = 6000, OATH_EVERY_MS = 6000;

    // The Hungering Rift's modifiers: slot (0 Maw, 1 Anchor, 2 Appetite) x 5 + choice.
    enum RiftModifier : uint8
    {
        MOD_DEVOURING_MAW, MOD_MANY_TONGUES, MOD_BLACK_BREATH, MOD_HELLMAW, MOD_HAUNTED_MAW,
        MOD_HAUNTING_RIFT, MOD_RIFT_OPEN_WOUND, MOD_AT_YOUR_BACK, MOD_PAINFUL_ANCHORING, MOD_SIPHON_CONDUIT,
        MOD_LINGERING_TORMENT, MOD_LAST_GASP, MOD_ENDLESS_APPETITE, MOD_DOOM_ECHO, MOD_NECROTIC_APPETITE,
    };

    // The Hungering Rift's tuning: Shadow Bolt (the Hero's own) on their Corruption target opens a Rift on it for 6
    // sec: a lash every 2 sec (1.5 while they channel Drain Soul there) for 25% of their Shadow spell power, 5% more
    // for each of their damage over time effects on the host (5 at most). Shadow Bolt on the host: the next lash
    // also strikes another Corruption target within 8 yards for half. Death Coil on a Corruption target opens or
    // renews it.
    constexpr int32 RIFT_MS = 6000;
    constexpr uint32 LASH_MS = 2000, DRAIN_LASH_MS = 1500;
    constexpr float LASH_POWER = 0.25f, LASH_PER_DOT = 0.05f, RIFT_REACH = 8.0f, RIFT_PENDING_SHARE = 0.5f;
    constexpr uint32 LASH_MOST_DOTS = 5;
    // Its options: Devouring Maw (35% more), Many Tongues (2 more Corruption targets within 8 yards, 60%; Shadow
    // Bolt's pending lash 20% more instead), Black Breath (a 12 yard cone beyond the host, 4 more at half), Hellmaw
    // (their Immolate host: 15% of spell power as Fire), Haunted Maw (their Haunt host: 20% more, Haunt 1 sec
    // longer, 4 sec an application); Haunting Rift (the host 5 yards on: 25% more, 3 sec apart), Open Wound
    // (the Rift stays where it opened, reaching 20 yards; 10% more a lash in a row, 30% at most), At Your Back (it
    // follows the Hero onto their Corruption target within 30 yards; a new target, a 50% lash, 4 sec apart), Painful
    // Anchoring (their Shadow Word: Pain host: one more Pain target within 8 yards, half), Siphon Conduit (their
    // Drain Life on the host: 25% of a lash back, 1% of health a second at most); Lingering Torment (25% of a lash
    // over 6 sec), Last Gasp (every third lash 30% about the host; its death a pending one at half), Endless Appetite
    // (Corruption 1 sec longer a lash, 4 sec an application; the host's death moves the Rift to a corrupted enemy
    // within 15 yards), Doom Echo (every third lash on their Curse of Agony host: 25%), Necrotic Appetite (Death Coil
    // on the host: 2 sec longer, once a Rift).
    constexpr float DEVOURING_MAW = 1.35f, MANY_TONGUES_SHARE = 0.6f, MANY_TONGUES_PENDING = 1.2f;
    constexpr float BLACK_BREATH_RANGE = 12.0f, BLACK_BREATH_SHARE = 0.5f, BLACK_BREATH_HALF_ARC = float(M_PI) / 4;
    constexpr float HELLMAW_POWER = 0.15f, HAUNTED_MAW = 1.2f, HAUNTING_DISTANCE = 5.0f, HAUNTING_RIFT = 1.25f;
    constexpr float OPEN_WOUND_REACH = 20.0f, OPEN_WOUND_STEP = 0.1f, OPEN_WOUND_MOST = 0.3f;
    constexpr float AT_YOUR_BACK_RANGE = 30.0f, AT_YOUR_BACK_SHARE = 0.5f, PAINFUL_SHARE = 0.5f;
    constexpr float SIPHON_CONDUIT_SHARE = 0.25f, SIPHON_CONDUIT_CAP_PCT = 1.0f, TORMENT_SHARE = 0.25f;
    constexpr float LAST_GASP_POWER = 0.3f, LAST_GASP_DEATH = 0.5f, LAST_GASP_RADIUS = 8.0f, DOOM_ECHO_POWER = 0.25f;
    constexpr float APPETITE_REACH = 15.0f;
    constexpr std::size_t MANY_TONGUES_TARGETS = 2, BLACK_BREATH_TARGETS = 4, LAST_GASP_TARGETS = 5;
    constexpr int32 HAUNTED_MAW_MS = 1000, HAUNTED_MAW_MOST_MS = 4000, APPETITE_MS = 1000, APPETITE_MOST_MS = 4000;
    constexpr int32 TORMENT_MS = 6000, NECROTIC_MS = 2000;
    constexpr uint32 HAUNTING_EVERY_MS = 3000, AT_YOUR_BACK_EVERY_MS = 4000;

    // Howling North's modifiers: slot (0 Blade, 1 Wind, 2 Winter) x 5 + choice.
    enum NorthModifier : uint8
    {
        MOD_GLACIAL_EDGE, MOD_RIMEBLADE, MOD_WINTERS_REACH, MOD_SHATTERING_COLD, MOD_PLAGUEWIND,
        MOD_SHARPENED_WIND, MOD_FROZEN_GROUND, MOD_WINTERS_RETURN, MOD_HORN_OF_THE_NORTH, MOD_DECAYING_ICE,
        MOD_NORTHERN_ENDURANCE, MOD_ICY_WOUND, MOD_COLD_RECOVERY, MOD_WINTERS_MENDING, MOD_GLACIAL_FLOW,
    };

    // Howling North's tuning: Howling Blast marks up to 5 enemies it hits with Northwind for 18 sec and readies 2
    // blades. Obliterate on a marked enemy looses one (60% of attack power as Frost; it costs a Frost and an Unholy
    // rune), Frost Strike on one too (35%); 1.5 sec apart. Icy Touch on a marked enemy: the next blade 20% more,
    // piercing one more enemy for half. After a blade, Frost Strike on its target within 6 sec: 15% of attack power
    // as Frost too. Chains of Ice on a marked enemy: the next blade leaves a 4 sec field, 10% of spell power a second
    // to up to 3 enemies.
    constexpr int32 NORTHWIND_MS = 18000, NORTH_FIELD_MS = 4000, NORTH_FOLLOW_MS = 6000, NORTH_DOT_MS = 6000;
    constexpr uint32 BLADE_EVERY_MS = 1500, NORTHWIND_WINDOW_MS = 500, RIMEBLADE_SPACING_MS = 300;
    constexpr uint8 NORTH_BLADES = 2;
    constexpr float OBLITERATE_BLADE_AP = 0.6f, FROST_STRIKE_BLADE_AP = 0.35f, NORTH_ICY = 1.2f;
    constexpr float NORTH_PIERCE_SHARE = 0.5f, NORTH_LINE = 20.0f;
    constexpr float NORTH_HALF_WIDTH = 2.0f, NORTH_FOLLOW_AP = 0.15f, NORTH_FIELD_POWER = 0.1f;
    constexpr float NORTH_FIELD_RADIUS = 6.0f;
    constexpr std::size_t NORTHWIND_TARGETS = 5, NORTH_FIELD_TARGETS = 3;
    // Its options: Glacial Edge (40% more), Rimeblade (3 blades, 40% each; Frost Strike waits for the last),
    // Winter's Reach (pierces up to 4 for half), Shattering Cold (on an enemy frozen by their Hungering Cold: 50%
    // more), Plaguewind (their Blood Plague target: one more nearby for half); Sharpened Wind (Icy Touch's 35%),
    // Frozen Ground (the field 50% more, 25% slower), Winter's Return (Frost Strike's 25%), Horn of the North (Horn
    // of Winter: the next blade 25% of attack power as Frost too), Decaying Ice (an enemy in their Death and Decay:
    // 30% over 6 sec as Shadowfrost); Northern Endurance (10% faster for 5 sec, the next Frost Strike 10% more), Icy
    // Wound (30% over 6 sec as Frost), Cold Recovery (Howling Blast 1 sec sooner, 8 sec apart), Winter's Mending (the
    // next Death Strike within 6 sec heals 30% more), Glacial Flow (their Frost Fever on the target: the next Frost
    // Strike within 6 sec 20% more).
    constexpr float GLACIAL_EDGE = 1.4f, RIMEBLADE_SHARE = 0.4f, SHATTERING_COLD = 1.5f;
    constexpr float PLAGUEWIND_SHARE = 0.5f, PLAGUEWIND_RANGE = 8.0f;
    constexpr float SHARPENED_WIND = 1.35f, FROZEN_GROUND_MORE = 1.5f, WINTERS_RETURN_AP = 0.25f;
    constexpr float HORN_OF_THE_NORTH_AP = 0.25f, DECAYING_ICE_SHARE = 0.3f, ENDURANCE_STRIKE = 1.1f;
    constexpr float ICY_WOUND_SHARE = 0.3f, WINTERS_MENDING = 1.3f, GLACIAL_FLOW = 1.2f;
    constexpr std::size_t RIMEBLADES = 3, WINTERS_REACH_TARGETS = 4;
    constexpr int32 COLD_RECOVERY_MS = 1000, WINTERS_MENDING_MS = 6000;
    constexpr uint32 COLD_RECOVERY_EVERY_MS = 8000;

    // Trueshot's modifiers: slot (0 Shot, 1 Mark, 2 Recovery) x 5 + choice.
    enum TrueshotModifier : uint8
    {
        MOD_PERFECT_SHOT, MOD_PIERCING_SHOT, MOD_STEADY_AIM, MOD_VENOM_ARROW, MOD_CONCUSSIVE_PIERCE,
        MOD_MARKED_WEAKNESS, MOD_WIDE_TRAJECTORY, MOD_ARCANE_ARROW, MOD_ARCANE_CALIBRATION, MOD_MARKED_RICOCHET,
        MOD_QUICK_RECOVERY, MOD_HUNTERS_MOBILITY, MOD_FOLLOW_UP_SHOT, MOD_STEADY_RECOVERY, MOD_MENDING_AIM,
    };

    // Trueshot's tuning: Aimed Shot becomes Trueshot (a 2 sec stationary windup, its cooldown and cost): the Hero's
    // Aimed Shot for 45% more, piercing up to 4 enemies behind its target (15 yards) for 40% of its hit. Its hit on
    // their Hunter's Mark target leaves Exposed Target (6 sec): their next Arcane Shot or Chimera Shot 20% more.
    // Multi-Shot: the next Trueshot within 8 sec a wider line and 20% more to the others. Silencing Shot: the
    // next within 8 sec 30% of ranged attack power as Arcane to its target.
    constexpr float TRUESHOT_MORE = 1.45f, TRUESHOT_PIERCE_SHARE = 0.4f, TRUESHOT_LINE = 15.0f;
    constexpr float TRUESHOT_HALF_WIDTH = 1.5f, WIDE_HALF_WIDTH = 3.0f, MULTI_PIERCE_MORE = 1.2f, EXPOSED_MORE = 1.2f;
    constexpr float ARCANE_PAYLOAD_POWER = 0.3f;
    constexpr std::size_t TRUESHOT_PIERCES = 4;
    constexpr int32 TRUESHOT_EXPOSED_MS = 6000;
    constexpr uint32 TRUESHOT_CHARGE_MS = 8000, TRUESHOT_PENDING_MS = 3000;
    // Its options: Perfect Shot (30% more), Piercing Shot (the others 65%), Steady Aim (2.5 sec; 30% more, the others
    // 20% more), Venom Arrow (their Serpent Sting target: 25% of ranged attack power as Nature), Concussive Pierce
    // (their Concussive Shot's slow on it: 25% of ranged attack power); Marked Weakness (Exposed Target 35%), Wide
    // Trajectory (Multi-Shot's line 2 more), Arcane Arrow (the payload 50%), Arcane Calibration (the payload 40% on
    // up to 2 pierced), Marked Ricochet (their marked target: one more within 8 yards, 25%); Quick Recovery (Arcane
    // Shot 2 sec sooner), Hunter's Mobility (25% faster for 4 sec), Follow-Up Shot (the next Arcane Shot within 6 sec
    // 20% more), Steady Recovery (the next Steady Shot 20% quicker), Mending Aim (their Mend Pet on their pet: 5% of
    // its health, 10 sec apart).
    constexpr float PERFECT_SHOT = 1.3f, PIERCING_SHARE = 0.65f, STEADY_AIM_MORE = 1.3f, STEADY_AIM_PIERCE = 1.2f;
    constexpr float VENOM_ARROW_RAP = 0.25f, CONCUSSIVE_RAP = 0.25f, MARKED_WEAKNESS = 1.35f;
    constexpr float ARCANE_ARROW_POWER = 0.5f, CALIBRATION_SHARE = 0.4f, RICOCHET_SHARE = 0.25f, RICOCHET_RANGE = 8.0f;
    constexpr float FOLLOW_UP_MORE = 1.2f, MENDING_AIM_PCT = 5.0f;
    constexpr std::size_t WIDE_MORE_PIERCES = 2, CALIBRATION_TARGETS = 2;
    constexpr int32 QUICK_RECOVERY_MS = 2000, FOLLOW_UP_MS = 6000;
    constexpr uint32 MENDING_AIM_EVERY_MS = 10000;

    // Scarlet Benediction's modifiers: slot (0 Bloom, 1 Tending, 2 Aftermath) x 5 + choice.
    enum ScarletModifier : uint8
    {
        MOD_SCARLET_DEEP_BLOOM, MOD_SCARLET_GARDEN, MOD_SCARLET_BLOODROSE, MOD_SCARLET_CIRCLING_BLOOM,
        MOD_SCARLET_SHIELDED_BLOOM,
        MOD_SCARLET_PATIENT_CULTIVATION, MOD_SCARLET_SWIFT_BLOSSOM, MOD_SCARLET_GUIDED_PETALS, MOD_SCARLET_CHORAL_BLOOM,
        MOD_SCARLET_MENDING_PETALS,
        MOD_SCARLET_SECOND_BLOOM, MOD_SCARLET_SHELTERING_PETALS, MOD_SCARLET_RENEWING_FLOWER,
        MOD_SCARLET_GUARDIAN_BLOOM, MOD_SCARLET_BINDING_PETALS,
    };

    // Scarlet Benediction's tuning: Renew plants a dormant Scarlet Bloom on its target (as long as the Renew). The
    // Hero's direct Flash Heal or Greater Heal on it opens it: the most injured other ally within 10 yards healed for
    // 80% of their spell power; none, the target shielded for 40% for 6 sec. Once a Bloom; that ally takes no new
    // Bloom for 8 sec after. A Renew that ticked 3 times (9 sec on): 30% more. Prayer of Healing opens up to 3 of the
    // Blooms it heals, Binding Heal its target's.
    constexpr float SCARLET_OPENING = 0.8f, SCARLET_PETALS = 0.4f, SCARLET_RANGE = 10.0f, SCARLET_RIPE = 1.3f;
    constexpr int32 SCARLET_RIPE_MS = 9000, SCARLET_PETALS_MS = 6000;
    constexpr uint32 SCARLET_REARM_MS = 8000;
    constexpr std::size_t SCARLET_PRAYER_OPENS = 3;
    // Its options: Deep Bloom (110%), Scarlet Garden (two allies, 60% each), Bloodrose (the healed ally 30% more over
    // 6 sec), Circling Bloom (Circle of Healing opens up to 2), Shielded Bloom (Power Word: Shield opens it); Patient
    // Cultivation (ripe 60% more), Swift Blossom (4 sec apart), Guided Petals (20 yards), Choral Bloom (Prayer of
    // Healing opens up to 5, 15% more), Mending Petals (their Prayer of Mending on the Bloom's ally: 30% more); Second
    // Bloom (opens twice), Sheltering Petals (its ally shielded as well), Renewing Flower (its Renew begins again),
    // Guardian Bloom (their Guardian Spirit on it: 50% more), Binding Petals (Binding Heal opens the Hero's own too).
    constexpr float DEEP_BLOOM = 1.1f, SCARLET_GARDEN_SHARE = 0.6f, BLOODROSE_SHARE = 0.3f, PATIENT_RIPE = 1.6f;
    constexpr float GUIDED_PETALS_RANGE = 20.0f, CHORAL_MORE = 1.15f, MENDING_PETALS = 1.3f, GUARDIAN_BLOOM = 1.5f;
    constexpr int32 BLOODROSE_MS = 6000;
    constexpr uint32 SWIFT_BLOSSOM_MS = 4000;
    constexpr std::size_t SCARLET_GARDEN_ALLIES = 2, CIRCLING_OPENS = 2, CHORAL_OPENS = 5;

    // The Evergrove's modifiers: slot (0 Grove, 1 Seeking, 2 Bloom) x 5 + choice.
    enum GroveModifier : uint8
    {
        MOD_GROVE_DEEP_ROOTS, MOD_GROVE_FOURFOLD, MOD_GROVE_REGROWTHS_ROOT, MOD_GROVE_NOURISHING, MOD_GROVE_THORNED,
        MOD_GROVE_WILD_SEEDING, MOD_GROVE_GENTLE_SEEKING, MOD_GROVE_LIFEBOUND, MOD_GROVE_TRANQUIL, MOD_GROVE_SWIFT,
        MOD_GROVE_VERDANT_SWIFTMEND, MOD_GROVE_OVERGROWTH, MOD_GROVE_SHELTER, MOD_GROVE_ROOTED_TOUCH,
        MOD_GROVE_INNERVATING,
    };

    // The Evergrove's tuning: Rejuvenation stacks. Cast on the Hero's own Rejuvenation it grows a Second, then a Third
    // Rejuvenation (each its own heal over time, as much as their Rejuvenation's every 3 sec for 15 sec); cast on a
    // full stack, it renews them all. Smart: on an ally at full health or a full stack, it seeks the most injured ally
    // within 30 yards without a full stack instead and grows there. Swiftmend 20% more for each extra Rejuvenation on
    // its target (they stay). Wild Growth grows a stack on up to 2 of the allies it heals with their Rejuvenation.
    constexpr uint8 GROVE_STACK = 3;
    constexpr float GROVE_SEEK_RANGE = 30.0f, GROVE_SWIFTMEND = 0.2f;
    constexpr int32 GROVE_ECHO_MS = 15000;
    constexpr std::size_t GROVE_WILD_GROWTHS = 2;
    // Its options: Deep Roots (the extra ones 20% more), Fourfold Grove (up to 4), Regrowth's Root (Regrowth on their
    // Rejuvenation grows it), Nourishing Grove (Nourish 8% more a Rejuvenation), Thorned Grove (their Thorns on the
    // ally: the extra ones 25% more); Wild Seeding (Wild Growth up to 4), Gentle Seeking (seeks from 90% health on),
    // Lifebound (Lifebloom's bloom grows it), Tranquil Grove (Tranquility grows it, once an ally every 8 sec), Swift
    // Grove (Nature's Swiftness: the full stack at once, spent); Verdant Swiftmend (40% a stack), Overgrowth
    // (Swiftmend on a full stack: 30% of it to up to 3 within 10 yards), Grove Shelter (a full stack shields the ally
    // for 40% of spell power, 6 sec), Rooted Touch (Healing Touch on it renews them), Innervating Grove (Innervate on
    // an ally grows their Rejuvenation there to the full stack).
    constexpr float DEEP_ROOTS = 1.2f, NOURISHING_GROVE = 0.08f, THORNED_GROVE = 1.25f, GENTLE_SEEKING_PCT = 90.0f;
    constexpr float VERDANT_SWIFTMEND = 0.4f, OVERGROWTH_SHARE = 0.3f, OVERGROWTH_RANGE = 10.0f, GROVE_SHELTER = 0.4f;
    constexpr int32 GROVE_SHELTER_MS = 6000;
    constexpr uint32 TRANQUIL_GROVE_MS = 8000;
    constexpr std::size_t WILD_SEEDING_GROWTHS = 4, OVERGROWTH_ALLIES = 3;

    // Iron Reckoning's modifiers: slot (0 Impact, 1 Empowerment, 2 Hold) x 5 + choice.
    enum IronModifier : uint8
    {
        MOD_IRON_CRUSHING_REPRISAL, MOD_IRON_WIDE_RECKONING, MOD_IRON_IMMOVABLE, MOD_IRON_SHOCKWAVE,
        MOD_IRON_CONCUSSIVE,
        MOD_IRON_RETALIATION, MOD_IRON_SHAKING_GROUND, MOD_IRON_SUNDERING_FORCE, MOD_IRON_VENGEFUL_DEVASTATION,
        MOD_IRON_RENDING_IMPACT,
        MOD_IRON_FORTIFIED_BULWARK, MOD_IRON_SHIELDED_ADVANCE, MOD_IRON_COMMANDING_FORCE, MOD_IRON_LAST_BASTION,
        MOD_IRON_WALL_OF_IRON,
    };

    // Iron Reckoning's tuning: Shield Block raises a 14 sec Iron Bulwark in front of the Hero, absorbing up to 15% of
    // their maximum health. Shield Slam in it releases an Iron impact once: 20% of Shield Slam's hit plus half of what
    // the bulwark absorbed (that part at most half their attack power), to every enemy within 8 yards (no limit); the
    // bulwark stays. In the bulwark: Revenge makes the impact 20% stronger, Thunder Clap reach 10 yards, Devastate
    // 15% stronger (each once).
    constexpr float IRON_BULWARK_PCT = 15.0f, IRON_SLAM_SHARE = 0.2f, IRON_ABSORBED_SHARE = 0.5f, IRON_AP_CAP = 0.5f;
    constexpr float IRON_RANGE = 8.0f, IRON_CLAP_RANGE = 10.0f, IRON_REVENGE = 1.2f, IRON_DEVASTATE = 1.15f;
    constexpr uint32 IRON_BULWARK_MS = 14000;
    // Its options: Crushing Reprisal (30% more), Wide Reckoning (30% more to every enemy past the first), Immovable
    // (released as the bulwark ends instead, 125%), Shockwave Reckoning (Shockwave in it releases it once more, 60%),
    // Concussive Reckoning (Concussion Blow in it: its first target 25% of attack power more); Retaliation (Revenge
    // 40%), Shaking Ground (after Thunder Clap, 12 yards), Sundering Force (Devastate 30%), Vengeful Devastation
    // (Devastate in it: the next Revenge 20% more), Rending Impact (their Rend target: 30% of it over 6 sec); Fortified
    // Bulwark (20% more), Shielded Advance (Shield Slam: 20% faster and 3% of maximum health absorbed, 4 sec),
    // Commanding Force (60% more threat), Last Bastion (Last Stand on them: 5% more of maximum health), Wall of Iron
    // (the impact heals them 2% of maximum health, 5% under Shield Wall, 10 sec apart).
    constexpr float CRUSHING_REPRISAL = 1.3f, IMMOVABLE_MORE = 1.25f, SHOCKWAVE_RECKONING = 0.6f, CONCUSSIVE_AP = 0.25f;
    constexpr float RETALIATION = 1.4f, SHAKING_GROUND_RANGE = 12.0f, SUNDERING_FORCE = 1.3f, VENGEFUL_REVENGE = 1.2f;
    constexpr float RENDING_IMPACT_SHARE = 0.3f, FORTIFIED_BULWARK = 1.2f, SHIELDED_ADVANCE_PCT = 3.0f;
    constexpr float IRON_COMMANDING_THREAT = 0.6f, LAST_BASTION_PCT = 5.0f;
    constexpr float WALL_OF_IRON_PCT = 2.0f, WALL_OF_IRON_WALL = 5.0f;
    constexpr int32 RENDING_IMPACT_MS = 6000, SHIELDED_ADVANCE_MS = 4000;
    constexpr uint32 WALL_OF_IRON_EVERY_MS = 10000;
    constexpr float WIDE_RECKONING = 1.3f;

    // The Hollow Congregation's modifiers (its Extended Options, a redesign): slot (0 Congregation, 1 Sermon, 2
    // Command) x 5 + choice. The Sermon's first three are the ones it had; the other two slots are new (their picks
    // reset: titan_runes_zz_extended_reset.sql).
    enum HollowModifier : uint8
    {
        MOD_FOLLOWING_SHADOWS, MOD_HAUNTED_GROUND, MOD_POSSESSION, MOD_PLAGUE_SERMON, MOD_MOONLIT_CONGREGATION,
        MOD_UNISON, MOD_DISSONANCE, MOD_ACCUSATION, MOD_DEVOURING_COMMAND, MOD_FROSTBOUND_SERMON,
        MOD_LAST_WORD, MOD_PROCESSION, MOD_SILENT_WITNESS, MOD_FEEDING_THE_VOID, MOD_RENEWED_WITNESS,
    };

    // The Hollow Congregation's tuning: an echo's channel ticks for 40% of the Hero's Mind Flay tick; a barrage is 3
    // bolts each, 0.4 sec apart, 60% of a tick each.
    constexpr uint32 HOLLOW_ECHO_MS = 10000, BARRAGE_SPACING_MS = 400;
    constexpr std::size_t MAX_HOLLOW_ECHOES = 3;
    constexpr uint32 BARRAGE_BOLTS = 3;
    constexpr float HOLLOW_SHARE = 0.4f, BARRAGE_SHARE = 0.6f, UNISON_BARRAGE = 1.2f, SCREAM_SHARE = 1.5f;
    constexpr float HOLLOW_RANGE = 40.0f, SCREAM_WIDTH = 2.0f;
    constexpr float LAST_WORD_TICKS = 3.0f;
    constexpr std::size_t SCREAM_TARGETS = 5;
    // The Extended Options: each distinct damage over time of the Hero's on an echo's target, 5% more (5 at most).
    // Its interactions: a barrage ending on their Shadow Word: Pain target leaves a patch, 10% of spell power a
    // second to up to 3 enemies for 3 sec (renewed); one barrage bolt of each echo also strikes another of their
    // Corruption targets within 8 yards for half. New options: Following Shadows 15% more (the echoes follow the
    // Hero), Haunted Ground 20% more and 10 yards farther (they stay), Possession 20% more (they follow their target,
    // one an enemy), Plague Sermon (barrage bolts on their Corruption target 20% more), Moonlit Congregation (a
    // channel on their Moonfire target: 10% of spell power as Arcane), Devouring Command (Shadow Bolt: an echo to its
    // target, a 30% hit, 8 sec apart), Frostbound Sermon (Frostbolt on an echo's target: its next barrage strikes one
    // more within 8 yards for half), Procession (Mind Blast: the echoes beside its target, the first bolt 30% more),
    // Silent Witness (the Hero moving: 25% more), Feeding the Void (their Drain Soul on the target: 20% more),
    // Renewed Witness (a barrage done in their Renew: 30% of spell power healed, 8 sec apart).
    constexpr uint32 PATCH_MS = 3000, PATCH_TICK_MS = 1000, DEVOURING_EVERY_MS = 8000, WITNESS_EVERY_MS = 8000;
    constexpr float DOT_STEP = 0.05f, PATCH_POWER = 0.1f, CORRUPTION_SHARE = 0.5f, SPLIT_RANGE_HOLLOW = 8.0f;
    constexpr float FOLLOWING = 1.15f, HAUNTED = 1.2f, HAUNTED_RANGE = 10.0f, POSSESSION = 1.2f, PLAGUE_SERMON = 1.2f;
    constexpr float MOONLIT_POWER = 0.1f, DEVOURING_POWER = 0.3f, FROSTBOUND_SHARE = 0.5f, PROCESSION = 1.3f;
    constexpr float SILENT_WITNESS = 1.25f, FEEDING = 1.2f, WITNESS_POWER = 0.3f, PATCH_RADIUS = 6.0f;
    constexpr std::size_t MAX_DOTS = 5, PATCH_TARGETS = 3;

    // The Shattered Sun's modifiers: slot (0 Formation, 1 Fracture, 2 Collapse) x 3 + choice.
    enum SunModifier : uint8
    {
        MOD_TWIN_SUNS, MOD_ORBITING_SUN, MOD_SOLAR_ANCHOR, MOD_LUNAR_SHARD, MOD_STORM_SHARD,
        MOD_SPLINTERSTORM, MOD_SOLAR_LANCE, MOD_SCALDING_GROUND, MOD_FROZEN_FLASH, MOD_FLAME_BURST,
        MOD_IMPLOSION, MOD_CHAIN_REACTION, MOD_SECOND_SUNRISE, MOD_SHATTERING_LANCE, MOD_ICY_SHELTER,
    };

    // The Shattered Sun's tuning (its Extended Options, a redesign): Fireball (the Hero's own) embeds a Sunshard in
    // its target for 10 sec, in 2 enemies at most (a new one replaces the oldest). Frostbolt on a shard fractures it:
    // 45% of the Hero's Frostfire power to the host, 25% to up to 4 enemies within 6 yards of it, and the host's
    // Broken Core (8 sec): their next Fireball there collapses it for 40%. Ice Lance on a Broken Core: 20% as Frost,
    // once a Core; a fracture on their Flame Shock target makes it 2 sec longer (6 sec an application).
    constexpr std::size_t SHARD_HOSTS = 2, TWIN_HOSTS = 3, FRACTURE_TARGETS = 4, SUN_TARGETS = 5;
    constexpr int32 SUNSHARD_MS = 10000, BROKEN_CORE_MS = 8000, SUN_FLAME_MS = 2000, SUN_FLAME_MOST_MS = 6000;
    constexpr float FRACTURE_HOST = 0.45f, FRACTURE_SPLASH = 0.25f, FRACTURE_RADIUS = 6.0f, COLLAPSE_POWER = 0.4f;
    constexpr float CORE_LANCE_POWER = 0.2f;
    // Its options: Twin Suns (3 hosts, the host 15% more), Orbiting Sun (their enemy within 15 yards 40% of the
    // host's), Solar Anchor (the shard stays where its host stood, 10 sec: Frostbolt on any enemy within 6 yards of
    // it fractures it, the host 20% more), Lunar Shard (Fireball on their Moonfire target: 25% as Arcane to the
    // host), Storm Shard (Lightning Bolt embeds one too, 8 sec apart: 20% as Nature to the host); Splinterstorm (a
    // 12 yard cone away from the Hero, up to 5, the others half the host's), Solar Lance (up to 3 behind the host
    // within 15 yards, 60% of the host's), Scalding Ground (6 yards, 4 sec, 10% a second to up to 5), Frozen Flash
    // (Ice Lance fractures too, 8 sec apart), Flame Burst (their Flame Shock spread to one enemy within 8 yards, 8 sec
    // apart); Implosion (the collapse 50% more), Chain Reaction (the collapse fractures one shard within 8 yards at
    // half), Second Sunrise (1 sec later half the collapse again on the host and 2 within 6 yards), Shattering Lance
    // (Ice Lance's Core hit 35%, the next Fireball 15% quicker for 6 sec), Icy Shelter (a collapse with Ice Barrier
    // up: 40% as a 6 sec absorb, 12 sec apart).
    constexpr float TWIN_SUNS = 1.15f, ORBITING_SHARE = 0.4f, ORBITING_RANGE = 15.0f, SOLAR_ANCHOR = 1.2f;
    constexpr float ANCHOR_REACH = 6.0f, LUNAR_POWER = 0.25f, STORM_SHARD_POWER = 0.2f, SPLINTER_RANGE = 12.0f;
    constexpr float SPLINTER_SHARE = 0.5f, SPLINTER_HALF_ARC = float(M_PI) / 4, SOLAR_LANCE_RANGE = 15.0f;
    constexpr float SOLAR_LANCE_SHARE = 0.6f, SOLAR_LANCE_HALF_WIDTH = 2.0f;
    constexpr float SCALDING_RADIUS = 6.0f, SCALDING_POWER = 0.1f;
    constexpr float FLAME_BURST_RANGE = 8.0f, IMPLOSION = 1.5f, CHAIN_RADIUS = 8.0f, CHAIN_SHARE = 0.5f;
    constexpr float SUNRISE_SHARE = 0.5f, SUNRISE_RADIUS = 6.0f, SHATTERING_LANCE_POWER = 0.35f;
    constexpr float ICY_SHELTER_POWER = 0.4f;
    constexpr std::size_t SOLAR_LANCE_TARGETS = 3, SUNRISE_TARGETS = 2;
    constexpr uint32 SCALDING_MS = 4000, SUNRISE_DELAY_MS = 1000, STORM_SHARD_EVERY_MS = 8000;
    constexpr uint32 FROZEN_FLASH_EVERY_MS = 8000, FLAME_BURST_EVERY_MS = 8000, ICY_SHELTER_EVERY_MS = 12000;
    constexpr int32 ICY_SHELTER_MS = 6000;

    // The Spectral Blade's modifiers (its Extended Options): slot (0 Execution Style, 1 Timing, 2 Aftermath) x 5 +
    // choice; each slot's first three are the ones it had (Quick Death is Fatal Opening now, Patient Killer Perfect
    // Ambush; Marked for Murder raises the Backstab mark).
    enum BladeModifier : uint8
    {
        MOD_EXECUTIONER, MOD_THOUSAND_CUTS, MOD_SHADOW_CLEAVE, MOD_FROST_PHANTOM, MOD_FLAME_PHANTOM,
        MOD_FATAL_OPENING, MOD_PERFECT_AMBUSH, MOD_OPEN_WOUND, MOD_MARKED_EXECUTION, MOD_CORRUPTED_BLADE,
        MOD_DEATH_FOLLOWS, MOD_VANISHING_ACT, MOD_MARKED_FOR_MURDER, MOD_RESTORING_PHANTOM, MOD_BLEEDING_FOLLOW,
    };

    // The Spectral Blade's tuning: Eviscerate's phantom strikes for twice its damage, 0.6 sec later; it cannot crit
    // and triggers nothing (a spell of its own). Each phantom's strike: Phantom Rush, Energy regenerating twice as
    // fast for 4 sec (its own spell data).
    constexpr uint32 PHANTOM_RUSH_MS = 4000;
    constexpr float PHANTOM_SHARE = 2.0f, EXECUTIONER = 1.4f, THOUSAND_CUTS = 1.15f, CLEAVE_SHARE = 0.5f;
    constexpr float PATIENT_KILLER = 1.4f, OPEN_WOUND = 1.2f;
    constexpr float CLEAVE_RADIUS = 8.0f, FOLLOW_RANGE = 10.0f;
    constexpr uint32 PHANTOM_DELAY_MS = 600, QUICK_DEATH_MS = 100, PATIENT_KILLER_MS = 2000, CUT_SPACING_MS = 200;
    constexpr uint32 CUTS = 3;
    constexpr int32 OPEN_WOUND_MS = 4000, OPEN_WOUND_TICKS = 4, VANISHING_ACT_MS = 3000;
    constexpr std::size_t CLEAVE_TARGETS = 3;
    // Its interactions: a Backstab hit marks its target for 6 sec, the phantom spending it for 25% more (Marked for
    // Murder 40%); the phantom's hit makes the next Sinister Strike 10 Energy cheaper (6 sec); on the Hero's Shadow
    // Word: Pain target, Phantom Wound: 20% of it over 6 sec. New options: Frost Phantom (their Frost Fever target:
    // 20% of attack power as Frost), Flame Phantom (their Flame Shock target: 20% of spell power as Fire), Fatal
    // Opening also the next builder 10 Energy cheaper, Marked Execution (their Hunter's Mark target: one more within
    // 8 yards for half), Corrupted Blade (their Corruption target: 25% of it as Shadow over 6 sec), Restoring Phantom
    // (in their Renew: 30% of spell power healed, 10 sec apart), Bleeding Follow-Through (their Rend target: the
    // next Sinister Strike 20% more, 6 sec).
    constexpr int32 BACKSTAB_MARK_MS = 6000, PHANTOM_DOT_MS = 6000;
    constexpr uint32 RESTORING_EVERY_MS = 10000;
    constexpr float BACKSTAB_MARK = 1.25f, MARKED_FOR_MURDER = 1.4f, PHANTOM_WOUND_SHARE = 0.2f, FROST_PHANTOM = 0.2f;
    constexpr float FLAME_PHANTOM = 0.2f, MARKED_EXECUTION = 0.5f, MARKED_EXECUTION_RANGE = 8.0f;
    constexpr float CORRUPTED_SHARE = 0.25f, RESTORING_POWER = 0.3f;

    // The Wild Hunt's modifiers: slot (0 Spectral Beast, 1 Hunting Behavior, 2 Final Attack) x 3 + choice.
    enum HuntModifier : uint8
    {
        MOD_ALPHA_PREDATOR, MOD_PACK_RAVAGER, MOD_GHOSTFANG, MOD_MARKED_ALPHA, MOD_FROSTFANG,
        MOD_FIXED_PREY, MOD_HUNTERS_COMMAND, MOD_BLOOD_SCENT, MOD_ARCANE_HUNT, MOD_STORM_HUNT,
        MOD_FINAL_POUNCE, MOD_FRENZIED_FAREWELL, MOD_PARTING_HOWL, MOD_MARKED_FINALE, MOD_HEALING_COMPANION,
    };

    // The Wild Hunt's tuning: a Spectral Beast bites every 1.5 sec for 60% of the Hero's pet's swing, for 10 sec (a
    // Kill Command's) or 4 sec (Bestial Wrath's: one a second for 10 sec); 8 out at most. In Bestial Wrath, Kill
    // Command comes back in 5 sec.
    constexpr uint32 BEAST_MS = 10000, WRATH_BEAST_MS = 4000, WRATH_CALL_MS = 1000, WRATH_KILL_COMMAND_MS = 5000;
    constexpr std::size_t MAX_BEASTS = 8;
    constexpr uint32 BITE_MS = 1500, FAREWELL_SPACING_MS = 150;
    constexpr uint32 FAREWELL_BITES = 4;
    constexpr float BITE_SHARE = 0.6f, ALPHA_PREDATOR = 1.3f, RAVAGER_SHARE = 0.5f, GHOSTFANG_SHARE = 0.25f;
    constexpr float POUNCE = 2.0f, FAREWELL_SHARE = 0.6f, BEAST_RANGE = 40.0f, RAVAGER_RADIUS = 5.0f;
    constexpr int32 GHOSTFANG_MS = 4000;
    constexpr std::size_t RAVAGER_TARGETS = 2;
    // Its interactions and Extended Options (Unending Wrath gone): the beasts 15% more on the Hero's marked target;
    // Arcane Shot on a beast's target, its next bite 20% more (Arcane Hunt 40%; one charge, 6 sec). Marked Alpha
    // (there, one more enemy nearby for 50%), Frostfang (their Frost Fever target: 10% of ranged attack power as
    // Frost); Fixed Prey (20% more), Storm Hunt (Lightning Bolt: the beasts on its target, the next bite 20% of spell
    // power as Nature, 6 sec apart); Marked Finale (the end: 35% of ranged attack power on the marked target, 40% of
    // it to 2 nearby), Healing Companion (the end, their Mend Pet on the pet: 5% of its health, 25% of ranged attack
    // power to the prey).
    constexpr float HUNT_MARK = 1.15f, ARCANE_CHARGE = 1.2f, ARCANE_HUNT = 1.4f, MARKED_ALPHA_SHARE = 0.5f;
    constexpr float FROSTFANG_RAP = 0.1f, FIXED_PREY = 1.2f, STORM_HUNT_SP = 0.2f, MARKED_FINALE_RAP = 0.35f;
    constexpr float MARKED_FINALE_SHARE = 0.4f, COMPANION_HEAL_PCT = 5.0f, COMPANION_RAP = 0.25f;
    constexpr uint32 HUNT_CHARGE_MS = 6000, STORM_HUNT_EVERY_MS = 6000;

    // Arcane Mind's modifiers (its Extended Options): slot (0 Invocation, 1 Manifestation, 2 Adaptation) x 5 +
    // choice; each slot's first three are the ones it had.
    enum MindModifier : uint8
    {
        MOD_ACCELERATED_THOUGHT, MOD_EXPANDING_INTELLECT, MOD_PATIENT_CALCULATION, MOD_STORM_CONDUIT,
        MOD_STARFALL_EQUATION,
        MOD_FOCUSED_INTENTION, MOD_FRACTURED_THOUGHT, MOD_LINGERING_EQUATION, MOD_MISSILE_SHARDS, MOD_ARCANE_CHARGE,
        MOD_UNBOUND_MIND, MOD_MENTAL_RESERVES, MOD_OVERCLOCKED_INTELLECT, MOD_EVOCATIVE_MIND, MOD_SHOCKING_CLARITY,
    };

    // Arcane Mind's tuning: Arcane Blast grants Insight (15 sec), up to 4; at 4 or more Arcane Missiles spends it:
    // free, and 5 Insight missiles fly with it, each 40% of one of its own (the dossier's 20% made worth having).
    constexpr uint8 INSIGHT_THRESHOLD = 4, MAX_INSIGHT = 4, PATIENT_INSIGHT = 6, ACCELERATED_EVERY = 3;
    constexpr uint32 INSIGHT_MISSILES = 5, UNBOUND_SPACING_MS = 500, OVERCLOCK_SPACING_MS = 150;
    constexpr float INSIGHT_SHARE = 0.4f, PATIENT_STEP = 0.15f, FOCUSED_INTENTION = 1.4f, OVERCLOCKED = 1.2f;
    constexpr float FRACTURED_SHARE = 0.35f, FRACTURED_RADIUS = 8.0f, ARCANE_ECHO_SHARE = 0.3f;
    constexpr float RESERVES_MANA_PCT = 3.0f;
    constexpr int32 ARCANE_ECHO_MS = 6000;
    constexpr std::size_t FRACTURED_TARGETS = 2, EXPANDING_ENEMIES = 3;
    // Its interactions: Arcane Barrage spends up to 2 Insight, 15% more each and an enemy within 8 yards each for
    // 30% of its hit; an empowered channel completed makes the next Arcane Blast instant (6 sec); a Lightning Bolt
    // hit grants Insight (8 sec apart). New options: Storm Conduit 20% more (a Lightning Bolt on an enemy the Hero's
    // Arcane Blast hit in the last 15 sec), Starfall Equation (Starfire: Insight 8 sec apart, the next Arcane Blast
    // 10% more), Missile Shards (a Frostbolt in the 15 sec before: the first missile hits 2 more within 8 yards for
    // half), Arcane Charge (Arcane Explosion within 6 sec of an empowered channel: 25% of spell power to 5 within 8
    // yards), Evocative Mind (Evocation within 30 sec of one: 2% of maximum mana, 30 sec apart), Shocking Clarity
    // (with Lightning Shield: a 25%-of-spell-power Nature bolt at its target).
    constexpr uint8 BARRAGE_INSIGHT = 2;
    constexpr uint32 BOLT_INSIGHT_EVERY_MS = 8000, STARFALL_EVERY_MS = 8000, BLASTED_MS = 15000;
    constexpr uint32 SHARDS_READY_MS = 15000, ARCANE_CHARGE_MS = 6000, EVOCATIVE_AFTER_MS = 30000;
    constexpr uint32 EVOCATIVE_EVERY_MS = 30000;
    constexpr float BARRAGE_STEP = 0.15f, BARRAGE_SPLASH = 0.3f, BARRAGE_RANGE = 8.0f, STORM_CONDUIT = 1.2f;
    constexpr float SHARDS_SHARE = 0.5f, SHARDS_RADIUS = 8.0f, ARCANE_CHARGE_POWER = 0.25f;
    constexpr float ARCANE_CHARGE_RADIUS = 8.0f, EVOCATIVE_MANA_PCT = 2.0f, CLARITY_POWER = 0.25f;
    constexpr std::size_t SHARDS_TARGETS = 2, ARCANE_CHARGE_TARGETS = 5;

    // The Splintered Crown's modifiers: slot (0 Formation, 1 Flight, 2 Aftermath) x 3 + choice.
    // (Its Extended Options, a redesign: slot x 5 + choice; its picks reset, titan_runes_zz_extended_reset2.sql.)
    enum CrownModifier : uint8
    {
        MOD_GLACIAL_SPEAR, MOD_TWIN_FANGS, MOD_FROZEN_VOLLEY, MOD_HAILSTORM_CROWN, MOD_SPLINTERING_ICE,
        MOD_COLD_SNAP, MOD_BRITTLE_IMPACT, MOD_GATHERING_BLIZZARD, MOD_FROSTFIRE_FRACTURE, MOD_RUNIC_RIME,
        MOD_DEEP_FREEZE, MOD_GLACIAL_WOUND, MOD_PERMAFROST, MOD_WHITEOUT, MOD_COLD_FRONT,
    };

    // The Splintered Crown's tuning: a direct Frostbolt hit forms an Icicle (5 waiting at most), loosed 1 sec later
    // at the same victim for 30% of the Frostbolt's damage. Its options keep the dossier's ratios to an Icicle.
    constexpr uint8 MAX_ICICLES = 5, AFTERMATH_EVERY = 3, GATHERING_STACKS = 4;
    constexpr uint32 ICICLE_DELAY_MS = 1000, TWIN_SPACING_MS = 200, RIME_MS = 6000, FRACTURE_EVERY_MS = 8000;
    constexpr uint32 HAILSTORM_MS = 2000, HAILSTORM_CROWN_MS = 1000;
    constexpr float ICICLE_SHARE = 0.3f, GLACIAL_SPEAR = 1.4f, ICICLE_RANGE = 45.0f;
    // The Extended Options: Ice Lance looses 2 Icicles (20% of spell power each), Cone of Cold one at each enemy it
    // hits (5, 15%), Blizzard one each 2 sec of its channel (20%); every Icicle leaves Frostbite (6 sec): the Hero's
    // Frostbolt and Ice Lance 8% more there. Its options as the dossier has them.
    constexpr int32 FROSTBITE_MS = 6000, TWIN_WOUND_MS = 4000, GLACIAL_WOUND_MS = 6000, PERMAFROST_MS = 3000;
    constexpr float TWIN_POWER = 0.2f, TWIN_FANGS_POWER = 0.25f, TWIN_WOUND_POWER = 0.1f, GLACIAL_WOUND_POWER = 0.2f;
    constexpr float FRONT_POWER = 0.15f, VOLLEY_POWER = 0.1f, BRITTLE_IMPACT = 1.3f, PERMAFROST = 1.25f;
    constexpr float HAILSTORM_POWER = 0.2f, HAILSTORM_CROWN_POWER = 0.15f, WHITEOUT_SHARE = 0.4f, WHITEOUT_ALONE = 1.2f;
    constexpr float FROSTBITE_BONUS = 1.08f, GATHERING_STEP = 0.05f, DEEP_FREEZE = 1.2f, RIME_SHARE = 0.5f;
    constexpr float FRACTURE_POWER = 0.25f, FROSTFIRE_RADIUS = 6.0f, SPLINTERING_SHARE = 0.5f;
    constexpr float WHITEOUT_RADIUS = 6.0f, SPLINTERING_RADIUS = 8.0f;
    constexpr std::size_t SPLINTERING_TARGETS = 2, FROSTFIRE_TARGETS = 2, WHITEOUT_TARGETS = 2, CONE_ICICLES = 5;

    // Phoenix Wake's modifiers (its Extended Options): slot (0 Phoenix, 1 Flight, 2 Wake) x 5 + choice; each
    // slot's first three are the ones it had.
    enum PhoenixModifier : uint8
    {
        MOD_SUN_TALON, MOD_BROADWING, MOD_ASHEN_FLOCK, MOD_FLAMEKEEPER, MOD_LAVA_FLOCK,
        MOD_HOMEWARD_FLAME, MOD_DEATH_SPIRAL, MOD_WILD_FLIGHT, MOD_BLAZING_PATH, MOD_HOT_PURSUIT,
        MOD_SCORCHED_EARTH, MOD_CLINGING_EMBERS, MOD_REBIRTH, MOD_WILDFIRE_RETURN, MOD_EMBERGUARD,
    };

    // Phoenix Wake's tuning: the Ember holds its Pyroblast's damage (8 sec); the phoenix dives from 8 yards above
    // the Hero onto the host at 20 yards a second, burning up to 4 enemies under its path (3 yards wide) for 25%,
    // and strikes the host for 55% (the Extended Options' 55% to the 25%).
    constexpr int32 EMBER_MS = 8000, CLINGING_MS = 6000;
    constexpr uint32 PHOENIX_MAX_MS = 8000, SPIRAL_MS = 2000, REPATH_MS = 300, HOMEWARD_MS = 2000;
    constexpr uint32 SCORCH_MS = 4000, SCORCH_TICK_MS = 1000, FLOCK_BIRDS = 3;
    constexpr float PHOENIX_HOST = 0.55f, PHOENIX_CROSS = 0.25f, SUN_TALON = 1.5f, BROADWING_CROSS = 0.3f;
    constexpr float SPIRAL_SHARE = 0.25f, CLINGING_SHARE = 0.25f, REBIRTH_SHARE = 0.35f, SCORCH_SHARE = 0.06f;
    constexpr float PHOENIX_HALF_WIDTH = 1.5f, BROADWING_HALF_WIDTH = 3.0f;
    constexpr float PHOENIX_SPEED = 20.0f, HOMEWARD_SPEED = 40.0f, PHOENIX_HEIGHT = 2.0f;
    constexpr float FLOCK_SPREAD = 2.0f, FLOCK_SCALE = 0.6f, SPIRAL_RADIUS = 3.0f;
    constexpr float WILD_FLIGHT_RANGE = 10.0f, VISIT_REACH = 2.0f, STRIKE_REACH = 3.0f, REBIRTH_RADIUS = 6.0f;
    constexpr float PHOENIX_DROP = 8.0f, PHOENIX_BEHIND = 2.0f;     // where it appears: above, behind the Hero
    constexpr float SCORCH_SPACING = 4.0f, SCORCH_HALF_WIDTH = 1.5f, SCORCH_REACH = 60.0f;
    constexpr std::size_t CROSS_TARGETS = 4, WILD_FLIGHT_VISITS = 2, REBIRTH_TARGETS = 5, SCORCH_TARGETS = 5;
    // Its interactions: the phoenix burns its host for 30% of spell power over 6 sec (Phoenix Burn); a direct
    // Pyroblast on a Phoenix Burn target 15% more; crossing the Hero's Flame Shock target erupts for 20% of spell
    // power to up to 3 within 6 yards (once a phoenix). New options: Flamekeeper 20% on a Flame Shock host, Lava
    // Flock a bird of 30% more (Lava Burst on the host, one an Ember), Blazing Path 20% to up to 4 in the Hero's
    // Flamestrike (once a phoenix), Hot Pursuit 25% more (Scorch on the host, 8 sec), Wildfire Return Flame Shock 2
    // sec longer (6 sec an application), Emberguard an absorb of 40% (in Fire Ward, 12 sec apart).
    constexpr int32 PHOENIX_BURN_MS = 6000, WILDFIRE_EXTEND_MS = 2000, WILDFIRE_MOST_MS = 6000;
    constexpr uint32 EMBERGUARD_EVERY_MS = 12000;
    constexpr float PHOENIX_BURN_POWER = 0.3f, BURNING_PYROBLAST = 1.15f, ERUPTION_POWER = 0.2f;
    constexpr float ERUPTION_RADIUS = 6.0f, FLAMEKEEPER_POWER = 0.2f, LAVA_FLOCK_SHARE = 0.3f;
    constexpr float BLAZING_PATH_POWER = 0.2f, HOT_PURSUIT = 1.25f, EMBERGUARD_POWER = 0.4f;
    constexpr std::size_t ERUPTION_TARGETS = 3, BLAZING_PATH_TARGETS = 4;

    // Astral Horizon's modifiers (its Extended Options): slot (0 Orbit, 1 Discharge, 2 Ending) x 5 + choice; each
    // slot's first three are the ones it had.
    enum HorizonModifier : uint8
    {
        MOD_WANDERING_STAR, MOD_EVENT_HORIZON, MOD_SATELLITE, MOD_ORBITAL_LIGHTNING, MOD_STARWARD_SATELLITE,
        MOD_ASTRAL_FOCUS, MOD_CONSTELLATION, MOD_PRISMATIC_LANCE, MOD_SHATTERING_ORBIT, MOD_DISTANT_SPARK,
        MOD_SUPERNOVA, MOD_DYING_LIGHT, MOD_ASTRAL_REMNANT, MOD_EVOKED_REMNANT, MOD_ASTRAL_SHELTER,
    };

    // Astral Horizon's tuning: Arcane Barrage sets the Orb loose at most once every 12 sec, for 6 sec (Satellite 8):
    // it flies at 20 yards a second until it first reaches an enemy, then drifts at 1.5; a bolt a second at its
    // victim within 10 yards for 20% of the Barrage's damage. Its options (the second pass: the weak ones raised):
    // Wandering Star 2 bolts an enemy passed, Event Horizon 50% more, Satellite 15 yards and 8 sec, Astral Focus 60%
    // more, Constellation 75%, Prismatic Lance 80%, Supernova 6 bolts, Dying Light 3 of 60%, Astral Remnant a bolt
    // a second.
    constexpr uint32 ORB_COOLDOWN_MS = 12000, ORB_MS = 6000, SATELLITE_MS = 8000, ORB_BOLT_MS = 1000;
    constexpr uint32 ORB_LINGER_MS = 1000;
    constexpr uint32 REMNANT_MS = 6000, REMNANT_TICK_MS = 1000, DYING_BOLTS = 3, DYING_SPACING_MS = 250;
    constexpr float ORB_BOLT_SHARE = 0.2f, ORB_FAST = 20.0f, ORB_SLOW = 1.5f, ORB_RANGE = 10.0f, ORB_START = 1.5f;
    constexpr float ORB_HEIGHT = 0.5f;                  // over the ground (its look floats a yard higher still)
    constexpr float ORB_CONTACT = 3.0f;                 // an enemy this close: it has reached them, it slows
    constexpr float EVENT_HORIZON = 1.5f, EVENT_RADIUS = 5.0f, ASTRAL_FOCUS = 1.6f, WANDERING_SHARE = 2.0f;
    constexpr float SATELLITE_RANGE = 15.0f;
    constexpr float CONSTELLATION_SHARE = 0.75f, LANCE_SHARE = 0.8f, LANCE_REACH = 10.0f, LANCE_HALF_WIDTH = 1.5f;
    constexpr float WANDERING_HALF_WIDTH = 2.0f, SUPERNOVA_BOLTS = 6.0f, SUPERNOVA_RADIUS = 8.0f, DYING_SHARE = 3.0f;
    constexpr float REMNANT_RADIUS = 6.0f;
    constexpr std::size_t CONSTELLATION_TARGETS = 2, LANCE_TARGETS = 3, WANDERING_TARGETS = 5;
    constexpr std::size_t SUPERNOVA_TARGETS = 5, REMNANT_TARGETS = 5;
    // The Extended Options: the Orb strikes its victim for two bolts as it is set loose (40% to a bolt's 20%). Its
    // interactions: Arcane Blast on its victim, the next bolt 35% more; Arcane Explosion within 8 yards of it, a
    // bolt's worth to up to 5 within 8 yards of it (4 sec apart); Starfire on its victim, the next bolt also at one
    // more within 8 yards for half. New options: Orbital Lightning (Lightning Bolt on its victim: a Nature bolt, 4
    // sec apart), Starward Satellite (on the Hero's Moonfire target 20% more, the Moonfire 1 sec longer, 4 sec an
    // application, 2 sec apart), Shattering Orbit (a bolt on their Frost Nova target: one more in it within 8 yards
    // for half), Distant Spark (Arcane Shot on its victim: the next bolt in 5 sec pierces one more for half),
    // Evoked Remnant (Evocation completed: 2 sec longer, once an Orb), Astral Shelter (set loose in their Power
    // Word: Shield: an absorb of 25% of spell power, 12 sec apart).
    constexpr uint32 ORB_PULSE_EVERY_MS = 4000, ORBITAL_LIGHTNING_EVERY_MS = 4000, STARWARD_EVERY_MS = 2000;
    constexpr uint32 DISTANT_SPARK_MS = 5000, EVOKED_MS = 2000, SHELTER_EVERY_MS = 12000;
    constexpr int32 STARWARD_EXTEND_MS = 1000, STARWARD_MOST_MS = 4000;
    constexpr float ORB_LAUNCH_SHARE = 2.0f, BLAST_EMPOWER = 1.35f, ORB_PULSE_RANGE = 8.0f, ORB_SPLIT = 0.5f;
    constexpr float SPLIT_RANGE = 8.0f, STARWARD = 1.2f, SHATTERING_SHARE = 0.5f, SHELTER_POWER = 0.25f;
    constexpr std::size_t ORB_PULSE_TARGETS = 5;

    // Guiding Light's modifiers (its Extended Options): slot (0 Radiance, 1 Guidance, 2 Farewell) x 5 + choice;
    // each slot's first three are the ones it had.
    enum GuidingModifier : uint8
    {
        MOD_STEADY_LIGHT, MOD_SHARED_LIGHT, MOD_SHELTERING_LIGHT, MOD_EARTHEN_STAR, MOD_MOONLIT_STAR,
        MOD_GENTLE_PASSAGE, MOD_GUARDIAN_STAR, MOD_SEEKING_LIGHT, MOD_CHAIN_GUIDANCE, MOD_HOLY_PASSAGE,
        MOD_FINAL_BENEDICTION, MOD_AFTERGLOW, MOD_PARTING_GRACE, MOD_TIDAL_FAREWELL, MOD_RENEWED_FAREWELL,
    };

    // Guiding Light's tuning: the star lasts 12 sec and heals its bearer every 2 sec for half a Prayer of Mending;
    // it arrives with a whole one, twice a pulse (the Extended Options' 80% to the 40%; moved at most once every 3
    // sec, 0.5 sec on the way). Its options keep the dossier's ratios to a pulse.
    constexpr uint32 STAR_MS = 12000, STAR_PULSE_MS = 2000, STAR_MOVE_MS = 3000, STAR_TRAVEL_MS = 500;
    constexpr int32 AFTERGLOW_MS = 8000;
    constexpr float STAR_PULSE_SHARE = 0.5f, ARRIVAL_SHARE = 2.0f, STEADY_LIGHT = 1.5f, SHARED_SHARE = 0.75f;
    constexpr float SHELTER_SHARE = 2.0f, PASSAGE_SHARE = 0.5f, GUARDIAN_STEP = 0.1f, BENEDICTION_SHARE = 2.5f;
    constexpr float AFTERGLOW_SHARE = 3.25f, GRACE_SHARE = 2.0f;
    constexpr float STAR_RANGE = 40.0f, SHARED_RANGE = 10.0f, SEEKING_STAR_RANGE = 15.0f, PASSAGE_HALF_WIDTH = 3.0f;
    constexpr uint8 GUARDIAN_STEPS = 3;
    constexpr uint32 SEEKING_EVERY = 3;
    constexpr std::size_t PASSAGE_ALLIES = 3;
    // Its interactions (a pulse is 40% of spell power): the star leaving the Hero's Renew target leaves a trail of
    // 15% every 2 sec for 6 sec; Greater Heal's redirect, an absorb of 30% on the new bearer (6 sec); arriving on
    // their Earth Shield bearer, 30% to one injured ally within 10 yards. New options: Earthen Star (each pulse on
    // the Earth Shield bearer, 25% to one injured ally within 10 yards), Moonlit Star (each pulse on their
    // Rejuvenation bearer: 1 sec longer, 4 sec an application), Chain Guidance (a Chain Heal on another ally moves
    // it), Holy Passage (Holy Shock on another ally moves it), Tidal Farewell (their Riptide on the bearer: 60%),
    // Renewed Farewell (their Renew on the bearer 4 sec longer, 6 sec an application, and 30%).
    constexpr int32 MOONLIT_EXTEND_MS = 1000, MOONLIT_MOST_MS = 4000, RENEWED_EXTEND_MS = 4000;
    constexpr int32 RENEWED_MOST_MS = 6000;
    constexpr float TRAIL_SHARE = 0.375f, GUIDING_WARD_SHARE = 0.75f, EARTH_ARRIVAL_SHARE = 0.75f;
    constexpr float EARTHEN_SHARE = 0.625f, TIDAL_FAREWELL_SHARE = 1.5f, RENEWED_FAREWELL_SHARE = 0.75f;
    constexpr float EARTH_RANGE = 10.0f;

    // Elune's Guardian's modifiers (its Extended Options): slot (0 Guardian, 1 Descent, 2 Blessing) x 5 + choice;
    // each slot's first three are the ones it had (Watchful Flight is Lunar Flock now; Moonlit Pursuit gave way to
    // Lunar Guidance, its picks reset; Twilight Hunt comes from the return strike).
    enum OwlModifier : uint8
    {
        MOD_MOONPIERCER, MOD_SILVER_WINGS, MOD_LUNAR_FLOCK, MOD_SENTINEL, MOD_FROSTFEATHER,
        MOD_SWIFT_JUDGMENT, MOD_PATIENT_VIGIL, MOD_LUNAR_GUIDANCE, MOD_COMMAND_THE_HUNT, MOD_SUN_AND_MOON,
        MOD_LUNAR_WOUND, MOD_GUIDING_WINGS, MOD_TWILIGHT_HUNT, MOD_HUNTERS_PURSUIT, MOD_WINTERS_BLESSING,
    };

    // Elune's Guardian's tuning: a direct Starfire hit calls a lunar owl (3 waiting at most) that dives 1 sec later
    // for 37.5% of that Starfire's damage (the Extended Options' 37.5% spell power, 25% over the first 30%).
    constexpr uint8 MAX_OWLS = 3, GUIDING_EVERY = 3;
    constexpr uint32 OWL_DELAY_MS = 1000, SWIFT_DELAY_MS = 100, VIGIL_DELAY_MS = 3000, WATCHFUL_SPACING_MS = 1000;
    constexpr uint32 WATCHFUL_ATTACKS = 3;
    constexpr float OWL_SHARE = 0.375f, MOONPIERCER = 1.4f, SILVER_WINGS_SHARE = 0.5f, WATCHFUL_SHARE = 0.4f;
    constexpr float PATIENT_VIGIL = 1.3f, LUNAR_WOUND_SHARE = 1.0f, TWILIGHT_HUNT = 1.25f;
    constexpr float SILVER_WINGS_RADIUS = 6.0f;
    constexpr int32 LUNAR_WOUND_MS = 6000, TWILIGHT_HUNT_MS = 5000;
    constexpr std::size_t SILVER_WINGS_TARGETS = 4;
    // Its interactions: the dive leaves Moonlit Prey (8 sec); a direct Wrath on it calls the owl back through it,
    // 20% of spell power as Nature (the return strike); a dive on the Hero's Moonfire target, the Moonfire 2 sec
    // longer (6 sec an application); on their Hunter's Mark target, 20% more, and the return strike also at one more
    // within 8 yards for half. New options: Sentinel of the Hunt (an owl on their Hunter's Mark target at once, at 3
    // and at 6 sec, 45% each, a new one replacing it), Frostfeather (half the dive as Frost; a Frostbolt on an owl's
    // victim before it dives, a burst of 25% of spell power on it and 2 more within 6 yards), Lunar Guidance (on
    // their Moonfire target 15% more, 4 sec longer, 8 at most), Command the Hunt (Arcane Shot or Aimed Shot on the
    // marked target with an owl out: 25% of spell power, 6 sec apart), Sun and Moon (the return strike also 15% to
    // up to 4 within 6 yards), Hunter's Pursuit (an owl on the marked target lays Exposed Prey, 4 sec apart; Arcane,
    // Aimed or Steady Shot spends it for 25% of spell power), Winter's Blessing (a dive on their Frostbolt's slow
    // lays Moonfrost: the next Frostbolt there 20% more, and their Moonfire 2 sec longer).
    constexpr int32 MOONLIT_PREY_MS = 8000, OWL_MOONFIRE_MS = 2000, OWL_MOONFIRE_MOST_MS = 6000, GUIDANCE_MS = 4000;
    constexpr int32 GUIDANCE_MOST_MS = 8000, EXPOSED_MS = 6000, MOONFROST_MS = 6000;
    constexpr uint32 SENTINEL_EVERY_MS = 3000, SENTINEL_ATTACKS = 3, COMMAND_EVERY_MS = 6000;
    constexpr uint32 EXPOSED_EVERY_MS = 4000;
    constexpr float RETURN_POWER = 0.2f, MARKED_OWL = 1.2f, RETURN_SPLIT = 0.5f, RETURN_RANGE = 8.0f;
    constexpr float SENTINEL_SHARE = 0.45f, FEATHER_POWER = 0.25f, FEATHER_RADIUS = 6.0f, GUIDANCE = 1.15f;
    constexpr float COMMAND_POWER = 0.25f, SUN_AND_MOON_POWER = 0.15f, SUN_AND_MOON_RADIUS = 6.0f;
    constexpr float EXPOSED_POWER = 0.25f, MOONFROST = 1.2f, MARK_SEARCH = 60.0f;
    constexpr std::size_t FEATHER_TARGETS = 2, SUN_AND_MOON_TARGETS = 4;

    // Astral Convergence's modifiers (its Extended Options): slot (0 Collision, 1 Alignment, 2 Flare) x 5 + choice;
    // each slot's first three are the ones it had, so earlier picks keep their meaning.
    enum ConvergenceModifier : uint8
    {
        MOD_PERFECT_ALIGNMENT, MOD_CONVERGING_SKIES, MOD_ASTRAL_RUPTURE, MOD_FLAME_ECLIPSE, MOD_FROZEN_ECLIPSE,
        MOD_LONG_TWILIGHT, MOD_CROSSED_HORIZONS, MOD_STEADY_ORBIT, MOD_TIDAL_FLARE, MOD_TWIN_MOONS,
        MOD_SOLAR_RETURN, MOD_LUNAR_DOMINANCE, MOD_EVENTIDE, MOD_INSECT_RENEWAL, MOD_ASTRAL_WARD,
    };

    // Astral Convergence's tuning: Wrath leaves Sunlight (18 sec); Starfire on it collides for 80% of the Hero's
    // Arcane spell power and leaves Moonlight (18 sec); Wrath on Moonlight flares for 40% of their Nature spell power
    // and leaves fresh Sunlight. 5 enemies marked at most. Its interactions: a collision on the Hero's Moonfire
    // target hits one more within 8 yards for 40% of it; a flare extends their Insect Swarm 2 sec (6 sec at most an
    // application); a Lightning Bolt spends Moonlight for a flare of 75%, no Sunlight after (6 sec a target).
    constexpr int32 MARK_MS = 18000, LONG_TWILIGHT_MS = 22000, RUPTURE_MS = 6000;
    constexpr int32 SWARM_EXTEND_MS = 2000, SWARM_EXTEND_MOST_MS = 6000;
    constexpr uint32 EVENTIDE_MS = 4000, EVENTIDE_TICK_MS = 1000, TWILIGHT_STEP_MS = 2000, BOLT_FLARE_EVERY_MS = 6000;
    constexpr uint32 TWIN_MOONS_EVERY_MS = 8000, ASTRAL_WARD_EVERY_MS = 12000;
    constexpr float COLLISION_POWER = 0.8f, FLARE_POWER = 0.4f, PERFECT_ALIGNMENT = 1.4f, CONVERGING_SHARE = 0.5f;
    constexpr float RUPTURE_SHARE = 1.5f, TWILIGHT_STEP = 0.05f, TWILIGHT_MOST = 0.2f, CROSSED_SHARE = 0.35f;
    constexpr float SOLAR_RETURN = 1.6f, EVENTIDE_POWER = 0.1f, MOONFIRE_SHARE = 0.4f, BOLT_FLARE = 0.75f;
    constexpr float FLAME_ECLIPSE_POWER = 0.25f, FROZEN_ECLIPSE = 1.25f, TIDAL_SHARE = 0.5f, ASTRAL_WARD_POWER = 0.4f;
    constexpr float CONVERGING_RADIUS = 6.0f, CROSSED_RANGE = 12.0f, EVENTIDE_RADIUS = 6.0f, ECHO_RANGE = 8.0f;
    constexpr std::size_t MAX_MARKED = 5, CONVERGING_TARGETS = 4, CROSSED_TARGETS = 3, EVENTIDE_TARGETS = 5;

    // Ursoc's Champion's modifiers: slot (0 Guard, 1 Answer, 2 Resolve) x 3 + choice.
    // Ursoc's Champion's modifiers (its Extended Options, a redesign): slot (0 Impact, 1 Guard, 2 Resolve) x 5 +
    // choice; its picks reset (titan_runes_zz_extended_reset3.sql).
    enum UrsocModifier : uint8
    {
        MOD_CRUSHING_CHAMPION, MOD_PACKBREAKER, MOD_UNBROKEN_CHAMPION, MOD_STONEGUARD, MOD_THUNDERBEAR,
        MOD_LONG_GUARD, MOD_EARTHEN_HIDE, MOD_URSOCS_CHALLENGE, MOD_RENDING_MAUL, MOD_EARTHSHOCK_ROAR,
        MOD_DEFIANT_ROAR, MOD_RESTORING_FORCE, MOD_BEARS_MOMENTUM, MOD_LIFEBLOOM_HIDE, MOD_SHIELDING_ROOTS,
    };

    // Ursoc's Champion's tuning: Barkskin in Bear Form adds Ursoc's Guard (16 sec, an absorb of 30% of maximum
    // health) and comes back 30 sec sooner; each Maul in Bear Form takes 3 sec more off it. Maul in the guard
    // releases the impact: 75% of attack power plus all the guard absorbed (that part at most 250% of attack power)
    // in a 10-yard cone in front (up to 5); ours kept over the Extended Options' 15% guard and 25% blow. Its
    // interactions: Mangle in the guard marks its target (the impact there: 20% of attack power to up to 4 near it,
    // and threat); Swipe in it, 1 sec longer (once); Earth Shock, the next impact 15% more, half of it Nature.
    constexpr int32 GUARD_MS = 16000, LONG_GUARD_MS = 2000, SWIPE_GUARD_MS = 1000, URSOCS_MARK_MS = 6000;
    constexpr int32 CHALLENGE_MS = 3000, RENDING_MS = 6000, GUARD_END_MS = 150;
    constexpr int32 BARKSKIN_SOONER_MS = 30000, MAUL_SOONER_MS = 3000;
    constexpr float GUARD_PCT = 30.0f, EARTHEN_HIDE = 1.2f, ROOTS_PCT = 3.0f, RESTORING_CAP_PCT = 3.0f;
    constexpr float URSOC_AP = 0.75f, ABSORBED_SHARE = 1.0f, ABSORBED_MOST_AP = 2.5f, CRUSHING_CHAMPION = 1.3f;
    constexpr float UNBROKEN_CHAMPION = 1.25f, STONEGUARD = 1.2f, EARTHEN_FURY = 1.15f, THUNDERBEAR_AP = 0.2f;
    constexpr float MARK_AP = 0.2f, MARK_RADIUS = 8.0f, DEFIANT_THREAT = 0.5f, RENDING_SHARE = 0.3f;
    constexpr float RESTORING_SHARE = 0.2f, LIFEBLOOM_POWER = 0.4f;
    constexpr float URSOC_REACH = 10.0f, URSOC_ARC = float(M_PI) / 3.0f;
    constexpr std::size_t URSOC_TARGETS = 5, PACKBREAKER_TARGETS = 8, MARK_TARGETS = 4;

    // The Duelist's modifiers (its Extended Options, a redesign): slot (0 Pistol, 1 Powder, 2 Footwork) x 5 + choice;
    // its picks reset (titan_runes_zz_extended_reset2.sql).
    enum DuelistModifier : uint8
    {
        MOD_HEAVY_CALIBER, MOD_GUNSLINGER, MOD_SCATTERSHOT, MOD_FLAME_PISTOL, MOD_STORM_PISTOL,
        MOD_POWDER_AND_STEEL, MOD_BETWEEN_THE_EYES, MOD_LOADED_POWDER, MOD_MARKED_SHOT, MOD_FROZEN_POWDER,
        MOD_QUICK_DRAW, MOD_CUT_AND_RUN, MOD_EFFICIENT_DUELIST, MOD_HEALING_POWDER, MOD_ARCANE_POWDER,
    };

    // The Duelist's tuning: every third direct Sabre Slash hit readies Opening Shot (8 sec, one held; Quick Draw:
    // every second); Pistol Shot spends it (10 Energy, 6 sec cooldown, 20 yards): 60% of attack power and a combo
    // point (the Extended Options' cadence and cost; our 60% kept over its 45%). Its interactions: after a Pistol
    // Shot, the next Sabre Slash within 6 sec 15% more and one more enemy within 5 yards for 40% (Loaded Steel);
    // Eviscerate with Opening Shot ready, the next Pistol Shot 10% more a combo point (50% at most); a Fire Blast
    // hit, the next Pistol Shot within 8 sec explodes for 20% of attack power plus 20% of spell power as Fire on up
    // to 3 enemies within 8 yards.
    constexpr uint8 OPENING_EVERY = 3, QUICK_DRAW_EVERY = 2;
    constexpr int32 OPENING_MS = 8000, PISTOL_COST = 10;
    constexpr uint32 FROZEN_POWDER_EVERY_MS = 8000, EFFICIENT_EVERY_MS = 6000, HEALING_POWDER_EVERY_MS = 10000;
    constexpr float PISTOL_AP = 0.6f, HEAVY_CALIBER = 1.4f, GUNSLINGER_SHARE = 0.65f, SCATTER_SHARE = 0.5f;
    constexpr float LOADED_STEEL = 1.15f, POWDER_AND_STEEL = 1.3f, LOADED_STEEL_SPLASH = 0.4f, MARKED_SHOT = 1.25f;
    constexpr float EVISCERATE_STEP = 0.1f, EVISCERATE_MOST = 0.5f, EYES_STEP = 0.15f, EYES_MOST = 0.75f;
    constexpr float POWDER_AP = 0.2f, POWDER_SP = 0.2f, FLAME_PISTOL = 1.35f, LOADED_POWDER = 1.25f;
    constexpr float STORM_PISTOL_POWER = 0.25f, FROZEN_POWDER_AP = 0.2f, HEALING_POWDER_POWER = 0.3f;
    constexpr float ARCANE_POWDER = 1.25f;
    constexpr float SWEEPING_RADIUS = 5.0f, SCATTER_REACH = 10.0f, SCATTER_ARC = float(M_PI) / 4.0f;
    constexpr float POWDER_RADIUS = 8.0f, FROZEN_POWDER_RADIUS = 6.0f;
    constexpr std::size_t SCATTER_TARGETS = 3, POWDER_TARGETS = 3, LOADED_POWDER_TARGETS = 5;
    constexpr std::size_t FROZEN_POWDER_TARGETS = 3;
    // Broadside (35 Energy, 20 sec, a finishing move): 3 pistol blasts 0.25 sec apart, each 30% of attack power
    // plus 5% a combo point to its target and up to 7 more within 8 yards (its tooltip shows each point's).
    constexpr uint32 BROADSIDE_BLASTS = 3, BROADSIDE_SPACING_MS = 250;
    constexpr float BROADSIDE_AP = 0.3f, BROADSIDE_AP_PER_POINT = 0.05f, BROADSIDE_RADIUS = 8.0f;
    constexpr std::size_t BROADSIDE_TARGETS = 8;

    // The Stormreaver's modifiers: slot (0 Storm, 1 Thunderlash, 2 Lightning Shield) x 3 + choice.
    // (Its Extended Options: slot x 5 + choice, the first three of each the ones it had.)
    enum StormreaverModifier : uint8
    {
        MOD_EYE_OF_THE_TEMPEST, MOD_FORKED_CATACLYSM, MOD_STORM_ROLLING, MOD_STORMHUNTER, MOD_FROZEN_FRONT,
        MOD_THUNDERHEAD, MOD_CHAINBREAKER, MOD_WHITE_HOT_CONDUCTOR, MOD_STORMHAMMER, MOD_FLAME_CONDUCTOR,
        MOD_OVERCHARGED_CORE, MOD_RIDE_THE_LIGHTNING, MOD_VIOLENT_GROUNDING, MOD_STATIC_STRIKE, MOD_WARSTORM,
    };

    // The Stormreaver's Extended Options. Its interactions: Earth Shock in the Stormfront, the next pulse 20% more (6
    // sec); Thunderlash on the Hero's Flame Shock target, the Flame Shock 2 sec longer (6 sec an application). New
    // options: Stormhunter (their Hunter's Mark target: 35% more and one more within 8 yards for 40%), Frozen Front
    // (Frost Shock in it: a frost trail, 10% of attack power a second to up to 3 for 4 sec, 8 sec apart),
    // Stormhammer (Shield Slam in it: 30% of attack power as Nature to its target and one more, 6 sec apart), Flame
    // Conductor (Thunderlash spreads their Flame Shock to one more within 8 yards, 10 sec apart), Static Strike
    // (Lightning Bolt in it: the next discharge in 6 sec strikes again for 40%), Warstorm (Thunder Clap in it: 25%
    // of attack power as Nature to up to 5 within 8 yards, 8 sec apart).
    constexpr int32 STORM_FS_EXTEND_MS = 2000, STORM_FS_MOST_MS = 6000;
    constexpr uint32 FROZEN_FRONT_EVERY_MS = 8000, FROZEN_FRONT_TICKS = 4, STORMHAMMER_EVERY_MS = 6000;
    constexpr uint32 FLAME_CONDUCTOR_EVERY_MS = 10000, WARSTORM_EVERY_MS = 8000;
    constexpr float STORM_SURGE = 1.2f, STORMHUNTER = 1.35f, STORMHUNTER_SHARE = 0.4f, FROZEN_FRONT_AP = 0.1f;
    constexpr float FROZEN_FRONT_RADIUS = 4.0f, STORMHAMMER_AP = 0.3f, STATIC_SHARE = 0.4f, WARSTORM_AP = 0.25f;
    constexpr float STORM_ECHO_RANGE = 8.0f;
    constexpr std::size_t FROZEN_FRONT_TARGETS = 3, WARSTORM_TARGETS = 5;

    // The Stormreaver's tuning: Stormstrike raises the Stormfront (16 sec, refreshed): a pulse every 2 sec, 25% of
    // attack power as Nature to up to 5 enemies within 8 yards. Lava Lash in it is Thunderlash (60% to its target,
    // arcs of 30% to 2 more within 8); melee abilities discharge Lightning Shield at their target once a second (the
    // shield keeps its charges meanwhile): its rank's damage, a third of spell power and 20% of attack power. The
    // dossier's 15%, 40% and 20%, raised.
    constexpr int32 STORMFRONT_MS = 16000, WHITE_HOT_MS = 6000, RIDE_MS = 3000;
    constexpr uint32 STORM_PULSE_MS = 2000, ROLLING_PULSE_MS = 1000, DISCHARGE_EVERY_MS = 1000;
    constexpr int32 RIDE_COOLDOWN_MS = 1000;
    constexpr float STORM_PULSE_AP = 0.25f, EYE_OF_THE_TEMPEST = 2.2f, ROLLING_SHARE = 0.7f;
    constexpr float THUNDERLASH_AP = 0.6f, THUNDERHEAD_AP = 1.2f, ARC_AP = 0.3f, WHITE_HOT_AP = 1.0f;
    constexpr float DISCHARGE_SP = 0.33f, DISCHARGE_AP = 0.2f, OVERCHARGED = 1.5f, GROUNDING_SHARE = 0.5f;
    constexpr float STORM_RADIUS = 8.0f, FORKED_RADIUS = 12.0f, THUNDERLASH_RANGE = 8.0f, GROUNDING_RADIUS = 8.0f;
    constexpr std::size_t STORM_TARGETS = 5, FORKED_TARGETS = 8, THUNDERLASH_ARCS = 2, CHAINBREAKER_ARCS = 4;
    constexpr std::size_t GROUNDING_TARGETS = 3;

    // Ember's Wrath's modifiers (its Extended Options, a redesign): slot (0 Ignition, 1 Claws, 2 Instinct) x 5 +
    // choice; its picks reset (titan_runes_zz_extended_reset2.sql). Burning Claws: the Kindled Claw, Blazing Strike.
    enum EmbersModifier : uint8
    {
        MOD_CINDERCLAW, MOD_WILDFIRE_CLAWS, MOD_LIVING_FLAME, MOD_FLAME_SHOCK_CLAWS, MOD_FROZEN_EMBERS,
        MOD_EMBER_RUSH, MOD_SMOLDERING_BITE, MOD_FIRESTORM_POUNCE, MOD_PHOENIX_CLAWS, MOD_WILDFIRE_SWIPE,
        MOD_ASHEN_RENEWAL, MOD_FLAME_TIPPED_CLAWS, MOD_PHOENIX_SCAR, MOD_HEALING_FLAME, MOD_BURNING_RAKE,
    };

    // Ember's Wrath's tuning: the Hero's own Fire Blast (its mana and cooldown), usable in Cat Form, ignites their
    // Rake or Rip victim: Cinder Wound, 150% of attack power plus 100% of spell power over 6 sec, a tick every 2 sec
    // (a new one keeps what is left of the last, up to twice a fresh one). The dossier's 30% and 30% (and its other
    // shares, five times or so over) made the rune 2-5% of a feral's damage.
    constexpr int32 CINDER_MS = 6000, EMBER_RUSH_MS = 4000, EMBER_RUSH_DISCOUNT_MS = 6000, BITE_EXTEND_MS = 1000;
    constexpr int32 BITE_EXTEND_MOST_MS = 5000, EMBER_REFUND = 10, ASHEN_ENERGY = 10, BURNING_RAKE_MS = 6000;
    constexpr uint32 SHRED_EMBER_EVERY_MS = 3000, FLAME_SPREAD_EVERY_MS = 8000, FROZEN_EMBERS_EVERY_MS = 6000;
    constexpr uint32 PHOENIX_CLAWS_EVERY_MS = 8000, WILDFIRE_SWIPE_EVERY_MS = 8000, ASHEN_EVERY_MS = 8000;
    constexpr uint32 HEALING_FLAME_EVERY_MS = 10000;
    constexpr float CINDER_AP = 1.5f, CINDER_SP = 1.0f, CINDERCLAW = 1.4f, LIVING_FLAME = 1.15f;
    // Its interactions: Shred on a Cinder Wound, 10% of attack power as Fire (3 sec a target); Ferocious Bite, the
    // wound 1 sec longer a combo point (5 sec an application); Fire Blast on the Hero's Flame Shock and Cinder Wound
    // target spreads the wound, what is left of it, to one more within 8 yards (8 sec apart). The options as the
    // dossier has them (our wound's 150% and 100% kept over its 40% and 40%).
    constexpr float SHRED_EMBER_AP = 0.1f, WILDFIRE_CLAWS_SHARE = 0.5f, WILDFIRE_ALONE = 0.25f, FS_CLAWS = 0.25f;
    constexpr float FROZEN_EMBERS_AP = 0.25f, PHOENIX_CLAWS_POWER = 0.4f, WILDFIRE_SWIPE_SHARE = 0.5f;
    constexpr float FLAME_TIPPED = 0.4f, PHOENIX_SCAR_AP = 0.25f, HEALING_FLAME_PCT = 2.0f, BURNING_RAKE = 0.3f;
    constexpr float EMBER_SPREAD_RANGE = 8.0f;
    constexpr std::size_t WILDFIRE_CLAWS_TARGETS = 2, FIRESTORM_TARGETS = 3;

    // Whitefang's modifiers (its Extended Options, a redesign): slot (0 Shatter, 1 Hunt, 2 Instinct) x 5 + choice;
    // its picks reset (titan_runes_zz_extended_reset3.sql).
    enum WhitefangModifier : uint8
    {
        MOD_PREDATORS_FANG, MOD_PACK_OF_WINTER, MOD_GLACIAL_CLAWS, MOD_STORMFANG, MOD_FLAMEFANG,
        MOD_FROZEN_WOUND, MOD_FANG_COLD_BLOOD, MOD_PREDATORS_CHILL, MOD_MARKED_PREY, MOD_MOONFROST_FANG,
        MOD_SWIFT_HUNT, MOD_WINTERS_RECOVERY, MOD_LINGERING_FROST, MOD_MENDING_FANG, MOD_CLAWING_COLD,
    };

    // Whitefang's tuning: the Hero's own Icy Touch (its Frost rune), in Cat Form or not, lays Frozen Prey for 16 sec;
    // the next Shred or Mangle on the victim shatters it for 150% of attack power as Frost damage (kept over the
    // Extended Options' 45%). Its interactions: with the Hero's Rake on it, the shatter leaves Frozen Wound (25% of
    // attack power over 6 sec); Ferocious Bite on Frozen Wound deals its next tick at once; a Frostbolt on Frozen
    // Prey, the shatter 25% more and the mark 2 sec longer (once a mark).
    constexpr int32 PREY_MS = 16000, WINTER_WOUND_MS = 6000, PACK_FROST_MS = 6000, FROSTBOLT_PREY_MS = 2000;
    constexpr int32 SWIFT_HUNT_MS = 4000, LINGERING_MS = 3000, FANG_REFUND = 10, FANG_MOONFIRE_MS = 2000;
    constexpr int32 FANG_MOONFIRE_MOST_MS = 6000;
    constexpr uint32 RECOVERY_EVERY_MS = 6000, MENDING_EVERY_MS = 10000;
    constexpr float SHATTER_AP = 1.5f, WHITEFANG_BONUS = 1.4f, ICEFANG_SHARE = 0.5f, PACK_FROST_SHARE = 0.5f;
    constexpr float FROSTBOLT_CHILL = 1.25f, PREDATORS_CHILL = 1.4f, FROZEN_WOUND_AP = 0.25f, FROZEN_WOUND_MORE = 1.5f;
    constexpr float FANG_COLD_BLOOD_AP = 0.1f, FANG_SPELL_POWER = 0.2f, MENDING_PCT = 2.0f, PACK_FROST_RADIUS = 8.0f;
    constexpr std::size_t GLACIAL_CLAWS_TARGETS = 3;

    // The shocks' empowered strikes (both runes): Flame Shock or Frost Shock (in Cat Form too) empowers the next Claw
    // or Swipe within 10 sec. Claw: a second enemy within 5 yards takes its hit again (Fire or Frost), and both
    // bleed (100% of attack power over 6 sec: a bleed Fire Blast ignites) or are frostbitten (100% over 6 sec, 30%
    // slower). Swipe: 75% of attack power to up to 8 enemies within 8 yards, each burning (half a Cinder Wound) or
    // slowed 50% for 4 sec, a Frozen Prey on any of them shattered.
    constexpr int32 EMPOWER_MS = 10000, CLAW_DOT_MS = 6000, FROSTBITE_SLOW_MS = 6000, GLACIAL_SLOW_MS = 4000;
    constexpr float CLAW_SECOND_SHARE = 1.0f, CLAW_DOT_AP = 1.0f, NOVA_AP = 0.75f, FIERY_WOUND_SHARE = 0.5f;
    constexpr float CLAW_SECOND_RADIUS = 5.0f, NOVA_RADIUS = 8.0f;
    constexpr std::size_t NOVA_TARGETS = 8;

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

    constexpr uint32 RUNE_WATCH_MS = 5000;

    constexpr uint32 NPC_RUNE_FELGUARD = 910120;
    constexpr uint32 NPC_RUNE_FEL_IMP = 910121;
    constexpr uint32 NPC_HOLLOW_ECHO = 910122;
    constexpr uint32 NPC_PHANTOM_ASSASSIN = 910124;
    constexpr uint32 PHANTOM_LINGER_MS = 900;                   // after its blow, then it vanishes
    constexpr float PHANTOM_BEHIND = 1.5f;                      // yards past the victim, away from the Hero
    constexpr uint32 NPC_SPECTRAL_BEAST = 910123;
    constexpr uint32 NPC_RUNE_PHOENIX = 910125;
    constexpr uint32 NPC_RUNE_ARCANE_ORB = 910126;
    // SummonProperties 61: category ally, type guardian. A pet-category row (67) would make a controllable guardian
    // that takes the pet slot from the Hero's own demon.
    constexpr uint32 SUMMON_PROPERTIES_GUARDIAN = 61;
    // The warlock's demons (creature entries of their summons), whose hits feed the rune.
    constexpr std::array<uint32, 5> DEMONS = { 416, 1860, 1863, 417, 17252 };

    // Tuning (the doc's table).
    constexpr uint8 MAX_STACKS = 5;
    constexpr uint32 DEMON_STACK_INTERVAL_MS = 4000;
    constexpr uint32 INFERNAL_PACT_INTERVAL_MS = 2000;
    constexpr uint32 BOLTS = 3, BARRAGE_BOLTS_MORE = 2, PACT_OF_RUIN_BOLTS = 2;
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
        uint32 MarkerFirst;         // its modifiers' markers: + slot * Choices + choice
        uint8 Choices = CHOICES;    // a slot's choices: 3, or 5 once the rune has its Extended Options
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
              { { SPELL_CONSECRATION } }, SPELL_RUNE_SANCTIFIED_PARAGON, SPELL_PARAGON_MODIFIER_FIRST, MAX_CHOICES },
            // DPS only: Lightning Bolt makes the connections, Lightning Shield's retaliations (and Chain Lightning,
            // if known) arc through them.
            { RUNE_STORMBOUND_CONDUIT,
              uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE) | Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_LIGHTNING_BOLT }, { SPELL_CHAIN_LIGHTNING } }, SPELL_RUNE_STORMBOUND_CONDUIT,
              SPELL_STORM_MODIFIER_FIRST, MAX_CHOICES },
            // Tanks: Thunder Clap places the storm, Shield Slam echoes it (and needs the shield the rune asks for).
            { RUNE_ECHOING_THUNDER, uint8(Wildcard::RoleMask(Wildcard::ROLE_TANK)),
              { { SPELL_THUNDER_CLAP }, { SPELL_SHIELD_SLAM } }, SPELL_RUNE_ECHOING_THUNDER,
              SPELL_THUNDER_MODIFIER_FIRST, MAX_CHOICES },
            // Healers: Chain Heal through the Earth Shield bearer's Wellspring; Soothing Mist's channel.
            { RUNE_WELLSPRING, uint8(Wildcard::RoleMask(Wildcard::ROLE_HEALER)),
              { { SPELL_CHAIN_HEAL }, { SPELL_EARTH_SHIELD } }, SPELL_RUNE_WELLSPRING,
              SPELL_WELLSPRING_MODIFIER_FIRST, MAX_CHOICES },
            { RUNE_WANDERING_MIST, uint8(Wildcard::RoleMask(Wildcard::ROLE_HEALER)),
              { { SPELL_SOOTHING_MIST } }, SPELL_RUNE_WANDERING_MIST, SPELL_MIST_MODIFIER_FIRST,
              MAX_CHOICES },
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
              { { SPELL_CRUSADER_STRIKE, SPELL_HOLY_SHOCK },
                { SPELL_JUDGEMENT_OF_LIGHT, SPELL_JUDGEMENT_OF_WISDOM, SPELL_JUDGEMENT_OF_JUSTICE } },
              SPELL_RUNE_DAWNBOUND_CRUSADER, SPELL_DAWN_MODIFIER_FIRST, MAX_CHOICES },
            // Ranged DPS: Mind Flay leaves the echoes, Mind Blast commands them.
            { RUNE_HOLLOW_CONGREGATION, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_MIND_FLAY }, { SPELL_MIND_BLAST } }, SPELL_RUNE_HOLLOW_CONGREGATION,
              SPELL_HOLLOW_MODIFIER_FIRST, MAX_CHOICES },
            // Ranged DPS: a fire spell embeds and collapses, a frost spell fractures.
            { RUNE_SHATTERED_SUN, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_FIREBALL }, { SPELL_FROSTBOLT } }, SPELL_RUNE_SHATTERED_SUN,
              SPELL_SUN_MODIFIER_FIRST, MAX_CHOICES },
            // Melee DPS: Eviscerate's phantom.
            { RUNE_SPECTRAL_BLADE, uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE)),
              { { SPELL_EVISCERATE } }, SPELL_RUNE_SPECTRAL_BLADE, SPELL_BLADE_MODIFIER_FIRST, MAX_CHOICES },
            // DPS: Kill Command or Bestial Wrath (a pet's) unleashes the beasts.
            { RUNE_WILD_HUNT,
              uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE) | Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_KILL_COMMAND, SPELL_BESTIAL_WRATH } }, SPELL_RUNE_WILD_HUNT, SPELL_HUNT_MODIFIER_FIRST,
              MAX_CHOICES },
            // Ranged DPS: Arcane Blast builds the Insight, Arcane Missiles spends it.
            { RUNE_ARCANE_MIND, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_ARCANE_BLAST }, { SPELL_ARCANE_MISSILES } }, SPELL_RUNE_ARCANE_MIND,
              SPELL_MIND_MODIFIER_FIRST, MAX_CHOICES },
            // Ranged DPS: Frostbolt's Icicles.
            { RUNE_SPLINTERED_CROWN, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_FROSTBOLT } }, SPELL_RUNE_SPLINTERED_CROWN, SPELL_CROWN_MODIFIER_FIRST, MAX_CHOICES },
            // Ranged DPS: Pyroblast leaves the Ember, Fire Blast wakes the phoenix.
            { RUNE_PHOENIX_WAKE, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_PYROBLAST }, { SPELL_FIRE_BLAST } }, SPELL_RUNE_PHOENIX_WAKE,
              SPELL_PHOENIX_MODIFIER_FIRST, MAX_CHOICES },
            // Ranged DPS: Arcane Barrage sets the Orb loose.
            { RUNE_ASTRAL_HORIZON, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_ARCANE_BARRAGE } }, SPELL_RUNE_ASTRAL_HORIZON,
              SPELL_HORIZON_MODIFIER_FIRST, MAX_CHOICES },
            // Healers: Prayer of Mending places the star, Flash Heal or Greater Heal moves it.
            { RUNE_GUIDING_LIGHT, uint8(Wildcard::RoleMask(Wildcard::ROLE_HEALER)),
              { { SPELL_PRAYER_OF_MENDING }, { SPELL_FLASH_HEAL, SPELL_GREATER_HEAL } }, SPELL_RUNE_GUIDING_LIGHT,
              SPELL_GUIDING_MODIFIER_FIRST, MAX_CHOICES },
            // Ranged DPS: Starfire calls the owls.
            { RUNE_ELUNES_GUARDIAN, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_STARFIRE } }, SPELL_RUNE_ELUNES_GUARDIAN, SPELL_OWL_MODIFIER_FIRST, MAX_CHOICES },
            // Ranged DPS: Wrath and Starfire in turn.
            { RUNE_ASTRAL_CONVERGENCE, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_WRATH }, { SPELL_STARFIRE } }, SPELL_RUNE_ASTRAL_CONVERGENCE,
              SPELL_CONVERGENCE_MODIFIER_FIRST, MAX_CHOICES },
            // Tanks: Barkskin in Bear Form raises the guard, Maul releases it.
            { RUNE_URSOCS_CHAMPION, uint8(Wildcard::RoleMask(Wildcard::ROLE_TANK)),
              { { SPELL_BEAR_FORM, SPELL_DIRE_BEAR_FORM }, { SPELL_BARKSKIN }, { SPELL_MAUL } },
              SPELL_RUNE_URSOCS_CHAMPION, SPELL_URSOC_MODIFIER_FIRST, MAX_CHOICES },
            // Melee DPS: Sinister Strike readies the pistol (the rune teaches Pistol Shot).
            { RUNE_DUELIST, uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE)),
              { { SPELL_SINISTER_STRIKE } }, SPELL_RUNE_DUELIST, SPELL_DUELIST_MODIFIER_FIRST, MAX_CHOICES },
            // Melee DPS: Stormstrike raises the storm, Lava Lash and Lightning Shield strike through it.
            { RUNE_STORMREAVER, uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE)),
              { { SPELL_STORMSTRIKE }, { SPELL_LAVA_LASH }, { SPELL_LIGHTNING_SHIELD } }, SPELL_RUNE_STORMREAVER,
              SPELL_STORMREAVER_MODIFIER_FIRST, MAX_CHOICES },
            // Melee DPS: Fire Blast in Cat Form ignites the Hero's Rake or Rip (the rune teaches it).
            { RUNE_EMBERS_WRATH, uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE)),
              { { SPELL_CAT_FORM }, { SPELL_FIRE_BLAST }, { SPELL_RAKE, SPELL_RIP } }, SPELL_RUNE_EMBERS_WRATH,
              SPELL_EMBERS_MODIFIER_FIRST, MAX_CHOICES },
            // Melee DPS: Icy Touch in Cat Form marks the prey, Shred or Mangle shatters it (the rune teaches it).
            { RUNE_WHITEFANG, uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE)),
              { { SPELL_CAT_FORM }, { SPELL_ICY_TOUCH }, { SPELL_SHRED, SPELL_MANGLE_CAT } }, SPELL_RUNE_WHITEFANG,
              SPELL_WHITEFANG_MODIFIER_FIRST, MAX_CHOICES },
            // The Tank Collection (src/tank_runes.cpp): Blood Boil links the pack; a Shock commands the totem; the
            // Voidwalker and Searing Pain make the covenant.
            { RUNE_RED_COMMUNION, uint8(Wildcard::RoleMask(Wildcard::ROLE_TANK)),
              { { TankRunes::SPELL_BLOOD_BOIL } }, TankRunes::SPELL_RUNE_RED_COMMUNION,
              TankRunes::SPELL_COMMUNION_MODIFIER_FIRST, MAX_CHOICES },
            { RUNE_PRIMAL_WORLDWARDEN, uint8(Wildcard::RoleMask(Wildcard::ROLE_TANK)),
              { { TankRunes::SPELL_EARTH_SHOCK, SPELL_FLAME_SHOCK, SPELL_FROST_SHOCK },
                { TankRunes::SPELL_STONESKIN_TOTEM } },
              TankRunes::SPELL_RUNE_PRIMAL_WORLDWARDEN, TankRunes::SPELL_WORLDWARDEN_MODIFIER_FIRST, MAX_CHOICES },
            { RUNE_BLACK_COVENANT, uint8(Wildcard::RoleMask(Wildcard::ROLE_TANK)),
              { { SPELL_SUMMON_VOIDWALKER }, { TankRunes::SPELL_SEARING_PAIN } },
              TankRunes::SPELL_RUNE_BLACK_COVENANT, TankRunes::SPELL_COVENANT_MODIFIER_FIRST, MAX_CHOICES },
            // Ranged DPS: Shadow Bolt on the Hero's Corruption opens the Rift.
            { RUNE_HUNGERING_RIFT, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_SHADOW_BOLT }, { SPELL_CORRUPTION } }, SPELL_RUNE_HUNGERING_RIFT, SPELL_RIFT_MODIFIER_FIRST,
              MAX_CHOICES },
            // Melee DPS: Howling Blast marks, Obliterate looses the blade.
            { RUNE_HOWLING_NORTH, uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE)),
              { { SPELL_HOWLING_BLAST }, { SPELL_OBLITERATE } }, SPELL_RUNE_HOWLING_NORTH, SPELL_NORTH_MODIFIER_FIRST,
              MAX_CHOICES },
            // Ranged DPS: Aimed Shot becomes Trueshot.
            { RUNE_TRUESHOT, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_AIMED_SHOT } }, SPELL_RUNE_TRUESHOT, SPELL_TRUESHOT_MODIFIER_FIRST, MAX_CHOICES },
            // Healer: Renew plants the Bloom, Flash Heal or Greater Heal opens it.
            { RUNE_SCARLET_BENEDICTION, uint8(Wildcard::RoleMask(Wildcard::ROLE_HEALER)),
              { { SPELL_RENEW }, { SPELL_FLASH_HEAL, SPELL_GREATER_HEAL } }, SPELL_RUNE_SCARLET_BENEDICTION,
              SPELL_SCARLET_MODIFIER_FIRST, MAX_CHOICES },
            // Healer: Rejuvenation stacks, and seeks the injured.
            { RUNE_EVERGROVE, uint8(Wildcard::RoleMask(Wildcard::ROLE_HEALER)),
              { { SPELL_REJUVENATION } }, SPELL_RUNE_EVERGROVE, SPELL_GROVE_MODIFIER_FIRST, MAX_CHOICES },
            // Tank: Shield Block raises the bulwark, Shield Slam releases it.
            { RUNE_IRON_RECKONING, uint8(Wildcard::RoleMask(Wildcard::ROLE_TANK)),
              { { SPELL_SHIELD_BLOCK }, { SPELL_SHIELD_SLAM } }, SPELL_RUNE_IRON_RECKONING, SPELL_IRON_MODIFIER_FIRST,
              MAX_CHOICES },
        };
        return runes;
    }

    // Runes taken out while their design is looked at again (too strong once online): never offered nor bound, and a
    // Hero who had one bound has it released at login, free to choose another. Their auras stay in the catalog, so
    // Apply still takes them off.
    constexpr uint8 RETIRED_RUNES[] = { RUNE_FEL_COMMANDMENT, RUNE_THREEFOLD_GRAVE, RUNE_UNBROKEN_ONSLAUGHT };

    bool IsRetired(uint8 id)
    {
        return std::find(std::begin(RETIRED_RUNES), std::end(RETIRED_RUNES), id) != std::end(RETIRED_RUNES);
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
        RuneDef const* def = FindRune(rune);
        uint8 modifier = slot * (def ? def->Choices : CHOICES) + choice;
        if (rune == RUNE_HUNGERING_RIFT)
            switch (modifier)
            {
                case MOD_HELLMAW: return { { SPELL_IMMOLATE } };
                case MOD_HAUNTED_MAW: return { { SPELL_HAUNT } };
                case MOD_PAINFUL_ANCHORING: return { { SPELL_SHADOW_WORD_PAIN } };
                case MOD_SIPHON_CONDUIT: return { { TankRunes::SPELL_DRAIN_LIFE } };
                case MOD_DOOM_ECHO: return { { SPELL_CURSE_OF_AGONY } };
                case MOD_NECROTIC_APPETITE: return { { SPELL_DEATH_COIL_DK, SPELL_DEATH_COIL_WARLOCK } };
                default: break;
            }
        if (rune == RUNE_HOWLING_NORTH)
            switch (modifier)
            {
                case MOD_RIMEBLADE: case MOD_WINTERS_RETURN: case MOD_NORTHERN_ENDURANCE: case MOD_GLACIAL_FLOW:
                    return { { SPELL_FROST_STRIKE } };
                case MOD_SHATTERING_COLD: return { { SPELL_HUNGERING_COLD } };
                case MOD_PLAGUEWIND: return { { SPELL_PLAGUE_STRIKE } };
                case MOD_SHARPENED_WIND: return { { SPELL_ICY_TOUCH } };
                case MOD_FROZEN_GROUND: return { { SPELL_CHAINS_OF_ICE } };
                case MOD_HORN_OF_THE_NORTH: return { { SPELL_HORN_OF_WINTER } };
                case MOD_DECAYING_ICE: return { { SPELL_DEATH_AND_DECAY } };
                case MOD_WINTERS_MENDING: return { { SPELL_DEATH_STRIKE } };
                default: break;
            }
        if (rune == RUNE_SCARLET_BENEDICTION)
            switch (modifier)
            {
                case MOD_SCARLET_CIRCLING_BLOOM: return { { SPELL_CIRCLE_OF_HEALING } };
                case MOD_SCARLET_SHIELDED_BLOOM: return { { SPELL_POWER_WORD_SHIELD } };
                case MOD_SCARLET_CHORAL_BLOOM: return { { SPELL_PRAYER_OF_HEALING } };
                case MOD_SCARLET_MENDING_PETALS: return { { SPELL_PRAYER_OF_MENDING } };
                case MOD_SCARLET_GUARDIAN_BLOOM: return { { SPELL_GUARDIAN_SPIRIT } };
                case MOD_SCARLET_BINDING_PETALS: return { { SPELL_BINDING_HEAL } };
                default: break;
            }
        if (rune == RUNE_EVERGROVE)
            switch (modifier)
            {
                case MOD_GROVE_REGROWTHS_ROOT: return { { SPELL_REGROWTH } };
                case MOD_GROVE_NOURISHING: return { { SPELL_NOURISH } };
                case MOD_GROVE_THORNED: return { { SPELL_THORNS } };
                case MOD_GROVE_WILD_SEEDING: return { { SPELL_WILD_GROWTH } };
                case MOD_GROVE_LIFEBOUND: return { { SPELL_LIFEBLOOM } };
                case MOD_GROVE_TRANQUIL: return { { SPELL_TRANQUILITY } };
                case MOD_GROVE_SWIFT: return { { SPELL_NATURES_SWIFTNESS_DRUID } };
                case MOD_GROVE_VERDANT_SWIFTMEND: case MOD_GROVE_OVERGROWTH: return { { SPELL_SWIFTMEND } };
                case MOD_GROVE_ROOTED_TOUCH: return { { SPELL_HEALING_TOUCH } };
                case MOD_GROVE_INNERVATING: return { { SPELL_INNERVATE } };
                default: break;
            }
        if (rune == RUNE_IRON_RECKONING)
            switch (modifier)
            {
                case MOD_IRON_SHOCKWAVE: return { { SPELL_SHOCKWAVE } };
                case MOD_IRON_CONCUSSIVE: return { { SPELL_CONCUSSION_BLOW } };
                case MOD_IRON_RETALIATION: return { { SPELL_REVENGE } };
                case MOD_IRON_SHAKING_GROUND: return { { SPELL_THUNDER_CLAP } };
                case MOD_IRON_SUNDERING_FORCE: return { { SPELL_DEVASTATE } };
                case MOD_IRON_VENGEFUL_DEVASTATION: return { { SPELL_DEVASTATE }, { SPELL_REVENGE } };
                case MOD_IRON_RENDING_IMPACT: return { { SPELL_REND } };
                case MOD_IRON_LAST_BASTION: return { { SPELL_LAST_STAND } };
                case MOD_IRON_WALL_OF_IRON: return { { SPELL_SHIELD_WALL } };
                default: break;
            }
        if (rune == RUNE_TRUESHOT)
            switch (modifier)
            {
                case MOD_VENOM_ARROW: return { { SPELL_SERPENT_STING } };
                case MOD_CONCUSSIVE_PIERCE: return { { SPELL_CONCUSSIVE_SHOT } };
                case MOD_WIDE_TRAJECTORY: return { { SPELL_MULTI_SHOT } };
                case MOD_ARCANE_ARROW: case MOD_ARCANE_CALIBRATION: return { { SPELL_SILENCING_SHOT } };
                case MOD_MARKED_RICOCHET: return { { SPELL_HUNTERS_MARK } };
                case MOD_QUICK_RECOVERY: case MOD_FOLLOW_UP_SHOT: return { { SPELL_ARCANE_SHOT } };
                case MOD_STEADY_RECOVERY: return { { SPELL_STEADY_SHOT } };
                case MOD_MENDING_AIM: return { { SPELL_MEND_PET } };
                default: break;
            }
        if (rune == RUNE_SHATTERED_SUN)
            switch (modifier)
            {
                case MOD_LUNAR_SHARD: return { { SPELL_MOONFIRE } };
                case MOD_STORM_SHARD: return { { SPELL_LIGHTNING_BOLT } };
                case MOD_FROZEN_FLASH: case MOD_SHATTERING_LANCE: return { { SPELL_ICE_LANCE } };
                case MOD_FLAME_BURST: return { { SPELL_FLAME_SHOCK } };
                case MOD_ICY_SHELTER: return { { SPELL_ICE_BARRIER } };
                default: break;
            }
        if (rune == RUNE_STORMBOUND_CONDUIT)
            switch (modifier)
            {
                case MOD_LIVE_WIRE: case MOD_STORM_WITHIN: case MOD_VIOLENT_REPRISAL: case MOD_DISTANT_CONDUCTOR:
                    return { { SPELL_LIGHTNING_SHIELD } };
                case MOD_ICY_CONDUCTOR: return { { SPELL_FROSTBOLT } };
                case MOD_STORM_FLAME_CONDUCTOR: return { { SPELL_FLAME_SHOCK } };
                case MOD_STORM_LASH: return { { SPELL_LAVA_LASH } };
                case MOD_STARBOUND_CIRCUIT: return { { SPELL_STARFIRE } };
                case MOD_STORMS_EYE: return { { SPELL_CHAIN_HEAL } };
                case MOD_SHOCKING_SURGE: return { { SPELL_THUNDER_CLAP } };
                default: break;
            }
        if (rune == RUNE_ECHOING_THUNDER)
            switch (modifier)
            {
                case MOD_STORMHEART: return { { SPELL_LIGHTNING_SHIELD } };
                case MOD_CONSECRATED_STORM: return { { SPELL_CONSECRATION } };
                case MOD_FLAME_ECHO: return { { SPELL_FLAME_SHOCK } };
                case MOD_FROST_ECHO: return { { SPELL_ICY_TOUCH } };
                case MOD_REVENGE_OF_THE_STORM: return { { SPELL_REVENGE } };
                case MOD_TIDAL_SHIELD: return { { SPELL_EARTH_SHIELD } };
                default: break;
            }
        if (rune == RUNE_WELLSPRING)
            switch (modifier)
            {
                case MOD_RIPTIDE_SPRING: case MOD_TIDAL_SURGE: return { { SPELL_RIPTIDE } };
                case MOD_RENEWING_SPRING: return { { SPELL_RENEW } };
                case MOD_GUIDED_CURRENT: return { { SPELL_HOLY_SHOCK } };
                case MOD_JADE_CURRENT: return { { SPELL_SOOTHING_MIST } };
                case MOD_SHIELDED_SPRING: return { { SPELL_POWER_WORD_SHIELD } };
                default: break;
            }
        if (rune == RUNE_WANDERING_MIST)
            switch (modifier)
            {
                case MOD_COMFORTING_VEIL: case MOD_PEACEFUL_FOCUS: return { { SPELL_ENVELOPING_MIST } };
                case MOD_VIVACIOUS_FLOW: return { { SPELL_VIVIFY } };
                case MOD_RENEWING_MIST: case MOD_RENEWING_BREATH: return { { SPELL_RENEW } };
                case MOD_TIDAL_MIST: return { { SPELL_RIPTIDE } };
                case MOD_HOLY_THREAD: return { { SPELL_HOLY_SHOCK } };
                case MOD_CHAIN_OF_JADE: return { { SPELL_CHAIN_HEAL } };
                case MOD_EARTHEN_COCOON: return { { SPELL_EARTH_SHIELD } };
                default: break;
            }
        if (rune == RUNE_DAWNBOUND_CRUSADER)
            switch (modifier)
            {
                case MOD_EARTHEN_OATH: return { { SPELL_EARTH_SHIELD } };
                case MOD_CRUSADERS_FLAME: return { { SPELL_FLAME_SHOCK } };
                case MOD_FROSTFIRE_DAWN: return { { SPELL_FROSTBOLT } };
                case MOD_CHAIN_OF_GRACE: return { { SPELL_CHAIN_HEAL } };
                case MOD_STEADFAST: return { { SPELL_HOLY_SHOCK } };
                case MOD_RENEWED_OATH: return { { SPELL_RENEW } };
                case MOD_SEARING_VOW: return { { SPELL_LAVA_LASH } };
                default: break;
            }
        if (rune == RUNE_HOLLOW_CONGREGATION && modifier == MOD_DISSONANCE)
            return { { SPELL_SHADOW_WORD_PAIN } };
        if (rune == RUNE_WILD_HUNT)
            switch (modifier)
            {
                case MOD_MARKED_ALPHA: case MOD_MARKED_FINALE: return { { SPELL_HUNTERS_MARK } };
                case MOD_FROSTFANG: return { { SPELL_ICY_TOUCH } };
                case MOD_ARCANE_HUNT: return { { SPELL_ARCANE_SHOT } };
                case MOD_STORM_HUNT: return { { SPELL_LIGHTNING_BOLT } };
                case MOD_HEALING_COMPANION: return { { SPELL_MEND_PET } };
                default: break;
            }
        if (rune == RUNE_ARCANE_MIND && modifier == MOD_EXPANDING_INTELLECT)
            return { { SPELL_ARCANE_EXPLOSION } };
        if (rune == RUNE_GUIDING_LIGHT && modifier == MOD_GUARDIAN_STAR)
            return { { SPELL_GREATER_HEAL } };
        if (rune == RUNE_ELUNES_GUARDIAN && modifier == MOD_TWILIGHT_HUNT)
            return { { SPELL_WRATH } };
        if (rune == RUNE_ARCANE_MIND)
            switch (modifier)
            {
                case MOD_STORM_CONDUIT: return { { SPELL_LIGHTNING_BOLT } };
                case MOD_STARFALL_EQUATION: return { { SPELL_STARFIRE } };
                case MOD_MISSILE_SHARDS: return { { SPELL_FROSTBOLT } };
                case MOD_ARCANE_CHARGE: return { { SPELL_ARCANE_EXPLOSION } };
                case MOD_EVOCATIVE_MIND: return { { SPELL_EVOCATION } };
                case MOD_SHOCKING_CLARITY: return { { SPELL_LIGHTNING_SHIELD } };
                default: break;
            }
        if (rune == RUNE_PHOENIX_WAKE)
            switch (modifier)
            {
                case MOD_FLAMEKEEPER: case MOD_WILDFIRE_RETURN: return { { SPELL_FLAME_SHOCK } };
                case MOD_LAVA_FLOCK: return { { SPELL_LAVA_BURST } };
                case MOD_BLAZING_PATH: return { { SPELL_FLAMESTRIKE } };
                case MOD_HOT_PURSUIT: return { { SPELL_SCORCH } };
                case MOD_EMBERGUARD: return { { SPELL_FIRE_WARD } };
                default: return {};
            }
        if (rune == RUNE_ASTRAL_HORIZON)
            switch (modifier)
            {
                case MOD_ORBITAL_LIGHTNING: return { { SPELL_LIGHTNING_BOLT } };
                case MOD_STARWARD_SATELLITE: return { { SPELL_MOONFIRE } };
                case MOD_SHATTERING_ORBIT: return { { SPELL_FROST_NOVA } };
                case MOD_DISTANT_SPARK: return { { SPELL_ARCANE_SHOT } };
                case MOD_EVOKED_REMNANT: return { { SPELL_EVOCATION } };
                case MOD_ASTRAL_SHELTER: return { { SPELL_POWER_WORD_SHIELD } };
                default: return {};
            }
        // The Tank Collection's (tank_runes.cpp's enums, by number: slot x 5 + choice).
        if (rune == RUNE_RED_COMMUNION)
            switch (modifier)
            {
                case 3: return { { SPELL_CORRUPTION } };                   // Corrupt Communion
                case 4: return { { SPELL_MANGLE_BEAR } };                  // Thorned Communion
                case 8: return { { SPELL_REND } };                         // Rending Communion
                case 9: return { { SPELL_EARTH_SHIELD } };                 // Earthbound Blood
                case 13: return { { SPELL_DEATH_COIL_DK, SPELL_DEATH_COIL_WARLOCK } };    // Death's Recall
                case 14: return { { SPELL_HOLY_SHOCK } };                  // Holy Communion
                default: return {};
            }
        if (rune == RUNE_BLACK_COVENANT)
            switch (modifier)
            {
                case 3: return { { SPELL_BLOOD_STRIKE } };                 // Blood Pact
                case 4: return { { SPELL_EARTH_SHIELD } };                 // Earthen Pact
                case 5: case 6: case 7: return { { TankRunes::SPELL_DRAIN_LIFE } };     // the Siphon's
                case 8: return { { TankRunes::SPELL_DRAIN_LIFE }, { SPELL_CORRUPTION } };     // Corrupted Drain
                case 9: return { { TankRunes::SPELL_DRAIN_LIFE }, { SPELL_ICY_TOUCH } };      // Frostbound Siphon
                case 13: return { { SPELL_JUDGEMENT_OF_LIGHT, SPELL_JUDGEMENT_OF_WISDOM, SPELL_JUDGEMENT_OF_JUSTICE } };
                case 14: return { { TankRunes::SPELL_BLOOD_BOIL } };       // Blood Intercession
                default: return {};
            }
        if (rune == RUNE_PRIMAL_WORLDWARDEN)
            switch (modifier)
            {
                case 0: return { { TankRunes::SPELL_EARTH_SHOCK } };       // Earthen Command
                case 3: return { { TankRunes::SPELL_EARTH_SHOCK }, { SPELL_BLOOD_STRIKE } };     // Bloodstone
                case 1: return { { SPELL_FLAME_SHOCK } };                  // Flame Dominion
                case 2: return { { SPELL_FROST_SHOCK } };                  // Winter Command
                case 4: return { { SPELL_JUDGEMENT_OF_LIGHT, SPELL_JUDGEMENT_OF_WISDOM, SPELL_JUDGEMENT_OF_JUSTICE } };
                case 8: return { { SPELL_CONSECRATION } };                 // Consecrated Heart
                case 9: return { { SPELL_EARTH_SHIELD } };                 // Earthen Communion
                case 10: return { { SPELL_STORMSTRIKE } };                 // Resonant Strikes
                case 11: case 12: return { { SPELL_LAVA_LASH } };          // Molten Causeway, Stoneblood Lash
                case 13: return { { SPELL_THUNDER_CLAP } };                // Thunderheart
                case 14: return { { SPELL_DEATH_COIL_DK, SPELL_DEATH_COIL_WARLOCK } };    // Runic Renewal
                default: return {};
            }
        if (rune == RUNE_SANCTIFIED_PARAGON)
            switch (modifier)
            {
                case MOD_CONSECRATED_CHARGE: return { { SPELL_CHARGE } };
                case MOD_SANCTUARY_MARCH: return { { SPELL_DIVINE_SHIELD } };
                case MOD_RADIANT_JUDGMENT:
                    return { { SPELL_JUDGEMENT_OF_LIGHT, SPELL_JUDGEMENT_OF_WISDOM, SPELL_JUDGEMENT_OF_JUSTICE } };
                case MOD_HEALING_LIGHT: return { { SPELL_FLASH_HEAL } };
                case MOD_DAWNWARD: return { { SPELL_HOLY_SHOCK } };
                case MOD_PARAGONS_AEGIS: return { { SPELL_SHIELD_OF_RIGHTEOUSNESS } };
                default: return {};
            }
        if (rune == RUNE_STORMREAVER)
            switch (modifier)
            {
                case MOD_STORMHUNTER: return { { SPELL_HUNTERS_MARK } };
                case MOD_FROZEN_FRONT: return { { SPELL_FROST_SHOCK } };
                case MOD_STORMHAMMER: return { { SPELL_SHIELD_SLAM } };
                case MOD_FLAME_CONDUCTOR: return { { SPELL_FLAME_SHOCK } };
                case MOD_OVERCHARGED_CORE: case MOD_RIDE_THE_LIGHTNING: case MOD_VIOLENT_GROUNDING:
                    return { { SPELL_LIGHTNING_SHIELD } };
                case MOD_STATIC_STRIKE: return { { SPELL_LIGHTNING_BOLT } };
                case MOD_WARSTORM: return { { SPELL_THUNDER_CLAP } };
                default: return {};
            }
        if (rune == RUNE_URSOCS_CHAMPION)
            switch (modifier)
            {
                case MOD_UNBROKEN_CHAMPION: case MOD_BEARS_MOMENTUM: return { { SPELL_MAUL } };
                case MOD_STONEGUARD: return { { SPELL_EARTH_SHIELD } };
                case MOD_THUNDERBEAR: return { { SPELL_THUNDER_CLAP } };
                case MOD_LONG_GUARD: return { { SPELL_SWIPE_BEAR } };
                case MOD_URSOCS_CHALLENGE: return { { SPELL_MANGLE_BEAR } };
                case MOD_RENDING_MAUL: return { { SPELL_RAKE } };
                case MOD_EARTHSHOCK_ROAR: return { { TankRunes::SPELL_EARTH_SHOCK } };
                case MOD_LIFEBLOOM_HIDE: return { { SPELL_LIFEBLOOM } };
                case MOD_SHIELDING_ROOTS: return { { SPELL_REJUVENATION } };
                default: return {};
            }
        if (rune == RUNE_WHITEFANG)
            switch (modifier)
            {
                case MOD_STORMFANG: return { { SPELL_LIGHTNING_SHIELD } };
                case MOD_FLAMEFANG: return { { SPELL_FLAME_SHOCK } };
                case MOD_FROZEN_WOUND: return { { SPELL_RAKE } };
                case MOD_FANG_COLD_BLOOD: return { { SPELL_RAKE }, { SPELL_FEROCIOUS_BITE } };
                case MOD_PREDATORS_CHILL: case MOD_CLAWING_COLD: return { { SPELL_FROSTBOLT } };
                case MOD_MARKED_PREY: return { { SPELL_HUNTERS_MARK } };
                case MOD_MOONFROST_FANG: return { { SPELL_MOONFIRE } };
                case MOD_LINGERING_FROST: return { { SPELL_SHRED } };
                case MOD_MENDING_FANG: return { { SPELL_REJUVENATION } };
                default: return {};
            }
        if (rune == RUNE_EMBERS_WRATH)
            switch (modifier)
            {
                case MOD_FLAME_SHOCK_CLAWS: case MOD_FIRESTORM_POUNCE: return { { SPELL_FLAME_SHOCK } };
                case MOD_FROZEN_EMBERS: return { { SPELL_ICY_TOUCH } };
                case MOD_EMBER_RUSH: return { { SPELL_SHRED } };
                case MOD_SMOLDERING_BITE: return { { SPELL_FEROCIOUS_BITE } };
                case MOD_PHOENIX_CLAWS: return { { SPELL_PYROBLAST } };
                case MOD_WILDFIRE_SWIPE: return { { SPELL_SWIPE_CAT } };
                case MOD_HEALING_FLAME: return { { SPELL_REJUVENATION } };
                case MOD_BURNING_RAKE: return { { SPELL_RAKE } };
                default: return {};
            }
        if (rune == RUNE_DUELIST)
            switch (modifier)
            {
                case MOD_FLAME_PISTOL: case MOD_LOADED_POWDER: return { { SPELL_FIRE_BLAST } };
                case MOD_STORM_PISTOL: return { { SPELL_LIGHTNING_SHIELD } };
                case MOD_BETWEEN_THE_EYES: return { { SPELL_EVISCERATE } };
                case MOD_MARKED_SHOT: return { { SPELL_HUNTERS_MARK } };
                case MOD_FROZEN_POWDER: return { { SPELL_ICY_TOUCH } };
                case MOD_HEALING_POWDER: return { { SPELL_RENEW } };
                case MOD_ARCANE_POWDER: return { { SPELL_ARCANE_SHOT } };
                default: return {};
            }
        if (rune == RUNE_SPLINTERED_CROWN)
            switch (modifier)
            {
                case MOD_TWIN_FANGS: case MOD_COLD_SNAP: case MOD_GLACIAL_WOUND: return { { SPELL_ICE_LANCE } };
                case MOD_FROZEN_VOLLEY: case MOD_BRITTLE_IMPACT: case MOD_PERMAFROST:
                    return { { SPELL_CONE_OF_COLD } };
                case MOD_HAILSTORM_CROWN: case MOD_WHITEOUT: return { { SPELL_BLIZZARD } };
                case MOD_GATHERING_BLIZZARD: return { { SPELL_BLIZZARD }, { SPELL_ICE_LANCE } };
                case MOD_FROSTFIRE_FRACTURE: return { { SPELL_FIRE_BLAST } };
                case MOD_RUNIC_RIME: return { { SPELL_ICY_TOUCH } };
                default: return {};
            }
        if (rune == RUNE_SPECTRAL_BLADE)
            switch (modifier)
            {
                case MOD_FROST_PHANTOM: return { { SPELL_ICY_TOUCH } };
                case MOD_FLAME_PHANTOM: return { { SPELL_FLAME_SHOCK } };
                case MOD_MARKED_EXECUTION: return { { SPELL_HUNTERS_MARK } };
                case MOD_CORRUPTED_BLADE: return { { SPELL_CORRUPTION } };
                case MOD_RESTORING_PHANTOM: return { { SPELL_RENEW } };
                case MOD_BLEEDING_FOLLOW: return { { SPELL_REND }, { SPELL_SINISTER_STRIKE } };
                case MOD_MARKED_FOR_MURDER: return { { SPELL_BACKSTAB } };
                default: return {};
            }
        if (rune == RUNE_ELUNES_GUARDIAN)
            switch (modifier)
            {
                case MOD_SENTINEL: return { { SPELL_HUNTERS_MARK } };
                case MOD_FROSTFEATHER: case MOD_WINTERS_BLESSING: return { { SPELL_FROSTBOLT } };
                case MOD_LUNAR_GUIDANCE: return { { SPELL_MOONFIRE } };
                case MOD_COMMAND_THE_HUNT: return { { SPELL_HUNTERS_MARK }, { SPELL_ARCANE_SHOT, SPELL_AIMED_SHOT } };
                case MOD_SUN_AND_MOON: case MOD_TWILIGHT_HUNT: return { { SPELL_WRATH } };
                case MOD_HUNTERS_PURSUIT:
                    return { { SPELL_HUNTERS_MARK }, { SPELL_ARCANE_SHOT, SPELL_AIMED_SHOT, SPELL_STEADY_SHOT } };
                default: return {};
            }
        if (rune == RUNE_HOLLOW_CONGREGATION)
            switch (modifier)
            {
                case MOD_PLAGUE_SERMON: return { { SPELL_CORRUPTION } };
                case MOD_MOONLIT_CONGREGATION: return { { SPELL_MOONFIRE } };
                case MOD_DISSONANCE: return { { SPELL_SHADOW_WORD_PAIN } };
                case MOD_DEVOURING_COMMAND: return { { SPELL_SHADOW_BOLT } };
                case MOD_FROSTBOUND_SERMON: return { { SPELL_FROSTBOLT } };
                case MOD_FEEDING_THE_VOID: return { { SPELL_DRAIN_SOUL } };
                case MOD_RENEWED_WITNESS: return { { SPELL_RENEW } };
                default: return {};
            }
        if (rune == RUNE_GUIDING_LIGHT)
            switch (modifier)
            {
                case MOD_EARTHEN_STAR: return { { SPELL_EARTH_SHIELD } };
                case MOD_MOONLIT_STAR: return { { SPELL_REJUVENATION } };
                case MOD_CHAIN_GUIDANCE: return { { SPELL_CHAIN_HEAL } };
                case MOD_HOLY_PASSAGE: return { { SPELL_HOLY_SHOCK } };
                case MOD_TIDAL_FAREWELL: return { { SPELL_RIPTIDE } };
                case MOD_RENEWED_FAREWELL: return { { SPELL_RENEW } };
                default: break;
            }
        if (rune == RUNE_ASTRAL_CONVERGENCE)
            switch (modifier)
            {
                case MOD_FLAME_ECLIPSE: return { { SPELL_FLAME_SHOCK } };
                case MOD_FROZEN_ECLIPSE: return { { SPELL_FROSTBOLT } };
                case MOD_TIDAL_FLARE: return { { SPELL_LIGHTNING_BOLT } };
                case MOD_TWIN_MOONS: return { { SPELL_MOONFIRE } };
                case MOD_INSECT_RENEWAL: return { { SPELL_INSECT_SWARM } };
                case MOD_ASTRAL_WARD: return { { SPELL_BARKSKIN } };
                default: return {};
            }
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

    // A phoenix of Phoenix Wake, diving from above the Hero onto the Ember's host.
    struct Flight
    {
        ObjectGuid Bird, Host;
        uint32 Release = 0;                 // the release it belongs to (a flock's birds share one)
        float Share = 1.0f;                 // of the flight's hits (Ashen Flock: a third each)
        float Pyro = 0.0f;                  // the Pyroblast damage the Ember held
        float Cross = 0.0f;                 // a crossed or visited enemy's hit, before Share
        float HalfWidth = 0.0f, Speed = 0.0f;
        std::set<ObjectGuid> Hit;           // each enemy once
        std::size_t Crossed = 0;
        std::vector<ObjectGuid> Visits;     // Wild Flight: the enemies still to visit
        bool Struck = false;                // it has dived onto its host and struck it
        bool Erupted = false, Blazed = false;   // its Flame Shock eruption, its Blazing Path: spent
        uint32 SpiralUntil = 0;             // Death Spiral: circling its host until (game ms)
        uint32 Born = 0, Repath = 0;
        Position Last, LastTrail;
    };

    struct State
    {
        std::array<std::array<Choice, RUNE_SLOTS>, MAX_SPECS> Choices;     // [spec][rune slot]
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
        float ParagonOutput = 0.0f;                 // the aura's last tick (before its target's mitigation)
        uint8 FreeTicks = 0;                        // Sanctuary March: ticks without upkeep
        bool ChargeTick = false;                    // Consecrated Charge: the next tick 15% more
        ObjectGuid LightAlly;                       // Healing Light's last ally
        uint32 ParagonJudgmentAt = 0, ParagonShockAt = 0, DawnwardAt = 0, RighteousAt = 0, ChargeUntil = 0;
        uint32 ConsecratedChargeAt = 0, MarchAt = 0, LightFlashAt = 0;     // game ms
        uint32 LastShelter = 0;
        ObjectGuid Sheltered;                                       // the one ally holding its shield
        std::vector<std::pair<Position, uint32>> Trail;             // footsteps: where, when
        std::unordered_map<ObjectGuid, uint8> Weighed;              // enemy -> damaging ticks in a row

        // The Stormbound Conduit.
        uint32 StormTimer = 0;                      // ms since Storm Within's last pulse
        uint32 ChainArcAt = 0, FaradayAt = 0, ShieldArcAt = 0, EarthArcAt = 0, StormLashAt = 0, StarboundAt = 0;
        uint32 DistantAt = 0, ShockingAt = 0, StormsEyeUntil = 0;     // game ms
        std::set<ObjectGuid> ConductiveFresh;       // Persistent Current: applications whose first arc is unspent

        // Echoing Thunder: the field laid (where, until), its first echo, the echo and options' times (game ms).
        Position FieldAt;
        uint32 FieldMap = 0, FieldUntil = 0, EchoAt = 0, RevengeUntil = 0, ThunderBurstAt = 0, StormRageAt = 0;
        uint32 TidalShieldAt = 0;
        int32 EarthShieldHeal = 0;                  // the last of their Earth Shield's heals on them
        bool FieldFirst = false;


        // The Wandering Mist.
        int32 MistTickAmount = 0;                   // Soothing Mist's last tick (Rolling Mist)
        ObjectGuid LesserTarget;                    // the lesser Enveloping Mist's (Comforting Veil)
        // The Wellspring: Riptide's charge (until), Renewing Spring's, Guided Current's (until); game ms.
        uint32 WellRiptideUntil = 0, GuidedUntil = 0, FlowingAt = 0, RiptideSpringAt = 0;
        bool WellRenewCharge = false;
        ObjectGuid MistLast, MistWatched;           // the last channel's target; Holy Shock's or Chain Heal's
        uint32 MistGoneAt = 0, MistWatchedAt = 0;   // game ms
        uint32 RenewingMistAt = 0, TidalMistAt = 0, HolyThreadAt = 0, ChainOfJadeAt = 0, RollingAt = 0;
        uint32 JadeShelterAt = 0, EarthenCocoonAt = 0;

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
        uint8 WakeRips = 0;                 // rips the wake has left (2 with Overpowering Echo)
        uint32 WakeRippedAt = 0;            // the last rip: one ability hitting several enemies rips once
        ObjectGuid WakePrimary;
        Position WakeOffset;                        // Relentless: from its target, as cut

        // The Dawnbound Crusader.
        // The Hungering Rift: its host and times (game ms), where it opened (Open Wound), its lashes.
        struct RiftState
        {
            ObjectGuid Host, Target;                // Target: At Your Back's last
            Position At, HostAt;                    // where it opened; the host at the last lash (Haunting Rift)
            uint32 Map = 0, Until = 0, Timer = 0, Lashes = 0, HauntAt = 0, BackAt = 0;
            uint32 InRow = 0;                       // Open Wound's lashes in a row
            bool Pending = false, Extended = false;
        } Rift;
        // Howling North: the blade readied, its charges, the field (game ms).
        uint8 NorthBlades = 0;                      // blades Howling Blast readied
        bool NorthIcy = false, NorthField = false, NorthHorn = false;
        uint32 NorthwindAt = 0, BladeAt = 0, NorthMarks = 0, ColdRecoveryAt = 0, MendingUntil = 0;
        ObjectGuid FollowTarget;
        uint32 FollowUntil = 0, FollowStrikeUntil = 0;     // Frost Strike's Frost; its 10%/20% more (Endurance, Flow)
        float FollowStrikeMore = 1.0f;
        Position NorthFieldAt;
        uint32 NorthFieldMap = 0, NorthFieldUntil = 0, NorthFieldTimer = 0;
        // Trueshot: its pending Aimed Shot, the charges (game ms).
        ObjectGuid ExposedTarget;
        uint32 TrueshotPendingUntil = 0, MultiChargeUntil = 0, ArcaneChargeUntil = 0, FollowUpUntil = 0;
        uint32 RenewedAimAt = 0;
        // Scarlet Benediction: each ally's Bloom (its openings, and when a new one may be planted; game ms).
        struct Bloom
        {
            uint8 Opens = 0;
            uint32 RearmAt = 0;
        };
        std::map<ObjectGuid, Bloom> Blooms;
        // The Evergrove: Tranquil Grove's last growth on each ally (game ms).
        std::map<ObjectGuid, uint32> TranquilAt;
        // Iron Reckoning: the bulwark's window and what it holds (game ms).
        uint32 IronUntil = 0, IronMendAt = 0;
        float IronAbsorbed = 0.0f, IronSlamHit = 0.0f;
        ObjectGuid IronSlamTarget;
        bool IronReleased = false, IronShockwave = false, IronRevenge = false, IronCircle = false;
        bool IronDevastate = false, IronConcussive = false, IronVengeful = false;

        // The Dawnbound Crusader: the vow, the mark, the echo's time and charges (game ms), Communion's turn,
        // Shared Pilgrimage's places.
        ObjectGuid Vow, DawnMark;
        uint32 DawnEchoAt = 0, FrostDawnUntil = 0, GraceUntil = 0, OathAt = 0;
        bool CommunionStrike = false, PilgrimCharge = false;
        Position PilgrimHero, PilgrimVow;
        Position SacredAt;                          // Sacred Ground's patch (SacredUntil 0: none)
        uint32 SacredMap = 0, SacredUntil = 0, SacredTimer = 0;
        float SacredTick = 0.0f;
        bool SacredHeal = false;

        // The Hollow Congregation.
        std::vector<ObjectGuid> HollowEchoes;       // oldest first

        std::unordered_map<uint32, int32> Reported;  // the rune auras' amounts last told to the addon (Report)
        uint32 WatchTimer = 0;                      // the bound rune's aura checked (OnPlayerUpdate)

        // The Shattered Sun: the enemies holding shards.
        struct Sunshard
        {
            ObjectGuid Host;
            Position At;                            // where its host stood (Solar Anchor)
            uint32 Map = 0, Until = 0;              // game ms
            bool Lunar = false, Storm = false;
        };
        std::vector<Sunshard> Shards;               // oldest first
        std::set<ObjectGuid> CoreLanced;            // Broken Cores Ice Lance has struck
        uint32 StormShardAt = 0, FrozenFlashAt = 0, FlameBurstAt = 0, IcyShelterAt = 0;
        Position ScaldAt;                           // Scalding Ground (ScaldUntil 0: none)
        uint32 ScaldMap = 0, ScaldUntil = 0, ScaldTimer = 0;
        float ScaldTick = 0.0f;

        // The Wild Hunt: its Spectral Beasts (oldest first), and Bestial Wrath's calling.
        std::vector<ObjectGuid> SpectralBeasts;
        uint32 WrathTimer = 0;
        ObjectGuid HuntChargePrey;                  // Arcane Shot's charge: its target, until (game ms)
        uint32 HuntChargeUntil = 0, StormHuntAt = 0, StormChargeUntil = 0;

        bool Debug = false;                         // .runedebug: the newest runes' steps told in chat

        // The Spectral Blade, the Hollow Congregation's Extended Options.
        uint32 RestoringAt = 0, DevouringAt = 0, WitnessAt = 0;     // game ms
        std::set<ObjectGuid> Frostbound;            // Frostbound Sermon: echoes whose next barrage splits
        std::map<ObjectGuid, uint32> RimeOn, BlizzardHit;   // Runic Rime's Icy Touch; Blizzard's victims (game ms)
        bool SabreSteel = false;                    // the Sabre Slash landing spent Loaded Steel
        uint32 FrozenPowderAt = 0, EfficientAt = 0, HealingPowderAt = 0;    // game ms
        std::map<ObjectGuid, uint32> ShredEmberAt;  // Ember's Wrath (game ms)
        uint32 FrozenEmbersAt = 0, FlameSpreadAt = 0, PhoenixClawsAt = 0, WildfireSwipeAt = 0, AshenRenewalAt = 0;
        uint32 HealingFlameAt = 0;
        uint32 FractureAt = 0, HailTimer = 0;
        Position PatchAt;                           // the Shadow Word: Pain patch (PatchUntil 0: none)
        uint32 PatchMap = 0, PatchUntil = 0, PatchTimer = 0;
        float PatchTick = 0.0f;

        // Arcane Mind.
        uint8 BlastCount = 0;                       // Accelerated Thought: Arcane Blasts since the last extra
        bool MindChannel = false;                   // an empowered Arcane Missiles channelling (Mental Reserves)
        ObjectGuid MindTarget;                      // the empowered channel's target
        std::map<ObjectGuid, uint32> Blasted;       // Storm Conduit: enemies the Hero's Arcane Blast hit (game ms)
        uint32 BoltInsightAt = 0, StarfallAt = 0, ShardsAt = 0, EmpoweredAt = 0, EvocativeAt = 0;   // game ms

        // The Splintered Crown.
        uint8 Icicles = 0, IcicleAttacks = 0;

        // Phoenix Wake.
        ObjectGuid EmberHost;
        float EmberPyro = 0.0f;
        uint32 Releases = 0, ArrivedRelease = 0;
        std::vector<Flight> Flights;
        std::vector<std::pair<Position, uint32>> Scorch;    // Scorched Earth's patches: where, when
        uint32 ScorchMap = 0, ScorchTimer = 0;
        float ScorchTick = 0.0f;
        ObjectGuid LavaFlockOn;                     // Lava Flock: the Ember host a Lava Burst hit (one an Ember)
        uint32 BurnRelease = 0, EmberguardAt = 0;   // the release whose host burns; game ms

        // Astral Horizon.
        ObjectGuid Orb, OrbVictim;
        uint32 OrbReadyAt = 0, OrbUntil = 0, OrbTimer = 0;
        float OrbBolt = 0.0f;
        bool OrbSatellite = false;                  // Satellite: the Hero wears it (SPELL_ORB_SATELLITE)
        bool OrbSlowed = false;                     // it has reached its first enemy: a drift from then on
        float OrbHeading = 0.0f;
        Position OrbLast;
        std::set<ObjectGuid> OrbCrossed;            // Wandering Star: each enemy once
        Position RemnantAt;                         // Astral Remnant (RemnantUntil 0: none)
        uint32 RemnantMap = 0, RemnantUntil = 0, RemnantTimer = 0;
        float RemnantTick = 0.0f;
        float OrbEmpower = 1.0f;                    // Arcane Blast on its victim: the next bolt 35% more
        bool OrbSplit = false, OrbEvoked = false;   // Starfire's split bolt waiting; Evoked Remnant spent
        uint32 SparkUntil = 0, OrbPulseAt = 0, OrbLightningAt = 0, StarwardAt = 0, ShelterAt = 0;  // game ms

        // Guiding Light.
        ObjectGuid StarBearer;
        uint32 StarUntil = 0, StarPulseTimer = 0, StarMovedAt = 0, StarArriveAt = 0, StarPulses = 0;
        uint8 StarFocus = 0;                        // Guardian Star: pulses in a row on its bearer
        bool StarTravelling = false;

        // Elune's Guardian.
        uint8 Owls = 0, OwlCount = 0;               // owls waiting to dive; owls since Guiding Wings' last
        uint32 SentinelGen = 0, SentinelUntil = 0;  // the Sentinel out (a new one's number ends the last); game ms
        std::map<ObjectGuid, std::pair<uint8, uint8>> Feathers;    // Frostfeather: an enemy's owls to dive, bursts
        uint32 CommandAt = 0, ExposedAt = 0;        // game ms

        // Astral Convergence.
        std::vector<ObjectGuid> Marked;             // enemies in the Hero's Sunlight or Moonlight, oldest first
        Position EventideAt;                        // Eventide's field (EventideUntil 0: none)
        uint32 EventideMap = 0, EventideUntil = 0, EventideTimer = 0;
        float EventideTick = 0.0f;
        std::map<ObjectGuid, uint32> BoltFlares;    // a Lightning Bolt's last flare on each enemy (game ms)
        std::set<ObjectGuid> FrozenSunlight;        // Frozen Eclipse: Sunlight a Frostbolt has empowered from
        uint32 TwinMoonsAt = 0, AstralWardAt = 0;   // game ms

        // Ursoc's Champion.
        int32 GuardShield = 0;                      // the guard's absorb as raised (0: none up)
        uint8 GuardBlows = 0;                       // Unbroken's blows from this guard
        bool GuardSwiped = false, GuardStone = false, GuardThunder = false, GuardShocked = false;

        // The Duelist: direct Sinister Strike hits toward Opening Shot.
        uint8 SabreHits = 0;

        // The Stormreaver: the Stormfront's pulse timer; the last discharge.
        uint32 StormfrontTimer = 0, LastDischarge = 0;
        uint32 FlameConductorAt = 0, FrozenFrontAt = 0, StormhammerAt = 0, WarstormAt = 0;     // game ms

        // Ember's Wrath: Blazing Rake's next extra hit; Ashen Pursuit's next bonus, its last victim and every one.
        uint32 BlazingRakeAt = 0, AshenReadyAt = 0;
        ObjectGuid AshenLast;
        std::vector<ObjectGuid> AshenSeen;

        // Whitefang: the primary mark (on whom, since when); Pursuing Chill's discount waiting; Cold Blood's victim.
        ObjectGuid PreyOn, ColdBloodOn;
        uint32 PreyAt = 0, RecoveryAt = 0, MendingAt = 0;
        bool PursuitDiscount = false;
        bool PreyChilled = false, PreyChillUsed = false;    // a Frostbolt's empowering of the mark (once a mark)

    };

    std::unordered_map<ObjectGuid::LowType, State> states;

    State& StateOf(Player* player)
    {
        return states[player->GetGUID().GetCounter()];
    }

    // The active specialization's rune in a slot (0 the first, 1 the second).
    Choice& ActiveChoice(Player* player, uint8 slot = 0)
    {
        return StateOf(player).Choices[Specs::ActiveSpec(player)][slot];
    }

    // The spell or any of its ranks.
    bool Knows(Player* player, uint32 firstRank)
    {
        for (uint32 id = firstRank; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (player->HasSpell(id))
                return true;
        return false;
    }

    // Every rank of the abilities the active specialization's slotted Skill Cards will bring.
    std::set<uint32> SlottedSpells(Player* player)
    {
        std::set<uint32> spells;
        for (auto const& [kind, id] : Specs::SlottedCards(player))
            if (Wildcard::Ability const* ability = kind == 0 ? Wildcard::FindAbility(id) : nullptr)
                for (Wildcard::Rank const& rank : ability->Ranks)
                    spells.insert(rank.SpellId);
        return spells;
    }

    bool InChain(std::set<uint32> const& spells, uint32 firstRank)
    {
        for (uint32 id = firstRank; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (spells.contains(id))
                return true;
        return false;
    }

    // The groups the player meets none of, as the protocol writes them ('-' when none); with slotted, the abilities
    // the slotted cards will bring count as met.
    std::string Missing(Player* player, std::vector<std::vector<uint32>> const& groups,
        std::set<uint32> const* slotted = nullptr)
    {
        std::string out;
        for (std::vector<uint32> const& group : groups)
        {
            if (std::any_of(group.begin(), group.end(), [player, slotted](uint32 id)
                { return Knows(player, id) || (slotted && InChain(*slotted, id)); }))
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

    // A rune aura's amount, told to the Hero's panel addon (V,<spell>,<kind>,<amount>): the 3.3.5 client never learns
    // an aura's amount from the server and would show its Spell.dbc base (1) in the tooltip. Kinds: A absorb, H
    // healing a second, D damage a second, S slow (percent).
    void Report(Player* player, uint32 spellId, char kind, int32 amount)
    {
        if (!player || !amount)
            return;
        int32& last = StateOf(player).Reported[spellId];
        if (last == std::abs(amount))
            return;                                 // told once a change: Graves tick every second
        last = std::abs(amount);
        Send(player, Acore::StringFormat("V,{},{},{}", spellId, kind, last));
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

    // One of Imp Legion's imps by the target, for LEGION_DURATION_MS.
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

    void ClearLegends(Player* player);
    void DismissBeast(Player* player);
    void ClearNewest(Player* player);

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
        player->CastCustomSpell(target, SPELL_VOLLEY_BOLT, &damage, nullptr, nullptr, true);
    }

    // Chaos Bolt Volley: at 5 Fel Command, Throw Glaive also fires 3 bolts (Barrage and Pact of Ruin 2 more each),
    // 0.15 sec apart: the first at the glaive's target, each next at another enemy within 10 yards of it, nearest
    // first; extra bolts go to the target. Felguard's Call: a Felguard; Imp Legion: an imp for each bolt.
    void FireVolley(Player* player, Unit* primary)
    {
        player->RemoveAurasDueToSpell(SPELL_FEL_COMMAND);
        uint32 bolts = BOLTS + (HasMarker(player, MOD_BARRAGE) ? BARRAGE_BOLTS_MORE : 0)
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

        if (HasMarker(player, MOD_FELGUARDS_CALL))
            SummonFelguard(player, primary);
        if (HasMarker(player, MOD_IMP_LEGION))
            for (uint32 i = 0; i < bolts; ++i)
                SummonDemon(player, primary, false);
    }

    // Defined further on (the runes' shared helpers).
    uint32 Now();
    float RunePower(Player* player, SpellSchoolMask school, uint32 floorSpell, bool withBonuses = true);
    bool Ready(uint32 at, uint32 ms);
    void Renew(Player* player, uint32 spellId);
    void Strike(Player* player, Unit* target, uint32 spellId, float amount);
    void AddDot(Player* player, Unit* target, uint32 spellId, float fresh, int32 ms);
    void Mend(Player* player, Unit* ally, uint32 spellId, float amount);
    void Ward(Player* player, Unit* unit, uint32 spellId, float amount);
    void ExtendAura(Aura* aura, int32 ms, int32 most);
    float SchoolBonus(Player* player, SpellSchoolMask school);

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

    // The aura's reach: 8 yards (Boundless Sanctity 12, Hallowed Footsteps 4); Consecrated Charge 4 more a while.
    float ParagonRadius(Player* player)
    {
        float radius = HasParagonMarker(player, MOD_BOUNDLESS_SANCTITY) ? BOUNDLESS_RADIUS
            : HasParagonMarker(player, MOD_HALLOWED_FOOTSTEPS) ? FOOTSTEPS_RADIUS : PARAGON_RADIUS;
        return radius + (Now() < StateOf(player).ChargeUntil ? CHARGE_RADIUS : 0.0f);
    }

    // The aura on, and the unit within it.
    bool InParagon(Player* player, Unit* unit)
    {
        return player->HasAura(SPELL_SANCTIFIED_PARAGON) && unit
            && player->IsWithinDistInMap(unit, ParagonRadius(player));
    }

    // Judgment on an enemy in the aura: a Holy hit of half a tick (Radiant Judgment a whole one; Bastion of Faith 50%
    // more threat), 6 sec apart.
    void ParagonJudgment(Player* player, Unit* enemy)
    {
        State& state = StateOf(player);
        if (!InParagon(player, enemy) || !Ready(state.ParagonJudgmentAt, PARAGON_JUDGMENT_EVERY_MS))
            return;
        state.ParagonJudgmentAt = Now();
        float hit = state.ParagonOutput
            * (HasParagonMarker(player, MOD_RADIANT_JUDGMENT) ? RADIANT_JUDGMENT : PARAGON_JUDGMENT);
        Strike(player, enemy, SPELL_PARAGON_JUDGMENT, hit);
        if (HasParagonMarker(player, MOD_BASTION_OF_FAITH) && enemy->IsAlive() && enemy->CanHaveThreatList())
            enemy->AddThreat(player, hit * BASTION_THREAT, SPELL_SCHOOL_MASK_HOLY);
    }

    // A Holy Shock heal on an injured ally in the aura: 15% of it to another injured ally within 8 yards (8 sec
    // apart); Dawnward, that one also shielded for 20% of it (4 sec, 8 sec apart).
    void ParagonHolyShock(Player* player, Unit* ally, float heal)
    {
        State& state = StateOf(player);
        if (!InParagon(player, ally) || heal <= 0.0f || !Ready(state.ParagonShockAt, PARAGON_SHOCK_EVERY_MS))
            return;
        for (Unit* other : MostInjured(player, STAR_RANGE))
            if (other != ally && !other->IsFullHealth() && other->IsWithinDistInMap(ally, PARAGON_SHOCK_RANGE))
            {
                state.ParagonShockAt = Now();
                float second = heal * PARAGON_SHOCK_SHARE;
                Mend(player, other, SPELL_PARAGON_MEND, second);
                if (HasParagonMarker(player, MOD_DAWNWARD) && Ready(state.DawnwardAt, DAWNWARD_EVERY_MS))
                {
                    state.DawnwardAt = Now();
                    Ward(player, other, SPELL_DAWNWARD, second * DAWNWARD_SHARE);
                }
                return;
            }
    }

    // Shield of Righteousness on an enemy in the aura: an absorb of 3% of maximum health (Paragon's Aegis 4%), 4 sec,
    // 8 sec apart.
    void ParagonRighteous(Player* player, Unit* enemy)
    {
        State& state = StateOf(player);
        if (!InParagon(player, enemy) || !Ready(state.RighteousAt, RIGHTEOUS_EVERY_MS))
            return;
        state.RighteousAt = Now();
        Ward(player, player, SPELL_RIGHTEOUS_WARD, float(player->CountPctFromMaxHealth(
            int32(HasParagonMarker(player, MOD_PARAGONS_AEGIS) ? AEGIS_PCT : RIGHTEOUS_PCT))));
    }

    // Consecrated Charge (Charge), Sanctuary March (Divine Shield), Healing Light (Flash Heal on the ally it heals).
    void ParagonAbility(Player* player, Unit* target, uint32 firstRank, float heal)
    {
        State& state = StateOf(player);
        if (firstRank == SPELL_CHARGE && HasParagonMarker(player, MOD_CONSECRATED_CHARGE)
            && Ready(state.ConsecratedChargeAt, CHARGE_EVERY_MS))
        {
            state.ConsecratedChargeAt = Now();
            state.ChargeUntil = Now() + PARAGON_CHARGE_MS;
            state.ChargeTick = true;
        }
        if (firstRank == SPELL_DIVINE_SHIELD && HasParagonMarker(player, MOD_SANCTUARY_MARCH)
            && player->HasAura(SPELL_SANCTIFIED_PARAGON) && Ready(state.MarchAt, MARCH_EVERY_MS))
        {
            state.MarchAt = Now();
            state.FreeTicks = MARCH_TICKS;
        }
        if (firstRank == SPELL_FLASH_HEAL && HasParagonMarker(player, MOD_HEALING_LIGHT) && target && heal > 0.0f
            && target->GetGUID() == state.LightAlly && Ready(state.LightFlashAt, LIGHT_EVERY_MS))
        {
            state.LightFlashAt = Now();
            Mend(player, target, SPELL_PARAGON_MEND, heal * LIGHT_FLASH);
        }
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
        if (state.FreeTicks)
            --state.FreeTicks;                      // Sanctuary March
        else
        {
            if (player->GetPower(POWER_MANA) < upkeep)
            {
                ChatHandler(player->GetSession()).SendSysMessage("|cffa335eeSanctified Paragon:|r out of mana.");
                return false;
            }
            player->ModifyPower(POWER_MANA, -upkeep);
        }

        bool boundless = HasParagonMarker(player, MOD_BOUNDLESS_SANCTITY);
        bool footsteps = HasParagonMarker(player, MOD_HALLOWED_FOOTSTEPS);
        float radius = ParagonRadius(player);

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
        state.ParagonOutput = output;
        if (state.ChargeTick)
        {
            state.ChargeTick = false;
            output *= CHARGE_TICK;
        }

        // Merciful Ground (Healing Light: 75% of it): no damage; the single most injured party member within range
        // healed.
        bool light = HasParagonMarker(player, MOD_HEALING_LIGHT);
        if (HasParagonMarker(player, MOD_MERCIFUL_GROUND) || light)
        {
            for (Unit* ally : MostInjured(player, radius))
                if (!ally->IsFullHealth())
                {
                    int32 heal = std::max<int32>(1, int32(ParagonSupport(player, output, false)
                        * (light ? HEALING_LIGHT_SHARE : MERCIFUL_SHARE)));
                    player->CastCustomSpell(ally, SPELL_MERCIFUL_GROUND, &heal, nullptr, nullptr, true);
                    state.LightAlly = ally->GetGUID();
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
                Report(player, SPELL_SHELTER_OF_THE_PARAGON, 'A', shield);
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
        bool weight = HasParagonMarker(player, MOD_WEIGHT_OF_THE_UNWORTHY);
        bool bastion = HasParagonMarker(player, MOD_BASTION_OF_FAITH);
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

    void Lasting(Player* player, Unit* unit, uint32 spellId, int32 ms, int32 const* amount);

    // The Hero's Conductive on the enemy: the hit it remembers (0 none).
    float ConductiveOn(Player* player, Unit* enemy)
    {
        AuraEffect const* effect = enemy
            ? enemy->GetAuraEffect(SPELL_CONDUCTIVE, EFFECT_0, player->GetGUID()) : nullptr;
        return effect ? float(effect->GetAmount()) : 0.0f;
    }

    // Conductive on the enemy for 6 sec (12 with Persistent Current), remembering hit.
    void Conduct(Player* player, Unit* enemy, float hit)
    {
        if (!enemy || !enemy->IsAlive() || hit < 1.0f)
            return;
        int32 amount = int32(hit);
        Lasting(player, enemy, SPELL_CONDUCTIVE,
            HasStormMarker(player, MOD_PERSISTENT_CURRENT) ? PERSISTENT_MS : CONDUCTIVE_MS, &amount);
        StateOf(player).ConductiveFresh.insert(enemy->GetGUID());
    }

    // Another enemy within arc range of the host (10 yards; Faraday Field 15), but except, else nullptr.
    Unit* ArcTarget(Player* player, Unit* host, Unit* except = nullptr, bool conductive = false)
    {
        float range = HasStormMarker(player, MOD_FARADAY_FIELD) ? FARADAY_RANGE : ARC_RANGE;
        for (Unit* enemy : EnemiesAround(player, host, range))
            if (enemy != host && enemy != except && (!conductive || ConductiveOn(player, enemy) > 0.0f))
                return enemy;
        return nullptr;
    }

    // One arc from the host to the enemy (back into the host: no bolt).
    void ArcTo(Player* player, Unit* host, Unit* enemy, uint32 spellId, float damage)
    {
        if (damage < 1.0f || !enemy->IsAlive())
            return;
        if (enemy != host)
            host->CastSpell(enemy, SPELL_STORMBOUND_ARC_VISUAL, true);
        Strike(player, enemy, spellId, damage);
    }

    // An arc from the host to another enemy, else back into it for half.
    void ArcOrBack(Player* player, Unit* host, uint32 spellId, float damage)
    {
        if (Unit* other = ArcTarget(player, host))
            ArcTo(player, host, other, spellId, damage);
        else
            ArcTo(player, host, host, spellId, damage * ALONE_SHARE);
    }

    // Chain Lightning on a Conductive enemy: its arc (once a cast), 30% of the remembered hit; the options.
    void ChainArc(Player* player, Unit* host)
    {
        State& state = StateOf(player);
        float hit = ConductiveOn(player, host);
        if (hit <= 0.0f || !Ready(state.ChainArcAt, CHAIN_ARC_EVERY_MS))
            return;
        state.ChainArcAt = Now();
        float damage = hit * CHAIN_ARC_SHARE;
        if (HasStormMarker(player, MOD_PERSISTENT_CURRENT) && state.ConductiveFresh.erase(host->GetGUID()))
            damage *= PERSISTENT_FIRST;
        if (HasStormMarker(player, MOD_FARADAY_FIELD) && Ready(state.FaradayAt, FARADAY_EVERY_MS))
        {
            state.FaradayAt = Now();
            damage *= FARADAY_MORE;
        }
        if (HasStormMarker(player, MOD_CLOSED_CIRCUIT))
        {
            ArcTo(player, host, host, SPELL_STORMBOUND_ARC, damage);
            return;
        }
        if (HasStormMarker(player, MOD_FORKED_TEMPEST))
        {
            Unit* first = ArcTarget(player, host);
            Unit* second = first ? ArcTarget(player, host, first) : nullptr;
            if (first && second)
            {
                ArcTo(player, host, first, SPELL_STORMBOUND_ARC, damage * FORKED_SHARE);
                ArcTo(player, host, second, SPELL_STORMBOUND_ARC, damage * FORKED_SHARE);
                return;
            }
        }
        ArcOrBack(player, host, SPELL_STORMBOUND_ARC, damage);
    }

    // A Lightning Shield retaliation the rune makes (Distant Conductor, Storm Within): the shield's rank's damage at
    // the target, a charge spent.
    void Retaliate(Player* player, Unit* target, AuraEffect* shield)
    {
        uint32 damageSpell = sSpellMgr->GetSpellWithRank(SPELL_LIGHTNING_SHIELD_DAMAGE,
            shield->GetSpellInfo()->GetRank());
        SpellInfo const* info = sSpellMgr->GetSpellInfo(damageSpell);
        if (!info)
            return;
        int32 damage = std::max<int32>(1, info->Effects[EFFECT_0].CalcValue(player));
        player->CastCustomSpell(target, SPELL_STORMBOUND_RETALIATION, &damage, nullptr, nullptr, true);
        shield->GetBase()->DropCharge();
    }

    // Storm Within: every 3 sec in combat, a retaliation at the nearest Conductive enemy within 8 yards.
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
        for (Unit* enemy : EnemiesAround(player, player, STORM_WITHIN_RANGE))
            if (ConductiveOn(player, enemy) > 0.0f)
            {
                state.StormTimer = 0;
                Retaliate(player, enemy, shield);
                return;
            }
    }

    // A retaliation hitting the attacker (the shield's own, or the rune's): Live Wire (Conductive on it), and its
    // arc to another Conductive enemy (3 sec apart; Violent Reprisal 40% more).
    void ShieldArc(Player* player, Unit* attacker, float damage)
    {
        if (HasStormMarker(player, MOD_LIVE_WIRE))
            Conduct(player, attacker, damage);
        State& state = StateOf(player);
        if (!Ready(state.ShieldArcAt, SHIELD_ARC_EVERY_MS))
            return;
        if (Unit* other = ArcTarget(player, attacker, nullptr, true))
        {
            state.ShieldArcAt = Now();
            ArcTo(player, attacker, other, SPELL_STORMBOUND_ARC, damage * SHIELD_ARC_SHARE
                * (HasStormMarker(player, MOD_VIOLENT_REPRISAL) ? VIOLENT_REPRISAL : 1.0f));
        }
    }

    // The Hero's own hits with the rune: Lightning Bolt (Conductive; Branching Current, Flame Conductor, Distant
    // Conductor), Frostbolt (Icy Conductor), Chain Lightning (its arc), Earth Shock, Lava Lash and Starfire on a
    // Conductive enemy, Thunder Clap (Shocking Surge), Chain Heal (Storm's Eye).
    void ConduitHit(Player* player, Unit* target, uint32 firstRank, float damage)
    {
        State& state = StateOf(player);
        ObjectGuid me = player->GetGUID();
        if (firstRank == SPELL_CHAIN_HEAL)
        {
            if (HasStormMarker(player, MOD_STORMS_EYE))
                state.StormsEyeUntil = Now() + STORMS_EYE_MS;
            return;
        }
        if (damage <= 0.0f || !player->IsValidAttackTarget(target))
            return;
        switch (firstRank)
        {
            case SPELL_LIGHTNING_BOLT:
            {
                Conduct(player, target, damage);
                bool flame = HasStormMarker(player, MOD_STORM_FLAME_CONDUCTOR);
                Aura* flameShock = flame ? target->GetAuraOfRankedSpell(SPELL_FLAME_SHOCK, me) : nullptr;
                if (flameShock)
                    ExtendAura(flameShock, FLAME_CONDUCTOR_MS, FLAME_CONDUCTOR_MOST_MS);
                if (HasStormMarker(player, MOD_BRANCHING_CURRENT) || flameShock)
                    if (Unit* other = ArcTarget(player, target))
                        Conduct(player, other, damage);
                if (HasStormMarker(player, MOD_DISTANT_CONDUCTOR) && target->IsAlive()
                    && Ready(state.DistantAt, DISTANT_EVERY_MS))
                    if (AuraEffect* shield = Shield(player))
                    {
                        state.DistantAt = Now();
                        Retaliate(player, target, shield);
                    }
                break;
            }
            case SPELL_FROSTBOLT:
                if (HasStormMarker(player, MOD_ICY_CONDUCTOR))
                    Conduct(player, target, damage);
                break;
            case SPELL_CHAIN_LIGHTNING:
                ChainArc(player, target);
                break;
            case TankRunes::SPELL_EARTH_SHOCK:
                if (ConductiveOn(player, target) > 0.0f && Ready(state.EarthArcAt, EARTH_ARC_EVERY_MS))
                {
                    state.EarthArcAt = Now();
                    ArcOrBack(player, target, SPELL_STORMBOUND_ARC,
                        RunePower(player, SPELL_SCHOOL_MASK_NATURE, 0) * EARTH_ARC_POWER);
                }
                break;
            case SPELL_LAVA_LASH:
                if (HasStormMarker(player, MOD_STORM_LASH) && ConductiveOn(player, target) > 0.0f
                    && Ready(state.StormLashAt, STORM_LASH_EVERY_MS))
                {
                    state.StormLashAt = Now();
                    ArcOrBack(player, target, SPELL_STORMBOUND_ARC,
                        player->GetTotalAttackPowerValue(BASE_ATTACK) * STORM_LASH_AP
                            * SchoolBonus(player, SPELL_SCHOOL_MASK_NATURE));
                }
                break;
            case SPELL_STARFIRE:
                if (HasStormMarker(player, MOD_STARBOUND_CIRCUIT) && ConductiveOn(player, target) > 0.0f
                    && Ready(state.StarboundAt, STARBOUND_EVERY_MS))
                {
                    state.StarboundAt = Now();
                    ArcOrBack(player, target, SPELL_STARBOUND_CIRCUIT,
                        RunePower(player, SPELL_SCHOOL_MASK_ARCANE, 0) * STARBOUND_POWER);
                }
                break;
            case SPELL_THUNDER_CLAP:
                if (HasStormMarker(player, MOD_SHOCKING_SURGE) && ConductiveOn(player, target) > 0.0f
                    && Ready(state.ShockingAt, SHOCKING_EVERY_MS))
                {
                    state.ShockingAt = Now();
                    float surge = RunePower(player, SPELL_SCHOOL_MASK_NATURE, 0) * SHOCKING_POWER;
                    std::size_t count = 0;
                    for (Unit* enemy : EnemiesAround(player, target, SHOCKING_RADIUS))
                        if (count++ < SHOCKING_TARGETS)
                            Strike(player, enemy, SPELL_SHOCKING_SURGE, surge);
                }
                break;
            default:
                break;
        }
    }

    // Lightning Bolt's or Frostbolt's damage with the rune: Storm's Eye (the next bolt), Icy Conductor.
    float ConduitDamage(Player* player, Unit* target, uint32 firstRank, float damage)
    {
        State& state = StateOf(player);
        if (firstRank == SPELL_LIGHTNING_BOLT && state.StormsEyeUntil)
        {
            if (Now() < state.StormsEyeUntil)
                damage *= STORMS_EYE;
            state.StormsEyeUntil = 0;
        }
        if (firstRank == SPELL_FROSTBOLT && HasStormMarker(player, MOD_ICY_CONDUCTOR)
            && ConductiveOn(player, target) > 0.0f)
            damage *= ICY_CONDUCTOR;
        return damage;
    }

    // ---- Echoing Thunder ---------------------------------------------------------------------------------------

    bool HasThunderMarker(Unit* unit, uint8 modifier)
    {
        return unit && unit->HasAura(SPELL_THUNDER_MODIFIER_FIRST + modifier);
    }

    bool InArc(Position const& center, float facing, Position const& p, float halfArc);
    bool Valid(Player* player, Unit* enemy);
    void PlaceDecals(Player* player, uint32 spellId, std::vector<Position> const& points);

    // The field goes with the Hero (March of the Storm, Stormheart: the small storm about them).
    bool FieldFollows(Player* player)
    {
        return HasThunderMarker(player, MOD_MARCH_OF_THE_STORM) || HasThunderMarker(player, MOD_STORMHEART);
    }

    // The unit stands in the Hero's Thunderfield.
    bool InThunderfield(Player* player, Unit* unit)
    {
        State& state = StateOf(player);
        if (FieldFollows(player))
            return player->HasAura(SPELL_THUNDERFIELD_MARCH) && unit->IsWithinDist2d(player, THUNDERFIELD_RADIUS);
        return state.FieldUntil && Now() < state.FieldUntil && state.FieldMap == player->GetMapId()
            && unit->GetExactDist2d(&state.FieldAt) <= THUNDERFIELD_RADIUS;
    }

    // Thunder Clap: the Thunderfield, where the Hero stands (Rolling Front: under their enemy within 10 yards), or
    // about them.
    void ChargeThunder(Player* player)
    {
        State& state = StateOf(player);
        int32 ms = HasThunderMarker(player, MOD_LINGERING_THUNDER) ? LINGERING_FIELD_MS : THUNDERFIELD_MS;
        uint32 until = Now() + ms;
        state.FieldUntil = until;
        state.FieldFirst = true;
        state.FieldMap = player->GetMapId();
        player->RemoveAurasDueToSpell(SPELL_THUNDERFIELD_MARCH);
        if (FieldFollows(player))
        {
            player->RemoveDynObject(SPELL_THUNDERFIELD);
            if (Aura* aura = player->AddAura(SPELL_THUNDERFIELD_MARCH, player))
            {
                aura->SetMaxDuration(ms);
                aura->SetDuration(ms);
            }
            return;
        }
        state.FieldAt = player->GetPosition();
        if (HasThunderMarker(player, MOD_ROLLING_FRONT))
            if (Unit* enemy = player->GetSelectedUnit(); Valid(player, enemy)
                && player->IsWithinDistInMap(enemy, ROLLING_FRONT_RANGE))
                state.FieldAt = enemy->GetPosition();
        PlaceDecals(player, SPELL_THUNDERFIELD, { state.FieldAt });
        if (DynamicObject* field = player->GetDynObject(SPELL_THUNDERFIELD))
            field->SetDuration(ms);                 // the field's look lasts as long as it does
        player->m_Events.AddEventAtOffset([player, until]()
        {
            if (player->IsInWorld() && StateOf(player).FieldUntil == until)
                player->RemoveDynObject(SPELL_THUNDERFIELD);
        }, Milliseconds(ms));
    }

    // Revenge in the field: the next echo 20% more (6 sec). Earth Shock on an enemy in it: 20% of attack power as
    // Nature to up to 4 within 6 yards of it (8 sec apart). Earth Shield's heal on the Hero: remembered (Tidal
    // Shield).
    void ThunderAbility(Player* player, Unit* target, uint32 firstRank, int32 heal)
    {
        State& state = StateOf(player);
        if (firstRank == SPELL_EARTH_SHIELD_HEAL)
        {
            if (target == player && heal > 0)
                state.EarthShieldHeal = heal;
            return;
        }
        if (firstRank == SPELL_REVENGE && InThunderfield(player, player))
            state.RevengeUntil = Now() + REVENGE_CHARGE_MS;
        if (firstRank == TankRunes::SPELL_EARTH_SHOCK && InThunderfield(player, target)
            && Ready(state.ThunderBurstAt, THUNDER_BURST_EVERY_MS))
        {
            state.ThunderBurstAt = Now();
            float burst = player->GetTotalAttackPowerValue(BASE_ATTACK) * THUNDER_BURST_AP
                * SchoolBonus(player, SPELL_SCHOOL_MASK_NATURE);
            std::size_t count = 0;
            for (Unit* enemy : EnemiesAround(player, target, THUNDER_BURST_RADIUS))
                if (count++ < THUNDER_BURST_TARGETS)
                    Strike(player, enemy, SPELL_THUNDER_BURST, burst);
        }
    }

    // Shield Slam standing in the field: the echo, 42% of the slam's damage before mitigation in a 90 degree cone 10
    // yards toward its target, up to 5 (2 sec apart); the options.
    void ThunderEcho(Player* player, Unit* target, int32 slamDamage)
    {
        State& state = StateOf(player);
        if (slamDamage <= 0 || !InThunderfield(player, player) || !Ready(state.EchoAt, ECHO_EVERY_MS))
            return;
        state.EchoAt = Now();
        ObjectGuid me = player->GetGUID();
        float damage = slamDamage * ECHO_SHARE;
        float threat = 0.0f;
        if (state.FieldFirst && HasThunderMarker(player, MOD_LINGERING_THUNDER))
            damage *= LINGERING_ECHO;
        state.FieldFirst = false;
        if (HasThunderMarker(player, MOD_MARCH_OF_THE_STORM))
            damage *= MARCH_ECHO;
        if (HasThunderMarker(player, MOD_ROLLING_FRONT))
            damage *= ROLLING_FRONT_ECHO;
        if (HasThunderMarker(player, MOD_CONSECRATED_STORM) && ConsecrationRank(player))
            for (uint32 id = SPELL_CONSECRATION; id; id = sSpellMgr->GetNextSpellInChain(id))
                if (DynamicObject* ground = player->GetDynObject(id);
                    ground && player->GetExactDist2d(ground) <= ground->GetRadius())
                {
                    damage *= CONSECRATED_ECHO;
                    threat += CONSECRATED_THREAT;
                    break;
                }
        if (state.RevengeUntil && Now() < state.RevengeUntil)
        {
            bool storm = HasThunderMarker(player, MOD_REVENGE_OF_THE_STORM);
            damage *= storm ? STORM_REVENGE_ECHO : REVENGE_ECHO;
            if (storm && Ready(state.StormRageAt, STORM_REVENGE_EVERY_MS))
            {
                state.StormRageAt = Now();
                player->ModifyPower(POWER_RAGE, STORM_REVENGE_RAGE);
            }
        }
        state.RevengeUntil = 0;
        if (HasThunderMarker(player, MOD_COMMANDING_THUNDER))
            threat += COMMANDING_THREAT;

        std::vector<Unit*> targets;
        if (target->IsAlive())
            targets.push_back(target);
        bool focused = HasThunderMarker(player, MOD_FOCUSED_PEAL);
        if (focused)
            damage *= FOCUSED_ECHO;
        else if (HasThunderMarker(player, MOD_REVERBERATING_IMPACT))
        {
            for (Unit* enemy : EnemiesAround(player, target, REVERBERATING_RADIUS))
                if (enemy != target && targets.size() < ECHO_TARGETS)
                    targets.push_back(enemy);
            target->CastSpell(target, SPELL_THUNDER_ECHO_BURST, true);
        }
        else
        {
            bool distant = HasThunderMarker(player, MOD_DISTANT_RUMBLE);
            float facing = player->GetAngle(target);
            for (Unit* enemy : EnemiesAround(player, player, distant ? DISTANT_CONE : ECHO_CONE))
                if (enemy != target && targets.size() < ECHO_TARGETS
                    && InArc(player->GetPosition(), facing, enemy->GetPosition(), ECHO_HALF_ARC))
                    targets.push_back(enemy);
            player->CastSpell(player, SPELL_THUNDER_ECHO_BURST, true);
        }

        for (Unit* enemy : targets)
        {
            float each = damage;
            if (enemy == target && HasThunderMarker(player, MOD_COMMANDING_THUNDER))
                each *= COMMANDING_MORE;
            if (HasThunderMarker(player, MOD_DISTANT_RUMBLE) && !player->IsWithinDist2d(enemy, ECHO_CONE))
                each *= DISTANT_MORE;
            if (HasThunderMarker(player, MOD_DEAFENING_IMPACT) && enemy->GetAuraOfRankedSpell(SPELL_THUNDER_CLAP, me))
                each *= DEAFENED_MORE;
            int32 points = std::max<int32>(1, int32(each));
            player->CastCustomSpell(enemy, SPELL_THUNDER_ECHO, &points, nullptr, nullptr, true);
            if (!enemy->IsAlive())
                continue;
            if (threat > 0.0f && enemy->CanHaveThreatList())
                enemy->AddThreat(player, float(points) * threat, SPELL_SCHOOL_MASK_NATURE);
            if (enemy == target)
            {
                if (HasThunderMarker(player, MOD_STORMHEART)
                    && player->GetAuraOfRankedSpell(SPELL_LIGHTNING_SHIELD, me))
                    Strike(player, enemy, SPELL_STORMHEART,
                        RunePower(player, SPELL_SCHOOL_MASK_NATURE, 0) * STORMHEART_POWER);
                if (HasThunderMarker(player, MOD_FLAME_ECHO) && enemy->GetAuraOfRankedSpell(SPELL_FLAME_SHOCK, me))
                    AddDot(player, enemy, SPELL_FLAME_ECHO, float(points) * FLAME_ECHO_SHARE, FLAME_ECHO_MS);
                if (HasThunderMarker(player, MOD_FROST_ECHO) && enemy->HasAura(SPELL_FROST_FEVER, me))
                {
                    Strike(player, enemy, SPELL_FROST_ECHO,
                        player->GetTotalAttackPowerValue(BASE_ATTACK) * FROST_ECHO_AP
                            * SchoolBonus(player, SPELL_SCHOOL_MASK_FROST));
                    player->CastSpell(enemy, SPELL_FROST_ECHO_SLOW, true);
                    if (Aura* slow = enemy->GetAura(SPELL_FROST_ECHO_SLOW, me))
                    {
                        slow->SetMaxDuration(FROST_ECHO_MS);
                        slow->SetDuration(FROST_ECHO_MS);
                    }
                }
            }
            if (!HasThunderMarker(player, MOD_DEAFENING_IMPACT))
                continue;
            player->CastSpell(enemy, SPELL_DEAFENING_IMPACT, true);
            if (Aura* slow = enemy->GetAura(SPELL_DEAFENING_IMPACT, me))
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
            Report(player, SPELL_SHELTERING_REVERBERATION, 'A', absorb);
            if (Aura* aura = player->GetAura(SPELL_SHELTERING_REVERBERATION))
            {
                aura->SetMaxDuration(SHELTERING_MS);
                aura->SetDuration(SHELTERING_MS);
            }
        }
        if (HasThunderMarker(player, MOD_TIDAL_SHIELD) && state.EarthShieldHeal > 0
            && player->GetAuraOfRankedSpell(SPELL_EARTH_SHIELD, me)
            && Ready(state.TidalShieldAt, TIDAL_SHIELD_EVERY_MS))
        {
            state.TidalShieldAt = Now();
            Ward(player, player, SPELL_TIDAL_SHIELD, float(state.EarthShieldHeal) * TIDAL_SHIELD_SHARE);
            if (Aura* aura = player->GetAura(SPELL_TIDAL_SHIELD, me))
            {
                aura->SetMaxDuration(TIDAL_SHIELD_MS);
                aura->SetDuration(TIDAL_SHIELD_MS);
            }
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
        bool shield = spellId == SPELL_TIDAL_SHELTER || spellId == SPELL_JADE_SHELTER
            || spellId == SPELL_EARTHEN_COCOON;
        Report(player, spellId, shield ? 'A' : 'H', amount);
    }

    bool HasEarthShield(Player* player, Unit* unit)
    {
        for (uint32 id = SPELL_EARTH_SHIELD; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (unit->HasAura(id, player->GetGUID()))
                return true;
        return false;
    }

    float WellPower(Player* player)
    {
        return float(player->SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_NATURE));
    }

    // The Wellspring on the ally (the Hero's), else nullptr.
    Aura* WellOn(Player* player, Unit* ally)
    {
        return ally ? ally->GetAura(SPELL_WELLSPRING_MARK, player->GetGUID()) : nullptr;
    }

    // A Wellspring laid on the bearer, or renewed (8 sec; Lasting Spring 12).
    void LayWell(Player* player, Unit* bearer)
    {
        int32 ms = HasWellMarker(player, MOD_LASTING_SPRING) ? LASTING_WELL_MS : WELL_MS;
        Aura* well = WellOn(player, bearer);
        if (!well)
            well = player->AddAura(SPELL_WELLSPRING_MARK, bearer);
        if (well)
        {
            well->SetMaxDuration(ms);
            well->SetDuration(ms);
        }
    }

    // A Riptide or Renew of the Hero's on the bearer with their Wellspring (direct): Riptide's charge (the
    // interaction), Riptide Spring, Renewing Spring.
    void WellTouch(Player* player, Unit* bearer, uint32 firstRank)
    {
        if (!WellOn(player, bearer))
            return;
        State& state = StateOf(player);
        if (firstRank == SPELL_RIPTIDE)
        {
            state.WellRiptideUntil = Now() + WELL_CHARGE_MS;
            if (HasWellMarker(player, MOD_RIPTIDE_SPRING))
            {
                LayWell(player, bearer);
                std::vector<Unit*> others = InjuredAllies(player, bearer, RIPTIDE_SPRING_RANGE, bearer);
                if (!others.empty() && Ready(state.RiptideSpringAt, RIPTIDE_SPRING_EVERY_MS))
                {
                    state.RiptideSpringAt = Now();
                    Mend(player, others.front(), SPELL_RIPTIDE_SPRING, WellPower(player) * RIPTIDE_SPRING_POWER);
                }
            }
        }
        else if (firstRank == SPELL_RENEW && HasWellMarker(player, MOD_RENEWING_SPRING))
        {
            LayWell(player, bearer);
            state.WellRenewCharge = true;
        }
        else if (firstRank == SPELL_HOLY_SHOCK_HEAL && HasWellMarker(player, MOD_GUIDED_CURRENT))
            state.GuidedUntil = Now() + GUIDED_MS;
    }

    // Earth Shield's heal on the bearer: their Wellspring renewed; Flowing Waters.
    void WellShield(Player* player, Unit* bearer)
    {
        if (!WellOn(player, bearer))
            return;
        LayWell(player, bearer);
        State& state = StateOf(player);
        if (!HasWellMarker(player, MOD_FLOWING_WATERS) || !Ready(state.FlowingAt, FLOWING_EVERY_MS))
            return;
        std::vector<Unit*> allies = InjuredAllies(player, bearer, FLOWING_RANGE_WELL, bearer);
        if (allies.empty())
            return;
        state.FlowingAt = Now();
        Mend(player, allies.front(), SPELL_FLOWING_WATERS, WellPower(player) * FLOWING_POWER);
    }

    // One release's heal on an ally and its Undertow (Tidal Shelter, Renewing Current, Jade Current, Shielded
    // Spring) and Widening Waters.
    void WellHeal(Player* player, Unit* from, Unit* ally, float amount)
    {
        ObjectGuid me = player->GetGUID();
        if (HasWellMarker(player, MOD_DEEP_CURRENT))
            amount *= ally->GetHealthPct() < DEEP_CURRENT_BELOW_PCT ? DEEP_CURRENT : DEEP_CURRENT_ABOVE;
        if (HasWellMarker(player, MOD_TIDAL_SURGE))
            if (Aura* riptide = ally->GetAuraOfRankedSpell(SPELL_RIPTIDE, me))
            {
                amount *= TIDAL_SURGE;
                ExtendAura(riptide, TIDAL_SURGE_MS, TIDAL_SURGE_MOST_MS);
            }
        int32 heal = std::max<int32>(1, int32(amount));
        int32 effective = Effective(ally, heal);
        if (from != ally)
            from->CastSpell(ally, SPELL_WELLSPRING_JUMP, true);
        player->CastCustomSpell(ally, SPELL_WELLSPRING_HEAL, &heal, nullptr, nullptr, true);
        if (HasWellMarker(player, MOD_WIDENING_WATERS))
            Refresh(player, ally, SPELL_WIDENING_WATERS,
                std::max<int32>(1, int32(WellPower(player) * WIDENING_POWER / WIDENING_TICKS)));
        if (HasWellMarker(player, MOD_JADE_CURRENT))
            if (AuraEffect const* mist = ally->GetAuraEffect(SPELL_SOOTHING_MIST, EFFECT_0, me))
                Mend(player, ally, SPELL_JADE_CURRENT, mist->GetAmount() * JADE_CURRENT_SHARE);
        if (effective <= 0)
            return;
        if (HasWellMarker(player, MOD_TIDAL_SHELTER))
            Refresh(player, ally, SPELL_TIDAL_SHELTER, int32(effective * UNDERTOW_SHARE));
        if (HasWellMarker(player, MOD_RENEWING_CURRENT))
            Refresh(player, ally, SPELL_RENEWING_CURRENT, int32(effective * UNDERTOW_SHARE / UNDERTOW_TICKS));
        if (HasWellMarker(player, MOD_SHIELDED_SPRING) && ally->GetAuraOfRankedSpell(SPELL_POWER_WORD_SHIELD, me))
            Refresh(player, ally, SPELL_SHIELDED_SPRING, int32(effective * SHIELDED_SHARE));
    }

    // The bearer's Wellspring released by a Chain Heal that healed them for received (before overhealing).
    void ReleaseWell(Player* player, Unit* bearer, float received)
    {
        State& state = StateOf(player);
        float range = HasWellMarker(player, MOD_LONG_REACH) ? LONG_REACH
            : HasWellMarker(player, MOD_WIDENING_WATERS) ? WIDENING_RANGE : WELL_RANGE;
        float amount = received * WELLSPRING_SHARE;
        if (HasWellMarker(player, MOD_LASTING_SPRING))
            amount *= LASTING_SPRING;
        if (HasWellMarker(player, MOD_LONG_REACH))
            amount *= LONG_REACH_MORE;
        if (HasWellMarker(player, MOD_CONCENTRATED_CURRENT))
            amount *= CONCENTRATED_CURRENT;
        if (state.WellRiptideUntil && Now() < state.WellRiptideUntil)
            amount *= WELL_RIPTIDE;
        state.WellRiptideUntil = 0;
        if (state.WellRenewCharge)
            amount *= RENEWING_SPRING;
        state.WellRenewCharge = false;
        std::vector<Unit*> allies = InjuredAllies(player, bearer, range, bearer);
        if (allies.empty())
        {
            WellHeal(player, bearer, bearer, received * BEARER_SHARE);
            return;
        }
        bool split = HasWellMarker(player, MOD_SPLIT_TRIBUTARY) && allies.size() >= SPLIT_ALLIES;
        bool guided = state.GuidedUntil && Now() < state.GuidedUntil;
        state.GuidedUntil = 0;
        std::size_t count = split ? SPLIT_ALLIES : 1;
        for (std::size_t i = 0; i < count; ++i)
            WellHeal(player, bearer, allies[i], amount * (split ? SPLIT_SHARE : 1.0f));
        if (guided && allies.size() > count)
            WellHeal(player, bearer, allies[count], amount * GUIDED_SHARE);
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

    // One Wandering Mist's heal on the ally, and what follows it: Renewing Mist, Tidal Mist, Jade Shelter, Earthen
    // Cocoon.
    void MistHeal(Player* player, Unit* ally, float raw)
    {
        State& state = StateOf(player);
        ObjectGuid me = player->GetGUID();
        int32 amount = std::max<int32>(1, int32(raw));
        int32 effective = Effective(ally, amount);
        player->CastCustomSpell(ally, SPELL_WANDERING_MIST, &amount, nullptr, nullptr, true);
        if (HasMistMarker(player, MOD_RENEWING_MIST) && Ready(state.RenewingMistAt, RENEWING_MIST_EVERY_MS))
            if (Aura* renew = ally->GetAuraOfRankedSpell(SPELL_RENEW, me))
            {
                state.RenewingMistAt = Now();
                ExtendAura(renew, MIST_RENEW_MS, MIST_RENEW_MOST_MS);
            }
        if (effective <= 0)
            return;
        if (HasMistMarker(player, MOD_TIDAL_MIST) && ally->GetAuraOfRankedSpell(SPELL_RIPTIDE, me)
            && Ready(state.TidalMistAt, TIDAL_MIST_EVERY_MS))
        {
            std::vector<Unit*> others = InjuredAllies(player, ally, TIDAL_MIST_RANGE, ally);
            if (!others.empty())
            {
                state.TidalMistAt = Now();
                int32 heal = std::max<int32>(1, int32(effective * TIDAL_MIST_SHARE));
                player->CastCustomSpell(others.front(), SPELL_TIDAL_MIST, &heal, nullptr, nullptr, true);
            }
        }
        if (HasMistMarker(player, MOD_JADE_SHELTER) && Ready(state.JadeShelterAt, JADE_SHELTER_EVERY_MS))
        {
            state.JadeShelterAt = Now();
            Refresh(player, ally, SPELL_JADE_SHELTER, std::max<int32>(1, int32(effective * JADE_SHELTER_SHARE)));
        }
        if (HasMistMarker(player, MOD_EARTHEN_COCOON) && HasEarthShield(player, ally)
            && Ready(state.EarthenCocoonAt, EARTHEN_COCOON_EVERY_MS))
        {
            state.EarthenCocoonAt = Now();
            Refresh(player, ally, SPELL_EARTHEN_COCOON, std::max<int32>(1, int32(effective * EARTHEN_COCOON_SHARE)));
        }
    }

    // A Wandering Mist: to the most injured ally within 20 yards of the channel target (30 with Flowing Currents), or
    // to two at 15% each (Drifting Mist); 15% more on the Hero's Renew target, 25% more below half health (Gathering
    // Mists).
    void Wander(Player* player, Unit* channelTarget, float tick)
    {
        float range = HasMistMarker(player, MOD_FLOWING_CURRENTS) ? FLOWING_RANGE : WANDER_RANGE;
        std::vector<Unit*> allies = InjuredAllies(player, channelTarget, range, channelTarget);
        bool drifting = HasMistMarker(player, MOD_DRIFTING_MIST);
        if (allies.size() > (drifting ? 2u : 1u))
            allies.resize(drifting ? 2 : 1);
        for (Unit* ally : allies)
        {
            float amount = tick * (drifting ? DRIFTING_SHARE : WANDER_SHARE);
            if (ally->GetAuraOfRankedSpell(SPELL_RENEW, player->GetGUID()))
                amount *= MIST_RENEW;
            if (HasMistMarker(player, MOD_GATHERING_MISTS) && ally->GetHealthPct() < GATHERING_BELOW_PCT)
                amount *= GATHERING_MISTS;
            MistHeal(player, ally, amount);
        }
    }

    // Holy Shock or Chain Heal begun on the Soothing Mist target (the channel then ends): their heal there still
    // counts as on the channel target (Holy Thread, Chain of Jade).
    void MistWatch(Player* player)
    {
        if (Unit* target = MistChannelTarget(player))
        {
            State& state = StateOf(player);
            state.MistWatched = target->GetGUID();
            state.MistWatchedAt = Now();
        }
    }

    // Holy Shock's heal or Chain Heal's first on the channel target: Holy Thread, Chain of Jade.
    void MistTechnique(Player* player, Unit* target, uint32 firstRank, float heal)
    {
        State& state = StateOf(player);
        bool watched = target == MistChannelTarget(player)
            || (target->GetGUID() == state.MistWatched && Now() - state.MistWatchedAt <= uint32(MIST_WATCH_MS));
        if (!watched || heal <= 0.0f)
            return;
        Unit* ally = SecondaryAlly(player, target);
        if (!ally)
            return;
        if (firstRank == SPELL_HOLY_SHOCK_HEAL && HasMistMarker(player, MOD_HOLY_THREAD)
            && Ready(state.HolyThreadAt, HOLY_THREAD_EVERY_MS))
        {
            state.HolyThreadAt = Now();
            int32 amount = std::max<int32>(1, int32(heal * HOLY_THREAD_SHARE));
            player->CastCustomSpell(ally, SPELL_HOLY_THREAD, &amount, nullptr, nullptr, true);
        }
        else if (firstRank == SPELL_CHAIN_HEAL && HasMistMarker(player, MOD_CHAIN_OF_JADE)
            && Ready(state.ChainOfJadeAt, CHAIN_OF_JADE_EVERY_MS))
        {
            state.ChainOfJadeAt = Now();
            MistHeal(player, ally, heal * CHAIN_OF_JADE_SHARE);
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
            case SPELL_OPEN_WOUND: return SPELL_OPEN_WOUND_TICK;
            case SPELL_GHOSTFANG: return SPELL_GHOSTFANG_TICK;
            case SPELL_ARCANE_ECHO: return SPELL_ARCANE_ECHO_TICK;
            case SPELL_FROST_WOUND: return SPELL_FROST_WOUND_TICK;
            case SPELL_CLINGING_EMBERS: return SPELL_CLINGING_EMBERS_TICK;
            case SPELL_LUNAR_WOUND: return SPELL_LUNAR_WOUND_TICK;
            case SPELL_ASTRAL_RUPTURE: return SPELL_ASTRAL_RUPTURE_TICK;
            case SPELL_PHOENIX_BURN: return SPELL_PHOENIX_BURN_TICK;
            case SPELL_FLAME_ECHO: return SPELL_FLAME_ECHO_TICK;
            case SPELL_LINGERING_TORMENT: return SPELL_LINGERING_TORMENT_TICK;
            case SPELL_DECAYING_ICE: return SPELL_DECAYING_ICE_TICK;
            case SPELL_ICY_WOUND: return SPELL_ICY_WOUND_TICK;
            case SPELL_RENDING_IMPACT: return SPELL_RENDING_IMPACT_TICK;
            case SPELL_PHANTOM_WOUND: return SPELL_PHANTOM_WOUND_TICK;
            case SPELL_RENDING_MAUL: return SPELL_RENDING_MAUL_TICK;
            case SPELL_CORRUPTED_BLADE: return SPELL_CORRUPTED_BLADE_TICK;
            case SPELL_CINDER_WOUND: return SPELL_CINDER_WOUND_TICK;
            case SPELL_CINDER_WOUND_SPREAD: return SPELL_CINDER_WOUND_TICK;
            case SPELL_WINTER_WOUND: return SPELL_WINTER_WOUND_TICK;
            case SPELL_BLAZING_BLEED: return SPELL_BLAZING_BLEED_TICK;
            case SPELL_FROSTBITE: return SPELL_FROSTBITE_TICK;
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
        if (amount)
            Report(player, spellId, DotDamageSpell(spellId) ? 'D' : spellId == SPELL_CRIMSON_WARD ? 'A'
                : spellId == SPELL_GRAVE_CHILL ? 'S' : 'H', *amount);
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

    // The wake's attack: 96% of a normalized swing to each enemy along it (up to 6), its primary target more;
    // Bloodletting: a bleed of 30% of the hit every second for 4 sec, refreshed; Second Wind: 1% of
    // the Hero's health back for each enemy hit; Battlefield Claim: each enemy hit marked.
    void WakeStrike(Player* player, Shape const& shape, ObjectGuid primary, float share)
    {
        if (!player->IsInWorld() || !player->IsAlive())
            return;
        float swing = float(player->CalculateDamage(BASE_ATTACK, true, true));
        float bonus = HasOnslaughtMarker(player, MOD_EXECUTIONERS_CUT) ? EXECUTIONER_PRIMARY : WAKE_PRIMARY;
        bool bleed = HasOnslaughtMarker(player, MOD_BLOODLETTING);
        bool claim = HasOnslaughtMarker(player, MOD_BATTLEFIELD_CLAIM);
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
            if (claim && enemy->IsAlive())
                player->CastSpell(enemy, SPELL_BATTLEFIELD_CLAIM, true);
            if (!bleed || !enemy->IsAlive())
                continue;
            int32 tick = std::max<int32>(1, int32(damage * BLEED_SHARE));
            Lasting(player, enemy, SPELL_WAKE_BLEED, BLEED_TICKS * 1000, &tick);
        }
    }

    // The next different melee ability (on any enemy) reopens the wake: its attack along the wake a moment later,
    // Aftershock a second impact there 1.5 sec on at half. Once (twice with Overpowering Echo: the wake stays for the
    // next ability); an ability striking several enemies rips it once.
    void ReopenWake(Player* player)
    {
        State& state = StateOf(player);
        if (state.WakeRippedAt && Now() - state.WakeRippedAt < RIP_GAP_MS)
            return;
        state.WakeRippedAt = Now();
        Shape shape;
        shape.Cone = state.WakeCrescent;
        shape.From = state.WakeFrom;
        shape.To = state.WakeTo;
        shape.Facing = state.WakeFacing;
        shape.HalfWidth = state.WakeHalfWidth;
        shape.Radius = CRESCENT_RADIUS;
        shape.Arc = CRESCENT_ARC;
        ObjectGuid primary = state.WakePrimary;
        if (state.WakeRips > 1)
            --state.WakeRips;
        else
            EndWake(player);
        uint32 delay = WAKE_DELAY_MS;
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

    // Mortal Strike or Bloodthirst: a new wake through the target (the last one gone), 6 sec, fixed where it was cut
    // (following the target with Relentless).
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
        state.WakeUntil = Now() + WAKE_MS;
        state.WakeRips = HasOnslaughtMarker(player, MOD_OVERPOWERING_ECHO) ? ECHO_RIPS : 1;
        state.WakeRippedAt = 0;
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

    float DawnPower(Player* player)
    {
        return std::max(float(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_HOLY)),
            player->GetTotalAttackPowerValue(BASE_ATTACK) / 2.0f);
    }

    // The vowed ally (in reach of the Hero's world), else the Hero.
    Unit* VowOf(Player* player)
    {
        State& state = StateOf(player);
        Unit* vow = state.Vow ? ObjectAccessor::GetUnit(*player, state.Vow) : nullptr;
        if (!vow || !vow->IsAlive() || !vow->IsInMap(player) || !vow->GetAura(SPELL_DAWN_VOWED, player->GetGUID()))
            return player;
        return vow;
    }

    // Dawn Vow on the ally (or the Hero): the last vow let go; Passing the Torch.
    void SwearVow(Player* player, Unit* ally)
    {
        State& state = StateOf(player);
        if (Unit* old = state.Vow ? ObjectAccessor::GetUnit(*player, state.Vow) : nullptr)
            old->RemoveAurasDueToSpell(SPELL_DAWN_VOWED, player->GetGUID());
        bool moved = state.Vow != ally->GetGUID();
        state.Vow = ally->GetGUID();
        player->AddAura(SPELL_DAWN_VOWED, ally);
        if (moved && HasDawnMarker(player, MOD_PASSING_THE_TORCH))
            Mend(player, ally, SPELL_DAWN_ECHO_HEAL, DawnPower(player) * TORCH_POWER);
        state.PilgrimVow = ally->GetPosition();
    }

    void ClearVow(Player* player)
    {
        State& state = StateOf(player);
        if (Unit* old = state.Vow ? ObjectAccessor::GetUnit(*player, state.Vow) : nullptr)
            old->RemoveAurasDueToSpell(SPELL_DAWN_VOWED, player->GetGUID());
        state.Vow.Clear();
    }

    // The marked enemy (alive, the Hero's), else nullptr.
    Unit* DawnMarked(Player* player)
    {
        State& state = StateOf(player);
        Unit* mark = state.DawnMark ? ObjectAccessor::GetUnit(*player, state.DawnMark) : nullptr;
        return Valid(player, mark) && mark->GetAura(SPELL_DAWN_MARK, player->GetGUID()) ? mark : nullptr;
    }

    // One echo's effect on a unit from the vow: a heal (Chain of Grace) or a strike (Frostfire Dawn, Crusader's
    // Flame).
    void DawnTouch(Player* player, Unit* from, Unit* unit, float amount, bool heal)
    {
        ObjectGuid me = player->GetGUID();
        from->CastSpell(unit, heal ? SPELL_DAWN_BOLT_HEAL : SPELL_DAWN_BOLT, true);
        if (heal)
        {
            Mend(player, unit, SPELL_DAWN_ECHO_HEAL, amount);
            return;
        }
        Strike(player, unit, SPELL_DAWN_ECHO_DAMAGE, amount * SchoolBonus(player, SPELL_SCHOOL_MASK_HOLY));
        if (HasDawnMarker(player, MOD_CRUSADERS_FLAME) && unit->GetAuraOfRankedSpell(SPELL_FLAME_SHOCK, me))
            Strike(player, unit, SPELL_CRUSADERS_FLAME, DawnPower(player) * CRUSADERS_FIRE_POWER
                * SchoolBonus(player, SPELL_SCHOOL_MASK_FIRE));
    }

    // A Dawn Echo from the vow (scale: Steadfast's again); the Doctrine, its shape and what follows.
    void DawnEcho(Player* player, float scale)
    {
        State& state = StateOf(player);
        ObjectGuid me = player->GetGUID();
        Unit* vow = VowOf(player);
        float power = DawnPower(player) * scale;
        // The Doctrine: a heal (and how much) or a strike.
        bool heal = true;
        float amount = power * DAWN_POWER;
        if (HasDawnMarker(player, MOD_MERCY))
            amount = power * MERCY_POWER;
        else if (HasDawnMarker(player, MOD_RETRIBUTION))
            heal = false, amount = power * RETRIBUTION_POWER;
        else if (HasDawnMarker(player, MOD_COMMUNION))
        {
            heal = !state.CommunionStrike;
            state.CommunionStrike = !state.CommunionStrike;
            amount = power * (heal ? COMMUNION_HEAL : COMMUNION_STRIKE);
        }
        else if (HasDawnMarker(player, MOD_EARTHEN_OATH) && vow->GetAuraOfRankedSpell(SPELL_EARTH_SHIELD, me))
        {
            amount = power * EARTHEN_OATH_POWER;
            Ward(player, vow, SPELL_RADIANT_WARD, power * EARTHEN_OATH_WARD);
            if (Aura* ward = vow->GetAura(SPELL_RADIANT_WARD, me))
            {
                ward->SetMaxDuration(EARTHEN_OATH_MS);
                ward->SetDuration(EARTHEN_OATH_MS);
            }
        }
        else if (HasDawnMarker(player, MOD_CRUSADERS_FLAME))
            heal = false, amount = power * CRUSADERS_FLAME_POWER;
        // The targets: the most injured allies within 10 yards of the vow, or the marked enemy within 20.
        std::vector<Unit*> targets;
        Unit* mark = DawnMarked(player);
        if (heal)
            targets = InjuredAllies(player, vow, DAWN_REACH, nullptr);
        else if (mark && mark->IsWithinDistInMap(vow, DAWN_STRIKE_REACH))
        {
            targets.push_back(mark);
            for (Unit* enemy : EnemiesAround(player, mark, SUNBURST_RADIUS))
                if (enemy != mark)
                    targets.push_back(enemy);
        }
        if (targets.empty())
            return;
        if (!heal && state.FrostDawnUntil && Now() < state.FrostDawnUntil)
        {
            state.FrostDawnUntil = 0;
            Strike(player, targets.front(), SPELL_FROSTFIRE_DAWN, power * FROSTFIRE_DAWN_POWER);
        }
        std::size_t count = 1;
        float share = 1.0f;
        if (HasDawnMarker(player, MOD_DAWNWAVE) && mark)
        {
            // From the vow toward the marked enemy: up to 3 on the way, half each.
            std::vector<Unit*> path;
            std::vector<Unit*> pool = heal ? InjuredAllies(player, vow, vow->GetExactDist2d(mark) + DAWN_REACH, nullptr)
                : EnemiesAround(player, vow, vow->GetExactDist2d(mark) + DAWN_REACH);
            for (Unit* unit : pool)
                if (path.size() < DAWNWAVE_TARGETS
                    && SegmentDist(unit->GetPosition(), vow->GetPosition(), mark->GetPosition()) <= DAWNWAVE_HALF_WIDTH)
                    path.push_back(unit);
            if (!path.empty())
            {
                targets = path;
                count = path.size();
                share = DAWNWAVE_SHARE;
            }
        }
        else if (HasDawnMarker(player, MOD_DAWNWAVE))
            count = std::min<std::size_t>(DAWNWAVE_TARGETS, targets.size()), share = DAWNWAVE_SHARE;
        count = std::min(count, targets.size());
        for (std::size_t i = 0; i < count; ++i)
            DawnTouch(player, vow, targets[i], amount * share, heal);
        std::size_t next = count;
        auto more = [&](float part)
        {
            if (next < targets.size())
                DawnTouch(player, vow, targets[next++], amount * part, heal);
        };
        if (HasDawnMarker(player, MOD_SUNBURST))
        {
            vow->CastSpell(vow, SPELL_SUNBURST, true);
            for (std::size_t i = 0; i < SUNBURST_TARGETS; ++i)
                if (next < targets.size() && targets[next]->IsWithinDist2d(vow, SUNBURST_RADIUS + DAWN_REACH))
                    more(SUNBURST_SHARE);
        }
        if (heal && state.GraceUntil && Now() < state.GraceUntil)
        {
            state.GraceUntil = 0;
            more(DAWN_GRACE_SHARE);
        }
        if (state.PilgrimCharge)
        {
            state.PilgrimCharge = false;
            more(PILGRIMAGE_SHARE);
        }
        state.PilgrimHero = player->GetPosition();
        state.PilgrimVow = vow->GetPosition();
        if (HasDawnMarker(player, MOD_SACRED_GROUND))
        {
            state.SacredAt = vow->GetPosition();
            state.SacredMap = player->GetMapId();
            state.SacredUntil = Now() + SACRED_MS;
            state.SacredTick = DawnPower(player) * SACRED_POWER;
            state.SacredHeal = true;
            state.SacredTimer = 0;
            PlaceDecals(player, SPELL_SACRED_GROUND, { state.SacredAt });
        }
        if (HasDawnMarker(player, MOD_RENEWED_OATH) && Ready(state.OathAt, OATH_EVERY_MS))
            if (Aura* renew = vow->GetAuraOfRankedSpell(SPELL_RENEW, me))
            {
                state.OathAt = Now();
                ExtendAura(renew, OATH_MS, OATH_MOST_MS);
            }
        if (scale >= 1.0f && vow->GetAura(SPELL_DAWN_AFTERIMAGE, me))
        {
            vow->RemoveAurasDueToSpell(SPELL_DAWN_AFTERIMAGE, me);
            DawnEcho(player, STEADFAST_SHARE);
        }
    }

    // Crusader Strike on the marked enemy, Holy Shock, Searing Vow's Lava Lash: an echo (3 sec apart).
    void SendEcho(Player* player)
    {
        State& state = StateOf(player);
        if (!Ready(state.DawnEchoAt, DAWN_EVERY_MS))
            return;
        state.DawnEchoAt = Now();
        DawnEcho(player, 1.0f);
    }

    // The Dawnbound Crusader's other abilities (the Hero's own): Judgment marks; Crusader Strike on the mark, Holy
    // Shock (Steadfast's afterimage first) and Lava Lash on it (Searing Vow) echo; Frostbolt on it (Frostfire Dawn),
    // Chain Heal on the vow (Chain of Grace).
    void DawnAbility(Player* player, Unit* target, uint32 firstRank)
    {
        State& state = StateOf(player);
        ObjectGuid me = player->GetGUID();
        bool judgment = firstRank == SPELL_JUDGEMENT_OF_LIGHT || firstRank == SPELL_JUDGEMENT_OF_WISDOM
            || firstRank == SPELL_JUDGEMENT_OF_JUSTICE;
        if (judgment && Valid(player, target))
        {
            if (Unit* old = DawnMarked(player); old && old != target)
                old->RemoveAurasDueToSpell(SPELL_DAWN_MARK, me);
            state.DawnMark = target->GetGUID();
            Lasting(player, target, SPELL_DAWN_MARK, DAWN_MARK_MS);
            return;
        }
        Unit* mark = DawnMarked(player);
        if (firstRank == SPELL_CRUSADER_STRIKE && target == mark)
            SendEcho(player);
        else if (firstRank == SPELL_HOLY_SHOCK)
        {
            if (HasDawnMarker(player, MOD_STEADFAST))
                Lasting(player, VowOf(player), SPELL_DAWN_AFTERIMAGE, AFTERIMAGE_MS);
            SendEcho(player);
        }
        else if (firstRank == SPELL_LAVA_LASH && target == mark && HasDawnMarker(player, MOD_SEARING_VOW))
            SendEcho(player);
        else if (firstRank == SPELL_FROSTBOLT && target == mark && HasDawnMarker(player, MOD_FROSTFIRE_DAWN))
            state.FrostDawnUntil = Now() + DAWN_CHARGE_MS;
        else if (firstRank == SPELL_CHAIN_HEAL && target == VowOf(player) && HasDawnMarker(player, MOD_CHAIN_OF_GRACE))
            state.GraceUntil = Now() + DAWN_CHARGE_MS;
    }

    // Sacred Ground's ticks (one injured ally in it a second); Shared Pilgrimage's 8 yards.
    void DawnTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        if (HasDawnMarker(player, MOD_SHARED_PILGRIMAGE) && !state.PilgrimCharge)
        {
            Unit* vow = VowOf(player);
            if (player->GetExactDist2d(&state.PilgrimHero) >= PILGRIMAGE_DISTANCE
                || vow->GetExactDist2d(&state.PilgrimVow) >= PILGRIMAGE_DISTANCE)
                state.PilgrimCharge = true;
        }
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
        for (Unit* ally : InjuredAllies(player, player, 60.0f, nullptr))
            if (patch.Holds(ally))
            {
                Mend(player, ally, SPELL_DAWN_ECHO_HEAL, state.SacredTick);
                return;
            }
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

    // Mind Flay: a Hollow Echo beside the Hero, 10 sec; a fourth replaces the oldest (Possession: one an enemy, a
    // new one on the same enemy replacing its echo).
    void CallEcho(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        std::vector<Creature*> echoes = LiveEchoes(player);
        if (HasHollowMarker(player, MOD_POSSESSION) && target)
            for (Creature* echo : echoes)
                if (echo->IsAIEnabled && echo->AI()->GetGUID(0) == target->GetGUID())
                    if (TempSummon* summon = echo->ToTempSummon())
                    {
                        summon->UnSummon();
                        echoes = LiveEchoes(player);
                        break;
                    }
        if (echoes.size() >= MAX_HOLLOW_ECHOES)
            if (TempSummon* oldest = echoes.front()->ToTempSummon())
            {
                oldest->UnSummon();
                state.HollowEchoes.erase(state.HollowEchoes.begin());
            }
        uint32 ms = HOLLOW_ECHO_MS;
        Position pos = player->GetNearPosition(1.5f, frand(0.0f, 2.0f * float(M_PI)));
        TempSummon* echo = player->GetMap()->SummonCreature(NPC_HOLLOW_ECHO, pos,
            sSummonPropertiesStore.LookupEntry(SUMMON_PROPERTIES_GUARDIAN), ms, player);
        if (!echo)
            return;
        state.HollowEchoes.push_back(echo->GetGUID());
        if (echo->IsAIEnabled)
            echo->AI()->SetGUID(target ? target->GetGUID() : ObjectGuid::Empty, 0);
    }

    // A barrage bolt of an echo's: Plague Sermon 20% more on the Hero's Corruption target; its first (with
    // Procession 30% more) also strikes another of their Corruption targets within 8 yards for half; Frostbound
    // Sermon, each also one more enemy within 8 yards for half.
    void HollowBolt(Creature* echo, Player* hero, Unit* at, int32 bolt, bool first, bool frostbound)
    {
        ObjectGuid me = hero->GetGUID();
        float amount = float(bolt);
        if (first && HasHollowMarker(hero, MOD_PROCESSION))
            amount *= PROCESSION;
        auto corrupted = [me](Unit* unit) { return unit->GetAuraOfRankedSpell(SPELL_CORRUPTION, me) != nullptr; };
        int32 points = std::max<int32>(1, int32(amount
            * (HasHollowMarker(hero, MOD_PLAGUE_SERMON) && corrupted(at) ? PLAGUE_SERMON : 1.0f)));
        echo->CastCustomSpell(at, SPELL_HOLLOW_BOLT, &points, nullptr, nullptr, true);
        int32 half = std::max<int32>(1, int32(amount * CORRUPTION_SHARE));
        if (first)
            for (Unit* other : EnemiesAround(hero, at, SPLIT_RANGE_HOLLOW))
                if (other != at && corrupted(other))
                {
                    echo->CastCustomSpell(other, SPELL_HOLLOW_BOLT, &half, nullptr, nullptr, true);
                    break;
                }
        if (frostbound)
            for (Unit* other : EnemiesAround(hero, at, SPLIT_RANGE_HOLLOW))
                if (other != at)
                {
                    int32 split = std::max<int32>(1, int32(amount * FROSTBOUND_SHARE));
                    echo->CastCustomSpell(other, SPELL_HOLLOW_BOLT, &split, nullptr, nullptr, true);
                    break;
                }
    }

    // A barrage over on its victim: on the Hero's Shadow Word: Pain target, a patch (3 sec, renewed); Renewed
    // Witness, in their Renew, 30% of spell power healed (8 sec apart).
    void BarrageDone(Player* player, Unit* victim)
    {
        State& state = StateOf(player);
        if (victim && victim->GetAuraOfRankedSpell(SPELL_SHADOW_WORD_PAIN, player->GetGUID()))
        {
            float z = victim->GetPositionZ();
            player->UpdateGroundPositionZ(victim->GetPositionX(), victim->GetPositionY(), z);
            state.PatchAt = Position(victim->GetPositionX(), victim->GetPositionY(), z);
            state.PatchMap = player->GetMapId();
            state.PatchUntil = Now() + PATCH_MS;
            state.PatchTimer = 0;
            state.PatchTick = RunePower(player, SPELL_SCHOOL_MASK_SHADOW, SPELL_MIND_BLAST) * PATCH_POWER;
            player->RemoveDynObject(SPELL_HOLLOW_PATCH);
            PlaceDecals(player, SPELL_HOLLOW_PATCH, { state.PatchAt });
        }
        if (HasHollowMarker(player, MOD_RENEWED_WITNESS) && Ready(state.WitnessAt, WITNESS_EVERY_MS)
            && player->GetAuraOfRankedSpell(SPELL_RENEW, player->GetGUID()))
        {
            state.WitnessAt = Now();
            int32 heal = std::max<int32>(1, int32(RunePower(player, SPELL_SCHOOL_MASK_SHADOW, SPELL_MIND_BLAST, false)
                * WITNESS_POWER));
            Report(player, SPELL_HOLLOW_MEND, 'H', heal);
            player->CastCustomSpell(player, SPELL_HOLLOW_MEND, &heal, nullptr, nullptr, true);
        }
    }

    // The Shadow Word: Pain patch: 10% of spell power a second to up to 3 enemies in it.
    void HollowPatchTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        if (Now() >= state.PatchUntil || player->GetMapId() != state.PatchMap)
        {
            state.PatchUntil = 0;
            player->RemoveDynObject(SPELL_HOLLOW_PATCH);
            return;
        }
        state.PatchTimer += diff;
        if (state.PatchTimer < PATCH_TICK_MS)
            return;
        state.PatchTimer = 0;
        for (Unit* enemy : EnemiesIn(player, Circle(state.PatchAt, PATCH_RADIUS), PATCH_TARGETS))
            Strike(player, enemy, SPELL_HOLLOW_PATCH_TICK, state.PatchTick);
    }

    // Devouring Command: a direct Shadow Bolt turns an echo on its target, a hit of 30% of spell power (8 sec
    // apart; no barrage). Frostbound Sermon: a direct Frostbolt on an echo's target, its next barrage splits.
    void HollowStrike(Player* player, Unit* enemy, uint32 firstRank)
    {
        State& state = StateOf(player);
        std::vector<Creature*> echoes = LiveEchoes(player);
        if (echoes.empty())
            return;
        if (firstRank == SPELL_SHADOW_BOLT && HasHollowMarker(player, MOD_DEVOURING_COMMAND)
            && Ready(state.DevouringAt, DEVOURING_EVERY_MS))
        {
            state.DevouringAt = Now();
            Creature* echo = echoes.front();
            if (echo->IsAIEnabled)
                echo->AI()->SetGUID(enemy->GetGUID(), 0);
            int32 hit = std::max<int32>(1, int32(RunePower(player, SPELL_SCHOOL_MASK_SHADOW, SPELL_MIND_BLAST)
                * DEVOURING_POWER));
            echo->CastCustomSpell(enemy, SPELL_DEVOURING_COMMAND, &hit, nullptr, nullptr, true);
        }
        if (firstRank == SPELL_FROSTBOLT && HasHollowMarker(player, MOD_FROSTBOUND_SERMON))
            for (Creature* echo : echoes)
                if (echo->IsAIEnabled && echo->AI()->GetGUID(0) == enemy->GetGUID())
                    state.Frostbound.insert(echo->GetGUID());
    }

    // Mind Blast's command: every echo turns on its target (Dissonance: each on another enemy with the Hero's
    // Shadow Word: Pain, the target first), in a barrage of 3 bolts (Unison: together, 20% more) or a piercing
    // scream along the way (Accusation); then their channels resume on it. Procession: the echoes beside its target
    // first.
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
            if (HasHollowMarker(player, MOD_PROCESSION))
            {
                Position beside = victim->GetNearPosition(2.0f, frand(0.0f, 2.0f * float(M_PI)));
                echo->NearTeleportTo(beside.GetPositionX(), beside.GetPositionY(), beside.GetPositionZ(),
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
                    BarrageDone(hero, at);
                }, Milliseconds(unison ? 0 : 150 * i));
                continue;
            }
            bool frostbound = player->IsInWorld() && StateOf(player).Frostbound.erase(echo->GetGUID()) > 0;
            for (uint32 b = 0; b < BARRAGE_BOLTS; ++b)
                echo->m_Events.AddEventAtOffset([echo, owner, victimGuid, bolt, b, frostbound]()
                {
                    Player* hero = ObjectAccessor::GetPlayer(*echo, owner);
                    Unit* at = ObjectAccessor::GetUnit(*echo, victimGuid);
                    if (!hero || !at || !at->IsAlive())
                        return;
                    HollowBolt(echo, hero, at, bolt, b == 0, frostbound);
                    if (b + 1 == BARRAGE_BOLTS)
                        BarrageDone(hero, at);
                }, Milliseconds(b * BARRAGE_SPACING_MS + (unison ? 0 : 150 * i)));
        }
    }

    // ---- The Shattered Sun ---------------------------------------------------------------------------------------

    bool HasSunMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_SUN_MODIFIER_FIRST, modifier);
    }

    float SunPower(Player* player)
    {
        return RunePower(player, SpellSchoolMask(SPELL_SCHOOL_MASK_FIRE | SPELL_SCHOOL_MASK_FROST), SPELL_FIREBALL);
    }

    // Solar Anchor's shards on the ground (their look).
    void DrawAnchors(Player* player)
    {
        if (!HasSunMarker(player, MOD_SOLAR_ANCHOR))
            return;
        std::vector<Position> points;
        for (State::Sunshard const& shard : StateOf(player).Shards)
            if (shard.Map == player->GetMapId())
                points.push_back(shard.At);
        PlaceDecals(player, SPELL_SOLAR_ANCHOR, points);
    }

    // A Sunshard in the target for 10 sec (2 hosts at most, 3 with Twin Suns: a new one replaces the oldest).
    void Embed(Player* player, Unit* target, bool storm)
    {
        State& state = StateOf(player);
        ObjectGuid me = player->GetGUID();
        auto old = std::find_if(state.Shards.begin(), state.Shards.end(),
            [target](State::Sunshard const& shard) { return shard.Host == target->GetGUID(); });
        if (old != state.Shards.end())
            state.Shards.erase(old);
        std::size_t most = HasSunMarker(player, MOD_TWIN_SUNS) ? TWIN_HOSTS : SHARD_HOSTS;
        while (state.Shards.size() >= most)
        {
            if (Unit* host = ObjectAccessor::GetUnit(*player, state.Shards.front().Host))
                host->RemoveAurasDueToSpell(SPELL_SUNSHARD, me);
            state.Shards.erase(state.Shards.begin());
        }
        State::Sunshard shard;
        shard.Host = target->GetGUID();
        shard.At = target->GetPosition();
        shard.Map = player->GetMapId();
        shard.Until = Now() + SUNSHARD_MS;
        shard.Lunar = HasSunMarker(player, MOD_LUNAR_SHARD) && target->GetAuraOfRankedSpell(SPELL_MOONFIRE, me);
        shard.Storm = storm;
        state.Shards.push_back(shard);
        Lasting(player, target, SPELL_SUNSHARD, SUNSHARD_MS);
        DrawAnchors(player);
    }

    // The shard a frost spell on the target fractures: its own, or (Solar Anchor) one lying within 6 yards of it.
    std::vector<State::Sunshard>::iterator ShardFor(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        bool anchor = HasSunMarker(player, MOD_SOLAR_ANCHOR);
        return std::find_if(state.Shards.begin(), state.Shards.end(), [&](State::Sunshard const& shard)
        {
            if (shard.Host == target->GetGUID())
                return true;
            return anchor && shard.Map == player->GetMapId() && target->GetExactDist2d(&shard.At) <= ANCHOR_REACH;
        });
    }

    // A fracture on the host (scale: Chain Reaction's half): Frostfire to it and about it (the options' shapes), its
    // Broken Core, Flame Shock longer (Flame Burst), Scalding Ground.
    void Fracture(Player* player, Unit* host, State::Sunshard shard, float scale)
    {
        State& state = StateOf(player);
        ObjectGuid me = player->GetGUID();
        if (Unit* old = ObjectAccessor::GetUnit(*player, shard.Host))
            old->RemoveAurasDueToSpell(SPELL_SUNSHARD, me);
        float power = SunPower(player);
        float main = power * FRACTURE_HOST * scale;
        if (HasSunMarker(player, MOD_TWIN_SUNS))
            main *= TWIN_SUNS;
        if (HasSunMarker(player, MOD_SOLAR_ANCHOR))
            main *= SOLAR_ANCHOR;
        float splash = power * FRACTURE_SPLASH * scale;
        Position at = host->GetPosition();
        host->CastSpell(host, SPELL_FRACTURE_LOOK, true);
        Strike(player, host, SPELL_FRACTURE, main);
        if (shard.Lunar)
            Strike(player, host, SPELL_LUNAR_SHARD, RunePower(player, SPELL_SCHOOL_MASK_ARCANE, 0) * LUNAR_POWER);
        if (shard.Storm)
            Strike(player, host, SPELL_STORM_SHARD,
                RunePower(player, SPELL_SCHOOL_MASK_NATURE, 0) * STORM_SHARD_POWER);
        if (HasSunMarker(player, MOD_SPLINTERSTORM))
        {
            float facing = player->GetAngle(host);
            std::size_t count = 1;
            for (Unit* enemy : EnemiesAround(player, host, SPLINTER_RANGE))
                if (enemy != host && count < SUN_TARGETS
                    && InArc(at, facing, enemy->GetPosition(), SPLINTER_HALF_ARC))
                {
                    ++count;
                    Strike(player, enemy, SPELL_FRACTURE, main * SPLINTER_SHARE);
                }
        }
        else
        {
            std::size_t count = 0;
            for (Unit* enemy : EnemiesAround(player, host, FRACTURE_RADIUS))
                if (enemy != host && count++ < FRACTURE_TARGETS)
                    Strike(player, enemy, SPELL_FRACTURE, splash);
        }
        if (HasSunMarker(player, MOD_SOLAR_LANCE))
        {
            Position end = Toward(at, player->GetAngle(host), SOLAR_LANCE_RANGE);
            std::size_t count = 0;
            for (Unit* enemy : EnemiesAround(player, host, SOLAR_LANCE_RANGE))
                if (enemy != host && count < SOLAR_LANCE_TARGETS
                    && SegmentDist(enemy->GetPosition(), at, end) <= SOLAR_LANCE_HALF_WIDTH
                    && player->GetExactDist2d(enemy) > player->GetExactDist2d(host))
                {
                    ++count;
                    Strike(player, enemy, SPELL_FRACTURE, main * SOLAR_LANCE_SHARE);
                }
        }
        if (HasSunMarker(player, MOD_ORBITING_SUN))
            if (Unit* chosen = player->GetSelectedUnit(); chosen != host && Valid(player, chosen)
                && player->IsWithinDistInMap(chosen, ORBITING_RANGE))
                Strike(player, chosen, SPELL_FRACTURE, main * ORBITING_SHARE);
        if (Aura* flame = host->GetAuraOfRankedSpell(SPELL_FLAME_SHOCK, me))
        {
            ExtendAura(flame, SUN_FLAME_MS, SUN_FLAME_MOST_MS);
            if (HasSunMarker(player, MOD_FLAME_BURST) && Ready(state.FlameBurstAt, FLAME_BURST_EVERY_MS))
                for (Unit* enemy : EnemiesAround(player, host, FLAME_BURST_RANGE))
                    if (enemy != host && !enemy->GetAuraOfRankedSpell(SPELL_FLAME_SHOCK, me))
                    {
                        state.FlameBurstAt = Now();
                        if (Aura* copy = player->AddAura(flame->GetId(), enemy))
                        {
                            copy->SetMaxDuration(flame->GetDuration());
                            copy->SetDuration(flame->GetDuration());
                        }
                        break;
                    }
        }
        if (host->IsAlive())
        {
            Lasting(player, host, SPELL_BROKEN_CORE, BROKEN_CORE_MS);
            state.CoreLanced.erase(host->GetGUID());
        }
        if (HasSunMarker(player, MOD_SCALDING_GROUND))
        {
            state.ScaldAt = at;
            state.ScaldMap = player->GetMapId();
            state.ScaldUntil = Now() + SCALDING_MS;
            state.ScaldTick = power * SCALDING_POWER;
            state.ScaldTimer = 0;
            PlaceDecals(player, SPELL_SCALDING_GROUND, { at });
        }
    }

    // A frost spell (Frostbolt; Ice Lance with Frozen Flash) on the target: the shard it reaches fractures.
    void SunFrost(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        auto shard = ShardFor(player, target);
        if (shard == state.Shards.end())
            return;
        State::Sunshard taken = *shard;
        state.Shards.erase(shard);
        DrawAnchors(player);
        Fracture(player, target, taken, 1.0f);
    }

    // Fireball on a Broken Core: the collapse, 40% (Implosion 50% more); Chain Reaction, Second Sunrise, Icy Shelter.
    void Collapse(Player* player, Unit* host)
    {
        State& state = StateOf(player);
        ObjectGuid me = player->GetGUID();
        if (!host->GetAura(SPELL_BROKEN_CORE, me))
            return;
        host->RemoveAurasDueToSpell(SPELL_BROKEN_CORE, me);
        state.CoreLanced.erase(host->GetGUID());
        float collapse = SunPower(player) * COLLAPSE_POWER * (HasSunMarker(player, MOD_IMPLOSION) ? IMPLOSION : 1.0f);
        host->CastSpell(host, SPELL_COLLAPSE_LOOK, true);
        Strike(player, host, SPELL_COLLAPSE, collapse);
        if (HasSunMarker(player, MOD_CHAIN_REACTION))
            for (Unit* other : EnemiesAround(player, host, CHAIN_RADIUS))
            {
                auto shard = std::find_if(state.Shards.begin(), state.Shards.end(),
                    [other](State::Sunshard const& each) { return each.Host == other->GetGUID(); });
                if (other == host || shard == state.Shards.end())
                    continue;
                State::Sunshard taken = *shard;
                state.Shards.erase(shard);
                DrawAnchors(player);
                Fracture(player, other, taken, CHAIN_SHARE);
                break;
            }
        if (HasSunMarker(player, MOD_SECOND_SUNRISE))
        {
            ObjectGuid hostGuid = host->GetGUID();
            float again = collapse * SUNRISE_SHARE;
            player->m_Events.AddEventAtOffset([player, hostGuid, again]()
            {
                if (!player->IsInWorld() || !player->IsAlive())
                    return;
                Unit* target = ObjectAccessor::GetUnit(*player, hostGuid);
                if (!target || !target->IsAlive())
                    return;
                target->CastSpell(target, SPELL_COLLAPSE_LOOK, true);
                Strike(player, target, SPELL_COLLAPSE, again);
                std::size_t count = 0;
                for (Unit* enemy : EnemiesAround(player, target, SUNRISE_RADIUS))
                    if (enemy != target && count++ < SUNRISE_TARGETS)
                        Strike(player, enemy, SPELL_COLLAPSE, again);
            }, Milliseconds(SUNRISE_DELAY_MS));
        }
        if (HasSunMarker(player, MOD_ICY_SHELTER) && player->GetAuraOfRankedSpell(SPELL_ICE_BARRIER, me)
            && Ready(state.IcyShelterAt, ICY_SHELTER_EVERY_MS))
        {
            state.IcyShelterAt = Now();
            Ward(player, player, SPELL_ICY_SHELTER, SunPower(player) * ICY_SHELTER_POWER);
            if (Aura* aura = player->GetAura(SPELL_ICY_SHELTER, me))
            {
                aura->SetMaxDuration(ICY_SHELTER_MS);
                aura->SetDuration(ICY_SHELTER_MS);
            }
        }
    }

    // The Shattered Sun's spells (the Hero's own): Fireball (collapse, then a shard), Frostbolt (a fracture), Ice
    // Lance (its Core hit; Frozen Flash), Lightning Bolt (Storm Shard).
    void SunSpell(Player* player, Unit* target, uint32 firstRank)
    {
        State& state = StateOf(player);
        ObjectGuid me = player->GetGUID();
        switch (firstRank)
        {
            case SPELL_FIREBALL:
                Collapse(player, target);
                if (target->IsAlive())
                    Embed(player, target, false);
                break;
            case SPELL_LIGHTNING_BOLT:
                if (HasSunMarker(player, MOD_STORM_SHARD) && target->IsAlive()
                    && Ready(state.StormShardAt, STORM_SHARD_EVERY_MS))
                {
                    state.StormShardAt = Now();
                    Embed(player, target, true);
                }
                break;
            case SPELL_FROSTBOLT:
                SunFrost(player, target);
                break;
            case SPELL_ICE_LANCE:
                if (target->GetAura(SPELL_BROKEN_CORE, me) && state.CoreLanced.insert(target->GetGUID()).second)
                {
                    bool shattering = HasSunMarker(player, MOD_SHATTERING_LANCE);
                    Strike(player, target, SPELL_CORE_LANCE, RunePower(player, SPELL_SCHOOL_MASK_FROST, 0)
                        * (shattering ? SHATTERING_LANCE_POWER : CORE_LANCE_POWER));
                    if (shattering)
                    {
                        player->RemoveAurasDueToSpell(SPELL_SHATTERING_LANCE);
                        player->CastSpell(player, SPELL_SHATTERING_LANCE, true);
                    }
                }
                if (HasSunMarker(player, MOD_FROZEN_FLASH) && Ready(state.FrozenFlashAt, FROZEN_FLASH_EVERY_MS)
                    && ShardFor(player, target) != state.Shards.end())
                {
                    state.FrozenFlashAt = Now();
                    SunFrost(player, target);
                }
                break;
            default:
                break;
        }
    }

    // Scalding Ground's ticks; the shards' times.
    void SunTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        std::size_t before = state.Shards.size();
        auto gone = [player](State::Sunshard const& shard)
        {
            Unit* host = ObjectAccessor::GetUnit(*player, shard.Host);
            bool anchored = HasSunMarker(player, MOD_SOLAR_ANCHOR);
            return Now() >= shard.Until || (!anchored && (!host || !host->IsAlive()));
        };
        state.Shards.erase(std::remove_if(state.Shards.begin(), state.Shards.end(), gone), state.Shards.end());
        if (state.Shards.size() != before)
        {
            if (state.Shards.empty())
                player->RemoveDynObject(SPELL_SOLAR_ANCHOR);
            else
                DrawAnchors(player);
        }
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
        for (Unit* enemy : EnemiesIn(player, Circle(state.ScaldAt, SCALDING_RADIUS), SUN_TARGETS))
            player->CastCustomSpell(enemy, SPELL_FRACTURE, &damage, nullptr, nullptr, true);
    }

    // ---- The Hungering Rift ------------------------------------------------------------------------------------

    uint32 KnownRank(Player* player, uint32 firstRank);

    bool HasRiftMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_RIFT_MODIFIER_FIRST, modifier);
    }

    // The Hero's distinct damage over time effects on the unit (5 at most).
    uint32 OwnDots(Player* player, Unit* unit)
    {
        std::set<uint32> dots;
        for (auto const& [id, application] : unit->GetAppliedAuras())
        {
            Aura const* aura = application->GetBase();
            if (aura->GetCasterGUID() != player->GetGUID())
                continue;
            for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                if (AuraEffect const* effect = aura->GetEffect(i); effect
                    && (effect->GetAuraType() == SPELL_AURA_PERIODIC_DAMAGE
                        || effect->GetAuraType() == SPELL_AURA_PERIODIC_LEECH
                        || effect->GetAuraType() == SPELL_AURA_PERIODIC_DAMAGE_PERCENT
                        || (effect->GetAuraType() == SPELL_AURA_PERIODIC_DUMMY && DotDamageSpell(aura->GetId()))))
                    dots.insert(aura->GetSpellInfo()->GetFirstRankSpell()->Id);
        }
        return std::min<uint32>(LASH_MOST_DOTS, uint32(dots.size()));
    }

    bool OwnCorruption(Player* player, Unit* unit)
    {
        return unit && unit->GetAuraOfRankedSpell(SPELL_CORRUPTION, player->GetGUID());
    }

    // Another enemy within range of the center with the Hero's aura (the chain's first rank), but except, else
    // nullptr.
    Unit* OtherAfflicted(Player* player, Unit* center, float range, uint32 firstRank, Unit* except)
    {
        for (Unit* enemy : EnemiesAround(player, center, range))
            if (enemy != center && enemy != except && enemy->GetAuraOfRankedSpell(firstRank, player->GetGUID()))
                return enemy;
        return nullptr;
    }

    // A Rift opened (or renewed) on the host.
    void OpenRift(Player* player, Unit* host)
    {
        State::RiftState& rift = StateOf(player).Rift;
        bool renew = rift.Until && Now() < rift.Until && rift.Host == host->GetGUID();
        if (!renew)
        {
            if (Unit* old = rift.Host ? ObjectAccessor::GetUnit(*player, rift.Host) : nullptr)
                old->RemoveAurasDueToSpell(SPELL_HUNGERING_RIFT, player->GetGUID());
            rift = State::RiftState();
            rift.Host = host->GetGUID();
            rift.Target = host->GetGUID();
            rift.At = host->GetPosition();
            rift.HostAt = host->GetPosition();
            rift.Map = player->GetMapId();
        }
        rift.Until = Now() + RIFT_MS;
        Lasting(player, host, SPELL_HUNGERING_RIFT, RIFT_MS);
    }

    void CloseRift(Player* player)
    {
        State::RiftState& rift = StateOf(player).Rift;
        if (Unit* host = rift.Host ? ObjectAccessor::GetUnit(*player, rift.Host) : nullptr)
            host->RemoveAurasDueToSpell(SPELL_HUNGERING_RIFT, player->GetGUID());
        rift = State::RiftState();
    }

    // Last Gasp's eruption about the unit.
    void RiftEruption(Player* player, Unit* at, float share)
    {
        float burst = RunePower(player, SPELL_SCHOOL_MASK_SHADOW, SPELL_SHADOW_BOLT) * LAST_GASP_POWER * share;
        std::size_t count = 0;
        for (Unit* enemy : EnemiesAround(player, at, LAST_GASP_RADIUS))
            if (count++ < LAST_GASP_TARGETS)
                Strike(player, enemy, SPELL_LAST_GASP, burst);
    }

    // One lash (share: At Your Back's extra one) on the host, and what follows it.
    void Lash(Player* player, Unit* host, float share)
    {
        State& state = StateOf(player);
        State::RiftState& rift = state.Rift;
        ObjectGuid me = player->GetGUID();
        float power = RunePower(player, SPELL_SCHOOL_MASK_SHADOW, SPELL_SHADOW_BOLT);
        float lash = power * LASH_POWER * (1.0f + LASH_PER_DOT * OwnDots(player, host)) * share;
        if (HasRiftMarker(player, MOD_DEVOURING_MAW))
            lash *= DEVOURING_MAW;
        if (HasRiftMarker(player, MOD_RIFT_OPEN_WOUND))
            lash *= 1.0f + std::min(OPEN_WOUND_MOST, OPEN_WOUND_STEP * float(rift.InRow++));
        if (HasRiftMarker(player, MOD_HAUNTING_RIFT) && host->GetExactDist2d(&rift.HostAt) >= HAUNTING_DISTANCE
            && Ready(rift.HauntAt, HAUNTING_EVERY_MS))
        {
            rift.HauntAt = Now();
            lash *= HAUNTING_RIFT;
        }
        rift.HostAt = host->GetPosition();
        Aura* haunt = HasRiftMarker(player, MOD_HAUNTED_MAW) ? host->GetAuraOfRankedSpell(SPELL_HAUNT, me) : nullptr;
        if (haunt)
        {
            lash *= HAUNTED_MAW;
            ExtendAura(haunt, HAUNTED_MAW_MS, HAUNTED_MAW_MOST_MS);
        }
        bool pending = rift.Pending;
        rift.Pending = false;
        bool tongues = HasRiftMarker(player, MOD_MANY_TONGUES);
        if (pending && tongues)
            lash *= MANY_TONGUES_PENDING;
        Strike(player, host, SPELL_RIFT_LASH, lash);
        ++rift.Lashes;
        // Others: Shadow Bolt's pending one, Many Tongues, Black Breath, Painful Anchoring.
        std::set<Unit*> struck = { host };
        auto also = [&](Unit* enemy, float part)
        {
            if (enemy && struck.insert(enemy).second)
                Strike(player, enemy, SPELL_RIFT_LASH, lash * part);
        };
        if (pending && !tongues)
            also(OtherAfflicted(player, host, RIFT_REACH, SPELL_CORRUPTION, nullptr), RIFT_PENDING_SHARE);
        if (tongues)
        {
            std::size_t count = 0;
            for (Unit* enemy : EnemiesAround(player, host, RIFT_REACH))
                if (enemy != host && count < MANY_TONGUES_TARGETS && OwnCorruption(player, enemy))
                {
                    ++count;
                    also(enemy, MANY_TONGUES_SHARE);
                }
        }
        if (HasRiftMarker(player, MOD_BLACK_BREATH))
        {
            float facing = player->GetAngle(host);
            std::size_t count = 0;
            for (Unit* enemy : EnemiesAround(player, host, BLACK_BREATH_RANGE))
                if (enemy != host && count < BLACK_BREATH_TARGETS
                    && InArc(host->GetPosition(), facing, enemy->GetPosition(), BLACK_BREATH_HALF_ARC))
                {
                    ++count;
                    also(enemy, BLACK_BREATH_SHARE);
                }
        }
        if (HasRiftMarker(player, MOD_PAINFUL_ANCHORING) && host->GetAuraOfRankedSpell(SPELL_SHADOW_WORD_PAIN, me))
            also(OtherAfflicted(player, host, RIFT_REACH, SPELL_SHADOW_WORD_PAIN, nullptr), PAINFUL_SHARE);
        // The host's: Hellmaw, Siphon Conduit, Lingering Torment, Endless Appetite, every third lash.
        if (HasRiftMarker(player, MOD_HELLMAW) && host->GetAuraOfRankedSpell(SPELL_IMMOLATE, me))
            Strike(player, host, SPELL_HELLMAW, RunePower(player, SPELL_SCHOOL_MASK_FIRE, 0) * HELLMAW_POWER);
        if (HasRiftMarker(player, MOD_SIPHON_CONDUIT))
            if (Spell* channel = player->GetCurrentSpell(CURRENT_CHANNELED_SPELL); channel
                && channel->m_spellInfo->GetFirstRankSpell()->Id == TankRunes::SPELL_DRAIN_LIFE
                && player->GetGuidValue(UNIT_FIELD_CHANNEL_OBJECT) == host->GetGUID())
            {
                float cap = float(player->GetMaxHealth()) * SIPHON_CONDUIT_CAP_PCT / 100.0f * LASH_MS / 1000.0f;
                Mend(player, player, SPELL_SIPHON_CONDUIT, std::min(lash * SIPHON_CONDUIT_SHARE, cap));
            }
        if (HasRiftMarker(player, MOD_LINGERING_TORMENT))
            AddDot(player, host, SPELL_LINGERING_TORMENT, lash * TORMENT_SHARE, TORMENT_MS);
        if (HasRiftMarker(player, MOD_ENDLESS_APPETITE))
            if (Aura* corruption = host->GetAuraOfRankedSpell(SPELL_CORRUPTION, me))
                ExtendAura(corruption, APPETITE_MS, APPETITE_MOST_MS);
        if (rift.Lashes % 3 == 0)
        {
            if (HasRiftMarker(player, MOD_LAST_GASP))
                RiftEruption(player, host, 1.0f);
            if (HasRiftMarker(player, MOD_DOOM_ECHO) && host->GetAuraOfRankedSpell(SPELL_CURSE_OF_AGONY, me))
                Strike(player, host, SPELL_DOOM_ECHO, power * DOOM_ECHO_POWER);
        }
    }

    // The Rift's lash timer; its host's death (Last Gasp, Endless Appetite); At Your Back's following.
    void RiftTick(Player* player, uint32 diff)
    {
        State::RiftState& rift = StateOf(player).Rift;
        if (!rift.Until)
            return;
        if (Now() >= rift.Until || rift.Map != player->GetMapId())
        {
            CloseRift(player);
            return;
        }
        Unit* host = ObjectAccessor::GetUnit(*player, rift.Host);
        if (HasRiftMarker(player, MOD_AT_YOUR_BACK))
        {
            Unit* chosen = player->GetSelectedUnit();
            if (Valid(player, chosen) && OwnCorruption(player, chosen)
                && player->IsWithinDistInMap(chosen, AT_YOUR_BACK_RANGE) && chosen != host)
            {
                if (host)
                    host->RemoveAurasDueToSpell(SPELL_HUNGERING_RIFT, player->GetGUID());
                host = chosen;
                rift.Host = chosen->GetGUID();
                Lasting(player, chosen, SPELL_HUNGERING_RIFT, int32(rift.Until - Now()));
                if (Ready(rift.BackAt, AT_YOUR_BACK_EVERY_MS))
                {
                    rift.BackAt = Now();
                    Lash(player, chosen, AT_YOUR_BACK_SHARE);
                }
            }
        }
        if (!host || !host->IsAlive())
        {
            if (HasRiftMarker(player, MOD_LAST_GASP) && host && rift.Lashes % 3)
                RiftEruption(player, host, LAST_GASP_DEATH);
            Unit* next = HasRiftMarker(player, MOD_ENDLESS_APPETITE) && host
                ? OtherAfflicted(player, host, APPETITE_REACH, SPELL_CORRUPTION, host) : nullptr;
            if (!next)
            {
                CloseRift(player);
                return;
            }
            rift.Host = next->GetGUID();
            rift.HostAt = next->GetPosition();
            Lasting(player, next, SPELL_HUNGERING_RIFT, int32(rift.Until - Now()));
            return;
        }
        bool reach = !HasRiftMarker(player, MOD_RIFT_OPEN_WOUND) || host->GetExactDist2d(&rift.At) <= OPEN_WOUND_REACH;
        Spell* channel = player->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        bool draining = channel && channel->m_spellInfo->GetFirstRankSpell()->Id == SPELL_DRAIN_SOUL
            && player->GetGuidValue(UNIT_FIELD_CHANNEL_OBJECT) == host->GetGUID();
        rift.Timer += diff;
        if (rift.Timer < (draining ? DRAIN_LASH_MS : LASH_MS))
            return;
        rift.Timer = 0;
        if (reach)
            Lash(player, host, 1.0f);
        else
            rift.InRow = 0;
    }

    // Shadow Bolt and Death Coil (the Hero's own) with the Hungering Rift.
    void RiftHit(Player* player, Unit* target, uint32 firstRank)
    {
        State::RiftState& rift = StateOf(player).Rift;
        bool deathCoil = firstRank == SPELL_DEATH_COIL_DK || firstRank == SPELL_DEATH_COIL_WARLOCK;
        bool onHost = rift.Until && Now() < rift.Until && rift.Host == target->GetGUID();
        if (firstRank == SPELL_SHADOW_BOLT)
        {
            if (onHost)
                rift.Pending = true;
            else if (OwnCorruption(player, target))
                OpenRift(player, target);
            return;
        }
        if (!deathCoil)
            return;
        if (onHost && HasRiftMarker(player, MOD_NECROTIC_APPETITE) && !rift.Extended)
        {
            rift.Extended = true;
            rift.Until += NECROTIC_MS;
            if (Aura* aura = target->GetAura(SPELL_HUNGERING_RIFT, player->GetGUID()))
            {
                aura->SetMaxDuration(aura->GetMaxDuration() + NECROTIC_MS);
                aura->SetDuration(aura->GetDuration() + NECROTIC_MS);
            }
            return;
        }
        if (OwnCorruption(player, target))
            OpenRift(player, target);
    }

    // ---- Howling North -----------------------------------------------------------------------------------------

    bool HasNorthMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_NORTH_MODIFIER_FIRST, modifier);
    }

    bool Northwind(Player* player, Unit* unit)
    {
        return unit && unit->GetAura(SPELL_NORTHWIND, player->GetGUID());
    }

    // One blade from the Hero through the target (power: its share of attack power, Rimeblade's part in it); its
    // pierce and what it leaves.
    void FrostBlade(Player* player, Unit* target, float power, bool last)
    {
        State& state = StateOf(player);
        ObjectGuid me = player->GetGUID();
        if (!target->IsAlive())
            return;
        float ap = player->GetTotalAttackPowerValue(BASE_ATTACK) * SchoolBonus(player, SPELL_SCHOOL_MASK_FROST);
        float blade = ap * power;
        if (HasNorthMarker(player, MOD_GLACIAL_EDGE))
            blade *= GLACIAL_EDGE;
        bool icy = state.NorthIcy;
        if (icy)
            blade *= HasNorthMarker(player, MOD_SHARPENED_WIND) ? SHARPENED_WIND : NORTH_ICY;
        if (HasNorthMarker(player, MOD_SHATTERING_COLD) && target->GetAura(SPELL_HUNGERING_COLD_FREEZE, me))
            blade *= SHATTERING_COLD;
        Strike(player, target, SPELL_FROST_BLADE, blade);
        // The pierce: Icy Touch's one, Winter's Reach's four, half each, on the line beyond the target.
        std::size_t pierces = HasNorthMarker(player, MOD_WINTERS_REACH) ? WINTERS_REACH_TARGETS : icy ? 1 : 0;
        if (pierces)
        {
            Position from = player->GetPosition();
            Position end = Toward(from, player->GetAngle(target), NORTH_LINE);
            std::size_t count = 0;
            for (Unit* enemy : EnemiesAround(player, player, NORTH_LINE))
                if (enemy != target && count < pierces
                    && SegmentDist(enemy->GetPosition(), from, end) <= NORTH_HALF_WIDTH
                    && player->GetExactDist2d(enemy) > player->GetExactDist2d(target))
                {
                    ++count;
                    Strike(player, enemy, SPELL_FROST_BLADE, blade * NORTH_PIERCE_SHARE);
                }
        }
        if (HasNorthMarker(player, MOD_PLAGUEWIND) && target->GetAura(SPELL_BLOOD_PLAGUE, me))
            for (Unit* enemy : EnemiesAround(player, target, PLAGUEWIND_RANGE))
                if (enemy != target)
                {
                    Strike(player, enemy, SPELL_FROST_BLADE, blade * PLAGUEWIND_SHARE);
                    break;
                }
        if (state.NorthHorn)
            Strike(player, target, SPELL_HORN_OF_THE_NORTH, ap * HORN_OF_THE_NORTH_AP);
        if (HasNorthMarker(player, MOD_DECAYING_ICE))
            for (uint32 id = SPELL_DEATH_AND_DECAY; id; id = sSpellMgr->GetNextSpellInChain(id))
                if (DynamicObject* ground = player->GetDynObject(id);
                    ground && target->GetExactDist2d(ground) <= ground->GetRadius())
                {
                    AddDot(player, target, SPELL_DECAYING_ICE, blade * DECAYING_ICE_SHARE, NORTH_DOT_MS);
                    break;
                }
        if (HasNorthMarker(player, MOD_ICY_WOUND))
            AddDot(player, target, SPELL_ICY_WOUND, blade * ICY_WOUND_SHARE, NORTH_DOT_MS);
        if (!last)
            return;
        state.NorthIcy = false;
        state.NorthHorn = false;
        if (state.NorthField)
        {
            state.NorthField = false;
            state.NorthFieldAt = target->GetPosition();
            state.NorthFieldMap = player->GetMapId();
            state.NorthFieldUntil = Now() + NORTH_FIELD_MS;
            state.NorthFieldTimer = 0;
            PlaceDecals(player, SPELL_NORTH_FIELD, { state.NorthFieldAt });
        }
        // Frost Strike's follow-up on this target; the options' next Frost Strike.
        state.FollowTarget = target->GetGUID();
        state.FollowUntil = Now() + NORTH_FOLLOW_MS;
        state.FollowStrikeMore = 1.0f;
        if (HasNorthMarker(player, MOD_NORTHERN_ENDURANCE))
        {
            state.FollowStrikeMore *= ENDURANCE_STRIKE;
            Lasting(player, player, SPELL_NORTHERN_ENDURANCE, 5000);
        }
        if (HasNorthMarker(player, MOD_GLACIAL_FLOW) && target->HasAura(SPELL_FROST_FEVER, me))
            state.FollowStrikeMore *= GLACIAL_FLOW;
        state.FollowStrikeUntil = state.FollowStrikeMore > 1.0f ? Now() + NORTH_FOLLOW_MS : 0;
        if (HasNorthMarker(player, MOD_COLD_RECOVERY) && Ready(state.ColdRecoveryAt, COLD_RECOVERY_EVERY_MS))
        {
            state.ColdRecoveryAt = Now();
            for (uint32 id = SPELL_HOWLING_BLAST; id; id = sSpellMgr->GetNextSpellInChain(id))
                if (player->HasSpell(id))
                    player->ModifySpellCooldown(id, -COLD_RECOVERY_MS);
        }
        if (HasNorthMarker(player, MOD_WINTERS_MENDING))
            state.MendingUntil = Now() + WINTERS_MENDING_MS;
    }

    // A readied blade loosed at the target (Obliterate's 60%, Frost Strike's 35%; Rimeblade's three).
    void LooseBlade(Player* player, Unit* target, float power)
    {
        State& state = StateOf(player);
        if (!state.NorthBlades || !Northwind(player, target) || !Ready(state.BladeAt, BLADE_EVERY_MS))
            return;
        --state.NorthBlades;
        state.BladeAt = Now();
        if (!HasNorthMarker(player, MOD_RIMEBLADE))
        {
            FrostBlade(player, target, power, true);
            return;
        }
        ObjectGuid guid = target->GetGUID();
        for (std::size_t i = 0; i < RIMEBLADES; ++i)
            player->m_Events.AddEventAtOffset([player, guid, power, i]()
            {
                if (!player->IsInWorld() || !player->IsAlive())
                    return;
                if (Unit* unit = ObjectAccessor::GetUnit(*player, guid))
                    FrostBlade(player, unit, power * RIMEBLADE_SHARE, i + 1 == RIMEBLADES);
            }, Milliseconds(i * RIMEBLADE_SPACING_MS));
    }

    // Howling North's abilities (the Hero's own): Howling Blast marks and readies 2 blades; Obliterate and Frost
    // Strike loose them; Icy Touch and Chains of Ice on a mark charge the next; Frost Strike follows a blade up; Horn
    // of Winter (Horn of the North).
    void NorthHit(Player* player, Unit* target, uint32 firstRank)
    {
        State& state = StateOf(player);
        if (firstRank == SPELL_HOWLING_BLAST)
        {
            if (!Ready(state.NorthwindAt, NORTHWIND_WINDOW_MS) && state.NorthMarks >= NORTHWIND_TARGETS)
                return;
            if (Ready(state.NorthwindAt, NORTHWIND_WINDOW_MS))
            {
                state.NorthwindAt = Now();
                state.NorthMarks = 0;
                state.NorthBlades = NORTH_BLADES;
            }
            ++state.NorthMarks;
            Lasting(player, target, SPELL_NORTHWIND, NORTHWIND_MS);
            return;
        }
        bool marked = Northwind(player, target);
        switch (firstRank)
        {
            case SPELL_OBLITERATE:
                LooseBlade(player, target, OBLITERATE_BLADE_AP);
                break;
            case SPELL_FROST_STRIKE:
                // The follow-up of an earlier blade first; then this one may loose its own.
                if (state.FollowUntil && Now() < state.FollowUntil && state.FollowTarget == target->GetGUID())
                {
                    state.FollowUntil = 0;
                    Strike(player, target, SPELL_NORTH_FOLLOW_UP, player->GetTotalAttackPowerValue(BASE_ATTACK)
                        * SchoolBonus(player, SPELL_SCHOOL_MASK_FROST)
                        * (HasNorthMarker(player, MOD_WINTERS_RETURN) ? WINTERS_RETURN_AP : NORTH_FOLLOW_AP));
                }
                LooseBlade(player, target, FROST_STRIKE_BLADE_AP);
                break;
            case SPELL_ICY_TOUCH:
                if (marked)
                    state.NorthIcy = true;
                break;
            case SPELL_CHAINS_OF_ICE:
                if (marked)
                    state.NorthField = true;
                break;
            default:
                break;
        }
    }

    // Horn of Winter with Horn of the North: the next blade adds Frost.
    void NorthHorn(Player* player)
    {
        if (HasNorthMarker(player, MOD_HORN_OF_THE_NORTH))
            StateOf(player).NorthHorn = true;
    }

    // Death Strike's heal after a blade (Winter's Mending: 30% more; spent).
    float NorthMending(Player* player, float heal)
    {
        State& state = StateOf(player);
        if (!state.MendingUntil)
            return heal;
        if (Now() < state.MendingUntil)
            heal *= WINTERS_MENDING;
        state.MendingUntil = 0;
        return heal;
    }

    // Frost Strike's damage: Northern Endurance's and Glacial Flow's 10%/20% more (spent).
    float NorthStrikeDamage(Player* player, float damage)
    {
        State& state = StateOf(player);
        if (!state.FollowStrikeUntil)
            return damage;
        if (Now() < state.FollowStrikeUntil)
            damage *= state.FollowStrikeMore;
        state.FollowStrikeUntil = 0;
        return damage;
    }

    // The Frostbolt field's ticks (Frozen Ground: 50% more, and its slow).
    void NorthTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        if (!state.NorthFieldUntil)
            return;
        if (Now() >= state.NorthFieldUntil || state.NorthFieldMap != player->GetMapId())
        {
            state.NorthFieldUntil = 0;
            player->RemoveDynObject(SPELL_NORTH_FIELD);
            return;
        }
        state.NorthFieldTimer += diff;
        if (state.NorthFieldTimer < 1000)
            return;
        state.NorthFieldTimer = 0;
        bool frozen = HasNorthMarker(player, MOD_FROZEN_GROUND);
        float tick = RunePower(player, SPELL_SCHOOL_MASK_FROST, 0) * NORTH_FIELD_POWER
            * (frozen ? FROZEN_GROUND_MORE : 1.0f);
        for (Unit* enemy : EnemiesIn(player, Circle(state.NorthFieldAt, NORTH_FIELD_RADIUS), NORTH_FIELD_TARGETS))
        {
            Strike(player, enemy, SPELL_NORTH_FIELD_TICK, tick);
            if (frozen && enemy->IsAlive())
                Lasting(player, enemy, SPELL_FROZEN_GROUND, 2000);
        }
    }

    // ---- Trueshot ----------------------------------------------------------------------------------------------

    bool HasTrueshotMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_TRUESHOT_MODIFIER_FIRST, modifier);
    }

    // Trueshot released: the Hero's Aimed Shot (its own known rank, free of cost and cooldown: Trueshot paid them),
    // made 45% stronger when it lands (TrueshotDamage).
    void ReleaseTrueshot(Player* player, Unit* target)
    {
        uint32 aimed = KnownRank(player, SPELL_AIMED_SHOT);
        if (!aimed || !target)
            return;
        StateOf(player).TrueshotPendingUntil = Now() + TRUESHOT_PENDING_MS;
        player->CastSpell(target, aimed, TRIGGERED_FULL_MASK);
    }

    // Aimed Shot's damage, Trueshot's (spent), and Arcane Shot's or Chimera Shot's (Exposed Target, Follow-Up Shot).
    float TrueshotDamage(Player* player, Unit* target, uint32 firstRank, float damage, bool triggered)
    {
        State& state = StateOf(player);
        if (firstRank == SPELL_AIMED_SHOT && triggered && state.TrueshotPendingUntil
            && Now() < state.TrueshotPendingUntil)
        {
            damage *= TRUESHOT_MORE;
            if (HasTrueshotMarker(player, MOD_PERFECT_SHOT))
                damage *= PERFECT_SHOT;
            if (HasTrueshotMarker(player, MOD_STEADY_AIM))
                damage *= STEADY_AIM_MORE;
            return damage;
        }
        if ((firstRank == SPELL_ARCANE_SHOT || firstRank == SPELL_CHIMERA_SHOT) && !triggered)
        {
            if (state.ExposedTarget == target->GetGUID() && target->GetAura(SPELL_EXPOSED_TARGET, player->GetGUID()))
            {
                damage *= HasTrueshotMarker(player, MOD_MARKED_WEAKNESS) ? MARKED_WEAKNESS : EXPOSED_MORE;
                target->RemoveAurasDueToSpell(SPELL_EXPOSED_TARGET, player->GetGUID());
                state.ExposedTarget.Clear();
            }
            if (firstRank == SPELL_ARCANE_SHOT && state.FollowUpUntil)
            {
                if (Now() < state.FollowUpUntil)
                    damage *= FOLLOW_UP_MORE;
                state.FollowUpUntil = 0;
            }
        }
        return damage;
    }

    bool TrueshotPending(Player* player)
    {
        State& state = StateOf(player);
        return state.TrueshotPendingUntil && Now() < state.TrueshotPendingUntil;
    }

    // Trueshot's Aimed Shot landing (its damage dealt): the pierce, the mark, the charges, the recovery.
    void TrueshotLanded(Player* player, Unit* target, float hit)
    {
        State& state = StateOf(player);
        ObjectGuid me = player->GetGUID();
        state.TrueshotPendingUntil = 0;
        bool multi = state.MultiChargeUntil && Now() < state.MultiChargeUntil;
        state.MultiChargeUntil = 0;
        bool arcane = state.ArcaneChargeUntil && Now() < state.ArcaneChargeUntil;
        state.ArcaneChargeUntil = 0;
        // The pierce behind the target.
        float share = HasTrueshotMarker(player, MOD_PIERCING_SHOT) ? PIERCING_SHARE : TRUESHOT_PIERCE_SHARE;
        if (HasTrueshotMarker(player, MOD_STEADY_AIM))
            share *= STEADY_AIM_PIERCE;
        if (multi)
            share *= MULTI_PIERCE_MORE;
        std::size_t most = TRUESHOT_PIERCES
            + (multi && HasTrueshotMarker(player, MOD_WIDE_TRAJECTORY) ? WIDE_MORE_PIERCES : 0);
        float width = multi ? WIDE_HALF_WIDTH : TRUESHOT_HALF_WIDTH;
        Position from = target->GetPosition();
        Position end = Toward(from, player->GetAngle(target), TRUESHOT_LINE);
        std::vector<Unit*> pierced;
        for (Unit* enemy : EnemiesAround(player, target, TRUESHOT_LINE))
            if (enemy != target && pierced.size() < most && SegmentDist(enemy->GetPosition(), from, end) <= width
                && player->GetExactDist2d(enemy) > player->GetExactDist2d(target))
                pierced.push_back(enemy);
        for (Unit* enemy : pierced)
            Strike(player, enemy, SPELL_TRUESHOT_PIERCE, hit * share);
        bool marked = target->GetAuraOfRankedSpell(SPELL_HUNTERS_MARK, me);
        if (marked)
        {
            state.ExposedTarget = target->GetGUID();
            Lasting(player, target, SPELL_EXPOSED_TARGET, TRUESHOT_EXPOSED_MS);
            if (HasTrueshotMarker(player, MOD_MARKED_RICOCHET))
                for (Unit* enemy : EnemiesAround(player, target, RICOCHET_RANGE))
                    if (enemy != target)
                    {
                        Strike(player, enemy, SPELL_TRUESHOT_PIERCE, hit * RICOCHET_SHARE);
                        break;
                    }
        }
        if (arcane)
        {
            float payload = player->GetTotalAttackPowerValue(RANGED_ATTACK)
                * SchoolBonus(player, SPELL_SCHOOL_MASK_ARCANE)
                * (HasTrueshotMarker(player, MOD_ARCANE_ARROW) ? ARCANE_ARROW_POWER : ARCANE_PAYLOAD_POWER);
            Strike(player, target, SPELL_ARCANE_PAYLOAD, payload);
            if (HasTrueshotMarker(player, MOD_ARCANE_CALIBRATION))
                for (std::size_t i = 0; i < pierced.size() && i < CALIBRATION_TARGETS; ++i)
                    Strike(player, pierced[i], SPELL_ARCANE_PAYLOAD, payload * CALIBRATION_SHARE);
        }
        float rap = player->GetTotalAttackPowerValue(RANGED_ATTACK);
        if (HasTrueshotMarker(player, MOD_VENOM_ARROW) && target->GetAuraOfRankedSpell(SPELL_SERPENT_STING, me))
            Strike(player, target, SPELL_VENOM_ARROW,
                rap * VENOM_ARROW_RAP * SchoolBonus(player, SPELL_SCHOOL_MASK_NATURE));
        if (HasTrueshotMarker(player, MOD_CONCUSSIVE_PIERCE) && target->GetAura(SPELL_CONCUSSIVE_SHOT, me))
            Strike(player, target, SPELL_CONCUSSIVE_PIERCE,
                rap * CONCUSSIVE_RAP * SchoolBonus(player, SPELL_SCHOOL_MASK_NORMAL));
        if (HasTrueshotMarker(player, MOD_QUICK_RECOVERY))
            for (uint32 id = SPELL_ARCANE_SHOT; id; id = sSpellMgr->GetNextSpellInChain(id))
                if (player->HasSpell(id))
                    player->ModifySpellCooldown(id, -QUICK_RECOVERY_MS);
        if (HasTrueshotMarker(player, MOD_HUNTERS_MOBILITY))
            Lasting(player, player, SPELL_HUNTERS_MOBILITY, 4000);
        if (HasTrueshotMarker(player, MOD_FOLLOW_UP_SHOT))
            state.FollowUpUntil = Now() + FOLLOW_UP_MS;
        if (HasTrueshotMarker(player, MOD_STEADY_RECOVERY))
        {
            player->RemoveAurasDueToSpell(SPELL_STEADY_RECOVERY);
            player->CastSpell(player, SPELL_STEADY_RECOVERY, true);
        }
        if (HasTrueshotMarker(player, MOD_MENDING_AIM) && Ready(state.RenewedAimAt, MENDING_AIM_EVERY_MS))
            if (Pet* pet = player->GetPet(); pet && pet->IsAlive() && pet->GetAuraOfRankedSpell(SPELL_MEND_PET, me))
            {
                state.RenewedAimAt = Now();
                Mend(player, pet, SPELL_MENDING_AIM, float(pet->GetMaxHealth()) * MENDING_AIM_PCT / 100.0f);
            }
    }

    // Trueshot's charges: Multi-Shot (a wider line), Silencing Shot (the payload).
    void TrueshotCharge(Player* player, uint32 firstRank)
    {
        State& state = StateOf(player);
        if (firstRank == SPELL_MULTI_SHOT)
            state.MultiChargeUntil = Now() + TRUESHOT_CHARGE_MS;
        else if (firstRank == SPELL_SILENCING_SHOT)
            state.ArcaneChargeUntil = Now() + TRUESHOT_CHARGE_MS;
    }

    // Trueshot (2 sec; Steady Aim's 2.5) in Aimed Shot's place while the rune is bound.
    void TeachTrueshot(Player* player, bool bound)
    {
        bool steady = bound && HasTrueshotMarker(player, MOD_STEADY_AIM);
        LegendaryRunes::ReplaceAbility(player, steady ? SPELL_TRUESHOT : SPELL_TRUESHOT_STEADY, { SPELL_AIMED_SHOT },
            false);
        LegendaryRunes::ReplaceAbility(player, steady ? SPELL_TRUESHOT_STEADY : SPELL_TRUESHOT, { SPELL_AIMED_SHOT },
            bound);
    }

    // ---- Scarlet Benediction -----------------------------------------------------------------------------------

    bool HasScarletMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_SCARLET_MODIFIER_FIRST, modifier);
    }

    // The Hero's healing power of a school with their healing done bonuses (the rune's heals ignore their modifiers).
    float RuneHealing(Player* player, SpellSchoolMask school)
    {
        return float(player->SpellBaseHealingBonusDone(school))
            * player->GetTotalAuraMultiplier(SPELL_AURA_MOD_HEALING_DONE_PERCENT);
    }

    bool HasPrayerOfMending(Player* player, Unit* ally)
    {
        for (uint32 id : { uint32(SPELL_PRAYER_OF_MENDING_BUFF), 48110u, 48111u })
            if (ally->HasAura(id, player->GetGUID()))
                return true;
        return false;
    }

    // Renew on an ally plants a dormant Scarlet Bloom there, as long as the Renew (one already there lasts as long
    // again, its openings kept), unless one opened there too lately.
    void PlantBloom(Player* player, Unit* ally)
    {
        ObjectGuid me = player->GetGUID();
        State::Bloom& bloom = StateOf(player).Blooms[ally->GetGUID()];
        if (Now() < bloom.RearmAt)
            return;
        Aura* renew = ally->GetAuraOfRankedSpell(SPELL_RENEW, me);
        int32 ms = renew ? renew->GetDuration() : 15000;
        Aura* planted = ally->GetAura(SPELL_SCARLET_BLOOM, me);
        if (!planted)
        {
            bloom.Opens = 0;
            planted = player->AddAura(SPELL_SCARLET_BLOOM, ally);
        }
        if (planted)
        {
            planted->SetMaxDuration(ms);
            planted->SetDuration(ms);
        }
    }

    // The Bloom on the ally opened (more: the opener's own share): its heal to the most injured other ally near it
    // (two with Scarlet Garden), else the ally shielded. False if there was none.
    bool OpenBloom(Player* player, Unit* ally, float more = 1.0f)
    {
        ObjectGuid me = player->GetGUID();
        if (!ally->HasAura(SPELL_SCARLET_BLOOM, me))
            return false;
        State::Bloom& bloom = StateOf(player).Blooms[ally->GetGUID()];
        float power = RuneHealing(player, SPELL_SCHOOL_MASK_HOLY) * more;
        Aura* renew = ally->GetAuraOfRankedSpell(SPELL_RENEW, me);
        if (renew && renew->GetMaxDuration() - renew->GetDuration() >= SCARLET_RIPE_MS)
            power *= HasScarletMarker(player, MOD_SCARLET_PATIENT_CULTIVATION) ? PATIENT_RIPE : SCARLET_RIPE;
        if (HasScarletMarker(player, MOD_SCARLET_MENDING_PETALS) && HasPrayerOfMending(player, ally))
            power *= MENDING_PETALS;
        if (HasScarletMarker(player, MOD_SCARLET_GUARDIAN_BLOOM) && ally->HasAura(SPELL_GUARDIAN_SPIRIT, me))
            power *= GUARDIAN_BLOOM;
        bool garden = HasScarletMarker(player, MOD_SCARLET_GARDEN);
        float share = garden ? SCARLET_GARDEN_SHARE
            : HasScarletMarker(player, MOD_SCARLET_DEEP_BLOOM) ? DEEP_BLOOM : SCARLET_OPENING;
        float range = HasScarletMarker(player, MOD_SCARLET_GUIDED_PETALS) ? GUIDED_PETALS_RANGE : SCARLET_RANGE;
        std::vector<Unit*> allies = InjuredAllies(player, ally, range, ally);
        if (allies.size() > (garden ? SCARLET_GARDEN_ALLIES : 1))
            allies.resize(garden ? SCARLET_GARDEN_ALLIES : 1);
        for (Unit* other : allies)
        {
            Mend(player, other, SPELL_SCARLET_OPENING, power * share);
            if (HasScarletMarker(player, MOD_SCARLET_BLOODROSE))
            {
                int32 tick = std::max<int32>(1, int32(power * BLOODROSE_SHARE / float(BLOODROSE_MS / IN_MILLISECONDS)));
                Lasting(player, other, SPELL_BLOODROSE, BLOODROSE_MS, &tick);
            }
        }
        if (allies.empty() || HasScarletMarker(player, MOD_SCARLET_SHELTERING_PETALS))
            Ward(player, ally, SPELL_SCARLET_PETALS, power * SCARLET_PETALS);
        if (renew && HasScarletMarker(player, MOD_SCARLET_RENEWING_FLOWER))
            renew->RefreshDuration();
        uint8 most = HasScarletMarker(player, MOD_SCARLET_SECOND_BLOOM) ? 2 : 1;
        if (++bloom.Opens >= most)
        {
            ally->RemoveAurasDueToSpell(SPELL_SCARLET_BLOOM, me);
            bloom.Opens = 0;
            bloom.RearmAt = Now()
                + (HasScarletMarker(player, MOD_SCARLET_SWIFT_BLOSSOM) ? SWIFT_BLOSSOM_MS : SCARLET_REARM_MS);
        }
        return true;
    }

    // The Hero's heals with Scarlet Benediction: Renew plants; Flash Heal, Greater Heal, Binding Heal, Prayer of
    // Healing (up to 3 a cast; opened counts them) and the options' Circle of Healing and Power Word: Shield open.
    void ScarletHeal(Player* player, Unit* target, uint32 firstRank, uint8& opened)
    {
        if (!player->IsFriendlyTo(target))
            return;
        switch (firstRank)
        {
            case SPELL_RENEW:
                PlantBloom(player, target);
                break;
            case SPELL_FLASH_HEAL:
            case SPELL_GREATER_HEAL:
                OpenBloom(player, target);
                break;
            case SPELL_BINDING_HEAL:
                if (target != player || HasScarletMarker(player, MOD_SCARLET_BINDING_PETALS))
                    OpenBloom(player, target);
                break;
            case SPELL_PRAYER_OF_HEALING:
            {
                bool choral = HasScarletMarker(player, MOD_SCARLET_CHORAL_BLOOM);
                if (opened < (choral ? CHORAL_OPENS : SCARLET_PRAYER_OPENS)
                    && OpenBloom(player, target, choral ? CHORAL_MORE : 1.0f))
                    ++opened;
                break;
            }
            case SPELL_CIRCLE_OF_HEALING:
                if (HasScarletMarker(player, MOD_SCARLET_CIRCLING_BLOOM) && opened < CIRCLING_OPENS
                    && OpenBloom(player, target))
                    ++opened;
                break;
            case SPELL_POWER_WORD_SHIELD:
                if (HasScarletMarker(player, MOD_SCARLET_SHIELDED_BLOOM))
                    OpenBloom(player, target);
                break;
            default:
                break;
        }
    }

    // ---- The Evergrove -----------------------------------------------------------------------------------------

    bool HasGroveMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_GROVE_MODIFIER_FIRST, modifier);
    }

    uint8 GroveMost(Player* player)
    {
        return HasGroveMarker(player, MOD_GROVE_FOURFOLD) ? GROVE_STACK + 1 : GROVE_STACK;
    }

    // The Hero's Rejuvenations on the ally: their own and the extra ones (0 without their own).
    uint8 GroveStacks(Player* player, Unit* ally)
    {
        ObjectGuid me = player->GetGUID();
        if (!ally->GetAuraOfRankedSpell(SPELL_REJUVENATION, me))
            return 0;
        uint8 stacks = 1;
        for (uint32 extra = 0; extra < GROVE_STACK; ++extra)
            if (ally->HasAura(SPELL_SECOND_REJUVENATION + extra, me))
                ++stacks;
        return stacks;
    }

    // The extra Rejuvenations on the ally (Swiftmend's bonus counts them).
    uint8 GroveExtras(Player* player, Unit* ally)
    {
        uint8 extras = 0;
        for (uint32 extra = 0; extra < GROVE_STACK; ++extra)
            if (ally->HasAura(SPELL_SECOND_REJUVENATION + extra, player->GetGUID()))
                ++extras;
        return extras;
    }

    // An extra Rejuvenation (15 sec): each 3 sec as much as the Hero's own Rejuvenation on the ally heals.
    void GrowExtra(Player* player, Unit* ally, uint32 spellId)
    {
        Aura* own = ally->GetAuraOfRankedSpell(SPELL_REJUVENATION, player->GetGUID());
        AuraEffect const* effect = own ? own->GetEffect(EFFECT_0) : nullptr;
        if (!effect)
            return;
        float tick = float(effect->GetAmount());
        if (HasGroveMarker(player, MOD_GROVE_DEEP_ROOTS))
            tick *= DEEP_ROOTS;
        if (HasGroveMarker(player, MOD_GROVE_THORNED) && ally->GetAuraOfRankedSpell(SPELL_THORNS, player->GetGUID()))
            tick *= THORNED_GROVE;
        int32 points = std::max<int32>(1, int32(tick));
        Lasting(player, ally, spellId, GROVE_ECHO_MS, &points);
    }

    // One more Rejuvenation on the ally: their own if it had none (the Hero's known rank, free), else the next
    // extra one; at the full stack, every extra one renewed. Grove Shelter: reaching the full stack shields it.
    void GrowStack(Player* player, Unit* ally)
    {
        ObjectGuid me = player->GetGUID();
        uint8 stacks = GroveStacks(player, ally);
        if (!stacks)
        {
            if (uint32 rank = KnownRank(player, SPELL_REJUVENATION))
                player->CastSpell(ally, rank, true);
            return;
        }
        uint8 most = GroveMost(player);
        if (stacks >= most)
        {
            for (uint32 extra = 0; extra + 1 < most; ++extra)
                GrowExtra(player, ally, SPELL_SECOND_REJUVENATION + extra);
            return;
        }
        for (uint32 extra = 0; extra + 1 < most; ++extra)
            if (!ally->HasAura(SPELL_SECOND_REJUVENATION + extra, me))
            {
                GrowExtra(player, ally, SPELL_SECOND_REJUVENATION + extra);
                break;
            }
        if (stacks + 1 >= most && HasGroveMarker(player, MOD_GROVE_SHELTER))
            Ward(player, ally, SPELL_GROVE_SHELTER, RuneHealing(player, SPELL_SCHOOL_MASK_NATURE) * GROVE_SHELTER);
    }

    // The ally grown to the full stack (Swift Grove, Innervating Grove).
    void GrowFull(Player* player, Unit* ally)
    {
        for (uint8 grown = 0; grown < GROVE_STACK + 1 && GroveStacks(player, ally) < GroveMost(player); ++grown)
            GrowStack(player, ally);
    }

    // The most injured ally within 30 yards of the target without a full stack (not the target), else nullptr.
    Unit* GroveSeek(Player* player, Unit* target)
    {
        for (Unit* ally : InjuredAllies(player, target, GROVE_SEEK_RANGE, target))
            if (GroveStacks(player, ally) < GroveMost(player))
                return ally;
        return nullptr;
    }

    // The Hero's own Rejuvenation landed on the target (before: the stacks it had then; full: it was at full health
    // then). On their Rejuvenation it grows a stack; at full health or on a full stack it seeks the most injured ally
    // instead (taken back off the target if it had none). Nature's Swiftness (Swift Grove): the full stack, spent.
    void GroveRejuvenation(Player* player, Unit* target, uint8 before, bool full)
    {
        uint8 most = GroveMost(player);
        Unit* grows = target;
        if (full || before >= most)
            if (Unit* seeker = GroveSeek(player, target))
            {
                if (!before)
                {
                    if (Aura* own = target->GetAuraOfRankedSpell(SPELL_REJUVENATION, player->GetGUID()))
                        own->Remove();
                }
                else if (before >= most)
                    GrowStack(player, target);              // a full stack there: renewed all the same
                grows = seeker;
            }
        bool swift = HasGroveMarker(player, MOD_GROVE_SWIFT) && player->HasAura(SPELL_NATURES_SWIFTNESS_DRUID);
        if (swift)
        {
            player->RemoveAurasDueToSpell(SPELL_NATURES_SWIFTNESS_DRUID);
            GrowFull(player, grows);
            return;
        }
        if (grows != target || before)
            GrowStack(player, grows);
    }

    // The Evergrove's other heals: Wild Growth (grown counts its allies a cast), Regrowth's Root, Lifebound's bloom,
    // Tranquil Grove, Rooted Touch, Innervating Grove.
    void GroveHeal(Player* player, Unit* target, uint32 firstRank, uint8& grown)
    {
        if (!player->IsFriendlyTo(target))
            return;
        bool has = GroveStacks(player, target) > 0;
        switch (firstRank)
        {
            case SPELL_WILD_GROWTH:
                if (has && grown < (HasGroveMarker(player, MOD_GROVE_WILD_SEEDING) ? WILD_SEEDING_GROWTHS
                    : GROVE_WILD_GROWTHS))
                {
                    ++grown;
                    GrowStack(player, target);
                }
                break;
            case SPELL_REGROWTH:
                if (has && HasGroveMarker(player, MOD_GROVE_REGROWTHS_ROOT))
                    GrowStack(player, target);
                break;
            case SPELL_LIFEBLOOM_BLOOM:
                if (HasGroveMarker(player, MOD_GROVE_LIFEBOUND))
                    GrowStack(player, target);
                break;
            case SPELL_TRANQUILITY_HEAL:
                if (has && HasGroveMarker(player, MOD_GROVE_TRANQUIL))
                    if (uint32& at = StateOf(player).TranquilAt[target->GetGUID()]; Ready(at, TRANQUIL_GROVE_MS))
                    {
                        at = Now();
                        GrowStack(player, target);
                    }
                break;
            case SPELL_HEALING_TOUCH:
                if (has && HasGroveMarker(player, MOD_GROVE_ROOTED_TOUCH))
                    for (uint32 extra = 0; extra < GROVE_STACK; ++extra)
                        if (target->HasAura(SPELL_SECOND_REJUVENATION + extra, player->GetGUID()))
                            GrowExtra(player, target, SPELL_SECOND_REJUVENATION + extra);
                break;
            case SPELL_INNERVATE:
                if (has && HasGroveMarker(player, MOD_GROVE_INNERVATING))
                    GrowFull(player, target);
                break;
            default:
                break;
        }
    }

    // Swiftmend (20% more an extra Rejuvenation on its target; 40% Verdant Swiftmend) and Nourish (Nourishing
    // Grove: 8% more a Rejuvenation).
    float GroveHealing(Player* player, Unit* target, uint32 firstRank, float heal)
    {
        if (firstRank == SPELL_SWIFTMEND)
            return heal * (1.0f + float(GroveExtras(player, target))
                * (HasGroveMarker(player, MOD_GROVE_VERDANT_SWIFTMEND) ? VERDANT_SWIFTMEND : GROVE_SWIFTMEND));
        if (firstRank == SPELL_NOURISH && HasGroveMarker(player, MOD_GROVE_NOURISHING))
            return heal * (1.0f + float(GroveStacks(player, target)) * NOURISHING_GROVE);
        return heal;
    }

    // Overgrowth: Swiftmend on a full stack, 30% of its heal to up to 3 injured allies within 10 yards.
    void GroveOvergrowth(Player* player, Unit* target, float heal)
    {
        if (!HasGroveMarker(player, MOD_GROVE_OVERGROWTH) || GroveExtras(player, target) + 1 < GroveMost(player))
            return;
        std::vector<Unit*> allies = InjuredAllies(player, target, OVERGROWTH_RANGE, target);
        if (allies.size() > OVERGROWTH_ALLIES)
            allies.resize(OVERGROWTH_ALLIES);
        for (Unit* ally : allies)
            Mend(player, ally, SPELL_OVERGROWTH, heal * OVERGROWTH_SHARE);
    }

    // ---- Iron Reckoning ----------------------------------------------------------------------------------------

    bool HasIronMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_IRON_MODIFIER_FIRST, modifier);
    }

    bool InBulwark(Player* player)
    {
        uint32 until = StateOf(player).IronUntil;
        return until && Now() < until;
    }

    // Shield Block: the Iron Bulwark (14 sec, 15% of maximum health; Fortified 20% more, Last Bastion 5% more under
    // Last Stand), its window begun again.
    void RaiseBulwark(Player* player)
    {
        player->RemoveAurasDueToSpell(SPELL_IRON_BULWARK, player->GetGUID());   // the last one ends first
        State& state = StateOf(player);
        state.IronUntil = Now() + IRON_BULWARK_MS;
        state.IronAbsorbed = 0.0f;
        state.IronSlamHit = 0.0f;
        state.IronSlamTarget.Clear();
        state.IronReleased = state.IronShockwave = state.IronRevenge = state.IronCircle = false;
        state.IronDevastate = state.IronConcussive = false;
        float pct = IRON_BULWARK_PCT;
        if (HasIronMarker(player, MOD_IRON_LAST_BASTION)
            && (player->HasAura(SPELL_LAST_STAND) || player->HasAura(SPELL_LAST_STAND_AURA)))
            pct += LAST_BASTION_PCT;
        float amount = float(player->GetMaxHealth()) * pct / 100.0f;
        if (HasIronMarker(player, MOD_IRON_FORTIFIED_BULWARK))
            amount *= FORTIFIED_BULWARK;
        Ward(player, player, SPELL_IRON_BULWARK, amount);
        if (Aura* bulwark = player->GetAura(SPELL_IRON_BULWARK, player->GetGUID()))
        {
            bulwark->SetMaxDuration(int32(IRON_BULWARK_MS));    // the spell's own is 4 sec
            bulwark->SetDuration(int32(IRON_BULWARK_MS));
        }
    }

    // The Iron impact: 20% of Shield Slam's hit (slamHit) plus half of what the bulwark absorbed (at most half their
    // attack power), to every enemy around the Hero (8 yards; 10 after Thunder Clap, 12 with Shaking Ground).
    void IronImpact(Player* player, Unit* primary, float slamHit, float more)
    {
        State& state = StateOf(player);
        float absorbed = std::min(state.IronAbsorbed * IRON_ABSORBED_SHARE,
            player->GetTotalAttackPowerValue(BASE_ATTACK) * IRON_AP_CAP)
            * SchoolBonus(player, SPELL_SCHOOL_MASK_NORMAL);
        float damage = (slamHit * IRON_SLAM_SHARE + absorbed) * more;
        if (HasIronMarker(player, MOD_IRON_CRUSHING_REPRISAL))
            damage *= CRUSHING_REPRISAL;
        if (state.IronRevenge)
            damage *= HasIronMarker(player, MOD_IRON_RETALIATION) ? RETALIATION : IRON_REVENGE;
        if (state.IronDevastate)
            damage *= HasIronMarker(player, MOD_IRON_SUNDERING_FORCE) ? SUNDERING_FORCE : IRON_DEVASTATE;
        if (damage < 1.0f)
            return;
        float range = !state.IronCircle ? IRON_RANGE
            : HasIronMarker(player, MOD_IRON_SHAKING_GROUND) ? SHAKING_GROUND_RANGE : IRON_CLAP_RANGE;
        bool wide = HasIronMarker(player, MOD_IRON_WIDE_RECKONING);
        std::vector<Unit*> enemies;
        if (primary && primary->IsAlive() && player->IsValidAttackTarget(primary))
            enemies.push_back(primary);
        for (Unit* enemy : EnemiesAround(player, player, range))
            if (enemy != primary)
                enemies.push_back(enemy);
        float ap = player->GetTotalAttackPowerValue(BASE_ATTACK);
        for (Unit* enemy : enemies)
        {
            float hit = damage;
            if (wide && enemy != enemies.front())
                hit *= WIDE_RECKONING;
            if (enemy == enemies.front() && state.IronConcussive)
                hit += ap * CONCUSSIVE_AP * SchoolBonus(player, SPELL_SCHOOL_MASK_NORMAL);
            Strike(player, enemy, SPELL_IRON_IMPACT, hit);
            if (HasIronMarker(player, MOD_IRON_RENDING_IMPACT) && enemy->GetAuraOfRankedSpell(SPELL_REND,
                player->GetGUID()))
                AddDot(player, enemy, SPELL_RENDING_IMPACT, hit * RENDING_IMPACT_SHARE, RENDING_IMPACT_MS);
            if (HasIronMarker(player, MOD_IRON_COMMANDING_FORCE))
                enemy->AddThreat(player, hit * IRON_COMMANDING_THREAT, SPELL_SCHOOL_MASK_NORMAL);
        }
        if (!enemies.empty() && HasIronMarker(player, MOD_IRON_WALL_OF_IRON) && Ready(state.IronMendAt,
            WALL_OF_IRON_EVERY_MS))
        {
            state.IronMendAt = Now();
            float pct = player->HasAura(SPELL_SHIELD_WALL) ? WALL_OF_IRON_WALL : WALL_OF_IRON_PCT;
            Mend(player, player, SPELL_IRON_MENDING, float(player->GetMaxHealth()) * pct / 100.0f);
        }
    }

    // The Hero's warrior abilities with Iron Reckoning (direct, not triggered).
    void IronAbility(Player* player, Unit* target, uint32 firstRank, float hit)
    {
        State& state = StateOf(player);
        if (firstRank == SPELL_SHIELD_BLOCK)
        {
            RaiseBulwark(player);
            return;
        }
        // A slam that kills its target still releases (the target is dead by now: not a valid attack target).
        if (target == player || (!player->IsValidAttackTarget(target) && (target->IsAlive()
            || !player->IsHostileTo(target))))
            return;
        if (firstRank == SPELL_SHIELD_SLAM && HasIronMarker(player, MOD_IRON_SHIELDED_ADVANCE))
        {
            player->CastSpell(player, SPELL_SHIELDED_ADVANCE, true);
            Ward(player, player, SPELL_SHIELDED_ADVANCE_WARD,
                float(player->GetMaxHealth()) * SHIELDED_ADVANCE_PCT / 100.0f);
        }
        if (!InBulwark(player))
            return;
        switch (firstRank)
        {
            case SPELL_SHIELD_SLAM:
                if (HasIronMarker(player, MOD_IRON_IMMOVABLE))
                {
                    state.IronSlamHit = hit;
                    state.IronSlamTarget = target->GetGUID();
                }
                else if (!state.IronReleased)
                {
                    state.IronReleased = true;
                    IronImpact(player, target, hit, 1.0f);
                }
                break;
            case SPELL_SHOCKWAVE:
                if (HasIronMarker(player, MOD_IRON_SHOCKWAVE) && !state.IronShockwave)
                {
                    state.IronShockwave = true;
                    IronImpact(player, target, state.IronSlamHit, SHOCKWAVE_RECKONING);
                }
                break;
            case SPELL_REVENGE:
                state.IronRevenge = true;
                break;
            case SPELL_THUNDER_CLAP:
                state.IronCircle = true;
                break;
            case SPELL_DEVASTATE:
                state.IronDevastate = true;
                if (HasIronMarker(player, MOD_IRON_VENGEFUL_DEVASTATION))
                    state.IronVengeful = true;
                break;
            case SPELL_CONCUSSION_BLOW:
                if (HasIronMarker(player, MOD_IRON_CONCUSSIVE))
                    state.IronConcussive = true;
                break;
            default:
                break;
        }
    }

    // Vengeful Devastation: the Revenge after Devastate in the bulwark, 20% more.
    float IronDamage(Player* player, uint32 firstRank, float damage)
    {
        State& state = StateOf(player);
        if (firstRank == SPELL_REVENGE && state.IronVengeful)
        {
            state.IronVengeful = false;
            return damage * VENGEFUL_REVENGE;
        }
        return damage;
    }

    // The bulwark gone: Immovable's impact (125%) at the Shield Slam target held, else the nearest before them.
    void BulwarkEnded(Player* player)
    {
        State& state = StateOf(player);
        if (!HasIronMarker(player, MOD_IRON_IMMOVABLE) || state.IronReleased || !state.IronUntil)
            return;
        state.IronReleased = true;
        Unit* primary = state.IronSlamTarget ? ObjectAccessor::GetUnit(*player, state.IronSlamTarget) : nullptr;
        IronImpact(player, primary, state.IronSlamHit, IMMOVABLE_MORE);
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
        state.Frostbound.clear();
        state.PatchUntil = 0;
        player->RemoveDynObject(SPELL_HOLLOW_PATCH);
        for (uint32 spell : { SPELL_PHANTOM_OPENING, SPELL_FATAL_OPENING, SPELL_BLEEDING_FOLLOW })
            player->RemoveAurasDueToSpell(spell);
        for (State::Sunshard const& shard : state.Shards)
            if (Unit* host = ObjectAccessor::GetUnit(*player, shard.Host))
                host->RemoveAurasDueToSpell(SPELL_SUNSHARD, player->GetGUID());
        state.Shards.clear();
        state.CoreLanced.clear();
        CloseRift(player);
        state.NorthFieldUntil = 0;
        player->RemoveDynObject(SPELL_NORTH_FIELD);
        player->RemoveDynObject(SPELL_SOLAR_ANCHOR);
        state.ScaldUntil = 0;
        player->RemoveDynObject(SPELL_SCALDING_GROUND);
        DismissBeast(player);
        ClearNewest(player);
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
    }

    // ---- The Spectral Blade ------------------------------------------------------------------------------------

    bool HasBladeMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_BLADE_MODIFIER_FIRST, modifier);
    }

    // The phantom's blow on one enemy: a strike, or (Open Wound) a bleed of it over 4 sec, refreshed.
    void PhantomBlow(Player* player, Unit* enemy, float amount)
    {
        int32 points = std::max<int32>(1, int32(amount));
        if (HasBladeMarker(player, MOD_OPEN_WOUND))
        {
            int32 tick = std::max<int32>(1, points / OPEN_WOUND_TICKS);
            Lasting(player, enemy, SPELL_OPEN_WOUND, OPEN_WOUND_MS, &tick);
        }
        else
            player->CastCustomSpell(enemy, SPELL_PHANTOM_STRIKE, &points, nullptr, nullptr, true);
    }

    // After the phantom's hit: the next Sinister Strike 10 Energy cheaper (Fatal Opening: the next builder too);
    // on the Hero's Shadow Word: Pain target, Phantom Wound; Frost Phantom, Flame Phantom, Marked Execution,
    // Corrupted Blade, Restoring Phantom, Bleeding Follow-Through on theirs.
    void PhantomAftermath(Player* player, Unit* victim, float amount)
    {
        ObjectGuid me = player->GetGUID();
        if (Knows(player, SPELL_SINISTER_STRIKE))
            Renew(player, SPELL_PHANTOM_OPENING);
        if (HasBladeMarker(player, MOD_FATAL_OPENING))
            Renew(player, SPELL_FATAL_OPENING);
        if (victim->IsAlive() && victim->GetAuraOfRankedSpell(SPELL_SHADOW_WORD_PAIN, me))
            AddDot(player, victim, SPELL_PHANTOM_WOUND, amount * PHANTOM_WOUND_SHARE, PHANTOM_DOT_MS);
        if (HasBladeMarker(player, MOD_FROST_PHANTOM) && victim->HasAura(SPELL_FROST_FEVER, me))
            Strike(player, victim, SPELL_FROST_PHANTOM, player->GetTotalAttackPowerValue(BASE_ATTACK) * FROST_PHANTOM
                * player->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE,
                    SPELL_SCHOOL_MASK_FROST));
        if (HasBladeMarker(player, MOD_FLAME_PHANTOM) && victim->GetAuraOfRankedSpell(SPELL_FLAME_SHOCK, me))
            Strike(player, victim, SPELL_FLAME_PHANTOM,
                RunePower(player, SPELL_SCHOOL_MASK_FIRE, SPELL_EVISCERATE) * FLAME_PHANTOM);
        if (HasBladeMarker(player, MOD_MARKED_EXECUTION) && victim->GetAuraOfRankedSpell(SPELL_HUNTERS_MARK, me))
            for (Unit* enemy : EnemiesAround(player, victim, MARKED_EXECUTION_RANGE))
                if (enemy != victim)
                {
                    PhantomBlow(player, enemy, amount * MARKED_EXECUTION);
                    break;
                }
        if (HasBladeMarker(player, MOD_CORRUPTED_BLADE) && victim->IsAlive()
            && victim->GetAuraOfRankedSpell(SPELL_CORRUPTION, me))
            AddDot(player, victim, SPELL_CORRUPTED_BLADE, amount * CORRUPTED_SHARE, PHANTOM_DOT_MS);
        State& state = StateOf(player);
        if (HasBladeMarker(player, MOD_RESTORING_PHANTOM) && Ready(state.RestoringAt, RESTORING_EVERY_MS)
            && player->GetAuraOfRankedSpell(SPELL_RENEW, me))
        {
            state.RestoringAt = Now();
            int32 heal = std::max<int32>(1, int32(RunePower(player, SPELL_SCHOOL_MASK_HOLY, SPELL_EVISCERATE, false)
                * RESTORING_POWER));
            Report(player, SPELL_RESTORING_PHANTOM, 'H', heal);
            player->CastCustomSpell(player, SPELL_RESTORING_PHANTOM, &heal, nullptr, nullptr, true);
        }
        if (HasBladeMarker(player, MOD_BLEEDING_FOLLOW) && victim->GetAuraOfRankedSpell(SPELL_REND, me))
            Renew(player, SPELL_BLEEDING_FOLLOW);
    }

    // The phantom arriving: on its victim (Death Follows: the nearest enemy within 10 yards if it died first, once),
    // all of it at once (Executioner 40% more), in 3 quick cuts (Thousand Cuts, 15% more; one bleed with Open Wound)
    // or with up to 3 more enemies within 8 yards at half (Shadow Cleave). Then the aftermath.
    void Phantom(Player* player, ObjectGuid victimGuid, Position at, float amount)
    {
        Unit* victim = ObjectAccessor::GetUnit(*player, victimGuid);
        if (!Valid(player, victim))
        {
            victim = nullptr;
            if (HasBladeMarker(player, MOD_DEATH_FOLLOWS))
            {
                float best = FOLLOW_RANGE;
                for (Unit* enemy : EnemiesAround(player, player, FOLLOW_RANGE * 3.0f))
                    if (float dist = enemy->GetExactDist(at); dist <= best)
                    {
                        best = dist;
                        victim = enemy;
                    }
            }
            if (!victim)
                return;
        }
        // The Backstab mark, spent: 25% more (Marked for Murder: 40%).
        if (victim->HasAura(SPELL_MARKED_FOR_MURDER, player->GetGUID()))
        {
            victim->RemoveAurasDueToSpell(SPELL_MARKED_FOR_MURDER, player->GetGUID());
            amount *= HasBladeMarker(player, MOD_MARKED_FOR_MURDER) ? MARKED_FOR_MURDER : BACKSTAB_MARK;
        }
        if (HasBladeMarker(player, MOD_EXECUTIONER))
            PhantomBlow(player, victim, amount * EXECUTIONER);
        else if (HasBladeMarker(player, MOD_THOUSAND_CUTS) && !HasBladeMarker(player, MOD_OPEN_WOUND))
        {
            float cut = amount * THOUSAND_CUTS / CUTS;
            ObjectGuid target = victim->GetGUID();
            for (uint32 i = 0; i < CUTS; ++i)
                player->m_Events.AddEventAtOffset([player, target, cut]()
                {
                    Unit* unit = ObjectAccessor::GetUnit(*player, target);
                    if (player->IsInWorld() && Valid(player, unit))
                        PhantomBlow(player, unit, cut);
                }, Milliseconds(i * CUT_SPACING_MS));
        }
        else if (HasBladeMarker(player, MOD_THOUSAND_CUTS))
            PhantomBlow(player, victim, amount * THOUSAND_CUTS);
        else
            PhantomBlow(player, victim, amount);
        if (HasBladeMarker(player, MOD_SHADOW_CLEAVE))
        {
            std::size_t count = 0;
            for (Unit* enemy : EnemiesAround(player, victim, CLEAVE_RADIUS))
                if (enemy != victim && count++ < CLEAVE_TARGETS)
                    PhantomBlow(player, enemy, amount * CLEAVE_SHARE);
        }
        Lasting(player, player, SPELL_PHANTOM_RUSH, PHANTOM_RUSH_MS);
        if (HasBladeMarker(player, MOD_VANISHING_ACT))
            Lasting(player, player, SPELL_VANISHING_ACT, VANISHING_ACT_MS);
        PhantomAftermath(player, victim, amount);
    }

    // Eviscerate's hit: the phantom, twice it, 0.6 sec later (0.1 with Quick Death; 2 sec and 40% more with
    // Patient Killer; 20% more as Open Wound's bleed).
    void QueuePhantom(Player* player, Unit* victim, float damage)
    {
        if (damage <= 0.0f)
            return;
        float amount = damage * PHANTOM_SHARE;
        uint32 delay = PHANTOM_DELAY_MS;
        if (HasBladeMarker(player, MOD_FATAL_OPENING))
            delay = QUICK_DEATH_MS;
        else if (HasBladeMarker(player, MOD_PERFECT_AMBUSH))
        {
            delay = PATIENT_KILLER_MS;
            amount *= PATIENT_KILLER;
        }
        else if (HasBladeMarker(player, MOD_OPEN_WOUND))
            amount *= OPEN_WOUND;
        ObjectGuid target = victim->GetGUID();
        Position at = victim->GetPosition();
        // The phantom itself: the Hero's shadowy double, out of a puff behind the victim (away from the Hero), its
        // blow when the strike lands, gone again a moment after (npc_titan_rune_phantom_assassin).
        ObjectGuid ghostGuid;
        float angle = player->GetAngle(victim);
        Position behind(victim->GetPositionX() + PHANTOM_BEHIND * std::cos(angle),
            victim->GetPositionY() + PHANTOM_BEHIND * std::sin(angle), victim->GetPositionZ(), angle + float(M_PI));
        if (TempSummon* ghost = player->GetMap()->SummonCreature(NPC_PHANTOM_ASSASSIN, behind,
            sSummonPropertiesStore.LookupEntry(SUMMON_PROPERTIES_GUARDIAN), delay + PHANTOM_LINGER_MS, player))
        {
            ghost->SetFacingToObject(victim);
            ghostGuid = ghost->GetGUID();
        }
        player->m_Events.AddEventAtOffset([player, target, at, amount, ghostGuid]()
        {
            if (!player->IsInWorld() || !player->IsAlive() || !player->HasAura(SPELL_RUNE_SPECTRAL_BLADE))
                return;
            if (Creature* ghost = ghostGuid ? ObjectAccessor::GetCreature(*player, ghostGuid) : nullptr)
            {
                if (Unit* victim = ObjectAccessor::GetUnit(*player, target))
                    ghost->SetFacingToObject(victim);
                ghost->HandleEmoteCommand(EMOTE_ONESHOT_ATTACK1H);
                ghost->m_Events.AddEventAtOffset([ghost]() { ghost->CastSpell(ghost, SPELL_PHANTOM_VANISH, true); },
                    Milliseconds(PHANTOM_LINGER_MS - 200));
            }
            Phantom(player, target, at, amount);
        }, Milliseconds(delay));
    }

    // ---- The Wild Hunt -----------------------------------------------------------------------------------------

    bool HasHuntMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_HUNT_MODIFIER_FIRST, modifier);
    }

    std::vector<Creature*> LiveBeasts(Player* player)
    {
        State& state = StateOf(player);
        std::vector<Creature*> live;
        std::vector<ObjectGuid> kept;
        for (ObjectGuid guid : state.SpectralBeasts)
            if (Creature* beast = ObjectAccessor::GetCreature(*player, guid); beast && beast->IsAlive())
            {
                live.push_back(beast);
                kept.push_back(guid);
            }
        state.SpectralBeasts = kept;
        return live;
    }

    void DismissBeast(Player* player)
    {
        for (Creature* beast : LiveBeasts(player))
            if (TempSummon* summon = beast->ToTempSummon())
                summon->UnSummon();
        StateOf(player).SpectralBeasts.clear();
    }

    // The Hero's pet in Bestial Wrath (theirs).
    bool InWrath(Player* player)
    {
        Pet* pet = player->GetPet();
        return pet && pet->IsAlive() && pet->HasAura(SPELL_BESTIAL_WRATH, player->GetGUID());
    }

    // A Spectral Beast for ms on the pet's enemy (else the Hero's target), at its side; 8 at most (the oldest simply
    // goes, no farewell). Needs a living pet and an enemy.
    void Unleash(Player* player, uint32 ms)
    {
        Pet* pet = player->GetPet();
        if (!pet || !pet->IsAlive())
            return;
        Unit* target = pet->GetVictim();
        if (!Valid(player, target))
            target = player->GetSelectedUnit();
        if (!Valid(player, target) || !player->IsWithinDistInMap(target, BEAST_RANGE))
            return;
        std::vector<Creature*> beasts = LiveBeasts(player);
        State& state = StateOf(player);
        if (beasts.size() >= MAX_BEASTS)
            if (TempSummon* oldest = beasts.front()->ToTempSummon())
            {
                oldest->UnSummon();
                state.SpectralBeasts.erase(state.SpectralBeasts.begin());
            }
        Position pos = target->GetNearPosition(1.5f, frand(0.0f, 2.0f * float(M_PI)));
        TempSummon* beast = player->GetMap()->SummonCreature(NPC_SPECTRAL_BEAST, pos,
            sSummonPropertiesStore.LookupEntry(SUMMON_PROPERTIES_GUARDIAN), ms, player);
        if (!beast)
            return;
        state.SpectralBeasts.push_back(beast->GetGUID());
        if (beast->IsAIEnabled)
        {
            beast->AI()->SetGUID(target->GetGUID(), 0);
            beast->AI()->SetData(0, ms);
        }
    }

    // Kill Command: a beast for 10 sec. In Bestial Wrath it comes back in 5 sec.
    void KillCommand(Player* player)
    {
        Unleash(player, BEAST_MS);
        if (!InWrath(player))
            return;
        if (player->GetSpellCooldownDelay(SPELL_KILL_COMMAND) > WRATH_KILL_COMMAND_MS)
            player->ModifySpellCooldown(SPELL_KILL_COMMAND,
                int32(WRATH_KILL_COMMAND_MS) - int32(player->GetSpellCooldownDelay(SPELL_KILL_COMMAND)));
    }

    // Arcane Shot on a beast's target: its next bite there 20% more (Arcane Hunt 40%; one charge, 6 sec).
    void HuntArcane(Player* player, Unit* target)
    {
        for (Creature* beast : LiveBeasts(player))
            if (beast->GetTarget() == target->GetGUID())
            {
                State& state = StateOf(player);
                state.HuntChargePrey = target->GetGUID();
                state.HuntChargeUntil = Now() + HUNT_CHARGE_MS;
                return;
            }
    }

    // Storm Hunt: Lightning Bolt sends the beasts to its target, their next bite 20% of spell power as Nature (6 sec
    // apart).
    void HuntStorm(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        if (!HasHuntMarker(player, MOD_STORM_HUNT) || !Ready(state.StormHuntAt, STORM_HUNT_EVERY_MS))
            return;
        std::vector<Creature*> beasts = LiveBeasts(player);
        if (beasts.empty())
            return;
        state.StormHuntAt = Now();
        state.StormChargeUntil = Now() + STORM_HUNT_EVERY_MS;
        for (Creature* beast : beasts)
            if (beast->IsAIEnabled)
                beast->AI()->SetGUID(target->GetGUID(), 0);
    }

    // The bite's share more for Arcane Shot's charge on the prey (spent).
    float HuntCharge(Player* player, Unit* prey)
    {
        State& state = StateOf(player);
        if (state.HuntChargePrey != prey->GetGUID() || Now() >= state.HuntChargeUntil)
            return 1.0f;
        state.HuntChargePrey.Clear();
        return HasHuntMarker(player, MOD_ARCANE_HUNT) ? ARCANE_HUNT : ARCANE_CHARGE;
    }

    // Storm Hunt's Nature for the next bite (spent), else 0.
    float HuntStormCharge(Player* player)
    {
        State& state = StateOf(player);
        if (!state.StormChargeUntil || Now() >= state.StormChargeUntil)
            return 0.0f;
        state.StormChargeUntil = 0;
        return RunePower(player, SPELL_SCHOOL_MASK_NATURE, 0) * STORM_HUNT_SP;
    }

    // Bestial Wrath: Kill Command at most 5 sec away; then a beast a second while it lasts (WrathTick).
    void BestialWrath(Player* player)
    {
        State& state = StateOf(player);
        state.WrathTimer = WRATH_CALL_MS;
        if (player->GetSpellCooldownDelay(SPELL_KILL_COMMAND) > WRATH_KILL_COMMAND_MS)
            player->ModifySpellCooldown(SPELL_KILL_COMMAND,
                int32(WRATH_KILL_COMMAND_MS) - int32(player->GetSpellCooldownDelay(SPELL_KILL_COMMAND)));
    }

    void WrathTick(Player* player, uint32 diff)
    {
        if (!InWrath(player))
            return;
        State& state = StateOf(player);
        state.WrathTimer += diff;
        if (state.WrathTimer < WRATH_CALL_MS)
            return;
        state.WrathTimer = 0;
        Unleash(player, WRATH_BEAST_MS);
    }

    // ---- Shared by the five newest ----------------------------------------------------------------------------

    // .runedebug: a step of the newest runes, told to the Hero in chat.
    void Trace(Player* player, std::string const& text)
    {
        if (StateOf(player).Debug)
            ChatHandler(player->GetSession()).SendSysMessage("|cff88ccff[rune]|r " + text);
    }

    // The highest rank of the spell the Hero knows (0 none).
    uint32 KnownRank(Player* player, uint32 firstRank)
    {
        uint32 known = 0;
        for (uint32 id = firstRank; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (player->HasSpell(id))
                known = id;
        return known;
    }

    // A spell's direct damage on the target as the Hero would deal it (their power and bonuses; no crit).
    float DirectDamage(Player* player, Unit* target, uint32 spellId)
    {
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        if (!info)
            return 0.0f;
        int32 base = std::max(0, info->Effects[EFFECT_0].CalcValue(player));
        return float(player->SpellDamageBonusDone(target, info, uint32(base), SPELL_DIRECT_DAMAGE, EFFECT_0));
    }

    // The Hero's spell power of a school for a rune's own hits, never less than the average damage of their known
    // rank of floorSpell (so a Hero with little gear still deals something); withBonuses, their damage done bonuses
    // of the school too (the rune's strikes ignore their modifiers).
    float RunePower(Player* player, SpellSchoolMask school, uint32 floorSpell, bool withBonuses)
    {
        float floor = 0.0f;
        if (SpellInfo const* rank = sSpellMgr->GetSpellInfo(KnownRank(player, floorSpell)))
            floor = float(rank->Effects[EFFECT_0].BasePoints) + float(rank->Effects[EFFECT_0].DieSides + 1) / 2.0f;
        float power = std::max(float(player->SpellBaseDamageBonusDone(school)), floor);
        return withBonuses
            ? power * player->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, school) : power;
    }

    // The Hero's damage done bonuses of a school (the rune's strikes ignore their modifiers).
    float SchoolBonus(Player* player, SpellSchoolMask school)
    {
        return player->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, school);
    }

    // An aura ms longer, at most `most` over its application's own duration (a new cast starts again).
    void ExtendAura(Aura* aura, int32 ms, int32 most)
    {
        int32 more = std::min(ms, most - (aura->GetMaxDuration() - aura->CalcMaxDuration()));
        if (more <= 0)
            return;
        aura->SetMaxDuration(aura->GetMaxDuration() + more);
        aura->SetDuration(aura->GetDuration() + more);
    }

    // An absorb of the Hero's on the unit (an earlier one replaced).
    void Ward(Player* player, Unit* unit, uint32 spellId, float amount)
    {
        int32 points = std::max<int32>(1, int32(amount));
        unit->RemoveAurasDueToSpell(spellId, player->GetGUID());
        Report(player, spellId, 'A', points);
        player->CastCustomSpell(unit, spellId, &points, nullptr, nullptr, true);
    }

    // A fresh buff on the Hero (an earlier one replaced).
    void Renew(Player* player, uint32 spellId)
    {
        player->RemoveAurasDueToSpell(spellId);
        player->AddAura(spellId, player);
    }

    // Since a moment (game ms; 0 never): at least ms ago.
    bool Ready(uint32 at, uint32 ms)
    {
        return !at || Now() - at >= ms;
    }

    // How often a rune damage over time ticks (its aura's period: a second unless it says otherwise).
    int32 DotPeriod(uint32 spellId)
    {
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        int32 period = info ? info->Effects[EFFECT_0].Amplitude : 0;
        return period > 0 ? period : 1000;
    }

    // What a rune damage over time of the Hero's still holds on the target.
    float DotLeft(Player* player, Unit* target, uint32 spellId)
    {
        Aura* aura = target->GetAura(spellId, player->GetGUID());
        auto tick = dotTicks.find({ target->GetGUID(), spellId, player->GetGUID() });
        return aura && tick != dotTicks.end()
            ? float(tick->second) * float(aura->GetDuration() / DotPeriod(spellId)) : 0.0f;
    }

    // A damage over time of fresh over ms (a tick each period): what is left of one already there is kept, up to
    // twice a fresh one; its tick timer runs on.
    void AddDot(Player* player, Unit* target, uint32 spellId, float fresh, int32 ms)
    {
        float total = std::min(DotLeft(player, target, spellId) + fresh, 2.0f * fresh);
        int32 tick = std::max<int32>(1, int32(total / float(std::max(1, ms / DotPeriod(spellId)))));
        Lasting(player, target, spellId, ms, &tick);
    }

    void Strike(Player* player, Unit* target, uint32 spellId, float amount)
    {
        int32 points = std::max<int32>(1, int32(amount));
        player->CastCustomSpell(target, spellId, &points, nullptr, nullptr, true);
    }

    // A summon of the rune's that only flies where it is told: the Hero's side, passive, out of reach.
    TempSummon* SummonFlier(Player* player, uint32 entry, Position const& at, uint32 ms, uint32 form)
    {
        TempSummon* flier = player->GetMap()->SummonCreature(entry, at, nullptr, ms, player);
        if (!flier)
            return nullptr;
        flier->SetFaction(player->GetFaction());
        flier->SetReactState(REACT_PASSIVE);
        flier->SetCanFly(true);
        flier->SetDisableGravity(true);
        flier->AddAura(form, flier);
        return flier;
    }

    void Dismiss(Player* player, ObjectGuid guid)
    {
        if (Creature* creature = guid ? ObjectAccessor::GetCreature(*player, guid) : nullptr)
            if (TempSummon* summon = creature->ToTempSummon())
                summon->UnSummon();
    }

    // ---- Arcane Mind -------------------------------------------------------------------------------------------

    bool HasMindMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_MIND_MODIFIER_FIRST, modifier);
    }

    uint8 Insight(Player* player)
    {
        Aura* aura = player->GetAura(SPELL_ARCANE_INSIGHT);
        return aura ? aura->GetStackAmount() : 0;
    }

    // Insight gained: up to 4 (6 with Patient Calculation), 15 sec, renewed; at 4 or more Mind Unbound shows it.
    void GainInsight(Player* player, uint8 count)
    {
        uint8 most = HasMindMarker(player, MOD_PATIENT_CALCULATION) ? PATIENT_INSIGHT : MAX_INSIGHT;
        uint8 have = Insight(player);
        Aura* aura = player->GetAura(SPELL_ARCANE_INSIGHT);
        if (!aura)
            aura = player->AddAura(SPELL_ARCANE_INSIGHT, player);
        if (!aura)
            return;
        aura->SetStackAmount(std::min<uint8>(most, have + count));
        aura->RefreshDuration();
        if (aura->GetStackAmount() < INSIGHT_THRESHOLD)
            return;
        Aura* unbound = player->GetAura(SPELL_MIND_UNBOUND);
        if (!unbound)
            unbound = player->AddAura(SPELL_MIND_UNBOUND, player);
        if (unbound)
            unbound->RefreshDuration();
    }

    // A direct Arcane Blast hit: 1 Insight (Accelerated Thought: every third, 2).
    void BlastInsight(Player* player)
    {
        uint8 count = 1;
        State& state = StateOf(player);
        if (HasMindMarker(player, MOD_ACCELERATED_THOUGHT) && ++state.BlastCount >= ACCELERATED_EVERY)
        {
            state.BlastCount = 0;
            ++count;
        }
        GainInsight(player, count);
    }

    // Arcane Missiles' own missile and how many it fires (a rank's).
    uint32 MissileOf(SpellInfo const* missiles)
    {
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            if (missiles->Effects[i].ApplyAuraName == SPELL_AURA_PERIODIC_TRIGGER_SPELL)
                return missiles->Effects[i].TriggerSpell;
        return 0;
    }

    uint32 MissileCount(SpellInfo const* missiles)
    {
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            if (missiles->Effects[i].ApplyAuraName == SPELL_AURA_PERIODIC_TRIGGER_SPELL
                && missiles->Effects[i].Amplitude > 0)
                return uint32(std::max<int32>(1, missiles->GetDuration() / int32(missiles->Effects[i].Amplitude)));
        return INSIGHT_MISSILES;
    }

    struct InsightVolley
    {
        ObjectGuid Primary;
        uint32 Missile = 0;
        float Bonus = 1.0f;                 // Patient Calculation, Overclocked Intellect
        bool Channelled = true;             // the missiles fly while Arcane Missiles channels
        float Echo = 0.0f;                  // Lingering Equation: the volley's damage so far
        float EchoCarry = -1.0f;            // what was left of an Arcane Echo already there (at the first missile)
        bool Shards = false;                // Missile Shards: the first missile still to split
    };

    // One Insight missile: 40% of one of Arcane Missiles' own (Focused Intention: 40% more); Fractured Thought: 2
    // more enemies within 8 yards for 35% of it; Lingering Equation: the volley's Arcane Echo grows.
    void FireInsight(Player* player, std::shared_ptr<InsightVolley> volley)
    {
        if (!player->IsInWorld() || !player->IsAlive())
            return;
        if (volley->Channelled)
        {
            Spell* channel = player->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
            if (!channel || channel->GetSpellInfo()->GetFirstRankSpell()->Id != SPELL_ARCANE_MISSILES)
                return;                     // interrupted: the rest of the volley is lost
        }
        Unit* target = ObjectAccessor::GetUnit(*player, volley->Primary);
        if (!Valid(player, target))
            return;
        float missile = DirectDamage(player, target, volley->Missile) * INSIGHT_SHARE * volley->Bonus;
        float hit = missile * (HasMindMarker(player, MOD_FOCUSED_INTENTION) ? FOCUSED_INTENTION : 1.0f);
        Strike(player, target, SPELL_INSIGHT_MISSILE, hit);
        if (volley->Shards)
        {
            volley->Shards = false;
            std::size_t count = SHARDS_TARGETS;
            for (Unit* enemy : EnemiesAround(player, target, SHARDS_RADIUS))
                if (enemy != target && count)
                {
                    Strike(player, enemy, SPELL_INSIGHT_MISSILE, hit * SHARDS_SHARE);
                    --count;
                }
        }
        if (HasMindMarker(player, MOD_FRACTURED_THOUGHT))
        {
            std::size_t count = FRACTURED_TARGETS;
            for (Unit* enemy : EnemiesAround(player, target, FRACTURED_RADIUS))
                if (enemy != target && count)
                {
                    Strike(player, enemy, SPELL_INSIGHT_MISSILE, missile * FRACTURED_SHARE);
                    --count;
                }
        }
        if (!HasMindMarker(player, MOD_LINGERING_EQUATION))
            return;
        if (volley->EchoCarry < 0.0f)
            volley->EchoCarry = DotLeft(player, target, SPELL_ARCANE_ECHO);
        volley->Echo += hit;
        float fresh = volley->Echo * ARCANE_ECHO_SHARE;
        float total = std::min(volley->EchoCarry + fresh, 2.0f * fresh);
        int32 tick = std::max<int32>(1, int32(total / float(ARCANE_ECHO_MS / 1000)));
        Lasting(player, target, SPELL_ARCANE_ECHO, ARCANE_ECHO_MS, &tick);
    }

    // Mental Reserves: 3% of maximum mana back, Arcane Blast 20% cheaper for 4 sec.
    void MindReserves(Player* player)
    {
        player->EnergizeBySpell(player, SPELL_MENTAL_RESERVES,
            uint32(player->GetMaxPower(POWER_MANA) * RESERVES_MANA_PCT / 100.0f), POWER_MANA);
        player->RemoveAurasDueToSpell(SPELL_MENTAL_RESERVES);
        player->AddAura(SPELL_MENTAL_RESERVES, player);
    }

    // An empowered Arcane Missiles completed: the next Arcane Blast instant (6 sec); Mental Reserves; Shocking
    // Clarity (in Lightning Shield) a Nature bolt at its target.
    void MindComplete(Player* player)
    {
        State& state = StateOf(player);
        state.EmpoweredAt = Now();
        Renew(player, SPELL_MIND_FLOW);
        if (HasMindMarker(player, MOD_MENTAL_RESERVES))
            MindReserves(player);
        Unit* target = ObjectAccessor::GetUnit(*player, state.MindTarget);
        if (HasMindMarker(player, MOD_SHOCKING_CLARITY) && Valid(player, target)
            && player->GetAuraOfRankedSpell(SPELL_LIGHTNING_SHIELD, player->GetGUID()))
            Strike(player, target, SPELL_SHOCKING_CLARITY,
                RunePower(player, SPELL_SCHOOL_MASK_NATURE, SPELL_ARCANE_BLAST) * CLARITY_POWER);
    }

    // Insight spent, count of it (its look gone below 4).
    void SpendInsight(Player* player, uint8 count)
    {
        Aura* aura = player->GetAura(SPELL_ARCANE_INSIGHT);
        if (!aura)
            return;
        if (aura->GetStackAmount() <= count)
            player->RemoveAurasDueToSpell(SPELL_ARCANE_INSIGHT);
        else
            aura->SetStackAmount(aura->GetStackAmount() - count);
        if (Insight(player) < INSIGHT_THRESHOLD)
            player->RemoveAurasDueToSpell(SPELL_MIND_UNBOUND);
    }

    // A direct Arcane Blast hit on an enemy: Storm Conduit's mark (15 sec).
    void MindBlasted(Player* player, Unit* enemy)
    {
        std::map<ObjectGuid, uint32>& blasted = StateOf(player).Blasted;
        for (auto it = blasted.begin(); it != blasted.end();)
            it = Now() - it->second >= BLASTED_MS ? blasted.erase(it) : std::next(it);
        blasted[enemy->GetGUID()] = Now();
    }

    // A direct Lightning Bolt hit: Insight (8 sec apart); Storm Conduit, on an enemy the Hero's Arcane Blast hit,
    // the next Insight volley 20% more.
    void MindBolt(Player* player, Unit* enemy)
    {
        State& state = StateOf(player);
        if (Ready(state.BoltInsightAt, BOLT_INSIGHT_EVERY_MS))
        {
            state.BoltInsightAt = Now();
            GainInsight(player, 1);
        }
        auto blasted = state.Blasted.find(enemy->GetGUID());
        if (HasMindMarker(player, MOD_STORM_CONDUIT) && blasted != state.Blasted.end()
            && Now() - blasted->second < BLASTED_MS)
            Renew(player, SPELL_STORM_CONDUIT_CHARGE);
    }

    // Starfall Equation: a direct Starfire hit, Insight and the next Arcane Blast 10% more (8 sec apart).
    void MindStarfire(Player* player)
    {
        State& state = StateOf(player);
        if (!HasMindMarker(player, MOD_STARFALL_EQUATION) || !Ready(state.StarfallAt, STARFALL_EVERY_MS))
            return;
        state.StarfallAt = Now();
        GainInsight(player, 1);
        Renew(player, SPELL_STARFALL_EQUATION);
    }

    // Missile Shards: a direct Frostbolt hit readies the next empowered channel's split (15 sec).
    void MindFrostbolt(Player* player)
    {
        if (HasMindMarker(player, MOD_MISSILE_SHARDS))
            StateOf(player).ShardsAt = Now();
    }

    // Arcane Charge: Arcane Explosion within 6 sec of an empowered channel bursts for 25% of spell power on up to
    // 5 enemies within 8 yards (once a channel).
    void MindExplosion(Player* player)
    {
        State& state = StateOf(player);
        if (!HasMindMarker(player, MOD_ARCANE_CHARGE) || !state.EmpoweredAt
            || Now() - state.EmpoweredAt > ARCANE_CHARGE_MS)
            return;
        state.EmpoweredAt = 0;
        float burst = RunePower(player, SPELL_SCHOOL_MASK_ARCANE, SPELL_ARCANE_BLAST) * ARCANE_CHARGE_POWER;
        for (Unit* enemy : EnemiesIn(player, Circle(player->GetPosition(), ARCANE_CHARGE_RADIUS),
            ARCANE_CHARGE_TARGETS))
            Strike(player, enemy, SPELL_ARCANE_CHARGE, burst);
    }

    // Evocative Mind: Evocation begun within 30 sec of an empowered channel, 2% of maximum mana (30 sec apart).
    void MindEvocation(Player* player)
    {
        State& state = StateOf(player);
        if (!HasMindMarker(player, MOD_EVOCATIVE_MIND) || !state.EmpoweredAt
            || Now() - state.EmpoweredAt > EVOCATIVE_AFTER_MS || !Ready(state.EvocativeAt, EVOCATIVE_EVERY_MS))
            return;
        state.EvocativeAt = Now();
        player->EnergizeBySpell(player, SPELL_EVOCATION,
            uint32(player->GetMaxPower(POWER_MANA) * EVOCATIVE_MANA_PCT / 100.0f), POWER_MANA);
    }

    // Arcane Missiles at 4 Insight or more spends it all: its Insight volley, 5 missiles, flies with the channel
    // (Overclocked Intellect: all as it starts; Unbound Mind: no channel, every missile on its own, 0.5 sec apart).
    // The mana comes back after the cast (spell_titan_rune_arcane_missiles).
    void Empower(Player* player, Unit* target, SpellInfo const* missiles)
    {
        uint8 spent = Insight(player);
        player->RemoveAurasDueToSpell(SPELL_ARCANE_INSIGHT);
        player->RemoveAurasDueToSpell(SPELL_MIND_UNBOUND);
        auto volley = std::make_shared<InsightVolley>();
        volley->Primary = target->GetGUID();
        volley->Missile = MissileOf(missiles);
        if (HasMindMarker(player, MOD_PATIENT_CALCULATION) && spent > INSIGHT_THRESHOLD)
            volley->Bonus += PATIENT_STEP * float(spent - INSIGHT_THRESHOLD);
        if (player->HasAura(SPELL_STORM_CONDUIT_CHARGE))
        {
            player->RemoveAurasDueToSpell(SPELL_STORM_CONDUIT_CHARGE);
            volley->Bonus *= STORM_CONDUIT;
        }
        State& state = StateOf(player);
        state.MindTarget = target->GetGUID();
        if (HasMindMarker(player, MOD_MISSILE_SHARDS) && state.ShardsAt && Now() - state.ShardsAt <= SHARDS_READY_MS)
        {
            state.ShardsAt = 0;
            volley->Shards = true;
        }
        bool unbound = HasMindMarker(player, MOD_UNBOUND_MIND);
        bool overclocked = HasMindMarker(player, MOD_OVERCLOCKED_INTELLECT);
        if (overclocked)
            volley->Bonus *= OVERCLOCKED;
        volley->Channelled = !unbound && !overclocked;
        StateOf(player).MindChannel = !unbound;
        uint32 duration = uint32(std::max(1000, missiles->GetDuration()));
        for (uint32 i = 0; i < INSIGHT_MISSILES; ++i)
        {
            uint32 at = unbound ? i * UNBOUND_SPACING_MS : overclocked ? i * OVERCLOCK_SPACING_MS
                : duration * (2 * i + 1) / (2 * INSIGHT_MISSILES);
            player->m_Events.AddEventAtOffset([player, volley]() { FireInsight(player, volley); }, Milliseconds(at));
        }
        if (!unbound)
            return;
        // Its own missiles (the rank's only fly at a channel's target): as much as they would deal.
        uint32 missile = volley->Missile, count = MissileCount(missiles);
        ObjectGuid victim = volley->Primary;
        for (uint32 i = 0; i < count && missile; ++i)
            player->m_Events.AddEventAtOffset([player, victim, missile]()
            {
                Unit* at = ObjectAccessor::GetUnit(*player, victim);
                if (player->IsInWorld() && player->IsAlive() && Valid(player, at))
                    Strike(player, at, SPELL_UNBOUND_MISSILE, DirectDamage(player, at, missile));
            }, Milliseconds(i * UNBOUND_SPACING_MS));
        player->m_Events.AddEventAtOffset([player]()
        {
            if (player->IsInWorld() && player->IsAlive() && player->HasAura(SPELL_RUNE_ARCANE_MIND))
                MindComplete(player);
        }, Milliseconds(std::max(count, INSIGHT_MISSILES) * UNBOUND_SPACING_MS));
    }

    // ---- The Splintered Crown ----------------------------------------------------------------------------------

    bool HasCrownMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_CROWN_MODIFIER_FIRST, modifier);
    }

    // The crown over the Hero: one stack an Icicle waiting.
    void ShowCrown(Player* player)
    {
        uint8 waiting = StateOf(player).Icicles;
        if (!waiting)
        {
            player->RemoveAurasDueToSpell(SPELL_FROST_CROWN);
            return;
        }
        Aura* aura = player->GetAura(SPELL_FROST_CROWN);
        if (!aura)
            aura = player->AddAura(SPELL_FROST_CROWN, player);
        if (aura)
            aura->SetStackAmount(waiting);
    }

    // The Hero's Frost spell power for the crown's other Icicles (never less than their Frostbolt's own damage).
    float CrownPower(Player* player)
    {
        return RunePower(player, SPELL_SCHOOL_MASK_FROST, SPELL_FROSTBOLT);
    }

    bool Frostbitten(Player* player, Unit* enemy)
    {
        return enemy->HasAura(SPELL_CROWN_FROSTBITE, player->GetGUID());
    }

    // An Icicle strikes (any of the crown's): Deep Freeze 20% more on the Hero's Frostbite (checked first); a
    // Frostbolt's on their Permafrost, 25% more (spent). Then Frostbite (6 sec); every third strike, Cold Front's
    // instant Frostbolt.
    void IcicleStrike(Player* player, Unit* victim, float amount, bool frostbolt)
    {
        State& state = StateOf(player);
        ObjectGuid me = player->GetGUID();
        if (HasCrownMarker(player, MOD_DEEP_FREEZE) && Frostbitten(player, victim))
            amount *= DEEP_FREEZE;
        if (frostbolt && victim->HasAura(SPELL_PERMAFROST, me))
        {
            victim->RemoveAurasDueToSpell(SPELL_PERMAFROST, me);
            amount *= PERMAFROST;
        }
        Strike(player, victim, SPELL_ICICLE, amount);
        if (victim->IsAlive())
            Lasting(player, victim, SPELL_CROWN_FROSTBITE, FROSTBITE_MS);
        if (HasCrownMarker(player, MOD_COLD_FRONT) && ++state.IcicleAttacks >= AFTERMATH_EVERY)
        {
            state.IcicleAttacks = 0;
            Renew(player, SPELL_COLD_FRONT);
        }
    }

    // A Frostbolt's Icicle loosed at its victim (gone or out of reach: it melts). Splintering Ice: half of it to up
    // to 2 others within 8 yards. crowned: it waited in the crown (Runic Rime's extra one did not).
    void LoosenIcicle(Player* player, ObjectGuid victimGuid, float icicle, bool crowned = true)
    {
        State& state = StateOf(player);
        if (crowned && state.Icicles)
            --state.Icicles;
        ShowCrown(player);
        Unit* victim = ObjectAccessor::GetUnit(*player, victimGuid);
        if (!player->IsAlive() || !Valid(player, victim) || !player->IsWithinDistInMap(victim, ICICLE_RANGE))
        {
            Trace(player, "Icicle melted: its target is gone or out of reach");
            return;
        }
        Trace(player, Acore::StringFormat("Icicle loosed at {} for {}", victim->GetName(), icicle));
        if (HasCrownMarker(player, MOD_SPLINTERING_ICE))
        {
            std::size_t count = SPLINTERING_TARGETS;
            for (Unit* enemy : EnemiesAround(player, victim, SPLINTERING_RADIUS))
                if (enemy != victim && count)
                {
                    Strike(player, enemy, SPELL_ICE_SPLINTER, icicle * SPLINTERING_SHARE);
                    --count;
                }
        }
        IcicleStrike(player, victim, icicle, true);
    }

    // A direct Frostbolt hit forms an Icicle (5 waiting at most): 30% of the Frostbolt's damage (Glacial Spear 40%
    // more), loosed 1 sec later at the same victim. Runic Rime: after the Hero's Icy Touch on it (6 sec), one more
    // at half, beside the crown.
    void FormIcicle(Player* player, Unit* victim, float frostbolt)
    {
        State& state = StateOf(player);
        if (frostbolt <= 0.0f)
            return;
        float icicle = frostbolt * ICICLE_SHARE * (HasCrownMarker(player, MOD_GLACIAL_SPEAR) ? GLACIAL_SPEAR : 1.0f);
        ObjectGuid guid = victim->GetGUID();
        auto rime = state.RimeOn.find(guid);
        if (rime != state.RimeOn.end())
        {
            if (Now() - rime->second <= RIME_MS)
                player->m_Events.AddEventAtOffset([player, guid, icicle]()
                {
                    if (player->IsInWorld())
                        LoosenIcicle(player, guid, icicle * RIME_SHARE, false);
                }, Milliseconds(ICICLE_DELAY_MS + 200));
            state.RimeOn.erase(rime);
        }
        if (state.Icicles >= MAX_ICICLES)
        {
            Trace(player, Acore::StringFormat("Frostbolt hit for {}: no Icicle ({} waiting)", frostbolt,
                state.Icicles));
            return;
        }
        ++state.Icicles;
        ShowCrown(player);
        player->m_Events.AddEventAtOffset([player, guid, icicle]()
        {
            if (player->IsInWorld())
                LoosenIcicle(player, guid, icicle);
        }, Milliseconds(ICICLE_DELAY_MS));
    }

    // Twin Icicles: a direct Ice Lance hit looses 2 at its target, 20% of spell power each (Twin Fangs 25%). The
    // second: Twin Fangs' wound (10% over 4 sec), Glacial Wound (20% over 6 sec), Cold Snap (the next Frostbolt 15%
    // faster).
    void CrownIceLance(Player* player, Unit* victim)
    {
        float icicle = CrownPower(player) * (HasCrownMarker(player, MOD_TWIN_FANGS) ? TWIN_FANGS_POWER : TWIN_POWER);
        IcicleStrike(player, victim, icicle, false);
        ObjectGuid guid = victim->GetGUID();
        player->m_Events.AddEventAtOffset([player, guid, icicle]()
        {
            Unit* target = ObjectAccessor::GetUnit(*player, guid);
            if (!player->IsInWorld() || !player->IsAlive() || !Valid(player, target))
                return;
            IcicleStrike(player, target, icicle, false);
            float power = CrownPower(player);
            if (HasCrownMarker(player, MOD_TWIN_FANGS) && target->IsAlive())
                AddDot(player, target, SPELL_FROST_WOUND, power * TWIN_WOUND_POWER, TWIN_WOUND_MS);
            if (HasCrownMarker(player, MOD_GLACIAL_WOUND) && target->IsAlive())
                AddDot(player, target, SPELL_FROST_WOUND, power * GLACIAL_WOUND_POWER, GLACIAL_WOUND_MS);
            if (HasCrownMarker(player, MOD_COLD_SNAP))
                Renew(player, SPELL_COLD_SNAP);
        }, Milliseconds(TWIN_SPACING_MS));
    }

    // Shattered Front: Cone of Cold's Icicle at an enemy it hit (5 a cast), 15% of spell power (Frozen Volley: 2 of
    // 10%); Brittle Impact 30% more on the Hero's Frostbite; Permafrost: 20% slower for 3 sec, and their next
    // Frostbolt Icicle there 25% more.
    void CrownCone(Player* player, Unit* victim)
    {
        bool volley = HasCrownMarker(player, MOD_FROZEN_VOLLEY);
        float icicle = CrownPower(player) * (volley ? VOLLEY_POWER : FRONT_POWER)
            * (HasCrownMarker(player, MOD_BRITTLE_IMPACT) && Frostbitten(player, victim) ? BRITTLE_IMPACT : 1.0f);
        IcicleStrike(player, victim, icicle, false);
        if (volley && victim->IsAlive())
            IcicleStrike(player, victim, icicle, false);
        if (HasCrownMarker(player, MOD_PERMAFROST) && victim->IsAlive())
            Lasting(player, victim, SPELL_PERMAFROST, PERMAFROST_MS);
    }

    // Frostfire Fracture: a direct Fire Blast on the Hero's Frostbite erupts in an Icicle, 25% of spell power to it
    // and up to 2 others within 6 yards (8 sec apart). Runic Rime: Icy Touch readies the extra Icicle (6 sec).
    void CrownStrike(Player* player, Unit* victim, uint32 firstRank)
    {
        State& state = StateOf(player);
        if (firstRank == SPELL_ICY_TOUCH && HasCrownMarker(player, MOD_RUNIC_RIME))
            state.RimeOn[victim->GetGUID()] = Now();
        if (firstRank == SPELL_FIRE_BLAST && HasCrownMarker(player, MOD_FROSTFIRE_FRACTURE)
            && Frostbitten(player, victim) && Ready(state.FractureAt, FRACTURE_EVERY_MS))
        {
            state.FractureAt = Now();
            float icicle = CrownPower(player) * FRACTURE_POWER;
            IcicleStrike(player, victim, icicle, false);
            std::size_t count = FROSTFIRE_TARGETS;
            for (Unit* enemy : EnemiesAround(player, victim, FROSTFIRE_RADIUS))
                if (enemy != victim && count)
                {
                    IcicleStrike(player, enemy, icicle, false);
                    --count;
                }
        }
    }

    // Hailstorm: each 2 sec of Blizzard's channel (Hailstorm Crown: each second), an Icicle at the enemy it damaged
    // in that time with the most health (by share), 20% of spell power (Hailstorm Crown 15%). Whiteout: also up to 2
    // within 6 yards for 40%, else 20% more. Gathering Blizzard: each, the next Ice Lance 5% more (20% at most).
    void HailTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        Spell* channel = player->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (!channel || channel->m_spellInfo->GetFirstRankSpell()->Id != SPELL_BLIZZARD)
        {
            state.HailTimer = 0;
            state.BlizzardHit.clear();
            return;
        }
        bool crown = HasCrownMarker(player, MOD_HAILSTORM_CROWN);
        uint32 every = crown ? HAILSTORM_CROWN_MS : HAILSTORM_MS;
        state.HailTimer += diff;
        if (state.HailTimer < every)
            return;
        state.HailTimer -= every;
        Unit* best = nullptr;
        for (auto const& [guid, at] : state.BlizzardHit)
            if (Now() - at <= every)
                if (Unit* enemy = ObjectAccessor::GetUnit(*player, guid);
                    Valid(player, enemy) && (!best || enemy->GetHealthPct() > best->GetHealthPct()))
                    best = enemy;
        state.BlizzardHit.clear();
        if (!best)
            return;
        float icicle = CrownPower(player) * (crown ? HAILSTORM_CROWN_POWER : HAILSTORM_POWER);
        if (HasCrownMarker(player, MOD_WHITEOUT))
        {
            std::size_t count = WHITEOUT_TARGETS;
            for (Unit* enemy : EnemiesAround(player, best, WHITEOUT_RADIUS))
                if (enemy != best && count)
                {
                    Strike(player, enemy, SPELL_ICE_SPLINTER, icicle * WHITEOUT_SHARE);
                    --count;
                }
            if (count == WHITEOUT_TARGETS)
                icicle *= WHITEOUT_ALONE;
        }
        IcicleStrike(player, best, icicle, false);
        if (HasCrownMarker(player, MOD_GATHERING_BLIZZARD))
        {
            Aura* gathering = player->GetAura(SPELL_GATHERING_BLIZZARD);
            if (!gathering)
                gathering = player->AddAura(SPELL_GATHERING_BLIZZARD, player);
            else if (gathering->GetStackAmount() < GATHERING_STACKS)
                gathering->ModStackAmount(1);
            if (gathering)
                gathering->RefreshDuration();
        }
    }

    // A direct Frostbolt or Ice Lance hit's damage: 8% more on the Hero's Frostbite; an Ice Lance spends Gathering
    // Blizzard (5% more a stack).
    float CrownBoltDamage(Player* player, Unit* victim, uint32 firstRank, float damage)
    {
        if (Frostbitten(player, victim))
            damage *= FROSTBITE_BONUS;
        if (firstRank == SPELL_ICE_LANCE)
            if (Aura* gathering = player->GetAura(SPELL_GATHERING_BLIZZARD))
            {
                damage *= 1.0f + GATHERING_STEP * float(gathering->GetStackAmount());
                player->RemoveAurasDueToSpell(SPELL_GATHERING_BLIZZARD);
            }
        return damage;
    }

    // ---- Phoenix Wake ------------------------------------------------------------------------------------------

    bool HasPhoenixMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_PHOENIX_MODIFIER_FIRST, modifier);
    }

    // A direct Pyroblast hit leaves the Ember (one: a new Pyroblast moves it), holding the hit's damage.
    void PlaceEmber(Player* player, Unit* target, float pyroblast)
    {
        State& state = StateOf(player);
        if (state.EmberHost && state.EmberHost != target->GetGUID())
            if (Unit* old = ObjectAccessor::GetUnit(*player, state.EmberHost))
                old->RemoveAurasDueToSpell(SPELL_PHOENIX_EMBER, player->GetGUID());
        state.EmberHost = target->GetGUID();
        state.EmberPyro = pyroblast;
        state.LavaFlockOn.Clear();
        Lasting(player, target, SPELL_PHOENIX_EMBER, EMBER_MS);
        Trace(player, Acore::StringFormat("Pyroblast hit {} for {}: Ember placed (on it: {})", target->GetName(),
            pyroblast, target->HasAura(SPELL_PHOENIX_EMBER, player->GetGUID()) ? "yes" : "NO"));
    }

    // Lava Flock (Lava Burst) and Hot Pursuit (Scorch): a direct hit on the Ember's host.
    void PhoenixStrike(Player* player, Unit* enemy, uint32 firstRank)
    {
        State& state = StateOf(player);
        if (enemy->GetGUID() != state.EmberHost || !enemy->HasAura(SPELL_PHOENIX_EMBER, player->GetGUID()))
            return;
        if (firstRank == SPELL_LAVA_BURST && HasPhoenixMarker(player, MOD_LAVA_FLOCK))
            state.LavaFlockOn = enemy->GetGUID();
        if (firstRank == SPELL_SCORCH && HasPhoenixMarker(player, MOD_HOT_PURSUIT))
            Renew(player, SPELL_HOT_PURSUIT);
    }

    // Crossing the Hero's Flame Shock target: an eruption of 20% of their spell power on up to 3 enemies within 6
    // yards of it (once a phoenix).
    void PhoenixErupt(Player* player, Flight& flight, Unit* enemy)
    {
        if (flight.Erupted || !enemy->GetAuraOfRankedSpell(SPELL_FLAME_SHOCK, player->GetGUID()))
            return;
        flight.Erupted = true;
        float burst = RunePower(player, SPELL_SCHOOL_MASK_FIRE, SPELL_PYROBLAST) * ERUPTION_POWER;
        for (Unit* other : EnemiesIn(player, Circle(enemy->GetPosition(), ERUPTION_RADIUS), ERUPTION_TARGETS))
            Strike(player, other, SPELL_PHOENIX_ERUPTION, burst);
    }

    // The Hero's Flamestrike out (any rank; nullptr none).
    DynamicObject* FlamestrikeArea(Player* player)
    {
        for (uint32 id = SPELL_FLAMESTRIKE; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (DynamicObject* area = player->GetDynObject(id))
                return area;
        return nullptr;
    }

    // A flight hit (Clinging Embers: 25% more of it over 6 sec).
    void FlightHit(Player* player, Unit* enemy, float amount)
    {
        Strike(player, enemy, SPELL_PHOENIX_FLIGHT, amount);
        if (HasPhoenixMarker(player, MOD_CLINGING_EMBERS) && enemy->IsAlive())
            AddDot(player, enemy, SPELL_CLINGING_EMBERS, amount * CLINGING_SHARE, CLINGING_MS);
    }

    // Fire Blast on the Ember's host calls the phoenix: it dives from high above the Hero onto the host at 20 yards
    // a second (Homeward Flame: 40), burning up to 4 enemies under its path for 25% of the Pyroblast's damage
    // (Broadwing: 30%, a wider path), and strikes the host for 50% (Sun Talon: 75%). Ashen Flock: 3 birds, a third
    // each. Then Death Spiral circles the host 2 sec and strikes it again; Wild Flight swoops through up to 2 enemies
    // within 10 yards of it (FlightTick).
    void ReleasePhoenix(Player* player, Unit* host, float pyroblast)
    {
        State& state = StateOf(player);
        uint32 release = ++state.Releases;
        bool flock = HasPhoenixMarker(player, MOD_ASHEN_FLOCK);
        bool broad = HasPhoenixMarker(player, MOD_BROADWING);
        if (HasPhoenixMarker(player, MOD_SCORCHED_EARTH))
        {
            state.ScorchTick = pyroblast * SCORCH_SHARE;
            state.ScorchMap = player->GetMapId();
        }
        // Flamekeeper: on the Hero's Flame Shock host, 20% of their spell power as it is released.
        if (HasPhoenixMarker(player, MOD_FLAMEKEEPER)
            && host->GetAuraOfRankedSpell(SPELL_FLAME_SHOCK, player->GetGUID()))
            Strike(player, host, SPELL_FLAMEKEEPER,
                RunePower(player, SPELL_SCHOOL_MASK_FIRE, SPELL_PYROBLAST) * FLAMEKEEPER_POWER);
        // Lava Flock: a Lava Burst hit this Ember's host, one bird more for 30%.
        bool lava = HasPhoenixMarker(player, MOD_LAVA_FLOCK) && state.LavaFlockOn == host->GetGUID();
        state.LavaFlockOn.Clear();
        uint32 birds = flock ? FLOCK_BIRDS : 1;
        uint32 total = birds + (lava ? 1 : 0);
        float facing = player->GetAngle(host);
        uint32 now = Now();
        for (uint32 i = 0; i < total; ++i)
        {
            bool extra = i >= birds;
            float side = extra ? FLOCK_SPREAD * float(birds > 1 ? 2 : 1)
                : birds > 1 ? (float(i) - 1.0f) * FLOCK_SPREAD : 0.0f;
            Position at = Toward(player->GetPosition(), facing + float(M_PI) / 2.0f, side);
            at = Toward(at, facing + float(M_PI), PHOENIX_BEHIND);
            at.m_positionZ = player->GetPositionZ() + PHOENIX_DROP;
            at.SetOrientation(facing);
            TempSummon* bird = SummonFlier(player, NPC_RUNE_PHOENIX, at, PHOENIX_MAX_MS, SPELL_PHOENIX_FORM);
            Trace(player, bird ? Acore::StringFormat("phoenix summoned {:.1f} yards from you, its look {}",
                player->GetExactDist(bird), bird->HasAura(SPELL_PHOENIX_FORM) ? "on" : "MISSING")
                : std::string("phoenix: the summon FAILED"));
            if (!bird)
                continue;
            if (flock || extra)
                bird->SetObjectScale(bird->GetObjectScale() * FLOCK_SCALE);
            Flight flight;
            flight.Bird = bird->GetGUID();
            flight.Host = host->GetGUID();
            flight.Release = release;
            flight.Share = extra ? LAVA_FLOCK_SHARE : 1.0f / float(birds);
            flight.Pyro = pyroblast;
            flight.Cross = pyroblast * (broad ? BROADWING_CROSS : PHOENIX_CROSS);
            flight.HalfWidth = broad ? BROADWING_HALF_WIDTH : PHOENIX_HALF_WIDTH;
            flight.Speed = HasPhoenixMarker(player, MOD_HOMEWARD_FLAME) ? HOMEWARD_SPEED : PHOENIX_SPEED;
            flight.Born = now;
            flight.Last = at;
            flight.LastTrail = at;
            state.Flights.push_back(flight);
        }
    }

    // A phoenix's flight over. Ashen Flock: each bird, the next Pyroblast 5% faster (up to 15%, 6 sec). Once a
    // release (a flock's birds share it): Homeward Flame, Fire Blast 2 sec closer; Rebirth, a burst of 35% of the
    // Pyroblast's damage to up to 5 enemies within 6 yards of where it ends.
    void PhoenixLands(Player* player, Flight const& flight, Position const& at)
    {
        State& state = StateOf(player);
        if (HasPhoenixMarker(player, MOD_ASHEN_FLOCK))
        {
            if (Aura* flock = player->GetAura(SPELL_ASHEN_FLOCK))
            {
                flock->ModStackAmount(1);
                flock->RefreshDuration();
            }
            else
                player->AddAura(SPELL_ASHEN_FLOCK, player);
        }
        if (state.ArrivedRelease == flight.Release)
            return;
        state.ArrivedRelease = flight.Release;
        // Wildfire Return: Flame Shock on its host 2 sec longer (6 sec an application); Emberguard: in Fire Ward,
        // an absorb of 40% of spell power (12 sec apart).
        Unit* host = ObjectAccessor::GetUnit(*player, flight.Host);
        if (HasPhoenixMarker(player, MOD_WILDFIRE_RETURN) && host)
            if (Aura* shock = host->GetAuraOfRankedSpell(SPELL_FLAME_SHOCK, player->GetGUID()))
                ExtendAura(shock, WILDFIRE_EXTEND_MS, WILDFIRE_MOST_MS);
        if (HasPhoenixMarker(player, MOD_EMBERGUARD) && Ready(state.EmberguardAt, EMBERGUARD_EVERY_MS)
            && player->GetAuraOfRankedSpell(SPELL_FIRE_WARD, player->GetGUID()))
        {
            state.EmberguardAt = Now();
            Ward(player, player, SPELL_EMBERGUARD,
                RunePower(player, SPELL_SCHOOL_MASK_FIRE, SPELL_PYROBLAST, false) * EMBERGUARD_POWER);
        }
        if (HasPhoenixMarker(player, MOD_HOMEWARD_FLAME))
            if (uint32 rank = KnownRank(player, SPELL_FIRE_BLAST); rank && player->GetSpellCooldownDelay(rank))
                player->ModifySpellCooldown(rank, -int32(std::min<uint32>(HOMEWARD_MS,
                    player->GetSpellCooldownDelay(rank))));
        if (!HasPhoenixMarker(player, MOD_REBIRTH))
            return;
        std::vector<Unit*> enemies = EnemiesIn(player, Circle(at, REBIRTH_RADIUS), REBIRTH_TARGETS);
        if (!enemies.empty())
            enemies.front()->CastSpell(enemies.front(), SPELL_REBIRTH_LOOK, true);
        for (Unit* enemy : enemies)
            Strike(player, enemy, SPELL_REBIRTH, flight.Pyro * REBIRTH_SHARE);
    }

    // Scorched Earth's trail: a tick a second (6% of the Pyroblast's damage) to up to 5 enemies on it, once however
    // many patches they stand on.
    void ScorchTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        uint32 now = Now();
        state.Scorch.erase(std::remove_if(state.Scorch.begin(), state.Scorch.end(),
            [now](std::pair<Position, uint32> const& patch) { return now - patch.second >= SCORCH_MS; }),
            state.Scorch.end());
        if (state.Scorch.empty() || player->GetMapId() != state.ScorchMap)
        {
            state.Scorch.clear();
            state.ScorchTimer = 0;
            return;
        }
        state.ScorchTimer += diff;
        if (state.ScorchTimer < SCORCH_TICK_MS)
            return;
        state.ScorchTimer = 0;
        std::size_t burnt = 0;
        for (Unit* enemy : EnemiesAround(player, player, SCORCH_REACH))
        {
            if (burnt >= SCORCH_TARGETS)
                break;
            float reach = SCORCH_HALF_WIDTH + enemy->GetCombatReach() * 0.5f;
            if (std::any_of(state.Scorch.begin(), state.Scorch.end(), [enemy, reach](auto const& patch)
                {
                    return enemy->GetExactDist2d(&patch.first) <= reach;
                }))
            {
                Strike(player, enemy, SPELL_SCORCHED_EARTH_TICK, state.ScorchTick);
                ++burnt;
            }
        }
    }

    // The phoenixes in flight: each burns what it passes over since the last update; dives onto its host (its
    // course renewed every 0.3 sec: the host moves) and strikes it; then circles it, then visits; then its flight
    // ends where it is.
    void FlightTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        uint32 now = Now();
        bool scorch = HasPhoenixMarker(player, MOD_SCORCHED_EARTH);
        for (auto itr = state.Flights.begin(); itr != state.Flights.end();)
        {
            Flight& flight = *itr;
            Creature* bird = ObjectAccessor::GetCreature(*player, flight.Bird);
            if (!bird || !bird->IsAlive() || !player->IsAlive() || !bird->IsInMap(player)
                || now - flight.Born >= PHOENIX_MAX_MS)
            {
                Trace(player, !bird ? "phoenix gone (not found)" : now - flight.Born >= PHOENIX_MAX_MS
                    ? "phoenix gone (its 8 sec are up)" : "phoenix gone (dead, or another map)");
                Dismiss(player, flight.Bird);
                itr = state.Flights.erase(itr);
                continue;
            }
            Position here = bird->GetPosition();
            Shape path;
            path.From = flight.Last;
            path.To = here;
            path.HalfWidth = flight.HalfWidth;
            for (Unit* enemy : EnemiesIn(player, path, CROSS_TARGETS + 2))
                if (enemy->GetGUID() != flight.Host && flight.Crossed < CROSS_TARGETS
                    && flight.Hit.insert(enemy->GetGUID()).second)
                {
                    FlightHit(player, enemy, flight.Cross * flight.Share);
                    PhoenixErupt(player, flight, enemy);
                    ++flight.Crossed;
                }
            if (!flight.Blazed && HasPhoenixMarker(player, MOD_BLAZING_PATH))
                if (DynamicObject* area = FlamestrikeArea(player);
                    area && area->GetExactDist2d(&here) <= area->GetRadius())
                {
                    flight.Blazed = true;
                    float blaze = RunePower(player, SPELL_SCHOOL_MASK_FIRE, SPELL_PYROBLAST) * BLAZING_PATH_POWER;
                    for (Unit* enemy : EnemiesIn(player, Circle(area->GetPosition(), area->GetRadius()),
                        BLAZING_PATH_TARGETS))
                        Strike(player, enemy, SPELL_BLAZING_PATH, blaze);
                }
            if (scorch && here.GetExactDist2d(&flight.LastTrail) >= SCORCH_SPACING)
            {
                float z = here.GetPositionZ();
                player->UpdateGroundPositionZ(here.GetPositionX(), here.GetPositionY(), z);
                Position ground(here.GetPositionX(), here.GetPositionY(), z);
                state.Scorch.emplace_back(ground, now);
                player->CastSpell(ground.GetPositionX(), ground.GetPositionY(), z, SPELL_SCORCHED_EARTH, true);
                flight.LastTrail = here;
            }
            flight.Last = here;
            Unit* host = ObjectAccessor::GetUnit(*player, flight.Host);
            Unit* to = nullptr;
            if (!flight.Struck)
            {
                if (!Valid(player, host))
                {
                    PhoenixLands(player, flight, here);
                    Dismiss(player, flight.Bird);
                    itr = state.Flights.erase(itr);
                    continue;
                }
                if (bird->GetExactDist(host) <= STRIKE_REACH)
                {
                    Trace(player, Acore::StringFormat("phoenix struck {}", host->GetName()));
                    flight.Struck = true;
                    flight.Repath = 0;
                    flight.Hit.insert(host->GetGUID());
                    float bonus = HasPhoenixMarker(player, MOD_SUN_TALON) ? SUN_TALON : 1.0f;
                    if (player->HasAura(SPELL_HOT_PURSUIT))
                    {
                        player->RemoveAurasDueToSpell(SPELL_HOT_PURSUIT);
                        bonus *= HOT_PURSUIT;
                    }
                    FlightHit(player, host, flight.Pyro * PHOENIX_HOST * flight.Share * bonus);
                    // Phoenix Burn: 30% of spell power over 6 sec on its host (once a release).
                    if (state.BurnRelease != flight.Release && host->IsAlive())
                    {
                        state.BurnRelease = flight.Release;
                        AddDot(player, host, SPELL_PHOENIX_BURN,
                            RunePower(player, SPELL_SCHOOL_MASK_FIRE, SPELL_PYROBLAST) * PHOENIX_BURN_POWER,
                            PHOENIX_BURN_MS);
                    }
                    if (HasPhoenixMarker(player, MOD_DEATH_SPIRAL))
                    {
                        flight.SpiralUntil = now + SPIRAL_MS;
                        bird->GetMotionMaster()->MoveCirclePath(host->GetPositionX(), host->GetPositionY(),
                            host->GetPositionZ() + PHOENIX_HEIGHT, SPIRAL_RADIUS, true, 8, FORCED_MOVEMENT_NONE,
                            flight.Speed);
                    }
                    else if (HasPhoenixMarker(player, MOD_WILD_FLIGHT))
                        for (Unit* enemy : EnemiesAround(player, host, WILD_FLIGHT_RANGE))
                            if (enemy != host && flight.Visits.size() < WILD_FLIGHT_VISITS)
                                flight.Visits.push_back(enemy->GetGUID());
                    ++itr;
                    continue;
                }
                to = host;
            }
            else if (flight.SpiralUntil)
            {
                if (now < flight.SpiralUntil)
                {
                    ++itr;
                    continue;
                }
                flight.SpiralUntil = 0;
                if (Valid(player, host))
                    Strike(player, host, SPELL_PHOENIX_FLIGHT, flight.Pyro * SPIRAL_SHARE * flight.Share);
            }
            else if (!flight.Visits.empty())
            {
                Unit* next = ObjectAccessor::GetUnit(*player, flight.Visits.front());
                if (!Valid(player, next))
                {
                    flight.Visits.erase(flight.Visits.begin());
                    flight.Repath = 0;
                    ++itr;
                    continue;
                }
                if (bird->GetExactDist2d(next) <= VISIT_REACH)
                {
                    if (flight.Hit.insert(next->GetGUID()).second)
                    {
                        FlightHit(player, next, flight.Cross * flight.Share);
                        PhoenixErupt(player, flight, next);
                    }
                    flight.Visits.erase(flight.Visits.begin());
                    flight.Repath = 0;
                    ++itr;
                    continue;
                }
                to = next;
            }
            if (!to)
            {
                PhoenixLands(player, flight, here);
                Dismiss(player, flight.Bird);
                itr = state.Flights.erase(itr);
                continue;
            }
            if (flight.Repath <= diff)
            {
                Position goal = to->GetPosition();
                goal.m_positionZ += PHOENIX_HEIGHT;
                bird->GetMotionMaster()->MovePoint(0, goal, FORCED_MOVEMENT_NONE, flight.Speed, false);
                flight.Repath = REPATH_MS;
            }
            else
                flight.Repath -= diff;
            ++itr;
        }
        ScorchTick(player, diff);
    }

    void DismissPhoenixes(Player* player)
    {
        State& state = StateOf(player);
        for (Flight const& flight : state.Flights)
            Dismiss(player, flight.Bird);
        state.Flights.clear();
        state.Scorch.clear();
        player->RemoveDynObject(SPELL_SCORCHED_EARTH);
        if (state.EmberHost)
            if (Unit* host = ObjectAccessor::GetUnit(*player, state.EmberHost))
                host->RemoveAurasDueToSpell(SPELL_PHOENIX_EMBER, player->GetGUID());
        state.EmberHost.Clear();
    }

    // ---- Astral Horizon ----------------------------------------------------------------------------------------

    bool HasHorizonMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_HORIZON_MODIFIER_FIRST, modifier);
    }

    // Where the Orb's bolts fly from: the Orb, or (Satellite) the Hero it circles; nullptr: none out.
    Unit* OrbSource(Player* player)
    {
        State& state = StateOf(player);
        if (state.OrbSatellite)
            return player;
        Creature* orb = state.Orb ? ObjectAccessor::GetCreature(*player, state.Orb) : nullptr;
        return orb && orb->IsAlive() && orb->IsInMap(player) ? orb : nullptr;
    }

    void DismissOrb(Player* player)
    {
        State& state = StateOf(player);
        Dismiss(player, state.Orb);
        state.Orb.Clear();
        state.OrbSatellite = false;
        player->RemoveAurasDueToSpell(SPELL_ORB_SATELLITE);
    }

    // The Orb's bolt (or lance) at an enemy, from the Orb, the Hero's: Event Horizon 20% more within 5 yards of it,
    // Astral Focus 40% more on its victim.
    void OrbStrike(Player* player, Unit* orb, Unit* enemy, float share, uint32 spellId)
    {
        State& state = StateOf(player);
        float amount = state.OrbBolt * share;
        if (HasHorizonMarker(player, MOD_EVENT_HORIZON) && orb->GetExactDist2d(enemy) <= EVENT_RADIUS)
            amount *= EVENT_HORIZON;
        if (HasHorizonMarker(player, MOD_ASTRAL_FOCUS) && enemy->GetGUID() == state.OrbVictim)
            amount *= ASTRAL_FOCUS;
        // Starward Satellite: on the Hero's Moonfire target 20% more, and the Moonfire 1 sec longer (4 sec an
        // application, 2 sec apart).
        if (HasHorizonMarker(player, MOD_STARWARD_SATELLITE))
            if (Aura* moonfire = enemy->GetAuraOfRankedSpell(SPELL_MOONFIRE, player->GetGUID()))
            {
                amount *= STARWARD;
                if (Ready(state.StarwardAt, STARWARD_EVERY_MS))
                {
                    state.StarwardAt = Now();
                    ExtendAura(moonfire, STARWARD_EXTEND_MS, STARWARD_MOST_MS);
                }
            }
        int32 points = std::max<int32>(1, int32(amount));
        orb->CastCustomSpell(enemy, spellId, &points, nullptr, nullptr, true, nullptr, nullptr, player->GetGUID());
    }

    // The Orb set loose: two bolts at its victim; Astral Shelter, in the Hero's Power Word: Shield, an absorb of
    // 25% of their spell power (12 sec apart).
    void OrbLaunched(Player* player, Unit* from, Unit* victim)
    {
        State& state = StateOf(player);
        OrbStrike(player, from, victim, ORB_LAUNCH_SHARE, SPELL_ORB_LAUNCH);
        if (HasHorizonMarker(player, MOD_ASTRAL_SHELTER) && Ready(state.ShelterAt, SHELTER_EVERY_MS)
            && player->GetAuraOfRankedSpell(SPELL_POWER_WORD_SHIELD, player->GetGUID()))
        {
            state.ShelterAt = Now();
            Ward(player, player, SPELL_ASTRAL_SHELTER,
                RunePower(player, SPELL_SCHOOL_MASK_ARCANE, SPELL_ARCANE_BARRAGE, false) * SHELTER_POWER);
        }
    }

    // The Orb's victim, while it is out (nullptr: none, or another enemy).
    Unit* OrbVictimHit(Player* player, Unit* enemy)
    {
        State& state = StateOf(player);
        return OrbSource(player) && enemy->GetGUID() == state.OrbVictim ? OrbSource(player) : nullptr;
    }

    // Arcane Blast on the Orb's victim: its next bolt 35% more.
    void HorizonBlast(Player* player, Unit* enemy)
    {
        if (OrbVictimHit(player, enemy))
            StateOf(player).OrbEmpower = BLAST_EMPOWER;
    }

    // Starfire on the Orb's victim: its next bolt also at one more enemy within 8 yards for half.
    void HorizonStarfire(Player* player, Unit* enemy)
    {
        if (OrbVictimHit(player, enemy))
            StateOf(player).OrbSplit = true;
    }

    // Orbital Lightning (Lightning Bolt) and Distant Spark (Arcane Shot) on the Orb's victim.
    void HorizonStrike(Player* player, Unit* enemy, uint32 firstRank)
    {
        Unit* orb = OrbVictimHit(player, enemy);
        if (!orb)
            return;
        State& state = StateOf(player);
        if (firstRank == SPELL_LIGHTNING_BOLT && HasHorizonMarker(player, MOD_ORBITAL_LIGHTNING)
            && Ready(state.OrbLightningAt, ORBITAL_LIGHTNING_EVERY_MS))
        {
            state.OrbLightningAt = Now();
            OrbStrike(player, orb, enemy, 1.0f, SPELL_ORBITAL_LIGHTNING);
        }
        if (firstRank == SPELL_ARCANE_SHOT && HasHorizonMarker(player, MOD_DISTANT_SPARK))
            state.SparkUntil = Now() + DISTANT_SPARK_MS;
    }

    // Arcane Explosion within 8 yards of the Orb: a bolt's worth to up to 5 enemies within 8 yards of it (4 sec
    // apart).
    void HorizonExplosion(Player* player)
    {
        State& state = StateOf(player);
        Unit* orb = OrbSource(player);
        if (!orb || !Ready(state.OrbPulseAt, ORB_PULSE_EVERY_MS)
            || (orb != player && !player->IsWithinDist(orb, ORB_PULSE_RANGE)))
            return;
        state.OrbPulseAt = Now();
        std::size_t count = ORB_PULSE_TARGETS;
        for (Unit* enemy : EnemiesAround(player, orb, ORB_PULSE_RANGE))
            if (count)
            {
                OrbStrike(player, orb, enemy, 1.0f, SPELL_ORB_PULSE);
                --count;
            }
    }

    // Evoked Remnant: Evocation completed while the Orb is out, 2 sec longer (once an Orb).
    void HorizonEvocation(Player* player)
    {
        State& state = StateOf(player);
        if (!HasHorizonMarker(player, MOD_EVOKED_REMNANT) || !OrbSource(player) || state.OrbEvoked)
            return;
        state.OrbEvoked = true;
        state.OrbUntil += EVOKED_MS;
        if (Aura* satellite = player->GetAura(SPELL_ORB_SATELLITE))
        {
            satellite->SetMaxDuration(satellite->GetMaxDuration() + int32(EVOKED_MS));
            satellite->SetDuration(satellite->GetDuration() + int32(EVOKED_MS));
        }
    }

    // The Orb's course to a point, its height kept over the ground there.
    void OrbFly(Player* player, Creature* orb, Position to, float speed)
    {
        float z = to.GetPositionZ();
        player->UpdateGroundPositionZ(to.GetPositionX(), to.GetPositionY(), z);
        to.m_positionZ = z + ORB_HEIGHT;
        orb->GetMotionMaster()->MovePoint(0, to, FORCED_MOVEMENT_NONE, speed, false);
    }

    // Arcane Barrage sets the Orb loose (at most once every 12 sec; a new one replaces it, no ending): 6 sec, a bolt
    // a second for 20% of the Barrage's damage. Its course: on through its victim at 6 yards a second; Event
    // Horizon, to the victim's place and there it stays. Satellite: no creature; the Hero wears it, circling them
    // (its aura's look, drawn by the client), and its bolts fly from them.
    void LaunchOrb(Player* player, Unit* victim, float barrage)
    {
        State& state = StateOf(player);
        uint32 now = Now();
        if (now < state.OrbReadyAt || barrage <= 0.0f)
            return;
        state.OrbReadyAt = now + ORB_COOLDOWN_MS;
        DismissOrb(player);
        state.OrbVictim = victim->GetGUID();
        state.OrbUntil = now + ORB_MS;
        state.OrbTimer = 0;
        state.OrbBolt = barrage * ORB_BOLT_SHARE;
        state.OrbCrossed.clear();
        state.OrbEmpower = 1.0f;
        state.OrbSplit = state.OrbEvoked = false;
        state.SparkUntil = 0;
        if (HasHorizonMarker(player, MOD_SATELLITE))
        {
            state.OrbSatellite = true;
            state.OrbUntil = now + SATELLITE_MS;
            if (Aura* aura = player->AddAura(SPELL_ORB_SATELLITE, player))
            {
                aura->SetMaxDuration(int32(SATELLITE_MS));
                aura->SetDuration(int32(SATELLITE_MS));
            }
            OrbLaunched(player, player, victim);
            return;
        }
        float angle = player->GetAngle(victim);
        Position at = Toward(player->GetPosition(), angle, ORB_START);
        at.m_positionZ = player->GetPositionZ() + ORB_HEIGHT;
        at.SetOrientation(angle);
        TempSummon* orb = SummonFlier(player, NPC_RUNE_ARCANE_ORB, at, ORB_MS + EVOKED_MS + ORB_LINGER_MS,
            SPELL_ORB_FORM);
        if (!orb)
            return;
        state.Orb = orb->GetGUID();
        OrbLaunched(player, orb, victim);
        state.OrbLast = at;
        state.OrbHeading = angle;
        state.OrbSlowed = false;
        Position to = HasHorizonMarker(player, MOD_EVENT_HORIZON) ? victim->GetPosition()
            : Toward(at, angle, ORB_FAST * float(ORB_MS) / 1000.0f);
        OrbFly(player, orb, to, ORB_FAST);
    }

    // A bolt a second at its victim within 10 yards of the Orb (Satellite: 12), else the nearest enemy there.
    // Constellation: 2 more enemies in range for half. Prismatic Lance: a lance through up to 3 enemies behind its
    // target, 60% each.
    void OrbVolley(Player* player, Unit* orb)
    {
        State& state = StateOf(player);
        float range = HasHorizonMarker(player, MOD_SATELLITE) ? SATELLITE_RANGE : ORB_RANGE;
        std::vector<Unit*> near = EnemiesAround(player, orb, range);
        Unit* victim = ObjectAccessor::GetUnit(*player, state.OrbVictim);
        Unit* target = std::find(near.begin(), near.end(), victim) != near.end() ? victim
            : near.empty() ? nullptr : near.front();
        if (!target)
            return;
        // Arcane Blast's empowering (35% more), spent on this bolt.
        float first = state.OrbEmpower;
        state.OrbEmpower = 1.0f;
        if (HasHorizonMarker(player, MOD_PRISMATIC_LANCE))
        {
            OrbStrike(player, orb, target, first, SPELL_PRISMATIC_LANCE);
            Shape line;
            line.From = target->GetPosition();
            line.To = Toward(line.From, orb->GetAngle(target), LANCE_REACH);
            line.HalfWidth = LANCE_HALF_WIDTH;
            std::size_t count = LANCE_TARGETS;
            for (Unit* enemy : EnemiesIn(player, line, LANCE_TARGETS + 1))
                if (enemy != target && count)
                {
                    OrbStrike(player, orb, enemy, LANCE_SHARE, SPELL_PRISMATIC_LANCE);
                    --count;
                }
        }
        else
            OrbStrike(player, orb, target, first, SPELL_ORB_BOLT);
        // Starfire's split and Distant Spark's pierce (5 sec): one more enemy within 8 yards for half, each.
        uint32 extra = (state.OrbSplit ? 1 : 0) + (state.SparkUntil && Now() < state.SparkUntil ? 1 : 0);
        state.OrbSplit = false;
        state.SparkUntil = 0;
        for (Unit* enemy : EnemiesAround(player, target, SPLIT_RANGE))
            if (enemy != target && extra)
            {
                OrbStrike(player, orb, enemy, ORB_SPLIT, SPELL_ORB_BOLT);
                --extra;
            }
        // Shattering Orbit: on the Hero's Frost Nova target, one more in their Frost Nova within 8 yards for half.
        if (HasHorizonMarker(player, MOD_SHATTERING_ORBIT)
            && target->GetAuraOfRankedSpell(SPELL_FROST_NOVA, player->GetGUID()))
            for (Unit* enemy : EnemiesAround(player, target, SPLIT_RANGE))
                if (enemy != target && enemy->GetAuraOfRankedSpell(SPELL_FROST_NOVA, player->GetGUID()))
                {
                    OrbStrike(player, orb, enemy, SHATTERING_SHARE, SPELL_ORB_BOLT);
                    break;
                }
        if (!HasHorizonMarker(player, MOD_CONSTELLATION))
            return;
        std::size_t count = CONSTELLATION_TARGETS;
        for (Unit* enemy : near)
            if (enemy != target && count)
            {
                OrbStrike(player, orb, enemy, CONSTELLATION_SHARE, SPELL_ORB_BOLT);
                --count;
            }
    }

    // The Orb's natural end. Supernova: 4 bolts' damage to up to 5 enemies within 8 yards; Dying Light: 3 bolts at
    // the Hero's target (else its victim), twice a bolt each; Astral Remnant: a field of 6 yards for 6 sec, a bolt's
    // damage every 2 sec to up to 5 enemies.
    void OrbEnding(Player* player, Unit* orb)
    {
        State& state = StateOf(player);
        Position at = orb->GetPosition();
        if (HasHorizonMarker(player, MOD_SUPERNOVA))
        {
            orb->CastSpell(orb, SPELL_SUPERNOVA_LOOK, true);
            for (Unit* enemy : EnemiesIn(player, Circle(at, SUPERNOVA_RADIUS), SUPERNOVA_TARGETS))
                OrbStrike(player, orb, enemy, SUPERNOVA_BOLTS, SPELL_SUPERNOVA);
        }
        if (HasHorizonMarker(player, MOD_DYING_LIGHT))
        {
            Unit* target = player->GetSelectedUnit();
            if (!Valid(player, target))
                target = ObjectAccessor::GetUnit(*player, state.OrbVictim);
            if (Valid(player, target))
            {
                ObjectGuid orbGuid = orb->GetGUID(), targetGuid = target->GetGUID();
                for (uint32 i = 0; i < DYING_BOLTS; ++i)
                    player->m_Events.AddEventAtOffset([player, orbGuid, targetGuid]()
                    {
                        Unit* from = ObjectAccessor::GetUnit(*player, orbGuid);
                        Unit* to = ObjectAccessor::GetUnit(*player, targetGuid);
                        if (player->IsInWorld() && from && Valid(player, to))
                            OrbStrike(player, from, to, DYING_SHARE, SPELL_ORB_BOLT);
                    }, Milliseconds(i * DYING_SPACING_MS));
            }
        }
        if (HasHorizonMarker(player, MOD_ASTRAL_REMNANT))
        {
            float z = at.GetPositionZ();
            player->UpdateGroundPositionZ(at.GetPositionX(), at.GetPositionY(), z);
            at.m_positionZ = z;
            state.RemnantAt = at;
            state.RemnantMap = player->GetMapId();
            state.RemnantUntil = Now() + REMNANT_MS;
            state.RemnantTimer = 0;
            state.RemnantTick = state.OrbBolt;
            PlaceDecals(player, SPELL_ASTRAL_REMNANT, { at });
        }
    }

    void RemnantTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        if (!state.RemnantUntil)
            return;
        if (Now() >= state.RemnantUntil || player->GetMapId() != state.RemnantMap)
        {
            state.RemnantUntil = 0;
            player->RemoveDynObject(SPELL_ASTRAL_REMNANT);
            return;
        }
        state.RemnantTimer += diff;
        if (state.RemnantTimer < REMNANT_TICK_MS)
            return;
        state.RemnantTimer = 0;
        for (Unit* enemy : EnemiesIn(player, Circle(state.RemnantAt, REMNANT_RADIUS), REMNANT_TARGETS))
            Strike(player, enemy, SPELL_ASTRAL_REMNANT_TICK, state.RemnantTick);
    }

    // The Orb out: Wandering Star burns each enemy it passes through once (a bolt's damage, up to 5); a bolt a
    // second; its natural end after 6 sec (it lingers a moment for Dying Light's bolts).
    void OrbTick(Player* player, uint32 diff)
    {
        RemnantTick(player, diff);
        State& state = StateOf(player);
        if (!state.Orb && !state.OrbSatellite)
            return;
        Unit* orb = OrbSource(player);
        if (!orb || !player->IsAlive())
        {
            DismissOrb(player);
            return;
        }
        Position here = orb->GetPosition();
        // Reaching its first enemy (Event Horizon: its place), it slows to a drift (Event Horizon: it stays).
        if (Creature* creature = orb->ToCreature(); creature && !state.OrbSlowed)
        {
            bool horizon = HasHorizonMarker(player, MOD_EVENT_HORIZON);
            Unit* victim = ObjectAccessor::GetUnit(*player, state.OrbVictim);
            bool reached = !EnemiesAround(player, creature, ORB_CONTACT).empty()
                || (horizon && victim && creature->GetExactDist2d(victim) <= ORB_CONTACT);
            if (reached)
            {
                state.OrbSlowed = true;
                if (horizon)
                    creature->GetMotionMaster()->Clear();
                else
                {
                    float left = float(state.OrbUntil - std::min(state.OrbUntil, Now())) / 1000.0f;
                    OrbFly(player, creature, Toward(here, state.OrbHeading, std::max(1.0f, ORB_SLOW * left)),
                        ORB_SLOW);
                }
            }
        }
        if (!state.OrbSatellite && HasHorizonMarker(player, MOD_WANDERING_STAR))
        {
            Shape path;
            path.From = state.OrbLast;
            path.To = here;
            path.HalfWidth = WANDERING_HALF_WIDTH;
            for (Unit* enemy : EnemiesIn(player, path, WANDERING_TARGETS))
                if (state.OrbCrossed.size() < WANDERING_TARGETS && state.OrbCrossed.insert(enemy->GetGUID()).second)
                    OrbStrike(player, orb, enemy, WANDERING_SHARE, SPELL_WANDERING_STAR);
        }
        state.OrbLast = here;
        state.OrbTimer += diff;
        if (state.OrbTimer >= ORB_BOLT_MS)
        {
            state.OrbTimer -= ORB_BOLT_MS;
            OrbVolley(player, orb);
        }
        if (Now() < state.OrbUntil)
            return;
        OrbEnding(player, orb);
        if (Creature* creature = orb->ToCreature())
        {
            creature->GetMotionMaster()->Clear();
            creature->DespawnOrUnsummon(Milliseconds(ORB_LINGER_MS));
            state.Orb.Clear();
        }
        else
            DismissOrb(player);
    }

    // ---- Guiding Light -----------------------------------------------------------------------------------------

    bool HasGuidingMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_GUIDING_MODIFIER_FIRST, modifier);
    }

    // A Prayer of Mending heal of the Hero's on the ally (their rank's, their healing power and bonuses).
    float PrayerHeal(Player* player, Unit* ally)
    {
        uint32 rank = KnownRank(player, SPELL_PRAYER_OF_MENDING);
        SpellInfo const* prayer = sSpellMgr->GetSpellInfo(rank ? rank : SPELL_PRAYER_OF_MENDING);
        SpellInfo const* heal = sSpellMgr->GetSpellInfo(SPELL_PRAYER_OF_MENDING_HEAL);
        if (!prayer || !heal)
            return 0.0f;
        int32 base = std::max(0, prayer->Effects[EFFECT_0].CalcValue(player));
        return float(player->SpellHealingBonusDone(ally, heal, uint32(base), HEAL, EFFECT_0));
    }

    // A pulse's heal on the bearer: half a Prayer of Mending (Steady Light: 75%).
    float StarPulse(Player* player, Unit* bearer)
    {
        return PrayerHeal(player, bearer) * STAR_PULSE_SHARE
            * (HasGuidingMarker(player, MOD_STEADY_LIGHT) ? STEADY_LIGHT : 1.0f);
    }

    // A bearer: the Hero or one of their party or raid, alive, within 40 yards.
    bool CanBear(Player* player, Unit* ally)
    {
        if (!ally || !ally->IsAlive() || !ally->IsInMap(player) || !player->IsWithinDistInMap(ally, STAR_RANGE))
            return false;
        Player* other = ally->ToPlayer();
        return ally == player || (other && player->IsInSameRaidWith(other));
    }

    // The Hero's allies within range of a unit, most injured first (by share of health).
    std::vector<Unit*> InjuredNear(Player* player, Unit* center, float range)
    {
        std::vector<Unit*> allies;
        for (Unit* ally : MostInjured(player, STAR_RANGE + range))
            if (ally->IsWithinDistInMap(center, range))
                allies.push_back(ally);
        return allies;
    }

    void Mend(Player* player, Unit* ally, uint32 spellId, float amount)
    {
        int32 points = std::max<int32>(1, int32(amount));
        player->CastCustomSpell(ally, spellId, &points, nullptr, nullptr, true);
    }

    void ClearStar(Player* player)
    {
        State& state = StateOf(player);
        if (Unit* bearer = state.StarBearer ? ObjectAccessor::GetUnit(*player, state.StarBearer) : nullptr)
            bearer->RemoveAurasDueToSpell(SPELL_GUIDING_STAR, player->GetGUID());
        state.StarBearer.Clear();
        state.StarUntil = 0;
        state.StarTravelling = false;
    }

    // The star's aura on its bearer for what is left of its 12 sec.
    void WearStar(Player* player, Unit* bearer)
    {
        State& state = StateOf(player);
        int32 left = int32(state.StarUntil - std::min(state.StarUntil, Now()));
        bearer->RemoveAurasDueToSpell(SPELL_GUIDING_STAR, player->GetGUID());
        if (Aura* aura = player->AddAura(SPELL_GUIDING_STAR, bearer))
        {
            aura->SetMaxDuration(STAR_MS);
            aura->SetDuration(std::max(1, left));
        }
    }

    // Prayer of Mending: the star on its target, 12 sec (a new one replaces it, no farewell).
    void PlaceStar(Player* player, Unit* ally)
    {
        ClearStar(player);
        State& state = StateOf(player);
        state.StarBearer = ally->GetGUID();
        state.StarUntil = Now() + STAR_MS;
        state.StarPulseTimer = 0;
        state.StarFocus = 0;
        state.StarPulses = 0;
        state.StarMovedAt = 0;
        WearStar(player, ally);
        if (ally != player)
            player->CastSpell(ally, SPELL_STAR_FLIGHT, true);
    }

    // The star leaves its bearer for another (0.5 sec on the way; its lifetime runs on). Gentle Passage: up to 3
    // injured allies along the way healed for half a pulse, once.
    void MoveStar(Player* player, Unit* from, Unit* to)
    {
        State& state = StateOf(player);
        if (from)
        {
            from->RemoveAurasDueToSpell(SPELL_GUIDING_STAR, player->GetGUID());
            from->CastSpell(to, SPELL_STAR_FLIGHT, true);
            // Leaving the Hero's Renew target: a trail, 15% of spell power every 2 sec for 6 sec (renewed).
            if (from->IsAlive() && from->GetAuraOfRankedSpell(SPELL_RENEW, player->GetGUID()))
            {
                int32 tick = std::max<int32>(1, int32(StarPulse(player, from) * TRAIL_SHARE));
                from->RemoveAurasDueToSpell(SPELL_GUIDING_TRAIL, player->GetGUID());
                Report(player, SPELL_GUIDING_TRAIL, 'H', tick);
                player->CastCustomSpell(from, SPELL_GUIDING_TRAIL, &tick, nullptr, nullptr, true);
            }
        }
        if (from && HasGuidingMarker(player, MOD_GENTLE_PASSAGE))
        {
            float half = StarPulse(player, to) * PASSAGE_SHARE;
            std::size_t count = PASSAGE_ALLIES;
            for (Unit* ally : MostInjured(player, STAR_RANGE))
                if (count && ally != from && ally != to && !ally->IsFullHealth()
                    && SegmentDist(ally->GetPosition(), from->GetPosition(), to->GetPosition()) <= PASSAGE_HALF_WIDTH)
                {
                    Mend(player, ally, SPELL_GENTLE_PASSAGE, half);
                    --count;
                }
        }
        state.StarBearer = to->GetGUID();
        state.StarTravelling = true;
        state.StarArriveAt = Now() + STAR_TRAVEL_MS;
        state.StarMovedAt = Now();
        state.StarFocus = 0;
    }

    // Its arrival: 75% of a Prayer of Mending (Sheltering Light: an absorb of a pulse twice over, 6 sec, renewed).
    void StarArrive(Player* player, Unit* bearer)
    {
        State& state = StateOf(player);
        state.StarTravelling = false;
        state.StarPulseTimer = 0;
        WearStar(player, bearer);
        float pulse = StarPulse(player, bearer);
        if (HasGuidingMarker(player, MOD_SHELTERING_LIGHT))
        {
            int32 shield = std::max<int32>(1, int32(pulse * SHELTER_SHARE));
            bearer->RemoveAurasDueToSpell(SPELL_SHELTERING_LIGHT, player->GetGUID());
            Report(player, SPELL_SHELTERING_LIGHT, 'A', shield);
            player->CastCustomSpell(bearer, SPELL_SHELTERING_LIGHT, &shield, nullptr, nullptr, true);
        }
        else
            Mend(player, bearer, SPELL_STAR_ARRIVAL, pulse * ARRIVAL_SHARE);
        // On the Hero's Earth Shield bearer: 30% of spell power to one injured ally within 10 yards (no charge).
        if (bearer->GetAuraOfRankedSpell(SPELL_EARTH_SHIELD, player->GetGUID()))
            for (Unit* ally : InjuredNear(player, bearer, EARTH_RANGE))
                if (ally != bearer && !ally->IsFullHealth())
                {
                    Mend(player, ally, SPELL_STAR_PULSE, pulse * EARTH_ARRIVAL_SHARE);
                    break;
                }
    }

    // Flash Heal or Greater Heal on another ally (Guardian Star: Greater Heal only) moves the star there, at most
    // once every 3 sec.
    void RedirectStar(Player* player, Unit* ally, bool greater)
    {
        State& state = StateOf(player);
        if (!state.StarBearer || state.StarTravelling || ally->GetGUID() == state.StarBearer || !CanBear(player, ally))
            return;
        if (HasGuidingMarker(player, MOD_GUARDIAN_STAR) && !greater)
            return;
        if (state.StarMovedAt && Now() - state.StarMovedAt < STAR_MOVE_MS)
            return;
        MoveStar(player, ObjectAccessor::GetUnit(*player, state.StarBearer), ally);
        // Greater Heal's redirect: an absorb of 30% of spell power on the new bearer (6 sec).
        if (greater)
            Ward(player, ally, SPELL_GUIDING_WARD, StarPulse(player, ally) * GUIDING_WARD_SHARE);
    }

    // Chain Guidance (Chain Heal) and Holy Passage (Holy Shock): their heal on another ally moves the star.
    void GuidingHeal(Player* player, Unit* ally, uint32 firstRank)
    {
        if ((firstRank == SPELL_CHAIN_HEAL && HasGuidingMarker(player, MOD_CHAIN_GUIDANCE))
            || (firstRank == SPELL_HOLY_SHOCK && HasGuidingMarker(player, MOD_HOLY_PASSAGE)))
            RedirectStar(player, ally, false);
    }

    // Its natural end on its bearer. Final Benediction: a heal of 2.5 pulses; Afterglow: 3.25 pulses over 8 sec;
    // Parting Grace: an absorb of 2 pulses (6 sec) and 30% speed (3 sec).
    void StarFarewell(Player* player, Unit* bearer)
    {
        float pulse = StarPulse(player, bearer);
        if (HasGuidingMarker(player, MOD_FINAL_BENEDICTION))
            Mend(player, bearer, SPELL_STAR_PULSE, pulse * BENEDICTION_SHARE);
        if (HasGuidingMarker(player, MOD_AFTERGLOW))
        {
            int32 tick = std::max<int32>(1, int32(pulse * AFTERGLOW_SHARE / float(AFTERGLOW_MS / 1000)));
            bearer->RemoveAurasDueToSpell(SPELL_AFTERGLOW, player->GetGUID());
            Report(player, SPELL_AFTERGLOW, 'H', tick);
            player->CastCustomSpell(bearer, SPELL_AFTERGLOW, &tick, nullptr, nullptr, true);
        }
        if (HasGuidingMarker(player, MOD_PARTING_GRACE))
        {
            int32 shield = std::max<int32>(1, int32(pulse * GRACE_SHARE));
            bearer->RemoveAurasDueToSpell(SPELL_PARTING_GRACE, player->GetGUID());
            Report(player, SPELL_PARTING_GRACE, 'A', shield);
            player->CastCustomSpell(bearer, SPELL_PARTING_GRACE, &shield, nullptr, nullptr, true);
            player->AddAura(SPELL_PARTING_GRACE_SPEED, bearer);
        }
        // Tidal Farewell: the Hero's Riptide on the bearer, 60% of spell power. Renewed Farewell: their Renew 4 sec
        // longer (6 sec an application), and 30%.
        if (HasGuidingMarker(player, MOD_TIDAL_FAREWELL)
            && bearer->GetAuraOfRankedSpell(SPELL_RIPTIDE, player->GetGUID()))
            Mend(player, bearer, SPELL_STAR_PULSE, pulse * TIDAL_FAREWELL_SHARE);
        if (HasGuidingMarker(player, MOD_RENEWED_FAREWELL))
            if (Aura* renew = bearer->GetAuraOfRankedSpell(SPELL_RENEW, player->GetGUID()))
            {
                ExtendAura(renew, RENEWED_EXTEND_MS, RENEWED_MOST_MS);
                Mend(player, bearer, SPELL_STAR_PULSE, pulse * RENEWED_FAREWELL_SHARE);
            }
    }

    // A pulse every 2 sec on its bearer while within reach (out of reach: paused). Guardian Star: 10% more a pulse
    // in a row on one bearer, up to 30%. Shared Light: the most injured other ally within 10 yards of the bearer
    // healed for 75% of it. Seeking Light: every third, the lowest ally within 15 yards for a pulse; a fallen
    // bearer's star moves on to them (else it fades).
    void StarTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        if (!state.StarBearer)
            return;
        uint32 now = Now();
        Unit* bearer = ObjectAccessor::GetUnit(*player, state.StarBearer);
        if (state.StarTravelling)
        {
            if (now < state.StarArriveAt)
                return;
            if (!bearer || !bearer->IsAlive() || !bearer->IsInMap(player))
            {
                ClearStar(player);
                return;
            }
            StarArrive(player, bearer);
        }
        if (now >= state.StarUntil)
        {
            if (CanBear(player, bearer))
                StarFarewell(player, bearer);
            ClearStar(player);
            return;
        }
        if (!bearer || !bearer->IsAlive() || !bearer->IsInMap(player))
        {
            if (bearer && bearer->IsInMap(player) && HasGuidingMarker(player, MOD_SEEKING_LIGHT))
                for (Unit* ally : InjuredNear(player, bearer, SEEKING_STAR_RANGE))
                    if (ally != bearer && CanBear(player, ally))
                    {
                        MoveStar(player, nullptr, ally);
                        player->CastSpell(ally, SPELL_STAR_FLIGHT, true);
                        return;
                    }
            ClearStar(player);
            return;
        }
        state.StarPulseTimer += diff;
        if (state.StarPulseTimer < STAR_PULSE_MS)
            return;
        state.StarPulseTimer -= STAR_PULSE_MS;
        if (!CanBear(player, bearer))
            return;
        float pulse = StarPulse(player, bearer);
        float heal = pulse;
        if (HasGuidingMarker(player, MOD_GUARDIAN_STAR))
        {
            heal *= 1.0f + GUARDIAN_STEP * float(state.StarFocus);
            state.StarFocus = std::min<uint8>(GUARDIAN_STEPS, state.StarFocus + 1);
        }
        Mend(player, bearer, SPELL_STAR_PULSE, heal);
        // Earthen Star: on the Hero's Earth Shield bearer, 25% of spell power to one injured ally within 10 yards.
        if (HasGuidingMarker(player, MOD_EARTHEN_STAR)
            && bearer->GetAuraOfRankedSpell(SPELL_EARTH_SHIELD, player->GetGUID()))
            for (Unit* ally : InjuredNear(player, bearer, EARTH_RANGE))
                if (ally != bearer && !ally->IsFullHealth())
                {
                    Mend(player, ally, SPELL_STAR_PULSE, pulse * EARTHEN_SHARE);
                    break;
                }
        // Moonlit Star: their Rejuvenation on the bearer 1 sec longer (4 sec an application).
        if (HasGuidingMarker(player, MOD_MOONLIT_STAR))
            if (Aura* rejuvenation = bearer->GetAuraOfRankedSpell(SPELL_REJUVENATION, player->GetGUID()))
                ExtendAura(rejuvenation, MOONLIT_EXTEND_MS, MOONLIT_MOST_MS);
        if (HasGuidingMarker(player, MOD_SHARED_LIGHT))
            for (Unit* ally : InjuredNear(player, bearer, SHARED_RANGE))
                if (ally != bearer && !ally->IsFullHealth())
                {
                    Mend(player, ally, SPELL_STAR_PULSE, heal * SHARED_SHARE);
                    break;
                }
        if (HasGuidingMarker(player, MOD_SEEKING_LIGHT) && ++state.StarPulses % SEEKING_EVERY == 0)
        {
            std::vector<Unit*> allies = InjuredNear(player, bearer, SEEKING_STAR_RANGE);
            if (!allies.empty())
                Mend(player, allies.front(), SPELL_STAR_PULSE, pulse);
        }
    }

    // ---- Elune's Guardian --------------------------------------------------------------------------------------

    bool HasOwlMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_OWL_MODIFIER_FIRST, modifier);
    }

    // The enemy with the Hero's Hunter's Mark (nullptr: none near).
    Unit* MarkedEnemy(Player* player)
    {
        for (Unit* enemy : EnemiesAround(player, player, MARK_SEARCH))
            if (enemy->GetAuraOfRankedSpell(SPELL_HUNTERS_MARK, player->GetGUID()))
                return enemy;
        return nullptr;
    }

    // The Hero's Moonfire on the unit, ms longer (at most `most` an application).
    void OwlMoonfire(Player* player, Unit* unit, int32 ms, int32 most)
    {
        if (Aura* moonfire = unit->GetAuraOfRankedSpell(SPELL_MOONFIRE, player->GetGUID()))
            ExtendAura(moonfire, ms, most);
    }

    // One owl strike from the sky on its victim. Its first (a summon's): on the Hero's Hunter's Mark target 20% more,
    // on their Moonfire target (Lunar Guidance) 15% more; Frostfeather, half of it as Frost; then Moonlit Prey, their
    // Moonfire longer, Winter's Blessing's Moonfrost. Silver Wings: half of it to up to 4 others within 6 yards; Lunar
    // Wound: as much again over 6 sec; Hunter's Pursuit: Exposed Prey on the marked target.
    void OwlStrike(Player* player, Unit* victim, float amount, bool first)
    {
        ObjectGuid me = player->GetGUID();
        bool marked = victim->GetAuraOfRankedSpell(SPELL_HUNTERS_MARK, me) != nullptr;
        bool moonfire = victim->GetAuraOfRankedSpell(SPELL_MOONFIRE, me) != nullptr;
        bool guidance = HasOwlMarker(player, MOD_LUNAR_GUIDANCE);
        if (first && marked)
            amount *= MARKED_OWL;
        if (first && moonfire && guidance)
            amount *= GUIDANCE;
        if (first && HasOwlMarker(player, MOD_FROSTFEATHER))
        {
            Strike(player, victim, SPELL_OWL_DIVE, amount / 2.0f);
            Strike(player, victim, SPELL_FROSTFEATHER, amount / 2.0f);
        }
        else
            Strike(player, victim, SPELL_OWL_DIVE, amount);
        if (HasOwlMarker(player, MOD_SILVER_WINGS) && first)
        {
            std::size_t count = SILVER_WINGS_TARGETS;
            for (Unit* enemy : EnemiesAround(player, victim, SILVER_WINGS_RADIUS))
                if (enemy != victim && count)
                {
                    Strike(player, enemy, SPELL_OWL_DIVE, amount * SILVER_WINGS_SHARE);
                    --count;
                }
        }
        if (!victim->IsAlive())
            return;
        State& state = StateOf(player);
        if (first)
        {
            Lasting(player, victim, SPELL_MOONLIT_PREY, MOONLIT_PREY_MS);
            if (moonfire)
                OwlMoonfire(player, victim, guidance ? GUIDANCE_MS : OWL_MOONFIRE_MS,
                    guidance ? GUIDANCE_MOST_MS : OWL_MOONFIRE_MOST_MS);
            if (HasOwlMarker(player, MOD_WINTERS_BLESSING) && victim->GetAuraOfRankedSpell(SPELL_FROSTBOLT, me))
                Lasting(player, victim, SPELL_MOONFROST, MOONFROST_MS);
        }
        if (HasOwlMarker(player, MOD_LUNAR_WOUND))
            AddDot(player, victim, SPELL_LUNAR_WOUND, amount * LUNAR_WOUND_SHARE, LUNAR_WOUND_MS);
        if (HasOwlMarker(player, MOD_HUNTERS_PURSUIT) && marked && Ready(state.ExposedAt, EXPOSED_EVERY_MS))
        {
            state.ExposedAt = Now();
            Lasting(player, victim, SPELL_EXPOSED_PREY, EXPOSED_MS);
        }
    }

    // A summon's own effects as it strikes: Swift Judgment, the next Starfire 10% faster; Guiding Wings, every
    // third owl, the next Starfire instant and 20% speed.
    void OwlSummoned(Player* player)
    {
        State& state = StateOf(player);
        if (HasOwlMarker(player, MOD_SWIFT_JUDGMENT))
            Renew(player, SPELL_SWIFT_JUDGMENT);
        if (HasOwlMarker(player, MOD_GUIDING_WINGS) && ++state.OwlCount >= GUIDING_EVERY)
        {
            state.OwlCount = 0;
            Renew(player, SPELL_GUIDING_WINGS);
            Renew(player, SPELL_GUIDING_WINGS_SPEED);
        }
    }

    // An owl's dive on its victim (gone: it fades). Lunar Flock: 3 strikes 1 sec apart, 40% each, the last making
    // the next Starfire 20% cheaper; Moonpiercer 40% more on its own victim.
    void OwlDive(Player* player, ObjectGuid victimGuid, float owl)
    {
        State& state = StateOf(player);
        if (state.Owls)
            --state.Owls;
        auto feathers = state.Feathers.find(victimGuid);
        if (feathers != state.Feathers.end())
        {
            if (feathers->second.first)
                --feathers->second.first;
            feathers->second.second = std::min(feathers->second.second, feathers->second.first);
            if (!feathers->second.first)
                state.Feathers.erase(feathers);
        }
        Unit* victim = ObjectAccessor::GetUnit(*player, victimGuid);
        if (!player->IsAlive() || !Valid(player, victim))
            return;
        if (HasOwlMarker(player, MOD_MOONPIERCER))
            owl *= MOONPIERCER;
        if (HasOwlMarker(player, MOD_LUNAR_FLOCK))
            for (uint32 i = 0; i < WATCHFUL_ATTACKS; ++i)
                player->m_Events.AddEventAtOffset([player, victimGuid, owl, i]()
                {
                    Unit* target = ObjectAccessor::GetUnit(*player, victimGuid);
                    if (!player->IsInWorld() || !player->IsAlive() || !Valid(player, target))
                        return;
                    OwlStrike(player, target, owl * WATCHFUL_SHARE, i == 0);
                    if (i + 1 == WATCHFUL_ATTACKS)
                        Renew(player, SPELL_WATCHFUL_FLIGHT);
                }, Milliseconds(i * WATCHFUL_SPACING_MS));
        else
            OwlStrike(player, victim, owl, true);
        OwlSummoned(player);
    }

    // Sentinel of the Hunt: its attack (of 3) on whoever wears the Hero's Hunter's Mark now; a newer Sentinel ends it.
    void SentinelAttack(Player* player, uint32 gen, float owl, uint32 attack)
    {
        State& state = StateOf(player);
        if (!player->IsInWorld() || !player->IsAlive() || state.SentinelGen != gen)
            return;
        if (Unit* marked = MarkedEnemy(player))
            OwlStrike(player, marked, owl * SENTINEL_SHARE, attack == 0);
        if (attack + 1 >= SENTINEL_ATTACKS)
        {
            state.SentinelUntil = 0;
            return;
        }
        player->m_Events.AddEventAtOffset([player, gen, owl, attack]()
        {
            SentinelAttack(player, gen, owl, attack + 1);
        }, Milliseconds(SENTINEL_EVERY_MS));
    }

    // A direct Starfire hit calls an owl (3 waiting at most): 37.5% of the Starfire's damage, diving 1 sec later
    // (Swift Judgment 0.1; Patient Vigil 3, 30% more). Sentinel of the Hunt, with a Hunter's Mark of the Hero's out:
    // the Sentinel instead, at once.
    void CallOwl(Player* player, Unit* victim, float starfire)
    {
        State& state = StateOf(player);
        if (starfire <= 0.0f)
            return;
        float owl = starfire * OWL_SHARE * (HasOwlMarker(player, MOD_PATIENT_VIGIL) ? PATIENT_VIGIL : 1.0f);
        if (HasOwlMarker(player, MOD_SENTINEL) && MarkedEnemy(player))
        {
            uint32 gen = ++state.SentinelGen;
            state.SentinelUntil = Now() + SENTINEL_EVERY_MS * SENTINEL_ATTACKS;
            OwlSummoned(player);
            SentinelAttack(player, gen, owl, 0);
            return;
        }
        if (state.Owls >= MAX_OWLS)
            return;
        ++state.Owls;
        ++state.Feathers[victim->GetGUID()].first;
        uint32 delay = HasOwlMarker(player, MOD_SWIFT_JUDGMENT) ? SWIFT_DELAY_MS
            : HasOwlMarker(player, MOD_PATIENT_VIGIL) ? VIGIL_DELAY_MS : OWL_DELAY_MS;
        ObjectGuid guid = victim->GetGUID();
        player->m_Events.AddEventAtOffset([player, guid, owl]()
        {
            if (player->IsInWorld())
                OwlDive(player, guid, owl);
        }, Milliseconds(delay));
    }

    // A direct Wrath on the Hero's Moonlit Prey: the owl's return strike, 20% of their spell power as Nature (on
    // their Hunter's Mark target, also one more within 8 yards for half; Sun and Moon, 15% to up to 4 within 6
    // yards); Twilight Hunt, the next Wrath there 25% more.
    void OwlReturn(Player* player, Unit* enemy)
    {
        ObjectGuid me = player->GetGUID();
        if (!enemy->HasAura(SPELL_MOONLIT_PREY, me))
            return;
        enemy->RemoveAurasDueToSpell(SPELL_MOONLIT_PREY, me);
        float strike = RunePower(player, SPELL_SCHOOL_MASK_NATURE, SPELL_STARFIRE) * RETURN_POWER;
        Strike(player, enemy, SPELL_OWL_RETURN, strike);
        if (enemy->GetAuraOfRankedSpell(SPELL_HUNTERS_MARK, me))
            for (Unit* other : EnemiesAround(player, enemy, RETURN_RANGE))
                if (other != enemy)
                {
                    Strike(player, other, SPELL_OWL_RETURN, strike * RETURN_SPLIT);
                    break;
                }
        if (HasOwlMarker(player, MOD_SUN_AND_MOON))
        {
            float sun = RunePower(player, SPELL_SCHOOL_MASK_NATURE, SPELL_STARFIRE) * SUN_AND_MOON_POWER;
            std::size_t count = SUN_AND_MOON_TARGETS;
            for (Unit* other : EnemiesAround(player, enemy, SUN_AND_MOON_RADIUS))
                if (other != enemy && count)
                {
                    Strike(player, other, SPELL_OWL_RETURN, sun);
                    --count;
                }
        }
        if (HasOwlMarker(player, MOD_TWILIGHT_HUNT) && enemy->IsAlive())
            Lasting(player, enemy, SPELL_TWILIGHT_HUNT, TWILIGHT_HUNT_MS);
    }

    // Arcane Shot, Aimed Shot or Steady Shot hitting: Command the Hunt (Arcane or Aimed Shot on the marked target,
    // an owl or the Sentinel out: 25% of spell power, 6 sec apart); Hunter's Pursuit's Exposed Prey spent.
    void OwlShot(Player* player, Unit* enemy, uint32 firstRank)
    {
        State& state = StateOf(player);
        ObjectGuid me = player->GetGUID();
        float power = RunePower(player, SPELL_SCHOOL_MASK_ARCANE, SPELL_STARFIRE);
        if (firstRank != SPELL_STEADY_SHOT && HasOwlMarker(player, MOD_COMMAND_THE_HUNT)
            && enemy->GetAuraOfRankedSpell(SPELL_HUNTERS_MARK, me) && (state.Owls || Now() < state.SentinelUntil)
            && Ready(state.CommandAt, COMMAND_EVERY_MS))
        {
            state.CommandAt = Now();
            Strike(player, enemy, SPELL_OWL_DIVE, power * COMMAND_POWER);
        }
        if (enemy->HasAura(SPELL_EXPOSED_PREY, me))
        {
            enemy->RemoveAurasDueToSpell(SPELL_EXPOSED_PREY, me);
            Strike(player, enemy, SPELL_OWL_DIVE, power * EXPOSED_POWER);
        }
    }

    // A direct Frostbolt hit: Frostfeather's burst on an owl's victim before it dives (one a summon); Winter's
    // Blessing's Moonfrost spent: the Moonfire there 2 sec longer (its Frostbolt 20% more: OwlFrostboltDamage).
    void OwlFrostbolt(Player* player, Unit* enemy)
    {
        State& state = StateOf(player);
        auto feathers = state.Feathers.find(enemy->GetGUID());
        if (HasOwlMarker(player, MOD_FROSTFEATHER) && feathers != state.Feathers.end()
            && feathers->second.second < feathers->second.first)
        {
            ++feathers->second.second;
            float burst = RunePower(player, SPELL_SCHOOL_MASK_FROST, SPELL_STARFIRE) * FEATHER_POWER;
            Strike(player, enemy, SPELL_FROSTFEATHER, burst);
            std::size_t count = FEATHER_TARGETS;
            for (Unit* other : EnemiesAround(player, enemy, FEATHER_RADIUS))
                if (other != enemy && count)
                {
                    Strike(player, other, SPELL_FROSTFEATHER, burst);
                    --count;
                }
        }
    }

    // Winter's Blessing: a Frostbolt on the Hero's Moonfrost spends it, 20% more, their Moonfire there 2 sec
    // longer. The bolt's damage, as it hits.
    float OwlFrostboltDamage(Player* player, Unit* enemy, float damage)
    {
        ObjectGuid me = player->GetGUID();
        if (!enemy->HasAura(SPELL_MOONFROST, me))
            return damage;
        enemy->RemoveAurasDueToSpell(SPELL_MOONFROST, me);
        bool guidance = HasOwlMarker(player, MOD_LUNAR_GUIDANCE);
        OwlMoonfire(player, enemy, OWL_MOONFIRE_MS, guidance ? GUIDANCE_MOST_MS : OWL_MOONFIRE_MOST_MS);
        return damage * MOONFROST;
    }

    // ---- Astral Convergence ------------------------------------------------------------------------------------

    bool HasConvergenceMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_CONVERGENCE_MODIFIER_FIRST, modifier);
    }

    // The Hero's spell power of a school, never less than their Starfire's own average damage.
    float ConvergencePower(Player* player, SpellSchoolMask school, bool withBonuses = true)
    {
        return RunePower(player, school, SPELL_STARFIRE, withBonuses);
    }

    // The nearest other enemy within range of the target (but, with fresh, not one already in the spell of the
    // Hero's; nullptr none).
    Unit* OtherEnemy(Player* player, Unit* target, float range, uint32 fresh = 0)
    {
        for (Unit* other : EnemiesAround(player, target, range))
            if (other != target && (!fresh || !other->GetAuraOfRankedSpell(fresh, player->GetGUID())))
                return other;
        return nullptr;
    }

    int32 MarkMs(Player* player)
    {
        return HasConvergenceMarker(player, MOD_LONG_TWILIGHT) ? LONG_TWILIGHT_MS : MARK_MS;
    }

    // A mark of the Hero's on an enemy (5 enemies marked at most: the longest marked loses theirs). Fresh Sunlight
    // may be empowered by a Frostbolt again.
    void Mark(Player* player, Unit* enemy, uint32 spellId)
    {
        State& state = StateOf(player);
        std::vector<ObjectGuid>& marked = state.Marked;
        marked.erase(std::remove_if(marked.begin(), marked.end(), [player](ObjectGuid guid)
        {
            Unit* unit = ObjectAccessor::GetUnit(*player, guid);
            return !unit || (!unit->HasAura(SPELL_SUNLIGHT, player->GetGUID())
                && !unit->HasAura(SPELL_MOONLIGHT, player->GetGUID()));
        }), marked.end());
        marked.erase(std::remove(marked.begin(), marked.end(), enemy->GetGUID()), marked.end());
        if (marked.size() >= MAX_MARKED)
        {
            if (Unit* oldest = ObjectAccessor::GetUnit(*player, marked.front()))
            {
                oldest->RemoveAurasDueToSpell(SPELL_SUNLIGHT, player->GetGUID());
                oldest->RemoveAurasDueToSpell(SPELL_MOONLIGHT, player->GetGUID());
            }
            marked.erase(marked.begin());
        }
        marked.push_back(enemy->GetGUID());
        if (spellId == SPELL_SUNLIGHT)
            state.FrozenSunlight.erase(enemy->GetGUID());
        Lasting(player, enemy, spellId, MarkMs(player));
    }

    // Astral Rupture's next tick on the enemy, now: its damage dealt and its time a tick shorter (no tick added).
    void AdvanceRupture(Player* player, Unit* enemy)
    {
        Aura* rupture = enemy->GetAura(SPELL_ASTRAL_RUPTURE, player->GetGUID());
        auto tick = dotTicks.find({ enemy->GetGUID(), SPELL_ASTRAL_RUPTURE, player->GetGUID() });
        if (!rupture || tick == dotTicks.end())
            return;
        int32 damage = std::max(1, tick->second);
        int32 period = DotPeriod(SPELL_ASTRAL_RUPTURE);
        player->CastCustomSpell(enemy, SPELL_ASTRAL_RUPTURE_TICK, &damage, nullptr, nullptr, true);
        if (rupture->GetDuration() <= period)
            enemy->RemoveAurasDueToSpell(SPELL_ASTRAL_RUPTURE, player->GetGUID());
        else
            rupture->SetDuration(rupture->GetDuration() - period);
    }

    // The flare on an enemy in the Hero's Moonlight (the mark spent): 40% of their Nature spell power (Solar Return
    // 60% more; scale: a Lightning Bolt's 75%), ending the sequence. Their Insect Swarm there 2 sec longer (6 sec an
    // application at most); Astral Rupture's next tick now; Tidal Flare (a bolt's): half to one more within 8 yards.
    void SolarFlare(Player* player, Unit* enemy, float scale, bool bolt)
    {
        enemy->RemoveAurasDueToSpell(SPELL_MOONLIGHT, player->GetGUID());
        float flare = ConvergencePower(player, SPELL_SCHOOL_MASK_NATURE) * FLARE_POWER * scale
            * (HasConvergenceMarker(player, MOD_SOLAR_RETURN) ? SOLAR_RETURN : 1.0f);
        Strike(player, enemy, SPELL_SOLAR_FLARE, flare);
        Trace(player, Acore::StringFormat("Solar Flare for {}", int32(flare)));
        if (bolt && HasConvergenceMarker(player, MOD_TIDAL_FLARE))
            if (Unit* other = OtherEnemy(player, enemy, ECHO_RANGE))
                Strike(player, other, SPELL_SOLAR_FLARE, flare * TIDAL_SHARE);
        if (Aura* swarm = enemy->GetAuraOfRankedSpell(SPELL_INSECT_SWARM, player->GetGUID()))
        {
            ExtendAura(swarm, SWARM_EXTEND_MS, SWARM_EXTEND_MOST_MS);
            if (HasConvergenceMarker(player, MOD_INSECT_RENEWAL))
                Renew(player, SPELL_INSECT_RENEWAL);
        }
        AdvanceRupture(player, enemy);
        if (HasConvergenceMarker(player, MOD_LUNAR_DOMINANCE))
            Renew(player, SPELL_LUNAR_DOMINANCE);
        if (HasConvergenceMarker(player, MOD_STEADY_ORBIT))
            Renew(player, SPELL_STEADY_ORBIT);
        State& state = StateOf(player);
        if (HasConvergenceMarker(player, MOD_EVENTIDE))
        {
            Position at = enemy->GetPosition();
            state.EventideAt = at;
            state.EventideMap = player->GetMapId();
            state.EventideUntil = Now() + EVENTIDE_MS;
            state.EventideTimer = 0;
            state.EventideTick = ConvergencePower(player, SPELL_SCHOOL_MASK_ARCANE) * EVENTIDE_POWER;
            PlaceDecals(player, SPELL_EVENTIDE, { at });
        }
        // Astral Ward: in Barkskin, an absorb of 40% of their spell power (12 sec apart).
        if (HasConvergenceMarker(player, MOD_ASTRAL_WARD) && player->HasAura(SPELL_BARKSKIN)
            && (!state.AstralWardAt || Now() - state.AstralWardAt >= ASTRAL_WARD_EVERY_MS))
        {
            state.AstralWardAt = Now();
            Ward(player, player, SPELL_ASTRAL_WARD,
                ConvergencePower(player, SPELL_SCHOOL_MASK_NATURE, false) * ASTRAL_WARD_POWER);
        }
    }

    // Wrath: on Moonlight, the flare; then (or else) it leaves Sunlight.
    void ConvergenceWrath(Player* player, Unit* enemy)
    {
        if (enemy->HasAura(SPELL_MOONLIGHT, player->GetGUID()))
            SolarFlare(player, enemy, 1.0f, false);
        if (enemy->IsAlive())
            Mark(player, enemy, SPELL_SUNLIGHT);
    }

    // A Lightning Bolt's direct hit on Moonlight: a flare of 75%, no Sunlight after (6 sec apart on each enemy).
    void ConvergenceBolt(Player* player, Unit* enemy)
    {
        if (!enemy->HasAura(SPELL_MOONLIGHT, player->GetGUID()))
            return;
        std::map<ObjectGuid, uint32>& flares = StateOf(player).BoltFlares;
        for (auto it = flares.begin(); it != flares.end();)
            it = Now() - it->second >= BOLT_FLARE_EVERY_MS ? flares.erase(it) : std::next(it);
        if (flares.contains(enemy->GetGUID()))
            return;
        flares[enemy->GetGUID()] = Now();
        SolarFlare(player, enemy, BOLT_FLARE, true);
    }

    // Frozen Eclipse: a Frostbolt's direct hit on the Hero's Sunlight empowers the next collision (once a Sunlight).
    void ConvergenceFrostbolt(Player* player, Unit* enemy)
    {
        State& state = StateOf(player);
        if (!HasConvergenceMarker(player, MOD_FROZEN_ECLIPSE) || !enemy->HasAura(SPELL_SUNLIGHT, player->GetGUID())
            || !state.FrozenSunlight.insert(enemy->GetGUID()).second)
            return;
        Renew(player, SPELL_FROZEN_ECLIPSE);
    }

    // Starfire on Sunlight: the collision (80% of the Hero's Arcane spell power; Perfect Alignment 40% more; Long
    // Twilight 5% more for each 2 sec the Sunlight was on, up to 20%; Frozen Eclipse 25% more, spent), then
    // Moonlight. Converging Skies: half to up to 4 more within 6 yards; Astral Rupture: 150% of it over 6 sec
    // instead; Crossed Horizons: 35% to up to 3 enemies within 12 yards in the Hero's Moonlight. On their Moonfire
    // target: 40% to one more within 8 yards, and Twin Moons spreads the Moonfire there (8 sec apart); on their
    // Flame Shock target, Flame Eclipse adds 25% of their Fire spell power.
    void ConvergenceStarfire(Player* player, Unit* enemy)
    {
        Aura* sunlight = enemy->GetAura(SPELL_SUNLIGHT, player->GetGUID());
        if (!sunlight)
            return;
        float bonus = 1.0f;
        if (HasConvergenceMarker(player, MOD_LONG_TWILIGHT))
        {
            int32 held = sunlight->GetMaxDuration() - sunlight->GetDuration();
            bonus += std::min(TWILIGHT_MOST, TWILIGHT_STEP * float(held / int32(TWILIGHT_STEP_MS)));
        }
        enemy->RemoveAurasDueToSpell(SPELL_SUNLIGHT, player->GetGUID());
        if (player->HasAura(SPELL_FROZEN_ECLIPSE))
        {
            player->RemoveAurasDueToSpell(SPELL_FROZEN_ECLIPSE);
            bonus *= FROZEN_ECLIPSE;
        }
        float collision = ConvergencePower(player, SPELL_SCHOOL_MASK_ARCANE) * COLLISION_POWER * bonus
            * (HasConvergenceMarker(player, MOD_PERFECT_ALIGNMENT) ? PERFECT_ALIGNMENT : 1.0f);
        Trace(player, Acore::StringFormat("Lunar Collision for {}", int32(collision)));
        std::vector<Unit*> crossed;
        if (HasConvergenceMarker(player, MOD_CROSSED_HORIZONS))
            for (Unit* other : EnemiesAround(player, enemy, CROSSED_RANGE))
                if (other != enemy && crossed.size() < CROSSED_TARGETS
                    && other->HasAura(SPELL_MOONLIGHT, player->GetGUID()))
                    crossed.push_back(other);
        if (HasConvergenceMarker(player, MOD_ASTRAL_RUPTURE))
        {
            enemy->CastSpell(enemy, SPELL_COLLISION_LOOK, true);
            AddDot(player, enemy, SPELL_ASTRAL_RUPTURE, collision * RUPTURE_SHARE, RUPTURE_MS);
        }
        else
            Strike(player, enemy, SPELL_LUNAR_COLLISION, collision);
        if (HasConvergenceMarker(player, MOD_CONVERGING_SKIES))
        {
            std::size_t count = CONVERGING_TARGETS;
            for (Unit* other : EnemiesAround(player, enemy, CONVERGING_RADIUS))
                if (other != enemy && count)
                {
                    Strike(player, other, SPELL_LUNAR_COLLISION, collision * CONVERGING_SHARE);
                    --count;
                }
        }
        for (Unit* other : crossed)
            Strike(player, other, SPELL_LUNAR_COLLISION, collision * CROSSED_SHARE);
        if (Aura* moonfire = enemy->GetAuraOfRankedSpell(SPELL_MOONFIRE, player->GetGUID()))
        {
            if (Unit* other = OtherEnemy(player, enemy, ECHO_RANGE))
                Strike(player, other, SPELL_LUNAR_COLLISION, collision * MOONFIRE_SHARE);
            State& state = StateOf(player);
            if (HasConvergenceMarker(player, MOD_TWIN_MOONS)
                && (!state.TwinMoonsAt || Now() - state.TwinMoonsAt >= TWIN_MOONS_EVERY_MS))
                if (Unit* other = OtherEnemy(player, enemy, ECHO_RANGE, SPELL_MOONFIRE))
                    if (Aura* spread = player->AddAura(moonfire->GetId(), other))
                    {
                        state.TwinMoonsAt = Now();
                        spread->SetMaxDuration(moonfire->GetDuration());
                        spread->SetDuration(moonfire->GetDuration());
                    }
        }
        if (HasConvergenceMarker(player, MOD_FLAME_ECLIPSE)
            && enemy->GetAuraOfRankedSpell(SPELL_FLAME_SHOCK, player->GetGUID()))
            Strike(player, enemy, SPELL_FLAME_ECLIPSE,
                ConvergencePower(player, SPELL_SCHOOL_MASK_FIRE) * FLAME_ECLIPSE_POWER);
        if (enemy->IsAlive())
            Mark(player, enemy, SPELL_MOONLIGHT);
    }

    // Eventide's field: 10% of the Hero's Arcane spell power every second to up to 5 enemies within 6 yards, 4 sec.
    void EventideTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        if (!state.EventideUntil)
            return;
        if (Now() >= state.EventideUntil || player->GetMapId() != state.EventideMap)
        {
            state.EventideUntil = 0;
            player->RemoveDynObject(SPELL_EVENTIDE);
            return;
        }
        state.EventideTimer += diff;
        if (state.EventideTimer < EVENTIDE_TICK_MS)
            return;
        state.EventideTimer = 0;
        for (Unit* enemy : EnemiesIn(player, Circle(state.EventideAt, EVENTIDE_RADIUS), EVENTIDE_TARGETS))
            Strike(player, enemy, SPELL_EVENTIDE_TICK, state.EventideTick);
    }

    // ---- Ursoc's Champion --------------------------------------------------------------------------------------

    bool HasUrsocMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_URSOC_MODIFIER_FIRST, modifier);
    }

    bool InBearForm(Player* player)
    {
        ShapeshiftForm form = player->GetShapeshiftForm();
        return form == FORM_BEAR || form == FORM_DIREBEAR;
    }

    AuraEffect* GuardEffect(Player* player)
    {
        Aura* guard = player->GetAura(SPELL_URSOCS_GUARD, player->GetGUID());
        return guard ? guard->GetEffect(EFFECT_0) : nullptr;
    }

    void EndGuard(Player* player)
    {
        State& state = StateOf(player);
        state.GuardShield = 0;
        state.GuardBlows = 0;
        state.GuardSwiped = state.GuardStone = state.GuardThunder = state.GuardShocked = false;
        player->RemoveAurasDueToSpell(SPELL_URSOCS_GUARD);
    }

    // The guard ms longer (Swipe's second, Earthshock Roar's).
    void LengthenGuard(Player* player, int32 ms)
    {
        if (Aura* guard = player->GetAura(SPELL_URSOCS_GUARD, player->GetGUID()))
        {
            guard->SetMaxDuration(guard->GetMaxDuration() + ms);
            guard->SetDuration(guard->GetDuration() + ms);
        }
    }

    // Barkskin in Bear Form: Ursoc's Guard, an absorb of 30% of maximum health (Earthen Hide 20% more) for 6 sec
    // (Long Guard 8); Barkskin back 30 sec sooner. Shielding Roots, in the Hero's Rejuvenation: 3% more for 4 sec.
    void RaiseGuard(Player* player)
    {
        State& state = StateOf(player);
        EndGuard(player);
        int32 shield = std::max<int32>(1, int32(player->GetMaxHealth() * GUARD_PCT / 100.0f
            * (HasUrsocMarker(player, MOD_EARTHEN_HIDE) ? EARTHEN_HIDE : 1.0f)));
        int32 ms = GUARD_MS + (HasUrsocMarker(player, MOD_LONG_GUARD) ? LONG_GUARD_MS : 0);
        Report(player, SPELL_URSOCS_GUARD, 'A', shield);
        player->CastCustomSpell(player, SPELL_URSOCS_GUARD, &shield, nullptr, nullptr, true);
        if (Aura* guard = player->GetAura(SPELL_URSOCS_GUARD, player->GetGUID()))
        {
            guard->SetMaxDuration(ms);
            guard->SetDuration(ms);
        }
        state.GuardShield = shield;
        state.GuardBlows = 0;
        if (HasUrsocMarker(player, MOD_SHIELDING_ROOTS)
            && player->GetAuraOfRankedSpell(SPELL_REJUVENATION, player->GetGUID()))
        {
            int32 roots = std::max<int32>(1, int32(player->CountPctFromMaxHealth(int32(ROOTS_PCT))));
            player->RemoveAurasDueToSpell(SPELL_SHIELDING_ROOTS);
            Report(player, SPELL_SHIELDING_ROOTS, 'A', roots);
            player->CastCustomSpell(player, SPELL_SHIELDING_ROOTS, &roots, nullptr, nullptr, true);
        }
    }

    // The impact (Maul in the guard; Unbroken Champion: the guard running out, 125%): 75% of attack power and all
    // the guard absorbed (that part at most 250% of attack power; Crushing Champion 30% more, Stoneguard 20% more)
    // in a 10-yard cone in front (up to 5; Packbreaker 8). Earth Shock's empowering: 15% more, half of it Nature.
    // On the Hero's Mangle mark: 20% of attack power to up to 4 near it, and threat. Thunderbear's burst; Rending
    // Maul's bleed on their Rake target; Defiant Roar's threat; Restoring Force, Lifebloom Hide; Bear's Momentum.
    void Release(Player* player, int32 left, float scale)
    {
        State& state = StateOf(player);
        ObjectGuid me = player->GetGUID();
        float ap = player->GetTotalAttackPowerValue(BASE_ATTACK);
        float absorbed = float(std::max(0, state.GuardShield - std::max(0, left)));
        float blow = (ap * URSOC_AP + std::min(absorbed * ABSORBED_SHARE, ap * ABSORBED_MOST_AP)) * scale;
        if (HasUrsocMarker(player, MOD_CRUSHING_CHAMPION))
            blow *= CRUSHING_CHAMPION;
        if (state.GuardStone)
            blow *= STONEGUARD;
        bool shocked = player->HasAura(SPELL_EARTHEN_FURY);
        if (shocked)
        {
            player->RemoveAurasDueToSpell(SPELL_EARTHEN_FURY);
            blow *= EARTHEN_FURY;
        }
        Shape cone;
        cone.Cone = true;
        cone.From = player->GetPosition();
        cone.Facing = player->GetOrientation();
        cone.Radius = URSOC_REACH;
        cone.Arc = URSOC_ARC;
        std::vector<Unit*> struck = EnemiesIn(player, cone,
            HasUrsocMarker(player, MOD_PACKBREAKER) ? PACKBREAKER_TARGETS : URSOC_TARGETS);
        player->CastSpell(player, SPELL_URSOC_BLOW_LOOK, true);
        float dealt = 0.0f;
        for (Unit* enemy : struck)
        {
            if (shocked)
            {
                Strike(player, enemy, SPELL_URSOC_BLOW, blow / 2.0f);
                Strike(player, enemy, SPELL_URSOC_NATURE, blow / 2.0f);
            }
            else
                Strike(player, enemy, SPELL_URSOC_BLOW, blow);
            dealt += blow;
            if (state.GuardThunder)
                Strike(player, enemy, SPELL_THUNDERBEAR,
                    ap * THUNDERBEAR_AP * SchoolBonus(player, SPELL_SCHOOL_MASK_NATURE));
            if (!enemy->IsAlive())
                continue;
            if (HasUrsocMarker(player, MOD_DEFIANT_ROAR))
                enemy->GetThreatMgr().AddThreat(player, blow * DEFIANT_THREAT);
            if (HasUrsocMarker(player, MOD_RENDING_MAUL) && enemy->GetAuraOfRankedSpell(SPELL_RAKE, me))
                AddDot(player, enemy, SPELL_RENDING_MAUL, blow * RENDING_SHARE, RENDING_MS);
            if (enemy->HasAura(SPELL_URSOCS_MARK, me))
            {
                enemy->RemoveAurasDueToSpell(SPELL_URSOCS_MARK, me);
                std::size_t count = MARK_TARGETS;
                for (Unit* other : EnemiesAround(player, enemy, MARK_RADIUS))
                    if (other != enemy && count)
                    {
                        Strike(player, other, SPELL_URSOC_BLOW, ap * MARK_AP);
                        other->GetThreatMgr().AddThreat(player, ap * MARK_AP);
                        --count;
                    }
            }
        }
        float cap = float(player->CountPctFromMaxHealth(int32(RESTORING_CAP_PCT)));
        float heal = 0.0f;
        if (HasUrsocMarker(player, MOD_RESTORING_FORCE))
            heal += std::min(dealt * RESTORING_SHARE, cap);
        if (HasUrsocMarker(player, MOD_LIFEBLOOM_HIDE) && player->GetAuraOfRankedSpell(SPELL_LIFEBLOOM, me))
            heal += std::min(RunePower(player, SPELL_SCHOOL_MASK_NATURE, SPELL_MAUL, false) * LIFEBLOOM_POWER, cap);
        if (heal >= 1.0f)
        {
            int32 points = int32(heal);
            Report(player, SPELL_URSOC_MEND, 'H', points);
            player->CastCustomSpell(player, SPELL_URSOC_MEND, &points, nullptr, nullptr, true);
        }
        if (HasUrsocMarker(player, MOD_BEARS_MOMENTUM))
            Renew(player, SPELL_BEARS_MOMENTUM);
        EndGuard(player);
    }

    // Maul in the guard: the impact (Unbroken Champion: no, it waits for the guard's end).
    void MaulRelease(Player* player)
    {
        AuraEffect* guard = GuardEffect(player);
        if (!guard || HasUrsocMarker(player, MOD_UNBROKEN_CHAMPION))
            return;
        Release(player, guard->GetAmount(), 1.0f);
    }

    // Abilities in the guard: Mangle marks its target (6 sec; Ursoc's Challenge: it deals 15% less for 3 sec);
    // Swipe lengthens the guard 1 sec (once; not with Long Guard); Earth Shock empowers the impact (Earthshock Roar:
    // the guard 1 sec longer, once); Thunder Clap, Thunderbear's burst. Earth Shield healing the Hero: Stoneguard.
    void UrsocAbility(Player* player, Unit* target, uint32 firstRank)
    {
        State& state = StateOf(player);
        if (!GuardEffect(player))
            return;
        if (firstRank == SPELL_MANGLE_BEAR && target)
        {
            Lasting(player, target, SPELL_URSOCS_MARK, URSOCS_MARK_MS);
            if (HasUrsocMarker(player, MOD_URSOCS_CHALLENGE))
                Lasting(player, target, SPELL_URSOCS_CHALLENGE, CHALLENGE_MS);
        }
        if (firstRank == SPELL_SWIPE_BEAR && !state.GuardSwiped && !HasUrsocMarker(player, MOD_LONG_GUARD))
        {
            state.GuardSwiped = true;
            LengthenGuard(player, SWIPE_GUARD_MS);
        }
        if (firstRank == TankRunes::SPELL_EARTH_SHOCK)
        {
            Renew(player, SPELL_EARTHEN_FURY);
            if (HasUrsocMarker(player, MOD_EARTHSHOCK_ROAR) && !state.GuardShocked)
            {
                state.GuardShocked = true;
                LengthenGuard(player, SWIPE_GUARD_MS);
            }
        }
        if (firstRank == SPELL_THUNDER_CLAP && HasUrsocMarker(player, MOD_THUNDERBEAR))
            state.GuardThunder = true;
        if (firstRank == SPELL_EARTH_SHIELD_HEAL && HasUrsocMarker(player, MOD_STONEGUARD))
            state.GuardStone = true;
    }

    // The guard out: it ends with Bear Form. Unbroken Champion: its end releases the impact, 125%.
    void GuardTick(Player* player)
    {
        State& state = StateOf(player);
        if (!state.GuardShield)
            return;
        AuraEffect* guard = GuardEffect(player);
        if (!InBearForm(player))
        {
            EndGuard(player);
            return;
        }
        bool ending = !guard || guard->GetBase()->GetDuration() <= GUARD_END_MS;
        if (!ending)
            return;
        if (HasUrsocMarker(player, MOD_UNBROKEN_CHAMPION))
            Release(player, guard ? guard->GetAmount() : 0, UNBROKEN_CHAMPION);
        else if (!guard)
            EndGuard(player);
    }

    void ClearDruids(Player* player)
    {
        State& state = StateOf(player);
        state.Owls = 0;
        state.OwlCount = 0;
        ++state.SentinelGen;                        // ends a Sentinel's attacks
        state.SentinelUntil = 0;
        state.Feathers.clear();
        for (uint32 spell : { SPELL_SWIFT_JUDGMENT, SPELL_WATCHFUL_FLIGHT, SPELL_GUIDING_WINGS,
                              SPELL_GUIDING_WINGS_SPEED, SPELL_STEADY_ORBIT, SPELL_LUNAR_DOMINANCE,
                              SPELL_EARTHEN_FURY, SPELL_BEARS_MOMENTUM, SPELL_SHIELDING_ROOTS,
                              SPELL_FROZEN_ECLIPSE, SPELL_INSECT_RENEWAL,
                              SPELL_ASTRAL_WARD })
            player->RemoveAurasDueToSpell(spell);
        for (ObjectGuid guid : state.Marked)
            if (Unit* unit = ObjectAccessor::GetUnit(*player, guid))
            {
                unit->RemoveAurasDueToSpell(SPELL_SUNLIGHT, player->GetGUID());
                unit->RemoveAurasDueToSpell(SPELL_MOONLIGHT, player->GetGUID());
            }
        state.Marked.clear();
        state.BoltFlares.clear();
        state.FrozenSunlight.clear();
        state.EventideUntil = 0;
        player->RemoveDynObject(SPELL_EVENTIDE);
        EndGuard(player);
    }


    // ---- The Duelist ------------------------------------------------------------------------------------------

    bool HasDuelistMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_DUELIST_MODIFIER_FIRST, modifier);
    }

    // A timed buff on the Hero (an earlier one replaced).
    void RenewFor(Player* player, uint32 spellId, int32 ms)
    {
        player->RemoveAurasDueToSpell(spellId);
        if (Aura* aura = player->AddAura(spellId, player))
        {
            aura->SetMaxDuration(ms);
            aura->SetDuration(ms);
        }
    }

    // A Sabre Slash's hit on its target, before it lands: Loaded Steel's 15% more (Powder and Steel 30%), Marked
    // Shot's 25% (both spent; Loaded Steel's second enemy after it lands).
    float SabreBonus(Player* player, Unit* /*target*/)
    {
        float bonus = 1.0f;
        State& state = StateOf(player);
        state.SabreSteel = player->HasAura(SPELL_LOADED_STEEL);
        if (state.SabreSteel)
        {
            player->RemoveAurasDueToSpell(SPELL_LOADED_STEEL);
            bonus *= HasDuelistMarker(player, MOD_POWDER_AND_STEEL) ? POWDER_AND_STEEL : LOADED_STEEL;
        }
        if (player->HasAura(SPELL_MARKED_SHOT))
        {
            player->RemoveAurasDueToSpell(SPELL_MARKED_SHOT);
            bonus *= MARKED_SHOT;
        }
        return bonus;
    }

    // A Sabre Slash that hit (damage, its target's): every third (Quick Draw: second) readies Opening Shot; Loaded
    // Steel's, one more enemy within 5 yards for 40%.
    void SabreSlash(Player* player, Unit* target, float damage)
    {
        State& state = StateOf(player);
        if (++state.SabreHits >= (HasDuelistMarker(player, MOD_QUICK_DRAW) ? QUICK_DRAW_EVERY : OPENING_EVERY))
        {
            state.SabreHits = 0;
            RenewFor(player, SPELL_OPENING_SHOT, OPENING_MS);
            Trace(player, "Opening Shot ready");
        }
        if (state.SabreSteel)
        {
            state.SabreSteel = false;
            for (Unit* enemy : EnemiesAround(player, target, SWEEPING_RADIUS))
                if (enemy != target)
                {
                    Strike(player, enemy, SPELL_SABRE_STRIKE, damage * LOADED_STEEL_SPLASH);
                    break;
                }
        }
    }

    // A Fire Blast hit: Loaded Powder, the next Pistol Shot within 8 sec explodes. An Arcane Shot hit: Arcane
    // Powder's 25% more.
    void DuelistStrike(Player* player, uint32 firstRank)
    {
        if (firstRank == SPELL_FIRE_BLAST)
            Renew(player, SPELL_LOADED_POWDER);
        if (firstRank == SPELL_ARCANE_SHOT && HasDuelistMarker(player, MOD_ARCANE_POWDER))
            Renew(player, SPELL_ARCANE_POWDER);
    }

    // Pistol Shot on its target: 60% of attack power (Heavy Caliber 40% more; Dead Aim, Arcane Powder) and a combo
    // point; Gunslinger, twice at 65%; Scattershot, a 10-yard cone, up to 3 more for half. Then Loaded Steel, the
    // powder's explosion, Storm Pistol, Marked Shot, Frozen Powder, Cut and Run, Efficient Duelist, Healing Powder.
    void FirePistol(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        ObjectGuid me = player->GetGUID();
        player->RemoveAurasDueToSpell(SPELL_OPENING_SHOT);
        float ap = player->GetTotalAttackPowerValue(BASE_ATTACK);
        float shot = ap * PISTOL_AP * (HasDuelistMarker(player, MOD_HEAVY_CALIBER) ? HEAVY_CALIBER : 1.0f);
        if (Aura* aim = player->GetAura(SPELL_DEAD_AIM))
        {
            shot *= 1.0f + 0.05f * float(aim->GetStackAmount());
            player->RemoveAurasDueToSpell(SPELL_DEAD_AIM);
        }
        if (player->HasAura(SPELL_ARCANE_POWDER))
        {
            player->RemoveAurasDueToSpell(SPELL_ARCANE_POWDER);
            shot *= ARCANE_POWDER;
        }
        if (HasDuelistMarker(player, MOD_GUNSLINGER))
        {
            Strike(player, target, SPELL_PISTOL_DAMAGE, shot * GUNSLINGER_SHARE);
            ObjectGuid guid = target->GetGUID();
            float second = shot * GUNSLINGER_SHARE;
            player->m_Events.AddEventAtOffset([player, guid, second]()
            {
                Unit* at = ObjectAccessor::GetUnit(*player, guid);
                if (player->IsInWorld() && Valid(player, at))
                    Strike(player, at, SPELL_PISTOL_DAMAGE, second);
            }, 250ms);
        }
        else
            Strike(player, target, SPELL_PISTOL_DAMAGE, shot);
        Trace(player, Acore::StringFormat("Pistol Shot for {}", int32(shot)));
        if (HasDuelistMarker(player, MOD_SCATTERSHOT))
        {
            Shape cone;
            cone.Cone = true;
            cone.From = player->GetPosition();
            cone.Facing = player->GetAbsoluteAngle(target);
            cone.Radius = SCATTER_REACH + player->GetExactDist2d(target);
            cone.Arc = SCATTER_ARC;
            std::size_t count = SCATTER_TARGETS;
            for (Unit* enemy : EnemiesIn(player, cone, SCATTER_TARGETS + 1))
                if (enemy != target && count)
                {
                    Strike(player, enemy, SPELL_PISTOL_DAMAGE, shot * SCATTER_SHARE);
                    --count;
                }
        }
        if (Knows(player, SPELL_SINISTER_STRIKE) || player->HasSpell(SPELL_SABRE_SLASH))
            Renew(player, SPELL_LOADED_STEEL);
        if (player->HasAura(SPELL_LOADED_POWDER))
        {
            player->RemoveAurasDueToSpell(SPELL_LOADED_POWDER);
            bool flame = HasDuelistMarker(player, MOD_FLAME_PISTOL);
            bool loaded = HasDuelistMarker(player, MOD_LOADED_POWDER);
            float blast = (ap * POWDER_AP + float(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE)) * POWDER_SP)
                * player->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, SPELL_SCHOOL_MASK_FIRE)
                * (flame ? FLAME_PISTOL : loaded ? LOADED_POWDER : 1.0f);
            for (Unit* enemy : EnemiesIn(player, Circle(target->GetPosition(), POWDER_RADIUS),
                flame || loaded ? LOADED_POWDER_TARGETS : POWDER_TARGETS))
                Strike(player, enemy, SPELL_POWDER_BLAST, blast);
        }
        if (HasDuelistMarker(player, MOD_STORM_PISTOL) && player->GetAuraOfRankedSpell(SPELL_LIGHTNING_SHIELD, me))
            Strike(player, target, SPELL_STORM_PISTOL,
                RunePower(player, SPELL_SCHOOL_MASK_NATURE, SPELL_SINISTER_STRIKE) * STORM_PISTOL_POWER);
        if (HasDuelistMarker(player, MOD_MARKED_SHOT) && target->GetAuraOfRankedSpell(SPELL_HUNTERS_MARK, me))
            Renew(player, SPELL_MARKED_SHOT);
        if (HasDuelistMarker(player, MOD_FROZEN_POWDER) && target->HasAura(SPELL_FROST_FEVER, me)
            && Ready(state.FrozenPowderAt, FROZEN_POWDER_EVERY_MS))
        {
            state.FrozenPowderAt = Now();
            float frost = ap * FROZEN_POWDER_AP
                * player->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, SPELL_SCHOOL_MASK_FROST);
            for (Unit* enemy : EnemiesIn(player, Circle(target->GetPosition(), FROZEN_POWDER_RADIUS),
                FROZEN_POWDER_TARGETS))
                Strike(player, enemy, SPELL_FROZEN_POWDER, frost);
        }
        if (HasDuelistMarker(player, MOD_CUT_AND_RUN))
            Renew(player, SPELL_CUT_AND_RUN);
        if (HasDuelistMarker(player, MOD_EFFICIENT_DUELIST) && Ready(state.EfficientAt, EFFICIENT_EVERY_MS))
        {
            state.EfficientAt = Now();
            player->ModifyPower(POWER_ENERGY, PISTOL_COST);
        }
        if (HasDuelistMarker(player, MOD_HEALING_POWDER) && Ready(state.HealingPowderAt, HEALING_POWDER_EVERY_MS)
            && player->GetAuraOfRankedSpell(SPELL_RENEW, me))
        {
            state.HealingPowderAt = Now();
            int32 heal = std::max<int32>(1, int32(RunePower(player, SPELL_SCHOOL_MASK_HOLY, SPELL_SINISTER_STRIKE,
                false) * HEALING_POWDER_POWER));
            Report(player, SPELL_HEALING_POWDER, 'H', heal);
            player->CastCustomSpell(player, SPELL_HEALING_POWDER, &heal, nullptr, nullptr, true);
        }
        if (target->IsAlive())
            player->AddComboPoints(target, 1);
    }

    // Eviscerate with Opening Shot ready (as it is cast): the next Pistol Shot 10% more a combo point spent, 50% at
    // most (Between the Eyes 15%, 75%); Opening Shot stays.
    void DuelistEviscerate(Player* player, uint8 points)
    {
        bool eyes = HasDuelistMarker(player, MOD_BETWEEN_THE_EYES);
        float bonus = std::min(eyes ? EYES_MOST : EVISCERATE_MOST,
            (eyes ? EYES_STEP : EVISCERATE_STEP) * float(points));
        RenewFor(player, SPELL_DEAD_AIM, OPENING_MS);
        if (Aura* aim = player->GetAura(SPELL_DEAD_AIM))
            aim->SetStackAmount(uint8(std::max(1, int32(bonus * 20.0f + 0.5f))));
    }

    // The highest rank of Sinister Strike the Hero knows: Sabre Slash's (rank 1's copy) bonus on top of its own.
    int32 SabreRankBonus(Player* player)
    {
        SpellInfo const* first = sSpellMgr->GetSpellInfo(SPELL_SINISTER_STRIKE);
        if (!first)
            return 0;
        int32 base = first->Effects[EFFECT_0].CalcValue(player);
        for (SpellInfo const* rank = first->GetLastRankSpell(); rank; rank = rank->GetPrevRankSpell())
            if (player->HasSpell(rank->Id))
                return std::max(0, rank->Effects[EFFECT_0].CalcValue(player) - base);
        return 0;
    }

    // Broadside: 3 blasts at its target (30% of attack power plus 5% a combo point, to it and up to 7 within 8 yards
    // each),
    // the last a great explosion.
    void FireBroadside(Player* player, Unit* target, uint8 points)
    {
        float blast = player->GetTotalAttackPowerValue(BASE_ATTACK) * (BROADSIDE_AP + BROADSIDE_AP_PER_POINT * points)
            * SchoolBonus(player, SPELL_SCHOOL_MASK_FIRE);
        uint32 blasts = BROADSIDE_BLASTS;
        Trace(player, Acore::StringFormat("Broadside: {} combo points, {} a blast", points, int32(blast)));
        ObjectGuid guid = target->GetGUID();
        for (uint32 i = 0; i < blasts; ++i)
            player->m_Events.AddEventAtOffset([player, guid, blast, i, blasts]()
            {
                Unit* at = ObjectAccessor::GetUnit(*player, guid);
                if (!player->IsInWorld() || !player->IsAlive() || !at || !at->IsInMap(player))
                    return;
                std::vector<Unit*> struck;
                if (Valid(player, at))
                    struck.push_back(at);
                for (Unit* enemy : EnemiesAround(player, at, BROADSIDE_RADIUS))
                    if (enemy != at && struck.size() < BROADSIDE_TARGETS)
                        struck.push_back(enemy);
                for (Unit* enemy : struck)
                    Strike(player, enemy, SPELL_BROADSIDE_BLAST, blast);
                if (i + 1 == blasts)
                    at->CastSpell(at, SPELL_BROADSIDE_LOOK, true);
            }, Milliseconds(i * BROADSIDE_SPACING_MS));
    }

    // The Duelist teaches Sabre Slash, Pistol Shot and Broadside while it is bound, each in place of an ability the
    // Hero knows (Sinister Strike, Arcane Shot, Rapid Fire: their buttons become the rune's); unbound, they go and
    // the buttons are the Hero's own again.
    // The Dawnbound Crusader teaches Dawn Vow in Beacon of Light's place; unbound, the vow is let go.
    void TeachDawn(Player* player, bool bound)
    {
        LegendaryRunes::ReplaceAbility(player, SPELL_DAWN_VOW, { SPELL_BEACON_OF_LIGHT }, bound);
        if (!bound)
            ClearVow(player);
    }

    void TeachDuelist(Player* player, bool bound)
    {
        LegendaryRunes::ReplaceAbility(player, SPELL_SABRE_SLASH, { SPELL_SINISTER_STRIKE }, bound);
        LegendaryRunes::ReplaceAbility(player, SPELL_PISTOL_SHOT, { SPELL_ARCANE_SHOT }, bound);
        LegendaryRunes::ReplaceAbility(player, SPELL_BROADSIDE, { SPELL_RAPID_FIRE }, bound);
    }

    // ---- The Stormreaver --------------------------------------------------------------------------------------

    bool HasStormreaverMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_STORMREAVER_MODIFIER_FIRST, modifier);
    }

    // Stormstrike: the Stormfront for 6 sec (a refresh keeps its pulse timer).
    void RaiseStormfront(Player* player)
    {
        if (Aura* storm = player->GetAura(SPELL_STORMFRONT, player->GetGUID()))
        {
            storm->SetMaxDuration(STORMFRONT_MS);
            storm->SetDuration(STORMFRONT_MS);
            return;
        }
        StateOf(player).StormfrontTimer = 0;
        RenewFor(player, SPELL_STORMFRONT, STORMFRONT_MS);
    }

    // A pulse: 25% of attack power to up to 5 enemies within 8 yards (Forked Cataclysm 8 within 12; Rolling Thunder
    // 70% each, every second; Eye of the Tempest: 120% more on a lone enemy).
    void StormfrontTick(Player* player, uint32 diff)
    {
        State& state = StateOf(player);
        if (!player->HasAura(SPELL_STORMFRONT, player->GetGUID()))
        {
            state.StormfrontTimer = 0;
            return;
        }
        bool rolling = HasStormreaverMarker(player, MOD_STORM_ROLLING);
        uint32 every = rolling ? ROLLING_PULSE_MS : STORM_PULSE_MS;
        state.StormfrontTimer += diff;
        if (state.StormfrontTimer < every)
            return;
        state.StormfrontTimer -= every;
        bool forked = HasStormreaverMarker(player, MOD_FORKED_CATACLYSM);
        std::vector<Unit*> struck = EnemiesIn(player, Circle(player->GetPosition(),
            forked ? FORKED_RADIUS : STORM_RADIUS), forked ? FORKED_TARGETS : STORM_TARGETS);
        float pulse = player->GetTotalAttackPowerValue(BASE_ATTACK) * STORM_PULSE_AP * (rolling ? ROLLING_SHARE : 1.0f)
            * SchoolBonus(player, SPELL_SCHOOL_MASK_NATURE);
        if (struck.size() == 1 && HasStormreaverMarker(player, MOD_EYE_OF_THE_TEMPEST))
            pulse *= EYE_OF_THE_TEMPEST;
        if (player->HasAura(SPELL_STORM_SURGE))
        {
            player->RemoveAurasDueToSpell(SPELL_STORM_SURGE);
            pulse *= STORM_SURGE;
        }
        bool hunter = HasStormreaverMarker(player, MOD_STORMHUNTER);
        for (Unit* enemy : struck)
        {
            bool marked = hunter && enemy->GetAuraOfRankedSpell(SPELL_HUNTERS_MARK, player->GetGUID());
            Strike(player, enemy, SPELL_STORMFRONT_PULSE, pulse * (marked ? STORMHUNTER : 1.0f));
            if (marked)
                for (Unit* other : EnemiesAround(player, enemy, STORM_ECHO_RANGE))
                    if (other != enemy)
                    {
                        Strike(player, other, SPELL_STORMFRONT_PULSE, pulse * STORMHUNTER_SHARE);
                        break;
                    }
        }
    }

    // An arc of the rune's lightning from one enemy to another (Chain Lightning's bolt between them).
    void StormArc(Player* player, Unit* from, Unit* to, uint32 spellId, float amount)
    {
        from->CastSpell(to, SPELL_STORMBOUND_ARC_VISUAL, true);
        Strike(player, to, spellId, amount);
    }

    // Lava Lash in the Stormfront: Thunderlash. 60% of attack power to its target (Thunderhead 120%, no arcs;
    // White-Hot Conductor a burn of 100% over 6 sec instead), arcs of 30% to 2 more within 8 yards (Chainbreaker 4).
    void Thunderlash(Player* player, Unit* target)
    {
        float ap = player->GetTotalAttackPowerValue(BASE_ATTACK) * SchoolBonus(player, SPELL_SCHOOL_MASK_NATURE);
        // On the Hero's Flame Shock target: 2 sec longer (6 sec an application); Flame Conductor spreads it.
        if (Aura* shock = target->GetAuraOfRankedSpell(SPELL_FLAME_SHOCK, player->GetGUID()))
        {
            ExtendAura(shock, STORM_FS_EXTEND_MS, STORM_FS_MOST_MS);
            State& state = StateOf(player);
            if (HasStormreaverMarker(player, MOD_FLAME_CONDUCTOR)
                && Ready(state.FlameConductorAt, FLAME_CONDUCTOR_EVERY_MS))
                for (Unit* other : EnemiesAround(player, target, STORM_ECHO_RANGE))
                    if (other != target && !other->GetAuraOfRankedSpell(SPELL_FLAME_SHOCK, player->GetGUID()))
                    {
                        if (Aura* spread = player->AddAura(shock->GetId(), other))
                        {
                            state.FlameConductorAt = Now();
                            spread->SetMaxDuration(shock->GetDuration());
                            spread->SetDuration(shock->GetDuration());
                        }
                        break;
                    }
        }
        bool thunderhead = HasStormreaverMarker(player, MOD_THUNDERHEAD);
        if (HasStormreaverMarker(player, MOD_WHITE_HOT_CONDUCTOR))
            AddDot(player, target, SPELL_WHITE_HOT_BURN, ap * WHITE_HOT_AP, WHITE_HOT_MS);
        else
            Strike(player, target, SPELL_THUNDERLASH, ap * (thunderhead ? THUNDERHEAD_AP : THUNDERLASH_AP));
        if (thunderhead)
            return;
        std::size_t count = HasStormreaverMarker(player, MOD_CHAINBREAKER) ? CHAINBREAKER_ARCS : THUNDERLASH_ARCS;
        for (Unit* enemy : EnemiesAround(player, target, THUNDERLASH_RANGE))
            if (enemy != target && count)
            {
                StormArc(player, target, enemy, SPELL_THUNDERLASH_ARC, ap * ARC_AP);
                --count;
            }
    }

    // A melee ability's hit in the Stormfront, with Lightning Shield up: a discharge at its target, once a second
    // (no charge): the shield's rank's damage, a third of spell power and 20% of attack power (Overcharged Core 50%
    // more; Violent Grounding: half to up to 3 more within 8 yards; Ride the Lightning: 30% speed for 3 sec and Lava
    // Lash 1 sec sooner).
    void StormDischarge(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        AuraEffect* shield = Shield(player);
        if (!shield || !player->HasAura(SPELL_STORMFRONT, player->GetGUID()) || Now() - state.LastDischarge
            < DISCHARGE_EVERY_MS)
            return;
        state.LastDischarge = Now();
        SpellInfo const* info = sSpellMgr->GetSpellInfo(sSpellMgr->GetSpellWithRank(SPELL_LIGHTNING_SHIELD_DAMAGE,
            shield->GetSpellInfo()->GetRank()));
        float discharge = float(info ? info->Effects[EFFECT_0].CalcValue(player) : 0)
            + float(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_NATURE)) * DISCHARGE_SP
            + player->GetTotalAttackPowerValue(BASE_ATTACK) * DISCHARGE_AP;
        discharge *= SchoolBonus(player, SPELL_SCHOOL_MASK_NATURE);
        if (HasStormreaverMarker(player, MOD_OVERCHARGED_CORE))
            discharge *= OVERCHARGED;
        Strike(player, target, SPELL_STORM_DISCHARGE, discharge);
        Trace(player, Acore::StringFormat("Lightning Shield discharges for {}", int32(discharge)));
        if (player->HasAura(SPELL_STATIC_STRIKE))
        {
            player->RemoveAurasDueToSpell(SPELL_STATIC_STRIKE);
            Strike(player, target, SPELL_STORM_DISCHARGE, discharge * STATIC_SHARE);
        }
        if (HasStormreaverMarker(player, MOD_VIOLENT_GROUNDING))
        {
            std::size_t count = GROUNDING_TARGETS;
            for (Unit* enemy : EnemiesAround(player, target, GROUNDING_RADIUS))
                if (enemy != target && count)
                {
                    StormArc(player, target, enemy, SPELL_STORM_DISCHARGE, discharge * GROUNDING_SHARE);
                    --count;
                }
        }
        else if (HasStormreaverMarker(player, MOD_RIDE_THE_LIGHTNING))
        {
            RenewFor(player, SPELL_RIDE_THE_LIGHTNING, RIDE_MS);
            player->ModifySpellCooldown(SPELL_LAVA_LASH, -RIDE_COOLDOWN_MS);
        }
    }

    // The Stormreaver's other abilities in the Stormfront: Earth Shock (the next pulse 20% more), Frozen Front (Frost
    // Shock), Stormhammer (Shield Slam), Static Strike (Lightning Bolt), Warstorm (Thunder Clap).
    void StormAbility(Player* player, Unit* target, uint32 firstRank)
    {
        if (!player->HasAura(SPELL_STORMFRONT, player->GetGUID()))
            return;
        State& state = StateOf(player);
        float ap = player->GetTotalAttackPowerValue(BASE_ATTACK);
        if (firstRank == TankRunes::SPELL_EARTH_SHOCK)
            Renew(player, SPELL_STORM_SURGE);
        if (firstRank == SPELL_FROST_SHOCK && target && HasStormreaverMarker(player, MOD_FROZEN_FRONT)
            && Ready(state.FrozenFrontAt, FROZEN_FRONT_EVERY_MS))
        {
            state.FrozenFrontAt = Now();
            Position at = target->GetPosition();
            PlaceDecals(player, SPELL_FROZEN_FRONT, { at });
            float tick = ap * FROZEN_FRONT_AP * SchoolBonus(player, SPELL_SCHOOL_MASK_FROST);
            for (uint32 i = 1; i <= FROZEN_FRONT_TICKS; ++i)
                player->m_Events.AddEventAtOffset([player, at, tick]()
                {
                    if (!player->IsInWorld() || !player->IsAlive())
                        return;
                    for (Unit* enemy : EnemiesIn(player, Circle(at, FROZEN_FRONT_RADIUS), FROZEN_FRONT_TARGETS))
                        Strike(player, enemy, SPELL_FROZEN_FRONT_TICK, tick);
                }, Milliseconds(i * 1000));
        }
        if (firstRank == SPELL_SHIELD_SLAM && target && HasStormreaverMarker(player, MOD_STORMHAMMER)
            && Ready(state.StormhammerAt, STORMHAMMER_EVERY_MS))
        {
            state.StormhammerAt = Now();
            float arc = ap * STORMHAMMER_AP * SchoolBonus(player, SPELL_SCHOOL_MASK_NATURE);
            Strike(player, target, SPELL_STORMHAMMER, arc);
            for (Unit* other : EnemiesAround(player, target, STORM_ECHO_RANGE))
                if (other != target)
                {
                    StormArc(player, target, other, SPELL_STORMHAMMER, arc);
                    break;
                }
        }
        if (firstRank == SPELL_LIGHTNING_BOLT && HasStormreaverMarker(player, MOD_STATIC_STRIKE))
            Renew(player, SPELL_STATIC_STRIKE);
        if (firstRank == SPELL_THUNDER_CLAP && HasStormreaverMarker(player, MOD_WARSTORM)
            && Ready(state.WarstormAt, WARSTORM_EVERY_MS))
        {
            state.WarstormAt = Now();
            float hit = ap * WARSTORM_AP * SchoolBonus(player, SPELL_SCHOOL_MASK_NATURE);
            for (Unit* enemy : EnemiesIn(player, Circle(player->GetPosition(), STORM_ECHO_RANGE), WARSTORM_TARGETS))
                Strike(player, enemy, SPELL_WARSTORM, hit);
        }
    }

    // ---- Ember's Wrath and Whitefang ---------------------------------------------------------------------

    bool HasEmbersMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_EMBERS_MODIFIER_FIRST, modifier);
    }

    bool HasWhitefangMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_WHITEFANG_MODIFIER_FIRST, modifier);
    }

    // The first rank of a spell (itself when it has no ranks).
    uint32 FirstRank(SpellInfo const* info)
    {
        return info ? info->GetFirstRankSpell()->Id : 0;
    }

    // The target bleeds from the Hero's own Rake or Rip.
    bool HasOwnRakeOrRip(Player* player, Unit* target)
    {
        for (auto const& [id, application] : target->GetAppliedAuras())
        {
            Aura const* aura = application->GetBase();
            uint32 first = FirstRank(aura->GetSpellInfo());
            if (aura->GetCasterGUID() == player->GetGUID()
                && (first == SPELL_RAKE || first == SPELL_RIP || first == SPELL_BLAZING_BLEED))
                return true;
        }
        return false;
    }

    bool HasCinderWound(Player* player, Unit* target)
    {
        return target->HasAura(SPELL_CINDER_WOUND, player->GetGUID())
            || target->HasAura(SPELL_CINDER_WOUND_SPREAD, player->GetGUID());
    }

    // A fresh Cinder Wound's whole damage: 150% of attack power plus 100% of spell power (Cinderclaw 40% more;
    // Living Flame 15% more).
    float CinderFresh(Player* player)
    {
        float fresh = player->GetTotalAttackPowerValue(BASE_ATTACK) * CINDER_AP
            + float(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE)) * CINDER_SP;
        fresh *= SchoolBonus(player, SPELL_SCHOOL_MASK_FIRE);
        if (HasEmbersMarker(player, MOD_CINDERCLAW))
            fresh *= CINDERCLAW;
        else if (HasEmbersMarker(player, MOD_LIVING_FLAME))
            fresh *= LIVING_FLAME;
        return fresh;
    }

    int32 CinderMs(Player* /*player*/)
    {
        return CINDER_MS;
    }

    // The Hero's Cinder Wound on the unit (the first's, else a spread one; nullptr none).
    Aura* CinderAura(Player* player, Unit* target)
    {
        if (Aura* wound = target->GetAura(SPELL_CINDER_WOUND, player->GetGUID()))
            return wound;
        return target->GetAura(SPELL_CINDER_WOUND_SPREAD, player->GetGUID());
    }

    // What is left of the Hero's Cinder Wound there spreads to up to count enemies within 8 yards without one, for
    // its remaining time (share of it).
    void SpreadCinder(Player* player, Unit* from, std::size_t count, float share)
    {
        Aura* wound = CinderAura(player, from);
        if (!wound)
            return;
        float left = DotLeft(player, from, wound->GetId()) * share;
        int32 ms = wound->GetDuration();
        if (left <= 0.0f || ms <= 0)
            return;
        for (Unit* enemy : EnemiesAround(player, from, EMBER_SPREAD_RANGE))
            if (enemy != from && count && !HasCinderWound(player, enemy))
            {
                AddDot(player, enemy, SPELL_CINDER_WOUND_SPREAD, left, ms);
                --count;
            }
    }

    // Fire Blast landed: it ignites the Hero's Rake or Rip victim (Ashen Renewal: 10 Energy, 8 sec apart); on their
    // Flame Shock target with the wound, it spreads (Firestorm Pounce: to 3). Ember Rush: speed, and the next Shred
    // 10 Energy back; Frozen Embers on their Frost Fever target; Healing Flame on a bleeding one in their
    // Rejuvenation.
    void CatFireBlast(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        ObjectGuid me = player->GetGUID();
        if (HasEmbersMarker(player, MOD_EMBER_RUSH))
        {
            RenewFor(player, SPELL_FLAMEBOUND, EMBER_RUSH_MS);
            RenewFor(player, SPELL_FLAMEBOUND_DISCOUNT, EMBER_RUSH_DISCOUNT_MS);
        }
        if (HasEmbersMarker(player, MOD_FROZEN_EMBERS) && target->HasAura(SPELL_FROST_FEVER, me)
            && Ready(state.FrozenEmbersAt, FROZEN_EMBERS_EVERY_MS))
        {
            state.FrozenEmbersAt = Now();
            Strike(player, target, SPELL_FROZEN_EMBERS, player->GetTotalAttackPowerValue(BASE_ATTACK)
                * FROZEN_EMBERS_AP
                * SchoolBonus(player, SPELL_SCHOOL_MASK_FROST));
        }
        bool bleeding = target->HasAuraWithMechanic(1ULL << MECHANIC_BLEED);
        if (HasEmbersMarker(player, MOD_HEALING_FLAME) && bleeding
            && Ready(state.HealingFlameAt, HEALING_FLAME_EVERY_MS)
            && player->GetAuraOfRankedSpell(SPELL_REJUVENATION, me))
        {
            state.HealingFlameAt = Now();
            int32 heal = std::max<int32>(1, int32(player->CountPctFromMaxHealth(int32(HEALING_FLAME_PCT))));
            Report(player, SPELL_HEALING_FLAME, 'H', heal);
            player->CastCustomSpell(player, SPELL_HEALING_FLAME, &heal, nullptr, nullptr, true);
        }
        if (target->GetAuraOfRankedSpell(SPELL_FLAME_SHOCK, me) && HasCinderWound(player, target)
            && Ready(state.FlameSpreadAt, FLAME_SPREAD_EVERY_MS))
        {
            state.FlameSpreadAt = Now();
            SpreadCinder(player, target, HasEmbersMarker(player, MOD_FIRESTORM_POUNCE) ? FIRESTORM_TARGETS : 1, 1.0f);
        }
        if (!target->IsAlive() || !HasOwnRakeOrRip(player, target))
            return;
        float fresh = CinderFresh(player);
        AddDot(player, target, SPELL_CINDER_WOUND, fresh, CinderMs(player));
        Trace(player, Acore::StringFormat("Cinder Wound: {} over {} sec", int32(fresh), CinderMs(player) / 1000));
        if (HasEmbersMarker(player, MOD_ASHEN_RENEWAL) && Ready(state.AshenRenewalAt, ASHEN_EVERY_MS))
        {
            state.AshenRenewalAt = Now();
            player->ModifyPower(POWER_ENERGY, ASHEN_ENERGY);
        }
    }

    // Wildfire Swipe: Swipe with a Cinder Wound in its reach spreads half of what is left of it to one more enemy
    // within 8 yards (8 sec apart).
    void CinderSwipe(Player* player)
    {
        State& state = StateOf(player);
        if (!Ready(state.WildfireSwipeAt, WILDFIRE_SWIPE_EVERY_MS))
            return;
        for (Unit* enemy : EnemiesAround(player, player, EMBER_SPREAD_RANGE))
            if (HasCinderWound(player, enemy))
            {
                state.WildfireSwipeAt = Now();
                SpreadCinder(player, enemy, 1, WILDFIRE_SWIPE_SHARE);
                return;
            }
    }

    // A direct Shred on the Hero's Cinder Wound: 10% of attack power as Fire (3 sec a target). A Rake on it: Burning
    // Rake, the next Burning Claws 30% more.
    void EmberClawHit(Player* player, Unit* target, uint32 firstRank)
    {
        State& state = StateOf(player);
        if (!HasCinderWound(player, target))
            return;
        if (firstRank == SPELL_SHRED)
        {
            auto& at = state.ShredEmberAt[target->GetGUID()];
            if (Ready(at, SHRED_EMBER_EVERY_MS))
            {
                at = Now();
                Strike(player, target, SPELL_SEARING_SHRED, player->GetTotalAttackPowerValue(BASE_ATTACK)
                    * SHRED_EMBER_AP
                    * SchoolBonus(player, SPELL_SCHOOL_MASK_FIRE));
            }
        }
        if (firstRank == SPELL_RAKE && HasEmbersMarker(player, MOD_BURNING_RAKE))
            Renew(player, SPELL_BURNING_RAKE);
    }

    // Ferocious Bite on the Hero's Cinder Wound: 1 sec longer a combo point (5 sec an application); Smoldering Bite,
    // its next tick at once.
    void EmberBite(Player* player, Unit* target, uint8 points)
    {
        Aura* wound = CinderAura(player, target);
        if (!wound)
            return;
        ExtendAura(wound, BITE_EXTEND_MS * int32(points), BITE_EXTEND_MOST_MS);
        if (!HasEmbersMarker(player, MOD_SMOLDERING_BITE))
            return;
        auto tick = dotTicks.find({ target->GetGUID(), wound->GetId(), player->GetGUID() });
        if (tick == dotTicks.end())
            return;
        int32 damage = std::max(1, tick->second);
        int32 period = DotPeriod(wound->GetId());
        player->CastCustomSpell(target, DotDamageSpell(wound->GetId()), &damage, nullptr, nullptr, true);
        if (wound->GetDuration() <= period)
            target->RemoveAurasDueToSpell(wound->GetId(), player->GetGUID());
        else
            wound->SetDuration(wound->GetDuration() - period);
    }

    // Phoenix Claws: a direct Pyroblast on the Hero's Cinder Wound, a burst of 40% of spell power (8 sec apart).
    void EmberPyroblast(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        if (!HasEmbersMarker(player, MOD_PHOENIX_CLAWS) || !HasCinderWound(player, target)
            || !Ready(state.PhoenixClawsAt, PHOENIX_CLAWS_EVERY_MS))
            return;
        state.PhoenixClawsAt = Now();
        Strike(player, target, SPELL_PHOENIX_CLAWS,
            RunePower(player, SPELL_SCHOOL_MASK_FIRE, SPELL_FIRE_BLAST) * PHOENIX_CLAWS_POWER);
    }

    // Icy Touch landed: Frozen Prey on its victim (one primary mark: an earlier one goes; a fresh mark can be
    // empowered by a Frostbolt again).
    void CatIcyTouch(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        if (state.PreyOn && state.PreyOn != target->GetGUID())
            if (Unit* earlier = ObjectAccessor::GetUnit(*player, state.PreyOn))
                earlier->RemoveAurasDueToSpell(SPELL_FROZEN_PREY, player->GetGUID());
        Lasting(player, target, SPELL_FROZEN_PREY, PREY_MS);
        state.PreyOn = target->GetGUID();
        state.PreyAt = Now();
        state.PreyChilled = false;
        state.PreyChillUsed = false;
        Trace(player, "Frozen Prey laid");
    }

    // A direct Frostbolt hit on the Hero's Frozen Prey: the next shatter 25% more (Predator's Chill 40%), the mark 2
    // sec longer (once a mark).
    void FangFrostbolt(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        Aura* prey = target->GetAura(SPELL_FROZEN_PREY, player->GetGUID());
        if (!prey || state.PreyChillUsed)
            return;
        state.PreyChillUsed = true;
        state.PreyChilled = true;
        prey->SetMaxDuration(prey->GetMaxDuration() + FROSTBOLT_PREY_MS);
        prey->SetDuration(prey->GetDuration() + FROSTBOLT_PREY_MS);
    }

    // A direct Shred or Mangle on a marked victim shatters the mark: 150% of attack power as Frost damage (Predator's
    // Fang 40% more; the Frostbolt's 25% or 40%; a Pack of Winter mark half); Glacial Claws, half to up to 3 within 8
    // yards; Marked Prey, half to one more. With the Hero's Rake on it, Frozen Wound: 25% of attack power over 6 sec
    // (Frozen Wound 50% more). Stormfang, Flamefang, Moonfrost; Pack of Winter's mark; Swift Hunt, Winter's
    // Recovery, Lingering Frost, Mending Fang, Clawing Cold.
    void Shatter(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        ObjectGuid me = player->GetGUID();
        bool primary = target->HasAura(SPELL_FROZEN_PREY, me);
        bool pack = !primary && target->HasAura(SPELL_FROZEN_PREY_PACK, me);
        if (!primary && !pack)
            return;
        target->RemoveAurasDueToSpell(primary ? SPELL_FROZEN_PREY : SPELL_FROZEN_PREY_PACK, me);
        float ap = player->GetTotalAttackPowerValue(BASE_ATTACK);
        float damage = ap * SHATTER_AP;
        if (HasWhitefangMarker(player, MOD_PREDATORS_FANG))
            damage *= WHITEFANG_BONUS;
        bool chilled = primary && state.PreyChilled;
        if (chilled)
            damage *= HasWhitefangMarker(player, MOD_PREDATORS_CHILL) ? PREDATORS_CHILL : FROSTBOLT_CHILL;
        if (pack)
            damage *= PACK_FROST_SHARE;
        if (primary)
        {
            state.PreyOn.Clear();
            state.PreyChilled = false;
        }
        Trace(player, Acore::StringFormat("Shatter for {}", int32(damage)));
        Strike(player, target, SPELL_FANG_SHATTER, damage);
        if (HasWhitefangMarker(player, MOD_GLACIAL_CLAWS))
        {
            std::size_t count = GLACIAL_CLAWS_TARGETS;
            for (Unit* enemy : EnemiesAround(player, target, PACK_FROST_RADIUS))
                if (enemy != target && count)
                {
                    Strike(player, enemy, SPELL_FANG_SHATTER, damage * ICEFANG_SHARE);
                    --count;
                }
        }
        if (HasWhitefangMarker(player, MOD_MARKED_PREY) && target->GetAuraOfRankedSpell(SPELL_HUNTERS_MARK, me))
            for (Unit* enemy : EnemiesAround(player, target, PACK_FROST_RADIUS))
                if (enemy != target)
                {
                    Strike(player, enemy, SPELL_FANG_SHATTER, damage * ICEFANG_SHARE);
                    break;
                }
        if (HasWhitefangMarker(player, MOD_STORMFANG) && player->GetAuraOfRankedSpell(SPELL_LIGHTNING_SHIELD, me))
            Strike(player, target, SPELL_STORMFANG, RunePower(player, SPELL_SCHOOL_MASK_NATURE, SPELL_ICY_TOUCH)
                * FANG_SPELL_POWER);
        if (HasWhitefangMarker(player, MOD_FLAMEFANG) && target->GetAuraOfRankedSpell(SPELL_FLAME_SHOCK, me))
            Strike(player, target, SPELL_FLAMEFANG, RunePower(player, SPELL_SCHOOL_MASK_FIRE, SPELL_ICY_TOUCH)
                * FANG_SPELL_POWER);
        if (HasWhitefangMarker(player, MOD_MOONFROST_FANG))
            if (Aura* moonfire = target->GetAuraOfRankedSpell(SPELL_MOONFIRE, me))
                ExtendAura(moonfire, FANG_MOONFIRE_MS, FANG_MOONFIRE_MOST_MS);
        if (target->IsAlive() && target->GetAuraOfRankedSpell(SPELL_RAKE, me))
            AddDot(player, target, SPELL_WINTER_WOUND, ap * FROZEN_WOUND_AP
                * SchoolBonus(player, SPELL_SCHOOL_MASK_FROST)
                * (HasWhitefangMarker(player, MOD_FROZEN_WOUND) ? FROZEN_WOUND_MORE : 1.0f), WINTER_WOUND_MS);
        if (primary && HasWhitefangMarker(player, MOD_PACK_OF_WINTER))
            for (Unit* enemy : EnemiesAround(player, target, PACK_FROST_RADIUS))
                if (enemy != target)
                {
                    Lasting(player, enemy, SPELL_FROZEN_PREY_PACK, PACK_FROST_MS);
                    break;
                }
        if (HasWhitefangMarker(player, MOD_SWIFT_HUNT))
            RenewFor(player, SPELL_PURSUING_CHILL, SWIFT_HUNT_MS);
        if (HasWhitefangMarker(player, MOD_WINTERS_RECOVERY) && Ready(state.RecoveryAt, RECOVERY_EVERY_MS))
        {
            state.RecoveryAt = Now();
            player->ModifyPower(POWER_ENERGY, FANG_REFUND);
        }
        if (HasWhitefangMarker(player, MOD_LINGERING_FROST))
        {
            if (target->IsAlive())
                Lasting(player, target, SPELL_SNOWBLIND_SLOW, LINGERING_MS);
            Renew(player, SPELL_LINGERING_FROST);
        }
        if (HasWhitefangMarker(player, MOD_MENDING_FANG) && Ready(state.MendingAt, MENDING_EVERY_MS)
            && player->GetAuraOfRankedSpell(SPELL_REJUVENATION, me))
        {
            state.MendingAt = Now();
            int32 heal = std::max<int32>(1, int32(player->CountPctFromMaxHealth(int32(MENDING_PCT))));
            Report(player, SPELL_MENDING_FANG, 'H', heal);
            player->CastCustomSpell(player, SPELL_MENDING_FANG, &heal, nullptr, nullptr, true);
        }
        if (chilled && HasWhitefangMarker(player, MOD_CLAWING_COLD))
            Renew(player, SPELL_CLAWING_COLD);
    }

    // Ferocious Bite on the Hero's Frozen Wound: its next tick at once (no tick added); Cold Blood, 10% of attack
    // power a combo point as Frost.
    void FangBite(Player* player, Unit* target, uint8 points)
    {
        Aura* wound = target->GetAura(SPELL_WINTER_WOUND, player->GetGUID());
        if (!wound)
            return;
        if (HasWhitefangMarker(player, MOD_FANG_COLD_BLOOD) && points)
            Strike(player, target, SPELL_FANG_SHATTER, player->GetTotalAttackPowerValue(BASE_ATTACK)
                * FANG_COLD_BLOOD_AP * float(points) * SchoolBonus(player, SPELL_SCHOOL_MASK_FROST));
        auto tick = dotTicks.find({ target->GetGUID(), SPELL_WINTER_WOUND, player->GetGUID() });
        if (tick == dotTicks.end())
            return;
        int32 damage = std::max(1, tick->second);
        int32 period = DotPeriod(SPELL_WINTER_WOUND);
        player->CastCustomSpell(target, SPELL_WINTER_WOUND_TICK, &damage, nullptr, nullptr, true);
        if (wound->GetDuration() <= period)
            target->RemoveAurasDueToSpell(SPELL_WINTER_WOUND, player->GetGUID());
        else
            wound->SetDuration(wound->GetDuration() - period);
    }

    // Kindled or Frostbitten Claw (its hit, damage): a second enemy within 5 yards takes it again as Fire or Frost,
    // and both bleed (Blazing Strike) or are frostbitten and slowed (Frostfang Strike).
    void EmpoweredClaw(Player* player, Unit* target, float damage, bool fire)
    {
        player->RemoveAurasDueToSpell(fire ? SPELL_KINDLED : SPELL_FROSTBITTEN);
        std::vector<Unit*> struck { target };
        bool wildfire = fire && HasEmbersMarker(player, MOD_WILDFIRE_CLAWS);
        if (fire)
        {
            // Burning Claws: Burning Rake 30% more (spent), Flame-Tipped 40% more, Flame Shock Claws 25% more on the
            // Hero's Flame Shock target: as Fire on its target, after its own hit.
            float more = HasEmbersMarker(player, MOD_FLAME_TIPPED_CLAWS) ? FLAME_TIPPED : 0.0f;
            if (player->HasAura(SPELL_BURNING_RAKE))
            {
                player->RemoveAurasDueToSpell(SPELL_BURNING_RAKE);
                more += BURNING_RAKE;
            }
            if (HasEmbersMarker(player, MOD_FLAME_SHOCK_CLAWS)
                && target->GetAuraOfRankedSpell(SPELL_FLAME_SHOCK, player->GetGUID()))
                more += FS_CLAWS;
            if (more > 0.0f)
                Strike(player, target, SPELL_BLAZING_STRIKE, damage * more);
        }
        std::size_t seconds = wildfire ? WILDFIRE_CLAWS_TARGETS : 1;
        for (Unit* enemy : EnemiesAround(player, target, CLAW_SECOND_RADIUS))
            if (enemy != target && seconds)
            {
                Strike(player, enemy, fire ? SPELL_BLAZING_STRIKE : SPELL_FROSTFANG_STRIKE,
                    damage * (wildfire ? WILDFIRE_CLAWS_SHARE : CLAW_SECOND_SHARE));
                struck.push_back(enemy);
                --seconds;
            }
        if (wildfire && struck.size() == 1)
            Strike(player, target, SPELL_BLAZING_STRIKE, damage * WILDFIRE_ALONE);
        float dot = player->GetTotalAttackPowerValue(BASE_ATTACK) * CLAW_DOT_AP
            * SchoolBonus(player, fire ? SPELL_SCHOOL_MASK_FIRE : SPELL_SCHOOL_MASK_FROST);
        for (Unit* enemy : struck)
        {
            if (!enemy->IsAlive())
                continue;
            AddDot(player, enemy, fire ? SPELL_BLAZING_BLEED : SPELL_FROSTBITE, dot, CLAW_DOT_MS);
            if (!fire)
                Lasting(player, enemy, SPELL_FROSTBITE_SLOW, FROSTBITE_SLOW_MS);
        }
        Trace(player, fire ? "Blazing Strike" : "Frostfang Strike");
    }

    // Kindled or Frostbitten Swipe: 75% of attack power to up to 8 enemies within 8 yards; Fiery Explosion leaves half
    // a Cinder Wound on each, Glacial Burst slows them 50% and shatters every Frozen Prey among them.
    void EmpoweredSwipe(Player* player, bool fire)
    {
        player->RemoveAurasDueToSpell(fire ? SPELL_KINDLED : SPELL_FROSTBITTEN);
        player->CastSpell(player, fire ? SPELL_FIERY_EXPLOSION_LOOK : SPELL_GLACIAL_BURST_LOOK, true);
        float blast = player->GetTotalAttackPowerValue(BASE_ATTACK) * NOVA_AP
            * SchoolBonus(player, fire ? SPELL_SCHOOL_MASK_FIRE : SPELL_SCHOOL_MASK_FROST);
        float wound = CinderFresh(player) * FIERY_WOUND_SHARE;
        for (Unit* enemy : EnemiesIn(player, Circle(player->GetPosition(), NOVA_RADIUS), NOVA_TARGETS))
        {
            Strike(player, enemy, fire ? SPELL_FIERY_EXPLOSION : SPELL_GLACIAL_BURST, blast);
            if (!enemy->IsAlive())
                continue;
            if (fire)
                AddDot(player, enemy, SPELL_CINDER_WOUND_SPREAD, wound, CinderMs(player));
            else
            {
                Lasting(player, enemy, SPELL_GLACIAL_SLOW, GLACIAL_SLOW_MS);
                if (player->HasAura(SPELL_RUNE_WHITEFANG))
                    Shatter(player, enemy);
            }
        }
        Trace(player, fire ? "Fiery Explosion" : "Glacial Burst");
    }

    // The Cat with a cat rune bound: the Flame Cat (Ember's Wrath) or the Ice Cat (Whitefang), displays 90207 and 90208
    // (client-patch/patch_creatures.py); none for the usual cat.
    constexpr uint32 DISPLAY_FLAME_CAT = 90207;
    constexpr uint32 DISPLAY_ICE_CAT = 90208;

    uint32 CatLook(Unit* unit)
    {
        if (unit->HasAura(SPELL_RUNE_EMBERS_WRATH))
            return DISPLAY_FLAME_CAT;
        return unit->HasAura(SPELL_RUNE_WHITEFANG) ? DISPLAY_ICE_CAT : 0;
    }

    // A Hero in Cat Form takes the look of the rune now bound, or the usual cat's back. A transform (a costume, a
    // polymorph) on top stays. Set again a moment later too: nothing else in the same update overrides it.
    void SetCatLook(Unit* unit)
    {
        if (unit->GetShapeshiftForm() != FORM_CAT || unit->HasAuraType(SPELL_AURA_TRANSFORM))
            return;
        uint32 look = CatLook(unit);
        if (!look)
            look = unit->GetModelForForm(FORM_CAT, SPELL_CAT_FORM);
        if (look && look != unit->GetDisplayId())
            unit->SetDisplayId(look, unit->GetObjectScale());
        if (Player* player = unit->ToPlayer())
            Trace(player, "Cat look " + std::to_string(look) + ", showing " + std::to_string(unit->GetDisplayId()));
    }

    void RefreshCatLook(Unit* unit)
    {
        SetCatLook(unit);
        unit->m_Events.AddEventAtOffset([unit]() { SetCatLook(unit); }, 100ms);
    }

    // A spell a Cat may cast with its rune (Fire Blast, Flame Shock: Ember's Wrath; Frost Shock: Whitefang): its "not
    // while shapeshifted" rule is gone from both copies so the client lets it be tried; here it holds for anyone in
    // a form but a Cat with the rune (a stance never had it).
    SpellCastResult CatFormGate(Unit* caster, uint32 runeAura)
    {
        ShapeshiftForm form = caster->GetShapeshiftForm();
        if (form == FORM_NONE)
            return SPELL_CAST_OK;
        SpellShapeshiftFormEntry const* shape = sSpellShapeshiftFormStore.LookupEntry(form);
        if (shape && (shape->flags1 & SHAPESHIFT_FLAG_STANCE))
            return SPELL_CAST_OK;
        return form == FORM_CAT && caster->HasAura(runeAura) ? SPELL_CAST_OK : SPELL_FAILED_NOT_SHAPESHIFT;
    }

    // The Cat Form copies the runes first taught (Fire Blast 91420, Icy Touch 91440) are gone: a Hero who learned one
    // forgets it, its buttons the real spell's (its highest known rank) or empty.
    void RetireCatCopy(Player* player, uint32 copy, uint32 firstRank)
    {
        if (!player->HasSpell(copy))
            return;
        uint32 real = KnownRank(player, firstRank);
        for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
            if (ActionButton const* action = player->GetActionButton(button);
                action && action->GetType() == ACTION_BUTTON_SPELL && action->GetAction() == copy)
            {
                if (real)
                    player->addActionButton(button, real, ACTION_BUTTON_SPELL);
                else
                    player->removeActionButton(button);
            }
        player->removeSpell(copy, SPEC_MASK_ALL, false);
        player->SendActionButtons(1);
    }

    void ClearDuelists(Player* player)
    {
        State& state = StateOf(player);
        state.SabreHits = 0;
        state.StormfrontTimer = 0;
        state.BlazingRakeAt = state.AshenReadyAt = state.PreyAt = 0;
        state.AshenLast.Clear();
        state.AshenSeen.clear();
        state.PreyOn.Clear();
        state.ColdBloodOn.Clear();
        state.PursuitDiscount = false;
        player->RemoveAurasDueToSpell(SPELL_KINDLED);
        player->RemoveAurasDueToSpell(SPELL_FROSTBITTEN);
        for (uint32 spell : { SPELL_OPENING_SHOT, SPELL_LOADED_STEEL, SPELL_LOADED_POWDER, SPELL_MARKED_SHOT,
                              SPELL_ARCANE_POWDER, SPELL_DEAD_AIM, SPELL_CUT_AND_RUN, SPELL_STORMFRONT,
                              SPELL_STORM_SURGE, SPELL_STATIC_STRIKE,
                              SPELL_RIDE_THE_LIGHTNING,
                              SPELL_FLAMEBOUND, SPELL_FLAMEBOUND_DISCOUNT, SPELL_BURNING_RAKE,
                              SPELL_PURSUING_CHILL, SPELL_SNOWBLIND,
                              SPELL_LINGERING_FROST, SPELL_CLAWING_COLD })
            player->RemoveAurasDueToSpell(spell);
        player->RemoveDynObject(SPELL_FROZEN_TRAIL);
    }

    // Everything the five newest left out (death, a rune or spec change, logout).
    void ClearNewest(Player* player)
    {
        State& state = StateOf(player);
        for (uint32 spell : { SPELL_ARCANE_INSIGHT, SPELL_MIND_UNBOUND, SPELL_MENTAL_RESERVES, SPELL_FROST_CROWN,
                              SPELL_COLD_SNAP, SPELL_COLD_FRONT, SPELL_ASHEN_FLOCK, SPELL_MIND_FLOW,
                              SPELL_GATHERING_BLIZZARD,
                              SPELL_STORM_CONDUIT_CHARGE, SPELL_STARFALL_EQUATION, SPELL_HOT_PURSUIT })
            player->RemoveAurasDueToSpell(spell);
        state.Icicles = 0;
        state.IcicleAttacks = 0;
        state.RimeOn.clear();
        state.BlizzardHit.clear();
        state.BlastCount = 0;
        state.MindChannel = false;
        DismissPhoenixes(player);
        DismissOrb(player);
        state.RemnantUntil = 0;
        player->RemoveDynObject(SPELL_ASTRAL_REMNANT);
        ClearStar(player);
        ClearDruids(player);
        ClearDuelists(player);
    }

    // ---- The rune's auras ------------------------------------------------------------------------------------

    std::vector<uint32> RuneAuras()
    {
        std::vector<uint32> auras;
        for (RuneDef const& rune : Catalog())
        {
            auras.push_back(rune.Aura);
            for (uint32 i = 0; i < uint32(SLOTS * rune.Choices); ++i)
                auras.push_back(rune.MarkerFirst + i);
        }
        return auras;
    }

    // The level a rune slot opens at.
    uint8 SlotUnlock(uint8 runeSlot)
    {
        return runeSlot ? SECOND_UNLOCK_LEVEL : UNLOCK_LEVEL;
    }

    // The active spec's runes and their modifiers as auras (the first from UNLOCK_LEVEL, the second from
    // SECOND_UNLOCK_LEVEL). Each rune's markers are its own spells: both runes' sit side by side.
    void Apply(Player* player)
    {
        for (uint32 spell : RuneAuras())
            player->RemoveAurasDueToSpell(spell);
        bool any = false, paragon = false, duelist = false, dawn = false, trueshot = false;
        for (uint8 runeSlot = 0; runeSlot < RUNE_SLOTS; ++runeSlot)
        {
            Choice const& choice = ActiveChoice(player, runeSlot);
            RuneDef const* rune = FindRune(choice.Rune);
            if (!rune || player->GetLevel() < SlotUnlock(runeSlot))
                continue;
            any = true;
            paragon |= rune->Id == RUNE_SANCTIFIED_PARAGON;
            duelist |= rune->Id == RUNE_DUELIST;
            dawn |= rune->Id == RUNE_DAWNBOUND_CRUSADER;
            trueshot |= rune->Id == RUNE_TRUESHOT;
            player->AddAura(rune->Aura, player);
            for (uint8 slot = 0; slot < SLOTS; ++slot)
                if (uint8 pick = choice.Mods[slot]; pick && pick <= rune->Choices)
                    player->AddAura(rune->MarkerFirst + slot * rune->Choices + pick - 1, player);
        }
        if (!any)
            ClearCombat(player);
        if (!paragon)
            player->RemoveAurasDueToSpell(SPELL_SANCTIFIED_PARAGON);
        TeachDuelist(player, duelist);
        TeachDawn(player, dawn);
        TeachTrueshot(player, trueshot);
        TankRunes::Apply(player);
        RetireCatCopy(player, SPELL_CAT_FIRE_BLAST, SPELL_FIRE_BLAST);
        RetireCatCopy(player, SPELL_CAT_ICY_TOUCH, SPELL_ICY_TOUCH);
        RefreshCatLook(player);
        ApplyLegend(player, 0);
    }

    // titan_runes holds each specialization's first rune, titan_runes_second its second (db-characters).
    char const* RuneTable(uint8 runeSlot)
    {
        return runeSlot ? "titan_runes_second" : "titan_runes";
    }

    void Save(Player* player, uint8 spec, uint8 runeSlot = 0)
    {
        Choice const& choice = StateOf(player).Choices[spec][runeSlot];
        CharacterDatabase.Execute(Acore::StringFormat("REPLACE INTO `{}` (`guid`, `spec`, `rune`, "
            "`invocation`, `manifestation`, `adaptation`) VALUES ({}, {}, {}, {}, {}, {})", RuneTable(runeSlot),
            player->GetGUID().GetCounter(), spec, choice.Rune, choice.Mods[0], choice.Mods[1], choice.Mods[2]));
    }

    void Load(Player* player)
    {
        State& state = StateOf(player);
        state = State();
        for (uint8 runeSlot = 0; runeSlot < RUNE_SLOTS; ++runeSlot)
        {
            QueryResult result = CharacterDatabase.Query("SELECT `spec`, `rune`, `invocation`, `manifestation`, "
                "`adaptation` FROM `{}` WHERE `guid` = {}", RuneTable(runeSlot), player->GetGUID().GetCounter());
            if (!result)
                continue;
            do
            {
                Field* fields = result->Fetch();
                uint8 spec = fields[0].Get<uint8>();
                if (spec >= MAX_SPECS)
                    continue;
                Choice& choice = state.Choices[spec][runeSlot];
                choice.Rune = fields[1].Get<uint8>();
                for (uint8 slot = 0; slot < SLOTS; ++slot)
                    choice.Mods[slot] = std::min<uint8>(fields[2 + slot].Get<uint8>(), MAX_CHOICES);
            } while (result->NextRow());
        }
    }

    // A retired rune bound in any specialization is let go (its modifiers with it), and the Hero told; so is a second
    // rune that is the same as the first.
    void ReleaseRetired(Player* player)
    {
        State& state = StateOf(player);
        for (uint8 spec = 0; spec < MAX_SPECS; ++spec)
            for (uint8 runeSlot = 0; runeSlot < RUNE_SLOTS; ++runeSlot)
            {
                Choice& choice = state.Choices[spec][runeSlot];
                if (!choice.Rune || !IsRetired(choice.Rune))
                    continue;
                choice = Choice();
                Save(player, spec, runeSlot);
                ChatHandler(player->GetSession()).PSendSysMessage("|cffa335eeLegendary Rune:|r your rune is "
                    "temporarily unavailable while its design is reworked, and has been released: choose another in "
                    "the rune panel.");
            }
        // The same rune in both slots (never allowed): the second is let go.
        for (uint8 spec = 0; spec < MAX_SPECS; ++spec)
        {
            Choice& second = state.Choices[spec][1];
            if (!second.Rune || second.Rune != state.Choices[spec][0].Rune)
                continue;
            second = Choice();
            Save(player, spec, 1);
            ChatHandler(player->GetSession()).SendSysMessage("|cffa335eeLegendary Rune:|r your two runes were the "
                "same rune; your second has been released: choose another in the rune panel.");
        }
    }

    // ---- Learn Available ---------------------------------------------------------------------------------------

    // What Learn Available would do for a rune: each ability learned now and the kit ability it replaces (none: a
    // pick), each card slotted and the slot it takes (the card it replaces: id 0 an empty slot); or why it can't.
    struct Plan
    {
        struct Learned
        {
            Wildcard::Ability const* Ability;
            Wildcard::Ability const* Replaces;
        };
        struct Slotted
        {
            Wildcard::Ability const* Ability;
            uint8 Slot;
            std::pair<uint8, uint32> Replaces;
        };
        std::vector<Learned> Learn;
        std::vector<Slotted> Slot;
        std::string Refusal;
    };

    std::string AbilityName(Wildcard::Ability const& ability)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(ability.Ranks.front().SpellId);
        return spellInfo && spellInfo->SpellName[LOCALE_enUS] ? spellInfo->SpellName[LOCALE_enUS] : "an ability";
    }

    // Learnable now as a kit ability: offered, its level reached, allowed for the Hero, and what it needs known (or
    // in the plan already).
    bool LearnableNow(Player* player, Wildcard::Ability const& ability, std::set<uint32> const& planned)
    {
        if (!ability.Offered || ability.Banned || ability.Capstone || ability.Companion || ability.Ranks.empty()
            || ability.Ranks.front().Level > player->GetLevel() || Wildcard::KnownRank(player, ability) >= 0
            || !Wildcard::FitsPlayer(player, ability) || Wildcard::PetConflict(player, ability))
            return false;
        return std::all_of(ability.Requires.begin(), ability.Requires.end(), [&](std::vector<uint32> const& givers)
        {
            return std::any_of(givers.begin(), givers.end(), [&](uint32 id)
                { return Knows(player, id) || planned.contains(id); });
        });
    }

    Plan MakePlan(Player* player, RuneDef const& rune)
    {
        Plan plan;
        if (!Wildcard::IsHero(player) || Specs::IsDormant(player))
        {
            plan.Refusal = "This specialization is empty until you prestige in it.";
            return plan;
        }
        if (!Wildcard::IsKitLocked(player) && Specs::CardPlaces(player).empty())
        {
            plan.Refusal = "Your specializations are still loading. Try again in a moment.";
            return plan;
        }
        if (Wildcard::IsKitLocked(player) || !Specs::CanSlotCards(player))
        {
            plan.Refusal = Acore::StringFormat("Learn Available works until level {}: your abilities and cards lock "
                "then.", Wildcard::LOCK_LEVEL);
            return plan;
        }
        std::set<uint32> slotted = SlottedSpells(player);
        // Kept: every ability the rune names, and what the planned ones need.
        std::set<uint32> keep, planned;
        for (std::vector<uint32> const& group : rune.Requires)
            keep.insert(group.begin(), group.end());
        uint32 kit = Wildcard::KitAbilities(player);
        uint32 picks = kit < Wildcard::KIT_SIZE ? std::min(Wildcard::GetPicks(player), Wildcard::KIT_SIZE - kit) : 0;
        std::vector<std::pair<uint8, uint32>> places = Specs::CardPlaces(player);
        std::set<uint32> replacedKit;
        auto kept = [&](Wildcard::Ability const& ability)
        {
            return keep.contains(ability.Ranks.front().SpellId) || planned.contains(ability.Ranks.front().SpellId);
        };
        for (std::vector<uint32> const& group : rune.Requires)
        {
            if (std::any_of(group.begin(), group.end(), [&](uint32 id)
                { return Knows(player, id) || InChain(slotted, id) || planned.contains(id); }))
                continue;
            // Learned now: a pick, else in place of a kit ability the rune does not need.
            Wildcard::Ability const* learn = nullptr;
            for (uint32 id : group)
                if (Wildcard::Ability const* ability = Wildcard::FindAbility(id))
                    if (LearnableNow(player, *ability, planned))
                    {
                        learn = ability;
                        break;
                    }
            if (learn)
            {
                Wildcard::Ability const* replaces = nullptr;
                if (picks)
                    --picks;
                else
                {
                    for (Wildcard::Ability const& ability : Wildcard::Abilities())
                        if (ability.Offered && !ability.Capstone && !ability.Companion
                            && Wildcard::KnownRank(player, ability) >= 0 && !kept(ability)
                            && !replacedKit.contains(ability.Ranks.front().SpellId))
                            replaces = &ability;
                    if (!replaces)
                    {
                        plan.Refusal = "Your kit has no room: every ability in it is one this rune needs.";
                        return plan;
                    }
                    replacedKit.insert(replaces->Ranks.front().SpellId);
                }
                planned.insert(learn->Ranks.front().SpellId);
                plan.Learn.push_back({ learn, replaces });
                continue;
            }
            // Its Skill Card slotted: in an empty slot, else in place of a card the rune does not need.
            Wildcard::Ability const* card = nullptr;
            for (uint32 id : group)
                if (Wildcard::Ability const* ability = Wildcard::FindAbility(id))
                    if (ability->Offered && Cards::Owns(player, 0, ability->Ranks.front().SpellId, false))
                    {
                        card = ability;
                        break;
                    }
            if (!card)
            {
                Wildcard::Ability const* first = Wildcard::FindAbility(group.front());
                plan.Refusal = Acore::StringFormat("You can't learn {} yet and don't have its Skill Card.",
                    first ? AbilityName(*first) : "an ability it needs");
                return plan;
            }
            int32 place = -1;
            for (uint8 slot = 0; slot < places.size() && place < 0; ++slot)
                if (!places[slot].second)
                    place = slot;
            for (int32 slot = int32(places.size()) - 1; slot >= 0 && place < 0; --slot)
            {
                auto [kind, id] = places[slot];
                Wildcard::Ability const* ability = kind == 0 ? Wildcard::FindAbility(id) : nullptr;
                if (id && id != uint32(-1) && (!ability || !kept(*ability)))
                    place = slot;
            }
            if (place < 0)
            {
                plan.Refusal = "Your card slots have no room: every card in them is one this rune needs.";
                return plan;
            }
            planned.insert(card->Ranks.front().SpellId);
            plan.Slot.push_back({ card, uint8(place), places[place] });
            places[place] = { 0, uint32(-1) };      // taken
        }
        if (plan.Learn.empty() && plan.Slot.empty())
            plan.Refusal = "There is nothing to learn: your abilities and cards already bring what it needs.";
        return plan;
    }

    std::string Ids(std::vector<uint32> const& ids)
    {
        std::string out;
        for (uint32 id : ids)
            out += (out.empty() ? "" : ".") + std::to_string(id);
        return out.empty() ? "-" : out;
    }

    void SendPlan(Player* player, RuneDef const& rune, Plan const& plan)
    {
        std::vector<uint32> learn, replaced;
        for (Plan::Learned const& entry : plan.Learn)
        {
            learn.push_back(entry.Ability->Ranks.front().SpellId);
            replaced.push_back(entry.Replaces ? entry.Replaces->Ranks.front().SpellId : 0);
        }
        std::string slots, cards;
        for (Plan::Slotted const& entry : plan.Slot)
        {
            slots += (slots.empty() ? "" : ".") + std::to_string(entry.Ability->Ranks.front().SpellId);
            cards += (cards.empty() ? "" : ".") + (entry.Replaces.second ? Acore::StringFormat("{}:{}:{}",
                entry.Slot + 1, entry.Replaces.first, entry.Replaces.second) : Acore::StringFormat("{}:-",
                entry.Slot + 1));
        }
        Send(player, Acore::StringFormat("L,{},{},{},{},{}", rune.Id, Ids(learn), Ids(replaced),
            slots.empty() ? "-" : slots, cards.empty() ? "-" : cards));
    }

    void ApplyPlan(Player* player, Plan const& plan)
    {
        ChatHandler handler(player->GetSession());
        for (Plan::Learned const& entry : plan.Learn)
        {
            if (entry.Replaces)
            {
                Wildcard::Unlearn(player, *entry.Replaces);
                handler.PSendSysMessage("|cffa335eeLearn Available:|r {} replaces {} in your kit.",
                    AbilityName(*entry.Ability), AbilityName(*entry.Replaces));
            }
            else
            {
                Wildcard::SetPicks(player, Wildcard::GetPicks(player) ? Wildcard::GetPicks(player) - 1 : 0);
                handler.PSendSysMessage("|cffa335eeLearn Available:|r {} learned.", AbilityName(*entry.Ability));
            }
            Wildcard::Learn(player, *entry.Ability, "picked");
        }
        for (Plan::Slotted const& entry : plan.Slot)
        {
            Specs::PutCard(player, entry.Slot, 0, entry.Ability->Ranks.front().SpellId);
            handler.PSendSysMessage("|cffa335eeLearn Available:|r {}'s Skill Card slotted: it comes at level {}.",
                AbilityName(*entry.Ability), Wildcard::AbilityUnlockLevel(*entry.Ability));
        }
        Classless::SendKnown(player);
        Classless::SendState(player);
    }

    // ---- The panel ---------------------------------------------------------------------------------------------

    // The level a rune slot's panel opens by itself at, while it is empty: Rune I's from PROMPT_LEVEL (bound earlier
    // from the panel itself), Rune II's when it unlocks.
    uint8 PromptLevel(uint8 runeSlot)
    {
        return runeSlot ? SECOND_UNLOCK_LEVEL : PROMPT_LEVEL;
    }

    // A slot open and empty: the panel opens by itself.
    bool Prompt(Player* player)
    {
        if (!Wildcard::IsHero(player) || Specs::IsDormant(player))
            return false;
        for (uint8 runeSlot = 0; runeSlot < RUNE_SLOTS; ++runeSlot)
            if (player->GetLevel() >= PromptLevel(runeSlot) && !ActiveChoice(player, runeSlot).Rune)
                return true;
        return false;
    }

    void SendState(Player* player)
    {
        Choice const& choice = ActiveChoice(player);
        Choice const& second = ActiveChoice(player, 1);
        uint8 role = Wildcard::GetRole(player);
        // Suggested: runes the Hero meets, their role's first (a Tank or Healer sees a hybrid only in Browse all);
        // then those the slotted Skill Cards will complete.
        std::set<uint32> slotted = SlottedSpells(player);
        std::vector<RuneDef const*> offers, withCards;
        for (RuneDef const& rune : Catalog())
            if (!IsRetired(rune.Id)
                && (role == Wildcard::ROLE_NONE || rune.Roles & Wildcard::RoleMask(Wildcard::Role(role))))
            {
                if (Missing(player, rune.Requires) == "-")
                    offers.push_back(&rune);
                else if (Missing(player, rune.Requires, &slotted) == "-")
                    withCards.push_back(&rune);
            }
        offers.insert(offers.end(), withCards.begin(), withCards.end());
        if (offers.size() > MAX_OFFERS + 1)
            offers.resize(MAX_OFFERS + 1);              // one more: the panel leaves out the other slot's rune
        std::string offerList;
        for (RuneDef const* rune : offers)
            offerList += (offerList.empty() ? "" : ".") + std::to_string(rune->Id);

        for (RuneDef const& rune : Catalog())
        {
            std::string missing = Missing(player, rune.Requires);
            bool learnable = missing != "-" && MakePlan(player, rune).Refusal.empty();
            Send(player, Acore::StringFormat("R,{},{},{},{}", rune.Id, missing, Missing(player, rune.Requires,
                &slotted), learnable ? 1 : 0));
            for (uint8 slot = 0; slot < SLOTS; ++slot)
                for (uint8 pick = 0; pick < rune.Choices; ++pick)
                {
                    std::vector<std::vector<uint32>> needs = ModifierNeeds(rune.Id, slot, pick);
                    if (!needs.empty())
                        Send(player, Acore::StringFormat("N,{},{},{},{}", rune.Id, slot + 1, pick + 1,
                            Missing(player, needs)));
                }
        }
        Send(player, Acore::StringFormat("S,{},{},{},{},{},{},{},{},{},{},{},{},{},{}", player->GetLevel(),
            UNLOCK_LEVEL, Specs::ActiveSpec(player) + 1, choice.Rune, choice.Mods[0], choice.Mods[1], choice.Mods[2],
            offerList.empty() ? "-" : offerList, Prompt(player) ? 1 : 0, second.Rune, second.Mods[0], second.Mods[1],
            second.Mods[2], SECOND_UNLOCK_LEVEL));
    }

    void Refuse(Player* player, std::string const& why)
    {
        Send(player, "E," + why);
        ChatHandler(player->GetSession()).PSendSysMessage("|cffa335eeLegendary Rune:|r {}", why);
    }

    void OnPick(Player* player, uint8 runeId, bool anyway, uint8 runeSlot)
    {
        if (runeSlot >= RUNE_SLOTS)
            return;
        Choice& choice = ActiveChoice(player, runeSlot);
        RuneDef const* rune = FindRune(runeId);
        if (player->GetLevel() < SlotUnlock(runeSlot))
            return Refuse(player, runeSlot ? Acore::StringFormat("The second Legendary Rune unlocks at level {}.",
                SECOND_UNLOCK_LEVEL) : Acore::StringFormat("Legendary Runes unlock at level {}.", UNLOCK_LEVEL));
        if (choice.Rune)
            return Refuse(player, "This rune slot is already bound: .unbindrune lets go of it.");
        if (ActiveChoice(player, runeSlot ? 0 : 1).Rune == runeId)
            return Refuse(player, "That rune is already your other Legendary Rune.");
        if (Specs::IsDormant(player))
            return Refuse(player, "This specialization is empty until you prestige in it.");
        if (!rune)
            return Refuse(player, "That rune is not available yet.");
        if (IsRetired(runeId))
            return Refuse(player, "That rune is temporarily unavailable while its design is reworked.");
        if (Missing(player, rune->Requires) != "-" && !anyway)
            return Refuse(player, "You lack an ability this rune needs.");
        choice = Choice();
        choice.Rune = runeId;
        Save(player, Specs::ActiveSpec(player), runeSlot);
        Apply(player);
        ChatHandler(player->GetSession()).PSendSysMessage(player->GetLevel() < Wildcard::LOCK_LEVEL
            ? Acore::StringFormat("|cffa335eeLegendary Rune:|r bound to this specialization. Until level {} you can "
                "unlearn it in the rune panel; then it stays until your next prestige.", Wildcard::LOCK_LEVEL)
            : std::string("|cffa335eeLegendary Rune:|r bound to this specialization until your next prestige. Choose "
                "its modifiers in the rune panel."));
        SendState(player);
    }

    void OnModifier(Player* player, uint8 slot, uint8 pick, uint8 runeSlot)
    {
        if (runeSlot >= RUNE_SLOTS)
            return;
        Choice& choice = ActiveChoice(player, runeSlot);
        RuneDef const* bound = FindRune(choice.Rune);
        if (!bound || slot < 1 || slot > SLOTS || pick < 1 || pick > bound->Choices)
            return;
        if (player->IsInCombat())
            return Refuse(player, "Modifiers can't be changed in combat.");
        if (Missing(player, ModifierNeeds(choice.Rune, slot - 1, pick - 1)) != "-")
            return Refuse(player, "You lack an ability this modifier needs.");
        // The Sanctified Paragon: Bastion of Faith and Weight of the Unworthy work on its damage only (a damaging
        // Manifestation: Sanctified Condemnation or Radiant Judgment); another Manifestation lets go of either.
        if (choice.Rune == RUNE_SANCTIFIED_PARAGON)
        {
            auto damaging = [](uint8 manifestation)
            {
                return manifestation == MOD_SANCTIFIED_CONDEMNATION % MAX_CHOICES + 1
                    || manifestation == MOD_RADIANT_JUDGMENT % MAX_CHOICES + 1;
            };
            auto damageOnly = [](uint8 covenant) { return covenant == MOD_BASTION_OF_FAITH % MAX_CHOICES + 1
                || covenant == MOD_WEIGHT_OF_THE_UNWORTHY % MAX_CHOICES + 1; };
            if (slot == 3 && damageOnly(pick) && !damaging(choice.Mods[1]))
                return Refuse(player, "That modifier needs a damaging Manifestation (Sanctified Condemnation or "
                    "Radiant Judgment).");
            if (slot == 2 && !damaging(pick) && damageOnly(choice.Mods[2]))
            {
                choice.Mods[2] = 0;
                ChatHandler(player->GetSession()).SendSysMessage("|cffa335eeLegendary Rune:|r your Covenant needs "
                    "a damaging Manifestation: choose it again.");
            }
        }
        // The Stormbound Conduit: Live Wire and Storm Within never together.
        if (choice.Rune == RUNE_STORMBOUND_CONDUIT
            && ((slot == 1 && pick == MOD_LIVE_WIRE % MAX_CHOICES + 1
                    && choice.Mods[2] == MOD_STORM_WITHIN % MAX_CHOICES + 1)
                || (slot == 3 && pick == MOD_STORM_WITHIN % MAX_CHOICES + 1
                    && choice.Mods[0] == MOD_LIVE_WIRE % MAX_CHOICES + 1)))
            return Refuse(player, "Live Wire and Storm Within cannot be chosen together.");
        if (choice.Mods[slot - 1] == pick)
            return SendState(player);
        choice.Mods[slot - 1] = pick;
        Save(player, Specs::ActiveSpec(player), runeSlot);
        ClearCombat(player);
        Apply(player);
        SendState(player);
    }

    // Unlearned in the panel: only before the kit locks (Wildcard::LOCK_LEVEL), out of combat.
    void OnUnbind(Player* player, uint8 which)
    {
        if (player->GetLevel() >= Wildcard::LOCK_LEVEL)
            return Refuse(player, Acore::StringFormat("Runes can be unlearned until level {}; now only a prestige "
                "frees them.", Wildcard::LOCK_LEVEL));
        if (player->IsInCombat())
            return Refuse(player, "Not in combat.");
        if (which < 1 || which > RUNE_SLOTS || !ActiveChoice(player, which - 1).Rune)
            return;
        LegendaryRunes::Unbind(player, which);
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
            OnPick(player, uint8(number(1)), number(2) == 1, uint8(std::max<uint32>(number(3), 1) - 1));
        else if (parts[0] == "MOD")
            OnModifier(player, uint8(number(1)), uint8(number(2)), uint8(std::max<uint32>(number(3), 1) - 1));
        else if (parts[0] == "UNBIND")
            OnUnbind(player, uint8(std::max<uint32>(number(1), 1)));
        else if (parts[0] == "PLAN" || parts[0] == "APPLY")
        {
            RuneDef const* rune = FindRune(uint8(number(1)));
            if (!rune || IsRetired(rune->Id))
                return;
            if (player->IsInCombat())
                return Refuse(player, "Not in combat.");
            Plan plan = MakePlan(player, *rune);
            if (!plan.Refusal.empty())
                return Refuse(player, plan.Refusal);
            if (parts[0] == "PLAN")
                return SendPlan(player, *rune, plan);
            ApplyPlan(player, plan);
            SendState(player);
        }
    }
}

namespace LegendaryRunes
{
    void ReplaceAbility(Player* player, uint32 taught, std::vector<uint32> const& bases, bool on)
    {
        uint32 known = 0;
        for (uint32 base : bases)
            if (!known)
                known = KnownRank(player, base);
        on = on && known;
        if (on && !player->HasSpell(taught))
            player->learnSpell(taught);
        bool swapped = false;
        for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
        {
            ActionButton const* action = player->GetActionButton(button);
            if (!action || action->GetType() != ACTION_BUTTON_SPELL)
                continue;
            uint32 spellId = action->GetAction();
            SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
            if (on && info && std::find(bases.begin(), bases.end(), info->GetFirstRankSpell()->Id) != bases.end())
                player->addActionButton(button, taught, ACTION_BUTTON_SPELL);
            else if (!on && spellId == taught && known)
                player->addActionButton(button, known, ACTION_BUTTON_SPELL);
            else
                continue;
            swapped = true;
        }
        if (!on && player->HasSpell(taught))
            player->removeSpell(taught, SPEC_MASK_ALL, false);
        if (swapped)
            player->SendActionButtons(1);
    }

    bool IsReplacedBase(uint32 firstRank)
    {
        return firstRank == SPELL_SINISTER_STRIKE || firstRank == SPELL_ARCANE_SHOT || firstRank == SPELL_RAPID_FIRE
            || firstRank == SPELL_BEACON_OF_LIGHT || firstRank == SPELL_AIMED_SHOT
            || TankRunes::IsReplacedBase(firstRank);
    }

    void OnSpecChanged(Player* player)
    {
        ClearCombat(player);
        Apply(player);
        SendState(player);
        if (Prompt(player))
            Send(player, "O");
    }

    bool Unbind(Player* player, uint8 which)
    {
        bool any = false;
        for (uint8 runeSlot = 0; runeSlot < RUNE_SLOTS; ++runeSlot)
        {
            Choice& choice = ActiveChoice(player, runeSlot);
            if (!choice.Rune || (which && which != runeSlot + 1))
                continue;
            choice = Choice();
            Save(player, Specs::ActiveSpec(player), runeSlot);
            any = true;
        }
        if (!any)
            return false;
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
        for (uint8 runeSlot = 0; runeSlot < RUNE_SLOTS; ++runeSlot)
            CharacterDatabase.Execute(Acore::StringFormat("DELETE FROM `{}` WHERE `guid` = {}", RuneTable(runeSlot),
                player->GetGUID().GetCounter()));
        State& state = StateOf(player);
        state.Choices = {};
        ClearCombat(player);
        Apply(player);
    }

    bool ToggleDebug(Player* player)
    {
        bool& debug = StateOf(player).Debug;
        debug = !debug;
        return debug;
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
// Chaos Strike crit 2); with Dreadfire Discipline, Shadow Bolt and Incinerate 2. A volley
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
        else if (first == SPELL_DEMONS_BITE)
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
// crowd-controlled (the volley would break it). A triggered throw never fires one.
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
        if (Armed())
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

// The Stormbound Conduit's aura (90750): nothing to proc now (its redesign works through ConduitHit).
class spell_titan_rune_stormbound_conduit : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_stormbound_conduit);

    bool CheckProc(ProcEventInfo& /*eventInfo*/)
    {
        return false;                               // the rune's work is in the extended hit script (ConduitHit)
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

    // The Stormreaver's Stormfront: the shield keeps the charge its proc took.
    void Keep(ProcEventInfo& /*eventInfo*/)
    {
        Unit* caster = GetCaster();
        if (caster && caster == GetTarget() && caster->HasAura(SPELL_RUNE_STORMREAVER)
            && caster->HasAura(SPELL_STORMFRONT, caster->GetGUID()))
            GetAura()->ModCharges(1);
    }

    void Register() override
    {
        OnProc += AuraProcFn(spell_titan_rune_lightning_shield::Keep);
    }
};

// A Lightning Shield retaliation (its damage, every rank, and the rune's own 90754) with the Stormbound Conduit:
// Live Wire's 15% more and Conductive; its arc to another Conductive enemy (ShieldArc).
class spell_titan_rune_lightning_shield_damage : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_lightning_shield_damage);

    void Strike()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_STORMBOUND_CONDUIT))
            return;
        if (HasStormMarker(player, MOD_LIVE_WIRE))
            SetHitDamage(int32(GetHitDamage() * LIVE_WIRE_MORE));
        ShieldArc(player, target, float(GetHitDamage()));
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

// Chain Heal (every rank) with the Wellspring, the Hero's own cast: 20% more on their Earth Shield bearer; there it
// releases the bearer's Wellspring (once a cast, not one it laid) and then lays or renews it.
class spell_titan_rune_chain_heal : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_chain_heal);

    void Flow()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || GetSpell()->IsTriggered() || !player->HasAura(SPELL_RUNE_WELLSPRING)
            || !HasEarthShield(player, target))
            return;
        SetHitHeal(int32(GetHitHeal() * WEARER_BONUS));
        if (!_released && WellOn(player, target))
        {
            _released = true;
            ReleaseWell(player, target, float(GetHitHeal()));
        }
        LayWell(player, target);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_titan_rune_chain_heal::Flow);
    }

    bool _released = false;
};

// Earth Shield's heal (every rank) with the Wellspring: on its bearer, their Wellspring renewed (Flowing Waters).
class spell_titan_rune_earth_shield_heal : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_earth_shield_heal);

    void Flow()
    {
        Unit* origin = GetOriginalCaster() ? GetOriginalCaster() : GetCaster();
        Player* player = origin ? origin->ToPlayer() : nullptr;
        Unit* target = GetHitUnit();
        if (player && target && player->HasAura(SPELL_RUNE_WELLSPRING))
            WellShield(player, target);
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
// of a tick's mana back; Renewing Breath: the third tick, the Hero's Renew on the target 2 sec longer. Rolling Mist:
// a channel begun on another ally within a second of the last one's end leaves two ticks over 4 sec on the last
// target (12 sec apart).
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
        if (HasMistMarker(player, MOD_RENEWING_BREATH) && aurEff->GetTickNumber() == RENEWING_BREATH_TICK)
            if (Aura* renew = target->GetAuraOfRankedSpell(SPELL_RENEW, player->GetGUID()))
                ExtendAura(renew, MIST_RENEW_MS, MIST_RENEW_MOST_MS);
        Wander(player, target, float(aurEff->GetAmount()));
    }

    void Begun(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* caster = GetCaster();
        Player* player = caster ? caster->ToPlayer() : nullptr;
        if (!player || !player->HasAura(SPELL_RUNE_WANDERING_MIST) || !HasMistMarker(player, MOD_ROLLING_MIST))
            return;
        State& state = StateOf(player);
        if (!state.MistLast || state.MistLast == GetTarget()->GetGUID()
            || Now() - state.MistGoneAt > uint32(ROLLING_SWITCH_MS) || !Ready(state.RollingAt, ROLLING_EVERY_MS)
            || state.MistTickAmount <= 0)
            return;
        Unit* last = ObjectAccessor::GetUnit(*player, state.MistLast);
        if (!last || !last->IsAlive() || !last->IsInMap(player))
            return;
        state.RollingAt = Now();
        Refresh(player, last, SPELL_ROLLING_MIST, std::max<int32>(1, state.MistTickAmount * ROLLING_TICKS / 4));
    }

    void Gone(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* caster = GetCaster();
        Player* player = caster ? caster->ToPlayer() : nullptr;
        if (!player || !player->HasAura(SPELL_RUNE_WANDERING_MIST))
            return;
        State& state = StateOf(player);
        state.MistLast = GetTarget()->GetGUID();
        state.MistGoneAt = Now();
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_titan_rune_soothing_mist::Tick, EFFECT_0,
            SPELL_AURA_PERIODIC_HEAL);
        AfterEffectApply += AuraEffectApplyFn(spell_titan_rune_soothing_mist::Begun, EFFECT_0,
            SPELL_AURA_PERIODIC_HEAL, AURA_EFFECT_HANDLE_REAL);
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

// Dawn Vow (the Dawnbound Crusader's, in Beacon of Light's place): the vow on its target.
class spell_titan_rune_dawn_vow : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_dawn_vow);

    void Swear(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target && player->HasAura(SPELL_RUNE_DAWNBOUND_CRUSADER))
            SwearVow(player, target);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_titan_rune_dawn_vow::Swear, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// A Judgement with the Dawnbound Crusader: the mark.
class spell_titan_rune_judgement : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_judgement);

    void Echo()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target && player->HasAura(SPELL_RUNE_DAWNBOUND_CRUSADER))
            DawnAbility(player, target, GetSpellInfo()->GetFirstRankSpell()->Id);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_judgement::Echo);
    }
};

// Crusader Strike and Holy Shock's damage or heal (every rank) with the Dawnbound Crusader: an echo (DawnAbility).
class spell_titan_rune_dawn_echo : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_dawn_echo);

    void Echo()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_DAWNBOUND_CRUSADER))
            return;
        uint32 first = GetSpellInfo()->GetFirstRankSpell()->Id;
        DawnAbility(player, target, first == SPELL_CRUSADER_STRIKE ? first : uint32(SPELL_HOLY_SHOCK));
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

// The Shattered Sun's spells (the Hero's own, every rank): Fireball (a collapse, a shard), Frostbolt (a fracture),
// Ice Lance (SunSpell); the others it was once registered on do nothing now.
class spell_titan_rune_shattered_sun : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_shattered_sun);

    void Strike()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || GetSpell()->IsTriggered() || !player->HasAura(SPELL_RUNE_SHATTERED_SUN))
            return;
        uint32 first = GetSpellInfo()->GetFirstRankSpell()->Id;
        if (first == SPELL_FIREBALL || first == SPELL_FROSTBOLT || first == SPELL_ICE_LANCE)
            SunSpell(player, target, first);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_shattered_sun::Strike);
    }
};

// Trueshot (both windups): released, the Hero's Aimed Shot at its target (ReleaseTrueshot).
class spell_titan_rune_trueshot : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_trueshot);

    void Release(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        if (player && player->HasAura(SPELL_RUNE_TRUESHOT))
            ReleaseTrueshot(player, GetHitUnit());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_titan_rune_trueshot::Release, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// The runes' damage over time (Grave Pestilence, Bloodletting, Hollow Flay): each second its kept tick, dealt as
// its own damage spell by whoever put it there.
class spell_titan_rune_dot : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_dot);

    void Tick(AuraEffect const* aurEff)
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        if (!caster || !target || !target->IsAlive())
            return;
        // The tick the script keeps; else the one the aura was cast with (never nothing).
        auto tick = dotTicks.find({ target->GetGUID(), GetId(), caster->GetGUID() });
        int32 damage = std::max(1, tick != dotTicks.end() ? tick->second : aurEff->GetAmount());
        if (Player* player = caster->ToPlayer())
            Trace(player, Acore::StringFormat("{} ticks for {}", GetSpellInfo()->SpellName[0], damage));
        caster->CastCustomSpell(target, DotDamageSpell(GetId()), &damage, nullptr, nullptr, true);
    }

    void Gone(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        // Phoenix Scar: a Cinder Wound running out, 25% of attack power as Fire on its target.
        if ((GetId() == SPELL_CINDER_WOUND || GetId() == SPELL_CINDER_WOUND_SPREAD)
            && GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
            if (Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
                player && HasEmbersMarker(player, MOD_PHOENIX_SCAR) && GetTarget()->IsAlive())
                Strike(player, GetTarget(), SPELL_PHOENIX_SCAR, player->GetTotalAttackPowerValue(BASE_ATTACK)
                    * PHOENIX_SCAR_AP
                    * SchoolBonus(player, SPELL_SCHOOL_MASK_FIRE));
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

// The Spectral Blade's phantom (910124): the Hero's double, in Shadow Dance's purple wisps and a shadow, out of a
// Vanish puff behind the victim; QueuePhantom makes it strike and leave. It holds the Hero's weapons.
struct npc_titan_rune_phantom_assassin : public ScriptedAI
{
    npc_titan_rune_phantom_assassin(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        Player* owner = summoner ? summoner->ToPlayer() : nullptr;
        if (!owner)
            return;
        owner->CastSpell(me, SPELL_CLONE_ME, true);
        for (uint8 i = 0; i < 3; ++i)
        {
            uint8 slot = i == 0 ? EQUIPMENT_SLOT_MAINHAND : i == 1 ? EQUIPMENT_SLOT_OFFHAND : EQUIPMENT_SLOT_RANGED;
            Item* item = owner->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + i, item ? item->GetEntry() : 0);
        }
        me->AddAura(SPELL_PHANTOM_FORM, me);
        me->CastSpell(me, SPELL_PHANTOM_VANISH, true);
        me->SetReactState(REACT_PASSIVE);
    }

    void AttackStart(Unit* /*who*/) override { }
    void UpdateAI(uint32 /*diff*/) override { }
};

// A Hollow Echo (910122): the Hero's look, hollowed, following them (Haunted Ground: where it was called; Possession:
// its target); for 10 sec a weaker Mind Flay on the Hero's target (else the one it was called for, else the
// nearest), 40 yards (Haunted Ground 50); quiet during a barrage. 5% more for each damage over time of the Hero's on
// its target (5 at most); Following Shadows, Haunted Ground, Possession, Silent Witness, Feeding the Void more;
// Moonlit Congregation an Arcane strike on their Moonfire target. Last Word: a last, stronger attack as it fades.
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
        if (!HasHollowMarker(owner, MOD_HAUNTED_GROUND) && !HasHollowMarker(owner, MOD_POSSESSION))
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

    void SetGUID(ObjectGuid const& guid, int32 /*id*/) override
    {
        _target = guid;
    }

    ObjectGuid GetGUID(int32 /*id*/) const override
    {
        return _target;
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
        float range = HOLLOW_RANGE + (HasHollowMarker(hero, MOD_HAUNTED_GROUND) ? HAUNTED_RANGE : 0.0f);
        auto fits = [&](Unit* unit)
        {
            return Valid(hero, unit) && me->IsWithinDistInMap(unit, range) && me->IsWithinLOSInMap(unit);
        };
        if (Unit* chosen = hero->GetSelectedUnit(); Now() >= _quietUntil && fits(chosen))
        {
            _target = chosen->GetGUID();
            return chosen;
        }
        if (Unit* unit = _target ? ObjectAccessor::GetUnit(*me, _target) : nullptr; fits(unit))
            return unit;
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
        // Possession: it follows its target.
        if (HasHollowMarker(hero, MOD_POSSESSION) && _following != target->GetGUID())
        {
            _following = target->GetGUID();
            me->GetMotionMaster()->MoveFollow(target, 2.5f, frand(0.0f, 2.0f * float(M_PI)));
        }
        float tick = FlayTick(hero) * HOLLOW_SHARE * (1.0f + DOT_STEP * float(DotsOn(hero, target)));
        if (HasHollowMarker(hero, MOD_FOLLOWING_SHADOWS))
            tick *= FOLLOWING;
        if (HasHollowMarker(hero, MOD_HAUNTED_GROUND))
            tick *= HAUNTED;
        if (HasHollowMarker(hero, MOD_POSSESSION))
            tick *= POSSESSION;
        if (HasHollowMarker(hero, MOD_SILENT_WITNESS) && hero->isMoving())
            tick *= SILENT_WITNESS;
        if (HasHollowMarker(hero, MOD_FEEDING_THE_VOID))
            if (Spell* drain = hero->GetCurrentSpell(CURRENT_CHANNELED_SPELL); drain
                && drain->m_spellInfo->GetFirstRankSpell()->Id == SPELL_DRAIN_SOUL
                && drain->m_targets.GetUnitTargetGUID() == target->GetGUID())
                tick *= FEEDING;
        int32 damage = std::max<int32>(1, int32(tick));
        me->SetFacingToObject(target);
        SetDotTick(me, target, SPELL_HOLLOW_FLAY, damage);
        Report(hero, SPELL_HOLLOW_FLAY, 'D', damage);
        me->CastCustomSpell(target, SPELL_HOLLOW_FLAY, &damage, nullptr, nullptr, true);
        if (HasHollowMarker(hero, MOD_MOONLIT_CONGREGATION) && Ready(_moonlitAt, 2000)
            && target->GetAuraOfRankedSpell(SPELL_MOONFIRE, hero->GetGUID()))
        {
            _moonlitAt = Now();
            int32 arcane = std::max<int32>(1, int32(RunePower(hero, SPELL_SCHOOL_MASK_ARCANE, SPELL_MIND_BLAST)
                * MOONLIT_POWER));
            me->CastCustomSpell(target, SPELL_MOONLIT_CONGREGATION, &arcane, nullptr, nullptr, true);
        }
    }

    // The distinct damage over time spells of the Hero's on the unit (5 at most).
    static std::size_t DotsOn(Player* hero, Unit* unit)
    {
        std::set<uint32> dots;
        for (AuraType type : { SPELL_AURA_PERIODIC_DAMAGE, SPELL_AURA_PERIODIC_LEECH })
            for (AuraEffect const* effect : unit->GetAuraEffectsByType(type))
                if (effect->GetCasterGUID() == hero->GetGUID())
                    dots.insert(effect->GetId());
        return std::min(dots.size(), MAX_DOTS);
    }

    ObjectGuid _owner, _target, _following;
    uint32 _quietUntil = 0, _moonlitAt = 0;
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
        PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT,
        PLAYERHOOK_ON_LEARN_SPELL,
        PLAYERHOOK_ON_FORGOT_SPELL
    }) { }

    // An ability a rune's taught one stands in for, learned or forgotten: taught again (or not).
    void OnPlayerLearnSpell(Player* player, uint32 spellId) override
    {
        Retaught(player, spellId);
    }

    void OnPlayerForgotSpell(Player* player, uint32 spellId) override
    {
        Retaught(player, spellId);
    }

    static void Retaught(Player* player, uint32 spellId)
    {
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        if (!info || !LegendaryRunes::IsReplacedBase(info->GetFirstRankSpell()->Id) || !player->IsInWorld())
            return;
        TeachDuelist(player, player->HasAura(SPELL_RUNE_DUELIST));
        TeachDawn(player, player->HasAura(SPELL_RUNE_DAWNBOUND_CRUSADER));
        TeachTrueshot(player, player->HasAura(SPELL_RUNE_TRUESHOT));
        TankRunes::Apply(player);
    }

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
        ReleaseRetired(player);
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
        for (uint8 runeSlot = 0; runeSlot < RUNE_SLOTS; ++runeSlot)
            CharacterDatabase.Execute(Acore::StringFormat("DELETE FROM `{}` WHERE `guid` = {}", RuneTable(runeSlot),
                guid.GetCounter()));
    }

    // Every level: the panel's state (its level); at PROMPT_LEVEL the panel opens while the active spec has no rune.
    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (!Wildcard::IsHero(player))
            return;
        Apply(player);
        SendState(player);
        // A slot opening: the panel opens (the second at level 30, with a word of it).
        bool opened = false;
        for (uint8 runeSlot = 0; runeSlot < RUNE_SLOTS; ++runeSlot)
            opened |= oldLevel < PromptLevel(runeSlot) && player->GetLevel() >= PromptLevel(runeSlot);
        if (oldLevel < SECOND_UNLOCK_LEVEL && player->GetLevel() >= SECOND_UNLOCK_LEVEL)
            ChatHandler(player->GetSession()).SendSysMessage("|cffa335eeLegendary Rune:|r your second Legendary "
                "Rune is unlocked. Choose it in the rune panel (Rune II).");
        if (opened && Prompt(player))
            Send(player, "O");
        // Again once the level's other work (Wildcard's rolls, the spec's rebuild) has run.
        ObjectGuid guid = player->GetGUID();
        player->m_Events.AddEventAtOffset([player, guid]()
        {
            if (!player->IsInWorld() || player->GetGUID() != guid)
                return;
            Apply(player);
            SendState(player);
        }, 2s);
    }

    void OnPlayerJustDied(Player* player) override
    {
        ClearCombat(player);
    }

    // Metamorphic Bond on the Hero's demon while they are in Metamorphosis; Storm Within's pulses; the five
    // newest runes' timers.
    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        // A bound rune whose aura went missing (something removed it) is put back, every 5 sec at most.
        State& watch = StateOf(player);
        watch.WatchTimer += diff;
        if (watch.WatchTimer >= RUNE_WATCH_MS)
        {
            watch.WatchTimer = 0;
            if (player->IsAlive() && Wildcard::IsHero(player) && player->GetLevel() >= UNLOCK_LEVEL)
                for (uint8 runeSlot = 0; runeSlot < RUNE_SLOTS; ++runeSlot)
                    if (RuneDef const* bound = FindRune(ActiveChoice(player, runeSlot).Rune); bound
                        && player->GetLevel() >= SlotUnlock(runeSlot) && !player->HasAura(bound->Aura))
                    {
                        Apply(player);
                        SendState(player);
                        break;
                    }
        }
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
        if (player->HasAura(SPELL_RUNE_HUNGERING_RIFT) || StateOf(player).Rift.Until)
            RiftTick(player, diff);
        if (player->HasAura(SPELL_RUNE_HOWLING_NORTH) || StateOf(player).NorthFieldUntil)
            NorthTick(player, diff);
        if (player->HasAura(SPELL_RUNE_WILD_HUNT))
            WrathTick(player, diff);
        if (player->HasAura(SPELL_RUNE_PHOENIX_WAKE) || !StateOf(player).Flights.empty())
            FlightTick(player, diff);
        if (player->HasAura(SPELL_RUNE_ASTRAL_HORIZON) || StateOf(player).Orb || StateOf(player).OrbSatellite)
            OrbTick(player, diff);
        if (player->HasAura(SPELL_RUNE_GUIDING_LIGHT))
            StarTick(player, diff);
        if (player->HasAura(SPELL_RUNE_ASTRAL_CONVERGENCE) || StateOf(player).EventideUntil)
            EventideTick(player, diff);
        if (StateOf(player).PatchUntil)
            HollowPatchTick(player, diff);
        if (player->HasAura(SPELL_RUNE_SPLINTERED_CROWN))
            HailTick(player, diff);
        if (StateOf(player).GuardShield)
            GuardTick(player);
        if (player->HasAura(SPELL_RUNE_STORMREAVER) || StateOf(player).StormfrontTimer)
            StormfrontTick(player, diff);
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

// Arcane Blast (every rank): Arcane Mind's Insight (and Storm Conduit's mark); Astral Horizon, the Orb's victim
// empowers its next bolt.
class spell_titan_rune_arcane_blast : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_arcane_blast);

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;
        if (player->HasAura(SPELL_RUNE_ARCANE_MIND))
        {
            BlastInsight(player);
            MindBlasted(player, target);
        }
        if (player->HasAura(SPELL_RUNE_ASTRAL_HORIZON) && GetHitDamage() > 0)
            HorizonBlast(player, target);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_arcane_blast::Hit);
    }
};

// Arcane Explosion (every rank): Arcane Mind's Expanding Intellect (2 Insight on 3 enemies or more, else 1, once a
// cast) and Arcane Charge; Astral Horizon, the Orb's pulse.
class spell_titan_rune_arcane_explosion : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_arcane_explosion);

    void Hit()
    {
        if (GetHitUnit())
            ++_hits;
    }

    void Done()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return;
        if (player->HasAura(SPELL_RUNE_ASTRAL_HORIZON))
            HorizonExplosion(player);
        if (!player->HasAura(SPELL_RUNE_ARCANE_MIND))
            return;
        MindExplosion(player);
        if (_hits && HasMindMarker(player, MOD_EXPANDING_INTELLECT))
            GainInsight(player, _hits >= EXPANDING_ENEMIES ? 2 : 1);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_arcane_explosion::Hit);
        AfterCast += SpellCastFn(spell_titan_rune_arcane_explosion::Done);
    }

    uint32 _hits = 0;
};

// Arcane Missiles (every rank) with Arcane Mind at 4 Insight or more: empowered (free, its Insight volley). Unbound
// Mind: no channel (its auras prevented; the missiles fly on their own).
class spell_titan_rune_arcane_missiles : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_arcane_missiles);

    void Cast()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetExplTargetUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_ARCANE_MIND) || Insight(player) < INSIGHT_THRESHOLD)
            return;
        _empowered = true;
        _unbound = HasMindMarker(player, MOD_UNBOUND_MIND);
        Empower(player, target, GetSpellInfo());
    }

    // Both its auras (the dummy on the target, the missiles' on the Hero): without them the channel ends at once.
    void NoChannel(SpellMissInfo /*missInfo*/)
    {
        if (_unbound)
            PreventHitAura();
    }

    void Refund()
    {
        if (!_empowered)
            return;
        if (Player* player = GetCaster()->ToPlayer())
            if (int32 cost = GetSpell()->GetPowerCost(); cost > 0)
                player->ModifyPower(POWER_MANA, cost);
    }

    void Register() override
    {
        OnCast += SpellCastFn(spell_titan_rune_arcane_missiles::Cast);
        BeforeHit += BeforeSpellHitFn(spell_titan_rune_arcane_missiles::NoChannel);
        AfterCast += SpellCastFn(spell_titan_rune_arcane_missiles::Refund);
    }

    bool _empowered = false;
    bool _unbound = false;
};

// Arcane Missiles' channel (every rank) with Arcane Mind: an empowered one completed.
class spell_titan_rune_arcane_missiles_channel : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_arcane_missiles_channel);

    void Gone(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* player = GetTarget()->ToPlayer();
        if (!player || !StateOf(player).MindChannel)
            return;
        StateOf(player).MindChannel = false;
        if (GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE && player->HasAura(SPELL_RUNE_ARCANE_MIND))
            MindComplete(player);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_titan_rune_arcane_missiles_channel::Gone, EFFECT_1,
            SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// Frostbolt (every rank) with the Splintered Crown: a direct hit forms an Icicle.
class spell_titan_rune_frostbolt_icicle : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_frostbolt_icicle);

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target && GetHitDamage() > 0 && player->HasAura(SPELL_RUNE_SPLINTERED_CROWN))
            FormIcicle(player, target, float(GetHitDamage()));
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_frostbolt_icicle::Hit);
    }
};

// Pyroblast (every rank) with Phoenix Wake: a direct hit leaves the Ember; the cast spends Ashen Flock's haste.
class spell_titan_rune_pyroblast_ember : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_pyroblast_ember);

    // Its direct hit on the Hero's Phoenix Burn target: 15% more.
    void Burning()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target && GetHitDamage() > 0 && player->HasAura(SPELL_RUNE_PHOENIX_WAKE)
            && target->HasAura(SPELL_PHOENIX_BURN, player->GetGUID()))
            SetHitDamage(int32(float(GetHitDamage()) * BURNING_PYROBLAST));
    }

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target && target->IsAlive() && GetHitDamage() > 0 && player->HasAura(SPELL_RUNE_PHOENIX_WAKE))
            PlaceEmber(player, target, float(GetHitDamage()));
        if (player && target && GetHitDamage() > 0 && player->HasAura(SPELL_RUNE_EMBERS_WRATH))
            EmberPyroblast(player, target);
    }

    void Spent()
    {
        if (Player* player = GetCaster()->ToPlayer())
            player->RemoveAurasDueToSpell(SPELL_ASHEN_FLOCK);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_titan_rune_pyroblast_ember::Burning);
        AfterHit += SpellHitFn(spell_titan_rune_pyroblast_ember::Hit);
        AfterCast += SpellCastFn(spell_titan_rune_pyroblast_ember::Spent);
    }
};

// Fire Blast (every rank) with Phoenix Wake, on the Ember's host: the phoenix.
class spell_titan_rune_fire_blast_phoenix : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_fire_blast_phoenix);

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_PHOENIX_WAKE))
            return;
        State& state = StateOf(player);
        Trace(player, Acore::StringFormat("Fire Blast on {}: the Ember's host {}, its Ember {}", target->GetName(),
            target->GetGUID() == state.EmberHost ? "yes" : "no",
            target->HasAura(SPELL_PHOENIX_EMBER, player->GetGUID()) ? "on it" : "not on it"));
        if (target->GetGUID() != state.EmberHost || !target->HasAura(SPELL_PHOENIX_EMBER, player->GetGUID()))
            return;
        target->RemoveAurasDueToSpell(SPELL_PHOENIX_EMBER, player->GetGUID());
        state.EmberHost.Clear();
        ReleasePhoenix(player, target, state.EmberPyro);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_fire_blast_phoenix::Hit);
    }
};

// Arcane Barrage (every rank): Astral Horizon's Orb; Arcane Mind spends up to 2 Insight, 15% more each and an
// enemy within 8 yards each for 30% of the hit.
class spell_titan_rune_arcane_barrage : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_arcane_barrage);

    void Spend()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player || !GetHitUnit() || GetHitDamage() <= 0 || !player->HasAura(SPELL_RUNE_ARCANE_MIND))
            return;
        _spent = std::min<uint8>(BARRAGE_INSIGHT, Insight(player));
        if (!_spent)
            return;
        SpendInsight(player, _spent);
        SetHitDamage(int32(float(GetHitDamage()) * (1.0f + BARRAGE_STEP * float(_spent))));
    }

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;
        if (player->HasAura(SPELL_RUNE_ASTRAL_HORIZON))
            LaunchOrb(player, target, float(std::max(0, GetHitDamage())));
        uint8 count = _spent;
        for (Unit* enemy : EnemiesAround(player, target, BARRAGE_RANGE))
            if (enemy != target && count)
            {
                Strike(player, enemy, SPELL_INSIGHT_MISSILE, float(GetHitDamage()) * BARRAGE_SPLASH);
                --count;
            }
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_titan_rune_arcane_barrage::Spend);
        AfterHit += SpellHitFn(spell_titan_rune_arcane_barrage::Hit);
    }

    uint8 _spent = 0;
};

// Prayer of Mending (every rank) with Guiding Light: the Guiding Star in place of its prayer (its effects trigger
// the prayer's aura and its jump: neither happens).
class spell_titan_rune_prayer_of_mending : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_prayer_of_mending);

    void Star(SpellEffIndex effIndex)
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* ally = GetHitUnit();
        if (!player || !ally || !player->HasAura(SPELL_RUNE_GUIDING_LIGHT))
            return;
        PreventHitDefaultEffect(effIndex);
        if (effIndex == EFFECT_0 && CanBear(player, ally))
            PlaceStar(player, ally);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_titan_rune_prayer_of_mending::Star, EFFECT_0,
            SPELL_EFFECT_TRIGGER_SPELL_WITH_VALUE);
        OnEffectHitTarget += SpellEffectFn(spell_titan_rune_prayer_of_mending::Star, EFFECT_1,
            SPELL_EFFECT_TRIGGER_SPELL);
    }
};

// Flash Heal and Greater Heal (every rank) with Guiding Light: the star moves to their target.
class spell_titan_rune_guiding_heal : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_guiding_heal);

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* ally = GetHitUnit();
        if (player && ally && player->HasAura(SPELL_RUNE_GUIDING_LIGHT))
            RedirectStar(player, ally, GetSpellInfo()->GetFirstRankSpell()->Id == SPELL_GREATER_HEAL);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_guiding_heal::Hit);
    }
};

// The phoenix (910125) and the Arcane Orb (910126), retail's looks on invisible bodies: they fly where the Hero's
// rune sends them (FlightTick, OrbTick), nothing else.
struct npc_titan_rune_flier : public ScriptedAI
{
    npc_titan_rune_flier(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override { }
    void AttackStart(Unit* /*who*/) override { }
    void MoveInLineOfSight(Unit* /*who*/) override { }
    void UpdateAI(uint32 /*diff*/) override { }
};

// Starfire (every rank, direct hits): Elune's Guardian's owl, Astral Convergence's collision, Arcane Mind's
// Starfall Equation, Astral Horizon's split bolt.
class spell_titan_rune_starfire : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_starfire);

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || GetHitDamage() <= 0)
            return;
        if (player->HasAura(SPELL_RUNE_ELUNES_GUARDIAN))
            CallOwl(player, target, float(GetHitDamage()));
        if (player->HasAura(SPELL_RUNE_ASTRAL_CONVERGENCE))
            ConvergenceStarfire(player, target);
        if (player->HasAura(SPELL_RUNE_ARCANE_MIND))
            MindStarfire(player);
        if (player->HasAura(SPELL_RUNE_ASTRAL_HORIZON))
            HorizonStarfire(player, target);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_starfire::Hit);
    }
};

// Wrath (every rank): Twilight Hunt's 25% more on its marked target (the mark spent); Elune's Guardian's return
// strike on Moonlit Prey; Astral Convergence's Sunlight or flare.
class spell_titan_rune_wrath : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_wrath);

    void Hunted()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !target->HasAura(SPELL_TWILIGHT_HUNT, player->GetGUID()))
            return;
        target->RemoveAurasDueToSpell(SPELL_TWILIGHT_HUNT, player->GetGUID());
        SetHitDamage(int32(GetHitDamage() * TWILIGHT_HUNT));
    }

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || GetHitDamage() <= 0)
            return;
        if (player->HasAura(SPELL_RUNE_ELUNES_GUARDIAN))
            OwlReturn(player, target);
        if (player->HasAura(SPELL_RUNE_ASTRAL_CONVERGENCE))
            ConvergenceWrath(player, target);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_titan_rune_wrath::Hunted);
        AfterHit += SpellHitFn(spell_titan_rune_wrath::Hit);
    }
};

// The Extended Options' other abilities (every rank): their direct hits and heals, to the runes that answer them.
// Lightning Bolt (Astral Convergence's lesser flare; Arcane Mind's Insight, Storm Conduit; Astral Horizon's
// Orbital Lightning), Frostbolt (Frozen Eclipse, Missile Shards, Frostfeather, Winter's Blessing, Frostbound Sermon),
// Shadow Bolt (Devouring Command), Lava Burst and Scorch (Phoenix Wake's Lava Flock, Hot Pursuit), Arcane Shot,
// Aimed Shot and Steady Shot (Distant Spark, Command the Hunt, Hunter's Pursuit), Chain Heal and Holy Shock on an
// ally (Guiding Light's Chain Guidance, Holy Passage).
class spell_titan_rune_extended_hit : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_extended_hit);

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;
        uint32 first = GetSpellInfo()->GetFirstRankSpell()->Id;
        if (player->HasAura(SPELL_RUNE_URSOCS_CHAMPION)
            && (first == SPELL_MANGLE_BEAR || first == SPELL_SWIPE_BEAR || first == TankRunes::SPELL_EARTH_SHOCK
                || first == SPELL_THUNDER_CLAP || (first == SPELL_EARTH_SHIELD_HEAL && target == player)))
            UrsocAbility(player, target, first);
        if (player->HasAura(SPELL_RUNE_SANCTIFIED_PARAGON))
        {
            if (first == SPELL_CHARGE || first == SPELL_DIVINE_SHIELD || first == SPELL_FLASH_HEAL)
                ParagonAbility(player, target, first, float(GetHitHeal()));
            if (first == SPELL_HOLY_SHOCK_HEAL)
                ParagonHolyShock(player, target, float(GetHitHeal()));
            if (GetHitDamage() > 0 && (first == SPELL_JUDGEMENT_OF_LIGHT || first == SPELL_JUDGEMENT_OF_WISDOM
                || first == SPELL_JUDGEMENT_OF_JUSTICE))
                ParagonJudgment(player, target);
            if (GetHitDamage() > 0 && first == SPELL_SHIELD_OF_RIGHTEOUSNESS)
                ParagonRighteous(player, target);
        }
        if ((first == SPELL_REVENGE || first == TankRunes::SPELL_EARTH_SHOCK || first == SPELL_EARTH_SHIELD_HEAL)
            && (first == SPELL_EARTH_SHIELD_HEAL || !GetSpell()->IsTriggered())
            && player->HasAura(SPELL_RUNE_ECHOING_THUNDER))
            ThunderAbility(player, target, first, GetHitHeal());
        if ((first == SPELL_RIPTIDE || first == SPELL_RENEW || first == SPELL_HOLY_SHOCK_HEAL)
            && (first == SPELL_HOLY_SHOCK_HEAL || !GetSpell()->IsTriggered()) && player->HasAura(SPELL_RUNE_WELLSPRING))
            WellTouch(player, target, first);
        if ((first == SPELL_HOLY_SHOCK_HEAL || (first == SPELL_CHAIN_HEAL && target == GetExplTargetUnit()))
            && player->HasAura(SPELL_RUNE_WANDERING_MIST))
            MistTechnique(player, target, first, float(GetHitHeal()));
        if (player->HasAura(SPELL_RUNE_STORMREAVER) && player->IsValidAttackTarget(target)
            && (first == TankRunes::SPELL_EARTH_SHOCK || first == SPELL_FROST_SHOCK || first == SPELL_SHIELD_SLAM
                || first == SPELL_LIGHTNING_BOLT || first == SPELL_THUNDER_CLAP))
            StormAbility(player, target, first);
        if (player->HasAura(SPELL_RUNE_STORMBOUND_CONDUIT) && !GetSpell()->IsTriggered())
            ConduitHit(player, target, first, float(GetHitDamage()));
        if ((first == SPELL_FROSTBOLT || first == SPELL_CHAIN_HEAL || first == SPELL_LAVA_LASH)
            && !GetSpell()->IsTriggered() && player->HasAura(SPELL_RUNE_DAWNBOUND_CRUSADER))
            DawnAbility(player, target, first);
        bool hostile = player->IsValidAttackTarget(target);
        if (first == SPELL_HORN_OF_WINTER && !GetSpell()->IsTriggered() && player->HasAura(SPELL_RUNE_HOWLING_NORTH))
            NorthHorn(player);
        if (hostile && GetHitDamage() > 0 && !GetSpell()->IsTriggered() && player->HasAura(SPELL_RUNE_HUNGERING_RIFT))
            RiftHit(player, target, first);
        if (hostile && !GetSpell()->IsTriggered() && player->HasAura(SPELL_RUNE_HOWLING_NORTH))
            NorthHit(player, target, first);
        if (hostile && player->HasAura(SPELL_RUNE_TRUESHOT))
        {
            if (first == SPELL_AIMED_SHOT && GetSpell()->IsTriggered() && TrueshotPending(player))
                TrueshotLanded(player, target, float(GetHitDamage()));
            else if ((first == SPELL_MULTI_SHOT || first == SPELL_SILENCING_SHOT) && !GetSpell()->IsTriggered())
                TrueshotCharge(player, first);
        }
        if (!GetSpell()->IsTriggered() && player->HasAura(SPELL_RUNE_SCARLET_BENEDICTION))
            ScarletHeal(player, target, first, _opened);
        if (player->HasAura(SPELL_RUNE_EVERGROVE) && player->IsFriendlyTo(target))
        {
            if (first == SPELL_REJUVENATION)
            {
                if (!GetSpell()->IsTriggered())
                    GroveRejuvenation(player, target, _groveBefore, _groveFull);
            }
            else if (first == SPELL_SWIFTMEND)
                GroveOvergrowth(player, target, float(GetHitHeal()));
            else
                GroveHeal(player, target, first, _opened);
        }
        if (!GetSpell()->IsTriggered() && player->HasAura(SPELL_RUNE_IRON_RECKONING))
            IronAbility(player, target, first, float(GetHitDamage()));
        if (first == SPELL_CHAIN_HEAL || first == SPELL_HOLY_SHOCK)
        {
            if (target != player && player->IsFriendlyTo(target) && player->HasAura(SPELL_RUNE_GUIDING_LIGHT))
                GuidingHeal(player, target, first);
            return;
        }
        if (GetHitDamage() <= 0)
            return;
        bool convergence = player->HasAura(SPELL_RUNE_ASTRAL_CONVERGENCE);
        bool mind = player->HasAura(SPELL_RUNE_ARCANE_MIND);
        bool horizon = player->HasAura(SPELL_RUNE_ASTRAL_HORIZON);
        switch (first)
        {
            case SPELL_LIGHTNING_BOLT:
                if (player->HasAura(SPELL_RUNE_SHATTERED_SUN) && !GetSpell()->IsTriggered())
                    SunSpell(player, target, first);
                if (player->HasAura(SPELL_RUNE_WILD_HUNT))
                    HuntStorm(player, target);
                if (convergence)
                    ConvergenceBolt(player, target);
                if (mind)
                    MindBolt(player, target);
                if (horizon)
                    HorizonStrike(player, target, first);
                break;
            case SPELL_ICE_LANCE:
                if (player->HasAura(SPELL_RUNE_SPLINTERED_CROWN))
                    CrownIceLance(player, target);
                break;
            case SPELL_CONE_OF_COLD:
                if (player->HasAura(SPELL_RUNE_SPLINTERED_CROWN) && _cones < CONE_ICICLES)
                {
                    ++_cones;
                    CrownCone(player, target);
                }
                break;
            case SPELL_ICY_TOUCH:
            case SPELL_FIRE_BLAST:
                if (player->HasAura(SPELL_RUNE_SPLINTERED_CROWN))
                    CrownStrike(player, target, first);
                if (first == SPELL_FIRE_BLAST && player->HasAura(SPELL_RUNE_DUELIST))
                    DuelistStrike(player, first);
                break;
            case SPELL_FROSTBOLT:
                if (convergence)
                    ConvergenceFrostbolt(player, target);
                if (mind)
                    MindFrostbolt(player);
                if (player->HasAura(SPELL_RUNE_ELUNES_GUARDIAN))
                    OwlFrostbolt(player, target);
                if (player->HasAura(SPELL_RUNE_HOLLOW_CONGREGATION))
                    HollowStrike(player, target, first);
                if (player->HasAura(SPELL_RUNE_WHITEFANG))
                    FangFrostbolt(player, target);
                break;
            case SPELL_SHADOW_BOLT:
                if (player->HasAura(SPELL_RUNE_HOLLOW_CONGREGATION))
                    HollowStrike(player, target, first);
                break;
            case SPELL_AIMED_SHOT:
            case SPELL_STEADY_SHOT:
                if (player->HasAura(SPELL_RUNE_ELUNES_GUARDIAN))
                    OwlShot(player, target, first);
                break;
            case SPELL_LAVA_BURST:
            case SPELL_SCORCH:
                if (player->HasAura(SPELL_RUNE_PHOENIX_WAKE))
                    PhoenixStrike(player, target, first);
                break;
            case SPELL_ARCANE_SHOT:
                if (player->HasAura(SPELL_RUNE_WILD_HUNT))
                    HuntArcane(player, target);
                if (horizon)
                    HorizonStrike(player, target, first);
                if (player->HasAura(SPELL_RUNE_DUELIST))
                    DuelistStrike(player, first);
                if (player->HasAura(SPELL_RUNE_ELUNES_GUARDIAN))
                    OwlShot(player, target, first);
                break;
            default:
                break;
        }
    }

    // Winter's Blessing: a Frostbolt on Moonfrost, 20% more. The Splintered Crown's Frostbite: a Frostbolt or Ice
    // Lance 8% more (and Gathering Blizzard's Ice Lance).
    void Damage()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target == player && GetHitHeal() > 0 && GetSpellInfo()->Id == SPELL_DEATH_STRIKE_HEAL
            && player->HasAura(SPELL_RUNE_HOWLING_NORTH))
            SetHitHeal(int32(NorthMending(player, float(GetHitHeal()))));
        if (player && target && GetHitHeal() > 0 && player->HasAura(SPELL_RUNE_EVERGROVE))
            SetHitHeal(int32(GroveHealing(player, target, GetSpellInfo()->GetFirstRankSpell()->Id,
                float(GetHitHeal()))));
        if (!player || !target || GetHitDamage() <= 0)
            return;
        uint32 first = GetSpellInfo()->GetFirstRankSpell()->Id;
        if (first == SPELL_REVENGE && !GetSpell()->IsTriggered() && player->HasAura(SPELL_RUNE_IRON_RECKONING))
            SetHitDamage(int32(IronDamage(player, first, float(GetHitDamage()))));
        float damage = float(GetHitDamage());
        if (first == SPELL_FROSTBOLT && player->HasAura(SPELL_RUNE_ELUNES_GUARDIAN))
            damage = OwlFrostboltDamage(player, target, damage);
        if ((first == SPELL_FROSTBOLT || first == SPELL_ICE_LANCE) && player->HasAura(SPELL_RUNE_SPLINTERED_CROWN))
            damage = CrownBoltDamage(player, target, first, damage);
        if ((first == SPELL_FROSTBOLT || first == SPELL_LIGHTNING_BOLT) && !GetSpell()->IsTriggered()
            && player->HasAura(SPELL_RUNE_STORMBOUND_CONDUIT))
            damage = ConduitDamage(player, target, first, damage);
        if (first == SPELL_FROST_STRIKE && !GetSpell()->IsTriggered() && player->HasAura(SPELL_RUNE_HOWLING_NORTH))
            damage = NorthStrikeDamage(player, damage);
        if ((first == SPELL_AIMED_SHOT || first == SPELL_ARCANE_SHOT || first == SPELL_CHIMERA_SHOT)
            && player->HasAura(SPELL_RUNE_TRUESHOT))
            damage = TrueshotDamage(player, target, first, damage, GetSpell()->IsTriggered());
        SetHitDamage(int32(damage));
    }

    // Holy Shock or Chain Heal begun on the Wandering Mist's channel target (it ends the channel).
    SpellCastResult Watch()
    {
        uint32 first = GetSpellInfo()->GetFirstRankSpell()->Id;
        if (Player* player = GetCaster()->ToPlayer(); player && (first == SPELL_HOLY_SHOCK || first == SPELL_CHAIN_HEAL)
            && !GetSpell()->IsTriggered() && player->HasAura(SPELL_RUNE_WANDERING_MIST))
            MistWatch(player);
        return SPELL_CAST_OK;
    }

    // The Evergrove: the Hero's own Rejuvenation about to land (its stacks there before, and whether the ally was
    // at full health: 90% with Gentle Seeking).
    void Before(SpellMissInfo /*missInfo*/)
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || GetSpell()->IsTriggered() || !player->HasAura(SPELL_RUNE_EVERGROVE)
            || GetSpellInfo()->GetFirstRankSpell()->Id != SPELL_REJUVENATION)
            return;
        _groveBefore = GroveStacks(player, target);
        _groveFull = HasGroveMarker(player, MOD_GROVE_GENTLE_SEEKING)
            ? target->GetHealthPct() >= GENTLE_SEEKING_PCT : target->IsFullHealth();
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_titan_rune_extended_hit::Watch);
        BeforeHit += BeforeSpellHitFn(spell_titan_rune_extended_hit::Before);
        OnHit += SpellHitFn(spell_titan_rune_extended_hit::Damage);
        AfterHit += SpellHitFn(spell_titan_rune_extended_hit::Hit);
    }

    uint8 _cones = 0;
    uint8 _opened = 0;                              // Blooms opened, allies grown, by this cast
    uint8 _groveBefore = 0;
    bool _groveFull = false;
};

// Lifebloom's bloom (cast by its ally on itself, the druid its original caster): Lifebound grows a Rejuvenation.
class spell_titan_rune_lifebloom_bloom : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_lifebloom_bloom);

    void Hit()
    {
        Player* player = GetOriginalCaster() ? GetOriginalCaster()->ToPlayer() : nullptr;
        uint8 grown = 0;
        if (player && GetHitUnit() && player->HasAura(SPELL_RUNE_EVERGROVE))
            GroveHeal(player, GetHitUnit(), SPELL_LIFEBLOOM_BLOOM, grown);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_lifebloom_bloom::Hit);
    }
};

// Iron Reckoning's bulwark: only what strikes from before the Hero (an attacker behind them goes through); what it
// absorbs feeds the impact; Immovable's impact as it ends.
class spell_titan_rune_iron_bulwark : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_iron_bulwark);

    void Absorb(AuraEffect* /*aurEff*/, DamageInfo& dmgInfo, uint32& absorbAmount)
    {
        Unit* attacker = dmgInfo.GetAttacker();
        if (attacker && attacker != GetTarget() && !GetTarget()->HasInArc(float(M_PI), attacker))
            absorbAmount = 0;
    }

    void Absorbed(AuraEffect* /*aurEff*/, DamageInfo& /*dmgInfo*/, uint32& absorbAmount)
    {
        if (Player* player = GetTarget()->ToPlayer())
            StateOf(player).IronAbsorbed += float(absorbAmount);
    }

    void Gone(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Player* player = GetTarget()->ToPlayer(); player && player->HasAura(SPELL_RUNE_IRON_RECKONING))
            BulwarkEnded(player);
    }

    void Register() override
    {
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_titan_rune_iron_bulwark::Absorb, EFFECT_0);
        AfterEffectAbsorb += AuraEffectAbsorbFn(spell_titan_rune_iron_bulwark::Absorbed, EFFECT_0);
        AfterEffectRemove += AuraEffectRemoveFn(spell_titan_rune_iron_bulwark::Gone, EFFECT_0,
            SPELL_AURA_SCHOOL_ABSORB, AURA_EFFECT_HANDLE_REAL);
    }
};

// Blizzard's damage (every rank's) with the Splintered Crown: the enemies it hit, for Hailstorm's Icicles.
class spell_titan_rune_blizzard_hit : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_blizzard_hit);

    void Hit()
    {
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (player && GetHitUnit() && player->HasAura(SPELL_RUNE_SPLINTERED_CROWN))
            StateOf(player).BlizzardHit[GetHitUnit()->GetGUID()] = Now();
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_blizzard_hit::Hit);
    }
};

// Evocation (Arcane Mind's Evocative Mind as it begins; Astral Horizon's Evoked Remnant as it completes).
class spell_titan_rune_evocation : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_evocation);

    void Begun(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Player* player = GetTarget()->ToPlayer(); player && player->HasAura(SPELL_RUNE_ARCANE_MIND))
            MindEvocation(player);
    }

    void Done(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* player = GetTarget()->ToPlayer();
        if (player && GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE
            && player->HasAura(SPELL_RUNE_ASTRAL_HORIZON))
            HorizonEvocation(player);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_titan_rune_evocation::Begun, EFFECT_0, SPELL_AURA_ANY,
            AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_titan_rune_evocation::Done, EFFECT_0, SPELL_AURA_ANY,
            AURA_EFFECT_HANDLE_REAL);
    }
};

// Barkskin in Bear Form with Ursoc's Champion: Ursoc's Guard, and Barkskin back 30 sec sooner.
class spell_titan_rune_barkskin : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_barkskin);

    void Cast()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player || !player->HasAura(SPELL_RUNE_URSOCS_CHAMPION) || !InBearForm(player))
            return;
        RaiseGuard(player);
        player->ModifySpellCooldown(SPELL_BARKSKIN, -BARKSKIN_SOONER_MS);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_rune_barkskin::Cast);
    }
};

// Maul (every rank) with Ursoc's Champion: the blow in Ursoc's Guard; in Bear Form, Barkskin 3 sec sooner.
class spell_titan_rune_maul : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_maul);

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player || !player->HasAura(SPELL_RUNE_URSOCS_CHAMPION))
            return;
        MaulRelease(player);
        if (InBearForm(player))
            player->ModifySpellCooldown(SPELL_BARKSKIN, -MAUL_SOONER_MS);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_maul::Hit);
    }
};

// Sabre Slash (the Duelist's Sinister Strike: its known rank's bonus) and Sinister Strike (every rank) with the
// Duelist: the rune's bonuses as it lands, then the rune's part.
class spell_titan_rune_sinister_strike : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_sinister_strike);

    void Bonus()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || GetHitDamage() <= 0)
            return;
        int32 damage = GetHitDamage();
        if (GetSpellInfo()->Id == SPELL_SABRE_SLASH)
            damage += SabreRankBonus(player);
        if (player->HasAura(SPELL_RUNE_DUELIST))
            damage = int32(damage * SabreBonus(player, target));
        SetHitDamage(damage);
    }

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target && GetHitDamage() > 0 && player->HasAura(SPELL_RUNE_DUELIST))
            SabreSlash(player, target, float(GetHitDamage()));
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_titan_rune_sinister_strike::Bonus);
        AfterHit += SpellHitFn(spell_titan_rune_sinister_strike::Hit);
    }
};

// Pistol Shot (the Duelist's action): the shot.
class spell_titan_rune_pistol_shot : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_pistol_shot);

    SpellCastResult Ready()
    {
        Player* player = GetCaster()->ToPlayer();
        return player && player->HasAura(SPELL_RUNE_DUELIST) && player->HasAura(SPELL_OPENING_SHOT)
            ? SPELL_CAST_OK : SPELL_FAILED_CASTER_AURASTATE;
    }

    void Fire(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        if (Unit* target = GetHitUnit(); player && target)
            FirePistol(player, target);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_titan_rune_pistol_shot::Ready);
        OnEffectHitTarget += SpellEffectFn(spell_titan_rune_pistol_shot::Fire, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Broadside (the Duelist's finishing move): the blasts, by the combo points it spends.
class spell_titan_rune_broadside : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_broadside);

    SpellCastResult Bound()
    {
        Player* player = GetCaster()->ToPlayer();
        return player && player->HasAura(SPELL_RUNE_DUELIST) ? SPELL_CAST_OK : SPELL_FAILED_DONT_REPORT;
    }

    void Fire(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;
        if (uint8 points = player->GetComboPoints(target))
            FireBroadside(player, target, points);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_titan_rune_broadside::Bound);
        OnEffectHitTarget += SpellEffectFn(spell_titan_rune_broadside::Fire, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Fire Blast (every rank) and Ember's Wrath: in Cat Form with the rune (its own mana and cooldown; anyone else, in a
// form, refused as the client used to: the rule is gone from both copies of the spell so the client lets it be
// tried), then the ignition.
class spell_titan_rune_embers_fire_blast : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_embers_fire_blast);

    SpellCastResult Check()
    {
        return CatFormGate(GetCaster(), SPELL_RUNE_EMBERS_WRATH);
    }

    void Ignite()
    {
        Player* player = GetCaster()->ToPlayer();
        if (Unit* target = GetHitUnit(); player && target && player->HasAura(SPELL_RUNE_EMBERS_WRATH))
            CatFireBlast(player, target);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_titan_rune_embers_fire_blast::Check);
        AfterHit += SpellHitFn(spell_titan_rune_embers_fire_blast::Ignite);
    }
};

// Cat Form with Ember's Wrath or Whitefang bound: the Flame Cat or the Ice Cat, once the form has set its own look.
class spell_titan_rune_cat_form : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_cat_form);

    void Look(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        RefreshCatLook(GetTarget());
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_titan_rune_cat_form::Look, EFFECT_0, SPELL_AURA_MOD_SHAPESHIFT,
            AURA_EFFECT_HANDLE_REAL);
    }
};

// Flame Shock and Frost Shock (every rank): in Cat Form with their rune (Ember's Wrath, Whitefang), and the next Claw
// or Swipe empowered: Kindled, Frostbitten.
class spell_titan_rune_cat_shock : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_cat_shock);

    bool Fire() { return FirstRank(GetSpellInfo()) == SPELL_FLAME_SHOCK; }

    SpellCastResult Check()
    {
        return CatFormGate(GetCaster(), Fire() ? SPELL_RUNE_EMBERS_WRATH : SPELL_RUNE_WHITEFANG);
    }

    void Empower()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player || !GetHitUnit())
            return;
        if (Fire() && player->HasAura(SPELL_RUNE_EMBERS_WRATH))
            RenewFor(player, SPELL_KINDLED, EMPOWER_MS);
        else if (!Fire() && player->HasAura(SPELL_RUNE_WHITEFANG))
            RenewFor(player, SPELL_FROSTBITTEN, EMPOWER_MS);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_titan_rune_cat_shock::Check);
        AfterHit += SpellHitFn(spell_titan_rune_cat_shock::Empower);
    }
};

// Icy Touch (every rank) and Whitefang: Frozen Prey on its victim (its Frost rune and Frost Fever as ever).
class spell_titan_rune_whitefang_icy_touch : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_whitefang_icy_touch);

    void Mark()
    {
        Player* player = GetCaster()->ToPlayer();
        if (Unit* target = GetHitUnit(); player && target && target->IsAlive() && player->HasAura(SPELL_RUNE_WHITEFANG))
            CatIcyTouch(player, target);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_whitefang_icy_touch::Mark);
    }
};

// Fire Blast's, Flame Shock's and Frost Shock's ranks (the player's): no "not while shapeshifted" rule at load (the
// runes' scripts keep it for everyone but a Cat with the rune: CatFormGate), as the client patch does
// (patch_spells.py, RUNE_FREED).
// The Duelist's Eviscerate with Opening Shot ready: its combo points counted as the cast starts, for the next
// Pistol Shot (DuelistEviscerate).
class titan_runes_duelist_finisher : public PlayerScript
{
public:
    titan_runes_duelist_finisher() : PlayerScript("titan_runes_duelist_finisher", { PLAYERHOOK_ON_SPELL_CAST }) { }

    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (spell->IsTriggered() || spell->GetSpellInfo()->GetFirstRankSpell()->Id != SPELL_EVISCERATE
            || !player->HasAura(SPELL_RUNE_DUELIST) || !player->HasAura(SPELL_OPENING_SHOT))
            return;
        if (uint8 points = player->GetComboPoints())
            DuelistEviscerate(player, points);
    }
};

class titan_runes_load : public GlobalScript
{
public:
    titan_runes_load() : GlobalScript("titan_runes_load", { GLOBALHOOK_ON_LOAD_SPELL_CUSTOM_ATTR }) { }

    void OnLoadSpellCustomAttr(SpellInfo* spellInfo) override
    {
        static constexpr std::array<uint32, 27> FREED = { 2136, 2137, 2138, 8412, 8413, 10197, 10199, 27078, 27079,
            42872, 42873,                                                               // Fire Blast
            8050, 8052, 8053, 10447, 10448, 29228, 25457, 49232, 49233,                 // Flame Shock
            8056, 8058, 10472, 10473, 25464, 49235, 49236 };                            // Frost Shock
        if (std::find(FREED.begin(), FREED.end(), spellInfo->Id) != FREED.end())
            spellInfo->Attributes &= ~SPELL_ATTR0_NOT_SHAPESHIFTED;
    }
};

// Rake, Mangle (Cat), Shred and Claw (every rank): Whitefang's shatter (Shred, Mangle), Ember's Wrath's Blazing Rake
// (Rake), Flamebound's Energy back (Rake, Mangle) and the shocks' empowered Claw.
class spell_titan_rune_cat_strike : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_cat_strike);

    void Refund()
    {
        Player* player = GetCaster()->ToPlayer();
        uint32 first = FirstRank(GetSpellInfo());
        if (player && first == SPELL_SHRED && player->HasAura(SPELL_FLAMEBOUND_DISCOUNT))
        {
            player->RemoveAurasDueToSpell(SPELL_FLAMEBOUND_DISCOUNT);
            player->ModifyPower(POWER_ENERGY, EMBER_REFUND);
        }
    }

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || GetHitDamage() <= 0)
            return;
        uint32 first = FirstRank(GetSpellInfo());
        if (first == SPELL_CLAW)
        {
            if (player->HasAura(SPELL_KINDLED))
                EmpoweredClaw(player, target, float(GetHitDamage()), true);
            else if (player->HasAura(SPELL_FROSTBITTEN))
                EmpoweredClaw(player, target, float(GetHitDamage()), false);
        }
        if ((first == SPELL_SHRED || first == SPELL_MANGLE_CAT) && player->HasAura(SPELL_RUNE_WHITEFANG))
            Shatter(player, target);
        if ((first == SPELL_SHRED || first == SPELL_RAKE) && player->HasAura(SPELL_RUNE_EMBERS_WRATH))
            EmberClawHit(player, target, first);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_rune_cat_strike::Refund);
        AfterHit += SpellHitFn(spell_titan_rune_cat_strike::Hit);
    }
};

// Ferocious Bite (every rank): Whitefang's Cold Blood, Ember's Wrath's Molten Bite.
class spell_titan_rune_ferocious_bite : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_ferocious_bite);

    void Bonus()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || GetHitDamage() <= 0)
            return;
        if (player->HasAura(SPELL_RUNE_WHITEFANG))
            if (uint8 points = player->GetComboPoints(target))
                FangBite(player, target, points);
        if (player->HasAura(SPELL_RUNE_EMBERS_WRATH))
            if (uint8 points = player->GetComboPoints(target))
                EmberBite(player, target, points);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_titan_rune_ferocious_bite::Bonus);
    }
};

// Swipe (Cat): the shocks' empowered Swipe; Ember's Wrath's Cinder Swipe (the wound spreads).
class spell_titan_rune_cat_swipe : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_cat_swipe);

    void Spread()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return;
        if (player->HasAura(SPELL_KINDLED))
            EmpoweredSwipe(player, true);
        else if (player->HasAura(SPELL_FROSTBITTEN))
            EmpoweredSwipe(player, false);
        if (player->HasAura(SPELL_RUNE_EMBERS_WRATH) && HasEmbersMarker(player, MOD_WILDFIRE_SWIPE))
            CinderSwipe(player);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_rune_cat_swipe::Spread);
    }
};

// Stormstrike with the Stormreaver: the Stormfront.
class spell_titan_rune_stormstrike : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_stormstrike);

    void Storm()
    {
        if (Player* player = GetCaster()->ToPlayer(); player && player->HasAura(SPELL_RUNE_STORMREAVER))
            RaiseStormfront(player);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_rune_stormstrike::Storm);
    }
};

// Lava Lash in the Stormreaver's Stormfront: Thunderlash.
class spell_titan_rune_lava_lash : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_lava_lash);

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target && player->HasAura(SPELL_RUNE_STORMREAVER)
            && player->HasAura(SPELL_STORMFRONT, player->GetGUID()))
            Thunderlash(player, target);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_lava_lash::Hit);
    }
};

// The Stormreaver's aura: a melee ability's hit (not the rune's own) discharges Lightning Shield at its target.
class spell_titan_rune_stormreaver : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_stormreaver);

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Player* player = GetTarget()->ToPlayer();
        SpellInfo const* spell = eventInfo.GetSpellInfo();
        Unit* struck = eventInfo.GetActionTarget();
        if (!player || !spell || !struck || struck == player || spell->DmgClass != SPELL_DAMAGE_CLASS_MELEE
            || (spell->Id >= SPELL_BROADSIDE && spell->Id <= SPELL_STORMREAVER_MODIFIER_FIRST + 9))
            return false;
        StormDischarge(player, struck);
        return false;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_titan_rune_stormreaver::CheckProc);
    }
};

// Eviscerate (every rank) with the Spectral Blade: its phantom.
class spell_titan_rune_eviscerate : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_eviscerate);

    void Phantom()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target && player->HasAura(SPELL_RUNE_SPECTRAL_BLADE))
            QueuePhantom(player, target, float(GetHitDamage()));
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_eviscerate::Phantom);
    }
};

// Backstab (every rank; the script still on Sinister Strike and Hemorrhage, which no longer mark) with the
// Spectral Blade: its hit marks the target for 6 sec, for the phantom.
class spell_titan_rune_marked_strike : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_marked_strike);

    void Mark()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target && target->IsAlive() && GetHitDamage() > 0
            && GetSpellInfo()->GetFirstRankSpell()->Id == SPELL_BACKSTAB && player->HasAura(SPELL_RUNE_SPECTRAL_BLADE))
            Lasting(player, target, SPELL_MARKED_FOR_MURDER, BACKSTAB_MARK_MS);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_marked_strike::Mark);
    }
};

// Bestial Wrath with the Wild Hunt: Kill Command at most 5 sec away, and a beast a second while it lasts.
class spell_titan_rune_bestial_wrath : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_bestial_wrath);

    void Hunt()
    {
        Player* player = GetCaster()->ToPlayer();
        if (player && player->HasAura(SPELL_RUNE_WILD_HUNT))
            BestialWrath(player);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_rune_bestial_wrath::Hunt);
    }
};

// Kill Command with the Wild Hunt: a Spectral Beast.
class spell_titan_rune_kill_command : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_kill_command);

    void Hunt()
    {
        Player* player = GetCaster()->ToPlayer();
        if (player && player->HasAura(SPELL_RUNE_WILD_HUNT))
            KillCommand(player);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_rune_kill_command::Hunt);
    }
};

// The Wild Hunt's Spectral Beast (910123): the Hero's pet's look, ghostly, no threat; for 10 sec (Kill Command's)
// or 4 (Bestial Wrath's) it chases its prey and bites every 1.5 sec for 60% of the pet's swing (Alpha Predator 30%
// more; Pack Ravager: 2 more enemies within 5 yards at half; Ghostfang: a refreshing bleed of a quarter of a bite a
// second).
// Its prey dead, it goes (Blood Scent: on to the Hero's target, else the nearest enemy); Hunter's Command: it turns
// to the Hero's target. Its natural end: Final Pounce (a bite at 200%), Frenzied Farewell (4 quick bites at 60%)
// or Parting Howl (the Hero's pet 30% stronger for 6 sec).
struct npc_titan_rune_spectral_beast : public ScriptedAI
{
    npc_titan_rune_spectral_beast(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        Player* owner = summoner ? summoner->ToPlayer() : nullptr;
        Pet* pet = owner ? owner->GetPet() : nullptr;
        if (!owner || !pet)
            return;
        _owner = owner->GetGUID();
        me->SetDisplayId(pet->GetDisplayId());
        me->SetNativeDisplayId(pet->GetDisplayId());
        me->SetObjectScale(pet->GetObjectScale());
        me->AddAura(SPELL_SPECTRAL_BEAST, me);
        me->SetReactState(REACT_PASSIVE);
        float swing = (pet->GetFloatValue(UNIT_FIELD_MINDAMAGE) + pet->GetFloatValue(UNIT_FIELD_MAXDAMAGE)) / 2.0f
            * pet->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, SPELL_SCHOOL_MASK_NORMAL);
        _bite = std::max(swing, 2.0f * owner->GetLevel()) * BITE_SHARE;
        scheduler.Schedule(300ms, [this](TaskContext bite)
        {
            Hunt();
            bite.Repeat(Milliseconds(BITE_MS));
        });
    }

    // Its lifetime (Unleash): the farewell just before it ends.
    void SetData(uint32 /*id*/, uint32 ms) override
    {
        scheduler.Schedule(Milliseconds(ms - FAREWELL_BITES * FAREWELL_SPACING_MS - 200), [this](TaskContext)
        {
            Farewell();
        });
    }

    void SetGUID(ObjectGuid const& guid, int32 /*id*/) override
    {
        _prey = guid;
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

    Unit* Prey(Player* hero)
    {
        auto fits = [&](Unit* unit) { return Valid(hero, unit) && me->IsWithinDistInMap(unit, BEAST_RANGE); };
        if (HasHuntMarker(hero, MOD_HUNTERS_COMMAND))
            if (Unit* chosen = hero->GetSelectedUnit(); fits(chosen))
            {
                _prey = chosen->GetGUID();
                return chosen;
            }
        if (Unit* prey = _prey ? ObjectAccessor::GetUnit(*me, _prey) : nullptr; fits(prey))
            return prey;
        if (!HasHuntMarker(hero, MOD_BLOOD_SCENT))
            return nullptr;
        Unit* next = hero->GetSelectedUnit();
        if (!fits(next))
        {
            std::vector<Unit*> enemies = EnemiesAround(hero, me, BEAST_RANGE / 2.0f);
            next = enemies.empty() ? nullptr : enemies.front();
        }
        if (next)
            _prey = next->GetGUID();
        return next;
    }

    // A strike of the beast's own (its amount set).
    void Smite(Unit* prey, uint32 spellId, float amount)
    {
        int32 damage = std::max<int32>(1, int32(amount));
        me->CastCustomSpell(prey, spellId, &damage, nullptr, nullptr, true);
    }

    void Bite(Player* hero, Unit* prey, float share, uint32 spellId = SPELL_SPECTRAL_BITE)
    {
        bool marked = prey->GetAuraOfRankedSpell(SPELL_HUNTERS_MARK, hero->GetGUID()) != nullptr;
        float amount = _bite * share * (HasHuntMarker(hero, MOD_ALPHA_PREDATOR) ? ALPHA_PREDATOR : 1.0f)
            * (HasHuntMarker(hero, MOD_FIXED_PREY) ? FIXED_PREY : 1.0f) * (marked ? HUNT_MARK : 1.0f)
            * HuntCharge(hero, prey);
        me->SetFacingToObject(prey);
        me->HandleEmoteCommand(EMOTE_ONESHOT_ATTACK1H);
        Smite(prey, spellId, amount);
        if (float nature = HuntStormCharge(hero); nature > 0.0f)
            Smite(prey, SPELL_STORM_HUNT, nature);
        if (HasHuntMarker(hero, MOD_FROSTFANG) && prey->HasAura(SPELL_FROST_FEVER, hero->GetGUID()))
            Smite(prey, SPELL_FROSTFANG, hero->GetTotalAttackPowerValue(RANGED_ATTACK) * FROSTFANG_RAP
                * SchoolBonus(hero, SPELL_SCHOOL_MASK_FROST));
        if (marked && HasHuntMarker(hero, MOD_MARKED_ALPHA))
            for (Unit* enemy : EnemiesAround(hero, prey, RAVAGER_RADIUS))
                if (enemy != prey)
                {
                    Smite(enemy, SPELL_SPECTRAL_BITE, amount * MARKED_ALPHA_SHARE);
                    break;
                }
        if (HasHuntMarker(hero, MOD_PACK_RAVAGER))
        {
            int32 cleave = std::max<int32>(1, int32(amount * RAVAGER_SHARE));
            std::size_t count = 0;
            for (Unit* enemy : EnemiesAround(hero, prey, RAVAGER_RADIUS))
                if (enemy != prey && count++ < RAVAGER_TARGETS)
                    me->CastCustomSpell(enemy, SPELL_SPECTRAL_BITE, &cleave, nullptr, nullptr, true);
        }
        if (HasHuntMarker(hero, MOD_GHOSTFANG))
        {
            int32 tick = std::max<int32>(1, int32(amount * GHOSTFANG_SHARE));
            Lasting(hero, prey, SPELL_GHOSTFANG, GHOSTFANG_MS, &tick);
        }
    }

    void Hunt()
    {
        Player* hero = Owner();
        Unit* prey = hero ? Prey(hero) : nullptr;
        if (!prey)
        {
            me->DespawnOrUnsummon();
            return;
        }
        if (_chasing != prey->GetGUID())
        {
            _chasing = prey->GetGUID();
            me->SetTarget(_chasing);
            me->GetMotionMaster()->MoveChase(prey);
        }
        if (me->IsWithinMeleeRange(prey))
            Bite(hero, prey, 1.0f);
    }

    void Farewell()
    {
        Player* hero = Owner();
        if (!hero)
            return;
        if (HasHuntMarker(hero, MOD_PARTING_HOWL))
        {
            if (Pet* pet = hero->GetPet(); pet && pet->IsAlive())
            {
                pet->RemoveAurasDueToSpell(SPELL_PARTING_HOWL);
                pet->CastSpell(pet, SPELL_PARTING_HOWL, true);
            }
            return;
        }
        Unit* prey = Prey(hero);
        if (HasHuntMarker(hero, MOD_MARKED_FINALE))
        {
            Unit* marked = prey && prey->GetAuraOfRankedSpell(SPELL_HUNTERS_MARK, hero->GetGUID()) ? prey
                : hero->GetSelectedUnit();
            if (!Valid(hero, marked) || !marked->GetAuraOfRankedSpell(SPELL_HUNTERS_MARK, hero->GetGUID())
                || !me->IsWithinDistInMap(marked, BEAST_RANGE))
                return;
            float pounce = hero->GetTotalAttackPowerValue(RANGED_ATTACK) * MARKED_FINALE_RAP
                * SchoolBonus(hero, SPELL_SCHOOL_MASK_NORMAL);
            me->SetFacingToObject(marked);
            Smite(marked, SPELL_FINAL_POUNCE, pounce);
            std::size_t count = 0;
            for (Unit* enemy : EnemiesAround(hero, marked, RAVAGER_RADIUS))
                if (enemy != marked && count++ < RAVAGER_TARGETS)
                    Smite(enemy, SPELL_SPECTRAL_BITE, pounce * MARKED_FINALE_SHARE);
            return;
        }
        if (HasHuntMarker(hero, MOD_HEALING_COMPANION))
        {
            Pet* pet = hero->GetPet();
            if (!pet || !pet->IsAlive() || !pet->GetAuraOfRankedSpell(SPELL_MEND_PET, hero->GetGUID()))
                return;
            int32 heal = std::max<int32>(1, int32(pet->CountPctFromMaxHealth(int32(COMPANION_HEAL_PCT))));
            hero->CastCustomSpell(pet, SPELL_HEALING_COMPANION, &heal, nullptr, nullptr, true);
            if (prey)
                Smite(prey, SPELL_SPECTRAL_BITE, hero->GetTotalAttackPowerValue(RANGED_ATTACK) * COMPANION_RAP
                    * SchoolBonus(hero, SPELL_SCHOOL_MASK_NORMAL));
            return;
        }
        if (!prey)
            return;
        if (HasHuntMarker(hero, MOD_FINAL_POUNCE))
            Bite(hero, prey, POUNCE, SPELL_FINAL_POUNCE);
        else if (HasHuntMarker(hero, MOD_FRENZIED_FAREWELL))
            for (uint32 i = 0; i < FAREWELL_BITES; ++i)
                scheduler.Schedule(Milliseconds(i * FAREWELL_SPACING_MS), [this](TaskContext)
                {
                    Player* owner = Owner();
                    if (Unit* at = owner ? Prey(owner) : nullptr)
                        Bite(owner, at, FAREWELL_SHARE);
                });
    }

    ObjectGuid _owner, _prey, _chasing;
    float _bite = 1.0f;
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
    RegisterSpellScript(spell_titan_rune_trueshot);
    RegisterSpellScript(spell_titan_rune_lifebloom_bloom);
    RegisterSpellScript(spell_titan_rune_iron_bulwark);
    RegisterCreatureAI(npc_titan_rune_hollow_echo);
    RegisterCreatureAI(npc_titan_rune_phantom_assassin);
    RegisterCreatureAI(npc_titan_rune_felguard);
    RegisterCreatureAI(npc_titan_rune_fel_imp);
    RegisterSpellScript(spell_titan_rune_eviscerate);
    RegisterSpellScript(spell_titan_rune_marked_strike);
    RegisterSpellScript(spell_titan_rune_kill_command);
    RegisterSpellScript(spell_titan_rune_bestial_wrath);
    RegisterSpellScript(spell_titan_rune_arcane_blast);
    RegisterSpellScript(spell_titan_rune_arcane_explosion);
    RegisterSpellScript(spell_titan_rune_arcane_missiles);
    RegisterSpellScript(spell_titan_rune_arcane_missiles_channel);
    RegisterSpellScript(spell_titan_rune_frostbolt_icicle);
    RegisterSpellScript(spell_titan_rune_pyroblast_ember);
    RegisterSpellScript(spell_titan_rune_fire_blast_phoenix);
    RegisterSpellScript(spell_titan_rune_arcane_barrage);
    RegisterSpellScript(spell_titan_rune_prayer_of_mending);
    RegisterSpellScript(spell_titan_rune_guiding_heal);
    RegisterSpellScript(spell_titan_rune_starfire);
    RegisterSpellScript(spell_titan_rune_wrath);
    RegisterSpellScript(spell_titan_rune_extended_hit);
    RegisterSpellScript(spell_titan_rune_blizzard_hit);
    RegisterSpellScript(spell_titan_rune_evocation);
    RegisterSpellScript(spell_titan_rune_barkskin);
    RegisterSpellScript(spell_titan_rune_maul);
    RegisterSpellScript(spell_titan_rune_sinister_strike);
    RegisterSpellScript(spell_titan_rune_pistol_shot);
    RegisterSpellScript(spell_titan_rune_broadside);
    RegisterSpellScript(spell_titan_rune_stormstrike);
    RegisterSpellScript(spell_titan_rune_lava_lash);
    RegisterSpellScript(spell_titan_rune_stormreaver);
    RegisterSpellScript(spell_titan_rune_embers_fire_blast);
    RegisterSpellScript(spell_titan_rune_whitefang_icy_touch);
    RegisterSpellScript(spell_titan_rune_cat_shock);
    new titan_runes_load();
    new titan_runes_duelist_finisher();
    RegisterSpellScript(spell_titan_rune_cat_strike);
    RegisterSpellScript(spell_titan_rune_ferocious_bite);
    RegisterSpellScript(spell_titan_rune_cat_swipe);
    RegisterSpellScript(spell_titan_rune_cat_form);
    RegisterCreatureAI(npc_titan_rune_spectral_beast);
    RegisterCreatureAI(npc_titan_rune_flier);
    new titan_runes_player();
    new titan_runes_unit();
}
