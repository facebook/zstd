/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */


/*_************************************
*  Includes
**************************************/
#define _CRT_SECURE_NO_WARNINGS /* disable Visual warning that it doesn't like fopen() */
#define ZSTD_DISABLE_DEPRECATE_WARNINGS /* No deprecation warnings, we still bench some deprecated functions */
#include <limits.h>
#include "util.h"        /* Compiler options, UTIL_GetFileSize */
#include <stdlib.h>      /* malloc */
#include <stdio.h>       /* fprintf, fopen, ftello64 */
#include <assert.h>

#include "mem.h"         /* U32 */
#include "compress/zstd_compress_internal.h"
#ifndef ZSTD_DLL_IMPORT
    #include "zstd_internal.h"   /* ZSTD_decodeSeqHeaders, ZSTD_blockHeaderSize, ZSTD_getcBlockSize, blockType_e, KB, MB */
    #include "decompress/zstd_decompress_internal.h"   /* ZSTD_DCtx struct */
#else
    #define KB *(1 <<10)
    #define MB *(1 <<20)
    #define GB *(1U<<30)
    typedef enum { bt_raw, bt_rle, bt_compressed, bt_reserved } blockType_e;
#endif
#define ZSTD_STATIC_LINKING_ONLY  /* ZSTD_compressBegin, ZSTD_compressContinue, etc. */
#include "zstd.h"        /* ZSTD_versionString */
#include "util.h"        /* time functions */
#include "datagen.h"
#include "lorem.h"
#include "benchfn.h"     /* CustomBench */
#include "benchzstd.h"   /* MB_UNIT */

/*_************************************
*  Constants
**************************************/
#define PROGRAM_DESCRIPTION "Zstandard speed analyzer"
#define AUTHOR "Yann Collet"
#define WELCOME_MESSAGE "*** %s %s %i-bits, by %s (%s) ***\n", PROGRAM_DESCRIPTION, ZSTD_versionString(), (int)(sizeof(void*)*8), AUTHOR, __DATE__

#define NBLOOPS    6
#define TIMELOOP_S 2

#define MAX_MEM    (1984 MB)

#define DEFAULT_CLEVEL 1

#define COMPRESSIBILITY_DEFAULT (-1.0)
static const size_t kSampleSizeDefault = 10000000;

#define TIMELOOP_NANOSEC      (1*1000000000ULL) /* 1 second */


/*_************************************
*  Macros
**************************************/
#define DISPLAY(...)  fprintf(stderr, __VA_ARGS__)

#define CONTROL(c)  { if (!(c)) { abort(); } }   /* like assert(), but cannot be disabled */


/*_************************************
*  Benchmark Parameters
**************************************/
static unsigned g_nbIterations = NBLOOPS;


/*_*******************************************************
*  Private functions
*********************************************************/
static size_t BMK_findMaxMem(U64 requiredMem)
{
    size_t const step = 64 MB;
    void* testmem = NULL;

    requiredMem = (((requiredMem >> 26) + 1) << 26);
    if (requiredMem > MAX_MEM) requiredMem = MAX_MEM;

    requiredMem += step;
    do {
        testmem = malloc ((size_t)requiredMem);
        requiredMem -= step;
    } while (!testmem);

    free (testmem);
    return (size_t) requiredMem;
}


/*_*******************************************************
*  Benchmark wrappers
*********************************************************/

static ZSTD_CCtx* g_zcc = NULL;
static size_t
local_ZSTD_compress(const void* src, size_t srcSize,
                    void* dst, size_t dstSize,
                    void* payload)
{
    ZSTD_parameters p;
    ZSTD_frameParameters f = { 1 /* contentSizeHeader*/, 0, 0 };
    p.fParams = f;
    p.cParams = *(ZSTD_compressionParameters*)payload;
    return ZSTD_compress_advanced (g_zcc, dst, dstSize, src, srcSize, NULL ,0, p);
}

static size_t
local_ZSTD_compress_freshCCtx(const void* src, size_t srcSize,
                    void* dst, size_t dstSize,
                    void* payload)
{
    ZSTD_parameters p;
    ZSTD_frameParameters f = { 1 /* contentSizeHeader*/, 0, 0 };
    p.fParams = f;
    p.cParams = *(ZSTD_compressionParameters*)payload;
    if (g_zcc != NULL) ZSTD_freeCCtx(g_zcc);
    g_zcc = ZSTD_createCCtx();
    assert(g_zcc != NULL);
    {   size_t const r = ZSTD_compress_advanced (g_zcc, dst, dstSize, src, srcSize, NULL ,0, p);
        ZSTD_freeCCtx(g_zcc);
        g_zcc = NULL;
        return r;
    }
}

typedef struct {
    void* prepBuffer;
    size_t prepSize;
    void* dst;
    size_t dstCapacity;
    size_t fixedOrigSize;  /* optional, 0 means "no modification" */
} PrepResult;
#define PREPRESULT_INIT { NULL, 0, NULL, 0, 0 }

static PrepResult prepDecompress(const void* src, size_t srcSize, int cLevel)
{
    size_t prepCapacity = ZSTD_compressBound(srcSize);
    void* prepBuffer = malloc(prepCapacity);
    size_t cSize = ZSTD_compress(prepBuffer, prepCapacity, src, srcSize, cLevel);
    void* dst = malloc(srcSize);
    PrepResult r = PREPRESULT_INIT;
    assert(dst != NULL);
    r.prepBuffer = prepBuffer;
    r.prepSize = cSize;
    r.dst = dst;
    r.dstCapacity = srcSize;
    return r;
}

static size_t local_ZSTD_decompress(const void* src, size_t srcSize,
                                    void* dst, size_t dstSize,
                                    void* unused)
{
    (void)unused;
    return ZSTD_decompress(dst, dstSize, src, srcSize);
}

static ZSTD_DCtx* g_zdc = NULL; /* will be initialized within benchMem */
static size_t local_ZSTD_decompressDCtx(const void* src, size_t srcSize,
                                    void* dst, size_t dstSize,
                                    void* unused)
{
    (void)unused;
    return ZSTD_decompressDCtx(g_zdc, dst, dstSize, src, srcSize);
}

#ifndef ZSTD_DLL_IMPORT

static PrepResult prepLiterals(const void* src, size_t srcSize, int cLevel)
{
    PrepResult r = PREPRESULT_INIT;
    size_t dstCapacity = srcSize;
    void* dst = malloc(dstCapacity);
    void* prepBuffer;
    size_t prepSize = ZSTD_compress(dst, dstCapacity, src, srcSize, cLevel);
    size_t frameHeaderSize = ZSTD_frameHeaderSize(dst, ZSTD_FRAMEHEADERSIZE_PREFIX(ZSTD_f_zstd1));
    CONTROL(!ZSTD_isError(frameHeaderSize));
    /* check block is compressible, hence contains a literals section */
    {   blockProperties_t bp;
        ZSTD_getcBlockSize((char*)dst+frameHeaderSize, dstCapacity, &bp);  /* Get 1st block type */
        if (bp.blockType != bt_compressed) {
            DISPLAY("no compressed literals\n");
            return r;
    }   }
    {   size_t const skippedSize = frameHeaderSize + ZSTD_blockHeaderSize;
        prepSize -= skippedSize;
        prepBuffer = malloc(prepSize);
        CONTROL(prepBuffer != NULL);
        memmove(prepBuffer, (char*)dst+skippedSize, prepSize);
    }
    ZSTD_decompressBegin(g_zdc);
    r.prepBuffer = prepBuffer;
    r.prepSize = prepSize;
    r.dst = dst;
    r.dstCapacity = dstCapacity;
    r.fixedOrigSize = srcSize > 128 KB ? 128 KB : srcSize;    /* speed relative to block */
    return r;
}

extern size_t ZSTD_decodeLiteralsBlock_wrapper(ZSTD_DCtx* dctx,
                          const void* src, size_t srcSize,
                          void* dst, size_t dstCapacity);
static size_t
local_ZSTD_decodeLiteralsBlock(const void* src, size_t srcSize, void* dst, size_t dstCapacity, void* unused)
{
    (void)unused;
    return ZSTD_decodeLiteralsBlock_wrapper(g_zdc, src, srcSize, dst, dstCapacity);
}

FORCE_NOINLINE size_t ZSTD_decodeLiteralsHeader(ZSTD_DCtx* dctx, void const* src, size_t srcSize)
{
    RETURN_ERROR_IF(srcSize < MIN_CBLOCK_SIZE, corruption_detected, "");
    {
        BYTE const* istart = (BYTE const*)src;
        SymbolEncodingType_e const litEncType = (SymbolEncodingType_e)(istart[0] & 3);
        if (litEncType == set_compressed) {
            RETURN_ERROR_IF(srcSize < 5, corruption_detected, "srcSize >= MIN_CBLOCK_SIZE == 3; here we need up to 5 for case 3");
            {
                size_t lhSize, litSize, litCSize;
                U32 const lhlCode = (istart[0] >> 2) & 3;
                U32 const lhc = MEM_readLE32(istart);
                int const flags = ZSTD_DCtx_get_bmi2(dctx) ? HUF_flags_bmi2 : 0;
                switch(lhlCode)
                {
                case 0: case 1: default:   /* note : default is impossible, since lhlCode into [0..3] */
                    /* 2 - 2 - 10 - 10 */
                    lhSize = 3;
                    litSize  = (lhc >> 4) & 0x3FF;
                    litCSize = (lhc >> 14) & 0x3FF;
                    break;
                case 2:
                    /* 2 - 2 - 14 - 14 */
                    lhSize = 4;
                    litSize  = (lhc >> 4) & 0x3FFF;
                    litCSize = lhc >> 18;
                    break;
                case 3:
                    /* 2 - 2 - 18 - 18 */
                    lhSize = 5;
                    litSize  = (lhc >> 4) & 0x3FFFF;
                    litCSize = (lhc >> 22) + ((size_t)istart[4] << 10);
                    break;
                }
                RETURN_ERROR_IF(litSize > ZSTD_BLOCKSIZE_MAX, corruption_detected, "");
                RETURN_ERROR_IF(litCSize + lhSize > srcSize, corruption_detected, "");
#ifndef HUF_FORCE_DECOMPRESS_X2
                return HUF_readDTableX1_wksp(
                        dctx->entropy.hufTable,
                        istart+lhSize, litCSize,
                        dctx->workspace, sizeof(dctx->workspace),
                        flags);
#else
                return HUF_readDTableX2_wksp(
                        dctx->entropy.hufTable,
                        istart+lhSize, litCSize,
                        dctx->workspace, sizeof(dctx->workspace), flags);
#endif
            }
        }
    }
    return 0;
}

static size_t
local_ZSTD_decodeLiteralsHeader(const void* src, size_t srcSize, void* dst, size_t dstCapacity, void* unused)
{
    (void)dst; (void)dstCapacity; (void)unused;
    return ZSTD_decodeLiteralsHeader(g_zdc, src, srcSize);
}

static PrepResult prepSequences1stBlock(const void* src, size_t srcSize, int cLevel)
{
    PrepResult r = PREPRESULT_INIT;
    size_t const dstCapacity = srcSize;
    void* dst = malloc(dstCapacity);
    const BYTE* ip = dst;
    const BYTE* iend;
    {   size_t const cSize = ZSTD_compress(dst, dstCapacity, src, srcSize, cLevel);
        CONTROL(cSize > ZSTD_FRAMEHEADERSIZE_PREFIX(ZSTD_f_zstd1));
    }
    /* Skip frame Header */
    {   size_t const frameHeaderSize = ZSTD_frameHeaderSize(dst, ZSTD_FRAMEHEADERSIZE_PREFIX(ZSTD_f_zstd1));
        CONTROL(!ZSTD_isError(frameHeaderSize));
        ip += frameHeaderSize;
    }
    /* Find end of block */
    {   blockProperties_t bp;
        size_t const cBlockSize = ZSTD_getcBlockSize(ip, dstCapacity, &bp);   /* Get 1st block type */
        if (bp.blockType != bt_compressed) {
            DISPLAY("no compressed sequences\n");
            return r;
        }
        iend = ip + ZSTD_blockHeaderSize + cBlockSize;   /* End of first block */
    }
    ip += ZSTD_blockHeaderSize;    /* skip block header */
    ZSTD_decompressBegin(g_zdc);
    CONTROL(iend > ip);
    ip += ZSTD_decodeLiteralsBlock_wrapper(g_zdc, ip, (size_t)(iend-ip), dst, dstCapacity);   /* skip literal segment */
    r.prepSize = (size_t)(iend-ip);
    r.prepBuffer = malloc(r.prepSize);
    CONTROL(r.prepBuffer != NULL);
    memmove(r.prepBuffer, ip, r.prepSize);   /* copy rest of block (it starts by SeqHeader) */
    r.dst = dst;
    r.dstCapacity = dstCapacity;
    r.fixedOrigSize = srcSize > 128 KB ? 128 KB : srcSize;   /* speed relative to block */
    return r;
}

static size_t
local_ZSTD_decodeSeqHeaders(const void* src, size_t srcSize, void* dst, size_t dstCapacity, void* unused)
{
    int nbSeq;
    (void)unused; (void)dst; (void)dstCapacity;
    return ZSTD_decodeSeqHeaders(g_zdc, &nbSeq, src, srcSize);
}

#endif

static ZSTD_CStream* g_cstream= NULL;
static size_t
local_ZSTD_compressStream(const void* src, size_t srcSize,
                          void* dst, size_t dstCapacity,
                          void* payload)
{
    ZSTD_outBuffer buffOut;
    ZSTD_inBuffer buffIn;
    ZSTD_parameters p;
    ZSTD_frameParameters f = {1 /* contentSizeHeader*/, 0, 0};
    p.fParams = f;
    p.cParams = *(ZSTD_compressionParameters*)payload;
    ZSTD_initCStream_advanced(g_cstream, NULL, 0, p, ZSTD_CONTENTSIZE_UNKNOWN);
    buffOut.dst = dst;
    buffOut.size = dstCapacity;
    buffOut.pos = 0;
    buffIn.src = src;
    buffIn.size = srcSize;
    buffIn.pos = 0;
    ZSTD_compressStream(g_cstream, &buffOut, &buffIn);
    ZSTD_endStream(g_cstream, &buffOut);
    return buffOut.pos;
}

static size_t
local_ZSTD_compressStream_freshCCtx(const void* src, size_t srcSize,
                          void* dst, size_t dstCapacity,
                          void* payload)
{
    if (g_cstream != NULL) ZSTD_freeCCtx(g_cstream);
    g_cstream = ZSTD_createCCtx();
    assert(g_cstream != NULL);

    {   size_t const r = local_ZSTD_compressStream(src, srcSize, dst, dstCapacity, payload);
        ZSTD_freeCCtx(g_cstream);
        g_cstream = NULL;
        return r;
    }
}

static size_t
local_ZSTD_compress2(const void* src, size_t srcSize,
                           void* dst, size_t dstCapacity,
                           void* payload)
{
    (void)payload;
    return ZSTD_compress2(g_cstream, dst, dstCapacity, src, srcSize);
}

static size_t
local_ZSTD_compressStream2_end(const void* src, size_t srcSize,
    void* dst, size_t dstCapacity,
    void* payload)
{
    ZSTD_outBuffer buffOut;
    ZSTD_inBuffer buffIn;
    (void)payload;
    buffOut.dst = dst;
    buffOut.size = dstCapacity;
    buffOut.pos = 0;
    buffIn.src = src;
    buffIn.size = srcSize;
    buffIn.pos = 0;
    ZSTD_compressStream2(g_cstream, &buffOut, &buffIn, ZSTD_e_end);
    return buffOut.pos;
}

static size_t
local_ZSTD_compressStream2_continue(const void* src, size_t srcSize,
                                 void* dst, size_t dstCapacity,
                                 void* payload)
{
    ZSTD_outBuffer buffOut;
    ZSTD_inBuffer buffIn;
    (void)payload;
    buffOut.dst = dst;
    buffOut.size = dstCapacity;
    buffOut.pos = 0;
    buffIn.src = src;
    buffIn.size = srcSize;
    buffIn.pos = 0;
    ZSTD_compressStream2(g_cstream, &buffOut, &buffIn, ZSTD_e_continue);
    ZSTD_compressStream2(g_cstream, &buffOut, &buffIn, ZSTD_e_end);
    return buffOut.pos;
}

static size_t
local_ZSTD_compress_generic_T2_end(const void* src, size_t srcSize,
                                   void* dst, size_t dstCapacity,
                                   void* payload)
{
    (void)payload;
    ZSTD_CCtx_setParameter(g_cstream, ZSTD_c_nbWorkers, 2);
    return ZSTD_compress2(g_cstream, dst, dstCapacity, src, srcSize);
}

static size_t
local_ZSTD_compress_generic_T2_continue(const void* src, size_t srcSize,
                                        void* dst, size_t dstCapacity,
                                        void* payload)
{
    ZSTD_outBuffer buffOut;
    ZSTD_inBuffer buffIn;
    (void)payload;
    ZSTD_CCtx_setParameter(g_cstream, ZSTD_c_nbWorkers, 2);
    buffOut.dst = dst;
    buffOut.size = dstCapacity;
    buffOut.pos = 0;
    buffIn.src = src;
    buffIn.size = srcSize;
    buffIn.pos = 0;
    ZSTD_compressStream2(g_cstream, &buffOut, &buffIn, ZSTD_e_continue);
    while(ZSTD_compressStream2(g_cstream, &buffOut, &buffIn, ZSTD_e_end)) {}
    return buffOut.pos;
}

static ZSTD_DStream* g_dstream= NULL;
static size_t
local_ZSTD_decompressStream(const void* src, size_t srcSize,
                            void* dst, size_t dstCapacity,
                            void* unused)
{
    ZSTD_outBuffer buffOut;
    ZSTD_inBuffer buffIn;
    (void)unused;
    ZSTD_initDStream(g_dstream);
    buffOut.dst = dst;
    buffOut.size = dstCapacity;
    buffOut.pos = 0;
    buffIn.src = src;
    buffIn.size = srcSize;
    buffIn.pos = 0;
    ZSTD_decompressStream(g_dstream, &buffOut, &buffIn);
    return buffOut.pos;
}

static size_t local_ZSTD_compressContinue(const void* src, size_t srcSize,
                                          void* dst, size_t dstCapacity,
                                          void* payload)
{
    ZSTD_parameters p;
    ZSTD_frameParameters f = { 1 /* contentSizeHeader*/, 0, 0 };
    p.fParams = f;
    p.cParams = *(ZSTD_compressionParameters*)payload;
    ZSTD_compressBegin_advanced(g_zcc, NULL, 0, p, srcSize);
    return ZSTD_compressEnd(g_zcc, dst, dstCapacity, src, srcSize);
}

#define FIRST_BLOCK_SIZE 8
static size_t
local_ZSTD_compressContinue_extDict(const void* src, size_t srcSize,
                                    void* dst, size_t dstCapacity,
                                    void* payload)
{
    BYTE firstBlockBuf[FIRST_BLOCK_SIZE];

    ZSTD_parameters p;
    ZSTD_frameParameters const f = { 1, 0, 0 };
    p.fParams = f;
    p.cParams = *(ZSTD_compressionParameters*)payload;
    ZSTD_compressBegin_advanced(g_zcc, NULL, 0, p, srcSize);
    memcpy(firstBlockBuf, src, FIRST_BLOCK_SIZE);

    {   size_t const compressResult = ZSTD_compressContinue(g_zcc,
                                            dst, dstCapacity,
                                            firstBlockBuf, FIRST_BLOCK_SIZE);
        if (ZSTD_isError(compressResult)) {
            DISPLAY("local_ZSTD_compressContinue_extDict error : %s\n",
                    ZSTD_getErrorName(compressResult));
            return compressResult;
        }
        dst = (BYTE*)dst + compressResult;
        dstCapacity -= compressResult;
    }
    return ZSTD_compressEnd(g_zcc, dst, dstCapacity,
                            (const BYTE*)src + FIRST_BLOCK_SIZE,
                            srcSize - FIRST_BLOCK_SIZE);
}

static size_t local_ZSTD_decompressContinue(const void* src, size_t srcSize,
                                            void* dst, size_t dstCapacity,
                                            void* unused)
{
    size_t regeneratedSize = 0;
    const BYTE* ip = (const BYTE*)src;
    const BYTE* const iend = ip + srcSize;
    BYTE* op = (BYTE*)dst;
    size_t remainingCapacity = dstCapacity;

    (void)unused;
    ZSTD_decompressBegin(g_zdc);
    while (ip < iend) {
        size_t const iSize = ZSTD_nextSrcSizeToDecompress(g_zdc);
        size_t const decodedSize = ZSTD_decompressContinue(g_zdc, op, remainingCapacity, ip, iSize);
        ip += iSize;
        regeneratedSize += decodedSize;
        op += decodedSize;
        remainingCapacity -= decodedSize;
    }

    return regeneratedSize;
}

static PrepResult prepSequences(const void* src, size_t srcSize, int cLevel)
{
    PrepResult r = PREPRESULT_INIT;
    size_t const dstCapacity = ZSTD_compressBound(srcSize);
    void* const dst = malloc(dstCapacity);
    size_t const prepCapacity = dstCapacity * 4;
    void* prepBuffer = malloc(prepCapacity);
    void* sequencesStart = (char*)prepBuffer + 2*sizeof(unsigned);
    ZSTD_Sequence* const seqs = sequencesStart;
    size_t const seqsCapacity = prepCapacity / sizeof(ZSTD_Sequence);
    size_t nbSeqs;
    ZSTD_CCtx_reset(g_zcc, ZSTD_reset_session_and_parameters);
    ZSTD_CCtx_setParameter(g_zcc, ZSTD_c_compressionLevel, cLevel);
    nbSeqs = ZSTD_generateSequences(g_zcc, seqs, seqsCapacity, src, srcSize);
    CONTROL(srcSize < UINT_MAX);
    MEM_write32(prepBuffer, (U32)srcSize);
    MEM_write32((char*)prepBuffer+4, (U32)nbSeqs);
    memcpy(seqs + nbSeqs, src, srcSize);
    r.prepBuffer = prepBuffer;
    r.prepSize = 8 + sizeof(ZSTD_Sequence)*nbSeqs + srcSize;
    r.dst = dst;
    r.dstCapacity = dstCapacity;
    return r;
}

static size_t local_compressSequences(const void* input, size_t inputSize,
                                      void* dst, size_t dstCapacity,
                                      void* payload)
{
    const char* ip = input;
    size_t srcSize = MEM_read32(ip);
    size_t nbSeqs = MEM_read32(ip+=4);
    const ZSTD_Sequence* seqs = (const ZSTD_Sequence*)(const void*)(ip+=4);
    const void* src = (ip+=nbSeqs * sizeof(ZSTD_Sequence));
    ZSTD_CCtx_reset(g_zcc, ZSTD_reset_session_and_parameters);
    ZSTD_CCtx_setParameter(g_zcc, ZSTD_c_blockDelimiters, ZSTD_sf_explicitBlockDelimiters);
    assert(8 + nbSeqs * sizeof(ZSTD_Sequence) + srcSize == inputSize); (void)inputSize;
    (void)payload;

    return ZSTD_compressSequences(g_zcc, dst, dstCapacity, seqs, nbSeqs, src, srcSize);
}

static PrepResult prepSequencesAndLiterals(const void* src, size_t srcSize, int cLevel)
{
    PrepResult r = PREPRESULT_INIT;
    size_t const dstCapacity = ZSTD_compressBound(srcSize);
    void* const dst = malloc(dstCapacity);
    size_t const prepCapacity = dstCapacity * 4;
    void* prepBuffer = malloc(prepCapacity);
    void* sequencesStart = (char*)prepBuffer + 3*sizeof(unsigned);
    ZSTD_Sequence* const seqs = sequencesStart;
    size_t const seqsCapacity = prepCapacity / sizeof(ZSTD_Sequence);
    size_t nbSeqs;
    ZSTD_CCtx_reset(g_zcc, ZSTD_reset_session_and_parameters);
    ZSTD_CCtx_setParameter(g_zcc, ZSTD_c_compressionLevel, cLevel);
    nbSeqs = ZSTD_generateSequences(g_zcc, seqs, seqsCapacity, src, srcSize);
    CONTROL(srcSize < UINT_MAX);
    MEM_write32(prepBuffer, (U32)srcSize);
    MEM_write32((char*)prepBuffer+4, (U32)nbSeqs);
    /* copy literals */
    {   char* const litStart = (char*)(seqs + nbSeqs);
        size_t nbLiterals = 0;
        const char* ip = src;
        size_t n;
        for (n=0; n<nbSeqs; n++) {
            size_t const litSize = seqs[n].litLength;
            memcpy(litStart + nbLiterals, ip, litSize);
            ip += litSize + seqs[n].matchLength;
            nbLiterals += litSize;
        }
        MEM_write32((char*)prepBuffer+8, (U32)nbLiterals);
        r.prepBuffer = prepBuffer;
        r.prepSize = 12 + sizeof(ZSTD_Sequence)*nbSeqs + nbLiterals;
        r.dst = dst;
        r.dstCapacity = dstCapacity;
    }
    return r;
}

static size_t
local_compressSequencesAndLiterals(const void* input, size_t inputSize,
                                    void* dst, size_t dstCapacity,
                                    void* payload)
{
    const char* ip = input;
    size_t decompressedSize = MEM_read32(ip);
    size_t nbSeqs = MEM_read32(ip+=4);
    size_t nbLiterals = MEM_read32(ip+=4);
    const ZSTD_Sequence* seqs = (const ZSTD_Sequence*)(const void*)(ip+=4);
    const void* literals = (ip+=nbSeqs * sizeof(ZSTD_Sequence));
    ZSTD_CCtx_reset(g_zcc, ZSTD_reset_session_and_parameters);
    ZSTD_CCtx_setParameter(g_zcc, ZSTD_c_blockDelimiters, ZSTD_sf_explicitBlockDelimiters);
# if 0 /* for tests */
    ZSTD_CCtx_setParameter(g_zcc, ZSTD_c_repcodeResolution, ZSTD_ps_enable);
#endif
    assert(12 + nbSeqs * sizeof(ZSTD_Sequence) + nbLiterals == inputSize); (void)inputSize;
    (void)payload;

    return ZSTD_compressSequencesAndLiterals(g_zcc, dst, dstCapacity, seqs, nbSeqs, literals, nbLiterals, nbLiterals + 8, decompressedSize);
}

static PrepResult prepConvertSequences(const void* src, size_t srcSize, int cLevel)
{
    PrepResult r = PREPRESULT_INIT;
    size_t const prepCapacity = srcSize * 4;
    void* prepBuffer = malloc(prepCapacity);
    void* sequencesStart = (char*)prepBuffer + 2*sizeof(unsigned);
    ZSTD_Sequence* const seqs = sequencesStart;
    size_t const seqsCapacity = prepCapacity / sizeof(ZSTD_Sequence);
    size_t totalNbSeqs, nbSeqs, blockSize=0;
    ZSTD_CCtx_reset(g_zcc, ZSTD_reset_session_and_parameters);
    ZSTD_CCtx_setParameter(g_zcc, ZSTD_c_compressionLevel, cLevel);
    totalNbSeqs = ZSTD_generateSequences(g_zcc, seqs, seqsCapacity, src, srcSize);
    CONTROL(!ZSTD_isError(totalNbSeqs));
    /* find nb sequences in first block */
    {   size_t n;
        for (n=0; n<totalNbSeqs; n++) {
            if (seqs[n].matchLength == 0) break;
            blockSize += seqs[n].litLength + seqs[n].matchLength;
        }
        blockSize += seqs[n].litLength;
        nbSeqs = n+1;
#if 0
        printf("found %zu sequences representing a first block of size %zu\n", nbSeqs, blockSize);
#endif
    }
    /* generate benchmarked input */
    CONTROL(blockSize < UINT_MAX);
    MEM_write32(prepBuffer, (U32)blockSize);
    MEM_write32((char*)prepBuffer+4, (U32)nbSeqs);
    memcpy(seqs + nbSeqs, src, srcSize);
    r.prepBuffer = prepBuffer;
    r.prepSize = 8 + sizeof(ZSTD_Sequence) * nbSeqs;
    r.fixedOrigSize = blockSize;
    return r;
}

static size_t
local_convertSequences(const void* input, size_t inputSize,
                       void* dst, size_t dstCapacity,
                       void* payload)
{
    const char* ip = input;
    size_t const blockSize = MEM_read32(ip);
    size_t const nbSeqs = MEM_read32(ip+=4);
    const ZSTD_Sequence* seqs = (const ZSTD_Sequence*)(const void*)(ip+=4);
    ZSTD_CCtx_reset(g_zcc, ZSTD_reset_session_and_parameters);
    ZSTD_resetSeqStore(&g_zcc->seqStore);
    ZSTD_CCtx_setParameter(g_zcc, ZSTD_c_blockDelimiters, ZSTD_sf_explicitBlockDelimiters);
# if 0 /* for tests */
    ZSTD_CCtx_setParameter(g_zcc, ZSTD_c_repcodeResolution, ZSTD_ps_enable);
#endif
    assert(8 + nbSeqs * sizeof(ZSTD_Sequence) == inputSize); (void)inputSize;
    (void)dst; (void)dstCapacity;
    (void)payload; (void)blockSize;

    (void)ZSTD_convertBlockSequences(g_zcc, seqs, nbSeqs, 0);
    return nbSeqs;
}

static size_t
check_compressedSequences(const void* compressed, size_t cSize, const void* orig, size_t origSize)
{
    size_t decSize;
    int diff;
    void* decompressed = malloc(origSize);
    if (decompressed == NULL) return 2;

    decSize = ZSTD_decompress(decompressed, origSize, compressed, cSize);
    if (decSize != origSize) { free(decompressed); DISPLAY("ZSTD_decompress failed (%u) ", (unsigned)decSize); return 1; }

    diff = memcmp(decompressed, orig, origSize);
    if (diff) { free(decompressed); return 1; }

    free(decompressed);
    return 0;
}

static size_t
local_get1BlockSummary(const void* input, size_t inputSize,
                       void* dst, size_t dstCapacity,
                       void* payload)
{
    const char* ip = input;
    size_t const blockSize = MEM_read32(ip);
    size_t const nbSeqs = MEM_read32(ip+=4);
    const ZSTD_Sequence* seqs = (const ZSTD_Sequence*)(const void*)(ip+=4);
    ZSTD_CCtx_reset(g_zcc, ZSTD_reset_session_and_parameters);
    ZSTD_resetSeqStore(&g_zcc->seqStore);
    ZSTD_CCtx_setParameter(g_zcc, ZSTD_c_blockDelimiters, ZSTD_sf_explicitBlockDelimiters);
    assert(8 + nbSeqs * sizeof(ZSTD_Sequence) == inputSize); (void)inputSize;
    (void)dst; (void)dstCapacity;
    (void)payload; (void)blockSize;

    (void)ZSTD_get1BlockSummary(seqs, nbSeqs);
    return nbSeqs;
}

static PrepResult prepCopy(const void* src, size_t srcSize, int cLevel)
{
    PrepResult r = PREPRESULT_INIT;
    (void)cLevel;
    r.prepSize = srcSize;
    r.prepBuffer = malloc(srcSize);
    CONTROL(r.prepBuffer != NULL);
    memcpy(r.prepBuffer, src, srcSize);
    r.dstCapacity = ZSTD_compressBound(srcSize);
    r.dst = malloc(r.dstCapacity);
    CONTROL(r.dst != NULL);
    return r;
}

static PrepResult prepShorterDstCapacity(const void* src, size_t srcSize, int cLevel)
{
    PrepResult r = prepCopy(src, srcSize, cLevel);
    assert(r.dstCapacity > 1);
    r.dstCapacity -= 1;
    return r;
}

/*_*******************************************************
*  List of Scenarios
*********************************************************/

/* if PrepFunction_f returns PrepResult.prepBuffSize == 0, benchmarking is cancelled */
typedef PrepResult (*PrepFunction_f)(const void* src, size_t srcSize, int cLevel);
typedef size_t (*BenchedFunction_f)(const void* src, size_t srcSize, void* dst, size_t dstSize, void* opaque);
/* must return 0, otherwise verification is considered failed */
typedef size_t (*VerifFunction_f)(const void* processed, size_t procSize, const void* input, size_t inputSize);

typedef struct {
    const char* name;
    PrepFunction_f preparation_f;
    BenchedFunction_f benched_f;
    VerifFunction_f verif_f; /* optional */
} BenchScenario;

static BenchScenario kScenarios[] = {
    { "compress", NULL, local_ZSTD_compress, check_compressedSequences },
    { "decompress", prepDecompress, local_ZSTD_decompress, NULL },
    { "compress_freshCCtx", NULL, local_ZSTD_compress_freshCCtx, check_compressedSequences },
    { "decompressDCtx", prepDecompress, local_ZSTD_decompressDCtx, NULL },
    { "compressContinue", NULL, local_ZSTD_compressContinue, check_compressedSequences },
    { "compressContinue_extDict", NULL, local_ZSTD_compressContinue_extDict, NULL },
    { "decompressContinue", prepDecompress, local_ZSTD_decompressContinue, NULL },
    { "compressStream", NULL, local_ZSTD_compressStream, check_compressedSequences },
    { "compressStream_freshCCtx", NULL, local_ZSTD_compressStream_freshCCtx, check_compressedSequences },
    { "decompressStream", prepDecompress, local_ZSTD_decompressStream, NULL },
    { "compress2", NULL, local_ZSTD_compress2, check_compressedSequences },
    { "compressStream2, end", NULL, local_ZSTD_compressStream2_end, check_compressedSequences },
    { "compressStream2, end & short", prepShorterDstCapacity, local_ZSTD_compressStream2_end, check_compressedSequences },
    { "compressStream2, continue", NULL, local_ZSTD_compressStream2_continue, check_compressedSequences },
    { "compressStream2, -T2, continue", NULL, local_ZSTD_compress_generic_T2_continue, check_compressedSequences },
    { "compressStream2, -T2, end", NULL, local_ZSTD_compress_generic_T2_end, check_compressedSequences },
    { "compressSequences", prepSequences, local_compressSequences, check_compressedSequences },
    { "compressSequencesAndLiterals", prepSequencesAndLiterals, local_compressSequencesAndLiterals, check_compressedSequences },
    { "convertSequences (1st block)", prepConvertSequences, local_convertSequences, NULL },
    { "get1BlockSummary (1st block)", prepConvertSequences, local_get1BlockSummary, NULL },
#ifndef ZSTD_DLL_IMPORT
    { "decodeLiteralsHeader (1st block)", prepLiterals, local_ZSTD_decodeLiteralsHeader, NULL },
    { "decodeLiteralsBlock (1st block)", prepLiterals, local_ZSTD_decodeLiteralsBlock, NULL },
    { "decodeSeqHeaders (1st block)", prepSequences1stBlock, local_ZSTD_decodeSeqHeaders, NULL },
#endif
};
#define NB_SCENARIOS (sizeof(kScenarios) / sizeof(kScenarios[0]))

/*_*******************************************************
*  Bench loop
*********************************************************/
static int benchMem(unsigned scenarioID,
                    const void* origSrc, size_t origSrcSize,
                    int cLevel, ZSTD_compressionParameters cparams)
{
    size_t dstCapacity = 0;
    void*  dst = NULL;
    void*  prepBuff = NULL;
    size_t prepBuffSize = 0;
    void*  payload;
    const char* benchName;
    BMK_benchFn_t benchFunction;
    PrepFunction_f prep_f;
    VerifFunction_f verif_f;
    int errorcode = 0;

    if (scenarioID >= NB_SCENARIOS) return 0; /* scenario doesn't exist */

    benchName = kScenarios[scenarioID].name;
    benchFunction = kScenarios[scenarioID].benched_f;
    prep_f = kScenarios[scenarioID].preparation_f;
    verif_f = kScenarios[scenarioID].verif_f;
    if (prep_f == NULL) prep_f = prepCopy; /* default */

    /* Initialization */
    if (g_zcc==NULL) g_zcc = ZSTD_createCCtx();
    if (g_zdc==NULL) g_zdc = ZSTD_createDCtx();
    if (g_cstream==NULL) g_cstream = ZSTD_createCStream();
    if (g_dstream==NULL) g_dstream = ZSTD_createDStream();

    /* DISPLAY("params: cLevel %d, wlog %d hlog %d clog %d slog %d mml %d tlen %d strat %d \n",
          cLevel, cparams->windowLog, cparams->hashLog, cparams->chainLog, cparams->searchLog,
          cparams->minMatch, cparams->targetLength, cparams->strategy); */

    ZSTD_CCtx_setParameter(g_zcc, ZSTD_c_compressionLevel, cLevel);
    ZSTD_CCtx_setParameter(g_zcc, ZSTD_c_windowLog, (int)cparams.windowLog);
    ZSTD_CCtx_setParameter(g_zcc, ZSTD_c_hashLog, (int)cparams.hashLog);
    ZSTD_CCtx_setParameter(g_zcc, ZSTD_c_chainLog, (int)cparams.chainLog);
    ZSTD_CCtx_setParameter(g_zcc, ZSTD_c_searchLog, (int)cparams.searchLog);
    ZSTD_CCtx_setParameter(g_zcc, ZSTD_c_minMatch, (int)cparams.minMatch);
    ZSTD_CCtx_setParameter(g_zcc, ZSTD_c_targetLength, (int)cparams.targetLength);
    ZSTD_CCtx_setParameter(g_zcc, ZSTD_c_strategy, (int)cparams.strategy);

    ZSTD_CCtx_setParameter(g_cstream, ZSTD_c_compressionLevel, cLevel);
    ZSTD_CCtx_setParameter(g_cstream, ZSTD_c_windowLog, (int)cparams.windowLog);
    ZSTD_CCtx_setParameter(g_cstream, ZSTD_c_hashLog, (int)cparams.hashLog);
    ZSTD_CCtx_setParameter(g_cstream, ZSTD_c_chainLog, (int)cparams.chainLog);
    ZSTD_CCtx_setParameter(g_cstream, ZSTD_c_searchLog, (int)cparams.searchLog);
    ZSTD_CCtx_setParameter(g_cstream, ZSTD_c_minMatch, (int)cparams.minMatch);
    ZSTD_CCtx_setParameter(g_cstream, ZSTD_c_targetLength, (int)cparams.targetLength);
    ZSTD_CCtx_setParameter(g_cstream, ZSTD_c_strategy, (int)cparams.strategy);

    /* Preparation */
    payload = &cparams;
    {   PrepResult pr = prep_f(origSrc, origSrcSize, cLevel);
        dst = pr.dst;
        dstCapacity = pr.dstCapacity;
        prepBuff = pr.prepBuffer;
        prepBuffSize = pr.prepSize;
        if (pr.fixedOrigSize) origSrcSize = pr.fixedOrigSize;
    }
    if (prepBuffSize==0) goto _cleanOut; /* failed preparation */

     /* warming up dstBuff */
    { size_t i; for (i=0; i<dstCapacity; i++) ((BYTE*)dst)[i]=(BYTE)i; }

    /* benchmark loop */
    {   BMK_timedFnState_t* const tfs = BMK_createTimedFnState(g_nbIterations * 1000, 1000);
        void* const avoidStrictAliasingPtr = &dst;
        const void* prepSrc = prepBuff;
        BMK_benchParams_t bp;
        BMK_runTime_t bestResult;
        bestResult.sumOfReturn = 0;
        bestResult.nanoSecPerRun = (double)TIMELOOP_NANOSEC * 2000000000;  /* hopefully large enough : must be larger than any potential measurement */
        CONTROL(tfs != NULL);

        bp.benchFn = benchFunction;
        bp.benchPayload = payload;
        bp.initFn = NULL;
        bp.initPayload = NULL;
        bp.errorFn = ZSTD_isError;
        bp.blockCount = 1;
        bp.srcBuffers = &prepSrc;
        bp.srcSizes = &prepBuffSize;
        bp.dstBuffers = (void* const*) avoidStrictAliasingPtr;  /* circumvent strict aliasing warning on gcc-8,
                                                                 * because gcc considers that `void* const *`  and `void**` are 2 different types */
        bp.dstCapacities = &dstCapacity;
        bp.blockResults = NULL;

        for (;;) {
            BMK_runOutcome_t const bOutcome = BMK_benchTimedFn(tfs, bp);

            if (!BMK_isSuccessful_runOutcome(bOutcome)) {
                DISPLAY("ERROR: Scenario %u: %s \n", scenarioID, ZSTD_getErrorName(BMK_extract_errorResult(bOutcome)));
                errorcode = 1;
                BMK_freeTimedFnState(tfs);
                goto _cleanOut;
            }

            {   BMK_runTime_t const newResult = BMK_extract_runTime(bOutcome);
                if (newResult.nanoSecPerRun < bestResult.nanoSecPerRun )
                    bestResult.nanoSecPerRun = newResult.nanoSecPerRun;
                DISPLAY("\r%2u#%-31.31s:%8.1f MB/s  (%8u) ",
                        scenarioID, benchName,
                        (double)origSrcSize * TIMELOOP_NANOSEC / bestResult.nanoSecPerRun / MB_UNIT,
                        (unsigned)newResult.sumOfReturn );

                if (verif_f) {
                    size_t const vRes = verif_f(dst, newResult.sumOfReturn, origSrc, origSrcSize);
                    if (vRes) {
                        DISPLAY(" validation failed ! (%u)\n", (unsigned)vRes);
                        break;
                    }
                }
            }

            if ( BMK_isCompleted_TimedFn(tfs) ) break;
        }
        BMK_freeTimedFnState(tfs);
    }
    DISPLAY("\n");

_cleanOut:
    free(prepBuff);
    free(dst);
    ZSTD_freeCCtx(g_zcc); g_zcc=NULL;
    ZSTD_freeDCtx(g_zdc); g_zdc=NULL;
    ZSTD_freeCStream(g_cstream); g_cstream=NULL;
    ZSTD_freeDStream(g_dstream); g_dstream=NULL;
    return errorcode;
}


#define BENCH_ALL_SCENARIOS 999
/*
 * if @compressibility < 0.0, use Lorem Ipsum generator
 * otherwise, @compressibility is expected to be between 0.0 and 1.0
 * if scenarioID == BENCH_ALL_SCENARIOS, all scenarios will be run on the sample
*/
static int benchSample(U32 scenarioID,
                       size_t benchedSize, double compressibility,
                       int cLevel, ZSTD_compressionParameters cparams)
{
    /* Allocation */
    void* const origBuff = malloc(benchedSize);
    if (!origBuff) { DISPLAY("\nError: not enough memory!\n"); return 12; }

    /* Fill buffer */
    if (compressibility < 0.0) {
        LOREM_genBuffer(origBuff, benchedSize, 0);
    } else {
        RDG_genBuffer(origBuff, benchedSize, compressibility, 0.0, 0);

    }

    /* bench */
    DISPLAY("\r%70s\r", "");
    DISPLAY(" Sample %u bytes : \n", (unsigned)benchedSize);
    if (scenarioID == BENCH_ALL_SCENARIOS) {
        for (scenarioID=0; scenarioID<100; scenarioID++) {
            benchMem(scenarioID, origBuff, benchedSize, cLevel, cparams);
        }
    } else {
        benchMem(scenarioID, origBuff, benchedSize, cLevel, cparams);
    }

    free(origBuff);
    return 0;
}


static int benchFiles(U32 scenarioID,
                      const char** fileNamesTable, const int nbFiles,
                      int cLevel, ZSTD_compressionParameters cparams)
{
    /* Loop for each file */
    int fileIdx;
    for (fileIdx=0; fileIdx<nbFiles; fileIdx++) {
        const char* const inFileName = fileNamesTable[fileIdx];
        FILE* const inFile = fopen( inFileName, "rb" );
        size_t benchedSize;

        /* Check file existence */
        if (inFile==NULL) { DISPLAY( "Pb opening %s\n", inFileName); return 11; }

        /* Memory allocation & restrictions */
        {   U64 const inFileSize = UTIL_getFileSize(inFileName);
            if (inFileSize == UTIL_FILESIZE_UNKNOWN) {
                DISPLAY( "Cannot measure size of %s\n", inFileName);
                fclose(inFile);
                return 11;
            }
            benchedSize = BMK_findMaxMem(inFileSize*3) / 3;
            if ((U64)benchedSize > inFileSize)
                benchedSize = (size_t)inFileSize;
            if ((U64)benchedSize < inFileSize) {
                DISPLAY("Not enough memory for '%s' full size; testing %u MB only... \n",
                        inFileName, (unsigned)(benchedSize>>20));
        }   }

        /* Alloc */
        {   void* const origBuff = malloc(benchedSize);
            if (!origBuff) { DISPLAY("\nError: not enough memory!\n"); fclose(inFile); return 12; }

            /* Fill input buffer */
            DISPLAY("Loading %s...       \r", inFileName);
            {   size_t const readSize = fread(origBuff, 1, benchedSize, inFile);
                fclose(inFile);
                if (readSize != benchedSize) {
                    DISPLAY("\nError: problem reading file '%s' !!    \n", inFileName);
                    free(origBuff);
                    return 13;
            }   }

            /* bench */
            DISPLAY("\r%70s\r", "");   /* blank line */
            DISPLAY(" %s : \n", inFileName);
            if (scenarioID == BENCH_ALL_SCENARIOS) {
                for (scenarioID=0; scenarioID<100; scenarioID++) {
                    benchMem(scenarioID, origBuff, benchedSize, cLevel, cparams);
                }
            } else {
                benchMem(scenarioID, origBuff, benchedSize, cLevel, cparams);
            }

            free(origBuff);
    }   }

    return 0;
}



/*_*******************************************************
*  Argument Parsing
*********************************************************/

#define ERROR_OUT(msg) { DISPLAY("%s \n", msg); exit(1); }

static unsigned readU32FromChar(const char** stringPtr)
{
    const char errorMsg[] = "error: numeric value too large";
    unsigned result = 0;
    while ((**stringPtr >='0') && (**stringPtr <='9')) {
        unsigned const max = (((unsigned)(-1)) / 10) - 1;
        if (result > max) ERROR_OUT(errorMsg);
        result *= 10;
        result += (unsigned)(**stringPtr - '0');
        (*stringPtr)++ ;
    }
    if ((**stringPtr=='K') || (**stringPtr=='M')) {
        unsigned const maxK = ((unsigned)(-1)) >> 10;
        if (result > maxK) ERROR_OUT(errorMsg);
        result <<= 10;
        if (**stringPtr=='M') {
            if (result > maxK) ERROR_OUT(errorMsg);
            result <<= 10;
        }
        (*stringPtr)++;  /* skip `K` or `M` */
        if (**stringPtr=='i') (*stringPtr)++;
        if (**stringPtr=='B') (*stringPtr)++;
    }
    return result;
}

static int longCommandWArg(const char** stringPtr, const char* longCommand)
{
    size_t const comSize = strlen(longCommand);
    int const result = !strncmp(*stringPtr, longCommand, comSize);
    if (result) *stringPtr += comSize;
    return result;
}


/*_*******************************************************
*  Command line
*********************************************************/

static int usage(const char* exename)
{
    DISPLAY( "Usage :\n");
    DISPLAY( "      %s [arg] file1 file2 ... fileX\n", exename);
    DISPLAY( "Arguments :\n");
    DISPLAY( " -H/-h  : Help (this text + advanced options)\n");
    return 0;
}

static int usage_advanced(const char* exename)
{
    usage(exename);
    DISPLAY( "\nAdvanced options :\n");
    DISPLAY( " -b#    : test only function # \n");
    DISPLAY( " -l#    : benchmark functions at that compression level (default : %i)\n", DEFAULT_CLEVEL);
    DISPLAY( "--zstd= : custom parameter selection. Format same as zstdcli \n");
    DISPLAY( " -P#    : sample compressibility (default : %.1f%%)\n", COMPRESSIBILITY_DEFAULT * 100);
    DISPLAY( " -B#    : sample size (default : %u)\n", (unsigned)kSampleSizeDefault);
    DISPLAY( " -i#    : iteration loops [1-9](default : %i)\n", NBLOOPS);
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
    int argNb, filenamesStart=0, result;
    const char* const exename = argv[0];
    const char* input_filename = NULL;
    U32 scenarioID = BENCH_ALL_SCENARIOS, main_pause = 0;
    int cLevel = DEFAULT_CLEVEL;
    ZSTD_compressionParameters cparams = ZSTD_getCParams(cLevel, 0, 0);
    size_t sampleSize = kSampleSizeDefault;
    double compressibility = COMPRESSIBILITY_DEFAULT;

    DISPLAY(WELCOME_MESSAGE);
    if (argc<1) return badusage(exename);

    for (argNb=1; argNb<argc; argNb++) {
        const char* argument = argv[argNb];
        CONTROL(argument != NULL);

        if (longCommandWArg(&argument, "--zstd=")) {
            for ( ; ;) {
                if (longCommandWArg(&argument, "windowLog=") || longCommandWArg(&argument, "wlog=")) { cparams.windowLog = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "chainLog=") || longCommandWArg(&argument, "clog=")) { cparams.chainLog = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "hashLog=") || longCommandWArg(&argument, "hlog=")) { cparams.hashLog = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "searchLog=") || longCommandWArg(&argument, "slog=")) { cparams.searchLog = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "minMatch=") || longCommandWArg(&argument, "mml=")) { cparams.minMatch = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "targetLength=") || longCommandWArg(&argument, "tlen=")) { cparams.targetLength = readU32FromChar(&argument); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "strategy=") || longCommandWArg(&argument, "strat=")) { cparams.strategy = (ZSTD_strategy)(readU32FromChar(&argument)); if (argument[0]==',') { argument++; continue; } else break; }
                if (longCommandWArg(&argument, "level=") || longCommandWArg(&argument, "lvl=")) { cLevel = (int)readU32FromChar(&argument); cparams = ZSTD_getCParams(cLevel, 0, 0); if (argument[0]==',') { argument++; continue; } else break; }
                DISPLAY("invalid compression parameter \n");
                return 1;
            }

            /* check end of string */
            if (argument[0] != 0) {
                DISPLAY("invalid --zstd= format \n");
                return 1;
            } else {
                continue;
            }

        } else if (argument[0]=='-') { /* Commands (note : aggregated commands are allowed) */
            argument++;
            while (argument[0]!=0) {

                switch(argument[0])
                {
                    /* Display help on usage */
                case 'h':
                case 'H': return usage_advanced(exename);

                    /* Pause at the end (hidden option) */
                case 'p': main_pause = 1; break;

                    /* Select specific algorithm to bench */
                case 'b':
                    argument++;
                    scenarioID = readU32FromChar(&argument);
                    break;

                    /* Select compression level to use */
                case 'l':
                    argument++;
                    cLevel = (int)readU32FromChar(&argument);
                    cparams = ZSTD_getCParams(cLevel, 0, 0);
                    break;

                    /* Select compressibility of synthetic sample */
                case 'P':
                    argument++;
                    compressibility = (double)readU32FromChar(&argument) / 100.;
                    break;

                    /* Select size of synthetic sample */
                case 'B':
                    argument++;
                    sampleSize = (size_t)readU32FromChar(&argument);
                    break;

                    /* Modify Nb Iterations */
                case 'i':
                    argument++;
                    g_nbIterations = readU32FromChar(&argument);
                    break;

                    /* Unknown command */
                default : return badusage(exename);
                }
            }
            continue;
        }

        /* first provided filename is input */
        if (!input_filename) { input_filename=argument; filenamesStart=argNb; continue; }
    }



    if (filenamesStart==0)   /* no input file */
        result = benchSample(scenarioID, sampleSize, compressibility, cLevel, cparams);
    else
        result = benchFiles(scenarioID, argv+filenamesStart, argc-filenamesStart, cLevel, cparams);

    if (main_pause) { int unused; printf("press enter...\n"); unused = getchar(); (void)unused; }

    return result;
}
