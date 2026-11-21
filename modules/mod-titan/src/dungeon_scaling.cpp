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

#include "Chat.h"
#include "CommandScript.h"
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
#include "wildcard.h"
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
// - and twice the health throughout (HEALTH_PASS): the damage the Hero deals halved, the health they see doubled;
//   their damage 65% more (DAMAGE_PASS); a boss's (and what it summons') damage 230% more on a tank, 15% less on
//   anyone else (BOSS_DAMAGE_TANK, BOSS_DAMAGE_OTHERS: a boss mechanic a player deals to those around them, a
//   tank's included, the latter);
// - experience for kills as if the creature were at their level: the core's (none for a creature gray to them) is
//   replaced by Acore::XP::Gain's at that level, split evenly over the group's members in range.
namespace
{
    constexpr uint8 EASE_LEVEL = 10;            // the level the ease is greatest at (and below)
    constexpr float EASE_HEALTH = 0.5f;         // creatures' health there
    constexpr float EASE_DAMAGE = 0.75f;        // creatures' damage there
    // Every scaled creature's health on top of all that (Heroes melted them at the intended difficulty).
    constexpr float HEALTH_PASS = 2.0f;
    // And their damage on top of all that.
    constexpr float DAMAGE_PASS = 1.65f;
    // A boss's damage on a Hero by their role: a tank's more (+30%, then +200% more), everyone else's less.
    constexpr float BOSS_DAMAGE_TANK = 3.3f;
    constexpr float BOSS_DAMAGE_OTHERS = 0.85f;
    // A boss kill's experience: this share of the Hero's level, for every member alike.
    constexpr float BOSS_EXPERIENCE_SHARE = 0.75f;
    constexpr uint8 HEALTH_PROXY_CLASS = CLASS_WARRIOR;         // players' health by level: a warrior creature's
    constexpr uint8 HIGHEST_STATS_LEVEL = 83;
    constexpr uint8 BOSS_LEVELS = 2;            // bosses over the group's lowest level
    // The most a creature's DamageModifier counts for (creature_template; trash, bosses): each expansion tuned its own
    // (dungeon elites: Classic's about 4.6, Wrath's 7.5 to 15), which at a level 20's base damage made a pack of
    // Wrath trash hit for hundreds. Melee and spells alike.
    constexpr float MAX_DAMAGE_MODIFIER = 3.0f;
    constexpr float MAX_BOSS_DAMAGE_MODIFIER = 5.0f;
    constexpr uint8 NATIVE_LEVEL_SPREAD = 10;   // a creature this far from the dungeon's level counts as at it

    // A dungeon's scaling, on its map (its own thread's).
    struct ScalingInfo : public DataMap::Base
    {
        uint8 target = 0;                       // the level the creatures are at (0: no player yet, native levels)
        int16 native = -1;                      // the dungeon's own level (LFGDungeons.dbc; -1 not looked up, 0 none)
        uint8 expansion = 0;                    // and its expansion (creature_classlevelstats' damage column)
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
        return (EASE_HEALTH + (1.0f - EASE_HEALTH) * Progress(level)) * HEALTH_PASS;
    }

    // A creature's health factor for a player at that level (bosses alike: no health of their own on top).
    float HealthFactorOf(Creature const* /*creature*/, uint8 level)
    {
        return HealthFactor(level);
    }

    // A dungeon boss: flagged so (an encounter of the dungeon's, instance_encounters), a boss by rank, or one by its
    // script (boss_*: the optional bosses no encounter names, Rhahk'Zor's kin).
    bool IsBoss(Creature const* creature)
    {
        if (!creature)
            return false;
        if (creature->IsDungeonBoss() || creature->GetCreatureTemplate()->rank == CREATURE_ELITE_WORLDBOSS)
            return true;
        std::string script = creature->GetScriptName();
        return script.rfind("boss_", 0) == 0;
    }

    // A boss, or what a boss summoned (its void zones, its adds' triggers).
    bool FromBoss(Creature const* creature)
    {
        if (IsBoss(creature))
            return true;
        if (!creature || !creature->IsSummon())
            return false;
        Unit* summoner = creature->ToTempSummon()->GetSummonerUnit();
        return summoner && IsBoss(summoner->ToCreature());
    }

    // A boss's damage on a player (or their pet) by the player's role: a tank (the player themself) takes
    // BOSS_DAMAGE_TANK, anyone else BOSS_DAMAGE_OTHERS.
    float BossRoleScale(Unit* target, Unit* attacker)
    {
        Creature* enemy = attacker ? attacker->ToCreature() : nullptr;
        if (!enemy || enemy->IsControlledByPlayer() || enemy->IsPet() || !FromBoss(enemy) || !target
            || !target->GetCharmerOrOwnerPlayerOrPlayerItself())
            return 1.0f;
        Player* player = target->ToPlayer();
        return player && Wildcard::GetRole(player) == Wildcard::ROLE_TANK ? BOSS_DAMAGE_TANK : BOSS_DAMAGE_OTHERS;
    }

    float DamageFactor(uint8 level)
    {
        return (EASE_DAMAGE + (1.0f - EASE_DAMAGE) * Progress(level)) * DAMAGE_PASS;
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

    // The level the dungeon was made for: its Dungeon Finder target level (this difficulty's, else any), 0 if none.
    uint8 DungeonLevel(Map* map)
    {
        ScalingInfo* scaling = Info(map);
        if (scaling->native < 0)
        {
            scaling->native = 0;
            for (LFGDungeonEntry const* dungeon : sLFGDungeonStore)
                if (dungeon->MapID == map->GetId() && dungeon->TargetLevel
                    && (!scaling->native || dungeon->Difficulty == uint32(map->GetDifficulty())))
                {
                    scaling->native = int16(dungeon->TargetLevel);
                    scaling->expansion = uint8(std::min<uint32>(dungeon->ExpansionLevel,
                        EXPANSION_WRATH_OF_THE_LICH_KING));
                }
        }
        return uint8(scaling->native);
    }

    // A creature's native level, its spells' amounts are for: its template's; the dungeon's for a trigger, or for one
    // far from it (invisible stalkers and helpers are often level 1 or 80, whatever the dungeon).
    uint8 NativeLevel(Creature* creature)
    {
        CreatureTemplate const* info = creature->GetCreatureTemplate();
        uint8 native = std::max(info->minlevel, info->maxlevel);
        uint8 dungeon = DungeonLevel(creature->GetMap());
        if (dungeon && (creature->HasFlagsExtra(CREATURE_FLAG_EXTRA_TRIGGER)
            || std::abs(int32(native) - int32(dungeon)) > NATIVE_LEVEL_SPREAD))
            return dungeon;
        return native;
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
        uint8 native = NativeLevel(creature);
        if (native == creature->GetLevel())
            return 1.0f;
        float own = healing ? Health(native, info->unit_class, info->expansion)
            : Damage(native, info->unit_class, info->expansion);
        float now = healing ? Health(creature->GetLevel(), info->unit_class, info->expansion)
            : Damage(creature->GetLevel(), info->unit_class, info->expansion);
        return own > 0.0f && now > 0.0f ? now / own : 1.0f;
    }

    // A boss mechanic a player deals to another (a spell they don't know: Krystallus' Shatter, cast by each stoned
    // player on those around them; a pulse from a tank's debuff): from the dungeon's level to the one hit's, as a
    // creature's spell, and the ease; and always BOSS_DAMAGE_OTHERS, never a tank's (a pulse from the tank would
    // otherwise shred those about them).
    float MechanicScale(Unit* target, Unit* attacker, SpellInfo const* spellInfo)
    {
        Player* caster = attacker ? attacker->ToPlayer() : nullptr;
        Player* hit = PlayerOf(target);
        if (!caster || !hit || !spellInfo || target == attacker || !IsScaled(caster->GetMap())
            || caster->HasSpell(spellInfo->Id))
            return 1.0f;
        if (spellInfo->HasAura(SPELL_AURA_PERIODIC_DAMAGE_PERCENT))
            return BOSS_DAMAGE_OTHERS;
        uint8 native = DungeonLevel(caster->GetMap());
        if (!native || native <= hit->GetLevel())
            return BOSS_DAMAGE_OTHERS;
        uint8 expansion = Info(caster->GetMap())->expansion;
        float own = Damage(native, HEALTH_PROXY_CLASS, expansion);
        float now = Damage(hit->GetLevel(), HEALTH_PROXY_CLASS, expansion);
        return (own > 0.0f && now > 0.0f ? now / own * DamageFactor(hit->GetLevel()) : 1.0f) * BOSS_DAMAGE_OTHERS;
    }

    // A dungeon creature's damage (melee and spells) with its DamageModifier capped (MAX_DAMAGE_MODIFIER).
    float ModifierCap(Unit* attacker)
    {
        Creature* creature = attacker ? attacker->ToCreature() : nullptr;
        if (!creature || !IsDungeonCreature(creature) || !IsScaled(creature->GetMap()))
            return 1.0f;
        float modifier = creature->GetCreatureTemplate()->DamageModifier;
        float cap = creature->IsDungeonBoss() ? MAX_BOSS_DAMAGE_MODIFIER : MAX_DAMAGE_MODIFIER;
        return modifier > cap ? cap / modifier : 1.0f;
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

    // Every dungeon creature's level and health sent again (a player leveled: their view of each changed).
    void Resend(Map* map)
    {
        for (auto const& [spawnId, creature] : map->GetCreatureBySpawnIdStore())
            if (creature->IsInWorld() && IsDungeonCreature(creature))
                for (uint16 index : { UNIT_FIELD_LEVEL, UNIT_FIELD_HEALTH, UNIT_FIELD_MAXHEALTH })
                    creature->ForceValuesUpdateAtIndex(index);
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
                return scale / HealthFactorOf(enemy, player->GetLevel());
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

    // How a dungeon creature looks to a player: at their level, its health as much as their damage takes to empty it
    // (DamageScale's own over there, and the ease), so a level 60 sees a level 60 creature with a level 60's health.
    struct View
    {
        uint8 level;
        float health;                           // the factor on its health and maximum health
    };

    View ViewFor(Creature const* creature, Player const* viewer)
    {
        CreatureTemplate const* info = creature->GetCreatureTemplate();
        uint8 at = LevelFor(creature, viewer->GetLevel(), TargetLevel(creature->GetMap()));
        float own = Health(creature->GetLevel(), info->unit_class, info->expansion);
        float there = Health(at, info->unit_class, info->expansion);
        float factor = own > 0.0f ? there / own * HealthFactorOf(creature, viewer->GetLevel()) : 1.0f;
        return { at, factor };
    }

    uint32 Shown(uint32 health, float factor)
    {
        return health ? std::max<uint32>(1, uint32(float(health) * factor + 0.5f)) : 0;
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
            // A boss: BOSS_EXPERIENCE_SHARE of the member's level, whatever the boss (their own experience rates
            // differ wildly: some gave a level's worth, some a trash pack's).
            uint32 xp = IsBoss(creature)
                ? uint32(float(member->GetUInt32Value(PLAYER_NEXT_LEVEL_XP)) * BOSS_EXPERIENCE_SHARE)
                : KillExperience(member, creature, target, group ? alive : 1, groupRate);
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
        UNITHOOK_ON_UNIT_DEATH,
        UNITHOOK_SHOULD_TRACK_VALUES_UPDATE_POS_BY_INDEX,
        UNITHOOK_ON_PATCH_VALUES_UPDATE
    }) { }

    // A dungeon creature's level, health and maximum health go to each player as they see it (ViewFor): the core
    // builds the fields once and patches these per receiver.
    bool ShouldTrackValuesUpdatePosByIndex(Unit const* unit, uint8 /*updateType*/, uint16 index) override
    {
        if (index != UNIT_FIELD_LEVEL && index != UNIT_FIELD_HEALTH && index != UNIT_FIELD_MAXHEALTH)
            return false;
        Creature const* creature = unit->ToCreature();
        return creature && IsDungeonCreature(creature) && IsScaled(creature->GetMap());
    }

    void OnPatchValuesUpdate(Unit const* unit, ByteBuffer& buffer, BuildValuesCachePosPointers& positions,
        Player* target) override
    {
        if (positions.other.empty() || !target)
            return;
        Creature const* creature = unit->ToCreature();
        if (!creature || !IsDungeonCreature(creature) || !IsScaled(creature->GetMap()))
            return;
        View view = ViewFor(creature, target);
        for (auto const& [index, position] : positions.other)
        {
            if (index == UNIT_FIELD_LEVEL)
                buffer.put<uint32>(position, view.level);
            else if (index == UNIT_FIELD_HEALTH)
                buffer.put<uint32>(position, Shown(creature->GetHealth(), view.health));
            else if (index == UNIT_FIELD_MAXHEALTH)
                buffer.put<uint32>(position, Shown(creature->GetMaxHealth(), view.health));
        }
    }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        Scale(damage, DamageScale(target, attacker) * ModifierCap(attacker) * BossRoleScale(target, attacker));
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        Scale(damage, DamageScale(target, attacker) * SpellScale(attacker, spellInfo, false) * ModifierCap(attacker)
            * MechanicScale(target, attacker, spellInfo) * BossRoleScale(target, attacker));
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage,
        SpellInfo const* spellInfo) override
    {
        Scale(damage, DamageScale(target, attacker) * SpellScale(attacker, spellInfo, false) * ModifierCap(attacker)
            * MechanicScale(target, attacker, spellInfo) * BossRoleScale(target, attacker));
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
        if (!IsScaled(map) || player->IsGameMaster())
            return;
        ScaleTo(map, LowestPlayer(map));
        Resend(map);
    }
};

class titan_dungeon_scaling_player : public PlayerScript
{
public:
    titan_dungeon_scaling_player() : PlayerScript("titan_dungeon_scaling_player", {
        PLAYERHOOK_ON_GIVE_EXP,
        PLAYERHOOK_ON_LEVEL_CHANGED
    }) { }

    // A player leveling in a dungeon: the dungeon follows the lowest level (every creature out of combat).
    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        Map* map = player->GetMap();
        if (IsScaled(map) && !player->IsGameMaster())
            ScaleTo(map, LowestPlayer(map));
    }

    // The core's experience for a kill in a scaled dungeon: none, ours instead (RewardKill).
    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* victim, uint8 xpSource) override
    {
        if (!granting && xpSource == XPSOURCE_KILL && EnemyOf(victim) && IsScaled(player->GetMap()))
            amount = 0;
    }
};

using namespace Acore::ChatCommands;

// .dungeonscale (GM): what the scaling does here, for you and the creature selected: whether the map is scaled, the
// level its creatures are at and whether you count for it (a GM does not: entering in GM mode leaves the creatures at
// their own levels), the creature's own, current and seen levels, and the factors on your damage to it and its to you.
class titan_dungeon_scaling_commands : public CommandScript
{
public:
    titan_dungeon_scaling_commands() : CommandScript("titan_dungeon_scaling_commands") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "dungeonscale", HandleDungeonScale, SEC_GAMEMASTER, Console::No },
        };
        return commandTable;
    }

    static bool HandleDungeonScale(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        Map* map = player->GetMap();
        if (!IsScaled(map))
        {
            handler->PSendSysMessage("Not a scaled map (5-player dungeons only): {}.", map->GetMapName());
            return true;
        }
        handler->PSendSysMessage("{}: scaled; creatures at level {} (the lowest player's: {}); its own level {}.",
            map->GetMapName(), TargetLevel(map), LowestPlayer(map), DungeonLevel(map));
        if (player->IsGameMaster())
            handler->PSendSysMessage("You are in GM mode: you do not count. Entering in GM mode leaves the creatures "
                "at their own levels; turn it off (.gm off) and enter again to test the scaling.");
        handler->PSendSysMessage("At your level {}: creatures' health x{:.2f}, their damage x{:.2f}.",
            player->GetLevel(), HealthFactor(player->GetLevel()), DamageFactor(player->GetLevel()));
        Creature* creature = handler->getSelectedCreature();
        if (!creature || !EnemyOf(creature))
            return true;
        CreatureTemplate const* info = creature->GetCreatureTemplate();
        uint8 seen = LevelFor(creature, player->GetLevel(), TargetLevel(map));
        handler->PSendSysMessage("{}: template levels {}-{}, now {}, to you {}{}; its health x{:.2f}.",
            creature->GetName(), info->minlevel, info->maxlevel, creature->GetLevel(), seen,
            creature->IsDungeonBoss() ? " (a boss)" : "", HealthFactorOf(creature, player->GetLevel()));
        handler->PSendSysMessage("Your damage to it x{:.3f}; its melee to you x{:.3f} (its damage modifier capped "
            "x{:.2f}).", DamageScale(creature, player), DamageScale(player, creature) * ModifierCap(creature),
            ModifierCap(creature));
        return true;
    }
};

void AddSC_dungeon_scaling()
{
    new titan_dungeon_scaling_commands();
    new titan_dungeon_scaling_unit();
    new titan_dungeon_scaling_player();
    new titan_dungeon_scaling_creature();
    new titan_dungeon_scaling_map();
}
