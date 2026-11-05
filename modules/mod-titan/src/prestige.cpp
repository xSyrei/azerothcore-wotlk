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

#include "prestige.h"
#include "emberspark.h"
#include "wildcard.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Item.h"
#include "Mail.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "StringFormat.h"
#include "World.h"
#include "WorldSession.h"
#include <vector>

// Prestige: at the level cap a Hero talks to the Prestige NPC (npc_titan_prestige, data/sql/db-world/
// titan_prestige.sql) and starts again from level 1:
// - lost: the level, every Wildcard ability and talent (a new random kit and both tomes, as a new Hero), every quest
//   (in the log or done: all can be done, and rewarded, again);
// - kept: gear, gold, Emberspark, professions, mounts, reputation, achievements and the role. Gear above level 1 goes
//   to the bags (what does not fit, by mail) and back on as soon as its level requirement is met;
// - given: 500 Emberspark, 2 Titan Souls of Rested Experience and Prestigious (+10% experience) until level 60, where
//   the run ends with 5,000 Emberspark more. The role is locked during the run.
// Everything is kept in the character's settings (EnablePlayerSettings = 1, which Wildcard needs too).
namespace
{
    constexpr char const* SETTING_SOURCE = "mod-titan-prestige";
    constexpr uint32 SETTING_RANK = 0;          // prestiges done
    constexpr uint32 SETTING_RUN = 1;           // 1 during a run: from a prestige to level 60
    constexpr uint32 SETTING_RUN_STARTED = 2;   // unix time of the last prestige
    constexpr uint32 SETTING_FASTEST = 3;       // the fastest run to level 60, in seconds (0: none yet)
    constexpr uint32 SETTING_GEAR = 10;         // + equipment slot: the item taken off there (guid counter; 0: none)

    constexpr uint32 SPELL_PRESTIGIOUS = 90607; // the buff (client-patch/patch_player.py); the bonus is OnPlayerGiveXP
    constexpr uint32 XP_BONUS_PCT = 10;
    constexpr uint32 SPARK_ON_PRESTIGE = 500;
    constexpr uint32 SPARK_AT_MAX_LEVEL = 5000;
    constexpr uint32 ITEM_SOUL_OF_RESTED_EXPERIENCE = 910001;
    constexpr uint32 SOULS_ON_PRESTIGE = 2;
    constexpr uint32 NPC_PRESTIGE = 910100;

    enum PrestigeTexts : uint32
    {
        TEXT_READY = 910100,        // npc_text: at the cap
        TEXT_NOT_YET = 910101,      // below it
        TEXT_DETAILS = 910102,      // what is lost, kept and given
    };

    enum PrestigeActions : uint32
    {
        ACTION_MENU = 1,
        ACTION_PRESTIGE,
        ACTION_DETAILS,
    };

    uint32 Get(Player* player, uint32 index)
    {
        return player->GetPlayerSetting(SETTING_SOURCE, index).value;
    }

    void Set(Player* player, uint32 index, uint32 value)
    {
        player->UpdatePlayerSetting(SETTING_SOURCE, index, value);
    }

    uint8 MaxLevel()
    {
        return uint8(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));
    }

    bool OnRun(Player* player)
    {
        return Get(player, SETTING_RUN) && player->GetLevel() < MaxLevel();
    }

    // 3h 12m, or 12m.
    std::string Duration(uint32 seconds)
    {
        uint32 minutes = seconds / MINUTE % 60, hours = seconds / HOUR;
        return hours ? Acore::StringFormat("{}h {}m", hours, minutes) : Acore::StringFormat("{}m", minutes);
    }

    // Why the player cannot prestige now; empty when they can.
    std::string CannotPrestige(Player* player)
    {
        if (!Wildcard::IsHero(player))
            return "Only a Hero can be forged anew.";
        if (player->GetLevel() < MaxLevel())
            return Acore::StringFormat("Come back at level {}.", MaxLevel());
        if (!player->IsAlive() || player->IsInCombat() || player->IsInFlight())
            return "Not now: you must be alive, out of combat and on the ground.";
        if (player->GetMap()->Instanceable() || player->InBattlegroundQueue())
            return "Leave your dungeon, battleground or queue first.";
        return {};
    }

    // Items into the bags; what does not fit comes by mail from the Prestige NPC.
    void GiveItems(Player* player, uint32 entry, uint32 count)
    {
        uint32 noSpace = 0;
        ItemPosCountVec dest;
        if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, entry, count, &noSpace) == EQUIP_ERR_OK
            || count > noSpace)
            if (Item* item = player->StoreNewItem(dest, entry, true))
                player->SendNewItem(item, count - noSpace, true, false);
        if (noSpace)
            player->SendItemRetrievalMail(entry, noSpace);
    }

    // Takes off every piece the player could not wear at level 1, into the bags (the rest by mail), and remembers the
    // slot of each, to put it back on once its level allows (Reequip).
    void StashGear(Player* player)
    {
        std::vector<Item*> toMail;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Set(player, SETTING_GEAR + slot, 0);
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item || item->GetTemplate()->RequiredLevel <= 1)
                continue;

            Set(player, SETTING_GEAR + slot, item->GetGUID().GetCounter());
            ItemPosCountVec dest;
            if (player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false) == EQUIP_ERR_OK)
            {
                player->RemoveItem(INVENTORY_SLOT_BAG_0, slot, true);
                player->StoreItem(dest, item, true);
            }
            else
            {
                player->MoveItemFromInventory(INVENTORY_SLOT_BAG_0, slot, true);
                toMail.push_back(item);
            }
        }

        // Mailed as the core mails gear it takes off (Player::AutoUnequipOffhandIfNeed), MAX_MAIL_ITEMS a letter.
        for (std::size_t first = 0; first < toMail.size(); first += MAX_MAIL_ITEMS)
        {
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            MailDraft draft("Your gear, kept safe", "Your bags were full, so the forge kept these for you. They go "
                "back on by themselves once you reach their level, as soon as they are in your bags.");
            for (std::size_t i = first; i < std::min(toMail.size(), first + MAX_MAIL_ITEMS); ++i)
            {
                toMail[i]->DeleteFromInventoryDB(trans);
                toMail[i]->SaveToDB(trans);
                draft.AddItem(toMail[i]);
            }
            draft.SendMailTo(trans, player, MailSender(MAIL_CREATURE, NPC_PRESTIGE), MAIL_CHECK_MASK_COPIED);
            CharacterDatabase.CommitTransaction(trans);
        }
    }

    // Puts stashed gear back on in its old slot once its level allows, whatever the Hero wears there now (to the
    // bags). Gear still in the mail goes on once taken out. At the cap the list is cleared: everything fits.
    void Reequip(Player* player)
    {
        std::vector<std::string> names;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            uint32 counter = Get(player, SETTING_GEAR + slot);
            if (!counter)
                continue;

            Item* item = player->GetItemByGuid(ObjectGuid::Create<HighGuid::Item>(counter));
            if (item && item->IsEquipped())                         // the Hero put it on already
                Set(player, SETTING_GEAR + slot, 0);
            else if (item && Player::IsInventoryPos(item->GetPos())
                && item->GetTemplate()->RequiredLevel <= player->GetLevel())
            {
                uint16 dest;
                if (player->CanEquipItem(slot, dest, item, true) == EQUIP_ERR_OK)
                {
                    player->SwapItem(item->GetPos(), dest);
                    Set(player, SETTING_GEAR + slot, 0);
                    names.push_back(item->GetTemplate()->Name1);
                }
            }

            if (player->GetLevel() >= MaxLevel())
                Set(player, SETTING_GEAR + slot, 0);
        }

        if (!names.empty())
        {
            std::string list;
            for (std::string const& name : names)
                list += (list.empty() ? "" : ", ") + name;
            ChatHandler(player->GetSession()).PSendSysMessage("|cffff8000Prestige:|r back on: {}.", list);
        }
    }

    // Every quest can be done, and rewarded, again: the log is emptied and the done ones forgotten (but the hidden
    // tracking quests the game uses as flags).
    void ForgetQuests(Player* player)
    {
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
            if (uint32 questId = player->GetQuestSlotQuestId(slot))
            {
                player->SetQuestSlot(slot, 0);
                player->TakeQuestSourceItem(questId, false);
                player->RemoveActiveQuest(questId, false);
            }

        std::vector<uint32> rewarded(player->getRewardedQuests().begin(), player->getRewardedQuests().end());
        for (uint32 questId : rewarded)
            if (Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
                !quest || !quest->HasFlag(QUEST_FLAGS_TRACKING))
                player->RemoveRewardedQuest(questId);
    }

    void DoPrestige(Player* player)
    {
        uint32 rank = Get(player, SETTING_RANK) + 1;
        Set(player, SETTING_RANK, rank);
        Set(player, SETTING_RUN, 1);
        Set(player, SETTING_RUN_STARTED, uint32(GameTime::GetGameTime().count()));

        player->Dismount();
        player->RemoveAurasByType(SPELL_AURA_MOUNTED);
        StashGear(player);
        ForgetQuests(player);
        // A new Hero again: the next level change (this one) rolls a new kit, the tomes come back.
        Wildcard::Reset(player);
        player->GiveLevel(1);
        player->InitTalentForLevel();
        player->SetUInt32Value(PLAYER_XP, 0);
        GiveItems(player, Wildcard::TOME_OF_RANDOMIZATION, 1);
        GiveItems(player, Wildcard::TOME_OF_SELECTION, 1);

        GiveItems(player, ITEM_SOUL_OF_RESTED_EXPERIENCE, SOULS_ON_PRESTIGE);
        player->AddAura(SPELL_PRESTIGIOUS, player);
        Emberspark::Add(player->GetSession()->GetAccountId(), SPARK_ON_PRESTIGE, "prestige");

        ChatHandler handler(player->GetSession());
        handler.PSendSysMessage("|cffff8000Prestige {}:|r you are forged anew. Your gear waits in your bags and goes "
            "back on as you reach its level; your role is locked until level {}.", rank, MaxLevel());
        Emberspark::Announce(player, SPARK_ON_PRESTIGE, "for your prestige");
        ChatHandler(nullptr).SendWorldText(Acore::StringFormat(
            "|cffff8000[Prestige]|r {} has been forged anew: Prestige {}!", player->GetName(), rank));

        // Saved before the trip home, the race's starting point.
        player->SaveToDB(false, false);
        if (PlayerInfo const* info = sObjectMgr->GetPlayerInfo(player->getRace(), player->getClass()))
            player->TeleportTo(info->mapId, info->positionX, info->positionY, info->positionZ, info->orientation);
    }

    // The run ends at the cap: 5,000 Emberspark, the fastest run kept, Prestigious gone, the role free.
    void FinishRun(Player* player)
    {
        Set(player, SETTING_RUN, 0);
        uint32 now = uint32(GameTime::GetGameTime().count()), started = Get(player, SETTING_RUN_STARTED);
        uint32 took = now > started ? now - started : 0, fastest = Get(player, SETTING_FASTEST);
        if (!fastest || took < fastest)
            Set(player, SETTING_FASTEST, took);
        player->RemoveAurasDueToSpell(SPELL_PRESTIGIOUS);
        Emberspark::Add(player->GetSession()->GetAccountId(), SPARK_AT_MAX_LEVEL, "prestige max level");

        uint32 rank = Get(player, SETTING_RANK);
        ChatHandler(player->GetSession()).PSendSysMessage("|cffff8000Prestige {}:|r level {} again, in {}. You can "
            "change your role again, and prestige once more.", rank, MaxLevel(), Duration(took));
        Emberspark::Announce(player, SPARK_AT_MAX_LEVEL, "for reaching the summit again");
        ChatHandler(nullptr).SendWorldText(Acore::StringFormat(
            "|cffff8000[Prestige]|r {} has completed Prestige {} in {}!", player->GetName(), rank, Duration(took)));
    }

    void ShowMenu(Player* player, ObjectGuid npc)
    {
        Emberspark::Query(player, [npc](Player* online, uint32 balance)
        {
            uint32 rank = Get(online, SETTING_RANK), next = rank + 1;
            bool ready = Wildcard::IsHero(online) && online->GetLevel() >= MaxLevel();
            ClearGossipMenuFor(online);
            if (ready)
                AddGossipItemFor(online, GOSSIP_ICON_BATTLE, Acore::StringFormat("Forge me anew: Prestige {}.", next),
                    GOSSIP_SENDER_MAIN, ACTION_PRESTIGE, Acore::StringFormat("Return to level 1 as Prestige {}?\n\n"
                    "Your Wildcard abilities and talents are rerolled, and every quest can be done again. Your gear "
                    "waits in your bags until you reach its level. Your role is locked until level {}.\n\nYou get "
                    "{} Emberspark, {} Titan Souls of Rested Experience and Prestigious (+{}% experience).", next,
                    MaxLevel(), SPARK_ON_PRESTIGE, SOULS_ON_PRESTIGE, XP_BONUS_PCT), 0, false);
            AddGossipItemFor(online, GOSSIP_ICON_CHAT, "What do I lose, keep and get?", GOSSIP_SENDER_MAIN,
                ACTION_DETAILS);
            AddGossipItemFor(online, GOSSIP_ICON_MONEY_BAG, Acore::StringFormat("Emberspark: {}",
                Emberspark::Format(balance)), GOSSIP_SENDER_MAIN, ACTION_MENU);
            if (rank)
            {
                std::string line = Acore::StringFormat("Prestige {}", rank);
                if (OnRun(online))
                    line += Acore::StringFormat(", this run: {}", Duration(uint32(GameTime::GetGameTime().count())
                        - Get(online, SETTING_RUN_STARTED)));
                if (uint32 fastest = Get(online, SETTING_FASTEST))
                    line += Acore::StringFormat(", fastest: {}", Duration(fastest));
                AddGossipItemFor(online, GOSSIP_ICON_TABARD, line, GOSSIP_SENDER_MAIN, ACTION_MENU);
            }
            SendGossipMenuFor(online, ready ? TEXT_READY : TEXT_NOT_YET, npc);
        });
    }
}

namespace Prestige
{
    uint32 GetRank(Player* player)
    {
        return Get(player, SETTING_RANK);
    }

    bool IsRoleLocked(Player* player)
    {
        return Wildcard::IsHero(player) && OnRun(player);
    }
}

class npc_titan_prestige : public CreatureScript
{
public:
    npc_titan_prestige() : CreatureScript("npc_titan_prestige") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ShowMenu(player, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        ClearGossipMenuFor(player);
        if (action == ACTION_PRESTIGE)
        {
            CloseGossipMenuFor(player);
            if (std::string why = CannotPrestige(player); !why.empty())
                ChatHandler(player->GetSession()).SendSysMessage(why);
            else
                DoPrestige(player);
        }
        else if (action == ACTION_DETAILS)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back.", GOSSIP_SENDER_MAIN, ACTION_MENU);
            SendGossipMenuFor(player, TEXT_DETAILS, creature);
        }
        else
            ShowMenu(player, creature->GetGUID());
        return true;
    }
};

class titan_prestige : public PlayerScript
{
public:
    titan_prestige() : PlayerScript("titan_prestige", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LEVEL_CHANGED,
        PLAYERHOOK_ON_GIVE_EXP
    }) { }

    // Prestigious while on a run (put back if a GM took it off), and gear taken out of the mail back on.
    void OnPlayerLogin(Player* player) override
    {
        if (!Wildcard::IsHero(player))
            return;
        if (OnRun(player))
        {
            if (!player->HasAura(SPELL_PRESTIGIOUS))
                player->AddAura(SPELL_PRESTIGIOUS, player);
        }
        else
            player->RemoveAurasDueToSpell(SPELL_PRESTIGIOUS);
        Reequip(player);
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        if (!Wildcard::IsHero(player))
            return;
        Reequip(player);
        if (Get(player, SETTING_RUN) && player->GetLevel() >= MaxLevel())
            FinishRun(player);
    }

    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* /*victim*/, uint8 /*xpSource*/) override
    {
        if (OnRun(player) && Wildcard::IsHero(player))
            amount += amount * XP_BONUS_PCT / 100;
    }
};

void AddSC_prestige()
{
    new npc_titan_prestige();
    new titan_prestige();
}
