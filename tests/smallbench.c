/*
 * Copyright (c) 2015-2020, Yann Collet, Facebook, Inc.
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
#include "util.h"        /* Compiler options, UTIL_GetFileSize */
#include <stdlib.h>      /* malloc */
#include <stdio.h>       /* fprintf, fopen, ftello64 */
#include <assert.h>

#include "mem.h"         /* U32 */
#include "zstd_internal.h"   /* ZSTD_decodeSeqHeaders, ZSTD_blockHeaderSize, ZSTD_getcBlockSize, blockType_e, KB, MB */
#include "decompress/zstd_decompress_internal.h"   /* ZSTD_DCtx internals */
#define ZSTD_STATIC_LINKING_ONLY  /* ZSTD_compressBegin, ZSTD_compressContinue, etc. */
#include "zstd.h"        /* ZSTD_versionString */
#include "util.h"        /* time functions */
#include "timefn.h"        /* time functions */
#include "datagen.h"
#include "benchfn.h"     /* CustomBench */
#include "benchzstd.h"   /* MB_UNIT */


/*_************************************
*  Constants
**************************************/
#define PROGRAM_DESCRIPTION "Zstandard small blocks benchmark"
#define AUTHOR "Nick Terrell"
#define WELCOME_MESSAGE "*** %s %s %i-bits, by %s (%s) ***\n", PROGRAM_DESCRIPTION, ZSTD_versionString(), (int)(sizeof(void*)*8), AUTHOR, __DATE__

#define NBLOOPS    6
#define TIMELOOP_S 2

#define MAX_MEM    (1984 MB)

#define DEFAULT_CLEVEL 1

#define COMPRESSIBILITY_DEFAULT 0.50
static const size_t kSampleSizeDefault = 10000000;

#define TIMELOOP_NANOSEC      (1*1000000000ULL) /* 1 second */


/*_************************************
*  Macros
**************************************/
#define DISPLAY(...)  fprintf(stderr, __VA_ARGS__)

#define CONTROL(c)  { if (!(c)) { DISPLAY("%s:%d:%s: CONTROL failed: %s \n", __FILE__, __LINE__, __func__, #c); abort(); } }   /* like assert(), but cannot be disabled */

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
typedef struct {
    BYTE const* begin;
    BYTE const* end;
    size_t uncompressedSize;
} block_t;

typedef struct {
    size_t numBlocks;
    block_t blocks[];
} blocks_t;

static size_t block_getSize(block_t block) {
    return (size_t)(block.end - block.begin);
}

static size_t compressBlockBound(size_t srcSize, size_t blockSize)
{
    size_t const blockBound = ZSTD_compressBound(blockSize);
    return blockBound * (srcSize + blockSize - 1) / blockSize;
}

static blocks_t* compressBlocks(ZSTD_CCtx* cctx, void* dst, size_t dstSize, void const* src, size_t srcSize, size_t blockSize)
{
    uint8_t* op = (uint8_t*)dst;
    uint8_t* const oend = op + dstSize;
    uint8_t const* ip = (uint8_t const*)src;
    uint8_t const* const iend = ip + srcSize;
    size_t const numBlocks = (srcSize + blockSize - 1) / blockSize;
    blocks_t* const blocks = (blocks_t*)malloc(sizeof(blocks_t) + numBlocks * sizeof(block_t));
    CONTROL(blocks != NULL);

    blocks->numBlocks = numBlocks;
    for (size_t i = 0; i < numBlocks; ++i) {
        size_t const isize = MIN(blockSize, (size_t)(iend - ip));
        size_t const cBlockSize = ZSTD_compress2(cctx, op, (size_t)(oend - op), ip, isize);
        CONTROL(!ZSTD_isError(cBlockSize));
        CONTROL(isize > 0);
        blocks->blocks[i].begin = op;
        blocks->blocks[i].end = op + cBlockSize;
        blocks->blocks[i].uncompressedSize = isize;
        ip += isize;
        op += cBlockSize;
    }
    CONTROL(ip == iend);

    return blocks;
}

static void skipToLiterals(blocks_t* blocks)
{
    size_t b;
    size_t outBlock = 0;
    for (b = 0; b < blocks->numBlocks; ++b) {
        block_t block = blocks->blocks[b];
        /* Skip frame header */
        {
            size_t const fhSize = ZSTD_frameHeaderSize(block.begin, block_getSize(block));
            CONTROL(!ZSTD_isError(fhSize)); 
            block.begin += fhSize;
        }
        /* Truncate to end of first block and skip uncompressed blocks */
        {
            blockProperties_t bp;
            size_t const cBlockSize = ZSTD_getcBlockSize(block.begin, block_getSize(block), &bp);
            CONTROL(!ZSTD_isError(cBlockSize));
            if (bp.blockType != bt_compressed) {
                /* Don't write the output block */
                continue;
            }
            /* End of first block */
            block.end = block.begin + ZSTD_blockHeaderSize + cBlockSize;
        }
        /* Skip block header */
        block.begin += ZSTD_blockHeaderSize;
        /* Write the output block */
        blocks->blocks[outBlock++] = block;
    }
    CONTROL(outBlock <= blocks->numBlocks);
    blocks->numBlocks = outBlock;
}

size_t ZSTD_decodeLiteralsBlock(ZSTD_DCtx* ctx, const void* src, size_t srcSize);
static void skipToSequences(blocks_t* blocks, ZSTD_DCtx* dctx)
{
    skipToLiterals(blocks);
    size_t b;
    for (b = 0; b < blocks->numBlocks; ++b) {
        block_t* const block = &blocks->blocks[b];
        CONTROL(!ZSTD_isError(ZSTD_decompressBegin(dctx)));
        CONTROL(block->begin < block->end);
        {
            size_t const litSize = ZSTD_decodeLiteralsBlock(dctx, block->begin, block_getSize(*block));
            CONTROL(!ZSTD_isError(litSize));
            block->begin += litSize;
        }
        CONTROL(block->begin < block->end);
    }
}

static size_t totalUncompressedSize(blocks_t const* blocks)
{
    size_t total = 0;
    size_t b;
    for (b = 0; b < blocks->numBlocks; ++b) {
        total += blocks->blocks[b].uncompressedSize;
    }
    return total;
}

FORCE_NOINLINE size_t ZSTD_decodeLiteralsHeader(ZSTD_DCtx* dctx, void const* src, size_t srcSize)
{
    RETURN_ERROR_IF(srcSize < MIN_CBLOCK_SIZE, corruption_detected, "");
    {
        BYTE const* istart = (BYTE const*)src;
        symbolEncodingType_e const litEncType = (symbolEncodingType_e)(istart[0] & 3);
        if (litEncType == set_compressed) {
            RETURN_ERROR_IF(srcSize < 5, corruption_detected, "srcSize >= MIN_CBLOCK_SIZE == 3; here we need up to 5 for case 3");
            size_t lhSize, litSize, litCSize;
            U32 singleStream=0;
            U32 const lhlCode = (istart[0] >> 2) & 3;
            U32 const lhc = MEM_readLE32(istart);
            switch(lhlCode)
            {
            case 0: case 1: default:   /* note : default is impossible, since lhlCode into [0..3] */
                /* 2 - 2 - 10 - 10 */
                singleStream = !lhlCode;
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
            return HUF_readDTableX1_wksp_bmi2(
                    dctx->entropy.hufTable,
                    istart+lhSize, litCSize,
                    dctx->workspace, sizeof(dctx->workspace),
                    dctx->bmi2);
        }
    }
    return 0;
}

static void benchmark_ZSTD_decodeLiteralsHeader(ZSTD_DCtx* dctx, blocks_t const* blocks)
{
    size_t const numBlocks = blocks->numBlocks;
    size_t b;
    CONTROL(!ZSTD_isError(ZSTD_decompressBegin(dctx)));
    for (b = 0; b < numBlocks; ++b) {
        block_t const block = blocks->blocks[b];
        size_t const ret = ZSTD_decodeLiteralsHeader(dctx, block.begin, block_getSize(block));
        CONTROL(!ZSTD_isError(ret));
    }
}

static void benchmark_ZSTD_decodeSeqHeaders(ZSTD_DCtx* dctx, blocks_t const* blocks)
{
    size_t const numBlocks = blocks->numBlocks;
    size_t b;
    CONTROL(!ZSTD_isError(ZSTD_decompressBegin(dctx)));
    for (b = 0; b < numBlocks; ++b) {
        block_t const block = blocks->blocks[b];
        int nbSeq;
        size_t const cSize = ZSTD_decodeSeqHeaders(dctx, &nbSeq, block.begin, block_getSize(block));
        CONTROL(!ZSTD_isError(cSize));
    }
}

#if 0
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
    //return ZSTD_compress(dst, dstSize, src, srcSize, cLevel);
}

static size_t g_cSize = 0;
static size_t local_ZSTD_decompress(const void* src, size_t srcSize,
                                    void* dst, size_t dstSize,
                                    void* buff2)
{
    (void)src; (void)srcSize;
    return ZSTD_decompress(dst, dstSize, buff2, g_cSize);
}

static ZSTD_DCtx* g_zdc = NULL;

#ifndef ZSTD_DLL_IMPORT
extern size_t ZSTD_decodeLiteralsBlock(ZSTD_DCtx* ctx, const void* src, size_t srcSize);
static size_t local_ZSTD_decodeLiteralsBlock(const void* src, size_t srcSize, void* dst, size_t dstSize, void* buff2)
{
    (void)src; (void)srcSize; (void)dst; (void)dstSize;
    return ZSTD_decodeLiteralsBlock(g_zdc, buff2, g_cSize);
}

static size_t local_ZSTD_decodeSeqHeaders(const void* src, size_t srcSize, void* dst, size_t dstSize, void* buff2)
{
    int nbSeq;
    (void)src; (void)srcSize; (void)dst; (void)dstSize;
    return ZSTD_decodeSeqHeaders(g_zdc, &nbSeq, buff2, g_cSize);
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
    ZSTD_CCtx* const cctx = ZSTD_createCCtx();
    size_t r;
    assert(cctx != NULL);

    r = local_ZSTD_compressStream(src, srcSize, dst, dstCapacity, payload);

    ZSTD_freeCCtx(cctx);

    return r;
}

static size_t
local_ZSTD_compress_generic_end(const void* src, size_t srcSize,
                                void* dst, size_t dstCapacity,
                                void* payload)
{
    (void)payload;
    return ZSTD_compress2(g_cstream, dst, dstCapacity, src, srcSize);
}

static size_t
local_ZSTD_compress_generic_continue(const void* src, size_t srcSize,
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
                            void* buff2)
{
    ZSTD_outBuffer buffOut;
    ZSTD_inBuffer buffIn;
    (void)src; (void)srcSize;
    ZSTD_initDStream(g_dstream);
    buffOut.dst = dst;
    buffOut.size = dstCapacity;
    buffOut.pos = 0;
    buffIn.src = buff2;
    buffIn.size = g_cSize;
    buffIn.pos = 0;
    ZSTD_decompressStream(g_dstream, &buffOut, &buffIn);
    return buffOut.pos;
}

#ifndef ZSTD_DLL_IMPORT
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
                                            void* buff2)
{
    size_t regeneratedSize = 0;
    const BYTE* ip = (const BYTE*)buff2;
    const BYTE* const iend = ip + g_cSize;
    BYTE* op = (BYTE*)dst;
    size_t remainingCapacity = dstCapacity;

    (void)src; (void)srcSize;  /* unused */
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
#endif
#endif

/*_*******************************************************
*  Bench functions
*********************************************************/
static void benchMem(unsigned benchNb, unsigned nbIters,
                     const void* src, size_t srcSize, size_t blockSize,
                     int cLevel, ZSTD_compressionParameters cparams)
{
    size_t const dstSize = compressBlockBound(srcSize, blockSize);
    void* const dst = malloc(dstSize);
    ZSTD_CCtx* const cctx = ZSTD_createCCtx();
    ZSTD_DCtx* const dctx = ZSTD_createDCtx();
    CONTROL(dst != NULL);
    CONTROL(cctx != NULL);
    CONTROL(dctx != NULL);

    DISPLAY("block size: %u \n", (unsigned)blockSize);
    DISPLAY("params: cLevel %d, wlog %d hlog %d clog %d slog %d mml %d tlen %d strat %d \n",
          cLevel, cparams.windowLog, cparams.hashLog, cparams.chainLog, cparams.searchLog,
          cparams.minMatch, cparams.targetLength, cparams.strategy);

    CONTROL(!ZSTD_isError(ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, cLevel)));
    CONTROL(!ZSTD_isError(ZSTD_CCtx_setParameter(cctx, ZSTD_c_windowLog, (int)cparams.windowLog)));
    CONTROL(!ZSTD_isError(ZSTD_CCtx_setParameter(cctx, ZSTD_c_hashLog, (int)cparams.hashLog)));
    CONTROL(!ZSTD_isError(ZSTD_CCtx_setParameter(cctx, ZSTD_c_chainLog, (int)cparams.chainLog)));
    CONTROL(!ZSTD_isError(ZSTD_CCtx_setParameter(cctx, ZSTD_c_searchLog, (int)cparams.searchLog)));
    CONTROL(!ZSTD_isError(ZSTD_CCtx_setParameter(cctx, ZSTD_c_minMatch, (int)cparams.minMatch)));
    CONTROL(!ZSTD_isError(ZSTD_CCtx_setParameter(cctx, ZSTD_c_targetLength, (int)cparams.targetLength)));
    CONTROL(!ZSTD_isError(ZSTD_CCtx_setParameter(cctx, ZSTD_c_strategy, cparams.strategy)));

    {
        /* Preparation */
        blocks_t* const blocks = compressBlocks(cctx, dst, dstSize, src, srcSize, blockSize);
        char const* benchName = "";
        size_t iter;
        switch (benchNb)
        {
        case 1:
            benchName = "ZSTD_decodeLiteralsHeaders";
            skipToLiterals(blocks);
            break;
        case 2:
            benchName = "ZSTD_decodeSeqHeaders";
            skipToSequences(blocks, dctx);
            break;
        default:
            break;
        }

        /* Benchmark loop */
        { 
            UTIL_time_t const begin = UTIL_getTime();
            for (iter = 0; iter < nbIters; ++iter) {
                switch (benchNb)
                {
                case 1:
                    benchmark_ZSTD_decodeLiteralsHeader(dctx, blocks);
                    break;
                case 2:
                    benchmark_ZSTD_decodeSeqHeaders(dctx, blocks);
                    break;
                default:
                    break;
                }
            }
            {
                UTIL_time_t const end = UTIL_getTime();
                size_t const bytesProcessed = nbIters * totalUncompressedSize(blocks);
                size_t const nanos = UTIL_getSpanTimeNano(begin, end);
                double const MBps = ((double)bytesProcessed * TIMELOOP_NANOSEC) / (nanos * MB_UNIT);
                DISPLAY("%2u#%-29.29s: %8.1f MB/s  (%u bytes in %u blocks over %u iters) \n", benchNb, benchName, MBps, (unsigned)bytesProcessed, (unsigned)blocks->numBlocks * nbIters, nbIters);
            }
        }
        free(blocks);
    }

    free(dst);
    ZSTD_freeCCtx(cctx);
    ZSTD_freeDCtx(dctx);
}


static int benchSample(U32 benchNb, U32 nbIters, size_t blockSize,
                       size_t benchedSize, double compressibility,
                       int cLevel, ZSTD_compressionParameters cparams)
{
    /* Allocation */
    void* const origBuff = malloc(benchedSize);
    if (!origBuff) { DISPLAY("\nError: not enough memory!\n"); return 12; }

    /* Fill buffer */
    RDG_genBuffer(origBuff, benchedSize, compressibility, 0.0, 0);

    /* bench */
    DISPLAY("\r%70s\r", "");
    DISPLAY(" Sample %u bytes : \n", (unsigned)benchedSize);
    benchMem(benchNb, nbIters, origBuff, benchedSize, blockSize, cLevel, cparams);

    free(origBuff);
    return 0;
}


static int benchFiles(U32 benchNb, U32 nbIters, size_t blockSize,
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
            benchMem(benchNb, nbIters, origBuff, benchedSize, blockSize, cLevel, cparams);

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
    U32 benchNb = 0, main_pause = 0;
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
                    benchNb = readU32FromChar(&argument);
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
        result = benchSample(benchNb, g_nbIterations, sampleSize, sampleSize, compressibility, cLevel, cparams);
    else
        result = benchFiles(benchNb, g_nbIterations, sampleSize,  argv+filenamesStart, argc-filenamesStart, cLevel, cparams);

    if (main_pause) { int unused; printf("press enter...\n"); unused = getchar(); (void)unused; }

    return result;
}
