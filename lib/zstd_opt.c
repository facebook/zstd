#include <stdio.h>
#include <math.h> // log

typedef struct
{
	int off;
	int len;
	int back;
} ZSTD_match_t;

typedef struct
{
	int price;
	int off;
	int mlen;
	int litlen;
   	int rep;
   	int rep2;
} ZSTD_optimal_t; 

#if 1
    #define ZSTD_LOG_PARSER(fmt, args...) ;// printf(fmt, ##args)
    #define ZSTD_LOG_PRICE(fmt, args...) ;//printf(fmt, ##args)
    #define ZSTD_LOG_ENCODE(fmt, args...) ;//printf(fmt, ##args) 
#else
    #define ZSTD_LOG_PARSER(fmt, args...) printf(fmt, ##args)
    #define ZSTD_LOG_PRICE(fmt, args...) printf(fmt, ##args)
    #define ZSTD_LOG_ENCODE(fmt, args...) printf(fmt, ##args) 
#endif

#define ZSTD_LOG_TRY_PRICE(fmt, args...) ;//printf(fmt, ##args)

#define ZSTD_OPT_NUM   (1<<12)


#define ZSTD_LIT_COST(len) 0 //(((len)<<3))

const int tab32[32] = {
     0,  9,  1, 10, 13, 21,  2, 29,
    11, 14, 16, 18, 22, 25,  3, 30,
     8, 12, 20, 28, 15, 17, 24,  7,
    19, 27, 23,  6, 26,  5,  4, 31};

int log2_32 (uint32_t value)
{
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return tab32[(uint32_t)(value*0x07C4ACDD) >> 27];
}


FORCE_INLINE size_t ZSTD_getLiteralPriceReal(seqStore_t* seqStorePtr, size_t litLength, const BYTE* literals)
{
    size_t freq;
    size_t price = 0;
    size_t litBits, litLenBits;
  //  printf("litSum=%d litLengthSum=%d matchLengthSum=%d offCodeSum=%d\n", seqStorePtr->litSum, seqStorePtr->litLengthSum, seqStorePtr->matchLengthSum, seqStorePtr->offCodeSum);

    /* literals */
    litBits = 0;
    if (litLength > 0) {
        for (int i=litLength-1; i>=0; i--)
//            litBits += -log2((double)seqStorePtr->litFreq[literals[i]]/(double)seqStorePtr->litSum);
            litBits += log2_32(seqStorePtr->litSum) - log2_32(seqStorePtr->litFreq[literals[i]]);

        /* literal Length */
        if (litLength >= MaxLL) {
            freq = seqStorePtr->litLengthFreq[MaxLL];
            if (litLength<255 + MaxLL) {
                price += 8;
            } else {
                price += 8;
                if (litLength < (1<<15)) price += 16; else price += 24;
        }   }
        else freq = seqStorePtr->litLengthFreq[litLength];
//        litLenBits = -log2((double)freq/(double)seqStorePtr->litLengthSum);
        litLenBits = log2_32(seqStorePtr->litLengthSum) - log2_32(freq);
    }
    else litLenBits = 0;

//    freq = round(1.0f*(litBits + litLenBits + price));
    freq = litBits + litLenBits + price;
//        printf("litLength=%d litBits=%.02f litLenBits=%.02f dumpsPrice=%d sum=%d\n", (int)litLength, litBits, litLenBits, (int)price, (int)freq);

 //   printf("old=%d new=%d\n", (int)((litLength<<3)+1), (int)freq/8);
    if (freq <= 0) freq = 1;
    return freq;
}



FORCE_INLINE size_t ZSTD_getLiteralPrice(seqStore_t* seqStorePtr, size_t litLength, const BYTE* literals)
{
#if 1
    return ZSTD_getLiteralPriceReal(seqStorePtr, litLength, literals);
#else
    size_t lit_cost = 1 + (litLength<<3);
    return lit_cost;
#endif
}



FORCE_INLINE size_t ZSTD_getMatchPriceReal(seqStore_t* seqStorePtr, size_t offset, size_t matchLength)
{
    size_t freq;
    size_t price = 0;
    size_t offCodeBits, matchBits;
  //  printf("litSum=%d litLengthSum=%d matchLengthSum=%d offCodeSum=%d\n", seqStorePtr->litSum, seqStorePtr->litLengthSum, seqStorePtr->matchLengthSum, seqStorePtr->offCodeSum);

    /* match offset */
    BYTE offCode = (BYTE)ZSTD_highbit(offset) + 1;
    if (offset==0) 
        offCode = 0;
 //   offCodeBits = -log2((double)seqStorePtr->offCodeFreq[offCode]/(double)seqStorePtr->offCodeSum);
    offCodeBits = log2_32(seqStorePtr->offCodeSum) - log2_32(seqStorePtr->offCodeFreq[offCode]);
 //   printf("offCodeBits=%.02f matchBits=%.02f dumpsPrice=%d sum=%d\n", offCodeBits, matchBits, (int)price, (int)freq);

    offCodeBits += offCode;

    /* match Length */
    if (matchLength >= MaxML) {
        freq = seqStorePtr->matchLengthFreq[MaxML];
        if (matchLength < 255+MaxML) {
            price += 8;
        } else {
            price += 8;
            if (matchLength < (1<<15)) price += 16; else price += 24;
    }   }
    else freq = seqStorePtr->matchLengthFreq[matchLength];
//    matchBits = -log2((double)freq/(double)seqStorePtr->matchLengthSum);
    matchBits = log2_32(seqStorePtr->matchLengthSum) - log2_32(freq);

//    freq = round(1.0f*(offCodeBits + matchBits + price));
    freq = offCodeBits + matchBits + price;
//        printf("offCodeBits=%.02f matchBits=%.02f dumpsPrice=%d sum=%d\n", offCodeBits, matchBits, (int)price, (int)freq);
    return freq;
}

// zstd v0.5 beta level 23       3.05 MB/s    448 MB/s     40868360  38.98
// zstd v0.5 beta level 24       2.90 MB/s    472 MB/s     40392170  38.52
// zstd v0.5 beta level 23       1.10 MB/s       ERROR     40584556  38.70
// zstd v0.5 beta level 24       0.87 MB/s       ERROR     40103205  38.25

FORCE_INLINE size_t ZSTD_getPrice(seqStore_t* seqStorePtr, size_t litLength, const BYTE* literals, size_t offset, size_t matchLength)
{
#if 1
    size_t lit_cost = ZSTD_getLiteralPriceReal(seqStorePtr, litLength, literals);
    size_t match_cost_old = ZSTD_highbit((U32)matchLength+1) + Offbits + ZSTD_highbit((U32)offset+1);
    size_t match_cost = ZSTD_getMatchPriceReal(seqStorePtr, offset, matchLength);
  //  printf("old=%d new=%d\n", (int)match_cost2, (int)match_cost);
    return lit_cost + match_cost_old;
#else
    size_t lit_cost = (litLength<<3);
    size_t match_cost = ZSTD_highbit((U32)matchLength+1) + Offbits + ZSTD_highbit((U32)offset+1);
    return lit_cost + match_cost;
#endif
}



MEM_STATIC void ZSTD_updatePrice(seqStore_t* seqStorePtr, size_t litLength, const BYTE* literals, size_t offset, size_t matchLength)
{
#if 0
    static const BYTE* g_start = NULL;
    if (g_start==NULL) g_start = literals;
    //if (literals - g_start == 8695)
    printf("pos %6u : %3u literals & match %3u bytes at distance %6u \n",
           (U32)(literals - g_start), (U32)litLength, (U32)matchLength+4, (U32)offset);
#endif
    /* literals */
    seqStorePtr->litSum += litLength;
    for (int i=litLength-1; i>=0; i--)
        seqStorePtr->litFreq[literals[i]]++;
    
    /* literal Length */
    seqStorePtr->litLengthSum++;
    if (litLength >= MaxLL)
        seqStorePtr->litLengthFreq[MaxLL]++;
    else 
        seqStorePtr->litLengthFreq[litLength]++;

    /* match offset */
    seqStorePtr->offCodeSum++;
    BYTE offCode = (BYTE)ZSTD_highbit(offset) + 1;
    if (offset==0) offCode=0;
    seqStorePtr->offCodeFreq[offCode]++;

    /* match Length */
    seqStorePtr->matchLengthSum++;
    if (matchLength >= MaxML)
        seqStorePtr->matchLengthFreq[MaxML]++;
    else 
        seqStorePtr->matchLengthFreq[matchLength]++;
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



FORCE_INLINE /* inlining is important to hardwire a hot branch (template emulation) */
size_t ZSTD_insertBtAndGetAllMatches (
                        ZSTD_CCtx* zc,
                        const BYTE* const ip, const BYTE* const iend,
                        U32 nbCompares, const U32 mls,
                        U32 extDict, ZSTD_match_t* matches, size_t bestLength)
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
    U32 matchEndIdx = current+8;
    U32 dummy32;   /* to be nullified at the end */
    size_t mnum = 0;
    
#if 1
    bestLength = 0;
#else
    bestLength--;
#endif
    hashTable[h] = current;   /* Update Hash Table */

    while (nbCompares-- && (matchIndex > windowLow)) {
        U32* nextPtr = bt + 2*(matchIndex & btMask);
        size_t matchLength = MIN(commonLengthSmaller, commonLengthLarger);   /* guaranteed minimum nb of common bytes */
        const BYTE* match;

        if ((!extDict) || (matchIndex+matchLength >= dictLimit)) {
            match = base + matchIndex;
            if (match[matchLength] == ip[matchLength])
                matchLength += ZSTD_count(ip+matchLength+1, match+matchLength+1, iend) +1;
        } else {
            match = dictBase + matchIndex;
            matchLength += ZSTD_count_2segments(ip+matchLength, match+matchLength, iend, dictEnd, prefixStart);
            if (matchIndex+matchLength >= dictLimit)
                match = base + matchIndex;   /* to prepare for next usage of match[matchLength] */
        }

#if 1
        if (matchLength > bestLength) {
            if (matchLength > matchEndIdx - matchIndex)
                matchEndIdx = matchIndex + (U32)matchLength;
            {
                if (matchLength >= MINMATCH) {
                    bestLength = matchLength; 
                    matches[mnum].off = current - matchIndex;
                    matches[mnum].len = matchLength;
                    matches[mnum].back = 0;
                    mnum++;
                }
                if (matchLength > ZSTD_OPT_NUM) break;
            }
            if (ip+matchLength == iend)   /* equal : no way to know if inf or sup */
                break;   /* drop, to guarantee consistency (miss a little bit of compression) */
        }
#else
        if (matchLength > matchEndIdx - matchIndex)
            matchEndIdx = matchIndex + (U32)matchLength;

        if (matchLength > bestLength) {
            bestLength = matchLength; 
            matches[mnum].off = current - matchIndex;
            matches[mnum].len = matchLength;
            matches[mnum].back = 0;
            mnum++;

            if (matchLength > ZSTD_OPT_NUM) break;
        }

        if (ip+matchLength == iend)   /* equal : no way to know if inf or sup */
            break;   /* drop, to guarantee consistency (miss a little bit of compression) */
#endif


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
        }
    }

    *smallerPtr = *largerPtr = 0;

    zc->nextToUpdate = (matchEndIdx > current + 8) ? matchEndIdx - 8 : current+1;
    return mnum;
}


/** Tree updater, providing best match */
FORCE_INLINE /* inlining is important to hardwire a hot branch (template emulation) */
size_t ZSTD_BtGetAllMatches (
                        ZSTD_CCtx* zc,
                        const BYTE* const ip, const BYTE* const iLimit,
                        const U32 maxNbAttempts, const U32 mls, ZSTD_match_t* matches, size_t minml)
{
    if (ip < zc->base + zc->nextToUpdate) return 0;   /* skipped area */
    ZSTD_updateTree(zc, ip, iLimit, maxNbAttempts, mls);
    return ZSTD_insertBtAndGetAllMatches(zc, ip, iLimit, maxNbAttempts, mls, 0, matches, minml);
}


FORCE_INLINE size_t ZSTD_BtGetAllMatches_selectMLS (
                        ZSTD_CCtx* zc,   /* Index table will be updated */
                        const BYTE* ip, const BYTE* const iLimit,
                        const U32 maxNbAttempts, const U32 matchLengthSearch, ZSTD_match_t* matches, size_t minml)
{
    switch(matchLengthSearch)
    {
    default :
    case 4 : return ZSTD_BtGetAllMatches(zc, ip, iLimit, maxNbAttempts, 4, matches, minml);
    case 5 : return ZSTD_BtGetAllMatches(zc, ip, iLimit, maxNbAttempts, 5, matches, minml);
    case 6 : return ZSTD_BtGetAllMatches(zc, ip, iLimit, maxNbAttempts, 6, matches, minml);
    }
}


FORCE_INLINE /* inlining is important to hardwire a hot branch (template emulation) */
size_t ZSTD_HcGetAllMatches_generic (
                        ZSTD_CCtx* zc,   /* Index table will be updated */
                        const BYTE* const ip, const BYTE* const iLimit,
                        const U32 maxNbAttempts, const U32 mls, const U32 extDict, ZSTD_match_t* matches, size_t minml)
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
    size_t mnum = 0;
    minml=MINMATCH-1;

    /* HC4 match finder */
    matchIndex = ZSTD_insertAndFindFirstIndex (zc, ip, mls);

    while ((matchIndex>lowLimit) && (nbAttempts)) {
        size_t currentMl=0;
        nbAttempts--;
        if ((!extDict) || matchIndex >= dictLimit) {
            match = base + matchIndex;
            if (match[minml] == ip[minml])   /* potentially better */
                currentMl = ZSTD_count(ip, match, iLimit);
        } else {
            match = dictBase + matchIndex;
            if (MEM_read32(match) == MEM_read32(ip))   /* assumption : matchIndex <= dictLimit-4 (by table construction) */
                currentMl = ZSTD_count_2segments(ip+MINMATCH, match+MINMATCH, iLimit, dictEnd, prefixStart) + MINMATCH;
        }

        /* save best solution */
        if (currentMl > minml) { 
            minml = currentMl; 
            matches[mnum].off = current - matchIndex;
            matches[mnum].len = currentMl;
            matches[mnum].back = 0;
            mnum++;
            if (currentMl > ZSTD_OPT_NUM) break;
            if (ip+currentMl == iLimit) break; /* best possible, and avoid read overflow*/ 
        }

        if (matchIndex <= minChain) break;
        matchIndex = NEXT_IN_CHAIN(matchIndex, chainMask);
    }

    return mnum;
}


FORCE_INLINE size_t ZSTD_HcGetAllMatches_selectMLS (
                        ZSTD_CCtx* zc,
                        const BYTE* ip, const BYTE* const iLimit,
                        const U32 maxNbAttempts, const U32 matchLengthSearch, ZSTD_match_t* matches, size_t minml)
{
    switch(matchLengthSearch)
    {
    default :
    case 4 : return ZSTD_HcGetAllMatches_generic(zc, ip, iLimit, maxNbAttempts, 4, 0, matches, minml);
    case 5 : return ZSTD_HcGetAllMatches_generic(zc, ip, iLimit, maxNbAttempts, 5, 0, matches, minml);
    case 6 : return ZSTD_HcGetAllMatches_generic(zc, ip, iLimit, maxNbAttempts, 6, 0, matches, minml);
    }
}


void print_hex_text(uint8_t* buf, int bufsize, int endline)
{
    int i, j;
    for (i=0; i<bufsize; i+=16) 
	{
		printf("%02d:", i);
		for (j=0; j<16; j++) 
			if (i+j<bufsize)
				printf("%02x,",buf[i+j]);
			else 
				printf("   ");
		printf(" ");
		for (j=0; i+j<bufsize && j<16; j++) 
			printf("%c",buf[i+j]>32?buf[i+j]:'.');
		printf("\n");
	}
    if (endline) printf("\n");
}


/* *******************************
*  Optimal parser OLD
*********************************/
FORCE_INLINE
void ZSTD_compressBlock_opt2_generic(ZSTD_CCtx* ctx,
                                     const void* src, size_t srcSize,
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
 
#if 0
    typedef size_t (*getAllMatches_f)(ZSTD_CCtx* zc, const BYTE* ip, const BYTE* iLimit,
                        U32 maxNbAttempts, U32 matchLengthSearch, ZSTD_match_t* matches);
    getAllMatches_f getAllMatches = searchMethod ? ZSTD_BtGetAllMatches_selectMLS : ZSTD_HcGetAllMatches_selectMLS;

    ZSTD_match_t matches[ZSTD_OPT_NUM+1];
#endif

    /* init */
    ZSTD_resetSeqStore(seqStorePtr);
    if ((ip-base) < REPCODE_STARTVALUE) ip = base + REPCODE_STARTVALUE;

    /* Match Loop */
    while (ip < ilimit) {
        size_t matchLength=0;
        size_t offset=0;
        const BYTE* start=ip+1;

#define ZSTD_USE_REP
#ifdef ZSTD_USE_REP
        /* check repCode */
        if (MEM_read32(start) == MEM_read32(start - offset_1)) {
            /* repcode : we take it */
            matchLength = ZSTD_count(start+MINMATCH, start+MINMATCH-offset_1, iend) + MINMATCH;
            if (depth==0) goto _storeSequence;
        }
#endif

        {
            /* first search (depth 0) */
#if 1
            size_t offsetFound = 99999999;
            size_t ml2 = searchMax(ctx, ip, iend, &offsetFound, maxSearches, mls);
            if (ml2 > matchLength)
                start=ip, matchLength = ml2,  offset=offsetFound;
#else
            size_t mnum = getAllMatches(ctx, ip, iend, maxSearches, mls, matches); 
            if (mnum > 0) {
                if (matches[mnum-1].len > matchLength)
                    start=ip, matchLength = matches[mnum-1].len, offset=matches[mnum-1].off;
            }
#endif
        }

        if (matchLength < MINMATCH) {
       //     ip += ((ip-anchor) >> g_searchStrength) + 1;   /* jump faster over incompressible sections */
            ip++;
            continue;
        }

#if 1
        /* let's try to find a better solution */
        if (depth>=1)
        while (ip<ilimit) {
            ip ++;
#ifdef ZSTD_USE_REP
            if ((offset) && (MEM_read32(ip) == MEM_read32(ip - offset_1))) {
                size_t mlRep = ZSTD_count(ip+MINMATCH, ip+MINMATCH-offset_1, iend) + MINMATCH;
                int gain2 = (int)(mlRep * 3);
                int gain1 = (int)(matchLength*3 - ZSTD_highbit((U32)offset+1) + 1);
                if ((mlRep >= MINMATCH) && (gain2 > gain1))
                    matchLength = mlRep, offset = 0, start = ip;
            }
#endif
            {
                size_t offset2=999999;
                size_t ml2 = searchMax(ctx, ip, iend, &offset2, maxSearches, mls);
                int gain2 = (int)(ml2*4 - ZSTD_highbit((U32)offset2+1));   /* raw approx */
                int gain1 = (int)(matchLength*4 - ZSTD_highbit((U32)offset+1) + 4);
                if ((ml2 >= MINMATCH) && (gain2 > gain1)) {
                    matchLength = ml2, offset = offset2, start = ip;
                    continue;   /* search a better one */
            }   }

            break;  /* nothing found : store previous solution */
        }
#endif

        /* store sequence */
_storeSequence:

        /* catch up */
        if (offset) {
            while ((start>anchor) && (start>base+offset) && (start[-1] == start[-1-offset]))   /* only search for offset within prefix */
                { start--; matchLength++; }
            offset_2 = offset_1; offset_1 = offset;
        }

        {
            size_t litLength = start - anchor;
            ZSTD_LOG_ENCODE("%d/%d: ENCODE literals=%d off=%d mlen=%d offset_1=%d offset_2=%d\n", (int)(ip-base), (int)(iend-base), (int)(litLength), (int)(offset), (int)matchLength, (int)offset_1, (int)offset_2);
            ZSTD_storeSeq(seqStorePtr, litLength, anchor, offset, matchLength-MINMATCH);
            anchor = ip = start + matchLength;
        }

#ifdef ZSTD_USE_REP      /* check immediate repcode */
        while ( (ip <= ilimit)
             && (MEM_read32(ip) == MEM_read32(ip - offset_2)) ) {
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
#endif   
    }

    /* Last Literals */
    {
        size_t lastLLSize = iend - anchor;
        ZSTD_LOG_ENCODE("%d/%d: ENCODE lastLLSize=%d\n", (int)(ip-base), (int)(iend-base), (int)(lastLLSize));
        memcpy(seqStorePtr->lit, anchor, lastLLSize);
        seqStorePtr->lit += lastLLSize;
    }
}




/* *******************************
*  Optimal parser
*********************************/
FORCE_INLINE
void ZSTD_compressBlock_opt_generic(ZSTD_CCtx* ctx,
                                     const void* src, size_t srcSize,
                                     const U32 searchMethod, const U32 depth)
{
    seqStore_t* seqStorePtr = &(ctx->seqStore);
    const BYTE* const istart = (const BYTE*)src;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const BYTE* const iend = istart + srcSize;
    const BYTE* const ilimit = iend - 8;
    const BYTE* const base = ctx->base + ctx->dictLimit;

    size_t rep_2=REPCODE_STARTVALUE, rep_1=REPCODE_STARTVALUE;
    const U32 maxSearches = 1 << ctx->params.searchLog;
    const U32 mls = ctx->params.searchLength;

    typedef size_t (*getAllMatches_f)(ZSTD_CCtx* zc, const BYTE* ip, const BYTE* iLimit,
                        U32 maxNbAttempts, U32 matchLengthSearch, ZSTD_match_t* matches, size_t minml);
    getAllMatches_f getAllMatches = searchMethod ? ZSTD_BtGetAllMatches_selectMLS : ZSTD_HcGetAllMatches_selectMLS;

    ZSTD_optimal_t opt[ZSTD_OPT_NUM+4];
    ZSTD_match_t matches[ZSTD_OPT_NUM+1];
    const uint8_t *inr;
    int cur, cur2, cur_min, skip_num = 0;
    int llen, litlen, price, match_num, last_pos;
  
    const int sufficient_len = 128; //ctx->params.sufficientLength;
    const int faster_get_matches = (ctx->params.strategy == ZSTD_opt); 


  //  printf("orig_file="); print_hex_text(ip, srcSize, 0);

    /* init */
    ZSTD_resetSeqStore(seqStorePtr);
    if ((ip-base) < REPCODE_STARTVALUE) ip = base + REPCODE_STARTVALUE;


    /* Match Loop */
    while (ip < ilimit) {
        int mlen=0;
        int best_mlen=0;
        int best_off=0;
        memset(opt, 0, sizeof(ZSTD_optimal_t));
        last_pos = 0;
        llen = ip - anchor;
        inr = ip;


        /* check repCode */
        if (MEM_read32(ip+1) == MEM_read32(ip+1 - rep_1)) {
            /* repcode : we take it */
            mlen = ZSTD_count(ip+1+MINMATCH, ip+1+MINMATCH-rep_1, iend) + MINMATCH;
            
            ZSTD_LOG_PARSER("%d: start try REP rep=%d mlen=%d\n", (int)(ip-base), (int)rep_1, (int)mlen);
            if (depth==0 || mlen > sufficient_len || mlen >= ZSTD_OPT_NUM) {
                ip+=1; best_mlen = mlen; best_off = 0; cur = 0; last_pos = 1;
                opt[0].rep = rep_1;
                goto _storeSequence;
            }

            do
            {
                price = ZSTD_getPrice(seqStorePtr, llen + 1, anchor, 0, mlen - MINMATCH) - ZSTD_LIT_COST(llen + 1);
                if (mlen + 1 > last_pos || price < opt[mlen + 1].price)
                    SET_PRICE(mlen + 1, mlen, 0, 1, price);
                mlen--;
            }
            while (mlen >= MINMATCH);
        }


       best_mlen = (last_pos) ? last_pos : MINMATCH;
        
       if (faster_get_matches && last_pos)
           match_num = 0;
       else
       {
            /* first search (depth 0) */
           match_num = getAllMatches(ctx, ip, iend, maxSearches, mls, matches, best_mlen); 
       }

       ZSTD_LOG_PARSER("%d: match_num=%d last_pos=%d\n", (int)(ip-base), match_num, last_pos);
       if (!last_pos && !match_num) { ip++; continue; }

        opt[0].rep = rep_2;
        opt[0].rep2 = rep_1;
        opt[0].mlen = 1;

       if (match_num && matches[match_num-1].len > sufficient_len)
       {
            best_mlen = matches[match_num-1].len;
            best_off = matches[match_num-1].off;
            cur = 0;
            last_pos = 1;
            goto _storeSequence;
       }

       // set prices using matches at position = 0
       for (int i = 0; i < match_num; i++)
       {
           mlen = (i>0) ? matches[i-1].len+1 : best_mlen;
           best_mlen = (matches[i].len < ZSTD_OPT_NUM) ? matches[i].len : ZSTD_OPT_NUM;
           ZSTD_LOG_PARSER("%d: start Found mlen=%d off=%d best_mlen=%d last_pos=%d\n", (int)(ip-base), matches[i].len, matches[i].off, (int)best_mlen, (int)last_pos);
           while (mlen <= best_mlen)
           {
                litlen = 0;
                price = ZSTD_getPrice(seqStorePtr, llen + litlen, anchor, matches[i].off, mlen - MINMATCH) - ZSTD_LIT_COST(llen);
                if (mlen > last_pos || price < opt[mlen].price)
                    SET_PRICE(mlen, mlen, matches[i].off, litlen, price);
                mlen++;
           }
        }

        if (last_pos < MINMATCH) { 
     //     ip += ((ip-anchor) >> g_searchStrength) + 1;   /* jump faster over incompressible sections */
            ip++; continue; 
        }


        // check further positions
        for (skip_num = 0, cur = 1; cur <= last_pos; cur++)
        { 
           inr = ip + cur;

           if (opt[cur-1].mlen == 1)
           {
                litlen = opt[cur-1].litlen + 1;
                
                if (cur > litlen)
                {
                    price = opt[cur - litlen].price + ZSTD_getLiteralPrice(seqStorePtr, litlen, inr-litlen);
                    ZSTD_LOG_TRY_PRICE("%d: TRY1 opt[%d].price=%d price=%d cur=%d litlen=%d\n", (int)(inr-base), cur - litlen, opt[cur - litlen].price, price, cur, litlen);
                }
                else
                {
                    price = ZSTD_getLiteralPrice(seqStorePtr, llen + litlen, anchor) - ZSTD_LIT_COST(llen);
                    ZSTD_LOG_TRY_PRICE("%d: TRY2 price=%d cur=%d litlen=%d llen=%d\n", (int)(inr-base), price, cur, litlen, llen);
                }
           }
           else
           {
                litlen = 1;
                price = opt[cur - 1].price + ZSTD_getLiteralPrice(seqStorePtr, litlen, inr-1);                  
                ZSTD_LOG_TRY_PRICE("%d: TRY3 price=%d cur=%d litlen=%d litonly=%d\n", (int)(inr-base), price, cur, litlen, (int)ZSTD_getLiteralPrice(seqStorePtr, litlen, inr-1));
           }

           mlen = 1;
           best_mlen = 0;
           ZSTD_LOG_TRY_PRICE("%d: TRY4 price=%d opt[%d].price=%d\n", (int)(inr-base), price, cur, opt[cur].price);

           if (cur > last_pos || price <= opt[cur].price) // || ((price == opt[cur].price) && (opt[cur-1].mlen == 1) && (cur != litlen)))
                SET_PRICE(cur, mlen, best_mlen, litlen, price);

           if (cur == last_pos) break;



            mlen = opt[cur].mlen;
            
            if (opt[cur-mlen].off)
            {
                opt[cur].rep2 = opt[cur-mlen].rep;
                opt[cur].rep = opt[cur-mlen].off;
                ZSTD_LOG_PARSER("%d: COPYREP1 cur=%d mlen=%d rep=%d rep2=%d\n", (int)(inr-base), cur, mlen, opt[cur].rep, opt[cur].rep2);
            }
            else
            {
                if (opt[cur-mlen].litlen == 0) 
                {
                    opt[cur].rep2 = opt[cur-mlen].rep;
                    opt[cur].rep = opt[cur-mlen].rep2;
                }
                else
                {
                    opt[cur].rep2 = opt[cur-mlen].rep2;
                    opt[cur].rep = opt[cur-mlen].rep;
                }
            }

           ZSTD_LOG_PARSER("%d: CURRENT price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d rep2=%d\n", (int)(inr-base), cur, last_pos, opt[cur].price, opt[cur].off, opt[cur].mlen, opt[cur].litlen, opt[cur].rep, opt[cur].rep2); 



           // best_mlen = 0;
           mlen = ZSTD_count(inr, inr - opt[cur].rep, iend); // check rep
           if (mlen >= MINMATCH && mlen > best_mlen)
           {
              ZSTD_LOG_PARSER("%d: try REP rep=%d mlen=%d\n", (int)(inr-base), opt[cur].rep, mlen);   
              ZSTD_LOG_PARSER("%d: Found REP mlen=%d off=%d rep=%d opt[%d].off=%d\n", (int)(inr-base), mlen, 0, opt[cur].rep, cur, opt[cur].off);

              if (mlen > sufficient_len || cur + mlen >= ZSTD_OPT_NUM)
              {
                best_mlen = mlen;
                best_off = 0;
                ZSTD_LOG_PARSER("%d: REP sufficient_len=%d best_mlen=%d best_off=%d last_pos=%d\n", (int)(inr-base), sufficient_len, best_mlen, best_off, last_pos);
                last_pos = cur + 1;
                goto _storeSequence;
               }

               if (opt[cur].mlen == 1)
               {
                    litlen = opt[cur].litlen;

                    if (cur > litlen)
                    {
                        price = opt[cur - litlen].price + ZSTD_getPrice(seqStorePtr, litlen, inr-litlen, 0, mlen - MINMATCH);
                        ZSTD_LOG_TRY_PRICE("%d: TRY5 opt[%d].price=%d price=%d cur=%d litlen=%d\n", (int)(inr-base), cur - litlen, opt[cur - litlen].price, price, cur, litlen);
                    }
                    else
                    {
                        price = ZSTD_getPrice(seqStorePtr, llen + litlen, anchor, 0, mlen - MINMATCH) - ZSTD_LIT_COST(llen);
                        ZSTD_LOG_TRY_PRICE("%d: TRY6 price=%d cur=%d litlen=%d llen=%d\n", (int)(inr-base), price, cur, litlen, llen);
                    }
                }
                else
                {
                    litlen = 0;
                    price = opt[cur].price + ZSTD_getPrice(seqStorePtr, 0, NULL, 0, mlen - MINMATCH);
                    ZSTD_LOG_TRY_PRICE("%d: TRY7 price=%d cur=%d litlen=0 getprice=%d\n", (int)(inr-base), price, cur, (int)ZSTD_getPrice(seqStorePtr, 0, NULL, 0, mlen - MINMATCH));
                }

                best_mlen = mlen;
                if (faster_get_matches)
                    skip_num = best_mlen;

                ZSTD_LOG_PARSER("%d: Found REP mlen=%d off=%d price=%d litlen=%d price[%d]=%d\n", (int)(inr-base), mlen, 0, price, litlen, cur - litlen, opt[cur - litlen].price);

                do
                {
                    if (cur + mlen > last_pos || price <= opt[cur + mlen].price) // || ((price == opt[cur + mlen].price) && (opt[cur].mlen == 1) && (cur != litlen))) // at equal price prefer REP instead of MATCH
                        SET_PRICE(cur + mlen, mlen, 0, litlen, price);
                    mlen--;
                }
                while (mlen >= MINMATCH);
            }


            if (faster_get_matches && skip_num > 0)
            {
                skip_num--; 
                continue;
            }


            best_mlen = (best_mlen > MINMATCH) ? best_mlen : MINMATCH;      

            match_num = getAllMatches(ctx, inr, iend, maxSearches, mls, matches, best_mlen); 
            ZSTD_LOG_PARSER("%d: ZSTD_GetAllMatches match_num=%d\n", (int)(inr-base), match_num);


            if (match_num > 0 && matches[match_num-1].len > sufficient_len)
            {
                cur -= matches[match_num-1].back;
                best_mlen = matches[match_num-1].len;
                best_off = matches[match_num-1].off;
                last_pos = cur + 1;
                goto _storeSequence;
            }

            cur_min = cur;

            // set prices using matches at position = cur
            for (int i = 0; i < match_num; i++)
            {
                mlen = (i>0) ? matches[i-1].len+1 : best_mlen;
                cur2 = cur - matches[i].back;
                best_mlen = (cur2 + matches[i].len < ZSTD_OPT_NUM) ? matches[i].len : ZSTD_OPT_NUM - cur2;

                ZSTD_LOG_PARSER("%d: Found1 cur=%d cur2=%d mlen=%d off=%d best_mlen=%d last_pos=%d\n", (int)(inr-base), cur, cur2, matches[i].len, matches[i].off, best_mlen, last_pos);

                while (mlen <= best_mlen)
                {
                    if (opt[cur2].mlen == 1)
                    {
                        litlen = opt[cur2].litlen;

                        if (cur2 > litlen)
                            price = opt[cur2 - litlen].price + ZSTD_getPrice(seqStorePtr, litlen, ip+cur2-litlen, matches[i].off, mlen - MINMATCH);
                        else
                            price = ZSTD_getPrice(seqStorePtr, llen + litlen, anchor, matches[i].off, mlen - MINMATCH) - ZSTD_LIT_COST(llen);
                    }
                    else
                    {
                        litlen = 0;
                        price = opt[cur2].price + ZSTD_getPrice(seqStorePtr, 0, NULL, matches[i].off, mlen - MINMATCH);
                    }

                //    ZSTD_LOG_PARSER("%d: Found2 pred=%d mlen=%d best_mlen=%d off=%d price=%d litlen=%d price[%d]=%d\n", (int)(inr-base), matches[i].back, mlen, best_mlen, matches[i].off, price, litlen, cur - litlen, opt[cur - litlen].price);
                //    ZSTD_LOG_TRY_PRICE("%d: TRY8 price=%d opt[%d].price=%d\n", (int)(inr-base), price, cur2 + mlen, opt[cur2 + mlen].price);

                    if (cur2 + mlen > last_pos || (price < opt[cur2 + mlen].price))
                    {
                        SET_PRICE(cur2 + mlen, mlen, matches[i].off, litlen, price);

                    //    opt[cur2 + mlen].rep = matches[i].off; // update reps
                    //    opt[cur2 + mlen].rep2 = opt[cur2].rep;
                    }

                    mlen++;
                }
            }
            
            if (cur_min < cur)
            {
                for (int i=cur_min-1; i<=last_pos; i++)
                {
                    ZSTD_LOG_PARSER("%d: BEFORE price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d\n", (int)(ip-base+i), i, last_pos, opt[i].price, opt[i].off, opt[i].mlen, opt[i].litlen, opt[i].rep); 
                }

                for (int i=cur_min+1; i<=last_pos; i++)
                if (opt[i].price < (1<<30) && (opt[i].off) < 1 && i - opt[i].mlen > cur_min) // invalidate reps
                {
                   if (opt[i-1].mlen == 1)
                   {
                        litlen = opt[i-1].litlen + 1;
                        
                        if (i > litlen)
                        {
                            price = opt[i - litlen].price + ZSTD_getLiteralPrice(seqStorePtr, litlen, ip+i-litlen);
                        	ZSTD_LOG_TRY_PRICE("%d: TRY9 opt[%d].price=%d price=%d cur=%d litlen=%d\n", (int)(inr-base), i - litlen, opt[i - litlen].price, price, i, litlen);
                        }
                        else
                        {
                            price = ZSTD_getLiteralPrice(seqStorePtr, llen + litlen, anchor) - ZSTD_LIT_COST(llen);
                        	ZSTD_LOG_TRY_PRICE("%d: TRY10 price=%d cur=%d litlen=%d llen=%d\n", (int)(inr-base), price, i, litlen, llen);
                        }
                    }
                    else
                    {
                        litlen = 1;
                        price = opt[i - 1].price + ZSTD_getLiteralPrice(seqStorePtr, 1, ip+i-1);
                        ZSTD_LOG_TRY_PRICE("%d: TRY11 price=%d cur=%d litlen=%d\n", (int)(inr-base), price, i, litlen);
                    }

                    mlen = 1;
                    best_mlen = 0;
                    ZSTD_LOG_TRY_PRICE("%d: TRY12 price=%d opt[%d].price=%d\n", (int)(inr-base), price, i + mlen, opt[i + mlen].price);
                    SET_PRICE(i, mlen, best_mlen, litlen, price);

                 //   opt[i].rep = opt[i-1].rep; // copy reps
                 //   opt[i].rep2 = opt[i-1].rep2; // copy reps
                
                    ZSTD_LOG_PARSER("%d: INVALIDATE pred=%d price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d\n", (int)(inr-base), cur-cur_min, i, last_pos, opt[i].price, opt[i].off, opt[i].mlen, opt[i].litlen, opt[i].rep);
                }
                
                for (int i=cur_min-1; i<=last_pos; i++)
                {
                    ZSTD_LOG_PARSER("%d: AFTER price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d\n", (int)(ip-base+i), i, last_pos, opt[i].price, opt[i].off, opt[i].mlen, opt[i].litlen, opt[i].rep); 
                }
                
            }
        } //  for (skip_num = 0, cur = 1; cur <= last_pos; cur++)


        best_mlen = opt[last_pos].mlen;
        best_off = opt[last_pos].off;
        cur = last_pos - best_mlen;
   //     printf("%d: start=%d best_mlen=%d best_off=%d cur=%d\n", (int)(ip - base), (int)(start - ip), (int)best_mlen, (int)best_off, cur);

        /* store sequence */
_storeSequence: // cur, last_pos, best_mlen, best_off have to be set
        for (int i = 1; i <= last_pos; i++)
            ZSTD_LOG_PARSER("%d: price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d rep2=%d\n", (int)(ip-base+i), i, last_pos, opt[i].price, opt[i].off, opt[i].mlen, opt[i].litlen, opt[i].rep, opt[i].rep2); 
        ZSTD_LOG_PARSER("%d: cur=%d/%d best_mlen=%d best_off=%d rep=%d\n", (int)(ip-base+cur), (int)cur, (int)last_pos, (int)best_mlen, (int)best_off, opt[cur].rep); 

        opt[0].mlen = 1;
        size_t offset;
        
        while (cur >= 0)
        {
            mlen = opt[cur].mlen;
            offset = opt[cur].off;
            opt[cur].mlen = best_mlen; 
            opt[cur].off = best_off;
            best_mlen = mlen;
            best_off = offset; 
            cur -= mlen;
        }
          
        for (int i = 0; i <= last_pos;)
        {
            ZSTD_LOG_PARSER("%d: price2[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d rep2=%d\n", (int)(ip-base+i), i, last_pos, opt[i].price, opt[i].off, opt[i].mlen, opt[i].litlen, opt[i].rep, opt[i].rep2); 
            i += opt[i].mlen;
        }

        cur = 0;

        while (cur < last_pos)
        {
            ZSTD_LOG_PARSER("%d: price3[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d rep2=%d\n", (int)(ip-base+cur), cur, last_pos, opt[cur].price, opt[cur].off, opt[cur].mlen, opt[cur].litlen, opt[cur].rep, opt[cur].rep2); 
            mlen = opt[cur].mlen;
            if (mlen == 1) { ip++; cur++; continue; }
            offset = opt[cur].off;
            cur += mlen;


            size_t litLength = ip - anchor;
            ZSTD_LOG_ENCODE("%d/%d: BEFORE_ENCODE literals=%d mlen=%d off=%d rep1=%d rep2=%d\n", (int)(ip-base), (int)(iend-base), (int)(litLength), (int)mlen, (int)(offset), (int)rep_1, (int)rep_2);

            if (offset)
            {
                rep_2 = rep_1;
                rep_1 = offset;
            }
            else
            {
                if (litLength == 0) 
                {
                    best_off = rep_2;
                    rep_2 = rep_1;
                    rep_1 = best_off;
                }
            }

            ZSTD_LOG_ENCODE("%d/%d: ENCODE literals=%d mlen=%d off=%d rep1=%d rep2=%d\n", (int)(ip-base), (int)(iend-base), (int)(litLength), (int)mlen, (int)(offset), (int)rep_1, (int)rep_2);
       //     printf("orig="); print_hex_text(ip, mlen, 0);
       //     printf("match="); print_hex_text(ip-offset, mlen, 0);

#if 1
            size_t ml2;
            if (offset)
                ml2 = ZSTD_count(ip, ip-offset, iend);
            else
                ml2 = ZSTD_count(ip, ip-rep_1, iend);

            if (ml2 < mlen && ml2 < MINMATCH)
            {
                printf("%d: ERROR iend=%d mlen=%d offset=%d ml2=%d\n", (int)(ip - base), (int)(iend - ip), (int)mlen, (int)offset, (int)ml2);
                exit(0);
            }

            if (ip < anchor)
            {
                printf("%d: ERROR ip < anchor iend=%d mlen=%d offset=%d\n", (int)(ip - base), (int)(iend - ip), (int)mlen, (int)offset);
                exit(0);
            }
            if (ip - offset < base)
            {
                printf("%d: ERROR ip - offset < base iend=%d mlen=%d offset=%d\n", (int)(ip - base), (int)(iend - ip), (int)mlen, (int)offset);
                exit(0);
            }
            if (mlen < MINMATCH)
            {
                printf("%d: ERROR mlen < MINMATCH iend=%d mlen=%d offset=%d\n", (int)(ip - base), (int)(iend - ip), (int)mlen, (int)offset);
                exit(0);
            }
            if (ip + mlen > iend) 
            {
                printf("%d: ERROR ip + mlen >= iend iend=%d mlen=%d offset=%d\n", (int)(ip - base), (int)(iend - ip), (int)mlen, (int)offset);
                exit(0);
            }
#endif

            ZSTD_updatePrice(seqStorePtr, litLength, anchor, offset, mlen-MINMATCH);
            ZSTD_storeSeq(seqStorePtr, litLength, anchor, offset, mlen-MINMATCH);
            anchor = ip = ip + mlen;
        }


       // check immediate repcode
        while ( (anchor <= ilimit)
             && (MEM_read32(anchor) == MEM_read32(anchor - rep_2)) ) {
            /* store sequence */
            best_mlen = ZSTD_count(anchor+MINMATCH, anchor+MINMATCH-rep_2, iend);
            best_off = rep_2;
            rep_2 = rep_1;
            rep_1 = best_off;
            ZSTD_LOG_ENCODE("%d/%d: ENCODE REP literals=%d mlen=%d off=%d rep1=%d rep2=%d\n", (int)(anchor-base), (int)(iend-base), (int)(0), (int)best_mlen, (int)(0), (int)rep_1, (int)rep_2);
            ZSTD_updatePrice(seqStorePtr, 0, anchor, 0, best_mlen);
            ZSTD_storeSeq(seqStorePtr, 0, anchor, 0, best_mlen);
            anchor += best_mlen+MINMATCH;
            ip = anchor;
            continue;   // faster when present ... (?)
        }    
    }

    /* Last Literals */
    {
        size_t lastLLSize = iend - anchor;
        ZSTD_LOG_ENCODE("%d: lastLLSize literals=%d\n", (int)(ip-base), (int)(lastLLSize));
        memcpy(seqStorePtr->lit, anchor, lastLLSize);
        seqStorePtr->lit += lastLLSize;
    }
}



