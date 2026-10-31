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
    SPELL_DEMON_SPIKES              = 90127,
    SPELL_FIERY_BRAND               = 90128,
    SPELL_FRAILTY                   = 90137,
    SPELL_SOUL_CLEAVE_HEAL          = 90131,
    SPELL_SIGIL_OF_CHAINS_PULL      = 90133,
    SPELL_SPIRIT_BOMB_BLAST         = 90136,
    SPELL_FRAILTY_HEAL              = 90139,
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
    SPELL_INERTIA                   = 90160,
    SPELL_INFERNAL_ARMOR            = 90161,
    SPELL_FEAST_OF_SOULS            = 90162,
    SPELL_VOID_REAVER               = 90163,
    SPELL_RAZOR_SPIKES              = 90164,
    SPELL_PAINBRINGER               = 90165,
    SPELL_FEL_FLAME_FORTIFICATION   = 90166,
    SPELL_UNCONTAINED_FEL           = 90167,
};

// The Havoc talents the scripts read, by rank (client-patch/patch_talents.py, HAVOC): each rank a passive aura
// whose amount is the talent's number.
constexpr std::array<uint32, 3> TALENT_BLIND_FURY = { 90200, 90201, 90202 };              // Fury per hit
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
constexpr std::array<uint32, 1> TALENT_ETERNAL_REAPING = { 90251 };
constexpr std::array<uint32, 1> TALENT_INERTIA = { 90252 };                               // % damage
// The Vengeance talents the scripts read, as the Havoc ones.
constexpr std::array<uint32, 1> TALENT_SHEAR_FURY = { 90255 };                            // Fury
constexpr std::array<uint32, 2> TALENT_INFERNAL_ARMOR = { 90259, 90260 };                 // armor %, % of AP
constexpr std::array<uint32, 3> TALENT_RAZOR_SPIKES = { 90261, 90262, 90263 };            // % damage
constexpr std::array<uint32, 2> TALENT_PAINBRINGER = { 90264, 90265 };                    // % less damage taken
constexpr std::array<uint32, 2> TALENT_QUICKENED_SIGILS = { 90266, 90267 };               // ms sooner
constexpr std::array<uint32, 1> TALENT_CONCENTRATED_SIGILS = { 90268 };                   // yards
constexpr std::array<uint32, 3> TALENT_CHARRED_FLESH = { 90271, 90272, 90273 };           // ms longer
constexpr std::array<uint32, 2> TALENT_FALLOUT = { 90274, 90275 };                        // % chance
constexpr std::array<uint32, 2> TALENT_COVER_OF_DARKNESS = { 90276, 90277 };              // % more avoidance
constexpr std::array<uint32, 1> TALENT_BURNING_ALIVE = { 90278 };
constexpr std::array<uint32, 2> TALENT_FEED_THE_DEMON = { 90279, 90280 };                 // ms per combo point
constexpr std::array<uint32, 3> TALENT_SHATTERED_SOULS = { 90529, 90530, 90531 };          // % chance
constexpr std::array<uint32, 2> TALENT_FEAST_OF_SOULS = { 90281, 90282 };                 // % of maximum health
constexpr std::array<uint32, 2> TALENT_FIERY_DEMISE = { 90283, 90284 };                   // % more Fire damage
constexpr std::array<uint32, 2> TALENT_RUINOUS_BULWARK = { 90286, 90287 };                // % more absorbed
constexpr std::array<uint32, 3> TALENT_VOID_REAVER = { 90288, 90289, 90290 };             // % less damage dealt
constexpr std::array<uint32, 2> TALENT_VULNERABILITY = { 90291, 90292 };                  // % more damage taken
constexpr std::array<uint32, 1> TALENT_LAST_RESORT = { 90285 };                           // % health left
constexpr std::array<uint32, 2> TALENT_FEL_FLAME_FORTIFICATION = { 90296, 90297 };        // % less damage taken
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
// when it hits; Unbound Chaos (talent): the buff that made it stronger is used up, and with Inertia (talent)
// turns into a damage buff.
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
        Unit* caster = GetCaster();
        if (!caster->HasAura(SPELL_UNBOUND_CHAOS))
            return;

        caster->RemoveAurasDueToSpell(SPELL_UNBOUND_CHAOS);
        CastWithValue(caster, SPELL_INERTIA, TalentValue(caster, TALENT_INERTIA), caster);
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
            if (int32 fury = TalentValue(caster, TALENT_BLIND_FURY))
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
// a 20% chance to be absorbed whole (more with the caster's Cover of Darkness, talent), and the shield takes
// nothing from the others.
class spell_titan_dh_darkness : public AuraScript
{
    PrepareAuraScript(spell_titan_dh_darkness);

    static constexpr int32 AVOID_CHANCE = 20;

    void Avoid(AuraEffect* /*aurEff*/, DamageInfo& dmgInfo, uint32& absorbAmount)
    {
        Unit* caster = GetCaster();
        int32 chance = AVOID_CHANCE + (caster ? TalentValue(caster, TALENT_COVER_OF_DARKNESS) : 0);
        absorbAmount = roll_chance_i(chance) ? dmgInfo.GetDamage() : 0;
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
// of the hit to the 3 other enemies nearest the target; every one with Eternal Reaping) and Cycle of Hatred (Eye
// Beam's cooldown shortened).
class spell_titan_dh_chaos_strike : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_chaos_strike);

    static constexpr float CLEAVE_RADIUS = 8.0f;
    static constexpr std::size_t CLEAVE_TARGETS = 3;

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
            enemies.remove_if([caster, target](Unit* enemy)
            {
                return enemy == target || !caster->IsValidAttackTarget(enemy);
            });
            if (!TalentValue(caster, TALENT_ETERNAL_REAPING) && enemies.size() > CLEAVE_TARGETS)
            {
                enemies.sort(Acore::ObjectDistanceOrderPred(target));
                enemies.resize(CLEAVE_TARGETS);
            }
            for (Unit* enemy : enemies)
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

// Immolation Aura (90119) and its talents: Unbound Chaos (the next Fel Rush is stronger), Infernal Armor (more
// armor, and Fire damage to melee attackers, while it burns) and Fel Flame Fortification (less damage taken).
class spell_titan_dh_immolation_aura : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_immolation_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_UNBOUND_CHAOS, SPELL_INFERNAL_ARMOR, SPELL_FEL_FLAME_FORTIFICATION });
    }

    void Empower()
    {
        Unit* caster = GetCaster();
        CastWithValue(caster, SPELL_UNBOUND_CHAOS, TalentValue(caster, TALENT_UNBOUND_CHAOS), caster);
        if (int32 pct = TalentValue(caster, TALENT_INFERNAL_ARMOR))
        {
            CustomSpellValues values;
            values.AddSpellMod(SPELLVALUE_BASE_POINT0, pct);
            values.AddSpellMod(SPELLVALUE_BASE_POINT1,
                std::max(1, CalculatePct(int32(caster->GetTotalAttackPowerValue(BASE_ATTACK)), pct)));
            caster->CastCustomSpell(SPELL_INFERNAL_ARMOR, values, caster, TRIGGERED_FULL_MASK);
        }
        CastWithValue(caster, SPELL_FEL_FLAME_FORTIFICATION, TalentValue(caster, TALENT_FEL_FLAME_FORTIFICATION),
            caster);
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

namespace
{
    constexpr uint8 MAX_COMBO_POINTS = 5;

    // Vengeance's finishers (Soul Cleave, Spirit Bomb, Soul Barrier) spend all the unit's combo points (Shear awards
    // them): how many there were. Feed the Demon (talent) shortens Demon Spikes' cooldown for each; Painbringer
    // (talent) lowers the damage the unit takes for a while; Shattered Souls (talent) may give them back.
    uint8 SpendComboPoints(Unit* unit)
    {
        uint8 points = unit->GetComboPoints();
        if (!points)
            return 0;

        unit->ClearComboPoints();
        if (Player* player = unit->ToPlayer())
            if (int32 ms = TalentValue(unit, TALENT_FEED_THE_DEMON))
                player->ModifySpellCooldown(SPELL_DEMON_SPIKES, -ms * points);
        CastWithValue(unit, SPELL_PAINBRINGER, TalentValue(unit, TALENT_PAINBRINGER), unit);
        if (Unit* victim = unit->GetVictim())
            if (roll_chance_i(TalentValue(unit, TALENT_SHATTERED_SOULS)))
                unit->AddComboPoints(victim, points);
        return points;
    }

    SpellCastResult NeedComboPoints(Unit const* unit)
    {
        return unit->GetComboPoints() ? SPELL_CAST_OK : SPELL_FAILED_NO_COMBO_POINTS;
    }
}

// Soul Cleave (90125), a finisher: from 1 combo point, spends them all, hitting as hard as the points spent (its
// weapon damage the 5-point value, scaled) and healing for 3% more of maximum health a point (90131). Feast of Souls
// (talent): more healing over 6 sec (3 ticks); Void Reaver (talent): the enemies struck deal less damage.
class spell_titan_dh_soul_cleave : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_soul_cleave);

    static constexpr int32 HEAL_PCT_PER_POINT = 3;
    static constexpr int32 FEAST_TICKS = 3;

    uint8 _points = 0;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SOUL_CLEAVE_HEAL, SPELL_FEAST_OF_SOULS, SPELL_VOID_REAVER });
    }

    SpellCastResult CheckPoints()
    {
        return NeedComboPoints(GetCaster());
    }

    void TakePoints()
    {
        _points = GetCaster()->GetComboPoints();
    }

    void Scale()
    {
        SetHitDamage(GetHitDamage() * _points / MAX_COMBO_POINTS);
    }

    void SpendPoints()
    {
        Unit* caster = GetCaster();
        CastWithValue(caster, SPELL_SOUL_CLEAVE_HEAL, SpendComboPoints(caster) * HEAL_PCT_PER_POINT, caster);
        if (int32 pct = TalentValue(caster, TALENT_FEAST_OF_SOULS))
            CastWithValue(caster, SPELL_FEAST_OF_SOULS, int32(caster->CountPctFromMaxHealth(pct)) / FEAST_TICKS,
                caster);
    }

    void Reave()
    {
        Unit* caster = GetCaster();
        if (Unit* target = GetHitUnit())
            if (int32 pct = TalentValue(caster, TALENT_VOID_REAVER))
                CastWithValue(caster, SPELL_VOID_REAVER, -pct, target);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_titan_dh_soul_cleave::CheckPoints);
        BeforeCast += SpellCastFn(spell_titan_dh_soul_cleave::TakePoints);
        OnHit += SpellHitFn(spell_titan_dh_soul_cleave::Scale);
        AfterCast += SpellCastFn(spell_titan_dh_soul_cleave::SpendPoints);
        AfterHit += SpellHitFn(spell_titan_dh_soul_cleave::Reave);
    }
};

// Spirit Bomb (90135), a finisher: needs a combo point; spends them all and explodes (90136) for 30% of attack power
// each.
class spell_titan_dh_spirit_bomb : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_spirit_bomb);

    static constexpr int32 AP_PCT_PER_POINT = 30;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SPIRIT_BOMB_BLAST });
    }

    SpellCastResult CheckPoints()
    {
        return NeedComboPoints(GetCaster());
    }

    void Explode()
    {
        Unit* caster = GetCaster();
        int32 points = SpendComboPoints(caster);
        int32 damage = CalculatePct(int32(caster->GetTotalAttackPowerValue(BASE_ATTACK)), AP_PCT_PER_POINT * points);
        caster->CastCustomSpell(SPELL_SPIRIT_BOMB_BLAST, SPELLVALUE_BASE_POINT0, std::max(damage, 1), caster, true);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_titan_dh_spirit_bomb::CheckPoints);
        AfterCast += SpellCastFn(spell_titan_dh_spirit_bomb::Explode);
    }
};

// Frailty (90137, from Spirit Bomb): damage the Demon Hunter deals to the marked enemy heals them for (the aura's
// amount)% of it (90139).
class spell_titan_dh_frailty : public AuraScript
{
    PrepareAuraScript(spell_titan_dh_frailty);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_FRAILTY_HEAL });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damage = eventInfo.GetDamageInfo();
        Unit* caster = GetCaster();
        return damage && damage->GetDamage() && caster && caster->IsAlive() && eventInfo.GetActor() == caster;
    }

    void Heal(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* caster = GetCaster();
        CastWithValue(caster, SPELL_FRAILTY_HEAL,
            CalculatePct(int32(eventInfo.GetDamageInfo()->GetDamage()), aurEff->GetAmount()), caster);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_titan_dh_frailty::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_titan_dh_frailty::Heal, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// Soul Barrier (90138): a shield of 20% of maximum health, 5% more for each combo point it spends (all of them).
class spell_titan_dh_soul_barrier : public AuraScript
{
    PrepareAuraScript(spell_titan_dh_soul_barrier);

    static constexpr int32 BASE_PCT = 20;
    static constexpr int32 PCT_PER_POINT = 5;

    void Size(AuraEffect const* /*aurEff*/, int32& amount, bool& canBeRecalculated)
    {
        canBeRecalculated = false;
        Unit* owner = GetUnitOwner();
        int32 points = SpendComboPoints(owner);
        amount = int32(owner->CountPctFromMaxHealth(BASE_PCT + PCT_PER_POINT * points));
        if (Unit* caster = GetCaster())
            AddPct(amount, TalentValue(caster, TALENT_RUINOUS_BULWARK));    // Ruinous Bulwark (talent)
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_titan_dh_soul_barrier::Size, EFFECT_0,
            SPELL_AURA_SCHOOL_ABSORB);
    }
};

// Sigil of Chains (90132): the sigil arms for ARM_TIME (less with Quickened Sigils, talent), then pulls every
// enemy within 8 yards of it to its center (90133, cast at the sigil).
class spell_titan_dh_sigil_of_chains : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_sigil_of_chains);

    static constexpr Milliseconds ARM_TIME = 1500ms;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SIGIL_OF_CHAINS_PULL });
    }

    void Arm()
    {
        WorldLocation const* dest = GetExplTargetDest();
        if (!dest)
            return;

        Unit* caster = GetCaster();
        Position sigil = dest->GetPosition();
        Milliseconds armTime = ARM_TIME - Milliseconds(TalentValue(caster, TALENT_QUICKENED_SIGILS));
        caster->m_Events.AddEventAtOffset([caster, sigil]()
        {
            if (caster->IsAlive())
                caster->CastSpell(sigil.GetPositionX(), sigil.GetPositionY(), sigil.GetPositionZ(),
                    SPELL_SIGIL_OF_CHAINS_PULL, true);
        }, armTime);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_dh_sigil_of_chains::Arm);
    }
};

// Sigil of Chains' pull (90133): reaches 12 yards; without Concentrated Sigils (talent), only the enemies within 8.
class spell_titan_dh_sigil_of_chains_pull : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_sigil_of_chains_pull);

    static constexpr float RADIUS = 8.0f;

    void KeepInReach(std::list<WorldObject*>& targets)
    {
        WorldLocation const* sigil = GetExplTargetDest();
        if (!sigil)
            return;

        float reach = TalentValue(GetCaster(), TALENT_CONCENTRATED_SIGILS) ? 0.0f : RADIUS;
        if (reach)
            targets.remove_if([sigil, reach](WorldObject* target) { return !target->IsWithinDist3d(sigil, reach); });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_titan_dh_sigil_of_chains_pull::KeepInReach,
            EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_titan_dh_sigil_of_chains_pull::KeepInReach,
            EFFECT_1, TARGET_UNIT_DEST_AREA_ENEMY);
    }
};

// Shear (90124) and Shear Fury (talent): more Fury.
class spell_titan_dh_shear : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_shear);

    void Empower()
    {
        Unit* caster = GetCaster();
        if (int32 fury = TalentValue(caster, TALENT_SHEAR_FURY))
            caster->ModifyPower(POWER_RUNIC_POWER, fury * FURY_UNITS);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_dh_shear::Empower);
    }
};

// Demon Spikes (90127) and Razor Spikes (talent): more Physical damage for as long (Calcified Spikes, talent, makes
// Demon Spikes longer).
class spell_titan_dh_demon_spikes : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_demon_spikes);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_RAZOR_SPIKES });
    }

    void Empower()
    {
        Unit* caster = GetCaster();
        CastWithValue(caster, SPELL_RAZOR_SPIKES, TalentValue(caster, TALENT_RAZOR_SPIKES), caster);
        Aura* spikes = caster->GetAura(SPELL_DEMON_SPIKES);
        Aura* razor = caster->GetAura(SPELL_RAZOR_SPIKES);
        if (spikes && razor)
        {
            razor->SetMaxDuration(spikes->GetDuration());
            razor->SetDuration(spikes->GetDuration());
        }
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_titan_dh_demon_spikes::Empower);
    }
};

// Immolation Aura's pulse (90120) and its talents, on each enemy it burns: Charred Flesh (the Demon Hunter's Fiery
// Brand on it lasts longer) and Fallout (a chance, once a pulse, to award a combo point).
class spell_titan_dh_immolation_aura_pulse : public SpellScript
{
    PrepareSpellScript(spell_titan_dh_immolation_aura_pulse);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_FIERY_BRAND });
    }

    void Burn(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        if (int32 ms = TalentValue(caster, TALENT_CHARRED_FLESH))
            if (Aura* brand = target->GetAura(SPELL_FIERY_BRAND, caster->GetGUID()))
            {
                brand->SetMaxDuration(std::max(brand->GetMaxDuration(), brand->GetDuration() + ms));
                brand->SetDuration(brand->GetDuration() + ms);
            }

        if (!_shattered)
            if (int32 chance = TalentValue(caster, TALENT_FALLOUT); chance && roll_chance_i(chance))
            {
                _shattered = true;
                caster->AddComboPoints(caster->GetVictim() ? caster->GetVictim() : target, 1);
            }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_titan_dh_immolation_aura_pulse::Burn, EFFECT_0,
            SPELL_EFFECT_SCHOOL_DAMAGE);
    }

private:
    bool _shattered = false;
};

// Fiery Brand (90128) and Burning Alive (talent): every 2 sec (effect 3) the brand spreads to the nearest enemy
// within 8 yards of the branded one that the Demon Hunter has not branded.
class spell_titan_dh_fiery_brand : public AuraScript
{
    PrepareAuraScript(spell_titan_dh_fiery_brand);

    static constexpr float SPREAD_RADIUS = 8.0f;

    void Spread(AuraEffect const* /*aurEff*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        if (!caster || !TalentValue(caster, TALENT_BURNING_ALIVE))
            return;

        std::list<Unit*> enemies;
        Acore::AnyUnfriendlyUnitInObjectRangeCheck check(target, caster, SPREAD_RADIUS);
        Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(target, enemies, check);
        Cell::VisitObjects(target, searcher, SPREAD_RADIUS);
        enemies.remove_if([caster](Unit* enemy)
        {
            return enemy->HasAura(SPELL_FIERY_BRAND, caster->GetGUID()) || !caster->IsValidAttackTarget(enemy);
        });
        if (enemies.empty())
            return;

        enemies.sort(Acore::ObjectDistanceOrderPred(target));
        caster->AddAura(SPELL_FIERY_BRAND, enemies.front());
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_titan_dh_fiery_brand::Spread, EFFECT_2,
            SPELL_AURA_PERIODIC_DUMMY);
    }
};

// Fiery Demise and Vulnerability (talents): enemies the Demon Hunter branded take more of their Fire damage, and
// enemies with their Frailty more of all their damage. Last Resort (talent): a blow that would kill the Demon
// Hunter leaves them at (the talent's number)% health instead, then not again while Uncontained Fel (3 min) lasts.
class titan_dh_damage_taken : public UnitScript
{
public:
    titan_dh_damage_taken() : UnitScript("titan_dh_damage_taken", true, {
        UNITHOOK_MODIFY_MELEE_DAMAGE,
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
        UNITHOOK_ON_DAMAGE
    }) { }

    void OnDamage(Unit* /*attacker*/, Unit* victim, uint32& damage) override
    {
        if (!victim || damage < victim->GetHealth() || victim->HasAura(SPELL_UNCONTAINED_FEL))
            return;

        int32 pct = TalentValue(victim, TALENT_LAST_RESORT);
        if (!pct)
            return;

        uint32 left = victim->CountPctFromMaxHealth(pct);
        damage = victim->GetHealth() > left ? victim->GetHealth() - left : 0;
        victim->CastSpell(victim, SPELL_UNCONTAINED_FEL, true);
    }

    static int32 BonusPct(Unit* target, Unit* attacker, SpellInfo const* spellInfo)
    {
        if (!target || !attacker)
            return 0;

        int32 pct = 0;
        if (spellInfo && (spellInfo->GetSchoolMask() & SPELL_SCHOOL_MASK_FIRE)
            && target->HasAura(SPELL_FIERY_BRAND, attacker->GetGUID()))
            pct += TalentValue(attacker, TALENT_FIERY_DEMISE);
        if (target->HasAura(SPELL_FRAILTY, attacker->GetGUID()))
            pct += TalentValue(attacker, TALENT_VULNERABILITY);
        return pct;
    }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        if (int32 pct = BonusPct(target, attacker, nullptr))
            AddPct(damage, pct);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (int32 pct = BonusPct(target, attacker, spellInfo))
            AddPct(damage, pct);
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* spellInfo)
        override
    {
        if (int32 pct = BonusPct(target, attacker, spellInfo))
            AddPct(damage, pct);
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
    RegisterSpellScript(spell_titan_dh_soul_cleave);
    RegisterSpellScript(spell_titan_dh_spirit_bomb);
    RegisterSpellScript(spell_titan_dh_frailty);
    RegisterSpellScript(spell_titan_dh_soul_barrier);
    RegisterSpellScript(spell_titan_dh_sigil_of_chains);
    RegisterSpellScript(spell_titan_dh_sigil_of_chains_pull);
    RegisterSpellScript(spell_titan_dh_shear);
    RegisterSpellScript(spell_titan_dh_demon_spikes);
    RegisterSpellScript(spell_titan_dh_immolation_aura_pulse);
    RegisterSpellScript(spell_titan_dh_fiery_brand);
    new titan_dh_damage_taken();
}
