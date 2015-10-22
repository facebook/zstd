/*
    zstd - standard compression library
    Header File for static linking only
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
#pragma once

/* The objects defined into this file should be considered experimental.
 * They are not labelled stable, as their prototype may change in the future.
 * You can use them for tests, provide feedback, or if you can endure risk of future changes.
 */

#if defined (__cplusplus)
extern "C" {
#endif

/* *************************************
*  Includes
***************************************/
#include "zstd.h"


/* *************************************
*  Streaming functions
***************************************/
size_t ZSTD_compressBegin(ZSTD_CCtx* cctx, void* dst, size_t maxDstSize);
size_t ZSTD_compressContinue(ZSTD_CCtx* cctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize);
size_t ZSTD_compressEnd(ZSTD_CCtx* cctx, void* dst, size_t maxDstSize);


typedef struct ZSTD_DCtx_s ZSTD_DCtx;
ZSTD_DCtx* ZSTD_createDCtx(void);
size_t     ZSTD_resetDCtx(ZSTD_DCtx* dctx);
size_t     ZSTD_freeDCtx(ZSTD_DCtx* dctx);

size_t ZSTD_nextSrcSizeToDecompress(ZSTD_DCtx* dctx);
size_t ZSTD_decompressContinue(ZSTD_DCtx* dctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize);
/*
  Use above functions alternatively.
  ZSTD_nextSrcSizeToDecompress() tells how much bytes to provide as 'srcSize' to ZSTD_decompressContinue().
  ZSTD_decompressContinue() will use previous data blocks to improve compression if they are located prior to current block.
  Result is the number of bytes regenerated within 'dst'.
  It can be zero, which is not an error; it just means ZSTD_decompressContinue() has decoded some header.
*/

/* *************************************
*  Prefix - version detection
***************************************/
#define ZSTD_magicNumber 0xFD2FB522   /* v0.2 (current)*/


/* *************************************
*  Error management
***************************************/
#include "error.h"


#if defined (__cplusplus)
}
#endif
