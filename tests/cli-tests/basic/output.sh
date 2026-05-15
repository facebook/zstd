#!/bin/sh

echo "some data" > file

println "+ zstd -q file --output compressed.zst"
zstd -q file --output compressed.zst
test -f compressed.zst || die "Expected --output to create compressed.zst"

println "+ zstd -q -d compressed.zst --output roundtrip"
zstd -q -d compressed.zst --output roundtrip
cmp file roundtrip || die "Expected --output decompression to round-trip"

exit 0
