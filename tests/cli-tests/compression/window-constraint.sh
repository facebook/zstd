#!/bin/sh
datagen -g256M > file

zstd --long=30 --single-thread --constrain-window=http-zstd -f < file > file.zst
zstd -l -v file.zst

zstd --long=30 --single-thread --constrain-window=http-dcz -f < file > file.zst
zstd -l -v file.zst

cp file dict
zstd --long=30 --single-thread --constrain-window=http-dcz --patch-from dict -f file
zstd -l -v file.zst

rm dict file file.zst
