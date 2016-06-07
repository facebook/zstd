@echo off

rem build 32-bit
call "%~p0%build.generic.cmd" VS2015 Release "Clean,Build" Win32 v140

rem build 64-bit
call "%~p0%build.generic.cmd" VS2015 Release "Clean,Build" x64 v140