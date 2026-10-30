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
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"

// Demon Hunter abilities are fought with a weapon in each hand: two one-handed weapons, or a
// main-hand and an off-hand weapon. Shields and held-in-off-hand items do not count.
//
// The main hand is checked by the spells themselves (Spell.dbc: one-handed weapon classes, "requires
// main hand weapon"; see client-patch/patch_spells.py). The off hand is checked here: the spell
// attribute for it would also make the ability strike with the off-hand weapon. Heroes all have
// Dual Wield (hero_equipment.cpp).

enum DemonHunterSpells
{
    SPELL_EYE_BEAM                  = 90105,
    SPELL_BLADE_DANCE_STRIKE        = 90107,
    SPELL_BLADE_DANCE_FINAL_STRIKE  = 90108,
    SPELL_DEMON_FORM                = 90115,
    SPELL_METAMORPHOSIS             = 90116,
    SPELL_BLUR                      = 90113,
    SPELL_FEL_BARRAGE_BALL          = 90123,
    // What the talents cast (client-patch/patch_talents.py, HELPERS)
    SPELL_DEMONIC_APPETITE          = 90150,
    SPELL_PREPARED                  = 90151,
    SPELL_BLOODLET                  = 90152,
    SPELL_MASTER_OF_THE_GLAIVE      = 90153,
    SPELL_MOMENTUM                  = 90154,
    SPELL_CHAOS_CLEAVE              = 90155,
    SPELL_TRAIL_OF_RUIN             = 90157,
    SPELL_SOUL_RENDING              = 90158,
    SPELL_UNBOUND_CHAOS             = 90159,
};

// The Havoc talents the scripts read, by rank (client-patch/patch_talents.py, HAVOC): each rank a passive aura
// whose amount is the talent's number.
constexpr std::array<uint32, 3> TALENT_BLIND_FURY = { 90200, 90201, 90202 };              // Fury per hit: effect 2
constexpr std::array<uint32, 3> TALENT_DEMONIC_APPETITE = { 90205, 90206, 90207 };        // % chance
constexpr std::array<uint32, 2> TALENT_PREPARED = { 90208, 90209 };                       // Fury per sec
constexpr std::array<uint32, 3> TALENT_BLOODLET = { 90210, 90211, 90212 };                // % of the hit
constexpr std::array<uint32, 1> TALENT_MASTER_OF_THE_GLAIVE = { 90217 };                  // slow: effect 2
constexpr std::array<uint32, 2> TALENT_MOMENTUM = { 90225, 90226 };                       // % damage
constexpr std::array<uint32, 3> TALENT_CHAOS_CLEAVE = { 90227, 90228, 90229 };            // % of the strike
constexpr std::array<uint32, 2> TALENT_FEL_MASTERY = { 90231, 90232 };                    // Fury: effect 2
constexpr std::array<uint32, 3> TALENT_CYCLE_OF_HATRED = { 90236, 90237, 90238 };         // sec
constexpr std::array<uint32, 2> TALENT_TRAIL_OF_RUIN = { 90239, 90240 };                  // % of the strike
constexpr std::array<uint32, 2> TALENT_UNBOUND_CHAOS = { 90246, 90247 };                  // % damage
constexpr std::array<uint32, 1> TALENT_DEMONIC = { 90248 };                               // sec
constexpr int32 FURY_UNITS = 10;    // Fury is Runic Power, counted in tenths

enum DemonHunterDisplays
{
    DISPLAY_DEMON_FORM_MALE         = 90201,    // client-patch/patch_creatures.py
    DISPLAY_DEMON_FORM_FEMALE       = 90202,
    DISPLAY_BLADE_DANCE_MALE        = 90203,    // the demon forms with Blade Dance as every stance and movement
    DISPLAY_BLADE_DANCE_FEMALE      = 90204,
};

class spell_titan_dh_dual_weapons : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_dual_weapons);

    SpellCastResult CheckOffHand()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return SPELL_CAST_OK;

        Item* offHand = player->GetWeaponForAttack(OFF_ATTACK, true);
        if (!offHand || offHand->IsBroken() || !offHand->IsFitToSpellRequirements(GetSpellInfo()))
            return SPELL_FAILED_EQUIPPED_ITEM_CLASS_OFFHAND;

        return SPELL_CAST_OK;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_titan_dh_dual_weapons::CheckOffHand);
    }
};

namespace
{
    // A talent's number for the unit: the amount of its highest rank's aura (effect effIndex), 0 without it.
    template <std::size_t N>
    int32 TalentValue(Unit const* unit, std::array<uint32, N> const& ranks, uint8 effIndex = EFFECT_0)
    {
        int32 value = 0;
        for (uint32 rank : ranks)
            if (AuraEffect const* effect = unit->GetAuraEffect(rank, effIndex))
                value = effect->GetAmount();
        return value;
    }

    // Casts a talent's helper spell with its first effect's value set (damage, healing, Fury, a %).
    void CastWithValue(Unit* caster, uint32 spellId, int32 value, Unit* target)
    {
        if (value > 0)
            caster->CastCustomSpell(spellId, SPELLVALUE_BASE_POINT0, value, target, true);
    }

    bool InDemonForm(Unit const* unit)
    {
        return unit->HasAura(SPELL_DEMON_FORM) || unit->HasAura(SPELL_METAMORPHOSIS);
    }

    // The demon form display of the unit's own gender (a player's chosen one, not the form's).
    uint32 DemonFormDisplay(Unit const* unit, bool bladeDance)
    {
        Player const* player = unit->ToPlayer();
        bool female = (player ? player->GetByteValue(PLAYER_BYTES_3, 0) : unit->getGender()) == GENDER_FEMALE;
        if (bladeDance)
            return female ? DISPLAY_BLADE_DANCE_FEMALE : DISPLAY_BLADE_DANCE_MALE;
        return female ? DISPLAY_DEMON_FORM_FEMALE : DISPLAY_DEMON_FORM_MALE;
    }

    // Keeps only the targets in a lane straight ahead of the caster, halfWidth yards to each side
    // (the area's radius sets its length).
    void KeepInLane(Unit* caster, std::list<WorldObject*>& targets, float halfWidth)
    {
        targets.remove_if([caster, halfWidth](WorldObject* target)
        {
            return !caster->HasInLine(target, target->GetObjectSize(), halfWidth);
        });
    }
}

// Fel Rush (90103): the core's charge to a point (Spell::EffectChargeDest), at Fel Rush's own
// speed: a snap across the 15 yards instead of the charge speed's third of a second.
class spell_titan_dh_fel_rush : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_fel_rush);

    static constexpr float DASH_SPEED = 180.0f;     // yards per second (charges: 42)

    void Dash(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        Unit* caster = GetCaster();
        WorldLocation const* dest = GetExplTargetDest();
        if (!dest)
            return;

        Position pos = dest->GetPosition();
        if (!caster->IsWithinLOS(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ()))
            pos = caster->GetFirstCollisionPosition(caster->GetDistance(pos), caster->GetRelativeAngle(&pos));

        caster->GetMotionMaster()->MoveCharge(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), DASH_SPEED);
        if (Player* player = caster->ToPlayer())
            sScriptMgr->AnticheatSetUnderACKmount(player);
    }

    // Momentum (talent): a damage buff.
    void Gather()
    {
        Unit* caster = GetCaster();
        CastWithValue(caster, SPELL_MOMENTUM, TalentValue(caster, TALENT_MOMENTUM), caster);
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_titan_dh_fel_rush::Dash, EFFECT_0, SPELL_EFFECT_CHARGE_DEST);
        AfterCast += SpellCastFn(spell_titan_dh_fel_rush::Gather);
    }
};

// Fel Rush damage (90104): cast as the Demon Hunter sets off, it hits only the enemies in the lane
// the dash crosses (15 yards ahead, 2 yards each side), not everything around. Fel Mastery (talent): Fury
// when it hits; Unbound Chaos (talent): the buff that made it stronger is used up.
class spell_titan_dh_fel_rush_damage : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_fel_rush_damage);

    void KeepPath(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster();
        KeepInLane(caster, targets, 2.0f);
        if (!targets.empty())
            if (int32 fury = TalentValue(caster, TALENT_FEL_MASTERY, EFFECT_1))
                caster->ModifyPower(POWER_RUNIC_POWER, fury * FURY_UNITS);
    }

    void UseUpUnboundChaos()
    {
        GetCaster()->RemoveAurasDueToSpell(SPELL_UNBOUND_CHAOS);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_titan_dh_fel_rush_damage::KeepPath, EFFECT_0,
            TARGET_UNIT_SRC_AREA_ENEMY);
        AfterCast += SpellCastFn(spell_titan_dh_fel_rush_damage::UseUpUnboundChaos);
    }
};

// Eye Beam damage (90106, every tick of the channel): only the enemies along the beam (20 yards
// ahead, 2.5 yards each side), not a wide cone. Blind Fury (talent): Fury every time it strikes.
class spell_titan_dh_eye_beam_damage : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_eye_beam_damage);

    void KeepBeam(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster();
        KeepInLane(caster, targets, 2.5f);
        if (!targets.empty())
            if (int32 fury = TalentValue(caster, TALENT_BLIND_FURY, EFFECT_1))
                caster->ModifyPower(POWER_RUNIC_POWER, fury * FURY_UNITS);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_titan_dh_eye_beam_damage::KeepBeam, EFFECT_0,
            TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

// Blade Dance (90102): strikes everything within 8 yards four times during the spin, three times
// for 24.75% of attack power and a last time for 74.2% (spell_bonus_data), a quarter second apart.
// Auto-attack waits for the spin to end: a swing's animation would replace it. For the dance the Demon Hunter
// shows the Blade Dance variant of the demon form (in or out of it): the client gives a moving character's legs
// to the movement animation, and in that variant every movement animation is the dance. Then the demon form
// again if in it, or the character.
class spell_titan_dh_blade_dance : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_blade_dance);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BLADE_DANCE_STRIKE, SPELL_BLADE_DANCE_FINAL_STRIKE });
    }

    static constexpr int32 SPIN_MS = 1000;
    static constexpr int32 DANCE_MS = 900;                  // the dance's three parts (DHBladeDance 1 to 3)

    void Strike()
    {
        Unit* caster = GetCaster();
        for (WeaponAttackType attack : { BASE_ATTACK, OFF_ATTACK })
            if (caster->getAttackTimer(attack) < SPIN_MS)
                caster->setAttackTimer(attack, SPIN_MS);
        caster->CastSpell(caster, SPELL_BLADE_DANCE_STRIKE, true);
        uint32 dance = DemonFormDisplay(caster, true);
        caster->SetDisplayId(dance, caster->GetObjectScale());
        caster->m_Events.AddEventAtOffset([caster, dance]()
        {
            if (caster->GetDisplayId() != dance)
                return;                                     // something else changed the look meanwhile
            if (InDemonForm(caster))
                caster->SetDisplayId(DemonFormDisplay(caster, false), caster->GetObjectScale());
            else
                caster->RestoreDisplayId();
        }, std::chrono::milliseconds(DANCE_MS));
        for (auto [delay, spellId] : { std::pair{ 250ms, SPELL_BLADE_DANCE_STRIKE },
            std::pair{ 500ms, SPELL_BLADE_DANCE_STRIKE }, std::pair{ 750ms, SPELL_BLADE_DANCE_FINAL_STRIKE } })
        {
            caster->m_Events.AddEventAtOffset([caster, spellId]()
            {
                if (caster->IsAlive())
                    caster->CastSpell(caster, spellId, true);
            }, delay);
        }
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_dh_blade_dance::Strike);
    }
};

// Demon Form (90115) and Metamorphosis (90116): the Demon Hunter's demon shape for as long as the aura lasts,
// the female or male one by the character's own gender. Either keeps the shape while the other is still on
// (an Eye Beam channeled in Metamorphosis ends without turning the Demon Hunter back).
class spell_titan_dh_demon_form : public AuraScript
{
    PrepareAuraScript(spell_titan_dh_demon_form);

    void Transform(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        target->SetDisplayId(DemonFormDisplay(target, false), target->GetObjectScale());
    }

    void Restore(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        for (uint32 spellId : { SPELL_DEMON_FORM, SPELL_METAMORPHOSIS })
            if (spellId != GetId() && target->HasAura(spellId))
                return;
        target->RestoreDisplayId();
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_titan_dh_demon_form::Transform, EFFECT_0, SPELL_AURA_DUMMY,
            AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_titan_dh_demon_form::Restore, EFFECT_0, SPELL_AURA_DUMMY,
            AURA_EFFECT_HANDLE_REAL);
    }
};

// Eye Beam (90105): the Demon Hunter takes demon form for the whole channel.
class spell_titan_dh_eye_beam : public AuraScript
{
    PrepareAuraScript(spell_titan_dh_eye_beam);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DEMON_FORM });
    }

    void Transform(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_DEMON_FORM, true);
        // Outlasts the channel: the channel's end takes it off (below), so the form never drops early.
        if (Aura* form = target->GetAura(SPELL_DEMON_FORM))
        {
            form->SetMaxDuration(GetMaxDuration() + 1000);
            form->SetDuration(GetMaxDuration() + 1000);
        }
    }

    // Demonic (talent): Metamorphosis for a while when the channel ends, before the demon form drops (so the
    // shape stays).
    void Restore(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        if (int32 seconds = TalentValue(target, TALENT_DEMONIC); seconds && target->IsAlive()
            && !target->HasAura(SPELL_METAMORPHOSIS))
        {
            target->CastSpell(target, SPELL_METAMORPHOSIS, true);
            if (Aura* metamorphosis = target->GetAura(SPELL_METAMORPHOSIS))
            {
                metamorphosis->SetMaxDuration(seconds * IN_MILLISECONDS);
                metamorphosis->SetDuration(seconds * IN_MILLISECONDS);
            }
        }
        target->RemoveAurasDueToSpell(SPELL_DEMON_FORM);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_titan_dh_eye_beam::Transform, EFFECT_0,
            SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_titan_dh_eye_beam::Restore, EFFECT_0,
            SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// Chaos Nova (90112): the Demon Hunter flashes into demon form for a moment (Demon Form's own 1 sec).
class spell_titan_dh_chaos_nova : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_chaos_nova);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DEMON_FORM });
    }

    void Transform()
    {
        Unit* caster = GetCaster();
        caster->CastSpell(caster, SPELL_DEMON_FORM, true);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_dh_chaos_nova::Transform);
    }
};

// Darkness (90118): its persistent area shields the allies in it with no set amount (-1); each hit has
// a 20% chance to be absorbed whole, and the shield takes nothing from the others.
class spell_titan_dh_darkness : public AuraScript
{
    PrepareAuraScript(spell_titan_dh_darkness);

    static constexpr uint32 AVOID_CHANCE = 20;

    void Avoid(AuraEffect* /*aurEff*/, DamageInfo& dmgInfo, uint32& absorbAmount)
    {
        absorbAmount = roll_chance_i(AVOID_CHANCE) ? dmgInfo.GetDamage() : 0;
    }

    void Register() override
    {
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_titan_dh_darkness::Avoid, EFFECT_0);
    }
};

// Fel Barrage (90121): each pulse drains 5 Fury (Fury is Runic Power, in tenths); with less left the barrage
// ends instead of pulsing.
class spell_titan_dh_fel_barrage : public AuraScript
{
    PrepareAuraScript(spell_titan_dh_fel_barrage);

    static constexpr int32 FURY_PER_PULSE = 50;

    void Drain(AuraEffect const* /*aurEff*/)
    {
        Unit* target = GetTarget();
        if (target->GetPower(POWER_RUNIC_POWER) < FURY_PER_PULSE)
        {
            PreventDefaultAction();
            Remove();
            return;
        }
        target->ModifyPower(POWER_RUNIC_POWER, -FURY_PER_PULSE);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_titan_dh_fel_barrage::Drain, EFFECT_0,
            SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// Fel Barrage's pulse (90122): BALLS_PER_PULSE fel balls (90123, only a look) fall on random spots on the
// ground within its 8 yards.
class spell_titan_dh_fel_barrage_pulse : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_fel_barrage_pulse);

    static constexpr uint32 BALLS_PER_PULSE = 2;
    static constexpr float RADIUS = 8.0f;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_FEL_BARRAGE_BALL });
    }

    void Bombard()
    {
        Unit* caster = GetCaster();
        for (uint32 i = 0; i < BALLS_PER_PULSE; ++i)
        {
            Position spot = caster->GetRandomPoint(*caster, RADIUS);
            caster->CastSpell(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), SPELL_FEL_BARRAGE_BALL,
                true);
        }
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_dh_fel_barrage_pulse::Bombard);
    }
};

// Chaos Strike (90101) and its talents: Demonic Appetite (a chance to heal and gain Fury), Chaos Cleave (a share
// of the hit to every other enemy near the target) and Cycle of Hatred (Eye Beam's cooldown shortened).
class spell_titan_dh_chaos_strike : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_chaos_strike);

    static constexpr float CLEAVE_RADIUS = 8.0f;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DEMONIC_APPETITE, SPELL_CHAOS_CLEAVE, SPELL_EYE_BEAM });
    }

    void Empower()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        if (int32 chance = TalentValue(caster, TALENT_DEMONIC_APPETITE); chance && roll_chance_i(chance))
            caster->CastSpell(caster, SPELL_DEMONIC_APPETITE, true);

        if (int32 share = TalentValue(caster, TALENT_CHAOS_CLEAVE))
        {
            int32 damage = CalculatePct(GetHitDamage(), share);
            std::list<Unit*> enemies;
            Acore::AnyUnfriendlyUnitInObjectRangeCheck check(target, caster, CLEAVE_RADIUS);
            Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(target, enemies, check);
            Cell::VisitObjects(target, searcher, CLEAVE_RADIUS);
            for (Unit* enemy : enemies)
                if (enemy != target && caster->IsValidAttackTarget(enemy))
                    CastWithValue(caster, SPELL_CHAOS_CLEAVE, damage, enemy);
        }

        if (Player* player = caster->ToPlayer())
            if (int32 seconds = TalentValue(caster, TALENT_CYCLE_OF_HATRED))
                player->ModifySpellCooldown(SPELL_EYE_BEAM, -seconds * IN_MILLISECONDS);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_dh_chaos_strike::Empower);
    }
};

// Vengeful Retreat (90109) and its talents: Prepared (Fury over 5 sec) and Momentum (a damage buff).
class spell_titan_dh_vengeful_retreat : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_vengeful_retreat);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PREPARED, SPELL_MOMENTUM });
    }

    void Empower()
    {
        Unit* caster = GetCaster();
        CastWithValue(caster, SPELL_PREPARED, TalentValue(caster, TALENT_PREPARED) * FURY_UNITS, caster);
        CastWithValue(caster, SPELL_MOMENTUM, TalentValue(caster, TALENT_MOMENTUM), caster);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_dh_vengeful_retreat::Empower);
    }
};

// Immolation Aura (90119) and Unbound Chaos (talent): the next Fel Rush is stronger.
class spell_titan_dh_immolation_aura : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_immolation_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_UNBOUND_CHAOS });
    }

    void Empower()
    {
        Unit* caster = GetCaster();
        CastWithValue(caster, SPELL_UNBOUND_CHAOS, TalentValue(caster, TALENT_UNBOUND_CHAOS), caster);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_dh_immolation_aura::Empower);
    }
};

// Throw Glaive (90111) and its talents, on each enemy it hits: Bloodlet (a share of the hit as a bleed over
// 6 sec, 3 ticks) and Master of the Glaive (a slow).
class spell_titan_dh_throw_glaive : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_throw_glaive);

    static constexpr int32 BLEED_TICKS = 3;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BLOODLET, SPELL_MASTER_OF_THE_GLAIVE });
    }

    void Empower()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        if (int32 share = TalentValue(caster, TALENT_BLOODLET))
            CastWithValue(caster, SPELL_BLOODLET, CalculatePct(GetHitDamage(), share) / BLEED_TICKS, target);
        if (TalentValue(caster, TALENT_MASTER_OF_THE_GLAIVE, EFFECT_1))
            caster->CastSpell(target, SPELL_MASTER_OF_THE_GLAIVE, true);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_dh_throw_glaive::Empower);
    }
};

// Blade Dance's last strike (90108) and Trail of Ruin (talent): a share of the hit burns over 4 sec (4 ticks).
class spell_titan_dh_blade_dance_final_strike : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_blade_dance_final_strike);

    static constexpr int32 BURN_TICKS = 4;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_TRAIL_OF_RUIN });
    }

    void Empower()
    {
        Unit* caster = GetCaster();
        if (Unit* target = GetHitUnit())
            if (int32 share = TalentValue(caster, TALENT_TRAIL_OF_RUIN))
                CastWithValue(caster, SPELL_TRAIL_OF_RUIN, CalculatePct(GetHitDamage(), share) / BURN_TICKS, target);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_titan_dh_blade_dance_final_strike::Empower);
    }
};

// Desperate Instincts (talent, 90218-90219): a hit that leaves the Demon Hunter below 35% health makes them Blur,
// at most once every (the talent's number) seconds.
class spell_titan_dh_desperate_instincts : public AuraScript
{
    PrepareAuraScript(spell_titan_dh_desperate_instincts);

    static constexpr int32 HEALTH_PCT = 35;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BLUR });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damage = eventInfo.GetDamageInfo();
        Unit* target = GetTarget();
        return damage && damage->GetDamage() && target->IsAlive() && target->HealthBelowPct(HEALTH_PCT)
            && GameTime::GetGameTime() >= _ready;
    }

    void Blur(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        _ready = GameTime::GetGameTime() + Seconds(aurEff->GetAmount());
        GetTarget()->CastSpell(GetTarget(), SPELL_BLUR, true);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_titan_dh_desperate_instincts::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_titan_dh_desperate_instincts::Blur, EFFECT_0, SPELL_AURA_DUMMY);
    }

private:
    Seconds _ready = 0s;
};

// Soul Rending (talent, 90241-90243): heals the Demon Hunter for (the talent's number)% of the damage they
// deal, twice as much in Metamorphosis.
class spell_titan_dh_soul_rending : public AuraScript
{
    PrepareAuraScript(spell_titan_dh_soul_rending);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SOUL_RENDING });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damage = eventInfo.GetDamageInfo();
        return damage && damage->GetDamage() && GetTarget()->IsAlive();
    }

    void Leech(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* target = GetTarget();
        int32 pct = aurEff->GetAmount() * (target->HasAura(SPELL_METAMORPHOSIS) ? 2 : 1);
        CastWithValue(target, SPELL_SOUL_RENDING, CalculatePct(int32(eventInfo.GetDamageInfo()->GetDamage()), pct),
            target);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_titan_dh_soul_rending::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_titan_dh_soul_rending::Leech, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

void AddSC_demon_hunter()
{
    RegisterSpellScript(spell_titan_dh_dual_weapons);
    RegisterSpellScript(spell_titan_dh_blade_dance);
    RegisterSpellScript(spell_titan_dh_fel_rush);
    RegisterSpellScript(spell_titan_dh_fel_rush_damage);
    RegisterSpellScript(spell_titan_dh_eye_beam_damage);
    RegisterSpellScript(spell_titan_dh_demon_form);
    RegisterSpellScript(spell_titan_dh_eye_beam);
    RegisterSpellScript(spell_titan_dh_chaos_nova);
    RegisterSpellScript(spell_titan_dh_darkness);
    RegisterSpellScript(spell_titan_dh_fel_barrage);
    RegisterSpellScript(spell_titan_dh_fel_barrage_pulse);
    RegisterSpellScript(spell_titan_dh_chaos_strike);
    RegisterSpellScript(spell_titan_dh_vengeful_retreat);
    RegisterSpellScript(spell_titan_dh_immolation_aura);
    RegisterSpellScript(spell_titan_dh_throw_glaive);
    RegisterSpellScript(spell_titan_dh_blade_dance_final_strike);
    RegisterSpellScript(spell_titan_dh_desperate_instincts);
    RegisterSpellScript(spell_titan_dh_soul_rending);
}
