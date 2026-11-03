// EmberforgeExtract: copies the files Emberforge needs out of a Mists of Pandaria (or Cataclysm) client's archives,
// without anyone opening them by hand. Built for Windows against StormLib (github.com/ladislav-zezula/StormLib), which
// reads the archives and applies the client's update archives on top, as the game does.
//
//     EmberforgeExtract.exe "C:\...\Data" [patterns.txt]
//
// Each base archive (Data\*.MPQ, Data\<locale>\*.MPQ) is opened with every update archive of its folder
// (wow-update-*.MPQ, oldest first) applied as a patch. Every file matching a pattern (wildcards * and ?, one pattern a
// line; built-in ones without a file) is written under EmberforgeExtract\ next to where it is run, at its own path,
// the newest version winning. A manifest of what was written goes with it.

#include <StormLib.h>
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static char const* const DEFAULT_PATTERNS[] = {
    "Interface\\EncounterJournal\\*",
    "Interface\\PaperDollInfoFrame\\*",
    "Interface\\LFGFrame\\*",
    "Interface\\Glues\\LoadingScreens\\*",
    "DBFilesClient\\*",
};

static std::string Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return text;
}

static bool IsUpdate(fs::path const& path)
{
    return Lower(path.filename().string()).rfind("wow-update-", 0) == 0;
}

// The update archives' build number (wow-update-base-16016.MPQ: 16016), to apply them oldest first.
static int UpdateNumber(fs::path const& path)
{
    std::string name = path.stem().string();
    std::size_t dash = name.rfind('-');
    return dash == std::string::npos ? 0 : std::atoi(name.c_str() + dash + 1);
}

static std::vector<fs::path> Archives(fs::path const& folder, bool updates)
{
    std::vector<fs::path> result;
    if (!fs::is_directory(folder))
        return result;
    for (auto const& entry : fs::directory_iterator(folder))
        if (entry.is_regular_file() && Lower(entry.path().extension().string()) == ".mpq"
            && IsUpdate(entry.path()) == updates)
            result.push_back(entry.path());
    if (updates)
        std::sort(result.begin(), result.end(),
            [](fs::path const& a, fs::path const& b) { return UpdateNumber(a) < UpdateNumber(b); });
    else
        std::sort(result.begin(), result.end());
    return result;
}

static bool Write(HANDLE archive, std::string const& name, fs::path const& out)
{
    HANDLE file = nullptr;
    if (!SFileOpenFileEx(archive, name.c_str(), SFILE_OPEN_FROM_MPQ, &file))
        return false;
    DWORD high = 0;
    DWORD size = SFileGetFileSize(file, &high);
    std::vector<char> data(size);
    DWORD read = 0;
    bool ok = size == 0 || (SFileReadFile(file, data.data(), size, &read, nullptr) && read == size);
    SFileCloseFile(file);
    if (!ok)
        return false;
    fs::create_directories(out.parent_path());
    std::ofstream stream(out, std::ios::binary);
    stream.write(data.data(), std::streamsize(data.size()));
    return bool(stream);
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf("Usage: EmberforgeExtract.exe \"C:\\...\\Data\" [patterns.txt]\n");
        return 1;
    }
    fs::path data = argv[1];
    std::vector<std::string> patterns;
    if (argc > 2)
    {
        std::ifstream list(argv[2]);
        for (std::string line; std::getline(list, line);)
        {
            line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
            if (!line.empty() && line[0] != '#')
                patterns.push_back(line);
        }
    }
    else
        patterns.assign(std::begin(DEFAULT_PATTERNS), std::end(DEFAULT_PATTERNS));

    // The folders holding archives: Data itself and its locale folders (enUS...), each with its own updates.
    std::vector<fs::path> folders = { data };
    for (auto const& entry : fs::directory_iterator(data))
        if (entry.is_directory() && !Archives(entry.path(), false).empty())
            folders.push_back(entry.path());

    fs::path outRoot = fs::current_path() / "EmberforgeExtract";
    std::map<std::string, std::string> written;    // lowercase path -> archive it came from
    std::ofstream manifest;
    fs::create_directories(outRoot);
    manifest.open(outRoot / "manifest.txt");

    for (fs::path const& folder : folders)
    {
        std::vector<fs::path> updates = Archives(folder, true);
        // Later expansions' archives first: their copy of a file wins.
        std::vector<fs::path> bases = Archives(folder, false);
        std::reverse(bases.begin(), bases.end());
        for (fs::path const& base : bases)
        {
            HANDLE archive = nullptr;
            if (!SFileOpenArchive(base.string().c_str(), 0, STREAM_FLAG_READ_ONLY, &archive))
            {
                std::printf("Could not open %s\n", base.string().c_str());
                continue;
            }
            for (fs::path const& update : updates)
                SFileOpenPatchArchive(archive, update.string().c_str(), nullptr, 0);

            unsigned count = 0;
            for (std::string const& pattern : patterns)
            {
                SFILE_FIND_DATA found;
                HANDLE find = SFileFindFirstFile(archive, pattern.c_str(), &found, nullptr);
                if (!find)
                    continue;
                do
                {
                    std::string name = found.cFileName;
                    std::string key = Lower(name);
                    if (written.count(key))
                        continue;
                    std::string relative = name;
                    std::replace(relative.begin(), relative.end(), '\\', '/');
                    if (Write(archive, name, outRoot / fs::path(relative)))
                    {
                        written[key] = base.filename().string();
                        manifest << name << "\t" << base.filename().string() << "\n";
                        ++count;
                    }
                } while (SFileFindNextFile(find, &found));
                SFileFindClose(find);
            }
            std::printf("%-40s %u files\n", base.filename().string().c_str(), count);
            SFileCloseArchive(archive);
        }
    }
    std::printf("\nWrote %zu files to %s\nZip that folder and send it.\n", written.size(), outRoot.string().c_str());
    return 0;
}
