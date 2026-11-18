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
// 31 the Shattered Sun; then 32 the Spectral Blade (its doc: Phantom Edge) and 33 the Wild Hunt; then five from their
// own dossiers: 34 Arcane Mind, 35 the Splintered Crown, 36 Phoenix Wake, 37 Astral Horizon, 38 Guiding Light; and
// three druid runes: 39 Elune's Guardian, 40 Astral Convergence, 41 Ursoc's Champion.
//
// - Unlocked at level 5: the panel (the Emberforge_Runes addon, addon whispers with prefix EFRN) opens by itself
//   while the active specialization has no rune, suggesting up to 3 the Hero's abilities can use, with every rune
//   under Browse all (one missing a required ability says so; it can still be bound after a warning naming what it
//   lacks), each with a Preview of its build that binds nothing.
// - Two runes per specialization, each with its own three modifiers: the first from level 5 (titan_runes,
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
// Protocol (fields by ','; the client sends STATE, PICK,<rune>,<anyway 0/1>,<rune slot 1-2>, MOD,<slot 1-3>,
// <choice 1-3>,<rune slot 1-2>; anyway 1: warned of the abilities the rune needs and lacks, the Hero binds it anyway;
// no rune slot: the first):
//   S,<level>,<unlock level>,<spec>,<rune>,<m1>,<m2>,<m3>,<offers a.b.c or ->,<prompt 0/1>,<second rune>,<m1>,<m2>,
//     <m3>,<second unlock level>      (offers: up to 4, the panel leaving out the other slot's rune)
//   R,<rune>,<missing>      an implemented rune and the abilities it lacks (groups by '/', one of by '+'; '-' none)
//   N,<rune>,<slot>,<choice>,<missing>      a modifier with needs of its own
//   E,<text>   O (open the panel)
namespace
{
    constexpr char const* PREFIX = "EFRN";
    constexpr uint8 UNLOCK_LEVEL = 5;
    constexpr uint8 RUNE_SLOTS = 2;                 // two Legendary Runes a specialization, each with its modifiers
    constexpr uint8 SECOND_UNLOCK_LEVEL = 30;       // the second, at full power
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
        SPELL_BATTLEFIELD_CLAIM         = 90866,    // Battlefield Claim's mark: slowed, more damage from the Hero
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

        SPELL_RUNE_SPECTRAL_BLADE       = 90980,
        SPELL_PHANTOM_STRIKE            = 90981,    // cannot crit
        SPELL_OPEN_WOUND                = 90983,
        SPELL_OPEN_WOUND_TICK           = 90984,
        SPELL_VANISHING_ACT             = 90985,
        SPELL_MARKED_FOR_MURDER         = 90986,
        SPELL_PHANTOM_RUSH              = 90987,    // Energy regenerating twice as fast, 4 sec
        SPELL_PHANTOM_FORM              = 90988,    // on the phantom: Shadow Dance's wisps, a shadow at its feet
        SPELL_PHANTOM_VANISH            = 90989,    // the phantom arriving and leaving: Vanish's puff
        SPELL_BLADE_MODIFIER_FIRST      = 91120,    // + slot * 3 + choice

        SPELL_RUNE_WILD_HUNT            = 91130,
        SPELL_SPECTRAL_BEAST            = 91131,    // on the beast: its look, no threat
        SPELL_SPECTRAL_BITE             = 91132,
        SPELL_GHOSTFANG                 = 91133,
        SPELL_GHOSTFANG_TICK            = 91134,
        SPELL_PARTING_HOWL              = 91135,    // on the Hero's pet
        SPELL_FINAL_POUNCE              = 91136,
        SPELL_HUNT_MODIFIER_FIRST       = 91150,    // + slot * 3 + choice

        SPELL_RUNE_ARCANE_MIND          = 91200,
        SPELL_ARCANE_INSIGHT            = 91201,
        SPELL_INSIGHT_MISSILE           = 91202,
        SPELL_ARCANE_ECHO               = 91203,
        SPELL_ARCANE_ECHO_TICK          = 91204,
        SPELL_MIND_UNBOUND              = 91205,    // Insight 4 or more: its look
        SPELL_MENTAL_RESERVES           = 91206,
        SPELL_UNBOUND_MISSILE           = 91207,    // Unbound Mind: an Arcane Missile without its channel
        SPELL_MIND_MODIFIER_FIRST       = 91210,    // + slot * 3 + choice

        SPELL_RUNE_SPLINTERED_CROWN     = 91220,
        SPELL_ICICLE                    = 91221,
        SPELL_FROST_CROWN               = 91222,    // on the Hero: a stack an Icicle waiting
        SPELL_SPLINTER_QUICKENING       = 91223,
        SPELL_FROST_WOUND               = 91224,
        SPELL_FROST_WOUND_TICK          = 91225,
        SPELL_WINTERS_GRASP             = 91226,
        SPELL_COLD_FRONT                = 91227,
        SPELL_ICE_SPLINTER              = 91228,    // a fragment, a pierce, a seeking shard
        SPELL_CROWN_MODIFIER_FIRST      = 91230,    // + slot * 3 + choice

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
        SPELL_PHOENIX_MODIFIER_FIRST    = 91250,    // + slot * 3 + choice
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
        SPELL_HORIZON_MODIFIER_FIRST    = 91270,    // + slot * 3 + choice

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
        SPELL_GUIDING_MODIFIER_FIRST    = 91290,    // + slot * 3 + choice

        SPELL_RUNE_ELUNES_GUARDIAN      = 91300,
        SPELL_OWL_DIVE                  = 91301,
        SPELL_LUNAR_WOUND               = 91302,
        SPELL_LUNAR_WOUND_TICK          = 91303,
        SPELL_SWIFT_JUDGMENT            = 91304,    // the next Starfire 10% faster
        SPELL_WATCHFUL_FLIGHT           = 91305,    // the next Starfire 20% cheaper
        SPELL_GUIDING_WINGS             = 91306,    // the next Starfire instant
        SPELL_GUIDING_WINGS_SPEED       = 91307,
        SPELL_TWILIGHT_HUNT             = 91308,    // on the owl's victim: the next Wrath 25% more
        SPELL_OWL_MODIFIER_FIRST        = 91310,    // + slot * 3 + choice

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
        SPELL_CONVERGENCE_MODIFIER_FIRST = 91330,   // + slot * 3 + choice
        SPELL_EVENTIDE_TICK             = 91339,

        SPELL_RUNE_URSOCS_CHAMPION      = 91340,
        SPELL_URSOCS_GUARD              = 91341,
        SPELL_URSOC_BLOW                = 91342,
        SPELL_URSOC_BLOW_LOOK           = 91343,
        SPELL_LASTING_STAND             = 91344,
        SPELL_MOVING_MOUNTAIN           = 91345,
        SPELL_ENDURING_CHAMPION         = 91346,
        SPELL_CHALLENGERS_ROAR          = 91347,
        SPELL_URSOC_MODIFIER_FIRST      = 91350,    // + slot * 3 + choice
        SPELL_URSOC_ROAR_LOOK           = 91359,
        SPELL_COLLISION_LOOK            = 91360,    // Astral Rupture: the collision's look, its damage over time

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
        SPELL_ADVANCE                   = 91386,    // speed
        SPELL_DISENGAGING_SHOT          = 91387,    // speed
        SPELL_DISENGAGING_DISCOUNT      = 91388,    // the next Pistol Shot 10 Energy cheaper
        SPELL_PRESS_THE_ADVANTAGE       = 91389,    // the next Sinister Strike strikes twice
        SPELL_DUELIST_MODIFIER_FIRST    = 91390,    // + slot * 3 + choice
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
        SPELL_STORMREAVER_MODIFIER_FIRST = 91410,   // + slot * 3 + choice
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
        SPELL_EMBERS_MODIFIER_FIRST     = 91431,    // + slot * 3 + choice
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
        SPELL_WHITEFANG_MODIFIER_FIRST  = 91451,    // + slot * 3 + choice
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
    // Slam releases the echo: 42% of its damage before mitigation to each enemy within 6 yards of its target (up to
    // 5).
    constexpr int32 CHARGE_MS = 8000, LINGERING_CHARGE_MS = 20000;
    constexpr uint8 DOUBLE_CHARGES = 2;
    constexpr float ECHO_SHARE = 0.42f, ECHO_RADIUS = 6.0f, DISTANT_RADIUS = 10.0f, FOCUSED_ECHO = 1.6f;
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

    // The Spectral Blade's modifiers: slot (0 Execution Style, 1 Timing, 2 Aftermath) x 3 + choice.
    enum BladeModifier : uint8
    {
        MOD_EXECUTIONER, MOD_THOUSAND_CUTS, MOD_SHADOW_CLEAVE,
        MOD_QUICK_DEATH, MOD_PATIENT_KILLER, MOD_OPEN_WOUND,
        MOD_DEATH_FOLLOWS, MOD_VANISHING_ACT, MOD_MARKED_FOR_MURDER,
    };

    // The Spectral Blade's tuning: Eviscerate's phantom strikes for twice its damage, 0.6 sec later; it cannot crit
    // and triggers nothing (a spell of its own). Each phantom's strike: Phantom Rush, Energy regenerating twice as
    // fast for 4 sec (its own spell data).
    constexpr uint32 PHANTOM_RUSH_MS = 4000;
    constexpr float PHANTOM_SHARE = 2.0f, EXECUTIONER = 1.4f, THOUSAND_CUTS = 1.15f, CLEAVE_SHARE = 0.5f;
    constexpr float PATIENT_KILLER = 1.4f, OPEN_WOUND = 1.2f, MARKED_BONUS = 1.3f;
    constexpr float CLEAVE_RADIUS = 8.0f, FOLLOW_RANGE = 10.0f;
    constexpr uint32 PHANTOM_DELAY_MS = 600, QUICK_DEATH_MS = 100, PATIENT_KILLER_MS = 2000, CUT_SPACING_MS = 200;
    constexpr uint32 CUTS = 3;
    constexpr int32 OPEN_WOUND_MS = 4000, OPEN_WOUND_TICKS = 4, VANISHING_ACT_MS = 3000, MARKED_MS = 10000;
    constexpr std::size_t CLEAVE_TARGETS = 3;

    // The Wild Hunt's modifiers: slot (0 Spectral Beast, 1 Hunting Behavior, 2 Final Attack) x 3 + choice.
    enum HuntModifier : uint8
    {
        MOD_ALPHA_PREDATOR, MOD_PACK_RAVAGER, MOD_GHOSTFANG,
        MOD_UNENDING_WRATH, MOD_HUNTERS_COMMAND, MOD_BLOOD_SCENT,
        MOD_FINAL_POUNCE, MOD_FRENZIED_FAREWELL, MOD_PARTING_HOWL,
    };

    // The Wild Hunt's tuning: a Spectral Beast bites every 1.5 sec for 60% of the Hero's pet's swing, for 10 sec (a
    // Kill Command's) or 4 sec (Bestial Wrath's: one a second for 10 sec); 8 out at most. In Bestial Wrath, Kill
    // Command comes back in 5 sec; Unending Wrath: each one there adds 2 sec to it, 10 sec more at most.
    constexpr uint32 BEAST_MS = 10000, WRATH_BEAST_MS = 4000, WRATH_CALL_MS = 1000, WRATH_KILL_COMMAND_MS = 5000;
    constexpr int32 UNENDING_MS = 2000, UNENDING_MOST_MS = 10000;
    constexpr std::size_t MAX_BEASTS = 8;
    constexpr uint32 BITE_MS = 1500, FAREWELL_SPACING_MS = 150;
    constexpr uint32 FAREWELL_BITES = 4;
    constexpr float BITE_SHARE = 0.6f, ALPHA_PREDATOR = 1.3f, RAVAGER_SHARE = 0.5f, GHOSTFANG_SHARE = 0.25f;
    constexpr float POUNCE = 2.0f, FAREWELL_SHARE = 0.6f, BEAST_RANGE = 40.0f, RAVAGER_RADIUS = 5.0f;
    constexpr int32 GHOSTFANG_MS = 4000;
    constexpr std::size_t RAVAGER_TARGETS = 2;

    // Arcane Mind's modifiers: slot (0 Invocation, 1 Manifestation, 2 Adaptation) x 3 + choice.
    enum MindModifier : uint8
    {
        MOD_ACCELERATED_THOUGHT, MOD_EXPANDING_INTELLECT, MOD_PATIENT_CALCULATION,
        MOD_FOCUSED_INTENTION, MOD_FRACTURED_THOUGHT, MOD_LINGERING_EQUATION,
        MOD_UNBOUND_MIND, MOD_MENTAL_RESERVES, MOD_OVERCLOCKED_INTELLECT,
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

    // The Splintered Crown's modifiers: slot (0 Formation, 1 Flight, 2 Aftermath) x 3 + choice.
    enum CrownModifier : uint8
    {
        MOD_GLACIAL_SPEAR, MOD_SPLINTER_VOLLEY, MOD_SHATTERGLASS,
        MOD_IMPALING_ICE, MOD_SEEKING_SPLINTERS, MOD_FROZEN_FOCUS,
        MOD_FROST_WOUND, MOD_WINTERS_GRASP, MOD_COLD_FRONT,
    };

    // The Splintered Crown's tuning: a direct Frostbolt hit forms an Icicle (5 waiting at most), loosed 1 sec later
    // at the same victim for 30% of the Frostbolt's damage. Its options keep the dossier's ratios to an Icicle.
    constexpr uint8 MAX_ICICLES = 5, FROZEN_FOCUS_EVERY = 3, AFTERMATH_EVERY = 3;
    constexpr uint32 ICICLE_DELAY_MS = 1000, SPLINTERS = 3, SPLINTER_SPACING_MS = 200;
    constexpr float ICICLE_SHARE = 0.3f, GLACIAL_SPEAR = 1.4f, SPLINTER_SHARE = 0.4f, SHATTERGLASS_SHARE = 0.5f;
    constexpr float FROZEN_FOCUS = 1.6f, IMPALE_SHARE = 0.4f, SEEKING_SHARE = 0.3f, SEEKING_SELF_SHARE = 0.15f;
    constexpr float FROST_WOUND_SHARE = 0.8f, WINTERS_GRASP = 1.15f;
    constexpr float ICICLE_RANGE = 45.0f, ICICLE_SPEED = 30.0f, SHATTERGLASS_RADIUS = 6.0f, SEEKING_RANGE = 10.0f;
    constexpr float IMPALE_REACH = 15.0f, IMPALE_HALF_WIDTH = 2.0f;
    constexpr int32 FROST_WOUND_MS = 6000, WINTERS_GRASP_MS = 3000;
    constexpr std::size_t SHATTERGLASS_TARGETS = 4, IMPALE_TARGETS = 3;

    // Phoenix Wake's modifiers: slot (0 Phoenix, 1 Flight, 2 Wake) x 3 + choice.
    enum PhoenixModifier : uint8
    {
        MOD_SUN_TALON, MOD_BROADWING, MOD_ASHEN_FLOCK,
        MOD_HOMEWARD_FLAME, MOD_DEATH_SPIRAL, MOD_WILD_FLIGHT,
        MOD_SCORCHED_EARTH, MOD_CLINGING_EMBERS, MOD_REBIRTH,
    };

    // Phoenix Wake's tuning: the Ember holds its Pyroblast's damage (8 sec); the phoenix dives from 8 yards above
    // the Hero onto the host at 20 yards a second, burning up to 4 enemies under its path (3 yards wide) for 25%,
    // and strikes the host for 50%.
    constexpr int32 EMBER_MS = 8000, CLINGING_MS = 6000;
    constexpr uint32 PHOENIX_MAX_MS = 8000, SPIRAL_MS = 2000, REPATH_MS = 300, HOMEWARD_MS = 2000;
    constexpr uint32 SCORCH_MS = 4000, SCORCH_TICK_MS = 1000, FLOCK_BIRDS = 3;
    constexpr float PHOENIX_HOST = 0.5f, PHOENIX_CROSS = 0.25f, SUN_TALON = 1.5f, BROADWING_CROSS = 0.3f;
    constexpr float SPIRAL_SHARE = 0.25f, CLINGING_SHARE = 0.25f, REBIRTH_SHARE = 0.35f, SCORCH_SHARE = 0.06f;
    constexpr float PHOENIX_HALF_WIDTH = 1.5f, BROADWING_HALF_WIDTH = 3.0f;
    constexpr float PHOENIX_SPEED = 20.0f, HOMEWARD_SPEED = 40.0f, PHOENIX_HEIGHT = 2.0f;
    constexpr float FLOCK_SPREAD = 2.0f, FLOCK_SCALE = 0.6f, SPIRAL_RADIUS = 3.0f;
    constexpr float WILD_FLIGHT_RANGE = 10.0f, VISIT_REACH = 2.0f, STRIKE_REACH = 3.0f, REBIRTH_RADIUS = 6.0f;
    constexpr float PHOENIX_DROP = 8.0f, PHOENIX_BEHIND = 2.0f;     // where it appears: above, behind the Hero
    constexpr float SCORCH_SPACING = 4.0f, SCORCH_HALF_WIDTH = 1.5f, SCORCH_REACH = 60.0f;
    constexpr std::size_t CROSS_TARGETS = 4, WILD_FLIGHT_VISITS = 2, REBIRTH_TARGETS = 5, SCORCH_TARGETS = 5;

    // Astral Horizon's modifiers: slot (0 Orbit, 1 Discharge, 2 Ending) x 3 + choice.
    enum HorizonModifier : uint8
    {
        MOD_WANDERING_STAR, MOD_EVENT_HORIZON, MOD_SATELLITE,
        MOD_ASTRAL_FOCUS, MOD_CONSTELLATION, MOD_PRISMATIC_LANCE,
        MOD_SUPERNOVA, MOD_DYING_LIGHT, MOD_ASTRAL_REMNANT,
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

    // Guiding Light's modifiers: slot (0 Radiance, 1 Guidance, 2 Farewell) x 3 + choice.
    enum GuidingModifier : uint8
    {
        MOD_STEADY_LIGHT, MOD_SHARED_LIGHT, MOD_SHELTERING_LIGHT,
        MOD_GENTLE_PASSAGE, MOD_GUARDIAN_STAR, MOD_SEEKING_LIGHT,
        MOD_FINAL_BENEDICTION, MOD_AFTERGLOW, MOD_PARTING_GRACE,
    };

    // Guiding Light's tuning: the star lasts 12 sec and heals its bearer every 2 sec for half a Prayer of Mending;
    // it arrives with 75% of one (moved at most once every 3 sec, 0.5 sec on the way). Its options keep the
    // dossier's ratios to a pulse.
    constexpr uint32 STAR_MS = 12000, STAR_PULSE_MS = 2000, STAR_MOVE_MS = 3000, STAR_TRAVEL_MS = 500;
    constexpr int32 AFTERGLOW_MS = 8000;
    constexpr float STAR_PULSE_SHARE = 0.5f, ARRIVAL_SHARE = 1.5f, STEADY_LIGHT = 1.5f, SHARED_SHARE = 0.75f;
    constexpr float SHELTER_SHARE = 2.0f, PASSAGE_SHARE = 0.5f, GUARDIAN_STEP = 0.1f, BENEDICTION_SHARE = 2.5f;
    constexpr float AFTERGLOW_SHARE = 3.25f, GRACE_SHARE = 2.0f;
    constexpr float STAR_RANGE = 40.0f, SHARED_RANGE = 10.0f, SEEKING_STAR_RANGE = 15.0f, PASSAGE_HALF_WIDTH = 3.0f;
    constexpr uint8 GUARDIAN_STEPS = 3;
    constexpr uint32 SEEKING_EVERY = 3;
    constexpr std::size_t PASSAGE_ALLIES = 3;

    // Elune's Guardian's modifiers: slot (0 Guardian, 1 Descent, 2 Blessing) x 3 + choice.
    enum OwlModifier : uint8
    {
        MOD_MOONPIERCER, MOD_SILVER_WINGS, MOD_WATCHFUL_FLIGHT,
        MOD_SWIFT_JUDGMENT, MOD_PATIENT_VIGIL, MOD_MOONLIT_PURSUIT,
        MOD_LUNAR_WOUND, MOD_GUIDING_WINGS, MOD_TWILIGHT_HUNT,
    };

    // Elune's Guardian's tuning: a direct Starfire hit calls a lunar owl (3 waiting at most) that dives 1 sec later
    // for 30% of that Starfire's damage (the dossier's 30% spell power: Starfire's own share of it).
    constexpr uint8 MAX_OWLS = 3, GUIDING_EVERY = 3;
    constexpr uint32 OWL_DELAY_MS = 1000, SWIFT_DELAY_MS = 100, VIGIL_DELAY_MS = 3000, WATCHFUL_SPACING_MS = 1000;
    constexpr uint32 WATCHFUL_ATTACKS = 3, PURSUIT_SECOND_MS = 1000;
    constexpr float OWL_SHARE = 0.3f, MOONPIERCER = 1.4f, SILVER_WINGS_SHARE = 0.5f, WATCHFUL_SHARE = 0.4f;
    constexpr float PATIENT_VIGIL = 1.3f, PURSUIT_SHARE = 0.2f, LUNAR_WOUND_SHARE = 1.0f, TWILIGHT_HUNT = 1.25f;
    constexpr float SILVER_WINGS_RADIUS = 6.0f, PURSUIT_RANGE = 15.0f;
    constexpr int32 LUNAR_WOUND_MS = 6000, TWILIGHT_HUNT_MS = 5000;
    constexpr std::size_t SILVER_WINGS_TARGETS = 4;

    // Astral Convergence's modifiers: slot (0 Collision, 1 Alignment, 2 Flare) x 3 + choice.
    enum ConvergenceModifier : uint8
    {
        MOD_PERFECT_ALIGNMENT, MOD_CONVERGING_SKIES, MOD_ASTRAL_RUPTURE,
        MOD_LONG_TWILIGHT, MOD_CROSSED_HORIZONS, MOD_STEADY_ORBIT,
        MOD_SOLAR_RETURN, MOD_LUNAR_DOMINANCE, MOD_EVENTIDE,
    };

    // Astral Convergence's tuning: Wrath leaves Sunlight (8 sec); Starfire on it collides for 40% of the
    // Starfire's damage and leaves Moonlight (8 sec); Wrath on Moonlight flares for 80% of the Wrath's damage (Wrath
    // being the small spell), ending the sequence. 5 enemies marked at most.
    constexpr int32 MARK_MS = 8000, LONG_TWILIGHT_MS = 12000, RUPTURE_MS = 6000;
    constexpr uint32 EVENTIDE_MS = 4000, EVENTIDE_TICK_MS = 1000, TWILIGHT_STEP_MS = 2000;
    constexpr float COLLISION_SHARE = 0.4f, FLARE_SHARE = 0.8f, PERFECT_ALIGNMENT = 1.4f, CONVERGING_SHARE = 0.5f;
    constexpr float RUPTURE_SHARE = 1.5f, TWILIGHT_STEP = 0.05f, TWILIGHT_MOST = 0.2f, CROSSED_SHARE = 0.35f;
    constexpr float SOLAR_RETURN = 1.6f, EVENTIDE_SHARE = 0.25f;
    constexpr float CONVERGING_RADIUS = 6.0f, CROSSED_RANGE = 12.0f, EVENTIDE_RADIUS = 6.0f;
    constexpr std::size_t MAX_MARKED = 5, CONVERGING_TARGETS = 4, CROSSED_TARGETS = 3, EVENTIDE_TARGETS = 5;

    // Ursoc's Champion's modifiers: slot (0 Guard, 1 Answer, 2 Resolve) x 3 + choice.
    enum UrsocModifier : uint8
    {
        MOD_ANCIENT_HIDE, MOD_LASTING_STAND, MOD_MOVING_MOUNTAIN,
        MOD_URSOCS_BITE, MOD_MOUNTAINS_ROAR, MOD_EARTHBREAKER,
        MOD_ENDURING_CHAMPION, MOD_CHALLENGERS_ROAR, MOD_UNBROKEN,
    };

    // Ursoc's Champion's tuning: Barkskin in Bear Form adds Ursoc's Guard (6 sec, an absorb of 30% of maximum
    // health) and comes back 30 sec sooner; each Maul in Bear Form takes 3 sec more off it. Maul in the guard
    // releases a blow of 75% of attack power plus all the guard absorbed (that part at most 250% of attack power)
    // in a 10-yard cone in front (up to 5).
    constexpr int32 GUARD_MS = 6000, LASTING_STAND_MS = 10000, ENDURING_MS = 8000, CHALLENGERS_MS = 6000;
    constexpr int32 BARKSKIN_SOONER_MS = 30000, MAUL_SOONER_MS = 3000;
    constexpr float GUARD_PCT = 30.0f, ANCIENT_HIDE_PCT = 45.0f, MOUNTAIN_PCT = 10.0f, MOUNTAIN_TRAVEL = 5.0f;
    constexpr float URSOC_AP = 0.75f, ABSORBED_SHARE = 1.0f, ABSORBED_MOST_AP = 2.5f, URSOCS_BITE = 2.0f;
    constexpr float URSOC_REACH = 10.0f, URSOC_ARC = float(M_PI) / 3.0f, ROAR_ARC = float(M_PI) / 2.0f;
    constexpr float EARTHBREAKER_REACH = 25.0f, EARTHBREAKER_HALF_WIDTH = 2.5f, EARTHBREAKER_FAR = 1.5f;
    constexpr float ENDURING_SHARE = 1.0f, CHALLENGERS_THREAT = 1.0f;
    constexpr uint8 UNBROKEN_BLOWS = 3;
    constexpr std::size_t URSOC_TARGETS = 5, ROAR_TARGETS = 10;

    // The Duelist's modifiers: slot (0 Sabre, 1 Pistol, 2 Footwork) x 3 + choice.
    enum DuelistModifier : uint8
    {
        MOD_DEEP_CUT, MOD_SWEEPING_STEEL, MOD_SERRATED_EDGE,
        MOD_HEAVY_BALL, MOD_SCATTERSHOT, MOD_PIERCING_ROUND,
        MOD_ADVANCE, MOD_DISENGAGING_SHOT, MOD_PRESS_THE_ADVANTAGE,
    };

    // The Duelist's tuning: every second direct Sabre Slash hit readies Opening Shot (8 sec, one held); Pistol Shot
    // spends it (20 Energy, 6 sec cooldown, 20 yards): 60% of attack power and a combo point (the dossier's every
    // third and 35%, made worth holding a button for).
    constexpr uint8 OPENING_EVERY = 2;
    constexpr int32 OPENING_MS = 8000, SERRATED_MS = 6000, FOOTWORK_MS = 5000, DISCOUNT_MS = 8000;
    constexpr uint32 PRESS_DELAY_MS = 300;
    constexpr int32 DISENGAGING_REFUND = 10;
    constexpr float PISTOL_AP = 0.6f, HEAVY_BALL = 1.6f, SCATTER_SHARE = 0.75f, PIERCING_SHARE = 0.8f;
    constexpr float DEEP_CUT = 1.25f, SWEEPING_SHARE = 0.5f, SERRATED_SHARE = 0.4f, ADVANCE_BONUS = 1.3f;
    constexpr float PRESS_SHARE = 0.5f;
    constexpr float SWEEPING_RADIUS = 5.0f, SCATTER_REACH = 10.0f, SCATTER_ARC = float(M_PI) / 4.0f;
    constexpr float PIERCING_REACH = 15.0f, PIERCING_HALF_WIDTH = 2.0f;
    constexpr std::size_t SWEEPING_TARGETS = 3, SCATTER_TARGETS = 4, PIERCING_TARGETS = 3;
    // Broadside (35 Energy, 20 sec, a finishing move): 3 pistol blasts 0.25 sec apart, each 30% of attack power
    // per combo point to its target and up to 7 more within 8 yards.
    constexpr uint32 BROADSIDE_BLASTS = 3, BROADSIDE_SPACING_MS = 250;
    constexpr float BROADSIDE_AP = 0.3f, BROADSIDE_RADIUS = 8.0f;
    constexpr std::size_t BROADSIDE_TARGETS = 8;

    // The Stormreaver's modifiers: slot (0 Storm, 1 Thunderlash, 2 Lightning Shield) x 3 + choice.
    enum StormreaverModifier : uint8
    {
        MOD_EYE_OF_THE_TEMPEST, MOD_FORKED_CATACLYSM, MOD_STORM_ROLLING,
        MOD_THUNDERHEAD, MOD_CHAINBREAKER, MOD_WHITE_HOT_CONDUCTOR,
        MOD_OVERCHARGED_CORE, MOD_RIDE_THE_LIGHTNING, MOD_VIOLENT_GROUNDING,
    };

    // The Stormreaver's tuning: Stormstrike raises the Stormfront (6 sec, refreshed): a pulse every 2 sec, 25% of
    // attack power as Nature to up to 5 enemies within 8 yards. Lava Lash in it is Thunderlash (60% to its target,
    // arcs of 30% to 2 more within 8); melee abilities discharge Lightning Shield at their target once a second (the
    // shield keeps its charges meanwhile): its rank's damage, a third of spell power and 20% of attack power. The
    // dossier's 15%, 40% and 20%, raised.
    constexpr int32 STORMFRONT_MS = 6000, WHITE_HOT_MS = 6000, RIDE_MS = 3000;
    constexpr uint32 STORM_PULSE_MS = 2000, ROLLING_PULSE_MS = 1000, DISCHARGE_EVERY_MS = 1000;
    constexpr int32 RIDE_COOLDOWN_MS = 1000;
    constexpr float STORM_PULSE_AP = 0.25f, EYE_OF_THE_TEMPEST = 2.2f, ROLLING_SHARE = 0.7f;
    constexpr float THUNDERLASH_AP = 0.6f, THUNDERHEAD_AP = 1.2f, ARC_AP = 0.3f, WHITE_HOT_AP = 1.0f;
    constexpr float DISCHARGE_SP = 0.33f, DISCHARGE_AP = 0.2f, OVERCHARGED = 1.5f, GROUNDING_SHARE = 0.5f;
    constexpr float STORM_RADIUS = 8.0f, FORKED_RADIUS = 12.0f, THUNDERLASH_RANGE = 8.0f, GROUNDING_RADIUS = 8.0f;
    constexpr std::size_t STORM_TARGETS = 5, FORKED_TARGETS = 8, THUNDERLASH_ARCS = 2, CHAINBREAKER_ARCS = 4;
    constexpr std::size_t GROUNDING_TARGETS = 3;

    // Ember's Wrath's modifiers: slot (0 Ignition, 1 Claws, 2 Instinct) x 3 + choice.
    enum EmbersModifier : uint8
    {
        MOD_SEARING_WOUND, MOD_WILD_EMBERS, MOD_SMOLDERING_FLESH,
        MOD_BLAZING_RAKE, MOD_MOLTEN_BITE, MOD_CINDER_SWIPE,
        MOD_FLAMEBOUND, MOD_PATIENT_PREDATOR, MOD_ASHEN_PURSUIT,
    };

    // Ember's Wrath's tuning: the Hero's own Fire Blast (its mana and cooldown), usable in Cat Form, ignites their
    // Rake or Rip victim: Cinder Wound, 150% of attack power plus 100% of spell power over 6 sec, a tick every 2 sec
    // (a new one keeps what is left of the last, up to twice a fresh one). The dossier's 30% and 30% (and its other
    // shares, five times or so over) made the rune 2-5% of a feral's damage.
    constexpr int32 CINDER_MS = 6000, SMOLDERING_MS = 10000, FLAMEBOUND_MS = 3000, FLAMEBOUND_DISCOUNT_MS = 5000;
    constexpr uint32 ASHEN_MS = 6000, BLAZING_RAKE_MS = 3000;
    constexpr int32 FLAMEBOUND_REFUND = 10;
    constexpr float CINDER_AP = 1.5f, CINDER_SP = 1.0f, SEARING_WOUND = 1.35f, SMOLDERING_FLESH = 1.3f;
    constexpr float WILD_EMBERS_AP = 0.4f, WILD_EMBERS_SP = 0.3f, BLAZING_RAKE_AP = 0.6f, MOLTEN_BITE_AP = 0.25f;
    constexpr float CINDER_SWIPE_SHARE = 0.5f, ASHEN_PURSUIT = 1.2f, PATIENT_PREDATOR = 2.0f;
    constexpr float WILD_EMBERS_RADIUS = 6.0f, CINDER_SWIPE_RADIUS = 8.0f;
    constexpr std::size_t WILD_EMBERS_TARGETS = 4, CINDER_SWIPE_TARGETS = 3;

    // Whitefang's modifiers: slot (0 Shatter, 1 Hunt, 2 Instinct) x 3 + choice.
    enum WhitefangModifier : uint8
    {
        MOD_WHITEFANG, MOD_ICEFANG_SPRAY, MOD_WINTER_WOUND,
        MOD_LONG_WINTER, MOD_PURSUING_CHILL, MOD_PACK_FROST,
        MOD_FANG_COLD_BLOOD, MOD_SNOWBLIND, MOD_FROZEN_TRAIL,
    };

    // Whitefang's tuning: the Hero's own Icy Touch (its Frost rune), in Cat Form or not, lays Frozen Prey for 6 sec;
    // the next Shred or Mangle on the victim shatters it for 150% of attack power as Frost damage (the dossier's
    // 30%, and Frozen Trail's 5% a second, made the rune 2-5% of a feral's damage).
    constexpr int32 PREY_MS = 6000, LONG_WINTER_MS = 10000, WINTER_WOUND_MS = 6000, PURSUING_MS = 3000;
    constexpr int32 PACK_FROST_MS = 6000, FANG_COLD_BLOOD_MS = 5000, SNOWBLIND_MS = 3000, TRAIL_SLOW_MS = 1500;
    constexpr uint32 LONG_WINTER_AFTER_MS = 3000, FROZEN_TRAIL_TICKS = 4, FROZEN_TRAIL_TICK_MS = 1000;
    constexpr int32 PURSUING_REFUND = 10;
    constexpr float SHATTER_AP = 1.5f, WHITEFANG_BONUS = 1.4f, ICEFANG_SHARE = 0.5f, WINTER_WOUND_SHARE = 1.5f;
    constexpr float LONG_WINTER_BONUS = 1.2f, PACK_FROST_SHARE = 0.6f, FANG_COLD_BLOOD = 1.2f, FROZEN_TRAIL_AP = 0.2f;
    constexpr float ICEFANG_REACH = 8.0f, ICEFANG_ARC = float(M_PI) / 4.0f, PACK_FROST_RADIUS = 8.0f;
    constexpr float SNOWBLIND_RADIUS = 6.0f, FROZEN_TRAIL_RADIUS = 6.0f;
    constexpr std::size_t ICEFANG_TARGETS = 4, PACK_FROST_TARGETS = 2, FROZEN_TRAIL_TARGETS = 5;

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
            // Melee DPS: Eviscerate's phantom.
            { RUNE_SPECTRAL_BLADE, uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE)),
              { { SPELL_EVISCERATE } }, SPELL_RUNE_SPECTRAL_BLADE, SPELL_BLADE_MODIFIER_FIRST },
            // DPS: Kill Command or Bestial Wrath (a pet's) unleashes the beasts.
            { RUNE_WILD_HUNT,
              uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE) | Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_KILL_COMMAND, SPELL_BESTIAL_WRATH } }, SPELL_RUNE_WILD_HUNT, SPELL_HUNT_MODIFIER_FIRST },
            // Ranged DPS: Arcane Blast builds the Insight, Arcane Missiles spends it.
            { RUNE_ARCANE_MIND, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_ARCANE_BLAST }, { SPELL_ARCANE_MISSILES } }, SPELL_RUNE_ARCANE_MIND,
              SPELL_MIND_MODIFIER_FIRST },
            // Ranged DPS: Frostbolt's Icicles.
            { RUNE_SPLINTERED_CROWN, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_FROSTBOLT } }, SPELL_RUNE_SPLINTERED_CROWN, SPELL_CROWN_MODIFIER_FIRST },
            // Ranged DPS: Pyroblast leaves the Ember, Fire Blast wakes the phoenix.
            { RUNE_PHOENIX_WAKE, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_PYROBLAST }, { SPELL_FIRE_BLAST } }, SPELL_RUNE_PHOENIX_WAKE, SPELL_PHOENIX_MODIFIER_FIRST },
            // Ranged DPS: Arcane Barrage sets the Orb loose.
            { RUNE_ASTRAL_HORIZON, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_ARCANE_BARRAGE } }, SPELL_RUNE_ASTRAL_HORIZON, SPELL_HORIZON_MODIFIER_FIRST },
            // Healers: Prayer of Mending places the star, Flash Heal or Greater Heal moves it.
            { RUNE_GUIDING_LIGHT, uint8(Wildcard::RoleMask(Wildcard::ROLE_HEALER)),
              { { SPELL_PRAYER_OF_MENDING }, { SPELL_FLASH_HEAL, SPELL_GREATER_HEAL } }, SPELL_RUNE_GUIDING_LIGHT,
              SPELL_GUIDING_MODIFIER_FIRST },
            // Ranged DPS: Starfire calls the owls.
            { RUNE_ELUNES_GUARDIAN, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_STARFIRE } }, SPELL_RUNE_ELUNES_GUARDIAN, SPELL_OWL_MODIFIER_FIRST },
            // Ranged DPS: Wrath and Starfire in turn.
            { RUNE_ASTRAL_CONVERGENCE, uint8(Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_WRATH }, { SPELL_STARFIRE } }, SPELL_RUNE_ASTRAL_CONVERGENCE,
              SPELL_CONVERGENCE_MODIFIER_FIRST },
            // Tanks: Barkskin in Bear Form raises the guard, Maul releases it.
            { RUNE_URSOCS_CHAMPION, uint8(Wildcard::RoleMask(Wildcard::ROLE_TANK)),
              { { SPELL_BEAR_FORM, SPELL_DIRE_BEAR_FORM }, { SPELL_BARKSKIN }, { SPELL_MAUL } },
              SPELL_RUNE_URSOCS_CHAMPION, SPELL_URSOC_MODIFIER_FIRST },
            // Melee DPS: Sinister Strike readies the pistol (the rune teaches Pistol Shot).
            { RUNE_DUELIST, uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE)),
              { { SPELL_SINISTER_STRIKE } }, SPELL_RUNE_DUELIST, SPELL_DUELIST_MODIFIER_FIRST },
            // Melee DPS: Stormstrike raises the storm, Lava Lash and Lightning Shield strike through it.
            { RUNE_STORMREAVER, uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE)),
              { { SPELL_STORMSTRIKE }, { SPELL_LAVA_LASH }, { SPELL_LIGHTNING_SHIELD } }, SPELL_RUNE_STORMREAVER,
              SPELL_STORMREAVER_MODIFIER_FIRST },
            // Melee DPS: Fire Blast in Cat Form ignites the Hero's Rake or Rip (the rune teaches it).
            { RUNE_EMBERS_WRATH, uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE)),
              { { SPELL_CAT_FORM }, { SPELL_FIRE_BLAST }, { SPELL_RAKE, SPELL_RIP } }, SPELL_RUNE_EMBERS_WRATH,
              SPELL_EMBERS_MODIFIER_FIRST },
            // Melee DPS: Icy Touch in Cat Form marks the prey, Shred or Mangle shatters it (the rune teaches it).
            { RUNE_WHITEFANG, uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE)),
              { { SPELL_CAT_FORM }, { SPELL_ICY_TOUCH }, { SPELL_SHRED, SPELL_MANGLE_CAT } }, SPELL_RUNE_WHITEFANG,
              SPELL_WHITEFANG_MODIFIER_FIRST },
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
        if (rune == RUNE_WILD_HUNT && modifier == MOD_UNENDING_WRATH)
            return { { SPELL_KILL_COMMAND }, { SPELL_BESTIAL_WRATH } };
        if (rune == RUNE_SPECTRAL_BLADE && modifier == MOD_MARKED_FOR_MURDER)
            return { { SPELL_SINISTER_STRIKE, SPELL_BACKSTAB, SPELL_HEMORRHAGE } };
        if (rune == RUNE_ARCANE_MIND && modifier == MOD_EXPANDING_INTELLECT)
            return { { SPELL_ARCANE_EXPLOSION } };
        if (rune == RUNE_GUIDING_LIGHT && modifier == MOD_GUARDIAN_STAR)
            return { { SPELL_GREATER_HEAL } };
        if (rune == RUNE_ELUNES_GUARDIAN && modifier == MOD_TWILIGHT_HUNT)
            return { { SPELL_WRATH } };
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
        uint8 WakeRips = 0;                 // rips the wake has left (2 with Overpowering Echo)
        uint32 WakeRippedAt = 0;            // the last rip: one ability hitting several enemies rips once
        ObjectGuid WakePrimary;
        Position WakeOffset;                        // Relentless: from its target, as cut

        // The Dawnbound Crusader.
        Position SacredAt;                          // Sacred Ground's patch (SacredUntil 0: none)
        uint32 SacredMap = 0, SacredUntil = 0, SacredTimer = 0;
        float SacredTick = 0.0f;
        bool SacredHeal = false;

        // The Hollow Congregation.
        std::vector<ObjectGuid> HollowEchoes;       // oldest first

        std::unordered_map<uint32, int32> Reported;  // the rune auras' amounts last told to the addon (Report)
        uint32 WatchTimer = 0;                      // the bound rune's aura checked (OnPlayerUpdate)

        // The Shattered Sun: the enemies holding shards.
        std::vector<ObjectGuid> Shards;
        Position ScaldAt;                           // Scalding Ground (ScaldUntil 0: none)
        uint32 ScaldMap = 0, ScaldUntil = 0, ScaldTimer = 0;
        float ScaldTick = 0.0f;

        // The Wild Hunt: its Spectral Beasts (oldest first), and Bestial Wrath's calling.
        std::vector<ObjectGuid> SpectralBeasts;
        uint32 WrathTimer = 0;
        int32 WrathAdded = 0;                       // Unending Wrath's, this Bestial Wrath

        bool Debug = false;                         // .runedebug: the newest runes' steps told in chat

        // Arcane Mind.
        uint8 BlastCount = 0;                       // Accelerated Thought: Arcane Blasts since the last extra
        bool MindChannel = false;                   // an empowered Arcane Missiles channelling (Mental Reserves)

        // The Splintered Crown.
        uint8 Icicles = 0, FrostboltHits = 0, IcicleAttacks = 0;

        // Phoenix Wake.
        ObjectGuid EmberHost;
        float EmberPyro = 0.0f;
        uint32 Releases = 0, ArrivedRelease = 0;
        std::vector<Flight> Flights;
        std::vector<std::pair<Position, uint32>> Scorch;    // Scorched Earth's patches: where, when
        uint32 ScorchMap = 0, ScorchTimer = 0;
        float ScorchTick = 0.0f;

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

        // Guiding Light.
        ObjectGuid StarBearer;
        uint32 StarUntil = 0, StarPulseTimer = 0, StarMovedAt = 0, StarArriveAt = 0, StarPulses = 0;
        uint8 StarFocus = 0;                        // Guardian Star: pulses in a row on its bearer
        bool StarTravelling = false;

        // Elune's Guardian.
        uint8 Owls = 0, OwlCount = 0;               // owls waiting to dive; owls since Guiding Wings' last

        // Astral Convergence.
        std::vector<ObjectGuid> Marked;             // enemies in the Hero's Sunlight or Moonlight, oldest first
        Position EventideAt;                        // Eventide's field (EventideUntil 0: none)
        uint32 EventideMap = 0, EventideUntil = 0, EventideTimer = 0;
        float EventideTick = 0.0f;

        // Ursoc's Champion.
        int32 GuardShield = 0;                      // the guard's absorb as raised (0: none up)
        uint8 GuardBlows = 0;                       // Unbroken's blows from this guard
        bool GuardTravelled = false;
        Position GuardFrom;

        // The Duelist: direct Sinister Strike hits toward Opening Shot; Advance's enemy and how long it holds.
        uint8 SabreHits = 0;
        ObjectGuid AdvanceOn;
        uint32 AdvanceUntil = 0;

        // The Stormreaver: the Stormfront's pulse timer; the last discharge.
        uint32 StormfrontTimer = 0, LastDischarge = 0;

        // Ember's Wrath: Blazing Rake's next extra hit; Ashen Pursuit's next bonus, its last victim and every one.
        uint32 BlazingRakeAt = 0, AshenReadyAt = 0;
        ObjectGuid AshenLast;
        std::vector<ObjectGuid> AshenSeen;

        // Whitefang: the primary mark (on whom, since when); Pursuing Chill's discount waiting; Cold Blood's victim.
        ObjectGuid PreyOn, ColdBloodOn;
        uint32 PreyAt = 0;
        bool PursuitDiscount = false;

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
            Report(player, SPELL_STATIC_WARD, 'A', ward->GetAmount());
            return;
        }
        player->CastCustomSpell(player, SPELL_STATIC_WARD, &step, nullptr, nullptr, true);
        Report(player, SPELL_STATIC_WARD, 'A', step);
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

    // Shield Slam while charged: the echo bursts about its target, 42% of the slam's damage before mitigation to
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
            Report(player, SPELL_SHELTERING_REVERBERATION, 'A', absorb);
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
        bool shield = spellId == SPELL_TIDAL_SHELTER || spellId == SPELL_JADE_SHELTER;
        Report(player, spellId, shield ? 'A' : 'H', amount);
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
            case SPELL_OPEN_WOUND: return SPELL_OPEN_WOUND_TICK;
            case SPELL_GHOSTFANG: return SPELL_GHOSTFANG_TICK;
            case SPELL_ARCANE_ECHO: return SPELL_ARCANE_ECHO_TICK;
            case SPELL_FROST_WOUND: return SPELL_FROST_WOUND_TICK;
            case SPELL_CLINGING_EMBERS: return SPELL_CLINGING_EMBERS_TICK;
            case SPELL_LUNAR_WOUND: return SPELL_LUNAR_WOUND_TICK;
            case SPELL_ASTRAL_RUPTURE: return SPELL_ASTRAL_RUPTURE_TICK;
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
            Report(player, SPELL_RADIANT_WARD, 'A', ward);
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
        // The Dawnbound Crusader's first draft taught Dawn Vow and vowed the Hero: gone with it.
        unlearn(SPELL_DAWN_VOW);
        player->RemoveAurasDueToSpell(SPELL_DAWN_VOWED);
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
        if (HasBladeMarker(player, MOD_MARKED_FOR_MURDER))
            Lasting(player, victim, SPELL_MARKED_FOR_MURDER, MARKED_MS);
    }

    // Eviscerate's hit: the phantom, twice it, 0.6 sec later (0.1 with Quick Death; 2 sec and 40% more with
    // Patient Killer; 20% more as Open Wound's bleed).
    void QueuePhantom(Player* player, Unit* victim, float damage)
    {
        if (damage <= 0.0f)
            return;
        float amount = damage * PHANTOM_SHARE;
        uint32 delay = PHANTOM_DELAY_MS;
        if (HasBladeMarker(player, MOD_QUICK_DEATH))
            delay = QUICK_DEATH_MS;
        else if (HasBladeMarker(player, MOD_PATIENT_KILLER))
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

    // Kill Command: a beast for 10 sec. In Bestial Wrath it comes back in 5 sec; Unending Wrath adds 2 sec to it.
    void KillCommand(Player* player)
    {
        Unleash(player, BEAST_MS);
        if (!InWrath(player))
            return;
        if (player->GetSpellCooldownDelay(SPELL_KILL_COMMAND) > WRATH_KILL_COMMAND_MS)
            player->ModifySpellCooldown(SPELL_KILL_COMMAND,
                int32(WRATH_KILL_COMMAND_MS) - int32(player->GetSpellCooldownDelay(SPELL_KILL_COMMAND)));
        State& state = StateOf(player);
        if (HasHuntMarker(player, MOD_UNENDING_WRATH) && state.WrathAdded < UNENDING_MOST_MS)
            if (Aura* wrath = player->GetPet()->GetAura(SPELL_BESTIAL_WRATH, player->GetGUID()))
            {
                state.WrathAdded += UNENDING_MS;
                wrath->SetMaxDuration(wrath->GetMaxDuration() + UNENDING_MS);
                wrath->SetDuration(wrath->GetDuration() + UNENDING_MS);
            }
    }

    // Bestial Wrath: Kill Command at most 5 sec away; then a beast a second while it lasts (WrathTick).
    void BestialWrath(Player* player)
    {
        State& state = StateOf(player);
        state.WrathAdded = 0;
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
        if (HasMindMarker(player, MOD_MENTAL_RESERVES))
            player->m_Events.AddEventAtOffset([player]()
            {
                if (player->IsInWorld() && player->IsAlive())
                    MindReserves(player);
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

    // One Icicle attack made (a fragmented volley counts once): every third, Frost Wound's wound on its victim (80%
    // of an Icicle over 6 sec) or Cold Front's instant Frostbolt.
    void IcicleAttacked(Player* player, Unit* victim, float icicle)
    {
        State& state = StateOf(player);
        if (++state.IcicleAttacks < AFTERMATH_EVERY)
            return;
        state.IcicleAttacks = 0;
        if (HasCrownMarker(player, MOD_FROST_WOUND) && Valid(player, victim))
            AddDot(player, victim, SPELL_FROST_WOUND, icicle * FROST_WOUND_SHARE, FROST_WOUND_MS);
        if (HasCrownMarker(player, MOD_COLD_FRONT))
        {
            player->RemoveAurasDueToSpell(SPELL_COLD_FRONT);
            player->AddAura(SPELL_COLD_FRONT, player);
        }
    }

    // An Icicle's (or a fragment's) hit: Winter's Grasp 15% more on an enemy it slowed already, and slows; Impaling
    // Ice pierces up to 3 enemies within 15 yards behind the victim (40%); Seeking Splinters sends a shard on as it
    // lands (30% to another enemy within 10 yards, else 15% to the victim).
    void IcicleHit(Player* player, Unit* victim, float amount, uint32 spellId)
    {
        bool grasp = HasCrownMarker(player, MOD_WINTERS_GRASP);
        auto graspOf = [player, grasp](Unit* enemy)
        {
            return grasp && enemy->HasAura(SPELL_WINTERS_GRASP, player->GetGUID()) ? WINTERS_GRASP : 1.0f;
        };
        Strike(player, victim, spellId, amount * graspOf(victim));
        std::vector<Unit*> struck = { victim };
        if (HasCrownMarker(player, MOD_IMPALING_ICE))
        {
            Shape line;
            line.From = victim->GetPosition();
            line.To = Toward(line.From, player->GetAngle(victim), IMPALE_REACH);
            line.HalfWidth = IMPALE_HALF_WIDTH;
            std::size_t count = IMPALE_TARGETS;
            for (Unit* enemy : EnemiesIn(player, line, IMPALE_TARGETS + 1))
                if (enemy != victim && count)
                {
                    Strike(player, enemy, SPELL_ICE_SPLINTER, amount * IMPALE_SHARE * graspOf(enemy));
                    struck.push_back(enemy);
                    --count;
                }
        }
        if (grasp)
            for (Unit* enemy : struck)
                if (enemy->IsAlive())
                    Lasting(player, enemy, SPELL_WINTERS_GRASP, WINTERS_GRASP_MS);
        if (!HasCrownMarker(player, MOD_SEEKING_SPLINTERS))
            return;
        uint32 flight = uint32(player->GetExactDist(victim) / ICICLE_SPEED * 1000.0f);
        ObjectGuid guid = victim->GetGUID();
        player->m_Events.AddEventAtOffset([player, guid, amount]()
        {
            Unit* from = ObjectAccessor::GetUnit(*player, guid);
            if (!player->IsInWorld() || !player->IsAlive() || !from || !from->IsInMap(player))
                return;
            for (Unit* enemy : EnemiesAround(player, from, SEEKING_RANGE))
                if (enemy != from)
                {
                    Strike(player, enemy, SPELL_ICE_SPLINTER, amount * SEEKING_SHARE);
                    return;
                }
            if (Valid(player, from))
                Strike(player, from, SPELL_ICE_SPLINTER, amount * SEEKING_SELF_SHARE);
        }, Milliseconds(flight));
    }

    // An Icicle loosed at its victim (gone or out of reach: it melts). Splinter Volley: 3 fragments 0.2 sec apart,
    // 40% of an Icicle each, the last making the next Frostbolt 10% faster. Shatterglass: half of it to up to 4
    // others within 6 yards.
    void LoosenIcicle(Player* player, ObjectGuid victimGuid, float icicle)
    {
        State& state = StateOf(player);
        if (state.Icicles)
            --state.Icicles;
        ShowCrown(player);
        Unit* victim = ObjectAccessor::GetUnit(*player, victimGuid);
        if (!player->IsAlive() || !Valid(player, victim) || !player->IsWithinDistInMap(victim, ICICLE_RANGE))
        {
            Trace(player, "Icicle melted: its target is gone or out of reach");
            return;
        }
        Trace(player, Acore::StringFormat("Icicle loosed at {} for {}", victim->GetName(), icicle));
        if (HasCrownMarker(player, MOD_SPLINTER_VOLLEY))
        {
            for (uint32 i = 0; i < SPLINTERS; ++i)
                player->m_Events.AddEventAtOffset([player, victimGuid, icicle, i]()
                {
                    Unit* target = ObjectAccessor::GetUnit(*player, victimGuid);
                    if (!player->IsInWorld() || !player->IsAlive() || !Valid(player, target))
                        return;
                    IcicleHit(player, target, icicle * SPLINTER_SHARE, SPELL_ICE_SPLINTER);
                    if (i + 1 < SPLINTERS)
                        return;
                    player->RemoveAurasDueToSpell(SPELL_SPLINTER_QUICKENING);
                    player->AddAura(SPELL_SPLINTER_QUICKENING, player);
                }, Milliseconds(i * SPLINTER_SPACING_MS));
            IcicleAttacked(player, victim, icicle);
            return;
        }
        IcicleHit(player, victim, icicle, SPELL_ICICLE);
        if (HasCrownMarker(player, MOD_SHATTERGLASS))
        {
            std::size_t count = SHATTERGLASS_TARGETS;
            for (Unit* enemy : EnemiesAround(player, victim, SHATTERGLASS_RADIUS))
                if (enemy != victim && count)
                {
                    Strike(player, enemy, SPELL_ICE_SPLINTER, icicle * SHATTERGLASS_SHARE);
                    --count;
                }
        }
        IcicleAttacked(player, victim, icicle);
    }

    // A direct Frostbolt hit forms an Icicle (5 waiting at most): 30% of the Frostbolt's damage (Glacial Spear 40%
    // more; Frozen Focus: every third Frostbolt's, 60% more), loosed 1 sec later at the same victim.
    void FormIcicle(Player* player, Unit* victim, float frostbolt)
    {
        State& state = StateOf(player);
        bool focused = false;
        if (HasCrownMarker(player, MOD_FROZEN_FOCUS) && ++state.FrostboltHits >= FROZEN_FOCUS_EVERY)
        {
            state.FrostboltHits = 0;
            focused = true;
        }
        if (state.Icicles >= MAX_ICICLES || frostbolt <= 0.0f)
        {
            Trace(player, Acore::StringFormat("Frostbolt hit for {}: no Icicle ({} waiting)", frostbolt,
                state.Icicles));
            return;
        }
        ++state.Icicles;
        Trace(player, Acore::StringFormat("Frostbolt hit for {}: Icicle formed ({} waiting)", frostbolt,
            state.Icicles));
        ShowCrown(player);
        float icicle = frostbolt * ICICLE_SHARE * (HasCrownMarker(player, MOD_GLACIAL_SPEAR) ? GLACIAL_SPEAR : 1.0f)
            * (focused ? FROZEN_FOCUS : 1.0f);
        ObjectGuid guid = victim->GetGUID();
        player->m_Events.AddEventAtOffset([player, guid, icicle]()
        {
            if (player->IsInWorld())
                LoosenIcicle(player, guid, icicle);
        }, Milliseconds(ICICLE_DELAY_MS));
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
        Lasting(player, target, SPELL_PHOENIX_EMBER, EMBER_MS);
        Trace(player, Acore::StringFormat("Pyroblast hit {} for {}: Ember placed (on it: {})", target->GetName(),
            pyroblast, target->HasAura(SPELL_PHOENIX_EMBER, player->GetGUID()) ? "yes" : "NO"));
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
        uint32 birds = flock ? FLOCK_BIRDS : 1;
        float facing = player->GetAngle(host);
        uint32 now = Now();
        for (uint32 i = 0; i < birds; ++i)
        {
            float side = birds > 1 ? (float(i) - 1.0f) * FLOCK_SPREAD : 0.0f;
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
            if (flock)
                bird->SetObjectScale(bird->GetObjectScale() * FLOCK_SCALE);
            Flight flight;
            flight.Bird = bird->GetGUID();
            flight.Host = host->GetGUID();
            flight.Release = release;
            flight.Share = 1.0f / float(birds);
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
                    ++flight.Crossed;
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
                    FlightHit(player, host, flight.Pyro * PHOENIX_HOST * flight.Share
                        * (HasPhoenixMarker(player, MOD_SUN_TALON) ? SUN_TALON : 1.0f));
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
                        FlightHit(player, next, flight.Cross * flight.Share);
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
        int32 points = std::max<int32>(1, int32(amount));
        orb->CastCustomSpell(enemy, spellId, &points, nullptr, nullptr, true, nullptr, nullptr, player->GetGUID());
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
        if (HasHorizonMarker(player, MOD_SATELLITE))
        {
            state.OrbSatellite = true;
            state.OrbUntil = now + SATELLITE_MS;
            if (Aura* aura = player->AddAura(SPELL_ORB_SATELLITE, player))
            {
                aura->SetMaxDuration(int32(SATELLITE_MS));
                aura->SetDuration(int32(SATELLITE_MS));
            }
            return;
        }
        float angle = player->GetAngle(victim);
        Position at = Toward(player->GetPosition(), angle, ORB_START);
        at.m_positionZ = player->GetPositionZ() + ORB_HEIGHT;
        at.SetOrientation(angle);
        TempSummon* orb = SummonFlier(player, NPC_RUNE_ARCANE_ORB, at, ORB_MS + ORB_LINGER_MS, SPELL_ORB_FORM);
        if (!orb)
            return;
        state.Orb = orb->GetGUID();
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
        if (HasHorizonMarker(player, MOD_PRISMATIC_LANCE))
        {
            OrbStrike(player, orb, target, 1.0f, SPELL_PRISMATIC_LANCE);
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
            OrbStrike(player, orb, target, 1.0f, SPELL_ORB_BOLT);
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

    // A fresh buff on the Hero (an earlier one replaced).
    void Renew(Player* player, uint32 spellId)
    {
        player->RemoveAurasDueToSpell(spellId);
        player->AddAura(spellId, player);
    }

    // One owl strike from the sky on its victim (Silver Wings: half of it to up to 4 others within 6 yards; Lunar
    // Wound: as much again over 6 sec; Twilight Hunt: the victim marked for the next Wrath).
    void OwlStrike(Player* player, Unit* victim, float amount, bool first)
    {
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
        if (HasOwlMarker(player, MOD_LUNAR_WOUND))
            AddDot(player, victim, SPELL_LUNAR_WOUND, amount * LUNAR_WOUND_SHARE, LUNAR_WOUND_MS);
        if (HasOwlMarker(player, MOD_TWILIGHT_HUNT))
            Lasting(player, victim, SPELL_TWILIGHT_HUNT, TWILIGHT_HUNT_MS);
    }

    // An owl's dive (its victim gone: Moonlit Pursuit takes the nearest enemy within 15 yards, else it fades).
    // Watchful Flight: 3 strikes 1 sec apart, 40% each, the last making the next Starfire 20% cheaper. Moonlit
    // Pursuit: a second strike 1 sec later for 20%. Swift Judgment: each dive, the next Starfire 10% faster. Guiding
    // Wings: every third owl, the next Starfire instant and 20% speed.
    void OwlDive(Player* player, ObjectGuid victimGuid, float owl)
    {
        State& state = StateOf(player);
        if (state.Owls)
            --state.Owls;
        Unit* victim = ObjectAccessor::GetUnit(*player, victimGuid);
        if (!player->IsAlive())
            return;
        if (!Valid(player, victim))
        {
            if (!HasOwlMarker(player, MOD_MOONLIT_PURSUIT) || !victim || !victim->IsInMap(player))
                return;
            std::vector<Unit*> near = EnemiesAround(player, victim, PURSUIT_RANGE);
            if (near.empty())
                return;
            victim = near.front();
            victimGuid = victim->GetGUID();
        }
        else if (HasOwlMarker(player, MOD_MOONPIERCER))
            owl *= MOONPIERCER;
        if (HasOwlMarker(player, MOD_WATCHFUL_FLIGHT))
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
        if (HasOwlMarker(player, MOD_MOONLIT_PURSUIT))
            player->m_Events.AddEventAtOffset([player, victimGuid, owl]()
            {
                Unit* target = ObjectAccessor::GetUnit(*player, victimGuid);
                if (player->IsInWorld() && player->IsAlive() && Valid(player, target))
                    Strike(player, target, SPELL_OWL_DIVE, owl * PURSUIT_SHARE);
            }, Milliseconds(PURSUIT_SECOND_MS));
        if (HasOwlMarker(player, MOD_SWIFT_JUDGMENT))
            Renew(player, SPELL_SWIFT_JUDGMENT);
        if (HasOwlMarker(player, MOD_GUIDING_WINGS) && ++state.OwlCount >= GUIDING_EVERY)
        {
            state.OwlCount = 0;
            Renew(player, SPELL_GUIDING_WINGS);
            Renew(player, SPELL_GUIDING_WINGS_SPEED);
        }
    }

    // A direct Starfire hit calls an owl (3 waiting at most): 30% of the Starfire's damage, diving 1 sec later
    // (Swift Judgment 0.1; Patient Vigil 3, 30% more).
    void CallOwl(Player* player, Unit* victim, float starfire)
    {
        State& state = StateOf(player);
        if (state.Owls >= MAX_OWLS || starfire <= 0.0f)
            return;
        ++state.Owls;
        float owl = starfire * OWL_SHARE * (HasOwlMarker(player, MOD_PATIENT_VIGIL) ? PATIENT_VIGIL : 1.0f);
        uint32 delay = HasOwlMarker(player, MOD_SWIFT_JUDGMENT) ? SWIFT_DELAY_MS
            : HasOwlMarker(player, MOD_PATIENT_VIGIL) ? VIGIL_DELAY_MS : OWL_DELAY_MS;
        ObjectGuid guid = victim->GetGUID();
        player->m_Events.AddEventAtOffset([player, guid, owl]()
        {
            if (player->IsInWorld())
                OwlDive(player, guid, owl);
        }, Milliseconds(delay));
    }

    // ---- Astral Convergence ------------------------------------------------------------------------------------

    bool HasConvergenceMarker(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_CONVERGENCE_MODIFIER_FIRST, modifier);
    }

    int32 MarkMs(Player* player)
    {
        return HasConvergenceMarker(player, MOD_LONG_TWILIGHT) ? LONG_TWILIGHT_MS : MARK_MS;
    }

    // A mark of the Hero's on an enemy (5 enemies marked at most: the longest marked loses theirs).
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
        Lasting(player, enemy, spellId, MarkMs(player));
    }

    // Wrath: on Moonlight, the flare (35% of the Wrath's damage; Solar Return 60% more) ends the sequence; else it
    // leaves Sunlight.
    void ConvergenceWrath(Player* player, Unit* enemy, float wrath)
    {
        if (!enemy->HasAura(SPELL_MOONLIGHT, player->GetGUID()))
        {
            if (enemy->IsAlive())
                Mark(player, enemy, SPELL_SUNLIGHT);
            return;
        }
        enemy->RemoveAurasDueToSpell(SPELL_MOONLIGHT, player->GetGUID());
        float flare = wrath * FLARE_SHARE * (HasConvergenceMarker(player, MOD_SOLAR_RETURN) ? SOLAR_RETURN : 1.0f);
        Strike(player, enemy, SPELL_SOLAR_FLARE, flare);
        if (HasConvergenceMarker(player, MOD_LUNAR_DOMINANCE))
            Renew(player, SPELL_LUNAR_DOMINANCE);
        if (HasConvergenceMarker(player, MOD_STEADY_ORBIT))
            Renew(player, SPELL_STEADY_ORBIT);
        if (HasConvergenceMarker(player, MOD_EVENTIDE))
        {
            State& state = StateOf(player);
            Position at = enemy->GetPosition();
            state.EventideAt = at;
            state.EventideMap = player->GetMapId();
            state.EventideUntil = Now() + EVENTIDE_MS;
            state.EventideTimer = 0;
            state.EventideTick = flare * EVENTIDE_SHARE;
            PlaceDecals(player, SPELL_EVENTIDE, { at });
        }
    }

    // Starfire on Sunlight: the collision (40% of the Starfire's damage; Perfect Alignment 40% more; Long Twilight
    // 5% more for each 2 sec the Sunlight was on, up to 20%), then Moonlight. Converging Skies: half to up to 4 more
    // within 6 yards; Astral Rupture: 150% of it over 6 sec instead; Crossed Horizons: 35% to up to 3 enemies within
    // 12 yards in the Hero's Moonlight.
    void ConvergenceStarfire(Player* player, Unit* enemy, float starfire)
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
        float collision = starfire * COLLISION_SHARE * bonus
            * (HasConvergenceMarker(player, MOD_PERFECT_ALIGNMENT) ? PERFECT_ALIGNMENT : 1.0f);
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
        if (enemy->IsAlive())
            Mark(player, enemy, SPELL_MOONLIGHT);
    }

    // Eventide's field: half the flare every second to up to 5 enemies within 6 yards, 4 sec.
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
        for (uint32 spell : { SPELL_URSOCS_GUARD, SPELL_LASTING_STAND, SPELL_MOVING_MOUNTAIN })
            player->RemoveAurasDueToSpell(spell);
    }

    // Barkskin in Bear Form: Ursoc's Guard, an absorb of 30% of maximum health (Ancient Hide 45%) for 6 sec (Lasting
    // Stand 10, healing taken 25% more meanwhile; Moving Mountain 40% speed meanwhile); Barkskin back 30 sec sooner.
    void RaiseGuard(Player* player)
    {
        State& state = StateOf(player);
        EndGuard(player);
        int32 shield = std::max<int32>(1, int32(player->GetMaxHealth()
            * (HasUrsocMarker(player, MOD_ANCIENT_HIDE) ? ANCIENT_HIDE_PCT : GUARD_PCT) / 100.0f));
        int32 ms = HasUrsocMarker(player, MOD_LASTING_STAND) ? LASTING_STAND_MS : GUARD_MS;
        Report(player, SPELL_URSOCS_GUARD, 'A', shield);
        player->CastCustomSpell(player, SPELL_URSOCS_GUARD, &shield, nullptr, nullptr, true);
        if (Aura* guard = player->GetAura(SPELL_URSOCS_GUARD, player->GetGUID()))
        {
            guard->SetMaxDuration(ms);
            guard->SetDuration(ms);
        }
        state.GuardShield = shield;
        state.GuardBlows = 0;
        state.GuardFrom = player->GetPosition();
        state.GuardTravelled = false;
        uint32 extra = HasUrsocMarker(player, MOD_LASTING_STAND) ? SPELL_LASTING_STAND
            : HasUrsocMarker(player, MOD_MOVING_MOUNTAIN) ? SPELL_MOVING_MOUNTAIN : 0;
        if (extra)
            if (Aura* aura = player->AddAura(extra, player))
            {
                aura->SetMaxDuration(ms);
                aura->SetDuration(ms);
            }
    }

    // Maul in the guard: the blow (75% of attack power and all the guard absorbed, that part at most 250% of attack
    // power; Unbroken: the 75% only, the guard kept, 3 blows a guard) in a 10-yard cone in front (up to 5; Ursoc's
    // Bite: Maul's victim only, double; Mountain's Roar: 180 degrees, up to 10; Earthbreaker: a 25-yard line, 50%
    // more past 10). Enduring Champion: all the guard left over stays as a shield 8 sec. Challenger's Roar: the
    // struck deal 20% less for 6 sec, and the blow's threat is doubled.
    void Release(Player* player, Unit* victim)
    {
        State& state = StateOf(player);
        AuraEffect* guard = GuardEffect(player);
        if (!guard || state.GuardBlows >= UNBROKEN_BLOWS)
            return;
        float ap = player->GetTotalAttackPowerValue(BASE_ATTACK);
        int32 left = std::max(0, guard->GetAmount());
        float absorbed = float(std::max(0, state.GuardShield - left));
        bool unbroken = HasUrsocMarker(player, MOD_UNBROKEN);
        float blow = ap * URSOC_AP + (unbroken ? 0.0f : std::min(absorbed * ABSORBED_SHARE, ap * ABSORBED_MOST_AP));
        std::vector<Unit*> struck;
        if (HasUrsocMarker(player, MOD_URSOCS_BITE))
        {
            if (Valid(player, victim))
                struck.push_back(victim);
            blow *= URSOCS_BITE;
        }
        else if (HasUrsocMarker(player, MOD_EARTHBREAKER))
        {
            Shape line;
            line.From = player->GetPosition();
            line.To = Toward(line.From, player->GetOrientation(), EARTHBREAKER_REACH);
            line.HalfWidth = EARTHBREAKER_HALF_WIDTH;
            struck = EnemiesIn(player, line, URSOC_TARGETS);
        }
        else
        {
            bool roar = HasUrsocMarker(player, MOD_MOUNTAINS_ROAR);
            Shape cone;
            cone.Cone = true;
            cone.From = player->GetPosition();
            cone.Facing = player->GetOrientation();
            cone.Radius = URSOC_REACH;
            cone.Arc = roar ? ROAR_ARC : URSOC_ARC;
            struck = EnemiesIn(player, cone, roar ? ROAR_TARGETS : URSOC_TARGETS);
        }
        player->CastSpell(player, HasUrsocMarker(player, MOD_MOUNTAINS_ROAR) ? SPELL_URSOC_ROAR_LOOK
            : SPELL_URSOC_BLOW_LOOK, true);
        bool challenge = HasUrsocMarker(player, MOD_CHALLENGERS_ROAR);
        for (Unit* enemy : struck)
        {
            float amount = blow;
            if (HasUrsocMarker(player, MOD_EARTHBREAKER) && player->GetExactDist2d(enemy) > URSOC_REACH)
                amount *= EARTHBREAKER_FAR;
            Strike(player, enemy, SPELL_URSOC_BLOW, amount);
            if (!enemy->IsAlive())
                continue;
            if (challenge)
            {
                Lasting(player, enemy, SPELL_CHALLENGERS_ROAR, CHALLENGERS_MS);
                enemy->GetThreatMgr().AddThreat(player, amount * CHALLENGERS_THREAT);
            }
        }
        if (unbroken)
        {
            ++state.GuardBlows;                     // the guard stays; 3 blows a guard
            return;
        }
        if (HasUrsocMarker(player, MOD_ENDURING_CHAMPION) && left > 0)
        {
            int32 kept = std::max<int32>(1, int32(float(left) * ENDURING_SHARE));
            player->RemoveAurasDueToSpell(SPELL_ENDURING_CHAMPION);
            Report(player, SPELL_ENDURING_CHAMPION, 'A', kept);
            player->CastCustomSpell(player, SPELL_ENDURING_CHAMPION, &kept, nullptr, nullptr, true);
            if (Aura* shield = player->GetAura(SPELL_ENDURING_CHAMPION, player->GetGUID()))
            {
                shield->SetMaxDuration(ENDURING_MS);
                shield->SetDuration(ENDURING_MS);
            }
        }
        EndGuard(player);
    }

    // The guard out: it ends with Bear Form; Moving Mountain's 5 yards travelled add 10% of maximum health to it,
    // once.
    void GuardTick(Player* player)
    {
        State& state = StateOf(player);
        if (!state.GuardShield)
            return;
        AuraEffect* guard = GuardEffect(player);
        if (!guard || !InBearForm(player))
        {
            EndGuard(player);
            return;
        }
        if (HasUrsocMarker(player, MOD_MOVING_MOUNTAIN) && !state.GuardTravelled
            && player->GetExactDist2d(&state.GuardFrom) >= MOUNTAIN_TRAVEL)
        {
            state.GuardTravelled = true;
            int32 more = int32(player->GetMaxHealth() * MOUNTAIN_PCT / 100.0f);
            guard->ChangeAmount(guard->GetAmount() + more);
            state.GuardShield += more;
        }
    }

    void ClearDruids(Player* player)
    {
        State& state = StateOf(player);
        state.Owls = 0;
        state.OwlCount = 0;
        for (uint32 spell : { SPELL_SWIFT_JUDGMENT, SPELL_WATCHFUL_FLIGHT, SPELL_GUIDING_WINGS,
                              SPELL_GUIDING_WINGS_SPEED, SPELL_STEADY_ORBIT, SPELL_LUNAR_DOMINANCE,
                              SPELL_ENDURING_CHAMPION })
            player->RemoveAurasDueToSpell(spell);
        for (ObjectGuid guid : state.Marked)
            if (Unit* unit = ObjectAccessor::GetUnit(*player, guid))
            {
                unit->RemoveAurasDueToSpell(SPELL_SUNLIGHT, player->GetGUID());
                unit->RemoveAurasDueToSpell(SPELL_MOONLIGHT, player->GetGUID());
            }
        state.Marked.clear();
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

    // A Sinister Strike's hit on its target, before it lands: Deep Cut 25% more; Advance's enemy within 5 sec 30%.
    float SabreBonus(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        float bonus = HasDuelistMarker(player, MOD_DEEP_CUT) ? DEEP_CUT : 1.0f;
        if (state.AdvanceUntil > Now() && target->GetGUID() == state.AdvanceOn)
        {
            bonus *= ADVANCE_BONUS;
            state.AdvanceUntil = 0;
        }
        return bonus;
    }

    // A Sinister Strike that hit (damage, its target's): every second readies Opening Shot; Sweeping Steel: half to
    // up to 3 more within 5 yards; Serrated Edge: 40% more over 6 sec; Press the Advantage's second strike (half).
    void SabreSlash(Player* player, Unit* target, float damage)
    {
        State& state = StateOf(player);
        if (++state.SabreHits >= OPENING_EVERY)
        {
            state.SabreHits = 0;
            RenewFor(player, SPELL_OPENING_SHOT, OPENING_MS);
            Trace(player, "Opening Shot ready");
        }
        if (HasDuelistMarker(player, MOD_SWEEPING_STEEL))
        {
            std::size_t count = SWEEPING_TARGETS;
            for (Unit* enemy : EnemiesAround(player, target, SWEEPING_RADIUS))
                if (enemy != target && count)
                {
                    Strike(player, enemy, SPELL_SABRE_STRIKE, damage * SWEEPING_SHARE);
                    --count;
                }
        }
        if (HasDuelistMarker(player, MOD_SERRATED_EDGE) && target->IsAlive())
            AddDot(player, target, SPELL_SERRATED_EDGE, damage * SERRATED_SHARE, SERRATED_MS);
        if (player->HasAura(SPELL_PRESS_THE_ADVANTAGE))
        {
            player->RemoveAurasDueToSpell(SPELL_PRESS_THE_ADVANTAGE);
            ObjectGuid guid = target->GetGUID();
            float second = damage * PRESS_SHARE;
            player->m_Events.AddEventAtOffset([player, guid, second]()
            {
                Unit* at = ObjectAccessor::GetUnit(*player, guid);
                if (player->IsInWorld() && player->IsAlive() && Valid(player, at))
                    Strike(player, at, SPELL_SABRE_STRIKE, second);
            }, Milliseconds(PRESS_DELAY_MS));
        }
    }

    // Pistol Shot on its target: 60% of attack power (Heavy Ball 60% more) and a combo point; Scattershot: a
    // 10-yard cone, up to 4 more for 75%; Piercing Round: up to 3 within 15 yards behind it for 80%. Then the
    // footwork: Advance (speed, the next Sinister Strike on it 30% more), Disengaging Shot (speed, the next shot 10
    // Energy back), Press the Advantage (the next Sinister Strike twice).
    void FirePistol(Player* player, Unit* target)
    {
        player->RemoveAurasDueToSpell(SPELL_OPENING_SHOT);
        if (player->HasAura(SPELL_DISENGAGING_DISCOUNT))
        {
            player->RemoveAurasDueToSpell(SPELL_DISENGAGING_DISCOUNT);
            player->ModifyPower(POWER_ENERGY, DISENGAGING_REFUND);
        }
        float shot = player->GetTotalAttackPowerValue(BASE_ATTACK) * PISTOL_AP
            * (HasDuelistMarker(player, MOD_HEAVY_BALL) ? HEAVY_BALL : 1.0f);
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
        else if (HasDuelistMarker(player, MOD_PIERCING_ROUND))
        {
            Shape line;
            line.From = target->GetPosition();
            line.To = Toward(line.From, player->GetAbsoluteAngle(target), PIERCING_REACH);
            line.HalfWidth = PIERCING_HALF_WIDTH;
            std::size_t count = PIERCING_TARGETS;
            for (Unit* enemy : EnemiesIn(player, line, PIERCING_TARGETS + 1))
                if (enemy != target && count)
                {
                    Strike(player, enemy, SPELL_PISTOL_DAMAGE, shot * PIERCING_SHARE);
                    --count;
                }
        }
        if (target->IsAlive())
            player->AddComboPoints(target, 1);
        State& state = StateOf(player);
        if (HasDuelistMarker(player, MOD_ADVANCE))
        {
            Renew(player, SPELL_ADVANCE);
            state.AdvanceOn = target->GetGUID();
            state.AdvanceUntil = Now() + FOOTWORK_MS;
        }
        else if (HasDuelistMarker(player, MOD_DISENGAGING_SHOT))
        {
            Renew(player, SPELL_DISENGAGING_SHOT);
            RenewFor(player, SPELL_DISENGAGING_DISCOUNT, DISCOUNT_MS);
        }
        else if (HasDuelistMarker(player, MOD_PRESS_THE_ADVANTAGE))
            RenewFor(player, SPELL_PRESS_THE_ADVANTAGE, FOOTWORK_MS);
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

    // Broadside: 3 blasts at its target (30% of attack power per combo point to it and up to 7 within 8 yards each),
    // the last a great explosion.
    void FireBroadside(Player* player, Unit* target, uint8 points)
    {
        float blast = player->GetTotalAttackPowerValue(BASE_ATTACK) * BROADSIDE_AP * float(points);
        Trace(player, Acore::StringFormat("Broadside: {} combo points, {} a blast", points, int32(blast)));
        ObjectGuid guid = target->GetGUID();
        for (uint32 i = 0; i < BROADSIDE_BLASTS; ++i)
            player->m_Events.AddEventAtOffset([player, guid, blast, i]()
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
                if (i + 1 == BROADSIDE_BLASTS)
                    at->CastSpell(at, SPELL_BROADSIDE_LOOK, true);
            }, Milliseconds(i * BROADSIDE_SPACING_MS));
    }

    // The Duelist teaches Sabre Slash, Pistol Shot and Broadside while it is bound (Sinister Strike's buttons become
    // Sabre Slash's); unbound, they go and Sabre Slash's buttons are Sinister Strike's again.
    void TeachDuelist(Player* player, bool bound)
    {
        for (uint32 spellId : { SPELL_SABRE_SLASH, SPELL_PISTOL_SHOT, SPELL_BROADSIDE })
            if (bound && !player->HasSpell(spellId))
                player->learnSpell(spellId);
        uint32 sinister = 0;
        if (SpellInfo const* first = sSpellMgr->GetSpellInfo(SPELL_SINISTER_STRIKE))
            for (SpellInfo const* rank = first->GetLastRankSpell(); rank && !sinister; rank = rank->GetPrevRankSpell())
                if (player->HasSpell(rank->Id))
                    sinister = rank->Id;
        bool swapped = false;
        for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
        {
            ActionButton const* action = player->GetActionButton(button);
            if (!action || action->GetType() != ACTION_BUTTON_SPELL)
                continue;
            uint32 spellId = action->GetAction();
            SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
            if (bound && info && info->GetFirstRankSpell()->Id == SPELL_SINISTER_STRIKE)
            {
                player->addActionButton(button, SPELL_SABRE_SLASH, ACTION_BUTTON_SPELL);
                swapped = true;
            }
            else if (!bound && spellId == SPELL_SABRE_SLASH && sinister)
            {
                player->addActionButton(button, sinister, ACTION_BUTTON_SPELL);
                swapped = true;
            }
        }
        if (!bound)
            for (uint32 spellId : { SPELL_SABRE_SLASH, SPELL_PISTOL_SHOT, SPELL_BROADSIDE })
                if (player->HasSpell(spellId))
                    player->removeSpell(spellId, SPEC_MASK_ALL, false);
        if (swapped)
            player->SendActionButtons(1);
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
        float pulse = player->GetTotalAttackPowerValue(BASE_ATTACK) * STORM_PULSE_AP * (rolling ? ROLLING_SHARE : 1.0f);
        if (struck.size() == 1 && HasStormreaverMarker(player, MOD_EYE_OF_THE_TEMPEST))
            pulse *= EYE_OF_THE_TEMPEST;
        for (Unit* enemy : struck)
            Strike(player, enemy, SPELL_STORMFRONT_PULSE, pulse);
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
        float ap = player->GetTotalAttackPowerValue(BASE_ATTACK);
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
        if (HasStormreaverMarker(player, MOD_OVERCHARGED_CORE))
            discharge *= OVERCHARGED;
        Strike(player, target, SPELL_STORM_DISCHARGE, discharge);
        Trace(player, Acore::StringFormat("Lightning Shield discharges for {}", int32(discharge)));
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

    // A fresh Cinder Wound's whole damage: 30% of attack power plus 30% of spell power (Searing Wound 35% more,
    // Smoldering Flesh 30% more over 10 sec).
    float CinderFresh(Player* player)
    {
        float fresh = player->GetTotalAttackPowerValue(BASE_ATTACK) * CINDER_AP
            + float(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE)) * CINDER_SP;
        if (HasEmbersMarker(player, MOD_SEARING_WOUND))
            fresh *= SEARING_WOUND;
        else if (HasEmbersMarker(player, MOD_SMOLDERING_FLESH))
            fresh *= SMOLDERING_FLESH;
        return fresh;
    }

    int32 CinderMs(Player* player)
    {
        return HasEmbersMarker(player, MOD_SMOLDERING_FLESH) ? SMOLDERING_MS : CINDER_MS;
    }

    // Ashen Pursuit: the first Fire Blast on a new victim, once every 6 sec (its last victim's death resets that).
    float AshenBonus(Player* player, Unit* target)
    {
        if (!HasEmbersMarker(player, MOD_ASHEN_PURSUIT))
            return 1.0f;
        State& state = StateOf(player);
        if (state.AshenLast)
        {
            Unit* last = ObjectAccessor::GetUnit(*player, state.AshenLast);
            if (!last || !last->IsAlive())
                state.AshenReadyAt = 0;
        }
        ObjectGuid guid = target->GetGUID();
        if (std::find(state.AshenSeen.begin(), state.AshenSeen.end(), guid) != state.AshenSeen.end()
            || Now() < state.AshenReadyAt)
            return 1.0f;
        state.AshenSeen.push_back(guid);
        if (state.AshenSeen.size() > 64)
            state.AshenSeen.erase(state.AshenSeen.begin());
        state.AshenLast = guid;
        state.AshenReadyAt = Now() + ASHEN_MS;
        Trace(player, "Ashen Pursuit: a new victim");
        return ASHEN_PURSUIT;
    }

    // Fire Blast landed: it ignites the Hero's Rake or Rip victim (Wild Embers: the flames leap to up to
    // 4 more within 6 yards); Flamebound: speed, and the next Rake or Mangle 10 Energy back.
    void CatFireBlast(Player* player, Unit* target)
    {
        if (HasEmbersMarker(player, MOD_FLAMEBOUND))
        {
            RenewFor(player, SPELL_FLAMEBOUND, FLAMEBOUND_MS);
            RenewFor(player, SPELL_FLAMEBOUND_DISCOUNT, FLAMEBOUND_DISCOUNT_MS);
        }
        if (!target->IsAlive() || !HasOwnRakeOrRip(player, target))
            return;
        float fresh = CinderFresh(player);
        AddDot(player, target, SPELL_CINDER_WOUND, fresh, CinderMs(player));
        Trace(player, Acore::StringFormat("Cinder Wound: {} over {} sec", int32(fresh), CinderMs(player) / 1000));
        if (HasEmbersMarker(player, MOD_WILD_EMBERS))
        {
            float leap = player->GetTotalAttackPowerValue(BASE_ATTACK) * WILD_EMBERS_AP
                + float(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE)) * WILD_EMBERS_SP;
            std::size_t count = WILD_EMBERS_TARGETS;
            for (Unit* enemy : EnemiesAround(player, target, WILD_EMBERS_RADIUS))
                if (enemy != target && count)
                {
                    Strike(player, enemy, SPELL_WILD_EMBERS, leap);
                    --count;
                }
        }
    }

    // Swipe in Cat Form with Cinder Swipe: a Cinder Wound in its reach spreads, half as strong, to up to 3 bleeding
    // enemies there (spread wounds never spread again).
    void CinderSwipe(Player* player)
    {
        std::vector<Unit*> around = EnemiesAround(player, player, CINDER_SWIPE_RADIUS);
        if (std::none_of(around.begin(), around.end(), [player](Unit* enemy)
            {
                return enemy->HasAura(SPELL_CINDER_WOUND, player->GetGUID());
            }))
            return;
        float fresh = CinderFresh(player) * CINDER_SWIPE_SHARE;
        std::size_t count = CINDER_SWIPE_TARGETS;
        for (Unit* enemy : around)
            if (count && !enemy->HasAura(SPELL_CINDER_WOUND, player->GetGUID())
                && enemy->HasAuraWithMechanic(1ULL << MECHANIC_BLEED))
            {
                AddDot(player, enemy, SPELL_CINDER_WOUND_SPREAD, fresh, CinderMs(player));
                --count;
            }
    }

    // Icy Touch landed: Frozen Prey on its victim (one primary mark: an earlier one goes); Pursuing
    // Chill: speed, and the shattering Shred or Mangle in its time 10 Energy back.
    void CatIcyTouch(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        if (state.PreyOn && state.PreyOn != target->GetGUID())
            if (Unit* earlier = ObjectAccessor::GetUnit(*player, state.PreyOn))
                earlier->RemoveAurasDueToSpell(SPELL_FROZEN_PREY, player->GetGUID());
        int32 ms = HasWhitefangMarker(player, MOD_LONG_WINTER) ? LONG_WINTER_MS : PREY_MS;
        Lasting(player, target, SPELL_FROZEN_PREY, ms);
        state.PreyOn = target->GetGUID();
        state.PreyAt = Now();
        Trace(player, "Frozen Prey laid");
        if (HasWhitefangMarker(player, MOD_PURSUING_CHILL))
        {
            RenewFor(player, SPELL_PURSUING_CHILL, PURSUING_MS);
            state.PursuitDiscount = true;
        }
    }

    // Frozen Trail: a 6-yard patch of frost where the prey shattered, for 4 sec: each second up to 5 enemies in it
    // take 20% of attack power as Frost damage and move 30% slower.
    void LayFrozenTrail(Player* player, Unit* target)
    {
        Position at = target->GetPosition();
        PlaceDecals(player, SPELL_FROZEN_TRAIL, { at });
        float tick = player->GetTotalAttackPowerValue(BASE_ATTACK) * FROZEN_TRAIL_AP;
        for (uint32 i = 1; i <= FROZEN_TRAIL_TICKS; ++i)
            player->m_Events.AddEventAtOffset([player, at, tick]()
            {
                if (!player->IsInWorld() || !player->IsAlive())
                    return;
                for (Unit* enemy : EnemiesIn(player, Circle(at, FROZEN_TRAIL_RADIUS), FROZEN_TRAIL_TARGETS))
                {
                    Strike(player, enemy, SPELL_FROZEN_TRAIL_TICK, tick);
                    Lasting(player, enemy, SPELL_FROZEN_TRAIL_SLOW, TRAIL_SLOW_MS);
                }
            }, Milliseconds(i * FROZEN_TRAIL_TICK_MS));
    }

    // A direct Shred or Mangle on a marked victim shatters the mark: 150% of attack power as Frost damage (Whitefang
    // 40% more; Long Winter 20% more for a mark at least 3 sec old; a Pack Frost mark 60%), or Winter Wound's 150%
    // over 6 sec; Icefang Spray: half to up to 4 in an 8-yard cone. Then the primary's Pack Frost marks and the
    // instincts: Cold Blood, Snowblind, Frozen Trail.
    void Shatter(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        bool primary = target->HasAura(SPELL_FROZEN_PREY, player->GetGUID());
        bool pack = !primary && target->HasAura(SPELL_FROZEN_PREY_PACK, player->GetGUID());
        if (!primary && !pack)
            return;
        target->RemoveAurasDueToSpell(primary ? SPELL_FROZEN_PREY : SPELL_FROZEN_PREY_PACK, player->GetGUID());
        if (state.PursuitDiscount && player->HasAura(SPELL_PURSUING_CHILL))
            player->ModifyPower(POWER_ENERGY, PURSUING_REFUND);
        state.PursuitDiscount = false;
        float damage = player->GetTotalAttackPowerValue(BASE_ATTACK) * SHATTER_AP;
        if (HasWhitefangMarker(player, MOD_WHITEFANG))
            damage *= WHITEFANG_BONUS;
        if (primary && HasWhitefangMarker(player, MOD_LONG_WINTER) && Now() - state.PreyAt >= LONG_WINTER_AFTER_MS)
            damage *= LONG_WINTER_BONUS;
        if (pack)
            damage *= PACK_FROST_SHARE;
        if (primary)
            state.PreyOn.Clear();
        Trace(player, Acore::StringFormat("Shatter for {}", int32(damage)));
        if (HasWhitefangMarker(player, MOD_WINTER_WOUND))
            AddDot(player, target, SPELL_WINTER_WOUND, damage * WINTER_WOUND_SHARE, WINTER_WOUND_MS);
        else
            Strike(player, target, SPELL_FANG_SHATTER, damage);
        if (HasWhitefangMarker(player, MOD_ICEFANG_SPRAY))
        {
            Shape cone;
            cone.Cone = true;
            cone.From = player->GetPosition();
            cone.Facing = player->GetAbsoluteAngle(target);
            cone.Radius = ICEFANG_REACH + player->GetExactDist2d(target);
            cone.Arc = ICEFANG_ARC;
            std::size_t count = ICEFANG_TARGETS;
            for (Unit* enemy : EnemiesIn(player, cone, ICEFANG_TARGETS + 1))
                if (enemy != target && count)
                {
                    Strike(player, enemy, SPELL_FANG_SHATTER, damage * ICEFANG_SHARE);
                    --count;
                }
        }
        if (primary && HasWhitefangMarker(player, MOD_PACK_FROST))
        {
            std::size_t count = PACK_FROST_TARGETS;
            for (Unit* enemy : EnemiesAround(player, target, PACK_FROST_RADIUS))
                if (enemy != target && count)
                {
                    Lasting(player, enemy, SPELL_FROZEN_PREY_PACK, PACK_FROST_MS);
                    --count;
                }
        }
        if (HasWhitefangMarker(player, MOD_FANG_COLD_BLOOD))
        {
            RenewFor(player, SPELL_FANG_COLD_BLOOD, FANG_COLD_BLOOD_MS);
            state.ColdBloodOn = target->GetGUID();
        }
        else if (HasWhitefangMarker(player, MOD_SNOWBLIND))
        {
            for (Unit* enemy : EnemiesAround(player, target, SNOWBLIND_RADIUS))
                Lasting(player, enemy, SPELL_SNOWBLIND_SLOW, SNOWBLIND_MS);
            RenewFor(player, SPELL_SNOWBLIND, SNOWBLIND_MS);
        }
        else if (HasWhitefangMarker(player, MOD_FROZEN_TRAIL))
            LayFrozenTrail(player, target);
    }

    // Ferocious Bite on the Hero's own victim: Cold Blood's 20% (it is spent); Molten Bite on a Cinder Wound victim:
    // 25% of attack power per combo point as Fire damage more.
    float BiteBonus(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        if (!player->HasAura(SPELL_FANG_COLD_BLOOD) || state.ColdBloodOn != target->GetGUID())
            return 1.0f;
        player->RemoveAurasDueToSpell(SPELL_FANG_COLD_BLOOD);
        state.ColdBloodOn.Clear();
        return FANG_COLD_BLOOD;
    }

    // Kindled or Frostbitten Claw (its hit, damage): a second enemy within 5 yards takes it again as Fire or Frost,
    // and both bleed (Blazing Strike) or are frostbitten and slowed (Frostfang Strike).
    void EmpoweredClaw(Player* player, Unit* target, float damage, bool fire)
    {
        player->RemoveAurasDueToSpell(fire ? SPELL_KINDLED : SPELL_FROSTBITTEN);
        std::vector<Unit*> struck { target };
        for (Unit* enemy : EnemiesAround(player, target, CLAW_SECOND_RADIUS))
            if (enemy != target)
            {
                Strike(player, enemy, fire ? SPELL_BLAZING_STRIKE : SPELL_FROSTFANG_STRIKE, damage * CLAW_SECOND_SHARE);
                struck.push_back(enemy);
                break;
            }
        float dot = player->GetTotalAttackPowerValue(BASE_ATTACK) * CLAW_DOT_AP;
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
        float blast = player->GetTotalAttackPowerValue(BASE_ATTACK) * NOVA_AP;
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
        state.AdvanceUntil = 0;
        state.StormfrontTimer = 0;
        state.BlazingRakeAt = state.AshenReadyAt = state.PreyAt = 0;
        state.AshenLast.Clear();
        state.AshenSeen.clear();
        state.PreyOn.Clear();
        state.ColdBloodOn.Clear();
        state.PursuitDiscount = false;
        player->RemoveAurasDueToSpell(SPELL_KINDLED);
        player->RemoveAurasDueToSpell(SPELL_FROSTBITTEN);
        for (uint32 spell : { SPELL_OPENING_SHOT, SPELL_ADVANCE, SPELL_DISENGAGING_SHOT, SPELL_DISENGAGING_DISCOUNT,
                              SPELL_PRESS_THE_ADVANTAGE, SPELL_STORMFRONT, SPELL_RIDE_THE_LIGHTNING, SPELL_FLAMEBOUND,
                              SPELL_FLAMEBOUND_DISCOUNT, SPELL_PURSUING_CHILL, SPELL_SNOWBLIND,
                              SPELL_FANG_COLD_BLOOD })
            player->RemoveAurasDueToSpell(spell);
        player->RemoveDynObject(SPELL_FROZEN_TRAIL);
    }

    // Everything the five newest left out (death, a rune or spec change, logout).
    void ClearNewest(Player* player)
    {
        State& state = StateOf(player);
        for (uint32 spell : { SPELL_ARCANE_INSIGHT, SPELL_MIND_UNBOUND, SPELL_MENTAL_RESERVES, SPELL_FROST_CROWN,
                              SPELL_SPLINTER_QUICKENING, SPELL_COLD_FRONT, SPELL_ASHEN_FLOCK })
            player->RemoveAurasDueToSpell(spell);
        state.Icicles = 0;
        state.IcicleAttacks = 0;
        state.FrostboltHits = 0;
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
            for (uint32 i = 0; i < SLOTS * CHOICES; ++i)
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
        bool any = false, paragon = false, duelist = false;
        for (uint8 runeSlot = 0; runeSlot < RUNE_SLOTS; ++runeSlot)
        {
            Choice const& choice = ActiveChoice(player, runeSlot);
            RuneDef const* rune = FindRune(choice.Rune);
            if (!rune || player->GetLevel() < SlotUnlock(runeSlot))
                continue;
            any = true;
            paragon |= rune->Id == RUNE_SANCTIFIED_PARAGON;
            duelist |= rune->Id == RUNE_DUELIST;
            player->AddAura(rune->Aura, player);
            for (uint8 slot = 0; slot < SLOTS; ++slot)
                if (uint8 pick = choice.Mods[slot])
                    player->AddAura(rune->MarkerFirst + slot * CHOICES + pick - 1, player);
        }
        if (!any)
            ClearCombat(player);
        if (!paragon)
            player->RemoveAurasDueToSpell(SPELL_SANCTIFIED_PARAGON);
        TeachDuelist(player, duelist);
        RetireCatCopy(player, SPELL_CAT_FIRE_BLAST, SPELL_FIRE_BLAST);
        RetireCatCopy(player, SPELL_CAT_ICY_TOUCH, SPELL_ICY_TOUCH);
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
                    choice.Mods[slot] = std::min<uint8>(fields[2 + slot].Get<uint8>(), CHOICES);
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

    // ---- The panel ---------------------------------------------------------------------------------------------

    // A slot open and empty: the panel opens by itself.
    bool Prompt(Player* player)
    {
        if (!Wildcard::IsHero(player) || Specs::IsDormant(player))
            return false;
        for (uint8 runeSlot = 0; runeSlot < RUNE_SLOTS; ++runeSlot)
            if (player->GetLevel() >= SlotUnlock(runeSlot) && !ActiveChoice(player, runeSlot).Rune)
                return true;
        return false;
    }

    void SendState(Player* player)
    {
        Choice const& choice = ActiveChoice(player);
        Choice const& second = ActiveChoice(player, 1);
        uint8 role = Wildcard::GetRole(player);
        // Suggested: runes the Hero meets, their role's first (a Tank or Healer sees a hybrid only in Browse all).
        std::vector<RuneDef const*> offers;
        for (RuneDef const& rune : Catalog())
            if (!IsRetired(rune.Id) && Missing(player, rune.Requires) == "-"
                && (role == Wildcard::ROLE_NONE || rune.Roles & Wildcard::RoleMask(Wildcard::Role(role))))
                offers.push_back(&rune);
        if (offers.size() > MAX_OFFERS + 1)
            offers.resize(MAX_OFFERS + 1);              // one more: the panel leaves out the other slot's rune
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
        ChatHandler(player->GetSession()).PSendSysMessage("|cffa335eeLegendary Rune:|r bound to this "
            "specialization until your next prestige. Choose its modifiers in the rune panel.");
        SendState(player);
    }

    void OnModifier(Player* player, uint8 slot, uint8 pick, uint8 runeSlot)
    {
        if (runeSlot >= RUNE_SLOTS)
            return;
        Choice& choice = ActiveChoice(player, runeSlot);
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
        Save(player, Specs::ActiveSpec(player), runeSlot);
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
            OnPick(player, uint8(number(1)), number(2) == 1, uint8(std::max<uint32>(number(3), 1) - 1));
        else if (parts[0] == "MOD")
            OnModifier(player, uint8(number(1)), uint8(number(2)), uint8(std::max<uint32>(number(3), 1) - 1));
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

    void Tick(AuraEffect const* aurEff)
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        if (!caster || !target || !target->IsAlive())
            return;
        // The tick the script keeps; else the one the aura was cast with (never nothing).
        auto tick = dotTicks.find({ target->GetGUID(), GetId(), caster->GetGUID() });
        int32 damage = std::max(1, tick != dotTicks.end() ? tick->second : aurEff->GetAmount());
        // Patient Predator: a Cinder Wound's last tick, twice its damage.
        if ((GetId() == SPELL_CINDER_WOUND || GetId() == SPELL_CINDER_WOUND_SPREAD)
            && HasEmbersMarker(caster, MOD_PATIENT_PREDATOR) && GetAura()->GetDuration() < aurEff->GetAmplitude())
            damage = int32(damage * PATIENT_PREDATOR);
        if (Player* player = caster->ToPlayer())
            Trace(player, Acore::StringFormat("{} ticks for {}", GetSpellInfo()->SpellName[0], damage));
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
        Report(hero, SPELL_HOLLOW_FLAY, 'D', damage);
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

    // Every level: the panel's state (its level); at UNLOCK_LEVEL the panel opens while the active spec has no rune.
    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (!Wildcard::IsHero(player))
            return;
        Apply(player);
        SendState(player);
        // A slot opening: the panel opens (the second at level 30, with a word of it).
        bool opened = false;
        for (uint8 runeSlot = 0; runeSlot < RUNE_SLOTS; ++runeSlot)
            opened |= oldLevel < SlotUnlock(runeSlot) && player->GetLevel() >= SlotUnlock(runeSlot);
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

// Arcane Blast (every rank) with Arcane Mind: a direct hit grants Insight.
class spell_titan_rune_arcane_blast : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_arcane_blast);

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        if (player && GetHitUnit() && player->HasAura(SPELL_RUNE_ARCANE_MIND))
            BlastInsight(player);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_arcane_blast::Hit);
    }
};

// Arcane Explosion (every rank) with Arcane Mind and Expanding Intellect: 2 Insight on 3 enemies or more, else 1,
// once a cast.
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
        if (!player || !_hits || !player->HasAura(SPELL_RUNE_ARCANE_MIND)
            || !HasMindMarker(player, MOD_EXPANDING_INTELLECT))
            return;
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

// Arcane Missiles' channel (every rank) with Mental Reserves: an empowered one completed.
class spell_titan_rune_arcane_missiles_channel : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_arcane_missiles_channel);

    void Gone(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* player = GetTarget()->ToPlayer();
        if (!player || !StateOf(player).MindChannel)
            return;
        StateOf(player).MindChannel = false;
        if (GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE
            && player->HasAura(SPELL_RUNE_ARCANE_MIND) && HasMindMarker(player, MOD_MENTAL_RESERVES))
            MindReserves(player);
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

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target && target->IsAlive() && GetHitDamage() > 0 && player->HasAura(SPELL_RUNE_PHOENIX_WAKE))
            PlaceEmber(player, target, float(GetHitDamage()));
    }

    void Spent()
    {
        if (Player* player = GetCaster()->ToPlayer())
            player->RemoveAurasDueToSpell(SPELL_ASHEN_FLOCK);
    }

    void Register() override
    {
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

// Arcane Barrage (every rank) with Astral Horizon: the Orb.
class spell_titan_rune_arcane_barrage : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_arcane_barrage);

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target && player->HasAura(SPELL_RUNE_ASTRAL_HORIZON))
            LaunchOrb(player, target, float(std::max(0, GetHitDamage())));
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_arcane_barrage::Hit);
    }
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

// Starfire (every rank): Elune's Guardian's owl, Astral Convergence's collision (direct hits).
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
            ConvergenceStarfire(player, target, float(GetHitDamage()));
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_rune_starfire::Hit);
    }
};

// Wrath (every rank): Twilight Hunt's 25% more on an owl's marked victim (the mark spent); Astral Convergence's
// Sunlight or flare.
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
        if (player && target && GetHitDamage() > 0 && player->HasAura(SPELL_RUNE_ASTRAL_CONVERGENCE))
            ConvergenceWrath(player, target, float(GetHitDamage()));
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_titan_rune_wrath::Hunted);
        AfterHit += SpellHitFn(spell_titan_rune_wrath::Hit);
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
        Release(player, GetHitUnit());
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
// tried), Ashen Pursuit, then the ignition.
class spell_titan_rune_embers_fire_blast : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_embers_fire_blast);

    SpellCastResult Check()
    {
        return CatFormGate(GetCaster(), SPELL_RUNE_EMBERS_WRATH);
    }

    void Damage()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target && GetHitDamage() > 0 && player->HasAura(SPELL_RUNE_EMBERS_WRATH))
            SetHitDamage(std::max(1, int32(GetHitDamage() * AshenBonus(player, target))));
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
        OnHit += SpellHitFn(spell_titan_rune_embers_fire_blast::Damage);
        AfterHit += SpellHitFn(spell_titan_rune_embers_fire_blast::Ignite);
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
        if (player && (first == SPELL_RAKE || first == SPELL_MANGLE_CAT) && player->HasAura(SPELL_FLAMEBOUND_DISCOUNT))
        {
            player->RemoveAurasDueToSpell(SPELL_FLAMEBOUND_DISCOUNT);
            player->ModifyPower(POWER_ENERGY, FLAMEBOUND_REFUND);
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
        if (first == SPELL_RAKE && player->HasAura(SPELL_RUNE_EMBERS_WRATH)
            && HasEmbersMarker(player, MOD_BLAZING_RAKE) && HasCinderWound(player, target)
            && Now() >= StateOf(player).BlazingRakeAt)
        {
            StateOf(player).BlazingRakeAt = Now() + BLAZING_RAKE_MS;
            Strike(player, target, SPELL_BLAZING_RAKE, player->GetTotalAttackPowerValue(BASE_ATTACK) * BLAZING_RAKE_AP);
        }
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
            SetHitDamage(int32(GetHitDamage() * BiteBonus(player, target)));
        if (player->HasAura(SPELL_RUNE_EMBERS_WRATH) && HasEmbersMarker(player, MOD_MOLTEN_BITE)
            && HasCinderWound(player, target))
            if (uint8 points = player->GetComboPoints(target))
                Strike(player, target, SPELL_MOLTEN_BITE,
                    player->GetTotalAttackPowerValue(BASE_ATTACK) * MOLTEN_BITE_AP * float(points));
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
        if (player->HasAura(SPELL_RUNE_EMBERS_WRATH) && HasEmbersMarker(player, MOD_CINDER_SWIPE))
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

// Sinister Strike, Backstab and Hemorrhage (every rank) on a target Marked for Murder by the Hero: 30% more, and the
// mark is spent.
class spell_titan_rune_marked_strike : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_marked_strike);

    void Strike()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target || !target->HasAura(SPELL_MARKED_FOR_MURDER, caster->GetGUID()))
            return;
        SetHitDamage(int32(GetHitDamage() * MARKED_BONUS));
        target->RemoveAurasDueToSpell(SPELL_MARKED_FOR_MURDER, caster->GetGUID());
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_titan_rune_marked_strike::Strike);
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

    void Bite(Player* hero, Unit* prey, float share, uint32 spellId = SPELL_SPECTRAL_BITE)
    {
        float amount = _bite * share * (HasHuntMarker(hero, MOD_ALPHA_PREDATOR) ? ALPHA_PREDATOR : 1.0f);
        int32 damage = std::max<int32>(1, int32(amount));
        me->SetFacingToObject(prey);
        me->HandleEmoteCommand(EMOTE_ONESHOT_ATTACK1H);
        me->CastCustomSpell(prey, spellId, &damage, nullptr, nullptr, true);
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
    RegisterSpellScript(spell_titan_rune_cat_strike);
    RegisterSpellScript(spell_titan_rune_ferocious_bite);
    RegisterSpellScript(spell_titan_rune_cat_swipe);
    RegisterCreatureAI(npc_titan_rune_spectral_beast);
    RegisterCreatureAI(npc_titan_rune_flier);
    new titan_runes_player();
    new titan_runes_unit();
}
