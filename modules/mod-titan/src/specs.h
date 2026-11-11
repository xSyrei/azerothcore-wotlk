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

#ifndef MOD_TITAN_SPECS_H
#define MOD_TITAN_SPECS_H

#include "Define.h"

class Player;

// Specializations (specs.cpp): a Hero's loadouts of Wildcard abilities and talents, rerolls and locks.
namespace Specs
{
    // A locked specialization, ability or talent: the Hero cannot prestige until it is unlocked.
    bool HasLocks(Player* player);
    // A prestige: every specialization starts over (the active one is the new Hero's; the others roll anew when
    // activated). Names, icons and the ones bought stay.
    void OnPrestige(Player* player);
    // The active specialization's index (0 to 3).
    uint8 ActiveSpec(Player* player);
}

#endif
