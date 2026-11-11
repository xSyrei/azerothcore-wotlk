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

#ifndef MOD_TITAN_RUNES_H
#define MOD_TITAN_RUNES_H

class Player;
class Unit;

// Legendary Runes (runes.cpp): one rune per specialization, chosen from level 10, kept until a prestige; its three
// modifiers change freely out of combat.
namespace LegendaryRunes
{
    // The active specialization changed: its rune's auras on, the last one's (stacks, summons) gone.
    void OnSpecChanged(Player* player);
    // A prestige: every specialization's rune is cleared, to be chosen again from level 10.
    void OnPrestige(Player* player);
    // Fel Rift (Fel Commandment): the enemy Fel Rush carries the Hero to, for 3 sec after a volley; else nullptr.
    Unit* FelRiftTarget(Player* player);
}

#endif
