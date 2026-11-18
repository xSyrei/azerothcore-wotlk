// EmberforgeArchive: the client's files on request, for the Emberforge Effect Suite (the launcher runs it). Built like
// EmberforgeExtract (StormLib), it opens the client's archives once, as the game reads them (Archives.h), and answers
// one request a line on its standard input:
//
//     R <path>        a file: its size on a line, then its bytes ("-1" when no archive has it)
//     F <pattern>     the files matching (wildcards * and ?), one a line, then an empty line
//
//     EmberforgeArchive.exe "C:\...\Data" [locale]
//
// It prints "READY <archives>" once the archives are open, and leaves when its input closes.

#include <StormLib.h>
#include "Archives.h"

#include <fcntl.h>
#include <io.h>

#include <cstdio>
#include <iostream>
#include <set>

int main(int argc, char** argv)
{
    _setmode(_fileno(stdout), _O_BINARY);
    if (argc < 2)
    {
        std::printf("ERROR usage: EmberforgeArchive.exe <Data folder> [locale]\n");
        return 1;
    }
    std::string dataArg = argv[1];
    while (!dataArg.empty() && (dataArg.back() == '"' || dataArg.back() == '\\' || dataArg.back() == '/'))
        dataArg.pop_back();
    fs::path data = dataArg;
    if (argc > 2)
        g_locale = Lower(argv[2]);
    if (!fs::is_directory(data))
    {
        std::printf("ERROR not a folder: %s\n", data.string().c_str());
        return 1;
    }

    // The locale's folder first (its files win), then Data; within each, the highest ranked base archive first.
    std::vector<fs::path> folders;
    for (auto const& entry : fs::directory_iterator(data))
        if (entry.is_directory() && Lower(entry.path().filename().string()) == g_locale)
            folders.push_back(entry.path());
    folders.push_back(data);
    std::vector<HANDLE> archives;
    for (fs::path const& folder : folders)
    {
        std::vector<fs::path> updates = Archives(folder, true);
        std::vector<fs::path> bases = Archives(folder, false);
        std::reverse(bases.begin(), bases.end());
        for (fs::path const& base : bases)
        {
            HANDLE archive = nullptr;
            if (!SFileOpenArchive(base.string().c_str(), 0, STREAM_FLAG_READ_ONLY, &archive))
                continue;
            for (fs::path const& update : updates)
                SFileOpenPatchArchive(archive, update.string().c_str(), nullptr, 0);
            archives.push_back(archive);
        }
    }
    if (archives.empty())
    {
        std::printf("ERROR no archives in %s\n", data.string().c_str());
        return 1;
    }
    std::printf("READY %zu\n", archives.size());
    std::fflush(stdout);

    for (std::string line; std::getline(std::cin, line);)
    {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        if (line.size() < 3)
            continue;
        std::string arg = line.substr(2);
        if (line[0] == 'R')
        {
            bool sent = false;
            for (HANDLE archive : archives)
            {
                HANDLE file = nullptr;
                if (!SFileOpenFileEx(archive, arg.c_str(), SFILE_OPEN_FROM_MPQ, &file))
                    continue;
                DWORD high = 0, size = SFileGetFileSize(file, &high), read = 0;
                std::vector<char> bytes(size);
                bool ok = size == 0 || (SFileReadFile(file, bytes.data(), size, &read, nullptr) && read == size);
                SFileCloseFile(file);
                if (!ok)
                    continue;
                std::printf("%lu\n", static_cast<unsigned long>(size));
                std::fwrite(bytes.data(), 1, bytes.size(), stdout);
                sent = true;
                break;
            }
            if (!sent)
                std::printf("-1\n");
        }
        else if (line[0] == 'F')
        {
            std::set<std::string> seen;
            for (HANDLE archive : archives)
            {
                SFILE_FIND_DATA found;
                HANDLE find = SFileFindFirstFile(archive, arg.c_str(), &found, nullptr);
                if (!find)
                    continue;
                do
                    if (seen.insert(Lower(found.cFileName)).second)
                        std::printf("%s\n", found.cFileName);
                while (SFileFindNextFile(find, &found));
                SFileFindClose(find);
            }
            std::printf("\n");
        }
        std::fflush(stdout);
    }
    for (HANDLE archive : archives)
        SFileCloseArchive(archive);
    return 0;
}
