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
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"

// A Hero's pets and demons have all their abilities from level 1: the first rank of each (the Felhunter's Devour
// Magic, the Succubus's Seduction, a cat's Dash), later ranks at their levels as usual. The core teaches a pet its
// abilities by level (Pet::InitLevelupSpellsForLevel) and takes back those above it; for a Hero's pet it keeps the
// first ranks.
class titan_pet_abilities : public PetScript
{
public:
    titan_pet_abilities() : PetScript("titan_pet_abilities", {
        PETHOOK_ON_PET_ADD_TO_WORLD,
        PETHOOK_CAN_UNLEARN_SPELL_SET,
        PETHOOK_CAN_UNLEARN_SPELL_DEFAULT
    }) { }

    static bool HeroPet(Pet* pet)
    {
        Player* owner = pet->GetOwner();
        return owner && Wildcard::IsHero(owner);
    }

    static bool FirstRank(uint32 spellId)
    {
        return sSpellMgr->GetFirstSpellInChain(spellId) == spellId;
    }

    void OnPetAddToWorld(Pet* pet) override
    {
        if (!HeroPet(pet))
            return;

        CreatureTemplate const* info = pet->GetCreatureTemplate();
        if (PetLevelupSpellSet const* spells = info->family ? sSpellMgr->GetPetLevelupSpellList(info->family) : nullptr)
            for (auto const& [level, spellId] : *spells)
                if (FirstRank(spellId) && !pet->HasSpell(spellId))
                    pet->learnSpell(spellId);

        int32 defaultsId = info->PetSpellDataId ? -int32(info->PetSpellDataId) : int32(pet->GetEntry());
        if (PetDefaultSpellsEntry const* defaults = sSpellMgr->GetPetDefaultSpellsEntry(defaultsId))
            for (uint32 spellId : defaults->spellid)
                if (spellId && sSpellMgr->GetSpellInfo(spellId) && FirstRank(spellId) && !pet->HasSpell(spellId))
                    pet->learnSpell(spellId);
    }

    bool CanUnlearnSpellSet(Pet* pet, uint32 /*level*/, uint32 spellId) override
    {
        return !HeroPet(pet) || !FirstRank(spellId);
    }

    bool CanUnlearnSpellDefault(Pet* pet, SpellInfo const* spellInfo) override
    {
        return !HeroPet(pet) || !FirstRank(spellInfo->Id);
    }
};

void AddSC_pets()
{
    new titan_pet_abilities();
}
