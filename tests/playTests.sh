#!/bin/sh -e

die() {
    $ECHO "$@" 1>&2
    exit 1
}

roundTripTest() {
    if [ -n "$3" ]; then
        local_c="$3"
        local_p="$2"
    else
        local_c="$2"
    fi

    rm -f tmp1 tmp2
    $ECHO "roundTripTest: ./datagen $1 $local_p | $ZSTD -v$local_c | $ZSTD -d"
    ./datagen $1 $local_p | $MD5SUM > tmp1
    ./datagen $1 $local_p | $ZSTD --ultra -v$local_c | $ZSTD -d  | $MD5SUM > tmp2
    $DIFF -q tmp1 tmp2
}

isWindows=false
ECHO="echo"
INTOVOID="/dev/null"
case "$OS" in
  Windows*)
    isWindows=true
    ECHO="echo -e"
    ;;
esac

UNAME=$(uname)
case "$UNAME" in
  Darwin) MD5SUM="md5 -r" ;;
  FreeBSD) MD5SUM="gmd5sum" ;;
  *) MD5SUM="md5sum" ;;
esac

DIFF="diff"
case "$UNAME" in
  SunOS) DIFF="gdiff" ;;
esac


$ECHO "\nStarting playTests.sh isWindows=$isWindows ZSTD='$ZSTD'"

[ -n "$ZSTD" ] || die "ZSTD variable must be defined!"

$ECHO "\n**** simple tests **** "

./datagen > tmp
$ECHO "test : basic compression "
$ZSTD -f tmp                      # trivial compression case, creates tmp.zst
$ECHO "test : basic decompression"
$ZSTD -df tmp.zst                 # trivial decompression case (overwrites tmp)
$ECHO "test : too large compression level (must fail)"
$ZSTD -99 -f tmp  # too large compression level, automatic sized down
$ECHO "test : compress to stdout"
$ZSTD tmp -c > tmpCompressed
$ZSTD tmp --stdout > tmpCompressed       # long command format
$ECHO "test : compress to named file"
rm tmpCompressed
$ZSTD tmp -o tmpCompressed
ls tmpCompressed   # must work
$ECHO "test : -o must be followed by filename (must fail)"
$ZSTD tmp -of tmpCompressed && die "-o must be followed by filename "
$ECHO "test : force write, correct order"
$ZSTD tmp -fo tmpCompressed
$ECHO "test : forgotten argument"
cp tmp tmp2
$ZSTD tmp2 -fo && die "-o must be followed by filename "
$ECHO "test : implied stdout when input is stdin"
$ECHO bob | $ZSTD | $ZSTD -d
$ECHO "test : null-length file roundtrip"
$ECHO -n '' | $ZSTD - --stdout | $ZSTD -d --stdout
$ECHO "test : decompress file with wrong suffix (must fail)"
$ZSTD -d tmpCompressed && die "wrong suffix error not detected!"
$ZSTD -df tmp && die "should have refused : wrong extension"
$ECHO "test : decompress into stdout"
$ZSTD -d tmpCompressed -c > tmpResult    # decompression using stdout
$ZSTD --decompress tmpCompressed -c > tmpResult
$ZSTD --decompress tmpCompressed --stdout > tmpResult
$ECHO "test : decompress from stdin into stdout"
$ZSTD -dc   < tmp.zst > $INTOVOID   # combine decompression, stdin & stdout
$ZSTD -dc - < tmp.zst > $INTOVOID
$ZSTD -d    < tmp.zst > $INTOVOID   # implicit stdout when stdin is used
$ZSTD -d  - < tmp.zst > $INTOVOID
$ECHO "test : impose memory limitation (must fail)"
$ZSTD -d -f tmp.zst -M2K -c > $INTOVOID && die "decompression needs more memory than allowed"
$ZSTD -d -f tmp.zst --memlimit=2K -c > $INTOVOID && die "decompression needs more memory than allowed"  # long command
$ZSTD -d -f tmp.zst --memory=2K -c > $INTOVOID && die "decompression needs more memory than allowed"  # long command
$ZSTD -d -f tmp.zst --memlimit-decompress=2K -c > $INTOVOID && die "decompression needs more memory than allowed"  # long command
$ECHO "test : overwrite protection"
$ZSTD -q tmp && die "overwrite check failed!"
$ECHO "test : force overwrite"
$ZSTD -q -f tmp
$ZSTD -q --force tmp
$ECHO "test : file removal"
$ZSTD -f --rm tmp
ls tmp && die "tmp should no longer be present"
$ZSTD -f -d --rm tmp.zst
ls tmp.zst && die "tmp.zst should no longer be present"
rm tmp
$ZSTD -f tmp && die "tmp not present : should have failed"
ls tmp.zst && die "tmp.zst should not be created"


$ECHO "\n**** Advanced compression parameters **** "
$ECHO "Hello world!" | $ZSTD --zstd=windowLog=21,      - -o tmp.zst && die "wrong parameters not detected!"
$ECHO "Hello world!" | $ZSTD --zstd=windowLo=21        - -o tmp.zst && die "wrong parameters not detected!"
$ECHO "Hello world!" | $ZSTD --zstd=windowLog=21,slog  - -o tmp.zst && die "wrong parameters not detected!"
ls tmp.zst && die "tmp.zst should not be created"
roundTripTest -g512K
roundTripTest -g512K " --zstd=slen=3,tlen=48,strat=6"
roundTripTest -g512K " --zstd=strat=6,wlog=23,clog=23,hlog=22,slog=6"
roundTripTest -g512K " --zstd=windowLog=23,chainLog=23,hashLog=22,searchLog=6,searchLength=3,targetLength=48,strategy=6"
roundTripTest -g512K 19


$ECHO "\n**** Pass-Through mode **** "
$ECHO "Hello world 1!" | $ZSTD -df
$ECHO "Hello world 2!" | $ZSTD -dcf
$ECHO "Hello world 3!" > tmp1
$ZSTD -dcf tmp1


$ECHO "\n**** frame concatenation **** "

$ECHO "hello " > hello.tmp
$ECHO "world!" > world.tmp
cat hello.tmp world.tmp > helloworld.tmp
$ZSTD -c hello.tmp > hello.zstd
$ZSTD -c world.tmp > world.zstd
cat hello.zstd world.zstd > helloworld.zstd
$ZSTD -dc helloworld.zstd > result.tmp
cat result.tmp
$DIFF helloworld.tmp result.tmp
$ECHO "frame concatenation without checksum"
$ZSTD -c hello.tmp > hello.zstd --no-check
$ZSTD -c world.tmp > world.zstd --no-check
cat hello.zstd world.zstd > helloworld.zstd
$ZSTD -dc helloworld.zstd > result.tmp
cat result.tmp
$DIFF helloworld.tmp result.tmp
rm ./*.tmp ./*.zstd
$ECHO "frame concatenation tests completed"


if [ "$isWindows" = false ] && [ "$UNAME" != 'SunOS' ] ; then
$ECHO "\n**** flush write error test **** "

$ECHO "$ECHO foo | $ZSTD > /dev/full"
$ECHO foo | $ZSTD > /dev/full && die "write error not detected!"
$ECHO "$ECHO foo | $ZSTD | $ZSTD -d > /dev/full"
$ECHO foo | $ZSTD | $ZSTD -d > /dev/full && die "write error not detected!"
fi


$ECHO "\n**** test sparse file support **** "

./datagen -g5M  -P100 > tmpSparse
$ZSTD tmpSparse -c | $ZSTD -dv -o tmpSparseRegen
$DIFF -s tmpSparse tmpSparseRegen
$ZSTD tmpSparse -c | $ZSTD -dv --sparse -c > tmpOutSparse
$DIFF -s tmpSparse tmpOutSparse
$ZSTD tmpSparse -c | $ZSTD -dv --no-sparse -c > tmpOutNoSparse
$DIFF -s tmpSparse tmpOutNoSparse
ls -ls tmpSparse*
./datagen -s1 -g1200007 -P100 | $ZSTD | $ZSTD -dv --sparse -c > tmpSparseOdd   # Odd size file (to not finish on an exact nb of blocks)
./datagen -s1 -g1200007 -P100 | $DIFF -s - tmpSparseOdd
ls -ls tmpSparseOdd
$ECHO "\n Sparse Compatibility with Console :"
$ECHO "Hello World 1 !" | $ZSTD | $ZSTD -d -c
$ECHO "Hello World 2 !" | $ZSTD | $ZSTD -d | cat
$ECHO "\n Sparse Compatibility with Append :"
./datagen -P100 -g1M > tmpSparse1M
cat tmpSparse1M tmpSparse1M > tmpSparse2M
$ZSTD -v -f tmpSparse1M -o tmpSparseCompressed
$ZSTD -d -v -f tmpSparseCompressed -o tmpSparseRegenerated
$ZSTD -d -v -f tmpSparseCompressed -c >> tmpSparseRegenerated
ls -ls tmpSparse*
$DIFF tmpSparse2M tmpSparseRegenerated
rm tmpSparse*


$ECHO "\n**** multiple files tests **** "

./datagen -s1        > tmp1 2> $INTOVOID
./datagen -s2 -g100K > tmp2 2> $INTOVOID
./datagen -s3 -g1M   > tmp3 2> $INTOVOID
$ECHO "compress tmp* : "
$ZSTD -f tmp*
ls -ls tmp*
rm tmp1 tmp2 tmp3
$ECHO "decompress tmp* : "
$ZSTD -df *.zst
ls -ls tmp*
$ECHO "compress tmp* into stdout > tmpall : "
$ZSTD -c tmp1 tmp2 tmp3 > tmpall
ls -ls tmp*
$ECHO "decompress tmpall* into stdout > tmpdec : "
cp tmpall tmpall2
$ZSTD -dc tmpall* > tmpdec
ls -ls tmp*
$ECHO "compress multiple files including a missing one (notHere) : "
$ZSTD -f tmp1 notHere tmp2 && die "missing file not detected!"


$ECHO "\n**** dictionary tests **** "

TESTFILE=../programs/zstdcli.c
./datagen > tmpDict
./datagen -g1M | $MD5SUM > tmp1
./datagen -g1M | $ZSTD -D tmpDict | $ZSTD -D tmpDict -dvq | $MD5SUM > tmp2
$DIFF -q tmp1 tmp2
$ECHO "- Create first dictionary"
$ZSTD --train *.c ../programs/*.c -o tmpDict
cp $TESTFILE tmp
$ZSTD -f tmp -D tmpDict
$ZSTD -d tmp.zst -D tmpDict -fo result
$DIFF $TESTFILE result
$ECHO "- Create second (different) dictionary"
$ZSTD --train *.c ../programs/*.c ../programs/*.h -o tmpDictC
$ZSTD -d tmp.zst -D tmpDictC -fo result && die "wrong dictionary not detected!"
$ECHO "- Create dictionary with short dictID"
$ZSTD --train *.c ../programs/*.c --dictID 1 -o tmpDict1
cmp tmpDict tmpDict1 && die "dictionaries should have different ID !"
$ECHO "- Create dictionary with wrong dictID parameter order (must fail)"
$ZSTD --train *.c ../programs/*.c --dictID -o 1 tmpDict1 && die "wrong order : --dictID must be followed by argument "
$ECHO "- Create dictionary with size limit"
$ZSTD --train *.c ../programs/*.c -o tmpDict2 --maxdict 4K -v
$ECHO "- Create dictionary with wrong parameter order (must fail)"
$ZSTD --train *.c ../programs/*.c -o tmpDict2 --maxdict -v 4K && die "wrong order : --maxdict must be followed by argument "
$ECHO "- Compress without dictID"
$ZSTD -f tmp -D tmpDict1 --no-dictID
$ZSTD -d tmp.zst -D tmpDict -fo result
$DIFF $TESTFILE result
$ECHO "- Compress with wrong argument order (must fail)"
$ZSTD tmp -Df tmpDict1 -c > /dev/null && die "-D must be followed by dictionary name "
$ECHO "- Compress multiple files with dictionary"
rm -rf dirTestDict
mkdir dirTestDict
cp *.c dirTestDict
cp ../programs/*.c dirTestDict
cp ../programs/*.h dirTestDict
$MD5SUM dirTestDict/* > tmph1
$ZSTD -f --rm dirTestDict/* -D tmpDictC
$ZSTD -d --rm dirTestDict/*.zst -D tmpDictC  # note : use internal checksum by default
case "$UNAME" in
  Darwin) $ECHO "md5sum -c not supported on OS-X : test skipped" ;;  # not compatible with OS-X's md5
  *) $MD5SUM -c tmph1 ;;
esac
rm -rf dirTestDict
rm tmp*


$ECHO "\n**** cover dictionary tests **** "

TESTFILE=../programs/zstdcli.c
./datagen > tmpDict
$ECHO "- Create first dictionary"
$ZSTD --train --cover=k=46,d=8 *.c ../programs/*.c -o tmpDict
cp $TESTFILE tmp
$ZSTD -f tmp -D tmpDict
$ZSTD -d tmp.zst -D tmpDict -fo result
$DIFF $TESTFILE result
$ECHO "- Create second (different) dictionary"
$ZSTD --train --cover=k=56,d=8 *.c ../programs/*.c ../programs/*.h -o tmpDictC
$ZSTD -d tmp.zst -D tmpDictC -fo result && die "wrong dictionary not detected!"
$ECHO "- Create dictionary with short dictID"
$ZSTD --train --cover=k=46,d=8 *.c ../programs/*.c --dictID 1 -o tmpDict1
cmp tmpDict tmpDict1 && die "dictionaries should have different ID !"
$ECHO "- Create dictionary with size limit"
$ZSTD --train --optimize-cover=steps=8 *.c ../programs/*.c -o tmpDict2 --maxdict 4K
rm tmp*


$ECHO "\n**** integrity tests **** "

$ECHO "test one file (tmp1.zst) "
./datagen > tmp1
$ZSTD tmp1
$ZSTD -t tmp1.zst
$ZSTD --test tmp1.zst
$ECHO "test multiple files (*.zst) "
$ZSTD -t *.zst
$ECHO "test bad files (*) "
$ZSTD -t * && die "bad files not detected !"
$ZSTD -t tmp1 && die "bad file not detected !"
cp tmp1 tmp2.zst
$ZSTD -t tmp2.zst && die "bad file not detected !"
./datagen -g0 > tmp3
$ZSTD -t tmp3 && die "bad file not detected !"   # detects 0-sized files as bad
$ECHO "test --rm and --test combined "
$ZSTD -t --rm tmp1.zst
ls -ls tmp1.zst  # check file is still present


$ECHO "\n**** benchmark mode tests **** "

$ECHO "bench one file"
./datagen > tmp1
$ZSTD -bi1 tmp1
$ECHO "bench multiple levels"
$ZSTD -i1b1e3 tmp1
$ECHO "with recursive and quiet modes"
$ZSTD -rqi1b1e3 tmp1


$ECHO "\n**** zstd round-trip tests **** "

roundTripTest
roundTripTest -g15K       # TableID==3
roundTripTest -g127K      # TableID==2
roundTripTest -g255K      # TableID==1
roundTripTest -g513K      # TableID==0
roundTripTest -g512K 6    # greedy, hash chain
roundTripTest -g512K 16   # btlazy2
roundTripTest -g512K 19   # btopt

rm tmp*

if [ "$1" != "--test-large-data" ]; then
    $ECHO "Skipping large data tests"
    exit 0
fi

roundTripTest -g270000000 1
roundTripTest -g270000000 2
roundTripTest -g270000000 3

roundTripTest -g140000000 -P60 4
roundTripTest -g140000000 -P60 5
roundTripTest -g140000000 -P60 6

roundTripTest -g70000000 -P70 7
roundTripTest -g70000000 -P70 8
roundTripTest -g70000000 -P70 9

roundTripTest -g35000000 -P75 10
roundTripTest -g35000000 -P75 11
roundTripTest -g35000000 -P75 12

roundTripTest -g18000000 -P80 13
roundTripTest -g18000000 -P80 14
roundTripTest -g18000000 -P80 15
roundTripTest -g18000000 -P80 16
roundTripTest -g18000000 -P80 17

roundTripTest -g50000000 -P94 18
roundTripTest -g50000000 -P94 19

roundTripTest -g99000000 -P99 20
roundTripTest -g6000000000 -P99 1

rm tmp*
