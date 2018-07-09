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


/*-************************************
*  Constants
**************************************/
#define PROGRAM_DESCRIPTION "ZSTD parameters tester"
#define AUTHOR "Yann Collet"
#define WELCOME_MESSAGE "*** %s %s %i-bits, by %s ***\n", PROGRAM_DESCRIPTION, ZSTD_VERSION_STRING, (int)(sizeof(void*)*8), AUTHOR


#define KB *(1<<10)
#define MB *(1<<20)
#define GB *(1ULL<<30)

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
#define ZSTD_TARGETLENGTH_MIN 0 //actually targeLengthlog min 
#define ZSTD_TARGETLENGTH_MAX 10

//#define ZSTD_TARGETLENGTH_MAX 1024
#define WLOG_RANGE (ZSTD_WINDOWLOG_MAX - ZSTD_WINDOWLOG_MIN + 1)
#define CLOG_RANGE (ZSTD_CHAINLOG_MAX - ZSTD_CHAINLOG_MIN + 1)
#define HLOG_RANGE (ZSTD_HASHLOG_MAX - ZSTD_HASHLOG_MIN + 1)
#define SLOG_RANGE (ZSTD_SEARCHLOG_MAX - ZSTD_SEARCHLOG_MIN + 1)
#define SLEN_RANGE (ZSTD_SEARCHLENGTH_MAX - ZSTD_SEARCHLENGTH_MIN + 1)
#define TLEN_RANGE 11
//hard coded since we only use powers of 2 (and 999 ~ 1024)

static const int mintable[NUM_PARAMS] = { ZSTD_WINDOWLOG_MIN, ZSTD_CHAINLOG_MIN, ZSTD_HASHLOG_MIN, ZSTD_SEARCHLOG_MIN, ZSTD_SEARCHLENGTH_MIN, ZSTD_TARGETLENGTH_MIN };
static const int maxtable[NUM_PARAMS] = { ZSTD_WINDOWLOG_MAX, ZSTD_CHAINLOG_MAX, ZSTD_HASHLOG_MAX, ZSTD_SEARCHLOG_MAX, ZSTD_SEARCHLENGTH_MAX, ZSTD_TARGETLENGTH_MAX };
static const int rangetable[NUM_PARAMS] = { WLOG_RANGE, CLOG_RANGE, HLOG_RANGE, SLOG_RANGE, SLEN_RANGE, TLEN_RANGE };

//use grid-search or something when space is small enough?
#define SMALL_SEARCH_SPACE 1000
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
    DISPLAY("Granularity: %llu\n", (unsigned long long)g_clockGranularity);
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
    paramTarget->targetLength = paramTarget->targetLength ? paramTarget->targetLength : 1;
}

static void BMK_translateAdvancedParams(ZSTD_compressionParameters params)
{
    DISPLAY("--zstd=windowLog=%u,chainLog=%u,hashLog=%u,searchLog=%u,searchLength=%u,targetLength=%u,strategy=%u \n",
             params.windowLog, params.chainLog, params.hashLog, params.searchLog, params.searchLength, params.targetLength, (U32)(params.strategy));
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

/* TODO: support additional parameters (more files, fileSizes) */
static size_t
BMK_benchParam(BMK_result_t* resultPtr,
               const void* srcBuffer, size_t srcSize,
               ZSTD_CCtx* ctx, ZSTD_DCtx* dctx, 
               const ZSTD_compressionParameters cParams) {

    BMK_return_t res = BMK_benchMem(srcBuffer,srcSize, &srcSize, 1, 0, &cParams, NULL, 0, ctx, dctx, 0, "File");
    *resultPtr = res.result;
    return res.error;
}

static void BMK_printWinner(FILE* f, U32 cLevel, BMK_result_t result, ZSTD_compressionParameters params, size_t srcSize)
{
    char lvlstr[15] = "Custom Level";
    DISPLAY("\r%79s\r", "");
    fprintf(f,"    {%3u,%3u,%3u,%3u,%3u,%3u, %s },  ",
            params.windowLog, params.chainLog, params.hashLog, params.searchLog, params.searchLength,
            params.targetLength, g_stratName[(U32)(params.strategy)]);
    if(cLevel != CUSTOM_LEVEL) {
        snprintf(lvlstr, 15, "  Level %2u  ", cLevel);
    }
    fprintf(f,
        "/* %s */   /* R:%5.3f at %5.1f MB/s - %5.1f MB/s */\n",
        lvlstr, (double)srcSize / result.cSize, result.cSpeed / 1000000., result.dSpeed / 1000000.);
}


typedef struct {
    BMK_result_t result;
    ZSTD_compressionParameters params;
} winnerInfo_t;

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

    BMK_benchParam(&testResult, srcBuffer, srcSize, ctx, dctx, params);


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
static int sanitizeVarArray(int varLength, U32* varArray, U32* varNew, ZSTD_strategy strat) {
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

/* computes inverse of above array, returns same number, -1 = unused ind */
static int inverseVariableParams(const ZSTD_compressionParameters paramConstraints, U32* res) {
    int j = 0;
    if(!paramConstraints.windowLog) {
        res[WLOG_IND] = j;
        j++;
    } else {
        res[WLOG_IND] = -1;
    }
    if(!paramConstraints.chainLog) {
        res[j] = CLOG_IND;
        j++;
    } else {
        res[CLOG_IND] = -1;
    }
    if(!paramConstraints.hashLog) {
        res[j] = HLOG_IND;
        j++;
    } else {
        res[HLOG_IND] = -1;
    }
    if(!paramConstraints.searchLog) {
        res[j] = SLOG_IND;
        j++;
    } else {
        res[SLOG_IND] = -1;
    }
    if(!paramConstraints.searchLength) {
        res[j] = SLEN_IND;
        j++;
    } else {
        res[SLEN_IND] = -1;
    }
    if(!paramConstraints.targetLength) {
        res[j] = TLEN_IND;
        j++;
    } else {
        res[TLEN_IND] = -1;
    }

    return j;
}

/* amt will probably always be \pm 1? */
/* slight change from old paramVariation, targetLength can only take on powers of 2 now (999 ~= 1024?) */
/* take max/min bounds into account as well? */
static void paramVaryOnce(U32 paramIndex, int amt, ZSTD_compressionParameters* ptr) {
    switch(paramIndex)
    {
        case WLOG_IND: ptr->windowLog    += amt; break;
        case CLOG_IND: ptr->chainLog     += amt; break;
        case HLOG_IND: ptr->hashLog      += amt; break;
        case SLOG_IND: ptr->searchLog    += amt; break;
        case SLEN_IND: ptr->searchLength += amt; break;
        case TLEN_IND: 
            if(amt >= 0) { 
                ptr->targetLength <<= amt; 
                ptr->targetLength = MIN(ptr->targetLength, 999);
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

//Don't fuzz fixed variables.
//turn pcs to pcs array with macro for params. 
//pass in variation array from variableParams
//take nbChanges as argument? 
static void paramVariation(ZSTD_compressionParameters* ptr, const U32* varyParams, const int varyLen, U32 nbChanges)
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
        //validated = cParamValid(p);

        //Make sure memory is at least close to feasible?
        //ZSTD_estimateCCtxSize thing.
    }
    *ptr = sanitizeParams(p);
}

//varyParams gives us table size? 
//1 per strategy
//varyParams should always be sorted smallest to largest
//take arrayLen to allocate memotable
//should be ~10^7 unconstrained. 
static size_t memoTableLen(const U32* varyParams, const int varyLen) {
    size_t arrayLen = 1;
    int i;
    for(i = 0; i < varyLen; i++) {
        arrayLen *= rangetable[varyParams[i]];
    }
    return arrayLen;
}

//sort of ~lg2 (replace 1024 w/ 999) for memoTableInd Tlen
static unsigned lg2(unsigned x) {
    unsigned j = 0;
    if(x == 999) {
        return 10;
    }
    while(x >>= 1) {
        j++;
    }
    return j;
}

//indexes compressionParameters into memotable
//of form 
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

/* presumably, the unfilled parameters are already at their correct value */
/* inverse above function for varyParams */
static void memoTableIndInv(ZSTD_compressionParameters* ptr, const U32* varyParams, const int varyLen, size_t ind) {
    int i;
    for(i = varyLen - 1; i >= 0; i--) {
        switch(varyParams[i]) {
            case WLOG_IND: ptr->windowLog    = ind % WLOG_RANGE + ZSTD_WINDOWLOG_MIN;    ind /= WLOG_RANGE; break;
            case CLOG_IND: ptr->chainLog     = ind % CLOG_RANGE + ZSTD_CHAINLOG_MIN;     ind /= CLOG_RANGE; break;
            case HLOG_IND: ptr->hashLog      = ind % HLOG_RANGE + ZSTD_HASHLOG_MIN;      ind /= HLOG_RANGE; break;
            case SLOG_IND: ptr->searchLog    = ind % SLOG_RANGE + ZSTD_SEARCHLOG_MIN;    ind /= SLOG_RANGE; break;
            case SLEN_IND: ptr->searchLength = ind % SLEN_RANGE + ZSTD_SEARCHLENGTH_MIN; ind /= SLEN_RANGE; break;
            case TLEN_IND: ptr->targetLength = MIN(1 << (ind % TLEN_RANGE), 999);        ind /= TLEN_RANGE; break;
        }
    }
}

//initializing memoTable
/* */
static void memoTableInit(U8* memoTable, ZSTD_compressionParameters paramConstraints, constraint_t target, const U32* varyParams, const int varyLen, const size_t srcSize) {
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
        if(ZSTD_estimateCCtxSize_usingCParams(paramConstraints) + (1 << paramConstraints.windowLog) > target.cMem) {
            //infeasible; 
            memoTable[i] = 255;
            j++;
        }
        //TODO: remove any where memoTable wlog is mark any where windowlog is too big for data. 
        if(wFixed && (1 << paramConstraints.windowLog) > (srcSize << 1)) {
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
    DISPLAY("%d / %d Invalid\n", j, (int)i);
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

//destructively modifies pc.
//Maybe if memoTable[ind] > 0 too often, count zeroes and explicitly choose from free stuff? 
//^ maybe this doesn't matter, with |mt| size it has \approx 1-(1/e) of finding even single free spot in |mt| tries, not too bad.
//TODO: maybe memoTable pc before sanitization too so no repeats? 
static void randomConstrainedParams(ZSTD_compressionParameters* pc, U32* varArray, int varLen, U8* memoTable)
{
    int tries = memoTableLen(varArray, varLen); //configurable, 
    const size_t maxSize = memoTableLen(varArray, varLen);
    size_t ind;
    do {
        ind = (FUZ_rand(&g_rand)) % maxSize;
        tries--;
    } while(memoTable[ind] > 0 && tries > 0); 
    //&& FUZ_rand(&g_rand) % 256 > memoTable[ind]); get nd choosing? (helpful w/ distance) /* maybe > infeasible bound? */

    /* memoTable[ind] == 0 -> unexplored */
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
    BMK_benchParam(&testResult, srcBuffer, srcSize, cctx, dctx, g_params);
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
        BMK_benchParam(&testResult, srcBuffer, srcSize, cctx, dctx, l1params);
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

//parameter feasibility is not checked, should just be restricted from use.
static int feasible(BMK_result_t results, constraint_t target) {
    return (results.cSpeed >= target.cSpeed) && (results.dSpeed >= target.dSpeed) && (results.cMem <= target.cMem || !target.cMem);
}

#define EPSILON 0.01
static int epsilonEqual(double c1, double c2) {
    return MAX(c1/c2,c2/c1) < 1 + EPSILON;
}

//so the compiler stops warning 
static int eqZero(double c1) {
    return (U64)c1 == (U64)0.0 || (U64)c1 == (U64)-0.0;
}

/* returns 1 if result2 is strictly 'better' than result1 */
/* strict comparison / cutoff based */
static int objective_lt(BMK_result_t result1, BMK_result_t result2) {
    return (result1.cSize > result2.cSize) || (epsilonEqual(result1.cSize, result2.cSize) && result2.cSpeed > result1.cSpeed)
    || (epsilonEqual(result1.cSize,result2.cSize) && epsilonEqual(result2.cSpeed, result1.cSpeed) && result2.dSpeed > result1.dSpeed);
}

//will probably be some linear combinartion of comp speed, decompSpeed, & ratio (maybe size), and memory?
//pretty arbitrary right now
//maybe better - higher coefficient when below threshold, lower when above
//need to normalize speed? or just use ratio speed / target? 
//Maybe don't use ratio at all when looking for feasibility? 

/* maybe dynamically vary the coefficients for this around based on what's already been discovered. (maybe make a reversed ratio cutoff?) concave to pheaily penalize below ratio? */
static double resultScore(BMK_result_t res, size_t srcSize, constraint_t target) {
    double cs = 0., ds = 0., rt, cm = 0.;
    const double r1 = 1, r2 = 0.1, rtr = 0.5;
    double ret;
    if(target.cSpeed) { cs = res.cSpeed / (double)target.cSpeed; }
    if(target.dSpeed) { ds = res.dSpeed / (double)target.dSpeed; }
    if(target.cMem != (U32)-1) { cm = (double)target.cMem / res.cMem; }
    rt = ((double)srcSize / res.cSize);

    //(void)rt;
    //(void)rtr;

    ret = (MIN(1, cs) + MIN(1, ds)  + MIN(1, cm))*r1 + rt * rtr + 
         (MAX(0, log(cs))+ MAX(0, log(ds))+ MAX(0, log(cm))) * r2;
    //DISPLAY("resultScore: %f\n", ret);
    return ret;
}

/*
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

*/
//ratio tradeoffs, may be useful in guiding

/* objective_lt, but based on scoring function */
static int objective_lt2(BMK_result_t result1, BMK_result_t result2, size_t srcSize, constraint_t target) {
    return resultScore(result1, srcSize, target) < resultScore(result2, srcSize, target);
}

/* res gives array dimensions, should be size NUM_PARAMS */ 
static size_t computeStateSize(const ZSTD_compressionParameters paramConstraints, U32* res) {
    int ind = 0;
    size_t base = 1;
    if(!paramConstraints.windowLog) { res[ind] = ZSTD_WINDOWLOG_MAX - ZSTD_WINDOWLOG_MIN + 1; base *= res[ind]; ind++; }
    if(!paramConstraints.chainLog) { res[ind] =  ZSTD_CHAINLOG_MAX - ZSTD_CHAINLOG_MIN + 1; base *= res[ind]; ind++; }
    if(!paramConstraints.hashLog) { res[ind] = ZSTD_HASHLOG_MAX - ZSTD_HASHLOG_MIN + 1; base *= res[ind]; ind++; }
    if(!paramConstraints.searchLog) { res[ind] = ZSTD_SEARCHLOG_MAX - ZSTD_SEARCHLOG_MIN + 1; base *= res[ind]; ind++; }
    if(!paramConstraints.searchLength) { res[ind] = ZSTD_SEARCHLENGTH_MAX - ZSTD_SEARCHLENGTH_MIN + 1; base *= res[ind]; ind++; }
    if(!paramConstraints.targetLength) { res[ind] = 11; base *= res[ind]; ind++; } //restricting from 2^[0,10], no such macros
    if(!(U32)paramConstraints.strategy) { res[ind] = 8; base *= 8; } //not strictly true, maybe would want to case on this. 

    return base;
}


static unsigned calcViolation(BMK_result_t results, constraint_t target) {
    int diffcSpeed = MAX(target.cSpeed - results.cSpeed, 0);
    int diffdSpeed = MAX(target.dSpeed - results.dSpeed, 0);
    int diffcMem = MAX(results.cMem - target.cMem, 0);
    return diffcSpeed + diffdSpeed + diffcMem;
}

/* 
uncertaintyConstant >= 1
returns -1 = 'certainly' infeasible
         0 = unceratin
         1 = 'certainly' feasible
*/
//paramTarget misnamed, should just be target
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
//presumably memory has already been compared, mostly worried about mem, cspeed, dspeed
//uncertainty only applies to speed. 
//if using objective fn, this could be much easier since we could just scale that. 
//difficult to make judgements about later parameters in prioritization type when there's
//uncertainty on the first. 
static int uncertainComparison(double const uncertaintyConstantC, double const uncertaintyConstantD, BMK_result_t* candidate, BMK_result_t* prevBest) {
    (void)uncertaintyConstantD; //unused for now
    if(candidate->cSpeed > prevBest->cSpeed * uncertaintyConstantC) {
        return 1;
    } else if (candidate->cSpeed * uncertaintyConstantC < prevBest->cSpeed) {
        return -1;
    } else {
        return 0;
    }
}

/* speed in b, srcSize in b/s loopDuration in ns */
//TODO: simplify code in feasibleBench with this instead of writing it all out.
//only applicable for single loop
static double calcUncertainty(double speed, size_t srcSize) {
    U64 loopDuration;
    if(eqZero(speed)) { return 2; }
    loopDuration = ((srcSize * TIMELOOP_NANOSEC) / speed);
    return MIN((loopDuration + (double)2 * g_clockGranularity) / loopDuration, 2);
}

//benchmarks and tests feasibility together
//1 = true = better
//0 = false = not better
//if true then resultPtr will give results.
//2+ on error? 
//alt: error = 0 / infeasible as well;
//maybe use compress_only mode for ratio-finding benchmark?
//prioritize ratio > cSpeed > dSpeed > cMem
//Misnamed - should be worse, better, error
//alternative (to make work for feasible-pt searching as well) - only compare to winner, not to target
//but then we need to judge what better means in this context, which shouldn't be the same (strict ratio improvement)
#define INFEASIBLE_RESULT 0
#define FEASIBLE_RESULT 1
#define ERROR_RESULT 2
static int feasibleBench(BMK_result_t* resultPtr,
               const void* srcBuffer, size_t srcSize,
               void* dstBuffer, size_t dstSize, 
               ZSTD_CCtx* ctx, ZSTD_DCtx* dctx, 
               const ZSTD_compressionParameters cParams,
               const constraint_t target,
               BMK_result_t* winnerResult) {
    BMK_advancedParams_t adv = BMK_initAdvancedParams();
    BMK_return_t benchres;
    U64 loopDurationC = 0, loopDurationD = 0;
    double uncertaintyConstantC, uncertaintyConstantD;
    adv.loopMode = BMK_iterMode;
    adv.nbSeconds = 1; //get ratio and 2x approx speed?

    //alternative - test 1 iter for ratio, (possibility of error 3 which is fine),
    //maybe iter this until 2x measurable for better guarantee? 
    DISPLAY("Feas:\n");
    benchres = BMK_benchMemAdvanced(srcBuffer,srcSize, dstBuffer, dstSize, &srcSize, 1, 0, &cParams, NULL, 0, ctx, dctx, 0, "File", &adv);
    if(benchres.error) {
        DISPLAY("ERROR %d !!\n", benchres.error);
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
            uncertaintyConstantC = MIN((loopDurationC + (double)(2 * g_clockGranularity)/loopDurationC), 2); //.02 seconds 
        }
        if(eqZero(benchres.result.dSpeed)) {
            loopDurationD = 0;
            uncertaintyConstantD = 2;
        } else {
            loopDurationD = ((srcSize * TIMELOOP_NANOSEC) / benchres.result.dSpeed); 
            //problem - tested in fullbench, saw speed vary 3x between iters, maybe raise uncertaintyConstraint up? 
            //possibly has to do with initCCtx? or system stuff?
            //asymmetric +/- constant needed? 
            uncertaintyConstantD = MIN((loopDurationD + (double)(2 * g_clockGranularity)/loopDurationD), 2); //.02 seconds 
        }


        if(benchres.result.cSize < winnerResult->cSize) { //better compression ratio, just needs to be feasible
            //optimistic assume speed
            //incoporate some sort of tradeoff comparison with the winner's results?
            int feas = uncertainFeasibility(uncertaintyConstantC, uncertaintyConstantD, target, &(benchres.result));
            if(feas == 0) { // uncertain feasibility
                adv.loopMode = BMK_timeMode;
                if(loopDurationC < TIMELOOP_NANOSEC) {
                    BMK_return_t benchres2;
                    adv.mode = BMK_compressOnly;
                    benchres2 = BMK_benchMemAdvanced(srcBuffer,srcSize, dstBuffer, dstSize, &srcSize, 1, 0, &cParams, NULL, 0, ctx, dctx, 0, "File", &adv);
                    if(benchres2.error) {
                        return ERROR_RESULT;
                    } else {
                        benchres.result.cSpeed = benchres2.result.cSpeed;
                    }
                } 
                if(loopDurationD < TIMELOOP_NANOSEC) {
                    BMK_return_t benchres2;
                    adv.mode = BMK_decodeOnly;
                    benchres2 = BMK_benchMemAdvanced(dstBuffer,dstSize, NULL, 0, &benchres.result.cSize, 1, 0, &cParams, NULL, 0, ctx, dctx, 0, "File", &adv);
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
            int feas = uncertainFeasibility(uncertaintyConstantC, uncertaintyConstantD, target, &(benchres.result));
            if(feas == 0) { // uncertain feasibility
                adv.loopMode = BMK_timeMode;
                if(loopDurationC < TIMELOOP_NANOSEC) {
                    BMK_return_t benchres2;
                    adv.mode = BMK_compressOnly;
                    benchres2 = BMK_benchMemAdvanced(srcBuffer,srcSize, dstBuffer, dstSize, &srcSize, 1, 0, &cParams, NULL, 0, ctx, dctx, 0, "File", &adv);
                    if(benchres2.error) {
                        return ERROR_RESULT;
                    } else {
                        benchres.result.cSpeed = benchres2.result.cSpeed;
                    }
                } 
                if(loopDurationD < TIMELOOP_NANOSEC) {
                    BMK_return_t benchres2;
                    adv.mode = BMK_decodeOnly;
                    benchres2 = BMK_benchMemAdvanced(dstBuffer,dstSize, NULL, 0, &benchres.result.cSize, 1, 0, &cParams, NULL, 0, ctx, dctx, 0, "File", &adv);
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
                    adv.loopMode = BMK_timeMode;
                    benchres = BMK_benchMemAdvanced(srcBuffer, srcSize, dstBuffer, dstSize, &srcSize, 1, 0, &cParams, NULL, 0, ctx, dctx, 0, "File", &adv);
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
//sameas before, but +/-? 
//alternative, just return comparison result, leave caller to worry about feasibility.
//have version of benchMemAdvanced which takes in dstBuffer/cap as well? 
//(motivation: repeat tests (maybe just on decompress) don't need further compress runs) 
static int infeasibleBench(BMK_result_t* resultPtr,
               const void* srcBuffer, size_t srcSize,
               void* dstBuffer, size_t dstSize, 
               ZSTD_CCtx* ctx, ZSTD_DCtx* dctx, 
               const ZSTD_compressionParameters cParams,
               const constraint_t target,
               BMK_result_t* winnerResult) {
    BMK_advancedParams_t adv = BMK_initAdvancedParams();
    BMK_return_t benchres;
    BMK_result_t resultMin, resultMax;
    U64 loopDurationC = 0, loopDurationD = 0;
    double uncertaintyConstantC, uncertaintyConstantD;
    double winnerRS = resultScore(*winnerResult, srcSize, target);
    adv.loopMode = BMK_iterMode; //can only use this for ratio measurement then, super inaccurate timing 
    adv.nbSeconds = 1; //get ratio and 2x approx speed? //maybe run until twice MIN(minloopinterval * clockDuration)

    DISPLAY("WinnerScore: %f\n ", winnerRS);
    /*
    adv.loopMode = BMK_timeMode;
    adv.nbSeconds = 1; */
    benchres = BMK_benchMemAdvanced(srcBuffer,srcSize, dstBuffer, dstSize, &srcSize, 1, 0, &cParams, NULL, 0, ctx, dctx, 0, "File", &adv);
    BMK_printWinner(stdout, CUSTOM_LEVEL, benchres.result, cParams, srcSize); 

    adv.loopMode = BMK_timeMode;
    adv.nbSeconds = 1;
    benchres = BMK_benchMemAdvanced(srcBuffer,srcSize, dstBuffer, dstSize, &srcSize, 1, 0, &cParams, NULL, 0, ctx, dctx, 0, "File", &adv);
    BMK_printWinner(stdout, CUSTOM_LEVEL, benchres.result, cParams, srcSize); 

    if(!benchres.error) { 
         *resultPtr = benchres.result;
        if(eqZero(benchres.result.cSpeed)) {
            loopDurationC = 0;
            uncertaintyConstantC = 2;
        } else {
            loopDurationC = ((srcSize * TIMELOOP_NANOSEC) / benchres.result.cSpeed); 
            //problem - tested in fullbench, saw speed vary 3x between iters, maybe raise uncertaintyConstraint up? 
            //possibly has to do with initCCtx? or system stuff?
            uncertaintyConstantC = MIN((loopDurationC + (double)(2 * g_clockGranularity)/loopDurationC), 2); //.02 seconds 
        }

        if(eqZero(benchres.result.dSpeed)) {
            loopDurationD = 0;
            uncertaintyConstantD = 2;
        } else {
            loopDurationD = ((srcSize * TIMELOOP_NANOSEC) / benchres.result.dSpeed); 
            //problem - tested in fullbench, saw speed vary 3x between iters, maybe raise uncertaintyConstraint up? 
            //possibly has to do with initCCtx? or system stuff?
            uncertaintyConstantD = MIN((loopDurationD + (double)(2 * g_clockGranularity)/loopDurationD), 2); //.02 seconds 
        }

        /* benchres's certainty range. */
        resultMax = benchres.result;
        resultMin = benchres.result;
        resultMax.cSpeed *= uncertaintyConstantC;
        resultMax.dSpeed *= uncertaintyConstantD;
        resultMin.cSpeed /= uncertaintyConstantC;
        resultMin.dSpeed /= uncertaintyConstantD;
        (void)resultMin;
        //TODO: consider if resultMin is actually needed. 
        if (winnerRS > resultScore(resultMax, srcSize, target)) {
            return INFEASIBLE_RESULT; 
        } else {
            //do this w/o copying / stuff
            adv.loopMode = BMK_timeMode;
            if(loopDurationC < TIMELOOP_NANOSEC) {
                BMK_return_t benchres2;
                adv.mode = BMK_compressOnly;
                benchres2 = BMK_benchMemAdvanced(srcBuffer,srcSize, dstBuffer, dstSize, &srcSize, 1, 0, &cParams, NULL, 0, ctx, dctx, 0, "File", &adv);
                if(benchres2.error) {
                    return ERROR_RESULT;
                } else {
                    benchres.result.cSpeed = benchres2.result.cSpeed;
                }
            } 
            if(loopDurationD < TIMELOOP_NANOSEC) {
                BMK_return_t benchres2;
                adv.mode = BMK_decodeOnly;
                //TODO: dstBuffer corrupted sometime between top and now
                //probably occuring in feasible bench too.
                benchres2 = BMK_benchMemAdvanced(dstBuffer, dstSize, NULL, 0, &benchres.result.cSize, 1, 0, &cParams, NULL, 0, ctx, dctx, 0, "File", &adv);
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
//TODO: void sanitized and unsanitized ver's so input doesn't double-choose
#define INFEASIBLE_THRESHOLD 200
static int feasibleBenchMemo(BMK_result_t* resultPtr,
               const void* srcBuffer, size_t srcSize,
               void* dstBuffer, size_t dstSize, 
               ZSTD_CCtx* ctx, ZSTD_DCtx* dctx, 
               const ZSTD_compressionParameters cParams,
               const constraint_t target,
               BMK_result_t* winnerResult, U8* memoTable,
               U32* varyParams, const int varyLen) {

    size_t memind = memoTableInd(&cParams, varyParams, varyLen);

    //BMK_translateAdvancedParams(cParams);
    if(memoTable[memind] >= INFEASIBLE_THRESHOLD) {
        return INFEASIBLE_RESULT; //probably pick a different code for already tested?
        //maybe remove this if we incorporate nonrandom location picking? 
        //what is the intended behavior in this case? 
        //ignore? stop iterating completely? other?
    } else {
        int res = feasibleBench(resultPtr, srcBuffer, srcSize, dstBuffer, dstSize, ctx, dctx, 
               cParams, target, winnerResult);
        memoTable[memind] = 255; //tested are all infeasible (other possible values for opti)
        return res;
    }
}

//should infeasible stage searching also be memo-marked in the same way? 
//don't actually memoize unless result is feasible/error? 
static int infeasibleBenchMemo(BMK_result_t* resultPtr,
               const void* srcBuffer, size_t srcSize,
               void* dstBuffer, size_t dstSize, 
               ZSTD_CCtx* ctx, ZSTD_DCtx* dctx, 
               const ZSTD_compressionParameters cParams,
               const constraint_t target,
               BMK_result_t* winnerResult, U8* memoTable,
               U32* varyParams, const int varyLen) {
    size_t memind = memoTableInd(&cParams, varyParams, varyLen);

    //BMK_translateAdvancedParams(cParams);
    if(memoTable[memind] >= INFEASIBLE_THRESHOLD) {
        return INFEASIBLE_RESULT; //see feasibleBenchMemo for concerns
    } else {
        int res = infeasibleBench(resultPtr, srcBuffer, srcSize, dstBuffer, dstSize, ctx, dctx, 
               cParams, target, winnerResult);
        if(res == FEASIBLE_RESULT) {
            memoTable[memind] = 255; //infeasible resultscores could still be normal feasible. 
        }
        return res;
    }
}

/* specifically feasibleBenchMemo and infeasibleBenchMemo */
//maybe not necessary 
typedef int (*BMK_benchMemo_t)(BMK_result_t*, const void*, size_t, void*, size_t, ZSTD_CCtx*, ZSTD_DCtx*, 
    const ZSTD_compressionParameters, const constraint_t, BMK_result_t*, U8*, U32*, const int);

//varArray should be sanitized when this is called.
//TODO: transition to simpler greedy method if evaluation time is too long? 
//would it be better to start at best feasible via feasible or infeasible metric? both? 
//possibility climb is infeasible, responsibility of caller to check that. but if something feasible is evaluated, it will be returned
// *actually if it performs too 
//sanitize all params here. 
//all generation after random should be sanitized. (maybe sanitize random)
static winnerInfo_t climbOnce(constraint_t target, U32* varArray, const int varLen, U8* memoTable,
    const void* srcBuffer, size_t srcSize, void* dstBuffer, size_t dstSize, ZSTD_CCtx* ctx, ZSTD_DCtx* dctx, ZSTD_compressionParameters init) {
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
        //TODO: initialize these values!
        winnerInfo_t bestFeasible1; /* uses feasibleBench Metric */
        winnerInfo_t bestFeasible2; /* uses resultScore Metric */

        //init these params 
        bestFeasible1.params = cparam;
        bestFeasible2.params = cparam;
        bestFeasible1.result.cSpeed = 0;
        bestFeasible1.result.dSpeed = 0;
        bestFeasible1.result.cMem = (size_t)-1;
        bestFeasible1.result.cSize = (size_t)-1;
        bestFeasible2.result.cSpeed = 0;
        bestFeasible2.result.dSpeed = 0;
        bestFeasible2.result.cMem = (size_t)-1;
        bestFeasible2.result.cSize = (size_t)-1;
        DISPLAY("Climb Part 1\n");
        while(better) {

            //UTIL_time_t timestart = UTIL_getTime(); TODO: adjust sampling based on time
            int i, d;
            better = 0;
            DISPLAY("Start\n");
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
                       ctx, dctx, 
                       candidateInfo.params, target, &winnerInfo.result, memoTable,
                       varArray, varLen);
                    if(res == FEASIBLE_RESULT) { /* synonymous with better when called w/ infeasibleBM */
                        winnerInfo = candidateInfo;
                        //BMK_printWinner(stdout, CUSTOM_LEVEL, winnerInfo.result, winnerInfo.params, srcSize);
                        better = 1;
                        if(feasible(candidateInfo.result, target)) {
                            bestFeasible2 = winnerInfo;
                            if(objective_lt(bestFeasible1.result, bestFeasible2.result)) { 
                                bestFeasible1 = bestFeasible2; /* using feasibleBench metric */
                            }
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
                       ctx, dctx, 
                       candidateInfo.params, target, &winnerInfo.result, memoTable,
                       varArray, varLen);
                    if(res == FEASIBLE_RESULT) { 
                        winnerInfo = candidateInfo;
                        //BMK_printWinner(stdout, CUSTOM_LEVEL, winnerInfo.result, winnerInfo.params, srcSize);
                        better = 1;
                        if(feasible(candidateInfo.result, target)) {
                            bestFeasible2 = winnerInfo;
                            if(objective_lt(bestFeasible1.result, bestFeasible2.result)) {
                                bestFeasible1 = bestFeasible2;
                            }
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
                for(i = 0; i < 5; i++) { //make ? relative to # of free dimensions.
                    int res;
                    candidateInfo.params = cparam;
                    /* param error checking already done here */
                    paramVariation(&candidateInfo.params, varArray, varLen, d);
                    res = infeasibleBenchMemo(&candidateInfo.result,
                       srcBuffer, srcSize,
                       dstBuffer, dstSize, 
                       ctx, dctx, 
                       candidateInfo.params, target, &winnerInfo.result, memoTable,
                       varArray, varLen);
                    if(res == FEASIBLE_RESULT) { /* synonymous with better in this case*/
                        winnerInfo = candidateInfo;
                        //BMK_printWinner(stdout, CUSTOM_LEVEL, winnerInfo.result, winnerInfo.params, srcSize);
                        better = 1;
                        if(feasible(candidateInfo.result, target)) {
                            bestFeasible2 = winnerInfo;
                            if(objective_lt(bestFeasible1.result, bestFeasible2.result)) {
                                bestFeasible1 = bestFeasible2;
                            }
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
        //TODO:Consider if this is best config. idea: explore from obj best keep rbest
        cparam = bestFeasible2.params;
        candidateInfo = bestFeasible2;
        winnerInfo = bestFeasible1;
    }

    //is it better to break here instead of bumbling about? 
    if(winnerInfo.result.cMem == (U32)-1) {
        DISPLAY("No Feasible Found\n");
        return winnerInfo;
    }
    DISPLAY("Climb Part 2\n");

    better = 1;
    /* feasible -> best feasible (hopefully) */
    {
        while(better) {

            //UTIL_time_t timestart = UTIL_getTime(); //TODO: if benchmarking is taking too long, be more greedy. 
            int i, d;
            better = 0;
            BMK_printWinner(stdout, CUSTOM_LEVEL, winnerInfo.result, winnerInfo.params, srcSize);
            //all dist-1 targets
            cparam = winnerInfo.params; //TODO: this messes the taking bestFeasible1, bestFeasible2 
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
                       ctx, dctx, 
                       candidateInfo.params, target, &winnerInfo.result, memoTable,
                       varArray, varLen);
                    if(res == FEASIBLE_RESULT) {
                        winnerInfo = candidateInfo;
                        //BMK_printWinner(stdout, CUSTOM_LEVEL, winnerInfo.result, winnerInfo.params, srcSize);
                        better = 1;
                    }
                }
                candidateInfo.params = cparam;
                paramVaryOnce(varArray[i], -1, &candidateInfo.params);
                candidateInfo.params = sanitizeParams(candidateInfo.params);
                //evaluate
                if(!ZSTD_isError(ZSTD_checkCParams(candidateInfo.params))) {
                //if(cParamValid(candidateInfo.params)) {
                    int res = feasibleBenchMemo(&candidateInfo.result,
                       srcBuffer, srcSize,
                       dstBuffer, dstSize, 
                       ctx, dctx, 
                       candidateInfo.params, target, &winnerInfo.result, memoTable,
                       varArray, varLen);
                    if(res == FEASIBLE_RESULT) {
                        winnerInfo = candidateInfo;
                        //BMK_printWinner(stdout, CUSTOM_LEVEL, winnerInfo.result, winnerInfo.params, srcSize);
                        better = 1;
                    }
                }
            } 
            //if 'better' enough, skip further parameter search, center there?
            //possible improvement - guide direction here w/ knowledge rather than completely random variation. 
            for(d = 2; d < varLen + 2; d++) { /* varLen is # dimensions */
                for(i = 0; i < 5; i++) { //TODO: make ? relative to # of free dimensions.
                    int res;
                    candidateInfo.params = cparam;
                    /* param error checking already done here */
                    paramVariation(&candidateInfo.params, varArray, varLen, d); //info candidateInfo.params is garbage, this is too.
                    res = feasibleBenchMemo(&candidateInfo.result,
                       srcBuffer, srcSize,
                       dstBuffer, dstSize, 
                       ctx, dctx, 
                       candidateInfo.params, target, &winnerInfo.result, memoTable,
                       varArray, varLen);
                    if(res == FEASIBLE_RESULT) {
                        winnerInfo = candidateInfo;
                        //BMK_printWinner(stdout, CUSTOM_LEVEL, winnerInfo.result, winnerInfo.params, srcSize);
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
    void* dstBuffer, size_t dstSize, 
    constraint_t target, ZSTD_compressionParameters paramTarget, 
    ZSTD_strategy strat, U32* varArray, int varLen) {
    int i = 0; //TODO: Temp fix 10 iters, check effects of changing this? 
    U32* varNew = malloc(sizeof(U32) * varLen);
    int varLenNew = sanitizeVarArray(varLen, varArray, varNew, strat);
    size_t memoLen = memoTableLen(varNew, varLenNew);
    U8* memoTable = malloc(sizeof(U8) * memoLen);
    ZSTD_compressionParameters init;
    ZSTD_CCtx* ctx = ZSTD_createCCtx();
    ZSTD_DCtx* dctx = ZSTD_createDCtx();
    winnerInfo_t winnerInfo, candidateInfo; 
    winnerInfo.result.cSpeed = 0;
    winnerInfo.result.dSpeed = 0;
    winnerInfo.result.cMem = (size_t)(-1);
    winnerInfo.result.cSize = (size_t)(-1);
    /* so climb is given the right fixed strategy */
    paramTarget.strategy = strat;
    /* to pass ZSTD_checkCParams */

    memoTableInit(memoTable, paramTarget, target, varNew, varLenNew, srcSize);

    //needs to happen after memoTableInit as that assumes 0 = undefined. 
    cParamZeroMin(&paramTarget);

    init = paramTarget;


    if(!ctx || !dctx || !memoTable || !varNew) {
        DISPLAY("NOT ENOUGH MEMORY ! ! ! \n");
        goto _cleanUp;
    }

    while(i < 10) {
        DISPLAY("Restart\n");
        randomConstrainedParams(&init, varNew, varLenNew, memoTable);
        candidateInfo = climbOnce(target, varNew, varLenNew, memoTable, srcBuffer, srcSize, dstBuffer, dstSize, ctx, dctx, init);
        if(objective_lt(winnerInfo.result, candidateInfo.result)) {
            winnerInfo = candidateInfo;
            DISPLAY("New Winner: ");
            BMK_printWinner(stdout, CUSTOM_LEVEL, winnerInfo.result, winnerInfo.params, srcSize);
        }
        i++;
    }

_cleanUp:
    ZSTD_freeCCtx(ctx);
    ZSTD_freeDCtx(dctx);
    free(memoTable);
    free(varNew);
    return winnerInfo;
}

// bigger and (hopefully) better* than optimizeForSize
// TODO: allow accept multiple files like benchFiles or bench.c fn's 
static int optimizeForSize2(const char* inFileName, constraint_t target, ZSTD_compressionParameters paramTarget)
{
    FILE* const inFile = fopen( inFileName, "rb" );
    U64 const inFileSize = UTIL_getFileSize(inFileName);
    size_t benchedSize = BMK_findMaxMem(inFileSize*3) / 3;
    void* origBuff;
    U32 varArray [NUM_PARAMS];
    int varLen = variableParams(paramTarget, varArray);
    /* Init */


    if(!cParamValid(paramTarget)) {
        return 10;
    }

    if (inFile==NULL) { DISPLAY( "Pb opening %s\n", inFileName); return 11; }
    if (inFileSize == UTIL_FILESIZE_UNKNOWN) {
        DISPLAY("Pb evaluatin size of %s \n", inFileName);
        fclose(inFile);
        return 11;
    }

    /* Memory allocation & restrictions */
    if ((U64)benchedSize > inFileSize) benchedSize = (size_t)inFileSize;
    if (benchedSize < inFileSize) {
        DISPLAY("Not enough memory for '%s' \n", inFileName);
        fclose(inFile);
        return 11;
    }

    /* Alloc */
    origBuff = malloc(benchedSize);
    if(!origBuff) {
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
    DISPLAY("optimizing for %s", inFileName);
    if(target.cSpeed != 0) { DISPLAY(" - limit compression speed %u MB/s", target.cSpeed / 1000000); }
    if(target.dSpeed != 0) { DISPLAY(" - limit decompression speed %u MB/s", target.dSpeed / 1000000); }
    if(target.cMem != (U32)-1) { DISPLAY(" - limit memory %u MB", target.cMem / 1000000); }
    DISPLAY("\n");
    findClockGranularity();

    {   ZSTD_CCtx* const ctx = ZSTD_createCCtx();
        ZSTD_DCtx* const dctx = ZSTD_createDCtx();
        winnerInfo_t winner;
        //BMK_result_t candidate;
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
        //Can't do this w/ cparameter constraints 
        //still useful though? 
        /*
        {   const int maxSeeds = g_noSeed ? 1 : ZSTD_maxCLevel();
            int i;
            for (i=1; i<=maxSeeds; i++) {
                ZSTD_compressionParameters const CParams = ZSTD_getCParams(i, blockSize, 0);
                BMK_benchParam(&candidate, origBuff, benchedSize, ctx, dctx, CParams);
                if (!feasible(candidate, target) ) {
                    break;
                }
                if (feasible(candidate,target) && objective_lt(winner.result, candidate))
                {
                    winner.params = CParams;
                    winner.result = candidate;
                    BMK_printWinner(stdout, i, winner.result, winner.params, benchedSize);
            }   }
        }*/
        BMK_printWinner(stdout, CUSTOM_LEVEL, winner.result, winner.params, benchedSize);

        BMK_translateAdvancedParams(winner.params);

        /* start real tests */
        {   
            if(paramTarget.strategy == 0) {
                int st;
                for(st = 1; st <= 8; st++) {
                    winnerInfo_t wc = optimizeFixedStrategy(origBuff, benchedSize, compressedBuffer, maxCompressedSize, 
                    target, paramTarget, st, varArray, varLen);
                    DISPLAY("StratNum %d\n", st);
                    if(objective_lt(winner.result, wc.result)) {
                        winner = wc;
                    }
                }
            } else {
                winner = optimizeFixedStrategy(origBuff, benchedSize, compressedBuffer, maxCompressedSize,
                    target, paramTarget, paramTarget.strategy, varArray, varLen);
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

    free(origBuff);
    return 0;
}


/* optimizeForSize():
 * targetSpeed : expressed in B/s */
/* expresses targeted compression, decompression speeds and memory requirements */
/* if state space is small (from paramTarget), exhaustive search?  */
//things to consider : if doing strategy-separate approach, what cutoffs to evaluate each strategy
//or do all? can't be absolute, should be relative after some sort of calibration 
//(synthetic? test levels (we don't care about data specifics rn, scale?) ?
int optimizeForSize(const char* inFileName, constraint_t target, ZSTD_compressionParameters paramTarget)
{
    FILE* const inFile = fopen( inFileName, "rb" );
    U64 const inFileSize = UTIL_getFileSize(inFileName);
    size_t benchedSize = BMK_findMaxMem(inFileSize*3) / 3;
    void* origBuff;
    U32 paramVarArray [NUM_PARAMS];
    int paramCount = variableParams(paramTarget, paramVarArray);
    /* Init */
    if (inFile==NULL) { DISPLAY( "Pb opening %s\n", inFileName); return 11; }
    if (inFileSize == UTIL_FILESIZE_UNKNOWN) {
        DISPLAY("Pb evaluatin size of %s \n", inFileName);
        fclose(inFile);
        return 11;
    }

    /* Memory allocation & restrictions */
    if ((U64)benchedSize > inFileSize) benchedSize = (size_t)inFileSize;
    if (benchedSize < inFileSize) {
        DISPLAY("Not enough memory for '%s' \n", inFileName);
        fclose(inFile);
        return 11;
    }

    /* Alloc */
    origBuff = malloc(benchedSize);
    if(!origBuff) {
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
    DISPLAY("optimizing for %s", inFileName);
    if(target.cSpeed != 0) { DISPLAY(" - limit compression speed %u MB/s", target.cSpeed / 1000000); }
    if(target.dSpeed != 0) { DISPLAY(" - limit decompression speed %u MB/s", target.dSpeed / 1000000); }
    if(target.cMem != 0) { DISPLAY(" - limit memory %u MB", target.cMem / 1000000); }
    DISPLAY("\n");
    {   ZSTD_CCtx* const ctx = ZSTD_createCCtx();
        ZSTD_DCtx* const dctx = ZSTD_createDCtx();
        winnerInfo_t winner;
        BMK_result_t candidate;
        const size_t blockSize = g_blockSize ? g_blockSize : benchedSize;

        /* init */
        if (ctx==NULL) { DISPLAY("\n ZSTD_createCCtx error \n"); free(origBuff); return 14; }

        memset(&winner, 0, sizeof(winner));
        winner.result.cSize = (size_t)(-1);

        /* find best solution from default params */
        //Can't do this w/ cparameter constraints 
        {   const int maxSeeds = g_noSeed ? 1 : ZSTD_maxCLevel();
            int i;
            for (i=1; i<=maxSeeds; i++) {
                ZSTD_compressionParameters const CParams = ZSTD_getCParams(i, blockSize, 0);
                BMK_benchParam(&candidate, origBuff, benchedSize, ctx, dctx, CParams);
                if (!feasible(candidate, target) ) {
                    break;
                }
                if (feasible(candidate,target) && objective_lt(winner.result, candidate))
                {
                    winner.params = CParams;
                    winner.result = candidate;
                    BMK_printWinner(stdout, i, winner.result, winner.params, benchedSize);
            }   }
        }
        BMK_printWinner(stdout, CUSTOM_LEVEL, winner.result, winner.params, benchedSize);

        BMK_translateAdvancedParams(winner.params);

        /* start tests */
        {   time_t const grillStart = time(NULL);
            do {
                ZSTD_compressionParameters params = winner.params;
                BYTE* b;
                paramVariation(&params, paramVarArray, paramCount, 4);
                if ((FUZ_rand(&g_rand) & 31) == 3) params = randomParams();  /* totally random config to improve search space */
                params = ZSTD_adjustCParams(params, blockSize, 0);

                /* exclude faster if already played set of params */
                if (FUZ_rand(&g_rand) & ((1 << *NB_TESTS_PLAYED(params))-1)) continue;

                /* test */
                b = NB_TESTS_PLAYED(params);
                (*b)++;
                BMK_benchParam(&candidate, origBuff, benchedSize, ctx, dctx, params);

                /* improvement found => new winner */
                if (feasible(candidate,target) && objective_lt(winner.result, candidate))
                {
                    winner.params = params;
                    winner.result = candidate;
                    BMK_printWinner(stdout, CUSTOM_LEVEL, winner.result, winner.params, benchedSize);
                    BMK_translateAdvancedParams(winner.params);
                }
            } while (BMK_timeSpan(grillStart) < g_grillDuration_s);
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

    free(origBuff);
    return 0;
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
    const char* input_filename=0;
    U32 optimizer = 0;
    U32 main_pause = 0;


    constraint_t target = { 0, 0, (U32)-1 }; //0 for anything unset
    ZSTD_compressionParameters paramTarget = { 0, 0, 0, 0, 0, 0, 0 };

    assert(argc>=1);   /* for exename */

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
            result = optimizeForSize2(input_filename, target, paramTarget);
            //optimizeForSize(input_filename, target, paramTarget);
        } else {
            result = benchFiles(argv+filenamesStart, argc-filenamesStart);
    }   }

    if (main_pause) { int unused; printf("press enter...\n"); unused = getchar(); (void)unused; }

    return result;
}
