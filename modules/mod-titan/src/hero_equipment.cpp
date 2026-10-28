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

// Classless Hero: every weapon and armor type, a weapon in each hand, and a ranged weapon with ammo.
//
// Each proficiency is a spell (it tells the client what may be equipped) plus, for weapons and
// armor, a skill (the server's check). Many of these skills are invalid for the Hero's hidden base
// class (Paladin) and the core drops them at every login, so they are set again at every login and
// level, weapon skills at their maximum for the level. Relics (librams, idols, totems, sigils) are
// also allowed by class: hero_resources.cpp treats a Hero as every class for that.
namespace
{
    constexpr uint32 SPELL_DUAL_WIELD = 674;

    struct Proficiency
    {
        uint32 SpellId;
        uint16 SkillId;     // 0: the spell alone (relics)
        bool Weapon;        // weapon skills grow with level; armor skills stay at 1
    };

    constexpr Proficiency PROFICIENCIES[] = {
        { 196, SKILL_AXES, true },
        { 197, SKILL_2H_AXES, true },
        { 198, SKILL_MACES, true },
        { 199, SKILL_2H_MACES, true },
        { 200, SKILL_POLEARMS, true },
        { 201, SKILL_SWORDS, true },
        { 202, SKILL_2H_SWORDS, true },
        { 227, SKILL_STAVES, true },
        { 1180, SKILL_DAGGERS, true },
        { 15590, SKILL_FIST_WEAPONS, true },
        { 264, SKILL_BOWS, true },
        { 266, SKILL_GUNS, true },
        { 5011, SKILL_CROSSBOWS, true },
        { 2567, SKILL_THROWN, true },
        { 5009, SKILL_WANDS, true },
        { 9078, SKILL_CLOTH, false },
        { 9077, SKILL_LEATHER, false },
        { 8737, SKILL_MAIL, false },
        { 750, SKILL_PLATE_MAIL, false },
        { 9116, SKILL_SHIELD, false },
        { 27762, 0, false },        // Libram
        { 27764, 0, false },        // Idol
        { 27763, 0, false },        // Totem
        { 52665, 0, false },        // Sigil
    };

    bool IsHero(Player const* player)
    {
        return player->getClass() == CLASS_PALADIN;
    }

    void EnsureProficiencies(Player* player)
    {
        if (!player->HasSpell(SPELL_DUAL_WIELD))
            player->learnSpell(SPELL_DUAL_WIELD);

        uint16 weaponMax = uint16(player->GetLevel()) * 5;
        for (Proficiency const& proficiency : PROFICIENCIES)
        {
            if (!player->HasSpell(proficiency.SpellId))
                player->learnSpell(proficiency.SpellId);

            if (!proficiency.SkillId)
                continue;

            uint16 value = proficiency.Weapon ? weaponMax : 1;
            if (player->GetPureSkillValue(proficiency.SkillId) != value
                || player->GetPureMaxSkillValue(proficiency.SkillId) != value)
                player->SetSkill(proficiency.SkillId, 0, value, value);
        }
    }
}

class hero_equipment : public PlayerScript
{
public:
    hero_equipment() : PlayerScript("hero_equipment", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LEVEL_CHANGED
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (IsHero(player))
            EnsureProficiencies(player);
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        if (IsHero(player))
            EnsureProficiencies(player);
    }
};

void AddSC_hero_equipment()
{
    new hero_equipment();
}
