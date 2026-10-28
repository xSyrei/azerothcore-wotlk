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

void AddSC_demon_hunter()
{
    RegisterSpellScript(spell_titan_dh_dual_weapons);
    RegisterSpellScript(spell_titan_dh_blade_dance);
}
