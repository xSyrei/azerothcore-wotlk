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

#include "Chat.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "Unit.h"
#include "WorldPacket.h"
#include <array>
#include <unordered_map>

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

// Runes for Heroes. The server recharges them (Player::RegenerateAll treats a Hero as a Death Knight), but the
// client only counts a spent rune's cooldown down for a real Death Knight: for a Hero it stays spent on the rune bar
// and the client refuses rune spells. So while a Hero has a rune cooling down, the server sends the runes' state
// every half second (SMSG_RESYNC_RUNES: each rune's type and how much of its cooldown has passed, 0-255), and the
// moment one is spent or ready, at once. A ready rune's state goes out before its SMSG_ADD_RUNE_POWER: the client
// casts from the resynced cooldown, so a rune that only got SMSG_ADD_RUNE_POWER looked ready on the rune bar but
// stayed spent for casting, the last resync having found it still cooling down (until a relog reset the runes).
class hero_runes : public PlayerScript
{
public:
    hero_runes() : PlayerScript("hero_runes", {
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_LOGOUT
    }) { }

    static constexpr uint32 RESYNC_INTERVAL = 500;

    struct State
    {
        uint32 Timer = 0;
        uint8 Cooling = 0;      // runes cooling down at the last update, a bit each
        int16 ComboPoints = -1; // the combo points last sent (-1: none sent yet)
    };

    // Combo points, likewise: the client keeps them only for a Rogue or Druid, so a Hero's never showed. The
    // Emberforge_Resources addon (Combo.lua) gets them (prefix EFCP, the count) and shows them on the combo frame.
    static void SendComboPoints(Player* player, State& state)
    {
        uint8 points = player->GetComboPoints();
        if (state.ComboPoints == points)
            return;
        state.ComboPoints = points;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
            "EFCP\t" + std::to_string(points));
        player->SendDirectMessage(&data);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (!hero_resources::IsHero(player) || !player->IsInWorld())
            return;

        State& state = _states[player->GetGUID().GetCounter()];
        SendComboPoints(player, state);
        uint8 cooling = 0;
        for (uint8 i = 0; i < MAX_RUNES; ++i)
            if (player->GetRuneCooldown(i))
                cooling |= 1 << i;

        uint8 ready = state.Cooling & ~cooling;
        bool changed = cooling != state.Cooling;
        state.Cooling = cooling;

        state.Timer += diff;
        if (changed || (cooling && state.Timer >= RESYNC_INTERVAL))
        {
            state.Timer = 0;
            Resync(player);
        }
        if (!cooling)
            state.Timer = 0;

        for (uint8 i = 0; i < MAX_RUNES; ++i)
            if (ready & (1 << i))
                player->AddRunePower(i);
    }

    // Every rune's type and how much of its cooldown has passed (255: ready).
    static void Resync(Player* player)
    {
        WorldPacket data(SMSG_RESYNC_RUNES, 4 + MAX_RUNES * 2);
        data << uint32(MAX_RUNES);
        for (uint8 i = 0; i < MAX_RUNES; ++i)
        {
            uint32 cooldown = player->GetRuneCooldown(i);
            uint32 base = std::max<uint32>(1, player->GetRuneBaseCooldown(i, true));
            data << uint8(player->GetCurrentRune(i));
            data << uint8(cooldown >= base ? 0 : 255 * (base - cooldown) / base);
        }
        player->SendDirectMessage(&data);
    }

    void OnPlayerLogout(Player* player) override
    {
        _states.erase(player->GetGUID().GetCounter());
    }

private:
    std::unordered_map<ObjectGuid::LowType, State> _states;
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

    // Class abilities need no reagents, tools or totems (Rune of Portals, Soul Shards, a Fire Totem...): a Hero has
    // every class's. Professions keep theirs (their spells have no class family). The client patch does the same
    // (client-patch/patch_spells.py, drop_class_reagents).
    static bool IsClassFamily(uint32 family)
    {
        return family != SPELLFAMILY_GENERIC && family != SPELLFAMILY_UNK1 && family != SPELLFAMILY_UNK2
            && family != SPELLFAMILY_POTION && family != SPELLFAMILY_PET;
    }

    void OnLoadSpellCustomAttr(SpellInfo* spellInfo) override
    {
        if (IsClassFamily(spellInfo->SpellFamilyName))
        {
            spellInfo->Reagent.fill(0);
            spellInfo->ReagentCount.fill(0);
            spellInfo->Totem.fill(0);
            spellInfo->TotemCategory.fill(0);
        }

        if (spellInfo->PowerType == POWER_RUNIC_POWER)
            spellInfo->PowerType = POWER_RAGE;
        for (SpellEffectInfo& effect : spellInfo->_GetEffects())
            if (effect.MiscValue == POWER_RUNIC_POWER && IsPowerEffect(effect))
                effect.MiscValue = POWER_RAGE;
    }
};

// Death Knight abilities come from level 1 for a Hero (wildcard.cpp, LEVEL_ONE_ABILITIES), but their damage is a
// level-55 class's: Icy Touch hit a level-1 wolf for about 236. A Hero's Death Knight spells (their direct damage
// and their diseases' and Death and Decay's ticks) deal a share of it below level 40 that follows what a
// level-appropriate spell deals (a Mage's Frostbolt of that level against Icy Touch's): 8% at level 1, 18% at 10,
// 38% at 20, 65% at 30, all of it from 40. Heals stay whole: the Death Knight's are a share of maximum health already.
class titan_dk_low_level : public UnitScript
{
public:
    titan_dk_low_level() : UnitScript("titan_dk_low_level", true, {
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK
    }) { }

    struct Point
    {
        uint8 Level;
        float Share;
    };

    static constexpr std::array<Point, 5> CURVE = { { { 1, 0.08f }, { 10, 0.18f }, { 20, 0.38f }, { 30, 0.65f },
        { 40, 1.0f } } };

    static float Share(uint8 level)
    {
        if (level <= CURVE.front().Level)
            return CURVE.front().Share;
        for (std::size_t i = 1; i < CURVE.size(); ++i)
            if (level < CURVE[i].Level)
            {
                Point const& low = CURVE[i - 1];
                Point const& high = CURVE[i];
                return low.Share + (high.Share - low.Share) * (level - low.Level) / float(high.Level - low.Level);
            }
        return 1.0f;
    }

    // The share for a Hero's Death Knight spell, 1 for anything else.
    static float ShareFor(Unit* attacker, SpellInfo const* spellInfo)
    {
        if (!attacker || !spellInfo || spellInfo->SpellFamilyName != SPELLFAMILY_DEATHKNIGHT)
            return 1.0f;
        Player* player = attacker->ToPlayer();
        if (!player || !hero_resources::IsHero(player))
            return 1.0f;
        return Share(player->GetLevel());
    }

    void ModifySpellDamageTaken(Unit* /*target*/, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        float share = ShareFor(attacker, spellInfo);
        if (share < 1.0f && damage > 0)
            damage = std::max<int32>(1, int32(damage * share));
    }

    void ModifyPeriodicDamageAurasTick(Unit* /*target*/, Unit* attacker, uint32& damage, SpellInfo const* spellInfo)
        override
    {
        // The core runs periodic heals through this hook too.
        if (spellInfo && (spellInfo->HasAura(SPELL_AURA_PERIODIC_HEAL)
            || spellInfo->HasAura(SPELL_AURA_OBS_MOD_HEALTH)))
            return;
        float share = ShareFor(attacker, spellInfo);
        if (share < 1.0f && damage > 0)
            damage = std::max<uint32>(1, uint32(damage * share));
    }
};

void AddSC_hero_resources()
{
    new hero_resources();
    new hero_emberforged();
    new titan_runic_power_to_rage();
    new hero_runes();
    new titan_dk_low_level();
}
