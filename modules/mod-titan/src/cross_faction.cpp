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

#include "DBCStores.h"
#include "Group.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"

// Mixed-faction groups in dungeons and raids (the dungeon finder puts both factions together; groups may cross
// factions, AllowTwoSide.Interaction.Group). The server already counts groupmates as friends (Unit::GetReactionTo),
// but the client judges by faction alone: an Alliance Hero could not heal a Horde groupmate (nor buff them). Inside
// an instance (battlegrounds and arenas aside), a member of a group with both factions wears the faction of the
// group leader's side, so the client sees friends; outside, or alone, or back in a one-faction group, their own
// race's faction again (in the open world the other side's faction would set their own city guards on them).
namespace
{
    constexpr uint8 ALLIANCE_RACE = RACE_HUMAN, HORDE_RACE = RACE_ORC;     // their side's faction template

    bool Mixed(Group* group)
    {
        bool alliance = false, horde = false;
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
            if (Player* member = itr->GetSource())
                (member->GetTeamId(true) == TEAM_ALLIANCE ? alliance : horde) = true;
        return alliance && horde;
    }

    // The side the group wears: its leader's (else its first member online's).
    TeamId GroupSide(Group* group)
    {
        if (Player* leader = ObjectAccessor::FindConnectedPlayer(group->GetLeaderGUID()))
            return leader->GetTeamId(true);
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
            if (Player* member = itr->GetSource())
                return member->GetTeamId(true);
        return TEAM_NEUTRAL;
    }

    uint32 SideFaction(TeamId side)
    {
        ChrRacesEntry const* race = sChrRacesStore.LookupEntry(side == TEAM_ALLIANCE ? ALLIANCE_RACE : HORDE_RACE);
        return race ? race->FactionID : 0;
    }

    void Refresh(Player* player)
    {
        if (!player || !player->IsInWorld())
            return;
        Map* map = player->GetMap();
        Group* group = player->GetGroup();
        bool instance = map && map->Instanceable() && !map->IsBattlegroundOrArena();
        TeamId side = group && instance && Mixed(group) ? GroupSide(group) : TEAM_NEUTRAL;
        if (side != TEAM_NEUTRAL && side != player->GetTeamId(true))
        {
            if (uint32 faction = SideFaction(side); faction && player->GetFaction() != faction)
                player->SetFaction(faction);
            return;
        }
        ChrRacesEntry const* own = sChrRacesStore.LookupEntry(player->getRace(true));
        if (own && player->GetFaction() != own->FactionID)
            player->SetFactionForRace(player->getRace(true));
    }

    // Every member online, a moment after a change (the group's own lists updated by then).
    void RefreshGroup(Group* group)
    {
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
            if (Player* member = itr->GetSource())
            {
                ObjectGuid guid = member->GetGUID();
                member->m_Events.AddEventAtOffset([guid]()
                {
                    Refresh(ObjectAccessor::FindConnectedPlayer(guid));
                }, 100ms);
            }
    }
}

class titan_cross_faction_group : public GroupScript
{
public:
    titan_cross_faction_group() : GroupScript("titan_cross_faction_group", {
        GROUPHOOK_ON_ADD_MEMBER,
        GROUPHOOK_ON_REMOVE_MEMBER,
        GROUPHOOK_ON_CHANGE_LEADER,
        GROUPHOOK_ON_DISBAND
    }) { }

    void OnAddMember(Group* group, ObjectGuid /*guid*/) override
    {
        RefreshGroup(group);
    }

    void OnRemoveMember(Group* group, ObjectGuid guid, RemoveMethod /*method*/, ObjectGuid /*kicker*/,
        char const* /*reason*/) override
    {
        if (Player* gone = ObjectAccessor::FindConnectedPlayer(guid))
            gone->m_Events.AddEventAtOffset([guid]()
            {
                Refresh(ObjectAccessor::FindConnectedPlayer(guid));
            }, 100ms);
        RefreshGroup(group);
    }

    void OnChangeLeader(Group* group, ObjectGuid /*newLeaderGuid*/, ObjectGuid /*oldLeaderGuid*/) override
    {
        RefreshGroup(group);
    }

    void OnDisband(Group* group) override
    {
        RefreshGroup(group);
    }
};

class titan_cross_faction_player : public PlayerScript
{
public:
    titan_cross_faction_player() : PlayerScript("titan_cross_faction_player", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_MAP_CHANGED
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        Refresh(player);
    }

    // Into or out of an instance: the group's side there, the Hero's own outside (and for the others: a member
    // coming in may make the group mixed there).
    void OnPlayerMapChanged(Player* player) override
    {
        Refresh(player);
        if (Group* group = player->GetGroup())
            RefreshGroup(group);
    }
};

void AddSC_cross_faction()
{
    new titan_cross_faction_group();
    new titan_cross_faction_player();
}
