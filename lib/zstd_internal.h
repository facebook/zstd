/*
    zstd_internal - common functions to include
    Header File for include
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
#ifndef ZSTD_CCOMMON_H_MODULE
#define ZSTD_CCOMMON_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif

/* *************************************
*  Includes
***************************************/
#include "mem.h"
#include "error.h"


/* *************************************
*  Common macros
***************************************/
#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define MAX(a,b) ((a)>(b) ? (a) : (b))


/* *************************************
*  Common constants
***************************************/
#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define BLOCKSIZE (128 KB)                 /* define, for static allocation */

static const size_t ZSTD_blockHeaderSize = 3;
static const size_t ZSTD_frameHeaderSize = 4;

#define BIT7 128
#define BIT6  64
#define BIT5  32
#define BIT4  16
#define BIT1   2
#define BIT0   1

#define IS_RAW BIT0
#define IS_RLE BIT1

#define MINMATCH 4
#define REPCODE_STARTVALUE 4

#define MLbits   7
#define LLbits   6
#define Offbits  5
#define MaxML  ((1<<MLbits) - 1)
#define MaxLL  ((1<<LLbits) - 1)
#define MaxOff ((1<<Offbits)- 1)
#define MLFSELog   10
#define LLFSELog   10
#define OffFSELog   9
#define MaxSeq MAX(MaxLL, MaxML)

#define MIN_SEQUENCES_SIZE (2 /*seqNb*/ + 2 /*dumps*/ + 3 /*seqTables*/ + 1 /*bitStream*/)
#define MIN_CBLOCK_SIZE (3 /*litCSize*/ + MIN_SEQUENCES_SIZE)

typedef enum { bt_compressed, bt_raw, bt_rle, bt_end } blockType_t;


/* ******************************************
*  Shared functions to include for inlining
********************************************/
static void ZSTD_copy8(void* dst, const void* src) { memcpy(dst, src, 8); }

#define COPY8(d,s) { ZSTD_copy8(d,s); d+=8; s+=8; }

/*! ZSTD_wildcopy : custom version of memcpy(), can copy up to 7-8 bytes too many */
static void ZSTD_wildcopy(void* dst, const void* src, size_t length)
{
    const BYTE* ip = (const BYTE*)src;
    BYTE* op = (BYTE*)dst;
    BYTE* const oend = op + length;
    do
        COPY8(op, ip)
    while (op < oend);
}


#if defined (__cplusplus)
}
#endif

#endif   /* ZSTD_CCOMMON_H_MODULE */
