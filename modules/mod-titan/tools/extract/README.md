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

With no list it takes the built-in patterns: the Dungeon Journal's art (`Interface\EncounterJournal\*`), the
character pane's (`Interface\PaperDollInfoFrame\*`), the dungeon finder's (`Interface\LFGFrame\*`), the loading
screens (`Interface\Glues\LoadingScreens\*`) and the client's data tables (`DBFilesClient\*`). A second argument
names a text file of patterns instead, one a line (`*` and `?` wildcards, `#` comments). `manifest.txt` lists
what was written and from which archive.

## Building it

On Linux with MinGW (`g++-mingw-w64-x86-64-posix`): build StormLib as a static library with a Windows toolchain
file, then

```sh
x86_64-w64-mingw32-g++-posix -std=c++17 -O2 -DSTORMLIB_NO_AUTO_LINK -I<StormLib>/src EmberforgeExtract.cpp \
    <build>/libStormLib.a -o EmberforgeExtract.exe -static -lwininet
```

The committed `EmberforgeExtract.exe` was built so from StormLib at 116135c.
