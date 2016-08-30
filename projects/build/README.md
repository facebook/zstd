Command line scripts for Visual Studio compilation without IDE
==============================================================

Here are a few command lines for reference :

### Build with Visual Studio 2013 for msvcr120.dll

Running the following command will build both the `Release Win32` and `Release x64` versions:
```batch
build\build.VS2013.cmd
```
The result of each build will be in the corresponding `build\bin\Release\{ARCH}\` folder.

If you want to only need one architecture:
- Win32: `build\build.generic.cmd VS2013 Win32 Release v120`
- x64: `build\build.generic.cmd VS2013 x64 Release v120`

If you want a Debug build:
- Win32: `build\build.generic.cmd VS2013 Win32 Debug v120`
- x64: `build\build.generic.cmd VS2013 x64 Debug v120`

### Build with Visual Studio 2015 for msvcr140.dll

Running the following command will build both the `Release Win32` and `Release x64` versions:
```batch
build\build.VS2015.cmd
```
The result of each build will be in the corresponding `build\bin\Release\{ARCH}\` folder.

If you want to only need one architecture:
- Win32: `build\build.generic.cmd VS2015 Win32 Release v140`
- x64: `build\build.generic.cmd VS2015 x64 Release v140`

If you want a Debug build:
- Win32: `build\build.generic.cmd VS2015 Win32 Debug v140`
- x64: `build\build.generic.cmd VS2015 x64 Debug v140`

### Build with Visual Studio 2015 for msvcr120.dll

You need to invoke `build\build.generic.cmd` with the proper arguments:

**For Win32**
```batch
build\build.generic.cmd VS2015 Win32 Release v120
```
The result of the build will be in the `build\bin\Release\Win32\` folder.

**For x64**
```batch
build\build.generic.cmd VS2015 x64 Release v120
```
The result of the build will be in the `build\bin\Release\x64\` folder.

If you want Debug builds, replace `Release` with `Debug`.
