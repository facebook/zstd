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

static ZSTD_CStream *cstream = NULL;
static ZSTD_DCtx *dctx = NULL;
static uint8_t* cBuf = NULL;
static uint8_t* rBuf = NULL;
static size_t bufSize = 0;
static uint32_t seed;

static ZSTD_outBuffer makeOutBuffer(uint8_t *dst, size_t capacity)
{
  ZSTD_outBuffer buffer = { dst, 0, 0 };

  FUZZ_ASSERT(capacity > 0);
  buffer.size = (FUZZ_rand(&seed) % capacity) + 1;
  FUZZ_ASSERT(buffer.size <= capacity);

  return buffer;
}

static ZSTD_inBuffer makeInBuffer(const uint8_t **src, size_t *size)
{
  ZSTD_inBuffer buffer = { *src, 0, 0 };

  FUZZ_ASSERT(*size > 0);
  buffer.size = (FUZZ_rand(&seed) % *size) + 1;
  FUZZ_ASSERT(buffer.size <= *size);
  *src += buffer.size;
  *size -= buffer.size;

  return buffer;
}

static size_t compress(uint8_t *dst, size_t capacity,
                       const uint8_t *src, size_t srcSize)
{
    int cLevel = FUZZ_rand(&seed) % kMaxClevel;
    size_t dstSize = 0;
    FUZZ_ASSERT(!ZSTD_isError(ZSTD_initCStream(cstream, cLevel)));

    while (srcSize > 0) {
        ZSTD_inBuffer in = makeInBuffer(&src, &srcSize);
        /* Mode controls the action. If mode == -1 we pick a new mode */
        int mode = -1;
        while (in.pos < in.size) {
          ZSTD_outBuffer out = makeOutBuffer(dst, capacity);
          /* Previous action finished, pick a new mode. */
          if (mode == -1) mode = FUZZ_rand(&seed) % 10;
          switch (mode) {
            case 0: /* fall-though */
            case 1: /* fall-though */
            case 2: {
                size_t const ret = ZSTD_flushStream(cstream, &out);
                FUZZ_ASSERT_MSG(!ZSTD_isError(ret), ZSTD_getErrorName(ret));
                if (ret == 0) mode = -1;
                break;
            }
            case 3: {
                size_t ret = ZSTD_endStream(cstream, &out);
                FUZZ_ASSERT_MSG(!ZSTD_isError(ret), ZSTD_getErrorName(ret));
                /* Reset the compressor when the frame is finished */
                if (ret == 0) {
                    cLevel = FUZZ_rand(&seed) % kMaxClevel;
                    ret = ZSTD_initCStream(cstream, cLevel);
                    FUZZ_ASSERT(!ZSTD_isError(ret));
                    mode = -1;
                }
                break;
            }
            default: {
                size_t const ret = ZSTD_compressStream(cstream, &out, &in);
                FUZZ_ASSERT_MSG(!ZSTD_isError(ret), ZSTD_getErrorName(ret));
                mode = -1;
            }
          }
          dst += out.pos;
          dstSize += out.pos;
          capacity -= out.pos;
        }
    }
    for (;;) {
        ZSTD_outBuffer out = makeOutBuffer(dst, capacity);
        size_t const ret = ZSTD_endStream(cstream, &out);
        FUZZ_ASSERT_MSG(!ZSTD_isError(ret), ZSTD_getErrorName(ret));

        dst += out.pos;
        dstSize += out.pos;
        capacity -= out.pos;
        if (ret == 0) break;
    }
    return dstSize;
}

int LLVMFuzzerTestOneInput(const uint8_t *src, size_t size)
{
    size_t const neededBufSize = ZSTD_compressBound(size) * 2;

    seed = FUZZ_seed(src, size);

    /* Allocate all buffers and contexts if not already allocated */
    if (neededBufSize > bufSize) {
        free(cBuf);
        free(rBuf);
        cBuf = (uint8_t*)malloc(neededBufSize);
        rBuf = (uint8_t*)malloc(neededBufSize);
        bufSize = neededBufSize;
        FUZZ_ASSERT(cBuf && rBuf);
    }
    if (!cstream) {
        cstream = ZSTD_createCStream();
        FUZZ_ASSERT(cstream);
    }
    if (!dctx) {
        dctx = ZSTD_createDCtx();
        FUZZ_ASSERT(dctx);
    }

    {
        size_t const cSize = compress(cBuf, neededBufSize, src, size);
        size_t const rSize =
            ZSTD_decompressDCtx(dctx, rBuf, neededBufSize, cBuf, cSize);
        FUZZ_ASSERT_MSG(!ZSTD_isError(rSize), ZSTD_getErrorName(rSize));
        FUZZ_ASSERT_MSG(rSize == size, "Incorrect regenerated size");
        FUZZ_ASSERT_MSG(!memcmp(src, rBuf, size), "Corruption!");
    }

#ifndef STATEFULL_FUZZING
    ZSTD_freeCStream(cstream); cstream = NULL;
    ZSTD_freeDCtx(dctx); dctx = NULL;
#endif
    return 0;
}
