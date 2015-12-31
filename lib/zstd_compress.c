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


/* *******************************************************
*  Compiler specifics
*********************************************************/
#ifdef _MSC_VER    /* Visual Studio */
#  define FORCE_INLINE static __forceinline
#  include <intrin.h>                    /* For Visual 2005 */
#  pragma warning(disable : 4127)        /* disable: C4127: conditional expression is constant */
#  pragma warning(disable : 4324)        /* disable: C4324: padded structure */
#else
#  define GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#  ifdef __GNUC__
#    define FORCE_INLINE static inline __attribute__((always_inline))
#  else
#    define FORCE_INLINE static inline
#  endif
#endif


/* *************************************
*  Includes
***************************************/
#include <stdlib.h>   /* malloc */
#include <string.h>   /* memset */
#include "mem.h"
#include "fse_static.h"
#include "huff0.h"
#include "zstd_static.h"
#include "zstd_internal.h"


/* *************************************
*  Constants
***************************************/
unsigned ZSTD_maxCLevel(void) { return ZSTD_MAX_CLEVEL; }
static const U32 g_searchStrength = 8;


/* *************************************
*  Sequence storage
***************************************/
typedef struct {
    void* buffer;
    U32*  offsetStart;
    U32*  offset;
    BYTE* offCodeStart;
    BYTE* offCode;
    BYTE* litStart;
    BYTE* lit;
    BYTE* litLengthStart;
    BYTE* litLength;
    BYTE* matchLengthStart;
    BYTE* matchLength;
    BYTE* dumpsStart;
    BYTE* dumps;
} seqStore_t;

static void ZSTD_resetSeqStore(seqStore_t* ssPtr)
{
    ssPtr->offset = ssPtr->offsetStart;
    ssPtr->lit = ssPtr->litStart;
    ssPtr->litLength = ssPtr->litLengthStart;
    ssPtr->matchLength = ssPtr->matchLengthStart;
    ssPtr->dumps = ssPtr->dumpsStart;
}


/* *************************************
*  Context memory management
***************************************/
struct ZSTD_CCtx_s
{
    const BYTE* nextSrc;    /* next block here to continue on current prefix */
    const BYTE* base;       /* All regular indexes relative to this position */
    const BYTE* dictBase;   /* extDict indexes relative to this position */
    U32   dictLimit;        /* below that point, need extDict */
    U32   lowLimit;         /* below that point, no more data */
    U32   nextToUpdate;     /* index from which to continue dictionary update */
    ZSTD_parameters params;
    void* workSpace;
    size_t workSpaceSize;
    size_t blockSize;

    seqStore_t seqStore;    /* sequences storage ptrs */
    U32* hashTable;
    U32* contentTable;
};


ZSTD_CCtx* ZSTD_createCCtx(void)
{
    return (ZSTD_CCtx*) calloc(1, sizeof(ZSTD_CCtx));
}

size_t ZSTD_freeCCtx(ZSTD_CCtx* cctx)
{
    free(cctx->workSpace);
    free(cctx);
    return 0;
}


static unsigned ZSTD_highbit(U32 val);

/** ZSTD_validateParams
    correct params value to remain within authorized range
    optimize for srcSize if srcSize > 0 */
void ZSTD_validateParams(ZSTD_parameters* params)
{
    const U32 btPlus = (params->strategy == ZSTD_btlazy2);

    /* validate params */
    if (MEM_32bits()) if (params->windowLog > 25) params->windowLog = 25;   /* 32 bits mode cannot flush > 24 bits */
    if (params->windowLog   > ZSTD_WINDOWLOG_MAX) params->windowLog = ZSTD_WINDOWLOG_MAX;
    if (params->windowLog   < ZSTD_WINDOWLOG_MIN) params->windowLog = ZSTD_WINDOWLOG_MIN;

    /* correct params, to use less memory */
    if ((params->srcSize > 0) && (params->srcSize < (1<<ZSTD_WINDOWLOG_MAX)))
    {
        U32 srcLog = ZSTD_highbit((U32)(params->srcSize)-1) + 1;
        if (params->windowLog > srcLog) params->windowLog = srcLog;
    }

    if (params->windowLog   < ZSTD_WINDOWLOG_ABSOLUTEMIN) params->windowLog = ZSTD_WINDOWLOG_ABSOLUTEMIN;  /* required for frame header */
    if (params->contentLog  > params->windowLog+btPlus) params->contentLog = params->windowLog+btPlus;   /* <= ZSTD_CONTENTLOG_MAX */
    if (params->contentLog  < ZSTD_CONTENTLOG_MIN) params->contentLog = ZSTD_CONTENTLOG_MIN;
    if (params->hashLog     > ZSTD_HASHLOG_MAX) params->hashLog = ZSTD_HASHLOG_MAX;
    if (params->hashLog     < ZSTD_HASHLOG_MIN) params->hashLog = ZSTD_HASHLOG_MIN;
    if (params->searchLog   > ZSTD_SEARCHLOG_MAX) params->searchLog = ZSTD_SEARCHLOG_MAX;
    if (params->searchLog   < ZSTD_SEARCHLOG_MIN) params->searchLog = ZSTD_SEARCHLOG_MIN;
    if (params->searchLength> ZSTD_SEARCHLENGTH_MAX) params->searchLength = ZSTD_SEARCHLENGTH_MAX;
    if (params->searchLength< ZSTD_SEARCHLENGTH_MIN) params->searchLength = ZSTD_SEARCHLENGTH_MIN;
    if ((U32)params->strategy>(U32)ZSTD_btlazy2) params->strategy = ZSTD_btlazy2;
}


static size_t ZSTD_resetCCtx_advanced (ZSTD_CCtx* zc,
                                       ZSTD_parameters params)
{
    /* note : params considered validated here */
    const size_t blockSize = MIN(BLOCKSIZE, (size_t)1 << params.windowLog);

    /* reserve table memory */
    {
        const U32 contentLog = (params.strategy == ZSTD_fast) ? 1 : params.contentLog;
        const size_t tableSpace = ((1 << contentLog) + (1 << params.hashLog)) * sizeof(U32);
        const size_t neededSpace = tableSpace + (3*blockSize);
        if (zc->workSpaceSize < neededSpace)
        {
            free(zc->workSpace);
            zc->workSpaceSize = neededSpace;
            zc->workSpace = malloc(neededSpace);
            if (zc->workSpace == NULL) return ERROR(memory_allocation);
        }
        memset(zc->workSpace, 0, tableSpace );
        zc->hashTable = (U32*)(zc->workSpace);
        zc->contentTable = zc->hashTable + ((size_t)1 << params.hashLog);
        zc->seqStore.buffer = (void*) (zc->contentTable + ((size_t)1 << contentLog));
    }

    zc->nextToUpdate = 1;
    zc->nextSrc = NULL;
    zc->base = NULL;
    zc->dictBase = NULL;
    zc->dictLimit = 0;
    zc->lowLimit = 0;
    zc->params = params;
    zc->blockSize = blockSize;
    zc->seqStore.offsetStart = (U32*) (zc->seqStore.buffer);
    zc->seqStore.offCodeStart = (BYTE*) (zc->seqStore.offsetStart + (blockSize>>2));
    zc->seqStore.litStart = zc->seqStore.offCodeStart + (blockSize>>2);
    zc->seqStore.litLengthStart =  zc->seqStore.litStart + blockSize;
    zc->seqStore.matchLengthStart = zc->seqStore.litLengthStart + (blockSize>>2);
    zc->seqStore.dumpsStart = zc->seqStore.matchLengthStart + (blockSize>>2);

    return 0;
}


/** ZSTD_reduceIndex
*   rescale indexes to avoid future overflow (indexes are U32) */
static void ZSTD_reduceIndex (ZSTD_CCtx* zc,
                        const U32 reducerValue)
{
    const U32 contentLog = (zc->params.strategy == ZSTD_fast) ? 1 : zc->params.contentLog;
    const U32 tableSpaceU32 = (1 << contentLog) + (1 << zc->params.hashLog);
    U32* table32 = zc->hashTable;
    U32 index;

    for (index=0 ; index < tableSpaceU32 ; index++)
    {
        if (table32[index] < reducerValue) table32[index] = 0;
        else table32[index] -= reducerValue;
    }
}


/* *******************************************************
*  Block entropic compression
*********************************************************/
size_t ZSTD_compressBound(size_t srcSize)   /* maximum compressed size */
{
    return FSE_compressBound(srcSize) + 12;
}


size_t ZSTD_noCompressBlock (void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    BYTE* const ostart = (BYTE* const)dst;

    if (srcSize + ZSTD_blockHeaderSize > maxDstSize) return ERROR(dstSize_tooSmall);
    memcpy(ostart + ZSTD_blockHeaderSize, src, srcSize);

    /* Build header */
    ostart[0]  = (BYTE)(srcSize>>16);
    ostart[1]  = (BYTE)(srcSize>>8);
    ostart[2]  = (BYTE) srcSize;
    ostart[0] += (BYTE)(bt_raw<<6);   /* is a raw (uncompressed) block */

    return ZSTD_blockHeaderSize+srcSize;
}


static size_t ZSTD_noCompressLiterals (void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    BYTE* const ostart = (BYTE* const)dst;

    if (srcSize + 3 > maxDstSize) return ERROR(dstSize_tooSmall);

    MEM_writeLE32(dst, ((U32)srcSize << 2) | IS_RAW);
    memcpy(ostart + 3, src, srcSize);
    return srcSize + 3;
}

static size_t ZSTD_compressRleLiteralsBlock (void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    BYTE* const ostart = (BYTE* const)dst;

    (void)maxDstSize;
    MEM_writeLE32(dst, ((U32)srcSize << 2) | IS_RLE);  /* note : maxDstSize > litHeaderSize > 4 */
    ostart[3] = *(const BYTE*)src;
    return 4;
}

size_t ZSTD_minGain(size_t srcSize) { return (srcSize >> 6) + 1; }

static size_t ZSTD_compressLiterals (void* dst, size_t maxDstSize,
                               const void* src, size_t srcSize)
{
    const size_t minGain = ZSTD_minGain(srcSize);
    BYTE* const ostart = (BYTE*)dst;
    size_t hsize;
    static const size_t litHeaderSize = 5;

    if (maxDstSize < litHeaderSize+1) return ERROR(dstSize_tooSmall);   /* not enough space for compression */

    hsize = HUF_compress(ostart+litHeaderSize, maxDstSize-litHeaderSize, src, srcSize);

    if ((hsize==0) || (hsize >= srcSize - minGain)) return ZSTD_noCompressLiterals(dst, maxDstSize, src, srcSize);
    if (hsize==1) return ZSTD_compressRleLiteralsBlock(dst, maxDstSize, src, srcSize);

    /* Build header */
    {
        ostart[0]  = (BYTE)(srcSize << 2); /* is a block, is compressed */
        ostart[1]  = (BYTE)(srcSize >> 6);
        ostart[2]  = (BYTE)(srcSize >>14);
        ostart[2] += (BYTE)(hsize << 5);
        ostart[3]  = (BYTE)(hsize >> 3);
        ostart[4]  = (BYTE)(hsize >>11);
    }

    return hsize+litHeaderSize;
}


#define LITERAL_NOENTROPY 63   /* cheap heuristic */

size_t ZSTD_compressSequences(void* dst, size_t maxDstSize,
                        const seqStore_t* seqStorePtr,
                              size_t srcSize)
{
    U32 count[MaxSeq+1];
    S16 norm[MaxSeq+1];
    size_t mostFrequent;
    U32 max = 255;
    U32 tableLog = 11;
    U32 CTable_LitLength  [FSE_CTABLE_SIZE_U32(LLFSELog, MaxLL )];
    U32 CTable_OffsetBits [FSE_CTABLE_SIZE_U32(OffFSELog,MaxOff)];
    U32 CTable_MatchLength[FSE_CTABLE_SIZE_U32(MLFSELog, MaxML )];
    U32 LLtype, Offtype, MLtype;   /* compressed, raw or rle */
    const BYTE* const op_lit_start = seqStorePtr->litStart;
    const BYTE* const llTable = seqStorePtr->litLengthStart;
    const BYTE* const llPtr = seqStorePtr->litLength;
    const BYTE* const mlTable = seqStorePtr->matchLengthStart;
    const U32*  const offsetTable = seqStorePtr->offsetStart;
    BYTE* const offCodeTable = seqStorePtr->offCodeStart;
    BYTE* const ostart = (BYTE*)dst;
    BYTE* op = ostart;
    BYTE* const oend = ostart + maxDstSize;
    const size_t nbSeq = llPtr - llTable;
    const size_t minGain = ZSTD_minGain(srcSize);
    const size_t maxCSize = srcSize - minGain;
    BYTE* seqHead;


    /* Compress literals */
    {
        size_t cSize;
        size_t litSize = seqStorePtr->lit - op_lit_start;

        if (litSize <= LITERAL_NOENTROPY)
            cSize = ZSTD_noCompressLiterals(op, maxDstSize, op_lit_start, litSize);
        else
            cSize = ZSTD_compressLiterals(op, maxDstSize, op_lit_start, litSize);
        if (ZSTD_isError(cSize)) return cSize;
        op += cSize;
    }

    /* Sequences Header */
    if ((oend-op) < MIN_SEQUENCES_SIZE)
        return ERROR(dstSize_tooSmall);
    MEM_writeLE16(op, (U16)nbSeq); op+=2;
    seqHead = op;

    /* dumps : contains too large lengths */
    {
        size_t dumpsLength = seqStorePtr->dumps - seqStorePtr->dumpsStart;
        if (dumpsLength < 512)
        {
            op[0] = (BYTE)(dumpsLength >> 8);
            op[1] = (BYTE)(dumpsLength);
            op += 2;
        }
        else
        {
            op[0] = 2;
            op[1] = (BYTE)(dumpsLength>>8);
            op[2] = (BYTE)(dumpsLength);
            op += 3;
        }
        if ((size_t)(oend-op) < dumpsLength+6) return ERROR(dstSize_tooSmall);
        memcpy(op, seqStorePtr->dumpsStart, dumpsLength);
        op += dumpsLength;
    }

    /* CTable for Literal Lengths */
    max = MaxLL;
    mostFrequent = FSE_countFast(count, &max, seqStorePtr->litLengthStart, nbSeq);
    if ((mostFrequent == nbSeq) && (nbSeq > 2))
    {
        *op++ = *(seqStorePtr->litLengthStart);
        FSE_buildCTable_rle(CTable_LitLength, (BYTE)max);
        LLtype = bt_rle;
    }
    else if ((nbSeq < 64) || (mostFrequent < (nbSeq >> (LLbits-1))))
    {
        FSE_buildCTable_raw(CTable_LitLength, LLbits);
        LLtype = bt_raw;
    }
    else
    {
        size_t NCountSize;
        tableLog = FSE_optimalTableLog(LLFSELog, nbSeq, max);
        FSE_normalizeCount(norm, tableLog, count, nbSeq, max);
        NCountSize = FSE_writeNCount(op, oend-op, norm, max, tableLog);   /* overflow protected */
        if (FSE_isError(NCountSize)) return ERROR(GENERIC);
        op += NCountSize;
        FSE_buildCTable(CTable_LitLength, norm, max, tableLog);
        LLtype = bt_compressed;
    }

    /* CTable for Offsets codes */
    {
        /* create Offset codes */
        size_t i;
        max = MaxOff;
        for (i=0; i<nbSeq; i++)
        {
            offCodeTable[i] = (BYTE)ZSTD_highbit(offsetTable[i]) + 1;
            if (offsetTable[i]==0) offCodeTable[i]=0;
        }
        mostFrequent = FSE_countFast(count, &max, offCodeTable, nbSeq);
    }
    if ((mostFrequent == nbSeq) && (nbSeq > 2))
    {
        *op++ = *offCodeTable;
        FSE_buildCTable_rle(CTable_OffsetBits, (BYTE)max);
        Offtype = bt_rle;
    }
    else if ((nbSeq < 64) || (mostFrequent < (nbSeq >> (Offbits-1))))
    {
        FSE_buildCTable_raw(CTable_OffsetBits, Offbits);
        Offtype = bt_raw;
    }
    else
    {
        size_t NCountSize;
        tableLog = FSE_optimalTableLog(OffFSELog, nbSeq, max);
        FSE_normalizeCount(norm, tableLog, count, nbSeq, max);
        NCountSize = FSE_writeNCount(op, oend-op, norm, max, tableLog);   /* overflow protected */
        if (FSE_isError(NCountSize)) return ERROR(GENERIC);
        op += NCountSize;
        FSE_buildCTable(CTable_OffsetBits, norm, max, tableLog);
        Offtype = bt_compressed;
    }

    /* CTable for MatchLengths */
    max = MaxML;
    mostFrequent = FSE_countFast(count, &max, seqStorePtr->matchLengthStart, nbSeq);
    if ((mostFrequent == nbSeq) && (nbSeq > 2))
    {
        *op++ = *seqStorePtr->matchLengthStart;
        FSE_buildCTable_rle(CTable_MatchLength, (BYTE)max);
        MLtype = bt_rle;
    }
    else if ((nbSeq < 64) || (mostFrequent < (nbSeq >> (MLbits-1))))
    {
        FSE_buildCTable_raw(CTable_MatchLength, MLbits);
        MLtype = bt_raw;
    }
    else
    {
        size_t NCountSize;
        tableLog = FSE_optimalTableLog(MLFSELog, nbSeq, max);
        FSE_normalizeCount(norm, tableLog, count, nbSeq, max);
        NCountSize = FSE_writeNCount(op, oend-op, norm, max, tableLog);   /* overflow protected */
        if (FSE_isError(NCountSize)) return ERROR(GENERIC);
        op += NCountSize;
        FSE_buildCTable(CTable_MatchLength, norm, max, tableLog);
        MLtype = bt_compressed;
    }

    seqHead[0] += (BYTE)((LLtype<<6) + (Offtype<<4) + (MLtype<<2));

    /* Encoding Sequences */
    {
        size_t streamSize, errorCode;
        BIT_CStream_t blockStream;
        FSE_CState_t stateMatchLength;
        FSE_CState_t stateOffsetBits;
        FSE_CState_t stateLitLength;
        int i;

        errorCode = BIT_initCStream(&blockStream, op, oend-op);
        if (ERR_isError(errorCode)) return ERROR(dstSize_tooSmall);   /* not enough space remaining */
        FSE_initCState(&stateMatchLength, CTable_MatchLength);
        FSE_initCState(&stateOffsetBits, CTable_OffsetBits);
        FSE_initCState(&stateLitLength, CTable_LitLength);

        for (i=(int)nbSeq-1; i>=0; i--)
        {
            BYTE matchLength = mlTable[i];
            U32  offset = offsetTable[i];
            BYTE offCode = offCodeTable[i];                                 /* 32b*/  /* 64b*/
            U32 nbBits = (offCode-1) * (!!offCode);
            BYTE litLength = llTable[i];                                    /* (7)*/  /* (7)*/
            FSE_encodeSymbol(&blockStream, &stateMatchLength, matchLength); /* 17 */  /* 17 */
            if (MEM_32bits()) BIT_flushBits(&blockStream);                  /*  7 */
            BIT_addBits(&blockStream, offset, nbBits);                      /* 31 */  /* 42 */   /* 24 bits max in 32-bits mode */
            if (MEM_32bits()) BIT_flushBits(&blockStream);                  /*  7 */
            FSE_encodeSymbol(&blockStream, &stateOffsetBits, offCode);      /* 16 */  /* 51 */
            FSE_encodeSymbol(&blockStream, &stateLitLength, litLength);     /* 26 */  /* 61 */
            BIT_flushBits(&blockStream);                                    /*  7 */  /*  7 */
        }

        FSE_flushCState(&blockStream, &stateMatchLength);
        FSE_flushCState(&blockStream, &stateOffsetBits);
        FSE_flushCState(&blockStream, &stateLitLength);

        streamSize = BIT_closeCStream(&blockStream);
        if (streamSize==0) return ERROR(dstSize_tooSmall);   /* not enough space */
        op += streamSize;
    }

    /* check compressibility */
    if ((size_t)(op-ostart) >= maxCSize) return 0;

    return op - ostart;
}


/** ZSTD_storeSeq
    Store a sequence (literal length, literals, offset code and match length) into seqStore_t
    @offsetCode : distance to match, or 0 == repCode
    @matchCode : matchLength - MINMATCH
*/
MEM_STATIC void ZSTD_storeSeq(seqStore_t* seqStorePtr, size_t litLength, const BYTE* literals, size_t offsetCode, size_t matchCode)
{
#if 0
    static const BYTE* g_start = NULL;
    if (g_start==NULL) g_start = literals;
    if (literals - g_start == 8695)
    printf("pos %6u : %3u literals & match %3u bytes at distance %6u \n",
           (U32)(literals - g_start), (U32)litLength, (U32)matchCode+4, (U32)offsetCode);
#endif

    /* copy Literals */
    ZSTD_wildcopy(seqStorePtr->lit, literals, litLength);
    seqStorePtr->lit += litLength;

    /* literal Length */
    if (litLength >= MaxLL)
    {
        *(seqStorePtr->litLength++) = MaxLL;
        if (litLength<255 + MaxLL)
            *(seqStorePtr->dumps++) = (BYTE)(litLength - MaxLL);
        else
        {
            *(seqStorePtr->dumps++) = 255;
            MEM_writeLE32(seqStorePtr->dumps, (U32)litLength); seqStorePtr->dumps += 3;
        }
    }
    else *(seqStorePtr->litLength++) = (BYTE)litLength;

    /* match offset */
    *(seqStorePtr->offset++) = (U32)offsetCode;

    /* match Length */
    if (matchCode >= MaxML)
    {
        *(seqStorePtr->matchLength++) = MaxML;
        if (matchCode < 255+MaxML)
            *(seqStorePtr->dumps++) = (BYTE)(matchCode - MaxML);
        else
        {
            *(seqStorePtr->dumps++) = 255;
            MEM_writeLE32(seqStorePtr->dumps, (U32)matchCode); seqStorePtr->dumps += 3;
        }
    }
    else *(seqStorePtr->matchLength++) = (BYTE)matchCode;
}


/* *************************************
*  Match length counter
***************************************/
static size_t ZSTD_read_ARCH(const void* p) { size_t r; memcpy(&r, p, sizeof(r)); return r; }

static unsigned ZSTD_highbit(U32 val)
{
#   if defined(_MSC_VER)   /* Visual */
    unsigned long r=0;
    _BitScanReverse(&r, val);
    return (unsigned)r;
#   elif defined(__GNUC__) && (__GNUC__ >= 3)   /* GCC Intrinsic */
    return 31 - __builtin_clz(val);
#   else   /* Software version */
    static const int DeBruijnClz[32] = { 0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30, 8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31 };
    U32 v = val;
    int r;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    r = DeBruijnClz[(U32)(v * 0x07C4ACDDU) >> 27];
    return r;
#   endif
}

static unsigned ZSTD_NbCommonBytes (register size_t val)
{
    if (MEM_isLittleEndian())
    {
        if (MEM_64bits())
        {
#       if defined(_MSC_VER) && defined(_WIN64)
            unsigned long r = 0;
            _BitScanForward64( &r, (U64)val );
            return (unsigned)(r>>3);
#       elif defined(__GNUC__) && (__GNUC__ >= 3)
            return (__builtin_ctzll((U64)val) >> 3);
#       else
            static const int DeBruijnBytePos[64] = { 0, 0, 0, 0, 0, 1, 1, 2, 0, 3, 1, 3, 1, 4, 2, 7, 0, 2, 3, 6, 1, 5, 3, 5, 1, 3, 4, 4, 2, 5, 6, 7, 7, 0, 1, 2, 3, 3, 4, 6, 2, 6, 5, 5, 3, 4, 5, 6, 7, 1, 2, 4, 6, 4, 4, 5, 7, 2, 6, 5, 7, 6, 7, 7 };
            return DeBruijnBytePos[((U64)((val & -(long long)val) * 0x0218A392CDABBD3FULL)) >> 58];
#       endif
        }
        else /* 32 bits */
        {
#       if defined(_MSC_VER)
            unsigned long r=0;
            _BitScanForward( &r, (U32)val );
            return (unsigned)(r>>3);
#       elif defined(__GNUC__) && (__GNUC__ >= 3)
            return (__builtin_ctz((U32)val) >> 3);
#       else
            static const int DeBruijnBytePos[32] = { 0, 0, 3, 0, 3, 1, 3, 0, 3, 2, 2, 1, 3, 2, 0, 1, 3, 3, 1, 2, 2, 2, 2, 0, 3, 1, 2, 0, 1, 0, 1, 1 };
            return DeBruijnBytePos[((U32)((val & -(S32)val) * 0x077CB531U)) >> 27];
#       endif
        }
    }
    else   /* Big Endian CPU */
    {
        if (MEM_64bits())
        {
#       if defined(_MSC_VER) && defined(_WIN64)
            unsigned long r = 0;
            _BitScanReverse64( &r, val );
            return (unsigned)(r>>3);
#       elif defined(__GNUC__) && (__GNUC__ >= 3)
            return (__builtin_clzll(val) >> 3);
#       else
            unsigned r;
            const unsigned n32 = sizeof(size_t)*4;   /* calculate this way due to compiler complaining in 32-bits mode */
            if (!(val>>n32)) { r=4; } else { r=0; val>>=n32; }
            if (!(val>>16)) { r+=2; val>>=8; } else { val>>=24; }
            r += (!val);
            return r;
#       endif
        }
        else /* 32 bits */
        {
#       if defined(_MSC_VER)
            unsigned long r = 0;
            _BitScanReverse( &r, (unsigned long)val );
            return (unsigned)(r>>3);
#       elif defined(__GNUC__) && (__GNUC__ >= 3)
            return (__builtin_clz((U32)val) >> 3);
#       else
            unsigned r;
            if (!(val>>16)) { r=2; val>>=8; } else { r=0; val>>=24; }
            r += (!val);
            return r;
#       endif
        }
    }
}


static size_t ZSTD_count(const BYTE* pIn, const BYTE* pMatch, const BYTE* pInLimit)
{
    const BYTE* const pStart = pIn;

    while ((pIn<pInLimit-(sizeof(size_t)-1)))
    {
        size_t diff = ZSTD_read_ARCH(pMatch) ^ ZSTD_read_ARCH(pIn);
        if (!diff) { pIn+=sizeof(size_t); pMatch+=sizeof(size_t); continue; }
        pIn += ZSTD_NbCommonBytes(diff);
        return (size_t)(pIn - pStart);
    }

    if (MEM_64bits()) if ((pIn<(pInLimit-3)) && (MEM_read32(pMatch) == MEM_read32(pIn))) { pIn+=4; pMatch+=4; }
    if ((pIn<(pInLimit-1)) && (MEM_read16(pMatch) == MEM_read16(pIn))) { pIn+=2; pMatch+=2; }
    if ((pIn<pInLimit) && (*pMatch == *pIn)) pIn++;
    return (size_t)(pIn - pStart);
}

/** ZSTD_count_2segments
*   can count match length with ip & match in potentially 2 different segments.
*   convention : on reaching mEnd, match count continue starting from iStart
*/
static size_t ZSTD_count_2segments(const BYTE* ip, const BYTE* match, const BYTE* iEnd, const BYTE* mEnd, const BYTE* iStart)
{
    size_t matchLength;
    const BYTE* vEnd = ip + (mEnd - match);
    if (vEnd > iEnd) vEnd = iEnd;
    matchLength = ZSTD_count(ip, match, vEnd);
    if (match + matchLength == mEnd)
        matchLength += ZSTD_count(ip+matchLength, iStart, iEnd);
    return matchLength;
}



/* *************************************
*  Hashes
***************************************/

static const U32 prime4bytes = 2654435761U;
static U32 ZSTD_hash4(U32 u, U32 h) { return (u * prime4bytes) >> (32-h) ; }
static size_t ZSTD_hash4Ptr(const void* ptr, U32 h) { return ZSTD_hash4(MEM_read32(ptr), h); }

static const U64 prime5bytes = 889523592379ULL;
static size_t ZSTD_hash5(U64 u, U32 h) { return (size_t)((u * prime5bytes) << (64-40) >> (64-h)) ; }
static size_t ZSTD_hash5Ptr(const void* p, U32 h) { return ZSTD_hash5(MEM_read64(p), h); }

static const U64 prime6bytes = 227718039650203ULL;
static size_t ZSTD_hash6(U64 u, U32 h) { return (size_t)((u * prime6bytes) << (64-48) >> (64-h)) ; }
static size_t ZSTD_hash6Ptr(const void* p, U32 h) { return ZSTD_hash6(MEM_read64(p), h); }

static const U64 prime7bytes = 58295818150454627ULL;
static size_t ZSTD_hash7(U64 u, U32 h) { return (size_t)((u * prime7bytes) << (64-56) >> (64-h)) ; }
static size_t ZSTD_hash7Ptr(const void* p, U32 h) { return ZSTD_hash7(MEM_read64(p), h); }

static size_t ZSTD_hashPtr(const void* p, U32 hBits, U32 mls)
{
    switch(mls)
    {
    default:
    case 4: return ZSTD_hash4Ptr(p, hBits);
    case 5: return ZSTD_hash5Ptr(p, hBits);
    case 6: return ZSTD_hash6Ptr(p, hBits);
    case 7: return ZSTD_hash7Ptr(p, hBits);
    }
}

/* *************************************
*  Fast Scan
***************************************/

#define FILLHASHSTEP 3
static void ZSTD_fillHashTable (ZSTD_CCtx* zc, const void* end, const U32 mls)
{
    U32* const hashTable = zc->hashTable;
    const U32 hBits = zc->params.hashLog;
    const BYTE* const base = zc->base;
    const BYTE* ip = base + zc->nextToUpdate;
    const BYTE* const iend = (const BYTE*) end;

    while(ip <= iend)
    {
        hashTable[ZSTD_hashPtr(ip, hBits, mls)] = (U32)(ip - base);
        ip += FILLHASHSTEP;
    }
}


FORCE_INLINE
size_t ZSTD_compressBlock_fast_generic(ZSTD_CCtx* zc,
                                       void* dst, size_t maxDstSize,
                                 const void* src, size_t srcSize,
                                 const U32 mls)
{
    U32* const hashTable = zc->hashTable;
    const U32 hBits = zc->params.hashLog;
    seqStore_t* seqStorePtr = &(zc->seqStore);
    const BYTE* const base = zc->base;
    const BYTE* const istart = (const BYTE*)src;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const U32 lowIndex = zc->dictLimit;
    const BYTE* const lowest = base + lowIndex;
    const BYTE* const iend = istart + srcSize;
    const BYTE* const ilimit = iend - 8;

    size_t offset_2=REPCODE_STARTVALUE, offset_1=REPCODE_STARTVALUE;


    /* init */
    ZSTD_resetSeqStore(seqStorePtr);
    if (ip < lowest+4)
    {
        hashTable[ZSTD_hashPtr(lowest+1, hBits, mls)] = lowIndex+1;
        hashTable[ZSTD_hashPtr(lowest+2, hBits, mls)] = lowIndex+2;
        hashTable[ZSTD_hashPtr(lowest+3, hBits, mls)] = lowIndex+3;
        ip = lowest+4;
    }

    /* Main Search Loop */
    while (ip < ilimit)  /* < instead of <=, because repcode check at (ip+1) */
    {
        size_t mlCode;
        size_t offset;
        const size_t h = ZSTD_hashPtr(ip, hBits, mls);
        const U32 matchIndex = hashTable[h];
        const BYTE* match = base + matchIndex;
        hashTable[h] = (U32)(ip-base);

        if (MEM_read32(ip+1-offset_1) == MEM_read32(ip+1))   /* note : by construction, offset_1 <= (ip-base) */
        {
            mlCode = ZSTD_count(ip+1+MINMATCH, ip+1+MINMATCH-offset_1, iend);
            ip++;
            offset = 0;
        }
        else
        {
            if ( (matchIndex <= lowIndex) ||
                 (MEM_read32(match) != MEM_read32(ip)) )
            {
                ip += ((ip-anchor) >> g_searchStrength) + 1;
                continue;
            }
            mlCode = ZSTD_count(ip+MINMATCH, match+MINMATCH, iend);
            offset = ip-match;
            while ((ip>anchor) && (match>lowest) && (ip[-1] == match[-1])) { ip--; match--; mlCode++; }  /* catch up */
            offset_2 = offset_1;
            offset_1 = offset;
        }

        /* match found */
        ZSTD_storeSeq(seqStorePtr, ip-anchor, anchor, offset, mlCode);
        ip += mlCode + MINMATCH;
        anchor = ip;

        if (ip <= ilimit)
        {
            /* Fill Table */
            hashTable[ZSTD_hashPtr(ip-(mlCode+MINMATCH)+2, hBits, mls)] = (U32)(ip-(mlCode+MINMATCH)+2-base);  /* here because ip-(mlCode+MINMATCH)+2 could be > iend-8 without ip <= ilimit check*/
            hashTable[ZSTD_hashPtr(ip-2, hBits, mls)] = (U32)(ip-2-base);
            /* check immediate repcode */
            while ( (ip <= ilimit)
                 && (MEM_read32(ip) == MEM_read32(ip - offset_2)) )
            {
                /* store sequence */
                size_t rlCode = ZSTD_count(ip+MINMATCH, ip+MINMATCH-offset_2, iend);
                size_t tmpOff = offset_2; offset_2 = offset_1; offset_1 = tmpOff;   /* swap offset_2 <=> offset_1 */
                hashTable[ZSTD_hashPtr(ip, hBits, mls)] = (U32)(ip-base);
                ZSTD_storeSeq(seqStorePtr, 0, anchor, 0, rlCode);
                ip += rlCode+MINMATCH;
                anchor = ip;
                continue;   /* faster when present ... (?) */
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
    return ZSTD_compressSequences(dst, maxDstSize,
                                  seqStorePtr, srcSize);
}


size_t ZSTD_compressBlock_fast(ZSTD_CCtx* ctx,
                               void* dst, size_t maxDstSize,
                         const void* src, size_t srcSize)
{
    const U32 mls = ctx->params.searchLength;
    switch(mls)
    {
    default:
    case 4 :
        return ZSTD_compressBlock_fast_generic(ctx, dst, maxDstSize, src, srcSize, 4);
    case 5 :
        return ZSTD_compressBlock_fast_generic(ctx, dst, maxDstSize, src, srcSize, 5);
    case 6 :
        return ZSTD_compressBlock_fast_generic(ctx, dst, maxDstSize, src, srcSize, 6);
    case 7 :
        return ZSTD_compressBlock_fast_generic(ctx, dst, maxDstSize, src, srcSize, 7);
    }
}


//FORCE_INLINE
size_t ZSTD_compressBlock_fast_extDict_generic(ZSTD_CCtx* ctx,
                                          void* dst, size_t maxDstSize,
                                    const void* src, size_t srcSize,
                                    const U32 mls)
{
    U32* hashTable = ctx->hashTable;
    const U32 hBits = ctx->params.hashLog;
    seqStore_t* seqStorePtr = &(ctx->seqStore);
    const BYTE* const base = ctx->base;
    const BYTE* const dictBase = ctx->dictBase;
    const BYTE* const istart = (const BYTE*)src;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const U32   lowLimit = ctx->lowLimit;
    const BYTE* const dictStart = dictBase + lowLimit;
    const U32   dictLimit = ctx->dictLimit;
    const BYTE* const lowPrefixPtr = base + dictLimit;
    const BYTE* const dictEnd = dictBase + dictLimit;
    const BYTE* const iend = istart + srcSize;
    const BYTE* const ilimit = iend - 8;

    U32 offset_2=REPCODE_STARTVALUE, offset_1=REPCODE_STARTVALUE;


    /* init */
    ZSTD_resetSeqStore(seqStorePtr);
    {
        /* skip first 4 positions to avoid read overflow during repcode match check */
        hashTable[ZSTD_hashPtr(ip+0, hBits, mls)] = (U32)(ip-base+0);
        hashTable[ZSTD_hashPtr(ip+1, hBits, mls)] = (U32)(ip-base+1);
        hashTable[ZSTD_hashPtr(ip+2, hBits, mls)] = (U32)(ip-base+2);
        hashTable[ZSTD_hashPtr(ip+3, hBits, mls)] = (U32)(ip-base+3);
        ip += 4;
    }

    /* Main Search Loop */
    while (ip < ilimit)  /* < instead of <=, because (ip+1) */
    {
        const size_t h = ZSTD_hashPtr(ip, hBits, mls);
        const U32 matchIndex = hashTable[h];
        const BYTE* matchBase = matchIndex < dictLimit ? dictBase : base;
        const BYTE* match = matchBase + matchIndex;
        const U32 current = (U32)(ip-base);
        const U32 repIndex = current + 1 - offset_1;
        const BYTE* repBase = repIndex < dictLimit ? dictBase : base;
        const BYTE* repMatch = repBase + repIndex;
        size_t mlCode;
        U32 offset;
        hashTable[h] = current;   /* update hash table */

        if ( ((repIndex <= dictLimit-4) || (repIndex >= dictLimit))
          && (MEM_read32(repMatch) == MEM_read32(ip+1)) )
        {
            const BYTE* repMatchEnd = repIndex < dictLimit ? dictEnd : iend;
            mlCode = ZSTD_count_2segments(ip+1+MINMATCH, repMatch+MINMATCH, iend, repMatchEnd, lowPrefixPtr);
            ip++;
            offset = 0;
        }
        else
        {
            if ( (matchIndex < lowLimit) ||
                 (MEM_read32(match) != MEM_read32(ip)) )
            { ip += ((ip-anchor) >> g_searchStrength) + 1; continue; }
            {
                const BYTE* matchEnd = matchIndex < dictLimit ? dictEnd : iend;
                const BYTE* lowMatchPtr = matchIndex < dictLimit ? dictStart : lowPrefixPtr;
                mlCode = ZSTD_count_2segments(ip+MINMATCH, match+MINMATCH, iend, matchEnd, lowPrefixPtr);
                while ((ip>anchor) && (match>lowMatchPtr) && (ip[-1] == match[-1])) { ip--; match--; mlCode++; }  /* catch up */
                offset = current - matchIndex;
                offset_2 = offset_1;
                offset_1 = offset;
            }
        }

        /* found a match : store it */
        ZSTD_storeSeq(seqStorePtr, ip-anchor, anchor, offset, mlCode);
        ip += mlCode + MINMATCH;
        anchor = ip;

        if (ip <= ilimit)
        {
            /* Fill Table */
			hashTable[ZSTD_hashPtr(base+current+2, hBits, mls)] = current+2;
            hashTable[ZSTD_hashPtr(ip-2, hBits, mls)] = (U32)(ip-2-base);
            /* check immediate repcode */
            while (ip <= ilimit)
            {
                U32 current2 = (U32)(ip-base);
                const U32 repIndex2 = current2 - offset_2;
                const BYTE* repMatch2 = repIndex2 < dictLimit ? dictBase + repIndex2 : base + repIndex2;
                if ( ((repIndex2 <= dictLimit-4) || (repIndex2 >= dictLimit))
                  && (MEM_read32(repMatch2) == MEM_read32(ip)) )
                {
                    const BYTE* const repEnd2 = repIndex2 < dictLimit ? dictEnd : iend;
                    size_t repLength2 = ZSTD_count_2segments(ip+MINMATCH, repMatch2+MINMATCH, iend, repEnd2, lowPrefixPtr);
                    U32 tmpOffset = offset_2; offset_2 = offset_1; offset_1 = tmpOffset;   /* swap offset_2 <=> offset_1 */
                    ZSTD_storeSeq(seqStorePtr, 0, anchor, 0, repLength2);
                    hashTable[ZSTD_hashPtr(ip, hBits, mls)] = current2;
                    ip += repLength2+MINMATCH;
                    anchor = ip;
                    continue;
                }
                break;
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
    return ZSTD_compressSequences(dst, maxDstSize,
                                  seqStorePtr, srcSize);
}


size_t ZSTD_compressBlock_fast_extDict(ZSTD_CCtx* ctx,
                               void* dst, size_t maxDstSize,
                         const void* src, size_t srcSize)
{
    const U32 mls = ctx->params.searchLength;
    switch(mls)
    {
    default:
    case 4 :
        return ZSTD_compressBlock_fast_extDict_generic(ctx, dst, maxDstSize, src, srcSize, 4);
    case 5 :
        return ZSTD_compressBlock_fast_extDict_generic(ctx, dst, maxDstSize, src, srcSize, 5);
    case 6 :
        return ZSTD_compressBlock_fast_extDict_generic(ctx, dst, maxDstSize, src, srcSize, 6);
    case 7 :
        return ZSTD_compressBlock_fast_extDict_generic(ctx, dst, maxDstSize, src, srcSize, 7);
    }
}


/* *************************************
*  Binary Tree search
***************************************/
/*! ZSTD_insertBt1 : add one or multiple positions to tree
*   @ip : assumed <= iend-8
*   @return : nb of positions added */
static U32 ZSTD_insertBt1(ZSTD_CCtx* zc, const BYTE* const ip, const U32 mls, const BYTE* const iend, U32 nbCompares)
{
    U32* const hashTable = zc->hashTable;
    const U32 hashLog = zc->params.hashLog;
    const size_t h  = ZSTD_hashPtr(ip, hashLog, mls);
    U32* const bt   = zc->contentTable;
    const U32 btLog = zc->params.contentLog - 1;
    const U32 btMask= (1 << btLog) - 1;
    U32 matchIndex  = hashTable[h];
    size_t commonLengthSmaller=0, commonLengthLarger=0;
    const BYTE* const base = zc->base;
    const BYTE* match = base + matchIndex;
    const U32 current = (U32)(ip-base);
    const U32 btLow = btMask >= current ? 0 : current - btMask;
    U32* smallerPtr = bt + 2*(current&btMask);
    U32* largerPtr  = bt + 2*(current&btMask) + 1;
    U32 dummy32;   /* to be nullified at the end */
    const U32 windowLow = zc->lowLimit;
    U32 matchEndIdx = current+8;

    hashTable[h] = current;   /* Update Hash Table */

    while (nbCompares-- && (matchIndex > windowLow))
    {
        U32* nextPtr = bt + 2*(matchIndex & btMask);
        size_t matchLength = MIN(commonLengthSmaller, commonLengthLarger);   /* guaranteed minimum nb of common bytes */

        match = base + matchIndex;
        if (match[matchLength] == ip[matchLength])
            matchLength += ZSTD_count(ip+matchLength+1, match+matchLength+1, iend) +1;

        if (matchLength > matchEndIdx - matchIndex)
            matchEndIdx = matchIndex + (U32)matchLength;

        if (ip+matchLength == iend)   /* equal : no way to know if inf or sup */
            break;   /* drop , to guarantee consistency ; miss a bit of compression, but required to not corrupt the tree */

        if (match[matchLength] < ip[matchLength])
        {
            /* match is smaller than current */
            *smallerPtr = matchIndex;             /* update smaller idx */
            commonLengthSmaller = matchLength;    /* all smaller will now have at least this guaranteed common length */
            if (matchIndex <= btLow) { smallerPtr=&dummy32; break; }   /* beyond tree size, stop the search */
            smallerPtr = nextPtr+1;               /* new "smaller" => larger of match */
            matchIndex = nextPtr[1];              /* new matchIndex larger than previous (closer to current) */
        }
        else
        {
            /* match is larger than current */
            *largerPtr = matchIndex;
            commonLengthLarger = matchLength;
            if (matchIndex <= btLow) { largerPtr=&dummy32; break; }   /* beyond tree size, stop the search */
            largerPtr = nextPtr;
            matchIndex = nextPtr[0];
        }
    }

    *smallerPtr = *largerPtr = 0;
    return (matchEndIdx > current + 8) ? matchEndIdx - current - 8 : 1;
}


FORCE_INLINE /* inlining is important to hardwire a hot branch (template emulation) */
size_t ZSTD_insertBtAndFindBestMatch (
                        ZSTD_CCtx* zc,
                        const BYTE* const ip, const BYTE* const iend,
                        size_t* offsetPtr,
                        U32 nbCompares, const U32 mls)
{
    U32* const hashTable = zc->hashTable;
    const U32 hashLog = zc->params.hashLog;
    const size_t h  = ZSTD_hashPtr(ip, hashLog, mls);
    U32* const bt   = zc->contentTable;
    const U32 btLog = zc->params.contentLog - 1;
    const U32 btMask= (1 << btLog) - 1;
    U32 matchIndex  = hashTable[h];
    size_t commonLengthSmaller=0, commonLengthLarger=0;
    const BYTE* const base = zc->base;
    const U32 current = (U32)(ip-base);
    const U32 btLow = btMask >= current ? 0 : current - btMask;
    const U32 windowLow = zc->lowLimit;
    U32* smallerPtr = bt + 2*(current&btMask);
    U32* largerPtr  = bt + 2*(current&btMask) + 1;
    size_t bestLength = 0;
    U32 matchEndIdx = current+8;
    U32 dummy32;   /* to be nullified at the end */

    hashTable[h] = current;   /* Update Hash Table */

    while (nbCompares-- && (matchIndex > windowLow))
    {
        U32* nextPtr = bt + 2*(matchIndex & btMask);
        const BYTE* match = base + matchIndex;
        size_t matchLength = MIN(commonLengthSmaller, commonLengthLarger);   /* guaranteed minimum nb of common bytes */

        if (match[matchLength] == ip[matchLength])
            matchLength += ZSTD_count(ip+matchLength+1, match+matchLength+1, iend) +1;

        if (matchLength > bestLength)
        {
            if (matchLength > matchEndIdx - matchIndex)
                matchEndIdx = matchIndex + (U32)matchLength;
            if ( (4*(int)(matchLength-bestLength)) > (int)(ZSTD_highbit(current-matchIndex+1) - ZSTD_highbit((U32)offsetPtr[0]+1)) )
                bestLength = matchLength, *offsetPtr = current - matchIndex;
            if (ip+matchLength == iend)   /* equal : no way to know if inf or sup */
                break;   /* drop, to guarantee consistency (miss a little bit of compression) */
        }

        if (match[matchLength] < ip[matchLength])
        {
            /* match is smaller than current */
            *smallerPtr = matchIndex;             /* update smaller idx */
            commonLengthSmaller = matchLength;    /* all smaller will now have at least this guaranteed common length */
            if (matchIndex <= btLow) { smallerPtr=&dummy32; break; }   /* beyond tree size, stop the search */
            smallerPtr = nextPtr+1;               /* new "smaller" => larger of match */
            matchIndex = nextPtr[1];              /* new matchIndex larger than previous (closer to current) */
        }
        else
        {
            /* match is larger than current */
            *largerPtr = matchIndex;
            commonLengthLarger = matchLength;
            if (matchIndex <= btLow) { largerPtr=&dummy32; break; }   /* beyond tree size, stop the search */
            largerPtr = nextPtr;
            matchIndex = nextPtr[0];
        }
    }

    *smallerPtr = *largerPtr = 0;

    zc->nextToUpdate = (matchEndIdx > current + 8) ? matchEndIdx - 8 : current+1;
    return bestLength;
}


static void ZSTD_updateTree(ZSTD_CCtx* zc, const BYTE* const ip, const BYTE* const iend, const U32 nbCompares, const U32 mls)
{
    const BYTE* const base = zc->base;
    const U32 target = (U32)(ip - base);
    U32 idx = zc->nextToUpdate;

    for( ; idx < target ; )
        idx += ZSTD_insertBt1(zc, base+idx, mls, iend, nbCompares);
}


/** Tree updater, providing best match */
FORCE_INLINE /* inlining is important to hardwire a hot branch (template emulation) */
size_t ZSTD_BtFindBestMatch (
                        ZSTD_CCtx* zc,
                        const BYTE* const ip, const BYTE* const iLimit,
                        size_t* offsetPtr,
                        const U32 maxNbAttempts, const U32 mls)
{
    if (ip < zc->base + zc->nextToUpdate) return 0;   /* skipped area */
    ZSTD_updateTree(zc, ip, iLimit, maxNbAttempts, mls);
    return ZSTD_insertBtAndFindBestMatch(zc, ip, iLimit, offsetPtr, maxNbAttempts, mls);
}


FORCE_INLINE size_t ZSTD_BtFindBestMatch_selectMLS (
                        ZSTD_CCtx* zc,   /* Index table will be updated */
                        const BYTE* ip, const BYTE* const iLimit,
                        size_t* offsetPtr,
                        const U32 maxNbAttempts, const U32 matchLengthSearch)
{
    switch(matchLengthSearch)
    {
    default :
    case 4 : return ZSTD_BtFindBestMatch(zc, ip, iLimit, offsetPtr, maxNbAttempts, 4);
    case 5 : return ZSTD_BtFindBestMatch(zc, ip, iLimit, offsetPtr, maxNbAttempts, 5);
    case 6 : return ZSTD_BtFindBestMatch(zc, ip, iLimit, offsetPtr, maxNbAttempts, 6);
    }
}


/** ZSTD_insertBt1_extDict : add one or multiple positions to tree
*   @ip : assumed <= iend-8
*   @return : nb of positions added */
static U32 ZSTD_insertBt1_extDict(ZSTD_CCtx* zc, const BYTE* const ip, const U32 mls, const BYTE* const iend, U32 nbCompares)
{
    U32* const hashTable = zc->hashTable;
    const U32 hashLog = zc->params.hashLog;
    const size_t h  = ZSTD_hashPtr(ip, hashLog, mls);
    U32* const bt   = zc->contentTable;
    const U32 btLog = zc->params.contentLog - 1;
    const U32 btMask= (1 << btLog) - 1;
    U32 matchIndex  = hashTable[h];
    size_t commonLengthSmaller=0, commonLengthLarger=0;
    const BYTE* const base = zc->base;
    const BYTE* const dictBase = zc->dictBase;
    const U32 dictLimit = zc->dictLimit;
    const BYTE* const dictEnd = dictBase + dictLimit;
    const BYTE* const prefixStart = base + dictLimit;
    const BYTE* match = base + matchIndex;
    const U32 current = (U32)(ip-base);
    const U32 btLow = btMask >= current ? 0 : current - btMask;
    U32* smallerPtr = bt + 2*(current&btMask);
    U32* largerPtr  = bt + 2*(current&btMask) + 1;
    U32 dummy32;   /* to be nullified at the end */
    const U32 windowLow = zc->lowLimit;
    U32 matchEndIdx = current+8;

    hashTable[h] = current;   /* Update Hash Table */

    while (nbCompares-- && (matchIndex > windowLow))
    {
        U32* nextPtr = bt + 2*(matchIndex & btMask);
        size_t matchLength = MIN(commonLengthSmaller, commonLengthLarger);   /* guaranteed minimum nb of common bytes */

        if (matchIndex+matchLength >= dictLimit)
        {
            match = base + matchIndex;
            if (match[matchLength] == ip[matchLength])
                matchLength += ZSTD_count(ip+matchLength+1, match+matchLength+1, iend) +1;
        }
        else
        {
            match = dictBase + matchIndex;
            matchLength += ZSTD_count_2segments(ip+matchLength, match+matchLength, iend, dictEnd, prefixStart);
            if (matchIndex+matchLength >= dictLimit)
				match = base + matchIndex;   /* to prepare for next usage of match[matchLength] */
        }

        if (matchLength > matchEndIdx - matchIndex)
            matchEndIdx = matchIndex + (U32)matchLength;

        if (ip+matchLength == iend)   /* equal : no way to know if inf or sup */
            break;   /* drop , to guarantee consistency ; miss a bit of compression, but other solutions can corrupt the tree */

        if (match[matchLength] < ip[matchLength])   /* necessarily within correct buffer */
        {
            /* match is smaller than current */
            *smallerPtr = matchIndex;             /* update smaller idx */
            commonLengthSmaller = matchLength;    /* all smaller will now have at least this guaranteed common length */
            if (matchIndex <= btLow) { smallerPtr=&dummy32; break; }   /* beyond tree size, stop the search */
            smallerPtr = nextPtr+1;               /* new "smaller" => larger of match */
            matchIndex = nextPtr[1];              /* new matchIndex larger than previous (closer to current) */
        }
        else
        {
            /* match is larger than current */
            *largerPtr = matchIndex;
            commonLengthLarger = matchLength;
            if (matchIndex <= btLow) { largerPtr=&dummy32; break; }   /* beyond tree size, stop the search */
            largerPtr = nextPtr;
            matchIndex = nextPtr[0];
        }
    }

    *smallerPtr = *largerPtr = 0;
    return (matchEndIdx > current + 8) ? matchEndIdx - current - 8 : 1;
}


static void ZSTD_updateTree_extDict(ZSTD_CCtx* zc, const BYTE* const ip, const BYTE* const iend, const U32 nbCompares, const U32 mls)
{
    const BYTE* const base = zc->base;
    const U32 target = (U32)(ip - base);
    U32 idx = zc->nextToUpdate;

    for( ; idx < target ; )
        idx += ZSTD_insertBt1_extDict(zc, base+idx, mls, iend, nbCompares);
}


FORCE_INLINE /* inlining is important to hardwire a hot branch (template emulation) */
size_t ZSTD_insertBtAndFindBestMatch_extDict (
                        ZSTD_CCtx* zc,
                        const BYTE* const ip, const BYTE* const iend,
                        size_t* offsetPtr,
                        U32 nbCompares, const U32 mls)
{
    U32* const hashTable = zc->hashTable;
    const U32 hashLog = zc->params.hashLog;
    const size_t h  = ZSTD_hashPtr(ip, hashLog, mls);
    U32* const bt   = zc->contentTable;
    const U32 btLog = zc->params.contentLog - 1;
    const U32 btMask= (1 << btLog) - 1;
    U32 matchIndex  = hashTable[h];
    size_t commonLengthSmaller=0, commonLengthLarger=0;
    const BYTE* const base = zc->base;
    const BYTE* const dictBase = zc->dictBase;
    const U32 dictLimit = zc->dictLimit;
    const BYTE* const dictEnd = dictBase + dictLimit;
    const BYTE* const prefixStart = base + dictLimit;
    const U32 current = (U32)(ip-base);
    const U32 btLow = btMask >= current ? 0 : current - btMask;
    const U32 windowLow = zc->lowLimit;
    U32* smallerPtr = bt + 2*(current&btMask);
    U32* largerPtr  = bt + 2*(current&btMask) + 1;
    size_t bestLength = 0;
    U32 matchEndIdx = current+8;
    U32 dummy32;   /* to be nullified at the end */

    hashTable[h] = current;   /* Update Hash Table */

    while (nbCompares-- && (matchIndex > windowLow))
    {
        U32* nextPtr = bt + 2*(matchIndex & btMask);
        size_t matchLength = MIN(commonLengthSmaller, commonLengthLarger);   /* guaranteed minimum nb of common bytes */
        const BYTE* match;

        if (matchIndex+matchLength >= dictLimit)
        {
            match = base + matchIndex;
            if (match[matchLength] == ip[matchLength])
                matchLength += ZSTD_count(ip+matchLength+1, match+matchLength+1, iend) +1;
        }
        else
        {
            match = dictBase + matchIndex;
            matchLength += ZSTD_count_2segments(ip+matchLength, match+matchLength, iend, dictEnd, prefixStart);
            if (matchIndex+matchLength >= dictLimit)
				match = base + matchIndex;   /* to prepare for next usage of match[matchLength] */
        }

        if (matchLength > bestLength)
        {
            if (matchLength > matchEndIdx - matchIndex)
                matchEndIdx = matchIndex + (U32)matchLength;
            if ( (4*(int)(matchLength-bestLength)) > (int)(ZSTD_highbit(current-matchIndex+1) - ZSTD_highbit((U32)offsetPtr[0]+1)) )
                bestLength = matchLength, *offsetPtr = current - matchIndex;
            if (ip+matchLength == iend)   /* equal : no way to know if inf or sup */
                break;   /* drop, to guarantee consistency (miss a little bit of compression) */
        }

        if (match[matchLength] < ip[matchLength])
        {
            /* match is smaller than current */
            *smallerPtr = matchIndex;             /* update smaller idx */
            commonLengthSmaller = matchLength;    /* all smaller will now have at least this guaranteed common length */
            if (matchIndex <= btLow) { smallerPtr=&dummy32; break; }   /* beyond tree size, stop the search */
            smallerPtr = nextPtr+1;               /* new "smaller" => larger of match */
            matchIndex = nextPtr[1];              /* new matchIndex larger than previous (closer to current) */
        }
        else
        {
            /* match is larger than current */
            *largerPtr = matchIndex;
            commonLengthLarger = matchLength;
            if (matchIndex <= btLow) { largerPtr=&dummy32; break; }   /* beyond tree size, stop the search */
            largerPtr = nextPtr;
            matchIndex = nextPtr[0];
        }
    }

    *smallerPtr = *largerPtr = 0;

    zc->nextToUpdate = (matchEndIdx > current + 8) ? matchEndIdx - 8 : current+1;
    return bestLength;
}

/** Tree updater, providing best match */
FORCE_INLINE /* inlining is important to hardwire a hot branch (template emulation) */
size_t ZSTD_BtFindBestMatch_extDict (
                        ZSTD_CCtx* zc,
                        const BYTE* const ip, const BYTE* const iLimit,
                        size_t* offsetPtr,
                        const U32 maxNbAttempts, const U32 mls)
{
    if (ip < zc->base + zc->nextToUpdate) return 0;   /* skipped area */
    ZSTD_updateTree_extDict(zc, ip, iLimit, maxNbAttempts, mls);
    return ZSTD_insertBtAndFindBestMatch_extDict(zc, ip, iLimit, offsetPtr, maxNbAttempts, mls);
}


FORCE_INLINE size_t ZSTD_BtFindBestMatch_selectMLS_extDict (
                        ZSTD_CCtx* zc,   /* Index table will be updated */
                        const BYTE* ip, const BYTE* const iLimit,
                        size_t* offsetPtr,
                        const U32 maxNbAttempts, const U32 matchLengthSearch)
{
    switch(matchLengthSearch)
    {
    default :
    case 4 : return ZSTD_BtFindBestMatch_extDict(zc, ip, iLimit, offsetPtr, maxNbAttempts, 4);
    case 5 : return ZSTD_BtFindBestMatch_extDict(zc, ip, iLimit, offsetPtr, maxNbAttempts, 5);
    case 6 : return ZSTD_BtFindBestMatch_extDict(zc, ip, iLimit, offsetPtr, maxNbAttempts, 6);
    }
}


/* ***********************
*  Hash Chain
*************************/

#define NEXT_IN_CHAIN(d, mask)   chainTable[(d) & mask]

/* Update chains up to ip (excluded)
   Assumption : always within prefix (ie. not within extDict) */
static U32 ZSTD_insertAndFindFirstIndex (ZSTD_CCtx* zc, const BYTE* ip, U32 mls)
{
    U32* const hashTable  = zc->hashTable;
    const U32 hashLog = zc->params.hashLog;
    U32* const chainTable = zc->contentTable;
    const U32 chainMask = (1 << zc->params.contentLog) - 1;
    const BYTE* const base = zc->base;
    const U32 target = (U32)(ip - base);
    U32 idx = zc->nextToUpdate;

    while(idx < target)
    {
        size_t h = ZSTD_hashPtr(base+idx, hashLog, mls);
        NEXT_IN_CHAIN(idx, chainMask) = hashTable[h];
        hashTable[h] = idx;
        idx++;
    }

    zc->nextToUpdate = target;
    return hashTable[ZSTD_hashPtr(ip, hashLog, mls)];
}


FORCE_INLINE /* inlining is important to hardwire a hot branch (template emulation) */
size_t ZSTD_HcFindBestMatch_generic (
                        ZSTD_CCtx* zc,   /* Index table will be updated */
                        const BYTE* const ip, const BYTE* const iLimit,
                        size_t* offsetPtr,
                        const U32 maxNbAttempts, const U32 mls, const U32 extDict)
{
    U32* const chainTable = zc->contentTable;
    const U32 chainSize = (1 << zc->params.contentLog);
    const U32 chainMask = chainSize-1;
    const BYTE* const base = zc->base;
    const BYTE* const dictBase = zc->dictBase;
    const U32 dictLimit = zc->dictLimit;
    const BYTE* const prefixStart = base + dictLimit;
    const BYTE* const dictEnd = dictBase + dictLimit;
    const U32 lowLimit = zc->lowLimit;
    const U32 current = (U32)(ip-base);
    const U32 minChain = current > chainSize ? current - chainSize : 0;
    U32 matchIndex;
    const BYTE* match;
    int nbAttempts=maxNbAttempts;
    size_t ml=MINMATCH-1;

    /* HC4 match finder */
    matchIndex = ZSTD_insertAndFindFirstIndex (zc, ip, mls);

    while ((matchIndex>lowLimit) && (nbAttempts))
    {
        size_t currentMl=0;
        nbAttempts--;
        if ((!extDict) || matchIndex >= dictLimit)
        {
            match = base + matchIndex;
            if (match[ml] == ip[ml])   /* potentially better */
                currentMl = ZSTD_count(ip, match, iLimit);
        }
        else
        {
            match = dictBase + matchIndex;
            if (MEM_read32(match) == MEM_read32(ip))   /* assumption : matchIndex <= dictLimit-4 (by table construction) */
                currentMl = ZSTD_count_2segments(ip+MINMATCH, match+MINMATCH, iLimit, dictEnd, prefixStart) + MINMATCH;
        }

        /* save best solution */
        if (currentMl > ml) { ml = currentMl; *offsetPtr = current - matchIndex; if (ip+currentMl == iLimit) break; /* best possible, and avoid read overflow*/ }

        if (matchIndex <= minChain) break;
        matchIndex = NEXT_IN_CHAIN(matchIndex, chainMask);
    }

    return ml;
}


FORCE_INLINE size_t ZSTD_HcFindBestMatch_selectMLS (
                        ZSTD_CCtx* zc,
                        const BYTE* ip, const BYTE* const iLimit,
                        size_t* offsetPtr,
                        const U32 maxNbAttempts, const U32 matchLengthSearch)
{
    switch(matchLengthSearch)
    {
    default :
    case 4 : return ZSTD_HcFindBestMatch_generic(zc, ip, iLimit, offsetPtr, maxNbAttempts, 4, 0);
    case 5 : return ZSTD_HcFindBestMatch_generic(zc, ip, iLimit, offsetPtr, maxNbAttempts, 5, 0);
    case 6 : return ZSTD_HcFindBestMatch_generic(zc, ip, iLimit, offsetPtr, maxNbAttempts, 6, 0);
    }
}


FORCE_INLINE size_t ZSTD_HcFindBestMatch_extDict_selectMLS (
                        ZSTD_CCtx* zc,
                        const BYTE* ip, const BYTE* const iLimit,
                        size_t* offsetPtr,
                        const U32 maxNbAttempts, const U32 matchLengthSearch)
{
    switch(matchLengthSearch)
    {
    default :
    case 4 : return ZSTD_HcFindBestMatch_generic(zc, ip, iLimit, offsetPtr, maxNbAttempts, 4, 1);
    case 5 : return ZSTD_HcFindBestMatch_generic(zc, ip, iLimit, offsetPtr, maxNbAttempts, 5, 1);
    case 6 : return ZSTD_HcFindBestMatch_generic(zc, ip, iLimit, offsetPtr, maxNbAttempts, 6, 1);
    }
}


/* *******************************
*  Common parser - lazy strategy
*********************************/
FORCE_INLINE
size_t ZSTD_compressBlock_lazy_generic(ZSTD_CCtx* ctx,
                                     void* dst, size_t maxDstSize, const void* src, size_t srcSize,
                                     const U32 searchMethod, const U32 depth)
{
    seqStore_t* seqStorePtr = &(ctx->seqStore);
    const BYTE* const istart = (const BYTE*)src;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const BYTE* const iend = istart + srcSize;
    const BYTE* const ilimit = iend - 8;
    const BYTE* const base = ctx->base + ctx->dictLimit;

    size_t offset_2=REPCODE_STARTVALUE, offset_1=REPCODE_STARTVALUE;
    const U32 maxSearches = 1 << ctx->params.searchLog;
    const U32 mls = ctx->params.searchLength;

    typedef size_t (*searchMax_f)(ZSTD_CCtx* zc, const BYTE* ip, const BYTE* iLimit,
                        size_t* offsetPtr,
                        U32 maxNbAttempts, U32 matchLengthSearch);
    searchMax_f searchMax = searchMethod ? ZSTD_BtFindBestMatch_selectMLS : ZSTD_HcFindBestMatch_selectMLS;

    /* init */
    ZSTD_resetSeqStore(seqStorePtr);
    if ((ip-base) < REPCODE_STARTVALUE) ip = base + REPCODE_STARTVALUE;

    /* Match Loop */
    while (ip < ilimit)
    {
        size_t matchLength=0;
        size_t offset=0;
        const BYTE* start=ip+1;

        /* check repCode */
        if (MEM_read32(ip+1) == MEM_read32(ip+1 - offset_1))
        {
            /* repcode : we take it */
            matchLength = ZSTD_count(ip+1+MINMATCH, ip+1+MINMATCH-offset_1, iend) + MINMATCH;
            if (depth==0) goto _storeSequence;
        }

        {
            /* first search (depth 0) */
            size_t offsetFound = 99999999;
            size_t ml2 = searchMax(ctx, ip, iend, &offsetFound, maxSearches, mls);
            if (ml2 > matchLength)
                matchLength = ml2, start = ip, offset=offsetFound;
        }

        if (matchLength < MINMATCH)
        {
            ip += ((ip-anchor) >> g_searchStrength) + 1;   /* jump faster over incompressible sections */
            continue;
        }

        /* let's try to find a better solution */
        if (depth>=1)
        while (ip<ilimit)
        {
            ip ++;
            if ((offset) && (MEM_read32(ip) == MEM_read32(ip - offset_1)))
            {
                size_t mlRep = ZSTD_count(ip+MINMATCH, ip+MINMATCH-offset_1, iend) + MINMATCH;
                int gain2 = (int)(mlRep * 3);
                int gain1 = (int)(matchLength*3 - ZSTD_highbit((U32)offset+1) + 1);
                if ((mlRep >= MINMATCH) && (gain2 > gain1))
                    matchLength = mlRep, offset = 0, start = ip;
            }
            {
                size_t offset2=999999;
                size_t ml2 = searchMax(ctx, ip, iend, &offset2, maxSearches, mls);
                int gain2 = (int)(ml2*4 - ZSTD_highbit((U32)offset2+1));   /* raw approx */
                int gain1 = (int)(matchLength*4 - ZSTD_highbit((U32)offset+1) + 4);
                if ((ml2 >= MINMATCH) && (gain2 > gain1))
                {
                    matchLength = ml2, offset = offset2, start = ip;
                    continue;   /* search a better one */
                }
            }

            /* let's find an even better one */
            if ((depth==2) && (ip<ilimit))
            {
                ip ++;
                if ((offset) && (MEM_read32(ip) == MEM_read32(ip - offset_1)))
                {
                    size_t ml2 = ZSTD_count(ip+MINMATCH, ip+MINMATCH-offset_1, iend) + MINMATCH;
                    int gain2 = (int)(ml2 * 4);
                    int gain1 = (int)(matchLength*4 - ZSTD_highbit((U32)offset+1) + 1);
                    if ((ml2 >= MINMATCH) && (gain2 > gain1))
                        matchLength = ml2, offset = 0, start = ip;
                }
                {
                    size_t offset2=999999;
                    size_t ml2 = searchMax(ctx, ip, iend, &offset2, maxSearches, mls);
                    int gain2 = (int)(ml2*4 - ZSTD_highbit((U32)offset2+1));   /* raw approx */
                    int gain1 = (int)(matchLength*4 - ZSTD_highbit((U32)offset+1) + 7);
                    if ((ml2 >= MINMATCH) && (gain2 > gain1))
                    {
                        matchLength = ml2, offset = offset2, start = ip;
                        continue;
                    }
                }
            }
            break;  /* nothing found : store previous solution */
        }

        /* catch up */
        if (offset)
        {
            while ((start>anchor) && (start>base+offset) && (start[-1] == start[-1-offset]))   /* only search for offset within prefix */
                { start--; matchLength++; }
            offset_2 = offset_1; offset_1 = offset;
        }

        /* store sequence */
_storeSequence:
        {
            size_t litLength = start - anchor;
            ZSTD_storeSeq(seqStorePtr, litLength, anchor, offset, matchLength-MINMATCH);
            anchor = ip = start + matchLength;
        }

        /* check immediate repcode */
        while ( (ip <= ilimit)
             && (MEM_read32(ip) == MEM_read32(ip - offset_2)) )
        {
            /* store sequence */
            matchLength = ZSTD_count(ip+MINMATCH, ip+MINMATCH-offset_2, iend);
            offset = offset_2;
            offset_2 = offset_1;
            offset_1 = offset;
            ZSTD_storeSeq(seqStorePtr, 0, anchor, 0, matchLength);
            ip += matchLength+MINMATCH;
            anchor = ip;
            continue;   /* faster when present ... (?) */
        }
    }

    /* Last Literals */
    {
        size_t lastLLSize = iend - anchor;
        memcpy(seqStorePtr->lit, anchor, lastLLSize);
        seqStorePtr->lit += lastLLSize;
    }

    /* Final compression stage */
    return ZSTD_compressSequences(dst, maxDstSize,
                                  seqStorePtr, srcSize);
}

size_t ZSTD_compressBlock_btlazy2(ZSTD_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_generic(ctx, dst, maxDstSize, src, srcSize, 1, 2);
}

size_t ZSTD_compressBlock_lazy2(ZSTD_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_generic(ctx, dst, maxDstSize, src, srcSize, 0, 2);
}

size_t ZSTD_compressBlock_lazy(ZSTD_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_generic(ctx, dst, maxDstSize, src, srcSize, 0, 1);
}

size_t ZSTD_compressBlock_greedy(ZSTD_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_generic(ctx, dst, maxDstSize, src, srcSize, 0, 0);
}


FORCE_INLINE
size_t ZSTD_compressBlock_lazy_extDict_generic(ZSTD_CCtx* ctx,
                                     void* dst, size_t maxDstSize, const void* src, size_t srcSize,
                                     const U32 searchMethod, const U32 depth)
{
    seqStore_t* seqStorePtr = &(ctx->seqStore);
    const BYTE* const istart = (const BYTE*)src;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const BYTE* const iend = istart + srcSize;
    const BYTE* const ilimit = iend - 8;
    const BYTE* const base = ctx->base;
    const U32 dictLimit = ctx->dictLimit;
    const BYTE* const prefixStart = base + dictLimit;
    const BYTE* const dictBase = ctx->dictBase;
    const BYTE* const dictEnd  = dictBase + dictLimit;
    const BYTE* const dictStart  = dictBase + ctx->lowLimit;

    size_t offset_2=REPCODE_STARTVALUE, offset_1=REPCODE_STARTVALUE;
    const U32 maxSearches = 1 << ctx->params.searchLog;
    const U32 mls = ctx->params.searchLength;

    typedef size_t (*searchMax_f)(ZSTD_CCtx* zc, const BYTE* ip, const BYTE* iLimit,
                        size_t* offsetPtr,
                        U32 maxNbAttempts, U32 matchLengthSearch);
    searchMax_f searchMax = searchMethod ? ZSTD_BtFindBestMatch_selectMLS_extDict : ZSTD_HcFindBestMatch_extDict_selectMLS;

    /* init */
    ZSTD_resetSeqStore(seqStorePtr);
    if ((ip - prefixStart) < REPCODE_STARTVALUE) ip += REPCODE_STARTVALUE;

    /* Match Loop */
    while (ip < ilimit)
    {
        size_t matchLength=0;
        size_t offset=0;
        const BYTE* start=ip+1;
        U32 current = (U32)(ip-base);

        /* check repCode */
        {
            const U32 repIndex = (U32)(current+1 - offset_1);
            const BYTE* const repBase = repIndex < dictLimit ? dictBase : base;
            const BYTE* const repMatch = repBase + repIndex;
            if ((U32)((dictLimit-1) - repIndex) >= 3)   /* intentional overflow */
            if (MEM_read32(ip+1) == MEM_read32(repMatch))
            {
                /* repcode detected we should take it */
                const BYTE* const repEnd = repIndex < dictLimit ? dictEnd : iend;
                matchLength = ZSTD_count_2segments(ip+1+MINMATCH, repMatch+MINMATCH, iend, repEnd, prefixStart) + MINMATCH;
                if (depth==0) goto _storeSequence;
            }
        }

        {
            /* first search (depth 0) */
            size_t offsetFound = 99999999;
            size_t ml2 = searchMax(ctx, ip, iend, &offsetFound, maxSearches, mls);
            if (ml2 > matchLength)
                matchLength = ml2, start = ip, offset=offsetFound;
        }

         if (matchLength < MINMATCH)
        {
            ip += ((ip-anchor) >> g_searchStrength) + 1;   /* jump faster over incompressible sections */
            continue;
        }

        /* let's try to find a better solution */
        if (depth>=1)
        while (ip<ilimit)
        {
            ip ++;
            current++;
            /* check repCode */
            if (offset)
            {
                const U32 repIndex = (U32)(current - offset_1);
                const BYTE* const repBase = repIndex < dictLimit ? dictBase : base;
                const BYTE* const repMatch = repBase + repIndex;
                if ((U32)((dictLimit-1) - repIndex) >= 3)   /* intentional overflow */
                if (MEM_read32(ip) == MEM_read32(repMatch))
                {
                    /* repcode detected */
                    const BYTE* const repEnd = repIndex < dictLimit ? dictEnd : iend;
                    size_t repLength = ZSTD_count_2segments(ip+MINMATCH, repMatch+MINMATCH, iend, repEnd, prefixStart) + MINMATCH;
                    int gain2 = (int)(repLength * 3);
                    int gain1 = (int)(matchLength*3 - ZSTD_highbit((U32)offset+1) + 1);
                    if ((repLength >= MINMATCH) && (gain2 > gain1))
                        matchLength = repLength, offset = 0, start = ip;
                }
            }

            /* search match, depth 1 */
            {
                size_t offset2=999999;
                size_t ml2 = searchMax(ctx, ip, iend, &offset2, maxSearches, mls);
                int gain2 = (int)(ml2*4 - ZSTD_highbit((U32)offset2+1));   /* raw approx */
                int gain1 = (int)(matchLength*4 - ZSTD_highbit((U32)offset+1) + 4);
                if ((ml2 >= MINMATCH) && (gain2 > gain1))
                {
                    matchLength = ml2, offset = offset2, start = ip;
                    continue;   /* search a better one */
                }
            }

            /* let's find an even better one */
            if ((depth==2) && (ip<ilimit))
            {
                ip ++;
                current++;
                /* check repCode */
                if (offset)
                {
                    const U32 repIndex = (U32)(current - offset_1);
                    const BYTE* const repBase = repIndex < dictLimit ? dictBase : base;
                    const BYTE* const repMatch = repBase + repIndex;
                    if ((U32)((dictLimit-1) - repIndex) >= 3)   /* intentional overflow */
                    if (MEM_read32(ip) == MEM_read32(repMatch))
                    {
                        /* repcode detected */
                        const BYTE* const repEnd = repIndex < dictLimit ? dictEnd : iend;
                        size_t repLength = ZSTD_count_2segments(ip+MINMATCH, repMatch+MINMATCH, iend, repEnd, prefixStart) + MINMATCH;
                        int gain2 = (int)(repLength * 4);
                        int gain1 = (int)(matchLength*4 - ZSTD_highbit((U32)offset+1) + 1);
                        if ((repLength >= MINMATCH) && (gain2 > gain1))
                            matchLength = repLength, offset = 0, start = ip;
                    }
                }

                /* search match, depth 2 */
                {
                    size_t offset2=999999;
                    size_t ml2 = searchMax(ctx, ip, iend, &offset2, maxSearches, mls);
                    int gain2 = (int)(ml2*4 - ZSTD_highbit((U32)offset2+1));   /* raw approx */
                    int gain1 = (int)(matchLength*4 - ZSTD_highbit((U32)offset+1) + 7);
                    if ((ml2 >= MINMATCH) && (gain2 > gain1))
                    {
                        matchLength = ml2, offset = offset2, start = ip;
                        continue;
                    }
                }
            }
            break;  /* nothing found : store previous solution */
        }

        /* catch up */
        if (offset)
        {
            U32 matchIndex = (U32)((start-base) - offset);
            const BYTE* match = (matchIndex < dictLimit) ? dictBase + matchIndex : base + matchIndex;
            const BYTE* const mStart = (matchIndex < dictLimit) ? dictStart : prefixStart;
            while ((start>anchor) && (match>mStart) && (start[-1] == match[-1])) { start--; match--; matchLength++; }  /* catch up */
            offset_2 = offset_1; offset_1 = offset;
        }

        /* store sequence */
_storeSequence:
        {
            size_t litLength = start - anchor;
            ZSTD_storeSeq(seqStorePtr, litLength, anchor, offset, matchLength-MINMATCH);
            anchor = ip = start + matchLength;
        }

        /* check immediate repcode */
        while (ip <= ilimit)
        {
            const U32 repIndex = (U32)((ip-base) - offset_2);
            const BYTE* const repBase = repIndex < dictLimit ? dictBase : base;
            const BYTE* const repMatch = repBase + repIndex;
            if ((U32)((dictLimit-1) - repIndex) >= 3)   /* intentional overflow */
            if (MEM_read32(ip) == MEM_read32(repMatch))
            {
                /* repcode detected we should take it */
                const BYTE* const repEnd = repIndex < dictLimit ? dictEnd : iend;
                matchLength = ZSTD_count_2segments(ip+MINMATCH, repMatch+MINMATCH, iend, repEnd, prefixStart) + MINMATCH;
                offset = offset_2; offset_2 = offset_1; offset_1 = offset;   /* swap offset history */
                ZSTD_storeSeq(seqStorePtr, 0, anchor, 0, matchLength-MINMATCH);
                ip += matchLength;
                anchor = ip;
                continue;   /* faster when present ... (?) */
            }
            break;
        }
    }

    /* Last Literals */
    {
        size_t lastLLSize = iend - anchor;
        memcpy(seqStorePtr->lit, anchor, lastLLSize);
        seqStorePtr->lit += lastLLSize;
    }

    /* Final compression stage */
    return ZSTD_compressSequences(dst, maxDstSize,
                                  seqStorePtr, srcSize);
}

size_t ZSTD_compressBlock_greedy_extDict(ZSTD_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_extDict_generic(ctx, dst, maxDstSize, src, srcSize, 0, 0);
}

size_t ZSTD_compressBlock_lazy_extDict(ZSTD_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_extDict_generic(ctx, dst, maxDstSize, src, srcSize, 0, 1);
}

size_t ZSTD_compressBlock_lazy2_extDict(ZSTD_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_extDict_generic(ctx, dst, maxDstSize, src, srcSize, 0, 2);
}

static size_t ZSTD_compressBlock_btlazy2_extDict(ZSTD_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_extDict_generic(ctx, dst, maxDstSize, src, srcSize, 1, 2);
}


typedef size_t (*ZSTD_blockCompressor) (ZSTD_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize);

static ZSTD_blockCompressor ZSTD_selectBlockCompressor(ZSTD_strategy strat, int extDict)
{
    static const ZSTD_blockCompressor blockCompressor[2][5] = {
        { ZSTD_compressBlock_fast, ZSTD_compressBlock_greedy, ZSTD_compressBlock_lazy,ZSTD_compressBlock_lazy2, ZSTD_compressBlock_btlazy2 },
        { ZSTD_compressBlock_fast_extDict, ZSTD_compressBlock_greedy_extDict, ZSTD_compressBlock_lazy_extDict,ZSTD_compressBlock_lazy2_extDict, ZSTD_compressBlock_btlazy2_extDict }
    };

    return blockCompressor[extDict][(U32)strat];
}


size_t ZSTD_compressBlock(ZSTD_CCtx* zc, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    ZSTD_blockCompressor blockCompressor = ZSTD_selectBlockCompressor(zc->params.strategy, zc->lowLimit < zc->dictLimit);
    if (srcSize < MIN_CBLOCK_SIZE+3) return 0;   /* don't even attempt compression below a certain srcSize */
    return blockCompressor(zc, dst, maxDstSize, src, srcSize);
}


static size_t ZSTD_compress_generic (ZSTD_CCtx* ctxPtr,
                                        void* dst, size_t maxDstSize,
                                  const void* src, size_t srcSize)
{
    size_t blockSize = ctxPtr->blockSize;
    size_t remaining = srcSize;
    const BYTE* ip = (const BYTE*)src;
    BYTE* const ostart = (BYTE*)dst;
    BYTE* op = ostart;
    const U32 maxDist = 1 << ctxPtr->params.windowLog;

    while (remaining)
    {
        size_t cSize;

        if (maxDstSize < 3 + MIN_CBLOCK_SIZE) return ERROR(dstSize_tooSmall);   /* not enough space to store compressed block */
        if (remaining < blockSize) blockSize = remaining;

        if ((U32)(ip+blockSize - (ctxPtr->base + ctxPtr->lowLimit)) > maxDist)
        {
            /* respect windowLog contract */
            ctxPtr->lowLimit = (U32)(ip+blockSize - ctxPtr->base) - maxDist;
            if (ctxPtr->dictLimit < ctxPtr->lowLimit) ctxPtr->dictLimit = ctxPtr->lowLimit;
        }

        cSize = ZSTD_compressBlock(ctxPtr, op+3, maxDstSize-3, ip, blockSize);
        if (ZSTD_isError(cSize)) return cSize;

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
        maxDstSize -= cSize;
        ip += blockSize;
        op += cSize;
    }

    return op-ostart;
}


size_t ZSTD_compressContinue (ZSTD_CCtx* zc,
                                 void* dst, size_t dstSize,
                           const void* src, size_t srcSize)
{
    const BYTE* const ip = (const BYTE*) src;

    /* Check if blocks follow each other */
    if (src != zc->nextSrc)
    {
        /* not contiguous */
        size_t delta = zc->nextSrc - ip;
        zc->lowLimit = zc->dictLimit;
        zc->dictLimit = (U32)(zc->nextSrc - zc->base);
        zc->dictBase = zc->base;
        zc->base -= delta;
        zc->nextToUpdate = zc->dictLimit;
        if (zc->dictLimit - zc->lowLimit < 8) zc->lowLimit = zc->dictLimit;   /* too small extDict */
    }

    /* preemptive overflow correction */
    if (zc->lowLimit > (1<<30))
    {
        U32 btplus = (zc->params.strategy == ZSTD_btlazy2);
        U32 contentMask = (1 << (zc->params.contentLog - btplus)) - 1;
        U32 newLowLimit = zc->lowLimit & contentMask;   /* preserve position % contentSize */
        U32 correction = zc->lowLimit - newLowLimit;
        ZSTD_reduceIndex(zc, correction);
        zc->base += correction;
        zc->dictBase += correction;
        zc->lowLimit = newLowLimit;
        zc->dictLimit -= correction;
        if (zc->nextToUpdate < correction) zc->nextToUpdate = 0;
        else zc->nextToUpdate -= correction;
    }

    /* input-dictionary overlap */
    if ((ip+srcSize > zc->dictBase + zc->lowLimit) && (ip < zc->dictBase + zc->dictLimit))
    {
        zc->lowLimit = (U32)(ip + srcSize - zc->dictBase);
        if (zc->lowLimit > zc->dictLimit) zc->lowLimit = zc->dictLimit;
    }

    zc->nextSrc = ip + srcSize;

    return ZSTD_compress_generic (zc, dst, dstSize, src, srcSize);
}

size_t ZSTD_compress_insertDictionary(ZSTD_CCtx* zc, const void* src, size_t srcSize)
{
    const BYTE* const ip = (const BYTE*) src;
    const BYTE* const iend = ip + srcSize;

    /* input becomes current prefix */
    zc->lowLimit = zc->dictLimit;
    zc->dictLimit = (U32)(zc->nextSrc - zc->base);
    zc->dictBase = zc->base;
    zc->base += ip - zc->nextSrc;
    zc->nextToUpdate = zc->dictLimit;

    zc->nextSrc = iend;
    if (srcSize <= 8) return 0;

    switch(zc->params.strategy)
    {
    case ZSTD_fast:
        ZSTD_fillHashTable (zc, iend-8, zc->params.searchLength);
        break;

    case ZSTD_greedy:
    case ZSTD_lazy:
    case ZSTD_lazy2:
        ZSTD_insertAndFindFirstIndex (zc, iend-8, zc->params.searchLength);
        break;

    case ZSTD_btlazy2:
        ZSTD_updateTree(zc, iend-8, iend, 1 << zc->params.searchLog, zc->params.searchLength);
        zc->nextToUpdate = (U32)(iend - zc->base);
        break;

    default:
        return ERROR(GENERIC);   /* strategy doesn't exist; impossible */
    }

    return 0;
}


/*! ZSTD_compressBegin_advanced
*   Write frame header, according to params
*   @return : nb of bytes written */
size_t ZSTD_compressBegin_advanced(ZSTD_CCtx* ctx,
                                   void* dst, size_t maxDstSize,
                                   ZSTD_parameters params)
{
    size_t errorCode;

    ZSTD_validateParams(&params);

    if (maxDstSize < ZSTD_frameHeaderSize_max) return ERROR(dstSize_tooSmall);
    errorCode = ZSTD_resetCCtx_advanced(ctx, params);
    if (ZSTD_isError(errorCode)) return errorCode;

    MEM_writeLE32(dst, ZSTD_MAGICNUMBER); /* Write Header */
    ((BYTE*)dst)[4] = (BYTE)(params.windowLog - ZSTD_WINDOWLOG_ABSOLUTEMIN);
    return ZSTD_frameHeaderSize_min;
}


/** ZSTD_getParams
*   return ZSTD_parameters structure for a selected compression level and srcSize.
*   srcSizeHint value is optional, select 0 if not known */
ZSTD_parameters ZSTD_getParams(int compressionLevel, U64 srcSizeHint)
{
    ZSTD_parameters result;
    int tableID = ((srcSizeHint-1) <= 256 KB) + ((srcSizeHint-1) <= 128 KB) + ((srcSizeHint-1) <= 16 KB);   /* intentional underflow for srcSizeHint == 0 */
    if (compressionLevel<=0) compressionLevel = 1;
    if (compressionLevel > ZSTD_MAX_CLEVEL) compressionLevel = ZSTD_MAX_CLEVEL;
    result = ZSTD_defaultParameters[tableID][compressionLevel];
    result.srcSize = srcSizeHint;
    return result;
}


size_t ZSTD_compressBegin(ZSTD_CCtx* ctx, void* dst, size_t maxDstSize, int compressionLevel)
{
    return ZSTD_compressBegin_advanced(ctx, dst, maxDstSize, ZSTD_getParams(compressionLevel, 0));
}


/*! ZSTD_compressEnd
*   Write frame epilogue
*   @return : nb of bytes written into dst (or an error code) */
size_t ZSTD_compressEnd(ZSTD_CCtx* ctx, void* dst, size_t maxDstSize)
{
    BYTE* op = (BYTE*)dst;

    /* Sanity check */
    (void)ctx;
    if (maxDstSize < 3) return ERROR(dstSize_tooSmall);

    /* End of frame */
    op[0] = (BYTE)(bt_end << 6);
    op[1] = 0;
    op[2] = 0;

    return 3;
}

size_t ZSTD_compress_advanced (ZSTD_CCtx* ctx,
                               void* dst, size_t maxDstSize,
                         const void* src, size_t srcSize,
                         const void* dict,size_t dictSize,
                               ZSTD_parameters params)
{
    BYTE* const ostart = (BYTE*)dst;
    BYTE* op = ostart;
    size_t oSize;

    /* Header */
    oSize = ZSTD_compressBegin_advanced(ctx, dst, maxDstSize, params);
    if(ZSTD_isError(oSize)) return oSize;
    op += oSize;
    maxDstSize -= oSize;

    /* dictionary */
    if (dict)
    {
        oSize = ZSTD_compress_insertDictionary(ctx, dict, dictSize);
        if (ZSTD_isError(oSize)) return oSize;
    }

    /* body (compression) */
    oSize = ZSTD_compressContinue (ctx, op,  maxDstSize, src, srcSize);
    if(ZSTD_isError(oSize)) return oSize;
    op += oSize;
    maxDstSize -= oSize;

    /* Close frame */
    oSize = ZSTD_compressEnd(ctx, op, maxDstSize);
    if(ZSTD_isError(oSize)) return oSize;
    op += oSize;

    return (op - ostart);
}

size_t ZSTD_compress_usingDict(ZSTD_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize, const void* dict, size_t dictSize, int compressionLevel)
{
    return ZSTD_compress_advanced(ctx, dst, maxDstSize, src, srcSize, dict, dictSize, ZSTD_getParams(compressionLevel, srcSize+dictSize));
}

size_t ZSTD_compressCCtx (ZSTD_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize, int compressionLevel)
{
    return ZSTD_compress_advanced(ctx, dst, maxDstSize, src, srcSize, NULL, 0, ZSTD_getParams(compressionLevel, srcSize));
}

size_t ZSTD_compress(void* dst, size_t maxDstSize, const void* src, size_t srcSize, int compressionLevel)
{
    size_t result;
    ZSTD_CCtx ctxBody;
    memset(&ctxBody, 0, sizeof(ctxBody));
    result = ZSTD_compressCCtx(&ctxBody, dst, maxDstSize, src, srcSize, compressionLevel);
    free(ctxBody.workSpace);   /* can't free ctxBody, since it's on stack; free heap content */
    return result;
}

