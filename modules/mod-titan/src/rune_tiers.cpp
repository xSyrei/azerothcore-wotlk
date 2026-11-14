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

#include "rune_tiers.h"
#include "wildcard.h"
#include "Chat.h"
#include "Containers.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Tokenize.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <algorithm>
#include <array>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Runes of Enhancement: Epic, Rare and Uncommon runes beside the Legendary Rune (runes.cpp), shown in the Halo
// window (the Emberforge_Runes addon). The catalog (titan_rune_catalog, written by tools/rune_tiers/generate.py):
// every rune's rarity and home ability (its first rank), an Uncommon's prefix too.
//
// - Every 5 levels a Hero is offered 3 runes and takes one (or keeps the offer for later): each Uncommon 60%,
//   Rare 30%, Epic 10%, of the runes on abilities the Hero knows and does not hold yet (Provoking: Tanks only). Once
//   10 abilities hold Uncommons, Uncommons are offered on those only. A rarity with nothing to offer gives way to
//   the others. A prestige's level 1 starts the offers again; the runes are kept.
// - Sockets: 5 Epic, 10 Rare (a full set: the one to replace is chosen). Uncommons: any number, on 10 abilities.
// - Releasing a rune, or an ability's Uncommons, is free.
// - A rune on an ability the Hero no longer knows is dormant (told once, by ability); it wakes when it is learned
//   again. The runes' effects are their own work (Uncommons and Resonance next, then Rares and Epics).
//
// Protocol (prefix EFRT, fields by ','; the client sends STATE, PICK,<seq>,<choice 1-3>[,<socket>], REL,<rune>,
// RELA,<ability>):
//   S,<level>,<epic sockets>,<rare sockets>,<ability slots>   the state begins
//   T,<rune>:<socket>:<dormant 0/1>[.<rune>:...]              the runes held (in pieces)
//   Q,<seq>,<level>,<rune>.<rune>.<rune>                      an offer waiting
//   Z                                                          the state ends
//   O (open the window: a new offer)   E,<text> (an error)
namespace
{
    constexpr char const* PREFIX = "EFRT";
    constexpr uint8 EPIC_SOCKETS = 5;
    constexpr uint8 RARE_SOCKETS = 10;
    constexpr std::size_t MAX_ABILITIES = 10;
    constexpr uint8 OFFER_EVERY = 5;
    constexpr uint8 CHOICES = 3;
    constexpr uint32 UNCOMMON_PERCENT = 60;
    constexpr uint32 RARE_PERCENT = 30;                     // Epic: the rest
    constexpr std::size_t CHUNK = 220;                      // an addon message's payload, at most
    constexpr uint32 DORMANCY_DELAY_MS = 1000;              // after a spell is learned or forgotten
    constexpr uint32 LOGIN_QUIET_MS = 15000;                // the login's own spell changes say nothing

    enum Rarity : uint8
    {
        RARITY_NONE,
        RARITY_UNCOMMON,
        RARITY_RARE,
        RARITY_EPIC,
        MAX_RARITY
    };

    enum Prefix : uint8
    {
        PREFIX_RAPID,
        PREFIX_AGGRESSIVE,
        PREFIX_MENDING,
        PREFIX_CRITICAL,
        PREFIX_REFRESHING,
        PREFIX_PROVOKING
    };

    struct RuneInfo
    {
        uint32 Id = 0;
        uint8 Rarity = RARITY_NONE;
        uint32 Ability = 0;                                 // first rank
        uint8 Prefix = 0;                                   // an Uncommon's
    };

    std::unordered_map<uint32, RuneInfo> catalog;
    std::array<std::vector<uint32>, MAX_RARITY> byRarity;

    void LoadCatalog()
    {
        catalog.clear();
        for (std::vector<uint32>& ids : byRarity)
            ids.clear();
        QueryResult result = WorldDatabase.Query("SELECT `id`, `rarity`, `ability`, `prefix` FROM "
            "`titan_rune_catalog`");
        if (!result)
        {
            LOG_WARN("module", "Runes of Enhancement: titan_rune_catalog is empty.");
            return;
        }
        do
        {
            Field* fields = result->Fetch();
            RuneInfo info { fields[0].Get<uint32>(), fields[1].Get<uint8>(), fields[2].Get<uint32>(),
                fields[3].Get<uint8>() };
            if (info.Rarity == RARITY_NONE || info.Rarity >= MAX_RARITY || !sSpellMgr->GetSpellInfo(info.Ability))
                continue;
            catalog[info.Id] = info;
            byRarity[info.Rarity].push_back(info.Id);
        } while (result->NextRow());
        LOG_INFO("module", "Runes of Enhancement: {} runes.", catalog.size());
    }

    RuneInfo const* Find(uint32 id)
    {
        auto itr = catalog.find(id);
        return itr != catalog.end() ? &itr->second : nullptr;
    }

    uint8 Sockets(uint8 rarity)
    {
        return rarity == RARITY_EPIC ? EPIC_SOCKETS : rarity == RARITY_RARE ? RARE_SOCKETS : 0;
    }

    struct Offer
    {
        uint32 Seq = 0;
        uint8 Level = 0;
        std::array<uint32, CHOICES> Runes = {};
    };

    struct State
    {
        std::unordered_map<uint32, uint8> Owned;            // rune -> socket (0: an Uncommon)
        std::deque<Offer> Offers;                           // oldest first
        uint8 OfferedLevel = 0;
        uint32 NextSeq = 1;
        std::unordered_set<uint32> Dormant;                 // home abilities of held runes the Hero does not know
        bool DormancyDirty = false;
        uint32 DormancyTimer = 0;
        uint32 QuietUntil = 0;                              // game ms
    };

    std::unordered_map<ObjectGuid::LowType, State> states;

    State& StateOf(Player* player)
    {
        return states[player->GetGUID().GetCounter()];
    }

    uint32 Now()
    {
        return GameTime::GetGameTimeMS().count();
    }

    void Send(Player* player, std::string const& payload)
    {
        WorldPacket data;
        std::string const message = std::string(PREFIX) + '\t' + payload;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, message);
        player->SendDirectMessage(&data);
    }

    void Say(Player* player, std::string const& text)
    {
        ChatHandler(player->GetSession()).SendSysMessage("|cff4ee04eRunes:|r " + text);
    }

    void Error(Player* player, std::string const& text)
    {
        Send(player, "E," + text);
        Say(player, text);
    }

    std::string AbilityName(uint32 spellId)
    {
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        return info && info->SpellName[0] ? info->SpellName[0] : "an ability";
    }

    // The ability (any rank of it) is the Hero's.
    bool Knows(Player* player, uint32 firstRank)
    {
        for (uint32 id = firstRank; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (player->HasSpell(id))
                return true;
        return false;
    }

    // The abilities holding the Hero's Uncommons (at most MAX_ABILITIES).
    std::unordered_set<uint32> Claimed(State const& state)
    {
        std::unordered_set<uint32> claimed;
        for (auto const& [rune, socket] : state.Owned)
            if (RuneInfo const* info = Find(rune); info && info->Rarity == RARITY_UNCOMMON)
                claimed.insert(info->Ability);
        return claimed;
    }

    std::unordered_set<uint32> DormantAbilities(Player* player, State const& state)
    {
        std::unordered_set<uint32> dormant;
        for (auto const& [rune, socket] : state.Owned)
            if (RuneInfo const* info = Find(rune); info && !Knows(player, info->Ability))
                dormant.insert(info->Ability);
        return dormant;
    }

    // ---- Saving ----------------------------------------------------------------------------------------------

    void SaveOwned(Player* player, uint32 rune, uint8 socket)
    {
        CharacterDatabase.Execute(Acore::StringFormat("REPLACE INTO `titan_rune_owned` (`guid`, `rune`, `socket`) "
            "VALUES ({}, {}, {})", player->GetGUID().GetCounter(), rune, socket));
    }

    void DeleteOwned(Player* player, uint32 rune)
    {
        CharacterDatabase.Execute(Acore::StringFormat("DELETE FROM `titan_rune_owned` WHERE `guid` = {} AND "
            "`rune` = {}", player->GetGUID().GetCounter(), rune));
    }

    void SaveOffer(Player* player, Offer const& offer)
    {
        CharacterDatabase.Execute(Acore::StringFormat("REPLACE INTO `titan_rune_offers` (`guid`, `seq`, `level`, "
            "`rune1`, `rune2`, `rune3`) VALUES ({}, {}, {}, {}, {}, {})", player->GetGUID().GetCounter(), offer.Seq,
            offer.Level, offer.Runes[0], offer.Runes[1], offer.Runes[2]));
    }

    void DeleteOffer(Player* player, uint32 seq)
    {
        CharacterDatabase.Execute(Acore::StringFormat("DELETE FROM `titan_rune_offers` WHERE `guid` = {} AND "
            "`seq` = {}", player->GetGUID().GetCounter(), seq));
    }

    void SaveOfferedLevel(Player* player, uint8 level)
    {
        CharacterDatabase.Execute(Acore::StringFormat("REPLACE INTO `titan_rune_state` (`guid`, `offered_level`) "
            "VALUES ({}, {})", player->GetGUID().GetCounter(), level));
    }

    void Load(Player* player)
    {
        State& state = StateOf(player);
        state = State();
        uint32 guid = player->GetGUID().GetCounter();
        if (QueryResult result = CharacterDatabase.Query("SELECT `rune`, `socket` FROM `titan_rune_owned` WHERE "
            "`guid` = {}", guid))
        {
            do
            {
                Field* fields = result->Fetch();
                if (Find(fields[0].Get<uint32>()))
                    state.Owned[fields[0].Get<uint32>()] = fields[1].Get<uint8>();
            } while (result->NextRow());
        }
        if (QueryResult result = CharacterDatabase.Query("SELECT `seq`, `level`, `rune1`, `rune2`, `rune3` FROM "
            "`titan_rune_offers` WHERE `guid` = {} ORDER BY `seq`", guid))
        {
            do
            {
                Field* fields = result->Fetch();
                Offer offer;
                offer.Seq = fields[0].Get<uint32>();
                offer.Level = fields[1].Get<uint8>();
                for (uint8 i = 0; i < CHOICES; ++i)
                    offer.Runes[i] = fields[2 + i].Get<uint32>();
                state.NextSeq = std::max(state.NextSeq, offer.Seq + 1);
                state.Offers.push_back(offer);
            } while (result->NextRow());
        }
        if (QueryResult result = CharacterDatabase.Query("SELECT `offered_level` FROM `titan_rune_state` WHERE "
            "`guid` = {}", guid))
            state.OfferedLevel = result->Fetch()[0].Get<uint8>();
        state.Dormant = DormantAbilities(player, state);
        state.QuietUntil = Now() + LOGIN_QUIET_MS;
    }

    // ---- The window ------------------------------------------------------------------------------------------

    void SendState(Player* player)
    {
        State& state = StateOf(player);
        Send(player, Acore::StringFormat("S,{},{},{},{}", player->GetLevel(), EPIC_SOCKETS, RARE_SOCKETS,
            MAX_ABILITIES));
        std::string chunk;
        for (auto const& [rune, socket] : state.Owned)
        {
            RuneInfo const* info = Find(rune);
            std::string item = Acore::StringFormat("{}:{}:{}", rune, socket,
                info && state.Dormant.count(info->Ability) ? 1 : 0);
            if (!chunk.empty() && chunk.size() + item.size() + 1 > CHUNK)
            {
                Send(player, "T," + chunk);
                chunk.clear();
            }
            chunk += (chunk.empty() ? "" : ".") + item;
        }
        if (!chunk.empty())
            Send(player, "T," + chunk);
        for (Offer const& offer : state.Offers)
            Send(player, Acore::StringFormat("Q,{},{},{}.{}.{}", offer.Seq, offer.Level, offer.Runes[0],
                offer.Runes[1], offer.Runes[2]));
        Send(player, "Z");
    }

    // ---- Offers ----------------------------------------------------------------------------------------------

    bool Offerable(Player* player, State const& state, RuneInfo const& info, std::unordered_set<uint32> const& claimed,
        std::vector<uint32> const& taken)
    {
        if (state.Owned.count(info.Id) || std::find(taken.begin(), taken.end(), info.Id) != taken.end()
            || !Knows(player, info.Ability))
            return false;
        if (info.Rarity != RARITY_UNCOMMON)
            return true;
        if (info.Prefix == PREFIX_PROVOKING && Wildcard::GetRole(player) != Wildcard::ROLE_TANK)
            return false;
        return claimed.size() < MAX_ABILITIES || claimed.count(info.Ability);
    }

    // One rune: its rarity rolled (Uncommon 60%, Rare 30%, Epic 10%); a rarity with nothing to offer gives way to
    // Rare, then Epic, then Uncommon. 0: nothing at all.
    uint32 RollOne(Player* player, State const& state, std::unordered_set<uint32> const& claimed,
        std::vector<uint32> const& taken)
    {
        uint32 roll = urand(1, 100);
        uint8 rarity = roll <= UNCOMMON_PERCENT ? RARITY_UNCOMMON
            : roll <= UNCOMMON_PERCENT + RARE_PERCENT ? RARITY_RARE : RARITY_EPIC;
        for (uint8 r : { rarity, uint8(RARITY_RARE), uint8(RARITY_EPIC), uint8(RARITY_UNCOMMON) })
        {
            std::vector<uint32> pool;
            for (uint32 id : byRarity[r])
                if (Offerable(player, state, catalog.at(id), claimed, taken))
                    pool.push_back(id);
            if (!pool.empty())
                return Acore::Containers::SelectRandomContainerElement(pool);
        }
        return 0;
    }

    // A choice of up to 3 runes (none already waiting in another offer), kept as an offer; nothing said. false:
    // none to offer.
    bool MakeOffer(Player* player, uint8 level)
    {
        State& state = StateOf(player);
        std::unordered_set<uint32> claimed = Claimed(state);
        std::vector<uint32> taken;                          // the waiting offers' runes are not offered twice
        for (Offer const& waiting : state.Offers)
            for (uint32 rune : waiting.Runes)
                if (rune)
                    taken.push_back(rune);
        std::size_t const before = taken.size();
        for (uint8 i = 0; i < CHOICES; ++i)
            if (uint32 rune = RollOne(player, state, claimed, taken))
                taken.push_back(rune);
        if (taken.size() == before)
            return false;
        Offer offer;
        offer.Seq = state.NextSeq++;
        offer.Level = level;
        std::copy(taken.begin() + before, taken.end(), offer.Runes.begin());
        state.Offers.push_back(offer);
        SaveOffer(player, offer);
        return true;
    }

    // Every 5 levels reached this run, an offer (all those owed at once: a Hero leveling past several, or one who
    // had runes before them), told once. A level below the last offered one (a prestige) starts them again. The
    // number of offers made.
    uint32 CatchUp(Player* player)
    {
        State& state = StateOf(player);
        uint8 level = player->GetLevel();
        uint8 offered = state.OfferedLevel;
        if (level < offered)
            offered = level / OFFER_EVERY * OFFER_EVERY;
        uint32 made = 0;
        while (offered + OFFER_EVERY <= level)
        {
            offered += OFFER_EVERY;
            made += MakeOffer(player, offered) ? 1 : 0;
        }
        if (offered != state.OfferedLevel)
        {
            state.OfferedLevel = offered;
            SaveOfferedLevel(player, offered);
        }
        if (made)
            Say(player, made == 1 ? std::string("the runes stir: a choice awaits you.")
                : Acore::StringFormat("the runes stir: {} choices await you.", made));
        return made;
    }

    Offer* FindOffer(State& state, uint32 seq)
    {
        for (Offer& offer : state.Offers)
            if (offer.Seq == seq)
                return &offer;
        return nullptr;
    }

    // The rune in a socket of that rarity, 0: none.
    uint32 Occupant(State const& state, uint8 rarity, uint8 socket)
    {
        for (auto const& [rune, at] : state.Owned)
            if (at == socket)
                if (RuneInfo const* info = Find(rune); info && info->Rarity == rarity)
                    return rune;
        return 0;
    }

    void Release(Player* player, State& state, uint32 rune)
    {
        state.Owned.erase(rune);
        DeleteOwned(player, rune);
    }

    // The Hero takes an offer's rune: an Uncommon on its ability (claiming one of 10), an Epic or Rare in the
    // socket chosen (its rune let go of) or the first free one.
    void Pick(Player* player, uint32 seq, uint8 choice, uint8 socket)
    {
        State& state = StateOf(player);
        Offer* offer = FindOffer(state, seq);
        if (!offer || choice < 1 || choice > CHOICES || !offer->Runes[choice - 1])
            return Error(player, "That offer is no longer there.");
        RuneInfo const* info = Find(offer->Runes[choice - 1]);
        if (!info)
            return Error(player, "That rune no longer exists.");
        if (state.Owned.count(info->Id))
            return Error(player, "You already hold that rune.");
        if (info->Rarity == RARITY_UNCOMMON)
        {
            std::unordered_set<uint32> claimed = Claimed(state);
            if (claimed.size() >= MAX_ABILITIES && !claimed.count(info->Ability))
                return Error(player, "All 10 ability slots hold Uncommon runes: release one first.");
            socket = 0;
        }
        else
        {
            uint8 sockets = Sockets(info->Rarity);
            if (socket > sockets)
                return Error(player, "There is no such socket.");
            if (!socket)
            {
                for (uint8 s = 1; s <= sockets && !socket; ++s)
                    if (!Occupant(state, info->Rarity, s))
                        socket = s;
                if (!socket)
                    return Error(player, Acore::StringFormat("Your {} sockets are full: choose one to replace.",
                        info->Rarity == RARITY_EPIC ? "Epic" : "Rare"));
            }
            else if (uint32 old = Occupant(state, info->Rarity, socket))
                Release(player, state, old);
        }
        state.Owned[info->Id] = socket;
        SaveOwned(player, info->Id, socket);
        DeleteOffer(player, seq);
        state.Offers.erase(std::remove_if(state.Offers.begin(), state.Offers.end(),
            [seq](Offer const& o) { return o.Seq == seq; }), state.Offers.end());
        state.Dormant = DormantAbilities(player, state);
        SendState(player);
    }

    // Spells learned or forgotten: the runes' dormancy, told by ability (not in the login's first moments).
    void CheckDormancy(Player* player)
    {
        State& state = StateOf(player);
        std::unordered_set<uint32> now = DormantAbilities(player, state);
        if (now == state.Dormant)
            return;
        if (Now() >= state.QuietUntil)
        {
            for (uint32 ability : now)
                if (!state.Dormant.count(ability))
                    Say(player, Acore::StringFormat("your runes on {0} are dormant: you no longer know {0}. They "
                        "wake if you learn it again.", AbilityName(ability)));
            for (uint32 ability : state.Dormant)
                if (!now.count(ability))
                    Say(player, Acore::StringFormat("your runes on {} wake.", AbilityName(ability)));
        }
        state.Dormant = std::move(now);
        SendState(player);
    }

    void OnMessage(Player* player, std::string_view message)
    {
        std::vector<std::string_view> parts = Acore::Tokenize(message, ',', true);
        if (parts.empty())
            return;
        auto num = [&](std::size_t i) { return i < parts.size() ? Acore::StringTo<uint32>(parts[i]).value_or(0) : 0; };
        if (parts[0] == "STATE")
            SendState(player);
        else if (parts[0] == "PICK")
            Pick(player, num(1), uint8(num(2)), uint8(num(3)));
        else if (parts[0] == "REL")
        {
            State& state = StateOf(player);
            if (state.Owned.count(num(1)))
                Release(player, state, num(1));
            state.Dormant = DormantAbilities(player, state);
            SendState(player);
        }
        else if (parts[0] == "RELA")
        {
            State& state = StateOf(player);
            std::vector<uint32> uncommons;
            for (auto const& [rune, socket] : state.Owned)
                if (RuneInfo const* info = Find(rune); info && info->Rarity == RARITY_UNCOMMON
                    && info->Ability == num(1))
                    uncommons.push_back(rune);
            for (uint32 rune : uncommons)
                Release(player, state, rune);
            state.Dormant = DormantAbilities(player, state);
            SendState(player);
        }
    }
}

namespace RuneTiers
{
    bool GrantOffer(Player* player, uint8 level)
    {
        if (!MakeOffer(player, level))
            return false;
        Say(player, "the runes stir: a choice awaits you.");
        SendState(player);
        Send(player, "O");
        return true;
    }
}

class titan_rune_tiers_world : public WorldScript
{
public:
    titan_rune_tiers_world() : WorldScript("titan_rune_tiers_world", { WORLDHOOK_ON_STARTUP }) { }

    void OnStartup() override
    {
        LoadCatalog();
    }
};

class titan_rune_tiers_player : public PlayerScript
{
public:
    titan_rune_tiers_player() : PlayerScript("titan_rune_tiers_player", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_LEVEL_CHANGED,
        PLAYERHOOK_ON_LEARN_SPELL,
        PLAYERHOOK_ON_FORGOT_SPELL,
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_DELETE,
        PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT
    }) { }

    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 language, std::string& msg,
        Player* receiver) override
    {
        std::string const prefix = std::string(PREFIX) + '\t';
        if (language != LANG_ADDON || receiver != player || msg.compare(0, prefix.size(), prefix) != 0)
            return true;
        if (Wildcard::IsHero(player))
            OnMessage(player, std::string_view(msg).substr(prefix.size()));
        return false;
    }

    // The runes, and any offers owed (levels reached before the runes existed, or while away); the window's state a
    // few seconds in (a request sent while loading can be lost).
    void OnPlayerLogin(Player* player) override
    {
        if (!Wildcard::IsHero(player))
            return;
        Load(player);
        bool owed = CatchUp(player) > 0;
        ObjectGuid guid = player->GetGUID();
        player->m_Events.AddEventAtOffset([player, guid, owed]()
        {
            if (!player->IsInWorld() || player->GetGUID() != guid)
                return;
            SendState(player);
            if (owed)
                Send(player, "O");
        }, 5s);
    }

    void OnPlayerLogout(Player* player) override
    {
        states.erase(player->GetGUID().GetCounter());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        for (char const* table : { "titan_rune_owned", "titan_rune_offers", "titan_rune_state" })
            CharacterDatabase.Execute(Acore::StringFormat("DELETE FROM `{}` WHERE `guid` = {}", table,
                guid.GetCounter()));
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        if (!Wildcard::IsHero(player) || !CatchUp(player))
            return;
        SendState(player);
        Send(player, "O");
    }

    void OnPlayerLearnSpell(Player* player, uint32 /*spellId*/) override
    {
        MarkDormancy(player);
    }

    void OnPlayerForgotSpell(Player* player, uint32 /*spellId*/) override
    {
        MarkDormancy(player);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        auto itr = states.find(player->GetGUID().GetCounter());
        if (itr == states.end() || !itr->second.DormancyDirty)
            return;
        itr->second.DormancyTimer += diff;
        if (itr->second.DormancyTimer < DORMANCY_DELAY_MS)
            return;
        itr->second.DormancyDirty = false;
        CheckDormancy(player);
    }

private:
    static void MarkDormancy(Player* player)
    {
        auto itr = states.find(player->GetGUID().GetCounter());
        if (itr == states.end() || itr->second.Owned.empty())
            return;
        itr->second.DormancyDirty = true;
        itr->second.DormancyTimer = 0;
    }
};

void AddSC_rune_tiers()
{
    new titan_rune_tiers_world();
    new titan_rune_tiers_player();
}
