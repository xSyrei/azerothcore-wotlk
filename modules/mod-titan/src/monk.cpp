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
#include "TemporarySummon.h"

// The Monk (client-patch/patch_monk.py): what its spells, copies of Blizzard healing spells, cannot do by
// themselves. Mistweaver first.

enum MonkSpells
{
    SPELL_RENEWING_MIST             = 90302,
    SPELL_ENVELOPING_MIST           = 90303,
    SPELL_VIVIFY_SPLASH             = 90310,
    SPELL_INVOKE_CHI_JI             = 90311,
    SPELL_CRANES_GUST               = 90312,
    SPELL_BLACKOUT_KICK             = 90321,
    SPELL_RISING_SUN_KICK           = 90322,
    SPELL_FISTS_OF_FURY             = 90323,
    SPELL_SPINNING_CRANE_KICK       = 90325,
    SPELL_STORM_EARTH_AND_FIRE      = 90329,
    SPELL_EARTH_SPIRIT_STRIKE       = 90330,
    SPELL_FIRE_SPIRIT_STRIKE        = 90331,
    SPELL_EARTH_SPIRIT_AURA         = 90332,
    SPELL_FIRE_SPIRIT_AURA          = 90333,
    SPELL_CLONE_ME                  = 45204,    // Mirror Image's: the summon looks like the caster
};

enum MonkCreatures
{
    NPC_CHI_JI                      = 90311,
    NPC_EARTH_SPIRIT                = 90330,
    NPC_FIRE_SPIRIT                 = 90331,
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

// Windwalker's finishers (Blackout Kick, Rising Sun Kick, Fists of Fury, Spinning Crane Kick): usable from 1 combo
// point (Tiger Palm awards them, on the Monk's current target, at most 5), they take them all and hit as hard as the
// points spent: their spell values are the 5-point ones, scaled by points / 5.
constexpr uint8 MAX_COMBO_POINTS = 5;

class spell_titan_monk_finisher : public SpellScript
{
    PrepareSpellScript(spell_titan_monk_finisher);

    uint8 _points = 0;

    SpellCastResult CheckPoints()
    {
        return GetCaster()->GetComboPoints() ? SPELL_CAST_OK : SPELL_FAILED_NO_COMBO_POINTS;
    }

    void TakePoints()
    {
        _points = GetCaster()->GetComboPoints();
    }

    void Scale()
    {
        SetHitDamage(GetHitDamage() * _points / MAX_COMBO_POINTS);
    }

    void ClearPoints()
    {
        GetCaster()->ClearComboPoints();
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_titan_monk_finisher::CheckPoints);
        BeforeCast += SpellCastFn(spell_titan_monk_finisher::TakePoints);
        OnHit += SpellHitFn(spell_titan_monk_finisher::Scale);
        AfterCast += SpellCastFn(spell_titan_monk_finisher::ClearPoints);
    }
};

// Fists of Fury (90323) and Spinning Crane Kick (90325): their strikes (weapon damage %) as strong as the combo points
// they were cast with (read when the channel starts, before the cast takes them).
class spell_titan_monk_finisher_channel : public AuraScript
{
    PrepareAuraScript(spell_titan_monk_finisher_channel);

    uint8 _points = MAX_COMBO_POINTS;

    void TakePoints(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Unit* caster = GetCaster())
            _points = std::max<uint8>(1, caster->GetComboPoints());
    }

    void Strike(AuraEffect const* aurEff)
    {
        PreventDefaultAction();
        Unit* caster = GetCaster();
        SpellInfo const* strike = sSpellMgr->GetSpellInfo(aurEff->GetSpellInfo()->Effects[aurEff->GetEffIndex()]
            .TriggerSpell);
        if (!caster || !strike)
            return;
        int32 weaponPct = strike->Effects[EFFECT_0].CalcValue(caster) * _points / MAX_COMBO_POINTS;
        caster->CastCustomSpell(strike->Id, SPELLVALUE_BASE_POINT0, weaponPct, caster, true, nullptr, aurEff);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_titan_monk_finisher_channel::TakePoints, EFFECT_0,
            SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_titan_monk_finisher_channel::Strike, EFFECT_0,
            SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// Blackout Kick (90321): Rising Sun Kick's and Fists of Fury's cooldowns 1 sec shorter.
class spell_titan_monk_blackout_kick : public SpellScript
{
    PrepareSpellScript(spell_titan_monk_blackout_kick);

    static constexpr int32 COOLDOWN_CUT = 1000;

    void CutCooldowns()
    {
        if (Player* player = GetCaster()->ToPlayer())
            for (uint32 spellId : { SPELL_RISING_SUN_KICK, SPELL_FISTS_OF_FURY })
                player->ModifySpellCooldown(spellId, -COOLDOWN_CUT);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_monk_blackout_kick::CutCooldowns);
    }
};

// Whirling Dragon Punch (90327): only while Rising Sun Kick and Fists of Fury are both cooling down.
class spell_titan_monk_whirling_dragon_punch : public SpellScript
{
    PrepareSpellScript(spell_titan_monk_whirling_dragon_punch);

    SpellCastResult CheckCooldowns()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player || !player->HasSpellCooldown(SPELL_RISING_SUN_KICK)
            || !player->HasSpellCooldown(SPELL_FISTS_OF_FURY))
            return SPELL_FAILED_NOT_READY;
        return SPELL_CAST_OK;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_titan_monk_whirling_dragon_punch::CheckCooldowns);
    }
};

// Storm, Earth, and Fire (90329): while the Monk's spirits are out, every Windwalker strike the Monk lands is echoed
// by each spirit on the same enemy, for 35% of its damage (Earth: Nature, Fire: Fire).
class spell_titan_monk_sef_echo : public SpellScript
{
    PrepareSpellScript(spell_titan_monk_sef_echo);

    static constexpr int32 ECHO_PCT = 35;

    void Echo()
    {
        Unit* monk = GetCaster();
        Unit* target = GetHitUnit();
        int32 damage = CalculatePct(GetHitDamage(), ECHO_PCT);
        if (!target || damage <= 0 || !monk->HasAura(SPELL_STORM_EARTH_AND_FIRE))
            return;
        for (Unit* controlled : monk->m_Controlled)
        {
            uint32 strike = controlled->GetEntry() == NPC_EARTH_SPIRIT ? SPELL_EARTH_SPIRIT_STRIKE
                : controlled->GetEntry() == NPC_FIRE_SPIRIT ? SPELL_FIRE_SPIRIT_STRIKE : 0;
            if (strike && controlled->IsAlive())
                controlled->CastCustomSpell(strike, SPELLVALUE_BASE_POINT0, damage, target, true, nullptr, nullptr,
                    monk->GetGUID());
        }
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_monk_sef_echo::Echo);
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

// Invoke Chi-Ji: with Chi-Ji out, every healing spell the Monk casts (any class's: a heal, a heal over time, or a
// channel or aura pulsing heals) sends one more Crane's Gust. Triggered spells (procs, ticks) do not.
class titan_monk_chi_ji_casts : public PlayerScript
{
public:
    titan_monk_chi_ji_casts() : PlayerScript("titan_monk_chi_ji_casts", { PLAYERHOOK_ON_SPELL_CAST }) { }

    static bool Heals(SpellInfo const* spellInfo, bool followTrigger = true)
    {
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            if (effect.IsEffect(SPELL_EFFECT_HEAL) || effect.IsEffect(SPELL_EFFECT_HEAL_PCT)
                || effect.IsEffect(SPELL_EFFECT_HEAL_MAX_HEALTH) || effect.IsAura(SPELL_AURA_PERIODIC_HEAL))
                return true;
            if (followTrigger && (effect.IsAura(SPELL_AURA_PERIODIC_TRIGGER_SPELL)
                || effect.IsEffect(SPELL_EFFECT_TRIGGER_SPELL)))
                if (SpellInfo const* triggered = sSpellMgr->GetSpellInfo(effect.TriggerSpell))
                    if (Heals(triggered, false))
                        return true;
        }
        return false;
    }

    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (spell->IsTriggered() || !Heals(spell->GetSpellInfo()))
            return;
        if (Creature* crane = ChiJiOf(player))
            CranesGust(crane, player, 1);
    }
};

// Storm, Earth, and Fire's spirits (90330 Earth, 90331 Fire): the Monk's look (Mirror Image's Clone Me) wrapped in
// their element (90332, 90333), one at each side; they fight whatever the Monk fights, and echo its strikes
// (spell_titan_monk_sef_echo).
struct npc_titan_monk_sef_spirit : public ScriptedAI
{
    npc_titan_monk_sef_spirit(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        Unit* owner = summoner ? summoner->ToUnit() : nullptr;
        if (!owner)
            return;
        owner->CastSpell(me, SPELL_CLONE_ME, true);
        bool earth = me->GetEntry() == NPC_EARTH_SPIRIT;
        me->CastSpell(me, earth ? SPELL_EARTH_SPIRIT_AURA : SPELL_FIRE_SPIRIT_AURA, true);
        float angle = earth ? float(M_PI) / 2 : float(M_PI) * 3 / 2;
        if (Minion* minion = dynamic_cast<Minion*>(me))
            minion->SetFollowAngle(angle);
        me->SetReactState(REACT_PASSIVE);
        me->GetMotionMaster()->MoveFollow(owner, PET_FOLLOW_DIST, angle);
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        Unit* owner = me->GetOwner();
        if (!owner)
            return;
        Unit* target = owner->GetVictim();
        if (target && target != me->GetVictim() && me->IsValidAttackTarget(target))
            AttackStart(target);
        else if (!target && me->GetVictim())
        {
            EnterEvadeMode();
            me->GetMotionMaster()->MoveFollow(owner, PET_FOLLOW_DIST, me->GetFollowAngle());
        }
        if (me->GetVictim())
            DoMeleeAttackIfReady();
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
    RegisterSpellScript(spell_titan_monk_jade_wind_heal);
    RegisterSpellScript(spell_titan_monk_finisher);
    RegisterSpellScript(spell_titan_monk_finisher_channel);
    RegisterSpellScript(spell_titan_monk_blackout_kick);
    RegisterSpellScript(spell_titan_monk_whirling_dragon_punch);
    RegisterSpellScript(spell_titan_monk_life_cocoon);
    RegisterSpellScript(spell_titan_monk_revival);
    RegisterCreatureAI(npc_titan_chi_ji);
    RegisterCreatureAI(npc_titan_monk_sef_spirit);
    RegisterSpellScript(spell_titan_monk_sef_echo);
    new titan_monk_chi_ji_casts();
    new titan_monk_healing();
}
