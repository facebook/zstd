#!/bin/sh

set -e

die() {
    println "$@" 1>&2
    exit 1
}

zstd() {
    if [ -z "$EXEC_PREFIX" ]; then
        "$ZSTD_BIN" "$@"
    else
        "$EXEC_PREFIX" "$ZSTD_BIN" "$@"
    fi
}

println() {
    printf '%b\n' "${*}"
}

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PRGDIR="$SCRIPT_DIR/../programs"
UNAME=$(uname)
ZSTDGREP="$PRGDIR/zstdgrep"
ZSTDLESS="$PRGDIR/zstdless"

INTOVOID="/dev/null"
case "$UNAME" in
  GNU) DEVDEVICE="/dev/random" ;;
  *) DEVDEVICE="/dev/zero" ;;
esac
case "$OS" in
  Windows*)
    INTOVOID="NUL"
    DEVDEVICE="NUL"
    ;;
esac

DIFF="diff"
case "$UNAME" in
  SunOS) DIFF="gdiff" ;;
esac

# check if ZSTD_BIN is defined. if not, use the default value
if [ -z "${ZSTD_BIN}" ]; then
  println "\nZSTD_BIN is not set. Using the default value..."
  ZSTD_BIN="$PRGDIR/zstd"
fi

# assertions
[ -n "$ZSTD_BIN" ] || die "zstd not found at $ZSTD_BIN! \n Please define ZSTD_BIN pointing to the zstd binary. You might also consider rebuilding zstd following the instructions in README.md"
println "\nStarting playToolTests.sh EXE_PREFIX='$EXE_PREFIX' ZSTD_BIN='$ZSTD_BIN' DATAGEN_BIN='$DATAGEN_BIN'"



# tool tests

println "\n===> zstdgrep tests"
ln -sf "$ZSTD_BIN" zstdcat
rm -f tmp_grep
echo "1234" > tmp_grep
zstd -f tmp_grep
lines=$(ZCAT=./zstdcat "$ZSTDGREP" 2>&1 "1234" tmp_grep tmp_grep.zst | wc -l)
test 2 -eq $lines
ZCAT=./zstdcat "$ZSTDGREP" 2>&1 "1234" tmp_grep_bad.zst && die "Should have failed"
ZCAT=./zstdcat "$ZSTDGREP" 2>&1 "1234" tmp_grep_bad.zst | grep "No such file or directory" || true
rm -f tmp_grep*

println "\n===> zstdless tests"
if [ -n "$(which less)" ]; then
  ln -sf "$ZSTD_BIN" zstd
  rm -f tmp_less*
  echo "1234" > tmp_less
  echo "foo" >> tmp_less
  echo "bar" >> tmp_less
  less -N -f tmp_less > tmp_less_counted
  zstd -f tmp_less
  lines=$(ZSTD=./zstd "$ZSTDLESS" 2>&1 tmp_less.zst | wc -l)
  test 3 -eq $lines
  ZSTD=./zstd "$ZSTDLESS" -f tmp_less.zst > tmp_less_regenerated
  $DIFF tmp_less tmp_less_regenerated
  ZSTD=./zstd "$ZSTDLESS" -N -f tmp_less.zst > tmp_less_regenerated_counted
  $DIFF tmp_less_counted tmp_less_regenerated_counted
  ZSTD=./zstd "$ZSTDLESS" 2>&1 tmp_less_bad.zst | grep "No such file or directory" || die
  rm -f tmp_less*
fi
