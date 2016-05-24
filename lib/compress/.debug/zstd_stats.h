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
*  Types
***************************************/
struct ZSTD_stats_s {
    U32  priceOffset, priceOffCode, priceMatchLength, priceLiteral, priceLitLength;
    U32  totalMatchSum, totalLitSum, totalSeqSum, totalRepSum;
    U32  litSum, matchLengthSum, litLengthSum, offCodeSum;
    U32  matchLengthFreq[MaxML+1];
    U32  litLengthFreq[MaxLL+1];
    U32  litFreq[1<<Litbits];
    U32  offCodeFreq[MaxOff+1];
};


/*-*************************************
*  Stats functions
***************************************/
MEM_STATIC ZSTD_stats_t* ZSTD_statsAlloc() { return malloc(sizeof(ZSTD_stats_t)); }
MEM_STATIC void ZSTD_statsFree(struct ZSTD_stats_s* stats) { free(stats); }

MEM_STATIC void ZSTD_statsPrint(ZSTD_stats_t* stats, U32 searchLength)
{
    stats->totalMatchSum += stats->totalSeqSum * ((searchLength == 3) ? 3 : 4);
    printf("\navgMatchL=%.2f avgLitL=%.2f match=%.1f%% lit=%.1f%% reps=%d seq=%d\n", (float)stats->totalMatchSum/stats->totalSeqSum, (float)stats->totalLitSum/stats->totalSeqSum, 100.0*stats->totalMatchSum/(stats->totalMatchSum+stats->totalLitSum), 100.0*stats->totalLitSum/(stats->totalMatchSum+stats->totalLitSum), stats->totalRepSum, stats->totalSeqSum);
    printf("SumBytes=%d Offset=%d OffCode=%d Match=%d Literal=%d LitLength=%d\n", (stats->priceOffset+stats->priceOffCode+stats->priceMatchLength+stats->priceLiteral+stats->priceLitLength)/8, stats->priceOffset/8, stats->priceOffCode/8, stats->priceMatchLength/8, stats->priceLiteral/8, stats->priceLitLength/8);
}


MEM_STATIC void ZSTD_statsInit(ZSTD_stats_t* stats)
{
    stats->totalLitSum = stats->totalMatchSum = stats->totalSeqSum = stats->totalRepSum = 1;
    stats->priceOffset = stats->priceOffCode = stats->priceMatchLength = stats->priceLiteral = stats->priceLitLength = 0;
}


MEM_STATIC void ZSTD_statsResetFreqs(ZSTD_stats_t* stats)
{
    unsigned u;

    stats->litSum = (2<<Litbits);
    stats->litLengthSum = MaxLL+1;
    stats->matchLengthSum = MaxML+1;
    stats->offCodeSum = (MaxOff+1);

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
    U32 u;
    /* literals */
    stats->priceLiteral += litLength * ZSTD_highbit(stats->litSum+1);
    for (u=0; u < litLength; u++)
        stats->priceLiteral -= ZSTD_highbit(stats->litFreq[literals[u]]+1);
    stats->litSum += litLength;
    for (u=0; u < litLength; u++)
        stats->litFreq[literals[u]]++;

    /* literal Length */
    {   static const BYTE LL_Code[64] = {  0,  1,  2,  3,  4,  5,  6,  7,
                                           8,  9, 10, 11, 12, 13, 14, 15,
                                          16, 16, 17, 17, 18, 18, 19, 19,
                                          20, 20, 20, 20, 21, 21, 21, 21,
                                          22, 22, 22, 22, 22, 22, 22, 22,
                                          23, 23, 23, 23, 23, 23, 23, 23,
                                          24, 24, 24, 24, 24, 24, 24, 24,
                                          24, 24, 24, 24, 24, 24, 24, 24 };
        const BYTE LL_deltaCode = 19;
        const BYTE llCode = (litLength>63) ? (BYTE)ZSTD_highbit(litLength) + LL_deltaCode : LL_Code[litLength];
        if (litLength) {
            stats->priceLitLength += LL_bits[llCode] + ZSTD_highbit(stats->litLengthSum+1) - ZSTD_highbit(stats->litLengthFreq[llCode]+1);
        } else {
            stats->priceLitLength += ZSTD_highbit(stats->litLengthSum+1) - ZSTD_highbit(stats->litLengthFreq[0]+1);
        }
        stats->litLengthFreq[llCode]++;
        stats->litLengthSum++;
    }

    /* match offset */
    {   BYTE offCode = (BYTE)ZSTD_highbit(offset+1);
        stats->priceOffCode += ZSTD_highbit(stats->offCodeSum+1) - ZSTD_highbit(stats->offCodeFreq[offCode]+1);
        stats->priceOffset += offCode;
        stats->offCodeSum++;
        stats->offCodeFreq[offCode]++;
    }

    /* match Length */
    {   static const BYTE ML_Code[128] = { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
                                          16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
                                          32, 32, 33, 33, 34, 34, 35, 35, 36, 36, 36, 36, 37, 37, 37, 37,
                                          38, 38, 38, 38, 38, 38, 38, 38, 39, 39, 39, 39, 39, 39, 39, 39,
                                          40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
                                          41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
                                          42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42,
                                          42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42 };
        const BYTE ML_deltaCode = 36;
        const BYTE mlCode = (matchLength>127) ? (BYTE)ZSTD_highbit(matchLength) + ML_deltaCode : ML_Code[matchLength];
        stats->priceMatchLength += ML_bits[mlCode] + ZSTD_highbit(stats->matchLengthSum+1) - ZSTD_highbit(stats->matchLengthFreq[mlCode]+1);
        stats->matchLengthFreq[mlCode]++;
        stats->matchLengthSum++;
    }

    if (offset == 0) stats->totalRepSum++;
    stats->totalSeqSum++;
    stats->totalMatchSum += matchLength;
    stats->totalLitSum += litLength;
}


#if defined (__cplusplus)
}
#endif

#endif  /* ZSTD_STATIC_H */
