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
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "DisableMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Tokenize.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <string_view>
#include <tuple>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Server side of the Dungeon Journal (client addon: client-patch/AddOns/Emberforge_Journal): every dungeon and raid
// of the game's dungeon list (LFGDungeons.dbc, its dungeons and raids, each difficulty an entry; the holidays' left
// out), with the levels of the creatures spawned in it as the world database has them (after the level squish), its
// size, and its bosses (DungeonEncounter.dbc).
//
// Addon whispers to and from the player themself, prefix EFDJ:
//   client -> server   LIST
//                      BOSS,<map>,<difficulty>             one instance's bosses: their details, abilities, loot
//   server -> client   B                                   begin
//                      D,<lfg id>~<map>~<type>~<expansion>~<difficulty>~<players>~<creature min>~<creature max>
//                        ~<queue min>~<queue max>~<seasonal>~<name>;...      instances (type 1 dungeon, 2 raid)
//                      F,<map>~<difficulty>~<boss>~<creature entry>;...   bosses, in the game's order (the entry, for
//                                                          its model: the creature of that name spawned there, 0)
//                      W,<lfg id>~<first boss>~<last boss>;...   a wing's bosses (Scarlet Monastery's Cathedral...):
//                                                          their indexes from 1 in its map's list; from the
//                                                          encounters that end a dungeon (instance_encounters)
//                      Z                                   end
//                      I,<boss>~<entry>~<level>~<rank>~<health>~<creature type>;...   (boss: its index from 1)
//                      A,<boss>~<spell>;...                its abilities: the spells of its C++ script's file
//                                                          (titan_journal_abilities) and its SmartAI casts
//                      L,<boss>~<item>~<quality>~<inventory type>~<class>~<subclass>~<name>;...   its loot (uncommon
//                                                          and up: gear; rare and up: anything but quest items)
//                      Y,<map>~<difficulty>                that instance done
// LIST also sends the client every boss's creature query response, so their models draw (the client only shows
// models of creatures it knows).
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

    // A boss's creature: the one of its name (any case) spawned on its map, else any of its name (0: none).
    uint32 BossEntry(uint32 mapId, std::string const& name)
    {
        // By name in lowercase: the encounters' names and the creatures' differ in case (Rhahk'zor, Rhahk'Zor).
        auto lower = [](std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
            return text;
        };
        static std::unordered_map<std::string, std::vector<uint32>> const byName = [&lower]
        {
            std::unordered_map<std::string, std::vector<uint32>> result;
            for (auto const& [entry, creature] : *sObjectMgr->GetCreatureTemplates())
                result[lower(creature.Name)].push_back(entry);
            return result;
        }();
        static std::unordered_set<uint64> const spawned = []
        {
            std::unordered_set<uint64> result;
            for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
                result.insert((uint64(data.mapid) << 32) | data.id);
            return result;
        }();

        auto itr = byName.find(lower(name));
        if (itr == byName.end())
            return 0;
        for (uint32 entry : itr->second)
            if (spawned.count((uint64(mapId) << 32) | entry))
                return entry;
        return itr->second.front();
    }

    // The encounters of a map and difficulty, in their order.
    std::vector<DungeonEncounterEntry const*> Encounters(uint32 mapId, uint32 difficulty)
    {
        std::vector<DungeonEncounterEntry const*> encounters;
        for (uint32 i = 0; i < sDungeonEncounterStore.GetNumRows(); ++i)
            if (DungeonEncounterEntry const* encounter = sDungeonEncounterStore.LookupEntry(i))
                if (encounter->mapId == mapId && encounter->difficulty == difficulty)
                    encounters.push_back(encounter);
        std::sort(encounters.begin(), encounters.end(),
            [](DungeonEncounterEntry const* a, DungeonEncounterEntry const* b)
            {
                return a->encounterIndex < b->encounterIndex;
            });
        return encounters;
    }

    // Spells by C++ script name (titan_journal_abilities) and by SmartAI creature (its casts). Loaded once.
    std::unordered_map<std::string, std::vector<uint32>> const& ScriptSpells()
    {
        static std::unordered_map<std::string, std::vector<uint32>> const spells = []
        {
            std::unordered_map<std::string, std::vector<uint32>> result;
            if (QueryResult rows = WorldDatabase.Query("SELECT `script`, `spell` FROM `titan_journal_abilities`"))
                do
                    result[(*rows)[0].Get<std::string>()].push_back((*rows)[1].Get<uint32>());
                while (rows->NextRow());
            return result;
        }();
        return spells;
    }

    std::unordered_map<uint32, std::vector<uint32>> const& SmartSpells()
    {
        static std::unordered_map<uint32, std::vector<uint32>> const spells = []
        {
            std::unordered_map<uint32, std::vector<uint32>> result;
            // source_type 0: a creature; action_type 11: cast.
            if (QueryResult rows = WorldDatabase.Query("SELECT DISTINCT `entryorguid`, `action_param1` FROM "
                "`smart_scripts` WHERE `source_type` = 0 AND `action_type` = 11 AND `entryorguid` > 0"))
                do
                    result[(*rows)[0].Get<int32>()].push_back((*rows)[1].Get<uint32>());
                while (rows->NextRow());
            return result;
        }();
        return spells;
    }

    // Loot tables: an entry's items and references (creature_loot_template, reference_loot_template). Loaded once.
    struct LootRows
    {
        std::unordered_map<uint32, std::vector<std::pair<uint32, uint32>>> creature, reference;
    };

    LootRows const& Loot()
    {
        static LootRows const loot = []
        {
            LootRows result;
            for (auto [table, rows] : { std::make_pair("creature_loot_template", &result.creature),
                std::make_pair("reference_loot_template", &result.reference) })
                if (QueryResult query = WorldDatabase.Query(Acore::StringFormat(
                    "SELECT `Entry`, `Item`, `Reference` FROM `{}` WHERE `QuestRequired` = 0", table)))
                    do
                        (*rows)[(*query)[0].Get<uint32>()].emplace_back((*query)[1].Get<uint32>(),
                            (*query)[2].Get<uint32>());
                    while (query->NextRow());
            return result;
        }();
        return loot;
    }

    void CollectLoot(std::unordered_map<uint32, std::vector<std::pair<uint32, uint32>>> const& table, uint32 entry,
        uint8 depth, std::vector<uint32>& items)
    {
        auto itr = table.find(entry);
        if (itr == table.end() || depth > 4)
            return;
        for (auto const& [item, reference] : itr->second)
        {
            if (reference)
                CollectLoot(Loot().reference, reference, depth + 1, items);
            else if (std::find(items.begin(), items.end(), item) == items.end())
                items.push_back(item);
        }
    }

    // Worth showing: gear of uncommon quality and up, anything else rare and up (mounts, patterns), never quest items.
    bool Shown(ItemTemplate const* item)
    {
        if (!item || item->Class == ITEM_CLASS_QUEST)
            return false;
        bool gear = item->Class == ITEM_CLASS_WEAPON || item->Class == ITEM_CLASS_ARMOR;
        return item->Quality >= (gear ? ITEM_QUALITY_UNCOMMON : ITEM_QUALITY_RARE)
            && item->Quality <= ITEM_QUALITY_ARTIFACT;
    }

    void SendBosses(Player* player, uint32 mapId, uint32 difficulty)
    {
        std::vector<std::string> details, abilities, loot;
        uint32 index = 0;
        for (DungeonEncounterEntry const* encounter : Encounters(mapId, difficulty))
        {
            ++index;
            char const* name = encounter->encounterName[LOCALE_enUS] ? encounter->encounterName[LOCALE_enUS] : "";
            CreatureTemplate const* base = sObjectMgr->GetCreatureTemplate(BossEntry(mapId, name));
            if (!base)
                continue;
            // The difficulty's own creature (its Heroic or 25-player version), else the base one.
            CreatureTemplate const* creature = base;
            uint32 versionEntry = difficulty > 0 && difficulty <= MAX_DIFFICULTY - 1
                ? base->DifficultyEntry[difficulty - 1] : 0;
            if (versionEntry)
                if (CreatureTemplate const* version = sObjectMgr->GetCreatureTemplate(versionEntry))
                    creature = version;

            uint32 health = 0;
            CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(creature->maxlevel, creature->unit_class);
            if (stats)
                health = stats->GenerateHealth(creature);
            details.push_back(Acore::StringFormat("{}~{}~{}~{}~{}~{}", index, creature->Entry, creature->maxlevel,
                creature->rank, health, creature->type));

            std::vector<uint32> spells;
            auto addSpells = [&](std::vector<uint32> const& list)
            {
                for (uint32 spell : list)
                    if (std::find(spells.begin(), spells.end(), spell) == spells.end())
                        spells.push_back(spell);
            };
            auto script = ScriptSpells().find(sObjectMgr->GetScriptName(base->ScriptID));
            if (script != ScriptSpells().end())
                addSpells(script->second);
            auto smart = SmartSpells().find(base->Entry);
            if (smart != SmartSpells().end())
                addSpells(smart->second);
            for (uint32 spell : spells)
                abilities.push_back(Acore::StringFormat("{}~{}", index, spell));

            std::vector<uint32> items;
            CollectLoot(Loot().creature, creature->lootid ? creature->lootid : creature->Entry, 0, items);
            uint32 shown = 0;
            for (uint32 itemId : items)
            {
                ItemTemplate const* item = sObjectMgr->GetItemTemplate(itemId);
                if (!Shown(item) || ++shown > 60)
                    continue;
                loot.push_back(Acore::StringFormat("{}~{}~{}~{}~{}~{}~{}", index, item->ItemId, item->Quality,
                    item->InventoryType, item->Class, item->SubClass, Clean(item->Name1.c_str())));
            }
        }
        SendBatched(player, "I,", details);
        SendBatched(player, "A,", abilities);
        SendBatched(player, "L,", loot);
        SendAddon(player, Acore::StringFormat("Y,{}~{}", mapId, difficulty));
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
            // Holiday dungeons (the Headless Horseman, Coren Direbrew, the Frost Lord Ahune...) are left out.
            if (dungeon->Flags & LFG_FLAG_SEASONAL)
                continue;
            // Closed instances (`disables`, a map: data/sql/db-world/titan_retired_dungeons.sql) are left out.
            if (sDisableMgr->IsDisabledFor(DISABLE_TYPE_MAP, dungeon->MapID, nullptr,
                uint8(dungeon->Difficulty)))
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
        // The encounters that end a dungeon of the finder (its last boss): a map's wings, in its order.
        std::unordered_map<uint32, uint32> lastOf;      // encounter -> the dungeon it ends
        for (auto const& listed : maps)
            if (DungeonEncounterList const* list = sObjectMgr->GetDungeonEncounterList(listed.first.first,
                Difficulty(listed.first.second)))
                for (DungeonEncounter const* encounter : *list)
                    if (encounter->lastEncounterDungeon)
                        lastOf[encounter->dbcEntry->id] = encounter->lastEncounterDungeon;

        std::vector<std::string> bosses, wings;
        std::unordered_set<uint32> known;
        std::pair<uint32, uint32> current = { UINT32_MAX, UINT32_MAX };
        uint32 index = 0, wingStart = 1;
        for (DungeonEncounterEntry const* encounter : encounters)
        {
            if (std::make_pair(encounter->mapId, encounter->difficulty) != current)
            {
                current = { encounter->mapId, encounter->difficulty };
                index = 0;
                wingStart = 1;
            }
            ++index;
            auto last = lastOf.find(encounter->id);
            if (last != lastOf.end())
            {
                wings.push_back(Acore::StringFormat("{}~{}~{}", last->second, wingStart, index));
                wingStart = index + 1;
            }
            char const* name = encounter->encounterName[LOCALE_enUS] ? encounter->encounterName[LOCALE_enUS] : "";
            uint32 entry = BossEntry(encounter->mapId, name);
            bosses.push_back(Acore::StringFormat("{}~{}~{}~{}", encounter->mapId, encounter->difficulty, Clean(name),
                entry));
            // The client learns the boss (and its other difficulties' versions) so it can draw its model.
            if (CreatureTemplate const* creature = sObjectMgr->GetCreatureTemplate(entry))
                for (uint32 version : { entry, creature->DifficultyEntry[0], creature->DifficultyEntry[1],
                    creature->DifficultyEntry[2] })
                    if (version && known.insert(version).second)
                        if (CreatureTemplate const* versionTemplate = sObjectMgr->GetCreatureTemplate(version))
                            player->SendDirectMessage(&versionTemplate->queryData);
        }

        SendAddon(player, "B");
        SendBatched(player, "D,", instances);
        SendBatched(player, "F,", bosses);
        SendBatched(player, "W,", wings);
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
        std::string_view request = std::string_view(msg).substr(prefix.size());
        if (request == "LIST")
            SendJournal(player);
        else if (request.starts_with("BOSS,"))
        {
            std::vector<std::string_view> parts = Acore::Tokenize(request.substr(5), ',', false);
            Optional<uint32> mapId = parts.size() == 2 ? Acore::StringTo<uint32>(parts[0]) : std::nullopt;
            Optional<uint32> difficulty = parts.size() == 2 ? Acore::StringTo<uint32>(parts[1]) : std::nullopt;
            if (mapId && difficulty)
                SendBosses(player, *mapId, *difficulty);
        }
        return false;
    }
};

void AddSC_journal()
{
    new titan_journal();
}
