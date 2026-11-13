// Spell modifiers by class family.
//
// The client keeps the server's spell modifiers (SMSG_SET_FLAT_SPELL_MODIFIER / SMSG_SET_PCT_SPELL_MODIFIER: a flag
// bit, an op, a total) in two tables, [96 bits][31 ops], and its lookup (Wow.exe 0x7FD970, called for every cost,
// cooldown, cast time, range, duration and tooltip number) applies them only to spells of its own class's family
// (ChrClasses' SpellClassSet, 0xD397B4): the packets carry no family. A classless Hero is a Paladin to the client,
// so their talents for another class's abilities never showed: Thunder Clap's cost cut stayed full in the tooltip,
// the button and the client's own "not enough rage" check, though the server charged the cut.
//
// The server (Player::SendSpellModifiersByFamily) sends every modifier the player has, totalled by family, bit, op
// and kind, on opcode 0x51A (a client-to-server opcode the client never handles: without this engine it is dropped).
// The lookup is replaced by one keyed by the spell's own family (and family 0's, which the server applies to every
// family), returning what the original did. Until a list has come (another server), the original runs.
//
// Wow.exe 3.3.5a (12340) addresses, found in its code: 0x7FD970 the lookup (cdecl: spell record, op, int *flat,
// int *pct; returns whether any applied; the record's SpellFamilyName at +0x240, SpellFamilyFlags at +0x244,
// AttributesEx3 at +0x1C), 0x6B0B80 ClientServices::SetMessageHandler (cdecl: opcode, handler, param), 0xC79CF4 the
// connection (its handlers at +0x53C + opcode * 4), 0x47B340 / 0x47B3C0 CDataStore::Get (uint8 / uint32, thiscall).
#include "spellmods.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

namespace
{
    void Log(const char *format, ...);

    constexpr uintptr_t LOOKUP = 0x7FD970;
    constexpr uintptr_t SET_HANDLER = 0x6B0B80;
    constexpr uintptr_t CONNECTION = 0xC79CF4;
    constexpr uintptr_t HANDLERS = 0x53C;
    constexpr uintptr_t GET_U8 = 0x47B340;
    constexpr uintptr_t GET_U32 = 0x47B3C0;
    constexpr uint32_t OPCODE = 0x51A;

    // The lookup's first bytes (push ebp; mov ebp, esp; push ecx; push ebx; mov ebx, [ebp+8]), checked before the
    // patch: another build's code is left alone.
    constexpr uint8_t PROLOGUE[8] = { 0x55, 0x8B, 0xEC, 0x51, 0x53, 0x8B, 0x5D, 0x08 };

    constexpr uint32_t FAMILIES = 256, BITS = 96, OPS = 31;
    constexpr uint32_t IGNORE_CASTER_MODIFIERS = 0x20000000;       // AttributesEx3

    using Lookup = bool(__cdecl *)(void *spell, int op, int *flat, int *pct);
    using Handler = int(__cdecl *)(void *param, uint32_t opcode, uint32_t time, void *message);
    using SetHandler = void(__cdecl *)(uint32_t opcode, Handler handler, void *param);
    using GetU8 = void *(__thiscall *)(void *store, uint8_t *out);
    using GetU32 = void *(__thiscall *)(void *store, uint32_t *out);

    struct Entry
    {
        int32_t Flat;
        int32_t Pct;
    };

    // Per family, the bits it has modifiers on (a list, not a table: a Hero has a few dozen).
    struct Mod
    {
        uint8_t Bit;
        uint8_t Op;
        Entry Value;
    };
    Mod *g_mods[FAMILIES] = {};
    uint32_t g_counts[FAMILIES] = {};
    bool g_active = false;          // a list has come from the server
    bool g_installed = false;
    Lookup g_original = nullptr;    // the original's first bytes, then a jump back into it

    void Clear()
    {
        for (uint32_t family = 0; family < FAMILIES; ++family)
        {
            delete[] g_mods[family];
            g_mods[family] = nullptr;
            g_counts[family] = 0;
        }
    }

    void Add(Entry &total, Mod const *mods, uint32_t count, uint32_t const *flags, int op)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            Mod const &mod = mods[i];
            if (mod.Op == op && (flags[mod.Bit / 32] & (1u << (mod.Bit % 32))))
            {
                total.Flat += mod.Value.Flat;
                total.Pct += mod.Value.Pct;
            }
        }
    }

    // The client's lookup, by the spell's own family: what the original returns, from every family's modifiers.
    bool __cdecl FamilyLookup(void *spell, int op, int *flat, int *pct)
    {
        if (!g_active)
            return g_original(spell, op, flat, pct);
        uint8_t const *record = static_cast<uint8_t const *>(spell);
        uint32_t family = 0, attributesEx3 = 0;
        memcpy(&family, record + 0x240, 4);
        memcpy(&attributesEx3, record + 0x1C, 4);
        if (!family || family >= FAMILIES || (attributesEx3 & IGNORE_CASTER_MODIFIERS) || op < 0
            || uint32_t(op) >= OPS)
        {
            *flat = 0;
            *pct = 100;
            return false;
        }
        uint32_t flags[3];
        memcpy(flags, record + 0x244, sizeof(flags));
        Entry total = { 0, 0 };
        Add(total, g_mods[family], g_counts[family], flags, op);
        Add(total, g_mods[0], g_counts[0], flags, op);      // the server's family 0: every family
        *flat = total.Flat;
        *pct = total.Pct;
        if (!total.Flat && !total.Pct)
            return false;                                   // as the original: no modifier, pct left 0
        *pct = total.Pct + 100 < 0 ? 0 : total.Pct + 100;
        return true;
    }

    // Opcode 0x51A: uint32 count, then count x (uint8 family, bit, op, kind (0 flat, 1 pct), int32 value): the whole
    // list, replacing the last.
    int __cdecl OnModifiers(void * /*param*/, uint32_t /*opcode*/, uint32_t /*time*/, void *message)
    {
        auto get8 = reinterpret_cast<GetU8>(GET_U8);
        auto get32 = reinterpret_cast<GetU32>(GET_U32);
        uint32_t count = 0;
        get32(message, &count);
        if (count > 20000)
            return 1;
        struct Raw
        {
            uint8_t Family, Bit, Op, Kind;
            int32_t Value;
        };
        Raw *raw = new Raw[count ? count : 1];
        uint32_t perFamily[FAMILIES] = {};
        for (uint32_t i = 0; i < count; ++i)
        {
            uint32_t value = 0;
            get8(message, &raw[i].Family);
            get8(message, &raw[i].Bit);
            get8(message, &raw[i].Op);
            get8(message, &raw[i].Kind);
            get32(message, &value);
            raw[i].Value = int32_t(value);
            if (raw[i].Bit < BITS && raw[i].Op < OPS)
                ++perFamily[raw[i].Family];
        }
        Clear();
        for (uint32_t family = 0; family < FAMILIES; ++family)
            if (perFamily[family])
                g_mods[family] = new Mod[perFamily[family]];
        for (uint32_t i = 0; i < count; ++i)
        {
            Raw const &r = raw[i];
            if (r.Bit >= BITS || r.Op >= OPS)
                continue;
            Mod &mod = g_mods[r.Family][g_counts[r.Family]++];
            mod.Bit = r.Bit;
            mod.Op = r.Op;
            mod.Value = { r.Kind ? 0 : r.Value, r.Kind ? r.Value : 0 };
        }
        delete[] raw;
        if (!g_active)
            Log("spell modifiers by family: first list (%u)", count);
        g_active = true;
        return 1;
    }
}

namespace SpellMods
{
    void Install()
    {
        // Wow.exe itself, the build these addresses are from: its lookup's first bytes are checked.
        if (reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)) != 0x400000
            || memcmp(reinterpret_cast<void *>(LOOKUP), PROLOGUE, sizeof(PROLOGUE)) != 0)
        {
            Log("spell modifiers by family: not Wow.exe 3.3.5a (12340), left alone");
            return;
        }
        // The original's first instructions, then a jump to the rest of it: what runs before a list comes.
        auto *trampoline = static_cast<uint8_t *>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE,
                                                                PAGE_EXECUTE_READWRITE));
        if (!trampoline)
            return;
        memcpy(trampoline, PROLOGUE, sizeof(PROLOGUE));
        trampoline[8] = 0xE9;
        int32_t back = int32_t(LOOKUP + sizeof(PROLOGUE) - (reinterpret_cast<uintptr_t>(trampoline) + 13));
        memcpy(trampoline + 9, &back, 4);
        g_original = reinterpret_cast<Lookup>(trampoline);

        // The lookup's start: a jump to ours.
        DWORD old = 0;
        auto *code = reinterpret_cast<uint8_t *>(LOOKUP);
        if (!VirtualProtect(code, 5, PAGE_EXECUTE_READWRITE, &old))
            return;
        int32_t to = int32_t(reinterpret_cast<uintptr_t>(&FamilyLookup) - (LOOKUP + 5));
        code[0] = 0xE9;
        memcpy(code + 1, &to, 4);
        VirtualProtect(code, 5, old, &old);
        FlushInstructionCache(GetCurrentProcess(), code, 5);
        g_installed = true;
        Log("spell modifiers by family: lookup replaced");
    }

    void Tick()
    {
        if (!g_installed)
            return;
        // The handler, set again while there is a connection (the game clears and sets its own as it logs in and
        // out; with none, SetMessageHandler is a fatal error).
        uintptr_t connection = *reinterpret_cast<uintptr_t *>(CONNECTION);
        if (!connection)
            return;
        auto *slot = reinterpret_cast<Handler *>(connection + HANDLERS + OPCODE * 4);
        if (*slot != &OnModifiers)
            reinterpret_cast<SetHandler>(SET_HANDLER)(OPCODE, &OnModifiers, nullptr);
    }
}
