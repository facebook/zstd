#!/bin/sh

set -e

# The interposer requires a native 64-bit Linux binary.
[ -z "${EXEC_PREFIX:-}" ] || exit 0
[ "$(uname -s)" = Linux ] || exit 0
[ "$(getconf LONG_BIT)" = 64 ] || exit 0
zstd --version | grep -Fq '(64-bit)' || exit 0

${CC:-cc} -Wall -Wextra -Werror -shared -fPIC \
    "$COMMON/source-identity.c" -ldl -o source-identity.so

printf 'protected sentinel\n' > protected
chmod 600 protected

checkReplacement() {
    name=$1
    printf 'ordinary input\n' > "$name"
    chmod 644 "$name"

    status=0
    LD_PRELOAD="$PWD/source-identity.so" \
        ZSTD_TEST_RACE_TARGET="$name" \
        ZSTD_TEST_RACE_REPLACEMENT="$name.replacement" \
        zstd -q "$name" -o "$name.zst" 2> "$name.stderr" || status=$?

    grep -Fxq "source-identity: changed $name" "$name.stderr"
    if [ "$status" -eq 0 ]; then
        zstd -dq "$name.zst" -o "$name.decoded"
        cmp protected "$name.decoded"
        die "source replacement was accepted and protected data was compressed"
    fi
    [ "$status" -eq 1 ] || die "unexpected source-race exit status: $status"
    grep -Fxq "zstd: $name changed while opening -- ignored" "$name.stderr"
    [ ! -e "$name.zst" ] || die "rejected source created an output file"
}

# Exercise both a replacement symlink and a different regular-file inode.
ln -s "$PWD/protected" symlink-input.replacement
checkReplacement symlink-input
cp protected regular-input.replacement
checkReplacement regular-input

# Changing mode does not change identity, but the output must use the new mode.
printf 'same inode\n' > metadata-input
chmod 644 metadata-input
LD_PRELOAD="$PWD/source-identity.so" \
    ZSTD_TEST_RACE_TARGET=metadata-input \
    zstd -q metadata-input -o metadata-input.zst 2> metadata-input.stderr
grep -Fxq 'source-identity: changed metadata-input' metadata-input.stderr
[ "$(stat -c %a metadata-input)" = 600 ]
[ "$(stat -c %a metadata-input.zst)" = 600 ]
zstd -dcq metadata-input.zst | cmp metadata-input -
