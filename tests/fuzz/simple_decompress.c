/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/**
 * This fuzz target attempts to decompress the fuzzed data with the simple
 * decompression function to ensure the decompressor never crashes.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#define ZSTD_STATIC_LINKING_ONLY

#include "fuzz_helpers.h"
#include "zstd.h"
#include "fuzz_data_producer.h"

static ZSTD_DCtx *dctx = NULL;

int LLVMFuzzerTestOneInput(const uint8_t *src, size_t size)
{
    /* Give a random portion of src data to the producer, to use for
    parameter generation. The rest will be used for (de)compression */
    FUZZ_dataProducer_t *producer = FUZZ_dataProducer_create(src, size);
    size = FUZZ_dataProducer_reserveDataPrefix(producer);

    if (!dctx) {
        dctx = ZSTD_createDCtx();
        FUZZ_ASSERT(dctx);
    }

    {
        size_t const bufSize = FUZZ_dataProducer_uint32Range(producer, 0, 10 * size);
        void *rBuf = FUZZ_malloc(bufSize);
        size_t const dSize = ZSTD_decompressDCtx(dctx, rBuf, bufSize, src, size);
        if (!ZSTD_isError(dSize)) {
            /* If decompression was successful, the content size from the frame header(s) should be valid. */
            unsigned long long const expectedSize = ZSTD_findDecompressedSize(src, size);
            FUZZ_ASSERT(expectedSize != ZSTD_CONTENTSIZE_ERROR);
            FUZZ_ASSERT(expectedSize == ZSTD_CONTENTSIZE_UNKNOWN || expectedSize == dSize);
        }
        free(rBuf);
    }

    FUZZ_dataProducer_free(producer);

#ifndef STATEFUL_FUZZING
    ZSTD_freeDCtx(dctx); dctx = NULL;
#endif
    return 0;
}
