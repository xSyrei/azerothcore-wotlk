# EmberforgeExtract

Copies the files Emberforge needs out of a Mists of Pandaria (or Cataclysm) client's archives, so nobody opens
them by hand. It is `EmberforgeExtract.cpp` built for Windows against
[StormLib](https://github.com/ladislav-zezula/StormLib) (MIT licensed, `StormLib-LICENSE.txt`), which reads the
archives and applies the client's update archives on top of them, as the game does.

## Running it

In PowerShell, from a folder with room to spare (it writes `EmberforgeExtract\` there):

```powershell
& "$HOME\azerothcore-wotlk\modules\mod-titan\tools\extract\EmberforgeExtract.exe" "C:\Firestorm\Mists of Pandaria\Data"
```

With no list it takes the built-in patterns, in this order: the Dungeon Journal's tables (`DBFilesClient\Journal*`,
DungeonEncounter, Map, MapDifficulty, LfgDungeons, AreaTable) and art (`Interface\EncounterJournal\*`), the dungeon
finder's art (`Interface\LFGFrame\*`), the character pane's (`Interface\PaperDollInfoFrame\*`), the rest of the
client's data tables (`DBFilesClient\*`) and the loading screens (`Interface\Glues\LoadingScreens\*`). A second
argument names a text file of patterns instead, one a line (`*` and `?` wildcards, `#` comments; `-` keeps the
built-in ones).

Besides the `EmberforgeExtract\` folder, it packs the same files into zips small enough to upload,
`EmberforgeExtract-part1.zip`, `-part2.zip`..., filled in the patterns' order, so part 1 holds what is needed first.
Parts are at most 24 MB; a fourth argument sets another size in MB (`- enUS 50`). `manifest.txt` lists each file, the
part holding it and the archive it came from.

Only English (`enUS`) text is read: a client with several languages installed (`deDE`, `frFR`...) would
otherwise mix them. A third argument picks another language, e.g. `- enGB`.

## Building it

On Linux with MinGW (`g++-mingw-w64-x86-64-posix`): build StormLib as a static library with a Windows toolchain
file, then

```sh
x86_64-w64-mingw32-g++-posix -std=c++17 -O2 -DSTORMLIB_NO_AUTO_LINK -I<StormLib>/src EmberforgeExtract.cpp \
    <build>/libStormLib.a -o EmberforgeExtract.exe -static -lwininet
```

The committed `EmberforgeExtract.exe` was built so from StormLib at 116135c.
