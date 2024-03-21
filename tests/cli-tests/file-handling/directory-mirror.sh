#!/bin/sh
set -e

# setup
mkdir -p src/.hidden src/dir
mkdir mid dst

echo "file1" > src/file1
echo "file2" > src/.file2
echo "file3" > src/.hidden/.file3
echo "file4" > src/dir/.file4

# relative paths
zstd -q -r --output-dir-mirror mid/ src/
zstd -q -d -r --output-dir-mirror dst/ mid/src/

diff --brief --recursive --new-file src/ dst/mid/src/

# reset
rm -rf mid dst
mkdir mid dst

# from inside the directory
(cd src; zstd -q -r --output-dir-mirror ../mid/ ./)
(cd mid; zstd -q -d -r --output-dir-mirror ../dst/ ./)

diff --brief --recursive --new-file src/ dst/

# reset
rm -rf mid dst
mkdir mid dst

# absolute paths
export BASE_PATH="$(pwd)"

zstd -q -r --output-dir-mirror mid/ "${BASE_PATH}/src/"
zstd -q -d -r --output-dir-mirror  dst/ "${BASE_PATH}/mid/${BASE_PATH}/src/"

diff --brief --recursive --new-file src/ "dst/${BASE_PATH}/mid/${BASE_PATH}/src/"

# reset
rm -rf mid dst
mkdir mid dst

# dots
zstd -q -r --output-dir-mirror mid/ ./src/./
zstd -q -d -r --output-dir-mirror  dst/ ./mid/./src/./

diff --brief --recursive --new-file src/ dst/mid/src/
