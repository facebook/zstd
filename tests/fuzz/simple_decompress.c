/*
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 */

/**
 * This fuzz target attempts to decompress the fuzzed data with the simple
 * decompression function to ensure the decompressor never crashes.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "fuzz_helpers.h"
#include "zstd.h"
#include "fuzz_data_producer.h"

static ZSTD_DCtx *dctx = NULL;

int LLVMFuzzerTestOneInput(const uint8_t *src, size_t size)
{

    FUZZ_dataProducer_t *producer = FUZZ_dataProducer_create(src, size);
    int i;
    if (!dctx) {
        dctx = ZSTD_createDCtx();
        FUZZ_ASSERT(dctx);
    }
    /* Run it 10 times over 10 output sizes. Reuse the context. */
    for (i = 0; i < 10; ++i) {
        size_t const bufSize = FUZZ_dataProducer_uint32Range(producer, 0, 2 * size);
        void* rBuf = malloc(bufSize);
        FUZZ_ASSERT(rBuf);
        ZSTD_decompressDCtx(dctx, rBuf, bufSize, src, size);
        free(rBuf);
    }

    FUZZ_dataProducer_free(producer);

#ifndef STATEFUL_FUZZING
    ZSTD_freeDCtx(dctx); dctx = NULL;
#endif
    return 0;
}
