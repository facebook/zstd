/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

/**
 * This fuzz target performs a zstd round-trip test (compress & decompress),
 * compares the result with the original, and calls abort() on corruption.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fuzz_helpers.h"
#include "zstd.h"

static const int kMaxClevel = 19;

static ZSTD_CCtx *cctx = NULL;
static ZSTD_DCtx *dctx = NULL;
static void* cBuf = NULL;
static void* rBuf = NULL;
static size_t bufSize = 0;
static uint32_t seed;

static size_t roundTripTest(void *result, size_t resultCapacity,
                            void *compressed, size_t compressedCapacity,
                            const void *src, size_t srcSize)
{
  int const cLevel = FUZZ_rand(&seed) % kMaxClevel;
  size_t const cSize = ZSTD_compressCCtx(cctx, compressed, compressedCapacity,
                                         src, srcSize, cLevel);
  if (ZSTD_isError(cSize)) {
    fprintf(stderr, "Compression error: %s\n", ZSTD_getErrorName(cSize));
    return cSize;
  }
  return ZSTD_decompressDCtx(dctx, result, resultCapacity, compressed, cSize);
}

int LLVMFuzzerTestOneInput(const uint8_t *src, size_t size)
{
    size_t const neededBufSize = ZSTD_compressBound(size);

    seed = FUZZ_seed(src, size);

    /* Allocate all buffers and contexts if not already allocated */
    if (neededBufSize > bufSize) {
        free(cBuf);
        free(rBuf);
        cBuf = malloc(neededBufSize);
        rBuf = malloc(neededBufSize);
        bufSize = neededBufSize;
        FUZZ_ASSERT(cBuf && rBuf);
    }
    if (!cctx) {
        cctx = ZSTD_createCCtx();
        FUZZ_ASSERT(cctx);
    }
    if (!dctx) {
        dctx = ZSTD_createDCtx();
        FUZZ_ASSERT(dctx);
    }

    {
        size_t const result =
            roundTripTest(rBuf, neededBufSize, cBuf, neededBufSize, src, size);
        FUZZ_ASSERT_MSG(!ZSTD_isError(result), ZSTD_getErrorName(result));
        FUZZ_ASSERT_MSG(result == size, "Incorrect regenerated size");
        FUZZ_ASSERT_MSG(!memcmp(src, rBuf, size), "Corruption!");
    }
#ifndef STATEFULL_FUZZING
    ZSTD_freeCCtx(cctx); cctx = NULL;
    ZSTD_freeDCtx(dctx); dctx = NULL;
#endif
    return 0;
}
