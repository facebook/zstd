@echo off

IF "%1%" == "" GOTO display_help

SETLOCAL

SET msbuild_version=%1

SET msbuild_platform=%2
IF "%msbuild_platform%" == "" SET msbuild_platform=x64

SET msbuild_configuration=%3
IF "%msbuild_configuration%" == "" SET msbuild_configuration=Release

SET msbuild_toolset=%4

GOTO build

:display_help

echo Syntax: build.generic.cmd msbuild_version msbuild_platform msbuild_configuration msbuild_toolset
echo   msbuild_version:          VS installed version (VS2012, VS2013, VS2015, ...)
echo   msbuild_platform:         Platform (x64 or Win32)
echo   msbuild_configuration:    VS configuration (Release or Debug)
echo   msbuild_toolset:          Platform Toolset (v100, v110, v120, v140)

EXIT /B 1

:build

SET msbuild="%windir%\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe"
IF %msbuild_version% == VS2013 SET msbuild="%programfiles(x86)%\MSBuild\12.0\Bin\MSBuild.exe"
IF %msbuild_version% == VS2015 SET msbuild="%programfiles(x86)%\MSBuild\14.0\Bin\MSBuild.exe"
rem TODO: Visual Studio "15" (vNext) will use MSBuild 15.0 ?

SET project="%~p0\..\VS2010\zstd.sln"

SET msbuild_params=/verbosity:minimal /nologo /t:Clean,Build /p:Platform=%msbuild_platform% /p:Configuration=%msbuild_configuration%
IF NOT "%msbuild_toolset%" == "" SET msbuild_params=%msbuild_params% /p:PlatformToolset=%msbuild_toolset%

SET output=%~p0%bin
SET output="%output%/%msbuild_configuration%/%msbuild_platform%/"
SET msbuild_params=%msbuild_params% /p:OutDir=%output%

echo ### Building %msbuild_version% project for %msbuild_configuration% %msbuild_platform% (%msbuild_toolset%)...
echo ### Build Params: %msbuild_params%

%msbuild% %project% %msbuild_params%
IF ERRORLEVEL 1 EXIT /B 1
echo # Success
echo # OutDir: %output%
echo #
