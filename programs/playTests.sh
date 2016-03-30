#!/bin/sh -e

die() {
    echo "$@" 1>&2
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
    echo "roundTripTest: ./datagen $1 $p | $ZSTD -v$c | $ZSTD -d"
    ./datagen $1 $p | md5sum > tmp1
    ./datagen $1 $p | $ZSTD -vq$c | $ZSTD -d  | md5sum > tmp2
    diff -q tmp1 tmp2
}

[ -n "$ZSTD" ] || die "ZSTD variable must be defined!"


echo "\n**** simple tests **** "
./datagen > tmp
echo -n "trivial compression : "
$ZSTD -f tmp
echo "OK"
$ZSTD -99 tmp && die "too large compression level undetected"
$ZSTD tmp -c > tmpCompressed
$ZSTD tmp --stdout > tmpCompressed
$ZSTD -d tmpCompressed && die "wrong suffix error not detected!"
$ZSTD -d tmpCompressed -c > tmpResult
$ZSTD --decompress tmpCompressed -c > tmpResult
$ZSTD --decompress tmpCompressed --stdout > tmpResult
$ZSTD -q tmp && die "overwrite check failed!"
$ZSTD -q -f tmp
$ZSTD -q --force tmp
$ZSTD -df tmp && die "should have refused : wrong extension"
cp tmp tmp2.zst
$ZSTD -df tmp2.zst && die "should have failed : wrong format"
rm tmp2.zst

echo "\n**** frame concatenation **** "

echo "hello " > hello.tmp
echo "world!" > world.tmp
cat hello.tmp world.tmp > helloworld.tmp
$ZSTD -c hello.tmp > hello.zstd
$ZSTD -c world.tmp > world.zstd
cat hello.zstd world.zstd > helloworld.zstd
$ZSTD -dc helloworld.zstd > result.tmp
cat result.tmp
sdiff helloworld.tmp result.tmp
rm ./*.tmp ./*.zstd

echo frame concatenation test completed


echo "\n**** flush write error test **** "

echo "echo foo | $ZSTD > /dev/full"
echo foo | $ZSTD > /dev/full && die "write error not detected!"
echo "echo foo | $ZSTD | $ZSTD -d > /dev/full"
echo foo | $ZSTD | $ZSTD -d > /dev/full && die "write error not detected!"


echo "\n**** dictionary tests **** "

./datagen > tmpDict
./datagen -g1M | md5sum > tmp1
./datagen -g1M | $ZSTD -D tmpDict | $ZSTD -D tmpDict -dvq | md5sum > tmp2
diff -q tmp1 tmp2
$ZSTD --train *.c *.h -o tmpDict
$ZSTD xxhash.c -D tmpDict -of tmp
$ZSTD -d tmp -D tmpDict -of result
diff xxhash.c result


echo "\n**** multiple files tests **** "

./datagen -s1        > tmp1 2> /dev/null
./datagen -s2 -g100K > tmp2 2> /dev/null
./datagen -s3 -g1M   > tmp3 2> /dev/null
$ZSTD -f tmp*
echo "compress tmp* : "
ls -ls tmp*
rm tmp1 tmp2 tmp3
echo "decompress tmp* : "
$ZSTD -df *.zst
ls -ls tmp*
echo "compress tmp* into stdout > tmpall : "
$ZSTD -c tmp1 tmp2 tmp3 > tmpall
ls -ls tmp*
echo "decompress tmpall* into stdout > tmpdec : "
cp tmpall tmpall2
$ZSTD -dc tmpall* > tmpdec
ls -ls tmp*
echo "compress multiple files including a missing one (notHere) : "
$ZSTD -f tmp1 notHere tmp2 && die "missing file not detected!"

echo "\n**** integrity tests **** "
echo "test one file (tmp1.zst) "
$ZSTD -t tmp1.zst
$ZSTD --test tmp1.zst
echo "test multiple files (*.zst) "
$ZSTD -t *.zst
echo "test good and bad files (*) "
$ZSTD -t * && die "bad files not detected !"

echo "\n**** zstd round-trip tests **** "

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
    echo "Skipping large data tests"
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

