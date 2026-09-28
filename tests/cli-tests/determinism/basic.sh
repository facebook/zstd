#!/bin/sh

. "$COMMON/platform.sh"

set -e

# To update checksums on version change run this from the tests/ directory
# make update-cli-tests

if [ -n "$NON_DETERMINISTIC" ]; then
    # Skip tests if we have a non-deterministic build
    cat "$CLI_TESTS/determinism/basic.sh.stdout.exact"
    exit 0
fi

for level in $(seq 1 19); do
    for file in $(ls files/); do
        file="files/$file"
        echo "level $level, file $file"
        zstd --single-thread -q -$level $file -c | md5hash
    done
done

for file in $(ls files/); do
    file="files/$file"
    echo "level 1, long=18, file $file"
    zstd --long=18 --single-thread -q -1 $file -c | md5hash
    echo "level 19, long=18, file $file"
    zstd --long=18 --single-thread -q -19 $file -c | md5hash
done

for file in $(ls files/); do
    file="files/$file"
    echo "level -1, file $file"
    zstd -q --single-thread --fast=1 $file -c | md5hash
done
