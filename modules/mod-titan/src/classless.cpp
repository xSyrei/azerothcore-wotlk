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
#include "Chat.h"
#include "DBCStores.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StringConvert.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <map>
#include <string>

// Server side of the Classless window (client addon: client-patch/AddOns/Emberforge_Classless).
//
// Both sides talk in addon whispers the player sends to and receives from themself, prefix EFCL:
//   client -> server   LIST                      send the ability list
//                      PICK,<first rank id>      spend a Selection pick on that ability
//   server -> client   B / T,<class>,<skill line>,<name> / A,<class>.<skill line>.<level>.<id>;... / E
//                                                the ability list (begin, spec tabs, abilities, end)
//                      K0 / K,<id>,...           known abilities (clear, then first rank ids)
//                      S,<picks left>,<locked>,<lock level>
//                      O                         open the window
namespace
{
    constexpr char const* PREFIX = "EFCL";
    constexpr std::size_t MAX_PAYLOAD = 230;    // the client caps prefix + message at 254 characters

    void SendAddon(Player* player, std::string const& payload)
    {
        WorldPacket data;
        std::string const message = std::string(PREFIX) + '\t' + payload;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, message);
        player->SendDirectMessage(&data);
    }

    // Sends items as "<head><item><sep><item>..." in as few messages as fit.
    void SendBatched(Player* player, std::string const& head, std::vector<std::string> const& items, char sep)
    {
        std::string payload = head;
        for (std::string const& item : items)
        {
            if (payload.size() > head.size() && payload.size() + 1 + item.size() > MAX_PAYLOAD)
            {
                SendAddon(player, payload);
                payload = head;
            }

            if (payload.size() > head.size())
                payload += sep;
            payload += item;
        }

        if (payload.size() > head.size())
            SendAddon(player, payload);
    }

    void SendList(Player* player)
    {
        LocaleConstant locale = player->GetSession()->GetSessionDbcLocale();
        std::map<uint32, uint8> tabs;   // skill line -> class
        std::vector<std::string> abilities;
        for (Wildcard::Ability const& ability : Wildcard::Abilities())
        {
            if (!ability.Offered)
                continue;

            if (ability.SkillLine)
                tabs.emplace(ability.SkillLine, ability.ClassId);

            abilities.push_back(Acore::StringFormat("{}.{}.{}.{}", ability.ClassId, ability.SkillLine,
                ability.Ranks.front().Level, ability.Ranks.front().SpellId));
        }

        SendAddon(player, "B");
        for (auto const& [skillLine, classId] : tabs)
            if (SkillLineEntry const* skill = sSkillLineStore.LookupEntry(skillLine))
            {
                char const* name = skill->name[locale] ? skill->name[locale] : "";
                SendAddon(player, Acore::StringFormat("T,{},{},{}", classId, skillLine, name));
            }

        SendBatched(player, "A,", abilities, ';');
        SendAddon(player, "E");
        Classless::SendKnown(player);
        Classless::SendState(player);
    }

    void Pick(Player* player, uint32 spellId)
    {
        ChatHandler handler(player->GetSession());
        Wildcard::Ability const* ability = Wildcard::FindAbility(spellId);
        uint32 picks = Wildcard::GetPicks(player);
        if (!Wildcard::IsHero(player) || Wildcard::IsKitLocked(player) || !picks)
            handler.SendSysMessage("You have no ability picks. Use the Titan Tome of Selection first.");
        else if (!ability || !Wildcard::CanLearn(player, *ability))
            handler.SendSysMessage("You cannot learn that ability yet.");
        else
        {
            Wildcard::Learn(player, *ability, "picked");
            Wildcard::SetPicks(player, picks - 1);
        }

        Classless::SendKnown(player);
        Classless::SendState(player);
    }
}

namespace Classless
{
    void SendState(Player* player)
    {
        SendAddon(player, Acore::StringFormat("S,{},{},{}", Wildcard::GetPicks(player),
            Wildcard::IsKitLocked(player) ? 1 : 0, Wildcard::LOCK_LEVEL));
    }

    void SendKnown(Player* player)
    {
        std::vector<std::string> known;
        for (Wildcard::Ability const& ability : Wildcard::Abilities())
            if (ability.Offered && Wildcard::KnownRank(player, ability) >= 0)
                known.push_back(std::to_string(ability.Ranks.front().SpellId));

        SendAddon(player, "K0");
        SendBatched(player, "K,", known, ',');
    }

    void Open(Player* player)
    {
        SendAddon(player, "O");
    }
}

class titan_classless : public PlayerScript
{
public:
    titan_classless() : PlayerScript("titan_classless", {
        PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT
    }) { }

    // Requests from the addon: whispers to oneself in the addon language with our prefix. They are
    // handled here and never delivered.
    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 language, std::string& msg,
        Player* receiver) override
    {
        std::string const prefix = std::string(PREFIX) + '\t';
        if (language != LANG_ADDON || receiver != player || msg.compare(0, prefix.size(), prefix) != 0)
            return true;

        std::string_view request = std::string_view(msg).substr(prefix.size());
        if (request == "LIST")
            SendList(player);
        else if (request.starts_with("PICK,"))
        {
            if (Optional<uint32> spellId = Acore::StringTo<uint32>(request.substr(5)))
                Pick(player, *spellId);
        }

        return false;
    }
};

void AddSC_classless()
{
    new titan_classless();
}
