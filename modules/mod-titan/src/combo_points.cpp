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

#include "combo_points.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include <array>

namespace
{
    // The talents' ranks (client-patch/patch_talents.py, patch_monk_talents.py), each rank's % in it.
    constexpr std::array<uint32, 3> TALENT_ANCIENT_TEACHINGS = { 90579, 90580, 90581 };
    constexpr std::array<uint32, 3> TALENT_SHATTERED_SOULS = { 90529, 90530, 90531 };

    template <std::size_t N>
    int32 TalentValue(Unit const* unit, std::array<uint32, N> const& ranks)
    {
        int32 value = 0;
        for (uint32 rank : ranks)
            if (AuraEffect const* effect = unit->GetAuraEffect(rank, EFFECT_0))
                value = effect->GetAmount();
        return value;
    }
}

int32 ComboPoints::RefundChance(Unit const* unit)
{
    return std::max(TalentValue(unit, TALENT_ANCIENT_TEACHINGS), TalentValue(unit, TALENT_SHATTERED_SOULS));
}

void ComboPoints::Refund(Unit* unit, Unit* target, uint8 points)
{
    if (target && target->IsAlive() && points && roll_chance_i(RefundChance(unit)))
        unit->AddComboPoints(target, points);
}

// The game's own finishers (Eviscerate, Ferocious Bite, Slice and Dice...): it takes their combo points after the
// cast, so the refund comes a moment later, on the same target.
class titan_combo_point_refund : public PlayerScript
{
public:
    titan_combo_point_refund() : PlayerScript("titan_combo_point_refund", { PLAYERHOOK_ON_SPELL_CAST }) { }

    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (spell->IsTriggered() || !spell->GetSpellInfo()->NeedsComboPoints())
            return;
        uint8 points = player->GetComboPoints();
        Unit* target = player->GetComboTarget();
        if (!points || !target || !roll_chance_i(ComboPoints::RefundChance(player)))
            return;
        ObjectGuid targetGuid = target->GetGUID();
        player->m_Events.AddEventAtOffset([player, targetGuid, points]()
        {
            if (Unit* again = ObjectAccessor::GetUnit(*player, targetGuid))
                if (again->IsAlive())
                    player->AddComboPoints(again, points);
        }, 1ms);
    }
};

void AddSC_combo_points()
{
    new titan_combo_point_refund();
}
