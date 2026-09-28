#!/bin/sh

set -e

# Test multi-threaded flags
zstd --single-thread file -f -q         ; zstd -t file.zst
zstd -T2 -f file -q                     ; zstd -t file.zst
zstd --rsyncable -f file -q             ; zstd -t file.zst
zstd -T0 -f file -q                     ; zstd -t file.zst
zstd -T0 --auto-threads=logical -f file -q ; zstd -t file.zst
zstd -T0 --auto-threads=physical -f file -q ; zstd -t file.zst
zstd -T0 --jobsize=1M -f file -q        ; zstd -t file.zst

# multi-thread decompression warning test
zstd -T0 -f file -q                     ; zstd -t file.zst; zstd -T0 -d file.zst -o file3
zstd -T0 -f file -q                     ; zstd -t file.zst; zstd -T2 -d file.zst -o file4
# setting multi-thread via environment variable does not trigger decompression warning
zstd -T0 -f file -q                     ; zstd -t file.zst; ZSTD_NBTHREADS=0 zstd -df file.zst -o file3
zstd -T0 -f file -q                     ; zstd -t file.zst; ZSTD_NBTHREADS=2 zstd -df file.zst -o file4
# setting nbThreads==1 does not trigger decompression warning
zstd -T0 -f file -q                     ; zstd -t file.zst; zstd -T1 -df file.zst -o file3
zstd -T0 -f file -q                     ; zstd -t file.zst; zstd -T2 -T1 -df file.zst -o file4
