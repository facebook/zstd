/*
    zstdhc - high compression variant
    Header File
    Copyright (C) 2015, Yann Collet.

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
    - zstd source repository : http://www.zstd.net
*/
#pragma once

#if defined (__cplusplus)
extern "C" {
#endif

/* *************************************
*  Includes
***************************************/
#include <stddef.h>   /* size_t */


/* *************************************
*  Simple function
***************************************/
/**
ZSTD_HC_compress() :
    Compresses 'srcSize' bytes from buffer 'src' into buffer 'dst', of maximum size 'dstSize'.
    Destination buffer must be already allocated.
    Compression runs faster if maxDstSize >=  ZSTD_compressBound(srcSize).
    @return : the number of bytes written into buffer 'dst'
              or an error code if it fails (which can be tested using ZSTD_isError())
*/
size_t ZSTD_HC_compress(void* dst, size_t maxDstSize,
                  const void* src, size_t srcSize,
                  unsigned compressionLevel);


/* *************************************
*  Advanced functions
***************************************/
typedef struct ZSTD_HC_CCtx_s ZSTD_HC_CCtx;   /* incomplete type */
ZSTD_HC_CCtx* ZSTD_HC_createCCtx(void);
size_t ZSTD_HC_freeCCtx(ZSTD_HC_CCtx* cctx);

/**
ZSTD_HC_compressCCtx() :
    Same as ZSTD_compress(), but requires a ZSTD_HC_CCtx working space already allocated
*/
size_t ZSTD_HC_compressCCtx(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize, unsigned compressionLevel);


#if defined (__cplusplus)
}
#endif
