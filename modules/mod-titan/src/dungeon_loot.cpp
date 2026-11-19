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

#include "dungeon_gear.h"
#include "scaled_gear.h"
#include "DBCStores.h"
#include "Item.h"
#include "ItemEnchantmentMgr.h"
#include "Log.h"
#include "LootMgr.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

// Dungeon drops at the party's level. Dungeons scale their creatures to the lowest player's level
// (dungeon_scaling.cpp), and their loot follows:
// - a dungeon's own items (Dungeon Scaled gear: tools/scaling/dungeon_gear.py lists them, dungeon_gear.h) drop as
//   their copy at that level, up or down (at its own level too): the same item at the level's item level
//   (LEVEL_ITEM_LEVEL over it, a green's there, as Leveling Scaled gear's) plus DUNGEON_ITEM_LEVELS, its stats,
//   armor, weapon damage and block scaled from its own item level to that one. The copies are made at start-up
//   (one per item and level, 1-60);
// - any other piece (the world's random greens) that asks for a higher level than that is swapped for a Leveling
//   Scaled piece (scaled_gear.h, the Smoldering Adventurer's Cache's pool) of the same slot and armor or weapon type
//   at that level, with a random suffix as its template gives. A piece with no such match (a relic, a thrown weapon,
//   a shirt) stays as it is.
namespace
{
    constexpr int32 LEVEL_ITEM_LEVEL = 5;           // a level's own item level over it (tools/scaling/scaled_gear.py)
    constexpr int32 DUNGEON_ITEM_LEVELS = 15;       // what a dungeon's own item is worth over that

    // What a piece is, for the match: the slot (a robe is a chest) and, for armor, its type; for a weapon, its kind
    // (a crossbow is a bow). Cloaks, necks, rings, trinkets and off-hands match whatever their type. 0: no match.
    uint32 Kind(ItemTemplate const* proto)
    {
        if (proto->Class == ITEM_CLASS_WEAPON)
        {
            uint32 subClass = proto->SubClass == ITEM_SUBCLASS_WEAPON_CROSSBOW ? ITEM_SUBCLASS_WEAPON_BOW
                : proto->SubClass;
            return 0x10000 | subClass;
        }
        if (proto->Class != ITEM_CLASS_ARMOR)
            return 0;
        uint32 slot = proto->InventoryType == INVTYPE_ROBE ? INVTYPE_CHEST : proto->InventoryType;
        switch (slot)
        {
            case INVTYPE_CLOAK:
            case INVTYPE_NECK:
            case INVTYPE_FINGER:
            case INVTYPE_TRINKET:
            case INVTYPE_HOLDABLE:
            case INVTYPE_SHIELD:
                return slot << 8;
            default:
                break;
        }
        if (proto->SubClass < ITEM_SUBCLASS_ARMOR_CLOTH || proto->SubClass > ITEM_SUBCLASS_ARMOR_PLATE)
            return 0;
        return (slot << 8) | proto->SubClass;
    }

    // The Leveling Scaled pieces by kind (their level 1 template's). Built on first use.
    std::unordered_map<uint32, std::vector<ScaledGear::Item const*>> const& Pool()
    {
        static std::unordered_map<uint32, std::vector<ScaledGear::Item const*>> const pool = []
        {
            std::unordered_map<uint32, std::vector<ScaledGear::Item const*>> items;
            for (ScaledGear::Item const& scaled : ScaledGear::ITEMS)
                if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(scaled.First))
                    if (uint32 kind = Kind(proto))
                        items[kind].push_back(&scaled);
            return items;
        }();
        return pool;
    }

    // Dungeon Scaled gear: each listed item's index (its copies' entries).
    std::unordered_map<uint32, uint32> const& DungeonIndex()
    {
        static std::unordered_map<uint32, uint32> const index = []
        {
            std::unordered_map<uint32, uint32> items;
            for (uint32 i = 0; i < std::size(DungeonGear::ITEMS); ++i)
                items.emplace(DungeonGear::ITEMS[i], i);
            return items;
        }();
        return index;
    }

    // A quality's stat budget at an item level (RandPropPoints.dbc, the head and chest's: every slot's is the same
    // share of it).
    float Budget(uint32 itemLevel, uint32 quality)
    {
        RandomPropertiesPointsEntry const* points = sRandomPropertiesPointsStore.LookupEntry(itemLevel);
        if (!points)
            return 0.0f;
        switch (quality)
        {
            case ITEM_QUALITY_EPIC: return float(points->EpicPropertiesPoints[0]);
            case ITEM_QUALITY_RARE: return float(points->RarePropertiesPoints[0]);
            default: return float(points->UncommonPropertiesPoints[0]);
        }
    }

    int32 Scaled(int32 value, float ratio)
    {
        if (!value)
            return 0;
        int32 scaled = int32(std::lround(value * ratio));
        return value > 0 ? std::max(1, scaled) : std::min(-1, scaled);
    }

    // An item's copy for a level: required level that level, item level the level's plus DUNGEON_ITEM_LEVELS, stats
    // and block by the stat budget's ratio, armor and damage by the item level's (as tools/squish/squish.py does).
    ItemTemplate DungeonCopy(ItemTemplate const& proto, uint32 entry, uint32 level)
    {
        ItemTemplate copy = proto;
        uint32 itemLevel = uint32(int32(level) + LEVEL_ITEM_LEVEL + DUNGEON_ITEM_LEVELS);
        float levelRatio = proto.ItemLevel ? float(itemLevel) / float(proto.ItemLevel) : 1.0f;
        float own = Budget(proto.ItemLevel, proto.Quality);
        float statRatio = own > 0.0f ? Budget(itemLevel, proto.Quality) / own : levelRatio;
        if (statRatio <= 0.0f)
            statRatio = levelRatio;
        copy.ItemId = entry;
        copy.ItemLevel = itemLevel;
        copy.RequiredLevel = level;
        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
            copy.ItemStat[i].ItemStatValue = Scaled(proto.ItemStat[i].ItemStatValue, statRatio);
        for (uint32 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            copy.Damage[i].DamageMin = proto.Damage[i].DamageMin * levelRatio;
            copy.Damage[i].DamageMax = proto.Damage[i].DamageMax * levelRatio;
        }
        copy.Armor = uint32(std::max<int32>(0, Scaled(int32(proto.Armor), levelRatio)));
        copy.Block = uint32(std::max<int32>(0, Scaled(int32(proto.Block), statRatio)));
        copy.SellPrice = uint32(std::lround(proto.SellPrice * statRatio));
        copy.BuyPrice = int32(std::lround(proto.BuyPrice * statRatio));
        return copy;
    }

    // The level the dungeon is at: its lowest player's (game masters aside), as dungeon_scaling.cpp scales it.
    uint8 PartyLevel(Map* map, Player const* owner)
    {
        uint8 lowest = 0;
        for (auto const& ref : map->GetPlayers())
            if (Player* player = ref.GetSource())
                if (!player->IsGameMaster() && (!lowest || player->GetLevel() < lowest))
                    lowest = player->GetLevel();
        return lowest ? lowest : owner->GetLevel();
    }
}

class titan_dungeon_loot : public MiscScript
{
public:
    titan_dungeon_loot() : MiscScript("titan_dungeon_loot", { MISCHOOK_ON_AFTER_LOOT_TEMPLATE_PROCESS }) { }

    void OnAfterLootTemplateProcess(Loot* loot, LootTemplate const* /*tab*/, LootStore const& store, Player* lootOwner,
        bool /*personal*/, bool /*noEmptyError*/, uint16 /*lootMode*/) override
    {
        if (!loot || !lootOwner || (&store != &LootTemplates_Creature && &store != &LootTemplates_Gameobject))
            return;
        Map* map = lootOwner->GetMap();
        if (!map || !map->IsDungeon())
            return;

        uint8 level = PartyLevel(map, lootOwner);
        for (LootItem& item : loot->items)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(item.itemid);
            if (!proto)
                continue;
            // A dungeon's own item: its copy at the party's level.
            auto own = DungeonIndex().find(item.itemid);
            if (own != DungeonIndex().end())
            {
                uint32 entry = DungeonGear::FIRST + own->second * DungeonGear::LEVELS +
                    std::clamp<uint32>(level, 1, DungeonGear::LEVELS) - 1;
                if (sObjectMgr->GetItemTemplate(entry))
                {
                    item.itemid = entry;
                    item.randomPropertyId = Item::GenerateItemRandomPropertyId(entry);
                    item.randomSuffix = GenerateEnchSuffixFactor(entry);
                }
                continue;
            }
            if (proto->RequiredLevel <= level)
                continue;
            uint32 kind = Kind(proto);
            auto pieces = kind ? Pool().find(kind) : Pool().end();
            if (pieces == Pool().end() || pieces->second.empty())
                continue;
            ScaledGear::Item const* scaled = pieces->second[urand(0, uint32(pieces->second.size()) - 1)];
            uint32 entry = ScaledGear::Entry(*scaled, level);
            if (!sObjectMgr->GetItemTemplate(entry))
                continue;
            item.itemid = entry;
            item.randomPropertyId = Item::GenerateItemRandomPropertyId(entry);
            item.randomSuffix = GenerateEnchSuffixFactor(entry);
        }
    }
};

// Dungeon Scaled gear's copies, made once the item templates are loaded (before any player's items are).
class titan_dungeon_gear : public WorldScript
{
public:
    titan_dungeon_gear() : WorldScript("titan_dungeon_gear", { WORLDHOOK_ON_BEFORE_WORLD_INITIALIZED }) { }

    void OnBeforeWorldInitialized() override
    {
        std::vector<ItemTemplate> copies;
        copies.reserve(std::size(DungeonGear::ITEMS) * DungeonGear::LEVELS);
        uint32 missing = 0;
        for (uint32 i = 0; i < std::size(DungeonGear::ITEMS); ++i)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(DungeonGear::ITEMS[i]);
            if (!proto)
            {
                ++missing;
                continue;
            }
            for (uint32 level = 1; level <= DungeonGear::LEVELS; ++level)
                copies.push_back(DungeonCopy(*proto, DungeonGear::FIRST + i * DungeonGear::LEVELS + level - 1, level));
        }
        sObjectMgr->AddItemTemplates(copies);
        LOG_INFO("module", "mod-titan: {} Dungeon Scaled items ({} copies; {} not in item_template)",
            std::size(DungeonGear::ITEMS) - missing, copies.size(), missing);
    }
};

void AddSC_dungeon_loot()
{
    new titan_dungeon_loot();
    new titan_dungeon_gear();
}
