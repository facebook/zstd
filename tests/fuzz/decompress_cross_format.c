/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

// This fuzz target validates decompression of magicless-format compressed data.

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fuzz_helpers.h"
#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"
#include "fuzz_data_producer.h"

static ZSTD_DCtx *dctx = NULL;

int LLVMFuzzerTestOneInput(const uint8_t *src, size_t size)
{
    // Give a random portion of src data to the producer, to use for parameter generation.
    // The rest will be interpreted as magicless compressed data.
    FUZZ_dataProducer_t *producer = FUZZ_dataProducer_create(src, size);
    size_t magiclessSize = FUZZ_dataProducer_reserveDataPrefix(producer);
    const uint8_t* const magiclessSrc = src;
    size_t const dstSize = FUZZ_dataProducer_uint32Range(producer, 0, 10 * size);
    uint8_t* const standardDst = (uint8_t*)FUZZ_malloc(dstSize);
    uint8_t* const magiclessDst = (uint8_t*)FUZZ_malloc(dstSize);

    // Create standard-format src from magicless-format src
    const uint32_t zstd_magic = ZSTD_MAGICNUMBER;
    size_t standardSize = sizeof(zstd_magic) + magiclessSize;
    uint8_t* const standardSrc = (uint8_t*)FUZZ_malloc(standardSize);
    memcpy(standardSrc, &zstd_magic, sizeof(zstd_magic)); // assume fuzzing on little-endian machine
    memcpy(standardSrc + sizeof(zstd_magic), magiclessSrc, magiclessSize);

    // Truncate to a single frame
    {
        const size_t standardFrameCompressedSize = ZSTD_findFrameCompressedSize(standardSrc, standardSize);
        if (ZSTD_isError(standardFrameCompressedSize)) {
            goto cleanup_and_return;
        }
        standardSize = standardFrameCompressedSize;
        magiclessSize = standardFrameCompressedSize - sizeof(zstd_magic);
    }

    // Create DCtx if needed
    if (!dctx) {
        dctx = ZSTD_createDCtx();
        FUZZ_ASSERT(dctx);
    }

    // Test one-shot decompression
    {
        FUZZ_ZASSERT(ZSTD_DCtx_reset(dctx, ZSTD_reset_session_and_parameters));
        FUZZ_ZASSERT(ZSTD_DCtx_setParameter(dctx, ZSTD_d_format, ZSTD_f_zstd1));
        const size_t standardRet = ZSTD_decompressDCtx(
                                        dctx, standardDst, dstSize, standardSrc, standardSize);

        FUZZ_ZASSERT(ZSTD_DCtx_reset(dctx, ZSTD_reset_session_and_parameters));
        FUZZ_ZASSERT(ZSTD_DCtx_setParameter(dctx, ZSTD_d_format, ZSTD_f_zstd1_magicless));
        const size_t magiclessRet = ZSTD_decompressDCtx(
                                        dctx, magiclessDst, dstSize, magiclessSrc, magiclessSize);

        // Standard accepts => magicless should accept
        if (!ZSTD_isError(standardRet)) FUZZ_ZASSERT(magiclessRet);

        // Magicless accepts => standard should accept
        // NOTE: this is nice-to-have, please disable this check if it is difficult to satisfy.
        if (!ZSTD_isError(magiclessRet)) FUZZ_ZASSERT(standardRet);

        // If both accept, decompressed size and data should match
        if (!ZSTD_isError(standardRet) && !ZSTD_isError(magiclessRet)) {
            FUZZ_ASSERT(standardRet == magiclessRet);
            if (standardRet > 0) {
                FUZZ_ASSERT(
                    memcmp(standardDst, magiclessDst, standardRet) == 0
                );
            }
        }
    }

    // Test streaming decompression
    {
        ZSTD_inBuffer standardIn = { standardSrc, standardSize, 0 };
        ZSTD_inBuffer magiclessIn = { magiclessSrc, magiclessSize, 0 };
        ZSTD_outBuffer standardOut = { standardDst, dstSize, 0 };
        ZSTD_outBuffer magiclessOut = { magiclessDst, dstSize, 0 };

        FUZZ_ZASSERT(ZSTD_DCtx_reset(dctx, ZSTD_reset_session_and_parameters));
        FUZZ_ZASSERT(ZSTD_DCtx_setParameter(dctx, ZSTD_d_format, ZSTD_f_zstd1));
        const size_t standardRet = ZSTD_decompressStream(dctx, &standardOut, &standardIn);

        FUZZ_ZASSERT(ZSTD_DCtx_reset(dctx, ZSTD_reset_session_and_parameters));
        FUZZ_ZASSERT(ZSTD_DCtx_setParameter(dctx, ZSTD_d_format, ZSTD_f_zstd1_magicless));
        const size_t magiclessRet = ZSTD_decompressStream(dctx, &magiclessOut, &magiclessIn);

        // Standard accepts => magicless should accept
        if (standardRet == 0) FUZZ_ASSERT(magiclessRet == 0);

        // Magicless accepts => standard should accept
        // NOTE: this is nice-to-have, please disable this check if it is difficult to satisfy.
        if (magiclessRet == 0) FUZZ_ASSERT(standardRet == 0);

        // If both accept, decompressed size and data should match
        if (standardRet == 0 && magiclessRet == 0) {
            FUZZ_ASSERT(standardOut.pos == magiclessOut.pos);
            if (standardOut.pos > 0) {
                FUZZ_ASSERT(
                    memcmp(standardOut.dst, magiclessOut.dst, standardOut.pos) == 0
                );
            }
        }
    }

cleanup_and_return:
#ifndef STATEFUL_FUZZING
    ZSTD_freeDCtx(dctx); dctx = NULL;
#endif
    free(standardSrc);
    free(standardDst);
    free(magiclessDst);
    FUZZ_dataProducer_free(producer);
    return 0;
}
