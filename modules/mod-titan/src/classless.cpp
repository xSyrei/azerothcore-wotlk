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
#include "prestige.h"
#include "Chat.h"
#include "DBCStores.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringConvert.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <algorithm>
#include <map>
#include <string>

// Server side of the Classless window (client addon: client-patch/AddOns/Emberforge_Classless).
//
// Both sides talk in addon whispers the player sends to and receives from themself, prefix EFCL:
//   client -> server   LIST                      send the ability list
//                      PICK,<first rank id>      spend a Selection pick on that ability
//                      ROLE,<role>               play that role (Wildcard::Role; not in combat)
//                      CANCEL,<spell id>         leave that stance, presence or form (the game will not)
//   server -> client   B / T,<class>,<skill line>,<name>
//                      A,<class>.<skill line>.<level>.<id>[.<needs>];...   needs: groups by '+', each one of
//                                                its abilities by '/' (first ranks: a form, stealth, a pet)
//                      U,<class>,<tree order>,<tab id>
//                      N,<tab>.<talent>.<row>.<col>.<rank1>+<rank2>....<requires talent>.<its rank>;...
//                      Q,<first rank id>.<talent id>.<capstone roles>;...   abilities from talents (talent id 0:
//                                                none) and capstones (the RoleMask() bits of the roles they come
//                                                to, at the capstone level only; 0: not one) / E
//                                                the list (begin, spec tabs, abilities, talent trees, end)
//                      K0 / K,<id>,...           known abilities (clear, then first rank ids)
//                      R0 / R,<talent id>:<rank>,...   owned talent ranks (clear, then talents)
//                      S,<picks left>,<locked>,<lock level>,<role>
//                      G,<spell id>,<first rank id>,<level>   ability spin (for that level's roll)
//                      H,<talent id>,<spell id>,<rank>,<level>   talent spin
//                      O                         open the window
//                      I,<0|1>                   spell ids on tooltips (GMs: .spellids)
namespace
{
    constexpr char const* PREFIX = "EFCL";
    constexpr char const* SPELL_IDS_SETTING = "mod-titan-spell-ids";
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

    // Every playable class's talent trees: tabs (class, order, id), then talents.
    void SendTalentTrees(Player* player)
    {
        std::vector<std::string> talents;
        for (uint32 i = 0; i < sTalentTabStore.GetNumRows(); ++i)
        {
            TalentTabEntry const* tab = sTalentTabStore.LookupEntry(i);
            if (!tab || tab->petTalentMask || !(tab->ClassMask & Wildcard::HERO_TALENT_CLASSMASK))
                continue;

            uint8 classId = 0;
            for (uint8 c = CLASS_WARRIOR; c <= Wildcard::CLASS_DEMON_HUNTER; ++c)
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
            if (!tab || tab->petTalentMask || !(tab->ClassMask & Wildcard::HERO_TALENT_CLASSMASK))
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

        std::vector<std::string> special;
        for (Wildcard::Ability const& ability : Wildcard::Abilities())
            if (ability.Offered && (ability.TalentId || ability.Capstone))
                special.push_back(Acore::StringFormat("{}.{}.{}", ability.Ranks.front().SpellId, ability.TalentId,
                    ability.Capstone ? ability.CapstoneRoles : 0));
        SendBatched(player, "Q,", special, ';');
    }

    void SendTalentRanks(Player* player)
    {
        std::vector<std::string> owned;
        for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
            if (TalentEntry const* talent = sTalentStore.LookupEntry(i))
            {
                // A talent learned as an ability: lit while the ability is known (any rank, or what it teaches).
                if (Wildcard::Ability const* ability = Wildcard::TalentAbility(talent->TalentID);
                    ability && Wildcard::KnownRank(player, *ability) >= 0)
                {
                    owned.push_back(Acore::StringFormat("{}:1", talent->TalentID));
                    continue;
                }

                for (uint8 rank = MAX_TALENT_RANK; rank > 0; --rank)
                    if (uint32 rankSpell = talent->RankID[rank - 1];
                        rankSpell && (player->HasTalent(rankSpell, player->GetActiveSpec())
                            || player->HasSpell(rankSpell)))
                    {
                        owned.push_back(Acore::StringFormat("{}:{}", talent->TalentID, rank));
                        break;
                    }
            }

        SendAddon(player, "R0");
        SendBatched(player, "R,", owned, ',');
    }

    void SendList(Player* player)
    {
        LocaleConstant locale = player->GetSession()->GetSessionDbcLocale();
        std::map<uint32, uint8> tabs;   // skill line -> class
        std::vector<std::string> abilities;
        for (Wildcard::Ability const& ability : Wildcard::Abilities())
        {
            // One of a faction's pair (Bloodlust or Heroism): the player's.
            if (!ability.Offered || !Wildcard::FitsPlayer(player, ability))
                continue;

            if (ability.SkillLine)
                tabs.emplace(ability.SkillLine, ability.ClassId);

            std::string entry = Acore::StringFormat("{}.{}.{}.{}", ability.ClassId, ability.SkillLine,
                ability.Ranks.front().Level, ability.Ranks.front().SpellId);
            // Its requirements: groups (+) of givers (/). The combo point group leads with a 0 (the window names it
            // "an ability that awards combo points" instead of listing every builder).
            for (std::size_t group = 0; group < ability.Requires.size(); ++group)
            {
                entry += group ? '+' : '.';
                if (int(group) == ability.ComboGroup)
                    entry += "0/";
                for (std::size_t i = 0; i < ability.Requires[group].size(); ++i)
                    entry += (i ? "/" : "") + std::to_string(ability.Requires[group][i]);
            }
            abilities.push_back(std::move(entry));
        }

        SendAddon(player, "B");
        for (auto const& [skillLine, classId] : tabs)
            if (SkillLineEntry const* skill = sSkillLineStore.LookupEntry(skillLine))
            {
                char const* name = skill->name[locale] ? skill->name[locale] : "";
                SendAddon(player, Acore::StringFormat("T,{},{},{}", classId, skillLine, name));
            }
            else if (char const* name = Wildcard::CustomSkillLineName(skillLine))
                SendAddon(player, Acore::StringFormat("T,{},{},{}", classId, skillLine, name));

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
        // The kit holds KIT_SIZE abilities, whatever picks are left over.
        if (picks && Wildcard::KitAbilities(player) >= Wildcard::KIT_SIZE)
        {
            picks = 0;
            Wildcard::SetPicks(player, 0);
        }
        if (!Wildcard::IsHero(player) || Wildcard::IsKitLocked(player) || !picks)
            handler.SendSysMessage("You have no ability picks. Use the Titan Tome of Selection first.");
        else if (ability && !Wildcard::MeetsRequirements(player, *ability))
            handler.SendSysMessage("You need its form, stance, stealth, pet or combo point ability first.");
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

    // The Death Knight's presences: auras the game never lets go (SPELL_ATTR0_NO_AURA_CANCEL), every rank.
    constexpr uint32 PRESENCES[] = { 48263, 48265, 48266 };

    // Leaves a stance, presence or form the player is in. The game keeps a warrior in a stance (their forms are
    // not toggleable: SpellShapeshiftForm.dbc) and a death knight in a presence; a Hero need not stay in either:
    // the Classless addon asks for it when the buff is right-clicked or the stance bar's lit button clicked.
    void CancelForm(Player* player, uint32 spellId)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!Wildcard::IsHero(player) || !spellInfo || !spellInfo->IsPositive() || spellInfo->IsPassive())
            return;

        uint32 firstRank = spellInfo->GetFirstRankSpell()->Id;
        bool presence = std::find(std::begin(PRESENCES), std::end(PRESENCES), firstRank) != std::end(PRESENCES);
        if (!presence && !spellInfo->HasAura(SPELL_AURA_MOD_SHAPESHIFT))
            return;

        player->RemoveOwnedAura(spellId, ObjectGuid::Empty, 0, AURA_REMOVE_BY_CANCEL);
    }
}

namespace Classless
{
    void SendState(Player* player)
    {
        SendAddon(player, Acore::StringFormat("S,{},{},{},{}", Wildcard::GetPicks(player),
            Wildcard::IsKitLocked(player) ? 1 : 0, Wildcard::LOCK_LEVEL, uint32(Wildcard::GetRole(player))));
        SendAddon(player, ShowsSpellIds(player) ? "I,1" : "I,0");
    }

    bool ShowsSpellIds(Player* player)
    {
        return player->GetSession()->GetSecurity() >= SEC_GAMEMASTER
            && player->GetPlayerSetting(SPELL_IDS_SETTING, 0).value;
    }

    void SetShowSpellIds(Player* player, bool show)
    {
        player->UpdatePlayerSetting(SPELL_IDS_SETTING, 0, show ? 1 : 0);
        SendAddon(player, ShowsSpellIds(player) ? "I,1" : "I,0");
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

    void Reveal(Player* player, Wildcard::Ability const& ability, uint8 level)
    {
        int32 known = Wildcard::KnownRank(player, ability);
        if (known < 0)
            return;

        SendAddon(player, Acore::StringFormat("G,{},{},{}", ability.Ranks[known].SpellId,
            ability.Ranks.front().SpellId, level));
    }

    void RevealTalent(Player* player, uint32 talentId, uint32 spellId, uint8 rank, uint8 level)
    {
        SendAddon(player, Acore::StringFormat("H,{},{},{},{}", talentId, spellId, rank, level));
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
        else if (request.starts_with("CANCEL,"))
        {
            if (Optional<uint32> spellId = Acore::StringTo<uint32>(request.substr(7)))
                CancelForm(player, *spellId);
        }
        else if (request.starts_with("ROLE,"))
        {
            Optional<uint32> role = Acore::StringTo<uint32>(request.substr(5));
            if (Wildcard::IsHero(player) && role && *role < Wildcard::MAX_ROLES)
            {
                // The role can change during a prestige run too (the specialization cannot: specs.cpp).
                if (!Wildcard::SetRole(player, Wildcard::Role(*role)))
                    ChatHandler(player->GetSession()).SendSysMessage("You can't change your role in combat.");
                Classless::SendState(player);
            }
        }

        return false;
    }
};

void AddSC_classless()
{
    new titan_classless();
}
