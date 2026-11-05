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

#ifndef MOD_TITAN_EMBERSPARK_H
#define MOD_TITAN_EMBERSPARK_H

#include "Define.h"
#include <functional>
#include <string>

class Player;

// Emberspark: Project Emberforge's server currency, one balance per account (auth database, titan_emberspark).
namespace Emberspark
{
    // Adds amount to the account's balance and logs why (reason: a short constant, such as "prestige").
    void Add(uint32 accountId, uint32 amount, char const* reason);
    // Takes amount from the player's account if it has that much, logging why, then calls back with the player (if
    // still online) and whether it was taken. The deduction is guarded in the statement itself (balance >= amount),
    // so the balance can never go below zero, even with the shop spending at the same moment.
    void Spend(Player* player, uint32 amount, char const* reason, std::function<void(Player*, bool)> callback);
    // Reads the player's account balance without blocking, then calls back with the player, if still online.
    void Query(Player* player, std::function<void(Player*, uint32)> callback);
    // "Emberspark: +500 for your prestige." in the player's chat.
    void Announce(Player* player, uint32 amount, char const* why);
    // 1234567 -> "1,234,567".
    std::string Format(uint32 amount);
}

#endif
