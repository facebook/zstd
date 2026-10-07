#!/bin/sh
set -e

# issue #4081: a cyclic symlink under -r -f used to hang; verify it is detected and skipped

mkdir dir
echo "payload" > dir/file
ln -s . dir/self        # dir/self resolves to dir: a cycle

zstd -r -f dir 2> cycle-stderr
grep "is a directory cycle" cycle-stderr > /dev/null
test -f dir/file.zst    # the real file is still compressed

# a non-cyclic directory symlink must still be followed under -f (no false positive)
mkdir -p tree/real tree/top
echo "data" > tree/real/inside
ln -s ../real tree/top/link

zstd -r -f tree/top 2> acyclic-stderr
test -f tree/real/inside.zst                 # the symlinked directory was followed
if grep -q "directory cycle" acyclic-stderr; then
    echo "acyclic symlink wrongly flagged as a cycle" >&2
    exit 1
fi
