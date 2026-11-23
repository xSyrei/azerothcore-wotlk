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
    // A prestige (called at the old level): the shop's prices start over (they climb with every purchase), and the
    // leveling Eyes not claimed yet are banked for the next claim.
    void ResetPrices(Player* player);
    // Eyes of Unlearning: spends one (false: none left); counts a reroll (talent: a Talent's, else an ability's).
    bool SpendEye(Player* player, bool talent);
    uint32 Eyes(Player* player);
    // The player talked to Silas Darkmoon and is still beside him (else a line in chat, false).
    bool BesideSilas(Player* player);
    // Sends the Eyes' state to the Darkmoon window.
    void SendEyes(Player* player);
    // The account's collection has loaded (it loads at login).
    bool IsLoaded(Player* player);
    // The account owns the card (kind 0 a Skill Card by its first rank, 1 a Talent Card by its talent), its Golden
    // copy when golden; false for a card not in the catalog.
    bool Owns(Player* player, uint8 kind, uint32 id, bool golden);
}

#endif
