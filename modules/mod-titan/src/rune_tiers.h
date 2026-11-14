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

#ifndef MOD_TITAN_RUNE_TIERS_H
#define MOD_TITAN_RUNE_TIERS_H

#include "Define.h"
#include <vector>

class Player;
struct SpellModifier;

// Runes of Enhancement (rune_tiers.cpp): Epic, Rare and Uncommon runes beside the Legendary Rune.
namespace RuneTiers
{
    // A choice of 3 runes for the Hero to take (the level offers' own, a GM's, an item's). false: none to offer.
    bool GrantOffer(Player* player, uint8 level);
    // The Hero holds the rune, and it is not dormant.
    bool Has(Player* player, uint32 rune);
    // A spell modifier on one ability (its first rank: every rank) and nothing else; op a SpellModOp, type a
    // SpellModType. Kept until RemoveAbilityMod (which deletes it) or the Hero logs out.
    SpellModifier* AddAbilityMod(Player* player, uint32 ability, uint8 op, uint8 type, int32 value);
    void RemoveAbilityMod(Player* player, SpellModifier* mod);

    // An Epic's or Rare's lasting effect while it is held and awake (rune_effects.cpp's tables): a spell modifier on
    // a spell (its first rank), or an aura on the Hero.
    struct RuneMod
    {
        uint32 Rune;
        uint32 Spell;
        uint8 Op;
        uint8 Type;
        int32 Value;
    };
    struct RuneAura
    {
        uint32 Rune;
        uint32 Aura;
    };
}

namespace RuneEffects
{
    std::vector<RuneTiers::RuneMod> const& Mods();
    std::vector<RuneTiers::RuneAura> const& Auras();
    // A Hero's runes went away (logout): the effects' own state with them.
    void Forget(Player* player);
}

#endif
