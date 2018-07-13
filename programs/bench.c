/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */



/* **************************************
*  Tuning parameters
****************************************/
#ifndef BMK_TIMETEST_DEFAULT_S   /* default minimum time per test */
#define BMK_TIMETEST_DEFAULT_S 3
#endif


/* **************************************
*  Compiler Warnings
****************************************/
#ifdef _MSC_VER
#  pragma warning(disable : 4127)   /* disable: C4127: conditional expression is constant */
#endif


/* *************************************
*  Includes
***************************************/
#include "platform.h"    /* Large Files support */
#include "util.h"        /* UTIL_getFileSize, UTIL_sleep */
#include <stdlib.h>      /* malloc, free */
#include <string.h>      /* memset */
#include <stdio.h>       /* fprintf, fopen */
#include <assert.h>      /* assert */

#include "mem.h"
#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"
#include "datagen.h"     /* RDG_genBuffer */
#include "xxhash.h"
#include "bench.h"
#include "zstd_errors.h"


/* *************************************
*  Constants
***************************************/
#ifndef ZSTD_GIT_COMMIT
#  define ZSTD_GIT_COMMIT_STRING ""
#else
#  define ZSTD_GIT_COMMIT_STRING ZSTD_EXPAND_AND_QUOTE(ZSTD_GIT_COMMIT)
#endif

#define TIMELOOP_MICROSEC     (1*1000000ULL) /* 1 second */
#define TIMELOOP_NANOSEC      (1*1000000000ULL) /* 1 second */
#define ACTIVEPERIOD_MICROSEC (70*TIMELOOP_MICROSEC) /* 70 seconds */
#define COOLPERIOD_SEC        10

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

static const size_t maxMemory = (sizeof(size_t)==4)  ?  (2 GB - 64 MB) : (size_t)(1ULL << ((sizeof(size_t)*8)-31));

/* *************************************
*  console display
***************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (displayLevel>=l) { DISPLAY(__VA_ARGS__); }
/* 0 : no display;   1: errors;   2 : + result + interaction + warnings;   3 : + progression;   4 : + information */

static const U64 g_refreshRate = SEC_TO_MICRO / 6;
static UTIL_time_t g_displayClock = UTIL_TIME_INITIALIZER;

#define DISPLAYUPDATE(l, ...) { if (displayLevel>=l) { \
            if ((UTIL_clockSpanMicro(g_displayClock) > g_refreshRate) || (displayLevel>=4)) \
            { g_displayClock = UTIL_getTime(); DISPLAY(__VA_ARGS__); \
            if (displayLevel>=4) fflush(stderr); } } }


/* *************************************
*  Exceptions
***************************************/
#ifndef DEBUG
#  define DEBUG 0
#endif
#define DEBUGOUTPUT(...) { if (DEBUG) DISPLAY(__VA_ARGS__); }

#define EXM_THROW_INT(errorNum, ...)  {               \
    DEBUGOUTPUT("%s: %i: \n", __FILE__, __LINE__);    \
    DISPLAYLEVEL(1, "Error %i : ", errorNum);         \
    DISPLAYLEVEL(1, __VA_ARGS__);                     \
    DISPLAYLEVEL(1, " \n");                           \
    return errorNum;                                  \
}

#define EXM_THROW(errorNum, retType, ...)  {          \
    retType r;                                        \
    memset(&r, 0, sizeof(retType));                   \
    DEBUGOUTPUT("%s: %i: \n", __FILE__, __LINE__);    \
    DISPLAYLEVEL(1, "Error %i : ", errorNum);         \
    DISPLAYLEVEL(1, __VA_ARGS__);                     \
    DISPLAYLEVEL(1, " \n");                           \
    r.error = errorNum;                               \
    return r;                                         \
}

/* error without displaying */
#define EXM_THROW_ND(errorNum, retType, ...)  {       \
    retType r;                                        \
    memset(&r, 0, sizeof(retType));                   \
    DEBUGOUTPUT("%s: %i: \n", __FILE__, __LINE__);    \
    DEBUGOUTPUT("Error %i : ", errorNum);             \
    DEBUGOUTPUT(__VA_ARGS__);                         \
    DEBUGOUTPUT(" \n");                               \
    r.error = errorNum;                               \
    return r;                                         \
}

/* *************************************
*  Benchmark Parameters
***************************************/

BMK_advancedParams_t BMK_initAdvancedParams(void) { 
    BMK_advancedParams_t res = { 
        BMK_both, /* mode */
        BMK_timeMode, /* loopMode */
        BMK_TIMETEST_DEFAULT_S, /* nbSeconds */
        0, /* blockSize */
        0, /* nbWorkers */
        0, /* realTime */
        0, /* additionalParam */
        0, /* ldmFlag */ 
        0, /* ldmMinMatch */
        0, /* ldmHashLog */
        0, /* ldmBuckSizeLog */
        0  /* ldmHashEveryLog */
    };
    return res;
}


/* ********************************************************
*  Bench functions
**********************************************************/
typedef struct {
    const void* srcPtr;
    size_t srcSize;
    void*  cPtr;
    size_t cRoom;
    size_t cSize;
    void*  resPtr;
    size_t resSize;
} blockParam_t;

struct  BMK_timeState_t{
    unsigned nbLoops;
    U64 timeRemaining;
    UTIL_time_t coolTime;
    U64 fastestTime;
};

#undef MIN
#undef MAX
#define MIN(a,b)    ((a) < (b) ? (a) : (b))
#define MAX(a,b)    ((a) > (b) ? (a) : (b))

static void BMK_initCCtx(ZSTD_CCtx* ctx, 
    const void* dictBuffer, size_t dictBufferSize, int cLevel, 
    const ZSTD_compressionParameters* comprParams, const BMK_advancedParams_t* adv) {
    if (adv->nbWorkers==1) {
        ZSTD_CCtx_setParameter(ctx, ZSTD_p_nbWorkers, 0);
    } else {
        ZSTD_CCtx_setParameter(ctx, ZSTD_p_nbWorkers, adv->nbWorkers);
    }
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_compressionLevel, cLevel);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_enableLongDistanceMatching, adv->ldmFlag);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_ldmMinMatch, adv->ldmMinMatch);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_ldmHashLog, adv->ldmHashLog);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_ldmBucketSizeLog, adv->ldmBucketSizeLog);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_ldmHashEveryLog, adv->ldmHashEveryLog);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_windowLog, comprParams->windowLog);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_hashLog, comprParams->hashLog);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_chainLog, comprParams->chainLog);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_searchLog, comprParams->searchLog);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_minMatch, comprParams->searchLength);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_targetLength, comprParams->targetLength);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_compressionStrategy , comprParams->strategy);
    ZSTD_CCtx_loadDictionary(ctx, dictBuffer, dictBufferSize);
}


static void BMK_initDCtx(ZSTD_DCtx* dctx, 
    const void* dictBuffer, size_t dictBufferSize) {
    ZSTD_DCtx_loadDictionary(dctx, dictBuffer, dictBufferSize);
}

typedef struct {
    ZSTD_CCtx* ctx;
    const void* dictBuffer;
    size_t dictBufferSize;
    int cLevel;
    const ZSTD_compressionParameters* comprParams;
    const BMK_advancedParams_t* adv;
} BMK_initCCtxArgs;

static size_t local_initCCtx(void* payload) {
    BMK_initCCtxArgs* ag = (BMK_initCCtxArgs*)payload;
    BMK_initCCtx(ag->ctx, ag->dictBuffer, ag->dictBufferSize, ag->cLevel, ag->comprParams, ag->adv);
    return 0;
}

typedef struct {
    ZSTD_DCtx* dctx;
    const void* dictBuffer;
    size_t dictBufferSize;
} BMK_initDCtxArgs;

static size_t local_initDCtx(void* payload) {
    BMK_initDCtxArgs* ag = (BMK_initDCtxArgs*)payload;
    BMK_initDCtx(ag->dctx, ag->dictBuffer, ag->dictBufferSize);
    return 0;
}

/* additional argument is just the context */
static size_t local_defaultCompress(
    const void* srcBuffer, size_t srcSize, 
    void* dstBuffer, size_t dstSize, 
    void* addArgs) {
    size_t moreToFlush = 1;
    ZSTD_CCtx* ctx = (ZSTD_CCtx*)addArgs;
    ZSTD_inBuffer in;
    ZSTD_outBuffer out;
    in.src = srcBuffer;
    in.size = srcSize;
    in.pos = 0;
    out.dst = dstBuffer;
    out.size = dstSize;
    out.pos = 0;
    while (moreToFlush) {
        if(out.pos == out.size) {
            return (size_t)-ZSTD_error_dstSize_tooSmall;
        }
        moreToFlush = ZSTD_compress_generic(ctx, &out, &in, ZSTD_e_end);
        if (ZSTD_isError(moreToFlush)) {
            return moreToFlush;
        }
    }
    return out.pos;
}

/* additional argument is just the context */
static size_t local_defaultDecompress(
    const void* srcBuffer, size_t srcSize, 
    void* dstBuffer, size_t dstSize, 
    void* addArgs) {
    size_t moreToFlush = 1;
    ZSTD_DCtx* dctx = (ZSTD_DCtx*)addArgs;
    ZSTD_inBuffer in;
    ZSTD_outBuffer out;
    in.src = srcBuffer;
    in.size = srcSize;
    in.pos = 0;
    out.dst = dstBuffer;
    out.size = dstSize;
    out.pos = 0;
    while (moreToFlush) {
        if(out.pos == out.size) {
            return (size_t)-ZSTD_error_dstSize_tooSmall;
        }
        moreToFlush = ZSTD_decompress_generic(dctx,
                            &out, &in);
        if (ZSTD_isError(moreToFlush)) {
            return moreToFlush;
        }
    }
    return out.pos;

}

volatile char g_touched;

/* initFn will be measured once, bench fn will be measured x times */
/* benchFn should return error value or out Size */
/* takes # of blocks and list of size & stuff for each. */
/* only does looping */
/* note time per loop could be zero if interval too short */
BMK_customReturn_t BMK_benchFunction(
    BMK_benchFn_t benchFn, void* benchPayload,
    BMK_initFn_t initFn, void* initPayload,
    size_t blockCount,
    const void* const * const srcBlockBuffers, const size_t* srcBlockSizes,
    void** const dstBlockBuffers, size_t* dstBlockCapacities,
    unsigned nbLoops) {
    size_t srcSize = 0, dstSize = 0, ind = 0;
    U64 totalTime;

    BMK_customReturn_t retval;
    UTIL_time_t clockStart;

    if(!nbLoops) {
        EXM_THROW_ND(1, BMK_customReturn_t, "nbLoops must be nonzero \n");
    }

    for(ind = 0; ind < blockCount; ind++) {
        srcSize += srcBlockSizes[ind];
    }

    {

        unsigned i, j;
        for(i = 0; i < blockCount; i++) {
            for(j = 0; j < srcBlockSizes[i]; j++) {
                g_touched = ((const char*)srcBlockBuffers[i])[j]; /* touch */
            }
        }
        for(i = 0; i < blockCount; i++) {
            memset(dstBlockBuffers[i], 0xE5, dstBlockCapacities[i]);  /* warm up and erase result buffer */
        }

        //UTIL_sleepMilli(5);  /* give processor time to other processes */
        //UTIL_waitForNextTick();

    }

    {      
        unsigned i, j, firstIter = 1;
        clockStart = UTIL_getTime();
        if(initFn != NULL) { initFn(initPayload); }
        for(i = 0; i < nbLoops; i++) {
            for(j = 0; j < blockCount; j++) {
                size_t res = benchFn(srcBlockBuffers[j], srcBlockSizes[j], dstBlockBuffers[j], dstBlockCapacities[j], benchPayload);
                if(ZSTD_isError(res)) {
                    EXM_THROW_ND(2, BMK_customReturn_t, "Function benchmarking failed on block %u of size %u : %s  \n",
                        j, (U32)dstBlockCapacities[j], ZSTD_getErrorName(res));
                }  else if(firstIter) {
                    dstSize += res;
                    //Make compressed blocks continuous
                    if(j != blockCount - 1) {
                        dstBlockBuffers[j+1] = (void*)((char*)dstBlockBuffers[j] + res);
                        dstBlockCapacities[j] = res;
                    }
                } 
            }
            firstIter = 0;
        }
        totalTime = UTIL_clockSpanNano(clockStart);
    }

    retval.error = 0;
    retval.result.nanoSecPerRun = totalTime / nbLoops;
    retval.result.sumOfReturn = dstSize;
    return retval;
} 

#define MINUSABLETIME 500000000ULL /* 0.5 seconds in ns */

void BMK_resetTimeState(BMK_timedFnState_t* r, unsigned nbSeconds) {
    r->nbLoops = 1;
    r->timeRemaining = (U64)nbSeconds * TIMELOOP_NANOSEC;
    r->coolTime = UTIL_getTime();
    r->fastestTime = (U64)(-1LL);
}

BMK_timedFnState_t* BMK_createTimeState(unsigned nbSeconds) {
    BMK_timedFnState_t* r = (BMK_timedFnState_t*)malloc(sizeof(struct BMK_timeState_t));
    BMK_resetTimeState(r, nbSeconds);
    return r;
}

void BMK_freeTimeState(BMK_timedFnState_t* state) {
    free(state);
}

/* make option for dstBlocks to be */
BMK_customTimedReturn_t BMK_benchFunctionTimed(
    BMK_timedFnState_t* cont,
    BMK_benchFn_t benchFn, void* benchPayload,
    BMK_initFn_t initFn, void* initPayload,
    size_t blockCount,
    const void* const* const srcBlockBuffers, const size_t* srcBlockSizes,
    void** const dstBlockBuffers, size_t* dstBlockCapacities) 
{
    U64 fastest = cont->fastestTime;
    int completed = 0;
    BMK_customTimedReturn_t r;
    r.completed = 0;

    while(!r.completed && !completed)
    {
        /* Overheat protection */
        if (UTIL_clockSpanMicro(cont->coolTime) > ACTIVEPERIOD_MICROSEC) {
            DEBUGOUTPUT("\rcooling down ...    \r");
            UTIL_sleep(COOLPERIOD_SEC);
            cont->coolTime = UTIL_getTime();
        }

        r.result = BMK_benchFunction(benchFn, benchPayload, initFn, initPayload,
        blockCount, srcBlockBuffers, srcBlockSizes, dstBlockBuffers, dstBlockCapacities, cont->nbLoops);
        if(r.result.error) { /* completed w/ error */
            r.completed = 1;
            return r;
        }

        {   U64 const loopDuration = r.result.result.nanoSecPerRun * cont->nbLoops;
            r.completed = (cont->timeRemaining <= loopDuration);
            cont->timeRemaining -= loopDuration;
            if (loopDuration > (TIMELOOP_NANOSEC / 100)) { 
                fastest = MIN(fastest, r.result.result.nanoSecPerRun);
                if(loopDuration >= MINUSABLETIME) {
                    r.result.result.nanoSecPerRun = fastest;
                    cont->fastestTime = fastest;
                }
                cont->nbLoops = (U32)(TIMELOOP_NANOSEC / r.result.result.nanoSecPerRun) + 1;
            } else {
                const unsigned multiplier = 2;
                assert(cont->nbLoops < ((unsigned)-1) / multiplier);  /* avoid overflow */
                cont->nbLoops *= multiplier;
            }
            if(loopDuration < MINUSABLETIME) { /* don't report results which have time too low */
                continue;
            }

        }
        completed = 1;
    }
    return r;
}

/* benchMem with no allocation */
static BMK_return_t BMK_benchMemAdvancedNoAlloc(
    const void ** const srcPtrs, size_t* const srcSizes,
    void** const cPtrs, size_t* const cSizes,
    void** const resPtrs, size_t* const resSizes,
    void** resultBufferPtr, void* compressedBuffer,
    const size_t maxCompressedSize,
    BMK_timedFnState_t* timeStateCompress, BMK_timedFnState_t* timeStateDecompress,

    const void* srcBuffer, size_t srcSize,
    const size_t* fileSizes, unsigned nbFiles,
    const int cLevel, const ZSTD_compressionParameters* comprParams,
    const void* dictBuffer, size_t dictBufferSize,
    ZSTD_CCtx* ctx, ZSTD_DCtx* dctx,
    int displayLevel, const char* displayName, const BMK_advancedParams_t* adv) 
{
    size_t const blockSize = ((adv->blockSize>=32 && (adv->mode != BMK_decodeOnly)) ? adv->blockSize : srcSize) + (!srcSize); /* avoid div by 0 */
    BMK_return_t results = { { 0, 0., 0., 0 }, 0 } ;
    size_t const loadedCompressedSize = srcSize;
    size_t cSize = 0;
    double ratio = 0.;
    U32 nbBlocks;

    if(!ctx || !dctx) 
        EXM_THROW(31, BMK_return_t, "error: passed in null context");

    /* init */
    if (strlen(displayName)>17) displayName += strlen(displayName)-17;   /* display last 17 characters */
    if (adv->mode == BMK_decodeOnly) {  /* benchmark only decompression : source must be already compressed */
        const char* srcPtr = (const char*)srcBuffer;
        U64 totalDSize64 = 0;
        U32 fileNb;
        for (fileNb=0; fileNb<nbFiles; fileNb++) {
            U64 const fSize64 = ZSTD_findDecompressedSize(srcPtr, fileSizes[fileNb]);
            if (fSize64==0) EXM_THROW(32, BMK_return_t, "Impossible to determine original size ");
            totalDSize64 += fSize64;
            srcPtr += fileSizes[fileNb];
        }
        {   size_t const decodedSize = (size_t)totalDSize64;
            free(*resultBufferPtr);
            *resultBufferPtr = malloc(decodedSize);
            if (!(*resultBufferPtr)) {
                EXM_THROW(33, BMK_return_t, "not enough memory"); 
            }
            if (totalDSize64 > decodedSize) {
                free(*resultBufferPtr); 
                EXM_THROW(32, BMK_return_t, "original size is too large");   /* size_t overflow */
            }
            cSize = srcSize;
            srcSize = decodedSize;
            ratio = (double)srcSize / (double)cSize;
        }   
    }

    /* Init data blocks  */
    {   const char* srcPtr = (const char*)srcBuffer;
        char* cPtr = (char*)compressedBuffer;
        char* resPtr = (char*)(*resultBufferPtr);
        U32 fileNb;
        for (nbBlocks=0, fileNb=0; fileNb<nbFiles; fileNb++) {
            size_t remaining = fileSizes[fileNb];
            U32 const nbBlocksforThisFile = (adv->mode == BMK_decodeOnly) ? 1 : (U32)((remaining + (blockSize-1)) / blockSize);
            U32 const blockEnd = nbBlocks + nbBlocksforThisFile;
            for ( ; nbBlocks<blockEnd; nbBlocks++) {
                size_t const thisBlockSize = MIN(remaining, blockSize);
                srcPtrs[nbBlocks] = (const void*)srcPtr;
                srcSizes[nbBlocks] = thisBlockSize;
                cPtrs[nbBlocks] = (void*)cPtr;
                cSizes[nbBlocks] = (adv->mode == BMK_decodeOnly) ? thisBlockSize : ZSTD_compressBound(thisBlockSize);
                resPtrs[nbBlocks] = (void*)resPtr;
                resSizes[nbBlocks] = (adv->mode == BMK_decodeOnly) ? (size_t) ZSTD_findDecompressedSize(srcPtr, thisBlockSize) : thisBlockSize;
                srcPtr += thisBlockSize;
                cPtr += cSizes[nbBlocks];
                resPtr += thisBlockSize;
                remaining -= thisBlockSize;
            }   
        }   
    }

    /* warmimg up memory */
    if (adv->mode == BMK_decodeOnly) {
        memcpy(compressedBuffer, srcBuffer, loadedCompressedSize);
    } else {
        RDG_genBuffer(compressedBuffer, maxCompressedSize, 0.10, 0.50, 1);
    }

    /* Bench */
    {   
        U64 const crcOrig = (adv->mode == BMK_decodeOnly) ? 0 : XXH64(srcBuffer, srcSize, 0);
#       define NB_MARKS 4
        const char* const marks[NB_MARKS] = { " |", " /", " =",  "\\" };
        U32 markNb = 0;
        DISPLAYLEVEL(2, "\r%79s\r", "");

        DISPLAYLEVEL(2, "%2s-%-17.17s :%10u ->\r", marks[markNb], displayName, (U32)srcSize);
        {
            BMK_initCCtxArgs cctxprep;
            BMK_initDCtxArgs dctxprep;
            cctxprep.ctx = ctx;
            cctxprep.dictBuffer = dictBuffer;
            cctxprep.dictBufferSize = dictBufferSize;
            cctxprep.cLevel = cLevel;
            cctxprep.comprParams = comprParams;
            cctxprep.adv = adv;
            dctxprep.dctx = dctx;
            dctxprep.dictBuffer = dictBuffer;
            dctxprep.dictBufferSize = dictBufferSize;
            if(adv->loopMode == BMK_timeMode) {
                BMK_customTimedReturn_t intermediateResultCompress;
                BMK_customTimedReturn_t intermediateResultDecompress;
                if(adv->mode == BMK_compressOnly) {
                    intermediateResultCompress.completed = 0;
                    intermediateResultDecompress.completed = 1;
                } else if (adv->mode == BMK_decodeOnly) {
                    intermediateResultCompress.completed = 1;
                    intermediateResultDecompress.completed = 0;
                } else { /* both */
                    intermediateResultCompress.completed = 0;
                    intermediateResultDecompress.completed = 0;
                }
                while(!(intermediateResultCompress.completed && intermediateResultDecompress.completed)) {
                    if(!intermediateResultCompress.completed) { 
                        intermediateResultCompress = BMK_benchFunctionTimed(timeStateCompress, &local_defaultCompress, (void*)ctx, &local_initCCtx, (void*)&cctxprep,
                        nbBlocks, srcPtrs, srcSizes, cPtrs, cSizes);
                        if(intermediateResultCompress.result.error) {
                            results.error = intermediateResultCompress.result.error;
                            return results;
                        }
                        ratio = (double)(srcSize / intermediateResultCompress.result.result.sumOfReturn);
                        {   
                            int const ratioAccuracy = (ratio < 10.) ? 3 : 2;
                            double const compressionSpeed = ((double)srcSize / intermediateResultCompress.result.result.nanoSecPerRun) * 1000;
                            int const cSpeedAccuracy = (compressionSpeed < 10.) ? 2 : 1;
                            results.result.cSpeed = compressionSpeed * 1000000;
                            cSize = intermediateResultCompress.result.result.sumOfReturn;
                            results.result.cSize = cSize;
                            ratio = (double)srcSize / results.result.cSize;
                            markNb = (markNb+1) % NB_MARKS;
                            DISPLAYLEVEL(2, "%2s-%-17.17s :%10u ->%10u (%5.*f),%6.*f MB/s\r",
                                    marks[markNb], displayName, (U32)srcSize, (U32)results.result.cSize,
                                    ratioAccuracy, ratio,
                                    cSpeedAccuracy, compressionSpeed);
                        }
                    }

                    if(!intermediateResultDecompress.completed) {
                        intermediateResultDecompress = BMK_benchFunctionTimed(timeStateDecompress, &local_defaultDecompress, (void*)(dctx), &local_initDCtx, (void*)&dctxprep,
                        nbBlocks, (const void* const*)cPtrs, cSizes, resPtrs, resSizes);
                        if(intermediateResultDecompress.result.error) {
                            results.error = intermediateResultDecompress.result.error;
                            return results;
                        }
      
                        {   
                            int const ratioAccuracy = (ratio < 10.) ? 3 : 2;
                            double const compressionSpeed = results.result.cSpeed / 1000000;
                            int const cSpeedAccuracy = (compressionSpeed < 10.) ? 2 : 1;
                            double const decompressionSpeed = ((double)srcSize / intermediateResultDecompress.result.result.nanoSecPerRun) * 1000;
                            results.result.dSpeed = decompressionSpeed * 1000000;
                            markNb = (markNb+1) % NB_MARKS;
                            DISPLAYLEVEL(2, "%2s-%-17.17s :%10u ->%10u (%5.*f),%6.*f MB/s ,%6.1f MB/s \r",
                                    marks[markNb], displayName, (U32)srcSize, (U32)results.result.cSize,
                                    ratioAccuracy, ratio,
                                    cSpeedAccuracy, compressionSpeed,
                                    decompressionSpeed);
                        }
                    }
                }

            } else { //iterMode;
                if(adv->mode != BMK_decodeOnly) {

                    BMK_customReturn_t compressionResults = BMK_benchFunction(&local_defaultCompress, (void*)ctx, &local_initCCtx, (void*)&cctxprep,
                        nbBlocks, srcPtrs, srcSizes, cPtrs, cSizes, adv->nbSeconds); 
                    if(compressionResults.error) {
                        results.error = compressionResults.error;
                        return results;
                    }
                    if(compressionResults.result.nanoSecPerRun == 0) {
                        results.result.cSpeed = 0;
                    } else {
                        results.result.cSpeed = (double)srcSize / compressionResults.result.nanoSecPerRun * TIMELOOP_NANOSEC;
                    }
                    results.result.cSize = compressionResults.result.sumOfReturn;
                    {   
                        int const ratioAccuracy = (ratio < 10.) ? 3 : 2;
                        double const compressionSpeed = results.result.cSpeed / 1000000;
                        int const cSpeedAccuracy = (compressionSpeed < 10.) ? 2 : 1;
                        ratio = (double)srcSize / results.result.cSize;
                        markNb = (markNb+1) % NB_MARKS;
                        DISPLAYLEVEL(2, "%2s-%-17.17s :%10u ->%10u (%5.*f),%6.*f MB/s\r",
                                marks[markNb], displayName, (U32)srcSize, (U32)results.result.cSize,
                                ratioAccuracy, ratio,
                                cSpeedAccuracy, compressionSpeed);
                    }
                }
                if(adv->mode != BMK_compressOnly) {
                    BMK_customReturn_t decompressionResults = BMK_benchFunction(
                        &local_defaultDecompress, (void*)(dctx),
                        &local_initDCtx, (void*)&dctxprep, nbBlocks,
                        (const void* const*)cPtrs, cSizes, resPtrs, resSizes, 
                        adv->nbSeconds);
                    if(decompressionResults.error) {
                        results.error = decompressionResults.error;
                        return results;
                    }
                    if(decompressionResults.result.nanoSecPerRun == 0) {
                        results.result.dSpeed = 0;
                    } else {
                        results.result.dSpeed = (double)srcSize / decompressionResults.result.nanoSecPerRun * TIMELOOP_NANOSEC;
                    }
                    {   int const ratioAccuracy = (ratio < 10.) ? 3 : 2;
                        double const compressionSpeed = results.result.cSpeed / 1000000;
                        int const cSpeedAccuracy = (compressionSpeed < 10.) ? 2 : 1;
                        double const decompressionSpeed = ((double)srcSize / decompressionResults.result.nanoSecPerRun) * 1000;
                        results.result.dSpeed = decompressionSpeed * 1000000;
                        markNb = (markNb+1) % NB_MARKS;
                        DISPLAYLEVEL(2, "%2s-%-17.17s :%10u ->%10u (%5.*f),%6.*f MB/s ,%6.1f MB/s \r",
                                marks[markNb], displayName, (U32)srcSize, (U32)results.result.cSize,
                                ratioAccuracy, ratio,
                                cSpeedAccuracy, compressionSpeed,
                                decompressionSpeed);
                    }
                }
            }
        }

        /* CRC Checking */
        {   void* resultBuffer = *resultBufferPtr;
            U64 const crcCheck = XXH64(resultBuffer, srcSize, 0);
            /* adv->mode == 0 -> compress + decompress */
            if ((adv->mode == BMK_both) && (crcOrig!=crcCheck)) {
                size_t u;
                DISPLAY("!!! WARNING !!! %14s : Invalid Checksum : %x != %x   \n", displayName, (unsigned)crcOrig, (unsigned)crcCheck);
                for (u=0; u<srcSize; u++) {
                    if (((const BYTE*)srcBuffer)[u] != ((const BYTE*)resultBuffer)[u]) {
                        U32 segNb, bNb, pos;
                        size_t bacc = 0;
                        DISPLAY("Decoding error at pos %u ", (U32)u);
                        for (segNb = 0; segNb < nbBlocks; segNb++) {
                            if (bacc + srcSizes[segNb] > u) break;
                            bacc += srcSizes[segNb];
                        }
                        pos = (U32)(u - bacc);
                        bNb = pos / (128 KB);
                        DISPLAY("(sample %u, block %u, pos %u) \n", segNb, bNb, pos);
                        if (u>5) {
                            int n;
                            DISPLAY("origin: ");
                            for (n=-5; n<0; n++) DISPLAY("%02X ", ((const BYTE*)srcBuffer)[u+n]);
                            DISPLAY(" :%02X:  ", ((const BYTE*)srcBuffer)[u]);
                            for (n=1; n<3; n++) DISPLAY("%02X ", ((const BYTE*)srcBuffer)[u+n]);
                            DISPLAY(" \n");
                            DISPLAY("decode: ");
                            for (n=-5; n<0; n++) DISPLAY("%02X ", ((const BYTE*)resultBuffer)[u+n]);
                            DISPLAY(" :%02X:  ", ((const BYTE*)resultBuffer)[u]);
                            for (n=1; n<3; n++) DISPLAY("%02X ", ((const BYTE*)resultBuffer)[u+n]);
                            DISPLAY(" \n");
                        }
                        break;
                    }
                    if (u==srcSize-1) {  /* should never happen */
                        DISPLAY("no difference detected\n");
                    }   
                }
            }   
        }   /* CRC Checking */

    if (displayLevel == 1) {   /* hidden display mode -q, used by python speed benchmark */
        double const cSpeed = results.result.cSpeed / 1000000;
        double const dSpeed = results.result.dSpeed / 1000000;
        if (adv->additionalParam) {
            DISPLAY("-%-3i%11i (%5.3f) %6.2f MB/s %6.1f MB/s  %s (param=%d)\n", cLevel, (int)cSize, ratio, cSpeed, dSpeed, displayName, adv->additionalParam);
        } else {
            DISPLAY("-%-3i%11i (%5.3f) %6.2f MB/s %6.1f MB/s  %s\n", cLevel, (int)cSize, ratio, cSpeed, dSpeed, displayName);
        }
    }
    DISPLAYLEVEL(2, "%2i#\n", cLevel);
    }   /* Bench */
    results.result.cMem = (1 << (comprParams->windowLog)) + ZSTD_sizeof_CCtx(ctx);
    results.error = 0;
    return results;
}

BMK_return_t BMK_benchMemAdvanced(const void* srcBuffer, size_t srcSize,
                        void* dstBuffer, size_t dstCapacity, 
                        const size_t* fileSizes, unsigned nbFiles,
                        const int cLevel, const ZSTD_compressionParameters* comprParams,
                        const void* dictBuffer, size_t dictBufferSize,
                        ZSTD_CCtx* ctx, ZSTD_DCtx* dctx,
                        int displayLevel, const char* displayName, const BMK_advancedParams_t* adv)

{
    size_t const blockSize = ((adv->blockSize>=32 && (adv->mode != BMK_decodeOnly)) ? adv->blockSize : srcSize) + (!srcSize) /* avoid div by 0 */ ;
    U32 const maxNbBlocks = (U32) ((srcSize + (blockSize-1)) / blockSize) + nbFiles;

    /* these are the blockTable parameters, just split up */
    const void ** const srcPtrs = (const void** const)malloc(maxNbBlocks * sizeof(void*));
    size_t* const srcSizes = (size_t* const)malloc(maxNbBlocks * sizeof(size_t));

    void ** const cPtrs = (void** const)malloc(maxNbBlocks * sizeof(void*));
    size_t* const cSizes = (size_t* const)malloc(maxNbBlocks * sizeof(size_t));

    void ** const resPtrs = (void** const)malloc(maxNbBlocks * sizeof(void*));
    size_t* const resSizes = (size_t* const)malloc(maxNbBlocks * sizeof(size_t));

    BMK_timedFnState_t* timeStateCompress = BMK_createTimeState(adv->nbSeconds);
    BMK_timedFnState_t* timeStateDecompress = BMK_createTimeState(adv->nbSeconds);

    void* compressedBuffer;
    const size_t maxCompressedSize = dstCapacity ? dstCapacity : ZSTD_compressBound(srcSize) + (maxNbBlocks * 1024);
    void* resultBuffer = malloc(srcSize);


    BMK_return_t results;
    int allocationincomplete;

    if(!dstCapacity) {
        compressedBuffer = malloc(maxCompressedSize);
    } else {
        compressedBuffer = dstBuffer;
    }

    allocationincomplete = !compressedBuffer || !resultBuffer ||  
        !srcPtrs || !srcSizes || !cPtrs || !cSizes || !resPtrs || !resSizes;

    if (!allocationincomplete) {
        results = BMK_benchMemAdvancedNoAlloc(srcPtrs, srcSizes, cPtrs, cSizes,
            resPtrs, resSizes, &resultBuffer, compressedBuffer, maxCompressedSize, timeStateCompress, timeStateDecompress,
            srcBuffer, srcSize, fileSizes, nbFiles, cLevel, comprParams,
            dictBuffer, dictBufferSize, ctx, dctx, displayLevel, displayName, adv);
    }


    
    /* clean up */
    BMK_freeTimeState(timeStateCompress);
    BMK_freeTimeState(timeStateDecompress);
    if(!dstCapacity) { /* only free if not given */
        free(compressedBuffer);
    }
    free(resultBuffer);

    free((void*)srcPtrs); 
    free(srcSizes); 
    free(cPtrs); 
    free(cSizes);
    free(resPtrs);
    free(resSizes);

    if(allocationincomplete) {
        EXM_THROW(31, BMK_return_t, "allocation error : not enough memory");
    }
    return results;
}

BMK_return_t BMK_benchMem(const void* srcBuffer, size_t srcSize,
                        const size_t* fileSizes, unsigned nbFiles,
                        const int cLevel, const ZSTD_compressionParameters* comprParams,
                        const void* dictBuffer, size_t dictBufferSize,
                        ZSTD_CCtx* ctx, ZSTD_DCtx* dctx,
                        int displayLevel, const char* displayName) {

    const BMK_advancedParams_t adv = BMK_initAdvancedParams();
    return BMK_benchMemAdvanced(srcBuffer, srcSize,
                                NULL, 0,
                                fileSizes, nbFiles,
                                cLevel, comprParams,
                                dictBuffer, dictBufferSize,
                                ctx, dctx,
                                displayLevel, displayName, &adv);
}

static BMK_return_t BMK_benchMemCtxless(const void* srcBuffer, size_t srcSize,
                        const size_t* fileSizes, unsigned nbFiles, 
                        int cLevel, const ZSTD_compressionParameters* const comprParams,
                        const void* dictBuffer, size_t dictBufferSize,
                        int displayLevel, const char* displayName, 
                        const BMK_advancedParams_t* const adv) 
{
    BMK_return_t res;
    ZSTD_CCtx* ctx = ZSTD_createCCtx();
    ZSTD_DCtx* dctx = ZSTD_createDCtx();
    if(ctx == NULL || dctx == NULL) {
        EXM_THROW(12, BMK_return_t, "not enough memory for contexts");
    }
    res = BMK_benchMemAdvanced(srcBuffer, srcSize, 
                NULL, 0, 
                fileSizes, nbFiles, 
                cLevel, comprParams, 
                dictBuffer, dictBufferSize, 
                ctx, dctx, 
                displayLevel, displayName, adv);
    ZSTD_freeCCtx(ctx);
    ZSTD_freeDCtx(dctx);
    return res;
}

static size_t BMK_findMaxMem(U64 requiredMem)
{
    size_t const step = 64 MB;
    BYTE* testmem = NULL;

    requiredMem = (((requiredMem >> 26) + 1) << 26);
    requiredMem += step;
    if (requiredMem > maxMemory) requiredMem = maxMemory;

    do {
        testmem = (BYTE*)malloc((size_t)requiredMem);
        requiredMem -= step;
    } while (!testmem);

    free(testmem);
    return (size_t)(requiredMem);
}

static BMK_return_t BMK_benchCLevel(const void* srcBuffer, size_t benchedSize,
                            const size_t* fileSizes, unsigned nbFiles,
                            const int cLevel, const ZSTD_compressionParameters* comprParams,
                            const void* dictBuffer, size_t dictBufferSize,
                            int displayLevel, const char* displayName,
                            BMK_advancedParams_t const * const adv)
{
    BMK_return_t res;

    const char* pch = strrchr(displayName, '\\'); /* Windows */

    if (!pch) pch = strrchr(displayName, '/'); /* Linux */
    if (pch) displayName = pch+1;

    if (adv->realTime) {
        DISPLAYLEVEL(2, "Note : switching to real-time priority \n");
        SET_REALTIME_PRIORITY;
    }

    if (displayLevel == 1 && !adv->additionalParam)
        DISPLAY("bench %s %s: input %u bytes, %u seconds, %u KB blocks\n", ZSTD_VERSION_STRING, ZSTD_GIT_COMMIT_STRING, (U32)benchedSize, adv->nbSeconds, (U32)(adv->blockSize>>10));

    res = BMK_benchMemCtxless(srcBuffer, benchedSize,
            fileSizes, nbFiles, 
            cLevel, comprParams, 
            dictBuffer, dictBufferSize, 
            displayLevel, displayName, 
            adv);

    return res;
}


/*! BMK_loadFiles() :
 *  Loads `buffer` with content of files listed within `fileNamesTable`.
 *  At most, fills `buffer` entirely. */
static int BMK_loadFiles(void* buffer, size_t bufferSize,
                          size_t* fileSizes, const char* const * const fileNamesTable, 
                          unsigned nbFiles, int displayLevel)
{
    size_t pos = 0, totalSize = 0;
    unsigned n;
    for (n=0; n<nbFiles; n++) {
        FILE* f;
        U64 fileSize = UTIL_getFileSize(fileNamesTable[n]);
        if (UTIL_isDirectory(fileNamesTable[n])) {
            DISPLAYLEVEL(2, "Ignoring %s directory...       \n", fileNamesTable[n]);
            fileSizes[n] = 0;
            continue;
        }
        if (fileSize == UTIL_FILESIZE_UNKNOWN) {
            DISPLAYLEVEL(2, "Cannot evaluate size of %s, ignoring ... \n", fileNamesTable[n]);
            fileSizes[n] = 0;
            continue;
        }
        f = fopen(fileNamesTable[n], "rb");
        if (f==NULL) EXM_THROW_INT(10, "impossible to open file %s", fileNamesTable[n]);
        DISPLAYUPDATE(2, "Loading %s...       \r", fileNamesTable[n]);
        if (fileSize > bufferSize-pos) fileSize = bufferSize-pos, nbFiles=n;   /* buffer too small - stop after this file */
        { size_t const readSize = fread(((char*)buffer)+pos, 1, (size_t)fileSize, f);
          if (readSize != (size_t)fileSize) EXM_THROW_INT(11, "could not read %s", fileNamesTable[n]);
          pos += readSize; }
        fileSizes[n] = (size_t)fileSize;
        totalSize += (size_t)fileSize;
        fclose(f);
    }

    if (totalSize == 0) EXM_THROW_INT(12, "no data to bench");
    return 0;
}

BMK_return_t BMK_benchFilesAdvanced(const char* const * const fileNamesTable, unsigned const nbFiles,
                               const char* const dictFileName, int const cLevel, 
                               const ZSTD_compressionParameters* const compressionParams, 
                               int displayLevel, const BMK_advancedParams_t * const adv)
{
    void* srcBuffer = NULL;
    size_t benchedSize;
    void* dictBuffer = NULL;
    size_t dictBufferSize = 0;
    size_t* fileSizes = NULL;
    BMK_return_t res;
    U64 const totalSizeToLoad = UTIL_getTotalFileSize(fileNamesTable, nbFiles);

    if(!nbFiles) {
        EXM_THROW(14, BMK_return_t, "No Files to Benchmark");
    }

    if (cLevel > ZSTD_maxCLevel()) {
        EXM_THROW(15, BMK_return_t, "Invalid Compression Level");
    }

    fileSizes = (size_t*)calloc(nbFiles, sizeof(size_t));
    if (!fileSizes) EXM_THROW(12, BMK_return_t, "not enough memory for fileSizes");
    /* Load dictionary */
    if (dictFileName != NULL) {
        U64 const dictFileSize = UTIL_getFileSize(dictFileName);
        if (dictFileSize > 64 MB) {
            free(fileSizes);
            EXM_THROW(10, BMK_return_t, "dictionary file %s too large", dictFileName);
        }
        dictBufferSize = (size_t)dictFileSize;
        dictBuffer = malloc(dictBufferSize);
        if (dictBuffer==NULL) {
            free(fileSizes);
            EXM_THROW(11, BMK_return_t, "not enough memory for dictionary (%u bytes)",
                            (U32)dictBufferSize);
        }
        {
            int errorCode = BMK_loadFiles(dictBuffer, dictBufferSize, fileSizes, &dictFileName, 1, displayLevel);
            if(errorCode) {
                res.error = errorCode;
                goto _cleanUp;
            }
        }
    }

    /* Memory allocation & restrictions */
    benchedSize = BMK_findMaxMem(totalSizeToLoad * 3) / 3;
    if ((U64)benchedSize > totalSizeToLoad) benchedSize = (size_t)totalSizeToLoad;
    if (benchedSize < totalSizeToLoad)
        DISPLAY("Not enough memory; testing %u MB only...\n", (U32)(benchedSize >> 20));
    srcBuffer = malloc(benchedSize);
    if (!srcBuffer) {
        free(dictBuffer);
        free(fileSizes);
        EXM_THROW(12, BMK_return_t, "not enough memory");
    }

    /* Load input buffer */
    {
        int errorCode = BMK_loadFiles(srcBuffer, benchedSize, fileSizes, fileNamesTable, nbFiles, displayLevel);
        if(errorCode) {
            res.error = errorCode;
            goto _cleanUp;
        }
    }
    /* Bench */
    {
        char mfName[20] = {0};
        snprintf (mfName, sizeof(mfName), " %u files", nbFiles);
        {   
            const char* const displayName = (nbFiles > 1) ? mfName : fileNamesTable[0];
            res = BMK_benchCLevel(srcBuffer, benchedSize, 
                            fileSizes, nbFiles, 
                            cLevel, compressionParams,
                            dictBuffer, dictBufferSize, 
                            displayLevel, displayName, 
                            adv);
    }   }

_cleanUp:
    free(srcBuffer);
    free(dictBuffer);
    free(fileSizes);
    return res;
}


BMK_return_t BMK_syntheticTest(int cLevel, double compressibility,
                              const ZSTD_compressionParameters* compressionParams,
                              int displayLevel, const BMK_advancedParams_t * const adv)
{
    char name[20] = {0};
    size_t benchedSize = 10000000;
    void* srcBuffer;
    BMK_return_t res;

    if (cLevel > ZSTD_maxCLevel()) {
        EXM_THROW(15, BMK_return_t, "Invalid Compression Level");
    }

    /* Memory allocation */
    srcBuffer = malloc(benchedSize);
    if (!srcBuffer) EXM_THROW(21, BMK_return_t, "not enough memory");

    /* Fill input buffer */
    RDG_genBuffer(srcBuffer, benchedSize, compressibility, 0.0, 0);

    /* Bench */
    snprintf (name, sizeof(name), "Synthetic %2u%%", (unsigned)(compressibility*100));
    res = BMK_benchCLevel(srcBuffer, benchedSize, 
                    &benchedSize, 1, 
                    cLevel, compressionParams, 
                    NULL, 0, 
                    displayLevel, name, adv);

    /* clean up */
    free(srcBuffer);

    return res;
}

BMK_return_t BMK_benchFiles(const char* const * const fileNamesTable, unsigned const nbFiles,
                   const char* const dictFileName, 
                   int const cLevel, const ZSTD_compressionParameters* const compressionParams, 
                   int displayLevel) {
    const BMK_advancedParams_t adv = BMK_initAdvancedParams();
    return BMK_benchFilesAdvanced(fileNamesTable, nbFiles, dictFileName, cLevel, compressionParams, displayLevel, &adv);
}
