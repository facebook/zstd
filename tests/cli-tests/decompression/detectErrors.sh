#!/bin/sh

set -e

GOLDEN_DIR="$ZSTD_REPO_DIR/tests/golden-decompression-errors/"

for file in "$GOLDEN_DIR"/*; do
    zstd -t $file && die "should have detected an error"
done
exit 0

