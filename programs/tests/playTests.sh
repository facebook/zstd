#!/bin/sh -e

die() {
    $ECHO "$@" 1>&2
    exit 1
}

roundTripTest() {
    if [ -n "$3" ]; then
        local c="$3"
        local p="$2"
    else
        local c="$2"
    fi

    rm -f tmp1 tmp2
    $ECHO "roundTripTest: ./datagen $1 $p | $ZSTD -v$c | $ZSTD -d"
    ./datagen $1 $p | md5sum > tmp1
    ./datagen $1 $p | $ZSTD -vq$c | $ZSTD -d  | md5sum > tmp2
    diff -q tmp1 tmp2
}

isWindows=false
ECHO="echo"
if [[ "$OS" == "Windows"* ]]; then
    isWindows=true
    ECHO="echo -e"
fi

$ECHO "\nStarting playTests.sh isWindows=$isWindows"

[ -n "$ZSTD" ] || die "ZSTD variable must be defined!"

$ECHO "\n**** simple tests **** "

./datagen > tmp
$ZSTD -f tmp                             # trivial compression case, creates tmp.zst
$ZSTD -df tmp.zst                        # trivial decompression case (overwrites tmp)
$ECHO "test : too large compression level (must fail)"
$ZSTD -99 tmp && die "too large compression level undetected"
$ECHO "test : compress to stdout"
$ZSTD tmp -c > tmpCompressed
$ZSTD tmp --stdout > tmpCompressed       # long command format
$ECHO "test : null-length file roundtrip"
$ECHO -n '' | $ZSTD - --stdout | $ZSTD -d --stdout
$ECHO "test : decompress file with wrong suffix (must fail)"
$ZSTD -d tmpCompressed && die "wrong suffix error not detected!"
$ZSTD -d tmpCompressed -c > tmpResult    # decompression using stdout
$ZSTD --decompress tmpCompressed -c > tmpResult
$ZSTD --decompress tmpCompressed --stdout > tmpResult
if [ "$isWindows" = false ] ; then
    $ZSTD -d    < tmp.zst > /dev/null        # combine decompression, stdin & stdout
    $ZSTD -d  - < tmp.zst > /dev/null
fi
$ZSTD -dc   < tmp.zst > /dev/null
$ZSTD -dc - < tmp.zst > /dev/null
$ZSTD -q tmp && die "overwrite check failed!"
$ZSTD -q -f tmp
$ZSTD -q --force tmp
$ZSTD -df tmp && die "should have refused : wrong extension"


$ECHO "\n**** Pass-Through mode **** "
$ECHO "Hello world !" | $ZSTD -df
$ECHO "Hello world !" | $ZSTD -dcf


$ECHO "\n**** frame concatenation **** "

$ECHO "hello " > hello.tmp
$ECHO "world!" > world.tmp
cat hello.tmp world.tmp > helloworld.tmp
$ZSTD -c hello.tmp > hello.zstd
$ZSTD -c world.tmp > world.zstd
cat hello.zstd world.zstd > helloworld.zstd
$ZSTD -dc helloworld.zstd > result.tmp
cat result.tmp
sdiff helloworld.tmp result.tmp
rm ./*.tmp ./*.zstd
$ECHO "frame concatenation tests completed"


if [ "$isWindows" = false ] ; then
$ECHO "\n**** flush write error test **** "

$ECHO "$ECHO foo | $ZSTD > /dev/full"
$ECHO foo | $ZSTD > /dev/full && die "write error not detected!"
$ECHO "$ECHO foo | $ZSTD | $ZSTD -d > /dev/full"
$ECHO foo | $ZSTD | $ZSTD -d > /dev/full && die "write error not detected!"
fi


$ECHO "\n**** test sparse file support **** "

./datagen -g5M  -P100 > tmpSparse
$ZSTD tmpSparse -c | $ZSTD -dv -o tmpSparseRegen
diff -s tmpSparse tmpSparseRegen
$ZSTD tmpSparse -c | $ZSTD -dv --sparse -c > tmpOutSparse
diff -s tmpSparse tmpOutSparse
$ZSTD tmpSparse -c | $ZSTD -dv --no-sparse -c > tmpOutNoSparse
diff -s tmpSparse tmpOutNoSparse
ls -ls tmpSparse*
./datagen -s1 -g1200007 -P100 | $ZSTD | $ZSTD -dv --sparse -c > tmpSparseOdd   # Odd size file (to not finish on an exact nb of blocks)
./datagen -s1 -g1200007 -P100 | diff -s - tmpSparseOdd
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
diff tmpSparse2M tmpSparseRegenerated
rm tmpSparse*


$ECHO "\n**** dictionary tests **** "

./datagen > tmpDict
./datagen -g1M | md5sum > tmp1
./datagen -g1M | $ZSTD -D tmpDict | $ZSTD -D tmpDict -dvq | md5sum > tmp2
diff -q tmp1 tmp2
$ECHO "Create first dictionary"
$ZSTD --train *.c -o tmpDict
cp zstdcli.c tmp
$ZSTD -f tmp -D tmpDict
$ZSTD -d tmp.zst -D tmpDict -of result
diff zstdcli.c result
$ECHO "Create second (different) dictionary"
$ZSTD --train *.c *.h -o tmpDictC
$ZSTD -d tmp.zst -D tmpDictC -of result && die "wrong dictionary not detected!"
$ECHO "Create dictionary with short dictID"
$ZSTD --train *.c --dictID 1 -o tmpDict1
cmp tmpDict tmpDict1 && die "dictionaries should have different ID !"
$ECHO "Compress without dictID"
$ZSTD -f tmp -D tmpDict1 --no-dictID
$ZSTD -d tmp.zst -D tmpDict -of result
diff zstdcli.c result
rm tmp*


$ECHO "\n**** multiple files tests **** "

./datagen -s1        > tmp1 2> /dev/null
./datagen -s2 -g100K > tmp2 2> /dev/null
./datagen -s3 -g1M   > tmp3 2> /dev/null
$ZSTD -f tmp*
$ECHO "compress tmp* : "
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


$ECHO "\n**** integrity tests **** "

$ECHO "test one file (tmp1.zst) "
$ZSTD -t tmp1.zst
$ZSTD --test tmp1.zst
$ECHO "test multiple files (*.zst) "
$ZSTD -t *.zst
$ECHO "test good and bad files (*) "
$ZSTD -t * && die "bad files not detected !"


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
