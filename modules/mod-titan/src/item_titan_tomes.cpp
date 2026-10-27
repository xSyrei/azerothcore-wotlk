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
#include "Player.h"
#include "ScriptMgr.h"

// The starting-kit tomes every Hero begins with (see wildcard.cpp for the kit). Both can be used any
// number of times until the kit locks at Wildcard::LOCK_LEVEL, when they vanish.
//
// - Titan Tome of Randomization: rerolls the kit, KIT_SIZE new random abilities.
// - Titan Tome of Selection: clears the kit and opens the Classless window with KIT_SIZE picks.
//
// Rerolling or clearing also drops any picks still unspent from an earlier Selection.
namespace
{
    // Common checks; false when the tome cannot be used now (and was dealt with).
    bool CanUseTome(Player* player, Item* item)
    {
        if (!Wildcard::IsHero(player))
        {
            ChatHandler(player->GetSession()).SendSysMessage("Only a Hero can read this tome.");
            player->SendEquipError(EQUIP_ERR_CANT_DO_RIGHT_NOW, item, nullptr);
            return false;
        }

        if (Wildcard::IsKitLocked(player))
        {
            ChatHandler(player->GetSession()).PSendSysMessage("Your abilities locked at level {}. The tome crumbles to dust.",
                Wildcard::LOCK_LEVEL);
            uint32 count = 1;
            player->DestroyItemCount(item, count, true);
            return false;
        }

        return true;
    }
}

class item_titan_tome_of_randomization : public ItemScript
{
public:
    item_titan_tome_of_randomization() : ItemScript("item_titan_tome_of_randomization") { }

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        if (!CanUseTome(player, item))
            return true;

        Wildcard::ClearAbilities(player);
        Wildcard::SetPicks(player, 0);
        for (uint8 i = 0; i < Wildcard::KIT_SIZE; ++i)
            Wildcard::GrantRandom(player, "kit ability");

        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cffff8000Wildcard:|r your abilities are rerolled. You can reroll again until level {}.", Wildcard::LOCK_LEVEL);
        Classless::SendKnown(player);
        Classless::SendState(player);
        return true;
    }
};

class item_titan_tome_of_selection : public ItemScript
{
public:
    item_titan_tome_of_selection() : ItemScript("item_titan_tome_of_selection") { }

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        if (!CanUseTome(player, item))
            return true;

        Wildcard::ClearAbilities(player);
        Wildcard::SetPicks(player, Wildcard::KIT_SIZE);

        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cffff8000Wildcard:|r a clean slate. Choose {} abilities in the Classless window (/classless) before level {}.",
            Wildcard::KIT_SIZE, Wildcard::LOCK_LEVEL);
        Classless::SendKnown(player);
        Classless::SendState(player);
        Classless::Open(player);
        return true;
    }
};

void AddSC_item_titan_tomes()
{
    new item_titan_tome_of_randomization();
    new item_titan_tome_of_selection();
}
