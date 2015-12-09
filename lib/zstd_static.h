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
#ifndef ZSTD_STATIC_H
#define ZSTD_STATIC_H

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
#include "mem.h"


/* *************************************
*  Types
***************************************/
#define ZSTD_WINDOWLOG_MAX 26
#define ZSTD_WINDOWLOG_MIN 18
#define ZSTD_WINDOWLOG_ABSOLUTEMIN 11
#define ZSTD_CONTENTLOG_MAX (ZSTD_WINDOWLOG_MAX+1)
#define ZSTD_CONTENTLOG_MIN 4
#define ZSTD_HASHLOG_MAX 28
#define ZSTD_HASHLOG_MIN 4
#define ZSTD_SEARCHLOG_MAX (ZSTD_CONTENTLOG_MAX-1)
#define ZSTD_SEARCHLOG_MIN 1
#define ZSTD_SEARCHLENGTH_MAX 7
#define ZSTD_SEARCHLENGTH_MIN 4

/** from faster to stronger */
typedef enum { ZSTD_fast, ZSTD_greedy, ZSTD_lazy, ZSTD_lazy2, ZSTD_btlazy2 } ZSTD_strategy;

typedef struct
{
    U64 srcSize;       /* optional : tells how much bytes are present in the frame. Use 0 if not known. */
    U32 windowLog;     /* largest match distance : larger == more compression, more memory needed during decompression */
    U32 contentLog;    /* full search segment : larger == more compression, slower, more memory (useless for fast) */
    U32 hashLog;       /* dispatch table : larger == more memory, faster */
    U32 searchLog;     /* nb of searches : larger == more compression, slower */
    U32 searchLength;  /* size of matches : larger == faster decompression, sometimes less compression */
    ZSTD_strategy strategy;
} ZSTD_parameters;


/* *************************************
*  Advanced function
***************************************/
/** ZSTD_getParams
*   return ZSTD_parameters structure for a selected compression level and srcSize.
*   srcSizeHint value is optional, select 0 if not known */
ZSTDLIB_API ZSTD_parameters ZSTD_getParams(int compressionLevel, U64 srcSizeHint);

/** ZSTD_validateParams
*   correct params value to remain within authorized range */
ZSTDLIB_API void ZSTD_validateParams(ZSTD_parameters* params);

/** ZSTD_compress_advanced
*   Same as ZSTD_compressCCtx(), with fine-tune control of each compression parameter */
ZSTDLIB_API size_t ZSTD_compress_advanced (ZSTD_CCtx* ctx,
                                           void* dst, size_t maxDstSize,
                                     const void* src, size_t srcSize,
                                           ZSTD_parameters params);


/* **************************************
*  Streaming functions (bufferless mode)
****************************************/
ZSTDLIB_API size_t ZSTD_compressBegin(ZSTD_CCtx* cctx, void* dst, size_t maxDstSize, int compressionLevel);
ZSTDLIB_API size_t ZSTD_compressBegin_advanced(ZSTD_CCtx* ctx, void* dst, size_t maxDstSize, ZSTD_parameters params);
ZSTDLIB_API size_t ZSTD_compress_insertDictionary(ZSTD_CCtx* ctx, const void* src, size_t srcSize);

ZSTDLIB_API size_t ZSTD_compressContinue(ZSTD_CCtx* cctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize);
ZSTDLIB_API size_t ZSTD_compressEnd(ZSTD_CCtx* cctx, void* dst, size_t maxDstSize);

/**
  Streaming compression, bufferless mode

  A ZSTD_CCtx object is required to track streaming operations.
  Use ZSTD_createCCtx() / ZSTD_freeCCtx() to manage it.
  A ZSTD_CCtx object can be re-used multiple times.

  First operation is to start a new frame.
  Use ZSTD_compressBegin().
  You may also prefer the advanced derivative ZSTD_compressBegin_advanced(), for finer parameter control.

  It's then possible to add a dictionary with ZSTD_compress_insertDictionary()
  Note that dictionary presence is a "hidden" information,
  the decoder needs to be aware that it is required for proper decoding, or decoding will fail.

  Then, consume your input using ZSTD_compressContinue().
  The interface is synchronous, so all input will be consumed.
  You must ensure there is enough space in destination buffer to store compressed data under worst case scenario.
  Worst case evaluation is provided by ZSTD_compressBound().

  Finish a frame with ZSTD_compressEnd(), which will write the epilogue.
  Without it, the frame will be considered incomplete by decoders.
  You can then re-use ZSTD_CCtx to compress new frames.
*/


typedef struct ZSTD_DCtx_s ZSTD_DCtx;
ZSTDLIB_API ZSTD_DCtx* ZSTD_createDCtx(void);
ZSTDLIB_API size_t     ZSTD_freeDCtx(ZSTD_DCtx* dctx);

ZSTDLIB_API size_t ZSTD_resetDCtx(ZSTD_DCtx* dctx);
ZSTDLIB_API size_t ZSTD_getFrameParams(ZSTD_parameters* params, const void* src, size_t srcSize);
ZSTDLIB_API void   ZSTD_decompress_insertDictionary(ZSTD_DCtx* ctx, const void* src, size_t srcSize);

ZSTDLIB_API size_t ZSTD_nextSrcSizeToDecompress(ZSTD_DCtx* dctx);
ZSTDLIB_API size_t ZSTD_decompressContinue(ZSTD_DCtx* dctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize);

/**
  Streaming decompression, bufferless mode

  A ZSTD_DCtx object is required to track streaming operations.
  Use ZSTD_createDCtx() / ZSTD_freeDCtx() to manage it.
  A ZSTD_DCtx object can be re-used multiple times. Use ZSTD_resetDCtx() to return to fresh status.

  First operation is to retrieve frame parameters, using ZSTD_getFrameParams().
  This function doesn't consume its input. It needs enough input data to properly decode the frame header.
  Objective is to retrieve *params.windowlog, to know minimum amount of memory required during decoding.
  Result : 0 when successful, it means the ZSTD_parameters structure has been filled.
           >0 : means there is not enough data into src. Provides the expected size to successfully decode header.
           errorCode, which can be tested using ZSTD_isError() (For example, if it's not a ZSTD header)

  Then, you can optionally insert a dictionary. This operation must mimic the compressor behavior, otherwise decompression will fail or be corrupted.

  Then it's possible to start decompression.
  Use ZSTD_nextSrcSizeToDecompress() and ZSTD_decompressContinue() alternatively.
  ZSTD_nextSrcSizeToDecompress() tells how much bytes to provide as 'srcSize' to ZSTD_decompressContinue().
  ZSTD_decompressContinue() requires this exact amount of bytes, or it will fail.
  ZSTD_decompressContinue() needs previous data blocks during decompression, up to (1 << windowlog).
  They should preferably be located contiguously, prior to current block. Alternatively, a round buffer is also possible.

  @result of ZSTD_decompressContinue() is the number of bytes regenerated within 'dst'.
  It can be zero, which is not an error; it just means ZSTD_decompressContinue() has decoded some header.

  A frame is fully decoded when ZSTD_nextSrcSizeToDecompress() returns zero.
*/


/* *************************************
*  Pre-defined compression levels
***************************************/
#define ZSTD_MAX_CLEVEL 20
unsigned ZSTD_maxCLevel (void);
static const ZSTD_parameters ZSTD_defaultParameters[4][ZSTD_MAX_CLEVEL+1] = {
{   /* "default" */
    /*    W,  C,  H,  S,  L, strat */
    { 0, 18, 12, 12,  1,  4, ZSTD_fast    },  /* level  0 - never used */
    { 0, 19, 13, 14,  1,  7, ZSTD_fast    },  /* level  1 */
    { 0, 19, 15, 16,  1,  6, ZSTD_fast    },  /* level  2 */
    { 0, 20, 18, 20,  1,  6, ZSTD_fast    },  /* level  3 */
    { 0, 21, 19, 21,  1,  6, ZSTD_fast    },  /* level  4 */
    { 0, 20, 14, 18,  3,  5, ZSTD_greedy  },  /* level  5 */
    { 0, 20, 18, 19,  3,  5, ZSTD_greedy  },  /* level  6 */
    { 0, 21, 17, 20,  3,  5, ZSTD_lazy    },  /* level  7 */
    { 0, 21, 19, 20,  3,  5, ZSTD_lazy    },  /* level  8 */
    { 0, 21, 20, 20,  3,  5, ZSTD_lazy2   },  /* level  9 */
    { 0, 21, 19, 21,  4,  5, ZSTD_lazy2   },  /* level 10 */
    { 0, 22, 20, 22,  4,  5, ZSTD_lazy2   },  /* level 11 */
    { 0, 22, 20, 22,  5,  5, ZSTD_lazy2   },  /* level 12 */
    { 0, 22, 21, 22,  5,  5, ZSTD_lazy2   },  /* level 13 */
    { 0, 22, 22, 23,  5,  5, ZSTD_lazy2   },  /* level 14 */
    { 0, 23, 23, 23,  5,  5, ZSTD_lazy2   },  /* level 15 */
    { 0, 23, 21, 22,  5,  5, ZSTD_btlazy2 },  /* level 16 */
    { 0, 23, 24, 23,  4,  5, ZSTD_btlazy2 },  /* level 17 */
    { 0, 25, 24, 23,  5,  5, ZSTD_btlazy2 },  /* level 18 */
    { 0, 25, 26, 23,  5,  5, ZSTD_btlazy2 },  /* level 19 */
    { 0, 26, 27, 25,  9,  5, ZSTD_btlazy2 },  /* level 20 */
},
{   /* for srcSize <= 256 KB */
    /*    W,  C,  H,  S,  L, strat */
    {  0,  0,  0,  0,  0,  0, ZSTD_fast    },  /* level  0 - never used */
    {  0, 18, 16, 15,  1,  7, ZSTD_fast    },  /* level  1 */
    {  0, 18, 16, 16,  1,  7, ZSTD_fast    },  /* level  2 */
    {  0, 18, 18, 18,  1,  7, ZSTD_fast    },  /* level  3 */
    {  0, 18, 14, 15,  4,  6, ZSTD_greedy  },  /* level  4 */
    {  0, 18, 16, 16,  1,  6, ZSTD_lazy    },  /* level  5 */
    {  0, 18, 15, 15,  3,  6, ZSTD_lazy    },  /* level  6 */
    {  0, 18, 15, 15,  4,  6, ZSTD_lazy    },  /* level  7 */
    {  0, 18, 16, 18,  4,  6, ZSTD_lazy    },  /* level  8 */
    {  0, 18, 18, 18,  4,  6, ZSTD_lazy    },  /* level  9 */
    {  0, 18, 18, 18,  5,  6, ZSTD_lazy    },  /* level 10 */
    {  0, 18, 18, 19,  6,  6, ZSTD_lazy    },  /* level 11 */
    {  0, 18, 18, 19,  7,  6, ZSTD_lazy    },  /* level 12 */
    {  0, 18, 19, 15,  7,  5, ZSTD_btlazy2 },  /* level 13 */
    {  0, 18, 19, 16,  8,  5, ZSTD_btlazy2 },  /* level 14 */
    {  0, 18, 19, 17,  9,  5, ZSTD_btlazy2 },  /* level 15 */
    {  0, 18, 19, 17, 10,  5, ZSTD_btlazy2 },  /* level 16 */
    {  0, 18, 19, 17, 11,  5, ZSTD_btlazy2 },  /* level 17 */
    {  0, 18, 19, 17, 12,  5, ZSTD_btlazy2 },  /* level 18 */
    {  0, 18, 19, 17, 13,  5, ZSTD_btlazy2 },  /* level 19 */
    {  0, 18, 19, 17, 14,  5, ZSTD_btlazy2 },  /* level 20 */
},
{   /* for srcSize <= 128 KB */
    /*    W,  C,  H,  S,  L, strat */
    { 0, 17, 12, 12,  1,  4, ZSTD_fast    },  /* level  0 - never used */
    { 0, 17, 12, 13,  1,  6, ZSTD_fast    },  /* level  1 */
    { 0, 17, 15, 16,  1,  5, ZSTD_fast    },  /* level  2 */
    { 0, 17, 16, 17,  1,  5, ZSTD_fast    },  /* level  3 */
    { 0, 17, 13, 15,  2,  4, ZSTD_greedy  },  /* level  4 */
    { 0, 17, 15, 17,  3,  4, ZSTD_greedy  },  /* level  5 */
    { 0, 17, 14, 17,  3,  4, ZSTD_lazy    },  /* level  6 */
    { 0, 17, 16, 17,  4,  4, ZSTD_lazy    },  /* level  7 */
    { 0, 17, 16, 17,  4,  4, ZSTD_lazy2   },  /* level  8 */
    { 0, 17, 17, 16,  5,  4, ZSTD_lazy2   },  /* level  9 */
    { 0, 17, 17, 16,  6,  4, ZSTD_lazy2   },  /* level 10 */
    { 0, 17, 17, 16,  7,  4, ZSTD_lazy2   },  /* level 11 */
    { 0, 17, 17, 16,  8,  4, ZSTD_lazy2   },  /* level 12 */
    { 0, 17, 18, 16,  4,  4, ZSTD_btlazy2 },  /* level 13 */
    { 0, 17, 18, 16,  5,  4, ZSTD_btlazy2 },  /* level 14 */
    { 0, 17, 18, 16,  6,  4, ZSTD_btlazy2 },  /* level 15 */
    { 0, 17, 18, 16,  7,  4, ZSTD_btlazy2 },  /* level 16 */
    { 0, 17, 18, 16,  8,  4, ZSTD_btlazy2 },  /* level 17 */
    { 0, 17, 18, 16,  9,  4, ZSTD_btlazy2 },  /* level 18 */
    { 0, 17, 18, 16, 10,  4, ZSTD_btlazy2 },  /* level 19 */
    { 0, 17, 18, 18, 12,  4, ZSTD_btlazy2 },  /* level 20 */
},
{   /* for srcSize <= 16 KB */
    /*     W,  C,  H,  S,  L, strat */
    {  0,  0,  0,  0,  0,  0, ZSTD_fast    },  /* level  0 - never used */
    {  0, 14, 14, 14,  1,  4, ZSTD_fast    },  /* level  1 */
    {  0, 14, 14, 16,  1,  4, ZSTD_fast    },  /* level  1 */
    {  0, 14, 14, 14,  5,  4, ZSTD_greedy  },  /* level  3 */
    {  0, 14, 14, 14,  8,  4, ZSTD_greedy  },  /* level  4 */
    {  0, 14, 11, 14,  6,  4, ZSTD_lazy    },  /* level  5 */
    {  0, 14, 14, 13,  6,  5, ZSTD_lazy    },  /* level  6 */
    {  0, 14, 14, 14,  7,  6, ZSTD_lazy    },  /* level  7 */
    {  0, 14, 14, 14,  8,  4, ZSTD_lazy    },  /* level  8 */
    {  0, 14, 14, 15,  9,  4, ZSTD_lazy    },  /* level  9 */
    {  0, 14, 14, 15, 10,  4, ZSTD_lazy    },  /* level 10 */
    {  0, 14, 15, 15,  6,  4, ZSTD_btlazy2 },  /* level 11 */
    {  0, 14, 15, 15,  7,  4, ZSTD_btlazy2 },  /* level 12 */
    {  0, 14, 15, 15,  8,  4, ZSTD_btlazy2 },  /* level 13 */
    {  0, 14, 15, 15,  9,  4, ZSTD_btlazy2 },  /* level 14 */
    {  0, 14, 15, 15, 10,  4, ZSTD_btlazy2 },  /* level 15 */
    {  0, 14, 15, 15, 11,  4, ZSTD_btlazy2 },  /* level 16 */
    {  0, 14, 15, 15, 12,  4, ZSTD_btlazy2 },  /* level 17 */
    {  0, 14, 15, 15, 13,  4, ZSTD_btlazy2 },  /* level 18 */
    {  0, 14, 15, 15, 14,  4, ZSTD_btlazy2 },  /* level 19 */
    {  0, 14, 15, 15, 15,  4, ZSTD_btlazy2 },  /* level 20 */
},
};


/* *************************************
*  Error management
***************************************/
#include "error.h"


#if defined (__cplusplus)
}
#endif

#endif  /* ZSTD_STATIC_H */
