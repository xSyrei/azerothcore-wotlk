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
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "LFGMgr.h"
#include "Log.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include "World.h"
#include "WorldSession.h"
#include <unordered_map>

// The dungeon finder's rewards: finishing a random dungeon (its reward quest, lfg_dungeon_rewards, is rewarded) gives
// 100 Emberspark, 500 for a heroic, and its experience is a quarter of what the Hero's level takes (the dungeon
// finder shows that amount too: LFGHandler.cpp asks the same hook). Every other quest gives QUEST_XP_MULTIPLIER times
// its experience.
namespace
{
    constexpr uint32 DUNGEON_EMBERSPARK = 100;
    constexpr uint32 HEROIC_EMBERSPARK = 500;
    constexpr uint32 EXPERIENCE_SHARE = 4;     // a quarter of the level's experience
    // Every other quest gives this many times its experience on turn-in (shown so in the quest window too), on top
    // of Rate.XP.Quest: the level squish leaves quests short of what a level asks.
    constexpr uint32 QUEST_XP_MULTIPLIER = 3;

    std::unordered_map<uint32, bool> rewardQuests;     // reward quest -> heroic

    void LoadRewardQuests()
    {
        rewardQuests.clear();
        QueryResult result = WorldDatabase.Query("SELECT `dungeonId`, `firstQuestId`, `otherQuestId` FROM "
            "`lfg_dungeon_rewards`");
        if (!result)
            return;
        do
        {
            Field* fields = result->Fetch();
            LFGDungeonEntry const* dungeon = sLFGDungeonStore.LookupEntry(fields[0].Get<uint32>());
            // The holiday bosses keep only their own rewards.
            if (!dungeon || (dungeon->Flags & lfg::LFG_FLAG_SEASONAL))
                continue;
            bool heroic = dungeon->Difficulty > DUNGEON_DIFFICULTY_NORMAL;
            for (uint32 quest : { fields[1].Get<uint32>(), fields[2].Get<uint32>() })
                if (quest)
                    rewardQuests[quest] = heroic;
        } while (result->NextRow());
        LOG_INFO("server.loading", ">> Dungeon finder: {} reward quests give Emberspark and experience",
            rewardQuests.size());
    }
}

class titan_dungeon_rewards_world : public WorldScript
{
public:
    titan_dungeon_rewards_world() : WorldScript("titan_dungeon_rewards_world", { WORLDHOOK_ON_STARTUP }) { }

    void OnStartup() override
    {
        LoadRewardQuests();
    }
};

class titan_dungeon_rewards : public PlayerScript
{
public:
    titan_dungeon_rewards() : PlayerScript("titan_dungeon_rewards", {
        PLAYERHOOK_ON_PLAYER_COMPLETE_QUEST,
        PLAYERHOOK_ON_QUEST_COMPUTE_EXP
    }) { }

    void OnPlayerQuestComputeXP(Player* player, Quest const* quest, uint32& xpValue) override
    {
        if (rewardQuests.count(quest->GetQuestId()))
            xpValue = player->GetUInt32Value(PLAYER_NEXT_LEVEL_XP) / EXPERIENCE_SHARE;
        else
            xpValue *= QUEST_XP_MULTIPLIER;
    }

    void OnPlayerCompleteQuest(Player* player, Quest const* quest) override
    {
        auto itr = rewardQuests.find(quest->GetQuestId());
        if (itr == rewardQuests.end())
            return;
        bool heroic = itr->second;
        uint32 amount = heroic ? HEROIC_EMBERSPARK : DUNGEON_EMBERSPARK;
        Emberspark::Add(player->GetSession()->GetAccountId(), amount, heroic ? "heroic dungeon" : "dungeon");
        Emberspark::Announce(player, amount, heroic ? "for your heroic dungeon" : "for your dungeon");
    }
};

void AddSC_dungeon_rewards()
{
    new titan_dungeon_rewards_world();
    new titan_dungeon_rewards();
}
