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

#include "wildcard.h"
#include "global_talents.h"
#include "Chat.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "Trainer.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <array>
#include <span>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

// Wildcard: how the classless Hero progresses.
//
// - Starting kit (levels 1-7): KIT_SIZE random abilities. The Tome of Randomization rerolls them and
//   the Tome of Selection clears them for KIT_SIZE picks in the Classless window (item_titan_tomes.cpp).
//   At LOCK_LEVEL the kit locks: unspent picks are filled at random and both tomes vanish.
// - From FIRST_ROLL_LEVEL the level-up rolls alternate: a random talent rank at 8, a random ability
//   at 9, a talent at 10, an ability at 11, and so on. Abilities come from every class trainer's list
//   (all 10 classes); talents from any class's trees (tier and prerequisite rules do not apply). The
//   talent UI cannot spend points on a Hero.
// - The role (Melee, Ranged, Healer, Tank: Classless window) puts on a buff changing the threat caused, and steers
//   the rolls: abilities are tagged by what they do (melee or ranged physical damage, magic damage, heals, taunts and
//   threat; ROLE_ABILITIES by name), and only those suiting the role are rolled; untagged ones (buffs, utility,
//   crowd control) suit everyone. Tome of Selection picks are free.
// - Talent ranks go to talents for abilities the Hero knows: a talent modifying spells (its class masks) needs a
//   known ability it modifies; a scripted one (no masks, a dummy) a known ability of its class. When none is left,
//   generic ones: stat, damage, crit, healing, defense talents suiting the role, and talents teaching an ability
//   suiting it.
// - Talents' abilities (TALENT_ABILITIES) are abilities: rolled with them from their tier's level, never as talents.
//   Capstones (CAPSTONE_ABILITIES, CUSTOM_CAPSTONES) are rolled at CAPSTONE_LEVEL only: its roll is a capstone of
//   the Hero's role, not a talent rank.
// - Owned abilities upgrade to the best rank for the Hero's level by themselves.
// - An ability cast only in a form or stance (Swipe (Cat): Cat Form, Overpower: Battle Stance), only
//   in stealth (Ambush) or on a pet (Kill Command: Tame Beast) is only rolled or picked once the Hero
//   knows an ability giving that form, stealth or pet.
//
// Grants are caught up at login and on every level change, so a multi-level jump or a character
// that predates Wildcard gets everything it is owed. The number of abilities granted and the picks
// left are kept in the character's settings (EnablePlayerSettings = 1); talents are counted from the
// talents known. Spells of other classes' skill lines survive a relog only with
// ValidateSkillLearnedBySpells = 0.
namespace
{
    using namespace Wildcard;

    constexpr char const* SETTING_SOURCE = "mod-titan-wildcard";
    constexpr uint32 SETTING_ABILITIES_GRANTED = 0;
    constexpr uint32 SETTING_PICKS_LEFT = 1;
    constexpr uint32 SETTING_ROLE = 2;
    constexpr uint32 SETTING_CAPSTONE_GRANTED = 3;
    constexpr uint32 SETTING_REROLLS = 4;           // rerolls made: the blacklist's clock
    constexpr uint32 SETTING_COOLDOWN_STREAK = 5;   // ability rolls in a row that gave a cooldown
    constexpr uint32 BLACKLIST_REROLLS = 25;        // a name rerolled away is not rolled for this many rerolls

    // Each Hero's blacklist: name (English, first rank's) -> the reroll count it lasts until (titan_reroll_blacklist).
    std::unordered_map<ObjectGuid::LowType, std::unordered_map<std::string, uint32>> blacklists;

    std::string FirstRankName(uint32 spellId)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        return spellInfo && spellInfo->SpellName[0] ? spellInfo->GetFirstRankSpell()->SpellName[0] : "";
    }

    std::string EscapeName(std::string name)
    {
        CharacterDatabase.EscapeString(name);
        return name;
    }

    bool IsBlacklisted(Player* player, std::string const& name)
    {
        auto itr = blacklists.find(player->GetGUID().GetCounter());
        if (itr == blacklists.end())
            return false;
        auto entry = itr->second.find(name);
        return entry != itr->second.end()
            && player->GetPlayerSetting(SETTING_SOURCE, SETTING_REROLLS).value < entry->second;
    }

    // The roles' buffs, by Role (ROLE_NONE has none).
    constexpr std::array<uint32, MAX_ROLES> ROLE_SPELLS = { 0, 90603, 90604, 90605, 90606 };

    // Ability lists match spell names (English): every rank and faction version of a name counts. A
    // name that matches nothing is logged at startup.

    // Removed from the game for Heroes: never granted or listed, and unlearned at login from anyone
    // who has them (with a random replacement). Everything in the Runeforging skill line goes too.
    constexpr char const* BANNED_ABILITIES[] = {
        // Class mounts.
        "Summon Charger", "Charger", "Summon Warhorse", "Warhorse", "Summon Felsteed", "Felsteed",
        "Summon Dreadsteed", "Dreadsteed",
        // Tracking and scouting.
        "Track Beasts", "Track Humanoids", "Track Undead", "Track Hidden", "Track Elementals", "Track Demons",
        "Track Dragonkin", "Track Giants", "Sense Undead", "Sense Demons", "Eagle Eye", "Beast Lore",
        "Mind Vision", "Far Sight", "Eye of Kilrogg", "Sentry Totem",
        // Runeforging and its runes (also banned by skill line).
        "Runeforging", "Rune of Cinderglacier", "Rune of Razorice", "Rune of Spellbreaking",
        "Rune of Spellshattering", "Rune of Lichbane", "Rune of Swordbreaking", "Rune of Swordshattering",
        "Rune of the Fallen Crusader", "Rune of the Nerubian Carapace", "Rune of the Stoneskin Gargoyle",
        // Others.
        "Ritual of Refreshment", "Divine Intervention", "Lesser Heal", "Disarm Trap", "Create Firestone",
    };

    // Available from level 1 (their first rank; later ranks keep their trainer levels). This includes
    // passives and talent abilities, which are otherwise never offered.
    constexpr char const* LEVEL_ONE_ABILITIES[] = {
        // Death Knight
        "Blood Strike", "Blood Presence", "Blood Boil", "Dark Command", "Icy Touch", "Frost Presence",
        "Runic Focus", "Death and Decay", "Plague Strike", "Death Coil", "Unholy Presence", "Death Strike",
        // Druid
        "Entangling Roots", "Thorns", "Wrath", "Moonfire", "Nature's Grasp", "Teleport: Moonglade",
        "Demoralizing Roar", "Cat Form", "Swipe (Bear)", "Rip", "Claw", "Prowl", "Bear Form", "Growl", "Maul",
        "Feral Charge", "Swipe (Cat)", "Rejuvenation", "Mark of the Wild", "Healing Touch",
        // Hunter
        "Auto Shot", "Hunter's Mark", "Serpent Sting", "Arcane Shot", "Concussive Shot", "Steady Shot",
        "Mongoose Bite", "Raptor Strike", "Tame Beast", "Aspect of the Monkey", "Aspect of the Hawk",
        // Mage
        "Frostbolt", "Frost Nova", "Frost Armor", "Fireball", "Fire Blast", "Polymorph", "Conjure Food",
        "Arcane Intellect", "Arcane Missiles", "Conjure Water", "Mage Armor",
        // Paladin
        "Devotion Aura", "Divine Protection", "Hammer of Justice", "Hand of Protection", "Righteous Fury",
        "Shield of Righteousness", "Blessing of Might", "Judgement of Light", "Crusader Strike", "Lay on Hands",
        "Holy Light", "Purify", "Seal of Wisdom", "Seal of Righteousness", "Consecration",
        // Priest
        "Renew", "Smite", "Resurrection", "Greater Heal", "Power Word: Shield", "Power Word: Fortitude", "Fade",
        "Shadow Word: Pain", "Mind Blast",
        // Rogue
        "Backstab", "Sinister Strike", "Gouge", "Sprint", "Evasion", "Pick Pocket", "Stealth", "Sap", "Mutilate",
        "Eviscerate", "Slice and Dice",
        // Shaman
        "Lightning Bolt", "Earthbind Totem", "Searing Totem", "Stoneclaw Totem", "Earth Shock", "Flame Shock",
        "Lightning Shield", "Rockbiter Weapon", "Flametongue Weapon", "Stoneskin Totem", "Strength of Earth Totem",
        "Stormstrike", "Healing Wave",
        // Warlock
        "Corruption", "Curse of Weakness", "Drain Soul", "Life Tap", "Fear", "Immolate", "Shadow Bolt",
        "Demon Skin", "Summon Imp", "Summon Voidwalker", "Create Healthstone",
        // Warrior
        "Heroic Strike", "Charge", "Rend", "Hamstring", "Battle Stance", "Thunder Clap", "Battle Shout",
        "Victory Rush", "Defensive Stance", "Taunt", "Shield Block", "Bloodrage", "Sunder Armor",
    };

    // Names in LEVEL_ONE_ABILITIES another class's ability shares: only this class's comes at level 1 (the Warlock's
    // Death Coil keeps its level, 32).
    struct LevelOneClass
    {
        char const* Name;
        uint8 ClassId;
    };

    constexpr LevelOneClass LEVEL_ONE_CLASS_ONLY[] = {
        { "Death Coil", CLASS_DEATH_KNIGHT },
    };

    // Talents learned as abilities (by name; their talent's rank 1 spell, or what it teaches: Mangle's Cat and Bear
    // versions), never as talents: rolled with the abilities from their tier's level (row 1: 10, ... row 11: 60) and
    // lit on their talent tree in the Classless window when known.
    constexpr char const* TALENT_ABILITIES[] = {
        // Death Knight
        "Mark of Blood", "Rune Tap", "Hysteria", "Heart Strike", "Dancing Rune Weapon", "Lichborne", "Deathchill",
        "Hungering Cold", "Unbreakable Armor", "Frost Strike", "Howling Blast", "Corpse Explosion", "Ghoul Frenzy",
        "Anti-Magic Zone", "Bone Shield", "Scourge Strike", "Summon Gargoyle",
        // Druid
        "Insect Swarm", "Moonkin Form", "Typhoon", "Force of Nature", "Starfall", "Survival Instincts", "Feral Charge",
        "Mangle", "Berserk", "Nature's Swiftness", "Swiftmend", "Tree of Life", "Wild Growth",
        // Hunter
        "Intimidation", "Bestial Wrath", "Beast Mastery", "Aimed Shot", "Readiness", "Trueshot Aura", "Silencing Shot",
        "Chimera Shot", "Scatter Shot", "Counterattack", "Wyvern Sting", "Black Arrow", "Explosive Shot",
        // Mage
        "Focus Magic", "Presence of Mind", "Arcane Power", "Slow", "Arcane Barrage", "Pyroblast", "Blast Wave",
        "Combustion", "Dragon's Breath", "Living Bomb", "Icy Veins", "Cold Snap", "Ice Barrier",
        "Summon Water Elemental", "Deep Freeze",
        // Paladin
        "Aura Mastery", "Divine Favor", "Holy Shock", "Divine Illumination", "Beacon of Light", "Divine Sacrifice",
        "Blessing of Sanctuary", "Holy Shield", "Avenger's Shield", "Hammer of the Righteous", "Seal of Command",
        "Repentance", "Divine Storm",
        // Priest
        "Inner Focus", "Power Infusion", "Pain Suppression", "Penance", "Desperate Prayer", "Lightwell",
        "Circle of Healing", "Guardian Spirit", "Mind Flay", "Silence", "Shadowform", "Psychic Horror",
        "Vampiric Touch", "Dispersion",
        // Rogue
        "Cold Blood", "Hunger For Blood", "Riposte", "Blade Flurry", "Adrenaline Rush", "Killing Spree",
        "Ghostly Strike", "Preparation", "Hemorrhage", "Premeditation", "Shadowstep", "Shadow Dance",
        // Shaman
        "Elemental Mastery", "Totem of Wrath", "Thunderstorm", "Feral Spirit", "Riptide", "Earth Shield",
        "Tidal Force", "Mana Tide Totem", "Shamanistic Rage", "Lava Lash",
        // Warlock
        "Curse of Exhaustion", "Dark Pact", "Unstable Affliction", "Haunt", "Soul Link", "Fel Domination",
        "Demonic Empowerment", "Summon Felguard", "Metamorphosis", "Shadowburn", "Conflagrate", "Shadowfury",
        "Chaos Bolt",
        // Warrior
        "Sweeping Strikes", "Mortal Strike", "Bladestorm", "Piercing Howl", "Death Wish", "Bloodthirst", "Heroic Fury",
        "Titan's Grip", "Last Stand", "Concussion Blow", "Vigilance", "Devastate", "Shockwave",
    };

    // Capstones: rolled at CAPSTONE_LEVEL only, and that level's roll is one of them (not a talent rank), one of the
    // Hero's role's. By name (the class abilities; Metamorphosis is the warlock's), and our own classes' by spell id.
    constexpr uint8 CAP_TANK = RoleMask(ROLE_TANK), CAP_HEALER = RoleMask(ROLE_HEALER);
    constexpr uint8 CAP_RANGED = RoleMask(ROLE_RANGED), CAP_MELEE = RoleMask(ROLE_MELEE);
    struct CapstoneName
    {
        char const* Name;
        uint8 Roles;
    };
    constexpr CapstoneName CAPSTONE_ABILITIES[] = {
        // Tank (Howling Blast and Summon Gargoyle: Melee too)
        { "Dancing Rune Weapon", CAP_TANK }, { "Howling Blast", CAP_TANK | CAP_MELEE },
        { "Hammer of the Righteous", CAP_TANK }, { "Shockwave", CAP_TANK }, { "Summon Gargoyle", CAP_TANK | CAP_MELEE },
        // Healer
        { "Wild Growth", CAP_HEALER }, { "Beacon of Light", CAP_HEALER }, { "Penance", CAP_HEALER },
        { "Guardian Spirit", CAP_HEALER }, { "Riptide", CAP_HEALER },
        // Ranged
        { "Starfall", CAP_RANGED }, { "Beast Mastery", CAP_RANGED }, { "Chimera Shot", CAP_RANGED },
        { "Explosive Shot", CAP_RANGED }, { "Arcane Barrage", CAP_RANGED }, { "Living Bomb", CAP_RANGED },
        { "Deep Freeze", CAP_RANGED }, { "Dispersion", CAP_RANGED }, { "Thunderstorm", CAP_RANGED },
        { "Chaos Bolt", CAP_RANGED }, { "Metamorphosis", CAP_RANGED }, { "Haunt", CAP_RANGED },
        // Melee
        { "Berserk", CAP_MELEE }, { "Divine Storm", CAP_MELEE }, { "Hunger For Blood", CAP_MELEE },
        { "Killing Spree", CAP_MELEE }, { "Shadow Dance", CAP_MELEE }, { "Feral Spirit", CAP_MELEE },
        { "Bladestorm", CAP_MELEE }, { "Titan's Grip", CAP_MELEE },
    };
    struct CustomCapstone
    {
        uint32 SpellId;
        uint8 Roles;
    };
    constexpr CustomCapstone CUSTOM_CAPSTONES[] = {
        { 90135, CAP_TANK },        // Spirit Bomb (Demon Hunter, Vengeance)
        { 90311, CAP_HEALER },      // Invoke Chi-Ji, the Red Crane (Monk, Mistweaver)
        { 90121, CAP_MELEE },       // Fel Barrage (Demon Hunter, Havoc)
        { 90329, CAP_MELEE },       // Storm, Earth, and Fire (Monk, Windwalker)
    };

    // Learned together with Tame Beast (and removed with it), at level 1 as it is, so a tamed pet can be called back,
    // dismissed, revived, healed and fed. They are not offered on their own.
    constexpr char const* TAME_BEAST = "Tame Beast";
    constexpr char const* TAME_BEAST_COMPANIONS[] = { "Call Pet", "Dismiss Pet", "Revive Pet", "Mend Pet", "Feed Pet" };

    // Group versions of a buff: learned with the buff once the Hero's level reaches them (the rank-ups give them, as a
    // trainer would), never rolled on their own, and gone with it.
    struct GroupVersion
    {
        char const* Single;
        char const* Group;
    };
    constexpr GroupVersion GROUP_VERSIONS[] = {
        { "Arcane Intellect", "Arcane Brilliance" }, { "Dalaran Intellect", "Dalaran Brilliance" },
        { "Power Word: Fortitude", "Prayer of Fortitude" }, { "Divine Spirit", "Prayer of Spirit" },
        { "Shadow Protection", "Prayer of Shadow Protection" }, { "Mark of the Wild", "Gift of the Wild" },
        { "Blessing of Might", "Greater Blessing of Might" }, { "Blessing of Wisdom", "Greater Blessing of Wisdom" },
        { "Blessing of Kings", "Greater Blessing of Kings" },
        { "Blessing of Sanctuary", "Greater Blessing of Sanctuary" },
        { "Demon Skin", "Demon Armor" },        // its later form, from Demon Armor's level
    };

    // Abilities that need a pet without targeting it (those that do are found by their targets): the
    // abilities giving that pet.
    constexpr char const* HUNTER_PETS[] = { "Tame Beast" };
    constexpr char const* DEMONS[] = { "Summon Imp", "Summon Voidwalker", "Summon Succubus", "Summon Felhunter",
        "Summon Felguard" };
    constexpr char const* UNDEAD_MINIONS[] = { "Raise Dead" };
    struct NeedsPet
    {
        char const* Name;
        std::span<char const* const> Pets;
    };
    // Finishers whose scripts take the combo points (the Monk's, the Demon Hunter's: src/monk.cpp,
    // src/demon_hunter.cpp); the game's own say so in their spell (SpellInfo::NeedsComboPoints).
    constexpr char const* SCRIPTED_FINISHERS[] = {
        "Spinning Crane Kick", "Soul Cleave", "Spirit Bomb", "Soul Barrier",
    };
    // Not finishers though their spell says so: Overpower's point comes from the target dodging, not a builder.
    constexpr char const* NOT_FINISHERS[] = { "Overpower" };
    // Builders the core awards combo points for in code (SpellEffects.cpp), not by their spell's effect.
    constexpr char const* CODE_BUILDERS[] = { "Mangle (Cat)", "Hemorrhage" };

    constexpr NeedsPet PET_ABILITIES[] = {
        { "Kill Command", HUNTER_PETS }, { "Bestial Wrath", HUNTER_PETS }, { "Intimidation", HUNTER_PETS },
        { "Master's Call", HUNTER_PETS }, { "Eyes of the Beast", HUNTER_PETS },
        { "Health Funnel", DEMONS }, { "Demonic Empowerment", DEMONS }, { "Soul Link", DEMONS },
        { "Death Pact", UNDEAD_MINIONS }, { "Ghoul Frenzy", UNDEAD_MINIONS },
    };

    // Abilities that only work with another: rolled once the Hero knows one of its partners (as a finisher waits for a
    // combo point builder). By name, every rank and faction version.
    constexpr char const* IMMOLATES[] = { "Immolate", "Shadowflame" };
    constexpr char const* FROST_CHILLS[] = { "Frostbolt", "Frost Nova", "Cone of Cold", "Blizzard", "Frostfire Bolt" };
    constexpr char const* DISEASES[] = { "Icy Touch", "Plague Strike" };
    constexpr char const* HEALS_OVER_TIME[] = { "Rejuvenation", "Regrowth" };
    constexpr char const* FLAME_SHOCK[] = { "Flame Shock" };
    constexpr char const* POISONS[] = { "Deadly Poison" };
    constexpr char const* CORRUPTIONS[] = { "Corruption", "Unstable Affliction" };
    constexpr char const* SHADOW_WORD_PAIN[] = { "Shadow Word: Pain" };
    constexpr NeedsPet PAIRED_ABILITIES[] = {
        { "Conflagrate", IMMOLATES }, { "Ice Lance", FROST_CHILLS }, { "Deep Freeze", FROST_CHILLS },
        { "Obliterate", DISEASES }, { "Scourge Strike", DISEASES }, { "Blood Boil", DISEASES },
        { "Pestilence", DISEASES }, { "Heart Strike", DISEASES }, { "Swiftmend", HEALS_OVER_TIME },
        { "Lava Burst", FLAME_SHOCK }, { "Envenom", POISONS }, { "Haunt", CORRUPTIONS },
        { "Mind Flay", SHADOW_WORD_PAIN },
    };

    // A pet targeted by a class's ability: that class's pets.
    std::span<char const* const> ClassPets(uint8 classId)
    {
        switch (classId)
        {
            case CLASS_HUNTER: return HUNTER_PETS;
            case CLASS_WARLOCK: return DEMONS;
            case CLASS_DEATH_KNIGHT: return UNDEAD_MINIONS;
            default: return { };
        }
    }

    constexpr uint8 TAG_MELEE = RoleMask(ROLE_MELEE), TAG_RANGED = RoleMask(ROLE_RANGED);
    constexpr uint8 TAG_HEALER = RoleMask(ROLE_HEALER), TAG_TANK = RoleMask(ROLE_TANK);

    // Roles tagged by hand where the spell data says too little (forms and stances, scripted spells). They replace
    // what the data says.
    struct RoleAbility
    {
        char const* Name;
        uint8 Roles;
    };
    constexpr RoleAbility ROLE_ABILITIES[] = {
        { "Defensive Stance", TAG_TANK }, { "Shield Block", TAG_TANK }, { "Shield Wall", TAG_TANK },
        { "Last Stand", TAG_TANK }, { "Bear Form", TAG_TANK }, { "Dire Bear Form", TAG_TANK },
        { "Frost Presence", TAG_TANK }, { "Righteous Fury", TAG_TANK }, { "Holy Shield", TAG_TANK },
        { "Righteous Defense", TAG_TANK }, { "Demon Spikes", TAG_TANK }, { "Soul Barrier", TAG_TANK },
        { "Holy Shock", TAG_HEALER | TAG_RANGED }, { "Penance", TAG_HEALER | TAG_RANGED },
        { "Invoke Chi-Ji, the Red Crane", TAG_HEALER }, { "Thunder Focus Tea", TAG_HEALER },
        { "Battle Stance", TAG_MELEE | TAG_TANK }, { "Berserker Stance", TAG_MELEE },
        { "Cat Form", TAG_MELEE }, { "Moonkin Form", TAG_RANGED }, { "Tree of Life", TAG_HEALER },
        { "Shadowform", TAG_RANGED }, { "Conjure Food", TAG_RANGED }, { "Conjure Water", TAG_RANGED },
        { "Conjure Refreshment", TAG_RANGED },
    };

    // What a spell does, as role tags: its damage (melee or ranged physical: Melee; magic: Ranged), heals and
    // shields on others (Healer), taunts and threat (Tank); spells it triggers count too.
    uint8 SpellRoles(SpellInfo const* spellInfo, uint8 depth = 0)
    {
        uint8 roles = 0;
        bool physical = spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MELEE
            || spellInfo->DmgClass == SPELL_DAMAGE_CLASS_RANGED;
        uint8 damage = physical ? TAG_MELEE : TAG_RANGED;
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            bool onOthers = effect.TargetA.GetTarget() != TARGET_UNIT_CASTER;
            switch (effect.Effect)
            {
                case SPELL_EFFECT_SCHOOL_DAMAGE:
                case SPELL_EFFECT_HEALTH_LEECH:
                    roles |= damage;
                    break;
                case SPELL_EFFECT_WEAPON_DAMAGE:
                case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
                case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
                case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
                    roles |= TAG_MELEE;
                    break;
                case SPELL_EFFECT_HEAL:
                case SPELL_EFFECT_HEAL_PCT:
                case SPELL_EFFECT_HEAL_MAX_HEALTH:
                    if (onOthers)
                        roles |= TAG_HEALER;
                    break;
                case SPELL_EFFECT_ATTACK_ME:
                case SPELL_EFFECT_THREAT:
                    roles |= TAG_TANK;
                    break;
                case SPELL_EFFECT_TRIGGER_SPELL:
                    if (depth < 2)
                        if (SpellInfo const* triggered = sSpellMgr->GetSpellInfo(effect.TriggerSpell))
                            roles |= SpellRoles(triggered, depth + 1);
                    break;
                case SPELL_EFFECT_APPLY_AURA:
                    switch (effect.ApplyAuraName)
                    {
                        case SPELL_AURA_PERIODIC_DAMAGE:
                        case SPELL_AURA_PERIODIC_LEECH:
                        case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
                            roles |= damage;
                            break;
                        case SPELL_AURA_PERIODIC_HEAL:
                        case SPELL_AURA_SCHOOL_ABSORB:
                            if (onOthers)
                                roles |= TAG_HEALER;
                            break;
                        case SPELL_AURA_MOD_TAUNT:
                            roles |= TAG_TANK;
                            break;
                        case SPELL_AURA_MOD_THREAT:
                            if (effect.BasePoints > 0)
                                roles |= TAG_TANK;
                            break;
                        case SPELL_AURA_PERIODIC_TRIGGER_SPELL:
                            if (depth < 2)
                                if (SpellInfo const* triggered = sSpellMgr->GetSpellInfo(effect.TriggerSpell))
                                    roles |= SpellRoles(triggered, depth + 1);
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
        }
        return roles;
    }

    // A talent whose words reach every spell of a kind (global_talents.h): -1 when it is not one, else whether its
    // modifiers reach checkSpell (any class's: titan_global_talents).
    int GlobalReach(uint32 talentSpellId, SpellInfo const* checkSpell)
    {
        static std::unordered_map<uint32, GlobalTalents::Entry> const entries = []
        {
            std::unordered_map<uint32, GlobalTalents::Entry> map;
            for (GlobalTalents::Entry const& entry : GlobalTalents::ENTRIES)
                map[entry.SpellId] = entry;
            return map;
        }();
        auto itr = entries.find(talentSpellId);
        if (itr == entries.end())
            return -1;
        if (checkSpell->IsPassive())
            return 0;

        bool heals = checkSpell->HasEffect(SPELL_EFFECT_HEAL) || checkSpell->HasEffect(SPELL_EFFECT_HEAL_PCT)
            || checkSpell->HasAura(SPELL_AURA_PERIODIC_HEAL);
        bool damages = checkSpell->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE)
            || checkSpell->HasAura(SPELL_AURA_PERIODIC_DAMAGE) || checkSpell->HasAura(SPELL_AURA_PERIODIC_LEECH)
            || checkSpell->HasEffect(SPELL_EFFECT_HEALTH_LEECH);
        switch (itr->second.Kind)
        {
            case GlobalTalents::HEALING:
                return heals ? 1 : 0;
            case GlobalTalents::SPELLS:
                return checkSpell->DmgClass == SPELL_DAMAGE_CLASS_MAGIC && (heals || damages) ? 1 : 0;
            case GlobalTalents::SCHOOL:
                return damages && (checkSpell->SchoolMask & itr->second.School) ? 1 : 0;
        }
        return 0;
    }

    // A generic talent's role tags, from the stats it raises (0: any role).
    uint8 StatTalentRoles(SpellInfo const* spellInfo)
    {
        uint8 roles = 0;
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            if (!effect.IsEffect(SPELL_EFFECT_APPLY_AURA))
                continue;
            switch (effect.ApplyAuraName)
            {
                case SPELL_AURA_MOD_DAMAGE_DONE:
                case SPELL_AURA_MOD_DAMAGE_PERCENT_DONE:
                    if (effect.MiscValue & SPELL_SCHOOL_MASK_NORMAL)
                        roles |= TAG_MELEE | TAG_TANK;
                    if (effect.MiscValue & SPELL_SCHOOL_MASK_MAGIC)
                        roles |= TAG_RANGED | TAG_HEALER;
                    break;
                case SPELL_AURA_MOD_HEALING_PCT:
                case SPELL_AURA_MOD_HEALING_DONE:
                case SPELL_AURA_MOD_HEALING_DONE_PERCENT:
                case SPELL_AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT:
                    roles |= TAG_HEALER;
                    break;
                case SPELL_AURA_MOD_SPELL_CRIT_CHANCE:
                case SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL:
                case SPELL_AURA_MOD_SPELL_HIT_CHANCE:
                case SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT:
                case SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK:
                    roles |= TAG_RANGED | TAG_HEALER;
                    break;
                case SPELL_AURA_MOD_WEAPON_CRIT_PERCENT:
                case SPELL_AURA_MOD_HIT_CHANCE:
                case SPELL_AURA_MOD_ATTACK_POWER_PCT:
                case SPELL_AURA_MOD_RANGED_ATTACK_POWER_PCT:
                case SPELL_AURA_MOD_MELEE_HASTE:
                case SPELL_AURA_MOD_EXPERTISE:
                case SPELL_AURA_MOD_ATTACK_POWER_OF_STAT_PERCENT:
                case SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT:
                    roles |= TAG_MELEE | TAG_TANK;
                    break;
                case SPELL_AURA_MOD_RESISTANCE_PCT:
                case SPELL_AURA_MOD_BASE_RESISTANCE_PCT:
                case SPELL_AURA_MOD_DODGE_PERCENT:
                case SPELL_AURA_MOD_PARRY_PERCENT:
                case SPELL_AURA_MOD_BLOCK_PERCENT:
                case SPELL_AURA_MOD_SHIELD_BLOCKVALUE_PCT:
                case SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT:
                case SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN:
                    roles |= TAG_TANK;
                    break;
                case SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE:
                case SPELL_AURA_MOD_PERCENT_STAT:
                    switch (effect.MiscValue)
                    {
                        case STAT_STRENGTH: roles |= TAG_MELEE | TAG_TANK; break;
                        case STAT_AGILITY: roles |= TAG_MELEE; break;
                        case STAT_STAMINA: roles |= TAG_TANK; break;
                        case STAT_INTELLECT: roles |= TAG_RANGED | TAG_HEALER; break;
                        case STAT_SPIRIT: roles |= TAG_HEALER; break;
                        default: break;
                    }
                    break;
                default:
                    break;
            }
        }
        return roles;
    }

    // Every player always knows Pick Lock, with Lockpicking at its maximum for their level.
    constexpr uint32 SPELL_PICK_LOCK = 1804;
    constexpr uint32 SPELL_CALL_PET = 883;

    // Emberforge's own abilities (no trainer teaches them): the spells are in spell_dbc
    // (data/sql/db-world/titan_demon_hunter_spells.sql) and the client patch (patch_spells.py). Their levels follow
    // their power, over the whole 1-60 the way the classes' own do: the builders and spenders from 1, the major
    // cooldowns late (Metamorphosis and Revival at 50, as the classes' big talent abilities), the capstones at 60.
    struct CustomAbility
    {
        uint32 SpellId;
        uint8 ClassId;
        uint32 SkillLine;
        uint8 Level;
    };
    constexpr CustomAbility CUSTOM_ABILITIES[] = {
        { 90100, CLASS_DEMON_HUNTER, SKILL_HAVOC, 1 },      // Demon's Bite
        { 90101, CLASS_DEMON_HUNTER, SKILL_HAVOC, 1 },      // Chaos Strike
        { 90103, CLASS_DEMON_HUNTER, SKILL_HAVOC, 1 },      // Fel Rush
        { 90111, CLASS_DEMON_HUNTER, SKILL_HAVOC, 6 },      // Throw Glaive
        { 90102, CLASS_DEMON_HUNTER, SKILL_HAVOC, 16 },     // Blade Dance
        { 90114, CLASS_DEMON_HUNTER, SKILL_HAVOC, 20 },     // Consume Magic
        { 90109, CLASS_DEMON_HUNTER, SKILL_HAVOC, 22 },     // Vengeful Retreat
        { 90113, CLASS_DEMON_HUNTER, SKILL_HAVOC, 28 },     // Blur
        { 90112, CLASS_DEMON_HUNTER, SKILL_HAVOC, 30 },     // Chaos Nova
        { 90105, CLASS_DEMON_HUNTER, SKILL_HAVOC, 34 },     // Eye Beam
        { 90116, CLASS_DEMON_HUNTER, SKILL_HAVOC, 50 },     // Metamorphosis
        { 90121, CLASS_DEMON_HUNTER, SKILL_HAVOC, 60 },     // Fel Barrage
        { 90119, CLASS_DEMON_HUNTER, SKILL_VENGEANCE, 1 },  // Immolation Aura
        { 90124, CLASS_DEMON_HUNTER, SKILL_VENGEANCE, 1 },  // Shear
        { 90125, CLASS_DEMON_HUNTER, SKILL_VENGEANCE, 1 },  // Soul Cleave
        { 90129, CLASS_DEMON_HUNTER, SKILL_VENGEANCE, 8 },  // Torment
        { 90127, CLASS_DEMON_HUNTER, SKILL_VENGEANCE, 14 }, // Demon Spikes
        { 90117, CLASS_DEMON_HUNTER, SKILL_VENGEANCE, 24 }, // Felblade
        { 90132, CLASS_DEMON_HUNTER, SKILL_VENGEANCE, 32 }, // Sigil of Chains
        { 90128, CLASS_DEMON_HUNTER, SKILL_VENGEANCE, 36 }, // Fiery Brand
        { 90138, CLASS_DEMON_HUNTER, SKILL_VENGEANCE, 44 }, // Soul Barrier
        { 90118, CLASS_DEMON_HUNTER, SKILL_VENGEANCE, 48 }, // Darkness
        { 90135, CLASS_DEMON_HUNTER, SKILL_VENGEANCE, 60 }, // Spirit Bomb
        { 90300, CLASS_MONK, SKILL_MISTWEAVER, 1 },         // Soothing Mist
        { 90301, CLASS_MONK, SKILL_MISTWEAVER, 1 },         // Vivify
        { 90302, CLASS_MONK, SKILL_MISTWEAVER, 6 },         // Renewing Mist
        { 90309, CLASS_MONK, SKILL_MISTWEAVER, 12 },        // Detox
        { 90303, CLASS_MONK, SKILL_MISTWEAVER, 18 },        // Enveloping Mist
        { 90308, CLASS_MONK, SKILL_MISTWEAVER, 26 },        // Thunder Focus Tea
        { 90306, CLASS_MONK, SKILL_MISTWEAVER, 32 },        // Life Cocoon
        { 90304, CLASS_MONK, SKILL_MISTWEAVER, 38 },        // Essence Font
        { 90313, CLASS_MONK, SKILL_MISTWEAVER, 42 },        // Refreshing Jade Wind
        { 90307, CLASS_MONK, SKILL_MISTWEAVER, 50 },        // Revival
        { 90311, CLASS_MONK, SKILL_MISTWEAVER, 60 },        // Invoke Chi-Ji, the Red Crane (the capstone)
        { 90320, CLASS_MONK, SKILL_WINDWALKER, 1 },         // Tiger Palm
        { 90321, CLASS_MONK, SKILL_WINDWALKER, 1 },         // Blackout Kick
        { 90325, CLASS_MONK, SKILL_WINDWALKER, 1 },         // Spinning Crane Kick
        { 90322, CLASS_MONK, SKILL_WINDWALKER, 16 },        // Rising Sun Kick
        { 90323, CLASS_MONK, SKILL_WINDWALKER, 30 },        // Fists of Fury
        { 90327, CLASS_MONK, SKILL_WINDWALKER, 44 },        // Whirling Dragon Punch
        { 90329, CLASS_MONK, SKILL_WINDWALKER, 60 },        // Storm, Earth, and Fire (the capstone)
    };
    constexpr std::pair<uint32, char const*> CUSTOM_SKILL_LINES[] = {
        { SKILL_HAVOC, "Havoc" },
        { SKILL_VENGEANCE, "Vengeance" },
        { SKILL_BREWMASTER, "Brewmaster" },
        { SKILL_MISTWEAVER, "Mistweaver" },
        { SKILL_WINDWALKER, "Windwalker" },
    };

    // The spells a trainer entry teaches: itself, or what it learns when it is a wrapper.
    std::vector<uint32> TaughtSpells(uint32 trainerSpellId)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(trainerSpellId);
        if (!spellInfo)
            return { };

        if (!spellInfo->HasEffect(SPELL_EFFECT_LEARN_SPELL))
            return { trainerSpellId };

        std::vector<uint32> taught;
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
            if (effect.IsEffect(SPELL_EFFECT_LEARN_SPELL) && effect.TriggerSpell)
                taught.push_back(effect.TriggerSpell);

        return taught;
    }

    std::string_view SpellName(uint32 spellId)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        char const* name = spellInfo ? spellInfo->SpellName[LOCALE_enUS] : nullptr;
        return name ? name : "";
    }

    // The class skill line (spec) a spell belongs to, 0 if none.
    uint32 ClassSkillLine(uint32 spellId)
    {
        SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
            if (SkillLineEntry const* skill = sSkillLineStore.LookupEntry(itr->second->SkillLine))
                if (skill->categoryId == SKILL_CATEGORY_CLASS)
                    return skill->id;

        return 0;
    }

    // Spells limited to some races (faction versions such as Heroism / Bloodlust) by their skill line: a Hero takes
    // their faction's (Bloodlust is the Orc's, Tauren's and Troll's, Heroism the Draenei's: an Undead or Human Hero
    // has their faction's too).
    bool FitsRace(Player const* player, uint32 spellId)
    {
        SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
        if (bounds.first == bounds.second)
            return true;

        uint32 factionMask = 0;
        for (uint8 race = RACE_HUMAN; race <= RACE_DRAENEI; ++race)
            if (sChrRacesStore.LookupEntry(race) && Player::TeamIdForRace(race) == player->GetTeamId(true))
                factionMask |= 1 << (race - 1);

        for (auto itr = bounds.first; itr != bounds.second; ++itr)
            if (!itr->second->RaceMask || (itr->second->RaceMask & factionMask))
                return true;

        return false;
    }

    // An ability first learned past level 60 (the Burning Crusade's and Wrath's: Obliterate at 61, Mirror Image at
    // 80), or at 60 (whose roll is a capstone), comes earlier: 61-80 to 41-59 (40 + its level past 60), 60 to 59.
    // Its later ranks keep their levels (past 60: never learned, the game ending on Classic's power).
    uint8 Reachable(uint8 level)
    {
        if (level > CAPSTONE_LEVEL)
            level = uint8(40 + (level - CAPSTONE_LEVEL));
        return std::min<uint8>(level, CAPSTONE_LEVEL - 1);
    }

    // Index of the best rank available at the player's level, or -1.
    int32 BestRank(Player const* player, Ability const& ability)
    {
        int32 best = -1;
        for (int32 i = 0; i < int32(ability.Ranks.size()); ++i)
            if (ability.Ranks[i].Level <= player->GetLevel())
                best = i;

        return best;
    }

    // Set while a specialization is put back (specs.cpp): no chat line for each ability and talent.
    bool quiet = false;
    // A talent the next talent roll may not give (a reroll's: it gives another).
    uint32 excludedTalent = 0;

    void Announce(Player* player, uint32 spellId, char const* what)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo || quiet)
            return;

        char const* name = spellInfo->SpellName[player->GetSession()->GetSessionDbcLocale()];
        ChatHandler(player->GetSession()).PSendSysMessage("|cffff8000Wildcard:|r {} |cff71d5ff|Hspell:{}|h[{}]|h|r",
            what, spellId, name ? name : "?");
    }

    // Every ability the Hero knows: every rank its level allows, learned (as a trainer teaches them, each one), from
    // the lowest one missing; nothing pending is left. Run after every level, login and learn.
    void UpgradeRanks(Player* player)
    {
        for (Ability const& ability : Abilities())
        {
            if (KnownRank(player, ability) < 0)
                continue;
            int32 best = BestRank(player, ability);
            for (int32 i = 0; i <= best; ++i)
                if (!player->HasSpell(ability.Ranks[i].SpellId))
                    player->learnSpell(ability.Ranks[i].SpellId);
        }
        // Companions the Hero's level now reaches (a group buff: Arcane Brilliance with Arcane Intellect).
        for (Ability const& ability : Abilities())
            if (ability.Offered && KnownRank(player, ability) >= 0)
                for (uint32 companionId : ability.Companions)
                    if (Ability const* companion = FindAbility(companionId))
                        if (int32 best = BestRank(player, *companion); best >= 0 && KnownRank(player, *companion) < 0)
                        {
                            for (int32 i = 0; i <= best; ++i)
                                player->learnSpell(companion->Ranks[i].SpellId);
                            Announce(player, companion->Ranks[best].SpellId, "came with your ability:");
                        }
    }

    // The talent points a Hero has spent, counted the way the core counts them (Player::addTalent at loading: every
    // talent row of the active specialization, its rank + 1). Wildcard's own grants keep the core's count equal to it
    // (SyncTalentCount), and CalculateTalentsPoints returns it: the core's "more talents than allowed" reset
    // (InitTalentForLevel) never fires for a Hero. It fired on a reroll into a talent of fewer ranks and at login,
    // wiping every talent.
    uint32 SpentTalentPoints(Player const* player)
    {
        uint32 spent = 0;
        for (auto const& [spellId, talent] : player->GetTalentMap())
            if (talent->State != PLAYERSPELL_REMOVED && talent->IsInSpec(player->GetActiveSpec()))
                if (TalentSpellPos const* talentPos = GetTalentSpellPos(spellId))
                    spent += talentPos->rank + 1;

        return spent;
    }

    void SyncTalentCount(Player* player)
    {
        player->SetUsedTalentCount(SpentTalentPoints(player));
    }

    // The talents rolled (each roll gives one, at its top rank): a Hero's talents, but those learned as abilities.
    uint32 RolledTalents(Player const* player)
    {
        uint32 rolled = 0;
        for (auto const& [spellId, talent] : player->GetTalentMap())
            if (talent->State != PLAYERSPELL_REMOVED && talent->IsInSpec(player->GetActiveSpec()))
                if (TalentSpellPos const* talentPos = GetTalentSpellPos(spellId))
                    if (!TalentAbility(talentPos->talent_id))
                        ++rolled;

        return rolled;
    }

    // A talent's top rank (index into RankID).
    uint8 TopRank(TalentEntry const* talentInfo)
    {
        uint8 top = 0;
        for (uint8 rank = 0; rank < MAX_TALENT_RANK; ++rank)
            if (talentInfo->RankID[rank])
                top = rank;
        return top;
    }

    // The talent it builds on (Eternal Reaping on Chaos Cleave), at the rank it needs, is known.
    bool HasPrerequisite(Player const* player, TalentEntry const* talentInfo)
    {
        if (!talentInfo->DependsOn)
            return true;
        TalentEntry const* required = sTalentStore.LookupEntry(talentInfo->DependsOn);
        return required && TalentRank(player, required) > talentInfo->DependsOnRank;
    }

    // The Paladin's own skill lines (Holy, Protection, Retribution) teach their spells at every login to whoever has
    // the skill (Player::learnSkillRewardedSpells: Holy Light and Seal of Righteousness, as temporary spells); a Hero
    // created before hero_cleanup.sql kept them, and Wildcard then counted those spells as known and ranked them up.
    // Only the temporary ones go: a Hero's own, rolled Holy Light stays.
    void DropSkillLineSpells(Player* player)
    {
        static constexpr std::array<uint32, 3> PALADIN_SKILLS = { 184, 267, 594 };
        std::vector<uint32> temporary;
        for (auto const& [spellId, spell] : player->GetSpellMap())
        {
            if (spell->State != PLAYERSPELL_TEMPORARY)
                continue;
            auto bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
            for (auto itr = bounds.first; itr != bounds.second; ++itr)
                if (std::find(PALADIN_SKILLS.begin(), PALADIN_SKILLS.end(), itr->second->SkillLine)
                    != PALADIN_SKILLS.end())
                {
                    temporary.push_back(spellId);
                    break;
                }
        }
        for (uint32 spellId : temporary)
            player->removeSpell(spellId, SPEC_MASK_ALL, true);
    }

    // A talent known at two ranks (an older rank left by the rank-up before this fix): only the higher stays.
    void DropStaleRanks(Player* player)
    {
        for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
            if (TalentEntry const* talentInfo = sTalentStore.LookupEntry(i))
                if (uint8 known = TalentRank(player, talentInfo); known > 1)
                    for (uint8 rank = 0; rank + 1 < known; ++rank)
                    {
                        uint32 spellId = talentInfo->RankID[rank];
                        if (spellId && player->HasTalent(spellId, player->GetActiveSpec()))
                            player->_removeTalent(spellId, player->GetActiveSpecMask());
                    }
        SyncTalentCount(player);
        player->InitTalentForLevel();
    }

    // A talent learned at a lower rank (rolled before rolls gave the top one) goes to its top rank.
    bool UpgradeTalents(Player* player)
    {
        bool upgraded = false;
        for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
        {
            TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
            if (!talentInfo || TalentAbility(talentInfo->TalentID))
                continue;
            uint8 rank = TalentRank(player, talentInfo);
            uint8 top = TopRank(talentInfo);
            if (rank && rank <= top)
                upgraded |= LearnTalentRank(player, talentInfo, top, false);
        }
        return upgraded;
    }

    // The Demon Hunter's scripted talents (a dummy aura: no spell class mask names their ability), each with the
    // abilities it improves, so it counts as linked to them, not to every Demon Hunter ability.
    struct TalentFor
    {
        char const* Talent;
        std::array<char const*, 2> Abilities;
    };
    constexpr TalentFor SCRIPTED_TALENTS[] = {
        // Havoc
        { "Blind Fury", { "Eye Beam" } }, { "Demonic Appetite", { "Chaos Strike" } },
        { "Prepared", { "Vengeful Retreat" } }, { "Bloodlet", { "Throw Glaive" } },
        { "Desperate Instincts", { "Blur" } }, { "Momentum", { "Fel Rush", "Vengeful Retreat" } },
        { "Chaos Cleave", { "Chaos Strike" } }, { "Cycle of Hatred", { "Chaos Strike", "Eye Beam" } },
        { "Trail of Ruin", { "Blade Dance" } }, { "Unbound Chaos", { "Immolation Aura", "Fel Rush" } },
        { "Demonic", { "Eye Beam" } }, { "Eternal Reaping", { "Chaos Strike" } }, { "Inertia", { "Fel Rush" } },
        // Vengeance
        { "Shear Fury", { "Shear" } }, { "Infernal Armor", { "Immolation Aura" } },
        { "Razor Spikes", { "Demon Spikes" } }, { "Painbringer", { "Soul Cleave", "Spirit Bomb" } },
        { "Concentrated Sigils", { "Sigil of Chains" } }, { "Charred Flesh", { "Fiery Brand" } },
        { "Fallout", { "Immolation Aura" } }, { "Cover of Darkness", { "Darkness" } },
        { "Burning Alive", { "Fiery Brand" } }, { "Feed the Demon", { "Demon Spikes" } },
        { "Feast of Souls", { "Soul Cleave" } }, { "Fiery Demise", { "Fiery Brand" } },
        { "Ruinous Bulwark", { "Soul Barrier" } }, { "Void Reaver", { "Soul Cleave" } },
        { "Vulnerability", { "Spirit Bomb" } }, { "Fel Flame Fortification", { "Immolation Aura" } },
    };

    // How much likelier a talent improving an ability the Hero knows is than one only for a class they have an
    // ability of (a scripted talent of another kind): reports had the Demon Hunter's barely ever rolling, lost
    // among every class's.
    constexpr uint32 SPECIFIC_WEIGHT = 4;

    // Learns a random talent from any class tree at its top rank, the way Player::LearnTalent does once its checks
    // pass; with reveal the player's Classless window spins for it. A talent another builds on comes first.
    bool GrantTalent(Player* player, bool reveal, uint8 rollLevel)
    {
        // The spells of the abilities the Hero knows (their known ranks), and their classes.
        std::vector<SpellInfo const*> knownSpells;
        std::unordered_set<std::string> knownNames;
        uint32 knownClasses = 0;
        for (Ability const& ability : Abilities())
            if (int32 rank = KnownRank(player, ability); rank >= 0)
            {
                knownSpells.push_back(sSpellMgr->AssertSpellInfo(ability.Ranks[rank].SpellId));
                knownNames.insert(FirstRankName(ability.Ranks.front().SpellId));
                if (ability.ClassId)
                    knownClasses |= 1 << (ability.ClassId - 1);
            }

        // Relevant: for a known ability (3: that ability's own; 2: a scripted talent of a class the Hero has an
        // ability of). Generic (1): stats and the like suiting the role, or teaching an ability that does.
        auto relevance = [&](TalentEntry const* talentInfo, TalentTabEntry const* tabInfo) -> int
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(talentInfo->RankID[0]);
            if (!spellInfo || TalentAbility(talentInfo->TalentID))
                return 0;

            std::string const name = FirstRankName(talentInfo->RankID[0]);
            for (TalentFor const& scripted : SCRIPTED_TALENTS)
                if (name == scripted.Talent)
                {
                    for (char const* ability : scripted.Abilities)
                        if (ability && knownNames.count(ability))
                            return 3;
                    return 0;
                }

            // A talent for every spell of a kind ("all means all"): for a Hero knowing any of that kind.
            if (GlobalReach(spellInfo->Id, spellInfo) >= 0)
            {
                for (SpellInfo const* known : knownSpells)
                    if (GlobalReach(spellInfo->Id, known) == 1)
                        return 3;
                return 0;
            }

            bool masked = false;
            for (SpellEffectInfo const& effect : spellInfo->GetEffects())
            {
                // Teaching an ability: never when it is a Wildcard ability (Mutilate, Stormstrike...): those come
                // as abilities, not talents.
                if (effect.IsEffect(SPELL_EFFECT_LEARN_SPELL))
                {
                    SpellInfo const* taught = sSpellMgr->GetSpellInfo(effect.TriggerSpell);
                    Ability const* ability = taught ? FindAbility(taught->GetFirstRankSpell()->Id) : nullptr;
                    if (!taught || (ability && ability->Offered))
                        return 0;
                    return SuitsRole(player, SpellRoles(taught)) ? 1 : 0;
                }
                if (!effect.SpellClassMask)
                    continue;
                masked = true;
                for (SpellInfo const* known : knownSpells)
                    if (known->IsAffected(spellInfo->SpellFamilyName, effect.SpellClassMask))
                        return 3;
            }
            if (masked)
                return 0;

            if (uint8 roles = StatTalentRoles(spellInfo))
                return SuitsRole(player, roles) ? 1 : 0;

            // Scripted (a dummy): for its class's abilities.
            return (tabInfo->ClassMask & knownClasses) ? 2 : 0;
        };

        std::vector<std::pair<TalentEntry const*, uint8>> candidates, generic;
        std::vector<uint32> weights;                    // candidates': SPECIFIC_WEIGHT for an ability's own
        for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
        {
            TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
            if (!talentInfo || talentInfo->TalentID == excludedTalent)
                continue;

            TalentTabEntry const* tabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
            if (!tabInfo || tabInfo->petTalentMask || !(tabInfo->ClassMask & HERO_TALENT_CLASSMASK))
                continue;

            if (TalentRank(player, talentInfo) || !HasPrerequisite(player, talentInfo)
                || IsBlacklisted(player, FirstRankName(talentInfo->RankID[0])))
                continue;

            uint8 top = TopRank(talentInfo);
            int kind = relevance(talentInfo, tabInfo);
            if (kind >= 2)
            {
                candidates.emplace_back(talentInfo, top);
                weights.push_back(kind == 3 ? SPECIFIC_WEIGHT : 1);
            }
            else if (kind == 1)
                generic.emplace_back(talentInfo, top);
        }

        // About 70% of the rolls improve an ability the Hero knows (one for an ability's own SPECIFIC_WEIGHT times as
        // likely as a scripted talent only for its class), the rest are talents for everyone that suit the role
        // (stats, all damage or healing, defense...); either pool stands in when the other is empty.
        constexpr uint32 LINKED_PCT = 70;
        bool linked = !candidates.empty() && (generic.empty() || urand(1, 100) <= LINKED_PCT);
        std::vector<std::pair<TalentEntry const*, uint8>>& pool = linked ? candidates : generic;
        if (pool.empty())
            return false;

        std::size_t pick = urand(0, uint32(pool.size() - 1));
        if (linked)
        {
            uint32 total = 0;
            for (uint32 weight : weights)
                total += weight;
            uint32 roll = urand(1, total);
            for (pick = 0; pick + 1 < weights.size() && roll > weights[pick]; ++pick)
                roll -= weights[pick];
        }
        auto [talentInfo, currentRank] = pool[pick];
        LOG_DEBUG("module", "Wildcard: {} rolled talent {} ({}; {} linked, {} for the role)", player->GetName(),
            FirstRankName(talentInfo->RankID[0]), linked ? "linked" : "role", candidates.size(), generic.size());
        return LearnTalentRank(player, talentInfo, currentRank, reveal, rollLevel);
    }
}

namespace Wildcard
{
    bool LearnTalentRank(Player* player, TalentEntry const* talentInfo, uint8 rank, bool reveal, uint8 level)
    {
        uint8 currentRank = rank;
        uint32 spellId = talentInfo->RankID[currentRank];
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            return false;
        uint8 known = TalentRank(player, talentInfo);     // the rank known (1 up), 0 for none
        bool upgrade = known > 0;

        bool learned = false;
        if (talentInfo->addToSpellBook && !spellInfo->HasAttribute(SPELL_ATTR0_PASSIVE)
            && !spellInfo->HasEffect(SPELL_EFFECT_LEARN_SPELL))
        {
            player->learnSpell(spellId);
            learned = true;
        }

        if (!learned)
            player->SendLearnPacket(spellId, true);

        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
            if (effect.IsEffect(SPELL_EFFECT_LEARN_SPELL) && sSpellMgr->IsAdditionalTalentSpell(effect.TriggerSpell))
                player->learnSpell(effect.TriggerSpell);

        // The rank known goes (addTalent removes it), and any lower one left from before.
        for (uint8 i = 0; known && i + 1 < known; ++i)
            if (talentInfo->RankID[i] && player->HasTalent(talentInfo->RankID[i], player->GetActiveSpec()))
                player->_removeTalent(talentInfo->RankID[i], player->GetActiveSpecMask());
        player->addTalent(spellId, player->GetActiveSpecMask(), known);
        SyncTalentCount(player);
        Announce(player, spellId, upgrade ? "talent rank up" : "new talent");
        UpgradeRanks(player);                   // an ability the talent teaches: every rank the level allows
        if (reveal)
            Classless::RevealTalent(player, talentInfo->TalentID, spellId, currentRank + 1,
                level ? level : player->GetLevel());
        return true;
    }
}

namespace
{
    void DestroyTomes(Player* player)
    {
        for (uint32 entry : { TOME_OF_RANDOMIZATION, TOME_OF_SELECTION })
            if (uint32 count = player->GetItemCount(entry, true))
                player->DestroyItemCount(entry, count, true);
    }

    // Unlearns banned abilities; returns how many the player had.
    uint32 RemoveBanned(Player* player)
    {
        uint32 removed = 0;
        for (Ability const& ability : Abilities())
        {
            if (!ability.Banned || KnownRank(player, ability) < 0)
                continue;

            for (Rank const& rank : ability.Ranks)
                if (player->HasSpell(rank.SpellId))
                    player->removeSpell(rank.SpellId, SPEC_MASK_ALL, false);

            ++removed;
        }

        return removed;
    }

    // Learns a random capstone of the player's role (no role: any) they can learn; another role's only when none of
    // theirs can be (a form or pet it needs); nullptr if none.
    Ability const* GrantCapstone(Player* player)
    {
        Role role = GetRole(player);
        std::vector<Ability const*> candidates, others;
        for (Ability const& ability : Abilities())
            if (ability.Capstone && CanLearn(player, ability)
                && !IsBlacklisted(player, FirstRankName(ability.Ranks.front().SpellId)))
                (role == ROLE_NONE || (ability.CapstoneRoles & RoleMask(role)) ? candidates : others)
                    .push_back(&ability);

        if (candidates.empty())
            candidates = std::move(others);
        if (candidates.empty())
            return nullptr;

        Ability const* ability = candidates[urand(0, uint32(candidates.size() - 1))];
        Learn(player, *ability, "capstone ability");
        return ability;
    }

    // Grants what the player's level is owed. Level-ups (levelUp) play a spin in the Classless window
    // for each ability and talent; catching up at login does not.
    void CatchUp(Player* player, bool grantAbilities, bool levelUp)
    {
        uint8 level = player->GetLevel();

        // Banned abilities go, and are owed again (rolled anew below).
        if (uint32 removed = RemoveBanned(player))
        {
            uint32 granted = player->GetPlayerSetting(SETTING_SOURCE, SETTING_ABILITIES_GRANTED).value;
            player->UpdatePlayerSetting(SETTING_SOURCE, SETTING_ABILITIES_GRANTED,
                granted > removed ? granted - removed : 0);
        }

        // The kit locks: picks never spent are rolled at random, and the tomes are gone.
        if (IsKitLocked(player))
        {
            for (uint32 picks = GetPicks(player); picks; --picks)
                if (!GrantRandom(player, "kit ability"))
                    break;

            SetPicks(player, 0);
            DestroyTomes(player);
        }

        if (grantAbilities)
        {
            // Odd levels from FIRST_ROLL_LEVEL + 1 (9, 11, ...).
            uint32 owed = KIT_SIZE + (level > FIRST_ROLL_LEVEL ? (level - FIRST_ROLL_LEVEL + 1) / 2 : 0);
            uint32 granted = player->GetPlayerSetting(SETTING_SOURCE, SETTING_ABILITIES_GRANTED).value;
            uint32 const before = granted;
            // Kit abilities come without a roll (the tomes handle them).
            while (granted < owed)
            {
                bool kit = granted < KIT_SIZE;
                Ability const* ability = GrantRandom(player, kit ? "kit ability" : "new ability");
                if (!ability)
                    break;

                // The roll's level: the odd ones from FIRST_ROLL_LEVEL + 1, one each.
                uint8 rollLevel = uint8(FIRST_ROLL_LEVEL + 1 + 2 * (granted - KIT_SIZE));
                ++granted;
                if (!kit && levelUp)
                    Classless::Reveal(player, *ability, std::min(rollLevel, level));
            }

            if (granted != before)
                player->UpdatePlayerSetting(SETTING_SOURCE, SETTING_ABILITIES_GRANTED, granted);

            // CAPSTONE_LEVEL's roll: a capstone.
            if (level >= CAPSTONE_LEVEL && !player->GetPlayerSetting(SETTING_SOURCE, SETTING_CAPSTONE_GRANTED).value)
                if (Ability const* capstone = GrantCapstone(player))
                {
                    player->UpdatePlayerSetting(SETTING_SOURCE, SETTING_CAPSTONE_GRANTED, 1);
                    if (levelUp)
                        Classless::Reveal(player, *capstone, CAPSTONE_LEVEL);
                }
        }

        UpgradeRanks(player);

        // Even levels from FIRST_ROLL_LEVEL (8, 10, ...), but CAPSTONE_LEVEL's (a capstone, above).
        uint32 lastTalentLevel = std::min<uint32>(level, CAPSTONE_LEVEL - 1);
        uint32 owedTalents = level >= FIRST_ROLL_LEVEL ? (lastTalentLevel - FIRST_ROLL_LEVEL) / 2 + 1 : 0;
        bool talentsChanged = UpgradeTalents(player);
        // The roll's level: the even ones from FIRST_ROLL_LEVEL, one each.
        for (uint32 spent = RolledTalents(player); spent < owedTalents
            && GrantTalent(player, levelUp, uint8(std::min<uint32>(FIRST_ROLL_LEVEL + 2 * spent, level))); ++spent)
            talentsChanged = true;

        if (talentsChanged)
            player->InitTalentForLevel();

        Classless::SendKnown(player);
        Classless::SendState(player);
    }
}

namespace Wildcard
{
    bool IsHero(Player const* player)
    {
        return player->getClass() == HERO_CLASS;
    }

    // Every class trainer spell, grouped into rank chains with the level each rank needs, plus the
    // level 1 abilities no trainer teaches (talent and starting spells). Built on first use, after the
    // world database is loaded.
    std::vector<Ability> const& Abilities()
    {
        static std::vector<Ability> const abilities = []
        {
            std::unordered_map<uint32, uint8> trainerLevel;
            std::unordered_map<uint32, uint8> trainerClass;
            for (uint8 classId = CLASS_WARRIOR; classId < MAX_CLASSES; ++classId)
            {
                if (!sChrClassesStore.LookupEntry(classId))
                    continue;

                for (Trainer::Trainer const* trainer : sObjectMgr->GetClassTrainers(classId))
                    for (Trainer::Spell const& trainerSpell : trainer->GetSpells())
                        for (uint32 spellId : TaughtSpells(trainerSpell.SpellId))
                        {
                            auto [itr, isNew] = trainerLevel.emplace(spellId, trainerSpell.ReqLevel);
                            if (!isNew)
                                itr->second = std::min(itr->second, trainerSpell.ReqLevel);
                            trainerClass.emplace(spellId, classId);
                        }
            }

            std::vector<Ability> result;
            std::unordered_set<uint32> chains;
            auto addChain = [&](uint32 firstRank, uint8 classId)
            {
                if (!chains.insert(firstRank).second)
                    return;

                Ability ability { };
                ability.ClassId = classId;
                for (uint32 rank = firstRank; rank; rank = sSpellMgr->GetNextSpellInChain(rank))
                {
                    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(rank);
                    if (!spellInfo)
                        break;

                    // Ranks no trainer teaches (a class's starting rank 1) use the spell's own level.
                    auto itr = trainerLevel.find(rank);
                    uint8 level = itr != trainerLevel.end() ? itr->second
                        : uint8(std::max<uint32>(1, spellInfo->SpellLevel));
                    ability.Ranks.push_back({ rank, level });

                    if (!ability.ClassId)
                        if (auto classItr = trainerClass.find(rank); classItr != trainerClass.end())
                            ability.ClassId = classItr->second;
                }

                if (!ability.Ranks.empty())
                {
                    ability.SkillLine = ClassSkillLine(firstRank);
                    result.push_back(std::move(ability));
                }
            };

            for (auto const& [spellId, level] : trainerLevel)
                addChain(sSpellMgr->GetFirstSpellInChain(spellId), 0);

            std::unordered_map<uint32, uint8> skillClass;      // class skill line -> class
            for (Ability const& ability : result)
                if (ability.SkillLine && ability.ClassId)
                    skillClass.emplace(ability.SkillLine, ability.ClassId);

            auto hasName = [](Ability const& ability, std::string_view name)
            {
                return std::any_of(ability.Ranks.begin(), ability.Ranks.end(),
                    [name](Rank const& rank) { return SpellName(rank.SpellId) == name; });
            };

            // Level 1 abilities no trainer teaches: found in the class skill lines (talents, starting spells).
            // What a found spell teaches counts as that ability (Feral Charge: its Bear and Cat versions).
            std::unordered_set<std::string_view> found;
            std::unordered_set<uint32> foundLevelOne;
            for (char const* name : LEVEL_ONE_ABILITIES)
            {
                if (std::any_of(result.begin(), result.end(), [&](Ability const& a) { return hasName(a, name); }))
                    continue;

                // The first spell of that name in a class skill line: the others are its parts (Tame Beast's
                // channel, Stormstrike's two hits), which would show as more abilities of the same name.
                SkillLineAbilityEntry const* first = nullptr;
                for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
                {
                    SkillLineAbilityEntry const* entry = sSkillLineAbilityStore.LookupEntry(i);
                    if (!entry || SpellName(entry->Spell) != name)
                        continue;

                    SkillLineEntry const* skill = sSkillLineStore.LookupEntry(entry->SkillLine);
                    if (!skill || skill->categoryId != SKILL_CATEGORY_CLASS)
                        continue;

                    if (!first || sSpellMgr->GetFirstSpellInChain(entry->Spell)
                        < sSpellMgr->GetFirstSpellInChain(first->Spell))
                        first = entry;
                }
                if (!first)
                    continue;

                auto classItr = skillClass.find(first->SkillLine);
                for (uint32 spellId : TaughtSpells(first->Spell))
                {
                    uint32 firstRank = sSpellMgr->GetFirstSpellInChain(spellId);
                    addChain(firstRank, classItr != skillClass.end() ? classItr->second : 0);
                    foundLevelOne.insert(firstRank);
                    found.insert(name);
                }
            }

            // The talents' abilities: their chains (what the talent's rank 1 teaches, or itself), at their tier's
            // level.
            std::unordered_map<uint32, std::pair<uint32, uint8>> talentAbilities;  // first rank -> talent, level
            // What else a talent teaches, learned along with its ability (Metamorphosis: Shadow Cleave and its
            // hidden aura; Tree of Life: its passive).
            std::unordered_map<uint32, std::vector<uint32>> talentCompanions;     // ability -> companions
            std::unordered_set<uint32> talentExtras;
            for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
            {
                TalentEntry const* talent = sTalentStore.LookupEntry(i);
                if (!talent || !talent->RankID[0])
                    continue;

                TalentTabEntry const* tab = sTalentTabStore.LookupEntry(talent->TalentTab);
                if (!tab || tab->petTalentMask || !(tab->ClassMask & HERO_TALENT_CLASSMASK))
                    continue;

                // Listed, or a talent among the level 1 abilities (Crusader Strike, Mutilate, Stormstrike): either way
                // an ability, never a talent roll.
                std::string_view name = SpellName(talent->RankID[0]);
                auto listed = std::find_if(std::begin(TALENT_ABILITIES), std::end(TALENT_ABILITIES),
                    [name](char const* listedName) { return name == listedName; });
                bool levelOneTalent = std::any_of(std::begin(LEVEL_ONE_ABILITIES), std::end(LEVEL_ONE_ABILITIES),
                    [name](char const* listedName) { return name == listedName; });
                if (listed == std::end(TALENT_ABILITIES) && !levelOneTalent)
                    continue;

                if (listed != std::end(TALENT_ABILITIES))
                    found.insert(*listed);
                uint8 classId = 0;
                for (uint8 c = CLASS_WARRIOR; c <= CLASS_DEMON_HUNTER; ++c)
                    if (tab->ClassMask & (1 << (c - 1)))
                        classId = c;
                uint8 level = uint8(std::min<uint32>(CAPSTONE_LEVEL, 10 + 5 * talent->Row));
                // Its abilities: what it teaches by its own name, shown and cast (Mangle (Cat), Mangle (Bear)); the
                // rest come along.
                std::vector<uint32> taught = TaughtSpells(talent->RankID[0]), mains, extras;
                for (uint32 spellId : taught)
                {
                    SpellInfo const* taughtInfo = sSpellMgr->AssertSpellInfo(spellId);
                    bool main = taught.size() == 1 || (!taughtInfo->IsPassive()
                        && !taughtInfo->HasAttribute(SPELL_ATTR0_DO_NOT_DISPLAY) && SpellName(spellId).starts_with(name)
                        && std::none_of(mains.begin(), mains.end(),
                            [&](uint32 other) { return SpellName(other) == SpellName(spellId); }));
                    uint32 firstRank = sSpellMgr->GetFirstSpellInChain(spellId);
                    addChain(firstRank, classId);
                    (main ? mains : extras).push_back(firstRank);
                }
                for (uint32 firstRank : mains)
                {
                    talentAbilities[firstRank] = { talent->TalentID, level };
                    talentCompanions[firstRank] = extras;
                }
                talentExtras.insert(extras.begin(), extras.end());
            }

            auto matches = [&](Ability const& ability, auto const& names)
            {
                bool matched = false;
                for (std::string_view name : names)
                    if (hasName(ability, name))
                    {
                        found.insert(name);
                        matched = true;
                    }
                return matched;
            };

            std::vector<uint32> companions;
            for (Ability& ability : result)
            {
                uint32 firstRank = ability.Ranks.front().SpellId;
                bool runeforging = std::any_of(ability.Ranks.begin(), ability.Ranks.end(), [](Rank const& rank)
                {
                    SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(rank.SpellId);
                    return std::any_of(bounds.first, bounds.second,
                        [](auto const& pair) { return pair.second->SkillLine == SKILL_RUNEFORGING; });
                });
                ability.Banned = matches(ability, BANNED_ABILITIES) || runeforging;
                bool levelOne = matches(ability, LEVEL_ONE_ABILITIES) || foundLevelOne.count(firstRank);
                for (LevelOneClass const& only : LEVEL_ONE_CLASS_ONLY)
                    if (hasName(ability, only.Name) && ability.ClassId != only.ClassId)
                        levelOne = false;
                ability.Companion = matches(ability, TAME_BEAST_COMPANIONS);
                if (ability.Companion)
                {
                    companions.push_back(firstRank);
                    levelOne = true;
                }
                if (talentExtras.count(firstRank) && !talentAbilities.count(firstRank))
                    ability.Companion = true;

                if (auto talentItr = talentAbilities.find(firstRank); talentItr != talentAbilities.end())
                {
                    ability.TalentId = talentItr->second.first;
                    ability.Ranks.front().Level = talentItr->second.second;
                }

                if (levelOne)
                    ability.Ranks.front().Level = 1;

                if (ability.TalentId)
                    for (CapstoneName const& capstone : CAPSTONE_ABILITIES)
                        if (hasName(ability, capstone.Name))
                        {
                            found.insert(capstone.Name);
                            ability.Capstone = true;
                            ability.CapstoneRoles = capstone.Roles;
                        }
                if (ability.Capstone)
                    ability.Ranks.front().Level = CAPSTONE_LEVEL;
                else
                    ability.Ranks.front().Level = Reachable(ability.Ranks.front().Level);

                SpellInfo const* firstInfo = sSpellMgr->AssertSpellInfo(firstRank);
                ability.Offered = !ability.Banned && !ability.Companion && firstRank != SPELL_PICK_LOCK
                    && (levelOne || ability.TalentId || (!GetTalentSpellCost(firstRank) && !firstInfo->IsPassive()));
            }

            for (Ability& ability : result)
            {
                if (hasName(ability, TAME_BEAST))
                    ability.Companions = companions;
                if (auto itr = talentCompanions.find(ability.Ranks.front().SpellId); itr != talentCompanions.end())
                    ability.Companions = itr->second;
            }

            // Group buffs: their single-target buff's companions, never offered on their own.
            for (GroupVersion const& pair : GROUP_VERSIONS)
            {
                std::vector<Ability*> singles, groups;
                for (Ability& ability : result)
                {
                    if (hasName(ability, pair.Single))
                        singles.push_back(&ability);
                    else if (hasName(ability, pair.Group))
                        groups.push_back(&ability);
                }
                if (singles.empty() || groups.empty())
                    LOG_WARN("module", "mod-titan: Wildcard group version \"{}\" / \"{}\" matches no ability",
                        pair.Single, pair.Group);
                for (Ability* group : groups)
                {
                    group->Companion = true;
                    group->Offered = false;
                    for (Ability* single : singles)
                        if (single->ClassId == group->ClassId)
                            single->Companions.push_back(group->Ranks.front().SpellId);
                }
            }

            for (CustomAbility const& custom : CUSTOM_ABILITIES)
            {
                if (!sSpellMgr->GetSpellInfo(custom.SpellId))
                {
                    LOG_ERROR("module", "mod-titan: Wildcard ability spell {} is missing (is "
                        "titan_demon_hunter_spells.sql applied?)", custom.SpellId);
                    continue;
                }

                // Later ranks (spell_ranks, client-patch/patch_ranks.py) at their spells' levels.
                Ability ability { };
                ability.Ranks.push_back({ custom.SpellId, custom.Level });
                for (uint32 rank = sSpellMgr->GetNextSpellInChain(custom.SpellId); rank;
                    rank = sSpellMgr->GetNextSpellInChain(rank))
                    ability.Ranks.push_back({ rank, uint8(sSpellMgr->AssertSpellInfo(rank)->SpellLevel) });
                ability.ClassId = custom.ClassId;
                ability.SkillLine = custom.SkillLine;
                ability.Offered = true;
                for (CustomCapstone const& capstone : CUSTOM_CAPSTONES)
                    if (capstone.SpellId == custom.SpellId)
                    {
                        ability.Capstone = true;
                        ability.CapstoneRoles = capstone.Roles;
                    }
                if (ability.Capstone)
                    ability.Ranks.front().Level = CAPSTONE_LEVEL;
                result.push_back(std::move(ability));
            }

            // Requirements. Forms and stances: the abilities whose spells give each form (any rank).
            // Stealth: those that stealth. Pets: by name or class. Combo points: those awarding them.
            std::unordered_map<uint32, std::vector<uint32>> formGivers;
            std::vector<uint32> stealthGivers;
            std::vector<uint32> comboGivers;
            for (Ability const& ability : result)
            {
                uint32 firstRank = ability.Ranks.front().SpellId;
                for (Rank const& rank : ability.Ranks)
                    for (SpellEffectInfo const& effect : sSpellMgr->AssertSpellInfo(rank.SpellId)->GetEffects())
                    {
                        if (effect.ApplyAuraName == SPELL_AURA_MOD_SHAPESHIFT && effect.MiscValue > 0)
                        {
                            std::vector<uint32>& givers = formGivers[effect.MiscValue];
                            if (std::find(givers.begin(), givers.end(), firstRank) == givers.end())
                                givers.push_back(firstRank);
                        }
                        else if (effect.ApplyAuraName == SPELL_AURA_MOD_STEALTH
                            && std::find(stealthGivers.begin(), stealthGivers.end(), firstRank) == stealthGivers.end())
                            stealthGivers.push_back(firstRank);
                        if (effect.Effect == SPELL_EFFECT_ADD_COMBO_POINTS && ability.Offered && !ability.Banned
                            && std::find(comboGivers.begin(), comboGivers.end(), firstRank) == comboGivers.end())
                            comboGivers.push_back(firstRank);
                    }
                if (ability.Offered && !ability.Banned
                    && std::any_of(std::begin(CODE_BUILDERS), std::end(CODE_BUILDERS),
                        [&](char const* name) { return hasName(ability, name); })
                    && std::find(comboGivers.begin(), comboGivers.end(), firstRank) == comboGivers.end())
                    comboGivers.push_back(firstRank);
            }

            auto byNames = [&](std::span<char const* const> names)
            {
                std::vector<uint32> givers;
                for (Ability const& ability : result)
                    if (std::any_of(names.begin(), names.end(),
                        [&](char const* name) { return hasName(ability, name); }))
                        givers.push_back(ability.Ranks.front().SpellId);
                return givers;
            };

            uint32 unreachable = 0, finishers = 0;
            for (Ability& ability : result)
            {
                if (!ability.Offered)
                    continue;

                SpellInfo const* spellInfo = sSpellMgr->AssertSpellInfo(ability.Ranks.front().SpellId);
                if (spellInfo->Stances && !spellInfo->HasAttribute(SPELL_ATTR2_ALLOW_WHILE_NOT_SHAPESHIFTED))
                {
                    std::vector<uint32> givers;
                    for (auto const& [form, formAbilities] : formGivers)
                        if (spellInfo->Stances & (1 << (form - 1)))
                            givers.insert(givers.end(), formAbilities.begin(), formAbilities.end());
                    ability.Requires.push_back(std::move(givers));
                }

                // Stealth that works with its form: Prowl (in Cat Form) for Pounce, Stealth for Ambush.
                if (spellInfo->HasAttribute(SPELL_ATTR0_ONLY_STEALTHED))
                {
                    std::vector<uint32> givers;
                    for (uint32 giver : stealthGivers)
                    {
                        uint32 giverStances = sSpellMgr->AssertSpellInfo(giver)->Stances;
                        bool givesForm = std::any_of(formGivers.begin(), formGivers.end(), [&](auto const& pair)
                        {
                            return (spellInfo->Stances & (1 << (pair.first - 1)))
                                && std::find(pair.second.begin(), pair.second.end(), giver) != pair.second.end();
                        });
                        bool sameForm = spellInfo->Stances ? (giverStances & spellInfo->Stances) != 0 : !giverStances;
                        if (givesForm || sameForm)
                            givers.push_back(giver);
                    }
                    ability.Requires.push_back(std::move(givers));
                }

                std::span<char const* const> pets;
                for (NeedsPet const& needs : PET_ABILITIES)
                    if (hasName(ability, needs.Name))
                        pets = needs.Pets;
                if (pets.empty())
                    for (SpellEffectInfo const& effect : spellInfo->GetEffects())
                        if (effect.TargetA.GetTarget() == TARGET_UNIT_PET
                            || effect.TargetB.GetTarget() == TARGET_UNIT_PET)
                            pets = ClassPets(ability.ClassId);
                if (!pets.empty())
                    ability.Requires.push_back(byNames(pets));
                for (NeedsPet const& pair : PAIRED_ABILITIES)
                    if (hasName(ability, pair.Name))
                        ability.Requires.push_back(byNames(pair.Pets));

                // A finisher (it spends combo points): only with an ability awarding them.
                auto named = [&](auto const& names)
                {
                    return std::any_of(std::begin(names), std::end(names),
                        [&](char const* name) { return hasName(ability, name); });
                };
                bool finisher = (std::any_of(ability.Ranks.begin(), ability.Ranks.end(), [](Rank const& rank)
                {
                    return sSpellMgr->AssertSpellInfo(rank.SpellId)->NeedsComboPoints();
                }) || named(SCRIPTED_FINISHERS)) && !named(NOT_FINISHERS);
                if (finisher)
                {
                    ability.ComboGroup = int8(ability.Requires.size());
                    ability.Requires.push_back(comboGivers);
                    ++finishers;
                }

                if (std::any_of(ability.Requires.begin(), ability.Requires.end(),
                    [](std::vector<uint32> const& givers) { return givers.empty(); }))
                    ++unreachable;
            }

            // Roles.
            for (Ability& ability : result)
            {
                SpellInfo const* spellInfo = sSpellMgr->AssertSpellInfo(ability.Ranks.front().SpellId);
                ability.Roles = SpellRoles(spellInfo);
                for (RoleAbility const& tagged : ROLE_ABILITIES)
                    if (hasName(ability, tagged.Name))
                        ability.Roles = tagged.Roles;
                // The Demon Hunter's Vengeance is a tank's: its damage serves a tank too.
                if (ability.SkillLine == SKILL_VENGEANCE && ability.Roles)
                    ability.Roles |= TAG_TANK;
            }

            if (unreachable)
                LOG_INFO("module", "mod-titan: Wildcard: {} abilities need a form, stealth or pet no ability gives; "
                    "they are never rolled", unreachable);
            LOG_INFO("module", "mod-titan: Wildcard: {} finishers need one of {} combo point builders", finishers,
                comboGivers.size());
            for (char const* name : SCRIPTED_FINISHERS)
                if (std::none_of(result.begin(), result.end(), [&](Ability const& a) { return hasName(a, name); }))
                    LOG_WARN("module", "mod-titan: Wildcard scripted finisher \"{}\" matches no ability", name);

            for (char const* name : BANNED_ABILITIES)
                if (!found.count(name))
                    LOG_WARN("module", "mod-titan: Wildcard banned ability \"{}\" matches no class spell", name);
            for (char const* name : TALENT_ABILITIES)
                if (!found.count(name))
                    LOG_WARN("module", "mod-titan: Wildcard talent ability \"{}\" matches no talent", name);
            for (CapstoneName const& capstone : CAPSTONE_ABILITIES)
                if (!found.count(capstone.Name))
                    LOG_WARN("module", "mod-titan: Wildcard capstone \"{}\" matches no talent ability",
                        capstone.Name);
            for (char const* name : LEVEL_ONE_ABILITIES)
                if (!found.count(name))
                    LOG_WARN("module", "mod-titan: Wildcard level 1 ability \"{}\" matches no class spell", name);

            LOG_INFO("module", "mod-titan: Wildcard pool has {} abilities ({} rank chains, {} banned, {} from talents, "
                "{} capstones)",
                std::count_if(result.begin(), result.end(), [](Ability const& a) { return a.Offered; }), result.size(),
                std::count_if(result.begin(), result.end(), [](Ability const& a) { return a.Banned; }),
                std::count_if(result.begin(), result.end(), [](Ability const& a) { return a.TalentId != 0; }),
                std::count_if(result.begin(), result.end(), [](Ability const& a) { return a.Capstone; }));
            return result;
        }();

        return abilities;
    }

    char const* CustomSkillLineName(uint32 skillLine)
    {
        for (auto const& [id, name] : CUSTOM_SKILL_LINES)
            if (id == skillLine)
                return name;

        return nullptr;
    }

    Ability const* FindAbility(uint32 firstRankSpellId)
    {
        static std::unordered_map<uint32, Ability const*> const byFirstRank = []
        {
            std::unordered_map<uint32, Ability const*> map;
            for (Ability const& ability : Abilities())
                map.emplace(ability.Ranks.front().SpellId, &ability);
            return map;
        }();

        auto itr = byFirstRank.find(firstRankSpellId);
        return itr != byFirstRank.end() ? itr->second : nullptr;
    }

    // Index of the highest rank the player knows, or -1.
    int32 KnownRank(Player const* player, Ability const& ability)
    {
        for (int32 i = int32(ability.Ranks.size()) - 1; i >= 0; --i)
            if (player->HasSpell(ability.Ranks[i].SpellId))
                return i;

        return -1;
    }

    bool MeetsRequirements(Player const* player, Ability const& ability)
    {
        return std::all_of(ability.Requires.begin(), ability.Requires.end(), [player](std::vector<uint32> const& givers)
        {
            return std::any_of(givers.begin(), givers.end(), [player](uint32 firstRank)
            {
                Ability const* giver = FindAbility(firstRank);
                return giver ? KnownRank(player, *giver) >= 0 : player->HasSpell(firstRank);
            });
        });
    }

    bool CanLearn(Player const* player, Ability const& ability)
    {
        return ability.Offered && BestRank(player, ability) >= 0 && KnownRank(player, ability) < 0
            && (!ability.Capstone || player->GetLevel() >= CAPSTONE_LEVEL)
            && FitsRace(player, ability.Ranks.front().SpellId) && MeetsRequirements(player, ability);
    }

    bool FitsPlayer(Player const* player, Ability const& ability)
    {
        return FitsRace(player, ability.Ranks.front().SpellId);
    }

    Ability const* TalentAbility(uint32 talentId)
    {
        static std::unordered_map<uint32, Ability const*> const byTalent = []
        {
            std::unordered_map<uint32, Ability const*> map;
            for (Ability const& ability : Abilities())
                if (ability.TalentId && ability.Offered)
                    map.emplace(ability.TalentId, &ability);
            return map;
        }();

        auto itr = byTalent.find(talentId);
        return itr != byTalent.end() ? itr->second : nullptr;
    }

    void Learn(Player* player, Ability const& ability, char const* what)
    {
        // Every rank up to the best the level allows (not the best alone: a reroll left Heroes at rank 1), then
        // every other ability checked for ranks still pending.
        int32 best = BestRank(player, ability);
        for (int32 i = 0; i <= best; ++i)
            if (!player->HasSpell(ability.Ranks[i].SpellId))
                player->learnSpell(ability.Ranks[i].SpellId);
        Announce(player, ability.Ranks[best].SpellId, what);

        for (uint32 companionId : ability.Companions)
            if (Ability const* companion = FindAbility(companionId))
                if (int32 top = BestRank(player, *companion); top >= 0 && KnownRank(player, *companion) < 0)
                    for (int32 i = 0; i <= top; ++i)
                        player->learnSpell(companion->Ranks[i].SpellId);
        UpgradeRanks(player);
    }

    namespace
    {
        // What the Hero's known abilities already do, for weighing a roll.
        struct RollFacts
        {
            std::array<uint8, MAX_SPELL_SCHOOL> SchoolCounts = {};  // damage abilities dealing each school
            uint32 Powers = 0;          // 1 << power type of every resource they spend (POWER_RUNE: runes)
            uint8 PetFamily = 0;        // 1 hunter pet, 2 demons, 3 undead minions; 0 none
            std::array<uint8, 8> RoleCounts = {};   // known abilities with each role tag
        };

        uint8 PetFamilyOf(Ability const& ability)
        {
            SpellInfo const* spellInfo = sSpellMgr->AssertSpellInfo(ability.Ranks.front().SpellId);
            char const* name = spellInfo->SpellName[0];
            if (!name)
                return 0;
            auto in = [name](std::span<char const* const> names)
            {
                return std::any_of(names.begin(), names.end(), [name](char const* n) { return !strcmp(n, name); });
            };
            if (in(HUNTER_PETS) || !strcmp(name, "Call Pet"))
                return 1;
            if (in(DEMONS))
                return 2;
            if (in(UNDEAD_MINIONS))
                return 3;
            return 0;
        }

        bool IsCooldown(uint32 spellId)
        {
            SpellInfo const* spellInfo = sSpellMgr->AssertSpellInfo(spellId);
            return spellInfo->RecoveryTime >= 10000 || spellInfo->CategoryRecoveryTime >= 10000;
        }

        // The resources a spell spends, as 1 << power type (POWER_RUNE for a rune cost).
        uint32 PowersOf(SpellInfo const* spellInfo)
        {
            uint32 powers = 0;
            if (spellInfo->ManaCost || spellInfo->ManaCostPercentage)
                powers |= 1 << spellInfo->PowerType;
            if (spellInfo->RuneCostID)
                powers |= 1 << POWER_RUNE;
            return powers;
        }

        // The schools a spell deals damage in (0 if it deals none).
        uint32 DamageSchools(SpellInfo const* spellInfo)
        {
            return (SpellRoles(spellInfo) & (TAG_MELEE | TAG_RANGED)) ? spellInfo->SchoolMask : 0;
        }

        RollFacts Facts(Player* player)
        {
            RollFacts facts;
            for (Ability const& ability : Abilities())
            {
                if (KnownRank(player, ability) < 0)
                    continue;
                SpellInfo const* spellInfo = sSpellMgr->AssertSpellInfo(ability.Ranks.front().SpellId);
                uint32 schools = DamageSchools(spellInfo);
                for (uint8 school = 0; school < MAX_SPELL_SCHOOL; ++school)
                    if (schools & (1 << school))
                        ++facts.SchoolCounts[school];
                facts.Powers |= PowersOf(spellInfo);
                if (uint8 family = PetFamilyOf(ability))
                    facts.PetFamily = family;
                for (uint8 bit = 0; bit < 8; ++bit)
                    if (ability.Roles & (1 << bit))
                        ++facts.RoleCounts[bit];
            }
            return facts;
        }

        bool PetAllowed(RollFacts const& facts, Ability const& ability)
        {
            uint8 family = PetFamilyOf(ability);
            return !family || !facts.PetFamily || family == facts.PetFamily;
        }

        // The role tags the kit still lacks (0: none): 2 abilities doing the role's work.
        uint8 KitNeed(Player* player, RollFacts const& facts)
        {
            auto count = [&](uint8 tags)
            {
                uint8 n = 0;
                for (uint8 bit = 0; bit < 8; ++bit)
                    if (tags & (1 << bit))
                        n = std::max(n, facts.RoleCounts[bit]);
                return n;
            };
            switch (GetRole(player))
            {
                case ROLE_MELEE: return count(TAG_MELEE) < 2 ? TAG_MELEE : 0;
                case ROLE_RANGED: return count(TAG_RANGED) < 2 ? TAG_RANGED : 0;
                case ROLE_HEALER: return count(TAG_HEALER) < 2 ? TAG_HEALER : 0;
                case ROLE_TANK:
                    if (count(TAG_TANK) < 1)
                        return TAG_TANK;
                    return count(TAG_MELEE) < 1 ? TAG_MELEE : 0;
                default:
                    return count(TAG_MELEE | TAG_RANGED) < 2 ? uint8(TAG_MELEE | TAG_RANGED) : 0;
            }
        }

        float Weight(RollFacts const& facts, Ability const& ability)
        {
            SpellInfo const* spellInfo = sSpellMgr->AssertSpellInfo(ability.Ranks.front().SpellId);
            float weight = 1.0f;
            uint32 schools = DamageSchools(spellInfo);
            uint8 matching = 0;
            for (uint8 school = 0; school < MAX_SPELL_SCHOOL; ++school)
                if (schools & (1 << school))
                    matching = std::max(matching, facts.SchoolCounts[school]);
            weight *= 1.0f + 0.25f * std::min<uint8>(matching, 4);
            if (uint32 powers = PowersOf(spellInfo); powers && (powers & facts.Powers))
                weight *= 1.4f;
            return weight;
        }
    }

    // The Mage's conjured food and water: rolled only for a Ranged Hero who knows another Mage ability.
    constexpr char const* CONJURES[] = { "Conjure Food", "Conjure Water", "Conjure Refreshment" };

    bool ConjureAllowed(Player* player, Ability const& ability)
    {
        std::string const name = FirstRankName(ability.Ranks.front().SpellId);
        auto isConjure = [](std::string const& spell)
        {
            return std::any_of(std::begin(CONJURES), std::end(CONJURES),
                [&spell](char const* conjure) { return spell == conjure; });
        };
        if (!isConjure(name))
            return true;
        if (GetRole(player) != ROLE_RANGED)
            return false;
        for (Ability const& other : Abilities())
            if (other.ClassId == CLASS_MAGE && KnownRank(player, other) >= 0
                && !isConjure(FirstRankName(other.Ranks.front().SpellId)))
                return true;
        return false;
    }

    // An ability roll (the kit's, a level's, a reroll's), steered so a build takes shape:
    // - those suiting the Hero's role (any when none does), never one blacklisted by a reroll; conjured food and
    //   water only for a Ranged Hero with another Mage ability (ConjureAllowed);
    // - one pet family: once the Hero summons a hunter pet, a demon or a ghoul, the other families' summons stay out;
    // - the kit (before LOCK_LEVEL) gets 2 abilities doing the role's work first (Healer: heals; Tank: a threat
    //   ability, then a melee one; no role: damage);
    // - after 2 rolls in a row gave a cooldown (10 sec or more), the next is one to press on repeat;
    // - weighted toward the damage schools the Hero's abilities already deal (+25% a matching ability, up to 4) and
    //   the resources they already spend (+40%).
    Ability const* GrantRandom(Player* player, char const* what)
    {
        RollFacts facts = Facts(player);
        std::vector<Ability const*> candidates, others;
        for (Ability const& ability : Abilities())
            if (!ability.Capstone && CanLearn(player, ability) && PetAllowed(facts, ability)
                && ConjureAllowed(player, ability)
                && !IsBlacklisted(player, FirstRankName(ability.Ranks.front().SpellId)))
                (SuitsRole(player, ability.Roles) ? candidates : others).push_back(&ability);

        if (candidates.empty())
            candidates = std::move(others);

        char const* reason = "role";
        auto narrow = [&](auto keep, char const* why)
        {
            std::vector<Ability const*> kept;
            std::copy_if(candidates.begin(), candidates.end(), std::back_inserter(kept), keep);
            if (!kept.empty())
            {
                candidates = std::move(kept);
                reason = why;
            }
        };
        if (player->GetLevel() < LOCK_LEVEL)
            if (uint8 need = KitNeed(player, facts))
                narrow([need](Ability const* ability) { return (ability->Roles & need) != 0; }, "kit floor");
        if (player->GetPlayerSetting(SETTING_SOURCE, SETTING_COOLDOWN_STREAK).value >= 2)
            narrow([](Ability const* ability) { return !IsCooldown(ability->Ranks.front().SpellId); }, "no cooldown");
        if (candidates.empty())
            return nullptr;

        std::vector<float> weights;
        float total = 0.0f;
        for (Ability const* ability : candidates)
        {
            weights.push_back(Weight(facts, *ability));
            total += weights.back();
        }
        float roll = frand(0.0f, total);
        std::size_t chosen = 0;
        for (; chosen + 1 < candidates.size() && roll >= weights[chosen]; ++chosen)
            roll -= weights[chosen];

        Ability const* ability = candidates[chosen];
        bool cooldown = IsCooldown(ability->Ranks.front().SpellId);
        uint32 streak = player->GetPlayerSetting(SETTING_SOURCE, SETTING_COOLDOWN_STREAK).value;
        player->UpdatePlayerSetting(SETTING_SOURCE, SETTING_COOLDOWN_STREAK, cooldown ? streak + 1 : 0);
        LOG_DEBUG("module", "Wildcard: {} rolled ability {} ({}; weight {:.2f} of {:.2f}, {} candidates)",
            player->GetName(), FirstRankName(ability->Ranks.front().SpellId), reason, weights[chosen], total,
            candidates.size());
        Learn(player, *ability, what);
        return ability;
    }

    void Unlearn(Player* player, Ability const& ability)
    {
        // The spells going, and what they cast (Raise Dead's ghoul comes from the spell it triggers).
        std::unordered_set<uint32> gone;
        auto remove = [&](Ability const& unlearned)
        {
            for (Rank const& rank : unlearned.Ranks)
            {
                gone.insert(rank.SpellId);
                if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(rank.SpellId))
                    for (SpellEffectInfo const& effect : spellInfo->GetEffects())
                        if (effect.TriggerSpell)
                            gone.insert(effect.TriggerSpell);
                if (player->HasSpell(rank.SpellId))
                    player->removeSpell(rank.SpellId, SPEC_MASK_ALL, false);
            }
        };
        remove(ability);
        for (uint32 companionId : ability.Companions)
            if (Ability const* companion = FindAbility(companionId))
                remove(*companion);

        // The pet it summoned goes too: a demon or ghoul from these spells, or a tamed beast once Call Pet is gone
        // (Tame Beast's companion). It stays in the stable.
        if (Pet* pet = player->GetPet())
            if (gone.count(pet->GetUInt32Value(UNIT_CREATED_BY_SPELL))
                || (pet->getPetType() == HUNTER_PET && !player->HasSpell(SPELL_CALL_PET)))
                player->RemovePet(pet, PET_SAVE_NOT_IN_SLOT);
    }

    uint32 KitAbilities(Player* player)
    {
        uint32 known = 0;
        for (Ability const& ability : Abilities())
            if (ability.Offered && KnownRank(player, ability) >= 0)
                ++known;
        return known;
    }

    uint32 ClearAbilities(Player* player)
    {
        uint32 cleared = 0;
        for (Ability const& ability : Abilities())
        {
            if (!(ability.Offered || ability.Companion) || KnownRank(player, ability) < 0)
                continue;

            Unlearn(player, ability);
            if (ability.Offered)
                ++cleared;
        }

        return cleared;
    }

    void Reset(Player* player)
    {
        ClearAbilities(player);
        player->resetTalents(true);
        for (uint32 setting : { SETTING_ABILITIES_GRANTED, SETTING_PICKS_LEFT, SETTING_CAPSTONE_GRANTED })
            player->UpdatePlayerSetting(SETTING_SOURCE, setting, 0);
    }

    uint32 GetPicks(Player* player)
    {
        return player->GetPlayerSetting(SETTING_SOURCE, SETTING_PICKS_LEFT).value;
    }

    void SetPicks(Player* player, uint32 picks)
    {
        player->UpdatePlayerSetting(SETTING_SOURCE, SETTING_PICKS_LEFT, picks);
    }

    void SetQuiet(bool on)
    {
        quiet = on;
    }

    void CatchUpNow(Player* player)
    {
        CatchUp(player, true, false);
    }

    void LearnQuiet(Player* player, Ability const& ability)
    {
        if (BestRank(player, ability) < 0 || KnownRank(player, ability) >= 0)
            return;
        bool was = quiet;
        quiet = true;
        Learn(player, ability, "");
        quiet = was;
    }

    bool GrantTalentExcept(Player* player, uint32 talentId, bool reveal)
    {
        excludedTalent = talentId;
        bool granted = GrantTalent(player, reveal, player->GetLevel());
        excludedTalent = 0;
        return granted;
    }

    Ability const* GrantNewCapstone(Player* player)
    {
        return GrantCapstone(player);
    }

    // The reroll blacklist: a spell's name (any rank, any class's talent of that name) kept out of every roll for the
    // next BLACKLIST_REROLLS rerolls.
    std::string Blacklist(Player* player, uint32 spellId)
    {
        std::string name = FirstRankName(spellId);
        if (name.empty())
            return name;
        uint32 until = player->GetPlayerSetting(SETTING_SOURCE, SETTING_REROLLS).value + 1 + BLACKLIST_REROLLS;
        blacklists[player->GetGUID().GetCounter()][name] = until;
        CharacterDatabase.Execute("REPLACE INTO `titan_reroll_blacklist` (`guid`, `name`, `until_reroll`) VALUES "
            "({}, '{}', {})", player->GetGUID().GetCounter(), EscapeName(name), until);
        return name;
    }

    void Unblacklist(Player* player, std::string const& name)
    {
        if (name.empty())
            return;
        blacklists[player->GetGUID().GetCounter()].erase(name);
        CharacterDatabase.Execute("DELETE FROM `titan_reroll_blacklist` WHERE `guid` = {} AND `name` = '{}'",
            player->GetGUID().GetCounter(), EscapeName(name));
    }

    void CountReroll(Player* player)
    {
        uint32 count = player->GetPlayerSetting(SETTING_SOURCE, SETTING_REROLLS).value + 1;
        player->UpdatePlayerSetting(SETTING_SOURCE, SETTING_REROLLS, count);
        auto& names = blacklists[player->GetGUID().GetCounter()];
        for (auto itr = names.begin(); itr != names.end();)
            itr = itr->second <= count ? names.erase(itr) : std::next(itr);
        CharacterDatabase.Execute("DELETE FROM `titan_reroll_blacklist` WHERE `guid` = {} AND `until_reroll` <= {}",
            player->GetGUID().GetCounter(), count);
    }

    uint8 TalentRank(Player const* player, TalentEntry const* talentInfo)
    {
        uint8 rank = 0;
        for (uint8 i = 0; i < MAX_TALENT_RANK; ++i)
            if (talentInfo->RankID[i] && player->HasTalent(talentInfo->RankID[i], player->GetActiveSpec()))
                rank = i + 1;
        return rank;
    }

    uint8 RemoveTalent(Player* player, TalentEntry const* talentInfo)
    {
        // As Player::resetTalents does it, for this talent's ranks only.
        uint8 removed = TalentRank(player, talentInfo);
        for (uint8 i = 0; i < MAX_TALENT_RANK; ++i)
        {
            uint32 spellId = talentInfo->RankID[i];
            if (!spellId || !player->HasTalent(spellId, player->GetActiveSpec()))
                continue;
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
            if (!spellInfo)
                continue;
            player->_removeTalentAurasAndSpells(spellId);
            if (talentInfo->addToSpellBook && !spellInfo->HasAttribute(SPELL_ATTR0_PASSIVE)
                && !spellInfo->HasEffect(SPELL_EFFECT_LEARN_SPELL))
                player->removeSpell(spellId, player->GetActiveSpecMask(), false);
            else
                player->SendLearnPacket(spellId, false);
            for (SpellEffectInfo const& effect : spellInfo->GetEffects())
                if (effect.IsEffect(SPELL_EFFECT_LEARN_SPELL)
                    && sSpellMgr->IsAdditionalTalentSpell(effect.TriggerSpell))
                    player->removeSpell(effect.TriggerSpell, player->GetActiveSpecMask(), false);
            player->_removeTalent(spellId, player->GetActiveSpecMask());
        }
        SyncTalentCount(player);
        return removed;
    }

    bool IsKitLocked(Player const* player)
    {
        return player->GetLevel() >= LOCK_LEVEL;
    }

    bool SuitsRole(Player* player, uint8 roles)
    {
        static constexpr std::array<uint8, MAX_ROLES> ACCEPTS = {
            0xFF,                       // none: everything
            TAG_MELEE,                  // Melee: melee and physical ranged
            TAG_RANGED,                 // Ranged: magic damage
            TAG_HEALER | TAG_RANGED,    // Healer: heals, and magic damage to level with
            TAG_TANK | TAG_MELEE,       // Tank: tanking and melee
        };
        return !roles || (roles & ACCEPTS[GetRole(player)]);
    }

    Role GetRole(Player* player)
    {
        uint32 role = player->GetPlayerSetting(SETTING_SOURCE, SETTING_ROLE).value;
        return role < MAX_ROLES ? Role(role) : ROLE_NONE;
    }

    bool SetRole(Player* player, Role role)
    {
        if (role >= MAX_ROLES || player->IsInCombat())
            return false;

        player->UpdatePlayerSetting(SETTING_SOURCE, SETTING_ROLE, role);
        ApplyRole(player);
        return true;
    }

    void ApplyRole(Player* player)
    {
        Role role = GetRole(player);
        for (uint8 i = ROLE_MELEE; i < MAX_ROLES; ++i)
        {
            if (i != role)
                player->RemoveAurasDueToSpell(ROLE_SPELLS[i]);
            else if (!player->HasAura(ROLE_SPELLS[i]))
                player->AddAura(ROLE_SPELLS[i], player);
        }
    }
}

namespace
{
    // Pick Lock for everyone. Lockpicking is a Rogue skill: the core drops it at every login for any
    // other class (it is invalid for the race/class), so it is set again each time, at its maximum.
    void EnsurePickLock(Player* player)
    {
        if (!player->HasSpell(SPELL_PICK_LOCK))
            player->learnSpell(SPELL_PICK_LOCK);

        uint16 maxValue = uint16(player->GetLevel()) * 5;
        if (player->GetPureSkillValue(SKILL_LOCKPICKING) != maxValue
            || player->GetPureMaxSkillValue(SKILL_LOCKPICKING) != maxValue)
            player->SetSkill(SKILL_LOCKPICKING, 0, maxValue, maxValue);
    }
}

class titan_wildcard : public PlayerScript
{
public:
    titan_wildcard() : PlayerScript("titan_wildcard", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LEVEL_CHANGED,
        PLAYERHOOK_ON_PLAYER_RESURRECT,
        PLAYERHOOK_ON_CALCULATE_TALENTS_POINTS,
        PLAYERHOOK_CAN_LEARN_TALENT,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_DELETE
    }) { }

    void OnPlayerLogout(Player* player) override
    {
        blacklists.erase(player->GetGUID().GetCounter());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        blacklists.erase(guid.GetCounter());
        CharacterDatabase.Execute("DELETE FROM `titan_reroll_blacklist` WHERE `guid` = {}", guid.GetCounter());
    }

    void OnPlayerLogin(Player* player) override
    {
        EnsurePickLock(player);
        if (!IsHero(player))
            return;

        // The reroll blacklist, for the rolls to come (the login's own catch-up rolls run before it is in: they
        // only give what the level is owed).
        ObjectGuid::LowType low = player->GetGUID().GetCounter();
        blacklists.erase(low);
        player->GetSession()->GetQueryProcessor().AddCallback(CharacterDatabase.AsyncQuery(Acore::StringFormat(
            "SELECT `name`, `until_reroll` FROM `titan_reroll_blacklist` WHERE `guid` = {}", low))
            .WithCallback([low](QueryResult result)
        {
            if (result)
                do
                    blacklists[low][(*result)[0].Get<std::string>()] = (*result)[1].Get<uint32>();
                while (result->NextRow());
        }));

        // Without saved settings the ability count resets every login and would be granted again.
        bool settingsSaved = sWorld->getBoolConfig(CONFIG_PLAYER_SETTINGS_ENABLED);
        static bool warned = false;
        if (!warned)
        {
            warned = true;
            if (!settingsSaved)
                LOG_ERROR("module", "mod-titan: Wildcard needs EnablePlayerSettings = 1 in worldserver.conf; "
                    "abilities are only granted on level up until then");
            if (sWorld->getBoolConfig(CONFIG_VALIDATE_SKILL_LEARNED_BY_SPELLS))
                LOG_ERROR("module", "mod-titan: Wildcard needs ValidateSkillLearnedBySpells = 0 in worldserver.conf; "
                    "other classes' spells are deleted at login until then");
        }

        DropSkillLineSpells(player);
        DropStaleRanks(player);
        CatchUp(player, settingsSaved, false);
        ApplyRole(player);
    }

    void OnPlayerResurrect(Player* player, float /*restorePercent*/, bool& /*applySickness*/) override
    {
        if (IsHero(player))
            ApplyRole(player);
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        EnsurePickLock(player);
        if (IsHero(player))
            CatchUp(player, true, true);
    }

    // A Hero's talents all come from Wildcard: as many points as are spent, none left to spend,
    // and the "more talents than allowed" reset never triggers.
    void OnPlayerCalculateTalentsPoints(Player const* player, uint32& talentPointsForLevel) override
    {
        if (IsHero(player))
            talentPointsForLevel = SpentTalentPoints(player);
    }

    bool OnPlayerCanLearnTalent(Player* player, TalentEntry const* /*talent*/, uint32 /*rank*/) override
    {
        return !IsHero(player);
    }
};

// "All means all": a talent whose words reach every spell of a kind ("all your healing spells", "your spells", "your
// Frost spells": global_talents.h, tools/wildcard/global_talents.py) has its spell modifiers apply to every class's
// spells of that kind, not only its own class's (SpellInfo::IsAffectedBySpellMod: false here means affected). A Hero
// has every class's spells; the talent's own class's keep its own masks too.
class titan_global_talents : public GlobalScript
{
public:
    titan_global_talents() : GlobalScript("titan_global_talents", { GLOBALHOOK_ON_IS_AFFECTED_BY_SPELL_MOD_CHECK }) { }

    bool OnIsAffectedBySpellModCheck(SpellInfo const* affectSpell, SpellInfo const* checkSpell,
        SpellModifier const* /*mod*/) override
    {
        return GlobalReach(affectSpell->Id, checkSpell) != 1;
    }
};

void AddSC_wildcard()
{
    new titan_global_talents();
    new titan_wildcard();
}
