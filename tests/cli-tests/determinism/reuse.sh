#!/bin/sh

. "$COMMON/platform.sh"

set -e

# To update checksums on version change run this from the tests/ directory
# make update-cli-tests

if [ -n "$NON_DETERMINISTIC" ]; then
    # Skip tests if we have a non-deterministic build
    cat "$CLI_TESTS/determinism/reuse.sh.stdout.exact"
    exit 0
fi

datagen -g0 > file0
datagen -g1 > file1
datagen -g1000 > file1000
datagen -g100000 > file100000

validate() {
    $DIFF file0.zst file0.zst.good
    $DIFF file1.zst file1.zst.good
    $DIFF file1000.zst file1000.zst.good
    $DIFF file100000.zst file100000.zst.good
}

# Check that context reuse doesn't impact determinism
for level in $(seq 1 19); do
    echo $level
    zstd -qf --single-thread -$level file0 -o file0.zst.good
    zstd -qf --single-thread -$level file1 -o file1.zst.good
    zstd -qf --single-thread -$level file1000 -o file1000.zst.good
    zstd -qf --single-thread -$level file100000 -o file100000.zst.good

    zstd -qf --single-thread -$level file0 file1 file1000 file100000
    validate
    zstd -qf --single-thread -$level file1 file0 file1000 file100000
    validate
    zstd -qf --single-thread -$level file1000 file1 file0 file100000
    validate
    zstd -qf --single-thread -$level file100000 file1000 file1 file0
    validate
done
