/*
    zstd_opt_internal - common optimal parser functions to include
    Header File for include
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
    - zstd source repository : https://github.com/Cyan4973/zstd
*/

/* Note : this file is intended to be included within zstd_compress.c */

#ifndef ZSTD_OPT_INTERNAL_H_MODULE
#define ZSTD_OPT_INTERNAL_H_MODULE


/*-*******************************************
*  The optimal parser
*********************************************/
/*-  Constants  -*/
#define ZSTD_OPT_NUM    (1<<12)
#define ZSTD_FREQ_START 1
#define ZSTD_FREQ_STEP  1
#define ZSTD_FREQ_DIV   4

/*-  Debug  -*/
#if defined(ZSTD_OPT_DEBUG) && ZSTD_OPT_DEBUG>=9
    #define ZSTD_LOG_PARSER(...) printf(__VA_ARGS__)
    #define ZSTD_LOG_ENCODE(...) printf(__VA_ARGS__)
    #define ZSTD_LOG_TRY_PRICE(...) printf(__VA_ARGS__)
#else
    #define ZSTD_LOG_PARSER(...)
    #define ZSTD_LOG_ENCODE(...)
    #define ZSTD_LOG_TRY_PRICE(...)
#endif


typedef struct {
    U32 off;
    U32 len;
} ZSTD_match_t;

typedef struct {
    U32 price;
    U32 off;
    U32 mlen;
    U32 litlen;
    U32 rep;
    U32 rep2;
} ZSTD_optimal_t;


MEM_STATIC void ZSTD_rescaleFreqs(seqStore_t* ssPtr)
{
    unsigned u;

    if (ssPtr->litLengthSum == 0) {
        ssPtr->matchLengthSum = (1<<MLbits);
        ssPtr->litLengthSum = (1<<LLbits);
        ssPtr->litSum = (1<<Litbits);
        ssPtr->offCodeSum = (1<<Offbits);
        ssPtr->matchSum = 0;
        
        for (u=0; u<=MaxLit; u++)
            ssPtr->litFreq[u] = 1;
        for (u=0; u<=MaxLL; u++)
            ssPtr->litLengthFreq[u] = 1;
        for (u=0; u<=MaxML; u++)
            ssPtr->matchLengthFreq[u] = 1;
        for (u=0; u<=MaxOff; u++)
            ssPtr->offCodeFreq[u] = 1;
    } else {
        ssPtr->matchLengthSum = 0;
        ssPtr->litLengthSum = 0;
        ssPtr->litSum = 0;
        ssPtr->offCodeSum = 0;
        ssPtr->matchSum = 0;

        for (u=0; u<=MaxLit; u++) {
            ssPtr->litFreq[u] = ZSTD_FREQ_START + (ssPtr->litFreq[u]>>ZSTD_FREQ_DIV);
            ssPtr->litSum += ssPtr->litFreq[u];
        }
        for (u=0; u<=MaxLL; u++) {
            ssPtr->litLengthFreq[u] = ZSTD_FREQ_START + (ssPtr->litLengthFreq[u]>>ZSTD_FREQ_DIV);
            ssPtr->litLengthSum += ssPtr->litLengthFreq[u];
        }
        for (u=0; u<=MaxML; u++) {
            ssPtr->matchLengthFreq[u] = ZSTD_FREQ_START + (ssPtr->matchLengthFreq[u]>>ZSTD_FREQ_DIV);
            ssPtr->matchLengthSum += ssPtr->matchLengthFreq[u];
            ssPtr->matchSum += ssPtr->matchLengthFreq[u] * (u + 3);
        }
        for (u=0; u<=MaxOff; u++) {
            ssPtr->offCodeFreq[u] = ZSTD_FREQ_START + (ssPtr->offCodeFreq[u]>>ZSTD_FREQ_DIV);
            ssPtr->offCodeSum += ssPtr->offCodeFreq[u];
        }
    }
}

MEM_STATIC void ZSTD_updatePrice(seqStore_t* seqStorePtr, U32 litLength, const BYTE* literals, U32 offset, U32 matchLength)
{
    U32 u;

    /* literals */
    seqStorePtr->litSum += litLength * ZSTD_FREQ_STEP;
    for (u=0; u < litLength; u++)
        seqStorePtr->litFreq[literals[u]] += ZSTD_FREQ_STEP;

    /* literal Length */
    seqStorePtr->litLengthSum += ZSTD_FREQ_STEP;
    if (litLength >= MaxLL)
        seqStorePtr->litLengthFreq[MaxLL] += ZSTD_FREQ_STEP;
    else
        seqStorePtr->litLengthFreq[litLength] += ZSTD_FREQ_STEP;

    /* match offset */
    seqStorePtr->offCodeSum += ZSTD_FREQ_STEP;
    BYTE offCode = offset ? (BYTE)ZSTD_highbit(offset) + 1 : 0;
    seqStorePtr->offCodeFreq[offCode] += ZSTD_FREQ_STEP;

    /* match Length */
    seqStorePtr->matchLengthSum += ZSTD_FREQ_STEP;
    if (matchLength >= MaxML)
        seqStorePtr->matchLengthFreq[MaxML] += ZSTD_FREQ_STEP;
    else
        seqStorePtr->matchLengthFreq[matchLength] += ZSTD_FREQ_STEP;
}

FORCE_INLINE U32 ZSTD_getLiteralPrice(seqStore_t* seqStorePtr, U32 litLength, const BYTE* literals)
{
    U32 price, u;

    if (litLength == 0)
        return ZSTD_highbit(seqStorePtr->litLengthSum) - ZSTD_highbit(seqStorePtr->litLengthFreq[0]);

    /* literals */
    price = litLength * ZSTD_highbit(seqStorePtr->litSum);
    for (u=0; u < litLength; u++)
        price -= ZSTD_highbit(seqStorePtr->litFreq[literals[u]]);

    /* literal Length */
    price += ((litLength >= MaxLL)<<3) + ((litLength >= 255+MaxLL)<<4) + ((litLength>=(1<<15))<<3);
    if (litLength >= MaxLL) litLength = MaxLL;
    price += ZSTD_highbit(seqStorePtr->litLengthSum) - ZSTD_highbit(seqStorePtr->litLengthFreq[litLength]);

    return price;
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

/* Update hashTable3 up to ip (excluded)
   Assumption : always within prefix (ie. not within extDict) */
static U32 ZSTD_insertAndFindFirstIndexHash3 (ZSTD_CCtx* zc, const BYTE* ip)
{
    U32* const hashTable3  = zc->hashTable3;
    const U32 hashLog3 = zc->params.hashLog3;
    const BYTE* const base = zc->base;
    const U32 target = (U32)(ip - base);
    U32 idx = zc->nextToUpdate3;

    while(idx < target) {
        hashTable3[ZSTD_hash3Ptr(base+idx, hashLog3)] = idx;
        idx++;
    }

    zc->nextToUpdate3 = target;
    return hashTable3[ZSTD_hash3Ptr(ip, hashLog3)];
}


#define MINMATCHOPT 4
#define MEM_readMINMATCH(ptr) (U32)(MEM_read32(ptr)) 
#define ZSTD_GETPRICE ZSTD_getPrice4
#define ZSTD_INSERTBTANDGETALLMATCHES ZSTD_insertBtAndGetAllMatches4
#define ZSTD_BTGETALLMATCHES ZSTD_BtGetAllMatches4
#define ZSTD_BTGETALLMATCHES_SELECTMLS ZSTD_BtGetAllMatches_selectMLS4
#define ZSTD_BTGETALLMATCHES_EXTDICT ZSTD_BtGetAllMatches_extDict4
#define ZSTD_BTGETALLMATCHES_SELECTMLS_EXTDICT ZSTD_BtGetAllMatches_selectMLS_extDict4
#define ZSTD_COMPRESSBLOCK_OPT_GENERIC ZSTD_compressBlock_opt_generic4
#define ZSTD_COMPRESSBLOCK_OPT_EXTDICT_GENERIC ZSTD_compressBlock_opt_extDict_generic4
#include "zstd_opt.h"
#undef MINMATCHOPT
#undef MEM_readMINMATCH
#undef ZSTD_GETPRICE
#undef ZSTD_INSERTBTANDGETALLMATCHES
#undef ZSTD_BTGETALLMATCHES
#undef ZSTD_BTGETALLMATCHES_SELECTMLS
#undef ZSTD_BTGETALLMATCHES_EXTDICT
#undef ZSTD_BTGETALLMATCHES_SELECTMLS_EXTDICT
#undef ZSTD_COMPRESSBLOCK_OPT_GENERIC
#undef ZSTD_COMPRESSBLOCK_OPT_EXTDICT_GENERIC

#define MINMATCHOPT 3
#define MEM_readMINMATCH(ptr) ((U32)(MEM_read32(ptr)<<8)) 
#define ZSTD_GETPRICE ZSTD_getPrice3
#define ZSTD_INSERTBTANDGETALLMATCHES ZSTD_insertBtAndGetAllMatches3
#define ZSTD_BTGETALLMATCHES ZSTD_BtGetAllMatches3
#define ZSTD_BTGETALLMATCHES_SELECTMLS ZSTD_BtGetAllMatches_selectMLS3
#define ZSTD_BTGETALLMATCHES_EXTDICT ZSTD_BtGetAllMatches_extDict3
#define ZSTD_BTGETALLMATCHES_SELECTMLS_EXTDICT ZSTD_BtGetAllMatches_selectMLS_extDict3
#define ZSTD_COMPRESSBLOCK_OPT_GENERIC ZSTD_compressBlock_opt_generic3
#define ZSTD_COMPRESSBLOCK_OPT_EXTDICT_GENERIC ZSTD_compressBlock_opt_extDict_generic3
#include "zstd_opt.h"


#endif   /* ZSTD_OPT_INTERNAL_H_MODULE */
