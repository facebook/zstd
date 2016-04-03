/*
    Fuzzer test tool for zstd_buffered
    Copyright (C) Yann Collet 2015-2016

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
    - ZSTD homepage : https://www.zstd.net/
*/

/*-************************************
*  Compiler specific
**************************************/
#ifdef _MSC_VER    /* Visual Studio */
#  define _CRT_SECURE_NO_WARNINGS     /* fgets */
#  pragma warning(disable : 4127)     /* disable: C4127: conditional expression is constant */
#  pragma warning(disable : 4146)     /* disable: C4146: minus unsigned expression */
#endif


/*-************************************
*  Includes
**************************************/
#include <stdlib.h>      /* free */
#include <stdio.h>       /* fgets, sscanf */
#include <sys/timeb.h>   /* timeb */
#include <string.h>      /* strcmp */
#include "mem.h"
#include "zbuff.h"
#include "zstd.h"        /* ZSTD_compressBound() */
#include "datagen.h"     /* RDG_genBuffer */
#include "xxhash.h"      /* XXH64 */


/*-************************************
*  Constants
**************************************/
#ifndef ZSTD_VERSION
#  define ZSTD_VERSION ""
#endif

#define KB *(1U<<10)
#define MB *(1U<<20)
#define GB *(1U<<30)

static const U32 nbTestsDefault = 10000;
#define COMPRESSIBLE_NOISE_LENGTH (10 MB)
#define FUZ_COMPRESSIBILITY_DEFAULT 50
static const U32 prime1 = 2654435761U;
static const U32 prime2 = 2246822519U;



/*-************************************
*  Display Macros
**************************************/
#define DISPLAY(...)          fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...)  if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); }
static U32 g_displayLevel = 2;

#define DISPLAYUPDATE(l, ...) if (g_displayLevel>=l) { \
            if ((FUZ_GetMilliSpan(g_displayTime) > g_refreshRate) || (g_displayLevel>=4)) \
            { g_displayTime = FUZ_GetMilliStart(); DISPLAY(__VA_ARGS__); \
            if (g_displayLevel>=4) fflush(stdout); } }
static const U32 g_refreshRate = 150;
static U32 g_displayTime = 0;

static U32 g_testTime = 0;


/*-*******************************************************
*  Fuzzer functions
*********************************************************/
#define MAX(a,b) ((a)>(b)?(a):(b))

static U32 FUZ_GetMilliStart(void)
{
    struct timeb tb;
    U32 nCount;
    ftime( &tb );
    nCount = (U32) (((tb.time & 0xFFFFF) * 1000) +  tb.millitm);
    return nCount;
}


static U32 FUZ_GetMilliSpan(U32 nTimeStart)
{
    U32 nCurrent = FUZ_GetMilliStart();
    U32 nSpan = nCurrent - nTimeStart;
    if (nTimeStart > nCurrent)
        nSpan += 0x100000 * 1000;
    return nSpan;
}


#  define FUZ_rotl32(x,r) ((x << r) | (x >> (32 - r)))
unsigned int FUZ_rand(unsigned int* src)
{
    U32 rand32 = *src;
    rand32 *= prime1;
    rand32 += prime2;
    rand32  = FUZ_rotl32(rand32, 13);
    *src = rand32;
    return rand32 >> 5;
}


/*
static unsigned FUZ_highbit32(U32 v32)
{
    unsigned nbBits = 0;
    if (v32==0) return 0;
    for ( ; v32 ; v32>>=1) nbBits++;
    return nbBits;
}
*/

static int basicUnitTests(U32 seed, double compressibility)
{
    int testResult = 0;
    void* CNBuffer;
    size_t CNBufferSize = COMPRESSIBLE_NOISE_LENGTH;
    void* compressedBuffer;
    size_t compressedBufferSize = ZSTD_compressBound(COMPRESSIBLE_NOISE_LENGTH);
    void* decodedBuffer;
    size_t decodedBufferSize = CNBufferSize;
    U32 randState = seed;
    size_t result, cSize, readSize, genSize;
    U32 testNb=0;
    ZBUFF_CCtx* zc = ZBUFF_createCCtx();
    ZBUFF_DCtx* zd = ZBUFF_createDCtx();

    /* Create compressible test buffer */
    CNBuffer = malloc(CNBufferSize);
    compressedBuffer = malloc(compressedBufferSize);
    decodedBuffer = malloc(decodedBufferSize);
    if (!CNBuffer || !compressedBuffer || !decodedBuffer || !zc || !zd) {
        DISPLAY("Not enough memory, aborting\n");
        goto _output_error;
    }
    RDG_genBuffer(CNBuffer, CNBufferSize, compressibility, 0., randState);

    /* Basic compression test */
    DISPLAYLEVEL(4, "test%3i : compress %u bytes : ", testNb++, COMPRESSIBLE_NOISE_LENGTH);
    ZBUFF_compressInitDictionary(zc, CNBuffer, 128 KB, 1);
    readSize = CNBufferSize;
    genSize = compressedBufferSize;
    result = ZBUFF_compressContinue(zc, compressedBuffer, &genSize, CNBuffer, &readSize);
    if (ZBUFF_isError(result)) goto _output_error;
    if (readSize != CNBufferSize) goto _output_error;   /* entire input should be consumed */
    cSize = genSize;
    genSize = compressedBufferSize - cSize;
    result = ZBUFF_compressEnd(zc, ((char*)compressedBuffer)+cSize, &genSize);
    if (result != 0) goto _output_error;   /* error, or some data not flushed */
    cSize += genSize;
    DISPLAYLEVEL(4, "OK (%u bytes : %.2f%%)\n", (U32)cSize, (double)cSize/COMPRESSIBLE_NOISE_LENGTH*100);

    /* Basic decompression test */
    DISPLAYLEVEL(4, "test%3i : decompress %u bytes : ", testNb++, COMPRESSIBLE_NOISE_LENGTH);
    ZBUFF_decompressInitDictionary(zd, CNBuffer, 128 KB);
    readSize = cSize;
    genSize = CNBufferSize;
    result = ZBUFF_decompressContinue(zd, decodedBuffer, &genSize, compressedBuffer, &readSize);
    if (result != 0) goto _output_error;  /* should reach end of frame == 0; otherwise, some data left, or an error */
    if (genSize != CNBufferSize) goto _output_error;   /* should regenerate the same amount */
    if (readSize != cSize) goto _output_error;   /* should have read the entire frame */
    DISPLAYLEVEL(4, "OK \n");

    /* check regenerated data is byte exact */
    {   size_t i;
        DISPLAYLEVEL(4, "test%3i : check decompressed result : ", testNb++);
        for (i=0; i<CNBufferSize; i++) {
            if (((BYTE*)decodedBuffer)[i] != ((BYTE*)CNBuffer)[i]) goto _output_error;;
        }
        DISPLAYLEVEL(4, "OK \n");
    }

_end:
    ZBUFF_freeCCtx(zc);
    ZBUFF_freeDCtx(zd);
    free(CNBuffer);
    free(compressedBuffer);
    free(decodedBuffer);
    return testResult;

_output_error:
    testResult = 1;
    DISPLAY("Error detected in Unit tests ! \n");
    goto _end;
}


static size_t findDiff(const void* buf1, const void* buf2, size_t max)
{
    const BYTE* b1 = (const BYTE*)buf1;
    const BYTE* b2 = (const BYTE*)buf2;
    size_t i;
    for (i=0; i<max; i++) {
        if (b1[i] != b2[i]) break;
    }
    return i;
}

#define MIN(a,b)   ( (a) < (b) ? (a) : (b) )

#define CHECK(cond, ...) if (cond) { DISPLAY("Error => "); DISPLAY(__VA_ARGS__); \
                         DISPLAY(" (seed %u, test nb %u)  \n", seed, testNb); goto _output_error; }

static int fuzzerTests(U32 seed, U32 nbTests, unsigned startTest, double compressibility)
{
    static const U32 maxSrcLog = 24;
    static const U32 maxSampleLog = 19;
    BYTE* cNoiseBuffer[5];
    BYTE* srcBuffer;
    size_t srcBufferSize = (size_t)1<<maxSrcLog;
    BYTE* copyBuffer;
    size_t copyBufferSize = srcBufferSize + (1<<maxSampleLog);
    BYTE* cBuffer;
    size_t cBufferSize   = ZSTD_compressBound(srcBufferSize);
    BYTE* dstBuffer;
    size_t dstBufferSize = srcBufferSize;
    U32 result = 0;
    U32 testNb = 0;
    U32 coreSeed = seed;
    ZBUFF_CCtx* zc;
    ZBUFF_DCtx* zd;
    U32 startTime = FUZ_GetMilliStart();

    /* allocations */
    zc = ZBUFF_createCCtx();
    zd = ZBUFF_createDCtx();
    cNoiseBuffer[0] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[1] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[2] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[3] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[4] = (BYTE*)malloc (srcBufferSize);
    copyBuffer= (BYTE*)malloc (copyBufferSize);
    dstBuffer = (BYTE*)malloc (dstBufferSize);
    cBuffer   = (BYTE*)malloc (cBufferSize);
    CHECK (!cNoiseBuffer[0] || !cNoiseBuffer[1] || !cNoiseBuffer[2] || !cNoiseBuffer[3] || !cNoiseBuffer[4] ||
           !copyBuffer || !dstBuffer || !cBuffer || !zc || !zd,
           "Not enough memory, fuzzer tests cancelled");

    /* Create initial samples */
    RDG_genBuffer(cNoiseBuffer[0], srcBufferSize, 0.00, 0., coreSeed);    /* pure noise */
    RDG_genBuffer(cNoiseBuffer[1], srcBufferSize, 0.05, 0., coreSeed);    /* barely compressible */
    RDG_genBuffer(cNoiseBuffer[2], srcBufferSize, compressibility, 0., coreSeed);
    RDG_genBuffer(cNoiseBuffer[3], srcBufferSize, 0.95, 0., coreSeed);    /* highly compressible */
    RDG_genBuffer(cNoiseBuffer[4], srcBufferSize, 1.00, 0., coreSeed);    /* sparse content */
    srcBuffer = cNoiseBuffer[2];
    memset(copyBuffer, 0x65, copyBufferSize);
    memcpy(copyBuffer, srcBuffer, MIN(copyBufferSize,srcBufferSize));   /* make copyBuffer considered initialized */

    /* catch up testNb */
    for (testNb=1; testNb < startTest; testNb++)
        FUZ_rand(&coreSeed);

    /* test loop */
    for ( ; (testNb <= nbTests) || (FUZ_GetMilliSpan(startTime) < g_testTime); testNb++ ) {
        U32 lseed;
        size_t sampleSize, sampleStart;
        const BYTE* dict;
        size_t cSize, dictSize;
        size_t maxTestSize, totalTestSize, readSize, totalCSize, genSize, totalGenSize;
        size_t errorCode;
        U32 sampleSizeLog, buffNb, n, nbChunks;
        XXH64_CREATESTATE_STATIC(xxh64);
        U64 crcOrig;

        /* init */
        DISPLAYUPDATE(2, "\r%6u", testNb);
        if (nbTests >= testNb) DISPLAYUPDATE(2, "/%6u   ", nbTests);
        FUZ_rand(&coreSeed);
        lseed = coreSeed ^ prime1;

        /* random state complete reset */
        /* some problems only happen when states are re-used in a specific order */
        if ((FUZ_rand(&lseed) & 0xFF) == 131) { ZBUFF_freeCCtx(zc); zc = ZBUFF_createCCtx(); }
        if ((FUZ_rand(&lseed) & 0xFF) == 132) { ZBUFF_freeDCtx(zd); zd = ZBUFF_createDCtx(); }

        /* srcBuffer selection */
        buffNb = FUZ_rand(&lseed) & 0x7F;
        if (buffNb & 7) buffNb=2;
        else {
            buffNb >>= 3;
            if (buffNb & 7) {
                const U32 tnb[2] = { 1, 3 };
                buffNb = tnb[buffNb >> 3];
            } else {
                const U32 tnb[2] = { 0, 4 };
                buffNb = tnb[buffNb >> 3];
        }   }
        srcBuffer = cNoiseBuffer[buffNb];

        /* Multi - segments compression test */
        XXH64_reset(xxh64, 0);
        nbChunks = (FUZ_rand(&lseed) & 127) + 2;
        sampleSizeLog = FUZ_rand(&lseed) % maxSrcLog;
        maxTestSize = (size_t)1 << sampleSizeLog;
        maxTestSize += FUZ_rand(&lseed) & (maxTestSize-1);

        sampleSizeLog = FUZ_rand(&lseed) % maxSampleLog;
        sampleSize = (size_t)1 << sampleSizeLog;
        sampleSize += FUZ_rand(&lseed) & (sampleSize-1);
        sampleStart = FUZ_rand(&lseed) % (srcBufferSize - sampleSize);
        dict = srcBuffer + sampleStart;
        dictSize = sampleSize;
        ZBUFF_compressInitDictionary(zc, dict, dictSize, (FUZ_rand(&lseed) % (20 - (sampleSizeLog/3))) + 1);

        totalTestSize = 0;
        cSize = 0;
        for (n=0; n<nbChunks; n++) {
            sampleSizeLog = FUZ_rand(&lseed) % maxSampleLog;
            sampleSize = (size_t)1 << sampleSizeLog;
            sampleSize += FUZ_rand(&lseed) & (sampleSize-1);
            sampleStart = FUZ_rand(&lseed) % (srcBufferSize - sampleSize);
            readSize = sampleSize;

            /* random size output buffer */
            sampleSizeLog = FUZ_rand(&lseed) % maxSampleLog;
            sampleSize = (size_t)1 << sampleSizeLog;
            sampleSize += FUZ_rand(&lseed) & (sampleSize-1);
            genSize = MIN (cBufferSize - cSize, sampleSize);

            errorCode = ZBUFF_compressContinue(zc, cBuffer+cSize, &genSize, srcBuffer+sampleStart, &readSize);
            CHECK (ZBUFF_isError(errorCode), "compression error : %s", ZBUFF_getErrorName(errorCode));

            XXH64_update(xxh64, srcBuffer+sampleStart, readSize);
            memcpy(copyBuffer+totalTestSize, srcBuffer+sampleStart, readSize);
            cSize += genSize;
            totalTestSize += readSize;

            /* random flush operation, to mess around */
            if ((FUZ_rand(&lseed) & 15) == 0) {
                sampleSizeLog = FUZ_rand(&lseed) % maxSampleLog;
                sampleSize = (size_t)1 << sampleSizeLog;
                sampleSize += FUZ_rand(&lseed) & (sampleSize-1);
                genSize = MIN (cBufferSize - cSize, sampleSize);
                errorCode = ZBUFF_compressFlush(zc, cBuffer+cSize, &genSize);
                CHECK (ZBUFF_isError(errorCode), "flush error : %s", ZBUFF_getErrorName(errorCode));
                cSize += genSize;
            }

            if (totalTestSize > maxTestSize) break;
        }
        genSize = cBufferSize - cSize;
        errorCode = ZBUFF_compressEnd(zc, cBuffer+cSize, &genSize);
        CHECK (ZBUFF_isError(errorCode), "compression error : %s", ZBUFF_getErrorName(errorCode));
        CHECK (errorCode != 0, "frame epilogue not fully consumed");
        cSize += genSize;
        crcOrig = XXH64_digest(xxh64);

        /* multi - fragments decompression test */
        ZBUFF_decompressInitDictionary(zd, dict, dictSize);
        totalCSize = 0;
        totalGenSize = 0;
        while (totalCSize < cSize) {
            sampleSizeLog  = FUZ_rand(&lseed) % maxSampleLog;
            sampleSize  = (size_t)1 << sampleSizeLog;
            sampleSize += FUZ_rand(&lseed) & (sampleSize-1);
            readSize = sampleSize;
            sampleSizeLog  = FUZ_rand(&lseed) % maxSampleLog;
            sampleSize  = (size_t)1 << sampleSizeLog;
            sampleSize += FUZ_rand(&lseed) & (sampleSize-1);
            genSize = MIN(sampleSize, dstBufferSize - totalGenSize);
            errorCode = ZBUFF_decompressContinue(zd, dstBuffer+totalGenSize, &genSize, cBuffer+totalCSize, &readSize);
            CHECK (ZBUFF_isError(errorCode), "decompression error : %s", ZBUFF_getErrorName(errorCode));
            totalGenSize += genSize;
            totalCSize += readSize;
        }
        CHECK (errorCode != 0, "frame not fully decoded");
        CHECK (totalGenSize != totalTestSize, "decompressed data : wrong size")
        CHECK (totalCSize != cSize, "compressed data should be fully read")
        { U64 const crcDest = XXH64(dstBuffer, totalTestSize, 0);
          if (crcDest!=crcOrig) findDiff(copyBuffer, dstBuffer, totalTestSize);
          CHECK (crcDest!=crcOrig, "decompressed data corrupted"); }

        /*=====   noisy/erroneous src decompression test   =====*/

        /* add some noise */
        {   U32 const nbNoiseChunks = (FUZ_rand(&lseed) & 7) + 2;
            U32 nn; for (nn=0; nn<nbNoiseChunks; nn++) {
                U32 const noiseLog = FUZ_rand(&lseed) % maxSampleLog;
                size_t const noiseMask = ((size_t)1 << noiseLog) - 1;
                size_t const noiseSize = MIN(cSize/3, noiseMask+1 + (FUZ_rand(&lseed) & noiseMask));
                size_t const noiseStart = FUZ_rand(&lseed) % (srcBufferSize - noiseSize);
                size_t const cStart = FUZ_rand(&lseed) % (cSize - noiseSize);
                memcpy(cBuffer+cStart, srcBuffer+noiseStart, noiseSize);
        }   }

        /* try decompression on noisy data */
        ZBUFF_decompressInit(zd);
        totalCSize = 0;
        totalGenSize = 0;
        while ( (totalCSize < cSize) && (totalGenSize < dstBufferSize) ) {
            sampleSizeLog  = FUZ_rand(&lseed) % maxSampleLog;
            sampleSize  = (size_t)1 << sampleSizeLog;
            sampleSize += FUZ_rand(&lseed) & (sampleSize-1);
            readSize = sampleSize;
            sampleSizeLog  = FUZ_rand(&lseed) % maxSampleLog;
            sampleSize  = (size_t)1 << sampleSizeLog;
            sampleSize += FUZ_rand(&lseed) & (sampleSize-1);
            genSize = MIN(sampleSize, dstBufferSize - totalGenSize);
            errorCode = ZBUFF_decompressContinue(zd, dstBuffer+totalGenSize, &genSize, cBuffer+totalCSize, &readSize);
            if (ZBUFF_isError(errorCode)) break;   /* error correctly detected */
            totalGenSize += genSize;
            totalCSize += readSize;
    }   }
    DISPLAY("\r%u fuzzer tests completed   \n", testNb);

_cleanup:
    ZBUFF_freeCCtx(zc);
    ZBUFF_freeDCtx(zd);
    free(cNoiseBuffer[0]);
    free(cNoiseBuffer[1]);
    free(cNoiseBuffer[2]);
    free(cNoiseBuffer[3]);
    free(cNoiseBuffer[4]);
    free(copyBuffer);
    free(cBuffer);
    free(dstBuffer);
    return result;

_output_error:
    result = 1;
    goto _cleanup;
}


/*-*******************************************************
*  Command line
*********************************************************/
int FUZ_usage(char* programName)
{
    DISPLAY( "Usage :\n");
    DISPLAY( "      %s [args]\n", programName);
    DISPLAY( "\n");
    DISPLAY( "Arguments :\n");
    DISPLAY( " -i#    : Nb of tests (default:%u) \n", nbTestsDefault);
    DISPLAY( " -s#    : Select seed (default:prompt user)\n");
    DISPLAY( " -t#    : Select starting test number (default:0)\n");
    DISPLAY( " -P#    : Select compressibility in %% (default:%i%%)\n", FUZ_COMPRESSIBILITY_DEFAULT);
    DISPLAY( " -v     : verbose\n");
    DISPLAY( " -p     : pause at the end\n");
    DISPLAY( " -h     : display help and exit\n");
    return 0;
}


int main(int argc, char** argv)
{
    U32 seed=0;
    int seedset=0;
    int argNb;
    int nbTests = nbTestsDefault;
    int testNb = 0;
    int proba = FUZ_COMPRESSIBILITY_DEFAULT;
    int result=0;
    U32 mainPause = 0;
    char* programName;

    /* Check command line */
    programName = argv[0];
    for(argNb=1; argNb<argc; argNb++) {
        char* argument = argv[argNb];

        if(!argument) continue;   /* Protection if argument empty */

        /* Handle commands. Aggregated commands are allowed */
        if (argument[0]=='-') {
            argument++;

            while (*argument!=0) {
                switch(*argument)
                {
                case 'h':
                    return FUZ_usage(programName);
                case 'v':
                    argument++;
                    g_displayLevel=4;
                    break;
                case 'q':
                    argument++;
                    g_displayLevel--;
                    break;
                case 'p': /* pause at the end */
                    argument++;
                    mainPause = 1;
                    break;

                case 'i':
                    argument++;
                    nbTests=0; g_testTime=0;
                    while ((*argument>='0') && (*argument<='9')) {
                        nbTests *= 10;
                        nbTests += *argument - '0';
                        argument++;
                    }
                    break;

                case 'T':
                    argument++;
                    nbTests=0; g_testTime=0;
                    while ((*argument>='0') && (*argument<='9')) {
                        g_testTime *= 10;
                        g_testTime += *argument - '0';
                        argument++;
                    }
                    if (*argument=='m') g_testTime *=60, argument++;
                    if (*argument=='n') argument++;
                    g_testTime *= 1000;
                    break;

                case 's':
                    argument++;
                    seed=0;
                    seedset=1;
                    while ((*argument>='0') && (*argument<='9')) {
                        seed *= 10;
                        seed += *argument - '0';
                        argument++;
                    }
                    break;

                case 't':
                    argument++;
                    testNb=0;
                    while ((*argument>='0') && (*argument<='9')) {
                        testNb *= 10;
                        testNb += *argument - '0';
                        argument++;
                    }
                    break;

                case 'P':   /* compressibility % */
                    argument++;
                    proba=0;
                    while ((*argument>='0') && (*argument<='9')) {
                        proba *= 10;
                        proba += *argument - '0';
                        argument++;
                    }
                    if (proba<0) proba=0;
                    if (proba>100) proba=100;
                    break;

                default:
                    return FUZ_usage(programName);
                }
    }   }   }   /* for(argNb=1; argNb<argc; argNb++) */

    /* Get Seed */
    DISPLAY("Starting zstd_buffered tester (%i-bits, %s)\n", (int)(sizeof(size_t)*8), ZSTD_VERSION);

    if (!seedset) seed = FUZ_GetMilliStart() % 10000;
    DISPLAY("Seed = %u\n", seed);
    if (proba!=FUZ_COMPRESSIBILITY_DEFAULT) DISPLAY("Compressibility : %i%%\n", proba);

    if (nbTests<=0) nbTests=1;

    if (testNb==0) result = basicUnitTests(0, ((double)proba) / 100);  /* constant seed for predictability */
    if (!result)
        result = fuzzerTests(seed, nbTests, testNb, ((double)proba) / 100);

    if (mainPause) {
        int unused;
        DISPLAY("Press Enter \n");
        unused = getchar();
        (void)unused;
    }
    return result;
}
