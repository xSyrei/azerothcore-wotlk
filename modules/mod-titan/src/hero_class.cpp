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

#include "Player.h"
#include "ScriptMgr.h"

// Classless Hero: no class talent points. Wildcard hands out talents itself; the Paladin talent
// trees the hidden base class would otherwise fill stay empty.
class hero_class : public PlayerScript
{
public:
    hero_class() : PlayerScript("hero_class", {
        PLAYERHOOK_ON_CALCULATE_TALENTS_POINTS
    }) { }

    static constexpr uint8 HERO_CLASS = CLASS_PALADIN;

    void OnPlayerCalculateTalentsPoints(Player const* player, uint32& talentPointsForLevel) override
    {
        if (player->getClass() == HERO_CLASS)
            talentPointsForLevel = 0;
    }
};

void AddSC_hero_class()
{
    new hero_class();
}
