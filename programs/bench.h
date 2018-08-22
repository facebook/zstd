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

/* Creates a struct type typeName, featuring:
 * - an .error field of type int
 * - a .result field of some baseType.
 * Functions with return type typeName
 * will either be successful, with .error = 0, providing a valid .result,
 * or return an error, with .error != 0, in which case .result is invalid.
 */
#define ERROR_STRUCT(baseType, typeName) typedef struct { \
    baseType result; \
    int error;       \
} typeName

typedef struct {
    size_t cSize;
    unsigned long long cSpeed;   /* bytes / sec */
    unsigned long long dSpeed;
    size_t cMem;                 /* ? what does it reports ? */
} BMK_result_t;

ERROR_STRUCT(BMK_result_t, BMK_return_t);

/*! BMK_benchFiles() -- called by zstdcli */
/*  Loads files from fileNamesTable into memory,
 *  loads dictionary from dictFileName,
 *  then uses benchMem().
 *  fileNamesTable - name of files to benchmark
 *  nbFiles - number of files (size of fileNamesTable), must be > 0  (what happens if not ?)
 *  dictFileName - name of dictionary file to load
 *  cLevel - compression level to benchmark, errors if invalid
 *  compressionParams - advanced compression Parameters
 *  displayLevel - what gets printed
 *      0 : no display;
 *      1 : errors;
 *      2 : + result + interaction + warnings;
 *      3 : + progression;
 *      4 : + information
 * @return
 *      .error will give a nonzero error value if an error has occured
 *      .result - only valid if .error = 0,
 *          .result will return compression speed (.cSpeed),
 *          decompression speed (.dSpeed), and compressed size (.cSize).
 */
BMK_return_t BMK_benchFiles(const char* const * fileNamesTable, unsigned nbFiles,
                   const char* dictFileName,
                   int cLevel, const ZSTD_compressionParameters* compressionParams,
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

/*! BMK_benchFilesAdvanced():
 *  Same as BMK_benchFiles(),
 *  with more controls, provided through advancedParams_t structure */
BMK_return_t BMK_benchFilesAdvanced(const char* const * fileNamesTable, unsigned nbFiles,
                   const char* dictFileName,
                   int cLevel, const ZSTD_compressionParameters* compressionParams,
                   int displayLevel, const BMK_advancedParams_t* adv);

/*! BMK_syntheticTest() -- called from zstdcli */
/*  Generates a sample with datagen, using compressibility argument */
/*  cLevel - compression level to benchmark, errors if invalid
 *  compressibility - determines compressibility of sample
 *  compressionParams - basic compression Parameters
 *  displayLevel - see benchFiles
 *  adv - see advanced_Params_t
 * @return:
 *      .error will give a nonzero error value if an error has occured
 *      .result - only valid if .error = 0,
 *          .result will return the compression speed (.cSpeed),
 *          decompression speed (.dSpeed), and compressed size (.cSize).
 */
BMK_return_t BMK_syntheticTest(int cLevel, double compressibility,
                              const ZSTD_compressionParameters* compressionParams,
                              int displayLevel, const BMK_advancedParams_t * const adv);

/** BMK_benchMem() -- core benchmarking function, called in paramgrill
 *  applies ZSTD_compress_generic() and ZSTD_decompress_generic() on data in srcBuffer
 *  with specific compression parameters provided by other arguments using benchFunction
 *  (cLevel, comprParams + adv in advanced Mode) */
/*  srcBuffer - data source, expected to be valid compressed data if in Decode Only Mode
 *  srcSize - size of data in srcBuffer
 *  fileSizes - srcBuffer is considered cut into 1+ segments, to compress separately.
 *              note : sum(fileSizes) must be == srcSize.  (<== ensure it's properly checked)
 *  nbFiles - nb of segments
 *  cLevel - compression level
 *  comprParams - basic compression parameters
 *  dictBuffer - a dictionary if used, null otherwise
 *  dictBufferSize - size of dictBuffer, 0 otherwise
 *  diplayLevel - see BMK_benchFiles
 *  displayName - name used by display
 * @return
 *      .error will give a nonzero value if an error has occured
 *      .result - only valid if .error = 0,
 *          provide the same results as benchFiles()
 *          but for the data stored in srcBuffer
 */
BMK_return_t BMK_benchMem(const void* srcBuffer, size_t srcSize,
                        const size_t* fileSizes, unsigned nbFiles,
                        int cLevel, const ZSTD_compressionParameters* comprParams,
                        const void* dictBuffer, size_t dictBufferSize,
                        int displayLevel, const char* displayName);

/* BMK_benchMemAdvanced() : same as BMK_benchMem()
 * with following additional options :
 * dstBuffer - destination buffer to write compressed output in, NULL if none provided.
 * dstCapacity - capacity of destination buffer, give 0 if dstBuffer = NULL
 * adv = see advancedParams_t
 */
BMK_return_t BMK_benchMemAdvanced(const void* srcBuffer, size_t srcSize,
                        void* dstBuffer, size_t dstCapacity,
                        const size_t* fileSizes, unsigned nbFiles,
                        int cLevel, const ZSTD_compressionParameters* comprParams,
                        const void* dictBuffer, size_t dictBufferSize,
                        int displayLevel, const char* displayName,
                        const BMK_advancedParams_t* adv);

typedef struct {
    size_t sumOfReturn;    /* sum of return values */
    unsigned long long nanoSecPerRun;     /* time per iteration */
} BMK_customResult_t;

ERROR_STRUCT(BMK_customResult_t, BMK_customReturn_t);

typedef size_t (*BMK_benchFn_t)(const void*, size_t, void*, size_t, void*);
typedef size_t (*BMK_initFn_t)(void*);

/* This function times the execution of 2 argument functions, benchFn and initFn  */

/* benchFn - (*benchFn)(srcBuffers[i], srcSizes[i], dstBuffers[i], dstCapacities[i], benchPayload)
 *      is run nbLoops times
 * initFn - (*initFn)(initPayload) is run once per benchmark at the beginning. This argument can
 *          be NULL, in which case nothing is run.
 * blockCount - number of blocks. Size of all array parameters : srcBuffers, srcSizes, dstBuffers, dstCapacities, blockResults
 * srcBuffers - an array of buffers to be operated on by benchFn
 * srcSizes - an array of the sizes of above buffers
 * dstBuffers - an array of buffers to be written into by benchFn
 * dstCapacities - an array of the capacities of above buffers
 * blockResults - the return value of benchFn called on each block.
 * nbLoops - defines number of times benchFn is run.
 * @return:
 *      .error will give a nonzero value if ZSTD_isError() is nonzero for any of the return
 *          of the calls to initFn and benchFn, or if benchFunction errors internally
 *      .result - if .error = 0, then .result will contain
 *          the sum of all return values of benchFn on the first iteration through all of the blocks (.sumOfReturn)
 *          and also the time per run of benchFn (.nanoSecPerRun).
 *          For the former, this is generally intended to be used on functions which return the # of bytes written into dstBuffer,
 *          hence this value will be the total amount of bytes written into dstBuffer.
 */
BMK_customReturn_t BMK_benchFunction(BMK_benchFn_t benchFn, void* benchPayload,
                        BMK_initFn_t initFn, void* initPayload,
                        size_t blockCount,
                        const void *const * srcBuffers, const size_t* srcSizes,
                        void *const * dstBuffers, const size_t* dstCapacities, size_t* blockResults,
                        unsigned nbLoops);


/* state information needed to advance computation for benchFunctionTimed */
typedef struct BMK_timeState_t BMK_timedFnState_t;
/* initializes timeState object with desired number of seconds */
BMK_timedFnState_t* BMK_createTimedFnState(unsigned nbSeconds);
/* resets existing timeState object */
void BMK_resetTimedFnState(BMK_timedFnState_t*, unsigned nbSeconds);
/* deletes timeState object */
void BMK_freeTimedFnState(BMK_timedFnState_t* state);

typedef struct {
    BMK_customReturn_t result;
    int completed;
} BMK_customTimedReturn_t;

/* BMK_benchFunctionTimed() :
 * Same as BMK_benchFunction(), but runs for nbSeconds seconds rather than a fixed number of loops.
 * Arguments are mostly the same other as BMK_benchFunction()
 * Usage - benchFunctionTimed will return in approximately one second.
 * Keep calling BMK_benchFunctionTimed() until @return.completed == 1,
 * to continue updating intermediate result.
 * Intermediate return values are returned by the function.
 */
BMK_customTimedReturn_t BMK_benchFunctionTimed(BMK_timedFnState_t* cont,
    BMK_benchFn_t benchFn, void* benchPayload,
    BMK_initFn_t initFn, void* initPayload,
    size_t blockCount,
    const void *const * srcBlockBuffers, const size_t* srcBlockSizes,
    void *const * dstBlockBuffers, const size_t* dstBlockCapacities, size_t* blockResults);

#endif   /* BENCH_H_121279284357 */

#if defined (__cplusplus)
}
#endif
