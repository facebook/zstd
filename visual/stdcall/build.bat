@echo off
setlocal
:: call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\vcvars32.bat"
:: call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64\vcvars64.bat"
set lib_name=zstdlib-x86
set cl_exe=cl.exe /DZSTD_DLL_STDCALL=1
set cl_flags=/MT /LD /O2 /Ox
set link_flags=/link /base:0x3f400000 /version:1.0 /incremental:no /opt:ref /merge:.rdata=.text /ignore:4078,4010
set src_dir=..\..\lib
set bin_dir=Release

if [%VisualStudioVersion%]==[14.0] (
    set link_flags=%link_flags% /subsystem:windows,5.1
)

if [%Platform%]==[X64] (
    set lib_name=zstdlib-x64
    set cl_exe=%cl_exe% /DZSTD_DLL_EXPORT=1
)
if exist %lib_name%.def set link_flags=%link_flags% /def:%lib_name%.def

pushd %~dp0

:parse_args
if [%1]==[debug] (
    set cl_flags=/MDd /LD /Zi
    set link_flags=%link_flags% /debug
    set bin_dir=Debug
    shift /1
    goto :parse_args
)

%cl_exe% %cl_flags% /Tp %src_dir%\zstd_compress.c /Tp %src_dir%\zstd_decompress.c /Tp %src_dir%\fse.c /Tp %src_dir%\huff0.c /Tp %src_dir%\zdict.c /Tp %src_dir%\divsufsort.c /Fe%lib_name%.dll %link_flags%
if errorlevel 1 goto :eof

echo Copying %lib_name% to %bin_dir%...
md %bin_dir% 2> nul
copy %lib_name%.dll %bin_dir% > nul
copy %lib_name%.pdb %bin_dir% > nul

:cleanup
del /q *.exp *.lib *.obj *.dll *.pdb *.ilk ~$* 2> nul

:eof
popd
