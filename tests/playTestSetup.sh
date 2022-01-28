#!/bin/sh

set -e

die() {
    println "$@" 1>&2
    exit 1
}

datagen() {
    "$DATAGEN_BIN" "$@"
}

zstd() {
    if [ -z "$EXEC_PREFIX" ]; then
        "$ZSTD_BIN" "$@"
    else
        "$EXEC_PREFIX" "$ZSTD_BIN" "$@"
    fi
}

sudoZstd() {
    if [ -z "$EXEC_PREFIX" ]; then
        sudo "$ZSTD_BIN" "$@"
    else
        sudo "$EXEC_PREFIX" "$ZSTD_BIN" "$@"
    fi
}

roundTripTest() {
    if [ -n "$3" ]; then
        cLevel="$3"
        proba="$2"
    else
        cLevel="$2"
        proba=""
    fi
    if [ -n "$4" ]; then
        dLevel="$4"
    else
        dLevel="$cLevel"
    fi

    rm -f tmp1 tmp2
    println "roundTripTest: datagen $1 $proba | zstd -v$cLevel | zstd -d$dLevel"
    datagen $1 $proba | $MD5SUM > tmp1
    datagen $1 $proba | zstd --ultra -v$cLevel | zstd -d$dLevel  | $MD5SUM > tmp2
    $DIFF -q tmp1 tmp2
}

fileRoundTripTest() {
    if [ -n "$3" ]; then
        local_c="$3"
        local_p="$2"
    else
        local_c="$2"
        local_p=""
    fi
    if [ -n "$4" ]; then
        local_d="$4"
    else
        local_d="$local_c"
    fi

    rm -f tmp.zst tmp.md5.1 tmp.md5.2
    println "fileRoundTripTest: datagen $1 $local_p > tmp && zstd -v$local_c -c tmp | zstd -d$local_d"
    datagen $1 $local_p > tmp
    < tmp $MD5SUM > tmp.md5.1
    zstd --ultra -v$local_c -c tmp | zstd -d$local_d | $MD5SUM > tmp.md5.2
    $DIFF -q tmp.md5.1 tmp.md5.2
}

truncateLastByte() {
    dd bs=1 count=$(($(wc -c < "$1") - 1)) if="$1"
}

println() {
    printf '%b\n' "${*}"
}

if [ -z "${size}" ]; then
    size=
else
    size=${size}
fi

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PRGDIR="$SCRIPT_DIR/../programs"
TESTDIR="$SCRIPT_DIR/../tests"
UNAME=$(uname)

detectedTerminal=false
if [ -t 0 ] && [ -t 1 ]
then
    detectedTerminal=true
fi
isTerminal=${isTerminal:-$detectedTerminal}

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
  NetBSD) MD5SUM="md5 -n" ;;
  OpenBSD) MD5SUM="md5" ;;
  *) MD5SUM="md5sum" ;;
esac

MTIME="stat -c %Y"
case "$UNAME" in
    Darwin | FreeBSD | OpenBSD | NetBSD) MTIME="stat -f %m" ;;
esac

assertSameMTime() {
    MT1=$($MTIME "$1")
    MT2=$($MTIME "$2")
    echo MTIME $MT1 $MT2
    [ "$MT1" = "$MT2" ] || die "mtime on $1 doesn't match mtime on $2 ($MT1 != $MT2)"
}

GET_PERMS="stat -c %a"
case "$UNAME" in
    Darwin | FreeBSD | OpenBSD | NetBSD) GET_PERMS="stat -f %Lp" ;;
esac

assertFilePermissions() {
    STAT1=$($GET_PERMS "$1")
    STAT2=$2
    [ "$STAT1" = "$STAT2" ] || die "permissions on $1 don't match expected ($STAT1 != $STAT2)"
}

assertSamePermissions() {
    STAT1=$($GET_PERMS "$1")
    STAT2=$($GET_PERMS "$2")
    [ "$STAT1" = "$STAT2" ] || die "permissions on $1 don't match those on $2 ($STAT1 != $STAT2)"
}

DIFF="diff"
case "$UNAME" in
  SunOS) DIFF="gdiff" ;;
esac


# check if ZSTD_BIN is defined. if not, use the default value
if [ -z "${ZSTD_BIN}" ]; then
  println "\nZSTD_BIN is not set. Using the default value..."
  ZSTD_BIN="$PRGDIR/zstd"
fi

# check if DATAGEN_BIN is defined. if not, use the default value
if [ -z "${DATAGEN_BIN}" ]; then
  println "\nDATAGEN_BIN is not set. Using the default value..."
  DATAGEN_BIN="$TESTDIR/datagen"
fi

# Why was this line here ? Generates a strange ZSTD_BIN when EXE_PREFIX is non empty
# ZSTD_BIN="$EXE_PREFIX$ZSTD_BIN"

if echo hello | zstd -v -T2 2>&1 > $INTOVOID | grep -q 'multi-threading is disabled'
then
    hasMT=""
else
    hasMT="true"
fi
