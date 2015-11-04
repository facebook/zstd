/*
    zstd_internal - common functions to include
    Header File for include
    Copyright (C) 2014-2015, Yann Collet.

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
    - ztsd public forum : https://groups.google.com/forum/#!forum/lz4c
*/
#ifndef ZSTD_CCOMMON_H_MODULE
#define ZSTD_CCOMMON_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif

/* *************************************
*  Includes
***************************************/
#include "mem.h"
#include "error.h"


/* **************************************
*  Function body to include for inlining
****************************************/
static size_t ZSTD_read_ARCH(const void* p) { size_t r; memcpy(&r, p, sizeof(r)); return r; }

#define MIN(a,b) ((a)<(b) ? (a) : (b))

static unsigned ZSTD_highbit(U32 val)
{
#   if defined(_MSC_VER)   /* Visual */
    unsigned long r=0;
    _BitScanReverse(&r, val);
    return (unsigned)r;
#   elif defined(__GNUC__) && (__GNUC__ >= 3)   /* GCC Intrinsic */
    return 31 - __builtin_clz(val);
#   else   /* Software version */
    static const int DeBruijnClz[32] = { 0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30, 8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31 };
    U32 v = val;
    int r;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    r = DeBruijnClz[(U32)(v * 0x07C4ACDDU) >> 27];
    return r;
#   endif
}

MEM_STATIC unsigned ZSTD_NbCommonBytes (register size_t val)
{
    if (MEM_isLittleEndian())
    {
        if (MEM_64bits())
        {
#       if defined(_MSC_VER) && defined(_WIN64)
            unsigned long r = 0;
            _BitScanForward64( &r, (U64)val );
            return (int)(r>>3);
#       elif defined(__GNUC__) && (__GNUC__ >= 3)
            return (__builtin_ctzll((U64)val) >> 3);
#       else
            static const int DeBruijnBytePos[64] = { 0, 0, 0, 0, 0, 1, 1, 2, 0, 3, 1, 3, 1, 4, 2, 7, 0, 2, 3, 6, 1, 5, 3, 5, 1, 3, 4, 4, 2, 5, 6, 7, 7, 0, 1, 2, 3, 3, 4, 6, 2, 6, 5, 5, 3, 4, 5, 6, 7, 1, 2, 4, 6, 4, 4, 5, 7, 2, 6, 5, 7, 6, 7, 7 };
            return DeBruijnBytePos[((U64)((val & -(long long)val) * 0x0218A392CDABBD3FULL)) >> 58];
#       endif
        }
        else /* 32 bits */
        {
#       if defined(_MSC_VER)
            unsigned long r=0;
            _BitScanForward( &r, (U32)val );
            return (int)(r>>3);
#       elif defined(__GNUC__) && (__GNUC__ >= 3)
            return (__builtin_ctz((U32)val) >> 3);
#       else
            static const int DeBruijnBytePos[32] = { 0, 0, 3, 0, 3, 1, 3, 0, 3, 2, 2, 1, 3, 2, 0, 1, 3, 3, 1, 2, 2, 2, 2, 0, 3, 1, 2, 0, 1, 0, 1, 1 };
            return DeBruijnBytePos[((U32)((val & -(S32)val) * 0x077CB531U)) >> 27];
#       endif
        }
    }
    else   /* Big Endian CPU */
    {
        if (MEM_32bits())
        {
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
        }
        else /* 32 bits */
        {
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
        }
    }
}


MEM_STATIC size_t ZSTD_count(const BYTE* pIn, const BYTE* pMatch, const BYTE* pInLimit)
{
    const BYTE* const pStart = pIn;

    while ((pIn<pInLimit-(sizeof(size_t)-1)))
    {
        size_t diff = ZSTD_read_ARCH(pMatch) ^ ZSTD_read_ARCH(pIn);
        if (!diff) { pIn+=sizeof(size_t); pMatch+=sizeof(size_t); continue; }
        pIn += ZSTD_NbCommonBytes(diff);
        return (size_t)(pIn - pStart);
    }

    if (MEM_64bits()) if ((pIn<(pInLimit-3)) && (MEM_read32(pMatch) == MEM_read32(pIn))) { pIn+=4; pMatch+=4; }
    if ((pIn<(pInLimit-1)) && (MEM_read16(pMatch) == MEM_read16(pIn))) { pIn+=2; pMatch+=2; }
    if ((pIn<pInLimit) && (*pMatch == *pIn)) pIn++;
    return (size_t)(pIn - pStart);
}


static void ZSTD_copy8(void* dst, const void* src) { memcpy(dst, src, 8); }

#define COPY8(d,s) { ZSTD_copy8(d,s); d+=8; s+=8; }

/*! ZSTD_wildcopy : custom version of memcpy(), can copy up to 7-8 bytes too many */
static void ZSTD_wildcopy(void* dst, const void* src, size_t length)
{
    const BYTE* ip = (const BYTE*)src;
    BYTE* op = (BYTE*)dst;
    BYTE* const oend = op + length;
    do COPY8(op, ip) while (op < oend);
}


typedef enum { bt_compressed, bt_raw, bt_rle, bt_end } blockType_t;

typedef struct
{
    blockType_t blockType;
    U32 origSize;
} blockProperties_t;

size_t ZSTD_noCompressBlock(void* op, size_t maxDstSize, const void* ip, size_t blockSize);


typedef struct {
    void* buffer;
    U32*  offsetStart;
    U32*  offset;
    BYTE* offCodeStart;
    BYTE* offCode;
    BYTE* litStart;
    BYTE* lit;
    BYTE* litLengthStart;
    BYTE* litLength;
    BYTE* matchLengthStart;
    BYTE* matchLength;
    BYTE* dumpsStart;
    BYTE* dumps;
} seqStore_t;

void ZSTD_resetSeqStore(seqStore_t* ssPtr);

#define REPCODE_STARTVALUE 4
#define MLbits   7
#define LLbits   6
#define Offbits  5
#define MaxML  ((1<<MLbits) - 1)
#define MaxLL  ((1<<LLbits) - 1)
#define MaxOff   31

/** ZSTD_storeSeq
    Store a sequence (literal length, literals, offset code and match length) into seqStore_t
    @offsetCode : distance to match, or 0 == repCode
    @matchCode : matchLength - MINMATCH
*/
MEM_STATIC void ZSTD_storeSeq(seqStore_t* seqStorePtr, size_t litLength, const BYTE* literals, size_t offsetCode, size_t matchCode)
{
    /* copy Literals */
    ZSTD_wildcopy(seqStorePtr->lit, literals, litLength);
    seqStorePtr->lit += litLength;

    /* literal Length */
    if (litLength >= MaxLL)
    {
        *(seqStorePtr->litLength++) = MaxLL;
        if (litLength<255 + MaxLL)
            *(seqStorePtr->dumps++) = (BYTE)(litLength - MaxLL);
        else
        {
            *(seqStorePtr->dumps++) = 255;
            MEM_writeLE32(seqStorePtr->dumps, (U32)litLength); seqStorePtr->dumps += 3;
        }
    }
    else *(seqStorePtr->litLength++) = (BYTE)litLength;

    /* match offset */
    *(seqStorePtr->offset++) = (U32)offsetCode;

    /* match Length */
    if (matchCode >= MaxML)
    {
        *(seqStorePtr->matchLength++) = MaxML;
        if (matchCode < 255+MaxML)
            *(seqStorePtr->dumps++) = (BYTE)(matchCode - MaxML);
        else
        {
            *(seqStorePtr->dumps++) = 255;
            MEM_writeLE32(seqStorePtr->dumps, (U32)matchCode); seqStorePtr->dumps += 3;
        }
    }
    else *(seqStorePtr->matchLength++) = (BYTE)matchCode;
}


/* prototype, body into zstd.c */
size_t ZSTD_compressSequences(BYTE* dst, size_t maxDstSize, const seqStore_t* seqStorePtr, size_t srcSize);


#if defined (__cplusplus)
}
#endif

#endif   /* ZSTD_CCOMMON_H_MODULE */
