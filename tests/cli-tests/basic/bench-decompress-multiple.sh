#!/bin/sh

set -eu

# A 512-byte skippable frame followed by a raw frame producing four bytes.
# Its compressed size (533) is much larger than its decoded size (4).
{
    printf '\120\052\115\030\000\002\000\000'
    dd if=/dev/zero bs=512 count=1 2>/dev/null
    printf '\050\265\057\375\040\004\041\000\000AAAA'
} > padded.zst

# A raw frame producing 32 bytes, and an RLE frame expanding to 64 bytes.
printf '\050\265\057\375\040\040\001\001\000BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB' > raw.zst
printf '\050\265\057\375\040\100\003\002\000R' > expanding.zst
printf '\120\052\115\030\000\000\000\000' > skippable.zst
printf '\050\265\057\375\040\000\001\000\000' > empty.zst
cat skippable.zst skippable.zst > skippables.zst

# This single-file input places a frame boundary at byte 64. Decode-only
# benchmarking must not split it into independently decoded 64-byte chunks.
{
    printf '\120\052\115\030\053\000\000\000'
    dd if=/dev/zero bs=43 count=1 2>/dev/null
    printf '\050\265\057\375\040\004\041\000\000AAAA'
    cat raw.zst
} > single.zst

zstd -qq -t padded.zst raw.zst expanding.zst skippable.zst empty.zst skippables.zst single.zst

# Advancing the second result pointer by compressed size caused a heap
# overwrite here. ASan detects it even when the allocator does not crash.
zstd -qq -d -b -i0 padded.zst raw.zst
zstd -qq -d -b -i0 expanding.zst raw.zst
zstd -qq -d -b -i0 raw.zst expanding.zst
zstd -qq -d -b -B64 -i0 single.zst

# The benchmark's streaming wrapper currently rejects zero output capacity.
# Accept that clean error (or success), but never a sanitizer failure.
check_zero_output() {
    if zstd -q -d -b -i0 "$@" > /dev/null 2> error.stderr; then
        return
    fi
    grep -Fq 'Error 30 : decompression error' error.stderr
}
check_zero_output skippables.zst expanding.zst empty.zst
check_zero_output empty.zst skippable.zst

# The individual frame sizes are known, but their sum overflows U64.
printf '\050\265\057\375\340\000\377\377\377\377\377\377\377\001\000\000' > overflow1.zst
printf '\050\265\057\375\340\020\001\000\000\000\000\000\000\001\000\000' > overflow2.zst
if zstd -q -d -b -i0 overflow1.zst overflow2.zst > /dev/null 2> overflow.stderr; then
    die 'benchmark accepted an overflowing decompressed-size sum'
fi
grep -Fq 'decompressed size is too large for local system' overflow.stderr

# Keep a normal compression/decompression benchmark as a control.
printf 'ordinary benchmark input\n' > plain
zstd -qq -b -i0 plain
zstd -qq -b -B32 -i0 padded.zst raw.zst
