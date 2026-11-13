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

#include "specs.h"
#include "cards.h"
#include "emberspark.h"
#include "prestige.h"
#include "runes.h"
#include "wildcard.h"
#include "Chat.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Tokenize.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <array>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Specializations: a Hero's loadouts of Wildcard abilities and talents (characters database: titan_specs, and
// titan_spec_entries for what each holds).
//
// - Four: Specialization I and II free, III and IV bought from Silas Darkmoon (the Darkmoon window). One is active:
//   its abilities and talents are the Hero's. Activating another (out of combat; the Classless window, right-click)
//   keeps the active one's, takes them away and puts the other's back. One never built is empty (dormant): no
//   abilities, talents, rune or bars, nothing rolled into it, until the Hero prestiges in it, which builds it from
//   level 1 as a new Hero's (the other specs keep their builds). A spec put back gets what its level is owed since.
// - Each has a name and an icon, chosen in the Classless window.
// - A reroll (right-click an ability or talent) spends an Eye of Unlearning (cards.cpp): the ability for another the
//   Hero can learn (a capstone for another capstone), the talent's ranks for as many of other talents.
// - Locks: a locked ability or talent is never rerolled; a locked specialization's none are. Nothing locked can be
//   prestiged: the Hero unlocks first. A prestige starts the active specialization over; the others keep their builds
//   (names, icons and the bought ones stay).
//
// Addon whispers, prefix EFSP, to and from the Classless and Darkmoon windows:
//   from server   P,<active>,<eyes>   S,<spec>,<owned>,<locked>,<used>,<price>,<icon>,<name>   (each of the four)
//                 L0 / L,<kind>.<id>;...   the active spec's locks (kind: 0 ability by first rank, 1 talent)   PE
//   to server     LIST, ACTIVATE,<spec>, LOCK,<spec>,<0|1>, NAME,<spec>,<name>, ICON,<spec>,<icon>, BUY,<spec>,
//                 LOCKENTRY,<kind>,<id>,<0|1>, REROLL,<kind>,<id>, UNLEARN,<ability> (free, before the kit locks)
namespace
{
    constexpr char const* PREFIX = "EFSP";
    constexpr std::size_t MAX_PAYLOAD = 230;
    constexpr char const* SETTING_SOURCE = "mod-titan-specs";
    constexpr uint32 SETTING_ACTIVE = 0;
    constexpr uint32 SETTING_DORMANT = 1;       // the active spec is empty until the next prestige
    constexpr char const* WILDCARD_SOURCE = "mod-titan-wildcard";       // wildcard.cpp's settings
    constexpr uint32 WILDCARD_GRANTED = 0, WILDCARD_PICKS = 1, WILDCARD_CAPSTONE = 3;
    constexpr uint8 MAX_SPECS = 4;
    constexpr uint8 FREE_SPECS = 2;
    constexpr std::array<uint32, MAX_SPECS> PRICES = { 0, 0, 25000, 50000 };
    constexpr std::array<char const*, MAX_SPECS> NUMERALS = { "I", "II", "III", "IV" };
    constexpr std::size_t MAX_NAME = 24, MAX_ICON = 64;

    enum EntryKind : uint8
    {
        ENTRY_ABILITY,
        ENTRY_TALENT,
    };

    struct Entry
    {
        uint8 Rank = 0;         // a talent's ranks (an inactive spec's); 0 for an ability
        bool Locked = false;
    };

    struct Spec
    {
        std::string Name;
        std::string Icon;
        bool Owned = false;
        bool Locked = false;
        bool Used = false;      // has been active: holds abilities and talents
        uint32 Granted = 0, Picks = 0, Capstone = 0;            // Wildcard's counts, while inactive
        std::map<std::pair<uint8, uint32>, Entry> Entries;      // inactive: all it holds; active: its locks
        std::map<uint8, std::pair<uint32, uint8>> Actions;      // inactive: its action bars (button: action, type)
    };

    struct Loadouts
    {
        uint8 Pending = 3;
        uint8 Active = 0;
        std::array<Spec, MAX_SPECS> Specs;
    };

    std::unordered_map<ObjectGuid, Loadouts> loadouts;

    Loadouts* Loaded(Player* player)
    {
        auto itr = loadouts.find(player->GetGUID());
        return itr != loadouts.end() && !itr->second.Pending ? &itr->second : nullptr;
    }

    uint32 Wild(Player* player, uint32 index)
    {
        return player->GetPlayerSetting(WILDCARD_SOURCE, index).value;
    }

    std::string DefaultName(uint8 spec)
    {
        return std::string("Specialization ") + NUMERALS[spec];
    }

    // Names: letters, digits, spaces, ' and -; icons: an icon file's name.
    std::string Clean(std::string_view text, bool icon)
    {
        std::string out;
        for (char c : text)
        {
            bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                || (icon ? c == '_' : (c == ' ' || c == '\'' || c == '-'));
            if (ok && out.size() < (icon ? MAX_ICON : MAX_NAME))
                out += c;
        }
        while (!out.empty() && out.back() == ' ')
            out.pop_back();
        while (!out.empty() && out.front() == ' ')
            out.erase(out.begin());
        return out;
    }

    void SendAddon(Player* player, std::string const& payload)
    {
        WorldPacket data;
        std::string const message = std::string(PREFIX) + '\t' + payload;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, message);
        player->SendDirectMessage(&data);
    }

    void Send(Player* player)
    {
        Loadouts* data = Loaded(player);
        if (!data)
            return;
        SendAddon(player, Acore::StringFormat("P,{},{},{}", data->Active, Cards::Eyes(player),
            Prestige::ReachedCap(player) ? 1 : 0));
        std::vector<std::string> locks;
        for (uint8 i = 0; i < MAX_SPECS; ++i)
        {
            Spec const& spec = data->Specs[i];
            SendAddon(player, Acore::StringFormat("S,{},{},{},{},{},{},{}", i, spec.Owned ? 1 : 0, spec.Locked ? 1 : 0,
                spec.Used || i == data->Active ? 1 : 0, spec.Owned ? 0 : PRICES[i], spec.Icon, spec.Name));
        }
        for (auto const& [key, entry] : data->Specs[data->Active].Entries)
            if (entry.Locked)
                locks.push_back(Acore::StringFormat("{}.{}", key.first, key.second));
        SendAddon(player, "L0");
        std::string payload = "L,";
        for (std::string const& lock : locks)
        {
            if (payload.size() > 2 && payload.size() + lock.size() + 1 > MAX_PAYLOAD)
            {
                SendAddon(player, payload);
                payload = "L,";
            }
            payload += (payload.size() > 2 ? ";" : "") + lock;
        }
        if (payload.size() > 2)
            SendAddon(player, payload);
        SendAddon(player, "PE");
    }

    void SaveSpec(Player* player, uint8 index, Spec const& spec)
    {
        std::string name = spec.Name, icon = spec.Icon;
        CharacterDatabase.EscapeString(name);
        CharacterDatabase.EscapeString(icon);
        CharacterDatabase.Execute(Acore::StringFormat("REPLACE INTO `titan_specs` (`guid`, `spec`, `name`, `icon`, "
            "`owned`, `locked`, `used`, `granted`, `picks`, `capstone`) "
            "VALUES ({}, {}, '{}', '{}', {}, {}, {}, {}, {}, {})", player->GetGUID().GetCounter(), index, name, icon,
            spec.Owned ? 1 : 0, spec.Locked ? 1 : 0, spec.Used ? 1 : 0, spec.Granted, spec.Picks, spec.Capstone));
    }

    void SaveEntries(Player* player, uint8 index, Spec const& spec)
    {
        uint32 guid = player->GetGUID().GetCounter();
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        trans->Append(Acore::StringFormat("DELETE FROM `titan_spec_entries` WHERE `guid` = {} AND `spec` = {}", guid,
            index));
        for (auto const& [key, entry] : spec.Entries)
            trans->Append(Acore::StringFormat("INSERT INTO `titan_spec_entries` (`guid`, `spec`, `kind`, `id`, `rank`, "
                "`locked`) VALUES ({}, {}, {}, {}, {}, {})", guid, index, key.first, key.second, entry.Rank,
                entry.Locked ? 1 : 0));
        CharacterDatabase.CommitTransaction(trans);
    }

    void SaveActions(Player* player, uint8 index, Spec const& spec)
    {
        uint32 guid = player->GetGUID().GetCounter();
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        trans->Append(Acore::StringFormat("DELETE FROM `titan_spec_actions` WHERE `guid` = {} AND `spec` = {}", guid,
            index));
        for (auto const& [button, action] : spec.Actions)
            trans->Append(Acore::StringFormat("INSERT INTO `titan_spec_actions` (`guid`, `spec`, `button`, `action`, "
                "`type`) VALUES ({}, {}, {}, {}, {})", guid, index, button, action.first, action.second));
        CharacterDatabase.CommitTransaction(trans);
    }

    void Load(Player* player)
    {
        ObjectGuid guid = player->GetGUID();
        Loadouts& data = loadouts[guid];
        data = Loadouts();
        data.Active = uint8(std::min<uint32>(player->GetPlayerSetting(SETTING_SOURCE, SETTING_ACTIVE).value,
            MAX_SPECS - 1));
        for (uint8 i = 0; i < MAX_SPECS; ++i)
        {
            data.Specs[i].Name = DefaultName(i);
            data.Specs[i].Owned = i < FREE_SPECS;
        }
        bool dormant = player->GetPlayerSetting(SETTING_SOURCE, SETTING_DORMANT).value != 0;
        data.Specs[data.Active].Used = !dormant;
        QueryCallbackProcessor& queries = player->GetSession()->GetQueryProcessor();
        queries.AddCallback(CharacterDatabase.AsyncQuery(Acore::StringFormat("SELECT `spec`, `name`, `icon`, `owned`, "
            "`locked`, `used`, `granted`, `picks`, `capstone` FROM `titan_specs` WHERE `guid` = {}", guid.GetCounter()))
            .WithCallback([guid, dormant](QueryResult result)
        {
            auto itr = loadouts.find(guid);
            if (itr == loadouts.end())
                return;
            if (result)
                do
                {
                    Field* fields = result->Fetch();
                    uint8 index = fields[0].Get<uint8>();
                    if (index >= MAX_SPECS)
                        continue;
                    Spec& spec = itr->second.Specs[index];
                    std::string name = Clean(fields[1].Get<std::string>(), false);
                    spec.Name = name.empty() ? DefaultName(index) : name;
                    spec.Icon = Clean(fields[2].Get<std::string>(), true);
                    spec.Owned = fields[3].Get<uint8>() || index < FREE_SPECS;
                    spec.Locked = fields[4].Get<uint8>();
                    spec.Used = fields[5].Get<uint8>() || (index == itr->second.Active && !dormant);
                    spec.Granted = fields[6].Get<uint32>();
                    spec.Picks = fields[7].Get<uint32>();
                    spec.Capstone = fields[8].Get<uint32>();
                } while (result->NextRow());
            --itr->second.Pending;
        }));
        queries.AddCallback(CharacterDatabase.AsyncQuery(Acore::StringFormat("SELECT `spec`, `kind`, `id`, `rank`, "
            "`locked` FROM `titan_spec_entries` WHERE `guid` = {}", guid.GetCounter()))
            .WithCallback([guid](QueryResult result)
        {
            auto itr = loadouts.find(guid);
            if (itr == loadouts.end())
                return;
            if (result)
                do
                {
                    Field* fields = result->Fetch();
                    uint8 index = fields[0].Get<uint8>();
                    if (index >= MAX_SPECS)
                        continue;
                    auto key = std::make_pair(fields[1].Get<uint8>(), fields[2].Get<uint32>());
                    Entry& entry = itr->second.Specs[index].Entries[key];
                    entry.Rank = fields[3].Get<uint8>();
                    entry.Locked = fields[4].Get<uint8>();
                } while (result->NextRow());
            // The active spec's rows are only its locks (what it holds is the Hero's own).
            auto& active = itr->second.Specs[itr->second.Active].Entries;
            for (auto entry = active.begin(); entry != active.end();)
                entry = entry->second.Locked ? std::next(entry) : active.erase(entry);
            --itr->second.Pending;
        }));
        queries.AddCallback(CharacterDatabase.AsyncQuery(Acore::StringFormat("SELECT `spec`, `button`, `action`, "
            "`type` FROM `titan_spec_actions` WHERE `guid` = {}", guid.GetCounter()))
            .WithCallback([guid](QueryResult result)
        {
            auto itr = loadouts.find(guid);
            if (itr == loadouts.end())
                return;
            if (result)
                do
                {
                    Field* fields = result->Fetch();
                    uint8 index = fields[0].Get<uint8>();
                    // The active spec's bars are the Hero's own.
                    if (index >= MAX_SPECS || index == itr->second.Active)
                        continue;
                    itr->second.Specs[index].Actions[fields[1].Get<uint8>()] = { fields[2].Get<uint32>(),
                        fields[3].Get<uint8>() };
                } while (result->NextRow());
            --itr->second.Pending;
        }));
    }

    // The Hero's action bars, into the spec.
    void KeepActions(Player* player, Spec& spec)
    {
        spec.Actions.clear();
        for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
            if (ActionButton const* action = player->GetActionButton(button))
                spec.Actions[button] = { action->GetAction(), uint8(action->GetType()) };
    }

    // The spec's action bars, the Hero's (after its spells are learned: a button needs its spell known), and the
    // client told.
    void PutBackActions(Player* player, Spec& spec)
    {
        for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
        {
            auto itr = spec.Actions.find(button);
            if (itr != spec.Actions.end())
                player->addActionButton(button, itr->second.first, itr->second.second);
            else if (player->GetActionButton(button))
                player->removeActionButton(button);
        }
        spec.Actions.clear();
        player->SendActionButtons(1);
    }

    TalentEntry const* HeroTalent(uint32 talentId)
    {
        TalentEntry const* talent = sTalentStore.LookupEntry(talentId);
        if (!talent)
            return nullptr;
        TalentTabEntry const* tab = sTalentTabStore.LookupEntry(talent->TalentTab);
        return tab && !tab->petTalentMask && (tab->ClassMask & Wildcard::HERO_TALENT_CLASSMASK) ? talent : nullptr;
    }

    // What the Hero holds now, into the spec (its locks kept on what is still there), with Wildcard's counts.
    void Keep(Player* player, Spec& spec)
    {
        std::map<std::pair<uint8, uint32>, Entry> entries;
        for (Wildcard::Ability const& ability : Wildcard::Abilities())
            if (ability.Offered && Wildcard::KnownRank(player, ability) >= 0)
            {
                auto key = std::make_pair(uint8(ENTRY_ABILITY), ability.Ranks.front().SpellId);
                auto old = spec.Entries.find(key);
                entries[key].Locked = old != spec.Entries.end() && old->second.Locked;
            }
        for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
            if (TalentEntry const* talent = HeroTalent(i))
                if (uint8 rank = Wildcard::TalentRank(player, talent))
                {
                    auto key = std::make_pair(uint8(ENTRY_TALENT), talent->TalentID);
                    auto old = spec.Entries.find(key);
                    Entry& entry = entries[key];
                    entry.Rank = rank;
                    entry.Locked = old != spec.Entries.end() && old->second.Locked;
                }
        spec.Entries = std::move(entries);
        spec.Granted = Wild(player, WILDCARD_GRANTED);
        spec.Picks = Wild(player, WILDCARD_PICKS);
        spec.Capstone = Wild(player, WILDCARD_CAPSTONE);
    }

    // The spec's abilities and talents the Hero's, then what its level is owed since (a spec never used is first
    // made a copy of the one left: Activate).
    void PutBack(Player* player, Spec& spec)
    {
        Wildcard::SetQuiet(true);
        Wildcard::Reset(player);
        // A spec never built: empty, nothing rolled into it, until a prestige in it.
        if (!spec.Used)
        {
            for (uint32 setting : { WILDCARD_GRANTED, WILDCARD_PICKS, WILDCARD_CAPSTONE })
                player->UpdatePlayerSetting(WILDCARD_SOURCE, setting, 0);
            player->UpdatePlayerSetting(SETTING_SOURCE, SETTING_DORMANT, 1);
            player->InitTalentForLevel();
            Wildcard::SetQuiet(false);
            spec.Entries.clear();
            return;
        }
        player->UpdatePlayerSetting(SETTING_SOURCE, SETTING_DORMANT, 0);
        if (spec.Used)
        {
            for (auto const& [key, entry] : spec.Entries)
            {
                if (key.first == ENTRY_ABILITY)
                {
                    if (Wildcard::Ability const* ability = Wildcard::FindAbility(key.second))
                        Wildcard::LearnQuiet(player, *ability);
                }
                else if (TalentEntry const* talent = HeroTalent(key.second))
                {
                    uint8 rank = std::min<uint8>(entry.Rank, MAX_TALENT_RANK);
                    while (rank && !talent->RankID[rank - 1])
                        --rank;
                    if (rank)
                        Wildcard::LearnTalentRank(player, talent, rank - 1, false);
                }
            }
            player->UpdatePlayerSetting(WILDCARD_SOURCE, WILDCARD_GRANTED, spec.Granted);
            player->UpdatePlayerSetting(WILDCARD_SOURCE, WILDCARD_PICKS, spec.Picks);
            player->UpdatePlayerSetting(WILDCARD_SOURCE, WILDCARD_CAPSTONE, spec.Capstone);
        }
        player->InitTalentForLevel();
        Wildcard::CatchUpNow(player);
        Wildcard::SetQuiet(false);
        player->InitTalentForLevel();
        // Now the Hero's own: only its locks stay here.
        for (auto entry = spec.Entries.begin(); entry != spec.Entries.end();)
            entry = entry->second.Locked ? std::next(entry) : spec.Entries.erase(entry);
        spec.Used = true;
    }

    void Activate(Player* player, uint8 index)
    {
        Loadouts* data = Loaded(player);
        ChatHandler handler(player->GetSession());
        if (!data || index >= MAX_SPECS || index == data->Active)
            return;
        Spec& target = data->Specs[index];
        // Every specialization but the first opens once the Hero has reached the level cap (or prestiged).
        if (index > 0 && !Prestige::ReachedCap(player))
        {
            handler.SendSysMessage("Your other specializations open once you reach level 60 for the first time.");
            return;
        }
        if (!target.Owned)
        {
            handler.PSendSysMessage("Buy {} from Silas Darkmoon first.", target.Name);
            return;
        }
        if (player->IsInCombat() || !player->IsAlive() || player->InBattleground() || player->IsInFlight())
        {
            handler.SendSysMessage("Not now: you must be alive, out of combat and out of battlegrounds.");
            return;
        }
        // A prestige run keeps its specialization to the end (the role can change).
        if (Prestige::IsOnRun(player))
        {
            handler.SendSysMessage("Your specialization is locked for this prestige. Reach the level cap to change "
                "it.");
            return;
        }
        Spec& current = data->Specs[data->Active];
        Keep(player, current);
        KeepActions(player, current);
        SaveEntries(player, data->Active, current);
        SaveSpec(player, data->Active, current);
        SaveActions(player, data->Active, current);
        // A spec never built is empty: no abilities, talents, rune or bars until the Hero prestiges in it.
        bool dormant = !target.Used;
        if (dormant)
        {
            target.Entries.clear();
            target.Actions.clear();
            target.Granted = target.Picks = target.Capstone = 0;
        }
        player->SendActionButtons(2);                     // the client's bars cleared while the spells change
        PutBack(player, target);
        PutBackActions(player, target);
        data->Active = index;
        player->UpdatePlayerSetting(SETTING_SOURCE, SETTING_ACTIVE, index);
        SaveSpec(player, index, target);
        SaveEntries(player, index, target);
        SaveActions(player, index, target);
        handler.PSendSysMessage("|cffff8000Specialization:|r {} is active{}.", target.Name,
            dormant ? ": it is empty until you prestige in it, which builds it from level 1" : "");
        Send(player);
        Classless::SendKnown(player);
        Classless::SendState(player);
        LegendaryRunes::OnSpecChanged(player);
    }

    bool IsKnown(Player* player, uint8 kind, uint32 id)
    {
        if (kind == ENTRY_ABILITY)
        {
            Wildcard::Ability const* ability = Wildcard::FindAbility(id);
            return ability && ability->Offered && Wildcard::KnownRank(player, *ability) >= 0;
        }
        TalentEntry const* talent = HeroTalent(id);
        return talent && Wildcard::TalentRank(player, talent);
    }

    void SetEntryLock(Player* player, uint8 kind, uint32 id, bool locked)
    {
        Loadouts* data = Loaded(player);
        if (!data || kind > ENTRY_TALENT || !IsKnown(player, kind, id))
            return;
        Spec& spec = data->Specs[data->Active];
        if (locked)
            spec.Entries[{ kind, id }].Locked = true;
        else
            spec.Entries.erase({ kind, id });
        SaveEntries(player, data->Active, spec);
        Send(player);
    }

    // Before the kit locks (Wildcard::LOCK_LEVEL) an ability is unlearned for free: its pick comes back, to pick
    // another in the Classless window. No Eye of Unlearning: those are for rerolls once the kit is locked.
    void UnlearnFree(Player* player, uint32 id)
    {
        Loadouts* data = Loaded(player);
        ChatHandler handler(player->GetSession());
        Wildcard::Ability const* ability = Wildcard::FindAbility(id);
        if (!data || !ability || !ability->Offered || !IsKnown(player, ENTRY_ABILITY, id))
            return;
        if (Wildcard::IsKitLocked(player))
        {
            handler.SendSysMessage("Your abilities are locked: reroll it with an Eye of Unlearning instead.");
            return;
        }
        Spec& spec = data->Specs[data->Active];
        auto entry = spec.Entries.find({ ENTRY_ABILITY, id });
        if (spec.Locked || (entry != spec.Entries.end() && entry->second.Locked))
        {
            handler.SendSysMessage("Unlock it first (right-click it in the Classless window).");
            return;
        }
        if (player->IsInCombat())
        {
            handler.SendSysMessage("Not in combat.");
            return;
        }
        Wildcard::Unlearn(player, *ability);
        // Back to KIT_SIZE with the picks: never more (unlearning twice gave a fifth ability).
        uint32 known = Wildcard::KitAbilities(player);
        Wildcard::SetPicks(player, known < Wildcard::KIT_SIZE ? Wildcard::KIT_SIZE - known : 0);
        handler.SendSysMessage("Unlearned. Pick another ability in the Classless window.");
        Classless::SendKnown(player);
        Classless::SendState(player);
        Send(player);
    }

    void Reroll(Player* player, uint8 kind, uint32 id)
    {
        Loadouts* data = Loaded(player);
        ChatHandler handler(player->GetSession());
        if (!data)
        {
            handler.SendSysMessage("Your specializations are still loading. Try again in a moment.");
            return;
        }
        if (kind > ENTRY_TALENT || !IsKnown(player, kind, id))
            return;
        Spec& spec = data->Specs[data->Active];
        auto entry = spec.Entries.find({ kind, id });
        if (spec.Locked || (entry != spec.Entries.end() && entry->second.Locked))
        {
            handler.SendSysMessage("Unlock it first (right-click it in the Classless window).");
            return;
        }
        if (player->IsInCombat())
        {
            handler.SendSysMessage("Not in combat.");
            return;
        }
        if (!Cards::Eyes(player))
        {
            char const* text = "You don't have any Eyes of Unlearning. Visit Silas Darkmoon in any of the Main Cities.";
            handler.SendSysMessage(text);
            WorldPacket error(SMSG_NOTIFICATION, strlen(text) + 1);
            error << text;
            player->SendDirectMessage(&error);
            return;
        }

        if (kind == ENTRY_ABILITY)
        {
            Wildcard::Ability const* old = Wildcard::FindAbility(id);
            // The new one first: the old one, still known, cannot come again; nor can its name for 25 rerolls.
            std::string gone = Wildcard::Blacklist(player, old->Ranks.front().SpellId);
            Wildcard::Ability const* rolled = old->Capstone ? Wildcard::GrantNewCapstone(player)
                : Wildcard::GrantRandom(player, "rerolled into");
            if (!rolled)
            {
                Wildcard::Unblacklist(player, gone);
                handler.SendSysMessage("There is nothing else to reroll it into.");
                return;
            }
            Wildcard::Unlearn(player, *old);
            Wildcard::CountReroll(player);
            Cards::SpendEye(player, false);
            Classless::Reveal(player, *rolled, player->GetLevel());
        }
        else
        {
            // One talent for one: a roll gives a talent at its top rank (Wildcard counts talents, not their ranks).
            // Nothing else to roll into: the talent comes back, and the Eye is kept.
            TalentEntry const* talent = HeroTalent(id);
            uint8 rank = Wildcard::RemoveTalent(player, talent);
            if (!rank)
                return;
            // Every talent of its name (Toughness of any tree) stays out for 25 rerolls.
            std::string gone = Wildcard::Blacklist(player, talent->RankID[0]);
            if (!Wildcard::GrantTalentExcept(player, id, true))
            {
                Wildcard::Unblacklist(player, gone);
                Wildcard::SetQuiet(true);
                Wildcard::LearnTalentRank(player, talent, rank - 1, false);
                Wildcard::SetQuiet(false);
                handler.SendSysMessage("There is nothing else to reroll it into.");
                Classless::SendKnown(player);
                return;
            }
            player->InitTalentForLevel();
            Wildcard::CountReroll(player);
            Cards::SpendEye(player, true);
        }
        Classless::SendKnown(player);
        Send(player);
    }

    void Buy(Player* player, uint8 index)
    {
        Loadouts* data = Loaded(player);
        if (!data || index >= MAX_SPECS || data->Specs[index].Owned || !Cards::BesideSilas(player))
            return;
        uint32 price = PRICES[index];
        Emberspark::Spend(player, price, "specialization", [index, price](Player* online, bool paid)
        {
            Loadouts* owner = Loaded(online);
            if (!owner)
                return;
            ChatHandler handler(online->GetSession());
            if (!paid)
                handler.PSendSysMessage("You need {} Emberspark for that.", Emberspark::Format(price));
            else
            {
                owner->Specs[index].Owned = true;
                SaveSpec(online, index, owner->Specs[index]);
                handler.PSendSysMessage("|cffff8000Specialization:|r {} is yours. Activate it in the Classless "
                    "window.", owner->Specs[index].Name);
            }
            Send(online);
        });
    }
}

namespace Specs
{
    bool HasLocks(Player* player)
    {
        Loadouts* data = Loaded(player);
        if (!data)
            return true;
        for (Spec const& spec : data->Specs)
        {
            if (spec.Locked)
                return true;
            for (auto const& [key, entry] : spec.Entries)
                if (entry.Locked)
                    return true;
        }
        return false;
    }

    bool IsDormant(Player* player)
    {
        return player->GetPlayerSetting(SETTING_SOURCE, SETTING_DORMANT).value != 0;
    }

    uint8 ActiveSpec(Player* player)
    {
        return uint8(std::min<uint32>(player->GetPlayerSetting(SETTING_SOURCE, SETTING_ACTIVE).value, MAX_SPECS - 1));
    }

    void OnPrestige(Player* player)
    {
        Loadouts* data = Loaded(player);
        if (!data)
            return;
        // The active spec starts over (built from level 1 by the new Hero's rolls, a dormant one included); the
        // others keep their builds.
        uint8 active = data->Active;
        CharacterDatabase.Execute(Acore::StringFormat("DELETE FROM `titan_spec_entries` WHERE `guid` = {} AND "
            "`spec` = {}", player->GetGUID().GetCounter(), active));
        CharacterDatabase.Execute(Acore::StringFormat("DELETE FROM `titan_spec_actions` WHERE `guid` = {} AND "
            "`spec` = {}", player->GetGUID().GetCounter(), active));
        Spec& spec = data->Specs[active];
        spec.Entries.clear();
        spec.Actions.clear();
        spec.Used = true;
        spec.Granted = spec.Picks = spec.Capstone = 0;
        SaveSpec(player, active, spec);
        player->UpdatePlayerSetting(SETTING_SOURCE, SETTING_DORMANT, 0);
    }
}

class titan_specs : public PlayerScript
{
public:
    titan_specs() : PlayerScript("titan_specs", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_DELETE,
        PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (Wildcard::IsHero(player))
            Load(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        loadouts.erase(player->GetGUID());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        CharacterDatabase.Execute(Acore::StringFormat("DELETE FROM `titan_specs` WHERE `guid` = {}",
            guid.GetCounter()));
        CharacterDatabase.Execute(Acore::StringFormat("DELETE FROM `titan_spec_entries` WHERE `guid` = {}",
            guid.GetCounter()));
        CharacterDatabase.Execute(Acore::StringFormat("DELETE FROM `titan_spec_actions` WHERE `guid` = {}",
            guid.GetCounter()));
    }

    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 language, std::string& msg,
        Player* receiver) override
    {
        std::string const prefix = std::string(PREFIX) + '\t';
        if (language != LANG_ADDON || receiver != player || msg.compare(0, prefix.size(), prefix) != 0)
            return true;

        std::string_view request = std::string_view(msg).substr(prefix.size());
        std::vector<std::string_view> parts = Acore::Tokenize(request, ',', true);
        auto number = [&parts](std::size_t index) -> uint32
        {
            Optional<uint32> value = index < parts.size() ? Acore::StringTo<uint32>(parts[index]) : Optional<uint32>();
            return value ? *value : UINT32_MAX;
        };
        Loadouts* data = Loaded(player);
        if (parts.empty() || !data)
            return false;
        std::string_view command = parts[0];
        uint32 spec = number(1);
        if (command == "LIST")
            Send(player);
        else if (command == "ACTIVATE")
            Activate(player, uint8(std::min<uint32>(spec, MAX_SPECS)));
        else if (command == "BUY" && spec < MAX_SPECS)
            Buy(player, uint8(spec));
        else if (command == "LOCK" && spec < MAX_SPECS && data->Specs[spec].Owned)
        {
            data->Specs[spec].Locked = number(2) == 1;
            SaveSpec(player, uint8(spec), data->Specs[spec]);
            Send(player);
        }
        else if ((command == "NAME" || command == "ICON") && spec < MAX_SPECS && parts.size() >= 3)
        {
            // A name may hold commas: the rest of the line.
            std::size_t at = request.find(',', request.find(',') + 1);
            std::string value = Clean(request.substr(at + 1), command == "ICON");
            if (command == "NAME")
                data->Specs[spec].Name = value.empty() ? DefaultName(uint8(spec)) : value;
            else
                data->Specs[spec].Icon = value;
            SaveSpec(player, uint8(spec), data->Specs[spec]);
            Send(player);
        }
        else if (command == "LOCKENTRY" && parts.size() == 4)
            SetEntryLock(player, uint8(std::min<uint32>(spec, 2)), number(2), number(3) == 1);
        else if (command == "REROLL" && parts.size() == 3)
            Reroll(player, uint8(std::min<uint32>(spec, 2)), number(2));
        else if (command == "UNLEARN" && parts.size() == 2)
            UnlearnFree(player, spec);
        return false;
    }
};

void AddSC_specs()
{
    new titan_specs();
}
