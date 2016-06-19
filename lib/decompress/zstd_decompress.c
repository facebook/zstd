/*
    zstd - standard compression library
    Copyright (C) 2014-2016, Yann Collet.

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

/* ***************************************************************
*  Tuning parameters
*****************************************************************/
/*!
 * HEAPMODE :
 * Select how default decompression function ZSTD_decompress() will allocate memory,
 * in memory stack (0), or in memory heap (1, requires malloc())
 */
#ifndef ZSTD_HEAPMODE
#  define ZSTD_HEAPMODE 1
#endif

/*!
*  LEGACY_SUPPORT :
*  if set to 1, ZSTD_decompress() can decode older formats (v0.1+)
*/
#ifndef ZSTD_LEGACY_SUPPORT
#  define ZSTD_LEGACY_SUPPORT 0
#endif


/*-*******************************************************
*  Dependencies
*********************************************************/
#include <string.h>      /* memcpy, memmove, memset */
#include <stdio.h>       /* debug only : printf */
#include "mem.h"         /* low level memory routines */
#define XXH_STATIC_LINKING_ONLY   /* XXH64_state_t */
#include "xxhash.h"      /* XXH64_* */
#define FSE_STATIC_LINKING_ONLY
#include "fse.h"
#define HUF_STATIC_LINKING_ONLY
#include "huf.h"
#include "zstd_internal.h"

#if defined(ZSTD_LEGACY_SUPPORT) && (ZSTD_LEGACY_SUPPORT>=1)
#  include "zstd_legacy.h"
#endif


/*-*******************************************************
*  Compiler specifics
*********************************************************/
#ifdef _MSC_VER    /* Visual Studio */
#  define FORCE_INLINE static __forceinline
#  include <intrin.h>                    /* For Visual 2005 */
#  pragma warning(disable : 4127)        /* disable: C4127: conditional expression is constant */
#  pragma warning(disable : 4324)        /* disable: C4324: padded structure */
#else
#  ifdef __GNUC__
#    define FORCE_INLINE static inline __attribute__((always_inline))
#  else
#    define FORCE_INLINE static inline
#  endif
#endif


/*-*************************************
*  Macros
***************************************/
#define ZSTD_isError ERR_isError   /* for inlining */
#define FSE_isError  ERR_isError
#define HUF_isError  ERR_isError


/*_*******************************************************
*  Memory operations
**********************************************************/
static void ZSTD_copy4(void* dst, const void* src) { memcpy(dst, src, 4); }


/*-*************************************************************
*   Context management
***************************************************************/
typedef enum { ZSTDds_getFrameHeaderSize, ZSTDds_decodeFrameHeader,
               ZSTDds_decodeBlockHeader, ZSTDds_decompressBlock,
               ZSTDds_decodeSkippableHeader, ZSTDds_skipFrame } ZSTD_dStage;

struct ZSTD_DCtx_s
{
    FSE_DTable LLTable[FSE_DTABLE_SIZE_U32(LLFSELog)];
    FSE_DTable OffTable[FSE_DTABLE_SIZE_U32(OffFSELog)];
    FSE_DTable MLTable[FSE_DTABLE_SIZE_U32(MLFSELog)];
    HUF_DTable hufTable[HUF_DTABLE_SIZE(HufLog)];  /* can accommodate HUF_decompress4X */
    const void* previousDstEnd;
    const void* base;
    const void* vBase;
    const void* dictEnd;
    size_t expected;
    U32 rep[3];
    ZSTD_frameParams fParams;
    blockType_t bType;   /* used in ZSTD_decompressContinue(), to transfer blockType between header decoding and block decoding stages */
    ZSTD_dStage stage;
    U32 litEntropy;
    U32 fseEntropy;
    XXH64_state_t xxhState;
    size_t headerSize;
    U32 dictID;
    const BYTE* litPtr;
    ZSTD_customMem customMem;
    size_t litBufSize;
    size_t litSize;
    BYTE litBuffer[ZSTD_BLOCKSIZE_MAX + WILDCOPY_OVERLENGTH];
    BYTE headerBuffer[ZSTD_FRAMEHEADERSIZE_MAX];
};  /* typedef'd to ZSTD_DCtx within "zstd_static.h" */

size_t ZSTD_sizeofDCtx (void) { return sizeof(ZSTD_DCtx); }   /* non published interface */

size_t ZSTD_decompressBegin(ZSTD_DCtx* dctx)
{
    dctx->expected = ZSTD_frameHeaderSize_min;
    dctx->stage = ZSTDds_getFrameHeaderSize;
    dctx->previousDstEnd = NULL;
    dctx->base = NULL;
    dctx->vBase = NULL;
    dctx->dictEnd = NULL;
    dctx->hufTable[0] = (HUF_DTable)((HufLog)*0x1000001);
    dctx->litEntropy = dctx->fseEntropy = 0;
    dctx->dictID = 0;
    { int i; for (i=0; i<ZSTD_REP_NUM; i++) dctx->rep[i] = repStartValue[i]; }
    return 0;
}

ZSTD_DCtx* ZSTD_createDCtx_advanced(ZSTD_customMem customMem)
{
    ZSTD_DCtx* dctx;

    if (!customMem.customAlloc && !customMem.customFree)
        customMem = defaultCustomMem;

    if (!customMem.customAlloc || !customMem.customFree)
        return NULL;

    dctx = (ZSTD_DCtx*) customMem.customAlloc(customMem.opaque, sizeof(ZSTD_DCtx));
    if (!dctx) return NULL;
    memcpy(&dctx->customMem, &customMem, sizeof(ZSTD_customMem));
    ZSTD_decompressBegin(dctx);
    return dctx;
}

ZSTD_DCtx* ZSTD_createDCtx(void)
{
    return ZSTD_createDCtx_advanced(defaultCustomMem);
}

size_t ZSTD_freeDCtx(ZSTD_DCtx* dctx)
{
    if (dctx==NULL) return 0;   /* support free on NULL */
    dctx->customMem.customFree(dctx->customMem.opaque, dctx);
    return 0;   /* reserved as a potential error code in the future */
}

void ZSTD_copyDCtx(ZSTD_DCtx* dstDCtx, const ZSTD_DCtx* srcDCtx)
{
    memcpy(dstDCtx, srcDCtx,
           sizeof(ZSTD_DCtx) - (ZSTD_BLOCKSIZE_MAX+WILDCOPY_OVERLENGTH + ZSTD_frameHeaderSize_max));  /* no need to copy workspace */
}


/*-*************************************************************
*   Decompression section
***************************************************************/

/* Frame format description
   Frame Header -  [ Block Header - Block ] - Frame End
   1) Frame Header
      - 4 bytes - Magic Number : ZSTD_MAGICNUMBER (defined within zstd_static.h)
      - 1 byte  - Frame Descriptor
   2) Block Header
      - 3 bytes, starting with a 2-bits descriptor
                 Uncompressed, Compressed, Frame End, unused
   3) Block
      See Block Format Description
   4) Frame End
      - 3 bytes, compatible with Block Header
*/


/* Frame descriptor

    // old
   1 byte - Alloc :
   bit 0-3 : windowLog - ZSTD_WINDOWLOG_ABSOLUTEMIN   (see zstd_internal.h)
   bit 4   : reserved for windowLog (must be zero)
   bit 5   : reserved (must be zero)
   bit 6-7 : Frame content size : unknown, 1 byte, 2 bytes, 8 bytes

   1 byte - checker :
   bit 0-1 : dictID (0, 1, 2 or 4 bytes)
   bit 2-7 : reserved (must be zero)

    // new
   1 byte - FrameHeaderDescription :
   bit 0-1 : dictID (0, 1, 2 or 4 bytes)
   bit 2   : checksumFlag
   bit 3   : reserved (must be zero)
   bit 4   : reserved (unused, can be any value)
   bit 5   : Single Segment (if 1, WindowLog byte is not present)
   bit 6-7 : FrameContentFieldSize (0, 2, 4, or 8)
             if (SkippedWindowLog && !FrameContentFieldsize) FrameContentFieldsize=1;

   Optional : WindowLog (0 or 1 byte)
   bit 0-2 : octal Fractional (1/8th)
   bit 3-7 : Power of 2, with 0 = 1 KB (up to 2 TB)

   Optional : dictID (0, 1, 2 or 4 bytes)
   Automatic adaptation
   0 : no dictID
   1 : 1 - 255
   2 : 256 - 65535
   4 : all other values

   Optional : content size (0, 1, 2, 4 or 8 bytes)
   0 : unknown          (fcfs==0 and swl==0)
   1 : 0-255 bytes      (fcfs==0 and swl==1)
   2 : 256 - 65535+256  (fcfs==1)
   4 : 0 - 4GB-1        (fcfs==2)
   8 : 0 - 16EB-1       (fcfs==3)
*/


/* Compressed Block, format description

   Block = Literal Section - Sequences Section
   Prerequisite : size of (compressed) block, maximum size of regenerated data

   1) Literal Section

   1.1) Header : 1-5 bytes
        flags: 2 bits
            00 compressed by Huff0
            01 unused
            10 is Raw (uncompressed)
            11 is Rle
            Note : using 01 => Huff0 with precomputed table ?
            Note : delta map ? => compressed ?

   1.1.1) Huff0-compressed literal block : 3-5 bytes
            srcSize < 1 KB => 3 bytes (2-2-10-10) => single stream
            srcSize < 1 KB => 3 bytes (2-2-10-10)
            srcSize < 16KB => 4 bytes (2-2-14-14)
            else           => 5 bytes (2-2-18-18)
            big endian convention

   1.1.2) Raw (uncompressed) literal block header : 1-3 bytes
        size :  5 bits: (IS_RAW<<6) + (0<<4) + size
               12 bits: (IS_RAW<<6) + (2<<4) + (size>>8)
                        size&255
               20 bits: (IS_RAW<<6) + (3<<4) + (size>>16)
                        size>>8&255
                        size&255

   1.1.3) Rle (repeated single byte) literal block header : 1-3 bytes
        size :  5 bits: (IS_RLE<<6) + (0<<4) + size
               12 bits: (IS_RLE<<6) + (2<<4) + (size>>8)
                        size&255
               20 bits: (IS_RLE<<6) + (3<<4) + (size>>16)
                        size>>8&255
                        size&255

   1.1.4) Huff0-compressed literal block, using precomputed CTables : 3-5 bytes
            srcSize < 1 KB => 3 bytes (2-2-10-10) => single stream
            srcSize < 1 KB => 3 bytes (2-2-10-10)
            srcSize < 16KB => 4 bytes (2-2-14-14)
            else           => 5 bytes (2-2-18-18)
            big endian convention

        1- CTable available (stored into workspace ?)
        2- Small input (fast heuristic ? Full comparison ? depend on clevel ?)


   1.2) Literal block content

   1.2.1) Huff0 block, using sizes from header
        See Huff0 format

   1.2.2) Huff0 block, using prepared table

   1.2.3) Raw content

   1.2.4) single byte


   2) Sequences section
      TO DO
*/

/** ZSTD_frameHeaderSize() :
*   srcSize must be >= ZSTD_frameHeaderSize_min.
*   @return : size of the Frame Header */
static size_t ZSTD_frameHeaderSize(const void* src, size_t srcSize)
{
    if (srcSize < ZSTD_frameHeaderSize_min) return ERROR(srcSize_wrong);
    {   BYTE const fhd = ((const BYTE*)src)[4];
        U32 const dictID= fhd & 3;
        U32 const directMode = (fhd >> 5) & 1;
        U32 const fcsId = fhd >> 6;
        return ZSTD_frameHeaderSize_min + !directMode + ZSTD_did_fieldSize[dictID] + ZSTD_fcs_fieldSize[fcsId]
                + (directMode && !ZSTD_fcs_fieldSize[fcsId]);
    }
}


/** ZSTD_getFrameParams() :
*   decode Frame Header, or require larger `srcSize`.
*   @return : 0, `fparamsPtr` is correctly filled,
*            >0, `srcSize` is too small, result is expected `srcSize`,
*             or an error code, which can be tested using ZSTD_isError() */
size_t ZSTD_getFrameParams(ZSTD_frameParams* fparamsPtr, const void* src, size_t srcSize)
{
    const BYTE* ip = (const BYTE*)src;

    if (srcSize < ZSTD_frameHeaderSize_min) return ZSTD_frameHeaderSize_min;
    if (MEM_readLE32(src) != ZSTD_MAGICNUMBER) {
        if ((MEM_readLE32(src) & 0xFFFFFFF0U) == ZSTD_MAGIC_SKIPPABLE_START) {
            if (srcSize < ZSTD_skippableHeaderSize) return ZSTD_skippableHeaderSize; /* magic number + skippable frame length */
            memset(fparamsPtr, 0, sizeof(*fparamsPtr));
            fparamsPtr->frameContentSize = MEM_readLE32((const char *)src + 4);
            fparamsPtr->windowSize = 0; /* windowSize==0 means a frame is skippable */
            return 0;
        }
        return ERROR(prefix_unknown);
    }

    /* ensure there is enough `srcSize` to fully read/decode frame header */
    { size_t const fhsize = ZSTD_frameHeaderSize(src, srcSize);
      if (srcSize < fhsize) return fhsize; }

    {   BYTE const fhdByte = ip[4];
        size_t pos = 5;
        U32 const dictIDSizeCode = fhdByte&3;
        U32 const checksumFlag = (fhdByte>>2)&1;
        U32 const directMode = (fhdByte>>5)&1;
        U32 const fcsID = fhdByte>>6;
        U32 const windowSizeMax = 1U << ZSTD_WINDOWLOG_MAX;
        U32 windowSize = 0;
        U32 dictID = 0;
        U64 frameContentSize = 0;
        if ((fhdByte & 0x08) != 0) return ERROR(frameParameter_unsupported);   /* reserved bits, which must be zero */
        if (!directMode) {
            BYTE const wlByte = ip[pos++];
            U32 const windowLog = (wlByte >> 3) + ZSTD_WINDOWLOG_ABSOLUTEMIN;
            if (windowLog > ZSTD_WINDOWLOG_MAX) return ERROR(frameParameter_unsupported);
            windowSize = (1U << windowLog);
            windowSize += (windowSize >> 3) * (wlByte&7);
        }

        switch(dictIDSizeCode)
        {
            default:   /* impossible */
            case 0 : break;
            case 1 : dictID = ip[pos]; pos++; break;
            case 2 : dictID = MEM_readLE16(ip+pos); pos+=2; break;
            case 3 : dictID = MEM_readLE32(ip+pos); pos+=4; break;
        }
        switch(fcsID)
        {
            default:   /* impossible */
            case 0 : if (directMode) frameContentSize = ip[pos]; break;
            case 1 : frameContentSize = MEM_readLE16(ip+pos)+256; break;
            case 2 : frameContentSize = MEM_readLE32(ip+pos); break;
            case 3 : frameContentSize = MEM_readLE64(ip+pos); break;
        }
        if (!windowSize) windowSize = (U32)frameContentSize;
        if (windowSize > windowSizeMax) return ERROR(frameParameter_unsupported);
        fparamsPtr->frameContentSize = frameContentSize;
        fparamsPtr->windowSize = windowSize;
        fparamsPtr->dictID = dictID;
        fparamsPtr->checksumFlag = checksumFlag;
    }
    return 0;
}


/** ZSTD_decodeFrameHeader() :
*   `srcSize` must be the size provided by ZSTD_frameHeaderSize().
*   @return : 0 if success, or an error code, which can be tested using ZSTD_isError() */
static size_t ZSTD_decodeFrameHeader(ZSTD_DCtx* dctx, const void* src, size_t srcSize)
{
    size_t const result = ZSTD_getFrameParams(&(dctx->fParams), src, srcSize);
    if (dctx->fParams.dictID && (dctx->dictID != dctx->fParams.dictID)) return ERROR(dictionary_wrong);
    if (dctx->fParams.checksumFlag) XXH64_reset(&dctx->xxhState, 0);
    return result;
}


typedef struct
{
    blockType_t blockType;
    U32 origSize;
} blockProperties_t;

/*! ZSTD_getcBlockSize() :
*   Provides the size of compressed block from block header `src` */
size_t ZSTD_getcBlockSize(const void* src, size_t srcSize, blockProperties_t* bpPtr)
{
    const BYTE* const in = (const BYTE* const)src;
    U32 cSize;

    if (srcSize < ZSTD_blockHeaderSize) return ERROR(srcSize_wrong);

    bpPtr->blockType = (blockType_t)((*in) >> 6);
    cSize = in[2] + (in[1]<<8) + ((in[0] & 7)<<16);
    bpPtr->origSize = (bpPtr->blockType == bt_rle) ? cSize : 0;

    if (bpPtr->blockType == bt_end) return 0;
    if (bpPtr->blockType == bt_rle) return 1;
    return cSize;
}


static size_t ZSTD_copyRawBlock(void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
    if (srcSize > dstCapacity) return ERROR(dstSize_tooSmall);
    memcpy(dst, src, srcSize);
    return srcSize;
}


/*! ZSTD_decodeLiteralsBlock() :
    @return : nb of bytes read from src (< srcSize ) */
size_t ZSTD_decodeLiteralsBlock(ZSTD_DCtx* dctx,
                          const void* src, size_t srcSize)   /* note : srcSize < BLOCKSIZE */
{
    const BYTE* const istart = (const BYTE*) src;
    litBlockType_t lbt;

    if (srcSize < MIN_CBLOCK_SIZE) return ERROR(corruption_detected);
    lbt = (litBlockType_t)(istart[0]>> 6);

    switch(lbt)
    {
    case lbt_huffman:
        {   size_t litSize, litCSize, singleStream=0;
            U32 lhSize = ((istart[0]) >> 4) & 3;
            if (srcSize < 5) return ERROR(corruption_detected);   /* srcSize >= MIN_CBLOCK_SIZE == 3; here we need up to 5 for lhSize, + cSize (+nbSeq) */
            switch(lhSize)
            {
            case 0: case 1: default:   /* note : default is impossible, since lhSize into [0..3] */
                /* 2 - 2 - 10 - 10 */
                lhSize=3;
                singleStream = istart[0] & 16;
                litSize  = ((istart[0] & 15) << 6) + (istart[1] >> 2);
                litCSize = ((istart[1] &  3) << 8) + istart[2];
                break;
            case 2:
                /* 2 - 2 - 14 - 14 */
                lhSize=4;
                litSize  = ((istart[0] & 15) << 10) + (istart[1] << 2) + (istart[2] >> 6);
                litCSize = ((istart[2] & 63) <<  8) + istart[3];
                break;
            case 3:
                /* 2 - 2 - 18 - 18 */
                lhSize=5;
                litSize  = ((istart[0] & 15) << 14) + (istart[1] << 6) + (istart[2] >> 2);
                litCSize = ((istart[2] &  3) << 16) + (istart[3] << 8) + istart[4];
                break;
            }
            if (litSize > ZSTD_BLOCKSIZE_MAX) return ERROR(corruption_detected);
            if (litCSize + lhSize > srcSize) return ERROR(corruption_detected);

            if (HUF_isError(singleStream ?
                            HUF_decompress1X2_DCtx(dctx->hufTable, dctx->litBuffer, litSize, istart+lhSize, litCSize) :
                            HUF_decompress4X_hufOnly (dctx->hufTable, dctx->litBuffer, litSize, istart+lhSize, litCSize) ))
                return ERROR(corruption_detected);

            dctx->litPtr = dctx->litBuffer;
            dctx->litBufSize = ZSTD_BLOCKSIZE_MAX+8;
            dctx->litSize = litSize;
            dctx->litEntropy = 1;
            return litCSize + lhSize;
        }
    case lbt_repeat:
        {   size_t litSize, litCSize;
            U32 lhSize = ((istart[0]) >> 4) & 3;
            if (lhSize != 1)  /* only case supported for now : small litSize, single stream */
                return ERROR(corruption_detected);
            if (dctx->litEntropy==0)
                return ERROR(dictionary_corrupted);

            /* 2 - 2 - 10 - 10 */
            lhSize=3;
            litSize  = ((istart[0] & 15) << 6) + (istart[1] >> 2);
            litCSize = ((istart[1] &  3) << 8) + istart[2];
            if (litCSize + lhSize > srcSize) return ERROR(corruption_detected);

            {   size_t const errorCode = HUF_decompress1X4_usingDTable(dctx->litBuffer, litSize, istart+lhSize, litCSize, dctx->hufTable);
                if (HUF_isError(errorCode)) return ERROR(corruption_detected);
            }
            dctx->litPtr = dctx->litBuffer;
            dctx->litBufSize = ZSTD_BLOCKSIZE_MAX+WILDCOPY_OVERLENGTH;
            dctx->litSize = litSize;
            return litCSize + lhSize;
        }
    case lbt_raw:
        {   size_t litSize;
            U32 lhSize = ((istart[0]) >> 4) & 3;
            switch(lhSize)
            {
            case 0: case 1: default:   /* note : default is impossible, since lhSize into [0..3] */
                lhSize=1;
                litSize = istart[0] & 31;
                break;
            case 2:
                litSize = ((istart[0] & 15) << 8) + istart[1];
                break;
            case 3:
                litSize = ((istart[0] & 15) << 16) + (istart[1] << 8) + istart[2];
                break;
            }

            if (lhSize+litSize+WILDCOPY_OVERLENGTH > srcSize) {  /* risk reading beyond src buffer with wildcopy */
                if (litSize+lhSize > srcSize) return ERROR(corruption_detected);
                memcpy(dctx->litBuffer, istart+lhSize, litSize);
                dctx->litPtr = dctx->litBuffer;
                dctx->litBufSize = ZSTD_BLOCKSIZE_MAX+8;
                dctx->litSize = litSize;
                return lhSize+litSize;
            }
            /* direct reference into compressed stream */
            dctx->litPtr = istart+lhSize;
            dctx->litBufSize = srcSize-lhSize;
            dctx->litSize = litSize;
            return lhSize+litSize;
        }
    case lbt_rle:
        {   size_t litSize;
            U32 lhSize = ((istart[0]) >> 4) & 3;
            switch(lhSize)
            {
            case 0: case 1: default:   /* note : default is impossible, since lhSize into [0..3] */
                lhSize = 1;
                litSize = istart[0] & 31;
                break;
            case 2:
                litSize = ((istart[0] & 15) << 8) + istart[1];
                break;
            case 3:
                litSize = ((istart[0] & 15) << 16) + (istart[1] << 8) + istart[2];
                if (srcSize<4) return ERROR(corruption_detected);   /* srcSize >= MIN_CBLOCK_SIZE == 3; here we need lhSize+1 = 4 */
                break;
            }
            if (litSize > ZSTD_BLOCKSIZE_MAX) return ERROR(corruption_detected);
            memset(dctx->litBuffer, istart[lhSize], litSize);
            dctx->litPtr = dctx->litBuffer;
            dctx->litBufSize = ZSTD_BLOCKSIZE_MAX+WILDCOPY_OVERLENGTH;
            dctx->litSize = litSize;
            return lhSize+1;
        }
    default:
        return ERROR(corruption_detected);   /* impossible */
    }
}


/*! ZSTD_buildSeqTable() :
    @return : nb bytes read from src,
              or an error code if it fails, testable with ZSTD_isError()
*/
FORCE_INLINE size_t ZSTD_buildSeqTable(FSE_DTable* DTable, U32 type, U32 max, U32 maxLog,
                                 const void* src, size_t srcSize,
                                 const S16* defaultNorm, U32 defaultLog, U32 flagRepeatTable)
{
    switch(type)
    {
    case FSE_ENCODING_RLE :
        if (!srcSize) return ERROR(srcSize_wrong);
        if ( (*(const BYTE*)src) > max) return ERROR(corruption_detected);
        FSE_buildDTable_rle(DTable, *(const BYTE*)src);   /* if *src > max, data is corrupted */
        return 1;
    case FSE_ENCODING_RAW :
        FSE_buildDTable(DTable, defaultNorm, max, defaultLog);
        return 0;
    case FSE_ENCODING_STATIC:
        if (!flagRepeatTable) return ERROR(corruption_detected);
        return 0;
    default :   /* impossible */
    case FSE_ENCODING_DYNAMIC :
        {   U32 tableLog;
            S16 norm[MaxSeq+1];
            size_t const headerSize = FSE_readNCount(norm, &max, &tableLog, src, srcSize);
            if (FSE_isError(headerSize)) return ERROR(corruption_detected);
            if (tableLog > maxLog) return ERROR(corruption_detected);
            FSE_buildDTable(DTable, norm, max, tableLog);
            return headerSize;
    }   }
}


size_t ZSTD_decodeSeqHeaders(int* nbSeqPtr,
                             FSE_DTable* DTableLL, FSE_DTable* DTableML, FSE_DTable* DTableOffb, U32 flagRepeatTable,
                             const void* src, size_t srcSize)
{
    const BYTE* const istart = (const BYTE* const)src;
    const BYTE* const iend = istart + srcSize;
    const BYTE* ip = istart;

    /* check */
    if (srcSize < MIN_SEQUENCES_SIZE) return ERROR(srcSize_wrong);

    /* SeqHead */
    {   int nbSeq = *ip++;
        if (!nbSeq) { *nbSeqPtr=0; return 1; }
        if (nbSeq > 0x7F) {
            if (nbSeq == 0xFF)
                nbSeq = MEM_readLE16(ip) + LONGNBSEQ, ip+=2;
            else
                nbSeq = ((nbSeq-0x80)<<8) + *ip++;
        }
        *nbSeqPtr = nbSeq;
    }

    /* FSE table descriptors */
    {   U32 const LLtype  = *ip >> 6;
        U32 const Offtype = (*ip >> 4) & 3;
        U32 const MLtype  = (*ip >> 2) & 3;
        ip++;

        /* check */
        if (ip > iend-3) return ERROR(srcSize_wrong); /* min : all 3 are "raw", hence no header, but at least xxLog bits per type */

        /* Build DTables */
        {   size_t const bhSize = ZSTD_buildSeqTable(DTableLL, LLtype, MaxLL, LLFSELog, ip, iend-ip, LL_defaultNorm, LL_defaultNormLog, flagRepeatTable);
            if (ZSTD_isError(bhSize)) return ERROR(corruption_detected);
            ip += bhSize;
        }
        {   size_t const bhSize = ZSTD_buildSeqTable(DTableOffb, Offtype, MaxOff, OffFSELog, ip, iend-ip, OF_defaultNorm, OF_defaultNormLog, flagRepeatTable);
            if (ZSTD_isError(bhSize)) return ERROR(corruption_detected);
            ip += bhSize;
        }
        {   size_t const bhSize = ZSTD_buildSeqTable(DTableML, MLtype, MaxML, MLFSELog, ip, iend-ip, ML_defaultNorm, ML_defaultNormLog, flagRepeatTable);
            if (ZSTD_isError(bhSize)) return ERROR(corruption_detected);
            ip += bhSize;
    }   }

    return ip-istart;
}


typedef struct {
    size_t litLength;
    size_t matchLength;
    size_t offset;
} seq_t;

typedef struct {
    BIT_DStream_t DStream;
    FSE_DState_t stateLL;
    FSE_DState_t stateOffb;
    FSE_DState_t stateML;
    size_t prevOffset[ZSTD_REP_INIT];
} seqState_t;


static seq_t ZSTD_decodeSequence(seqState_t* seqState)
{
    seq_t seq;

    U32 const llCode = FSE_peekSymbol(&(seqState->stateLL));
    U32 const mlCode = FSE_peekSymbol(&(seqState->stateML));
    U32 const ofCode = FSE_peekSymbol(&(seqState->stateOffb));   /* <= maxOff, by table construction */

    U32 const llBits = LL_bits[llCode];
    U32 const mlBits = ML_bits[mlCode];
    U32 const ofBits = ofCode;
    U32 const totalBits = llBits+mlBits+ofBits;

    static const U32 LL_base[MaxLL+1] = {
                             0,  1,  2,  3,  4,  5,  6,  7,  8,  9,   10,    11,    12,    13,    14,     15,
                            16, 18, 20, 22, 24, 28, 32, 40, 48, 64, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000,
                            0x2000, 0x4000, 0x8000, 0x10000 };

    static const U32 ML_base[MaxML+1] = {
                             0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,   11,    12,    13,    14,    15,
                            16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,   27,    28,    29,    30,    31,
                            32, 34, 36, 38, 40, 44, 48, 56, 64, 80, 96, 0x80, 0x100, 0x200, 0x400, 0x800,
                            0x1000, 0x2000, 0x4000, 0x8000, 0x10000 };

    static const U32 OF_base[MaxOff+1] = {
                 0,        1,       3,       7,     0xF,     0x1F,     0x3F,     0x7F,
                 0xFF,   0x1FF,   0x3FF,   0x7FF,   0xFFF,   0x1FFF,   0x3FFF,   0x7FFF,
                 0xFFFF, 0x1FFFF, 0x3FFFF, 0x7FFFF, 0xFFFFF, 0x1FFFFF, 0x3FFFFF, 0x7FFFFF,
                 0xFFFFFF, 0x1FFFFFF, 0x3FFFFFF, /*fake*/ 1, 1 };

    /* sequence */
    {   size_t offset;
        if (!ofCode)
            offset = 0;
        else {
            offset = OF_base[ofCode] + BIT_readBits(&(seqState->DStream), ofBits);   /* <=  26 bits */
            if (MEM_32bits()) BIT_reloadDStream(&(seqState->DStream));
        }

        if (offset < ZSTD_REP_NUM) {
            if (llCode == 0 && offset <= 1) offset = 1-offset;

            if (offset != 0) {
                size_t temp = seqState->prevOffset[offset];
                if (offset != 1) {
                    seqState->prevOffset[2] = seqState->prevOffset[1];
                }
                seqState->prevOffset[1] = seqState->prevOffset[0];
                seqState->prevOffset[0] = offset = temp;

            } else {
                offset = seqState->prevOffset[0];
            }
        } else {
            offset -= ZSTD_REP_MOVE;
            seqState->prevOffset[2] = seqState->prevOffset[1];
            seqState->prevOffset[1] = seqState->prevOffset[0];
            seqState->prevOffset[0] = offset;
        }
        seq.offset = offset;
    }

    seq.matchLength = ML_base[mlCode] + MINMATCH + ((mlCode>31) ? BIT_readBits(&(seqState->DStream), mlBits) : 0);   /* <=  16 bits */
    if (MEM_32bits() && (mlBits+llBits>24)) BIT_reloadDStream(&(seqState->DStream));

    seq.litLength = LL_base[llCode] + ((llCode>15) ? BIT_readBits(&(seqState->DStream), llBits) : 0);   /* <=  16 bits */
    if (MEM_32bits() |
       (totalBits > 64 - 7 - (LLFSELog+MLFSELog+OffFSELog)) ) BIT_reloadDStream(&(seqState->DStream));

    /* ANS state update */
    FSE_updateState(&(seqState->stateLL), &(seqState->DStream));   /* <=  9 bits */
    FSE_updateState(&(seqState->stateML), &(seqState->DStream));   /* <=  9 bits */
    if (MEM_32bits()) BIT_reloadDStream(&(seqState->DStream));     /* <= 18 bits */
    FSE_updateState(&(seqState->stateOffb), &(seqState->DStream)); /* <=  8 bits */

    return seq;
}


FORCE_INLINE
size_t ZSTD_execSequence(BYTE* op,
                                BYTE* const oend, seq_t sequence,
                                const BYTE** litPtr, const BYTE* const litLimit_w,
                                const BYTE* const base, const BYTE* const vBase, const BYTE* const dictEnd)
{
    BYTE* const oLitEnd = op + sequence.litLength;
    size_t const sequenceLength = sequence.litLength + sequence.matchLength;
    BYTE* const oMatchEnd = op + sequenceLength;   /* risk : address space overflow (32-bits) */
    BYTE* const oend_w = oend-WILDCOPY_OVERLENGTH;
    const BYTE* const iLitEnd = *litPtr + sequence.litLength;
    const BYTE* match = oLitEnd - sequence.offset;

    /* check */
    if ((oLitEnd>oend_w) | (oMatchEnd>oend)) return ERROR(dstSize_tooSmall); /* last match must start at a minimum distance of WILDCOPY_OVERLENGTH from oend */
    if (iLitEnd > litLimit_w) return ERROR(corruption_detected);   /* over-read beyond lit buffer */

    /* copy Literals */
    ZSTD_wildcopy(op, *litPtr, sequence.litLength);   /* note : since oLitEnd <= oend-WILDCOPY_OVERLENGTH, no risk of overwrite beyond oend */
    op = oLitEnd;
    *litPtr = iLitEnd;   /* update for next sequence */

    /* copy Match */
    if (sequence.offset > (size_t)(oLitEnd - base)) {
        /* offset beyond prefix */
        if (sequence.offset > (size_t)(oLitEnd - vBase)) return ERROR(corruption_detected);
        match = dictEnd - (base-match);
        if (match + sequence.matchLength <= dictEnd) {
            memmove(oLitEnd, match, sequence.matchLength);
            return sequenceLength;
        }
        /* span extDict & currentPrefixSegment */
        {   size_t const length1 = dictEnd - match;
            memmove(oLitEnd, match, length1);
            op = oLitEnd + length1;
            sequence.matchLength -= length1;
            match = base;
    }   }

    /* match within prefix */
    if (sequence.offset < 8) {
        /* close range match, overlap */
        static const U32 dec32table[] = { 0, 1, 2, 1, 4, 4, 4, 4 };   /* added */
        static const int dec64table[] = { 8, 8, 8, 7, 8, 9,10,11 };   /* substracted */
        int const sub2 = dec64table[sequence.offset];
        op[0] = match[0];
        op[1] = match[1];
        op[2] = match[2];
        op[3] = match[3];
        match += dec32table[sequence.offset];
        ZSTD_copy4(op+4, match);
        match -= sub2;
    } else {
        ZSTD_copy8(op, match);
    }
    op += 8; match += 8;

    if (oMatchEnd > oend-(16-MINMATCH)) {
        if (op < oend_w) {
            ZSTD_wildcopy(op, match, oend_w - op);
            match += oend_w - op;
            op = oend_w;
        }
        while (op < oMatchEnd) *op++ = *match++;
    } else {
        ZSTD_wildcopy(op, match, sequence.matchLength-8);   /* works even if matchLength < 8 */
    }
    return sequenceLength;
}


static size_t ZSTD_decompressSequences(
                               ZSTD_DCtx* dctx,
                               void* dst, size_t maxDstSize,
                         const void* seqStart, size_t seqSize)
{
    const BYTE* ip = (const BYTE*)seqStart;
    const BYTE* const iend = ip + seqSize;
    BYTE* const ostart = (BYTE* const)dst;
    BYTE* const oend = ostart + maxDstSize;
    BYTE* op = ostart;
    const BYTE* litPtr = dctx->litPtr;
    const BYTE* const litLimit_w = litPtr + dctx->litBufSize - WILDCOPY_OVERLENGTH;
    const BYTE* const litEnd = litPtr + dctx->litSize;
    FSE_DTable* DTableLL = dctx->LLTable;
    FSE_DTable* DTableML = dctx->MLTable;
    FSE_DTable* DTableOffb = dctx->OffTable;
    const BYTE* const base = (const BYTE*) (dctx->base);
    const BYTE* const vBase = (const BYTE*) (dctx->vBase);
    const BYTE* const dictEnd = (const BYTE*) (dctx->dictEnd);
    int nbSeq;

    /* Build Decoding Tables */
    {   size_t const seqHSize = ZSTD_decodeSeqHeaders(&nbSeq, DTableLL, DTableML, DTableOffb, dctx->fseEntropy, ip, seqSize);
        if (ZSTD_isError(seqHSize)) return seqHSize;
        ip += seqHSize;
    }

    /* Regen sequences */
    if (nbSeq) {
        seqState_t seqState;
        dctx->fseEntropy = 1;
        { U32 i; for (i=0; i<ZSTD_REP_INIT; i++) seqState.prevOffset[i] = dctx->rep[i]; }
        { size_t const errorCode = BIT_initDStream(&(seqState.DStream), ip, iend-ip);
          if (ERR_isError(errorCode)) return ERROR(corruption_detected); }
        FSE_initDState(&(seqState.stateLL), &(seqState.DStream), DTableLL);
        FSE_initDState(&(seqState.stateOffb), &(seqState.DStream), DTableOffb);
        FSE_initDState(&(seqState.stateML), &(seqState.DStream), DTableML);

        for ( ; (BIT_reloadDStream(&(seqState.DStream)) <= BIT_DStream_completed) && nbSeq ; ) {
            nbSeq--;
            {   seq_t const sequence = ZSTD_decodeSequence(&seqState);
                size_t const oneSeqSize = ZSTD_execSequence(op, oend, sequence, &litPtr, litLimit_w, base, vBase, dictEnd);
                if (ZSTD_isError(oneSeqSize)) return oneSeqSize;
                op += oneSeqSize;
        }   }

        /* check if reached exact end */
        if (nbSeq) return ERROR(corruption_detected);
        /* save reps for next block */
        { U32 i; for (i=0; i<ZSTD_REP_INIT; i++) dctx->rep[i] = (U32)(seqState.prevOffset[i]); }
    }

    /* last literal segment */
    {   size_t const lastLLSize = litEnd - litPtr;
        //if (litPtr > litEnd) return ERROR(corruption_detected);   /* too many literals already used */
        if (lastLLSize > (size_t)(oend-op)) return ERROR(dstSize_tooSmall);
        memcpy(op, litPtr, lastLLSize);
        op += lastLLSize;
    }

    return op-ostart;
}


static void ZSTD_checkContinuity(ZSTD_DCtx* dctx, const void* dst)
{
    if (dst != dctx->previousDstEnd) {   /* not contiguous */
        dctx->dictEnd = dctx->previousDstEnd;
        dctx->vBase = (const char*)dst - ((const char*)(dctx->previousDstEnd) - (const char*)(dctx->base));
        dctx->base = dst;
        dctx->previousDstEnd = dst;
    }
}


static size_t ZSTD_decompressBlock_internal(ZSTD_DCtx* dctx,
                            void* dst, size_t dstCapacity,
                      const void* src, size_t srcSize)
{   /* blockType == blockCompressed */
    const BYTE* ip = (const BYTE*)src;

    if (srcSize >= ZSTD_BLOCKSIZE_MAX) return ERROR(srcSize_wrong);

    /* Decode literals sub-block */
    {   size_t const litCSize = ZSTD_decodeLiteralsBlock(dctx, src, srcSize);
        if (ZSTD_isError(litCSize)) return litCSize;
        ip += litCSize;
        srcSize -= litCSize;
    }
    return ZSTD_decompressSequences(dctx, dst, dstCapacity, ip, srcSize);
}


size_t ZSTD_decompressBlock(ZSTD_DCtx* dctx,
                            void* dst, size_t dstCapacity,
                      const void* src, size_t srcSize)
{
    ZSTD_checkContinuity(dctx, dst);
    return ZSTD_decompressBlock_internal(dctx, dst, dstCapacity, src, srcSize);
}


size_t ZSTD_generateNxByte(void* dst, size_t dstCapacity, BYTE byte, size_t length)
{
    if (length > dstCapacity) return ERROR(dstSize_tooSmall);
    memset(dst, byte, length);
    return length;
}


/*! ZSTD_decompressFrame() :
*   `dctx` must be properly initialized */
static size_t ZSTD_decompressFrame(ZSTD_DCtx* dctx,
                                 void* dst, size_t dstCapacity,
                                 const void* src, size_t srcSize)
{
    const BYTE* ip = (const BYTE*)src;
    const BYTE* const iend = ip + srcSize;
    BYTE* const ostart = (BYTE* const)dst;
    BYTE* const oend = ostart + dstCapacity;
    BYTE* op = ostart;
    size_t remainingSize = srcSize;

    /* check */
    if (srcSize < ZSTD_frameHeaderSize_min+ZSTD_blockHeaderSize) return ERROR(srcSize_wrong);

    /* Frame Header */
    {   size_t const frameHeaderSize = ZSTD_frameHeaderSize(src, ZSTD_frameHeaderSize_min);
        if (ZSTD_isError(frameHeaderSize)) return frameHeaderSize;
        if (srcSize < frameHeaderSize+ZSTD_blockHeaderSize) return ERROR(srcSize_wrong);
        if (ZSTD_decodeFrameHeader(dctx, src, frameHeaderSize)) return ERROR(corruption_detected);
        ip += frameHeaderSize; remainingSize -= frameHeaderSize;
    }

    /* Loop on each block */
    while (1) {
        size_t decodedSize;
        blockProperties_t blockProperties;
        size_t const cBlockSize = ZSTD_getcBlockSize(ip, iend-ip, &blockProperties);
        if (ZSTD_isError(cBlockSize)) return cBlockSize;

        ip += ZSTD_blockHeaderSize;
        remainingSize -= ZSTD_blockHeaderSize;
        if (cBlockSize > remainingSize) return ERROR(srcSize_wrong);

        switch(blockProperties.blockType)
        {
        case bt_compressed:
            decodedSize = ZSTD_decompressBlock_internal(dctx, op, oend-op, ip, cBlockSize);
            break;
        case bt_raw :
            decodedSize = ZSTD_copyRawBlock(op, oend-op, ip, cBlockSize);
            break;
        case bt_rle :
            decodedSize = ZSTD_generateNxByte(op, oend-op, *ip, blockProperties.origSize);
            break;
        case bt_end :
            /* end of frame */
            if (remainingSize) return ERROR(srcSize_wrong);
            decodedSize = 0;
            break;
        default:
            return ERROR(GENERIC);   /* impossible */
        }
        if (cBlockSize == 0) break;   /* bt_end */

        if (ZSTD_isError(decodedSize)) return decodedSize;
        if (dctx->fParams.checksumFlag) XXH64_update(&dctx->xxhState, op, decodedSize);
        op += decodedSize;
        ip += cBlockSize;
        remainingSize -= cBlockSize;
    }

    return op-ostart;
}


/*! ZSTD_decompress_usingPreparedDCtx() :
*   Same as ZSTD_decompress_usingDict, but using a reference context `preparedDCtx`, where dictionary has been loaded.
*   It avoids reloading the dictionary each time.
*   `preparedDCtx` must have been properly initialized using ZSTD_decompressBegin_usingDict().
*   Requires 2 contexts : 1 for reference (preparedDCtx), which will not be modified, and 1 to run the decompression operation (dctx) */
size_t ZSTD_decompress_usingPreparedDCtx(ZSTD_DCtx* dctx, const ZSTD_DCtx* refDCtx,
                                         void* dst, size_t dstCapacity,
                                   const void* src, size_t srcSize)
{
    ZSTD_copyDCtx(dctx, refDCtx);
    ZSTD_checkContinuity(dctx, dst);
    return ZSTD_decompressFrame(dctx, dst, dstCapacity, src, srcSize);
}


size_t ZSTD_decompress_usingDict(ZSTD_DCtx* dctx,
                                 void* dst, size_t dstCapacity,
                                 const void* src, size_t srcSize,
                                 const void* dict, size_t dictSize)
{
#if defined(ZSTD_LEGACY_SUPPORT) && (ZSTD_LEGACY_SUPPORT==1)
    {   U32 const magicNumber = MEM_readLE32(src);
        if (ZSTD_isLegacy(magicNumber))
            return ZSTD_decompressLegacy(dst, dstCapacity, src, srcSize, dict, dictSize, magicNumber);
    }
#endif
    ZSTD_decompressBegin_usingDict(dctx, dict, dictSize);
    ZSTD_checkContinuity(dctx, dst);
    return ZSTD_decompressFrame(dctx, dst, dstCapacity, src, srcSize);
}


size_t ZSTD_decompressDCtx(ZSTD_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
    return ZSTD_decompress_usingDict(dctx, dst, dstCapacity, src, srcSize, NULL, 0);
}


size_t ZSTD_decompress(void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
#if defined(ZSTD_HEAPMODE) && (ZSTD_HEAPMODE==1)
    size_t regenSize;
    ZSTD_DCtx* const dctx = ZSTD_createDCtx();
    if (dctx==NULL) return ERROR(memory_allocation);
    regenSize = ZSTD_decompressDCtx(dctx, dst, dstCapacity, src, srcSize);
    ZSTD_freeDCtx(dctx);
    return regenSize;
#else   /* stack mode */
    ZSTD_DCtx dctx;
    return ZSTD_decompressDCtx(&dctx, dst, dstCapacity, src, srcSize);
#endif
}


/*_******************************
*  Streaming Decompression API
********************************/
size_t ZSTD_nextSrcSizeToDecompress(ZSTD_DCtx* dctx)
{
    return dctx->expected;
}

int ZSTD_isSkipFrame(ZSTD_DCtx* dctx)
{
    return dctx->stage == ZSTDds_skipFrame;
}

/** ZSTD_decompressContinue() :
*   @return : nb of bytes generated into `dst` (necessarily <= `dstCapacity)
*             or an error code, which can be tested using ZSTD_isError() */
size_t ZSTD_decompressContinue(ZSTD_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
    /* Sanity check */
    if (srcSize != dctx->expected) return ERROR(srcSize_wrong);
    if (dstCapacity) ZSTD_checkContinuity(dctx, dst);

    switch (dctx->stage)
    {
    case ZSTDds_getFrameHeaderSize :
        if (srcSize != ZSTD_frameHeaderSize_min) return ERROR(srcSize_wrong);   /* impossible */
        if ((MEM_readLE32(src) & 0xFFFFFFF0U) == ZSTD_MAGIC_SKIPPABLE_START) {
            memcpy(dctx->headerBuffer, src, ZSTD_frameHeaderSize_min);
            dctx->expected = ZSTD_skippableHeaderSize - ZSTD_frameHeaderSize_min; /* magic number + skippable frame length */
            dctx->stage = ZSTDds_decodeSkippableHeader;
            return 0;
        }
        dctx->headerSize = ZSTD_frameHeaderSize(src, ZSTD_frameHeaderSize_min);
        if (ZSTD_isError(dctx->headerSize)) return dctx->headerSize;
        memcpy(dctx->headerBuffer, src, ZSTD_frameHeaderSize_min);
        if (dctx->headerSize > ZSTD_frameHeaderSize_min) {
            dctx->expected = dctx->headerSize - ZSTD_frameHeaderSize_min;
            dctx->stage = ZSTDds_decodeFrameHeader;
            return 0;
        }
        dctx->expected = 0;   /* not necessary to copy more */

    case ZSTDds_decodeFrameHeader:
        {   size_t result;
            memcpy(dctx->headerBuffer + ZSTD_frameHeaderSize_min, src, dctx->expected);
            result = ZSTD_decodeFrameHeader(dctx, dctx->headerBuffer, dctx->headerSize);
            if (ZSTD_isError(result)) return result;
            dctx->expected = ZSTD_blockHeaderSize;
            dctx->stage = ZSTDds_decodeBlockHeader;
            return 0;
        }
    case ZSTDds_decodeBlockHeader:
        {   blockProperties_t bp;
            size_t const cBlockSize = ZSTD_getcBlockSize(src, ZSTD_blockHeaderSize, &bp);
            if (ZSTD_isError(cBlockSize)) return cBlockSize;
            if (bp.blockType == bt_end) {
                if (dctx->fParams.checksumFlag) {
                    U64 const h64 = XXH64_digest(&dctx->xxhState);
                    U32 const h32 = (U32)(h64>>11) & ((1<<22)-1);
                    const BYTE* const ip = (const BYTE*)src;
                    U32 const check32 = ip[2] + (ip[1] << 8) + ((ip[0] & 0x3F) << 16);
                    if (check32 != h32) return ERROR(checksum_wrong);
                }
                dctx->expected = 0;
                dctx->stage = ZSTDds_getFrameHeaderSize;
            } else {
                dctx->expected = cBlockSize;
                dctx->bType = bp.blockType;
                dctx->stage = ZSTDds_decompressBlock;
            }
            return 0;
        }
    case ZSTDds_decompressBlock:
        {   size_t rSize;
            switch(dctx->bType)
            {
            case bt_compressed:
                rSize = ZSTD_decompressBlock_internal(dctx, dst, dstCapacity, src, srcSize);
                break;
            case bt_raw :
                rSize = ZSTD_copyRawBlock(dst, dstCapacity, src, srcSize);
                break;
            case bt_rle :
                return ERROR(GENERIC);   /* not yet handled */
                break;
            case bt_end :   /* should never happen (filtered at phase 1) */
                rSize = 0;
                break;
            default:
                return ERROR(GENERIC);   /* impossible */
            }
            dctx->stage = ZSTDds_decodeBlockHeader;
            dctx->expected = ZSTD_blockHeaderSize;
            dctx->previousDstEnd = (char*)dst + rSize;
            if (ZSTD_isError(rSize)) return rSize;
            if (dctx->fParams.checksumFlag) XXH64_update(&dctx->xxhState, dst, rSize);
            return rSize;
        }
    case ZSTDds_decodeSkippableHeader:
        {   memcpy(dctx->headerBuffer + ZSTD_frameHeaderSize_min, src, dctx->expected);
            dctx->expected = MEM_readLE32(dctx->headerBuffer + 4);
            dctx->stage = ZSTDds_skipFrame;
            return 0;
        }
    case ZSTDds_skipFrame:
        {   dctx->expected = 0;
            dctx->stage = ZSTDds_getFrameHeaderSize;
            return 0;
        }
    default:
        return ERROR(GENERIC);   /* impossible */
    }
}


static void ZSTD_refDictContent(ZSTD_DCtx* dctx, const void* dict, size_t dictSize)
{
    dctx->dictEnd = dctx->previousDstEnd;
    dctx->vBase = (const char*)dict - ((const char*)(dctx->previousDstEnd) - (const char*)(dctx->base));
    dctx->base = dict;
    dctx->previousDstEnd = (const char*)dict + dictSize;
}

static size_t ZSTD_loadEntropy(ZSTD_DCtx* dctx, const void* const dict, size_t const dictSize)
{
    const BYTE* dictPtr = (const BYTE*)dict;
    const BYTE* const dictEnd = dictPtr + dictSize;

    {   size_t const hSize = HUF_readDTableX4(dctx->hufTable, dict, dictSize);
        if (HUF_isError(hSize)) return ERROR(dictionary_corrupted);
        dictPtr += hSize;
    }

    {   short offcodeNCount[MaxOff+1];
        U32 offcodeMaxValue=MaxOff, offcodeLog=OffFSELog;
        size_t const offcodeHeaderSize = FSE_readNCount(offcodeNCount, &offcodeMaxValue, &offcodeLog, dictPtr, dictEnd-dictPtr);
        if (FSE_isError(offcodeHeaderSize)) return ERROR(dictionary_corrupted);
        { size_t const errorCode = FSE_buildDTable(dctx->OffTable, offcodeNCount, offcodeMaxValue, offcodeLog);
          if (FSE_isError(errorCode)) return ERROR(dictionary_corrupted); }
        dictPtr += offcodeHeaderSize;
    }

    {   short matchlengthNCount[MaxML+1];
        unsigned matchlengthMaxValue = MaxML, matchlengthLog = MLFSELog;
        size_t const matchlengthHeaderSize = FSE_readNCount(matchlengthNCount, &matchlengthMaxValue, &matchlengthLog, dictPtr, dictEnd-dictPtr);
        if (FSE_isError(matchlengthHeaderSize)) return ERROR(dictionary_corrupted);
        { size_t const errorCode = FSE_buildDTable(dctx->MLTable, matchlengthNCount, matchlengthMaxValue, matchlengthLog);
          if (FSE_isError(errorCode)) return ERROR(dictionary_corrupted); }
        dictPtr += matchlengthHeaderSize;
    }

    {   short litlengthNCount[MaxLL+1];
        unsigned litlengthMaxValue = MaxLL, litlengthLog = LLFSELog;
        size_t const litlengthHeaderSize = FSE_readNCount(litlengthNCount, &litlengthMaxValue, &litlengthLog, dictPtr, dictEnd-dictPtr);
        if (FSE_isError(litlengthHeaderSize)) return ERROR(dictionary_corrupted);
        { size_t const errorCode = FSE_buildDTable(dctx->LLTable, litlengthNCount, litlengthMaxValue, litlengthLog);
          if (FSE_isError(errorCode)) return ERROR(dictionary_corrupted); }
        dictPtr += litlengthHeaderSize;
    }

    if (dictPtr+12 > dictEnd) return ERROR(dictionary_corrupted);
    dctx->rep[0] = MEM_readLE32(dictPtr+0); if (dctx->rep[0] >= dictSize) return ERROR(dictionary_corrupted);
    dctx->rep[1] = MEM_readLE32(dictPtr+4); if (dctx->rep[1] >= dictSize) return ERROR(dictionary_corrupted);
    dctx->rep[2] = MEM_readLE32(dictPtr+8); if (dctx->rep[2] >= dictSize) return ERROR(dictionary_corrupted);
    dictPtr += 12;

    dctx->litEntropy = dctx->fseEntropy = 1;
    return dictPtr - (const BYTE*)dict;
}

static size_t ZSTD_decompress_insertDictionary(ZSTD_DCtx* dctx, const void* dict, size_t dictSize)
{
    if (dictSize < 8) return ERROR(dictionary_corrupted);
    {   U32 const magic = MEM_readLE32(dict);
        if (magic != ZSTD_DICT_MAGIC) {
            /* pure content mode */
            ZSTD_refDictContent(dctx, dict, dictSize);
            return 0;
        }
        dctx->dictID = MEM_readLE32((const char*)dict + 4);

        /* load entropy tables */
        dict = (const char*)dict + 8;
        dictSize -= 8;
        {   size_t const eSize = ZSTD_loadEntropy(dctx, dict, dictSize);
            if (ZSTD_isError(eSize)) return ERROR(dictionary_corrupted);
            dict = (const char*)dict + eSize;
            dictSize -= eSize;
        }

        /* reference dictionary content */
        ZSTD_refDictContent(dctx, dict, dictSize);

        return 0;
    }
}


size_t ZSTD_decompressBegin_usingDict(ZSTD_DCtx* dctx, const void* dict, size_t dictSize)
{
    { size_t const errorCode = ZSTD_decompressBegin(dctx);
      if (ZSTD_isError(errorCode)) return errorCode; }

    if (dict && dictSize) {
        size_t const errorCode = ZSTD_decompress_insertDictionary(dctx, dict, dictSize);
        if (ZSTD_isError(errorCode)) return ERROR(dictionary_corrupted);
    }

    return 0;
}


struct ZSTD_DDict_s {
    void* dictContent;
    size_t dictContentSize;
    ZSTD_DCtx* refContext;
};  /* typedef'd tp ZSTD_CDict within zstd.h */

ZSTD_DDict* ZSTD_createDDict_advanced(const void* dict, size_t dictSize, ZSTD_customMem customMem)
{
    if (!customMem.customAlloc && !customMem.customFree)
        customMem = defaultCustomMem;

    if (!customMem.customAlloc || !customMem.customFree)
        return NULL;

    {   ZSTD_DDict* const ddict = (ZSTD_DDict*) customMem.customAlloc(customMem.opaque, sizeof(*ddict));
        void* const dictContent = customMem.customAlloc(customMem.opaque, dictSize);
        ZSTD_DCtx* const dctx = ZSTD_createDCtx_advanced(customMem);

        if (!dictContent || !ddict || !dctx) {
            customMem.customFree(customMem.opaque, dictContent);
            customMem.customFree(customMem.opaque, ddict);
            customMem.customFree(customMem.opaque, dctx);
            return NULL;
        }

        memcpy(dictContent, dict, dictSize);
        {   size_t const errorCode = ZSTD_decompressBegin_usingDict(dctx, dictContent, dictSize);
            if (ZSTD_isError(errorCode)) {
                customMem.customFree(customMem.opaque, dictContent);
                customMem.customFree(customMem.opaque, ddict);
                customMem.customFree(customMem.opaque, dctx);
                return NULL;
        }   }

        ddict->dictContent = dictContent;
        ddict->dictContentSize = dictSize;
        ddict->refContext = dctx;
        return ddict;
    }
}

/*! ZSTD_createDDict() :
*   Create a digested dictionary, ready to start decompression operation without startup delay.
*   `dict` can be released after `ZSTD_DDict` creation */
ZSTD_DDict* ZSTD_createDDict(const void* dict, size_t dictSize)
{
    ZSTD_customMem const allocator = { NULL, NULL, NULL };
    return ZSTD_createDDict_advanced(dict, dictSize, allocator);
}

size_t ZSTD_freeDDict(ZSTD_DDict* ddict)
{
    ZSTD_freeFunction const cFree = ddict->refContext->customMem.customFree;
    void* const opaque = ddict->refContext->customMem.opaque;
    ZSTD_freeDCtx(ddict->refContext);
    cFree(opaque, ddict->dictContent);
    cFree(opaque, ddict);
    return 0;
}

/*! ZSTD_decompress_usingDDict() :
*   Decompression using a pre-digested Dictionary
*   Use dictionary without significant overhead. */
ZSTDLIB_API size_t ZSTD_decompress_usingDDict(ZSTD_DCtx* dctx,
                                           void* dst, size_t dstCapacity,
                                     const void* src, size_t srcSize,
                                     const ZSTD_DDict* ddict)
{
    return ZSTD_decompress_usingPreparedDCtx(dctx, ddict->refContext,
                                           dst, dstCapacity,
                                           src, srcSize);
}
