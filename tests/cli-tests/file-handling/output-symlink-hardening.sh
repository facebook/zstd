#!/bin/sh
set -e

. "$COMMON/platform.sh"

echo "sensitive-data" > sensitive
echo "payload-data" > payload

# Skip if symlink creation is unavailable in this environment.
if ! ln -s sensitive out.zst 2>"$INTOVOID"; then
    exit 0
fi
if zstd -q -f payload -o out.zst 2>"$INTOVOID"; then
    die "compression to symlink output must fail"
fi

test "$(cat sensitive)" = "sensitive-data" || die "symlink target was modified"
test -L out.zst || die "output path should remain a symlink"

zstd -q -f payload -o payload.zst
ln -s sensitive decoded
if zstd -q -d -f payload.zst -o decoded 2>"$INTOVOID"; then
    die "decompression to symlink output must fail"
fi

test "$(cat sensitive)" = "sensitive-data" || die "symlink target was modified"
test -L decoded || die "decoded output path should remain a symlink"
