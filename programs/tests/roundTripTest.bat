@echo off
if [%3]==[] (SET C=%2 && SET P=) ELSE (SET C=%3 && SET P=%2) 
rm -f tmp1 tmp2
echo roundTripTest: datagen %1 %P% ^| %ZSTD% -v%C% ^| %ZSTD% -d
datagen %1 %P% | md5sum > tmp1
datagen %1 %P% | %ZSTD% -vq%C% | %ZSTD% -d  | md5sum > tmp2
fc tmp1 tmp2
EXIT /B %ERRORLEVEL%
