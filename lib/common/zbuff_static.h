/*
    zstd - buffered version of compression library
    experimental complementary API, for static linking only
    Copyright (C) 2015-2016, Yann Collet.

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
#ifndef ZSTD_BUFFERED_STATIC_H
#define ZSTD_BUFFERED_STATIC_H

/* The objects defined into this file must be considered experimental.
 * Their prototype may change in future versions.
 * Never use them with a dynamic library.
 */

#if defined (__cplusplus)
extern "C" {
#endif

/* *************************************
*  Includes
***************************************/
#include "zstd_static.h"     /* ZSTD_parameters */
#include "zbuff.h"

#define ZBUFF_MIN(a,b) ((a)<(b) ? (a) : (b))


/*-*************************************
*  Advanced functions
***************************************/
/*! ZBUFF_createCCtx_advanced() :
 *  Create a ZBUFF compression context using external alloc and free functions */
ZSTDLIB_API ZBUFF_CCtx* ZBUFF_createCCtx_advanced(ZSTD_customMem customMem);

/*! ZBUFF_createDCtx_advanced() :
 *  Create a ZBUFF decompression context using external alloc and free functions */
ZSTDLIB_API ZBUFF_DCtx* ZBUFF_createDCtx_advanced(ZSTD_customMem customMem);


/* *************************************
*  Advanced Streaming functions
***************************************/
ZSTDLIB_API size_t ZBUFF_compressInit_advanced(ZBUFF_CCtx* zbc,
                                               const void* dict, size_t dictSize,
                                               ZSTD_parameters params, U64 pledgedSrcSize);


/* internal util function */

MEM_STATIC size_t ZBUFF_limitCopy(void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
    size_t length = ZBUFF_MIN(dstCapacity, srcSize);
    memcpy(dst, src, length);
    return length;
}


#if defined (__cplusplus)
}
#endif

#endif  /* ZSTD_BUFFERED_STATIC_H */
