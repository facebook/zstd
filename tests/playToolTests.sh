#!/bin/sh

# shellcheck source=./playTestSetup.sh 
# shellcheck disable=SC2154
. "$(dirname "$0")"/playTestSetup.sh

ZSTDGREP="$PRGDIR/zstdgrep"
ZSTDLESS="$PRGDIR/zstdless"

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
