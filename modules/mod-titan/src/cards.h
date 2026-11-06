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

#ifndef MOD_TITAN_CARDS_H
#define MOD_TITAN_CARDS_H

#include "Define.h"

class Player;

// Skill and Talent Cards (cards.cpp): an account's collection, its packs and Silas Darkmoon's shop.
namespace Cards
{
    // A prestige: the shop's prices start over (they climb with every purchase).
    void ResetPrices(Player* player);
    // Eyes of Unlearning: spends one (false: none left); counts a reroll (talent: a Talent's, else an ability's).
    bool SpendEye(Player* player, bool talent);
    uint32 Eyes(Player* player);
    // The player talked to Silas Darkmoon and is still beside him (else a line in chat, false).
    bool BesideSilas(Player* player);
    // Sends the Eyes' state to the Darkmoon window.
    void SendEyes(Player* player);
}

#endif
