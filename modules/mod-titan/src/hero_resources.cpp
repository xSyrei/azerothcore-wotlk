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

#include "Chat.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Unit.h"
#include "World.h"
#include "WorldPacket.h"
#include <algorithm>
#include <array>
#include <unordered_map>

// Classless Hero: every class resource at once.
//
// The Hero is the hidden base class (Paladin, see data/sql/db-world/hero_class.sql). Its display
// bar stays mana; this script makes the core also run rage (gain from combat, decay out of
// combat), energy (regen) and runes for Heroes, and
// treats a Hero as every class for ability mechanics (runes, combo points, reactive abilities like
// Overpower and Revenge, form power swaps), and as a Hunter for pets (taming, resummoning,
// stables). Class contexts that are not about abilities (starting zone, quests, trainers, stats,
// equipment) keep the Hero's real class.
class hero_resources : public PlayerScript
{
public:
    hero_resources() : PlayerScript("hero_resources", {
        PLAYERHOOK_ON_PLAYER_IS_CLASS,
        PLAYERHOOK_ON_PLAYER_HAS_ACTIVE_POWER_TYPE
    }) { }

    static constexpr uint8 HERO_CLASS = CLASS_PALADIN;

    static bool IsHero(Player const* player)
    {
        return player->getClass() == HERO_CLASS;
    }

    Optional<bool> OnPlayerIsClass(Player const* player, Classes playerClass, ClassContext context) override
    {
        if (!IsHero(player))
            return {};

        if (context == CLASS_CONTEXT_ABILITY || context == CLASS_CONTEXT_ABILITY_REACTIVE)
            return true;

        // Relics: Heroes can equip librams, idols, totems and sigils (see hero_equipment.cpp).
        if (context == CLASS_CONTEXT_EQUIP_RELIC)
            return true;

        // Hunter pets: Heroes can tame (Wildcard's Tame Beast) and keep, revive and stable their pets.
        if (context == CLASS_CONTEXT_PET && playerClass == CLASS_HUNTER)
            return true;

        return {};
    }

    bool OnPlayerHasActivePowerType(Player const* player, Powers power) override
    {
        if (!IsHero(player))
            return false;

        switch (power)
        {
            case POWER_MANA:
            case POWER_RAGE:
            case POWER_ENERGY:
            case POWER_RUNE:
                return true;
            default:
                return false;
        }
    }
};

// Emberforged Hero (90600, client-patch/patch_player.py): every stat counts for a Hero. The buff and its two hidden
// halves (90601, 90602) go on every Hero at login, and back after anything took them off (a resurrection).
class hero_emberforged : public PlayerScript
{
public:
    hero_emberforged() : PlayerScript("hero_emberforged", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_PLAYER_RESURRECT
    }) { }

    static constexpr std::array<uint32, 3> SPELLS = { 90600, 90601, 90602 };

    // fresh (at login): put on again even if there, so a buff saved before its spell gained an effect (the
    // pushback cut) has it: a loaded aura keeps the effects it was saved with.
    static void Forge(Player* player, bool fresh = false)
    {
        if (player->getClass() != hero_resources::HERO_CLASS)
            return;
        for (uint32 spellId : SPELLS)
        {
            if (fresh)
                player->RemoveAurasDueToSpell(spellId);
            if (!player->HasAura(spellId))
                player->AddAura(spellId, player);
        }
    }

    void OnPlayerLogin(Player* player) override
    {
        Forge(player, true);
    }

    void OnPlayerResurrect(Player* player, float /*restorePercent*/, bool& /*applySickness*/) override
    {
        Forge(player);
    }
};

// Runes for Heroes. The server recharges them (Player::RegenerateAll treats a Hero as a Death Knight), but the
// client only counts a spent rune's cooldown down for a real Death Knight: for a Hero it stays spent on the rune bar
// and the client refuses rune spells. So while a Hero has a rune cooling down, the server sends the runes' state
// every half second (SMSG_RESYNC_RUNES: each rune's type and how much of its cooldown has passed, 0-255), and the
// moment one is spent or ready, at once. A ready rune's state goes out before its SMSG_ADD_RUNE_POWER: the client
// casts from the resynced cooldown, so a rune that only got SMSG_ADD_RUNE_POWER looked ready on the rune bar but
// stayed spent for casting, the last resync having found it still cooling down (until a relog reset the runes).
class hero_runes : public PlayerScript
{
public:
    hero_runes() : PlayerScript("hero_runes", {
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_LOGOUT
    }) { }

    static constexpr uint32 RESYNC_INTERVAL = 500;
    // While no rune cools, the state goes again this often: the client knows a Hero's runes only from these (none
    // came before the first rune was spent, so the rune bar stood empty after a login or a reload).
    static constexpr uint32 IDLE_RESYNC_INTERVAL = 10000;

    struct State
    {
        uint32 Timer = 0;
        uint32 IdleTimer = IDLE_RESYNC_INTERVAL;     // the first update sends them
        uint8 Cooling = 0;      // runes cooling down at the last update, a bit each
        int16 ComboPoints = -1; // the combo points last sent (-1: none sent yet)
    };

    // Combo points, likewise: the client keeps them only for a Rogue or Druid, so a Hero's never showed. The
    // Emberforge_Resources addon (Combo.lua) gets them (prefix EFCP, the count) and shows them on the combo frame.
    static void SendComboPoints(Player* player, State& state)
    {
        uint8 points = player->GetComboPoints();
        if (state.ComboPoints == points)
            return;
        state.ComboPoints = points;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
            "EFCP\t" + std::to_string(points));
        player->SendDirectMessage(&data);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (!hero_resources::IsHero(player) || !player->IsInWorld())
            return;

        State& state = _states[player->GetGUID().GetCounter()];
        SendComboPoints(player, state);
        uint8 cooling = 0;
        for (uint8 i = 0; i < MAX_RUNES; ++i)
            if (player->GetRuneCooldown(i))
                cooling |= 1 << i;

        uint8 ready = state.Cooling & ~cooling;
        bool changed = cooling != state.Cooling;
        state.Cooling = cooling;

        state.Timer += diff;
        if (changed || (cooling && state.Timer >= RESYNC_INTERVAL))
        {
            state.Timer = 0;
            Resync(player);
        }
        if (!cooling)
        {
            state.Timer = 0;
            state.IdleTimer += diff;
            if (state.IdleTimer >= IDLE_RESYNC_INTERVAL)
            {
                state.IdleTimer = 0;
                Resync(player);
            }
        }

        for (uint8 i = 0; i < MAX_RUNES; ++i)
            if (ready & (1 << i))
                player->AddRunePower(i);
    }

    // Every rune's type and how much of its cooldown has passed (255: ready).
    static void Resync(Player* player)
    {
        WorldPacket data(SMSG_RESYNC_RUNES, 4 + MAX_RUNES * 2);
        data << uint32(MAX_RUNES);
        for (uint8 i = 0; i < MAX_RUNES; ++i)
        {
            uint32 cooldown = player->GetRuneCooldown(i);
            uint32 base = std::max<uint32>(1, player->GetRuneBaseCooldown(i, true));
            data << uint8(player->GetCurrentRune(i));
            data << uint8(cooldown >= base ? 0 : 255 * (base - cooldown) / base);
        }
        player->SendDirectMessage(&data);
    }

    void OnPlayerLogout(Player* player) override
    {
        _states.erase(player->GetGUID().GetCounter());
    }

private:
    std::unordered_map<ObjectGuid::LowType, State> _states;
};

// Emberforge has no Runic Power: every spell costing it costs Rage (the same tenths), and every effect giving, taking
// or raising it does so to Rage, as the client patch does (client-patch/patch_spells.py, convert_runic_power). Runes
// give Rage instead too (a core change in Spell::TakeRunePower).
// A mana cost's share kept: 35%, rounded half up (as patch_spells.py's reduce_mana_costs).
static uint32 ManaShare(uint32 cost)
{
    return (cost * 35 + 50) / 100;
}

class titan_runic_power_to_rage : public GlobalScript
{
public:
    titan_runic_power_to_rage() : GlobalScript("titan_runic_power_to_rage", {
        GLOBALHOOK_ON_LOAD_SPELL_CUSTOM_ATTR
    }) { }

    static bool IsPowerEffect(SpellEffectInfo const& effect)
    {
        switch (effect.Effect)
        {
            case SPELL_EFFECT_POWER_DRAIN:
            case SPELL_EFFECT_ENERGIZE:
            case SPELL_EFFECT_POWER_BURN:
            case SPELL_EFFECT_ENERGIZE_PCT:
                return true;
            case SPELL_EFFECT_APPLY_AURA:
                switch (effect.ApplyAuraName)
                {
                    case SPELL_AURA_PERIODIC_ENERGIZE:
                    case SPELL_AURA_MOD_INCREASE_ENERGY:
                    case SPELL_AURA_MOD_POWER_REGEN:
                    case SPELL_AURA_MOD_POWER_REGEN_PERCENT:
                    case SPELL_AURA_MOD_INCREASE_ENERGY_PERCENT:
                        return true;
                    default:
                        return false;
                }
            default:
                return false;
        }
    }

    // Class abilities need no reagents, tools or totems (Rune of Portals, Soul Shards, a Fire Totem...): a Hero has
    // every class's. Professions keep theirs (their spells have no class family). The client patch does the same
    // (client-patch/patch_spells.py, drop_class_reagents).
    static bool IsClassFamily(uint32 family)
    {
        return family != SPELLFAMILY_GENERIC && family != SPELLFAMILY_UNK1 && family != SPELLFAMILY_UNK2
            && family != SPELLFAMILY_POTION && family != SPELLFAMILY_PET;
    }

    static constexpr std::array<uint32, 9> THUNDER_CLAP = { 6343, 8198, 8204, 8205, 11580, 11581, 25264, 47501,
        47502 };
    static constexpr uint32 THUNDER_CLAP_RAGE = 100;            // in tenths
    // Sinister Strike (every rank a Hero learns) costs 30 Energy, not 45.
    static constexpr std::array<uint32, 12> SINISTER_STRIKE = { 1752, 1757, 1758, 1759, 1760, 8621, 11293, 11294,
        26861, 26862, 48637, 48638 };
    static constexpr uint32 SINISTER_STRIKE_ENERGY = 30;
    static constexpr uint32 HEARTHSTONE = 8690;
    static constexpr uint32 HEARTHSTONE_COOLDOWN_MS = 5 * MINUTE * IN_MILLISECONDS;

    // A Hero in Cat Form uses any class's Energy abilities (a Rogue's, a Monk's), in Bear or Dire Bear Form any class's
    // Rage abilities (a Warrior's, a Death Knight's, Runic Power being Rage): those the game keeps out of every form
    // ("not while shapeshifted") are allowed in that form too, and still out of it. The client patch does the same
    // (patch_spells.py, physical_forms).
    static void PhysicalForms(SpellInfo* spellInfo)
    {
        constexpr uint32 OWN_FIRST = 90000, OWN_END = 92000;            // Emberforge's own (the Monk's, the DH's...)
        bool own = spellInfo->Id >= OWN_FIRST && spellInfo->Id < OWN_END;
        if ((!IsClassFamily(spellInfo->SpellFamilyName) && !own) || spellInfo->SpellFamilyName == SPELLFAMILY_DRUID
            || spellInfo->IsPassive() || !spellInfo->HasAttribute(SPELL_ATTR0_NOT_SHAPESHIFTED))
            return;
        if (spellInfo->PowerType == POWER_ENERGY)
            spellInfo->Stances |= 1 << (FORM_CAT - 1);
        else if (spellInfo->PowerType == POWER_RAGE)
            spellInfo->Stances |= (1 << (FORM_BEAR - 1)) | (1 << (FORM_DIREBEAR - 1));
        else
            return;
        spellInfo->AttributesEx2 |= SPELL_ATTR2_ALLOW_WHILE_NOT_SHAPESHIFTED;
    }

    void OnLoadSpellCustomAttr(SpellInfo* spellInfo) override
    {
        if (IsClassFamily(spellInfo->SpellFamilyName))
        {
            spellInfo->Reagent.fill(0);
            spellInfo->ReagentCount.fill(0);
            spellInfo->Totem.fill(0);
            spellInfo->TotemCategory.fill(0);
        }

        // A Warrior's abilities need no stance (their passives keep theirs: a stance's own applies only in it), as
        // the client patch does (patch_spells.py, free_warrior_stances).
        if (spellInfo->SpellFamilyName == SPELLFAMILY_WARRIOR && !spellInfo->IsPassive())
            spellInfo->Stances = 0;

        // A buff cast at the caster's own pet (Bestial Wrath, Kill Command's...) is never downranked to the pet's
        // level: a Hero's pet may be far below the spell (titan_hero_low_level_buffs), and with no rank for its
        // level the core would assert (Spell::DoSpellHitOnUnit, GetAuraRankForLevel).
        for (SpellEffectInfo const& effect : spellInfo->Effects)
            if (effect.TargetA.GetTarget() == TARGET_UNIT_PET)
            {
                spellInfo->AttributesEx2 |= SPELL_ATTR2_ALLOW_LOW_LEVEL_BUFF;
                break;
            }

        if (spellInfo->PowerType == POWER_RUNIC_POWER)
            spellInfo->PowerType = POWER_RAGE;

        PhysicalForms(spellInfo);

        // Thunder Clap (every rank) costs 10 Rage, not 20; Sinister Strike 30 Energy, not 45; the Hearthstone cools
        // down in 5 minutes, not 30. The client patch does the same (patch_spells.py, hero_tuning).
        if (std::find(THUNDER_CLAP.begin(), THUNDER_CLAP.end(), spellInfo->Id) != THUNDER_CLAP.end())
            spellInfo->ManaCost = THUNDER_CLAP_RAGE;
        if (std::find(SINISTER_STRIKE.begin(), SINISTER_STRIKE.end(), spellInfo->Id) != SINISTER_STRIKE.end())
            spellInfo->ManaCost = SINISTER_STRIKE_ENERGY;
        if (spellInfo->Id == HEARTHSTONE)
            spellInfo->CategoryRecoveryTime = HEARTHSTONE_COOLDOWN_MS;

        // Mana costs are 35% of the game's (spells needing mana cost too much for a Hero of every class's spells),
        // rounded to the nearest; the client patch cuts its copy the same way (patch_spells.py, reduce_mana_costs).
        if (spellInfo->PowerType == POWER_MANA)
        {
            spellInfo->ManaCost = ManaShare(spellInfo->ManaCost);
            spellInfo->ManaCostPerlevel = ManaShare(spellInfo->ManaCostPerlevel);
            spellInfo->ManaCostPercentage = ManaShare(spellInfo->ManaCostPercentage);
        }
        for (SpellEffectInfo& effect : spellInfo->_GetEffects())
            if (effect.MiscValue == POWER_RUNIC_POWER && IsPowerEffect(effect))
                effect.MiscValue = POWER_RAGE;
    }
};

// Death Knight abilities come from level 1 for a Hero (wildcard.cpp, LEVEL_ONE_ABILITIES), but their numbers are a
// level-55 class's: Icy Touch hit a level-1 wolf for about 236. Below level 40 a Hero's Death Knight abilities with
// those numbers (Spell.dbc level 55 and up) deal a share of their damage: the spells (Icy Touch, Death Coil, Death and
// Decay's ticks) 15% at level 1, 25% at 10, 45% at 20, 70% at 30; the weapon strikes (Plague Strike, Blood Strike,
// Heart Strike...), whose weapon part is already the Hero's, 35%, 50%, 70%, 85%; all of it from 40. The diseases
// (Frost Fever, Blood Plague: level 0, their ticks from attack power) and the heals (a share of maximum health) stay
// whole.
class titan_dk_low_level : public UnitScript
{
public:
    titan_dk_low_level() : UnitScript("titan_dk_low_level", true, {
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK
    }) { }

    struct Point
    {
        uint8 Level;
        float Share;
    };

    using Curve = std::array<Point, 5>;
    static constexpr Curve SPELLS = { { { 1, 0.15f }, { 10, 0.25f }, { 20, 0.45f }, { 30, 0.7f }, { 40, 1.0f } } };
    static constexpr Curve STRIKES = { { { 1, 0.35f }, { 10, 0.5f }, { 20, 0.7f }, { 30, 0.85f }, { 40, 1.0f } } };
    // The level the Death Knight's own abilities start at: below it (the diseases, 0) nothing is scaled.
    static constexpr uint32 DEATH_KNIGHT_LEVEL = 55;

    static float Share(Curve const& curve, uint8 level)
    {
        if (level <= curve.front().Level)
            return curve.front().Share;
        for (std::size_t i = 1; i < curve.size(); ++i)
            if (level < curve[i].Level)
            {
                Point const& low = curve[i - 1];
                Point const& high = curve[i];
                return low.Share + (high.Share - low.Share) * (level - low.Level) / float(high.Level - low.Level);
            }
        return 1.0f;
    }

    static bool IsWeaponStrike(SpellInfo const* spellInfo)
    {
        return spellInfo->HasEffect(SPELL_EFFECT_WEAPON_DAMAGE)
            || spellInfo->HasEffect(SPELL_EFFECT_WEAPON_PERCENT_DAMAGE)
            || spellInfo->HasEffect(SPELL_EFFECT_NORMALIZED_WEAPON_DMG)
            || spellInfo->HasEffect(SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL);
    }

    // The share for a Hero's Death Knight spell, 1 for anything else.
    static float ShareFor(Unit* attacker, SpellInfo const* spellInfo)
    {
        if (!attacker || !spellInfo || spellInfo->SpellFamilyName != SPELLFAMILY_DEATHKNIGHT
            || spellInfo->SpellLevel < DEATH_KNIGHT_LEVEL)
            return 1.0f;
        Player* player = attacker->ToPlayer();
        if (!player || !hero_resources::IsHero(player))
            return 1.0f;
        return Share(IsWeaponStrike(spellInfo) ? STRIKES : SPELLS, player->GetLevel());
    }

    void ModifySpellDamageTaken(Unit* /*target*/, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        float share = ShareFor(attacker, spellInfo);
        if (share < 1.0f && damage > 0)
            damage = std::max<int32>(1, int32(damage * share));
    }

    void ModifyPeriodicDamageAurasTick(Unit* /*target*/, Unit* attacker, uint32& damage, SpellInfo const* spellInfo)
        override
    {
        // The core runs periodic heals through this hook too.
        if (spellInfo && (spellInfo->HasAura(SPELL_AURA_PERIODIC_HEAL)
            || spellInfo->HasAura(SPELL_AURA_OBS_MOD_HEALTH)))
            return;
        float share = ShareFor(attacker, spellInfo);
        if (share < 1.0f && damage > 0)
            damage = std::max<uint32>(1, uint32(damage * share));
    }
};

// A Hero's damage tuning by ability (players' reports): Soul Cleave 25% less, Blade Dance (its strikes) 15% less, every
// Feral Combat ability 87.5% more (25%, then 50% on top), every Rogue ability 30% more, Flamestrike 15% more (its
// blast and its burning ground). Direct damage and ticks; by first rank, so every rank.
class titan_hero_damage_tuning : public UnitScript
{
public:
    titan_hero_damage_tuning() : UnitScript("titan_hero_damage_tuning", true, {
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK
    }) { }

    static constexpr uint32 SOUL_CLEAVE = 90125;
    static constexpr std::array<uint32, 3> BLADE_DANCE = { 90102, 90107, 90108 };
    static constexpr float SOUL_CLEAVE_SHARE = 0.75f, BLADE_DANCE_SHARE = 0.85f;
    // Rising Sun Kick 20% less; Demon's Bite and Chaos Strike 25% less; Death and Decay (every rank, and the
    // damage its ground deals) 15% less.
    static constexpr uint32 RISING_SUN_KICK = 90322, DEMONS_BITE = 90100, CHAOS_STRIKE = 90101;
    static constexpr uint32 DEATH_AND_DECAY = 43265, DEATH_AND_DECAY_DAMAGE = 52212;
    static constexpr float RISING_SUN_KICK_SHARE = 0.8f, DEMON_HUNTER_STRIKE_SHARE = 0.75f;
    static constexpr float DEATH_AND_DECAY_SHARE = 0.85f;
    static constexpr float FERAL_SHARE = 1.25f * 1.5f, ROGUE_SHARE = 1.3f;
    static constexpr uint32 FLAMESTRIKE = 2120;
    static constexpr float FLAMESTRIKE_SHARE = 1.15f;

    static bool IsFeral(SpellInfo const* spellInfo)
    {
        if (spellInfo->SpellFamilyName != SPELLFAMILY_DRUID)
            return false;
        SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellInfo->Id);
        return std::any_of(bounds.first, bounds.second,
            [](auto const& pair) { return pair.second->SkillLine == SKILL_FERAL_COMBAT; });
    }

    static float Factor(Unit* attacker, SpellInfo const* spellInfo)
    {
        Player* player = attacker ? attacker->ToPlayer() : nullptr;
        if (!player || !spellInfo || !hero_resources::IsHero(player))
            return 1.0f;
        SpellInfo const* first = spellInfo->GetFirstRankSpell();
        if (first->Id == SOUL_CLEAVE)
            return SOUL_CLEAVE_SHARE;
        if (first->Id == RISING_SUN_KICK)
            return RISING_SUN_KICK_SHARE;
        if (first->Id == DEMONS_BITE || first->Id == CHAOS_STRIKE)
            return DEMON_HUNTER_STRIKE_SHARE;
        if (first->Id == DEATH_AND_DECAY || first->Id == DEATH_AND_DECAY_DAMAGE)
            return DEATH_AND_DECAY_SHARE;
        if (first->Id == FLAMESTRIKE)
            return FLAMESTRIKE_SHARE;
        if (std::find(BLADE_DANCE.begin(), BLADE_DANCE.end(), first->Id) != BLADE_DANCE.end())
            return BLADE_DANCE_SHARE;
        if (spellInfo->SpellFamilyName == SPELLFAMILY_ROGUE)
            return ROGUE_SHARE;
        if (IsFeral(first))
            return FERAL_SHARE;
        return 1.0f;
    }

    void ModifySpellDamageTaken(Unit* /*target*/, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        float factor = Factor(attacker, spellInfo);
        if (factor != 1.0f && damage > 0)
            damage = std::max<int32>(1, int32(damage * factor + 0.5f));
    }

    void ModifyPeriodicDamageAurasTick(Unit* /*target*/, Unit* attacker, uint32& damage, SpellInfo const* spellInfo)
        override
    {
        if (spellInfo && (spellInfo->HasAura(SPELL_AURA_PERIODIC_HEAL)
            || spellInfo->HasAura(SPELL_AURA_OBS_MOD_HEALTH)))
            return;
        float factor = Factor(attacker, spellInfo);
        if (factor != 1.0f && damage > 0)
            damage = std::max<uint32>(1, uint32(damage * factor + 0.5f));
    }
};

// A Hero's buff on their own pet or summon more than 10 levels below the spell (Bestial Wrath, Spell.dbc level 40, on
// a level-12 pet): the core drops the target and the cast fails with "Target is too low level". A Hero learns any
// spell at any level, so it lands on their own minions anyway (other players still get the core's rule).
class titan_hero_low_level_buffs : public AllSpellScript
{
public:
    titan_hero_low_level_buffs() : AllSpellScript("titan_hero_low_level_buffs",
        { ALLSPELLHOOK_ON_SCALE_AURA_UNIT_ADD }) { }

    void OnScaleAuraUnitAdd(Spell* spell, Unit* target, uint32 /*effectMask*/, bool /*checkIfValid*/,
        bool /*implicit*/, uint8 auraScaleMask, TargetInfo& targetInfo) override
    {
        if (!auraScaleMask || targetInfo.scaleAura || targetInfo.effectMask != auraScaleMask || !target)
            return;
        Unit* caster = spell->GetCaster() ? spell->GetCaster()->ToUnit() : nullptr;
        Player* player = caster ? caster->ToPlayer() : nullptr;
        // Only when the core finds a rank of it for the target's level (pet spells always: their attribute above);
        // with none, Spell::DoSpellHitOnUnit asserts and the world server stops.
        if (player && hero_resources::IsHero(player) && target->GetCharmerOrOwnerGUID() == player->GetGUID()
            && spell->GetSpellInfo()->GetAuraRankForLevel(target->GetLevel()))
            targetInfo.scaleAura = true;
    }
};

// A Hero's spell above their level: Wildcard gives many abilities' first rank from level 1 (wildcard.cpp,
// LEVEL_ONE_ABILITIES; talent abilities from their tier's level), with the numbers of the rank's own level: Greater
// Heal's first rank (a level-40 spell) healed a level-10 Hero's party for about 900. Below the spell's level its damage
// and healing are (level + 2) / (spell level + 2) of it (a class's spell power grows about so with level: Lesser Heal
// at 1, Heal at 16, Greater Heal at 40): a level-10 Hero's Greater Heal heals 29%, a level-30's 76%. A spell past the
// level cap (a Burning Crusade or Wrath rank: Shield of Righteousness, 75) counts as at the cap. Blizzard's class
// spells only (the runes' and items' are the Hero's own already); the Death Knight's have their own curve
// (titan_dk_low_level).
class titan_hero_low_level_spells : public UnitScript
{
public:
    titan_hero_low_level_spells() : UnitScript("titan_hero_low_level_spells", true, {
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
        UNITHOOK_MODIFY_HEAL_RECEIVED
    }) { }

    static constexpr uint32 FIRST_CUSTOM_SPELL = 90000;    // Emberforge's own spells start here
    static constexpr float LEVEL_OFFSET = 2.0f;

    static float Share(Unit* caster, SpellInfo const* spellInfo)
    {
        Player* player = caster ? caster->ToPlayer() : nullptr;
        if (!player || !spellInfo || spellInfo->Id >= FIRST_CUSTOM_SPELL || !hero_resources::IsHero(player))
            return 1.0f;
        if (spellInfo->SpellFamilyName == SPELLFAMILY_GENERIC || spellInfo->SpellFamilyName == SPELLFAMILY_DEATHKNIGHT
            || spellInfo->SpellFamilyName == SPELLFAMILY_POTION)
            return 1.0f;
        uint32 cap = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
        uint32 spellLevel = std::min(std::max(spellInfo->SpellLevel, spellInfo->BaseLevel), cap);
        uint32 level = player->GetLevel();
        if (spellLevel <= level)
            return 1.0f;
        return (float(level) + LEVEL_OFFSET) / (float(spellLevel) + LEVEL_OFFSET);
    }

    template <typename T>
    static void Apply(T& amount, float share)
    {
        if (share < 1.0f && amount > 0)
            amount = std::max<T>(1, T(float(amount) * share + 0.5f));
    }

    void ModifySpellDamageTaken(Unit* /*target*/, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        Apply(damage, Share(attacker, spellInfo));
    }

    void ModifyPeriodicDamageAurasTick(Unit* /*target*/, Unit* attacker, uint32& damage, SpellInfo const* spellInfo)
        override
    {
        // Heals over time come through here too, and again through ModifyHealReceived: scaled there only.
        if (spellInfo && (spellInfo->HasAura(SPELL_AURA_PERIODIC_HEAL)
            || spellInfo->HasAura(SPELL_AURA_OBS_MOD_HEALTH)))
            return;
        Apply(damage, Share(attacker, spellInfo));
    }

    // The heal's caster: the core passes (target, healer) for direct heals and heals over time alike (Unit.cpp,
    // HealBySpell, once passed them the other way round).
    void ModifyHealReceived(Unit* /*target*/, Unit* healer, uint32& heal, SpellInfo const* spellInfo) override
    {
        if (spellInfo && healer)
            Apply(heal, Share(healer, spellInfo));
    }
};

// A Hero's racials of a class the game keeps them from: Heroes are Paladins to the game (wildcard.h, HERO_CLASS), and
// no Orc nor Night Elf Paladin was ever meant to be, so Blood Fury (Warriors', Hunters', Rogues', Shamans', Warlocks'
// only) and Elusiveness never came. A Hero gets the one that suits every role (Blood Fury: the Shaman's, attack power
// and spell power) at login and on a level change.
class titan_hero_racials : public PlayerScript
{
public:
    titan_hero_racials() : PlayerScript("titan_hero_racials", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LEVEL_CHANGED
    }) { }

    struct Racial
    {
        uint8 Race;
        std::array<uint32, 3> Versions;                 // any of them known: nothing to do
        uint32 Teach;
    };

    static constexpr std::array<Racial, 2> RACIALS = { {
        { RACE_ORC, { 20572, 33697, 33702 }, 33697 },   // Blood Fury
        { RACE_NIGHTELF, { 21009, 0, 0 }, 21009 },      // Elusiveness
    } };

    static void Teach(Player* player)
    {
        if (!hero_resources::IsHero(player))
            return;
        for (Racial const& racial : RACIALS)
        {
            if (player->getRace() != racial.Race)
                continue;
            if (std::none_of(racial.Versions.begin(), racial.Versions.end(),
                    [player](uint32 spell) { return spell && player->HasSpell(spell); }))
                player->learnSpell(racial.Teach);
        }
    }

    void OnPlayerLogin(Player* player) override
    {
        Teach(player);
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        Teach(player);
    }
};

void AddSC_hero_resources()
{
    new hero_resources();
    new hero_emberforged();
    new titan_runic_power_to_rage();
    new hero_runes();
    new titan_dk_low_level();
    new titan_hero_damage_tuning();
    new titan_hero_low_level_buffs();
    new titan_hero_low_level_spells();
    new titan_hero_racials();
}
