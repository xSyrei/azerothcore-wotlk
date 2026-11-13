// Spell modifiers by class family (spellmods.cpp): a classless Hero's talents for every class's spells, in the
// client's costs, cooldowns, cast times and tooltips.
#pragma once

namespace SpellMods
{
    // Wow.exe 3.3.5a (12340) only: the client's spell modifier lookup replaced (DllMain).
    void Install();
    // Each frame: the server's list (opcode 0x51A) listened for while the game is connected.
    void Tick();
}
