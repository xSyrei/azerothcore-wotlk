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

#include "wildcard.h"
#include "Chat.h"
#include "DBCStores.h"
#include "Containers.h"
#include "Item.h"
#include "ItemEnchantmentMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include <algorithm>
#include <array>
#include <optional>
#include <unordered_map>

// Smoldering Adventurer's Cache (data/sql/db-world/titan_smoldering_cache.sql): a Hero gets one for every level gained
// and every quest turned in. Opening one gives a piece of Blizzard gear for the Hero's level in the slot of their
// lowest item level piece (an empty slot counts as 0; the next one up when that slot has nothing for their level),
// picked for their role (Wildcard::Role):
//
// - from the uncommon (green) items for that slot whose required level is just under the Hero's, with stats only (no
//   "Equip:", "Use:" or "Chance on hit:", their random enchantment too), that the Hero can use;
// - scored by the stats the role wants: Melee strength, agility, attack power and weapon damage; Caster intellect
//   and spell power; Healer intellect, spirit, spell power and mana regeneration; Tank stamina, strength, armor and
//   avoidance; no role a bit of everything. Armor type does not matter; the scores decide (plate wins a tank's for
//   its armor). One of the three best is given.
//
// Gear a Hero gets (loot, quest rewards, purchases, caches) goes on by itself when its slot is empty.
namespace
{
    constexpr uint32 ITEM_SMOLDERING_CACHE = 910005;
    constexpr uint8 LEVEL_WINDOW = 4;           // required levels from the Hero's level - 4 to it
    constexpr uint8 TOP_PICKS = 3;

    struct GearSlot
    {
        uint8 Slot;
        char const* Name;
        std::array<uint8, 4> Types;             // InventoryType; 0 ends the list
    };

    constexpr GearSlot GEAR_SLOTS[] = {
        { EQUIPMENT_SLOT_HEAD, "head", { INVTYPE_HEAD } },
        { EQUIPMENT_SLOT_NECK, "neck", { INVTYPE_NECK } },
        { EQUIPMENT_SLOT_SHOULDERS, "shoulders", { INVTYPE_SHOULDERS } },
        { EQUIPMENT_SLOT_BACK, "back", { INVTYPE_CLOAK } },
        { EQUIPMENT_SLOT_CHEST, "chest", { INVTYPE_CHEST, INVTYPE_ROBE } },
        { EQUIPMENT_SLOT_WRISTS, "wrists", { INVTYPE_WRISTS } },
        { EQUIPMENT_SLOT_HANDS, "hands", { INVTYPE_HANDS } },
        { EQUIPMENT_SLOT_WAIST, "waist", { INVTYPE_WAIST } },
        { EQUIPMENT_SLOT_LEGS, "legs", { INVTYPE_LEGS } },
        { EQUIPMENT_SLOT_FEET, "feet", { INVTYPE_FEET } },
        { EQUIPMENT_SLOT_FINGER1, "finger", { INVTYPE_FINGER } },
        { EQUIPMENT_SLOT_FINGER2, "finger", { INVTYPE_FINGER } },
        { EQUIPMENT_SLOT_TRINKET1, "trinket", { INVTYPE_TRINKET } },
        { EQUIPMENT_SLOT_TRINKET2, "trinket", { INVTYPE_TRINKET } },
        { EQUIPMENT_SLOT_MAINHAND, "main hand", { INVTYPE_WEAPON, INVTYPE_WEAPONMAINHAND, INVTYPE_2HWEAPON } },
        { EQUIPMENT_SLOT_OFFHAND, "off hand",
            { INVTYPE_WEAPON, INVTYPE_WEAPONOFFHAND, INVTYPE_SHIELD, INVTYPE_HOLDABLE } },
        { EQUIPMENT_SLOT_RANGED, "ranged", { INVTYPE_RANGED, INVTYPE_THROWN, INVTYPE_RANGEDRIGHT } },
    };

    bool IsPlaceholder(std::string const& name)
    {
        for (char const* word : { "Monster", "NPC Equip", "Test", "TEST", "OLD", "Deprecated", "[PH]", "QA " })
            if (name.find(word) != std::string::npos)
                return true;
        return false;
    }

    // Every item that may come out of a cache, by InventoryType. Built on first use.
    std::unordered_map<uint8, std::vector<ItemTemplate const*>> const& Pool()
    {
        static std::unordered_map<uint8, std::vector<ItemTemplate const*>> const pool = []
        {
            std::unordered_map<uint8, std::vector<ItemTemplate const*>> items;
            for (auto const& [entry, proto] : *sObjectMgr->GetItemTemplateStore())
            {
                // Uncommon, with a real required level and an item level that fits it (unused items have none:
                // a required level 0 item level 187 helm), stats only: no "Equip:", "Use:" or "Chance on hit:".
                if ((proto.Class != ITEM_CLASS_ARMOR && proto.Class != ITEM_CLASS_WEAPON)
                    || proto.Quality != ITEM_QUALITY_UNCOMMON || !proto.RequiredLevel
                    || proto.ItemLevel > proto.RequiredLevel * 5 / 2 + 10 || proto.ScalingStatDistribution
                    || std::any_of(std::begin(proto.Spells), std::end(proto.Spells),
                        [](_Spell const& spell) { return spell.SpellId != 0; })
                    || proto.RequiredSkill || proto.RequiredSpell || proto.RequiredHonorRank
                    || proto.RequiredCityRank || proto.RequiredReputationFaction || proto.HolidayId
                    || proto.Bonding == BIND_QUEST_ITEM || IsPlaceholder(proto.Name1))
                    continue;
                if (proto.Class == ITEM_CLASS_WEAPON && (proto.SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE
                    || proto.SubClass == ITEM_SUBCLASS_WEAPON_MISC || proto.SubClass == ITEM_SUBCLASS_WEAPON_obsolete
                    || proto.SubClass == ITEM_SUBCLASS_WEAPON_EXOTIC || proto.SubClass == ITEM_SUBCLASS_WEAPON_EXOTIC2
                    || proto.SubClass == ITEM_SUBCLASS_WEAPON_SPEAR))
                    continue;
                items[uint8(proto.InventoryType)].push_back(&proto);
            }
            return items;
        }();
        return pool;
    }

    // How much a role wants a point of a stat (ItemModType).
    float StatWeight(Wildcard::Role role, uint32 stat)
    {
        using namespace Wildcard;
        bool rating = (stat >= ITEM_MOD_HIT_MELEE_RATING && stat <= ITEM_MOD_HASTE_SPELL_RATING)
            || stat == ITEM_MOD_HIT_RATING || stat == ITEM_MOD_CRIT_RATING || stat == ITEM_MOD_HASTE_RATING;
        switch (role)
        {
            case ROLE_MELEE:
                switch (stat)
                {
                    case ITEM_MOD_STRENGTH: case ITEM_MOD_AGILITY: return 1.0f;
                    case ITEM_MOD_ATTACK_POWER: return 0.5f;
                    case ITEM_MOD_RANGED_ATTACK_POWER: return 0.3f;
                    case ITEM_MOD_ARMOR_PENETRATION_RATING: case ITEM_MOD_EXPERTISE_RATING: return 0.8f;
                    case ITEM_MOD_STAMINA: return 0.3f;
                    default: return rating ? 0.7f : 0.0f;
                }
            case ROLE_CASTER:
                switch (stat)
                {
                    case ITEM_MOD_INTELLECT: case ITEM_MOD_SPELL_POWER: case ITEM_MOD_SPELL_DAMAGE_DONE: return 1.0f;
                    case ITEM_MOD_SPIRIT: case ITEM_MOD_MANA_REGENERATION: return 0.5f;
                    case ITEM_MOD_SPELL_PENETRATION: case ITEM_MOD_STAMINA: return 0.3f;
                    default: return rating ? 0.7f : 0.0f;
                }
            case ROLE_HEALER:
                switch (stat)
                {
                    case ITEM_MOD_INTELLECT: case ITEM_MOD_SPIRIT: case ITEM_MOD_SPELL_POWER:
                    case ITEM_MOD_SPELL_HEALING_DONE: case ITEM_MOD_MANA_REGENERATION: return 1.0f;
                    case ITEM_MOD_STAMINA: return 0.3f;
                    default: return rating ? 0.6f : 0.0f;
                }
            case ROLE_TANK:
                switch (stat)
                {
                    case ITEM_MOD_STAMINA: case ITEM_MOD_STRENGTH: return 1.0f;
                    case ITEM_MOD_DEFENSE_SKILL_RATING: case ITEM_MOD_DODGE_RATING: case ITEM_MOD_PARRY_RATING:
                    case ITEM_MOD_BLOCK_RATING: return 0.9f;
                    case ITEM_MOD_AGILITY: return 0.6f;
                    case ITEM_MOD_BLOCK_VALUE: case ITEM_MOD_EXPERTISE_RATING: return 0.5f;
                    case ITEM_MOD_ATTACK_POWER: return 0.2f;
                    default: return rating ? 0.4f : 0.0f;
                }
            default:
                if (stat == ITEM_MOD_MANA || stat == ITEM_MOD_HEALTH)
                    return 0.1f;
                return stat == ITEM_MOD_STAMINA ? 0.5f : 1.0f;
        }
    }

    // The stats of a random enchantment (ItemRandomProperties, or ItemRandomSuffix when negative); false when it
    // has more than stats (an equip spell, a proc).
    bool AddRandomStats(uint32 itemId, int32 randomPropertyId, std::vector<std::pair<uint32, int32>>& stats)
    {
        bool statsOnly = true;
        auto addEnchant = [&](uint32 enchantId, float scale, bool scaled)
        {
            SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);
            if (!enchant)
                return;
            for (uint8 i = 0; i < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS; ++i)
            {
                if (enchant->type[i] == ITEM_ENCHANTMENT_TYPE_STAT)
                    stats.emplace_back(enchant->spellid[i], scaled ? int32(scale) : int32(enchant->amount[i]));
                else if (enchant->type[i] != ITEM_ENCHANTMENT_TYPE_NONE
                    && enchant->type[i] != ITEM_ENCHANTMENT_TYPE_RESISTANCE)
                    statsOnly = false;
            }
        };

        if (randomPropertyId > 0)
        {
            if (ItemRandomPropertiesEntry const* properties = sItemRandomPropertiesStore.LookupEntry(randomPropertyId))
                for (uint32 enchantId : properties->Enchantment)
                    addEnchant(enchantId, 0.0f, false);
        }
        else if (randomPropertyId < 0)
        {
            if (ItemRandomSuffixEntry const* suffix = sItemRandomSuffixStore.LookupEntry(-randomPropertyId))
            {
                uint32 factor = GenerateEnchSuffixFactor(itemId);
                for (uint8 i = 0; i < MAX_ITEM_ENCHANTMENT_EFFECTS; ++i)
                    addEnchant(suffix->Enchantment[i], float(factor * suffix->AllocationPct[i]) / 10000.0f, true);
            }
        }
        return statsOnly;
    }

    // The item's worth to the role; nullopt when its random enchantment has more than stats.
    std::optional<float> Score(Wildcard::Role role, ItemTemplate const* proto, int32 randomPropertyId)
    {
        std::vector<std::pair<uint32, int32>> stats;
        for (uint32 i = 0; i < proto->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
            stats.emplace_back(proto->ItemStat[i].ItemStatType, proto->ItemStat[i].ItemStatValue);
        if (!AddRandomStats(proto->ItemId, randomPropertyId, stats))
            return std::nullopt;

        float score = 0.0f;
        for (auto const& [stat, value] : stats)
            score += StatWeight(role, stat) * value;

        score += proto->Armor * (role == Wildcard::ROLE_TANK ? 0.05f : 0.01f);
        if (proto->Class == ITEM_CLASS_WEAPON && proto->Delay)
        {
            float dps = (proto->Damage[0].DamageMin + proto->Damage[0].DamageMax) / 2.0f / (proto->Delay / 1000.0f);
            bool physical = role == Wildcard::ROLE_MELEE || role == Wildcard::ROLE_TANK || role == Wildcard::ROLE_NONE;
            bool wand = proto->SubClass == ITEM_SUBCLASS_WEAPON_WAND;
            score += dps * (physical ? (wand ? 0.0f : 2.0f) : (wand ? 1.5f : 0.0f));
        }
        return score;
    }

    // The Hero's gear slots from the lowest item level piece up (empty: 0; ties in random order); the off hand only
    // when free to hold one.
    std::vector<GearSlot const*> SlotsByLevel(Player* player)
    {
        Item* mainHand = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
        bool twoHanded = mainHand && mainHand->GetTemplate()->InventoryType == INVTYPE_2HWEAPON
            && !player->CanTitanGrip();

        std::vector<std::pair<uint32, GearSlot const*>> slots;
        for (GearSlot const& gear : GEAR_SLOTS)
        {
            if (gear.Slot == EQUIPMENT_SLOT_OFFHAND && twoHanded)
                continue;
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, gear.Slot);
            slots.emplace_back(item ? item->GetTemplate()->ItemLevel : 0, &gear);
        }
        Acore::Containers::RandomShuffle(slots);
        std::stable_sort(slots.begin(), slots.end(), [](auto const& a, auto const& b) { return a.first < b.first; });

        std::vector<GearSlot const*> result;
        for (auto const& [level, gear] : slots)
            result.push_back(gear);
        return result;
    }

    struct Pick
    {
        ItemTemplate const* Proto;
        int32 RandomPropertyId;
        float Score;
    };

    // A green for that slot at the Hero's level: required level up to 4 under theirs, wider when there is none.
    std::optional<Pick> PickGear(Player* player, GearSlot const& gear)
    {
        Wildcard::Role role = Wildcard::GetRole(player);
        uint8 level = player->GetLevel();
        for (uint8 window : { LEVEL_WINDOW, uint8(LEVEL_WINDOW * 2), level })
        {
            uint8 lowest = level > window ? level - window : 0;
            std::vector<Pick> picks;
            for (uint8 type : gear.Types)
            {
                if (!type)
                    break;
                auto itr = Pool().find(type);
                if (itr == Pool().end())
                    continue;
                for (ItemTemplate const* proto : itr->second)
                    if (proto->RequiredLevel <= level && proto->RequiredLevel >= lowest
                        && player->CanUseItem(proto) == EQUIP_ERR_OK)
                    {
                        int32 random = Item::GenerateItemRandomPropertyId(proto->ItemId);
                        if (std::optional<float> score = Score(role, proto, random))
                            picks.push_back({ proto, random, *score });
                    }
            }
            if (picks.empty())
                continue;

            std::sort(picks.begin(), picks.end(), [](Pick const& a, Pick const& b) { return a.Score > b.Score; });
            return picks[urand(0, std::min<uint32>(TOP_PICKS, uint32(picks.size())) - 1)];
        }
        return std::nullopt;
    }

    // Caches into the bags; what does not fit comes by mail.
    void GiveCaches(Player* player, uint32 count)
    {
        uint32 noSpace = 0;
        ItemPosCountVec dest;
        if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, ITEM_SMOLDERING_CACHE, count, &noSpace) == EQUIP_ERR_OK
            || count > noSpace)
            if (Item* item = player->StoreNewItem(dest, ITEM_SMOLDERING_CACHE, true))
                player->SendNewItem(item, count - noSpace, true, false);
        if (noSpace)
            player->SendItemRetrievalMail(ITEM_SMOLDERING_CACHE, noSpace);
    }

    // Puts an item the Hero got into its slot when that slot is empty (and the Hero can wear it).
    void EquipIfEmpty(Player* player, ObjectGuid itemGuid)
    {
        Item* item = player->GetItemByGuid(itemGuid);
        if (!item || !Player::IsInventoryPos(item->GetPos()))     // the backpack or a bag, not worn or banked
            return;
        ItemTemplate const* proto = item->GetTemplate();
        if (proto->InventoryType == INVTYPE_NON_EQUIP || proto->InventoryType == INVTYPE_AMMO)
            return;
        uint16 dest;
        if (player->CanEquipItem(NULL_SLOT, dest, item, false) == EQUIP_ERR_OK)
            player->SwapItem(item->GetPos(), dest);
    }
}

class item_titan_smoldering_cache : public ItemScript
{
public:
    item_titan_smoldering_cache() : ItemScript("item_titan_smoldering_cache") { }

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        // The lowest slot that has gear for the Hero's level (a level 2 Hero's empty neck may have none).
        ChatHandler handler(player->GetSession());
        GearSlot const* gear = nullptr;
        std::optional<Pick> pick;
        for (GearSlot const* slot : SlotsByLevel(player))
            if ((pick = PickGear(player, *slot)))
            {
                gear = slot;
                break;
            }
        if (!pick)
        {
            handler.SendSysMessage("The cache's embers flicker: nothing in it fits you yet. Keep it: its gear starts "
                "around level 5.");
            player->SendEquipError(EQUIP_ERR_CANT_DO_RIGHT_NOW, item, nullptr);
            return true;
        }

        ItemPosCountVec dest;
        if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, pick->Proto->ItemId, 1) != EQUIP_ERR_OK)
        {
            player->SendEquipError(EQUIP_ERR_INVENTORY_FULL, item, nullptr);
            return true;
        }

        uint32 one = 1;
        player->DestroyItemCount(item, one, true);
        if (Item* gearItem = player->StoreNewItem(dest, pick->Proto->ItemId, true, pick->RandomPropertyId))
        {
            player->SendNewItem(gearItem, 1, true, false);
            handler.PSendSysMessage("|cffff8000Smoldering Adventurer's Cache:|r gear for your {}.", gear->Name);
        }
        return true;
    }
};

class titan_smoldering_cache : public PlayerScript
{
public:
    titan_smoldering_cache() : PlayerScript("titan_smoldering_cache", {
        PLAYERHOOK_ON_LEVEL_CHANGED,
        PLAYERHOOK_ON_PLAYER_COMPLETE_QUEST,
        PLAYERHOOK_ON_STORE_NEW_ITEM
    }) { }

    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (Wildcard::IsHero(player) && player->GetLevel() > oldLevel)
            GiveCaches(player, player->GetLevel() - oldLevel);
    }

    // Called when the quest is turned in (Player::RewardQuest).
    void OnPlayerCompleteQuest(Player* player, Quest const* /*quest*/) override
    {
        if (Wildcard::IsHero(player))
            GiveCaches(player, 1);
    }

    // After the store is done (its caller may still use the item where it was put).
    void OnPlayerStoreNewItem(Player* player, Item* item, uint32 /*count*/) override
    {
        if (!Wildcard::IsHero(player) || !player->IsInWorld()
            || item->GetTemplate()->InventoryType == INVTYPE_NON_EQUIP)
            return;
        ObjectGuid guid = item->GetGUID();
        player->m_Events.AddEventAtOffset([player, guid] { EquipIfEmpty(player, guid); }, 1ms);
    }
};

void AddSC_smoldering_cache()
{
    new item_titan_smoldering_cache();
    new titan_smoldering_cache();
}
