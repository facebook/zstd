@echo off
setlocal

rem Detect build type based on available files
set BUILD_TYPE=make
if exist "build\cmake\build\lib\Release\zstd_static.lib" set BUILD_TYPE=cmake

echo Detected build type: %BUILD_TYPE%

rem Create required directories
mkdir bin\dll bin\static bin\example bin\include

rem Copy common files using a subroutine. Exits immediately on failure.
call :copyFile "tests\fullbench.c" "bin\example\"
call :copyFile "programs\datagen.c" "bin\example\"
call :copyFile "programs\datagen.h" "bin\example\"
call :copyFile "programs\util.h" "bin\example\"
call :copyFile "programs\platform.h" "bin\example\"
call :copyFile "lib\common\mem.h" "bin\example\"
call :copyFile "lib\common\zstd_internal.h" "bin\example\"
call :copyFile "lib\common\error_private.h" "bin\example\"
call :copyFile "lib\common\xxhash.h" "bin\example\"
call :copyFile "lib\dll\example\Makefile" "bin\example\"
call :copyFile "lib\dll\example\fullbench-dll.*" "bin\example\"
call :copyFile "lib\zstd.h" "bin\include\"
call :copyFile "lib\zstd_errors.h" "bin\include\"
call :copyFile "lib\zdict.h" "bin\include\"

rem Copy build-specific files
if "%BUILD_TYPE%"=="cmake" (
    echo Copying CMake build artifacts...
    call :copyFile "build\cmake\build\lib\Release\zstd_static.lib" "bin\static\libzstd_static.lib"
    call :copyFile "build\cmake\build\lib\Release\zstd.dll" "bin\dll\libzstd.dll"
    call :copyFile "build\cmake\build\lib\Release\zstd.lib" "bin\dll\zstd.lib"
    call :copyFile "build\cmake\build\programs\Release\zstd.exe" "bin\zstd.exe"
    call :copyFile "lib\dll\example\README.md" "bin\README.md"
) else (
    echo Copying Make build artifacts...
    call :copyFile "lib\libzstd.a" "bin\static\libzstd_static.lib"
    call :copyFile "lib\dll\libzstd.*" "bin\dll\"
    call :copyFile "programs\zstd.exe" "bin\zstd.exe"
    call :copyFile "lib\dll\example\README.md" "bin\"
)

echo Build package created successfully for %BUILD_TYPE% build!
endlocal
exit /b 0

:copyFile
copy "%~1" "%~2"
if errorlevel 1 (
    echo Failed to copy "%~1"
    exit 1
)
exit /b