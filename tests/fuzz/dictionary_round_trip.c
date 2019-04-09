/*
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 */

/**
 * This fuzz target performs a zstd round-trip test (compress & decompress) with
 * a dictionary, compares the result with the original, and calls abort() on
 * corruption.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fuzz_helpers.h"
#include "zstd_helpers.h"

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
    FUZZ_dict_t dict = FUZZ_train(src, srcSize, &seed);
    size_t cSize;
    if ((FUZZ_rand(&seed) & 15) == 0) {
        int const cLevel = FUZZ_rand(&seed) % kMaxClevel;

        cSize = ZSTD_compress_usingDict(cctx,
                compressed, compressedCapacity,
                src, srcSize,
                dict.buff, dict.size,
                cLevel);
    } else {
        FUZZ_setRandomParameters(cctx, srcSize, &seed);
        FUZZ_ZASSERT(ZSTD_CCtx_loadDictionary(cctx, dict.buff, dict.size));
        cSize = ZSTD_compress2(cctx, compressed, compressedCapacity, src, srcSize);
    }
    FUZZ_ZASSERT(cSize);
    {
        size_t const ret = ZSTD_decompress_usingDict(dctx,
                result, resultCapacity,
                compressed, cSize,
                dict.buff, dict.size);
        free(dict.buff);
        return ret;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *src, size_t size)
{
    size_t neededBufSize;

    seed = FUZZ_seed(&src, &size);
    neededBufSize = ZSTD_compressBound(size);

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
        FUZZ_ZASSERT(result);
        FUZZ_ASSERT_MSG(result == size, "Incorrect regenerated size");
        FUZZ_ASSERT_MSG(!memcmp(src, rBuf, size), "Corruption!");
    }
#ifndef STATEFUL_FUZZING
    ZSTD_freeCCtx(cctx); cctx = NULL;
    ZSTD_freeDCtx(dctx); dctx = NULL;
#endif
    return 0;
}
