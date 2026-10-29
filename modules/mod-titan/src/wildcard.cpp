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
#include <string_view>
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

    // Ability lists match spell names (English): every rank and faction version of a name counts. A
    // name that matches nothing is logged at startup.

    // Removed from the game for Heroes: never granted or listed, and unlearned at login from anyone
    // who has them (with a random replacement). Everything in the Runeforging skill line goes too.
    constexpr char const* BANNED_ABILITIES[] = {
        // Class mounts.
        "Summon Charger", "Charger", "Summon Warhorse", "Warhorse", "Summon Felsteed", "Felsteed",
        "Summon Dreadsteed", "Dreadsteed",
        // Tracking and scouting.
        "Track Beasts", "Track Humanoids", "Track Undead", "Track Hidden", "Track Elementals", "Track Demons",
        "Track Dragonkin", "Track Giants", "Sense Undead", "Sense Demons", "Eagle Eye", "Beast Lore",
        "Mind Vision", "Far Sight", "Eye of Kilrogg", "Sentry Totem",
        // Runeforging and its runes (also banned by skill line).
        "Runeforging", "Rune of Cinderglacier", "Rune of Razorice", "Rune of Spellbreaking",
        "Rune of Spellshattering", "Rune of Lichbane", "Rune of Swordbreaking", "Rune of Swordshattering",
        "Rune of the Fallen Crusader", "Rune of the Nerubian Carapace", "Rune of the Stoneskin Gargoyle",
        // Others.
        "Ritual of Refreshment", "Divine Intervention", "Lesser Heal", "Disarm Trap", "Create Firestone",
    };

    // Available from level 1 (their first rank; later ranks keep their trainer levels). This includes
    // passives and talent abilities, which are otherwise never offered.
    constexpr char const* LEVEL_ONE_ABILITIES[] = {
        // Death Knight
        "Blood Strike", "Blood Presence", "Blood Boil", "Dark Command", "Icy Touch", "Frost Presence",
        "Runic Focus", "Death and Decay", "Plague Strike", "Death Coil", "Unholy Presence", "Death Strike",
        // Druid
        "Entangling Roots", "Thorns", "Wrath", "Moonfire", "Nature's Grasp", "Teleport: Moonglade",
        "Demoralizing Roar", "Cat Form", "Swipe (Bear)", "Rip", "Claw", "Prowl", "Bear Form", "Growl", "Maul",
        "Feral Charge", "Swipe (Cat)", "Rejuvenation", "Mark of the Wild", "Healing Touch",
        // Hunter
        "Auto Shot", "Hunter's Mark", "Serpent Sting", "Arcane Shot", "Concussive Shot", "Steady Shot",
        "Mongoose Bite", "Raptor Strike", "Tame Beast", "Aspect of the Monkey", "Aspect of the Hawk",
        // Mage
        "Frostbolt", "Frost Nova", "Frost Armor", "Fireball", "Fire Blast", "Polymorph", "Conjure Food",
        "Arcane Intellect", "Arcane Missiles", "Conjure Water", "Mage Armor",
        // Paladin
        "Devotion Aura", "Divine Protection", "Hammer of Justice", "Hand of Protection", "Righteous Fury",
        "Shield of Righteousness", "Blessing of Might", "Judgement of Light", "Crusader Strike", "Lay on Hands",
        "Holy Light", "Purify", "Seal of Wisdom", "Seal of Righteousness", "Consecration",
        // Priest
        "Renew", "Smite", "Resurrection", "Greater Heal", "Power Word: Shield", "Power Word: Fortitude", "Fade",
        "Shadow Word: Pain", "Mind Blast",
        // Rogue
        "Backstab", "Sinister Strike", "Gouge", "Sprint", "Evasion", "Pick Pocket", "Stealth", "Sap", "Mutilate",
        "Eviscerate", "Slice and Dice",
        // Shaman
        "Lightning Bolt", "Earthbind Totem", "Searing Totem", "Stoneclaw Totem", "Earth Shock", "Flame Shock",
        "Lightning Shield", "Rockbiter Weapon", "Flametongue Weapon", "Stoneskin Totem", "Strength of Earth Totem",
        "Stormstrike", "Healing Wave",
        // Warlock
        "Corruption", "Curse of Weakness", "Drain Soul", "Life Tap", "Fear", "Immolate", "Shadow Bolt",
        "Demon Skin", "Summon Imp", "Summon Voidwalker", "Create Healthstone",
        // Warrior
        "Heroic Strike", "Charge", "Rend", "Hamstring", "Battle Stance", "Thunder Clap", "Battle Shout",
        "Victory Rush", "Defensive Stance", "Taunt", "Shield Block", "Bloodrage", "Sunder Armor",
    };

    // Learned together with Tame Beast (and removed with it), so a tamed pet can be called back,
    // dismissed, revived and fed. They are not offered on their own.
    constexpr char const* TAME_BEAST = "Tame Beast";
    constexpr char const* TAME_BEAST_COMPANIONS[] = { "Call Pet", "Dismiss Pet", "Revive Pet", "Feed Pet" };

    // Every player always knows Pick Lock, with Lockpicking at its maximum for their level.
    constexpr uint32 SPELL_PICK_LOCK = 1804;

    // Emberforge's own abilities (no trainer teaches them): the spells are in spell_dbc
    // (data/sql/db-world/titan_demon_hunter_spells.sql) and the client patch (patch_spells.py).
    struct CustomAbility
    {
        uint32 SpellId;
        uint8 ClassId;
        uint32 SkillLine;
        uint8 Level;
    };
    constexpr CustomAbility CUSTOM_ABILITIES[] = {
        { 90100, CLASS_DEMON_HUNTER, SKILL_HAVOC, 1 },      // Demon's Bite
        { 90101, CLASS_DEMON_HUNTER, SKILL_HAVOC, 1 },      // Chaos Strike
        { 90103, CLASS_DEMON_HUNTER, SKILL_HAVOC, 1 },      // Fel Rush
        { 90111, CLASS_DEMON_HUNTER, SKILL_HAVOC, 4 },      // Throw Glaive
        { 90102, CLASS_DEMON_HUNTER, SKILL_HAVOC, 10 },     // Blade Dance
        { 90114, CLASS_DEMON_HUNTER, SKILL_HAVOC, 12 },     // Consume Magic
        { 90109, CLASS_DEMON_HUNTER, SKILL_HAVOC, 14 },     // Vengeful Retreat
        { 90113, CLASS_DEMON_HUNTER, SKILL_HAVOC, 18 },     // Blur
        { 90105, CLASS_DEMON_HUNTER, SKILL_HAVOC, 20 },     // Eye Beam
        { 90112, CLASS_DEMON_HUNTER, SKILL_HAVOC, 24 },     // Chaos Nova
        { 90116, CLASS_DEMON_HUNTER, SKILL_HAVOC, 30 },     // Metamorphosis
    };
    constexpr std::pair<uint32, char const*> CUSTOM_SKILL_LINES[] = {
        { SKILL_HAVOC, "Havoc" },
        { SKILL_VENGEANCE, "Vengeance" },
    };

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

    std::string_view SpellName(uint32 spellId)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        char const* name = spellInfo ? spellInfo->SpellName[LOCALE_enUS] : nullptr;
        return name ? name : "";
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
    // once its checks pass; with reveal the player's Classless window spins for it.
    bool GrantTalent(Player* player, bool reveal)
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
        if (talentInfo->addToSpellBook && !spellInfo->HasAttribute(SPELL_ATTR0_PASSIVE)
            && !spellInfo->HasEffect(SPELL_EFFECT_LEARN_SPELL))
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
        if (reveal)
            Classless::RevealTalent(player, talentInfo->TalentID, spellId, currentRank + 1);
        return true;
    }

    void DestroyTomes(Player* player)
    {
        for (uint32 entry : { TOME_OF_RANDOMIZATION, TOME_OF_SELECTION })
            if (uint32 count = player->GetItemCount(entry, true))
                player->DestroyItemCount(entry, count, true);
    }

    // Unlearns banned abilities; returns how many the player had.
    uint32 RemoveBanned(Player* player)
    {
        uint32 removed = 0;
        for (Ability const& ability : Abilities())
        {
            if (!ability.Banned || KnownRank(player, ability) < 0)
                continue;

            for (Rank const& rank : ability.Ranks)
                if (player->HasSpell(rank.SpellId))
                    player->removeSpell(rank.SpellId, SPEC_MASK_ALL, false);

            ++removed;
        }

        return removed;
    }

    // Grants what the player's level is owed. Level-ups (levelUp) play a spin in the Classless window
    // for each ability and talent; catching up at login does not.
    void CatchUp(Player* player, bool grantAbilities, bool levelUp)
    {
        uint8 level = player->GetLevel();

        // Banned abilities go, and are owed again (rolled anew below).
        if (uint32 removed = RemoveBanned(player))
        {
            uint32 granted = player->GetPlayerSetting(SETTING_SOURCE, SETTING_ABILITIES_GRANTED).value;
            player->UpdatePlayerSetting(SETTING_SOURCE, SETTING_ABILITIES_GRANTED,
                granted > removed ? granted - removed : 0);
        }

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
            // Kit abilities come without a roll (the tomes handle them).
            while (granted < owed)
            {
                bool kit = granted < KIT_SIZE;
                Ability const* ability = GrantRandom(player, kit ? "kit ability" : "new ability");
                if (!ability)
                    break;

                ++granted;
                if (!kit && levelUp)
                    Classless::Reveal(player, *ability);
            }

            if (granted != before)
                player->UpdatePlayerSetting(SETTING_SOURCE, SETTING_ABILITIES_GRANTED, granted);
        }

        UpgradeRanks(player);

        uint32 owedTalents = level >= FIRST_TALENT_LEVEL ? level - FIRST_TALENT_LEVEL + 1 : 0;
        bool talentsChanged = false;
        for (uint32 spent = SpentTalentPoints(player); spent < owedTalents && GrantTalent(player, levelUp); ++spent)
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

    // Every class trainer spell, grouped into rank chains with the level each rank needs, plus the
    // level 1 abilities no trainer teaches (talent and starting spells). Built on first use, after the
    // world database is loaded.
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

            std::vector<Ability> result;
            std::unordered_set<uint32> chains;
            auto addChain = [&](uint32 firstRank, uint8 classId)
            {
                if (!chains.insert(firstRank).second)
                    return;

                Ability ability { };
                ability.ClassId = classId;
                for (uint32 rank = firstRank; rank; rank = sSpellMgr->GetNextSpellInChain(rank))
                {
                    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(rank);
                    if (!spellInfo)
                        break;

                    // Ranks no trainer teaches (a class's starting rank 1) use the spell's own level.
                    auto itr = trainerLevel.find(rank);
                    uint8 level = itr != trainerLevel.end() ? itr->second
                        : uint8(std::max<uint32>(1, spellInfo->SpellLevel));
                    ability.Ranks.push_back({ rank, level });

                    if (!ability.ClassId)
                        if (auto classItr = trainerClass.find(rank); classItr != trainerClass.end())
                            ability.ClassId = classItr->second;
                }

                if (!ability.Ranks.empty())
                {
                    ability.SkillLine = ClassSkillLine(firstRank);
                    result.push_back(std::move(ability));
                }
            };

            for (auto const& [spellId, level] : trainerLevel)
                addChain(sSpellMgr->GetFirstSpellInChain(spellId), 0);

            std::unordered_map<uint32, uint8> skillClass;      // class skill line -> class
            for (Ability const& ability : result)
                if (ability.SkillLine && ability.ClassId)
                    skillClass.emplace(ability.SkillLine, ability.ClassId);

            auto hasName = [](Ability const& ability, std::string_view name)
            {
                return std::any_of(ability.Ranks.begin(), ability.Ranks.end(),
                    [name](Rank const& rank) { return SpellName(rank.SpellId) == name; });
            };

            // Level 1 abilities no trainer teaches: found in the class skill lines (talents, starting spells).
            // What a found spell teaches counts as that ability (Feral Charge: its Bear and Cat versions).
            std::unordered_set<std::string_view> found;
            std::unordered_set<uint32> foundLevelOne;
            for (char const* name : LEVEL_ONE_ABILITIES)
            {
                if (std::any_of(result.begin(), result.end(), [&](Ability const& a) { return hasName(a, name); }))
                    continue;

                for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
                {
                    SkillLineAbilityEntry const* entry = sSkillLineAbilityStore.LookupEntry(i);
                    if (!entry || SpellName(entry->Spell) != name)
                        continue;

                    SkillLineEntry const* skill = sSkillLineStore.LookupEntry(entry->SkillLine);
                    if (!skill || skill->categoryId != SKILL_CATEGORY_CLASS)
                        continue;

                    auto classItr = skillClass.find(skill->id);
                    for (uint32 spellId : TaughtSpells(entry->Spell))
                    {
                        uint32 firstRank = sSpellMgr->GetFirstSpellInChain(spellId);
                        addChain(firstRank, classItr != skillClass.end() ? classItr->second : 0);
                        foundLevelOne.insert(firstRank);
                        found.insert(name);
                    }
                }
            }

            auto matches = [&](Ability const& ability, auto const& names)
            {
                bool matched = false;
                for (std::string_view name : names)
                    if (hasName(ability, name))
                    {
                        found.insert(name);
                        matched = true;
                    }
                return matched;
            };

            std::vector<uint32> companions;
            for (Ability& ability : result)
            {
                uint32 firstRank = ability.Ranks.front().SpellId;
                bool runeforging = std::any_of(ability.Ranks.begin(), ability.Ranks.end(), [](Rank const& rank)
                {
                    SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(rank.SpellId);
                    return std::any_of(bounds.first, bounds.second,
                        [](auto const& pair) { return pair.second->SkillLine == SKILL_RUNEFORGING; });
                });
                ability.Banned = matches(ability, BANNED_ABILITIES) || runeforging;
                bool levelOne = matches(ability, LEVEL_ONE_ABILITIES) || foundLevelOne.count(firstRank);
                ability.Companion = matches(ability, TAME_BEAST_COMPANIONS);
                if (ability.Companion)
                    companions.push_back(firstRank);

                if (levelOne)
                    ability.Ranks.front().Level = 1;

                SpellInfo const* firstInfo = sSpellMgr->AssertSpellInfo(firstRank);
                ability.Offered = !ability.Banned && !ability.Companion && firstRank != SPELL_PICK_LOCK
                    && (levelOne || (!GetTalentSpellCost(firstRank) && !firstInfo->IsPassive()));
            }

            for (Ability& ability : result)
                if (hasName(ability, TAME_BEAST))
                    ability.Companions = companions;

            for (CustomAbility const& custom : CUSTOM_ABILITIES)
            {
                if (!sSpellMgr->GetSpellInfo(custom.SpellId))
                {
                    LOG_ERROR("module", "mod-titan: Wildcard ability spell {} is missing (is "
                        "titan_demon_hunter_spells.sql applied?)", custom.SpellId);
                    continue;
                }

                Ability ability { };
                ability.Ranks.push_back({ custom.SpellId, custom.Level });
                ability.ClassId = custom.ClassId;
                ability.SkillLine = custom.SkillLine;
                ability.Offered = true;
                result.push_back(std::move(ability));
            }

            for (char const* name : BANNED_ABILITIES)
                if (!found.count(name))
                    LOG_WARN("module", "mod-titan: Wildcard banned ability \"{}\" matches no class spell", name);
            for (char const* name : LEVEL_ONE_ABILITIES)
                if (!found.count(name))
                    LOG_WARN("module", "mod-titan: Wildcard level 1 ability \"{}\" matches no class spell", name);

            LOG_INFO("module", "mod-titan: Wildcard pool has {} abilities ({} rank chains, {} banned)",
                std::count_if(result.begin(), result.end(), [](Ability const& a) { return a.Offered; }), result.size(),
                std::count_if(result.begin(), result.end(), [](Ability const& a) { return a.Banned; }));
            return result;
        }();

        return abilities;
    }

    char const* CustomSkillLineName(uint32 skillLine)
    {
        for (auto const& [id, name] : CUSTOM_SKILL_LINES)
            if (id == skillLine)
                return name;

        return nullptr;
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

        for (uint32 companionId : ability.Companions)
            if (Ability const* companion = FindAbility(companionId))
                if (int32 best = BestRank(player, *companion); best >= 0 && KnownRank(player, *companion) < 0)
                    player->learnSpell(companion->Ranks[best].SpellId);
    }

    Ability const* GrantRandom(Player* player, char const* what)
    {
        std::vector<Ability const*> candidates;
        for (Ability const& ability : Abilities())
            if (CanLearn(player, ability))
                candidates.push_back(&ability);

        if (candidates.empty())
            return nullptr;

        Ability const* ability = candidates[urand(0, uint32(candidates.size() - 1))];
        Learn(player, *ability, what);
        return ability;
    }

    void Unlearn(Player* player, Ability const& ability)
    {
        for (Rank const& rank : ability.Ranks)
            if (player->HasSpell(rank.SpellId))
                player->removeSpell(rank.SpellId, SPEC_MASK_ALL, false);

        for (uint32 companionId : ability.Companions)
            if (Ability const* companion = FindAbility(companionId))
                for (Rank const& rank : companion->Ranks)
                    if (player->HasSpell(rank.SpellId))
                        player->removeSpell(rank.SpellId, SPEC_MASK_ALL, false);
    }

    uint32 ClearAbilities(Player* player)
    {
        uint32 cleared = 0;
        for (Ability const& ability : Abilities())
        {
            if (!(ability.Offered || ability.Companion) || KnownRank(player, ability) < 0)
                continue;

            Unlearn(player, ability);
            if (ability.Offered)
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

namespace
{
    // Pick Lock for everyone. Lockpicking is a Rogue skill: the core drops it at every login for any
    // other class (it is invalid for the race/class), so it is set again each time, at its maximum.
    void EnsurePickLock(Player* player)
    {
        if (!player->HasSpell(SPELL_PICK_LOCK))
            player->learnSpell(SPELL_PICK_LOCK);

        uint16 maxValue = uint16(player->GetLevel()) * 5;
        if (player->GetPureSkillValue(SKILL_LOCKPICKING) != maxValue
            || player->GetPureMaxSkillValue(SKILL_LOCKPICKING) != maxValue)
            player->SetSkill(SKILL_LOCKPICKING, 0, maxValue, maxValue);
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
        EnsurePickLock(player);
        if (!IsHero(player))
            return;

        // Without saved settings the ability count resets every login and would be granted again.
        bool settingsSaved = sWorld->getBoolConfig(CONFIG_PLAYER_SETTINGS_ENABLED);
        static bool warned = false;
        if (!warned)
        {
            warned = true;
            if (!settingsSaved)
                LOG_ERROR("module", "mod-titan: Wildcard needs EnablePlayerSettings = 1 in worldserver.conf; "
                    "abilities are only granted on level up until then");
            if (sWorld->getBoolConfig(CONFIG_VALIDATE_SKILL_LEARNED_BY_SPELLS))
                LOG_ERROR("module", "mod-titan: Wildcard needs ValidateSkillLearnedBySpells = 0 in worldserver.conf; "
                    "other classes' spells are deleted at login until then");
        }

        CatchUp(player, settingsSaved, false);
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        EnsurePickLock(player);
        if (IsHero(player))
            CatchUp(player, true, true);
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
