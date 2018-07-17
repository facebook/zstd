/*
 * Copyright (c) 2015-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */


/*-************************************
*  Dependencies
**************************************/
#include "util.h"      /* Compiler options, UTIL_GetFileSize */
#include <stdlib.h>    /* malloc */
#include <stdio.h>     /* fprintf, fopen, ftello64 */
#include <string.h>    /* strcmp */
#include <math.h>      /* log */
#include <time.h>
#include <assert.h>

#include "mem.h"
#define ZSTD_STATIC_LINKING_ONLY   /* ZSTD_parameters, ZSTD_estimateCCtxSize */
#include "zstd.h"
#include "datagen.h"
#include "xxhash.h"
#include "util.h"
#include "bench.h"
#include "zstd_errors.h"
#include "zstd_internal.h"

/*-************************************
*  Constants
**************************************/
#define PROGRAM_DESCRIPTION "ZSTD parameters tester"
#define AUTHOR "Yann Collet"
#define WELCOME_MESSAGE "*** %s %s %i-bits, by %s ***\n", PROGRAM_DESCRIPTION, ZSTD_VERSION_STRING, (int)(sizeof(void*)*8), AUTHOR

#define TIMELOOP_NANOSEC      (1*1000000000ULL) /* 1 second */

#define NBLOOPS    2
#define TIMELOOP  (2 * SEC_TO_MICRO)
#define NB_LEVELS_TRACKED 22   /* ensured being >= ZSTD_maxCLevel() in BMK_init_level_constraints() */

static const size_t maxMemory = (sizeof(size_t)==4)  ?  (2 GB - 64 MB) : (size_t)(1ULL << ((sizeof(size_t)*8)-31));

#define COMPRESSIBILITY_DEFAULT 0.50

static const U64 g_maxVariationTime = 60 * SEC_TO_MICRO;
static const int g_maxNbVariations = 64;

/*-************************************
*  Macros
**************************************/
#define DISPLAY(...)  fprintf(stderr, __VA_ARGS__)
#define TIMED 0
#ifndef DEBUG
#  define DEBUG 1
#endif
#define DEBUGOUTPUT(...) { if (DEBUG) DISPLAY(__VA_ARGS__); }

#undef MIN
#undef MAX
#define MIN(a,b)   ( (a) < (b) ? (a) : (b) )
#define MAX(a,b)   ( (a) > (b) ? (a) : (b) )
#define CUSTOM_LEVEL 99

/* indices for each of the variables */
#define WLOG_IND 0
#define CLOG_IND 1
#define HLOG_IND 2
#define SLOG_IND 3
#define SLEN_IND 4
#define TLEN_IND 5
//#define STRT_IND 6
//#define NUM_PARAMS 7
#define NUM_PARAMS 6
//just don't use strategy as a param. 

#undef ZSTD_WINDOWLOG_MAX
#define ZSTD_WINDOWLOG_MAX 27 //no long range stuff for now. 

//make 2^[0,10] w/ 999 
#define ZSTD_TARGETLENGTH_MIN 0 
#define ZSTD_TARGETLENGTH_MAX 999

//#define ZSTD_TARGETLENGTH_MAX 1024
#define WLOG_RANGE (ZSTD_WINDOWLOG_MAX - ZSTD_WINDOWLOG_MIN + 1)
#define CLOG_RANGE (ZSTD_CHAINLOG_MAX - ZSTD_CHAINLOG_MIN + 1)
#define HLOG_RANGE (ZSTD_HASHLOG_MAX - ZSTD_HASHLOG_MIN + 1)
#define SLOG_RANGE (ZSTD_SEARCHLOG_MAX - ZSTD_SEARCHLOG_MIN + 1)
#define SLEN_RANGE (ZSTD_SEARCHLENGTH_MAX - ZSTD_SEARCHLENGTH_MIN + 1)
#define TLEN_RANGE 12
//TLEN_RANGE = 0, 2^0 to 2^10; 
//hard coded since we only use powers of 2 (and 999 ~ 1024)

//static const int mintable[NUM_PARAMS] = { ZSTD_WINDOWLOG_MIN, ZSTD_CHAINLOG_MIN, ZSTD_HASHLOG_MIN, ZSTD_SEARCHLOG_MIN, ZSTD_SEARCHLENGTH_MIN, ZSTD_TARGETLENGTH_MIN };
//static const int maxtable[NUM_PARAMS] = { ZSTD_WINDOWLOG_MAX, ZSTD_CHAINLOG_MAX, ZSTD_HASHLOG_MAX, ZSTD_SEARCHLOG_MAX, ZSTD_SEARCHLENGTH_MAX, ZSTD_TARGETLENGTH_MAX };
static const int rangetable[NUM_PARAMS] = { WLOG_RANGE, CLOG_RANGE, HLOG_RANGE, SLOG_RANGE, SLEN_RANGE, TLEN_RANGE };

/*-************************************
*  Benchmark Parameters
**************************************/

typedef BYTE U8;

static double g_grillDuration_s = 99999;   /* about 27 hours */
static U32 g_nbIterations = NBLOOPS;
static double g_compressibility = COMPRESSIBILITY_DEFAULT;
static U32 g_blockSize = 0;
static U32 g_rand = 1;
static U32 g_singleRun = 0;
static U32 g_target = 0;
static U32 g_noSeed = 0;
static ZSTD_compressionParameters g_params = { 0, 0, 0, 0, 0, 0, ZSTD_greedy };
static UTIL_time_t g_time; /* to be used to compare solution finding speeds to compare to original */

void BMK_SetNbIterations(int nbLoops)
{
    g_nbIterations = nbLoops;
    DISPLAY("- %u iterations -\n", g_nbIterations);
}

/*-*******************************************************
*  Private functions
*********************************************************/

/* accuracy in seconds only, span can be multiple years */
static double BMK_timeSpan(time_t tStart) { return difftime(time(NULL), tStart); }

static size_t BMK_findMaxMem(U64 requiredMem)
{
    size_t const step = 64 MB;
    void* testmem = NULL;

    requiredMem = (((requiredMem >> 26) + 1) << 26);
    if (requiredMem > maxMemory) requiredMem = maxMemory;

    requiredMem += 2*step;
    while (!testmem) {
        requiredMem -= step;
        testmem = malloc ((size_t)requiredMem);
    }

    free (testmem);
    return (size_t) (requiredMem - step);
}


static U32 FUZ_rotl32(U32 x, U32 r)
{
    return ((x << r) | (x >> (32 - r)));
}

U32 FUZ_rand(U32* src)
{
    const U32 prime1 = 2654435761U;
    const U32 prime2 = 2246822519U;
    U32 rand32 = *src;
    rand32 *= prime1;
    rand32 += prime2;
    rand32  = FUZ_rotl32(rand32, 13);
    *src = rand32;
    return rand32 >> 5;
}

/** longCommandWArg() :
 *  check if *stringPtr is the same as longCommand.
 *  If yes, @return 1 and advances *stringPtr to the position which immediately follows longCommand.
 * @return 0 and doesn't modify *stringPtr otherwise.
 * from zstdcli.c
 */
static unsigned longCommandWArg(const char** stringPtr, const char* longCommand)
{
    size_t const comSize = strlen(longCommand);
    int const result = !strncmp(*stringPtr, longCommand, comSize);
    if (result) *stringPtr += comSize;
    return result;
}

//assume that clock can at least measure .01 second intervals? 
//make this a settable global initialized with fn? 
//#define CLOCK_GRANULARITY 100000000ULL
static U64 g_clockGranularity = 100000000ULL;

static void findClockGranularity(void) {
    UTIL_time_t clockStart = UTIL_getTime();
    U64 el1 = 0, el2 = 0;
    int i = 0;
    do {
        el1 = el2;
        el2 = UTIL_clockSpanNano(clockStart);
        if(el1 < el2) {
            U64 iv = el2 - el1;
            if(g_clockGranularity > iv) {
                g_clockGranularity = iv;
                i = 0;
            } else {
                i++;
            }
        }
    } while(i < 10);
    DEBUGOUTPUT("Granularity: %llu\n", (unsigned long long)g_clockGranularity);
}

typedef struct {
    U32 cSpeed; /* bytes / sec */
    U32 dSpeed;
    U32 cMem;    /* bytes */    
} constraint_t;

#define CLAMPCHECK(val,min,max) {                     \
    if (val && (((val)<(min)) | ((val)>(max)))) {     \
        DISPLAY("INVALID PARAMETER CONSTRAINTS\n");   \
        return 0;                                     \
}   }


/* Like ZSTD_checkCParams() but allows 0's */
/* no check on targetLen? */
static int cParamValid(ZSTD_compressionParameters paramTarget) {
    CLAMPCHECK(paramTarget.hashLog, ZSTD_HASHLOG_MIN, ZSTD_HASHLOG_MAX);
    CLAMPCHECK(paramTarget.searchLog, ZSTD_SEARCHLOG_MIN, ZSTD_SEARCHLOG_MAX);
    CLAMPCHECK(paramTarget.searchLength, ZSTD_SEARCHLENGTH_MIN, ZSTD_SEARCHLENGTH_MAX);
    CLAMPCHECK(paramTarget.windowLog, ZSTD_WINDOWLOG_MIN, ZSTD_WINDOWLOG_MAX);
    CLAMPCHECK(paramTarget.chainLog, ZSTD_CHAINLOG_MIN, ZSTD_CHAINLOG_MAX);
    if(paramTarget.targetLength > ZSTD_TARGETLENGTH_MAX) {
        DISPLAY("INVALID PARAMETER CONSTRAINTS\n");
        return 0;
    }
    if(paramTarget.strategy > ZSTD_btultra) {
        DISPLAY("INVALID PARAMETER CONSTRAINTS\n");
        return 0;
    }
    return 1;
}

static void cParamZeroMin(ZSTD_compressionParameters* paramTarget) {
    paramTarget->windowLog = paramTarget->windowLog ? paramTarget->windowLog : ZSTD_WINDOWLOG_MIN;
    paramTarget->searchLog = paramTarget->searchLog ? paramTarget->searchLog : ZSTD_SEARCHLOG_MIN;
    paramTarget->chainLog = paramTarget->chainLog ? paramTarget->chainLog : ZSTD_CHAINLOG_MIN;
    paramTarget->hashLog = paramTarget->hashLog ? paramTarget->hashLog : ZSTD_HASHLOG_MIN;
    paramTarget->searchLength = paramTarget->searchLength ? paramTarget->searchLength : ZSTD_SEARCHLENGTH_MIN;
    paramTarget->targetLength = paramTarget->targetLength ? paramTarget->targetLength : 0;
}

static void BMK_translateAdvancedParams(const ZSTD_compressionParameters params)
{
    DISPLAY("--zstd=windowLog=%u,chainLog=%u,hashLog=%u,searchLog=%u,searchLength=%u,targetLength=%u,strategy=%u \n",
             params.windowLog, params.chainLog, params.hashLog, params.searchLog, params.searchLength, params.targetLength, (U32)(params.strategy));
}

/* checks results are feasible */
static int feasible(const BMK_result_t results, const constraint_t target) {
    return (results.cSpeed >= target.cSpeed) && (results.dSpeed >= target.dSpeed) && (results.cMem <= target.cMem || !target.cMem);
}

#define EPSILON 0.001
static int epsilonEqual(const double c1, const double c2) {
    return MAX(c1/c2,c2/c1) < 1 + EPSILON;
}

/* checks exact equivalence to 0, to stop compiler complaining fpeq */
static int eqZero(const double c1) {
    return (U64)c1 == (U64)0.0 || (U64)c1 == (U64)-0.0;
}

/* returns 1 if result2 is strictly 'better' than result1 */
/* strict comparison / cutoff based */
static int objective_lt(const BMK_result_t result1, const BMK_result_t result2) {
    return (result1.cSize > result2.cSize) || (result1.cSize == result2.cSize && result2.cSpeed > result1.cSpeed)
    || (result1.cSize == result2.cSize && epsilonEqual(result2.cSpeed, result1.cSpeed) && result2.dSpeed > result1.dSpeed);
}

/* hill climbing value for part 1 */
static double resultScore(const BMK_result_t res, const size_t srcSize, const constraint_t target) {
    double cs = 0., ds = 0., rt, cm = 0.;
    const double r1 = 1, r2 = 0.1, rtr = 0.5;
    double ret;
    if(target.cSpeed) { cs = res.cSpeed / (double)target.cSpeed; }
    if(target.dSpeed) { ds = res.dSpeed / (double)target.dSpeed; }
    if(target.cMem != (U32)-1) { cm = (double)target.cMem / res.cMem; }
    rt = ((double)srcSize / res.cSize);

    ret = (MIN(1, cs) + MIN(1, ds)  + MIN(1, cm))*r1 + rt * rtr + 
         (MAX(0, log(cs))+ MAX(0, log(ds))+ MAX(0, log(cm))) * r2;
    //DISPLAY("resultScore: %f\n", ret);
    return ret;
}

/* factor sort of arbitrary */
static constraint_t relaxTarget(constraint_t target) {
    target.cMem = (U32)-1;
    target.cSpeed *= 0.9; 
    target.dSpeed *= 0.9;
    return target;
}

/*-*******************************************************
*  Bench functions
*********************************************************/

typedef struct
{
    const char* srcPtr;
    size_t srcSize;
    char*  cPtr;
    size_t cRoom;
    size_t cSize;
    char*  resPtr;
    size_t resSize;
} blockParam_t;


const char* g_stratName[ZSTD_btultra+1] = {
                "(none)       ", "ZSTD_fast    ", "ZSTD_dfast   ",
                "ZSTD_greedy  ", "ZSTD_lazy    ", "ZSTD_lazy2   ",
                "ZSTD_btlazy2 ", "ZSTD_btopt   ", "ZSTD_btultra "};

static size_t
BMK_benchParam(BMK_result_t* resultPtr,
               const void* srcBuffer, const size_t srcSize,
               const size_t* fileSizes, const unsigned nbFiles,
               ZSTD_CCtx* ctx, ZSTD_DCtx* dctx, 
               const ZSTD_compressionParameters cParams) {

    BMK_return_t res = BMK_benchMem(srcBuffer,srcSize, fileSizes, nbFiles, 0, &cParams, NULL, 0, ctx, dctx, 0, "File");
    *resultPtr = res.result;
    return res.error;
}

/* benchParam but only takes in one file. */
static size_t
BMK_benchParam1(BMK_result_t* resultPtr,
               const void* srcBuffer, size_t srcSize,
               ZSTD_CCtx* ctx, ZSTD_DCtx* dctx, 
               const ZSTD_compressionParameters cParams) {

    BMK_return_t res = BMK_benchMem(srcBuffer,srcSize, &srcSize, 1, 0, &cParams, NULL, 0, ctx, dctx, 0, "File");
    *resultPtr = res.result;
    return res.error;
}

typedef struct {
    BMK_result_t result;
    ZSTD_compressionParameters params;
} winnerInfo_t;

/*-*******************************************************
*  From Paramgrill
*********************************************************/

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

/*-*******************************************************
*  From Paramgrill End
*********************************************************/

/* Replicate function of benchMemAdvanced, but with pre-split src / dst buffers, with relevant info to invert it (compressedSizes) passed out. */
/*BMK_benchMemAdvanced(srcBuffer,srcSize, dstBuffer, dstSize, fileSizes, nbFiles, 0, &cParams, dictBuffer, dictSize, ctx, dctx, 0, "File", &adv); */
/* nbSeconds used in same way as in BMK_advancedParams_t, as nbIters when in iterMode */

/* if in decodeOnly, then srcPtr's will be compressed blocks, and uncompressedBlocks will be written to dstPtrs? */
/* dictionary nullable, nothing else though. */
static BMK_return_t BMK_benchMemInvertible(const void * const * const srcPtrs, size_t const * const srcSizes,
                        void** dstPtrs, size_t* dstCapacityToSizes, U32 const nbBlocks, 
                        const int cLevel, const ZSTD_compressionParameters* comprParams,
                        const void* dictBuffer, const size_t dictBufferSize,
                        ZSTD_CCtx* ctx, ZSTD_DCtx* dctx,
                        const BMK_mode_t mode, const BMK_loopMode_t loopMode, const unsigned nbSeconds) {
    U32 i;
    BMK_return_t results = { { 0, 0., 0., 0 }, 0 } ;
    size_t srcSize = 0;
    void** const resPtrs = malloc(sizeof(void*) * nbBlocks); /* only really needed in both mode. */
    size_t* const resSizes = malloc(sizeof(size_t) * nbBlocks);
    int freeDST = 0;

    BMK_advancedParams_t adv = BMK_initAdvancedParams();
    adv.mode = mode;
    adv.loopMode = loopMode;
    adv.nbSeconds = nbSeconds;

    /* resSizes == srcSizes, but modifiable */
    memcpy(resSizes, srcSizes, sizeof(size_t) * nbBlocks);

    for(i = 0; i < nbBlocks; i++) {
        srcSize += srcSizes[i];
    }

    if(!ctx || !dctx || !srcPtrs || ! srcSizes)
    {
        results.error = 31;
        DISPLAY("error: passed in null argument\n");
        free(resPtrs);
        free(resSizes);
        return results;
    }
    if(!resPtrs || !resSizes) {
        results.error = 32;
        DISPLAY("error: allocation failed\n");
        free(resPtrs);
        free(resSizes);
        return results;
    }

    /* so resPtr is continuous */ 
    resPtrs[0] = malloc(srcSize);

    if(!(resPtrs[0])) {
        results.error = 32;
        DISPLAY("error: allocation failed\n");
        free(resPtrs);
        free(resSizes);
        return results;
    }

    for(i = 1; i < nbBlocks; i++) {
        resPtrs[i] = (void*)(((char*)resPtrs[i-1]) + srcSizes[i-1]);
    }

    /* allocate own dst if NULL */
    if(dstPtrs == NULL) {
        freeDST = 1;
        dstPtrs = malloc(nbBlocks * sizeof(void*));
        dstCapacityToSizes = malloc(nbBlocks * sizeof(size_t));
        if(dstPtrs == NULL) {
            results.error = 33;
            DISPLAY("error: allocation failed\n");
            free(resPtrs);
            free(resSizes);
            return results;
        }

        if(mode == BMK_decodeOnly) { //dst is src
            size_t dstSize = 0;
            for(i = 0; i < nbBlocks; i++) {
                dstCapacityToSizes[i] = ZSTD_getDecompressedSize(srcPtrs[i], srcSizes[i]);
                dstSize += dstCapacityToSizes[i];
            }
            dstPtrs[0] = malloc(dstSize);
            if(dstPtrs[0] == NULL) {
                results.error = 34;
                DISPLAY("error: allocation failed\n");
                goto _cleanUp;
            }
            for(i = 1; i < nbBlocks; i++) {
                dstPtrs[i] = (void*)(((char*)dstPtrs[i-1]) + ZSTD_getDecompressedSize(srcPtrs[i-1], srcSizes[i-1]));
            }
        } else {
            dstPtrs[0] = malloc(ZSTD_compressBound(srcSize) + (nbBlocks * 1024));
            if(dstPtrs[0] == NULL) {
                results.error = 35;
                DISPLAY("error: allocation failed\n");
                goto _cleanUp;
            }
            dstCapacityToSizes[0] = ZSTD_compressBound(srcSizes[0]);
            for(i = 1; i < nbBlocks; i++) {
                dstPtrs[i] = (void*)(((char*)dstPtrs[i-1]) + dstCapacityToSizes[i-1]);
                dstCapacityToSizes[i] = ZSTD_compressBound(srcSizes[i]);
            }
        }
    }

    /* warmimg up memory */
    for(i = 0; i < nbBlocks; i++) {
        RDG_genBuffer(dstPtrs[i], dstCapacityToSizes[i], 0.10, 0.50, 1);
    }

    /* Bench */
    {   
        {
            BMK_initCCtxArgs cctxprep;
            BMK_initDCtxArgs dctxprep;
            cctxprep.ctx = ctx;
            cctxprep.dictBuffer = dictBuffer;
            cctxprep.dictBufferSize = dictBufferSize;
            cctxprep.cLevel = cLevel;
            cctxprep.comprParams = comprParams;
            cctxprep.adv = &adv;
            dctxprep.dctx = dctx;
            dctxprep.dictBuffer = dictBuffer;
            dctxprep.dictBufferSize = dictBufferSize;
            if(loopMode == BMK_timeMode) {
                BMK_customTimedReturn_t intermediateResultCompress;
                BMK_customTimedReturn_t intermediateResultDecompress;  
                BMK_timedFnState_t* timeStateCompress = BMK_createTimeState(nbSeconds);
                BMK_timedFnState_t* timeStateDecompress = BMK_createTimeState(nbSeconds);
                if(mode == BMK_compressOnly) {
                    intermediateResultCompress.completed = 0;
                    intermediateResultDecompress.completed = 1;
                } else if (mode == BMK_decodeOnly) {
                    intermediateResultCompress.completed = 1;
                    intermediateResultDecompress.completed = 0;
                } else { /* both */
                    intermediateResultCompress.completed = 0;
                    intermediateResultDecompress.completed = 0;
                }
                while(!(intermediateResultCompress.completed && intermediateResultDecompress.completed)) {
                    if(!intermediateResultCompress.completed) { 
                        intermediateResultCompress = BMK_benchFunctionTimed(timeStateCompress, &local_defaultCompress, (void*)ctx, &local_initCCtx, (void*)&cctxprep,
                        nbBlocks, srcPtrs, srcSizes, dstPtrs, dstCapacityToSizes);
                        if(intermediateResultCompress.result.error) {
                            results.error = intermediateResultCompress.result.error;
                            BMK_freeTimeState(timeStateCompress);
                            BMK_freeTimeState(timeStateDecompress);
                            goto _cleanUp;
                        }
                        results.result.cSpeed = ((double)srcSize / intermediateResultCompress.result.result.nanoSecPerRun) * 1000000000;
                        results.result.cSize = intermediateResultCompress.result.result.sumOfReturn;
                    }

                    if(!intermediateResultDecompress.completed) {
                        if(mode == BMK_decodeOnly) {
                            intermediateResultDecompress = BMK_benchFunctionTimed(timeStateDecompress, &local_defaultDecompress, (void*)(dctx), &local_initDCtx, (void*)&dctxprep,
                            nbBlocks, (const void* const*)srcPtrs, srcSizes, dstPtrs, dstCapacityToSizes);
                        } else { /* both, decompressed result already written to dstPtr */
                            intermediateResultDecompress = BMK_benchFunctionTimed(timeStateDecompress, &local_defaultDecompress, (void*)(dctx), &local_initDCtx, (void*)&dctxprep,
                            nbBlocks, (const void* const*)dstPtrs, dstCapacityToSizes, resPtrs, resSizes);
                        }

                        if(intermediateResultDecompress.result.error) {
                            results.error = intermediateResultDecompress.result.error;
                            BMK_freeTimeState(timeStateCompress);
                            BMK_freeTimeState(timeStateDecompress);
                            goto _cleanUp;
                        }
                        results.result.dSpeed = ((double)srcSize / intermediateResultDecompress.result.result.nanoSecPerRun) * 1000000000;
                    }
                }
                BMK_freeTimeState(timeStateCompress);
                BMK_freeTimeState(timeStateDecompress);
            } else { //iterMode;
                if(mode != BMK_decodeOnly) {

                    BMK_customReturn_t compressionResults = BMK_benchFunction(&local_defaultCompress, (void*)ctx, &local_initCCtx, (void*)&cctxprep,
                        nbBlocks, srcPtrs, srcSizes, dstPtrs, dstCapacityToSizes, nbSeconds); 
                    if(compressionResults.error) {
                        results.error = compressionResults.error;
                        goto _cleanUp;
                    }
                    if(compressionResults.result.nanoSecPerRun == 0) {
                        results.result.cSpeed = 0;
                    } else {
                        results.result.cSpeed = (double)srcSize / compressionResults.result.nanoSecPerRun * TIMELOOP_NANOSEC;
                    }
                    results.result.cSize = compressionResults.result.sumOfReturn;
                }
                if(mode != BMK_compressOnly) {
                    BMK_customReturn_t decompressionResults;
                    if(mode == BMK_decodeOnly) {
                        decompressionResults = BMK_benchFunction(
                            &local_defaultDecompress, (void*)(dctx),
                            &local_initDCtx, (void*)&dctxprep, nbBlocks,
                            (const void* const*)srcPtrs, srcSizes, dstPtrs, dstCapacityToSizes, 
                            nbSeconds);
                    } else {
                        decompressionResults = BMK_benchFunction(
                            &local_defaultDecompress, (void*)(dctx),
                            &local_initDCtx, (void*)&dctxprep, nbBlocks,
                            (const void* const*)dstPtrs, dstCapacityToSizes, resPtrs, resSizes, 
                            nbSeconds);
                    }

                    if(decompressionResults.error) {
                        results.error = decompressionResults.error;
                        goto _cleanUp;
                    }

                    if(decompressionResults.result.nanoSecPerRun == 0) {
                        results.result.dSpeed = 0;
                    } else {
                        results.result.dSpeed = (double)srcSize / decompressionResults.result.nanoSecPerRun * TIMELOOP_NANOSEC;
                    }
                }
            }
        }
    }   /* Bench */
    results.result.cMem = (1 << (comprParams->windowLog)) + ZSTD_sizeof_CCtx(ctx);

_cleanUp:
    free(resPtrs[0]);
    free(resPtrs);
    free(resSizes);
    if(freeDST) {
        free(dstPtrs[0]);
        free(dstPtrs);
    }
    return results;
}

/* global winner used for display. */
//Should be totally 0 initialized? 
static winnerInfo_t g_winner; 
static constraint_t g_targetConstraints; 

static void BMK_printWinner(FILE* f, const U32 cLevel, const BMK_result_t result, const ZSTD_compressionParameters params, const size_t srcSize)
{
    if(DEBUG || (objective_lt(g_winner.result, result) && feasible(result, g_targetConstraints))) {
        char lvlstr[15] = "Custom Level";
        const U64 time = UTIL_clockSpanNano(g_time);
        const U64 minutes = time / (60ULL * TIMELOOP_NANOSEC);
        if(DEBUG && (objective_lt(g_winner.result, result) && feasible(result, g_targetConstraints))) {
            DISPLAY("New Winner: \n");
        }

        DISPLAY("\r%79s\r", "");

        fprintf(f,"    {%3u,%3u,%3u,%3u,%3u,%3u, %s },  ",
                params.windowLog, params.chainLog, params.hashLog, params.searchLog, params.searchLength,
                params.targetLength, g_stratName[(U32)(params.strategy)]);
        if(cLevel != CUSTOM_LEVEL) {
            snprintf(lvlstr, 15, "  Level %2u  ", cLevel);
        }
        fprintf(f,
            "/* %s */   /* R:%5.3f at %5.1f MB/s - %5.1f MB/s */",
            lvlstr, (double)srcSize / result.cSize, result.cSpeed / 1000000., result.dSpeed / 1000000.);

        if(TIMED) { fprintf(f, " - %1lu:%2lu:%05.2f", (unsigned long) minutes / 60,(unsigned long) minutes % 60, (double)(time - minutes * TIMELOOP_NANOSEC * 60ULL)/TIMELOOP_NANOSEC); }
        fprintf(f, "\n");
        if(objective_lt(g_winner.result, result) && feasible(result, g_targetConstraints)) {
            BMK_translateAdvancedParams(params);
            g_winner.result = result;
            g_winner.params = params;
        }
    }  
    //else {
    //    DISPLAY("G_WINNER: ");
    //    DISPLAY("/* R:%5.3f at %5.1f MB/s - %5.1f MB/s */ \n",(double)srcSize / g_winner.result.cSize , g_winner.result.cSpeed / 1000000 , g_winner.result.dSpeed / 1000000);
    //    DISPLAY("LOSER   : ");
    //    DISPLAY("/* R:%5.3f at %5.1f MB/s - %5.1f MB/s */ \n",(double)srcSize / result.cSize, result.cSpeed / 1000000 , result.dSpeed / 1000000);
    //} 
}

static void BMK_printWinners2(FILE* f, const winnerInfo_t* winners, size_t srcSize)
{
    int cLevel;

    fprintf(f, "\n /* Proposed configurations : */ \n");
    fprintf(f, "    /* W,  C,  H,  S,  L,  T, strat */ \n");

    for (cLevel=0; cLevel <= NB_LEVELS_TRACKED; cLevel++)
        BMK_printWinner(f, cLevel, winners[cLevel].result, winners[cLevel].params, srcSize);
}


static void BMK_printWinners(FILE* f, const winnerInfo_t* winners, size_t srcSize)
{
    fseek(f, 0, SEEK_SET);
    BMK_printWinners2(f, winners, srcSize);
    fflush(f);
    BMK_printWinners2(stdout, winners, srcSize);
}


typedef struct {
    double cSpeed_min;
    double dSpeed_min;
    U32 windowLog_max;
    ZSTD_strategy strategy_max;
} level_constraints_t;

static level_constraints_t g_level_constraint[NB_LEVELS_TRACKED+1];

static void BMK_init_level_constraints(int bytePerSec_level1)
{
    assert(NB_LEVELS_TRACKED >= ZSTD_maxCLevel());
    memset(g_level_constraint, 0, sizeof(g_level_constraint));
    g_level_constraint[1].cSpeed_min = bytePerSec_level1;
    g_level_constraint[1].dSpeed_min = 0.;
    g_level_constraint[1].windowLog_max = 19;
    g_level_constraint[1].strategy_max = ZSTD_fast;

    /* establish speed objectives (relative to level 1) */
    {   int l;
        for (l=2; l<=NB_LEVELS_TRACKED; l++) {
            g_level_constraint[l].cSpeed_min = (g_level_constraint[l-1].cSpeed_min * 49) / 64;
            g_level_constraint[l].dSpeed_min = 0.;
            g_level_constraint[l].windowLog_max = (l<20) ? 23 : l+5;   /* only --ultra levels >= 20 can use windowlog > 23 */
            g_level_constraint[l].strategy_max = (l<19) ? ZSTD_btopt : ZSTD_btultra;   /* level 19 is allowed to use btultra */
    }   }
}

static int BMK_seed(winnerInfo_t* winners, const ZSTD_compressionParameters params,
              const void* srcBuffer, size_t srcSize,
                    ZSTD_CCtx* ctx, ZSTD_DCtx* dctx)
{
    BMK_result_t testResult;
    int better = 0;
    int cLevel;

    BMK_benchParam1(&testResult, srcBuffer, srcSize, ctx, dctx, params);


    for (cLevel = 1; cLevel <= NB_LEVELS_TRACKED; cLevel++) {
        if (testResult.cSpeed < g_level_constraint[cLevel].cSpeed_min)
            continue;   /* not fast enough for this level */
        if (testResult.dSpeed < g_level_constraint[cLevel].dSpeed_min)
            continue;   /* not fast enough for this level */
        if (params.windowLog > g_level_constraint[cLevel].windowLog_max)
            continue;   /* too much memory for this level */
        if (params.strategy > g_level_constraint[cLevel].strategy_max)
            continue;   /* forbidden strategy for this level */
        if (winners[cLevel].result.cSize==0) {
            /* first solution for this cLevel */
            winners[cLevel].result = testResult;
            winners[cLevel].params = params;
            BMK_printWinner(stdout, cLevel, testResult, params, srcSize);
            better = 1;
            continue;
        }

        if ((double)testResult.cSize <= ((double)winners[cLevel].result.cSize * (1. + (0.02 / cLevel))) ) {
            /* Validate solution is "good enough" */
            double W_ratio = (double)srcSize / testResult.cSize;
            double O_ratio = (double)srcSize / winners[cLevel].result.cSize;
            double W_ratioNote = log (W_ratio);
            double O_ratioNote = log (O_ratio);
            size_t W_DMemUsed = (1 << params.windowLog) + (16 KB);
            size_t O_DMemUsed = (1 << winners[cLevel].params.windowLog) + (16 KB);
            double W_DMemUsed_note = W_ratioNote * ( 40 + 9*cLevel) - log((double)W_DMemUsed);
            double O_DMemUsed_note = O_ratioNote * ( 40 + 9*cLevel) - log((double)O_DMemUsed);

            size_t W_CMemUsed = (1 << params.windowLog) + ZSTD_estimateCCtxSize_usingCParams(params);
            size_t O_CMemUsed = (1 << winners[cLevel].params.windowLog) + ZSTD_estimateCCtxSize_usingCParams(winners[cLevel].params);
            double W_CMemUsed_note = W_ratioNote * ( 50 + 13*cLevel) - log((double)W_CMemUsed);
            double O_CMemUsed_note = O_ratioNote * ( 50 + 13*cLevel) - log((double)O_CMemUsed);

            double W_CSpeed_note = W_ratioNote * ( 30 + 10*cLevel) + log(testResult.cSpeed);
            double O_CSpeed_note = O_ratioNote * ( 30 + 10*cLevel) + log(winners[cLevel].result.cSpeed);

            double W_DSpeed_note = W_ratioNote * ( 20 + 2*cLevel) + log(testResult.dSpeed);
            double O_DSpeed_note = O_ratioNote * ( 20 + 2*cLevel) + log(winners[cLevel].result.dSpeed);

            if (W_DMemUsed_note < O_DMemUsed_note) {
                /* uses too much Decompression memory for too little benefit */
                if (W_ratio > O_ratio)
                DISPLAY ("Decompression Memory : %5.3f @ %4.1f MB  vs  %5.3f @ %4.1f MB   : not enough for level %i\n",
                         W_ratio, (double)(W_DMemUsed) / 1024 / 1024,
                         O_ratio, (double)(O_DMemUsed) / 1024 / 1024,   cLevel);
                continue;
            }
            if (W_CMemUsed_note < O_CMemUsed_note) {
                /* uses too much memory for compression for too little benefit */
                if (W_ratio > O_ratio)
                DISPLAY ("Compression Memory : %5.3f @ %4.1f MB  vs  %5.3f @ %4.1f MB   : not enough for level %i\n",
                         W_ratio, (double)(W_CMemUsed) / 1024 / 1024,
                         O_ratio, (double)(O_CMemUsed) / 1024 / 1024,   cLevel);
                continue;
            }
            if (W_CSpeed_note   < O_CSpeed_note  ) {
                /* too large compression speed difference for the compression benefit */
                if (W_ratio > O_ratio)
                DISPLAY ("Compression Speed : %5.3f @ %4.1f MB/s  vs  %5.3f @ %4.1f MB/s   : not enough for level %i\n",
                         W_ratio, testResult.cSpeed / 1000000,
                         O_ratio, winners[cLevel].result.cSpeed / 1000000.,   cLevel);
                continue;
            }
            if (W_DSpeed_note   < O_DSpeed_note  ) {
                /* too large decompression speed difference for the compression benefit */
                if (W_ratio > O_ratio)
                DISPLAY ("Decompression Speed : %5.3f @ %4.1f MB/s  vs  %5.3f @ %4.1f MB/s   : not enough for level %i\n",
                         W_ratio, testResult.dSpeed / 1000000.,
                         O_ratio, winners[cLevel].result.dSpeed / 1000000.,   cLevel);
                continue;
            }

            if (W_ratio < O_ratio)
                DISPLAY("Solution %4.3f selected over %4.3f at level %i, due to better secondary statistics \n", W_ratio, O_ratio, cLevel);

            winners[cLevel].result = testResult;
            winners[cLevel].params = params;
            BMK_printWinner(stdout, cLevel, testResult, params, srcSize);

            better = 1;
    }   }

    return better;
}

/* bounds check in sanitize too? */
#define CLAMP(var, lo, hi) {                          \
    var = MAX(MIN(var, hi), lo);                      \
}

/* nullified useless params, to ensure count stats */
/* no point in windowLog < chainLog (no point 2x chainLog for bt) */
/* now with built in bounds-checking */
/* no longer does anything with sanitizeVarArray + clampcheck */
static ZSTD_compressionParameters sanitizeParams(ZSTD_compressionParameters params)
{
    if (params.strategy == ZSTD_fast)
        g_params.chainLog = 0, g_params.searchLog = 0;
    if (params.strategy == ZSTD_dfast)
        g_params.searchLog = 0;
    if (params.strategy != ZSTD_btopt && params.strategy != ZSTD_btultra && params.strategy != ZSTD_fast)
        g_params.targetLength = 0;

    return params;
}

/* new length */
/* keep old array, will need if iter over strategy. */
static int sanitizeVarArray(const int varLength, const U32* varArray, U32* varNew, const ZSTD_strategy strat) {
    int i, j = 0;
    for(i = 0; i < varLength; i++) {
        if( !((varArray[i] == CLOG_IND && strat == ZSTD_fast)
            || (varArray[i] == SLOG_IND && strat == ZSTD_dfast) 
            || (varArray[i] == TLEN_IND && strat != ZSTD_btopt && strat != ZSTD_btultra && strat != ZSTD_fast))) {
            varNew[j] = varArray[i];
            j++;
        }
    }
    return j;

}

/* res should be NUM_PARAMS size */
/* constructs varArray from ZSTD_compressionParameters style parameter */
static int variableParams(const ZSTD_compressionParameters paramConstraints, U32* res) {
    int j = 0;
    if(!paramConstraints.windowLog) {
        res[j] = WLOG_IND;
        j++;
    }
    if(!paramConstraints.chainLog) {
        res[j] = CLOG_IND;
        j++;
    }
    if(!paramConstraints.hashLog) {
        res[j] = HLOG_IND;
        j++;
    }
    if(!paramConstraints.searchLog) {
        res[j] = SLOG_IND;
        j++;
    }
    if(!paramConstraints.searchLength) {
        res[j] = SLEN_IND;
        j++;
    }
    if(!paramConstraints.targetLength) {
        res[j] = TLEN_IND;
        j++;
    }
    return j;
}

/* amt will probably always be \pm 1? */
/* slight change from old paramVariation, targetLength can only take on powers of 2 now (999 ~= 1024?) */
/* take max/min bounds into account as well? */
static void paramVaryOnce(const U32 paramIndex, const int amt, ZSTD_compressionParameters* ptr) {
    switch(paramIndex)
    {
        case WLOG_IND: ptr->windowLog    += amt; break;
        case CLOG_IND: ptr->chainLog     += amt; break;
        case HLOG_IND: ptr->hashLog      += amt; break;
        case SLOG_IND: ptr->searchLog    += amt; break;
        case SLEN_IND: ptr->searchLength += amt; break;
        case TLEN_IND: 
            if(amt >= 0) { 
                if(ptr->targetLength == 0) {
                    if(amt > 0) {
                        ptr->targetLength = MIN(1 << (amt - 1), 999);
                    }
                } else {
                    ptr->targetLength <<= amt; 
                    ptr->targetLength = MIN(ptr->targetLength, 999);
                }
            } else { 
                if(ptr->targetLength == 999) {
                    ptr->targetLength = 1024;
                }
                ptr->targetLength >>= -amt; 
            } 
            break;
        default: break;
    }
}

/* varies ptr by nbChanges respecting varyParams*/
static void paramVariation(ZSTD_compressionParameters* ptr, const U32* varyParams, const int varyLen, const U32 nbChanges)
{
    ZSTD_compressionParameters p;
    U32 validated = 0;
    while (!validated) {
        U32 i;
        p = *ptr;
        for (i = 0 ; i < nbChanges ; i++) {
            const U32 changeID = FUZ_rand(&g_rand) % (varyLen << 1);
            paramVaryOnce(varyParams[changeID >> 1], ((changeID & 1) << 1) - 1, &p);
        }
        validated = !ZSTD_isError(ZSTD_checkCParams(p));
    }
    *ptr = p;//sanitizeParams(p);
}

/* length of memo table given free variables */
static size_t memoTableLen(const U32* varyParams, const int varyLen) {
    size_t arrayLen = 1;
    int i;
    for(i = 0; i < varyLen; i++) {
        arrayLen *= rangetable[varyParams[i]];
    }
    return arrayLen;
}

//sort of ~lg2 (replace 1024 w/ 999, and add 0 at lower end of range) for memoTableInd Tlen
static unsigned lg2(unsigned x) {
    if(x == 999) {
        return 11;
    }
    return x ? ZSTD_highbit32(x) + 1 : 0;
}

/* returns unique index of compression parameters */
static unsigned memoTableInd(const ZSTD_compressionParameters* ptr, const U32* varyParams, const int varyLen) {
    int i;
    unsigned ind = 0;
    for(i = 0; i < varyLen; i++) {
        switch(varyParams[i]) {
            case WLOG_IND: ind *= WLOG_RANGE; ind += ptr->windowLog         - ZSTD_WINDOWLOG_MIN   ; break;
            case CLOG_IND: ind *= CLOG_RANGE; ind += ptr->chainLog          - ZSTD_CHAINLOG_MIN    ; break;
            case HLOG_IND: ind *= HLOG_RANGE; ind += ptr->hashLog           - ZSTD_HASHLOG_MIN     ; break;
            case SLOG_IND: ind *= SLOG_RANGE; ind += ptr->searchLog         - ZSTD_SEARCHLOG_MIN   ; break;
            case SLEN_IND: ind *= SLEN_RANGE; ind += ptr->searchLength      - ZSTD_SEARCHLENGTH_MIN; break;
            case TLEN_IND: ind *= TLEN_RANGE; ind += lg2(ptr->targetLength) - ZSTD_TARGETLENGTH_MIN; break;
        }
    }
    return ind;
}

/* inverse of above function (from index to parameters) */
static void memoTableIndInv(ZSTD_compressionParameters* ptr, const U32* varyParams, const int varyLen, size_t ind) {
    int i;
    for(i = varyLen - 1; i >= 0; i--) {
        switch(varyParams[i]) {
            case WLOG_IND: ptr->windowLog    = ind % WLOG_RANGE + ZSTD_WINDOWLOG_MIN;                             ind /= WLOG_RANGE; break;
            case CLOG_IND: ptr->chainLog     = ind % CLOG_RANGE + ZSTD_CHAINLOG_MIN;                              ind /= CLOG_RANGE; break;
            case HLOG_IND: ptr->hashLog      = ind % HLOG_RANGE + ZSTD_HASHLOG_MIN;                               ind /= HLOG_RANGE; break;
            case SLOG_IND: ptr->searchLog    = ind % SLOG_RANGE + ZSTD_SEARCHLOG_MIN;                             ind /= SLOG_RANGE; break;
            case SLEN_IND: ptr->searchLength = ind % SLEN_RANGE + ZSTD_SEARCHLENGTH_MIN;                          ind /= SLEN_RANGE; break;
            case TLEN_IND: ptr->targetLength = (ind % TLEN_RANGE) ? MIN(1 << ((ind % TLEN_RANGE) - 1), 999) : 0;  ind /= TLEN_RANGE; break;
        }
    }
}


/* Initialize memotable, immediately mark redundant / obviously infeasible params as */
static void memoTableInit(U8* memoTable, ZSTD_compressionParameters paramConstraints, const constraint_t target, const U32* varyParams, const int varyLen, const size_t srcSize) {
    size_t i;
    size_t arrayLen = memoTableLen(varyParams, varyLen);
    int cwFixed = !paramConstraints.chainLog || !paramConstraints.windowLog;
    int scFixed = !paramConstraints.searchLog || !paramConstraints.chainLog;
    int wFixed = !paramConstraints.windowLog;
    int j = 0;
    memset(memoTable, 0, arrayLen);
    cParamZeroMin(&paramConstraints);

    for(i = 0; i < arrayLen; i++) {
        memoTableIndInv(&paramConstraints, varyParams, varyLen, i);
        if(ZSTD_estimateCStreamSize_usingCParams(paramConstraints) > (size_t)target.cMem) {
            memoTable[i] = 255;
            j++;
        }
        if(wFixed && (1ULL << paramConstraints.windowLog) > (srcSize << 1)) {
            memoTable[i] = 255;
        }
        /* nil out parameter sets equivalent to others. */
        if(cwFixed/* at most least 1 param fixed. */) {
            if(paramConstraints.strategy == ZSTD_btlazy2 || paramConstraints.strategy == ZSTD_btopt || paramConstraints.strategy == ZSTD_btultra) {
                if(paramConstraints.chainLog > paramConstraints.windowLog + 1) {
                    if(memoTable[i] != 255) { j++; }
                    memoTable[i] = 255;
                }
            } else {
                if(paramConstraints.chainLog > paramConstraints.windowLog) {
                    if(memoTable[i] != 255) { j++; }
                    memoTable[i] = 255;
                }
            }
        }
        if(scFixed) {
            if(paramConstraints.searchLog > paramConstraints.chainLog) {
                if(memoTable[i] != 255) { j++; }
                memoTable[i] = 255;
            }
        }
    }
    DEBUGOUTPUT("%d / %d Invalid\n", j, (int)i);
    if((int)i == j) {
        DEBUGOUTPUT("!!!Strategy %d totally infeasible\n", (int)paramConstraints.strategy)
    }
}

/* inits memotables for all (including mallocs), all strategies */
/* takes unsanitized varyParams */
static U8** memoTableInitAll(ZSTD_compressionParameters paramConstraints, constraint_t target, const U32* varyParams, const int varyLen, const size_t srcSize) {
    U32 varNew[NUM_PARAMS];
    int varLenNew;
    U8** mtAll = malloc(sizeof(U8*) * (ZSTD_btultra + 1));
    int i;
    if(mtAll == NULL) {
        return NULL;
    }
    for(i = 1; i <= (int)ZSTD_btultra; i++) {
        varLenNew = sanitizeVarArray(varyLen, varyParams, varNew, i);
        mtAll[i] = malloc(sizeof(U8) * memoTableLen(varNew, varLenNew));
        if(mtAll[i] == NULL) {
            return NULL;
        }
        memoTableInit(mtAll[i], paramConstraints, target, varNew, varLenNew, srcSize);
    }
    return mtAll;
}

static void memoTableFreeAll(U8** mtAll) {
    int i;
    if(mtAll == NULL) { return; }
    for(i = 1; i <= (int)ZSTD_btultra; i++) {
        free(mtAll[i]);
    }
    free(mtAll);
}

#define PARAMTABLELOG   25
#define PARAMTABLESIZE (1<<PARAMTABLELOG)
#define PARAMTABLEMASK (PARAMTABLESIZE-1)
static BYTE g_alreadyTested[PARAMTABLESIZE] = {0};   /* init to zero */

/*
#define NB_TESTS_PLAYED(p) \
    g_alreadyTested[(XXH64(((void*)&sanitizeParams(p), sizeof(p), 0) >> 3) & PARAMTABLEMASK] */

static BYTE* NB_TESTS_PLAYED(ZSTD_compressionParameters p) {
    ZSTD_compressionParameters p2 = sanitizeParams(p);
    return &g_alreadyTested[(XXH64((void*)&p2, sizeof(p2), 0) >> 3) & PARAMTABLEMASK];
}

static void playAround(FILE* f, winnerInfo_t* winners,
                       ZSTD_compressionParameters params,
                       const void* srcBuffer, size_t srcSize,
                       ZSTD_CCtx* ctx, ZSTD_DCtx* dctx)
{
    int nbVariations = 0;
    UTIL_time_t const clockStart = UTIL_getTime();
    const U32 unconstrained[NUM_PARAMS] = { 0, 1, 2, 3, 4, 5 };


    while (UTIL_clockSpanMicro(clockStart) < g_maxVariationTime) {
        ZSTD_compressionParameters p = params;
        BYTE* b;

        if (nbVariations++ > g_maxNbVariations) break;
        paramVariation(&p, unconstrained, 7, 4);

        /* exclude faster if already played params */
        if (FUZ_rand(&g_rand) & ((1 << *NB_TESTS_PLAYED(p))-1))
            continue;

        /* test */
        b = NB_TESTS_PLAYED(p);
        (*b)++;
        if (!BMK_seed(winners, p, srcBuffer, srcSize, ctx, dctx)) continue;

        /* improvement found => search more */
        BMK_printWinners(f, winners, srcSize);
        playAround(f, winners, p, srcBuffer, srcSize, ctx, dctx);
    }

}


static ZSTD_compressionParameters randomParams(void)
{
    ZSTD_compressionParameters p;
    U32 validated = 0;
    while (!validated) {
        /* totally random entry */
        p.chainLog   = (FUZ_rand(&g_rand) % (ZSTD_CHAINLOG_MAX+1 - ZSTD_CHAINLOG_MIN))         + ZSTD_CHAINLOG_MIN;
        p.hashLog    = (FUZ_rand(&g_rand) % (ZSTD_HASHLOG_MAX+1 - ZSTD_HASHLOG_MIN))           + ZSTD_HASHLOG_MIN;
        p.searchLog  = (FUZ_rand(&g_rand) % (ZSTD_SEARCHLOG_MAX+1 - ZSTD_SEARCHLOG_MIN))       + ZSTD_SEARCHLOG_MIN;
        p.windowLog  = (FUZ_rand(&g_rand) % (ZSTD_WINDOWLOG_MAX+1 - ZSTD_WINDOWLOG_MIN))       + ZSTD_WINDOWLOG_MIN;
        p.searchLength=(FUZ_rand(&g_rand) % (ZSTD_SEARCHLENGTH_MAX+1 - ZSTD_SEARCHLENGTH_MIN)) + ZSTD_SEARCHLENGTH_MIN;
        p.targetLength=(FUZ_rand(&g_rand) % (512));
        p.strategy   = (ZSTD_strategy) (FUZ_rand(&g_rand) % (ZSTD_btultra +1));
        validated = !ZSTD_isError(ZSTD_checkCParams(p));
        //validated = cParamValid(p);
    }
    return p;
}

/* Sets pc to random unmeasured set of parameters */
static void randomConstrainedParams(ZSTD_compressionParameters* pc, U32* varArray, int varLen, U8* memoTable)
{
    int tries = memoTableLen(varArray, varLen); //configurable, 
    const size_t maxSize = memoTableLen(varArray, varLen);
    size_t ind;
    do {
        ind = (FUZ_rand(&g_rand)) % maxSize;
        tries--;
    } while(memoTable[ind] > 0 && tries > 0); 

    memoTableIndInv(pc, varArray, varLen, (unsigned)ind);
    *pc = sanitizeParams(*pc);
}

static void BMK_selectRandomStart(
                       FILE* f, winnerInfo_t* winners,
                       const void* srcBuffer, size_t srcSize,
                       ZSTD_CCtx* ctx, ZSTD_DCtx* dctx)
{
    U32 const id = FUZ_rand(&g_rand) % (NB_LEVELS_TRACKED+1);
    if ((id==0) || (winners[id].params.windowLog==0)) {
        /* use some random entry */
        ZSTD_compressionParameters const p = ZSTD_adjustCParams(randomParams(), srcSize, 0);
        playAround(f, winners, p, srcBuffer, srcSize, ctx, dctx);
    } else {
        playAround(f, winners, winners[id].params, srcBuffer, srcSize, ctx, dctx);
    }
}


static void BMK_benchOnce(ZSTD_CCtx* cctx, ZSTD_DCtx* dctx, const void* srcBuffer, size_t srcSize)
{
    BMK_result_t testResult;
    g_params = ZSTD_adjustCParams(g_params, srcSize, 0);
    BMK_benchParam1(&testResult, srcBuffer, srcSize, cctx, dctx, g_params);
    DISPLAY("Compression Ratio: %.3f  Compress Speed: %.1f MB/s Decompress Speed: %.1f MB/s\n", (double)srcSize / testResult.cSize, 
        testResult.cSpeed / 1000000, testResult.dSpeed / 1000000);
    return;
}

static void BMK_benchFullTable(ZSTD_CCtx* cctx, ZSTD_DCtx* dctx, const void* srcBuffer, size_t srcSize)
{
    ZSTD_compressionParameters params;
    winnerInfo_t winners[NB_LEVELS_TRACKED+1];
    const char* const rfName = "grillResults.txt";
    FILE* const f = fopen(rfName, "w");
    const size_t blockSize = g_blockSize ? g_blockSize : srcSize;   /* cut by block or not ? */

    /* init */
    assert(g_singleRun==0);
    memset(winners, 0, sizeof(winners));
    if (f==NULL) { DISPLAY("error opening %s \n", rfName); exit(1); }

    if (g_target) {
        BMK_init_level_constraints(g_target*1000000);
    } else {
        /* baseline config for level 1 */
        ZSTD_compressionParameters const l1params = ZSTD_getCParams(1, blockSize, 0);
        BMK_result_t testResult;
        BMK_benchParam1(&testResult, srcBuffer, srcSize, cctx, dctx, l1params);
        BMK_init_level_constraints((int)((testResult.cSpeed * 31) / 32));
    }

    /* populate initial solution */
    {   const int maxSeeds = g_noSeed ? 1 : ZSTD_maxCLevel();
        int i;
        for (i=0; i<=maxSeeds; i++) {
            params = ZSTD_getCParams(i, blockSize, 0);
            BMK_seed(winners, params, srcBuffer, srcSize, cctx, dctx);
    }   }
    BMK_printWinners(f, winners, srcSize);

    /* start tests */
    {   const time_t grillStart = time(NULL);
        do {
            BMK_selectRandomStart(f, winners, srcBuffer, srcSize, cctx, dctx);
        } while (BMK_timeSpan(grillStart) < g_grillDuration_s);
    }

    /* end summary */
    BMK_printWinners(f, winners, srcSize);
    DISPLAY("grillParams operations completed \n");

    /* clean up*/
    fclose(f);
}

static void BMK_benchMem_usingCCtx(ZSTD_CCtx* const cctx, ZSTD_DCtx* const dctx, const void* srcBuffer, size_t srcSize)
{
    if (g_singleRun)
        return BMK_benchOnce(cctx, dctx, srcBuffer, srcSize);
    else
        return BMK_benchFullTable(cctx, dctx, srcBuffer, srcSize);
}

static void BMK_benchMemCCtxInit(const void* srcBuffer, size_t srcSize)
{
    ZSTD_CCtx* const cctx = ZSTD_createCCtx();
    ZSTD_DCtx* const dctx = ZSTD_createDCtx();
    if (cctx==NULL || dctx==NULL) { DISPLAY("Context Creation failed \n"); exit(1); }
    BMK_benchMem_usingCCtx(cctx, dctx, srcBuffer, srcSize);
    ZSTD_freeCCtx(cctx);
}


static int benchSample(void)
{
    const char* const name = "Sample 10MB";
    size_t const benchedSize = 10000000;

    void* origBuff = malloc(benchedSize);
    if (!origBuff) { perror("not enough memory"); return 12; }

    /* Fill buffer */
    RDG_genBuffer(origBuff, benchedSize, g_compressibility, 0.0, 0);

    /* bench */
    DISPLAY("\r%79s\r", "");
    DISPLAY("using %s %i%%: \n", name, (int)(g_compressibility*100));
    BMK_benchMemCCtxInit(origBuff, benchedSize);

    free(origBuff);
    return 0;
}


/* benchFiles() :
 * note: while this function takes a table of filenames,
 * in practice, only the first filename will be used */
int benchFiles(const char** fileNamesTable, int nbFiles)
{
    int fileIdx=0;

    /* Loop for each file */
    while (fileIdx<nbFiles) {
        const char* const inFileName = fileNamesTable[fileIdx++];
        FILE* const inFile = fopen( inFileName, "rb" );
        U64 const inFileSize = UTIL_getFileSize(inFileName);
        size_t benchedSize;
        void* origBuff;

        /* Check file existence */
        if (inFile==NULL) {
            DISPLAY( "Pb opening %s\n", inFileName);
            return 11;
        }
        if (inFileSize == UTIL_FILESIZE_UNKNOWN) {
            DISPLAY("Pb evaluating size of %s \n", inFileName);
            fclose(inFile);
            return 11;
        }

        /* Memory allocation */
        benchedSize = BMK_findMaxMem(inFileSize*3) / 3;
        if ((U64)benchedSize > inFileSize) benchedSize = (size_t)inFileSize;
        if (benchedSize < inFileSize)
            DISPLAY("Not enough memory for '%s' full size; testing %i MB only...\n", inFileName, (int)(benchedSize>>20));
        origBuff = malloc(benchedSize);
        if (origBuff==NULL) {
            DISPLAY("\nError: not enough memory!\n");
            fclose(inFile);
            return 12;
        }

        /* Fill input buffer */
        DISPLAY("Loading %s...       \r", inFileName);
        {   size_t const readSize = fread(origBuff, 1, benchedSize, inFile);
            fclose(inFile);
            if(readSize != benchedSize) {
                DISPLAY("\nError: problem reading file '%s' !!    \n", inFileName);
                free(origBuff);
                return 13;
        }   }

        /* bench */
        DISPLAY("\r%79s\r", "");
        DISPLAY("using %s : \n", inFileName);
        BMK_benchMemCCtxInit(origBuff, benchedSize);

        /* clean */
        free(origBuff);
    }

    return 0;
}



/* 
checks feasibility with uncertainty. 
-1 : certainly infeasible
 0 : uncertain
 1 : certainly feasible
*/
static int uncertainFeasibility(double const uncertaintyConstantC, double const uncertaintyConstantD, const constraint_t paramTarget, const BMK_result_t* const results) {
    if((paramTarget.cSpeed != 0 && results->cSpeed * uncertaintyConstantC < paramTarget.cSpeed) ||
       (paramTarget.dSpeed != 0 && results->dSpeed * uncertaintyConstantD < paramTarget.dSpeed) ||
       (paramTarget.cMem != 0 && results->cMem > paramTarget.cMem)) {
        return -1;
    } else if((paramTarget.cSpeed == 0 || results->cSpeed / uncertaintyConstantC > paramTarget.cSpeed) && 
        (paramTarget.dSpeed == 0 || results->dSpeed / uncertaintyConstantD > paramTarget.dSpeed) &&
        (paramTarget.cMem == 0 || results->cMem <= paramTarget.cMem)) {
        return 1;
    } else {
        return 0;
    }
}

/* 1 - better than prev best 
   0 - uncertain
   -1 - worse 
   assume prev_best status is run fully? 
   but then we'd have to rerun any winners anyway */
/* not as useful as initially believed */
static int uncertainComparison(double const uncertaintyConstantC, double const uncertaintyConstantD, const BMK_result_t* candidate, const BMK_result_t* prevBest) {
    (void)uncertaintyConstantD; //unused for now
    if(candidate->cSpeed > prevBest->cSpeed * uncertaintyConstantC) {
        return 1;
    } else if (candidate->cSpeed * uncertaintyConstantC < prevBest->cSpeed) {
        return -1;
    } else {
        return 0;
    }
}

/*benchmarks and tests feasibility together
 1 = true = better
 0 = false = not better
 if true then resultPtr will give results.
 2+ on error? */

//Maybe use compress_only for benchmark
#define INFEASIBLE_RESULT 0
#define FEASIBLE_RESULT 1
#define ERROR_RESULT 2
static int feasibleBench(BMK_result_t* resultPtr,
                const void* const * const srcPtrs, size_t const * const srcSizes,
                void** const dstPtrs, size_t* dstCapacityToSizes, U32 const nbBlocks, 
               void* dictBuffer, const size_t dictBufferSize, 
               ZSTD_CCtx* ctx, ZSTD_DCtx* dctx, 
               const ZSTD_compressionParameters cParams,
               const constraint_t target,
               BMK_result_t* winnerResult) {
    BMK_return_t benchres;
    U64 loopDurationC = 0, loopDurationD = 0;
    double uncertaintyConstantC, uncertaintyConstantD;
    size_t srcSize = 0;
    U32 i;
    //alternative - test 1 iter for ratio, (possibility of error 3 which is fine),
    //maybe iter this until 2x measurable for better guarantee? 
    DEBUGOUTPUT("Feas:\n");
    benchres = BMK_benchMemInvertible(srcPtrs, srcSizes, dstPtrs, dstCapacityToSizes, nbBlocks, 0, &cParams, dictBuffer, dictBufferSize, ctx, dctx,
                        BMK_both, BMK_iterMode, 1);
    if(benchres.error) {
        DISPLAY("ERROR %d !!\n", benchres.error);
    }
    for(i = 0; i < nbBlocks; i++) {
        srcSize += srcSizes[i];
    }
    BMK_printWinner(stdout, CUSTOM_LEVEL, benchres.result, cParams, srcSize);

    if(!benchres.error) { 
        *resultPtr = benchres.result;
        /* if speed is 0 (only happens when time = 0) */
        if(eqZero(benchres.result.cSpeed)) {
            loopDurationC = 0;
            uncertaintyConstantC = 2;
        } else {
            loopDurationC = ((srcSize * TIMELOOP_NANOSEC) / benchres.result.cSpeed); 
            //problem - tested in fullbench, saw speed vary 3x between iters, maybe raise uncertaintyConstraint up? 
            //possibly has to do with initCCtx? or system stuff?
            //asymmetric +/- constant needed? 
            uncertaintyConstantC = MIN((loopDurationC + (double)(2 * g_clockGranularity)/loopDurationC) * 1.1, 3); //.02 seconds 
        }
        if(eqZero(benchres.result.dSpeed)) {
            loopDurationD = 0;
            uncertaintyConstantD = 2;
        } else {
            loopDurationD = ((srcSize * TIMELOOP_NANOSEC) / benchres.result.dSpeed); 
            //problem - tested in fullbench, saw speed vary 3x between iters, maybe raise uncertaintyConstraint up? 
            //possibly has to do with initCCtx? or system stuff?
            //asymmetric +/- constant needed? 
            uncertaintyConstantD = MIN((loopDurationD + (double)(2 * g_clockGranularity)/loopDurationD) * 1.1, 3); //.02 seconds 
        }


        if(benchres.result.cSize < winnerResult->cSize) { //better compression ratio, just needs to be feasible
            int feas;
            if(loopDurationC < TIMELOOP_NANOSEC / 10) {
                BMK_return_t benchres2 = BMK_benchMemInvertible(srcPtrs, srcSizes, dstPtrs, dstCapacityToSizes, nbBlocks, 0, &cParams, dictBuffer, dictBufferSize, ctx, dctx,
                        BMK_compressOnly, BMK_iterMode, 1);
                if(benchres2.error) {
                    return ERROR_RESULT;
                } else {
                    benchres = benchres2;
                }
            }
            if(loopDurationD < TIMELOOP_NANOSEC / 10) {
                BMK_return_t benchres2 = BMK_benchMemInvertible((const void* const*)dstPtrs, dstCapacityToSizes, NULL, NULL, nbBlocks, 0, &cParams, dictBuffer, dictBufferSize, ctx, dctx,
                        BMK_decodeOnly, BMK_iterMode, 1);
                if(benchres2.error) {
                    return ERROR_RESULT;
                } else {
                    benchres.result.dSpeed = benchres2.result.dSpeed;
                }
            }
            *resultPtr = benchres.result;

            feas = uncertainFeasibility(uncertaintyConstantC, uncertaintyConstantD, target, &(benchres.result));
            if(feas == 0) { // uncertain feasibility
                if(loopDurationC < TIMELOOP_NANOSEC) {
                    BMK_return_t benchres2 = BMK_benchMemInvertible(srcPtrs, srcSizes, dstPtrs, dstCapacityToSizes, nbBlocks, 0, &cParams, dictBuffer, dictBufferSize, ctx, dctx,
                        BMK_compressOnly, BMK_timeMode, 1);
                    if(benchres2.error) {
                        return ERROR_RESULT;
                    } else {
                        benchres.result.cSpeed = benchres2.result.cSpeed;
                    }
                } 
                if(loopDurationD < TIMELOOP_NANOSEC) {
                    BMK_return_t benchres2 = BMK_benchMemInvertible((const void* const*)dstPtrs, dstCapacityToSizes, NULL, NULL, nbBlocks, 0, &cParams, dictBuffer, dictBufferSize, ctx, dctx,
                        BMK_decodeOnly, BMK_timeMode, 1);
                    if(benchres2.error) {
                        return ERROR_RESULT;
                    } else {
                        benchres.result.dSpeed = benchres2.result.dSpeed;
                    }
                }
                *resultPtr = benchres.result;
                return feasible(benchres.result, target);
            } else { //feas = 1 or -1 map to 1, 0 respectively
                return (feas + 1) >> 1; //relies on INFEASIBLE_RESULT == 0, FEASIBLE_RESULT == 1
            }
        } else if (benchres.result.cSize == winnerResult->cSize) { //equal ratio, needs to be better than winner in cSpeed/ dSpeed / cMem
            int feas;
            if(loopDurationC < TIMELOOP_NANOSEC / 10) {
                BMK_return_t benchres2 = BMK_benchMemInvertible(srcPtrs, srcSizes, dstPtrs, dstCapacityToSizes, nbBlocks, 0, &cParams, dictBuffer, dictBufferSize, ctx, dctx,
                    BMK_compressOnly, BMK_iterMode, 1);
                if(benchres2.error) {
                    return ERROR_RESULT;
                } else {
                    benchres = benchres2;
                }
            }
            if(loopDurationD < TIMELOOP_NANOSEC / 10) {
                BMK_return_t benchres2 = BMK_benchMemInvertible((const void* const*)dstPtrs, dstCapacityToSizes, NULL, NULL, nbBlocks, 0, &cParams, dictBuffer, dictBufferSize, ctx, dctx,
                    BMK_decodeOnly, BMK_iterMode, 1);
                if(benchres2.error) {
                    return ERROR_RESULT;
                } else {
                    benchres.result.dSpeed = benchres2.result.dSpeed;
                }
            }
            feas = uncertainFeasibility(uncertaintyConstantC, uncertaintyConstantD, target, &(benchres.result));
            if(feas == 0) { // uncertain feasibility
                if(loopDurationC < TIMELOOP_NANOSEC) {
                    BMK_return_t benchres2 = BMK_benchMemInvertible(srcPtrs, srcSizes, dstPtrs, dstCapacityToSizes, nbBlocks, 0, &cParams, dictBuffer, dictBufferSize, ctx, dctx,
                        BMK_compressOnly, BMK_timeMode, 1);
                    if(benchres2.error) {
                        return ERROR_RESULT;
                    } else {
                        benchres.result.cSpeed = benchres2.result.cSpeed;
                    }
                } 
                if(loopDurationD < TIMELOOP_NANOSEC) {
                    BMK_return_t benchres2 = BMK_benchMemInvertible((const void* const*)dstPtrs, dstCapacityToSizes, NULL, NULL, nbBlocks, 0, &cParams, dictBuffer, dictBufferSize, ctx, dctx,
                        BMK_decodeOnly, BMK_timeMode, 1);
                    if(benchres2.error) {
                        return ERROR_RESULT;
                    } else {
                        benchres.result.dSpeed = benchres2.result.dSpeed;
                    }
                }

                *resultPtr = benchres.result;
                return feasible(benchres.result, target) && objective_lt(*winnerResult, benchres.result);
            } else if (feas == 1) { //no need to check feasibility compares (maybe only it is chosen as a winner)
                int btw = uncertainComparison(uncertaintyConstantC, uncertaintyConstantD, &(benchres.result), winnerResult);
                if(btw == -1) {
                    return INFEASIBLE_RESULT;
                } else { //possibly better, benchmark and find out
                    benchres = BMK_benchMemInvertible(srcPtrs, srcSizes, dstPtrs, dstCapacityToSizes, nbBlocks, 0, &cParams, dictBuffer, dictBufferSize, ctx, dctx,
                        BMK_both, BMK_timeMode, 1);
                    *resultPtr = benchres.result;
                    return objective_lt(*winnerResult, benchres.result);
                }
            } else { //feas == -1
                return INFEASIBLE_RESULT; //infeasible
            }
        } else {
            return INFEASIBLE_RESULT; //infeasible
        }
    } else {
        return ERROR_RESULT; //BMK error
    }
}

//same as before, but +/-? 
//alternative, just return comparison result, leave caller to worry about feasibility.
//have version of benchMemAdvanced which takes in dstBuffer/cap as well? 
//(motivation: repeat tests (maybe just on decompress) don't need further compress runs) 
static int infeasibleBench(BMK_result_t* resultPtr,
                const void* const * const srcPtrs, size_t const * const srcSizes,
                void** const dstPtrs, size_t* dstCapacityToSizes, U32 const nbBlocks, 
                void* dictBuffer, const size_t dictBufferSize, 
                ZSTD_CCtx* ctx, ZSTD_DCtx* dctx, 
                const ZSTD_compressionParameters cParams,
                const constraint_t target,
                BMK_result_t* winnerResult) {
    BMK_return_t benchres;
    BMK_result_t resultMin, resultMax;
    U64 loopDurationC = 0, loopDurationD = 0;
    double uncertaintyConstantC, uncertaintyConstantD;
    double winnerRS;
    size_t srcSize = 0;
    U32 i;

    benchres = BMK_benchMemInvertible(srcPtrs, srcSizes, dstPtrs, dstCapacityToSizes, nbBlocks, 0, &cParams, dictBuffer, dictBufferSize, ctx, dctx,
                        BMK_both, BMK_iterMode, 1);
    for(i = 0; i < nbBlocks; i++) {
        srcSize += srcSizes[i];
    }
    BMK_printWinner(stdout, CUSTOM_LEVEL, benchres.result, cParams, srcSize);
    winnerRS = resultScore(*winnerResult, srcSize, target);

    DEBUGOUTPUT("WinnerScore: %f\n ", winnerRS);

    if(!benchres.error) { 
         *resultPtr = benchres.result;
        if(eqZero(benchres.result.cSpeed)) {
            loopDurationC = 0;
            uncertaintyConstantC = 2;
        } else {
            loopDurationC = ((srcSize * TIMELOOP_NANOSEC) / benchres.result.cSpeed); 
            uncertaintyConstantC = MIN((loopDurationC + (double)(2 * g_clockGranularity)/loopDurationC * 1.1), 3); //.02 seconds 
        }

        if(eqZero(benchres.result.dSpeed)) {
            loopDurationD = 0;
            uncertaintyConstantD = 2;
        } else {
            loopDurationD = ((srcSize * TIMELOOP_NANOSEC) / benchres.result.dSpeed); 
            uncertaintyConstantD = MIN((loopDurationD + (double)(2 * g_clockGranularity)/loopDurationD) * 1.1 , 3); //.02 seconds 
        }


        if(loopDurationC < TIMELOOP_NANOSEC / 10) {
            BMK_return_t benchres2 = BMK_benchMemInvertible(srcPtrs, srcSizes, dstPtrs, dstCapacityToSizes, nbBlocks, 0, &cParams, dictBuffer, dictBufferSize, ctx, dctx,
                        BMK_compressOnly, BMK_iterMode, 1);
            if(benchres2.error) {
                return ERROR_RESULT;
            } else {
                benchres = benchres2;
            }
        }
        if(loopDurationD < TIMELOOP_NANOSEC / 10) {
            BMK_return_t benchres2 = BMK_benchMemInvertible((const void* const*)dstPtrs, dstCapacityToSizes, NULL, NULL, nbBlocks, 0, &cParams, dictBuffer, dictBufferSize, ctx, dctx,
                        BMK_decodeOnly, BMK_iterMode, 1);
            if(benchres2.error) {
                return ERROR_RESULT;
            } else {
                benchres.result.dSpeed = benchres2.result.dSpeed;
            }
        }
        *resultPtr = benchres.result;

        /* benchres's certainty range. */
        resultMax = benchres.result;
        resultMin = benchres.result;
        resultMax.cSpeed *= uncertaintyConstantC;
        resultMax.dSpeed *= uncertaintyConstantD;
        resultMin.cSpeed /= uncertaintyConstantC;
        resultMin.dSpeed /= uncertaintyConstantD;
        if (winnerRS > resultScore(resultMax, srcSize, target)) {
            return INFEASIBLE_RESULT; 
        } else {
            if(loopDurationC < TIMELOOP_NANOSEC) {
                BMK_return_t benchres2 = BMK_benchMemInvertible(srcPtrs, srcSizes, dstPtrs, dstCapacityToSizes, nbBlocks, 0, &cParams, dictBuffer, dictBufferSize, ctx, dctx,
                        BMK_compressOnly, BMK_timeMode, 1);
                if(benchres2.error) {
                    return ERROR_RESULT;
                } else {
                    benchres.result.cSpeed = benchres2.result.cSpeed;
                }
            } 
            if(loopDurationD < TIMELOOP_NANOSEC) {
                BMK_return_t benchres2 = BMK_benchMemInvertible((const void* const*)dstPtrs, dstCapacityToSizes, NULL, NULL, nbBlocks, 0, &cParams, dictBuffer, dictBufferSize, ctx, dctx,
                        BMK_decodeOnly, BMK_timeMode, 1);
                if(benchres2.error) {
                    return ERROR_RESULT;
                } else {
                    benchres.result.dSpeed = benchres2.result.dSpeed;
                }
            }
            *resultPtr = benchres.result;
            return (resultScore(benchres.result, srcSize, target) > winnerRS);
        }

    *resultPtr = benchres.result;
    } else {
        return ERROR_RESULT; //BMK error
    }

}

/* wrap feasibleBench w/ memotable */
#define INFEASIBLE_THRESHOLD 200
static int feasibleBenchMemo(BMK_result_t* resultPtr,
               const void* srcBuffer, const size_t srcSize,
               void* dstBuffer, const size_t dstSize, 
               void* dictBuffer, const size_t dictSize, 
               const size_t* fileSizes, const size_t nbFiles, 
               ZSTD_CCtx* ctx, ZSTD_DCtx* dctx, 
               const ZSTD_compressionParameters cParams,
               const constraint_t target,
               BMK_result_t* winnerResult, U8* memoTable,
               const U32* varyParams, const int varyLen) {

    const size_t memind = memoTableInd(&cParams, varyParams, varyLen);
    if(memoTable[memind] >= INFEASIBLE_THRESHOLD) {
        return INFEASIBLE_RESULT; 
    } else {
        const size_t blockSize = g_blockSize ? g_blockSize : srcSize;
        U32 const maxNbBlocks = (U32) ((srcSize + (blockSize-1)) / blockSize) + nbFiles;
        const void ** const srcPtrs = (const void** const)malloc(maxNbBlocks * sizeof(void*));
        size_t* const srcSizes = (size_t* const)malloc(maxNbBlocks * sizeof(size_t));
        void ** const dstPtrs = (void** const)malloc(maxNbBlocks * sizeof(void*));
        size_t* const dstCapacities = (size_t* const)malloc(maxNbBlocks * sizeof(size_t));
        U32 nbBlocks;
        int res;

        if(!srcPtrs || !srcSizes || !dstPtrs || !dstCapacities) {
            free(srcPtrs);
            free(srcSizes);
            free(dstPtrs);
            free(dstCapacities);
            DISPLAY("Allocation Error\n");
            return ERROR_RESULT;
        }

        {   
            const char* srcPtr = (const char*)srcBuffer;
            char* dstPtr = (char*)dstBuffer;
            size_t dstSizeRemaining = dstSize;
            U32 fileNb;
            for (nbBlocks=0, fileNb=0; fileNb<nbFiles; fileNb++) {
                size_t remaining = fileSizes[fileNb];
                U32 const nbBlocksforThisFile = (U32)((remaining + (blockSize-1)) / blockSize);
                U32 const blockEnd = nbBlocks + nbBlocksforThisFile;
                if(remaining > dstSizeRemaining) {
                    DEBUGOUTPUT("Warning: dstSize too small to benchmark completely \n");
                    remaining = dstSizeRemaining;
                    dstSizeRemaining = 0;
                } else {
                    dstSizeRemaining -= remaining;
                }
                for ( ; nbBlocks<blockEnd; nbBlocks++) {
                    size_t const thisBlockSize = MIN(remaining, blockSize);
                    srcPtrs[nbBlocks] = (const void*)srcPtr;
                    srcSizes[nbBlocks] = thisBlockSize;
                    dstPtrs[nbBlocks] = (void*)dstPtr;
                    dstCapacities[nbBlocks] = ZSTD_compressBound(thisBlockSize);
                    srcPtr += thisBlockSize;
                    dstPtr += dstCapacities[nbBlocks];
                }
                if(!dstSize) { break; }   
            }   
        }
        res = feasibleBench(resultPtr, srcPtrs, srcSizes, dstPtrs, dstCapacities, nbBlocks, dictBuffer, dictSize, ctx, dctx, 
               cParams, target, winnerResult);
        memoTable[memind] = 255; //tested are all infeasible (other possible values for opti)
        free(srcPtrs);
        free(srcSizes);
        free(dstPtrs);
        free(dstCapacities);
        return res;
    }
}

//should infeasible stage searching also be memo-marked in the same way? 
//don't actually memoize unless result is feasible/error? 
static int infeasibleBenchMemo(BMK_result_t* resultPtr,
               const void* srcBuffer, const size_t srcSize,
               void* dstBuffer, const size_t dstSize, 
               void* dictBuffer, const size_t dictSize, 
               const size_t* fileSizes, const size_t nbFiles, 
               ZSTD_CCtx* ctx, ZSTD_DCtx* dctx, 
               const ZSTD_compressionParameters cParams,
               const constraint_t target,
               BMK_result_t* winnerResult, U8* memoTable,
               const U32* varyParams, const int varyLen) {
    size_t memind = memoTableInd(&cParams, varyParams, varyLen);
    if(memoTable[memind] >= INFEASIBLE_THRESHOLD) {
        return INFEASIBLE_RESULT; //see feasibleBenchMemo for concerns
    } else {
        const size_t blockSize = g_blockSize ? g_blockSize : srcSize;
        U32 const maxNbBlocks = (U32) ((srcSize + (blockSize-1)) / blockSize) + nbFiles;
        const void ** const srcPtrs = (const void** const)malloc(maxNbBlocks * sizeof(void*));
        size_t* const srcSizes = (size_t* const)malloc(maxNbBlocks * sizeof(size_t));
        void ** const dstPtrs = (void** const)malloc(maxNbBlocks * sizeof(void*));
        size_t* const dstCapacities = (size_t* const)malloc(maxNbBlocks * sizeof(size_t));
        U32 nbBlocks;
        int res;

        if(!srcPtrs || !srcSizes || !dstPtrs || !dstCapacities) {
            free(srcPtrs);
            free(srcSizes);
            free(dstPtrs);
            free(dstCapacities);
            DISPLAY("Allocation Error\n");
            return ERROR_RESULT;
        }

        {   
            const char* srcPtr = (const char*)srcBuffer;
            char* dstPtr = (char*)dstBuffer;
            size_t dstSizeRemaining = dstSize;
            U32 fileNb;
            for (nbBlocks=0, fileNb=0; fileNb<nbFiles; fileNb++) {
                size_t remaining = fileSizes[fileNb];
                U32 const nbBlocksforThisFile = (U32)((remaining + (blockSize-1)) / blockSize);
                U32 const blockEnd = nbBlocks + nbBlocksforThisFile;
                if(remaining > dstSizeRemaining) {
                    DEBUGOUTPUT("Warning: dstSize too small to benchmark completely \n");
                    remaining = dstSizeRemaining;
                    dstSizeRemaining = 0;
                } else {
                    dstSizeRemaining -= remaining;
                }
                for ( ; nbBlocks<blockEnd; nbBlocks++) {
                    size_t const thisBlockSize = MIN(remaining, blockSize);
                    srcPtrs[nbBlocks] = (const void*)srcPtr;
                    srcSizes[nbBlocks] = thisBlockSize;
                    dstPtrs[nbBlocks] = (void*)dstPtr;
                    dstCapacities[nbBlocks] = ZSTD_compressBound(thisBlockSize);
                    srcPtr += thisBlockSize;
                    dstPtr += dstCapacities[nbBlocks];
                }
                if(!dstSize) { break; }   
            }   
        }

        res = infeasibleBench(resultPtr, srcPtrs, srcSizes, dstPtrs, dstCapacities, nbBlocks, dictBuffer, dictSize, ctx, dctx, 
               cParams, target, winnerResult);
        if(res == FEASIBLE_RESULT) {
            memoTable[memind] = 255; 
        }
        free(srcPtrs);
        free(srcSizes);
        free(dstPtrs);
        free(dstCapacities);
        return res;
    }
}

/* specifically feasibleBenchMemo and infeasibleBenchMemo */
//maybe not necessary 
typedef int (*BMK_benchMemo_t)(BMK_result_t*, const void*, size_t, void*, size_t, ZSTD_CCtx*, ZSTD_DCtx*, 
    const ZSTD_compressionParameters, const constraint_t, BMK_result_t*, U8*, U32*, const int);

//varArray should be sanitized when this is called.
//possibility climb is infeasible, responsibility of caller to check that. but if something feasible is evaluated, it will be returned
// *actually if it performs too 
//sanitize all params here. 
//all generation after random should be sanitized. (maybe sanitize random)
static winnerInfo_t climbOnce(const constraint_t target, 
                const U32* varArray, const int varLen, U8* memoTable,
                const void* srcBuffer, size_t srcSize, 
                void* dstBuffer, const size_t dstSize, 
                void* dictBuffer, const size_t dictSize, 
                const size_t* fileSizes, const size_t nbFiles, 
                ZSTD_CCtx* ctx, ZSTD_DCtx* dctx, 
                const ZSTD_compressionParameters init) {
    //pick later initializations non-randomly? high dist from explored nodes.
    //how to do this efficiently? (might not be too much of a problem, happens rarely, running time probably dominated by benchmarking)
    //distance maximizing selection? 
    //cparam - currently considered center
    //candidate - params to benchmark/results
    //winner - best option found so far.
    ZSTD_compressionParameters cparam = init;
    winnerInfo_t candidateInfo, winnerInfo;
    int better = 1;

    winnerInfo.params = init;
    winnerInfo.result.cSpeed = 0;
    winnerInfo.result.dSpeed = 0;
    winnerInfo.result.cMem = (size_t)-1;
    winnerInfo.result.cSize = (size_t)-1;

    /* ineasible -> (hopefully) feasible */
    /* when nothing is found, this garbages part 2. */
    {
        winnerInfo_t bestFeasible1; /* uses feasibleBench Metric */

        //init these params 
        bestFeasible1.params = cparam;
        bestFeasible1.result.cSpeed = 0;
        bestFeasible1.result.dSpeed = 0;
        bestFeasible1.result.cMem = (size_t)-1;
        bestFeasible1.result.cSize = (size_t)-1;
        DISPLAY("Climb Part 1\n");
        while(better) {

            int i, d;
            better = 0;
            DEBUGOUTPUT("Start\n");
            cparam = winnerInfo.params;
            BMK_printWinner(stdout, CUSTOM_LEVEL, winnerInfo.result, winnerInfo.params, srcSize);
            candidateInfo.params = cparam;
            //all dist-1 targets
            //if we early end this, we should also randomize the order these are picked. 
            for(i = 0; i < varLen; i++) {
                paramVaryOnce(varArray[i], 1, &candidateInfo.params); /* +1 */
                candidateInfo.params = sanitizeParams(candidateInfo.params);
                //evaluate
                if(!ZSTD_isError(ZSTD_checkCParams(candidateInfo.params))) {
                //if(cParamValid(candidateInfo.params)) {
                    int res = infeasibleBenchMemo(&candidateInfo.result,
                       srcBuffer, srcSize,
                       dstBuffer, dstSize, 
                       dictBuffer, dictSize, 
                       fileSizes, nbFiles, 
                       ctx, dctx, 
                       candidateInfo.params, target, &winnerInfo.result, memoTable,
                       varArray, varLen);
                    if(res == FEASIBLE_RESULT) { /* synonymous with better when called w/ infeasibleBM */
                        winnerInfo = candidateInfo;
                        BMK_printWinner(stdout, CUSTOM_LEVEL, winnerInfo.result, winnerInfo.params, srcSize);
                        better = 1;
                        if(feasible(candidateInfo.result, target) && objective_lt(bestFeasible1.result, winnerInfo.result)) {
                            bestFeasible1 = winnerInfo;
                        }
                    }
                }
                candidateInfo.params = cparam;
                paramVaryOnce(varArray[i], -1, &candidateInfo.params); /* -1 */
                candidateInfo.params = sanitizeParams(candidateInfo.params);
                //evaluate
                if(!ZSTD_isError(ZSTD_checkCParams(candidateInfo.params))) {
                //if(cParamValid(candidateInfo.params)) {
                    int res = infeasibleBenchMemo(&candidateInfo.result,
                       srcBuffer, srcSize,
                       dstBuffer, dstSize, 
                       dictBuffer, dictSize, 
                       fileSizes, nbFiles, 
                       ctx, dctx, 
                       candidateInfo.params, target, &winnerInfo.result, memoTable,
                       varArray, varLen);
                    if(res == FEASIBLE_RESULT) { 
                        winnerInfo = candidateInfo;
                        BMK_printWinner(stdout, CUSTOM_LEVEL, winnerInfo.result, winnerInfo.params, srcSize);
                        better = 1;
                        if(feasible(candidateInfo.result, target) && objective_lt(bestFeasible1.result, winnerInfo.result)) {
                            bestFeasible1 = winnerInfo;
                        }
                    }
                }
            }

            if(better) {
                continue;
            }
            //if 'better' enough, skip further parameter search, center there?
            //possible improvement - guide direction here w/ knowledge rather than completely random variation. 
            for(d = 2; d < varLen + 2; d++) { /* varLen is # dimensions */
                for(i = 0; i < 2 * varLen + 2; i++) {
                    int res;
                    candidateInfo.params = cparam;
                    /* param error checking already done here */
                    paramVariation(&candidateInfo.params, varArray, varLen, d);
                    res = infeasibleBenchMemo(&candidateInfo.result,
                       srcBuffer, srcSize,
                       dstBuffer, dstSize,
                       dictBuffer, dictSize, 
                       fileSizes, nbFiles, 
                       ctx, dctx, 
                       candidateInfo.params, target, &winnerInfo.result, memoTable,
                       varArray, varLen);
                    if(res == FEASIBLE_RESULT) { /* synonymous with better in this case*/
                        winnerInfo = candidateInfo;
                        BMK_printWinner(stdout, CUSTOM_LEVEL, winnerInfo.result, winnerInfo.params, srcSize);
                        better = 1;
                        if(feasible(candidateInfo.result, target) && objective_lt(bestFeasible1.result, winnerInfo.result)) {
                            bestFeasible1 = winnerInfo;
                        }
                    }

                }
                if(better) {
                    continue;
                }
            }
            //bias to test previous delta? 
            //change cparam -> candidate before restart
        }
        winnerInfo = bestFeasible1;
    }

    //break out if no feasible. 
    if(winnerInfo.result.cMem == (U32)-1) {
        DEBUGOUTPUT("No Feasible Found\n");
        return winnerInfo;
    }
    DISPLAY("Climb Part 2\n");

    better = 1;
    /* feasible -> best feasible (hopefully) */
    {
        while(better) { 
            int i, d;
            better = 0;
            BMK_printWinner(stdout, CUSTOM_LEVEL, winnerInfo.result, winnerInfo.params, srcSize);
            //all dist-1 targets
            cparam = winnerInfo.params; 
            candidateInfo.params = cparam;
            for(i = 0; i < varLen; i++) {
                paramVaryOnce(varArray[i], 1, &candidateInfo.params);
                candidateInfo.params = sanitizeParams(candidateInfo.params);

                //evaluate
                if(!ZSTD_isError(ZSTD_checkCParams(candidateInfo.params))) {
                //if(cParamValid(candidateInfo.params)) {
                    int res = feasibleBenchMemo(&candidateInfo.result,
                       srcBuffer, srcSize,
                       dstBuffer, dstSize, 
                       dictBuffer, dictSize, 
                       fileSizes, nbFiles, 
                       ctx, dctx, 
                       candidateInfo.params, target, &winnerInfo.result, memoTable,
                       varArray, varLen);
                    if(res == FEASIBLE_RESULT) {
                        winnerInfo = candidateInfo;
                        BMK_printWinner(stdout, CUSTOM_LEVEL, winnerInfo.result, winnerInfo.params, srcSize);
                        better = 1;
                    }
                }
                candidateInfo.params = cparam;
                paramVaryOnce(varArray[i], -1, &candidateInfo.params);
                candidateInfo.params = sanitizeParams(candidateInfo.params);
                //evaluate
                if(!ZSTD_isError(ZSTD_checkCParams(candidateInfo.params))) {
                    int res = feasibleBenchMemo(&candidateInfo.result,
                       srcBuffer, srcSize,
                       dstBuffer, dstSize, 
                       dictBuffer, dictSize, 
                       fileSizes, nbFiles, 
                       ctx, dctx, 
                       candidateInfo.params, target, &winnerInfo.result, memoTable,
                       varArray, varLen);
                    if(res == FEASIBLE_RESULT) {
                        winnerInfo = candidateInfo;
                        BMK_printWinner(stdout, CUSTOM_LEVEL, winnerInfo.result, winnerInfo.params, srcSize);
                        better = 1;
                    }
                }
            } 
            //if 'better' enough, skip further parameter search, center there?
            //possible improvement - guide direction here w/ knowledge rather than completely random variation. 
            for(d = 2; d < varLen + 2; d++) { /* varLen is # dimensions */
                for(i = 0; i < 2 * varLen + 2; i++) {
                    int res;
                    candidateInfo.params = cparam;
                    /* param error checking already done here */
                    paramVariation(&candidateInfo.params, varArray, varLen, d); //info candidateInfo.params is garbage, this is too.
                    res = feasibleBenchMemo(&candidateInfo.result,
                       srcBuffer, srcSize,
                       dstBuffer, dstSize, 
                       dictBuffer, dictSize, 
                       fileSizes, nbFiles, 
                       ctx, dctx, 
                       candidateInfo.params, target, &winnerInfo.result, memoTable,
                       varArray, varLen);
                    if(res == FEASIBLE_RESULT) {
                        winnerInfo = candidateInfo;
                        BMK_printWinner(stdout, CUSTOM_LEVEL, winnerInfo.result, winnerInfo.params, srcSize);
                        better = 1;
                    }
                }
                if(better) {
                    continue;
                }
            }
            //bias to test previous delta? 
            //change cparam -> candidate before restart
        }
    }

    return winnerInfo;
}

//optimizeForSize but with fixed strategy
//place to configure/filter out strategy specific parameters.
//need args for all buffers and parameter stuff
//sanitization here. 

//flexible parameters: iterations of (failed?) climbing (or if we do non-random, maybe this is when everything is close to visitied)
//weight more on visit for bad results, less on good results/more on later results / ones with more failures.
//allocate memoTable here. 
//only real use for paramTarget is to get the fixed values, right? 
static winnerInfo_t optimizeFixedStrategy(
    const void* srcBuffer, const size_t srcSize, 
    void* dstBuffer, const size_t dstSize, 
    void* dictBuffer, const size_t dictSize, 
    const size_t* fileSizes, const size_t nbFiles, 
    const constraint_t target, ZSTD_compressionParameters paramTarget, 
    const ZSTD_strategy strat, const U32* varArray, const int varLen, U8* memoTable) {
    int i = 0;
    U32* varNew = malloc(sizeof(U32) * varLen);
    int varLenNew = sanitizeVarArray(varLen, varArray, varNew, strat);
    ZSTD_compressionParameters init;
    ZSTD_CCtx* ctx = ZSTD_createCCtx();
    ZSTD_DCtx* dctx = ZSTD_createDCtx();
    winnerInfo_t winnerInfo, candidateInfo; 
    winnerInfo.result.cSpeed = 0;
    winnerInfo.result.dSpeed = 0;
    winnerInfo.result.cMem = (size_t)(-1LL);
    winnerInfo.result.cSize = (size_t)(-1LL);
    /* so climb is given the right fixed strategy */
    paramTarget.strategy = strat;
    /* to pass ZSTD_checkCParams */

    //needs to happen after memoTableInit as that assumes 0 = undefined. 
    cParamZeroMin(&paramTarget);

    init = paramTarget;


    if(!ctx || !dctx || !memoTable || !varNew) {
        DISPLAY("NOT ENOUGH MEMORY ! ! ! \n");
        goto _cleanUp;
    }

    while(i < 10) { //make i adjustable (user input?) depending on how much time they have. 
        DEBUGOUTPUT("Restart\n"); 
        //look into improving this to maximize distance from searched infeasible stuff / towards promising regions? 
        randomConstrainedParams(&init, varNew, varLenNew, memoTable);
        candidateInfo = climbOnce(target, varNew, varLenNew, memoTable, srcBuffer, srcSize, dstBuffer, dstSize, dictBuffer, dictSize, fileSizes, nbFiles, ctx, dctx, init);
        if(objective_lt(winnerInfo.result, candidateInfo.result)) {
            winnerInfo = candidateInfo;
            DISPLAY("New Winner: ");
            BMK_printWinner(stdout, CUSTOM_LEVEL, winnerInfo.result, winnerInfo.params, srcSize);
            i = 0;
        }
        i++;
    }

_cleanUp:
    ZSTD_freeCCtx(ctx);
    ZSTD_freeDCtx(dctx);
    free(varNew);
    return winnerInfo;
}

static int BMK_loadFiles(void* buffer, size_t bufferSize,
                          size_t* fileSizes, const char* const * const fileNamesTable, 
                          unsigned nbFiles)
{
    size_t pos = 0, totalSize = 0;
    unsigned n;
    for (n=0; n<nbFiles; n++) {
        FILE* f;
        U64 fileSize = UTIL_getFileSize(fileNamesTable[n]);
        if (UTIL_isDirectory(fileNamesTable[n])) {
            DISPLAY("Ignoring %s directory...       \n", fileNamesTable[n]);
            fileSizes[n] = 0;
            continue;
        }
        if (fileSize == UTIL_FILESIZE_UNKNOWN) {
            DISPLAY("Cannot evaluate size of %s, ignoring ... \n", fileNamesTable[n]);
            fileSizes[n] = 0;
            continue;
        }
        f = fopen(fileNamesTable[n], "rb");
        if (f==NULL) {
            DISPLAY("impossible to open file %s", fileNamesTable[n]);
            return 10;
        }
        DISPLAY("Loading %s...       \r", fileNamesTable[n]);
        if (fileSize > bufferSize-pos) fileSize = bufferSize-pos, nbFiles=n;   /* buffer too small - stop after this file */
        { size_t const readSize = fread(((char*)buffer)+pos, 1, (size_t)fileSize, f);
            if (readSize != (size_t)fileSize) {
                DISPLAY("could not read %s", fileNamesTable[n]);
                return 11;
            }
          pos += readSize; }
        fileSizes[n] = (size_t)fileSize;
        totalSize += (size_t)fileSize;
        fclose(f);
    }

    if (totalSize == 0) { DISPLAY("\nno data to bench\n"); return 12; }
    return 0;
}

//goes best, best-1, best+1, best-2, ...
//return 0 if nothing remaining
static int nextStrategy(const int currentStrategy, const int bestStrategy) {
    if(bestStrategy <= currentStrategy) {
        int candidate = 2 * bestStrategy - currentStrategy - 1;
        if(candidate < 1) {
            candidate = currentStrategy + 1;
            if(candidate > (int)ZSTD_btultra) {
                return 0;
            } else {
                return candidate;
            }
        } else {
            return candidate;
        }
    } else { /* bestStrategy >= currentStrategy */
        int candidate = 2 * bestStrategy - currentStrategy;
        if(candidate > (int)ZSTD_btultra) {
            candidate = currentStrategy - 1;
            if(candidate < 1) {
                return 0;
            } else {
                return candidate;
            }
        } else {
            return candidate;
        }
    }
}

//optimize fixed strategy. 
static int optimizeForSize(const char* const * const fileNamesTable, const size_t nbFiles, const char* dictFileName, constraint_t target, ZSTD_compressionParameters paramTarget, int cLevel)
{
    size_t benchedSize;
    void* origBuff = NULL;
    void* dictBuffer = NULL;
    size_t dictBufferSize = 0;
    U32 varArray [NUM_PARAMS];
    int ret = 0;
    size_t* fileSizes = calloc(sizeof(size_t),nbFiles);
    const int varLen = variableParams(paramTarget, varArray);
    U8** allMT = NULL;
    g_winner.result.cSize = (size_t)-1;
    /* Init */
    if(!cParamValid(paramTarget)) {
        return 10;
    }

    /* load dictionary*/
    if (dictFileName != NULL) {
        U64 const dictFileSize = UTIL_getFileSize(dictFileName);
        if (dictFileSize > 64 MB) {
            DISPLAY("dictionary file %s too large", dictFileName);
            ret = 10;
            goto _cleanUp;
        }
        dictBufferSize = (size_t)dictFileSize;
        dictBuffer = malloc(dictBufferSize);
        if (dictBuffer==NULL) {
            DISPLAY("not enough memory for dictionary (%u bytes)",
                            (U32)dictBufferSize);
            ret = 11;
            goto _cleanUp;

        }

        {
            int errorCode = BMK_loadFiles(dictBuffer, dictBufferSize, &dictBufferSize, &dictFileName, 1);
            if(errorCode) {
                ret = errorCode;
                goto _cleanUp;
            }
        }
    }

    /* Fill input buffer */
    if(nbFiles == 1) {
        DISPLAY("Loading %s...       \r", fileNamesTable[0]);
    } else {
        DISPLAY("Loading %lu Files...       \r", (unsigned long)nbFiles); 
    }

    {   
        U64 const totalSizeToLoad = UTIL_getTotalFileSize(fileNamesTable, nbFiles);
        int ec;
        unsigned i;
        benchedSize = BMK_findMaxMem(totalSizeToLoad * 3) / 3;

        origBuff = malloc(benchedSize);
        if(!origBuff || !fileSizes) {
            DISPLAY("Not enough memory for stuff\n");
            ret = 1;
            goto _cleanUp;
        }
        ec = BMK_loadFiles(origBuff, benchedSize, fileSizes, fileNamesTable, nbFiles);
        if(ec) {
            DISPLAY("Error Loading Files");
            ret = ec;
            goto _cleanUp;
        }
        benchedSize = 0;
        for(i = 0; i < nbFiles; i++) {
            benchedSize += fileSizes[i];
        }
        origBuff = realloc(origBuff, benchedSize);
    }

    allMT = memoTableInitAll(paramTarget, target, varArray, varLen, benchedSize);
    if(!allMT) {
        ret = 2;
        goto _cleanUp;
    }

    //TODO: cLevel Stuff. 
    if(cLevel) {
        BMK_result_t candidate;
        const size_t blockSize = g_blockSize ? g_blockSize : benchedSize;
        ZSTD_CCtx* const ctx = ZSTD_createCCtx();
        ZSTD_DCtx* const dctx = ZSTD_createDCtx();
        ZSTD_compressionParameters const CParams = ZSTD_getCParams(cLevel, blockSize, dictBufferSize);
        if(BMK_benchParam(&candidate, origBuff, benchedSize, fileSizes, nbFiles, ctx, dctx, CParams)) {
            ZSTD_freeCCtx(ctx);
            ZSTD_freeDCtx(dctx);
            ret = 3;
            goto _cleanUp;
        }

        target.cSpeed = candidate.cSpeed; //TODO: maybe have a small bit of slack here, like x.99?  
        target.dSpeed = candidate.dSpeed;
        BMK_printWinner(stdout, cLevel, candidate, CParams, benchedSize);

        ZSTD_freeCCtx(ctx);
        ZSTD_freeDCtx(dctx);
    }

    g_targetConstraints = target;

    /* bench */
    DISPLAY("\r%79s\r", "");
    if(nbFiles == 1) {
        DISPLAY("optimizing for %s", fileNamesTable[0]);
    } else {
        DISPLAY("optimizing for %lu Files", (unsigned long)nbFiles);
    }
    if(target.cSpeed != 0) { DISPLAY(" - limit compression speed %u MB/s", target.cSpeed / 1000000); }
    if(target.dSpeed != 0) { DISPLAY(" - limit decompression speed %u MB/s", target.dSpeed / 1000000); }
    if(target.cMem != (U32)-1) { DISPLAY(" - limit memory %u MB", target.cMem / 1000000); }
    DISPLAY("\n");
    findClockGranularity();

    {   ZSTD_CCtx* const ctx = ZSTD_createCCtx();
        ZSTD_DCtx* const dctx = ZSTD_createDCtx();
        winnerInfo_t winner;
        U32 varNew[NUM_PARAMS];
        const size_t blockSize = g_blockSize ? g_blockSize : benchedSize;
        U32 const maxNbBlocks = (U32) ((benchedSize + (blockSize-1)) / blockSize) + 1;
        const size_t maxCompressedSize = ZSTD_compressBound(benchedSize) + (maxNbBlocks * 1024);
        void* compressedBuffer = malloc(maxCompressedSize);

        /* init */
        if (ctx==NULL) { DISPLAY("\n ZSTD_createCCtx error \n"); free(origBuff); return 14;}
        if(compressedBuffer==NULL) { DISPLAY("\n Allocation Error \n"); free(origBuff); free(ctx); return 15; }
        memset(&winner, 0, sizeof(winner));
        winner.result.cSize = (size_t)(-1);


        /* find best solution from default params */
        {
            /* strategy selection */
            const int maxSeeds = g_noSeed ? 1 : ZSTD_maxCLevel();
            DEBUGOUTPUT("Strategy Selection\n");
            if(varLen == NUM_PARAMS && paramTarget.strategy == 0) { /* no variable based constraints */  
                BMK_result_t candidate;
                int feas = 0, i;
                for (i=1; i<=maxSeeds; i++) {
                    ZSTD_compressionParameters const CParams = ZSTD_getCParams(i, blockSize, dictBufferSize);
                    int ec = BMK_benchParam(&candidate, origBuff, benchedSize, fileSizes, nbFiles, ctx, dctx, CParams);
                    BMK_printWinner(stdout, i, candidate, CParams, benchedSize);

                    if(!ec) {
                        if(feas) {
                            if(feasible(candidate, relaxTarget(target)) && objective_lt(winner.result, candidate)) {
                                winner.result = candidate;
                                winner.params = CParams;
                            }
                        } else {
                            if(feasible(candidate, relaxTarget(target))) {
                                feas = 1;
                                winner.result = candidate;
                                winner.params = CParams;

                            } else {
                                if(resultScore(candidate, benchedSize, target) > resultScore(winner.result, benchedSize, target)) {
                                    winner.result = candidate;
                                    winner.params = CParams;
                                }
                            }
                        }
                    }
                } //best, -1, +1, ..., 

            } else if (paramTarget.strategy == 0) { //constrained
                int feas = 0, i, j; 
                for(j = 1; j < 10; j++) {
                    for(i = 1; i <= maxSeeds; i++) {
                        int varLenNew = sanitizeVarArray(varLen, varArray, varNew, i);
                        ZSTD_compressionParameters candidateParams = paramTarget;
                        BMK_result_t candidate;
                        int ec;
                        randomConstrainedParams(&candidateParams, varNew, varLenNew, allMT[i]);
                        cParamZeroMin(&candidateParams);
                        candidateParams = sanitizeParams(candidateParams);
                        ec = BMK_benchParam(&candidate, origBuff, benchedSize, fileSizes, nbFiles, ctx, dctx, candidateParams);
                        
                        if(!ec) {
                            if(feas) {
                                if(feasible(candidate, relaxTarget(target)) && objective_lt(winner.result, candidate)) {
                                    winner.result = candidate;
                                    winner.params = candidateParams;
                                    BMK_printWinner(stdout, i, winner.result, winner.params, benchedSize);
                                }
                            } else {
                                if(feasible(candidate, relaxTarget(target))) {
                                    feas = 1;
                                    winner.result = candidate;
                                    winner.params = candidateParams;
                                    BMK_printWinner(stdout, i, winner.result, winner.params, benchedSize);

                                } else {
                                    if(resultScore(candidate, benchedSize, target) > resultScore(winner.result, benchedSize, target)) {
                                        winner.result = candidate;
                                        winner.params = candidateParams;
                                        BMK_printWinner(stdout, i, winner.result, winner.params, benchedSize);
                                    }
                                }
                            }
                        }

                    }
                }
            }
        }

        BMK_printWinner(stdout, CUSTOM_LEVEL, winner.result, winner.params, benchedSize);
        BMK_translateAdvancedParams(winner.params);
        DEBUGOUTPUT("Real Opt\n");
        /* start 'real' tests */
        {   
            int bestStrategy = (int)winner.params.strategy;
            if(paramTarget.strategy == 0) {
                int st = (int)winner.params.strategy;

                { 
                    int varLenNew = sanitizeVarArray(varLen, varArray, varNew, st);
                    winnerInfo_t w1 = climbOnce(target, varNew, varLenNew, allMT[st], 
                        origBuff, benchedSize, compressedBuffer, maxCompressedSize, dictBuffer, dictBufferSize, 
                        fileSizes, nbFiles, ctx, dctx, winner.params);
                    if(objective_lt(winner.result, w1.result)) {
                        winner = w1;
                    }
                }

                while(st) {
                    winnerInfo_t wc = optimizeFixedStrategy(origBuff, benchedSize, compressedBuffer, maxCompressedSize, dictBuffer, dictBufferSize, fileSizes, nbFiles, 
                    target, paramTarget, st, varArray, varLen, allMT[st]);
                    DEBUGOUTPUT("StratNum %d\n", st);
                    if(objective_lt(winner.result, wc.result)) {
                        winner = wc;
                    }
                    //We could double back to increase search of 'better' strategies 
                    st = nextStrategy(st, bestStrategy);
                }
            } else {
                winner = optimizeFixedStrategy(origBuff, benchedSize, compressedBuffer, maxCompressedSize, dictBuffer, dictBufferSize, fileSizes, nbFiles, 
                    target, paramTarget, paramTarget.strategy, varArray, varLen, allMT[paramTarget.strategy]);
            }

        }

        /* no solution found */
        if(winner.result.cSize == (size_t)-1) {
            DISPLAY("No feasible solution found\n");
            return 1;
        }
        /* end summary */
        BMK_printWinner(stdout, CUSTOM_LEVEL, winner.result, winner.params, benchedSize);
        BMK_translateAdvancedParams(winner.params);
        DISPLAY("grillParams size - optimizer completed \n");


        /* clean up*/
        ZSTD_freeCCtx(ctx);
        ZSTD_freeDCtx(dctx);

    }
_cleanUp: 
    free(fileSizes);
    free(dictBuffer);
    memoTableFreeAll(allMT);
    free(origBuff);
    return ret;
}

static void errorOut(const char* msg)
{
    DISPLAY("%s \n", msg); exit(1);
}

/*! readU32FromChar() :
 * @return : unsigned integer value read from input in `char` format.
 *  allows and interprets K, KB, KiB, M, MB and MiB suffix.
 *  Will also modify `*stringPtr`, advancing it to position where it stopped reading.
 *  Note : function will exit() program if digit sequence overflows */
static unsigned readU32FromChar(const char** stringPtr)
{
    const char errorMsg[] = "error: numeric value too large";
    unsigned result = 0;
    while ((**stringPtr >='0') && (**stringPtr <='9')) {
        unsigned const max = (((unsigned)(-1)) / 10) - 1;
        if (result > max) errorOut(errorMsg);
        result *= 10, result += **stringPtr - '0', (*stringPtr)++ ;
    }
    if ((**stringPtr=='K') || (**stringPtr=='M')) {
        unsigned const maxK = ((unsigned)(-1)) >> 10;
        if (result > maxK) errorOut(errorMsg);
        result <<= 10;
        if (**stringPtr=='M') {
            if (result > maxK) errorOut(errorMsg);
            result <<= 10;
        }
        (*stringPtr)++;  /* skip `K` or `M` */
        if (**stringPtr=='i') (*stringPtr)++;
        if (**stringPtr=='B') (*stringPtr)++;
    }
    return result;
}

static int usage(const char* exename)
{
    DISPLAY( "Usage :\n");
    DISPLAY( "      %s [arg] file\n", exename);
    DISPLAY( "Arguments :\n");
    DISPLAY( " file : path to the file used as reference (if none, generates a compressible sample)\n");
    DISPLAY( " -H/-h  : Help (this text + advanced options)\n");
    return 0;
}

static int usage_advanced(void)
{
    DISPLAY( "\nAdvanced options :\n");
    DISPLAY( " -T#    : set level 1 speed objective \n");
    DISPLAY( " -B#    : cut input into blocks of size # (default : single block) \n");
    DISPLAY( " -i#    : iteration loops [1-9](default : %i) \n", NBLOOPS);
    DISPLAY( " -O#    : find Optimized parameters for # MB/s compression speed (default : 0) \n");
    DISPLAY( " -S     : Single run \n");
    DISPLAY( " --zstd : Single run, parameter selection same as zstdcli \n");
    DISPLAY( " -P#    : generated sample compressibility (default : %.1f%%) \n", COMPRESSIBILITY_DEFAULT * 100);
    DISPLAY( " -t#    : Caps runtime of operation in seconds (default : %u seconds (%.1f hours)) \n", (U32)g_grillDuration_s, g_grillDuration_s / 3600);
    DISPLAY( " -v     : Prints Benchmarking output\n");
    DISPLAY( " -D     : Next argument dictionary file\n");
    return 0;
}

static int badusage(const char* exename)
{
    DISPLAY("Wrong parameters\n");
    usage(exename);
    return 1;
}

int main(int argc, const char** argv)
{
    int i,
        filenamesStart=0,
        result;
    const char* exename=argv[0];
    const char* input_filename = 0;
    const char* dictFileName = 0;
    U32 optimizer = 0;
    U32 main_pause = 0;
    int optimizerCLevel = 0;


    constraint_t target = { 0, 0, (U32)-1 }; //0 for anything unset
    ZSTD_compressionParameters paramTarget = { 0, 0, 0, 0, 0, 0, 0 };

    assert(argc>=1);   /* for exename */

    g_time = UTIL_getTime();

    /* Welcome message */
    DISPLAY(WELCOME_MESSAGE);

    for(i=1; i<argc; i++) {
        const char* argument = argv[i];
        assert(argument != NULL);

        if(!strcmp(argument,"--no-seed")) { g_noSeed = 1; continue; }

        if (longCommandWArg(&argument, "--optimize=")) {
            optimizer = 1;
            for ( ; ;) {
                if (longCommandWArg(&argument, "windowLog=") || longCommandWArg(&argument, "wlog=")) { paramTarget.windowLog = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "chainLog=") || longCommandWArg(&argument, "clog=")) { paramTarget.chainLog = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "hashLog=") || longCommandWArg(&argument, "hlog=")) { paramTarget.hashLog = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "searchLog=") || longCommandWArg(&argument, "slog=")) { paramTarget.searchLog = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "searchLength=") || longCommandWArg(&argument, "slen=")) { paramTarget.searchLength = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "targetLength=") || longCommandWArg(&argument, "tlen=")) { paramTarget.targetLength = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "strategy=") || longCommandWArg(&argument, "strat=")) { paramTarget.strategy = (ZSTD_strategy)(readU32FromChar(&argument)); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "compressionSpeed=") || longCommandWArg(&argument, "cSpeed=")) { target.cSpeed = readU32FromChar(&argument) * 1000000; if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "decompressionSpeed=") || longCommandWArg(&argument, "dSpeed=")) { target.dSpeed = readU32FromChar(&argument) * 1000000; if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "compressionMemory=") || longCommandWArg(&argument, "cMem=")) { target.cMem = readU32FromChar(&argument) * 1000000; if (argument[0]==',') { argument++; continue; } else break; }
                //TODO: add Level;
                /* in MB or MB/s */
                DISPLAY("invalid optimization parameter \n");
                return 1;
            }

            if (argument[0] != 0) {
                DISPLAY("invalid --optimize= format\n");
                return 1; /* check the end of string */
            }
            continue;
        } else if (longCommandWArg(&argument, "--zstd=")) {
        /* Decode command (note : aggregated commands are allowed) */
            g_singleRun = 1;
            g_params = ZSTD_getCParams(2, g_blockSize, 0);
            for ( ; ;) {
                if (longCommandWArg(&argument, "windowLog=") || longCommandWArg(&argument, "wlog=")) { g_params.windowLog = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "chainLog=") || longCommandWArg(&argument, "clog=")) { g_params.chainLog = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "hashLog=") || longCommandWArg(&argument, "hlog=")) { g_params.hashLog = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "searchLog=") || longCommandWArg(&argument, "slog=")) { g_params.searchLog = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "searchLength=") || longCommandWArg(&argument, "slen=")) { g_params.searchLength = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "targetLength=") || longCommandWArg(&argument, "tlen=")) { g_params.targetLength = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "strategy=") || longCommandWArg(&argument, "strat=")) { g_params.strategy = (ZSTD_strategy)(readU32FromChar(&argument)); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "level=") || longCommandWArg(&argument, "lvl=")) { g_params = ZSTD_getCParams(readU32FromChar(&argument), g_blockSize, 0); if (argument[0]==',') { argument++; continue; } else break; }
                DISPLAY("invalid compression parameter \n");
                return 1;
            }

            if (argument[0] != 0) {
                DISPLAY("invalid --zstd= format\n");
                return 1; /* check the end of string */
            }
            continue;
            /* if not return, success */
        } else if (argument[0]=='-') {
            argument++;

            while (argument[0]!=0) {

                switch(argument[0])
                {
                    /* Display help on usage */
                case 'h' :
                case 'H': usage(exename); usage_advanced(); return 0;

                    /* Pause at the end (hidden option) */
                case 'p': main_pause = 1; argument++; break;
                    /* Modify Nb Iterations */

                case 'i':
                    argument++;
                    g_nbIterations = readU32FromChar(&argument);
                    break;

                    /* Sample compressibility (when no file provided) */
                case 'P':
                    argument++;
                    {   U32 const proba32 = readU32FromChar(&argument);
                        g_compressibility = (double)proba32 / 100.;
                    }
                    break;

                case 'O':
                    argument++;
                    optimizer = 1;
                    for ( ; ; ) {
                        switch(*argument)
                        {
                        /* Inputs in MB or MB/s */
                        case 'C':
                            argument++;
                            target.cSpeed = readU32FromChar(&argument) * 1000000;
                            continue;
                        case 'D':
                            argument++;
                            target.dSpeed = readU32FromChar(&argument) * 1000000;
                            continue;
                        case 'M':
                            argument++;
                            target.cMem = readU32FromChar(&argument) * 1000000;
                            continue;
                        case 'w':
                            argument++;
                            paramTarget.windowLog = readU32FromChar(&argument);
                            continue;
                        case 'c':
                            argument++;
                            paramTarget.chainLog = readU32FromChar(&argument);
                            continue;
                        case 'h':
                            argument++;
                            paramTarget.hashLog = readU32FromChar(&argument);
                            continue;
                        case 's':
                            argument++;
                            paramTarget.searchLog = readU32FromChar(&argument);
                            continue;
                        case 'l':  /* search length */
                            argument++;
                            paramTarget.searchLength = readU32FromChar(&argument);
                            continue;
                        case 't':  /* target length */
                            argument++;
                            paramTarget.targetLength = readU32FromChar(&argument);
                            continue;
                        case 'S':  /* strategy */
                            argument++;
                            paramTarget.strategy = (ZSTD_strategy)readU32FromChar(&argument);
                            continue;
                        case 'L': /* level centers around a level */
                            argument++;
                            optimizerCLevel = (int)readU32FromChar(&argument);
                            continue;
                        default : ;
                        }
                        break;
                    }
                    break;

                    /* Run Single conf */
                case 'S':
                    g_singleRun = 1;
                    argument++;
                    g_params = ZSTD_getCParams(2, g_blockSize, 0);
                    for ( ; ; ) {
                        switch(*argument)
                        {
                        case 'w':
                            argument++;
                            g_params.windowLog = readU32FromChar(&argument);
                            continue;
                        case 'c':
                            argument++;
                            g_params.chainLog = readU32FromChar(&argument);
                            continue;
                        case 'h':
                            argument++;
                            g_params.hashLog = readU32FromChar(&argument);
                            continue;
                        case 's':
                            argument++;
                            g_params.searchLog = readU32FromChar(&argument);
                            continue;
                        case 'l':  /* search length */
                            argument++;
                            g_params.searchLength = readU32FromChar(&argument);
                            continue;
                        case 't':  /* target length */
                            argument++;
                            g_params.targetLength = readU32FromChar(&argument);
                            continue;
                        case 'S':  /* strategy */
                            argument++;
                            g_params.strategy = (ZSTD_strategy)readU32FromChar(&argument);
                            continue;
                        case 'L':
                            {   int const cLevel = readU32FromChar(&argument);
                                g_params = ZSTD_getCParams(cLevel, g_blockSize, 0);
                                continue;
                            }
                        default : ;
                        }
                        break;
                    }

                    break;

                    /* target level1 speed objective, in MB/s */
                case 'T':
                    argument++;
                    g_target = readU32FromChar(&argument);
                    break;

                    /* cut input into blocks */
                case 'B':
                    argument++;
                    g_blockSize = readU32FromChar(&argument);
                    DISPLAY("using %u KB block size \n", g_blockSize>>10);
                    break;

                    /* caps runtime (in seconds) */
                case 't':
                    argument++;
                    g_grillDuration_s = (double)readU32FromChar(&argument);
                    break;

                /* load dictionary file (only applicable for optimizer rn) */
                case 'D':
                    if(i == argc - 1) { //last argument, return error. 
                        DISPLAY("Dictionary file expected but not given\n");
                        return 1;
                    } else {
                        i++;
                        dictFileName = argv[i];
                    }
                    break;

                    /* Unknown command */
                default : return badusage(exename);
                }
            }
            continue;
        }   /* if (argument[0]=='-') */

        /* first provided filename is input */
        if (!input_filename) { input_filename=argument; filenamesStart=i; continue; }
    }

    if (filenamesStart==0) {
        if (optimizer) {
            DISPLAY("Optimizer Expects File\n");
            return 1;
        } else {
            result = benchSample();
        }
    } else {
        if (optimizer) {
            result = optimizeForSize(argv+filenamesStart, argc-filenamesStart, dictFileName, target, paramTarget, optimizerCLevel);
        } else {
            result = benchFiles(argv+filenamesStart, argc-filenamesStart);
    }   }

    if (main_pause) { int unused; printf("press enter...\n"); unused = getchar(); (void)unused; }

    return result;
}
