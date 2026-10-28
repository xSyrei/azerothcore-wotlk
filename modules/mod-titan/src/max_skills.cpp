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

// Every character's weapon skills, Defense and Unarmed always sit at the cap for their level: on
// login, and on every level up (after the core has raised the caps). Professions and riding keep
// their own progress. (Heroes' extra weapon skills are set by hero_equipment.cpp.)
class titan_max_skills : public PlayerScript
{
public:
    titan_max_skills() : PlayerScript("titan_max_skills", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LEVEL_CHANGED
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        player->UpdateSkillsToMaxSkillsForLevel();
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        player->UpdateSkillsToMaxSkillsForLevel();
    }
};

void AddSC_max_skills()
{
    new titan_max_skills();
}
