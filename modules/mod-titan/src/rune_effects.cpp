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

#include "rune_tiers.h"
#include "runes.h"
#include "CellImpl.h"
#include "DynamicObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include <algorithm>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Runes of Enhancement: the Epic and Rare runes' own effects (rune_tiers.cpp holds, offers and applies the runes;
// tools/rune_tiers/runes.py words them): every class's.
//
// - Lasting spell modifiers and auras (Mods, Auras): applied by rune_tiers.cpp while a rune is held and awake.
// - Cooldowns cut just after the cast, server and client alike (TRIMS).
// - Usability a rune lifts (Revenge without a block, Execute at 35%, Hammer of Wrath at any health, Deep Freeze on
//   any target, Ambush out of Stealth, Backstab from the front): the server's copy of the spell loses the rule at load
//   (the client's too: patch_spells.py rune_usability) and titan_rune_effects_checks puts it back for everyone else.
// - On-hit extras (spell_titan_rune_tier_hit, by each spell's first rank), auras that spread or break
//   (spell_titan_rune_tier_spread, spell_titan_rune_tier_ward), and the hits, ticks and heals they change.
// Extra hits and heals are the script's own spells (91171-91178), a share of the Hero's hit with its bonuses in it.
namespace
{
    enum RuneIds : uint32
    {
        RUNE_OPEN_GUARD = 1001, RUNE_HEADSMAN, RUNE_UNENDING_STORM, RUNE_RAGING_BLOW, RUNE_AVENGER, RUNE_QUAKING_EARTH,
        RUNE_BEACON, RUNE_INFUSION, RUNE_BULWARKS_TOSS, RUNE_SACRED_WALL, RUNE_TEMPEST, RUNE_AVENGING_HAMMER,
        RUNE_TWIN_FANGS = 1019, RUNE_TOXIC_FINALE, RUNE_ENDLESS_FLURRY, RUNE_ENDLESS_SPREE, RUNE_SHADOW_DANCE,
        RUNE_OPEN_VEIN, RUNE_TWIN_PENANCE, RUNE_UNBROKEN_WARD, RUNE_WIDE_CIRCLE, RUNE_GUARDIAN, RUNE_FINAL_WORD,
        RUNE_ENDLESS_THIRST,
        RUNE_ARCANE_VOLLEY = 1043, RUNE_UNENDING_POWER, RUNE_LIVING_INFERNO, RUNE_PHOENIX, RUNE_GLACIER,
        RUNE_FROZEN_HEART,
        RUNE_LONG_WOUND = 2001, RUNE_BLOODY_SWEEP, RUNE_SHARP_TONGUE, RUNE_WRECKING_CHARGE, RUNE_THUNDERING_BLADE,
        RUNE_RADIANCE, RUNE_HOLY_WRATH, RUNE_SHINING_SHIELD, RUNE_EXORCIST, RUNE_RECKONING_HAMMER,
        RUNE_OPPORTUNIST = 2016, RUNE_VENOMOUS_WOUND, RUNE_SHADOWS_EDGE, RUNE_QUICK_RECOVERY, RUNE_AMBUSHER,
        RUNE_MINDS_ECHO, RUNE_GLIMMERING_SHIELD, RUNE_SEARING_LIGHT, RUNE_FLASHING_GRACE, RUNE_GRASPING_MIND,
        RUNE_KINDLING = 2036, RUNE_FROSTFIRE_BRIDGE, RUNE_CHARGED_BLAST, RUNE_FROZEN_STEP, RUNE_DEEP_CHILL,
        // Hunter, Death Knight, Shaman, Warlock, Druid, Demon Hunter, Monk.
        RUNE_ALPHA = 1013, RUNE_KINDRED_SPIRITS, RUNE_MARKSMAN, RUNE_ENDLESS_BARRAGE, RUNE_BLAST_RADIUS,
        RUNE_DARK_HUNTER,
        RUNE_BLOOD_DEBT = 1031, RUNE_CRIMSON_TIDE, RUNE_TWIN_BLADES, RUNE_ENDLESS_WINTER, RUNE_FESTERING_WOUND,
        RUNE_DEFILE, RUNE_LAVA_SURGE, RUNE_STORMS_REACH, RUNE_ASCENDANCE, RUNE_MOLTEN_EDGE, RUNE_TIDAL_ECHO,
        RUNE_ANCESTRAL_WAVE,
        RUNE_HAUNTING = 1049, RUNE_CONTAGION, RUNE_DEMON_WITHIN, RUNE_MOLTEN_SOUL, RUNE_CHAOS, RUNE_FIRE_AND_BRIMSTONE,
        RUNE_CELESTIAL_FALL, RUNE_LUNAR_STRIKE, RUNE_PRIMAL_WRATH, RUNE_THRASH, RUNE_EVER_BLOOMING, RUNE_BLOOMING_GROVE,
        RUNE_UNENDING_FURY, RUNE_DEATH_SWEEP, RUNE_SOUL_BARRAGE, RUNE_BURNING_BRAND, RUNE_RISING_STORM,
        RUNE_DRAGONS_FURY, RUNE_OVERFLOWING_FONT, RUNE_SILKEN_COCOON,
        RUNE_RAPTORS_EDGE = 2011, RUNE_SERPENTS_KISS, RUNE_STEADY_HAND, RUNE_HUNTERS_LEAP, RUNE_PACK_LEADER,
        RUNE_PLAGUE_BEARER = 2026, RUNE_PLAGUED_BLOOD, RUNE_BLOODY_CROWN, RUNE_KILLING_FROST, RUNE_SHAMBLING_DEAD,
        RUNE_CHAIN_SPARK, RUNE_MOLTEN_FLOW, RUNE_TIDAL_WAVE, RUNE_EARTHEN_GRIP, RUNE_MAELSTROM,
        RUNE_LINGERING_CURSE = 2041, RUNE_BACKDRAFT, RUNE_SIPHONING_SOUL, RUNE_BACKLASH, RUNE_DREAD, RUNE_LUNAR_FALL,
        RUNE_SHREDDER, RUNE_LINGERING_BLOOM, RUNE_THICK_HIDE, RUNE_REJUVENATING_TOUCH, RUNE_FEL_EDGE,
        RUNE_SPREADING_FLAME, RUNE_VENGEFUL_STEP, RUNE_SOUL_FEAST, RUNE_BRANDING_IRON, RUNE_SECOND_BREATH,
        RUNE_TIGERS_CLAW, RUNE_BLACKOUT_COMBO, RUNE_MISTWEAVERS_GRACE, RUNE_WHIRLING_CRANE,
    };

    // Overpower's chance after a dodge, for players (the core's Unit::ProcDamageAndSpell): a game aura state it leaves
    // unused, in place of the combo point it gave before.
    constexpr AuraStateType AURA_STATE_OVERPOWER = AuraStateType(6);

    // First ranks.
    enum Spells : uint32
    {
        SPELL_OVERPOWER = 7384, SPELL_EXECUTE = 5308, SPELL_WHIRLWIND = 1680, SPELL_SLAM = 1464, SPELL_REVENGE = 6572,
        SPELL_SHOCKWAVE = 46968, SPELL_REND = 772, SPELL_CLEAVE = 845, SPELL_SHIELD_SLAM = 23922, SPELL_CHARGE = 100,
        SPELL_CHARGE_STUN = 7922, SPELL_HEROIC_STRIKE = 78, SPELL_THUNDER_CLAP = 6343,
        SPELL_HOLY_LIGHT = 635, SPELL_FLASH_OF_LIGHT = 19750, SPELL_AVENGERS_SHIELD = 31935, SPELL_HOLY_SHIELD = 20925,
        SPELL_DIVINE_STORM = 53385, SPELL_HAMMER_OF_WRATH = 24275, SPELL_EXORCISM = 879, SPELL_HAMMER_OF_JUSTICE = 853,
        SPELL_CRUSADER_STRIKE = 35395, SPELL_CONSECRATION = 26573, SPELL_JUDGEMENT_OF_LIGHT = 20271,
        SPELL_JUDGEMENT_OF_WISDOM = 53408, SPELL_JUDGEMENT_OF_JUSTICE = 53407,
        SPELL_MUTILATE = 1329, SPELL_ENVENOM = 32645, SPELL_BLADE_FLURRY_HIT = 22482, SPELL_KILLING_SPREE = 51690,
        SPELL_AMBUSH = 8676, SPELL_HEMORRHAGE = 16511, SPELL_SINISTER_STRIKE = 1752, SPELL_RUPTURE = 1943,
        SPELL_BACKSTAB = 53, SPELL_EVASION = 5277, SPELL_EVASION_2 = 26669,
        SPELL_PENANCE_DAMAGE = 47758, SPELL_PENANCE_HEAL = 47757, SPELL_WEAKENED_SOUL = 6788,
        SPELL_CIRCLE_OF_HEALING = 34861, SPELL_GUARDIAN_SPIRIT = 47788, SPELL_DEATH_BACKLASH = 32409,
        SPELL_VAMPIRIC_TOUCH = 34914, SPELL_MIND_BLAST = 8092, SPELL_SHADOW_WORD_PAIN = 589,
        SPELL_POWER_WORD_SHIELD = 17, SPELL_HOLY_FIRE = 14914, SPELL_FLASH_HEAL = 2061, SPELL_RENEW = 139,
        SPELL_MIND_FLAY = 15407,
        SPELL_ARCANE_BARRAGE = 44425, SPELL_ARCANE_POWER = 12042, SPELL_LIVING_BOMB = 44457, SPELL_COMBUSTION = 11129,
        SPELL_DEEP_FREEZE = 44572, SPELL_ICY_VEINS = 12472, SPELL_FIREBALL = 133, SPELL_FIRE_BLAST = 2136,
        SPELL_FROSTBOLT = 116, SPELL_ARCANE_BLAST_STACK = 36032, SPELL_BLINK = 1953, SPELL_FROST_NOVA = 122,
        SPELL_CONE_OF_COLD = 120,
        SPELL_INTIMIDATION = 19577, SPELL_MEND_PET = 136, SPELL_AIMED_SHOT = 19434, SPELL_MULTI_SHOT = 2643,
        SPELL_EXPLOSIVE_SHOT_HIT = 53352, SPELL_BLACK_ARROW = 3674, SPELL_RAPTOR_STRIKE = 2973,
        SPELL_ARCANE_SHOT = 3044,
        SPELL_SERPENT_STING = 1978, SPELL_STEADY_SHOT = 56641, SPELL_DISENGAGE = 781, SPELL_KILL_COMMAND = 34026,
        SPELL_DEATH_STRIKE = 49998, SPELL_DEATH_STRIKE_HEAL = 45470, SPELL_VAMPIRIC_BLOOD = 55233,
        SPELL_OBLITERATE = 49020, SPELL_HOWLING_BLAST = 49184, SPELL_SCOURGE_STRIKE = 55090,
        SPELL_DEATH_AND_DECAY = 43265, SPELL_BLOOD_BOIL = 48721, SPELL_HEART_STRIKE = 55050,
        SPELL_RAISE_DEAD_GHOUL = 46585, SPELL_RAISE_DEAD_PET = 52150, SPELL_FROST_FEVER = 55095,
        SPELL_BLOOD_PLAGUE = 55078,
        SPELL_LAVA_BURST = 51505, SPELL_CHAIN_LIGHTNING = 421, SPELL_STORMSTRIKE_MAIN = 32175,
        SPELL_STORMSTRIKE_OFF = 32176, SPELL_LAVA_LASH = 60103, SPELL_FLAME_SHOCK = 8050, SPELL_RIPTIDE = 61295,
        SPELL_HEALING_WAVE = 331, SPELL_LIGHTNING_BOLT = 403, SPELL_EARTH_SHOCK = 8042,
        SPELL_HAUNT = 48181, SPELL_UNSTABLE_AFFLICTION = 30108, SPELL_METAMORPHOSIS_WARLOCK = 47241,
        SPELL_SOUL_FIRE = 6353, SPELL_CHAOS_BOLT = 50796, SPELL_CONFLAGRATE = 17962, SPELL_IMMOLATE = 348,
        SPELL_CORRUPTION = 172, SPELL_INCINERATE = 29722, SPELL_SHADOW_BOLT = 686, SPELL_DRAIN_LIFE = 689,
        SPELL_FEAR = 5782,
        SPELL_STARFALL = 48505, SPELL_MOONFIRE = 8921, SPELL_FEROCIOUS_BITE = 22568, SPELL_SWIPE_BEAR = 779,
        SPELL_WILD_GROWTH = 48438, SPELL_SWIFTMEND = 18562, SPELL_STARFIRE = 2912, SPELL_SHRED = 5221, SPELL_RIP = 1079,
        SPELL_MAUL = 6807, SPELL_REGROWTH = 8936, SPELL_REJUVENATION = 774,
        // The Demon Hunter's and the Monk's (patch_spells.py, patch_monk.py).
        SPELL_DEMONS_BITE = 90100, SPELL_BLADE_DANCE_STRIKE = 90107, SPELL_BLADE_DANCE_FINAL = 90108,
        SPELL_FEL_RUSH = 90103, SPELL_EYE_BEAM = 90105, SPELL_VENGEFUL_RETREAT = 90109, SPELL_METAMORPHOSIS_DH = 90116,
        SPELL_IMMOLATION_AURA = 90119, SPELL_IMMOLATION_TICK = 90120, SPELL_SOUL_CLEAVE = 90125,
        SPELL_FIERY_BRAND = 90128, SPELL_SPIRIT_BOMB_BLAST = 90136,
        SPELL_VIVIFY = 90301, SPELL_RENEWING_MIST = 90302, SPELL_ENVELOPING_MIST = 90303,
        SPELL_ESSENCE_FONT_BOLT = 90305,
        SPELL_LIFE_COCOON = 90306, SPELL_TIGER_PALM = 90320, SPELL_BLACKOUT_KICK = 90321, SPELL_RISING_SUN_KICK = 90322,
        SPELL_FISTS_OF_FURY = 90323, SPELL_CRANE_KICK_STRIKE = 90326, SPELL_WHIRLING_DRAGON_PUNCH = 90327,
        // Our own (patch_runes.py).
        SPELL_STRIKE_PHYSICAL = 91171, SPELL_STRIKE_ARCANE = 91172, SPELL_STRIKE_FIRE = 91173,
        SPELL_STRIKE_FROST = 91174, SPELL_STRIKE_SHADOW = 91175, SPELL_STRIKE_HOLY = 91176, SPELL_BLEED = 91177,
        SPELL_RUNE_MEND = 91178, SPELL_GRASPING_MIND = 91179, SPELL_PHOENIX = 91180, SPELL_QUICK_RECOVERY = 91181,
        SPELL_ALPHA = 91182, SPELL_HUNTERS_LEAP = 91183, SPELL_EARTHEN_GRIP = 91184,
    };

    // Deadly Poison's damage over time, by rank (Venomous Wound: the best for the Hero's level).
    constexpr std::array<uint32, 9> DEADLY_POISON = { 2818, 2819, 11353, 11354, 25349, 26968, 27187, 57969, 57970 };

    constexpr float NEARBY = 8.0f;
    constexpr uint32 OPEN_VEIN_TICKS = 4, OPEN_VEIN_MS = 2000, BURST_SPREAD = 3, CIRCLE_MORE = 5;

    struct Trim
    {
        uint32 Rune;
        uint32 Spell;
        uint32 Pct;                                         // of the cooldown taken off (100: none left)
    };
    constexpr std::array<Trim, 11> TRIMS = { {
        { RUNE_UNENDING_STORM, SPELL_WHIRLWIND, 100 }, { RUNE_QUAKING_EARTH, SPELL_SHOCKWAVE, 50 },
        { RUNE_TEMPEST, SPELL_DIVINE_STORM, 50 }, { RUNE_GUARDIAN, SPELL_GUARDIAN_SPIRIT, 50 },
        { RUNE_MARKSMAN, SPELL_AIMED_SHOT, 100 }, { RUNE_ENDLESS_WINTER, SPELL_HOWLING_BLAST, 100 },
        { RUNE_DEFILE, SPELL_DEATH_AND_DECAY, 50 }, { RUNE_LAVA_SURGE, SPELL_LAVA_BURST, 50 },
        { RUNE_CELESTIAL_FALL, SPELL_STARFALL, 50 }, { RUNE_DRAGONS_FURY, SPELL_WHIRLING_DRAGON_PUNCH, 50 },
        { RUNE_SILKEN_COCOON, SPELL_LIFE_COCOON, 50 },
    } };
    constexpr std::array<uint32, 2> DISEASES = { SPELL_FROST_FEVER, SPELL_BLOOD_PLAGUE };
    constexpr int32 RIP_EXTEND_MS = 2000, RIP_EXTEND_MOST_MS = 6000, IMMOLATION_EXTEND_MOST_MS = 4000;

    // A Hero's one-off modifiers ("your next X") and passing state.
    struct Effects
    {
        std::unordered_map<uint32, SpellModifier*> Next;    // ability -> its modifier until it is cast
        bool FreeRevenge = false;
        uint8 BlastStacks = 0;                              // Arcane Blast's, at the last Arcane Barrage
        uint8 Diseases = 0;                                 // on the last Death Strike's target
        uint8 Backdraft = 0;                                // Incinerates or Shadow Bolts left faster
        bool BlackoutCombo = false;
        int32 ImmolationAdded = 0;                          // ms, this Immolation Aura
        std::unordered_map<ObjectGuid, int32> RipAdded;     // target -> ms Shred added to its Rip
    };
    std::unordered_map<ObjectGuid::LowType, Effects> effects;

    Effects& EffectsOf(Player* player)
    {
        return effects[player->GetGUID().GetCounter()];
    }

    uint32 First(uint32 spellId)
    {
        return sSpellMgr->GetFirstSpellInChain(spellId);
    }

    bool IsAbility(SpellInfo const* spellInfo, uint32 first)
    {
        return spellInfo && First(spellInfo->Id) == first;
    }

    // The Hero's best rank of an ability, 0: not known.
    uint32 KnownRank(Player* player, uint32 first)
    {
        uint32 best = 0;
        for (uint32 id = first; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (player->HasSpell(id))
                best = id;
        return best;
    }

    // "Your next X": a modifier on X until X is cast.
    void Next(Player* player, uint32 ability, SpellModOp op, int32 value)
    {
        Effects& state = EffectsOf(player);
        if (state.Next.count(ability))
            return;
        state.Next[ability] = RuneTiers::AddAbilityMod(player, ability, op, SPELLMOD_PCT, value);
    }

    void Spend(Player* player, uint32 ability)
    {
        Effects& state = EffectsOf(player);
        auto itr = state.Next.find(ability);
        if (itr == state.Next.end())
            return;
        RuneTiers::RemoveAbilityMod(player, itr->second);
        state.Next.erase(itr);
    }

    void Hit(Player* player, Unit* target, uint32 strike, float amount)
    {
        int32 damage = int32(amount);
        if (damage > 0 && target && target->IsAlive())
            player->CastCustomSpell(target, strike, &damage, nullptr, nullptr, true);
    }

    void Mend(Player* player, Unit* target, float amount)
    {
        int32 heal = int32(amount);
        if (heal > 0 && target && target->IsAlive())
            player->CastCustomSpell(target, SPELL_RUNE_MEND, &heal, nullptr, nullptr, true);
    }

    // Enemies near a point the Hero may hit (not the one given), nearest first, at most count.
    std::vector<Unit*> Enemies(Player* player, Unit* center, float range, std::size_t count)
    {
        std::list<Unit*> found;
        Acore::AnyUnfriendlyUnitInObjectRangeCheck check(center, player, range);
        Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(center, found, check);
        Cell::VisitObjects(center, searcher, range);
        std::vector<Unit*> enemies;
        for (Unit* unit : found)
            if (unit != center && unit->IsAlive() && player->IsValidAttackTarget(unit)
                && !unit->HasBreakableByDamageCrowdControlAura())
                enemies.push_back(unit);
        std::sort(enemies.begin(), enemies.end(), [center](Unit* a, Unit* b)
        {
            return center->GetExactDist(a) < center->GetExactDist(b);
        });
        if (enemies.size() > count)
            enemies.resize(count);
        return enemies;
    }

    // Injured allies near a point (not those given), most injured first, at most count.
    std::vector<Unit*> Injured(Player* player, Unit* center, float range, std::unordered_set<ObjectGuid> const& skip,
        std::size_t count)
    {
        std::list<Unit*> found;
        Acore::AnyFriendlyUnitInObjectRangeCheck check(center, player, range);
        Acore::UnitListSearcher<Acore::AnyFriendlyUnitInObjectRangeCheck> searcher(center, found, check);
        Cell::VisitObjects(center, searcher, range);
        std::vector<Unit*> allies;
        for (Unit* unit : found)
            if (unit->IsAlive() && !unit->IsFullHealth() && !skip.count(unit->GetGUID())
                && player->IsValidAssistTarget(unit))
                allies.push_back(unit);
        std::sort(allies.begin(), allies.end(), [](Unit* a, Unit* b) { return a->GetHealthPct() < b->GetHealthPct(); });
        if (allies.size() > count)
            allies.resize(count);
        return allies;
    }

    void Refresh(Unit* target, uint32 first, ObjectGuid caster)
    {
        for (uint32 id = first; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (Aura* aura = target->GetAura(id, caster))
                aura->RefreshDuration();
    }

    void CutCooldown(Player* player, uint32 first, int32 ms)
    {
        if (uint32 known = KnownRank(player, first))
            if (player->GetSpellCooldownDelay(known))
                player->ModifySpellCooldown(known, -ms);
    }

    Player* HeroOf(Unit* unit)
    {
        return unit ? unit->ToPlayer() : nullptr;
    }

    // The bleed of Open Vein: a share of the hit every 2 sec, 4 times (no armor).
    void OpenVein(Player* player, Unit* target, float total)
    {
        float tick = total / OPEN_VEIN_TICKS;
        ObjectGuid victim = target->GetGUID(), hero = player->GetGUID();
        for (uint32 i = 1; i <= OPEN_VEIN_TICKS; ++i)
            player->m_Events.AddEventAtOffset([player, hero, victim, tick]()
            {
                if (player->GetGUID() != hero || !player->IsInWorld())
                    return;
                Hit(player, ObjectAccessor::GetUnit(*player, victim), SPELL_BLEED, tick);
            }, Milliseconds(i * OPEN_VEIN_MS));
    }

    uint8 Diseases(Unit* target, ObjectGuid caster)
    {
        uint8 count = 0;
        for (uint32 disease : DISEASES)
            count += target->HasAura(disease, caster) ? 1 : 0;
        return count;
    }

    // A heal over 6 sec, in 3 parts.
    void HealOverTime(Player* player, Unit* target, float total)
    {
        float tick = total / 3;
        ObjectGuid ally = target->GetGUID(), hero = player->GetGUID();
        for (uint32 i = 1; i <= 3; ++i)
            player->m_Events.AddEventAtOffset([player, hero, ally, tick]()
            {
                if (player->GetGUID() == hero && player->IsInWorld())
                    Mend(player, ObjectAccessor::GetUnit(*player, ally), tick);
            }, Milliseconds(i * 2000));
    }

    // The Hero's auras of these abilities (every rank) on the target: id and time left.
    std::vector<std::pair<uint32, int32>> AurasOf(Unit* target, ObjectGuid caster, std::initializer_list<uint32> firsts)
    {
        std::vector<std::pair<uint32, int32>> found;
        for (uint32 first : firsts)
            for (uint32 id = first; id; id = sSpellMgr->GetNextSpellInChain(id))
                if (Aura* aura = target->GetAura(id, caster))
                    found.emplace_back(id, aura->GetDuration());
        return found;
    }

    void SpreadAura(Player* player, uint32 spellId, Unit* to, int32 duration = 0)
    {
        if (to->HasAura(spellId, player->GetGUID()))
            return;
        if (Aura* aura = player->AddAura(spellId, to); aura && duration > 0)
        {
            aura->SetMaxDuration(duration);
            aura->SetDuration(duration);
        }
    }

    bool HitWasCrit(Spell* spell, Unit* target)
    {
        for (TargetInfo const& info : *spell->GetUniqueTargetInfo())
            if (info.targetGUID == target->GetGUID())
                return info.crit;
        return false;
    }
}

namespace RuneEffects
{
    std::vector<RuneTiers::RuneMod> const& Mods()
    {
        static std::vector<RuneTiers::RuneMod> const mods = {
            { RUNE_RAGING_BLOW, SPELL_SLAM, SPELLMOD_CASTING_TIME, SPELLMOD_PCT, -100 },
            { RUNE_BULWARKS_TOSS, SPELL_AVENGERS_SHIELD, SPELLMOD_JUMP_TARGETS, SPELLMOD_FLAT, 3 },
            { RUNE_SHINING_SHIELD, SPELL_AVENGERS_SHIELD, SPELLMOD_JUMP_TARGETS, SPELLMOD_FLAT, 2 },
            { RUNE_ENDLESS_SPREE, SPELL_KILLING_SPREE, SPELLMOD_DURATION, SPELLMOD_PCT, 100 },
            { RUNE_TWIN_PENANCE, SPELL_PENANCE_DAMAGE, SPELLMOD_ACTIVATION_TIME, SPELLMOD_PCT, -50 },
            { RUNE_TWIN_PENANCE, SPELL_PENANCE_HEAL, SPELLMOD_ACTIVATION_TIME, SPELLMOD_PCT, -50 },
            { RUNE_UNENDING_POWER, SPELL_ARCANE_POWER, SPELLMOD_DURATION, SPELLMOD_PCT, 100 },
            { RUNE_FROZEN_HEART, SPELL_ICY_VEINS, SPELLMOD_DURATION, SPELLMOD_PCT, 100 },
            { RUNE_LONG_WOUND, SPELL_REND, SPELLMOD_DURATION, SPELLMOD_FLAT, 6000 },
            { RUNE_WRECKING_CHARGE, SPELL_CHARGE_STUN, SPELLMOD_DURATION, SPELLMOD_FLAT, 1000 },
            { RUNE_CRIMSON_TIDE, SPELL_VAMPIRIC_BLOOD, SPELLMOD_DURATION, SPELLMOD_PCT, 100 },
            { RUNE_SHAMBLING_DEAD, SPELL_RAISE_DEAD_GHOUL, SPELLMOD_DURATION, SPELLMOD_FLAT, 30000 },
            { RUNE_SHAMBLING_DEAD, SPELL_RAISE_DEAD_PET, SPELLMOD_DURATION, SPELLMOD_FLAT, 30000 },
            { RUNE_STORMS_REACH, SPELL_CHAIN_LIGHTNING, SPELLMOD_JUMP_TARGETS, SPELLMOD_FLAT, 3 },
            { RUNE_DEMON_WITHIN, SPELL_METAMORPHOSIS_WARLOCK, SPELLMOD_DURATION, SPELLMOD_PCT, 100 },
            { RUNE_MOLTEN_SOUL, SPELL_SOUL_FIRE, SPELLMOD_CASTING_TIME, SPELLMOD_PCT, -100 },
        };
        return mods;
    }

    std::vector<RuneTiers::RuneAura> const& Auras()
    {
        static std::vector<RuneTiers::RuneAura> const auras = { { RUNE_QUICK_RECOVERY, SPELL_QUICK_RECOVERY } };
        return auras;
    }

    void Forget(Player* player)
    {
        auto itr = effects.find(player->GetGUID().GetCounter());
        if (itr == effects.end())
            return;
        for (auto const& [ability, mod] : itr->second.Next)
            RuneTiers::RemoveAbilityMod(player, mod);
        effects.erase(itr);
    }
}

// The rules a rune lifts: gone from the server's copy of the spells at load (and the client's, patch_spells.py), and
// checked here instead, for everyone without the rune.
class titan_rune_effects_checks : public AllSpellScript
{
public:
    titan_rune_effects_checks() : AllSpellScript("titan_rune_effects_checks", { ALLSPELLHOOK_ON_SPELL_CHECK_CAST }) { }

    void OnSpellCheckCast(Spell* spell, bool /*strict*/, SpellCastResult& res) override
    {
        Unit* caster = spell->GetCaster() ? spell->GetCaster()->ToUnit() : nullptr;
        if (res != SPELL_CAST_OK || !caster || spell->IsTriggered())
            return;
        SpellInfo const* spellInfo = spell->GetSpellInfo();
        Unit* target = spell->m_targets.GetUnitTarget();
        Player* hero = caster->ToPlayer();
        auto has = [hero](uint32 rune) { return hero && RuneTiers::Has(hero, rune); };
        switch (First(spellInfo->Id))
        {
            case SPELL_REVENGE:
                if (!caster->HasAuraState(AURA_STATE_DEFENSE) && !has(RUNE_AVENGER)
                    && !(hero && EffectsOf(hero).FreeRevenge))
                    res = SPELL_FAILED_CASTER_AURASTATE;
                break;
            case SPELL_EXECUTE:
                if (target && target->GetHealthPct() > 20.0f && !(has(RUNE_HEADSMAN) && target->GetHealthPct() < 35.0f))
                    res = SPELL_FAILED_TARGET_AURASTATE;
                break;
            case SPELL_HAMMER_OF_WRATH:
                if (target && target->GetHealthPct() > 20.0f && !has(RUNE_AVENGING_HAMMER))
                    res = SPELL_FAILED_TARGET_AURASTATE;
                break;
            case SPELL_DEEP_FREEZE:
                if (target && !target->HasAuraState(AURA_STATE_FROZEN, spellInfo, caster) && !has(RUNE_GLACIER))
                    res = SPELL_FAILED_TARGET_AURASTATE;
                break;
            case SPELL_AMBUSH:
                if (!caster->HasStealthAura() && !has(RUNE_SHADOW_DANCE))
                    res = SPELL_FAILED_ONLY_STEALTHED;
                break;
            case SPELL_BACKSTAB:
                if (target && target->HasInArc(float(M_PI), caster) && !has(RUNE_SHADOWS_EDGE))
                    res = SPELL_FAILED_NOT_BEHIND;
                break;
            default:
                break;
        }
    }
};

class titan_rune_effects_load : public GlobalScript
{
public:
    titan_rune_effects_load() : GlobalScript("titan_rune_effects_load", { GLOBALHOOK_ON_LOAD_SPELL_CUSTOM_ATTR }) { }

    void OnLoadSpellCustomAttr(SpellInfo* spellInfo) override
    {
        switch (First(spellInfo->Id))
        {
            case SPELL_REVENGE:
                spellInfo->CasterAuraState = 0;
                break;
            // Overpower is readied by the target's dodge as an aura state (6, the core's Unit::ProcDamageAndSpell
            // for players), not a combo point: no longer a finishing move, so it needs and spends none.
            case SPELL_OVERPOWER:
                spellInfo->AttributesEx &= ~(SPELL_ATTR1_FINISHING_MOVE_DAMAGE | SPELL_ATTR1_FINISHING_MOVE_DURATION);
                spellInfo->CasterAuraState = AURA_STATE_OVERPOWER;
                break;
            case SPELL_EXECUTE:
            case SPELL_HAMMER_OF_WRATH:
            case SPELL_DEEP_FREEZE:
                spellInfo->TargetAuraState = 0;
                break;
            case SPELL_AMBUSH:
                spellInfo->Attributes &= ~SPELL_ATTR0_ONLY_STEALTHED;
                break;
            case SPELL_BACKSTAB:
                spellInfo->AttributesCu &= ~SPELL_ATTR0_CU_REQ_CASTER_BEHIND_TARGET;
                break;
            default:
                break;
        }
    }
};

// The runes' on-hit extras, by the spell's first rank (spell_script_names: every rank).
class spell_titan_rune_tier_hit : public SpellScript
{
    PrepareSpellScript(spell_titan_rune_tier_hit);

    void AfterHitEffects()
    {
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;
        float damage = float(GetHitDamage()), heal = float(GetHitHeal());
        bool crit = HitWasCrit(GetSpell(), target);
        auto has = [player](uint32 rune) { return RuneTiers::Has(player, rune); };
        _hit.insert(target->GetGUID());
        _heal = std::max(_heal, heal);
        switch (LegendaryRunes::CountsAs(GetSpellInfo()))     // a replacement's main hit: its base
        {
            case SPELL_OVERPOWER:
                if (has(RUNE_OPEN_GUARD))
                    Hit(player, target, SPELL_STRIKE_PHYSICAL, damage);
                break;
            case SPELL_REVENGE:
                if (has(RUNE_AVENGER))
                    for (Unit* enemy : Enemies(player, target, NEARBY, 2))
                        Hit(player, enemy, SPELL_STRIKE_PHYSICAL, damage);
                break;
            case SPELL_CLEAVE:
                if (has(RUNE_BLOODY_SWEEP) && ++_cleaved == 2)
                    player->ModifyPower(POWER_RAGE, 50);
                break;
            case SPELL_SHIELD_SLAM:
                if (has(RUNE_SHARP_TONGUE) && crit)
                {
                    EffectsOf(player).FreeRevenge = true;
                    Next(player, SPELL_REVENGE, SPELLMOD_COST, -100);
                }
                break;
            case SPELL_THUNDER_CLAP:
                if (has(RUNE_THUNDERING_BLADE))
                    if (uint32 rend = KnownRank(player, SPELL_REND))
                        player->CastSpell(target, rend, true);
                break;
            case SPELL_HOLY_LIGHT:
                if (has(RUNE_BEACON) && target != player)
                    Mend(player, player, heal / 2);
                if (has(RUNE_RADIANCE))
                    for (Unit* ally : Injured(player, target, NEARBY, { target->GetGUID() }, 5))
                        Mend(player, ally, heal * 0.1f);
                break;
            case SPELL_EXORCISM:
                if (has(RUNE_EXORCIST) && crit)
                    Next(player, SPELL_FLASH_OF_LIGHT, SPELLMOD_CASTING_TIME, -100);
                break;
            case SPELL_CRUSADER_STRIKE:
                if (has(RUNE_RECKONING_HAMMER))
                    CutCooldown(player, SPELL_HAMMER_OF_JUSTICE, 1000);
                break;
            case SPELL_MUTILATE:
                if (has(RUNE_TWIN_FANGS))
                    player->AddComboPoints(target, 1);
                break;
            case SPELL_BLADE_FLURRY_HIT:
                if (has(RUNE_ENDLESS_FLURRY))
                    for (Unit* enemy : Enemies(player, target, NEARBY, 2))
                        Hit(player, enemy, SPELL_STRIKE_PHYSICAL, damage);
                break;
            case SPELL_HEMORRHAGE:
                if (has(RUNE_OPEN_VEIN))
                    OpenVein(player, target, damage / 2);
                break;
            case SPELL_SINISTER_STRIKE:
                if (has(RUNE_OPPORTUNIST) && roll_chance_i(20))
                    player->AddComboPoints(target, 1);
                break;
            case SPELL_AMBUSH:
                if (has(RUNE_AMBUSHER))
                    player->AddComboPoints(target, 2);
                break;
            case SPELL_MIND_BLAST:
                if (has(RUNE_MINDS_ECHO) && crit)
                    Refresh(target, SPELL_SHADOW_WORD_PAIN, player->GetGUID());
                break;
            case SPELL_FLASH_HEAL:
                if (has(RUNE_FLASHING_GRACE))
                    Refresh(target, SPELL_RENEW, player->GetGUID());
                break;
            case SPELL_ARCANE_BARRAGE:
                if (has(RUNE_ARCANE_VOLLEY))
                    for (Unit* enemy : Enemies(player, target, NEARBY, 2))
                        Hit(player, enemy, SPELL_STRIKE_ARCANE, damage);
                break;
            case SPELL_FIREBALL:
                if (has(RUNE_KINDLING) && crit)
                    CutCooldown(player, SPELL_FIRE_BLAST, 1000);
                break;
            case SPELL_FROSTBOLT:
                if (has(RUNE_FROSTFIRE_BRIDGE) && crit)
                    Next(player, SPELL_FIREBALL, SPELLMOD_CASTING_TIME, -100);
                break;
            case SPELL_CONE_OF_COLD:
                if (has(RUNE_DEEP_CHILL))
                    CutCooldown(player, SPELL_FROST_NOVA, 2000);
                break;
            default:
                AfterHitMore(player, target, damage, heal, crit);
                break;
        }
    }

    // Batch two: Hunter, Death Knight, Shaman, Warlock, Druid, Demon Hunter, Monk.
    void AfterHitMore(Player* player, Unit* target, float damage, float heal, bool crit)
    {
        auto has = [player](uint32 rune) { return RuneTiers::Has(player, rune); };
        ObjectGuid hero = player->GetGUID();
        switch (First(GetSpellInfo()->Id))
        {
            case SPELL_MULTI_SHOT:
                if (has(RUNE_ENDLESS_BARRAGE))
                    Hit(player, target, SPELL_STRIKE_PHYSICAL, damage);
                break;
            case SPELL_RAPTOR_STRIKE:
                if (has(RUNE_RAPTORS_EDGE))
                    for (Unit* enemy : Enemies(player, target, NEARBY, 2))
                        Hit(player, enemy, SPELL_STRIKE_PHYSICAL, damage);
                break;
            case SPELL_ARCANE_SHOT:
                if (has(RUNE_SERPENTS_KISS))
                    Refresh(target, SPELL_SERPENT_STING, hero);
                break;
            case SPELL_STEADY_SHOT:
                if (has(RUNE_STEADY_HAND) && crit)
                    CutCooldown(player, SPELL_AIMED_SHOT, 2000);
                break;
            case SPELL_DEATH_STRIKE:
                EffectsOf(player).Diseases = Diseases(target, hero);
                break;
            case SPELL_OBLITERATE:
                if (has(RUNE_TWIN_BLADES))
                    for (Unit* enemy : Enemies(player, target, NEARBY, 1))
                        Hit(player, enemy, SPELL_STRIKE_PHYSICAL, damage);
                if (has(RUNE_KILLING_FROST) && crit)
                    if (uint32 blast = KnownRank(player, SPELL_HOWLING_BLAST))
                        player->RemoveSpellCooldown(blast, true);
                break;
            case SPELL_SCOURGE_STRIKE:
                if (has(RUNE_FESTERING_WOUND))
                    for (uint32 disease : DISEASES)
                        if (target->HasAura(disease, hero))
                            for (Unit* enemy : Enemies(player, target, NEARBY, 5))
                                player->CastSpell(enemy, disease, true);
                break;
            case SPELL_BLOOD_BOIL:
                if (has(RUNE_PLAGUE_BEARER))
                    if (Unit* source = player->GetSelectedUnit(); source && source != target)
                        for (uint32 disease : DISEASES)
                            if (source->HasAura(disease, hero))
                                player->CastSpell(target, disease, true);
                break;
            case SPELL_HEART_STRIKE:
                if (has(RUNE_BLOODY_CROWN) && crit)
                    Mend(player, player, float(player->CountPctFromMaxHealth(2)));
                break;
            case SPELL_STORMSTRIKE_MAIN:
            case SPELL_STORMSTRIKE_OFF:
                if (has(RUNE_ASCENDANCE))
                    for (Unit* enemy : Enemies(player, target, NEARBY, 4))
                        if (player->HasInArc(float(M_PI), enemy))
                            Hit(player, enemy, SPELL_STRIKE_PHYSICAL, damage);
                if (has(RUNE_MAELSTROM) && crit)
                    Next(player, SPELL_LIGHTNING_BOLT, SPELLMOD_CASTING_TIME, -100);
                break;
            case SPELL_LAVA_LASH:
                if (has(RUNE_MOLTEN_EDGE))
                    for (auto const& [shock, left] : AurasOf(target, hero, { SPELL_FLAME_SHOCK }))
                        for (Unit* enemy : Enemies(player, target, 10.0f, 4))
                            player->CastSpell(enemy, shock, true);
                break;
            case SPELL_RIPTIDE:
                if (has(RUNE_TIDAL_ECHO) && !GetSpell()->IsTriggered())
                    for (Unit* ally : Injured(player, target, 20.0f, { target->GetGUID() }, 1))
                        player->CastSpell(ally, GetSpellInfo()->Id, true);
                break;
            case SPELL_HEALING_WAVE:
                if (has(RUNE_ANCESTRAL_WAVE))
                    for (Unit* ally : Injured(player, target, 10.0f, { target->GetGUID() }, 2))
                        Mend(player, ally, heal * 0.3f);
                break;
            case SPELL_LIGHTNING_BOLT:
                if (has(RUNE_CHAIN_SPARK) && crit)
                    Next(player, SPELL_CHAIN_LIGHTNING, SPELLMOD_CASTING_TIME, -100);
                break;
            case SPELL_EARTH_SHOCK:
                if (has(RUNE_EARTHEN_GRIP))
                    player->CastSpell(target, SPELL_EARTHEN_GRIP, true);
                break;
            case SPELL_HAUNT:
                if (has(RUNE_HAUNTING))
                    for (Unit* enemy : Enemies(player, target, NEARBY, 1))
                        Hit(player, enemy, SPELL_STRIKE_SHADOW, damage);
                break;
            case SPELL_CHAOS_BOLT:
                if (has(RUNE_CHAOS))
                    for (Unit* enemy : Enemies(player, target, NEARBY, 2))
                        Hit(player, enemy, SPELL_STRIKE_FIRE, damage);
                break;
            case SPELL_CONFLAGRATE:
                if (has(RUNE_FIRE_AND_BRIMSTONE))
                    Restore(player, target);
                break;
            case SPELL_MOONFIRE:
                if (has(RUNE_LUNAR_STRIKE))
                    for (Unit* enemy : Enemies(player, target, NEARBY, 5))
                        Hit(player, enemy, SPELL_STRIKE_ARCANE, damage);
                if (has(RUNE_LUNAR_FALL) && crit)
                    Next(player, SPELL_STARFIRE, SPELLMOD_CASTING_TIME, -100);
                break;
            case SPELL_FEROCIOUS_BITE:
                if (has(RUNE_PRIMAL_WRATH))
                {
                    OpenVein(player, target, damage / 2);
                    for (Unit* enemy : Enemies(player, target, NEARBY, 5))
                        OpenVein(player, enemy, damage / 2);
                }
                break;
            case SPELL_SWIPE_BEAR:
                if (has(RUNE_THRASH))
                    OpenVein(player, target, damage / 2);
                break;
            case SPELL_SWIFTMEND:
                if (has(RUNE_BLOOMING_GROVE))
                    Restore(player, target);
                if (has(RUNE_LINGERING_BLOOM))
                    HealOverTime(player, target, heal / 2);
                break;
            case SPELL_SHRED:
                if (has(RUNE_SHREDDER))
                    for (uint32 id = SPELL_RIP; id; id = sSpellMgr->GetNextSpellInChain(id))
                        if (Aura* rip = target->GetAura(id, hero))
                        {
                            int32& added = EffectsOf(player).RipAdded[target->GetGUID()];
                            if (added >= RIP_EXTEND_MOST_MS)
                                break;
                            added += RIP_EXTEND_MS;
                            rip->SetMaxDuration(rip->GetMaxDuration() + RIP_EXTEND_MS);
                            rip->SetDuration(rip->GetDuration() + RIP_EXTEND_MS);
                        }
                break;
            case SPELL_MAUL:
                if (has(RUNE_THICK_HIDE) && crit)
                    player->ModifyPower(POWER_RAGE, 50);
                break;
            case SPELL_REGROWTH:
                if (has(RUNE_REJUVENATING_TOUCH))
                    Refresh(target, SPELL_REJUVENATION, hero);
                break;
            case SPELL_DEMONS_BITE:
                if (has(RUNE_FEL_EDGE) && crit)
                    player->ModifyPower(POWER_RAGE, 50);
                break;
            case SPELL_BLADE_DANCE_STRIKE:
            case SPELL_BLADE_DANCE_FINAL:
                if (has(RUNE_DEATH_SWEEP))
                    Hit(player, target, SPELL_STRIKE_PHYSICAL, damage);
                break;
            case SPELL_SOUL_CLEAVE:
                if (has(RUNE_SOUL_FEAST))
                    Mend(player, player, float(player->CountPctFromMaxHealth(3)));
                break;
            case SPELL_FIERY_BRAND:
                if (has(RUNE_BURNING_BRAND))
                    for (Unit* enemy : Enemies(player, target, NEARBY, 10))
                        SpreadAura(player, SPELL_FIERY_BRAND, enemy);
                else if (has(RUNE_BRANDING_IRON))
                    if (Aura* brand = target->GetAura(SPELL_FIERY_BRAND, hero))
                        for (Unit* enemy : Enemies(player, target, NEARBY, 2))
                            SpreadAura(player, SPELL_FIERY_BRAND, enemy, brand->GetMaxDuration() / 2);
                break;
            case SPELL_VIVIFY:
                if (has(RUNE_SECOND_BREATH))
                    for (Unit* ally : Injured(player, target, 20.0f, { target->GetGUID() }, 1))
                        Mend(player, ally, heal / 2);
                break;
            case SPELL_TIGER_PALM:
                if (has(RUNE_TIGERS_CLAW))
                    CutCooldown(player, SPELL_RISING_SUN_KICK, 1000);
                break;
            case SPELL_BLACKOUT_KICK:
                if (has(RUNE_BLACKOUT_COMBO))
                    EffectsOf(player).BlackoutCombo = true;
                break;
            case SPELL_ENVELOPING_MIST:
                if (has(RUNE_MISTWEAVERS_GRACE))
                    SpreadAura(player, SPELL_RENEWING_MIST, target);
                break;
            default:
                break;
        }
    }

    // Conflagrate (Fire and Brimstone) and Swiftmend (the Blooming Grove) keep what they would consume.
    void Snapshot()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;
        uint32 first = First(GetSpellInfo()->Id);
        if (first == SPELL_CONFLAGRATE)
            _kept = AurasOf(target, caster->GetGUID(), { SPELL_IMMOLATE });
        else if (first == SPELL_SWIFTMEND)
            _kept = AurasOf(target, caster->GetGUID(), { SPELL_REJUVENATION, SPELL_REGROWTH });
    }

    void Restore(Player* player, Unit* target)
    {
        for (auto const& [id, left] : _kept)
            if (!target->HasAura(id, player->GetGUID()) && left > 0)
                if (Aura* aura = player->AddAura(id, target))
                    aura->SetDuration(left);
        _kept.clear();
    }

    // Circle of Healing with the Wide Circle: 5 more injured allies (not healed already) for its best heal. Wild
    // Growth with the Ever-Blooming: its heal over time on 3 more.
    void AfterCastEffects()
    {
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!player)
            return;
        Unit* center = GetExplTargetUnit() ? GetExplTargetUnit() : player;
        uint32 first = First(GetSpellInfo()->Id);
        if (first == SPELL_CIRCLE_OF_HEALING && RuneTiers::Has(player, RUNE_WIDE_CIRCLE) && _heal > 0.0f)
            for (Unit* ally : Injured(player, center, 15.0f, _hit, CIRCLE_MORE))
                Mend(player, ally, _heal);
        else if (first == SPELL_WILD_GROWTH && RuneTiers::Has(player, RUNE_EVER_BLOOMING))
            for (Unit* ally : Injured(player, center, 30.0f, _hit, 3))
                SpreadAura(player, GetSpellInfo()->Id, ally);
    }

    void Register() override
    {
        BeforeHit += BeforeSpellHitFn(spell_titan_rune_tier_hit::SnapshotOnHit);
        AfterHit += SpellHitFn(spell_titan_rune_tier_hit::AfterHitEffects);
        AfterCast += SpellCastFn(spell_titan_rune_tier_hit::AfterCastEffects);
    }

    void SnapshotOnHit(SpellMissInfo missInfo)
    {
        if (missInfo == SPELL_MISS_NONE)
            Snapshot();
    }

private:
    std::vector<std::pair<uint32, int32>> _kept;
    uint32 _cleaved = 0;
    float _heal = 0.0f;
    std::unordered_set<ObjectGuid> _hit;
};

// Living Bomb (the Living Inferno: its explosion sets up to 3 more, which do not spread again) and Vampiric Touch
// (the Endless Thirst: its target's death passes it to the nearest enemy), every rank.
class spell_titan_rune_tier_spread : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_tier_spread);

    void Spread(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        Unit* target = GetTarget();
        if (!player || !target)
            return;
        AuraRemoveMode removed = GetTargetApplication()->GetRemoveMode();
        uint32 first = First(GetId());
        if (first == SPELL_LIVING_BOMB && removed == AURA_REMOVE_BY_EXPIRE
            && RuneTiers::Has(player, RUNE_LIVING_INFERNO))
        {
            if (_spread.erase(target->GetGUID()))
                return;                                     // one set by a spread: no further
            for (Unit* enemy : Enemies(player, target, 10.0f, BURST_SPREAD))
                if (!enemy->HasAura(GetId(), player->GetGUID()))
                {
                    _spread.insert(enemy->GetGUID());
                    player->CastSpell(enemy, GetId(), true);
                }
        }
        else if (removed == AURA_REMOVE_BY_DEATH
            && ((first == SPELL_VAMPIRIC_TOUCH && RuneTiers::Has(player, RUNE_ENDLESS_THIRST))
                || (first == SPELL_UNSTABLE_AFFLICTION && RuneTiers::Has(player, RUNE_CONTAGION))
                || (first == SPELL_CORRUPTION && RuneTiers::Has(player, RUNE_LINGERING_CURSE))))
            for (Unit* enemy : Enemies(player, target, 10.0f, 1))
                player->CastSpell(enemy, GetId(), true);
        else if (first == SPELL_BLACK_ARROW && removed == AURA_REMOVE_BY_DEATH
            && RuneTiers::Has(player, RUNE_DARK_HUNTER))
            if (uint32 arrow = KnownRank(player, SPELL_BLACK_ARROW))
                player->RemoveSpellCooldown(arrow, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_titan_rune_tier_spread::Spread, EFFECT_ALL,
            SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
    }

    static std::unordered_set<ObjectGuid> _spread;
};
std::unordered_set<ObjectGuid> spell_titan_rune_tier_spread::_spread;

// Power Word: Shield (every rank) with the Glimmering Shield: broken, it heals its target for 20% of what it held.
class spell_titan_rune_tier_ward : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_tier_ward);

    void Held(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        _held = aurEff->GetAmount();
    }

    void Broken(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (player && GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_ENEMY_SPELL
            && RuneTiers::Has(player, RUNE_GLIMMERING_SHIELD))
            Mend(player, GetTarget(), _held * 0.2f);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_titan_rune_tier_ward::Held, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB,
            AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_titan_rune_tier_ward::Broken, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB,
            AURA_EFFECT_HANDLE_REAL);
    }

private:
    int32 _held = 0;
};

// Quick Recovery (Evasion's Rare, worn while held): a dodge during Evasion heals the Hero for 2% of their health.
class spell_titan_rune_tier_quick_recovery : public AuraScript
{
    PrepareAuraScript(spell_titan_rune_tier_quick_recovery);

    bool CheckProc(ProcEventInfo& /*eventInfo*/)
    {
        Unit* owner = GetTarget();
        return owner && (owner->HasAura(SPELL_EVASION) || owner->HasAura(SPELL_EVASION_2));
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& /*eventInfo*/)
    {
        if (Player* player = GetTarget()->ToPlayer())
            Mend(player, player, float(player->CountPctFromMaxHealth(2)));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_titan_rune_tier_quick_recovery::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_titan_rune_tier_quick_recovery::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// The hits, ticks and heals the runes change.
class titan_rune_effects_unit : public UnitScript
{
public:
    titan_rune_effects_unit() : UnitScript("titan_rune_effects_unit", true, { UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK, UNITHOOK_MODIFY_HEAL_RECEIVED, UNITHOOK_ON_AURA_APPLY }) { }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        Player* player = HeroOf(attacker);
        if (!player || !spellInfo || damage <= 0)
            return;
        uint32 first = First(spellInfo->Id);
        if (first == SPELL_DEATH_BACKLASH && target == attacker && RuneTiers::Has(player, RUNE_FINAL_WORD))
            damage = 0;
        else if (first == SPELL_ENVENOM && target->HealthBelowPct(35) && RuneTiers::Has(player, RUNE_TOXIC_FINALE))
            damage = int32(damage * 1.5f);
        else if (first == SPELL_BACKSTAB && target->HasInArc(float(M_PI), attacker)
            && RuneTiers::Has(player, RUNE_SHADOWS_EDGE))
            damage /= 2;
        else if (first == SPELL_ARCANE_BARRAGE && RuneTiers::Has(player, RUNE_CHARGED_BLAST))
            damage = int32(damage * (1.0f + 0.1f * EffectsOf(player).BlastStacks));
        else if (first == SPELL_HOLY_SHIELD && RuneTiers::Has(player, RUNE_SACRED_WALL))
            Mend(player, player, float(player->CountPctFromMaxHealth(1)));
        else if (first == SPELL_EXPLOSIVE_SHOT_HIT && RuneTiers::Has(player, RUNE_BLAST_RADIUS))
            for (Unit* enemy : Enemies(player, target, NEARBY, 5))
                Hit(player, enemy, SPELL_STRIKE_FIRE, float(damage));
        else if (first == SPELL_SPIRIT_BOMB_BLAST && RuneTiers::Has(player, RUNE_SOUL_BARRAGE))
            Mend(player, player, damage * 0.2f);
        else if (first == SPELL_TIGER_PALM && EffectsOf(player).BlackoutCombo)
        {
            EffectsOf(player).BlackoutCombo = false;
            damage *= 2;
        }
        else if (first == SPELL_CRANE_KICK_STRIKE && RuneTiers::Has(player, RUNE_WHIRLING_CRANE))
        {
            std::size_t near = std::min<std::size_t>(5, Enemies(player, player, NEARBY, 5).size());
            damage = int32(damage * (1.0f + 0.1f * near));
        }
        else if (first == SPELL_IMMOLATION_TICK && RuneTiers::Has(player, RUNE_SPREADING_FLAME))
            if (Aura* immolation = player->GetAura(SPELL_IMMOLATION_AURA);
                immolation && EffectsOf(player).ImmolationAdded < IMMOLATION_EXTEND_MOST_MS)
            {
                EffectsOf(player).ImmolationAdded += 1000;
                immolation->SetMaxDuration(immolation->GetMaxDuration() + 1000);
                immolation->SetDuration(immolation->GetDuration() + 1000);
            }
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage,
        SpellInfo const* spellInfo) override
    {
        Player* player = HeroOf(attacker);
        if (!player || !spellInfo || !damage || spellInfo->HasAura(SPELL_AURA_PERIODIC_HEAL))
            return;
        switch (First(spellInfo->Id))
        {
            case SPELL_REND:
                if (RuneTiers::Has(player, RUNE_LONG_WOUND))
                    if (Aura* rend = target->GetAura(spellInfo->Id, player->GetGUID()))
                        if (AuraEffect const* tick = rend->GetEffect(EFFECT_0);
                            tick && rend->GetDuration() <= tick->GetAmplitude())
                            damage *= 3;
                break;
            case SPELL_RUPTURE:
                if (RuneTiers::Has(player, RUNE_VENOMOUS_WOUND) && roll_chance_i(30))
                {
                    uint32 poison = DEADLY_POISON[0];
                    for (uint32 id : DEADLY_POISON)
                        if (SpellInfo const* rank = sSpellMgr->GetSpellInfo(id);
                            rank && rank->SpellLevel <= player->GetLevel())
                            poison = id;
                    player->CastSpell(target, poison, true);
                }
                break;
            case SPELL_HOLY_FIRE:
                if (RuneTiers::Has(player, RUNE_SEARING_LIGHT))
                    Mend(player, player, damage / 2.0f);
                break;
            case SPELL_MIND_FLAY:
                if (RuneTiers::Has(player, RUNE_GRASPING_MIND))
                    player->CastSpell(target, SPELL_GRASPING_MIND, true);
                break;
            case SPELL_FLAME_SHOCK:
                if (RuneTiers::Has(player, RUNE_MOLTEN_FLOW) && roll_chance_i(10))
                    if (uint32 burst = KnownRank(player, SPELL_LAVA_BURST))
                        player->RemoveSpellCooldown(burst, true);
                break;
            case SPELL_IMMOLATE:
                if (RuneTiers::Has(player, RUNE_BACKLASH) && roll_chance_i(10))
                    Next(player, SPELL_INCINERATE, SPELLMOD_CASTING_TIME, -100);
                break;
            case SPELL_CORRUPTION:
                if (RuneTiers::Has(player, RUNE_DREAD) && !AurasOf(target, player->GetGUID(), { SPELL_FEAR }).empty())
                    damage = uint32(damage * 1.2f);
                break;
            default:
                break;
        }
    }

    void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* spellInfo) override
    {
        Player* player = HeroOf(healer);
        if (!player || !spellInfo)
            return;
        switch (First(spellInfo->Id))
        {
            case SPELL_FLASH_OF_LIGHT:
                if (target->HealthBelowPct(35) && RuneTiers::Has(player, RUNE_INFUSION))
                    heal = uint32(heal * 1.5f);
                break;
            case SPELL_DEATH_STRIKE_HEAL:
                if (RuneTiers::Has(player, RUNE_BLOOD_DEBT))
                    heal *= 2;
                if (RuneTiers::Has(player, RUNE_PLAGUED_BLOOD))
                    heal = uint32(heal * (1.0f + 0.05f * EffectsOf(player).Diseases));
                break;
            case SPELL_DRAIN_LIFE:
                if (RuneTiers::Has(player, RUNE_SIPHONING_SOUL))
                    if (Spell* channel = player->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
                        if (Unit* drained = channel->m_targets.GetUnitTarget();
                            drained && !AurasOf(drained, player->GetGUID(), { SPELL_CORRUPTION }).empty())
                            heal = uint32(heal * 1.2f);
                break;
            case SPELL_MEND_PET:
                if (target != player && RuneTiers::Has(player, RUNE_KINDRED_SPIRITS))
                    Mend(player, player, float(heal));
                break;
            case SPELL_ESSENCE_FONT_BOLT:
                if (RuneTiers::Has(player, RUNE_OVERFLOWING_FONT))
                    for (Unit* ally : Injured(player, player, 30.0f, { target->GetGUID() }, 1))
                        Mend(player, ally, float(heal));
                break;
            default:
                break;
        }
    }

    // The Unbroken Ward: the Hero's Power Word: Shield leaves no Weakened Soul.
    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        if (aura->GetId() != SPELL_WEAKENED_SOUL)
            return;
        Player* player = HeroOf(aura->GetCaster());
        if (!player || !RuneTiers::Has(player, RUNE_UNBROKEN_WARD))
            return;
        ObjectGuid guid = unit->GetGUID();
        unit->m_Events.AddEventAtOffset([unit, guid]()
        {
            if (unit->GetGUID() == guid)
                unit->RemoveAurasDueToSpell(SPELL_WEAKENED_SOUL);
        }, 1ms);
    }
};

// What a cast sets off: cooldowns cut, "your next X" spent, Combustion's Phoenix, Blink's Frost Nova, Judgements on
// Consecration, Charge's free Heroic Strike, Revenge's own block consumed (without the Avenger).
class titan_rune_effects_player : public PlayerScript
{
public:
    titan_rune_effects_player() : PlayerScript("titan_rune_effects_player", { PLAYERHOOK_ON_SPELL_CAST }) { }

    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (spell->IsTriggered())
            return;
        SpellInfo const* spellInfo = spell->GetSpellInfo();
        uint32 first = First(spellInfo->Id), spellId = spellInfo->Id;
        auto has = [player](uint32 rune) { return RuneTiers::Has(player, rune); };
        // Backdraft: two Incinerates or Shadow Bolts faster, then spent together.
        Effects& fx = EffectsOf(player);
        bool backdraft = (first == SPELL_INCINERATE || first == SPELL_SHADOW_BOLT) && fx.Backdraft > 0;
        if (backdraft && --fx.Backdraft == 0)
        {
            ObjectGuid guid = player->GetGUID();
            player->m_Events.AddEventAtOffset([player, guid]()
            {
                if (player->GetGUID() != guid)
                    return;
                Spend(player, SPELL_INCINERATE);
                Spend(player, SPELL_SHADOW_BOLT);
            }, 1ms);
        }
        // "Your next X" is spent once the cast has taken its cost and time (after this hook).
        else if (!backdraft && fx.Next.count(first))
        {
            ObjectGuid guid = player->GetGUID();
            player->m_Events.AddEventAtOffset([player, guid, first]()
            {
                if (player->GetGUID() == guid)
                    Spend(player, first);
            }, 1ms);
        }
        switch (first)
        {
            case SPELL_REVENGE:
                if (!has(RUNE_AVENGER) && !EffectsOf(player).FreeRevenge)
                    player->ModifyAuraState(AURA_STATE_DEFENSE, false);
                EffectsOf(player).FreeRevenge = false;
                break;
            case SPELL_CHARGE:
                if (has(RUNE_WRECKING_CHARGE))
                    Next(player, SPELL_HEROIC_STRIKE, SPELLMOD_COST, -100);
                break;
            case SPELL_COMBUSTION:
                if (has(RUNE_PHOENIX))
                    player->CastSpell(player, SPELL_PHOENIX, true);
                break;
            case SPELL_BLINK:
                if (has(RUNE_FROZEN_STEP))
                {
                    uint32 nova = SPELL_FROST_NOVA;
                    for (uint32 id = SPELL_FROST_NOVA; id; id = sSpellMgr->GetNextSpellInChain(id))
                        if (SpellInfo const* rank = sSpellMgr->GetSpellInfo(id);
                            rank && rank->SpellLevel <= player->GetLevel())
                            nova = id;
                    player->CastSpell(player, nova, true);
                }
                break;
            case SPELL_JUDGEMENT_OF_LIGHT:
            case SPELL_JUDGEMENT_OF_WISDOM:
            case SPELL_JUDGEMENT_OF_JUSTICE:
                if (has(RUNE_HOLY_WRATH))
                    for (uint32 id = SPELL_CONSECRATION; id; id = sSpellMgr->GetNextSpellInChain(id))
                        if (DynamicObject* ground = player->GetDynObject(id))
                            ground->SetDuration(ground->GetDuration() + 2000);
                break;
            case SPELL_ARCANE_BARRAGE:
                if (Aura* stacks = player->GetAura(SPELL_ARCANE_BLAST_STACK))
                    EffectsOf(player).BlastStacks = stacks->GetStackAmount();
                else
                    EffectsOf(player).BlastStacks = 0;
                break;
            case SPELL_INTIMIDATION:
                if (Pet* pet = player->GetPet(); pet && has(RUNE_ALPHA))
                    pet->CastSpell(pet, SPELL_ALPHA, true);
                break;
            case SPELL_DISENGAGE:
                if (has(RUNE_HUNTERS_LEAP))
                    player->CastSpell(player, SPELL_HUNTERS_LEAP, true);
                break;
            case SPELL_KILL_COMMAND:
                if (Pet* pet = player->GetPet(); pet && pet->IsAlive() && has(RUNE_PACK_LEADER))
                    Mend(player, pet, float(pet->CountPctFromMaxHealth(10)));
                break;
            case SPELL_RIPTIDE:
                if (has(RUNE_TIDAL_WAVE))
                    Next(player, SPELL_HEALING_WAVE, SPELLMOD_CASTING_TIME, -30);
                break;
            case SPELL_RISING_SUN_KICK:
                if (has(RUNE_RISING_STORM))
                    player->RemoveSpellCooldown(SPELL_FISTS_OF_FURY, true);
                break;
            case SPELL_VENGEFUL_RETREAT:
                if (has(RUNE_VENGEFUL_STEP))
                    player->RemoveSpellCooldown(SPELL_FEL_RUSH, true);
                break;
            case SPELL_EYE_BEAM:
                if (has(RUNE_UNENDING_FURY) && !player->HasAura(SPELL_METAMORPHOSIS_DH))
                {
                    player->CastSpell(player, SPELL_METAMORPHOSIS_DH, true);
                    if (Aura* form = player->GetAura(SPELL_METAMORPHOSIS_DH))
                    {
                        form->SetMaxDuration(6000);
                        form->SetDuration(6000);
                    }
                }
                break;
            case SPELL_CONFLAGRATE:
                if (has(RUNE_BACKDRAFT))
                {
                    fx.Backdraft = 2;
                    Next(player, SPELL_INCINERATE, SPELLMOD_CASTING_TIME, -20);
                    Next(player, SPELL_SHADOW_BOLT, SPELLMOD_CASTING_TIME, -20);
                }
                break;
            case SPELL_IMMOLATION_AURA:
                fx.ImmolationAdded = 0;
                break;
            default:
                break;
        }
        for (Trim const& trim : TRIMS)
        {
            if (trim.Spell != first || !has(trim.Rune))
                continue;
            ObjectGuid guid = player->GetGUID();
            uint32 pct = trim.Pct;
            player->m_Events.AddEventAtOffset([player, guid, spellId, pct]()
            {
                if (player->GetGUID() != guid)
                    return;
                if (pct >= 100)
                    player->RemoveSpellCooldown(spellId, true);
                else if (uint32 left = player->GetSpellCooldownDelay(spellId))
                    player->ModifySpellCooldown(spellId, -int32(left * pct / 100));
            }, 1ms);
        }
    }
};

// Overpower spends its chance (the aura state the dodge gave), as it spent the combo point before.
class titan_overpower_spend : public PlayerScript
{
public:
    titan_overpower_spend() : PlayerScript("titan_overpower_spend", { PLAYERHOOK_ON_SPELL_CAST }) { }

    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (First(spell->GetSpellInfo()->Id) == SPELL_OVERPOWER)
            player->ModifyAuraState(AURA_STATE_OVERPOWER, false);
    }
};

void AddSC_rune_effects()
{
    new titan_overpower_spend();
    new titan_rune_effects_checks();
    new titan_rune_effects_load();
    new titan_rune_effects_unit();
    new titan_rune_effects_player();
    RegisterSpellScript(spell_titan_rune_tier_hit);
    RegisterSpellScript(spell_titan_rune_tier_spread);
    RegisterSpellScript(spell_titan_rune_tier_ward);
    RegisterSpellScript(spell_titan_rune_tier_quick_recovery);
}
