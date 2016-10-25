/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */


/*-************************************
*  Dependencies
**************************************/
#include "util.h"      /* Compiler options, UTIL_GetFileSize */
#include <stdlib.h>    /* malloc */
#include <stdio.h>     /* fprintf, fopen, ftello64 */
#include <string.h>    /* strcmp */
#include <math.h>      /* log */
#include <time.h>      /* clock_t */

#include "mem.h"
#define ZSTD_STATIC_LINKING_ONLY   /* ZSTD_parameters, ZSTD_estimateCCtxSize */
#include "zstd.h"
#include "datagen.h"
#include "xxhash.h"


/*-************************************
*  Constants
**************************************/
#define PROGRAM_DESCRIPTION "ZSTD parameters tester"
#define AUTHOR "Yann Collet"
#define WELCOME_MESSAGE "*** %s %s %i-bits, by %s (%s) ***\n", PROGRAM_DESCRIPTION, ZSTD_VERSION_STRING, (int)(sizeof(void*)*8), AUTHOR, __DATE__


#define KB *(1<<10)
#define MB *(1<<20)
#define GB *(1ULL<<30)

#define NBLOOPS    2
#define TIMELOOP   (2 * CLOCKS_PER_SEC)

#define NB_LEVELS_TRACKED 30

static const size_t maxMemory = (sizeof(size_t)==4)  ?  (2 GB - 64 MB) : (size_t)(1ULL << ((sizeof(size_t)*8)-31));

#define COMPRESSIBILITY_DEFAULT 0.50
static const size_t sampleSize = 10000000;

static const U32 g_grillDuration_s = 60000;   /* about 16 hours */
static const clock_t g_maxParamTime = 15 * CLOCKS_PER_SEC;
static const clock_t g_maxVariationTime = 60 * CLOCKS_PER_SEC;
static const int g_maxNbVariations = 64;


/*-************************************
*  Macros
**************************************/
#define DISPLAY(...)  fprintf(stderr, __VA_ARGS__)


/*-************************************
*  Benchmark Parameters
**************************************/
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

static clock_t BMK_clockSpan(clock_t cStart) { return clock() - cStart; }  /* works even if overflow ; max span ~ 30 mn */

static U32 BMK_timeSpan(time_t tStart) { return (U32)difftime(time(NULL), tStart); }  /* accuracy in seconds only, span can be multiple years */


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


#  define FUZ_rotl32(x,r) ((x << r) | (x >> (32 - r)))
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


/*-*******************************************************
*  Bench functions
*********************************************************/
typedef struct {
    size_t cSize;
    double cSpeed;
    double dSpeed;
} BMK_result_t;

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


#define MIN(a,b)  ( (a) < (b) ? (a) : (b) )

static size_t BMK_benchParam(BMK_result_t* resultPtr,
                             const void* srcBuffer, size_t srcSize,
                             ZSTD_CCtx* ctx,
                             const ZSTD_compressionParameters cParams)
{
    const size_t blockSize = g_blockSize ? g_blockSize : srcSize;
    const U32 nbBlocks = (U32) ((srcSize + (blockSize-1)) / blockSize);
    blockParam_t* const blockTable = (blockParam_t*) malloc(nbBlocks * sizeof(blockParam_t));
    const size_t maxCompressedSize = (size_t)nbBlocks * ZSTD_compressBound(blockSize);
    void* const compressedBuffer = malloc(maxCompressedSize);
    void* const resultBuffer = malloc(srcSize);
    ZSTD_parameters params;
    U32 Wlog = cParams.windowLog;
    U32 Clog = cParams.chainLog;
    U32 Hlog = cParams.hashLog;
    U32 Slog = cParams.searchLog;
    U32 Slength = cParams.searchLength;
    U32 Tlength = cParams.targetLength;
    ZSTD_strategy strat = cParams.strategy;
    char name[30] = { 0 };
    U64 crcOrig;

    /* Memory allocation & restrictions */
    snprintf(name, 30, "Sw%02uc%02uh%02us%02ul%1ut%03uS%1u", Wlog, Clog, Hlog, Slog, Slength, Tlength, strat);
    if (!compressedBuffer || !resultBuffer || !blockTable) {
        DISPLAY("\nError: not enough memory!\n");
        free(compressedBuffer);
        free(resultBuffer);
        free(blockTable);
        return 12;
    }

    /* Calculating input Checksum */
    crcOrig = XXH64(srcBuffer, srcSize, 0);

    /* Init blockTable data */
    {
        U32 i;
        size_t remaining = srcSize;
        const char* srcPtr = (const char*)srcBuffer;
        char* cPtr = (char*)compressedBuffer;
        char* resPtr = (char*)resultBuffer;
        for (i=0; i<nbBlocks; i++) {
            size_t thisBlockSize = MIN(remaining, blockSize);
            blockTable[i].srcPtr = srcPtr;
            blockTable[i].cPtr = cPtr;
            blockTable[i].resPtr = resPtr;
            blockTable[i].srcSize = thisBlockSize;
            blockTable[i].cRoom = ZSTD_compressBound(thisBlockSize);
            srcPtr += thisBlockSize;
            cPtr += blockTable[i].cRoom;
            resPtr += thisBlockSize;
            remaining -= thisBlockSize;
    }   }

    /* warmimg up memory */
    RDG_genBuffer(compressedBuffer, maxCompressedSize, 0.10, 0.10, 1);

    /* Bench */
    {   U32 loopNb;
        size_t cSize = 0;
        double fastestC = 100000000., fastestD = 100000000.;
        double ratio = 0.;
        U64 crcCheck = 0;
        clock_t const benchStart = clock();

        DISPLAY("\r%79s\r", "");
        memset(&params, 0, sizeof(params));
        params.cParams = cParams;
        for (loopNb = 1; loopNb <= g_nbIterations; loopNb++) {
            int nbLoops;
            U32 blockNb;
            clock_t roundStart, roundClock;

            { clock_t const benchTime = BMK_clockSpan(benchStart);
              if (benchTime > g_maxParamTime) break; }

            /* Compression */
            DISPLAY("\r%1u-%s : %9u ->", loopNb, name, (U32)srcSize);
            memset(compressedBuffer, 0xE5, maxCompressedSize);

            nbLoops = 0;
            roundStart = clock();
            while (clock() == roundStart);
            roundStart = clock();
            while (BMK_clockSpan(roundStart) < TIMELOOP) {
                for (blockNb=0; blockNb<nbBlocks; blockNb++)
                    blockTable[blockNb].cSize = ZSTD_compress_advanced(ctx,
                                                    blockTable[blockNb].cPtr,  blockTable[blockNb].cRoom,
                                                    blockTable[blockNb].srcPtr, blockTable[blockNb].srcSize,
                                                    NULL, 0,
                                                    params);
                nbLoops++;
            }
            roundClock = BMK_clockSpan(roundStart);

            cSize = 0;
            for (blockNb=0; blockNb<nbBlocks; blockNb++)
                cSize += blockTable[blockNb].cSize;
            if ((double)roundClock < fastestC * CLOCKS_PER_SEC * nbLoops) fastestC = ((double)roundClock / CLOCKS_PER_SEC) / nbLoops;
            ratio = (double)srcSize / (double)cSize;
            DISPLAY("\r");
            DISPLAY("%1u-%s : %9u ->", loopNb, name, (U32)srcSize);
            DISPLAY(" %9u (%4.3f),%7.1f MB/s", (U32)cSize, ratio, (double)srcSize / fastestC / 1000000.);
            resultPtr->cSize = cSize;
            resultPtr->cSpeed = (double)srcSize / fastestC;

#if 1
            /* Decompression */
            memset(resultBuffer, 0xD6, srcSize);

            nbLoops = 0;
            roundStart = clock();
            while (clock() == roundStart);
            roundStart = clock();
            for ( ; BMK_clockSpan(roundStart) < TIMELOOP; nbLoops++) {
                for (blockNb=0; blockNb<nbBlocks; blockNb++)
                    blockTable[blockNb].resSize = ZSTD_decompress(blockTable[blockNb].resPtr, blockTable[blockNb].srcSize,
                                                                  blockTable[blockNb].cPtr, blockTable[blockNb].cSize);
            }
            roundClock = BMK_clockSpan(roundStart);

            if ((double)roundClock < fastestD * CLOCKS_PER_SEC * nbLoops) fastestD = ((double)roundClock / CLOCKS_PER_SEC) / nbLoops;
            DISPLAY("\r");
            DISPLAY("%1u-%s : %9u -> ", loopNb, name, (U32)srcSize);
            DISPLAY("%9u (%4.3f),%7.1f MB/s, ", (U32)cSize, ratio, (double)srcSize / fastestC / 1000000.);
            DISPLAY("%7.1f MB/s", (double)srcSize / fastestD / 1000000.);
            resultPtr->dSpeed = (double)srcSize / fastestD;

            /* CRC Checking */
            crcCheck = XXH64(resultBuffer, srcSize, 0);
            if (crcOrig!=crcCheck) {
                unsigned u;
                unsigned eBlockSize = (unsigned)(MIN(65536*2, blockSize));
                DISPLAY("\n!!! WARNING !!! Invalid Checksum : %x != %x\n", (unsigned)crcOrig, (unsigned)crcCheck);
                for (u=0; u<srcSize; u++) {
                    if (((const BYTE*)srcBuffer)[u] != ((BYTE*)resultBuffer)[u]) {
                        printf("Decoding error at pos %u (block %u, pos %u) \n", u, u / eBlockSize, u % eBlockSize);
                        break;
                }   }
                break;
            }
#endif
    }   }

    /* End cleaning */
    DISPLAY("\r");
    free(compressedBuffer);
    free(resultBuffer);
    return 0;
}


const char* g_stratName[] = { "ZSTD_fast   ",
                              "ZSTD_dfast  ",
                              "ZSTD_greedy ",
                              "ZSTD_lazy   ",
                              "ZSTD_lazy2  ",
                              "ZSTD_btlazy2",
                              "ZSTD_btopt  ",
                              "ZSTD_btopt2 "};

static void BMK_printWinner(FILE* f, U32 cLevel, BMK_result_t result, ZSTD_compressionParameters params, size_t srcSize)
{
    DISPLAY("\r%79s\r", "");
    fprintf(f,"    {%3u,%3u,%3u,%3u,%3u,%3u, %s },  ",
            params.windowLog, params.chainLog, params.hashLog, params.searchLog, params.searchLength,
            params.targetLength, g_stratName[(U32)(params.strategy)]);
    fprintf(f,
            "/* level %2u */   /* R:%5.3f at %5.1f MB/s - %5.1f MB/s */\n",
            cLevel, (double)srcSize / result.cSize, result.cSpeed / 1000000., result.dSpeed / 1000000.);
}


static double g_cSpeedTarget[NB_LEVELS_TRACKED] = { 0. };   /* NB_LEVELS_TRACKED : checked at main() */

typedef struct {
    BMK_result_t result;
    ZSTD_compressionParameters params;
} winnerInfo_t;

static void BMK_printWinners2(FILE* f, const winnerInfo_t* winners, size_t srcSize)
{
    int cLevel;

    fprintf(f, "\n /* Proposed configurations : */ \n");
    fprintf(f, "    /* W,  C,  H,  S,  L,  T, strat */ \n");

    for (cLevel=0; cLevel <= ZSTD_maxCLevel(); cLevel++)
        BMK_printWinner(f, cLevel, winners[cLevel].result, winners[cLevel].params, srcSize);
}


static void BMK_printWinners(FILE* f, const winnerInfo_t* winners, size_t srcSize)
{
    fseek(f, 0, SEEK_SET);
    BMK_printWinners2(f, winners, srcSize);
    fflush(f);
    BMK_printWinners2(stdout, winners, srcSize);
}

static int BMK_seed(winnerInfo_t* winners, const ZSTD_compressionParameters params,
              const void* srcBuffer, size_t srcSize,
                    ZSTD_CCtx* ctx)
{
    BMK_result_t testResult;
    int better = 0;
    int cLevel;

    BMK_benchParam(&testResult, srcBuffer, srcSize, ctx, params);

    for (cLevel = 1; cLevel <= ZSTD_maxCLevel(); cLevel++) {
        if (testResult.cSpeed < g_cSpeedTarget[cLevel])
            continue;   /* not fast enough for this level */
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

            size_t W_CMemUsed = (1 << params.windowLog) + ZSTD_estimateCCtxSize(params);
            size_t O_CMemUsed = (1 << winners[cLevel].params.windowLog) + ZSTD_estimateCCtxSize(winners[cLevel].params);
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


/* nullified useless params, to ensure count stats */
static ZSTD_compressionParameters* sanitizeParams(ZSTD_compressionParameters params)
{
    g_params = params;
    if (params.strategy == ZSTD_fast)
        g_params.chainLog = 0, g_params.searchLog = 0;
    if (params.strategy == ZSTD_dfast)
        g_params.searchLog = 0;
    if (params.strategy != ZSTD_btopt && params.strategy != ZSTD_btopt2)
        g_params.targetLength = 0;
    return &g_params;
}


static void paramVariation(ZSTD_compressionParameters* ptr)
{
    ZSTD_compressionParameters p;
    U32 validated = 0;
    while (!validated) {
        U32 nbChanges = (FUZ_rand(&g_rand) & 3) + 1;
        p = *ptr;
        for ( ; nbChanges ; nbChanges--) {
            const U32 changeID = FUZ_rand(&g_rand) % 14;
            switch(changeID)
            {
            case 0:
                p.chainLog++; break;
            case 1:
                p.chainLog--; break;
            case 2:
                p.hashLog++; break;
            case 3:
                p.hashLog--; break;
            case 4:
                p.searchLog++; break;
            case 5:
                p.searchLog--; break;
            case 6:
                p.windowLog++; break;
            case 7:
                p.windowLog--; break;
            case 8:
                p.searchLength++; break;
            case 9:
                p.searchLength--; break;
            case 10:
                p.strategy = (ZSTD_strategy)(((U32)p.strategy)+1); break;
            case 11:
                p.strategy = (ZSTD_strategy)(((U32)p.strategy)-1); break;
            case 12:
                p.targetLength *= 1 + ((double)(FUZ_rand(&g_rand)&255)) / 256.; break;
            case 13:
                p.targetLength /= 1 + ((double)(FUZ_rand(&g_rand)&255)) / 256.; break;
            }
        }
        validated = !ZSTD_isError(ZSTD_checkCParams(p));
    }
    *ptr = p;
}


#define PARAMTABLELOG   25
#define PARAMTABLESIZE (1<<PARAMTABLELOG)
#define PARAMTABLEMASK (PARAMTABLESIZE-1)
static BYTE g_alreadyTested[PARAMTABLESIZE] = {0};   /* init to zero */

#define NB_TESTS_PLAYED(p) \
    g_alreadyTested[(XXH64(sanitizeParams(p), sizeof(p), 0) >> 3) & PARAMTABLEMASK]


#define MAX(a,b)   ( (a) > (b) ? (a) : (b) )

static void playAround(FILE* f, winnerInfo_t* winners,
                       ZSTD_compressionParameters params,
                       const void* srcBuffer, size_t srcSize,
                       ZSTD_CCtx* ctx)
{
    int nbVariations = 0;
    clock_t const clockStart = clock();

    while (BMK_clockSpan(clockStart) < g_maxVariationTime) {
        ZSTD_compressionParameters p = params;

        if (nbVariations++ > g_maxNbVariations) break;
        paramVariation(&p);

        /* exclude faster if already played params */
        if (FUZ_rand(&g_rand) & ((1 << NB_TESTS_PLAYED(p))-1))
            continue;

        /* test */
        NB_TESTS_PLAYED(p)++;
        if (!BMK_seed(winners, p, srcBuffer, srcSize, ctx)) continue;

        /* improvement found => search more */
        BMK_printWinners(f, winners, srcSize);
        playAround(f, winners, p, srcBuffer, srcSize, ctx);
    }

}


static ZSTD_compressionParameters randomParams(void)
{
    ZSTD_compressionParameters p;
    U32 validated = 0;
    while (!validated) {
        /* totally random entry */
        p.chainLog   = FUZ_rand(&g_rand) % (ZSTD_CHAINLOG_MAX+1 - ZSTD_CHAINLOG_MIN) + ZSTD_CHAINLOG_MIN;
        p.hashLog    = FUZ_rand(&g_rand) % (ZSTD_HASHLOG_MAX+1 - ZSTD_HASHLOG_MIN) + ZSTD_HASHLOG_MIN;
        p.searchLog  = FUZ_rand(&g_rand) % (ZSTD_SEARCHLOG_MAX+1 - ZSTD_SEARCHLOG_MIN) + ZSTD_SEARCHLOG_MIN;
        p.windowLog  = FUZ_rand(&g_rand) % (ZSTD_WINDOWLOG_MAX+1 - ZSTD_WINDOWLOG_MIN) + ZSTD_WINDOWLOG_MIN;
        p.searchLength=FUZ_rand(&g_rand) % (ZSTD_SEARCHLENGTH_MAX+1 - ZSTD_SEARCHLENGTH_MIN) + ZSTD_SEARCHLENGTH_MIN;
        p.targetLength=FUZ_rand(&g_rand) % (ZSTD_TARGETLENGTH_MAX+1 - ZSTD_TARGETLENGTH_MIN) + ZSTD_TARGETLENGTH_MIN;
        p.strategy   = (ZSTD_strategy) (FUZ_rand(&g_rand) % (ZSTD_btopt2 +1));
        validated = !ZSTD_isError(ZSTD_checkCParams(p));
    }
    return p;
}

static void BMK_selectRandomStart(
                       FILE* f, winnerInfo_t* winners,
                       const void* srcBuffer, size_t srcSize,
                       ZSTD_CCtx* ctx)
{
    U32 const id = (FUZ_rand(&g_rand) % (ZSTD_maxCLevel()+1));
    if ((id==0) || (winners[id].params.windowLog==0)) {
        /* totally random entry */
        ZSTD_compressionParameters const p = ZSTD_adjustCParams(randomParams(), srcSize, 0);
        playAround(f, winners, p, srcBuffer, srcSize, ctx);
    }
    else
        playAround(f, winners, winners[id].params, srcBuffer, srcSize, ctx);
}


static void BMK_benchMem(void* srcBuffer, size_t srcSize)
{
    ZSTD_CCtx* const ctx = ZSTD_createCCtx();
    ZSTD_compressionParameters params;
    winnerInfo_t winners[NB_LEVELS_TRACKED];
    const char* const rfName = "grillResults.txt";
    FILE* const f = fopen(rfName, "w");
    const size_t blockSize = g_blockSize ? g_blockSize : srcSize;

    /* init */
    if (ctx==NULL) { DISPLAY("ZSTD_createCCtx() failed \n"); exit(1); }
    memset(winners, 0, sizeof(winners));
    if (f==NULL) { DISPLAY("error opening %s \n", rfName); exit(1); }

    if (g_singleRun) {
        BMK_result_t testResult;
        g_params = ZSTD_adjustCParams(g_params, srcSize, 0);
        BMK_benchParam(&testResult, srcBuffer, srcSize, ctx, g_params);
        DISPLAY("\n");
        return;
    }

    if (g_target)
        g_cSpeedTarget[1] = g_target * 1000000;
    else {
        /* baseline config for level 1 */
        BMK_result_t testResult;
        params = ZSTD_getCParams(1, blockSize, 0);
        BMK_benchParam(&testResult, srcBuffer, srcSize, ctx, params);
        g_cSpeedTarget[1] = (testResult.cSpeed * 31) / 32;
    }

    /* establish speed objectives (relative to level 1) */
    {   int i;
        for (i=2; i<=ZSTD_maxCLevel(); i++)
            g_cSpeedTarget[i] = (g_cSpeedTarget[i-1] * 25) / 32;
    }

    /* populate initial solution */
    {   const int maxSeeds = g_noSeed ? 1 : ZSTD_maxCLevel();
        int i;
        for (i=0; i<=maxSeeds; i++) {
            params = ZSTD_getCParams(i, blockSize, 0);
            BMK_seed(winners, params, srcBuffer, srcSize, ctx);
    }   }
    BMK_printWinners(f, winners, srcSize);

    /* start tests */
    {   const time_t grillStart = time(NULL);
        do {
            BMK_selectRandomStart(f, winners, srcBuffer, srcSize, ctx);
        } while (BMK_timeSpan(grillStart) < g_grillDuration_s);
    }

    /* end summary */
    BMK_printWinners(f, winners, srcSize);
    DISPLAY("grillParams operations completed \n");

    /* clean up*/
    fclose(f);
    ZSTD_freeCCtx(ctx);
}


static int benchSample(void)
{
    void* origBuff;
    size_t const benchedSize = sampleSize;
    const char* const name = "Sample 10MiB";

    /* Allocation */
    origBuff = malloc(benchedSize);
    if (!origBuff) { DISPLAY("\nError: not enough memory!\n"); return 12; }

    /* Fill buffer */
    RDG_genBuffer(origBuff, benchedSize, g_compressibility, 0.0, 0);

    /* bench */
    DISPLAY("\r%79s\r", "");
    DISPLAY("using %s %i%%: \n", name, (int)(g_compressibility*100));
    BMK_benchMem(origBuff, benchedSize);

    free(origBuff);
    return 0;
}


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
        BMK_benchMem(origBuff, benchedSize);

        /* clean */
        free(origBuff);
    }

    return 0;
}


int optimizeForSize(const char* inFileName, U32 targetSpeed)
{
    FILE* const inFile = fopen( inFileName, "rb" );
    U64 const inFileSize = UTIL_getFileSize(inFileName);
    size_t benchedSize = BMK_findMaxMem(inFileSize*3) / 3;
    void* origBuff;

    /* Init */
    if (inFile==NULL) { DISPLAY( "Pb opening %s\n", inFileName); return 11; }

    /* Memory allocation & restrictions */
    if ((U64)benchedSize > inFileSize) benchedSize = (size_t)inFileSize;
    if (benchedSize < inFileSize)
        DISPLAY("Not enough memory for '%s' full size; testing %i MB only...\n", inFileName, (int)(benchedSize>>20));

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
    DISPLAY("optimizing for %s - limit speed %u MB/s \n", inFileName, targetSpeed);
    targetSpeed *= 1000;

    {   ZSTD_CCtx* const ctx = ZSTD_createCCtx();
        ZSTD_compressionParameters params;
        winnerInfo_t winner;
        BMK_result_t candidate;
        const size_t blockSize = g_blockSize ? g_blockSize : benchedSize;

        /* init */
        if (ctx==NULL) { DISPLAY("\n ZSTD_createCCtx error \n"); free(origBuff); return 14;}
        memset(&winner, 0, sizeof(winner));
        winner.result.cSize = (size_t)(-1);

        /* find best solution from default params */
        {   const int maxSeeds = g_noSeed ? 1 : ZSTD_maxCLevel();
            int i;
            for (i=1; i<=maxSeeds; i++) {
                params = ZSTD_getCParams(i, blockSize, 0);
                BMK_benchParam(&candidate, origBuff, benchedSize, ctx, params);
                if (candidate.cSpeed < targetSpeed)
                    break;
                if ( (candidate.cSize < winner.result.cSize)
                   | ((candidate.cSize == winner.result.cSize) & (candidate.cSpeed > winner.result.cSpeed)) )
                {
                    winner.params = params;
                    winner.result = candidate;
                    BMK_printWinner(stdout, i, winner.result, winner.params, benchedSize);
            }   }
        }
        BMK_printWinner(stdout, 99, winner.result, winner.params, benchedSize);

        /* start tests */
        {   time_t const grillStart = time(NULL);
            do {
                params = winner.params;
                paramVariation(&params);
                if ((FUZ_rand(&g_rand) & 15) == 3) params = randomParams();

                /* exclude faster if already played set of params */
                if (FUZ_rand(&g_rand) & ((1 << NB_TESTS_PLAYED(params))-1)) continue;

                /* test */
                NB_TESTS_PLAYED(params)++;
                BMK_benchParam(&candidate, origBuff, benchedSize, ctx, params);

                /* improvement found => new winner */
                if ( (candidate.cSpeed > targetSpeed)
                   & ( (candidate.cSize < winner.result.cSize)
                     | ((candidate.cSize == winner.result.cSize) & (candidate.cSpeed > winner.result.cSpeed)) )  )
                {
                    winner.params = params;
                    winner.result = candidate;
                    BMK_printWinner(stdout, 99, winner.result, winner.params, benchedSize);
                }
            } while (BMK_timeSpan(grillStart) < g_grillDuration_s);
        }

        /* end summary */
        BMK_printWinner(stdout, 99, winner.result, winner.params, benchedSize);
        DISPLAY("grillParams size - optimizer completed \n");

        /* clean up*/
        ZSTD_freeCCtx(ctx);
    }

    free(origBuff);
    return 0;
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
    DISPLAY( " -O#    : find Optimized parameters for # target speed (default : 0) \n");
    DISPLAY( " -S     : Single run \n");
    DISPLAY( " -P#    : generated sample compressibility (default : %.1f%%) \n", COMPRESSIBILITY_DEFAULT * 100);
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
    U32 targetSpeed = 0;

    /* checks */
    if (NB_LEVELS_TRACKED <= ZSTD_maxCLevel()) {
        DISPLAY("Error : NB_LEVELS_TRACKED <= ZSTD_maxCLevel() \n");
        exit(1);
    }

    /* Welcome message */
    DISPLAY(WELCOME_MESSAGE);

    if (argc<1) { badusage(exename); return 1; }

    for(i=1; i<argc; i++) {
        const char* argument = argv[i];

        if(!argument) continue;   /* Protection if argument empty */

        if(!strcmp(argument,"--no-seed")) { g_noSeed = 1; continue; }

        /* Decode command (note : aggregated commands are allowed) */
        if (argument[0]=='-') {
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
                    if ((argument[0] >='0') & (argument[0] <='9'))
                        g_nbIterations = *argument++ - '0';
                    break;

                    /* Sample compressibility (when no file provided) */
                case 'P':
                    argument++;
                    {   U32 proba32 = 0;
                        while ((argument[0]>= '0') & (argument[0]<= '9'))
                            proba32 = (proba32*10) + (*argument++ - '0');
                        g_compressibility = (double)proba32 / 100.;
                    }
                    break;

                case 'O':
                    argument++;
                    optimizer=1;
                    targetSpeed = 0;
                    while ((*argument >= '0') & (*argument <= '9'))
                        targetSpeed = (targetSpeed*10) + (*argument++ - '0');
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
                            g_params.windowLog = 0;
                            argument++;
                            while ((*argument>= '0') && (*argument<='9'))
                                g_params.windowLog *= 10, g_params.windowLog += *argument++ - '0';
                            continue;
                        case 'c':
                            g_params.chainLog = 0;
                            argument++;
                            while ((*argument>= '0') && (*argument<='9'))
                                g_params.chainLog *= 10, g_params.chainLog += *argument++ - '0';
                            continue;
                        case 'h':
                            g_params.hashLog = 0;
                            argument++;
                            while ((*argument>= '0') && (*argument<='9'))
                                g_params.hashLog *= 10, g_params.hashLog += *argument++ - '0';
                            continue;
                        case 's':
                            g_params.searchLog = 0;
                            argument++;
                            while ((*argument>= '0') && (*argument<='9'))
                                g_params.searchLog *= 10, g_params.searchLog += *argument++ - '0';
                            continue;
                        case 'l':  /* search length */
                            g_params.searchLength = 0;
                            argument++;
                            while ((*argument>= '0') && (*argument<='9'))
                                g_params.searchLength *= 10, g_params.searchLength += *argument++ - '0';
                            continue;
                        case 't':  /* target length */
                            g_params.targetLength = 0;
                            argument++;
                            while ((*argument>= '0') && (*argument<='9'))
                                g_params.targetLength *= 10, g_params.targetLength += *argument++ - '0';
                            continue;
                        case 'S':  /* strategy */
                            argument++;
                            while ((*argument>= '0') && (*argument<='9'))
                                g_params.strategy = (ZSTD_strategy)(*argument++ - '0');
                            continue;
                        case 'L':
                            {   int cLevel = 0;
                                argument++;
                                while ((*argument>= '0') && (*argument<='9'))
                                    cLevel *= 10, cLevel += *argument++ - '0';
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
                    g_target = 0;
                    while ((*argument >= '0') && (*argument <= '9'))
                        g_target = (g_target*10) + (*argument++ - '0');
                    break;

                    /* cut input into blocks */
                case 'B':
                    g_blockSize = 0;
                    argument++;
                    while ((*argument >='0') & (*argument <='9'))
                        g_blockSize = (g_blockSize*10) + (*argument++ - '0');
                    if (*argument=='K') g_blockSize<<=10, argument++;  /* allows using KB notation */
                    if (*argument=='M') g_blockSize<<=20, argument++;
                    if (*argument=='B') argument++;
                    DISPLAY("using %u KB block size \n", g_blockSize>>10);
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

    if (filenamesStart==0)
        result = benchSample();
    else {
        if (optimizer)
            result = optimizeForSize(input_filename, targetSpeed);
        else
            result = benchFiles(argv+filenamesStart, argc-filenamesStart);
    }

    if (main_pause) { int unused; printf("press enter...\n"); unused = getchar(); (void)unused; }

    return result;
}
