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

void AddSC_hero_resources();
void AddSC_item_titan_soul_of_rested_experience();
void AddSC_wildcard();
void AddSC_classless();
void AddSC_item_titan_tomes();
void AddSC_demon_hunter();
void AddSC_monk();
void AddSC_hero_equipment();
void AddSC_max_skills();

// Loader name follows the module folder: mod-titan -> Addmod_titanScripts
void Addmod_titanScripts()
{
    AddSC_hero_resources();
    AddSC_item_titan_soul_of_rested_experience();
    AddSC_wildcard();
    AddSC_classless();
    AddSC_item_titan_tomes();
    AddSC_demon_hunter();
    AddSC_monk();
    AddSC_hero_equipment();
    AddSC_max_skills();
}
