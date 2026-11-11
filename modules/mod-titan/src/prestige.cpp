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
#include "cards.h"
#include "hero_equipment.h"
#include "specs.h"
#include "emberspark.h"
#include "wildcard.h"
#include "Bag.h"
#include "Chat.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Item.h"
#include "Mail.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Creature.h"
#include "ScriptedGossip.h"
#include "StringFormat.h"
#include "World.h"
#include "WorldSessionMgr.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <string_view>
#include <unordered_map>
#include <vector>

// Prestige: at the level cap a Hero talks to the Keeper of the Ember (npc_titan_prestige, data/sql/db-world/
// titan_prestige.sql), whose Prestige window (the Emberforge_Prestige addon) starts them again from level 1:
// - lost: the level, every Wildcard ability and talent (a new random kit and both tomes, as a new Hero), every quest
//   (in the log or done: all can be done, and rewarded, again);
// - kept: gear, gold, Emberspark, professions, mounts, reputation, achievements and the role (it can change during the
//   run; the specialization cannot: specs.cpp). Every piece worn is packed into a Prestige Gear Cache (one item in the
//   bags, by mail when they are full): used, it gives the gear back (the very items: enchantments, gems and all), and
//   each piece goes back on in its old slot as soon as the Hero reaches its level (Reequip);
// - given: the race's starting outfit, worn, and the starter weapons in the bags (a new Hero's), 500 Emberspark, 2
//   Titan Souls of Rested Experience, Prestigious (+10% experience) until level 60, and the quest Trial of Ember:
//   Prestigious: done at level 60, where the run ends; turned in to the Keeper, it gives 5,000 Emberspark.
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
    constexpr uint32 QUEST_TRIAL = 910100;      // Trial of Ember: Prestigious (titan_prestige.sql)
    constexpr uint32 ITEM_PRESTIGE_CACHE = 910021;  // Prestige Gear Cache (titan_prestige.sql)

    constexpr char const* PREFIX = "EFPR";     // addon whispers with Emberforge_Prestige (the window)
    constexpr float NPC_RANGE = 15.0f;          // a prestige asked from the window: this close to the Keeper

    // The Keeper each player last opened the window at, to check they are still beside it when they prestige.
    std::unordered_map<ObjectGuid, ObjectGuid> openedAt;

    // A system message to every player online. (ChatHandler(nullptr).SendWorldText would use a session it has not:
    // a crash.)
    void Broadcast(std::string const& text)
    {
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, LANG_UNIVERSAL, nullptr, nullptr, text);
        sWorldSessionMgr->SendGlobalMessage(&data);
    }

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

    // Classic items with no required level ask for their item level less this; later ones the level cap.
    constexpr uint32 CLASSIC_ITEM_LEVEL = 70;
    constexpr uint32 CLASSIC_LEVEL_GAP = 10;

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
        if (Specs::HasLocks(player))
            return "Unlock your locked specializations and abilities first (Classless window: right-click them).";
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

    // Takes off every piece above the level given (Prestige::EquipLevel), into the bags (the rest by mail), and
    // remembers the slot of each, to put it back on once its level allows (Reequip). Returns how many.
    uint32 StashGear(Player* player, uint8 level)
    {
        std::vector<Item*> toMail;
        uint32 stashed = 0;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item || Prestige::EquipLevel(item) <= level)
                continue;

            ++stashed;

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
        return stashed;
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
                && Prestige::EquipLevel(item) <= player->GetLevel())
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

    // Packs every piece worn into a new Prestige Gear Cache: the items leave the inventory but stay in the database
    // (item_instance, as mailed ones), listed by the cache's guid in titan_prestige_gear (db-characters); the slot of
    // each is remembered for Reequip. The cache goes to the bags, or by mail when they are full. Returns how many.
    uint32 PackGear(Player* player)
    {
        std::vector<std::pair<uint8, Item*>> worn;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                worn.emplace_back(slot, item);
        if (worn.empty())
            return 0;

        Item* cache = Item::CreateItem(ITEM_PRESTIGE_CACHE, 1, player);
        if (!cache)
            return 0;                               // no template: the gear stays on
        cache->SetBinding(true);
        uint32 cacheGuid = cache->GetGUID().GetCounter();

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        for (auto const& [slot, item] : worn)
        {
            uint32 itemGuid = item->GetGUID().GetCounter();
            Set(player, SETTING_GEAR + slot, itemGuid);
            player->MoveItemFromInventory(INVENTORY_SLOT_BAG_0, slot, true);
            item->DeleteFromInventoryDB(trans);
            item->SetState(ITEM_CHANGED);
            item->SaveToDB(trans);
            trans->Append("INSERT INTO `titan_prestige_gear` (`cache_guid`, `item_guid`, `slot`) VALUES ({}, {}, {})",
                cacheGuid, itemGuid, slot);
            delete item;
        }

        ItemPosCountVec dest;
        if (player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, cache, false) == EQUIP_ERR_OK)
        {
            player->StoreItem(dest, cache, true);
            player->SendNewItem(cache, 1, true, false);
        }
        else
        {
            cache->SaveToDB(trans);
            MailDraft("Your gear, kept safe", "Your bags were full, so the forge sent your Prestige Gear Cache here. "
                "Use it to take your gear back.")
                .AddItem(cache)
                .SendMailTo(trans, player, MailSender(MAIL_CREATURE, NPC_PRESTIGE), MAIL_CHECK_MASK_COPIED);
        }
        CharacterDatabase.CommitTransaction(trans);
        return uint32(worn.size());
    }

    // A new Hero's gear: the race's starting outfit worn (Player::Create's: CharStartOutfit.dbc and
    // playercreateinfo_item; a unique item they still have, the Hearthstone, skipped), the starter weapons in the bags.
    void GiveStarterGear(Player* player)
    {
        std::vector<std::pair<uint32, uint32>> items;
        if (CharStartOutfitEntry const* outfit =
            GetCharStartOutfitEntry(player->getRace(), player->getClass(), player->getGender()))
            for (int32 itemId : outfit->ItemId)
                if (itemId > 0)
                    if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(uint32(itemId)))
                        items.emplace_back(uint32(itemId), std::max<uint32>(proto->BuyCount, 1));
        if (PlayerInfo const* info = sObjectMgr->GetPlayerInfo(player->getRace(), player->getClass()))
            for (PlayerCreateInfoItem const& item : info->item)
                items.emplace_back(item.item_id, item.item_amount);

        for (auto const& [itemId, count] : items)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
            if (!proto || (proto->MaxCount > 0 && player->HasItemCount(itemId, uint32(proto->MaxCount), true)))
                continue;
            uint16 dest;
            if (player->CanEquipNewItem(NULL_SLOT, dest, itemId, false) == EQUIP_ERR_OK)
                player->EquipNewItem(dest, itemId, true);
            else
                GiveItems(player, itemId, count);
        }
        for (auto const& [itemId, count] : HeroEquipment::StarterBagKit())
            GiveItems(player, itemId, count);
    }

    // Every quest can be done, and rewarded, again: the log is emptied, the done ones forgotten (but the hidden
    // tracking quests the game uses as flags), and the dailies, weeklies and monthlies done (holidays reset theirs
    // each year).
    // Quest givers show theirs again once the Hero is home (the trip sends the world anew).
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
                player->RemoveRewardedQuest(questId, false);
        player->ResetDailyQuestStatus();
        player->ResetWeeklyQuestStatus();
        player->ResetMonthlyQuestStatus();
    }

    void DoPrestige(Player* player)
    {
        uint32 rank = Get(player, SETTING_RANK) + 1;
        Set(player, SETTING_RANK, rank);
        Set(player, SETTING_RUN, 1);
        Set(player, SETTING_RUN_STARTED, uint32(GameTime::GetGameTime().count()));

        player->Dismount();
        player->RemoveAurasByType(SPELL_AURA_MOUNTED);
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            Set(player, SETTING_GEAR + slot, 0);
        uint32 packed = PackGear(player);
        ForgetQuests(player);
        // A new Hero again: the next level change (this one) rolls a new kit, the tomes come back.
        Wildcard::Reset(player);
        Specs::OnPrestige(player);
        Cards::ResetPrices(player);
        player->GiveLevel(1);
        player->InitTalentForLevel();
        player->SetUInt32Value(PLAYER_XP, 0);
        GiveStarterGear(player);
        GiveItems(player, Wildcard::TOME_OF_RANDOMIZATION, 1);
        GiveItems(player, Wildcard::TOME_OF_SELECTION, 1);

        GiveItems(player, ITEM_SOUL_OF_RESTED_EXPERIENCE, SOULS_ON_PRESTIGE);
        player->AddAura(SPELL_PRESTIGIOUS, player);
        // The run's quest (the quests were all forgotten above): done at level 60, turned in to the Keeper.
        if (Quest const* trial = sObjectMgr->GetQuestTemplate(QUEST_TRIAL))
            player->AddQuestAndCheckCompletion(trial, nullptr);
        Emberspark::Add(player->GetSession()->GetAccountId(), SPARK_ON_PRESTIGE, "prestige");

        ChatHandler handler(player->GetSession());
        handler.PSendSysMessage("|cffff8000Prestige {}:|r you are forged anew, in a new Hero's gear.{} Your "
            "specialization is locked until level {}, where the Keeper awaits you with your reward.", rank,
            packed ? " Your old gear is packed in your |cffa335eePrestige Gear Cache|r: use it to take it back, and "
            "each piece goes back on as you reach its level." : "", MaxLevel());
        Emberspark::Announce(player, SPARK_ON_PRESTIGE, "for your prestige");
        Broadcast(Acore::StringFormat(
            "|cffff8000[Prestige]|r {} has been forged anew: Prestige {}!", player->GetName(), rank));

        // Saved before the trip home, the race's starting point.
        player->SaveToDB(false, false);
        if (PlayerInfo const* info = sObjectMgr->GetPlayerInfo(player->getRace(), player->getClass()))
            player->TeleportTo(info->mapId, info->positionX, info->positionY, info->positionZ, info->orientation);
    }

    // The run ends at the cap: the fastest run kept, Prestigious gone, the specialization free, and the run's quest
    // done (its 5,000 Emberspark at the Keeper: OnQuestReward). A run started before the quest gets them now.
    void FinishRun(Player* player)
    {
        Set(player, SETTING_RUN, 0);
        uint32 now = uint32(GameTime::GetGameTime().count()), started = Get(player, SETTING_RUN_STARTED);
        uint32 took = now > started ? now - started : 0, fastest = Get(player, SETTING_FASTEST);
        if (!fastest || took < fastest)
            Set(player, SETTING_FASTEST, took);
        player->RemoveAurasDueToSpell(SPELL_PRESTIGIOUS);
        bool trial = player->GetQuestStatus(QUEST_TRIAL) == QUEST_STATUS_INCOMPLETE;
        if (trial)
            player->AreaExploredOrEventHappens(QUEST_TRIAL);
        else
            Emberspark::Add(player->GetSession()->GetAccountId(), SPARK_AT_MAX_LEVEL, "prestige max level");

        uint32 rank = Get(player, SETTING_RANK);
        ChatHandler(player->GetSession()).PSendSysMessage("|cffff8000Prestige {}:|r level {} again, in {}. You can "
            "change your specialization again, and prestige once more.{}", rank, MaxLevel(), Duration(took),
            trial ? " Return to the Keeper of the Ember for your reward." : "");
        if (!trial)
            Emberspark::Announce(player, SPARK_AT_MAX_LEVEL, "for reaching the summit again");
        Broadcast(Acore::StringFormat(
            "|cffff8000[Prestige]|r {} has completed Prestige {} in {}!", player->GetName(), rank, Duration(took)));
    }

    void SendAddon(Player* player, std::string const& payload)
    {
        WorldPacket data;
        std::string const message = std::string(PREFIX) + '\t' + payload;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, message);
        player->SendDirectMessage(&data);
    }

    // The window's state, then "O" to open it (see Emberforge_Prestige.lua for the fields).
    void OpenWindow(Player* player)
    {
        Emberspark::Query(player, [](Player* online, uint32 balance)
        {
            std::string why = CannotPrestige(online);
            uint32 now = uint32(GameTime::GetGameTime().count()), started = Get(online, SETTING_RUN_STARTED);
            SendAddon(online, Acore::StringFormat("S,{},{},{},{},{},{},{},{},{},{},{},{},{}", Get(online, SETTING_RANK),
                online->GetLevel(), MaxLevel(), balance, OnRun(online) ? 1 : 0,
                OnRun(online) && now > started ? now - started : 0, Get(online, SETTING_FASTEST), why.empty() ? 1 : 0,
                SPARK_ON_PRESTIGE, SPARK_AT_MAX_LEVEL, SOULS_ON_PRESTIGE, XP_BONUS_PCT, why));
            SendAddon(online, "O");
        });
    }

    // "PRESTIGE" from the window: checked again here, beside the Keeper it was opened at.
    void OnPrestigeAsked(Player* player)
    {
        auto itr = openedAt.find(player->GetGUID());
        Creature* keeper = itr != openedAt.end() ? player->GetMap()->GetCreature(itr->second) : nullptr;
        if (!keeper || !player->IsWithinDistInMap(keeper, NPC_RANGE))
        {
            ChatHandler(player->GetSession()).SendSysMessage("Speak to the Keeper of the Ember to prestige.");
            return;
        }
        if (std::string why = CannotPrestige(player); !why.empty())
        {
            ChatHandler(player->GetSession()).SendSysMessage(why);
            return;
        }
        openedAt.erase(itr);
        SendAddon(player, "C");
        DoPrestige(player);
    }
}

namespace Prestige
{
    uint32 GetRank(Player* player)
    {
        return Get(player, SETTING_RANK);
    }

    bool IsOnRun(Player* player)
    {
        return Wildcard::IsHero(player) && OnRun(player);
    }

    uint8 EquipLevel(Item const* item)
    {
        ItemTemplate const* proto = item->GetTemplate();
        if (proto->RequiredLevel || proto->Quality < ITEM_QUALITY_UNCOMMON || proto->Quality == ITEM_QUALITY_HEIRLOOM
            || (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR))
            return proto->RequiredLevel;
        if (proto->ItemLevel > CLASSIC_ITEM_LEVEL)
            return MaxLevel();
        return uint8(proto->ItemLevel > CLASSIC_LEVEL_GAP ? proto->ItemLevel - CLASSIC_LEVEL_GAP : 0);
    }
}

// Talking to the Keeper opens the Prestige window (Emberforge_Prestige) instead of a menu.
class npc_titan_prestige : public CreatureScript
{
public:
    npc_titan_prestige() : CreatureScript("npc_titan_prestige") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        // The run's quest, done: turned in here first (the window after).
        if (player->GetQuestStatus(QUEST_TRIAL) == QUEST_STATUS_COMPLETE)
        {
            player->PrepareQuestMenu(creature->GetGUID());
            player->SendPreparedQuest(creature->GetGUID());
            return true;
        }
        CloseGossipMenuFor(player);
        openedAt[player->GetGUID()] = creature->GetGUID();
        OpenWindow(player);
        return true;
    }

    // Trial of Ember: Prestigious turned in: the run's 5,000 Emberspark.
    bool OnQuestReward(Player* player, Creature* /*creature*/, Quest const* quest, uint32 /*opt*/) override
    {
        if (quest->GetQuestId() != QUEST_TRIAL)
            return false;
        Emberspark::Add(player->GetSession()->GetAccountId(), SPARK_AT_MAX_LEVEL, "prestige trial");
        Emberspark::Announce(player, SPARK_AT_MAX_LEVEL, "for the Trial of Ember");
        return false;
    }
};

// The Prestige Gear Cache: used, it gives back the gear packed into it (PackGear), the very items, into the bags; what
// does not fit stays in it. Empty, it is gone. Gear that can go back on does (Reequip).
class item_titan_prestige_cache : public ItemScript
{
public:
    item_titan_prestige_cache() : ItemScript("item_titan_prestige_cache") { }

    bool OnUse(Player* player, Item* cache, SpellCastTargets const& /*targets*/) override
    {
        uint32 cacheGuid = cache->GetGUID().GetCounter();
        // Item::LoadFromDB's fields first (as CHAR_SEL_MAILITEMS), then the guid and entry.
        QueryResult result = CharacterDatabase.Query("SELECT ii.`creatorGuid`, ii.`giftCreatorGuid`, ii.`count`, "
            "ii.`duration`, ii.`charges`, ii.`flags`, ii.`enchantments`, ii.`randomPropertyId`, ii.`durability`, "
            "ii.`playedTime`, ii.`text`, g.`item_guid`, ii.`itemEntry` FROM `titan_prestige_gear` g "
            "JOIN `item_instance` ii ON ii.`guid` = g.`item_guid` WHERE g.`cache_guid` = {}", cacheGuid);

        ChatHandler handler(player->GetSession());
        uint32 back = 0, left = 0;
        if (result)
        {
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            do
            {
                Field* fields = result->Fetch();
                uint32 itemGuid = fields[11].Get<uint32>(), entry = fields[12].Get<uint32>();
                ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
                if (!proto)
                {
                    ++left;
                    continue;
                }
                Item* item = NewItemOrBag(proto);
                ItemPosCountVec dest;
                if (!item->LoadFromDB(itemGuid, player->GetGUID(), fields, entry)
                    || player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false) != EQUIP_ERR_OK)
                {
                    delete item;
                    ++left;
                    continue;
                }
                player->MoveItemToInventory(dest, item, true);
                trans->Append("DELETE FROM `titan_prestige_gear` WHERE `item_guid` = {}", itemGuid);
                ++back;
            } while (result->NextRow());
            player->SaveInventoryAndGoldToDB(trans);
            CharacterDatabase.CommitTransaction(trans);
        }

        if (left)
            handler.PSendSysMessage("|cffff8000Prestige Gear Cache:|r {} pieces back in your bags; {} still in the "
                "cache: make room in your bags and use it again.", back, left);
        else
        {
            handler.PSendSysMessage("|cffff8000Prestige Gear Cache:|r {} pieces back in your bags.", back);
            player->DestroyItem(cache->GetBagSlot(), cache->GetSlot(), true);
        }
        Reequip(player);
        return true;
    }
};

class titan_prestige : public PlayerScript
{
public:
    titan_prestige() : PlayerScript("titan_prestige", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LEVEL_CHANGED,
        PLAYERHOOK_ON_GIVE_EXP,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT,
        PLAYERHOOK_CAN_EQUIP_ITEM
    }) { }

    // A Hero cannot put on gear above their level (Prestige::EquipLevel: also gear with no required level).
    bool OnPlayerCanEquipItem(Player* player, uint8 /*slot*/, uint16& /*dest*/, Item* item, bool /*swap*/,
        bool notLoading) override
    {
        return !notLoading || !item || !Wildcard::IsHero(player) || Prestige::EquipLevel(item) <= player->GetLevel();
    }

    // Requests from the window: whispers to oneself in the addon language with our prefix, never delivered.
    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 language, std::string& msg,
        Player* receiver) override
    {
        std::string const prefix = std::string(PREFIX) + '\t';
        if (language != LANG_ADDON || receiver != player || msg.compare(0, prefix.size(), prefix) != 0)
            return true;

        if (std::string_view(msg).substr(prefix.size()) == "PRESTIGE")
            OnPrestigeAsked(player);
        return false;
    }

    void OnPlayerLogout(Player* player) override
    {
        openedAt.erase(player->GetGUID());
    }

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
        // Gear worn above the Hero's level (worn before EquipLevel: e.g. prestiged in gear with no required level)
        if (uint32 stashed = StashGear(player, player->GetLevel()))
            ChatHandler(player->GetSession()).PSendSysMessage("|cffff8000Emberforge:|r {} of your pieces are above "
                "your level. They wait in your bags and go back on by themselves once you reach their level.", stashed);
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
    new item_titan_prestige_cache();
}
