#include <stdio.h>

typedef struct
{
	int off;
	int len;
	int back;
} LZ5HC_match_t;

typedef struct
{
	int price;
	int off;
	int mlen;
	int litlen;
   	int rep;
} LZ5HC_optimal_t; 


#define LZ5HC_DEBUG(fmt, args...) ;//printf(fmt, ##args)
#define LZ5_LOG_PARSER(fmt, args...) ;// printf(fmt, ##args)
#define LZ5_LOG_PRICE(fmt, args...) ;//printf(fmt, ##args)
#define LZ5_LOG_ENCODE(fmt, args...) ;//printf(fmt, ##args) 

#define LZ5_OPT_NUM   (1<<12)

#define LZ5_SHORT_OFFSET_BITS 10
#define LZ5_SHORT_OFFSET_DISTANCE (1<<LZ5_SHORT_OFFSET_BITS)
#define LZ5_MID_OFFSET_BITS 16
#define LZ5_MID_OFFSET_DISTANCE (1<<LZ5_MID_OFFSET_BITS)

#define RUN_BITS 3
#define RUN_BITS2 2

#define LZ5_SHORT_LITERALS          ((1<<RUN_BITS2)-1)
#define LZ5_LITERALS                ((1<<RUN_BITS)-1)

#define LZ5_SHORT_LITLEN_COST(len)  (len<LZ5_SHORT_LITERALS ? 0 : (len-LZ5_SHORT_LITERALS < 255 ? 1 : (len-LZ5_SHORT_LITERALS-255 < (1<<7) ? 2 : 3)))
#define LZ5_LEN_COST(len)           (len<LZ5_LITERALS ? 0 : (len-LZ5_LITERALS < 255 ? 1 : (len-LZ5_LITERALS-255 < (1<<7) ? 2 : 3)))

static size_t LZ5_LIT_COST(size_t len, size_t offset){ return (len)+(((offset > LZ5_MID_OFFSET_DISTANCE) || (offset<LZ5_SHORT_OFFSET_DISTANCE)) ? LZ5_SHORT_LITLEN_COST(len) : LZ5_LEN_COST(len)); }
static size_t LZ5_MATCH_COST(size_t mlen, size_t offset) { return LZ5_LEN_COST(mlen) + ((offset == 0) ? 1 : (offset<LZ5_SHORT_OFFSET_DISTANCE ? 2 : (offset<LZ5_MID_OFFSET_DISTANCE ? 3 : 4))); }

#define LZ5_CODEWORD_COST(litlen,offset,mlen)   (LZ5_MATCH_COST(mlen,offset) + LZ5_LIT_COST(litlen,offset))
#define LZ5_LIT_ONLY_COST(len)                  ((len)+(LZ5_LEN_COST(len))+1)

#define LZ5_NORMAL_MATCH_COST(mlen,offset)  (LZ5_MATCH_COST(mlen,offset))
#define LZ5_NORMAL_LIT_COST(len)            (len) 


FORCE_INLINE U32 LZ5HC_get_price(U32 litlen, U32 offset, U32 mlen)
{
	return LZ5_CODEWORD_COST(litlen, offset, mlen);
}


#define SET_PRICE(pos, mlen, offset, litlen, price)   \
    {                                                 \
        while (last_pos < pos)  { opt[last_pos+1].price = 1<<30; last_pos++; } \
        opt[pos].mlen = mlen;                         \
        opt[pos].off = offset;                        \
        opt[pos].litlen = litlen;                     \
        opt[pos].price = price;                       \
        LZ5_LOG_PARSER("%d: SET price[%d/%d]=%d litlen=%d len=%d off=%d\n", (int)(inr-istart), (int)pos, (int)last_pos, opt[pos].price, opt[pos].litlen, opt[pos].mlen, opt[pos].off); \
    }



FORCE_INLINE /* inlining is important to hardwire a hot branch (template emulation) */
size_t ZSTD_insertBtAndGetAllMatches (
                        ZSTD_CCtx* zc,
                        const BYTE* const ip, const BYTE* const iend,
                        U32 nbCompares, const U32 mls,
                        U32 extDict, LZ5HC_match_t* matches)
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
    size_t mnum = 0;
    
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

        if (matchLength > bestLength) {
            if (matchLength > matchEndIdx - matchIndex)
                matchEndIdx = matchIndex + (U32)matchLength;
#if 0
            if ( (4*(int)(matchLength-bestLength)) > (int)(ZSTD_highbit(current-matchIndex+1) - ZSTD_highbit((U32)offsetPtr[0]+1)) )
                bestLength = matchLength, *offsetPtr = current - matchIndex;
#else
            if (mnum ==  0 ||  (4*(int)(matchLength-bestLength)) > (int)(ZSTD_highbit(current-matchIndex+1) - ZSTD_highbit((U32)matches[mnum-1].off+1)) )
            {
                bestLength = matchLength; 
                matches[mnum].off = current - matchIndex;
                matches[mnum].len = matchLength;
                matches[mnum].back = 0;
                mnum++;
                if (matchLength > LZ5_OPT_NUM) break;
            }
#endif
            if (ip+matchLength == iend)   /* equal : no way to know if inf or sup */
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
                        const U32 maxNbAttempts, const U32 mls, LZ5HC_match_t* matches)
{
    if (ip < zc->base + zc->nextToUpdate) return 0;   /* skipped area */
    ZSTD_updateTree(zc, ip, iLimit, maxNbAttempts, mls);
    return ZSTD_insertBtAndGetAllMatches(zc, ip, iLimit, maxNbAttempts, mls, 0, matches);
}


FORCE_INLINE size_t ZSTD_BtGetAllMatches_selectMLS (
                        ZSTD_CCtx* zc,   /* Index table will be updated */
                        const BYTE* ip, const BYTE* const iLimit,
                        const U32 maxNbAttempts, const U32 matchLengthSearch, LZ5HC_match_t* matches)
{
    switch(matchLengthSearch)
    {
    default :
    case 4 : return ZSTD_BtGetAllMatches(zc, ip, iLimit, maxNbAttempts, 4, matches);
    case 5 : return ZSTD_BtGetAllMatches(zc, ip, iLimit, maxNbAttempts, 5, matches);
    case 6 : return ZSTD_BtGetAllMatches(zc, ip, iLimit, maxNbAttempts, 6, matches);
    }
}


FORCE_INLINE /* inlining is important to hardwire a hot branch (template emulation) */
size_t ZSTD_HcGetAllMatches_generic (
                        ZSTD_CCtx* zc,   /* Index table will be updated */
                        const BYTE* const ip, const BYTE* const iLimit,
                        const U32 maxNbAttempts, const U32 mls, const U32 extDict, LZ5HC_match_t* matches)
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
    size_t mnum = 0;

    /* HC4 match finder */
    matchIndex = ZSTD_insertAndFindFirstIndex (zc, ip, mls);

    while ((matchIndex>lowLimit) && (nbAttempts)) {
        size_t currentMl=0;
        nbAttempts--;
        if ((!extDict) || matchIndex >= dictLimit) {
            match = base + matchIndex;
            if (match[ml] == ip[ml])   /* potentially better */
                currentMl = ZSTD_count(ip, match, iLimit);
        } else {
            match = dictBase + matchIndex;
            if (MEM_read32(match) == MEM_read32(ip))   /* assumption : matchIndex <= dictLimit-4 (by table construction) */
                currentMl = ZSTD_count_2segments(ip+MINMATCH, match+MINMATCH, iLimit, dictEnd, prefixStart) + MINMATCH;
        }

        /* save best solution */
        if (currentMl > ml) { 
            ml = currentMl; 
            matches[mnum].off = current - matchIndex;
            matches[mnum].len = currentMl;
            matches[mnum].back = 0;
            mnum++;
            if (currentMl > LZ5_OPT_NUM) break;
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
                        const U32 maxNbAttempts, const U32 matchLengthSearch, LZ5HC_match_t* matches)
{
    switch(matchLengthSearch)
    {
    default :
    case 4 : return ZSTD_HcGetAllMatches_generic(zc, ip, iLimit, maxNbAttempts, 4, 0, matches);
    case 5 : return ZSTD_HcGetAllMatches_generic(zc, ip, iLimit, maxNbAttempts, 5, 0, matches);
    case 6 : return ZSTD_HcGetAllMatches_generic(zc, ip, iLimit, maxNbAttempts, 6, 0, matches);
    }
}



/* *******************************
*  Optimal parser OLD
*********************************/
FORCE_INLINE
void ZSTD_compressBlock_opt_generic_OLD(ZSTD_CCtx* ctx,
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
    typedef size_t (*getAllMatches_f)(ZSTD_CCtx* zc, const BYTE* ip, const BYTE* iLimit,
                        U32 maxNbAttempts, U32 matchLengthSearch, LZ5HC_match_t* matches);
    searchMax_f searchMax = searchMethod ? ZSTD_BtFindBestMatch_selectMLS : ZSTD_HcFindBestMatch_selectMLS;
    getAllMatches_f getAllMatches = searchMethod ? ZSTD_BtGetAllMatches_selectMLS : ZSTD_HcGetAllMatches_selectMLS;

    LZ5HC_match_t matches[LZ5_OPT_NUM+1];
    
    /* init */
    ZSTD_resetSeqStore(seqStorePtr);
    if ((ip-base) < REPCODE_STARTVALUE) ip = base + REPCODE_STARTVALUE;

    /* Match Loop */
    while (ip < ilimit) {
        size_t matchLength=0;
        size_t offset=0;
        const BYTE* start=ip+1;

#if 1
        /* check repCode */
        if (MEM_read32(start) == MEM_read32(start - offset_1)) {
            /* repcode : we take it */
            matchLength = ZSTD_count(start+MINMATCH, start+MINMATCH-offset_1, iend) + MINMATCH;
            if (depth==0) goto _storeSequence;
        }
#endif

        {
            /* first search (depth 0) */
#if 0
            size_t offsetFound = 99999999;
            size_t ml2 = searchMax(ctx, ip, iend, &offsetFound, maxSearches, mls);
            if (ml2 > matchLength)
                matchLength = ml2, start = ip, offset=offsetFound;
#else
            size_t mnum = getAllMatches(ctx, ip, iend, maxSearches, mls, matches); 
            if (mnum > 0) {
                if (matches[mnum-1].len > matchLength)
                    matchLength = matches[mnum-1].len, start = ip, offset=matches[mnum-1].off;
            }
#endif
        }

        if (matchLength < MINMATCH) {
       //     ip += ((ip-anchor) >> g_searchStrength) + 1;   /* jump faster over incompressible sections */
            ip++;
            continue;
        }

#if 0
        /* let's try to find a better solution */
        if (depth>=1)
        while (ip<ilimit) {
            ip ++;
            if ((offset) && (MEM_read32(ip) == MEM_read32(ip - offset_1))) {
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
                if ((ml2 >= MINMATCH) && (gain2 > gain1)) {
                    matchLength = ml2, offset = offset2, start = ip;
                    continue;   /* search a better one */
            }   }

            break;  /* nothing found : store previous solution */
        }
#endif

        /* catch up */
        if (offset) {
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

#if 1       /* check immediate repcode */
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
                        U32 maxNbAttempts, U32 matchLengthSearch, LZ5HC_match_t* matches);
    getAllMatches_f getAllMatches = searchMethod ? ZSTD_BtGetAllMatches_selectMLS : ZSTD_HcGetAllMatches_selectMLS;

    LZ5HC_optimal_t opt[LZ5_OPT_NUM+4];
    LZ5HC_match_t matches[LZ5_OPT_NUM+1];
    const uint8_t *inr;
    int cur, cur2, cur_min, skip_num = 0;
    int llen, litlen, price, match_num, last_pos;
  
    const int sufficient_len = 128; //ctx->params.sufficientLength;
    const int faster_get_matches = 0;//(ctx->params.strategy == ZSTD_opt); 

    /* init */
    ZSTD_resetSeqStore(seqStorePtr);
    if ((ip-base) < REPCODE_STARTVALUE) ip = base + REPCODE_STARTVALUE;

    /* Match Loop */
    while (ip < ilimit) {
        size_t mlen=0;
        size_t best_mlen=0;
        size_t best_off=0;
        memset(opt, 0, sizeof(LZ5HC_optimal_t));
        last_pos = 0;
        llen = ip - anchor;
        inr = ip;

        /* check repCode */
        if (MEM_read32(ip+1) == MEM_read32(ip+1 - rep_1)) {
            /* repcode : we take it */
            mlen = ZSTD_count(ip+1+MINMATCH, ip+1+MINMATCH-rep_1, iend) + MINMATCH;
            
            LZ5_LOG_PARSER("%d: start try REP rep=%d mlen=%d\n", (int)(ip-istart), (int)rep_1, (int)mlen);
            if (depth==0 || mlen > sufficient_len || mlen >= LZ5_OPT_NUM) {
                ip++; best_mlen = mlen; best_off = 0; cur = 0; last_pos = 1;
                goto _storeSequence;
            }

            do
            {
                litlen = 0;
                price = LZ5HC_get_price(llen, 0, mlen - MINMATCH) - llen;
                if (mlen + 1 > last_pos || price < opt[mlen + 1].price)
                    SET_PRICE(mlen + 1, mlen, 0, litlen, price);
                mlen--;
            }
            while (mlen >= MINMATCH);
        }

     //  best_mlen = (last_pos) ? last_pos : MINMATCH;
        
       if (faster_get_matches && last_pos)
           match_num = 0;
       else
       {
            /* first search (depth 0) */
           match_num = getAllMatches(ctx, ip, iend, maxSearches, mls, matches); 
       }

       LZ5_LOG_PARSER("%d: match_num=%d last_pos=%d\n", (int)(ip-istart), match_num, last_pos);
       if (!last_pos && !match_num) { ip++; continue; }

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
           best_mlen = (matches[i].len < LZ5_OPT_NUM) ? matches[i].len : LZ5_OPT_NUM;
           LZ5_LOG_PARSER("%d: start Found mlen=%d off=%d best_mlen=%d last_pos=%d\n", (int)(ip-istart), matches[i].len, matches[i].off, (int)best_mlen, (int)last_pos);
           while (mlen <= best_mlen)
           {
                litlen = 0;
                price = LZ5HC_get_price(llen + litlen, matches[i].off, mlen - MINMATCH) - llen;
                if (mlen > last_pos || price < opt[mlen].price)
                    SET_PRICE(mlen, mlen, matches[i].off, litlen, price);
                mlen++;
           }
        }

        if (last_pos < MINMATCH) { 
     //     ip += ((ip-anchor) >> g_searchStrength) + 1;   /* jump faster over incompressible sections */
            ip++; continue; 
        }

  //      printf("%d: last_pos=%d\n", (int)(ip - istart), (int)last_pos);

        opt[0].rep = opt[1].rep = rep_1;
        opt[0].mlen = opt[1].mlen = 1;

#if 0
        // check further positions
        for (skip_num = 0, cur = 1; cur <= last_pos; cur++)
        { 
           inr = ip + cur;

           if (opt[cur-1].mlen == 1)
           {
                litlen = opt[cur-1].litlen + 1;
                
                if (cur != litlen)
                {
                    price = opt[cur - litlen].price + LZ5_LIT_ONLY_COST(litlen);
                    LZ5_LOG_PRICE("%d: TRY1 opt[%d].price=%d price=%d cur=%d litlen=%d\n", (int)((char*)inr-source), cur - litlen, opt[cur - litlen].price, price, cur, litlen);
                }
                else
                {
                    price = LZ5_LIT_ONLY_COST(llen + litlen) - llen;
                    LZ5_LOG_PRICE("%d: TRY2 price=%d cur=%d litlen=%d llen=%d\n", (int)((char*)inr-source), price, cur, litlen, llen);
                }
           }
           else
           {
                litlen = 1;
                price = opt[cur - 1].price + LZ5_LIT_ONLY_COST(litlen);                  
                LZ5_LOG_PRICE("%d: TRY3 price=%d cur=%d litlen=%d litonly=%d\n", (int)((char*)inr-source), price, cur, litlen, LZ5_LIT_ONLY_COST(litlen));
           }
           
           mlen = 1;
           best_mlen = 0;
           LZ5_LOG_PARSER("%d: TRY price=%d opt[%d].price=%d\n", (int)((char*)inr-source), price, cur, opt[cur].price);

           if (cur > last_pos || price <= opt[cur].price) // || ((price == opt[cur].price) && (opt[cur-1].mlen == 1) && (cur != litlen)))
                SET_PRICE(cur, mlen, best_mlen, litlen, price);

           if (cur == last_pos) break;

           if (opt[cur].mlen > 1)
           {
                mlen = opt[cur].mlen;
                if (opt[cur].off < 1)
                {
                    opt[cur].rep = opt[cur-mlen].rep;
                    LZ5_LOG_PARSER("%d: COPYREP1 cur=%d mlen=%d rep=%d\n", (int)((char*)inr-source), cur, mlen, opt[cur-mlen].rep);
                }
                else
                {
                    opt[cur].rep = 0;
                    LZ5_LOG_PARSER("%d: COPYREP2 cur=%d offset=%d rep=%d\n", (int)((char*)inr-source), cur, 0, opt[cur].rep);
                }
           }
           else
           {
                opt[cur].rep = opt[cur-1].rep; // copy rep
           }


            LZ5_LOG_PARSER("%d: CURRENT price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d\n", (int)((char*)inr-source), cur, last_pos, opt[cur].price, opt[cur].off, opt[cur].mlen, opt[cur].litlen, opt[cur].rep); 

           // check rep
           // best_mlen = 0;
           mlen = ZSTD_count(inr, inr - opt[cur].rep, iend);
           if (mlen >= MINMATCH && mlen > best_mlen)
           {
              LZ5_LOG_PARSER("%d: try REP rep=%d mlen=%d\n", (int)((char*)inr-source), opt[cur].rep, mlen);   
              LZ5_LOG_PARSER("%d: Found REP mlen=%d off=%d rep=%d opt[%d].off=%d\n", (int)((char*)inr-source), mlen, 0, opt[cur].rep, cur, opt[cur].off);

              if (mlen > sufficient_len || cur + mlen >= LZ5_OPT_NUM)
              {
                best_mlen = mlen;
                best_off = 0;
                LZ5_LOG_PARSER("%d: REP sufficient_len=%d best_mlen=%d best_off=%d last_pos=%d\n", (int)((char*)inr-source), sufficient_len, best_mlen, best_off, last_pos);
                last_pos = cur + 1;
                goto encode;
               }

               if (opt[cur].mlen == 1)
               {
                    litlen = opt[cur].litlen;

                    if (cur != litlen)
                    {
                        price = opt[cur - litlen].price + LZ5HC_get_price(litlen, 0, mlen - MINMATCH);
                        LZ5_LOG_PRICE("%d: TRY1 opt[%d].price=%d price=%d cur=%d litlen=%d\n", (int)((char*)inr-source), cur - litlen, opt[cur - litlen].price, price, cur, litlen);
                    }
                    else
                    {
                        price = LZ5HC_get_price(llen + litlen, 0, mlen - MINMATCH) - llen;
                        LZ5_LOG_PRICE("%d: TRY2 price=%d cur=%d litlen=%d llen=%d\n", (int)((char*)inr-source), price, cur, litlen, llen);
                    }
                }
                else
                {
                    litlen = 0;
                    price = opt[cur].price + LZ5HC_get_price(litlen, 0, mlen - MINMATCH);
                    LZ5_LOG_PRICE("%d: TRY3 price=%d cur=%d litlen=%d getprice=%d\n", (int)((char*)inr-source), price, cur, litlen, LZ5HC_get_price(litlen, 0, mlen - MINMATCH));
                }

                best_mlen = mlen;
                if (faster_get_matches)
                    skip_num = best_mlen;

                LZ5_LOG_PARSER("%d: Found REP mlen=%d off=%d price=%d litlen=%d price[%d]=%d\n", (int)((char*)inr-source), mlen, 0, price, litlen, cur - litlen, opt[cur - litlen].price);

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

            match_num = getAllMatches(ctx, inr, iend, maxSearches, mls, matches); 
         //   match_num = LZ5HC_GetAllMatches(ctx, inr, ip, matchlimit, best_mlen, matches);
            LZ5_LOG_PARSER("%d: LZ5HC_GetAllMatches match_num=%d\n", (int)((char*)inr-source), match_num);


            if (match_num > 0 && matches[match_num-1].len > sufficient_len)
            {
                cur -= matches[match_num-1].back;
                best_mlen = matches[match_num-1].len;
                best_off = matches[match_num-1].off;
                last_pos = cur + 1;
                goto encode;
            }

            cur_min = cur;

            // set prices using matches at position = cur
            for (int i = 0; i < match_num; i++)
            {
                mlen = (i>0) ? matches[i-1].len+1 : best_mlen;
                cur2 = cur - matches[i].back;
                best_mlen = (cur2 + matches[i].len < LZ5_OPT_NUM) ? matches[i].len : LZ5_OPT_NUM - cur2;
                LZ5_LOG_PARSER("%d: Found1 cur=%d cur2=%d mlen=%d off=%d best_mlen=%d last_pos=%d\n", (int)((char*)inr-source), cur, cur2, matches[i].len, matches[i].off, best_mlen, last_pos);

                while (mlen <= best_mlen)
                {
                    if (opt[cur2].mlen == 1)
                    {
                        litlen = opt[cur2].litlen;

                        if (cur2 != litlen)
                            price = opt[cur2 - litlen].price + LZ5HC_get_price(litlen, matches[i].off, mlen - MINMATCH);
                        else
                            price = LZ5HC_get_price(llen + litlen, matches[i].off, mlen - MINMATCH) - llen;
                    }
                    else
                    {
                        litlen = 0;
                        price = opt[cur2].price + LZ5HC_get_price(litlen, matches[i].off, mlen - MINMATCH);
                    }

                    LZ5_LOG_PARSER("%d: Found2 pred=%d mlen=%d best_mlen=%d off=%d price=%d litlen=%d price[%d]=%d\n", (int)((char*)inr-source), matches[i].back, mlen, best_mlen, matches[i].off, price, litlen, cur - litlen, opt[cur - litlen].price);
                    if (cur2 + mlen > last_pos || price < opt[cur2 + mlen].price)
                    {
                        SET_PRICE(cur2 + mlen, mlen, matches[i].off, litlen, price);

                        opt[cur2 + mlen].rep = matches[i].off; // update reps
                        if (cur2 < cur_min) cur_min = cur2;
                    }

                    mlen++;
                }
            }
            
            if (cur_min < cur)
            {
                for (int i=cur_min-1; i<=last_pos; i++)
                {
                    LZ5_LOG_PARSER("%d: BEFORE price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d\n", (int)((char*)ip-source+i), i, last_pos, opt[i].price, opt[i].off, opt[i].mlen, opt[i].litlen, opt[i].rep); 
                }

                for (int i=cur_min+1; i<=last_pos; i++)
                if (opt[i].price < (1<<30) && (opt[i].off) < 1 && i - opt[i].mlen > cur_min) // invalidate reps
                {
                   if (opt[i-1].mlen == 1)
                   {
                        litlen = opt[i-1].litlen + 1;
                        
                        if (i != litlen)
                        {
                            price = opt[i - litlen].price + LZ5_LIT_ONLY_COST(litlen);
                        //	LZ5_LOG_PRICE("%d: TRY1 opt[%d].price=%d price=%d cur=%d litlen=%d\n", (int)(inr-base), i - litlen, opt[i - litlen].price, price, i, litlen);
                        }
                        else
                        {
                            price = LZ5_LIT_ONLY_COST(llen + litlen) - llen;
                        //	LZ5_LOG_PRICE("%d: TRY2 price=%d cur=%d litlen=%d llen=%d\n", (int)(inr-base), price, i, litlen, llen);
                        }
                    }
                    else
                    {
                        litlen = 1;
                        price = opt[i - 1].price + LZ5_LIT_ONLY_COST(litlen);                  
                    //	LZ5_LOG_PRICE("%d: TRY3 price=%d cur=%d litlen=%d\n", (int)(inr-base), price, i, litlen);
                    }

                    mlen = 1;
                    best_mlen = 0;
                    SET_PRICE(i, mlen, best_mlen, litlen, price);

                    opt[i].rep = opt[i-1].rep; // copy reps

                    LZ5_LOG_PARSER("%d: INVALIDATE pred=%d price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d\n", (int)((char*)inr-source), cur-cur_min, i, last_pos, opt[i].price, opt[i].off, opt[i].mlen, opt[i].litlen, opt[i].rep);
                }
                
                for (int i=cur_min-1; i<=last_pos; i++)
                {
                    LZ5_LOG_PARSER("%d: AFTER price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d\n", (int)((char*)ip-source+i), i, last_pos, opt[i].price, opt[i].off, opt[i].mlen, opt[i].litlen, opt[i].rep); 
                }
                
            }
        } //  for (skip_num = 0, cur = 1; cur <= last_pos; cur++)
#endif

        best_mlen = opt[last_pos].mlen;
        best_off = opt[last_pos].off;
        cur = last_pos - best_mlen;
   //     printf("%d: start=%d best_mlen=%d best_off=%d cur=%d\n", (int)(ip - istart), (int)(start - ip), (int)best_mlen, (int)best_off, cur);

        /* store sequence */
_storeSequence: // cur, last_pos, best_mlen, best_off have to be set
        for (int i = 1; i <= last_pos; i++)
            LZ5_LOG_PARSER("%d: price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d\n", (int)(ip-istart+i), i, last_pos, opt[i].price, opt[i].off, opt[i].mlen, opt[i].litlen, opt[i].rep); 
        LZ5_LOG_PARSER("%d: cur=%d/%d best_mlen=%d best_off=%d rep=%d\n", (int)(ip-istart+cur), (int)cur, (int)last_pos, (int)best_mlen, (int)best_off, opt[cur].rep); 

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
            LZ5_LOG_PARSER("%d: price2[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d\n", (int)(ip-istart+i), i, last_pos, opt[i].price, opt[i].off, opt[i].mlen, opt[i].litlen, opt[i].rep); 
            i += opt[i].mlen;
        }

        cur = 0;

        while (cur < last_pos)
        {
            LZ5_LOG_PARSER("%d: price3[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d\n", (int)(ip-istart+cur), cur, last_pos, opt[cur].price, opt[cur].off, opt[cur].mlen, opt[cur].litlen, opt[cur].rep); 
            mlen = opt[cur].mlen;
            if (mlen == 1) { ip++; cur++; continue; }
            offset = opt[cur].off;
            cur += mlen;

#if 0
            if (offset)
            {
                size_t ml2 = ZSTD_count(start, start-offset, iend);
                printf("%d: mlen=%d offset=%d\n", (int)(ip - istart), (int)mlen, (int)offset);
                if (ml2 < mlen)
                {
                    printf("%d: start=%d iend=%d mlen=%d offset=%d ml2=%d\n", (int)(ip - istart), (int)(start - ip), (int)(iend - ip), (int)mlen, (int)offset, (int)ml2);
                    exit(0);
                }
            }
            else
                printf("%d: mlen=%d offset=%d\n", (int)(ip - istart), (int)mlen, (int)offset);
#endif

            LZ5_LOG_ENCODE("%d: ENCODE literals=%d off=%d mlen=%d ", (int)(ip-istart), (int)(ip-anchor), (int)(offset), (int)mlen);
            size_t litLength = ip - anchor;
            ZSTD_storeSeq(seqStorePtr, litLength, anchor, offset, mlen-MINMATCH);
            anchor = ip = ip + mlen;

            if (offset)
                rep_2 = rep_1, rep_1 = offset;

            LZ5_LOG_PARSER("%d: offset=%d rep=%d\n", (int)(ip-istart), (int)offset, (int)rep_1);
        }


       /* check immediate repcode */
        while ( (ip <= ilimit)
             && (MEM_read32(ip) == MEM_read32(ip - rep_2)) ) {
            /* store sequence */
            best_mlen = ZSTD_count(ip+MINMATCH, ip+MINMATCH-rep_2, iend);
            best_off = rep_2;
            rep_2 = rep_1;
            rep_1 = best_off;
            ZSTD_storeSeq(seqStorePtr, 0, anchor, 0, best_mlen);
            ip += best_mlen+MINMATCH;
            anchor = ip;
            continue;   /* faster when present ... (?) */
    }    }

    /* Last Literals */
    {
        size_t lastLLSize = iend - anchor;
        memcpy(seqStorePtr->lit, anchor, lastLLSize);
        seqStorePtr->lit += lastLLSize;
    }
}




#if 0
static int LZ5HC_compress_optimal_price (
    LZ5HC_Data_Structure* ctx,
    const char* source,
    char* dest,
    int inputSize,
    int maxOutputSize,
    limitedOutput_directive limit
    )
{
    ctx->inputBuffer = (const BYTE*) source;
    ctx->outputBuffer = (const BYTE*) dest;
    const BYTE* ip = (const BYTE*) source;
    const BYTE* anchor = ip;
    const BYTE* const iend = ip + inputSize;
    const BYTE* const mflimit = iend - MFLIMIT;
    const BYTE* const matchlimit = (iend - LASTLITERALS);
    BYTE* op = (BYTE*) dest;
    BYTE* const oend = op + maxOutputSize;
    const int sufficient_len = ctx->params.sufficientLength;
    const bool faster_get_matches = (ctx->params.fullSearch == 0); 
    
    LZ5HC_optimal_t opt[LZ5_OPT_NUM+4];
    LZ5HC_match_t matches[LZ5_OPT_NUM+1];
    const uint8_t *inr;
    int res, cur, cur2, cur_min, skip_num = 0;
    int rep, llen, litlen, mlen, best_mlen, price, offset, best_off, match_num, last_pos;
    
    rep = 1;
    

    /* init */
    ctx->end += inputSize;
    ip++;

    /* Main Loop */
    while (ip < mflimit)
    {
        memset(opt, 0, sizeof(LZ5HC_optimal_t));
        last_pos = 0;
        llen = ip - anchor;
        inr = ip;


        // check rep
        mlen = MEM_count(ip, ip - rep, matchlimit);
        if (mlen >= MINMATCH)
        {
            LZ5_LOG_PARSER("%d: start try REP rep=%d mlen=%d\n", (int)((char*)ip-source), rep, mlen);
            if (mlen > sufficient_len || mlen >= LZ5_OPT_NUM)
            {
                best_mlen = mlen; best_off = 0; cur = 0; last_pos = 1;
                goto encode;
            }

            do
            {
                litlen = 0;
                price = LZ5HC_get_price(llen, 0, mlen - MINMATCH) - llen;
                if (mlen > last_pos || price < opt[mlen].price)
                    SET_PRICE(mlen, mlen, 0, litlen, price);
                mlen--;
            }
            while (mlen >= MINMATCH);
        }

 
       best_mlen = (last_pos) ? last_pos : MINMATCH;

       if (faster_get_matches && last_pos)
           match_num = 0;
       else
       {
            if (ctx->params.strategy == LZ5HC_optimal_price)
            {
                LZ5HC_Insert(ctx, ip);
                match_num = LZ5HC_GetAllMatches(ctx, ip, ip, matchlimit, best_mlen, matches);
            }
            else
            {
                if (ctx->params.fullSearch < 2)
                    LZ5HC_BinTree_Insert(ctx, ip);
                else
                    LZ5HC_BinTree_InsertFull(ctx, ip, matchlimit);
                match_num = LZ5HC_BinTree_GetAllMatches(ctx, ip, matchlimit, best_mlen, matches);
            }
       }

       LZ5_LOG_PARSER("%d: match_num=%d last_pos=%d\n", (int)((char*)ip-source), match_num, last_pos);
       if (!last_pos && !match_num) { ip++; continue; }

       if (match_num && matches[match_num-1].len > sufficient_len)
       {
            best_mlen = matches[match_num-1].len;
            best_off = matches[match_num-1].off;
            cur = 0;
            last_pos = 1;
            goto encode;
       }

       // set prices using matches at position = 0
       for (int i = 0; i < match_num; i++)
       {
           mlen = (i>0) ? matches[i-1].len+1 : best_mlen;
           best_mlen = (matches[i].len < LZ5_OPT_NUM) ? matches[i].len : LZ5_OPT_NUM;
           LZ5_LOG_PARSER("%d: start Found mlen=%d off=%d best_mlen=%d last_pos=%d\n", (int)((char*)ip-source), matches[i].len, matches[i].off, best_mlen, last_pos);
           while (mlen <= best_mlen)
           {
                litlen = 0;
                price = LZ5HC_get_price(llen + litlen, matches[i].off, mlen - MINMATCH) - llen;
                if (mlen > last_pos || price < opt[mlen].price)
                    SET_PRICE(mlen, mlen, matches[i].off, litlen, price);
                mlen++;
           }
        }

        if (!last_pos) { ip++; continue; }

        opt[0].rep = rep;
        opt[0].mlen = 1;

        // check further positions
        for (skip_num = 0, cur = 1; cur <= last_pos; cur++)
        { 
           inr = ip + cur;

           if (opt[cur-1].mlen == 1)
           {
                litlen = opt[cur-1].litlen + 1;
                
                if (cur != litlen)
                {
                    price = opt[cur - litlen].price + LZ5_LIT_ONLY_COST(litlen);
                    LZ5_LOG_PRICE("%d: TRY1 opt[%d].price=%d price=%d cur=%d litlen=%d\n", (int)((char*)inr-source), cur - litlen, opt[cur - litlen].price, price, cur, litlen);
                }
                else
                {
                    price = LZ5_LIT_ONLY_COST(llen + litlen) - llen;
                    LZ5_LOG_PRICE("%d: TRY2 price=%d cur=%d litlen=%d llen=%d\n", (int)((char*)inr-source), price, cur, litlen, llen);
                }
           }
           else
           {
                litlen = 1;
                price = opt[cur - 1].price + LZ5_LIT_ONLY_COST(litlen);                  
                LZ5_LOG_PRICE("%d: TRY3 price=%d cur=%d litlen=%d litonly=%d\n", (int)((char*)inr-source), price, cur, litlen, LZ5_LIT_ONLY_COST(litlen));
           }
           
           mlen = 1;
           best_mlen = 0;
           LZ5_LOG_PARSER("%d: TRY price=%d opt[%d].price=%d\n", (int)((char*)inr-source), price, cur, opt[cur].price);

           if (cur > last_pos || price <= opt[cur].price) // || ((price == opt[cur].price) && (opt[cur-1].mlen == 1) && (cur != litlen)))
                SET_PRICE(cur, mlen, best_mlen, litlen, price);

           if (cur == last_pos) break;

           if (opt[cur].mlen > 1)
           {
                mlen = opt[cur].mlen;
                offset = opt[cur].off;
                if (offset < 1)
                {
                    opt[cur].rep = opt[cur-mlen].rep;
                    LZ5_LOG_PARSER("%d: COPYREP1 cur=%d mlen=%d rep=%d\n", (int)((char*)inr-source), cur, mlen, opt[cur-mlen].rep);
                }
                else
                {
                    opt[cur].rep = offset;
                    LZ5_LOG_PARSER("%d: COPYREP2 cur=%d offset=%d rep=%d\n", (int)((char*)inr-source), cur, offset, opt[cur].rep);
                }
           }
           else
           {
                opt[cur].rep = opt[cur-1].rep; // copy rep
           }


            LZ5_LOG_PARSER("%d: CURRENT price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d\n", (int)((char*)inr-source), cur, last_pos, opt[cur].price, opt[cur].off, opt[cur].mlen, opt[cur].litlen, opt[cur].rep); 

           // check rep
           // best_mlen = 0;
           mlen = MEM_count(inr, inr - opt[cur].rep, matchlimit);
           if (mlen >= MINMATCH && mlen > best_mlen)
           {
              LZ5_LOG_PARSER("%d: try REP rep=%d mlen=%d\n", (int)((char*)inr-source), opt[cur].rep, mlen);   
              LZ5_LOG_PARSER("%d: Found REP mlen=%d off=%d rep=%d opt[%d].off=%d\n", (int)((char*)inr-source), mlen, 0, opt[cur].rep, cur, opt[cur].off);

              if (mlen > sufficient_len || cur + mlen >= LZ5_OPT_NUM)
              {
                best_mlen = mlen;
                best_off = 0;
                LZ5_LOG_PARSER("%d: REP sufficient_len=%d best_mlen=%d best_off=%d last_pos=%d\n", (int)((char*)inr-source), sufficient_len, best_mlen, best_off, last_pos);
                last_pos = cur + 1;
                goto encode;
               }

               if (opt[cur].mlen == 1)
               {
                    litlen = opt[cur].litlen;

                    if (cur != litlen)
                    {
                        price = opt[cur - litlen].price + LZ5HC_get_price(litlen, 0, mlen - MINMATCH);
                        LZ5_LOG_PRICE("%d: TRY1 opt[%d].price=%d price=%d cur=%d litlen=%d\n", (int)((char*)inr-source), cur - litlen, opt[cur - litlen].price, price, cur, litlen);
                    }
                    else
                    {
                        price = LZ5HC_get_price(llen + litlen, 0, mlen - MINMATCH) - llen;
                        LZ5_LOG_PRICE("%d: TRY2 price=%d cur=%d litlen=%d llen=%d\n", (int)((char*)inr-source), price, cur, litlen, llen);
                    }
                }
                else
                {
                    litlen = 0;
                    price = opt[cur].price + LZ5HC_get_price(litlen, 0, mlen - MINMATCH);
                    LZ5_LOG_PRICE("%d: TRY3 price=%d cur=%d litlen=%d getprice=%d\n", (int)((char*)inr-source), price, cur, litlen, LZ5HC_get_price(litlen, 0, mlen - MINMATCH));
                }

                best_mlen = mlen;
                if (faster_get_matches)
                    skip_num = best_mlen;

                LZ5_LOG_PARSER("%d: Found REP mlen=%d off=%d price=%d litlen=%d price[%d]=%d\n", (int)((char*)inr-source), mlen, 0, price, litlen, cur - litlen, opt[cur - litlen].price);

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

            if (ctx->params.strategy == LZ5HC_optimal_price)
            {
                LZ5HC_Insert(ctx, inr);
                match_num = LZ5HC_GetAllMatches(ctx, inr, ip, matchlimit, best_mlen, matches);
                LZ5_LOG_PARSER("%d: LZ5HC_GetAllMatches match_num=%d\n", (int)((char*)inr-source), match_num);
            }
            else
            {
                if (ctx->params.fullSearch < 2)
                    LZ5HC_BinTree_Insert(ctx, inr);
                else
                    LZ5HC_BinTree_InsertFull(ctx, inr, matchlimit);
                match_num = LZ5HC_BinTree_GetAllMatches(ctx, inr, matchlimit, best_mlen, matches);
                LZ5_LOG_PARSER("%d: LZ5HC_BinTree_GetAllMatches match_num=%d\n", (int)((char*)inr-source), match_num);
            }


            if (match_num > 0 && matches[match_num-1].len > sufficient_len)
            {
                cur -= matches[match_num-1].back;
                best_mlen = matches[match_num-1].len;
                best_off = matches[match_num-1].off;
                last_pos = cur + 1;
                goto encode;
            }

            cur_min = cur;

            // set prices using matches at position = cur
            for (int i = 0; i < match_num; i++)
            {
                mlen = (i>0) ? matches[i-1].len+1 : best_mlen;
                cur2 = cur - matches[i].back;
                best_mlen = (cur2 + matches[i].len < LZ5_OPT_NUM) ? matches[i].len : LZ5_OPT_NUM - cur2;
                LZ5_LOG_PARSER("%d: Found1 cur=%d cur2=%d mlen=%d off=%d best_mlen=%d last_pos=%d\n", (int)((char*)inr-source), cur, cur2, matches[i].len, matches[i].off, best_mlen, last_pos);

                while (mlen <= best_mlen)
                {
                    if (opt[cur2].mlen == 1)
                    {
                        litlen = opt[cur2].litlen;

                        if (cur2 != litlen)
                            price = opt[cur2 - litlen].price + LZ5HC_get_price(litlen, matches[i].off, mlen - MINMATCH);
                        else
                            price = LZ5HC_get_price(llen + litlen, matches[i].off, mlen - MINMATCH) - llen;
                    }
                    else
                    {
                        litlen = 0;
                        price = opt[cur2].price + LZ5HC_get_price(litlen, matches[i].off, mlen - MINMATCH);
                    }

                    LZ5_LOG_PARSER("%d: Found2 pred=%d mlen=%d best_mlen=%d off=%d price=%d litlen=%d price[%d]=%d\n", (int)((char*)inr-source), matches[i].back, mlen, best_mlen, matches[i].off, price, litlen, cur - litlen, opt[cur - litlen].price);
                    if (cur2 + mlen > last_pos || price < opt[cur2 + mlen].price)
                    {
                        SET_PRICE(cur2 + mlen, mlen, matches[i].off, litlen, price);

                        opt[cur2 + mlen].rep = matches[i].off; // update reps
                        if (cur2 < cur_min) cur_min = cur2;
                    }

                    mlen++;
                }
            }
            
            if (cur_min < cur)
            {
                for (int i=cur_min-1; i<=last_pos; i++)
                {
                    LZ5_LOG_PARSER("%d: BEFORE price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d\n", (int)((char*)ip-source+i), i, last_pos, opt[i].price, opt[i].off, opt[i].mlen, opt[i].litlen, opt[i].rep); 
                }

                for (int i=cur_min+1; i<=last_pos; i++)
                if (opt[i].price < (1<<30) && (opt[i].off) < 1 && i - opt[i].mlen > cur_min) // invalidate reps
                {
                   if (opt[i-1].mlen == 1)
                   {
                        litlen = opt[i-1].litlen + 1;
                        
                        if (i != litlen)
                        {
                            price = opt[i - litlen].price + LZ5_LIT_ONLY_COST(litlen);
                        //	LZ5_LOG_PRICE("%d: TRY1 opt[%d].price=%d price=%d cur=%d litlen=%d\n", (int)(inr-base), i - litlen, opt[i - litlen].price, price, i, litlen);
                        }
                        else
                        {
                            price = LZ5_LIT_ONLY_COST(llen + litlen) - llen;
                        //	LZ5_LOG_PRICE("%d: TRY2 price=%d cur=%d litlen=%d llen=%d\n", (int)(inr-base), price, i, litlen, llen);
                        }
                    }
                    else
                    {
                        litlen = 1;
                        price = opt[i - 1].price + LZ5_LIT_ONLY_COST(litlen);                  
                    //	LZ5_LOG_PRICE("%d: TRY3 price=%d cur=%d litlen=%d\n", (int)(inr-base), price, i, litlen);
                    }

                    mlen = 1;
                    best_mlen = 0;
                    SET_PRICE(i, mlen, best_mlen, litlen, price);

                    opt[i].rep = opt[i-1].rep; // copy reps

                    LZ5_LOG_PARSER("%d: INVALIDATE pred=%d price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d\n", (int)((char*)inr-source), cur-cur_min, i, last_pos, opt[i].price, opt[i].off, opt[i].mlen, opt[i].litlen, opt[i].rep);
                }
                
                for (int i=cur_min-1; i<=last_pos; i++)
                {
                    LZ5_LOG_PARSER("%d: AFTER price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d\n", (int)((char*)ip-source+i), i, last_pos, opt[i].price, opt[i].off, opt[i].mlen, opt[i].litlen, opt[i].rep); 
                }
                
            }
        } //  for (skip_num = 0, cur = 1; cur <= last_pos; cur++)


        best_mlen = opt[last_pos].mlen;
        best_off = opt[last_pos].off;
        cur = last_pos - best_mlen;

encode: // cur, last_pos, best_mlen, best_off have to be set
        for (int i = 1; i <= last_pos; i++)
        {
            LZ5_LOG_PARSER("%d: price[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d\n", (int)((char*)ip-source+i), i, last_pos, opt[i].price, opt[i].off, opt[i].mlen, opt[i].litlen, opt[i].rep); 
        }

        LZ5_LOG_PARSER("%d: cur=%d/%d best_mlen=%d best_off=%d rep=%d\n", (int)((char*)ip-source+cur), cur, last_pos, best_mlen, best_off, opt[cur].rep); 

        opt[0].mlen = 1;
        
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
            LZ5_LOG_PARSER("%d: price2[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d\n", (int)((char*)ip-source+i), i, last_pos, opt[i].price, opt[i].off, opt[i].mlen, opt[i].litlen, opt[i].rep); 
            i += opt[i].mlen;
        }

        cur = 0;

        while (cur < last_pos)
        {
            LZ5_LOG_PARSER("%d: price3[%d/%d]=%d off=%d mlen=%d litlen=%d rep=%d\n", (int)((char*)ip-source+cur), cur, last_pos, opt[cur].price, opt[cur].off, opt[cur].mlen, opt[cur].litlen, opt[cur].rep); 
            mlen = opt[cur].mlen;
            if (mlen == 1) { ip++; cur++; continue; }
            offset = opt[cur].off;
            cur += mlen;

            LZ5_LOG_ENCODE("%d: ENCODE literals=%d off=%d mlen=%d ", (int)((char*)ip-source), (int)(ip-anchor), (int)(offset), mlen);
            if (offset == 0)
            {
                res = LZ5HC_encodeSequence(ctx, &ip, &op, &anchor, mlen, ip - rep, limit, oend);
            }
            else
            {
                res = LZ5HC_encodeSequence(ctx, &ip, &op, &anchor, mlen, ip - offset, limit, oend);
                rep = offset;
            }
            LZ5_LOG_ENCODE("out=%d\n", (int)((char*)op - dest));

            if (res) return 0; 

            LZ5_LOG_PARSER("%d: offset=%d rep=%d\n", (int)((char*)ip-source), offset, rep);
        }
    }

    /* Encode Last Literals */
    {
        int lastRun = (int)(iend - anchor);
    //    if (inputSize > LASTLITERALS && lastRun < LASTLITERALS) { printf("ERROR: lastRun=%d\n", lastRun); }
        if ((limit) && (((char*)op - dest) + lastRun + 1 + ((lastRun+255-RUN_MASK)/255) > (U32)maxOutputSize)) return 0;  /* Check output limit */
        if (lastRun>=(int)RUN_MASK) { *op++=(RUN_MASK<<ML_BITS); lastRun-=RUN_MASK; for(; lastRun > 254 ; lastRun-=255) *op++ = 255; *op++ = (BYTE) lastRun; }
        else *op++ = (BYTE)(lastRun<<ML_BITS);
        LZ5_LOG_ENCODE("%d: ENCODE_LAST literals=%d out=%d\n", (int)((char*)ip-source), (int)(iend-anchor), (int)((char*)op -dest));
        memcpy(op, anchor, iend - anchor);
        op += iend-anchor;
    }

    /* End */
    return (int) ((char*)op-dest);
}

#endif