#!/usr/bin/env sh

set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
INCLUDE_DIR="$SCRIPT_DIR/../linux/include"
LIB_DIR="$SCRIPT_DIR/../linux/lib"


print() {
    printf '%b' "${*}"
}

println() {
    printf '%b\n' "${*}"
}

die() {
    println "$@" 1>&2
    exit 1
}

# Search preprocessor directives only. freestanding.py deliberately keeps
# comments, and a comment explaining a macro has to be able to name it; since a
# comment is never a directive, they are ignored here without having to be
# stripped. Line continuations are joined first, so a macro named on the second
# line of a multi-line #if is still seen.
#
# `#if MACRO` needs no help from this test: the kernel build compiles with
# -Wundef, so an undefined macro used there is already an error, continuation
# lines included. What is left is the forms -Wundef cannot see, and they all sit
# on a directive line: #ifdef, #ifndef, #if defined(), and a surviving #define.

# built once; read by redirect rather than a pipe, so the loop below runs in
# this shell and can accumulate into $matches
FILELIST=$(mktemp)
trap 'rm -f "$FILELIST"' EXIT
find "$INCLUDE_DIR" "$LIB_DIR" -type f > "$FILELIST"

test_not_present() {
    print "Testing that '$1' is not present... "
    matches=""
    while read -r f; do
        found=$(sed -e :a -e '/\\$/N; s/\\\n//; ta' "$f" \
                | grep -nE "^[[:space:]]*#.*[^A-Za-z0-9_]?$1([^A-Za-z0-9_]|$)" \
                | sed "s|^|$f:|") || true
        if [ -n "$found" ]; then
            matches="$matches$found
"
        fi
    done < "$FILELIST"
    if [ -n "$matches" ]; then
        println ""
        printf '%s' "$matches" | sed "s|$SCRIPT_DIR/../|contrib/linux-kernel/|"
        die "Fail!"
    fi
    println "Okay"
}

println "This test checks that the macro removal process worked as expected"
println "If this test fails, then freestanding.py wasn't able to remove one of these"
println "macros from the source code completely. You'll either need to rewrite the check"
println "or improve freestanding.py."
println ""

test_not_present "ZSTD_NO_INTRINSICS"
test_not_present "ZSTD_NO_UNUSED_FUNCTIONS"
test_not_present "ZSTD_LEGACY_SUPPORT"
test_not_present "STATIC_BMI2"
test_not_present "ZSTD_DLL_EXPORT"
test_not_present "ZSTD_DLL_IMPORT"
test_not_present "__ICCARM__"
test_not_present "_MSC_VER"
test_not_present "_WIN32"
test_not_present "__linux__"
