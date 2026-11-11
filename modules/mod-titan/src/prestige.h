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

#ifndef MOD_TITAN_PRESTIGE_H
#define MOD_TITAN_PRESTIGE_H

#include "Define.h"

class Item;
class Player;

// Prestige (prestige.cpp): a level 60 Hero starts again at level 1 from the Prestige NPC, for Emberspark.
namespace Prestige
{
    uint32 GetRank(Player* player);
    // During a run (prestiged, level 60 not reached again): the specialization cannot change (specs.cpp).
    bool IsOnRun(Player* player);
    // The level a Hero needs to wear an item: its required level. A weapon or armor of uncommon quality or better
    // with none that the squish found no source level for (zz_titan_squish.sql gives quest rewards their quest's)
    // asks for its item level less 10 (classic, item level 70 at most) or the level cap (the later ones).
    uint8 EquipLevel(Item const* item);
}

#endif
