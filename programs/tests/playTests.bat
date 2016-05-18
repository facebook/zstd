@echo off
if [%ZSTD%]==[] echo ZSTD variable must be defined! && exit /b 1

echo. && echo **** simple tests ****
datagen > tmp
%ZSTD% -f tmp                             && REM trivial compression case, creates tmp.zst
%ZSTD% -df tmp.zst                        && REM trivial decompression case (overwrites tmp)
echo test : too large compression level (must fail)
%ZSTD% -99 tmp && (echo too large compression level undetected && exit /b 1)
echo test : compress to stdout
%ZSTD% tmp -c > tmpCompressed                 
%ZSTD% tmp --stdout > tmpCompressed       && REM long command format
echo test : null-length file roundtrip
echo. 2>tmpEmpty | cat tmpEmpty | %ZSTD% - --stdout | %ZSTD% -d --stdout || (echo wrong null-length file roundtrip && exit /b 1)
echo test : decompress file with wrong suffix (must fail)
%ZSTD% -d tmpCompressed && (echo wrong suffix error not detected! && exit /b 1)
%ZSTD% -d tmpCompressed -c > tmpResult    && REM decompression using stdout   
%ZSTD% --decompress tmpCompressed -c > tmpResult
%ZSTD% --decompress tmpCompressed --stdout > tmpResult
REM %ZSTD% -d    < tmp.zst > NUL              && REM combine decompression, stdin & stdout
REM %ZSTD% -d  - < tmp.zst > NUL
%ZSTD% -dc   < tmp.zst > NUL
%ZSTD% -dc - < tmp.zst > NUL
%ZSTD% -q tmp && (echo overwrite check failed! && exit /b 1)
%ZSTD% -q -f tmp
%ZSTD% -q --force tmp
%ZSTD% -df tmp && (echo should have refused : wrong extension && exit /b 1)
cp tmp tmp2.zst
%ZSTD% -df tmp2.zst && (echo should have failed : wrong format && exit /b 1)
rm tmp2.zst

echo. && echo **** frame concatenation ****

echo hello > hello.tmp
echo world! > world.tmp
cat hello.tmp world.tmp > helloworld.tmp
%ZSTD% -c hello.tmp > hello.zstd
%ZSTD% -c world.tmp > world.zstd
cat hello.zstd world.zstd > helloworld.zstd
%ZSTD% -dc helloworld.zstd > result.tmp
cat result.tmp
fc /b helloworld.tmp result.tmp
rm *.tmp *.zstd

echo frame concatenation test completed


REM echo. && echo **** flush write error test ****

REM echo echo foo ^| %ZSTD% ^> v:\full
REM echo foo | %ZSTD% > v:\full && (echo write error not detected! && exit /b 1)
REM echo "echo foo | %ZSTD% | %ZSTD% -d > /dev/full"
REM echo foo | %ZSTD% | %ZSTD% -d > /dev/full && (echo write error not detected! && exit /b 1)


echo. && echo **** dictionary tests ****

datagen > tmpDict
datagen -g1M | md5sum > tmp1
datagen -g1M | %ZSTD% -D tmpDict | %ZSTD% -D tmpDict -dvq | md5sum > tmp2
fc tmp1 tmp2
%ZSTD% --train *.c *.h -o tmpDict
%ZSTD% xxhash.c -D tmpDict -of tmp
%ZSTD% -d tmp -D tmpDict -of result
fc xxhash.c result


echo. && echo **** multiple files tests ****

datagen -s1        > tmp1 2> NUL
datagen -s2 -g100K > tmp2 2> NUL
datagen -s3 -g1M   > tmp3 2> NUL
%ZSTD% -f tmp*
echo compress tmp* :
ls -ls tmp*
rm tmp1 tmp2 tmp3
echo decompress tmp* :
%ZSTD% -df *.zst
ls -ls tmp*

echo compress tmp* into stdout ^> tmpall :
%ZSTD% -c tmp1 tmp2 tmp3 > tmpall
ls -ls tmp*
echo decompress tmpall* into stdout ^> tmpdec :
cp tmpall tmpall2
%ZSTD% -dc tmpall* > tmpdec
ls -ls tmp*
echo compress multiple files including a missing one (notHere) :
%ZSTD% -f tmp1 notHere tmp2 && (echo missing file not detected! && exit /b 1)


echo. && echo **** integrity tests ****
echo test one file (tmp1.zst)
%ZSTD% -t tmp1.zst
%ZSTD% --test tmp1.zst
echo test multiple files (*.zst)
%ZSTD% -t *.zst
echo test good and bad files (*)
%ZSTD% -t * && (echo bad files not detected! && exit /b 1)


echo. && echo **** zstd round-trip tests ****

CALL roundTripTest.bat
CALL roundTripTest.bat -g15K       && REM TableID==3
CALL roundTripTest.bat -g127K      && REM TableID==2
CALL roundTripTest.bat -g255K      && REM TableID==1
CALL roundTripTest.bat -g513K      && REM TableID==0
CALL roundTripTest.bat -g512K 6    && REM greedy, hash chain
CALL roundTripTest.bat -g512K 16   && REM btlazy2 
CALL roundTripTest.bat -g512K 19   && REM btopt

rm tmp*
echo Param = %1
if NOT "%1"=="--test-large-data" (
    echo skipping large data tests
    exit /b 0
)

CALL roundTripTest.bat -g270000000 1
CALL roundTripTest.bat -g270000000 2
CALL roundTripTest.bat -g270000000 3

CALL roundTripTest.bat -g140000000 -P60 4
CALL roundTripTest.bat -g140000000 -P60 5
CALL roundTripTest.bat -g140000000 -P60 6

CALL roundTripTest.bat -g70000000 -P70 7
CALL roundTripTest.bat -g70000000 -P70 8
CALL roundTripTest.bat -g70000000 -P70 9

CALL roundTripTest.bat -g35000000 -P75 10
CALL roundTripTest.bat -g35000000 -P75 11
CALL roundTripTest.bat -g35000000 -P75 12

CALL roundTripTest.bat -g18000000 -P80 13
CALL roundTripTest.bat -g18000000 -P80 14
CALL roundTripTest.bat -g18000000 -P80 15
CALL roundTripTest.bat -g18000000 -P80 16
CALL roundTripTest.bat -g18000000 -P80 17

CALL roundTripTest.bat -g50000000 -P94 18
CALL roundTripTest.bat -g50000000 -P94 19

CALL roundTripTest.bat -g99000000 -P99 20
CALL roundTripTest.bat -g6000000000 -P99 1

rm tmp*
exit /b 0
