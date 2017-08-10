#!/bin/sh -e

die() {
    $ECHO "$@" 1>&2
    exit 1
}

roundTripTest() {
    if [ -n "$3" ]; then
        cLevel="$3"
        proba="$2"
    else
        cLevel="$2"
        proba=""
    fi

    rm -f tmp1 tmp2
    $ECHO "roundTripTest: ./datagen $1 $proba | $ZSTD -v$cLevel | $ZSTD -d"
    ./datagen $1 $proba | $MD5SUM > tmp1
    ./datagen $1 $proba | $ZSTD --ultra -v$cLevel | $ZSTD -d  | $MD5SUM > tmp2
    $DIFF -q tmp1 tmp2
}

fileRoundTripTest() {
    if [ -n "$3" ]; then
        local_c="$3"
        local_p="$2"
    else
        local_c="$2"
        local_p=""
    fi

    rm -f tmp.zstd tmp.md5.1 tmp.md5.2
    $ECHO "fileRoundTripTest: ./datagen $1 $local_p > tmp && $ZSTD -v$local_c -c tmp | $ZSTD -d"
    ./datagen $1 $local_p > tmp
    cat tmp | $MD5SUM > tmp.md5.1
    $ZSTD --ultra -v$local_c -c tmp | $ZSTD -d | $MD5SUM > tmp.md5.2
    $DIFF -q tmp.md5.1 tmp.md5.2
}

isTerminal=false
if [ -t 0 ] && [ -t 1 ]
then
    isTerminal=true
fi

isWindows=false
ECHO="echo -e"
INTOVOID="/dev/null"
case "$OS" in
  Windows*)
    isWindows=true
    INTOVOID="NUL"
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

if [ -n "$(echo hello | $ZSTD -v -T2 2>&1 > $INTOVOID | grep 'multi-threading is disabled')" ]
then
    hasMT=""
else
    hasMT="true"
fi

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
test -f tmpCompressed   # file must be created
$ECHO "test : -o must be followed by filename (must fail)"
$ZSTD tmp -of tmpCompressed && die "-o must be followed by filename "
$ECHO "test : force write, correct order"
$ZSTD tmp -fo tmpCompressed
$ECHO "test : forgotten argument"
cp tmp tmp2
$ZSTD tmp2 -fo && die "-o must be followed by filename "
$ECHO "test : implied stdout when input is stdin"
$ECHO bob | $ZSTD | $ZSTD -d
if [ "$isTerminal" = true ]; then
$ECHO "test : compressed data to terminal"
$ECHO bob | $ZSTD && die "should have refused : compressed data to terminal"
$ECHO "test : compressed data from terminal (a hang here is a test fail, zstd is wrongly waiting on data from terminal)"
$ZSTD -d > $INTOVOID && die "should have refused : compressed data from terminal"
fi
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
$ECHO "test : overwrite readonly file"
rm -f tmpro tmpro.zst
$ECHO foo > tmpro.zst
$ECHO foo > tmpro
chmod 400 tmpro.zst
$ZSTD -q tmpro && die "should have refused to overwrite read-only file"
$ZSTD -q -f tmpro
rm -f tmpro tmpro.zst
$ECHO "test : file removal"
$ZSTD -f --rm tmp
test ! -f tmp  # tmp should no longer be present
$ZSTD -f -d --rm tmp.zst
test ! -f tmp.zst   # tmp.zst should no longer be present
$ECHO "test : --rm on stdin"
$ECHO a | $ZSTD --rm > $INTOVOID   # --rm should remain silent
rm tmp
$ZSTD -f tmp && die "tmp not present : should have failed"
test ! -f tmp.zst  # tmp.zst should not be created


$ECHO "\n**** Advanced compression parameters **** "
$ECHO "Hello world!" | $ZSTD --zstd=windowLog=21,      - -o tmp.zst && die "wrong parameters not detected!"
$ECHO "Hello world!" | $ZSTD --zstd=windowLo=21        - -o tmp.zst && die "wrong parameters not detected!"
$ECHO "Hello world!" | $ZSTD --zstd=windowLog=21,slog  - -o tmp.zst && die "wrong parameters not detected!"
test ! -f tmp.zst  # tmp.zst should not be created
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


$ECHO "\n**** symbolic link test **** "

rm -f hello.tmp world.tmp hello.tmp.zst world.tmp.zst
$ECHO "hello world" > hello.tmp
ln -s hello.tmp world.tmp
$ZSTD world.tmp hello.tmp
test -f hello.tmp.zst  # regular file should have been compressed!
test ! -f world.tmp.zst  # symbolic link should not have been compressed!
$ZSTD world.tmp hello.tmp -f
test -f world.tmp.zst  # symbolic link should have been compressed with --force
rm -f hello.tmp world.tmp hello.tmp.zst world.tmp.zst

fi


$ECHO "\n**** test sparse file support **** "

./datagen -g5M  -P100 > tmpSparse
$ZSTD tmpSparse -c | $ZSTD -dv -o tmpSparseRegen
$DIFF -s tmpSparse tmpSparseRegen
$ZSTD tmpSparse -c | $ZSTD -dv --sparse -c > tmpOutSparse
$DIFF -s tmpSparse tmpOutSparse
$ZSTD tmpSparse -c | $ZSTD -dv --no-sparse -c > tmpOutNoSparse
$DIFF -s tmpSparse tmpOutNoSparse
ls -ls tmpSparse*  # look at file size and block size on disk
./datagen -s1 -g1200007 -P100 | $ZSTD | $ZSTD -dv --sparse -c > tmpSparseOdd   # Odd size file (to not finish on an exact nb of blocks)
./datagen -s1 -g1200007 -P100 | $DIFF -s - tmpSparseOdd
ls -ls tmpSparseOdd  # look at file size and block size on disk
$ECHO "\n Sparse Compatibility with Console :"
$ECHO "Hello World 1 !" | $ZSTD | $ZSTD -d -c
$ECHO "Hello World 2 !" | $ZSTD | $ZSTD -d | cat
$ECHO "\n Sparse Compatibility with Append :"
./datagen -P100 -g1M > tmpSparse1M
cat tmpSparse1M tmpSparse1M > tmpSparse2M
$ZSTD -v -f tmpSparse1M -o tmpSparseCompressed
$ZSTD -d -v -f tmpSparseCompressed -o tmpSparseRegenerated
$ZSTD -d -v -f tmpSparseCompressed -c >> tmpSparseRegenerated
ls -ls tmpSparse*  # look at file size and block size on disk
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
ls -ls tmp*  # check size of tmpall (should be tmp1.zst + tmp2.zst + tmp3.zst)
$ECHO "decompress tmpall* into stdout > tmpdec : "
cp tmpall tmpall2
$ZSTD -dc tmpall* > tmpdec
ls -ls tmp* # check size of tmpdec (should be 2*(tmp1 + tmp2 + tmp3))
$ECHO "compress multiple files including a missing one (notHere) : "
$ZSTD -f tmp1 notHere tmp2 && die "missing file not detected!"


$ECHO "\n**** dictionary tests **** "

$ECHO "- test with raw dict (content only) "
./datagen > tmpDict
./datagen -g1M | $MD5SUM > tmp1
./datagen -g1M | $ZSTD -D tmpDict | $ZSTD -D tmpDict -dvq | $MD5SUM > tmp2
$DIFF -q tmp1 tmp2
$ECHO "- Create first dictionary "
TESTFILE=../programs/zstdcli.c
$ZSTD --train *.c ../programs/*.c -o tmpDict
cp $TESTFILE tmp
$ZSTD -f tmp -D tmpDict
$ZSTD -d tmp.zst -D tmpDict -fo result
$DIFF $TESTFILE result
$ECHO "- Create second (different) dictionary "
$ZSTD --train *.c ../programs/*.c ../programs/*.h -o tmpDictC
$ZSTD -d tmp.zst -D tmpDictC -fo result && die "wrong dictionary not detected!"
$ECHO "- Create dictionary with short dictID"
$ZSTD --train *.c ../programs/*.c --dictID=1 -o tmpDict1
cmp tmpDict tmpDict1 && die "dictionaries should have different ID !"
$ECHO "- Create dictionary with wrong dictID parameter order (must fail)"
$ZSTD --train *.c ../programs/*.c --dictID -o 1 tmpDict1 && die "wrong order : --dictID must be followed by argument "
$ECHO "- Create dictionary with size limit"
$ZSTD --train *.c ../programs/*.c -o tmpDict2 --maxdict=4K -v
$ECHO "- Create dictionary with wrong parameter order (must fail)"
$ZSTD --train *.c ../programs/*.c -o tmpDict2 --maxdict -v 4K && die "wrong order : --maxdict must be followed by argument "
$ECHO "- Compress without dictID"
$ZSTD -f tmp -D tmpDict1 --no-dictID
$ZSTD -d tmp.zst -D tmpDict -fo result
$DIFF $TESTFILE result
$ECHO "- Compress with wrong argument order (must fail)"
$ZSTD tmp -Df tmpDict1 -c > $INTOVOID && die "-D must be followed by dictionary name "
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
$ECHO "- dictionary builder on bogus input"
$ECHO "Hello World" > tmp
$ZSTD --train-legacy -q tmp && die "Dictionary training should fail : not enough input source"
./datagen -P0 -g10M > tmp
$ZSTD --train-legacy -q tmp && die "Dictionary training should fail : source is pure noise"
rm tmp*


$ECHO "\n**** cover dictionary tests **** "

TESTFILE=../programs/zstdcli.c
./datagen > tmpDict
$ECHO "- Create first dictionary"
$ZSTD --train-cover=k=46,d=8 *.c ../programs/*.c -o tmpDict
cp $TESTFILE tmp
$ZSTD -f tmp -D tmpDict
$ZSTD -d tmp.zst -D tmpDict -fo result
$DIFF $TESTFILE result
$ECHO "- Create second (different) dictionary"
$ZSTD --train-cover=k=56,d=8 *.c ../programs/*.c ../programs/*.h -o tmpDictC
$ZSTD -d tmp.zst -D tmpDictC -fo result && die "wrong dictionary not detected!"
$ECHO "- Create dictionary with short dictID"
$ZSTD --train-cover=k=46,d=8 *.c ../programs/*.c --dictID=1 -o tmpDict1
cmp tmpDict tmpDict1 && die "dictionaries should have different ID !"
$ECHO "- Create dictionary with size limit"
$ZSTD --train-cover=steps=8 *.c ../programs/*.c -o tmpDict2 --maxdict=4K
rm tmp*

$ECHO "\n**** legacy dictionary tests **** "

TESTFILE=../programs/zstdcli.c
./datagen > tmpDict
$ECHO "- Create first dictionary"
$ZSTD --train-legacy=selectivity=8 *.c ../programs/*.c -o tmpDict
cp $TESTFILE tmp
$ZSTD -f tmp -D tmpDict
$ZSTD -d tmp.zst -D tmpDict -fo result
$DIFF $TESTFILE result
$ECHO "- Create second (different) dictionary"
$ZSTD --train-legacy=s=5 *.c ../programs/*.c ../programs/*.h -o tmpDictC
$ZSTD -d tmp.zst -D tmpDictC -fo result && die "wrong dictionary not detected!"
$ECHO "- Create dictionary with short dictID"
$ZSTD --train-legacy -s5 *.c ../programs/*.c --dictID=1 -o tmpDict1
cmp tmpDict tmpDict1 && die "dictionaries should have different ID !"
$ECHO "- Create dictionary with size limit"
$ZSTD --train-legacy -s9 *.c ../programs/*.c -o tmpDict2 --maxdict=4K
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
test -f tmp1.zst   # check file is still present
split -b16384 tmp1.zst tmpSplit.
$ZSTD -t tmpSplit.* && die "bad file not detected !"
./datagen | $ZSTD -c | $ZSTD -t



$ECHO "\n**** golden files tests **** "

$ZSTD -t -r files
$ZSTD -c -r files | $ZSTD -t


$ECHO "\n**** benchmark mode tests **** "

$ECHO "bench one file"
./datagen > tmp1
$ZSTD -bi0 tmp1
$ECHO "bench multiple levels"
$ZSTD -i0b0e3 tmp1
$ECHO "with recursive and quiet modes"
$ZSTD -rqi1b1e2 tmp1


$ECHO "\n**** gzip compatibility tests **** "

GZIPMODE=1
$ZSTD --format=gzip -V || GZIPMODE=0
if [ $GZIPMODE -eq 1 ]; then
    $ECHO "gzip support detected"
    GZIPEXE=1
    gzip -V || GZIPEXE=0
    if [ $GZIPEXE -eq 1 ]; then
        ./datagen > tmp
        $ZSTD --format=gzip -f tmp
        gzip -t -v tmp.gz
        gzip -f tmp
        $ZSTD -d -f -v tmp.gz
        rm tmp*
    else
        $ECHO "gzip binary not detected"
    fi
else
    $ECHO "gzip mode not supported"
fi


$ECHO "\n**** gzip frame tests **** "

if [ $GZIPMODE -eq 1 ]; then
    ./datagen > tmp
    $ZSTD -f --format=gzip tmp
    $ZSTD -f tmp
    cat tmp.gz tmp.zst tmp.gz tmp.zst | $ZSTD -d -f -o tmp
    head -c -1 tmp.gz | $ZSTD -t > $INTOVOID && die "incomplete frame not detected !"
    rm tmp*
else
    $ECHO "gzip mode not supported"
fi


$ECHO "\n**** xz compatibility tests **** "

LZMAMODE=1
$ZSTD --format=xz -V || LZMAMODE=0
if [ $LZMAMODE -eq 1 ]; then
    $ECHO "xz support detected"
    XZEXE=1
    xz -V && lzma -V || XZEXE=0
    if [ $XZEXE -eq 1 ]; then
        $ECHO "Testing zstd xz and lzma support"
        ./datagen > tmp
        $ZSTD --format=lzma -f tmp
        $ZSTD --format=xz -f tmp
        xz -t -v tmp.xz
        xz -t -v tmp.lzma
        xz -f -k tmp
        lzma -f -k --lzma1 tmp
        $ZSTD -d -f -v tmp.xz
        $ZSTD -d -f -v tmp.lzma
        rm tmp*
        $ECHO "Creating symlinks"
        ln -s $ZSTD ./xz
        ln -s $ZSTD ./unxz
        ln -s $ZSTD ./lzma
        ln -s $ZSTD ./unlzma
        $ECHO "Testing xz and lzma symlinks"
        ./datagen > tmp
        ./xz tmp
        xz -d tmp.xz
        ./lzma tmp
        lzma -d tmp.lzma
        $ECHO "Testing unxz and unlzma symlinks"
        xz tmp
        ./xz -d tmp.xz
        lzma tmp
        ./lzma -d tmp.lzma
        rm xz unxz lzma unlzma
        rm tmp*
    else
        $ECHO "xz binary not detected"
    fi
else
    $ECHO "xz mode not supported"
fi


$ECHO "\n**** xz frame tests **** "

if [ $LZMAMODE -eq 1 ]; then
    ./datagen > tmp
    $ZSTD -f --format=xz tmp
    $ZSTD -f --format=lzma tmp
    $ZSTD -f tmp
    cat tmp.xz tmp.lzma tmp.zst tmp.lzma tmp.xz tmp.zst | $ZSTD -d -f -o tmp
    head -c -1 tmp.xz | $ZSTD -t > $INTOVOID && die "incomplete frame not detected !"
    head -c -1 tmp.lzma | $ZSTD -t > $INTOVOID && die "incomplete frame not detected !"
    rm tmp*
else
    $ECHO "xz mode not supported"
fi

$ECHO "\n**** lz4 compatibility tests **** "

LZ4MODE=1
$ZSTD --format=lz4 -V || LZ4MODE=0
if [ $LZ4MODE -eq 1 ]; then
    $ECHO "lz4 support detected"
    LZ4EXE=1
    lz4 -V || LZ4EXE=0
    if [ $LZ4EXE -eq 1 ]; then
        ./datagen > tmp
        $ZSTD --format=lz4 -f tmp
        lz4 -t -v tmp.lz4
        lz4 -f tmp
        $ZSTD -d -f -v tmp.lz4
        rm tmp*
    else
        $ECHO "lz4 binary not detected"
    fi
else
    $ECHO "lz4 mode not supported"
fi


$ECHO "\n**** lz4 frame tests **** "

if [ $LZ4MODE -eq 1 ]; then
    ./datagen > tmp
    $ZSTD -f --format=lz4 tmp
    $ZSTD -f tmp
    cat tmp.lz4 tmp.zst tmp.lz4 tmp.zst | $ZSTD -d -f -o tmp
    head -c -1 tmp.lz4 | $ZSTD -t > $INTOVOID && die "incomplete frame not detected !"
    rm tmp*
else
    $ECHO "lz4 mode not supported"
fi

$ECHO "\n**** zstd round-trip tests **** "

roundTripTest
roundTripTest -g15K       # TableID==3
roundTripTest -g127K      # TableID==2
roundTripTest -g255K      # TableID==1
roundTripTest -g522K      # TableID==0
roundTripTest -g519K 6    # greedy, hash chain
roundTripTest -g517K 16   # btlazy2
roundTripTest -g516K 19   # btopt

fileRoundTripTest -g500K

if [ -n "$hasMT" ]
then
    $ECHO "\n**** zstdmt round-trip tests **** "
    roundTripTest -g4M "1 -T0"
    roundTripTest -g8M "3 -T2"
    roundTripTest -g8000K "2 --threads=2"
    fileRoundTripTest -g4M "19 -T2 -B1M"
else
    $ECHO "\n**** no multithreading, skipping zstdmt tests **** "
fi

rm tmp*

$ECHO "\n**** zstd --list/-l single frame tests ****"
./datagen > tmp1
./datagen > tmp2
./datagen > tmp3
./datagen > tmp4
$ZSTD tmp*
$ZSTD -l *.zst
$ZSTD -lv *.zst
$ZSTD --list *.zst
$ZSTD --list -v *.zst

$ECHO "\n**** zstd --list/-l multiple frame tests ****"
cat tmp1.zst tmp2.zst > tmp12.zst
cat tmp3.zst tmp4.zst > tmp34.zst
cat tmp12.zst tmp34.zst > tmp1234.zst
cat tmp12.zst tmp4.zst > tmp124.zst
$ZSTD -l *.zst
$ZSTD -lv *.zst
$ZSTD --list *.zst
$ZSTD --list -v *.zst

$ECHO "\n**** zstd --list/-l error detection tests ****"
! $ZSTD -l tmp1 tmp1.zst
! $ZSTD --list tmp*
! $ZSTD -lv tmp1*
! $ZSTD --list -v tmp2 tmp23.zst

$ECHO "\n**** zstd --list/-l test with null files ****"
./datagen -g0 > tmp5
$ZSTD tmp5
$ZSTD -l tmp5.zst
! $ZSTD -l tmp5*
$ZSTD -lv tmp5.zst
! $ZSTD -lv tmp5*

$ECHO "\n**** zstd --list/-l test with no content size field ****"
./datagen -g1MB | $ZSTD > tmp6.zst
$ZSTD -l tmp6.zst
$ZSTD -lv tmp6.zst

$ECHO "\n**** zstd --list/-l test with no checksum ****"
$ZSTD -f --no-check tmp1
$ZSTD -l tmp1.zst
$ZSTD -lv tmp1.zst

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

roundTripTest -g18000013 -P80 13
roundTripTest -g18000014 -P80 14
roundTripTest -g18000015 -P80 15
roundTripTest -g18000016 -P80 16
roundTripTest -g18000017 -P80 17
roundTripTest -g18000018 -P94 18
roundTripTest -g18000019 -P94 19

roundTripTest -g68000020 -P99 20
roundTripTest -g6000000000 -P99 1

fileRoundTripTest -g4193M -P99 1

if [ -n "$hasMT" ]
then
    $ECHO "\n**** zstdmt long round-trip tests **** "
    roundTripTest -g99000000 -P99 "20 -T2"
    roundTripTest -g6000000000 -P99 "1 -T2"
    roundTripTest -g1500000000 -P97 "1 -T999"
    fileRoundTripTest -g4195M -P98 " -T0"
else
    $ECHO "\n**** no multithreading, skipping zstdmt tests **** "
fi

rm tmp*
