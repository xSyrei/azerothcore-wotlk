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

#include "cards.h"
#include "emberspark.h"
#include "wildcard.h"
#include "Chat.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Item.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Tokenize.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <array>
#include <map>
#include <string_view>
#include <unordered_map>
#include <vector>

// Skill and Talent Cards: every Wildcard ability is a Skill Card, every talent a Talent Card (one copy a rank), each of
// a rarity. A card, once opened, is the account's forever (auth database: titan_cards), for every character on it;
// slotting cards into Specializations (loadouts the level-up rolls follow) comes next.
//
// - Packs (items in the bags): a Skill Pack or Talent Pack holds 3 cards, a Golden Pack 3 Golden Cards. Each card's
//   rarity is rolled (Common 70%, Rare 24%, Epic 5%, Legendary 1%), then a card of it, 3 times in 4 one suiting the
//   Hero's role. A card already owned (every rank of a talent) is a duplicate: Emberspark back by its rarity, and the
//   account's duplicate tally goes up. Every 10th duplicate gives a free pack of the same kind, every 50th a Golden
//   Pack and 2,000 Emberspark.
// - Silas Darkmoon (npc_titan_silas) opens the Darkmoon shop (the Emberforge_Darkmoon addon): Skill and Talent Packs
//   and any card (but Golden ones) for Emberspark, at prices that climb with each purchase and start over at a
//   prestige; Skill Packs for gold too, 5 a week an account; Golden Packs.
//
// Addon whispers with prefix EFDM (protocol in Emberforge_Darkmoon.lua).
namespace
{
    constexpr char const* PREFIX = "EFDM";
    constexpr std::size_t MAX_PAYLOAD = 230;
    constexpr char const* SETTING_SOURCE = "mod-titan-darkmoon";
    constexpr uint32 SETTING_SKILL_PACKS = 0;   // bought this prestige, for the price
    constexpr uint32 SETTING_TALENT_PACKS = 1;
    constexpr uint32 SETTING_CARDS = 2;
    constexpr uint32 SETTING_SOURCE_NONE = UINT32_MAX;     // a fixed price (the Golden Pack), nothing counted

    constexpr uint32 ITEM_SKILL_PACK = 910010;
    constexpr uint32 ITEM_TALENT_PACK = 910011;
    constexpr uint32 ITEM_GOLDEN_PACK = 910012;
    constexpr uint32 NPC_SILAS = 910110;
    constexpr float NPC_RANGE = 15.0f;

    enum Kind : uint8
    {
        KIND_SKILL,
        KIND_TALENT,
        KIND_GOLDEN_SKILL,
        KIND_GOLDEN_TALENT,
    };

    enum Pack : uint8
    {
        PACK_SKILL,
        PACK_TALENT,
        PACK_GOLDEN,
    };

    enum Rarity : uint8
    {
        RARITY_COMMON,
        RARITY_RARE,
        RARITY_EPIC,
        RARITY_LEGENDARY,
        MAX_RARITIES
    };

    constexpr uint32 CARDS_PER_PACK = 3;
    constexpr std::array<uint32, MAX_RARITIES> RARITY_CHANCE = { 70, 24, 5, 1 };
    constexpr std::array<uint32, MAX_RARITIES> DUPLICATE_REFUND = { 40, 120, 400, 1200 };
    constexpr uint32 GOLDEN_DUPLICATE_REFUND = 2000;
    constexpr std::array<uint32, MAX_RARITIES> CARD_PRICE_FACTOR = { 1, 2, 4, 8 };    // times the climbing price
    constexpr uint32 FREE_PACK_EVERY = 10, GOLDEN_PACK_EVERY = 50, GOLDEN_PACK_BONUS = 2000;
    constexpr uint32 GOLDEN_PACK_PRICE = 10000;
    constexpr uint32 GOLD_PACK_PRICE = 75 * GOLD, GOLD_PACKS_A_WEEK = 5;
    constexpr uint32 SUITING_CHANCE = 75;      // % of cards rolled from those suiting the Hero's role
    // The climbing price: the nth purchase this prestige costs PRICES[n], then 250 more each.
    constexpr std::array<uint32, 21> PRICES = { 100, 125, 150, 175, 200, 250, 300, 350, 400, 450, 500, 600, 700, 800,
        900, 1000, 1200, 1400, 1600, 1800, 2000 };

    uint32 Price(uint32 bought)
    {
        return bought < PRICES.size() ? PRICES[bought] : PRICES.back() + 250 * (bought - uint32(PRICES.size()) + 1);
    }

    struct Card
    {
        uint8 Kind;         // KIND_SKILL or KIND_TALENT
        uint32 Id;          // first rank, or talent id
        uint32 SpellId;     // what the client shows: the first rank
        uint8 Rarity;
        uint8 MaxCount;     // 1, or the talent's ranks
        uint8 Roles;        // Wildcard RoleMask bits; 0: every role
    };

    struct Catalog
    {
        std::vector<Card> cards;
        std::array<std::array<std::vector<uint32>, MAX_RARITIES>, 2> byRarity;   // [kind][rarity] -> card indexes
        std::map<std::pair<uint8, uint32>, uint32> byId;
    };

    Rarity SkillRarity(Wildcard::Ability const& ability)
    {
        if (ability.Capstone)
            return RARITY_LEGENDARY;
        if (ability.TalentId)
            return RARITY_EPIC;
        return ability.Ranks.front().Level >= 30 ? RARITY_RARE : RARITY_COMMON;
    }

    Rarity TalentRarity(TalentEntry const* talent)
    {
        return talent->Row >= 9 ? RARITY_LEGENDARY : talent->Row >= 6 ? RARITY_EPIC : talent->Row >= 3 ? RARITY_RARE
            : RARITY_COMMON;
    }

    // Every offered ability (a Skill Card) and every talent of the Hero's trees not learned as an ability (a Talent
    // Card), built once.
    Catalog const& GetCatalog()
    {
        static Catalog const catalog = []
        {
            Catalog built;
            auto add = [&built](Card card)
            {
                built.byId[{ card.Kind, card.Id }] = uint32(built.cards.size());
                built.byRarity[card.Kind][card.Rarity].push_back(uint32(built.cards.size()));
                built.cards.push_back(card);
            };
            for (Wildcard::Ability const& ability : Wildcard::Abilities())
                if (ability.Offered && !ability.Banned && !ability.Companion && !ability.Ranks.empty())
                    add({ KIND_SKILL, ability.Ranks.front().SpellId, ability.Ranks.front().SpellId,
                        SkillRarity(ability), 1, ability.Roles });
            for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
            {
                TalentEntry const* talent = sTalentStore.LookupEntry(i);
                if (!talent || !talent->RankID[0] || Wildcard::TalentAbility(talent->TalentID))
                    continue;
                TalentTabEntry const* tab = sTalentTabStore.LookupEntry(talent->TalentTab);
                if (!tab || tab->petTalentMask || !(tab->ClassMask & Wildcard::HERO_TALENT_CLASSMASK))
                    continue;
                uint8 ranks = 0;
                for (uint32 rank : talent->RankID)
                    if (rank)
                        ++ranks;
                add({ KIND_TALENT, talent->TalentID, talent->RankID[0], TalentRarity(talent), ranks, 0 });
            }
            return built;
        }();
        return catalog;
    }

    // An account's collection, loaded at login (two queries: cards and tallies).
    struct Collection
    {
        uint8 pending = 2;
        std::map<std::pair<uint8, uint32>, uint8> owned;     // (kind, card id) -> count
        uint32 duplicates = 0, goldWeek = 0, goldBought = 0;
    };

    std::unordered_map<uint32, Collection> collections;
    std::unordered_map<ObjectGuid, ObjectGuid> openedAt;      // the Silas each player last opened the shop at
    std::unordered_map<ObjectGuid, bool> busy;                 // a purchase waiting on its Emberspark

    Collection* Loaded(Player* player)
    {
        auto itr = collections.find(player->GetSession()->GetAccountId());
        return itr != collections.end() && !itr->second.pending ? &itr->second : nullptr;
    }

    uint8 Owned(Collection const& collection, uint8 kind, uint32 id)
    {
        auto itr = collection.owned.find({ kind, id });
        return itr != collection.owned.end() ? itr->second : 0;
    }

    void SaveCard(uint32 accountId, uint8 kind, uint32 id, uint8 count)
    {
        LoginDatabase.Execute(Acore::StringFormat("INSERT INTO `titan_cards` (`account_id`, `kind`, `card_id`, "
            "`count`) VALUES ({}, {}, {}, {}) ON DUPLICATE KEY UPDATE `count` = {}",
            accountId, kind, id, count, count));
    }

    void SaveTallies(uint32 accountId, Collection const& collection)
    {
        LoginDatabase.Execute(Acore::StringFormat("INSERT INTO `titan_card_tallies` (`account_id`, `duplicates`, "
            "`gold_week`, `gold_bought`) VALUES ({}, {}, {}, {}) ON DUPLICATE KEY UPDATE `duplicates` = {}, "
            "`gold_week` = {}, `gold_bought` = {}", accountId, collection.duplicates, collection.goldWeek,
            collection.goldBought, collection.duplicates, collection.goldWeek, collection.goldBought));
    }

    void Load(Player* player)
    {
        uint32 accountId = player->GetSession()->GetAccountId();
        collections[accountId] = Collection();
        QueryCallbackProcessor& queries = player->GetSession()->GetQueryProcessor();
        queries.AddCallback(LoginDatabase.AsyncQuery(Acore::StringFormat("SELECT `kind`, `card_id`, `count` FROM "
            "`titan_cards` WHERE `account_id` = {}", accountId)).WithCallback([accountId](QueryResult result)
        {
            auto itr = collections.find(accountId);
            if (itr == collections.end())
                return;
            if (result)
                do
                {
                    Field* fields = result->Fetch();
                    itr->second.owned[{ fields[0].Get<uint8>(), fields[1].Get<uint32>() }] = fields[2].Get<uint8>();
                } while (result->NextRow());
            --itr->second.pending;
        }));
        queries.AddCallback(LoginDatabase.AsyncQuery(Acore::StringFormat("SELECT `duplicates`, `gold_week`, "
            "`gold_bought` FROM `titan_card_tallies` WHERE `account_id` = {}", accountId))
            .WithCallback([accountId](QueryResult result)
        {
            auto itr = collections.find(accountId);
            if (itr == collections.end())
                return;
            if (result)
            {
                Field* fields = result->Fetch();
                itr->second.duplicates = fields[0].Get<uint32>();
                itr->second.goldWeek = fields[1].Get<uint32>();
                itr->second.goldBought = fields[2].Get<uint8>();
            }
            --itr->second.pending;
        }));
    }

    uint32 Week()
    {
        return uint32(GameTime::GetGameTime().count() / WEEK);
    }

    uint32 Setting(Player* player, uint32 index)
    {
        return player->GetPlayerSetting(SETTING_SOURCE, index).value;
    }

    void SendAddon(Player* player, std::string const& payload)
    {
        WorldPacket data;
        std::string const message = std::string(PREFIX) + '\t' + payload;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, message);
        player->SendDirectMessage(&data);
    }

    void SendBatched(Player* player, std::string const& head, std::vector<std::string> const& items)
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
                payload += ';';
            payload += item;
        }
        if (payload.size() > head.size())
            SendAddon(player, payload);
    }

    // The whole catalog with what the account owns: K0, K,<kind>.<id>.<spell>.<rarity>.<owned>.<max>.<golden>;... KE
    void SendCatalog(Player* player, Collection const& collection)
    {
        std::vector<std::string> items;
        for (Card const& card : GetCatalog().cards)
            items.push_back(Acore::StringFormat("{}.{}.{}.{}.{}.{}.{}", card.Kind, card.Id, card.SpellId, card.Rarity,
                Owned(collection, card.Kind, card.Id), card.MaxCount, Owned(collection, card.Kind + 2, card.Id)));
        SendAddon(player, "K0");
        SendBatched(player, "K,", items);
        SendAddon(player, "KE");
    }

    // S,<Emberspark>,<Skill Pack>,<Talent Pack>,<card base price>,<gold packs left>,<gold price>,<Golden Pack>,
    //   <duplicates>,<every N a pack>,<every N a Golden Pack>,<factor common>,<rare>,<epic>,<legendary>
    void SendState(Player* player)
    {
        Emberspark::Query(player, [](Player* online, uint32 balance)
        {
            Collection* collection = Loaded(online);
            uint32 duplicates = collection ? collection->duplicates : 0;
            uint32 goldLeft = !collection ? 0 : collection->goldWeek != Week() ? GOLD_PACKS_A_WEEK
                : GOLD_PACKS_A_WEEK - std::min(GOLD_PACKS_A_WEEK, collection->goldBought);
            SendAddon(online, Acore::StringFormat("S,{},{},{},{},{},{},{},{},{},{},{},{},{},{}", balance,
                Price(Setting(online, SETTING_SKILL_PACKS)), Price(Setting(online, SETTING_TALENT_PACKS)),
                Price(Setting(online, SETTING_CARDS)), goldLeft, GOLD_PACK_PRICE, GOLDEN_PACK_PRICE, duplicates,
                FREE_PACK_EVERY, GOLDEN_PACK_EVERY, CARD_PRICE_FACTOR[0], CARD_PRICE_FACTOR[1], CARD_PRICE_FACTOR[2],
                CARD_PRICE_FACTOR[3]));
        });
    }

    // Items into the bags; what does not fit comes by mail.
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

    uint8 RollRarity()
    {
        uint32 roll = urand(1, 100);
        for (uint8 rarity = RARITY_LEGENDARY; rarity > RARITY_COMMON; --rarity)
        {
            uint32 above = 0;
            for (uint8 r = rarity; r < MAX_RARITIES; ++r)
                above += RARITY_CHANCE[r];
            if (roll <= above)
                return rarity;
        }
        return RARITY_COMMON;
    }

    // A card of the kind and a rolled rarity (the next rarity down when none), mostly one suiting the Hero's role;
    // for a Golden Pack one the account has no Golden copy of when there is one.
    Card const* RollCard(Player* player, Collection const& collection, uint8 kind, bool golden)
    {
        Catalog const& catalog = GetCatalog();
        for (int32 rarity = RollRarity(); rarity >= RARITY_COMMON; --rarity)
        {
            std::vector<uint32> const& all = catalog.byRarity[kind][rarity];
            if (all.empty())
                continue;
            std::vector<uint32> suiting, fresh;
            for (uint32 index : all)
            {
                Card const& card = catalog.cards[index];
                if (Wildcard::SuitsRole(player, card.Roles))
                    suiting.push_back(index);
                if (Owned(collection, card.Kind + 2, card.Id) == 0)
                    fresh.push_back(index);
            }
            std::vector<uint32> const& from = golden && !fresh.empty() ? fresh
                : !suiting.empty() && roll_chance_i(SUITING_CHANCE) ? suiting : all;
            return &catalog.cards[from[urand(0, uint32(from.size() - 1))]];
        }
        return nullptr;
    }

    // Adds a card to the account (one more rank of a talent); false when it was a duplicate. Duplicates pay back and
    // count toward the free packs.
    bool AddCard(Player* player, Collection& collection, Card const& card, bool golden, uint32& refund)
    {
        uint32 accountId = player->GetSession()->GetAccountId();
        uint8 kind = card.Kind + (golden ? 2 : 0);
        uint8& count = collection.owned[{ kind, card.Id }];
        refund = 0;
        if (count < card.MaxCount)
        {
            SaveCard(accountId, kind, card.Id, ++count);
            return true;
        }

        refund = golden ? GOLDEN_DUPLICATE_REFUND : DUPLICATE_REFUND[card.Rarity];
        Emberspark::Add(accountId, refund, "card duplicate");
        ++collection.duplicates;
        ChatHandler handler(player->GetSession());
        if (collection.duplicates % FREE_PACK_EVERY == 0)
        {
            GiveItems(player, card.Kind == KIND_SKILL ? ITEM_SKILL_PACK : ITEM_TALENT_PACK, 1);
            handler.PSendSysMessage("|cffa335eeDarkmoon:|r {} duplicates: a free {} Pack!", collection.duplicates,
                card.Kind == KIND_SKILL ? "Skill" : "Talent");
        }
        if (collection.duplicates % GOLDEN_PACK_EVERY == 0)
        {
            GiveItems(player, ITEM_GOLDEN_PACK, 1);
            Emberspark::Add(accountId, GOLDEN_PACK_BONUS, "card duplicates bonus");
            handler.PSendSysMessage("|cffa335eeDarkmoon:|r {} duplicates: a Golden Pack and {} Emberspark!",
                collection.duplicates, Emberspark::Format(GOLDEN_PACK_BONUS));
        }
        SaveTallies(accountId, collection);
        return false;
    }

    // Opens a pack: its cards to the account, R,<pack>,<kind>.<id>.<spell>.<rarity>.<new 0|1>.<refund>;... to the
    // window, which plays the reveal, then U,<kind>.<id>.<owned>.<golden>;... and the state.
    bool OpenPack(Player* player, uint8 pack)
    {
        Collection* collection = Loaded(player);
        if (!collection)
            return false;

        std::vector<std::string> shown, updates;
        uint32 refunds = 0;
        for (uint32 i = 0; i < CARDS_PER_PACK; ++i)
        {
            uint8 kind = pack == PACK_GOLDEN ? (urand(0, 1) ? KIND_SKILL : KIND_TALENT) : pack == PACK_SKILL
                ? KIND_SKILL : KIND_TALENT;
            Card const* card = RollCard(player, *collection, kind, pack == PACK_GOLDEN);
            if (!card)
                continue;
            uint32 refund = 0;
            bool fresh = AddCard(player, *collection, *card, pack == PACK_GOLDEN, refund);
            refunds += refund;
            shown.push_back(Acore::StringFormat("{}.{}.{}.{}.{}.{}", card->Kind, card->Id, card->SpellId,
                card->Rarity, fresh ? 1 : 0, refund));
            updates.push_back(Acore::StringFormat("{}.{}.{}.{}", card->Kind, card->Id,
                Owned(*collection, card->Kind, card->Id), Owned(*collection, card->Kind + 2, card->Id)));
        }
        SendBatched(player, Acore::StringFormat("R,{},", pack), shown);
        SendBatched(player, "U,", updates);
        if (refunds)
            Emberspark::Announce(player, refunds, "for duplicates");
        SendState(player);
        return true;
    }

    bool BesideSilas(Player* player)
    {
        auto itr = openedAt.find(player->GetGUID());
        Creature* silas = itr != openedAt.end() ? player->GetMap()->GetCreature(itr->second) : nullptr;
        if (silas && player->IsWithinDistInMap(silas, NPC_RANGE))
            return true;
        ChatHandler(player->GetSession()).SendSysMessage("Speak to Silas Darkmoon to shop.");
        return false;
    }

    // Pays the climbing price in Emberspark (one purchase at a time), then gives.
    void BuyWithEmberspark(Player* player, uint32 setting, uint32 factor, char const* reason,
        std::function<void(Player*)> give)
    {
        if (busy[player->GetGUID()])
            return;
        busy[player->GetGUID()] = true;
        uint32 price = (setting == SETTING_SOURCE_NONE ? GOLDEN_PACK_PRICE : Price(Setting(player, setting))) * factor;
        Emberspark::Spend(player, price, reason, [setting, price, give = std::move(give)](Player* online, bool paid)
        {
            busy[online->GetGUID()] = false;
            if (!paid)
            {
                ChatHandler(online->GetSession()).PSendSysMessage("You need {} Emberspark for that.",
                    Emberspark::Format(price));
                SendState(online);
                return;
            }
            if (setting != SETTING_SOURCE_NONE)
                online->UpdatePlayerSetting(SETTING_SOURCE, setting, Setting(online, setting) + 1);
            give(online);
            SendState(online);
        });
    }

    void BuyPack(Player* player, uint8 pack, bool withGold)
    {
        uint32 entry = pack == PACK_SKILL ? ITEM_SKILL_PACK : pack == PACK_TALENT ? ITEM_TALENT_PACK : ITEM_GOLDEN_PACK;
        if (withGold)
        {
            Collection* collection = Loaded(player);
            if (!collection || pack != PACK_SKILL)
                return;
            if (collection->goldWeek != Week())
            {
                collection->goldWeek = Week();
                collection->goldBought = 0;
            }
            ChatHandler handler(player->GetSession());
            if (collection->goldBought >= GOLD_PACKS_A_WEEK)
                handler.PSendSysMessage("You have bought your {} Skill Packs with gold this week.", GOLD_PACKS_A_WEEK);
            else if (!player->HasEnoughMoney(GOLD_PACK_PRICE))
                player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, nullptr, 0, 0);
            else
            {
                player->ModifyMoney(-int32(GOLD_PACK_PRICE));
                ++collection->goldBought;
                SaveTallies(player->GetSession()->GetAccountId(), *collection);
                GiveItems(player, entry, 1);
            }
            SendState(player);
            return;
        }

        uint32 setting = pack == PACK_SKILL ? SETTING_SKILL_PACKS : pack == PACK_TALENT ? SETTING_TALENT_PACKS
            : SETTING_SOURCE_NONE;
        BuyWithEmberspark(player, setting, 1, "card pack", [entry](Player* online) { GiveItems(online, entry, 1); });
    }

    void BuyCard(Player* player, uint8 kind, uint32 id)
    {
        Collection* collection = Loaded(player);
        auto itr = GetCatalog().byId.find({ kind, id });
        if (!collection || itr == GetCatalog().byId.end())
            return;
        Card const& card = GetCatalog().cards[itr->second];
        if (Owned(*collection, kind, id) >= card.MaxCount)
        {
            ChatHandler(player->GetSession()).SendSysMessage("You own that card already.");
            return;
        }
        BuyWithEmberspark(player, SETTING_CARDS, CARD_PRICE_FACTOR[card.Rarity], "card", [&card](Player* online)
        {
            if (Collection* owner = Loaded(online))
            {
                uint32 refund = 0;
                AddCard(online, *owner, card, false, refund);
                SendAddon(online, Acore::StringFormat("U,{}.{}.{}.{}", card.Kind, card.Id,
                    Owned(*owner, card.Kind, card.Id), Owned(*owner, card.Kind + 2, card.Id)));
                SendAddon(online, Acore::StringFormat("B,{}.{}.{}.{}", card.Kind, card.Id, card.SpellId, card.Rarity));
            }
        });
    }
}

namespace Cards
{
    void ResetPrices(Player* player)
    {
        for (uint32 setting : { SETTING_SKILL_PACKS, SETTING_TALENT_PACKS, SETTING_CARDS })
            player->UpdatePlayerSetting(SETTING_SOURCE, setting, 0);
    }
}

// The packs: opened from the bags, the reveal plays in the Darkmoon window.
class item_titan_card_pack : public ItemScript
{
public:
    item_titan_card_pack(char const* name, uint8 pack) : ItemScript(name), _pack(pack) { }

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        if (!Loaded(player))
        {
            ChatHandler(player->GetSession()).SendSysMessage("Your card collection is still loading. Try again.");
            player->SendEquipError(EQUIP_ERR_CANT_DO_RIGHT_NOW, item, nullptr);
            return true;
        }
        uint32 one = 1;
        player->DestroyItemCount(item, one, true);
        OpenPack(player, _pack);
        return true;
    }

private:
    uint8 _pack;
};

// Silas Darkmoon opens the Darkmoon shop.
class npc_titan_silas : public CreatureScript
{
public:
    npc_titan_silas() : CreatureScript("npc_titan_silas") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        CloseGossipMenuFor(player);
        openedAt[player->GetGUID()] = creature->GetGUID();
        SendState(player);
        SendAddon(player, "O");
        return true;
    }
};

class titan_cards : public PlayerScript
{
public:
    titan_cards() : PlayerScript("titan_cards", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        Load(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        collections.erase(player->GetSession()->GetAccountId());
        openedAt.erase(player->GetGUID());
        busy.erase(player->GetGUID());
    }

    // Requests from the window: LIST (the catalog), BUY,<pack>,<0 Emberspark|1 gold>, CARD,<kind>,<id>.
    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 language, std::string& msg,
        Player* receiver) override
    {
        std::string const prefix = std::string(PREFIX) + '\t';
        if (language != LANG_ADDON || receiver != player || msg.compare(0, prefix.size(), prefix) != 0)
            return true;

        std::string_view request = std::string_view(msg).substr(prefix.size());
        std::vector<std::string_view> parts = Acore::Tokenize(request, ',', false);
        if (request == "LIST")
        {
            if (Collection* collection = Loaded(player))
                SendCatalog(player, *collection);
        }
        else if (parts.size() == 3 && parts[0] == "BUY" && BesideSilas(player))
        {
            Optional<uint8> pack = Acore::StringTo<uint8>(parts[1]);
            if (pack && *pack <= PACK_GOLDEN)
                BuyPack(player, *pack, parts[2] == "1");
        }
        else if (parts.size() == 3 && parts[0] == "CARD" && BesideSilas(player))
        {
            Optional<uint8> kind = Acore::StringTo<uint8>(parts[1]);
            Optional<uint32> id = Acore::StringTo<uint32>(parts[2]);
            if (kind && id && *kind <= KIND_TALENT)
                BuyCard(player, *kind, *id);
        }
        return false;
    }
};

void AddSC_cards()
{
    new item_titan_card_pack("item_titan_skill_pack", PACK_SKILL);
    new item_titan_card_pack("item_titan_talent_pack", PACK_TALENT);
    new item_titan_card_pack("item_titan_golden_pack", PACK_GOLDEN);
    new npc_titan_silas();
    new titan_cards();
}
