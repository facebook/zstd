/*
    ZSTD Optimal mode
    Copyright (C) 2016, Przemyslaw Skibinski, Yann Collet.

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

/* Note : this file is intended to be included within zstd_compress.c */ 


#define ZSTD_FREQ_DIV   5

/*-*************************************
*  Price functions for optimal parser
***************************************/
FORCE_INLINE void ZSTD_setLog2Prices(seqStore_t* ssPtr)
{
    ssPtr->log2matchLengthSum = ZSTD_highbit(ssPtr->matchLengthSum+1);
    ssPtr->log2litLengthSum = ZSTD_highbit(ssPtr->litLengthSum+1);
    ssPtr->log2litSum = ZSTD_highbit(ssPtr->litSum+1);
    ssPtr->log2offCodeSum = ZSTD_highbit(ssPtr->offCodeSum+1);
    ssPtr->factor = 1 + ((ssPtr->litSum>>5) / ssPtr->litLengthSum) + ((ssPtr->litSum<<1) / (ssPtr->litSum + ssPtr->matchSum));
}


MEM_STATIC void ZSTD_rescaleFreqs(seqStore_t* ssPtr)
{
    unsigned u;

    if (ssPtr->litLengthSum == 0) {
        ssPtr->litSum = (2<<Litbits);
        ssPtr->litLengthSum = (1<<LLbits);
        ssPtr->matchLengthSum = (1<<MLbits);
        ssPtr->offCodeSum = (1<<Offbits);
        ssPtr->matchSum = (2<<Litbits);

        for (u=0; u<=MaxLit; u++)
            ssPtr->litFreq[u] = 2;
        for (u=0; u<=MaxLL; u++)
            ssPtr->litLengthFreq[u] = 1;
        for (u=0; u<=MaxML; u++)
            ssPtr->matchLengthFreq[u] = 1;
        for (u=0; u<=MaxOff; u++)
            ssPtr->offCodeFreq[u] = 1;
    } else {
        ssPtr->matchLengthSum = 0;
        ssPtr->litLengthSum = 0;
        ssPtr->offCodeSum = 0;
        ssPtr->matchSum = 0;
        ssPtr->litSum = 0;

        for (u=0; u<=MaxLit; u++) {
            ssPtr->litFreq[u] = 1 + (ssPtr->litFreq[u]>>ZSTD_FREQ_DIV);
            ssPtr->litSum += ssPtr->litFreq[u];
        }
        for (u=0; u<=MaxLL; u++) {
            ssPtr->litLengthFreq[u] = 1 + (ssPtr->litLengthFreq[u]>>ZSTD_FREQ_DIV);
            ssPtr->litLengthSum += ssPtr->litLengthFreq[u];
        }
        for (u=0; u<=MaxML; u++) {
            ssPtr->matchLengthFreq[u] = 1 + (ssPtr->matchLengthFreq[u]>>ZSTD_FREQ_DIV);
            ssPtr->matchLengthSum += ssPtr->matchLengthFreq[u];
            ssPtr->matchSum += ssPtr->matchLengthFreq[u] * (u + 3);
        }
        for (u=0; u<=MaxOff; u++) {
            ssPtr->offCodeFreq[u] = 1 + (ssPtr->offCodeFreq[u]>>ZSTD_FREQ_DIV);
            ssPtr->offCodeSum += ssPtr->offCodeFreq[u];
        }
    }
    
    ZSTD_setLog2Prices(ssPtr);
}


FORCE_INLINE U32 ZSTD_getLiteralPrice(seqStore_t* seqStorePtr, U32 litLength, const BYTE* literals)
{
    U32 price, u;

    if (litLength == 0)
        return seqStorePtr->log2litLengthSum - ZSTD_highbit(seqStorePtr->litLengthFreq[0]+1);

    /* literals */
    price = litLength * seqStorePtr->log2litSum;
    for (u=0; u < litLength; u++)
        price -= ZSTD_highbit(seqStorePtr->litFreq[literals[u]]+1);

    /* literal Length */
    price += ((litLength >= MaxLL)<<3) + ((litLength >= 255+MaxLL)<<4) + ((litLength>=(1<<15))<<3);
    if (litLength >= MaxLL) litLength = MaxLL;
    price += seqStorePtr->log2litLengthSum - ZSTD_highbit(seqStorePtr->litLengthFreq[litLength]+1);

    return price;
}


FORCE_INLINE U32 ZSTD_getPrice(seqStore_t* seqStorePtr, U32 litLength, const BYTE* literals, U32 offset, U32 matchLength)
{
    /* offset */
    BYTE offCode = offset ? (BYTE)ZSTD_highbit(offset+1) + 1 : 0;
    U32 price = (offCode-1) + (!offCode) + seqStorePtr->log2offCodeSum - ZSTD_highbit(seqStorePtr->offCodeFreq[offCode]+1);

    /* match Length */
    price += ((matchLength >= MaxML)<<3) + ((matchLength >= 255+MaxML)<<4) + ((matchLength>=(1<<15))<<3);
    if (matchLength >= MaxML) matchLength = MaxML;
    price += ZSTD_getLiteralPrice(seqStorePtr, litLength, literals) + seqStorePtr->log2matchLengthSum - ZSTD_highbit(seqStorePtr->matchLengthFreq[matchLength]+1);

#if ZSTD_OPT_DEBUG == 3
    switch (seqStorePtr->priceFunc) {
        default:
        case 0:
            return 1 + price + ((seqStorePtr->litSum>>5) / seqStorePtr->litLengthSum) + ((seqStorePtr->litSum<<1) / (seqStorePtr->litSum + seqStorePtr->matchSum));
        case 1:
            return 1 + price;
    }
#else
    return price + seqStorePtr->factor;
#endif
}


MEM_STATIC void ZSTD_updatePrice(seqStore_t* seqStorePtr, U32 litLength, const BYTE* literals, U32 offset, U32 matchLength)
{
    U32 u;

    /* literals */
    seqStorePtr->litSum += litLength;
    for (u=0; u < litLength; u++)
        seqStorePtr->litFreq[literals[u]]++;

    /* literal Length */
    seqStorePtr->litLengthSum++;
    if (litLength >= MaxLL)
        seqStorePtr->litLengthFreq[MaxLL]++;
    else
        seqStorePtr->litLengthFreq[litLength]++;

    /* match offset */
    seqStorePtr->offCodeSum++;
    BYTE offCode = offset ? (BYTE)ZSTD_highbit(offset+1) + 1 : 0;
    seqStorePtr->offCodeFreq[offCode]++;

    /* match Length */
    seqStorePtr->matchLengthSum++;
    if (matchLength >= MaxML)
        seqStorePtr->matchLengthFreq[MaxML]++;
    else
        seqStorePtr->matchLengthFreq[matchLength]++;

    ZSTD_setLog2Prices(seqStorePtr);
}


#define SET_PRICE(pos, mlen_, offset_, litlen_, price_)   \
    {                                                 \
        while (last_pos < pos)  { opt[last_pos+1].price = 1<<30; last_pos++; } \
        opt[pos].mlen = mlen_;                         \
        opt[pos].off = offset_;                        \
        opt[pos].litlen = litlen_;                     \
        opt[pos].price = price_;                       \
        ZSTD_LOG_PARSER("%d: SET price[%d/%d]=%d litlen=%d len=%d off=%d\n", (int)(inr-base), (int)pos, (int)last_pos, opt[pos].price, opt[pos].litlen, opt[pos].mlen, opt[pos].off); \
    }



/*-*************************************
*  Binary Tree search
***************************************/
/* Update hashTable3 up to ip (excluded)
   Assumption : always within prefix (ie. not within extDict) */
static U32 ZSTD_insertAndFindFirstIndexHash3 (ZSTD_CCtx* zc, const BYTE* ip)
{
    U32* const hashTable3  = zc->hashTable3;
    const BYTE* const base = zc->base;
    const U32 target = (U32)(ip - base);
    U32 idx = zc->nextToUpdate3;

    while(idx < target) {
        hashTable3[ZSTD_hash3Ptr(base+idx, HASHLOG3)] = idx;
        idx++;
    }

    zc->nextToUpdate3 = target;
    return hashTable3[ZSTD_hash3Ptr(ip, HASHLOG3)];
}


static U32 ZSTD_insertBtAndGetAllMatches (
                        ZSTD_CCtx* zc,
                        const BYTE* const ip, const BYTE* const iLimit,
                        U32 nbCompares, const U32 mls,
                        U32 extDict, ZSTD_match_t* matches)
{
    const BYTE* const base = zc->base;
    const U32 current = (U32)(ip-base);
    const U32 hashLog = zc->params.hashLog;
    const size_t h  = ZSTD_hashPtr(ip, hashLog, mls);
    U32* const hashTable = zc->hashTable;
    U32 matchIndex  = hashTable[h];
    U32* const bt   = zc->contentTable;
    const U32 btLog = zc->params.contentLog - 1;
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
    U32 matchEndIdx = current+8;
    U32 dummy32;   /* to be nullified at the end */
    U32 mnum = 0;

    const U32 minMatch = (mls == 3) ? 3 : 4;
    size_t bestLength = minMatch-1;

    if (minMatch == 3) { /* HC3 match finder */
        U32 matchIndex3 = ZSTD_insertAndFindFirstIndexHash3 (zc, ip);
        
        if (matchIndex3>windowLow && (current - matchIndex3 < (1<<18))) {
            const BYTE* match;
            size_t currentMl=0;
            if ((!extDict) || matchIndex3 >= dictLimit) {
                match = base + matchIndex3;
                if (match[bestLength] == ip[bestLength]) currentMl = ZSTD_count(ip, match, iLimit);
            } else {
                match = dictBase + matchIndex3;
                if (MEM_readMINMATCH(match, minMatch) == MEM_readMINMATCH(ip, minMatch))    /* assumption : matchIndex3 <= dictLimit-4 (by table construction) */
                    currentMl = ZSTD_count_2segments(ip+minMatch, match+minMatch, iLimit, dictEnd, prefixStart) + minMatch;
            }

            /* save best solution */
            if (currentMl > bestLength) {
                bestLength = currentMl;
                matches[mnum].off = current - matchIndex3;
                matches[mnum].len = (U32)currentMl;
                mnum++;
                if (currentMl > ZSTD_OPT_NUM) goto update;
                if (ip+currentMl == iLimit) goto update; /* best possible, and avoid read overflow*/
            }
        }
    }

    hashTable[h] = current;   /* Update Hash Table */

    while (nbCompares-- && (matchIndex > windowLow)) {
        U32* nextPtr = bt + 2*(matchIndex & btMask);
        size_t matchLength = MIN(commonLengthSmaller, commonLengthLarger);   /* guaranteed minimum nb of common bytes */
        const BYTE* match;

        if ((!extDict) || (matchIndex+matchLength >= dictLimit)) {
            match = base + matchIndex;
            if (match[matchLength] == ip[matchLength]) {
#if ZSTD_OPT_DEBUG >= 5
            size_t ml;
            if (matchIndex < dictLimit)
                ml = ZSTD_count_2segments(ip, dictBase + matchIndex, iLimit, dictEnd, prefixStart);
            else
                ml = ZSTD_count(ip, match, ip+matchLength);
            if (ml < matchLength)
                printf("%d: ERROR_NOEXT: offset=%d matchLength=%d matchIndex=%d dictLimit=%d ml=%d\n", current, (int)(current - matchIndex), (int)matchLength, (int)matchIndex, (int)dictLimit, (int)ml), exit(0);
#endif
                matchLength += ZSTD_count(ip+matchLength+1, match+matchLength+1, iLimit) +1;
            }
        } else {
            match = dictBase + matchIndex;
#if ZSTD_OPT_DEBUG >= 5
            if (memcmp(match, ip, matchLength) != 0)
                 printf("%d: ERROR_EXT: matchLength=%d ZSTD_count=%d\n", current, (int)matchLength, (int)ZSTD_count_2segments(ip+matchLength, match+matchLength, iLimit, dictEnd, prefixStart)), exit(0);
#endif
            matchLength += ZSTD_count_2segments(ip+matchLength, match+matchLength, iLimit, dictEnd, prefixStart);
            ZSTD_LOG_PARSER("%d: ZSTD_INSERTBTANDGETALLMATCHES=%d offset=%d dictBase=%p dictEnd=%p prefixStart=%p ip=%p match=%p\n", (int)current, (int)matchLength, (int)(current - matchIndex), dictBase, dictEnd, prefixStart, ip, match);
            if (matchIndex+matchLength >= dictLimit)
                match = base + matchIndex;   /* to prepare for next usage of match[matchLength] */
        }

        if (matchLength > bestLength) {
            if (matchLength > matchEndIdx - matchIndex) matchEndIdx = matchIndex + (U32)matchLength;
            bestLength = matchLength;
            matches[mnum].off = current - matchIndex;
            matches[mnum].len = (U32)matchLength;
            mnum++;
            if (matchLength > ZSTD_OPT_NUM) break;
            if (ip+matchLength == iLimit)   /* equal : no way to know if inf or sup */
                break;   /* drop, to guarantee consistency (miss a little bit of compression) */
        }

        if (match[matchLength] < ip[matchLength]) {
            /* match is smaller than current */
            *smallerPtr = matchIndex;             /* update smaller idx */
            commonLengthSmaller = matchLength;    /* all smaller will now have at least this guaranteed common length */
            if (matchIndex <= btLow) { smallerPtr=&dummy32; break; }   /* beyond tree size, stop the search */
            smallerPtr = nextPtr+1;               /* new "smaller" => larger of match */
            matchIndex = nextPtr[1];              /* new matchIndex larger than previous (closer to current) */
        } else {
            /* match is larger than current */
            *largerPtr = matchIndex;
            commonLengthLarger = matchLength;
            if (matchIndex <= btLow) { largerPtr=&dummy32; break; }   /* beyond tree size, stop the search */
            largerPtr = nextPtr;
            matchIndex = nextPtr[0];
    }   }

    *smallerPtr = *largerPtr = 0;

update:
    zc->nextToUpdate = (matchEndIdx > current + 8) ? matchEndIdx - 8 : current+1;
    return mnum;
}


/** Tree updater, providing best match */
static U32 ZSTD_BtGetAllMatches (
                        ZSTD_CCtx* zc,
                        const BYTE* const ip, const BYTE* const iLimit,
                        const U32 maxNbAttempts, const U32 mls, ZSTD_match_t* matches)
{
    if (ip < zc->base + zc->nextToUpdate) return 0;   /* skipped area */
    ZSTD_updateTree(zc, ip, iLimit, maxNbAttempts, mls);
    return ZSTD_insertBtAndGetAllMatches(zc, ip, iLimit, maxNbAttempts, mls, 0, matches);
}


static U32 ZSTD_BtGetAllMatches_selectMLS (
                        ZSTD_CCtx* zc,   /* Index table will be updated */
                        const BYTE* ip, const BYTE* const iHighLimit,
                        const U32 maxNbAttempts, const U32 matchLengthSearch, ZSTD_match_t* matches)
{
    switch(matchLengthSearch)
    {
    case 3 : return ZSTD_BtGetAllMatches(zc, ip, iHighLimit, maxNbAttempts, 3, matches);
    default :
    case 4 : return ZSTD_BtGetAllMatches(zc, ip, iHighLimit, maxNbAttempts, 4, matches);
    case 5 : return ZSTD_BtGetAllMatches(zc, ip, iHighLimit, maxNbAttempts, 5, matches);
    case 6 : return ZSTD_BtGetAllMatches(zc, ip, iHighLimit, maxNbAttempts, 6, matches);
    }
}

/** Tree updater, providing best match */
static U32 ZSTD_BtGetAllMatches_extDict (
                        ZSTD_CCtx* zc,
                        const BYTE* const ip, const BYTE* const iLimit,
                        const U32 maxNbAttempts, const U32 mls, ZSTD_match_t* matches)
{
    if (ip < zc->base + zc->nextToUpdate) return 0;   /* skipped area */
    ZSTD_updateTree_extDict(zc, ip, iLimit, maxNbAttempts, mls);
    return ZSTD_insertBtAndGetAllMatches(zc, ip, iLimit, maxNbAttempts, mls, 1, matches);
}


static U32 ZSTD_BtGetAllMatches_selectMLS_extDict (
                        ZSTD_CCtx* zc,   /* Index table will be updated */
                        const BYTE* ip, const BYTE* const iHighLimit,
                        const U32 maxNbAttempts, const U32 matchLengthSearch, ZSTD_match_t* matches)
{
    switch(matchLengthSearch)
    {
    case 3 : return ZSTD_BtGetAllMatches_extDict(zc, ip, iHighLimit, maxNbAttempts, 3, matches);
    default :
    case 4 : return ZSTD_BtGetAllMatches_extDict(zc, ip, iHighLimit, maxNbAttempts, 4, matches);
    case 5 : return ZSTD_BtGetAllMatches_extDict(zc, ip, iHighLimit, maxNbAttempts, 5, matches);
    case 6 : return ZSTD_BtGetAllMatches_extDict(zc, ip, iHighLimit, maxNbAttempts, 6, matches);
    }
}


/*-*******************************
*  Optimal parser
*********************************/
FORCE_INLINE
void ZSTD_compressBlock_opt_generic(ZSTD_CCtx* ctx,
                                    const void* src, size_t srcSize,
                                    const U32 depth)
{
    seqStore_t* seqStorePtr = &(ctx->seqStore);
    const BYTE* const istart = (const BYTE*)src;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const BYTE* litstart;
    const BYTE* const iend = istart + srcSize;
    const BYTE* const ilimit = iend - 8;
    const BYTE* const base = ctx->base;
    const BYTE* const prefixStart = base + ctx->dictLimit;
    
    const U32 maxSearches = 1U << ctx->params.searchLog;
    const U32 sufficient_len = ctx->params.targetLength;
    const U32 mls = ctx->params.searchLength;
    const U32 minMatch = (ctx->params.searchLength == 3) ? 3 : 4;

    ZSTD_optimal_t* opt = seqStorePtr->priceTable;
    ZSTD_match_t* matches = seqStorePtr->matchTable;
    const BYTE* inr;
    U32 cur, match_num, last_pos, litlen, price;

    /* init */
    U32 rep[ZSTD_REP_NUM+1];
    for (int i=0; i<ZSTD_REP_NUM+1; i++)
        rep[i]=REPCODE_STARTVALUE;

    ctx->nextToUpdate3 = ctx->nextToUpdate;
    ZSTD_resetSeqStore(seqStorePtr);
    ZSTD_rescaleFreqs(seqStorePtr);
    if ((ip-prefixStart) < REPCODE_STARTVALUE) ip = prefixStart + REPCODE_STARTVALUE;

    ZSTD_LOG_BLOCK("%d: COMPBLOCK_OPT_GENERIC srcSz=%d maxSrch=%d mls=%d sufLen=%d\n", (int)(ip-base), (int)srcSize, maxSearches, mls, sufficient_len);

    /* Match Loop */
    while (ip < ilimit) {
        U32 u;
        U32 mlen=0;
        U32 best_mlen=0;
        U32 best_off=0;
        memset(opt, 0, sizeof(ZSTD_optimal_t));
        last_pos = 0;
        inr = ip;
        litstart = ((U32)(ip - anchor) > 128) ? ip - 128 : anchor;
        opt[0].litlen = (U32)(ip - litstart);

        /* check repCode */
        if (MEM_readMINMATCH(ip+1, minMatch) == MEM_readMINMATCH(ip+1 - rep[0], minMatch)) {
            /* repcode : we take it */
            mlen = (U32)ZSTD_count(ip+1+minMatch, ip+1+minMatch-rep[0], iend) + minMatch;

            ZSTD_LOG_PARSER("%d: start try REP rep=%d mlen=%d\n", (int)(ip-base), (int)rep[0], (int)mlen);
            if (depth==0 || mlen > sufficient_len || mlen >= ZSTD_OPT_NUM) {
                ip+=1; best_mlen = mlen; best_off = 0; cur = 0; last_pos = 1;
                goto _storeSequence;
            }

            litlen = opt[0].litlen + 1;
            do {
                price = ZSTD_getPrice(seqStorePtr, litlen, litstart, 0, mlen - minMatch);
                if (mlen + 1 > last_pos || price < opt[mlen + 1].price)
                    SET_PRICE(mlen + 1, mlen, 0, litlen, price);   /* note : macro modifies last_pos */
                mlen--;
            } while (mlen >= minMatch);
        }

        match_num = ZSTD_BtGetAllMatches_selectMLS(ctx, ip, iend, maxSearches, mls, matches); /* first search (depth 0) */

        ZSTD_LOG_PARSER("%d: match_num=%d last_pos=%d\n", (int)(ip-base), match_num, last_pos);
        if (!last_pos && !match_num) { ip++; continue; }

        opt[0].rep = rep[0];
        opt[0].rep2 = rep[1];
        opt[0].mlen = 1;

        if (match_num && matches[match_num-1].len > sufficient_len) {
            best_mlen = matches[match_num-1].len;
            best_off = matches[match_num-1].off;
            cur = 0;
            last_pos = 1;
            goto _storeSequence;
        }

       best_mlen = (last_pos) ? last_pos : minMatch;

       // set prices using matches at position = 0
       for (u = 0; u < match_num; u++) {
           mlen = (u>0) ? matches[u-1].len+1 : best_mlen;
           best_mlen = (matches[u].len < ZSTD_OPT_NUM) ? matches[u].len : ZSTD_OPT_NUM;
           ZSTD_LOG_PARSER("%d: start Found mlen=%d off=%d best_mlen=%d last_pos=%d\n", (int)(ip-base), matches[u].len, matches[u].off, (int)best_mlen, (int)last_pos);
           litlen = opt[0].litlen;
           while (mlen <= best_mlen) {
                price = ZSTD_getPrice(seqStorePtr, litlen, litstart, matches[u].off, mlen - minMatch);
                if (mlen > last_pos || price < opt[mlen].price)
                    SET_PRICE(mlen, mlen, matches[u].off, litlen, price);
                mlen++;
        }  }

        if (last_pos < minMatch) { ip++; continue; }

         /* check further positions */
        for (cur = 1; cur <= last_pos; cur++) {
           size_t cur_rep;
           inr = ip + cur;

           if (opt[cur-1].mlen == 1) {
                litlen = opt[cur-1].litlen + 1;
                if (cur > litlen) {
                    price = opt[cur - litlen].price + ZSTD_getLiteralPrice(seqStorePtr, litlen, inr-litlen);
                } else
                    price = ZSTD_getLiteralPrice(seqStorePtr, litlen, litstart);
           } else {
                litlen = 1;
                price = opt[cur - 1].price + ZSTD_getLiteralPrice(seqStorePtr, litlen, inr-1);
           }

           if (cur > last_pos || price <= opt[cur].price) // || ((price == opt[cur].price) && (opt[cur-1].mlen == 1) && (cur != litlen)))
                SET_PRICE(cur, 1, 0, litlen, price);

           if (cur == last_pos) break;

           if (inr > ilimit)  /* last match must start at a minimum distance of 8 from oend */
               continue;

           mlen = opt[cur].mlen;

           if (opt[cur].off) {
                opt[cur].rep2 = opt[cur-mlen].rep;
                opt[cur].rep = opt[cur].off;
                ZSTD_LOG_ENCODE("%d: COPYREP_OFF cur=%d mlen=%d rep=%d rep2=%d\n", (int)(inr-base), cur, mlen, opt[cur].rep, opt[cur].rep2);
           } else {
                if (cur!=mlen && opt[cur].litlen == 0) {
                    opt[cur].rep2 = opt[cur-mlen].rep;
                    opt[cur].rep = opt[cur-mlen].rep2;
                    ZSTD_LOG_ENCODE("%d: COPYREP_SWI cur=%d mlen=%d rep=%d rep2=%d\n", (int)(inr-base), cur, mlen, opt[cur].rep, opt[cur].rep2);
                } else {
                    opt[cur].rep2 = opt[cur-mlen].rep2;
                    opt[cur].rep = opt[cur-mlen].rep;
                    ZSTD_LOG_ENCODE("%d: COPYREP_NOR cur=%d mlen=%d rep=%d rep2=%d\n", (int)(inr-base), cur, mlen, opt[cur].rep, opt[cur].rep2);
           }   }

           ZSTD_LOG_PARSER("%d: CURRENT_NoExt price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d rep2=%d\n", (int)(inr-base), cur, last_pos, opt[cur].price, opt[cur].off, opt[cur].mlen, opt[cur].litlen, opt[cur].rep, opt[cur].rep2);

           best_mlen = 0;

           if (opt[cur].mlen != 1) {
               cur_rep = opt[cur].rep2;
               ZSTD_LOG_PARSER("%d: tryNoExt REP2 rep2=%u mlen=%u\n", (int)(inr-base), (U32)cur_rep, mlen);
           } else {
               cur_rep = opt[cur].rep;
               ZSTD_LOG_PARSER("%d: tryNoExt REP1 rep=%u mlen=%u\n", (int)(inr-base), (U32)cur_rep, mlen);
           }

           if (MEM_readMINMATCH(inr, minMatch) == MEM_readMINMATCH(inr - cur_rep, minMatch)) {  // check rep
               mlen = (U32)ZSTD_count(inr+minMatch, inr+minMatch - cur_rep, iend) + minMatch;
               ZSTD_LOG_PARSER("%d: Found REP mlen=%d off=%d rep=%d opt[%d].off=%d\n", (int)(inr-base), mlen, 0, opt[cur].rep, cur, opt[cur].off);

               if (mlen > sufficient_len || cur + mlen >= ZSTD_OPT_NUM) {
                    best_mlen = mlen;
                    best_off = 0;
                    ZSTD_LOG_PARSER("%d: REP sufficient_len=%d best_mlen=%d best_off=%d last_pos=%d\n", (int)(inr-base), sufficient_len, best_mlen, best_off, last_pos);
                    last_pos = cur + 1;
                    goto _storeSequence;
               }

               if (opt[cur].mlen == 1) {
                    litlen = opt[cur].litlen;
                    if (cur > litlen) {
                        price = opt[cur - litlen].price + ZSTD_getPrice(seqStorePtr, litlen, inr-litlen, 0, mlen - minMatch);
                    } else
                        price = ZSTD_getPrice(seqStorePtr, litlen, litstart, 0, mlen - minMatch);
                } else {
                    litlen = 0;
                    price = opt[cur].price + ZSTD_getPrice(seqStorePtr, 0, NULL, 0, mlen - minMatch);
                }

                best_mlen = mlen;
                ZSTD_LOG_PARSER("%d: Found REP mlen=%d off=%d price=%d litlen=%d\n", (int)(inr-base), mlen, 0, price, litlen);

                do {
                    if (cur + mlen > last_pos || price <= opt[cur + mlen].price)
                        SET_PRICE(cur + mlen, mlen, 0, litlen, price);
                    mlen--;
                } while (mlen >= minMatch);
            }

            match_num = ZSTD_BtGetAllMatches_selectMLS(ctx, inr, iend, maxSearches, mls, matches);
            ZSTD_LOG_PARSER("%d: ZSTD_GetAllMatches match_num=%d\n", (int)(inr-base), match_num);

            if (match_num > 0 && matches[match_num-1].len > sufficient_len) {
                best_mlen = matches[match_num-1].len;
                best_off = matches[match_num-1].off;
                last_pos = cur + 1;
                goto _storeSequence;
            }

            best_mlen = (best_mlen > minMatch) ? best_mlen : minMatch;

            /* set prices using matches at position = cur */
            for (u = 0; u < match_num; u++) {
                mlen = (u>0) ? matches[u-1].len+1 : best_mlen;
                best_mlen = (cur + matches[u].len < ZSTD_OPT_NUM) ? matches[u].len : ZSTD_OPT_NUM - cur;

              //  ZSTD_LOG_PARSER("%d: Found1 cur=%d mlen=%d off=%d best_mlen=%d last_pos=%d\n", (int)(inr-base), cur, matches[u].len, matches[u].off, best_mlen, last_pos);

                while (mlen <= best_mlen) {
                    if (opt[cur].mlen == 1) {
                        litlen = opt[cur].litlen;
                        if (cur > litlen)
                            price = opt[cur - litlen].price + ZSTD_getPrice(seqStorePtr, litlen, ip+cur-litlen, matches[u].off, mlen - minMatch);
                        else
                            price = ZSTD_getPrice(seqStorePtr, litlen, litstart, matches[u].off, mlen - minMatch);
                    } else {
                        litlen = 0;
                        price = opt[cur].price + ZSTD_getPrice(seqStorePtr, 0, NULL, matches[u].off, mlen - minMatch);
                    }

                  //  ZSTD_LOG_PARSER("%d: Found2 mlen=%d best_mlen=%d off=%d price=%d litlen=%d\n", (int)(inr-base), mlen, best_mlen, matches[u].off, price, litlen);

                    if (cur + mlen > last_pos || (price < opt[cur + mlen].price))
                        SET_PRICE(cur + mlen, mlen, matches[u].off, litlen, price);

                    mlen++;
        }   }   }   //  for (cur = 1; cur <= last_pos; cur++)

        best_mlen = opt[last_pos].mlen;
        best_off = opt[last_pos].off;
        cur = last_pos - best_mlen;

        /* store sequence */
_storeSequence:   /* cur, last_pos, best_mlen, best_off have to be set */
        for (u = 1; u <= last_pos; u++)
            ZSTD_LOG_PARSER("%d: price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d rep2=%d\n", (int)(ip-base+u), u, last_pos, opt[u].price, opt[u].off, opt[u].mlen, opt[u].litlen, opt[u].rep, opt[u].rep2);
        ZSTD_LOG_PARSER("%d: cur=%d/%d best_mlen=%d best_off=%d rep=%d\n", (int)(ip-base+cur), (int)cur, (int)last_pos, (int)best_mlen, (int)best_off, opt[cur].rep);

        opt[0].mlen = 1;
        U32 offset;

        while (1) {
            mlen = opt[cur].mlen;
            offset = opt[cur].off;
            opt[cur].mlen = best_mlen;
            opt[cur].off = best_off;
            best_mlen = mlen;
            best_off = offset;
            if (mlen > cur) break;
            cur -= mlen;
        }

        for (u = 0; u <= last_pos;) {
            ZSTD_LOG_PARSER("%d: price2[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d rep2=%d\n", (int)(ip-base+u), u, last_pos, opt[u].price, opt[u].off, opt[u].mlen, opt[u].litlen, opt[u].rep, opt[u].rep2);
            u += opt[u].mlen;
        }

        for (cur=0; cur < last_pos; ) {
            ZSTD_LOG_PARSER("%d: price3[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d rep2=%d\n", (int)(ip-base+cur), cur, last_pos, opt[cur].price, opt[cur].off, opt[cur].mlen, opt[cur].litlen, opt[cur].rep, opt[cur].rep2);
            mlen = opt[cur].mlen;
            if (mlen == 1) { ip++; cur++; continue; }
            offset = opt[cur].off;
            cur += mlen;

            U32 litLength = (U32)(ip - anchor);
            ZSTD_LOG_ENCODE("%d/%d: ENCODE literals=%d mlen=%d off=%d rep1=%d rep2=%d\n", (int)(ip-base), (int)(iend-base), (int)(litLength), (int)mlen, (int)(offset), (int)rep[0], (int)rep[1]);

            if (offset) {
                rep[1] = rep[0];
                rep[0] = offset;
            } else {
                if (litLength == 0) {
                    best_off = rep[1];
                    rep[1] = rep[0];
                    rep[0] = best_off;
            }   }

           // ZSTD_LOG_ENCODE("%d/%d: ENCODE2 literals=%d mlen=%d off=%d rep1=%d rep2=%d\n", (int)(ip-base), (int)(iend-base), (int)(litLength), (int)mlen, (int)(offset), (int)rep[1], (int)rep_2);

#if ZSTD_OPT_DEBUG >= 5
            U32 ml2;
            if (offset)
                ml2 = (U32)ZSTD_count(ip, ip-offset, iend);
            else
                ml2 = (U32)ZSTD_count(ip, ip-rep[0], iend);
            if ((offset >= 8) && (ml2 < mlen || ml2 < minMatch)) {
                printf("%d: ERROR_NoExt iend=%d mlen=%d offset=%d ml2=%d\n", (int)(ip - base), (int)(iend - ip), (int)mlen, (int)offset, (int)ml2); exit(0); }
            if (ip < anchor) {
                printf("%d: ERROR_NoExt ip < anchor iend=%d mlen=%d offset=%d\n", (int)(ip - base), (int)(iend - ip), (int)mlen, (int)offset); exit(0); }
            if (ip + mlen > iend) {
                printf("%d: ERROR_NoExt ip + mlen >= iend iend=%d mlen=%d offset=%d\n", (int)(ip - base), (int)(iend - ip), (int)mlen, (int)offset); exit(0); }
#endif

            ZSTD_updatePrice(seqStorePtr, litLength, anchor, offset, mlen-minMatch);
            ZSTD_storeSeq(seqStorePtr, litLength, anchor, offset ? offset + ZSTD_REP_NUM - 1 : 0, mlen-minMatch);
            anchor = ip = ip + mlen;
        }   /* for (cur=0; cur < last_pos; ) */

        /* check immediate repcode */
        while ((anchor >= prefixStart + rep[1]) && (anchor <= ilimit)
             && (MEM_readMINMATCH(anchor, minMatch) == MEM_readMINMATCH(anchor - rep[1], minMatch)) ) {
            /* store sequence */
            best_mlen = (U32)ZSTD_count(anchor+minMatch, anchor+minMatch-rep[1], iend);
            best_off = rep[1];
            rep[1] = rep[0];
            rep[0] = best_off;
            ZSTD_LOG_ENCODE("%d/%d: ENCODE REP literals=%d mlen=%d off=%d rep1=%d rep2=%d\n", (int)(anchor-base), (int)(iend-base), (int)(0), (int)best_mlen, (int)(0), (int)rep[0], (int)rep[1]);
            ZSTD_updatePrice(seqStorePtr, 0, anchor, 0, best_mlen);
            ZSTD_storeSeq(seqStorePtr, 0, anchor, 0, best_mlen);
            anchor += best_mlen+minMatch;
            continue;   /* faster when present ... (?) */
        }
        if (anchor > ip) ip = anchor;
    }

    {   /* Last Literals */
        size_t lastLLSize = iend - anchor;
        ZSTD_LOG_ENCODE("%d: lastLLSize literals=%u\n", (int)(ip-base), (U32)lastLLSize);
        memcpy(seqStorePtr->lit, anchor, lastLLSize);
        seqStorePtr->lit += lastLLSize;
    }
}


FORCE_INLINE
void ZSTD_compressBlock_opt_extDict_generic(ZSTD_CCtx* ctx,
                                     const void* src, size_t srcSize,
                                     const U32 depth)
{
    seqStore_t* seqStorePtr = &(ctx->seqStore);
    const BYTE* const istart = (const BYTE*)src;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const BYTE* litstart;
    const BYTE* const iend = istart + srcSize;
    const BYTE* const ilimit = iend - 8;
    const BYTE* const base = ctx->base;
    const U32 dictLimit = ctx->dictLimit;
    const BYTE* const prefixStart = base + dictLimit;
    const BYTE* const dictBase = ctx->dictBase;
    const BYTE* const dictEnd  = dictBase + dictLimit;
    const U32 lowLimit = ctx->lowLimit;
   
    const U32 maxSearches = 1U << ctx->params.searchLog;
    const U32 sufficient_len = ctx->params.targetLength;
    const U32 mls = ctx->params.searchLength;
    const U32 minMatch = (ctx->params.searchLength == 3) ? 3 : 4;

    ZSTD_optimal_t* opt = seqStorePtr->priceTable;
    ZSTD_match_t* matches = seqStorePtr->matchTable;
    const BYTE* inr;
    U32 cur, match_num, last_pos, litlen, price;

    /* init */
    U32 rep[ZSTD_REP_NUM+1];
    for (int i=0; i<ZSTD_REP_NUM+1; i++)
        rep[i]=REPCODE_STARTVALUE;

    ctx->nextToUpdate3 = ctx->nextToUpdate;
    ZSTD_resetSeqStore(seqStorePtr);
    ZSTD_rescaleFreqs(seqStorePtr);
    if ((ip - prefixStart) < REPCODE_STARTVALUE) ip += REPCODE_STARTVALUE;

    ZSTD_LOG_BLOCK("%d: COMPBLOCK_OPT_EXTDICT srcSz=%d maxSrch=%d mls=%d sufLen=%d\n", (int)(ip-base), (int)srcSize, maxSearches, mls, sufficient_len);

    /* Match Loop */
    while (ip < ilimit) {
        U32 u, offset, best_off=0;
        U32 mlen=0, best_mlen=0;
        U32 current = (U32)(ip-base);
        memset(opt, 0, sizeof(ZSTD_optimal_t));
        last_pos = 0;
        inr = ip;
        litstart = ((U32)(ip - anchor) > 128) ? ip - 128 : anchor;
        opt[0].litlen = (U32)(ip - litstart);

        /* check repCode */
        {
            const U32 repIndex = (U32)(current+1 - rep[0]);
            const BYTE* const repBase = repIndex < dictLimit ? dictBase : base;
            const BYTE* const repMatch = repBase + repIndex;
            if ( ((U32)((dictLimit-1) - repIndex) >= 3)   /* intentional overflow */
               && (MEM_readMINMATCH(ip+1, minMatch) == MEM_readMINMATCH(repMatch, minMatch)) ) {
                /* repcode detected we should take it */
                const BYTE* const repEnd = repIndex < dictLimit ? dictEnd : iend;
                mlen = (U32)ZSTD_count_2segments(ip+1+minMatch, repMatch+minMatch, iend, repEnd, prefixStart) + minMatch;

                ZSTD_LOG_PARSER("%d: start try REP rep=%d mlen=%d\n", (int)(ip-base), (int)rep[0], (int)mlen);
                if (depth==0 || mlen > sufficient_len || mlen >= ZSTD_OPT_NUM) {
                    ip+=1; best_mlen = mlen; best_off = 0; cur = 0; last_pos = 1;
                    goto _storeSequence;
                }

                litlen = opt[0].litlen + 1;
                do {
                    price = ZSTD_getPrice(seqStorePtr, litlen, litstart, 0, mlen - minMatch);
                    if (mlen + 1 > last_pos || price < opt[mlen + 1].price)
                        SET_PRICE(mlen + 1, mlen, 0, litlen, price);
                    mlen--;
                } while (mlen >= minMatch);
        }   }

       best_mlen = (last_pos) ? last_pos : minMatch;

       match_num = ZSTD_BtGetAllMatches_selectMLS_extDict(ctx, ip, iend, maxSearches, mls, matches);  /* first search (depth 0) */

       ZSTD_LOG_PARSER("%d: match_num=%d last_pos=%d\n", (int)(ip-base), match_num, last_pos);
       if (!last_pos && !match_num) { ip++; continue; }

       opt[0].rep = rep[0];
       opt[0].rep2 = rep[1];
       opt[0].mlen = 1;

       if (match_num && matches[match_num-1].len > sufficient_len) {
            best_mlen = matches[match_num-1].len;
            best_off = matches[match_num-1].off;
            cur = 0;
            last_pos = 1;
            goto _storeSequence;
       }

        // set prices using matches at position = 0
        for (u = 0; u < match_num; u++) {
            mlen = (u>0) ? matches[u-1].len+1 : best_mlen;
            best_mlen = (matches[u].len < ZSTD_OPT_NUM) ? matches[u].len : ZSTD_OPT_NUM;
            ZSTD_LOG_PARSER("%d: start Found mlen=%d off=%d best_mlen=%d last_pos=%d\n", (int)(ip-base), matches[u].len, matches[u].off, (int)best_mlen, (int)last_pos);
            litlen = opt[0].litlen;
            while (mlen <= best_mlen) {
                price = ZSTD_getPrice(seqStorePtr, litlen, litstart, matches[u].off, mlen - minMatch);
                if (mlen > last_pos || price < opt[mlen].price)
                    SET_PRICE(mlen, mlen, matches[u].off, litlen, price);
                mlen++;
        }   }

        if (last_pos < minMatch) {
            // ip += ((ip-anchor) >> g_searchStrength) + 1;   /* jump faster over incompressible sections */
            ip++; continue;
        }

        /* check further positions */
        for (cur = 1; cur <= last_pos; cur++) {
           size_t cur_rep;
           inr = ip + cur;

           if (opt[cur-1].mlen == 1) {
                litlen = opt[cur-1].litlen + 1;
                if (cur > litlen) {
                    price = opt[cur - litlen].price + ZSTD_getLiteralPrice(seqStorePtr, litlen, inr-litlen);
                } else
                    price = ZSTD_getLiteralPrice(seqStorePtr, litlen, litstart);
           } else {
                litlen = 1;
                price = opt[cur - 1].price + ZSTD_getLiteralPrice(seqStorePtr, litlen, inr-1);
           }

           if (cur > last_pos || price <= opt[cur].price) // || ((price == opt[cur].price) && (opt[cur-1].mlen == 1) && (cur != litlen)))
                SET_PRICE(cur, 1, 0, litlen, price);

           if (cur == last_pos) break;

           if (inr > ilimit) // last match must start at a minimum distance of 8 from oend
               continue;

           mlen = opt[cur].mlen;

           if (opt[cur].off) {
                opt[cur].rep2 = opt[cur-mlen].rep;
                opt[cur].rep = opt[cur].off;
                ZSTD_LOG_ENCODE("%d: COPYREP_OFF cur=%d mlen=%d rep=%d rep2=%d\n", (int)(inr-base), cur, mlen, opt[cur].rep, opt[cur].rep2);
           } else {
                if (cur!=mlen && opt[cur].litlen == 0) {
                    opt[cur].rep2 = opt[cur-mlen].rep;
                    opt[cur].rep = opt[cur-mlen].rep2;
                    ZSTD_LOG_ENCODE("%d: COPYREP_SWI cur=%d mlen=%d rep=%d rep2=%d\n", (int)(inr-base), cur, mlen, opt[cur].rep, opt[cur].rep2);
                } else {
                    opt[cur].rep2 = opt[cur-mlen].rep2;
                    opt[cur].rep = opt[cur-mlen].rep;
                    ZSTD_LOG_ENCODE("%d: COPYREP_NOR cur=%d mlen=%d rep=%d rep2=%d\n", (int)(inr-base), cur, mlen, opt[cur].rep, opt[cur].rep2);
           }   }

           ZSTD_LOG_PARSER("%d: CURRENT_Ext price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d rep2=%d\n", (int)(inr-base), cur, last_pos, opt[cur].price, opt[cur].off, opt[cur].mlen, opt[cur].litlen, opt[cur].rep, opt[cur].rep2);

           best_mlen = 0;

           if (opt[cur].mlen != 1) {
               cur_rep = opt[cur].rep2;
               ZSTD_LOG_PARSER("%d: tryExt REP2 rep2=%u mlen=%u\n", (int)(inr-base), (U32)cur_rep, mlen);
           } else {
               cur_rep = opt[cur].rep;
               ZSTD_LOG_PARSER("%d: tryExt REP1 rep=%u mlen=%u\n", (int)(inr-base), (U32)cur_rep, mlen);
           }

           const U32 repIndex = (U32)(current+cur - cur_rep);
           const BYTE* const repBase = repIndex < dictLimit ? dictBase : base;
           const BYTE* const repMatch = repBase + repIndex;
           if ( ((U32)((dictLimit-1) - repIndex) >= 3)   /* intentional overflow */
              && (MEM_readMINMATCH(inr, minMatch) == MEM_readMINMATCH(repMatch, minMatch)) ) {
                /* repcode detected */
                const BYTE* const repEnd = repIndex < dictLimit ? dictEnd : iend;
                mlen = (U32)ZSTD_count_2segments(inr+minMatch, repMatch+minMatch, iend, repEnd, prefixStart) + minMatch;
                ZSTD_LOG_PARSER("%d: Found REP mlen=%d off=%d rep=%d opt[%d].off=%d\n", (int)(inr-base), mlen, 0, opt[cur].rep, cur, opt[cur].off);

                if (mlen > sufficient_len || cur + mlen >= ZSTD_OPT_NUM) {
                    best_mlen = mlen;
                    best_off = 0;
                    ZSTD_LOG_PARSER("%d: REP sufficient_len=%d best_mlen=%d best_off=%d last_pos=%d\n", (int)(inr-base), sufficient_len, best_mlen, best_off, last_pos);
                    last_pos = cur + 1;
                    goto _storeSequence;
                }

                if (opt[cur].mlen == 1) {
                    litlen = opt[cur].litlen;
                    if (cur > litlen) {
                        price = opt[cur - litlen].price + ZSTD_getPrice(seqStorePtr, litlen, inr-litlen, 0, mlen - minMatch);
                    } else
                        price = ZSTD_getPrice(seqStorePtr, litlen, litstart, 0, mlen - minMatch);
                } else {
                    litlen = 0;
                    price = opt[cur].price + ZSTD_getPrice(seqStorePtr, 0, NULL, 0, mlen - minMatch);
                }

                best_mlen = mlen;

                ZSTD_LOG_PARSER("%d: Found REP mlen=%d off=%d price=%d litlen=%d\n", (int)(inr-base), mlen, 0, price, litlen);

                do {
                    if (cur + mlen > last_pos || price <= opt[cur + mlen].price) // || ((price == opt[cur + mlen].price) && (opt[cur].mlen == 1) && (cur != litlen))) // at equal price prefer REP instead of MATCH
                        SET_PRICE(cur + mlen, mlen, 0, litlen, price);
                    mlen--;
                } while (mlen >= minMatch);
            }

            best_mlen = (best_mlen > minMatch) ? best_mlen : minMatch;

            match_num = ZSTD_BtGetAllMatches_selectMLS_extDict(ctx, inr, iend, maxSearches, mls, matches);
            ZSTD_LOG_PARSER("%d: ZSTD_GetAllMatches match_num=%d\n", (int)(inr-base), match_num);

            if (match_num > 0 && matches[match_num-1].len > sufficient_len) {
                best_mlen = matches[match_num-1].len;
                best_off = matches[match_num-1].off;
                last_pos = cur + 1;
                goto _storeSequence;
            }

            // set prices using matches at position = cur
            for (u = 0; u < match_num; u++) {
                mlen = (u>0) ? matches[u-1].len+1 : best_mlen;
                best_mlen = (cur + matches[u].len < ZSTD_OPT_NUM) ? matches[u].len : ZSTD_OPT_NUM - cur;

            //    ZSTD_LOG_PARSER("%d: Found1 cur=%d mlen=%d off=%d best_mlen=%d last_pos=%d\n", (int)(inr-base), cur, matches[u].len, matches[u].off, best_mlen, last_pos);

                while (mlen <= best_mlen) {
                    if (opt[cur].mlen == 1) {
                        litlen = opt[cur].litlen;
                        if (cur > litlen)
                            price = opt[cur - litlen].price + ZSTD_getPrice(seqStorePtr, litlen, ip+cur-litlen, matches[u].off, mlen - minMatch);
                        else
                            price = ZSTD_getPrice(seqStorePtr, litlen, litstart, matches[u].off, mlen - minMatch);
                    } else {
                        litlen = 0;
                        price = opt[cur].price + ZSTD_getPrice(seqStorePtr, 0, NULL, matches[u].off, mlen - minMatch);
                    }

                //    ZSTD_LOG_PARSER("%d: Found2 mlen=%d best_mlen=%d off=%d price=%d litlen=%d\n", (int)(inr-base), mlen, best_mlen, matches[u].off, price, litlen);

                    if (cur + mlen > last_pos || (price < opt[cur + mlen].price))
                        SET_PRICE(cur + mlen, mlen, matches[u].off, litlen, price);

                    mlen++;
        }   }   }   //  for (cur = 1; cur <= last_pos; cur++)

        best_mlen = opt[last_pos].mlen;
        best_off = opt[last_pos].off;
        cur = last_pos - best_mlen;

        /* store sequence */
_storeSequence: // cur, last_pos, best_mlen, best_off have to be set
        for (u = 1; u <= last_pos; u++)
            ZSTD_LOG_PARSER("%d: price[%u/%d]=%d off=%d mlen=%d litlen=%d rep=%d rep2=%d\n", (int)(ip-base+u), u, last_pos, opt[u].price, opt[u].off, opt[u].mlen, opt[u].litlen, opt[u].rep, opt[u].rep2);
        ZSTD_LOG_PARSER("%d: cur=%d/%d best_mlen=%d best_off=%d rep=%d\n", (int)(ip-base+cur), (int)cur, (int)last_pos, (int)best_mlen, (int)best_off, opt[cur].rep);

        opt[0].mlen = 1;

        while (1) {
            mlen = opt[cur].mlen;
            offset = opt[cur].off;
            opt[cur].mlen = best_mlen;
            opt[cur].off = best_off;
            best_mlen = mlen;
            best_off = offset;
            if (mlen > cur) break;
            cur -= mlen;
        }

        for (u = 0; u <= last_pos; ) {
            ZSTD_LOG_PARSER("%d: price2[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d rep2=%d\n", (int)(ip-base+u), u, last_pos, opt[u].price, opt[u].off, opt[u].mlen, opt[u].litlen, opt[u].rep, opt[u].rep2);
            u += opt[u].mlen;
        }

        for (cur=0; cur < last_pos; ) {
            U32 litLength;
            ZSTD_LOG_PARSER("%d: price3[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d rep2=%d\n", (int)(ip-base+cur), cur, last_pos, opt[cur].price, opt[cur].off, opt[cur].mlen, opt[cur].litlen, opt[cur].rep, opt[cur].rep2);
            mlen = opt[cur].mlen;
            if (mlen == 1) { ip++; cur++; continue; }
            offset = opt[cur].off;
            cur += mlen;

            litLength = (U32)(ip - anchor);
            ZSTD_LOG_ENCODE("%d/%d: ENCODE1 literals=%d mlen=%d off=%d rep1=%d rep2=%d\n", (int)(ip-base), (int)(iend-base), (int)(litLength), (int)mlen, (int)(offset), (int)rep[0], (int)rep[1]);

            if (offset) {
                rep[1] = rep[0];
                rep[0] = offset;
            } else {
                if (litLength == 0) {
                    best_off = rep[1];
                    rep[1] = rep[0];
                    rep[0] = best_off;
            }   }

            ZSTD_LOG_ENCODE("%d/%d: ENCODE2 literals=%d mlen=%d off=%d rep1=%d rep2=%d\n", (int)(ip-base), (int)(iend-base), (int)(litLength), (int)mlen, (int)(offset), (int)rep[0], (int)rep[1]);

#if ZSTD_OPT_DEBUG >= 5
            U32 ml2;
            if (offset) {
                if (offset > (size_t)(ip - prefixStart))  {
                    const BYTE* match = dictEnd - (offset - (ip - prefixStart));
                    ml2 = ZSTD_count_2segments(ip, match, iend, dictEnd, prefixStart);
                    ZSTD_LOG_PARSER("%d: ZSTD_count_2segments=%d offset=%d dictBase=%p dictEnd=%p prefixStart=%p ip=%p match=%p\n", (int)current, (int)ml2, (int)offset, dictBase, dictEnd, prefixStart, ip, match);
                }
                else ml2 = (U32)ZSTD_count(ip, ip-offset, iend);
            }
            else ml2 = (U32)ZSTD_count(ip, ip-rep[0], iend);
            if ((offset >= 8) && (ml2 < mlen || ml2 < minMatch)) {
                printf("%d: ERROR_Ext iend=%d mlen=%d offset=%d ml2=%d\n", (int)(ip - base), (int)(iend - ip), (int)mlen, (int)offset, (int)ml2); exit(0); }
            if (ip < anchor) {
                printf("%d: ERROR_Ext ip < anchor iend=%d mlen=%d offset=%d\n", (int)(ip - base), (int)(iend - ip), (int)mlen, (int)offset); exit(0); }
            if (ip + mlen > iend) {
                printf("%d: ERROR_Ext ip + mlen >= iend iend=%d mlen=%d offset=%d\n", (int)(ip - base), (int)(iend - ip), (int)mlen, (int)offset); exit(0); }
#endif

            ZSTD_updatePrice(seqStorePtr, litLength, anchor, offset, mlen-minMatch);
            ZSTD_storeSeq(seqStorePtr, litLength, anchor, offset ? offset + ZSTD_REP_NUM - 1 : 0, mlen-minMatch);
            anchor = ip = ip + mlen;
        }

#if 0
        /* check immediate repcode */
        while ((anchor >= base + lowLimit + rep[1]) && (anchor <= ilimit)) {
            if ((anchor - rep[1]) >= prefixStart) {
                if (MEM_readMINMATCH(anchor, minMatch) == MEM_readMINMATCH(anchor - rep[1], minMatch))
                    mlen = (U32)ZSTD_count(anchor+minMatch, anchor - rep[1] + minMatch, iend) + minMatch;
                else
                    break;
            } else {
                const BYTE* repMatch = dictBase + ((anchor-base) - rep[1]);
                if ((repMatch + minMatch <= dictEnd) && (MEM_readMINMATCH(anchor, minMatch) == MEM_readMINMATCH(repMatch, minMatch))) 
                    mlen = (U32)ZSTD_count_2segments(anchor+minMatch, repMatch+minMatch, iend, dictEnd, prefixStart) + minMatch;
                else
                    break;
            }
                   
            offset = rep[1]; rep[1] = rep[0]; rep[0] = offset;   /* swap offset history */
            ZSTD_LOG_ENCODE("%d/%d: ENCODE REP literals=%d mlen=%d off=%d rep1=%d rep2=%d\n", (int)(anchor-base), (int)(iend-base), (int)(0), (int)best_mlen, (int)(0), (int)rep[0], (int)rep[1]);
            ZSTD_updatePrice(seqStorePtr, 0, anchor, 0, mlen-minMatch);
            ZSTD_storeSeq(seqStorePtr, 0, anchor, 0, mlen-minMatch);
            anchor += mlen;
        }
#else
        /* check immediate repcode */
        /* minimal correctness condition = while ((anchor >= prefixStart + REPCODE_STARTVALUE) && (anchor <= ilimit)) { */
        while ((anchor >= base + lowLimit + rep[1]) && (anchor <= ilimit)) {
            const U32 repIndex = (U32)((anchor-base) - rep[1]);
            const BYTE* const repBase = repIndex < dictLimit ? dictBase : base;
            const BYTE* const repMatch = repBase + repIndex;
            if ( ((U32)((dictLimit-1) - repIndex) >= 3)   /* intentional overflow */
               && (MEM_readMINMATCH(anchor, minMatch) == MEM_readMINMATCH(repMatch, minMatch)) ) {
                /* repcode detected, let's take it */
                const BYTE* const repEnd = repIndex < dictLimit ? dictEnd : iend;
                mlen = (U32)ZSTD_count_2segments(anchor+minMatch, repMatch+minMatch, iend, repEnd, prefixStart) + minMatch;
                offset = rep[1]; rep[1] = rep[0]; rep[0] = offset;   /* swap offset history */
                ZSTD_LOG_ENCODE("%d/%d: ENCODE REP literals=%d mlen=%d off=%d rep1=%d rep2=%d\n", (int)(anchor-base), (int)(iend-base), (int)(0), (int)best_mlen, (int)(0), (int)rep[0], (int)rep[1]);
                ZSTD_updatePrice(seqStorePtr, 0, anchor, 0, mlen-minMatch);
                ZSTD_storeSeq(seqStorePtr, 0, anchor, 0, mlen-minMatch);
                anchor += mlen;
                continue;   /* faster when present ... (?) */
            }
            break;
        }
#endif
        if (anchor > ip) ip = anchor;
    }

    {   /* Last Literals */
        size_t lastLLSize = iend - anchor;
        ZSTD_LOG_ENCODE("%d: lastLLSize literals=%u\n", (int)(ip-base), (U32)(lastLLSize));
        memcpy(seqStorePtr->lit, anchor, lastLLSize);
        seqStorePtr->lit += lastLLSize;
    }
}
