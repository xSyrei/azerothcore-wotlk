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

#include "hero_equipment.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Bag.h"
#include "Item.h"
#include "Mail.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

#include <unordered_map>

// Classless Hero: every weapon and armor type, a weapon in each hand, and ammo that never runs out.
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
        { 2131, 1 },        // a second Shortsword, for two swords
        { 2092, 1 },        // Worn Dagger (one-hand)
        { 2092, 1 },        // a second Worn Dagger, for two daggers
        { 35, 1 },          // Bent Staff
        { 2362, 1 },        // Worn Wooden Shield
        { 2504, 1 },        // Worn Shortbow (its arrows: EnsureAmmo)
    };

    bool IsHero(Player const* player)
    {
        return player->getClass() == CLASS_PALADIN;
    }

    // The spells a skill teaches (Thrown: Throw), added before the skill is set again, as the core adds them while a
    // character loads: without a "learned" packet. These skills are set again at every login and level change (they
    // belong to no Paladin, so the core drops them while loading), and the core taught Throw in play then: the client
    // put the new spell on the action bars, at every login and prestige.
    void AddSkillSpellsQuietly(Player* player, uint16 skillId)
    {
        for (SkillLineAbilityEntry const* ability : GetSkillLineAbilitiesBySkillLine(skillId))
        {
            if (ability->AcquireMethod != SKILL_LINE_ABILITY_LEARNED_ON_SKILL_LEARN
                && ability->AcquireMethod != SKILL_LINE_ABILITY_LEARNED_ON_SKILL_VALUE)
                continue;
            if (ability->MinSkillLineRank > 1 || player->HasSpell(ability->Spell)
                || (ability->RaceMask && !(ability->RaceMask & player->getRaceMask()))
                || (ability->ClassMask && !(ability->ClassMask & player->getClassMask())))
                continue;
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(ability->Spell);
            if (spellInfo && !spellInfo->IsPassive())
                player->addSpell(ability->Spell, SPEC_MASK_ALL, true, true, true);
        }
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
            {
                AddSkillSpellsQuietly(player, proficiency.SkillId);
                player->SetSkill(proficiency.SkillId, 0, value, value);
            }
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

std::vector<std::pair<uint32, uint32>> HeroEquipment::StarterBagKit()
{
    std::vector<std::pair<uint32, uint32>> kit;
    for (KitItem const& item : STARTER_BAG_KIT)
        kit.emplace_back(item.ItemId, item.Count);
    return kit;
}

// Ammo that never runs out: a Hero with a bow, crossbow or gun has the arrows or bullets of their level in the ammo
// slot, a stack topped up should it run low (InfiniteAmmo.Enabled, docker-compose.yml, spends none; checked every few
// seconds, and at login, each level and each ranged weapon equipped). Real ammo: the client checks it before a shot
// leaves (Requires No Ammo, Thori'dal's aura, which this used before, only the server honors: Auto Shot and the
// Hunter's shots failed with "no ammo"). Other arrows and
// bullets the Hero carries are taken away, and the damage their shots add is the level's ammo's
// (OnPlayerApplyAmmoBonuses), whatever is in the slot.
namespace
{
    constexpr uint32 SPELL_REQUIRES_NO_AMMO = 46699;   // the old way: taken off
    constexpr uint32 AMMO_STACK = 1000, AMMO_REFILL_BELOW = 200, AMMO_CHECK_MS = 3000;

    struct AmmoTier
    {
        uint8 Level;
        uint32 Arrow;
        uint32 Bullet;
    };

    constexpr AmmoTier AMMO_TIERS[] = {
        { 1, 2512, 2516 },      // Rough Arrow, Light Shot
        { 10, 2515, 2519 },     // Sharp Arrow, Heavy Shot
        { 25, 3030, 3033 },     // Razor Arrow, Solid Shot
        { 40, 11285, 11284 },   // Jagged Arrow, Accurate Slugs
        { 55, 28053, 28060 },   // Wicked Arrow, Impact Shot
    };

    // The level's arrows (bow, crossbow) or bullets (gun) for the Hero's ranged weapon; 0 without such a weapon.
    uint32 LevelAmmo(Player const* player)
    {
        Item* ranged = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
        if (!ranged)
            return 0;
        uint32 subClass = ranged->GetTemplate()->SubClass;
        bool arrows = subClass == ITEM_SUBCLASS_WEAPON_BOW || subClass == ITEM_SUBCLASS_WEAPON_CROSSBOW;
        if (!arrows && subClass != ITEM_SUBCLASS_WEAPON_GUN)
            return 0;
        uint32 ammo = 0;
        for (AmmoTier const& tier : AMMO_TIERS)
            if (tier.Level <= player->GetLevel())
                ammo = arrows ? tier.Arrow : tier.Bullet;
        return ammo;
    }

    // The damage per second of the level's ammo (0 without a bow, crossbow or gun, or broken).
    float LevelAmmoDps(Player const* player)
    {
        Item* ranged = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(LevelAmmo(player));
        if (!ranged || ranged->IsBroken() || !proto)
            return 0.0f;
        return (proto->Damage[0].DamageMin + proto->Damage[0].DamageMax) / 2;
    }

    // The level's ammo in the slot and at least AMMO_REFILL_BELOW of it carried (topped up to AMMO_STACK); other
    // arrows and bullets taken away.
    void EnsureAmmo(Player* player)
    {
        if (player->HasAura(SPELL_REQUIRES_NO_AMMO))
            player->RemoveAurasDueToSpell(SPELL_REQUIRES_NO_AMMO);
        uint32 wanted = LevelAmmo(player);

        std::vector<uint32> other;
        auto look = [&](Item* item)
        {
            if (item && item->GetTemplate()->Class == ITEM_CLASS_PROJECTILE && item->GetEntry() != wanted)
                other.push_back(item->GetEntry());
        };
        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
            look(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
            if (Bag* bag = player->GetBagByPos(bagSlot))
                for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                    look(bag->GetItemByPos(slot));
        for (uint32 entry : other)
            player->DestroyItemCount(entry, player->GetItemCount(entry), true);

        if (!wanted)
        {
            if (player->GetUInt32Value(PLAYER_AMMO_ID))
                player->RemoveAmmo();
            return;
        }
        uint32 carried = player->GetItemCount(wanted);
        if (carried < AMMO_REFILL_BELOW)
            player->AddItem(wanted, AMMO_STACK - carried);
        if (player->GetUInt32Value(PLAYER_AMMO_ID) != wanted && player->HasItemCount(wanted))
            player->SetAmmo(wanted);
        player->_ApplyAmmoBonuses();
    }

    // Each Hero's time to the next top-up.
    std::unordered_map<ObjectGuid::LowType, uint32> ammoTimers;
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
        PLAYERHOOK_ON_LEVEL_CHANGED,
        PLAYERHOOK_CHECK_ITEM_IN_SLOT_AT_LOAD_INVENTORY,
        PLAYERHOOK_ON_EQUIP,
        PLAYERHOOK_ON_APPLY_AMMO_BONUSES,
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_LOGOUT
    }) { }

    // The ammo topped up as it is shot.
    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        uint32& timer = ammoTimers[player->GetGUID().GetCounter()];
        timer += diff;
        if (timer < AMMO_CHECK_MS)
            return;
        timer = 0;
        if (!IsHero(player) || !player->IsInWorld())
            return;
        if (uint32 wanted = LevelAmmo(player); wanted && (player->GetUInt32Value(PLAYER_AMMO_ID) != wanted
            || player->GetItemCount(wanted) < AMMO_REFILL_BELOW))
            EnsureAmmo(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        ammoTimers.erase(player->GetGUID().GetCounter());
    }

    void OnPlayerApplyAmmoBonuses(Player* player, ItemTemplate const* /*proto*/, float& currentAmmoDPS) override
    {
        if (IsHero(player))
            currentAmmoDPS = LevelAmmoDps(player);
    }

    void OnPlayerEquip(Player* player, Item* item, uint8 /*bag*/, uint8 slot, bool /*update*/) override
    {
        // Not while the character loads: OnPlayerLogin sees to it then.
        if (IsHero(player) && slot == EQUIPMENT_SLOT_RANGED && item && player->IsInWorld())
            EnsureAmmo(player);
    }

    // A Hero's gear is checked while the character loads, before its weapon and armor skills and Dual Wield are set
    // again (they belong to no Paladin, EnsureProficiencies), so the core took off what needed them and mailed it.
    // Proficiency and Dual Wield refusals are ignored then: the gear stays where it was worn.
    bool OnPlayerCheckItemInSlotAtLoadInventory(Player* player, Item* item, uint8 slot, uint8& err, uint16& dest)
        override
    {
        if (!IsHero(player))
            return true;
        err = player->CanEquipItem(slot, dest, item, false, false);
        if (err == EQUIP_ERR_NO_REQUIRED_PROFICIENCY || err == EQUIP_ERR_CANT_EQUIP_SKILL
            || err == EQUIP_ERR_CANT_DUAL_WIELD)
        {
            err = EQUIP_ERR_OK;
            dest = (INVENTORY_SLOT_BAG_0 << 8) | slot;
        }
        return false;
    }

    void OnPlayerLogin(Player* player) override
    {
        if (!IsHero(player))
            return;
        EnsureProficiencies(player);
        EquipSatchels(player);
        EnsureAmmo(player);
    }

    void OnPlayerFirstLogin(Player* player) override
    {
        if (IsHero(player))
            GiveStarterBagKit(player);
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        if (!IsHero(player))
            return;
        EnsureProficiencies(player);
        EnsureAmmo(player);
    }
};

void AddSC_hero_equipment()
{
    new hero_equipment();
}
