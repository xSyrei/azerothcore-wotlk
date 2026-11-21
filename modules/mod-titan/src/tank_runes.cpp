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

#include "tank_runes.h"
#include "wildcard.h"
#include "CellImpl.h"
#include "Chat.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

// The Tank Collection (the compendium's dossiers 02, 06 and 07), Rank I values throughout (the server has no rune
// ranks). Each works through its rune's aura and modifier markers (runes.cpp binds them):
//
// - Red Communion (46): Blood Boil links up to 5 enemies it hits to the Hero for 10 sec (the Hero's selected enemy
//   first, then the nearest; a full set replaces its oldest other link). An original Blood Strike hit on a linked
//   enemy heals 1.5% of maximum health plus 0.15% for each other link within 20 yards (2.1% at most) and pulses 20%
//   of attack power as Shadow damage to each other link in range, once every 2 sec. Sanguine Recall (taught; 18 sec,
//   free, off the global cooldown): 30% of attack power to each link in range and an absorb of 3% of maximum health
//   plus 0.5% for each further link, 5% at most, for 5 sec. Modifiers: Sovereign Bond (the selected linked enemy
//   in range deals 8% less damage to the Hero), Bloodbound Court (8 links), Long Veins (15 sec, 30 yards); Crimson
//   Feast (2%, 2.6% at most), Arterial Echo (Blood Strike also leaves 35% of attack power over 4 sec on its linked
//   victim, a refresh keeping the unpaid damage up to 200% of a fresh one), Shared Weakness (pulsed enemies deal 8%
//   less magic damage to the Hero for 4 sec); Coagulated Throne (5%, 7% at most), Rapid Recall (14 sec), Blood for
//   Blood (Recall heals 2% and deals 45%).
// - Primal Worldwarden (47): the Worldheart Totem (taught; 30 min, follows 4 yards behind; 6 sec) gives Worldguard
//   while a shield is on (8% less damage taken, 200% more threat). An original Earth, Flame or Frost Shock commands
//   it: Earth an absorb of 3% of maximum health for 4 sec (the larger of the old and new kept), Flame 25% of attack
//   power as Fire damage to up to 5 enemies within 8 yards of the totem, Frost slows up to 5 there by 35% and has them
//   deal 6% less damage to the Hero for 4 sec. An original Stormstrike repeats the last command (10 sec remembered)
//   at 60% once every 6 sec. Lava Lash while the totem is out draws a magma line from the Hero to it (20 yards at
//   most) for 4 sec: every second 10% of attack power as Fire damage to up to 5 enemies within 1.5 yards of it. Place
//   Worldheart (taught; 8 sec, off the global cooldown) anchors the totem at the Hero's target (or the Hero) within
//   20 yards for 6 sec. Modifiers: Earthen Command (5%, the echo 3%), Flame Dominion (40%, the echo 24%), Winter
//   Command (45% slow and 9% less, the echo 27% and 5.4%); Deepstone Heart (11%), Wide Communion (12 yards, Flame 8
//   targets, Frost 8), Striding Heart (5 sec, 25% speed for 3 sec after); Resonant Strikes (an echo every 4 sec),
//   Molten Causeway (16%, 2.5 yards each side), Stoneblood Lash (Lava Lash heals 1.5%, once every 4 sec).
// - Black Covenant (48): Summon Voidwalker's buttons become Bind: Voidwalker's (taught; 2 sec cast, 10 sec), which
//   binds a Voidwalker to the Hero as Covenant Armor until cast again (a Voidwalker out is dismissed into it; any pet
//   summoned meanwhile ends it): armor from items 400% more, 15% more maximum health (its share kept), 200% more
//   threat, 6% less damage taken, Shadowform's look in purple. While bound, Searing Pain's buttons are Covenant Lash
//   (instant, 10 yards, 8% of base mana: 100% of spell power as Shadowflame, 100% more threat) and, for a Hero who
//   knows Drain Life, Drain Life's are Soul Siphon (a 3 sec channel at 70% speed, 6% of base mana, 15 sec: each
//   second 25% of spell power as Shadow damage and 1.5% of maximum health back). Dark Intercession (taught; 30 sec,
//   free, off the global cooldown): 15% less damage taken for 6 sec, Covenant Lash meanwhile hitting up to 4 more
//   enemies within 6 yards of its victim for 50%. The Hero's
//   Curse of Weakness has its victim deal 6% less Physical damage to them, Curse of Tongues 6% less magic damage.
//   Modifiers: Voidforged Plate (2500 more armor at level 60, a level's share below), Nether Insulation (8% less
//   magic damage taken), Hungry Covenant (Lash: 1% of base mana back, once every 2 sec); Deep Extraction (Siphon
//   heals 2.5%), Ravenous Siphon (40% of spell power a tick), Lashing Rhythm (each Covenant Lash 1 sec off Soul
//   Siphon's cooldown); Guardian Manifest (22%), Legion's Rebuke (7 more, 75%), Frequent Petition (24 sec, and a 2%
//   heal).
// - The foundation, while any of them is bound: NPC attacks never critically strike the Hero; Red Communion and the
//   Primal Worldwarden also teach Challenge (a free 30-yard taunt off the global cooldown, 8 sec). The Black
//   Covenant's Hero gets their taunt from Wildcard.
namespace
{
    using namespace TankRunes;

    enum TankSpells : uint32
    {
        SPELL_BLOOD_STRIKE          = 45902,
        SPELL_FLAME_SHOCK           = 8050,
        SPELL_FROST_SHOCK           = 8056,
        SPELL_STORMSTRIKE           = 17364,
        SPELL_LAVA_LASH             = 60103,
        SPELL_DRAIN_LIFE            = 689,
        SPELL_SUMMON_VOIDWALKER     = 697,
        SPELL_CURSE_OF_WEAKNESS     = 702,
        SPELL_CURSE_OF_TONGUES      = 1714,

        SPELL_CRIMSON_LINK          = 91480,
        SPELL_COMMUNION_PULSE       = 91481,
        SPELL_SANGUINE_RECALL       = 91482,
        SPELL_RECALL_STRIKE         = 91483,
        SPELL_SANGUINE_WARD         = 91484,
        SPELL_BLOOD_RECLAMATION     = 91485,
        SPELL_SHARED_WEAKNESS       = 91486,
        SPELL_ARTERIAL_ECHO         = 91487,
        SPELL_WORLDHEART_TOTEM      = 91500,
        SPELL_PLACE_WORLDHEART      = 91501,
        SPELL_WORLDGUARD            = 91502,
        SPELL_EARTHEN_COMMAND       = 91503,
        SPELL_FLAME_COMMAND         = 91504,
        SPELL_FROST_COMMAND         = 91505,
        SPELL_MAGMA_STRIKE          = 91506,
        SPELL_MAGMA_GROUND          = 91507,
        SPELL_STRIDING_HEART        = 91508,
        SPELL_STONEBLOOD_LASH       = 91509,
        SPELL_BIND_GUARDIAN         = 91520,
        SPELL_COVENANT_ARMOR        = 91521,
        SPELL_COVENANT_PACT         = 91522,
        SPELL_COVENANT_LASH         = 91523,
        SPELL_LASH_STRIKE           = 91524,
        SPELL_SOUL_SIPHON           = 91525,
        SPELL_SIPHON_STRIKE         = 91526,
        SPELL_SIPHON_HEAL           = 91527,
        SPELL_DARK_INTERCESSION     = 91529,
        SPELL_INTERCESSION          = 91540,
        SPELL_SIPHON_SLOW           = 91541,
        SPELL_CHALLENGE             = 91542,
        SPELL_TANK_FOUNDATION       = 91543,
        SPELL_COMMUNION_BOIL        = 91544,

        NPC_WORLDHEART_TOTEM        = 910125,
    };

    // The modifiers: slot x 3 + choice.
    enum CommunionModifier : uint8
    {
        MOD_SOVEREIGN_BOND, MOD_BLOODBOUND_COURT, MOD_LONG_VEINS,
        MOD_CRIMSON_FEAST, MOD_ARTERIAL_ECHO, MOD_SHARED_WEAKNESS,
        MOD_COAGULATED_THRONE, MOD_RAPID_RECALL, MOD_BLOOD_FOR_BLOOD,
    };

    enum WorldwardenModifier : uint8
    {
        MOD_EARTHEN_COMMAND, MOD_FLAME_DOMINION, MOD_WINTER_COMMAND,
        MOD_DEEPSTONE_HEART, MOD_WIDE_COMMUNION, MOD_STRIDING_HEART,
        MOD_RESONANT_STRIKES, MOD_MOLTEN_CAUSEWAY, MOD_STONEBLOOD_LASH,
    };

    enum CovenantModifier : uint8
    {
        MOD_VOIDFORGED_PLATE, MOD_NETHER_INSULATION, MOD_HUNGRY_COVENANT,
        MOD_DEEP_EXTRACTION, MOD_RAVENOUS_SIPHON, MOD_LASHING_RHYTHM,
        MOD_GUARDIAN_MANIFEST, MOD_LEGIONS_REBUKE, MOD_FREQUENT_PETITION,
    };

    // Red Communion's numbers. Blood Boil: Rage and a cooldown instead of its rune.
    constexpr int32 BLOOD_BOIL_RAGE = 20;
    constexpr uint32 BLOOD_BOIL_COOLDOWN_MS = 6000;
    constexpr uint32 LINKS = 5, COURT_LINKS = 8;
    constexpr int32 LINK_MS = 10000, LONG_LINK_MS = 15000;
    constexpr float LINK_RANGE = 20.0f, LONG_LINK_RANGE = 30.0f;
    constexpr uint32 RECLAIM_EVERY_MS = 2000;
    constexpr float RECLAIM_PCT = 1.5f, FEAST_PCT = 2.0f, RECLAIM_PER_LINK = 0.15f;
    constexpr float RECLAIM_CAP = 2.1f, FEAST_CAP = 2.6f;
    constexpr float PULSE_AP = 0.2f, RECALL_AP = 0.3f, BLOOD_FOR_BLOOD_AP = 0.45f;
    constexpr float RECALL_ABSORB = 3.0f, THRONE_ABSORB = 5.0f, RECALL_PER_LINK = 0.5f;
    constexpr float RECALL_CAP = 5.0f, THRONE_CAP = 7.0f, BLOOD_FOR_BLOOD_HEAL = 2.0f;
    constexpr int32 RECALL_MS = 5000, RAPID_RECALL_MS = 4000;  // the cut: 18 to 14 sec
    constexpr float SOVEREIGN_SHARE = 0.92f, SHARED_WEAKNESS_SHARE = 0.92f;
    constexpr int32 WEAKNESS_MS = 4000, ARTERIAL_MS = 4000;
    constexpr float ARTERIAL_AP = 0.35f, ARTERIAL_CARRY_CAP = 2.0f;
    constexpr int32 ARTERIAL_TICKS = 4;

    // Primal Worldwarden's numbers.
    constexpr uint32 TOTEM_MS = 30 * MINUTE * IN_MILLISECONDS;
    constexpr float TOTEM_FOLLOW = 4.0f, TOTEM_LEASH = 30.0f, PLACE_RANGE = 20.0f;
    constexpr int32 ANCHOR_MS = 6000, STRIDING_PLACE_MS = 3000;     // Striding Heart: 8 to 5 sec
    constexpr float WORLDGUARD_PCT = 8.0f, DEEPSTONE_PCT = 11.0f, WORLDGUARD_THREAT = 200.0f;
    constexpr float EARTH_ABSORB = 3.0f, EARTHEN_ABSORB = 5.0f;
    constexpr float FLAME_AP = 0.25f, DOMINION_AP = 0.4f;
    constexpr float FROST_SLOW = 35.0f, WINTER_SLOW = 45.0f, FROST_REDUCTION = 6.0f, WINTER_REDUCTION = 9.0f;
    constexpr float COMMAND_RADIUS = 8.0f, WIDE_RADIUS = 12.0f;
    constexpr uint32 COMMAND_TARGETS = 5, WIDE_TARGETS = 8;
    constexpr int32 COMMAND_MS = 4000, COMMAND_MEMORY_MS = 10000;
    constexpr float ECHO_SHARE = 0.6f;
    constexpr uint32 ECHO_EVERY_MS = 6000, RESONANT_EVERY_MS = 4000;
    constexpr float MAGMA_AP = 0.1f, MOLTEN_AP = 0.16f, MAGMA_HALF_WIDTH = 1.5f, MOLTEN_HALF_WIDTH = 2.5f;
    constexpr float MAGMA_RANGE = 20.0f, MAGMA_DECAL_STEP = 4.0f;
    constexpr uint32 MAGMA_TARGETS = 5, MAGMA_TICKS = 4;
    constexpr float STONEBLOOD_PCT = 1.5f;
    constexpr uint32 STONEBLOOD_EVERY_MS = 4000;

    // Black Covenant's numbers.
    constexpr int32 ARMOR_PCT = 400, PACT_THREAT = 200, INSULATION_PCT = 8;
    // Voidforged Plate: this much armor on top at the level cap, a share of it below (level / 60).
    constexpr int32 VOIDFORGED_ARMOR = 2500;
    constexpr float LASH_SP = 1.0f, LASH_CLEAVE_RANGE = 6.0f, LASH_CLEAVE = 0.5f, REBUKE_CLEAVE = 0.75f;
    constexpr uint32 LASH_CLEAVE_TARGETS = 4, REBUKE_TARGETS = 7;
    constexpr float HUNGRY_MANA_PCT = 1.0f;
    constexpr uint32 HUNGRY_EVERY_MS = 2000;
    constexpr float SIPHON_SP = 0.25f, RAVENOUS_SP = 0.4f, SIPHON_HEAL = 1.5f, DEEP_HEAL = 2.5f;
    constexpr int32 RHYTHM_MS = 1000;           // Lashing Rhythm: off Soul Siphon's cooldown a Lash
    constexpr uint32 VOIDFORGED_LEVEL = 60;     // Voidforged Plate's full armor at this level
    constexpr int32 INTERCESSION_PCT = 15, MANIFEST_PCT = 22, INTERCESSION_MS = 6000;
    constexpr int32 PETITION_CUT_MS = 6000;     // 30 to 24 sec
    constexpr float PETITION_HEAL = 2.0f;
    constexpr float CURSE_SHARE = 0.94f;

    struct Link
    {
        ObjectGuid Guid;
        uint32 At;                      // when linked (the oldest is replaced)
    };

    struct FrostHold
    {
        float Pct;
        uint32 Until;
    };

    struct TankState
    {
        std::vector<Link> Links;
        uint32 LastReclaim = 0;
        ObjectGuid Totem;
        uint32 AnchorUntil = 0;         // 0: following
        uint8 Command = 0;              // 1 Earth, 2 Flame, 3 Frost (the last original Shock's)
        uint32 CommandAt = 0;
        uint32 LastEcho = 0;
        uint32 LastStoneblood = 0;
        Position MagmaFrom, MagmaTo;
        uint32 MagmaTicks = 0;          // ticks left
        uint32 MagmaTimer = 0;
        float MagmaShare = 0.0f;
        float MagmaHalfWidth = 0.0f;
        uint32 FollowTimer = 0;
        std::unordered_map<ObjectGuid, FrostHold> Frost;     // the enemies' reduced damage to the Hero
        uint32 LastHungry = 0;
        bool Bound = false;             // Covenant Armor on (its own aura is the truth; this ends it once)
    };

    std::unordered_map<uint32, TankState> states;

    TankState& StateOf(Player* player)
    {
        return states[player->GetGUID().GetCounter()];
    }

    uint32 Now()
    {
        return GameTime::GetGameTimeMS().count();
    }

    bool Marked(Unit* unit, uint32 first, uint8 modifier)
    {
        return unit && unit->HasAura(first + modifier);
    }

    bool Communion(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_COMMUNION_MODIFIER_FIRST, modifier);
    }

    bool Worldwarden(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_WORLDWARDEN_MODIFIER_FIRST, modifier);
    }

    bool Covenant(Unit* unit, uint8 modifier)
    {
        return Marked(unit, SPELL_COVENANT_MODIFIER_FIRST, modifier);
    }

    uint32 FirstRank(uint32 spellId)
    {
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        return info ? info->GetFirstRankSpell()->Id : spellId;
    }

    // The aura of the chain's (any rank's) the caster put on the unit.
    bool HasRankAura(Unit* unit, uint32 firstRank, ObjectGuid caster)
    {
        for (SpellInfo const* rank = sSpellMgr->GetSpellInfo(firstRank); rank; rank = rank->GetNextRankSpell())
            if (unit->HasAura(rank->Id, caster))
                return true;
        return false;
    }

    // Enemies near a point the Hero may hit: alive, attackable, in sight, nearest first.
    std::vector<Unit*> EnemiesNear(Player* player, WorldObject* center, float range)
    {
        std::list<Unit*> found;
        Acore::AnyUnfriendlyUnitInObjectRangeCheck check(center, player, range);
        Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(center, found, check);
        Cell::VisitObjects(center, searcher, range);
        std::vector<Unit*> enemies;
        for (Unit* unit : found)
            if (unit->IsAlive() && player->IsValidAttackTarget(unit) && !unit->HasBreakableByDamageCrowdControlAura()
                && center->IsWithinLOSInMap(unit))
                enemies.push_back(unit);
        std::sort(enemies.begin(), enemies.end(), [center](Unit* a, Unit* b)
        {
            return center->GetExactDist(a) < center->GetExactDist(b);
        });
        return enemies;
    }

    void Strike(Player* player, Unit* target, uint32 spellId, float amount)
    {
        int32 points = std::max<int32>(1, int32(amount));
        player->CastCustomSpell(target, spellId, &points, nullptr, nullptr, true);
    }

    void HealPct(Player* player, float pct)
    {
        int32 amount = std::max<int32>(1, int32(player->CountPctFromMaxHealth(int32(pct * 100.0f)) / 100));
        player->CastCustomSpell(player, SPELL_BLOOD_RECLAMATION, &amount, nullptr, nullptr, true);
    }

    int32 PctOfHealth(Player* player, float pct)
    {
        return std::max<int32>(1, int32(float(player->GetMaxHealth()) * pct / 100.0f));
    }

    // An absorb of the Hero's for ms: amount, or (keepLarger) the larger of it and what is left of the last.
    void Shield(Player* player, uint32 spellId, int32 amount, int32 ms, bool keepLarger)
    {
        if (keepLarger)
            if (AuraEffect* left = player->GetAuraEffect(spellId, EFFECT_0, player->GetGUID()))
                amount = std::max(amount, left->GetAmount());
        player->RemoveAurasDueToSpell(spellId, player->GetGUID());
        player->CastCustomSpell(player, spellId, &amount, nullptr, nullptr, true);
        if (Aura* aura = player->GetAura(spellId, player->GetGUID()))
        {
            aura->SetMaxDuration(ms);
            aura->SetDuration(ms);
        }
    }

    // A timed aura of the Hero's on the unit (bp: its first effect's amount, 0 the spell's own).
    void Timed(Player* player, Unit* unit, uint32 spellId, int32 ms, int32 bp = 0)
    {
        unit->RemoveAurasDueToSpell(spellId, player->GetGUID());
        if (bp)
            player->CastCustomSpell(unit, spellId, &bp, nullptr, nullptr, true);
        else
            player->CastSpell(unit, spellId, true);
        if (Aura* aura = unit->GetAura(spellId, player->GetGUID()))
        {
            aura->SetMaxDuration(ms);
            aura->SetDuration(ms);
        }
    }

    float AttackPower(Player* player)
    {
        return player->GetTotalAttackPowerValue(BASE_ATTACK);
    }

    float SpellPower(Player* player, SpellSchoolMask school)
    {
        return float(player->SpellBaseDamageBonusDone(school));
    }

    // ---- Red Communion ------------------------------------------------------------------------------------------

    float LinkRange(Player* player)
    {
        return Communion(player, MOD_LONG_VEINS) ? LONG_LINK_RANGE : LINK_RANGE;
    }

    // The Hero's live links (the dead, gone or expired dropped).
    std::vector<Unit*> LiveLinks(Player* player)
    {
        TankState& state = StateOf(player);
        std::vector<Unit*> links;
        state.Links.erase(std::remove_if(state.Links.begin(), state.Links.end(), [&](Link const& link)
        {
            Unit* unit = ObjectAccessor::GetUnit(*player, link.Guid);
            if (!unit || !unit->IsAlive() || !unit->IsInMap(player)
                || !unit->HasAura(SPELL_CRIMSON_LINK, player->GetGUID()))
                return true;
            links.push_back(unit);
            return false;
        }), state.Links.end());
        return links;
    }

    // The links within operating range, the one given left out.
    std::vector<Unit*> LinksInRange(Player* player, Unit* except)
    {
        std::vector<Unit*> near;
        float range = LinkRange(player);
        for (Unit* unit : LiveLinks(player))
            if (unit != except && player->IsWithinDistInMap(unit, range))
                near.push_back(unit);
        return near;
    }

    bool IsLinked(Player* player, Unit* unit)
    {
        return unit && unit->HasAura(SPELL_CRIMSON_LINK, player->GetGUID());
    }

    // Blood Boil's targets: the selected one first, then the nearest; each linked (or refreshed) up to the cap, a
    // full set replacing its oldest link other than the selected enemy.
    void LinkTargets(Player* player, std::list<WorldObject*> const& targets)
    {
        TankState& state = StateOf(player);
        uint32 cap = Communion(player, MOD_BLOODBOUND_COURT) ? COURT_LINKS : LINKS;
        int32 ms = Communion(player, MOD_LONG_VEINS) ? LONG_LINK_MS : LINK_MS;
        ObjectGuid selected = player->GetTarget();
        std::vector<Unit*> order;
        for (WorldObject* object : targets)
            if (Unit* unit = object->ToUnit(); unit && unit->IsAlive())
                order.push_back(unit);
        std::stable_sort(order.begin(), order.end(), [&](Unit* a, Unit* b)
        {
            if ((a->GetGUID() == selected) != (b->GetGUID() == selected))
                return a->GetGUID() == selected;
            return player->GetExactDist(a) < player->GetExactDist(b);
        });
        LiveLinks(player);
        for (Unit* unit : order)
        {
            bool known = std::any_of(state.Links.begin(), state.Links.end(),
                [unit](Link const& link) { return link.Guid == unit->GetGUID(); });
            if (!known && state.Links.size() >= cap)
            {
                auto oldest = state.Links.end();
                for (auto itr = state.Links.begin(); itr != state.Links.end(); ++itr)
                    if (itr->Guid != selected && (oldest == state.Links.end() || itr->At < oldest->At))
                        oldest = itr;
                if (oldest == state.Links.end())
                    continue;
                if (Unit* old = ObjectAccessor::GetUnit(*player, oldest->Guid))
                    old->RemoveAurasDueToSpell(SPELL_CRIMSON_LINK, player->GetGUID());
                state.Links.erase(oldest);
            }
            Timed(player, unit, SPELL_CRIMSON_LINK, ms);
            if (!known)
                state.Links.push_back({ unit->GetGUID(), Now() });
        }
    }

    // Arterial Echo on the victim: 35% of attack power over 4 sec, a refresh keeping what is unpaid (up to 200% of a
    // fresh one in all).
    void ArterialEcho(Player* player, Unit* victim)
    {
        float fresh = AttackPower(player) * ARTERIAL_AP;
        float unpaid = 0.0f;
        if (AuraEffect* dot = victim->GetAuraEffect(SPELL_ARTERIAL_ECHO, EFFECT_0, player->GetGUID()))
        {
            int32 left = dot->GetTotalTicks() - int32(dot->GetTickNumber());
            unpaid = float(std::max(0, left)) * float(dot->GetAmount());
        }
        float total = std::min(fresh + unpaid, fresh * ARTERIAL_CARRY_CAP);
        Timed(player, victim, SPELL_ARTERIAL_ECHO, ARTERIAL_MS, std::max(1, int32(total / ARTERIAL_TICKS)));
    }

    // An original Blood Strike hit on a linked enemy: the reclamation and the pulse (once every 2 sec), Arterial Echo.
    void BloodStrike(Player* player, Unit* victim)
    {
        if (!player->HasAura(SPELL_RUNE_RED_COMMUNION) || !IsLinked(player, victim))
            return;
        if (Communion(player, MOD_ARTERIAL_ECHO))
            ArterialEcho(player, victim);
        TankState& state = StateOf(player);
        if (Now() - state.LastReclaim < RECLAIM_EVERY_MS)
            return;
        state.LastReclaim = Now();
        std::vector<Unit*> others = LinksInRange(player, victim);
        bool feast = Communion(player, MOD_CRIMSON_FEAST);
        float pct = std::min((feast ? FEAST_PCT : RECLAIM_PCT) + RECLAIM_PER_LINK * float(others.size()),
            feast ? FEAST_CAP : RECLAIM_CAP);
        HealPct(player, pct);
        float pulse = AttackPower(player) * PULSE_AP;
        for (Unit* other : others)
        {
            Strike(player, other, SPELL_COMMUNION_PULSE, pulse);
            if (Communion(player, MOD_SHARED_WEAKNESS))
                Timed(player, other, SPELL_SHARED_WEAKNESS, WEAKNESS_MS);
        }
    }

    // Sanguine Recall: each link in range struck, the absorb; Blood for Blood's heal.
    void SanguineRecall(Player* player)
    {
        std::vector<Unit*> links = LinksInRange(player, nullptr);
        if (links.empty())
            return;
        bool blood = Communion(player, MOD_BLOOD_FOR_BLOOD);
        float damage = AttackPower(player) * (blood ? BLOOD_FOR_BLOOD_AP : RECALL_AP);
        for (Unit* link : links)
            Strike(player, link, SPELL_RECALL_STRIKE, damage);
        bool throne = Communion(player, MOD_COAGULATED_THRONE);
        float pct = std::min((throne ? THRONE_ABSORB : RECALL_ABSORB) + RECALL_PER_LINK * float(links.size() - 1),
            throne ? THRONE_CAP : RECALL_CAP);
        Shield(player, SPELL_SANGUINE_WARD, PctOfHealth(player, pct), RECALL_MS, false);
        if (blood)
            HealPct(player, BLOOD_FOR_BLOOD_HEAL);
        if (Communion(player, MOD_RAPID_RECALL))
            player->ModifySpellCooldown(SPELL_SANGUINE_RECALL, -RAPID_RECALL_MS);
    }

    // ---- Primal Worldwarden -------------------------------------------------------------------------------------

    Creature* Totem(Player* player)
    {
        TankState& state = StateOf(player);
        if (!state.Totem)
            return nullptr;
        Creature* totem = ObjectAccessor::GetCreature(*player, state.Totem);
        if (!totem || !totem->IsAlive() || !totem->IsInMap(player))
        {
            state.Totem.Clear();
            return nullptr;
        }
        return totem;
    }

    void DismissTotem(Player* player)
    {
        if (Creature* totem = Totem(player))
            if (TempSummon* summon = totem->ToTempSummon())
                summon->UnSummon();
        TankState& state = StateOf(player);
        state.Totem.Clear();
        state.MagmaTicks = 0;
        player->RemoveAurasDueToSpell(SPELL_WORLDGUARD);
    }

    Position BehindHero(Player* player)
    {
        Position at = player->GetPosition();
        float angle = player->GetOrientation() + float(M_PI);
        at.m_positionX += std::cos(angle) * TOTEM_FOLLOW;
        at.m_positionY += std::sin(angle) * TOTEM_FOLLOW;
        player->UpdateGroundPositionZ(at.m_positionX, at.m_positionY, at.m_positionZ);
        return at;
    }

    void Follow(Player* player, Creature* totem)
    {
        totem->GetMotionMaster()->Clear();
        totem->GetMotionMaster()->MoveFollow(player, TOTEM_FOLLOW, float(M_PI));
    }

    void SummonTotem(Player* player)
    {
        DismissTotem(player);
        TempSummon* totem = player->GetMap()->SummonCreature(NPC_WORLDHEART_TOTEM, BehindHero(player), nullptr,
            TOTEM_MS, player);
        if (!totem)
            return;
        totem->SetFaction(player->GetFaction());
        totem->SetReactState(REACT_PASSIVE);
        totem->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
        totem->SetSpeed(MOVE_RUN, player->GetSpeedRate(MOVE_RUN) * 1.2f);
        Follow(player, totem);
        TankState& state = StateOf(player);
        state.Totem = totem->GetGUID();
        state.AnchorUntil = 0;
    }

    void PlaceTotem(Player* player)
    {
        Creature* totem = Totem(player);
        if (!totem)
        {
            ChatHandler(player->GetSession()).SendSysMessage("Your Worldheart Totem is not out.");
            return;
        }
        Unit* target = player->GetSelectedUnit();
        Position at = target && target != player && player->IsWithinDistInMap(target, PLACE_RANGE)
            && player->IsWithinLOSInMap(target) ? target->GetPosition() : player->GetPosition();
        totem->GetMotionMaster()->Clear();
        totem->NearTeleportTo(at.GetPositionX(), at.GetPositionY(), at.GetPositionZ(), totem->GetOrientation());
        totem->GetMotionMaster()->MoveIdle();
        StateOf(player).AnchorUntil = Now() + ANCHOR_MS;
        if (Worldwarden(player, MOD_STRIDING_HEART))
        {
            player->ModifySpellCooldown(SPELL_PLACE_WORLDHEART, -STRIDING_PLACE_MS);
            Timed(player, player, SPELL_STRIDING_HEART, 3000);
        }
    }

    // A command at the totem: Earth (1), Flame (2) or Frost (3), at share (1 the Shock's own, 0.6 an echo's).
    void Command(Player* player, uint8 command, float share, bool echo)
    {
        Creature* totem = Totem(player);
        if (!totem)
            return;
        bool wide = Worldwarden(player, MOD_WIDE_COMMUNION);
        float radius = wide ? WIDE_RADIUS : COMMAND_RADIUS;
        uint32 cap = wide ? WIDE_TARGETS : COMMAND_TARGETS;
        switch (command)
        {
            case 1:
            {
                bool earthen = Worldwarden(player, MOD_EARTHEN_COMMAND);
                float pct = (earthen ? EARTHEN_ABSORB : EARTH_ABSORB) * share;
                Shield(player, SPELL_EARTHEN_COMMAND, PctOfHealth(player, pct), COMMAND_MS, true);
                break;
            }
            case 2:
            {
                float damage = AttackPower(player) * (Worldwarden(player, MOD_FLAME_DOMINION) ? DOMINION_AP : FLAME_AP)
                    * share;
                std::vector<Unit*> enemies = EnemiesNear(player, totem, radius);
                if (enemies.size() > cap)
                    enemies.resize(cap);
                for (Unit* enemy : enemies)
                    Strike(player, enemy, SPELL_FLAME_COMMAND, damage);
                break;
            }
            case 3:
            {
                bool winter = Worldwarden(player, MOD_WINTER_COMMAND);
                float slow = (winter ? WINTER_SLOW : FROST_SLOW) * share;
                float reduction = (winter ? WINTER_REDUCTION : FROST_REDUCTION) * share;
                std::vector<Unit*> enemies = EnemiesNear(player, totem, radius);
                if (enemies.size() > cap)
                    enemies.resize(cap);
                TankState& state = StateOf(player);
                for (Unit* enemy : enemies)
                {
                    // The stronger slow and reduction of the original and the echo, never both.
                    int32 bp = -int32(slow);
                    if (AuraEffect* have = enemy->GetAuraEffect(SPELL_FROST_COMMAND, EFFECT_0, player->GetGUID()))
                        bp = std::min(bp, have->GetAmount());
                    Timed(player, enemy, SPELL_FROST_COMMAND, COMMAND_MS, bp);
                    FrostHold& hold = state.Frost[enemy->GetGUID()];
                    if (hold.Until < Now() || hold.Pct < reduction)
                        hold.Pct = reduction;
                    hold.Until = Now() + COMMAND_MS;
                }
                break;
            }
            default:
                return;
        }
        if (!echo)
        {
            TankState& state = StateOf(player);
            state.Command = command;
            state.CommandAt = Now();
        }
    }

    void Echo(Player* player)
    {
        TankState& state = StateOf(player);
        uint32 every = Worldwarden(player, MOD_RESONANT_STRIKES) ? RESONANT_EVERY_MS : ECHO_EVERY_MS;
        if (!state.Command || Now() - state.CommandAt > COMMAND_MEMORY_MS || Now() - state.LastEcho < every)
            return;
        state.LastEcho = Now();
        Command(player, state.Command, ECHO_SHARE, true);
    }

    // Lava Lash with the totem out: the magma line (replacing the last) and Stoneblood Lash's heal.
    void LavaLash(Player* player)
    {
        TankState& state = StateOf(player);
        if (Worldwarden(player, MOD_STONEBLOOD_LASH) && Now() - state.LastStoneblood >= STONEBLOOD_EVERY_MS)
        {
            state.LastStoneblood = Now();
            HealPct(player, STONEBLOOD_PCT);
        }
        Creature* totem = Totem(player);
        if (!totem || player->GetExactDist(totem) > MAGMA_RANGE)
            return;
        bool molten = Worldwarden(player, MOD_MOLTEN_CAUSEWAY);
        state.MagmaFrom = player->GetPosition();
        state.MagmaTo = totem->GetPosition();
        state.MagmaTicks = MAGMA_TICKS;
        state.MagmaTimer = 0;
        state.MagmaShare = molten ? MOLTEN_AP : MAGMA_AP;
        state.MagmaHalfWidth = molten ? MOLTEN_HALF_WIDTH : MAGMA_HALF_WIDTH;
        float length = state.MagmaFrom.GetExactDist2d(state.MagmaTo);
        uint32 steps = std::max<uint32>(1, uint32(length / MAGMA_DECAL_STEP));
        for (uint32 i = 0; i <= steps; ++i)
        {
            float t = float(i) / float(steps);
            Position const& from = state.MagmaFrom;
            Position const& to = state.MagmaTo;
            float x = from.GetPositionX() + (to.GetPositionX() - from.GetPositionX()) * t;
            float y = from.GetPositionY() + (to.GetPositionY() - from.GetPositionY()) * t;
            float z = state.MagmaFrom.GetPositionZ();
            player->UpdateGroundPositionZ(x, y, z);
            player->CastSpell(x, y, z, SPELL_MAGMA_GROUND, true);
        }
    }

    float SegmentDistance(Position const& p, Position const& a, Position const& b)
    {
        float dx = b.GetPositionX() - a.GetPositionX(), dy = b.GetPositionY() - a.GetPositionY();
        float length = dx * dx + dy * dy;
        float t = length > 0.0f ? std::clamp(((p.GetPositionX() - a.GetPositionX()) * dx
            + (p.GetPositionY() - a.GetPositionY()) * dy) / length, 0.0f, 1.0f) : 0.0f;
        float x = a.GetPositionX() + dx * t, y = a.GetPositionY() + dy * t;
        return std::hypot(p.GetPositionX() - x, p.GetPositionY() - y);
    }

    void MagmaTick(Player* player)
    {
        TankState& state = StateOf(player);
        --state.MagmaTicks;
        float reach = state.MagmaFrom.GetExactDist2d(state.MagmaTo) / 2.0f + state.MagmaHalfWidth;
        float damage = AttackPower(player) * state.MagmaShare;
        uint32 hit = 0;
        for (Unit* enemy : EnemiesNear(player, player, reach + player->GetExactDist2d(state.MagmaFrom)
            + state.MagmaFrom.GetExactDist2d(state.MagmaTo)))
        {
            if (hit >= MAGMA_TARGETS)
                break;
            if (SegmentDistance(enemy->GetPosition(), state.MagmaFrom, state.MagmaTo) > state.MagmaHalfWidth)
                continue;
            Strike(player, enemy, SPELL_MAGMA_STRIKE, damage);
            ++hit;
        }
    }

    // Every update: the totem's leash and anchor, Worldguard by the shield, the magma line's ticks.
    void WorldwardenTick(Player* player, uint32 diff)
    {
        TankState& state = StateOf(player);
        Creature* totem = Totem(player);
        bool guard = totem && player->GetShield(true) && player->HasAura(SPELL_RUNE_PRIMAL_WORLDWARDEN);
        if (guard)
        {
            int32 pct = -int32(Worldwarden(player, MOD_DEEPSTONE_HEART) ? DEEPSTONE_PCT : WORLDGUARD_PCT);
            AuraEffect* have = player->GetAuraEffect(SPELL_WORLDGUARD, EFFECT_1);
            if (!have || have->GetAmount() != pct)
            {
                player->RemoveAurasDueToSpell(SPELL_WORLDGUARD);
                int32 threat = int32(WORLDGUARD_THREAT);
                player->CastCustomSpell(player, SPELL_WORLDGUARD, &threat, &pct, nullptr, true);
            }
        }
        else if (player->HasAura(SPELL_WORLDGUARD))
            player->RemoveAurasDueToSpell(SPELL_WORLDGUARD);
        if (totem)
        {
            state.FollowTimer += diff;
            if (state.FollowTimer >= 1000)
            {
                state.FollowTimer = 0;
                if (state.AnchorUntil && Now() >= state.AnchorUntil)
                {
                    state.AnchorUntil = 0;
                    Follow(player, totem);
                }
                else if (!state.AnchorUntil && player->GetExactDist(totem) > TOTEM_LEASH)
                {
                    Position at = BehindHero(player);
                    totem->NearTeleportTo(at.GetPositionX(), at.GetPositionY(), at.GetPositionZ(),
                        player->GetOrientation());
                    Follow(player, totem);
                }
            }
        }
        if (state.MagmaTicks)
        {
            state.MagmaTimer += diff;
            if (state.MagmaTimer >= 1000)
            {
                state.MagmaTimer = 0;
                MagmaTick(player);
            }
        }
    }

    // ---- Black Covenant -----------------------------------------------------------------------------------------

    bool Bound(Player* player)
    {
        return player->HasAura(SPELL_COVENANT_ARMOR);
    }

    // The known rank of a chain (0 none).
    uint32 KnownRank(Player* player, uint32 firstRank)
    {
        uint32 known = 0;
        for (SpellInfo const* rank = sSpellMgr->GetSpellInfo(firstRank); rank; rank = rank->GetNextRankSpell())
            if (player->HasSpell(rank->Id))
                known = rank->Id;
        return known;
    }

    // Summon Voidwalker's buttons become Bind: Voidwalker's while the rune is bound, or back.
    void SwapSummon(Player* player, bool rune)
    {
        bool swapped = false;
        for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
        {
            ActionButton const* action = player->GetActionButton(button);
            if (!action || action->GetType() != ACTION_BUTTON_SPELL)
                continue;
            uint32 spellId = action->GetAction();
            if (rune && spellId == SPELL_SUMMON_VOIDWALKER)
                player->addActionButton(button, SPELL_BIND_GUARDIAN, ACTION_BUTTON_SPELL);
            else if (!rune && spellId == SPELL_BIND_GUARDIAN && player->HasSpell(SPELL_SUMMON_VOIDWALKER))
                player->addActionButton(button, SPELL_SUMMON_VOIDWALKER, ACTION_BUTTON_SPELL);
            else
                continue;
            swapped = true;
        }
        if (swapped)
            player->SendActionButtons(1);
    }

    // Blood Boil's buttons become Red Communion's Blood Boil (Rage, not a rune: the client checks the rune itself),
    // or back to the rank the Hero knows.
    void SwapBoil(Player* player, bool communion)
    {
        uint32 known = KnownRank(player, SPELL_BLOOD_BOIL);
        bool swapped = false;
        for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
        {
            ActionButton const* action = player->GetActionButton(button);
            if (!action || action->GetType() != ACTION_BUTTON_SPELL)
                continue;
            uint32 spellId = action->GetAction();
            if (communion && FirstRank(spellId) == SPELL_BLOOD_BOIL)
                player->addActionButton(button, SPELL_COMMUNION_BOIL, ACTION_BUTTON_SPELL);
            else if (!communion && spellId == SPELL_COMMUNION_BOIL && known)
                player->addActionButton(button, known, ACTION_BUTTON_SPELL);
            else
                continue;
            swapped = true;
        }
        if (swapped)
            player->SendActionButtons(1);
    }

    // Searing Pain's and Drain Life's buttons become Covenant Lash's and Soul Siphon's (bound; Soul Siphon only for
    // a Hero who knows Drain Life), or back.
    void SwapButtons(Player* player, bool bound)
    {
        uint32 searing = KnownRank(player, SPELL_SEARING_PAIN), drain = KnownRank(player, SPELL_DRAIN_LIFE);
        bool swapped = false;
        for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
        {
            ActionButton const* action = player->GetActionButton(button);
            if (!action || action->GetType() != ACTION_BUTTON_SPELL)
                continue;
            uint32 spellId = action->GetAction();
            uint32 to = 0;
            if (bound && FirstRank(spellId) == SPELL_SEARING_PAIN)
                to = SPELL_COVENANT_LASH;
            else if (bound && FirstRank(spellId) == SPELL_DRAIN_LIFE && player->HasSpell(SPELL_SOUL_SIPHON))
                to = SPELL_SOUL_SIPHON;
            else if (!bound && spellId == SPELL_COVENANT_LASH)
                to = searing;
            else if (!bound && spellId == SPELL_SOUL_SIPHON)
                to = drain;
            if (to)
            {
                player->addActionButton(button, to, ACTION_BUTTON_SPELL);
                swapped = true;
            }
        }
        if (swapped)
            player->SendActionButtons(1);
    }

    // Health kept at the same share across a change of maximum health.
    template <typename F>
    void KeepHealthShare(Player* player, F change)
    {
        float share = player->GetHealthPct();
        change();
        player->SetHealth(std::max<uint32>(1, player->CountPctFromMaxHealth(int32(share))));
    }

    // The pact: its threat, Nether Insulation's magic reduction and Voidforged Plate's armor (by the Hero's level).
    void Pact(Player* player)
    {
        player->RemoveAurasDueToSpell(SPELL_COVENANT_PACT);
        int32 threat = PACT_THREAT;
        int32 magic = Covenant(player, MOD_NETHER_INSULATION) ? -INSULATION_PCT : 0;
        int32 level = int32(std::min<uint32>(player->GetLevel(), VOIDFORGED_LEVEL));
        int32 armor = Covenant(player, MOD_VOIDFORGED_PLATE) ? VOIDFORGED_ARMOR * level / int32(VOIDFORGED_LEVEL) : 0;
        player->CastCustomSpell(player, SPELL_COVENANT_PACT, &threat, &magic, &armor, true);
    }

    void Unbind(Player* player)
    {
        KeepHealthShare(player, [player]()
        {
            player->RemoveAurasDueToSpell(SPELL_COVENANT_ARMOR);
            player->RemoveAurasDueToSpell(SPELL_COVENANT_PACT);
        });
        SwapButtons(player, false);
        for (uint32 spellId : { SPELL_COVENANT_LASH, SPELL_SOUL_SIPHON })
            if (player->HasSpell(spellId))
                player->removeSpell(spellId, SPEC_MASK_ALL, false);
        StateOf(player).Bound = false;
    }

    void Bind(Player* player)
    {
        if (Bound(player))
        {
            Unbind(player);
            return;
        }
        // A major tank form (Bear, Dire Bear, Metamorphosis) and the armor never go together; a stance is no form.
        ShapeshiftForm form = player->GetShapeshiftForm();
        if (form == FORM_BEAR || form == FORM_DIREBEAR || form == FORM_METAMORPHOSIS)
        {
            ChatHandler(player->GetSession()).SendSysMessage("Covenant Armor cannot be worn with another form.");
            return;
        }
        // The Voidwalker comes from the void into the armor; a pet already out (the Hero's Voidwalker or another)
        // is dismissed.
        if (Pet* pet = player->GetPet())
            player->RemovePet(pet, PET_SAVE_NOT_IN_SLOT);
        KeepHealthShare(player, [player]()
        {
            int32 armor = ARMOR_PCT;
            player->CastCustomSpell(player, SPELL_COVENANT_ARMOR, &armor, nullptr, nullptr, true);
            Pact(player);
        });
        if (!player->HasSpell(SPELL_COVENANT_LASH))
            player->learnSpell(SPELL_COVENANT_LASH);
        if (KnownRank(player, SPELL_DRAIN_LIFE) && !player->HasSpell(SPELL_SOUL_SIPHON))
            player->learnSpell(SPELL_SOUL_SIPHON);
        SwapButtons(player, true);
        StateOf(player).Bound = true;
    }

    // Covenant Lash's hit: 45% of spell power as Shadowflame with 100% more threat (on top of the pact's); during
    // Intercession its cleave; Hungry Covenant's mana.
    void Lash(Player* player, Unit* victim)
    {
        if (!Bound(player))
            return;
        float damage = SpellPower(player, SpellSchoolMask(SPELL_SCHOOL_MASK_SHADOW | SPELL_SCHOOL_MASK_FIRE))
            * LASH_SP;
        auto hit = [&](Unit* enemy, float amount)
        {
            Strike(player, enemy, SPELL_LASH_STRIKE, amount);
            if (enemy->IsAlive() && enemy->CanHaveThreatList())
                enemy->GetThreatMgr().AddThreat(player, amount);
        };
        hit(victim, damage);
        if (player->HasAura(SPELL_INTERCESSION))
        {
            bool rebuke = Covenant(player, MOD_LEGIONS_REBUKE);
            uint32 cap = rebuke ? REBUKE_TARGETS : LASH_CLEAVE_TARGETS;
            std::vector<Unit*> enemies = EnemiesNear(player, victim, LASH_CLEAVE_RANGE);
            enemies.erase(std::remove(enemies.begin(), enemies.end(), victim), enemies.end());
            if (enemies.size() > cap)
                enemies.resize(cap);
            for (Unit* enemy : enemies)
                hit(enemy, damage * (rebuke ? REBUKE_CLEAVE : LASH_CLEAVE));
        }
        if (Covenant(player, MOD_LASHING_RHYTHM))
            player->ModifySpellCooldown(SPELL_SOUL_SIPHON, -RHYTHM_MS);
        TankState& state = StateOf(player);
        if (Covenant(player, MOD_HUNGRY_COVENANT) && Now() - state.LastHungry >= HUNGRY_EVERY_MS)
        {
            state.LastHungry = Now();
            player->ModifyPower(POWER_MANA, int32(float(player->GetCreateMana()) * HUNGRY_MANA_PCT / 100.0f));
        }
    }

    // A Soul Siphon tick on its victim: damage, and (it dealt damage) the heal.
    void SiphonTick(Player* player, Unit* victim)
    {
        if (!victim->IsAlive() || victim->IsImmunedToDamage(SPELL_SCHOOL_MASK_SHADOW))
            return;
        Strike(player, victim, SPELL_SIPHON_STRIKE, SpellPower(player, SPELL_SCHOOL_MASK_SHADOW)
            * (Covenant(player, MOD_RAVENOUS_SIPHON) ? RAVENOUS_SP : SIPHON_SP));
        int32 heal = PctOfHealth(player, Covenant(player, MOD_DEEP_EXTRACTION) ? DEEP_HEAL : SIPHON_HEAL);
        player->CastCustomSpell(player, SPELL_SIPHON_HEAL, &heal, nullptr, nullptr, true);
    }

    void Intercession(Player* player)
    {
        int32 pct = -(Covenant(player, MOD_GUARDIAN_MANIFEST) ? MANIFEST_PCT : INTERCESSION_PCT);
        Timed(player, player, SPELL_INTERCESSION, INTERCESSION_MS, pct);
        if (Covenant(player, MOD_FREQUENT_PETITION))
        {
            player->ModifySpellCooldown(SPELL_DARK_INTERCESSION, -PETITION_CUT_MS);
            HealPct(player, PETITION_HEAL);
        }
    }

    // ---- Damage the Hero takes ----------------------------------------------------------------------------------

    // The share of an enemy's damage a Hero with these runes takes: Sovereign Bond, Shared Weakness, the Frost
    // command, the curses.
    float TakenShare(Unit* target, Unit* attacker, SpellSchoolMask school)
    {
        Player* player = target ? target->ToPlayer() : nullptr;
        if (!player || !attacker || attacker == target)
            return 1.0f;
        float share = 1.0f;
        bool magic = (school & SPELL_SCHOOL_MASK_MAGIC) != 0;
        if (player->HasAura(SPELL_RUNE_RED_COMMUNION))
        {
            if (Communion(player, MOD_SOVEREIGN_BOND) && attacker->GetGUID() == player->GetTarget()
                && IsLinked(player, attacker) && player->IsWithinDistInMap(attacker, LinkRange(player)))
                share *= SOVEREIGN_SHARE;
            if (magic && attacker->HasAura(SPELL_SHARED_WEAKNESS, player->GetGUID()))
                share *= SHARED_WEAKNESS_SHARE;
        }
        if (player->HasAura(SPELL_RUNE_PRIMAL_WORLDWARDEN))
        {
            TankState& state = StateOf(player);
            auto hold = state.Frost.find(attacker->GetGUID());
            if (hold != state.Frost.end())
            {
                if (hold->second.Until >= Now())
                    share *= 1.0f - hold->second.Pct / 100.0f;
                else
                    state.Frost.erase(hold);
            }
        }
        if (player->HasAura(SPELL_RUNE_BLACK_COVENANT))
        {
            if (!magic && HasRankAura(attacker, SPELL_CURSE_OF_WEAKNESS, player->GetGUID()))
                share *= CURSE_SHARE;
            if (magic && HasRankAura(attacker, SPELL_CURSE_OF_TONGUES, player->GetGUID()))
                share *= CURSE_SHARE;
        }
        return share;
    }

    template <typename T>
    void Scale(T& amount, float share)
    {
        if (share < 1.0f && amount > 0)
            amount = std::max<T>(1, T(float(amount) * share + 0.5f));
    }

    // ---- Binding ------------------------------------------------------------------------------------------------

    void Teach(Player* player, uint32 spellId, bool on)
    {
        if (on && !player->HasSpell(spellId))
            player->learnSpell(spellId);
        else if (!on && player->HasSpell(spellId))
            player->removeSpell(spellId, SPEC_MASK_ALL, false);
    }
}

namespace TankRunes
{
    void Apply(Player* player)
    {
        bool communion = player->HasAura(SPELL_RUNE_RED_COMMUNION);
        bool worldwarden = player->HasAura(SPELL_RUNE_PRIMAL_WORLDWARDEN);
        bool covenant = player->HasAura(SPELL_RUNE_BLACK_COVENANT);
        bool any = communion || worldwarden || covenant;
        Teach(player, SPELL_CHALLENGE, communion || worldwarden);
        if (any && !player->HasAura(SPELL_TANK_FOUNDATION))
            player->AddAura(SPELL_TANK_FOUNDATION, player);
        else if (!any)
            player->RemoveAurasDueToSpell(SPELL_TANK_FOUNDATION);
        Teach(player, SPELL_SANGUINE_RECALL, communion);
        bool boil = communion && KnownRank(player, SPELL_BLOOD_BOIL);
        if (!boil)
            SwapBoil(player, false);
        Teach(player, SPELL_COMMUNION_BOIL, boil);
        if (boil)
            SwapBoil(player, true);
        if (!communion)
            for (Unit* link : LiveLinks(player))
                link->RemoveAurasDueToSpell(SPELL_CRIMSON_LINK, player->GetGUID());
        Teach(player, SPELL_WORLDHEART_TOTEM, worldwarden);
        Teach(player, SPELL_PLACE_WORLDHEART, worldwarden);
        if (!worldwarden)
            DismissTotem(player);
        if (!covenant)
            SwapSummon(player, false);
        Teach(player, SPELL_BIND_GUARDIAN, covenant);
        Teach(player, SPELL_DARK_INTERCESSION, covenant);
        if (covenant)
            SwapSummon(player, true);
        if (!covenant && (Bound(player) || player->HasSpell(SPELL_COVENANT_LASH)))
            Unbind(player);
        else if (covenant && Bound(player))
            Pact(player);                   // a level or a modifier changed: the pact's numbers again
    }
}

// Blood Boil (every rank) with Red Communion: the enemies it hits are linked. Cast through Red Communion's own Blood
// Boil (SPELL_COMMUNION_BOIL, which pays its Rage and cooldown); cast directly (a macro), it costs BLOOD_BOIL_RAGE
// Rage and has a BLOOD_BOIL_COOLDOWN_MS cooldown instead of its Blood rune (the core waives the rune:
// Spell::RunesWaived).
class spell_titan_tank_blood_boil : public SpellScript
{
    PrepareSpellScript(spell_titan_tank_blood_boil);

    SpellCastResult Check()
    {
        Player* player = GetCaster()->ToPlayer();
        if (player && !GetSpell()->IsTriggered() && player->HasAura(SPELL_RUNE_RED_COMMUNION)
            && player->GetPower(POWER_RAGE) < BLOOD_BOIL_RAGE * 10)
            return SPELL_FAILED_NO_POWER;
        return SPELL_CAST_OK;
    }

    void Paid()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player || GetSpell()->IsTriggered() || !player->HasAura(SPELL_RUNE_RED_COMMUNION))
            return;
        player->ModifyPower(POWER_RAGE, -BLOOD_BOIL_RAGE * 10);
        player->AddSpellCooldown(GetSpellInfo()->Id, 0, BLOOD_BOIL_COOLDOWN_MS, true);
    }

    void Link(std::list<WorldObject*>& targets)
    {
        if (Player* player = GetCaster()->ToPlayer(); player && player->HasAura(SPELL_RUNE_RED_COMMUNION))
            LinkTargets(player, targets);
    }

    void Register() override
    {
        // Blood Boil's damage picks enemies about a point at the caster (TARGET_UNIT_DEST_AREA_ENEMY, 16), not
        // about the caster itself: a handler on the source's area never ran.
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_titan_tank_blood_boil::Link, EFFECT_0,
            TARGET_UNIT_DEST_AREA_ENEMY);
        OnCheckCast += SpellCheckCastFn(spell_titan_tank_blood_boil::Check);
        AfterCast += SpellCastFn(spell_titan_tank_blood_boil::Paid);
    }
};

// Blood Strike (every rank) with Red Communion: an original hit on a linked enemy reclaims blood.
class spell_titan_tank_blood_strike : public SpellScript
{
    PrepareSpellScript(spell_titan_tank_blood_strike);

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (player && target && !GetSpell()->IsTriggered() && target->IsAlive())
            BloodStrike(player, target);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_tank_blood_strike::Hit);
    }
};

// Earth, Flame and Frost Shock (every rank) with Primal Worldwarden: an original Shock commands the totem.
class spell_titan_tank_shock : public SpellScript
{
    PrepareSpellScript(spell_titan_tank_shock);

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player || GetSpell()->IsTriggered() || !player->HasAura(SPELL_RUNE_PRIMAL_WORLDWARDEN))
            return;
        uint32 first = GetSpellInfo()->GetFirstRankSpell()->Id;
        uint8 command = first == SPELL_EARTH_SHOCK ? 1 : first == SPELL_FLAME_SHOCK ? 2 : 3;
        Command(player, command, 1.0f, false);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_tank_shock::Hit);
    }
};

// Stormstrike with Primal Worldwarden: an original hit echoes the last command.
class spell_titan_tank_stormstrike : public SpellScript
{
    PrepareSpellScript(spell_titan_tank_stormstrike);

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        if (player && !GetSpell()->IsTriggered() && player->HasAura(SPELL_RUNE_PRIMAL_WORLDWARDEN))
            Echo(player);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_tank_stormstrike::Hit);
    }
};

// Lava Lash (every rank) with Primal Worldwarden: Magma Lash's line, Stoneblood Lash's heal.
class spell_titan_tank_lava_lash : public SpellScript
{
    PrepareSpellScript(spell_titan_tank_lava_lash);

    void Hit()
    {
        Player* player = GetCaster()->ToPlayer();
        if (player && !GetSpell()->IsTriggered() && player->HasAura(SPELL_RUNE_PRIMAL_WORLDWARDEN))
            LavaLash(player);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_tank_lava_lash::Hit);
    }
};

// The collection's taught actions: Sanguine Recall, Worldheart Totem, Place Worldheart, Bind: Voidwalker, Covenant
// Lash and Dark Intercession.
class spell_titan_tank_action : public SpellScript
{
    PrepareSpellScript(spell_titan_tank_action);

    SpellCastResult Check()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return SPELL_FAILED_DONT_REPORT;
        if (GetSpellInfo()->Id == SPELL_SANGUINE_RECALL && LinksInRange(player, nullptr).empty())
            return SPELL_FAILED_NO_VALID_TARGETS;
        if (GetSpellInfo()->Id == SPELL_COVENANT_LASH && !Bound(player))
            return SPELL_FAILED_NOT_READY;
        if (GetSpellInfo()->Id == SPELL_COMMUNION_BOIL && !KnownRank(player, SPELL_BLOOD_BOIL))
            return SPELL_FAILED_NOT_READY;
        return SPELL_CAST_OK;
    }

    void Act(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return;
        switch (GetSpellInfo()->Id)
        {
            case SPELL_SANGUINE_RECALL: SanguineRecall(player); break;
            case SPELL_WORLDHEART_TOTEM: SummonTotem(player); break;
            case SPELL_PLACE_WORLDHEART: PlaceTotem(player); break;
            case SPELL_BIND_GUARDIAN: Bind(player); break;
            case SPELL_DARK_INTERCESSION: Intercession(player); break;
            case SPELL_COMMUNION_BOIL:      // the Hero's Blood Boil, already paid for
                if (uint32 rank = KnownRank(player, SPELL_BLOOD_BOIL))
                    player->CastSpell(player, rank, TriggerCastFlags(TRIGGERED_IGNORE_GCD
                        | TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_IGNORE_POWER_AND_REAGENT_COST));
                break;
            case SPELL_COVENANT_LASH:
                if (Unit* target = GetHitUnit())
                    Lash(player, target);
                break;
            default: break;
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_titan_tank_action::Check);
        OnEffectHitTarget += SpellEffectFn(spell_titan_tank_action::Act, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Soul Siphon's channel: its ticks, and the Hero slowed to 70% while it lasts.
class spell_titan_tank_siphon : public AuraScript
{
    PrepareAuraScript(spell_titan_tank_siphon);

    void Tick(AuraEffect const* /*effect*/)
    {
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (player && GetTarget())
            SiphonTick(player, GetTarget());
    }

    void Apply(AuraEffect const* /*effect*/, AuraEffectHandleModes /*mode*/)
    {
        if (Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr)
            player->CastSpell(player, SPELL_SIPHON_SLOW, true);
    }

    void Remove(AuraEffect const* /*effect*/, AuraEffectHandleModes /*mode*/)
    {
        if (Unit* caster = GetCaster())
            caster->RemoveAurasDueToSpell(SPELL_SIPHON_SLOW);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_titan_tank_siphon::Tick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        AfterEffectApply += AuraEffectApplyFn(spell_titan_tank_siphon::Apply, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY,
            AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_titan_tank_siphon::Remove, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY,
            AURA_EFFECT_HANDLE_REAL);
    }
};

// Soul Siphon only while bound.
class spell_titan_tank_siphon_cast : public SpellScript
{
    PrepareSpellScript(spell_titan_tank_siphon_cast);

    SpellCastResult Check()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player || !Bound(player))
            return SPELL_FAILED_NOT_READY;
        return SPELL_CAST_OK;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_titan_tank_siphon_cast::Check);
    }
};

// The runes' timers (the totem, Worldguard, the magma line); a pet summoned while bound ends Covenant Armor; the
// state dropped at logout.
class titan_tank_runes_player : public PlayerScript
{
public:
    titan_tank_runes_player() : PlayerScript("titan_tank_runes_player", {
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_PLAYER_JUST_DIED
    }) { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        auto itr = states.find(player->GetGUID().GetCounter());
        if (itr == states.end())
            return;
        WorldwardenTick(player, diff);
        if (itr->second.Bound && (!Bound(player) || player->GetPet()))
            Unbind(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        DismissTotem(player);
        states.erase(player->GetGUID().GetCounter());
    }

    // Death removes the runes' temporary effects.
    void OnPlayerJustDied(Player* player) override
    {
        if (states.find(player->GetGUID().GetCounter()) == states.end())
            return;
        TankState& state = StateOf(player);
        state.MagmaTicks = 0;
        state.Frost.clear();
        state.Command = 0;
    }
};

// The damage enemies deal a Hero with these runes (TakenShare).
class titan_tank_runes_unit : public UnitScript
{
public:
    titan_tank_runes_unit() : UnitScript("titan_tank_runes_unit", true, {
        UNITHOOK_MODIFY_MELEE_DAMAGE,
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK
    }) { }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        if (target && target->IsPlayer() && attacker)
            Scale(damage, TakenShare(target, attacker, attacker->GetMeleeDamageSchoolMask()));
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (target && target->IsPlayer() && spellInfo)
            Scale(damage, TakenShare(target, attacker, spellInfo->GetSchoolMask()));
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* spellInfo)
        override
    {
        if (!target || !target->IsPlayer() || !spellInfo || spellInfo->HasAura(SPELL_AURA_PERIODIC_HEAL)
            || spellInfo->HasAura(SPELL_AURA_OBS_MOD_HEALTH))
            return;
        Scale(damage, TakenShare(target, attacker, spellInfo->GetSchoolMask()));
    }
};

void AddSC_tank_runes()
{
    RegisterSpellScript(spell_titan_tank_blood_boil);
    RegisterSpellScript(spell_titan_tank_blood_strike);
    RegisterSpellScript(spell_titan_tank_shock);
    RegisterSpellScript(spell_titan_tank_stormstrike);
    RegisterSpellScript(spell_titan_tank_lava_lash);
    RegisterSpellScript(spell_titan_tank_action);
    RegisterSpellScript(spell_titan_tank_siphon);
    RegisterSpellScript(spell_titan_tank_siphon_cast);
    new titan_tank_runes_player();
    new titan_tank_runes_unit();
}
