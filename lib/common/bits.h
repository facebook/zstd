/*
 * Copyright (c) Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_BITS_H
#define ZSTD_BITS_H

#include "mem.h"

MEM_STATIC unsigned ZSTD_countTrailingZeros32_fallback(U32 val)
{
    assert(val != 0);
    {
        static const int DeBruijnBytePos[32] = {0, 1, 28, 2, 29, 14, 24, 3,
                                                30, 22, 20, 15, 25, 17, 4, 8,
                                                31, 27, 13, 23, 21, 19, 16, 7,
                                                26, 12, 18, 6, 11, 5, 10, 9};
        return DeBruijnBytePos[((U32) ((val & -(S32) val) * 0x077CB531U)) >> 27];
    }
}

MEM_STATIC unsigned ZSTD_countTrailingZeros32(U32 val)
{
    assert(val != 0);
#   if defined(_MSC_VER)
#       if STATIC_BMI2 == 1
            return _tzcnt_u32(val);
#       else
            unsigned long r;
            _BitScanForward(&r, val);
            return (unsigned)r;
#       endif
#   elif defined(__GNUC__) && (__GNUC__ >= 4)
        return (unsigned)__builtin_ctz(val);
#   else
        return ZSTD_countTrailingZeros32_fallback(val);
#   endif
}

MEM_STATIC unsigned ZSTD_countLeadingZeros32_fallback(U32 val) {
    assert(val != 0);
    {
        unsigned result = 0;
        while ((val & 0x80000000) == 0) {
            result++;
            val <<= 1;
        }
        return result;
    }
}

MEM_STATIC unsigned ZSTD_countLeadingZeros32(U32 val)
{
    assert(val != 0);
#   if defined(_MSC_VER)
#       if STATIC_BMI2 == 1
            return _lzcnt_u32(val);
#       else
            unsigned long r;
            _BitScanReverse(&r, val);
            return (unsigned)(r ^ 31);
#       endif
#   elif defined(__GNUC__) && (__GNUC__ >= 4)
        return (unsigned)__builtin_clz(val);
#   else
        return ZSTD_countLeadingZeros32_fallback(val);
#   endif
}

MEM_STATIC unsigned ZSTD_countTrailingZeros64(U64 val)
{
    assert(val != 0);
#   if defined(_MSC_VER) && defined(_WIN64)
#       if STATIC_BMI2 == 1
            return _tzcnt_u64(val);
#       else
            unsigned long r;
            _BitScanForward64(&r, val);
            return (unsigned)r;
#       endif
#   elif defined(__GNUC__) && (__GNUC__ >= 4) && defined(__LP64__)
        return (unsigned)__builtin_ctzll(val);
#   else
        {
            U32 mostSignificantWord = (U32)(val >> 32);
            U32 leastSignificantWord = (U32)val;
            if (leastSignificantWord == 0) {
                return 32 + ZSTD_countTrailingZeros32(mostSignificantWord);
            } else {
                return ZSTD_countTrailingZeros32(leastSignificantWord);
            }
        }
#   endif
}

MEM_STATIC unsigned ZSTD_countLeadingZeros64(U64 val)
{
    assert(val != 0);
#   if defined(_MSC_VER) && defined(_WIN64)
#       if STATIC_BMI2 == 1
            return _lzcnt_u64(val);
#       else
            unsigned long r;
            _BitScanReverse64(&r, val);
            return (unsigned)(r ^ 63);
#       endif
#   elif defined(__GNUC__) && (__GNUC__ >= 4)
        return (unsigned)(__builtin_clzll(val));
#   else
        {
            U32 mostSignificantWord = (U32)(val >> 32);
            U32 leastSignificantWord = (U32)val;
            if (mostSignificantWord == 0) {
                return 32 + ZSTD_countLeadingZeros32(leastSignificantWord);
            } else {
                return ZSTD_countLeadingZeros32(mostSignificantWord);
            }
        }
#   endif
}

MEM_STATIC unsigned ZSTD_NbCommonBytes(size_t val)
{
    if (MEM_isLittleEndian()) {
        if (MEM_64bits()) {
            return ZSTD_countTrailingZeros64((U64)val) >> 3;
        } else {
            return ZSTD_countTrailingZeros32((U32)val) >> 3;
        }
    } else {  /* Big Endian CPU */
        if (MEM_64bits()) {
            return ZSTD_countLeadingZeros64((U64)val) >> 3;
        } else {
            return ZSTD_countLeadingZeros32((U32)val) >> 3;
        }
    }
}

MEM_STATIC unsigned ZSTD_highbit32(U32 val)   /* compress, dictBuilder, decodeCorpus */
{
    assert(val != 0);
    return ZSTD_countLeadingZeros32(val) ^ 31;
}

#endif /* ZSTD_BITS_H */
