/*
    zstd_zlibwrapper.c - zstd wrapper for zlib
    Copyright (C) 2016, Przemyslaw Skibinski.

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
*/

#include <stdio.h>           /* fprintf */
#include <stdlib.h>          /* malloc */
#include <stdarg.h>          /* va_list */
#include <zlib.h>
#include "zstd_zlibwrapper.h"
#include "zstd.h"
#include "zstd_static.h"      /* ZSTD_MAGICNUMBER */
#include "zbuff.h"


#define Z_INFLATE_SYNC              8
#define ZWRAP_HEADERSIZE            4
#define ZWRAP_DEFAULT_CLEVEL        5   /* Z_DEFAULT_COMPRESSION is translated to ZWRAP_DEFAULT_CLEVEL for zstd */

#define LOG_WRAPPER(...)  // printf(__VA_ARGS__)


#define MIN(a,b) ((a)<(b)?(a):(b))

#define FINISH_WITH_ERR(msg) { \
    fprintf(stderr, "ERROR: %s\n", msg); \
    return Z_MEM_ERROR; \
}

#define FINISH_WITH_NULL_ERR(msg) { \
    fprintf(stderr, "ERROR: %s\n", msg); \
    return NULL; \
}

#ifndef ZWRAP_USE_ZSTD
    #define ZWRAP_USE_ZSTD 0
#endif

static int g_useZSTD = ZWRAP_USE_ZSTD;   /* 0 = don't use ZSTD */



void useZSTD(int turn_on) { g_useZSTD = turn_on; }

int isUsingZSTD() { return g_useZSTD; }

const char * zstdVersion() { return ZSTD_VERSION_STRING; }

ZEXTERN const char * ZEXPORT z_zlibVersion OF((void)) { return zlibVersion();  }



/* *** Compression *** */

typedef struct {
    ZBUFF_CCtx* zbc;
    size_t bytesLeft;
    int compressionLevel;
} ZWRAP_CCtx;


ZWRAP_CCtx* ZWRAP_createCCtx()
{
    ZWRAP_CCtx* zwc = (ZWRAP_CCtx*)malloc(sizeof(ZWRAP_CCtx));
    if (zwc==NULL) return NULL;
    memset(zwc, 0, sizeof(*zwc));
    zwc->zbc = ZBUFF_createCCtx();
    return zwc;
}


size_t ZWRAP_freeCCtx(ZWRAP_CCtx* zwc)
{
    if (zwc==NULL) return 0;   /* support free on NULL */
    ZBUFF_freeCCtx(zwc->zbc);
    free(zwc);
    return 0;
}


ZEXTERN int ZEXPORT z_deflateInit_ OF((z_streamp strm, int level,
                                     const char *version, int stream_size))
{
    ZWRAP_CCtx* zwc;
    
    if (!g_useZSTD) {
        LOG_WRAPPER("- deflateInit level=%d\n", level);
        return deflateInit_((strm), (level), version, stream_size);
    }

    LOG_WRAPPER("- deflateInit level=%d\n", level);
    zwc = ZWRAP_createCCtx();
    if (zwc == NULL) return Z_MEM_ERROR;

    if (level == Z_DEFAULT_COMPRESSION)
        level = ZWRAP_DEFAULT_CLEVEL;

    { size_t const errorCode = ZBUFF_compressInit(zwc->zbc, level);
      if (ZSTD_isError(errorCode)) return Z_MEM_ERROR; }

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


ZEXTERN int ZEXPORT z_deflateSetDictionary OF((z_streamp strm,
                                             const Bytef *dictionary,
                                             uInt  dictLength))
{
    if (!g_useZSTD)
        return deflateSetDictionary(strm, dictionary, dictLength);

    {   ZWRAP_CCtx* zwc = (ZWRAP_CCtx*) strm->state;
        LOG_WRAPPER("- deflateSetDictionary level=%d\n", (int)strm->data_type);
        { size_t const errorCode = ZBUFF_compressInitDictionary(zwc->zbc, dictionary, dictLength, zwc->compressionLevel);
          if (ZSTD_isError(errorCode)) return Z_MEM_ERROR; }
    }
    
    return Z_OK;
}


ZEXTERN int ZEXPORT z_deflate OF((z_streamp strm, int flush))
{
    ZWRAP_CCtx* zwc;

    if (!g_useZSTD) {
        int res = deflate(strm, flush);
        LOG_WRAPPER("- avail_in=%d total_in=%d total_out=%d\n", (int)strm->avail_in, (int)strm->total_in, (int)strm->total_out);
        return res;
    }

    zwc = (ZWRAP_CCtx*) strm->state;

    LOG_WRAPPER("deflate flush=%d avail_in=%d avail_out=%d total_in=%d total_out=%d\n", (int)flush, (int)strm->avail_in, (int)strm->avail_out, (int)strm->total_in, (int)strm->total_out);
    if (strm->avail_in > 0) {
        size_t dstCapacity = strm->avail_out;
        size_t srcSize = strm->avail_in;
        size_t const errorCode = ZBUFF_compressContinue(zwc->zbc, strm->next_out, &dstCapacity, strm->next_in, &srcSize);
        LOG_WRAPPER("ZBUFF_compressContinue srcSize=%d dstCapacity=%d\n", (int)srcSize, (int)dstCapacity);
        if (ZSTD_isError(errorCode)) return Z_MEM_ERROR;
        strm->next_out += dstCapacity;
        strm->total_out += dstCapacity;
        strm->avail_out -= dstCapacity;
        strm->total_in += srcSize;
        strm->next_in += srcSize;
        strm->avail_in -= srcSize;
    }

    if (flush == Z_FULL_FLUSH) FINISH_WITH_ERR("Z_FULL_FLUSH is not supported!");

    if (flush == Z_FINISH || flush == Z_FULL_FLUSH) {
        size_t bytesLeft;
        size_t dstCapacity = strm->avail_out;
        if (zwc->bytesLeft) {
            bytesLeft = ZBUFF_compressFlush(zwc->zbc, strm->next_out, &dstCapacity);
            LOG_WRAPPER("ZBUFF_compressFlush avail_out=%d dstCapacity=%d bytesLeft=%d\n", (int)strm->avail_out, (int)dstCapacity, (int)bytesLeft);
        } else {
            bytesLeft = ZBUFF_compressEnd(zwc->zbc, strm->next_out, &dstCapacity);
            LOG_WRAPPER("ZBUFF_compressEnd dstCapacity=%d bytesLeft=%d\n", (int)dstCapacity, (int)bytesLeft);
      //      { size_t const errorCode = ZBUFF_compressInit(zwc->zbc, 1);
      //        if (ZSTD_isError(errorCode)) return Z_MEM_ERROR; }
        }
        if (ZSTD_isError(bytesLeft)) return Z_MEM_ERROR;
        strm->next_out += dstCapacity;
        strm->total_out += dstCapacity;
        strm->avail_out -= dstCapacity;
        if (flush == Z_FINISH && bytesLeft == 0) return Z_STREAM_END;
        zwc->bytesLeft = bytesLeft;
    }
    return Z_OK;
}


ZEXTERN int ZEXPORT z_deflateEnd OF((z_streamp strm)) 
{
    if (!g_useZSTD) {
        LOG_WRAPPER("- deflateEnd\n");
        return deflateEnd(strm);
    }
    LOG_WRAPPER("- deflateEnd total_in=%d total_out=%d\n", (int)(strm->total_in), (int)(strm->total_out));
    {   ZWRAP_CCtx* zwc = (ZWRAP_CCtx*) strm->state;
        size_t const errorCode = ZWRAP_freeCCtx(zwc);
        if (ZSTD_isError(errorCode)) return Z_MEM_ERROR;
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
        LOG_WRAPPER("- deflateParams level=%d strategy=%d\n", level, strategy);
        return deflateParams(strm, level, strategy);
    }

    return Z_OK;
}





/* *** Decompression *** */

typedef struct {
    ZBUFF_DCtx* zbd;
    char headerBuf[ZWRAP_HEADERSIZE];
    int errorCount;
    
    /* zlib params */
    int stream_size;
    char *version;
    int windowBits;
} ZWRAP_DCtx;


ZWRAP_DCtx* ZWRAP_createDCtx(void)
{
    ZWRAP_DCtx* zwd = (ZWRAP_DCtx*)malloc(sizeof(ZWRAP_DCtx));
    if (zwd==NULL) return NULL;
    memset(zwd, 0, sizeof(*zwd));
    return zwd;
}


size_t ZWRAP_freeDCtx(ZWRAP_DCtx* zwd)
{
    if (zwd==NULL) return 0;   /* support free on null */
    if (zwd->version) free(zwd->version);
    if (zwd->zbd) ZBUFF_freeDCtx(zwd->zbd);
    free(zwd);
    return 0;
}


ZEXTERN int ZEXPORT z_inflateInit_ OF((z_streamp strm,
                                     const char *version, int stream_size))
{
    ZWRAP_DCtx* zwd = ZWRAP_createDCtx();
    LOG_WRAPPER("- inflateInit\n");
    if (zwd == NULL) return Z_MEM_ERROR;

    zwd->stream_size = stream_size;
    zwd->version = strdup(version);

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
        zwd->windowBits = windowBits;
    }
    return ret;
}




ZEXTERN int ZEXPORT z_inflateSetDictionary OF((z_streamp strm,
                                             const Bytef *dictionary,
                                             uInt  dictLength))
{
    if (!strm->reserved)
        return inflateSetDictionary(strm, dictionary, dictLength);

    LOG_WRAPPER("- inflateSetDictionary\n");
    {   ZWRAP_DCtx* zwd = (ZWRAP_DCtx*) strm->state;
        size_t errorCode = ZBUFF_decompressInitDictionary(zwd->zbd, dictionary, dictLength);
        if (ZSTD_isError(errorCode)) return Z_MEM_ERROR; 
        
        if (strm->total_in == ZSTD_frameHeaderSize_min) {
            size_t dstCapacity = 0;
            size_t srcSize = strm->total_in;
            errorCode = ZBUFF_decompressContinue(zwd->zbd, strm->next_out, &dstCapacity, zwd->headerBuf, &srcSize);
            LOG_WRAPPER("ZBUFF_decompressContinue3 errorCode=%d srcSize=%d dstCapacity=%d\n", (int)errorCode, (int)srcSize, (int)dstCapacity);
            if (dstCapacity > 0 || ZSTD_isError(errorCode)) {
                LOG_WRAPPER("ERROR: ZBUFF_decompressContinue %s\n", ZSTD_getErrorName(errorCode));
                return Z_MEM_ERROR;
            }
        }
    }

    return Z_OK;
}


ZEXTERN int ZEXPORT z_inflate OF((z_streamp strm, int flush)) 
{
    if (!strm->reserved)
        return inflate(strm, flush);

    if (strm->avail_in > 0) {
        size_t errorCode, dstCapacity, srcSize;
        ZWRAP_DCtx* zwd = (ZWRAP_DCtx*) strm->state;
        LOG_WRAPPER("inflate avail_in=%d avail_out=%d total_in=%d total_out=%d\n", (int)strm->avail_in, (int)strm->avail_out, (int)strm->total_in, (int)strm->total_out);
        if (strm->total_in < ZWRAP_HEADERSIZE)
        {
            srcSize = MIN(strm->avail_in, ZWRAP_HEADERSIZE - strm->total_in);
            memcpy(zwd->headerBuf+strm->total_in, strm->next_in, srcSize);
            strm->total_in += srcSize;
            strm->next_in += srcSize;
            strm->avail_in -= srcSize;
            if (strm->total_in < ZWRAP_HEADERSIZE) return Z_OK;

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
                LOG_WRAPPER("ZLIB inflateInit errorCode=%d\n", (int)errorCode);
                if (errorCode != Z_OK) return errorCode;

                /* inflate header */
                strm->next_in = (unsigned char*)zwd->headerBuf;
                strm->avail_in = ZWRAP_HEADERSIZE;
                strm->avail_out = 0;
                errorCode = inflate(strm, Z_NO_FLUSH);
                LOG_WRAPPER("ZLIB inflate errorCode=%d strm->avail_in=%d\n", (int)errorCode, (int)strm->avail_in);
                if (errorCode != Z_OK) return errorCode;
                if (strm->avail_in > 0) return Z_MEM_ERROR;
                
                strm->next_in = strm2.next_in;
                strm->avail_in = strm2.avail_in;
                strm->next_out = strm2.next_out;
                strm->avail_out = strm2.avail_out;

                strm->reserved = 0; /* mark as zlib stream */
                { size_t const errorCode = ZWRAP_freeDCtx(zwd);
                  if (ZSTD_isError(errorCode)) return Z_MEM_ERROR; }

                if (flush == Z_INFLATE_SYNC) return inflateSync(strm);
                return inflate(strm, flush);
            }

            zwd->zbd = ZBUFF_createDCtx();
            { size_t const errorCode = ZBUFF_decompressInit(zwd->zbd);
              if (ZSTD_isError(errorCode)) return Z_MEM_ERROR; }

            srcSize = ZWRAP_HEADERSIZE;
            dstCapacity = 0;
            errorCode = ZBUFF_decompressContinue(zwd->zbd, strm->next_out, &dstCapacity, zwd->headerBuf, &srcSize);
            LOG_WRAPPER("ZBUFF_decompressContinue1 errorCode=%d srcSize=%d dstCapacity=%d\n", (int)errorCode, (int)srcSize, (int)dstCapacity);
            if (ZSTD_isError(errorCode)) {
                LOG_WRAPPER("ERROR: ZBUFF_decompressContinue %s\n", ZSTD_getErrorName(errorCode));
                return Z_MEM_ERROR;
            }
            if (strm->avail_in == 0) return Z_OK;
        }

        srcSize = strm->avail_in;
        dstCapacity = strm->avail_out;
        errorCode = ZBUFF_decompressContinue(zwd->zbd, strm->next_out, &dstCapacity, strm->next_in, &srcSize);
        LOG_WRAPPER("ZBUFF_decompressContinue2 errorCode=%d srcSize=%d dstCapacity=%d\n", (int)errorCode, (int)srcSize, (int)dstCapacity);
        if (ZSTD_isError(errorCode)) {
            LOG_WRAPPER("ERROR: ZBUFF_decompressContinue %s\n", ZSTD_getErrorName(errorCode));
            zwd->errorCount++;
            return (zwd->errorCount<=1) ? Z_NEED_DICT : Z_MEM_ERROR;
        }
        strm->next_out += dstCapacity;
        strm->total_out += dstCapacity;
        strm->avail_out -= dstCapacity;
        strm->total_in += srcSize;
        strm->next_in += srcSize;
        strm->avail_in -= srcSize;
        if (errorCode == 0) return Z_STREAM_END;
    }
    return Z_OK;
}


ZEXTERN int ZEXPORT z_inflateEnd OF((z_streamp strm))
{
    int ret = Z_OK;
    if (!strm->reserved)
        return inflateEnd(strm);
 
    LOG_WRAPPER("- inflateEnd total_in=%d total_out=%d\n", (int)(strm->total_in), (int)(strm->total_out));
    {   ZWRAP_DCtx* zwd = (ZWRAP_DCtx*) strm->state;
        size_t const errorCode = ZWRAP_freeDCtx(zwd);
        if (ZSTD_isError(errorCode)) return Z_MEM_ERROR;
    }
    return ret;
}


ZEXTERN int ZEXPORT z_inflateSync OF((z_streamp strm))
{
    return z_inflate(strm, Z_INFLATE_SYNC);
}




/* Advanced compression functions */
ZEXTERN int ZEXPORT z_deflateCopy OF((z_streamp dest,
                                    z_streamp source))
{
    if (!g_useZSTD)
        return deflateCopy(dest, source);
    FINISH_WITH_ERR("deflateCopy is not supported!");
}


ZEXTERN int ZEXPORT z_deflateReset OF((z_streamp strm))
{
    if (!g_useZSTD)
        return deflateReset(strm);
    FINISH_WITH_ERR("deflateReset is not supported!");
}


ZEXTERN int ZEXPORT z_deflateTune OF((z_streamp strm,
                                    int good_length,
                                    int max_lazy,
                                    int nice_length,
                                    int max_chain))
{
    if (!g_useZSTD)
        return deflateTune(strm, good_length, max_lazy, nice_length, max_chain);
    FINISH_WITH_ERR("deflateTune is not supported!");
}


#if ZLIB_VERNUM >= 0x1260
ZEXTERN int ZEXPORT z_deflatePending OF((z_streamp strm,
                                       unsigned *pending,
                                       int *bits))
{
    if (!g_useZSTD)
        return deflatePending(strm, pending, bits);
    FINISH_WITH_ERR("deflatePending is not supported!");
}
#endif


ZEXTERN int ZEXPORT z_deflatePrime OF((z_streamp strm,
                                     int bits,
                                     int value))
{
    if (!g_useZSTD)
        return deflatePrime(strm, bits, value);
    FINISH_WITH_ERR("deflatePrime is not supported!");
}


ZEXTERN int ZEXPORT z_deflateSetHeader OF((z_streamp strm,
                                         gz_headerp head))
{
    if (!g_useZSTD)
        return deflateSetHeader(strm, head);
    FINISH_WITH_ERR("deflateSetHeader is not supported!");
}




/* Advanced compression functions */
#if ZLIB_VERNUM >= 0x1280
ZEXTERN int ZEXPORT z_inflateGetDictionary OF((z_streamp strm,
                                             Bytef *dictionary,
                                             uInt  *dictLength))
{
    if (!strm->reserved)
        return inflateGetDictionary(strm, dictionary, dictLength);
    FINISH_WITH_ERR("inflateGetDictionary is not supported!");
}
#endif


ZEXTERN int ZEXPORT z_inflateCopy OF((z_streamp dest,
                                    z_streamp source))
{
    if (!g_useZSTD)
        return inflateCopy(dest, source);
    FINISH_WITH_ERR("inflateCopy is not supported!");
}


ZEXTERN int ZEXPORT z_inflateReset OF((z_streamp strm))
{
    if (!strm->reserved)
        return inflateReset(strm);
    FINISH_WITH_ERR("inflateReset is not supported!");
}


#if ZLIB_VERNUM >= 0x1240
ZEXTERN int ZEXPORT z_inflateReset2 OF((z_streamp strm,
                                      int windowBits))
{
    if (!strm->reserved)
        return inflateReset2(strm, windowBits);
    FINISH_WITH_ERR("inflateReset2 is not supported!");
}
#endif


#if ZLIB_VERNUM >= 0x1240
ZEXTERN long ZEXPORT z_inflateMark OF((z_streamp strm))
{
    if (!strm->reserved)
        return inflateMark(strm);
    FINISH_WITH_ERR("inflateMark is not supported!");
}
#endif


ZEXTERN int ZEXPORT z_inflatePrime OF((z_streamp strm,
                                     int bits,
                                     int value))
{
    if (!strm->reserved)
        return inflatePrime(strm, bits, value);
    FINISH_WITH_ERR("inflatePrime is not supported!");
}


ZEXTERN int ZEXPORT z_inflateGetHeader OF((z_streamp strm,
                                         gz_headerp head))
{
    if (!strm->reserved)
        return inflateGetHeader(strm, head);
    FINISH_WITH_ERR("inflateGetHeader is not supported!");
}


ZEXTERN int ZEXPORT z_inflateBackInit_ OF((z_streamp strm, int windowBits,
                                         unsigned char FAR *window,
                                         const char *version,
                                         int stream_size))
{
    if (!strm->reserved)
        return inflateBackInit_(strm, windowBits, window, version, stream_size);
    FINISH_WITH_ERR("inflateBackInit is not supported!");
}


ZEXTERN int ZEXPORT z_inflateBack OF((z_streamp strm,
                                    in_func in, void FAR *in_desc,
                                    out_func out, void FAR *out_desc))
{
    if (!strm->reserved)
        return inflateBack(strm, in, in_desc, out, out_desc);
    FINISH_WITH_ERR("inflateBack is not supported!");
}


ZEXTERN int ZEXPORT z_inflateBackEnd OF((z_streamp strm))
{
    if (!strm->reserved)
        return inflateBackEnd(strm);
    FINISH_WITH_ERR("inflateBackEnd is not supported!");
}


ZEXTERN uLong ZEXPORT z_zlibCompileFlags OF((void)) { return zlibCompileFlags(); };



                        /* utility functions */
#ifndef Z_SOLO

ZEXTERN int ZEXPORT z_compress OF((Bytef *dest,   uLongf *destLen,
                                 const Bytef *source, uLong sourceLen))
{
    if (!g_useZSTD)
        return compress(dest, destLen, source, sourceLen);

    size_t dstCapacity = *destLen; 
    LOG_WRAPPER("z_compress sourceLen=%d dstCapacity=%d\n", (int)sourceLen, (int)dstCapacity);
    { size_t const errorCode = ZSTD_compress(dest, dstCapacity, source, sourceLen, -1);
      if (ZSTD_isError(errorCode)) return Z_MEM_ERROR;
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
        
    size_t dstCapacity = *destLen; 
    { size_t const errorCode = ZSTD_compress(dest, dstCapacity, source, sourceLen, level);
      if (ZSTD_isError(errorCode)) return Z_MEM_ERROR;
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
//    if (!g_useZSTD)
        return uncompress(dest, destLen, source, sourceLen);

    size_t dstCapacity = *destLen; 
    { size_t const errorCode = ZSTD_decompress(dest, dstCapacity, source, sourceLen);
      if (ZSTD_isError(errorCode)) return Z_MEM_ERROR;
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
    FINISH_WITH_ERR("gzbuffer is not supported!");
}


ZEXTERN z_off_t ZEXPORT z_gzoffset OF((gzFile file))
{
    if (!g_useZSTD)
        return gzoffset(file);
    FINISH_WITH_ERR("gzoffset is not supported!");
}


ZEXTERN int ZEXPORT z_gzclose_r OF((gzFile file))
{
    if (!g_useZSTD)
        return gzclose_r(file);
    FINISH_WITH_ERR("gzclose_r is not supported!");
}


ZEXTERN int ZEXPORT z_gzclose_w OF((gzFile file))
{
    if (!g_useZSTD)
        return gzclose_w(file);
    FINISH_WITH_ERR("gzclose_w is not supported!");
}
#endif


ZEXTERN int ZEXPORT z_gzsetparams OF((gzFile file, int level, int strategy))
{
    if (!g_useZSTD)
        return gzsetparams(file, level, strategy);
    FINISH_WITH_ERR("gzsetparams is not supported!");
}


ZEXTERN int ZEXPORT z_gzread OF((gzFile file, voidp buf, unsigned len))
{
    if (!g_useZSTD)
        return gzread(file, buf, len);
    FINISH_WITH_ERR("gzread is not supported!");
}


ZEXTERN int ZEXPORT z_gzwrite OF((gzFile file,
                                voidpc buf, unsigned len))
{
    if (!g_useZSTD)
        return gzwrite(file, buf, len);
    FINISH_WITH_ERR("gzwrite is not supported!");
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
      //  printf("gzprintf ret=%d\n", ret);
        return ret;
    }
    FINISH_WITH_ERR("gzprintf is not supported!");
}


ZEXTERN int ZEXPORT z_gzputs OF((gzFile file, const char *s))
{
    if (!g_useZSTD)
        return gzputs(file, s);
    FINISH_WITH_ERR("gzputs is not supported!");
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
    FINISH_WITH_ERR("gzputc is not supported!");
}


#if ZLIB_VERNUM == 0x1260
ZEXTERN int ZEXPORT z_gzgetc_ OF((gzFile file))
#else
ZEXTERN int ZEXPORT z_gzgetc OF((gzFile file))
#endif
{
    if (!g_useZSTD)
        return gzgetc(file);
    FINISH_WITH_ERR("gzgetc is not supported!");
}


ZEXTERN int ZEXPORT z_gzungetc OF((int c, gzFile file))
{
    if (!g_useZSTD)
        return gzungetc(c, file);
    FINISH_WITH_ERR("gzungetc is not supported!");
}


ZEXTERN int ZEXPORT z_gzflush OF((gzFile file, int flush))
{
    if (!g_useZSTD)
        return gzflush(file, flush);
    FINISH_WITH_ERR("gzflush is not supported!");
}


ZEXTERN z_off_t ZEXPORT z_gzseek OF((gzFile file, z_off_t offset, int whence))
{
    if (!g_useZSTD)
        return gzseek(file, offset, whence);
    FINISH_WITH_ERR("gzseek is not supported!");
}


ZEXTERN int ZEXPORT    z_gzrewind OF((gzFile file))
{
    if (!g_useZSTD)
        return gzrewind(file);
    FINISH_WITH_ERR("gzrewind is not supported!");
}


ZEXTERN z_off_t ZEXPORT    z_gztell OF((gzFile file))
{
    if (!g_useZSTD)
        return gztell(file);
    FINISH_WITH_ERR("gztell is not supported!");
}


ZEXTERN int ZEXPORT z_gzeof OF((gzFile file))
{
    if (!g_useZSTD)
        return gzeof(file);
    FINISH_WITH_ERR("gzeof is not supported!");
}


ZEXTERN int ZEXPORT z_gzdirect OF((gzFile file))
{
    if (!g_useZSTD)
        return gzdirect(file);
    FINISH_WITH_ERR("gzdirect is not supported!");
}


ZEXTERN int ZEXPORT    z_gzclose OF((gzFile file))
{
    if (!g_useZSTD)
        return gzclose(file);
    FINISH_WITH_ERR("gzclose is not supported!");
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
