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


/**************************************
*  Includes
**************************************/
#include <stdlib.h>       /* malloc */
#include <stdio.h>        /* fprintf, fopen, ftello64 */
#include <sys/types.h>    /* stat64 */
#include <sys/stat.h>     /* stat64 */
#include <string.h>       /* strcmp */

/* Use ftime() if gettimeofday() is not available on your target */
#if defined(BMK_LEGACY_TIMER)
#  include <sys/timeb.h>  /* timeb, ftime */
#else
#  include <sys/time.h>   /* gettimeofday */
#endif

#include "mem.h"
#include "zstdhc_static.h"
#include "zstd.h"
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

#define NBLOOPS    3
#define TIMELOOP   2500

#define KNUTH      2654435761U
#define MAX_MEM    (1984 MB)
#define DEFAULT_CHUNKSIZE   (4<<20)

#define COMPRESSIBILITY_DEFAULT 0.50
static const size_t sampleSize = 10000000;


/**************************************
*  Macros
**************************************/
#define DISPLAY(...)  fprintf(stderr, __VA_ARGS__)


/**************************************
*  Benchmark Parameters
**************************************/
static U32 nbIterations = NBLOOPS;
static double g_compressibility = COMPRESSIBILITY_DEFAULT;
static U32 g_blockSize = 0;

void BMK_SetNbIterations(int nbLoops)
{
    nbIterations = nbLoops;
    DISPLAY("- %u iterations -\n", nbIterations);
}


/*********************************************************
*  Private functions
*********************************************************/

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
                             ZSTD_HC_CCtx* ctx,
                             const ZSTD_HC_parameters params)
{
    const size_t blockSize = g_blockSize ? g_blockSize : srcSize;
    const U32 nbBlocks = (U32) ((srcSize + (blockSize-1)) / blockSize);
    blockParam_t* const blockTable = (blockParam_t*) malloc(nbBlocks * sizeof(blockParam_t));
    const size_t maxCompressedSize = (size_t)nbBlocks * ZSTD_compressBound(blockSize);
    void* const compressedBuffer = malloc(maxCompressedSize);
    void* const resultBuffer = malloc(srcSize);
    U32 Wlog = params.windowLog;
    U32 Clog = params.chainLog;
    U32 Hlog = params.hashLog;
    U32 Slog = params.searchLog;
    U64 crcOrig;

    /* Memory allocation & restrictions */
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

        DISPLAY("\r%79s\r", "");
        for (loopNb = 1; loopNb <= nbIterations; loopNb++)
        {
            int nbLoops;
            int milliTime;
            U32 blockNb;

            /* Compression */
            DISPLAY("%1u-W%02uC%02uH%02uS%02u : %9u ->\r", loopNb, Wlog, Clog, Hlog, Slog, (U32)srcSize);
            memset(compressedBuffer, 0xE5, maxCompressedSize);

            nbLoops = 0;
            milliTime = BMK_GetMilliStart();
            while (BMK_GetMilliStart() == milliTime);
            milliTime = BMK_GetMilliStart();
            while (BMK_GetMilliSpan(milliTime) < TIMELOOP)
            {
                for (blockNb=0; blockNb<nbBlocks; blockNb++)
                    blockTable[blockNb].cSize = ZSTD_HC_compress_advanced(ctx,
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
            DISPLAY("%1u-W%02uC%02uH%02uS%02u : %9u ->", loopNb, Wlog, Clog, Hlog, Slog, (U32)srcSize);
            DISPLAY(" %9u (%4.3f),%7.1f MB/s\r", (U32)cSize, ratio, (double)srcSize / fastestC / 1000.);
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
            DISPLAY("%1u-W%02uC%02uH%02uS%02u : %9u -> ", loopNb, Wlog, Clog, Hlog, Slog, (U32)srcSize);
            DISPLAY("%9u (%4.3f),%7.1f MB/s, ", (U32)cSize, ratio, (double)srcSize / fastestC / 1000.);
            DISPLAY("%7.1f MB/s\r", (double)srcSize / fastestD / 1000.);
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
    free(compressedBuffer);
    free(resultBuffer);
    return 0;
}


static void BMK_printWinner(U32 cLevel, BMK_result_t result, ZSTD_HC_parameters params, size_t srcSize)
{
    DISPLAY("\r%79s\r", "");
    DISPLAY("level %2u: R:%5.3f at %5.1f MB/s => W%02uC%02uH%02uS%02u \n",
            cLevel, (double)srcSize / result.cSize, (double)result.cSpeed / 1000.,
            params.windowLog, params.chainLog, params.hashLog, params.searchLog);
}


#define CLEVEL_MAX 20
typedef struct {
    BMK_result_t result;
    ZSTD_HC_parameters params;
} winnerInfo_t;

static void BMK_printWinners(const winnerInfo_t* winners, size_t srcSize)
{
    int cLevel;

    DISPLAY("\nSelected configurations :\n");
    for (cLevel=0; cLevel < CLEVEL_MAX; cLevel++)
        BMK_printWinner(cLevel, winners[cLevel].result, winners[cLevel].params, srcSize);

}

#define WINDOWLOG_MAX 26
#define WINDOWLOG_MIN 17
#define CHAINLOG_MAX WINDOWLOG_MAX
#define CHAINLOG_MIN 4
#define HASHLOG_MAX (CHAINLOG_MAX+4)
#define HASHLOG_MIN 4
#define SEARCHLOG_MAX CHAINLOG_MAX
#define SEARCHLOG_MIN 1

U32 g_cSpeedTarget[CLEVEL_MAX] = { 300000, 200000, 150000, 100000, 70000, 50000, 35000, 25000, 15000, 10000, /* 0 - 9 */
                                   7000, 5000, 3500, 2500, 1500, 1000, 700, 500, 350, 250 }; /* 10 - 19 */

static void playAround(winnerInfo_t* winners, ZSTD_HC_parameters params,
                       const void* srcBuffer, size_t srcSize,
                       ZSTD_HC_CCtx* ctx)
{
    BMK_result_t testResult;
    int better = 0;
    int cLevel;

    BMK_benchParam(&testResult, srcBuffer, srcSize, ctx, params);

    for (cLevel = 0; cLevel < CLEVEL_MAX; cLevel++)
    {
        if ( (testResult.cSpeed > g_cSpeedTarget[cLevel])
            && ((winners[cLevel].result.cSize==0) || (winners[cLevel].result.cSize > testResult.cSize)) )
        {
            better = 1;
            winners[cLevel].result = testResult;
            winners[cLevel].params = params;
            BMK_printWinner(cLevel, testResult, params, srcSize);
        }
    }

    if (!better) return;

    BMK_printWinners(winners, srcSize);

    if (params.searchLog > SEARCHLOG_MIN)
    {
        params.searchLog--;
        playAround(winners, params, srcBuffer, srcSize, ctx);
        params.searchLog++;
    }

    if (params.hashLog < HASHLOG_MAX)
    {
        params.hashLog++;
        playAround(winners, params, srcBuffer, srcSize, ctx);
        params.hashLog--;
    }

    if (params.chainLog > CHAINLOG_MIN)
    {
        params.chainLog--;
        playAround(winners, params, srcBuffer, srcSize, ctx);
        params.chainLog++;
    }

    if (params.windowLog > params.chainLog)
    {
        params.windowLog--;
        playAround(winners, params, srcBuffer, srcSize, ctx);
        params.windowLog++;
    }

    if (params.windowLog < WINDOWLOG_MAX)
    {
        params.windowLog++;
        playAround(winners, params, srcBuffer, srcSize, ctx);
        params.windowLog--;
    }

    if (params.chainLog < params.windowLog)
    {
        params.chainLog++;
        playAround(winners, params, srcBuffer, srcSize, ctx);
        params.chainLog--;
    }

    if (params.hashLog > HASHLOG_MIN)
    {
        params.hashLog--;
        playAround(winners, params, srcBuffer, srcSize, ctx);
        params.hashLog++;
    }

    if (params.searchLog < params.chainLog)
    {
        params.searchLog++;
        playAround(winners, params, srcBuffer, srcSize, ctx);
        params.searchLog--;
    }

}

static void BMK_benchMem(void* srcBuffer, size_t srcSize)
{
    ZSTD_HC_CCtx* ctx = ZSTD_HC_createCCtx();
    ZSTD_HC_parameters params;
    winnerInfo_t winners[CLEVEL_MAX];
    BMK_result_t testResult;

    /* init */
    memset(winners, 0, sizeof(winners));
    params.windowLog = 19;
    params.chainLog = 18;
    params.hashLog = 17;
    params.searchLog = 9;

    /* set start config for level 9 */
    BMK_benchParam(&testResult, srcBuffer, srcSize, ctx, params);

    {
        int i;
        g_cSpeedTarget[9] = (testResult.cSpeed * 15) >> 4;
        g_cSpeedTarget[1] = g_cSpeedTarget[9] << 4;
        g_cSpeedTarget[0] = (g_cSpeedTarget[1] * 181) >> 7;  /* sqrt2 */
        for (i=2; i<CLEVEL_MAX; i++)
            g_cSpeedTarget[i] = (g_cSpeedTarget[i-1] * 181) >> 8;
    }

    /* establish speed objectives (relative to current platform) */
    playAround(winners, params, srcBuffer, srcSize, ctx);

    /* end summary */
    BMK_printWinners(winners, srcSize);

    /* clean up*/
    ZSTD_HC_freeCCtx(ctx);
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

        // Memory allocation & restrictions
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
    DISPLAY( " -H/-h  : Help (this text + advanced options)\n");
    return 0;
}

int usage_advanced(void)
{
    DISPLAY( "\nAdvanced options :\n");
    DISPLAY( " -i#    : iteration loops [1-9](default : %i)\n", NBLOOPS);
    DISPLAY( " -P#    : sample compressibility (default : %.1f%%)\n", COMPRESSIBILITY_DEFAULT * 100);
    return 0;
}

int badusage(char* exename)
{
    DISPLAY("Wrong parameters\n");
    usage(exename);
    return 0;
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

        if(!argument) continue;   // Protection if argument empty

        // Decode command (note : aggregated commands are allowed)
        if (argument[0]=='-')
        {
            while (argument[1]!=0)
            {
                argument ++;

                switch(argument[0])
                {
                    /* Display help on usage */
                case 'h' :
                case 'H': usage(exename); usage_advanced(); return 0;

                    /* Pause at the end (hidden option) */
                case 'p': main_pause = 1; break;

                    /* Modify Nb Iterations */
                case 'i':
                    if ((argument[1] >='0') && (argument[1] <='9'))
                    {
                        int iters = argument[1] - '0';
                        BMK_SetNbIterations(iters);
                        argument++;
                    }
                    break;

                    /* Sample compressibility (when no file provided) */
                case 'P':
                    {
                        U32 proba32 = 0;
                        while ((argument[1]>= '0') && (argument[1]<= '9'))
                        {
                            proba32 *= 10;
                            proba32 += argument[1] - '0';
                            argument++;
                        }
                        g_compressibility = (double)proba32 / 100.;
                    }
                    break;

                    /* Unknown command */
                default : badusage(exename); return 1;
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

