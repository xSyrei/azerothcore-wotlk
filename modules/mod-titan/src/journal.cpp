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

#include "Chat.h"
#include "DBCStores.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StringFormat.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <algorithm>
#include <map>
#include <string_view>
#include <tuple>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Server side of the Dungeon Journal (client addon: client-patch/AddOns/Emberforge_Journal): every dungeon and raid
// of the game's dungeon list (LFGDungeons.dbc, its dungeons and raids, each difficulty an entry), with the levels of
// the creatures spawned in it as the world database has them (after the level squish), its size, and its bosses
// (DungeonEncounter.dbc).
//
// Addon whispers to and from the player themself, prefix EFDJ:
//   client -> server   LIST
//   server -> client   B                                   begin
//                      D,<lfg id>~<map>~<type>~<expansion>~<difficulty>~<players>~<creature min>~<creature max>
//                        ~<queue min>~<queue max>~<seasonal>~<name>;...      instances (type 1 dungeon, 2 raid)
//                      F,<map>~<difficulty>~<boss>~<creature entry>;...   bosses, in the game's order (the entry, for
//                                                          its model: the creature of that name spawned there, 0)
//                      Z                                   end
namespace
{
    constexpr char const* PREFIX = "EFDJ";
    constexpr std::size_t MAX_PAYLOAD = 230;
    constexpr uint32 LFG_TYPE_DUNGEON = 1;
    constexpr uint32 LFG_TYPE_RAID = 2;
    constexpr uint32 LFG_FLAG_SEASONAL = 0x4;

    void SendAddon(Player* player, std::string const& payload)
    {
        WorldPacket data;
        std::string const message = std::string(PREFIX) + '\t' + payload;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, message);
        player->SendDirectMessage(&data);
    }

    void SendBatched(Player* player, std::string const& head, std::vector<std::string> const& items)
    {
        std::string payload = head;
        for (std::string const& item : items)
        {
            if (payload.size() > head.size() && payload.size() + 1 + item.size() > MAX_PAYLOAD)
            {
                SendAddon(player, payload);
                payload = head;
            }
            if (payload.size() > head.size())
                payload += ';';
            payload += item;
        }
        if (payload.size() > head.size())
            SendAddon(player, payload);
    }

    // Names travel between separators: theirs out.
    std::string Clean(char const* text)
    {
        std::string name = text ? text : "";
        std::replace(name.begin(), name.end(), ';', ',');
        std::replace(name.begin(), name.end(), '~', '-');
        return name;
    }

    // The lowest and highest level of the creatures spawned on each map (critters, level 1 and placeholder levels
    // above 83 left out). Built on first use, after the world is loaded.
    std::unordered_map<uint32, std::pair<uint8, uint8>> const& MapLevels()
    {
        static std::unordered_map<uint32, std::pair<uint8, uint8>> const levels = []
        {
            std::unordered_map<uint32, std::pair<uint8, uint8>> result;
            for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
            {
                CreatureTemplate const* creature = sObjectMgr->GetCreatureTemplate(data.id);
                if (!creature || creature->type == CREATURE_TYPE_CRITTER || creature->maxlevel < 2
                    || creature->maxlevel > 83)
                    continue;
                auto [itr, isNew] = result.emplace(data.mapid, std::make_pair(creature->minlevel, creature->maxlevel));
                if (!isNew)
                {
                    itr->second.first = std::min(itr->second.first, creature->minlevel);
                    itr->second.second = std::max(itr->second.second, creature->maxlevel);
                }
            }
            return result;
        }();
        return levels;
    }

    // A boss's creature: the one of its name spawned on its map, else any of its name (0: none).
    uint32 BossEntry(uint32 mapId, std::string const& name)
    {
        static std::unordered_map<std::string, std::vector<uint32>> const byName = []
        {
            std::unordered_map<std::string, std::vector<uint32>> result;
            for (auto const& [entry, creature] : *sObjectMgr->GetCreatureTemplates())
                result[creature.Name].push_back(entry);
            return result;
        }();
        static std::unordered_set<uint64> const spawned = []
        {
            std::unordered_set<uint64> result;
            for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
                result.insert((uint64(data.mapid) << 32) | data.id);
            return result;
        }();

        auto itr = byName.find(name);
        if (itr == byName.end())
            return 0;
        for (uint32 entry : itr->second)
            if (spawned.count((uint64(mapId) << 32) | entry))
                return entry;
        return itr->second.front();
    }

    void SendJournal(Player* player)
    {
        std::vector<std::string> instances;
        std::map<std::pair<uint32, uint32>, bool> maps;     // (map, difficulty) listed
        for (uint32 i = 0; i < sLFGDungeonStore.GetNumRows(); ++i)
        {
            LFGDungeonEntry const* dungeon = sLFGDungeonStore.LookupEntry(i);
            if (!dungeon || (dungeon->TypeID != LFG_TYPE_DUNGEON && dungeon->TypeID != LFG_TYPE_RAID))
                continue;

            MapDifficulty const* difficulty = GetMapDifficultyData(dungeon->MapID, Difficulty(dungeon->Difficulty));
            auto levels = MapLevels().find(dungeon->MapID);
            uint8 low = levels != MapLevels().end() ? levels->second.first : 0;
            uint8 high = levels != MapLevels().end() ? levels->second.second : 0;
            instances.push_back(Acore::StringFormat("{}~{}~{}~{}~{}~{}~{}~{}~{}~{}~{}~{}", dungeon->ID, dungeon->MapID,
                dungeon->TypeID, dungeon->ExpansionLevel, dungeon->Difficulty, difficulty ? difficulty->maxPlayers : 0,
                low, high, dungeon->MinLevel, dungeon->MaxLevel, (dungeon->Flags & LFG_FLAG_SEASONAL) ? 1 : 0,
                Clean(dungeon->Name[LOCALE_enUS])));
            maps[{ dungeon->MapID, dungeon->Difficulty }] = true;
        }

        // Bosses of the listed instances, in their encounter order.
        std::vector<DungeonEncounterEntry const*> encounters;
        for (uint32 i = 0; i < sDungeonEncounterStore.GetNumRows(); ++i)
            if (DungeonEncounterEntry const* encounter = sDungeonEncounterStore.LookupEntry(i))
                if (maps.count({ encounter->mapId, encounter->difficulty }))
                    encounters.push_back(encounter);
        std::sort(encounters.begin(), encounters.end(),
            [](DungeonEncounterEntry const* a, DungeonEncounterEntry const* b)
            {
                return std::tie(a->mapId, a->difficulty, a->encounterIndex)
                    < std::tie(b->mapId, b->difficulty, b->encounterIndex);
            });
        std::vector<std::string> bosses;
        for (DungeonEncounterEntry const* encounter : encounters)
        {
            char const* name = encounter->encounterName[LOCALE_enUS] ? encounter->encounterName[LOCALE_enUS] : "";
            bosses.push_back(Acore::StringFormat("{}~{}~{}~{}", encounter->mapId, encounter->difficulty, Clean(name),
                BossEntry(encounter->mapId, name)));
        }

        SendAddon(player, "B");
        SendBatched(player, "D,", instances);
        SendBatched(player, "F,", bosses);
        SendAddon(player, "Z");
    }
}

class titan_journal : public PlayerScript
{
public:
    titan_journal() : PlayerScript("titan_journal", {
        PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT
    }) { }

    // Requests from the addon: whispers to oneself in the addon language with our prefix, handled here and never
    // delivered.
    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 language, std::string& msg,
        Player* receiver) override
    {
        std::string const prefix = std::string(PREFIX) + '\t';
        if (language != LANG_ADDON || receiver != player || msg.compare(0, prefix.size(), prefix) != 0)
            return true;
        if (std::string_view(msg).substr(prefix.size()) == "LIST")
            SendJournal(player);
        return false;
    }
};

void AddSC_journal()
{
    new titan_journal();
}
