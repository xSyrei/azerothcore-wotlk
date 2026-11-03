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
// The same files are then packed into zips small enough to upload (EmberforgeExtract-part1.zip..., 24 MB each unless
// named), in the patterns' order: what is needed first (the Dungeon Journal's tables and art) is in part 1.

#include <StormLib.h>
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

namespace fs = std::filesystem;

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

static std::string g_locale = "enus";

// Another language's archive (locale-deDE.MPQ, speech-frFR.MPQ, wow-update-deDE-...): skipped.
static bool OtherLocale(fs::path const& path)
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

static std::vector<fs::path> Archives(fs::path const& folder, bool updates)
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
        std::printf("Usage: EmberforgeExtract.exe \"C:\\...\\Data\" [patterns.txt] [locale]\n");
        return 1;
    }
    fs::path data = argv[1];
    std::vector<std::string> patterns;
    if (argc > 3)
        g_locale = Lower(argv[3]);
    std::uintmax_t partSize = (argc > 4 ? std::max(1, std::atoi(argv[4])) : 24) * std::uintmax_t(1024 * 1024);
    if (argc > 2 && std::string(argv[2]) != "-")
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

    // The folders holding archives: the locale's folder first (its files win), then Data itself; the other
    // languages' folders are left out.
    std::vector<fs::path> folders;
    for (auto const& entry : fs::directory_iterator(data))
        if (entry.is_directory() && Lower(entry.path().filename().string()) == g_locale)
            folders.push_back(entry.path());
    folders.push_back(data);
    std::printf("Language: %s\n", g_locale.c_str());

    fs::path outRoot = fs::current_path() / "EmberforgeExtract";
    std::map<std::string, std::string> written;    // lowercase path -> archive it came from
    std::vector<Extracted> extracted;
    fs::create_directories(outRoot);

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
            for (std::size_t priority = 0; priority < patterns.size(); ++priority)
            {
                std::string const& pattern = patterns[priority];
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
            "Send part 1 first (the Dungeon Journal's files), then the others one by one.\n", extracted.size(),
            outRoot.string().c_str(), parts.Parts());
    }
    return 0;
}
