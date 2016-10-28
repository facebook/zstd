@echo off
REM http://stackoverflow.com/questions/708238/how-do-i-add-an-icon-to-a-mingw-gcc-compiled-executable

where /q windres.exe
IF ERRORLEVEL 1 (
    ECHO The windres.exe is missing. Ensure it is installed and placed in your PATH.
    EXIT /B
) ELSE (
    windres.exe -I ..\..\..\..\lib -O coff -I . -i ..\zstd.rc -o zstd.res
)
