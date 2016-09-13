REM http://stackoverflow.com/questions/708238/how-do-i-add-an-icon-to-a-mingw-gcc-compiled-executable
REM copy "c:\Program Files (x86)\Windows Kits\8.1\Include\um\verrsrc.h" .
windres -I ..\..\..\lib -O coff -i zstd.rc -o zstd.res
