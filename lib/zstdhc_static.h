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
typedef enum { ZSTD_HC_greedy, ZSTD_HC_lazy } ZSTD_HC_strategy;
typedef struct
{
    U32 windowLog;     /* largest match distance : impact decompression buffer size */
    U32 chainLog;      /* full search distance : larger == more compression, slower, more memory*/
    U32 hashLog;       /* dispatch table : larger == more memory, faster*/
    U32 searchLog;     /* nb of searches : larger == more compression, slower*/
    U32 searchLength;  /* size of matches : larger == faster decompression */
    ZSTD_HC_strategy strategy;   /* greedy, lazy (stronger, slower) */
} ZSTD_HC_parameters;

/* parameters boundaries */
#define ZSTD_HC_WINDOWLOG_MAX 26
#define ZSTD_HC_WINDOWLOG_MIN 18
#define ZSTD_HC_CHAINLOG_MAX ZSTD_HC_WINDOWLOG_MAX
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
*   Same as ZSTD_HC_compressCCtx(), but can fine-tune each compression parameter */
size_t ZSTD_HC_compress_advanced (ZSTD_HC_CCtx* ctx,
                                 void* dst, size_t maxDstSize,
                           const void* src, size_t srcSize,
                                 ZSTD_HC_parameters params);


/* *************************************
*  Streaming functions
***************************************/
size_t ZSTD_HC_compressBegin(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, int compressionLevel);
size_t ZSTD_HC_compressContinue(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize);
size_t ZSTD_HC_compressEnd(ZSTD_HC_CCtx* ctx, void* dst, size_t maxDstSize);


/* *************************************
*  Pre-defined compression levels
***************************************/
#define ZSTD_HC_MAX_CLEVEL 26
static const ZSTD_HC_parameters ZSTD_HC_defaultParameters[ZSTD_HC_MAX_CLEVEL+1] = {
    /* W,  C,  H,  S */
    { 18, 12, 14,  1,  4, ZSTD_HC_greedy },   /* level  0 - never used */
    { 18, 12, 14,  1,  4, ZSTD_HC_greedy },   /* level  1 - in fact redirected towards zstd fast */
    { 18, 12, 15,  2,  4, ZSTD_HC_greedy },   /* level  2 */
    { 19, 13, 17,  3,  5, ZSTD_HC_greedy },   /* level  3 */
    { 20, 18, 19,  2,  5, ZSTD_HC_greedy },   /* level  4 */
    { 20, 19, 19,  3,  5, ZSTD_HC_greedy },   /* level  5 */
    { 20, 19, 19,  4,  5, ZSTD_HC_greedy },   /* level  6 */
    { 20, 19, 19,  5,  5, ZSTD_HC_greedy },   /* level  7 */
    { 20, 20, 20,  5,  5, ZSTD_HC_greedy },   /* level  8 */
    { 20, 20, 20,  6,  5, ZSTD_HC_greedy },   /* level  9 */
    { 21, 20, 21,  6,  5, ZSTD_HC_greedy },   /* level 10 */
    { 22, 21, 22,  6,  5, ZSTD_HC_greedy },   /* level 11 */
    { 23, 21, 22,  6,  5, ZSTD_HC_greedy },   /* level 12 */
    { 22, 22, 22,  6,  5, ZSTD_HC_greedy },   /* level 13 */
    { 22, 22, 23,  7,  5, ZSTD_HC_greedy },   /* level 14 */
    { 22, 22, 23,  7,  5, ZSTD_HC_greedy },   /* level 15 */
    { 22, 22, 23,  8,  5, ZSTD_HC_greedy },   /* level 16 */
    { 22, 22, 23,  8,  5, ZSTD_HC_greedy },   /* level 17 */
    { 22, 22, 23,  9,  5, ZSTD_HC_greedy },   /* level 18 */
    { 22, 22, 23, 10,  5, ZSTD_HC_greedy },   /* level 19 */
    { 23, 23, 23,  9,  5, ZSTD_HC_greedy },   /* level 20 */
    { 23, 23, 23,  9,  5, ZSTD_HC_greedy },   /* level 21 */
    { 23, 23, 23, 10,  5, ZSTD_HC_greedy },   /* level 22 */
    { 23, 23, 23, 11,  5, ZSTD_HC_greedy },   /* level 23 */
    { 23, 23, 23, 11,  5, ZSTD_HC_greedy },   /* level 24 */
    { 24, 24, 23, 11,  5, ZSTD_HC_greedy },   /* level 25 */
    { 24, 24, 24, 12,  5, ZSTD_HC_greedy },   /* level 26 */   /* ZSTD_HC_MAX_CLEVEL */
};

#if defined (__cplusplus)
}
#endif
