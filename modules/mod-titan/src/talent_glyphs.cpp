/*
 * Emberforge: a talent's glyph is part of the talent. A Hero is a Paladin to the game and inscribes only a Paladin's
 * glyphs, so every other class's talent glyphs were out of reach: a Hero who knows a talent (any rank) now holds its
 * glyph's effect too (Vigor: 10 more maximum Energy, Glyph of Vigor's). The glyph's own spell, a hidden passive, rides
 * on the Hero as an aura (passive auras are not saved: put back at login), added when the talent is learned and taken
 * when its last rank is forgotten. The client patch says so on the talent's tooltip (patch_spells.py,
 * bake_talent_glyphs). The pairs: every inscribable glyph named after a talent of its own class (Spell.dbc; the
 * class's talents, not a pet's); left out, the glyphs that trade something away (Insect Swarm's hit debuff, Blast
 * Wave's, Thunderstorm's and Typhoon's knockback, Avenger's Shield's targets).
 */

#include "Player.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include <unordered_map>

namespace
{
    // A talent's first rank (its spell) -> its glyph's effect.
    std::unordered_map<uint32, uint32> const TALENT_GLYPHS = {
        { 13750, 56808 },   // Adrenaline Rush
        { 19434, 56824 },   // Aimed Shot
        { 44425, 63092 },   // Arcane Barrage
        { 12042, 56381 },   // Arcane Power
        { 53563, 63218 },   // Beacon of Light
        { 50334, 62969 },   // Berserk
        { 19574, 56830 },   // Bestial Wrath
        { 13877, 56818 },   // Blade Flurry
        { 46924, 63324 },   // Bladestorm
        { 23881, 58369 },   // Bloodthirst
        { 49222, 58673 },   // Bone Shield
        { 50796, 63304 },   // Chaos Bolt
        { 53209, 63065 },   // Chimera Shot
        { 34861, 55675 },   // Circle of Healing
        { 17962, 56235 },   // Conflagrate
        { 49158, 59307 },   // Corpse Explosion
        { 35395, 54927 },   // Crusader Strike
        { 49028, 63330 },   // Dancing Rune Weapon
        { 44572, 63090 },   // Deep Freeze
        { 20243, 58388 },   // Devastate
        { 47585, 63229 },   // Dispersion
        { 53385, 63220 },   // Divine Storm
        {   974, 63279 },   // Earth Shield
        { 16166, 55452 },   // Elemental Mastery
        { 53301, 63066 },   // Explosive Shot
        { 51533, 63271 },   // Feral Spirit
        { 49143, 58647 },   // Frost Strike
        { 14278, 56814 },   // Ghostly Strike
        { 53595, 63219 },   // Hammer of the Righteous
        { 48181, 63302 },   // Haunt
        { 55050, 58616 },   // Heart Strike
        { 16511, 56807 },   // Hemorrhage
        { 20473, 63224 },   // Holy Shock
        { 49184, 63335 },   // Howling Blast
        { 51662, 63249 },   // Hunger For Blood
        { 49203, 63331 },   // Hungering Cold
        { 11426, 63095 },   // Ice Barrier
        { 12472, 56374 },   // Icy Veins
        { 51690, 63252 },   // Killing Spree
        { 12975, 58376 },   // Last Stand
        { 60103, 55444 },   // Lava Lash
        {   724, 55673 },   // Lightwell
        { 44457, 63091 },   // Living Bomb
        { 59672, 63303 },   // Metamorphosis
        { 15407, 55687 },   // Mind Flay
        { 12294, 58368 },   // Mortal Strike
        {  1329, 63268 },   // Mutilate
        { 33206, 63248 },   // Pain Suppression
        { 47540, 63235 },   // Penance
        { 14185, 56819 },   // Preparation
        { 61295, 63273 },   // Riptide
        { 48982, 59327 },   // Rune Tap
        { 19503, 63069 },   // Scatter Shot
        { 20375, 54925 },   // Seal of Command
        { 51713, 63253 },   // Shadow Dance
        { 17877, 56229 },   // Shadowburn
        { 46968, 63325 },   // Shockwave
        { 63108, 56216 },   // Siphon Life
        { 19028, 63312 },   // Soul Link
        { 20711, 55685 },   // Spirit of Redemption
        { 31785, 54924 },   // Spiritual Attunement
        { 48505, 54828 },   // Starfall
        { 17364, 55446 },   // Stormstrike
        { 61336, 65243 },   // Survival Instincts
        { 12328, 58384 },   // Sweeping Strikes
        { 18562, 54824 },   // Swiftmend
        { 30706, 63280 },   // Totem of Wrath
        { 19506, 56842 },   // Trueshot Aura
        { 51271, 58635 },   // Unbreakable Armor
        { 49194, 63332 },   // Unholy Blight
        { 30108, 56233 },   // Unstable Affliction
        { 55233, 58676 },   // Vampiric Blood
        { 14983, 56805 },   // Vigor
        { 48438, 62970 },   // Wild Growth
        { 19386, 56848 },   // Wyvern Sting
    };

    bool KnowsTalent(Player* player, uint32 firstRank)
    {
        for (uint32 rank = firstRank; rank; rank = sSpellMgr->GetNextSpellInChain(rank))
            if (player->HasSpell(rank))
                return true;
        return false;
    }

    void Sync(Player* player, uint32 firstRank, uint32 glyph)
    {
        bool knows = KnowsTalent(player, firstRank);
        if (knows && !player->HasAura(glyph))
            player->AddAura(glyph, player);
        else if (!knows && player->HasAura(glyph))
            player->RemoveAurasDueToSpell(glyph);
    }

    void SyncSpell(Player* player, uint32 spellId)
    {
        uint32 first = sSpellMgr->GetFirstSpellInChain(spellId);
        if (auto itr = TALENT_GLYPHS.find(first ? first : spellId); itr != TALENT_GLYPHS.end())
            Sync(player, itr->first, itr->second);
    }
}

class titan_talent_glyphs : public PlayerScript
{
public:
    titan_talent_glyphs() : PlayerScript("titan_talent_glyphs", {
        PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LEARN_SPELL, PLAYERHOOK_ON_FORGOT_SPELL }) { }

    void OnPlayerLogin(Player* player) override
    {
        for (auto const& [firstRank, glyph] : TALENT_GLYPHS)
            Sync(player, firstRank, glyph);
    }

    void OnPlayerLearnSpell(Player* player, uint32 spellId) override
    {
        SyncSpell(player, spellId);
    }

    void OnPlayerForgotSpell(Player* player, uint32 spellId) override
    {
        SyncSpell(player, spellId);
    }
};

void AddSC_talent_glyphs()
{
    new titan_talent_glyphs();
}
