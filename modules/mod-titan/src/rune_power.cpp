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

#include "rune_power.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include <unordered_set>

// The Legendary Runes' own output, raised across the board (they were too weak to build around): their damage
// RUNE_DAMAGE_POWER times, their healing RUNE_HEALING_POWER times, their shields RUNE_SHIELD_POWER times. Theirs:
// the spells of client-patch/patch_runes.py (rune_power.h, tools/runes/rune_power.py), from a player or what is
// theirs (an echo, an orb, a beast). What the runes add to the Hero's own abilities (a share more on a spell of
// theirs) is not raised: only what the runes deal, heal and shield by themselves.
namespace
{
    constexpr float RUNE_DAMAGE_POWER = 3.5f, RUNE_HEALING_POWER = 3.0f, RUNE_SHIELD_POWER = 3.0f;

    std::unordered_set<uint32> const DAMAGE(std::begin(RunePower::DAMAGE), std::end(RunePower::DAMAGE));
    std::unordered_set<uint32> const HEALING(std::begin(RunePower::HEALING), std::end(RunePower::HEALING));
    std::unordered_set<uint32> const SHIELDS(std::begin(RunePower::SHIELDS), std::end(RunePower::SHIELDS));

    bool PlayersOwn(Unit* unit)
    {
        return unit && unit->GetCharmerOrOwnerPlayerOrPlayerItself();
    }

    template <typename T>
    void Raise(T& amount, float power)
    {
        amount = T(std::lround(double(amount) * power));
    }
}

class titan_rune_power : public UnitScript
{
public:
    titan_rune_power() : UnitScript("titan_rune_power", true, { UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK, UNITHOOK_MODIFY_HEAL_RECEIVED, UNITHOOK_ON_AURA_APPLY }) { }

    void ModifySpellDamageTaken(Unit* /*target*/, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (damage > 0 && spellInfo && DAMAGE.count(spellInfo->Id) && PlayersOwn(attacker))
            Raise(damage, RUNE_DAMAGE_POWER);
    }

    void ModifyPeriodicDamageAurasTick(Unit* /*target*/, Unit* attacker, uint32& damage,
        SpellInfo const* spellInfo) override
    {
        if (damage && spellInfo && DAMAGE.count(spellInfo->Id) && PlayersOwn(attacker))
            Raise(damage, RUNE_DAMAGE_POWER);
    }

    void ModifyHealReceived(Unit* /*target*/, Unit* healer, uint32& heal, SpellInfo const* spellInfo) override
    {
        if (heal && spellInfo && HEALING.count(spellInfo->Id) && PlayersOwn(healer))
            Raise(heal, RUNE_HEALING_POWER);
    }

    // A shield of the runes', as it is made (or made again: the core recalculates its amount before this).
    void OnAuraApply(Unit* /*unit*/, Aura* aura) override
    {
        if (!aura || !SHIELDS.count(aura->GetId()) || !PlayersOwn(aura->GetCaster()))
            return;
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            if (AuraEffect* effect = aura->GetEffect(i); effect && effect->GetAuraType() == SPELL_AURA_SCHOOL_ABSORB)
                effect->SetAmount(int32(std::lround(effect->GetAmount() * RUNE_SHIELD_POWER)));
    }
};

void AddSC_rune_power()
{
    new titan_rune_power();
}
