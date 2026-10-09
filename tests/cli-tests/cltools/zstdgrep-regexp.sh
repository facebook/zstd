#!/bin/sh

set -e

printf 'start\nstop\n' > plain
zstd -qf plain -o plain.zst

println "+ multi -e on .zst"
zstdgrep -e start -e stop plain.zst

println "+ --regexp= repeated"
zstdgrep --regexp=start --regexp=stop plain.zst

println "+ --regexp separate args"
zstdgrep --regexp start --regexp stop plain.zst

println "+ mixed --regexp / -e"
zstdgrep --regexp start --regexp=stop -e start plain.zst

println "+ uncompressed path still works"
zstdgrep -e start -e stop plain
