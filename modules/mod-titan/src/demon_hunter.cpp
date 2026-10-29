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
    SPELL_BLADE_DANCE_STRIKE        = 90107,
    SPELL_BLADE_DANCE_FINAL_STRIKE  = 90108,
    SPELL_DEMON_FORM                = 90115,
    SPELL_METAMORPHOSIS             = 90116,
};

enum DemonHunterDisplays
{
    DISPLAY_DEMON_FORM_MALE         = 90201,    // client-patch/patch_creatures.py
    DISPLAY_DEMON_FORM_FEMALE       = 90202,
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

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_titan_dh_fel_rush::Dash, EFFECT_0, SPELL_EFFECT_CHARGE_DEST);
    }
};

// Fel Rush damage (90104): cast as the Demon Hunter sets off, it hits only the enemies in the lane
// the dash crosses (15 yards ahead, 2 yards each side), not everything around.
class spell_titan_dh_fel_rush_damage : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_fel_rush_damage);

    void KeepPath(std::list<WorldObject*>& targets)
    {
        KeepInLane(GetCaster(), targets, 2.0f);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_titan_dh_fel_rush_damage::KeepPath, EFFECT_0,
            TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

// Eye Beam damage (90106, every tick of the channel): only the enemies along the beam (20 yards
// ahead, 2.5 yards each side), not a wide cone.
class spell_titan_dh_eye_beam_damage : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_eye_beam_damage);

    void KeepBeam(std::list<WorldObject*>& targets)
    {
        KeepInLane(GetCaster(), targets, 2.5f);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_titan_dh_eye_beam_damage::KeepBeam, EFFECT_0,
            TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

// Blade Dance (90102): strikes everything within 8 yards four times during the spin, three times
// for 24.75% of attack power and a last time for 74.2% (spell_bonus_data), a quarter second apart.
class spell_titan_dh_blade_dance : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_blade_dance);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BLADE_DANCE_STRIKE, SPELL_BLADE_DANCE_FINAL_STRIKE });
    }

    void Strike()
    {
        Unit* caster = GetCaster();
        caster->CastSpell(caster, SPELL_BLADE_DANCE_STRIKE, true);
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
        Player* player = target->ToPlayer();
        uint8 gender = player ? player->GetByteValue(PLAYER_BYTES_3, 0) : target->getGender();
        target->SetDisplayId(gender == GENDER_FEMALE ? DISPLAY_DEMON_FORM_FEMALE : DISPLAY_DEMON_FORM_MALE,
            target->GetObjectScale());
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

    void Restore(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_DEMON_FORM);
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
}
