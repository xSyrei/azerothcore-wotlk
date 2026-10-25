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
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "World.h"

// Titan Soul of Rested Experience: grants 4 levels of rested experience.
//
// Rested experience is shown the way the game shows it: the rested marker moves
// 4 full levels ahead, so kill XP is doubled across the next 4 levels' worth of XP.
// The core stores rest bonus as half of the displayed amount (the client doubles it).
// The item may push the pool past the normal Rate.Rest.MaxBonus cap.
class item_titan_soul_of_rested_experience : public ItemScript
{
public:
    item_titan_soul_of_rested_experience() : ItemScript("item_titan_soul_of_rested_experience") { }

    static constexpr uint8 RESTED_LEVELS = 4;

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        uint8 const maxLevel = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
        uint8 const level = player->GetLevel();

        // Rested experience does nothing at max level (the core clears it there).
        if (level >= maxLevel)
        {
            ChatHandler(player->GetSession()).SendSysMessage("You are already at the maximum level.");
            player->SendEquipError(EQUIP_ERR_CANT_DO_RIGHT_NOW, item, nullptr);
            return true;
        }

        uint32 restedXp = 0;
        for (uint8 lvl = level; lvl < level + RESTED_LEVELS && lvl < maxLevel; ++lvl)
            restedXp += sObjectMgr->GetXPForLevel(lvl);

        player->SetRestBonus(player->GetRestBonus() + restedXp / 2.0f, true);
        uint32 count = 1;
        player->DestroyItemCount(item, count, true);

        ChatHandler(player->GetSession()).PSendSysMessage(
            "The Titan Soul fills you with {} levels of rested experience.", RESTED_LEVELS);
        return true;
    }
};

void AddSC_item_titan_soul_of_rested_experience()
{
    new item_titan_soul_of_rested_experience();
}
