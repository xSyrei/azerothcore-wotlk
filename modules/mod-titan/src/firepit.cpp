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

#include "firepit.h"
#include "firepit_gear.h"
#include "fireforged_effects.h"
#include "firepit_roster.h"
#include "wildcard.h"
#include "Chat.h"
#include "CommandScript.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "GameObjectScript.h"
#include "GameTime.h"
#include "Group.h"
#include "InstanceSaveMgr.h"
#include "InstanceScript.h"
#include "Item.h"
#include "Mail.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Containers.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "WorldPacket.h"
#include <random>
#include "WorldSession.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// The Firepit: an endless wave arena in a private instance of its own map (760: Trial of the Champion's arena, its
// terrain copied, under a molten sky: titan_firepit.sql, the client patch), solo or for a group of up to 10
// (src/firepit.cpp, docs in README.md).
// - The Firepit Keeper (beside Silas Darkmoon in Stormwind and Orgrimmar) sends the Hero, or the leader's group, into
//   a fresh instance of map 760 (InstanceOverride: the core sends them there, and lets 10 in). Its instance script is
//   the Firepit's own: the map has no creatures of its own (any that load are sent away), and it runs the waves.
// - Waves every 75 sec, or 10 sec after the last enemy dies. Wave n has min(12, 3 + n/5) enemies, plus one for every
//   two Heroes past the first (20 at most), from the Depths' roster (a new kind every wave or two); every 5th wave is
//   one boss alone, the waves waiting while it lives (20 sec after it falls). Survivors stay. At the cap (8 alive, 4
//   more a Hero) the rest wait as reinforcements, and Pressure builds: 5% more enemy damage every 30 sec full, 20% at
//   most, cleared by 10 sec under it.
// - Creatures are at the group's level (dungeon_scaling.cpp); their health H(n) = 0.70 + 0.05 (n-1) + 0.0008 (n-1)^2
//   of a dungeon's, 55% more for each player past the first; the waves leave their damage as it is.
// - The dead stay down (no release: they stay where they fell); a battle resurrection brings them back. The last
//   death ends the run: everyone is brought back at full, each Hero who survived a 15th wave gets a chest of their
//   own (a piece of gear at their level a 15 waves; epic from wave 45), the results go to the Emberforge_Firepit
//   addon, and the best wave is recorded for its group size: solo, duo, trio, group (titan_firepit_records).
namespace
{
    constexpr uint32 MAP_FIREPIT = 760;                 // Trial of the Champion's arena under its own sky
                                                        // (titan_firepit.sql, the client patch's Map.dbc)
    constexpr uint32 MOST_PLAYERS = 10;
    constexpr uint32 NPC_KEEPER = 910140;
    constexpr uint32 GO_SPOILS = 910140;
    constexpr uint32 ENEMY_FACTION = 14;                // hostile to everyone
    constexpr char const* PREFIX = "EFFP";

    Position const CENTER = { 746.5f, 618.5f, 411.2f, 4.71f };
    Position const GATES[] = {                          // dressed with lava pillars (no enemy comes through them:
        { 746.67f, 684.08f, 412.5f, 4.65f },           // the tunnels behind are not all walkable)
        { 805.23f, 618.04f, 412.39f, 3.146f },
    };
    // The enemies come in on the arena's floor, around its edge (the floor is 54 yards out all round; the stands above
    // it). An enemy off the floor (farther out, or up in the stands) is put back in.
    constexpr float SPAWN_RADIUS_MIN = 38.0f, SPAWN_RADIUS_MAX = 46.0f;
    constexpr float ARENA_RADIUS = 52.0f;
    constexpr float ARENA_HEIGHT = 8.0f;                // above the centre's floor: off it
    constexpr float ARRIVAL_RADIUS = 9.0f;              // the Heroes come in around the pit, not in it

    // The arena dressed for the Firepit (titan_firepit.sql: game objects of the client's own fire): the pit in the
    // middle, braziers along the wall, embers rising, lava pillars at the gates.
    enum Decor : uint32
    {
        GO_DARK_IRON_BRAZIER = 910141,
        GO_BONFIRE = 910142,                            // unused (too big: a wall of fire)
        GO_FIRE_PIT = 910143,
        GO_LAVA_PILLAR = 910144,
        GO_FLAME_CIRCLE = 910145,                       // a Molten Eruption's warning (too bright to stand)
        GO_EMBERS = 910146,
        GO_FIRE_RIFT = 910147,                          // a Searing Nova's warning, under the boss
    };
    // The area decals (client-patch build_firepit_decals.py): each shape's outline (a faint fill, a bright rim)
    // and its timer (GO_TIMER_FIRST: the fill that grows to the edge as the blow comes), a yard of radius (a line's
    // piece: a yard either side of its axis, 2 yards long), scaled to the area; in six colours, 16 objects a colour
    // (its 8 outlines, then 8 old timers no longer laid). Danger: get out. Safe: stand here. Soak: share it. Heal:
    // a helpful area. Arcane: magic, something special. Frost: slowing, frozen ground.
    enum Decal : uint8
    {
        DECAL_CIRCLE, DECAL_RING, DECAL_CONE90, DECAL_CONE180, DECAL_CONE180_LARGE, DECAL_CONE120_LARGE,
        DECAL_LINE_MID, DECAL_LINE_CAP,
    };
    enum DecalColour : uint8
    {
        DECAL_DANGER, DECAL_SAFE, DECAL_SOAK, DECAL_HEAL, DECAL_ARCANE, DECAL_FROST,
    };
    constexpr uint32 GO_DECAL_FIRST = 910160, DECALS_A_COLOUR = 16;
    // Their textured overlays, laid over the fill in the colour's glow: runic (a sigil), cracked (molten cracks),
    // hazard (warning stripes), swirl (a smoky spiral); 48 objects a style (8 a colour) from GO_STYLE_FIRST.
    enum DecalStyle : int8
    {
        STYLE_AUTO = -1,                                // chosen from what the blast is (StyleOf)
        STYLE_PLAIN, STYLE_RUNIC, STYLE_CRACKED, STYLE_HAZARD, STYLE_SWIRL,
    };
    constexpr uint32 GO_STYLE_FIRST = 910256, STYLE_OBJECTS = 48, DECAL_SHAPES = 8;
    constexpr float DECAL_LARGE = 15.0f;                // beyond: the large cuts' thinner rims
    constexpr float DECAL_LIFT = 0.15f;                 // yards above the floor (the models lie at their origin)
    constexpr uint32 TIMER_EVERY_MS = 100;
    // The timers (the fill that grows to the edge as the blow comes): the client never redraws an object at a new
    // size, so each grows in its own model, over one of these wind-ups (48 objects each, 8 a colour, from
    // GO_TIMER_FIRST); a blast's is the longest that fits its warning, laid when that much of it is left.
    constexpr uint32 GO_TIMER_FIRST = 910450, TIMER_OBJECTS = 48;
    constexpr std::array<uint32, 11> TIMER_WIND_UPS_MS = { 1000, 1500, 2000, 2500, 3000, 4000, 5000, 6000, 8000,
        10000, 12000 };
    struct Ring
    {
        uint32 entry;
        float radius;
        uint32 count;
        float turn;                                     // the first one's angle (radians)
    };
    // Nothing tall or bright in the fighting space (the bonfires, the ring of flame and the rift walled the view off):
    // the pit low in the middle, the rest along the wall.
    Ring const DECOR_RINGS[] = {
        { GO_FIRE_PIT, 0.0f, 1, 0.0f },
        { GO_EMBERS, 50.0f, 4, 0.785f },
        { GO_DARK_IRON_BRAZIER, 51.0f, 16, 0.2f },
    };
    constexpr float GATE_PILLAR_SPREAD = 7.0f;          // a lava pillar each side of a gate, this far from it

    // The bosses' mechanics, from wave 15 (the third boss): more of them as the waves climb. Their damage is a share
    // of each Hero's own health (wave and level aside: it is about moving, not gear).
    // - Molten Eruption (15+): flame rings under up to 3 Heroes; 3 sec later each erupts, 30% to anyone within 6 yd.
    // - Searing Nova (30+): the boss roots itself and gathers flame; 4 sec later, 50% to anyone within 12 yd of it.
    // - Enrage (45+): under 30% health, the boss's damage x1.5.
    constexpr uint32 MECHANICS_FROM = 15, NOVA_FROM = 30, ENRAGE_FROM = 45;

    // World affixes (The Firepit Ignites): arena-wide rules, not buffs on enemies. A new one enters at waves 10, 20,
    // 30 and 40; from 50, every 10 waves the oldest gives way to the next; four at most. The order is the week's own
    // (a seed of the week: every group sees the same schedule; a checkpoint start the set of its wave). Each one's
    // timer starts fresh as it enters. Their damage is a share of the Hero's maximum health; no two major dangers
    // resolve within 3 sec of one another (the later waits).
    enum Affix : uint8
    {
        AFFIX_NONE, AFFIX_MOLTEN_GROUND, AFFIX_CINDER_WINDS, AFFIX_SEARING_BRAND, AFFIX_EMBERFALL,
        // The expanded pool: priority targets, movement, pressure.
        AFFIX_BOLSTERING, AFFIX_MENDING_EMBERS, AFFIX_CINDER_SHIELD, AFFIX_ASHEN_ECHO, AFFIX_VOLATILE_REMAINS,
        AFFIX_SHACKLING_CHAINS, AFFIX_BURNING_PURSUIT, AFFIX_CRACKED_EARTH, AFFIX_VENGEFUL_FLAME, AFFIX_FLAMEBOUND,
        AFFIX_END
    };
    char const* const AFFIX_NAMES[] = { "", "Molten Ground", "Cinder Winds", "Searing Brand", "Emberfall",
        "Bolstering", "Mending Embers", "Cinder Shield", "Ashen Echo", "Volatile Remains", "Shackling Chains",
        "Burning Pursuit", "Cracked Earth", "Vengeful Flame", "Flamebound" };
    char const* const AFFIX_TEXTS[] = { "",
        "every 14 sec the ground erupts under every Hero and leaves a burning pool for 40 sec: the floor fills up",
        "every 22 sec a burning wind blasts a lane across the arena and leaves a wall of fire in it for 12 sec",
        "every 20 sec Heroes are branded, burning harder each second and spreading to anyone within 8 yards: "
            "get 8 yards from everyone for 3 sec",
        "every 18 sec embers fall under every Hero and across the arena, each leaving a burning pool",
        "every fallen enemy bolsters the enemies near it: more damage and more health, stacking up to 10 times",
        "every 20 sec an enemy mends every enemy around it for 10 sec: kill it",
        "each wave one enemy wears a Cinder Shield and deals 30% more while it holds; it bursts when it breaks",
        "every 25 sec an Ashen Echo channels into an enemy: kill or stun it, or that enemy rages for 20 sec",
        "every fallen enemy leaves an ember that explodes 3 sec later and burns where it lay",
        "every 25 sec two Heroes are chained, burning every second while apart: get within 12 yards for 3 sec",
        "every 20 sec an enemy hunts a Hero for 10 sec, faster and harder, leaving fire in its wake",
        "every 20 sec the ground cracks: leave the ring before it erupts; it stays frozen and slows for 25 sec",
        "every fallen enemy's allies near it grow 8% stronger for 20 sec, up to 10 times",
        "each wave two enemies are Flamebound: together they deal 30% more and take half damage, keep them apart" };
    constexpr uint32 AFFIX_FIRST = 10, AFFIX_EVERY = 10, AFFIXES_MOST = 4, MAJOR_SPACING_MS = 3000;
    constexpr uint32 WEEK_SECONDS = 7 * 24 * 3600;
    // The affixes are meant to stay with the Heroes, not come and go: most leave something behind (pools, a wall of
    // fire, frozen ground, fire trails) or build up (Bolstering, Vengeful Flame, a Brand that spreads), and what they
    // leave lies on the floor as a decal (Zone), never as smoke to see through.
    // Molten Ground: every Hero (5 at most), 4 sec, 15% within 4.5 yd; a pool for 40 sec, 4% a second (14 at most:
    // the oldest goes).
    constexpr uint32 MOLTEN_EVERY_MS = 14000, MOLTEN_DELAY_MS = 4000, MOLTEN_POOL_MS = 40000, MOLTEN_TARGETS = 5;
    constexpr float MOLTEN_RADIUS = 4.5f, MOLTEN_SHARE = 0.15f, POOL_SHARE = 0.04f;
    constexpr std::size_t POOLS_MOST = 14;
    // Cinder Winds: a lane 14 yd wide across the arena, 4 sec warning, 20% and slowed; then a wall of fire in the
    // lane for 12 sec, 5% a second.
    constexpr uint32 WINDS_EVERY_MS = 22000, WINDS_WARNING_MS = 4000, FIREWALL_MS = 12000;
    constexpr float WINDS_HALF_WIDTH = 7.0f, WINDS_OFFSET = 20.0f, WINDS_SHARE = 0.20f, FIREWALL_SHARE = 0.05f;
    // Searing Brand: a Hero (two of 4 or more) for 12 sec, 3% a second and 1% more each second (8% at most); a Hero
    // within 8 yd of a branded one is branded too; 8 yd from every other Hero (solo: from where it struck) for 3 sec
    // puts it out.
    constexpr uint32 BRAND_EVERY_MS = 20000, BRAND_MS = 12000, BRAND_CLEAR_MS = 3000;
    constexpr float BRAND_SHARE = 0.03f, BRAND_RAMP = 0.01f, BRAND_MOST = 0.08f, BRAND_APART = 8.0f;
    // Emberfall: under every Hero and 5 spots apart, 4 sec, 20% within 5 yd; each leaves a pool.
    constexpr uint32 EMBER_EVERY_MS = 18000, EMBER_DELAY_MS = 4000, EMBER_SPOTS = 5;
    constexpr float EMBER_RADIUS = 5.0f, EMBER_SHARE = 0.20f, EMBER_APART = 10.0f, EMBER_REACH = 40.0f;
    constexpr uint32 SPELL_CINDER_WINDS = 93480, SPELL_SEARING_BRAND = 93481;   // client-patch/patch_runes.py
    // The expanded pool's (their looks the client patch's; their effects the server's, but the speeds).
    constexpr uint32 SPELL_BOLSTERED = 93482, SPELL_MENDING_EMBERS = 93483, SPELL_CINDER_SHIELD = 93484;
    constexpr uint32 SPELL_ASHEN_EMPOWERMENT = 93485, SPELL_BURNING_PURSUIT = 93486, SPELL_VENGEFUL_FLAME = 93487;
    constexpr uint32 SPELL_FLAMEBOUND = 93488, SPELL_SHACKLING_CHAINS = 93489, SPELL_CRACKED_EARTH = 93490;
    constexpr uint32 SPELL_ECHO_CHANNEL = 689;          // its beam only (Drain Life's), set as its channel
    constexpr uint32 NPC_ASHEN_ECHO = 910129;           // titan_firepit.sql
    // Bolstering: each fallen enemy, every other within 25 yd: 12% more damage and health a stack, 10 at most.
    constexpr float BOLSTER_RANGE = 25.0f, BOLSTER_MORE = 0.12f;
    constexpr uint8 BOLSTER_MOST = 10;
    // Mending Embers: 10 sec, every 2 sec every other enemy within 20 yd mended 5%.
    constexpr uint32 MENDING_EVERY_MS = 20000, MENDING_MS = 10000, MENDING_TICK_MS = 2000;
    constexpr float MENDING_RANGE = 20.0f, MENDING_PCT = 5.0f;
    // Cinder Shield: an absorb of 35% of its health, 30% more damage while it holds; broken, a 7 yd burst after 2 sec,
    // 15%.
    constexpr float CINDER_SHIELD_SHARE = 0.35f, CINDER_SHIELD_DAMAGE = 1.3f;
    constexpr float CINDER_BURST_RADIUS = 7.0f, CINDER_BURST_SHARE = 0.15f;
    constexpr uint32 CINDER_BURST_MS = 2000;
    // Ashen Echo: every 25 sec at the edge, an 8 sec channel; done, its enemy empowered 20 sec (25% more damage).
    constexpr uint32 ECHO_EVERY_MS = 25000, ECHO_CHANNEL_MS = 8000, ECHO_BUFF_MS = 20000;
    constexpr float ECHO_DAMAGE = 1.25f;
    // Volatile Remains: an ember 3 sec, 15% within 6 yd, six at most; a pool where it went off.
    constexpr uint32 VOLATILE_DELAY_MS = 3000, VOLATILE_MOST = 6;
    constexpr float VOLATILE_RADIUS = 6.0f, VOLATILE_SHARE = 0.15f;
    // Shackling Chains: 12 sec, two pairs of 4 or more Heroes; over 12 yd apart 4% a second each; 3 sec together
    // breaks it (solo: a point, 6 yd).
    constexpr uint32 CHAINS_EVERY_MS = 25000, CHAINS_MS = 12000, CHAINS_BREAK_MS = 3000;
    constexpr float CHAINS_APART = 12.0f, CHAINS_SOLO = 6.0f, CHAINS_SHARE = 0.04f;
    // Burning Pursuit: 10 sec fixed on a Hero, faster, 50% more damage, a 3 yd pool in its wake every 2 sec.
    constexpr uint32 PURSUIT_EVERY_MS = 20000, PURSUIT_MS = 10000, TRAIL_EVERY_MS = 2000, TRAIL_MS = 12000;
    constexpr float PURSUIT_DAMAGE = 1.5f, TRAIL_RADIUS = 3.0f;
    // Cracked Earth: a 12 yd circle, 5 sec, 20%, then frozen ground slowing 50% on it for 25 sec.
    constexpr uint32 CRACKED_EVERY_MS = 20000, CRACKED_DELAY_MS = 5000, CRACKED_MS = 25000;
    constexpr float CRACKED_RADIUS = 12.0f, CRACKED_SHARE = 0.20f;
    constexpr int32 CRACKED_SLOW = -50;                 // % movement speed on the frozen ground
    // Vengeful Flame: each fallen enemy, every other within 15 yd: 8% a stack, 20 sec, 10 at most.
    constexpr uint32 VENGEFUL_MS = 20000, VENGEFUL_MOST = 10;
    constexpr float VENGEFUL_RANGE = 15.0f, VENGEFUL_DAMAGE = 0.08f;
    // Flamebound: within 10 yd of each other, 30% more damage and half the damage taken (solo: 15% more).
    constexpr float FLAMEBOUND_RANGE = 10.0f, FLAMEBOUND_DAMAGE = 1.3f, FLAMEBOUND_SOLO = 1.15f;
    constexpr float FLAMEBOUND_TAKEN = 0.5f;
    // ---- The major bosses (The Firepit Ignites): Marked for Carnage at waves 45, 60, 75, 90, 100, 105 and every 15
    // after: one boss, a 5 min limit from its entry (warned at 2:00, 1:00, 0:30 and the last 10 sec), at zero the
    // arena kills everyone standing. Waves 45-100 bring the five majors (and they come round again after 105),
    // each a scripted encounter (`MajorTick`).
    enum MajorKind : uint8 { MAJOR_NONE, MAJOR_CAPTAIN, MAJOR_COLOSSUS, MAJOR_MATRON, MAJOR_WARDEN, MAJOR_CHAMPION };
    uint32 const MAJOR_ENTRIES[] = { 0, 910150, 910151, 910152, 910153, 910154 };   // titan_firepit.sql
    constexpr uint32 NPC_WEB_ANCHOR = 910155;
    constexpr uint32 SPELL_CINDERWEB = 93491;           // its slow (client-patch/patch_runes.py)
    constexpr uint32 SPELL_PHYSICAL_LOG = 845;          // the combat log's name for a boss's blow (Cleave)
    constexpr uint32 CARNAGE_MS = 300000, PHASE_MS = 12000, WEBSTORM_MS = 8000, WEAPON_LEAST_MS = 10000;
    constexpr float FINAL_STAND_DAMAGE = 1.2f, EXPOSED_TAKEN = 1.2f, TRIAL_TAKEN = 0.5f;

    // The bosses' words (each one yelled across the arena): the majors' as they enter, at each phase (the Champion's
    // weapons), at their enrage and as they fall.
    struct MajorLines
    {
        char const* Enter[2];
        char const* Phase[3];
        char const* Enrage;
        char const* Fall;
    };
    MajorLines const MAJOR_LINES[] = {
        {},
        { { "Run out the guns! The Ashfleet has come to collect its due!",
            "Ye picked the wrong ring to fight in, landlubbers. Powder's dry and so's my patience!" },
          { "Trial by Fire! Every cannon, on my mark... FIRE!", "Reload and fire again! Sink 'em where they stand!",
            "" },
          "I've sailed through worse storms than you! Stand and fight, if ye dare!",
          "The tide... turns... Remember the Ashfleet..." },
        { { "THE FORGE AWAKENS. THE COLOSSUS ENDURES.",
            "IRON DOES NOT YIELD. FLESH WILL." },
          { "CORE TEMPERATURE CRITICAL. SHATTERING.", "SECOND RUPTURE. THE CORE WILL NOT HOLD YOU BACK.", "" },
          "FORGE UNSTABLE. OVERPRESSURE IMMINENT.",
          "SYSTEMS... FAILING... THE FORGE... GROWS COLD..." },
        { { "Little flies... crawling into my web of cinders. How delicious.",
            "My children are hungry, and you smell of ash and fear." },
          { "Up, up into the dark! Let the webs burn around you!", "The storm of silk and fire returns!", "" },
          "",
          "My web... unravels..." },
        { { "The furnace never sleeps, and neither shall you. Feed the flames!",
            "I am the Warden of this forge. None leave it unburned." },
          { "The cycle turns! Every vent, every fire, AWAKEN!", "Again the furnace roars! Burn, all of you!", "" },
          "Meltdown! Let the whole furnace pour out!",
          "The fires... go out..." },
        { { "Champions of the Firepit! I have bled on this sand for a hundred waves. Show me you are worthy!",
            "The crowd wants blood, and I always give the crowd what it wants!" },
          { "Let's see you stand against the Ironbreaker!", "The Ashen Glaive sings for you!",
            "Enough games. The Executioner's Blade ends this!" },
          "THE FIREPIT ANSWERS! The whole arena burns with me!",
          "You... are the champions now. The crowd... is yours..." },
    };
    // The ladder's bosses (BOSSES, below): each one's own words as it enters and as it falls.
    struct LadderLines
    {
        uint32 Entry;
        char const* Enter;
        char const* Fall;
    };
    LadderLines const LADDER_LINES[] = {
        { 9027, "Gorosh spins for the crowd! Step close and be cut to ribbons!",
          "The spinning... stops..." },
        { 9028, "Grizzle's hide is stone! Your blows are pebbles!",
          "Grizzle... crumbles..." },
        { 9029, "The Eviscerator has carved a hundred fools. You make a hundred and one!",
          "My blades... dulled... by you?" },
        { 9030, "Ok'thor break you! Break bones, break shields, break EVERYTHING!",
          "Ok'thor... is broken..." },
        { 9031, "The shadows hunger, and Anub'shiah feeds them your souls.",
          "The shadows... take me instead..." },
        { 9032, "Hedrum creeps... Hedrum bites... Hedrum feasts.",
          "Hedrum's web... is torn..." },
        { 9025, "The mountain rises against you! Roccor will grind you to dust!",
          "Return me... to the stone..." },
        { 9319, "Sic 'em, boys! Fresh meat for the hounds!",
          "Who's gonna feed... the dogs now...?" },
        { 9016, "BAEL'GAR... RISES... BAEL'GAR... BURNS!",
          "THE FIRE... RETURNS... TO THE DEEP..." },
        { 9033, "Soldiers of the Dark Iron, form up! These intruders die in the Firepit today!",
          "The Dark Iron legions... will avenge me..." },
        { 8983, "My creations are perfected. You will be their final test!",
          "My work... unfinished..." },
        { 9938, "None pass these flames while Magmus stands!",
          "The gate... is yours..." },
        { 9019, "Kneel before the Emperor of the Dark Iron! Your deaths will warm my throne!",
          "Ragnaros... has abandoned... me..." },
    };

    LadderLines const* LadderLinesOf(uint32 entry)
    {
        for (LadderLines const& lines : LADDER_LINES)
            if (lines.Entry == entry)
                return &lines;
        return nullptr;
    }

    bool IsCarnage(uint32 wave)
    {
        return wave == 45 || wave == 60 || wave == 75 || wave == 90 || wave == 100 || wave == 105
            || (wave > 105 && (wave - 105) % 15 == 0);
    }

    uint8 MajorAt(uint32 wave)
    {
        switch (wave)
        {
            case 45: return MAJOR_CAPTAIN;
            case 60: return MAJOR_COLOSSUS;
            case 75: return MAJOR_MATRON;
            case 90: return MAJOR_WARDEN;
            case 100: return MAJOR_CHAMPION;
            default: return wave >= 105 && IsCarnage(wave) ? uint8(MAJOR_CAPTAIN + ((wave - 105) / 15) % 5)
                : MAJOR_NONE;
        }
    }

    // Two that never share the arena: a forced spread and a forced closeness.
    bool Incompatible(uint8 a, uint8 b)
    {
        return (a == AFFIX_SEARING_BRAND && b == AFFIX_SHACKLING_CHAINS)
            || (a == AFFIX_SHACKLING_CHAINS && b == AFFIX_SEARING_BRAND);
    }

    // The week's order of every affix.
    std::vector<uint8> AffixRotation()
    {
        std::vector<uint8> rotation;
        for (uint8 affix = AFFIX_NONE + 1; affix < AFFIX_END; ++affix)
            rotation.push_back(affix);
        std::mt19937 week(uint32(GameTime::GetGameTime().count() / WEEK_SECONDS));
        std::shuffle(rotation.begin(), rotation.end(), week);
        return rotation;
    }

    // The affixes active at a wave: each entry the week's next that is neither active nor at odds with one, the
    // oldest leaving once four are.
    std::vector<uint8> AffixesAt(uint32 wave)
    {
        std::vector<uint8> rotation = AffixRotation();
        uint32 entered = wave >= AFFIX_FIRST ? wave / AFFIX_EVERY : 0;
        std::vector<uint8> active;
        std::size_t next = 0;
        for (uint32 e = 0; e < entered; ++e)
        {
            if (active.size() >= AFFIXES_MOST)
                active.erase(active.begin());
            for (std::size_t tries = 0; tries < rotation.size(); ++tries)
            {
                uint8 affix = rotation[next++ % rotation.size()];
                if (std::find(active.begin(), active.end(), affix) == active.end()
                    && std::none_of(active.begin(), active.end(), [affix](uint8 a) { return Incompatible(a, affix); }))
                {
                    active.push_back(affix);
                    break;
                }
            }
        }
        return active;
    }
    constexpr uint32 ERUPTION_EVERY_MS = 14000, ERUPTION_DELAY_MS = 3000, ERUPTION_TARGETS = 3;
    constexpr float ERUPTION_RADIUS = 6.0f, ERUPTION_DAMAGE = 0.3f;
    constexpr uint32 NOVA_EVERY_MS = 22000, NOVA_DELAY_MS = 4000;
    constexpr float NOVA_RADIUS = 12.0f, NOVA_DAMAGE = 0.5f;
    constexpr float ENRAGE_BELOW = 30.0f, ENRAGE_DAMAGE = 1.5f;
    constexpr uint32 NPC_ERUPTION = 910141;             // titan_firepit.sql: invisible, the eruptions' source
    constexpr uint32 SPELL_FIRE_NOVA = 16079;           // its look only: cast by a friendly source, it hits no one
    constexpr uint32 SPELL_ERUPTION_LOG = 11307;        // the combat log's name for an eruption's hit (Fire Nova)
    // The blows' looks (the client's own, every one a dummy: cast by a friendly source, they hit no one): explosions
    // great and small, a dust blast, a cannon's muzzle flash, and the camera's shake for the Heroes near; the
    // Ashfleet's cannonball (client-patch/patch_runes.py: the Icecrown gunship's cannonball, flying to a point).
    constexpr uint32 SPELL_EXPLOSION_HUGE = 46225, SPELL_EXPLOSION = 60081, SPELL_EXPLOSION_SMALL = 46419;
    constexpr uint32 SPELL_DUST_BLAST = 63360, SPELL_MUZZLE_FLASH = 42468;
    constexpr uint32 SPELL_SHAKE_SMALL = 47533, SPELL_SHAKE_MEDIUM = 44762;
    constexpr uint32 SPELL_CANNONBALL = 93492;
    constexpr float CANNONBALL_SPEED = 32.0f;           // yards a second (the spell's own)
    constexpr uint32 GO_ASHFLEET_CANNON = 910448, GO_POWDER_KEG = 910449;   // titan_firepit.sql
    constexpr uint8 ASHFLEET_CANNONS = 6;
    constexpr uint32 BROADSIDE_EVERY_MS = 9000;
    // His fight never lets up: lit Powder Kegs rolled in near the Heroes every 14 sec (10 in his Final Stand; two
    // among 3 or more Heroes), each blowing after a 12 sec fuse (35% within 10 yd) unless destroyed; one destroyed
    // within 8 yd of him blasts him (5% of his health) and stuns him 3 sec. Ashfleet Deckhands board from his cannons
    // every 25 sec (15 in his Final Stand), two and one more for every two more Heroes (4 at most). Stray cannon fire
    // lands about the arena every 7 sec.
    constexpr uint32 NPC_DECKHAND = 910158, NPC_POWDER_KEG = 910159;     // titan_firepit.sql
    constexpr uint32 KEG_EVERY_MS = 14000, KEG_STAND_EVERY_MS = 10000, KEG_FUSE_MS = 12000, KEG_STUN_MS = 3000;
    constexpr float KEG_RADIUS = 10.0f, KEG_SHARE = 0.35f, KEG_HEALTH = 0.5f, KEG_BOSS_RANGE = 8.0f;
    constexpr float KEG_BOSS_SHARE = 0.05f;
    constexpr uint32 DECKHANDS_EVERY_MS = 25000, DECKHANDS_STAND_EVERY_MS = 15000, STRAY_EVERY_MS = 7000;
    constexpr uint8 DECKHANDS_MOST = 4, STRAY_SHOTS = 2;

    // The Firepit Ignites (its design doc): a wave every 75 sec, or 10 sec after the last enemy dies; 20 sec after a
    // boss falls. A wave's enemies: min(12, 3 + wave / 5) + ceil((Heroes - 1) / 2), 20 at most; every 5th wave one
    // boss alone, the waves' timer stopped while it lives.
    constexpr uint32 COUNTDOWN_MS = 10000, WAVE_MS = 75000, BREATHER_MS = 10000, BOSS_BREATHER_MS = 20000;
    // Before a boss wave: the field cleared, then BOSS_PREP_MS to prepare. A boss wave cleared with a Hero standing:
    // the fallen rise at the pit's edge with half their health and mana, protected for PROTECT_MS (ended early by
    // their own attack).
    constexpr uint32 BOSS_PREP_MS = 20000, PROTECT_MS = 5000;
    constexpr float REVIVE_SHARE = 0.5f;
    constexpr uint32 SPAWN_SPREAD_MS = 4000, TICK_MS = 1000, END_DELAY_MS = 4000, CORPSE_MS = 3000;
    constexpr uint32 WAVE_BASE = 3, WAVE_MOST = 12, WAVE_MOST_ALL = 20;
    // Health: H(n) = 0.70 + 0.05 (n - 1) + 0.0008 (n - 1)^2 of a level-scaled dungeon enemy's (wave 15 1.56, 45 4.45,
    // 90 11.48), 55% more for each Hero past the first; the waves never raise their damage (bosses and Pressure do).
    constexpr float WAVE_HEALTH_BASE = 0.70f, WAVE_HEALTH_LINEAR = 0.05f, WAVE_HEALTH_SQUARE = 0.0008f;
    constexpr float GROUP_HEALTH = 0.55f;
    constexpr uint32 BOSS_EVERY = 5, LOOT_EVERY = 15;
    constexpr float TANK_THREAT = 500.0f, TANK_LEAD = 1.5f;        // tank priority (Prioritize)
    // The field's cap: 8 enemies alive and 4 more for each Hero (the run's own size). Those past it wait in the
    // reinforcements' queue, entering 3 sec after room opens. Pressure: the cap full 30 sec running, the enemies deal
    // 5% more, 5% more every 30 sec after, 20% at most; 10 sec under it clears it all. No one dies of it.
    constexpr uint32 MOST_ALIVE_BASE = 8, MOST_ALIVE_A_HERO = 4, REINFORCE_MS = 3000;
    constexpr uint32 PRESSURE_EVERY_MS = 30000, PRESSURE_RELIEF_MS = 10000, PRESSURE_MOST = 4;
    constexpr float PRESSURE_DAMAGE = 0.05f;
    // A boss (unless the dungeon scaling already counts it one: a boss_ script or a boss's rank, with its own
    // health): waves 5-25 2.0 times the health and 1.1 times the damage of the wave's enemies, 30-40 2.2 and 1.15,
    // 45 on 2.5 and 1.2.
    float BossHealth(uint32 wave)
    {
        return wave >= 45 ? 2.5f : wave >= 30 ? 2.2f : 2.0f;
    }

    float BossDamage(uint32 wave)
    {
        return wave >= 45 ? 1.2f : wave >= 30 ? 1.15f : 1.1f;
    }
    constexpr uint32 SPOILS_SECONDS = 900;
    constexpr float SPOILS_OFFSET = 6.0f;               // the run's chest, this far east of the pit
    constexpr uint8 FIREPIT_LEVEL = 10;                 // the group finder's tab opens there too
    constexpr uint32 PROPOSAL_MS = 40000;               // a group's members this long to accept
    constexpr uint32 LEADERBOARD_ROWS = 5;              // a bracket's, in the group finder's tab
    constexpr uint32 CHECKPOINT_EVERY = 5;              // a run may start at any 5th wave its Heroes have all reached
    constexpr uint8 BRACKETS = 4;                       // records: 1 solo, 2 duo, 3 trio, 4 a group of 4 to 10

    uint8 Bracket(uint32 groupSize)
    {
        return uint8(std::clamp<uint32>(groupSize, 1, BRACKETS));
    }

    // The roster (firepit_roster.h, tools/firepit/firepit_roster.py): a chapter of CHAPTER_WAVES waves per dungeon,
    // in leveling order, each bringing its own enemies.
    using FirepitRoster::CHAPTER_WAVES;
    constexpr std::size_t NEWEST_KINDS = 6;             // a wave's enemies come from the newest few kinds
    // The healers (they keep everything that piles up alive): one a wave at most; alive at once across the waves,
    // one to wave 15, two after. Ranged enemies alive at once: 2 to wave 15, 3 to 30, 4 after (a pick past a cap
    // becomes a fighter of the wave's).
    constexpr uint32 HEALERS_A_WAVE = 1;
    uint32 MostHealers(uint32 wave)
    {
        return wave <= 15 ? 1 : 2;
    }

    uint32 MostRanged(uint32 wave)
    {
        return wave <= 15 ? 2 : wave <= 30 ? 3 : 4;
    }

    // The Enemy Forces tracker's roles: one an enemy, given at its spawn.
    enum ForceRole : uint8 { FORCE_MELEE, FORCE_RANGED, FORCE_HEALER, FORCE_BOSS, FORCE_OTHER, FORCE_ROLES };

    bool IsHealer(uint32 entry);
    uint8 RoleOf(uint32 entry, bool boss)
    {
        if (boss)
            return FORCE_BOSS;
        if (IsHealer(entry))
            return FORCE_HEALER;
        CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(entry);
        if (!info)
            return FORCE_OTHER;
        return info->unit_class == CLASS_MAGE ? FORCE_RANGED : FORCE_MELEE;
    }

    // How the Enemy Forces tracker shows an enemy (its icon: a skull, a shield, a sword, a bow, a wand, a cross): a
    // boss; a healer; a caster (a mana user: its class a paladin's or a mage's); a ranged fighter (a bow, gun,
    // crossbow or thrown weapon in its ranged slot); a tank (a shield in its off hand, or four times the health);
    // else a melee fighter.
    enum ForceLook : uint8 { LOOK_BOSS, LOOK_TANK, LOOK_MELEE, LOOK_RANGED, LOOK_CASTER, LOOK_HEALER, LOOKS };
    constexpr float TANK_HEALTH = 4.0f;

    uint8 LookOf(uint32 entry, bool boss)
    {
        if (boss)
            return LOOK_BOSS;
        if (IsHealer(entry))
            return LOOK_HEALER;
        CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(entry);
        if (!info)
            return LOOK_MELEE;
        if (info->unit_class == CLASS_MAGE || info->unit_class == CLASS_PALADIN)
            return LOOK_CASTER;
        int8 id = 1;
        EquipmentInfo const* gear = sObjectMgr->GetEquipmentInfo(entry, id);
        ItemEntry const* offHand = gear ? sItemStore.LookupEntry(gear->ItemEntry[1]) : nullptr;
        ItemEntry const* ranged = gear ? sItemStore.LookupEntry(gear->ItemEntry[2]) : nullptr;
        if (ranged && ranged->ClassID == ITEM_CLASS_WEAPON && (ranged->SubclassID == ITEM_SUBCLASS_WEAPON_BOW
            || ranged->SubclassID == ITEM_SUBCLASS_WEAPON_GUN || ranged->SubclassID == ITEM_SUBCLASS_WEAPON_CROSSBOW
            || ranged->SubclassID == ITEM_SUBCLASS_WEAPON_THROWN))
            return LOOK_RANGED;
        if ((offHand && offHand->InventoryType == INVTYPE_SHIELD) || info->ModHealth >= TANK_HEALTH)
            return LOOK_TANK;
        return LOOK_MELEE;
    }

    // The Spectators: the stands' seats (Trial of the Champion's own spectators'), a cosmetic crowd dressed as the
    // roster's enemies, cheering as the run goes; never fighting, never counted, never in the way.
    constexpr uint32 NPC_SPECTATOR = 910156;            // titan_firepit.sql
    Position const SEATS[] = {
        { 733.8f, 545.2f, 442.1f, 1.41f }, { 731.0f, 552.7f, 438.8f, 1.34f }, { 734.4f, 560.2f, 435.5f, 1.38f },
        { 726.4f, 557.2f, 437.0f, 1.26f }, { 709.6f, 570.1f, 435.5f, 0.94f }, { 712.9f, 563.2f, 437.0f, 1.03f },
        { 714.3f, 553.7f, 440.2f, 1.12f }, { 700.3f, 559.2f, 442.1f, 0.93f }, { 718.9f, 564.1f, 435.5f, 1.12f },
        { 722.0f, 548.2f, 442.1f, 1.24f }, { 718.4f, 555.9f, 438.8f, 1.15f }, { 725.7f, 560.8f, 435.5f, 1.24f },
        { 697.2f, 583.9f, 435.5f, 0.63f }, { 689.6f, 582.8f, 438.8f, 0.56f }, { 696.3f, 577.5f, 437.0f, 0.70f },
        { 682.9f, 586.2f, 440.2f, 0.47f }, { 688.0f, 573.9f, 442.1f, 0.66f }, { 699.0f, 654.9f, 435.5f, 5.53f },
        { 693.6f, 654.9f, 437.0f, 5.60f }, { 690.5f, 661.7f, 440.2f, 5.55f }, { 684.1f, 656.7f, 442.1f, 5.69f },
        { 686.9f, 650.8f, 438.8f, 5.76f }, { 677.2f, 640.7f, 442.1f, 6.00f }, { 689.4f, 639.3f, 435.5f, 5.95f },
        { 680.0f, 648.9f, 440.2f, 5.86f }, { 686.4f, 643.4f, 437.0f, 5.88f }, { 692.4f, 644.9f, 435.5f, 5.83f },
        { 770.0f, 547.9f, 442.1f, 1.80f }, { 761.2f, 549.1f, 440.2f, 1.59f }, { 764.1f, 553.4f, 438.8f, 1.87f },
        { 675.3f, 590.0f, 442.1f, 0.38f }, { 675.1f, 597.7f, 442.1f, 0.28f }, { 692.9f, 590.6f, 435.5f, 0.47f },
        { 689.6f, 598.0f, 435.5f, 0.33f }, { 685.0f, 591.0f, 438.8f, 0.42f }, { 780.0f, 550.7f, 442.1f, 2.06f },
        { 773.5f, 555.5f, 438.8f, 1.95f }, { 688.0f, 629.6f, 435.5f, 6.20f }, { 688.7f, 604.7f, 435.5f, 0.21f },
        { 678.0f, 634.1f, 440.2f, 5.78f }, { 680.6f, 604.0f, 438.8f, 0.19f }, { 685.1f, 600.4f, 437.0f, 0.28f },
        { 685.1f, 634.4f, 437.0f, 6.09f }, { 806.5f, 574.6f, 442.1f, 2.50f }, { 799.2f, 575.2f, 438.8f, 2.37f },
        { 790.6f, 559.3f, 442.1f, 2.41f }, { 813.5f, 644.9f, 440.3f, 3.49f }, { 818.5f, 640.6f, 442.1f, 3.39f },
        { 813.3f, 650.7f, 442.1f, 3.49f }, { 813.6f, 587.6f, 442.1f, 2.71f }, { 814.3f, 597.8f, 440.3f, 2.90f },
        { 808.9f, 586.0f, 440.3f, 2.64f }, { 818.2f, 607.1f, 440.2f, 2.95f }, { 818.1f, 627.0f, 440.2f, 3.28f },
        { 810.6f, 659.8f, 442.1f, 3.68f }, { 800.2f, 660.7f, 438.8f, 3.82f }, { 805.5f, 648.7f, 436.9f, 3.51f },
        { 696.4f, 653.6f, 435.5f, 5.53f }, { 697.1f, 583.1f, 435.5f, 0.63f }, { 689.2f, 597.0f, 435.5f, 0.35f },
        { 687.8f, 617.6f, 435.5f, 1.59f }, { 687.2f, 618.1f, 435.5f, 0.00f }, { 690.6f, 642.0f, 435.5f, 5.88f },
    };

    bool IsHealer(uint32 entry)
    {
        for (FirepitRoster::Kind const& kind : FirepitRoster::KINDS)
            if (kind.Entry == entry)
                return kind.Healer;
        return false;
    }

    // The bosses, one every 5th wave in turn: the Ring of Law's six, then the Depths' own; then around again.
    uint32 const BOSSES[] = {
        9027,   // Gorosh the Dervish
        9028,   // Grizzle
        9029,   // Eviscerator
        9030,   // Ok'thor the Breaker
        9031,   // Anub'shiah
        9032,   // Hedrum the Creeper
        9025,   // Lord Roccor
        9319,   // Houndmaster Grebmar
        9016,   // Bael'Gar
        9033,   // General Angerforge
        8983,   // Golem Lord Argelmach
        9938,   // Magmus
        9019,   // Emperor Dagran Thaurissan
    };

    // A Firepit enemy's own: its multipliers, its wave, whether it is a boss.
    struct Enemy : DataMap::Base
    {
        float health = 1.0f;
        float damage = 1.0f;
        float pressure = 1.0f;                      // Pressure's 5% a stack
        float affix = 1.0f;                         // the affixes' (Cinder Shield, Vengeful Flame, Flamebound)
        uint32 wave = 0;
        bool boss = false;
        uint8 vengeful = 0;                         // Vengeful Flame's stacks
        uint8 bolster = 0;                          // Bolstering's stacks
        bool shielded = false;                      // its Cinder Shield up last second
        uint32 trailMs = 0;                         // Burning Pursuit: to its next pool
        uint8 role = 0;                             // the waves' mix (FORCE_*: healers and casters kept few)
        uint8 look = LOOK_MELEE;                    // the Enemy Forces tracker's icon (LOOK_*)
        uint32 vengefulUntil = 0;                   // game ms
        ObjectGuid partner;                         // Flamebound's other
    };

    // What the Keeper (or .firepit start) sets up for a run: who goes in, from which wave, and where each came from.
    struct Setup
    {
        std::vector<ObjectGuid> players;
        std::unordered_map<ObjectGuid, WorldLocation> homes;
        uint32 startWave = 1;
    };

    // Shared with the core's hooks (any thread): the runs being set up, who goes where, the Firepit's instances.
    std::mutex registryLock;
    std::unordered_map<uint32, Setup> setups;
    std::unordered_map<ObjectGuid, uint32> destinations;
    std::unordered_set<uint32> firepits;
    std::vector<std::pair<ObjectGuid, uint32>> unbinds;    // players who left a run: their bind to it to drop
    // The group finder tab's requests (from any map's thread), answered on the world's (titan_firepit_world).
    std::vector<std::pair<ObjectGuid, std::string>> requests;

    uint32 Destination(Player* player, uint32 mapId)
    {
        if (mapId != MAP_FIREPIT)
            return 0;
        std::lock_guard guard(registryLock);
        auto destination = destinations.find(player->GetGUID());
        return destination != destinations.end() ? destination->second : 0;
    }

    // No gear wear in the Firepit (deaths and hits alike): the core's hook (InstanceOverride::KeepsDurability).
    bool KeepsDurability(Player* player)
    {
        return player->GetMapId() == MAP_FIREPIT;
    }

    uint32 MaxPlayers(uint32 instanceId)
    {
        std::lock_guard guard(registryLock);
        return firepits.count(instanceId) ? MOST_PLAYERS : 0;
    }

    void SendAddon(Player* player, std::string const& payload)
    {
        WorldPacket data;
        std::string const message = std::string(PREFIX) + '\t' + payload;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, message);
        player->SendDirectMessage(&data);
    }

    // No commas or tabs in a name sent to the addon.
    std::string Clean(std::string text)
    {
        std::replace(text.begin(), text.end(), ',', ' ');
        std::replace(text.begin(), text.end(), '\t', ' ');
        return text;
    }

    float WaveFactor(uint32 wave)
    {
        float n = float(std::max<uint32>(wave, 1) - 1);
        return WAVE_HEALTH_BASE + WAVE_HEALTH_LINEAR * n + WAVE_HEALTH_SQUARE * n * n;
    }

    // One Hero's run.
    struct Record
    {
        std::string name;
        uint8 level = 1;
        uint32 peakWave = 0, peakAlive = 0, mostAlive = 0;
        uint32 kills = 0, bossKills = 0, deaths = 0, deathWave = 0, resurrections = 0, loot = 0;
        std::vector<uint32> lootWaves;              // the waves each piece was earned at
        uint64 damage = 0, healing = 0, taken = 0;
        uint32 biggestHit = 0, biggestTaken = 0;
        std::string killer;
        bool down = false;
        bool left = false;
    };

    struct Spawn
    {
        uint32 entry;
        bool boss;
        uint32 dueMs;
        uint8 role = 0;                             // an affix's chosen (SPAWN_*)
    };
    enum : uint8 { SPAWN_BOLSTERED = 1, SPAWN_CINDER_SHIELD = 2, SPAWN_FLAMEBOUND = 4 };

    // A boss's mechanics' timers, and a blast on its way (a warning on the floor, then the hit).
    struct BossState
    {
        uint32 eruptMs;
        uint32 novaMs;
        bool enraged;
    };
    // A blast's timer waiting to be laid: its object (of TIMER_WIND_UPS_MS's first) and where.
    struct TimerSpot
    {
        uint32 entry;
        Position at;
        float facing, scale;
    };
    struct Blast
    {
        ObjectGuid source;                              // the boss
        ObjectGuid marker;                              // the warning on the floor
        Position at;
        float radius = 0.0f;
        float share = 0.0f;                             // of each Hero's max health
        uint32 ms = 0;
        bool nova = false;                              // at the boss as it goes off
        bool pool = false;                              // Molten Ground: a burning pool where it went off
        uint8 shape = 0;                                // SHAPE_* (0: a circle of radius at `at`)
        float facing = 0.0f, inner = 0.0f, length = 0.0f, halfArc = 0.0f;
        bool physical = false;                          // a Physical blow, else Fire
        bool release = false;                           // its boss let go of its wind-up as it lands
        std::vector<ObjectGuid> markers;                // its decals (deleted as it lands)
        std::vector<TimerSpot> timers;                  // its timers, laid as their wind-up's left reaches theirs
        uint8 timerWindUp = 0;                          // its timers' TIMER_WIND_UPS_MS index
        uint32 total = 0;                               // its warning's ms in all
        uint8 colour = 0;                               // its decals' DecalColour
        int8 style = -1;                                // its decals' DecalStyle (STYLE_AUTO: StyleOf)
        uint8 cannon = 0;                               // the Ashfleet's cannon that fires it (its index + 1)
        bool wind = false;                              // Cinder Winds: slows, and leaves a wall of fire
        ObjectGuid keg;                                 // a Powder Keg's fuse (destroyed: no blast)
        float poolRadius = 0.0f;                        // with pool: the pool's reach (0: the blast's)
        bool launched = false;                          // its cannonball on its way
        bool volatileEmber = false;                     // Volatile Remains' (three at most)
        int32 crack = -1;                               // Cracked Earth: its crack (index in _cracks)
        uint32 log = 0;                                 // the combat log's spell (0: the eruption's)
    };

    struct Crack                                        // Cracked Earth's marked ground, then slowing ground
    {
        Position at;
        std::vector<ObjectGuid> markers;
        uint32 until = 0;                               // run ms (0: not erupted yet)
        bool done = false;
    };

    struct Chain                                        // Shackling Chains
    {
        ObjectGuid a, b;                                // b empty: solo, chained to its point
        Position point;
        std::vector<ObjectGuid> markers;                // solo: its spot's ring
        uint32 left = 0, togetherMs = 0;
    };

    struct Web                                          // the Cinder Matron's web line
    {
        Position center;
        float angle = 0.0f;
        ObjectGuid anchor;
        std::vector<ObjectGuid> markers;
        uint32 activeAt = 0, until = 0;                 // run ms
    };

    struct Major                                        // a major boss's encounter
    {
        ObjectGuid guid;
        uint8 kind = 0;
        uint32 t1 = 0, t2 = 0, t3 = 0, t4 = 0;          // its abilities' timers
        std::vector<uint8> thresholds;                  // health % its phases (or weapons) come at
        std::vector<uint8> queued;                      // crossed, waiting for the running one to end
        uint8 phase = 0, step = 0, enrageAt = 0, weapon = 0, phaseCount = 0;
        uint32 phaseMs = 0, stepMs = 0, exposedUntil = 0, edgeAt = 0;
        bool enraged = false, enragePending = false, side = false;
        float taken = 1.0f;
        std::vector<ObjectGuid> edgeMarkers;
        std::vector<ObjectGuid> props;                  // its dressing (the Ashfleet's cannons and kegs)
        std::vector<Position> cannons;                  // the Ashfleet's muzzles
        std::vector<ObjectGuid> kegs;                   // the Ashfleet's lit Powder Kegs
        uint32 kegMs = 0, deckhandMs = 0, strayMs = 0, stunUntil = 0;
    };

    struct Fx                                           // a look to come (run ms), and the shake of those near
    {
        Position at;
        uint32 spell = 0, due = 0, shake = 0;
        float shakeRange = 0.0f;
    };

    struct Echo                                         // Ashen Echo's channel
    {
        ObjectGuid echo, target;
        uint32 left = 0;
    };

    // A danger's shape: a circle, a cone before its source, a line, a ring, half the arena, or a third of it.
    enum : uint8 { SHAPE_CIRCLE, SHAPE_CONE, SHAPE_LINE, SHAPE_RING, SHAPE_HALF, SHAPE_SECTOR };

    struct Pool                                         // burning ground (Molten Ground, Emberfall, embers, trails)
    {
        Position at;
        std::vector<ObjectGuid> markers;                // its decal
        uint32 until = 0;                               // run ms
        float radius = 0.0f, share = 0.0f;              // its reach, its burn a second
    };

    struct Wind                                         // Cinder Winds' wall of fire, left in its lane
    {
        Position from;                                  // the lane's start, at the arena's edge
        float angle = 0.0f;                             // along the lane
        uint32 until = 0;                               // run ms
        std::vector<ObjectGuid> markers;
    };

    struct Brand                                        // Searing Brand on a Hero
    {
        ObjectGuid hero;
        Position at;                                    // where it struck (solo: the place to leave)
        uint32 left = 0, apartMs = 0, burned = 0;       // burned: its seconds so far (it burns harder each)
    };

    enum class Phase : uint8
    {
        WAITING,        // for the first Hero
        COUNTDOWN,      // the others coming in
        RUNNING,
        ENDING,         // the last one fell
        DONE,
    };
}

// Fireforged gear (firepit_gear.h, tools/scaling/firepit_gear.py): each spoil made for the Hero who earned it. The
// slot: one of their weakest (the lowest item levels worn, an empty slot lowest), among the slots their role uses. The
// piece: the same kind as what is worn there (its armor type, its weapon kind), else their role's. Its item level: what
// is worn there (at least a dungeon piece of their level's; an heirloom counts as one) plus FIREFORGED_MARGIN, or
// FIREFORGED_SHARE of it if more, rounded up to a tier, but never past the cap their level and the run's depth set
// (FIREFORGED_CAP_*): the spoils follow the Hero's level, not an endless climb on their own. Its stats: one of their
// role's stat mixes (a caster never strength, a fighter never intellect). One spoil in TITANFORGED_IN is Titanforged:
// every stat doubled.
namespace
{
    constexpr uint32 FIREFORGED_MARGIN = 9;             // item levels (three tiers)
    constexpr float FIREFORGED_SHARE = 0.12f;
    constexpr uint32 DUNGEON_OVER_LEVEL = 20;           // a dungeon piece's item level over its level (dungeon_loot)
    // The most a spoil may be: the Hero's level + 30 (a 15th wave's), 3 more for each 15 waves past it.
    constexpr uint32 FIREFORGED_CAP_OVER_LEVEL = 30, FIREFORGED_CAP_A_LOOT_WAVE = 3;
    constexpr uint32 WEAKEST_SLOTS = 3;                 // the spoil's slot is one of the Hero's this many weakest
    constexpr uint32 TITANFORGED_IN = 200;
    constexpr uint32 EPIC_FROM = 45;                    // Rare below (The Firepit Ignites)
    constexpr EnchantmentSlot MODIFIER_SLOTS[] = { EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + 3),
        EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + 4) };

    using namespace FirepitGear;

    struct Spoil
    {
        uint32 entry;
        int32 randomPropertyId;                         // the suffix (negative: a random suffix)
        bool titanforged;
        std::vector<uint32> modifiers;                  // enchantments, MODIFIER_SLOTS in order
    };

    enum Primary : uint8 { PRIMARY_STRENGTH, PRIMARY_AGILITY, PRIMARY_INTELLECT };

    Primary PrimaryOf(Player* player)
    {
        float strength = player->GetStat(STAT_STRENGTH), agility = player->GetStat(STAT_AGILITY);
        float intellect = player->GetStat(STAT_INTELLECT);
        if (intellect >= strength && intellect >= agility)
            return PRIMARY_INTELLECT;
        return agility > strength ? PRIMARY_AGILITY : PRIMARY_STRENGTH;
    }

    // The Hero's kind of fighter: their role (the Classless window's), and for the roles both sides play, their
    // strongest stat (an agility tank, a strength or agility fighter, a caster or a hunter at range).
    struct Profile
    {
        Wildcard::Role role;
        Primary primary;
    };

    Profile ProfileOf(Player* player)
    {
        Profile profile{ Wildcard::GetRole(player), PrimaryOf(player) };
        if (profile.role == Wildcard::ROLE_HEALER)
            profile.primary = PRIMARY_INTELLECT;
        else if ((profile.role == Wildcard::ROLE_MELEE || profile.role == Wildcard::ROLE_TANK)
            && profile.primary == PRIMARY_INTELLECT)
            profile.primary = PRIMARY_STRENGTH;
        return profile;
    }

    std::vector<Mix> MixesOf(Profile const& profile)
    {
        switch (profile.role)
        {
            case Wildcard::ROLE_TANK:
                return profile.primary == PRIMARY_AGILITY ? std::vector<Mix>{ MIX_MONKEY }
                    : std::vector<Mix>{ MIX_CHAMPION, MIX_BEAR };
            case Wildcard::ROLE_MELEE:
                return profile.primary == PRIMARY_AGILITY ? std::vector<Mix>{ MIX_MONKEY, MIX_TIGER }
                    : std::vector<Mix>{ MIX_BEAR, MIX_TIGER };
            case Wildcard::ROLE_RANGED:
                return profile.primary == PRIMARY_INTELLECT ? std::vector<Mix>{ MIX_SORCERER }
                    : std::vector<Mix>{ MIX_HUNT };
            case Wildcard::ROLE_HEALER:
                return { MIX_SORCERER, MIX_ELDER };
            default:
                break;
        }
        switch (profile.primary)
        {
            case PRIMARY_AGILITY: return { MIX_MONKEY };
            case PRIMARY_INTELLECT: return { MIX_SORCERER };
            default: return { MIX_BEAR };
        }
    }

    uint8 ArmorTypeOf(Profile const& profile)
    {
        switch (profile.role)
        {
            case Wildcard::ROLE_HEALER:
                return ITEM_SUBCLASS_ARMOR_CLOTH;
            case Wildcard::ROLE_RANGED:
                return profile.primary == PRIMARY_INTELLECT ? ITEM_SUBCLASS_ARMOR_CLOTH : ITEM_SUBCLASS_ARMOR_MAIL;
            default:
                break;
        }
        switch (profile.primary)
        {
            case PRIMARY_AGILITY: return ITEM_SUBCLASS_ARMOR_LEATHER;
            case PRIMARY_INTELLECT: return ITEM_SUBCLASS_ARMOR_CLOTH;
            default: return ITEM_SUBCLASS_ARMOR_PLATE;
        }
    }

    int32 FindPiece(uint8 itemClass, uint8 subClass, std::initializer_list<uint8> inventoryTypes, uint8 armorType = 0)
    {
        for (uint32 i = 0; i < std::size(PIECES); ++i)
        {
            Piece const& piece = PIECES[i];
            if (piece.Class != itemClass || (itemClass == ITEM_CLASS_ARMOR && piece.ArmorType != armorType)
                || (itemClass == ITEM_CLASS_WEAPON && piece.SubClass != subClass))
                continue;
            if (itemClass == ITEM_CLASS_ARMOR && !armorType && piece.SubClass != subClass)
                continue;
            for (uint8 type : inventoryTypes)
                if (piece.InventoryType == type)
                    return int32(i);
        }
        return -1;
    }

    int32 WeaponPiece(uint8 subClass, bool offHand)
    {
        if (subClass == ITEM_SUBCLASS_WEAPON_CROSSBOW)
            subClass = ITEM_SUBCLASS_WEAPON_BOW;
        if (offHand)
            return FindPiece(ITEM_CLASS_WEAPON, subClass, { INVTYPE_WEAPON, INVTYPE_WEAPONOFFHAND });
        return FindPiece(ITEM_CLASS_WEAPON, subClass, { INVTYPE_WEAPON, INVTYPE_WEAPONMAINHAND, INVTYPE_2HWEAPON,
            INVTYPE_RANGED, INVTYPE_RANGEDRIGHT });
    }

    // The piece for an equipment slot, matching what is worn there, else the Hero's kind's; -1 if the slot is not
    // theirs (an off hand behind a two-hander, a ranged slot a fighter leaves empty).
    int32 PieceFor(Player* player, uint8 slot, Profile const& profile)
    {
        Item* worn = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        ItemTemplate const* proto = worn ? worn->GetTemplate() : nullptr;
        bool intellect = profile.primary == PRIMARY_INTELLECT;
        switch (slot)
        {
            case EQUIPMENT_SLOT_NECK: return FindPiece(ITEM_CLASS_ARMOR, 0, { INVTYPE_NECK });
            case EQUIPMENT_SLOT_FINGER1:
            case EQUIPMENT_SLOT_FINGER2: return FindPiece(ITEM_CLASS_ARMOR, 0, { INVTYPE_FINGER });
            case EQUIPMENT_SLOT_TRINKET1:
            case EQUIPMENT_SLOT_TRINKET2: return FindPiece(ITEM_CLASS_ARMOR, 0, { INVTYPE_TRINKET });
            case EQUIPMENT_SLOT_BACK: return FindPiece(ITEM_CLASS_ARMOR, ITEM_SUBCLASS_ARMOR_CLOTH, { INVTYPE_CLOAK });
            case EQUIPMENT_SLOT_MAINHAND:
            {
                if (proto && proto->Class == ITEM_CLASS_WEAPON)
                    if (int32 piece = WeaponPiece(uint8(proto->SubClass), false); piece >= 0)
                        return piece;
                switch (profile.role)
                {
                    case Wildcard::ROLE_TANK: return WeaponPiece(ITEM_SUBCLASS_WEAPON_SWORD, false);
                    case Wildcard::ROLE_HEALER: return WeaponPiece(ITEM_SUBCLASS_WEAPON_MACE, false);
                    case Wildcard::ROLE_RANGED:
                        return WeaponPiece(intellect ? ITEM_SUBCLASS_WEAPON_STAFF : ITEM_SUBCLASS_WEAPON_POLEARM,
                            false);
                    default:
                        break;
                }
                if (intellect)
                    return WeaponPiece(ITEM_SUBCLASS_WEAPON_STAFF, false);
                return WeaponPiece(profile.primary == PRIMARY_AGILITY ? ITEM_SUBCLASS_WEAPON_DAGGER
                    : ITEM_SUBCLASS_WEAPON_SWORD2, false);
            }
            case EQUIPMENT_SLOT_OFFHAND:
            {
                Item* main = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
                if (main && main->GetTemplate()->InventoryType == INVTYPE_2HWEAPON)
                    return -1;
                if (proto && proto->InventoryType == INVTYPE_SHIELD)
                    return FindPiece(ITEM_CLASS_ARMOR, ITEM_SUBCLASS_ARMOR_SHIELD, { INVTYPE_SHIELD });
                if (proto && proto->InventoryType == INVTYPE_HOLDABLE)
                    return FindPiece(ITEM_CLASS_ARMOR, ITEM_SUBCLASS_ARMOR_MISC, { INVTYPE_HOLDABLE });
                if (proto && proto->Class == ITEM_CLASS_WEAPON)
                {
                    int32 piece = WeaponPiece(uint8(proto->SubClass), true);
                    return piece >= 0 ? piece : WeaponPiece(ITEM_SUBCLASS_WEAPON_DAGGER, true);
                }
                if (profile.role == Wildcard::ROLE_TANK)
                    return FindPiece(ITEM_CLASS_ARMOR, ITEM_SUBCLASS_ARMOR_SHIELD, { INVTYPE_SHIELD });
                if (intellect)
                    return FindPiece(ITEM_CLASS_ARMOR, ITEM_SUBCLASS_ARMOR_MISC, { INVTYPE_HOLDABLE });
                return -1;
            }
            case EQUIPMENT_SLOT_RANGED:
            {
                if (proto && proto->Class == ITEM_CLASS_WEAPON)
                    if (int32 piece = WeaponPiece(uint8(proto->SubClass), false); piece >= 0)
                        return piece;
                if (profile.role == Wildcard::ROLE_RANGED && !intellect)
                    return WeaponPiece(ITEM_SUBCLASS_WEAPON_BOW, false);
                if (intellect)
                    return WeaponPiece(ITEM_SUBCLASS_WEAPON_WAND, false);
                return -1;
            }
            default:
                break;
        }
        uint8 inventoryType = 0;
        switch (slot)
        {
            case EQUIPMENT_SLOT_HEAD: inventoryType = INVTYPE_HEAD; break;
            case EQUIPMENT_SLOT_SHOULDERS: inventoryType = INVTYPE_SHOULDERS; break;
            case EQUIPMENT_SLOT_CHEST: inventoryType = INVTYPE_CHEST; break;
            case EQUIPMENT_SLOT_WAIST: inventoryType = INVTYPE_WAIST; break;
            case EQUIPMENT_SLOT_LEGS: inventoryType = INVTYPE_LEGS; break;
            case EQUIPMENT_SLOT_FEET: inventoryType = INVTYPE_FEET; break;
            case EQUIPMENT_SLOT_WRISTS: inventoryType = INVTYPE_WRISTS; break;
            case EQUIPMENT_SLOT_HANDS: inventoryType = INVTYPE_HANDS; break;
            default: return -1;
        }
        uint8 armorType = proto && proto->Class == ITEM_CLASS_ARMOR && proto->SubClass >= ITEM_SUBCLASS_ARMOR_CLOTH
            && proto->SubClass <= ITEM_SUBCLASS_ARMOR_PLATE ? uint8(proto->SubClass) : ArmorTypeOf(profile);
        if (inventoryType == INVTYPE_CHEST)
            return FindPiece(ITEM_CLASS_ARMOR, armorType, { INVTYPE_CHEST, INVTYPE_ROBE }, armorType);
        return FindPiece(ITEM_CLASS_ARMOR, armorType, { inventoryType }, armorType);
    }

    // An Epic piece's one bonus effect (The Firepit Ignites): from the Hero's loot pool (their role's) and the
    // Hybrid pool, only those their build can use; its enchantment for the piece's tier (its words its numbers).
    uint32 RollEffect(Player* player, Profile const& profile, uint32 tier)
    {
        using namespace FireforgedEffects;
        uint8 pool = POOL_HYBRID;
        switch (profile.role)
        {
            case Wildcard::ROLE_TANK: pool |= POOL_TANK; break;
            case Wildcard::ROLE_HEALER: pool |= POOL_HEALER; break;
            case Wildcard::ROLE_RANGED: pool |= POOL_RANGED; break;
            default: pool |= profile.primary == PRIMARY_INTELLECT ? POOL_RANGED : POOL_MELEE; break;
        }
        bool caster = profile.primary == PRIMARY_INTELLECT || profile.role == Wildcard::ROLE_HEALER;
        Item* ranged = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
        uint16 has = 0;
        if (player->CanBlock())
            has |= NEED_BLOCK;
        if (player->CanParry())
            has |= NEED_PARRY;
        if (player->GetShield())
            has |= NEED_SHIELD;
        if (player->GetMaxPower(POWER_MANA) > 0)
            has |= NEED_MANA;
        if (player->GetMaxPower(POWER_ENERGY) > 0)
            has |= NEED_ENERGY;
        if (profile.role == Wildcard::ROLE_HEALER)
            has |= NEED_HEAL;
        if (caster)
            has |= NEED_SPELL | NEED_CASTER_PICK;
        else
            has |= NEED_PHYSICAL | NEED_PHYSICAL_PICK;
        if (ranged && ranged->GetTemplate()->SubClass != ITEM_SUBCLASS_WEAPON_WAND)
            has |= NEED_RANGED;
        if (profile.role == Wildcard::ROLE_TANK)
            has |= NEED_TANK;
        std::vector<uint32> eligible;
        for (uint32 i = 0; i < std::size(EFFECTS); ++i)
            if ((EFFECTS[i].Pool & pool) && (EFFECTS[i].Needs & has) == EFFECTS[i].Needs)
                eligible.push_back(i);
        if (eligible.empty())
            return 0;
        return ENCHANT_FIRST + eligible[urand(0, uint32(eligible.size()) - 1)] * TIERS + tier;
    }

    // A Hero's spoils: a piece for each wave earned at, each in another of their weakest slots.
    std::vector<Spoil> Fireforged(Player* player, std::vector<uint32> const& waves)
    {
        std::size_t const count = waves.size();
        Profile const profile = ProfileOf(player);
        std::vector<Mix> const mixes = MixesOf(profile);
        struct Candidate
        {
            uint8 slot;
            int32 piece;
            uint32 worn;                                // the item level worn there (0: nothing)
        };
        std::vector<Candidate> candidates;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_TABARD; ++slot)
        {
            if (slot == EQUIPMENT_SLOT_BODY)
                continue;
            int32 piece = PieceFor(player, slot, profile);
            if (piece < 0)
                continue;
            // An heirloom (it grows with its wearer) counts as a dungeon piece of their level.
            Item* worn = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            uint32 level = !worn ? 0u : worn->GetTemplate()->Quality == ITEM_QUALITY_HEIRLOOM
                ? uint32(player->GetLevel() + DUNGEON_OVER_LEVEL) : worn->GetTemplate()->ItemLevel;
            candidates.push_back({ slot, piece, level });
        }
        std::vector<Spoil> spoils;
        uint32 const baseline = player->GetLevel() + DUNGEON_OVER_LEVEL;
        while (spoils.size() < count && !candidates.empty())
        {
            uint32 const wave = waves[spoils.size()];
            uint32 const cap = player->GetLevel() + FIREFORGED_CAP_OVER_LEVEL
                + FIREFORGED_CAP_A_LOOT_WAVE * (wave / LOOT_EVERY > 1 ? wave / LOOT_EVERY - 1 : 0);
            // The weakest slots first, and among them those still under the cap (the rest cannot get better).
            Acore::Containers::RandomShuffle(candidates);
            std::stable_sort(candidates.begin(), candidates.end(), [cap](Candidate const& a, Candidate const& b)
            {
                bool aRoom = a.worn < cap, bRoom = b.worn < cap;
                return aRoom != bRoom ? aRoom : a.worn < b.worn;
            });
            std::size_t pick = urand(0, uint32(std::min<std::size_t>(WEAKEST_SLOTS, candidates.size())) - 1);
            Candidate const chosen = candidates[pick];
            candidates.erase(candidates.begin() + pick);

            // Above what is worn there, by the Hero's level: at least a dungeon piece of theirs and the margin, at
            // most the cap (FIREFORGED_CAP_*: deeper runs raise it).
            uint32 from = std::max(chosen.worn, baseline);
            uint32 target = from + std::max<uint32>(FIREFORGED_MARGIN, uint32(std::lround(from * FIREFORGED_SHARE)));
            target = std::min(target, cap);
            bool titan = urand(1, TITANFORGED_IN) == 1;
            Mix mix = mixes[urand(0, uint32(mixes.size()) - 1)];
            uint32 tier = TierAtLeast(target);
            if (tier && FIRST_ITEM_LEVEL + tier * ITEM_LEVEL_STEP > cap)
                --tier;                                 // the tier at or under the cap
            // Rare at waves 15 and 30, Epic from 45 (with one bonus effect; Titanforged: a socket too).
            bool epic = wave >= EPIC_FROM;
            titan = titan && epic;
            uint32 entry = PIECES[chosen.piece].First + tier + (titan ? TITAN_OFFSET : epic ? 0 : RARE_OFFSET);
            if (!sObjectMgr->GetItemTemplate(entry))
                continue;
            std::vector<uint32> effect;
            if (epic)
                if (uint32 enchant = RollEffect(player, profile, tier))
                    effect.push_back(enchant);
            spoils.push_back({ entry, -int32(SUFFIX[mix]), titan, effect });
        }
        return spoils;
    }
}

namespace
{
class FirepitInstance : public InstanceScript
{
public:
    // The run's setup is taken as the script is made: the Firepit's instance has a save (StartRun makes it), so the
    // core loads it rather than calling Initialize (InstanceMap::CreateInstanceScript, for a module's script).
    explicit FirepitInstance(InstanceMap* map) : InstanceScript(map) { TakeSetup(); }

    void Initialize() override { TakeSetup(); }

    void TakeSetup()
    {
        std::lock_guard guard(registryLock);
        auto setup = setups.find(instance->GetInstanceId());
        if (setup != setups.end())
        {
            _setup = setup->second;
            setups.erase(setup);
        }
        _groupSize = std::max<uint32>(1, uint32(_setup.players.size()));
    }

    // The Depths' own creatures go as they load (they would come back in a week, after the instance is long gone).
    void OnCreatureCreate(Creature* creature) override
    {
        if (creature->GetSpawnId())
            _natives.push_back(creature->GetGUID());
    }

    void OnPlayerEnter(Player* player) override
    {
        Record& record = _records[player->GetGUID()];
        record.name = player->GetName();
        record.level = player->GetLevel();
        record.left = false;
        if (_phase == Phase::WAITING)
        {
            Decorate();
            _phase = Phase::COUNTDOWN;
            _timer = COUNTDOWN_MS;
            Announce("The Firepit roars to life. The first wave comes in 10 seconds.");
        }
        SendAddon(player, "INTRO");
        SendHud(player);
        SendAffixes(player);
    }

    void OnPlayerLeave(Player* player) override
    {
        auto record = _records.find(player->GetGUID());
        if (record != _records.end())
        {
            record->second.left = true;
            if (_phase == Phase::RUNNING && !record->second.down)
                FellNow(record->second, "left the Firepit");
        }
        std::lock_guard guard(registryLock);
        destinations.erase(player->GetGUID());
        unbinds.emplace_back(player->GetGUID(), instance->GetInstanceId());
    }

    void OnUnitDeath(Unit* unit) override
    {
        if (_phase != Phase::RUNNING)
            return;
        // A ladder boss's last words (a major's are EndMajor's).
        if (Creature* creature = unit->ToCreature())
            if (Enemy const* own = creature->CustomData.Get<Enemy>("titan_firepit"); own && own->boss
                && !MajorAt(own->wave))
                if (LadderLines const* lines = LadderLinesOf(creature->GetEntry()))
                    creature->Yell(lines->Fall, LANG_UNIVERSAL);
        if (Player* player = unit->ToPlayer())
        {
            auto record = _records.find(player->GetGUID());
            if (record != _records.end() && !record->second.down)
                FellNow(record->second, "");
        }
    }

    void Update(uint32 diff) override
    {
        for (ObjectGuid const& guid : _natives)
            if (Creature* native = instance->GetCreature(guid))
                native->DespawnOrUnsummon(0ms, 7 * 24h);
        _natives.clear();

        switch (_phase)
        {
            case Phase::COUNTDOWN:
                if (_timer <= diff)
                {
                    _phase = Phase::RUNNING;
                    StartWave(_setup.startWave);
                }
                else
                    _timer -= diff;
                break;
            case Phase::RUNNING:
                Run(diff);
                break;
            case Phase::ENDING:
                if (_timer <= diff)
                    Finish();
                else
                    _timer -= diff;
                break;
            default:
                break;
        }
    }

    // --- What the hooks report ----------------------------------------------------------------------------------

    bool Running() const { return _phase == Phase::RUNNING; }
    bool Over() const { return _phase == Phase::DONE; }

    Record* RecordOf(Player* player)
    {
        auto record = _records.find(player->GetGUID());
        return record != _records.end() ? &record->second : nullptr;
    }

    void Damage(Player* attacker, uint32 damage)
    {
        if (Record* record = Running() ? RecordOf(attacker) : nullptr)
        {
            record->damage += damage;
            record->biggestHit = std::max(record->biggestHit, damage);
        }
    }

    void Taken(Player* victim, uint32 damage)
    {
        if (Record* record = Running() ? RecordOf(victim) : nullptr)
        {
            record->taken += damage;
            record->biggestTaken = std::max(record->biggestTaken, damage);
        }
    }

    void Healed(Player* healer, uint32 gain)
    {
        if (Record* record = Running() ? RecordOf(healer) : nullptr)
            record->healing += gain;
    }

    void Killed(Player* killer, Creature* enemy)
    {
        if (!Running())
            return;
        ++_kills;
        if (Record* record = RecordOf(killer))
        {
            ++record->kills;
            Enemy const* own = enemy->CustomData.Get<Enemy>("titan_firepit");
            if (own && own->boss)
                ++record->bossKills;
        }
    }

    void KilledBy(Player* player, Creature* killer)
    {
        if (Record* record = Running() ? RecordOf(player) : nullptr)
            record->killer = Clean(killer->GetName());
    }

    // A Hero leaving by the eye (Teleport out): fallen if the run goes on, and brought back to walk out (their
    // stand-up not counted as a resurrection).
    void Leaving(Player* player)
    {
        if (Record* record = RecordOf(player))
        {
            if (Running() && !record->down)
                FellNow(*record, "left the Firepit");
            record->left = true;
        }
        if (!player->IsAlive())
        {
            player->ResurrectPlayer(1.0f);
            player->SpawnCorpseBones();
        }
        player->CombatStop(true);
    }

    void Resurrected(Player* player)
    {
        if (Record* record = Running() ? RecordOf(player) : nullptr)
            if (record->down && !record->left)
            {
                record->down = false;
                ++record->resurrections;
            }
    }

    // .firepit next: the timer out now.
    void SkipWave()
    {
        if (_phase == Phase::RUNNING)
            _waveMs = 0;
    }

    // .firepit stop: the run over now, as if the last one fell.
    void Stop()
    {
        if (_phase == Phase::RUNNING || _phase == Phase::COUNTDOWN)
        {
            _phase = Phase::ENDING;
            ClearMechanics();
            _timer = 0;
        }
    }

    uint32 Wave() const { return _wave; }
    uint32 Alive() const { return uint32(_enemies.size()); }

    // The run's chest: the Hero's own spoils from it (empty: none, or already taken); the chest goes once all
    // are taken (gone: true).
    std::vector<Spoil> TakeSpoils(GameObject* chest, Player* player, bool& gone)
    {
        gone = false;
        if (chest->GetGUID() != _chest)
            return {};
        auto spoils = _spoils.find(player->GetGUID());
        if (spoils == _spoils.end())
            return {};
        std::vector<Spoil> items = std::move(spoils->second);
        _spoils.erase(spoils);
        gone = _spoils.empty();
        return items;
    }

    // Where the Hero came from (the Keeper's side), else nowhere.
    bool Home(Player* player, WorldLocation& home) const
    {
        auto found = _setup.homes.find(player->GetGUID());
        if (found == _setup.homes.end())
            return false;
        home = found->second;
        return true;
    }

private:
    // The run's news, on each Hero's wave tracker (Emberforge_Firepit): urgent, in red with a raid warning's sound.
    // The ring dressed (once a run): each piece on the floor where it stands.
    // The arena's floor under (x, y), not the stands, walls or fences above it (the floor is flat: anything found
    // much above or below the centre's is not it).
    float FloorAt(float x, float y) const
    {
        float z = instance->GetHeight(x, y, CENTER.GetPositionZ() + 1.5f);
        if (z < CENTER.GetPositionZ() - 3.0f || z > CENTER.GetPositionZ() + 1.0f)
            z = CENTER.GetPositionZ();
        return z;
    }

    // The crowd in the stands: dressed as the roster's enemies (those of the run's dungeons so far and the next,
    // some of any), a little varied in size.
    void SeatCrowd()
    {
        if (!_crowd.empty())
            return;
        std::size_t known = 0;
        for (FirepitRoster::Kind const& kind : FirepitRoster::KINDS)
            if (kind.Wave <= std::max<uint32>(_setup.startWave, 1) + CHAPTER_WAVES * 2)
                ++known;
        known = std::max<std::size_t>(known, 4);
        for (Position const& seat : SEATS)
        {
            uint32 entry = FirepitRoster::KINDS[roll_chance_i(75) ? urand(0, uint32(std::min(known,
                std::size(FirepitRoster::KINDS))) - 1) : urand(0, uint32(std::size(FirepitRoster::KINDS)) - 1)].Entry;
            CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(entry);
            CreatureModel const* model = info ? ObjectMgr::ChooseDisplayId(info) : nullptr;
            TempSummon* fan = instance->SummonCreature(NPC_SPECTATOR, seat);
            if (!fan)
                continue;
            if (model)
                fan->SetDisplayId(model->CreatureDisplayID);
            fan->SetObjectScale(frand(0.9f, 1.15f));
            fan->SetReactState(REACT_PASSIVE);
            _crowd.push_back(fan->GetGUID());
        }
    }

    // The crowd reacts (share: how many, of 100): a cheer, a roar or applause; the losing side's mock.
    void Cheer(uint32 share, bool mock = false)
    {
        static Emote const CHEERS[] = { EMOTE_ONESHOT_CHEER, EMOTE_ONESHOT_ROAR, EMOTE_ONESHOT_APPLAUD };
        for (ObjectGuid const& guid : _crowd)
            if (Creature* fan = instance->GetCreature(guid); fan && roll_chance_i(int32(share)))
                fan->HandleEmoteCommand(mock ? EMOTE_ONESHOT_LAUGH : CHEERS[urand(0, 2)]);
    }

    void Decorate()
    {
        SeatCrowd();
        auto place = [this](uint32 entry, float x, float y, float facing)
        {
            instance->SummonGameObject(entry, x, y, FloorAt(x, y), facing, 0.0f, 0.0f, std::sin(facing / 2.0f),
                std::cos(facing / 2.0f), 0);
        };
        for (Ring const& ring : DECOR_RINGS)
            for (uint32 i = 0; i < ring.count; ++i)
            {
                float angle = ring.turn + 2.0f * float(M_PI) * float(i) / float(ring.count);
                place(ring.entry, CENTER.GetPositionX() + ring.radius * std::cos(angle),
                    CENTER.GetPositionY() + ring.radius * std::sin(angle), angle + float(M_PI));
            }
        for (Position const& gate : GATES)
        {
            float toward = std::atan2(CENTER.GetPositionY() - gate.GetPositionY(),
                CENTER.GetPositionX() - gate.GetPositionX());
            for (float side : { -1.0f, 1.0f })
            {
                float across = toward + side * float(M_PI) / 2.0f;
                place(GO_LAVA_PILLAR, gate.GetPositionX() + GATE_PILLAR_SPREAD * std::cos(across),
                    gate.GetPositionY() + GATE_PILLAR_SPREAD * std::sin(across), toward);
            }
        }
    }

    void Announce(std::string const& text, bool urgent = false)
    {
        for (auto const& ref : instance->GetPlayers())
            if (Player* player = ref.GetSource())
                SendAddon(player, "A," + Clean(text) + ',' + (urgent ? "1" : "0"));
    }

    // A boss wave cleared with a Hero standing: every fallen Hero rises at the pit's edge, half their health and
    // mana, protected for 5 sec.
    void Recover()
    {
        if (Living().empty())
            return;
        uint32 n = 0;
        for (auto const& ref : instance->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player || player->IsAlive() || player->IsGameMaster())
                continue;
            player->ResurrectPlayer(REVIVE_SHARE);
            player->SpawnCorpseBones();
            player->SetHealth(std::max<uint32>(1, uint32(player->GetMaxHealth() * REVIVE_SHARE)));
            if (player->getPowerType() == POWER_MANA)
                player->SetPower(POWER_MANA, uint32(player->GetMaxPower(POWER_MANA) * REVIVE_SHARE));
            float angle = float(n++) * 0.7f;
            player->NearTeleportTo(CENTER.GetPositionX() + ARRIVAL_RADIUS * std::cos(angle),
                CENTER.GetPositionY() + ARRIVAL_RADIUS * std::sin(angle), CENTER.GetPositionZ(), angle + float(M_PI));
            _protected[player->GetGUID()] = _runMs + PROTECT_MS;
            if (Record* record = RecordOf(player))
                record->down = false;
            ChatHandler(player->GetSession()).SendSysMessage("|cffff7f27The Firepit:|r you rise again, protected "
                "for 5 sec (attacking ends it).");
        }
        if (n)
            Announce("The boss wave is cleared: the fallen rise again!", true);
    }

public:
    // A revived Hero's protection: on (until its time, or their own attack).
    bool IsProtected(Player* player)
    {
        auto itr = _protected.find(player->GetGUID());
        if (itr == _protected.end())
            return false;
        if (_runMs < itr->second)
            return true;
        _protected.erase(itr);
        return false;
    }

    void Unprotect(Player* player)
    {
        _protected.erase(player->GetGUID());
    }

private:
    std::vector<Player*> Living() const
    {
        std::vector<Player*> living;
        for (auto const& ref : instance->GetPlayers())
            if (Player* player = ref.GetSource())
                if (player->IsAlive() && !player->IsGameMaster())
                    living.push_back(player);
        return living;
    }

    void FellNow(Record& record, std::string const& how)
    {
        record.down = true;
        ++record.deaths;
        record.deathWave = _wave;
        if (!how.empty())
            record.killer = how;
    }

    void StartWave(uint32 wave)
    {
        _wave = wave;
        // A new wave: every enemy already on the field turns to the tank (tank priority).
        if (Player* tank = Tank())
            for (ObjectGuid const& guid : _enemies)
                if (Creature* enemy = instance->GetCreature(guid); enemy && enemy->IsAlive() && enemy->AI())
                {
                    Prioritize(enemy, tank);
                    enemy->AI()->AttackStart(tank);
                }
        _waveMs = WAVE_MS;
        _breather = false;
        UpdateAffixes();
        for (Player* player : Living())
            if (Record* record = RecordOf(player))
            {
                record->peakWave = _wave;
                record->peakAlive = uint32(_enemies.size());
            }

        bool bossWave = _wave % BOSS_EVERY == 0;
        uint32 count = bossWave ? 0 : std::min(WAVE_MOST_ALL, std::min(WAVE_MOST, WAVE_BASE + _wave / 5)
            + _groupSize / 2);                      // ceil((Heroes - 1) / 2); a boss comes alone

        std::vector<uint32> kinds;
        for (FirepitRoster::Kind const& kind : FirepitRoster::KINDS)
            if (kind.Wave <= _wave)
                kinds.push_back(kind.Entry);
        if (kinds.size() > NEWEST_KINDS)
            kinds.erase(kinds.begin(), kinds.end() - NEWEST_KINDS);

        std::vector<uint32> fighters;
        for (uint32 entry : kinds)
            if (RoleOf(entry, false) == FORCE_MELEE)
                fighters.push_back(entry);
        uint32 healers = 0, ranged = 0;
        for (ObjectGuid const& guid : _enemies)
            if (Creature* enemy = instance->GetCreature(guid); enemy && enemy->IsAlive())
                if (Enemy* own = EnemyOf(enemy))
                {
                    healers += own->role == FORCE_HEALER ? 1 : 0;
                    ranged += own->role == FORCE_RANGED ? 1 : 0;
                }
        for (Spawn const& queued : _reinforcements)
        {
            uint8 role = RoleOf(queued.entry, queued.boss);
            healers += role == FORCE_HEALER ? 1 : 0;
            ranged += role == FORCE_RANGED ? 1 : 0;
        }
        uint32 healersNow = 0, rangedNow = 0;
        std::size_t firstOfWave = _queue.size();
        for (uint32 i = 0; i < count && !kinds.empty(); ++i)
        {
            uint32 entry = i == 0 ? kinds.back() : kinds[urand(0, uint32(kinds.size()) - 1)];
            uint8 role = RoleOf(entry, false);
            if ((role == FORCE_HEALER && (healersNow >= HEALERS_A_WAVE || healers + healersNow >= MostHealers(_wave)))
                || (role == FORCE_RANGED && ranged + rangedNow >= MostRanged(_wave)))
            {
                if (fighters.empty())
                    continue;
                entry = fighters[urand(0, uint32(fighters.size()) - 1)];
                role = FORCE_MELEE;
            }
            healersNow += role == FORCE_HEALER ? 1 : 0;
            rangedNow += role == FORCE_RANGED ? 1 : 0;
            _queue.push_back({ entry, false, SPAWN_SPREAD_MS * i / std::max<uint32>(1, count) });
        }

        if (!bossWave)
            AssignRoles(firstOfWave);
        std::string text = "Wave " + std::to_string(_wave);
        // A new chapter (its first wave, never a boss's): its dungeon's enemies pour in.
        uint32 chapter = (_wave - 1) / CHAPTER_WAVES;
        if ((_wave - 1) % CHAPTER_WAVES == 0 && chapter < std::size(FirepitRoster::CHAPTERS))
            text += ": the foes of " + std::string(FirepitRoster::CHAPTERS[chapter]) + " pour in!";
        if (bossWave)
        {
            uint32 entry = BossAt(_wave);
            _queue.push_back({ entry, true, 0 });
            if (CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(entry))
                text += ": " + info->Name;
            text += " enters the Ring!";
        }
        Announce(text, bossWave);
        Cheer(bossWave ? 100 : 30);
    }

    void SpawnOne(Spawn const& spawn, Position const* place = nullptr)
    {
        // Too many alive: this one waits with the reinforcements, coming 3 sec after room opens.
        if (_enemies.size() >= MostAlive())
        {
            _reinforcements.push_back(spawn);
            return;
        }
        float angle = frand(0.0f, 2.0f * float(M_PI));
        float radius = frand(SPAWN_RADIUS_MIN, SPAWN_RADIUS_MAX);
        float x = CENTER.GetPositionX() + radius * std::cos(angle);
        float y = CENTER.GetPositionY() + radius * std::sin(angle);
        Position at = place ? *place : Position(x, y, FloorAt(x, y), angle + float(M_PI));
        TempSummon* enemy = instance->SummonCreature(spawn.entry, at);
        if (!enemy)
            return;
        Enemy* own = enemy->CustomData.GetDefault<Enemy>("titan_firepit");
        float factor = WaveFactor(_wave);
        own->health = factor * (1.0f + GROUP_HEALTH * float(_groupSize - 1));
        own->damage = 1.0f;
        own->pressure = 1.0f + PRESSURE_DAMAGE * float(_pressure);
        own->wave = _wave;
        own->boss = spawn.boss;
        own->role = RoleOf(spawn.entry, spawn.boss);
        own->look = LookOf(spawn.entry, spawn.boss);
        if (spawn.boss && enemy->GetScriptName().rfind("boss_", 0) != 0
            && enemy->GetCreatureTemplate()->rank != CREATURE_ELITE_WORLDBOSS)
        {
            own->health *= BossHealth(_wave);
            own->damage *= BossDamage(_wave);
        }
        if (spawn.boss && MajorAt(_wave))
        {
            StartMajor(enemy, MajorAt(_wave));
            enemy->Yell(MAJOR_LINES[MajorAt(_wave)].Enter[urand(0, 1)], LANG_UNIVERSAL);
        }
        else if (spawn.boss)
        {
            if (LadderLines const* lines = LadderLinesOf(spawn.entry))
                enemy->Yell(lines->Enter, LANG_UNIVERSAL);
            if (_wave >= MECHANICS_FROM)
                _bosses[enemy->GetGUID()] = { urand(5000, ERUPTION_EVERY_MS),
                    urand(NOVA_EVERY_MS / 2, NOVA_EVERY_MS), false };
        }
        enemy->SetFaction(ENEMY_FACTION);
        enemy->SetReactState(REACT_AGGRESSIVE);
        enemy->SetHomePosition(CENTER);
        DungeonScaling::ApplyHealth(enemy);         // its health with its wave's multiplier, now it has one
        enemy->SetFullHealth();
        for (uint16 index : { UNIT_FIELD_HEALTH, UNIT_FIELD_MAXHEALTH })
            enemy->ForceValuesUpdateAtIndex(index);
        _enemies.push_back(enemy->GetGUID());
        // A 15th wave's boss: its fall earns the run a piece of spoils (not the wave the run started at).
        if (spawn.boss && _wave % LOOT_EVERY == 0 && _wave > _setup.startWave)
        {
            _lootBosses.push_back(enemy->GetGUID());
            _lootWave = _wave;
        }
        ApplyRole(enemy, spawn.role);
        Engage(enemy);
    }

    // A loot wave's bosses all fallen: every Hero still in the run (standing or not) earns a piece of spoils.
    void CheckLoot()
    {
        if (_lootBosses.empty())
            return;
        for (ObjectGuid const& guid : _lootBosses)
            if (Creature* boss = instance->GetCreature(guid); boss && boss->IsAlive())
                return;
        _lootBosses.clear();
        for (auto& [guid, record] : _records)
            if (!record.left)
            {
                ++record.loot;
                record.lootWaves.push_back(_lootWave);
            }
        Announce("Wave " + std::to_string(_lootWave) + "'s boss falls: spoils for every Hero!", true);
    }

    // The bosses' mechanics (MECHANICS_FROM): each boss's timers, and the blasts on their way.
    static bool Due(uint32& ms, uint32 diff, uint32 every)
    {
        if (ms > diff)
        {
            ms -= diff;
            return false;
        }
        ms = every;
        return true;
    }

    void Mechanics(uint32 diff)
    {
        for (auto it = _bosses.begin(); it != _bosses.end();)
        {
            Creature* boss = instance->GetCreature(it->first);
            Enemy* own = boss ? boss->CustomData.Get<Enemy>("titan_firepit") : nullptr;
            if (!boss || !boss->IsAlive() || !own)
            {
                it = _bosses.erase(it);
                continue;
            }
            BossState& state = it->second;
            if (Due(state.eruptMs, diff, ERUPTION_EVERY_MS))
                Erupt(boss);
            if (own->wave >= NOVA_FROM && Due(state.novaMs, diff, NOVA_EVERY_MS))
                Nova(boss);
            if (own->wave >= ENRAGE_FROM && !state.enraged && boss->HealthBelowPct(int32(ENRAGE_BELOW)))
            {
                state.enraged = true;
                own->damage *= ENRAGE_DAMAGE;
                Announce(boss->GetName() + " is enraged!", true);
            }
            ++it;
        }
        for (std::size_t i = 0; i < _blasts.size();)
        {
            Blast& blast = _blasts[i];
            if (blast.keg)
                if (Creature* keg = instance->GetCreature(blast.keg); !keg || !keg->IsAlive())
                {
                    Defused(blast, keg);
                    _blasts.erase(_blasts.begin() + i);
                    continue;
                }
            if (blast.ms > diff)
            {
                blast.ms -= diff;
                ++i;
                continue;
            }
            Blast due = blast;
            _blasts.erase(_blasts.begin() + i);
            Detonate(due);
        }
        GrowTimers(diff);
        LaunchCannonballs();
        FxTick();
    }

    GameObject* Marker(uint32 entry, Position const& at)
    {
        return instance->SummonGameObject(entry, at.GetPositionX(), at.GetPositionY(), at.GetPositionZ(), 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f, 0);
    }

    void Erupt(Creature* boss)
    {
        std::vector<Player*> heroes = Living();
        Acore::Containers::RandomShuffle(heroes);
        if (heroes.size() > ERUPTION_TARGETS)
            heroes.resize(ERUPTION_TARGETS);
        for (Player* hero : heroes)
        {
            Blast blast;
            blast.source = boss->GetGUID();
            blast.at = hero->GetPosition();
            blast.radius = ERUPTION_RADIUS;
            blast.share = ERUPTION_DAMAGE;
            blast.ms = ERUPTION_DELAY_MS;
            Telegraph(blast);
            _blasts.push_back(blast);
        }
        if (!heroes.empty())
            Announce(boss->GetName() + " calls up the magma: move out of the flames!");
    }

    void Nova(Creature* boss)
    {
        Blast blast;
        blast.source = boss->GetGUID();
        blast.at = boss->GetPosition();
        blast.radius = NOVA_RADIUS;
        blast.share = NOVA_DAMAGE;
        blast.ms = NOVA_DELAY_MS;
        blast.nova = true;
        Telegraph(blast);
        boss->SetControlled(true, UNIT_STATE_ROOT);
        _blasts.push_back(blast);
        Announce(boss->GetName() + " gathers a Searing Nova: get away from it!", true);
    }

    void Detonate(Blast const& blast)
    {
        if (GameObject* marker = instance->GetGameObject(blast.marker))
            marker->Delete();
        for (ObjectGuid const& guid : blast.markers)
            if (GameObject* marker = instance->GetGameObject(guid))
                marker->Delete();
        Creature* boss = instance->GetCreature(blast.source);
        if (blast.release && boss)
            boss->SetControlled(false, UNIT_STATE_ROOT);
        if (blast.nova && boss)
            boss->SetControlled(false, UNIT_STATE_ROOT);
        if (blast.nova && (!boss || !boss->IsAlive()))
            return;                                     // a nova dies with its boss
        Position at = blast.nova ? boss->GetPosition() : blast.at;
        TempSummon* source = instance->SummonCreature(NPC_ERUPTION, at, nullptr, 2000);
        if (source)
            source->SetFaction(35);                     // friendly: its looks hit no one
        if (blast.keg)
            if (Creature* keg = instance->GetCreature(blast.keg))
                keg->DespawnOrUnsummon();
        LandLook(blast, at, source);
        Unit* attacker = blast.nova ? static_cast<Unit*>(boss) : static_cast<Unit*>(source);
        if (!attacker)
            return;
        SpellInfo const* spell = sSpellMgr->GetSpellInfo(blast.nova ? SPELL_FIRE_NOVA
            : blast.log ? blast.log : SPELL_ERUPTION_LOG);
        if (blast.volatileEmber && _volatile)
            --_volatile;
        if (blast.crack >= 0 && std::size_t(blast.crack) < _cracks.size())
        {
            Crack& crack = _cracks[blast.crack];
            crack.until = _runMs + CRACKED_MS;
            Zone(crack.markers, DECAL_CIRCLE, crack.at, 0.0f, CRACKED_RADIUS, DECAL_FROST, STYLE_CRACKED);
        }
        if (blast.pool)
            LayPool(at, blast.poolRadius ? blast.poolRadius : blast.radius, POOL_SHARE, MOLTEN_POOL_MS);
        if (blast.wind)
        {
            Wind wind;
            wind.from = blast.at;
            wind.angle = blast.facing;
            wind.until = _runMs + FIREWALL_MS;
            ZoneLine(wind.markers, blast.at, blast.facing, blast.length, blast.radius, DECAL_DANGER, STYLE_SWIRL);
            _winds.push_back(wind);
            for (Player* hero : Living())
                if (InShape(blast, at, hero))
                    hero->AddAura(SPELL_CINDER_WINDS, hero);
        }
        if (blast.physical)
            spell = sSpellMgr->GetSpellInfo(SPELL_PHYSICAL_LOG);
        SpellSchoolMask school = blast.physical ? SPELL_SCHOOL_MASK_NORMAL : SPELL_SCHOOL_MASK_FIRE;
        for (Player* hero : Living())
        {
            if (!InShape(blast, at, hero) || std::abs(hero->GetPositionZ() - at.GetPositionZ()) > 6.0f
                || IsProtected(hero))
                continue;
            DamageInfo damage(attacker, hero, uint32(float(hero->GetMaxHealth()) * blast.share), spell, school,
                SPELL_DIRECT_DAMAGE);
            Unit::CalcAbsorbResist(damage);
            attacker->SendSpellNonMeleeDamageLog(hero, spell, damage.GetDamage(), school, damage.GetAbsorb(),
                damage.GetResist(), false, 0);
            Unit::DealDamage(attacker, hero, damage.GetDamage(), nullptr, SPELL_DIRECT_DAMAGE, school, spell, false);
        }
    }

    // Whether a Hero stands in a danger's shape (at: where it is centred as it goes off).
    static bool InShape(Blast const& blast, Position const& at, Player* hero)
    {
        float dx = hero->GetPositionX() - at.GetPositionX(), dy = hero->GetPositionY() - at.GetPositionY();
        float distance = std::hypot(dx, dy);
        float c = std::cos(blast.facing), s = std::sin(blast.facing);
        auto within = [&blast](float angle)
        {
            float diff = std::fabs(Position::NormalizeOrientation(angle - blast.facing));
            return std::min(diff, 2.0f * float(M_PI) - diff) <= blast.halfArc;
        };
        switch (blast.shape)
        {
            case SHAPE_CONE: return distance <= blast.radius && within(std::atan2(dy, dx));
            case SHAPE_LINE:
            {
                float along = dx * c + dy * s, side = -dx * s + dy * c;
                return along >= 0.0f && along <= blast.length && std::fabs(side) <= blast.radius;
            }
            case SHAPE_RING: return distance >= blast.inner && distance <= blast.radius;
            case SHAPE_HALF: return dx * c + dy * s >= 0.0f;
            case SHAPE_SECTOR: return within(std::atan2(dy, dx));
            default: return distance <= blast.radius;
        }
    }

    // A danger's warning on the floor: a small circle its ring; any other shape, embers along its outline (and,
    // for a half or a third of the arena, across it).
    // A decal (and its timer) of the blast's on the floor: at `at`, facing `facing`, `scale` yards of radius, in
    // `colour` (DECAL_DANGER unless said).
    void Decal(Blast& blast, uint8 decal, Position const& at, float facing, float scale,
        uint8 colour = DECAL_DANGER, int8 style = STYLE_PLAIN)
    {
        float z = FloorAt(at.GetPositionX(), at.GetPositionY()) + DECAL_LIFT;
        std::vector<uint32> entries = { GO_DECAL_FIRST + colour * DECALS_A_COLOUR + decal };
        if (style > STYLE_PLAIN)
            entries.push_back(GO_STYLE_FIRST + uint32(style - 1) * STYLE_OBJECTS + colour * DECAL_SHAPES + decal);
        for (uint32 entry : entries)
            if (GameObject* go = instance->SummonGameObject(entry, at.GetPositionX(), at.GetPositionY(), z,
                facing, 0.0f, 0.0f, std::sin(facing / 2.0f), std::cos(facing / 2.0f), 0))
            {
                go->SetObjectScale(scale);
                blast.markers.push_back(go->GetGUID());
            }
        blast.timers.push_back({ colour * DECAL_SHAPES + uint32(decal), Position(at.GetPositionX(),
            at.GetPositionY(), z), facing, scale });
    }

    // A lasting zone on the floor (a pool, a wall of fire, frozen ground): its decal and texture, no timer, until
    // its owner deletes `into`'s objects.
    void Zone(std::vector<ObjectGuid>& into, uint8 decal, Position const& at, float facing, float scale, uint8 colour,
        int8 style = STYLE_PLAIN)
    {
        float z = FloorAt(at.GetPositionX(), at.GetPositionY()) + DECAL_LIFT;
        std::vector<uint32> entries = { GO_DECAL_FIRST + colour * DECALS_A_COLOUR + decal };
        if (style > STYLE_PLAIN)
            entries.push_back(GO_STYLE_FIRST + uint32(style - 1) * STYLE_OBJECTS + colour * DECAL_SHAPES + decal);
        for (uint32 entry : entries)
            if (GameObject* go = instance->SummonGameObject(entry, at.GetPositionX(), at.GetPositionY(), z, facing,
                0.0f, 0.0f, std::sin(facing / 2.0f), std::cos(facing / 2.0f), 0))
            {
                go->SetObjectScale(scale);
                into.push_back(go->GetGUID());
            }
    }

    // A lasting line zone: pieces `width` yards either side of its axis from `from` along `facing` for `length`
    // yards (as far as the arena reaches), capped at both ends.
    void ZoneLine(std::vector<ObjectGuid>& into, Position const& from, float facing, float length, float width,
        uint8 colour, int8 style = STYLE_PLAIN)
    {
        std::vector<Position> pieces;
        for (float along = width; along - width < length; along += width * 2.0f)
        {
            Position piece(from.GetPositionX() + along * std::cos(facing), from.GetPositionY() + along
                * std::sin(facing), from.GetPositionZ());
            if (piece.GetExactDist2d(&CENTER) > ARENA_RADIUS)
            {
                if (!pieces.empty())
                    break;
                continue;
            }
            pieces.push_back(piece);
        }
        for (std::size_t i = 0; i < pieces.size(); ++i)
        {
            bool first = i == 0, last = i + 1 == pieces.size();
            Zone(into, first || last ? DECAL_LINE_CAP : DECAL_LINE_MID, pieces[i],
                first && !last ? facing + float(M_PI) : facing, width, colour, style);
        }
    }

    void DeleteAll(std::vector<ObjectGuid> const& objects)
    {
        for (ObjectGuid const& guid : objects)
            if (GameObject* go = instance->GetGameObject(guid))
                go->Delete();
    }

    // Burning ground: `share` a second within `radius` for `ms`, a cracked red decal; never on the revival ring; 14
    // at most (the oldest goes).
    void LayPool(Position const& at, float radius, float share, uint32 ms)
    {
        if (at.GetExactDist2d(&CENTER) <= ARRIVAL_RADIUS + radius)
            return;
        if (_pools.size() >= POOLS_MOST)
        {
            DeleteAll(_pools.front().markers);
            _pools.erase(_pools.begin());
        }
        Pool pool;
        pool.at = at;
        pool.until = _runMs + ms;
        pool.radius = radius;
        pool.share = share;
        Zone(pool.markers, DECAL_CIRCLE, at, 0.0f, radius, DECAL_DANGER, STYLE_CRACKED);
        _pools.push_back(pool);
    }

    // A cone's or a sector's decal: the cut nearest its arc (a quarter, a third, a half), large past 15 yards.
    static uint8 ConeDecal(float halfArc, float radius)
    {
        if (halfArc < float(M_PI) * 0.29f)
            return DECAL_CONE90;
        if (halfArc < float(M_PI) * 0.42f)
            return DECAL_CONE120_LARGE;
        return radius > DECAL_LARGE ? DECAL_CONE180_LARGE : DECAL_CONE180;
    }

    // A blast's texture when it names none: the Searing Nova swirls, Molten Ground and Cracked Earth crack (and the
    // arena's halves and thirds), a physical blow and a line are hazard-striped, a ring is a runic sigil.
    static int8 StyleOf(Blast const& blast)
    {
        if (blast.style != STYLE_AUTO)
            return blast.style;
        if (blast.nova)
            return STYLE_SWIRL;
        if (blast.pool || blast.crack >= 0 || blast.shape == SHAPE_HALF || blast.shape == SHAPE_SECTOR)
            return STYLE_CRACKED;
        if (blast.physical || blast.shape == SHAPE_LINE)
            return STYLE_HAZARD;
        if (blast.shape == SHAPE_RING)
            return STYLE_RUNIC;
        return STYLE_PLAIN;
    }

    // The blast's warning on the floor: its shape's decal in its colour and texture, its timer growing to the edge as
    // it comes. A line is laid in pieces along its axis (as far as the arena reaches), capped at both ends.
    void Telegraph(Blast& blast)
    {
        blast.total = std::max<uint32>(1, blast.ms);
        blast.timerWindUp = 0;
        while (blast.timerWindUp + 1u < TIMER_WIND_UPS_MS.size()
            && TIMER_WIND_UPS_MS[blast.timerWindUp + 1] <= blast.ms)
            ++blast.timerWindUp;
        uint8 colour = blast.colour;
        int8 style = StyleOf(blast);
        switch (blast.shape)
        {
            case SHAPE_CIRCLE:
                Decal(blast, DECAL_CIRCLE, blast.at, 0.0f, blast.radius, colour, style);
                break;
            case SHAPE_RING:                            // its middle marked safe
                Decal(blast, DECAL_RING, blast.at, 0.0f, blast.radius, colour, style);
                if (blast.inner > 1.0f)
                    Decal(blast, DECAL_CIRCLE, blast.at, 0.0f, blast.inner * 0.9f, DECAL_SAFE);
                break;
            case SHAPE_CONE:
            case SHAPE_SECTOR:
                Decal(blast, ConeDecal(blast.halfArc, blast.radius), blast.at, blast.facing, blast.radius, colour,
                    style);
                break;
            case SHAPE_HALF:
                Decal(blast, DECAL_CONE180_LARGE, blast.at, blast.facing, blast.radius, colour, style);
                break;
            case SHAPE_LINE:
            {
                float width = std::max(0.5f, blast.radius), step = width * 2.0f;
                std::vector<Position> pieces;
                for (float along = width; along - width < blast.length; along += step)
                {
                    Position piece(blast.at.GetPositionX() + along * std::cos(blast.facing),
                        blast.at.GetPositionY() + along * std::sin(blast.facing), blast.at.GetPositionZ());
                    if (piece.GetExactDist2d(&CENTER) > ARENA_RADIUS)
                    {
                        if (!pieces.empty())
                            break;
                        continue;
                    }
                    pieces.push_back(piece);
                }
                for (std::size_t i = 0; i < pieces.size(); ++i)
                {
                    bool first = i == 0, last = i + 1 == pieces.size();
                    if (first || last)
                        Decal(blast, DECAL_LINE_CAP, pieces[i], first && !last ? blast.facing + float(M_PI)
                            : blast.facing, width, colour, style);
                    else
                        Decal(blast, DECAL_LINE_MID, pieces[i], blast.facing, width, colour, style);
                }
                break;
            }
            default:
                break;
        }
        LayTimers(blast);
    }

    // A blast's timers laid once its warning has no more left than their wind-up: each grows (in its model) from
    // the middle (a cone's apex, a line's axis) to the edge just as the blow lands.
    void LayTimers(Blast& blast)
    {
        if (blast.timers.empty() || blast.ms > TIMER_WIND_UPS_MS[blast.timerWindUp] + TIMER_EVERY_MS / 2)
            return;
        uint32 first = GO_TIMER_FIRST + uint32(blast.timerWindUp) * TIMER_OBJECTS;
        for (TimerSpot const& spot : blast.timers)
            if (GameObject* go = instance->SummonGameObject(first + spot.entry, spot.at.GetPositionX(),
                spot.at.GetPositionY(), spot.at.GetPositionZ(), spot.facing, 0.0f, 0.0f, std::sin(spot.facing / 2.0f),
                std::cos(spot.facing / 2.0f), 0))
            {
                go->SetObjectScale(spot.scale);
                blast.markers.push_back(go->GetGUID());
            }
        blast.timers.clear();
    }

    void GrowTimers(uint32 diff)
    {
        _timerMs += diff;
        if (_timerMs < TIMER_EVERY_MS)
            return;
        _timerMs = 0;
        for (Blast& blast : _blasts)
            LayTimers(blast);
    }

    // ---- The blows' looks ------------------------------------------------------------------------------------

    // A look at a point: a friendly, invisible source casting it on itself.
    void Look(Position const& at, uint32 spell)
    {
        if (TempSummon* source = instance->SummonCreature(NPC_ERUPTION, at, nullptr, 3000))
        {
            source->SetFaction(35);
            source->CastSpell(source, spell, true);
        }
    }

    // The camera shaken for every Hero within range of the point (any range: everyone).
    void Shake(Position const& at, float range, uint32 spell)
    {
        for (Player* hero : Living())
            if (!range || hero->GetExactDist2d(&at) <= range)
                hero->CastSpell(hero, spell, true);
    }

    void Queue(Position const& at, uint32 spell, uint32 delayMs, uint32 shake = 0, float shakeRange = 0.0f)
    {
        Fx fx;
        fx.at = at;
        fx.spell = spell;
        fx.due = _runMs + delayMs;
        fx.shake = shake;
        fx.shakeRange = shakeRange;
        _fx.push_back(fx);
    }

    void FxTick()
    {
        for (auto it = _fx.begin(); it != _fx.end();)
        {
            if (it->due > _runMs)
            {
                ++it;
                continue;
            }
            Fx fx = *it;
            it = _fx.erase(it);
            if (fx.spell)
                Look(fx.at, fx.spell);
            if (fx.shake)
                Shake(fx.at, fx.shakeRange, fx.shake);
        }
    }

    // A point of the blast's area, `along` (0 to 1) of its reach out and `side` (-1 to 1) of its width.
    Position PointIn(Blast const& blast, Position const& at, float along, float side) const
    {
        float angle = blast.facing, distance = blast.radius * along;
        if (blast.shape == SHAPE_CONE || blast.shape == SHAPE_SECTOR || blast.shape == SHAPE_HALF)
            angle += side * (blast.shape == SHAPE_HALF ? float(M_PI) / 2.0f : blast.halfArc) * 0.8f;
        float x = at.GetPositionX() + distance * std::cos(angle), y = at.GetPositionY() + distance * std::sin(angle);
        return Position(x, y, FloorAt(x, y));
    }

    // How a blast looks as it lands, by its shape: a circle's fire nova (a cannonball's explosion), a line's chain of
    // explosions racing out from its source, a cone's dust (a physical blow) or fire across its width, a ring's ring
    // of blasts, half or a third of the arena erupting all over; the ground shaking under those near.
    void LandLook(Blast const& blast, Position const& at, TempSummon* source)
    {
        switch (blast.shape)
        {
            case SHAPE_LINE:
            {
                float step = 10.0f;
                uint32 i = 0;
                for (float along = 2.0f; along <= blast.length; along += step, ++i)
                {
                    float x = at.GetPositionX() + along * std::cos(blast.facing);
                    float y = at.GetPositionY() + along * std::sin(blast.facing);
                    if (std::hypot(x - CENTER.GetPositionX(), y - CENTER.GetPositionY()) > ARENA_RADIUS)
                        break;
                    Queue(Position(x, y, FloorAt(x, y)), SPELL_EXPLOSION_SMALL, i * 60, SPELL_SHAKE_SMALL, step);
                }
                return;
            }
            case SHAPE_CONE:
            {
                uint32 look = blast.physical ? SPELL_DUST_BLAST : SPELL_EXPLOSION_SMALL;
                for (float side : { -0.8f, 0.0f, 0.8f })
                    Queue(PointIn(blast, at, 0.75f, side), look, side == 0.0f ? 0 : 80);
                Queue(PointIn(blast, at, 0.4f, 0.0f), look, 40, SPELL_SHAKE_SMALL, blast.radius + 4.0f);
                if (Creature* boss = instance->GetCreature(blast.source))
                    boss->HandleEmoteCommand(EMOTE_ONESHOT_ATTACK1H);
                return;
            }
            case SHAPE_RING:
                for (uint32 i = 0; i < 8; ++i)
                {
                    float a = float(i) * float(M_PI) / 4.0f, d = (blast.inner + blast.radius) / 2.0f;
                    float x = at.GetPositionX() + d * std::cos(a), y = at.GetPositionY() + d * std::sin(a);
                    Queue(Position(x, y, FloorAt(x, y)), SPELL_EXPLOSION_SMALL, i * 40);
                }
                Shake(at, blast.radius + 6.0f, SPELL_SHAKE_SMALL);
                return;
            case SHAPE_HALF:
            case SHAPE_SECTOR:
                for (uint32 i = 0; i < 14; ++i)
                    Queue(PointIn(blast, at, frand(0.15f, 0.95f), frand(-1.0f, 1.0f)),
                        i % 3 ? SPELL_EXPLOSION_SMALL : SPELL_EXPLOSION, urand(0, 700));
                Shake(at, 0.0f, SPELL_SHAKE_SMALL);
                return;
            default:
                break;
        }
        if (blast.keg)
        {
            Look(at, SPELL_EXPLOSION);
            Shake(at, blast.radius + 15.0f, SPELL_SHAKE_MEDIUM);
        }
        else if (blast.cannon)
        {
            Look(at, SPELL_EXPLOSION);
            Shake(at, blast.radius + 8.0f, SPELL_SHAKE_SMALL);
        }
        else if (source)
            source->CastSpell(source, SPELL_FIRE_NOVA, true);
        if (blast.nova)
            Shake(at, blast.radius + 6.0f, SPELL_SHAKE_MEDIUM);
    }

    // ---- The Ashfleet Captain's guns ---------------------------------------------------------------------------

    // Six cannons (and a powder keg by each) run out round the rim, facing the middle; a salute fired as he enters.
    void RunOutTheGuns()
    {
        for (uint8 i = 0; i < ASHFLEET_CANNONS; ++i)
        {
            float a = float(i) * 2.0f * float(M_PI) / ASHFLEET_CANNONS + float(M_PI) / ASHFLEET_CANNONS;
            float facing = Position::NormalizeOrientation(a + float(M_PI));
            float x = CENTER.GetPositionX() + (ARENA_RADIUS - 3.0f) * std::cos(a);
            float y = CENTER.GetPositionY() + (ARENA_RADIUS - 3.0f) * std::sin(a);
            float z = FloorAt(x, y);
            if (GameObject* cannon = instance->SummonGameObject(GO_ASHFLEET_CANNON, x, y, z, facing, 0.0f, 0.0f,
                std::sin(facing / 2.0f), std::cos(facing / 2.0f), 0))
                _major.props.push_back(cannon->GetGUID());
            float kx = x + 2.5f * std::cos(a + float(M_PI) / 2.0f), ky = y + 2.5f * std::sin(a + float(M_PI) / 2.0f);
            if (GameObject* keg = instance->SummonGameObject(GO_POWDER_KEG, kx, ky, FloorAt(kx, ky), facing, 0.0f,
                0.0f, std::sin(facing / 2.0f), std::cos(facing / 2.0f), 0))
                _major.props.push_back(keg->GetGUID());
            Position muzzle(x + 1.5f * std::cos(facing), y + 1.5f * std::sin(facing), z + 1.2f);
            _major.cannons.push_back(muzzle);
            Queue(muzzle, SPELL_MUZZLE_FLASH, 800 + i * 300);
            Queue(muzzle, SPELL_EXPLOSION_SMALL, 850 + i * 300, i + 1 == ASHFLEET_CANNONS ? SPELL_SHAKE_SMALL : 0);
        }
    }

    // A cannonball at a point: its warning now, the nearest cannon firing as it comes in, an explosion as it lands.
    void CannonShot(Creature* boss, Position const& at, float radius, uint32 ms)
    {
        if (_major.cannons.empty())
            return;
        Blast blast = Circle(at, radius, 0.2f);
        uint8 best = 0;
        for (uint8 i = 1; i < _major.cannons.size(); ++i)
            if (_major.cannons[i].GetExactDist2d(&at) < _major.cannons[best].GetExactDist2d(&at))
                best = i;
        blast.cannon = uint8(best + 1);
        Danger(boss, blast, ms);
    }

    // Each cannonball fired so it lands as its warning runs out: its flight timed from its cannon.
    void LaunchCannonballs()
    {
        for (Blast& blast : _blasts)
        {
            if (!blast.cannon || blast.launched || blast.cannon > _major.cannons.size())
                continue;
            Position const& muzzle = _major.cannons[blast.cannon - 1];
            uint32 flight = uint32(muzzle.GetExactDist(&blast.at) / CANNONBALL_SPEED * 1000.0f);
            if (blast.ms > flight)
                continue;
            blast.launched = true;
            if (TempSummon* gun = instance->SummonCreature(NPC_ERUPTION, muzzle, nullptr, flight + 2000))
            {
                gun->SetFaction(35);
                gun->CastSpell(gun, SPELL_MUZZLE_FLASH, true);
                gun->CastSpell(blast.at.GetPositionX(), blast.at.GetPositionY(), blast.at.GetPositionZ() + 0.5f,
                    SPELL_CANNONBALL, true);
            }
        }
    }

    // A volley: a cannonball under each Hero and `extra` more about the arena.
    void Volley(Creature* boss, uint8 extra, float radius, uint32 ms)
    {
        for (Player* hero : Living())
            CannonShot(boss, hero->GetPosition(), radius, ms);
        for (uint8 i = 0; i < extra; ++i)
        {
            float a = frand(0.0f, 2.0f * float(M_PI)), d = frand(ARRIVAL_RADIUS + 6.0f, ARENA_RADIUS - 8.0f);
            float x = CENTER.GetPositionX() + d * std::cos(a), y = CENTER.GetPositionY() + d * std::sin(a);
            CannonShot(boss, Position(x, y, FloorAt(x, y)), radius, ms);
        }
    }

    // A lit Powder Keg rolled in 9 to 13 yd from a Hero (on the floor, off the revival ring): rooted, passive, half a
    // Hero's health (more for more Heroes); its fuse a 12 sec warning on the floor.
    void RollKeg(Creature* boss)
    {
        std::vector<Player*> heroes = Living();
        if (heroes.empty())
            return;
        Player* near = heroes[urand(0, uint32(heroes.size()) - 1)];
        Position at;
        for (uint32 tries = 0; tries < 12; ++tries)
        {
            float a = frand(0.0f, 2.0f * float(M_PI)), d = frand(9.0f, 13.0f);
            float x = near->GetPositionX() + d * std::cos(a), y = near->GetPositionY() + d * std::sin(a);
            float out = std::hypot(x - CENTER.GetPositionX(), y - CENTER.GetPositionY());
            at = Position(x, y, FloorAt(x, y));
            if (out <= ARENA_RADIUS - 4.0f && out >= ARRIVAL_RADIUS + KEG_RADIUS)
                break;
        }
        TempSummon* keg = instance->SummonCreature(NPC_POWDER_KEG, at, nullptr, KEG_FUSE_MS + 5000);
        if (!keg)
            return;
        keg->SetFaction(ENEMY_FACTION);
        keg->SetReactState(REACT_PASSIVE);
        keg->SetControlled(true, UNIT_STATE_ROOT);
        uint32 health = 0;
        for (Player* hero : heroes)
            health += hero->GetMaxHealth();
        health = uint32(float(health) / float(heroes.size()) * KEG_HEALTH * (1.0f + 0.3f * float(heroes.size() - 1)));
        keg->SetMaxHealth(std::max<uint32>(1, health));
        keg->SetFullHealth();
        _major.kegs.push_back(keg->GetGUID());
        Look(at, SPELL_DUST_BLAST);
        Blast fuse = Circle(at, KEG_RADIUS, KEG_SHARE);
        fuse.keg = keg->GetGUID();
        fuse.source = boss->GetGUID();
        fuse.style = STYLE_PLAIN;
        Danger(boss, fuse, KEG_FUSE_MS);
        Announce("A lit Powder Keg rolls in: destroy it before it blows, or blow it next to the Captain!");
    }

    // A keg destroyed before its fuse ran out: a puff, no blast; next to the Captain, it blasts him and stuns him.
    void Defused(Blast const& blast, Creature* keg)
    {
        DeleteAll(blast.markers);
        Position at = keg ? keg->GetPosition() : blast.at;
        Look(at, SPELL_EXPLOSION_SMALL);
        Creature* boss = instance->GetCreature(_major.guid);
        if (!boss || !boss->IsAlive() || _major.kind != MAJOR_CAPTAIN || boss->GetExactDist2d(&at) > KEG_BOSS_RANGE)
            return;
        Look(boss->GetPosition(), SPELL_EXPLOSION);
        Shake(at, 20.0f, SPELL_SHAKE_SMALL);
        uint32 hurt = uint32(float(boss->GetMaxHealth()) * KEG_BOSS_SHARE);
        Unit::DealDamage(boss, boss, std::min(hurt, boss->GetHealth() > 1 ? boss->GetHealth() - 1 : 0u));
        boss->SetControlled(true, UNIT_STATE_STUNNED);
        _major.stunUntil = _runMs + KEG_STUN_MS;
        boss->Yell("ARGH! Me own powder!", LANG_UNIVERSAL);
        Announce("The keg blows up in the Captain's face: he's stunned!", true);
    }

    // Ashfleet Deckhands boarding from the cannons: two, one more for every two more Heroes (4 at most).
    void Boarders()
    {
        if (_major.cannons.empty())
            return;
        uint8 count = uint8(std::min<std::size_t>(DECKHANDS_MOST, 2 + (Living().size() - 1) / 2));
        std::vector<Position> from = _major.cannons;
        Acore::Containers::RandomShuffle(from);
        for (uint8 i = 0; i < count; ++i)
        {
            Position const& muzzle = from[i % from.size()];
            float a = std::atan2(CENTER.GetPositionY() - muzzle.GetPositionY(),
                CENTER.GetPositionX() - muzzle.GetPositionX());
            float x = muzzle.GetPositionX() + 4.0f * std::cos(a), y = muzzle.GetPositionY() + 4.0f * std::sin(a);
            Position at(x, y, FloorAt(x, y), a);
            Look(muzzle, SPELL_MUZZLE_FLASH);
            SpawnOne(Spawn{ NPC_DECKHAND, false, 0, 0 }, &at);
        }
        Announce("Deckhands board the Ring: the Ashfleet's crew joins the fight!");
    }

    // Stray cannon fire: two shots about the arena (never the revival ring).
    void StrayFire(Creature* boss)
    {
        for (uint8 i = 0; i < STRAY_SHOTS; ++i)
        {
            float a = frand(0.0f, 2.0f * float(M_PI)), d = frand(ARRIVAL_RADIUS + 6.0f, ARENA_RADIUS - 6.0f);
            float x = CENTER.GetPositionX() + d * std::cos(a), y = CENTER.GetPositionY() + d * std::sin(a);
            CannonShot(boss, Position(x, y, FloorAt(x, y)), 6.0f, 2500);
        }
    }

    // ---- The major bosses and Marked for Carnage ------------------------------------------------------------

    // The boss a boss wave brings: a major at Carnage waves, else the ladder's.
    static uint32 BossAt(uint32 wave)
    {
        if (uint8 major = MajorAt(wave))
            return MAJOR_ENTRIES[major];
        return BOSSES[(std::max<uint32>(wave, BOSS_EVERY) / BOSS_EVERY - 1) % std::size(BOSSES)];
    }

    // A danger of a major boss's, warned on the floor and set off ms later (its shape's fields set by the caller).
    Blast& Danger(Creature* boss, Blast blast, uint32 ms)
    {
        blast.source = boss->GetGUID();
        blast.ms = ms;
        Telegraph(blast);
        _blasts.push_back(blast);
        return _blasts.back();
    }

    Blast Circle(Position const& at, float radius, float share)
    {
        Blast blast;
        blast.at = at;
        blast.radius = radius;
        blast.share = share;
        return blast;
    }

    // A frontal cone (the boss rooted, facing its victim, until it lands).
    void Cone(Creature* boss, float radius, float halfArc, float share, uint32 ms, bool physical, char const* name)
    {
        if (Unit* victim = boss->GetVictim())
            boss->SetFacingToObject(victim);
        boss->SetControlled(true, UNIT_STATE_ROOT);
        Blast blast = Circle(boss->GetPosition(), radius, share);
        blast.shape = SHAPE_CONE;
        blast.facing = boss->GetOrientation();
        blast.halfArc = halfArc;
        blast.physical = physical;
        blast.release = true;
        Danger(boss, blast, ms);
        Announce(boss->GetName() + " winds up " + name + ": get behind it or away!");
    }

    // Circles under up to `most` Heroes' feet.
    void UnderHeroes(Creature* boss, uint32 most, float radius, float share, uint32 ms, bool pool)
    {
        std::vector<Player*> heroes = Living();
        Acore::Containers::RandomShuffle(heroes);
        if (heroes.size() > most)
            heroes.resize(most);
        for (Player* hero : heroes)
        {
            Blast blast = Circle(hero->GetPosition(), radius, share);
            blast.pool = pool;
            Danger(boss, blast, ms);
        }
    }

    // A third of the arena (about its centre), facing `angle`.
    void Third(Creature* boss, float angle, float share, uint32 ms)
    {
        Blast blast = Circle(CENTER, ARENA_RADIUS, share);
        blast.shape = SHAPE_SECTOR;
        blast.facing = angle;
        blast.halfArc = float(M_PI) / 3.0f;
        Danger(boss, blast, ms);
    }

    void StartMajor(Creature* boss, uint8 kind)
    {
        _major = Major();
        _major.guid = boss->GetGUID();
        _major.kind = kind;
        switch (kind)
        {
            case MAJOR_CAPTAIN: _major.thresholds = { 70, 35 }; _major.enrageAt = 30; break;
            case MAJOR_COLOSSUS: _major.thresholds = { 70, 35 }; _major.enrageAt = 25; break;
            case MAJOR_MATRON: _major.thresholds = { 65, 30 }; break;
            case MAJOR_WARDEN: _major.thresholds = { 70, 35 }; _major.enrageAt = 25; break;
            case MAJOR_CHAMPION: _major.thresholds = { 75, 50, 25 }; _major.enrageAt = 10; break;
            default: break;
        }
        _major.t1 = 8000;
        _major.t2 = 14000;
        _major.t3 = 20000;
        _major.t4 = 12000;
        if (kind == MAJOR_CAPTAIN)
        {
            RunOutTheGuns();
            _major.kegMs = 9000;
            _major.deckhandMs = 15000;
            _major.strayMs = 5000;
        }
        _carnageMs = IsCarnage(_wave) ? CARNAGE_MS : 0;
        if (_carnageMs)
            Announce("MARKED FOR CARNAGE: defeat " + boss->GetName() + " within 5 minutes, or the arena detonates!",
                true);
    }

    // Each update with a major boss: its phases (queued, never two at once), its enrage, its abilities.
    void MajorTick(uint32 diff)
    {
        if (!_major.guid)
            return;
        Creature* boss = instance->GetCreature(_major.guid);
        if (!boss || !boss->IsAlive())
        {
            EndMajor(boss);
            return;
        }
        if (_major.stunUntil && _runMs >= _major.stunUntil)
        {
            _major.stunUntil = 0;
            boss->SetControlled(false, UNIT_STATE_STUNNED);
        }
        uint8 health = uint8(boss->GetHealthPct());
        while (!_major.thresholds.empty() && health <= _major.thresholds.front())
        {
            _major.queued.push_back(_major.thresholds.front());
            _major.thresholds.erase(_major.thresholds.begin());
        }
        if (_major.enrageAt && !_major.enraged && health <= _major.enrageAt)
        {
            _major.enraged = true;
            _major.enragePending = true;
        }
        if (_major.phase)
        {
            PhaseTick(boss, diff);
            return;
        }
        if (_major.enragePending)
        {
            _major.enragePending = false;
            Enrage(boss);
        }
        if (!_major.queued.empty())
        {
            _major.phase = _major.queued.front();
            _major.queued.erase(_major.queued.begin());
            StartPhase(boss);
            return;
        }
        auto due = [diff](uint32& ms, uint32 every)
        {
            if (ms > diff)
            {
                ms -= diff;
                return false;
            }
            ms = every;
            return true;
        };
        bool solo = Living().size() <= 1;
        switch (_major.kind)
        {
            case MAJOR_CAPTAIN:
                if (due(_major.t1, 18000))
                    if (Player* hero = RandomHero())
                    {
                        Blast line = Circle(boss->GetPosition(), 1.5f, 0.2f);
                        line.shape = SHAPE_LINE;
                        line.facing = boss->GetAngle(hero);
                        line.length = ARENA_RADIUS * 2.0f;
                        Danger(boss, line, 4000);
                        Announce(boss->GetName() + " lays a Powder Line: step off it!");
                    }
                if (due(_major.t2, 24000))
                    Cone(boss, 8.0f, float(M_PI) / 4.0f, 0.25f, 3000, true, "a Cutlass Sweep");
                if (due(_major.kegMs, _major.enraged ? KEG_STAND_EVERY_MS : KEG_EVERY_MS))
                    for (std::size_t k = 0; k < (Living().size() >= 3 ? 2u : 1u); ++k)
                        RollKeg(boss);
                if (due(_major.deckhandMs, _major.enraged ? DECKHANDS_STAND_EVERY_MS : DECKHANDS_EVERY_MS))
                    Boarders();
                if (due(_major.strayMs, STRAY_EVERY_MS))
                    StrayFire(boss);
                // Final Stand: the guns fire on every Hero, 9 sec apart.
                if (_major.enraged && due(_major.t3, BROADSIDE_EVERY_MS))
                {
                    Volley(boss, 2, 6.0f, 3000);
                    Announce("BROADSIDE! The Ashfleet's guns are on you, keep moving!");
                }
                break;
            case MAJOR_COLOSSUS:
                if (due(_major.t1, 22000))
                    Cone(boss, 8.0f, float(M_PI) / 4.0f, 0.25f, 4000, true, "Ironbreaker");
                if (due(_major.t2, _major.enraged ? 22000 : 28000))
                {
                    UnderHeroes(boss, solo ? 1 : 2, 4.0f, 0.18f, 4000, false);
                    Announce(boss->GetName() + "'s Molten Rupture: move!");
                }
                break;
            case MAJOR_MATRON:
                if (due(_major.t1, 24000))
                {
                    UnderHeroes(boss, 1, 4.0f, 0.18f, 4000, true);
                    Announce(boss->GetName() + " spits fire: move away!");
                }
                if (due(_major.t2, 20000))
                    Cone(boss, 7.0f, float(M_PI) / 4.0f, 0.23f, 3000, true, "its Rending Mandibles");
                if (due(_major.t3, 35000))
                    Cinderweb(boss);
                break;
            case MAJOR_WARDEN:
                if (due(_major.t1, 22000))
                    Cone(boss, 7.0f, float(M_PI) / 4.0f, 0.25f, 4000, true, "a Warden's Crush");
                if (due(_major.t2, _major.enraged ? 22000 : 28000))
                {
                    for (uint32 i = 0; i < 2; ++i)
                    {
                        float angle = frand(0.0f, 2.0f * float(M_PI)), radius = frand(ARRIVAL_RADIUS, 40.0f);
                        float x = CENTER.GetPositionX() + radius * std::cos(angle);
                        float y = CENTER.GetPositionY() + radius * std::sin(angle);
                        Danger(boss, Circle(Position(x, y, FloorAt(x, y)), 4.0f, 0.18f), 5000);
                    }
                    Announce("Furnace Vents glow: stay clear!");
                }
                if (due(_major.t3, 40000))
                {
                    _major.side = !_major.side;
                    Blast half = Circle(CENTER, ARENA_RADIUS, 0.25f);
                    half.shape = SHAPE_HALF;
                    half.facing = _major.side ? 0.0f : float(M_PI);
                    Danger(boss, half, 6000);
                    Announce("Divided Furnace: half the arena glows, cross to the other side!", true);
                }
                break;
            case MAJOR_CHAMPION:
                if (due(_major.t1, 20000))
                    Cone(boss, 8.0f, float(M_PI) / 4.0f, 0.25f, 3000, true, "a Champion Cleave");
                if (due(_major.t2, 25000))
                {
                    UnderHeroes(boss, 1, 4.0f, 0.18f, 4000, false);
                    Announce(boss->GetName() + " takes a Blazing Shot: move!");
                }
                if (due(_major.t3, 35000))
                {
                    Third(boss, frand(0.0f, 2.0f * float(M_PI)), 0.2f, 5000);
                    Announce(boss->GetName() + " Challenges the Crowd: leave the marked third!", true);
                }
                if (_major.weapon && due(_major.t4, _major.weapon == 3 ? 20000 : 24000))
                    WeaponStrike(boss);
                break;
            default:
                break;
        }
    }

    Player* RandomHero()
    {
        std::vector<Player*> heroes = Living();
        return heroes.empty() ? nullptr : heroes[urand(0, uint32(heroes.size()) - 1)];
    }

    // The Matron's Cinderweb: three web lines across the arena for 15 sec after a 5 sec warning, each held by a
    // Web Anchor (20% of an enemy's health) whose death takes its line; touching one slows by 30% for 3 sec.
    void Cinderweb(Creature* boss)
    {
        for (uint32 i = 0; i < 3; ++i)
        {
            Web web;
            web.angle = frand(0.0f, float(M_PI));
            float offset = frand(-25.0f, 25.0f), across = web.angle + float(M_PI) / 2.0f;
            web.center = Position(CENTER.GetPositionX() + offset * std::cos(across),
                CENTER.GetPositionY() + offset * std::sin(across), CENTER.GetPositionZ());
            web.activeAt = _runMs + 5000;
            web.until = web.activeAt + 15000;
            // Its line, 80 yd through its middle: an arcane band of runes (its 1.5 yd reach either side).
            Position from(web.center.GetPositionX() - 40.0f * std::cos(web.angle),
                web.center.GetPositionY() - 40.0f * std::sin(web.angle), web.center.GetPositionZ());
            ZoneLine(web.markers, from, web.angle, 80.0f, 1.5f, DECAL_ARCANE, STYLE_RUNIC);
            if (TempSummon* anchor = instance->SummonCreature(NPC_WEB_ANCHOR, Position(web.center.GetPositionX(),
                web.center.GetPositionY(), FloorAt(web.center.GetPositionX(), web.center.GetPositionY()))))
            {
                anchor->SetFaction(ENEMY_FACTION);
                anchor->SetReactState(REACT_PASSIVE);
                anchor->SetControlled(true, UNIT_STATE_ROOT);
                Enemy* own = anchor->CustomData.GetDefault<Enemy>("titan_firepit");
                own->health = WaveFactor(_wave) * (1.0f + GROUP_HEALTH * float(_groupSize - 1)) * 0.2f;
                own->wave = _wave;
                DungeonScaling::ApplyHealth(anchor);
                anchor->SetFullHealth();
                web.anchor = anchor->GetGUID();
            }
            _webs.push_back(web);
        }
        Announce(boss->GetName() + " spins a Cinderweb: avoid the lines, or break their anchors!", true);
    }

    void WebsTick()
    {
        for (std::size_t i = 0; i < _webs.size();)
        {
            Web& web = _webs[i];
            Creature* anchor = instance->GetCreature(web.anchor);
            if (_runMs >= web.until || !anchor || !anchor->IsAlive())
            {
                for (ObjectGuid const& guid : web.markers)
                    if (GameObject* marker = instance->GetGameObject(guid))
                        marker->Delete();
                if (anchor)
                    anchor->DespawnOrUnsummon(Milliseconds(anchor->IsAlive() ? 0 : CORPSE_MS));
                _webs.erase(_webs.begin() + i);
                continue;
            }
            if (_runMs >= web.activeAt)
            {
                float c = std::cos(web.angle), s = std::sin(web.angle);
                for (Player* hero : Living())
                {
                    float dx = hero->GetPositionX() - web.center.GetPositionX();
                    float dy = hero->GetPositionY() - web.center.GetPositionY();
                    if (std::fabs(-dx * s + dy * c) <= 1.5f)
                        hero->AddAura(SPELL_CINDERWEB, hero);
                }
            }
            ++i;
        }
    }

    // The Champion's weapon signature.
    void WeaponStrike(Creature* boss)
    {
        switch (_major.weapon)
        {
            case 1:
                if (Player* hero = RandomHero())
                {
                    Blast line = Circle(boss->GetPosition(), 2.0f, 0.22f);
                    line.shape = SHAPE_LINE;
                    line.facing = boss->GetAngle(hero);
                    line.length = ARENA_RADIUS * 2.0f;
                    line.physical = true;
                    Danger(boss, line, 4000);
                    Announce(boss->GetName() + "'s Crushing Line: step off it!");
                }
                break;
            case 2:
            {
                Blast ring = Circle(boss->GetPosition(), 12.0f, 0.22f);
                ring.shape = SHAPE_RING;
                ring.inner = 6.0f;
                Danger(boss, ring, 5000);
                Announce(boss->GetName() + "'s Ring of Embers: get close or far!", true);
                break;
            }
            case 3:
                Cone(boss, 10.0f, float(M_PI) / 2.0f, 0.28f, 4000, true, "an Executioner's Arc");
                break;
            default:
                break;
        }
    }

    void StartPhase(Creature* boss)
    {
        uint8 index = _major.kind == MAJOR_CHAMPION ? _major.weapon : _major.phaseCount;
        if (index < 3 && *MAJOR_LINES[_major.kind].Phase[index])
            boss->Yell(MAJOR_LINES[_major.kind].Phase[index], LANG_UNIVERSAL);
        ++_major.phaseCount;
        _major.phaseMs = _major.kind == MAJOR_MATRON ? WEBSTORM_MS : PHASE_MS;
        _major.step = 0;
        _major.stepMs = 0;
        switch (_major.kind)
        {
            case MAJOR_CAPTAIN:
                boss->NearTeleportTo(CENTER.GetPositionX(), CENTER.GetPositionY(), CENTER.GetPositionZ(),
                    boss->GetOrientation());
                boss->SetControlled(true, UNIT_STATE_ROOT);
                _major.taken = TRIAL_TAKEN;
                Announce(boss->GetName() + ": TRIAL BY FIRE! Cannon volleys incoming, watch the floor!", true);
                break;
            case MAJOR_COLOSSUS:
                boss->SetControlled(true, UNIT_STATE_ROOT);
                Announce(boss->GetName() + " SHATTERS THE CORE: hammer strikes on the cracks!", true);
                break;
            case MAJOR_MATRON:
                boss->AttackStop();
                boss->SetControlled(true, UNIT_STATE_ROOT);
                boss->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                Announce(boss->GetName() + " climbs out of reach: WEBSTORM!", true);
                break;
            case MAJOR_WARDEN:
                boss->SetControlled(true, UNIT_STATE_ROOT);
                Announce(boss->GetName() + ": FURNACE CYCLE! The arena erupts in sections!", true);
                break;
            case MAJOR_CHAMPION:
                // A weapon change: its signature, kept at least 10 sec.
                _major.weapon = uint8(_major.weapon + 1);
                _major.phaseMs = WEAPON_LEAST_MS;
                _major.t4 = 3000;
                if (_major.weapon == 3)
                    if (Aura* aura = boss->AddAura(SPELL_ASHEN_EMPOWERMENT, boss))
                    {
                        aura->SetMaxDuration(int32(CARNAGE_MS));
                        aura->SetDuration(int32(CARNAGE_MS));
                    }
                Announce(boss->GetName() + std::string(_major.weapon == 1 ? " takes up the Ironbreaker!"
                    : _major.weapon == 2 ? " takes up the Ashen Glaive!" : " takes up the Executioner's Blade!"),
                    true);
                break;
            default:
                break;
        }
    }

    void PhaseTick(Creature* boss, uint32 diff)
    {
        if (_major.kind == MAJOR_CHAMPION)
        {
            // Its weapons' least time: the Champion fights on meanwhile.
            _major.phaseMs = _major.phaseMs > diff ? _major.phaseMs - diff : 0;
            if (!_major.phaseMs)
                _major.phase = 0;
            if (_major.t4 > diff)
                _major.t4 -= diff;
            else
            {
                _major.t4 = _major.weapon == 3 ? 20000 : 24000;
                WeaponStrike(boss);
            }
            return;
        }
        // Three dangers 4 sec apart (Webstorm: two), each with its own 4 sec warning.
        uint8 steps = _major.kind == MAJOR_MATRON ? 2 : 3;
        _major.stepMs = _major.stepMs > diff ? _major.stepMs - diff : 0;
        if (!_major.stepMs && _major.step < steps)
        {
            _major.stepMs = 4000;
            ++_major.step;
            float angle = frand(0.0f, 2.0f * float(M_PI));
            if (_major.kind == MAJOR_CAPTAIN)           // Trial by Fire: a volley under every Hero, and more
                Volley(boss, 3, 7.0f, 3500);
            else if (_major.kind == MAJOR_COLOSSUS)
            {
                float radius = frand(ARRIVAL_RADIUS + 6.0f, 40.0f);
                float x = CENTER.GetPositionX() + radius * std::cos(angle);
                float y = CENTER.GetPositionY() + radius * std::sin(angle);
                Danger(boss, Circle(Position(x, y, FloorAt(x, y)), 8.0f, 0.2f), 4000);
            }
            else
                Third(boss, angle, 0.2f, 4000);
        }
        _major.phaseMs = _major.phaseMs > diff ? _major.phaseMs - diff : 0;
        if (_major.phaseMs)
            return;
        // The phase ends.
        _major.phase = 0;
        _major.taken = 1.0f;
        boss->SetControlled(false, UNIT_STATE_ROOT);
        if (_major.kind == MAJOR_MATRON)
            boss->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        if (_major.kind == MAJOR_COLOSSUS)
        {
            _major.exposedUntil = _runMs + PHASE_MS;
            Announce("The Colossus's core is EXPOSED: it takes 20% more damage for 12 sec!", true);
        }
        Engage(boss);
    }

    // Final Stand (the Captain), Unstable Forge (the Colossus: Molten Rupture sooner), Meltdown (the Warden),
    // the Firepit Answers (the Champion: the arena's edge ignites).
    void Enrage(Creature* boss)
    {
        if (*MAJOR_LINES[_major.kind].Enrage)
            boss->Yell(MAJOR_LINES[_major.kind].Enrage, LANG_UNIVERSAL);
        Enemy* own = EnemyOf(boss);
        switch (_major.kind)
        {
            case MAJOR_CAPTAIN:
                if (own)
                    own->damage *= FINAL_STAND_DAMAGE;
                // Final Stand: a blast that shakes the whole arena, and the Captain grows; his guns keep firing
                // (Broadside).
                Look(boss->GetPosition(), SPELL_EXPLOSION_HUGE);
                Shake(boss->GetPosition(), 0.0f, SPELL_SHAKE_MEDIUM);
                boss->SetObjectScale(boss->GetObjectScale() * 1.25f);
                _major.t3 = 3000;
                [[fallthrough]];
            case MAJOR_WARDEN:
                if (Aura* aura = boss->AddAura(SPELL_ASHEN_EMPOWERMENT, boss))
                {
                    aura->SetMaxDuration(int32(CARNAGE_MS));
                    aura->SetDuration(int32(CARNAGE_MS));
                }
                Announce(boss->GetName() + (_major.kind == MAJOR_CAPTAIN ? " makes a FINAL STAND!"
                    : " melts down: its vents quicken!"), true);
                break;
            case MAJOR_COLOSSUS:
                Announce(boss->GetName() + "'s forge grows unstable: Molten Rupture comes faster!", true);
                break;
            case MAJOR_CHAMPION:
                _major.edgeAt = _runMs + 6000;
                for (float a = 0.0f; a < 2.0f * float(M_PI); a += 0.12f)
                {
                    float x = CENTER.GetPositionX() + (ARENA_RADIUS - 2.0f) * std::cos(a);
                    float y = CENTER.GetPositionY() + (ARENA_RADIUS - 2.0f) * std::sin(a);
                    if (GameObject* marker = Marker(GO_EMBERS, Position(x, y, FloorAt(x, y))))
                        _major.edgeMarkers.push_back(marker->GetGUID());
                }
                Announce("THE FIREPIT ANSWERS: the arena's edge ignites in 6 sec, get to the middle!", true);
                break;
            default:
                break;
        }
    }

    // Each second: the burning edge (the Champion's last stand), the Carnage clock.
    void MajorSecond()
    {
        WebsTick();
        if (_major.edgeAt && _runMs >= _major.edgeAt)
        {
            Unit* source = AffixSource();
            for (Player* hero : Living())
                if (hero->GetExactDist2d(CENTER.GetPositionX(), CENTER.GetPositionY()) >= ARENA_RADIUS - 4.0f)
                    Burn(source, hero, 0.03f, SPELL_ERUPTION_LOG);
        }
        if (!_carnageMs)
            return;
        _carnageMs = _carnageMs > 1000 ? _carnageMs - 1000 : 0;
        uint32 left = _carnageMs / 1000;
        if (left == 120 || left == 60 || left == 30)
            Announce("Carnage: " + std::to_string(left) + " sec left!", true);
        if (left && left <= 30 && left % 3 == 0)
            Cheer(60);
        else if (left && left <= 10)
            Announce(std::to_string(left) + "...", true);
        if (_carnageMs)
            return;
        Announce("Time's up: the arena DETONATES!", true);
        for (Player* player : Living())
        {
            if (Record* record = RecordOf(player))
                record->killer = "Marked for Carnage";
            Unit::Kill(player, player, false);
        }
    }

    void EndMajor(Creature* boss)
    {
        if (boss && !boss->IsAlive() && _major.kind)
            boss->Yell(MAJOR_LINES[_major.kind].Fall, LANG_UNIVERSAL);
        if (boss)
        {
            boss->SetControlled(false, UNIT_STATE_ROOT);
            boss->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        }
        for (ObjectGuid const& guid : _major.edgeMarkers)
            if (GameObject* marker = instance->GetGameObject(guid))
                marker->Delete();
        // The Captain's last: a huge blast, and his guns blow with him.
        if (boss && !boss->IsAlive() && _major.kind == MAJOR_CAPTAIN)
        {
            Look(boss->GetPosition(), SPELL_EXPLOSION_HUGE);
            Shake(boss->GetPosition(), 0.0f, SPELL_SHAKE_MEDIUM);
            uint32 delay = 400;
            for (Position const& muzzle : _major.cannons)
            {
                Queue(muzzle, SPELL_EXPLOSION, delay);
                delay += 250;
            }
        }
        for (ObjectGuid const& guid : _major.props)
            if (GameObject* prop = instance->GetGameObject(guid))
                prop->Delete();
        // His kegs go out with him (their fuses with them).
        for (auto it = _blasts.begin(); it != _blasts.end();)
            if (it->keg)
            {
                DeleteAll(it->markers);
                it = _blasts.erase(it);
            }
            else
                ++it;
        for (ObjectGuid const& guid : _major.kegs)
            if (Creature* keg = instance->GetCreature(guid))
                keg->DespawnOrUnsummon();
        for (Web const& web : _webs)
        {
            for (ObjectGuid const& guid : web.markers)
                if (GameObject* marker = instance->GetGameObject(guid))
                    marker->Delete();
            if (Creature* anchor = instance->GetCreature(web.anchor))
                anchor->DespawnOrUnsummon();
        }
        _webs.clear();
        _major = Major();
        if (_carnageMs)
        {
            _carnageMs = 0;
            Announce("The Carnage is ended: the arena is spared.", true);
        }
        if (boss && !boss->IsAlive())
            Cheer(100);                             // the boss beaten: the crowd roars
    }

public:
    // A major boss's damage taken: half in Trial by Fire, 20% more with its core exposed.
    void MajorTaken(Creature* victim, uint32& damage)
    {
        if (!_major.guid || victim->GetGUID() != _major.guid)
            return;
        float factor = _major.taken * (_runMs < _major.exposedUntil ? EXPOSED_TAKEN : 1.0f);
        damage = uint32(float(damage) * factor);
    }

private:

    // ---- World affixes --------------------------------------------------------------------------------------

    // The set for the wave now: each new one announced (its timer fresh), each gone said so.
    void UpdateAffixes()
    {
        std::vector<uint8> now = AffixesAt(_wave);
        if (now == _affixes)
            return;
        for (uint8 affix : now)
            if (std::find(_affixes.begin(), _affixes.end(), affix) == _affixes.end())
            {
                _affixMs[affix] = Spaced(AffixEvery(affix));
                Announce(std::string("A new affix: ") + AFFIX_NAMES[affix] + ", " + AFFIX_TEXTS[affix] + ".", true);
            }
        for (uint8 affix : _affixes)
            if (std::find(now.begin(), now.end(), affix) == now.end())
            {
                _affixMs.erase(affix);
                Announce(std::string(AFFIX_NAMES[affix]) + " fades.");
            }
        _affixes = now;
        for (auto const& ref : instance->GetPlayers())
            if (Player* player = ref.GetSource())
                SendAffixes(player);
    }

    // X,<affix name>,... : the active affixes, for the tracker.
    void SendAffixes(Player* player)
    {
        std::string text = "X";
        for (uint8 affix : _affixes)
            text += std::string(",") + AFFIX_NAMES[affix];
        SendAddon(player, text);
    }

    static uint32 AffixEvery(uint8 affix)
    {
        switch (affix)
        {
            case AFFIX_MOLTEN_GROUND: return MOLTEN_EVERY_MS;
            case AFFIX_CINDER_WINDS: return WINDS_EVERY_MS;
            case AFFIX_SEARING_BRAND: return BRAND_EVERY_MS;
            case AFFIX_EMBERFALL: return EMBER_EVERY_MS;
            case AFFIX_MENDING_EMBERS: return MENDING_EVERY_MS;
            case AFFIX_ASHEN_ECHO: return ECHO_EVERY_MS;
            case AFFIX_SHACKLING_CHAINS: return CHAINS_EVERY_MS;
            case AFFIX_BURNING_PURSUIT: return PURSUIT_EVERY_MS;
            case AFFIX_CRACKED_EARTH: return CRACKED_EVERY_MS;
            default: return 0;                          // a wave's start or a death sets it off, not a timer
        }
    }

    // A major danger's resolution, ms from now, moved later until no other resolves within 3 sec of it.
    uint32 Spaced(uint32 ms) const
    {
        for (bool moved = true; moved;)
        {
            moved = false;
            for (Blast const& blast : _blasts)
                if ((blast.ms > ms ? blast.ms - ms : ms - blast.ms) < MAJOR_SPACING_MS)
                {
                    ms = blast.ms + MAJOR_SPACING_MS;
                    moved = true;
                }
        }
        return ms;
    }

    // The Firepit's own hand: an invisible source the affixes burn the Heroes with.
    Unit* AffixSource()
    {
        if (Creature* source = instance->GetCreature(_affixSource))
            return source;
        TempSummon* source = instance->SummonCreature(NPC_ERUPTION, CENTER);
        if (!source)
            return nullptr;
        source->SetFaction(35);
        _affixSource = source->GetGUID();
        return source;
    }

    // A share of the Hero's maximum health as Fire damage, under the spell's name in the combat log.
    void Burn(Unit* attacker, Player* hero, float share, uint32 logSpell)
    {
        if (!attacker || !hero->IsAlive() || IsProtected(hero))
            return;
        SpellInfo const* spell = sSpellMgr->GetSpellInfo(logSpell);
        DamageInfo damage(attacker, hero, uint32(float(hero->GetMaxHealth()) * share), spell,
            SPELL_SCHOOL_MASK_FIRE, SPELL_DIRECT_DAMAGE);
        Unit::CalcAbsorbResist(damage);
        attacker->SendSpellNonMeleeDamageLog(hero, spell, damage.GetDamage(), SPELL_SCHOOL_MASK_FIRE,
            damage.GetAbsorb(), damage.GetResist(), false, 0);
        Unit::DealDamage(attacker, hero, damage.GetDamage(), nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_FIRE,
            spell, false);
    }

    void Affixes(uint32 diff)
    {
        if (_phase != Phase::RUNNING)
            return;
        for (uint8 affix : _affixes)
        {
            if (!AffixEvery(affix))
                continue;
            uint32& ms = _affixMs[affix];
            if (ms > diff)
            {
                ms -= diff;
                continue;
            }
            ms = AffixEvery(affix);
            switch (affix)
            {
                case AFFIX_MOLTEN_GROUND: MoltenGround(); break;
                case AFFIX_CINDER_WINDS: CinderWinds(); break;
                case AFFIX_SEARING_BRAND: SearingBrand(); break;
                case AFFIX_EMBERFALL: Emberfall(); break;
                case AFFIX_MENDING_EMBERS: MendingEmbers(); break;
                case AFFIX_ASHEN_ECHO: AshenEcho(); break;
                case AFFIX_SHACKLING_CHAINS: ShacklingChains(); break;
                case AFFIX_BURNING_PURSUIT: BurningPursuit(); break;
                case AFFIX_CRACKED_EARTH: CrackedEarth(); break;
                default: break;
            }
        }
        EchoesTick(diff);
        _secondMs += diff;
        if (_secondMs < 1000)
            return;
        _secondMs -= 1000;
        PoolsTick();
        WindsTick();
        MajorSecond();
        BrandsTick();
        MendingTick();
        ChainsTick();
        PursuitTick();
        CracksTick();
        EnemyAffixTick();
    }

    bool Active(uint8 affix) const
    {
        return std::find(_affixes.begin(), _affixes.end(), affix) != _affixes.end();
    }

    Enemy* EnemyOf(Creature* creature) const
    {
        return creature ? creature->CustomData.Get<Enemy>("titan_firepit") : nullptr;
    }

    // The living non-boss enemies on the field.
    std::vector<Creature*> Fighters() const
    {
        std::vector<Creature*> fighters;
        for (ObjectGuid const& guid : _enemies)
            if (Creature* enemy = instance->GetCreature(guid); enemy && enemy->IsAlive())
                if (Enemy* own = EnemyOf(enemy); own && !own->boss)
                    fighters.push_back(enemy);
        return fighters;
    }

    // A normal wave's start: the affixes' chosen among its enemies (each takes effect as it enters).
    void AssignRoles(std::size_t first)
    {
        std::vector<std::size_t> picks;
        for (std::size_t i = first; i < _queue.size(); ++i)
            if (!_queue[i].boss)
                picks.push_back(i);
        Acore::Containers::RandomShuffle(picks);
        std::size_t used = 0;
        auto take = [&](uint8 role)
        {
            if (used < picks.size())
                _queue[picks[used++]].role |= role;
        };
        if (Active(AFFIX_CINDER_SHIELD))
            take(SPAWN_CINDER_SHIELD);
        if (Active(AFFIX_FLAMEBOUND) && picks.size() - used >= 2)
        {
            _flameboundWaiting.clear();
            take(SPAWN_FLAMEBOUND);
            take(SPAWN_FLAMEBOUND);
        }
    }

    // An enemy entering with an affix's role: Cinder Shield, Flamebound.
    void ApplyRole(Creature* enemy, uint8 role)
    {
        if (role & SPAWN_CINDER_SHIELD)
            if (Aura* aura = enemy->AddAura(SPELL_CINDER_SHIELD, enemy))
                if (AuraEffect* effect = aura->GetEffect(EFFECT_0))
                    effect->ChangeAmount(int32(float(enemy->GetMaxHealth()) * CINDER_SHIELD_SHARE));
        if (role & SPAWN_FLAMEBOUND)
        {
            _flameboundWaiting.push_back(enemy->GetGUID());
            if (_flameboundWaiting.size() == 2)
            {
                Creature* a = instance->GetCreature(_flameboundWaiting[0]);
                Creature* b = instance->GetCreature(_flameboundWaiting[1]);
                if (a && b)
                {
                    EnemyOf(a)->partner = b->GetGUID();
                    EnemyOf(b)->partner = a->GetGUID();
                    a->AddAura(SPELL_FLAMEBOUND, a);
                    b->AddAura(SPELL_FLAMEBOUND, b);
                    Announce(a->GetName() + " and " + b->GetName() + " are Flamebound: keep them apart!");
                }
                _flameboundWaiting.clear();
            }
        }
    }

    // An enemy fallen: Bolstering and Vengeful Flame on the enemies near it; Volatile Remains.
    void EnemyFell(Creature* enemy)
    {
        Enemy* own = EnemyOf(enemy);
        if (!own || own->boss)
            return;
        if (Active(AFFIX_VOLATILE_REMAINS) && _volatile < VOLATILE_MOST)
        {
            ++_volatile;
            Blast blast;
            blast.at = enemy->GetPosition();
            blast.radius = VOLATILE_RADIUS;
            blast.share = VOLATILE_SHARE;
            blast.ms = VOLATILE_DELAY_MS;
            blast.volatileEmber = true;
            blast.pool = true;
            blast.poolRadius = VOLATILE_RADIUS - 2.0f;
            Telegraph(blast);
            _blasts.push_back(blast);
        }
        bool bolstering = Active(AFFIX_BOLSTERING), vengeful = Active(AFFIX_VENGEFUL_FLAME);
        if (!bolstering && !vengeful)
            return;
        for (Creature* other : Fighters())
        {
            if (other == enemy)
                continue;
            float apart = enemy->GetExactDist2d(other);
            Enemy* near = EnemyOf(other);
            if (bolstering && apart <= BOLSTER_RANGE && near->bolster < BOLSTER_MOST)
            {
                // 12% more health (and as much healed) and 12% more damage a stack.
                ++near->bolster;
                uint32 more = uint32(float(other->GetMaxHealth()) * BOLSTER_MORE);
                other->SetMaxHealth(other->GetMaxHealth() + more);
                other->ModifyHealth(int32(more));
                if (Aura* aura = other->GetAura(SPELL_BOLSTERED))
                {
                    aura->SetStackAmount(near->bolster);
                    aura->RefreshDuration();
                }
                else if (Aura* fresh = other->AddAura(SPELL_BOLSTERED, other))
                    fresh->SetStackAmount(near->bolster);
            }
            if (vengeful && apart <= VENGEFUL_RANGE)
            {
                near->vengeful = uint8(std::min<uint32>(VENGEFUL_MOST, near->vengeful + 1u));
                near->vengefulUntil = _runMs + VENGEFUL_MS;
                if (Aura* aura = other->GetAura(SPELL_VENGEFUL_FLAME))
                {
                    aura->SetStackAmount(near->vengeful);
                    aura->RefreshDuration();
                }
                else if (Aura* fresh = other->AddAura(SPELL_VENGEFUL_FLAME, other))
                    fresh->SetStackAmount(near->vengeful);
            }
        }
    }

    // Each second: each enemy's affix damage (Cinder Shield, Vengeful Flame, Flamebound, Bolstering, an Ashen
    // Echo's empowerment, Burning Pursuit); a Cinder Shield's burst as it breaks; a hunter's trail of fire.
    void EnemyAffixTick()
    {
        bool solo = _groupSize == 1;
        for (Creature* enemy : Fighters())
        {
            Enemy* own = EnemyOf(enemy);
            float more = 1.0f + BOLSTER_MORE * float(own->bolster);
            bool shielded = enemy->HasAura(SPELL_CINDER_SHIELD);
            if (shielded)
                more *= CINDER_SHIELD_DAMAGE;
            else if (own->shielded)
            {
                Blast burst = Circle(enemy->GetPosition(), CINDER_BURST_RADIUS, CINDER_BURST_SHARE);
                burst.ms = CINDER_BURST_MS;
                burst.source = enemy->GetGUID();
                Telegraph(burst);
                _blasts.push_back(burst);
                Announce(enemy->GetName() + "'s Cinder Shield breaks: it bursts, step away!");
            }
            own->shielded = shielded;
            if (own->vengeful && _runMs >= own->vengefulUntil)
            {
                own->vengeful = 0;
                enemy->RemoveAurasDueToSpell(SPELL_VENGEFUL_FLAME);
            }
            more *= 1.0f + VENGEFUL_DAMAGE * float(own->vengeful);
            if (enemy->HasAura(SPELL_ASHEN_EMPOWERMENT))
                more *= ECHO_DAMAGE;
            if (_pursuers.count(enemy->GetGUID()))
            {
                more *= PURSUIT_DAMAGE;
                own->trailMs += 1000;
                if (own->trailMs >= TRAIL_EVERY_MS)
                {
                    own->trailMs = 0;
                    LayPool(enemy->GetPosition(), TRAIL_RADIUS, POOL_SHARE, TRAIL_MS);
                }
            }
            if (own->partner)
            {
                Creature* partner = instance->GetCreature(own->partner);
                if (!partner || !partner->IsAlive())
                {
                    own->partner.Clear();
                    enemy->RemoveAurasDueToSpell(SPELL_FLAMEBOUND);
                }
                else if (enemy->GetExactDist2d(partner) <= FLAMEBOUND_RANGE)
                    more *= solo ? FLAMEBOUND_SOLO : FLAMEBOUND_DAMAGE;
            }
            own->affix = more;
        }
    }

public:
    // Flamebound: two partners within 10 yd of each other take half damage (keep them apart).
    void FlameboundTaken(Creature* victim, uint32& damage)
    {
        Enemy* own = EnemyOf(victim);
        if (!own || own->boss || !own->partner || _groupSize == 1)
            return;
        Creature* partner = instance->GetCreature(own->partner);
        if (partner && partner->IsAlive() && victim->GetExactDist2d(partner) <= FLAMEBOUND_RANGE)
            damage = uint32(float(damage) * FLAMEBOUND_TAKEN);
    }

private:
    void MendingEmbers()
    {
        std::vector<Creature*> fighters = Fighters();
        if (fighters.empty())
            return;
        Creature* mender = fighters[urand(0, uint32(fighters.size()) - 1)];
        _mender = mender->GetGUID();
        _mendLeft = MENDING_MS;
        _mendTick = 0;
        if (Aura* aura = mender->AddAura(SPELL_MENDING_EMBERS, mender))
        {
            aura->SetMaxDuration(int32(MENDING_MS));
            aura->SetDuration(int32(MENDING_MS));
        }
        Announce(mender->GetName() + " is Mending Embers: kill it before it heals every enemy around it!", true);
    }

    void MendingTick()
    {
        if (!_mender)
            return;
        Creature* mender = instance->GetCreature(_mender);
        _mendLeft = _mendLeft > 1000 ? _mendLeft - 1000 : 0;
        if (!mender || !mender->IsAlive() || !_mendLeft)
        {
            if (mender)
                mender->RemoveAurasDueToSpell(SPELL_MENDING_EMBERS);
            _mender.Clear();
            return;
        }
        _mendTick += 1000;
        if (_mendTick < MENDING_TICK_MS)
            return;
        _mendTick = 0;
        // Every other enemy within 20 yd, 5% of its health.
        for (ObjectGuid const& guid : _enemies)
            if (Creature* other = instance->GetCreature(guid); other && other != mender && other->IsAlive()
                && other->GetHealthPct() < 100.0f && mender->GetExactDist2d(other) <= MENDING_RANGE)
            {
                uint32 amount = uint32(other->CountPctFromMaxHealth(int32(MENDING_PCT)));
                Unit::DealHeal(mender, other, amount);
                HealInfo heal(mender, other, amount, sSpellMgr->GetSpellInfo(SPELL_MENDING_EMBERS),
                    SPELL_SCHOOL_MASK_FIRE);
                mender->SendHealSpellLog(heal);
            }
    }

    void AshenEcho()
    {
        std::vector<ObjectGuid> living;
        for (ObjectGuid const& guid : _enemies)
            if (Creature* enemy = instance->GetCreature(guid); enemy && enemy->IsAlive())
                living.push_back(guid);
        if (living.empty())
            return;
        Creature* target = instance->GetCreature(living[urand(0, uint32(living.size()) - 1)]);
        float angle = frand(0.0f, 2.0f * float(M_PI));
        float x = CENTER.GetPositionX() + SPAWN_RADIUS_MIN * std::cos(angle);
        float y = CENTER.GetPositionY() + SPAWN_RADIUS_MIN * std::sin(angle);
        TempSummon* echo = instance->SummonCreature(NPC_ASHEN_ECHO, Position(x, y, FloorAt(x, y), angle + float(M_PI)));
        if (!echo || !target)
            return;
        echo->SetFaction(ENEMY_FACTION);
        echo->SetReactState(REACT_PASSIVE);
        echo->SetControlled(true, UNIT_STATE_ROOT);
        echo->SetFacingToObject(target);
        echo->SetGuidValue(UNIT_FIELD_CHANNEL_OBJECT, target->GetGUID());
        echo->SetUInt32Value(UNIT_CHANNEL_SPELL, SPELL_ECHO_CHANNEL);
        _echoes.push_back({ echo->GetGUID(), target->GetGUID(), ECHO_CHANNEL_MS });
        Announce("An Ashen Echo channels into " + target->GetName() + ": kill or stun it!", true);
    }

    // Each update: an Echo stunned, silenced, feared or slain is broken; its channel done, its target empowered.
    void EchoesTick(uint32 diff)
    {
        for (std::size_t i = 0; i < _echoes.size();)
        {
            Echo& echo = _echoes[i];
            Creature* source = instance->GetCreature(echo.echo);
            Creature* target = instance->GetCreature(echo.target);
            bool broken = !source || !source->IsAlive() || !target || !target->IsAlive()
                || source->HasAuraType(SPELL_AURA_MOD_STUN) || source->HasAuraType(SPELL_AURA_MOD_SILENCE)
                || source->HasAuraType(SPELL_AURA_MOD_FEAR) || source->HasAuraType(SPELL_AURA_MOD_CONFUSE);
            echo.left = echo.left > diff ? echo.left - diff : 0;
            if (!broken && !echo.left)
                if (Aura* aura = target->AddAura(SPELL_ASHEN_EMPOWERMENT, target))
                {
                    aura->SetMaxDuration(int32(ECHO_BUFF_MS));
                    aura->SetDuration(int32(ECHO_BUFF_MS));
                    Announce(target->GetName() + " is empowered by the Ashen Echo!");
                }
            if (broken || !echo.left)
            {
                if (source)
                    source->DespawnOrUnsummon(Milliseconds(broken && source->IsAlive() ? 0 : CORPSE_MS));
                _echoes.erase(_echoes.begin() + i);
                continue;
            }
            ++i;
        }
    }

    void DismissEchoes()
    {
        for (Echo const& echo : _echoes)
            if (Creature* source = instance->GetCreature(echo.echo))
                source->DespawnOrUnsummon();
        _echoes.clear();
    }

    // Shackling Chains: two Heroes (two pairs among 4 or more); solo, the Hero and their spot (an arcane ring).
    void ShacklingChains()
    {
        std::vector<Player*> heroes = Living();
        if (heroes.empty())
            return;
        Acore::Containers::RandomShuffle(heroes);
        if (heroes.size() == 1)
        {
            Chain chain;
            chain.a = heroes[0]->GetGUID();
            chain.left = CHAINS_MS;
            chain.point = heroes[0]->GetPosition();
            heroes[0]->AddAura(SPELL_SHACKLING_CHAINS, heroes[0]);
            Zone(chain.markers, DECAL_CIRCLE, chain.point, 0.0f, CHAINS_SOLO, DECAL_ARCANE, STYLE_RUNIC);
            _chains.push_back(chain);
            Announce("Shackling Chains bind you to this spot: stay within 6 yards of it for 3 sec!", true);
            return;
        }
        std::size_t pairs = heroes.size() >= 4 ? 2 : 1;
        for (std::size_t i = 0; i < pairs; ++i)
        {
            Chain chain;
            chain.a = heroes[i * 2]->GetGUID();
            chain.b = heroes[i * 2 + 1]->GetGUID();
            chain.left = CHAINS_MS;
            heroes[i * 2]->AddAura(SPELL_SHACKLING_CHAINS, heroes[i * 2]);
            heroes[i * 2 + 1]->AddAura(SPELL_SHACKLING_CHAINS, heroes[i * 2 + 1]);
            _chains.push_back(chain);
            Announce("Shackling Chains bind " + heroes[i * 2]->GetName() + " and " + heroes[i * 2 + 1]->GetName()
                + ": get within 12 yards of each other for 3 sec!", true);
        }
    }

    void ChainsTick()
    {
        Unit* source = _chains.empty() ? nullptr : AffixSource();
        for (std::size_t i = 0; i < _chains.size();)
        {
            Chain& chain = _chains[i];
            Player* a = ObjectAccessor::GetPlayer(instance, chain.a);
            Player* b = chain.b ? ObjectAccessor::GetPlayer(instance, chain.b) : nullptr;
            chain.left = chain.left > 1000 ? chain.left - 1000 : 0;
            bool done = !a || !a->IsAlive() || (chain.b && (!b || !b->IsAlive())) || !chain.left;
            if (!done)
            {
                bool close = b ? a->GetExactDist2d(b) <= CHAINS_APART
                    : a->GetExactDist2d(chain.point.GetPositionX(), chain.point.GetPositionY()) <= CHAINS_SOLO;
                chain.togetherMs = close ? chain.togetherMs + 1000 : 0;
                if (chain.togetherMs >= CHAINS_BREAK_MS)
                    done = true;
                else if (!close)
                {
                    Burn(source, a, CHAINS_SHARE, SPELL_SHACKLING_CHAINS);
                    if (b)
                        Burn(source, b, CHAINS_SHARE, SPELL_SHACKLING_CHAINS);
                }
            }
            if (done)
            {
                for (Player* hero : { a, b })
                    if (hero)
                        hero->RemoveAurasDueToSpell(SPELL_SHACKLING_CHAINS);
                DeleteAll(chain.markers);
                _chains.erase(_chains.begin() + i);
                continue;
            }
            ++i;
        }
    }

    void BurningPursuit()
    {
        std::vector<Creature*> fighters = Fighters();
        std::vector<Player*> heroes = Living();
        if (fighters.empty() || heroes.empty())
            return;
        Creature* hunter = fighters[urand(0, uint32(fighters.size()) - 1)];
        Player* prey = heroes[urand(0, uint32(heroes.size()) - 1)];
        hunter->GetThreatMgr().FixateTarget(prey);
        if (hunter->AI())
            hunter->AI()->AttackStart(prey);
        if (Aura* aura = hunter->AddAura(SPELL_BURNING_PURSUIT, hunter))
        {
            aura->SetMaxDuration(int32(PURSUIT_MS));
            aura->SetDuration(int32(PURSUIT_MS));
        }
        _pursuers[hunter->GetGUID()] = _runMs + PURSUIT_MS;
        Announce(hunter->GetName() + " hunts " + prey->GetName() + "!", true);
    }

    void PursuitTick()
    {
        for (auto itr = _pursuers.begin(); itr != _pursuers.end();)
        {
            Creature* hunter = instance->GetCreature(itr->first);
            if (hunter && _runMs < itr->second && hunter->IsAlive())
            {
                ++itr;
                continue;
            }
            if (hunter)
            {
                hunter->GetThreatMgr().ClearFixate();
                hunter->RemoveAurasDueToSpell(SPELL_BURNING_PURSUIT);
            }
            itr = _pursuers.erase(itr);
        }
    }

    void CrackedEarth()
    {
        float angle = frand(0.0f, 2.0f * float(M_PI));
        float radius = frand(ARRIVAL_RADIUS + CRACKED_RADIUS, ARENA_RADIUS - CRACKED_RADIUS);
        float x = CENTER.GetPositionX() + radius * std::cos(angle);
        float y = CENTER.GetPositionY() + radius * std::sin(angle);
        Crack crack;
        crack.at = Position(x, y, FloorAt(x, y));
        _cracks.push_back(crack);
        Blast blast;
        blast.at = crack.at;
        blast.radius = CRACKED_RADIUS;
        blast.share = CRACKED_SHARE;
        blast.ms = Spaced(CRACKED_DELAY_MS);
        blast.crack = int32(_cracks.size() - 1);
        Telegraph(blast);
        _blasts.push_back(blast);
        Announce("Cracked Earth: the ground splits, get out of the marked ring! It freezes over for 25 sec.", true);
    }

    // The cracked ground after it erupts: 30% slower on it for 10 sec; then gone.
    void CracksTick()
    {
        for (Crack& crack : _cracks)
        {
            if (!crack.until || crack.done)
                continue;
            if (_runMs >= crack.until)
            {
                crack.done = true;
                for (ObjectGuid const& guid : crack.markers)
                    if (GameObject* marker = instance->GetGameObject(guid))
                        marker->Delete();
                continue;
            }
            for (Player* hero : Living())
                if (hero->GetExactDist2d(crack.at.GetPositionX(), crack.at.GetPositionY()) <= CRACKED_RADIUS)
                    if (Aura* aura = hero->AddAura(SPELL_CRACKED_EARTH, hero))
                        if (AuraEffect* slow = aura->GetEffect(EFFECT_0))
                            slow->ChangeAmount(CRACKED_SLOW);
        }
        if (std::all_of(_cracks.begin(), _cracks.end(), [](Crack const& c) { return c.done; })
            && std::none_of(_blasts.begin(), _blasts.end(), [](Blast const& b) { return b.crack >= 0; }))
            _cracks.clear();
    }

    void MoltenGround()
    {
        std::vector<Player*> heroes = Living();
        Acore::Containers::RandomShuffle(heroes);
        std::stable_sort(heroes.begin(), heroes.end(), [this](Player* a, Player* b)
        {
            return !_moltenLast.count(a->GetGUID()) && _moltenLast.count(b->GetGUID());
        });
        if (heroes.size() > MOLTEN_TARGETS)
            heroes.resize(MOLTEN_TARGETS);
        _moltenLast.clear();
        uint32 ms = Spaced(MOLTEN_DELAY_MS);
        for (Player* hero : heroes)
        {
            _moltenLast.insert(hero->GetGUID());
            Blast blast;
            blast.at = hero->GetPosition();
            blast.radius = MOLTEN_RADIUS;
            blast.share = MOLTEN_SHARE;
            blast.ms = ms;
            blast.pool = true;
            Telegraph(blast);
            _blasts.push_back(blast);
        }
        if (!heroes.empty())
            Announce("Molten Ground: the floor erupts under you, move!");
    }

    void PoolsTick()
    {
        Unit* source = _pools.empty() ? nullptr : AffixSource();
        for (std::size_t i = 0; i < _pools.size();)
        {
            Pool const& pool = _pools[i];
            if (_runMs >= pool.until)
            {
                DeleteAll(pool.markers);
                _pools.erase(_pools.begin() + i);
                continue;
            }
            for (Player* hero : Living())
                if (hero->GetExactDist2d(pool.at.GetPositionX(), pool.at.GetPositionY()) <= pool.radius
                    && std::abs(hero->GetPositionZ() - pool.at.GetPositionZ()) <= 6.0f)
                    Burn(source, hero, pool.share, SPELL_ERUPTION_LOG);
            ++i;
        }
    }

    // Cinder Winds: a lane across the arena (a third of it off the middle at most), blasted after its warning; it
    // slows those in it and leaves a wall of fire there (Detonate).
    void CinderWinds()
    {
        float angle = frand(0.0f, 2.0f * float(M_PI));
        float offset = frand(-WINDS_OFFSET, WINDS_OFFSET);
        float across = angle + float(M_PI) / 2.0f;
        float cx = CENTER.GetPositionX() + offset * std::cos(across), cy = CENTER.GetPositionY() + offset
            * std::sin(across);
        float sx = cx - ARENA_RADIUS * std::cos(angle), sy = cy - ARENA_RADIUS * std::sin(angle);
        Blast blast = Circle(Position(sx, sy, CENTER.GetPositionZ()), WINDS_HALF_WIDTH, WINDS_SHARE);
        blast.shape = SHAPE_LINE;
        blast.facing = angle;
        blast.length = ARENA_RADIUS * 2.0f;
        blast.wind = true;
        blast.style = STYLE_SWIRL;
        blast.log = SPELL_CINDER_WINDS;
        blast.ms = Spaced(WINDS_WARNING_MS);
        Telegraph(blast);
        _blasts.push_back(blast);
        Announce("Cinder Winds: a burning wind gathers, get out of its lane!", true);
    }

    // The walls of fire: 5% a second to those in them, until they burn out.
    void WindsTick()
    {
        Unit* source = _winds.empty() ? nullptr : AffixSource();
        for (std::size_t i = 0; i < _winds.size();)
        {
            Wind const& wind = _winds[i];
            if (_runMs >= wind.until)
            {
                DeleteAll(wind.markers);
                _winds.erase(_winds.begin() + i);
                continue;
            }
            float c = std::cos(wind.angle), sn = std::sin(wind.angle);
            for (Player* hero : Living())
            {
                float dx = hero->GetPositionX() - wind.from.GetPositionX();
                float dy = hero->GetPositionY() - wind.from.GetPositionY();
                if (std::abs(-dx * sn + dy * c) <= WINDS_HALF_WIDTH)
                    Burn(source, hero, FIREWALL_SHARE, SPELL_CINDER_WINDS);
            }
            ++i;
        }
    }

    // A Brand on the Hero (none already there): 12 sec, burning harder each second.
    void BrandHero(Player* hero, bool spread)
    {
        if (std::any_of(_brands.begin(), _brands.end(), [hero](Brand const& b) { return b.hero == hero->GetGUID(); }))
            return;
        Brand brand;
        brand.hero = hero->GetGUID();
        brand.at = hero->GetPosition();
        brand.left = BRAND_MS;
        _brands.push_back(brand);
        if (Aura* aura = hero->AddAura(SPELL_SEARING_BRAND, hero))
        {
            aura->SetMaxDuration(int32(BRAND_MS));
            aura->SetDuration(int32(BRAND_MS));
        }
        Announce("Searing Brand " + std::string(spread ? "spreads to " : "on ") + hero->GetName()
            + ": get 8 yards away from everyone for 3 sec!", true);
        ChatHandler(hero->GetSession()).SendSysMessage(Living().size() > 1
            ? "|cffff7f27Searing Brand:|r stay 8 yards from every other Hero for 3 sec to put it out. It spreads "
              "to anyone near you and burns harder every second."
            : "|cffff7f27Searing Brand:|r move 8 yards from where it struck and stay away 3 sec to put it out. It "
              "burns harder every second.");
    }

    // Searing Brand: one Hero (two among 4 or more), not one already branded.
    void SearingBrand()
    {
        std::vector<Player*> heroes;
        for (Player* hero : Living())
            if (std::none_of(_brands.begin(), _brands.end(),
                [hero](Brand const& b) { return b.hero == hero->GetGUID(); }))
                heroes.push_back(hero);
        Acore::Containers::RandomShuffle(heroes);
        std::size_t count = Living().size() >= 4 ? 2 : 1;
        for (std::size_t i = 0; i < heroes.size() && i < count; ++i)
            BrandHero(heroes[i], false);
    }

    // Each second: a Brand burns (3%, 1% more each second, 8% at most) and spreads to every Hero within 8 yd; 3 sec
    // 8 yd from every other Hero (solo: from where it struck) puts it out.
    void BrandsTick()
    {
        Unit* source = _brands.empty() ? nullptr : AffixSource();
        std::vector<Player*> living = Living();
        std::vector<Player*> spreads;
        for (std::size_t i = 0; i < _brands.size();)
        {
            Brand& brand = _brands[i];
            Player* hero = ObjectAccessor::GetPlayer(instance, brand.hero);
            bool done = !hero || !hero->IsAlive();
            if (!done)
            {
                bool apart = true;
                if (living.size() > 1)
                {
                    for (Player* other : living)
                        if (other != hero && hero->GetExactDist2d(other) < BRAND_APART)
                        {
                            apart = false;
                            spreads.push_back(other);
                        }
                }
                else
                    apart = hero->GetExactDist2d(brand.at.GetPositionX(), brand.at.GetPositionY()) >= BRAND_APART;
                brand.apartMs = apart ? brand.apartMs + 1000 : 0;
                if (brand.apartMs >= BRAND_CLEAR_MS)
                {
                    done = true;
                    ChatHandler(hero->GetSession()).SendSysMessage("|cffff7f27Searing Brand|r put out.");
                }
                else
                {
                    Burn(source, hero, std::min(BRAND_MOST, BRAND_SHARE + BRAND_RAMP * float(brand.burned)),
                        SPELL_SEARING_BRAND);
                    ++brand.burned;
                    brand.left = brand.left > 1000 ? brand.left - 1000 : 0;
                    done = !brand.left;
                }
            }
            if (done)
            {
                if (hero)
                    hero->RemoveAurasDueToSpell(SPELL_SEARING_BRAND);
                _brands.erase(_brands.begin() + i);
                continue;
            }
            ++i;
        }
        for (Player* hero : spreads)
            BrandHero(hero, true);
    }

    // Emberfall: under every Hero and 5 spots apart across the arena; each leaves a pool.
    void Emberfall()
    {
        std::vector<Position> spots;
        for (Player* hero : Living())
            spots.push_back(hero->GetPosition());
        std::size_t under = spots.size();
        for (uint32 tries = 0; spots.size() < under + EMBER_SPOTS && tries < 60; ++tries)
        {
            float angle = frand(0.0f, 2.0f * float(M_PI)), radius = frand(ARRIVAL_RADIUS + EMBER_RADIUS, EMBER_REACH);
            float x = CENTER.GetPositionX() + radius * std::cos(angle);
            float y = CENTER.GetPositionY() + radius * std::sin(angle);
            Position at(x, y, FloorAt(x, y));
            if (std::all_of(spots.begin(), spots.end(), [&at](Position const& p) { return p.GetExactDist2d(&at)
                >= EMBER_APART; }))
                spots.push_back(at);
        }
        uint32 ms = Spaced(EMBER_DELAY_MS);
        for (Position const& at : spots)
        {
            Blast blast;
            blast.at = at;
            blast.radius = EMBER_RADIUS;
            blast.share = EMBER_SHARE;
            blast.ms = ms;
            blast.pool = true;
            blast.poolRadius = EMBER_RADIUS - 1.0f;
            Telegraph(blast);
            _blasts.push_back(blast);
        }
        Announce("Emberfall: embers rain down on everyone, stay out of the circles!");
    }

    void ClearAffixes()
    {
        for (Pool const& pool : _pools)
            DeleteAll(pool.markers);
        for (Wind const& wind : _winds)
            for (ObjectGuid const& guid : wind.markers)
                if (GameObject* marker = instance->GetGameObject(guid))
                    marker->Delete();
        for (Brand const& brand : _brands)
            if (Player* hero = ObjectAccessor::GetPlayer(instance, brand.hero))
                hero->RemoveAurasDueToSpell(SPELL_SEARING_BRAND);
        for (Chain const& chain : _chains)
        {
            for (ObjectGuid const& guid : { chain.a, chain.b })
                if (Player* hero = guid ? ObjectAccessor::GetPlayer(instance, guid) : nullptr)
                    hero->RemoveAurasDueToSpell(SPELL_SHACKLING_CHAINS);
            DeleteAll(chain.markers);
        }
        for (Crack const& crack : _cracks)
            for (ObjectGuid const& guid : crack.markers)
                if (GameObject* marker = instance->GetGameObject(guid))
                    marker->Delete();
        DismissEchoes();
        _chains.clear();
        _cracks.clear();
        _pursuers.clear();
        _flameboundWaiting.clear();
        _mender.Clear();
        _volatile = 0;
        _pools.clear();
        _winds.clear();
        _brands.clear();
        _affixes.clear();
        _affixMs.clear();
        if (Creature* source = instance->GetCreature(_affixSource))
            source->DespawnOrUnsummon();
    }

    void ClearMechanics()
    {
        ClearAffixes();
        EndMajor(instance->GetCreature(_major.guid));
        _fx.clear();
        _carnageMs = 0;
        for (Blast const& blast : _blasts)
        {
            if (GameObject* marker = instance->GetGameObject(blast.marker))
                marker->Delete();
            for (ObjectGuid const& guid : blast.markers)
                if (GameObject* marker = instance->GetGameObject(guid))
                    marker->Delete();
            if (Creature* boss = instance->GetCreature(blast.source))
                boss->SetControlled(false, UNIT_STATE_ROOT);
        }
        _blasts.clear();
        _bosses.clear();
    }

    // At the nearest living Hero.
    // The run's tank: its first living Hero in the tank role, else none.
    Player* Tank()
    {
        for (Player* player : Living())
            if (Wildcard::GetRole(player) == Wildcard::ROLE_TANK)
                return player;
        return nullptr;
    }

    // Tank priority: the tank's threat on the enemy raised to the greatest of its own, 500, or 150% of the highest
    // anyone else has (a 50% lead, not a lock: taunts and threat work on as ever).
    void Prioritize(Creature* enemy, Player* tank)
    {
        float highest = 0.0f;
        for (ThreatReference const* ref : enemy->GetThreatMgr().GetUnsortedThreatList())
            if (ref->GetVictim() != tank)
                highest = std::max(highest, ref->GetThreat());
        float now = enemy->GetThreatMgr().GetThreat(tank);
        float wanted = std::max({ now, TANK_THREAT, highest * TANK_LEAD });
        if (wanted > now)
            enemy->GetThreatMgr().AddThreat(tank, wanted - now, nullptr, true, true);
    }

    // An enemy into the fight: at the tank (Prioritize), else the nearest Hero.
    void Engage(Creature* enemy)
    {
        if (!enemy->AI())
            return;
        Player* target = Tank();
        if (!target)
            for (Player* player : Living())
                if (!target || enemy->GetExactDist(player) < enemy->GetExactDist(target))
                    target = player;
        if (!target)
            return;
        enemy->SetInCombatWithZone();
        if (Wildcard::GetRole(target) == Wildcard::ROLE_TANK)
            Prioritize(enemy, target);
        enemy->AI()->AttackStart(target);
    }

    void Run(uint32 diff)
    {
        _runMs += diff;
        for (Spawn& spawn : _queue)
            spawn.dueMs = spawn.dueMs > diff ? spawn.dueMs - diff : 0;
        while (!_queue.empty() && !_queue.front().dueMs)
        {
            SpawnOne(_queue.front());
            _queue.erase(_queue.begin());
        }
        for (std::size_t i = 0; i < _queue.size();)
        {
            if (!_queue[i].dueMs)
            {
                SpawnOne(_queue[i]);
                _queue.erase(_queue.begin() + i);
            }
            else
                ++i;
        }

        Mechanics(diff);
        MajorTick(diff);
        Affixes(diff);
        CheckLoot();

        _waveMs = _waveMs > diff ? _waveMs - diff : 0;
        _tickMs += diff;
        if (_tickMs < TICK_MS && _waveMs)
            return;
        _tickMs = 0;

        // The fallen go (no loot, no corpses piling up); the rest keep at it, inside the ring.
        std::vector<ObjectGuid> alive;
        for (ObjectGuid const& guid : _enemies)
        {
            Creature* enemy = instance->GetCreature(guid);
            if (!enemy)
                continue;
            if (!enemy->IsAlive())
            {
                EnemyFell(enemy);
                enemy->DespawnOrUnsummon(Milliseconds(CORPSE_MS));
                continue;
            }
            if (enemy->GetExactDist2d(CENTER.GetPositionX(), CENTER.GetPositionY()) > ARENA_RADIUS
                || enemy->GetPositionZ() > CENTER.GetPositionZ() + ARENA_HEIGHT)
            {
                float angle = enemy->GetAngle(CENTER.GetPositionX(), CENTER.GetPositionY()) + float(M_PI);
                float x = CENTER.GetPositionX() + SPAWN_RADIUS_MIN * std::cos(angle);
                float y = CENTER.GetPositionY() + SPAWN_RADIUS_MIN * std::sin(angle);
                enemy->NearTeleportTo(x, y, FloorAt(x, y), angle + float(M_PI));
            }
            if (!enemy->GetVictim() || !enemy->GetVictim()->IsAlive())
                Engage(enemy);
            alive.push_back(guid);
        }
        _enemies = std::move(alive);

        // Reinforcements: one enters 3 sec after room opens.
        if (!_reinforcements.empty() && _enemies.size() < MostAlive())
        {
            _reinforceMs = _reinforceMs > TICK_MS ? _reinforceMs - TICK_MS : 0;
            if (!_reinforceMs)
            {
                Spawn next = _reinforcements.front();
                _reinforcements.erase(_reinforcements.begin());
                SpawnOne(next);
                _reinforceMs = REINFORCE_MS;
            }
        }
        else
            _reinforceMs = REINFORCE_MS;

        // Pressure: the cap full 30 sec running, 5% more damage a stack (4 at most); 10 sec under it clears it.
        uint8 pressure = _pressure;
        if (_enemies.size() >= MostAlive())
        {
            _reliefMs = 0;
            _fullMs += TICK_MS;
            if (_fullMs >= PRESSURE_EVERY_MS)
            {
                _fullMs = 0;
                if (_pressure < PRESSURE_MOST)
                    ++_pressure;
            }
        }
        else
        {
            _fullMs = 0;
            _reliefMs += TICK_MS;
            if (_reliefMs >= PRESSURE_RELIEF_MS)
                _pressure = 0;
        }
        if (_pressure != pressure)
        {
            for (ObjectGuid const& guid : _enemies)
                if (Creature* enemy = instance->GetCreature(guid))
                    if (Enemy* own = enemy->CustomData.Get<Enemy>("titan_firepit"))
                        own->pressure = 1.0f + PRESSURE_DAMAGE * float(_pressure);
            Announce(_pressure > pressure ? "Pressure mounts: the enemies deal " + std::to_string(_pressure * 5)
                + "% more damage. Thin them out!" : std::string("The pressure eases."), _pressure > pressure);
        }

        std::vector<Player*> living = Living();
        for (Player* player : living)
            if (Record* record = RecordOf(player))
            {
                record->down = false;
                record->mostAlive = std::max(record->mostAlive, uint32(_enemies.size()));
                if (record->peakWave == _wave)
                    record->peakAlive = std::max(record->peakAlive, uint32(_enemies.size()));
            }
        for (auto& [guid, record] : _records)
            if (!record.down && !record.left)
                if (Player* player = ObjectAccessor::GetPlayer(instance, guid); player && !player->IsAlive())
                    FellNow(record, "");

        if (living.empty())
        {
            Announce("The Firepit claims its last challenger.", true);
            Cheer(100, true);
            _phase = Phase::ENDING;
            ClearMechanics();
            _timer = END_DELAY_MS;
            return;
        }

        // A boss alive: the waves wait for it. A cleared wave: the next in 10 sec (a boss's, 20).
        bool bossAlive = false;
        for (ObjectGuid const& guid : _enemies)
            if (Creature* enemy = instance->GetCreature(guid))
                if (Enemy const* own = enemy->CustomData.Get<Enemy>("titan_firepit"); own && own->boss)
                    bossAlive = true;
        if (bossAlive || (_wave % BOSS_EVERY == 0 && !_queue.empty()))
            _waveMs = std::max<uint32>(_waveMs, BOSS_BREATHER_MS);
        bool clear = _enemies.empty() && _queue.empty() && _reinforcements.empty();
        if (clear && _wave)
            _clearedWave = std::max(_clearedWave, _wave);
        if (clear && !_echoes.empty())
            DismissEchoes();                        // the Echoes go as the wave clears
        if (clear && !_breather)
        {
            _breather = true;
            bool bossWave = _wave % BOSS_EVERY == 0;
            if (bossWave)
                Recover();                          // a boss wave cleared: the fallen rise
            _waveMs = std::min(_waveMs, bossWave ? BOSS_BREATHER_MS : BREATHER_MS);
        }
        // Before a boss: the field cleared first (the timer held), then 20 sec to prepare.
        if (!_waveMs && (_wave + 1) % BOSS_EVERY == 0 && !_preparing)
        {
            if (!clear)
            {
                if (!_holding)
                    Announce("The next wave is a boss's: clear the field to face it.");
                _holding = true;
                _waveMs = TICK_MS;
            }
            else
            {
                _holding = false;
                _preparing = true;
                _waveMs = BOSS_PREP_MS;
                uint32 entry = BossAt(_wave + 1);
                CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(entry);
                Announce("Wave " + std::to_string(_wave + 1) + ": " + (info ? info->Name : std::string("a boss"))
                    + " enters the Ring in 20 sec. Prepare!", true);
            }
        }
        else if (!_waveMs)
        {
            _preparing = false;
            StartWave(_wave + 1);
        }

        for (auto const& ref : instance->GetPlayers())
            if (Player* player = ref.GetSource())
                SendHud(player);
    }

    // The field's cap (MOST_ALIVE_*).
    uint32 MostAlive() const
    {
        return MOST_ALIVE_BASE + MOST_ALIVE_A_HERO * _groupSize;
    }

    // F,<bosses>,<tanks>,<melee>,<ranged>,<casters>,<healers>,<queued>,<badges>: the Enemy Forces tracker (LookOf).
    // Only the living, hostile enemies the wave's clear needs, each once in its look (the Ashen Echo, web anchors and
    // spectators never); the reinforcements apart; badges for the Bolstered and Mending ones.
    void SendForces(Player* player)
    {
        uint32 counts[LOOKS] = {};
        bool bolstered = false;
        for (ObjectGuid const& guid : _enemies)
            if (Creature* enemy = instance->GetCreature(guid); enemy && enemy->IsAlive())
                if (Enemy* own = EnemyOf(enemy))
                {
                    ++counts[std::min<uint8>(own->look, LOOKS - 1)];
                    bolstered |= own->bolster > 0;
                }
        std::string badges;
        if (bolstered)
            badges += "Bolstered";
        if (Creature* mender = instance->GetCreature(_mender); mender && mender->IsAlive())
            badges += badges.empty() ? "Mending" : " Mending";
        std::string text = "F";
        for (uint32 count : counts)
            text += ',' + std::to_string(count);
        SendAddon(player, text + ',' + std::to_string(_reinforcements.size() + _queue.size()) + ',' + badges);
    }

    void SendHud(Player* player)
    {
        uint32 living = uint32(Living().size());
        uint32 overwhelm = _pressure;               // the HUD's Pressure (stacks), and the queue's size below
        SendAddon(player, "H," + std::to_string(_wave) + ',' + std::to_string((_waveMs + 999) / 1000) + ','
            + std::to_string(_enemies.size()) + ',' + std::to_string(_wave && _wave % BOSS_EVERY == 0 ? 1 : 0) + ','
            + std::to_string(living) + ',' + std::to_string(_records.size()) + ','
            + std::to_string(_phase == Phase::COUNTDOWN ? (_timer + 999) / 1000 : 0) + ','
            + std::to_string(overwhelm) + ',' + std::to_string(MostAlive()) + ','
            + std::to_string(_reinforcements.size()) + ',' + std::to_string((_carnageMs + 999) / 1000));
        SendForces(player);
    }

    void Finish()
    {
        _phase = Phase::DONE;
        for (ObjectGuid const& guid : _enemies)
            if (Creature* enemy = instance->GetCreature(guid))
                enemy->DespawnOrUnsummon();
        _enemies.clear();
        _queue.clear();
        _reinforcements.clear();
        _pressure = 0;
        _fullMs = _reliefMs = 0;

        // Everyone back on their feet, the spoils out, the records kept, the results shown.
        std::vector<Player*> present;
        for (auto const& ref : instance->GetPlayers())
            if (Player* player = ref.GetSource())
                present.push_back(player);
        for (Player* player : present)
        {
            if (!player->IsAlive())
            {
                player->ResurrectPlayer(1.0f);
                player->SpawnCorpseBones();
            }
            player->CombatStop(true);
            player->SetFullHealth();
            player->SetPower(player->getPowerType(), player->GetMaxPower(player->getPowerType()));

            Record* record = RecordOf(player);
            if (!record || record->lootWaves.empty())
                continue;
            std::vector<Spoil> items = Fireforged(player, record->lootWaves);
            for (Spoil const& spoil : items)
                if (spoil.titanforged)
                    Announce(player->GetName() + " forged a Titanforged piece!", true);
            if (!items.empty())
                _spoils[player->GetGUID()] = items;
        }
        // One great chest for the run, before the Keeper: each Hero takes their own from it.
        if (!_spoils.empty())
            if (GameObject* chest = instance->SummonGameObject(GO_SPOILS, CENTER.GetPositionX() + SPOILS_OFFSET,
                CENTER.GetPositionY(), FloorAt(CENTER.GetPositionX() + SPOILS_OFFSET, CENTER.GetPositionY()),
                float(M_PI), 0.0f, 0.0f, 1.0f, 0.0f, SPOILS_SECONDS))
                _chest = chest->GetGUID();

        SaveRecords();
        for (Player* player : present)
            SendResults(player);
        if (TempSummon* keeper = instance->SummonCreature(NPC_KEEPER, Position(CENTER.GetPositionX() - 14.0f,
            CENTER.GetPositionY(), CENTER.GetPositionZ(), 0.0f)))
            keeper->SetFaction(35);
        Announce("The fires die down. Claim your spoils, and speak to the Keeper to leave.");
    }

    void SaveRecords()
    {
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        for (auto const& [guid, record] : _records)
        {
            if (!_clearedWave)
                continue;
            trans->Append("INSERT INTO `titan_firepit_records` (`guid`, `bracket`, `best_wave`, `group_size`, "
                "`kills`, `run_date`) VALUES ({}, {}, {}, {}, {}, UNIX_TIMESTAMP()) ON DUPLICATE KEY UPDATE "
                "`group_size` = IF(VALUES(`best_wave`) > `best_wave`, VALUES(`group_size`), `group_size`), "
                "`kills` = IF(VALUES(`best_wave`) > `best_wave`, VALUES(`kills`), `kills`), "
                "`run_date` = IF(VALUES(`best_wave`) > `best_wave`, VALUES(`run_date`), `run_date`), "
                "`best_wave` = GREATEST(`best_wave`, VALUES(`best_wave`))",
                guid.GetCounter(), Bracket(_groupSize), _clearedWave, _groupSize, record.kills);
        }
        CharacterDatabase.CommitTransaction(trans);
    }

    // The results screen's numbers: RUN, one P a Hero, then END.
    void SendResults(Player* player)
    {
        uint32 reached = 0;
        for (auto const& [guid, record] : _records)
            reached = std::max(reached, record.peakWave);
        SendAddon(player, "RUN," + std::to_string(reached) + ',' + std::to_string(_runMs / 1000) + ','
            + std::to_string(_groupSize) + ',' + std::to_string(_kills) + ','
            + std::to_string(uint32(std::lround((1.0f + GROUP_HEALTH * float(_groupSize - 1)) * 100.0f))) + ','
            + std::to_string(_setup.startWave));
        for (auto const& [guid, record] : _records)
            SendAddon(player, "P," + Clean(record.name) + ',' + std::to_string(record.level) + ','
                + std::to_string(record.peakWave) + ',' + std::to_string(record.peakAlive) + ','
                + std::to_string(record.mostAlive) + ',' + std::to_string(record.damage) + ','
                + std::to_string(record.healing) + ',' + std::to_string(record.taken) + ','
                + std::to_string(record.kills) + ',' + std::to_string(record.bossKills) + ','
                + std::to_string(record.biggestHit) + ',' + std::to_string(record.biggestTaken) + ','
                + std::to_string(record.deaths) + ',' + std::to_string(record.deathWave) + ','
                + std::to_string(record.resurrections) + ',' + std::to_string(record.loot) + ','
                + Clean(record.killer.substr(0, 40)));
        SendAddon(player, "END");
    }

    Setup _setup;
    uint32 _groupSize = 1;
    Phase _phase = Phase::WAITING;
    uint32 _timer = 0, _wave = 0, _waveMs = 0, _tickMs = 0, _runMs = 0, _kills = 0;
    std::vector<Spawn> _reinforcements;             // past the cap: they enter as room opens
    uint32 _reinforceMs = REINFORCE_MS, _fullMs = 0, _reliefMs = 0;
    uint8 _pressure = 0;                            // Pressure's stacks
    bool _breather = false;
    bool _holding = false, _preparing = false;      // before a boss: waiting for the field to clear, the prep
    uint32 _clearedWave = 0;                        // the run's highest fully cleared wave (records, checkpoints)
    std::map<ObjectGuid, uint32> _protected;        // revived Heroes: run ms their protection ends
    std::unordered_map<ObjectGuid, BossState> _bosses;
    std::vector<Blast> _blasts;
    std::vector<Fx> _fx;                            // looks to come (FxTick)
    uint32 _timerMs = 0;                            // GrowTimers'
    std::vector<uint8> _affixes;                    // the world affixes active now
    std::map<uint8, uint32> _affixMs;               // each one's next trigger (ms)
    std::set<ObjectGuid> _moltenLast;               // Molten Ground's last marked
    std::vector<Pool> _pools;
    std::vector<Wind> _winds;
    std::vector<Brand> _brands;
    uint32 _secondMs = 0;                           // the affixes' once-a-second work
    ObjectGuid _affixSource;                        // the Firepit itself: what deals the affixes' burns
    ObjectGuid _mender;
    uint32 _mendLeft = 0, _mendTick = 0, _volatile = 0;
    std::vector<ObjectGuid> _flameboundWaiting;
    std::vector<Echo> _echoes;
    std::vector<Chain> _chains;
    std::map<ObjectGuid, uint32> _pursuers;         // Burning Pursuit: run ms each one's fixate ends
    std::vector<Crack> _cracks;
    Major _major;
    std::vector<Web> _webs;
    uint32 _carnageMs = 0;                          // Marked for Carnage's time left (0: none)
    std::vector<ObjectGuid> _crowd;                 // the Spectators
    std::vector<ObjectGuid> _natives, _enemies;
    std::vector<Spawn> _queue;
    std::map<ObjectGuid, Record> _records;
    std::unordered_map<ObjectGuid, std::vector<Spoil>> _spoils;     // each Hero's, in the one chest
    ObjectGuid _chest;
    std::vector<ObjectGuid> _lootBosses;            // the loot wave's bosses still to fall
    uint32 _lootWave = 0;
};

    FirepitInstance* RunOf(Map* map)
    {
        if (!map || map->GetId() != MAP_FIREPIT || !map->IsDungeon())
            return nullptr;
        InstanceMap* instanceMap = map->ToInstanceMap();
        return instanceMap ? dynamic_cast<FirepitInstance*>(instanceMap->GetInstanceScript()) : nullptr;
    }

    // Who goes in with the Hero: their group's members near them (all of them with a GM's start), or just them.
    bool Gather(Player* leader, bool anywhere, std::vector<Player*>& members, std::string& why)
    {
        members.clear();
        Group* group = leader->GetGroup();
        if (group && group->GetLeaderGUID() != leader->GetGUID())
        {
            why = "Only your group's leader can lead it into the Firepit.";
            return false;
        }
        if (!group)
            members.push_back(leader);
        else
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                if (Player* member = ref->GetSource(); member && member->IsInWorld() && member->GetSession())
                {
                    if (!anywhere && (member->GetMap() != leader->GetMap() || !member->IsWithinDist(leader, 40.0f)))
                    {
                        why = member->GetName() + " is too far away.";
                        return false;
                    }
                    members.push_back(member);
                }
        if (members.size() > MOST_PLAYERS)
        {
            why = "The Firepit takes at most 10.";
            return false;
        }
        for (Player* member : members)
        {
            if (member->GetLevel() < FIREPIT_LEVEL)
            {
                why = member->GetName() + " must be level " + std::to_string(FIREPIT_LEVEL) + ".";
                return false;
            }
            if (!member->IsAlive() || member->IsInCombat() || member->GetMap()->Instanceable())
            {
                why = member->GetName() + " must be alive, out of combat and out of any instance.";
                return false;
            }
        }
        return true;
    }

    // A new run: its instance made, everyone sent in.
    bool StartRun(Player* leader, uint32 startWave, bool anywhere, std::string& why)
    {
        std::vector<Player*> members;
        if (!Gather(leader, anywhere, members, why))
            return false;
        uint32 instanceId = sMapMgr->GenerateInstanceId();
        if (!sInstanceSaveMgr->AddInstanceSave(MAP_FIREPIT, instanceId, DUNGEON_DIFFICULTY_NORMAL))
        {
            why = "The Firepit could not be lit.";
            return false;
        }
        {
            std::lock_guard guard(registryLock);
            Setup& setup = setups[instanceId];
            setup.startWave = std::max<uint32>(1, startWave);
            for (Player* member : members)
            {
                setup.players.push_back(member->GetGUID());
                setup.homes[member->GetGUID()] = member->GetWorldLocation();
                destinations[member->GetGUID()] = instanceId;
            }
            firepits.insert(instanceId);
        }
        for (std::size_t i = 0; i < members.size(); ++i)
        {
            float angle = 2.0f * float(M_PI) * float(i) / float(members.size());
            members[i]->TeleportTo(MAP_FIREPIT, CENTER.GetPositionX() + ARRIVAL_RADIUS * std::cos(angle),
                CENTER.GetPositionY() + ARRIVAL_RADIUS * std::sin(angle), CENTER.GetPositionZ(),
                angle + float(M_PI));
        }
        return true;
    }

    void SendHome(Player* player)
    {
        WorldLocation home;
        FirepitInstance* run = RunOf(player->GetMap());
        if (run && run->Home(player, home))
            player->TeleportTo(home);
        else
            player->TeleportTo(player->m_homebindMapId, player->m_homebindX, player->m_homebindY, player->m_homebindZ,
                player->GetOrientation());
    }

    void ShowLeaderboard(Player* player)
    {
        ChatHandler handler(player->GetSession());
        char const* const titles[] = { "Solo", "Duo", "Trio", "Group" };
        for (uint8 bracket = 1; bracket <= BRACKETS; ++bracket)
        {
            handler.PSendSysMessage("|cffff7f2aThe Firepit - {}:|r", titles[bracket - 1]);
            QueryResult result = CharacterDatabase.Query("SELECT c.`name`, b.`best_wave`, b.`group_size` FROM "
                "`titan_firepit_records` b JOIN `characters` c ON c.`guid` = b.`guid` WHERE b.`bracket` = {} "
                "ORDER BY b.`best_wave` DESC, b.`run_date` ASC LIMIT {}", bracket, LEADERBOARD_ROWS);
            if (!result)
            {
                handler.PSendSysMessage("  No one yet.");
                continue;
            }
            uint32 rank = 0;
            do
            {
                Field* fields = result->Fetch();
                handler.PSendSysMessage("  {}. {}: wave {}{}", ++rank, fields[0].Get<std::string>(),
                    fields[1].Get<uint32>(), bracket == BRACKETS
                        ? " (group of " + std::to_string(fields[2].Get<uint32>()) + ")" : std::string());
            } while (result->NextRow());
        }
    }

    // The Hero's best wave in any bracket (0 none), and the latest checkpoint they may start from.
    uint32 BestWave(ObjectGuid const& guid)
    {
        QueryResult result = CharacterDatabase.Query("SELECT MAX(`best_wave`) FROM `titan_firepit_records` "
            "WHERE `guid` = {}", guid.GetCounter());
        return result && !result->Fetch()[0].IsNull() ? result->Fetch()[0].Get<uint32>() : 0;
    }

    uint32 Checkpoint(uint32 best)
    {
        return best >= CHECKPOINT_EVERY ? best / CHECKPOINT_EVERY * CHECKPOINT_EVERY : 1;
    }
}

// The group finder's Firepit tab (Emberforge_Firepit): the state, the leaderboard, and the queue. A Hero alone goes in
// at once; a group's leader proposes, every member accepts (40 sec), then all go in together. On the world's thread
// only (members on other maps are teleported from there).
namespace
{
    struct Proposal
    {
        uint32 startWave = 1;
        std::vector<ObjectGuid> members;
        std::unordered_map<ObjectGuid, uint8> answers;      // 0 waiting, 1 accepted, 2 declined
        uint32 expiresAt = 0;
    };
    std::unordered_map<ObjectGuid, Proposal> proposals;      // by leader

    std::pair<ObjectGuid const, Proposal>* ProposalOf(ObjectGuid const& guid)
    {
        for (auto& entry : proposals)
            if (std::find(entry.second.members.begin(), entry.second.members.end(), guid) != entry.second.members.end())
                return &entry;
        return nullptr;
    }

    void ToMembers(Proposal const& proposal, std::string const& payload)
    {
        for (ObjectGuid const& guid : proposal.members)
            if (Player* member = ObjectAccessor::FindConnectedPlayer(guid))
                SendAddon(member, payload);
    }

    // S,<best solo>,<duo>,<trio>,<group>,<in a proposal 0|1>,<the level it opens at>,<latest checkpoint>, then each
    // bracket's leaderboard: L,<bracket 1-4>,<rank>,<name>,<wave>,<group size> and LE.
    void SendFinderState(Player* player)
    {
        uint32 best[BRACKETS + 1] = {};
        if (QueryResult own = CharacterDatabase.Query("SELECT `bracket`, `best_wave` FROM `titan_firepit_records` "
            "WHERE `guid` = {}", player->GetGUID().GetCounter()))
            do
            {
                uint8 bracket = own->Fetch()[0].Get<uint8>();
                if (bracket >= 1 && bracket <= BRACKETS)
                    best[bracket] = own->Fetch()[1].Get<uint32>();
            } while (own->NextRow());
        uint32 highest = *std::max_element(std::begin(best), std::end(best));
        SendAddon(player, "S," + std::to_string(best[1]) + ',' + std::to_string(best[2]) + ','
            + std::to_string(best[3]) + ',' + std::to_string(best[4]) + ','
            + std::to_string(ProposalOf(player->GetGUID()) ? 1 : 0) + ',' + std::to_string(FIREPIT_LEVEL) + ','
            + std::to_string(Checkpoint(highest)));
        for (uint8 bracket = 1; bracket <= BRACKETS; ++bracket)
        {
            QueryResult result = CharacterDatabase.Query("SELECT c.`name`, b.`best_wave`, b.`group_size` FROM "
                "`titan_firepit_records` b JOIN `characters` c ON c.`guid` = b.`guid` WHERE b.`bracket` = {} "
                "ORDER BY b.`best_wave` DESC, b.`run_date` ASC LIMIT {}", bracket, LEADERBOARD_ROWS);
            uint32 rank = 0;
            if (result)
                do
                {
                    Field* fields = result->Fetch();
                    SendAddon(player, "L," + std::to_string(bracket) + ',' + std::to_string(++rank) + ','
                        + Clean(fields[0].Get<std::string>()) + ',' + std::to_string(fields[1].Get<uint32>()) + ','
                        + std::to_string(fields[2].Get<uint32>()));
                } while (result->NextRow());
        }
        SendAddon(player, "LE");
    }

    void CancelProposal(ObjectGuid const& leader, std::string const& why)
    {
        auto found = proposals.find(leader);
        if (found == proposals.end())
            return;
        ToMembers(found->second, "QC," + Clean(why));
        proposals.erase(found);
    }

    // A run from wave 1, or from a checkpoint (every 5th) all its Heroes have reached.
    void Queue(Player* player, uint32 startWave)
    {
        std::vector<Player*> members;
        std::string why;
        if (ProposalOf(player->GetGUID()))
            return;
        if (!Gather(player, true, members, why))
        {
            SendAddon(player, "QC," + Clean(why));
            return;
        }
        if (startWave != 1)
        {
            if (!startWave || startWave % CHECKPOINT_EVERY)
                return;
            for (Player* member : members)
                if (Checkpoint(BestWave(member->GetGUID())) < startWave)
                {
                    SendAddon(player, "QC," + Clean(member->GetName() + " has not reached wave "
                        + std::to_string(startWave) + "."));
                    return;
                }
        }
        if (members.size() == 1)
        {
            if (!StartRun(player, startWave, true, why))
                SendAddon(player, "QC," + Clean(why));
            else
                SendAddon(player, "QG");
            return;
        }
        Proposal& proposal = proposals[player->GetGUID()];
        proposal.startWave = startWave;
        proposal.expiresAt = GameTime::GetGameTimeMS().count() + PROPOSAL_MS;
        for (Player* member : members)
        {
            proposal.members.push_back(member->GetGUID());
            proposal.answers[member->GetGUID()] = member == player ? 1 : 0;
        }
        ToMembers(proposal, "QP," + Clean(player->GetName()) + ',' + std::to_string(members.size()) + ','
            + std::to_string(PROPOSAL_MS / 1000) + ',' + std::to_string(startWave));
        for (Player* member : members)
            ToMembers(proposal, "QA," + Clean(member->GetName()) + ',' + std::to_string(proposal.answers[
                member->GetGUID()]));
    }

    void Answer(Player* player, bool accept)
    {
        auto* entry = ProposalOf(player->GetGUID());
        if (!entry)
            return;
        ObjectGuid leader = entry->first;
        Proposal& proposal = entry->second;
        if (!accept)
        {
            CancelProposal(leader, player->GetName() + " declined.");
            return;
        }
        proposal.answers[player->GetGUID()] = 1;
        ToMembers(proposal, "QA," + Clean(player->GetName()) + ",1");
        for (auto const& [guid, answer] : proposal.answers)
            if (answer != 1)
                return;
        // Everyone in: the group as it is now goes.
        Player* lead = ObjectAccessor::FindConnectedPlayer(leader);
        std::string why = "The group's leader is gone.";
        if (lead && StartRun(lead, proposal.startWave, true, why))
        {
            ToMembers(proposal, "QG");
            proposals.erase(leader);
        }
        else
            CancelProposal(leader, why);
    }

    void HandleRequest(Player* player, std::string const& request)
    {
        if (request == "STATE")
            SendFinderState(player);
        else if (request == "QUEUE")
            Queue(player, 1);
        else if (request.rfind("QUEUE,", 0) == 0)
        {
            Optional<uint32> wave = Acore::StringTo<uint32>(std::string_view(request).substr(6));
            if (wave)
                Queue(player, *wave);
        }
        else if (request == "ACCEPT")
            Answer(player, true);
        else if (request == "DECLINE")
            Answer(player, false);
        else if (request == "EXIT")
        {
            if (FirepitInstance* run = RunOf(player->GetMap()))
            {
                run->Leaving(player);   // the minimap eye's Teleport out (Emberforge_Firepit)
                SendHome(player);
            }
        }
        else if (request == "LEAVE")
        {
            if (auto* entry = ProposalOf(player->GetGUID()))
                CancelProposal(entry->first, player->GetName() + " left the queue.");
        }
    }

    void ExpireProposals()
    {
        uint32 now = GameTime::GetGameTimeMS().count();
        std::vector<ObjectGuid> expired;
        for (auto const& [leader, proposal] : proposals)
            if (now >= proposal.expiresAt)
                expired.push_back(leader);
        for (ObjectGuid const& leader : expired)
            CancelProposal(leader, "Not everyone accepted in time.");
    }
}

float Firepit::HealthMultiplier(Creature const* creature)
{
    Enemy const* own = creature ? creature->CustomData.Get<Enemy>("titan_firepit") : nullptr;
    return own ? own->health : 1.0f;
}

float Firepit::DamageMultiplier(Creature const* creature)
{
    Enemy const* own = creature ? creature->CustomData.Get<Enemy>("titan_firepit") : nullptr;
    return own ? own->damage * own->pressure * own->affix : 1.0f;
}

bool Firepit::IsBoss(Creature const* creature)
{
    Enemy const* own = creature ? creature->CustomData.Get<Enemy>("titan_firepit") : nullptr;
    return own && own->boss;
}

// A Firepit instance's script is its own (the Depths' never runs there).
class titan_firepit_map : public AllMapScript
{
public:
    titan_firepit_map() : AllMapScript("titan_firepit_map", { ALLMAPHOOK_ON_BEFORE_CREATE_INSTANCE_SCRIPT,
        ALLMAPHOOK_ON_DESTROY_INSTANCE }) { }

    void OnBeforeCreateInstanceScript(InstanceMap* map, InstanceScript** script, bool /*load*/,
        std::string /*data*/, uint32 /*completedEncounterMask*/) override
    {
        if (map->GetId() != MAP_FIREPIT)
            return;
        bool ours;
        {
            std::lock_guard guard(registryLock);
            ours = firepits.count(map->GetInstanceId()) != 0;
        }
        if (ours)
            *script = new FirepitInstance(map);         // outside the lock: it takes the lock for its setup
    }

    void OnDestroyInstance(MapInstanced* /*mapInstanced*/, Map* map) override
    {
        std::lock_guard guard(registryLock);
        firepits.erase(map->GetInstanceId());
        setups.erase(map->GetInstanceId());
        for (auto destination = destinations.begin(); destination != destinations.end();)
            destination = destination->second == map->GetInstanceId() ? destinations.erase(destination)
                : std::next(destination);
    }
};

// Players who left a run lose its bind (the Depths' own instance theirs again); the core's hooks are ours.
class titan_firepit_world : public WorldScript
{
public:
    titan_firepit_world() : WorldScript("titan_firepit_world", { WORLDHOOK_ON_STARTUP, WORLDHOOK_ON_UPDATE }) { }

    void OnStartup() override
    {
        InstanceOverride::Destination = &Destination;
        InstanceOverride::MaxPlayers = &MaxPlayers;
        InstanceOverride::KeepsDurability = &KeepsDurability;
    }

    void OnUpdate(uint32 /*diff*/) override
    {
        std::vector<std::pair<ObjectGuid, std::string>> asked;
        std::vector<std::pair<ObjectGuid, uint32>> pending;
        {
            std::lock_guard guard(registryLock);
            asked.swap(requests);
            pending.swap(unbinds);
        }
        for (auto const& [guid, request] : asked)
            if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
                HandleRequest(player, request);
        if (!proposals.empty())
            ExpireProposals();
        if (pending.empty())
            return;
        std::vector<std::pair<ObjectGuid, uint32>> later;
        for (auto const& [guid, instanceId] : pending)
        {
            Player* player = ObjectAccessor::FindConnectedPlayer(guid);
            if (player && player->GetMapId() == MAP_FIREPIT && player->GetInstanceId() == instanceId)
                continue;                               // back in it
            if (player && player->IsBeingTeleported())
            {
                later.emplace_back(guid, instanceId);   // still on the way out
                continue;
            }
            InstancePlayerBind* bind = sInstanceSaveMgr->PlayerGetBoundInstance(guid, MAP_FIREPIT,
                DUNGEON_DIFFICULTY_NORMAL);
            if (bind && bind->save && bind->save->GetInstanceId() == instanceId)
                sInstanceSaveMgr->PlayerUnbindInstance(guid, MAP_FIREPIT, DUNGEON_DIFFICULTY_NORMAL, true, player);
        }
        if (!later.empty())
        {
            std::lock_guard guard(registryLock);
            unbinds.insert(unbinds.end(), later.begin(), later.end());
        }
    }
};

// What a run counts: damage, healing, kills, deaths, resurrections.
class titan_firepit_unit : public UnitScript
{
public:
    titan_firepit_unit() : UnitScript("titan_firepit_unit", true, { UNITHOOK_ON_DAMAGE, UNITHOOK_ON_HEAL,
        UNITHOOK_ON_UNIT_DEATH }) { }

    // Nothing in the Firepit drops anything (no items, no coin, no skinning): the end-of-run cache is its only loot.
    void OnUnitDeath(Unit* unit, Unit* /*killer*/) override
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !RunOf(creature->GetMap()))
            return;
        creature->loot.clear();
        creature->RemoveDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
        creature->RemoveUnitFlag(UNIT_FLAG_SKINNABLE);
    }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (!attacker || !victim || !damage)
            return;
        FirepitInstance* run = RunOf(victim->GetMap());
        if (!run)
            return;
        if (Player* player = attacker->GetCharmerOrOwnerPlayerOrPlayerItself(); player && victim->ToCreature())
        {
            run->Unprotect(player);                 // a revived Hero's attack ends their protection
            run->FlameboundTaken(victim->ToCreature(), damage);
            run->MajorTaken(victim->ToCreature(), damage);
            run->Damage(player, damage);
        }
        if (Player* player = victim->ToPlayer(); player && attacker->ToCreature()
            && !attacker->GetCharmerOrOwnerPlayerOrPlayerItself())
        {
            if (run->IsProtected(player))
            {
                damage = 0;
                return;
            }
            run->Taken(player, damage);
        }
    }

    void OnHeal(Unit* healer, Unit* receiver, uint32& gain) override
    {
        if (!healer || !receiver || !gain)
            return;
        if (FirepitInstance* run = RunOf(receiver->GetMap()))
            if (Player* player = healer->GetCharmerOrOwnerPlayerOrPlayerItself())
                run->Healed(player, gain);
    }
};

class titan_firepit_player : public PlayerScript
{
public:
    titan_firepit_player() : PlayerScript("titan_firepit_player", {
        PLAYERHOOK_ON_CREATURE_KILL,
        PLAYERHOOK_ON_CREATURE_KILLED_BY_PET,
        PLAYERHOOK_ON_PLAYER_KILLED_BY_CREATURE,
        PLAYERHOOK_ON_PLAYER_RESURRECT,
        PLAYERHOOK_CAN_REPOP_AT_GRAVEYARD,
        PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT
    }) { }

    // The group finder tab's requests (STATE, QUEUE, ACCEPT, DECLINE, LEAVE) and the eye's EXIT: to the world's
    // thread.
    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 language, std::string& msg,
        Player* receiver) override
    {
        std::string const prefix = std::string(PREFIX) + '\t';
        if (language != LANG_ADDON || receiver != player || msg.compare(0, prefix.size(), prefix) != 0)
            return true;
        std::lock_guard guard(registryLock);
        requests.emplace_back(player->GetGUID(), msg.substr(prefix.size()));
        return false;
    }

    void OnPlayerCreatureKill(Player* killer, Creature* killed) override
    {
        if (FirepitInstance* run = RunOf(killer->GetMap()))
            run->Killed(killer, killed);
    }

    void OnPlayerCreatureKilledByPet(Player* owner, Creature* killed) override
    {
        if (FirepitInstance* run = RunOf(owner->GetMap()))
            run->Killed(owner, killed);
    }

    void OnPlayerKilledByCreature(Creature* killer, Player* killed) override
    {
        if (FirepitInstance* run = RunOf(killed->GetMap()))
            run->KilledBy(killed, killer);
    }

    void OnPlayerResurrect(Player* player, float /*restorePercent*/, bool& /*applySickness*/) override
    {
        if (FirepitInstance* run = RunOf(player->GetMap()))
            run->Resurrected(player);
    }

    // The fallen stay where they fell (a ghost there): a battle resurrection can still reach them.
    bool OnPlayerCanRepopAtGraveyard(Player* player) override
    {
        FirepitInstance* run = RunOf(player->GetMap());
        return !run || run->Over();
    }
};

// The Firepit Keeper: outside, the way in, the leaderboard; inside, the way out.
class npc_titan_firepit_keeper : public CreatureScript
{
public:
    npc_titan_firepit_keeper() : CreatureScript("npc_titan_firepit_keeper") { }

    enum Actions : uint32
    {
        ACTION_ENTER = 1,
        ACTION_LEADERBOARD = 2,
        ACTION_LEAVE = 3,
    };

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);
        if (RunOf(player->GetMap()))
            AddGossipItemFor(player, GOSSIP_ICON_TAXI, "Take me out of the Firepit.", GOSSIP_SENDER_MAIN, ACTION_LEAVE);
        else
        {
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Open the Firepit (the group finder).", GOSSIP_SENDER_MAIN,
                ACTION_ENTER);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Show me the leaderboard.", GOSSIP_SENDER_MAIN,
                ACTION_LEADERBOARD);
        }
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* /*creature*/, uint32 /*sender*/, uint32 action) override
    {
        CloseGossipMenuFor(player);
        switch (action)
        {
            case ACTION_ENTER:
                SendAddon(player, "OPEN");      // the group finder's Firepit tab: the queue
                break;
            case ACTION_LEADERBOARD:
                ShowLeaderboard(player);
                break;
            case ACTION_LEAVE:
                SendHome(player);
                break;
            default:
                break;
        }
        return true;
    }
};

// A Hero's spoils chest: theirs alone.
class go_titan_firepit_spoils : public GameObjectScript
{
public:
    go_titan_firepit_spoils() : GameObjectScript("go_titan_firepit_spoils") { }

    bool OnGossipHello(Player* player, GameObject* chest) override
    {
        FirepitInstance* run = RunOf(chest->GetMap());
        bool gone = false;
        std::vector<Spoil> items = run ? run->TakeSpoils(chest, player, gone) : std::vector<Spoil>();
        if (items.empty())
        {
            ChatHandler(player->GetSession()).SendSysMessage("Nothing in the chest is yours: no boss of a 15th wave "
                "fell for you this run, or you took your spoils already.");
            return true;
        }
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        bool mailed = false;
        ChatHandler chat(player->GetSession());
        for (Spoil const& spoil : items)
        {
            if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(spoil.entry))
                chat.PSendSysMessage("|cffff7f2aThe Firepit:|r {}{} (item level {}).",
                    spoil.titanforged ? "|cffffcc33Titanforged|r " : "", proto->Name1, proto->ItemLevel);
            auto modify = [&spoil](Item* item)
            {
                for (std::size_t i = 0; i < spoil.modifiers.size() && i < std::size(MODIFIER_SLOTS); ++i)
                    item->SetEnchantment(MODIFIER_SLOTS[i], spoil.modifiers[i], 0, 0);
            };
            for (uint32 modifier : spoil.modifiers)
                if (SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(modifier))
                    chat.PSendSysMessage("|cffff7f2a  +|r {}", enchant->description[0]);
            ItemPosCountVec dest;
            if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, spoil.entry, 1) == EQUIP_ERR_OK)
                if (Item* item = player->StoreNewItem(dest, spoil.entry, true, spoil.randomPropertyId))
                {
                    modify(item);
                    player->SendNewItem(item, 1, true, false);
                    continue;
                }
            if (Item* item = Item::CreateItem(spoil.entry, 1, player, false, uint32(spoil.randomPropertyId)))
            {
                modify(item);
                item->SaveToDB(trans);
                MailDraft("Spoils of the Firepit", "Your bags were full; the Firepit's spoils came here.")
                    .AddItem(item)
                    .SendMailTo(trans, player, MailSender(MAIL_CREATURE, NPC_KEEPER), MAIL_CHECK_MASK_COPIED);
                mailed = true;
            }
        }
        CharacterDatabase.CommitTransaction(trans);
        if (mailed)
            ChatHandler(player->GetSession()).SendSysMessage("Your bags were full: the rest of your spoils are in "
                "your mailbox.");
        if (gone)
        {
            chest->SetRespawnTime(0);
            chest->Delete();
        }
        return true;
    }
};

using namespace Acore::ChatCommands;

// .firepit start [wave] (the GM's group, wherever they are), .firepit next, .firepit stop.
class titan_firepit_commands : public CommandScript
{
public:
    titan_firepit_commands() : CommandScript("titan_firepit_commands") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable firepit =
        {
            { "start", HandleStart, SEC_GAMEMASTER, Console::No },
            { "next", HandleNext, SEC_GAMEMASTER, Console::No },
            { "stop", HandleStop, SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commandTable =
        {
            { "firepit", firepit },
        };
        return commandTable;
    }

    static bool HandleStart(ChatHandler* handler, Optional<uint32> wave)
    {
        std::string why;
        if (!StartRun(handler->GetSession()->GetPlayer(), wave.value_or(1), true, why))
            handler->SendSysMessage(why);
        return true;
    }

    static bool HandleNext(ChatHandler* handler)
    {
        FirepitInstance* run = RunOf(handler->GetSession()->GetPlayer()->GetMap());
        if (!run)
        {
            handler->SendSysMessage("You are not in the Firepit.");
            return true;
        }
        run->SkipWave();
        return true;
    }

    static bool HandleStop(ChatHandler* handler)
    {
        FirepitInstance* run = RunOf(handler->GetSession()->GetPlayer()->GetMap());
        if (!run)
        {
            handler->SendSysMessage("You are not in the Firepit.");
            return true;
        }
        run->Stop();
        return true;
    }
};

void AddSC_firepit()
{
    new titan_firepit_map();
    new titan_firepit_world();
    new titan_firepit_unit();
    new titan_firepit_player();
    new npc_titan_firepit_keeper();
    new go_titan_firepit_spoils();
    new titan_firepit_commands();
}
