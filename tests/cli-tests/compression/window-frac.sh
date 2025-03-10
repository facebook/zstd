#!/bin/sh

set -e

zstd --zstd=wlog=21,wfrac=5 < file > file.zst
zstd -vv -l file.zst
