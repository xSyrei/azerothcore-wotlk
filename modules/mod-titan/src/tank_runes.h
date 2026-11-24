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

#ifndef MOD_TITAN_TANK_RUNES_H
#define MOD_TITAN_TANK_RUNES_H

#include "Define.h"

class Player;

// The Tank Collection's Legendary Runes (their compendium): 46 Red Communion, 47 Primal Worldwarden, 48 Black
// Covenant. runes.cpp binds them like any rune (their auras and markers); src/tank_runes.cpp does what they do.
namespace TankRunes
{
    enum Spells : uint32
    {
        SPELL_BLOOD_BOIL                    = 48721,    // first ranks
        SPELL_EARTH_SHOCK                   = 8042,
        SPELL_SEARING_PAIN                  = 5676,
        SPELL_RUNE_RED_COMMUNION            = 91490,
        SPELL_COMMUNION_MODIFIER_FIRST      = 93000,    // + slot * 5 + choice (91491-91499 retired)
        SPELL_RUNE_PRIMAL_WORLDWARDEN       = 91510,
        SPELL_WORLDWARDEN_MODIFIER_FIRST    = 93025,    // + slot * 5 + choice (91511-91519 retired)
        SPELL_RUNE_BLACK_COVENANT           = 91530,
        SPELL_COVENANT_MODIFIER_FIRST       = 91531,
    };

    // The bound runes' own abilities taught or taken away, the collection's foundation (Challenge, no critical
    // strikes from NPCs) on or off: after every change of the Hero's runes (runes.cpp, Apply).
    void Apply(Player* player);
}

#endif
