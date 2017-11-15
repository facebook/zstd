/*
 * Copyright (c) 2016-present, Przemyslaw Skibinski, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "zstd_compress_internal.h"
#include "zstd_opt.h"
#include "zstd_lazy.h"   /* ZSTD_updateTree, ZSTD_updateTree_extDict */


#define ZSTD_LITFREQ_ADD    2   /* sort of scaling factor for litSum and litFreq (why the need ?), but also used for matchSum ? */
#define ZSTD_FREQ_DIV       4   /* log factor when using previous stats to init next stats */
#define ZSTD_MAX_PRICE     (1<<30)


/*-*************************************
*  Price functions for optimal parser
***************************************/
static void ZSTD_setLog2Prices(optState_t* optPtr)
{
    optPtr->log2litSum = ZSTD_highbit32(optPtr->litSum+1);
    optPtr->log2litLengthSum = ZSTD_highbit32(optPtr->litLengthSum+1);
    optPtr->log2matchLengthSum = ZSTD_highbit32(optPtr->matchLengthSum+1);
    optPtr->log2offCodeSum = ZSTD_highbit32(optPtr->offCodeSum+1);
    optPtr->factor = 1 + ((optPtr->litSum>>5) / optPtr->litLengthSum)
                       + ((optPtr->litSum<<1) / (optPtr->litSum + optPtr->matchSum));  /* => {0,1}, == (optPtr->matchSum <= optPtr->litSum) */
}


static void ZSTD_rescaleFreqs(optState_t* optPtr, const BYTE* src, size_t srcSize)
{
    optPtr->cachedLiterals = NULL;
    optPtr->cachedPrice = optPtr->cachedLitLength = 0;
    optPtr->staticPrices = 0;

    if (optPtr->litLengthSum == 0) {  /* first init */
        unsigned u;
        if (srcSize <= 1024) optPtr->staticPrices = 1;

        assert(optPtr->litFreq!=NULL);
        for (u=0; u<=MaxLit; u++)
            optPtr->litFreq[u] = 0;
        for (u=0; u<srcSize; u++)
            optPtr->litFreq[src[u]]++;
        optPtr->litSum = 0;
        for (u=0; u<=MaxLit; u++) {
            optPtr->litFreq[u] = 1 + (optPtr->litFreq[u] >> ZSTD_FREQ_DIV);
            optPtr->litSum += optPtr->litFreq[u];
        }

        for (u=0; u<=MaxLL; u++)
            optPtr->litLengthFreq[u] = 1;
        optPtr->litLengthSum = MaxLL+1;
        for (u=0; u<=MaxML; u++)
            optPtr->matchLengthFreq[u] = 1;
        optPtr->matchLengthSum = MaxML+1;
        optPtr->matchSum = (ZSTD_LITFREQ_ADD << Litbits);
        for (u=0; u<=MaxOff; u++)
            optPtr->offCodeFreq[u] = 1;
        optPtr->offCodeSum = (MaxOff+1);

    } else {
        unsigned u;

        optPtr->litSum = 0;
        for (u=0; u<=MaxLit; u++) {
            optPtr->litFreq[u] = 1 + (optPtr->litFreq[u] >> (ZSTD_FREQ_DIV+1));
            optPtr->litSum += optPtr->litFreq[u];
        }
        optPtr->litLengthSum = 0;
        for (u=0; u<=MaxLL; u++) {
            optPtr->litLengthFreq[u] = 1 + (optPtr->litLengthFreq[u]>>(ZSTD_FREQ_DIV+1));
            optPtr->litLengthSum += optPtr->litLengthFreq[u];
        }
        optPtr->matchLengthSum = 0;
        optPtr->matchSum = 0;
        for (u=0; u<=MaxML; u++) {
            optPtr->matchLengthFreq[u] = 1 + (optPtr->matchLengthFreq[u]>>ZSTD_FREQ_DIV);
            optPtr->matchLengthSum += optPtr->matchLengthFreq[u];
            optPtr->matchSum += optPtr->matchLengthFreq[u] * (u + 3);
        }
        optPtr->matchSum *= ZSTD_LITFREQ_ADD;
        optPtr->offCodeSum = 0;
        for (u=0; u<=MaxOff; u++) {
            optPtr->offCodeFreq[u] = 1 + (optPtr->offCodeFreq[u]>>ZSTD_FREQ_DIV);
            optPtr->offCodeSum += optPtr->offCodeFreq[u];
        }
    }

    ZSTD_setLog2Prices(optPtr);
}


static U32 ZSTD_getLiteralPrice(optState_t* optPtr, U32 const litLength, const BYTE* literals)
{
    U32 price;

    if (optPtr->staticPrices)
        return ZSTD_highbit32((U32)litLength+1) + (litLength*6);  /* 6 bit per literal - no statistic used */

    if (litLength == 0)
        return optPtr->log2litLengthSum - ZSTD_highbit32(optPtr->litLengthFreq[0]+1);

    /* literals */
    if (optPtr->cachedLiterals == literals) {
        U32 u;
        U32 const additional = litLength - optPtr->cachedLitLength;
        const BYTE* const literals2 = optPtr->cachedLiterals + optPtr->cachedLitLength;
        price = optPtr->cachedPrice + additional * optPtr->log2litSum;
        for (u=0; u < additional; u++)
            price -= ZSTD_highbit32(optPtr->litFreq[literals2[u]]+1);
        optPtr->cachedPrice = price;
        optPtr->cachedLitLength = litLength;
    } else {
        U32 u;
        price = litLength * optPtr->log2litSum;
        for (u=0; u < litLength; u++)
            price -= ZSTD_highbit32(optPtr->litFreq[literals[u]]+1);

        if (litLength >= 12) {
            optPtr->cachedLiterals = literals;
            optPtr->cachedPrice = price;
            optPtr->cachedLitLength = litLength;
        }
    }

    /* literal Length */
    {   U32 const llCode = ZSTD_LLcode(litLength);
        price += LL_bits[llCode] + optPtr->log2litLengthSum - ZSTD_highbit32(optPtr->litLengthFreq[llCode]+1);
    }

    return price;
}


FORCE_INLINE_TEMPLATE U32 ZSTD_getPrice(optState_t* optPtr, U32 litLength, const BYTE* literals, U32 offset, U32 matchLength, const int ultra)
{
    BYTE const offCode = (BYTE)ZSTD_highbit32(offset+1);
    U32 const mlBase = matchLength - MINMATCH;
    U32 price;

    if (optPtr->staticPrices)  /* fixed scheme, do not use statistics */
        return ZSTD_getLiteralPrice(optPtr, litLength, literals) + ZSTD_highbit32((U32)mlBase+1) + 16 + offCode;

    price = offCode + optPtr->log2offCodeSum - ZSTD_highbit32(optPtr->offCodeFreq[offCode]+1);
    if (!ultra /*static*/ && offCode >= 20) price += (offCode-19)*2; /* handicap for long matches, favor decompression speed */

    /* match Length */
    {   U32 const mlCode = ZSTD_MLcode(mlBase);
        price += ML_bits[mlCode] + optPtr->log2matchLengthSum - ZSTD_highbit32(optPtr->matchLengthFreq[mlCode]+1);
    }

    price += ZSTD_getLiteralPrice(optPtr, litLength, literals) + optPtr->factor;
    DEBUGLOG(8, "ZSTD_getPrice(ll:%u, ml:%u) = %u", litLength, matchLength, price);
    return price;
}


static void ZSTD_updatePrice(optState_t* optPtr, U32 litLength, const BYTE* literals, U32 offset, U32 matchLength)
{
    U32 u;

    /* literals */
    optPtr->litSum += litLength*ZSTD_LITFREQ_ADD;
    for (u=0; u < litLength; u++)
        optPtr->litFreq[literals[u]] += ZSTD_LITFREQ_ADD;

    /* literal Length */
    {   const U32 llCode = ZSTD_LLcode(litLength);
        optPtr->litLengthFreq[llCode]++;
        optPtr->litLengthSum++;
    }

    /* match offset */
    {   BYTE const offCode = (BYTE)ZSTD_highbit32(offset+1);
        optPtr->offCodeSum++;
        optPtr->offCodeFreq[offCode]++;
    }

    /* match Length */
    {   U32 const mlBase = matchLength - MINMATCH;
        U32 const mlCode = ZSTD_MLcode(mlBase);
        optPtr->matchLengthFreq[mlCode]++;
        optPtr->matchLengthSum++;
    }

    ZSTD_setLog2Prices(optPtr);
}


/* function safe only for comparisons */
MEM_STATIC U32 ZSTD_readMINMATCH(const void* memPtr, U32 length)
{
    switch (length)
    {
    default :
    case 4 : return MEM_read32(memPtr);
    case 3 : if (MEM_isLittleEndian())
                return MEM_read32(memPtr)<<8;
             else
                return MEM_read32(memPtr)>>8;
    }
}


/* Update hashTable3 up to ip (excluded)
   Assumption : always within prefix (i.e. not within extDict) */
static U32 ZSTD_insertAndFindFirstIndexHash3 (ZSTD_CCtx* zc, const BYTE* ip)
{
    U32* const hashTable3  = zc->hashTable3;
    U32 const hashLog3  = zc->hashLog3;
    const BYTE* const base = zc->base;
    U32 idx = zc->nextToUpdate3;
    U32 const target = zc->nextToUpdate3 = (U32)(ip - base);
    size_t const hash3 = ZSTD_hash3Ptr(ip, hashLog3);

    while(idx < target) {
        hashTable3[ZSTD_hash3Ptr(base+idx, hashLog3)] = idx;
        idx++;
    }

    return hashTable3[hash3];
}


/*-*************************************
*  Binary Tree search
***************************************/
static U32 ZSTD_insertBtAndGetAllMatches (
                        ZSTD_CCtx* zc,
                        const BYTE* const ip, const BYTE* const iLimit, const int extDict,
                        U32 nbCompares, U32 const mls,
                        U32 rep[ZSTD_REP_NUM], U32 const ll0,
                        ZSTD_match_t* matches, const U32 minMatchLen)
{
    const BYTE* const base = zc->base;
    const U32 current = (U32)(ip-base);
    const U32 hashLog = zc->appliedParams.cParams.hashLog;
    const size_t h  = ZSTD_hashPtr(ip, hashLog, mls);
    U32 const minMatch = (mls==3) ? 3 : 4;
    U32* const hashTable = zc->hashTable;
    U32 matchIndex  = hashTable[h];
    U32* const bt   = zc->chainTable;
    const U32 btLog = zc->appliedParams.cParams.chainLog - 1;
    const U32 btMask= (1U << btLog) - 1;
    size_t commonLengthSmaller=0, commonLengthLarger=0;
    const BYTE* const dictBase = zc->dictBase;
    const U32 dictLimit = zc->dictLimit;
    const BYTE* const dictEnd = dictBase + dictLimit;
    const BYTE* const prefixStart = base + dictLimit;
    const U32 btLow = btMask >= current ? 0 : current - btMask;
    const U32 windowLow = zc->lowLimit;
    U32* smallerPtr = bt + 2*(current&btMask);
    U32* largerPtr  = bt + 2*(current&btMask) + 1;
    U32 matchEndIdx = current+8;   /* farthest referenced position of any match => detects repetitive patterns */
    U32 dummy32;   /* to be nullified at the end */
    U32 mnum = 0;

    size_t bestLength = minMatchLen-1;

#if defined(ZSTD_DEBUG) && (ZSTD_DEBUG >= 8)
    g_debuglog_enable = (current ==  5202593);
    //g_debuglog_enable = (current ==  8845622);
    //if (current == 12193408) g_debuglog_enable = 1;
#endif

    /* check repCode */
#if 0
    if (!extDict) /*static*/ {
        U32 const lastR = ZSTD_REP_NUM + ll0;
        U32 repCode;
        for (repCode = ll0; repCode < lastR; repCode++) {
            S32 const repOffset = (repCode==ZSTD_REP_NUM) ? (rep[0] - 1) : rep[repCode];
            if ( (repOffset > 0)
              && (repOffset < (S32)(ip-prefixStart))  /* within current mem segment */
              && (ZSTD_readMINMATCH(ip, minMatch) == ZSTD_readMINMATCH(ip - repOffset, minMatch))) {
                U32 const repLen = (U32)ZSTD_count(ip+minMatch, ip+minMatch-repOffset, iLimit) + minMatch;
                /* save longer solution */
                if (repLen > bestLength) {
                    DEBUGLOG(8, "current %u : found rep%u match of size%3u",
                                current, repCode - ll0, repLen);
                    bestLength = repLen;
                    matches[mnum].off = repCode - ll0;
                    matches[mnum].len = (U32)repLen;
                    mnum++;
                    if ( (repLen > ZSTD_OPT_NUM)
                       | (ip+repLen == iLimit) ) {  /* best possible */
                        return mnum;
        }   }   }   }
    } else {   /* extDict */
        U32 const lastR = ZSTD_REP_NUM + ll0;
        U32 repCode;
        for (repCode=ll0; repCode<lastR; repCode++) {
            const S32 repCur = (repCode==ZSTD_REP_NUM) ? (rep[0] - 1) : rep[repCode];
            const U32 repIndex = (U32)(current - repCur);
            const BYTE* const repBase = repIndex < dictLimit ? dictBase : base;
            const BYTE* const repMatch = repBase + repIndex;
            if ( (repCur > 0 && repCur <= (S32)current)
               && (((U32)((dictLimit-1) - repIndex) >= 3) & (repIndex>windowLow))  /* intentional overflow */
               && (ZSTD_readMINMATCH(ip, minMatch) == ZSTD_readMINMATCH(repMatch, minMatch)) ) {
                /* repcode detected we should take it */
                const BYTE* const repEnd = repIndex < dictLimit ? dictEnd : iLimit;
                U32 const repLen = (U32)ZSTD_count_2segments(ip+minMatch, repMatch+minMatch, iLimit, repEnd, prefixStart) + minMatch;

                /* save longer solution */
                if (repLen > bestLength) {
                    DEBUGLOG(8, "current %u : found rep%u match of size%3u",
                                current, repCode - ll0, repLen);
                    bestLength = repLen;
                    matches[mnum].off = repCode - ll0;
                    matches[mnum].len = (U32)repLen;
                    mnum++;
                    if ( (repLen > ZSTD_OPT_NUM)
                       | (ip+repLen == iLimit) ) {  /* best possible */
                        return mnum;
        }   }   }   }
    }
#else
    (void)ll0; (void)rep; (void)minMatch;
#endif

#if 1
    /* HC3 match finder */
    if ((mls == 3) /*static*/ && (bestLength < mls)) {
        U32 const matchIndex3 = ZSTD_insertAndFindFirstIndexHash3 (zc, ip);
        if ((matchIndex3>windowLow)
          & (current - matchIndex3 < (1<<18)) /*heuristic : longer distance likely too expensive*/ ) {
            size_t mlen;
            if ((!extDict) /*static*/ || (matchIndex3 >= dictLimit)) {
                const BYTE* const match = base + matchIndex3;
                mlen = ZSTD_count(ip, match, iLimit);
            } else {
                const BYTE* const match = dictBase + matchIndex3;
                mlen = ZSTD_count_2segments(ip, match, iLimit, dictEnd, prefixStart);
            }

            /* save best solution */
            if (mlen >= mls /* == 3 > bestLength */) {
                DEBUGLOG(8, "current %u : found short match of size%3u at distance%7u",
                            current, (U32)mlen, current - matchIndex3);
                bestLength = mlen;
                assert(current > matchIndex3);
                assert(mnum==0);  /* no prior solution */
                matches[0].off = (current - matchIndex3) + ZSTD_REP_MOVE;
                matches[0].len = (U32)mlen;
                mnum = 1;
                if ( (mlen > ZSTD_OPT_NUM)
                   | (ip+mlen == iLimit) ) {  /* best possible */
                    return 1;
    }   }   }   }
#endif

    hashTable[h] = current;   /* Update Hash Table */

    while (nbCompares-- && (matchIndex > windowLow)) {
        U32* const nextPtr = bt + 2*(matchIndex & btMask);
        size_t matchLength = MIN(commonLengthSmaller, commonLengthLarger);   /* guaranteed minimum nb of common bytes */
        const BYTE* match;
        assert(current > matchIndex);
#if defined(ZSTD_DEBUG) && (ZSTD_DEBUG>=8)
        if (matchIndex==5189477) g_debuglog_enable = 1;
        DEBUGLOG(8, "candidate match at index:%7u; presumed min matchLength:%3u ",
                    matchIndex, (U32)matchLength);
#endif
        if ((!extDict) || (matchIndex+matchLength >= dictLimit)) {
            match = base + matchIndex;
#if defined(ZSTD_DEBUG) && (ZSTD_DEBUG>=2)
            {   size_t const controlSize = ZSTD_count(ip, match, iLimit);
                if (controlSize < matchLength) {
                    DEBUGLOG(2, "Warning !! => matchIndex %u while searching %u within prefix is smaller than minimum expectation (%u<%u) !",
                                matchIndex, current, (U32)controlSize, (U32)matchLength);
            }   }
#endif
            if (match[matchLength] == ip[matchLength]) {
                matchLength += ZSTD_count(ip+matchLength+1, match+matchLength+1, iLimit) +1;
            }
#if defined(ZSTD_DEBUG) && (ZSTD_DEBUG>=8)
            {   int i;
                RAWLOG(8, "orig %7u: ", current);
                for (i=0; i<8; i++) RAWLOG(7," %02X ", ip[i]);
                RAWLOG(8, " \n");
                RAWLOG(8, "match%7u: ", matchIndex);
                for (i=0; i<8; i++) RAWLOG(7," %02X ", match[i]);
                RAWLOG(8, " \n");
            }
#endif
        } else {
            match = dictBase + matchIndex;
#if defined(ZSTD_DEBUG) && (ZSTD_DEBUG>=2)
            {   size_t const controlSize = ZSTD_count_2segments(ip, match, iLimit, dictEnd, prefixStart);
                if (controlSize < matchLength) {
                    DEBUGLOG(2, "Warning !! => matchIndex %u while searching %u into _extDict is smaller than minimum expectation (%u<%u) !",
                                matchIndex, current, (U32)controlSize, (U32)matchLength);
            }   }
#endif
            matchLength += ZSTD_count_2segments(ip+matchLength, match+matchLength, iLimit, dictEnd, prefixStart);
            if (matchIndex+matchLength >= dictLimit)
                match = base + matchIndex;   /* prepare for match[matchLength] */
#if defined(ZSTD_DEBUG) && (ZSTD_DEBUG>=8)
            if (matchIndex + 8 < dictLimit)
            {   int i;
                const BYTE* const ptr = dictBase + matchIndex;
                RAWLOG(8, "orig %7u: ", current);
                for (i=0; i<8; i++) RAWLOG(7," %02X ", ip[i]);
                RAWLOG(8, " \n");
                RAWLOG(8, "match%7u: ", matchIndex);
                for (i=0; i<8; i++) RAWLOG(7," %02X ", ptr[i]);
                RAWLOG(8, " \n");
            }
#endif
        }
        DEBUGLOG(8, "candidate match has length %u (best:%u) ",
                    (U32)matchLength, (U32)bestLength);

        if (matchLength > bestLength) {
            DEBUGLOG(8, "current %u : found longer match of size%3u at distance%7u",
                        current, (U32)matchLength, current - matchIndex);
            assert(matchEndIdx > matchIndex);
            if (matchLength > matchEndIdx - matchIndex)
                matchEndIdx = matchIndex + (U32)matchLength;
            bestLength = matchLength;
            matches[mnum].off = (current - matchIndex) + ZSTD_REP_MOVE;
            matches[mnum].len = (U32)matchLength;
            mnum++;
            if (matchLength > ZSTD_OPT_NUM) break;
            if (ip+matchLength == iLimit) {  /* equal : no way to know if inf or sup */
                DEBUGLOG(8, "match at index %u has equal value at length %u : cannot determine > or <",
                            matchIndex, (U32)matchLength);
                break;   /* drop, to preserve bt consistency (miss a little bit of compression) */
            }
        }

        if (match[matchLength] < ip[matchLength]) {
#if defined(ZSTD_DEBUG) && (ZSTD_DEBUG>=8)
            DEBUGLOG(8, "match at index %u is smaller than current (%02X<%02X) (pos%u)",
                        matchIndex, match[matchLength], ip[matchLength], (U32)matchLength);
            {   int i;
                RAWLOG(8, "index %u: ", matchIndex);
                for (i=0; i<18; i++) RAWLOG(7," %02X ", match[i]);
                RAWLOG(8, " \n");
            }
#endif
            *smallerPtr = matchIndex;             /* update smaller idx */
            commonLengthSmaller = matchLength;    /* all smaller will now have at least this guaranteed common length */
            if (matchIndex <= btLow) { smallerPtr=&dummy32; break; }   /* beyond tree size, stop the search */
            smallerPtr = nextPtr+1;               /* new "smaller" => larger of match */
            matchIndex = nextPtr[1];              /* new matchIndex larger than previous (closer to current) */
#if defined(ZSTD_DEBUG) && (ZSTD_DEBUG>=8)
            DEBUGLOG(8, "selecting larger candidate : ");
            {   int i;
                const BYTE* const match2 = (matchIndex < dictLimit) ? dictBase + matchIndex : base + matchIndex;
                RAWLOG(8, "index %u: ", matchIndex);
                for (i=0; i<18; i++) RAWLOG(7," %02X ", match2[i]);
                RAWLOG(8, " \n");
            }
#endif
        } else {
#if defined(ZSTD_DEBUG) && (ZSTD_DEBUG>=8)
            DEBUGLOG(8, "match at index %u is larger than current (%02X>%02X) (pos%u)",
                        matchIndex, match[matchLength], ip[matchLength], (U32)matchLength);
            {   int i;
                RAWLOG(8, "index %u: ", matchIndex);
                for (i=0; i<18; i++) RAWLOG(7," %02X ", match[i]);
                RAWLOG(8, " \n");
            }
#endif
            *largerPtr = matchIndex;
            commonLengthLarger = matchLength;
            if (matchIndex <= btLow) { largerPtr=&dummy32; break; }   /* beyond tree size, stop the search */
            largerPtr = nextPtr;
            matchIndex = nextPtr[0];
#if defined(ZSTD_DEBUG) && (ZSTD_DEBUG>=8)
            DEBUGLOG(8, "selecting smaller candidate : ");
            {   int i;
                const BYTE* const match2 = (matchIndex < dictLimit) ? dictBase + matchIndex : base + matchIndex;
                RAWLOG(8, "index %u: ", matchIndex);
                for (i=0; i<18; i++) RAWLOG(7," %02X ", match2[i]);
                RAWLOG(8, " \n");
            }
#endif
    }   }

    *smallerPtr = *largerPtr = 0;

    zc->nextToUpdate = (matchEndIdx > current + 8) ? matchEndIdx - 8 : current+1;  /* skip repetitive patterns */
    return mnum;
}


static U32 ZSTD_BtGetAllMatches (
                        ZSTD_CCtx* zc,   /* Index table will be updated */
                        const BYTE* ip, const BYTE* const iHighLimit, const int extDict,
                        const U32 maxNbAttempts, const U32 matchLengthSearch,
                        U32 rep[ZSTD_REP_NUM], U32 const ll0,
                        ZSTD_match_t* matches, const U32 minMatchLen)
{
    if (ip < zc->base + zc->nextToUpdate) return 0;   /* skipped area */
    ZSTD_updateTree(zc, ip, iHighLimit, maxNbAttempts, matchLengthSearch);
    switch(matchLengthSearch)
    {
    case 3 : return ZSTD_insertBtAndGetAllMatches(zc, ip, iHighLimit, extDict, maxNbAttempts, 3, rep, ll0, matches, minMatchLen);
    default :
    case 4 : return ZSTD_insertBtAndGetAllMatches(zc, ip, iHighLimit, extDict, maxNbAttempts, 4, rep, ll0, matches, minMatchLen);
    case 5 : return ZSTD_insertBtAndGetAllMatches(zc, ip, iHighLimit, extDict, maxNbAttempts, 5, rep, ll0, matches, minMatchLen);
    case 7 :
    case 6 : return ZSTD_insertBtAndGetAllMatches(zc, ip, iHighLimit, extDict, maxNbAttempts, 6, rep, ll0, matches, minMatchLen);
    }
}


/*-*******************************
*  Optimal parser
*********************************/
typedef struct repcodes_s {
    U32 rep[3];
} repcodes_t;

repcodes_t ZSTD_updateRep(U32 const rep[3], U32 const offset, U32 const ll0)
{
    repcodes_t newReps;
    if (offset >= ZSTD_REP_NUM) {  /* full offset */
        newReps.rep[2] = rep[1];
        newReps.rep[1] = rep[0];
        newReps.rep[0] = offset - ZSTD_REP_MOVE;
    } else {   /* repcode */
        U32 const repCode = offset + ll0;
        if (repCode > 0) {  /* note : if repCode==0, no change */
            U32 const currentOffset = (repCode==ZSTD_REP_NUM) ? (rep[0] - 1) : rep[repCode];
            newReps.rep[2] = (repCode >= 2) ? rep[1] : rep[2];
            newReps.rep[1] = rep[0];
            newReps.rep[0] = currentOffset;
        } else {   /* repCode == 0 */
            memcpy(&newReps, rep, sizeof(newReps));
        }
    }
    return newReps;
}

/* update opt[pos] and last_pos */
#define SET_PRICE(pos, mlen_, offset_, litlen_, price_, rep_) \
    {                                                         \
        while (last_pos < pos)  { opt[last_pos+1].price = ZSTD_MAX_PRICE; last_pos++; } \
        opt[pos].mlen = mlen_;                                \
        opt[pos].off = offset_;                               \
        opt[pos].litlen = litlen_;                            \
        opt[pos].price = price_;                              \
        memcpy(opt[pos].rep, &rep_, sizeof(rep_));            \
    }

FORCE_INLINE_TEMPLATE
size_t ZSTD_compressBlock_opt_generic(ZSTD_CCtx* ctx,
                                      const void* src, size_t srcSize,
                                      const int ultra, const int extDict)
{
    seqStore_t* const seqStorePtr = &(ctx->seqStore);
    optState_t* const optStatePtr = &(ctx->optState);
    const BYTE* const istart = (const BYTE*)src;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const BYTE* const iend = istart + srcSize;
    const BYTE* const ilimit = iend - 8;
    const BYTE* const base = ctx->base;
    const BYTE* const prefixStart = base + ctx->dictLimit;

    U32 const maxSearches = 1U << ctx->appliedParams.cParams.searchLog;
    U32 const sufficient_len = MIN(ctx->appliedParams.cParams.targetLength, ZSTD_OPT_NUM -1);
    U32 const mls = ctx->appliedParams.cParams.searchLength;
    U32 const minMatch = (ctx->appliedParams.cParams.searchLength == 3) ? 3 : 4;

    ZSTD_optimal_t* const opt = optStatePtr->priceTable;
    ZSTD_match_t* const matches = optStatePtr->matchTable;
    U32 rep[ZSTD_REP_NUM];

    /* init */
    ctx->nextToUpdate3 = ctx->nextToUpdate;
    ZSTD_rescaleFreqs(optStatePtr, (const BYTE*)src, srcSize);
    ip += (ip==prefixStart);
    { int i; for (i=0; i<ZSTD_REP_NUM; i++) rep[i]=seqStorePtr->rep[i]; }

    /* Match Loop */
    while (ip < ilimit) {
        U32 cur, last_pos = 0;
        U32 best_mlen, best_off;

        /* find first match */
        {   U32 const litlen = (U32)(ip - anchor);
            U32 const ll0 = !litlen;
            U32 const nbMatches = ZSTD_BtGetAllMatches(ctx, ip, iend, extDict, maxSearches, mls, rep, ll0, matches, minMatch);
            if (!nbMatches) { ip++; continue; }

            /* initialize opt[0] */
            { U32 i ; for (i=0; i<ZSTD_REP_NUM; i++) opt[0].rep[i] = rep[i]; }
            opt[0].mlen = 1;
            opt[0].litlen = litlen;

            /* large match -> immediate encoding */
            {   U32 const maxML = matches[nbMatches-1].len;
                DEBUGLOG(7, "found %u matches of maxLength=%u and offset=%u at cPos=%u => start new serie",
                            nbMatches, maxML, matches[nbMatches-1].off, (U32)(ip-prefixStart));

                if (maxML > sufficient_len) {
                    best_mlen = maxML;
                    best_off = matches[nbMatches-1].off;
                    DEBUGLOG(7, "large match (%u>%u), immediate encoding",
                                best_mlen, sufficient_len);
                    cur = 0;
                    last_pos = 1;
                    goto _shortestPath;
            }   }

            /* set prices for first matches starting position == 0 */
            {   U32 pos = minMatch;
                U32 matchNb;
                for (matchNb = 0; matchNb < nbMatches; matchNb++) {
                    U32 const offset = matches[matchNb].off;
                    U32 const end = matches[matchNb].len;
                    repcodes_t const repHistory = ZSTD_updateRep(rep, offset, ll0);
                    DEBUGLOG(7, "match %u: starting reps=%u,%u,%u ==> %u,%u,%u",
                                matchNb, rep[0], rep[1], rep[2], repHistory.rep[0], repHistory.rep[1], repHistory.rep[2])
                    while (pos <= end) {
                        U32 const matchPrice = ZSTD_getPrice(optStatePtr, litlen, anchor, offset, pos, ultra);
                        if (pos > last_pos || matchPrice < opt[pos].price) {
                            DEBUGLOG(7, "rPos:%u => set initial price : %u",
                                        pos, matchPrice);
                            DEBUGLOG(7, "transferring rep:%u,%u,%u",
                                        repHistory.rep[0], repHistory.rep[1], repHistory.rep[2]);
                            SET_PRICE(pos, pos, offset, litlen, matchPrice, repHistory);   /* note : macro modifies last_pos */
                        }
                        pos++;
            }   }   }
        }

        /* check further positions */
        for (cur = 1; cur <= last_pos; cur++) {
            const BYTE* const inr = ip + cur;
            assert(cur < ZSTD_OPT_NUM);

            /* Fix current position with one literal if cheaper */
            {   U32 const litlen = (opt[cur-1].mlen == 1) ? opt[cur-1].litlen + 1 : 1;
                U32 price;
                if (cur > litlen) {
                    price = opt[cur - litlen].price + ZSTD_getLiteralPrice(optStatePtr, litlen, inr-litlen);
                } else {
                    price = ZSTD_getLiteralPrice(optStatePtr, litlen, anchor);
                }
                if (price <= opt[cur].price) {
                    DEBUGLOG(7, "rPos:%u : better price (%u<%u) using literal",
                                cur, price, opt[cur].price);
                    DEBUGLOG(7, "transferring rep:%u,%u,%u",
                                opt[cur-1].rep[0], opt[cur-1].rep[1], opt[cur-1].rep[2]);
                    SET_PRICE(cur, 1/*mlen*/, 0/*offset*/, litlen, price, opt[cur-1].rep);
            }   }

            if (cur == last_pos) break;

            /* last match must start at a minimum distance of 8 from oend */
            if (inr > ilimit) continue;

            {   U32 const ll0 = (opt[cur].mlen != 1);
                U32 const litlen = (opt[cur].mlen == 1) ? opt[cur].litlen : 0;
                U32 const basePrice = (cur > litlen) ? opt[cur-litlen].price : 0;
                const BYTE* const baseLiterals = ip + cur - litlen;
                U32 const nbMatches = ZSTD_BtGetAllMatches(ctx, inr, iend, extDict, maxSearches, mls, opt[cur].rep, ll0, matches, minMatch);
                U32 matchNb;
                if (!nbMatches) continue;
                assert(baseLiterals >= prefixStart);

                {   U32 const maxML = matches[nbMatches-1].len;
                    DEBUGLOG(7, "rPos:%u, found %u matches, of maxLength=%u",
                                cur, nbMatches, maxML);

                    if ( (maxML > sufficient_len)
                       | (cur + maxML >= ZSTD_OPT_NUM) ) {
                        best_mlen = maxML;
                        best_off = matches[nbMatches-1].off;
                        last_pos = cur + 1;
                        goto _shortestPath;
                    }
                }

                /* set prices using matches found at position == cur */
                for (matchNb = 0; matchNb < nbMatches; matchNb++) {
                    U32 mlen = (matchNb>0) ? matches[matchNb-1].len+1 : minMatch;
                    U32 const lastML = matches[matchNb].len;
                    U32 const offset = matches[matchNb].off;
                    repcodes_t const repHistory = ZSTD_updateRep(opt[cur].rep, offset, ll0);

                    DEBUGLOG(7, "testing match %u => offCode=%u, mlen=%u, llen=%u",
                                matchNb, matches[matchNb].off, lastML, litlen);

                    while (mlen <= lastML) {
                        U32 const pos = cur + mlen;
                        U32 const price = basePrice + ZSTD_getPrice(optStatePtr, litlen, baseLiterals, offset, mlen, ultra);
                        assert(pos < ZSTD_OPT_NUM);

                        if ((pos > last_pos) | (price < opt[pos].price)) {
                            DEBUGLOG(7, "rPos:%u => new better price (%u<%u)",
                                        pos, price, opt[pos].price);
                            SET_PRICE(pos, mlen, offset, litlen, price, repHistory);  /* note : macro modifies last_pos */
                        }

                        mlen++;
        }   }   }   }

        best_mlen = opt[last_pos].mlen;
        best_off = opt[last_pos].off;
        cur = last_pos - best_mlen;
        DEBUGLOG(7, "Preparing path selection : last_pos:%u, cur:%u, best_mlen:%u",
                    last_pos, cur, best_mlen);

_shortestPath:   /* cur, last_pos, best_mlen, best_off have to be set */
        assert(opt[0].mlen == 1);

        /* reverse traversal */
        DEBUGLOG(7, "start reverse traversal (last_pos:%u, cur:%u)",
                    last_pos, cur);
        {   U32 selectedMatchLength = best_mlen;
            U32 selectedOffset = best_off;
            U32 pos = cur;
            while (1) {
                U32 const mlen = opt[pos].mlen;
                U32 const off = opt[pos].off;
                opt[pos].mlen = selectedMatchLength;
                opt[pos].off = selectedOffset;
                selectedMatchLength = mlen;
                selectedOffset = off;
                if (mlen > pos) break;
                pos -= mlen;
        }   }

        /* save sequences */
        {   U32 pos;
            for (pos=0; pos < last_pos; ) {
                U32 const llen = (U32)(ip - anchor);
                U32 const mlen = opt[pos].mlen;
                U32 const offset = opt[pos].off;
                if (mlen == 1) { ip++; pos++; continue; }  /* literal position => move on */
                pos += mlen; ip += mlen;

                /* repcodes update : like ZSTD_updateRep(), but update in place */
                if (offset >= ZSTD_REP_NUM) {  /* full offset */
                    rep[2] = rep[1];
                    rep[1] = rep[0];
                    rep[0] = offset - ZSTD_REP_MOVE;
                } else {   /* repcode */
                    U32 const repCode = offset + (llen==0);
                    if (repCode) {  /* note : if repCode==0, no change */
                        U32 const currentOffset = (repCode==ZSTD_REP_NUM) ? (rep[0] - 1) : rep[repCode];
                        if (repCode >= 2) rep[2] = rep[1];
                        rep[1] = rep[0];
                        rep[0] = currentOffset;
                    }
                }

                ZSTD_updatePrice(optStatePtr, llen, anchor, offset, mlen);
                ZSTD_storeSeq(seqStorePtr, llen, anchor, offset, mlen-MINMATCH);
                DEBUGLOG(7, "seq: ll:%u,ml:%u,off:%u - rep:%u,%u,%u",
                            llen, mlen, offset, rep[0], rep[1], rep[2]);
                anchor = ip;
        }   }
    }   /* for (cur=0; cur < last_pos; ) */

    /* Save reps for next block */
    { int i; for (i=0; i<ZSTD_REP_NUM; i++) seqStorePtr->repToConfirm[i] = rep[i]; }

    /* Return the last literals size */
    return iend - anchor;
}


size_t ZSTD_compressBlock_btopt(ZSTD_CCtx* ctx, const void* src, size_t srcSize)
{
    return ZSTD_compressBlock_opt_generic(ctx, src, srcSize, 0 /*ultra*/, 0 /*extDict*/);
}

size_t ZSTD_compressBlock_btultra(ZSTD_CCtx* ctx, const void* src, size_t srcSize)
{
    return ZSTD_compressBlock_opt_generic(ctx, src, srcSize, 1 /*ultra*/, 0 /*extDict*/);
}

size_t ZSTD_compressBlock_btopt_extDict(ZSTD_CCtx* ctx, const void* src, size_t srcSize)
{
    return ZSTD_compressBlock_opt_generic(ctx, src, srcSize, 0 /*ultra*/, 1 /*extDict*/);
}

size_t ZSTD_compressBlock_btultra_extDict(ZSTD_CCtx* ctx, const void* src, size_t srcSize)
{
    return ZSTD_compressBlock_opt_generic(ctx, src, srcSize, 1 /*ultra*/, 1 /*extDict*/);
}
