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
#include "zstdhc.h"
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
#define MAX_DISTANCE 65535   /* <=== To be changed (dynamic ?) */

#define DICTIONARY_LOGSIZE 16
#define MAXD (1<<DICTIONARY_LOGSIZE)
#define MAXD_MASK (MAXD - 1)

#define HASH_LOG (DICTIONARY_LOGSIZE-1)
#define HASHTABLESIZE (1 << HASH_LOG)
#define HASH_MASK (HASHTABLESIZE - 1)

static const U32 g_maxCompressionLevel = 19;

#define KB *1024
#define MB *1024*1024
#define GB *(1ULL << 30)

/* *************************************
*  Local Types
***************************************/
#define BLOCKSIZE (128 KB)                 /* define, for static allocation */
#define WORKPLACESIZE (BLOCKSIZE*3)

struct ZSTD_HC_CCtx_s
{
    U32   hashTable[HASHTABLESIZE];
    U16   chainTable[MAXD];
    const BYTE* end;        /* next block here to continue on current prefix */
    const BYTE* base;       /* All index relative to this position */
    const BYTE* dictBase;   /* alternate base for extDict */
    BYTE* inputBuffer;      /* deprecated */
    U32   dictLimit;        /* below that point, need extDict */
    U32   lowLimit;         /* below that point, no more dict */
    U32   nextToUpdate;     /* index from which to continue dictionary update */
    U32   compressionLevel;
    seqStore_t seqStore;
	BYTE buffer[WORKPLACESIZE];
};


ZSTD_HC_CCtx* ZSTD_HC_createCCtx(void)
{
    return (ZSTD_HC_CCtx*) malloc(sizeof(ZSTD_HC_CCtx));
}

size_t ZSTD_HC_freeCCtx(ZSTD_HC_CCtx* cctx) { free(cctx); return 0; }

static void ZSTD_HC_resetCCtx (ZSTD_HC_CCtx* zc, U32 compressionLevel, const BYTE* start)
{
    if (compressionLevel==0) compressionLevel = ZSTD_HC_compressionLevel_default;
    if (compressionLevel > g_maxCompressionLevel) compressionLevel = g_maxCompressionLevel;
    memset(zc->hashTable, 0, sizeof(zc->hashTable));
    memset(zc->chainTable, 0xFF, sizeof(zc->chainTable));
    zc->nextToUpdate = 64 KB;
    zc->base = start - 64 KB;
    zc->end = start;
    zc->dictBase = start - 64 KB;
    zc->dictLimit = 64 KB;
    zc->lowLimit = 64 KB;
    zc->compressionLevel = compressionLevel;
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
#define HASH_FUNCTION(u)       (((u) * 2654435761U) >> ((MINMATCH*8)-HASH_LOG))
//#define DELTANEXTU16(p)        chainTable[(p) & MAXD_MASK]   /* flexible, MAXD dependent */
#define DELTANEXTU16(p)        chainTable[(U16)(p)]   /* faster */

static U32 ZSTD_HC_hashPtr(const void* ptr) { return HASH_FUNCTION(MEM_read32(ptr)); }


/* *************************************
*  HC Compression
***************************************/
/* Update chains up to ip (excluded) */
static void ZSTD_HC_insert (ZSTD_HC_CCtx* zc, const BYTE* ip)
{
    U16* chainTable = zc->chainTable;
    U32* HashTable  = zc->hashTable;
    const BYTE* const base = zc->base;
    const U32 target = (U32)(ip - base);
    U32 idx = zc->nextToUpdate;

    while(idx < target)
    {
        U32 h = ZSTD_HC_hashPtr(base+idx);
        size_t delta = idx - HashTable[h];
        if (delta>MAX_DISTANCE) delta = MAX_DISTANCE;
        DELTANEXTU16(idx) = (U16)delta;
        HashTable[h] = idx;
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
    U16* const chainTable = zc->chainTable;
    U32* const HashTable = zc->hashTable;
    const BYTE* const base = zc->base;
    const BYTE* const dictBase = zc->dictBase;
    const U32 dictLimit = zc->dictLimit;
    const U32 lowLimit = (zc->lowLimit + 64 KB > (U32)(ip-base)) ? zc->lowLimit : (U32)(ip - base) - (64 KB - 1);
    U32 matchIndex;
    const BYTE* match;
    int nbAttempts=maxNbAttempts;
    size_t ml=0;

    /* HC4 match finder */
    ZSTD_HC_insert(zc, ip);
    matchIndex = HashTable[ZSTD_HC_hashPtr(ip)];

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
        matchIndex -= DELTANEXTU16(matchIndex);
    }

    return ml;
}


size_t ZSTD_HC_InsertAndGetWiderMatch (
    ZSTD_HC_CCtx* zc,
    const BYTE* const ip,
    const BYTE* const iLowLimit,
    const BYTE* const iHighLimit,
    size_t longest,
    const BYTE** matchpos,
    const BYTE** startpos,
    const int maxNbAttempts)
{
    U16* const chainTable = zc->chainTable;
    U32* const HashTable = zc->hashTable;
    const BYTE* const base = zc->base;
    const U32 dictLimit = zc->dictLimit;
    const BYTE* const lowPrefixPtr = base + dictLimit;
    const U32 lowLimit = (zc->lowLimit + 64 KB > (U32)(ip-base)) ? zc->lowLimit : (U32)(ip - base) - (64 KB - 1);
    const BYTE* const dictBase = zc->dictBase;
    U32   matchIndex;
    int nbAttempts = maxNbAttempts;
    int delta = (int)(ip-iLowLimit);


    /* First Match */
    ZSTD_HC_insert(zc, ip);
    matchIndex = HashTable[ZSTD_HC_hashPtr(ip)];

    while ((matchIndex>=lowLimit) && (nbAttempts))
    {
        nbAttempts--;
        if (matchIndex >= dictLimit)
        {
            const BYTE* matchPtr = base + matchIndex;
            if (*(iLowLimit + longest) == *(matchPtr - delta + longest))
                if (MEM_read32(matchPtr) == MEM_read32(ip))
                {
                    size_t mlt = MINMATCH + ZSTD_count(ip+MINMATCH, matchPtr+MINMATCH, iHighLimit);
                    int back = 0;

                    while ((ip+back>iLowLimit)
                           && (matchPtr+back > lowPrefixPtr)
                           && (ip[back-1] == matchPtr[back-1]))
                            back--;

                    mlt -= back;

                    if (mlt > longest)
                    {
                        longest = mlt;
                        *matchpos = matchPtr+back;
                        *startpos = ip+back;
                    }
                }
        }
        else
        {
            const BYTE* matchPtr = dictBase + matchIndex;
            if (MEM_read32(matchPtr) == MEM_read32(ip))
            {
                size_t mlt;
                int back=0;
                const BYTE* vLimit = ip + (dictLimit - matchIndex);
                if (vLimit > iHighLimit) vLimit = iHighLimit;
                mlt = ZSTD_count(ip+MINMATCH, matchPtr+MINMATCH, vLimit) + MINMATCH;
                if ((ip+mlt == vLimit) && (vLimit < iHighLimit))
                    mlt += ZSTD_count(ip+mlt, base+dictLimit, iHighLimit);
                while ((ip+back > iLowLimit) && (matchIndex+back > lowLimit) && (ip[back-1] == matchPtr[back-1])) back--;
                mlt -= back;
                if (mlt > longest) { longest = (int)mlt; *matchpos = base + matchIndex + back; *startpos = ip+back; }
            }
        }
        matchIndex -= DELTANEXTU16(matchIndex);
    }

    return longest;
}


static size_t ZSTD_HC_compressBlock(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    seqStore_t* seqStorePtr = &(ctx->seqStore);
    const BYTE* const istart = (const BYTE*)src;
    const BYTE* ip = istart + 1;
    const BYTE* anchor = istart;
    const BYTE* const iend = istart + srcSize;
    const BYTE* const ilimit = iend - 8;

    size_t offset_2=4, offset_1=4;
    const U32 maxSearches = 1 << ctx->compressionLevel;

    /* init */
    ZSTD_resetSeqStore(seqStorePtr);

    /* Main Search Loop */
    while (ip < ilimit)
    {
        const BYTE* match;
        size_t matchLength = ZSTD_HC_insertAndFindBestMatch(ctx, ip, ilimit, &match, maxSearches);
        if (!matchLength) { ip++; continue; }
        /* save match */
        {
            size_t litLength = ip-anchor;
            size_t offset = ip-match;
            if (litLength) offset_2 = offset_1;
            if (offset == offset_2) offset = 0;
            offset_2 = offset_1;
            offset_1 = ip-match;
            ZSTD_storeSeq(seqStorePtr, litLength, anchor, offset, matchLength-MINMATCH);
            ip += matchLength;
            anchor = ip;
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
    ctxPtr->base = newBlock - ctxPtr->dictLimit;
    ctxPtr->end  = newBlock;
    ctxPtr->nextToUpdate = ctxPtr->dictLimit;   /* match referencing will resume from there */
}

size_t ZSTD_HC_compress_continue (ZSTD_HC_CCtx* ctxPtr,
                                  void* dst, size_t dstSize,
                            const void* src, size_t srcSize)
{
    /* Check overflow */
    if ((size_t)(ctxPtr->end - ctxPtr->base) > 2 GB)
    {
        size_t dictSize = (size_t)(ctxPtr->end - ctxPtr->base) - ctxPtr->dictLimit;
        if (dictSize > 64 KB) dictSize = 64 KB;

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


size_t ZSTD_HC_compressBegin(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, unsigned compressionLevel, const void* src)
{
    /* Sanity check */
    if (maxDstSize < 4) return ERROR(dstSize_tooSmall);

    /* Init */
    ZSTD_HC_resetCCtx(ctx, compressionLevel, src);

    /* Write Header */
    MEM_writeLE32(dst, ZSTD_magicNumber);

    return 4;
}

size_t ZSTD_HC_compressCCtx (ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize, unsigned compressionLevel)
{
    BYTE* const ostart = (BYTE*)dst;
    BYTE* op = ostart;

    /* Header */
    size_t oSize = ZSTD_HC_compressBegin(ctx, dst, maxDstSize, compressionLevel, src);
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

size_t ZSTD_HC_compress(void* dst, size_t maxDstSize, const void* src, size_t srcSize, unsigned compressionLevel)
{
    ZSTD_HC_CCtx ctxBody;
    return ZSTD_HC_compressCCtx(&ctxBody, dst, maxDstSize, src, srcSize, compressionLevel);
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

