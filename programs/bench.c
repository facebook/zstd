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

//TODO: remove this gv as well
//Only used in Synthetic test. Separate?
static U32 g_compressibilityDefault = 50;

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

/* *************************************
*  Benchmark Parameters
***************************************/

#define BMK_LDM_PARAM_NOTSET 9999

BMK_advancedParams_t BMK_defaultAdvancedParams(void) { 
    BMK_advancedParams_t res = { 
        0, /* mode */
        0, /* nbCycles */
        BMK_TIMETEST_DEFAULT_S, /* nbSeconds */
        0, /* blockSize */
        0, /* nbWorkers */
        0, /* realTime */
        1, /* separateFiles */
        0, /* additionalParam */
        0, /* ldmFlag */ 
        0, /* ldmMinMatch */
        0, /* ldmHashLog */
        BMK_LDM_PARAM_NOTSET, /* ldmBuckSizeLog */
        BMK_LDM_PARAM_NOTSET  /* ldmHashEveryLog */
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
    if (adv->ldmBucketSizeLog != BMK_LDM_PARAM_NOTSET) {
      ZSTD_CCtx_setParameter(ctx, ZSTD_p_ldmBucketSizeLog, adv->ldmBucketSizeLog);
    }
    if (adv->ldmHashEveryLog != BMK_LDM_PARAM_NOTSET) {
      ZSTD_CCtx_setParameter(ctx, ZSTD_p_ldmHashEveryLog, adv->ldmHashEveryLog);
    }
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_windowLog, comprParams->windowLog);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_hashLog, comprParams->hashLog);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_chainLog, comprParams->chainLog);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_searchLog, comprParams->searchLog);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_minMatch, comprParams->searchLength);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_targetLength, comprParams->targetLength);
    ZSTD_CCtx_setParameter(ctx, ZSTD_p_compressionStrategy, comprParams->strategy);
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
        moreToFlush = ZSTD_compress_generic(ctx, &out, &in, ZSTD_e_end);
        if (ZSTD_isError(moreToFlush)) {
            return moreToFlush;
        }
    }
    return out.pos;
}

/* addiional argument is just the context */
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
        moreToFlush = ZSTD_decompress_generic(dctx,
                            &out, &in);
        if (ZSTD_isError(moreToFlush)) {
            return moreToFlush;
        }
    }
    return out.pos;

}

//ignore above for error stuff, return type still undecided

/* mode 0 : iter = # seconds, else iter = # cycles */
/* initFn will be measured once, bench fn will be measured x times */
/* benchFn should return error value or out Size */
//problem : how to get cSize this way for ratio? 
//also possible fastest rounds down to 0 if 0 < loopDuration < nbLoops (that would mean <1ns / op though)
/* takes # of blocks and list of size & stuff for each. */
BMK_customReturn_t BMK_benchCustom(
    const char* functionName, size_t blockCount,
    const void* const * const srcBuffers, size_t* srcSizes,
    void* const * const dstBuffers, size_t* dstSizes,
    size_t (*initFn)(void*), size_t (*benchFn)(const void*, size_t, void*, size_t, void*), 
    void* initPayload, void* benchPayload,
    unsigned mode, unsigned iter,
    int displayLevel) {
    size_t srcSize = 0, dstSize = 0, ind = 0;
    unsigned toAdd = 1;

    BMK_customReturn_t retval;
    U64 totalTime = 0, fastest = (U64)(-1LL);
    UTIL_time_t clockStart;

    {
        unsigned i;
        for(i = 0; i < blockCount; i++) {
            memset(dstBuffers[i], 0xE5, dstSizes[i]);  /* warm up and erase result buffer */
        }

        UTIL_sleepMilli(5);  /* give processor time to other processes */
        UTIL_waitForNextTick();
    }

    /* display last 17 char's of functionName*/
    if (strlen(functionName)>17) functionName += strlen(functionName)-17;
    if(!iter) {
        if(mode) {
            EXM_THROW(1, BMK_customReturn_t, "nbSeconds must be nonzero \n");
        } else {
            EXM_THROW(1, BMK_customReturn_t, "nbLoops must be nonzero \n");
        }

    }

    for(ind = 0; ind < blockCount; ind++) {
        srcSize += srcSizes[ind];
    }

    //change to switch if more modes? 
    if(!mode) {
        int completed = 0;
        U64 const maxTime = (iter * TIMELOOP_NANOSEC) + 1;
        unsigned nbLoops = 1;
        UTIL_time_t coolTime = UTIL_getTime();
        while(!completed) {
            unsigned i, j;
            /* Overheat protection */
            if (UTIL_clockSpanMicro(coolTime) > ACTIVEPERIOD_MICROSEC) {
                DISPLAYLEVEL(2, "\rcooling down ...    \r");
                UTIL_sleep(COOLPERIOD_SEC);
                coolTime = UTIL_getTime();
            }
            
            for(i = 0; i < blockCount; i++) {
                memset(dstBuffers[i], 0xD6, dstSizes[i]);  /* warm up and erase result buffer */
            }

            clockStart = UTIL_getTime();
            (*initFn)(initPayload);

            for(i = 0; i < nbLoops; i++) {
                for(j = 0; j < blockCount; j++) {
                    size_t res = (*benchFn)(srcBuffers[j], srcSizes[j], dstBuffers[j], dstSizes[j], benchPayload);
                    if(ZSTD_isError(res)) {
                        EXM_THROW(2, BMK_customReturn_t, "%s() failed on block %u of size %u : %s  \n",
                            functionName, j, (U32)dstSizes[j], ZSTD_getErrorName(res));
                    } else if (toAdd) {
                        dstSize += res;
                    }
                }
                toAdd = 0;
            }
            {   U64 const loopDuration = UTIL_clockSpanNano(clockStart);
                if (loopDuration > 0) {
                    fastest = MIN(fastest, loopDuration / nbLoops);
                    nbLoops = (U32)(TIMELOOP_NANOSEC / fastest) + 1;
                } else {
                    assert(nbLoops < 40000000);  /* avoid overflow */
                    nbLoops *= 100;
                }
                totalTime += loopDuration;
                completed = (totalTime >= maxTime);
            } 
        }
    } else {
        unsigned i, j;
        clockStart = UTIL_getTime();
        for(i = 0; i < iter; i++) {
            for(j = 0; j < blockCount; j++) {
                size_t res = (*benchFn)(srcBuffers[j], srcSizes[j], dstBuffers[j], dstSizes[j], benchPayload);
                if(ZSTD_isError(res)) {
                    EXM_THROW(2, BMK_customReturn_t, "%s() failed on block %u of size %u : %s  \n",
                        functionName, j, (U32)dstSizes[j], ZSTD_getErrorName(res));
                } else if(toAdd) {
                    dstSize += res;
                }
            }
            toAdd = 0;
        }
        totalTime = UTIL_clockSpanNano(clockStart);
        if(!totalTime) {
            EXM_THROW(3, BMK_customReturn_t, "Cycle count (%u) too short to measure \n", iter);
        } else {
            fastest = totalTime / iter;
        }
    }
    retval.error = 0;
    retval.result.time = fastest;
    retval.result.size = dstSize;
    return retval;
}

BMK_return_t BMK_benchMemAdvanced(const void* srcBuffer, size_t srcSize,
                        const size_t* fileSizes, unsigned nbFiles,
                        const int cLevel, const ZSTD_compressionParameters* comprParams,
                        const void* dictBuffer, size_t dictBufferSize,
                        ZSTD_CCtx* ctx, ZSTD_DCtx* dctx,
                        int displayLevel, const char* displayName, const BMK_advancedParams_t* adv)

{
    size_t const blockSize = ((adv->blockSize>=32 && (adv->mode != BMK_DECODE_ONLY)) ? adv->blockSize : srcSize) + (!srcSize) /* avoid div by 0 */ ;
    U32 const maxNbBlocks = (U32) ((srcSize + (blockSize-1)) / blockSize) + nbFiles;

    /* these are the blockTable parameters, just split up */
    const void ** const srcPtrs = malloc(maxNbBlocks * sizeof(void*));
    size_t* const srcSizes = malloc(maxNbBlocks * sizeof(size_t));

    void ** const cPtrs = malloc(maxNbBlocks * sizeof(void*));
    size_t* const cSizes = malloc(maxNbBlocks * sizeof(size_t));

    void ** const resPtrs = malloc(maxNbBlocks * sizeof(void*));
    size_t* const resSizes = malloc(maxNbBlocks * sizeof(size_t));

    const size_t maxCompressedSize = ZSTD_compressBound(srcSize) + (maxNbBlocks * 1024);   /* add some room for safety */
    void* compressedBuffer = malloc(maxCompressedSize);
    void* resultBuffer = malloc(srcSize);
    
    BMK_return_t results;

    size_t const loadedCompressedSize = srcSize;
    size_t cSize = 0;
    double ratio = 0.;
    U32 nbBlocks;

    /* checks */
    if (!compressedBuffer || !resultBuffer ||  
        !srcPtrs || !srcSizes || !cPtrs || !cSizes || !resPtrs || !resSizes)
        EXM_THROW(31, BMK_return_t, "allocation error : not enough memory");

    if(!ctx || !dctx) 
        EXM_THROW(31, BMK_return_t, "error: passed in null context");

    /* init */
    if (strlen(displayName)>17) displayName += strlen(displayName)-17;   /* display last 17 characters */
    if (adv->mode == BMK_DECODE_ONLY) {  /* benchmark only decompression : source must be already compressed */
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
            if (totalDSize64 > decodedSize) EXM_THROW(32, BMK_return_t, "original size is too large");   /* size_t overflow */
            free(resultBuffer);
            resultBuffer = malloc(decodedSize);
            if (!resultBuffer) EXM_THROW(33, BMK_return_t, "not enough memory");
            cSize = srcSize;
            srcSize = decodedSize;
            ratio = (double)srcSize / (double)cSize;
        }   
    }

    /* Init data blocks  */
    {   const char* srcPtr = (const char*)srcBuffer;
        char* cPtr = (char*)compressedBuffer;
        char* resPtr = (char*)resultBuffer;
        U32 fileNb;
        for (nbBlocks=0, fileNb=0; fileNb<nbFiles; fileNb++) {
            size_t remaining = fileSizes[fileNb];
            U32 const nbBlocksforThisFile = (adv->mode == BMK_DECODE_ONLY) ? 1 : (U32)((remaining + (blockSize-1)) / blockSize);
            U32 const blockEnd = nbBlocks + nbBlocksforThisFile;
            for ( ; nbBlocks<blockEnd; nbBlocks++) {
                size_t const thisBlockSize = MIN(remaining, blockSize);
                srcPtrs[nbBlocks] = (const void*)srcPtr;
                srcSizes[nbBlocks] = thisBlockSize;
                cPtrs[nbBlocks] = (void*)cPtr;
                cSizes[nbBlocks] = (adv->mode == BMK_DECODE_ONLY) ? thisBlockSize : ZSTD_compressBound(thisBlockSize);
                //blockTable[nbBlocks].cSize = blockTable[nbBlocks].cRoom;
                resPtrs[nbBlocks] = (void*)resPtr;
                resSizes[nbBlocks] = (adv->mode == BMK_DECODE_ONLY) ? (size_t) ZSTD_findDecompressedSize(srcPtr, thisBlockSize) : thisBlockSize;
                srcPtr += thisBlockSize;
                cPtr += cSizes[nbBlocks]; //blockTable[nbBlocks].cRoom;
                resPtr += thisBlockSize;
                remaining -= thisBlockSize;
            }   
        }   
    }

    /* warmimg up memory */
    if (adv->mode == BMK_DECODE_ONLY) {
        memcpy(compressedBuffer, srcBuffer, loadedCompressedSize);
    } else {
        RDG_genBuffer(compressedBuffer, maxCompressedSize, 0.10, 0.50, 1);
    }

    /* Bench */

    //TODO: Make sure w/o new loop decode_only code isn't run
    //TODO: Support nbLoops and nbSeconds
    {   
        U64 const crcOrig = (adv->mode == BMK_DECODE_ONLY) ? 0 : XXH64(srcBuffer, srcSize, 0);
#       define NB_MARKS 4
        const char* const marks[NB_MARKS] = { " |", " /", " =",  "\\" };
        U32 markNb = 0;
        DISPLAYLEVEL(2, "\r%79s\r", "");

        if (adv->mode != BMK_DECODE_ONLY) {
            BMK_initCCtxArgs cctxprep = { ctx, dictBuffer, dictBufferSize, cLevel, comprParams, adv };
            BMK_customReturn_t compressionResults;
            /* Compression */
            DISPLAYLEVEL(2, "%2s-%-17.17s :%10u ->\r", marks[markNb], displayName, (U32)srcSize);
            compressionResults = BMK_benchCustom("ZSTD_compress_generic", nbBlocks, 
                srcPtrs, srcSizes, cPtrs, cSizes, 
                &local_initCCtx, &local_defaultCompress, 
                (void*)&cctxprep, (void*)(ctx),
                adv->loopMode, adv->nbSeconds, displayLevel);

            if(compressionResults.error) {
                results.error = compressionResults.error;
                return results;
            }

            results.result.cSize = compressionResults.result.size;
            ratio = (double)srcSize / (double)results.result.cSize;
            markNb = (markNb+1) % NB_MARKS;
            {   
                int const ratioAccuracy = (ratio < 10.) ? 3 : 2;
                double const compressionSpeed = ((double)srcSize / compressionResults.result.time) * 1000;
                int const cSpeedAccuracy = (compressionSpeed < 10.) ? 2 : 1;
                results.result.cSpeed = compressionSpeed * 1000000;
                DISPLAYLEVEL(2, "%2s-%-17.17s :%10u ->%10u (%5.*f),%6.*f MB/s\r",
                        marks[markNb], displayName, (U32)srcSize, (U32)results.result.cSize,
                        ratioAccuracy, ratio,
                        cSpeedAccuracy, compressionSpeed);
            }
        }  /* if (adv->mode != BMK_DECODE_ONLY) */
        {
            BMK_initDCtxArgs dctxprep = { dctx, dictBuffer, dictBufferSize };
            BMK_customReturn_t decompressionResults;

            decompressionResults = BMK_benchCustom("ZSTD_decompress_generic", nbBlocks, 
                (const void * const *)cPtrs, cSizes, resPtrs, resSizes, 
                &local_initDCtx, &local_defaultDecompress, 
                (void*)&dctxprep, (void*)(dctx),
                adv->loopMode, adv->nbSeconds, displayLevel);

            if(decompressionResults.error) {
                results.error = decompressionResults.error;
                return results;
            }

            markNb = (markNb+1) % NB_MARKS;
            {   int const ratioAccuracy = (ratio < 10.) ? 3 : 2;
                double const compressionSpeed = results.result.cSpeed / 1000000;
                int const cSpeedAccuracy = (compressionSpeed < 10.) ? 2 : 1;
                double const decompressionSpeed = ((double)srcSize / decompressionResults.result.time) * 1000;
                results.result.dSpeed = decompressionSpeed * 1000000;
                DISPLAYLEVEL(2, "%2s-%-17.17s :%10u ->%10u (%5.*f),%6.*f MB/s ,%6.1f MB/s \r",
                        marks[markNb], displayName, (U32)srcSize, (U32)results.result.cSize,
                        ratioAccuracy, ratio,
                        cSpeedAccuracy, compressionSpeed,
                        decompressionSpeed);
            }
        }

        /* CRC Checking */
        {   U64 const crcCheck = XXH64(resultBuffer, srcSize, 0);
            if ((adv->mode != BMK_DECODE_ONLY) && (crcOrig!=crcCheck)) {
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
        if (adv->additionalParam)
            DISPLAY("-%-3i%11i (%5.3f) %6.2f MB/s %6.1f MB/s  %s (param=%d)\n", cLevel, (int)cSize, ratio, cSpeed, dSpeed, displayName, adv->additionalParam);
        else
            DISPLAY("-%-3i%11i (%5.3f) %6.2f MB/s %6.1f MB/s  %s\n", cLevel, (int)cSize, ratio, cSpeed, dSpeed, displayName);
    }
    DISPLAYLEVEL(2, "%2i#\n", cLevel);
}   /* Bench */

    /* clean up */
    free(compressedBuffer);
    free(resultBuffer);

    free(srcPtrs); 
    free(srcSizes); 
    free(cPtrs); 
    free(cSizes);
    free(resPtrs);
    free(resSizes);

    results.error = 0;
    return results;
}

BMK_return_t BMK_benchMem(const void* srcBuffer, size_t srcSize,
                        const size_t* fileSizes, unsigned nbFiles,
                        const int cLevel, const ZSTD_compressionParameters* comprParams,
                        const void* dictBuffer, size_t dictBufferSize,
                        ZSTD_CCtx* ctx, ZSTD_DCtx* dctx,
                        int displayLevel, const char* displayName) {

    const BMK_advancedParams_t adv = BMK_defaultAdvancedParams();
    return BMK_benchMemAdvanced(srcBuffer, srcSize,
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
                        const BMK_advancedParams_t * const adv) 
{
    BMK_return_t res;
    ZSTD_CCtx* ctx = ZSTD_createCCtx();
    ZSTD_DCtx* dctx = ZSTD_createDCtx();
    if(ctx == NULL || dctx == NULL) {
        EXM_THROW(12, BMK_return_t, "not enough memory for contexts");
    }
    res = BMK_benchMemAdvanced(srcBuffer, srcSize, 
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

ERROR_STRUCT(BMK_result_t*, BMK_returnPtr_t);

/* returns average stats over all range [cLevel, cLevelLast] */
static BMK_returnPtr_t BMK_benchCLevel(const void* srcBuffer, size_t benchedSize,
                            const size_t* fileSizes, unsigned nbFiles,
                            const int cLevel, const int cLevelLast, const ZSTD_compressionParameters* comprParams,
                            const void* dictBuffer, size_t dictBufferSize,
                            int displayLevel, const char* displayName,
                            BMK_advancedParams_t const * const adv)
{
    int l;
    BMK_result_t* res = (BMK_result_t*)malloc(sizeof(BMK_result_t) * (cLevelLast - cLevel + 1));
    BMK_returnPtr_t ret = { 0, res };

    const char* pch = strrchr(displayName, '\\'); /* Windows */
    if (!pch) pch = strrchr(displayName, '/'); /* Linux */
    if (pch) displayName = pch+1;

    if(res == NULL) {
        EXM_THROW(12, BMK_returnPtr_t, "not enough memory\n");
    }
    if (adv->realTime) {
        DISPLAYLEVEL(2, "Note : switching to real-time priority \n");
        SET_REALTIME_PRIORITY;
    }

    if (displayLevel == 1 && !adv->additionalParam)
        DISPLAY("bench %s %s: input %u bytes, %u seconds, %u KB blocks\n", ZSTD_VERSION_STRING, ZSTD_GIT_COMMIT_STRING, (U32)benchedSize, adv->nbSeconds, (U32)(adv->blockSize>>10));

    for (l=cLevel; l <= cLevelLast; l++) {
        BMK_return_t rettmp;
        if (l==0) continue;  /* skip level 0 */
        rettmp = BMK_benchMemCtxless(srcBuffer, benchedSize,
                fileSizes, nbFiles, 
                l, comprParams, 
                dictBuffer, dictBufferSize, 
                displayLevel, displayName, 
                adv);
        if(rettmp.error) {
            ret.error = rettmp.error;
            return ret;
        }
        res[l-cLevel] = rettmp.result;
    }

    return ret;
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

static BMK_returnSet_t BMK_benchFileTable(const char* const * const fileNamesTable, unsigned const nbFiles,
                               const char* const dictFileName, int const cLevel, int const cLevelLast,
                               const ZSTD_compressionParameters* const compressionParams, int displayLevel,
                               const BMK_advancedParams_t * const adv)
{
    void* srcBuffer;
    size_t benchedSize;
    void* dictBuffer = NULL;
    size_t dictBufferSize = 0;
    size_t* const fileSizes = (size_t*)malloc(nbFiles * sizeof(size_t));
    BMK_returnSet_t res;
    U64 const totalSizeToLoad = UTIL_getTotalFileSize(fileNamesTable, nbFiles);

    res.result.cLevel = cLevel;
    res.result.cLevelLast = cLevelLast;
    if (!fileSizes) EXM_THROW(12, BMK_returnSet_t, "not enough memory for fileSizes");

    /* Load dictionary */
    if (dictFileName != NULL) {
        U64 const dictFileSize = UTIL_getFileSize(dictFileName);
        if (dictFileSize > 64 MB)
            EXM_THROW(10, BMK_returnSet_t, "dictionary file %s too large", dictFileName);
        dictBufferSize = (size_t)dictFileSize;
        dictBuffer = malloc(dictBufferSize);
        if (dictBuffer==NULL)
            EXM_THROW(11, BMK_returnSet_t, "not enough memory for dictionary (%u bytes)",
                            (U32)dictBufferSize);
        {
            int errorCode = BMK_loadFiles(dictBuffer, dictBufferSize, fileSizes, &dictFileName, 1, displayLevel);
            if(errorCode) {
                res.error = errorCode;
                return res;
            }
        }
    }

    /* Memory allocation & restrictions */
    benchedSize = BMK_findMaxMem(totalSizeToLoad * 3) / 3;
    if ((U64)benchedSize > totalSizeToLoad) benchedSize = (size_t)totalSizeToLoad;
    if (benchedSize < totalSizeToLoad)
        DISPLAY("Not enough memory; testing %u MB only...\n", (U32)(benchedSize >> 20));
    srcBuffer = malloc(benchedSize);
    if (!srcBuffer) EXM_THROW(12, BMK_returnSet_t, "not enough memory");

    /* Load input buffer */
    {
        int errorCode = BMK_loadFiles(srcBuffer, benchedSize, fileSizes, fileNamesTable, nbFiles, displayLevel);
        if(errorCode) {
            res.error = errorCode;
            return res;
        }
    }
    /* Bench */
    if (adv->separateFiles) {
        const BYTE* srcPtr = (const BYTE*)srcBuffer;
        U32 fileNb;
        res.result.results = (BMK_result_t**)malloc(sizeof(BMK_result_t*) * nbFiles);
        res.result.nbFiles = nbFiles;
        if(res.result.results == NULL) EXM_THROW(12, BMK_returnSet_t, "not enough memory");
        for (fileNb=0; fileNb<nbFiles; fileNb++) {
            size_t const fileSize = fileSizes[fileNb];
            BMK_returnPtr_t errorOrPtr = BMK_benchCLevel(srcPtr, fileSize,
                            fileSizes+fileNb, 1, 
                            cLevel, cLevelLast, compressionParams,
                            dictBuffer, dictBufferSize, 
                            displayLevel, fileNamesTable[fileNb], 
                            adv);
            if(errorOrPtr.error) {
                res.error = errorOrPtr.error;
                return res;
            }
            res.result.results[fileNb] = errorOrPtr.result;
            srcPtr += fileSize;
        }

    } else {
        char mfName[20] = {0};
        res.result.nbFiles = 1;
        snprintf (mfName, sizeof(mfName), " %u files", nbFiles);
        {   
            const char* const displayName = (nbFiles > 1) ? mfName : fileNamesTable[0];
            res.result.results = (BMK_result_t**)malloc(sizeof(BMK_result_t*));
            BMK_returnPtr_t errorOrPtr = BMK_benchCLevel(srcBuffer, benchedSize, 
                            fileSizes, nbFiles, 
                            cLevel, cLevelLast, compressionParams,
                            dictBuffer, dictBufferSize, 
                            displayLevel, displayName, 
                            adv);
            if(res.result.results == NULL) EXM_THROW(12, BMK_returnSet_t, "not enough memory");
            if(errorOrPtr.error) {
                res.error = errorOrPtr.error;
                return res;
            }
            res.result.results[0] = errorOrPtr.result;
    }   }

    /* clean up */
    free(srcBuffer);
    free(dictBuffer);
    free(fileSizes);
    res.error = 0;
    return res;
}


static BMK_returnSet_t BMK_syntheticTest(int cLevel, int cLevelLast, double compressibility,
                              const ZSTD_compressionParameters* compressionParams,
                              int displayLevel, const BMK_advancedParams_t * const adv)
{
    char name[20] = {0};
    size_t benchedSize = 10000000;
    void* const srcBuffer = malloc(benchedSize);
    BMK_returnSet_t res;
    res.result.results = malloc(sizeof(BMK_result_t*));
    res.result.nbFiles = 1;
    res.result.cLevel = cLevel;
    res.result.cLevelLast = cLevelLast;
    /* Memory allocation */
    if (!srcBuffer || !res.result.results) EXM_THROW(21, BMK_returnSet_t, "not enough memory");

    /* Fill input buffer */
    RDG_genBuffer(srcBuffer, benchedSize, compressibility, 0.0, 0);

    /* Bench */
    snprintf (name, sizeof(name), "Synthetic %2u%%", (unsigned)(compressibility*100));
    BMK_returnPtr_t errPtr = BMK_benchCLevel(srcBuffer, benchedSize, 
                    &benchedSize, 1, 
                    cLevel, cLevelLast, compressionParams, 
                    NULL, 0, 
                    displayLevel, name, adv);
    if(errPtr.error) {
        res.error = errPtr.error;
        return res;
    }
    res.result.results[0] = errPtr.result;

    /* clean up */
    free(srcBuffer);
    res.error = 0;
    return res;
}


BMK_returnSet_t BMK_benchFilesAdvanced(const char** fileNamesTable, unsigned nbFiles,
                   const char* dictFileName, 
                   int cLevel, int cLevelLast, 
                   const ZSTD_compressionParameters* compressionParams, 
                   int displayLevel, const BMK_advancedParams_t * const adv)
{
    double const compressibility = (double)g_compressibilityDefault / 100;

    if (cLevel > ZSTD_maxCLevel()) cLevel = ZSTD_maxCLevel();
    if (cLevelLast > ZSTD_maxCLevel()) cLevelLast = ZSTD_maxCLevel();
    if (cLevelLast < cLevel) cLevelLast = cLevel;
    if (cLevelLast > cLevel)
        DISPLAYLEVEL(2, "Benchmarking levels from %d to %d\n", cLevel, cLevelLast);

    if (nbFiles == 0) {
        return BMK_syntheticTest(cLevel, cLevelLast, compressibility, compressionParams, displayLevel, adv);
    }
    else {
        return BMK_benchFileTable(fileNamesTable, nbFiles, dictFileName, cLevel, cLevelLast, compressionParams, displayLevel, adv);
    }
}

int BMK_benchFiles(const char** fileNamesTable, unsigned nbFiles,
                   const char* dictFileName,
                   int cLevel, int cLevelLast,
                   const ZSTD_compressionParameters* compressionParams,
                   int displayLevel) {
    const BMK_advancedParams_t adv = BMK_defaultAdvancedParams();
    return BMK_benchFilesAdvanced(fileNamesTable, nbFiles, dictFileName, cLevel, cLevelLast, compressionParams, displayLevel, &adv).error;
}

/* errorable or just return? */
BMK_result_t BMK_getResult(BMK_resultSet_t resultSet, unsigned fileIdx, int cLevel) {
    assert(resultSet.nbFiles > fileIdx);
    assert(resultSet.cLevel <= cLevel && cLevel <= resultSet.cLevelLast);
    return resultSet.results[fileIdx][cLevel - resultSet.cLevel];
}

void BMK_freeResultSet(BMK_resultSet_t src) {
    unsigned i;
    for(i = 0; i <= src.nbFiles; i++) {
        free(src.results[i]);
    }
    free(src.results);
}
