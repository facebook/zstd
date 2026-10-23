#!/bin/sh

set -e

datagen > file
cp file file.orig

# The same path spelled differently must still be detected as the input file.
# On Windows the old strcmp fallback treated "./file" as a different file and
# truncated the input before reading it.
println "+ zstd -q -f file -o ./file"
zstd -q -f file -o ./file && die "Should not overwrite the input file"

cmp file file.orig
