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
struct TalentEntry;

// Wildcard: how the classless Hero progresses (see wildcard.cpp), and the Classless window that
// shows it (classless.cpp, with the Emberforge_Classless client addon).
namespace Wildcard
{
    constexpr uint8 HERO_CLASS = CLASS_PALADIN;
    constexpr uint8 KIT_SIZE = 4;                   // abilities in the starting kit
    constexpr uint8 LOCK_LEVEL = 9;                 // the kit locks here and the tomes vanish
    constexpr uint8 FIRST_ROLL_LEVEL = 8;           // level-up rolls alternate from here: a talent rank on even
                                                    // levels (8, 10...), an ability on odd ones (9, 11...)
    constexpr uint8 CAPSTONE_LEVEL = 60;            // its roll is a capstone ability instead of a talent rank, and
                                                    // capstones are rolled there only
    constexpr uint32 TOME_OF_RANDOMIZATION = 910003;
    constexpr uint32 TOME_OF_SELECTION = 910004;
    // Classes the game lacks, with abilities of Emberforge's own (retail class ids). Their spec tabs
    // are skill lines of our own too, past the game's.
    constexpr uint8 CLASS_DEMON_HUNTER = 12;
    constexpr uint32 SKILL_HAVOC = 2000;
    constexpr uint32 SKILL_VENGEANCE = 2001;
    constexpr uint8 CLASS_MONK = 10;
    constexpr uint32 SKILL_BREWMASTER = 2002;
    constexpr uint32 SKILL_MISTWEAVER = 2003;
    constexpr uint32 SKILL_WINDWALKER = 2004;
    // Talent trees Heroes draw from: every playable class's, the Demon Hunter's and the Monk's (TalentTab rows of our
    // own, client-patch/patch_talents.py and patch_monk_talents.py).
    constexpr uint32 CLASSMASK_DEMON_HUNTER = 1 << (CLASS_DEMON_HUNTER - 1);
    constexpr uint32 CLASSMASK_MONK = 1 << (CLASS_MONK - 1);
    constexpr uint32 HERO_TALENT_CLASSMASK = CLASSMASK_ALL_PLAYABLE | CLASSMASK_DEMON_HUNTER | CLASSMASK_MONK;

    // The role a Hero plays, chosen in the Classless window: a buff that changes the threat they cause
    // (client-patch/patch_player.py). Kept in the character's settings.
    enum Role : uint8
    {
        ROLE_NONE,
        ROLE_MELEE,     // threat -50%
        ROLE_RANGED,    // threat -50%
        ROLE_HEALER,    // threat unchanged
        ROLE_TANK,      // threat +200%
        MAX_ROLES
    };

    constexpr uint8 RoleMask(Role role) { return uint8(1 << role); }
    // An ability or talent tagged with roles (RoleMask bits, 0: neutral) suits the player's role: Melee takes melee
    // and physical ranged, Ranged magic damage, Healer heals and magic damage, Tank tanking and melee. No role: all.
    bool SuitsRole(Player* player, uint8 roles);

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
        // What it needs first, as groups of first ranks: one ability of every group must be known (the
        // form or stance it is cast in, stealth, a pet). Empty: nothing.
        std::vector<std::vector<uint32>> Requires;
        uint8 Roles = 0;            // RoleMask() bits of the roles it serves; 0: every role (buffs, utility)
        uint32 TalentId = 0;        // a talent's ability (Mortal Strike...): learned as an ability, never as a talent
        bool Capstone = false;      // rolled at CAPSTONE_LEVEL only (Bladestorm, Divine Storm...)
        uint8 CapstoneRoles = 0;    // RoleMask() bits of the roles whose capstone roll can give it
    };

    bool IsHero(Player const* player);
    std::vector<Ability> const& Abilities();
    // Name of one of our own skill lines (SKILL_HAVOC...), or nullptr.
    char const* CustomSkillLineName(uint32 skillLine);
    Ability const* FindAbility(uint32 firstRankSpellId);

    int32 KnownRank(Player const* player, Ability const& ability);
    // Every group of ability.Requires has a known ability.
    bool MeetsRequirements(Player const* player, Ability const& ability);
    // Offered, not known, allowed for the player's race, available at the player's level and its
    // requirements met.
    bool CanLearn(Player const* player, Ability const& ability);
    void Learn(Player* player, Ability const& ability, char const* what);
    // Learns a random learnable ability (not a capstone); the ability, or nullptr if none is left.
    Ability const* GrantRandom(Player* player, char const* what);
    // Its faction's version for the player's race (Bloodlust or Heroism, Teleport: Shattrath's two): one each.
    bool FitsPlayer(Player const* player, Ability const& ability);
    // The talent's ability, when it is learned as one (TalentId), or nullptr.
    Ability const* TalentAbility(uint32 talentId);
    // Unlearns every rank of an ability, and its companion spells.
    void Unlearn(Player* player, Ability const& ability);
    // Unlearns every offered ability (the kit and later Wildcard abilities; talents stay).
    uint32 ClearAbilities(Player* player);
    // Prestige: forgets every Wildcard ability and talent and the count of what was granted, so the next level
    // change rolls a new kit, then every roll again (prestige.cpp).
    void Reset(Player* player);

    // Learns rank (from 0) of a talent the way Player::LearnTalent does once its checks pass; with reveal the
    // Classless window spins for it.
    // level: the level the roll is for (0: the player's).
    bool LearnTalentRank(Player* player, TalentEntry const* talentInfo, uint8 rank, bool reveal, uint8 level = 0);

    // Specializations (specs.cpp): learning without the chat lines, the catch-up of what the level is owed, a talent
    // roll that may not give one talent, a capstone roll, and one talent's ranks taken away.
    void SetQuiet(bool on);
    void CatchUpNow(Player* player);
    void LearnQuiet(Player* player, Ability const& ability);
    bool GrantTalentExcept(Player* player, uint32 talentId, bool reveal);
    Ability const* GrantNewCapstone(Player* player);
    uint8 TalentRank(Player const* player, TalentEntry const* talentInfo);
    uint8 RemoveTalent(Player* player, TalentEntry const* talentInfo);

    uint32 GetPicks(Player* player);
    void SetPicks(Player* player, uint32 picks);
    bool IsKitLocked(Player const* player);

    Role GetRole(Player* player);
    // Sets the role and its buff; false (and nothing changed) in combat.
    bool SetRole(Player* player, Role role);
    // Puts the role's buff on, and takes the others off (login, resurrection).
    void ApplyRole(Player* player);
}

namespace Classless
{
    void SendState(Player* player);
    void SendKnown(Player* player);
    void Open(Player* player);
    // Plays the Wildcard roll for a granted ability.
    // level: the level the roll is for (several rolls from a jump of levels each say theirs).
    void Reveal(Player* player, Wildcard::Ability const& ability, uint8 level);
    // Shows the spin for a talent rank (rank counts from 1).
    void RevealTalent(Player* player, uint32 talentId, uint32 spellId, uint8 rank, uint8 level);
    // GMs only: spell ids on the tooltips (.spellids), kept per character.
    bool ShowsSpellIds(Player* player);
    void SetShowSpellIds(Player* player, bool show);
}

#endif
