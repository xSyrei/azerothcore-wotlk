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

#ifndef MOD_TITAN_HERO_EQUIPMENT_H
#define MOD_TITAN_HERO_EQUIPMENT_H

#include "Define.h"
#include <utility>
#include <vector>

// A Hero's equipment (hero_equipment.cpp).
namespace HeroEquipment
{
    // The weapons a new Hero gets in their bags, one of each kind: (item, count). Prestige gives them again.
    std::vector<std::pair<uint32, uint32>> StarterBagKit();
}

#endif
