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

The archives are read in the client's own order, the newest copy of a file winning: the language folder's patches
(`patch-enUS-3.MPQ` above `patch-enUS-2.MPQ` above `patch-enUS.MPQ`), then the main folder's (`patch-3.MPQ`... above
`patch.MPQ`, letters above numbers), then the rest (`lichking` above `expansion` above `common`); Mists of Pandaria's
update archives are applied as patches.

`patterns/` holds the lists for the 3.3.5a client's files the Dungeon Journal's head shots need
(`tools/journal/portraits.py`), in rounds (`portraits-1.txt`, `-2`, `-3`), each made from what the one before
brought.

Only English (`enUS`) text is read: a client with several languages installed (`deDE`, `frFR`...) would
otherwise mix them. A third argument picks another language, e.g. `- enGB`.

## Building it

On Linux with MinGW (`g++-mingw-w64-x86-64-posix`): build StormLib as a static library with a Windows toolchain
file, then

```sh
x86_64-w64-mingw32-g++-posix -std=c++17 -O2 -DSTORMLIB_NO_AUTO_LINK -I<StormLib>/src -I<StormLib>/src/zlib EmberforgeExtract.cpp \
    <build>/libStormLib.a -o EmberforgeExtract.exe -static -lwininet
```

The committed `EmberforgeExtract.exe` was built so from StormLib at 116135c.
