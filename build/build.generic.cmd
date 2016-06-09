@echo off

IF "%1%" == "" GOTO display_help

SET vs_version=%1

SET vs_platform=%2
IF "%vs_platform%" == "" SET vs_platform=x64

SET vs_configuration=%3
IF "%vs_configuration%" == "" SET vs_configuration=Release

SET vs_toolset=%4

GOTO build

:display_help

echo Syntax: build.generic.cmd vs_version vs_platform vs_configuration vs_toolset
echo   vs_version:          VS installed version (VS2012, VS2013, VS2015, ...)
echo   vs_platform:         Platform (x64 or Win32)
echo   vs_configuration:    VS configuration (Release or Debug)
echo   vs_toolset:          Platform Toolset (v100, v110, v120, v140)

EXIT /B 1

:build

SET msbuild="%windir%\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe"
IF %vs_version% == VS2013 SET msbuild="C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe"
IF %vs_version% == VS2015 SET msbuild="C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe"
rem TODO: Visual Studio "15" (vNext) will use MSBuild 15.0 ?

SET project="%~p0\..\projects\VS2010\zstd.sln"

SET msbuildparams=/verbosity:minimal /nologo /t:Clean,Build /p:Platform=%vs_platform% /p:Configuration=%vs_configuration%
IF NOT "%vs_toolset%" == "" SET msbuildparams=%msbuildparams% /p:PlatformToolset=%vs_toolset%

SET output=%~p0%bin
SET output="%output%/%vs_configuration%/%vs_platform%/"
SET msbuildparams=%msbuildparams% /p:OutDir=%output%

echo ### Building %vs_version% project for %vs_configuration% %vs_platform% (%vs_toolset%)...
echo ### Build Params: %msbuildparams%

%msbuild% %project% %msbuildparams%
IF ERRORLEVEL 1 EXIT /B 1
echo # Success
echo # OutDir: %output%
echo #

