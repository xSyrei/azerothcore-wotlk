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

#include "scaled_gear.h"
#include "Item.h"
#include "ItemEnchantmentMgr.h"
#include "LootMgr.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include <unordered_map>
#include <vector>

// Dungeon drops at the party's level. Dungeons scale their creatures to the lowest player's level
// (dungeon_scaling.cpp), but their loot kept the dungeon's own level: gear the Heroes could not wear for many levels.
// A piece of a dungeon's creature or chest loot that asks for a higher level than that is swapped for a Leveling
// Scaled piece (scaled_gear.h, the Smoldering Adventurer's Cache's pool) of the same slot and armor or weapon type at
// that level, with a random suffix as its template gives. A piece with no such match (a relic, a thrown weapon, a
// shirt) stays as it is.
namespace
{
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
            if (!proto || proto->RequiredLevel <= level)
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

void AddSC_dungeon_loot()
{
    new titan_dungeon_loot();
}
