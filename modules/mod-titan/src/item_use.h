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

#ifndef MOD_TITAN_ITEM_USE_H
#define MOD_TITAN_ITEM_USE_H

#include "Item.h"
#include "Player.h"
#include "Spell.h"
#include "SpellMgr.h"

// Our usable items carry an instant, empty spell (90608) so the 3.3.5 client lets them be used; their ItemScript's
// OnUse does the work and returns true, so the spell is never cast. The client still waits for the cast's end: it
// is told here, silently (WorldSession::HandleUseItemOpcode: "it must send appropriate data to client").
inline void EndItemUse(Player* player, Item* item)
{
    if (!player || !item)
        return;
    uint32 spellId = item->GetTemplate()->Spells[0].SpellId;
    if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId))
        Spell::SendCastResult(player, spellInfo, 1, SPELL_FAILED_DONT_REPORT);
}

#endif
