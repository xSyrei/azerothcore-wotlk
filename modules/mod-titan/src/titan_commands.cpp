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
#include "Chat.h"
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
