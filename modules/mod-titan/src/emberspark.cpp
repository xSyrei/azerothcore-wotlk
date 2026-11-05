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

#include "emberspark.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "StringFormat.h"
#include "WorldSession.h"

// The balance lives in the auth database (data/sql/db-auth/titan_emberspark.sql), one row per account, so the shop
// can read and spend it too. Every change is one atomic statement, so two realms or the shop can never lose one.
// The values are numbers and constant reasons only: nothing a player typed reaches these queries.
namespace Emberspark
{
    void Add(uint32 accountId, uint32 amount, char const* reason)
    {
        if (!amount)
            return;

        LoginDatabaseTransaction trans = LoginDatabase.BeginTransaction();
        trans->Append(Acore::StringFormat("INSERT INTO `titan_emberspark` (`account_id`, `balance`) VALUES ({}, {}) "
            "ON DUPLICATE KEY UPDATE `balance` = `balance` + {}", accountId, amount, amount));
        trans->Append(Acore::StringFormat("INSERT INTO `titan_emberspark_log` (`account_id`, `amount`, `reason`) "
            "VALUES ({}, {}, '{}')", accountId, amount, reason));
        LoginDatabase.CommitTransaction(trans);
    }

    void Spend(Player* player, uint32 amount, char const* reason, std::function<void(Player*, bool)> callback)
    {
        uint32 accountId = player->GetSession()->GetAccountId();
        Query(player, [accountId, amount, reason, callback = std::move(callback)](Player* online, uint32 balance)
        {
            if (balance < amount)
            {
                callback(online, false);
                return;
            }
            LoginDatabaseTransaction trans = LoginDatabase.BeginTransaction();
            trans->Append(Acore::StringFormat("UPDATE `titan_emberspark` SET `balance` = `balance` - {} "
                "WHERE `account_id` = {} AND `balance` >= {}", amount, accountId, amount));
            trans->Append(Acore::StringFormat("INSERT INTO `titan_emberspark_log` (`account_id`, `amount`, `reason`) "
                "VALUES ({}, -{}, '{}')", accountId, amount, reason));
            LoginDatabase.CommitTransaction(trans);
            callback(online, true);
        });
    }

    void Query(Player* player, std::function<void(Player*, uint32)> callback)
    {
        ObjectGuid guid = player->GetGUID();
        player->GetSession()->GetQueryProcessor().AddCallback(LoginDatabase.AsyncQuery(Acore::StringFormat(
            "SELECT `balance` FROM `titan_emberspark` WHERE `account_id` = {}", player->GetSession()->GetAccountId()))
            .WithCallback([guid, callback = std::move(callback)](QueryResult result)
            {
                if (Player* online = ObjectAccessor::FindConnectedPlayer(guid))
                    callback(online, result ? result->Fetch()[0].Get<uint32>() : 0);
            }));
    }

    void Announce(Player* player, uint32 amount, char const* why)
    {
        ChatHandler(player->GetSession()).PSendSysMessage("|cffff8000Emberspark:|r +{} {}.", Format(amount), why);
    }

    std::string Format(uint32 amount)
    {
        std::string digits = std::to_string(amount);
        for (int32 i = int32(digits.size()) - 3; i > 0; i -= 3)
            digits.insert(std::size_t(i), ",");
        return digits;
    }
}
