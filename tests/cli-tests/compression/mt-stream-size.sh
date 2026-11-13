#!/bin/sh

. "$COMMON/platform.sh"

set -e

if [ -z "$hasMT" ]; then
    cat "$CLI_TESTS/compression/mt-stream-size.sh.stdout.exact"
    exit 0
fi

# The input must be large enough to span several jobs, otherwise zstd falls
# back to compressing it with a single thread.
datagen -g2M > file2M
SIZE=2097152

echo "over-declared --stream-size must fail"
zstd -T2 -B512K --stream-size=$((SIZE + 1)) < file2M > $INTOVOID 2> stderr && die "should have failed"
ret=$?
[ "$ret" -eq 11 ] || die "expected exit code 11, got $ret"
grep -q "Src size is incorrect" stderr || die "expected a 'Src size is incorrect' error"

echo "under-declared --stream-size must fail"
zstd -T2 -B512K --stream-size=$((SIZE - 1)) < file2M > $INTOVOID 2> stderr && die "should have failed"
ret=$?
[ "$ret" -eq 11 ] || die "expected exit code 11, got $ret"
grep -q "Src size is incorrect" stderr || die "expected a 'Src size is incorrect' error"

echo "exact --stream-size must succeed and round-trip"
zstd -T2 -B512K --stream-size=$SIZE < file2M > file2M.zst
zstd -d < file2M.zst | cmp - file2M

echo "no --stream-size must still succeed and round-trip"
zstd -T2 -B512K < file2M | zstd -d | cmp - file2M
