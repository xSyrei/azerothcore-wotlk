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

// The Fireforged bonus effects (The Firepit Ignites; tools/scaling/firepit_effects.py, fireforged_effects.h): an Epic
// Fireforged piece's one effect, an equip spell its enchantment puts on the Hero.
// - Its values scale with the piece's own item level: L = clamp(item level / 90, 0.25, 2.0), P = 225 L spell power,
//   A = 335 L attack power, R = 160 L rating (the equip aura's amounts set here, from the aura's item).
// - Percentage effects (direct heals, heals over time, melee abilities, auto-attacks, direct spells, ranged
//   abilities, spells over time) raise those hits here.
// - Procs: 15% on their event, 30 sec apart for each Hero and effect, at most one roll a cast (a roll per effect
//   every 0.25 sec); their buff at the same scale. Item effects never set each other off (their own spells are left
//   out).

#include "fireforged_effects.h"
#include "Item.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "GameTime.h"
#include <map>

using namespace FireforgedEffects;

namespace
{
    constexpr float PROC_CHANCE = 15.0f, LOW_HEALTH = 40.0f;
    constexpr uint32 PROC_COOLDOWN_MS = 30000, ROLL_EVERY_MS = 250;
    constexpr std::size_t EFFECT_COUNT = std::size(EFFECTS);

    // The effect an equip spell is, else -1.
    int32 EffectOf(uint32 spellId)
    {
        if (spellId < EQUIP_SPELL_FIRST || spellId >= EQUIP_SPELL_FIRST + EFFECT_COUNT)
            return -1;
        return int32(spellId - EQUIP_SPELL_FIRST);
    }

    float Scale(uint32 itemLevel)
    {
        return std::clamp(float(itemLevel) / 90.0f, 0.25f, 2.0f);
    }

    int32 Amount(Value const& value, uint32 itemLevel)
    {
        float L = Scale(itemLevel);
        switch (value.Kind)
        {
            case 'P': return int32(std::lround(std::round(225.0f * L) * value.Multiplier));
            case 'A': return int32(std::lround(std::round(335.0f * L) * value.Multiplier));
            case 'R': return int32(std::lround(std::round(160.0f * L) * value.Multiplier));
            case 'L': return int32(std::lround(L * value.Multiplier));
            default: return int32(std::lround(value.Multiplier));
        }
    }

    // The item level of the piece an equip aura comes from (90 if it cannot be told).
    uint32 ItemLevelOf(Player* player, Aura const* aura)
    {
        if (Item* item = player->GetItemByGuid(aura->GetCastItemGUID()))
            return item->GetTemplate()->ItemLevel;
        return 90;
    }

    // The Hero's Fireforged effects now: each equip aura's effect and its piece's item level.
    struct Worn
    {
        int32 effect;
        uint32 itemLevel;
        ObjectGuid item;
    };

    std::vector<Worn> WornBy(Player* player)
    {
        std::vector<Worn> worn;
        for (auto const& [id, application] : player->GetAppliedAuras())
            if (int32 effect = EffectOf(id); effect >= 0)
            {
                Aura const* aura = application->GetBase();
                worn.push_back({ effect, ItemLevelOf(player, aura), aura->GetCastItemGUID() });
            }
        return worn;
    }

    // A percentage effect worn (the highest of identical ones, which never stack): its factor.
    float Percent(Player* player, uint8 special)
    {
        float best = 0.0f;
        for (Worn const& w : WornBy(player))
            if (EFFECTS[w.effect].Special == special)
                best = std::max(best, EFFECTS[w.effect].SpecialValue);
        return 1.0f + best / 100.0f;
    }

    std::map<std::pair<ObjectGuid, int32>, uint32> procAt, rollAt;  // (Hero, effect) -> game ms

    uint32 Now()
    {
        return uint32(GameTime::GetGameTimeMS().count());
    }

    // A proc's buff (or its special) on its Hero (or the healed one).
    void Fire(Player* player, Worn const& w, Unit* other)
    {
        Effect const& effect = EFFECTS[w.effect];
        switch (effect.Special)
        {
            case SPECIAL_MANA:
                player->ModifyPower(POWER_MANA, int32(player->GetMaxPower(POWER_MANA) * effect.SpecialValue / 100.0f));
                return;
            case SPECIAL_STRIKE:
                if (other && other->IsAlive())
                {
                    int32 hit = std::max<int32>(1, Amount({ 'A', effect.SpecialValue }, w.itemLevel));
                    player->CastCustomSpell(other, STRIKE_SPELL, &hit, nullptr, nullptr, true);
                }
                return;
            default:
                break;
        }
        if (!effect.Buff)
            return;
        int32 first = Amount(effect.Proc[0], w.itemLevel), second = Amount(effect.Proc[1], w.itemLevel);
        Unit* onto = player;
        if (effect.Special == SPECIAL_ARMOR)
        {
            Item* item = player->GetItemByGuid(w.item);
            first = item ? int32(item->GetTemplate()->Armor * effect.SpecialValue / 100.0f) : 0;
        }
        else if (effect.Special == SPECIAL_ABSORB_SELF || effect.Special == SPECIAL_ABSORB_TARGET)
        {
            first = int32(player->GetMaxHealth() * effect.SpecialValue / 100.0f);
            if (effect.Special == SPECIAL_ABSORB_TARGET && other)
                onto = other;
        }
        if (first <= 0)
            return;
        onto->RemoveAurasDueToSpell(effect.Buff, player->GetGUID());      // a fresh proc refreshes, never stacks
        player->CastCustomSpell(onto, effect.Buff, &first, second > 0 ? &second : nullptr, nullptr, true);
    }

    // An event of the Hero's: each worn proc of that trigger rolls (15%, 30 sec apart, a roll every 0.25 sec).
    void Proc(Player* player, uint8 trigger, Unit* other)
    {
        uint32 now = Now();
        for (Worn const& w : WornBy(player))
        {
            Effect const& effect = EFFECTS[w.effect];
            if (effect.Trigger != trigger && !(effect.Trigger == TRIGGER_DIRECT
                && (trigger == TRIGGER_MELEE || trigger == TRIGGER_SPELL || trigger == TRIGGER_RANGED
                    || trigger == TRIGGER_HEAL)))
                continue;
            auto key = std::make_pair(player->GetGUID(), w.effect);
            uint32& rolled = rollAt[key];
            if (rolled && now - rolled < ROLL_EVERY_MS)
                continue;
            rolled = now;
            uint32& procced = procAt[key];
            if ((procced && now - procced < PROC_COOLDOWN_MS) || !roll_chance_f(PROC_CHANCE))
                continue;
            procced = now;
            Fire(player, w, other);
        }
    }

    bool IsItemEffect(SpellInfo const* spellInfo)
    {
        return spellInfo && spellInfo->Id >= EQUIP_SPELL_FIRST && spellInfo->Id <= STRIKE_SPELL;
    }
}

// The equip spells (every effect's): their amounts from their piece's item level.
class spell_titan_fireforged_equip : public AuraScript
{
    PrepareAuraScript(spell_titan_fireforged_equip);

    void Calc(AuraEffect const* aurEff, int32& amount, bool& /*canBeRecalculated*/)
    {
        int32 effect = EffectOf(GetId());
        Player* player = GetUnitOwner() ? GetUnitOwner()->ToPlayer() : nullptr;
        if (effect < 0 || !player || aurEff->GetEffIndex() > EFFECT_1)
            return;
        Value const& value = EFFECTS[effect].Equip[aurEff->GetEffIndex()];
        if (value.Multiplier > 0.0f)
            amount = Amount(value, ItemLevelOf(player, GetAura()));
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_titan_fireforged_equip::Calc, EFFECT_ALL, SPELL_AURA_ANY);
    }
};

class titan_fireforged_unit : public UnitScript
{
public:
    titan_fireforged_unit() : UnitScript("titan_fireforged_unit", true, { UNITHOOK_ON_DAMAGE,
        UNITHOOK_MODIFY_MELEE_DAMAGE, UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN, UNITHOOK_MODIFY_HEAL_RECEIVED,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK }) { }

    // A damaging hit taken: the tank procs (below 40% too).
    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        Player* player = victim ? victim->ToPlayer() : nullptr;
        if (!player || !attacker || attacker == victim || !damage)
            return;
        Proc(player, TRIGGER_TAKEN, attacker);
        if (player->HealthBelowPctDamaged(int32(LOW_HEALTH), damage))
            Proc(player, TRIGGER_TAKEN_LOW, attacker);
    }

    // An auto-attack: Finely Honed, the melee procs; taken with a shield on, the block procs.
    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        if (!damage)
            return;
        if (Player* player = attacker ? attacker->ToPlayer() : nullptr)
        {
            damage = uint32(float(damage) * Percent(player, SPECIAL_AUTO_ATTACK));
            Proc(player, TRIGGER_MELEE, target);
        }
        if (Player* player = target ? target->ToPlayer() : nullptr; player && player->GetShield())
            Proc(player, TRIGGER_BLOCKED, attacker);
    }

    // A direct hit: the percentage effects of its kind, and the procs.
    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        Player* player = attacker ? attacker->ToPlayer() : nullptr;
        if (!player || !spellInfo || damage <= 0 || IsItemEffect(spellInfo) || target == attacker)
            return;
        switch (spellInfo->DmgClass)
        {
            case SPELL_DAMAGE_CLASS_MELEE:
                damage = int32(float(damage) * Percent(player, SPECIAL_MELEE_ABILITY));
                Proc(player, TRIGGER_MELEE, target);
                break;
            case SPELL_DAMAGE_CLASS_RANGED:
                damage = int32(float(damage) * Percent(player, SPECIAL_RANGED_ABILITY));
                Proc(player, TRIGGER_RANGED, target);
                break;
            case SPELL_DAMAGE_CLASS_MAGIC:
                damage = int32(float(damage) * Percent(player, SPECIAL_DIRECT_SPELL));
                Proc(player, TRIGGER_SPELL, target);
                break;
            default:
                break;
        }
    }

    // A heal: Gentle Remedy, the heal procs (an effective heal only; below 40% before it; on oneself).
    void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* spellInfo) override
    {
        Player* player = healer ? healer->ToPlayer() : nullptr;
        if (!player || !target || !heal || IsItemEffect(spellInfo))
            return;
        heal = uint32(float(heal) * Percent(player, SPECIAL_DIRECT_HEAL));
        if (target->IsFullHealth())
            return;
        bool low = target->HealthBelowPct(int32(LOW_HEALTH));
        Proc(player, TRIGGER_HEAL, target);
        if (low)
            Proc(player, TRIGGER_HEAL_LOW, target);
        if (target == player)
            Proc(player, TRIGGER_SELF_HEAL, target);
    }

    // A tick: Lasting Affliction (damage) or Renewing Touch (healing).
    void ModifyPeriodicDamageAurasTick(Unit* /*target*/, Unit* attacker, uint32& damage, SpellInfo const* spellInfo)
        override
    {
        Player* player = attacker ? attacker->ToPlayer() : nullptr;
        if (!player || !damage || !spellInfo || IsItemEffect(spellInfo))
            return;
        bool heal = spellInfo->HasAura(SPELL_AURA_PERIODIC_HEAL);
        damage = uint32(float(damage) * Percent(player, heal ? SPECIAL_PERIODIC_HEAL : SPECIAL_PERIODIC_SPELL));
    }
};

void AddSC_fireforged()
{
    RegisterSpellScript(spell_titan_fireforged_equip);
    new titan_fireforged_unit();
}
