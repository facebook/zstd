#!/bin/sh

set -e

. "$COMMON/platform.sh"

echo "some data" > frame1
echo "more data" > frame2
zstd -q frame1 frame2
cat frame1 frame2 > both

println "+ input that never contained a frame keeps its errors"

printf 'garbage' > notzstd
zstd -dc notzstd > /dev/null && die "should fail"
printf 'ab' > short
zstd -dc short > /dev/null && die "should fail"

println "+ trailing data after the last frame is ignored with a warning"

# 7 bytes: enough to check a magic number, which no known format matches
cp frame1.zst junk7.zst
printf 'garbage' >> junk7.zst
zstd -dc junk7.zst > junk7.out
$DIFF frame1 junk7.out
zstd -dc < junk7.zst > junk7-stdin.out
$DIFF frame1 junk7-stdin.out
zstd -t junk7.zst
zstd -d -f junk7.zst
$DIFF frame1 junk7

# 2 bytes: too short to check a magic number
cp frame1.zst junk2.zst
printf 'ab' >> junk2.zst
zstd -dc junk2.zst > junk2.out
$DIFF frame1 junk2.out

# several frames, then trailing data
cat frame1.zst frame2.zst > two-frames.zst
printf 'garbage' >> two-frames.zst
zstd -dc two-frames.zst > two-frames.out
$DIFF both two-frames.out

println "+ -qq suppresses the warning"

zstd -qq -dc junk7.zst > junk7-quiet.out
$DIFF frame1 junk7-quiet.out

println "+ pass-through still copies the trailing data"

zstd -dcf junk7.zst > junk7-pass.out
cat frame1 > expected-pass
printf 'garbage' >> expected-pass
$DIFF expected-pass junk7-pass.out

println "+ --rm keeps the source when trailing data was ignored"

zstd -d -f --rm junk7.zst
test -f junk7.zst
$DIFF frame1 junk7
# a clean archive is still removed
cp frame1.zst clean.zst
zstd -d -f --rm clean.zst
test ! -f clean.zst
# writing to stdout never removes the source
zstd -dc --rm frame2.zst > /dev/null
test -f frame2.zst
