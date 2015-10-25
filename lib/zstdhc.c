/*
    ZSTD HC - High Compression Mode of Zstandard
    Copyright (C) 2015, Yann Collet.

    BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
       - Zstd source repository : https://www.zstd.net
*/


/* *************************************
*  Includes
***************************************/
#include <stdlib.h>   /* malloc */
#include <string.h>   /* memset */
#include "zstdhc_static.h"
#include "zstd_static.h"
#include "mem.h"


/* *************************************
*  Tuning Parameter
***************************************/
static const U32 ZSTD_HC_compressionLevel_default = 9;


/* *************************************
*  Local Constants
***************************************/
#define MINMATCH 4
#define MAXD_LOG 26

#define KB *1024
#define MB *1024*1024
#define GB *(1ULL << 30)

/* *************************************
*  Local Types
***************************************/
#define BLOCKSIZE (128 KB)                 /* define, for static allocation */
#define WORKPLACESIZE (BLOCKSIZE*3)

#define MAX_CLEVEL 20
static const ZSTD_HC_parameters ZSTD_HC_defaultParameters[MAX_CLEVEL] = {
   /*  W,  C,  H,  S */
    { 19, 18, 17 , 1},
    { 19, 18, 17 , 1},
    { 19, 18, 17 , 2},
    { 19, 18, 17 , 3},
    { 19, 18, 17 , 4},
    { 19, 18, 17 , 5},
    { 19, 18, 17 , 6},
    { 19, 18, 17 , 7},
    { 19, 18, 17 , 8},
    { 19, 18, 17 , 9},
    { 19, 18, 17 ,10},
    { 19, 18, 17 ,11},
    { 19, 18, 17 ,12},
    { 19, 18, 17 ,13},
    { 19, 18, 17 ,14},
    { 19, 18, 17 ,15},
    { 19, 18, 17 ,16},
    { 19, 18, 17 ,17},
    { 19, 18, 17 ,18},
    { 19, 18, 17 ,19},
};

struct ZSTD_HC_CCtx_s
{
    const BYTE* end;        /* next block here to continue on current prefix */
    const BYTE* base;       /* All regular indexes relative to this position */
    const BYTE* dictBase;   /* extDict indexes relative to this position */
    U32   dictLimit;        /* below that point, need extDict */
    U32   lowLimit;         /* below that point, no more data */
    U32   nextToUpdate;     /* index from which to continue dictionary update */
    ZSTD_HC_parameters params;
    U32 hashTableLog;
    U32 chainTableLog;
    U32* hashTable;
    U32* chainTable;
    seqStore_t seqStore;    /* sequences storage ptrs */
	BYTE buffer[WORKPLACESIZE];
};


ZSTD_HC_CCtx* ZSTD_HC_createCCtx(void)
{
    ZSTD_HC_CCtx* ctx = (ZSTD_HC_CCtx*) malloc(sizeof(ZSTD_HC_CCtx));
    ctx->hashTable = NULL;
    ctx->chainTable = NULL;
    ctx->hashTableLog = 0;
    ctx->chainTableLog = 0;
    return ctx;
}

size_t ZSTD_HC_freeCCtx(ZSTD_HC_CCtx* cctx)
{
    free(cctx->hashTable);
    free(cctx->chainTable);
    free(cctx);
    return 0;
}

static void ZSTD_HC_resetCCtx_advanced (ZSTD_HC_CCtx* zc,
                                        const ZSTD_HC_parameters params, const void* start)
{
    U32 maxDistance = ( 1 << params.searchLog) + 1;

    if (zc->hashTableLog < params.hashLog)
    {
        free(zc->hashTable);
        zc->hashTableLog = params.hashLog;
        zc->hashTable = (U32*) malloc ( (1 << zc->hashTableLog) * sizeof(U32) );
    }
    memset(zc->hashTable, 0, (1 << params.hashLog) * sizeof(U32) );

    if (zc->chainTableLog < params.chainLog)
    {
        free(zc->chainTable);
        zc->chainTableLog = params.chainLog;
        zc->chainTable = (U32*) malloc ( (1 << zc->chainTableLog) * sizeof(U32) );
    }
    memset(zc->chainTable, 0, (1 << params.chainLog) * sizeof(U32) );

    zc->nextToUpdate = maxDistance;
    zc->end = (const BYTE*)start;
    zc->base = zc->end - maxDistance;
    zc->dictBase = zc->base;
    zc->dictLimit = maxDistance;
    zc->lowLimit = maxDistance;
    zc->params = params;
	zc->seqStore.buffer = zc->buffer;
    zc->seqStore.offsetStart = (U32*) (zc->seqStore.buffer);
    zc->seqStore.offCodeStart = (BYTE*) (zc->seqStore.offsetStart + (BLOCKSIZE>>2));
    zc->seqStore.litStart = zc->seqStore.offCodeStart + (BLOCKSIZE>>2);
    zc->seqStore.litLengthStart =  zc->seqStore.litStart + BLOCKSIZE;
    zc->seqStore.matchLengthStart = zc->seqStore.litLengthStart + (BLOCKSIZE>>2);
    zc->seqStore.dumpsStart = zc->seqStore.matchLengthStart + (BLOCKSIZE>>2);
}


/* *************************************
*  Local Macros
***************************************/
#define KNUTH 2654435761U
static U32 ZSTD_HC_hash(U32 u, U32 h) { return (u * KNUTH) >> (32-h) ; }
#define DELTANEXT(d)           chainTable[(d) & chainMask]   /* flexible, CHAINSIZE dependent */

static U32 ZSTD_HC_hashPtr(const void* ptr, U32 h) { return ZSTD_HC_hash(MEM_read32(ptr), h); }


/* *************************************
*  HC Compression
***************************************/
/* Update chains up to ip (excluded) */
static void ZSTD_HC_insert (ZSTD_HC_CCtx* zc, const BYTE* ip)
{
    U32* const hashTable  = zc->hashTable;
    const U32 hashLog = zc->params.hashLog;
    U32* const chainTable = zc->chainTable;
    const U32 chainMask = (1 << zc->params.chainLog) - 1;
    const BYTE* const base = zc->base;
    const U32 target = (U32)(ip - base);
    U32 idx = zc->nextToUpdate;

    while(idx < target)
    {
        U32 h = ZSTD_HC_hashPtr(base+idx, hashLog);
        size_t delta = idx - hashTable[h];
        DELTANEXT(idx) = (U32)delta;
        hashTable[h] = idx;
        idx++;
    }

    zc->nextToUpdate = target;
}


static size_t ZSTD_HC_insertAndFindBestMatch (
                        ZSTD_HC_CCtx* zc,   /* Index table will be updated */
                        const BYTE* ip, const BYTE* const iLimit,
                        const BYTE** matchpos,
                        const U32 maxNbAttempts)
{
    U32* const hashTable = zc->hashTable;
    const U32 hashLog = zc->params.hashLog;
    U32* const chainTable = zc->chainTable;
    const U32 chainSize = (1 << zc->params.chainLog);
    const U32 chainMask = chainSize-1;
    const BYTE* const base = zc->base;
    const BYTE* const dictBase = zc->dictBase;
    const U32 dictLimit = zc->dictLimit;
    const U32 maxDistance = (1 << zc->params.windowLog);
    const U32 lowLimit = (zc->lowLimit + maxDistance > (U32)(ip-base)) ? zc->lowLimit : (U32)(ip - base) - (maxDistance - 1);
    U32 matchIndex;
    const BYTE* match;
    int nbAttempts=maxNbAttempts;
    size_t ml=0;

    /* HC4 match finder */
    ZSTD_HC_insert(zc, ip);
    matchIndex = hashTable[ZSTD_HC_hashPtr(ip, hashLog)];

    while ((matchIndex>=lowLimit) && (nbAttempts))
    {
        nbAttempts--;
        if (matchIndex >= dictLimit)
        {
            match = base + matchIndex;
            if (*(match+ml) == *(ip+ml)
                && (MEM_read32(match) == MEM_read32(ip)))
            {
                const size_t mlt = ZSTD_count(ip+MINMATCH, match+MINMATCH, iLimit) + MINMATCH;
                if (mlt > ml) { ml = mlt; *matchpos = match; }
            }
        }
        else
        {
            match = dictBase + matchIndex;
            if (MEM_read32(match) == MEM_read32(ip))
            {
                size_t mlt;
                const BYTE* vLimit = ip + (dictLimit - matchIndex);
                if (vLimit > iLimit) vLimit = iLimit;
                mlt = ZSTD_count(ip+MINMATCH, match+MINMATCH, vLimit) + MINMATCH;
                if ((ip+mlt == vLimit) && (vLimit < iLimit))
                    mlt += ZSTD_count(ip+mlt, base+dictLimit, iLimit);
                if (mlt > ml) { ml = mlt; *matchpos = base + matchIndex; }   /* virtual matchpos */
            }
        }

        if (base + matchIndex <= ip - chainSize) break;
        matchIndex -= DELTANEXT(matchIndex);
    }

    return ml;
}


static size_t ZSTD_HC_compressBlock(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    seqStore_t* seqStorePtr = &(ctx->seqStore);
    const BYTE* const istart = (const BYTE*)src;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const BYTE* const iend = istart + srcSize;
    const BYTE* const ilimit = iend - 8;

    size_t offset_2=REPCODE_STARTVALUE, offset_1=REPCODE_STARTVALUE;
    const U32 maxSearches = 1 << ctx->params.searchLog;

    /* init */
    ZSTD_resetSeqStore(seqStorePtr);
    if (((ip-ctx->base) - ctx->dictLimit) < REPCODE_STARTVALUE) ip += REPCODE_STARTVALUE;

    /* Match Loop */
    while (ip < ilimit)
    {
        /* repcode */
        if (MEM_read32(ip) == MEM_read32(ip - offset_2))
        /* store sequence */
        {
            size_t matchLength = ZSTD_count(ip+4, ip+4-offset_2, iend);
            size_t litLength = ip-anchor;
            size_t offset = offset_2;
            offset_2 = offset_1;
            offset_1 = offset;
            ZSTD_storeSeq(seqStorePtr, litLength, anchor, 0, matchLength);
            ip += matchLength+MINMATCH;
            anchor = ip;
            continue;
        }

        /* search */
        {
            const BYTE* match;
            size_t matchLength = ZSTD_HC_insertAndFindBestMatch(ctx, ip, iend, &match, maxSearches);
            if (!matchLength) { ip++; offset_2 = offset_1; continue; }
            /* store sequence */
            {
                size_t litLength = ip-anchor;
                size_t offset = ip-match;
                if (offset == offset_2) offset = 0;
                offset_2 = offset_1;
                offset_1 = ip-match;
                ZSTD_storeSeq(seqStorePtr, litLength, anchor, offset, matchLength-MINMATCH);
                ip += matchLength;
                anchor = ip;
            }
        }
    }

    /* Last Literals */
    {
        size_t lastLLSize = iend - anchor;
        memcpy(seqStorePtr->lit, anchor, lastLLSize);
        seqStorePtr->lit += lastLLSize;
    }

    /* Finale compression stage */
    return ZSTD_compressSequences((BYTE*)dst, maxDstSize,
                                  seqStorePtr, srcSize);
}

static size_t ZSTD_HC_compress_generic (ZSTD_HC_CCtx* ctxPtr,
                                        void* dst, size_t maxDstSize,
                                  const void* src, size_t srcSize)
{
    static const size_t blockSize = 128 KB;
    size_t remaining = srcSize;
    const BYTE* ip = (const BYTE*)src;
    BYTE* const ostart = (BYTE*)dst;
    BYTE* op = ostart;
    BYTE* const oend = op + maxDstSize;

    while (remaining > blockSize)
    {
        size_t cSize = ZSTD_HC_compressBlock(ctxPtr, op+3, oend-op, ip, blockSize);

        if (cSize == 0)
        {
            cSize = ZSTD_noCompressBlock(op, maxDstSize, ip, blockSize);   /* block is not compressible */
        }
        else
        {
            op[0] = (BYTE)(cSize>>16);
            op[1] = (BYTE)(cSize>>8);
            op[2] = (BYTE)cSize;
            op[0] += (BYTE)(bt_compressed << 6); /* is a compressed block */
            cSize += 3;
        }

        remaining -= blockSize;
        ip += blockSize;
        op += cSize;
        if (ZSTD_isError(cSize)) return cSize;
    }

    /* last block */
    {
        size_t cSize = ZSTD_HC_compressBlock(ctxPtr, op+3, oend-op, ip, remaining);

        if (cSize == 0)
        {
            cSize = ZSTD_noCompressBlock(op, maxDstSize, ip, remaining);   /* block is not compressible */
        }
        else
        {
            op[0] = (BYTE)(cSize>>16);
            op[1] = (BYTE)(cSize>>8);
            op[2] = (BYTE)cSize;
            op[0] += (BYTE)(bt_compressed << 6); /* is a compressed block */
            cSize += 3;
        }

        op += cSize;
        if (ZSTD_isError(cSize)) return cSize;
    }

    return op-ostart;
}


size_t ZSTD_HC_loadDict(ZSTD_HC_CCtx* ctx, const void* dictionary, size_t dictSize)
{
    /* TBD */
    (void)ctx; (void)dictionary; (void)dictSize;
    return 0;
}

static void ZSTD_HC_setExternalDict(ZSTD_HC_CCtx* ctxPtr, const void* newBlock)
{
    if (ctxPtr->end >= ctxPtr->base + 4)
        ZSTD_HC_insert (ctxPtr, ctxPtr->end-3);   /* Referencing remaining dictionary content */
    /* Only one memory segment for extDict, so any previous extDict is lost at this stage */
    ctxPtr->lowLimit  = ctxPtr->dictLimit;
    ctxPtr->dictLimit = (U32)(ctxPtr->end - ctxPtr->base);
    ctxPtr->dictBase  = ctxPtr->base;
    ctxPtr->base = (const BYTE*)newBlock - ctxPtr->dictLimit;
    ctxPtr->end  = (const BYTE*)newBlock;
    ctxPtr->nextToUpdate = ctxPtr->dictLimit;   /* match referencing will resume from there */
}

size_t ZSTD_HC_compress_continue (ZSTD_HC_CCtx* ctxPtr,
                                  void* dst, size_t dstSize,
                            const void* src, size_t srcSize)
{
    const U32 maxDistance = 1 << ctxPtr->params.windowLog;

    /* Check overflow */
    if ((size_t)(ctxPtr->end - ctxPtr->base) > 2 GB)
    {
        size_t dictSize = (size_t)(ctxPtr->end - ctxPtr->base) - ctxPtr->dictLimit;
        if (dictSize > maxDistance) dictSize = maxDistance;

        ZSTD_HC_loadDict(ctxPtr, ctxPtr->end - dictSize, dictSize);
    }

    /* Check if blocks follow each other */
    if ((const BYTE*)src != ctxPtr->end)
        ZSTD_HC_setExternalDict(ctxPtr, (const BYTE*)src);

    /* Check overlapping src/dictionary space (typical of cycling buffers) */
    {
        const BYTE* sourceEnd = (const BYTE*) src + srcSize;
        const BYTE* dictBegin = ctxPtr->dictBase + ctxPtr->lowLimit;
        const BYTE* dictEnd   = ctxPtr->dictBase + ctxPtr->dictLimit;
        if ((sourceEnd > dictBegin) && ((const BYTE*)src < dictEnd))
        {
            if (sourceEnd > dictEnd) sourceEnd = dictEnd;
            ctxPtr->lowLimit = (U32)(sourceEnd - ctxPtr->dictBase);
            if (ctxPtr->dictLimit - ctxPtr->lowLimit < 4) ctxPtr->lowLimit = ctxPtr->dictLimit;
        }
    }

    return ZSTD_HC_compress_generic (ctxPtr, dst, dstSize, src, srcSize);
}


size_t ZSTD_HC_compressBegin_advanced(ZSTD_HC_CCtx* ctx,
                                      void* dst, size_t maxDstSize,
                                      const ZSTD_HC_parameters params, const void* src)
{
    /* Sanity check */
    if (maxDstSize < 4) return ERROR(dstSize_tooSmall);

    /* Init */
    ZSTD_HC_resetCCtx_advanced(ctx, params, src);

    /* Write Header */
    MEM_writeLE32(dst, ZSTD_magicNumber);

    return 4;
}


size_t ZSTD_HC_compressBegin(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, unsigned compressionLevel, const void* src)
{
    if (compressionLevel==0) compressionLevel = ZSTD_HC_compressionLevel_default;
    if (compressionLevel > MAX_CLEVEL) compressionLevel = MAX_CLEVEL;
    return ZSTD_HC_compressBegin_advanced(ctx, dst, maxDstSize, ZSTD_HC_defaultParameters[compressionLevel], src);
}


size_t ZSTD_HC_compress_advanced (ZSTD_HC_CCtx* ctx,
                                 void* dst, size_t maxDstSize,
                                 const void* src, size_t srcSize,
                                 ZSTD_HC_parameters params)
{
    BYTE* const ostart = (BYTE*)dst;
    BYTE* op = ostart;

    /* Header */
    size_t oSize = ZSTD_HC_compressBegin_advanced(ctx, dst, maxDstSize, params, src);
    if(ZSTD_isError(oSize)) return oSize;
    op += oSize;
    maxDstSize -= oSize;

    /* body (compression) */
    op += ZSTD_HC_compress_generic (ctx, op,  maxDstSize, src, srcSize);
    if(ZSTD_isError(oSize)) return oSize;
    op += oSize;
    maxDstSize -= oSize;

    /* Close frame */
    oSize = ZSTD_compressEnd((ZSTD_CCtx*)ctx, op, maxDstSize);
    if(ZSTD_isError(oSize)) return oSize;
    op += oSize;

    return (op - ostart);
}

size_t ZSTD_HC_compressCCtx (ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize, unsigned compressionLevel)
{
    if (compressionLevel==0) compressionLevel = ZSTD_HC_compressionLevel_default;
    if (compressionLevel > MAX_CLEVEL) compressionLevel = MAX_CLEVEL;
    return ZSTD_HC_compress_advanced(ctx, dst, maxDstSize, src, srcSize, ZSTD_HC_defaultParameters[compressionLevel]);
}

size_t ZSTD_HC_compress(void* dst, size_t maxDstSize, const void* src, size_t srcSize, unsigned compressionLevel)
{
    ZSTD_HC_CCtx* ctx = ZSTD_HC_createCCtx();
    size_t result = ZSTD_HC_compressCCtx(ctx, dst, maxDstSize, src, srcSize, compressionLevel);
    ZSTD_HC_freeCCtx(ctx);
    return result;
}



/**************************************
*  Streaming Functions
**************************************/
/* dictionary saving */

size_t ZSTD_HC_saveDict (ZSTD_HC_CCtx* ctx, void* safeBuffer, size_t dictSize)
{
    /* TBD */
    (void)ctx; (void)safeBuffer; (void)dictSize;
    return 0;
}

