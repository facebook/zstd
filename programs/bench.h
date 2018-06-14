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

typedef enum {
    BMK_timeMode = 0,
    BMK_iterMode = 1
} BMK_loopMode_t;

typedef enum {
    BMK_both = 0,
    BMK_decodeOnly = 1,
    BMK_compressOnly = 2
} BMK_mode_t;

#define ERROR_STRUCT(baseType, typeName) typedef struct { \
    int error;       \
    baseType result; \
} typeName

typedef struct {
    size_t cSize;
    double cSpeed;   /* bytes / sec */
    double dSpeed;
} BMK_result_t;

typedef struct {
    int cLevel;
    int cLevelLast;
    unsigned nbFiles;
    BMK_result_t** results;
} BMK_resultSet_t;

typedef struct {
    size_t size;
    U64 time;
} BMK_customResult_t;


ERROR_STRUCT(BMK_result_t, BMK_return_t);
ERROR_STRUCT(BMK_resultSet_t, BMK_returnSet_t);
ERROR_STRUCT(BMK_customResult_t, BMK_customReturn_t);

/* want all 0 to be default, but wb ldmBucketSizeLog/ldmHashEveryLog */
typedef struct {
    BMK_mode_t mode; /* 0: all, 1: compress only 2: decode only */
    BMK_loopMode_t loopMode; /* if loopmode, then nbSeconds = nbLoops */
    unsigned nbSeconds; /* default timing is in nbSeconds. If nbCycles != 0 then use that */
    size_t blockSize; /* Maximum allowable size of a block*/
    unsigned nbWorkers; /* multithreading */
    unsigned realTime;
    unsigned separateFiles;
    int additionalParam;
    unsigned ldmFlag;
    unsigned ldmMinMatch;
    unsigned ldmHashLog;
    unsigned ldmBucketSizeLog;
    unsigned ldmHashEveryLog;
} BMK_advancedParams_t;

/* returns default parameters used by nonAdvanced functions */
BMK_advancedParams_t BMK_defaultAdvancedParams(void);

/* functionName - name of function
 * blockCount - number of blocks (size of srcBuffers, srcSizes, dstBuffers, dstSizes)
 * initFn - (*initFn)(initPayload) is run once per benchmark
 * benchFn - (*benchFn)(srcBuffers[i], srcSizes[i], dstBuffers[i], dstSizes[i], benchPayload)
 *      is run a variable number of times, specified by mode and iter args
 * mode - if 0, iter will be interpreted as the minimum number of seconds to run
 * iter - see mode
 * displayLevel - what gets printed
 *      0 : no display;   
 *      1 : errors;   
 *      2 : + result + interaction + warnings;   
 *      3 : + progression;   
 *      4 : + information
 * return 
 *      .error will give a nonzero value if any error has occured
 *      .result will contain the speed (B/s) and time per loop (ns)
 */
BMK_customReturn_t BMK_benchCustom(const char* functionName, size_t blockCount,
                        const void* const * const srcBuffers, const size_t* srcSizes,
                        void* const * const dstBuffers, const size_t* dstSizes,
                        size_t (*initFn)(void*), size_t (*benchFn)(const void*, size_t, void*, size_t, void*), 
                        void* initPayload, void* benchPayload,
                        unsigned mode, unsigned iter,
                        int displayLevel);

/* basic benchmarking function, called in paramgrill ctx, dctx must be provided */
/* srcBuffer - data source, expected to be valid compressed data if in Decode Only Mode
 * srcSize - size of data in srcBuffer
 * cLevel - compression level  
 * comprParams - basic compression parameters
 * dictBuffer - a dictionary if used, null otherwise
 * dictBufferSize - size of dictBuffer, 0 otherwise
 * ctx - Compression Context
 * dctx - Decompression Context
 * diplayLevel - see BMK_benchCustom
 * displayName - name used in display
 */
BMK_return_t BMK_benchMem(const void* srcBuffer, size_t srcSize,
                        const size_t* fileSizes, unsigned nbFiles,
                        const int cLevel, const ZSTD_compressionParameters* comprParams,
                        const void* dictBuffer, size_t dictBufferSize,
                        ZSTD_CCtx* ctx, ZSTD_DCtx* dctx,
                        int displayLevel, const char* displayName);

BMK_return_t BMK_benchMemAdvanced(const void* srcBuffer, size_t srcSize,
                        const size_t* fileSizes, unsigned nbFiles,
                        const int cLevel, const ZSTD_compressionParameters* comprParams,
                        const void* dictBuffer, size_t dictBufferSize,
                        ZSTD_CCtx* ctx, ZSTD_DCtx* dctx,
                        int displayLevel, const char* displayName,
                        const BMK_advancedParams_t* adv);

/* called in cli */
/* fileNamesTable - name of files to benchmark
 * nbFiles - number of files (size of fileNamesTable)
 * dictFileName - name of dictionary file to load
 * cLevel - lowest compression level to benchmark
 * cLevellast - highest compression level to benchmark (everything in the range [cLevel, cLevellast]) will be benchmarked
 * compressionParams - basic compression Parameters
 * displayLevel - see BMK_benchCustom
 */
int BMK_benchFiles(const char** fileNamesTable, unsigned nbFiles, const char* dictFileName,
                   int cLevel, int cLevelLast, const ZSTD_compressionParameters* compressionParams, 
                   int displayLevel);

BMK_returnSet_t BMK_benchFilesAdvanced(const char** fileNamesTable, unsigned nbFiles,
                   const char* dictFileName, 
                   int cLevel, int cLevelLast, 
                   const ZSTD_compressionParameters* compressionParams, 
                   int displayLevel, const BMK_advancedParams_t* adv);

/* get data from resultSet */
/* when aggregated (separateFiles = 0), just be getResult(r,0,cl) */
BMK_result_t BMK_getResult(BMK_resultSet_t results, unsigned fileIdx, int cLevel);
void BMK_freeResultSet(BMK_resultSet_t src);

#endif   /* BENCH_H_121279284357 */

#if defined (__cplusplus)
}
#endif
