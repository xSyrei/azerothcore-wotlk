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
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Trainer.h"
#include "World.h"
#include <algorithm>
#include <array>
#include <unordered_map>
#include <unordered_set>

// Wildcard: how the classless Hero progresses.
//
// - Starting kit (levels 1-8): KIT_SIZE random abilities. The Tome of Randomization rerolls them and
//   the Tome of Selection clears them for KIT_SIZE picks in the Classless window (item_titan_tomes.cpp).
//   At LOCK_LEVEL the kit locks: unspent picks are filled at random and both tomes vanish.
// - From FIRST_TALENT_LEVEL: one random ability and one random talent rank per level. Abilities
//   come from every class trainer's list (all 10 classes); talents from any class's trees (tier and
//   prerequisite rules do not apply). The talent UI cannot spend points on a Hero.
// - Owned abilities upgrade to the best rank for the Hero's level by themselves.
//
// Grants are caught up at login and on every level change, so a multi-level jump or a character
// that predates Wildcard gets everything it is owed. The number of abilities granted and the picks
// left are kept in the character's settings (EnablePlayerSettings = 1); talents are counted from the
// talents known. Spells of other classes' skill lines survive a relog only with
// ValidateSkillLearnedBySpells = 0.
namespace
{
    using namespace Wildcard;

    constexpr char const* SETTING_SOURCE = "mod-titan-wildcard";
    constexpr uint32 SETTING_ABILITIES_GRANTED = 0;
    constexpr uint32 SETTING_PICKS_LEFT = 1;

    // Never offered: hunter pet handling (a Hero has no hunter pet to tame, call or heal).
    constexpr std::array<uint32, 8> EXCLUDED_SPELLS = { 1515, 883, 2641, 982, 6991, 136, 1002, 62757 };

    // The spells a trainer entry teaches: itself, or what it learns when it is a wrapper.
    std::vector<uint32> TaughtSpells(uint32 trainerSpellId)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(trainerSpellId);
        if (!spellInfo)
            return { };

        if (!spellInfo->HasEffect(SPELL_EFFECT_LEARN_SPELL))
            return { trainerSpellId };

        std::vector<uint32> taught;
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
            if (effect.IsEffect(SPELL_EFFECT_LEARN_SPELL) && effect.TriggerSpell)
                taught.push_back(effect.TriggerSpell);

        return taught;
    }

    // The class skill line (spec) a spell belongs to, 0 if none.
    uint32 ClassSkillLine(uint32 spellId)
    {
        SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
            if (SkillLineEntry const* skill = sSkillLineStore.LookupEntry(itr->second->SkillLine))
                if (skill->categoryId == SKILL_CATEGORY_CLASS)
                    return skill->id;

        return 0;
    }

    // Spells limited to some races (faction versions such as Heroism / Bloodlust) by their skill line.
    bool FitsRace(Player const* player, uint32 spellId)
    {
        SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
        if (bounds.first == bounds.second)
            return true;

        for (auto itr = bounds.first; itr != bounds.second; ++itr)
            if (!itr->second->RaceMask || (itr->second->RaceMask & player->getRaceMask()))
                return true;

        return false;
    }

    // Index of the best rank available at the player's level, or -1.
    int32 BestRank(Player const* player, Ability const& ability)
    {
        int32 best = -1;
        for (int32 i = 0; i < int32(ability.Ranks.size()); ++i)
            if (ability.Ranks[i].Level <= player->GetLevel())
                best = i;

        return best;
    }

    void Announce(Player* player, uint32 spellId, char const* what)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            return;

        char const* name = spellInfo->SpellName[player->GetSession()->GetSessionDbcLocale()];
        ChatHandler(player->GetSession()).PSendSysMessage("|cffff8000Wildcard:|r {} |cff71d5ff|Hspell:{}|h[{}]|h|r",
            what, spellId, name ? name : "?");
    }

    void UpgradeRanks(Player* player)
    {
        for (Ability const& ability : Abilities())
        {
            int32 known = KnownRank(player, ability);
            int32 best = BestRank(player, ability);
            if (known >= 0 && best > known)
                player->learnSpell(ability.Ranks[best].SpellId);
        }
    }

    uint32 SpentTalentPoints(Player const* player)
    {
        uint32 spent = 0;
        for (auto const& [spellId, talent] : player->GetTalentMap())
            if (talent->State != PLAYERSPELL_REMOVED && talent->IsInSpec(player->GetActiveSpec()))
                if (TalentSpellPos const* talentPos = GetTalentSpellPos(spellId))
                    spent += talentPos->rank + 1;

        return spent;
    }

    // Learns the next rank of a random talent from any class tree, the way Player::LearnTalent does
    // once its checks pass.
    bool GrantTalent(Player* player)
    {
        std::vector<std::pair<TalentEntry const*, uint8>> candidates;
        for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
        {
            TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
            if (!talentInfo)
                continue;

            TalentTabEntry const* tabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
            if (!tabInfo || tabInfo->petTalentMask || !(tabInfo->ClassMask & CLASSMASK_ALL_PLAYABLE))
                continue;

            uint8 currentRank = 0;
            for (uint8 rank = 0; rank < MAX_TALENT_RANK; ++rank)
                if (talentInfo->RankID[rank] && player->HasTalent(talentInfo->RankID[rank], player->GetActiveSpec()))
                    currentRank = rank + 1;

            if (currentRank < MAX_TALENT_RANK && talentInfo->RankID[currentRank])
                candidates.emplace_back(talentInfo, currentRank);
        }

        if (candidates.empty())
            return false;

        auto [talentInfo, currentRank] = candidates[urand(0, uint32(candidates.size() - 1))];
        uint32 spellId = talentInfo->RankID[currentRank];
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            return false;

        bool learned = false;
        if (talentInfo->addToSpellBook && !spellInfo->HasAttribute(SPELL_ATTR0_PASSIVE) && !spellInfo->HasEffect(SPELL_EFFECT_LEARN_SPELL))
        {
            player->learnSpell(spellId);
            learned = true;
        }

        if (!learned)
            player->SendLearnPacket(spellId, true);

        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
            if (effect.IsEffect(SPELL_EFFECT_LEARN_SPELL) && sSpellMgr->IsAdditionalTalentSpell(effect.TriggerSpell))
                player->learnSpell(effect.TriggerSpell);

        player->addTalent(spellId, player->GetActiveSpecMask(), currentRank);
        Announce(player, spellId, currentRank ? "talent rank up" : "new talent");
        return true;
    }

    void DestroyTomes(Player* player)
    {
        for (uint32 entry : { TOME_OF_RANDOMIZATION, TOME_OF_SELECTION })
            if (uint32 count = player->GetItemCount(entry, true))
                player->DestroyItemCount(entry, count, true);
    }

    void CatchUp(Player* player, bool grantAbilities)
    {
        uint8 level = player->GetLevel();

        // The kit locks: picks never spent are rolled at random, and the tomes are gone.
        if (IsKitLocked(player))
        {
            for (uint32 picks = GetPicks(player); picks; --picks)
                if (!GrantRandom(player, "kit ability"))
                    break;

            SetPicks(player, 0);
            DestroyTomes(player);
        }

        if (grantAbilities)
        {
            uint32 owed = KIT_SIZE + (level >= FIRST_TALENT_LEVEL ? level - FIRST_TALENT_LEVEL + 1 : 0);
            uint32 granted = player->GetPlayerSetting(SETTING_SOURCE, SETTING_ABILITIES_GRANTED).value;
            uint32 const before = granted;
            while (granted < owed && GrantRandom(player, granted < KIT_SIZE ? "kit ability" : "new ability"))
                ++granted;

            if (granted != before)
                player->UpdatePlayerSetting(SETTING_SOURCE, SETTING_ABILITIES_GRANTED, granted);
        }

        UpgradeRanks(player);

        uint32 owedTalents = level >= FIRST_TALENT_LEVEL ? level - FIRST_TALENT_LEVEL + 1 : 0;
        bool talentsChanged = false;
        for (uint32 spent = SpentTalentPoints(player); spent < owedTalents && GrantTalent(player); ++spent)
            talentsChanged = true;

        if (talentsChanged)
            player->InitTalentForLevel();

        Classless::SendKnown(player);
        Classless::SendState(player);
    }
}

namespace Wildcard
{
    bool IsHero(Player const* player)
    {
        return player->getClass() == HERO_CLASS;
    }

    // Every class trainer spell, grouped into rank chains with the level each rank needs. Built on
    // first use, after the world database is loaded.
    std::vector<Ability> const& Abilities()
    {
        static std::vector<Ability> const abilities = []
        {
            std::unordered_map<uint32, uint8> trainerLevel;
            std::unordered_map<uint32, uint8> trainerClass;
            for (uint8 classId = CLASS_WARRIOR; classId < MAX_CLASSES; ++classId)
            {
                if (!sChrClassesStore.LookupEntry(classId))
                    continue;

                for (Trainer::Trainer const* trainer : sObjectMgr->GetClassTrainers(classId))
                    for (Trainer::Spell const& trainerSpell : trainer->GetSpells())
                        for (uint32 spellId : TaughtSpells(trainerSpell.SpellId))
                        {
                            auto [itr, isNew] = trainerLevel.emplace(spellId, trainerSpell.ReqLevel);
                            if (!isNew)
                                itr->second = std::min(itr->second, trainerSpell.ReqLevel);
                            trainerClass.emplace(spellId, classId);
                        }
            }

            std::unordered_set<uint32> firstRanks;
            for (auto const& [spellId, level] : trainerLevel)
                firstRanks.insert(sSpellMgr->GetFirstSpellInChain(spellId));

            std::vector<Ability> result;
            for (uint32 firstRank : firstRanks)
            {
                Ability ability { };
                for (uint32 rank = firstRank; rank; rank = sSpellMgr->GetNextSpellInChain(rank))
                {
                    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(rank);
                    if (!spellInfo)
                        break;

                    // Ranks no trainer teaches (a class's starting rank 1) use the spell's own level.
                    auto itr = trainerLevel.find(rank);
                    uint8 level = itr != trainerLevel.end() ? itr->second : uint8(std::max<uint32>(1, spellInfo->SpellLevel));
                    ability.Ranks.push_back({ rank, level });

                    if (!ability.ClassId)
                        if (auto classItr = trainerClass.find(rank); classItr != trainerClass.end())
                            ability.ClassId = classItr->second;
                }

                if (ability.Ranks.empty())
                    continue;

                SpellInfo const* firstInfo = sSpellMgr->GetSpellInfo(firstRank);
                ability.SkillLine = ClassSkillLine(firstRank);
                ability.Offered = !GetTalentSpellCost(firstRank) && !firstInfo->IsPassive()
                    && std::find(EXCLUDED_SPELLS.begin(), EXCLUDED_SPELLS.end(), firstRank) == EXCLUDED_SPELLS.end();
                result.push_back(std::move(ability));
            }

            LOG_INFO("module", "mod-titan: Wildcard pool has {} abilities ({} rank chains from class trainers)",
                std::count_if(result.begin(), result.end(), [](Ability const& a) { return a.Offered; }), result.size());
            return result;
        }();

        return abilities;
    }

    Ability const* FindAbility(uint32 firstRankSpellId)
    {
        for (Ability const& ability : Abilities())
            if (ability.Ranks.front().SpellId == firstRankSpellId)
                return &ability;

        return nullptr;
    }

    // Index of the highest rank the player knows, or -1.
    int32 KnownRank(Player const* player, Ability const& ability)
    {
        for (int32 i = int32(ability.Ranks.size()) - 1; i >= 0; --i)
            if (player->HasSpell(ability.Ranks[i].SpellId))
                return i;

        return -1;
    }

    bool CanLearn(Player const* player, Ability const& ability)
    {
        return ability.Offered && BestRank(player, ability) >= 0 && KnownRank(player, ability) < 0
            && FitsRace(player, ability.Ranks.front().SpellId);
    }

    void Learn(Player* player, Ability const& ability, char const* what)
    {
        uint32 spellId = ability.Ranks[BestRank(player, ability)].SpellId;
        player->learnSpell(spellId);
        Announce(player, spellId, what);
    }

    bool GrantRandom(Player* player, char const* what)
    {
        std::vector<Ability const*> candidates;
        for (Ability const& ability : Abilities())
            if (CanLearn(player, ability))
                candidates.push_back(&ability);

        if (candidates.empty())
            return false;

        Learn(player, *candidates[urand(0, uint32(candidates.size() - 1))], what);
        return true;
    }

    uint32 ClearAbilities(Player* player)
    {
        uint32 cleared = 0;
        for (Ability const& ability : Abilities())
        {
            if (!ability.Offered || KnownRank(player, ability) < 0)
                continue;

            for (Rank const& rank : ability.Ranks)
                if (player->HasSpell(rank.SpellId))
                    player->removeSpell(rank.SpellId, SPEC_MASK_ALL, false);

            ++cleared;
        }

        return cleared;
    }

    uint32 GetPicks(Player* player)
    {
        return player->GetPlayerSetting(SETTING_SOURCE, SETTING_PICKS_LEFT).value;
    }

    void SetPicks(Player* player, uint32 picks)
    {
        player->UpdatePlayerSetting(SETTING_SOURCE, SETTING_PICKS_LEFT, picks);
    }

    bool IsKitLocked(Player const* player)
    {
        return player->GetLevel() >= LOCK_LEVEL;
    }
}

class titan_wildcard : public PlayerScript
{
public:
    titan_wildcard() : PlayerScript("titan_wildcard", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LEVEL_CHANGED,
        PLAYERHOOK_ON_CALCULATE_TALENTS_POINTS,
        PLAYERHOOK_CAN_LEARN_TALENT
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!IsHero(player))
            return;

        // Without saved settings the ability count resets every login and would be granted again.
        bool settingsSaved = sWorld->getBoolConfig(CONFIG_PLAYER_SETTINGS_ENABLED);
        static bool warned = false;
        if (!warned)
        {
            warned = true;
            if (!settingsSaved)
                LOG_ERROR("module", "mod-titan: Wildcard needs EnablePlayerSettings = 1 in worldserver.conf; abilities are only granted on level up until then");
            if (sWorld->getBoolConfig(CONFIG_VALIDATE_SKILL_LEARNED_BY_SPELLS))
                LOG_ERROR("module", "mod-titan: Wildcard needs ValidateSkillLearnedBySpells = 0 in worldserver.conf; other classes' spells are deleted at login until then");
        }

        CatchUp(player, settingsSaved);
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        if (IsHero(player))
            CatchUp(player, true);
    }

    // A Hero's talents all come from Wildcard: as many points as are spent, none left to spend,
    // and the "more talents than allowed" reset never triggers.
    void OnPlayerCalculateTalentsPoints(Player const* player, uint32& talentPointsForLevel) override
    {
        if (IsHero(player))
            talentPointsForLevel = SpentTalentPoints(player);
    }

    bool OnPlayerCanLearnTalent(Player* player, TalentEntry const* /*talent*/, uint32 /*rank*/) override
    {
        return !IsHero(player);
    }
};

void AddSC_wildcard()
{
    new titan_wildcard();
}
