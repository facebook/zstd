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

MEM_STATIC unsigned ZSTD_highbit32_fallback(U32 val) {
    static const U32 DeBruijnClz[32] = {  0,  9,  1, 10, 13, 21,  2, 29,
                                         11, 14, 16, 18, 22, 25,  3, 30,
                                          8, 12, 20, 28, 15, 17, 24,  7,
                                         19, 27, 23,  6, 26,  5,  4, 31 };
    val |= val >> 1;
    val |= val >> 2;
    val |= val >> 4;
    val |= val >> 8;
    val |= val >> 16;
    return DeBruijnClz[(val * 0x07C4ACDDU) >> 27];
}

MEM_STATIC unsigned ZSTD_highbit32(U32 val)   /* compress, dictBuilder, decodeCorpus */
{
    assert(val != 0);
    {
#   if defined(_MSC_VER)   /* Visual */
#       if STATIC_BMI2 == 1
            return _lzcnt_u32(val)^31;
#       else
            if (val != 0) {
                unsigned long r;
                _BitScanReverse(&r, val);
                return (unsigned)r;
            } else {
                /* Should not reach this code path */
                __assume(0);
            }
#       endif
#   elif defined(__GNUC__) && (__GNUC__ >= 3)   /* GCC Intrinsic */
        return (unsigned)__builtin_clz (val) ^ 31;
#   elif defined(__ICCARM__)    /* IAR Intrinsic */
        return 31 - __CLZ(val);
#   else   /* Software version */
        return ZSTD_highbit32_fallback(val);
#   endif
    }
}

MEM_STATIC unsigned ZSTD_countTrailingZeros32_fallback(U32 val)
{
    static const int DeBruijnBytePos[32] = {  0,  1, 28,  2, 29, 14, 24,  3,
                                             30, 22, 20, 15, 25, 17,  4,  8,
                                             31, 27, 13, 23, 21, 19, 16,  7,
                                             26, 12, 18,  6, 11,  5, 10,  9 };
    return DeBruijnBytePos[((U32)((val & -(S32)val) * 0x077CB531U)) >> 27];
}

MEM_STATIC unsigned ZSTD_countTrailingZeros32(U32 val)
{
    assert(val != 0);
#   if defined(_MSC_VER)
        if (val != 0) {
            unsigned long r;
            _BitScanForward(&r, val);
            return (unsigned)r;
        } else {
            /* Should not reach this code path */
            __assume(0);
        }
#   elif defined(__GNUC__) && (__GNUC__ >= 3)
        return (unsigned)__builtin_ctz(val);
#   elif defined(__ICCARM__)    /* IAR Intrinsic */
        return __CTZ(val);
#   else
        return ZSTD_countTrailingZeros32_fallback(val);
#   endif
}

MEM_STATIC unsigned ZSTD_countTrailingZeros64_fallback(U64 val)
{
    static const int DeBruijnBytePos[64] = {  0,  1,  2,  7,  3, 13,  8, 19,
                                              4, 25, 14, 28,  9, 34, 20, 56,
                                              5, 17, 26, 54, 15, 41, 29, 43,
                                             10, 31, 38, 35, 21, 45, 49, 57,
                                             63,  6, 12, 18, 24, 27, 33, 55,
                                             16, 53, 40, 42, 30, 37, 44, 48,
                                             62, 11, 23, 32, 52, 39, 36, 47,
                                             61, 22, 51, 46, 60, 50, 59, 58 };
    return DeBruijnBytePos[((U64)((val & -(long long)val) * 0x0218A392CDABBD3FULL)) >> 58];
}

MEM_STATIC unsigned ZSTD_countTrailingZeros64(U64 val)
{
    assert(val != 0);
#   if defined(_MSC_VER) && defined(_WIN64)
#       if STATIC_BMI2
            return _tzcnt_u64(val);
#       else
            if (val != 0) {
                unsigned long r;
                _BitScanForward64(&r, val);
                return (unsigned)r;
            } else {
                /* Should not reach this code path */
                __assume(0);
            }
#       endif
#   elif defined(__GNUC__) && (__GNUC__ >= 4)
        if (MEM_32bits()) {
            U32 mostSignificantWord = (U32)(val >> 32);
            U32 leastSignificantWord = (U32)val;
            if (leastSignificantWord == 0) {
                return 32 + (unsigned)__builtin_ctz(mostSignificantWord);
            } else {
                return (unsigned)__builtin_ctz(leastSignificantWord);
            }
        } else {
            return (unsigned)__builtin_ctzll(val);
        }
#   else
        return ZSTD_countTrailingZeros64_fallback(val);
#   endif
}

MEM_STATIC unsigned ZSTD_NbCommonBytes_fallback(size_t val)
{
    if (MEM_isLittleEndian()) {
        if (MEM_64bits()) {
            static const int DeBruijnBytePos[64] = { 0, 0, 0, 0, 0, 1, 1, 2,
                                                     0, 3, 1, 3, 1, 4, 2, 7,
                                                     0, 2, 3, 6, 1, 5, 3, 5,
                                                     1, 3, 4, 4, 2, 5, 6, 7,
                                                     7, 0, 1, 2, 3, 3, 4, 6,
                                                     2, 6, 5, 5, 3, 4, 5, 6,
                                                     7, 1, 2, 4, 6, 4, 4, 5,
                                                     7, 2, 6, 5, 7, 6, 7, 7 };
            return DeBruijnBytePos[((U64)((val & -(long long)val) * 0x0218A392CDABBD3FULL)) >> 58];
        } else { /* 32 bits */
            static const int DeBruijnBytePos[32] = { 0, 0, 3, 0, 3, 1, 3, 0,
                                                     3, 2, 2, 1, 3, 2, 0, 1,
                                                     3, 3, 1, 2, 2, 2, 2, 0,
                                                     3, 1, 2, 0, 1, 0, 1, 1 };
            return DeBruijnBytePos[((U32)((val & -(S32)val) * 0x077CB531U)) >> 27];
        }
    } else {  /* Big Endian CPU */
        unsigned r;
        if (MEM_64bits()) {
            const unsigned n32 = sizeof(size_t)*4;   /* calculate this way due to compiler complaining in 32-bits mode */
            if (!(val>>n32)) { r=4; } else { r=0; val>>=n32; }
            if (!(val>>16)) { r+=2; val>>=8; } else { val>>=24; }
            r += (!val);
            return r;
        } else { /* 32 bits */
            if (!(val>>16)) { r=2; val>>=8; } else { r=0; val>>=24; }
            r += (!val);
            return r;
        }
    }
}

MEM_STATIC unsigned ZSTD_NbCommonBytes(size_t val)
{
    if (MEM_isLittleEndian()) {
        if (MEM_64bits()) {
#           if defined(_MSC_VER) && defined(_WIN64)
#               if STATIC_BMI2
                    return _tzcnt_u64(val) >> 3;
#               else
                    if (val != 0) {
                        unsigned long r;
                        _BitScanForward64(&r, (U64)val);
                        return (unsigned)(r >> 3);
                    } else {
                        /* Should not reach this code path */
                        __assume(0);
                    }
#               endif
#           elif defined(__GNUC__) && (__GNUC__ >= 4)
                return (unsigned)(__builtin_ctzll((U64)val) >> 3);
#           else
                return ZSTD_NbCommonBytes_fallback(val);
#           endif
        } else { /* 32 bits */
#           if defined(_MSC_VER)
                if (val != 0) {
                    unsigned long r;
                    _BitScanForward(&r, (U32)val);
                    return (unsigned)(r >> 3);
                } else {
                    /* Should not reach this code path */
                    __assume(0);
                }
#           elif defined(__GNUC__) && (__GNUC__ >= 3)
                return (unsigned)(__builtin_ctz((U32)val) >> 3);
#           else
                return ZSTD_NbCommonBytes_fallback(val);
#           endif
        }
    } else {  /* Big Endian CPU */
        if (MEM_64bits()) {
#           if defined(_MSC_VER) && defined(_WIN64)
#               if STATIC_BMI2
                    return _lzcnt_u64(val) >> 3;
#               else
                    if (val != 0) {
                        unsigned long r;
                        _BitScanReverse64(&r, (U64)val);
                        return (unsigned)(r >> 3);
                    } else {
                        /* Should not reach this code path */
                        __assume(0);
                    }
#               endif
#           elif defined(__GNUC__) && (__GNUC__ >= 4)
                return (unsigned)(__builtin_clzll(val) >> 3);
#           else
                return ZSTD_NbCommonBytes_fallback(val);
#           endif
        } else { /* 32 bits */
#           if defined(_MSC_VER)
                if (val != 0) {
                    unsigned long r;
                    _BitScanReverse(&r, (unsigned long)val);
                    return (unsigned)(r >> 3);
                } else {
                    /* Should not reach this code path */
                    __assume(0);
                }
#           elif defined(__GNUC__) && (__GNUC__ >= 3)
                return (unsigned)(__builtin_clz((U32)val) >> 3);
#           else
                return ZSTD_NbCommonBytes_fallback(val);
#           endif
    }   }
}

#endif /* ZSTD_BITS_H */
