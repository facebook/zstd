@echo off

rem build 32-bit
call "%~p0%build.generic.cmd" VS2012 Release Rebuild Win32 v110
rem build 64-bit
call "%~p0%build.generic.cmd" VS2012 Release Rebuild x64 v110