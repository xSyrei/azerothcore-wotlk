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

#include "runes.h"
#include "specs.h"
#include "wildcard.h"
#include "CellImpl.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "StringFormat.h"
#include "TemporarySummon.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <algorithm>
#include <array>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

// Legendary Runes (the design: the "Emberforge Legendary Runes" doc). Twenty-one runes, each a loop (a builder
// makes a resource, a spender cashes it in) and three modifier slots (Invocation, Manifestation, Adaptation), one
// pick each, a 22nd, the Rune of the Sanctified Paragon (its own concept doc; slots Presence, Manifestation,
// Covenant), and a 23rd, the Rune of the Stormbound Conduit (its own doc; slots Conductivity, Arc Pattern, Lightning
// Shield). Built so far: Fel Commandment, the Sanctified Paragon and the Stormbound Conduit; the panel lists the
// others as coming.
//
// - Unlocked at level 10: the panel (the Emberforge_Runes addon, addon whispers with prefix EFRN) opens by itself
//   while the active specialization has no rune, suggesting up to 3 the Hero's abilities can use, with every rune
//   under Browse all (one missing a required ability says so; it can still be bound after a warning naming what it
//   lacks), each with a Preview of its build that binds nothing.
// - One rune per specialization (titan_runes, db-characters), chosen once: only a prestige clears it. Modifiers
//   change freely out of combat.
// - The rune works through auras: its own while it is the active spec's (90700 Fel Commandment, 90730 the
//   Sanctified Paragon, 90750 the Stormbound Conduit), and one marker per picked modifier (90710 / 90740 / 90760 +
//   slot * 3 + choice); scripts look for them.
//
// Protocol (fields by ','; the client sends STATE, PICK,<rune>[,1], MOD,<slot 1-3>,<choice 1-3>; PICK's 1: warned
// of the abilities the rune needs and lacks, the Hero binds it anyway):
//   S,<level>,<unlock level>,<spec>,<rune>,<m1>,<m2>,<m3>,<offers a.b.c or ->,<prompt 0/1>
//   R,<rune>,<missing>      an implemented rune and the abilities it lacks (groups by '/', one of by '+'; '-' none)
//   N,<rune>,<slot>,<choice>,<missing>      a modifier with needs of its own
//   E,<text>   O (open the panel)
namespace
{
    constexpr char const* PREFIX = "EFRN";
    constexpr uint8 UNLOCK_LEVEL = 10;
    constexpr uint8 MAX_SPECS = 4;
    constexpr uint8 SLOTS = 3;
    constexpr uint8 CHOICES = 3;
    constexpr uint8 MAX_OFFERS = 3;

    constexpr uint8 RUNE_FEL_COMMANDMENT = 21;      // the doc's order: the 20 class runes, then the hybrid
    constexpr uint8 RUNE_SANCTIFIED_PARAGON = 22;   // its own doc
    constexpr uint8 RUNE_STORMBOUND_CONDUIT = 23;   // its own doc

    enum Spells : uint32
    {
        SPELL_RUNE_FEL_COMMANDMENT  = 90700,
        SPELL_FEL_COMMAND           = 90701,
        SPELL_VOLLEY_BOLT           = 90702,
        SPELL_FELGUARD_GUARDIAN     = 90703,
        SPELL_FEL_IMP_GUARDIAN      = 90704,
        SPELL_COMMANDERS_FURY       = 90705,
        SPELL_METAMORPHIC_BOND      = 90706,
        SPELL_FEL_RIFT              = 90707,
        SPELL_FELSTORM              = 90708,
        SPELL_METAMORPHIC_CLEAVE    = 90709,
        SPELL_MODIFIER_FIRST        = 90710,        // + slot * 3 + choice

        SPELL_RUNE_SANCTIFIED_PARAGON   = 90730,
        SPELL_SANCTIFIED_PARAGON        = 90731,    // the aura (Consecration turned on)
        SPELL_PARAGON_TICK              = 90732,
        SPELL_CONDEMNATION              = 90733,
        SPELL_MERCIFUL_GROUND           = 90734,
        SPELL_BASTION_OF_FAITH          = 90735,
        SPELL_WEIGHT_OF_THE_UNWORTHY    = 90736,
        SPELL_SHELTER_OF_THE_PARAGON    = 90737,
        SPELL_SEAT_OF_THE_PARAGON       = 90738,
        SPELL_HALLOWED_FOOTSTEPS        = 90739,
        SPELL_PARAGON_MODIFIER_FIRST    = 90740,    // + slot * 3 + choice
        SPELL_PARAGON_AWAKEN            = 90749,    // its look as it turns on (a shockwave and a flash)

        SPELL_RUNE_STORMBOUND_CONDUIT   = 90750,
        SPELL_CONDUCTIVE                = 90751,
        SPELL_STORMBOUND_ARC            = 90752,    // a secondary arc's damage
        SPELL_STORMBOUND_ARC_VISUAL     = 90753,    // its bolt, cast by the enemy it leaves
        SPELL_STORMBOUND_RETALIATION    = 90754,    // a Lightning Shield retaliation the rune makes
        SPELL_STORM_MODIFIER_FIRST      = 90760,    // + slot * 3 + choice

        SPELL_CONSECRATION          = 26573,        // first rank
        SPELL_LIGHTNING_BOLT        = 403,          // first ranks
        SPELL_CHAIN_LIGHTNING       = 421,
        SPELL_LIGHTNING_SHIELD      = 324,
        SPELL_LIGHTNING_SHIELD_DAMAGE = 26364,

        SPELL_DEMONS_BITE           = 90100,
        SPELL_CHAOS_STRIKE          = 90101,
        SPELL_THROW_GLAIVE          = 90111,
        SPELL_METAMORPHOSIS_DH      = 90116,
        SPELL_METAMORPHOSIS_WARLOCK = 47241,
        SPELL_SHADOW_BOLT           = 686,
        SPELL_INCINERATE            = 29722,
        SPELL_SUMMON_IMP            = 688,
        SPELL_SUMMON_VOIDWALKER     = 697,
        SPELL_SUMMON_SUCCUBUS       = 712,
        SPELL_SUMMON_FELHUNTER      = 691,
        SPELL_SUMMON_FELGUARD       = 30146,
        SPELL_FIREBOLT              = 3110,
        SPELL_FELGUARD_CLEAVE       = 30213,
    };

    // Fel Commandment's modifiers: slot (0 Invocation, 1 Manifestation, 2 Adaptation) x 3 + choice.
    enum Modifier : uint8
    {
        MOD_INFERNAL_PACT, MOD_DEMONIC_ASCENSION, MOD_DREADFIRE_DISCIPLINE,
        MOD_IMP_LEGION, MOD_SHATTERED_GLAIVE, MOD_FEL_HOST,
        MOD_METAMORPHIC_BOND, MOD_FEL_RIFT, MOD_PACT_OF_RUIN,
    };

    // The Sanctified Paragon's modifiers: slot (0 Presence, 1 Manifestation, 2 Covenant) x 3 + choice.
    enum ParagonModifier : uint8
    {
        MOD_BOUNDLESS_SANCTITY, MOD_HALLOWED_FOOTSTEPS, MOD_SEAT_OF_THE_PARAGON,
        MOD_SANCTIFIED_CONDEMNATION, MOD_MERCIFUL_GROUND, MOD_SHELTER_OF_THE_PARAGON,
        MOD_BASTION_OF_FAITH, MOD_WEIGHT_OF_THE_UNWORTHY, MOD_MEASURED_DEVOTION,
    };

    // The Stormbound Conduit's modifiers: slot (0 Conductivity, 1 Arc Pattern, 2 Lightning Shield) x 3 + choice.
    enum StormModifier : uint8
    {
        MOD_PERSISTENT_CURRENT, MOD_BRANCHING_CURRENT, MOD_LIVE_WIRE,
        MOD_FORKED_TEMPEST, MOD_CLOSED_CIRCUIT, MOD_FARADAY_FIELD,
        MOD_STORM_WITHIN, MOD_VIOLENT_REPRISAL, MOD_DISTANT_CONDUCTOR,
    };

    // The Stormbound Conduit's tuning (the draft's initial values; the arc range, Storm Within's pulse and the
    // Lightning Bolt reference are ours to tune).
    constexpr int32 CONDUCTIVE_MS = 6000, PERSISTENT_CONDUCTIVE_MS = 12000;
    constexpr float ARC_RANGE = 8.0f;                           // a secondary arc's reach from the enemy it leaves
    constexpr float BRANCHING_RANGE = 8.0f, BRANCHING_ARCS = 0.8f;
    constexpr float CHAIN_ARC_SHARE = 0.15f;                    // of Lightning Bolt's damage
    constexpr float SHIELD_ARC_SHARE = 0.25f;                   // of the retaliation's damage
    constexpr std::size_t FORKED_TARGETS = 2;
    constexpr float CLOSED_CIRCUIT_SHARE = 0.5f;
    constexpr float FARADAY_REACH = 1.5f, FARADAY_ARCS = 0.85f;
    constexpr uint32 STORM_WITHIN_MS = 3000, STORM_CHECK_MS = 250;
    constexpr float STORM_WITHIN_RANGE = 8.0f;
    constexpr float VIOLENT_ARCS = 1.4f, VIOLENT_RETALIATION = 0.9f;
    constexpr uint32 LIGHTNING_SHIELD_FAMILY_FLAG = 0x400;      // SpellFamilyFlags0

    // The Sanctified Paragon's tuning (the revised balance draft's initial values).
    constexpr float PARAGON_SHARE = 0.4f;                       // of a Consecration tick, a second
    constexpr float UPKEEP_PCT = 1.0f;                          // of base mana, a second
    constexpr float PARAGON_RADIUS = 8.0f, BOUNDLESS_RADIUS = 12.0f, BOUNDLESS_OUTPUT = 0.75f;
    constexpr float CONSECRATION_COEFFICIENT = 0.04f;           // spell power and attack power, a tick
    constexpr float FOOTSTEPS_RADIUS = 4.0f;                    // the aura's own with Hallowed Footsteps
    constexpr float TRAIL_RADIUS = 4.0f, TRAIL_SPACING = 3.0f;  // a footstep's patch; one at most every 3 yards
    constexpr uint32 TRAIL_MS = 2000;
    constexpr float MOVED = 0.3f;                               // yards between ticks that count as moving
    constexpr uint32 SEAT_MS = 3000;
    constexpr float SEAT_OUTPUT = 1.1f;
    constexpr uint32 CONDEMNATION_PCT = 2, CONDEMNATION_STACKS = 5;
    constexpr float MERCIFUL_SHARE = 1.0f;                      // of the aura's tick, healed (to tune)
    constexpr uint32 SHELTER_MS = 5000;
    constexpr float SHELTER_TICKS = 5.0f;                       // a shield of five ticks (to tune)
    constexpr float BASTION_THREAT = 0.5f;
    constexpr uint8 WEIGHT_TICKS = 3;
    constexpr float DEVOTION_UPKEEP = 0.75f, DEVOTION_OUTPUT = 0.9f;

    constexpr uint32 NPC_RUNE_FELGUARD = 910120;
    constexpr uint32 NPC_RUNE_FEL_IMP = 910121;
    // SummonProperties 61: category ally, type guardian. A pet-category row (67) would make a controllable guardian
    // that takes the pet slot from the Hero's own demon.
    constexpr uint32 SUMMON_PROPERTIES_GUARDIAN = 61;
    // The warlock's demons (creature entries of their summons), whose hits feed the rune.
    constexpr std::array<uint32, 5> DEMONS = { 416, 1860, 1863, 417, 17252 };

    // Tuning (the doc's table).
    constexpr uint8 MAX_STACKS = 5;
    constexpr uint32 DEMON_STACK_INTERVAL_MS = 4000;
    constexpr uint32 INFERNAL_PACT_INTERVAL_MS = 2000;
    constexpr uint32 BOLTS = 3, BOLTS_EMPOWERED = 5, PACT_OF_RUIN_BOLTS = 2;
    constexpr Milliseconds BOLT_SPACING = 150ms;
    constexpr float BOLT_RANGE = 10.0f;             // the volley's other targets, around the glaive's
    constexpr float BOLT_SHARE = 0.6f;              // of a Chaos Bolt
    constexpr float CHAOS_BOLT_COEFFICIENT = 0.7143f;
    constexpr float CHAOS_BOLT_RANK1_AVERAGE = 949.0f;      // 50796: 837 to 1061, level 60
    constexpr uint8 CHAOS_BOLT_RANK1_LEVEL = 60;
    constexpr uint32 FELGUARD_DURATION_MS = 8000;
    constexpr uint32 FEL_IMP_DURATION_MS = 10000;
    // Imp Legion and Fel Host: every Throw Glaive spends the Fel Command, a demon (10 sec) for each stack, no limit
    // to how many are out; each bolt of a volley deals 10% more for every one of the Hero's demons out.
    constexpr uint32 LEGION_DURATION_MS = 10000;
    constexpr uint32 BOLT_PCT_PER_DEMON = 10;
    constexpr int32 SHATTER_CUT_MS = 3000, SHATTER_BUDGET_MS = 9000;
    constexpr uint32 PACT_OF_RUIN_PCT = 3;
    constexpr float BOND_CLEAVE_RANGE = 5.0f;
    constexpr uint32 BOND_CLEAVE_TARGETS = 2;
    constexpr uint32 BOND_CHECK_MS = 500;

    // A rune: what it needs to be offered (each group: one of its spells, by first rank) and the roles it is
    // suggested to.
    struct RuneDef
    {
        uint8 Id;
        uint8 Roles;
        std::vector<std::vector<uint32>> Requires;
        uint32 Aura;                // worn while it is the active spec's rune
        uint32 MarkerFirst;         // its modifiers' markers: + slot * 3 + choice
    };

    std::vector<RuneDef> const& Catalog()
    {
        static std::vector<RuneDef> const runes = {
            { RUNE_FEL_COMMANDMENT,
              uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE) | Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              // Throw Glaive (it spends the stacks) and any one generator.
              { { SPELL_THROW_GLAIVE },
                { SPELL_DEMONS_BITE, SPELL_CHAOS_STRIKE, SPELL_SHADOW_BOLT, SPELL_INCINERATE } },
              SPELL_RUNE_FEL_COMMANDMENT, SPELL_MODIFIER_FIRST },
            // Every role: a damage aura, a defensive zone or a source of healing, as its modifiers make it.
            { RUNE_SANCTIFIED_PARAGON,
              uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE) | Wildcard::RoleMask(Wildcard::ROLE_RANGED)
                  | Wildcard::RoleMask(Wildcard::ROLE_HEALER) | Wildcard::RoleMask(Wildcard::ROLE_TANK)),
              { { SPELL_CONSECRATION } }, SPELL_RUNE_SANCTIFIED_PARAGON, SPELL_PARAGON_MODIFIER_FIRST },
            // DPS only: Lightning Bolt makes the connections, Lightning Shield's retaliations (and Chain Lightning,
            // if known) arc through them.
            { RUNE_STORMBOUND_CONDUIT,
              uint8(Wildcard::RoleMask(Wildcard::ROLE_MELEE) | Wildcard::RoleMask(Wildcard::ROLE_RANGED)),
              { { SPELL_LIGHTNING_BOLT }, { SPELL_LIGHTNING_SHIELD } }, SPELL_RUNE_STORMBOUND_CONDUIT,
              SPELL_STORM_MODIFIER_FIRST },
        };
        return runes;
    }

    RuneDef const* FindRune(uint8 id)
    {
        for (RuneDef const& rune : Catalog())
            if (rune.Id == id)
                return &rune;
        return nullptr;
    }

    // A modifier's own needs (Dreadfire Discipline: Shadow Bolt or Incinerate; Infernal Pact and Metamorphic Bond:
    // a demon to summon; Closed Circuit: Chain Lightning).
    std::vector<std::vector<uint32>> ModifierNeeds(uint8 rune, uint8 slot, uint8 choice)
    {
        uint8 modifier = slot * CHOICES + choice;
        if (rune == RUNE_STORMBOUND_CONDUIT && modifier == MOD_CLOSED_CIRCUIT)
            return { { SPELL_CHAIN_LIGHTNING } };
        if (rune != RUNE_FEL_COMMANDMENT)
            return {};
        if (modifier == MOD_DREADFIRE_DISCIPLINE)
            return { { SPELL_SHADOW_BOLT, SPELL_INCINERATE } };
        if (modifier == MOD_INFERNAL_PACT || modifier == MOD_METAMORPHIC_BOND)
            return { { SPELL_SUMMON_IMP, SPELL_SUMMON_VOIDWALKER, SPELL_SUMMON_SUCCUBUS, SPELL_SUMMON_FELHUNTER,
                       SPELL_SUMMON_FELGUARD } };
        return {};
    }

    struct Choice
    {
        uint8 Rune = 0;
        std::array<uint8, SLOTS> Mods = {};         // 1 to 3; 0 none yet
    };

    struct State
    {
        std::array<Choice, MAX_SPECS> Choices;
        uint32 LastDemonStack = 0;                  // game ms
        uint32 BondTimer = 0;
        int32 ShatterBudget = 0;
        ObjectGuid Felguard;
        std::vector<ObjectGuid> Imps;
        ObjectGuid RiftTarget;

        // The Sanctified Paragon.
        Position LastPosition;
        bool HasLastPosition = false;
        uint32 StillSince = 0;                      // game ms
        uint32 LastShelter = 0;
        ObjectGuid Sheltered;                                       // the one ally holding its shield
        std::vector<std::pair<Position, uint32>> Trail;             // footsteps: where, when
        std::unordered_map<ObjectGuid, uint8> Weighed;              // enemy -> damaging ticks in a row

        // The Stormbound Conduit.
        uint32 StormTimer = 0;                      // ms since Storm Within's last pulse
        bool Pulsing = false;                       // a Storm Within pulse is landing: it never arcs
    };

    std::unordered_map<ObjectGuid::LowType, State> states;

    State& StateOf(Player* player)
    {
        return states[player->GetGUID().GetCounter()];
    }

    Choice& ActiveChoice(Player* player)
    {
        return StateOf(player).Choices[Specs::ActiveSpec(player)];
    }

    // The spell or any of its ranks.
    bool Knows(Player* player, uint32 firstRank)
    {
        for (uint32 id = firstRank; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (player->HasSpell(id))
                return true;
        return false;
    }

    // The groups the player meets none of, as the protocol writes them ('-' when none).
    std::string Missing(Player* player, std::vector<std::vector<uint32>> const& groups)
    {
        std::string out;
        for (std::vector<uint32> const& group : groups)
        {
            if (std::any_of(group.begin(), group.end(), [player](uint32 id) { return Knows(player, id); }))
                continue;
            std::string ids;
            for (uint32 id : group)
                ids += (ids.empty() ? "" : "+") + std::to_string(id);
            out += (out.empty() ? "" : "/") + ids;
        }
        return out.empty() ? "-" : out;
    }

    bool HasMarker(Unit* unit, uint8 modifier)
    {
        return unit && unit->HasAura(SPELL_MODIFIER_FIRST + modifier);
    }

    bool HasParagonMarker(Unit* unit, uint8 modifier)
    {
        return unit && unit->HasAura(SPELL_PARAGON_MODIFIER_FIRST + modifier);
    }

    bool InMetamorphosis(Unit* unit)
    {
        return unit->HasAura(SPELL_METAMORPHOSIS_DH) || unit->HasAura(SPELL_METAMORPHOSIS_WARLOCK);
    }

    void Send(Player* player, std::string const& payload)
    {
        WorldPacket data;
        std::string const message = std::string(PREFIX) + '\t' + payload;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, message);
        player->SendDirectMessage(&data);
    }

    // ---- Fel Commandment ------------------------------------------------------------------------------------

    uint8 Stacks(Player* player)
    {
        Aura* aura = player->GetAura(SPELL_FEL_COMMAND);
        return aura ? aura->GetStackAmount() : 0;
    }

    // Fel Command gained: up to 5; every new stack restarts the 20 sec, stacks past 5 are wasted.
    void Gain(Player* player, uint8 count)
    {
        if (!count || !player->HasAura(SPELL_RUNE_FEL_COMMANDMENT))
            return;
        uint8 have = Stacks(player);
        if (have >= MAX_STACKS)
            return;
        Aura* aura = player->GetAura(SPELL_FEL_COMMAND);
        if (!aura)
            aura = player->AddAura(SPELL_FEL_COMMAND, player);
        if (!aura)
            return;
        aura->SetStackAmount(std::min<uint8>(MAX_STACKS, have + count));
        aura->RefreshDuration();
    }

    // A Chaos Bolt of the Hero's level (rank 1's, scaled by a creature's damage at that level below 60) x 0.6, plus
    // the higher of spell power and half the attack power x Chaos Bolt's coefficient x 0.6.
    int32 BoltDamage(Player* player)
    {
        uint8 level = std::min<uint8>(player->GetLevel(), CHAOS_BOLT_RANK1_LEVEL);
        float scale = 1.0f;
        CreatureBaseStats const* there = sObjectMgr->GetCreatureBaseStats(level, CLASS_WARRIOR);
        CreatureBaseStats const* top = sObjectMgr->GetCreatureBaseStats(CHAOS_BOLT_RANK1_LEVEL, CLASS_WARRIOR);
        if (there && top && top->BaseDamage[0] > 0.0f)
            scale = there->BaseDamage[0] / top->BaseDamage[0];
        float power = std::max(float(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE)),
            player->GetTotalAttackPowerValue(BASE_ATTACK) / 2.0f);
        float damage = (CHAOS_BOLT_RANK1_AVERAGE * scale + power * CHAOS_BOLT_COEFFICIENT) * BOLT_SHARE;
        return std::max<int32>(1, int32(damage));
    }

    // Enemies within range of a point the player may hit: in sight, attackable, not crowd-controlled, nearest first.
    std::vector<Unit*> EnemiesAround(Player* player, WorldObject* center, float range)
    {
        std::list<Unit*> found;
        Acore::AnyUnfriendlyUnitInObjectRangeCheck check(center, player, range);
        Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(center, found, check);
        Cell::VisitObjects(center, searcher, range);
        std::vector<Unit*> enemies;
        for (Unit* unit : found)
            if (unit->IsAlive() && player->IsValidAttackTarget(unit) && !unit->HasBreakableByDamageCrowdControlAura()
                && player->IsWithinLOSInMap(unit))
                enemies.push_back(unit);
        std::sort(enemies.begin(), enemies.end(), [center](Unit* a, Unit* b)
        {
            return center->GetExactDist(a) < center->GetExactDist(b);
        });
        return enemies;
    }

    Creature* Felguard(Player* player)
    {
        State& state = StateOf(player);
        Creature* felguard = state.Felguard ? ObjectAccessor::GetCreature(*player, state.Felguard) : nullptr;
        return felguard && felguard->IsAlive() ? felguard : nullptr;
    }

    // The player's warlock demon (its hits feed the rune), if out.
    Pet* Demon(Player* player)
    {
        Pet* pet = player->GetPet();
        return pet && pet->IsAlive() && std::find(DEMONS.begin(), DEMONS.end(), pet->GetEntry()) != DEMONS.end()
            ? pet : nullptr;
    }

    void SummonFelguard(Player* player, Unit* target)
    {
        State& state = StateOf(player);
        if (Creature* felguard = Felguard(player))
        {
            if (TempSummon* summon = felguard->ToTempSummon())
                summon->SetTimer(FELGUARD_DURATION_MS);         // one at a time: the next resets its 8 sec
            return;
        }
        Position pos = player->GetNearPosition(2.0f, float(M_PI));
        TempSummon* felguard = player->GetMap()->SummonCreature(NPC_RUNE_FELGUARD, pos,
            sSummonPropertiesStore.LookupEntry(SUMMON_PROPERTIES_GUARDIAN), FELGUARD_DURATION_MS, player);
        if (!felguard)
            return;
        state.Felguard = felguard->GetGUID();
        if (felguard->IsAIEnabled && target)
            felguard->AI()->AttackStart(target);
    }

    bool Legion(Player* player)
    {
        return HasMarker(player, MOD_IMP_LEGION) || HasMarker(player, MOD_FEL_HOST);
    }

    // The legion's demons still out (Imp Legion's imps, Fel Host's Felguards), the dead ones dropped.
    std::size_t LegionOut(Player* player)
    {
        State& state = StateOf(player);
        state.Imps.erase(std::remove_if(state.Imps.begin(), state.Imps.end(), [player](ObjectGuid guid)
        {
            Creature* demon = ObjectAccessor::GetCreature(*player, guid);
            return !demon || !demon->IsAlive();
        }), state.Imps.end());
        return state.Imps.size();
    }

    // One of the legion's demons by the target (an imp; a Felguard with felguard), for LEGION_DURATION_MS.
    void SummonDemon(Player* player, Unit* target, bool felguard)
    {
        State& state = StateOf(player);
        Position pos = target->GetNearPosition(frand(3.0f, 6.0f), target->GetAngle(player) + frand(-1.2f, 1.2f));
        TempSummon* demon = player->GetMap()->SummonCreature(felguard ? NPC_RUNE_FELGUARD : NPC_RUNE_FEL_IMP, pos,
            sSummonPropertiesStore.LookupEntry(SUMMON_PROPERTIES_GUARDIAN), LEGION_DURATION_MS, player);
        if (!demon)
            return;
        state.Imps.push_back(demon->GetGUID());
        if (demon->IsAIEnabled)
            demon->AI()->AttackStart(target);
    }

    // Every demon of the Hero's out: the legion's, the volley's Felguard and their own warlock demon.
    std::size_t DemonsOut(Player* player)
    {
        return LegionOut(player) + (Felguard(player) ? 1 : 0) + (Demon(player) ? 1 : 0);
    }

    // Stacks, the Felguard and the imps gone (death, prestige, a spec or modifier change).
    void ClearCombat(Player* player)
    {
        player->RemoveAurasDueToSpell(SPELL_FEL_COMMAND);
        player->RemoveAurasDueToSpell(SPELL_FEL_RIFT);
        State& state = StateOf(player);
        std::vector<ObjectGuid> summons = state.Imps;
        summons.push_back(state.Felguard);
        for (ObjectGuid guid : summons)
            if (Creature* creature = guid ? ObjectAccessor::GetCreature(*player, guid) : nullptr)
                if (TempSummon* summon = creature->ToTempSummon())
                    summon->UnSummon();
        state.Imps.clear();
        state.Felguard.Clear();
        state.RiftTarget.Clear();
        for (uint32 spell : { SPELL_SANCTIFIED_PARAGON, SPELL_SEAT_OF_THE_PARAGON, SPELL_BASTION_OF_FAITH })
            player->RemoveAurasDueToSpell(spell);
        state.Trail.clear();
        state.Weighed.clear();
        state.HasLastPosition = false;
        if (Pet* demon = player->GetPet())
        {
            demon->RemoveAurasDueToSpell(SPELL_METAMORPHIC_BOND);
            demon->RemoveAurasDueToSpell(SPELL_COMMANDERS_FURY);
        }
    }

    struct Volley
    {
        ObjectGuid Primary;
        std::vector<ObjectGuid> Targets;            // one per bolt, in order
        std::vector<Position> LastSeen;
        std::set<ObjectGuid> Hit;
        bool Stopped = false;
    };

    void FireBolt(Player* player, std::shared_ptr<Volley> volley, std::size_t index)
    {
        if (volley->Stopped || !player->IsInWorld() || !player->IsAlive())
            return;
        Unit* target = ObjectAccessor::GetUnit(*player, volley->Targets[index]);
        if (!target || !target->IsAlive() || !player->IsValidAttackTarget(target))
        {
            // Its target died: the nearest other enemy within 10 yards of where it was, else the bolt fizzles.
            Position const& where = volley->LastSeen[index];
            target = nullptr;
            float best = BOLT_RANGE;
            for (Unit* enemy : EnemiesAround(player, player, player->GetExactDist(&where) + BOLT_RANGE))
                if (float dist = enemy->GetExactDist(&where); dist <= best)
                {
                    best = dist;
                    target = enemy;
                }
            if (!target)
                return;
        }

        if (HasMarker(player, MOD_PACT_OF_RUIN))
        {
            uint32 cost = player->CountPctFromMaxHealth(PACT_OF_RUIN_PCT);
            if (player->GetHealth() <= cost)
            {
                volley->Stopped = true;             // never kills: this bolt and the rest are not fired
                return;
            }
            player->ModifyHealth(-int32(cost));
        }

        int32 damage = BoltDamage(player);
        if (Legion(player))
            damage += damage * int32(BOLT_PCT_PER_DEMON * DemonsOut(player)) / 100;
        player->CastCustomSpell(target, SPELL_VOLLEY_BOLT, &damage, nullptr, nullptr, true);

        if (!volley->Hit.insert(target->GetGUID()).second)
            return;
        if (HasMarker(player, MOD_SHATTERED_GLAIVE) && target->GetGUID() != volley->Primary)
            player->CastSpell(target, SPELL_THROW_GLAIVE, true);    // the glaive bounces to it, full damage
    }

    // Chaos Bolt Volley: at 5 Fel Command, Throw Glaive also fires 3 bolts (5 in Metamorphosis, then the Felguard;
    // Pact of Ruin 2 more), 0.15 sec apart: the first at the glaive's target, each next at another enemy within
    // 10 yards of it, nearest first; extra bolts go to the target.
    void FireVolley(Player* player, Unit* primary)
    {
        player->RemoveAurasDueToSpell(SPELL_FEL_COMMAND);
        bool empowered = InMetamorphosis(player);
        uint32 bolts = (empowered ? BOLTS_EMPOWERED : BOLTS)
            + (HasMarker(player, MOD_PACT_OF_RUIN) ? PACT_OF_RUIN_BOLTS : 0);

        auto volley = std::make_shared<Volley>();
        volley->Primary = primary->GetGUID();
        std::vector<Unit*> others = EnemiesAround(player, primary, BOLT_RANGE);
        others.erase(std::remove(others.begin(), others.end(), primary), others.end());
        for (uint32 i = 0; i < bolts; ++i)
        {
            Unit* target = (i > 0 && i - 1 < others.size()) ? others[i - 1] : primary;
            volley->Targets.push_back(target->GetGUID());
            volley->LastSeen.push_back(target->GetPosition());
        }

        State& state = StateOf(player);
        state.ShatterBudget = SHATTER_BUDGET_MS;
        for (std::size_t i = 0; i < volley->Targets.size(); ++i)
            player->m_Events.AddEventAtOffset([player, volley, i]() { FireBolt(player, volley, i); },
                Milliseconds(BOLT_SPACING.count() * i));

        if (empowered)
            SummonFelguard(player, primary);
        if (HasMarker(player, MOD_FEL_RIFT))
        {
            state.RiftTarget = primary->GetGUID();
            player->AddAura(SPELL_FEL_RIFT, player);
        }
    }

    // ---- The Sanctified Paragon -------------------------------------------------------------------------------

    // The Hero's best Consecration rank they know, else 0.
    uint32 ConsecrationRank(Player* player)
    {
        uint32 best = 0;
        for (uint32 id = SPELL_CONSECRATION; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (player->HasSpell(id))
                best = id;
        return best;
    }

    // A tick of the Hero's Consecration before the target's mitigation: their rank's tick and 4% of their spell
    // power and attack power (as Consecration's). The damage, heal and shield spells add no power of their own
    // (spell_bonus_data 0): the modifiers scale all of it.
    float ParagonTick(Player* player)
    {
        SpellInfo const* rank = sSpellMgr->GetSpellInfo(ConsecrationRank(player));
        int32 points = rank ? std::max(1, rank->Effects[EFFECT_0].CalcValue(player)) : 1;
        float power = float(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_HOLY))
            + player->GetTotalAttackPowerValue(BASE_ATTACK);
        return float(points) + power * CONSECRATION_COEFFICIENT;
    }

    // Allies of the Hero's within range (themselves first, then their group's, nearest first), alive.
    std::vector<Unit*> AlliesAround(Player* player, float range)
    {
        std::vector<Unit*> allies = { player };
        if (Group* group = player->GetGroup())
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                if (Player* member = ref->GetSource())
                    if (member != player && member->IsInMap(player) && member->IsAlive()
                        && member->IsWithinDistInMap(player, range))
                        allies.push_back(member);
        std::sort(allies.begin() + 1, allies.end(), [player](Unit* a, Unit* b)
        {
            return player->GetExactDist(a) < player->GetExactDist(b);
        });
        return allies;
    }

    // The party members within range (the Hero first), alive, most injured first (by share of health).
    std::vector<Unit*> MostInjured(Player* player, float range)
    {
        std::vector<Unit*> allies = AlliesAround(player, range);
        std::stable_sort(allies.begin(), allies.end(), [](Unit* a, Unit* b)
        {
            return a->GetHealthPct() < b->GetHealthPct();
        });
        return allies;
    }

    // A second of the aura (90731's tick), the revised balance draft's: its upkeep (1% of base mana, 0.75% with
    // Measured Devotion; the aura ends when it cannot be paid), then one pulse at 40% of a Consecration tick, through
    // the Manifestation picked: damage to each enemy within 8 yards (12 with Boundless Sanctity, 4 and the
    // footsteps' patches with Hallowed Footsteps; an enemy once a pulse however many it stands in), or a heal on the
    // single most injured party member (Merciful Ground), or a shield every 5 sec on one (Shelter of the Paragon).
    // Nothing a modifier makes sets off another; toggling neither pulses at once nor resets their timers.
    // false: the upkeep could not be paid (the aura's script ends it).
    bool SanctifiedGround(Player* player)
    {
        if (!player->IsAlive() || !ConsecrationRank(player))
            return true;
        State& state = StateOf(player);
        uint32 now = GameTime::GetGameTimeMS().count();

        bool devotion = HasParagonMarker(player, MOD_MEASURED_DEVOTION);
        int32 upkeep = std::max<int32>(1, int32(player->GetCreateMana() * UPKEEP_PCT / 100.0f
            * (devotion ? DEVOTION_UPKEEP : 1.0f)));
        if (player->GetPower(POWER_MANA) < upkeep)
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cffa335eeSanctified Paragon:|r out of mana.");
            return false;
        }
        player->ModifyPower(POWER_MANA, -upkeep);

        bool boundless = HasParagonMarker(player, MOD_BOUNDLESS_SANCTITY);
        bool footsteps = HasParagonMarker(player, MOD_HALLOWED_FOOTSTEPS);
        float radius = boundless ? BOUNDLESS_RADIUS : footsteps ? FOOTSTEPS_RADIUS : PARAGON_RADIUS;

        // Moving or still (Seat of the Paragon, Hallowed Footsteps).
        Position here = player->GetPosition();
        bool moved = state.HasLastPosition && state.LastPosition.GetExactDist(&here) > MOVED;
        if (!state.HasLastPosition || moved)
            state.StillSince = now;
        if (moved && footsteps
            && (state.Trail.empty() || state.Trail.back().first.GetExactDist(&state.LastPosition) >= TRAIL_SPACING))
        {
            state.Trail.emplace_back(state.LastPosition, now);
            player->CastSpell(state.LastPosition.GetPositionX(), state.LastPosition.GetPositionY(),
                state.LastPosition.GetPositionZ(), SPELL_HALLOWED_FOOTSTEPS, true);
        }
        state.LastPosition = here;
        state.HasLastPosition = true;
        state.Trail.erase(std::remove_if(state.Trail.begin(), state.Trail.end(),
            [now](auto const& step) { return now - step.second >= TRAIL_MS; }), state.Trail.end());

        bool seated = HasParagonMarker(player, MOD_SEAT_OF_THE_PARAGON) && now - state.StillSince >= SEAT_MS;
        if (seated && !player->HasAura(SPELL_SEAT_OF_THE_PARAGON))
            player->AddAura(SPELL_SEAT_OF_THE_PARAGON, player);
        else if (!seated)
            player->RemoveAurasDueToSpell(SPELL_SEAT_OF_THE_PARAGON);

        float output = ParagonTick(player) * PARAGON_SHARE * (boundless ? BOUNDLESS_OUTPUT : 1.0f)
            * (seated ? SEAT_OUTPUT : 1.0f) * (devotion ? DEVOTION_OUTPUT : 1.0f);

        // Merciful Ground: no damage; the single most injured party member within range healed.
        if (HasParagonMarker(player, MOD_MERCIFUL_GROUND))
        {
            for (Unit* ally : MostInjured(player, radius))
                if (!ally->IsFullHealth())
                {
                    int32 heal = std::max<int32>(1, int32(output * MERCIFUL_SHARE));
                    player->CastCustomSpell(ally, SPELL_MERCIFUL_GROUND, &heal, nullptr, nullptr, true);
                    break;
                }
            return true;
        }

        // Shelter of the Paragon: no damage; every 5 sec one party member within range (the most injured) shielded,
        // one shield of the aura's at a time (the last one's goes), none on an ally another Paragon shields.
        if (HasParagonMarker(player, MOD_SHELTER_OF_THE_PARAGON))
        {
            if (now - state.LastShelter < SHELTER_MS)
                return true;
            for (Unit* ally : MostInjured(player, radius))
            {
                if (ally->HasAura(SPELL_SHELTER_OF_THE_PARAGON) && ally->GetGUID() != state.Sheltered)
                    continue;
                if (Unit* last = state.Sheltered ? ObjectAccessor::GetUnit(*player, state.Sheltered) : nullptr)
                    last->RemoveAurasDueToSpell(SPELL_SHELTER_OF_THE_PARAGON, player->GetGUID());
                int32 shield = std::max<int32>(1, int32(output * SHELTER_TICKS));
                player->CastCustomSpell(ally, SPELL_SHELTER_OF_THE_PARAGON, &shield, nullptr, nullptr, true);
                state.Sheltered = ally->GetGUID();
                state.LastShelter = now;
                break;
            }
            return true;
        }

        // Damage (Sanctified Condemnation, or no Manifestation yet): the enemies sanctified this second, the aura's
        // then the footsteps' (each once).
        std::vector<Unit*> enemies = EnemiesAround(player, player, radius);
        if (footsteps)
            for (auto const& step : state.Trail)
                for (Unit* enemy : EnemiesAround(player, player, player->GetExactDist(&step.first) + TRAIL_RADIUS))
                    if (enemy->GetExactDist(&step.first) <= TRAIL_RADIUS
                        && std::find(enemies.begin(), enemies.end(), enemy) == enemies.end())
                        enemies.push_back(enemy);

        bool condemnation = HasParagonMarker(player, MOD_SANCTIFIED_CONDEMNATION);
        bool weight = condemnation && HasParagonMarker(player, MOD_WEIGHT_OF_THE_UNWORTHY);
        bool bastion = condemnation && HasParagonMarker(player, MOD_BASTION_OF_FAITH);
        std::unordered_map<ObjectGuid, uint8> weighed;
        for (Unit* enemy : enemies)
        {
            float amount = output;
            if (condemnation)
                if (Aura* condemned = enemy->GetAura(SPELL_CONDEMNATION, player->GetGUID()))
                    amount *= 1.0f + CONDEMNATION_PCT * condemned->GetStackAmount() / 100.0f;
            int32 damage = std::max<int32>(1, int32(amount));
            player->CastCustomSpell(enemy, SPELL_PARAGON_TICK, &damage, nullptr, nullptr, true);
            if (!enemy->IsAlive())
                continue;
            if (condemnation)
            {
                Aura* condemned = enemy->GetAura(SPELL_CONDEMNATION, player->GetGUID());
                if (!condemned)
                    condemned = player->AddAura(SPELL_CONDEMNATION, enemy);
                else if (condemned->GetStackAmount() < CONDEMNATION_STACKS)
                    condemned->ModStackAmount(1);
                if (condemned)
                    condemned->RefreshDuration();
            }
            // Weight of the Unworthy: slowed from the third damaging tick in a row, while it stays in the aura.
            if (weight)
            {
                auto itr = state.Weighed.find(enemy->GetGUID());
                uint8 ticks = uint8(std::min<uint32>((itr != state.Weighed.end() ? itr->second : 0) + 1, 255));
                weighed[enemy->GetGUID()] = ticks;
                if (ticks >= WEIGHT_TICKS)
                    player->CastSpell(enemy, SPELL_WEIGHT_OF_THE_UNWORTHY, true);
            }
            if (bastion && enemy->CanHaveThreatList())
                enemy->AddThreat(player, float(damage) * BASTION_THREAT, SPELL_SCHOOL_MASK_HOLY);
        }
        // Those not struck this second lose the count and the slow.
        for (auto const& [guid, ticks] : state.Weighed)
            if (!weighed.count(guid))
                if (Unit* enemy = ObjectAccessor::GetUnit(*player, guid))
                    enemy->RemoveAurasDueToSpell(SPELL_WEIGHT_OF_THE_UNWORTHY, player->GetGUID());
        state.Weighed = std::move(weighed);
        return true;
    }

    // ---- The Stormbound Conduit -------------------------------------------------------------------------------

    bool HasStormMarker(Unit* unit, uint8 modifier)
    {
        return unit && unit->HasAura(SPELL_STORM_MODIFIER_FIRST + modifier);
    }

    bool IsConductive(Player* player, Unit* unit)
    {
        return unit->HasAura(SPELL_CONDUCTIVE, player->GetGUID());
    }

    // Conductive on the enemy: 6 sec (12 with Persistent Current), refreshed by a new one; never stacks.
    void MakeConductive(Player* player, Unit* target)
    {
        player->CastSpell(target, SPELL_CONDUCTIVE, true);
        if (Aura* aura = target->GetAura(SPELL_CONDUCTIVE, player->GetGUID()))
        {
            int32 ms = HasStormMarker(player, MOD_PERSISTENT_CURRENT) ? PERSISTENT_CONDUCTIVE_MS : CONDUCTIVE_MS;
            aura->SetMaxDuration(ms);
            aura->SetDuration(ms);
        }
    }

    // The player's Lightning Shield (any rank): its proc effect.
    AuraEffect* Shield(Player* player)
    {
        return player->GetAuraEffect(SPELL_AURA_PROC_TRIGGER_SPELL, SPELLFAMILY_SHAMAN, LIGHTNING_SHIELD_FAMILY_FLAG, 0,
            0, player->GetGUID());
    }

    // The damage the player's best Lightning Bolt would deal the target (its rank's, with their spell power).
    float LightningBoltDamage(Player* player, Unit* target)
    {
        SpellInfo const* best = nullptr;
        for (uint32 id = SPELL_LIGHTNING_BOLT; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (player->HasSpell(id))
                best = sSpellMgr->GetSpellInfo(id);
        if (!best)
            return 0.0f;
        int32 base = std::max<int32>(0, best->Effects[EFFECT_0].CalcValue(player));
        return float(player->SpellDamageBonusDone(target, best, uint32(base), SPELL_DIRECT_DAMAGE, EFFECT_0));
    }

    // Secondary arcs leaving source: Chain Lightning's to any enemy near it (Closed Circuit: back into source at
    // half), Lightning Shield's to a Conductive one; Forked Tempest splits one between two. An arc never arcs again
    // (its damage is a spell of its own).
    void Arc(Player* player, Unit* source, float damage, bool shield)
    {
        float range = ARC_RANGE;
        if (HasStormMarker(player, MOD_BRANCHING_CURRENT))
            damage *= BRANCHING_ARCS;
        if (HasStormMarker(player, MOD_FARADAY_FIELD))
        {
            range *= FARADAY_REACH;
            damage *= FARADAY_ARCS;
        }
        if (shield && HasStormMarker(player, MOD_VIOLENT_REPRISAL))
            damage *= VIOLENT_ARCS;

        std::vector<Unit*> targets;
        if (!shield && HasStormMarker(player, MOD_CLOSED_CIRCUIT))
        {
            targets.push_back(source);
            damage *= CLOSED_CIRCUIT_SHARE;
        }
        else
        {
            for (Unit* enemy : EnemiesAround(player, source, range))
                if (enemy != source && (!shield || IsConductive(player, enemy)))
                    targets.push_back(enemy);
            std::size_t most = HasStormMarker(player, MOD_FORKED_TEMPEST) ? FORKED_TARGETS : 1;
            if (targets.size() > most)
                targets.resize(most);
        }
        if (targets.empty() || damage < 1.0f)
            return;
        int32 each = std::max<int32>(1, int32(damage / targets.size()));
        for (Unit* target : targets)
        {
            if (target != source)
                source->CastSpell(target, SPELL_STORMBOUND_ARC_VISUAL, true);
            player->CastCustomSpell(target, SPELL_STORMBOUND_ARC, &each, nullptr, nullptr, true);
        }
    }

    // A Lightning Shield retaliation the rune makes (Distant Conductor, Storm Within): the shield's rank's damage at
    // the target, a charge spent. A pulse never arcs.
    void Retaliate(Player* player, Unit* target, AuraEffect* shield, bool pulse)
    {
        uint32 damageSpell = sSpellMgr->GetSpellWithRank(SPELL_LIGHTNING_SHIELD_DAMAGE,
            shield->GetSpellInfo()->GetRank());
        SpellInfo const* info = sSpellMgr->GetSpellInfo(damageSpell);
        if (!info)
            return;
        int32 damage = std::max<int32>(1, info->Effects[EFFECT_0].CalcValue(player));
        Aura* aura = shield->GetBase();
        State& state = StateOf(player);
        state.Pulsing = pulse;
        player->CastCustomSpell(target, SPELL_STORMBOUND_RETALIATION, &damage, nullptr, nullptr, true);
        state.Pulsing = false;
        aura->DropCharge();
    }

    // Storm Within: every 3 sec, a pulse at the nearest Conductive enemy within 8 yards (checked every 0.25 sec
    // until there is one).
    void StormPulse(Player* player, uint32 diff)
    {
        if (!HasStormMarker(player, MOD_STORM_WITHIN))
            return;
        State& state = StateOf(player);
        state.StormTimer += diff;
        if (state.StormTimer < STORM_WITHIN_MS)
            return;
        AuraEffect* shield = Shield(player);
        Unit* target = nullptr;
        if (shield && player->IsInCombat())
            for (Unit* enemy : EnemiesAround(player, player, STORM_WITHIN_RANGE))
                if (IsConductive(player, enemy))
                {
                    target = enemy;
                    break;
                }
        if (!target)
        {
            state.StormTimer = STORM_WITHIN_MS - STORM_CHECK_MS;
            return;
        }
        state.StormTimer = 0;
        Retaliate(player, target, shield, true);
    }

    // ---- The rune's auras ------------------------------------------------------------------------------------

    std::vector<uint32> RuneAuras()
    {
        std::vector<uint32> auras;
        for (RuneDef const& rune : Catalog())
        {
            auras.push_back(rune.Aura);
            for (uint32 i = 0; i < SLOTS * CHOICES; ++i)
                auras.push_back(rune.MarkerFirst + i);
        }
        return auras;
    }

    // The active spec's rune and modifiers as auras (none below level 10).
    void Apply(Player* player)
    {
        for (uint32 spell : RuneAuras())
            player->RemoveAurasDueToSpell(spell);
        Choice const& choice = ActiveChoice(player);
        RuneDef const* rune = FindRune(choice.Rune);
        if (player->GetLevel() < UNLOCK_LEVEL || !rune)
        {
            ClearCombat(player);
            return;
        }
        if (rune->Id != RUNE_SANCTIFIED_PARAGON)
            player->RemoveAurasDueToSpell(SPELL_SANCTIFIED_PARAGON);
        player->AddAura(rune->Aura, player);
        for (uint8 slot = 0; slot < SLOTS; ++slot)
            if (uint8 pick = choice.Mods[slot])
                player->AddAura(rune->MarkerFirst + slot * CHOICES + pick - 1, player);
    }

    void Save(Player* player, uint8 spec)
    {
        Choice const& choice = StateOf(player).Choices[spec];
        CharacterDatabase.Execute(Acore::StringFormat("REPLACE INTO `titan_runes` (`guid`, `spec`, `rune`, "
            "`invocation`, `manifestation`, `adaptation`) VALUES ({}, {}, {}, {}, {}, {})",
            player->GetGUID().GetCounter(), spec, choice.Rune, choice.Mods[0], choice.Mods[1], choice.Mods[2]));
    }

    void Load(Player* player)
    {
        State& state = StateOf(player);
        state = State();
        if (QueryResult result = CharacterDatabase.Query("SELECT `spec`, `rune`, `invocation`, `manifestation`, "
            "`adaptation` FROM `titan_runes` WHERE `guid` = {}", player->GetGUID().GetCounter()))
        {
            do
            {
                Field* fields = result->Fetch();
                uint8 spec = fields[0].Get<uint8>();
                if (spec >= MAX_SPECS)
                    continue;
                Choice& choice = state.Choices[spec];
                choice.Rune = fields[1].Get<uint8>();
                for (uint8 slot = 0; slot < SLOTS; ++slot)
                    choice.Mods[slot] = std::min<uint8>(fields[2 + slot].Get<uint8>(), CHOICES);
            } while (result->NextRow());
        }
    }

    // ---- The panel ---------------------------------------------------------------------------------------------

    bool Prompt(Player* player)
    {
        return Wildcard::IsHero(player) && player->GetLevel() >= UNLOCK_LEVEL && !ActiveChoice(player).Rune
            && !Specs::IsDormant(player);
    }

    void SendState(Player* player)
    {
        Choice const& choice = ActiveChoice(player);
        uint8 role = Wildcard::GetRole(player);
        // Suggested: runes the Hero meets, their role's first (a Tank or Healer sees a hybrid only in Browse all).
        std::vector<RuneDef const*> offers;
        for (RuneDef const& rune : Catalog())
            if (Missing(player, rune.Requires) == "-"
                && (role == Wildcard::ROLE_NONE || rune.Roles & Wildcard::RoleMask(Wildcard::Role(role))))
                offers.push_back(&rune);
        if (offers.size() > MAX_OFFERS)
            offers.resize(MAX_OFFERS);
        std::string offerList;
        for (RuneDef const* rune : offers)
            offerList += (offerList.empty() ? "" : ".") + std::to_string(rune->Id);

        for (RuneDef const& rune : Catalog())
        {
            Send(player, Acore::StringFormat("R,{},{}", rune.Id, Missing(player, rune.Requires)));
            for (uint8 slot = 0; slot < SLOTS; ++slot)
                for (uint8 pick = 0; pick < CHOICES; ++pick)
                {
                    std::vector<std::vector<uint32>> needs = ModifierNeeds(rune.Id, slot, pick);
                    if (!needs.empty())
                        Send(player, Acore::StringFormat("N,{},{},{},{}", rune.Id, slot + 1, pick + 1,
                            Missing(player, needs)));
                }
        }
        Send(player, Acore::StringFormat("S,{},{},{},{},{},{},{},{},{}", player->GetLevel(), UNLOCK_LEVEL,
            Specs::ActiveSpec(player) + 1, choice.Rune, choice.Mods[0], choice.Mods[1], choice.Mods[2],
            offerList.empty() ? "-" : offerList, Prompt(player) ? 1 : 0));
    }

    void Refuse(Player* player, std::string const& why)
    {
        Send(player, "E," + why);
        ChatHandler(player->GetSession()).PSendSysMessage("|cffa335eeLegendary Rune:|r {}", why);
    }

    void OnPick(Player* player, uint8 runeId, bool anyway)
    {
        Choice& choice = ActiveChoice(player);
        RuneDef const* rune = FindRune(runeId);
        if (player->GetLevel() < UNLOCK_LEVEL)
            return Refuse(player, Acore::StringFormat("Legendary Runes unlock at level {}.", UNLOCK_LEVEL));
        if (choice.Rune)
            return Refuse(player, "This specialization already has its rune: only a prestige changes it.");
        if (Specs::IsDormant(player))
            return Refuse(player, "This specialization is empty until you prestige in it.");
        if (!rune)
            return Refuse(player, "That rune is not available yet.");
        if (Missing(player, rune->Requires) != "-" && !anyway)
            return Refuse(player, "You lack an ability this rune needs.");
        choice = Choice();
        choice.Rune = runeId;
        Save(player, Specs::ActiveSpec(player));
        Apply(player);
        ChatHandler(player->GetSession()).PSendSysMessage("|cffa335eeLegendary Rune:|r bound to this "
            "specialization until your next prestige. Choose its modifiers in the rune panel.");
        SendState(player);
    }

    void OnModifier(Player* player, uint8 slot, uint8 pick)
    {
        Choice& choice = ActiveChoice(player);
        if (!choice.Rune || slot < 1 || slot > SLOTS || pick < 1 || pick > CHOICES)
            return;
        if (player->IsInCombat())
            return Refuse(player, "Modifiers can't be changed in combat.");
        if (Missing(player, ModifierNeeds(choice.Rune, slot - 1, pick - 1)) != "-")
            return Refuse(player, "You lack an ability this modifier needs.");
        // The Sanctified Paragon: Bastion of Faith and Weight of the Unworthy work on its damage only (Sanctified
        // Condemnation); another Manifestation lets go of either.
        if (choice.Rune == RUNE_SANCTIFIED_PARAGON)
        {
            uint8 condemnation = MOD_SANCTIFIED_CONDEMNATION % CHOICES + 1;
            auto damageOnly = [](uint8 covenant) { return covenant == MOD_BASTION_OF_FAITH % CHOICES + 1
                || covenant == MOD_WEIGHT_OF_THE_UNWORTHY % CHOICES + 1; };
            if (slot == 3 && damageOnly(pick) && choice.Mods[1] != condemnation)
                return Refuse(player, "That modifier needs Sanctified Condemnation as your Manifestation.");
            if (slot == 2 && pick != condemnation && damageOnly(choice.Mods[2]))
            {
                choice.Mods[2] = 0;
                ChatHandler(player->GetSession()).SendSysMessage("|cffa335eeLegendary Rune:|r your Covenant needs "
                    "Sanctified Condemnation: choose it again.");
            }
        }
        if (choice.Mods[slot - 1] == pick)
            return SendState(player);
        choice.Mods[slot - 1] = pick;
        Save(player, Specs::ActiveSpec(player));
        ClearCombat(player);
        Apply(player);
        SendState(player);
    }

    void OnMessage(Player* player, std::string_view message)
    {
        std::vector<std::string_view> parts;
        for (std::size_t start = 0; start <= message.size();)
        {
            std::size_t end = message.find(',', start);
            if (end == std::string_view::npos)
                end = message.size();
            parts.push_back(message.substr(start, end - start));
            start = end + 1;
        }
        auto number = [&parts](std::size_t i) -> uint32
        {
            return i < parts.size() ? Acore::StringTo<uint32>(parts[i]).value_or(0) : 0;
        };
        if (!Wildcard::IsHero(player) || parts.empty())
            return;
        if (parts[0] == "STATE")
            SendState(player);
        else if (parts[0] == "PICK")
            OnPick(player, uint8(number(1)), number(2) == 1);
        else if (parts[0] == "MOD")
            OnModifier(player, uint8(number(1)), uint8(number(2)));
    }
}

namespace LegendaryRunes
{
    void OnSpecChanged(Player* player)
    {
        ClearCombat(player);
        Apply(player);
        SendState(player);
        if (Prompt(player))
            Send(player, "O");
    }

    void OnPrestige(Player* player)
    {
        CharacterDatabase.Execute(Acore::StringFormat("DELETE FROM `titan_runes` WHERE `guid` = {}",
            player->GetGUID().GetCounter()));
        State& state = StateOf(player);
        state.Choices = {};
        ClearCombat(player);
        Apply(player);
    }

    Unit* FelRiftTarget(Player* player)
    {
        if (!player->HasAura(SPELL_FEL_RIFT))
            return nullptr;
        State& state = StateOf(player);
        Unit* target = state.RiftTarget ? ObjectAccessor::GetUnit(*player, state.RiftTarget) : nullptr;
        return target && target->IsAlive() && player->IsValidAttackTarget(target) ? target : nullptr;
    }
}

// The rune's aura (90700): Demon's Bite, Chaos Strike, Shadow Bolt and Incinerate hits grant Fel Command (a
// Chaos Strike crit 2); with Dreadfire Discipline, Shadow Bolt and Incinerate 2 and Demon's Bite none. A volley
// bolt's crit takes 3 sec off Throw Glaive with Shattered Glaive (9 sec a volley at most).
class spell_titan_rune_fel_commandment : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_fel_commandment);

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        SpellInfo const* spell = eventInfo.GetSpellInfo();
        Player* player = GetTarget()->ToPlayer();
        if (!spell || !player || !(eventInfo.GetHitMask() & (PROC_HIT_NORMAL | PROC_HIT_CRITICAL | PROC_HIT_ABSORB)))
            return false;
        bool crit = eventInfo.GetHitMask() & PROC_HIT_CRITICAL;
        uint32 first = spell->GetFirstRankSpell()->Id;

        if (first == SPELL_VOLLEY_BOLT)
        {
            State& state = StateOf(player);
            if (crit && HasMarker(player, MOD_SHATTERED_GLAIVE) && state.ShatterBudget > 0)
            {
                player->ModifySpellCooldown(SPELL_THROW_GLAIVE, -SHATTER_CUT_MS);
                state.ShatterBudget -= SHATTER_CUT_MS;
            }
            return false;
        }

        bool dreadfire = HasMarker(player, MOD_DREADFIRE_DISCIPLINE);
        if (first == SPELL_CHAOS_STRIKE)
            Gain(player, crit ? 2 : 1);
        else if (first == SPELL_DEMONS_BITE && !dreadfire)
            Gain(player, 1);
        else if (first == SPELL_SHADOW_BOLT || first == SPELL_INCINERATE)
            Gain(player, dreadfire ? 2 : 1);
        return false;                               // the stacks are granted here; nothing else to proc
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_titan_rune_fel_commandment::CheckProc);
    }
};

// Throw Glaive (90111) with the rune at 5 Fel Command: Chaos Bolt Volley. Not when the only enemy about is
// crowd-controlled (the volley would break it). A triggered throw (Shattered Glaive's bounce) never fires one.
class spell_titan_rune_throw_glaive : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_throw_glaive);

    bool Armed()
    {
        Player* player = GetCaster()->ToPlayer();
        return player && !GetSpell()->IsTriggered() && player->HasAura(SPELL_RUNE_FEL_COMMANDMENT)
            && Stacks(player) >= MAX_STACKS;
    }

    SpellCastResult CheckTarget()
    {
        Unit* target = GetExplTargetUnit();
        if (!Armed() || !target || !target->HasBreakableByDamageCrowdControlAura())
            return SPELL_CAST_OK;
        Player* player = GetCaster()->ToPlayer();
        return EnemiesAround(player, target, BOLT_RANGE).empty() ? SPELL_FAILED_BAD_TARGETS : SPELL_CAST_OK;
    }

    void Volley()
    {
        Unit* target = GetExplTargetUnit();
        Player* player = GetCaster()->ToPlayer();
        if (!target || !player || GetSpell()->IsTriggered() || !player->HasAura(SPELL_RUNE_FEL_COMMANDMENT))
            return;
        // Imp Legion / Fel Host: every throw spends the Fel Command, a demon for each stack (Felguards in
        // Metamorphosis with Fel Host), before the volley so its bolts count them.
        uint8 stacks = Stacks(player);
        bool armed = Armed();
        if (Legion(player) && stacks)
        {
            bool felguards = HasMarker(player, MOD_FEL_HOST) && InMetamorphosis(player);
            for (uint8 i = 0; i < stacks; ++i)
                SummonDemon(player, target, felguards);
            if (!armed)
                player->RemoveAurasDueToSpell(SPELL_FEL_COMMAND);
        }
        if (armed)
            FireVolley(player, target);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_titan_rune_throw_glaive::CheckTarget);
        AfterCast += SpellCastFn(spell_titan_rune_throw_glaive::Volley);
    }
};

// Consecration (every rank) with the Sanctified Paragon: no ground left behind; the cast turns the aura on (paying
// Consecration's cost and starting its cooldown), and a cast while it is on turns it off (free, no cooldown).
class spell_titan_rune_consecration : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_consecration);

    SpellCastResult Toggle()
    {
        Unit* caster = GetCaster();
        if (!caster->HasAura(SPELL_RUNE_SANCTIFIED_PARAGON) || !caster->HasAura(SPELL_SANCTIFIED_PARAGON))
            return SPELL_CAST_OK;
        caster->RemoveAurasDueToSpell(SPELL_SANCTIFIED_PARAGON);
        return SPELL_FAILED_DONT_REPORT;
    }

    void Sanctify(SpellEffIndex effIndex)
    {
        Unit* caster = GetCaster();
        if (!caster->HasAura(SPELL_RUNE_SANCTIFIED_PARAGON))
            return;
        PreventHitDefaultEffect(effIndex);
        caster->AddAura(SPELL_SANCTIFIED_PARAGON, caster);
        caster->CastSpell(caster, SPELL_PARAGON_AWAKEN, true);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_titan_rune_consecration::Toggle);
        OnEffectHit += SpellEffectFn(spell_titan_rune_consecration::Sanctify, EFFECT_0,
            SPELL_EFFECT_PERSISTENT_AREA_AURA);
    }
};

// Sanctified Paragon (90731): a second of sanctified ground each tick (SanctifiedGround); its bonuses go with it.
class spell_titan_rune_sanctified_paragon : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_sanctified_paragon);

    void Tick(AuraEffect const* /*aurEff*/)
    {
        if (Player* player = GetTarget()->ToPlayer())
        {
            if (!player->HasAura(SPELL_RUNE_SANCTIFIED_PARAGON) || !SanctifiedGround(player))
                return Remove();
        }
    }

    void Gone(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        target->RemoveAurasDueToSpell(SPELL_SEAT_OF_THE_PARAGON);
        target->RemoveAurasDueToSpell(SPELL_BASTION_OF_FAITH);
        if (Player* player = target->ToPlayer())
        {
            State& state = StateOf(player);
            state.Trail.clear();
            state.HasLastPosition = false;
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_titan_rune_sanctified_paragon::Tick, EFFECT_0,
            SPELL_AURA_PERIODIC_DUMMY);
        AfterEffectRemove += AuraEffectRemoveFn(spell_titan_rune_sanctified_paragon::Gone, EFFECT_0,
            SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// The Stormbound Conduit's aura (90750): a Lightning Bolt hit makes its target Conductive (Branching Current: one
// more enemy within 8 yards; Distant Conductor: a Lightning Shield retaliation at it, a charge each); a Chain
// Lightning hit on a Conductive enemy (each jump's) arcs for 15% of Lightning Bolt's damage.
class spell_titan_rune_stormbound_conduit : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_stormbound_conduit);

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        SpellInfo const* spell = eventInfo.GetSpellInfo();
        Player* player = GetTarget()->ToPlayer();
        Unit* target = eventInfo.GetActionTarget();
        if (!spell || !player || !target || target == player || !target->IsAlive()
            || !(eventInfo.GetHitMask() & (PROC_HIT_NORMAL | PROC_HIT_CRITICAL | PROC_HIT_ABSORB)))
            return false;
        uint32 first = spell->GetFirstRankSpell()->Id;
        if (first == SPELL_LIGHTNING_BOLT)
        {
            MakeConductive(player, target);
            if (HasStormMarker(player, MOD_BRANCHING_CURRENT))
                for (Unit* enemy : EnemiesAround(player, target, BRANCHING_RANGE))
                    if (enemy != target)
                    {
                        MakeConductive(player, enemy);
                        break;
                    }
            if (HasStormMarker(player, MOD_DISTANT_CONDUCTOR))
                if (AuraEffect* shield = Shield(player))
                    Retaliate(player, target, shield, false);
        }
        else if (first == SPELL_CHAIN_LIGHTNING && IsConductive(player, target))
            Arc(player, target, LightningBoltDamage(player, target) * CHAIN_ARC_SHARE, false);
        return false;                               // the rune's work is done here; nothing else to proc
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_titan_rune_stormbound_conduit::CheckProc);
    }
};

// Lightning Shield (every rank) with the Stormbound Conduit: Storm Within and Distant Conductor replace the
// retaliation when struck.
class spell_titan_rune_lightning_shield : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_lightning_shield);

    bool CheckProc(ProcEventInfo& /*eventInfo*/)
    {
        Player* player = GetTarget()->ToPlayer();
        return !player || !player->HasAura(SPELL_RUNE_STORMBOUND_CONDUIT)
            || (!HasStormMarker(player, MOD_STORM_WITHIN) && !HasStormMarker(player, MOD_DISTANT_CONDUCTOR));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_titan_rune_lightning_shield::CheckProc);
    }
};

// A Lightning Shield retaliation (its damage, every rank, and the rune's own 90754) with the Stormbound Conduit:
// Violent Reprisal takes 10% off it; it arcs to a Conductive enemy near the target for 25% of its damage (40% more
// with Violent Reprisal). A Storm Within pulse never arcs.
class spell_titan_rune_lightning_shield_damage : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_lightning_shield_damage);

    void Strike()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target || !player->HasAura(SPELL_RUNE_STORMBOUND_CONDUIT))
            return;
        int32 damage = GetHitDamage();
        if (HasStormMarker(player, MOD_VIOLENT_REPRISAL))
        {
            damage = int32(damage * VIOLENT_RETALIATION);
            SetHitDamage(damage);
        }
        if (StateOf(player).Pulsing)
            return;
        Arc(player, target, damage * SHIELD_ARC_SHARE, true);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_titan_rune_lightning_shield_damage::Strike);
    }
};

// The rune's Felguard (910120): 8 sec at the Hero's side, on the Hero's target (the next when it dies). Felstorm as
// it arrives (a 2 sec spin: 4 ticks), then Cleave and melee. 50% of the Hero's power (spell power or attack power,
// the higher) in its weapon, 30% of their health; no threat.
struct npc_titan_rune_felguard : public ScriptedAI
{
    npc_titan_rune_felguard(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        Player* owner = summoner ? summoner->ToPlayer() : nullptr;
        if (!owner)
            return;
        me->AddAura(SPELL_FELGUARD_GUARDIAN, me);
        uint32 health = std::max<uint32>(1, owner->CountPctFromMaxHealth(30));
        me->SetMaxHealth(health);
        me->SetHealth(health);
        float power = std::max(float(owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_ALL)),
            owner->GetTotalAttackPowerValue(BASE_ATTACK)) * 0.5f;
        float swing = (BoltDamage(owner) * 0.25f) + power / 14.0f * (me->GetAttackTime(BASE_ATTACK) / 1000.0f);
        me->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, swing * 0.9f);
        me->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, swing * 1.1f);
        me->UpdateDamagePhysical(BASE_ATTACK);
        me->SetReactState(REACT_AGGRESSIVE);
        for (uint32 tick = 0; tick < 4; ++tick)
            scheduler.Schedule(Milliseconds(500 * tick), [this](TaskContext) { DoCastSelf(SPELL_FELSTORM, true); });
        scheduler.Schedule(3s, [this](TaskContext cleave)
        {
            if (me->GetVictim())
                DoCastVictim(SPELL_FELGUARD_CLEAVE, true);
            cleave.Repeat(6s);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
        Unit* victim = me->GetVictim();
        if (!victim || !victim->IsAlive())
            if (Unit* owner = me->GetOwner())
                if (Unit* next = owner->GetVictim())
                    AttackStart(next);
        DoMeleeAttackIfReady();
    }
};

// Imp Legion's imp (910121): 10 sec, Firebolt at its enemy every 2 sec from where it stands (a quarter of a volley
// bolt), then the Hero's target; no threat.
struct npc_titan_rune_fel_imp : public ScriptedAI
{
    npc_titan_rune_fel_imp(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        me->AddAura(SPELL_FEL_IMP_GUARDIAN, me);
        me->SetReactState(REACT_AGGRESSIVE);
        scheduler.Schedule(500ms, [this](TaskContext bolt)
        {
            Unit* victim = me->GetVictim();
            if (!victim || !victim->IsAlive())
                if (Unit* owner = me->GetOwner())
                    victim = owner->GetVictim();
            Player* owner = me->GetOwner() ? me->GetOwner()->ToPlayer() : nullptr;
            if (victim && owner)
            {
                int32 damage = std::max<int32>(1, BoltDamage(owner) / 4);
                me->CastCustomSpell(victim, SPELL_FIREBOLT, &damage, nullptr, nullptr, true);
            }
            bolt.Repeat(2s);
        });
    }

    void AttackStart(Unit* who) override
    {
        AttackStartCaster(who, 25.0f);
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }
};

class titan_runes_player : public PlayerScript
{
public:
    titan_runes_player() : PlayerScript("titan_runes_player", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_LEVEL_CHANGED,
        PLAYERHOOK_ON_PLAYER_JUST_DIED,
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_DELETE,
        PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT
    }) { }

    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 language, std::string& msg,
        Player* receiver) override
    {
        std::string const prefix = std::string(PREFIX) + '\t';
        if (language != LANG_ADDON || receiver != player || msg.compare(0, prefix.size(), prefix) != 0)
            return true;
        OnMessage(player, std::string_view(msg).substr(prefix.size()));
        return false;
    }

    void OnPlayerLogin(Player* player) override
    {
        Load(player);
        if (Wildcard::IsHero(player))
            Apply(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        ClearCombat(player);
        states.erase(player->GetGUID().GetCounter());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        CharacterDatabase.Execute(Acore::StringFormat("DELETE FROM `titan_runes` WHERE `guid` = {}",
            guid.GetCounter()));
    }

    // Level 10: the panel opens while the active spec has no rune.
    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (!Wildcard::IsHero(player))
            return;
        Apply(player);
        if (oldLevel < UNLOCK_LEVEL && player->GetLevel() >= UNLOCK_LEVEL)
        {
            SendState(player);
            if (Prompt(player))
                Send(player, "O");
        }
    }

    void OnPlayerJustDied(Player* player) override
    {
        ClearCombat(player);
    }

    // Metamorphic Bond on the Hero's demon while they are in Metamorphosis; Storm Within's pulses.
    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (player->HasAura(SPELL_RUNE_STORMBOUND_CONDUIT))
            StormPulse(player, diff);
        if (!player->HasAura(SPELL_RUNE_FEL_COMMANDMENT))
            return;
        State& state = StateOf(player);
        state.BondTimer += diff;
        if (state.BondTimer < BOND_CHECK_MS)
            return;
        state.BondTimer = 0;
        Pet* demon = Demon(player);
        if (!demon)
            return;
        bool bond = HasMarker(player, MOD_METAMORPHIC_BOND) && InMetamorphosis(player);
        if (bond && !demon->HasAura(SPELL_METAMORPHIC_BOND))
            demon->AddAura(SPELL_METAMORPHIC_BOND, demon);
        else if (!bond && demon->HasAura(SPELL_METAMORPHIC_BOND))
            demon->RemoveAurasDueToSpell(SPELL_METAMORPHIC_BOND);
    }
};

// Live Wire (the Stormbound Conduit): melee attackers become Conductive.
// The Hero's demon: a hit grants Fel Command, once every 4 sec (2 with Infernal Pact); with Metamorphic Bond, in
// Metamorphosis, it strikes 2 more enemies within 5 yards for half. Entering Metamorphosis with Demonic Ascension:
// 5 Fel Command.
class titan_runes_unit : public UnitScript
{
public:
    titan_runes_unit() : UnitScript("titan_runes_unit", true, { UNITHOOK_ON_DAMAGE, UNITHOOK_ON_AURA_APPLY }) { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        // Live Wire: an enemy that strikes the Hero in melee range becomes Conductive (whatever their shield does).
        if (attacker && victim && damage && attacker != victim && victim->IsPlayer())
        {
            Player* hero = victim->ToPlayer();
            if (HasStormMarker(hero, MOD_LIVE_WIRE) && hero->HasAura(SPELL_RUNE_STORMBOUND_CONDUIT)
                && attacker->IsAlive() && !IsConductive(hero, attacker) && hero->IsWithinMeleeRange(attacker)
                && hero->IsValidAttackTarget(attacker))
                MakeConductive(hero, attacker);
        }

        static bool cleaving = false;
        if (!attacker || !victim || !damage || cleaving || !attacker->IsPet())
            return;
        Player* owner = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (!owner || owner == attacker || !owner->HasAura(SPELL_RUNE_FEL_COMMANDMENT) || Demon(owner) != attacker)
            return;

        State& state = StateOf(owner);
        uint32 now = GameTime::GetGameTimeMS().count();
        uint32 interval = HasMarker(owner, MOD_INFERNAL_PACT) ? INFERNAL_PACT_INTERVAL_MS : DEMON_STACK_INTERVAL_MS;
        if (now - state.LastDemonStack >= interval)
        {
            state.LastDemonStack = now;
            Gain(owner, 1);
        }

        if (HasMarker(owner, MOD_METAMORPHIC_BOND) && InMetamorphosis(owner))
        {
            std::vector<Unit*> enemies = EnemiesAround(owner, victim, BOND_CLEAVE_RANGE);
            enemies.erase(std::remove(enemies.begin(), enemies.end(), victim), enemies.end());
            if (enemies.size() > BOND_CLEAVE_TARGETS)
                enemies.resize(BOND_CLEAVE_TARGETS);
            int32 share = std::max<int32>(1, int32(damage / 2));
            cleaving = true;
            for (Unit* enemy : enemies)
                attacker->CastCustomSpell(enemy, SPELL_METAMORPHIC_CLEAVE, &share, nullptr, nullptr, true);
            cleaving = false;
        }
    }

    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        uint32 id = aura->GetId();
        if (id != SPELL_METAMORPHOSIS_DH && id != SPELL_METAMORPHOSIS_WARLOCK)
            return;
        if (Player* player = unit->ToPlayer())
            if (HasMarker(player, MOD_DEMONIC_ASCENSION))
                Gain(player, MAX_STACKS);
    }
};

void AddSC_runes()
{
    RegisterSpellScript(spell_titan_rune_fel_commandment);
    RegisterSpellScript(spell_titan_rune_throw_glaive);
    RegisterSpellScript(spell_titan_rune_consecration);
    RegisterSpellScript(spell_titan_rune_sanctified_paragon);
    RegisterSpellScript(spell_titan_rune_stormbound_conduit);
    RegisterSpellScript(spell_titan_rune_lightning_shield);
    RegisterSpellScript(spell_titan_rune_lightning_shield_damage);
    RegisterCreatureAI(npc_titan_rune_felguard);
    RegisterCreatureAI(npc_titan_rune_fel_imp);
    new titan_runes_player();
    new titan_runes_unit();
}
