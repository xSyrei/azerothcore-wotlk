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

#include "DatabaseEnv.h"
#include "Bag.h"
#include "Item.h"
#include "Mail.h"
#include "Player.h"
#include "ScriptMgr.h"

// Classless Hero: every weapon and armor type, a weapon in each hand, and a ranged weapon with ammo.
//
// Each proficiency is a spell (it tells the client what may be equipped) plus, for weapons and
// armor, a skill (the server's check). Many of these skills are invalid for the Hero's hidden base
// class (Paladin) and the core drops them at every login, so they are set again at every login and
// level, weapon skills at their maximum for the level. Relics (librams, idols, totems, sigils) are
// also allowed by class: hero_resources.cpp treats a Hero as every class for that.
namespace
{
    constexpr uint32 SPELL_DUAL_WIELD = 674;

    struct Proficiency
    {
        uint32 SpellId;
        uint16 SkillId;     // 0: the spell alone (relics)
        bool Weapon;        // weapon skills grow with level; armor skills stay at 1
    };

    constexpr Proficiency PROFICIENCIES[] = {
        { 196, SKILL_AXES, true },
        { 197, SKILL_2H_AXES, true },
        { 198, SKILL_MACES, true },
        { 199, SKILL_2H_MACES, true },
        { 200, SKILL_POLEARMS, true },
        { 201, SKILL_SWORDS, true },
        { 202, SKILL_2H_SWORDS, true },
        { 227, SKILL_STAVES, true },
        { 1180, SKILL_DAGGERS, true },
        { 15590, SKILL_FIST_WEAPONS, true },
        { 264, SKILL_BOWS, true },
        { 266, SKILL_GUNS, true },
        { 5011, SKILL_CROSSBOWS, true },
        { 2567, SKILL_THROWN, true },
        { 5009, SKILL_WANDS, true },
        { 9078, SKILL_CLOTH, false },
        { 9077, SKILL_LEATHER, false },
        { 8737, SKILL_MAIL, false },
        { 750, SKILL_PLATE_MAIL, false },
        { 9116, SKILL_SHIELD, false },
        { 27762, 0, false },        // Libram
        { 27764, 0, false },        // Idol
        { 27763, 0, false },        // Totem
        { 52665, 0, false },        // Sigil
    };

    // A Hero's first login puts one of each weapon kind in the bags (the Battleworn Hammer from the
    // starting outfit is already in hand), so a new Hero can try any style from level 1.
    struct KitItem
    {
        uint32 ItemId;
        uint32 Count;
    };

    constexpr KitItem STARTER_BAG_KIT[] = {
        { 23346, 1 },       // Battleworn Claymore (two-handed sword)
        { 2131, 1 },        // Shortsword (one-hand: either hand, for Dual Wield)
        { 2092, 1 },        // Worn Dagger (one-hand)
        { 35, 1 },          // Bent Staff
        { 2362, 1 },        // Worn Wooden Shield
        { 2504, 1 },        // Worn Shortbow
        { 2512, 200 },      // Rough Arrow
    };

    bool IsHero(Player const* player)
    {
        return player->getClass() == CLASS_PALADIN;
    }

    void EnsureProficiencies(Player* player)
    {
        if (!player->HasSpell(SPELL_DUAL_WIELD))
            player->learnSpell(SPELL_DUAL_WIELD);

        uint16 weaponMax = uint16(player->GetLevel()) * 5;
        for (Proficiency const& proficiency : PROFICIENCIES)
        {
            if (!player->HasSpell(proficiency.SpellId))
                player->learnSpell(proficiency.SpellId);

            if (!proficiency.SkillId)
                continue;

            uint16 value = proficiency.Weapon ? weaponMax : 1;
            if (player->GetPureSkillValue(proficiency.SkillId) != value
                || player->GetPureMaxSkillValue(proficiency.SkillId) != value)
                player->SetSkill(proficiency.SkillId, 0, value, value);
        }
    }

    // Bags only: the outfit decides what is worn.
    void GiveStarterBagKit(Player* player)
    {
        for (KitItem const& kit : STARTER_BAG_KIT)
        {
            ItemPosCountVec dest;
            if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, kit.ItemId, kit.Count) == EQUIP_ERR_OK)
                player->StoreNewItem(dest, kit.ItemId, true);
        }
    }
}

// Four Emberforged Satchels (30 slots each, data/sql/db-world/titan_satchels.sql) in the bag slots, put there at every
// login: an empty bag slot gets one; another bag is emptied into the backpack and the satchels, then sent to the Hero
// by mail and replaced (a bag whose contents do not fit stays, for the next login). Losing one is harmless.
namespace
{
    constexpr uint32 ITEM_EMBERFORGED_SATCHEL = 910020;
    constexpr uint32 NPC_KEEPER_OF_THE_EMBER = 910100;

    bool EquipSatchel(Player* player, uint8 slot)
    {
        uint16 dest;
        if (player->CanEquipNewItem(slot, dest, ITEM_EMBERFORGED_SATCHEL, false) != EQUIP_ERR_OK)
            return false;
        player->EquipNewItem(dest, ITEM_EMBERFORGED_SATCHEL, true);
        return true;
    }

    // Moves a bag's contents into the backpack and the satchels worn. True if the bag is empty after.
    bool EmptyBag(Player* player, uint8 slot, Bag* bag)
    {
        for (uint32 i = 0; i < bag->GetBagSize(); ++i)
        {
            Item* item = bag->GetItemByPos(uint8(i));
            if (!item)
                continue;

            ItemPosCountVec dest;
            bool stored = player->CanStoreItem(INVENTORY_SLOT_BAG_0, NULL_SLOT, dest, item, false) == EQUIP_ERR_OK;
            for (uint8 other = INVENTORY_SLOT_BAG_START; !stored && other < INVENTORY_SLOT_BAG_END; ++other)
            {
                Item* satchel = player->GetItemByPos(INVENTORY_SLOT_BAG_0, other);
                if (satchel && satchel->GetEntry() == ITEM_EMBERFORGED_SATCHEL)
                {
                    dest.clear();
                    stored = player->CanStoreItem(other, NULL_SLOT, dest, item, false) == EQUIP_ERR_OK;
                }
            }
            if (!stored)
                continue;

            player->RemoveItem(slot, uint8(i), true);
            player->StoreItem(dest, item, true);
        }
        return bag->IsEmpty();
    }

    void MailOldBag(Player* player, Item* bag)
    {
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        bag->DeleteFromInventoryDB(trans);
        bag->SaveToDB(trans);
        MailDraft("Your old bag", "An Emberforged Satchel took its place. Its contents are in your satchels.")
            .AddItem(bag)
            .SendMailTo(trans, player, MailSender(MAIL_CREATURE, NPC_KEEPER_OF_THE_EMBER), MAIL_CHECK_MASK_COPIED);
        CharacterDatabase.CommitTransaction(trans);
    }

    void EquipSatchels(Player* player)
    {
        // Empty slots first: the satchels there take in the other bags' contents.
        for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
            if (!player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                EquipSatchel(player, slot);

        for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item || item->GetEntry() == ITEM_EMBERFORGED_SATCHEL)
                continue;

            Bag* bag = item->ToBag();
            if (!bag || !EmptyBag(player, slot, bag))
                continue;

            player->MoveItemFromInventory(INVENTORY_SLOT_BAG_0, slot, true);
            MailOldBag(player, item);
            EquipSatchel(player, slot);
        }
    }
}

class hero_equipment : public PlayerScript
{
public:
    hero_equipment() : PlayerScript("hero_equipment", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_FIRST_LOGIN,
        PLAYERHOOK_ON_LEVEL_CHANGED
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!IsHero(player))
            return;
        EnsureProficiencies(player);
        EquipSatchels(player);
    }

    void OnPlayerFirstLogin(Player* player) override
    {
        if (IsHero(player))
            GiveStarterBagKit(player);
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        if (IsHero(player))
            EnsureProficiencies(player);
    }
};

void AddSC_hero_equipment()
{
    new hero_equipment();
}
