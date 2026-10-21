#!/bin/sh

set -e

mkdir d1 d2 out
printf 'first\n' > d1/x
printf 'second\n' > d2/x

zstd -f --rm --output-dir-flat out d1/x d2/x && die "Collision with --rm should fail!"
test -f d1/x
test -f d2/x
test ! -e out/x.zst

zstd -f --output-dir-flat out d1/x d2/x
test -f d1/x
test -f d2/x
test -f out/x.zst

mkdir dz1 dz2 dout
printf 'first\n' | zstd -q > dz1/x.zst
printf 'second\n' | zstd -q > dz2/x.zst

zstd -d -f --rm --output-dir-flat dout dz1/x.zst dz2/x.zst && die "Decompression collision with --rm should fail!"
test -f dz1/x.zst
test -f dz2/x.zst
test ! -e dout/x

zstd -d -f --output-dir-flat dout dz1/x.zst dz2/x.zst
test -f dz1/x.zst
test -f dz2/x.zst
test -f dout/x

if [ "${OS:-}" = "Windows_NT" ]; then
    printf 'first case\n' > d1/A
    printf 'second case\n' > d2/a
    zstd -f --rm --output-dir-flat out d1/A d2/a && die "Case collision with --rm should fail!"
    test -f d1/A
    test -f d2/a
fi
exit 0
