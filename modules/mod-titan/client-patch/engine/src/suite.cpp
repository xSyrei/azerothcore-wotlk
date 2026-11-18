// The Emberforge Effect Suite from the game: the server's .ees command (accounts of the highest clearance only) sends
// opcode 0x51B (another client-to-server opcode the client never handles), and the engine opens the Suite: it runs
// the launcher (its path in <game>\Emberforge\launcher.path, written by the launcher) with --ees. The launcher, one
// at a time, opens its Suite window, which reads this game's own files and keeps effects in <game>\Emberforge\Effects.
// Set up with the spell modifiers' handler (spellmods.cpp: same connection, same Wow.exe 3.3.5a (12340) check).
#include <shellapi.h>

namespace
{
    constexpr uint32_t SUITE_OPCODE = 0x51B;

    std::wstring Widen(std::string const &text)
    {
        int n = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), int(text.size()), nullptr, 0);
        std::wstring out(n > 0 ? n : 0, L'\0');
        if (n > 0)
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), int(text.size()), &out[0], n);
        return out;
    }

    int __cdecl OnOpenSuite(void * /*param*/, uint32_t /*opcode*/, uint32_t /*time*/, void * /*message*/)
    {
        static DWORD last = 0;
        DWORD now = GetTickCount();
        if (last && now - last < 3000)
            return 1;                                       // one window, however often it is asked
        last = now;
        std::string launcher;
        if (FILE *file = fopen((GameFolder() + "Emberforge\\launcher.path").c_str(), "rb"))
        {
            char buffer[1024] = {};
            size_t n = fread(buffer, 1, sizeof(buffer) - 1, file);
            fclose(file);
            launcher.assign(buffer, n);
            while (!launcher.empty() && (launcher.back() == '\r' || launcher.back() == '\n' || launcher.back() == ' '))
                launcher.pop_back();
        }
        if (launcher.empty())
        {
            Log("effect suite: no Emberforge\\launcher.path (start the game from the launcher once)");
            return 1;
        }
        auto result = reinterpret_cast<intptr_t>(ShellExecuteW(nullptr, L"open", Widen(launcher).c_str(), L"--ees",
                                                               nullptr, SW_SHOWNORMAL));
        Log("effect suite: launcher started (%d)", int(result));
        return 1;
    }
}

namespace Suite
{
    // Each frame, with the spell modifiers' handler: the Suite's opcode listened for while the game is connected.
    void Tick()
    {
        if (!g_installed)
            return;
        uintptr_t connection = *reinterpret_cast<uintptr_t *>(CONNECTION);
        if (!connection)
            return;
        auto *slot = reinterpret_cast<Handler *>(connection + HANDLERS + SUITE_OPCODE * 4);
        if (*slot != &OnOpenSuite)
            reinterpret_cast<SetHandler>(SET_HANDLER)(SUITE_OPCODE, &OnOpenSuite, nullptr);
    }
}
