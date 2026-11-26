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

#ifndef MOD_TITAN_RUNES_H
#define MOD_TITAN_RUNES_H

#include "Define.h"
#include <vector>

class Player;
class Spell;
class SpellInfo;
class Unit;

// Legendary Runes (runes.cpp): two runes per specialization (the first from level 5, the second from level 30),
// kept until a prestige or .unbindrune; each one's three modifiers change freely out of combat.
namespace LegendaryRunes
{
    // The active specialization changed: its rune's auras on, the last one's (stacks, summons) gone.
    void OnSpecChanged(Player* player);
    // A prestige: every specialization's rune is cleared, to be chosen again from level 5.
    void OnPrestige(Player* player);
    // The active specialization's runes let go of (which: 1 the first, 2 the second, 0 both; modifiers too, their
    // auras and actions gone), the panel opened to bind again. false: none was bound.
    bool Unbind(Player* player, uint8 which);
    // .runedebug: the newest runes' steps (Icicles, the Ember, the phoenix) told to the Hero in chat, or no more.
    // true: now on.
    bool ToggleDebug(Player* player);
    // Fel Rift (Fel Commandment): the enemy Fel Rush carries the Hero to, for 3 sec after a volley; else nullptr.
    Unit* FelRiftTarget(Player* player);
    // A rune's taught ability in place of one the Hero knows (bases: first ranks, any one of them): taught while on
    // and a base is known (any rank), the base's buttons becoming it; else forgotten, its buttons the base's highest
    // known rank again.
    void ReplaceAbility(Player* player, uint32 taught, std::vector<uint32> const& bases, bool on);
    // Every first rank a rune's taught ability stands in for (learning or forgetting one teaches again).
    bool IsReplacedBase(uint32 firstRank);
    // A rune's replacement ability counts as the one it replaces on its main target: while the hit spellId lands
    // there, it is base (Thunderbomb Stormstrike's main-hand hit, Magma Breaker Lava Lash, Ball Lightning Lightning
    // Bolt, Storm Lance Earth Shock), so whatever reacts to the base (other runes, the runes' wildcard effects)
    // reacts to it. CountHitAs(0, 0) ends it.
    void CountHitAs(uint32 spellId, uint32 base);
    // The first rank a spell's hit stands for: the base while it counts as one, else its own first rank.
    uint32 CountsAs(SpellInfo const* spellInfo);
    // An original cast (not triggered), or a replacement's hit counting as its base.
    bool IsOriginal(Spell const* spell);
}

#endif
