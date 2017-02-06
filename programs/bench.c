/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
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
#  pragma warning(disable : 4127)                /* disable: C4127: conditional expression is constant */
#endif


/* *************************************
*  Includes
***************************************/
#include "platform.h"    /* Large Files support */
#include "util.h"        /* UTIL_getFileSize, UTIL_sleep */
#include <stdlib.h>      /* malloc, free */
#include <string.h>      /* memset */
#include <stdio.h>       /* fprintf, fopen */
#include <time.h>        /* clock_t, clock, CLOCKS_PER_SEC */

#include "mem.h"
#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"
#include "datagen.h"     /* RDG_genBuffer */
#include "xxhash.h"
#include "zstdmt_compress.h"


/* *************************************
*  Constants
***************************************/
#ifndef ZSTD_GIT_COMMIT
#  define ZSTD_GIT_COMMIT_STRING ""
#else
#  define ZSTD_GIT_COMMIT_STRING ZSTD_EXPAND_AND_QUOTE(ZSTD_GIT_COMMIT)
#endif

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

#define DISPLAYUPDATE(l, ...) if (g_displayLevel>=l) { \
            if ((clock() - g_time > refreshRate) || (g_displayLevel>=4)) \
            { g_time = clock(); DISPLAY(__VA_ARGS__); \
            if (g_displayLevel>=4) fflush(stdout); } }
static const clock_t refreshRate = CLOCKS_PER_SEC * 15 / 100;
static clock_t g_time = 0;


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
    DISPLAYLEVEL(1, " \n");                                               \
    exit(error);                                                          \
}


/* *************************************
*  Benchmark Parameters
***************************************/
static int g_additionalParam = 0;
static U32 g_decodeOnly = 0;

void BMK_setNotificationLevel(unsigned level) { g_displayLevel=level; }

void BMK_setAdditionalParam(int additionalParam) { g_additionalParam=additionalParam; }

static U32 g_nbSeconds = BMK_TIMETEST_DEFAULT_S;
void BMK_setNbSeconds(unsigned nbSeconds)
{
    g_nbSeconds = nbSeconds;
    DISPLAYLEVEL(3, "- test >= %u seconds per compression / decompression - \n", g_nbSeconds);
}

static size_t g_blockSize = 0;
void BMK_setBlockSize(size_t blockSize)
{
    g_blockSize = blockSize;
    if (g_blockSize) DISPLAYLEVEL(2, "using blocks of size %u KB \n", (U32)(blockSize>>10));
}

void BMK_setDecodeOnlyMode(unsigned decodeFlag) { g_decodeOnly = (decodeFlag>0); }

static U32 g_nbThreads = 1;
void BMK_setNbThreads(unsigned nbThreads) {
#ifndef ZSTD_MULTITHREAD
    if (nbThreads > 1) DISPLAYLEVEL(2, "Note : multi-threading is disabled \n");
#endif
    g_nbThreads = nbThreads;
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


#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define MAX(a,b) ((a)>(b) ? (a) : (b))

static int BMK_benchMem(const void* srcBuffer, size_t srcSize,
                        const char* displayName, int cLevel,
                        const size_t* fileSizes, U32 nbFiles,
                        const void* dictBuffer, size_t dictBufferSize,
                        ZSTD_compressionParameters *comprParams)
{
    size_t const blockSize = ((g_blockSize>=32 && !g_decodeOnly) ? g_blockSize : srcSize) + (!srcSize) /* avoid div by 0 */ ;
    size_t const avgSize = MIN(g_blockSize, (srcSize / nbFiles));
    U32 const maxNbBlocks = (U32) ((srcSize + (blockSize-1)) / blockSize) + nbFiles;
    blockParam_t* const blockTable = (blockParam_t*) malloc(maxNbBlocks * sizeof(blockParam_t));
    size_t const maxCompressedSize = ZSTD_compressBound(srcSize) + (maxNbBlocks * 1024);   /* add some room for safety */
    void* const compressedBuffer = malloc(maxCompressedSize);
    void* resultBuffer = malloc(srcSize);
    ZSTDMT_CCtx* const mtctx = ZSTDMT_createCCtx(g_nbThreads);
    ZSTD_CCtx* const ctx = ZSTD_createCCtx();
    ZSTD_DCtx* const dctx = ZSTD_createDCtx();
    size_t const loadedCompressedSize = srcSize;
    size_t cSize = 0;
    double ratio = 0.;
    U32 nbBlocks;
    UTIL_time_t ticksPerSecond;

    /* checks */
    if (!compressedBuffer || !resultBuffer || !blockTable || !ctx || !dctx)
        EXM_THROW(31, "allocation error : not enough memory");

    /* init */
    if (strlen(displayName)>17) displayName += strlen(displayName)-17;   /* can only display 17 characters */
    UTIL_initTimer(&ticksPerSecond);

    if (g_decodeOnly) {
        const char* srcPtr = (const char*) srcBuffer;
        U64 dSize64 = 0;
        U32 fileNb;
        for (fileNb=0; fileNb<nbFiles; fileNb++) {
            U64 const fSize64 = ZSTD_getDecompressedSize(srcPtr, fileSizes[fileNb]);
            if (fSize64==0) EXM_THROW(32, "Impossible to determine original size ");
            dSize64 += fSize64;
            srcPtr += fileSizes[fileNb];
        }
        {   size_t const decodedSize = (size_t)dSize64;
            if (dSize64 > decodedSize) EXM_THROW(32, "original size is too large");
            if (decodedSize==0) EXM_THROW(32, "Impossible to determine original size ");
            free(resultBuffer);
            resultBuffer = malloc(decodedSize);
            if (!resultBuffer) EXM_THROW(33, "not enough memory");
            cSize = srcSize;
            srcSize = decodedSize;
            ratio = (double)srcSize / (double)cSize;
    }   }

    /* Init blockTable data */
    {   const char* srcPtr = (const char*)srcBuffer;
        char* cPtr = (char*)compressedBuffer;
        char* resPtr = (char*)resultBuffer;
        U32 fileNb;
        for (nbBlocks=0, fileNb=0; fileNb<nbFiles; fileNb++) {
            size_t remaining = fileSizes[fileNb];
            U32 const nbBlocksforThisFile = g_decodeOnly ? 1 : (U32)((remaining + (blockSize-1)) / blockSize);
            U32 const blockEnd = nbBlocks + nbBlocksforThisFile;
            for ( ; nbBlocks<blockEnd; nbBlocks++) {
                size_t const thisBlockSize = MIN(remaining, blockSize);
                blockTable[nbBlocks].srcPtr = (const void*)srcPtr;
                blockTable[nbBlocks].srcSize = thisBlockSize;
                blockTable[nbBlocks].cPtr = (void*)cPtr;
                blockTable[nbBlocks].cRoom = g_decodeOnly ? thisBlockSize : ZSTD_compressBound(thisBlockSize);
                blockTable[nbBlocks].cSize = blockTable[nbBlocks].cRoom;
                blockTable[nbBlocks].resPtr = (void*)resPtr;
                blockTable[nbBlocks].resSize = g_decodeOnly ? (size_t) ZSTD_getDecompressedSize(srcPtr, thisBlockSize) : thisBlockSize;
                srcPtr += thisBlockSize;
                cPtr += blockTable[nbBlocks].cRoom;
                resPtr += thisBlockSize;
                remaining -= thisBlockSize;
    }   }   }

    /* warmimg up memory */
    RDG_genBuffer(compressedBuffer, maxCompressedSize, 0.10, 0.50, 1);

    /* Bench */
    {   U64 fastestC = (U64)(-1LL), fastestD = (U64)(-1LL);
        U64 const crcOrig = g_decodeOnly ? 0 : XXH64(srcBuffer, srcSize, 0);
        UTIL_time_t coolTime, coolTick;
        U64 const maxTime = (g_nbSeconds * TIMELOOP_MICROSEC) + 1;
        U64 totalCTime=0, totalDTime=0;
        U32 cCompleted=g_decodeOnly, dCompleted=0;
#       define NB_MARKS 4
        const char* const marks[NB_MARKS] = { " |", " /", " =",  "\\" };
        U32 markNb = 0;

        UTIL_initTimer(&coolTick);
        UTIL_getTime(&coolTime);
        DISPLAYLEVEL(2, "\r%79s\r", "");
        while (!cCompleted || !dCompleted) {

            /* overheat protection */
            if (UTIL_clockSpanMicro(coolTime, coolTick) > ACTIVEPERIOD_MICROSEC) {
                DISPLAYLEVEL(2, "\rcooling down ...    \r");
                UTIL_sleep(COOLPERIOD_SEC);
                UTIL_getTime(&coolTime);
            }

            if (!g_decodeOnly) {
                UTIL_time_t clockTick, clockStart;
                /* Compression */
                DISPLAYLEVEL(2, "%2s-%-17.17s :%10u ->\r", marks[markNb], displayName, (U32)srcSize);
                if (!cCompleted) memset(compressedBuffer, 0xE5, maxCompressedSize);  /* warm up and erase result buffer */

                UTIL_sleepMilli(1);  /* give processor time to other processes */
                UTIL_waitForNextTick(ticksPerSecond);
                UTIL_initTimer(&clockTick);
                UTIL_getTime(&clockStart);

                if (!cCompleted) {   /* still some time to do compression tests */
                    ZSTD_parameters zparams = ZSTD_getParams(cLevel, avgSize, dictBufferSize);
                    ZSTD_customMem const cmem = { NULL, NULL, NULL };
                    U64 clockLoop = g_nbSeconds ? TIMELOOP_MICROSEC : 1;
                    U32 nbLoops = 0;
                    ZSTD_CDict* const cdict = ZSTD_createCDict_advanced(dictBuffer, dictBufferSize, 1, zparams, cmem);
                    if (cdict==NULL) EXM_THROW(1, "ZSTD_createCDict_advanced() allocation failure");
                    if (comprParams->windowLog) zparams.cParams.windowLog = comprParams->windowLog;
                    if (comprParams->chainLog) zparams.cParams.chainLog = comprParams->chainLog;
                    if (comprParams->hashLog) zparams.cParams.hashLog = comprParams->hashLog;
                    if (comprParams->searchLog) zparams.cParams.searchLog = comprParams->searchLog;
                    if (comprParams->searchLength) zparams.cParams.searchLength = comprParams->searchLength;
                    if (comprParams->targetLength) zparams.cParams.targetLength = comprParams->targetLength;
                    if (comprParams->strategy) zparams.cParams.strategy = (ZSTD_strategy)(comprParams->strategy - 1);
                    do {
                        U32 blockNb;
                        size_t rSize;
                        for (blockNb=0; blockNb<nbBlocks; blockNb++) {
                            if (dictBufferSize) {
                                rSize = ZSTD_compress_usingCDict(ctx,
                                                blockTable[blockNb].cPtr,  blockTable[blockNb].cRoom,
                                                blockTable[blockNb].srcPtr,blockTable[blockNb].srcSize,
                                                cdict);
                            } else {
#ifdef ZSTD_MULTITHREAD         /* note : limitation : MT single-pass does not support compression with dictionary */
                                rSize = ZSTDMT_compressCCtx(mtctx,
                                                blockTable[blockNb].cPtr,  blockTable[blockNb].cRoom,
                                                blockTable[blockNb].srcPtr,blockTable[blockNb].srcSize,
                                                cLevel);
#else
                                rSize = ZSTD_compress_advanced (ctx,
                                                blockTable[blockNb].cPtr,  blockTable[blockNb].cRoom,
                                                blockTable[blockNb].srcPtr,blockTable[blockNb].srcSize, NULL, 0, zparams);
#endif
                            }
                            if (ZSTD_isError(rSize)) EXM_THROW(1, "ZSTD_compress_usingCDict() failed : %s", ZSTD_getErrorName(rSize));
                            blockTable[blockNb].cSize = rSize;
                        }
                        nbLoops++;
                    } while (UTIL_clockSpanMicro(clockStart, clockTick) < clockLoop);
                    ZSTD_freeCDict(cdict);
                    {   U64 const clockSpanMicro = UTIL_clockSpanMicro(clockStart, clockTick);
                        if (clockSpanMicro < fastestC*nbLoops) fastestC = clockSpanMicro / nbLoops;
                        totalCTime += clockSpanMicro;
                        cCompleted = (totalCTime >= maxTime);
                }   }

                cSize = 0;
                { U32 blockNb; for (blockNb=0; blockNb<nbBlocks; blockNb++) cSize += blockTable[blockNb].cSize; }
                ratio = (double)srcSize / (double)cSize;
                markNb = (markNb+1) % NB_MARKS;
                DISPLAYLEVEL(2, "%2s-%-17.17s :%10u ->%10u (%5.3f),%6.1f MB/s\r",
                        marks[markNb], displayName, (U32)srcSize, (U32)cSize, ratio,
                        (double)srcSize / fastestC );
            } else {   /* g_decodeOnly */
                memcpy(compressedBuffer, srcBuffer, loadedCompressedSize);
            }

#if 0       /* disable decompression test */
            dCompleted=1;
            (void)totalDTime; (void)fastestD; (void)crcOrig;   /*  unused when decompression disabled */
#else
            /* Decompression */
            if (!dCompleted) memset(resultBuffer, 0xD6, srcSize);  /* warm result buffer */

            UTIL_sleepMilli(1); /* give processor time to other processes */
            UTIL_waitForNextTick(ticksPerSecond);

            if (!dCompleted) {
                U64 clockLoop = g_nbSeconds ? TIMELOOP_MICROSEC : 1;
                U32 nbLoops = 0;
                UTIL_time_t clockStart, clockTick;
                ZSTD_DDict* const ddict = ZSTD_createDDict(dictBuffer, dictBufferSize);
                if (!ddict) EXM_THROW(2, "ZSTD_createDDict() allocation failure");
                UTIL_initTimer(&clockTick);
                UTIL_getTime(&clockStart);
                do {
                    U32 blockNb;
                    for (blockNb=0; blockNb<nbBlocks; blockNb++) {
                        size_t const regenSize = ZSTD_decompress_usingDDict(dctx,
                            blockTable[blockNb].resPtr, blockTable[blockNb].resSize,
                            blockTable[blockNb].cPtr, blockTable[blockNb].cSize,
                            ddict);
                        if (ZSTD_isError(regenSize)) {
                            DISPLAY("ZSTD_decompress_usingDDict() failed on block %u of size %u : %s  \n",
                                      blockNb, (U32)blockTable[blockNb].cSize, ZSTD_getErrorName(regenSize));
                            clockLoop = 0;   /* force immediate test end */
                            break;
                        }
                        blockTable[blockNb].resSize = regenSize;
                    }
                    nbLoops++;
                } while (UTIL_clockSpanMicro(clockStart, clockTick) < clockLoop);
                ZSTD_freeDDict(ddict);
                {   U64 const clockSpanMicro = UTIL_clockSpanMicro(clockStart, clockTick);
                    if (clockSpanMicro < fastestD*nbLoops) fastestD = clockSpanMicro / nbLoops;
                    totalDTime += clockSpanMicro;
                    dCompleted = (totalDTime >= maxTime);
            }   }

            markNb = (markNb+1) % NB_MARKS;
            DISPLAYLEVEL(2, "%2s-%-17.17s :%10u ->%10u (%5.3f),%6.1f MB/s ,%6.1f MB/s\r",
                    marks[markNb], displayName, (U32)srcSize, (U32)cSize, ratio,
                    (double)srcSize / fastestC,
                    (double)srcSize / fastestD );

            /* CRC Checking */
            {   U64 const crcCheck = XXH64(resultBuffer, srcSize, 0);
                if (!g_decodeOnly && (crcOrig!=crcCheck)) {
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
                            if (u>5) {
                                int n;
                                for (n=-5; n<0; n++) DISPLAY("%02X ", ((const BYTE*)srcBuffer)[u+n]);
                                DISPLAY(" :%02X:  ", ((const BYTE*)srcBuffer)[u]);
                                for (n=1; n<3; n++) DISPLAY("%02X ", ((const BYTE*)srcBuffer)[u+n]);
                                DISPLAY(" \n");
                                for (n=-5; n<0; n++) DISPLAY("%02X ", ((const BYTE*)resultBuffer)[u+n]);
                                DISPLAY(" :%02X:  ", ((const BYTE*)resultBuffer)[u]);
                                for (n=1; n<3; n++) DISPLAY("%02X ", ((const BYTE*)resultBuffer)[u+n]);
                                DISPLAY(" \n");
                            }
                            break;
                        }
                        if (u==srcSize-1) {  /* should never happen */
                            DISPLAY("no difference detected\n");
                    }   }
                    break;
            }   }   /* CRC Checking */
#endif
        }   /* for (testNb = 1; testNb <= (g_nbSeconds + !g_nbSeconds); testNb++) */

        if (g_displayLevel == 1) {
            double cSpeed = (double)srcSize / fastestC;
            double dSpeed = (double)srcSize / fastestD;
            if (g_additionalParam)
                DISPLAY("-%-3i%11i (%5.3f) %6.2f MB/s %6.1f MB/s  %s (param=%d)\n", cLevel, (int)cSize, ratio, cSpeed, dSpeed, displayName, g_additionalParam);
            else
                DISPLAY("-%-3i%11i (%5.3f) %6.2f MB/s %6.1f MB/s  %s\n", cLevel, (int)cSize, ratio, cSpeed, dSpeed, displayName);
        }
        DISPLAYLEVEL(2, "%2i#\n", cLevel);
    }   /* Bench */

    /* clean up */
    free(blockTable);
    free(compressedBuffer);
    free(resultBuffer);
    ZSTDMT_freeCCtx(mtctx);
    ZSTD_freeCCtx(ctx);
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
                            const void* dictBuffer, size_t dictBufferSize,
                            ZSTD_compressionParameters *compressionParams)
{
    int l;

    const char* pch = strrchr(displayName, '\\'); /* Windows */
    if (!pch) pch = strrchr(displayName, '/'); /* Linux */
    if (pch) displayName = pch+1;

    SET_HIGH_PRIORITY;

    if (g_displayLevel == 1 && !g_additionalParam)
        DISPLAY("bench %s %s: input %u bytes, %u seconds, %u KB blocks\n", ZSTD_VERSION_STRING, ZSTD_GIT_COMMIT_STRING, (U32)benchedSize, g_nbSeconds, (U32)(g_blockSize>>10));

    if (cLevelLast < cLevel) cLevelLast = cLevel;

    for (l=cLevel; l <= cLevelLast; l++) {
        BMK_benchMem(srcBuffer, benchedSize,
                     displayName, l,
                     fileSizes, nbFiles,
                     dictBuffer, dictBufferSize, compressionParams);
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
        DISPLAYUPDATE(2, "Loading %s...       \r", fileNamesTable[n]);
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

static void BMK_benchFileTable(const char** fileNamesTable, unsigned nbFiles, const char* dictFileName,
                               int cLevel, int cLevelLast, ZSTD_compressionParameters *compressionParams)
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
                        dictBuffer, dictBufferSize, compressionParams);
    }

    /* clean up */
    free(srcBuffer);
    free(dictBuffer);
    free(fileSizes);
}


static void BMK_syntheticTest(int cLevel, int cLevelLast, double compressibility, ZSTD_compressionParameters* compressionParams)
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
    BMK_benchCLevel(srcBuffer, benchedSize, name, cLevel, cLevelLast, &benchedSize, 1, NULL, 0, compressionParams);

    /* clean up */
    free(srcBuffer);
}


int BMK_benchFiles(const char** fileNamesTable, unsigned nbFiles, const char* dictFileName,
                   int cLevel, int cLevelLast, ZSTD_compressionParameters* compressionParams)
{
    double const compressibility = (double)g_compressibilityDefault / 100;

    if (cLevel > ZSTD_maxCLevel()) cLevel = ZSTD_maxCLevel();
    if (cLevelLast > ZSTD_maxCLevel()) cLevelLast = ZSTD_maxCLevel();
    if (cLevelLast < cLevel) cLevelLast = cLevel;
    if (cLevelLast > cLevel) DISPLAYLEVEL(2, "Benchmarking levels from %d to %d\n", cLevel, cLevelLast);

    if (nbFiles == 0)
        BMK_syntheticTest(cLevel, cLevelLast, compressibility, compressionParams);
    else
        BMK_benchFileTable(fileNamesTable, nbFiles, dictFileName, cLevel, cLevelLast, compressionParams);
    return 0;
}
