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

#ifndef MOD_TITAN_WILDCARD_H
#define MOD_TITAN_WILDCARD_H

#include "SharedDefines.h"
#include <vector>

class Player;

// Wildcard: how the classless Hero progresses (see wildcard.cpp), and the Classless window that
// shows it (classless.cpp, with the Emberforge_Classless client addon).
namespace Wildcard
{
    constexpr uint8 HERO_CLASS = CLASS_PALADIN;
    constexpr uint8 KIT_SIZE = 4;                   // abilities in the starting kit
    constexpr uint8 LOCK_LEVEL = 9;                 // the kit locks here and the tomes vanish
    constexpr uint8 FIRST_TALENT_LEVEL = 10;        // first per-level ability and talent
    constexpr uint32 TOME_OF_RANDOMIZATION = 910003;
    constexpr uint32 TOME_OF_SELECTION = 910004;

    struct Rank
    {
        uint32 SpellId;
        uint8 Level;
    };

    struct Ability
    {
        std::vector<Rank> Ranks;    // in chain order, first rank first
        uint8 ClassId;              // a class whose trainer teaches it
        uint32 SkillLine;           // its class skill line (spec tab), 0 if none
        bool Offered;               // grantable / pickable; talent chains are only upgraded
        bool Banned;                // removed from the game for Heroes (see wildcard.cpp)
        bool Companion;             // only learned along with another ability (Tame Beast's pet spells)
        std::vector<uint32> Companions; // first ranks learned along with this one
    };

    bool IsHero(Player const* player);
    std::vector<Ability> const& Abilities();
    Ability const* FindAbility(uint32 firstRankSpellId);

    int32 KnownRank(Player const* player, Ability const& ability);
    // Offered, not known, allowed for the player's race and available at the player's level.
    bool CanLearn(Player const* player, Ability const& ability);
    void Learn(Player* player, Ability const& ability, char const* what);
    // Learns a random learnable ability (not excludeFirstRank); the ability, or nullptr if none is left.
    Ability const* GrantRandom(Player* player, char const* what, uint32 excludeFirstRank = 0);
    // Unlearns every rank of an ability, and its companion spells.
    void Unlearn(Player* player, Ability const& ability);
    // Unlearns every offered ability (the kit and later Wildcard abilities; talents stay).
    uint32 ClearAbilities(Player* player);

    uint32 GetPicks(Player* player);
    void SetPicks(Player* player, uint32 picks);
    bool IsKitLocked(Player const* player);
}

namespace Classless
{
    void SendState(Player* player);
    void SendKnown(Player* player);
    void Open(Player* player);
    // Shows the roll reveal card for a granted ability; with canReroll it offers Keep / Reroll.
    void Reveal(Player* player, Wildcard::Ability const& ability, bool canReroll);
}

#endif
