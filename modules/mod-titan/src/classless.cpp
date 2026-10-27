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
#include <unordered_map>
#include <unordered_set>

// Server side of the Classless window (client addon: client-patch/AddOns/Emberforge_Classless).
//
// Both sides talk in addon whispers the player sends to and receives from themself, prefix EFCL:
//   client -> server   LIST                      send the ability list
//                      PICK,<first rank id>      spend a Selection pick on that ability
//                      KEEP,<first rank id> / REROLL,<first rank id>   answer a roll reveal
//   server -> client   B / T,<class>,<skill line>,<name> / A,<class>.<skill line>.<level>.<id>;...
//                      U,<class>,<tree order>,<tab id>
//                      N,<tab>.<talent>.<row>.<col>.<rank1>+<rank2>....<requires talent>.<its rank>;... / E
//                                                the list (begin, spec tabs, abilities, talent trees, end)
//                      K0 / K,<id>,...           known abilities (clear, then first rank ids)
//                      R0 / R,<talent id>:<rank>,...   owned talent ranks (clear, then talents)
//                      S,<picks left>,<locked>,<lock level>
//                      G,<spell id>,<first rank id>,<can reroll>   roll reveal card
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

    // Rolled abilities still offering a reroll, per player (lost at logout: the roll is then kept).
    std::unordered_map<ObjectGuid, std::unordered_set<uint32>> pendingRerolls;

    // Every playable class's talent trees: tabs (class, order, id), then talents.
    void SendTalentTrees(Player* player)
    {
        std::vector<std::string> talents;
        for (uint32 i = 0; i < sTalentTabStore.GetNumRows(); ++i)
        {
            TalentTabEntry const* tab = sTalentTabStore.LookupEntry(i);
            if (!tab || tab->petTalentMask || !(tab->ClassMask & CLASSMASK_ALL_PLAYABLE))
                continue;

            uint8 classId = 0;
            for (uint8 c = CLASS_WARRIOR; c < MAX_CLASSES; ++c)
                if (tab->ClassMask & (1 << (c - 1)))
                    classId = c;
            SendAddon(player, Acore::StringFormat("U,{},{},{}", classId, tab->tabpage, tab->TalentTabID));
        }

        for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
        {
            TalentEntry const* talent = sTalentStore.LookupEntry(i);
            if (!talent)
                continue;

            TalentTabEntry const* tab = sTalentTabStore.LookupEntry(talent->TalentTab);
            if (!tab || tab->petTalentMask || !(tab->ClassMask & CLASSMASK_ALL_PLAYABLE))
                continue;

            std::string ranks;
            for (uint32 rankSpell : talent->RankID)
                if (rankSpell)
                    ranks += (ranks.empty() ? "" : "+") + std::to_string(rankSpell);

            // Prerequisite talent and the rank it needs (DependsOnRank counts from 0), for the tree's arrows.
            talents.push_back(Acore::StringFormat("{}.{}.{}.{}.{}.{}.{}", talent->TalentTab, talent->TalentID,
                talent->Row, talent->Col, ranks, talent->DependsOn, talent->DependsOn ? talent->DependsOnRank + 1 : 0));
        }

        SendBatched(player, "N,", talents, ';');
    }

    void SendTalentRanks(Player* player)
    {
        std::vector<std::string> owned;
        for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
            if (TalentEntry const* talent = sTalentStore.LookupEntry(i))
                for (uint8 rank = MAX_TALENT_RANK; rank > 0; --rank)
                    if (uint32 rankSpell = talent->RankID[rank - 1];
                        rankSpell && player->HasTalent(rankSpell, player->GetActiveSpec()))
                    {
                        owned.push_back(Acore::StringFormat("{}:{}", talent->TalentID, rank));
                        break;
                    }

        SendAddon(player, "R0");
        SendBatched(player, "R,", owned, ',');
    }

    void Reroll(Player* player, uint32 firstRank)
    {
        auto itr = pendingRerolls.find(player->GetGUID());
        Wildcard::Ability const* ability = Wildcard::FindAbility(firstRank);
        if (itr == pendingRerolls.end() || !itr->second.erase(firstRank) || !ability
            || Wildcard::KnownRank(player, *ability) < 0)
            return;

        Wildcard::Unlearn(player, *ability);
        if (Wildcard::Ability const* rolled = Wildcard::GrantRandom(player, "rerolled into", firstRank))
            Classless::Reveal(player, *rolled, false);
        else
            Wildcard::Learn(player, *ability, "nothing else to roll, kept");

        Classless::SendKnown(player);
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
        SendTalentTrees(player);
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
        SendTalentRanks(player);
    }

    void Open(Player* player)
    {
        SendAddon(player, "O");
    }

    void Reveal(Player* player, Wildcard::Ability const& ability, bool canReroll)
    {
        uint32 firstRank = ability.Ranks.front().SpellId;
        int32 known = Wildcard::KnownRank(player, ability);
        if (known < 0)
            return;

        if (canReroll)
            pendingRerolls[player->GetGUID()].insert(firstRank);
        SendAddon(player, Acore::StringFormat("G,{},{},{}", ability.Ranks[known].SpellId, firstRank,
            canReroll ? 1 : 0));
    }
}

class titan_classless : public PlayerScript
{
public:
    titan_classless() : PlayerScript("titan_classless", {
        PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT,
        PLAYERHOOK_ON_LOGOUT
    }) { }

    void OnPlayerLogout(Player* player) override
    {
        pendingRerolls.erase(player->GetGUID());
    }

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
        else if (request.starts_with("REROLL,"))
        {
            if (Optional<uint32> spellId = Acore::StringTo<uint32>(request.substr(7)))
                Reroll(player, *spellId);
        }
        else if (request.starts_with("KEEP,"))
        {
            if (Optional<uint32> spellId = Acore::StringTo<uint32>(request.substr(5)))
                if (auto itr = pendingRerolls.find(player->GetGUID()); itr != pendingRerolls.end())
                    itr->second.erase(*spellId);
        }

        return false;
    }
};

void AddSC_classless()
{
    new titan_classless();
}
