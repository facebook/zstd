/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */


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

/* called in cli */
int BMK_benchFiles(const char** fileNamesTable, unsigned nbFiles, const char* dictFileName,
                   int cLevel, int cLevelLast, const ZSTD_compressionParameters* compressionParams);

/* more options */
BMK_result_t BMK_benchFilesFull(const char** fileNamesTable, unsigned nbFiles, const char* dictFileName,
                   int cLevel, int cLevelLast, const ZSTD_compressionParameters* compressionParams,
                   int printable);

/* basic benchmarking function, called in paramgrill
 * results, cctx, dctx, dictbuffer can be null or passed in */
BMK_result_t BMK_benchMem(const void* srcBuffer, size_t srcSize, const char* displayName, int cLevel,
                 const size_t* fileSizes, unsigned nbFiles, const void* dictBuffer, size_t dictBufferSize,
                 const ZSTD_compressionParameters* const comprParams, ZSTD_CCtx* ctx, ZSTD_DCtx* dctx,
                 int printable);

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
