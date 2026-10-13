#!/bin/sh

set -e

. "$COMMON/platform.sh"

echo "some data" > frame1
echo "more data" > frame2
cat frame1 frame2 > both
zstd -q frame1 frame2
cat frame1.zst frame2.zst > both.zst
SIZE=$(wc -c < both.zst)

println "+ a frame that is cut short reports how much was decoded"

# the last frame loses most of its checksum
head -c $((SIZE - 3)) both.zst > cut.zst
zstd -dc cut.zst > cut.out && die "should fail"
zstd -t cut.zst && die "should fail"
$DIFF both cut.out

println "+ a corrupted frame reports how much was decoded"

# the last byte of the content of the last frame is replaced
{ head -c $((SIZE - 5)) both.zst; printf 'X'; tail -c 4 both.zst; } > corrupt.zst
zstd -dc corrupt.zst > corrupt.out && die "should fail"
zstd -t corrupt.zst && die "should fail"
$DIFF frame1 corrupt.out
