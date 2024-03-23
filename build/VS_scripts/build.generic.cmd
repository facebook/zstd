@echo off

IF "%1%" == "" GOTO display_help

SETLOCAL ENABLEDELAYEDEXPANSION

SET msbuild_version=%1

SET msbuild_platform=%2
IF "%msbuild_platform%" == "" SET msbuild_platform=x64

SET msbuild_configuration=%3
IF "%msbuild_configuration%" == "" SET msbuild_configuration=Release

SET msbuild_toolset=%4

GOTO build

:display_help

echo Syntax: build.generic.cmd msbuild_version msbuild_platform msbuild_configuration msbuild_toolset
echo   msbuild_version:          VS installed version (VS2012, VS2013, VS2015, VS2017, VS2019, ...)
echo   msbuild_platform:         Platform (x64 or Win32)
echo   msbuild_configuration:    VS configuration (Release or Debug)
echo   msbuild_toolset:          Platform Toolset (v100, v110, v120, v140, v141, v142, ...)

EXIT /B 1

:build

SET msbuild="%windir%\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe"
IF %msbuild_version% == VS2013 SET msbuild="%programfiles(x86)%\MSBuild\12.0\Bin\MSBuild.exe"
IF %msbuild_version% == VS2015 SET msbuild="%programfiles(x86)%\MSBuild\14.0\Bin\MSBuild.exe"
IF %msbuild_version% == VS2017 SET vswhere_version=[15,16)
IF %msbuild_version% == VS2017Community SET vswhere_version=[15,16) & SET vswhere_products=Community
IF %msbuild_version% == VS2017Enterprise SET vswhere_version=[15,16) & SET vswhere_products=Enterprise
IF %msbuild_version% == VS2017Professional SET vswhere_version=[15,16) & SET vswhere_products=Professional
IF %msbuild_version% == VS2019 SET vswhere_version=[16,17)
IF %msbuild_version% == VS2022 SET vswhere_version=[17,18)
REM Add the next Visual Studio version here. 

IF NOT DEFINED vswhere_version GOTO skip_vswhere
IF NOT DEFINED vswhere_products SET vswhere_products=*
SET vswhere="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
FOR /F "USEBACKQ TOKENS=*" %%F IN (`%vswhere% -utf8 -property installationPath -products !vswhere_products! -version !vswhere_version!`) DO (
	SET msbuild="%%F\MSBuild\Current\Bin\MSBuild.exe"
)
:skip_vswhere

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
