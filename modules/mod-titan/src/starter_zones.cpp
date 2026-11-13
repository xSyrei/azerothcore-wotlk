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


#include "CombatManager.h"
#include "PassiveAI.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include <unordered_map>

// The starting areas (titan_starter_zones.sql): their training dummy and a mount from level 1.
//
// The training dummy (910130, data/sql/db-world/titan_starter_zones.sql): as the core's
// npc_training_dummy (never damaged, so the hits still show; combat with an attacker ends 5 sec after their last
// hit), and at the level of whoever engages it, so a level 1 Hero's swings land as on a creature of their own level.
struct npc_titan_training_dummy : NullCreatureAI
{
    npc_titan_training_dummy(Creature* creature) : NullCreatureAI(creature) { }

    void JustEnteredCombat(Unit* who) override
    {
        _combatTimer[who->GetGUID()] = 5s;
        if (Unit* attacker = who->GetCharmerOrOwnerOrSelf())
            if (attacker->GetLevel() != me->GetLevel())
                me->SetLevel(attacker->GetLevel());
    }

    void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType damageType, SpellSchoolMask) override
    {
        damage = 0;
        if (!attacker || damageType == DOT)
            return;
        _combatTimer[attacker->GetGUID()] = 5s;
        if (Unit* owner = attacker->GetCharmerOrOwner())
            if (me->GetCombatManager().IsInCombatWith(owner))
                _combatTimer[owner->GetGUID()] = 5s;
    }

    void UpdateAI(uint32 diff) override
    {
        for (auto itr = _combatTimer.begin(); itr != _combatTimer.end();)
        {
            itr->second -= Milliseconds(diff);
            if (itr->second <= 0s)
            {
                auto const& pveRefs = me->GetCombatManager().GetPvECombatRefs();
                auto it = pveRefs.find(itr->first);
                if (it != pveRefs.end())
                    it->second->EndCombat();
                itr = _combatTimer.erase(itr);
            }
            else
                ++itr;
        }
    }

private:
    std::unordered_map<ObjectGuid, Milliseconds> _combatTimer;
};

// A mount from level 1: Apprentice Riding and the race's first mount (its trainer's), taught at each login while the
// player lacks them (a prestige or a respec cannot take them for good).
class titan_starter_mount : public PlayerScript
{
public:
    titan_starter_mount() : PlayerScript("titan_starter_mount", { PLAYERHOOK_ON_LOGIN }) { }

    static constexpr uint32 APPRENTICE_RIDING = 33388;

    static uint32 RaceMount(uint8 race)
    {
        switch (race)
        {
            case RACE_HUMAN:         return 458;        // Brown Horse
            case RACE_ORC:           return 580;        // Timber Wolf
            case RACE_DWARF:         return 6899;       // Brown Ram
            case RACE_NIGHTELF:      return 8394;       // Striped Frostsaber
            case RACE_UNDEAD_PLAYER: return 17464;      // Brown Skeletal Horse
            case RACE_TAUREN:        return 18990;      // Brown Kodo
            case RACE_GNOME:         return 10873;      // Red Mechanostrider
            case RACE_TROLL:         return 8395;       // Emerald Raptor
            case RACE_BLOODELF:      return 35020;      // Blue Hawkstrider
            case RACE_DRAENEI:       return 34406;      // Brown Elekk
            default:                 return 458;
        }
    }

    void OnPlayerLogin(Player* player) override
    {
        if (!player->HasSpell(APPRENTICE_RIDING))
            player->learnSpell(APPRENTICE_RIDING);
        uint32 mount = RaceMount(player->getRace());
        if (!player->HasSpell(mount))
            player->learnSpell(mount);
    }
};

void AddSC_starter_zones()
{
    RegisterCreatureAI(npc_titan_training_dummy);
    new titan_starter_mount();
}
