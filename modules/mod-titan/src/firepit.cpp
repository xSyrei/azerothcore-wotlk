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

#include "firepit.h"
#include "Chat.h"
#include "CommandScript.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "GameObjectScript.h"
#include "GameTime.h"
#include "Group.h"
#include "InstanceSaveMgr.h"
#include "InstanceScript.h"
#include "Item.h"
#include "Mail.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "TemporarySummon.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// The Firepit: an endless wave arena in a private instance of Blackrock Depths' Ring of Law, solo or for a group of
// up to 10 (src/firepit.cpp, docs in README.md).
// - The Firepit Keeper (beside Silas Darkmoon in Stormwind and Orgrimmar) sends the Hero, or the leader's group, into
//   a fresh instance of map 230 (InstanceOverride: the core sends them there, and lets 10 in). Its instance script is
//   the Firepit's own (the Depths' never runs): the native creatures are sent away as they load, and it runs the
//   waves.
// - Waves every 60 sec, or 5 sec after the last enemy dies. Wave n has 3 + n/2 enemies (20% more for each player
//   past the first), from the Depths' roster (a new kind every wave or two); every 5th wave brings a boss (two from
//   wave 60, three from 120). Survivors stay: they pile up. Past 150 alive, new ones are not spawned; every enemy
//   left grows 3% instead.
// - Creatures are at the group's level (dungeon_scaling.cpp), then their health and damage are 35% of a dungeon's at
//   wave 1, 10% more each wave (compounding), health 40% more for each player past the first.
// - The dead stay down (no release: they stay where they fell); a battle resurrection brings them back. The last
//   death ends the run: everyone is brought back at full, each Hero who survived a 15th wave gets a chest of their
//   own (a piece of gear at their level a 15 waves; epic from wave 45), the results go to the Emberforge_Firepit
//   addon, and the best wave is recorded for its group size: solo, duo, trio, group (titan_firepit_records).
namespace
{
    constexpr uint32 MAP_RING_OF_LAW = 230;            // Blackrock Depths
    constexpr uint32 MOST_PLAYERS = 10;
    constexpr uint32 NPC_KEEPER = 910140;
    constexpr uint32 GO_SPOILS = 910140;
    constexpr uint32 ENEMY_FACTION = 14;                // hostile to everyone
    constexpr char const* PREFIX = "EFFP";

    Position const CENTER = { 596.48f, -187.91f, -54.14f, 4.9f };
    Position const GATES[] = {
        { 608.96f, -235.32f, -53.91f, 1.857f },        // the beasts' gate
        { 644.30f, -175.99f, -53.74f, 3.418f },        // the champions' gate
    };
    constexpr float ARENA_RADIUS = 40.0f;               // an enemy farther from the center is put back in

    constexpr uint32 COUNTDOWN_MS = 10000, WAVE_MS = 60000, BREATHER_MS = 5000, SPAWN_SPREAD_MS = 4000;
    constexpr uint32 TICK_MS = 1000, END_DELAY_MS = 4000, CORPSE_MS = 3000;
    constexpr float FIRST_WAVE = 0.35f, WAVE_GROWTH = 1.1f, GROUP_HEALTH = 0.4f, GROUP_COUNT = 0.2f;
    constexpr uint32 BOSS_EVERY = 5, LOOT_EVERY = 15, EPIC_FROM = 45, MOST_ALIVE = 150;
    constexpr float OVERFLOW_GROWTH = 1.03f;
    // A wave's boss: 4 times the health and 1.5 times the damage of the wave's others (unless the dungeon scaling
    // already counts it a boss: a boss_ script or a boss's rank, with its own health).
    constexpr float BOSS_HEALTH = 4.0f, BOSS_DAMAGE = 1.5f;
    constexpr uint32 SPOILS_SECONDS = 900;
    constexpr uint8 FIREPIT_LEVEL = 10;                 // the group finder's tab opens there too
    constexpr uint32 PROPOSAL_MS = 40000;               // a group's members this long to accept
    constexpr uint32 LEADERBOARD_ROWS = 5;              // a bracket's, in the group finder's tab
    constexpr uint32 CHECKPOINT_EVERY = 5;              // a run may start at any 5th wave its Heroes have all reached
    constexpr uint8 BRACKETS = 4;                       // records: 1 solo, 2 duo, 3 trio, 4 a group of 4 to 10

    uint8 Bracket(uint32 groupSize)
    {
        return uint8(std::clamp<uint32>(groupSize, 1, BRACKETS));
    }

    // The roster: the wave each kind joins at. The Ring of Law's beasts first, then the Depths' Dark Iron, their
    // fire, their golems and the Twilight's Hammer.
    struct Kind
    {
        uint32 wave;
        uint32 entry;
    };
    Kind const ROSTER[] = {
        { 1, 8932 },    // Borer Beetle
        { 2, 8925 },    // Dredge Worm
        { 3, 8933 },    // Cave Creeper
        { 4, 8926 },    // Deep Stinger
        { 6, 8927 },    // Dark Screecher
        { 7, 8928 },    // Thundersnout
        { 8, 8921 },    // Bloodhound
        { 9, 8892 },    // Anvilrage Footman
        { 11, 8894 },   // Anvilrage Medic
        { 12, 8909 },   // Fireguard
        { 13, 8912 },   // Twilight's Hammer Torturer
        { 14, 8893 },   // Anvilrage Soldier
        { 16, 8900 },   // Doomforge Arcanasmith
        { 17, 8910 },   // Blazing Fireguard
        { 18, 8922 },   // Bloodhound Mastiff
        { 19, 8913 },   // Twilight Emissary
        { 21, 8911 },   // Fireguard Destroyer
        { 22, 8898 },   // Anvilrage Marshal
        { 23, 8905 },   // Warbringer Construct
        { 24, 8906 },   // Ragereaver Golem
        { 26, 8899 },   // Doomforge Dragoon
        { 27, 8908 },   // Molten War Golem
        { 28, 8914 },   // Twilight Bodyguard
        { 29, 8903 },   // Anvilrage Captain
    };
    constexpr std::size_t NEWEST_KINDS = 6;             // a wave's enemies come from the newest few kinds

    // The bosses, one every 5th wave in turn: the Ring of Law's six, then the Depths' own; then around again.
    uint32 const BOSSES[] = {
        9027,   // Gorosh the Dervish
        9028,   // Grizzle
        9029,   // Eviscerator
        9030,   // Ok'thor the Breaker
        9031,   // Anub'shiah
        9032,   // Hedrum the Creeper
        9025,   // Lord Roccor
        9319,   // Houndmaster Grebmar
        9016,   // Bael'Gar
        9033,   // General Angerforge
        8983,   // Golem Lord Argelmach
        9938,   // Magmus
        9019,   // Emperor Dagran Thaurissan
    };

    // A Firepit enemy's own: its multipliers, its wave, whether it is a boss.
    struct Enemy : DataMap::Base
    {
        float health = 1.0f;
        float damage = 1.0f;
        uint32 wave = 0;
        bool boss = false;
    };

    // What the Keeper (or .firepit start) sets up for a run: who goes in, from which wave, and where each came from.
    struct Setup
    {
        std::vector<ObjectGuid> players;
        std::unordered_map<ObjectGuid, WorldLocation> homes;
        uint32 startWave = 1;
    };

    // Shared with the core's hooks (any thread): the runs being set up, who goes where, the Firepit's instances.
    std::mutex registryLock;
    std::unordered_map<uint32, Setup> setups;
    std::unordered_map<ObjectGuid, uint32> destinations;
    std::unordered_set<uint32> firepits;
    std::vector<std::pair<ObjectGuid, uint32>> unbinds;    // players who left a run: their bind to it to drop
    // The group finder tab's requests (from any map's thread), answered on the world's (titan_firepit_world).
    std::vector<std::pair<ObjectGuid, std::string>> requests;

    uint32 Destination(Player* player, uint32 mapId)
    {
        if (mapId != MAP_RING_OF_LAW)
            return 0;
        std::lock_guard guard(registryLock);
        auto destination = destinations.find(player->GetGUID());
        return destination != destinations.end() ? destination->second : 0;
    }

    uint32 MaxPlayers(uint32 instanceId)
    {
        std::lock_guard guard(registryLock);
        return firepits.count(instanceId) ? MOST_PLAYERS : 0;
    }

    void SendAddon(Player* player, std::string const& payload)
    {
        WorldPacket data;
        std::string const message = std::string(PREFIX) + '\t' + payload;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, message);
        player->SendDirectMessage(&data);
    }

    // No commas or tabs in a name sent to the addon.
    std::string Clean(std::string text)
    {
        std::replace(text.begin(), text.end(), ',', ' ');
        std::replace(text.begin(), text.end(), '\t', ' ');
        return text;
    }

    float WaveFactor(uint32 wave)
    {
        return FIRST_WAVE * std::pow(WAVE_GROWTH, float(std::max<uint32>(wave, 1) - 1));
    }

    // One Hero's run.
    struct Record
    {
        std::string name;
        uint8 level = 1;
        uint32 peakWave = 0, peakAlive = 0, mostAlive = 0;
        uint32 kills = 0, bossKills = 0, deaths = 0, deathWave = 0, resurrections = 0, loot = 0;
        uint64 damage = 0, healing = 0, taken = 0;
        uint32 biggestHit = 0, biggestTaken = 0;
        std::string killer;
        bool down = false;
        bool left = false;
    };

    struct Spawn
    {
        uint32 entry;
        bool boss;
        uint32 dueMs;
    };

    enum class Phase : uint8
    {
        WAITING,        // for the first Hero
        COUNTDOWN,      // the others coming in
        RUNNING,
        ENDING,         // the last one fell
        DONE,
    };
}

namespace
{
class FirepitInstance : public InstanceScript
{
public:
    explicit FirepitInstance(InstanceMap* map) : InstanceScript(map) { }

    void Initialize() override
    {
        std::lock_guard guard(registryLock);
        auto setup = setups.find(instance->GetInstanceId());
        if (setup != setups.end())
        {
            _setup = setup->second;
            setups.erase(setup);
        }
        _groupSize = std::max<uint32>(1, uint32(_setup.players.size()));
    }

    // The Depths' own creatures go as they load (they would come back in a week, after the instance is long gone).
    void OnCreatureCreate(Creature* creature) override
    {
        if (creature->GetSpawnId())
            _natives.push_back(creature->GetGUID());
    }

    void OnPlayerEnter(Player* player) override
    {
        Record& record = _records[player->GetGUID()];
        record.name = player->GetName();
        record.level = player->GetLevel();
        record.left = false;
        if (_phase == Phase::WAITING)
        {
            _phase = Phase::COUNTDOWN;
            _timer = COUNTDOWN_MS;
            Announce("The Firepit roars to life. The first wave comes in 10 seconds.");
        }
        SendHud(player);
    }

    void OnPlayerLeave(Player* player) override
    {
        auto record = _records.find(player->GetGUID());
        if (record != _records.end())
        {
            record->second.left = true;
            if (_phase == Phase::RUNNING && !record->second.down)
                FellNow(record->second, "left the Firepit");
        }
        std::lock_guard guard(registryLock);
        destinations.erase(player->GetGUID());
        unbinds.emplace_back(player->GetGUID(), instance->GetInstanceId());
    }

    void OnUnitDeath(Unit* unit) override
    {
        if (_phase != Phase::RUNNING)
            return;
        if (Player* player = unit->ToPlayer())
        {
            auto record = _records.find(player->GetGUID());
            if (record != _records.end() && !record->second.down)
                FellNow(record->second, "");
        }
    }

    void Update(uint32 diff) override
    {
        for (ObjectGuid const& guid : _natives)
            if (Creature* native = instance->GetCreature(guid))
                native->DespawnOrUnsummon(0ms, 7 * 24h);
        _natives.clear();

        switch (_phase)
        {
            case Phase::COUNTDOWN:
                if (_timer <= diff)
                {
                    _phase = Phase::RUNNING;
                    StartWave(_setup.startWave);
                }
                else
                    _timer -= diff;
                break;
            case Phase::RUNNING:
                Run(diff);
                break;
            case Phase::ENDING:
                if (_timer <= diff)
                    Finish();
                else
                    _timer -= diff;
                break;
            default:
                break;
        }
    }

    // --- What the hooks report ----------------------------------------------------------------------------------

    bool Running() const { return _phase == Phase::RUNNING; }
    bool Over() const { return _phase == Phase::DONE; }

    Record* RecordOf(Player* player)
    {
        auto record = _records.find(player->GetGUID());
        return record != _records.end() ? &record->second : nullptr;
    }

    void Damage(Player* attacker, uint32 damage)
    {
        if (Record* record = Running() ? RecordOf(attacker) : nullptr)
        {
            record->damage += damage;
            record->biggestHit = std::max(record->biggestHit, damage);
        }
    }

    void Taken(Player* victim, uint32 damage)
    {
        if (Record* record = Running() ? RecordOf(victim) : nullptr)
        {
            record->taken += damage;
            record->biggestTaken = std::max(record->biggestTaken, damage);
        }
    }

    void Healed(Player* healer, uint32 gain)
    {
        if (Record* record = Running() ? RecordOf(healer) : nullptr)
            record->healing += gain;
    }

    void Killed(Player* killer, Creature* enemy)
    {
        if (!Running())
            return;
        ++_kills;
        if (Record* record = RecordOf(killer))
        {
            ++record->kills;
            Enemy const* own = enemy->CustomData.Get<Enemy>("titan_firepit");
            if (own && own->boss)
                ++record->bossKills;
        }
    }

    void KilledBy(Player* player, Creature* killer)
    {
        if (Record* record = Running() ? RecordOf(player) : nullptr)
            record->killer = Clean(killer->GetName());
    }

    void Resurrected(Player* player)
    {
        if (Record* record = Running() ? RecordOf(player) : nullptr)
            if (record->down)
            {
                record->down = false;
                ++record->resurrections;
            }
    }

    // .firepit next: the timer out now.
    void SkipWave()
    {
        if (_phase == Phase::RUNNING)
            _waveMs = 0;
    }

    // .firepit stop: the run over now, as if the last one fell.
    void Stop()
    {
        if (_phase == Phase::RUNNING || _phase == Phase::COUNTDOWN)
        {
            _phase = Phase::ENDING;
            _timer = 0;
        }
    }

    uint32 Wave() const { return _wave; }
    uint32 Alive() const { return uint32(_enemies.size()); }

    // A spoils chest of the Hero's: what is in it (empty: not theirs, or already taken).
    std::vector<uint32> TakeSpoils(GameObject* chest, Player* player)
    {
        auto spoils = _spoils.find(chest->GetGUID());
        if (spoils == _spoils.end() || spoils->second.first != player->GetGUID())
            return {};
        std::vector<uint32> items = std::move(spoils->second.second);
        _spoils.erase(spoils);
        return items;
    }

    // Where the Hero came from (the Keeper's side), else nowhere.
    bool Home(Player* player, WorldLocation& home) const
    {
        auto found = _setup.homes.find(player->GetGUID());
        if (found == _setup.homes.end())
            return false;
        home = found->second;
        return true;
    }

private:
    // The run's news, on each Hero's wave tracker (Emberforge_Firepit): urgent, in red with a raid warning's sound.
    void Announce(std::string const& text, bool urgent = false)
    {
        for (auto const& ref : instance->GetPlayers())
            if (Player* player = ref.GetSource())
                SendAddon(player, "A," + Clean(text) + ',' + (urgent ? "1" : "0"));
    }

    std::vector<Player*> Living() const
    {
        std::vector<Player*> living;
        for (auto const& ref : instance->GetPlayers())
            if (Player* player = ref.GetSource())
                if (player->IsAlive() && !player->IsGameMaster())
                    living.push_back(player);
        return living;
    }

    void FellNow(Record& record, std::string const& how)
    {
        record.down = true;
        ++record.deaths;
        record.deathWave = _wave;
        if (!how.empty())
            record.killer = how;
    }

    void StartWave(uint32 wave)
    {
        // The wave just ended: who lived through it, and a piece of spoils every 15th.
        if (_wave)
            for (Player* player : Living())
                if (Record* record = RecordOf(player); record && _wave % LOOT_EVERY == 0)
                    ++record->loot;

        _wave = wave;
        _waveMs = WAVE_MS;
        _breather = false;
        for (Player* player : Living())
            if (Record* record = RecordOf(player))
            {
                record->peakWave = _wave;
                record->peakAlive = uint32(_enemies.size());
            }

        bool bossWave = _wave % BOSS_EVERY == 0;
        float groupCount = 1.0f + GROUP_COUNT * float(_groupSize - 1);
        uint32 count = uint32(std::lround(float(3 + _wave / 2) * groupCount * (bossWave ? 0.5f : 1.0f)));

        std::vector<uint32> kinds;
        for (Kind const& kind : ROSTER)
            if (kind.wave <= _wave)
                kinds.push_back(kind.entry);
        if (kinds.size() > NEWEST_KINDS)
            kinds.erase(kinds.begin(), kinds.end() - NEWEST_KINDS);

        for (uint32 i = 0; i < count && !kinds.empty(); ++i)
        {
            uint32 entry = i == 0 ? kinds.back() : kinds[urand(0, uint32(kinds.size()) - 1)];
            _queue.push_back({ entry, false, SPAWN_SPREAD_MS * i / std::max<uint32>(1, count) });
        }

        std::string text = "Wave " + std::to_string(_wave);
        if (bossWave)
        {
            uint32 bosses = 1 + (_wave >= 60 ? 1 : 0) + (_wave >= 120 ? 1 : 0);
            uint32 first = (_wave / BOSS_EVERY - 1) % std::size(BOSSES);
            for (uint32 i = 0; i < bosses; ++i)
            {
                uint32 entry = BOSSES[(first + i * 5) % std::size(BOSSES)];
                _queue.push_back({ entry, true, 0 });
                if (CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(entry))
                    text += (i ? " and " : ": ") + info->Name;
            }
            text += bosses > 1 ? " enter the Ring!" : " enters the Ring!";
        }
        Announce(text, bossWave);
    }

    void SpawnOne(Spawn const& spawn)
    {
        if (_enemies.size() >= MOST_ALIVE)
        {
            // Too many alive: the ones there grow instead.
            _overflow *= OVERFLOW_GROWTH;
            for (ObjectGuid const& guid : _enemies)
                if (Creature* enemy = instance->GetCreature(guid))
                    if (Enemy* own = enemy->CustomData.Get<Enemy>("titan_firepit"))
                    {
                        own->health *= OVERFLOW_GROWTH;
                        own->damage *= OVERFLOW_GROWTH;
                    }
            return;
        }
        Position const& gate = GATES[urand(0, uint32(std::size(GATES)) - 1)];
        Position at(gate.GetPositionX() + frand(-3.0f, 3.0f), gate.GetPositionY() + frand(-3.0f, 3.0f),
            gate.GetPositionZ(), gate.GetOrientation());
        TempSummon* enemy = instance->SummonCreature(spawn.entry, at);
        if (!enemy)
            return;
        Enemy* own = enemy->CustomData.GetDefault<Enemy>("titan_firepit");
        float factor = WaveFactor(_wave) * _overflow;
        own->health = factor * (1.0f + GROUP_HEALTH * float(_groupSize - 1));
        own->damage = factor;
        own->wave = _wave;
        own->boss = spawn.boss;
        if (spawn.boss && enemy->GetScriptName().rfind("boss_", 0) != 0
            && enemy->GetCreatureTemplate()->rank != CREATURE_ELITE_WORLDBOSS)
        {
            own->health *= BOSS_HEALTH;
            own->damage *= BOSS_DAMAGE;
        }
        enemy->SetFaction(ENEMY_FACTION);
        enemy->SetReactState(REACT_AGGRESSIVE);
        enemy->SetHomePosition(CENTER);
        enemy->SetFullHealth();
        for (uint16 index : { UNIT_FIELD_HEALTH, UNIT_FIELD_MAXHEALTH })
            enemy->ForceValuesUpdateAtIndex(index);
        _enemies.push_back(enemy->GetGUID());
        Engage(enemy);
    }

    // At the nearest living Hero.
    void Engage(Creature* enemy)
    {
        Player* nearest = nullptr;
        for (Player* player : Living())
            if (!nearest || enemy->GetExactDist(player) < enemy->GetExactDist(nearest))
                nearest = player;
        if (!nearest || !enemy->AI())
            return;
        enemy->SetInCombatWithZone();
        enemy->AI()->AttackStart(nearest);
    }

    void Run(uint32 diff)
    {
        _runMs += diff;
        for (Spawn& spawn : _queue)
            spawn.dueMs = spawn.dueMs > diff ? spawn.dueMs - diff : 0;
        while (!_queue.empty() && !_queue.front().dueMs)
        {
            SpawnOne(_queue.front());
            _queue.erase(_queue.begin());
        }
        for (std::size_t i = 0; i < _queue.size();)
        {
            if (!_queue[i].dueMs)
            {
                SpawnOne(_queue[i]);
                _queue.erase(_queue.begin() + i);
            }
            else
                ++i;
        }

        _waveMs = _waveMs > diff ? _waveMs - diff : 0;
        _tickMs += diff;
        if (_tickMs < TICK_MS && _waveMs)
            return;
        _tickMs = 0;

        // The fallen go (no loot, no corpses piling up); the rest keep at it, inside the ring.
        std::vector<ObjectGuid> alive;
        for (ObjectGuid const& guid : _enemies)
        {
            Creature* enemy = instance->GetCreature(guid);
            if (!enemy)
                continue;
            if (!enemy->IsAlive())
            {
                enemy->DespawnOrUnsummon(Milliseconds(CORPSE_MS));
                continue;
            }
            if (enemy->GetExactDist2d(CENTER.GetPositionX(), CENTER.GetPositionY()) > ARENA_RADIUS)
                enemy->NearTeleportTo(CENTER.GetPositionX(), CENTER.GetPositionY(), CENTER.GetPositionZ(),
                    enemy->GetOrientation());
            if (!enemy->GetVictim() || !enemy->GetVictim()->IsAlive())
                Engage(enemy);
            alive.push_back(guid);
        }
        _enemies = std::move(alive);

        std::vector<Player*> living = Living();
        for (Player* player : living)
            if (Record* record = RecordOf(player))
            {
                record->down = false;
                record->mostAlive = std::max(record->mostAlive, uint32(_enemies.size()));
                if (record->peakWave == _wave)
                    record->peakAlive = std::max(record->peakAlive, uint32(_enemies.size()));
            }
        for (auto& [guid, record] : _records)
            if (!record.down && !record.left)
                if (Player* player = ObjectAccessor::GetPlayer(instance, guid); player && !player->IsAlive())
                    FellNow(record, "");

        if (living.empty())
        {
            Announce("The Firepit claims its last challenger.", true);
            _phase = Phase::ENDING;
            _timer = END_DELAY_MS;
            return;
        }

        // A cleared wave: the next in 5 sec.
        if (_enemies.empty() && _queue.empty() && !_breather)
        {
            _breather = true;
            _waveMs = std::min(_waveMs, BREATHER_MS);
        }
        if (!_waveMs)
            StartWave(_wave + 1);

        for (auto const& ref : instance->GetPlayers())
            if (Player* player = ref.GetSource())
                SendHud(player);
    }

    void SendHud(Player* player)
    {
        uint32 living = uint32(Living().size());
        SendAddon(player, "H," + std::to_string(_wave) + ',' + std::to_string((_waveMs + 999) / 1000) + ','
            + std::to_string(_enemies.size()) + ',' + std::to_string(_wave && _wave % BOSS_EVERY == 0 ? 1 : 0) + ','
            + std::to_string(living) + ',' + std::to_string(_records.size()) + ','
            + std::to_string(_phase == Phase::COUNTDOWN ? (_timer + 999) / 1000 : 0));
    }

    void Finish()
    {
        _phase = Phase::DONE;
        for (ObjectGuid const& guid : _enemies)
            if (Creature* enemy = instance->GetCreature(guid))
                enemy->DespawnOrUnsummon();
        _enemies.clear();
        _queue.clear();

        // Everyone back on their feet, the spoils out, the records kept, the results shown.
        uint32 index = 0;
        std::vector<Player*> present;
        for (auto const& ref : instance->GetPlayers())
            if (Player* player = ref.GetSource())
                present.push_back(player);
        for (Player* player : present)
        {
            if (!player->IsAlive())
            {
                player->ResurrectPlayer(1.0f);
                player->SpawnCorpseBones();
            }
            player->CombatStop(true);
            player->SetFullHealth();
            player->SetPower(player->getPowerType(), player->GetMaxPower(player->getPowerType()));

            Record* record = RecordOf(player);
            if (!record || !record->loot)
                continue;
            float angle = float(index++) * 2.0f * float(M_PI) / float(std::max<std::size_t>(present.size(), 1));
            float x = CENTER.GetPositionX() + 4.0f * std::cos(angle);
            float y = CENTER.GetPositionY() + 4.0f * std::sin(angle);
            GameObject* chest = player->SummonGameObject(GO_SPOILS, x, y, CENTER.GetPositionZ(), angle + float(M_PI),
                0.0f, 0.0f, 0.0f, 0.0f, SPOILS_SECONDS);
            if (!chest)
                continue;
            std::vector<uint32> items;
            for (uint32 piece = 0; piece < record->loot; ++piece)
            {
                uint32 wave = (piece + 1) * LOOT_EVERY;
                if (uint32 entry = DungeonLoot::RandomPiece(player->GetLevel(),
                    wave >= EPIC_FROM ? ITEM_QUALITY_EPIC : ITEM_QUALITY_RARE))
                    items.push_back(entry);
            }
            _spoils[chest->GetGUID()] = { player->GetGUID(), items };
        }

        SaveRecords();
        for (Player* player : present)
            SendResults(player);
        if (TempSummon* keeper = instance->SummonCreature(NPC_KEEPER, Position(CENTER.GetPositionX() - 6.0f,
            CENTER.GetPositionY(), CENTER.GetPositionZ(), 0.0f)))
            keeper->SetFaction(35);
        Announce("The fires die down. Claim your spoils, and speak to the Keeper to leave.");
    }

    void SaveRecords()
    {
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        for (auto const& [guid, record] : _records)
        {
            if (!record.peakWave)
                continue;
            trans->Append("INSERT INTO `titan_firepit_records` (`guid`, `bracket`, `best_wave`, `group_size`, "
                "`kills`, `run_date`) VALUES ({}, {}, {}, {}, {}, UNIX_TIMESTAMP()) ON DUPLICATE KEY UPDATE "
                "`group_size` = IF(VALUES(`best_wave`) > `best_wave`, VALUES(`group_size`), `group_size`), "
                "`kills` = IF(VALUES(`best_wave`) > `best_wave`, VALUES(`kills`), `kills`), "
                "`run_date` = IF(VALUES(`best_wave`) > `best_wave`, VALUES(`run_date`), `run_date`), "
                "`best_wave` = GREATEST(`best_wave`, VALUES(`best_wave`))",
                guid.GetCounter(), Bracket(_groupSize), record.peakWave, _groupSize, record.kills);
        }
        CharacterDatabase.CommitTransaction(trans);
    }

    // The results screen's numbers: RUN, one P a Hero, then END.
    void SendResults(Player* player)
    {
        uint32 reached = 0;
        for (auto const& [guid, record] : _records)
            reached = std::max(reached, record.peakWave);
        SendAddon(player, "RUN," + std::to_string(reached) + ',' + std::to_string(_runMs / 1000) + ','
            + std::to_string(_groupSize) + ',' + std::to_string(_kills) + ','
            + std::to_string(uint32(std::lround((1.0f + GROUP_HEALTH * float(_groupSize - 1)) * 100.0f))) + ','
            + std::to_string(_setup.startWave));
        for (auto const& [guid, record] : _records)
            SendAddon(player, "P," + Clean(record.name) + ',' + std::to_string(record.level) + ','
                + std::to_string(record.peakWave) + ',' + std::to_string(record.peakAlive) + ','
                + std::to_string(record.mostAlive) + ',' + std::to_string(record.damage) + ','
                + std::to_string(record.healing) + ',' + std::to_string(record.taken) + ','
                + std::to_string(record.kills) + ',' + std::to_string(record.bossKills) + ','
                + std::to_string(record.biggestHit) + ',' + std::to_string(record.biggestTaken) + ','
                + std::to_string(record.deaths) + ',' + std::to_string(record.deathWave) + ','
                + std::to_string(record.resurrections) + ',' + std::to_string(record.loot) + ','
                + Clean(record.killer.substr(0, 40)));
        SendAddon(player, "END");
    }

    Setup _setup;
    uint32 _groupSize = 1;
    Phase _phase = Phase::WAITING;
    uint32 _timer = 0, _wave = 0, _waveMs = 0, _tickMs = 0, _runMs = 0, _kills = 0;
    bool _breather = false;
    float _overflow = 1.0f;
    std::vector<ObjectGuid> _natives, _enemies;
    std::vector<Spawn> _queue;
    std::map<ObjectGuid, Record> _records;
    std::unordered_map<ObjectGuid, std::pair<ObjectGuid, std::vector<uint32>>> _spoils;
};

    FirepitInstance* RunOf(Map* map)
    {
        if (!map || map->GetId() != MAP_RING_OF_LAW || !map->IsDungeon())
            return nullptr;
        InstanceMap* instanceMap = map->ToInstanceMap();
        return instanceMap ? dynamic_cast<FirepitInstance*>(instanceMap->GetInstanceScript()) : nullptr;
    }

    // Who goes in with the Hero: their group's members near them (all of them with a GM's start), or just them.
    bool Gather(Player* leader, bool anywhere, std::vector<Player*>& members, std::string& why)
    {
        members.clear();
        Group* group = leader->GetGroup();
        if (group && group->GetLeaderGUID() != leader->GetGUID())
        {
            why = "Only your group's leader can lead it into the Firepit.";
            return false;
        }
        if (!group)
            members.push_back(leader);
        else
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                if (Player* member = ref->GetSource(); member && member->IsInWorld() && member->GetSession())
                {
                    if (!anywhere && (member->GetMap() != leader->GetMap() || !member->IsWithinDist(leader, 40.0f)))
                    {
                        why = member->GetName() + " is too far away.";
                        return false;
                    }
                    members.push_back(member);
                }
        if (members.size() > MOST_PLAYERS)
        {
            why = "The Firepit takes at most 10.";
            return false;
        }
        for (Player* member : members)
        {
            if (member->GetLevel() < FIREPIT_LEVEL)
            {
                why = member->GetName() + " must be level " + std::to_string(FIREPIT_LEVEL) + ".";
                return false;
            }
            if (!member->IsAlive() || member->IsInCombat() || member->GetMap()->Instanceable())
            {
                why = member->GetName() + " must be alive, out of combat and out of any instance.";
                return false;
            }
        }
        return true;
    }

    // A new run: its instance made, everyone sent in.
    bool StartRun(Player* leader, uint32 startWave, bool anywhere, std::string& why)
    {
        std::vector<Player*> members;
        if (!Gather(leader, anywhere, members, why))
            return false;
        uint32 instanceId = sMapMgr->GenerateInstanceId();
        if (!sInstanceSaveMgr->AddInstanceSave(MAP_RING_OF_LAW, instanceId, DUNGEON_DIFFICULTY_NORMAL))
        {
            why = "The Firepit could not be lit.";
            return false;
        }
        {
            std::lock_guard guard(registryLock);
            Setup& setup = setups[instanceId];
            setup.startWave = std::max<uint32>(1, startWave);
            for (Player* member : members)
            {
                setup.players.push_back(member->GetGUID());
                setup.homes[member->GetGUID()] = member->GetWorldLocation();
                destinations[member->GetGUID()] = instanceId;
            }
            firepits.insert(instanceId);
        }
        for (Player* member : members)
            member->TeleportTo(MAP_RING_OF_LAW, CENTER.GetPositionX() + frand(-2.0f, 2.0f),
                CENTER.GetPositionY() + frand(-2.0f, 2.0f), CENTER.GetPositionZ(), CENTER.GetOrientation());
        return true;
    }

    void SendHome(Player* player)
    {
        WorldLocation home;
        FirepitInstance* run = RunOf(player->GetMap());
        if (run && run->Home(player, home))
            player->TeleportTo(home);
        else
            player->TeleportTo(player->m_homebindMapId, player->m_homebindX, player->m_homebindY, player->m_homebindZ,
                player->GetOrientation());
    }

    void ShowLeaderboard(Player* player)
    {
        ChatHandler handler(player->GetSession());
        char const* const titles[] = { "Solo", "Duo", "Trio", "Group" };
        for (uint8 bracket = 1; bracket <= BRACKETS; ++bracket)
        {
            handler.PSendSysMessage("|cffff7f2aThe Firepit - {}:|r", titles[bracket - 1]);
            QueryResult result = CharacterDatabase.Query("SELECT c.`name`, b.`best_wave`, b.`group_size` FROM "
                "`titan_firepit_records` b JOIN `characters` c ON c.`guid` = b.`guid` WHERE b.`bracket` = {} "
                "ORDER BY b.`best_wave` DESC, b.`run_date` ASC LIMIT {}", bracket, LEADERBOARD_ROWS);
            if (!result)
            {
                handler.PSendSysMessage("  No one yet.");
                continue;
            }
            uint32 rank = 0;
            do
            {
                Field* fields = result->Fetch();
                handler.PSendSysMessage("  {}. {}: wave {}{}", ++rank, fields[0].Get<std::string>(),
                    fields[1].Get<uint32>(), bracket == BRACKETS
                        ? " (group of " + std::to_string(fields[2].Get<uint32>()) + ")" : std::string());
            } while (result->NextRow());
        }
    }

    // The Hero's best wave in any bracket (0 none), and the latest checkpoint they may start from.
    uint32 BestWave(ObjectGuid const& guid)
    {
        QueryResult result = CharacterDatabase.Query("SELECT MAX(`best_wave`) FROM `titan_firepit_records` "
            "WHERE `guid` = {}", guid.GetCounter());
        return result && !result->Fetch()[0].IsNull() ? result->Fetch()[0].Get<uint32>() : 0;
    }

    uint32 Checkpoint(uint32 best)
    {
        return best >= CHECKPOINT_EVERY ? best / CHECKPOINT_EVERY * CHECKPOINT_EVERY : 1;
    }
}

// The group finder's Firepit tab (Emberforge_Firepit): the state, the leaderboard, and the queue. A Hero alone goes in
// at once; a group's leader proposes, every member accepts (40 sec), then all go in together. On the world's thread
// only (members on other maps are teleported from there).
namespace
{
    struct Proposal
    {
        uint32 startWave = 1;
        std::vector<ObjectGuid> members;
        std::unordered_map<ObjectGuid, uint8> answers;      // 0 waiting, 1 accepted, 2 declined
        uint32 expiresAt = 0;
    };
    std::unordered_map<ObjectGuid, Proposal> proposals;      // by leader

    std::pair<ObjectGuid const, Proposal>* ProposalOf(ObjectGuid const& guid)
    {
        for (auto& entry : proposals)
            if (std::find(entry.second.members.begin(), entry.second.members.end(), guid) != entry.second.members.end())
                return &entry;
        return nullptr;
    }

    void ToMembers(Proposal const& proposal, std::string const& payload)
    {
        for (ObjectGuid const& guid : proposal.members)
            if (Player* member = ObjectAccessor::FindConnectedPlayer(guid))
                SendAddon(member, payload);
    }

    // S,<best solo>,<duo>,<trio>,<group>,<in a proposal 0|1>,<the level it opens at>,<latest checkpoint>, then each
    // bracket's leaderboard: L,<bracket 1-4>,<rank>,<name>,<wave>,<group size> and LE.
    void SendFinderState(Player* player)
    {
        uint32 best[BRACKETS + 1] = {};
        if (QueryResult own = CharacterDatabase.Query("SELECT `bracket`, `best_wave` FROM `titan_firepit_records` "
            "WHERE `guid` = {}", player->GetGUID().GetCounter()))
            do
            {
                uint8 bracket = own->Fetch()[0].Get<uint8>();
                if (bracket >= 1 && bracket <= BRACKETS)
                    best[bracket] = own->Fetch()[1].Get<uint32>();
            } while (own->NextRow());
        uint32 highest = *std::max_element(std::begin(best), std::end(best));
        SendAddon(player, "S," + std::to_string(best[1]) + ',' + std::to_string(best[2]) + ','
            + std::to_string(best[3]) + ',' + std::to_string(best[4]) + ','
            + std::to_string(ProposalOf(player->GetGUID()) ? 1 : 0) + ',' + std::to_string(FIREPIT_LEVEL) + ','
            + std::to_string(Checkpoint(highest)));
        for (uint8 bracket = 1; bracket <= BRACKETS; ++bracket)
        {
            QueryResult result = CharacterDatabase.Query("SELECT c.`name`, b.`best_wave`, b.`group_size` FROM "
                "`titan_firepit_records` b JOIN `characters` c ON c.`guid` = b.`guid` WHERE b.`bracket` = {} "
                "ORDER BY b.`best_wave` DESC, b.`run_date` ASC LIMIT {}", bracket, LEADERBOARD_ROWS);
            uint32 rank = 0;
            if (result)
                do
                {
                    Field* fields = result->Fetch();
                    SendAddon(player, "L," + std::to_string(bracket) + ',' + std::to_string(++rank) + ','
                        + Clean(fields[0].Get<std::string>()) + ',' + std::to_string(fields[1].Get<uint32>()) + ','
                        + std::to_string(fields[2].Get<uint32>()));
                } while (result->NextRow());
        }
        SendAddon(player, "LE");
    }

    void CancelProposal(ObjectGuid const& leader, std::string const& why)
    {
        auto found = proposals.find(leader);
        if (found == proposals.end())
            return;
        ToMembers(found->second, "QC," + Clean(why));
        proposals.erase(found);
    }

    // A run from wave 1, or from a checkpoint (every 5th) all its Heroes have reached.
    void Queue(Player* player, uint32 startWave)
    {
        std::vector<Player*> members;
        std::string why;
        if (ProposalOf(player->GetGUID()))
            return;
        if (!Gather(player, true, members, why))
        {
            SendAddon(player, "QC," + Clean(why));
            return;
        }
        if (startWave != 1)
        {
            if (!startWave || startWave % CHECKPOINT_EVERY)
                return;
            for (Player* member : members)
                if (Checkpoint(BestWave(member->GetGUID())) < startWave)
                {
                    SendAddon(player, "QC," + Clean(member->GetName() + " has not reached wave "
                        + std::to_string(startWave) + "."));
                    return;
                }
        }
        if (members.size() == 1)
        {
            if (!StartRun(player, startWave, true, why))
                SendAddon(player, "QC," + Clean(why));
            else
                SendAddon(player, "QG");
            return;
        }
        Proposal& proposal = proposals[player->GetGUID()];
        proposal.startWave = startWave;
        proposal.expiresAt = GameTime::GetGameTimeMS().count() + PROPOSAL_MS;
        for (Player* member : members)
        {
            proposal.members.push_back(member->GetGUID());
            proposal.answers[member->GetGUID()] = member == player ? 1 : 0;
        }
        ToMembers(proposal, "QP," + Clean(player->GetName()) + ',' + std::to_string(members.size()) + ','
            + std::to_string(PROPOSAL_MS / 1000) + ',' + std::to_string(startWave));
        for (Player* member : members)
            ToMembers(proposal, "QA," + Clean(member->GetName()) + ',' + std::to_string(proposal.answers[
                member->GetGUID()]));
    }

    void Answer(Player* player, bool accept)
    {
        auto* entry = ProposalOf(player->GetGUID());
        if (!entry)
            return;
        ObjectGuid leader = entry->first;
        Proposal& proposal = entry->second;
        if (!accept)
        {
            CancelProposal(leader, player->GetName() + " declined.");
            return;
        }
        proposal.answers[player->GetGUID()] = 1;
        ToMembers(proposal, "QA," + Clean(player->GetName()) + ",1");
        for (auto const& [guid, answer] : proposal.answers)
            if (answer != 1)
                return;
        // Everyone in: the group as it is now goes.
        Player* lead = ObjectAccessor::FindConnectedPlayer(leader);
        std::string why = "The group's leader is gone.";
        if (lead && StartRun(lead, proposal.startWave, true, why))
        {
            ToMembers(proposal, "QG");
            proposals.erase(leader);
        }
        else
            CancelProposal(leader, why);
    }

    void HandleRequest(Player* player, std::string const& request)
    {
        if (request == "STATE")
            SendFinderState(player);
        else if (request == "QUEUE")
            Queue(player, 1);
        else if (request.rfind("QUEUE,", 0) == 0)
        {
            Optional<uint32> wave = Acore::StringTo<uint32>(std::string_view(request).substr(6));
            if (wave)
                Queue(player, *wave);
        }
        else if (request == "ACCEPT")
            Answer(player, true);
        else if (request == "DECLINE")
            Answer(player, false);
        else if (request == "LEAVE")
        {
            if (auto* entry = ProposalOf(player->GetGUID()))
                CancelProposal(entry->first, player->GetName() + " left the queue.");
        }
    }

    void ExpireProposals()
    {
        uint32 now = GameTime::GetGameTimeMS().count();
        std::vector<ObjectGuid> expired;
        for (auto const& [leader, proposal] : proposals)
            if (now >= proposal.expiresAt)
                expired.push_back(leader);
        for (ObjectGuid const& leader : expired)
            CancelProposal(leader, "Not everyone accepted in time.");
    }
}

float Firepit::HealthMultiplier(Creature const* creature)
{
    Enemy const* own = creature ? creature->CustomData.Get<Enemy>("titan_firepit") : nullptr;
    return own ? own->health : 1.0f;
}

float Firepit::DamageMultiplier(Creature const* creature)
{
    Enemy const* own = creature ? creature->CustomData.Get<Enemy>("titan_firepit") : nullptr;
    return own ? own->damage : 1.0f;
}

// A Firepit instance's script is its own (the Depths' never runs there).
class titan_firepit_map : public AllMapScript
{
public:
    titan_firepit_map() : AllMapScript("titan_firepit_map", { ALLMAPHOOK_ON_BEFORE_CREATE_INSTANCE_SCRIPT,
        ALLMAPHOOK_ON_DESTROY_INSTANCE }) { }

    void OnBeforeCreateInstanceScript(InstanceMap* map, InstanceScript** script, bool /*load*/,
        std::string /*data*/, uint32 /*completedEncounterMask*/) override
    {
        if (map->GetId() != MAP_RING_OF_LAW)
            return;
        std::lock_guard guard(registryLock);
        if (firepits.count(map->GetInstanceId()))
            *script = new FirepitInstance(map);
    }

    void OnDestroyInstance(MapInstanced* /*mapInstanced*/, Map* map) override
    {
        std::lock_guard guard(registryLock);
        firepits.erase(map->GetInstanceId());
        setups.erase(map->GetInstanceId());
        for (auto destination = destinations.begin(); destination != destinations.end();)
            destination = destination->second == map->GetInstanceId() ? destinations.erase(destination)
                : std::next(destination);
    }
};

// Players who left a run lose its bind (the Depths' own instance theirs again); the core's hooks are ours.
class titan_firepit_world : public WorldScript
{
public:
    titan_firepit_world() : WorldScript("titan_firepit_world", { WORLDHOOK_ON_STARTUP, WORLDHOOK_ON_UPDATE }) { }

    void OnStartup() override
    {
        InstanceOverride::Destination = &Destination;
        InstanceOverride::MaxPlayers = &MaxPlayers;
    }

    void OnUpdate(uint32 /*diff*/) override
    {
        std::vector<std::pair<ObjectGuid, std::string>> asked;
        std::vector<std::pair<ObjectGuid, uint32>> pending;
        {
            std::lock_guard guard(registryLock);
            asked.swap(requests);
            pending.swap(unbinds);
        }
        for (auto const& [guid, request] : asked)
            if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
                HandleRequest(player, request);
        if (!proposals.empty())
            ExpireProposals();
        if (pending.empty())
            return;
        std::vector<std::pair<ObjectGuid, uint32>> later;
        for (auto const& [guid, instanceId] : pending)
        {
            Player* player = ObjectAccessor::FindConnectedPlayer(guid);
            if (player && player->GetMapId() == MAP_RING_OF_LAW && player->GetInstanceId() == instanceId)
                continue;                               // back in it
            if (player && player->IsBeingTeleported())
            {
                later.emplace_back(guid, instanceId);   // still on the way out
                continue;
            }
            InstancePlayerBind* bind = sInstanceSaveMgr->PlayerGetBoundInstance(guid, MAP_RING_OF_LAW,
                DUNGEON_DIFFICULTY_NORMAL);
            if (bind && bind->save && bind->save->GetInstanceId() == instanceId)
                sInstanceSaveMgr->PlayerUnbindInstance(guid, MAP_RING_OF_LAW, DUNGEON_DIFFICULTY_NORMAL, true, player);
        }
        if (!later.empty())
        {
            std::lock_guard guard(registryLock);
            unbinds.insert(unbinds.end(), later.begin(), later.end());
        }
    }
};

// What a run counts: damage, healing, kills, deaths, resurrections.
class titan_firepit_unit : public UnitScript
{
public:
    titan_firepit_unit() : UnitScript("titan_firepit_unit", true, { UNITHOOK_ON_DAMAGE, UNITHOOK_ON_HEAL }) { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (!attacker || !victim || !damage)
            return;
        FirepitInstance* run = RunOf(victim->GetMap());
        if (!run)
            return;
        if (Player* player = attacker->GetCharmerOrOwnerPlayerOrPlayerItself(); player && victim->ToCreature())
            run->Damage(player, damage);
        if (Player* player = victim->ToPlayer(); player && attacker->ToCreature()
            && !attacker->GetCharmerOrOwnerPlayerOrPlayerItself())
            run->Taken(player, damage);
    }

    void OnHeal(Unit* healer, Unit* receiver, uint32& gain) override
    {
        if (!healer || !receiver || !gain)
            return;
        if (FirepitInstance* run = RunOf(receiver->GetMap()))
            if (Player* player = healer->GetCharmerOrOwnerPlayerOrPlayerItself())
                run->Healed(player, gain);
    }
};

class titan_firepit_player : public PlayerScript
{
public:
    titan_firepit_player() : PlayerScript("titan_firepit_player", {
        PLAYERHOOK_ON_CREATURE_KILL,
        PLAYERHOOK_ON_CREATURE_KILLED_BY_PET,
        PLAYERHOOK_ON_PLAYER_KILLED_BY_CREATURE,
        PLAYERHOOK_ON_PLAYER_RESURRECT,
        PLAYERHOOK_CAN_REPOP_AT_GRAVEYARD,
        PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT
    }) { }

    // The group finder tab's requests (STATE, QUEUE, ACCEPT, DECLINE, LEAVE): to the world's thread.
    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 language, std::string& msg,
        Player* receiver) override
    {
        std::string const prefix = std::string(PREFIX) + '\t';
        if (language != LANG_ADDON || receiver != player || msg.compare(0, prefix.size(), prefix) != 0)
            return true;
        std::lock_guard guard(registryLock);
        requests.emplace_back(player->GetGUID(), msg.substr(prefix.size()));
        return false;
    }

    void OnPlayerCreatureKill(Player* killer, Creature* killed) override
    {
        if (FirepitInstance* run = RunOf(killer->GetMap()))
            run->Killed(killer, killed);
    }

    void OnPlayerCreatureKilledByPet(Player* owner, Creature* killed) override
    {
        if (FirepitInstance* run = RunOf(owner->GetMap()))
            run->Killed(owner, killed);
    }

    void OnPlayerKilledByCreature(Creature* killer, Player* killed) override
    {
        if (FirepitInstance* run = RunOf(killed->GetMap()))
            run->KilledBy(killed, killer);
    }

    void OnPlayerResurrect(Player* player, float /*restorePercent*/, bool& /*applySickness*/) override
    {
        if (FirepitInstance* run = RunOf(player->GetMap()))
            run->Resurrected(player);
    }

    // The fallen stay where they fell (a ghost there): a battle resurrection can still reach them.
    bool OnPlayerCanRepopAtGraveyard(Player* player) override
    {
        FirepitInstance* run = RunOf(player->GetMap());
        return !run || run->Over();
    }
};

// The Firepit Keeper: outside, the way in, the leaderboard; inside, the way out.
class npc_titan_firepit_keeper : public CreatureScript
{
public:
    npc_titan_firepit_keeper() : CreatureScript("npc_titan_firepit_keeper") { }

    enum Actions : uint32
    {
        ACTION_ENTER = 1,
        ACTION_LEADERBOARD = 2,
        ACTION_LEAVE = 3,
    };

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);
        if (RunOf(player->GetMap()))
            AddGossipItemFor(player, GOSSIP_ICON_TAXI, "Take me out of the Firepit.", GOSSIP_SENDER_MAIN, ACTION_LEAVE);
        else
        {
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Open the Firepit (the group finder).", GOSSIP_SENDER_MAIN,
                ACTION_ENTER);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Show me the leaderboard.", GOSSIP_SENDER_MAIN,
                ACTION_LEADERBOARD);
        }
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* /*creature*/, uint32 /*sender*/, uint32 action) override
    {
        CloseGossipMenuFor(player);
        switch (action)
        {
            case ACTION_ENTER:
                SendAddon(player, "OPEN");      // the group finder's Firepit tab: the queue
                break;
            case ACTION_LEADERBOARD:
                ShowLeaderboard(player);
                break;
            case ACTION_LEAVE:
                SendHome(player);
                break;
            default:
                break;
        }
        return true;
    }
};

// A Hero's spoils chest: theirs alone.
class go_titan_firepit_spoils : public GameObjectScript
{
public:
    go_titan_firepit_spoils() : GameObjectScript("go_titan_firepit_spoils") { }

    bool OnGossipHello(Player* player, GameObject* chest) override
    {
        FirepitInstance* run = RunOf(chest->GetMap());
        std::vector<uint32> items = run ? run->TakeSpoils(chest, player) : std::vector<uint32>();
        if (items.empty())
        {
            ChatHandler(player->GetSession()).SendSysMessage("These spoils are someone else's.");
            return true;
        }
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        bool mailed = false;
        for (uint32 entry : items)
        {
            if (player->AddItem(entry, 1))
                continue;
            if (Item* item = Item::CreateItem(entry, 1, player))
            {
                item->SaveToDB(trans);
                MailDraft("Spoils of the Firepit", "Your bags were full; the Firepit's spoils came here.")
                    .AddItem(item)
                    .SendMailTo(trans, player, MailSender(MAIL_CREATURE, NPC_KEEPER), MAIL_CHECK_MASK_COPIED);
                mailed = true;
            }
        }
        CharacterDatabase.CommitTransaction(trans);
        if (mailed)
            ChatHandler(player->GetSession()).SendSysMessage("Your bags were full: the rest of your spoils are in "
                "your mailbox.");
        chest->SetRespawnTime(0);
        chest->Delete();
        return true;
    }
};

using namespace Acore::ChatCommands;

// .firepit start [wave] (the GM's group, wherever they are), .firepit next, .firepit stop.
class titan_firepit_commands : public CommandScript
{
public:
    titan_firepit_commands() : CommandScript("titan_firepit_commands") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable firepit =
        {
            { "start", HandleStart, SEC_GAMEMASTER, Console::No },
            { "next", HandleNext, SEC_GAMEMASTER, Console::No },
            { "stop", HandleStop, SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commandTable =
        {
            { "firepit", firepit },
        };
        return commandTable;
    }

    static bool HandleStart(ChatHandler* handler, Optional<uint32> wave)
    {
        std::string why;
        if (!StartRun(handler->GetSession()->GetPlayer(), wave.value_or(1), true, why))
            handler->SendSysMessage(why);
        return true;
    }

    static bool HandleNext(ChatHandler* handler)
    {
        FirepitInstance* run = RunOf(handler->GetSession()->GetPlayer()->GetMap());
        if (!run)
        {
            handler->SendSysMessage("You are not in the Firepit.");
            return true;
        }
        run->SkipWave();
        return true;
    }

    static bool HandleStop(ChatHandler* handler)
    {
        FirepitInstance* run = RunOf(handler->GetSession()->GetPlayer()->GetMap());
        if (!run)
        {
            handler->SendSysMessage("You are not in the Firepit.");
            return true;
        }
        run->Stop();
        return true;
    }
};

void AddSC_firepit()
{
    new titan_firepit_map();
    new titan_firepit_world();
    new titan_firepit_unit();
    new titan_firepit_player();
    new npc_titan_firepit_keeper();
    new go_titan_firepit_spoils();
    new titan_firepit_commands();
}
