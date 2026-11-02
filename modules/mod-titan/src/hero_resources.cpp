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
#include "SpellInfo.h"
#include <array>

// Classless Hero: every class resource at once.
//
// The Hero is the hidden base class (Paladin, see data/sql/db-world/hero_class.sql). Its display
// bar stays mana; this script makes the core also run rage (gain from combat, decay out of
// combat), energy (regen) and runes for Heroes, and
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

        // Relics: Heroes can equip librams, idols, totems and sigils (see hero_equipment.cpp).
        if (context == CLASS_CONTEXT_EQUIP_RELIC)
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
            case POWER_RUNE:
                return true;
            default:
                return false;
        }
    }
};

// Emberforged Hero (90600, client-patch/patch_player.py): every stat counts for a Hero. The buff and its two hidden
// halves (90601, 90602) go on every Hero at login, and back after anything took them off (a resurrection).
class hero_emberforged : public PlayerScript
{
public:
    hero_emberforged() : PlayerScript("hero_emberforged", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_PLAYER_RESURRECT
    }) { }

    static constexpr std::array<uint32, 3> SPELLS = { 90600, 90601, 90602 };

    static void Forge(Player* player)
    {
        if (player->getClass() != hero_resources::HERO_CLASS)
            return;
        for (uint32 spellId : SPELLS)
            if (!player->HasAura(spellId))
                player->AddAura(spellId, player);
    }

    void OnPlayerLogin(Player* player) override
    {
        Forge(player);
    }

    void OnPlayerResurrect(Player* player, float /*restorePercent*/, bool& /*applySickness*/) override
    {
        Forge(player);
    }
};

// Emberforge has no Runic Power: every spell costing it costs Rage (the same tenths), and every effect giving, taking
// or raising it does so to Rage, as the client patch does (client-patch/patch_spells.py, convert_runic_power). Runes
// give Rage instead too (a core change in Spell::TakeRunePower).
class titan_runic_power_to_rage : public GlobalScript
{
public:
    titan_runic_power_to_rage() : GlobalScript("titan_runic_power_to_rage", {
        GLOBALHOOK_ON_LOAD_SPELL_CUSTOM_ATTR
    }) { }

    static bool IsPowerEffect(SpellEffectInfo const& effect)
    {
        switch (effect.Effect)
        {
            case SPELL_EFFECT_POWER_DRAIN:
            case SPELL_EFFECT_ENERGIZE:
            case SPELL_EFFECT_POWER_BURN:
            case SPELL_EFFECT_ENERGIZE_PCT:
                return true;
            case SPELL_EFFECT_APPLY_AURA:
                switch (effect.ApplyAuraName)
                {
                    case SPELL_AURA_PERIODIC_ENERGIZE:
                    case SPELL_AURA_MOD_INCREASE_ENERGY:
                    case SPELL_AURA_MOD_POWER_REGEN:
                    case SPELL_AURA_MOD_POWER_REGEN_PERCENT:
                    case SPELL_AURA_MOD_INCREASE_ENERGY_PERCENT:
                        return true;
                    default:
                        return false;
                }
            default:
                return false;
        }
    }

    void OnLoadSpellCustomAttr(SpellInfo* spellInfo) override
    {
        if (spellInfo->PowerType == POWER_RUNIC_POWER)
            spellInfo->PowerType = POWER_RAGE;
        for (SpellEffectInfo& effect : spellInfo->_GetEffects())
            if (effect.MiscValue == POWER_RUNIC_POWER && IsPowerEffect(effect))
                effect.MiscValue = POWER_RAGE;
    }
};

void AddSC_hero_resources()
{
    new hero_resources();
    new hero_emberforged();
    new titan_runic_power_to_rage();
}
