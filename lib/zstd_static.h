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
/** from faster to stronger */
typedef enum { ZSTD_fast, ZSTD_greedy, ZSTD_lazy, ZSTD_lazy2, ZSTD_btlazy2 } ZSTD_strategy;

typedef struct
{
    U32 windowLog;     /* largest match distance : impact decompression buffer size */
    U32 contentLog;    /* full search segment : larger == more compression, slower, more memory (useless for fast) */
    U32 hashLog;       /* dispatch table : larger == more memory, faster*/
    U32 searchLog;     /* nb of searches : larger == more compression, slower*/
    U32 searchLength;  /* size of matches : larger == faster decompression */
    ZSTD_strategy strategy;
} ZSTD_parameters;


/* *************************************
*  Advanced function
***************************************/
/** ZSTD_compress_advanced
*   Same as ZSTD_compressCCtx(), with fine-tune control of each compression parameter */
size_t ZSTD_compress_advanced (ZSTD_CCtx* ctx,
                                 void* dst, size_t maxDstSize,
                           const void* src, size_t srcSize,
                                 ZSTD_parameters params);

/** ZSTD_validateParams
    correct params value to remain within authorized range
    srcSizeHint value is optional, select 0 if not known */
void ZSTD_validateParams(ZSTD_parameters* params, U64 srcSizeHint);


/* *************************************
*  Streaming functions
***************************************/
size_t ZSTD_compressBegin(ZSTD_CCtx* cctx, void* dst, size_t maxDstSize, int compressionLevel, U64 srcSizeHint);
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
#define ZSTD_magicNumber 0xFD2FB523   /* v0.3 (current)*/


/* *************************************
*  Pre-defined compression levels
***************************************/
#define ZSTD_MAX_CLEVEL 20
#define ZSTD_WINDOWLOG_MAX 26
#define ZSTD_WINDOWLOG_MIN 18
#define ZSTD_CONTENTLOG_MAX (ZSTD_WINDOWLOG_MAX+1)
#define ZSTD_CONTENTLOG_MIN 4
#define ZSTD_HASHLOG_MAX 28
#define ZSTD_HASHLOG_MIN 4
#define ZSTD_SEARCHLOG_MAX (ZSTD_CONTENTLOG_MAX-1)
#define ZSTD_SEARCHLOG_MIN 1
#define ZSTD_SEARCHLENGTH_MAX 7
#define ZSTD_SEARCHLENGTH_MIN 4

static const ZSTD_parameters ZSTD_defaultParameters[2][ZSTD_MAX_CLEVEL+1] = {
{   /* for <= 128 KB */
    /* W,  C,  H,  S,  L, strat */
    { 17, 12, 12,  1,  4, ZSTD_fast    },  /* level  0 - never used */
    { 17, 12, 13,  1,  6, ZSTD_fast    },  /* level  1 */
    { 17, 15, 16,  1,  5, ZSTD_fast    },  /* level  2 */
    { 17, 16, 17,  1,  5, ZSTD_fast    },  /* level  3 */
    { 17, 13, 15,  2,  4, ZSTD_greedy  },  /* level  4 */
    { 17, 15, 17,  3,  4, ZSTD_greedy  },  /* level  5 */
    { 17, 14, 17,  3,  4, ZSTD_lazy    },  /* level  6 */
    { 17, 16, 17,  4,  4, ZSTD_lazy    },  /* level  7 */
    { 17, 16, 17,  4,  4, ZSTD_lazy2   },  /* level  8 */
    { 17, 17, 16,  5,  4, ZSTD_lazy2   },  /* level  9 */
    { 17, 17, 16,  6,  4, ZSTD_lazy2   },  /* level 10 */
    { 17, 17, 16,  7,  4, ZSTD_lazy2   },  /* level 11 */
    { 17, 17, 16,  8,  4, ZSTD_lazy2   },  /* level 12 */
    { 17, 18, 16,  4,  4, ZSTD_btlazy2 },  /* level 13 */
    { 17, 18, 16,  5,  4, ZSTD_btlazy2 },  /* level 14 */
    { 17, 18, 16,  6,  4, ZSTD_btlazy2 },  /* level 15 */
    { 17, 18, 16,  7,  4, ZSTD_btlazy2 },  /* level 16 */
    { 17, 18, 16,  8,  4, ZSTD_btlazy2 },  /* level 17 */
    { 17, 18, 16,  9,  4, ZSTD_btlazy2 },  /* level 18 */
    { 17, 18, 16, 10,  4, ZSTD_btlazy2 },  /* level 19 */
    { 17, 18, 18, 12,  4, ZSTD_btlazy2 },  /* level 20 */
},
{   /* for > 128 KB */
    /* W,  C,  H,  S,  L, strat */
    { 18, 12, 12,  1,  4, ZSTD_fast    },  /* level  0 - never used */
    { 19, 13, 14,  1,  7, ZSTD_fast    },  /* level  1 */
    { 19, 15, 16,  1,  6, ZSTD_fast    },  /* level  2 */
    { 20, 18, 20,  1,  6, ZSTD_fast    },  /* level  3 */
    { 21, 19, 21,  1,  6, ZSTD_fast    },  /* level  4 */
    { 20, 14, 18,  3,  5, ZSTD_greedy  },  /* level  5 */
    { 20, 18, 19,  3,  5, ZSTD_greedy  },  /* level  6 */
    { 21, 17, 20,  3,  5, ZSTD_lazy    },  /* level  7 */
    { 21, 19, 20,  3,  5, ZSTD_lazy    },  /* level  8 */
    { 21, 20, 20,  3,  5, ZSTD_lazy2   },  /* level  9 */
    { 21, 19, 21,  4,  5, ZSTD_lazy2   },  /* level 10 */
    { 22, 20, 22,  4,  5, ZSTD_lazy2   },  /* level 11 */
    { 22, 20, 22,  5,  5, ZSTD_lazy2   },  /* level 12 */
    { 22, 21, 22,  5,  5, ZSTD_lazy2   },  /* level 13 */
    { 22, 22, 23,  5,  5, ZSTD_lazy2   },  /* level 14 */
    { 23, 23, 23,  5,  5, ZSTD_lazy2   },  /* level 15 */
    { 23, 21, 22,  5,  5, ZSTD_btlazy2 },  /* level 16 */
    { 23, 24, 23,  4,  5, ZSTD_btlazy2 },  /* level 17 */
    { 25, 24, 23,  5,  5, ZSTD_btlazy2 },  /* level 18 */
    { 25, 26, 23,  5,  5, ZSTD_btlazy2 },  /* level 19 */
    { 25, 26, 25,  6,  5, ZSTD_btlazy2 },  /* level 20 */
}
};


/* *************************************
*  Error management
***************************************/
#include "error.h"


#if defined (__cplusplus)
}
#endif

#endif  /* ZSTD_STATIC_H */
