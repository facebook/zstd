#!/bin/sh

set -e

datagen | zstd -q > file.zst

zstd -dq --trace-file-stat file.zst
