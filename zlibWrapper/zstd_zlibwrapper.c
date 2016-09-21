/**
 * Copyright (c) 2016-present, Przemyslaw Skibinski, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */


#include <stdio.h>                 /* vsprintf */
#include <stdarg.h>                /* va_list, for z_gzprintf */
#include <zlib.h>
#include "zstd_zlibwrapper.h"
#define ZSTD_STATIC_LINKING_ONLY   /* ZSTD_MAGICNUMBER */
#include "zstd.h"
#include "zstd_internal.h"         /* defaultCustomMem */


#define Z_INFLATE_SYNC              8
#define ZLIB_HEADERSIZE             4
#define ZSTD_HEADERSIZE             ZSTD_frameHeaderSize_min
#define ZWRAP_DEFAULT_CLEVEL        5   /* Z_DEFAULT_COMPRESSION is translated to ZWRAP_DEFAULT_CLEVEL for zstd */

#define LOG_WRAPPERC(...)   /*printf(__VA_ARGS__)*/ 
#define LOG_WRAPPERD(...)   /*printf(__VA_ARGS__)*/


#define FINISH_WITH_GZ_ERR(msg) { \
    (void)msg; \
    return Z_STREAM_ERROR; \
}

#define FINISH_WITH_ERR(strm, message) { \
    strm->msg = message; \
    return Z_STREAM_ERROR; \
}

#define FINISH_WITH_NULL_ERR(msg) { \
    (void)msg; \
    return NULL; \
}

#ifndef ZWRAP_USE_ZSTD
    #define ZWRAP_USE_ZSTD 0
#endif

static int g_useZSTD = ZWRAP_USE_ZSTD;   /* 0 = don't use ZSTD */



void useZSTD(int turn_on) { g_useZSTD = turn_on; }

int isUsingZSTD(void) { return g_useZSTD; }

const char * zstdVersion(void) { return ZSTD_VERSION_STRING; }

ZEXTERN const char * ZEXPORT z_zlibVersion OF((void)) { return zlibVersion();  }


static void* ZWRAP_allocFunction(void* opaque, size_t size)
{
    z_streamp strm = (z_streamp) opaque;
    void* address = strm->zalloc(strm->opaque, 1, size);
  /*  printf("ZWRAP alloc %p, %d \n", address, (int)size); */
    return address;
}

static void ZWRAP_freeFunction(void* opaque, void* address)
{
    z_streamp strm = (z_streamp) opaque;
    strm->zfree(strm->opaque, address);
   /* if (address) printf("ZWRAP free %p \n", address); */
}



/* *** Compression *** */

typedef struct {
    ZSTD_CStream* zbc;
    int compressionLevel;
    ZSTD_customMem customMem;
    z_stream allocFunc; /* copy of zalloc, zfree, opaque */
    ZSTD_inBuffer inBuffer;
    ZSTD_outBuffer outBuffer;
} ZWRAP_CCtx;


size_t ZWRAP_freeCCtx(ZWRAP_CCtx* zwc)
{
    if (zwc==NULL) return 0;   /* support free on NULL */
    ZSTD_freeCStream(zwc->zbc);
    zwc->customMem.customFree(zwc->customMem.opaque, zwc);
    return 0;
}


ZWRAP_CCtx* ZWRAP_createCCtx(z_streamp strm)
{
    ZWRAP_CCtx* zwc;

    if (strm->zalloc && strm->zfree) {
        zwc = (ZWRAP_CCtx*)strm->zalloc(strm->opaque, 1, sizeof(ZWRAP_CCtx));
        if (zwc==NULL) return NULL;
        memset(zwc, 0, sizeof(ZWRAP_CCtx));
        memcpy(&zwc->allocFunc, strm, sizeof(z_stream));
        { ZSTD_customMem ZWRAP_customMem = { ZWRAP_allocFunction, ZWRAP_freeFunction, &zwc->allocFunc };
          memcpy(&zwc->customMem, &ZWRAP_customMem, sizeof(ZSTD_customMem));
        }
    } else {
        zwc = (ZWRAP_CCtx*)defaultCustomMem.customAlloc(defaultCustomMem.opaque, sizeof(ZWRAP_CCtx));
        if (zwc==NULL) return NULL;
        memset(zwc, 0, sizeof(ZWRAP_CCtx));
        memcpy(&zwc->customMem, &defaultCustomMem, sizeof(ZSTD_customMem));
    }

    zwc->zbc = ZSTD_createCStream_advanced(zwc->customMem);
    if (zwc->zbc == NULL) { ZWRAP_freeCCtx(zwc); return NULL; }
    return zwc;
}


int ZWRAPC_finish_with_error(ZWRAP_CCtx* zwc, z_streamp strm, int error)
{
    LOG_WRAPPERC("- ZWRAPC_finish_with_error=%d\n", error);
    if (zwc) ZWRAP_freeCCtx(zwc);
    if (strm) strm->state = NULL;
    return (error) ? error : Z_STREAM_ERROR;
}


ZEXTERN int ZEXPORT z_deflateInit_ OF((z_streamp strm, int level,
                                     const char *version, int stream_size))
{
    ZWRAP_CCtx* zwc;

    LOG_WRAPPERC("- deflateInit level=%d\n", level);
    if (!g_useZSTD) {
        return deflateInit_((strm), (level), version, stream_size);
    }

    zwc = ZWRAP_createCCtx(strm);
    if (zwc == NULL) return Z_MEM_ERROR;

    if (level == Z_DEFAULT_COMPRESSION)
        level = ZWRAP_DEFAULT_CLEVEL;

    { size_t const errorCode = ZSTD_initCStream(zwc->zbc, level);
      if (ZSTD_isError(errorCode)) return ZWRAPC_finish_with_error(zwc, strm, 0); }

    zwc->compressionLevel = level;
    strm->state = (struct internal_state*) zwc; /* use state which in not used by user */
    strm->total_in = 0;
    strm->total_out = 0;
    return Z_OK;
}


ZEXTERN int ZEXPORT z_deflateInit2_ OF((z_streamp strm, int level, int method,
                                      int windowBits, int memLevel,
                                      int strategy, const char *version,
                                      int stream_size))
{
    if (!g_useZSTD)
        return deflateInit2_(strm, level, method, windowBits, memLevel, strategy, version, stream_size);

    return z_deflateInit_ (strm, level, version, stream_size);
}


ZEXTERN int ZEXPORT z_deflateReset OF((z_streamp strm))
{
    LOG_WRAPPERC("- deflateReset\n");
    if (!g_useZSTD)
        return deflateReset(strm);

    {   ZWRAP_CCtx* zwc = (ZWRAP_CCtx*) strm->state;
        if (zwc == NULL) return Z_STREAM_ERROR;
        { size_t const errorCode = ZSTD_resetCStream(zwc->zbc, 0);
          if (ZSTD_isError(errorCode)) return ZWRAPC_finish_with_error(zwc, strm, 0); }
    }
    
    strm->total_in = 0;
    strm->total_out = 0;
    return Z_OK;
}


ZEXTERN int ZEXPORT z_deflateSetDictionary OF((z_streamp strm,
                                             const Bytef *dictionary,
                                             uInt  dictLength))
{
    if (!g_useZSTD) {
        LOG_WRAPPERC("- deflateSetDictionary\n");
        return deflateSetDictionary(strm, dictionary, dictLength);
    }

    {   ZWRAP_CCtx* zwc = (ZWRAP_CCtx*) strm->state;
        LOG_WRAPPERC("- deflateSetDictionary level=%d\n", (int)zwc->compressionLevel);
        if (zwc == NULL) return Z_STREAM_ERROR;
        { size_t const errorCode = ZSTD_initCStream_usingDict(zwc->zbc, dictionary, dictLength, zwc->compressionLevel);
          if (ZSTD_isError(errorCode)) return ZWRAPC_finish_with_error(zwc, strm, 0); }
    }

    return Z_OK;
}


ZEXTERN int ZEXPORT z_deflate OF((z_streamp strm, int flush))
{
    ZWRAP_CCtx* zwc;

    if (!g_useZSTD) {
        int res;
        LOG_WRAPPERC("- deflate1 flush=%d avail_in=%d avail_out=%d total_in=%d total_out=%d\n", (int)flush, (int)strm->avail_in, (int)strm->avail_out, (int)strm->total_in, (int)strm->total_out);
        res = deflate(strm, flush);
        LOG_WRAPPERC("- deflate2 flush=%d avail_in=%d avail_out=%d total_in=%d total_out=%d\n", (int)flush, (int)strm->avail_in, (int)strm->avail_out, (int)strm->total_in, (int)strm->total_out);
        return res;
    }

    zwc = (ZWRAP_CCtx*) strm->state;
    if (zwc == NULL) return Z_STREAM_ERROR;

    LOG_WRAPPERC("- deflate1 flush=%d avail_in=%d avail_out=%d total_in=%d total_out=%d\n", (int)flush, (int)strm->avail_in, (int)strm->avail_out, (int)strm->total_in, (int)strm->total_out);
    if (strm->avail_in > 0) {
        zwc->inBuffer.src = strm->next_in;
        zwc->inBuffer.size = strm->avail_in;
        zwc->inBuffer.pos = 0;
        zwc->outBuffer.dst = strm->next_out;
        zwc->outBuffer.size = strm->avail_out;
        zwc->outBuffer.pos = 0;
        { size_t const errorCode = ZSTD_compressStream(zwc->zbc, &zwc->outBuffer, &zwc->inBuffer);
          LOG_WRAPPERC("deflate ZSTD_compressStream srcSize=%d dstCapacity=%d\n", (int)zwc->inBuffer.size, (int)zwc->outBuffer.size);
          if (ZSTD_isError(errorCode)) return ZWRAPC_finish_with_error(zwc, strm, 0);
        }
        strm->next_out += zwc->outBuffer.pos;
        strm->total_out += zwc->outBuffer.pos;
        strm->avail_out -= zwc->outBuffer.pos;
        strm->total_in += zwc->inBuffer.pos;
        strm->next_in += zwc->inBuffer.pos;
        strm->avail_in -= zwc->inBuffer.pos;
    }

    if (flush == Z_FULL_FLUSH || flush == Z_BLOCK || flush == Z_TREES) FINISH_WITH_ERR(strm, "Z_FULL_FLUSH, Z_BLOCK and Z_TREES are not supported!");

    if (flush == Z_FINISH) {
        size_t bytesLeft;
        zwc->outBuffer.dst = strm->next_out;
        zwc->outBuffer.size = strm->avail_out;
        zwc->outBuffer.pos = 0;
        bytesLeft = ZSTD_endStream(zwc->zbc, &zwc->outBuffer);
        LOG_WRAPPERC("deflate ZSTD_endStream dstCapacity=%d bytesLeft=%d\n", (int)strm->avail_out, (int)bytesLeft);
        if (ZSTD_isError(bytesLeft)) return ZWRAPC_finish_with_error(zwc, strm, 0);
        strm->next_out += zwc->outBuffer.pos;
        strm->total_out += zwc->outBuffer.pos;
        strm->avail_out -= zwc->outBuffer.pos;
        if (bytesLeft == 0) { LOG_WRAPPERC("Z_STREAM_END2 strm->total_in=%d strm->avail_out=%d strm->total_out=%d\n", (int)strm->total_in, (int)strm->avail_out, (int)strm->total_out); return Z_STREAM_END; }
    }
    else
    if (flush == Z_SYNC_FLUSH || flush == Z_PARTIAL_FLUSH) {
        size_t bytesLeft;
        zwc->outBuffer.dst = strm->next_out;
        zwc->outBuffer.size = strm->avail_out;
        zwc->outBuffer.pos = 0;
        bytesLeft = ZSTD_flushStream(zwc->zbc, &zwc->outBuffer);
        LOG_WRAPPERC("deflate ZSTD_flushStream dstCapacity=%d bytesLeft=%d\n", (int)strm->avail_out, (int)bytesLeft);
        if (ZSTD_isError(bytesLeft)) return ZWRAPC_finish_with_error(zwc, strm, 0);
        strm->next_out += zwc->outBuffer.pos;
        strm->total_out += zwc->outBuffer.pos;
        strm->avail_out -= zwc->outBuffer.pos;
    }
    LOG_WRAPPERC("- deflate2 flush=%d avail_in=%d avail_out=%d total_in=%d total_out=%d\n", (int)flush, (int)strm->avail_in, (int)strm->avail_out, (int)strm->total_in, (int)strm->total_out);
    return Z_OK;
}


ZEXTERN int ZEXPORT z_deflateEnd OF((z_streamp strm))
{
    if (!g_useZSTD) {
        LOG_WRAPPERC("- deflateEnd\n");
        return deflateEnd(strm);
    }
    LOG_WRAPPERC("- deflateEnd total_in=%d total_out=%d\n", (int)(strm->total_in), (int)(strm->total_out));
    {   size_t errorCode;
        ZWRAP_CCtx* zwc = (ZWRAP_CCtx*) strm->state;
        if (zwc == NULL) return Z_OK;  /* structures are already freed */
        strm->state = NULL;
        errorCode = ZWRAP_freeCCtx(zwc);
        if (ZSTD_isError(errorCode)) return Z_STREAM_ERROR;
    }
    return Z_OK;
}


ZEXTERN uLong ZEXPORT z_deflateBound OF((z_streamp strm,
                                       uLong sourceLen))
{
    if (!g_useZSTD)
        return deflateBound(strm, sourceLen);

    return ZSTD_compressBound(sourceLen);
}


ZEXTERN int ZEXPORT z_deflateParams OF((z_streamp strm,
                                      int level,
                                      int strategy))
{
    if (!g_useZSTD) {
        LOG_WRAPPERC("- deflateParams level=%d strategy=%d\n", level, strategy);
        return deflateParams(strm, level, strategy);
    }

    return Z_OK;
}





/* *** Decompression *** */

typedef struct {
    ZSTD_DStream* zbd;
    char headerBuf[16]; /* should be equal or bigger than ZSTD_frameHeaderSize_min */
    int errorCount;
    int decompState;
    ZSTD_inBuffer inBuffer;
    ZSTD_outBuffer outBuffer;

    /* zlib params */
    int stream_size;
    char *version;
    int windowBits;
    ZSTD_customMem customMem;
    z_stream allocFunc; /* copy of zalloc, zfree, opaque */
} ZWRAP_DCtx;


void ZWRAP_initDCtx(ZWRAP_DCtx* zwd)
{
    zwd->errorCount = zwd->decompState = 0;
    zwd->outBuffer.pos = 0;
    zwd->outBuffer.size = 0;
}

ZWRAP_DCtx* ZWRAP_createDCtx(z_streamp strm)
{
    ZWRAP_DCtx* zwd;

    if (strm->zalloc && strm->zfree) {
        zwd = (ZWRAP_DCtx*)strm->zalloc(strm->opaque, 1, sizeof(ZWRAP_DCtx));
        if (zwd==NULL) return NULL;
        memset(zwd, 0, sizeof(ZWRAP_DCtx));
        memcpy(&zwd->allocFunc, strm, sizeof(z_stream));
        { ZSTD_customMem ZWRAP_customMem = { ZWRAP_allocFunction, ZWRAP_freeFunction, &zwd->allocFunc };
          memcpy(&zwd->customMem, &ZWRAP_customMem, sizeof(ZSTD_customMem));
        }
    } else {
        zwd = (ZWRAP_DCtx*)defaultCustomMem.customAlloc(defaultCustomMem.opaque, sizeof(ZWRAP_DCtx));
        if (zwd==NULL) return NULL;
        memset(zwd, 0, sizeof(ZWRAP_DCtx));
        memcpy(&zwd->customMem, &defaultCustomMem, sizeof(ZSTD_customMem));
    }

    ZWRAP_initDCtx(zwd);
    return zwd;
}


size_t ZWRAP_freeDCtx(ZWRAP_DCtx* zwd)
{
    if (zwd==NULL) return 0;   /* support free on null */
    ZSTD_freeDStream(zwd->zbd);
    if (zwd->version) zwd->customMem.customFree(zwd->customMem.opaque, zwd->version);
    zwd->customMem.customFree(zwd->customMem.opaque, zwd);
    return 0;
}


int ZWRAPD_finish_with_error(ZWRAP_DCtx* zwd, z_streamp strm, int error)
{
    LOG_WRAPPERD("- ZWRAPD_finish_with_error=%d\n", error);
    if (zwd) ZWRAP_freeDCtx(zwd);
    if (strm) strm->state = NULL;
    return (error) ? error : Z_STREAM_ERROR;
}


ZEXTERN int ZEXPORT z_inflateInit_ OF((z_streamp strm,
                                     const char *version, int stream_size))
{
    ZWRAP_DCtx* zwd = ZWRAP_createDCtx(strm);
    LOG_WRAPPERD("- inflateInit\n");
    if (zwd == NULL) return ZWRAPD_finish_with_error(zwd, strm, 0);

    zwd->version = zwd->customMem.customAlloc(zwd->customMem.opaque, strlen(version) + 1);
    if (zwd->version == NULL) return ZWRAPD_finish_with_error(zwd, strm, 0);
    strcpy(zwd->version, version);

    zwd->stream_size = stream_size;
    strm->state = (struct internal_state*) zwd; /* use state which in not used by user */
    strm->total_in = 0;
    strm->total_out = 0;
    strm->reserved = 1; /* mark as unknown steam */

    return Z_OK;
}


ZEXTERN int ZEXPORT z_inflateInit2_ OF((z_streamp strm, int  windowBits,
                                      const char *version, int stream_size))
{
    int ret = z_inflateInit_ (strm, version, stream_size);
    if (ret == Z_OK) {
        ZWRAP_DCtx* zwd = (ZWRAP_DCtx*)strm->state;
        if (zwd == NULL) return Z_STREAM_ERROR;
        zwd->windowBits = windowBits;
    }
    return ret;
}


ZEXTERN int ZEXPORT z_inflateReset OF((z_streamp strm))
{
    LOG_WRAPPERD("- inflateReset\n");
    if (!strm->reserved)
        return inflateReset(strm);

    {   ZWRAP_DCtx* zwd = (ZWRAP_DCtx*) strm->state;
        if (zwd == NULL) return Z_STREAM_ERROR;
        { size_t const errorCode = ZSTD_resetDStream(zwd->zbd);
          if (ZSTD_isError(errorCode)) return ZWRAPD_finish_with_error(zwd, strm, 0); }
        ZWRAP_initDCtx(zwd);
    }
    
    strm->total_in = 0;
    strm->total_out = 0;
    return Z_OK;
}


#if ZLIB_VERNUM >= 0x1240
ZEXTERN int ZEXPORT z_inflateReset2 OF((z_streamp strm,
                                      int windowBits))
{
    if (!strm->reserved)
        return inflateReset2(strm, windowBits);

    {   int ret = z_inflateReset (strm);
        if (ret == Z_OK) {
            ZWRAP_DCtx* zwd = (ZWRAP_DCtx*)strm->state;
            if (zwd == NULL) return Z_STREAM_ERROR;
            zwd->windowBits = windowBits;
        }
        return ret;
    }
}
#endif


ZEXTERN int ZEXPORT z_inflateSetDictionary OF((z_streamp strm,
                                             const Bytef *dictionary,
                                             uInt  dictLength))
{
    LOG_WRAPPERD("- inflateSetDictionary\n");
    if (!strm->reserved)
        return inflateSetDictionary(strm, dictionary, dictLength);

    {   size_t errorCode;
        ZWRAP_DCtx* zwd = (ZWRAP_DCtx*) strm->state;
        if (zwd == NULL) return Z_STREAM_ERROR;
        errorCode = ZSTD_initDStream_usingDict(zwd->zbd, dictionary, dictLength);
        if (ZSTD_isError(errorCode)) return ZWRAPD_finish_with_error(zwd, strm, 0);

        if (strm->total_in == ZSTD_HEADERSIZE) {
            zwd->inBuffer.src = zwd->headerBuf;
            zwd->inBuffer.size = strm->total_in;
            zwd->inBuffer.pos = 0;
            zwd->outBuffer.dst = strm->next_out;
            zwd->outBuffer.size = 0;
            zwd->outBuffer.pos = 0;
            errorCode = ZSTD_decompressStream(zwd->zbd, &zwd->outBuffer, &zwd->inBuffer);
            LOG_WRAPPERD("inflateSetDictionary ZSTD_decompressStream errorCode=%d srcSize=%d dstCapacity=%d\n", (int)errorCode, (int)zwd->inBuffer.size, (int)zwd->outBuffer.size);
            if (zwd->inBuffer.pos < zwd->outBuffer.size || ZSTD_isError(errorCode)) {
                LOG_WRAPPERD("ERROR: ZSTD_decompressStream %s\n", ZSTD_getErrorName(errorCode));
                return ZWRAPD_finish_with_error(zwd, strm, 0);
            }
        }
    }

    return Z_OK;
}


ZEXTERN int ZEXPORT z_inflate OF((z_streamp strm, int flush))
{
    int res;
    if (!strm->reserved) {
        LOG_WRAPPERD("- inflate1 flush=%d avail_in=%d avail_out=%d total_in=%d total_out=%d\n", (int)flush, (int)strm->avail_in, (int)strm->avail_out, (int)strm->total_in, (int)strm->total_out);
        res = inflate(strm, flush);
        LOG_WRAPPERD("- inflate2 flush=%d avail_in=%d avail_out=%d total_in=%d total_out=%d res=%d\n", (int)flush, (int)strm->avail_in, (int)strm->avail_out, (int)strm->total_in, (int)strm->total_out, res);
        return res;
    }

    if (strm->avail_in > 0) {
        size_t errorCode, srcSize, inPos;
        ZWRAP_DCtx* zwd = (ZWRAP_DCtx*) strm->state;
        if (zwd == NULL) return Z_STREAM_ERROR;
        LOG_WRAPPERD("- inflate1 flush=%d avail_in=%d avail_out=%d total_in=%d total_out=%d\n", (int)flush, (int)strm->avail_in, (int)strm->avail_out, (int)strm->total_in, (int)strm->total_out);

        if (zwd->decompState == Z_STREAM_END) return Z_STREAM_END;

     //   if (((strm->avail_in < ZSTD_HEADERSIZE) || (strm->total_in > 0)) && (strm->total_in < ZLIB_HEADERSIZE))
        if (strm->total_in < ZLIB_HEADERSIZE)
        {
          //  printf(".");
            srcSize = MIN(strm->avail_in, ZLIB_HEADERSIZE - strm->total_in);
            memcpy(zwd->headerBuf+strm->total_in, strm->next_in, srcSize);
            strm->total_in += srcSize;
            strm->next_in += srcSize;
            strm->avail_in -= srcSize;
            if (strm->total_in < ZLIB_HEADERSIZE) return Z_OK;

            if (MEM_readLE32(zwd->headerBuf) != ZSTD_MAGICNUMBER) {
                z_stream strm2;
                strm2.next_in = strm->next_in;
                strm2.avail_in = strm->avail_in;
                strm2.next_out = strm->next_out;
                strm2.avail_out = strm->avail_out;

                if (zwd->windowBits)
                    errorCode = inflateInit2_(strm, zwd->windowBits, zwd->version, zwd->stream_size);
                else
                    errorCode = inflateInit_(strm, zwd->version, zwd->stream_size);
                LOG_WRAPPERD("ZLIB inflateInit errorCode=%d\n", (int)errorCode);
                if (errorCode != Z_OK) return ZWRAPD_finish_with_error(zwd, strm, (int)errorCode);

                /* inflate header */
                strm->next_in = (unsigned char*)zwd->headerBuf;
                strm->avail_in = ZLIB_HEADERSIZE;
                strm->avail_out = 0;
                errorCode = inflate(strm, Z_NO_FLUSH);
                LOG_WRAPPERD("ZLIB inflate errorCode=%d strm->avail_in=%d\n", (int)errorCode, (int)strm->avail_in);
                if (errorCode != Z_OK) return ZWRAPD_finish_with_error(zwd, strm, (int)errorCode);
                if (strm->avail_in > 0) goto error;

                strm->next_in = strm2.next_in;
                strm->avail_in = strm2.avail_in;
                strm->next_out = strm2.next_out;
                strm->avail_out = strm2.avail_out;

                strm->reserved = 0; /* mark as zlib stream */
                errorCode = ZWRAP_freeDCtx(zwd);
                if (ZSTD_isError(errorCode)) goto error;

                if (flush == Z_INFLATE_SYNC) res = inflateSync(strm);
                else res = inflate(strm, flush);
                LOG_WRAPPERD("- inflate2 flush=%d avail_in=%d avail_out=%d total_in=%d total_out=%d res=%d\n", (int)flush, (int)strm->avail_in, (int)strm->avail_out, (int)strm->total_in, (int)strm->total_out, res);
                return res;
            }
        }

     //   if (((strm->avail_in < ZSTD_HEADERSIZE) || (strm->total_in > 0)) && (strm->total_in < ZSTD_HEADERSIZE))
        if (strm->total_in < ZSTD_HEADERSIZE)
        {
          //  printf("+");
            srcSize = MIN(strm->avail_in, ZSTD_HEADERSIZE - strm->total_in);
            memcpy(zwd->headerBuf+strm->total_in, strm->next_in, srcSize);
            strm->total_in += srcSize;
            strm->next_in += srcSize;
            strm->avail_in -= srcSize;
            if (strm->total_in < ZSTD_HEADERSIZE) return Z_OK;

            zwd->zbd = ZSTD_createDStream_advanced(zwd->customMem);
            if (zwd->zbd == NULL) goto error;

            errorCode = ZSTD_initDStream(zwd->zbd);
            if (ZSTD_isError(errorCode)) goto error;

            if (flush == Z_INFLATE_SYNC) { strm->msg = "inflateSync is not supported!"; goto error; }

            inPos = zwd->inBuffer.pos;
            zwd->inBuffer.src = zwd->headerBuf;
            zwd->inBuffer.size = ZSTD_HEADERSIZE;
            zwd->inBuffer.pos = 0;
            zwd->outBuffer.dst = strm->next_out;
            zwd->outBuffer.size = 0;
            zwd->outBuffer.pos = 0;
            errorCode = ZSTD_decompressStream(zwd->zbd, &zwd->outBuffer, &zwd->inBuffer);
            LOG_WRAPPERD("inflate ZSTD_decompressStream1 errorCode=%d srcSize=%d dstCapacity=%d\n", (int)errorCode, (int)zwd->inBuffer.size, (int)zwd->outBuffer.size);
            if (ZSTD_isError(errorCode)) {
                LOG_WRAPPERD("ERROR: ZSTD_decompressStream1 %s\n", ZSTD_getErrorName(errorCode));
                goto error;
            }
          //  LOG_WRAPPERD("1srcSize=%d inpos=%d inBuffer.pos=%d inBuffer.size=%d outBuffer.pos=%d\n", (int)srcSize, (int)inPos, (int)zwd->inBuffer.pos, (int)zwd->inBuffer.size, (int)zwd->outBuffer.pos);
            if (zwd->inBuffer.pos != zwd->inBuffer.size) return ZWRAPD_finish_with_error(zwd, strm, 0); /* not consumed */
        }

        inPos = 0;//zwd->inBuffer.pos;
        zwd->inBuffer.src = strm->next_in;
        zwd->inBuffer.size = strm->avail_in;
        zwd->inBuffer.pos = 0;
        zwd->outBuffer.dst = strm->next_out;
        zwd->outBuffer.size = strm->avail_out;
        zwd->outBuffer.pos = 0;
        errorCode = ZSTD_decompressStream(zwd->zbd, &zwd->outBuffer, &zwd->inBuffer);
        LOG_WRAPPERD("inflate ZSTD_decompressStream2 errorCode=%d srcSize=%d dstCapacity=%d\n", (int)errorCode, (int)strm->avail_in, (int)strm->avail_out);
        if (ZSTD_isError(errorCode)) {
            zwd->errorCount++;
            LOG_WRAPPERD("ERROR: ZSTD_decompressStream2 %s zwd->errorCount=%d\n", ZSTD_getErrorName(errorCode), zwd->errorCount);
            if (zwd->errorCount<=1) return Z_NEED_DICT; else goto error;
        }
        LOG_WRAPPERD("inflate inpos=%d inBuffer.pos=%d inBuffer.size=%d outBuffer.pos=%d outBuffer.size=%d o\n", (int)inPos, (int)zwd->inBuffer.pos, (int)zwd->inBuffer.size, (int)zwd->outBuffer.pos, (int)zwd->outBuffer.size);
        strm->next_out += zwd->outBuffer.pos;
        strm->total_out += zwd->outBuffer.pos;
        strm->avail_out -= zwd->outBuffer.pos;
        strm->total_in += zwd->inBuffer.pos - inPos;
        strm->next_in += zwd->inBuffer.pos - inPos;
        strm->avail_in -= zwd->inBuffer.pos - inPos;
        if (errorCode == 0) { 
            LOG_WRAPPERD("inflate Z_STREAM_END1 avail_in=%d avail_out=%d total_in=%d total_out=%d\n", (int)strm->avail_in, (int)strm->avail_out, (int)strm->total_in, (int)strm->total_out); 
            zwd->decompState = Z_STREAM_END; 
            return Z_STREAM_END;
        }
        goto finish;
error:
        return ZWRAPD_finish_with_error(zwd, strm, 0);
    }
finish:
    LOG_WRAPPERD("- inflate2 flush=%d avail_in=%d avail_out=%d total_in=%d total_out=%d res=%d\n", (int)flush, (int)strm->avail_in, (int)strm->avail_out, (int)strm->total_in, (int)strm->total_out, Z_OK);
    return Z_OK;
}


ZEXTERN int ZEXPORT z_inflateEnd OF((z_streamp strm))
{
    if (!strm->reserved)
        return inflateEnd(strm);

    LOG_WRAPPERD("- inflateEnd total_in=%d total_out=%d\n", (int)(strm->total_in), (int)(strm->total_out));
    {   size_t errorCode;
        ZWRAP_DCtx* zwd = (ZWRAP_DCtx*) strm->state;
        if (zwd == NULL) return Z_OK;  /* structures are already freed */
        strm->state = NULL;
        errorCode = ZWRAP_freeDCtx(zwd);
        if (ZSTD_isError(errorCode)) return Z_STREAM_ERROR;
    }
    return Z_OK;
}


ZEXTERN int ZEXPORT z_inflateSync OF((z_streamp strm))
{
    if (!strm->reserved) {
        return inflateSync(strm);
    }

    return z_inflate(strm, Z_INFLATE_SYNC);
}




/* Advanced compression functions */
ZEXTERN int ZEXPORT z_deflateCopy OF((z_streamp dest,
                                    z_streamp source))
{
    if (!g_useZSTD)
        return deflateCopy(dest, source);
    FINISH_WITH_ERR(source, "deflateCopy is not supported!");
}


ZEXTERN int ZEXPORT z_deflateTune OF((z_streamp strm,
                                    int good_length,
                                    int max_lazy,
                                    int nice_length,
                                    int max_chain))
{
    if (!g_useZSTD)
        return deflateTune(strm, good_length, max_lazy, nice_length, max_chain);
    FINISH_WITH_ERR(strm, "deflateTune is not supported!");
}


#if ZLIB_VERNUM >= 0x1260
ZEXTERN int ZEXPORT z_deflatePending OF((z_streamp strm,
                                       unsigned *pending,
                                       int *bits))
{
    if (!g_useZSTD)
        return deflatePending(strm, pending, bits);
    FINISH_WITH_ERR(strm, "deflatePending is not supported!");
}
#endif


ZEXTERN int ZEXPORT z_deflatePrime OF((z_streamp strm,
                                     int bits,
                                     int value))
{
    if (!g_useZSTD)
        return deflatePrime(strm, bits, value);
    FINISH_WITH_ERR(strm, "deflatePrime is not supported!");
}


ZEXTERN int ZEXPORT z_deflateSetHeader OF((z_streamp strm,
                                         gz_headerp head))
{
    if (!g_useZSTD)
        return deflateSetHeader(strm, head);
    FINISH_WITH_ERR(strm, "deflateSetHeader is not supported!");
}




/* Advanced decompression functions */
#if ZLIB_VERNUM >= 0x1280
ZEXTERN int ZEXPORT z_inflateGetDictionary OF((z_streamp strm,
                                             Bytef *dictionary,
                                             uInt  *dictLength))
{
    if (!strm->reserved)
        return inflateGetDictionary(strm, dictionary, dictLength);
    FINISH_WITH_ERR(strm, "inflateGetDictionary is not supported!");
}
#endif


ZEXTERN int ZEXPORT z_inflateCopy OF((z_streamp dest,
                                    z_streamp source))
{
    if (!g_useZSTD)
        return inflateCopy(dest, source);
    FINISH_WITH_ERR(source, "inflateCopy is not supported!");
}


#if ZLIB_VERNUM >= 0x1240
ZEXTERN long ZEXPORT z_inflateMark OF((z_streamp strm))
{
    if (!strm->reserved)
        return inflateMark(strm);
    FINISH_WITH_ERR(strm, "inflateMark is not supported!");
}
#endif


ZEXTERN int ZEXPORT z_inflatePrime OF((z_streamp strm,
                                     int bits,
                                     int value))
{
    if (!strm->reserved)
        return inflatePrime(strm, bits, value);
    FINISH_WITH_ERR(strm, "inflatePrime is not supported!");
}


ZEXTERN int ZEXPORT z_inflateGetHeader OF((z_streamp strm,
                                         gz_headerp head))
{
    if (!strm->reserved)
        return inflateGetHeader(strm, head);
    FINISH_WITH_ERR(strm, "inflateGetHeader is not supported!");
}


ZEXTERN int ZEXPORT z_inflateBackInit_ OF((z_streamp strm, int windowBits,
                                         unsigned char FAR *window,
                                         const char *version,
                                         int stream_size))
{
    if (!strm->reserved)
        return inflateBackInit_(strm, windowBits, window, version, stream_size);
    FINISH_WITH_ERR(strm, "inflateBackInit is not supported!");
}


ZEXTERN int ZEXPORT z_inflateBack OF((z_streamp strm,
                                    in_func in, void FAR *in_desc,
                                    out_func out, void FAR *out_desc))
{
    if (!strm->reserved)
        return inflateBack(strm, in, in_desc, out, out_desc);
    FINISH_WITH_ERR(strm, "inflateBack is not supported!");
}


ZEXTERN int ZEXPORT z_inflateBackEnd OF((z_streamp strm))
{
    if (!strm->reserved)
        return inflateBackEnd(strm);
    FINISH_WITH_ERR(strm, "inflateBackEnd is not supported!");
}


ZEXTERN uLong ZEXPORT z_zlibCompileFlags OF((void)) { return zlibCompileFlags(); };



                        /* utility functions */
#ifndef Z_SOLO

ZEXTERN int ZEXPORT z_compress OF((Bytef *dest,   uLongf *destLen,
                                 const Bytef *source, uLong sourceLen))
{
    if (!g_useZSTD)
        return compress(dest, destLen, source, sourceLen);

    { size_t dstCapacity = *destLen;
      size_t const errorCode = ZSTD_compress(dest, dstCapacity, source, sourceLen, ZWRAP_DEFAULT_CLEVEL);
      LOG_WRAPPERD("z_compress sourceLen=%d dstCapacity=%d\n", (int)sourceLen, (int)dstCapacity);
      if (ZSTD_isError(errorCode)) return Z_STREAM_ERROR;
      *destLen = errorCode;
    }
    return Z_OK;
}


ZEXTERN int ZEXPORT z_compress2 OF((Bytef *dest,   uLongf *destLen,
                                  const Bytef *source, uLong sourceLen,
                                  int level))
{
    if (!g_useZSTD)
        return compress2(dest, destLen, source, sourceLen, level);

    { size_t dstCapacity = *destLen;
      size_t const errorCode = ZSTD_compress(dest, dstCapacity, source, sourceLen, level);
      if (ZSTD_isError(errorCode)) return Z_STREAM_ERROR;
      *destLen = errorCode;
    }
    return Z_OK;
}


ZEXTERN uLong ZEXPORT z_compressBound OF((uLong sourceLen))
{
    if (!g_useZSTD)
        return compressBound(sourceLen);

    return ZSTD_compressBound(sourceLen);
}


ZEXTERN int ZEXPORT z_uncompress OF((Bytef *dest,   uLongf *destLen,
                                   const Bytef *source, uLong sourceLen))
{
    if (sourceLen < 4 || MEM_readLE32(source) != ZSTD_MAGICNUMBER)
        return uncompress(dest, destLen, source, sourceLen);

    { size_t dstCapacity = *destLen;
      size_t const errorCode = ZSTD_decompress(dest, dstCapacity, source, sourceLen);
      if (ZSTD_isError(errorCode)) return Z_STREAM_ERROR;
      *destLen = errorCode;
     }
    return Z_OK;
}



                        /* gzip file access functions */
ZEXTERN gzFile ZEXPORT z_gzopen OF((const char *path, const char *mode))
{
    if (!g_useZSTD)
        return gzopen(path, mode);
    FINISH_WITH_NULL_ERR("gzopen is not supported!");
}


ZEXTERN gzFile ZEXPORT z_gzdopen OF((int fd, const char *mode))
{
    if (!g_useZSTD)
        return gzdopen(fd, mode);
    FINISH_WITH_NULL_ERR("gzdopen is not supported!");
}


#if ZLIB_VERNUM >= 0x1240
ZEXTERN int ZEXPORT z_gzbuffer OF((gzFile file, unsigned size))
{
    if (!g_useZSTD)
        return gzbuffer(file, size);
    FINISH_WITH_GZ_ERR("gzbuffer is not supported!");
}


ZEXTERN z_off_t ZEXPORT z_gzoffset OF((gzFile file))
{
    if (!g_useZSTD)
        return gzoffset(file);
    FINISH_WITH_GZ_ERR("gzoffset is not supported!");
}


ZEXTERN int ZEXPORT z_gzclose_r OF((gzFile file))
{
    if (!g_useZSTD)
        return gzclose_r(file);
    FINISH_WITH_GZ_ERR("gzclose_r is not supported!");
}


ZEXTERN int ZEXPORT z_gzclose_w OF((gzFile file))
{
    if (!g_useZSTD)
        return gzclose_w(file);
    FINISH_WITH_GZ_ERR("gzclose_w is not supported!");
}
#endif


ZEXTERN int ZEXPORT z_gzsetparams OF((gzFile file, int level, int strategy))
{
    if (!g_useZSTD)
        return gzsetparams(file, level, strategy);
    FINISH_WITH_GZ_ERR("gzsetparams is not supported!");
}


ZEXTERN int ZEXPORT z_gzread OF((gzFile file, voidp buf, unsigned len))
{
    if (!g_useZSTD)
        return gzread(file, buf, len);
    FINISH_WITH_GZ_ERR("gzread is not supported!");
}


ZEXTERN int ZEXPORT z_gzwrite OF((gzFile file,
                                voidpc buf, unsigned len))
{
    if (!g_useZSTD)
        return gzwrite(file, buf, len);
    FINISH_WITH_GZ_ERR("gzwrite is not supported!");
}


#if ZLIB_VERNUM >= 0x1260
ZEXTERN int ZEXPORTVA z_gzprintf Z_ARG((gzFile file, const char *format, ...))
#else
ZEXTERN int ZEXPORTVA z_gzprintf OF((gzFile file, const char *format, ...))
#endif
{
    if (!g_useZSTD) {
        int ret;
        char buf[1024];
        va_list args;
        va_start (args, format);
        ret = vsprintf (buf, format, args);
        va_end (args);

        ret = gzprintf(file, buf);
        return ret;
    }
    FINISH_WITH_GZ_ERR("gzprintf is not supported!");
}


ZEXTERN int ZEXPORT z_gzputs OF((gzFile file, const char *s))
{
    if (!g_useZSTD)
        return gzputs(file, s);
    FINISH_WITH_GZ_ERR("gzputs is not supported!");
}


ZEXTERN char * ZEXPORT z_gzgets OF((gzFile file, char *buf, int len))
{
    if (!g_useZSTD)
        return gzgets(file, buf, len);
    FINISH_WITH_NULL_ERR("gzgets is not supported!");
}


ZEXTERN int ZEXPORT z_gzputc OF((gzFile file, int c))
{
    if (!g_useZSTD)
        return gzputc(file, c);
    FINISH_WITH_GZ_ERR("gzputc is not supported!");
}


#if ZLIB_VERNUM == 0x1260
ZEXTERN int ZEXPORT z_gzgetc_ OF((gzFile file))
#else
ZEXTERN int ZEXPORT z_gzgetc OF((gzFile file))
#endif
{
    if (!g_useZSTD)
        return gzgetc(file);
    FINISH_WITH_GZ_ERR("gzgetc is not supported!");
}


ZEXTERN int ZEXPORT z_gzungetc OF((int c, gzFile file))
{
    if (!g_useZSTD)
        return gzungetc(c, file);
    FINISH_WITH_GZ_ERR("gzungetc is not supported!");
}


ZEXTERN int ZEXPORT z_gzflush OF((gzFile file, int flush))
{
    if (!g_useZSTD)
        return gzflush(file, flush);
    FINISH_WITH_GZ_ERR("gzflush is not supported!");
}


ZEXTERN z_off_t ZEXPORT z_gzseek OF((gzFile file, z_off_t offset, int whence))
{
    if (!g_useZSTD)
        return gzseek(file, offset, whence);
    FINISH_WITH_GZ_ERR("gzseek is not supported!");
}


ZEXTERN int ZEXPORT    z_gzrewind OF((gzFile file))
{
    if (!g_useZSTD)
        return gzrewind(file);
    FINISH_WITH_GZ_ERR("gzrewind is not supported!");
}


ZEXTERN z_off_t ZEXPORT    z_gztell OF((gzFile file))
{
    if (!g_useZSTD)
        return gztell(file);
    FINISH_WITH_GZ_ERR("gztell is not supported!");
}


ZEXTERN int ZEXPORT z_gzeof OF((gzFile file))
{
    if (!g_useZSTD)
        return gzeof(file);
    FINISH_WITH_GZ_ERR("gzeof is not supported!");
}


ZEXTERN int ZEXPORT z_gzdirect OF((gzFile file))
{
    if (!g_useZSTD)
        return gzdirect(file);
    FINISH_WITH_GZ_ERR("gzdirect is not supported!");
}


ZEXTERN int ZEXPORT    z_gzclose OF((gzFile file))
{
    if (!g_useZSTD)
        return gzclose(file);
    FINISH_WITH_GZ_ERR("gzclose is not supported!");
}


ZEXTERN const char * ZEXPORT z_gzerror OF((gzFile file, int *errnum))
{
    if (!g_useZSTD)
        return gzerror(file, errnum);
    FINISH_WITH_NULL_ERR("gzerror is not supported!");
}


ZEXTERN void ZEXPORT z_gzclearerr OF((gzFile file))
{
    if (!g_useZSTD)
        gzclearerr(file);
}


#endif /* !Z_SOLO */


                        /* checksum functions */

ZEXTERN uLong ZEXPORT z_adler32 OF((uLong adler, const Bytef *buf, uInt len))
{
    return adler32(adler, buf, len);
}

ZEXTERN uLong ZEXPORT z_crc32   OF((uLong crc, const Bytef *buf, uInt len))
{
    return crc32(crc, buf, len);
}
