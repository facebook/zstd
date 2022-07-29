#!/bin/sh

set -e

# Test multi-threaded flags
zstd --single-thread file -f            ; zstd -t file.zst
zstd -T2 -f file                        ; zstd -t file.zst
zstd --rsyncable -f file                ; zstd -t file.zst
zstd -T0 -f file                        ; zstd -t file.zst
zstd -T0 --auto-threads=logical -f file ; zstd -t file.zst
zstd -T0 --auto-threads=physical -f file; zstd -t file.zst

# multi-thread decompression warning test
zstd -T0 -f file                        ; zstd -t file.zst; zstd -T0 -d file.zst -o file3
zstd -T0 -f file                        ; zstd -t file.zst; zstd -T2 -d file.zst -o file4
