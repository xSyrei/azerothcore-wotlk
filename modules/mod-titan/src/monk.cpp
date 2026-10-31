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

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_monk_vivify::Splash);
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
    RegisterSpellScript(spell_titan_monk_life_cocoon);
    RegisterSpellScript(spell_titan_monk_revival);
    new titan_monk_healing();
}
