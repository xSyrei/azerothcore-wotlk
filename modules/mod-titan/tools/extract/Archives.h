// The client's archives as the game reads them (EmberforgeExtract, EmberforgeArchive): one language, each base archive
// with its folder's update archives applied as patches, the highest ranked first.
#pragma once

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

inline std::string Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return text;
}

inline bool IsUpdate(fs::path const& path)
{
    return Lower(path.filename().string()).rfind("wow-update-", 0) == 0;
}

// The update archives' build number (wow-update-base-16016.MPQ: 16016), to apply them oldest first.
inline int UpdateNumber(fs::path const& path)
{
    std::string name = path.stem().string();
    std::size_t dash = name.rfind('-');
    return dash == std::string::npos ? 0 : std::atoi(name.c_str() + dash + 1);
}

inline std::string g_locale = "enus";

// Another language's archive (locale-deDE.MPQ, speech-frFR.MPQ, wow-update-deDE-...): skipped.
inline bool OtherLocale(fs::path const& path)
{
    static char const* const LOCALES[] = { "enus", "engb", "dede", "frfr", "eses", "esmx", "ruru", "kokr", "zhcn",
        "zhtw", "ptbr", "itit", "ptpt", "encn" };
    std::string name = Lower(path.filename().string());
    for (char const* locale : LOCALES)
    {
        std::string code = Lower(locale);
        if (code != g_locale && name.find(code) != std::string::npos)
            return true;
    }
    return false;
}

// A base archive's rank, the highest read first: the 3.3.5a client's patches (patch.MPQ, patch-2.MPQ...,
// patch-enUS.MPQ, patch-enUS-2.MPQ..., then letters: patch-A.MPQ...) above everything else, in their own order
// (by name, "patch.MPQ" would come after "patch-2.MPQ"); the others by name (lichking above expansion above
// common-2 above common; Mists of Pandaria's patch through its update archives instead).
inline std::pair<int, std::string> Rank(fs::path const& path)
{
    std::string name = Lower(path.stem().string());
    if (name.rfind("patch", 0) != 0)
        return { 0, name };
    std::string rest = name.substr(5);
    if (rest.size() >= 5 && rest[0] == '-' && rest.substr(1, 4) == g_locale)
        rest = rest.substr(5);          // patch-enUS-2: its number, as patch-2
    if (rest.empty())
        return { 1, "" };
    if (rest[0] == '-')
        rest = rest.substr(1);
    // Digits, then letters, each by value: "2" < "9" < "a" < "z" (a longer number above a shorter one).
    return { 2, std::string(1, char('0' + std::min<std::size_t>(rest.size(), 9))) + rest };
}

inline std::vector<fs::path> Archives(fs::path const& folder, bool updates)
{
    std::vector<fs::path> result;
    if (!fs::is_directory(folder))
        return result;
    for (auto const& entry : fs::directory_iterator(folder))
        if (entry.is_regular_file() && Lower(entry.path().extension().string()) == ".mpq"
            && IsUpdate(entry.path()) == updates && !OtherLocale(entry.path()))
            result.push_back(entry.path());
    if (updates)
        std::sort(result.begin(), result.end(),
            [](fs::path const& a, fs::path const& b) { return UpdateNumber(a) < UpdateNumber(b); });
    else
        std::sort(result.begin(), result.end(),
            [](fs::path const& a, fs::path const& b) { return Rank(a) < Rank(b); });
    return result;
}

