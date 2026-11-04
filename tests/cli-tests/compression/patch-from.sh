#!/bin/sh

set -e

# Test --patch-from with an empty file to patch
datagen -g1000 -P50 > ref
touch empty
zstd --patch-from=ref empty -o empty.zst
zstd -d --patch-from=ref empty.zst -o empty.out
cmp empty empty.out
