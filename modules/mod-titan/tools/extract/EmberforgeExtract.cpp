// EmberforgeExtract: copies the files Emberforge needs out of a Mists of Pandaria (or Cataclysm) client's archives,
// without anyone opening them by hand. Built for Windows against StormLib (github.com/ladislav-zezula/StormLib), which
// reads the archives and applies the client's update archives on top, as the game does.
//
//     EmberforgeExtract.exe "C:\...\Data" [patterns.txt|-] [locale] [part size in MB]
//
// Only one language's files are read (enUS unless named): a client with several (deDE, frFR...) would otherwise mix
// them. Each base archive (Data\*.MPQ, Data\<locale>\*.MPQ, and the locale's own archives anywhere in Data:
// locale-enUS.MPQ, speech-enUS.MPQ...) is opened with every update archive of its folder
// (wow-update-*.MPQ, oldest first) applied as a patch. Every file matching a pattern (wildcards * and ?, one pattern a
// line; built-in ones without a file) is written under EmberforgeExtract\ next to where it is run, at its own path,
// the newest version winning. A manifest of what was written goes with it.
//
// A model brings what it needs with it: every .m2 written (3.3.5a's MD20) is read for its skins (<name>00.skin...),
// its animation files (<name>*.anim) and the textures it names, and those are extracted too (and theirs, in turn).
//
// The same files are then packed into zips small enough to upload (EmberforgeExtract-part1.zip..., 24 MB each unless
// named), in the patterns' order: what is needed first (the Dungeon Journal's tables and art) is in part 1.

#include <StormLib.h>
#include "Archives.h"
#include <windows.h>
#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

// In the order they are needed: the zips are filled in this order.
static char const* const DEFAULT_PATTERNS[] = {
    "DBFilesClient\\Journal*",
    "DBFilesClient\\DungeonEncounter.dbc",
    "DBFilesClient\\Map.dbc",
    "DBFilesClient\\MapDifficulty.dbc",
    "DBFilesClient\\LfgDungeons.dbc",
    "DBFilesClient\\AreaTable.dbc",
    "Interface\\EncounterJournal\\*",
    "Interface\\LFGFrame\\*",
    "Interface\\PaperDollInfoFrame\\*",
    "DBFilesClient\\*",
    "Interface\\Glues\\LoadingScreens\\*",
};

struct Extracted
{
    std::string name;       // its path in the archives
    fs::path path;          // where it was written
    std::size_t priority;   // the first pattern it matched
};

// A zip of the files, deflated (or stored when that is no smaller), split into parts of at most `limit` bytes.
class ZipParts
{
public:
    ZipParts(fs::path folder, std::uintmax_t limit) : _folder(std::move(folder)), _limit(limit) { }
    ~ZipParts() { Close(); }

    // Returns the part it went into.
    unsigned Add(std::string name, std::vector<unsigned char> const& data)
    {
        std::replace(name.begin(), name.end(), '\\', '/');
        uLong crc = crc32(0L, data.data(), uInt(data.size()));
        std::vector<unsigned char> packed(deflateBound(nullptr, uLong(data.size())) + 64);
        z_stream z = {};
        deflateInit2(&z, 9, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
        z.next_in = const_cast<Bytef*>(data.data());
        z.avail_in = uInt(data.size());
        z.next_out = packed.data();
        z.avail_out = uInt(packed.size());
        deflate(&z, Z_FINISH);
        packed.resize(z.total_out);
        deflateEnd(&z);
        bool stored = packed.size() >= data.size();
        std::vector<unsigned char> const& body = stored ? data : packed;

        std::uintmax_t entry = 30 + name.size() + body.size() + 46 + name.size();
        if (!_stream.is_open() || (_entries.size() && _size + entry + 22 > _limit))
            Open();
        Entry e = { name, uint32_t(crc), uint32_t(body.size()), uint32_t(data.size()), uint32_t(_size),
            uint16_t(stored ? 0 : 8) };
        Header(0x04034b50, e, false);
        _stream.write(reinterpret_cast<char const*>(body.data()), std::streamsize(body.size()));
        _size += 30 + name.size() + body.size();
        _entries.push_back(e);
        return _part;
    }

    unsigned Parts() const { return _part; }

    void Close()
    {
        if (!_stream.is_open())
            return;
        uint32_t start = uint32_t(_size), length = 0;
        for (Entry const& e : _entries)
        {
            Header(0x02014b50, e, true);
            length += uint32_t(46 + e.name.size());
        }
        Le32(0x06054b50);
        Le16(0);
        Le16(0);
        Le16(uint16_t(_entries.size()));
        Le16(uint16_t(_entries.size()));
        Le32(length);
        Le32(start);
        Le16(0);
        _stream.close();
        _entries.clear();
    }

private:
    struct Entry
    {
        std::string name;
        uint32_t crc, packed, size, offset;
        uint16_t method;
    };

    void Open()
    {
        Close();
        ++_part;
        _stream.open(_folder / ("EmberforgeExtract-part" + std::to_string(_part) + ".zip"), std::ios::binary);
        _size = 0;
    }

    void Le16(uint16_t v) { _stream.put(char(v & 0xFF)).put(char(v >> 8)); }
    void Le32(uint32_t v) { Le16(uint16_t(v & 0xFFFF)); Le16(uint16_t(v >> 16)); }

    // A local header (central: false) or a central directory entry (central: true).
    void Header(uint32_t signature, Entry const& e, bool central)
    {
        Le32(signature);
        if (central)
            Le16(20);                       // made by
        Le16(20);                           // needed to extract
        Le16(0);                            // flags
        Le16(e.method);
        Le16(0);                            // time
        Le16((2024 - 1980) << 9 | 1 << 5 | 1); // date
        Le32(e.crc);
        Le32(e.packed);
        Le32(e.size);
        Le16(uint16_t(e.name.size()));
        Le16(0);                            // extra
        if (central)
        {
            Le16(0);                        // comment
            Le16(0);                        // disk
            Le16(0);                        // internal attributes
            Le32(0);                        // external attributes
            Le32(e.offset);
        }
        _stream.write(e.name.data(), std::streamsize(e.name.size()));
    }

    fs::path _folder;
    std::uintmax_t _limit;
    std::ofstream _stream;
    std::uintmax_t _size = 0;
    unsigned _part = 0;
    std::vector<Entry> _entries;
};

static uint32_t U32(std::vector<char> const& d, std::size_t o)
{
    return o + 4 <= d.size() ? uint32_t(uint8_t(d[o])) | uint32_t(uint8_t(d[o + 1])) << 8
        | uint32_t(uint8_t(d[o + 2])) << 16 | uint32_t(uint8_t(d[o + 3])) << 24 : 0;
}

// What a 3.3.5a model (MD20) needs: its skins, its .anim files and the textures it names (type 0: a file).
static std::vector<std::string> ModelNeeds(fs::path const& path, std::string const& name)
{
    std::vector<std::string> needs;
    std::ifstream in(path, std::ios::binary);
    std::vector<char> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (d.size() < 0x60 || std::string(d.data(), 4) != "MD20")
        return needs;
    std::string base = name.substr(0, name.size() - 3);    // without ".m2"
    uint32_t views = U32(d, 0x44);
    for (uint32_t v = 0; v < std::max<uint32_t>(1, std::min<uint32_t>(views, 4)); ++v)
        needs.push_back(base + "0" + std::to_string(v) + ".skin");
    needs.push_back(base + "*.anim");
    uint32_t count = U32(d, 0x50), offset = U32(d, 0x54);
    for (uint32_t i = 0; i < count && i < 256; ++i)
    {
        std::size_t t = offset + i * 16;
        uint32_t type = U32(d, t), length = U32(d, t + 8), at = U32(d, t + 12);
        if (type == 0 && length > 1 && at + length <= d.size())
            needs.push_back(std::string(d.data() + at, length - 1));
    }
    return needs;
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
        std::printf("Usage: EmberforgeExtract.exe \"C:\\...\\Data\" [patterns.txt] [locale]\n");
        return 1;
    }
    // A path ending in a backslash before its closing quote ("...\\Data\\") reaches us with a stray quote.
    std::string dataArg = argv[1];
    while (!dataArg.empty() && (dataArg.back() == '"' || dataArg.back() == '\\' || dataArg.back() == '/'))
        dataArg.pop_back();
    fs::path data = dataArg;
    if (!fs::is_directory(data))
    {
        std::printf("ERROR: \"%s\" is not a folder. Give the client's Data folder (the one holding common.MPQ).\n",
            data.string().c_str());
        return 1;
    }
    std::vector<std::string> patterns;
    if (argc > 3)
        g_locale = Lower(argv[3]);
    std::uintmax_t partSize = (argc > 4 ? std::max(1, std::atoi(argv[4])) : 24) * std::uintmax_t(1024 * 1024);
    if (argc > 2 && std::string(argv[2]) != "-")
    {
        std::ifstream list(argv[2]);
        if (!list)
        {
            std::printf("ERROR: could not open the list \"%s\". Is the repository up to date (git pull)?\n", argv[2]);
            return 1;
        }
        for (std::string line; std::getline(list, line);)
        {
            line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
            while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
                line.pop_back();
            if (line.size() >= 3 && uint8_t(line[0]) == 0xEF && uint8_t(line[1]) == 0xBB && uint8_t(line[2]) == 0xBF)
                line.erase(0, 3);   // a UTF-8 mark at the start of the file
            if (!line.empty() && line[0] != '#')
                patterns.push_back(line);
        }
    }
    else
        patterns.assign(std::begin(DEFAULT_PATTERNS), std::end(DEFAULT_PATTERNS));

    // The folders holding archives: the locale's folder first (its files win), then Data itself; the other
    // languages' folders are left out.
    std::vector<fs::path> folders;
    for (auto const& entry : fs::directory_iterator(data))
        if (entry.is_directory() && Lower(entry.path().filename().string()) == g_locale)
            folders.push_back(entry.path());
    folders.push_back(data);
    std::printf("Language: %s\n%zu patterns to look for\n", g_locale.c_str(), patterns.size());
    std::size_t archiveCount = 0;
    for (fs::path const& folder : folders)
        archiveCount += Archives(folder, false).size();
    if (!archiveCount)
    {
        std::printf("ERROR: no .MPQ archives in %s (or its %s folder). Give the client's Data folder.\n",
            data.string().c_str(), g_locale.c_str());
        return 1;
    }
    std::printf("%zu archives found\n", archiveCount);

    fs::path outRoot = fs::current_path() / "EmberforgeExtract";
    std::map<std::string, std::string> written;    // lowercase path -> archive it came from
    std::vector<Extracted> extracted;
    fs::create_directories(outRoot);

    // One pass over every archive for the patterns from `first` on (later passes: what the models found need).
    auto extract = [&](std::size_t first)
    {
        for (fs::path const& folder : folders)
        {
            std::vector<fs::path> updates = Archives(folder, true);
            // The highest ranked first (the newest patch, the later expansion): its copy of a file wins.
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
                for (std::size_t priority = first; priority < patterns.size(); ++priority)
                {
                    std::string const& pattern = patterns[priority];
                    // An exact name is opened directly: an archive's file list may leave it out.
                    if (pattern.find_first_of("*?") == std::string::npos)
                    {
                        std::string key = Lower(pattern);
                        if (written.count(key) || !SFileHasFile(archive, pattern.c_str()))
                            continue;
                        std::string relative = pattern;
                        std::replace(relative.begin(), relative.end(), '\\', '/');
                        if (Write(archive, pattern, outRoot / fs::path(relative)))
                        {
                            written[key] = base.filename().string();
                            extracted.push_back({ pattern, outRoot / fs::path(relative), priority });
                            ++count;
                        }
                        continue;
                    }
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
                            extracted.push_back({ name, outRoot / fs::path(relative), priority });
                            ++count;
                        }
                    } while (SFileFindNextFile(find, &found));
                    SFileFindClose(find);
                }
                std::printf("%-40s %u files\n", base.filename().string().c_str(), count);
                SFileCloseArchive(archive);
            }
        }
    };
    std::size_t done = 0, first = 0;
    std::size_t asked = patterns.size();
    for (int round = 0; round < 4 && first < patterns.size(); ++round)
    {
        extract(first);
        if (round == 0)
        {
            // What the list asked for that no archive has (a wrong name, or another client's file).
            std::set<std::size_t> hit;
            for (Extracted const& e : extracted)
                hit.insert(e.priority);
            std::size_t missing = 0;
            for (std::size_t i = 0; i < asked; ++i)
                if (!hit.count(i) && missing++ < 30)
                    std::printf("  not found: %s\n", patterns[i].c_str());
            if (missing)
                std::printf("%zu of the list's %zu patterns matched nothing.\n", missing, asked);
        }
        first = patterns.size();
        // What the models just written need, not yet written: the next pass's patterns.
        std::set<std::string> needs;
        for (std::size_t i = done; i < extracted.size(); ++i)
        {
            std::string name = extracted[i].name;
            if (Lower(fs::path(name).extension().string()) != ".m2")
                continue;
            for (std::string const& need : ModelNeeds(extracted[i].path, name))
                if (!written.count(Lower(need)) && needs.insert(Lower(need)).second)
                    patterns.push_back(need);
        }
        done = extracted.size();
        if (first < patterns.size())
            std::printf("\nThe models need %zu more files: extracting them.\n", patterns.size() - first);
    }
    // The zips, in the patterns' order; the manifest says which part holds each file.
    std::stable_sort(extracted.begin(), extracted.end(), [](Extracted const& a, Extracted const& b) {
        return a.priority != b.priority ? a.priority < b.priority : Lower(a.name) < Lower(b.name);
    });
    fs::path zips = fs::current_path();
    for (auto const& entry : fs::directory_iterator(zips))
    {
        std::string old = Lower(entry.path().filename().string());
        if (old.rfind("emberforgeextract-part", 0) == 0 && entry.path().extension() == ".zip")
            fs::remove(entry.path());
    }
    if (extracted.empty())
    {
        std::printf("\nNothing was found: no file matched the list. Send this window's text to see why.\n");
        return 1;
    }
    std::ofstream manifest(zips / "manifest.txt");
    {
        ZipParts parts(zips, partSize);
        for (Extracted const& file : extracted)
        {
            std::ifstream in(file.path, std::ios::binary);
            std::vector<unsigned char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            unsigned part = parts.Add(file.name, data);
            manifest << "part" << part << "\t" << file.name << "\t" << written[Lower(file.name)] << "\n";
        }
        parts.Close();
        std::printf("\nWrote %zu files to %s\nand packed them into %u zips: EmberforgeExtract-part1.zip...\n"
            "Send them one by one, part 1 first (it holds what the list asks for first).\n", extracted.size(),
            outRoot.string().c_str(), parts.Parts());
    }
    return 0;
}
