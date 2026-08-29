/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_PORTABILITY_MACROS_H
#define ZSTD_PORTABILITY_MACROS_H

/**
 * This header file contains macro definitions to support portability.
 * This header is shared between C and ASM code, so it MUST only
 * contain macro definitions. It MUST not contain any C code.
 *
 * This header ONLY defines macros to detect platforms/feature support.
 *
 */

/* compat. with non-clang compilers */
#ifndef __has_attribute
  #define __has_attribute(x) 0
#endif

/* compat. with non-clang compilers */
#ifndef __has_builtin
#  define __has_builtin(x) 0
#endif

/* compat. with non-clang compilers */
#ifndef __has_feature
#  define __has_feature(x) 0
#endif

/* detects whether we are being compiled under msan */
#ifndef ZSTD_MEMORY_SANITIZER
#  if __has_feature(memory_sanitizer)
#    define ZSTD_MEMORY_SANITIZER 1
#  else
#    define ZSTD_MEMORY_SANITIZER 0
#  endif
#endif

/* detects whether we are being compiled under asan */
#ifndef ZSTD_ADDRESS_SANITIZER
#  if __has_feature(address_sanitizer)
#    define ZSTD_ADDRESS_SANITIZER 1
#  elif defined(__SANITIZE_ADDRESS__)
#    define ZSTD_ADDRESS_SANITIZER 1
#  else
#    define ZSTD_ADDRESS_SANITIZER 0
#  endif
#endif

/* detects whether we are being compiled under dfsan */
#ifndef ZSTD_DATAFLOW_SANITIZER
# if __has_feature(dataflow_sanitizer)
#  define ZSTD_DATAFLOW_SANITIZER 1
# else
#  define ZSTD_DATAFLOW_SANITIZER 0
# endif
#endif

/* Mark the internal assembly functions as hidden  */
#ifdef __ELF__
# define ZSTD_HIDE_ASM_FUNCTION(func) .hidden func
#elif defined(__APPLE__)
# define ZSTD_HIDE_ASM_FUNCTION(func) .private_extern func
#else
# define ZSTD_HIDE_ASM_FUNCTION(func)
#endif

/* Two macros the build may set, and four derived from them below. The user
 * facing rules, with the full table of combinations, are in lib/README.md,
 * under "bmi2 instructions, on x86"; what follows is how they are derived.
 *
 * Both build knobs say what zstd should do with its *own* bmi2 code -- the
 * intrinsics, the x86-64 assembly, the functions duplicated into a plain and a
 * BMI2_TARGET_ATTRIBUTE variant. Neither says anything about what the compiler
 * emits on its own from -mbmi2.
 *
 *   STATIC_BMI2  : =1 promises bmi2 is compiled in, so zstd may use it
 *                  unconditionally; =0 asks zstd to use none of its bmi2 paths,
 *                  even under -mbmi2. Autodetected from __BMI2__ when unset.
 *   DYNAMIC_BMI2 : =1 asks for the duplicated variants, one picked per context
 *                  at runtime after probing the CPU once at creation time.
 *
 * They are NOT orthogonal, and the four value combinations are not four valid
 * builds. Setting STATIC_BMI2 to *either* value already settles what zstd does
 * with bmi2, so nothing is left for a dispatcher to decide:
 *
 *   - an explicit STATIC_BMI2 with DYNAMIC_BMI2=1 is refused below. The build
 *     asked for two things that cannot both hold. Note this is about the build
 *     *setting* STATIC_BMI2=0, not about it resolving to 0: the ordinary
 *     generic build leaves it unset, autodetects 0, and dispatches.
 *   - an explicit STATIC_BMI2 with DYNAMIC_BMI2 unset simply wins.
 *
 * A request that the target cannot satisfy is ignored rather than refused; see
 * ZSTD_BMI2_DISPATCH_POSSIBLE below for which cases those are and why.
 *
 * Test STATIC_BMI2 rather than __BMI2__ directly: the two are not equivalent,
 * STATIC_BMI2 is also set for MSVC builds targeting AVX2, and may be forced to
 * 0 under -mbmi2. Note that "BMI2 instructions run on this path" is
 * STATIC_BMI2 || (ZSTD_BMI2_DISPATCH && dispatched), so no single macro answers
 * that question on its own.
 */

/* A constraint on everything below that mentions STATIC_BMI2.
 *
 * The freestanding export (contrib/linux-kernel) is given STATIC_BMI2 but not
 * DYNAMIC_BMI2, and does not preprocess conditions in general. PartialPreprocessor,
 * in contrib/freestanding_lib/freestanding.py, reads only the *first* term of an
 * #if, and only in one of these shapes:
 *     #ifdef MACRO   /   #ifndef MACRO
 *     #if [!]defined(MACRO)
 *     #if defined(MACRO) && MACRO <cmp> <n>
 *     #if MACRO
 * The macro must be one the export was given on its command line; any other is
 * left alone. A trailing && or || is noticed, but resolves the line only when
 * the first term settles it on its own -- false && ..., or true || ... . If it
 * does not, a defined() first term is deleted from the condition and the rest
 * kept, while a bare `#if MACRO` first term leaves the whole line untouched.
 * Macro bodies are never preprocessed.
 *
 * What survives untouched still has to compile. STATIC_BMI2 is resolved away
 * and never defined in the exported sources, so a condition naming it that the
 * export could not resolve becomes a reference to an undefined macro: -Wundef
 * for `#if STATIC_BMI2`, contrib/linux-kernel/test/macro-test.sh for the
 * #ifdef and defined() spellings. Either way the linux-kernel CI job fails
 * rather than shipping it.
 *
 * Hence the derived macros below test one macro at a time. The natural
 * (DYNAMIC_BMI2 && !STATIC_BMI2) does match a shape, but the export is never
 * told what DYNAMIC_BMI2 is, so the line would survive verbatim, carrying
 * STATIC_BMI2 with it.
 */

#ifndef ZSTD_BMI2_STATIC_IS_SET
/* Whether the build set STATIC_BMI2 itself, captured before the derivation
 * below gives it a value and erases the distinction. An explicit setting, 0 or
 * 1, states what the build wants from zstd's own bmi2 code paths, so there is
 * nothing left for a runtime dispatcher to decide and DYNAMIC_BMI2 is ignored.
 * Overridable so the freestanding export can say "not set by the build" while
 * still supplying a value for STATIC_BMI2 itself.
 * Inside the #ifndef, not above it, so that an export which supplies the value
 * drops the explanation along with the code; see the constraint above. */
#  ifdef STATIC_BMI2
#    define ZSTD_BMI2_STATIC_IS_SET 1
#  else
#    define ZSTD_BMI2_STATIC_IS_SET 0
#  endif
#endif

#ifndef STATIC_BMI2
/* Compile time determination of BMI2 support */
#  if defined(__BMI2__)
#    define STATIC_BMI2 1
#  elif defined(_MSC_VER) && !defined(__clang__) && defined(__AVX2__)
/* cl has no bmi2 switch and never defines __BMI2__, so read /arch:AVX2, which
 * does make it emit bmi2, and whose target cpus all have it. Only cl needs this
 * proxy: clang-cl defines _MSC_VER too, but reports __BMI2__ accurately above,
 * and /arch:AVX2 does not imply bmi2 for it. */
#    define STATIC_BMI2 1
#  endif
#endif

#ifndef STATIC_BMI2
#  define STATIC_BMI2 0
#endif

/* Whether the target is x86, the only architecture with BMI2 at all. */
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
#  define ZSTD_TARGET_X86 1
#else
#  define ZSTD_TARGET_X86 0
#endif

#if STATIC_BMI2
/* STATIC_BMI2=1 claims the whole library is compiled with BMI2, and the rest of
 * the library takes that at its word: it is what lets the Huffman assembly run
 * with no CPU check in front of it. Check the claim here, both ways.
 *
 * Off x86 it cannot hold at all.
 *
 * On x86 it requires that the compiler was actually told to emit BMI2, which is
 * exactly what __BMI2__ reports. Nothing else verifies this: a promise the build
 * makes and does not keep used to surface only as an undeclared _bzhi_u64, and
 * under ZSTD_NO_INTRINSICS there is no intrinsic left to fail on -- so the
 * library would silently link, with unguarded BMI2 in the assembly, and SIGILL
 * on a CPU without it.
 * cl is the exception, and only cl: it has no BMI2 switch and never defines
 * __BMI2__, so the derivation above reads /arch:AVX2 instead. Trust the same
 * reasoning here. clang-cl defines _MSC_VER but is not covered, deliberately --
 * it reports __BMI2__ like any clang, and exempting it would wave through a
 * build with no bmi2 codegen and unguarded bmi2 in the assembly.
 *
 * Nested rather than written as one condition, and commented from the inside,
 * so that the freestanding export resolves the outer test and drops the whole
 * block, comment included; see the constraint above. */
#  if !ZSTD_TARGET_X86
#    error "STATIC_BMI2=1 requires an x86 target: BMI2 exists nowhere else"
#  endif
#  if !defined(__BMI2__)
#    if !(defined(_MSC_VER) && !defined(__clang__) && defined(__AVX2__))
#      error "STATIC_BMI2=1 needs BMI2 codegen: -mbmi2, or /arch:AVX2 on MSVC"
#    endif
#  endif
#endif

/* Whether runtime BMI2 dispatch is possible at all. Three things are required,
 * and none of them is negotiable:
 *   - a compiler able to give one function a different target than the rest;
 *   - an x86 target, since there is nothing to dispatch on elsewhere;
 *   - BMI2 *not* already enabled for the whole translation unit. Under -mbmi2
 *     the compiler emits BMI2 into the plain variant too, so there is no
 *     non-BMI2 implementation to fall back to: a dispatcher would choose
 *     between two identical variants, its cpu probe would be pure overhead,
 *     and the branch it takes on a cpu without BMI2 would fault anyway.
 *
 * DYNAMIC_BMI2 is ignored when this is 0: asking for a dispatcher that cannot
 * work is not a request the library can honour.
 *
 * The last clause tests __BMI2__ rather than STATIC_BMI2, the one place that
 * does not follow the rule above, and it has to stay that way: this condition
 * is far too complex for the export to evaluate, so naming STATIC_BMI2 in it
 * would leave a dangling reference. See the constraint noted above.
 */
#if ((defined(__clang__) && __has_attribute(__target__)) \
     || (defined(__GNUC__) \
         && (__GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)))) \
    && ZSTD_TARGET_X86 \
    && !defined(__BMI2__)
#  define ZSTD_BMI2_DISPATCH_POSSIBLE 1
#else
#  define ZSTD_BMI2_DISPATCH_POSSIBLE 0
#endif

/* Whether the build set DYNAMIC_BMI2 itself, captured before the derivation
 * below gives it a value. Only needed to tell a request apart from a default,
 * so that a request that contradicts an explicit STATIC_BMI2 can be refused
 * rather than quietly dropped. Overridable for the freestanding export, as
 * ZSTD_BMI2_STATIC_IS_SET is. */
#ifndef ZSTD_BMI2_DYNAMIC_IS_SET
#  ifdef DYNAMIC_BMI2
#    define ZSTD_BMI2_DYNAMIC_IS_SET 1
#  else
#    define ZSTD_BMI2_DYNAMIC_IS_SET 0
#  endif
#endif

/* Enable runtime BMI2 dispatch based on the CPU.
 * Enabled for clang & gcc >=4.8 on x86 when BMI2 isn't enabled by default.
 */
#ifndef DYNAMIC_BMI2
#  if ZSTD_BMI2_DISPATCH_POSSIBLE
#    define DYNAMIC_BMI2 1
#  else
#    define DYNAMIC_BMI2 0
#  endif
#endif

#if ZSTD_BMI2_STATIC_IS_SET
/* An explicit STATIC_BMI2 already settles whether zstd uses its own bmi2 code,
 * so asking for a dispatcher as well cannot be honoured. Where dispatch is
 * merely impossible -- off x86, on a compiler without per-function target
 * attributes, or under -mbmi2 -- DYNAMIC_BMI2 is quietly ignored instead: that
 * is the target's doing, not the build's, and a portable build system should
 * not need per-architecture conditionals. This is the one case where the build
 * itself states two things that cannot both hold.
 *
 * To a build meeting the #error below: drop one of the two macros. For runtime
 * dispatch, leave STATIC_BMI2 unset -- STATIC_BMI2=0 does not mean "dispatch",
 * it means "use no bmi2 code path of zstd's own". To keep those paths off,
 * drop DYNAMIC_BMI2 instead; STATIC_BMI2=0 alone already does it.
 *
 * Nested, and commented from the inside, so that the freestanding export
 * resolves the outer test and drops the whole block, comment included. */
#  if ZSTD_BMI2_DYNAMIC_IS_SET
#    if DYNAMIC_BMI2
#      error "DYNAMIC_BMI2=1 contradicts an explicit STATIC_BMI2: set only one"
#    endif
#  endif
#endif

/* Whether two variants of the dispatched functions are compiled, one plain and
 * one BMI2_TARGET_ATTRIBUTE, to be selected at runtime.
 *
 * This is what the dispatch machinery must test, not DYNAMIC_BMI2 directly:
 * once the whole library is compiled with BMI2 the two variants are identical,
 * so there is nothing to select and the runtime CPUID probe is pure overhead.
 * STATIC_BMI2 therefore wins over DYNAMIC_BMI2.
 *
 * Tests one macro at a time, per the constraint noted above the STATIC_BMI2
 * derivation; DYNAMIC_BMI2 is defined to 0 or 1 by then, so the #elif is safe.
 *
 * Derived, not a build knob, and deliberately not #ifndef-guarded: STATIC_BMI2
 * means "use BMI2 unconditionally", so a dispatcher must not exist alongside
 * it, and an override could put that incoherent state back. Set STATIC_BMI2 or
 * DYNAMIC_BMI2 instead. The other derived macros here are unguarded too.
 *
 * ODR: this controls whether ZSTD_CCtx and ZSTD_DCtx carry a bmi2 field, so it
 * changes their layout and must be identical in every translation unit linked
 * into one binary. Inherited from DYNAMIC_BMI2, which has the same property.
 */
#if !ZSTD_BMI2_DISPATCH_POSSIBLE
#  define ZSTD_BMI2_DISPATCH 0
#elif ZSTD_BMI2_STATIC_IS_SET
#  define ZSTD_BMI2_DISPATCH 0
#elif STATIC_BMI2
#  define ZSTD_BMI2_DISPATCH 0
#elif DYNAMIC_BMI2
#  define ZSTD_BMI2_DISPATCH 1
#else
#  define ZSTD_BMI2_DISPATCH 0
#endif

/* Whether this build guarantees that BMI2 instructions can execute: either the
 * whole library is compiled with them, or a dispatcher checks the CPU before
 * reaching them. Anything emitting BMI2 unconditionally, such as the Huffman
 * assembly, must be gated on this rather than on DYNAMIC_BMI2, which says only
 * that dispatch was requested. Tests one macro at a time, per the constraint
 * noted above the STATIC_BMI2 derivation.
 */
#if STATIC_BMI2
#  define ZSTD_BMI2_AVAILABLE 1
#elif ZSTD_BMI2_DISPATCH
#  define ZSTD_BMI2_AVAILABLE 1
#else
#  define ZSTD_BMI2_AVAILABLE 0
#endif

/**
 * Only enable assembly for GNU C compatible compilers,
 * because other platforms may not support GAS assembly syntax.
 *
 * Only enable assembly for Linux / MacOS / Win32, other platforms may
 * work, but they haven't been tested. This could likely be
 * extended to BSD systems.
 *
 * Disable assembly when MSAN is enabled, because MSAN requires
 * 100% of code to be instrumented to work.
 */
#if defined(__GNUC__)
#  if defined(__linux__) || defined(__linux) || defined(__APPLE__) || defined(_WIN32)
#    if ZSTD_MEMORY_SANITIZER
#      define ZSTD_ASM_SUPPORTED 0
#    elif ZSTD_DATAFLOW_SANITIZER
#      define ZSTD_ASM_SUPPORTED 0
#    else
#      define ZSTD_ASM_SUPPORTED 1
#    endif
#  else
#    define ZSTD_ASM_SUPPORTED 0
#  endif
#else
#  define ZSTD_ASM_SUPPORTED 0
#endif

/**
 * Determines whether we should enable assembly for x86-64
 * with BMI2.
 *
 * Enable if all of the following conditions hold:
 * - ASM hasn't been explicitly disabled by defining ZSTD_DISABLE_ASM
 * - Assembly is supported
 * - We are compiling for x86-64 and either:
 *   - runtime dispatch is compiled in, so a caller can establish that the CPU
 *     supports BMI2 before reaching the assembly
 *   - BMI2 is supported at compile time
 *
 * Gate on ZSTD_BMI2_AVAILABLE, not DYNAMIC_BMI2: the assembly uses BMI2 with no
 * check of its own. The two agree unless ZSTD_BMI2_DISPATCH is overridden from
 * the build, and forcing it to 0 removes exactly the guard the assembly relies
 * on, leaving BMI2 instructions with no CPU detection anywhere.
 */
#if !defined(ZSTD_DISABLE_ASM) &&                                 \
    ZSTD_ASM_SUPPORTED &&                                         \
    defined(__x86_64__) &&                                        \
    ZSTD_BMI2_AVAILABLE
# define ZSTD_ENABLE_ASM_X86_64_BMI2 1
#else
# define ZSTD_ENABLE_ASM_X86_64_BMI2 0
#endif

/*
 * For x86 ELF targets, add .note.gnu.property section for Intel CET in
 * assembly sources when CET is enabled.
 *
 * Additionally, any function that may be called indirectly must begin
 * with ZSTD_CET_ENDBRANCH.
 */
#if defined(__ELF__) && (defined(__x86_64__) || defined(__i386__)) \
    && defined(__has_include)
# if __has_include(<cet.h>)
#  include <cet.h>
#  define ZSTD_CET_ENDBRANCH _CET_ENDBR
# endif
#endif

#ifndef ZSTD_CET_ENDBRANCH
# define ZSTD_CET_ENDBRANCH
#endif

/**
 * ZSTD_IS_DETERMINISTIC_BUILD must be set to 0 if any compilation macro is
 * active that impacts the compressed output.
 *
 * NOTE: ZSTD_MULTITHREAD is allowed to be set or unset.
 */
#if defined(ZSTD_CLEVEL_DEFAULT) \
    || defined(ZSTD_EXCLUDE_DFAST_BLOCK_COMPRESSOR) \
    || defined(ZSTD_EXCLUDE_GREEDY_BLOCK_COMPRESSOR) \
    || defined(ZSTD_EXCLUDE_LAZY_BLOCK_COMPRESSOR) \
    || defined(ZSTD_EXCLUDE_LAZY2_BLOCK_COMPRESSOR) \
    || defined(ZSTD_EXCLUDE_BTLAZY2_BLOCK_COMPRESSOR) \
    || defined(ZSTD_EXCLUDE_BTOPT_BLOCK_COMPRESSOR) \
    || defined(ZSTD_EXCLUDE_BTULTRA_BLOCK_COMPRESSOR)
# define ZSTD_IS_DETERMINISTIC_BUILD 0
#else
# define ZSTD_IS_DETERMINISTIC_BUILD 1
#endif

#endif /* ZSTD_PORTABILITY_MACROS_H */
