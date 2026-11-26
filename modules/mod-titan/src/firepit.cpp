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
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <algorithm>
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
// - Waves every 60 sec, or 5 sec after the last enemy dies. Wave n has 3 + n/2 enemies (20% more for each player
//   past the first), from the Depths' roster (a new kind every wave or two); every 5th wave brings a boss (two from
//   wave 60, three from 120). Survivors stay: they pile up. At the cap (30 alive, 10 more a Hero) no more come, and
//   the Heroes are Overwhelmed: 60 sec to get back under it, or the Firepit kills them all.
// - Creatures are at the group's level (dungeon_scaling.cpp), then their health and damage are 35% of a dungeon's at
//   wave 1, 10% more each wave (compounding), health 40% more for each player past the first.
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
    constexpr uint32 ERUPTION_EVERY_MS = 14000, ERUPTION_DELAY_MS = 3000, ERUPTION_TARGETS = 3;
    constexpr float ERUPTION_RADIUS = 6.0f, ERUPTION_DAMAGE = 0.3f;
    constexpr uint32 NOVA_EVERY_MS = 22000, NOVA_DELAY_MS = 4000;
    constexpr float NOVA_RADIUS = 12.0f, NOVA_DAMAGE = 0.5f;
    constexpr float ENRAGE_BELOW = 30.0f, ENRAGE_DAMAGE = 1.5f;
    constexpr uint32 NPC_ERUPTION = 910141;             // titan_firepit.sql: invisible, the eruptions' source
    constexpr uint32 SPELL_FIRE_NOVA = 16079;           // its look only: cast by a friendly source, it hits no one
    constexpr uint32 SPELL_ERUPTION_LOG = 11307;        // the combat log's name for an eruption's hit (Fire Nova)

    constexpr uint32 COUNTDOWN_MS = 10000, WAVE_MS = 60000, BREATHER_MS = 5000, SPAWN_SPREAD_MS = 4000;
    constexpr uint32 TICK_MS = 1000, END_DELAY_MS = 4000, CORPSE_MS = 3000;
    constexpr float FIRST_WAVE = 0.35f, WAVE_GROWTH = 1.1f, GROUP_HEALTH = 0.4f, GROUP_COUNT = 0.2f;
    constexpr uint32 BOSS_EVERY = 5, LOOT_EVERY = 15;
    // The field's cap: 30 enemies alive and 10 more for each Hero (a duo's 50). At it no more come, and the Heroes
    // are Overwhelmed: OVERWHELM_MS to get back under it, or the Firepit takes them all.
    constexpr uint32 MOST_ALIVE_BASE = 30, MOST_ALIVE_A_HERO = 10, OVERWHELM_MS = 60000;
    // A wave's boss: 4 times the health and 1.5 times the damage of the wave's others (unless the dungeon scaling
    // already counts it a boss: a boss_ script or a boss's rank, with its own health).
    constexpr float BOSS_HEALTH = 4.0f, BOSS_DAMAGE = 1.5f;
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
    // The healers (they keep everything that piles up alive): one a wave at most, two alive at once at most.
    constexpr uint32 HEALERS_A_WAVE = 1, MOST_HEALERS_ALIVE = 2;
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
        uint32 wave = 0;
        bool boss = false;
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
        return FIRST_WAVE * std::pow(WAVE_GROWTH, float(std::max<uint32>(wave, 1) - 1));
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
    };

    // A boss's mechanics' timers, and a blast on its way (a warning on the floor, then the hit).
    struct BossState
    {
        uint32 eruptMs;
        uint32 novaMs;
        bool enraged;
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
    // Modifiers (firepit_gear.h MODIFIERS): a spoil's first from wave MODIFIER_FROM (certain MODIFIER_RAMP waves
    // later), its second from SECOND_MODIFIER_FROM, in the random-property slots the suffix leaves free.
    constexpr uint32 MODIFIER_FROM = 30, SECOND_MODIFIER_FROM = 75, MODIFIER_RAMP = 60;
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

    // A Hero's modifier roles (firepit_gear.h ModifierRole).
    uint8 ModifierRolesOf(Profile const& profile)
    {
        switch (profile.role)
        {
            case Wildcard::ROLE_TANK: return ROLE_TANK | ROLE_PHYSICAL;
            case Wildcard::ROLE_HEALER: return ROLE_HEALER;
            default: break;
        }
        return profile.primary == PRIMARY_INTELLECT ? ROLE_CASTER : ROLE_PHYSICAL;
    }

    // The chance (percent) of a spoil earned at a wave having a modifier: 0 at from, certain MODIFIER_RAMP later.
    uint32 ModifierChance(uint32 wave, uint32 from)
    {
        return wave <= from ? 0 : std::min<uint32>(100, (wave - from) * 100 / MODIFIER_RAMP);
    }

    std::vector<uint32> RollModifiers(Player* player, Profile const& profile, bool weapon, uint32 wave)
    {
        std::vector<uint32> pool;
        uint8 roles = ModifierRolesOf(profile);
        for (Modifier const& modifier : MODIFIERS)
            if ((modifier.Roles & roles) && (weapon || !modifier.WeaponOnly)
                && player->GetLevel() >= ERA_LEVEL[modifier.Era])
                pool.push_back(modifier.Enchantment);
        std::vector<uint32> rolled;
        for (uint32 from : { MODIFIER_FROM, SECOND_MODIFIER_FROM })
        {
            if (pool.empty() || !roll_chance_i(int32(ModifierChance(wave, from))))
                break;
            std::size_t pick = urand(0, uint32(pool.size()) - 1);
            rolled.push_back(pool[pick]);
            pool.erase(pool.begin() + pick);
        }
        return rolled;
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
            uint32 entry = PIECES[chosen.piece].First + tier + (titan ? TITAN_OFFSET : 0);
            if (!sObjectMgr->GetItemTemplate(entry))
                continue;
            bool weapon = PIECES[chosen.piece].Class == ITEM_CLASS_WEAPON;
            spoils.push_back({ entry, -int32(titan ? TITAN_SUFFIX[mix] : SUFFIX[mix]), titan,
                RollModifiers(player, profile, weapon, wave) });
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
    // The arena's floor under (x, y), not the stands above it.
    float FloorAt(float x, float y) const
    {
        float z = instance->GetHeight(x, y, CENTER.GetPositionZ() + 5.0f);
        if (z < CENTER.GetPositionZ() - 10.0f || z > CENTER.GetPositionZ() + 5.0f)
            z = CENTER.GetPositionZ() + 1.0f;
        return z;
    }

    void Decorate()
    {
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
        _waveMs = WAVE_MS;
        _breather = false;
        for (Player* player : Living())
            if (Record* record = RecordOf(player))
            {
                record->peakWave = _wave;
                record->peakAlive = uint32(_enemies.size());
            }

        bool bossWave = _wave % BOSS_EVERY == 0;
        float groupCount = 1.0f + GROUP_COUNT * float(_groupSize - 1);
        uint32 count = bossWave ? 0 : uint32(std::lround(float(3 + _wave / 2) * groupCount));   // a boss comes alone

        std::vector<uint32> kinds;
        for (FirepitRoster::Kind const& kind : FirepitRoster::KINDS)
            if (kind.Wave <= _wave)
                kinds.push_back(kind.Entry);
        if (kinds.size() > NEWEST_KINDS)
            kinds.erase(kinds.begin(), kinds.end() - NEWEST_KINDS);

        std::vector<uint32> fighters;
        for (uint32 entry : kinds)
            if (!IsHealer(entry))
                fighters.push_back(entry);
        uint32 healers = 0;
        for (ObjectGuid const& guid : _enemies)
            if (Creature* enemy = instance->GetCreature(guid))
                if (enemy->IsAlive() && IsHealer(enemy->GetEntry()))
                    ++healers;
        uint32 healersNow = 0;
        for (uint32 i = 0; i < count && !kinds.empty(); ++i)
        {
            uint32 entry = i == 0 ? kinds.back() : kinds[urand(0, uint32(kinds.size()) - 1)];
            if (IsHealer(entry) && (healersNow >= HEALERS_A_WAVE || healers + healersNow >= MOST_HEALERS_ALIVE))
            {
                if (fighters.empty())
                    continue;
                entry = fighters[urand(0, uint32(fighters.size()) - 1)];
            }
            if (IsHealer(entry))
                ++healersNow;
            _queue.push_back({ entry, false, SPAWN_SPREAD_MS * i / std::max<uint32>(1, count) });
        }

        std::string text = "Wave " + std::to_string(_wave);
        // A new chapter (its first wave, never a boss's): its dungeon's enemies pour in.
        uint32 chapter = (_wave - 1) / CHAPTER_WAVES;
        if ((_wave - 1) % CHAPTER_WAVES == 0 && chapter < std::size(FirepitRoster::CHAPTERS))
            text += ": the foes of " + std::string(FirepitRoster::CHAPTERS[chapter]) + " pour in!";
        if (bossWave)
        {
            uint32 bosses = 1 + (_wave >= 60 ? 1 : 0) + (_wave >= 120 ? 1 : 0);
            uint32 first = (_wave / BOSS_EVERY - 1) % std::size(BOSSES);
            for (uint32 i = 0; i < bosses; ++i)
            {
                uint32 entry = BOSSES[(first + i * 5) % std::size(BOSSES)];
                _queue.push_back({ entry, true, 0 });
                if (CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(entry))
                    text += (i ? " and " : ": ") + info->Name;
            }
            text += bosses > 1 ? " enter the Ring!" : " enters the Ring!";
        }
        Announce(text, bossWave);
    }

    void SpawnOne(Spawn const& spawn)
    {
        // Too many alive: this one stays away (the field is full enough). They once grew 3% for each kept away,
        // without end: a group behind was buried under ever stronger enemies.
        if (_enemies.size() >= MostAlive())
            return;
        float angle = frand(0.0f, 2.0f * float(M_PI));
        float radius = frand(SPAWN_RADIUS_MIN, SPAWN_RADIUS_MAX);
        float x = CENTER.GetPositionX() + radius * std::cos(angle);
        float y = CENTER.GetPositionY() + radius * std::sin(angle);
        Position at(x, y, FloorAt(x, y), angle + float(M_PI));
        TempSummon* enemy = instance->SummonCreature(spawn.entry, at);
        if (!enemy)
            return;
        Enemy* own = enemy->CustomData.GetDefault<Enemy>("titan_firepit");
        float factor = WaveFactor(_wave);
        own->health = factor * (1.0f + GROUP_HEALTH * float(_groupSize - 1));
        own->damage = factor;
        own->wave = _wave;
        own->boss = spawn.boss;
        if (spawn.boss && enemy->GetScriptName().rfind("boss_", 0) != 0
            && enemy->GetCreatureTemplate()->rank != CREATURE_ELITE_WORLDBOSS)
        {
            own->health *= BOSS_HEALTH;
            own->damage *= BOSS_DAMAGE;
        }
        if (spawn.boss && _wave >= MECHANICS_FROM)
            _bosses[enemy->GetGUID()] = { urand(5000, ERUPTION_EVERY_MS), urand(NOVA_EVERY_MS / 2, NOVA_EVERY_MS),
                false };
        enemy->SetFaction(ENEMY_FACTION);
        enemy->SetReactState(REACT_AGGRESSIVE);
        enemy->SetHomePosition(CENTER);
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
            if (GameObject* marker = Marker(GO_FLAME_CIRCLE, blast.at))
                blast.marker = marker->GetGUID();
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
        if (GameObject* marker = Marker(GO_FIRE_RIFT, blast.at))
            blast.marker = marker->GetGUID();
        boss->SetControlled(true, UNIT_STATE_ROOT);
        _blasts.push_back(blast);
        Announce(boss->GetName() + " gathers a Searing Nova: get away from it!", true);
    }

    void Detonate(Blast const& blast)
    {
        if (GameObject* marker = instance->GetGameObject(blast.marker))
            marker->Delete();
        Creature* boss = instance->GetCreature(blast.source);
        if (blast.nova && boss)
            boss->SetControlled(false, UNIT_STATE_ROOT);
        if (blast.nova && (!boss || !boss->IsAlive()))
            return;                                     // a nova dies with its boss
        Position at = blast.nova ? boss->GetPosition() : blast.at;
        TempSummon* source = instance->SummonCreature(NPC_ERUPTION, at, nullptr, 2000);
        if (source)
        {
            source->SetFaction(35);                     // friendly: its Fire Nova is the look, and hits no one
            source->CastSpell(source, SPELL_FIRE_NOVA, true);
        }
        Unit* attacker = blast.nova ? static_cast<Unit*>(boss) : static_cast<Unit*>(source);
        if (!attacker)
            return;
        SpellInfo const* spell = sSpellMgr->GetSpellInfo(blast.nova ? SPELL_FIRE_NOVA : SPELL_ERUPTION_LOG);
        for (Player* hero : Living())
        {
            if (hero->GetExactDist2d(at.GetPositionX(), at.GetPositionY()) > blast.radius
                || std::abs(hero->GetPositionZ() - at.GetPositionZ()) > 6.0f)
                continue;
            DamageInfo damage(attacker, hero, uint32(float(hero->GetMaxHealth()) * blast.share), spell,
                SPELL_SCHOOL_MASK_FIRE, SPELL_DIRECT_DAMAGE);
            Unit::CalcAbsorbResist(damage);
            attacker->SendSpellNonMeleeDamageLog(hero, spell, damage.GetDamage(), SPELL_SCHOOL_MASK_FIRE,
                damage.GetAbsorb(), damage.GetResist(), false, 0);
            Unit::DealDamage(attacker, hero, damage.GetDamage(), nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_FIRE,
                spell, false);
        }
    }

    void ClearMechanics()
    {
        for (Blast const& blast : _blasts)
        {
            if (GameObject* marker = instance->GetGameObject(blast.marker))
                marker->Delete();
            if (Creature* boss = instance->GetCreature(blast.source))
                boss->SetControlled(false, UNIT_STATE_ROOT);
        }
        _blasts.clear();
        _bosses.clear();
    }

    // At the nearest living Hero.
    void Engage(Creature* enemy)
    {
        Player* nearest = nullptr;
        for (Player* player : Living())
            if (!nearest || enemy->GetExactDist(player) < enemy->GetExactDist(nearest))
                nearest = player;
        if (!nearest || !enemy->AI())
            return;
        enemy->SetInCombatWithZone();
        enemy->AI()->AttackStart(nearest);
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

        // Overwhelmed: at the cap, a minute to get back under it, or the Firepit takes everyone.
        if (_enemies.size() >= MostAlive())
        {
            if (!_overwhelmUntil)
            {
                _overwhelmUntil = _runMs + OVERWHELM_MS;
                Announce("OVERWHELMED! Get under " + std::to_string(MostAlive()) + " enemies within 60 sec, or the "
                    "Firepit takes you all!", true);
            }
            else if (_runMs >= _overwhelmUntil)
            {
                _overwhelmUntil = 0;
                Announce("Overwhelmed: the Firepit takes its challengers.", true);
                for (Player* player : Living())
                {
                    if (Record* record = RecordOf(player))
                        record->killer = "Overwhelmed";
                    Unit::Kill(player, player, false);
                }
            }
        }
        else if (_overwhelmUntil)
        {
            _overwhelmUntil = 0;
            Announce("Back under the cap: the Firepit relents.");
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
            _phase = Phase::ENDING;
            ClearMechanics();
            _timer = END_DELAY_MS;
            return;
        }

        // A cleared wave: the next in 5 sec.
        if (_enemies.empty() && _queue.empty() && !_breather)
        {
            _breather = true;
            _waveMs = std::min(_waveMs, BREATHER_MS);
        }
        if (!_waveMs)
            StartWave(_wave + 1);

        for (auto const& ref : instance->GetPlayers())
            if (Player* player = ref.GetSource())
                SendHud(player);
    }

    // The field's cap (MOST_ALIVE_*).
    uint32 MostAlive() const
    {
        return MOST_ALIVE_BASE + MOST_ALIVE_A_HERO * _groupSize;
    }

    void SendHud(Player* player)
    {
        uint32 living = uint32(Living().size());
        uint32 overwhelm = _overwhelmUntil > _runMs ? (_overwhelmUntil - _runMs + 999) / 1000 : 0;
        SendAddon(player, "H," + std::to_string(_wave) + ',' + std::to_string((_waveMs + 999) / 1000) + ','
            + std::to_string(_enemies.size()) + ',' + std::to_string(_wave && _wave % BOSS_EVERY == 0 ? 1 : 0) + ','
            + std::to_string(living) + ',' + std::to_string(_records.size()) + ','
            + std::to_string(_phase == Phase::COUNTDOWN ? (_timer + 999) / 1000 : 0) + ','
            + std::to_string(overwhelm) + ',' + std::to_string(MostAlive()));
    }

    void Finish()
    {
        _phase = Phase::DONE;
        for (ObjectGuid const& guid : _enemies)
            if (Creature* enemy = instance->GetCreature(guid))
                enemy->DespawnOrUnsummon();
        _enemies.clear();
        _queue.clear();

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
            if (!record.peakWave)
                continue;
            trans->Append("INSERT INTO `titan_firepit_records` (`guid`, `bracket`, `best_wave`, `group_size`, "
                "`kills`, `run_date`) VALUES ({}, {}, {}, {}, {}, UNIX_TIMESTAMP()) ON DUPLICATE KEY UPDATE "
                "`group_size` = IF(VALUES(`best_wave`) > `best_wave`, VALUES(`group_size`), `group_size`), "
                "`kills` = IF(VALUES(`best_wave`) > `best_wave`, VALUES(`kills`), `kills`), "
                "`run_date` = IF(VALUES(`best_wave`) > `best_wave`, VALUES(`run_date`), `run_date`), "
                "`best_wave` = GREATEST(`best_wave`, VALUES(`best_wave`))",
                guid.GetCounter(), Bracket(_groupSize), record.peakWave, _groupSize, record.kills);
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
    uint32 _overwhelmUntil = 0;                     // run ms the Heroes must be under the cap by (0: not overwhelmed)
    bool _breather = false;
    std::unordered_map<ObjectGuid, BossState> _bosses;
    std::vector<Blast> _blasts;
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
    return own ? own->damage : 1.0f;
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
    titan_firepit_unit() : UnitScript("titan_firepit_unit", true, { UNITHOOK_ON_DAMAGE, UNITHOOK_ON_HEAL }) { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (!attacker || !victim || !damage)
            return;
        FirepitInstance* run = RunOf(victim->GetMap());
        if (!run)
            return;
        if (Player* player = attacker->GetCharmerOrOwnerPlayerOrPlayerItself(); player && victim->ToCreature())
            run->Damage(player, damage);
        if (Player* player = victim->ToPlayer(); player && attacker->ToCreature()
            && !attacker->GetCharmerOrOwnerPlayerOrPlayerItself())
            run->Taken(player, damage);
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
