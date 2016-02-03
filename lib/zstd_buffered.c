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
#include "zstd_static.h"
#include "zstd_buffered_static.h"


/* *************************************
*  Constants
***************************************/
static size_t ZBUFF_blockHeaderSize = 3;
static size_t ZBUFF_endFrameSize = 3;

/** ************************************************
*  Streaming compression
*
*  A ZBUFF_CCtx object is required to track streaming operation.
*  Use ZBUFF_createCCtx() and ZBUFF_freeCCtx() to create/release resources.
*  Use ZBUFF_compressInit() to start a new compression operation.
*  ZBUFF_CCtx objects can be reused multiple times.
*
*  Use ZBUFF_compressContinue() repetitively to consume your input.
*  *srcSizePtr and *maxDstSizePtr can be any size.
*  The function will report how many bytes were read or written by modifying *srcSizePtr and *maxDstSizePtr.
*  Note that it may not consume the entire input, in which case it's up to the caller to call again the function with remaining input.
*  The content of dst will be overwritten (up to *maxDstSizePtr) at each function call, so save its content if it matters or change dst .
*  @return : a hint to preferred nb of bytes to use as input for next function call (it's only a hint, to improve latency)
*            or an error code, which can be tested using ZBUFF_isError().
*
*  ZBUFF_compressFlush() can be used to instruct ZBUFF to compress and output whatever remains within its buffer.
*  Note that it will not output more than *maxDstSizePtr.
*  Therefore, some content might still be left into its internal buffer if dst buffer is too small.
*  @return : nb of bytes still present into internal buffer (0 if it's empty)
*            or an error code, which can be tested using ZBUFF_isError().
*
*  ZBUFF_compressEnd() instructs to finish a frame.
*  It will perform a flush and write frame epilogue.
*  Similar to ZBUFF_compressFlush(), it may not be able to output the entire internal buffer content if *maxDstSizePtr is too small.
*  @return : nb of bytes still present into internal buffer (0 if it's empty)
*            or an error code, which can be tested using ZBUFF_isError().
*
*  Hint : recommended buffer sizes (not compulsory)
*  input : 128 KB block size is the internal unit, it improves latency to use this value.
*  output : ZSTD_compressBound(128 KB) + 3 + 3 : ensures it's always possible to write/flush/end a full block at best speed.
* **************************************************/

typedef enum { ZBUFFcs_init, ZBUFFcs_load, ZBUFFcs_flush } ZBUFF_cStage;

/* *** Ressources *** */
struct ZBUFF_CCtx_s {
    ZSTD_CCtx* zc;
    char* inBuff;
    size_t inBuffSize;
    size_t inToCompress;
    size_t inBuffPos;
    size_t inBuffTarget;
    size_t blockSize;
    char* outBuff;
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

#define MIN(a,b)    ( ((a)<(b)) ? (a) : (b) )
#define BLOCKSIZE   (128 * 1024)   /* a bit too "magic", should come from reference */
size_t ZBUFF_compressInit_advanced(ZBUFF_CCtx* zbc, const void* dict, size_t dictSize, ZSTD_parameters params)
{
    size_t neededInBuffSize;

    ZSTD_validateParams(&params);
    neededInBuffSize = (size_t)1 << params.windowLog;

    /* allocate buffers */
    if (zbc->inBuffSize < neededInBuffSize) {
        zbc->inBuffSize = neededInBuffSize;
        free(zbc->inBuff);   /* should not be necessary */
        zbc->inBuff = (char*)malloc(neededInBuffSize);
        if (zbc->inBuff == NULL) return ERROR(memory_allocation);
    }
    zbc->blockSize = MIN(BLOCKSIZE, zbc->inBuffSize);
    if (zbc->outBuffSize < ZSTD_compressBound(zbc->blockSize)+1) {
        zbc->outBuffSize = ZSTD_compressBound(zbc->blockSize)+1;
        free(zbc->outBuff);   /* should not be necessary */
        zbc->outBuff = (char*)malloc(zbc->outBuffSize);
        if (zbc->outBuff == NULL) return ERROR(memory_allocation);
    }

    zbc->outBuffContentSize = ZSTD_compressBegin_advanced(zbc->zc, dict, dictSize, params);
    if (ZSTD_isError(zbc->outBuffContentSize)) return zbc->outBuffContentSize;

    zbc->inToCompress = 0;
    zbc->inBuffPos = 0;
    zbc->inBuffTarget = zbc->blockSize;
    zbc->outBuffFlushedSize = 0;
    zbc->stage = ZBUFFcs_flush;   /* starts by flushing the header */
    return 0;   /* ready to go */
}

size_t ZBUFF_compressInit(ZBUFF_CCtx* zbc, int compressionLevel)
{
    return ZBUFF_compressInit_advanced(zbc, NULL, 0, ZSTD_getParams(compressionLevel, 0));
}


ZSTDLIB_API size_t ZBUFF_compressInitDictionary(ZBUFF_CCtx* zbc, const void* dict, size_t dictSize, int compressionLevel)
{
    return ZBUFF_compressInit_advanced(zbc, dict, dictSize, ZSTD_getParams(compressionLevel, 0));
}


/* *** Compression *** */

static size_t ZBUFF_limitCopy(void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    size_t length = MIN(maxDstSize, srcSize);
    memcpy(dst, src, length);
    return length;
}

static size_t ZBUFF_compressContinue_generic(ZBUFF_CCtx* zbc,
                              void* dst, size_t* maxDstSizePtr,
                        const void* src, size_t* srcSizePtr,
                              int flush)   /* aggregate : wait for full block before compressing */
{
    U32 notDone = 1;
    const char* const istart = (const char*)src;
    const char* ip = istart;
    const char* const iend = istart + *srcSizePtr;
    char* const ostart = (char*)dst;
    char* op = ostart;
    char* const oend = ostart + *maxDstSizePtr;

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
    *maxDstSizePtr = op - ostart;
    {
        size_t hintInSize = zbc->inBuffTarget - zbc->inBuffPos;
        if (hintInSize==0) hintInSize = zbc->blockSize;
        return hintInSize;
    }
}

size_t ZBUFF_compressContinue(ZBUFF_CCtx* zbc,
                              void* dst, size_t* maxDstSizePtr,
                        const void* src, size_t* srcSizePtr)
{
    return ZBUFF_compressContinue_generic(zbc, dst, maxDstSizePtr, src, srcSizePtr, 0);
}



/* *** Finalize *** */

size_t ZBUFF_compressFlush(ZBUFF_CCtx* zbc, void* dst, size_t* maxDstSizePtr)
{
    size_t srcSize = 0;
    ZBUFF_compressContinue_generic(zbc, dst, maxDstSizePtr, &srcSize, &srcSize, 1);  /* use a valid src address instead of NULL, as some sanitizer don't like it */
    return zbc->outBuffContentSize - zbc->outBuffFlushedSize;
}


size_t ZBUFF_compressEnd(ZBUFF_CCtx* zbc, void* dst, size_t* maxDstSizePtr)
{
    BYTE* const ostart = (BYTE*)dst;
    BYTE* op = ostart;
    BYTE* const oend = ostart + *maxDstSizePtr;
    size_t outSize = *maxDstSizePtr;
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
    *maxDstSizePtr = op-ostart;                 /* tells how many bytes were written */
    return remaining;
}



/** ************************************************
*  Streaming decompression
*
*  A ZBUFF_DCtx object is required to track streaming operation.
*  Use ZBUFF_createDCtx() and ZBUFF_freeDCtx() to create/release resources.
*  Use ZBUFF_decompressInit() to start a new decompression operation.
*  ZBUFF_DCtx objects can be reused multiple times.
*
*  Use ZBUFF_decompressContinue() repetitively to consume your input.
*  *srcSizePtr and *maxDstSizePtr can be any size.
*  The function will report how many bytes were read or written by modifying *srcSizePtr and *maxDstSizePtr.
*  Note that it may not consume the entire input, in which case it's up to the caller to call again the function with remaining input.
*  The content of dst will be overwritten (up to *maxDstSizePtr) at each function call, so save its content if it matters or change dst .
*  @return : a hint to preferred nb of bytes to use as input for next function call (it's only a hint, to improve latency)
*            or 0 when a frame is completely decoded
*            or an error code, which can be tested using ZBUFF_isError().
*
*  Hint : recommended buffer sizes (not compulsory)
*  output : 128 KB block size is the internal unit, it ensures it's always possible to write a full block when it's decoded.
*  input : just follow indications from ZBUFF_decompressContinue() to minimize latency. It should always be <= 128 KB + 3 .
* **************************************************/

typedef enum { ZBUFFds_init, ZBUFFds_readHeader, ZBUFFds_loadHeader, ZBUFFds_decodeHeader,
               ZBUFFds_read, ZBUFFds_load, ZBUFFds_flush } ZBUFF_dStage;

/* *** Resource management *** */

#define ZSTD_frameHeaderSize_max 5   /* too magical, should come from reference */
struct ZBUFF_DCtx_s {
    ZSTD_DCtx* zc;
    ZSTD_parameters params;
    char* inBuff;
    size_t inBuffSize;
    size_t inPos;
    char* outBuff;
    size_t outBuffSize;
    size_t outStart;
    size_t outEnd;
    size_t hPos;
    ZBUFF_dStage stage;
    unsigned char headerBuffer[ZSTD_frameHeaderSize_max];
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
    zbc->hPos = zbc->inPos = zbc->outStart = zbc->outEnd = 0;
    return ZSTD_decompressBegin_usingDict(zbc->zc, dict, dictSize);
}

size_t ZBUFF_decompressInit(ZBUFF_DCtx* zbc)
{
    return ZBUFF_decompressInitDictionary(zbc, NULL, 0);
}


/* *** Decompression *** */

size_t ZBUFF_decompressContinue(ZBUFF_DCtx* zbc, void* dst, size_t* maxDstSizePtr, const void* src, size_t* srcSizePtr)
{
    const char* const istart = (const char*)src;
    const char* ip = istart;
    const char* const iend = istart + *srcSizePtr;
    char* const ostart = (char*)dst;
    char* op = ostart;
    char* const oend = ostart + *maxDstSizePtr;
    U32 notDone = 1;

    while (notDone) {
        switch(zbc->stage)
        {
        case ZBUFFds_init :
            return ERROR(init_missing);

        case ZBUFFds_readHeader :
            /* read header from src */
            {
                size_t headerSize = ZSTD_getFrameParams(&(zbc->params), src, *srcSizePtr);
                if (ZSTD_isError(headerSize)) return headerSize;
                if (headerSize) {
                    /* not enough input to decode header : tell how many bytes would be necessary */
                    memcpy(zbc->headerBuffer+zbc->hPos, src, *srcSizePtr);
                    zbc->hPos += *srcSizePtr;
                    *maxDstSizePtr = 0;
                    zbc->stage = ZBUFFds_loadHeader;
                    return headerSize - zbc->hPos;
                }
                zbc->stage = ZBUFFds_decodeHeader;
                break;
            }

        case ZBUFFds_loadHeader:
            /* complete header from src */
            {
                size_t headerSize = ZBUFF_limitCopy(
                    zbc->headerBuffer + zbc->hPos, ZSTD_frameHeaderSize_max - zbc->hPos,
                    src, *srcSizePtr);
                zbc->hPos += headerSize;
                ip += headerSize;
                headerSize = ZSTD_getFrameParams(&(zbc->params), zbc->headerBuffer, zbc->hPos);
                if (ZSTD_isError(headerSize)) return headerSize;
                if (headerSize) {
                    /* not enough input to decode header : tell how many bytes would be necessary */
                    *maxDstSizePtr = 0;
                    return headerSize - zbc->hPos;
                }
                // zbc->stage = ZBUFFds_decodeHeader; break;   /* useless : stage follows */
            }

        case ZBUFFds_decodeHeader:
                /* apply header to create / resize buffers */
                {
                    size_t neededOutSize = (size_t)1 << zbc->params.windowLog;
                    size_t neededInSize = BLOCKSIZE;   /* a block is never > BLOCKSIZE */
                    if (zbc->inBuffSize < neededInSize) {
                        free(zbc->inBuff);
                        zbc->inBuffSize = neededInSize;
                        zbc->inBuff = (char*)malloc(neededInSize);
                        if (zbc->inBuff == NULL) return ERROR(memory_allocation);
                    }
                    if (zbc->outBuffSize < neededOutSize) {
                        free(zbc->outBuff);
                        zbc->outBuffSize = neededOutSize;
                        zbc->outBuff = (char*)malloc(neededOutSize);
                        if (zbc->outBuff == NULL) return ERROR(memory_allocation);
                }   }
                if (zbc->hPos) {
                    /* some data already loaded into headerBuffer : transfer into inBuff */
                    memcpy(zbc->inBuff, zbc->headerBuffer, zbc->hPos);
                    zbc->inPos = zbc->hPos;
                    zbc->hPos = 0;
                    zbc->stage = ZBUFFds_load;
                    break;
                }
                zbc->stage = ZBUFFds_read;

        case ZBUFFds_read:
            {
                size_t neededInSize = ZSTD_nextSrcSizeToDecompress(zbc->zc);
                if (neededInSize==0) {  /* end of frame */
                    zbc->stage = ZBUFFds_init;
                    notDone = 0;
                    break;
                }
                if ((size_t)(iend-ip) >= neededInSize) {
                    /* directly decode from src */
                    size_t decodedSize = ZSTD_decompressContinue(zbc->zc,
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
                size_t neededInSize = ZSTD_nextSrcSizeToDecompress(zbc->zc);
                size_t toLoad = neededInSize - zbc->inPos;   /* should always be <= remaining space within inBuff */
                size_t loadedSize;
                if (toLoad > zbc->inBuffSize - zbc->inPos) return ERROR(corruption_detected);   /* should never happen */
                loadedSize = ZBUFF_limitCopy(zbc->inBuff + zbc->inPos, toLoad, ip, iend-ip);
                ip += loadedSize;
                zbc->inPos += loadedSize;
                if (loadedSize < toLoad) { notDone = 0; break; }   /* not enough input, wait for more */
                {
                    size_t decodedSize = ZSTD_decompressContinue(zbc->zc,
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
                size_t toFlushSize = zbc->outEnd - zbc->outStart;
                size_t flushedSize = ZBUFF_limitCopy(op, oend-op, zbc->outBuff + zbc->outStart, toFlushSize);
                op += flushedSize;
                zbc->outStart += flushedSize;
                if (flushedSize == toFlushSize) {
                    zbc->stage = ZBUFFds_read;
                    if (zbc->outStart + BLOCKSIZE > zbc->outBuffSize)
                        zbc->outStart = zbc->outEnd = 0;
                    break;
                }
                /* cannot flush everything */
                notDone = 0;
                break;
            }
        default: return ERROR(GENERIC);   /* impossible */
    }   }

    *srcSizePtr = ip-istart;
    *maxDstSizePtr = op-ostart;

    {
        size_t nextSrcSizeHint = ZSTD_nextSrcSizeToDecompress(zbc->zc);
        if (nextSrcSizeHint > ZBUFF_blockHeaderSize) nextSrcSizeHint+= ZBUFF_blockHeaderSize;   /* get next block header too */
        nextSrcSizeHint -= zbc->inPos;   /* already loaded*/
        return nextSrcSizeHint;
    }
}



/* *************************************
*  Tool functions
***************************************/
unsigned ZBUFF_isError(size_t errorCode) { return ERR_isError(errorCode); }
const char* ZBUFF_getErrorName(size_t errorCode) { return ERR_getErrorName(errorCode); }

size_t ZBUFF_recommendedCInSize(void)  { return BLOCKSIZE; }
size_t ZBUFF_recommendedCOutSize(void) { return ZSTD_compressBound(BLOCKSIZE) + ZBUFF_blockHeaderSize + ZBUFF_endFrameSize; }
size_t ZBUFF_recommendedDInSize(void)  { return BLOCKSIZE + ZBUFF_blockHeaderSize /* block header size*/ ; }
size_t ZBUFF_recommendedDOutSize(void) { return BLOCKSIZE; }
