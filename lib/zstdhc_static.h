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
typedef struct
{
    U32 windowLog;    /* largest match distance : impact decompression buffer size */
    U32 chainLog;     /* full search distance : larger == more compression, slower, more memory*/
    U32 hashLog;      /* dispatch table : larger == more memory, faster*/
    U32 searchLog;    /* nb of searches : larger == more compression, slower*/
} ZSTD_HC_parameters;

/* parameters boundaries */
#define ZSTD_HC_WINDOWLOG_MAX 26
#define ZSTD_HC_WINDOWLOG_MIN 17
#define ZSTD_HC_CHAINLOG_MAX ZSTD_HC_WINDOWLOG_MAX
#define ZSTD_HC_CHAINLOG_MIN 4
#define ZSTD_HC_HASHLOG_MAX 28
#define ZSTD_HC_HASHLOG_MIN 4
#define ZSTD_HC_SEARCHLOG_MAX (ZSTD_HC_CHAINLOG_MAX-1)
#define ZSTD_HC_SEARCHLOG_MIN 1


/* *************************************
*  Functions
***************************************/
/** ZSTD_HC_compress_advanced
*   Same as ZSTD_HC_compressCCtx(), but can fine-tune each compression parameter */
size_t ZSTD_HC_compress_advanced (ZSTD_HC_CCtx* ctx,
                                 void* dst, size_t maxDstSize,
                           const void* src, size_t srcSize,
                                 ZSTD_HC_parameters params);


/* *************************************
*  Pre-defined compression levels
***************************************/
#define ZSTD_HC_MAX_CLEVEL 20
static const ZSTD_HC_parameters ZSTD_HC_defaultParameters[ZSTD_HC_MAX_CLEVEL+1] = {
    /* W,  C,  H,  S */
    { 18,  4, 12,  1 },  /* 0 - should never be used */
    { 18, 11, 14,  1 },  /* 1 */
    { 20, 13, 16,  2 },  /* 2 */
    { 20, 19, 20,  2 },  /* 3 */
    { 21, 19, 20,  4 },  /* 4 */
    { 21, 20, 21,  4 },  /* 5 */
    { 21, 20, 22,  5 },  /* 6 */
    { 21, 20, 22,  6 },  /* 7 */
    { 21, 20, 22,  7 },  /* 8 */
    { 21, 21, 23,  7 },  /* 9 */
    { 21, 21, 23,  8 },  /*10 */
    { 21, 21, 23,  9 },  /*11 */
    { 21, 21, 23, 10 },  /*12 */
    { 22, 22, 24,  9 },  /*13 */
    { 22, 22, 24, 10 },  /*14 */
    { 23, 23, 23, 10 },  /*15 */
    { 24, 23, 22, 11 },  /*16 */
    { 24, 24, 23, 11 },  /*17 */
    { 24, 24, 22, 12 },  /*18 */
    { 24, 24, 22, 13 },  /*19 */
    { 25, 25, 25, 22 },  /*20 - ultra-slow */
};


#if defined (__cplusplus)
}
#endif
