#!/bin/sh
# Checks how STATIC_BMI2, DYNAMIC_BMI2 and -mbmi2 combine, which no functional
# test can see: every wrong answer below still produces correct output, and
# still runs fine on a BMI2-capable machine.
#
# Part 1, the resulting state of each configuration. Each lands in exactly one
# of three: bmi2 everywhere, runtime dispatch, or no zstd bmi2 -- or refuses to
# build, when the build asked for something impossible or self-contradictory.
#
# Part 2, the object code, for two properties a state check cannot see:
#   - -mbmi2 leaves no runtime CPUID probe. With BMI2 compiled in there is no
#     second variant to dispatch to, so the probe is dead weight. CPUID is a
#     serializing instruction and traps to the hypervisor on every virtualized
#     guest. One such probe survived in ZSTD_initCCtx() for years.
#   - asking for a dispatcher on top of -mbmi2 must build exactly what -mbmi2
#     alone builds, byte for byte.
#
# Not covered: that BMI2_TARGET_ATTRIBUTE is still applied to the dispatched
# variants. Several are inlined away and carry no symbol, so it cannot be checked
# reliably.

set -e
set -u

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
LIB_DIR="$SCRIPT_DIR/../lib"
CC=${CC:-cc}
OBJDUMP=${OBJDUMP:-objdump}

println() { printf '%b\n' "${*}"; }
die() { println "$@" 1>&2; exit 1; }

# What matters is what the compiler targets, not what the host runs: `uname -m`
# would try -mbmi2 under an ARM cross-compiler on an x86 box, and skip an x86
# cross-compiler on an ARM one.
target_macros=$($CC -dM -E - < /dev/null 2>/dev/null) \
    || { println "skipped: $CC does not support -dM -E, cannot determine its target"; exit 0; }
case "$target_macros" in
    *__x86_64__*|*__i386__*) ;;
    *) println "skipped: $CC does not target x86, where CPUID and BMI2 apply"; exit 0 ;;
esac

command -v "$OBJDUMP" > /dev/null 2>&1 || { println "skipped: $OBJDUMP not available"; exit 0; }

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

cat > "$WORK/probe.c" <<'EOF'
#include "portability_macros.h"
#if ZSTD_BMI2_DISPATCH
#pragma message "ZSTD_STATE=dispatch"
#elif ZSTD_BMI2_AVAILABLE
#pragma message "ZSTD_STATE=everywhere"
#else
#pragma message "ZSTD_STATE=none"
#endif
typedef int zstd_bmi2_probe_t;
EOF

# Which of the four outcomes a set of build flags produces. The real translation
# unit is compiled first, because "STATIC_BMI2=1 without bmi2 codegen" is caught
# by the compiler refusing an intrinsic, not by an #error the probe would see.
outcome() {
    if ! $CC -O2 "$@" -I"$LIB_DIR" -I"$LIB_DIR/common" -I"$LIB_DIR/compress" \
            -I"$LIB_DIR/decompress" -c "$LIB_DIR/compress/zstd_compress_sequences.c" \
            -o "$WORK/o.o" 2> /dev/null; then
        echo "build fails"; return
    fi
    case "$($CC -fsyntax-only "$@" -I"$LIB_DIR" -I"$LIB_DIR/common" "$WORK/probe.c" 2>&1)" in
        *ZSTD_STATE=dispatch*)   echo "runtime dispatch" ;;
        *ZSTD_STATE=everywhere*) echo "bmi2 everywhere" ;;
        *)                       echo "no zstd bmi2" ;;
    esac
}

expect() {
    want=$1; shift
    got=$(outcome "$@")
    [ "$got" = "$want" ] \
        || die "FAIL: ${*:-<no flags>} produced '$got', expected '$want'"
    println "ok: ${*:-<no flags>} -> $got"
}

# left alone, or told only about the dispatcher
expect "runtime dispatch"                                     # the default, on x86
expect "bmi2 everywhere"  -mbmi2
expect "no zstd bmi2"     -DDYNAMIC_BMI2=0
expect "runtime dispatch" -DDYNAMIC_BMI2=1
# a dispatcher cannot exist under -mbmi2; the request is ignored, not refused,
# because nothing the *build* said is contradictory
expect "bmi2 everywhere"  -DDYNAMIC_BMI2=1 -mbmi2
# an explicit STATIC_BMI2 settles it, and is honoured even against -mbmi2
expect "no zstd bmi2"     -DSTATIC_BMI2=0
expect "no zstd bmi2"     -DSTATIC_BMI2=0 -mbmi2
expect "bmi2 everywhere"  -DSTATIC_BMI2=1 -mbmi2
# promises that cannot be kept, and requests that contradict each other
expect "build fails"      -DSTATIC_BMI2=1
expect "build fails"      -DSTATIC_BMI2=0 -DDYNAMIC_BMI2=1
expect "build fails"      -DSTATIC_BMI2=1 -DDYNAMIC_BMI2=1 -mbmi2
# the same broken promise, with nothing left to trip over it by accident. The
# row above used to pass only because the compiler rejected _bzhi_u64; with no
# intrinsics to reject, the build once linked, and its assembly ran BMI2 with
# no CPU check in front of it.
expect "build fails"      -DSTATIC_BMI2=1 -DZSTD_NO_INTRINSICS

# check-bmi2-state.c makes the same three-way judgement as the probe above, for
# the Windows toolchains, which this script cannot drive. Only their CI job
# compiles it, so exercise it here too: otherwise it silently stops tracking
# portability_macros.h, and the first sign is a red Windows job on an unrelated
# change.
for pair in "EVERYWHERE:-mbmi2" "NONE:-DSTATIC_BMI2=0" "DISPATCH:"; do
    want=${pair%%:*}
    flag=${pair#*:}
    # unquoted on purpose: empty means "no flag", and none of these contain spaces
    if $CC -fsyntax-only -I"$LIB_DIR/common" $flag "-DZSTD_EXPECT_$want" \
            "$SCRIPT_DIR/check-bmi2-state.c" 2>/dev/null; then
        println "ok: check-bmi2-state.c agrees on $want"
    else
        die "FAIL: check-bmi2-state.c disagrees with the header on $want" \
            "(flags: ${flag:-<none>}). It is asserted by the bmi2-state-windows" \
            "CI job; fix it here rather than discovering it on Windows."
    fi
done

# The architecture gate cannot be reached from an x86 runner, so hide the arch
# macros from the preprocessor. That exercises the same #if logic a real ARM
# build would, without needing a cross-compiler. Header only: the library
# sources would fail to compile for unrelated reasons under a faked target.
NOT_X86="-U__x86_64__ -U__i386__ -U_M_IX86 -U_M_X64"

header_state() {
    out=$($CC -fsyntax-only "$@" -I"$LIB_DIR" -I"$LIB_DIR/common" "$WORK/probe.c" 2>&1) \
        || { echo "build fails"; return; }
    case "$out" in
        *ZSTD_STATE=dispatch*)   echo "runtime dispatch" ;;
        *ZSTD_STATE=everywhere*) echo "bmi2 everywhere" ;;
        *)                       echo "no zstd bmi2" ;;
    esac
}

expect_off_x86() {
    want=$1; shift
    got=$(header_state $NOT_X86 "$@")
    [ "$got" = "$want" ] \
        || die "FAIL: off x86, ${*:-<no flags>} produced '$got', expected '$want'"
    println "ok: off x86, ${*:-<no flags>} -> $got"
}

# nothing to dispatch on, so the request is ignored rather than obeyed
expect_off_x86 "no zstd bmi2" -DDYNAMIC_BMI2=1
# and claiming bmi2 is compiled in is a contradiction there
expect_off_x86 "build fails"  -DSTATIC_BMI2=1

# disassemble a build of libzstd.a into $2.
# No `make clean` between configurations: lib/ keys its object directories on
# the build flags, so each configuration has its own, and cleaning would throw
# away every other configuration's objects as a side effect of running a test.
disassemble() {
    make -C "$LIB_DIR" -j libzstd.a MOREFLAGS="$1" > /dev/null
    "$OBJDUMP" -d "$LIB_DIR/libzstd.a" > "$2"
    # an objdump that cannot read the target format prints nothing, which would
    # silently satisfy a "no cpuid" assertion; insist it actually disassembled
    grep -q 'file format' "$2" \
        || die "$OBJDUMP produced no disassembly; is it the right one for $CC's target?"
}

# Only the two objects that call ZSTD_cpuSupportsBmi2() are inspected, so that
# CPU detection added elsewhere for an unrelated feature does not fail this.
probe_cpuid() {
    awk '/\.o:[[:space:]]+file format/ { obj = $1 }
         /\tcpuid/ { if (obj == "zstd_compress.o:" || obj == "zstd_decompress.o:") n++ }
         END { print n + 0 }' "$1"
}

disassemble "-mbmi2" "$WORK/static.txt"
nb_cpuid=$(probe_cpuid "$WORK/static.txt")
[ "$nb_cpuid" -eq 0 ] \
    || die "FAIL: $nb_cpuid cpuid instruction(s) in a -mbmi2 build of" \
           "zstd_compress.o/zstd_decompress.o, expected none." \
           "If this is CPU detection unrelated to BMI2, narrow the check rather" \
           "than dropping it."
println "ok: -mbmi2 leaves no cpuid probe"

disassemble "-DDYNAMIC_BMI2=1 -mbmi2" "$WORK/dominance.txt"
diff -q "$WORK/static.txt" "$WORK/dominance.txt" > /dev/null \
    || die "FAIL: -DDYNAMIC_BMI2=1 -mbmi2 differs from -mbmi2, STATIC_BMI2 no longer dominates"
println "ok: STATIC_BMI2 dominates, -DDYNAMIC_BMI2=1 changes nothing"

# Instructions that only a BMI2 cpu can execute. tzcnt is deliberately absent:
# it is assembled as a backward compatible `rep bsf` and runs everywhere.
probe_bmi2() {
    awk '{ for (i = 1; i <= NF; i++)
               if ($i ~ /^(bzhi|shrx|sarx|shlx|pdep|pext|mulx|rorx)$/) n++ }
         END { print n + 0 }' "$1"
}

# The invariant that keeps a build off the illegal-instruction path: BMI2 may
# appear either because the whole library was compiled with it, or behind a
# runtime probe. Without -mbmi2 and without a probe, there must be none at all.
#
# This is what STATIC_BMI2=1 without -mbmi2 used to violate, silently, whenever
# the intrinsics were disabled: the C code carried no BMI2, so the build looked
# fine, while the Huffman assembly was still emitted and still entered with no
# check. The state assertions above cannot see it -- the state was "bmi2
# everywhere", which was exactly the problem, since it was not true.
for flags in "-DSTATIC_BMI2=0" "-DDYNAMIC_BMI2=0"; do
    disassemble "$flags" "$WORK/nobmi2.txt"
    nb_cpuid=$(probe_cpuid "$WORK/nobmi2.txt")
    nb_bmi2=$(probe_bmi2 "$WORK/nobmi2.txt")
    [ "$nb_cpuid" -eq 0 ] \
        || die "FAIL: $flags builds no dispatcher, yet $nb_cpuid cpuid probe(s) remain"
    [ "$nb_bmi2" -eq 0 ] \
        || die "FAIL: $nb_bmi2 BMI2 instruction(s) in a $flags build, which has" \
               "neither -mbmi2 nor a runtime probe. They would fault on a cpu" \
               "without BMI2."
    println "ok: $flags leaves no unguarded BMI2 instruction"
done

println "bmi2 build-configuration check: OK"
