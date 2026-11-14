#!/bin/sh

set -e

# With several files, zstdgrep should exit like grep(1):
# 0 if a line is selected in any file, 1 if no line is selected at all,
# and more than 1 if grep fails.

check_status() {
    expected="$1"
    shift
    rc=0
    zstdgrep "$@" 2>/dev/null || rc=$?
    [ "$rc" -eq "$expected" ] || die "zstdgrep $*: expected exit status $expected, got $rc"
}

echo "5678" > other
zstd -q other

println "+ match in the first file"
check_status 0 "1234" file.zst other.zst
println "+ match in the last file"
check_status 0 "1234" other.zst file.zst
println "+ quiet, match in one file"
check_status 0 -q "1234" other.zst file.zst
println "+ no match in any file"
check_status 1 "abcd" file.zst other.zst
println "+ grep error"
check_status 2 "a[" file.zst other.zst
