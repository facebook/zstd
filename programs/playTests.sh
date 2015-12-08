#!/bin/sh -e

die() {
    echo "$@" 1>&2
    exit 1
}

echo "\n**** frame concatenation **** "

echo "hello " > hello.tmp
echo "world!" > world.tmp
cat hello.tmp world.tmp > helloworld.tmp
$ZSTD hello.tmp > hello.zstd
$ZSTD world.tmp > world.zstd
cat hello.zstd world.zstd > helloworld.zstd
$ZSTD -df helloworld.zstd > result.tmp
cat result.tmp
sdiff helloworld.tmp result.tmp
rm *.tmp *.zstd

echo frame concatenation test completed

echo "**** flush write error test **** "

echo foo | $ZSTD > /dev/full && die "write error not detected!"
echo foo | $ZSTD | $ZSTD -d > /dev/full && die "write error not detected!"

echo "**** zstd round-trip tests **** "
./datagen             | md5sum > tmp1
./datagen              | $ZSTD -v    | $ZSTD -d  | md5sum > tmp2
diff tmp1 tmp2
./datagen              | $ZSTD -6 -v | $ZSTD -d  | md5sum > tmp2
diff tmp1 tmp2
./datagen -g270000000 | md5sum > tmp1
./datagen -g270000000  | $ZSTD -v    | $ZSTD -d  | md5sum > tmp2
diff tmp1 tmp2
./datagen -g270000000  | $ZSTD -v2   | $ZSTD -d  | md5sum > tmp2
diff tmp1 tmp2
./datagen -g270000000  | $ZSTD -v3   | $ZSTD -d  | md5sum > tmp2
diff tmp1 tmp2
./datagen -g140000000 -P60| md5sum > tmp1
./datagen -g140000000 -P60 | $ZSTD -v4   | $ZSTD -d  | md5sum > tmp2
diff tmp1 tmp2
./datagen -g140000000 -P60 | $ZSTD -v5   | $ZSTD -d  | md5sum > tmp2
diff tmp1 tmp2
./datagen -g140000000 -P60 | $ZSTD -v6   | $ZSTD -d  | md5sum > tmp2
diff tmp1 tmp2
./datagen -g70000000 -P70 | md5sum > tmp1
./datagen -g70000000 -P70  | $ZSTD -v7   | $ZSTD -d  | md5sum > tmp2
diff tmp1 tmp2
./datagen -g70000000 -P70  | $ZSTD -v8   | $ZSTD -d  | md5sum > tmp2
diff tmp1 tmp2
./datagen -g70000000 -P70  | $ZSTD -v9   | $ZSTD -d  | md5sum > tmp2
diff tmp1 tmp2
./datagen -g35000000 -P75 | md5sum > tmp1
./datagen -g35000000 -P75  | $ZSTD -v10  | $ZSTD -d  | md5sum > tmp2
diff tmp1 tmp2
./datagen -g35000000 -P75  | $ZSTD -v11  | $ZSTD -d  | md5sum > tmp2
diff tmp1 tmp2
./datagen -g35000000 -P75  | $ZSTD -v12  | $ZSTD -d  | md5sum > tmp2
diff tmp1 tmp2

