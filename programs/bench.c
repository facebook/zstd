/*
    bench.c - open-source compression benchmark module
    Copyright (C) Yann Collet 2012-2016

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
    - zstd homepage : http://www.zstd.net
    - zstd source repository : https://github.com/Cyan4973/zstd
*/

/* *************************************
*  Includes
***************************************/
#include "util.h"        /* Compiler options, UTIL_GetFileSize, UTIL_sleep */
#include <stdlib.h>      /* malloc, free */
#include <string.h>      /* memset */
#include <stdio.h>       /* fprintf, fopen, ftello64 */

#include "mem.h"
#include "zstd_static.h"
#include "datagen.h"     /* RDG_genBuffer */
#include "xxhash.h"



/* *************************************
*  Constants
***************************************/
#ifndef ZSTD_VERSION
#  define ZSTD_VERSION ""
#endif

#define NBLOOPS               3
#define TIMELOOP_MICROSEC     1*1000000ULL /* 1 second */
#define ACTIVEPERIOD_MICROSEC 70*1000000ULL /* 70 seconds */
#define COOLPERIOD_SEC        10

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

static const size_t maxMemory = (sizeof(size_t)==4)  ?  (2 GB - 64 MB) : (size_t)(1ULL << ((sizeof(size_t)*8)-31));

static U32 g_compressibilityDefault = 50;


/* *************************************
*  console display
***************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); }
static U32 g_displayLevel = 2;   /* 0 : no display;   1: errors;   2 : + result + interaction + warnings;   3 : + progression;   4 : + information */


/* *************************************
*  Exceptions
***************************************/
#ifndef DEBUG
#  define DEBUG 0
#endif
#define DEBUGOUTPUT(...) if (DEBUG) DISPLAY(__VA_ARGS__);
#define EXM_THROW(error, ...)                                             \
{                                                                         \
    DEBUGOUTPUT("Error defined at %s, line %i : \n", __FILE__, __LINE__); \
    DISPLAYLEVEL(1, "Error %i : ", error);                                \
    DISPLAYLEVEL(1, __VA_ARGS__);                                         \
    DISPLAYLEVEL(1, "\n");                                                \
    exit(error);                                                          \
}


/* *************************************
*  Benchmark Parameters
***************************************/
static U32 g_nbIterations = NBLOOPS;
static size_t g_blockSize = 0;
int g_additionalParam = 0;

void BMK_setNotificationLevel(unsigned level) { g_displayLevel=level; }

void BMK_setAdditionalParam(int additionalParam) { g_additionalParam=additionalParam; }

void BMK_SetNbIterations(unsigned nbLoops)
{
    g_nbIterations = nbLoops;
    DISPLAYLEVEL(2, "- %i iterations -\n", g_nbIterations);
}

void BMK_SetBlockSize(size_t blockSize)
{
    g_blockSize = blockSize;
    DISPLAYLEVEL(2, "using blocks of size %u KB \n", (U32)(blockSize>>10));
}


/* ********************************************************
*  Bench functions
**********************************************************/
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

typedef struct
{
    double ratio;
    size_t cSize;
    double cSpeed;
    double dSpeed;
} benchResult_t;


#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define MAX(a,b) ((a)>(b) ? (a) : (b))

static int BMK_benchMem(const void* srcBuffer, size_t srcSize,
                        const char* displayName, int cLevel,
                        const size_t* fileSizes, U32 nbFiles,
                        const void* dictBuffer, size_t dictBufferSize, benchResult_t *result)
{
    size_t const blockSize = (g_blockSize>=32 ? g_blockSize : srcSize) + (!srcSize);   /* avoid div by 0 */
    U32 const maxNbBlocks = (U32) ((srcSize + (blockSize-1)) / blockSize) + nbFiles;
    blockParam_t* const blockTable = (blockParam_t*) malloc(maxNbBlocks * sizeof(blockParam_t));
    size_t const maxCompressedSize = ZSTD_compressBound(srcSize) + (maxNbBlocks * 1024);   /* add some room for safety */
    void* const compressedBuffer = malloc(maxCompressedSize);
    void* const resultBuffer = malloc(srcSize);
    ZSTD_CCtx* refCtx = ZSTD_createCCtx();
    ZSTD_CCtx* ctx = ZSTD_createCCtx();
    ZSTD_DCtx* refDCtx = ZSTD_createDCtx();
    ZSTD_DCtx* dctx = ZSTD_createDCtx();
    U32 nbBlocks;
    UTIL_time_t ticksPerSecond;

    /* checks */
    if (!compressedBuffer || !resultBuffer || !blockTable || !refCtx || !ctx || !refDCtx || !dctx)
        EXM_THROW(31, "not enough memory");

    /* init */
    if (strlen(displayName)>17) displayName += strlen(displayName)-17;   /* can only display 17 characters */
    UTIL_initTimer(&ticksPerSecond);

    /* Init blockTable data */
    {   const char* srcPtr = (const char*)srcBuffer;
        char* cPtr = (char*)compressedBuffer;
        char* resPtr = (char*)resultBuffer;
        U32 fileNb;
        for (nbBlocks=0, fileNb=0; fileNb<nbFiles; fileNb++) {
            size_t remaining = fileSizes[fileNb];
            U32 const nbBlocksforThisFile = (U32)((remaining + (blockSize-1)) / blockSize);
            U32 const blockEnd = nbBlocks + nbBlocksforThisFile;
            for ( ; nbBlocks<blockEnd; nbBlocks++) {
                size_t const thisBlockSize = MIN(remaining, blockSize);
                blockTable[nbBlocks].srcPtr = srcPtr;
                blockTable[nbBlocks].cPtr = cPtr;
                blockTable[nbBlocks].resPtr = resPtr;
                blockTable[nbBlocks].srcSize = thisBlockSize;
                blockTable[nbBlocks].cRoom = ZSTD_compressBound(thisBlockSize);
                srcPtr += thisBlockSize;
                cPtr += blockTable[nbBlocks].cRoom;
                resPtr += thisBlockSize;
                remaining -= thisBlockSize;
    }   }   }

    /* warmimg up memory */
    RDG_genBuffer(compressedBuffer, maxCompressedSize, 0.10, 0.50, 1);

    /* Bench */
    {   U64 fastestC = (U64)(-1LL), fastestD = (U64)(-1LL);
        U64 const crcOrig = XXH64(srcBuffer, srcSize, 0);
        UTIL_time_t coolTime;
        U32 testNb;
        size_t cSize = 0;
        double ratio = 0.;

        UTIL_getTime(&coolTime);
        DISPLAYLEVEL(2, "\r%79s\r", "");
        for (testNb = 1; testNb <= (g_nbIterations + !g_nbIterations); testNb++) {
            UTIL_time_t clockStart;
            U64 clockLoop = g_nbIterations ? TIMELOOP_MICROSEC : 1;

            /* overheat protection */
            if (UTIL_clockSpanMicro(coolTime, ticksPerSecond) > ACTIVEPERIOD_MICROSEC) {
                DISPLAY("\rcooling down ...    \r");
                UTIL_sleep(COOLPERIOD_SEC);
                UTIL_getTime(&coolTime);
            }

            /* Compression */
            DISPLAYLEVEL(2, "%2i-%-17.17s :%10u ->\r", testNb, displayName, (U32)srcSize);
            memset(compressedBuffer, 0xE5, maxCompressedSize);  /* warm up and erase result buffer */

            UTIL_sleepMilli(1); /* give processor time to other processes */
            UTIL_waitForNextTick(ticksPerSecond);
            UTIL_getTime(&clockStart);

            {   U32 nbLoops = 0;
                do {
                    U32 blockNb;
                    {   ZSTD_parameters params;
                        params.cParams = ZSTD_getCParams(cLevel, blockSize, dictBufferSize);
                        params.fParams.contentSizeFlag = 1;
                        ZSTD_adjustCParams(&params.cParams, blockSize, dictBufferSize);
                        {   size_t const initResult = ZSTD_compressBegin_advanced(refCtx, dictBuffer, dictBufferSize, params, blockSize);
                            if (ZSTD_isError(initResult)) break;
                    }   }
                    for (blockNb=0; blockNb<nbBlocks; blockNb++) {
                        size_t const rSize = ZSTD_compress_usingPreparedCCtx(ctx, refCtx,
                                            blockTable[blockNb].cPtr,  blockTable[blockNb].cRoom,
                                            blockTable[blockNb].srcPtr,blockTable[blockNb].srcSize);
                        if (ZSTD_isError(rSize)) EXM_THROW(1, "ZSTD_compress_usingPreparedCCtx() failed : %s", ZSTD_getErrorName(rSize));
                        blockTable[blockNb].cSize = rSize;
                    }
                    nbLoops++;
                } while (UTIL_clockSpanMicro(clockStart, ticksPerSecond) < clockLoop);
                {   U64 const clockSpan = UTIL_clockSpanMicro(clockStart, ticksPerSecond);
                    if (clockSpan < fastestC*nbLoops) fastestC = clockSpan / nbLoops;
            }   }

            cSize = 0;
            { U32 blockNb; for (blockNb=0; blockNb<nbBlocks; blockNb++) cSize += blockTable[blockNb].cSize; }
            ratio = (double)srcSize / (double)cSize;
            DISPLAYLEVEL(2, "%2i-%-17.17s :%10u ->%10u (%5.3f),%6.1f MB/s\r",
                    testNb, displayName, (U32)srcSize, (U32)cSize, ratio,
                    (double)srcSize / fastestC );

            (void)fastestD; (void)crcOrig;   /*  unused when decompression disabled */
#if 1
            /* Decompression */
            memset(resultBuffer, 0xD6, srcSize);  /* warm result buffer */

            UTIL_sleepMilli(1); /* give processor time to other processes */
            UTIL_waitForNextTick(ticksPerSecond);
            UTIL_getTime(&clockStart);

            {   U32 nbLoops = 0;
                do {
                    U32 blockNb;
                    ZSTD_decompressBegin_usingDict(refDCtx, dictBuffer, dictBufferSize);
                    for (blockNb=0; blockNb<nbBlocks; blockNb++) {
                        size_t const regenSize = ZSTD_decompress_usingPreparedDCtx(dctx, refDCtx,
                            blockTable[blockNb].resPtr, blockTable[blockNb].srcSize,
                            blockTable[blockNb].cPtr, blockTable[blockNb].cSize);
                        if (ZSTD_isError(regenSize)) {
                            DISPLAY("ZSTD_decompress_usingPreparedDCtx() failed on block %u : %s  \n",
                                      blockNb, ZSTD_getErrorName(regenSize));
                            clockLoop = 0;   /* force immediate test end */
                            break;
                        }
                        blockTable[blockNb].resSize = regenSize;
                    }
                    nbLoops++;
                } while (UTIL_clockSpanMicro(clockStart, ticksPerSecond) < clockLoop);
                {   U64 const clockSpan = UTIL_clockSpanMicro(clockStart, ticksPerSecond);
                    if (clockSpan < fastestD*nbLoops) fastestD = clockSpan / nbLoops;
            }   }

            DISPLAYLEVEL(2, "%2i-%-17.17s :%10u ->%10u (%5.3f),%6.1f MB/s ,%6.1f MB/s\r",
                    testNb, displayName, (U32)srcSize, (U32)cSize, ratio,
                    (double)srcSize / fastestC,
                    (double)srcSize / fastestD );

            /* CRC Checking */
            {   U64 const crcCheck = XXH64(resultBuffer, srcSize, 0);
                if (crcOrig!=crcCheck) {
                    size_t u;
                    DISPLAY("!!! WARNING !!! %14s : Invalid Checksum : %x != %x   \n", displayName, (unsigned)crcOrig, (unsigned)crcCheck);
                    for (u=0; u<srcSize; u++) {
                        if (((const BYTE*)srcBuffer)[u] != ((const BYTE*)resultBuffer)[u]) {
                            U32 segNb, bNb, pos;
                            size_t bacc = 0;
                            DISPLAY("Decoding error at pos %u ", (U32)u);
                            for (segNb = 0; segNb < nbBlocks; segNb++) {
                                if (bacc + blockTable[segNb].srcSize > u) break;
                                bacc += blockTable[segNb].srcSize;
                            }
                            pos = (U32)(u - bacc);
                            bNb = pos / (128 KB);
                            DISPLAY("(block %u, sub %u, pos %u) \n", segNb, bNb, pos);
                            break;
                        }
                        if (u==srcSize-1) {  /* should never happen */
                            DISPLAY("no difference detected\n");
                    }   }
                    break;
            }   }   /* CRC Checking */
#endif
        }   /* for (testNb = 1; testNb <= (g_nbIterations + !g_nbIterations); testNb++) */

        result->ratio = ratio;
        result->cSize = cSize;
        result->cSpeed = (double)srcSize / fastestC;
        result->dSpeed = (double)srcSize / fastestD;
        DISPLAYLEVEL(2, "%2i#\n", cLevel);
    }   /* Bench */

    /* clean up */
    free(blockTable);
    free(compressedBuffer);
    free(resultBuffer);
    ZSTD_freeCCtx(refCtx);
    ZSTD_freeCCtx(ctx);
    ZSTD_freeDCtx(refDCtx);
    ZSTD_freeDCtx(dctx);
    return 0;
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

static void BMK_benchCLevel(void* srcBuffer, size_t benchedSize,
                            const char* displayName, int cLevel, int cLevelLast,
                            const size_t* fileSizes, unsigned nbFiles,
                            const void* dictBuffer, size_t dictBufferSize)
{
    benchResult_t result, total;
    int l;

    const char* pch = strrchr(displayName, '\\'); /* Windows */
    if (!pch) pch = strrchr(displayName, '/'); /* Linux */
    if (pch) displayName = pch+1;

    SET_HIGH_PRIORITY;

    memset(&result, 0, sizeof(result));
    memset(&total, 0, sizeof(total));

    if (g_displayLevel == 1 && !g_additionalParam)
        DISPLAY("bench %s: input %u bytes, %i iterations, %u KB blocks\n", ZSTD_VERSION, (U32)benchedSize, g_nbIterations, (U32)(g_blockSize>>10));

    if (cLevelLast < cLevel) cLevelLast = cLevel;

    for (l=cLevel; l <= cLevelLast; l++) {
        BMK_benchMem(srcBuffer, benchedSize,
                     displayName, l,
                     fileSizes, nbFiles,
                     dictBuffer, dictBufferSize, &result);
        if (g_displayLevel == 1) {
            if (g_additionalParam)
                DISPLAY("%-3i%11i (%5.3f) %6.2f MB/s %6.1f MB/s  %s (param=%d)\n", -l, (int)result.cSize, result.ratio, result.cSpeed, result.dSpeed, displayName, g_additionalParam);
            else
                DISPLAY("%-3i%11i (%5.3f) %6.2f MB/s %6.1f MB/s  %s\n", -l, (int)result.cSize, result.ratio, result.cSpeed, result.dSpeed, displayName);
            total.cSize += result.cSize;
            total.cSpeed += result.cSpeed;
            total.dSpeed += result.dSpeed;
            total.ratio += result.ratio;
    }   }
    if (g_displayLevel == 1 && cLevelLast > cLevel) {
        total.cSize /= 1+cLevelLast-cLevel;
        total.cSpeed /= 1+cLevelLast-cLevel;
        total.dSpeed /= 1+cLevelLast-cLevel;
        total.ratio /= 1+cLevelLast-cLevel;
        DISPLAY("avg%11i (%5.3f) %6.2f MB/s %6.1f MB/s  %s\n", (int)total.cSize, total.ratio, total.cSpeed, total.dSpeed, displayName);
    }
}


/*! BMK_loadFiles() :
    Loads `buffer` with content of files listed within `fileNamesTable`.
    At most, fills `buffer` entirely */
static void BMK_loadFiles(void* buffer, size_t bufferSize,
                          size_t* fileSizes,
                          const char** fileNamesTable, unsigned nbFiles)
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
        f = fopen(fileNamesTable[n], "rb");
        if (f==NULL) EXM_THROW(10, "impossible to open file %s", fileNamesTable[n]);
        DISPLAYLEVEL(2, "Loading %s...       \r", fileNamesTable[n]);
        if (fileSize > bufferSize-pos) fileSize = bufferSize-pos, nbFiles=n;   /* buffer too small - stop after this file */
        { size_t const readSize = fread(((char*)buffer)+pos, 1, (size_t)fileSize, f);
          if (readSize != (size_t)fileSize) EXM_THROW(11, "could not read %s", fileNamesTable[n]);
          pos += readSize; }
        fileSizes[n] = (size_t)fileSize;
        totalSize += (size_t)fileSize;
        fclose(f);
    }

    if (totalSize == 0) EXM_THROW(12, "no data to bench");
}

static void BMK_benchFileTable(const char** fileNamesTable, unsigned nbFiles,
                               const char* dictFileName, int cLevel, int cLevelLast)
{
    void* srcBuffer;
    size_t benchedSize;
    void* dictBuffer = NULL;
    size_t dictBufferSize = 0;
    size_t* fileSizes = (size_t*)malloc(nbFiles * sizeof(size_t));
    U64 const totalSizeToLoad = UTIL_getTotalFileSize(fileNamesTable, nbFiles);
    char mfName[20] = {0};

    if (!fileSizes) EXM_THROW(12, "not enough memory for fileSizes");

    /* Load dictionary */
    if (dictFileName != NULL) {
        U64 dictFileSize = UTIL_getFileSize(dictFileName);
        if (dictFileSize > 64 MB) EXM_THROW(10, "dictionary file %s too large", dictFileName);
        dictBufferSize = (size_t)dictFileSize;
        dictBuffer = malloc(dictBufferSize);
        if (dictBuffer==NULL) EXM_THROW(11, "not enough memory for dictionary (%u bytes)", (U32)dictBufferSize);
        BMK_loadFiles(dictBuffer, dictBufferSize, fileSizes, &dictFileName, 1);
    }

    /* Memory allocation & restrictions */
    benchedSize = BMK_findMaxMem(totalSizeToLoad * 3) / 3;
    if ((U64)benchedSize > totalSizeToLoad) benchedSize = (size_t)totalSizeToLoad;
    if (benchedSize < totalSizeToLoad)
        DISPLAY("Not enough memory; testing %u MB only...\n", (U32)(benchedSize >> 20));
    srcBuffer = malloc(benchedSize);
    if (!srcBuffer) EXM_THROW(12, "not enough memory");

    /* Load input buffer */
    BMK_loadFiles(srcBuffer, benchedSize, fileSizes, fileNamesTable, nbFiles);

    /* Bench */
    snprintf (mfName, sizeof(mfName), " %u files", nbFiles);
    {   const char* displayName = (nbFiles > 1) ? mfName : fileNamesTable[0];
        BMK_benchCLevel(srcBuffer, benchedSize,
                        displayName, cLevel, cLevelLast,
                        fileSizes, nbFiles,
                        dictBuffer, dictBufferSize);
    }

    /* clean up */
    free(srcBuffer);
    free(dictBuffer);
    free(fileSizes);
}


static void BMK_syntheticTest(int cLevel, int cLevelLast, double compressibility)
{
    char name[20] = {0};
    size_t benchedSize = 10000000;
    void* const srcBuffer = malloc(benchedSize);

    /* Memory allocation */
    if (!srcBuffer) EXM_THROW(21, "not enough memory");

    /* Fill input buffer */
    RDG_genBuffer(srcBuffer, benchedSize, compressibility, 0.0, 0);

    /* Bench */
    snprintf (name, sizeof(name), "Synthetic %2u%%", (unsigned)(compressibility*100));
    BMK_benchCLevel(srcBuffer, benchedSize, name, cLevel, cLevelLast, &benchedSize, 1, NULL, 0);

    /* clean up */
    free(srcBuffer);
}


int BMK_benchFiles(const char** fileNamesTable, unsigned nbFiles,
                   const char* dictFileName, int cLevel, int cLevelLast)
{
    double const compressibility = (double)g_compressibilityDefault / 100;

    if (nbFiles == 0)
        BMK_syntheticTest(cLevel, cLevelLast, compressibility);
    else
        BMK_benchFileTable(fileNamesTable, nbFiles, dictFileName, cLevel, cLevelLast);
    return 0;
}

