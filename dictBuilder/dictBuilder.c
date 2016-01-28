/*
    dictBuilder.c
    Copyright (C) Yann Collet 2016

    GPL v2 License

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    You can contact the author at :
    - zstd source repository : https://github.com/Cyan4973/zstd
    - ztsd public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/* **************************************
*  Compiler Options
****************************************/
/* Disable some Visual warning messages */
#ifdef _MSC_VER
#  define _CRT_SECURE_NO_WARNINGS                /* fopen */
#  pragma warning(disable : 4127)                /* disable: C4127: conditional expression is constant */
#endif

/* Unix Large Files support (>4GB) */
#define _FILE_OFFSET_BITS 64
#if (defined(__sun__) && (!defined(__LP64__)))   /* Sun Solaris 32-bits requires specific definitions */
#  define _LARGEFILE_SOURCE
#elif ! defined(__LP64__)                        /* No point defining Large file for 64 bit */
#  define _LARGEFILE64_SOURCE
#endif

/* S_ISREG & gettimeofday() are not supported by MSVC */
#if defined(_MSC_VER) || defined(_WIN32)
#  define BMK_LEGACY_TIMER 1
#endif


/* *************************************
*  Includes
***************************************/
#include <stdlib.h>      /* malloc, free */
#include <string.h>      /* memset */
#include <stdio.h>       /* fprintf, fopen, ftello64 */
#include <sys/types.h>   /* stat64 */
#include <sys/stat.h>    /* stat64 */
#include <time.h>        /* clock */

#include "mem.h"         /* read */
#include "divsufsort.h"
#include "dictBuilder.h"
#include "zstd_compress.c"
#include "huff0_static.h"


/* *************************************
*  Compiler specifics
***************************************/
#if !defined(S_ISREG)
#  define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif

#ifdef _MSC_VER
#define snprintf sprintf_s
#endif


/* *************************************
*  Constants
***************************************/
#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define DICTLISTSIZE 10000
#define MEMMULT 11
static const size_t maxMemory = (sizeof(size_t)==4)  ?  (2 GB - 64 MB) : (size_t)(3 GB) * MEMMULT;

#define NOISELENGTH 32
#define PRIME1   2654435761U
#define PRIME2   2246822519U

#define MINRATIO 4


/* *************************************
*  console display
***************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); }
static unsigned g_displayLevel = 0;   /* 0 : no display;   1: errors;   2: default;  4: full information */
void DiB_setNotificationLevel(unsigned l) { g_displayLevel=l; }

#define DISPLAYUPDATE(l, ...) if (g_displayLevel>=l) { \
            if (DiB_GetMilliSpan(g_time) > refreshRate)  \
            { g_time = clock(); DISPLAY(__VA_ARGS__); \
            if (g_displayLevel>=4) fflush(stdout); } }
static const unsigned refreshRate = 300;
static clock_t g_time = 0;

void DiB_printHex(U32 dlevel, const void* ptr, size_t length)
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


/* *************************************
*  Exceptions
***************************************/
#ifndef DEBUG
#  define DEBUG 0
#endif
#define DEBUGOUTPUT(...) if (DEBUG) DISPLAY(__VA_ARGS__);
#define EXM_THROW(error, ...)                                             \
{                                                                         \
    DEBUGOUTPUT("Error defined at %s, line %i : \n", __FILE__, __LINE__); \
    DISPLAYLEVEL(1, "Error %i : ", error);                                \
    DISPLAYLEVEL(1, __VA_ARGS__);                                         \
    DISPLAYLEVEL(1, "\n");                                                \
    exit(error);                                                          \
}


/* ********************************************************
*  Helper functions
**********************************************************/
unsigned DiB_versionNumber (void) { return DiB_VERSION_NUMBER; }

static unsigned DiB_GetMilliSpan(clock_t nPrevious)
{
    clock_t nCurrent = clock();
    unsigned nSpan = (unsigned)(((nCurrent - nPrevious) * 1000) / CLOCKS_PER_SEC);
    return nSpan;
}


/* ********************************************************
*  File related operations
**********************************************************/
static unsigned long long DiB_getFileSize(const char* infilename)
{
    int r;
#if defined(_MSC_VER)
    struct _stat64 statbuf;
    r = _stat64(infilename, &statbuf);
#else
    struct stat statbuf;
    r = stat(infilename, &statbuf);
#endif
    if (r || !S_ISREG(statbuf.st_mode)) return 0;   /* No good... */
    return (unsigned long long)statbuf.st_size;
}


static unsigned long long DiB_getTotalFileSize(const char** fileNamesTable, unsigned nbFiles)
{
    unsigned long long total = 0;
    unsigned n;
    for (n=0; n<nbFiles; n++)
        total += DiB_getFileSize(fileNamesTable[n]);
    return total;
}


static void DiB_loadFiles(void* buffer, size_t bufferSize,
                          size_t* fileSizes,
                          const char** fileNamesTable, unsigned nbFiles)
{
    char* buff = (char*)buffer;
    size_t pos = 0;
    unsigned n;

    for (n=0; n<nbFiles; n++) {
        size_t readSize;
        unsigned long long fileSize = DiB_getFileSize(fileNamesTable[n]);
        FILE* f = fopen(fileNamesTable[n], "rb");
        if (f==NULL) EXM_THROW(10, "impossible to open file %s", fileNamesTable[n]);
        DISPLAYLEVEL(2, "Loading %s...       \r", fileNamesTable[n]);
        if (fileSize > bufferSize-pos) fileSize = 0;  /* stop there, not enough memory to load all files */
        readSize = fread(buff+pos, 1, (size_t)fileSize, f);
        if (readSize != (size_t)fileSize) EXM_THROW(11, "could not read %s", fileNamesTable[n]);
        pos += readSize;
        fileSizes[n] = (size_t)fileSize;
        fclose(f);
    }
}


/*-********************************************************
*  Dictionary training functions
**********************************************************/
static size_t DiB_read_ARCH(const void* p) { size_t r; memcpy(&r, p, sizeof(r)); return r; }

static unsigned DiB_NbCommonBytes (register size_t val)
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


/*! DiB_count() :
    Count the nb of common bytes between 2 pointers.
    Note : this function presumes end of buffer followed by noisy guard band.
*/
static size_t DiB_count(const void* pIn, const void* pMatch)
{
    const char* const pStart = (const char*)pIn;
    for (;;) {
        size_t diff = DiB_read_ARCH(pMatch) ^ DiB_read_ARCH(pIn);
        if (!diff) { pIn = (const char*)pIn+sizeof(size_t); pMatch = (const char*)pMatch+sizeof(size_t); continue; }
        pIn = (const char*)pIn+DiB_NbCommonBytes(diff);
        return (size_t)((const char*)pIn - pStart);
    }
}


typedef struct {
    U32 pos;
    U32 length;
    U32 savings;
} dictItem;

void DiB_initDictItem(dictItem* d)
{
    d->pos = 1;
    d->length = 0;
    d->savings = (U32)(-1);
}


#define LLIMIT 64          /* heuristic determined experimentally */
#define MINMATCHLENGTH 7   /* heuristic determined experimentally */
static dictItem DiB_analyzePos(
                       BYTE* doneMarks,
                       const saidx_t* suffix, U32 start,
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
        length = DiB_count(b + pos, b + suffix[end]);
    } while (length >=MINMATCHLENGTH);

    /* look backward */
    do {
        length = DiB_count(b + pos, b + *(suffix+start-1));
        if (length >=MINMATCHLENGTH) start--;
    } while(length >= MINMATCHLENGTH);

    /* exit if not found a minimum nb of repetitions */
    if (end-start < minRatio) {
        U32 idx;
        for(idx=start; idx<end; idx++)
            doneMarks[suffix[idx]] = 1;
        return solution;
    }

    {
        int i;
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
            //DISPLAYLEVEL(4, "best char at length %u: %02X  (seen %u times)  (pos %u) \n", searchLength+1, selectedChar, selectedCount, selectedRef);
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
            length = DiB_count(b + pos, b + suffix[end]);
            if (length >= LLIMIT) length = LLIMIT-1;
            lengthList[length]++;
        } while (length >=MINMATCHLENGTH);

        /* look backward */
        do {
            length = DiB_count(b + pos, b + suffix[start-1]);
            if (length >= LLIMIT) length = LLIMIT-1;
            lengthList[length]++;
            if (length >=MINMATCHLENGTH) start--;
        } while(length >= MINMATCHLENGTH);

        /* largest useful length */
        memset(cumulLength, 0, sizeof(cumulLength));
        cumulLength[maxLength-1] = lengthList[maxLength-1];
        for (i=maxLength-2; i>=0; i--)
            cumulLength[i] = cumulLength[i+1] + lengthList[i];

        for (i=LLIMIT-1; i>=MINMATCHLENGTH; i--) if (cumulLength[i]>=minRatio) break;
        maxLength = i;

        /* reduce maxLength in case of final into repetitive data */
        {
            U32 l = maxLength;
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

        solution.pos = pos;
        solution.length = maxLength;
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
                    length = DiB_count(b+pos, b+testedPos);
                    if (length > solution.length) length = solution.length;
                }
                pEnd = testedPos + length;
                for (p=testedPos; p<pEnd; p++)
                    doneMarks[p] = 1;
    }   }   }

    return solution;
}


/*! DiB_checkMerge
    check if dictItem can be merged, do it if possible
    @return : id of destination elt, 0 if not merged
*/
static U32 DiB_checkMerge(dictItem* table, dictItem elt, U32 eltNbToSkip)
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


static void DiB_removeDictItem(dictItem* table, U32 id)
{
    /* convention : first element is nb of elts */
    U32 max = table->pos;
    U32 u;
    if (!id) return;   /* protection, should never happen */
    for (u=id; u<max; u++)
        table[u] = table[u+1];
    table->pos--;
}


static void DiB_insertDictItem(dictItem* table, U32 maxSize, dictItem elt)
{
    /* merge if possible */
    U32 mergeId = DiB_checkMerge(table, elt, 0);
    if (mergeId) {
        U32 newMerge = 1;
        while (newMerge) {
            newMerge = DiB_checkMerge(table, table[mergeId], mergeId);
            if (newMerge) DiB_removeDictItem(table, mergeId);
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


static U32 DiB_dictSize(const dictItem* dictList)
{
    U32 u, dictSize = 0;
    for (u=1; u<dictList[0].pos; u++)
        dictSize += dictList[u].length;
    return dictSize;
}


static void DiB_trainBuffer(dictItem* dictList, U32 dictListSize,
                            const void* const buffer, const size_t bufferSize,   /* buffer must end with noisy guard band */
                            const char* displayName,
                            const size_t* fileSizes, size_t nbFiles, unsigned maxDictSize,
                            U32 shiftRatio)
{
    saidx_t* const suffix0 = (saidx_t*)malloc((bufferSize+2)*sizeof(*suffix0));
    saidx_t* const suffix = suffix0+1;
    U32* reverseSuffix = (U32*)malloc((bufferSize)*sizeof(*reverseSuffix));
    BYTE* doneMarks = (BYTE*)malloc((bufferSize+16)*sizeof(*doneMarks));   /* +16 for overflow security */
    U32* filePos = (U32*)malloc(nbFiles * sizeof(*filePos));
    U32 minRatio = nbFiles >> shiftRatio;
    saint_t errorCode;

    /* init */
    DISPLAYLEVEL(2, "\r%70s\r", "");   /* clean display line */
    if (!suffix0 || !reverseSuffix || !doneMarks || !filePos)
        EXM_THROW(1, "not enough memory for DiB_trainBuffer");
    if (minRatio < MINRATIO) minRatio = MINRATIO;
    memset(doneMarks, 0, bufferSize+16);

    /* sort */
    DISPLAYLEVEL(2, "sorting %s ...\n", displayName);
    errorCode = divsufsort((const sauchar_t*)buffer, suffix, bufferSize);
    if (errorCode != 0) EXM_THROW(2, "sort failed");
    suffix[bufferSize] = bufferSize;   /* leads into noise */
    suffix0[0] = bufferSize;           /* leads into noise */
    {
        /* build reverse suffix sort */
        size_t pos;
        for (pos=0; pos < bufferSize; pos++)
            reverseSuffix[suffix[pos]] = pos;
        /* build file pos */
        filePos[0] = 0;
        for (pos=1; pos<nbFiles; pos++)
            filePos[pos] = filePos[pos-1] + fileSizes[pos-1];
    }

    DISPLAYLEVEL(2, "finding patterns ... \n");
    DISPLAYLEVEL(4, "minimum ratio : %u \n", minRatio);

    {
        U32 cursor; for (cursor=0; cursor < bufferSize; ) {
            dictItem solution;

            if (doneMarks[cursor]) { cursor++; continue; }
            solution = DiB_analyzePos(doneMarks, suffix, reverseSuffix[cursor], buffer, minRatio);
            if (solution.length==0) { cursor++; continue; }
            DiB_insertDictItem(dictList, dictListSize, solution);
            cursor += solution.length;
            DISPLAYUPDATE(2, "\r%4.2f %% \r", (double)cursor / bufferSize * 100);
        }

#if 0
        /* 2nd scan */
        for (cursor=0; cursor < bufferSize; cursor++ )
        {
            dictItem solution;

            if (doneMarks[cursor]) continue;
            solution = DiB_analyzePos(doneMarks, suffix, reverseSuffix[cursor], buffer, minRatio);
            if (solution.length==0) continue;
            DiB_insertDictItem(dictList, dictListSize, solution);
            DISPLAYUPDATE(2, "\r%4.2f %% \r", (double)cursor / bufferSize * 100);
        }
#endif
    }

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

    free(suffix0);
    free(reverseSuffix);
    free(doneMarks);
    free(filePos);
}


static size_t DiB_findMaxMem(unsigned long long requiredMem)
{
    size_t step = 8 MB;
    void* testmem = NULL;

    requiredMem = (((requiredMem >> 23) + 1) << 23);
    requiredMem += 2 * step;
    if (requiredMem > maxMemory) requiredMem = maxMemory;

    while (!testmem) {
        requiredMem -= step;
        testmem = malloc((size_t)requiredMem);
    }

    free(testmem);
    return (size_t)(requiredMem - step);
}


static void DiB_fillNoise(void* buffer, size_t length)
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
    void* workPlace;   /* must be BLOCKSIZE allocated */
} EStats_ress_t;


static void DiB_countEStats(EStats_ress_t esr,
                            U32* countLit, U32* offsetcodeCount, U32* matchlengthCount, U32* litlengthCount,
                            const void* src, size_t srcSize)
{
    const BYTE* bytePtr;
    const U32* u32Ptr;

    if (srcSize > BLOCKSIZE) srcSize = BLOCKSIZE;   /* protection vs large samples */
    ZSTD_copyCCtx(esr.zc, esr.ref);
    ZSTD_compressBlock(esr.zc, esr.workPlace, BLOCKSIZE, src, srcSize);

    /* count stats */
    for(bytePtr = esr.zc->seqStore.litStart; bytePtr < esr.zc->seqStore.lit; bytePtr++)
        countLit[*bytePtr]++;
    for(u32Ptr = esr.zc->seqStore.offsetStart; u32Ptr < esr.zc->seqStore.offset; u32Ptr++) {
        BYTE offcode = (BYTE)ZSTD_highbit(*u32Ptr) + 1;
        if (*u32Ptr==0) offcode=0;
        offsetcodeCount[offcode]++;
    }
    for(bytePtr = esr.zc->seqStore.matchLengthStart; bytePtr < esr.zc->seqStore.matchLength; bytePtr++)
        matchlengthCount[*bytePtr]++;
    for(bytePtr = esr.zc->seqStore.litLengthStart; bytePtr < esr.zc->seqStore.litLength; bytePtr++)
        litlengthCount[*bytePtr]++;
}


#define OFFCODE_MAX 18
static size_t DiB_analyzeEntropy(void*  dstBuffer, size_t maxDstSize,
                           const void*  srcBuffer, size_t* fileSizes, unsigned nbFiles,
                           const void* dictBuffer, size_t  dictBufferSize)
{
    U32 countLit[256];
    U32 offcodeCount[MaxOff+1];
    HUF_CREATE_STATIC_CTABLE(hufTable, 255);
    short offcodeNCount[MaxOff+1];
    U32 matchLengthCount[MaxML+1];
    short matchLengthNCount[MaxML+1];
    U32 litlengthCount[MaxLL+1];
    short litlengthNCount[MaxLL+1];
    EStats_ress_t esr;
    ZSTD_parameters params;
    U32 u, huffLog = 12, Offlog = OffFSELog, mlLog = MLFSELog, llLog = LLFSELog, total;
    size_t pos = 0, errorCode;
    size_t eSize = 0;

    /* init */
    for (u=0; u<256; u++) countLit[u]=1;   /* any character must be described */
    for (u=0; u<=OFFCODE_MAX; u++) offcodeCount[u]=1;
    for (u=0; u<=MaxML; u++) matchLengthCount[u]=1;
    for (u=0; u<=MaxLL; u++) litlengthCount[u]=1;
    esr.ref = ZSTD_createCCtx();
    esr.zc = ZSTD_createCCtx();
    esr.workPlace = malloc(BLOCKSIZE);
    if (!esr.ref || !esr.zc || !esr.workPlace) EXM_THROW(30, "Not enough memory");
    params = ZSTD_getParams(5, dictBufferSize + 15 KB);
    params.strategy = ZSTD_greedy;
    ZSTD_compressBegin_advanced(esr.ref, dictBuffer, dictBufferSize, params);

    /* collect stats on all files */
    for (u=0; u<nbFiles; u++) {
        DiB_countEStats(esr,
                        countLit, offcodeCount, matchLengthCount, litlengthCount,
           (const char*)srcBuffer + pos, fileSizes[u]);
        pos += fileSizes[u];
    }

    /* analyze */
    errorCode = HUF_buildCTable (hufTable, countLit, 255, huffLog);
    if (HUF_isError(errorCode)) EXM_THROW(31, "HUF_buildCTable error");
    huffLog = (U32)errorCode;

    total=0; for (u=0; u<=OFFCODE_MAX; u++) total+=offcodeCount[u];
    errorCode = FSE_normalizeCount(offcodeNCount, Offlog, offcodeCount, total, OFFCODE_MAX);
    if (FSE_isError(errorCode)) EXM_THROW(32, "FSE_normalizeCount error with offcodeCount");
    Offlog = (U32)errorCode;

    total=0; for (u=0; u<=MaxML; u++) total+=matchLengthCount[u];
    errorCode = FSE_normalizeCount(matchLengthNCount, mlLog, matchLengthCount, total, MaxML);
    if (FSE_isError(errorCode)) EXM_THROW(33, "FSE_normalizeCount error with matchLengthCount");
    mlLog = (U32)errorCode;

    total=0; for (u=0; u<=MaxLL; u++) total+=litlengthCount[u];
    errorCode = FSE_normalizeCount(litlengthNCount, llLog, litlengthCount, total, MaxLL);
    if (FSE_isError(errorCode)) EXM_THROW(34, "FSE_normalizeCount error with litlengthCount");
    llLog = (U32)errorCode;

    /* write result to buffer */
    errorCode = HUF_writeCTable(dstBuffer, maxDstSize, hufTable, 255, huffLog);
    if (HUF_isError(errorCode)) EXM_THROW(41, "HUF_writeCTable error");
    dstBuffer = (char*)dstBuffer + errorCode;
    maxDstSize -= errorCode;
    eSize += errorCode;

    errorCode = FSE_writeNCount(dstBuffer, maxDstSize, offcodeNCount, OFFCODE_MAX, Offlog);
    if (FSE_isError(errorCode)) EXM_THROW(42, "FSE_writeNCount error with offcodeNCount");
    dstBuffer = (char*)dstBuffer + errorCode;
    maxDstSize -= errorCode;
    eSize += errorCode;

    errorCode = FSE_writeNCount(dstBuffer, maxDstSize, matchLengthNCount, MaxML, mlLog);
    if (FSE_isError(errorCode)) EXM_THROW(43, "FSE_writeNCount error with matchLengthNCount");
    dstBuffer = (char*)dstBuffer + errorCode;
    maxDstSize -= errorCode;
    eSize += errorCode;

    errorCode = FSE_writeNCount(dstBuffer, maxDstSize, litlengthNCount, MaxLL, llLog);
    if (FSE_isError(errorCode)) EXM_THROW(43, "FSE_writeNCount error with litlengthNCount");
    dstBuffer = (char*)dstBuffer + errorCode;
    maxDstSize -= errorCode;
    eSize += errorCode;

    /* clean */
    ZSTD_freeCCtx(esr.ref);
    ZSTD_freeCCtx(esr.zc);
    free(esr.workPlace);

    return eSize;
}


static void DiB_saveDict(const char* dictFileName,
                         const void* buff1, size_t buff1Size,
                         const void* buff2, size_t buff2Size)
{
    FILE* f;
    size_t n;

    f = fopen(dictFileName, "wb");
    if (f==NULL) EXM_THROW(3, "cannot open %s ", dictFileName);

    n = fwrite(buff1, 1, buff1Size, f);
    if (n!=buff1Size) EXM_THROW(4, "%s : write error", dictFileName)

    n = fwrite(buff2, 1, buff2Size, f);
    if (n!=buff2Size) EXM_THROW(4, "%s : write error", dictFileName)

    n = (size_t)fclose(f);
    if (n!=0) EXM_THROW(5, "%s : flush error", dictFileName)
}


int DiB_trainDictionary(const char* dictFileName, unsigned maxDictSize, unsigned shiftRatio,
                   const char** fileNamesTable, unsigned nbFiles)
{
    void* srcBuffer;
    size_t benchedSize;
    size_t* fileSizes = (size_t*)malloc(nbFiles * sizeof(size_t));
    unsigned long long totalSizeToLoad = DiB_getTotalFileSize(fileNamesTable, nbFiles);
    const U32 dictListSize = DICTLISTSIZE;
    dictItem* dictList = (dictItem*)malloc(dictListSize * sizeof(*dictList));
    char mfName[20] = {0};
    const char* displayName = NULL;

    /* init */
    benchedSize = DiB_findMaxMem(totalSizeToLoad * MEMMULT) / MEMMULT;
    if ((unsigned long long)benchedSize > totalSizeToLoad) benchedSize = (size_t)totalSizeToLoad;
    if (benchedSize < totalSizeToLoad)
        DISPLAY("Not enough memory; training on %u MB only...\n", (unsigned)(benchedSize >> 20));

    /* Memory allocation & restrictions */
    srcBuffer = malloc(benchedSize+NOISELENGTH);                                          /* + noise */
    if ((!fileSizes) || (!srcBuffer) || (!dictList)) EXM_THROW(12, "not enough memory for DiB_trainFiles");  /* should not happen */
    DiB_initDictItem(dictList);

    /* Load input buffer */
    DiB_loadFiles(srcBuffer, benchedSize, fileSizes, fileNamesTable, nbFiles);
    DiB_fillNoise((char*)srcBuffer + benchedSize, NOISELENGTH);   /* for end of buffer condition */

    /* Train */
    snprintf (mfName, sizeof(mfName), " %u files", nbFiles);
    if (nbFiles > 1) displayName = mfName;
    else displayName = fileNamesTable[0];

    DiB_trainBuffer(dictList, dictListSize,
                    srcBuffer, benchedSize,
                    displayName,
                    fileSizes, nbFiles, maxDictSize,
                    shiftRatio);

    /* display best matches */
    if (g_displayLevel>= 3) {
        const U32 nb = 25;
        U32 u;
        U32 dictContentSize = DiB_dictSize(dictList);
        DISPLAYLEVEL(3, "\n %u segments found, of total size %u \n", dictList[0].pos, dictContentSize);
        DISPLAYLEVEL(3, "list %u best segments \n", nb);
        for (u=1; u<=nb; u++) {
            U32 p = dictList[u].pos;
            U32 l = dictList[u].length;
            U32 d = MIN(40, l);
            DISPLAYLEVEL(3, "%3u:%3u bytes at pos %8u, savings %7u bytes |",
                         u, l, p, dictList[u].savings);
            DiB_printHex(3, (char*)srcBuffer+p, d);
            DISPLAYLEVEL(3, "| \n");
    }   }

    /* create dictionary */
    {
        void* dictContent;
        U32 dictContentSize = DiB_dictSize(dictList);
        void* dictHeader;
        size_t dictHeaderSize, hSize;
        BYTE* ptr;
        U32 u;

        /* build dict */
        #define EBSIZE (2 KB)
        dictHeaderSize = EBSIZE;
        dictHeader = malloc(dictHeaderSize);
        dictContent = malloc(dictContentSize);
        if (!dictHeader || !dictContent) EXM_THROW(2, "not enough memory");

        /* build dict content */
        ptr = (BYTE*)dictContent + dictContentSize;

        for (u=1; u<dictList->pos; u++) {
            U32 l = dictList[u].length;
            ptr -= l;
            memcpy(ptr, (char*)srcBuffer+dictList[u].pos, l);
        }

        /* dictionary header */
        MEM_writeLE32(dictHeader, ZSTD_DICT_MAGIC);
        hSize = 4;
        dictHeaderSize -= 4;

        /* entropic tables */
        DISPLAYLEVEL(2, "statistics ... \n");
        hSize += DiB_analyzeEntropy((char*)dictHeader+4, dictHeaderSize,
                           srcBuffer, fileSizes, nbFiles,
                           dictContent, dictContentSize);

        /* save dict */
        {
            size_t dictSize = hSize + dictContentSize;
            DISPLAYLEVEL(2, "Save dictionary of size %u into file %s \n", (U32)dictSize, dictFileName);
            DiB_saveDict(dictFileName, dictHeader, hSize, dictContent, dictContentSize);
            //DiB_saveDict(dictFileName, NULL, 0, dictContent, dictContentSize);   // content only
        }
        /* clean */
        free(dictHeader);
        free(dictContent);
    }

    /* clean up */
    free(srcBuffer);
    free(fileSizes);
    free(dictList);
    return 0;
}

