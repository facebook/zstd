/*
    zstd - standard compression library
    Header File for static linking only
    Copyright (C) 2014-2016, Yann Collet.

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
    - zstd homepage : http://www.zstd.net
*/
#ifndef ZSTD_STATS_H
#define ZSTD_STATS_H


#if defined (__cplusplus)
extern "C" {
#endif

/*-*************************************
*  Dependencies
***************************************/
//#include "zstd.h"
//#include "mem.h"


/*-*************************************
*  Constants
***************************************/
//#define ZSTD_MAGICNUMBER 0xFD2FB526   /* v0.6 */


/*-*************************************
*  Types
***************************************/
typedef struct {
    U32  priceOffset, priceOffCode, priceMatchLength, priceLiteral, priceLitLength, priceDumpsLength;
    U32  totalMatchSum, totalLitSum, totalSeqSum, totalRepSum;
    U32  litSum, matchLengthSum, litLengthSum, offCodeSum;
    U32  matchLengthFreq[1<<MLbits];
    U32  litLengthFreq[1<<LLbits];
    U32  litFreq[1<<Litbits];
    U32  offCodeFreq[1<<Offbits];
} ZSTD_stats_t;


/*-*************************************
*  Advanced functions
***************************************/
MEM_STATIC void ZSTD_statsPrint(ZSTD_stats_t* stats, U32 searchLength)
{
    stats->totalMatchSum += stats->totalSeqSum * ((searchLength == 3) ? 3 : 4);
    printf("avgMatchL=%.2f avgLitL=%.2f match=%.1f%% lit=%.1f%% reps=%d seq=%d\n", (float)stats->totalMatchSum/stats->totalSeqSum, (float)stats->totalLitSum/stats->totalSeqSum, 100.0*stats->totalMatchSum/(stats->totalMatchSum+stats->totalLitSum), 100.0*stats->totalLitSum/(stats->totalMatchSum+stats->totalLitSum), stats->totalRepSum, stats->totalSeqSum);
    printf("SumBytes=%d Offset=%d OffCode=%d Match=%d Literal=%d LitLength=%d DumpsLength=%d\n", (stats->priceOffset+stats->priceOffCode+stats->priceMatchLength+stats->priceLiteral+stats->priceLitLength+stats->priceDumpsLength)/8, stats->priceOffset/8, stats->priceOffCode/8, stats->priceMatchLength/8, stats->priceLiteral/8, stats->priceLitLength/8, stats->priceDumpsLength/8);
}

MEM_STATIC void ZSTD_statsInit(ZSTD_stats_t* stats)
{
    stats->totalLitSum = stats->totalMatchSum = stats->totalSeqSum = stats->totalRepSum = 1;
    stats->priceOffset = stats->priceOffCode = stats->priceMatchLength = stats->priceLiteral = stats->priceLitLength = stats->priceDumpsLength = 0;
}

MEM_STATIC void ZSTD_statsResetFreqs(ZSTD_stats_t* stats)
{
    unsigned u;

    stats->litSum = (1<<Litbits);
    stats->litLengthSum = (1<<LLbits);
    stats->matchLengthSum = (1<<MLbits);
    stats->offCodeSum = (1<<Offbits);

    for (u=0; u<=MaxLit; u++)
        stats->litFreq[u] = 1;
    for (u=0; u<=MaxLL; u++)
        stats->litLengthFreq[u] = 1;
    for (u=0; u<=MaxML; u++)
        stats->matchLengthFreq[u] = 1;
    for (u=0; u<=MaxOff; u++)
        stats->offCodeFreq[u] = 1;
}

MEM_STATIC void ZSTD_statsUpdatePrices(ZSTD_stats_t* stats, size_t litLength, const BYTE* literals, size_t offset, size_t matchLength)
{
    /* offset */
    BYTE offCode = offset ? (BYTE)ZSTD_highbit(offset+1) + 1 : 0;
    stats->priceOffCode += ZSTD_highbit(stats->offCodeSum+1) - ZSTD_highbit(stats->offCodeFreq[offCode]+1);
    stats->priceOffset += (offCode-1) + (!offCode);

    /* match Length */
    stats->priceDumpsLength += ((matchLength >= MaxML)<<3) + ((matchLength >= 255+MaxML)<<4) + ((matchLength>=(1<<15))<<3);
    stats->priceMatchLength += ZSTD_highbit(stats->matchLengthSum+1) - ZSTD_highbit(stats->matchLengthFreq[(matchLength >= MaxML) ? MaxML : matchLength]+1);

    if (litLength) {
        /* literals */
        U32 u;
        stats->priceLiteral += litLength * ZSTD_highbit(stats->litSum+1);
        for (u=0; u < litLength; u++)
            stats->priceLiteral -= ZSTD_highbit(stats->litFreq[literals[u]]+1);

        /* literal Length */
        stats->priceDumpsLength += ((litLength >= MaxLL)<<3) + ((litLength >= 255+MaxLL)<<4) + ((litLength>=(1<<15))<<3);
        stats->priceLitLength += ZSTD_highbit(stats->litLengthSum+1) - ZSTD_highbit(stats->litLengthFreq[(litLength >= MaxLL) ? MaxLL : litLength]+1);
    } else {
        stats->priceLitLength += ZSTD_highbit(stats->litLengthSum+1) - ZSTD_highbit(stats->litLengthFreq[0]+1);
    }


    if (offset == 0) stats->totalRepSum++;
    stats->totalSeqSum++;
    stats->totalMatchSum += matchLength;
    stats->totalLitSum += litLength;
        
    U32 u;
    /* literals */
    stats->litSum += litLength;
    for (u=0; u < litLength; u++)
        stats->litFreq[literals[u]]++;

    /* literal Length */
    stats->litLengthSum++;
    if (litLength >= MaxLL)
        stats->litLengthFreq[MaxLL]++;
    else
        stats->litLengthFreq[litLength]++;

    /* match offset */
    stats->offCodeSum++;
    stats->offCodeFreq[offCode]++;

    /* match Length */
    stats->matchLengthSum++;
    if (matchLength >= MaxML)
        stats->matchLengthFreq[MaxML]++;
    else
        stats->matchLengthFreq[matchLength]++;
}



#if defined (__cplusplus)
}
#endif

#endif  /* ZSTD_STATIC_H */
