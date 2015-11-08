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
#include "zstdhc_static.h"
#include "zstd_static.h"
#include "zstd_internal.h"
#include "mem.h"


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

struct ZSTD_HC_CCtx_s
{
    const BYTE* end;        /* next block here to continue on current prefix */
    const BYTE* base;       /* All regular indexes relative to this position */
    const BYTE* dictBase;   /* extDict indexes relative to this position */
    U32   dictLimit;        /* below that point, need extDict */
    U32   lowLimit;         /* below that point, no more data */
    U32   nextToUpdate;     /* index from which to continue dictionary update */
    ZSTD_HC_parameters params;
    void* workSpace;
    size_t workSpaceSize;

    seqStore_t seqStore;    /* sequences storage ptrs */
    U32* hashTable;
    U32* contentTable;
};


ZSTD_HC_CCtx* ZSTD_HC_createCCtx(void)
{
    return (ZSTD_HC_CCtx*) calloc(1, sizeof(ZSTD_HC_CCtx));
}

size_t ZSTD_HC_freeCCtx(ZSTD_HC_CCtx* cctx)
{
    free(cctx->workSpace);
    free(cctx);
    return 0;
}


/** ZSTD_HC_validateParams
    correct params value to remain within authorized range
    optimize for srcSize if srcSize > 0 */
void ZSTD_HC_validateParams(ZSTD_HC_parameters* params, size_t srcSize)
{
    const U32 btPlus = (params->strategy == ZSTD_HC_btlazy2);

    /* validate params */
    if (params->windowLog   > ZSTD_HC_WINDOWLOG_MAX) params->windowLog = ZSTD_HC_WINDOWLOG_MAX;
    if (params->windowLog   < ZSTD_HC_WINDOWLOG_MIN) params->windowLog = ZSTD_HC_WINDOWLOG_MIN;

    /* correct params, to use less memory */
    if (srcSize > 0)
    {
        U32 srcLog = ZSTD_highbit((U32)srcSize-1) + 1;
        if (params->windowLog > srcLog) params->windowLog = srcLog;
    }

    if (params->contentLog  > params->windowLog+btPlus) params->contentLog = params->windowLog+btPlus;   /* <= ZSTD_HC_CONTENTLOG_MAX */
    if (params->contentLog  < ZSTD_HC_CONTENTLOG_MIN) params->contentLog = ZSTD_HC_CONTENTLOG_MIN;
    if (params->hashLog     > ZSTD_HC_HASHLOG_MAX) params->hashLog = ZSTD_HC_HASHLOG_MAX;
    if (params->hashLog     < ZSTD_HC_HASHLOG_MIN) params->hashLog = ZSTD_HC_HASHLOG_MIN;
    if (params->searchLog   > ZSTD_HC_SEARCHLOG_MAX) params->searchLog = ZSTD_HC_SEARCHLOG_MAX;
    if (params->searchLog   < ZSTD_HC_SEARCHLOG_MIN) params->searchLog = ZSTD_HC_SEARCHLOG_MIN;
    if (params->searchLength> ZSTD_HC_SEARCHLENGTH_MAX) params->searchLength = ZSTD_HC_SEARCHLENGTH_MAX;
    if (params->searchLength< ZSTD_HC_SEARCHLENGTH_MIN) params->searchLength = ZSTD_HC_SEARCHLENGTH_MIN;
    if ((U32)params->strategy>(U32)ZSTD_HC_btlazy2) params->strategy = ZSTD_HC_btlazy2;
}


static size_t ZSTD_HC_resetCCtx_advanced (ZSTD_HC_CCtx* zc,
                                          ZSTD_HC_parameters params)
{
    ZSTD_HC_validateParams(&params, 0);

    /* reserve table memory */
    {
        const U32 contentLog = params.strategy == ZSTD_HC_fast ? 1 : params.contentLog;
        const size_t tableSpace = ((1 << contentLog) + (1 << params.hashLog)) * sizeof(U32);
        const size_t neededSpace = tableSpace + WORKPLACESIZE;
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
    zc->end = NULL;
    zc->base = NULL;
    zc->dictBase = NULL;
    zc->dictLimit = 0;
    zc->lowLimit = 0;
    zc->params = params;
    zc->seqStore.offsetStart = (U32*) (zc->seqStore.buffer);
    zc->seqStore.offCodeStart = (BYTE*) (zc->seqStore.offsetStart + (BLOCKSIZE>>2));
    zc->seqStore.litStart = zc->seqStore.offCodeStart + (BLOCKSIZE>>2);
    zc->seqStore.litLengthStart =  zc->seqStore.litStart + BLOCKSIZE;
    zc->seqStore.matchLengthStart = zc->seqStore.litLengthStart + (BLOCKSIZE>>2);
    zc->seqStore.dumpsStart = zc->seqStore.matchLengthStart + (BLOCKSIZE>>2);

    return 0;
}


/* *************************************
*  Inline functions and Macros
***************************************/

static const U32 prime4bytes = 2654435761U;
static U32 ZSTD_HC_hash4(U32 u, U32 h) { return (u * prime4bytes) >> (32-h) ; }
static size_t ZSTD_HC_hash4Ptr(const void* ptr, U32 h) { return ZSTD_HC_hash4(MEM_read32(ptr), h); }

static const U64 prime5bytes = 889523592379ULL;
static size_t ZSTD_HC_hash5(U64 u, U32 h) { return (size_t)((u * prime5bytes) << (64-40) >> (64-h)) ; }
static size_t ZSTD_HC_hash5Ptr(const void* p, U32 h) { return ZSTD_HC_hash5(MEM_read64(p), h); }

static const U64 prime6bytes = 227718039650203ULL;
static size_t ZSTD_HC_hash6(U64 u, U32 h) { return (size_t)((u * prime6bytes) << (64-48) >> (64-h)) ; }
static size_t ZSTD_HC_hash6Ptr(const void* p, U32 h) { return ZSTD_HC_hash6(MEM_read64(p), h); }

static const U64 prime7bytes =    58295818150454627ULL;
static size_t ZSTD_HC_hash7(U64 u, U32 h) { return (size_t)((u * prime7bytes) << (64-56) >> (64-h)) ; }
static size_t ZSTD_HC_hash7Ptr(const void* p, U32 h) { return ZSTD_HC_hash7(MEM_read64(p), h); }

static size_t ZSTD_HC_hashPtr(const void* p, U32 hBits, U32 mls)
{
    switch(mls)
    {
    default:
    case 4: return ZSTD_HC_hash4Ptr(p, hBits);
    case 5: return ZSTD_HC_hash5Ptr(p, hBits);
    case 6: return ZSTD_HC_hash6Ptr(p, hBits);
    case 7: return ZSTD_HC_hash7Ptr(p, hBits);
    }
}

/* *************************************
*  Fast Scan
***************************************/

FORCE_INLINE
size_t ZSTD_HC_compressBlock_fast_generic(ZSTD_HC_CCtx* ctx,
                                          void* dst, size_t maxDstSize,
                                    const void* src, size_t srcSize,
                                    const U32 mls)
{
    U32* hashTable = ctx->hashTable;
    const U32 hBits = ctx->params.hashLog;
    seqStore_t* seqStorePtr = &(ctx->seqStore);
    const BYTE* const base = ctx->base;
    const size_t maxDist = ((size_t)1 << ctx->params.windowLog);

    const BYTE* const istart = (const BYTE*)src;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const BYTE* const lowest = (size_t)(istart-base) > maxDist ? istart-maxDist : base;
    const BYTE* const iend = istart + srcSize;
    const BYTE* const ilimit = iend - 8;

    size_t offset_2=4, offset_1=4;


    /* init */
    if (ip == base)
    {
        hashTable[ZSTD_HC_hashPtr(base+1, hBits, mls)] = 1;
        hashTable[ZSTD_HC_hashPtr(base+2, hBits, mls)] = 2;
        hashTable[ZSTD_HC_hashPtr(base+3, hBits, mls)] = 3;
        ip = base+4;
    }
    ZSTD_resetSeqStore(seqStorePtr);

    /* Main Search Loop */
    while (ip < ilimit)  /* < instead of <=, because unconditionnal ZSTD_addPtr(ip+1) */
    {
        const size_t h = ZSTD_HC_hashPtr(ip, hBits, mls);
        const BYTE* match = base + hashTable[h];
        hashTable[h] = (U32)(ip-base);

        if (MEM_read32(ip-offset_2) == MEM_read32(ip)) match = ip-offset_2;
        if ( (match < lowest) ||
             (MEM_read32(match) != MEM_read32(ip)) )
        { ip += ((ip-anchor) >> g_searchStrength) + 1; offset_2 = offset_1; continue; }
        while ((ip>anchor) && (match>base) && (ip[-1] == match[-1])) { ip--; match--; }  /* catch up */

        {
            size_t litLength = ip-anchor;
            size_t matchLength = ZSTD_count(ip+MINMATCH, match+MINMATCH, iend);
            size_t offsetCode = ip-match;
            if (offsetCode == offset_2) offsetCode = 0;
            offset_2 = offset_1;
            offset_1 = ip-match;
            ZSTD_storeSeq(seqStorePtr, litLength, anchor, offsetCode, matchLength);

            /* Fill Table */
            hashTable[ZSTD_HC_hashPtr(ip+1, hBits, mls)] = (U32)(ip+1-base);
            ip += matchLength + MINMATCH;
            anchor = ip;
            if (ip < ilimit) /* same test as loop, for speed */
                hashTable[ZSTD_HC_hashPtr(ip-2, hBits, mls)] = (U32)(ip-2-base);
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


size_t ZSTD_HC_compressBlock_fast(ZSTD_HC_CCtx* ctx,
                               void* dst, size_t maxDstSize,
                         const void* src, size_t srcSize)
{
    const U32 mls = ctx->params.searchLength;
    switch(mls)
    {
    default:
    case 4 :
        return ZSTD_HC_compressBlock_fast_generic(ctx, dst, maxDstSize, src, srcSize, 4);
    case 5 :
        return ZSTD_HC_compressBlock_fast_generic(ctx, dst, maxDstSize, src, srcSize, 5);
    case 6 :
        return ZSTD_HC_compressBlock_fast_generic(ctx, dst, maxDstSize, src, srcSize, 6);
    case 7 :
        return ZSTD_HC_compressBlock_fast_generic(ctx, dst, maxDstSize, src, srcSize, 7);
    }
}


/* *************************************
*  Binary Tree search
***************************************/
/** ZSTD_HC_insertBt1 : add one ptr to tree
    @ip : assumed <= iend-8 */
static U32 ZSTD_HC_insertBt1(ZSTD_HC_CCtx* zc, const BYTE* const ip, const U32 mls, const BYTE* const iend, U32 nbCompares)
{
    U32* const hashTable = zc->hashTable;
    const U32 hashLog = zc->params.hashLog;
    const size_t h  = ZSTD_HC_hashPtr(ip, hashLog, mls);
    U32* const bt   = zc->contentTable;
    const U32 btLog = zc->params.contentLog - 1;
    const U32 btMask= (1 << btLog) - 1;
    U32 matchIndex  = hashTable[h];
    size_t commonLengthSmaller=0, commonLengthLarger=0;
    const BYTE* const base = zc->base;
    const BYTE* match = base + matchIndex;
    U32 current = (U32)(ip-base);
    const U32 btLow = btMask >= current ? 0 : current - btMask;
    U32* smallerPtr = bt + 2*(current&btMask);
    U32* largerPtr  = bt + 2*(current&btMask) + 1;
    U32 dummy32;   /* to be nullified at the end */
    const U32 windowSize = 1 << zc->params.windowLog;
    const U32 windowLow = windowSize >= current ? 0 : current - windowSize;

    if ((current-matchIndex == 1)   /* RLE */
        && ZSTD_read_ARCH(match) == ZSTD_read_ARCH(ip))
    {
        size_t rleLength = ZSTD_count(ip+sizeof(size_t), match+sizeof(size_t), iend) + sizeof(size_t);
        return (U32)(rleLength - mls);
    }

    hashTable[h] = (U32)(ip - base);   /* Update Hash Table */

    while (nbCompares-- && (matchIndex > windowLow))
    {
        U32* nextPtr = bt + 2*(matchIndex & btMask);
        size_t matchLength = MIN(commonLengthSmaller, commonLengthLarger);   /* guaranteed minimum nb of common bytes */

        match = base + matchIndex;
        matchLength += ZSTD_count(ip+matchLength, match+matchLength, iend);

        if (ip+matchLength == iend)   /* equal : no way to know if inf or sup */
            break;   /* just drop , to guarantee consistency (miss a bit of compression; if someone knows better, please tell) */

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
    return 1;
}


FORCE_INLINE /* inlining is important to hardwire a hot branch (template emulation) */
size_t ZSTD_HC_insertBtAndFindBestMatch (
                        ZSTD_HC_CCtx* zc,
                        const BYTE* const ip, const BYTE* const iend,
                        size_t* offsetPtr,
                        U32 nbCompares, const U32 mls)
{
    U32* const hashTable = zc->hashTable;
    const U32 hashLog = zc->params.hashLog;
    const size_t h  = ZSTD_HC_hashPtr(ip, hashLog, mls);
    U32* const bt   = zc->contentTable;
    const U32 btLog = zc->params.contentLog - 1;
    const U32 btMask= (1 << btLog) - 1;
    U32 matchIndex  = hashTable[h];
    size_t commonLengthSmaller=0, commonLengthLarger=0;
    const BYTE* const base = zc->base;
    const U32 current = (U32)(ip-base);
    const U32 btLow = btMask >= current ? 0 : current - btMask;
    const U32 windowSize = 1 << zc->params.windowLog;
    const U32 windowLow = windowSize >= current ? 0 : current - windowSize;
    U32* smallerPtr = bt + 2*(current&btMask);
    U32* largerPtr  = bt + 2*(current&btMask) + 1;
    size_t bestLength = 0;
    U32 dummy32;   /* to be nullified at the end */

    hashTable[h] = (U32)(ip-base);   /* Update Hash Table */

    while (nbCompares-- && (matchIndex > windowLow))
    {
        U32* nextPtr = bt + 2*(matchIndex & btMask);
        const BYTE* match = base + matchIndex;
        size_t matchLength = MIN(commonLengthSmaller, commonLengthLarger);   /* guaranteed minimum nb of common bytes */

        matchLength += ZSTD_count(ip+matchLength, match+matchLength, iend);

        if (matchLength > bestLength)
        {
            if ( (4*(int)(matchLength-bestLength)) > (int)(ZSTD_highbit(current-matchIndex+1) - ZSTD_highbit((U32)offsetPtr[0]+1)) )
                bestLength = matchLength, *offsetPtr = current - matchIndex;
            if (ip+matchLength == iend)   /* equal : no way to know if inf or sup */
                break;   /* just drop, to guarantee consistency (miss a little bit of compression) */
        }

        if (match[matchLength] < ip[matchLength])
        {
            /* match is smaller than current */
            *smallerPtr = matchIndex;             /* update smaller idx */
            commonLengthSmaller = matchLength;    /* all smaller will now have at least this guaranteed common length */
            smallerPtr = nextPtr+1;               /* new "smaller" => larger of match */
            if (matchIndex <= btLow) smallerPtr=&dummy32;  /* beyond tree size, stop the search */
            matchIndex = (matchIndex <= btLow) ? windowLow : nextPtr[1];
        }
        else
        {
            /* match is larger than current */
            *largerPtr = matchIndex;
            commonLengthLarger = matchLength;
            largerPtr = nextPtr;
            if (matchIndex <= btLow) largerPtr=&dummy32; /* beyond tree size, stop the search */
            matchIndex = (matchIndex <= btLow) ? windowLow : nextPtr[0];
        }
    }

    *smallerPtr = *largerPtr = 0;

    zc->nextToUpdate = current+1;   /* current has been inserted */
    if (bestLength < MINMATCH) return 0;
    return bestLength;
}


static const BYTE* ZSTD_HC_updateTree(ZSTD_HC_CCtx* zc, const BYTE* const ip, const BYTE* const iend, const U32 nbCompares, const U32 mls)
{
    const BYTE* const base = zc->base;
    const U32 target = (U32)(ip - base);
    U32 idx = zc->nextToUpdate;
    //size_t dummy;

    for( ; idx < target ; )
        idx += ZSTD_HC_insertBt1(zc, base+idx, mls, iend, nbCompares);
        //ZSTD_HC_insertBtAndFindBestMatch(zc, base+idx, iend, &dummy, nbCompares, mls);

    zc->nextToUpdate = idx;
    return base + idx;
}


/** Tree updater, providing best match */
FORCE_INLINE /* inlining is important to hardwire a hot branch (template emulation) */
size_t ZSTD_HC_BtFindBestMatch (
                        ZSTD_HC_CCtx* zc,
                        const BYTE* const ip, const BYTE* const iLimit,
                        size_t* offsetPtr,
                        const U32 maxNbAttempts, const U32 mls)
{
    const BYTE* nextToUpdate = ZSTD_HC_updateTree(zc, ip, iLimit, maxNbAttempts, mls);
    if (nextToUpdate > ip)
    {
        /* RLE data */
        *offsetPtr = 1;
        return ZSTD_count(ip, ip-1, iLimit);
    }
    return ZSTD_HC_insertBtAndFindBestMatch(zc, ip, iLimit, offsetPtr, maxNbAttempts, mls);
}


FORCE_INLINE size_t ZSTD_HC_BtFindBestMatch_selectMLS (
                        ZSTD_HC_CCtx* zc,   /* Index table will be updated */
                        const BYTE* ip, const BYTE* const iLimit,
                        size_t* offsetPtr,
                        const U32 maxNbAttempts, const U32 matchLengthSearch)
{
    switch(matchLengthSearch)
    {
    default :
    case 4 : return ZSTD_HC_BtFindBestMatch(zc, ip, iLimit, offsetPtr, maxNbAttempts, 4);
    case 5 : return ZSTD_HC_BtFindBestMatch(zc, ip, iLimit, offsetPtr, maxNbAttempts, 5);
    case 6 : return ZSTD_HC_BtFindBestMatch(zc, ip, iLimit, offsetPtr, maxNbAttempts, 6);
    }
}


/* ***********************
*  Hash Chain
*************************/

#define NEXT_IN_CHAIN(d, mask)   chainTable[(d) & mask]

/* Update chains up to ip (excluded) */
static U32 ZSTD_HC_insertAndFindFirstIndex  (ZSTD_HC_CCtx* zc, const BYTE* ip, U32 mls)
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
        size_t h = ZSTD_HC_hashPtr(base+idx, hashLog, mls);
        NEXT_IN_CHAIN(idx, chainMask) = hashTable[h];
        hashTable[h] = idx;
        idx++;
    }

    zc->nextToUpdate = target;
    return hashTable[ZSTD_HC_hashPtr(ip, hashLog, mls)];
}


FORCE_INLINE /* inlining is important to hardwire a hot branch (template emulation) */
size_t ZSTD_HC_HcFindBestMatch (
                        ZSTD_HC_CCtx* zc,   /* Index table will be updated */
                        const BYTE* const ip, const BYTE* const iLimit,
                        size_t* offsetPtr,
                        const U32 maxNbAttempts, const U32 matchLengthSearch)
{
    U32* const chainTable = zc->contentTable;
    const U32 chainSize = (1 << zc->params.contentLog);
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
    matchIndex = ZSTD_HC_insertAndFindFirstIndex (zc, ip, matchLengthSearch);

    while ((matchIndex>lowLimit) && (nbAttempts))
    {
        nbAttempts--;
        if (matchIndex >= dictLimit)
        {
            match = base + matchIndex;
            if ( (match[ml] == ip[ml])
              && (MEM_read32(match) == MEM_read32(ip)) )   /* ensures minimum match of 4 */
            {
                const size_t mlt = ZSTD_count(ip+MINMATCH, match+MINMATCH, iLimit) + MINMATCH;
                if (mlt > ml)
                //if (((int)(4*mlt) - (int)ZSTD_highbit((U32)(ip-match)+1)) > ((int)(4*ml) - (int)ZSTD_highbit((U32)((*offsetPtr)+1))))
                {
                    ml = mlt; *offsetPtr = ip-match;
                    if (ip+ml >= iLimit) break;
                }
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
                if (mlt > ml) { ml = mlt; *offsetPtr = (ip-base) - matchIndex; }
            }
        }

        if (base + matchIndex <= ip - chainSize) break;
        matchIndex = NEXT_IN_CHAIN(matchIndex, chainMask);
    }

    return ml;
}


FORCE_INLINE size_t ZSTD_HC_HcFindBestMatch_selectMLS (
                        ZSTD_HC_CCtx* zc,   /* Index table will be updated */
                        const BYTE* ip, const BYTE* const iLimit,
                        size_t* offsetPtr,
                        const U32 maxNbAttempts, const U32 matchLengthSearch)
{
    switch(matchLengthSearch)
    {
    default :
    case 4 : return ZSTD_HC_HcFindBestMatch(zc, ip, iLimit, offsetPtr, maxNbAttempts, 4);
    case 5 : return ZSTD_HC_HcFindBestMatch(zc, ip, iLimit, offsetPtr, maxNbAttempts, 5);
    case 6 : return ZSTD_HC_HcFindBestMatch(zc, ip, iLimit, offsetPtr, maxNbAttempts, 6);
    }
}


/* common lazy function, to be inlined */
FORCE_INLINE
size_t ZSTD_HC_compressBlock_lazy_generic(ZSTD_HC_CCtx* ctx,
                                     void* dst, size_t maxDstSize, const void* src, size_t srcSize,
                                     const U32 searchMethod, const U32 deep)   /* 0 : hc; 1 : bt */
{
    seqStore_t* seqStorePtr = &(ctx->seqStore);
    const BYTE* const istart = (const BYTE*)src;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const BYTE* const iend = istart + srcSize;
    const BYTE* const ilimit = iend - 8;

    size_t offset_2=REPCODE_STARTVALUE, offset_1=REPCODE_STARTVALUE;
    const U32 maxSearches = 1 << ctx->params.searchLog;
    const U32 mls = ctx->params.searchLength;

    typedef size_t (*searchMax_f)(ZSTD_HC_CCtx* zc, const BYTE* ip, const BYTE* iLimit,
                        size_t* offsetPtr,
                        U32 maxNbAttempts, U32 matchLengthSearch);
    searchMax_f searchMax = searchMethod ? ZSTD_HC_BtFindBestMatch_selectMLS : ZSTD_HC_HcFindBestMatch_selectMLS;

    /* init */
    ZSTD_resetSeqStore(seqStorePtr);
    if (((ip-ctx->base) - ctx->dictLimit) < REPCODE_STARTVALUE) ip += REPCODE_STARTVALUE;

    /* Match Loop */
    while (ip <= ilimit)
    {
        size_t matchLength;
        size_t offset=999999;
        const BYTE* start;

        /* try to find a first match */
        if (MEM_read32(ip) == MEM_read32(ip - offset_2))
        {
            /* repcode : we take it*/
            size_t offtmp = offset_2;
            size_t litLength = ip - anchor;
            matchLength = ZSTD_count(ip+MINMATCH, ip+MINMATCH-offset_2, iend);
            offset_2 = offset_1;
            offset_1 = offtmp;
            ZSTD_storeSeq(seqStorePtr, litLength, anchor, 0, matchLength);
            ip += matchLength+MINMATCH;
            anchor = ip;
            continue;
        }

        offset_2 = offset_1;
        matchLength = searchMax(ctx, ip, iend, &offset, maxSearches, mls);
        if (!matchLength)
        {
            ip += ((ip-anchor) >> g_searchStrength) + 1;   /* jump faster over incompressible sections */
            continue;
        }

        /* let's try to find a better solution */
        start = ip;

        while (ip<ilimit)
        {
            ip ++;
            if (MEM_read32(ip) == MEM_read32(ip - offset_1))
            {
                size_t ml2 = ZSTD_count(ip+MINMATCH, ip+MINMATCH-offset_1, iend) + MINMATCH;
                int gain2 = (int)(ml2 * 3);
                int gain1 = (int)(matchLength*3 - ZSTD_highbit((U32)offset+1) + 1);
                if (gain2 > gain1)
                    matchLength = ml2, offset = 0, start = ip;
            }
            {
                size_t offset2=999999;
                size_t ml2 = searchMax(ctx, ip, iend, &offset2, maxSearches, mls);
                int gain2 = (int)(ml2*(3+deep) - ZSTD_highbit((U32)offset2+1));   /* raw approx */
                int gain1 = (int)(matchLength*(3+deep) - ZSTD_highbit((U32)offset+1) + (3+deep));
                if (gain2 > gain1)
                {
                    matchLength = ml2, offset = offset2, start = ip;
                    continue;   /* search a better one */
                }
            }

            /* let's find an even better one */
            if (deep && (ip<ilimit))
            {
                ip ++;
                if (MEM_read32(ip) == MEM_read32(ip - offset_1))
                {
                    size_t ml2 = ZSTD_count(ip+MINMATCH, ip+MINMATCH-offset_1, iend) + MINMATCH;
                    int gain2 = (int)(ml2 * 4);
                    int gain1 = (int)(matchLength*4 - ZSTD_highbit((U32)offset+1) + 1);
                    if (gain2 > gain1)
                        matchLength = ml2, offset = 0, start = ip;
                }
                {
                    size_t offset2=999999;
                    size_t ml2 = searchMax(ctx, ip, iend, &offset2, maxSearches, mls);
                    int gain2 = (int)(ml2*4 - ZSTD_highbit((U32)offset2+1));   /* raw approx */
                    int gain1 = (int)(matchLength*4 - ZSTD_highbit((U32)offset+1) + 7);
                    if (gain2 > gain1)
                    {
                        matchLength = ml2, offset = offset2, start = ip;
                        continue;
                    }
                }
            }
            break;  /* nothing found : store previous solution */
        }

        /* store sequence */
        if (offset)
        while ((start>anchor) && (start-offset>ctx->base) && (start[-1] == start[-1-offset])) { start--; matchLength++; }  /* catch up */
        {
            size_t litLength = start - anchor;
            if (offset) offset_1 = offset;
            ZSTD_storeSeq(seqStorePtr, litLength, anchor, offset, matchLength-MINMATCH);
            ip = start + matchLength;
            anchor = ip;
        }

    }

    /* Last Literals */
    {
        size_t lastLLSize = iend - anchor;
        memcpy(seqStorePtr->lit, anchor, lastLLSize);
        seqStorePtr->lit += lastLLSize;
    }

    /* Final compression stage */
    return ZSTD_compressSequences((BYTE*)dst, maxDstSize,
                                  seqStorePtr, srcSize);
}

size_t ZSTD_HC_compressBlock_btlazy2(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    return ZSTD_HC_compressBlock_lazy_generic(ctx, dst, maxDstSize, src, srcSize, 1, 1);
}

size_t ZSTD_HC_compressBlock_lazy2(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    return ZSTD_HC_compressBlock_lazy_generic(ctx, dst, maxDstSize, src, srcSize, 0, 1);
}

size_t ZSTD_HC_compressBlock_lazy(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    return ZSTD_HC_compressBlock_lazy_generic(ctx, dst, maxDstSize, src, srcSize, 0, 0);
}


size_t ZSTD_HC_compressBlock_greedy(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    seqStore_t* seqStorePtr = &(ctx->seqStore);
    const BYTE* const istart = (const BYTE*)src;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const BYTE* const iend = istart + srcSize;
    const BYTE* const ilimit = iend - 8;

    size_t offset_2=REPCODE_STARTVALUE, offset_1=REPCODE_STARTVALUE;
    const U32 maxSearches = 1 << ctx->params.searchLog;
    const U32 mls = ctx->params.searchLength;

    /* init */
    ZSTD_resetSeqStore(seqStorePtr);
    if (((ip-ctx->base) - ctx->dictLimit) < REPCODE_STARTVALUE) ip += REPCODE_STARTVALUE;

    /* Match Loop */
    while (ip < ilimit)
    {
        /* repcode */
        if (MEM_read32(ip) == MEM_read32(ip - offset_2))
        {
            /* store sequence */
            size_t matchLength = ZSTD_count(ip+MINMATCH, ip+MINMATCH-offset_2, iend);
            size_t litLength = ip-anchor;
            size_t offset = offset_2;
            offset_2 = offset_1;
            offset_1 = offset;
            ZSTD_storeSeq(seqStorePtr, litLength, anchor, 0, matchLength);
            ip += matchLength+MINMATCH;
            anchor = ip;
            continue;
        }

        offset_2 = offset_1;  /* failed once : necessarily offset_1 now */

        /* repcode at ip+1 */
        if (MEM_read32(ip+1) == MEM_read32(ip+1 - offset_1))
        {
            size_t matchLength = ZSTD_count(ip+1+MINMATCH, ip+1+MINMATCH-offset_1, iend);
            size_t litLength = ip+1-anchor;
            ZSTD_storeSeq(seqStorePtr, litLength, anchor, 0, matchLength);
            ip += 1+matchLength+MINMATCH;
            anchor = ip;
            continue;
        }

        /* search */
        {
            size_t offset=999999;
            size_t matchLength = ZSTD_HC_HcFindBestMatch_selectMLS(ctx, ip, iend, &offset, maxSearches, mls);
            if (!matchLength)
            {
                ip += ((ip-anchor) >> g_searchStrength) + 1;   /* jump faster over incompressible sections */
                continue;
            }
            while ((ip>anchor) && (ip-offset>ctx->base) && (ip[-1] == ip[-1-offset])) { ip--; matchLength++; }  /* catch up */
            /* store sequence */
            {
                size_t litLength = ip-anchor;
                offset_1 = offset;
                ZSTD_storeSeq(seqStorePtr, litLength, anchor, offset_1, matchLength-MINMATCH);
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

    /* Final compression stage */
    return ZSTD_compressSequences((BYTE*)dst, maxDstSize,

                                  seqStorePtr, srcSize);
}


typedef size_t (*ZSTD_HC_blockCompressor) (ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize);

static ZSTD_HC_blockCompressor ZSTD_HC_selectBlockCompressor(ZSTD_HC_strategy strat)
{
    switch(strat)
    {
    default :
    case ZSTD_HC_fast:
        return ZSTD_HC_compressBlock_fast;
    case ZSTD_HC_greedy:
        return ZSTD_HC_compressBlock_greedy;
    case ZSTD_HC_lazy:
        return ZSTD_HC_compressBlock_lazy;
    case ZSTD_HC_lazy2:
        return ZSTD_HC_compressBlock_lazy2;
    case ZSTD_HC_btlazy2:
        return ZSTD_HC_compressBlock_btlazy2;
    }
}


size_t ZSTD_HC_compressBlock(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    ZSTD_HC_blockCompressor blockCompressor = ZSTD_HC_selectBlockCompressor(ctx->params.strategy);
    return blockCompressor(ctx, dst, maxDstSize, src, srcSize);
}


static size_t ZSTD_HC_compress_generic (ZSTD_HC_CCtx* ctxPtr,
                                        void* dst, size_t maxDstSize,
                                  const void* src, size_t srcSize)
{
    size_t blockSize = BLOCKSIZE;
    size_t remaining = srcSize;
    const BYTE* ip = (const BYTE*)src;
    BYTE* const ostart = (BYTE*)dst;
    BYTE* op = ostart;
    const ZSTD_HC_blockCompressor blockCompressor = ZSTD_HC_selectBlockCompressor(ctxPtr->params.strategy);

    while (remaining)
    {
        size_t cSize;

        if (maxDstSize < 3 + MIN_CBLOCK_SIZE) return ERROR(dstSize_tooSmall);   /* not enough space to store compressed block */

        if (remaining < blockSize) blockSize = remaining;
        cSize = blockCompressor(ctxPtr, op+3, maxDstSize-3, ip, blockSize);
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


size_t ZSTD_HC_compressContinue (ZSTD_HC_CCtx* ctxPtr,
                                 void* dst, size_t dstSize,
                           const void* src, size_t srcSize)
{
    const BYTE* const ip = (const BYTE*) src;

    /* Check if blocks follow each other */
    if (ip != ctxPtr->end)
    {
        if (ctxPtr->end != NULL)
            ZSTD_HC_resetCCtx_advanced(ctxPtr, ctxPtr->params);
        ctxPtr->base = ip;
    }

    ctxPtr->end = ip + srcSize;
    return ZSTD_HC_compress_generic (ctxPtr, dst, dstSize, src, srcSize);
}


size_t ZSTD_HC_compressBegin_advanced(ZSTD_HC_CCtx* ctx,
                                      void* dst, size_t maxDstSize,
                                      const ZSTD_HC_parameters params)
{
    size_t errorCode;
    if (maxDstSize < 4) return ERROR(dstSize_tooSmall);
    errorCode = ZSTD_HC_resetCCtx_advanced(ctx, params);
    if (ZSTD_isError(errorCode)) return errorCode;
    MEM_writeLE32(dst, ZSTD_magicNumber); /* Write Header */
    return 4;
}


size_t ZSTD_HC_compressBegin(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, int compressionLevel)
{
    if (compressionLevel<=0) compressionLevel = 1;
    if (compressionLevel > ZSTD_HC_MAX_CLEVEL) compressionLevel = ZSTD_HC_MAX_CLEVEL;
    return ZSTD_HC_compressBegin_advanced(ctx, dst, maxDstSize, ZSTD_HC_defaultParameters[compressionLevel]);
}


size_t ZSTD_HC_compressEnd(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize)
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

size_t ZSTD_HC_compress_advanced (ZSTD_HC_CCtx* ctx,
                                 void* dst, size_t maxDstSize,
                                 const void* src, size_t srcSize,
                                 ZSTD_HC_parameters params)
{
    BYTE* const ostart = (BYTE*)dst;
    BYTE* op = ostart;
    size_t oSize;

    /* correct params, to use less memory */
    {
        U32 srcLog = ZSTD_highbit((U32)srcSize-1) + 1;
        U32 contentBtPlus = (ctx->params.strategy == ZSTD_HC_btlazy2);
        if (params.windowLog > srcLog) params.windowLog = srcLog;
        if (params.contentLog > srcLog+contentBtPlus) params.contentLog = srcLog+contentBtPlus;
    }

    /* Header */
    oSize = ZSTD_HC_compressBegin_advanced(ctx, dst, maxDstSize, params);
    if(ZSTD_isError(oSize)) return oSize;
    op += oSize;
    maxDstSize -= oSize;

    /* body (compression) */
    ctx->base = (const BYTE*)src;
    oSize = ZSTD_HC_compress_generic (ctx, op,  maxDstSize, src, srcSize);
    if(ZSTD_isError(oSize)) return oSize;
    op += oSize;
    maxDstSize -= oSize;

    /* Close frame */
    oSize = ZSTD_HC_compressEnd(ctx, op, maxDstSize);
    if(ZSTD_isError(oSize)) return oSize;
    op += oSize;

    return (op - ostart);
}

size_t ZSTD_HC_compressCCtx (ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize, int compressionLevel)
{
    if (compressionLevel<=1) return ZSTD_compress(dst, maxDstSize, src, srcSize);   /* fast mode */
    if (compressionLevel > ZSTD_HC_MAX_CLEVEL) compressionLevel = ZSTD_HC_MAX_CLEVEL;
    return ZSTD_HC_compress_advanced(ctx, dst, maxDstSize, src, srcSize, ZSTD_HC_defaultParameters[compressionLevel]);
}

size_t ZSTD_HC_compress(void* dst, size_t maxDstSize, const void* src, size_t srcSize, int compressionLevel)
{
    size_t result;
    ZSTD_HC_CCtx ctxBody;
    memset(&ctxBody, 0, sizeof(ctxBody));
    result = ZSTD_HC_compressCCtx(&ctxBody, dst, maxDstSize, src, srcSize, compressionLevel);
    free(ctxBody.workSpace);
    return result;
}
