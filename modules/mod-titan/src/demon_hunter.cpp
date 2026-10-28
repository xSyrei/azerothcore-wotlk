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
// attribute for it would also make the ability strike with the off-hand weapon.
namespace
{
    constexpr uint32 SPELL_DUAL_WIELD = 674;

    // Every Demon Hunter ability that needs both weapons (spell_script_names binds the check to them).
    constexpr uint32 DUAL_WEAPON_SPELLS[] = {
        90100,      // Demon's Bite
        90101,      // Chaos Strike
        90102,      // Blade Dance
        90103,      // Fel Rush
        90105,      // Eye Beam
    };

    bool IsDualWeaponSpell(uint32 spellId)
    {
        for (uint32 id : DUAL_WEAPON_SPELLS)
            if (id == spellId)
                return true;
        return false;
    }
}

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

// Wielding two weapons needs Dual Wield, which a Hero does not have by default: learning a
// Demon Hunter ability teaches it (and logging in teaches it to Heroes who learned one before).
class titan_dh_dual_wield : public PlayerScript
{
public:
    titan_dh_dual_wield() : PlayerScript("titan_dh_dual_wield", {
        PLAYERHOOK_ON_LEARN_SPELL,
        PLAYERHOOK_ON_LOGIN
    }) { }

    void OnPlayerLearnSpell(Player* player, uint32 spellId) override
    {
        if (IsDualWeaponSpell(spellId))
            TeachDualWield(player);
    }

    void OnPlayerLogin(Player* player) override
    {
        for (uint32 spellId : DUAL_WEAPON_SPELLS)
        {
            if (player->HasSpell(spellId))
            {
                TeachDualWield(player);
                return;
            }
        }
    }

private:
    static void TeachDualWield(Player* player)
    {
        if (!player->HasSpell(SPELL_DUAL_WIELD))
            player->learnSpell(SPELL_DUAL_WIELD);
    }
};

void AddSC_demon_hunter()
{
    RegisterSpellScript(spell_titan_dh_dual_weapons);
    new titan_dh_dual_wield();
}
