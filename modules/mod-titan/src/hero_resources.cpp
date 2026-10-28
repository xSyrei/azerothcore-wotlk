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

// Classless Hero: every class resource at once.
//
// The Hero is the hidden base class (Paladin, see data/sql/db-world/hero_class.sql). Its display
// bar stays mana; this script makes the core also run rage (gain from combat, decay out of
// combat), energy (regen), Fury (the client's Runic Power, renamed) and runes for Heroes, and
// treats a Hero as every class for ability mechanics (runes, combo points, reactive abilities like
// Overpower and Revenge, form power swaps), and as a Hunter for pets (taming, resummoning,
// stables). Class contexts that are not about abilities (starting zone, quests, trainers, stats,
// equipment) keep the Hero's real class.
class hero_resources : public PlayerScript
{
public:
    hero_resources() : PlayerScript("hero_resources", {
        PLAYERHOOK_ON_PLAYER_IS_CLASS,
        PLAYERHOOK_ON_PLAYER_HAS_ACTIVE_POWER_TYPE
    }) { }

    static constexpr uint8 HERO_CLASS = CLASS_PALADIN;

    static bool IsHero(Player const* player)
    {
        return player->getClass() == HERO_CLASS;
    }

    Optional<bool> OnPlayerIsClass(Player const* player, Classes playerClass, ClassContext context) override
    {
        if (!IsHero(player))
            return {};

        if (context == CLASS_CONTEXT_ABILITY || context == CLASS_CONTEXT_ABILITY_REACTIVE)
            return true;

        // Hunter pets: Heroes can tame (Wildcard's Tame Beast) and keep, revive and stable their pets.
        if (context == CLASS_CONTEXT_PET && playerClass == CLASS_HUNTER)
            return true;

        return {};
    }

    bool OnPlayerHasActivePowerType(Player const* player, Powers power) override
    {
        if (!IsHero(player))
            return false;

        switch (power)
        {
            case POWER_MANA:
            case POWER_RAGE:
            case POWER_ENERGY:
            case POWER_RUNIC_POWER:
            case POWER_RUNE:
                return true;
            default:
                return false;
        }
    }
};

void AddSC_hero_resources()
{
    new hero_resources();
}
