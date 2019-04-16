/*
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 */

/**
 * This fuzz target attempts to decompress the fuzzed data with the dictionary
 * decompression function to ensure the decompressor never crashes. It does not
 * fuzz the dictionary.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "fuzz_helpers.h"
#include "zstd_helpers.h"

static ZSTD_DCtx *dctx = NULL;
static void* rBuf = NULL;
static size_t bufSize = 0;

int LLVMFuzzerTestOneInput(const uint8_t *src, size_t size)
{
    FUZZ_dict_t dict;
    size_t neededBufSize;

    uint32_t seed = FUZZ_seed(&src, &size);
    neededBufSize = MAX(20 * size, (size_t)256 << 10);

    /* Allocate all buffers and contexts if not already allocated */
    if (neededBufSize > bufSize) {
        free(rBuf);
        rBuf = malloc(neededBufSize);
        bufSize = neededBufSize;
        FUZZ_ASSERT(rBuf);
    }
    if (!dctx) {
        dctx = ZSTD_createDCtx();
        FUZZ_ASSERT(dctx);
    }
    dict = FUZZ_train(src, size, &seed);
    if (FUZZ_rand32(&seed, 0, 1) == 0) {
        ZSTD_decompress_usingDict(dctx,
                rBuf, neededBufSize,
                src, size,
                dict.buff, dict.size);
    } else {
        FUZZ_ZASSERT(ZSTD_DCtx_loadDictionary_advanced(
                dctx, dict.buff, dict.size,
                (ZSTD_dictLoadMethod_e)FUZZ_rand32(&seed, 0, 1),
                (ZSTD_dictContentType_e)FUZZ_rand32(&seed, 0, 2)));
        ZSTD_decompressDCtx(dctx, rBuf, neededBufSize, src, size);
    }

    free(dict.buff);
#ifndef STATEFUL_FUZZING
    ZSTD_freeDCtx(dctx); dctx = NULL;
#endif
    return 0;
}
