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
 * This fuzz target fuzzes all of the helper functions that consume compressed
 * input.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "fuzz_helpers.h"
#include "zstd_helpers.h"

int LLVMFuzzerTestOneInput(const uint8_t *src, size_t size)
{
    ZSTD_FrameHeader zfh;
    if (size == 0) {
        src = NULL;
   }
    /* You can fuzz any helper functions here that are fast, and take zstd
     * compressed data as input. E.g. don't expect the input to be a dictionary,
     * so don't fuzz ZSTD_getDictID_fromDict().
     */
    ZSTD_getFrameContentSize(src, size);
    ZSTD_getDecompressedSize(src, size);
    ZSTD_findFrameCompressedSize(src, size);
    ZSTD_getDictID_fromFrame(src, size);
    ZSTD_findDecompressedSize(src, size);
    ZSTD_decompressBound(src, size);
    ZSTD_frameHeaderSize(src, size);
    ZSTD_isFrame(src, size);
    ZSTD_getFrameHeader(&zfh, src, size);
    ZSTD_getFrameHeader_advanced(&zfh, src, size, ZSTD_f_zstd1);
    /* Frame-boundary scanner (issue #4734): validate structural invariants. */
    {
        /* 1024 entries is plenty: smallest possible frame is 6 bytes, and we
         * accept dstSize_tooSmall as a valid outcome for denser inputs. */
        ZSTD_FrameBoundary bounds[1024];
        size_t const counted = ZSTD_findFrameBoundaries(src, size, NULL, 0);
        size_t const filled = ZSTD_findFrameBoundaries(src, size, bounds, 1024);
        if (!ZSTD_isError(counted) && !ZSTD_isError(filled)) {
            size_t i;
            FUZZ_ASSERT(counted == filled);
            FUZZ_ASSERT(filled <= 1024);
            for (i = 0; i < filled; i++) {
                FUZZ_ASSERT(bounds[i].compressedSize > 0);
                FUZZ_ASSERT(bounds[i].offset <= size);
                FUZZ_ASSERT(bounds[i].compressedSize <= size - bounds[i].offset);
                if (i == 0) {
                    FUZZ_ASSERT(bounds[i].offset == 0);
                } else {
                    FUZZ_ASSERT(bounds[i].offset ==
                                bounds[i-1].offset + bounds[i-1].compressedSize);
                }
            }
            if (filled > 0) {
                FUZZ_ASSERT(bounds[filled-1].offset + bounds[filled-1].compressedSize == size);
            } else {
                FUZZ_ASSERT(size == 0);
            }
        } else if (!ZSTD_isError(counted)) {
            /* counting pass succeeded: the fill pass may only fail on capacity */
            FUZZ_ASSERT(ZSTD_getErrorCode(filled) == ZSTD_error_dstSize_tooSmall);
        } else {
            /* counting pass failed: the fill pass must also fail (it walks the
             * same frames; it may hit dstSize_tooSmall before the bad frame). */
            FUZZ_ASSERT(ZSTD_isError(filled));
        }
    }
    return 0;
}
