/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* Asserts which of the three bmi2 states a build resolved to.
 *
 * tests/check-bmi2-dispatch.sh covers this for gcc and clang on x86, and goes
 * further, inspecting the object code. It does not port to the Windows
 * toolchains, and they are the ones whose rules differ: cl has no bmi2 switch,
 * so zstd reads /arch:AVX2 instead, and no per-function target attributes, so
 * it cannot dispatch at all. clang-cl defines _MSC_VER as well, but is a clang
 * and follows clang's rules. Those differences are worth pinning down.
 *
 * This is the part that ports: pure preprocessor. No build, no execution, and
 * no headers beyond zstd's own, which is what lets it run as a bare compiler
 * invocation on a Windows runner.
 *
 * Compile with exactly one of ZSTD_EXPECT_EVERYWHERE, ZSTD_EXPECT_DISPATCH or
 * ZSTD_EXPECT_NONE. The three states are the ones named in lib/README.md.
 */

#include "portability_macros.h"

/* The same three-way choice the probe in check-bmi2-dispatch.sh makes, in the
 * same order: dispatch first, because under dispatch ZSTD_BMI2_AVAILABLE is
 * also 1, and it is the more specific answer. */
#if ZSTD_BMI2_DISPATCH
#  define ZSTD_STATE_DISPATCH 1
#elif ZSTD_BMI2_AVAILABLE
#  define ZSTD_STATE_EVERYWHERE 1
#else
#  define ZSTD_STATE_NONE 1
#endif

/* Reported whether or not the assertion holds: on failure it says what was
 * resolved, which an #error cannot; on success it records the answer for a
 * toolchain we cannot easily interrogate any other way. The parenthesised form
 * is the one cl, clang-cl and gcc all accept. */
#if defined(ZSTD_STATE_EVERYWHERE)
#  pragma message("zstd bmi2 state: bmi2 everywhere")
#elif defined(ZSTD_STATE_DISPATCH)
#  pragma message("zstd bmi2 state: runtime dispatch")
#else
#  pragma message("zstd bmi2 state: no zstd bmi2")
#endif

#if (defined(ZSTD_EXPECT_EVERYWHERE) + defined(ZSTD_EXPECT_DISPATCH) \
     + defined(ZSTD_EXPECT_NONE)) != 1
#  error "define exactly one of ZSTD_EXPECT_EVERYWHERE, _DISPATCH, _NONE"
#endif

#if defined(ZSTD_EXPECT_EVERYWHERE) && !defined(ZSTD_STATE_EVERYWHERE)
#  error "expected 'bmi2 everywhere'"
#endif
#if defined(ZSTD_EXPECT_DISPATCH) && !defined(ZSTD_STATE_DISPATCH)
#  error "expected 'runtime dispatch'"
#endif
#if defined(ZSTD_EXPECT_NONE) && !defined(ZSTD_STATE_NONE)
#  error "expected 'no zstd bmi2'"
#endif

/* a translation unit may not be empty */
typedef int zstd_bmi2_state_probe_t;
