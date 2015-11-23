/*
    paramgrill.c - parameter tester for zstd_hc
    Copyright (C) Yann Collet 2015

    GPL v2 License

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    You can contact the author at :
    - zstd source repository : https://github.com/Cyan4973/zstd
    - ztsd public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/**************************************
*  Compiler Options
**************************************/
/* Disable some Visual warning messages */
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE     /* VS2005 */

/* Unix Large Files support (>4GB) */
#if (defined(__sun__) && (!defined(__LP64__)))   /* Sun Solaris 32-bits requires specific definitions */
#  define _LARGEFILE_SOURCE
#  define _FILE_OFFSET_BITS 64
#elif ! defined(__LP64__)                        /* No point defining Large file for 64 bit */
#  define _LARGEFILE64_SOURCE
#endif

/* S_ISREG & gettimeofday() are not supported by MSVC */
#if defined(_MSC_VER) || defined(_WIN32)
#  define BMK_LEGACY_TIMER 1
#endif

#if defined(_MSC_VER)
#  define snprintf _snprintf    /* snprintf unsupported by Visual <= 2012 */
#endif
 

/**************************************
*  Includes
**************************************/
#include <stdlib.h>       /* malloc */
#include <stdio.h>        /* fprintf, fopen, ftello64 */
#include <sys/types.h>    /* stat64 */
#include <sys/stat.h>     /* stat64 */
#include <string.h>       /* strcmp */
#include <math.h>         /* log */

/* Use ftime() if gettimeofday() is not available on your target */
#if defined(BMK_LEGACY_TIMER)
#  include <sys/timeb.h>  /* timeb, ftime */
#else
#  include <sys/time.h>   /* gettimeofday */
#endif

#include "mem.h"
#include "zstd_static.h"
#include "datagen.h"
#include "xxhash.h"


/**************************************
*  Compiler Options
**************************************/
/* S_ISREG & gettimeofday() are not supported by MSVC */
#if !defined(S_ISREG)
#  define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif


/**************************************
*  Constants
**************************************/
#define PROGRAM_DESCRIPTION "ZSTD_HC parameters tester"
#ifndef ZSTD_VERSION
#  define ZSTD_VERSION ""
#endif
#define AUTHOR "Yann Collet"
#define WELCOME_MESSAGE "*** %s %s %i-bits, by %s (%s) ***\n", PROGRAM_DESCRIPTION, ZSTD_VERSION, (int)(sizeof(void*)*8), AUTHOR, __DATE__


#define KB *(1<<10)
#define MB *(1<<20)

#define NBLOOPS    2
#define TIMELOOP   2000

#define KNUTH      2654435761U
#define MAX_MEM    (1984 MB)
#define DEFAULT_CHUNKSIZE   (4<<20)

#define COMPRESSIBILITY_DEFAULT 0.50
static const size_t sampleSize = 10000000;

static const int g_grillDuration = 50000000;   /* about 13 hours */
static const int g_maxParamTime = 15000;   /* 15 sec */
static const int g_maxVariationTime = 60000;   /* 60 sec */
static const int g_maxNbVariations = 64;

/**************************************
*  Macros
**************************************/
#define DISPLAY(...)  fprintf(stderr, __VA_ARGS__)


/**************************************
*  Benchmark Parameters
**************************************/
static U32 g_nbIterations = NBLOOPS;
static double g_compressibility = COMPRESSIBILITY_DEFAULT;
static U32 g_blockSize = 0;
static U32 g_rand = 1;
static U32 g_singleRun = 0;
static U32 g_target = 0;
static U32 g_noSeed = 0;
static const ZSTD_parameters* g_seedParams = ZSTD_defaultParameters[0];
static ZSTD_parameters g_params = { 0, 0, 0, 0, 0, ZSTD_greedy };

void BMK_SetNbIterations(int nbLoops)
{
    g_nbIterations = nbLoops;
    DISPLAY("- %u iterations -\n", g_nbIterations);
}


/*********************************************************
*  Private functions
*********************************************************/

static unsigned BMK_highbit(U32 val)
{
#   if defined(_MSC_VER)   /* Visual */
    unsigned long r;
    _BitScanReverse(&r, val);
    return (unsigned)r;
#   elif defined(__GNUC__) && (__GNUC__ >= 3)   /* GCC Intrinsic */
    return 31 - __builtin_clz(val);
#   else   /* Software version */
    static const int DeBruijnClz[32] = { 0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30, 8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31 };
    U32 v = val;
    int r;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    r = DeBruijnClz[(U32)(v * 0x07C4ACDDU) >> 27];
    return r;
#   endif
}

#if defined(BMK_LEGACY_TIMER)

static int BMK_GetMilliStart(void)
{
  /* Based on Legacy ftime()
  *  Rolls over every ~ 12.1 days (0x100000/24/60/60)
  *  Use GetMilliSpan to correct for rollover */
  struct timeb tb;
  int nCount;
  ftime( &tb );
  nCount = (int) (tb.millitm + (tb.time & 0xfffff) * 1000);
  return nCount;
}

#else

static int BMK_GetMilliStart(void)
{
  /* Based on newer gettimeofday()
  *  Use GetMilliSpan to correct for rollover */
  struct timeval tv;
  int nCount;
  gettimeofday(&tv, NULL);
  nCount = (int) (tv.tv_usec/1000 + (tv.tv_sec & 0xfffff) * 1000);
  return nCount;
}

#endif


static int BMK_GetMilliSpan( int nTimeStart )
{
  int nSpan = BMK_GetMilliStart() - nTimeStart;
  if ( nSpan < 0 )
    nSpan += 0x100000 * 1000;
  return nSpan;
}


static size_t BMK_findMaxMem(U64 requiredMem)
{
    size_t step = 64 MB;
    BYTE* testmem=NULL;

    requiredMem = (((requiredMem >> 26) + 1) << 26);
    if (requiredMem > MAX_MEM) requiredMem = MAX_MEM;

    requiredMem += 2*step;
    while (!testmem)
    {
        requiredMem -= step;
        testmem = (BYTE*) malloc ((size_t)requiredMem);
    }

    free (testmem);
    return (size_t) (requiredMem - step);
}


static U64 BMK_GetFileSize(char* infilename)
{
    int r;
#if defined(_MSC_VER)
    struct _stat64 statbuf;
    r = _stat64(infilename, &statbuf);
#else
    struct stat statbuf;
    r = stat(infilename, &statbuf);
#endif
    if (r || !S_ISREG(statbuf.st_mode)) return 0;   /* No good... */
    return (U64)statbuf.st_size;
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


/*********************************************************
*  Bench functions
*********************************************************/
typedef struct {
    size_t cSize;
    U32 cSpeed;
    U32 dSpeed;
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
                             const ZSTD_parameters params)
{
    const size_t blockSize = g_blockSize ? g_blockSize : srcSize;
    const U32 nbBlocks = (U32) ((srcSize + (blockSize-1)) / blockSize);
    blockParam_t* const blockTable = (blockParam_t*) malloc(nbBlocks * sizeof(blockParam_t));
    const size_t maxCompressedSize = (size_t)nbBlocks * ZSTD_compressBound(blockSize);
    void* const compressedBuffer = malloc(maxCompressedSize);
    void* const resultBuffer = malloc(srcSize);
    U32 Wlog = params.windowLog;
    U32 Clog = params.contentLog;
    U32 Hlog = params.hashLog;
    U32 Slog = params.searchLog;
    U32 Slength = params.searchLength;
    ZSTD_strategy strat = params.strategy;
    char name[30] = { 0 };
    U64 crcOrig;

    /* Memory allocation & restrictions */
    snprintf(name, 30, "Sw%02uc%02uh%02us%02ul%1ut%1u", Wlog, Clog, Hlog, Slog, Slength, strat);
    if (!compressedBuffer || !resultBuffer || !blockTable)
    {
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
        for (i=0; i<nbBlocks; i++)
        {
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
        }
    }

    /* warmimg up memory */
    RDG_genBuffer(compressedBuffer, maxCompressedSize, 0.10, 0.10, 1);

    /* Bench */
    {
        U32 loopNb;
        size_t cSize = 0;
        double fastestC = 100000000., fastestD = 100000000.;
        double ratio = 0.;
        U64 crcCheck = 0;
        const int startTime =BMK_GetMilliStart();

        DISPLAY("\r%79s\r", "");
        for (loopNb = 1; loopNb <= g_nbIterations; loopNb++)
        {
            int nbLoops;
            int milliTime;
            U32 blockNb;
            const int totalTime = BMK_GetMilliSpan(startTime);

            /* early break (slow params) */
            if (totalTime > g_maxParamTime) break;

            /* Compression */
            DISPLAY("\r%1u-%s : %9u ->", loopNb, name, (U32)srcSize);
            memset(compressedBuffer, 0xE5, maxCompressedSize);

            nbLoops = 0;
            milliTime = BMK_GetMilliStart();
            while (BMK_GetMilliStart() == milliTime);
            milliTime = BMK_GetMilliStart();
            while (BMK_GetMilliSpan(milliTime) < TIMELOOP)
            {
                for (blockNb=0; blockNb<nbBlocks; blockNb++)
                    blockTable[blockNb].cSize = ZSTD_compress_advanced(ctx,
                                                    blockTable[blockNb].cPtr,  blockTable[blockNb].cRoom,
                                                    blockTable[blockNb].srcPtr, blockTable[blockNb].srcSize,
                                                    params);
                nbLoops++;
            }
            milliTime = BMK_GetMilliSpan(milliTime);

            cSize = 0;
            for (blockNb=0; blockNb<nbBlocks; blockNb++)
                cSize += blockTable[blockNb].cSize;
            if ((double)milliTime < fastestC*nbLoops) fastestC = (double)milliTime / nbLoops;
            ratio = (double)srcSize / (double)cSize;
            DISPLAY("\r");
            DISPLAY("%1u-%s : %9u ->", loopNb, name, (U32)srcSize);
            DISPLAY(" %9u (%4.3f),%7.1f MB/s", (U32)cSize, ratio, (double)srcSize / fastestC / 1000.);
            resultPtr->cSize = cSize;
            resultPtr->cSpeed = (U32)((double)srcSize / fastestC);

#if 1
            /* Decompression */
            memset(resultBuffer, 0xD6, srcSize);

            nbLoops = 0;
            milliTime = BMK_GetMilliStart();
            while (BMK_GetMilliStart() == milliTime);
            milliTime = BMK_GetMilliStart();
            for ( ; BMK_GetMilliSpan(milliTime) < TIMELOOP; nbLoops++)
            {
                for (blockNb=0; blockNb<nbBlocks; blockNb++)
                    blockTable[blockNb].resSize = ZSTD_decompress(blockTable[blockNb].resPtr, blockTable[blockNb].srcSize,
                                                                  blockTable[blockNb].cPtr, blockTable[blockNb].cSize);
            }
            milliTime = BMK_GetMilliSpan(milliTime);

            if ((double)milliTime < fastestD*nbLoops) fastestD = (double)milliTime / nbLoops;
            DISPLAY("\r");
            DISPLAY("%1u-%s : %9u -> ", loopNb, name, (U32)srcSize);
            DISPLAY("%9u (%4.3f),%7.1f MB/s, ", (U32)cSize, ratio, (double)srcSize / fastestC / 1000.);
            DISPLAY("%7.1f MB/s", (double)srcSize / fastestD / 1000.);
            resultPtr->dSpeed = (U32)((double)srcSize / fastestD);

            /* CRC Checking */
            crcCheck = XXH64(resultBuffer, srcSize, 0);
            if (crcOrig!=crcCheck)
            {
                unsigned u;
                unsigned eBlockSize = (unsigned)(MIN(65536*2, blockSize));
                DISPLAY("\n!!! WARNING !!! Invalid Checksum : %x != %x\n", (unsigned)crcOrig, (unsigned)crcCheck);
                for (u=0; u<srcSize; u++)
                {
                    if (((const BYTE*)srcBuffer)[u] != ((BYTE*)resultBuffer)[u])
                    {
                        printf("Decoding error at pos %u (block %u, pos %u) \n", u, u / eBlockSize, u % eBlockSize);
                        break;
                    }
                }
                break;
            }
#endif
        }
    }

    /* End cleaning */
    DISPLAY("\r");
    free(compressedBuffer);
    free(resultBuffer);
    return 0;
}


const char* g_stratName[] = { "ZSTD_fast   ",
                              "ZSTD_greedy ",
                              "ZSTD_lazy   ",
                              "ZSTD_lazy2  ",
                              "ZSTD_btlazy2" };

static void BMK_printWinner(FILE* f, U32 cLevel, BMK_result_t result, ZSTD_parameters params, size_t srcSize)
{
    DISPLAY("\r%79s\r", "");
    fprintf(f,"    {%3u,%3u,%3u,%3u,%3u, %s },  ",
            params.windowLog, params.contentLog, params.hashLog, params.searchLog, params.searchLength,
            g_stratName[(U32)(params.strategy)]);
    fprintf(f,
            "/* level %2u */   /* R:%5.3f at %5.1f MB/s - %5.1f MB/s */\n",
            cLevel, (double)srcSize / result.cSize, (double)result.cSpeed / 1000., (double)result.dSpeed / 1000.);
}


static U32 g_cSpeedTarget[ZSTD_MAX_CLEVEL+1] = { 0 };

typedef struct {
    BMK_result_t result;
    ZSTD_parameters params;
} winnerInfo_t;

static void BMK_printWinners2(FILE* f, const winnerInfo_t* winners, size_t srcSize)
{
    int cLevel;

    fprintf(f, "\n /* Selected configurations : */ \n");
    fprintf(f, "#define ZSTD_MAX_CLEVEL %2u \n", ZSTD_MAX_CLEVEL);
    fprintf(f, "static const ZSTD_parameters ZSTD_defaultParameters[ZSTD_MAX_CLEVEL+1] = {\n");
    fprintf(f, "    /* W,  C,  H,  S,  L, strat */ \n");

    for (cLevel=0; cLevel <= ZSTD_MAX_CLEVEL; cLevel++)
        BMK_printWinner(f, cLevel, winners[cLevel].result, winners[cLevel].params, srcSize);
}


static void BMK_printWinners(FILE* f, const winnerInfo_t* winners, size_t srcSize)
{
    fseek(f, 0, SEEK_SET);
    BMK_printWinners2(f, winners, srcSize);
    fflush(f);
    BMK_printWinners2(stdout, winners, srcSize);
}


static int BMK_seed(winnerInfo_t* winners, const ZSTD_parameters params,
              const void* srcBuffer, size_t srcSize,
                    ZSTD_CCtx* ctx)
{
    BMK_result_t testResult;
    int better = 0;
    int cLevel;

    BMK_benchParam(&testResult, srcBuffer, srcSize, ctx, params);

    for (cLevel = 1; cLevel <= ZSTD_MAX_CLEVEL; cLevel++)
    {
        if (testResult.cSpeed < g_cSpeedTarget[cLevel])
            continue;   /* not fast enough for this level */
        if (winners[cLevel].result.cSize==0)
        {
            /* first solution for this cLevel */
            winners[cLevel].result = testResult;
            winners[cLevel].params = params;
            BMK_printWinner(stdout, cLevel, testResult, params, srcSize);
            better = 1;
            continue;
        }

        if ((double)testResult.cSize <= ((double)winners[cLevel].result.cSize * (1. + (0.02 / cLevel))) )
        {
            /* Validate solution is "good enough" */
            double W_ratio = (double)srcSize / testResult.cSize;
            double O_ratio = (double)srcSize / winners[cLevel].result.cSize;
            double W_ratioNote = log (W_ratio);
            double O_ratioNote = log (O_ratio);
            size_t W_DMemUsed = (1 << params.windowLog) + (16 KB);
            size_t O_DMemUsed = (1 << winners[cLevel].params.windowLog) + (16 KB);
            double W_DMemUsed_note = W_ratioNote * ( 40 + 9*cLevel) - log((double)W_DMemUsed);
            double O_DMemUsed_note = O_ratioNote * ( 40 + 9*cLevel) - log((double)O_DMemUsed);

            size_t W_CMemUsed = (1 << params.windowLog) + 4 * (1 << params.hashLog) +
                                ((params.strategy==ZSTD_fast) ? 0 : 4 * (1 << params.contentLog));
            size_t O_CMemUsed = (1 << winners[cLevel].params.windowLog) + 4 * (1 << winners[cLevel].params.hashLog) +
                                ((winners[cLevel].params.strategy==ZSTD_fast) ? 0 :  4 * (1 << winners[cLevel].params.contentLog));
            double W_CMemUsed_note = W_ratioNote * ( 50 + 13*cLevel) - log((double)W_CMemUsed);
            double O_CMemUsed_note = O_ratioNote * ( 50 + 13*cLevel) - log((double)O_CMemUsed);

            double W_CSpeed_note = W_ratioNote * ( 30 + 10*cLevel) + log((double)testResult.cSpeed);
            double O_CSpeed_note = O_ratioNote * ( 30 + 10*cLevel) + log((double)winners[cLevel].result.cSpeed);

            double W_DSpeed_note = W_ratioNote * ( 20 + 2*cLevel) + log((double)testResult.dSpeed);
            double O_DSpeed_note = O_ratioNote * ( 20 + 2*cLevel) + log((double)winners[cLevel].result.dSpeed);


            if (W_DMemUsed_note < O_DMemUsed_note)
            {
                /* uses too much Decompression memory for too little benefit */
                if (W_ratio > O_ratio)
                DISPLAY ("Decompression Memory : %5.3f @ %4.1f MB  vs  %5.3f @ %4.1f MB   : not enough for level %i\n",
                         W_ratio, (double)(W_DMemUsed) / 1024 / 1024,
                         O_ratio, (double)(O_DMemUsed) / 1024 / 1024,   cLevel);
                continue;
            }
            if (W_CMemUsed_note < O_CMemUsed_note)
            {
                /* uses too much memory for compression for too little benefit */
                if (W_ratio > O_ratio)
                DISPLAY ("Compression Memory : %5.3f @ %4.1f MB  vs  %5.3f @ %4.1f MB   : not enough for level %i\n",
                         W_ratio, (double)(W_CMemUsed) / 1024 / 1024,
                         O_ratio, (double)(O_CMemUsed) / 1024 / 1024,   cLevel);
                continue;
            }
            if (W_CSpeed_note   < O_CSpeed_note  )
            {
                /* too large compression speed difference for the compression benefit */
                if (W_ratio > O_ratio)
                DISPLAY ("Compression Speed : %5.3f @ %4.1f MB/s  vs  %5.3f @ %4.1f MB/s   : not enough for level %i\n",
                         W_ratio, (double)(testResult.cSpeed) / 1000.,
                         O_ratio, (double)(winners[cLevel].result.cSpeed) / 1000.,   cLevel);
                continue;
            }
            if (W_DSpeed_note   < O_DSpeed_note  )
            {
                /* too large decompression speed difference for the compression benefit */
                if (W_ratio > O_ratio)
                DISPLAY ("Decompression Speed : %5.3f @ %4.1f MB/s  vs  %5.3f @ %4.1f MB/s   : not enough for level %i\n",
                         W_ratio, (double)(testResult.dSpeed) / 1000.,
                         O_ratio, (double)(winners[cLevel].result.dSpeed) / 1000.,   cLevel);
                continue;
            }

            if (W_ratio < O_ratio)
                DISPLAY("Solution %4.3f selected over %4.3f at level %i, due to better secondary statistics \n", W_ratio, O_ratio, cLevel);

            winners[cLevel].result = testResult;
            winners[cLevel].params = params;
            BMK_printWinner(stdout, cLevel, testResult, params, srcSize);

            better = 1;
        }

    }

    return better;
}


/* nullified useless params, to ensure count stats */
static ZSTD_parameters* sanitizeParams(ZSTD_parameters params)
{
    g_params = params;
    if (params.strategy == ZSTD_fast)
    {
        g_params.contentLog = 0;
        g_params.searchLog = 0;
    }
    return &g_params;
}

#define PARAMTABLELOG   25
#define PARAMTABLESIZE (1<<PARAMTABLELOG)
#define PARAMTABLEMASK (PARAMTABLESIZE-1)
static BYTE g_alreadyTested[PARAMTABLESIZE] = {0};   /* init to zero */

#define NB_TESTS_PLAYED(p) \
    g_alreadyTested[(XXH64(sanitizeParams(p), sizeof(p), 0) >> 3) & PARAMTABLEMASK]


#define MAX(a,b)   ( (a) > (b) ? (a) : (b) )

static void playAround(FILE* f, winnerInfo_t* winners,
                       ZSTD_parameters params,
                       const void* srcBuffer, size_t srcSize,
                       ZSTD_CCtx* ctx)
{
    int nbVariations = 0;
    const int startTime = BMK_GetMilliStart();

    while (BMK_GetMilliSpan(startTime) < g_maxVariationTime)
    {
        ZSTD_parameters p = params;
        U32 nbChanges = (FUZ_rand(&g_rand) & 3) + 1;
        if (nbVariations++ > g_maxNbVariations) break;

        for (; nbChanges; nbChanges--)
        {
            const U32 changeID = FUZ_rand(&g_rand) % 12;
            switch(changeID)
            {
            case 0:
                p.contentLog++; break;
            case 1:
                p.contentLog--; break;
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
            }
        }

        /* validate new conf */
        {
            ZSTD_parameters saved = p;
            ZSTD_validateParams(&p, g_blockSize ? g_blockSize : srcSize);
            if (memcmp(&p, &saved, sizeof(p))) continue;  /* p was invalid */
        }

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


static void BMK_selectRandomStart(
                       FILE* f, winnerInfo_t* winners,
                       const void* srcBuffer, size_t srcSize,
                       ZSTD_CCtx* ctx)
{
    U32 id = (FUZ_rand(&g_rand) % (ZSTD_MAX_CLEVEL+1));
    if ((id==0) || (winners[id].params.windowLog==0))
    {
        /* totally random entry */
        ZSTD_parameters p;
        p.contentLog = FUZ_rand(&g_rand) % (ZSTD_CONTENTLOG_MAX+1 - ZSTD_CONTENTLOG_MIN) + ZSTD_CONTENTLOG_MIN;
        p.hashLog    = FUZ_rand(&g_rand) % (ZSTD_HASHLOG_MAX+1 - ZSTD_HASHLOG_MIN) + ZSTD_HASHLOG_MIN;
        p.searchLog  = FUZ_rand(&g_rand) % (ZSTD_SEARCHLOG_MAX+1 - ZSTD_SEARCHLOG_MIN) + ZSTD_SEARCHLOG_MIN;
        p.windowLog  = FUZ_rand(&g_rand) % (ZSTD_WINDOWLOG_MAX+1 - ZSTD_WINDOWLOG_MIN) + ZSTD_WINDOWLOG_MIN;
        p.searchLength=FUZ_rand(&g_rand) % (ZSTD_SEARCHLENGTH_MAX+1 - ZSTD_SEARCHLENGTH_MIN) + ZSTD_SEARCHLENGTH_MIN;
        p.strategy   = (ZSTD_strategy) (FUZ_rand(&g_rand) % (ZSTD_btlazy2+1));
        playAround(f, winners, p, srcBuffer, srcSize, ctx);
    }
    else
        playAround(f, winners, winners[id].params, srcBuffer, srcSize, ctx);
}


static void BMK_benchMem(void* srcBuffer, size_t srcSize)
{
    ZSTD_CCtx* ctx = ZSTD_createCCtx();
    ZSTD_parameters params;
    winnerInfo_t winners[ZSTD_MAX_CLEVEL+1];
    int i;
    const char* rfName = "grillResults.txt";
    FILE* f;
    const size_t blockSize = g_blockSize ? g_blockSize : srcSize;
    const U32 srcLog = BMK_highbit((U32)(blockSize-1))+1;

    if (g_singleRun)
    {
        BMK_result_t testResult;
        ZSTD_validateParams(&g_params, blockSize);
        BMK_benchParam(&testResult, srcBuffer, srcSize, ctx, g_params);
        DISPLAY("\n");
        return;
    }

    /* init */
    memset(winners, 0, sizeof(winners));
    f = fopen(rfName, "w");
    if (f==NULL) { DISPLAY("error opening %s \n", rfName); exit(1); }

    if (g_target)
        g_cSpeedTarget[1] = g_target * 1000;
    else
    {
        /* baseline config for level 1 */
        BMK_result_t testResult;
        params.windowLog = 18;
        params.hashLog = 14;
        params.contentLog = 1;
        params.searchLog = 1;
        params.searchLength = 7;
        params.strategy = ZSTD_fast;
        ZSTD_validateParams(&params, blockSize);
        BMK_benchParam(&testResult, srcBuffer, srcSize, ctx, params);
        g_cSpeedTarget[1] = (testResult.cSpeed * 15) >> 4;
    }

    /* establish speed objectives (relative to level 1) */
    for (i=2; i<=ZSTD_MAX_CLEVEL; i++)
        g_cSpeedTarget[i] = (g_cSpeedTarget[i-1] * 25) >> 5;

    /* populate initial solution */
    {
        const int tableID = (blockSize > 128 KB);
        const int maxSeeds = g_noSeed ? 1 : ZSTD_MAX_CLEVEL;
        g_seedParams = ZSTD_defaultParameters[tableID];
        for (i=1; i<=maxSeeds; i++)
        {
            const U32 btPlus = (params.strategy == ZSTD_btlazy2);
            params = g_seedParams[i];
            params.windowLog = MIN(srcLog, params.windowLog);
            params.contentLog = MIN(params.windowLog+btPlus, params.contentLog);
            params.searchLog = MIN(params.contentLog, params.searchLog);
            BMK_seed(winners, params, srcBuffer, srcSize, ctx);
        }
    }
    BMK_printWinners(f, winners, srcSize);

    /* start tests */
    {
        const int milliStart = BMK_GetMilliStart();
        int mLength;
        do
        {
            BMK_selectRandomStart(f, winners, srcBuffer, srcSize, ctx);
            mLength = BMK_GetMilliSpan(milliStart);
        } while (mLength < g_grillDuration);
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
    char* origBuff;
    size_t benchedSize = sampleSize;
    const char* name = "Sample 10MiB";

    /* Allocation */
    origBuff = (char*) malloc((size_t)benchedSize);
    if(!origBuff)
    {
        DISPLAY("\nError: not enough memory!\n");
        return 12;
    }

    /* Fill buffer */
    RDG_genBuffer(origBuff, benchedSize, g_compressibility, 0.0, 0);

    /* bench */
    DISPLAY("\r%79s\r", "");
    DISPLAY("using %s %i%%: \n", name, (int)(g_compressibility*100));
    BMK_benchMem(origBuff, benchedSize);

    free(origBuff);
    return 0;
}


int benchFiles(char** fileNamesTable, int nbFiles)
{
    int fileIdx=0;

    /* Loop for each file */
    while (fileIdx<nbFiles)
    {
        FILE* inFile;
        char* inFileName;
        U64   inFileSize;
        size_t benchedSize;
        size_t readSize;
        char* origBuff;

        /* Check file existence */
        inFileName = fileNamesTable[fileIdx++];
        inFile = fopen( inFileName, "rb" );
        if (inFile==NULL)
        {
            DISPLAY( "Pb opening %s\n", inFileName);
            return 11;
        }

        /* Memory allocation & restrictions */
        inFileSize = BMK_GetFileSize(inFileName);
        benchedSize = (size_t) BMK_findMaxMem(inFileSize*3) / 3;
        if ((U64)benchedSize > inFileSize) benchedSize = (size_t)inFileSize;
        if (benchedSize < inFileSize)
        {
            DISPLAY("Not enough memory for '%s' full size; testing %i MB only...\n", inFileName, (int)(benchedSize>>20));
        }

        /* Alloc */
        origBuff = (char*) malloc((size_t)benchedSize);
        if(!origBuff)
        {
            DISPLAY("\nError: not enough memory!\n");
            fclose(inFile);
            return 12;
        }

        /* Fill input buffer */
        DISPLAY("Loading %s...       \r", inFileName);
        readSize = fread(origBuff, 1, benchedSize, inFile);
        fclose(inFile);

        if(readSize != benchedSize)
        {
            DISPLAY("\nError: problem reading file '%s' !!    \n", inFileName);
            free(origBuff);
            return 13;
        }

        /* bench */
        DISPLAY("\r%79s\r", "");
        DISPLAY("using %s : \n", inFileName);
        BMK_benchMem(origBuff, benchedSize);
    }

    return 0;
}


int usage(char* exename)
{
    DISPLAY( "Usage :\n");
    DISPLAY( "      %s [arg] file\n", exename);
    DISPLAY( "Arguments :\n");
    DISPLAY( " file : path to the file used as reference (if none, generates a compressible sample)\n");
    DISPLAY( " -H/-h  : Help (this text + advanced options)\n");
    return 0;
}

int usage_advanced(void)
{
    DISPLAY( "\nAdvanced options :\n");
    DISPLAY( " -i#    : iteration loops [1-9](default : %i)\n", NBLOOPS);
    DISPLAY( " -B#    : cut input into blocks of size # (default : single block)\n");
    DISPLAY( " -P#    : generated sample compressibility (default : %.1f%%)\n", COMPRESSIBILITY_DEFAULT * 100);
    return 0;
}

int badusage(char* exename)
{
    DISPLAY("Wrong parameters\n");
    usage(exename);
    return 1;
}

int main(int argc, char** argv)
{
    int i,
        filenamesStart=0,
        result;
    char* exename=argv[0];
    char* input_filename=0;
    U32 main_pause = 0;

    /* Welcome message */
    DISPLAY(WELCOME_MESSAGE);

    if (argc<1) { badusage(exename); return 1; }

    for(i=1; i<argc; i++)
    {
        char* argument = argv[i];

        if(!argument) continue;   /* Protection if argument empty */

        if(!strcmp(argument,"--no-seed")) { g_noSeed = 1; continue; }

        /* Decode command (note : aggregated commands are allowed) */
        if (argument[0]=='-')
        {
            argument++;

            while (argument[0]!=0)
            {

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
                    if ((argument[0] >='0') && (argument[0] <='9'))
                        g_nbIterations = *argument++ - '0';
                    break;

                    /* Sample compressibility (when no file provided) */
                case 'P':
                    argument++;
                    {
                        U32 proba32 = 0;
                        while ((argument[0]>= '0') && (argument[0]<= '9'))
                        {
                            proba32 *= 10;
                            proba32 += argument[0] - '0';
                            argument++;
                        }
                        g_compressibility = (double)proba32 / 100.;
                    }
                    break;

                    /* Run Single conf */
                case 'S':
                    g_singleRun = 1;
                    argument++;
                    g_params = g_seedParams[2];
                    for ( ; ; )
                    {
                        switch(*argument)
                        {
                        case 'w':
                            g_params.windowLog = 0;
                            argument++;
                            while ((*argument>= '0') && (*argument<='9'))
                                g_params.windowLog *= 10, g_params.windowLog += *argument++ - '0';
                            continue;
                        case 'c':
                            g_params.contentLog = 0;
                            argument++;
                            while ((*argument>= '0') && (*argument<='9'))
                                g_params.contentLog *= 10, g_params.contentLog += *argument++ - '0';
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
                        case 't':  /* strategy */
                            g_params.strategy = (ZSTD_strategy)0;
                            argument++;
                            while ((*argument>= '0') && (*argument<='9'))
                            {
                                g_params.strategy = (ZSTD_strategy)((U32)g_params.strategy *10);
                                g_params.strategy = (ZSTD_strategy)((U32)g_params.strategy + *argument++ - '0');
                            }
                            continue;
                        case 'L':
                            {
                                int cLevel = 0;
                                argument++;
                                while ((*argument>= '0') && (*argument<='9'))
                                    cLevel *= 10, cLevel += *argument++ - '0';
                                g_params = g_seedParams[cLevel];
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
                    {
                        g_target *= 10;
                        g_target += *argument - '0';
                        argument++;
                    }
                    break;

                    /* cut input into blocks */
                case 'B':
                    {
                        g_blockSize = 0;
                        argument++;
                        while ((*argument >='0') && (*argument <='9'))
                            g_blockSize *= 10, g_blockSize += *argument++ - '0';
                        if (*argument=='K') g_blockSize<<=10, argument++;  /* allows using KB notation */
                        if (*argument=='M') g_blockSize<<=20, argument++;
                        if (*argument=='B') argument++;
                        DISPLAY("using %u KB block size \n", g_blockSize>>10);
                    }
                    break;

                    /* Unknown command */
                default : return badusage(exename);
                }
            }
            continue;
        }

        /* first provided filename is input */
        if (!input_filename) { input_filename=argument; filenamesStart=i; continue; }
    }

    if (filenamesStart==0)
        result = benchSample();
    else result = benchFiles(argv+filenamesStart, argc-filenamesStart);

    if (main_pause) { int unused; printf("press enter...\n"); unused = getchar(); (void)unused; }

    return result;
}

