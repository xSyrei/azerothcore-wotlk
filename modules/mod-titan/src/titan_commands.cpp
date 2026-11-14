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

#include "wildcard.h"
#include "emberspark.h"
#include "runes.h"
#include "Chat.h"
#include "GameTime.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "CommandScript.h"
#include "DBCStores.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

using namespace Acore::ChatCommands;

// GM commands for testing Wildcard builds.
//
// - .learnability <spell id or link>: teaches the selected player (or yourself) an ability or talent the way
//   Wildcard does: an ability at its best rank for their level, with its companion spells (Tame Beast's pet spells);
//   a talent rank as a talent (plain .learn cannot give a Hero talents). Requirements, role and level are ignored.
// - .spellids [on|off]: shows the spell id on every spell tooltip (Classless window, spellbook, action bars, talents,
//   buffs), for .learnability. Kept per character; the Emberforge_Classless addon draws it.
// - .emberspark [amount]: adds amount Emberspark to the selected player's (or your) account; without one, shows the
//   balance.
// - .runes: the selected player's (or your) runes as the server sees them: each one's type and how long until it is
//   ready (0: ready). Tells a server-side rune problem from a client-side one: runes the server has ready that the
//   game still refuses are the client's.
// - .unbindrune (anyone): lets go of the active specialization's Legendary Rune and its modifiers, and opens the
//   rune panel to bind another (out of combat). A GM's with a Hero selected does it for them.
// - .daytime [hour]: your own client's time of day (0-23), for looking at the lighting, water and fog at any hour;
//   no one else's changes, and the clock runs on from there. Without an hour: back to the real time.
class titan_commandscript : public CommandScript
{
public:
    titan_commandscript() : CommandScript("titan_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "learnability", HandleLearnAbility, SEC_GAMEMASTER, Console::No },
            { "spellids", HandleSpellIds, SEC_GAMEMASTER, Console::No },
            { "emberspark", HandleEmberspark, SEC_GAMEMASTER, Console::No },
            { "runes", HandleRunes, SEC_GAMEMASTER, Console::No },
            { "daytime", HandleDaytime, SEC_GAMEMASTER, Console::No },
            { "unbindrune", HandleUnbindRune, SEC_PLAYER, Console::No },
        };
        return commandTable;
    }

    static bool HandleLearnAbility(ChatHandler* handler, SpellInfo const* spell)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
        {
            handler->SendErrorMessage(LANG_NO_CHAR_SELECTED);
            return false;
        }

        uint32 spellId = spell->Id;
        char const* name = spell->SpellName[LOCALE_enUS] ? spell->SpellName[LOCALE_enUS] : "?";
        if (TalentSpellPos const* position = GetTalentSpellPos(spellId))
        {
            TalentEntry const* talent = sTalentStore.LookupEntry(position->talent_id);
            if (!talent || !Wildcard::LearnTalentRank(target, talent, position->rank, false))
            {
                handler->SendErrorMessage("That talent rank could not be learned.", false);
                return false;
            }
            target->InitTalentForLevel();
            handler->PSendSysMessage("{} learned the talent {} (rank {}).", target->GetName(), name,
                uint32(position->rank) + 1);
        }
        else if (Wildcard::Ability const* ability = Wildcard::FindAbility(sSpellMgr->GetFirstSpellInChain(spellId)))
        {
            if (Wildcard::KnownRank(target, *ability) >= 0)
            {
                handler->PSendSysMessage("{} already knows {}.", target->GetName(), name);
                return true;
            }
            // Below its first rank's level: the spell asked for.
            if (ability->Ranks.front().Level > target->GetLevel())
                target->learnSpell(spellId);
            else
                Wildcard::Learn(target, *ability, "learned (GM)");
            handler->PSendSysMessage("{} learned the ability {}.", target->GetName(), name);
        }
        else
        {
            target->learnSpell(spellId);
            handler->PSendSysMessage("{} learned {} (not a Wildcard ability or talent).", target->GetName(), name);
        }

        Classless::SendKnown(target);
        Classless::SendState(target);
        return true;
    }

    static bool HandleEmberspark(ChatHandler* handler, Optional<uint32> amount)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
        {
            handler->SendErrorMessage(LANG_NO_CHAR_SELECTED);
            return false;
        }

        if (amount && *amount)
        {
            Emberspark::Add(target->GetSession()->GetAccountId(), *amount, "gm");
            handler->PSendSysMessage("Added {} Emberspark to {}'s account.", Emberspark::Format(*amount),
                target->GetName());
            return true;
        }

        // Read without blocking; the answer goes to the GM, if still online.
        ObjectGuid gm = handler->GetSession()->GetPlayer()->GetGUID();
        std::string name = target->GetName();
        Emberspark::Query(target, [gm, name](Player* /*online*/, uint32 balance)
        {
            if (Player* asker = ObjectAccessor::FindConnectedPlayer(gm))
                ChatHandler(asker->GetSession()).PSendSysMessage("{}'s account: {} Emberspark.", name,
                    Emberspark::Format(balance));
        });
        return true;
    }

    static bool HandleUnbindRune(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        Player* target = player;
        if (handler->GetSession()->GetSecurity() >= SEC_GAMEMASTER)
            if (Player* selected = handler->getSelectedPlayer())
                target = selected;
        if (!Wildcard::IsHero(target))
        {
            handler->SendErrorMessage("Only a Hero has Legendary Runes.", false);
            return false;
        }
        if (target->IsInCombat())
        {
            handler->SendErrorMessage("A rune can't be unbound in combat.", false);
            return false;
        }
        if (!LegendaryRunes::Unbind(target))
        {
            handler->SendErrorMessage("This specialization has no Legendary Rune bound.", false);
            return false;
        }
        if (target != player)
            handler->PSendSysMessage("{}'s rune is unbound.", target->GetName());
        return true;
    }

    static bool HandleRunes(ChatHandler* handler)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target || !target->IsClass(CLASS_DEATH_KNIGHT, CLASS_CONTEXT_ABILITY))
        {
            handler->SendErrorMessage("No runes: select a Hero.");
            return false;
        }
        static char const* const TYPES[] = { "Blood", "Unholy", "Frost", "Death" };
        for (uint8 i = 0; i < MAX_RUNES; ++i)
        {
            uint8 type = target->GetCurrentRune(i);
            handler->PSendSysMessage("Rune {}: {}, {} ms until ready (base {} ms)", i + 1,
                type < 4 ? TYPES[type] : "?", target->GetRuneCooldown(i), target->GetRuneBaseCooldown(i, true));
        }
        return true;
    }

    static bool HandleDaytime(ChatHandler* handler, Optional<uint32> hour)
    {
        time_t now = GameTime::GetGameTime().count();
        time_t shown = now;
        if (hour)
        {
            if (*hour > 23)
            {
                handler->SendErrorMessage("An hour from 0 to 23.");
                return false;
            }
            tm local = Acore::Time::TimeBreakdown(now);
            shown = now - (local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec) + *hour * 3600;
        }
        // As at login (Player::SendInitialPacketsAfterAddToMap): the client lights the world by this time.
        WorldPacket data(SMSG_LOGIN_SETTIMESPEED, 4 + 4 + 4);
        data.AppendPackedTime(shown);
        data << float(0.01666667f);
        data << uint32(0);
        handler->GetSession()->SendPacket(&data);
        if (hour)
            handler->PSendSysMessage("Your time of day: {}:00 (only yours). .daytime puts it back.", *hour);
        else
            handler->SendSysMessage("Your time of day is the real time again.");
        return true;
    }

    static bool HandleSpellIds(ChatHandler* handler, Optional<bool> enable)
    {
        Player* player = handler->GetSession()->GetPlayer();
        bool on = enable ? *enable : !Classless::ShowsSpellIds(player);
        Classless::SetShowSpellIds(player, on);
        handler->PSendSysMessage("Spell ids on tooltips: {}.", on ? "on" : "off");
        return true;
    }
};

void AddSC_titan_commands()
{
    new titan_commandscript();
}
