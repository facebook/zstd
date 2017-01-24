/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

/* ===   Dependencies   === */
#include <stddef.h>   /* size_t */
#define ZSTD_STATIC_LINKING_ONLY   /* ZSTD_parameters */
#include "zstd.h"     /* ZSTD_inBuffer, ZSTD_outBuffer, ZSTDLIB_API */


/* ===   Simple one-pass functions   === */

typedef struct ZSTDMT_CCtx_s ZSTDMT_CCtx;
ZSTDLIB_API ZSTDMT_CCtx* ZSTDMT_createCCtx(unsigned nbThreads);
ZSTDLIB_API size_t ZSTDMT_freeCCtx(ZSTDMT_CCtx* cctx);

ZSTDLIB_API size_t ZSTDMT_compressCCtx(ZSTDMT_CCtx* cctx,
                           void* dst, size_t dstCapacity,
                     const void* src, size_t srcSize,
                           int compressionLevel);


/* ===   Streaming functions   === */

ZSTDLIB_API size_t ZSTDMT_initCStream(ZSTDMT_CCtx* zcs, int compressionLevel);
ZSTDLIB_API size_t ZSTDMT_resetCStream(ZSTDMT_CCtx* zcs, unsigned long long pledgedSrcSize);    /**< pledgedSrcSize is optional and can be zero == unknown */
ZSTDLIB_API size_t ZSTDMT_initCStream_advanced(ZSTDMT_CCtx* zcs, const void* dict, size_t dictSize,   /**< dict can be released after init, a local copy is preserved within zcs */
                                   ZSTD_parameters params, unsigned long long pledgedSrcSize);  /**< pledgedSrcSize is optional and can be zero == unknown */

ZSTDLIB_API size_t ZSTDMT_compressStream(ZSTDMT_CCtx* zcs, ZSTD_outBuffer* output, ZSTD_inBuffer* input);

ZSTDLIB_API size_t ZSTDMT_flushStream(ZSTDMT_CCtx* zcs, ZSTD_outBuffer* output);   /**< @return : 0 == all flushed; >0 : still some data to be flushed; or an error code (ZSTD_isError()) */
ZSTDLIB_API size_t ZSTDMT_endStream(ZSTDMT_CCtx* zcs, ZSTD_outBuffer* output);     /**< @return : 0 == all flushed; >0 : still some data to be flushed; or an error code (ZSTD_isError()) */
