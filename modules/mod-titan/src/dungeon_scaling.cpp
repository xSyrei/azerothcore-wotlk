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

#include "Creature.h"
#include "DBCStores.h"
#include "Formulas.h"
#include "Group.h"
#include "Map.h"
#include "Pet.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "World.h"
#include <algorithm>

// Dungeon scaling, Dragonflight's way: every 5-player dungeon is fought at every Hero's own level.
//
// The dungeon's creatures are at the group's lowest level (bosses BOSS_LEVELS over): the core's own stats for that
// level (Creature::SelectLevel, through OnBeforeCreatureSelectLevel: health, mana, armor, melee damage), set when a
// creature spawns or respawns and again for every creature out of combat whenever a player enters. Their spells
// (damage, healing, their own buffs' damage: Lightning Shield) carry their native level's amounts, so are scaled by
// the class damage (healing: health) at the level they are at over the native one. Then, for every player in a
// 5-player dungeon:
// - the creature is as if at the player's own level (the same offset from the group's lowest level): the damage the
//   player and their pets deal is scaled by the creature's health at its level over its health at that one, and the
//   damage they take by the creature's damage there over its own (creature_classlevelstats, its class), so a level
//   45 Hero with a level 12 meets the level 12's creatures as level 45 ones;
// - healing between players of different levels by their health the same way (a level 45's heal on a level 12);
// - easier the lower the Hero: at level 10 creatures have half their health (the Hero's damage doubled) and deal
//   75% of their damage, evenly up to the intended difficulty at the level cap (heroics: always);
// - experience for kills as if the creature were at their level: the core's (none for a creature gray to them) is
//   replaced by Acore::XP::Gain's at that level, split evenly over the group's members in range.
namespace
{
    constexpr uint8 EASE_LEVEL = 10;            // the level the ease is greatest at (and below)
    constexpr float EASE_HEALTH = 0.5f;         // creatures' health there
    constexpr float EASE_DAMAGE = 0.75f;        // creatures' damage there
    constexpr uint8 HEALTH_PROXY_CLASS = CLASS_WARRIOR;         // players' health by level: a warrior creature's
    constexpr uint8 HIGHEST_STATS_LEVEL = 83;
    constexpr uint8 BOSS_LEVELS = 2;            // bosses over the group's lowest level

    // A dungeon's scaling, on its map (its own thread's).
    struct ScalingInfo : public DataMap::Base
    {
        uint8 target = 0;                       // the level the creatures are at (0: no player yet, native levels)
    };

    bool granting = false;                      // our own experience, through OnPlayerGiveXP

    bool IsScaled(Map const* map)
    {
        return map && map->IsDungeon() && !map->IsRaid() && map->GetInstanceId();
    }

    uint8 MaxLevel()
    {
        return uint8(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));
    }

    // How far along from the ease to the intended difficulty a level is (0 at EASE_LEVEL, 1 at the cap).
    float Progress(uint8 level)
    {
        uint8 cap = MaxLevel();
        if (cap <= EASE_LEVEL)
            return 1.0f;
        return std::clamp(float(int32(level) - EASE_LEVEL) / float(cap - EASE_LEVEL), 0.0f, 1.0f);
    }

    float HealthFactor(uint8 level)
    {
        return EASE_HEALTH + (1.0f - EASE_HEALTH) * Progress(level);
    }

    float DamageFactor(uint8 level)
    {
        return EASE_DAMAGE + (1.0f - EASE_DAMAGE) * Progress(level);
    }

    ScalingInfo* Info(Map* map)
    {
        return map->CustomData.GetDefault<ScalingInfo>("titan_dungeon_scaling");
    }

    // The lowest player's level (GMs aside).
    uint8 LowestPlayer(Map* map)
    {
        uint8 lowest = 0;
        for (auto const& ref : map->GetPlayers())
            if (Player* player = ref.GetSource())
                if (!player->IsGameMaster() && (!lowest || player->GetLevel() < lowest))
                    lowest = player->GetLevel();
        return lowest;
    }

    // The level the creatures are scaled to.
    uint8 TargetLevel(Map* map)
    {
        uint8 target = Info(map)->target;
        return target ? target : LowestPlayer(map);
    }

    // The dungeon's own creature (not a player's pet, guardian, totem or charm).
    bool IsDungeonCreature(Creature const* creature)
    {
        return !creature->IsPet() && !creature->IsControlledByPlayer() && !creature->GetCharmerOrOwnerGUID().IsPlayer();
    }

    uint8 LevelInDungeon(Creature const* creature, uint8 target)
    {
        int32 level = int32(target) + (creature->IsDungeonBoss() ? BOSS_LEVELS : 0);
        return uint8(std::clamp<int32>(level, 1, HIGHEST_STATS_LEVEL));
    }

    // The level a creature is to a player: the player's, with the creature's offset from the group's lowest level.
    uint8 LevelFor(Creature const* creature, uint8 playerLevel, uint8 target)
    {
        int32 level = int32(playerLevel) + int32(creature->GetLevel()) - int32(target ? target : creature->GetLevel());
        return uint8(std::clamp<int32>(level, 1, HIGHEST_STATS_LEVEL));
    }

    CreatureBaseStats const* Stats(uint8 level, uint8 unitClass)
    {
        return sObjectMgr->GetCreatureBaseStats(std::min(level, HIGHEST_STATS_LEVEL), unitClass);
    }

    float Health(uint8 level, uint8 unitClass, uint8 expansion)
    {
        CreatureBaseStats const* stats = Stats(level, unitClass);
        return stats ? float(stats->BaseHealth[expansion]) : 0.0f;
    }

    float Damage(uint8 level, uint8 unitClass, uint8 expansion)
    {
        CreatureBaseStats const* stats = Stats(level, unitClass);
        return stats ? stats->BaseDamage[expansion] : 0.0f;
    }

    // The player behind a unit (itself, or its pet's, totem's or guardian's owner).
    Player* PlayerOf(Unit* unit)
    {
        return unit ? unit->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
    }

    // A creature fighting for the dungeon (not one a player controls).
    Creature* EnemyOf(Unit* unit)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        return creature && !creature->IsControlledByPlayer() && !creature->IsPet() ? creature : nullptr;
    }

    // A dungeon creature's spell, from its native level's amount to the level it is at: by its class damage there
    // over its own (healing: health). Spells by a share of health stay as they are.
    float SpellScale(Unit* caster, SpellInfo const* spellInfo, bool healing)
    {
        Creature* creature = caster ? caster->ToCreature() : nullptr;
        if (!creature || !spellInfo || !IsDungeonCreature(creature) || !IsScaled(creature->GetMap())
            || spellInfo->HasAura(SPELL_AURA_PERIODIC_DAMAGE_PERCENT))
            return 1.0f;
        CreatureTemplate const* info = creature->GetCreatureTemplate();
        uint8 native = std::max(info->minlevel, info->maxlevel);
        if (native == creature->GetLevel())
            return 1.0f;
        float own = healing ? Health(native, info->unit_class, info->expansion)
            : Damage(native, info->unit_class, info->expansion);
        float now = healing ? Health(creature->GetLevel(), info->unit_class, info->expansion)
            : Damage(creature->GetLevel(), info->unit_class, info->expansion);
        return own > 0.0f && now > 0.0f ? now / own : 1.0f;
    }

    // A creature's level and the stats the core gives it there (Creature::UpdateEntry's armor, then every stat).
    void Rescale(Creature* creature)
    {
        creature->SelectLevel(true);
        CreatureTemplate const* info = creature->GetCreatureTemplate();
        if (CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(creature->GetLevel(), info->unit_class))
            creature->SetStatFlatModifier(UNIT_MOD_ARMOR, BASE_VALUE, stats->GenerateArmor(info));
        creature->UpdateAllStats();
    }

    // Every creature of the dungeon out of combat to the group's lowest level (a fight in progress keeps its own).
    void ScaleTo(Map* map, uint8 target)
    {
        ScalingInfo* info = Info(map);
        if (!target || info->target == target)
            return;
        info->target = target;
        for (auto const& [spawnId, creature] : map->GetCreatureBySpawnIdStore())
            if (creature->IsInWorld() && creature->IsAlive() && !creature->IsInCombat() && IsDungeonCreature(creature))
                Rescale(creature);
    }

    // Damage a player (or their pet) deals to a creature, or takes from one.
    float DamageScale(Unit* target, Unit* attacker)
    {
        if (!attacker || !target || !IsScaled(target->GetMap()))
            return 1.0f;

        if (Creature* enemy = EnemyOf(target))
            if (Player* player = PlayerOf(attacker))
            {
                CreatureTemplate const* info = enemy->GetCreatureTemplate();
                uint8 at = LevelFor(enemy, player->GetLevel(), TargetLevel(enemy->GetMap()));
                float own = Health(enemy->GetLevel(), info->unit_class, info->expansion);
                float there = Health(at, info->unit_class, info->expansion);
                float scale = there > 0.0f ? own / there : 1.0f;
                return scale / HealthFactor(player->GetLevel());
            }

        if (Creature* enemy = EnemyOf(attacker))
            if (Player* player = PlayerOf(target))
            {
                CreatureTemplate const* info = enemy->GetCreatureTemplate();
                uint8 at = LevelFor(enemy, player->GetLevel(), TargetLevel(enemy->GetMap()));
                float own = Damage(enemy->GetLevel(), info->unit_class, info->expansion);
                float there = Damage(at, info->unit_class, info->expansion);
                float scale = own > 0.0f ? there / own : 1.0f;
                return scale * DamageFactor(player->GetLevel());
            }

        return 1.0f;
    }

    template <typename T>
    void Scale(T& amount, float scale)
    {
        if (scale != 1.0f && amount > 0)
            amount = T(std::max(1.0f, float(amount) * scale + 0.5f));
    }

    // Experience for a kill, as if the creature were at the player's level (Acore::XP::Gain's and KillRewarder's).
    uint32 KillExperience(Player* player, Creature* creature, uint8 target, uint32 members, float groupRate)
    {
        uint8 level = player->GetLevel();
        uint32 gain = Acore::XP::BaseGain(level, LevelFor(creature, level, target),
            GetContentLevelsForMapAndZone(creature->GetMapId(), creature->GetZoneId()));
        if (!gain)
            return 0;

        CreatureTemplate const* info = creature->GetCreatureTemplate();
        float xp = float(gain) * info->ModExperience * sWorld->getRate(RATE_XP_KILL);
        if (creature->isElite())
            xp *= 2.0f;
        if (info->ModHealth <= 0.75f && info->ModHealth >= 0.0f)  // weak creatures give less (KillRewarder)
            xp *= info->ModHealth;
        xp *= groupRate / float(std::max<uint32>(members, 1));
        xp *= player->GetTotalAuraMultiplier(SPELL_AURA_MOD_XP_PCT);
        return uint32(xp);
    }

    // The player the kill rewards, as Unit::Kill finds them: the creature's tapper, else its tapping group's first
    // member in range.
    Player* Rewarded(Creature* creature)
    {
        if (!creature->IsDamageEnoughForLootingAndReward() || creature->IsLootRewardDisabled())
            return nullptr;
        if (Player* tapper = creature->GetLootRecipient())
            if (tapper->IsInMap(creature))
                return tapper;
        if (Group* group = creature->GetLootRecipientGroup())
            for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
                if (Player* member = itr->GetSource())
                    if (member->IsAtLootRewardDistance(creature))
                        return member;
        return nullptr;
    }

    void RewardKill(Creature* creature)
    {
        if (creature->IsTotem() || creature->IsPet() || creature->IsCritter()
            || creature->HasFlagsExtra(CREATURE_FLAG_EXTRA_NO_XP))
            return;
        Player* player = Rewarded(creature);
        if (!player)
            return;

        std::vector<Player*> members;
        Group* group = player->GetGroup();
        if (group)
        {
            for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
                if (Player* member = itr->GetSource())
                    if (member == player || member->IsAtGroupRewardDistance(creature))
                        members.push_back(member);
        }
        else
            members.push_back(player);

        uint32 alive = uint32(std::count_if(members.begin(), members.end(), [](Player* m) { return m->IsAlive(); }));
        float groupRate = group ? Acore::XP::xp_in_group_rate(alive, false) : 1.0f;
        uint8 target = TargetLevel(creature->GetMap());
        for (Player* member : members)
        {
            if (!member->IsAlive() || member->GetLevel() >= MaxLevel())
                continue;
            uint32 xp = KillExperience(member, creature, target, group ? alive : 1, groupRate);
            if (!xp)
                continue;
            granting = true;
            sScriptMgr->OnPlayerGiveXP(member, xp, creature, XPSOURCE_KILL);
            granting = false;
            member->GiveXP(xp, creature, groupRate);
            if (Pet* pet = member->GetPet())
                pet->GivePetXP(group ? xp / 2 : xp);
        }
    }
}

class titan_dungeon_scaling_unit : public UnitScript
{
public:
    titan_dungeon_scaling_unit() : UnitScript("titan_dungeon_scaling_unit", true, {
        UNITHOOK_MODIFY_MELEE_DAMAGE,
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
        UNITHOOK_MODIFY_HEAL_RECEIVED,
        UNITHOOK_ON_UNIT_DEATH
    }) { }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        Scale(damage, DamageScale(target, attacker));
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        Scale(damage, DamageScale(target, attacker) * SpellScale(attacker, spellInfo, false));
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage,
        SpellInfo const* spellInfo) override
    {
        Scale(damage, DamageScale(target, attacker) * SpellScale(attacker, spellInfo, false));
    }

    // A heal between players of different levels: by their health at each level.
    void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* spellInfo) override
    {
        if (!target || !IsScaled(target->GetMap()))
            return;
        // A creature's heal: from its native level, as its damage.
        if (Creature* creature = healer ? healer->ToCreature() : nullptr; creature && IsDungeonCreature(creature))
        {
            Scale(heal, SpellScale(healer, spellInfo, true));
            return;
        }
        Player* from = PlayerOf(healer);
        Player* to = PlayerOf(target);
        if (!from || !to || from->GetLevel() == to->GetLevel())
            return;
        float own = Health(from->GetLevel(), HEALTH_PROXY_CLASS, 0);
        float there = Health(to->GetLevel(), HEALTH_PROXY_CLASS, 0);
        if (own > 0.0f)
            Scale(heal, there / own);
    }

    void OnUnitDeath(Unit* unit, Unit* killer) override
    {
        if (Creature* creature = EnemyOf(unit))
            if (killer && IsScaled(creature->GetMap()))
                RewardKill(creature);
    }
};

// A dungeon creature's level as it spawns or respawns (the core then gives it that level's stats).
class titan_dungeon_scaling_creature : public AllCreatureScript
{
public:
    titan_dungeon_scaling_creature() : AllCreatureScript("titan_dungeon_scaling_creature") { }

    void OnBeforeCreatureSelectLevel(CreatureTemplate const* /*cinfo*/, Creature* creature, uint8& level) override
    {
        Map* map = creature->GetMap();
        if (!IsScaled(map) || !IsDungeonCreature(creature))
            return;
        if (uint8 target = Info(map)->target)
            level = LevelInDungeon(creature, target);
    }
};

// A player entering: the dungeon at the lowest level of those in it.
class titan_dungeon_scaling_map : public AllMapScript
{
public:
    titan_dungeon_scaling_map() : AllMapScript("titan_dungeon_scaling_map", { ALLMAPHOOK_ON_PLAYER_ENTER_ALL }) { }

    void OnPlayerEnterAll(Map* map, Player* player) override
    {
        if (IsScaled(map) && !player->IsGameMaster())
            ScaleTo(map, LowestPlayer(map));
    }
};

class titan_dungeon_scaling_player : public PlayerScript
{
public:
    titan_dungeon_scaling_player() : PlayerScript("titan_dungeon_scaling_player", { PLAYERHOOK_ON_GIVE_EXP }) { }

    // The core's experience for a kill in a scaled dungeon: none, ours instead (RewardKill).
    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* victim, uint8 xpSource) override
    {
        if (!granting && xpSource == XPSOURCE_KILL && EnemyOf(victim) && IsScaled(player->GetMap()))
            amount = 0;
    }
};

void AddSC_dungeon_scaling()
{
    new titan_dungeon_scaling_unit();
    new titan_dungeon_scaling_player();
    new titan_dungeon_scaling_creature();
    new titan_dungeon_scaling_map();
}
