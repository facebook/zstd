/*
    Buffered version of Zstd compression library
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
    - zstd source repository : https://github.com/Cyan4973/zstd
    - ztsd public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/* The objects defined into this file should be considered experimental.
 * They are not labelled stable, as their prototype may change in the future.
 * You can use them for tests, provide feedback, or if you can endure risk of future changes.
 */

/* *************************************
*  Dependencies
***************************************/
#include <stdlib.h>
#include "error_private.h"
#include "zstd_internal.h"  /* MIN, ZSTD_blockHeaderSize */
#include "zstd_static.h"    /* ZSTD_BLOCKSIZE_MAX */
#include "zbuff_static.h"


/* *************************************
*  Constants
***************************************/
static size_t const ZBUFF_endFrameSize = ZSTD_BLOCKHEADERSIZE;


/*_**************************************************
*  Streaming compression
*
*  A ZBUFF_CCtx object is required to track streaming operation.
*  Use ZBUFF_createCCtx() and ZBUFF_freeCCtx() to create/release resources.
*  Use ZBUFF_compressInit() to start a new compression operation.
*  ZBUFF_CCtx objects can be reused multiple times.
*
*  Use ZBUFF_compressContinue() repetitively to consume your input.
*  *srcSizePtr and *dstCapacityPtr can be any size.
*  The function will report how many bytes were read or written by modifying *srcSizePtr and *dstCapacityPtr.
*  Note that it may not consume the entire input, in which case it's up to the caller to call again the function with remaining input.
*  The content of dst will be overwritten (up to *dstCapacityPtr) at each function call, so save its content if it matters or change dst .
*  @return : a hint to preferred nb of bytes to use as input for next function call (it's only a hint, to improve latency)
*            or an error code, which can be tested using ZBUFF_isError().
*
*  ZBUFF_compressFlush() can be used to instruct ZBUFF to compress and output whatever remains within its buffer.
*  Note that it will not output more than *dstCapacityPtr.
*  Therefore, some content might still be left into its internal buffer if dst buffer is too small.
*  @return : nb of bytes still present into internal buffer (0 if it's empty)
*            or an error code, which can be tested using ZBUFF_isError().
*
*  ZBUFF_compressEnd() instructs to finish a frame.
*  It will perform a flush and write frame epilogue.
*  Similar to ZBUFF_compressFlush(), it may not be able to output the entire internal buffer content if *dstCapacityPtr is too small.
*  @return : nb of bytes still present into internal buffer (0 if it's empty)
*            or an error code, which can be tested using ZBUFF_isError().
*
*  Hint : recommended buffer sizes (not compulsory)
*  input : ZSTD_BLOCKSIZE_MAX (128 KB), internal unit size, it improves latency to use this value.
*  output : ZSTD_compressBound(ZSTD_BLOCKSIZE_MAX) + ZSTD_blockHeaderSize + ZBUFF_endFrameSize : ensures it's always possible to write/flush/end a full block at best speed.
* **************************************************/

typedef enum { ZBUFFcs_init, ZBUFFcs_load, ZBUFFcs_flush } ZBUFF_cStage;

/* *** Ressources *** */
struct ZBUFF_CCtx_s {
    ZSTD_CCtx* zc;
    char*  inBuff;
    size_t inBuffSize;
    size_t inToCompress;
    size_t inBuffPos;
    size_t inBuffTarget;
    size_t blockSize;
    char*  outBuff;
    size_t outBuffSize;
    size_t outBuffContentSize;
    size_t outBuffFlushedSize;
    ZBUFF_cStage stage;
};   /* typedef'd tp ZBUFF_CCtx within "zstd_buffered.h" */

ZBUFF_CCtx* ZBUFF_createCCtx(void)
{
    ZBUFF_CCtx* zbc = (ZBUFF_CCtx*)malloc(sizeof(ZBUFF_CCtx));
    if (zbc==NULL) return NULL;
    memset(zbc, 0, sizeof(*zbc));
    zbc->zc = ZSTD_createCCtx();
    return zbc;
}

size_t ZBUFF_freeCCtx(ZBUFF_CCtx* zbc)
{
    if (zbc==NULL) return 0;   /* support free on NULL */
    ZSTD_freeCCtx(zbc->zc);
    free(zbc->inBuff);
    free(zbc->outBuff);
    free(zbc);
    return 0;
}


/* *** Initialization *** */

size_t ZBUFF_compressInit_advanced(ZBUFF_CCtx* zbc,
                                   const void* dict, size_t dictSize,
                                   ZSTD_parameters params, U64 pledgedSrcSize)
{
    size_t neededInBuffSize;

    ZSTD_adjustCParams(&params.cParams, pledgedSrcSize, dictSize);
    neededInBuffSize = (size_t)1 << params.cParams.windowLog;

    /* allocate buffers */
    if (zbc->inBuffSize < neededInBuffSize) {
        zbc->inBuffSize = neededInBuffSize;
        free(zbc->inBuff);   /* should not be necessary */
        zbc->inBuff = (char*)malloc(neededInBuffSize);
        if (zbc->inBuff == NULL) return ERROR(memory_allocation);
    }
    zbc->blockSize = MIN(ZSTD_BLOCKSIZE_MAX, zbc->inBuffSize);
    if (zbc->outBuffSize < ZSTD_compressBound(zbc->blockSize)+1) {
        zbc->outBuffSize = ZSTD_compressBound(zbc->blockSize)+1;
        free(zbc->outBuff);   /* should not be necessary */
        zbc->outBuff = (char*)malloc(zbc->outBuffSize);
        if (zbc->outBuff == NULL) return ERROR(memory_allocation);
    }

    zbc->outBuffContentSize = ZSTD_compressBegin_advanced(zbc->zc, dict, dictSize, params, pledgedSrcSize);
    if (ZSTD_isError(zbc->outBuffContentSize)) return zbc->outBuffContentSize;

    zbc->inToCompress = 0;
    zbc->inBuffPos = 0;
    zbc->inBuffTarget = zbc->blockSize;
    zbc->outBuffFlushedSize = 0;
    zbc->stage = ZBUFFcs_flush;   /* starts by flushing the header */
    return 0;   /* ready to go */
}


ZSTDLIB_API size_t ZBUFF_compressInitDictionary(ZBUFF_CCtx* zbc, const void* dict, size_t dictSize, int compressionLevel)
{
    ZSTD_parameters params;
    params.cParams = ZSTD_getCParams(compressionLevel, 0, dictSize);
    params.fParams.contentSizeFlag = 0;
    return ZBUFF_compressInit_advanced(zbc, dict, dictSize, params, 0);
}

size_t ZBUFF_compressInit(ZBUFF_CCtx* zbc, int compressionLevel)
{
    return ZBUFF_compressInitDictionary(zbc, NULL, 0, compressionLevel);
}


/* *** Compression *** */

static size_t ZBUFF_limitCopy(void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
    size_t length = MIN(dstCapacity, srcSize);
    memcpy(dst, src, length);
    return length;
}

static size_t ZBUFF_compressContinue_generic(ZBUFF_CCtx* zbc,
                              void* dst, size_t* dstCapacityPtr,
                        const void* src, size_t* srcSizePtr,
                              int flush)   /* aggregate : wait for full block before compressing */
{
    U32 notDone = 1;
    const char* const istart = (const char*)src;
    const char* ip = istart;
    const char* const iend = istart + *srcSizePtr;
    char* const ostart = (char*)dst;
    char* op = ostart;
    char* const oend = ostart + *dstCapacityPtr;

    while (notDone) {
        switch(zbc->stage)
        {
        case ZBUFFcs_init: return ERROR(init_missing);   /* call ZBUFF_compressInit() first ! */

        case ZBUFFcs_load:
            /* complete inBuffer */
            {
                size_t toLoad = zbc->inBuffTarget - zbc->inBuffPos;
                size_t loaded = ZBUFF_limitCopy(zbc->inBuff + zbc->inBuffPos, toLoad, ip, iend-ip);
                zbc->inBuffPos += loaded;
                ip += loaded;
                if ( (zbc->inBuffPos==zbc->inToCompress) || (!flush && (toLoad != loaded)) ) {
                    notDone = 0; break;  /* not enough input to get a full block : stop there, wait for more */
            }   }
            /* compress current block (note : this stage cannot be stopped in the middle) */
            {
                void* cDst;
                size_t cSize;
                size_t iSize = zbc->inBuffPos - zbc->inToCompress;
                size_t oSize = oend-op;
                if (oSize >= ZSTD_compressBound(iSize))
                    cDst = op;   /* compress directly into output buffer (avoid flush stage) */
                else
                    cDst = zbc->outBuff, oSize = zbc->outBuffSize;
                cSize = ZSTD_compressContinue(zbc->zc, cDst, oSize, zbc->inBuff + zbc->inToCompress, iSize);
                if (ZSTD_isError(cSize)) return cSize;
                /* prepare next block */
                zbc->inBuffTarget = zbc->inBuffPos + zbc->blockSize;
                if (zbc->inBuffTarget > zbc->inBuffSize)
                    { zbc->inBuffPos = 0; zbc->inBuffTarget = zbc->blockSize; }   /* note : inBuffSize >= blockSize */
                zbc->inToCompress = zbc->inBuffPos;
                if (cDst == op) { op += cSize; break; }   /* no need to flush */
                zbc->outBuffContentSize = cSize;
                zbc->outBuffFlushedSize = 0;
                zbc->stage = ZBUFFcs_flush;
                // break;   /* flush stage follows */
            }

        case ZBUFFcs_flush:
            /* flush into dst */
            {
                size_t toFlush = zbc->outBuffContentSize - zbc->outBuffFlushedSize;
                size_t flushed = ZBUFF_limitCopy(op, oend-op, zbc->outBuff + zbc->outBuffFlushedSize, toFlush);
                op += flushed;
                zbc->outBuffFlushedSize += flushed;
                if (toFlush!=flushed) { notDone = 0; break; } /* not enough space within dst to store compressed block : stop there */
                zbc->outBuffContentSize = 0;
                zbc->outBuffFlushedSize = 0;
                zbc->stage = ZBUFFcs_load;
                break;
            }
        default:
            return ERROR(GENERIC);   /* impossible */
        }
    }

    *srcSizePtr = ip - istart;
    *dstCapacityPtr = op - ostart;
    {
        size_t hintInSize = zbc->inBuffTarget - zbc->inBuffPos;
        if (hintInSize==0) hintInSize = zbc->blockSize;
        return hintInSize;
    }
}

size_t ZBUFF_compressContinue(ZBUFF_CCtx* zbc,
                              void* dst, size_t* dstCapacityPtr,
                        const void* src, size_t* srcSizePtr)
{
    return ZBUFF_compressContinue_generic(zbc, dst, dstCapacityPtr, src, srcSizePtr, 0);
}



/* *** Finalize *** */

size_t ZBUFF_compressFlush(ZBUFF_CCtx* zbc, void* dst, size_t* dstCapacityPtr)
{
    size_t srcSize = 0;
    ZBUFF_compressContinue_generic(zbc, dst, dstCapacityPtr, &srcSize, &srcSize, 1);  /* use a valid src address instead of NULL, as some sanitizer don't like it */
    return zbc->outBuffContentSize - zbc->outBuffFlushedSize;
}


size_t ZBUFF_compressEnd(ZBUFF_CCtx* zbc, void* dst, size_t* dstCapacityPtr)
{
    BYTE* const ostart = (BYTE*)dst;
    BYTE* op = ostart;
    BYTE* const oend = ostart + *dstCapacityPtr;
    size_t outSize = *dstCapacityPtr;
    size_t epilogueSize, remaining;
    ZBUFF_compressFlush(zbc, dst, &outSize);    /* flush any remaining inBuff */
    op += outSize;
    epilogueSize = ZSTD_compressEnd(zbc->zc, zbc->outBuff + zbc->outBuffContentSize, zbc->outBuffSize - zbc->outBuffContentSize);   /* epilogue into outBuff */
    zbc->outBuffContentSize += epilogueSize;
    outSize = oend-op;
    zbc->stage = ZBUFFcs_flush;
    remaining = ZBUFF_compressFlush(zbc, op, &outSize);   /* attempt to flush epilogue into dst */
    op += outSize;
    if (!remaining) zbc->stage = ZBUFFcs_init;  /* close only if nothing left to flush */
    *dstCapacityPtr = op-ostart;                 /* tells how many bytes were written */
    return remaining;
}


/*-***************************************************************************
*  Streaming decompression howto
*
*  A ZBUFF_DCtx object is required to track streaming operations.
*  Use ZBUFF_createDCtx() and ZBUFF_freeDCtx() to create/release resources.
*  Use ZBUFF_decompressInit() to start a new decompression operation,
*   or ZBUFF_decompressInitDictionary() if decompression requires a dictionary.
*  Note that ZBUFF_DCtx objects can be re-init multiple times.
*
*  Use ZBUFF_decompressContinue() repetitively to consume your input.
*  *srcSizePtr and *dstCapacityPtr can be any size.
*  The function will report how many bytes were read or written by modifying *srcSizePtr and *dstCapacityPtr.
*  Note that it may not consume the entire input, in which case it's up to the caller to present remaining input again.
*  The content of @dst will be overwritten (up to *dstCapacityPtr) at each function call, so save its content if it matters, or change @dst.
*  @return : a hint to preferred nb of bytes to use as input for next function call (it's only a hint, to help latency),
*            or 0 when a frame is completely decoded,
*            or an error code, which can be tested using ZBUFF_isError().
*
*  Hint : recommended buffer sizes (not compulsory) : ZBUFF_recommendedDInSize() and ZBUFF_recommendedDOutSize()
*  output : ZBUFF_recommendedDOutSize==128 KB block size is the internal unit, it ensures it's always possible to write a full block when decoded.
*  input  : ZBUFF_recommendedDInSize == 128KB + 3;
*           just follow indications from ZBUFF_decompressContinue() to minimize latency. It should always be <= 128 KB + 3 .
* *******************************************************************************/

typedef enum { ZBUFFds_init, ZBUFFds_readHeader,
               ZBUFFds_read, ZBUFFds_load, ZBUFFds_flush } ZBUFF_dStage;

/* *** Resource management *** */
struct ZBUFF_DCtx_s {
    ZSTD_DCtx* zc;
    ZSTD_frameParams fParams;
    char* inBuff;
    size_t inBuffSize;
    size_t inPos;
    char* outBuff;
    size_t outBuffSize;
    size_t outStart;
    size_t outEnd;
    ZBUFF_dStage stage;
};   /* typedef'd to ZBUFF_DCtx within "zstd_buffered.h" */


ZBUFF_DCtx* ZBUFF_createDCtx(void)
{
    ZBUFF_DCtx* zbc = (ZBUFF_DCtx*)malloc(sizeof(ZBUFF_DCtx));
    if (zbc==NULL) return NULL;
    memset(zbc, 0, sizeof(*zbc));
    zbc->zc = ZSTD_createDCtx();
    zbc->stage = ZBUFFds_init;
    return zbc;
}

size_t ZBUFF_freeDCtx(ZBUFF_DCtx* zbc)
{
    if (zbc==NULL) return 0;   /* support free on null */
    ZSTD_freeDCtx(zbc->zc);
    free(zbc->inBuff);
    free(zbc->outBuff);
    free(zbc);
    return 0;
}


/* *** Initialization *** */

size_t ZBUFF_decompressInitDictionary(ZBUFF_DCtx* zbc, const void* dict, size_t dictSize)
{
    zbc->stage = ZBUFFds_readHeader;
    zbc->inPos = zbc->outStart = zbc->outEnd = 0;
    return ZSTD_decompressBegin_usingDict(zbc->zc, dict, dictSize);
}

size_t ZBUFF_decompressInit(ZBUFF_DCtx* zbc)
{
    return ZBUFF_decompressInitDictionary(zbc, NULL, 0);
}


/* *** Decompression *** */

size_t ZBUFF_decompressContinue(ZBUFF_DCtx* zbc,
                                void* dst, size_t* dstCapacityPtr,
                          const void* src, size_t* srcSizePtr)
{
    const char* const istart = (const char*)src;
    const char* ip = istart;
    const char* const iend = istart + *srcSizePtr;
    char* const ostart = (char*)dst;
    char* op = ostart;
    char* const oend = ostart + *dstCapacityPtr;
    U32 notDone = 1;

    while (notDone) {
        switch(zbc->stage)
        {
        case ZBUFFds_init :
            return ERROR(init_missing);

        case ZBUFFds_readHeader :
            /* read header from src */
            {   size_t const headerSize = ZSTD_getFrameParams(&(zbc->fParams), src, *srcSizePtr);
                if (ZSTD_isError(headerSize)) return headerSize;
                if (headerSize) {
                    /* not enough input to decode header : needs headerSize > *srcSizePtr */
                    *dstCapacityPtr = 0;
                    *srcSizePtr = 0;
                    return headerSize;
            }   }

            /* Frame header provides buffer sizes */
            {   size_t const neededInSize = ZSTD_BLOCKSIZE_MAX;   /* a block is never > ZSTD_BLOCKSIZE_MAX */
                if (zbc->inBuffSize < neededInSize) {
                    free(zbc->inBuff);
                    zbc->inBuffSize = neededInSize;
                    zbc->inBuff = (char*)malloc(neededInSize);
                    if (zbc->inBuff == NULL) return ERROR(memory_allocation);
            }   }
            {
                size_t const neededOutSize = (size_t)1 << zbc->fParams.windowLog;
                if (zbc->outBuffSize < neededOutSize) {
                    free(zbc->outBuff);
                    zbc->outBuffSize = neededOutSize;
                    zbc->outBuff = (char*)malloc(neededOutSize);
                    if (zbc->outBuff == NULL) return ERROR(memory_allocation);
            }   }
            zbc->stage = ZBUFFds_read;

        case ZBUFFds_read:
            {
                size_t const neededInSize = ZSTD_nextSrcSizeToDecompress(zbc->zc);
                if (neededInSize==0) {  /* end of frame */
                    zbc->stage = ZBUFFds_init;
                    notDone = 0;
                    break;
                }
                if ((size_t)(iend-ip) >= neededInSize) {
                    /* directly decode from src */
                    size_t const decodedSize = ZSTD_decompressContinue(zbc->zc,
                        zbc->outBuff + zbc->outStart, zbc->outBuffSize - zbc->outStart,
                        ip, neededInSize);
                    if (ZSTD_isError(decodedSize)) return decodedSize;
                    ip += neededInSize;
                    if (!decodedSize) break;   /* this was just a header */
                    zbc->outEnd = zbc->outStart +  decodedSize;
                    zbc->stage = ZBUFFds_flush;
                    break;
                }
                if (ip==iend) { notDone = 0; break; }   /* no more input */
                zbc->stage = ZBUFFds_load;
            }

        case ZBUFFds_load:
            {
                size_t const neededInSize = ZSTD_nextSrcSizeToDecompress(zbc->zc);
                size_t const toLoad = neededInSize - zbc->inPos;   /* should always be <= remaining space within inBuff */
                size_t loadedSize;
                if (toLoad > zbc->inBuffSize - zbc->inPos) return ERROR(corruption_detected);   /* should never happen */
                loadedSize = ZBUFF_limitCopy(zbc->inBuff + zbc->inPos, toLoad, ip, iend-ip);
                ip += loadedSize;
                zbc->inPos += loadedSize;
                if (loadedSize < toLoad) { notDone = 0; break; }   /* not enough input, wait for more */
                {
                    size_t const decodedSize = ZSTD_decompressContinue(zbc->zc,
                        zbc->outBuff + zbc->outStart, zbc->outBuffSize - zbc->outStart,
                        zbc->inBuff, neededInSize);
                    if (ZSTD_isError(decodedSize)) return decodedSize;
                    zbc->inPos = 0;   /* input is consumed */
                    if (!decodedSize) { zbc->stage = ZBUFFds_read; break; }   /* this was just a header */
                    zbc->outEnd = zbc->outStart +  decodedSize;
                    zbc->stage = ZBUFFds_flush;
                    // break; /* ZBUFFds_flush follows */
            }   }
        case ZBUFFds_flush:
            {
                size_t const toFlushSize = zbc->outEnd - zbc->outStart;
                size_t const flushedSize = ZBUFF_limitCopy(op, oend-op, zbc->outBuff + zbc->outStart, toFlushSize);
                op += flushedSize;
                zbc->outStart += flushedSize;
                if (flushedSize == toFlushSize) {
                    zbc->stage = ZBUFFds_read;
                    if (zbc->outStart + ZSTD_BLOCKSIZE_MAX > zbc->outBuffSize)
                        zbc->outStart = zbc->outEnd = 0;
                    break;
                }
                /* cannot flush everything */
                notDone = 0;
                break;
            }
        default: return ERROR(GENERIC);   /* impossible */
    }   }

    /* result */
    *srcSizePtr = ip-istart;
    *dstCapacityPtr = op-ostart;
    {
        size_t nextSrcSizeHint = ZSTD_nextSrcSizeToDecompress(zbc->zc);
        if (nextSrcSizeHint > ZSTD_blockHeaderSize) nextSrcSizeHint+= ZSTD_blockHeaderSize;   /* get following block header too */
        nextSrcSizeHint -= zbc->inPos;   /* already loaded*/
        return nextSrcSizeHint;
    }
}



/* *************************************
*  Tool functions
***************************************/
unsigned ZBUFF_isError(size_t errorCode) { return ERR_isError(errorCode); }
const char* ZBUFF_getErrorName(size_t errorCode) { return ERR_getErrorName(errorCode); }

size_t ZBUFF_recommendedCInSize(void)  { return ZSTD_BLOCKSIZE_MAX; }
size_t ZBUFF_recommendedCOutSize(void) { return ZSTD_compressBound(ZSTD_BLOCKSIZE_MAX) + ZSTD_blockHeaderSize + ZBUFF_endFrameSize; }
size_t ZBUFF_recommendedDInSize(void)  { return ZSTD_BLOCKSIZE_MAX + ZSTD_blockHeaderSize /* block header size*/ ; }
size_t ZBUFF_recommendedDOutSize(void) { return ZSTD_BLOCKSIZE_MAX; }
