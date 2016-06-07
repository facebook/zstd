@echo off

rem
rem Parameters
rem   %1: vs_solution_version: VS solution version: VS2012, VS2013, VS2015
rem   %2: vs_configuration:    VS configuration:    Debug, Release
rem   %3: vs_target:           target:              Build or Rebuild
rem   %4: vs_platform:         platform:            x64 or Win32
rem   %5: vs_toolset:          toolset:             v100, v110, v120, v140
rem

set vs_solution_version=%1
set vs_configuration=%2
set vs_target=%3
set vs_platform=%4
set vs_toolset=%5

set msbuild="%windir%\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe"
IF %vs_solution_version% == VS2013 SET msbuild="C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe"
IF %vs_solution_version% == VS2015 SET msbuild="C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe"
rem TODO: Visual Studio "15" (vNext) will use MSBuild 15.0 ?

set project="%~p0\..\projects\VS2010\zstd.sln"

set output=%~p0%bin
set output="%output%/%vs_solution_version%/%vs_platform%/"

set msbuildparams=/verbosity:minimal /p:Platform="%vs_platform%" /p:Configuration="%vs_configuration%" /p:PlatformToolset="%vs_toolset%" /t:Clean,Build /nologo /p:OutDir=%output%

echo ### Building %vs_solution_version% project for %vs_configuration% %vs_platform% (%vs_toolset%)...
echo ### Build Params: %msbuildparams%

%msbuild% %project% %msbuildparams%
IF ERRORLEVEL 1 EXIT /B 1
echo # Success
echo # OutDir: %output%
echo #

