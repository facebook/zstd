Projects for various integrated development environments (IDE)
==============================================================

#### Included projects

The following projects are included with the zstd distribution:
- `cmake` - CMake project contributed by Artyom Dymchenko
- `VS2005` - Visual Studio 2005 project
- `VS2008` - Visual Studio 2008 project
- `VS2010` - Visual Studio 2010 project (which also works well with Visual Studio 2012, 2013, 2015)
- `VS_scripts` - command line scripts prepared for Visual Studio compilation without IDE


#### How to compile zstd with Visual Studio

1. Install Visual Studio e.g. VS 2015 Community Edition (it's free).
2. Download the latest version of zstd from https://github.com/facebook/zstd/releases
3. Decompress ZIP archive.
4. Go to decompressed directory then to `projects` then `VS2010` and open `zstd.sln`
5. Visual Studio will ask about converting VS2010 project to VS2015 and you should agree.
6. Change `Debug` to `Release` and if you have 64-bit Windows change also `Win32` to `x64`.
7. Press F7 on keyboard or select `BUILD` from the menu bar and choose `Build Solution`.
8. If compilation will be fine a compiled executable will be in `projects\VS2010\bin\x64\Release\zstd.exe`
