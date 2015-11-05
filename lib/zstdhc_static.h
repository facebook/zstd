/*
    zstdhc - high compression variant
    Header File - Experimental API, static linking only
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
#include "mem.h"
#include "zstdhc.h"


/* *************************************
*  Types
***************************************/
/** from faster to stronger */
typedef enum { ZSTD_HC_greedy, ZSTD_HC_lazy, ZSTD_HC_hclazy2, ZSTD_HC_btlazy2 } ZSTD_HC_strategy;

typedef struct
{
    U32 windowLog;     /* largest match distance : impact decompression buffer size */
    U32 chainLog;      /* full search distance : larger == more compression, slower, more memory*/
    U32 hashLog;       /* dispatch table : larger == more memory, faster*/
    U32 searchLog;     /* nb of searches : larger == more compression, slower*/
    U32 searchLength;  /* size of matches : larger == faster decompression */
    ZSTD_HC_strategy strategy;
} ZSTD_HC_parameters;

/* parameters boundaries */
#define ZSTD_HC_WINDOWLOG_MAX 26
#define ZSTD_HC_WINDOWLOG_MIN 18
#define ZSTD_HC_CHAINLOG_MAX (ZSTD_HC_WINDOWLOG_MAX+1)
#define ZSTD_HC_CHAINLOG_MIN 4
#define ZSTD_HC_HASHLOG_MAX 28
#define ZSTD_HC_HASHLOG_MIN 4
#define ZSTD_HC_SEARCHLOG_MAX (ZSTD_HC_CHAINLOG_MAX-1)
#define ZSTD_HC_SEARCHLOG_MIN 1
#define ZSTD_HC_SEARCHLENGTH_MAX 6
#define ZSTD_HC_SEARCHLENGTH_MIN 4


/* *************************************
*  Advanced function
***************************************/
/** ZSTD_HC_compress_advanced
*   Same as ZSTD_HC_compressCCtx(), with fine-tune control of each compression parameter */
size_t ZSTD_HC_compress_advanced (ZSTD_HC_CCtx* ctx,
                                 void* dst, size_t maxDstSize,
                           const void* src, size_t srcSize,
                                 ZSTD_HC_parameters params);

/** ZSTD_HC_validateParams
    correct params value to remain within authorized range
    optimize for srcSize if srcSize > 0 */
void ZSTD_HC_validateParams(ZSTD_HC_parameters* params, size_t srcSize);


/* *************************************
*  Streaming functions
***************************************/
size_t ZSTD_HC_compressBegin(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, int compressionLevel);
size_t ZSTD_HC_compressContinue(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize);
size_t ZSTD_HC_compressEnd(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize);


/* *************************************
*  Pre-defined compression levels
***************************************/
#define ZSTD_HC_MAX_CLEVEL 22
static const ZSTD_HC_parameters ZSTD_HC_defaultParameters[ZSTD_HC_MAX_CLEVEL+1] = {
    /* W,  C,  H,  S,  L, strat */
    { 18, 12, 14,  1,  4, ZSTD_HC_greedy   },  /* level  0 - never used */
    { 18, 12, 14,  1,  4, ZSTD_HC_greedy   },  /* level  1 - in fact redirected towards zstd fast */
    { 18, 12, 15,  2,  4, ZSTD_HC_greedy   },  /* level  2 */
    { 19, 14, 18,  2,  5, ZSTD_HC_greedy   },  /* level  3 */
    { 20, 17, 19,  3,  5, ZSTD_HC_greedy   },  /* level  4 */
    { 20, 18, 19,  2,  5, ZSTD_HC_lazy     },  /* level  5 */
    { 21, 18, 20,  3,  5, ZSTD_HC_lazy     },  /* level  6 */
    { 21, 19, 20,  3,  5, ZSTD_HC_lazy     },  /* level  7 */
    { 21, 19, 20,  4,  5, ZSTD_HC_lazy     },  /* level  8 */
    { 21, 19, 20,  5,  5, ZSTD_HC_lazy     },  /* level  9 */
    { 21, 20, 20,  5,  5, ZSTD_HC_lazy     },  /* level 10 */
    { 21, 20, 20,  5,  5, ZSTD_HC_hclazy2  },  /* level 11 */
    { 22, 20, 22,  5,  5, ZSTD_HC_hclazy2  },  /* level 12 */
    { 22, 20, 22,  6,  5, ZSTD_HC_hclazy2  },  /* level 13 */
    { 22, 21, 22,  6,  5, ZSTD_HC_hclazy2  },  /* level 14 */
    { 22, 21, 22,  6,  5, ZSTD_HC_hclazy2  },  /* level 15 */
    { 22, 21, 22,  4,  5, ZSTD_HC_btlazy2  },  /* level 16 */
    { 23, 23, 23,  4,  5, ZSTD_HC_btlazy2  },  /* level 17 */
    { 23, 23, 23,  5,  5, ZSTD_HC_btlazy2  },  /* level 18 */
    { 25, 25, 23,  5,  5, ZSTD_HC_btlazy2  },  /* level 19 */
    { 25, 25, 23,  6,  5, ZSTD_HC_btlazy2  },  /* level 20 */
    { 25, 26, 23,  8,  5, ZSTD_HC_btlazy2  },  /* level 21 */
    { 25, 26, 23,  8,  5, ZSTD_HC_btlazy2  },  /* level 22 */
};


#if defined (__cplusplus)
}
#endif
