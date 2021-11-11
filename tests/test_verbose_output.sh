#!/usr/bin/env bash
set -e -u -o pipefail

make -j -C .. CFLAGS=-O1 zstd
empty_file=$(mktemp)
trap 'rm -f $empty_file' EXIT
../zstd -vv "$empty_file" 2>&1 | \
grep -q -E -- "--zstd=wlog=[[:digit:]]+,clog=[[:digit:]]+,hlog=[[:digit:]]+,\
slog=[[:digit:]]+,mml=[[:digit:]]+,tlen=[[:digit:]]+,strat=[[:digit:]]+"
