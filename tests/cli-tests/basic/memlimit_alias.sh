#!/bin/sh
set -eu

datagen -g500K > input
zstd -q input
for option in --memlimit --memory --memlimit-decompress; do
    zstd -dc "$option=8M" input.zst > output
    cmp input output
    zstd -dc "$option" 8M input.zst > output
    cmp input output
    if zstd -qdc "$option=1K" input.zst > output 2> error; then
        die "Decompression exceeded the memory limit"
    fi
    grep -q 'Window size larger than maximum' error
done
