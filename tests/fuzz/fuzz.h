/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

/**
 * Fuzz target interface.
 * Fuzz targets have some common parameters passed as macros during compilation.
 * Check the documentation for each individual fuzzer for more parameters.
 *
 * @param STATEFULL_FUZZING:
 *        Define this to reuse state between fuzzer runs. This can be useful to
 *        test code paths which are only executed when contexts are reused.
 *        WARNING: Makes reproducing crashes much harder.
 *        Default: Not defined.
 * @param FUZZ_RNG_SEED_SIZE:
 *        The number of bytes of the source to look at when constructing a seed
 *        for the deterministic RNG.
 *        Default: 128.
 * @param ZSTD_DEBUG:
 *        This is a parameter for the zstd library. Defining `ZSTD_DEBUG=1`
 *        enables assert() statements in the zstd library. Higher levels enable
 *        logging, so aren't recommended. Defining `ZSTD_DEBUG=1` is
 *        recommended.
 * @param MEM_FORCE_MEMORY_ACCESS:
 *        This flag controls how the zstd library accesses unaligned memory.
 *        It can be undefined, or 0 through 2. If it is undefined, it selects
 *        the method to use based on the compiler. If testing with UBSAN set
 *        MEM_FORCE_MEMORY_ACCESS=0 to use the standard compliant method.
 * @param FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
 *        This is the canonical flag to enable deterministic builds for fuzzing.
 *        Changes to zstd for fuzzing are gated behind this define.
 *        It is recommended to define this when building zstd for fuzzing.
 */

#ifndef FUZZ_H
#define FUZZ_H

#ifndef FUZZ_RNG_SEED_SIZE
#  define FUZZ_RNG_SEED_SIZE 128
#endif

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *src, size_t size);

#endif
