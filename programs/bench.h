/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#if defined (__cplusplus)
extern "C" {
#endif

#ifndef BENCH_H_121279284357
#define BENCH_H_121279284357

#include <stddef.h>   /* size_t */
#define ZSTD_STATIC_LINKING_ONLY   /* ZSTD_compressionParameters */
#include "zstd.h"     /* ZSTD_compressionParameters */

typedef struct {
    size_t cSize;
    double cSpeed;   /* bytes / sec */
    double dSpeed;
} BMK_result_t;

/* 0 = no Error */
typedef struct {
	int errorCode;
	BMK_result_t result;
} BMK_return_t;

/* called in cli */
int BMK_benchFiles(const char** fileNamesTable, unsigned nbFiles, const char* dictFileName,
                   int cLevel, int cLevelLast, const ZSTD_compressionParameters* compressionParams, 
                   int displayLevel);

/* basic benchmarking function, called in paramgrill
 * ctx, dctx must be valid */
BMK_return_t BMK_benchMem(const void* srcBuffer, size_t srcSize,
                        const size_t* fileSizes, unsigned nbFiles,
                        const int cLevel, const ZSTD_compressionParameters* comprParams,
                        const void* dictBuffer, size_t dictBufferSize,
                        ZSTD_CCtx* ctx, ZSTD_DCtx* dctx,
                        int displayLevel, const char* displayName);

/* Set Parameters */
void BMK_setNbSeconds(unsigned nbLoops);
void BMK_setBlockSize(size_t blockSize);
void BMK_setNbWorkers(unsigned nbWorkers);
void BMK_setRealTime(unsigned priority);
void BMK_setNotificationLevel(unsigned level);
void BMK_setSeparateFiles(unsigned separate);
void BMK_setAdditionalParam(int additionalParam);
void BMK_setDecodeOnlyMode(unsigned decodeFlag);
void BMK_setLdmFlag(unsigned ldmFlag);
void BMK_setLdmMinMatch(unsigned ldmMinMatch);
void BMK_setLdmHashLog(unsigned ldmHashLog);
void BMK_setLdmBucketSizeLog(unsigned ldmBucketSizeLog);
void BMK_setLdmHashEveryLog(unsigned ldmHashEveryLog);

#endif   /* BENCH_H_121279284357 */

#if defined (__cplusplus)
}
#endif
