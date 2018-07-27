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

/* Creates a struct of type typeName with an int type .error field
 * and a .result field of some baseType. Functions with return
 * typeName pass a successful result with .error = 0 and .result
 * with the intended result, while returning an error will result
 * in .error != 0. 
 */
#define ERROR_STRUCT(baseType, typeName) typedef struct { \
    baseType result; \
    int error;       \
} typeName

typedef struct {
    size_t cSize;
    double cSpeed;   /* bytes / sec */
    double dSpeed;
    size_t cMem;
} BMK_result_t;

ERROR_STRUCT(BMK_result_t, BMK_return_t);

/* called in cli */
/* Loads files in fileNamesTable into memory, as well as a dictionary 
 * from dictFileName, and then uses benchMem */
/* fileNamesTable - name of files to benchmark
 * nbFiles - number of files (size of fileNamesTable), must be > 0
 * dictFileName - name of dictionary file to load
 * cLevel - compression level to benchmark, errors if invalid
 * compressionParams - basic compression Parameters
 * displayLevel - what gets printed
 *      0 : no display;   
 *      1 : errors;   
 *      2 : + result + interaction + warnings;   
 *      3 : + progression;   
 *      4 : + information
 * return 
 *      .error will give a nonzero error value if an error has occured
 *      .result - if .error = 0, .result will return the time taken to compression speed
 *          (.cSpeed), decompression speed (.dSpeed), and compressed size (.cSize) of the original
 *          file
 */
BMK_return_t BMK_benchFiles(const char* const * const fileNamesTable, unsigned const nbFiles,
                   const char* const dictFileName, 
                   int const cLevel, const ZSTD_compressionParameters* const compressionParams, 
                   int displayLevel);

typedef enum {
    BMK_timeMode = 0,
    BMK_iterMode = 1
} BMK_loopMode_t;

typedef enum {
    BMK_both = 0,
    BMK_decodeOnly = 1,
    BMK_compressOnly = 2
} BMK_mode_t;

typedef struct {
    BMK_mode_t mode;            /* 0: all, 1: compress only 2: decode only */
    BMK_loopMode_t loopMode;    /* if loopmode, then nbSeconds = nbLoops */
    unsigned nbSeconds;         /* default timing is in nbSeconds */
    size_t blockSize;           /* Maximum allowable size of a block*/
    unsigned nbWorkers;         /* multithreading */
    unsigned realTime;          /* real time priority */
    int additionalParam;        /* used by python speed benchmark */
    unsigned ldmFlag;           /* enables long distance matching */
    unsigned ldmMinMatch;       /* below: parameters for long distance matching, see zstd.1.md for meaning */
    unsigned ldmHashLog; 
    unsigned ldmBucketSizeLog;
    unsigned ldmHashEveryLog;
} BMK_advancedParams_t;

/* returns default parameters used by nonAdvanced functions */
BMK_advancedParams_t BMK_initAdvancedParams(void);

/* See benchFiles for normal parameter uses and return, see advancedParams_t for adv */
BMK_return_t BMK_benchFilesAdvanced(const char* const * const fileNamesTable, unsigned const nbFiles,
                   const char* const dictFileName, 
                   int const cLevel, const ZSTD_compressionParameters* const compressionParams, 
                   int displayLevel, const BMK_advancedParams_t* const adv);

/* called in cli */
/* Generates a sample with datagen with the compressibility argument*/
/* cLevel - compression level to benchmark, errors if invalid
 * compressibility - determines compressibility of sample
 * compressionParams - basic compression Parameters
 * displayLevel - see benchFiles
 * adv - see advanced_Params_t
 * return 
 *      .error will give a nonzero error value if an error has occured
 *      .result - if .error = 0, .result will return the time taken to compression speed
 *          (.cSpeed), decompression speed (.dSpeed), and compressed size (.cSize) of the original
 *          file
 */
BMK_return_t BMK_syntheticTest(int cLevel, double compressibility,
                              const ZSTD_compressionParameters* compressionParams,
                              int displayLevel, const BMK_advancedParams_t * const adv);

/* basic benchmarking function, called in paramgrill 
 * applies ZSTD_compress_generic() and ZSTD_decompress_generic() on data in srcBuffer
 * with specific compression parameters specified by other arguments using benchFunction
 * (cLevel, comprParams + adv in advanced Mode) */
/* srcBuffer - data source, expected to be valid compressed data if in Decode Only Mode
 * srcSize - size of data in srcBuffer
 * cLevel - compression level  
 * comprParams - basic compression parameters
 * dictBuffer - a dictionary if used, null otherwise
 * dictBufferSize - size of dictBuffer, 0 otherwise
 * ctx - Compression Context (must be provided)
 * dctx - Decompression Context (must be provided)
 * diplayLevel - see BMK_benchFiles
 * displayName - name used by display
 * return
 *      .error will give a nonzero value if an error has occured
 *      .result - if .error = 0, will give the same results as benchFiles
 *          but for the data stored in srcBuffer
 */
BMK_return_t BMK_benchMem(const void* srcBuffer, size_t srcSize,
                        const size_t* fileSizes, unsigned nbFiles,
                        const int cLevel, const ZSTD_compressionParameters* comprParams,
                        const void* dictBuffer, size_t dictBufferSize,
                        ZSTD_CCtx* ctx, ZSTD_DCtx* dctx,
                        int displayLevel, const char* displayName);

/* See benchMem for normal parameter uses and return, see advancedParams_t for adv 
 * dstBuffer - destination buffer to write compressed output in, NULL if none provided.
 * dstCapacity - capacity of destination buffer, give 0 if dstBuffer = NULL
 */
BMK_return_t BMK_benchMemAdvanced(const void* srcBuffer, size_t srcSize,
                        void* dstBuffer, size_t dstCapacity, 
                        const size_t* fileSizes, unsigned nbFiles,
                        const int cLevel, const ZSTD_compressionParameters* comprParams,
                        const void* dictBuffer, size_t dictBufferSize,
                        ZSTD_CCtx* ctx, ZSTD_DCtx* dctx,
                        int displayLevel, const char* displayName,
                        const BMK_advancedParams_t* adv);

typedef struct {
    size_t sumOfReturn;    /* sum of return values */
    U64 nanoSecPerRun;     /* time per iteration */
} BMK_customResult_t;

ERROR_STRUCT(BMK_customResult_t, BMK_customReturn_t);

typedef size_t (*BMK_benchFn_t)(const void*, size_t, void*, size_t, void*);
typedef size_t (*BMK_initFn_t)(void*);

/* This function times the execution of 2 argument functions, benchFn and initFn  */

/* benchFn - (*benchFn)(srcBuffers[i], srcSizes[i], dstBuffers[i], dstCapacities[i], benchPayload)
 *      is run nbLoops times
 * initFn - (*initFn)(initPayload) is run once per benchmark at the beginning. This argument can 
 *          be NULL, in which case nothing is run.
 * blockCount - number of blocks (size of srcBuffers, srcSizes, dstBuffers, dstCapacities)
 * srcBuffers - an array of buffers to be operated on by benchFn
 * srcSizes - an array of the sizes of above buffers
 * dstBuffers - an array of buffers to be written into by benchFn
 * dstCapacities - an array of the capacities of above buffers
 * blockResults - the return value of benchFn called on each block.
 * nbLoops - defines number of times benchFn is run.
 * assumed array of size blockCount, will have compressed size of each block written to it.
 * return 
 *      .error will give a nonzero value if ZSTD_isError() is nonzero for any of the return
 *          of the calls to initFn and benchFn, or if benchFunction errors internally
 *      .result - if .error = 0, then .result will contain the sum of all return values of 
 *          benchFn on the first iteration through all of the blocks (.sumOfReturn) and also 
 *          the time per run of benchFn (.nanoSecPerRun). For the former, this
 *          is generally intended to be used on functions which return the # of bytes written 
 *          into dstBuffer, hence this value will be the total amount of bytes written to 
 *          dstBuffer.
 */
BMK_customReturn_t BMK_benchFunction(BMK_benchFn_t benchFn, void* benchPayload,
                        BMK_initFn_t initFn, void* initPayload,
                        size_t blockCount,
                        const void* const * const srcBuffers, const size_t* srcSizes,
                        void * const * const dstBuffers, const size_t* dstCapacities, size_t* blockResults,  
                        unsigned nbLoops);


/* state information needed to advance computation for benchFunctionTimed */
typedef struct BMK_timeState_t BMK_timedFnState_t;
/* initializes timeState object with desired number of seconds */
BMK_timedFnState_t* BMK_createTimeState(unsigned nbSeconds);
/* resets existing timeState object */
void BMK_resetTimeState(BMK_timedFnState_t*, unsigned nbSeconds);
/* deletes timeState object */
void BMK_freeTimeState(BMK_timedFnState_t* state);

typedef struct {
    BMK_customReturn_t result;
    int completed;
} BMK_customTimedReturn_t;

/* 
 * Benchmarks custom functions like BMK_benchFunction(), but runs for nbSeconds seconds rather than a fixed number of loops
 * arguments mostly the same other than BMK_benchFunction()
 * Usage - benchFunctionTimed will return in approximately one second. Keep calling BMK_benchFunctionTimed() until the return's completed field = 1. 
 * to continue updating intermediate result. Intermediate return values are returned by the function.
 */
BMK_customTimedReturn_t BMK_benchFunctionTimed(BMK_timedFnState_t* cont,
    BMK_benchFn_t benchFn, void* benchPayload,
    BMK_initFn_t initFn, void* initPayload,
    size_t blockCount,
    const void* const * const srcBlockBuffers, const size_t* srcBlockSizes,
    void* const * const dstBlockBuffers, const size_t* dstBlockCapacities, size_t* blockResults);

#endif   /* BENCH_H_121279284357 */

#if defined (__cplusplus)
}
#endif
