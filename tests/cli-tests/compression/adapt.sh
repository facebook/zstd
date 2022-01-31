#!/bin/sh

set -e

# Test --adapt
zstd -f file --adapt -c | zstd -t
