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
#include "scaled_gear.h"
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
// and every quest turned in. Opening one gives a piece of Leveling Scaled gear (tools/scaling/scaled_gear.py,
// scaled_gear.h) in the slot of their lowest item level piece (an empty slot counts as 0), fixed at the level it was
// opened at: a small pool (a cloth, leather, mail and plate set, a cloak, a neck, a ring, a trinket, weapons, a shield
// and an off-hand), each piece a template per level (a green's item level, armor and damage there, that required
// level) and a suffix per stat mix ("(Leveling Scaled)"). For the Hero's role (Wildcard::Role):
//
// - every pool item for the slot, with every stat mix it can take, is scored by the stats the role wants (Melee
//   strength or agility, by the classes of its melee abilities, attack power and weapon damage; Ranged intellect and
//   spell power; Healer intellect, spirit, spell power and mana regeneration; Tank stamina, strength, armor and
//   defense; no role a bit of everything);
// - armor is one of the role's two types, whichever scores best: Tank plate or mail, Melee leather or mail, Ranged
//   cloth or mail, Healer leather or plate (no role: any);
// - one of the picks within 10% of the best is given.
//
// Gear from a cache is soulbound. Gear a Hero gets (loot, quest rewards, purchases, caches) goes on by itself when its
// slot is empty.
namespace
{
    constexpr uint32 ITEM_SMOLDERING_CACHE = 910005;
    constexpr float NEAR_BEST = 0.9f;           // picks this close to the best are as good

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

    // The Leveling Scaled pool's pieces, by InventoryType (their level 1 template's). Built on first use.
    std::unordered_map<uint8, std::vector<ScaledGear::Item const*>> const& Pool()
    {
        static std::unordered_map<uint8, std::vector<ScaledGear::Item const*>> const pool = []
        {
            std::unordered_map<uint8, std::vector<ScaledGear::Item const*>> items;
            for (ScaledGear::Item const& scaled : ScaledGear::ITEMS)
                if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(scaled.First))
                    items[uint8(proto->InventoryType)].push_back(&scaled);
            return items;
        }();
        return pool;
    }

    // A Melee Hero's main stat, by the classes of the melee abilities it knows: the rogue's, hunter's, druid's,
    // shaman's, Demon Hunter's and Monk's are Agility's, the warrior's, paladin's and death knight's Strength's.
    bool PrefersAgility(Player* player)
    {
        int32 agility = 0, strength = 0;
        for (Wildcard::Ability const& ability : Wildcard::Abilities())
        {
            if (!ability.Offered || !(ability.Roles & Wildcard::RoleMask(Wildcard::ROLE_MELEE))
                || Wildcard::KnownRank(player, ability) < 0)
                continue;
            switch (ability.ClassId)
            {
                case CLASS_WARRIOR: case CLASS_PALADIN: case CLASS_DEATH_KNIGHT: ++strength; break;
                case CLASS_ROGUE: case CLASS_HUNTER: case CLASS_DRUID: case CLASS_SHAMAN:
                case Wildcard::CLASS_DEMON_HUNTER: case Wildcard::CLASS_MONK: ++agility; break;
                default: break;
            }
        }
        return agility > strength;
    }

    // How much a role wants a point of a stat (ItemModType).
    float StatWeight(Wildcard::Role role, bool agility, uint32 stat)
    {
        using namespace Wildcard;
        bool rating = (stat >= ITEM_MOD_HIT_MELEE_RATING && stat <= ITEM_MOD_HASTE_SPELL_RATING)
            || stat == ITEM_MOD_HIT_RATING || stat == ITEM_MOD_CRIT_RATING || stat == ITEM_MOD_HASTE_RATING;
        switch (role)
        {
            case ROLE_MELEE:
                switch (stat)
                {
                    case ITEM_MOD_STRENGTH: return agility ? 0.4f : 1.0f;
                    case ITEM_MOD_AGILITY: return agility ? 1.0f : 0.4f;
                    case ITEM_MOD_ATTACK_POWER: return 0.5f;
                    case ITEM_MOD_RANGED_ATTACK_POWER: return 0.3f;
                    case ITEM_MOD_ARMOR_PENETRATION_RATING: case ITEM_MOD_EXPERTISE_RATING: return 0.8f;
                    case ITEM_MOD_STAMINA: return 0.3f;
                    default: return rating ? 0.7f : 0.0f;
                }
            case ROLE_RANGED:
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

    // The stats a suffix gives the item (by the factor of the item's template).
    std::vector<std::pair<uint32, int32>> SuffixStats(uint32 itemId, int32 randomPropertyId)
    {
        std::vector<std::pair<uint32, int32>> stats;
        ItemRandomSuffixEntry const* suffix = randomPropertyId < 0
            ? sItemRandomSuffixStore.LookupEntry(-randomPropertyId) : nullptr;
        if (!suffix)
            return stats;
        uint32 factor = GenerateEnchSuffixFactor(itemId);
        for (uint8 i = 0; i < MAX_ITEM_ENCHANTMENT_EFFECTS; ++i)
        {
            SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(suffix->Enchantment[i]);
            if (!enchant)
                continue;
            int32 amount = int32(factor * suffix->AllocationPct[i] / 10000);
            for (uint8 e = 0; e < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS; ++e)
                if (enchant->type[e] == ITEM_ENCHANTMENT_TYPE_STAT)
                    stats.emplace_back(enchant->spellid[e], amount);
        }
        return stats;
    }

    // The item's worth to the role with that suffix.
    float Score(Wildcard::Role role, bool agility, ItemTemplate const* proto, int32 randomPropertyId)
    {
        float score = 0.0f;
        for (auto const& [stat, value] : SuffixStats(proto->ItemId, randomPropertyId))
            score += StatWeight(role, agility, stat) * value;
        score += proto->Armor * (role == Wildcard::ROLE_TANK ? 0.05f : 0.01f);
        if (proto->Class == ITEM_CLASS_WEAPON && proto->Delay)
        {
            float hit = (proto->Damage[0].DamageMin + proto->Damage[0].DamageMax) / 2.0f;
            float dps = hit / (proto->Delay / 1000.0f);
            bool physical = role == Wildcard::ROLE_MELEE || role == Wildcard::ROLE_TANK || role == Wildcard::ROLE_NONE;
            bool wand = proto->SubClass == ITEM_SUBCLASS_WEAPON_WAND;
            score += dps * (physical ? (wand ? 0.0f : 2.0f) : (wand ? 1.5f : 0.0f));
        }
        return score;
    }

    // The armor types a role chooses between (no role: any).
    bool ArmorSuitsRole(Wildcard::Role role, uint8 armorType)
    {
        if (!armorType)
            return true;
        switch (role)
        {
            case Wildcard::ROLE_TANK:
                return armorType == ITEM_SUBCLASS_ARMOR_PLATE || armorType == ITEM_SUBCLASS_ARMOR_MAIL;
            case Wildcard::ROLE_MELEE:
                return armorType == ITEM_SUBCLASS_ARMOR_LEATHER || armorType == ITEM_SUBCLASS_ARMOR_MAIL;
            case Wildcard::ROLE_RANGED:
                return armorType == ITEM_SUBCLASS_ARMOR_CLOTH || armorType == ITEM_SUBCLASS_ARMOR_MAIL;
            case Wildcard::ROLE_HEALER:
                return armorType == ITEM_SUBCLASS_ARMOR_LEATHER || armorType == ITEM_SUBCLASS_ARMOR_PLATE;
            default: return true;
        }
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

    std::optional<Pick> PickGearOf(Player* player, GearSlot const& gear);

    // A pool piece for that slot. The main hand is a one-hand weapon or a two-hander, half the time each: two-handers
    // score higher (their damage), so one-handers, and with them the off hand, would stop coming.
    std::optional<Pick> PickGear(Player* player, GearSlot const& gear)
    {
        if (gear.Slot == EQUIPMENT_SLOT_MAINHAND)
        {
            static constexpr GearSlot ONE_HAND = { EQUIPMENT_SLOT_MAINHAND, "main hand",
                { INVTYPE_WEAPON, INVTYPE_WEAPONMAINHAND } };
            static constexpr GearSlot TWO_HAND = { EQUIPMENT_SLOT_MAINHAND, "main hand", { INVTYPE_2HWEAPON } };
            bool oneHand = roll_chance_i(50);
            if (std::optional<Pick> pick = PickGearOf(player, oneHand ? ONE_HAND : TWO_HAND))
                return pick;
            return PickGearOf(player, oneHand ? TWO_HAND : ONE_HAND);
        }
        return PickGearOf(player, gear);
    }

    // Every pool piece for the slot at the Hero's level, with every suffix it takes, scored; one near the best.
    std::optional<Pick> PickGearOf(Player* player, GearSlot const& gear)
    {
        Wildcard::Role role = Wildcard::GetRole(player);
        bool agility = role == Wildcard::ROLE_MELEE && PrefersAgility(player);
        uint8 level = uint8(std::min<uint32>(player->GetLevel(), ScaledGear::LEVELS));
        std::vector<Pick> picks;
        for (uint8 type : gear.Types)
        {
            if (!type)
                break;
            auto itr = Pool().find(type);
            if (itr == Pool().end())
                continue;
            for (ScaledGear::Item const* scaled : itr->second)
            {
                ItemTemplate const* proto = sObjectMgr->GetItemTemplate(ScaledGear::Entry(*scaled, level));
                if (!proto || !ArmorSuitsRole(role, scaled->ArmorType) || player->CanUseItem(proto) != EQUIP_ERR_OK)
                    continue;
                for (uint8 i = 0; i < scaled->SuffixCount; ++i)
                {
                    int32 random = -int32(scaled->Suffixes[i]);
                    picks.push_back({ proto, random, Score(role, agility, proto, random) });
                }
            }
        }
        if (picks.empty())
            return std::nullopt;

        std::sort(picks.begin(), picks.end(), [](Pick const& a, Pick const& b) { return a.Score > b.Score; });
        std::size_t near = 1;
        while (near < picks.size() && picks[near].Score >= picks.front().Score * NEAR_BEST)
            ++near;
        return picks[urand(0, uint32(near) - 1)];
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
            handler.SendSysMessage("The cache's embers flicker: nothing in it fits you.");
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
            gearItem->SetBinding(true);     // soulbound, as the cache it came from
            gearItem->SetState(ITEM_CHANGED, player);
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
