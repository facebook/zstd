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

/*-************************************
*  Benchmark Parameters
**************************************/

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

//TODO: benchMem dctx can't = NULL in new system
static size_t
BMK_benchParam(BMK_result_t* resultPtr,
               const void* srcBuffer, size_t srcSize,
               ZSTD_CCtx* ctx, ZSTD_DCtx* dctx, 
               const ZSTD_compressionParameters cParams) {


    BMK_return_t res = BMK_benchMem(srcBuffer,srcSize, &srcSize, 1, 0, &cParams, NULL, 0, ctx, dctx, 0, "File");
    *resultPtr = res.result;
    return res.errorCode;
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


/* nullified useless params, to ensure count stats */
static ZSTD_compressionParameters* sanitizeParams(ZSTD_compressionParameters params)
{
    g_params = params;
    if (params.strategy == ZSTD_fast)
        g_params.chainLog = 0, g_params.searchLog = 0;
    if (params.strategy == ZSTD_dfast)
        g_params.searchLog = 0;
    if (params.strategy != ZSTD_btopt && params.strategy != ZSTD_btultra)
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


static void playAround(FILE* f, winnerInfo_t* winners,
                       ZSTD_compressionParameters params,
                       const void* srcBuffer, size_t srcSize,
                       ZSTD_CCtx* ctx, ZSTD_DCtx* dctx)
{
    int nbVariations = 0;
    UTIL_time_t const clockStart = UTIL_getTime();

    while (UTIL_clockSpanMicro(clockStart) < g_maxVariationTime) {
        ZSTD_compressionParameters p = params;

        if (nbVariations++ > g_maxNbVariations) break;
        paramVariation(&p);

        /* exclude faster if already played params */
        if (FUZ_rand(&g_rand) & ((1 << NB_TESTS_PLAYED(p))-1))
            continue;

        /* test */
        NB_TESTS_PLAYED(p)++;
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
        p.chainLog   = (FUZ_rand(&g_rand) % (ZSTD_CHAINLOG_MAX+1 - ZSTD_CHAINLOG_MIN)) + ZSTD_CHAINLOG_MIN;
        p.hashLog    = (FUZ_rand(&g_rand) % (ZSTD_HASHLOG_MAX+1 - ZSTD_HASHLOG_MIN)) + ZSTD_HASHLOG_MIN;
        p.searchLog  = (FUZ_rand(&g_rand) % (ZSTD_SEARCHLOG_MAX+1 - ZSTD_SEARCHLOG_MIN)) + ZSTD_SEARCHLOG_MIN;
        p.windowLog  = (FUZ_rand(&g_rand) % (ZSTD_WINDOWLOG_MAX+1 - ZSTD_WINDOWLOG_MIN)) + ZSTD_WINDOWLOG_MIN;
        p.searchLength=(FUZ_rand(&g_rand) % (ZSTD_SEARCHLENGTH_MAX+1 - ZSTD_SEARCHLENGTH_MIN)) + ZSTD_SEARCHLENGTH_MIN;
        p.targetLength=(FUZ_rand(&g_rand) % (512));
        p.strategy   = (ZSTD_strategy) (FUZ_rand(&g_rand) % (ZSTD_btultra +1));
        validated = !ZSTD_isError(ZSTD_checkCParams(p));
    }
    return p;
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


static void BMK_translateAdvancedParams(ZSTD_compressionParameters params)
{
    DISPLAY("--zstd=windowLog=%u,chainLog=%u,hashLog=%u,searchLog=%u,searchLength=%u,targetLength=%u,strategy=%u \n",
             params.windowLog, params.chainLog, params.hashLog, params.searchLog, params.searchLength, params.targetLength, (U32)(params.strategy));
}

/* optimizeForSize():
 * targetSpeed : expressed in MB/s */
int optimizeForSize(const char* inFileName, U32 targetSpeed)
{
    FILE* const inFile = fopen( inFileName, "rb" );
    U64 const inFileSize = UTIL_getFileSize(inFileName);
    size_t benchedSize = BMK_findMaxMem(inFileSize*3) / 3;
    void* origBuff;
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
    DISPLAY("optimizing for %s - limit speed %u MB/s \n", inFileName, targetSpeed);
    targetSpeed *= 1000000;
    {   ZSTD_CCtx* const ctx = ZSTD_createCCtx();
        ZSTD_DCtx* const dctx = ZSTD_createDCtx();
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
                ZSTD_compressionParameters const CParams = ZSTD_getCParams(i, blockSize, 0);
                BMK_benchParam(&candidate, origBuff, benchedSize, ctx, dctx, CParams);
                if (candidate.cSpeed < (double)targetSpeed) {
                    break;
                }
                if ( (candidate.cSize < winner.result.cSize)
                   | ((candidate.cSize == winner.result.cSize) & (candidate.cSpeed > winner.result.cSpeed)) )
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
                paramVariation(&params);
                if ((FUZ_rand(&g_rand) & 31) == 3) params = randomParams();  /* totally random config to improve search space */
                params = ZSTD_adjustCParams(params, blockSize, 0);

                /* exclude faster if already played set of params */
                if (FUZ_rand(&g_rand) & ((1 << NB_TESTS_PLAYED(params))-1)) continue;

                /* test */
                NB_TESTS_PLAYED(params)++;
                BMK_benchParam(&candidate, origBuff, benchedSize, ctx, dctx, params);

                /* improvement found => new winner */
                if ( (candidate.cSpeed > targetSpeed)
                   & ( (candidate.cSize < winner.result.cSize)
                     | ((candidate.cSize == winner.result.cSize) & (candidate.cSpeed > winner.result.cSpeed)) )  )
                {
                    winner.params = params;
                    winner.result = candidate;
                    BMK_printWinner(stdout, CUSTOM_LEVEL, winner.result, winner.params, benchedSize);
                    BMK_translateAdvancedParams(winner.params);
                }
            } while (BMK_timeSpan(grillStart) < g_grillDuration_s);
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
    U32 targetSpeed = 0;

    assert(argc>=1);   /* for exename */

    /* Welcome message */
    DISPLAY(WELCOME_MESSAGE);

    for(i=1; i<argc; i++) {
        const char* argument = argv[i];
        assert(argument != NULL);

        if(!strcmp(argument,"--no-seed")) { g_noSeed = 1; continue; }

        /* Decode command (note : aggregated commands are allowed) */
        if (longCommandWArg(&argument, "--zstd=")) {
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
                    targetSpeed = readU32FromChar(&argument);
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
            result = optimizeForSize(input_filename, targetSpeed);
        } else {
            result = benchFiles(argv+filenamesStart, argc-filenamesStart);
    }   }

    if (main_pause) { int unused; printf("press enter...\n"); unused = getchar(); (void)unused; }

    return result;
}
