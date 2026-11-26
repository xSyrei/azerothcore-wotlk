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

#ifndef MOD_TITAN_FIREPIT_H
#define MOD_TITAN_FIREPIT_H

#include "Define.h"

class Creature;

// The Firepit (firepit.cpp): an endless wave arena on a map of its own. Its creatures are scaled by
// dungeon_scaling.cpp like any dungeon's, then by their wave's multipliers.
namespace Firepit
{
    // A Firepit creature's multiplier on its health (as players' damage and view see it), and on its damage; 1 for
    // any other creature.
    float HealthMultiplier(Creature const* creature);
    float DamageMultiplier(Creature const* creature);
    // A Firepit boss (its spells are mechanics: dungeon_scaling.cpp leaves them uncapped).
    bool IsBoss(Creature const* creature);
}

namespace DungeonLoot
{
    // A random dungeon piece of the quality (a rare's if none) as its copy at the level: its item entry, 0 if none.
    uint32 RandomPiece(uint8 level, uint32 quality);
}

#endif
