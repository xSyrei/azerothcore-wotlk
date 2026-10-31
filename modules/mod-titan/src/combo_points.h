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

#ifndef MOD_TITAN_COMBO_POINTS_H
#define MOD_TITAN_COMBO_POINTS_H

#include "Define.h"

class Unit;

// Combo point refunds: Ancient Teachings (Windwalker) and Shattered Souls (Vengeance) give a finisher's combo points
// back by chance, on every finisher a Hero has, any class's: the Monk's and the Demon Hunter's (their scripts call
// Refund) and the game's own (Eviscerate, Ferocious Bite...: combo_points.cpp). The two do not add up: the better
// chance counts.
namespace ComboPoints
{
    // The refund chance, %: the better of the unit's two talents.
    int32 RefundChance(Unit const* unit);

    // A finisher spent points on target: maybe award them again.
    void Refund(Unit* unit, Unit* target, uint8 points);
}

#endif
