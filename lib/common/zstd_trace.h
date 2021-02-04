/*
 * Copyright (c) 2016-2021, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_TRACE_H
#define ZSTD_TRACE_H

#include <stddef.h>

/* weak symbol support */
#if !defined(ZSTD_HAVE_WEAK_SYMBOLS) && defined(__GNUC__) && \
    !defined(__APPLE__) && !defined(_WIN32) && !defined(__MINGW32__) && \
    !defined(__CYGWIN__)
#  define ZSTD_HAVE_WEAK_SYMBOLS 1
#else
#  define ZSTD_HAVE_WEAK_SYMBOLS 0
#endif
#if ZSTD_HAVE_WEAK_SYMBOLS
#  define ZSTD_WEAK_ATTR __attribute__((__weak__))
#else
#  define ZSTD_WEAK_ATTR
#endif

/* Only enable tracing when weak symbols are available. */
#ifndef ZSTD_TRACE
#  define ZSTD_TRACE ZSTD_HAVE_WEAK_SYMBOLS
#endif

#if ZSTD_TRACE

struct ZSTD_CCtx_s;
struct ZSTD_DCtx_s;
struct ZSTD_CCtx_params_s;

typedef struct {
    /**
     * ZSTD_VERSION_NUMBER
     *
     * This is guaranteed to be the first member of ZSTD_trace.
     * Otherwise, this struct is not stable between versions. If
     * the version number does not match your expectation, you
     * should not interpret the rest of the struct.
     */
    unsigned version;
    /**
     * Non-zero if streaming (de)compression is used.
     */
    unsigned streaming;
    /**
     * The dictionary ID.
     */
    unsigned dictionaryID;
    /**
     * Is the dictionary cold?
     * Only set on decompression.
     */
    unsigned dictionaryIsCold;
    /**
     * The dictionary size or zero if no dictionary.
     */
    size_t dictionarySize;
    /**
     * The uncompressed size of the data.
     */
    size_t uncompressedSize;
    /**
     * The compressed size of the data.
     */
    size_t compressedSize;
    /**
     * The fully resolved CCtx parameters (NULL on decompression).
     */
    struct ZSTD_CCtx_params_s const* params;
} ZSTD_trace;

/**
 * Trace the beginning of a compression call.
 * @param cctx The dctx pointer for the compression.
 *             It can be used as a key to map begin() to end().
 * @returns Non-zero if tracing is enabled.
 */
int ZSTD_trace_compress_begin(struct ZSTD_CCtx_s const* cctx);

/**
 * Trace the end of a compression call.
 * @param cctx The dctx pointer for the decompression.
 * @param trace The zstd tracing info.
 */
void ZSTD_trace_compress_end(
    struct ZSTD_CCtx_s const* cctx,
    ZSTD_trace const* trace);

/**
 * Trace the beginning of a decompression call.
 * @param dctx The dctx pointer for the decompression.
 *             It can be used as a key to map begin() to end().
 * @returns Non-zero if tracing is enabled.
 */
int ZSTD_trace_decompress_begin(struct ZSTD_DCtx_s const* dctx);

/**
 * Trace the end of a decompression call.
 * @param dctx The dctx pointer for the decompression.
 * @param trace The zstd tracing info.
 */
void ZSTD_trace_decompress_end(
    struct ZSTD_DCtx_s const* dctx,
    ZSTD_trace const* trace);

#endif /* ZSTD_TRACE */

#endif /* ZSTD_TRACE_H */
