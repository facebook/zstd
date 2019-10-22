#!/bin/bash

println() {
    printf '%b\n' "${*}"
}

UNAME=$(uname)

isWindows=false
INTOVOID="/dev/null"
case "$UNAME" in
  GNU) DEVDEVICE="/dev/random" ;;
  *) DEVDEVICE="/dev/zero" ;;
esac
case "$OS" in
  Windows*)
    isWindows=true
    INTOVOID="NUL"
    DEVDEVICE="NUL"
    ;;
esac

case "$UNAME" in
  Darwin) MD5SUM="md5 -r" ;;
  FreeBSD) MD5SUM="gmd5sum" ;;
  OpenBSD) MD5SUM="md5" ;;
  *) MD5SUM="md5sum" ;;
esac

DIFF="diff"
case "$UNAME" in
  SunOS) DIFF="gdiff" ;;
esac

println "\nStarting playTests_bash.sh isWindows=$isWindows ZSTD='$ZSTD'"

[ -n "$ZSTD" ] || die "ZSTD variable must be defined!"

println "\n===>  simple tests "

println "\n===>  zstd fifo named pipe test "
head -c 1M /dev/zero > tmp_original
$ZSTD <(head -c 1M /dev/zero) -o tmp_compressed
$ZSTD -d -o tmp_decompressed tmp_compressed
$DIFF -s tmp_original tmp_decompressed
rm -rf tmp*
