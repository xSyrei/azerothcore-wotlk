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

#ifndef MOD_TITAN_RUNE_TIERS_H
#define MOD_TITAN_RUNE_TIERS_H

#include "Define.h"

class Player;

// Runes of Enhancement (rune_tiers.cpp): Epic, Rare and Uncommon runes beside the Legendary Rune.
namespace RuneTiers
{
    // A choice of 3 runes for the Hero to take (the level offers' own, a GM's, an item's). false: none to offer.
    bool GrantOffer(Player* player, uint8 level);
}

#endif
