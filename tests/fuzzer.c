/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */


/*-************************************
*  Compiler specific
**************************************/
#ifdef _MSC_VER    /* Visual Studio */
#  define _CRT_SECURE_NO_WARNINGS     /* fgets */
#  pragma warning(disable : 4127)     /* disable: C4127: conditional expression is constant */
#  pragma warning(disable : 4204)     /* disable: C4204: non-constant aggregate initializer */
#endif


/*-************************************
*  Includes
**************************************/
#include <stdlib.h>       /* free */
#include <stdio.h>        /* fgets, sscanf */
#include <string.h>       /* strcmp */
#include <time.h>         /* clock_t */
#define ZSTD_STATIC_LINKING_ONLY   /* ZSTD_compressContinue, ZSTD_compressBlock */
#include "zstd.h"         /* ZSTD_VERSION_STRING */
#include "zstd_errors.h"  /* ZSTD_getErrorCode */
#define ZDICT_STATIC_LINKING_ONLY
#include "zdict.h"        /* ZDICT_trainFromBuffer */
#include "datagen.h"      /* RDG_genBuffer */
#include "mem.h"
#define XXH_STATIC_LINKING_ONLY
#include "xxhash.h"       /* XXH64 */


/*-************************************
*  Constants
**************************************/
#define KB *(1U<<10)
#define MB *(1U<<20)
#define GB *(1U<<30)

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
static const clock_t g_refreshRate = CLOCKS_PER_SEC / 6;
static clock_t g_displayClock = 0;


/*-*******************************************************
*  Fuzzer functions
*********************************************************/
#define MIN(a,b) ((a)<(b)?(a):(b))

static clock_t FUZ_clockSpan(clock_t cStart)
{
    return clock() - cStart;   /* works even when overflow; max span ~ 30mn */
}


#define FUZ_rotl32(x,r) ((x << r) | (x >> (32 - r)))
static unsigned FUZ_rand(unsigned* src)
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
    while (v32) v32 >>= 1, nbBits++;
    return nbBits;
}


/*=============================================
*   Basic Unit tests
=============================================*/

#define CHECK_V(var, fn)  size_t const var = fn; if (ZSTD_isError(var)) goto _output_error
#define CHECK(fn)  { CHECK_V(err, fn); }
#define CHECKPLUS(var, fn, more)  { CHECK_V(var, fn); more; }
static int basicUnitTests(U32 seed, double compressibility)
{
    size_t const CNBuffSize = 5 MB;
    void* const CNBuffer = malloc(CNBuffSize);
    void* const compressedBuffer = malloc(ZSTD_compressBound(CNBuffSize));
    void* const decodedBuffer = malloc(CNBuffSize);
    int testResult = 0;
    U32 testNb=0;
    size_t cSize;

    /* Create compressible noise */
    if (!CNBuffer || !compressedBuffer || !decodedBuffer) {
        DISPLAY("Not enough memory, aborting\n");
        testResult = 1;
        goto _end;
    }
    RDG_genBuffer(CNBuffer, CNBuffSize, compressibility, 0., seed);

    /* Basic tests */
    DISPLAYLEVEL(4, "test%3i : ZSTD_getErrorName : ", testNb++);
    {   const char* errorString = ZSTD_getErrorName(0);
        DISPLAYLEVEL(4, "OK : %s \n", errorString);
    }

    DISPLAYLEVEL(4, "test%3i : ZSTD_getErrorName with wrong value : ", testNb++);
    {   const char* errorString = ZSTD_getErrorName(499);
        DISPLAYLEVEL(4, "OK : %s \n", errorString);
    }

    DISPLAYLEVEL(4, "test%3i : compress %u bytes : ", testNb++, (U32)CNBuffSize);
    CHECKPLUS(r, ZSTD_compress(compressedBuffer, ZSTD_compressBound(CNBuffSize),
                               CNBuffer, CNBuffSize, 1),
              cSize=r );
    DISPLAYLEVEL(4, "OK (%u bytes : %.2f%%)\n", (U32)cSize, (double)cSize/CNBuffSize*100);

    DISPLAYLEVEL(4, "test%3i : decompressed size test : ", testNb++);
    {   unsigned long long const rSize = ZSTD_getDecompressedSize(compressedBuffer, cSize);
        if (rSize != CNBuffSize) goto _output_error;
    }
    DISPLAYLEVEL(4, "OK \n");

    DISPLAYLEVEL(4, "test%3i : decompress %u bytes : ", testNb++, (U32)CNBuffSize);
    { size_t const r = ZSTD_decompress(decodedBuffer, CNBuffSize, compressedBuffer, cSize);
      if (r != CNBuffSize) goto _output_error; }
    DISPLAYLEVEL(4, "OK \n");

    DISPLAYLEVEL(4, "test%3i : check decompressed result : ", testNb++);
    {   size_t u;
        for (u=0; u<CNBuffSize; u++) {
            if (((BYTE*)decodedBuffer)[u] != ((BYTE*)CNBuffer)[u]) goto _output_error;;
    }   }
    DISPLAYLEVEL(4, "OK \n");

    DISPLAYLEVEL(4, "test%3i : decompress with 1 missing byte : ", testNb++);
    { size_t const r = ZSTD_decompress(decodedBuffer, CNBuffSize, compressedBuffer, cSize-1);
      if (!ZSTD_isError(r)) goto _output_error;
      if (ZSTD_getErrorCode((size_t)r) != ZSTD_error_srcSize_wrong) goto _output_error; }
    DISPLAYLEVEL(4, "OK \n");

    DISPLAYLEVEL(4, "test%3i : decompress with 1 too much byte : ", testNb++);
    { size_t const r = ZSTD_decompress(decodedBuffer, CNBuffSize, compressedBuffer, cSize+1);
      if (!ZSTD_isError(r)) goto _output_error;
      if (ZSTD_getErrorCode(r) != ZSTD_error_srcSize_wrong) goto _output_error; }
    DISPLAYLEVEL(4, "OK \n");

    /* Dictionary and CCtx Duplication tests */
    {   ZSTD_CCtx* const ctxOrig = ZSTD_createCCtx();
        ZSTD_CCtx* const ctxDuplicated = ZSTD_createCCtx();
        ZSTD_DCtx* const dctx = ZSTD_createDCtx();
        static const size_t dictSize = 551;

        DISPLAYLEVEL(4, "test%3i : copy context too soon : ", testNb++);
        { size_t const copyResult = ZSTD_copyCCtx(ctxDuplicated, ctxOrig, 0);
          if (!ZSTD_isError(copyResult)) goto _output_error; }   /* error must be detected */
        DISPLAYLEVEL(4, "OK \n");

        DISPLAYLEVEL(4, "test%3i : load dictionary into context : ", testNb++);
        CHECK( ZSTD_compressBegin_usingDict(ctxOrig, CNBuffer, dictSize, 2) );
        CHECK( ZSTD_copyCCtx(ctxDuplicated, ctxOrig, CNBuffSize - dictSize) );
        DISPLAYLEVEL(4, "OK \n");

        DISPLAYLEVEL(4, "test%3i : compress with flat dictionary : ", testNb++);
        cSize = 0;
        CHECKPLUS(r, ZSTD_compressEnd(ctxOrig, compressedBuffer, ZSTD_compressBound(CNBuffSize),
                                           (const char*)CNBuffer + dictSize, CNBuffSize - dictSize),
                  cSize += r);
        DISPLAYLEVEL(4, "OK (%u bytes : %.2f%%)\n", (U32)cSize, (double)cSize/CNBuffSize*100);

        DISPLAYLEVEL(4, "test%3i : frame built with flat dictionary should be decompressible : ", testNb++);
        CHECKPLUS(r, ZSTD_decompress_usingDict(dctx,
                                       decodedBuffer, CNBuffSize,
                                       compressedBuffer, cSize,
                                       CNBuffer, dictSize),
                  if (r != CNBuffSize - dictSize) goto _output_error);
        DISPLAYLEVEL(4, "OK \n");

        DISPLAYLEVEL(4, "test%3i : compress with duplicated context : ", testNb++);
        {   size_t const cSizeOrig = cSize;
            cSize = 0;
            CHECKPLUS(r, ZSTD_compressEnd(ctxDuplicated, compressedBuffer, ZSTD_compressBound(CNBuffSize),
                                               (const char*)CNBuffer + dictSize, CNBuffSize - dictSize),
                      cSize += r);
            if (cSize != cSizeOrig) goto _output_error;   /* should be identical ==> same size */
        }
        DISPLAYLEVEL(4, "OK (%u bytes : %.2f%%)\n", (U32)cSize, (double)cSize/CNBuffSize*100);

        DISPLAYLEVEL(4, "test%3i : frame built with duplicated context should be decompressible : ", testNb++);
        CHECKPLUS(r, ZSTD_decompress_usingDict(dctx,
                                           decodedBuffer, CNBuffSize,
                                           compressedBuffer, cSize,
                                           CNBuffer, dictSize),
                  if (r != CNBuffSize - dictSize) goto _output_error);
        DISPLAYLEVEL(4, "OK \n");

        DISPLAYLEVEL(4, "test%3i : check content size on duplicated context : ", testNb++);
        {   size_t const testSize = CNBuffSize / 3;
            {   ZSTD_parameters p = ZSTD_getParams(2, testSize, dictSize);
                p.fParams.contentSizeFlag = 1;
                CHECK( ZSTD_compressBegin_advanced(ctxOrig, CNBuffer, dictSize, p, testSize-1) );
            }
            CHECK( ZSTD_copyCCtx(ctxDuplicated, ctxOrig, testSize) );

            CHECKPLUS(r, ZSTD_compressEnd(ctxDuplicated, compressedBuffer, ZSTD_compressBound(testSize),
                                          (const char*)CNBuffer + dictSize, testSize),
                      cSize = r);
            {   ZSTD_frameParams fp;
                if (ZSTD_getFrameParams(&fp, compressedBuffer, cSize)) goto _output_error;
                if ((fp.frameContentSize != testSize) && (fp.frameContentSize != 0)) goto _output_error;
        }   }
        DISPLAYLEVEL(4, "OK \n");

        ZSTD_freeCCtx(ctxOrig);
        ZSTD_freeCCtx(ctxDuplicated);
        ZSTD_freeDCtx(dctx);
    }

    /* Dictionary and dictBuilder tests */
    {   ZSTD_CCtx* const cctx = ZSTD_createCCtx();
        ZSTD_DCtx* const dctx = ZSTD_createDCtx();
        size_t dictSize = 16 KB;
        void* dictBuffer = malloc(dictSize);
        size_t const totalSampleSize = 1 MB;
        size_t const sampleUnitSize = 8 KB;
        U32 const nbSamples = (U32)(totalSampleSize / sampleUnitSize);
        size_t* const samplesSizes = (size_t*) malloc(nbSamples * sizeof(size_t));
        U32 dictID;

        if (dictBuffer==NULL || samplesSizes==NULL) {
            free(dictBuffer);
            free(samplesSizes);
            goto _output_error;
        }

        DISPLAYLEVEL(4, "test%3i : dictBuilder : ", testNb++);
        { U32 u; for (u=0; u<nbSamples; u++) samplesSizes[u] = sampleUnitSize; }
        dictSize = ZDICT_trainFromBuffer(dictBuffer, dictSize,
                                         CNBuffer, samplesSizes, nbSamples);
        if (ZDICT_isError(dictSize)) goto _output_error;
        DISPLAYLEVEL(4, "OK, created dictionary of size %u \n", (U32)dictSize);

        DISPLAYLEVEL(4, "test%3i : check dictID : ", testNb++);
        dictID = ZDICT_getDictID(dictBuffer, dictSize);
        if (dictID==0) goto _output_error;
        DISPLAYLEVEL(4, "OK : %u \n", dictID);

        DISPLAYLEVEL(4, "test%3i : compress with dictionary : ", testNb++);
        cSize = ZSTD_compress_usingDict(cctx, compressedBuffer, ZSTD_compressBound(CNBuffSize),
                                        CNBuffer, CNBuffSize,
                                        dictBuffer, dictSize, 4);
        if (ZSTD_isError(cSize)) goto _output_error;
        DISPLAYLEVEL(4, "OK (%u bytes : %.2f%%)\n", (U32)cSize, (double)cSize/CNBuffSize*100);

        DISPLAYLEVEL(4, "test%3i : retrieve dictID from dictionary : ", testNb++);
        {   U32 const did = ZSTD_getDictID_fromDict(dictBuffer, dictSize);
            if (did != dictID) goto _output_error;   /* non-conformant (content-only) dictionary */
        }
        DISPLAYLEVEL(4, "OK \n");

        DISPLAYLEVEL(4, "test%3i : retrieve dictID from frame : ", testNb++);
        {   U32 const did = ZSTD_getDictID_fromFrame(compressedBuffer, cSize);
            if (did != dictID) goto _output_error;   /* non-conformant (content-only) dictionary */
        }
        DISPLAYLEVEL(4, "OK \n");

        DISPLAYLEVEL(4, "test%3i : frame built with dictionary should be decompressible : ", testNb++);
        CHECKPLUS(r, ZSTD_decompress_usingDict(dctx,
                                       decodedBuffer, CNBuffSize,
                                       compressedBuffer, cSize,
                                       dictBuffer, dictSize),
                  if (r != CNBuffSize) goto _output_error);
        DISPLAYLEVEL(4, "OK \n");

        DISPLAYLEVEL(4, "test%3i : compress without dictID : ", testNb++);
        {   ZSTD_parameters p = ZSTD_getParams(3, CNBuffSize, dictSize);
            p.fParams.noDictIDFlag = 1;
            cSize = ZSTD_compress_advanced(cctx, compressedBuffer, ZSTD_compressBound(CNBuffSize),
                                           CNBuffer, CNBuffSize,
                                           dictBuffer, dictSize, p);
            if (ZSTD_isError(cSize)) goto _output_error;
        }
        DISPLAYLEVEL(4, "OK (%u bytes : %.2f%%)\n", (U32)cSize, (double)cSize/CNBuffSize*100);

        DISPLAYLEVEL(4, "test%3i : frame built without dictID should be decompressible : ", testNb++);
        CHECKPLUS(r, ZSTD_decompress_usingDict(dctx,
                                       decodedBuffer, CNBuffSize,
                                       compressedBuffer, cSize,
                                       dictBuffer, dictSize),
                  if (r != CNBuffSize) goto _output_error);
        DISPLAYLEVEL(4, "OK \n");

        DISPLAYLEVEL(4, "test%3i : dictionary containing only header should return error : ", testNb++);
        {
          const size_t ret = ZSTD_decompress_usingDict(
              dctx, decodedBuffer, CNBuffSize, compressedBuffer, cSize,
              "\x37\xa4\x30\xec\x11\x22\x33\x44", 8);
          if (ZSTD_getErrorCode(ret) != ZSTD_error_dictionary_corrupted) goto _output_error;
        }
        DISPLAYLEVEL(4, "OK \n");

        ZSTD_freeCCtx(cctx);
        ZSTD_freeDCtx(dctx);
        free(dictBuffer);
        free(samplesSizes);
    }

    /* COVER dictionary builder tests */
    {   ZSTD_CCtx* const cctx = ZSTD_createCCtx();
        ZSTD_DCtx* const dctx = ZSTD_createDCtx();
        size_t dictSize = 16 KB;
        size_t optDictSize = dictSize;
        void* dictBuffer = malloc(dictSize);
        size_t const totalSampleSize = 1 MB;
        size_t const sampleUnitSize = 8 KB;
        U32 const nbSamples = (U32)(totalSampleSize / sampleUnitSize);
        size_t* const samplesSizes = (size_t*) malloc(nbSamples * sizeof(size_t));
        COVER_params_t params;
        U32 dictID;

        if (dictBuffer==NULL || samplesSizes==NULL) {
            free(dictBuffer);
            free(samplesSizes);
            goto _output_error;
        }

        DISPLAYLEVEL(4, "test%3i : COVER_trainFromBuffer : ", testNb++);
        { U32 u; for (u=0; u<nbSamples; u++) samplesSizes[u] = sampleUnitSize; }
        memset(&params, 0, sizeof(params));
        params.d = 1 + (FUZ_rand(&seed) % 16);
        params.k = params.d + (FUZ_rand(&seed) % 256);
        dictSize = COVER_trainFromBuffer(dictBuffer, dictSize,
                                         CNBuffer, samplesSizes, nbSamples,
                                         params);
        if (ZDICT_isError(dictSize)) goto _output_error;
        DISPLAYLEVEL(4, "OK, created dictionary of size %u \n", (U32)dictSize);

        DISPLAYLEVEL(4, "test%3i : check dictID : ", testNb++);
        dictID = ZDICT_getDictID(dictBuffer, dictSize);
        if (dictID==0) goto _output_error;
        DISPLAYLEVEL(4, "OK : %u \n", dictID);

        DISPLAYLEVEL(4, "test%3i : COVER_optimizeTrainFromBuffer : ", testNb++);
        memset(&params, 0, sizeof(params));
        params.steps = 4;
        optDictSize = COVER_optimizeTrainFromBuffer(dictBuffer, optDictSize,
                                                    CNBuffer, samplesSizes, nbSamples,
                                                    &params);
        if (ZDICT_isError(optDictSize)) goto _output_error;
        DISPLAYLEVEL(4, "OK, created dictionary of size %u \n", (U32)optDictSize);

        DISPLAYLEVEL(4, "test%3i : check dictID : ", testNb++);
        dictID = ZDICT_getDictID(dictBuffer, optDictSize);
        if (dictID==0) goto _output_error;
        DISPLAYLEVEL(4, "OK : %u \n", dictID);

        ZSTD_freeCCtx(cctx);
        ZSTD_freeDCtx(dctx);
        free(dictBuffer);
        free(samplesSizes);
    }

    /* Decompression defense tests */
    DISPLAYLEVEL(4, "test%3i : Check input length for magic number : ", testNb++);
    { size_t const r = ZSTD_decompress(decodedBuffer, CNBuffSize, CNBuffer, 3);
      if (!ZSTD_isError(r)) goto _output_error;
      if (r != (size_t)-ZSTD_error_srcSize_wrong) goto _output_error; }
    DISPLAYLEVEL(4, "OK \n");

    DISPLAYLEVEL(4, "test%3i : Check magic Number : ", testNb++);
    ((char*)(CNBuffer))[0] = 1;
    { size_t const r = ZSTD_decompress(decodedBuffer, CNBuffSize, CNBuffer, 4);
      if (!ZSTD_isError(r)) goto _output_error; }
    DISPLAYLEVEL(4, "OK \n");

    /* block API tests */
    {   ZSTD_CCtx* const cctx = ZSTD_createCCtx();
        ZSTD_DCtx* const dctx = ZSTD_createDCtx();
        static const size_t dictSize = 65 KB;
        static const size_t blockSize = 100 KB;   /* won't cause pb with small dict size */
        size_t cSize2;

        /* basic block compression */
        DISPLAYLEVEL(4, "test%3i : Block compression test : ", testNb++);
        CHECK( ZSTD_compressBegin(cctx, 5) );
        cSize = ZSTD_compressBlock(cctx, compressedBuffer, ZSTD_compressBound(blockSize), CNBuffer, blockSize);
        if (ZSTD_isError(cSize)) goto _output_error;
        DISPLAYLEVEL(4, "OK \n");

        DISPLAYLEVEL(4, "test%3i : Block decompression test : ", testNb++);
        CHECK( ZSTD_decompressBegin(dctx) );
        { CHECK_V(r, ZSTD_decompressBlock(dctx, decodedBuffer, CNBuffSize, compressedBuffer, cSize) );
          if (r != blockSize) goto _output_error; }
        DISPLAYLEVEL(4, "OK \n");

        /* dictionary block compression */
        DISPLAYLEVEL(4, "test%3i : Dictionary Block compression test : ", testNb++);
        CHECK( ZSTD_compressBegin_usingDict(cctx, CNBuffer, dictSize, 5) );
        cSize = ZSTD_compressBlock(cctx, compressedBuffer, ZSTD_compressBound(blockSize), (char*)CNBuffer+dictSize, blockSize);
        if (ZSTD_isError(cSize)) goto _output_error;
        cSize2 = ZSTD_compressBlock(cctx, (char*)compressedBuffer+cSize, ZSTD_compressBound(blockSize), (char*)CNBuffer+dictSize+blockSize, blockSize);
        if (ZSTD_isError(cSize2)) goto _output_error;
        memcpy((char*)compressedBuffer+cSize, (char*)CNBuffer+dictSize+blockSize, blockSize);   /* fake non-compressed block */
        cSize2 = ZSTD_compressBlock(cctx, (char*)compressedBuffer+cSize+blockSize, ZSTD_compressBound(blockSize),
                                          (char*)CNBuffer+dictSize+2*blockSize, blockSize);
        if (ZSTD_isError(cSize2)) goto _output_error;
        DISPLAYLEVEL(4, "OK \n");

        DISPLAYLEVEL(4, "test%3i : Dictionary Block decompression test : ", testNb++);
        CHECK( ZSTD_decompressBegin_usingDict(dctx, CNBuffer, dictSize) );
        { CHECK_V( r, ZSTD_decompressBlock(dctx, decodedBuffer, CNBuffSize, compressedBuffer, cSize) );
          if (r != blockSize) goto _output_error; }
        ZSTD_insertBlock(dctx, (char*)decodedBuffer+blockSize, blockSize);   /* insert non-compressed block into dctx history */
        { CHECK_V( r, ZSTD_decompressBlock(dctx, (char*)decodedBuffer+2*blockSize, CNBuffSize, (char*)compressedBuffer+cSize+blockSize, cSize2) );
          if (r != blockSize) goto _output_error; }
        DISPLAYLEVEL(4, "OK \n");

        ZSTD_freeCCtx(cctx);
        ZSTD_freeDCtx(dctx);
    }

    /* long rle test */
    {   size_t sampleSize = 0;
        DISPLAYLEVEL(4, "test%3i : Long RLE test : ", testNb++);
        RDG_genBuffer(CNBuffer, sampleSize, compressibility, 0., seed+1);
        memset((char*)CNBuffer+sampleSize, 'B', 256 KB - 1);
        sampleSize += 256 KB - 1;
        RDG_genBuffer((char*)CNBuffer+sampleSize, 96 KB, compressibility, 0., seed+2);
        sampleSize += 96 KB;
        cSize = ZSTD_compress(compressedBuffer, ZSTD_compressBound(sampleSize), CNBuffer, sampleSize, 1);
        if (ZSTD_isError(cSize)) goto _output_error;
        { CHECK_V(regenSize, ZSTD_decompress(decodedBuffer, sampleSize, compressedBuffer, cSize));
          if (regenSize!=sampleSize) goto _output_error; }
        DISPLAYLEVEL(4, "OK \n");
    }

    /* All zeroes test (test bug #137) */
    #define ZEROESLENGTH 100
    DISPLAYLEVEL(4, "test%3i : compress %u zeroes : ", testNb++, ZEROESLENGTH);
    memset(CNBuffer, 0, ZEROESLENGTH);
    { CHECK_V(r, ZSTD_compress(compressedBuffer, ZSTD_compressBound(ZEROESLENGTH), CNBuffer, ZEROESLENGTH, 1) );
      cSize = r; }
    DISPLAYLEVEL(4, "OK (%u bytes : %.2f%%)\n", (U32)cSize, (double)cSize/ZEROESLENGTH*100);

    DISPLAYLEVEL(4, "test%3i : decompress %u zeroes : ", testNb++, ZEROESLENGTH);
    { CHECK_V(r, ZSTD_decompress(decodedBuffer, ZEROESLENGTH, compressedBuffer, cSize) );
      if (r != ZEROESLENGTH) goto _output_error; }
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
        {   int i;
            for (i=0; i < NB3BYTESSEQ; i++) {
                _3BytesSeqs[i][0] = (BYTE)(FUZ_rand(&rSeed) & 255);
                _3BytesSeqs[i][1] = (BYTE)(FUZ_rand(&rSeed) & 255);
                _3BytesSeqs[i][2] = (BYTE)(FUZ_rand(&rSeed) & 255);
        }   }

        /* randomly fills CNBuffer with prepared 3-bytes sequences */
        {   int i;
            for (i=0; i < _3BYTESTESTLENGTH; i += 3) {   /* note : CNBuffer size > _3BYTESTESTLENGTH+3 */
                U32 const id = FUZ_rand(&rSeed) & NB3BYTESSEQMASK;
                ((BYTE*)CNBuffer)[i+0] = _3BytesSeqs[id][0];
                ((BYTE*)CNBuffer)[i+1] = _3BytesSeqs[id][1];
                ((BYTE*)CNBuffer)[i+2] = _3BytesSeqs[id][2];
    }   }   }
    DISPLAYLEVEL(4, "test%3i : compress lots 3-bytes sequences : ", testNb++);
    { CHECK_V(r, ZSTD_compress(compressedBuffer, ZSTD_compressBound(_3BYTESTESTLENGTH),
                                 CNBuffer, _3BYTESTESTLENGTH, 19) );
      cSize = r; }
    DISPLAYLEVEL(4, "OK (%u bytes : %.2f%%)\n", (U32)cSize, (double)cSize/_3BYTESTESTLENGTH*100);

    DISPLAYLEVEL(4, "test%3i : decompress lots 3-bytes sequence : ", testNb++);
    { CHECK_V(r, ZSTD_decompress(decodedBuffer, _3BYTESTESTLENGTH, compressedBuffer, cSize) );
      if (r != _3BYTESTESTLENGTH) goto _output_error; }
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
    size_t u;
    for (u=0; u<max; u++) {
        if (b1[u] != b2[u]) break;
    }
    return u;
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

#undef CHECK
#define CHECK(cond, ...) if (cond) { DISPLAY("Error => "); DISPLAY(__VA_ARGS__); \
                         DISPLAY(" (seed %u, test nb %u)  \n", seed, testNb); goto _output_error; }

static int fuzzerTests(U32 seed, U32 nbTests, unsigned startTest, U32 const maxDurationS, double compressibility)
{
    static const U32 maxSrcLog = 23;
    static const U32 maxSampleLog = 22;
    size_t const srcBufferSize = (size_t)1<<maxSrcLog;
    size_t const dstBufferSize = (size_t)1<<maxSampleLog;
    size_t const cBufferSize   = ZSTD_compressBound(dstBufferSize);
    BYTE* cNoiseBuffer[5];
    BYTE* srcBuffer;   /* jumping pointer */
    BYTE* const cBuffer = (BYTE*) malloc (cBufferSize);
    BYTE* const dstBuffer = (BYTE*) malloc (dstBufferSize);
    BYTE* const mirrorBuffer = (BYTE*) malloc (dstBufferSize);
    ZSTD_CCtx* const refCtx = ZSTD_createCCtx();
    ZSTD_CCtx* const ctx = ZSTD_createCCtx();
    ZSTD_DCtx* const dctx = ZSTD_createDCtx();
    U32 result = 0;
    U32 testNb = 0;
    U32 coreSeed = seed, lseed = 0;
    clock_t const startClock = clock();
    clock_t const maxClockSpan = maxDurationS * CLOCKS_PER_SEC;

    /* allocation */
    cNoiseBuffer[0] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[1] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[2] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[3] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[4] = (BYTE*)malloc (srcBufferSize);
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
        size_t sampleSize, maxTestSize, totalTestSize;
        size_t cSize, totalCSize, totalGenSize;
        XXH64_state_t xxhState;
        U64 crcOrig;
        BYTE* sampleBuffer;
        const BYTE* dict;
        size_t dictSize;

        /* notification */
        if (nbTests >= testNb) { DISPLAYUPDATE(2, "\r%6u/%6u    ", testNb, nbTests); }
        else { DISPLAYUPDATE(2, "\r%6u          ", testNb); }

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
        sampleSize = FUZ_randomLength(&lseed, maxSampleLog);

        /* create sample buffer (to catch read error with valgrind & sanitizers)  */
        sampleBuffer = (BYTE*)malloc(sampleSize);
        CHECK(sampleBuffer==NULL, "not enough memory for sample buffer");
        { size_t const sampleStart = FUZ_rand(&lseed) % (srcBufferSize - sampleSize);
          memcpy(sampleBuffer, srcBuffer + sampleStart, sampleSize); }
        crcOrig = XXH64(sampleBuffer, sampleSize, 0);

        /* compression tests */
        {   unsigned const cLevel = (FUZ_rand(&lseed) % (ZSTD_maxCLevel() - (FUZ_highbit32((U32)sampleSize)/3))) + 1;
            cSize = ZSTD_compressCCtx(ctx, cBuffer, cBufferSize, sampleBuffer, sampleSize, cLevel);
            CHECK(ZSTD_isError(cSize), "ZSTD_compressCCtx failed : %s", ZSTD_getErrorName(cSize));

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
        }   }

        /* Decompressed size test */
        {   unsigned long long const rSize = ZSTD_getDecompressedSize(cBuffer, cSize);
            CHECK(rSize != sampleSize, "decompressed size incorrect");
        }

        /* frame header decompression test */
        {   ZSTD_frameParams dParams;
            size_t const check = ZSTD_getFrameParams(&dParams, cBuffer, cSize);
            CHECK(ZSTD_isError(check), "Frame Parameters extraction failed");
            CHECK(dParams.frameContentSize != sampleSize, "Frame content size incorrect");
        }

        /* successful decompression test */
        {   size_t const margin = (FUZ_rand(&lseed) & 1) ? 0 : (FUZ_rand(&lseed) & 31) + 1;
            size_t const dSize = ZSTD_decompress(dstBuffer, sampleSize + margin, cBuffer, cSize);
            CHECK(dSize != sampleSize, "ZSTD_decompress failed (%s) (srcSize : %u ; cSize : %u)", ZSTD_getErrorName(dSize), (U32)sampleSize, (U32)cSize);
            {   U64 const crcDest = XXH64(dstBuffer, sampleSize, 0);
                CHECK(crcOrig != crcDest, "decompression result corrupted (pos %u / %u)", (U32)findDiff(sampleBuffer, dstBuffer, sampleSize), (U32)sampleSize);
        }   }

        free(sampleBuffer);   /* no longer useful after this point */

        /* truncated src decompression test */
        {   size_t const missing = (FUZ_rand(&lseed) % (cSize-2)) + 1;   /* no problem, as cSize > 4 (frameHeaderSizer) */
            size_t const tooSmallSize = cSize - missing;
            void* cBufferTooSmall = malloc(tooSmallSize);   /* valgrind will catch read overflows */
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

            dictSize = FUZ_randomLength(&lseed, maxSampleLog);   /* needed also for decompression */
            dict = srcBuffer + (FUZ_rand(&lseed) % (srcBufferSize - dictSize));

            if (FUZ_rand(&lseed) & 0xF) {
                size_t const errorCode = ZSTD_compressBegin_usingDict(refCtx, dict, dictSize, cLevel);
                CHECK (ZSTD_isError(errorCode), "ZSTD_compressBegin_usingDict error : %s", ZSTD_getErrorName(errorCode));
            } else {
                ZSTD_compressionParameters const cPar = ZSTD_getCParams(cLevel, 0, dictSize);
                ZSTD_frameParameters const fpar = { FUZ_rand(&lseed)&1 /* contentSizeFlag */,
                                                    !(FUZ_rand(&lseed)&3) /* contentChecksumFlag*/,
                                                    0 /*NodictID*/ };   /* note : since dictionary is fake, dictIDflag has no impact */
                ZSTD_parameters p;
                size_t errorCode;
                p.cParams = cPar; p.fParams = fpar;
                errorCode = ZSTD_compressBegin_advanced(refCtx, dict, dictSize, p, 0);
                CHECK (ZSTD_isError(errorCode), "ZSTD_compressBegin_advanced error : %s", ZSTD_getErrorName(errorCode));
            }
            {   size_t const errorCode = ZSTD_copyCCtx(ctx, refCtx, 0);
                CHECK (ZSTD_isError(errorCode), "ZSTD_copyCCtx error : %s", ZSTD_getErrorName(errorCode));
        }   }
        XXH64_reset(&xxhState, 0);
        ZSTD_setCCtxParameter(ctx, ZSTD_p_forceWindow, FUZ_rand(&lseed) & 1);
        {   U32 const nbChunks = (FUZ_rand(&lseed) & 127) + 2;
            U32 n;
            for (totalTestSize=0, cSize=0, n=0 ; n<nbChunks ; n++) {
                size_t const segmentSize = FUZ_randomLength(&lseed, maxSampleLog);
                size_t const segmentStart = FUZ_rand(&lseed) % (srcBufferSize - segmentSize);

                if (cBufferSize-cSize < ZSTD_compressBound(segmentSize)) break;   /* avoid invalid dstBufferTooSmall */
                if (totalTestSize+segmentSize > maxTestSize) break;

                {   size_t const compressResult = ZSTD_compressContinue(ctx, cBuffer+cSize, cBufferSize-cSize, srcBuffer+segmentStart, segmentSize);
                    CHECK (ZSTD_isError(compressResult), "multi-segments compression error : %s", ZSTD_getErrorName(compressResult));
                    cSize += compressResult;
                }
                XXH64_update(&xxhState, srcBuffer+segmentStart, segmentSize);
                memcpy(mirrorBuffer + totalTestSize, srcBuffer+segmentStart, segmentSize);
                totalTestSize += segmentSize;
        }   }

        {   size_t const flushResult = ZSTD_compressEnd(ctx, cBuffer+cSize, cBufferSize-cSize, NULL, 0);
            CHECK (ZSTD_isError(flushResult), "multi-segments epilogue error : %s", ZSTD_getErrorName(flushResult));
            cSize += flushResult;
        }
        crcOrig = XXH64_digest(&xxhState);

        /* streaming decompression test */
        if (dictSize<8) dictSize=0, dict=NULL;   /* disable dictionary */
        { size_t const errorCode = ZSTD_decompressBegin_usingDict(dctx, dict, dictSize);
          CHECK (ZSTD_isError(errorCode), "ZSTD_decompressBegin_usingDict error : %s", ZSTD_getErrorName(errorCode)); }
        totalCSize = 0;
        totalGenSize = 0;
        while (totalCSize < cSize) {
            size_t const inSize = ZSTD_nextSrcSizeToDecompress(dctx);
            size_t const genSize = ZSTD_decompressContinue(dctx, dstBuffer+totalGenSize, dstBufferSize-totalGenSize, cBuffer+totalCSize, inSize);
            CHECK (ZSTD_isError(genSize), "ZSTD_decompressContinue error : %s", ZSTD_getErrorName(genSize));
            totalGenSize += genSize;
            totalCSize += inSize;
        }
        CHECK (ZSTD_nextSrcSizeToDecompress(dctx) != 0, "frame not fully decoded");
        CHECK (totalGenSize != totalTestSize, "streaming decompressed data : wrong size")
        CHECK (totalCSize != cSize, "compressed data should be fully read")
        {   U64 const crcDest = XXH64(dstBuffer, totalTestSize, 0);
            if (crcDest!=crcOrig) {
                size_t const errorPos = findDiff(mirrorBuffer, dstBuffer, totalTestSize);
                CHECK (1, "streaming decompressed data corrupted : byte %u / %u  (%02X!=%02X)",
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

    if (!seedset) {
        time_t const t = time(NULL);
        U32 const h = XXH32(&t, sizeof(t), 1);
        seed = h % 10000;
    }

    DISPLAY("Seed = %u\n", seed);
    if (proba!=FUZ_compressibility_default) DISPLAY("Compressibility : %u%%\n", proba);

    if (nbTests < testNb) nbTests = testNb;

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
