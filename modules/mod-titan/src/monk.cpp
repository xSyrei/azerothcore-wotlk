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

#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"

// The Monk (client-patch/patch_monk.py): what its spells, copies of Blizzard healing spells, cannot do by
// themselves. Mistweaver first.

enum MonkSpells
{
    SPELL_RENEWING_MIST             = 90302,
    SPELL_ENVELOPING_MIST           = 90303,
    SPELL_VIVIFY_SPLASH             = 90310,
    SPELL_INVOKE_CHI_JI             = 90311,
    SPELL_CRANES_GUST               = 90312,
};

enum MonkCreatures
{
    NPC_CHI_JI                      = 90311,
};

namespace
{
    // The friendly units within range of center (center included).
    std::list<Unit*> FriendsAround(WorldObject* center, Unit* caster, float range)
    {
        std::list<Unit*> friends;
        Acore::AnyFriendlyUnitInObjectRangeCheck check(center, caster, range);
        Acore::UnitListSearcher<Acore::AnyFriendlyUnitInObjectRangeCheck> searcher(center, friends, check);
        Cell::VisitObjects(center, searcher, range);
        return friends;
    }

    // Chi-Ji, the Red Crane (90311): heals with Crane's Gust (90312) the count most injured allies within 40 yards
    // of the Monk, for its heal at the Monk's level; cast for the Monk (its original caster), the Monk's healing
    // power (spell_bonus_data), crits and healing modifiers apply.
    constexpr float CRANES_GUST_RANGE = 40.0f;

    void CranesGust(Creature* crane, Unit* monk, uint32 count)
    {
        SpellInfo const* gust = sSpellMgr->GetSpellInfo(SPELL_CRANES_GUST);
        if (!gust)
            return;

        std::vector<Unit*> injured;
        for (Unit* ally : FriendsAround(monk, monk, CRANES_GUST_RANGE))
            if (ally->IsAlive() && !ally->IsFullHealth() && ally->GetEntry() != NPC_CHI_JI)
                injured.push_back(ally);
        std::sort(injured.begin(), injured.end(),
            [](Unit* a, Unit* b) { return a->GetHealthPct() < b->GetHealthPct(); });
        if (injured.size() > count)
            injured.resize(count);

        int32 heal = gust->Effects[EFFECT_0].CalcValue(monk);
        for (Unit* ally : injured)
            crane->CastCustomSpell(SPELL_CRANES_GUST, SPELLVALUE_BASE_POINT0, heal, ally, true, nullptr, nullptr,
                monk->GetGUID());
    }

    // The Monk's Chi-Ji, if out.
    Creature* ChiJiOf(Unit* monk)
    {
        if (!monk->HasAura(SPELL_INVOKE_CHI_JI))
            return nullptr;
        for (Unit* controlled : monk->m_Controlled)
            if (controlled->GetEntry() == NPC_CHI_JI && controlled->IsAlive())
                return controlled->ToCreature();
        return nullptr;
    }
}

// Vivify (90301): every ally within 40 yards wearing the Monk's Renewing Mist is healed for half as much
// (90310).
class spell_titan_monk_vivify : public SpellScript
{
    PrepareSpellScript(spell_titan_monk_vivify);

    static constexpr float RANGE = 40.0f;
    static constexpr int32 SPLASH_PCT = 50;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_RENEWING_MIST, SPELL_VIVIFY_SPLASH });
    }

    void Splash()
    {
        Unit* caster = GetCaster();
        int32 amount = CalculatePct(GetHitHeal(), SPLASH_PCT);
        if (amount <= 0)
            return;

        for (Unit* ally : FriendsAround(caster, caster, RANGE))
            if (ally->HasAura(SPELL_RENEWING_MIST, caster->GetGUID()))
                caster->CastCustomSpell(SPELL_VIVIFY_SPLASH, SPELLVALUE_BASE_POINT0, amount, ally, true);
    }

    // With Chi-Ji out, it sends a Crane's Gust as well.
    void Gust()
    {
        if (Creature* crane = ChiJiOf(GetCaster()))
            CranesGust(crane, GetCaster(), 1);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_monk_vivify::Splash);
        AfterCast += SpellCastFn(spell_titan_monk_vivify::Gust);
    }
};

// Renewing Mist (90302): once its target is at full health, the mist (what is left of it) jumps to the most
// injured ally within 20 yards of them that does not have the Monk's Renewing Mist yet.
class spell_titan_monk_renewing_mist : public AuraScript
{
    PrepareAuraScript(spell_titan_monk_renewing_mist);

    static constexpr float JUMP_RANGE = 20.0f;

    void Jump(AuraEffect const* /*aurEff*/)
    {
        Unit* target = GetTarget();
        Unit* caster = GetCaster();
        if (!caster || !target->IsFullHealth())
            return;

        Unit* next = nullptr;
        for (Unit* ally : FriendsAround(target, caster, JUMP_RANGE))
            if (!ally->IsFullHealth() && ally->IsAlive() && !ally->HasAura(SPELL_RENEWING_MIST, caster->GetGUID())
                && (!next || ally->GetHealthPct() < next->GetHealthPct()))
                next = ally;
        if (!next)
            return;

        int32 left = GetDuration();
        if (Aura* mist = caster->AddAura(SPELL_RENEWING_MIST, next))
        {
            mist->SetDuration(left);
            Remove();
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_titan_monk_renewing_mist::Jump, EFFECT_0,
            SPELL_AURA_PERIODIC_HEAL);
    }
};

// Renewing Mist (90302) cast: with Chi-Ji out, it sends a Crane's Gust as well.
class spell_titan_monk_renewing_mist_cast : public SpellScript
{
    PrepareSpellScript(spell_titan_monk_renewing_mist_cast);

    void Gust()
    {
        if (Creature* crane = ChiJiOf(GetCaster()))
            CranesGust(crane, GetCaster(), 1);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_monk_renewing_mist_cast::Gust);
    }
};

// Refreshing Jade Wind's heal (90314): of the allies within 10 yards of the Monk, the 6 most injured.
class spell_titan_monk_jade_wind_heal : public SpellScript
{
    PrepareSpellScript(spell_titan_monk_jade_wind_heal);

    static constexpr std::size_t TARGETS = 6;

    void MostInjured(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject* target)
        {
            Unit* unit = target->ToUnit();
            return !unit || unit->IsFullHealth();
        });
        targets.sort([](WorldObject* a, WorldObject* b)
            { return a->ToUnit()->GetHealthPct() < b->ToUnit()->GetHealthPct(); });
        if (targets.size() > TARGETS)
            targets.resize(TARGETS);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_titan_monk_jade_wind_heal::MostInjured,
            EFFECT_0, TARGET_UNIT_SRC_AREA_ALLY);
    }
};

// Life Cocoon (90306): the shield absorbs as much as 30% of the Monk's maximum health.
class spell_titan_monk_life_cocoon : public AuraScript
{
    PrepareAuraScript(spell_titan_monk_life_cocoon);

    static constexpr int32 HEALTH_PCT = 30;

    void Size(AuraEffect const* /*aurEff*/, int32& amount, bool& canBeRecalculated)
    {
        canBeRecalculated = false;
        if (Unit* caster = GetCaster())
            amount = int32(caster->CountPctFromMaxHealth(HEALTH_PCT));
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_titan_monk_life_cocoon::Size, EFFECT_0,
            SPELL_AURA_SCHOOL_ABSORB);
    }
};

// Revival (90307): each ally it heals is also cleared of one Magic, one Poison and one Disease effect.
class spell_titan_monk_revival : public SpellScript
{
    PrepareSpellScript(spell_titan_monk_revival);

    void Cleanse(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        for (DispelType type : { DISPEL_MAGIC, DISPEL_POISON, DISPEL_DISEASE })
        {
            DispelChargesList auras;
            target->GetDispellableAuraList(caster, 1 << type, auras, GetSpellInfo());
            if (!auras.empty())
            {
                Aura* aura = auras.front().first;
                target->RemoveAurasDueToSpellByDispel(aura->GetId(), GetSpellInfo()->Id, aura->GetCasterGUID(),
                    caster);
            }
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_titan_monk_revival::Cleanse, EFFECT_0, SPELL_EFFECT_HEAL);
    }
};

// Chi-Ji, the Red Crane (90311, summoned by Invoke Chi-Ji): follows the Monk, never fights, and every 2 sec heals the
// two most injured allies around the Monk with Crane's Gust.
struct npc_titan_chi_ji : public ScriptedAI
{
    npc_titan_chi_ji(Creature* creature) : ScriptedAI(creature) { }

    static constexpr Milliseconds GUST_INTERVAL = 2s;
    static constexpr uint32 GUST_TARGETS = 2;

    void IsSummonedBy(WorldObject* summoner) override
    {
        Unit* owner = summoner ? summoner->ToUnit() : nullptr;
        if (!owner)
            return;
        me->SetReactState(REACT_PASSIVE);
        me->GetMotionMaster()->MoveFollow(owner, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
        scheduler.Schedule(GUST_INTERVAL, [this](TaskContext gust)
        {
            if (Unit* monk = me->GetOwner())
                CranesGust(me, monk, GUST_TARGETS);
            gust.Repeat();
        });
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }
};

// Enveloping Mist (90303): the Monk's healing on its target is 30% stronger while it lasts.
class titan_monk_healing : public UnitScript
{
public:
    titan_monk_healing() : UnitScript("titan_monk_healing", true, { UNITHOOK_MODIFY_HEAL_RECEIVED }) { }

    static constexpr int32 ENVELOPING_MIST_PCT = 30;

    void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* /*spellInfo*/) override
    {
        if (target && healer && target->HasAura(SPELL_ENVELOPING_MIST, healer->GetGUID()))
            AddPct(heal, ENVELOPING_MIST_PCT);
    }
};

void AddSC_monk()
{
    RegisterSpellScript(spell_titan_monk_vivify);
    RegisterSpellScript(spell_titan_monk_renewing_mist);
    RegisterSpellScript(spell_titan_monk_renewing_mist_cast);
    RegisterSpellScript(spell_titan_monk_jade_wind_heal);
    RegisterSpellScript(spell_titan_monk_life_cocoon);
    RegisterSpellScript(spell_titan_monk_revival);
    RegisterCreatureAI(npc_titan_chi_ji);
    new titan_monk_healing();
}
