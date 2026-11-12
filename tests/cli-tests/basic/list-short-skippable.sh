#!/bin/sh

set -eu

# A complete frame whose dictionary-ID bytes leave known values in the list
# parser's header buffer. Listing does not need the referenced dictionary.
# Layout: magic, single-segment descriptor, dictID 11 22 de ad, FCS 4,
# and a final raw block containing four bytes.
printf '\050\265\057\375\043\021\042\336\255\004\041\000\000AAAA' > prefix.zst

check_incomplete_header() {
    if zstd -l "$1" > list.stdout 2> list.stderr; then
        die "list accepted an incomplete skippable header: $1"
    fi
    grep -Fq 'incomplete' list.stderr
    if grep -Fq 'seeked to position' list.stderr; then
        die "list used a length from an incomplete header: $1"
    fi
    if grep -Eq 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:' list.stderr; then
        die "sanitizer failure while listing $1"
    fi
}

# An empty file is incomplete, but no next-frame bytes after a complete frame
# is a valid boundary.
printf '' > short.zst
check_incomplete_header short.zst
zstd -l prefix.zst > /dev/null

# Exercise the lowest and highest skippable magics and every nonempty
# truncated header, both at the start of a file and after a frame that primes
# the header buffer. The loop values are octal escapes for the low magic byte,
# 0x50 and 0x5F.
for magic in 120 137; do
    printf "\\$magic\052\115\030\000\000\000\000" > header.zst
    for length in 1 2 3 4 5 6 7; do
        dd if=header.zst of=short.zst bs=1 count="$length" 2>/dev/null
        check_incomplete_header short.zst
        cat prefix.zst short.zst > after.zst
        check_incomplete_header after.zst
    done

    # A complete eight-byte, zero-payload skippable frame is still valid.
    zstd -l header.zst > /dev/null
    cat prefix.zst header.zst > after.zst
    zstd -l after.zst > /dev/null
done

# Complete nonempty payloads still skip to the next frame. The short case
# requires seeking backward after read-ahead; the long case seeks forward.
printf '\120\052\115\030\004\000\000\000data' > short-payload.zst
printf '\137\052\115\030\040\000\000\0000123456789abcdef0123456789abcdef' > long-payload.zst
for payload in short-payload.zst long-payload.zst; do
    zstd -l "$payload" > /dev/null
    cat "$payload" prefix.zst > after.zst
    zstd -l after.zst > /dev/null
done

# Preserve the separate signed-64-bit seek fix: this complete header declares
# a payload extending to position 2^32, and must fail without seeking backward.
printf '\120\052\115\030\370\377\377\377' > large.zst
if zstd -l large.zst > list.stdout 2> list.stderr; then
    die 'list accepted an incomplete large skippable frame'
fi
grep -Fq 'seeked to position 4294967296' list.stderr
