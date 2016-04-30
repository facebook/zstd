/*
    dictBuilder - dictionary builder for zstd
    Copyright (C) Yann Collet 2016

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
    - Zstd homepage : https://www.zstd.net
*/

/*-**************************************
*  Compiler Options
****************************************/
/* Disable some Visual warning messages */
#ifdef _MSC_VER
#  pragma warning(disable : 4127)                /* disable: C4127: conditional expression is constant */
#endif

/* Unix Large Files support (>4GB) */
#define _FILE_OFFSET_BITS 64
#if (defined(__sun__) && (!defined(__LP64__)))   /* Sun Solaris 32-bits requires specific definitions */
#  define _LARGEFILE_SOURCE
#elif ! defined(__LP64__)                        /* No point defining Large file for 64 bit */
#  define _LARGEFILE64_SOURCE
#endif


/*-*************************************
*  Dependencies
***************************************/
#include <stdlib.h>        /* malloc, free */
#include <string.h>        /* memset */
#include <stdio.h>         /* fprintf, fopen, ftello64 */
#include <time.h>          /* clock */

#include "mem.h"           /* read */
#include "error_private.h"
#include "fse.h"
#include "huf_static.h"
#include "zstd_internal.h"
#include "divsufsort.h"
#include "zdict_static.h"



/*-*************************************
*  Constants
***************************************/
#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define DICTLISTSIZE 10000

#define NOISELENGTH 32
#define PRIME1   2654435761U
#define PRIME2   2246822519U

#define MINRATIO 4
static const U32 g_compressionLevel_default = 5;
static const U32 g_selectivity_default = 9;
static const size_t g_provision_entropySize = 200;
static const size_t g_min_fast_dictContent = 192;


/*-*************************************
*  Console display
***************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); }
static unsigned g_displayLevel = 0;   /* 0 : no display;   1: errors;   2: default;  4: full information */

#define DISPLAYUPDATE(l, ...) if (g_displayLevel>=l) { \
            if (ZDICT_GetMilliSpan(g_time) > refreshRate)  \
            { g_time = clock(); DISPLAY(__VA_ARGS__); \
            if (g_displayLevel>=4) fflush(stdout); } }
static const unsigned refreshRate = 300;
static clock_t g_time = 0;

static void ZDICT_printHex(U32 dlevel, const void* ptr, size_t length)
{
    const BYTE* const b = (const BYTE*)ptr;
    size_t u;
    for (u=0; u<length; u++)
    {
        BYTE c = b[u];
        if (c<32 || c>126) c = '.';   /* non-printable char */
        DISPLAYLEVEL(dlevel, "%c", c);
    }
}


/*-********************************************************
*  Helper functions
**********************************************************/
static unsigned ZDICT_GetMilliSpan(clock_t nPrevious)
{
    clock_t nCurrent = clock();
    unsigned nSpan = (unsigned)(((nCurrent - nPrevious) * 1000) / CLOCKS_PER_SEC);
    return nSpan;
}

unsigned ZDICT_isError(size_t errorCode) { return ERR_isError(errorCode); }

const char* ZDICT_getErrorName(size_t errorCode) { return ERR_getErrorName(errorCode); }


/*-********************************************************
*  Dictionary training functions
**********************************************************/
static unsigned ZDICT_NbCommonBytes (register size_t val)
{
    if (MEM_isLittleEndian()) {
        if (MEM_64bits()) {
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
        } else { /* 32 bits */
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
    } else {  /* Big Endian CPU */
        if (MEM_64bits()) {
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
        } else { /* 32 bits */
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
    }   }
}


/*! ZDICT_count() :
    Count the nb of common bytes between 2 pointers.
    Note : this function presumes end of buffer followed by noisy guard band.
*/
static size_t ZDICT_count(const void* pIn, const void* pMatch)
{
    const char* const pStart = (const char*)pIn;
    for (;;) {
        size_t diff = MEM_readST(pMatch) ^ MEM_readST(pIn);
        if (!diff) { pIn = (const char*)pIn+sizeof(size_t); pMatch = (const char*)pMatch+sizeof(size_t); continue; }
        pIn = (const char*)pIn+ZDICT_NbCommonBytes(diff);
        return (size_t)((const char*)pIn - pStart);
    }
}


typedef struct {
    U32 pos;
    U32 length;
    U32 savings;
} dictItem;

static void ZDICT_initDictItem(dictItem* d)
{
    d->pos = 1;
    d->length = 0;
    d->savings = (U32)(-1);
}


#define LLIMIT 64          /* heuristic determined experimentally */
#define MINMATCHLENGTH 7   /* heuristic determined experimentally */
static dictItem ZDICT_analyzePos(
                       BYTE* doneMarks,
                       const int* suffix, U32 start,
                       const void* buffer, U32 minRatio)
{
    U32 lengthList[LLIMIT] = {0};
    U32 cumulLength[LLIMIT] = {0};
    U32 savings[LLIMIT] = {0};
    const BYTE* b = (const BYTE*)buffer;
    size_t length;
    size_t maxLength = LLIMIT;
    size_t pos = suffix[start];
    U32 end = start;
    dictItem solution;

    /* init */
    memset(&solution, 0, sizeof(solution));
    doneMarks[pos] = 1;

    /* trivial repetition cases */
    if ( (MEM_read16(b+pos+0) == MEM_read16(b+pos+2))
       ||(MEM_read16(b+pos+1) == MEM_read16(b+pos+3))
       ||(MEM_read16(b+pos+2) == MEM_read16(b+pos+4)) ) {
        /* skip and mark segment */
        U16 u16 = MEM_read16(b+pos+4);
        U32 u, e = 6;
        while (MEM_read16(b+pos+e) == u16) e+=2 ;
        if (b[pos+e] == b[pos+e-1]) e++;
        for (u=1; u<e; u++)
            doneMarks[pos+u] = 1;
        return solution;
    }

    /* look forward */
    do {
        end++;
        length = ZDICT_count(b + pos, b + suffix[end]);
    } while (length >=MINMATCHLENGTH);

    /* look backward */
    do {
        length = ZDICT_count(b + pos, b + *(suffix+start-1));
        if (length >=MINMATCHLENGTH) start--;
    } while(length >= MINMATCHLENGTH);

    /* exit if not found a minimum nb of repetitions */
    if (end-start < minRatio) {
        U32 idx;
        for(idx=start; idx<end; idx++)
            doneMarks[suffix[idx]] = 1;
        return solution;
    }

    {   int i;
        U32 searchLength;
        U32 refinedStart = start;
        U32 refinedEnd = end;

        DISPLAYLEVEL(4, "\n");
        DISPLAYLEVEL(4, "found %3u matches of length >= %u at pos %7u  ", (U32)(end-start), MINMATCHLENGTH, (U32)pos);
        DISPLAYLEVEL(4, "\n");

        for (searchLength = MINMATCHLENGTH ; ; searchLength++) {
            BYTE currentChar = 0;
            U32 currentCount = 0;
            U32 currentID = refinedStart;
            U32 id;
            U32 selectedCount = 0;
            U32 selectedID = currentID;
            for (id =refinedStart; id < refinedEnd; id++) {
                if (b[ suffix[id] + searchLength] != currentChar) {
                    if (currentCount > selectedCount) {
                        selectedCount = currentCount;
                        selectedID = currentID;
                    }
                    currentID = id;
                    currentChar = b[ suffix[id] + searchLength];
                    currentCount = 0;
                }
                currentCount ++;
            }
            if (currentCount > selectedCount) {  /* for last */
                selectedCount = currentCount;
                selectedID = currentID;
            }

            if (selectedCount < minRatio)
                break;
            refinedStart = selectedID;
            refinedEnd = refinedStart + selectedCount;
        }

        /* evaluate gain based on new ref */
        start = refinedStart;
        pos = suffix[refinedStart];
        end = start;
        memset(lengthList, 0, sizeof(lengthList));

        /* look forward */
        do {
            end++;
            length = ZDICT_count(b + pos, b + suffix[end]);
            if (length >= LLIMIT) length = LLIMIT-1;
            lengthList[length]++;
        } while (length >=MINMATCHLENGTH);

        /* look backward */
        do {
            length = ZDICT_count(b + pos, b + suffix[start-1]);
            if (length >= LLIMIT) length = LLIMIT-1;
            lengthList[length]++;
            if (length >=MINMATCHLENGTH) start--;
        } while(length >= MINMATCHLENGTH);

        /* largest useful length */
        memset(cumulLength, 0, sizeof(cumulLength));
        cumulLength[maxLength-1] = lengthList[maxLength-1];
        for (i=(int)(maxLength-2); i>=0; i--)
            cumulLength[i] = cumulLength[i+1] + lengthList[i];

        for (i=LLIMIT-1; i>=MINMATCHLENGTH; i--) if (cumulLength[i]>=minRatio) break;
        maxLength = i;

        /* reduce maxLength in case of final into repetitive data */
        {
            U32 l = (U32)maxLength;
            BYTE c = b[pos + maxLength-1];
            while (b[pos+l-2]==c) l--;
            maxLength = l;
        }
        if (maxLength < MINMATCHLENGTH) return solution;   /* skip : no long-enough solution */

        /* calculate savings */
        savings[5] = 0;
        for (i=MINMATCHLENGTH; i<=(int)maxLength; i++)
            savings[i] = savings[i-1] + (lengthList[i] * (i-3));

        DISPLAYLEVEL(4, "Selected ref at position %u, of length %u : saves %u (ratio: %.2f)  \n",
                     (U32)pos, (U32)maxLength, savings[maxLength], (double)savings[maxLength] / maxLength);

        solution.pos = (U32)pos;
        solution.length = (U32)maxLength;
        solution.savings = savings[maxLength];

        /* mark positions done */
        {
            U32 id;
            U32 testedPos;
            for (id=start; id<end; id++) {
                U32 p, pEnd;
                testedPos = suffix[id];
                if (testedPos == pos)
                    length = solution.length;
                else {
                    length = ZDICT_count(b+pos, b+testedPos);
                    if (length > solution.length) length = solution.length;
                }
                pEnd = (U32)(testedPos + length);
                for (p=testedPos; p<pEnd; p++)
                    doneMarks[p] = 1;
    }   }   }

    return solution;
}


/*! ZDICT_checkMerge
    check if dictItem can be merged, do it if possible
    @return : id of destination elt, 0 if not merged
*/
static U32 ZDICT_checkMerge(dictItem* table, dictItem elt, U32 eltNbToSkip)
{
    const U32 tableSize = table->pos;
    const U32 max = elt.pos + (elt.length-1);

    /* tail overlap */
    U32 u; for (u=1; u<tableSize; u++) {
        if (u==eltNbToSkip) continue;
        if ((table[u].pos > elt.pos) && (table[u].pos < max)) {  /* overlap */
            /* append */
            U32 addedLength = table[u].pos - elt.pos;
            table[u].length += addedLength;
            table[u].pos = elt.pos;
            table[u].savings += elt.savings * addedLength / elt.length;   /* rough approx */
            table[u].savings += elt.length / 8;    /* rough approx */
            elt = table[u];
            while ((u>1) && (table[u-1].savings < elt.savings))
                table[u] = table[u-1], u--;
            table[u] = elt;
            return u;
    }   }

    /* front overlap */
    for (u=1; u<tableSize; u++) {
        if (u==eltNbToSkip) continue;
        if ((table[u].pos + table[u].length > elt.pos) && (table[u].pos < elt.pos)) {  /* overlap */
            /* append */
            int addedLength = (elt.pos + elt.length) - (table[u].pos + table[u].length);
            table[u].savings += elt.length / 8;    /* rough approx */
            if (addedLength > 0) {   /* otherwise, already included */
                table[u].length += addedLength;
                table[u].savings += elt.savings * addedLength / elt.length;   /* rough approx */
            }
            elt = table[u];
            while ((u>1) && (table[u-1].savings < elt.savings))
                table[u] = table[u-1], u--;
            table[u] = elt;
            return u;
    }   }

    return 0;
}


static void ZDICT_removeDictItem(dictItem* table, U32 id)
{
    /* convention : first element is nb of elts */
    U32 max = table->pos;
    U32 u;
    if (!id) return;   /* protection, should never happen */
    for (u=id; u<max-1; u++)
        table[u] = table[u+1];
    table->pos--;
}


static void ZDICT_insertDictItem(dictItem* table, U32 maxSize, dictItem elt)
{
    /* merge if possible */
    U32 mergeId = ZDICT_checkMerge(table, elt, 0);
    if (mergeId) {
        U32 newMerge = 1;
        while (newMerge) {
            newMerge = ZDICT_checkMerge(table, table[mergeId], mergeId);
            if (newMerge) ZDICT_removeDictItem(table, mergeId);
            mergeId = newMerge;
        }
        return;
    }

    /* insert */
    {
        U32 current;
        U32 nextElt = table->pos;
        if (nextElt >= maxSize) nextElt = maxSize-1;
        current = nextElt-1;
        while (table[current].savings < elt.savings) {
            table[current+1] = table[current];
            current--;
        }
        table[current+1] = elt;
        table->pos = nextElt+1;
    }
}


static U32 ZDICT_dictSize(const dictItem* dictList)
{
    U32 u, dictSize = 0;
    for (u=1; u<dictList[0].pos; u++)
        dictSize += dictList[u].length;
    return dictSize;
}


static size_t ZDICT_trainBuffer(dictItem* dictList, U32 dictListSize,
                            const void* const buffer, const size_t bufferSize,   /* buffer must end with noisy guard band */
                            const size_t* fileSizes, unsigned nbFiles,
                            U32 shiftRatio, unsigned maxDictSize)
{
    int* const suffix0 = (int*)malloc((bufferSize+2)*sizeof(*suffix0));
    int* const suffix = suffix0+1;
    U32* reverseSuffix = (U32*)malloc((bufferSize)*sizeof(*reverseSuffix));
    BYTE* doneMarks = (BYTE*)malloc((bufferSize+16)*sizeof(*doneMarks));   /* +16 for overflow security */
    U32* filePos = (U32*)malloc(nbFiles * sizeof(*filePos));
    U32 minRatio = nbFiles >> shiftRatio;
    int divSuftSortResult;
    size_t result = 0;

    /* init */
    DISPLAYLEVEL(2, "\r%70s\r", "");   /* clean display line */
    if (!suffix0 || !reverseSuffix || !doneMarks || !filePos) {
        result = ERROR(memory_allocation);
        goto _cleanup;
    }
    if (minRatio < MINRATIO) minRatio = MINRATIO;
    memset(doneMarks, 0, bufferSize+16);

    /* sort */
    DISPLAYLEVEL(2, "sorting %u files of total size %u MB ...\n", nbFiles, (U32)(bufferSize>>20));
    divSuftSortResult = divsufsort((const unsigned char*)buffer, suffix, (int)bufferSize, 0);
    if (divSuftSortResult != 0) { result = ERROR(GENERIC); goto _cleanup; }
    suffix[bufferSize] = (int)bufferSize;   /* leads into noise */
    suffix0[0] = (int)bufferSize;           /* leads into noise */
    {
        /* build reverse suffix sort */
        size_t pos;
        for (pos=0; pos < bufferSize; pos++)
            reverseSuffix[suffix[pos]] = (U32)pos;
        /* build file pos */
        filePos[0] = 0;
        for (pos=1; pos<nbFiles; pos++)
            filePos[pos] = (U32)(filePos[pos-1] + fileSizes[pos-1]);
    }

    DISPLAYLEVEL(2, "finding patterns ... \n");
    DISPLAYLEVEL(3, "minimum ratio : %u \n", minRatio);

    {
        U32 cursor; for (cursor=0; cursor < bufferSize; ) {
            dictItem solution;
            if (doneMarks[cursor]) { cursor++; continue; }
            solution = ZDICT_analyzePos(doneMarks, suffix, reverseSuffix[cursor], buffer, minRatio);
            if (solution.length==0) { cursor++; continue; }
            ZDICT_insertDictItem(dictList, dictListSize, solution);
            cursor += solution.length;
            DISPLAYUPDATE(2, "\r%4.2f %% \r", (double)cursor / bufferSize * 100);
    }   }

    /* limit dictionary size */
    {
        U32 max = dictList->pos;   /* convention : nb of useful elts within dictList */
        U32 currentSize = 0;
        U32 n; for (n=1; n<max; n++) {
            currentSize += dictList[n].length;
            if (currentSize > maxDictSize) break;
        }
        dictList->pos = n;
    }

_cleanup:
    free(suffix0);
    free(reverseSuffix);
    free(doneMarks);
    free(filePos);
    return result;
}


static void ZDICT_fillNoise(void* buffer, size_t length)
{
    unsigned acc = PRIME1;
    size_t p=0;;
    for (p=0; p<length; p++) {
        acc *= PRIME2;
        ((unsigned char*)buffer)[p] = (unsigned char)(acc >> 21);
    }
}


typedef struct
{
    ZSTD_CCtx* ref;
    ZSTD_CCtx* zc;
    void* workPlace;   /* must be ZSTD_BLOCKSIZE_MAX allocated */
} EStats_ress_t;


static void ZDICT_countEStats(EStats_ress_t esr,
                            U32* countLit, U32* offsetcodeCount, U32* matchlengthCount, U32* litlengthCount,
                            const void* src, size_t srcSize)
{
    const seqStore_t* seqStorePtr;

    if (srcSize > ZSTD_BLOCKSIZE_MAX) srcSize = ZSTD_BLOCKSIZE_MAX;   /* protection vs large samples */
    ZSTD_copyCCtx(esr.zc, esr.ref);
    ZSTD_compressBlock(esr.zc, esr.workPlace, ZSTD_BLOCKSIZE_MAX, src, srcSize);
    seqStorePtr = ZSTD_getSeqStore(esr.zc);

    /* literals stats */
    {   const BYTE* bytePtr;
        for(bytePtr = seqStorePtr->litStart; bytePtr < seqStorePtr->lit; bytePtr++)
            countLit[*bytePtr]++;
    }

    /* seqStats */
    {   size_t const nbSeq = (size_t)(seqStorePtr->offset - seqStorePtr->offsetStart);
        ZSTD_seqToCodes(seqStorePtr, nbSeq);

        {   const BYTE* codePtr = seqStorePtr->offCodeStart;
            size_t u;
            for (u=0; u<nbSeq; u++) offsetcodeCount[codePtr[u]]++;
        }

        {   const BYTE* codePtr = seqStorePtr->mlCodeStart;
            size_t u;
            for (u=0; u<nbSeq; u++) matchlengthCount[codePtr[u]]++;
        }

        {   const BYTE* codePtr = seqStorePtr->llCodeStart;
            size_t u;
            for (u=0; u<nbSeq; u++) litlengthCount[codePtr[u]]++;
    }   }
}

/*
static size_t ZDICT_maxSampleSize(const size_t* fileSizes, unsigned nbFiles)
{
    unsigned u;
    size_t max=0;
    for (u=0; u<nbFiles; u++)
        if (max < fileSizes[u]) max = fileSizes[u];
    return max;
}
*/

static size_t ZDICT_totalSampleSize(const size_t* fileSizes, unsigned nbFiles)
{
    size_t total;
    unsigned u;
    for (u=0, total=0; u<nbFiles; u++) total += fileSizes[u];
    return total;
}

#define OFFCODE_MAX 18  /* only applicable to first block */
static size_t ZDICT_analyzeEntropy(void*  dstBuffer, size_t maxDstSize,
                                 unsigned compressionLevel,
                           const void*  srcBuffer, const size_t* fileSizes, unsigned nbFiles,
                           const void* dictBuffer, size_t  dictBufferSize)
{
    U32 countLit[256];
    HUF_CREATE_STATIC_CTABLE(hufTable, 255);
    U32 offcodeCount[OFFCODE_MAX+1];
    short offcodeNCount[OFFCODE_MAX+1];
    U32 matchLengthCount[MaxML+1];
    short matchLengthNCount[MaxML+1];
    U32 litLengthCount[MaxLL+1];
    short litLengthNCount[MaxLL+1];
    EStats_ress_t esr;
    ZSTD_parameters params;
    U32 u, huffLog = 12, Offlog = OffFSELog, mlLog = MLFSELog, llLog = LLFSELog, total;
    size_t pos = 0, errorCode;
    size_t eSize = 0;
    size_t const totalSrcSize = ZDICT_totalSampleSize(fileSizes, nbFiles);
    size_t const averageSampleSize = totalSrcSize / nbFiles;

    /* init */
    for (u=0; u<256; u++) countLit[u]=1;   /* any character must be described */
    for (u=0; u<=OFFCODE_MAX; u++) offcodeCount[u]=1;
    for (u=0; u<=MaxML; u++) matchLengthCount[u]=1;
    for (u=0; u<=MaxLL; u++) litLengthCount[u]=1;
    esr.ref = ZSTD_createCCtx();
    esr.zc = ZSTD_createCCtx();
    esr.workPlace = malloc(ZSTD_BLOCKSIZE_MAX);
    if (!esr.ref || !esr.zc || !esr.workPlace) {
            eSize = ERROR(memory_allocation);
            DISPLAYLEVEL(1, "Not enough memory");
            goto _cleanup;
    }
    if (compressionLevel==0) compressionLevel=g_compressionLevel_default;
    params.cParams = ZSTD_getCParams(compressionLevel, averageSampleSize, dictBufferSize);
    params.cParams.strategy = ZSTD_greedy;
    params.fParams.contentSizeFlag = 0;
    ZSTD_compressBegin_advanced(esr.ref, dictBuffer, dictBufferSize, params, 0);

    /* collect stats on all files */
    for (u=0; u<nbFiles; u++) {
        ZDICT_countEStats(esr,
                        countLit, offcodeCount, matchLengthCount, litLengthCount,
           (const char*)srcBuffer + pos, fileSizes[u]);
        pos += fileSizes[u];
    }

    /* analyze */
    errorCode = HUF_buildCTable (hufTable, countLit, 255, huffLog);
    if (HUF_isError(errorCode)) {
        eSize = ERROR(GENERIC);
        DISPLAYLEVEL(1, "HUF_buildCTable error");
        goto _cleanup;
    }
    huffLog = (U32)errorCode;

    total=0; for (u=0; u<=OFFCODE_MAX; u++) total+=offcodeCount[u];
    errorCode = FSE_normalizeCount(offcodeNCount, Offlog, offcodeCount, total, OFFCODE_MAX);
    if (FSE_isError(errorCode)) {
        eSize = ERROR(GENERIC);
        DISPLAYLEVEL(1, "FSE_normalizeCount error with offcodeCount");
        goto _cleanup;
    }
    Offlog = (U32)errorCode;

    total=0; for (u=0; u<=MaxML; u++) total+=matchLengthCount[u];
    errorCode = FSE_normalizeCount(matchLengthNCount, mlLog, matchLengthCount, total, MaxML);
    if (FSE_isError(errorCode)) {
        eSize = ERROR(GENERIC);
        DISPLAYLEVEL(1, "FSE_normalizeCount error with matchLengthCount");
        goto _cleanup;
    }
    mlLog = (U32)errorCode;

    total=0; for (u=0; u<=MaxLL; u++) total+=litLengthCount[u];
    errorCode = FSE_normalizeCount(litLengthNCount, llLog, litLengthCount, total, MaxLL);
    if (FSE_isError(errorCode)) {
        eSize = ERROR(GENERIC);
        DISPLAYLEVEL(1, "FSE_normalizeCount error with litLengthCount");
        goto _cleanup;
    }
    llLog = (U32)errorCode;

    /* write result to buffer */
    errorCode = HUF_writeCTable(dstBuffer, maxDstSize, hufTable, 255, huffLog);
    if (HUF_isError(errorCode)) {
        eSize = ERROR(GENERIC);
        DISPLAYLEVEL(1, "HUF_writeCTable error");
        goto _cleanup;
    }
    dstBuffer = (char*)dstBuffer + errorCode;
    maxDstSize -= errorCode;
    eSize += errorCode;

    errorCode = FSE_writeNCount(dstBuffer, maxDstSize, offcodeNCount, OFFCODE_MAX, Offlog);
    if (FSE_isError(errorCode)) {
        eSize = ERROR(GENERIC);
        DISPLAYLEVEL(1, "FSE_writeNCount error with offcodeNCount");
        goto _cleanup;
    }
    dstBuffer = (char*)dstBuffer + errorCode;
    maxDstSize -= errorCode;
    eSize += errorCode;

    errorCode = FSE_writeNCount(dstBuffer, maxDstSize, matchLengthNCount, MaxML, mlLog);
    if (FSE_isError(errorCode)) {
        eSize = ERROR(GENERIC);
        DISPLAYLEVEL(1, "FSE_writeNCount error with matchLengthNCount");
        goto _cleanup;
    }
    dstBuffer = (char*)dstBuffer + errorCode;
    maxDstSize -= errorCode;
    eSize += errorCode;

    errorCode = FSE_writeNCount(dstBuffer, maxDstSize, litLengthNCount, MaxLL, llLog);
    if (FSE_isError(errorCode)) {
        eSize = ERROR(GENERIC);
        DISPLAYLEVEL(1, "FSE_writeNCount error with litlengthNCount");
        goto _cleanup;
    }
    dstBuffer = (char*)dstBuffer + errorCode;
    maxDstSize -= errorCode;
    eSize += errorCode;

_cleanup:
    ZSTD_freeCCtx(esr.ref);
    ZSTD_freeCCtx(esr.zc);
    free(esr.workPlace);

    return eSize;
}


#define DIB_FASTSEGMENTSIZE 64
/*! ZDICT_fastSampling()  (based on an idea proposed by Giuseppe Ottaviano) :
    Fill `dictBuffer` with stripes of size DIB_FASTSEGMENTSIZE from `samplesBuffer`,
    up to `dictSize`.
    Filling starts from the end of `dictBuffer`, down to maximum possible.
    if `dictSize` is not a multiply of DIB_FASTSEGMENTSIZE, some bytes at beginning of `dictBuffer` won't be used.
    @return : amount of data written into `dictBuffer`,
              or an error code
*/
static size_t ZDICT_fastSampling(void* dictBuffer, size_t dictSize,
                         const void* samplesBuffer, size_t samplesSize)
{
    char* dstPtr = (char*)dictBuffer + dictSize;
    const char* srcPtr = (const char*)samplesBuffer;
    size_t nbSegments = dictSize / DIB_FASTSEGMENTSIZE;
    size_t segNb, interSize;

    if (nbSegments <= 2) return ERROR(srcSize_wrong);
    if (samplesSize < dictSize) return ERROR(srcSize_wrong);

    /* first and last segments are part of dictionary, in case they contain interesting header/footer */
    dstPtr -= DIB_FASTSEGMENTSIZE;
    memcpy(dstPtr, srcPtr, DIB_FASTSEGMENTSIZE);
    dstPtr -= DIB_FASTSEGMENTSIZE;
    memcpy(dstPtr, srcPtr+samplesSize-DIB_FASTSEGMENTSIZE, DIB_FASTSEGMENTSIZE);

    /* regularly copy a segment */
    interSize = (samplesSize - nbSegments*DIB_FASTSEGMENTSIZE) / (nbSegments-1);
    srcPtr += DIB_FASTSEGMENTSIZE;
    for (segNb=2; segNb < nbSegments; segNb++) {
        srcPtr += interSize;
        dstPtr -= DIB_FASTSEGMENTSIZE;
        memcpy(dstPtr, srcPtr, DIB_FASTSEGMENTSIZE);
        srcPtr += DIB_FASTSEGMENTSIZE;
    }

    return nbSegments * DIB_FASTSEGMENTSIZE;
}


#define DIB_MINSAMPLESSIZE (DIB_FASTSEGMENTSIZE*3)
/*! ZDICT_trainFromBuffer_unsafe() :
*   `samplesBuffer` must be followed by noisy guard band.
*   @return : size of dictionary.
*/
size_t ZDICT_trainFromBuffer_unsafe(
                            void* dictBuffer, size_t maxDictSize,
                            const void* samplesBuffer, const size_t* sampleSizes, unsigned nbSamples,
                            ZDICT_params_t params)
{
    U32 const dictListSize = MAX( MAX(DICTLISTSIZE, nbSamples), (U32)(maxDictSize/16));
    dictItem* dictList = (dictItem*)malloc(dictListSize * sizeof(*dictList));
    unsigned selectivity = params.selectivityLevel;
    unsigned compressionLevel = params.compressionLevel;
    size_t targetDictSize = maxDictSize;
    size_t sBuffSize;
    size_t dictSize = 0;

    /* checks */
    if (maxDictSize <= g_provision_entropySize + g_min_fast_dictContent) return ERROR(dstSize_tooSmall);
    if (!dictList) return ERROR(memory_allocation);

    /* init */
    { unsigned u; for (u=0, sBuffSize=0; u<nbSamples; u++) sBuffSize += sampleSizes[u]; }
    if (sBuffSize < DIB_MINSAMPLESSIZE) return 0;   /* not enough source to create dictionary */
    ZDICT_initDictItem(dictList);
    g_displayLevel = params.notificationLevel;
    if (selectivity==0) selectivity = g_selectivity_default;
    if (compressionLevel==0) compressionLevel = g_compressionLevel_default;

    /* build dictionary */
    if (selectivity>1) {  /* selectivity == 1 => fast mode */
        ZDICT_trainBuffer(dictList, dictListSize,
                        samplesBuffer, sBuffSize,
                        sampleSizes, nbSamples,
                        selectivity, (U32)targetDictSize);

        /* display best matches */
        if (g_displayLevel>= 3) {
            U32 const nb = 25;
            U32 const dictContentSize = ZDICT_dictSize(dictList);
            U32 u;
            DISPLAYLEVEL(3, "\n %u segments found, of total size %u \n", dictList[0].pos, dictContentSize);
            DISPLAYLEVEL(3, "list %u best segments \n", nb);
            for (u=1; u<=nb; u++) {
                U32 p = dictList[u].pos;
                U32 l = dictList[u].length;
                U32 d = MIN(40, l);
                DISPLAYLEVEL(3, "%3u:%3u bytes at pos %8u, savings %7u bytes |",
                             u, l, p, dictList[u].savings);
                ZDICT_printHex(3, (const char*)samplesBuffer+p, d);
                DISPLAYLEVEL(3, "| \n");
    }   }   }

    /* create dictionary */
    {   U32 dictContentSize = ZDICT_dictSize(dictList);
        size_t hSize;
        BYTE* ptr;
        U32 u;

        /* build dict content */
        ptr = (BYTE*)dictBuffer + maxDictSize;
        for (u=1; u<dictList->pos; u++) {
            U32 l = dictList[u].length;
            ptr -= l;
            if (ptr<(BYTE*)dictBuffer) return ERROR(GENERIC);   /* should not happen */
            memcpy(ptr, (const char*)samplesBuffer+dictList[u].pos, l);
        }

        /* fast mode dict content */
        if (selectivity==1) {  /* note could also be used to complete a dictionary, but not necessarily better */
            DISPLAYLEVEL(3, "\r%70s\r", "");   /* clean display line */
            DISPLAYLEVEL(3, "Adding %u KB with fast sampling \n", (U32)(targetDictSize>>10));
            dictContentSize = (U32)ZDICT_fastSampling(dictBuffer, targetDictSize,
                                                      samplesBuffer, sBuffSize);
        }

       /* dictionary header */
        MEM_writeLE32(dictBuffer, ZSTD_DICT_MAGIC);
        hSize = 4;

        /* entropic tables */
        DISPLAYLEVEL(2, "\r%70s\r", "");   /* clean display line */
        DISPLAYLEVEL(2, "statistics ... \n");
        hSize += ZDICT_analyzeEntropy((char*)dictBuffer+4, maxDictSize-4,
                                    compressionLevel,
                                    samplesBuffer, sampleSizes, nbSamples,
                                    (char*)dictBuffer + maxDictSize - dictContentSize, dictContentSize);

        if (hSize + dictContentSize < maxDictSize)
            memmove((char*)dictBuffer + hSize, (char*)dictBuffer + maxDictSize - dictContentSize, dictContentSize);
        dictSize = MIN(maxDictSize, hSize+dictContentSize);
    }

    /* clean up */
    free(dictList);
    return dictSize;
}


/* issue : samplesBuffer need to be followed by a noisy guard band.
*  work around : duplicate the buffer, and add the noise */
size_t ZDICT_trainFromBuffer_advanced(void* dictBuffer, size_t dictBufferCapacity,
                           const void* samplesBuffer, const size_t* samplesSizes, unsigned nbSamples,
                           ZDICT_params_t params)
{
    void* newBuff;
    size_t sBuffSize;

    { unsigned u; for (u=0, sBuffSize=0; u<nbSamples; u++) sBuffSize += samplesSizes[u]; }
    if (sBuffSize==0) return 0;   /* empty content => no dictionary */
    newBuff = malloc(sBuffSize + NOISELENGTH);
    if (!newBuff) return ERROR(memory_allocation);

    memcpy(newBuff, samplesBuffer, sBuffSize);
    ZDICT_fillNoise((char*)newBuff + sBuffSize, NOISELENGTH);   /* guard band, for end of buffer condition */

    { size_t const result = ZDICT_trainFromBuffer_unsafe(
                                        dictBuffer, dictBufferCapacity,
                                        newBuff, samplesSizes, nbSamples,
                                        params);
      free(newBuff);
      return result; }
}


size_t ZDICT_trainFromBuffer(void* dictBuffer, size_t dictBufferCapacity,
                             const void* samplesBuffer, const size_t* samplesSizes, unsigned nbSamples)
{
    ZDICT_params_t params;
    memset(&params, 0, sizeof(params));
    return ZDICT_trainFromBuffer_advanced(dictBuffer, dictBufferCapacity,
                                          samplesBuffer, samplesSizes, nbSamples,
                                          params);
}

