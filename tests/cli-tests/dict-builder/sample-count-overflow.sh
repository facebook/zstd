#!/bin/sh

set -eu

# The count must fit both an int and a size_t array allocation. Query the
# tested binary, since the test runner may be executing a 32-bit build.
case "$(zstd -V)" in
    *32-bit*) max_samples=1073741823 ;;
    *)        max_samples=2147483647 ;;
esac

printf '%s\n' 'Error 14 : too many samples for dictionary training' > expected.stderr

make_sparse_file() {
    dd if=/dev/zero of="$1" bs=1 count=0 seek="$2" 2>/dev/null
}

check_too_many_samples() {
    status=0
    zstd -q --train --memory=1MB "$@" -o dictionary > actual.stdout 2> actual.stderr || status=$?
    test "$status" -eq 14 || die "expected sample-count rejection, got exit $status: $*"
    test ! -s actual.stdout || die 'unexpected dictionary-training output'
    cmp expected.stderr actual.stderr
    test ! -e dictionary || die 'oversized sample set produced a dictionary'
}

# The first count above the limit. On 32-bit builds this is the original
# 1 GiB / -B1 reproducer, whose sample-table allocation wrapped to zero.
make_sparse_file sample "$((max_samples + 1))"
for trainer in --train --train-cover --train-fastcover --train-legacy; do
    check_too_many_samples "$trainer" -B1 sample
done

# A nonzero wrapped allocation is unsafe too.
make_sparse_file sample "$((max_samples + 2))"
check_too_many_samples --split=1 sample

# Include the last partial chunk when checking the limit.
make_sparse_file sample "$((max_samples * 2 + 1))"
check_too_many_samples --split=2 sample

# Individually valid counts must also be checked when they are added.
make_sparse_file sample "$max_samples"
make_sparse_file extra 1
check_too_many_samples --split=1 sample extra
