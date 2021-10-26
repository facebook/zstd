/* ******************************************************************
 * bitstream
 * Part of FSE library
 * Copyright (c) Yann Collet, Facebook, Inc.
 *
 * You can contact the author at :
 * - Source repository : https://github.com/Cyan4973/FiniteStateEntropy
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
****************************************************************** */
#ifndef BITSTREAM_H_MODULE
#define BITSTREAM_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif
/*
*  This API consists of small unitary functions, which must be inlined for best performance.
*  Since link-time-optimization is not available for all compilers,
*  these functions are defined into a .h to be included.
*/

/*-****************************************
*  Dependencies
******************************************/
#include "mem.h"            /* unaligned access routines */
#include "compiler.h"       /* UNLIKELY() */
#include "debug.h"          /* assert(), DEBUGLOG(), RAWLOG() */
#include "error_private.h"  /* error codes and messages */


/*=========================================
*  Target specific
=========================================*/
#ifndef ZSTD_NO_INTRINSICS
#  if defined(__BMI__) && defined(__GNUC__)
#    include <immintrin.h>   /* support for bextr (experimental) */
#  elif defined(__ICCARM__)
#    include <intrinsics.h>
#  endif
#endif

#define STREAM_ACCUMULATOR_MIN_32  25
#define STREAM_ACCUMULATOR_MIN_64  57
#define STREAM_ACCUMULATOR_MIN    ((U32)(MEM_32bits() ? STREAM_ACCUMULATOR_MIN_32 : STREAM_ACCUMULATOR_MIN_64))


/*-******************************************
*  bitStream encoding API (write forward)
********************************************/
/* bitStream can mix input from multiple sources.
 * A critical property of these streams is that they encode and decode in **reverse** direction.
 * So the first bit sequence you add will be the last to be read, like a LIFO stack.
 */
typedef struct {
    size_t bitContainer;
    unsigned bitPos;
    char*  startPtr;
    char*  ptr;
    char*  endPtr;
} BIT_CStream_t;

MEM_STATIC size_t BIT_initCStream(BIT_CStream_t* bitC, void* dstBuffer, size_t dstCapacity);
MEM_STATIC void   BIT_addBits(BIT_CStream_t* bitC, size_t value, unsigned nbBits);
MEM_STATIC void   BIT_flushBits(BIT_CStream_t* bitC);
MEM_STATIC size_t BIT_closeCStream(BIT_CStream_t* bitC);

/* Start with initCStream, providing the size of buffer to write into.
*  bitStream will never write outside of this buffer.
*  `dstCapacity` must be >= sizeof(bitD->bitContainer), otherwise @return will be an error code.
*
*  bits are first added to a local register.
*  Local register is size_t, hence 64-bits on 64-bits systems, or 32-bits on 32-bits systems.
*  Writing data into memory is an explicit operation, performed by the flushBits function.
*  Hence keep track how many bits are potentially stored into local register to avoid register overflow.
*  After a flushBits, a maximum of 7 bits might still be stored into local register.
*
*  Avoid storing elements of more than 24 bits if you want compatibility with 32-bits bitstream readers.
*
*  Last operation is to close the bitStream.
*  The function returns the final size of CStream in bytes.
*  If data couldn't fit into `dstBuffer`, it will return a 0 ( == not storable)
*/


/*-********************************************
*  bitStream decoding API (read backward)
**********************************************/
typedef struct {
    size_t   bitContainer;
    unsigned bitsConsumed;
    const char* ptr;
    const char* start;
    const char* limitPtr;
} BIT_DStream_t;

typedef enum { BIT_DStream_unfinished = 0,
               BIT_DStream_endOfBuffer = 1,
               BIT_DStream_completed = 2,
               BIT_DStream_overflow = 3 } BIT_DStream_status;  /* result of BIT_reloadDStream() */
               /* 1,2,4,8 would be better for bitmap combinations, but slows down performance a bit ... :( */

MEM_STATIC size_t   BIT_initDStream(BIT_DStream_t* bitD, const void* srcBuffer, size_t srcSize);
MEM_STATIC size_t   BIT_readBits(BIT_DStream_t* bitD, unsigned nbBits);
MEM_STATIC BIT_DStream_status BIT_reloadDStream(BIT_DStream_t* bitD);
MEM_STATIC unsigned BIT_endOfDStream(const BIT_DStream_t* bitD);


/* Start by invoking BIT_initDStream().
*  A chunk of the bitStream is then stored into a local register.
*  Local register size is 64-bits on 64-bits systems, 32-bits on 32-bits systems (size_t).
*  You can then retrieve bitFields stored into the local register, **in reverse order**.
*  Local register is explicitly reloaded from memory by the BIT_reloadDStream() method.
*  A reload guarantee a minimum of ((8*sizeof(bitD->bitContainer))-7) bits when its result is BIT_DStream_unfinished.
*  Otherwise, it can be less than that, so proceed accordingly.
*  Checking if DStream has reached its end can be performed with BIT_endOfDStream().
*/


/*-****************************************
*  unsafe API
******************************************/
MEM_STATIC void BIT_addBitsFast(BIT_CStream_t* bitC, size_t value, unsigned nbBits);
/* faster, but works only if value is "clean", meaning all high bits above nbBits are 0 */

MEM_STATIC void BIT_flushBitsFast(BIT_CStream_t* bitC);
/* unsafe version; does not check buffer overflow */

MEM_STATIC size_t BIT_readBitsFast(BIT_DStream_t* bitD, unsigned nbBits);
/* faster, but works only if nbBits >= 1 */



/*-**************************************************************
*  Internal functions
****************************************************************/

/* Counts the number of trailing zeros of `val`, from the least significant
   bit (LSB) to the most significant bit (MSB).
   If `val` is 0, the result is undefined. */
MEM_STATIC U32 BIT_ctz32(U32 val)
{
#if defined(_MSC_VER) && STATIC_BMI2 == 1
    /* unsigned int _tzcnt_u32 (unsigned int a) */
    return _tzcnt_u32(val);
#elif defined(_MSC_VER)
    {   /* unsigned char _BitScanForward(
                unsigned long * Index,
                unsigned long Mask) */
        unsigned long r;
        _BitScanForward(&r, val);
        return (U32) r;
    }
#elif defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))
    /* int __builtin_ctz (unsigned int x) */
    return (U32) __builtin_ctz(val);
#else
    #define BIT_CTZ32_SOFTWARE

    static const BYTE DeBruijnBytePos[32] =
        { 0,  1, 28,  2, 29, 14, 24,  3,
         30, 22, 20, 15, 25, 17,  4,  8,
         31, 27, 13, 23, 21, 19, 16,  7,
         26, 12, 18,  6, 11,  5, 10,  9};
    return (U32) DeBruijnBytePos[((U32)((val & -(S32)val) * 0x077CB531U)) >> 27];
#endif
}

/* Counts the number of trailing zeros of `val`, from the least significant
   bit (LSB) to the most significant bit (MSB).
   If `val` is 0, the result is undefined. */
MEM_STATIC U64 BIT_ctz64(U64 val)
{
    if (MEM_32bits()) {
        assert(0);  /* Only use in 64-bit build */
    }

#if defined(_MSC_VER) && defined(_WIN64) && STATIC_BMI2 == 1
    /* unsigned __int64 _tzcnt_u64 (unsigned __int64 a) */
    return _tzcnt_u64(val);
#elif defined(_MSC_VER) && defined(_WIN64)
    {   /* unsigned char _BitScanForward64(
                unsigned long * Index,
                unsigned __int64 Mask) */
        unsigned long r;
        _BitScanForward64(&r, val);
        return (U64) r;
    }
#elif defined(__GNUC__) && defined(__x86_64__)
    {   /* Remove the S32->U64 cast */
        U64 ret;
        __asm__ ("rep bsfq %1, %0"
                 : "=r" (ret)  /* = means overwriting an existing value */
                 : "rm" (val)  /* register or memory */
                 : "cc");
        return ret;
    }
#elif defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))
    /* int __builtin_ctzll (unsigned long long) */
    return (U64) __builtin_ctzll(val);
#else
    #define BIT_CTZ64_SOFTWARE

    static const BYTE DeBruijnBytePos[64] =
        { 0,  1,  2,  7,  3, 13,  8, 19,
          4, 25, 14, 28,  9, 34, 20, 56,
          5, 17, 26, 54, 15, 41, 29, 43,
         10, 31, 38, 35, 21, 45, 49, 57,
         63,  6, 12, 18, 24, 27, 33, 55,
         16, 53, 40, 42, 30, 37, 44, 48,
         62, 11, 23, 32, 52, 39, 36, 47,
         61, 22, 51, 46, 60, 50, 59, 58};
    return (U64) DeBruijnBytePos[((U64)((val & -(S64)val) * 0x0218A392CDABBD3FULL)) >> 58];
#endif
}

/* Counts the number of leading zeros of `val`, from the most significant
   bit (MSB) to the least significant bit (LSB).
   To get the index of the first set bit (1), use `BIT_clz32(v) ^ 31`.
   If `val` is 0, the result is undefined. */
MEM_STATIC U32 BIT_clz32(U32 val)
{
#if defined(_MSC_VER) && STATIC_BMI2 == 1
    /* unsigned int _lzcnt_u32 (unsigned int a) */
    return _lzcnt_u32(val);
#elif defined(_MSC_VER)
    {   /* unsigned char _BitScanReverse(
                unsigned long * Index,
                unsigned long Mask) */
        unsigned long r;
        _BitScanReverse(&r, val);
        return (U32) (r ^ 31);
    }
#elif defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))
    /* int __builtin_clz (unsigned int x) */
    return (U32) __builtin_clz(val);
#elif defined(__ICCARM__)    /* IAR Intrinsic */
    /* unsigned int __CLZ(unsigned int) */
    return __CLZ(val);
#else
    #define BIT_CLZ32_SOFTWARE

    static const BYTE DeBruijnClz[32] =
        {31, 22, 30, 21, 18, 10, 29,  2,
         20, 17, 15, 13,  9,  6, 28,  1,
         23, 19, 11,  3, 16, 14,  7, 24,
         12,  4,  8, 25,  5, 26, 27, 0};
    U32 v = val;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return DeBruijnClz[(U32)(v*0x07C4ACDDU) >> 27];
#endif
}

/* Counts the number of leading zeros of `val`, from the most significant
   bit (MSB) to the least significant bit (LSB).
   To get the index of the first set bit (1), use `BIT_clz64(v) ^ 63`.
   If `val` is 0, the result is undefined. */
MEM_STATIC U64 BIT_clz64(U64 val)
{
    if (MEM_32bits()) {
        assert(0);  /* Only use in 64-bit build */
    }

#if defined(_MSC_VER) && defined(_WIN64) && STATIC_BMI2 == 1
    /* unsigned __int64 _lzcnt_u64 (unsigned __int64 a) */
    return _lzcnt_u64(val);
#elif defined(_MSC_VER) && defined(_WIN64)
    {
        /* unsigned char _BitScanReverse64(
                unsigned long * Index,
                unsigned __int64 Mask) */
        unsigned long r;
        _BitScanReverse64(&r, val);
        return ((U64)r) ^ 63;
    }
#elif defined(__GNUC__) && defined(__x86_64__)
    {   /* Remove the S32->U64 cast */
        U64 ret;
        __asm__ ("bsrq %1, %0"
                 : "=r" (ret)  /* = means overwriting an existing value */
                 : "rm" (val)  /* register or memory */
                 : "cc");
        return ret ^ 63;
    }
#elif defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))
    /* int __builtin_clzll (unsigned long long) */
    return (U64) __builtin_clzll(val);
#else
    #define BIT_CLZ64_SOFTWARE

    U32 mostSignificantWord = (U32)(val >> 32);
    U32 leastSignificantWord = (U32)val;
    if (mostSignificantWord == 0) {
        return 32 + BIT_clz32(leastSignificantWord);
    } else {
        return BIT_clz32(mostSignificantWord);
    }
#endif
}

MEM_STATIC U32 BIT_highbit32(U32 val)
{
    assert(val != 0);

#   ifndef BIT_CLZ32_SOFTWARE
        return BIT_clz32(val) ^ 31;
#   else
        {
            static const BYTE DeBruijnMSB[32] =
                { 0,  9,  1, 10, 13, 21,  2, 29,
                 11, 14, 16, 18, 22, 25,  3, 30,
                  8, 12, 20, 28, 15, 17, 24,  7,
                 19, 27, 23,  6, 26,  5,  4, 31};
            U32 v = val;
            v |= v >> 1;
            v |= v >> 2;
            v |= v >> 4;
            v |= v >> 8;
            v |= v >> 16;
            return DeBruijnMSB[(U32)(v*0x07C4ACDDU) >> 27];
        }
#   endif
}

/*=====    Local Constants   =====*/
static const unsigned BIT_mask[] = {
    0,          1,         3,         7,         0xF,       0x1F,
    0x3F,       0x7F,      0xFF,      0x1FF,     0x3FF,     0x7FF,
    0xFFF,      0x1FFF,    0x3FFF,    0x7FFF,    0xFFFF,    0x1FFFF,
    0x3FFFF,    0x7FFFF,   0xFFFFF,   0x1FFFFF,  0x3FFFFF,  0x7FFFFF,
    0xFFFFFF,   0x1FFFFFF, 0x3FFFFFF, 0x7FFFFFF, 0xFFFFFFF, 0x1FFFFFFF,
    0x3FFFFFFF, 0x7FFFFFFF}; /* up to 31 bits */
#define BIT_MASK_SIZE (sizeof(BIT_mask) / sizeof(BIT_mask[0]))

/*-**************************************************************
*  bitStream encoding
****************************************************************/
/*! BIT_initCStream() :
 *  `dstCapacity` must be > sizeof(size_t)
 *  @return : 0 if success,
 *            otherwise an error code (can be tested using ERR_isError()) */
MEM_STATIC size_t BIT_initCStream(BIT_CStream_t* bitC,
                                  void* startPtr, size_t dstCapacity)
{
    bitC->bitContainer = 0;
    bitC->bitPos = 0;
    bitC->startPtr = (char*)startPtr;
    bitC->ptr = bitC->startPtr;
    bitC->endPtr = bitC->startPtr + dstCapacity - sizeof(bitC->bitContainer);
    if (dstCapacity <= sizeof(bitC->bitContainer)) return ERROR(dstSize_tooSmall);
    return 0;
}

/*! BIT_addBits() :
 *  can add up to 31 bits into `bitC`.
 *  Note : does not check for register overflow ! */
MEM_STATIC void BIT_addBits(BIT_CStream_t* bitC,
                            size_t value, unsigned nbBits)
{
    DEBUG_STATIC_ASSERT(BIT_MASK_SIZE == 32);
    assert(nbBits < BIT_MASK_SIZE);
    assert(nbBits + bitC->bitPos < sizeof(bitC->bitContainer) * 8);
    bitC->bitContainer |= (value & BIT_mask[nbBits]) << bitC->bitPos;
    bitC->bitPos += nbBits;
}

/*! BIT_addBitsFast() :
 *  works only if `value` is _clean_,
 *  meaning all high bits above nbBits are 0 */
MEM_STATIC void BIT_addBitsFast(BIT_CStream_t* bitC,
                                size_t value, unsigned nbBits)
{
    assert((value>>nbBits) == 0);
    assert(nbBits + bitC->bitPos < sizeof(bitC->bitContainer) * 8);
    bitC->bitContainer |= value << bitC->bitPos;
    bitC->bitPos += nbBits;
}

/*! BIT_flushBitsFast() :
 *  assumption : bitContainer has not overflowed
 *  unsafe version; does not check buffer overflow */
MEM_STATIC void BIT_flushBitsFast(BIT_CStream_t* bitC)
{
    size_t const nbBytes = bitC->bitPos >> 3;
    assert(bitC->bitPos < sizeof(bitC->bitContainer) * 8);
    assert(bitC->ptr <= bitC->endPtr);
    MEM_writeLEST(bitC->ptr, bitC->bitContainer);
    bitC->ptr += nbBytes;
    bitC->bitPos &= 7;
    bitC->bitContainer >>= nbBytes*8;
}

/*! BIT_flushBits() :
 *  assumption : bitContainer has not overflowed
 *  safe version; check for buffer overflow, and prevents it.
 *  note : does not signal buffer overflow.
 *  overflow will be revealed later on using BIT_closeCStream() */
MEM_STATIC void BIT_flushBits(BIT_CStream_t* bitC)
{
    size_t const nbBytes = bitC->bitPos >> 3;
    assert(bitC->bitPos < sizeof(bitC->bitContainer) * 8);
    assert(bitC->ptr <= bitC->endPtr);
    MEM_writeLEST(bitC->ptr, bitC->bitContainer);
    bitC->ptr += nbBytes;
    if (bitC->ptr > bitC->endPtr) bitC->ptr = bitC->endPtr;
    bitC->bitPos &= 7;
    bitC->bitContainer >>= nbBytes*8;
}

/*! BIT_closeCStream() :
 *  @return : size of CStream, in bytes,
 *            or 0 if it could not fit into dstBuffer */
MEM_STATIC size_t BIT_closeCStream(BIT_CStream_t* bitC)
{
    BIT_addBitsFast(bitC, 1, 1);   /* endMark */
    BIT_flushBits(bitC);
    if (bitC->ptr >= bitC->endPtr) return 0; /* overflow detected */
    return (bitC->ptr - bitC->startPtr) + (bitC->bitPos > 0);
}


/*-********************************************************
*  bitStream decoding
**********************************************************/
/*! BIT_initDStream() :
 *  Initialize a BIT_DStream_t.
 * `bitD` : a pointer to an already allocated BIT_DStream_t structure.
 * `srcSize` must be the *exact* size of the bitStream, in bytes.
 * @return : size of stream (== srcSize), or an errorCode if a problem is detected
 */
MEM_STATIC size_t BIT_initDStream(BIT_DStream_t* bitD, const void* srcBuffer, size_t srcSize)
{
    if (srcSize < 1) { ZSTD_memset(bitD, 0, sizeof(*bitD)); return ERROR(srcSize_wrong); }

    bitD->start = (const char*)srcBuffer;
    bitD->limitPtr = bitD->start + sizeof(bitD->bitContainer);

    if (srcSize >=  sizeof(bitD->bitContainer)) {  /* normal case */
        bitD->ptr   = (const char*)srcBuffer + srcSize - sizeof(bitD->bitContainer);
        bitD->bitContainer = MEM_readLEST(bitD->ptr);
        { BYTE const lastByte = ((const BYTE*)srcBuffer)[srcSize-1];
          bitD->bitsConsumed = lastByte ? 8 - BIT_highbit32(lastByte) : 0;  /* ensures bitsConsumed is always set */
          if (lastByte == 0) return ERROR(GENERIC); /* endMark not present */ }
    } else {
        bitD->ptr   = bitD->start;
        bitD->bitContainer = *(const BYTE*)(bitD->start);
        switch(srcSize)
        {
        case 7: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[6]) << (sizeof(bitD->bitContainer)*8 - 16);
                ZSTD_FALLTHROUGH;

        case 6: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[5]) << (sizeof(bitD->bitContainer)*8 - 24);
                ZSTD_FALLTHROUGH;

        case 5: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[4]) << (sizeof(bitD->bitContainer)*8 - 32);
                ZSTD_FALLTHROUGH;

        case 4: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[3]) << 24;
                ZSTD_FALLTHROUGH;

        case 3: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[2]) << 16;
                ZSTD_FALLTHROUGH;

        case 2: bitD->bitContainer += (size_t)(((const BYTE*)(srcBuffer))[1]) <<  8;
                ZSTD_FALLTHROUGH;

        default: break;
        }
        {   BYTE const lastByte = ((const BYTE*)srcBuffer)[srcSize-1];
            bitD->bitsConsumed = lastByte ? 8 - BIT_highbit32(lastByte) : 0;
            if (lastByte == 0) return ERROR(corruption_detected);  /* endMark not present */
        }
        bitD->bitsConsumed += (U32)(sizeof(bitD->bitContainer) - srcSize)*8;
    }

    return srcSize;
}

MEM_STATIC FORCE_INLINE_ATTR size_t BIT_getUpperBits(size_t bitContainer, U32 const start)
{
    return bitContainer >> start;
}

MEM_STATIC FORCE_INLINE_ATTR size_t BIT_getMiddleBits(size_t bitContainer, U32 const start, U32 const nbBits)
{
    U32 const regMask = sizeof(bitContainer)*8 - 1;
    /* if start > regMask, bitstream is corrupted, and result is undefined */
    assert(nbBits < BIT_MASK_SIZE);
    /* x86 transform & ((1 << nbBits) - 1) to bzhi instruction, it is better
     * than accessing memory. When bmi2 instruction is not present, we consider
     * such cpus old (pre-Haswell, 2013) and their performance is not of that
     * importance.
     */
#if defined(__x86_64__) || defined(_M_X86)
    return (bitContainer >> (start & regMask)) & ((((U64)1) << nbBits) - 1);
#else
    return (bitContainer >> (start & regMask)) & BIT_mask[nbBits];
#endif
}

MEM_STATIC FORCE_INLINE_ATTR size_t BIT_getLowerBits(size_t bitContainer, U32 const nbBits)
{
#if defined(STATIC_BMI2) && STATIC_BMI2 == 1
	return  _bzhi_u64(bitContainer, nbBits);
#else
    assert(nbBits < BIT_MASK_SIZE);
    return bitContainer & BIT_mask[nbBits];
#endif
}

/*! BIT_lookBits() :
 *  Provides next n bits from local register.
 *  local register is not modified.
 *  On 32-bits, maxNbBits==24.
 *  On 64-bits, maxNbBits==56.
 * @return : value extracted */
MEM_STATIC  FORCE_INLINE_ATTR size_t BIT_lookBits(const BIT_DStream_t*  bitD, U32 nbBits)
{
    /* arbitrate between double-shift and shift+mask */
#if 1
    /* if bitD->bitsConsumed + nbBits > sizeof(bitD->bitContainer)*8,
     * bitstream is likely corrupted, and result is undefined */
    return BIT_getMiddleBits(bitD->bitContainer, (sizeof(bitD->bitContainer)*8) - bitD->bitsConsumed - nbBits, nbBits);
#else
    /* this code path is slower on my os-x laptop */
    U32 const regMask = sizeof(bitD->bitContainer)*8 - 1;
    return ((bitD->bitContainer << (bitD->bitsConsumed & regMask)) >> 1) >> ((regMask-nbBits) & regMask);
#endif
}

/*! BIT_lookBitsFast() :
 *  unsafe version; only works if nbBits >= 1 */
MEM_STATIC size_t BIT_lookBitsFast(const BIT_DStream_t* bitD, U32 nbBits)
{
    U32 const regMask = sizeof(bitD->bitContainer)*8 - 1;
    assert(nbBits >= 1);
    return (bitD->bitContainer << (bitD->bitsConsumed & regMask)) >> (((regMask+1)-nbBits) & regMask);
}

MEM_STATIC FORCE_INLINE_ATTR void BIT_skipBits(BIT_DStream_t* bitD, U32 nbBits)
{
    bitD->bitsConsumed += nbBits;
}

/*! BIT_readBits() :
 *  Read (consume) next n bits from local register and update.
 *  Pay attention to not read more than nbBits contained into local register.
 * @return : extracted value. */
MEM_STATIC FORCE_INLINE_ATTR size_t BIT_readBits(BIT_DStream_t* bitD, unsigned nbBits)
{
    size_t const value = BIT_lookBits(bitD, nbBits);
    BIT_skipBits(bitD, nbBits);
    return value;
}

/*! BIT_readBitsFast() :
 *  unsafe version; only works only if nbBits >= 1 */
MEM_STATIC size_t BIT_readBitsFast(BIT_DStream_t* bitD, unsigned nbBits)
{
    size_t const value = BIT_lookBitsFast(bitD, nbBits);
    assert(nbBits >= 1);
    BIT_skipBits(bitD, nbBits);
    return value;
}

/*! BIT_reloadDStreamFast() :
 *  Similar to BIT_reloadDStream(), but with two differences:
 *  1. bitsConsumed <= sizeof(bitD->bitContainer)*8 must hold!
 *  2. Returns BIT_DStream_overflow when bitD->ptr < bitD->limitPtr, at this
 *     point you must use BIT_reloadDStream() to reload.
 */
MEM_STATIC BIT_DStream_status BIT_reloadDStreamFast(BIT_DStream_t* bitD)
{
    if (UNLIKELY(bitD->ptr < bitD->limitPtr))
        return BIT_DStream_overflow;
    assert(bitD->bitsConsumed <= sizeof(bitD->bitContainer)*8);
    bitD->ptr -= bitD->bitsConsumed >> 3;
    bitD->bitsConsumed &= 7;
    bitD->bitContainer = MEM_readLEST(bitD->ptr);
    return BIT_DStream_unfinished;
}

/*! BIT_reloadDStream() :
 *  Refill `bitD` from buffer previously set in BIT_initDStream() .
 *  This function is safe, it guarantees it will not read beyond src buffer.
 * @return : status of `BIT_DStream_t` internal register.
 *           when status == BIT_DStream_unfinished, internal register is filled with at least 25 or 57 bits */
MEM_STATIC BIT_DStream_status BIT_reloadDStream(BIT_DStream_t* bitD)
{
    if (bitD->bitsConsumed > (sizeof(bitD->bitContainer)*8))  /* overflow detected, like end of stream */
        return BIT_DStream_overflow;

    if (bitD->ptr >= bitD->limitPtr) {
        return BIT_reloadDStreamFast(bitD);
    }
    if (bitD->ptr == bitD->start) {
        if (bitD->bitsConsumed < sizeof(bitD->bitContainer)*8) return BIT_DStream_endOfBuffer;
        return BIT_DStream_completed;
    }
    /* start < ptr < limitPtr */
    {   U32 nbBytes = bitD->bitsConsumed >> 3;
        BIT_DStream_status result = BIT_DStream_unfinished;
        if (bitD->ptr - nbBytes < bitD->start) {
            nbBytes = (U32)(bitD->ptr - bitD->start);  /* ptr > start */
            result = BIT_DStream_endOfBuffer;
        }
        bitD->ptr -= nbBytes;
        bitD->bitsConsumed -= nbBytes*8;
        bitD->bitContainer = MEM_readLEST(bitD->ptr);   /* reminder : srcSize > sizeof(bitD->bitContainer), otherwise bitD->ptr == bitD->start */
        return result;
    }
}

/*! BIT_endOfDStream() :
 * @return : 1 if DStream has _exactly_ reached its end (all bits consumed).
 */
MEM_STATIC unsigned BIT_endOfDStream(const BIT_DStream_t* DStream)
{
    return ((DStream->ptr == DStream->start) && (DStream->bitsConsumed == sizeof(DStream->bitContainer)*8));
}

#if defined (__cplusplus)
}
#endif

#endif /* BITSTREAM_H_MODULE */
