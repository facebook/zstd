#!/bin/sh

set -e

println "+ good path"
zstdless file.zst
println "+ bad path"
zstdless bad.zst
