@echo off

rem build 32-bit
call "%~p0%build.generic.cmd" preview Win32 Release v143

rem build 64-bit
call "%~p0%build.generic.cmd" preview x64 Release v143