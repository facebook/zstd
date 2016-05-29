/*
    Fuzzer test tool for zstd
    Copyright (C) Yann Collet 2014-2016

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
    - ZSTD homepage : http://www.zstd.net
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
#include <time.h>        /* clock_t */
#include "zstd_static.h" /* ZSTD_VERSION_STRING */
#include "datagen.h"     /* RDG_genBuffer */
#include "mem.h"
#define XXH_STATIC_LINKING_ONLY
#include "xxhash.h"      /* XXH64 */


/*-************************************
*  Constants
**************************************/
#define KB *(1U<<10)
#define MB *(1U<<20)
#define GB *(1U<<30)

static const size_t COMPRESSIBLE_NOISE_LENGTH = 10 MB;   /* capital, used to be a macro */
static const U32 FUZ_compressibility_default = 50;
static const U32 nbTestsDefault = 30000;


/*-************************************
*  Display Macros
**************************************/
#define DISPLAY(...)          fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...)  if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); }
static U32 g_displayLevel = 2;

#define DISPLAYUPDATE(l, ...) if (g_displayLevel>=l) { \
            if ((FUZ_clockSpan(g_displayClock) > g_refreshRate) || (g_displayLevel>=4)) \
            { g_displayClock = clock(); DISPLAY(__VA_ARGS__); \
            if (g_displayLevel>=4) fflush(stdout); } }
static const clock_t g_refreshRate = CLOCKS_PER_SEC * 150 / 1000;
static clock_t g_displayClock = 0;


/*-*******************************************************
*  Fuzzer functions
*********************************************************/
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

static clock_t FUZ_clockSpan(clock_t cStart)
{
    return clock() - cStart;   /* works even when overflow; max span ~ 30mn */
}


#  define FUZ_rotl32(x,r) ((x << r) | (x >> (32 - r)))
unsigned FUZ_rand(unsigned* src)
{
    static const U32 prime1 = 2654435761U;
    static const U32 prime2 = 2246822519U;
    U32 rand32 = *src;
    rand32 *= prime1;
    rand32 += prime2;
    rand32  = FUZ_rotl32(rand32, 13);
    *src = rand32;
    return rand32 >> 5;
}


static unsigned FUZ_highbit32(U32 v32)
{
    unsigned nbBits = 0;
    if (v32==0) return 0;
    while (v32) {
        v32 >>= 1;
        nbBits ++;
    }
    return nbBits;
}


static int basicUnitTests(U32 seed, double compressibility)
{
    int testResult = 0;
    void* const CNBuffer = malloc(COMPRESSIBLE_NOISE_LENGTH);
    void* const compressedBuffer = malloc(ZSTD_compressBound(COMPRESSIBLE_NOISE_LENGTH));
    void* const decodedBuffer = malloc(COMPRESSIBLE_NOISE_LENGTH);
    U32 randState = seed;
    size_t result, cSize;
    U32 testNb=0;

    /* Create compressible test buffer */
    if (!CNBuffer || !compressedBuffer || !decodedBuffer) {
        DISPLAY("Not enough memory, aborting\n");
        testResult = 1;
        goto _end;
    }
    RDG_genBuffer(CNBuffer, COMPRESSIBLE_NOISE_LENGTH, compressibility, 0., randState);

    /* Basic tests */
    DISPLAYLEVEL(4, "test%3i : compress %u bytes : ", testNb++, (U32)COMPRESSIBLE_NOISE_LENGTH);
    result = ZSTD_compress(compressedBuffer, ZSTD_compressBound(COMPRESSIBLE_NOISE_LENGTH), CNBuffer, COMPRESSIBLE_NOISE_LENGTH, 1);
    if (ZSTD_isError(result)) goto _output_error;
    cSize = result;
    DISPLAYLEVEL(4, "OK (%u bytes : %.2f%%)\n", (U32)cSize, (double)cSize/COMPRESSIBLE_NOISE_LENGTH*100);

    DISPLAYLEVEL(4, "test%3i : decompress %u bytes : ", testNb++, (U32)COMPRESSIBLE_NOISE_LENGTH);
    result = ZSTD_decompress(decodedBuffer, COMPRESSIBLE_NOISE_LENGTH, compressedBuffer, cSize);
    if (ZSTD_isError(result)) goto _output_error;
    if (result != COMPRESSIBLE_NOISE_LENGTH) goto _output_error;
    DISPLAYLEVEL(4, "OK \n");

    {   size_t i;
        DISPLAYLEVEL(4, "test%3i : check decompressed result : ", testNb++);
        for (i=0; i<COMPRESSIBLE_NOISE_LENGTH; i++) {
            if (((BYTE*)decodedBuffer)[i] != ((BYTE*)CNBuffer)[i]) goto _output_error;;
        }
        DISPLAYLEVEL(4, "OK \n");
    }

    DISPLAYLEVEL(4, "test%3i : decompress with 1 missing byte : ", testNb++);
    result = ZSTD_decompress(decodedBuffer, COMPRESSIBLE_NOISE_LENGTH, compressedBuffer, cSize-1);
    if (!ZSTD_isError(result)) goto _output_error;
    if (result != (size_t)-ZSTD_error_srcSize_wrong) goto _output_error;
    DISPLAYLEVEL(4, "OK \n");

    DISPLAYLEVEL(4, "test%3i : decompress with 1 too much byte : ", testNb++);
    result = ZSTD_decompress(decodedBuffer, COMPRESSIBLE_NOISE_LENGTH, compressedBuffer, cSize+1);
    if (!ZSTD_isError(result)) goto _output_error;
    if (result != (size_t)-ZSTD_error_srcSize_wrong) goto _output_error;
    DISPLAYLEVEL(4, "OK \n");

    /* Dictionary and CCtx Duplication tests */
    {   ZSTD_CCtx* ctxOrig = ZSTD_createCCtx();
        ZSTD_CCtx* ctxDuplicated = ZSTD_createCCtx();
        ZSTD_DCtx* dctx = ZSTD_createDCtx();
        size_t const dictSize = 500;

        DISPLAYLEVEL(4, "test%3i : copy context too soon : ", testNb++);
        { size_t const copyResult = ZSTD_copyCCtx(ctxDuplicated, ctxOrig);
          if (!ZSTD_isError(copyResult)) goto _output_error; }  /* error should be detected */
        DISPLAYLEVEL(4, "OK \n");

        DISPLAYLEVEL(4, "test%3i : load dictionary into context : ", testNb++);
        { size_t const initResult = ZSTD_compressBegin_usingDict(ctxOrig, CNBuffer, dictSize, 2);
          if (ZSTD_isError(initResult)) goto _output_error; }
        { size_t const copyResult = ZSTD_copyCCtx(ctxDuplicated, ctxOrig);
          if (ZSTD_isError(copyResult)) goto _output_error; }
        DISPLAYLEVEL(4, "OK \n");

        DISPLAYLEVEL(4, "test%3i : compress with dictionary : ", testNb++);
        cSize = 0;
        result = ZSTD_compressContinue(ctxOrig, compressedBuffer, ZSTD_compressBound(COMPRESSIBLE_NOISE_LENGTH), (const char*)CNBuffer + dictSize, COMPRESSIBLE_NOISE_LENGTH - dictSize);
        if (ZSTD_isError(result)) goto _output_error;
        cSize += result;
        result = ZSTD_compressEnd(ctxOrig, (char*)compressedBuffer+cSize, ZSTD_compressBound(COMPRESSIBLE_NOISE_LENGTH)-cSize);
        if (ZSTD_isError(result)) goto _output_error;
        cSize += result;
        DISPLAYLEVEL(4, "OK (%u bytes : %.2f%%)\n", (U32)cSize, (double)cSize/COMPRESSIBLE_NOISE_LENGTH*100);

        DISPLAYLEVEL(4, "test%3i : frame built with dictionary should be decompressible : ", testNb++);
        result = ZSTD_decompress_usingDict(dctx,
                                           decodedBuffer, COMPRESSIBLE_NOISE_LENGTH,
                                           compressedBuffer, cSize,
                                           CNBuffer, dictSize);
        if (ZSTD_isError(result)) goto _output_error;
        if (result != COMPRESSIBLE_NOISE_LENGTH - dictSize) goto _output_error;
        DISPLAYLEVEL(4, "OK \n");

        DISPLAYLEVEL(4, "test%3i : compress with duplicated context : ", testNb++);
        {   size_t const cSizeOrig = cSize;
            cSize = 0;
            result = ZSTD_compressContinue(ctxDuplicated, compressedBuffer, ZSTD_compressBound(COMPRESSIBLE_NOISE_LENGTH), (const char*)CNBuffer + dictSize, COMPRESSIBLE_NOISE_LENGTH - dictSize);
            if (ZSTD_isError(result)) goto _output_error;
            cSize += result;
            result = ZSTD_compressEnd(ctxDuplicated, (char*)compressedBuffer+cSize, ZSTD_compressBound(COMPRESSIBLE_NOISE_LENGTH)-cSize);
            if (ZSTD_isError(result)) goto _output_error;
            cSize += result;
            if (cSize != cSizeOrig) goto _output_error;   /* should be identical == have same size */
        }
        DISPLAYLEVEL(4, "OK (%u bytes : %.2f%%)\n", (U32)cSize, (double)cSize/COMPRESSIBLE_NOISE_LENGTH*100);

        DISPLAYLEVEL(4, "test%3i : frame built with duplicated context should be decompressible : ", testNb++);
        result = ZSTD_decompress_usingDict(dctx,
                                           decodedBuffer, COMPRESSIBLE_NOISE_LENGTH,
                                           compressedBuffer, cSize,
                                           CNBuffer, dictSize);
        if (ZSTD_isError(result)) goto _output_error;
        if (result != COMPRESSIBLE_NOISE_LENGTH - dictSize) goto _output_error;
        DISPLAYLEVEL(4, "OK \n");

        DISPLAYLEVEL(4, "test%3i : check content size on duplicated context : ", testNb++);
        {   size_t const testSize = COMPRESSIBLE_NOISE_LENGTH / 3;
            {   ZSTD_parameters const p = (ZSTD_parameters) { ZSTD_getCParams(2, testSize, dictSize), { 1, 0 } };
                size_t const initResult = ZSTD_compressBegin_advanced(ctxOrig, CNBuffer, dictSize, p, testSize-1);
                if (ZSTD_isError(initResult)) goto _output_error;
            }
            { size_t const copyResult = ZSTD_copyCCtx(ctxDuplicated, ctxOrig);
              if (ZSTD_isError(copyResult)) goto _output_error;  }
            cSize = ZSTD_compressContinue(ctxDuplicated, compressedBuffer, ZSTD_compressBound(testSize), (const char*)CNBuffer + dictSize, COMPRESSIBLE_NOISE_LENGTH - dictSize);
            if (ZSTD_isError(cSize)) goto _output_error;
            {   ZSTD_frameParams fp;
                if (ZSTD_getFrameParams(&fp, compressedBuffer, cSize)) goto _output_error;
                if ((fp.frameContentSize != testSize) && (fp.frameContentSize != 0)) goto _output_error;
        }   }
        DISPLAYLEVEL(4, "OK \n");

        ZSTD_freeCCtx(ctxOrig);
        ZSTD_freeCCtx(ctxDuplicated);
        ZSTD_freeDCtx(dctx);
    }

    /* Decompression defense tests */
    DISPLAYLEVEL(4, "test%3i : Check input length for magic number : ", testNb++);
    result = ZSTD_decompress(decodedBuffer, COMPRESSIBLE_NOISE_LENGTH, CNBuffer, 3);
    if (!ZSTD_isError(result)) goto _output_error;
    if (result != (size_t)-ZSTD_error_srcSize_wrong) goto _output_error;
    DISPLAYLEVEL(4, "OK \n");

    DISPLAYLEVEL(4, "test%3i : Check magic Number : ", testNb++);
    ((char*)(CNBuffer))[0] = 1;
    { size_t const r = ZSTD_decompress(decodedBuffer, COMPRESSIBLE_NOISE_LENGTH, CNBuffer, 4);
      if (!ZSTD_isError(r)) goto _output_error; }
    DISPLAYLEVEL(4, "OK \n");

    /* block API tests */
    {   ZSTD_CCtx* const cctx = ZSTD_createCCtx();
        ZSTD_DCtx* const dctx = ZSTD_createDCtx();
        static const size_t blockSize = 100 KB;
        static const size_t dictSize = 16 KB;

        /* basic block compression */
        DISPLAYLEVEL(4, "test%3i : Block compression test : ", testNb++);
        { size_t const r = ZSTD_compressBegin(cctx, 5);
          if (ZSTD_isError(r)) goto _output_error; }
        cSize = ZSTD_compressBlock(cctx, compressedBuffer, ZSTD_compressBound(blockSize), CNBuffer, blockSize);
        if (ZSTD_isError(cSize)) goto _output_error;
        DISPLAYLEVEL(4, "OK \n");

        DISPLAYLEVEL(4, "test%3i : Block decompression test : ", testNb++);
        result = ZSTD_decompressBegin(dctx);
        if (ZSTD_isError(result)) goto _output_error;
        result = ZSTD_decompressBlock(dctx, decodedBuffer, COMPRESSIBLE_NOISE_LENGTH, compressedBuffer, cSize);
        if (ZSTD_isError(result)) goto _output_error;
        if (result != blockSize) goto _output_error;
        DISPLAYLEVEL(4, "OK \n");

        /* dictionary block compression */
        DISPLAYLEVEL(4, "test%3i : Dictionary Block compression test : ", testNb++);
        result = ZSTD_compressBegin_usingDict(cctx, CNBuffer, dictSize, 5);
        if (ZSTD_isError(result)) goto _output_error;
        cSize = ZSTD_compressBlock(cctx, compressedBuffer, ZSTD_compressBound(blockSize), (char*)CNBuffer+dictSize, blockSize);
        if (ZSTD_isError(cSize)) goto _output_error;
        DISPLAYLEVEL(4, "OK \n");

        DISPLAYLEVEL(4, "test%3i : Dictionary Block decompression test : ", testNb++);
        result = ZSTD_decompressBegin_usingDict(dctx, CNBuffer, dictSize);
        if (ZSTD_isError(result)) goto _output_error;
        result = ZSTD_decompressBlock(dctx, decodedBuffer, COMPRESSIBLE_NOISE_LENGTH, compressedBuffer, cSize);
        if (ZSTD_isError(result)) goto _output_error;
        if (result != blockSize) goto _output_error;
        DISPLAYLEVEL(4, "OK \n");

        ZSTD_freeCCtx(cctx);
        ZSTD_freeDCtx(dctx);
    }

    /* long rle test */
    {   size_t sampleSize = 0;
        DISPLAYLEVEL(4, "test%3i : Long RLE test : ", testNb++);
        RDG_genBuffer(CNBuffer, sampleSize, compressibility, 0., randState);
        memset((char*)CNBuffer+sampleSize, 'B', 256 KB - 1);
        sampleSize += 256 KB - 1;
        RDG_genBuffer((char*)CNBuffer+sampleSize, 96 KB, compressibility, 0., randState);
        sampleSize += 96 KB;
        cSize = ZSTD_compress(compressedBuffer, ZSTD_compressBound(sampleSize), CNBuffer, sampleSize, 1);
        if (ZSTD_isError(cSize)) goto _output_error;
        result = ZSTD_decompress(decodedBuffer, sampleSize, compressedBuffer, cSize);
        if (ZSTD_isError(result)) goto _output_error;
        if (result!=sampleSize) goto _output_error;
        DISPLAYLEVEL(4, "OK \n");
    }

    /* All zeroes test (#137 verif) */
    #define ZEROESLENGTH 100
    DISPLAYLEVEL(4, "test%3i : compress %u zeroes : ", testNb++, ZEROESLENGTH);
    memset(CNBuffer, 0, ZEROESLENGTH);
    result = ZSTD_compress(compressedBuffer, ZSTD_compressBound(ZEROESLENGTH), CNBuffer, ZEROESLENGTH, 1);
    if (ZSTD_isError(result)) goto _output_error;
    cSize = result;
    DISPLAYLEVEL(4, "OK (%u bytes : %.2f%%)\n", (U32)cSize, (double)cSize/ZEROESLENGTH*100);

    DISPLAYLEVEL(4, "test%3i : decompress %u zeroes : ", testNb++, ZEROESLENGTH);
    result = ZSTD_decompress(decodedBuffer, ZEROESLENGTH, compressedBuffer, cSize);
    if (ZSTD_isError(result)) goto _output_error;
    if (result != ZEROESLENGTH) goto _output_error;
    DISPLAYLEVEL(4, "OK \n");

    /* nbSeq limit test */
    #define _3BYTESTESTLENGTH 131000
    #define NB3BYTESSEQLOG   9
    #define NB3BYTESSEQ     (1 << NB3BYTESSEQLOG)
    #define NB3BYTESSEQMASK (NB3BYTESSEQ-1)
    /* creates a buffer full of 3-bytes sequences */
    {   BYTE _3BytesSeqs[NB3BYTESSEQ][3];
        U32 rSeed = 1;

        /* create batch of 3-bytes sequences */
        { int i; for (i=0; i < NB3BYTESSEQ; i++) {
            _3BytesSeqs[i][0] = (BYTE)(FUZ_rand(&rSeed) & 255);
            _3BytesSeqs[i][1] = (BYTE)(FUZ_rand(&rSeed) & 255);
            _3BytesSeqs[i][2] = (BYTE)(FUZ_rand(&rSeed) & 255);
        }}

        /* randomly fills CNBuffer with prepared 3-bytes sequences */
        { int i; for (i=0; i < _3BYTESTESTLENGTH; ) {   /* note : CNBuffer size > _3BYTESTESTLENGTH+3 */
            U32 id = FUZ_rand(&rSeed) & NB3BYTESSEQMASK;
            ((BYTE*)CNBuffer)[i+0] = _3BytesSeqs[id][0];
            ((BYTE*)CNBuffer)[i+1] = _3BytesSeqs[id][1];
            ((BYTE*)CNBuffer)[i+2] = _3BytesSeqs[id][2];
            i += 3;
    }   }}
    DISPLAYLEVEL(4, "test%3i : compress lots 3-bytes sequences : ", testNb++);
    result = ZSTD_compress(compressedBuffer, ZSTD_compressBound(_3BYTESTESTLENGTH), CNBuffer, _3BYTESTESTLENGTH, 19);
    if (ZSTD_isError(result)) goto _output_error;
    cSize = result;
    DISPLAYLEVEL(4, "OK (%u bytes : %.2f%%)\n", (U32)cSize, (double)cSize/_3BYTESTESTLENGTH*100);

    DISPLAYLEVEL(4, "test%3i : decompress lots 3-bytes sequence : ", testNb++);
    result = ZSTD_decompress(decodedBuffer, _3BYTESTESTLENGTH, compressedBuffer, cSize);
    if (ZSTD_isError(result)) goto _output_error;
    if (result != _3BYTESTESTLENGTH) goto _output_error;
    DISPLAYLEVEL(4, "OK \n");

_end:
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


static size_t FUZ_rLogLength(U32* seed, U32 logLength)
{
    size_t const lengthMask = ((size_t)1 << logLength) - 1;
    return (lengthMask+1) + (FUZ_rand(seed) & lengthMask);
}

static size_t FUZ_randomLength(U32* seed, U32 maxLog)
{
    U32 const logLength = FUZ_rand(seed) % maxLog;
    return FUZ_rLogLength(seed, logLength);
}

#define CHECK(cond, ...) if (cond) { DISPLAY("Error => "); DISPLAY(__VA_ARGS__); \
                         DISPLAY(" (seed %u, test nb %u)  \n", seed, testNb); goto _output_error; }

static int fuzzerTests(U32 seed, U32 nbTests, unsigned startTest, U32 const maxDurationS, double compressibility)
{
    static const U32 maxSrcLog = 23;
    static const U32 maxSampleLog = 22;
    BYTE* cNoiseBuffer[5];
    BYTE* srcBuffer;
    BYTE* cBuffer;
    BYTE* dstBuffer;
    BYTE* mirrorBuffer;
    size_t srcBufferSize = (size_t)1<<maxSrcLog;
    size_t dstBufferSize = (size_t)1<<maxSampleLog;
    size_t cBufferSize   = ZSTD_compressBound(dstBufferSize);
    U32 result = 0;
    U32 testNb = 0;
    U32 coreSeed = seed, lseed = 0;
    ZSTD_CCtx* refCtx;
    ZSTD_CCtx* ctx;
    ZSTD_DCtx* dctx;
    clock_t startClock = clock();
    clock_t const maxClockSpan = maxDurationS * CLOCKS_PER_SEC;

    /* allocation */
    refCtx = ZSTD_createCCtx();
    ctx = ZSTD_createCCtx();
    dctx= ZSTD_createDCtx();
    cNoiseBuffer[0] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[1] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[2] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[3] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[4] = (BYTE*)malloc (srcBufferSize);
    dstBuffer = (BYTE*)malloc (dstBufferSize);
    mirrorBuffer = (BYTE*)malloc (dstBufferSize);
    cBuffer   = (BYTE*)malloc (cBufferSize);
    CHECK (!cNoiseBuffer[0] || !cNoiseBuffer[1] || !cNoiseBuffer[2] || !cNoiseBuffer[3] || !cNoiseBuffer[4]
           || !dstBuffer || !mirrorBuffer || !cBuffer || !refCtx || !ctx || !dctx,
           "Not enough memory, fuzzer tests cancelled");

    /* Create initial samples */
    RDG_genBuffer(cNoiseBuffer[0], srcBufferSize, 0.00, 0., coreSeed);    /* pure noise */
    RDG_genBuffer(cNoiseBuffer[1], srcBufferSize, 0.05, 0., coreSeed);    /* barely compressible */
    RDG_genBuffer(cNoiseBuffer[2], srcBufferSize, compressibility, 0., coreSeed);
    RDG_genBuffer(cNoiseBuffer[3], srcBufferSize, 0.95, 0., coreSeed);    /* highly compressible */
    RDG_genBuffer(cNoiseBuffer[4], srcBufferSize, 1.00, 0., coreSeed);    /* sparse content */
    srcBuffer = cNoiseBuffer[2];

    /* catch up testNb */
    for (testNb=1; testNb < startTest; testNb++) FUZ_rand(&coreSeed);

    /* main test loop */
    for ( ; (testNb <= nbTests) || (FUZ_clockSpan(startClock) < maxClockSpan); testNb++ ) {
        size_t sampleSize, sampleStart, maxTestSize, totalTestSize;
        size_t cSize, dSize, totalCSize, totalGenSize;
        U32 sampleSizeLog, nbChunks, n;
        XXH64_state_t xxhState;
        U64 crcOrig;
        BYTE* sampleBuffer;
        const BYTE* dict;
        size_t dictSize;

        /* notification */
        if (nbTests >= testNb) { DISPLAYUPDATE(2, "\r%6u/%6u    ", testNb, nbTests); }
        else { DISPLAYUPDATE(2, "\r%6u      ", testNb); }

        FUZ_rand(&coreSeed);
        { U32 const prime1 = 2654435761U; lseed = coreSeed ^ prime1; }

        /* srcBuffer selection [0-4] */
        {   U32 buffNb = FUZ_rand(&lseed) & 0x7F;
            if (buffNb & 7) buffNb=2;   /* most common : compressible (P) */
            else {
                buffNb >>= 3;
                if (buffNb & 7) {
                    const U32 tnb[2] = { 1, 3 };   /* barely/highly compressible */
                    buffNb = tnb[buffNb >> 3];
                } else {
                    const U32 tnb[2] = { 0, 4 };   /* not compressible / sparse */
                    buffNb = tnb[buffNb >> 3];
            }   }
            srcBuffer = cNoiseBuffer[buffNb];
        }

        /* select src segment */
        sampleSizeLog = FUZ_rand(&lseed) % maxSampleLog;
        sampleSize  = FUZ_rLogLength(&lseed, sampleSizeLog);
        sampleStart = FUZ_rand(&lseed) % (srcBufferSize - sampleSize);

        /* create sample buffer (to catch read error with valgrind & sanitizers)  */
        sampleBuffer = (BYTE*)malloc(sampleSize);
        CHECK (sampleBuffer==NULL, "not enough memory for sample buffer");
        memcpy(sampleBuffer, srcBuffer + sampleStart, sampleSize);
        crcOrig = XXH64(sampleBuffer, sampleSize, 0);

        /* compression tests */
        {   int const cLevel = (FUZ_rand(&lseed) % (ZSTD_maxCLevel() - (sampleSizeLog/3))) + 1;
            cSize = ZSTD_compressCCtx(ctx, cBuffer, cBufferSize, sampleBuffer, sampleSize, cLevel);
            CHECK(ZSTD_isError(cSize), "ZSTD_compressCCtx failed");

            /* compression failure test : too small dest buffer */
            if (cSize > 3) {
                const size_t missing = (FUZ_rand(&lseed) % (cSize-2)) + 1;   /* no problem, as cSize > 4 (frameHeaderSizer) */
                const size_t tooSmallSize = cSize - missing;
                const U32 endMark = 0x4DC2B1A9;
                memcpy(dstBuffer+tooSmallSize, &endMark, 4);
                { size_t const errorCode = ZSTD_compressCCtx(ctx, dstBuffer, tooSmallSize, sampleBuffer, sampleSize, cLevel);
                  CHECK(!ZSTD_isError(errorCode), "ZSTD_compressCCtx should have failed ! (buffer too small : %u < %u)", (U32)tooSmallSize, (U32)cSize); }
                { U32 endCheck; memcpy(&endCheck, dstBuffer+tooSmallSize, 4);
                  CHECK(endCheck != endMark, "ZSTD_compressCCtx : dst buffer overflow"); }
            }
        }


        /* frame header decompression test */
        {   ZSTD_frameParams dParams;
            size_t const check = ZSTD_getFrameParams(&dParams, cBuffer, cSize);
            CHECK(ZSTD_isError(check), "Frame Parameters extraction failed");
            CHECK(dParams.frameContentSize != sampleSize, "Frame content size incorrect");
        }

        /* successful decompression test */
        {   size_t const margin = (FUZ_rand(&lseed) & 1) ? 0 : (FUZ_rand(&lseed) & 31) + 1;
            dSize = ZSTD_decompress(dstBuffer, sampleSize + margin, cBuffer, cSize);
            CHECK(dSize != sampleSize, "ZSTD_decompress failed (%s) (srcSize : %u ; cSize : %u)", ZSTD_getErrorName(dSize), (U32)sampleSize, (U32)cSize);
            {   U64 const crcDest = XXH64(dstBuffer, sampleSize, 0);
                CHECK(crcOrig != crcDest, "decompression result corrupted (pos %u / %u)", (U32)findDiff(sampleBuffer, dstBuffer, sampleSize), (U32)sampleSize);
        }   }

        free(sampleBuffer);   /* no longer useful after this point */

        /* truncated src decompression test */
        {   size_t const missing = (FUZ_rand(&lseed) % (cSize-2)) + 1;   /* no problem, as cSize > 4 (frameHeaderSizer) */
            size_t const tooSmallSize = cSize - missing;
            void* cBufferTooSmall = malloc(tooSmallSize);   /* valgrind will catch overflows */
            CHECK(cBufferTooSmall == NULL, "not enough memory !");
            memcpy(cBufferTooSmall, cBuffer, tooSmallSize);
            { size_t const errorCode = ZSTD_decompress(dstBuffer, dstBufferSize, cBufferTooSmall, tooSmallSize);
              CHECK(!ZSTD_isError(errorCode), "ZSTD_decompress should have failed ! (truncated src buffer)"); }
            free(cBufferTooSmall);
        }

        /* too small dst decompression test */
        if (sampleSize > 3) {
            size_t const missing = (FUZ_rand(&lseed) % (sampleSize-2)) + 1;   /* no problem, as cSize > 4 (frameHeaderSizer) */
            size_t const tooSmallSize = sampleSize - missing;
            static const BYTE token = 0xA9;
            dstBuffer[tooSmallSize] = token;
            { size_t const errorCode = ZSTD_decompress(dstBuffer, tooSmallSize, cBuffer, cSize);
              CHECK(!ZSTD_isError(errorCode), "ZSTD_decompress should have failed : %u > %u (dst buffer too small)", (U32)errorCode, (U32)tooSmallSize); }
            CHECK(dstBuffer[tooSmallSize] != token, "ZSTD_decompress : dst buffer overflow");
        }

        /* noisy src decompression test */
        if (cSize > 6) {
            /* insert noise into src */
            {   U32 const maxNbBits = FUZ_highbit32((U32)(cSize-4));
                size_t pos = 4;   /* preserve magic number (too easy to detect) */
                for (;;) {
                    /* keep some original src */
                    {   U32 const nbBits = FUZ_rand(&lseed) % maxNbBits;
                        size_t const mask = (1<<nbBits) - 1;
                        size_t const skipLength = FUZ_rand(&lseed) & mask;
                        pos += skipLength;
                    }
                    if (pos <= cSize) break;
                    /* add noise */
                    {   U32 const nbBitsCodes = FUZ_rand(&lseed) % maxNbBits;
                        U32 const nbBits = nbBitsCodes ? nbBitsCodes-1 : 0;
                        size_t const mask = (1<<nbBits) - 1;
                        size_t const rNoiseLength = (FUZ_rand(&lseed) & mask) + 1;
                        size_t const noiseLength = MIN(rNoiseLength, cSize-pos);
                        size_t const noiseStart = FUZ_rand(&lseed) % (srcBufferSize - noiseLength);
                        memcpy(cBuffer + pos, srcBuffer + noiseStart, noiseLength);
                        pos += noiseLength;
            }   }   }

            /* decompress noisy source */
            {   U32 const endMark = 0xA9B1C3D6;
                memcpy(dstBuffer+sampleSize, &endMark, 4);
                {   size_t const decompressResult = ZSTD_decompress(dstBuffer, sampleSize, cBuffer, cSize);
                    /* result *may* be an unlikely success, but even then, it must strictly respect dst buffer boundaries */
                    CHECK((!ZSTD_isError(decompressResult)) && (decompressResult>sampleSize),
                          "ZSTD_decompress on noisy src : result is too large : %u > %u (dst buffer)", (U32)decompressResult, (U32)sampleSize);
                }
                {   U32 endCheck; memcpy(&endCheck, dstBuffer+sampleSize, 4);
                    CHECK(endMark!=endCheck, "ZSTD_decompress on noisy src : dst buffer overflow");
        }   }   }   /* noisy src decompression test */

        /*=====   Streaming compression test, scattered segments and dictionary   =====*/

        {   U32 const testLog = FUZ_rand(&lseed) % maxSrcLog;
            int const cLevel = (FUZ_rand(&lseed) % (ZSTD_maxCLevel() - (testLog/3))) + 1;
            maxTestSize = FUZ_rLogLength(&lseed, testLog);
            if (maxTestSize >= dstBufferSize) maxTestSize = dstBufferSize-1;

            sampleSize = FUZ_randomLength(&lseed, maxSampleLog);
            sampleStart = FUZ_rand(&lseed) % (srcBufferSize - sampleSize);
            dict = srcBuffer + sampleStart;
            dictSize = sampleSize;

            if (FUZ_rand(&lseed) & 15) {
                size_t const errorCode = ZSTD_compressBegin_usingDict(refCtx, dict, dictSize, cLevel);
                CHECK (ZSTD_isError(errorCode), "ZSTD_compressBegin_usingDict error : %s", ZSTD_getErrorName(errorCode));
            } else {
                ZSTD_frameParameters const fpar = { FUZ_rand(&lseed)&1, FUZ_rand(&lseed)&1 };   /* note : since dictionary is fake, dictIDflag has no impact */
                ZSTD_parameters p = (ZSTD_parameters) { ZSTD_getCParams(cLevel, 0, dictSize), fpar };
                size_t const errorCode = ZSTD_compressBegin_advanced(refCtx, dict, dictSize, p, 0);
                CHECK (ZSTD_isError(errorCode), "ZSTD_compressBegin_advanced error : %s", ZSTD_getErrorName(errorCode));
            }
            { size_t const errorCode = ZSTD_copyCCtx(ctx, refCtx);
              CHECK (ZSTD_isError(errorCode), "ZSTD_copyCCtx error : %s", ZSTD_getErrorName(errorCode)); }
        }
        XXH64_reset(&xxhState, 0);
        nbChunks = (FUZ_rand(&lseed) & 127) + 2;
        for (totalTestSize=0, cSize=0, n=0 ; n<nbChunks ; n++) {
            sampleSizeLog = FUZ_rand(&lseed) % maxSampleLog;
            sampleSize = (size_t)1 << sampleSizeLog;
            sampleSize += FUZ_rand(&lseed) & (sampleSize-1);
            sampleStart = FUZ_rand(&lseed) % (srcBufferSize - sampleSize);

            if (cBufferSize-cSize < ZSTD_compressBound(sampleSize)) break;   /* avoid invalid dstBufferTooSmall */
            if (totalTestSize+sampleSize > maxTestSize) break;

            {   size_t const compressResult = ZSTD_compressContinue(ctx, cBuffer+cSize, cBufferSize-cSize, srcBuffer+sampleStart, sampleSize);
                CHECK (ZSTD_isError(compressResult), "multi-segments compression error : %s", ZSTD_getErrorName(compressResult));
                cSize += compressResult;
            }
            XXH64_update(&xxhState, srcBuffer+sampleStart, sampleSize);
            memcpy(mirrorBuffer + totalTestSize, srcBuffer+sampleStart, sampleSize);
            totalTestSize += sampleSize;
        }
        {   size_t const flushResult = ZSTD_compressEnd(ctx, cBuffer+cSize, cBufferSize-cSize);
            CHECK (ZSTD_isError(flushResult), "multi-segments epilogue error : %s", ZSTD_getErrorName(flushResult));
            cSize += flushResult;
        }
        crcOrig = XXH64_digest(&xxhState);

        /* streaming decompression test */
        if (dictSize<8) dictSize=0, dict=NULL;   /* disable dictionary */
        { size_t const errorCode = ZSTD_decompressBegin_usingDict(dctx, dict, dictSize);
          CHECK (ZSTD_isError(errorCode), "cannot init DCtx : %s", ZSTD_getErrorName(errorCode)); }
        totalCSize = 0;
        totalGenSize = 0;
        while (totalCSize < cSize) {
            size_t const inSize = ZSTD_nextSrcSizeToDecompress(dctx);
            size_t const genSize = ZSTD_decompressContinue(dctx, dstBuffer+totalGenSize, dstBufferSize-totalGenSize, cBuffer+totalCSize, inSize);
            CHECK (ZSTD_isError(genSize), "streaming decompression error : %s", ZSTD_getErrorName(genSize));
            totalGenSize += genSize;
            totalCSize += inSize;
        }
        CHECK (ZSTD_nextSrcSizeToDecompress(dctx) != 0, "frame not fully decoded");
        CHECK (totalGenSize != totalTestSize, "decompressed data : wrong size")
        CHECK (totalCSize != cSize, "compressed data should be fully read")
        {   U64 const crcDest = XXH64(dstBuffer, totalTestSize, 0);
            if (crcDest!=crcOrig) {
                size_t const errorPos = findDiff(mirrorBuffer, dstBuffer, totalTestSize);
                CHECK (crcDest!=crcOrig, "streaming decompressed data corrupted : byte %u / %u  (%02X!=%02X)",
                   (U32)errorPos, (U32)totalTestSize, dstBuffer[errorPos], mirrorBuffer[errorPos]);
        }   }
    }   /* for ( ; (testNb <= nbTests) */
    DISPLAY("\r%u fuzzer tests completed   \n", testNb-1);

_cleanup:
    ZSTD_freeCCtx(refCtx);
    ZSTD_freeCCtx(ctx);
    ZSTD_freeDCtx(dctx);
    free(cNoiseBuffer[0]);
    free(cNoiseBuffer[1]);
    free(cNoiseBuffer[2]);
    free(cNoiseBuffer[3]);
    free(cNoiseBuffer[4]);
    free(cBuffer);
    free(dstBuffer);
    free(mirrorBuffer);
    return result;

_output_error:
    result = 1;
    goto _cleanup;
}


/*_*******************************************************
*  Command line
*********************************************************/
int FUZ_usage(const char* programName)
{
    DISPLAY( "Usage :\n");
    DISPLAY( "      %s [args]\n", programName);
    DISPLAY( "\n");
    DISPLAY( "Arguments :\n");
    DISPLAY( " -i#    : Nb of tests (default:%u) \n", nbTestsDefault);
    DISPLAY( " -s#    : Select seed (default:prompt user)\n");
    DISPLAY( " -t#    : Select starting test number (default:0)\n");
    DISPLAY( " -P#    : Select compressibility in %% (default:%u%%)\n", FUZ_compressibility_default);
    DISPLAY( " -v     : verbose\n");
    DISPLAY( " -p     : pause at the end\n");
    DISPLAY( " -h     : display help and exit\n");
    return 0;
}


int main(int argc, const char** argv)
{
    U32 seed=0;
    int seedset=0;
    int argNb;
    int nbTests = nbTestsDefault;
    int testNb = 0;
    U32 proba = FUZ_compressibility_default;
    int result=0;
    U32 mainPause = 0;
    U32 maxDuration = 0;
    const char* programName = argv[0];

    /* Check command line */
    for (argNb=1; argNb<argc; argNb++) {
        const char* argument = argv[argNb];
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
                    argument++; maxDuration=0;
                    nbTests=0;
                    while ((*argument>='0') && (*argument<='9')) {
                        nbTests *= 10;
                        nbTests += *argument - '0';
                        argument++;
                    }
                    break;

                case 'T':
                    argument++;
                    nbTests=0; maxDuration=0;
                    while ((*argument>='0') && (*argument<='9')) {
                        maxDuration *= 10;
                        maxDuration += *argument - '0';
                        argument++;
                    }
                    if (*argument=='m') maxDuration *=60, argument++;
                    if (*argument=='n') argument++;
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
                    if (proba>100) proba=100;
                    break;

                default:
                    return FUZ_usage(programName);
    }   }   }   }   /* for (argNb=1; argNb<argc; argNb++) */

    /* Get Seed */
    DISPLAY("Starting zstd tester (%i-bits, %s)\n", (int)(sizeof(size_t)*8), ZSTD_VERSION_STRING);

    if (!seedset) seed = (U32)(clock() % 10000);
    DISPLAY("Seed = %u\n", seed);
    if (proba!=FUZ_compressibility_default) DISPLAY("Compressibility : %u%%\n", proba);

    if (testNb==0)
        result = basicUnitTests(0, ((double)proba) / 100);  /* constant seed for predictability */
    if (!result)
        result = fuzzerTests(seed, nbTests, testNb, maxDuration, ((double)proba) / 100);
    if (mainPause) {
        int unused;
        DISPLAY("Press Enter \n");
        unused = getchar();
        (void)unused;
    }
    return result;
}
