/*
 * Copyright (c) 2016-2021, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "zstdcli_trace.h"

#include <stdio.h>
#include <stdlib.h>

#include "timefn.h"
#include "util.h"

#define ZSTD_STATIC_LINKING_ONLY
#include "../lib/zstd.h"
/* We depend on the trace header to avoid duplicating the ZSTD_trace struct.
 * But, we check the version so it is compatible with dynamic linking.
 */
#include "../lib/common/zstd_trace.h"
/* We only use macros from threading.h so it is compatible with dynamic linking */
#include "../lib/common/threading.h"

#if ZSTD_TRACE

static FILE* g_traceFile = NULL;
static int g_mutexInit = 0;
static ZSTD_pthread_mutex_t g_mutex;

void TRACE_enable(char const* filename)
{
    int const writeHeader = !UTIL_isRegularFile(filename);
    if (g_traceFile)
        fclose(g_traceFile);
    g_traceFile = fopen(filename, "a");
    if (g_traceFile && writeHeader) {
        /* Fields:
        * algorithm
        * version
        * method
        * streaming
        * level
        * workers
        * dictionary size
        * uncompressed size
        * compressed size
        * duration nanos
        * compression ratio
        * speed MB/s
        */
        fprintf(g_traceFile, "Algorithm, Version, Method, Mode, Level, Workers, Dictionary Size, Uncompressed Size, Compressed Size, Duration Nanos, Compression Ratio, Speed MB/s\n");
    }
    if (!g_mutexInit) {
        if (!ZSTD_pthread_mutex_init(&g_mutex, NULL)) {
            g_mutexInit = 1;
        } else {
            TRACE_finish();
        }
    }
}

void TRACE_finish(void)
{
    if (g_traceFile) {
        fclose(g_traceFile);
    }
    g_traceFile = NULL;
    if (g_mutexInit) {
        ZSTD_pthread_mutex_destroy(&g_mutex);
        g_mutexInit = 0;
    }
}

static void TRACE_log(char const* method, PTime duration, ZSTD_trace const* trace)
{
    int level = 0;
    int workers = 0;
    double const ratio = (double)trace->uncompressedSize / (double)trace->compressedSize;
    double const speed = ((double)trace->uncompressedSize * 1000) / (double)duration;
    if (trace->params) {
        ZSTD_CCtxParams_getParameter(trace->params, ZSTD_c_compressionLevel, &level);
        ZSTD_CCtxParams_getParameter(trace->params, ZSTD_c_nbWorkers, &workers);
    }
    assert(g_traceFile != NULL);

    /* Fields:
     * algorithm
     * version
     * method
     * streaming
     * level
     * workers
     * dictionary size
     * uncompressed size
     * compressed size
     * duration nanos
     * compression ratio
     * speed MB/s
     */
    fprintf(g_traceFile,
        "zstd, %u, %s, %s, %d, %d, %llu, %llu, %llu, %llu, %.2f, %.2f\n",
        trace->version,
        method,
        trace->streaming ? "streaming" : "single-pass",
        level,
        workers,
        (unsigned long long)trace->dictionarySize,
        (unsigned long long)trace->uncompressedSize,
        (unsigned long long)trace->compressedSize,
        (unsigned long long)duration,
        ratio,
        speed);
}

static ZSTD_CCtx const* g_cctx;
static ZSTD_DCtx const* g_dctx;
static UTIL_time_t g_begin = UTIL_TIME_INITIALIZER;

/**
 * These symbols override the weak symbols provided by the library.
 */

int ZSTD_trace_compress_begin(ZSTD_CCtx const* cctx)
{
    int enabled = 0;
    if (g_traceFile == NULL)
        return 0;
    ZSTD_pthread_mutex_lock(&g_mutex);
    if (g_cctx == NULL) {
        g_cctx = cctx;
        g_dctx = NULL;
        g_begin = UTIL_getTime();
        enabled = 1;
    }
    ZSTD_pthread_mutex_unlock(&g_mutex);
    return enabled;
}

void ZSTD_trace_compress_end(ZSTD_CCtx const* cctx, ZSTD_trace const* trace)
{
    assert(g_traceFile != NULL);
    ZSTD_pthread_mutex_lock(&g_mutex);
    assert(g_cctx == cctx);
    assert(g_dctx == NULL);
    if (cctx == g_cctx && trace->version == ZSTD_VERSION_NUMBER)
        TRACE_log("compress", UTIL_clockSpanNano(g_begin), trace);
    g_cctx = NULL;
    ZSTD_pthread_mutex_unlock(&g_mutex);
}

int ZSTD_trace_decompress_begin(ZSTD_DCtx const* dctx)
{
    int enabled = 0;
    if (g_traceFile == NULL)
        return 0;
    ZSTD_pthread_mutex_lock(&g_mutex);
    if (g_dctx == NULL) {
        g_cctx = NULL;
        g_dctx = dctx;
        g_begin = UTIL_getTime();
        enabled = 1;
    }
    ZSTD_pthread_mutex_unlock(&g_mutex);
    return enabled;
}

void ZSTD_trace_decompress_end(ZSTD_DCtx const* dctx, ZSTD_trace const* trace)
{
    assert(g_traceFile != NULL);
    ZSTD_pthread_mutex_lock(&g_mutex);
    assert(g_cctx == NULL);
    assert(g_dctx == dctx);
    if (dctx == g_dctx && trace->version == ZSTD_VERSION_NUMBER)
        TRACE_log("decompress", UTIL_clockSpanNano(g_begin), trace);
    g_dctx = NULL;
    ZSTD_pthread_mutex_unlock(&g_mutex);
}

#else /* ZSTD_TRACE */

void TRACE_enable(char const* filename)
{
    (void)filename;
}

void TRACE_finish(void) {}

#endif /* ZSTD_TRACE */
