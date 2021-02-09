/*
 * Copyright (c) 2016-2021, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "zstd_trace.h"
#include "../zstd.h"

#include "compiler.h"

#if ZSTD_TRACE && ZSTD_HAVE_WEAK_SYMBOLS

ZSTD_WEAK_ATTR ZSTD_TraceCtx ZSTD_trace_compress_begin(ZSTD_CCtx const* cctx)
{
    (void)cctx;
    return 0;
}

ZSTD_WEAK_ATTR void ZSTD_trace_compress_end(ZSTD_TraceCtx ctx, ZSTD_Trace const* trace)
{
    (void)ctx;
    (void)trace;
}

ZSTD_WEAK_ATTR ZSTD_TraceCtx ZSTD_trace_decompress_begin(ZSTD_DCtx const* dctx)
{
    (void)dctx;
    return 0;
}

ZSTD_WEAK_ATTR void ZSTD_trace_decompress_end(ZSTD_TraceCtx ctx, ZSTD_Trace const* trace)
{
    (void)ctx;
    (void)trace;
}

#endif
