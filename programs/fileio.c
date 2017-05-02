/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */


/* *************************************
*  Compiler Options
***************************************/
#ifdef _MSC_VER   /* Visual */
#  pragma warning(disable : 4127)  /* disable: C4127: conditional expression is constant */
#  pragma warning(disable : 4204)  /* non-constant aggregate initializer */
#endif
#if defined(__MINGW32__) && !defined(_POSIX_SOURCE)
#  define _POSIX_SOURCE 1          /* disable %llu warnings with MinGW on Windows */
#endif


/*-*************************************
*  Includes
***************************************/
#include "platform.h"   /* Large Files support, SET_BINARY_MODE */
#include "util.h"       /* UTIL_getFileSize */
#include <stdio.h>      /* fprintf, fopen, fread, _fileno, stdin, stdout */
#include <stdlib.h>     /* malloc, free */
#include <string.h>     /* strcmp, strlen */
#include <time.h>       /* clock */
#include <errno.h>      /* errno */

#if defined (_MSC_VER)
#  include <sys/stat.h>
#  include <io.h>
#endif

#include "mem.h"
#include "fileio.h"
#define ZSTD_STATIC_LINKING_ONLY   /* ZSTD_magicNumber, ZSTD_frameHeaderSize_max */
#include "zstd.h"
#ifdef ZSTD_MULTITHREAD
#  include "zstdmt_compress.h"
#endif
#if defined(ZSTD_GZCOMPRESS) || defined(ZSTD_GZDECOMPRESS)
#  include <zlib.h>
#  if !defined(z_const)
#    define z_const
#  endif
#endif
#if defined(ZSTD_LZMACOMPRESS) || defined(ZSTD_LZMADECOMPRESS)
#  include <lzma.h>
#endif

#define LZ4_MAGICNUMBER 0x184D2204
#if defined(ZSTD_LZ4COMPRESS) || defined(ZSTD_LZ4DECOMPRESS)
#  include <lz4frame.h>
#  include <lz4.h>
#endif


/*-*************************************
*  Constants
***************************************/
#define KB *(1<<10)
#define MB *(1<<20)
#define GB *(1U<<30)

#define _1BIT  0x01
#define _2BITS 0x03
#define _3BITS 0x07
#define _4BITS 0x0F
#define _6BITS 0x3F
#define _8BITS 0xFF

#define BLOCKSIZE      (128 KB)
#define ROLLBUFFERSIZE (BLOCKSIZE*8*64)

#define FIO_FRAMEHEADERSIZE  5        /* as a define, because needed to allocated table on stack */
#define FSE_CHECKSUM_SEED    0

#define CACHELINE 64

#define DICTSIZE_MAX (32 MB)   /* protection against large input (attack scenario) */

#define FNSPACE 30


/*-*************************************
*  Macros
***************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) { if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); } }
static int g_displayLevel = 2;   /* 0 : no display;   1: errors;   2 : + result + interaction + warnings;   3 : + progression;   4 : + information */
void FIO_setNotificationLevel(unsigned level) { g_displayLevel=level; }

#define DISPLAYUPDATE(l, ...) { if (g_displayLevel>=l) { \
            if ((clock() - g_time > refreshRate) || (g_displayLevel>=4)) \
            { g_time = clock(); DISPLAY(__VA_ARGS__); \
            if (g_displayLevel>=4) fflush(stderr); } } }
static const clock_t refreshRate = CLOCKS_PER_SEC * 15 / 100;
static clock_t g_time = 0;

#undef MIN
#define MIN(a,b)    ((a) < (b) ? (a) : (b))


/* ************************************************************
* Avoid fseek()'s 2GiB barrier with MSVC, MacOS, *BSD, MinGW
***************************************************************/
#if defined(_MSC_VER) && _MSC_VER >= 1400
#   define LONG_SEEK _fseeki64
#elif !defined(__64BIT__) && (PLATFORM_POSIX_VERSION >= 200112L) /* No point defining Large file for 64 bit */
#  define LONG_SEEK fseeko
#elif defined(__MINGW32__) && !defined(__STRICT_ANSI__) && !defined(__NO_MINGW_LFS) && defined(__MSVCRT__)
#   define LONG_SEEK fseeko64
#elif defined(_WIN32) && !defined(__DJGPP__)
#   include <windows.h>
    static int LONG_SEEK(FILE* file, __int64 offset, int origin) {
        LARGE_INTEGER off;
        DWORD method;
        off.QuadPart = offset;
        if (origin == SEEK_END)
            method = FILE_END;
        else if (origin == SEEK_CUR)
            method = FILE_CURRENT;
        else
            method = FILE_BEGIN;

        if (SetFilePointerEx((HANDLE) _get_osfhandle(_fileno(file)), off, NULL, method))
            return 0;
        else
            return -1;
    }
#else
#   define LONG_SEEK fseek
#endif


/*-*************************************
*  Local Parameters - Not thread safe
***************************************/
static FIO_compressionType_t g_compressionType = FIO_zstdCompression;
void FIO_setCompressionType(FIO_compressionType_t compressionType) { g_compressionType = compressionType; }
static U32 g_overwrite = 0;
void FIO_overwriteMode(void) { g_overwrite=1; }
static U32 g_sparseFileSupport = 1;   /* 0 : no sparse allowed; 1: auto (file yes, stdout no); 2: force sparse */
void FIO_setSparseWrite(unsigned sparse) { g_sparseFileSupport=sparse; }
static U32 g_dictIDFlag = 1;
void FIO_setDictIDFlag(unsigned dictIDFlag) { g_dictIDFlag = dictIDFlag; }
static U32 g_checksumFlag = 1;
void FIO_setChecksumFlag(unsigned checksumFlag) { g_checksumFlag = checksumFlag; }
static U32 g_removeSrcFile = 0;
void FIO_setRemoveSrcFile(unsigned flag) { g_removeSrcFile = (flag>0); }
static U32 g_memLimit = 0;
void FIO_setMemLimit(unsigned memLimit) { g_memLimit = memLimit; }
static U32 g_nbThreads = 1;
void FIO_setNbThreads(unsigned nbThreads) {
#ifndef ZSTD_MULTITHREAD
    if (nbThreads > 1) DISPLAYLEVEL(2, "Note : multi-threading is disabled \n");
#endif
    g_nbThreads = nbThreads;
}
static U32 g_blockSize = 0;
void FIO_setBlockSize(unsigned blockSize) {
    if (blockSize && g_nbThreads==1)
        DISPLAYLEVEL(2, "Setting block size is useless in single-thread mode \n");
#ifdef ZSTD_MULTITHREAD
    if (blockSize-1 < ZSTDMT_SECTION_SIZE_MIN-1)   /* intentional underflow */
        DISPLAYLEVEL(2, "Note : minimum block size is %u KB \n", (ZSTDMT_SECTION_SIZE_MIN>>10));
#endif
    g_blockSize = blockSize;
}
#define FIO_OVERLAP_LOG_NOTSET 9999
static U32 g_overlapLog = FIO_OVERLAP_LOG_NOTSET;
void FIO_setOverlapLog(unsigned overlapLog){
    if (overlapLog && g_nbThreads==1)
        DISPLAYLEVEL(2, "Setting overlapLog is useless in single-thread mode \n");
    g_overlapLog = overlapLog;
}


/*-*************************************
*  Exceptions
***************************************/
#ifndef DEBUG
#  define DEBUG 0
#endif
#define DEBUGOUTPUT(...) if (DEBUG) DISPLAY(__VA_ARGS__);
#define EXM_THROW(error, ...)                                             \
{                                                                         \
    DEBUGOUTPUT("Error defined at %s, line %i : \n", __FILE__, __LINE__); \
    DISPLAYLEVEL(1, "Error %i : ", error);                                \
    DISPLAYLEVEL(1, __VA_ARGS__);                                         \
    DISPLAYLEVEL(1, " \n");                                               \
    exit(error);                                                          \
}


/*-*************************************
*  Functions
***************************************/
/** FIO_remove() :
 * @result : Unlink `fileName`, even if it's read-only */
static int FIO_remove(const char* path)
{
#if defined(_WIN32) || defined(WIN32)
    /* windows doesn't allow remove read-only files, so try to make it
     * writable first */
    chmod(path, _S_IWRITE);
#endif
    return remove(path);
}

/** FIO_openSrcFile() :
 * condition : `dstFileName` must be non-NULL.
 * @result : FILE* to `dstFileName`, or NULL if it fails */
static FILE* FIO_openSrcFile(const char* srcFileName)
{
    FILE* f;

    if (!strcmp (srcFileName, stdinmark)) {
        DISPLAYLEVEL(4,"Using stdin for input\n");
        f = stdin;
        SET_BINARY_MODE(stdin);
    } else {
        if (!UTIL_isRegFile(srcFileName)) {
            DISPLAYLEVEL(1, "zstd: %s is not a regular file -- ignored \n", srcFileName);
            return NULL;
        }
        f = fopen(srcFileName, "rb");
        if ( f==NULL ) DISPLAYLEVEL(1, "zstd: %s: %s \n", srcFileName, strerror(errno));
    }

    return f;
}

/** FIO_openDstFile() :
 * condition : `dstFileName` must be non-NULL.
 * @result : FILE* to `dstFileName`, or NULL if it fails */
static FILE* FIO_openDstFile(const char* dstFileName)
{
    FILE* f;

    if (!strcmp (dstFileName, stdoutmark)) {
        DISPLAYLEVEL(4,"Using stdout for output\n");
        f = stdout;
        SET_BINARY_MODE(stdout);
        if (g_sparseFileSupport==1) {
            g_sparseFileSupport = 0;
            DISPLAYLEVEL(4, "Sparse File Support is automatically disabled on stdout ; try --sparse \n");
        }
    } else {
        if (g_sparseFileSupport == 1) {
            g_sparseFileSupport = ZSTD_SPARSE_DEFAULT;
        }
        if (strcmp (dstFileName, nulmark)) {  /* Check if destination file already exists */
            f = fopen( dstFileName, "rb" );
            if (f != 0) {  /* dest file exists, prompt for overwrite authorization */
                fclose(f);
                if (!g_overwrite) {
                    if (g_displayLevel <= 1) {
                        /* No interaction possible */
                        DISPLAY("zstd: %s already exists; not overwritten  \n", dstFileName);
                        return NULL;
                    }
                    DISPLAY("zstd: %s already exists; do you wish to overwrite (y/N) ? ", dstFileName);
                    {   int ch = getchar();
                        if ((ch!='Y') && (ch!='y')) {
                            DISPLAY("    not overwritten  \n");
                            return NULL;
                        }
                        while ((ch!=EOF) && (ch!='\n')) ch = getchar();  /* flush rest of input line */
                    }
                }

                /* need to unlink */
                FIO_remove(dstFileName);
        }   }
        f = fopen( dstFileName, "wb" );
        if (f==NULL) DISPLAYLEVEL(1, "zstd: %s: %s\n", dstFileName, strerror(errno));
    }

    return f;
}


/*! FIO_createDictBuffer() :
 *  creates a buffer, pointed by `*bufferPtr`,
 *  loads `filename` content into it, up to DICTSIZE_MAX bytes.
 *  @return : loaded size
 *  if fileName==NULL, returns 0 and a NULL pointer
 */
static size_t FIO_createDictBuffer(void** bufferPtr, const char* fileName)
{
    FILE* fileHandle;
    U64 fileSize;

    *bufferPtr = NULL;
    if (fileName == NULL) return 0;

    DISPLAYLEVEL(4,"Loading %s as dictionary \n", fileName);
    fileHandle = fopen(fileName, "rb");
    if (fileHandle==0) EXM_THROW(31, "zstd: %s: %s", fileName, strerror(errno));
    fileSize = UTIL_getFileSize(fileName);
    if (fileSize > DICTSIZE_MAX) EXM_THROW(32, "Dictionary file %s is too large (> %u MB)", fileName, DICTSIZE_MAX >> 20);   /* avoid extreme cases */
    *bufferPtr = malloc((size_t)fileSize);
    if (*bufferPtr==NULL) EXM_THROW(34, "zstd: %s", strerror(errno));
    { size_t const readSize = fread(*bufferPtr, 1, (size_t)fileSize, fileHandle);
      if (readSize!=fileSize) EXM_THROW(35, "Error reading dictionary file %s", fileName); }
    fclose(fileHandle);
    return (size_t)fileSize;
}

#ifndef ZSTD_NOCOMPRESS

/*-**********************************************************************
*  Compression
************************************************************************/
typedef struct {
    FILE* srcFile;
    FILE* dstFile;
    void*  srcBuffer;
    size_t srcBufferSize;
    void*  dstBuffer;
    size_t dstBufferSize;
#ifdef ZSTD_MULTITHREAD
    ZSTDMT_CCtx* cctx;
#else
    ZSTD_CStream* cctx;
#endif
} cRess_t;

static cRess_t FIO_createCResources(const char* dictFileName, int cLevel,
                                    U64 srcSize, int srcRegFile,
                                    ZSTD_compressionParameters* comprParams) {
    cRess_t ress;
    memset(&ress, 0, sizeof(ress));

#ifdef ZSTD_MULTITHREAD
    ress.cctx = ZSTDMT_createCCtx(g_nbThreads);
    if (ress.cctx == NULL) EXM_THROW(30, "zstd: allocation error : can't create ZSTD_CStream");
    if ((cLevel==ZSTD_maxCLevel()) && (g_overlapLog==FIO_OVERLAP_LOG_NOTSET))
        ZSTDMT_setMTCtxParameter(ress.cctx, ZSTDMT_p_overlapSectionLog, 9);   /* use complete window for overlap */
    if (g_overlapLog != FIO_OVERLAP_LOG_NOTSET)
        ZSTDMT_setMTCtxParameter(ress.cctx, ZSTDMT_p_overlapSectionLog, g_overlapLog);
#else
    ress.cctx = ZSTD_createCStream();
    if (ress.cctx == NULL) EXM_THROW(30, "zstd: allocation error : can't create ZSTD_CStream");
#endif
    ress.srcBufferSize = ZSTD_CStreamInSize();
    ress.srcBuffer = malloc(ress.srcBufferSize);
    ress.dstBufferSize = ZSTD_CStreamOutSize();
    ress.dstBuffer = malloc(ress.dstBufferSize);
    if (!ress.srcBuffer || !ress.dstBuffer) EXM_THROW(31, "zstd: allocation error : not enough memory");

    /* dictionary */
    {   void* dictBuffer;
        size_t const dictBuffSize = FIO_createDictBuffer(&dictBuffer, dictFileName);   /* works with dictFileName==NULL */
        if (dictFileName && (dictBuffer==NULL)) EXM_THROW(32, "zstd: allocation error : can't create dictBuffer");
        {   ZSTD_parameters params = ZSTD_getParams(cLevel, srcSize, dictBuffSize);
            params.fParams.contentSizeFlag = srcRegFile;
            params.fParams.checksumFlag = g_checksumFlag;
            params.fParams.noDictIDFlag = !g_dictIDFlag;
            if (comprParams->windowLog) params.cParams.windowLog = comprParams->windowLog;
            if (comprParams->chainLog) params.cParams.chainLog = comprParams->chainLog;
            if (comprParams->hashLog) params.cParams.hashLog = comprParams->hashLog;
            if (comprParams->searchLog) params.cParams.searchLog = comprParams->searchLog;
            if (comprParams->searchLength) params.cParams.searchLength = comprParams->searchLength;
            if (comprParams->targetLength) params.cParams.targetLength = comprParams->targetLength;
            if (comprParams->strategy) params.cParams.strategy = (ZSTD_strategy)(comprParams->strategy - 1);   /* 0 means : do not change */
#ifdef ZSTD_MULTITHREAD
            {   size_t const errorCode = ZSTDMT_initCStream_advanced(ress.cctx, dictBuffer, dictBuffSize, params, srcSize);
                if (ZSTD_isError(errorCode)) EXM_THROW(33, "Error initializing CStream : %s", ZSTD_getErrorName(errorCode));
                ZSTDMT_setMTCtxParameter(ress.cctx, ZSTDMT_p_sectionSize, g_blockSize);
#else
            {   size_t const errorCode = ZSTD_initCStream_advanced(ress.cctx, dictBuffer, dictBuffSize, params, srcSize);
                if (ZSTD_isError(errorCode)) EXM_THROW(33, "Error initializing CStream : %s", ZSTD_getErrorName(errorCode));
#endif
        }   }
        free(dictBuffer);
    }

    return ress;
}

static void FIO_freeCResources(cRess_t ress)
{
    free(ress.srcBuffer);
    free(ress.dstBuffer);
#ifdef ZSTD_MULTITHREAD
    ZSTDMT_freeCCtx(ress.cctx);
#else
    ZSTD_freeCStream(ress.cctx);   /* never fails */
#endif
}


#ifdef ZSTD_GZCOMPRESS
static unsigned long long FIO_compressGzFrame(cRess_t* ress, const char* srcFileName, U64 const srcFileSize, int compressionLevel, U64* readsize)
{
    unsigned long long inFileSize = 0, outFileSize = 0;
    z_stream strm;
    int ret;

    if (compressionLevel > Z_BEST_COMPRESSION) compressionLevel = Z_BEST_COMPRESSION;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    ret = deflateInit2(&strm, compressionLevel, Z_DEFLATED, 15 /* maxWindowLogSize */ + 16 /* gzip only */, 8, Z_DEFAULT_STRATEGY); /* see http://www.zlib.net/manual.html */
    if (ret != Z_OK) EXM_THROW(71, "zstd: %s: deflateInit2 error %d \n", srcFileName, ret);

    strm.next_in = 0;
    strm.avail_in = 0;
    strm.next_out = (Bytef*)ress->dstBuffer;
    strm.avail_out = (uInt)ress->dstBufferSize;

    while (1) {
        if (strm.avail_in == 0) {
            size_t const inSize = fread(ress->srcBuffer, 1, ress->srcBufferSize, ress->srcFile);
            if (inSize == 0) break;
            inFileSize += inSize;
            strm.next_in = (z_const unsigned char*)ress->srcBuffer;
            strm.avail_in = (uInt)inSize;
        }
        ret = deflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK) EXM_THROW(72, "zstd: %s: deflate error %d \n", srcFileName, ret);
        {   size_t const decompBytes = ress->dstBufferSize - strm.avail_out;
            if (decompBytes) {
                if (fwrite(ress->dstBuffer, 1, decompBytes, ress->dstFile) != decompBytes) EXM_THROW(73, "Write error : cannot write to output file");
                outFileSize += decompBytes;
                strm.next_out = (Bytef*)ress->dstBuffer;
                strm.avail_out = (uInt)ress->dstBufferSize;
            }
        }
        if (!srcFileSize) DISPLAYUPDATE(2, "\rRead : %u MB ==> %.2f%%", (U32)(inFileSize>>20), (double)outFileSize/inFileSize*100)
        else DISPLAYUPDATE(2, "\rRead : %u / %u MB ==> %.2f%%", (U32)(inFileSize>>20), (U32)(srcFileSize>>20), (double)outFileSize/inFileSize*100);
    }

    while (1) {
        ret = deflate(&strm, Z_FINISH);
        {   size_t const decompBytes = ress->dstBufferSize - strm.avail_out;
            if (decompBytes) {
                if (fwrite(ress->dstBuffer, 1, decompBytes, ress->dstFile) != decompBytes) EXM_THROW(75, "Write error : cannot write to output file");
                outFileSize += decompBytes;
                strm.next_out = (Bytef*)ress->dstBuffer;
                strm.avail_out = (uInt)ress->dstBufferSize;
            }
        }
        if (ret == Z_STREAM_END) break;
        if (ret != Z_BUF_ERROR) EXM_THROW(77, "zstd: %s: deflate error %d \n", srcFileName, ret);
    }

    ret = deflateEnd(&strm);
    if (ret != Z_OK) EXM_THROW(79, "zstd: %s: deflateEnd error %d \n", srcFileName, ret);
    *readsize = inFileSize;

    return outFileSize;
}
#endif


#ifdef ZSTD_LZMACOMPRESS
static unsigned long long FIO_compressLzmaFrame(cRess_t* ress, const char* srcFileName, U64 const srcFileSize, int compressionLevel, U64* readsize, int plain_lzma)
{
    unsigned long long inFileSize = 0, outFileSize = 0;
    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_action action = LZMA_RUN;
    lzma_ret ret;

    if (compressionLevel < 0) compressionLevel = 0;
    if (compressionLevel > 9) compressionLevel = 9;

    if (plain_lzma) {
        lzma_options_lzma opt_lzma;
        if (lzma_lzma_preset(&opt_lzma, compressionLevel)) EXM_THROW(71, "zstd: %s: lzma_lzma_preset error", srcFileName);
        ret = lzma_alone_encoder(&strm, &opt_lzma); /* LZMA */
        if (ret != LZMA_OK) EXM_THROW(71, "zstd: %s: lzma_alone_encoder error %d", srcFileName, ret);
    } else {
        ret = lzma_easy_encoder(&strm, compressionLevel, LZMA_CHECK_CRC64); /* XZ */
        if (ret != LZMA_OK) EXM_THROW(71, "zstd: %s: lzma_easy_encoder error %d", srcFileName, ret);
    }

    strm.next_in = 0;
    strm.avail_in = 0;
    strm.next_out = ress->dstBuffer;
    strm.avail_out = ress->dstBufferSize;

    while (1) {
        if (strm.avail_in == 0) {
            size_t const inSize = fread(ress->srcBuffer, 1, ress->srcBufferSize, ress->srcFile);
            if (inSize == 0) action = LZMA_FINISH;
            inFileSize += inSize;
            strm.next_in = ress->srcBuffer;
            strm.avail_in = inSize;
        }

        ret = lzma_code(&strm, action);

        if (ret != LZMA_OK && ret != LZMA_STREAM_END) EXM_THROW(72, "zstd: %s: lzma_code encoding error %d", srcFileName, ret);
        {   size_t const compBytes = ress->dstBufferSize - strm.avail_out;
            if (compBytes) {
                if (fwrite(ress->dstBuffer, 1, compBytes, ress->dstFile) != compBytes) EXM_THROW(73, "Write error : cannot write to output file");
                outFileSize += compBytes;
                strm.next_out = ress->dstBuffer;
                strm.avail_out = ress->dstBufferSize;
            }
        }
        if (!srcFileSize) DISPLAYUPDATE(2, "\rRead : %u MB ==> %.2f%%", (U32)(inFileSize>>20), (double)outFileSize/inFileSize*100)
        else DISPLAYUPDATE(2, "\rRead : %u / %u MB ==> %.2f%%", (U32)(inFileSize>>20), (U32)(srcFileSize>>20), (double)outFileSize/inFileSize*100);
        if (ret == LZMA_STREAM_END) break;
    }

    lzma_end(&strm);
    *readsize = inFileSize;

    return outFileSize;
}
#endif

#ifdef ZSTD_LZ4COMPRESS
static int FIO_LZ4_GetBlockSize_FromBlockId (int id) { return (1 << (8 + (2 * id))); }
static unsigned long long FIO_compressLz4Frame(cRess_t* ress, const char* srcFileName, U64 const srcFileSize, int compressionLevel, U64* readsize)
{
    unsigned long long inFileSize = 0, outFileSize = 0;

    LZ4F_preferences_t prefs;
    LZ4F_compressionContext_t ctx;

    LZ4F_errorCode_t const errorCode = LZ4F_createCompressionContext(&ctx, LZ4F_VERSION);
    if (LZ4F_isError(errorCode)) EXM_THROW(31, "zstd: failed to create lz4 compression context");

    memset(&prefs, 0, sizeof(prefs));

#if LZ4_VERSION_NUMBER <= 10600
#define LZ4F_blockIndependent blockIndependent
#define LZ4F_max4MB max4MB
#endif

    prefs.autoFlush = 1;
    prefs.compressionLevel = compressionLevel;
    prefs.frameInfo.blockMode = LZ4F_blockIndependent; /* stick to defaults for lz4 cli */
    prefs.frameInfo.blockSizeID = LZ4F_max4MB;
    prefs.frameInfo.contentChecksumFlag = (contentChecksum_t)g_checksumFlag;
#if LZ4_VERSION_NUMBER >= 10600
    prefs.frameInfo.contentSize = srcFileSize;
#endif

    {
        size_t blockSize = FIO_LZ4_GetBlockSize_FromBlockId(LZ4F_max4MB);
        size_t readSize;
        size_t headerSize = LZ4F_compressBegin(ctx, ress->dstBuffer, ress->dstBufferSize, &prefs);
        if (LZ4F_isError(headerSize)) EXM_THROW(33, "File header generation failed : %s", LZ4F_getErrorName(headerSize));
        { size_t const sizeCheck = fwrite(ress->dstBuffer, 1, headerSize, ress->dstFile);
          if (sizeCheck!=headerSize) EXM_THROW(34, "Write error : cannot write header"); }
        outFileSize += headerSize;

        /* Read first block */
        readSize  = fread(ress->srcBuffer, (size_t)1, (size_t)blockSize, ress->srcFile);
        inFileSize += readSize;

        /* Main Loop */
        while (readSize>0) {
            size_t outSize;

            /* Compress Block */
            outSize = LZ4F_compressUpdate(ctx, ress->dstBuffer, ress->dstBufferSize, ress->srcBuffer, readSize, NULL);
            if (LZ4F_isError(outSize)) EXM_THROW(35, "zstd: %s: lz4 compression failed : %s", srcFileName, LZ4F_getErrorName(outSize));
            outFileSize += outSize;
            if (!srcFileSize) DISPLAYUPDATE(2, "\rRead : %u MB ==> %.2f%%", (U32)(inFileSize>>20), (double)outFileSize/inFileSize*100)
            else DISPLAYUPDATE(2, "\rRead : %u / %u MB ==> %.2f%%", (U32)(inFileSize>>20), (U32)(srcFileSize>>20), (double)outFileSize/inFileSize*100);

            /* Write Block */
            { size_t const sizeCheck = fwrite(ress->dstBuffer, 1, outSize, ress->dstFile);
              if (sizeCheck!=outSize) EXM_THROW(36, "Write error : cannot write compressed block"); }

            /* Read next block */
            readSize  = fread(ress->srcBuffer, (size_t)1, (size_t)blockSize, ress->srcFile);
            inFileSize += readSize;
        }
        if (ferror(ress->srcFile)) EXM_THROW(37, "Error reading %s ", srcFileName);

        /* End of Stream mark */
        headerSize = LZ4F_compressEnd(ctx, ress->dstBuffer, ress->dstBufferSize, NULL);
        if (LZ4F_isError(headerSize)) EXM_THROW(38, "zstd: %s: lz4 end of file generation failed : %s", srcFileName, LZ4F_getErrorName(headerSize));

        { size_t const sizeCheck = fwrite(ress->dstBuffer, 1, headerSize, ress->dstFile);
          if (sizeCheck!=headerSize) EXM_THROW(39, "Write error : cannot write end of stream"); }
        outFileSize += headerSize;
    }

    *readsize = inFileSize;
    LZ4F_freeCompressionContext(ctx);

    return outFileSize;
}
#endif


/*! FIO_compressFilename_internal() :
 *  same as FIO_compressFilename_extRess(), with `ress.desFile` already opened.
 *  @return : 0 : compression completed correctly,
 *            1 : missing or pb opening srcFileName
 */
static int FIO_compressFilename_internal(cRess_t ress,
                                         const char* dstFileName, const char* srcFileName, int compressionLevel)
{
    FILE* const srcFile = ress.srcFile;
    FILE* const dstFile = ress.dstFile;
    U64 readsize = 0;
    U64 compressedfilesize = 0;
    U64 const fileSize = UTIL_getFileSize(srcFileName);

    switch (g_compressionType) {
        case FIO_zstdCompression:
            break;

        case FIO_gzipCompression:
#ifdef ZSTD_GZCOMPRESS
            compressedfilesize = FIO_compressGzFrame(&ress, srcFileName, fileSize, compressionLevel, &readsize);
#else
            (void)compressionLevel;
            EXM_THROW(20, "zstd: %s: file cannot be compressed as gzip (zstd compiled without ZSTD_GZCOMPRESS) -- ignored \n", srcFileName);
#endif
            goto finish;

        case FIO_xzCompression:
        case FIO_lzmaCompression:
#ifdef ZSTD_LZMACOMPRESS
            compressedfilesize = FIO_compressLzmaFrame(&ress, srcFileName, fileSize, compressionLevel, &readsize, g_compressionType==FIO_lzmaCompression);
#else
            (void)compressionLevel;
            EXM_THROW(20, "zstd: %s: file cannot be compressed as xz/lzma (zstd compiled without ZSTD_LZMACOMPRESS) -- ignored \n", srcFileName);
#endif
            goto finish;

        case FIO_lz4Compression:
#ifdef ZSTD_LZ4COMPRESS
            compressedfilesize = FIO_compressLz4Frame(&ress, srcFileName, fileSize, compressionLevel, &readsize);
#else
            (void)compressionLevel;
            EXM_THROW(20, "zstd: %s: file cannot be compressed as lz4 (zstd compiled without ZSTD_LZ4COMPRESS) -- ignored \n", srcFileName);
#endif
            goto finish;
    }

    /* init */
#ifdef ZSTD_MULTITHREAD
    {   size_t const resetError = ZSTDMT_resetCStream(ress.cctx, fileSize);
#else
    {   size_t const resetError = ZSTD_resetCStream(ress.cctx, fileSize);
#endif
        if (ZSTD_isError(resetError)) EXM_THROW(21, "Error initializing compression : %s", ZSTD_getErrorName(resetError));
    }

    /* Main compression loop */
    while (1) {
        /* Fill input Buffer */
        size_t const inSize = fread(ress.srcBuffer, (size_t)1, ress.srcBufferSize, srcFile);
        if (inSize==0) break;
        readsize += inSize;

        {   ZSTD_inBuffer  inBuff = { ress.srcBuffer, inSize, 0 };
            while (inBuff.pos != inBuff.size) {
                ZSTD_outBuffer outBuff = { ress.dstBuffer, ress.dstBufferSize, 0 };
#ifdef ZSTD_MULTITHREAD
                size_t const result = ZSTDMT_compressStream(ress.cctx, &outBuff, &inBuff);
#else
                size_t const result = ZSTD_compressStream(ress.cctx, &outBuff, &inBuff);
#endif
                if (ZSTD_isError(result)) EXM_THROW(23, "Compression error : %s ", ZSTD_getErrorName(result));

                /* Write compressed stream */
                if (outBuff.pos) {
                    size_t const sizeCheck = fwrite(ress.dstBuffer, 1, outBuff.pos, dstFile);
                    if (sizeCheck!=outBuff.pos) EXM_THROW(25, "Write error : cannot write compressed block into %s", dstFileName);
                    compressedfilesize += outBuff.pos;
        }   }   }
        if (g_nbThreads > 1) {
            if (!fileSize) DISPLAYUPDATE(2, "\rRead : %u MB", (U32)(readsize>>20))
            else DISPLAYUPDATE(2, "\rRead : %u / %u MB", (U32)(readsize>>20), (U32)(fileSize>>20));
        } else {
            if (!fileSize) DISPLAYUPDATE(2, "\rRead : %u MB ==> %.2f%%", (U32)(readsize>>20), (double)compressedfilesize/readsize*100)
            else DISPLAYUPDATE(2, "\rRead : %u / %u MB ==> %.2f%%", (U32)(readsize>>20), (U32)(fileSize>>20), (double)compressedfilesize/readsize*100);
        }
    }

    /* End of Frame */
    {   size_t result = 1;
        while (result!=0) {   /* note : is there any possibility of endless loop ? */
            ZSTD_outBuffer outBuff = { ress.dstBuffer, ress.dstBufferSize, 0 };
#ifdef ZSTD_MULTITHREAD
            result = ZSTDMT_endStream(ress.cctx, &outBuff);
#else
            result = ZSTD_endStream(ress.cctx, &outBuff);
#endif
            if (ZSTD_isError(result)) EXM_THROW(26, "Compression error during frame end : %s", ZSTD_getErrorName(result));
            { size_t const sizeCheck = fwrite(ress.dstBuffer, 1, outBuff.pos, dstFile);
              if (sizeCheck!=outBuff.pos) EXM_THROW(27, "Write error : cannot write frame end into %s", dstFileName); }
            compressedfilesize += outBuff.pos;
        }
    }

finish:
    /* Status */
    DISPLAYLEVEL(2, "\r%79s\r", "");
    DISPLAYLEVEL(2,"%-20s :%6.2f%%   (%6llu => %6llu bytes, %s) \n", srcFileName,
        (double)compressedfilesize/(readsize+(!readsize) /* avoid div by zero */ )*100,
        (unsigned long long)readsize, (unsigned long long) compressedfilesize,
         dstFileName);

    return 0;
}


/*! FIO_compressFilename_srcFile() :
 *  note : ress.destFile already opened
 *  @return : 0 : compression completed correctly,
 *            1 : missing or pb opening srcFileName
 */
static int FIO_compressFilename_srcFile(cRess_t ress,
                                        const char* dstFileName, const char* srcFileName, int compressionLevel)
{
    int result;

    /* File check */
    if (UTIL_isDirectory(srcFileName)) {
        DISPLAYLEVEL(1, "zstd: %s is a directory -- ignored \n", srcFileName);
        return 1;
    }

    ress.srcFile = FIO_openSrcFile(srcFileName);
    if (!ress.srcFile) return 1;   /* srcFile could not be opened */

    result = FIO_compressFilename_internal(ress, dstFileName, srcFileName, compressionLevel);

    fclose(ress.srcFile);
    if (g_removeSrcFile /* --rm */ && !result && strcmp(srcFileName, stdinmark)) {
        if (remove(srcFileName))
            EXM_THROW(1, "zstd: %s: %s", srcFileName, strerror(errno));
    }
    return result;
}


/*! FIO_compressFilename_dstFile() :
 *  @return : 0 : compression completed correctly,
 *            1 : pb
 */
static int FIO_compressFilename_dstFile(cRess_t ress,
                                        const char* dstFileName, const char* srcFileName, int compressionLevel)
{
    int result;
    stat_t statbuf;
    int stat_result = 0;

    ress.dstFile = FIO_openDstFile(dstFileName);
    if (ress.dstFile==NULL) return 1;  /* could not open dstFileName */

    if (strcmp (srcFileName, stdinmark) && UTIL_getFileStat(srcFileName, &statbuf)) stat_result = 1;
    result = FIO_compressFilename_srcFile(ress, dstFileName, srcFileName, compressionLevel);

    if (fclose(ress.dstFile)) { DISPLAYLEVEL(1, "zstd: %s: %s \n", dstFileName, strerror(errno)); result=1; }  /* error closing dstFile */
    if (result!=0) { if (remove(dstFileName)) EXM_THROW(1, "zstd: %s: %s", dstFileName, strerror(errno)); }  /* remove operation artefact */
    else if (strcmp (dstFileName, stdoutmark) && stat_result) UTIL_setFileStat(dstFileName, &statbuf);
    return result;
}


int FIO_compressFilename(const char* dstFileName, const char* srcFileName,
                         const char* dictFileName, int compressionLevel, ZSTD_compressionParameters* comprParams)
{
    clock_t const start = clock();
    U64 const srcSize = UTIL_getFileSize(srcFileName);
    int const regFile = UTIL_isRegFile(srcFileName);

    cRess_t const ress = FIO_createCResources(dictFileName, compressionLevel, srcSize, regFile, comprParams);
    int const result = FIO_compressFilename_dstFile(ress, dstFileName, srcFileName, compressionLevel);

    double const seconds = (double)(clock() - start) / CLOCKS_PER_SEC;
    DISPLAYLEVEL(4, "Completed in %.2f sec \n", seconds);

    FIO_freeCResources(ress);
    return result;
}


int FIO_compressMultipleFilenames(const char** inFileNamesTable, unsigned nbFiles,
                                  const char* suffix,
                                  const char* dictFileName, int compressionLevel,
                                  ZSTD_compressionParameters* comprParams)
{
    int missed_files = 0;
    size_t dfnSize = FNSPACE;
    char*  dstFileName = (char*)malloc(FNSPACE);
    size_t const suffixSize = suffix ? strlen(suffix) : 0;
    U64 const srcSize = (nbFiles != 1) ? 0 : UTIL_getFileSize(inFileNamesTable[0]) ;
    int const regFile = (nbFiles != 1) ? 0 : UTIL_isRegFile(inFileNamesTable[0]);
    cRess_t ress = FIO_createCResources(dictFileName, compressionLevel, srcSize, regFile, comprParams);

    /* init */
    if (dstFileName==NULL) EXM_THROW(27, "FIO_compressMultipleFilenames : allocation error for dstFileName");
    if (suffix == NULL) EXM_THROW(28, "FIO_compressMultipleFilenames : dst unknown");  /* should never happen */

    /* loop on each file */
    if (!strcmp(suffix, stdoutmark)) {
        unsigned u;
        ress.dstFile = stdout;
        SET_BINARY_MODE(stdout);
        for (u=0; u<nbFiles; u++)
            missed_files += FIO_compressFilename_srcFile(ress, stdoutmark, inFileNamesTable[u], compressionLevel);
        if (fclose(ress.dstFile)) EXM_THROW(29, "Write error : cannot properly close stdout");
    } else {
        unsigned u;
        for (u=0; u<nbFiles; u++) {
            size_t ifnSize = strlen(inFileNamesTable[u]);
            if (dfnSize <= ifnSize+suffixSize+1) { free(dstFileName); dfnSize = ifnSize + 20; dstFileName = (char*)malloc(dfnSize); }
            strcpy(dstFileName, inFileNamesTable[u]);
            strcat(dstFileName, suffix);
            missed_files += FIO_compressFilename_dstFile(ress, dstFileName, inFileNamesTable[u], compressionLevel);
    }   }

    /* Close & Free */
    FIO_freeCResources(ress);
    free(dstFileName);

    return missed_files;
}

#endif /* #ifndef ZSTD_NOCOMPRESS */



#ifndef ZSTD_NODECOMPRESS

/* **************************************************************************
*  Decompression
****************************************************************************/
typedef struct {
    void*  srcBuffer;
    size_t srcBufferLoaded;
    size_t srcBufferSize;
    void*  dstBuffer;
    size_t dstBufferSize;
    ZSTD_DStream* dctx;
    FILE*  dstFile;
} dRess_t;

static dRess_t FIO_createDResources(const char* dictFileName)
{
    dRess_t ress;
    memset(&ress, 0, sizeof(ress));

    /* Allocation */
    ress.dctx = ZSTD_createDStream();
    if (ress.dctx==NULL) EXM_THROW(60, "Can't create ZSTD_DStream");
    ZSTD_setDStreamParameter(ress.dctx, DStream_p_maxWindowSize, g_memLimit);
    ress.srcBufferSize = ZSTD_DStreamInSize();
    ress.srcBuffer = malloc(ress.srcBufferSize);
    ress.dstBufferSize = ZSTD_DStreamOutSize();
    ress.dstBuffer = malloc(ress.dstBufferSize);
    if (!ress.srcBuffer || !ress.dstBuffer) EXM_THROW(61, "Allocation error : not enough memory");

    /* dictionary */
    {   void* dictBuffer;
        size_t const dictBufferSize = FIO_createDictBuffer(&dictBuffer, dictFileName);
        size_t const initError = ZSTD_initDStream_usingDict(ress.dctx, dictBuffer, dictBufferSize);
        if (ZSTD_isError(initError)) EXM_THROW(61, "ZSTD_initDStream_usingDict error : %s", ZSTD_getErrorName(initError));
        free(dictBuffer);
    }

    return ress;
}

static void FIO_freeDResources(dRess_t ress)
{
    size_t const errorCode = ZSTD_freeDStream(ress.dctx);
    if (ZSTD_isError(errorCode)) EXM_THROW(69, "Error : can't free ZSTD_DStream context resource : %s", ZSTD_getErrorName(errorCode));
    free(ress.srcBuffer);
    free(ress.dstBuffer);
}


/** FIO_fwriteSparse() :
*   @return : storedSkips, to be provided to next call to FIO_fwriteSparse() of LZ4IO_fwriteSparseEnd() */
static unsigned FIO_fwriteSparse(FILE* file, const void* buffer, size_t bufferSize, unsigned storedSkips)
{
    const size_t* const bufferT = (const size_t*)buffer;   /* Buffer is supposed malloc'ed, hence aligned on size_t */
    size_t bufferSizeT = bufferSize / sizeof(size_t);
    const size_t* const bufferTEnd = bufferT + bufferSizeT;
    const size_t* ptrT = bufferT;
    static const size_t segmentSizeT = (32 KB) / sizeof(size_t);   /* 0-test re-attempted every 32 KB */

    if (!g_sparseFileSupport) {  /* normal write */
        size_t const sizeCheck = fwrite(buffer, 1, bufferSize, file);
        if (sizeCheck != bufferSize) EXM_THROW(70, "Write error : cannot write decoded block");
        return 0;
    }

    /* avoid int overflow */
    if (storedSkips > 1 GB) {
        int const seekResult = LONG_SEEK(file, 1 GB, SEEK_CUR);
        if (seekResult != 0) EXM_THROW(71, "1 GB skip error (sparse file support)");
        storedSkips -= 1 GB;
    }

    while (ptrT < bufferTEnd) {
        size_t seg0SizeT = segmentSizeT;
        size_t nb0T;

        /* count leading zeros */
        if (seg0SizeT > bufferSizeT) seg0SizeT = bufferSizeT;
        bufferSizeT -= seg0SizeT;
        for (nb0T=0; (nb0T < seg0SizeT) && (ptrT[nb0T] == 0); nb0T++) ;
        storedSkips += (unsigned)(nb0T * sizeof(size_t));

        if (nb0T != seg0SizeT) {   /* not all 0s */
            int const seekResult = LONG_SEEK(file, storedSkips, SEEK_CUR);
            if (seekResult) EXM_THROW(72, "Sparse skip error ; try --no-sparse");
            storedSkips = 0;
            seg0SizeT -= nb0T;
            ptrT += nb0T;
            {   size_t const sizeCheck = fwrite(ptrT, sizeof(size_t), seg0SizeT, file);
                if (sizeCheck != seg0SizeT) EXM_THROW(73, "Write error : cannot write decoded block");
        }   }
        ptrT += seg0SizeT;
    }

    {   static size_t const maskT = sizeof(size_t)-1;
        if (bufferSize & maskT) {   /* size not multiple of sizeof(size_t) : implies end of block */
            const char* const restStart = (const char*)bufferTEnd;
            const char* restPtr = restStart;
            size_t restSize =  bufferSize & maskT;
            const char* const restEnd = restStart + restSize;
            for ( ; (restPtr < restEnd) && (*restPtr == 0); restPtr++) ;
            storedSkips += (unsigned) (restPtr - restStart);
            if (restPtr != restEnd) {
                int seekResult = LONG_SEEK(file, storedSkips, SEEK_CUR);
                if (seekResult) EXM_THROW(74, "Sparse skip error ; try --no-sparse");
                storedSkips = 0;
                {   size_t const sizeCheck = fwrite(restPtr, 1, restEnd - restPtr, file);
                    if (sizeCheck != (size_t)(restEnd - restPtr)) EXM_THROW(75, "Write error : cannot write decoded end of block");
    }   }   }   }

    return storedSkips;
}

static void FIO_fwriteSparseEnd(FILE* file, unsigned storedSkips)
{
    if (storedSkips-->0) {   /* implies g_sparseFileSupport>0 */
        int const seekResult = LONG_SEEK(file, storedSkips, SEEK_CUR);
        if (seekResult != 0) EXM_THROW(69, "Final skip error (sparse file)");
        {   const char lastZeroByte[1] = { 0 };
            size_t const sizeCheck = fwrite(lastZeroByte, 1, 1, file);
            if (sizeCheck != 1) EXM_THROW(69, "Write error : cannot write last zero");
    }   }
}


/** FIO_passThrough() : just copy input into output, for compatibility with gzip -df mode
    @return : 0 (no error) */
static unsigned FIO_passThrough(FILE* foutput, FILE* finput, void* buffer, size_t bufferSize, size_t alreadyLoaded)
{
    size_t const blockSize = MIN(64 KB, bufferSize);
    size_t readFromInput = 1;
    unsigned storedSkips = 0;

    /* assumption : ress->srcBufferLoaded bytes already loaded and stored within buffer */
    { size_t const sizeCheck = fwrite(buffer, 1, alreadyLoaded, foutput);
      if (sizeCheck != alreadyLoaded) EXM_THROW(50, "Pass-through write error"); }

    while (readFromInput) {
        readFromInput = fread(buffer, 1, blockSize, finput);
        storedSkips = FIO_fwriteSparse(foutput, buffer, readFromInput, storedSkips);
    }

    FIO_fwriteSparseEnd(foutput, storedSkips);
    return 0;
}


/** FIO_decompressFrame() :
    @return : size of decoded frame
*/
unsigned long long FIO_decompressFrame(dRess_t* ress,
                                       FILE* finput,
                                       U64 alreadyDecoded)
{
    U64 frameSize = 0;
    U32 storedSkips = 0;

    ZSTD_resetDStream(ress->dctx);

    /* Header loading (optional, saves one loop) */
    {   size_t const toRead = 9;
        if (ress->srcBufferLoaded < toRead)
            ress->srcBufferLoaded += fread(((char*)ress->srcBuffer) + ress->srcBufferLoaded, 1, toRead - ress->srcBufferLoaded, finput);
    }

    /* Main decompression Loop */
    while (1) {
        ZSTD_inBuffer  inBuff = { ress->srcBuffer, ress->srcBufferLoaded, 0 };
        ZSTD_outBuffer outBuff= { ress->dstBuffer, ress->dstBufferSize, 0 };
        size_t const readSizeHint = ZSTD_decompressStream(ress->dctx, &outBuff, &inBuff);
        if (ZSTD_isError(readSizeHint)) EXM_THROW(36, "Decoding error : %s", ZSTD_getErrorName(readSizeHint));

        /* Write block */
        storedSkips = FIO_fwriteSparse(ress->dstFile, ress->dstBuffer, outBuff.pos, storedSkips);
        frameSize += outBuff.pos;
        DISPLAYUPDATE(2, "\rDecoded : %u MB...     ", (U32)((alreadyDecoded+frameSize)>>20) );

        if (inBuff.pos > 0) {
            memmove(ress->srcBuffer, (char*)ress->srcBuffer + inBuff.pos, inBuff.size - inBuff.pos);
            ress->srcBufferLoaded -= inBuff.pos;
        }

        if (readSizeHint == 0) break;   /* end of frame */
        if (inBuff.size != inBuff.pos) EXM_THROW(37, "Decoding error : should consume entire input");

        /* Fill input buffer */
        {   size_t const toRead = MIN(readSizeHint, ress->srcBufferSize);  /* support large skippable frames */
            if (ress->srcBufferLoaded < toRead)
                ress->srcBufferLoaded += fread(((char*)ress->srcBuffer) + ress->srcBufferLoaded, 1, toRead - ress->srcBufferLoaded, finput);
            if (ress->srcBufferLoaded < toRead) EXM_THROW(39, "Read error : premature end");
    }   }

    FIO_fwriteSparseEnd(ress->dstFile, storedSkips);

    return frameSize;
}


#ifdef ZSTD_GZDECOMPRESS
static unsigned long long FIO_decompressGzFrame(dRess_t* ress, FILE* srcFile, const char* srcFileName)
{
    unsigned long long outFileSize = 0;
    z_stream strm;
    int flush = Z_NO_FLUSH;
    int ret;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in = 0;
    strm.avail_in = 0;
    if (inflateInit2(&strm, 15 /* maxWindowLogSize */ + 16 /* gzip only */) != Z_OK) return 0;  /* see http://www.zlib.net/manual.html */

    strm.next_out = (Bytef*)ress->dstBuffer;
    strm.avail_out = (uInt)ress->dstBufferSize;
    strm.avail_in = (uInt)ress->srcBufferLoaded;
    strm.next_in = (z_const unsigned char*)ress->srcBuffer;

    for ( ; ; ) {
        if (strm.avail_in == 0) {
            ress->srcBufferLoaded = fread(ress->srcBuffer, 1, ress->srcBufferSize, srcFile);
            if (ress->srcBufferLoaded == 0) flush = Z_FINISH;
            strm.next_in = (z_const unsigned char*)ress->srcBuffer;
            strm.avail_in = (uInt)ress->srcBufferLoaded;
        }
        ret = inflate(&strm, flush);
        if (ret == Z_BUF_ERROR) EXM_THROW(39, "zstd: %s: premature end", srcFileName);
        if (ret != Z_OK && ret != Z_STREAM_END) { DISPLAY("zstd: %s: inflate error %d \n", srcFileName, ret); return 0; }
        {   size_t const decompBytes = ress->dstBufferSize - strm.avail_out;
            if (decompBytes) {
                if (fwrite(ress->dstBuffer, 1, decompBytes, ress->dstFile) != decompBytes) EXM_THROW(31, "Write error : cannot write to output file");
                outFileSize += decompBytes;
                strm.next_out = (Bytef*)ress->dstBuffer;
                strm.avail_out = (uInt)ress->dstBufferSize;
            }
        }
        if (ret == Z_STREAM_END) break;
    }

    if (strm.avail_in > 0) memmove(ress->srcBuffer, strm.next_in, strm.avail_in);
    ress->srcBufferLoaded = strm.avail_in;
    ret = inflateEnd(&strm);
    if (ret != Z_OK) EXM_THROW(32, "zstd: %s: inflateEnd error %d", srcFileName, ret);
    return outFileSize;
}
#endif


#ifdef ZSTD_LZMADECOMPRESS
static unsigned long long FIO_decompressLzmaFrame(dRess_t* ress, FILE* srcFile, const char* srcFileName, int plain_lzma)
{
    unsigned long long outFileSize = 0;
    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_action action = LZMA_RUN;
    lzma_ret ret;

    strm.next_in = 0;
    strm.avail_in = 0;
    if (plain_lzma) {
        ret = lzma_alone_decoder(&strm, UINT64_MAX); /* LZMA */
    } else {
        ret = lzma_stream_decoder(&strm, UINT64_MAX, 0); /* XZ */
    }

    if (ret != LZMA_OK) EXM_THROW(71, "zstd: %s: lzma_alone_decoder/lzma_stream_decoder error %d", srcFileName, ret);

    strm.next_out = ress->dstBuffer;
    strm.avail_out = ress->dstBufferSize;
    strm.avail_in = ress->srcBufferLoaded;
    strm.next_in = ress->srcBuffer;

    for ( ; ; ) {
        if (strm.avail_in == 0) {
            ress->srcBufferLoaded = fread(ress->srcBuffer, 1, ress->srcBufferSize, srcFile);
            if (ress->srcBufferLoaded == 0) action = LZMA_FINISH;
            strm.next_in = ress->srcBuffer;
            strm.avail_in = ress->srcBufferLoaded;
        }
        ret = lzma_code(&strm, action);

        if (ret == LZMA_BUF_ERROR) EXM_THROW(39, "zstd: %s: premature end", srcFileName);
        if (ret != LZMA_OK && ret != LZMA_STREAM_END) { DISPLAY("zstd: %s: lzma_code decoding error %d \n", srcFileName, ret); return 0; }
        {   size_t const decompBytes = ress->dstBufferSize - strm.avail_out;
            if (decompBytes) {
                if (fwrite(ress->dstBuffer, 1, decompBytes, ress->dstFile) != decompBytes) EXM_THROW(31, "Write error : cannot write to output file");
                outFileSize += decompBytes;
                strm.next_out = ress->dstBuffer;
                strm.avail_out = ress->dstBufferSize;
            }
        }
        if (ret == LZMA_STREAM_END) break;
    }

    if (strm.avail_in > 0) memmove(ress->srcBuffer, strm.next_in, strm.avail_in);
    ress->srcBufferLoaded = strm.avail_in;
    lzma_end(&strm);
    return outFileSize;
}
#endif

#ifdef ZSTD_LZ4DECOMPRESS
static unsigned long long FIO_decompressLz4Frame(dRess_t* ress, FILE* srcFile, const char* srcFileName)
{
    unsigned long long filesize = 0;
    LZ4F_errorCode_t nextToLoad;
    LZ4F_decompressionContext_t dCtx;
    LZ4F_errorCode_t const errorCode = LZ4F_createDecompressionContext(&dCtx, LZ4F_VERSION);

    if (LZ4F_isError(errorCode)) EXM_THROW(61, "zstd: failed to create lz4 decompression context");

    /* Init feed with magic number (already consumed from FILE* sFile) */
    {   size_t inSize = 4;
        size_t outSize= 0;
        MEM_writeLE32(ress->srcBuffer, LZ4_MAGICNUMBER);
        nextToLoad = LZ4F_decompress(dCtx, ress->dstBuffer, &outSize, ress->srcBuffer, &inSize, NULL);
        if (LZ4F_isError(nextToLoad)) EXM_THROW(62, "zstd: %s: lz4 header error : %s", srcFileName, LZ4F_getErrorName(nextToLoad));
    }

    /* Main Loop */
    for (;nextToLoad;) {
        size_t readSize;
        size_t pos = 0;
        size_t decodedBytes = ress->dstBufferSize;

        /* Read input */
        if (nextToLoad > ress->srcBufferSize) nextToLoad = ress->srcBufferSize;
        readSize = fread(ress->srcBuffer, 1, nextToLoad, srcFile);
        if (!readSize) break;   /* reached end of file or stream */

        while ((pos < readSize) || (decodedBytes == ress->dstBufferSize)) {  /* still to read, or still to flush */
            /* Decode Input (at least partially) */
            size_t remaining = readSize - pos;
            decodedBytes = ress->dstBufferSize;
            nextToLoad = LZ4F_decompress(dCtx, ress->dstBuffer, &decodedBytes, (char*)(ress->srcBuffer)+pos, &remaining, NULL);
            if (LZ4F_isError(nextToLoad)) EXM_THROW(66, "zstd: %s: decompression error : %s", srcFileName, LZ4F_getErrorName(nextToLoad));
            pos += remaining;

            /* Write Block */
            if (decodedBytes) {
                if (fwrite(ress->dstBuffer, 1, decodedBytes, ress->dstFile) != decodedBytes) EXM_THROW(63, "Write error : cannot write to output file");
                filesize += decodedBytes;
                DISPLAYUPDATE(2, "\rDecompressed : %u MB  ", (unsigned)(filesize>>20));
            }

            if (!nextToLoad) break;
        }
    }
    /* can be out because readSize == 0, which could be an fread() error */
    if (ferror(srcFile)) EXM_THROW(67, "zstd: %s: read error", srcFileName);

    if (nextToLoad!=0) EXM_THROW(68, "zstd: %s: unfinished stream", srcFileName);

    LZ4F_freeDecompressionContext(dCtx);
    ress->srcBufferLoaded = 0; /* LZ4F will go to the frame boundary */

    return filesize;
}
#endif



/** FIO_decompressSrcFile() :
    Decompression `srcFileName` into `ress.dstFile`
    @return : 0 : OK
              1 : operation not started
*/
static int FIO_decompressSrcFile(dRess_t ress, const char* dstFileName, const char* srcFileName)
{
    FILE* srcFile;
    unsigned readSomething = 0;
    unsigned long long filesize = 0;

    if (UTIL_isDirectory(srcFileName)) {
        DISPLAYLEVEL(1, "zstd: %s is a directory -- ignored \n", srcFileName);
        return 1;
    }

    srcFile = FIO_openSrcFile(srcFileName);
    if (srcFile==NULL) return 1;

    /* for each frame */
    for ( ; ; ) {
        /* check magic number -> version */
        size_t const toRead = 4;
        const BYTE* buf = (const BYTE*)ress.srcBuffer;
        if (ress.srcBufferLoaded < toRead)
            ress.srcBufferLoaded += fread((char*)ress.srcBuffer + ress.srcBufferLoaded, (size_t)1, toRead - ress.srcBufferLoaded, srcFile);
        if (ress.srcBufferLoaded==0) {
            if (readSomething==0) { DISPLAY("zstd: %s: unexpected end of file \n", srcFileName); fclose(srcFile); return 1; }  /* srcFileName is empty */
            break;   /* no more input */
        }
        readSomething = 1;   /* there is at least >= 4 bytes in srcFile */
        if (ress.srcBufferLoaded < toRead) { DISPLAY("zstd: %s: unknown header \n", srcFileName); fclose(srcFile); return 1; }  /* srcFileName is empty */
        if (buf[0] == 31 && buf[1] == 139) { /* gz magic number */
#ifdef ZSTD_GZDECOMPRESS
            unsigned long long const result = FIO_decompressGzFrame(&ress, srcFile, srcFileName);
            if (result == 0) return 1;
            filesize += result;
#else
            DISPLAYLEVEL(1, "zstd: %s: gzip file cannot be uncompressed (zstd compiled without ZSTD_GZDECOMPRESS) -- ignored \n", srcFileName);
            return 1;
#endif
        } else if ((buf[0] == 0xFD && buf[1] == 0x37)  /* xz magic number */
                || (buf[0] == 0x5D && buf[1] == 0x00)) { /* lzma header (no magic number) */
#ifdef ZSTD_LZMADECOMPRESS
            unsigned long long const result = FIO_decompressLzmaFrame(&ress, srcFile, srcFileName, buf[0] != 0xFD);
            if (result == 0) return 1;
            filesize += result;
#else
            DISPLAYLEVEL(1, "zstd: %s: xz/lzma file cannot be uncompressed (zstd compiled without ZSTD_LZMADECOMPRESS) -- ignored \n", srcFileName);
            return 1;
#endif
        } else if (MEM_readLE32(buf) == LZ4_MAGICNUMBER) {
#ifdef ZSTD_LZ4DECOMPRESS
            unsigned long long const result = FIO_decompressLz4Frame(&ress, srcFile, srcFileName);
            if (result == 0) return 1;
            filesize += result;
#else
            DISPLAYLEVEL(1, "zstd: %s: lz4 file cannot be uncompressed (zstd compiled without ZSTD_LZ4DECOMPRESS) -- ignored \n", srcFileName);
            return 1;
#endif
        } else {
            if (!ZSTD_isFrame(ress.srcBuffer, toRead)) {
                if ((g_overwrite) && !strcmp (dstFileName, stdoutmark)) {  /* pass-through mode */
                    unsigned const result = FIO_passThrough(ress.dstFile, srcFile, ress.srcBuffer, ress.srcBufferSize, ress.srcBufferLoaded);
                    if (fclose(srcFile)) EXM_THROW(32, "zstd: %s close error", srcFileName);  /* error should never happen */
                    return result;
                } else {
                    DISPLAYLEVEL(1, "zstd: %s: not in zstd format \n", srcFileName);
                    fclose(srcFile);
                    return 1;
            }   }
            filesize += FIO_decompressFrame(&ress, srcFile, filesize);
        }
    }

    /* Final Status */
    DISPLAYLEVEL(2, "\r%79s\r", "");
    DISPLAYLEVEL(2, "%-20s: %llu bytes \n", srcFileName, filesize);

    /* Close file */
    if (fclose(srcFile)) EXM_THROW(33, "zstd: %s close error", srcFileName);  /* error should never happen */
    if (g_removeSrcFile /* --rm */ && strcmp(srcFileName, stdinmark)) { if (remove(srcFileName)) EXM_THROW(34, "zstd: %s: %s", srcFileName, strerror(errno)); };
    return 0;
}


/** FIO_decompressFile_extRess() :
    decompress `srcFileName` into `dstFileName`
    @return : 0 : OK
              1 : operation aborted (src not available, dst already taken, etc.)
*/
static int FIO_decompressDstFile(dRess_t ress,
                                 const char* dstFileName, const char* srcFileName)
{
    int result;
    stat_t statbuf;
    int stat_result = 0;

    ress.dstFile = FIO_openDstFile(dstFileName);
    if (ress.dstFile==0) return 1;

    if (strcmp (srcFileName, stdinmark) && UTIL_getFileStat(srcFileName, &statbuf)) stat_result = 1;
    result = FIO_decompressSrcFile(ress, dstFileName, srcFileName);

    if (fclose(ress.dstFile)) EXM_THROW(38, "Write error : cannot properly close %s", dstFileName);

    if ( (result != 0)
       && strcmp(dstFileName, nulmark)  /* special case : don't remove() /dev/null (#316) */
       && remove(dstFileName) )
        result=1;   /* don't do anything special if remove() fails */
    else if (strcmp (dstFileName, stdoutmark) && stat_result) UTIL_setFileStat(dstFileName, &statbuf);
    return result;
}


int FIO_decompressFilename(const char* dstFileName, const char* srcFileName,
                           const char* dictFileName)
{
    int missingFiles = 0;
    dRess_t ress = FIO_createDResources(dictFileName);

    missingFiles += FIO_decompressDstFile(ress, dstFileName, srcFileName);

    FIO_freeDResources(ress);
    return missingFiles;
}


#define MAXSUFFIXSIZE 8
int FIO_decompressMultipleFilenames(const char** srcNamesTable, unsigned nbFiles,
                                    const char* suffix,
                                    const char* dictFileName)
{
    int skippedFiles = 0;
    int missingFiles = 0;
    dRess_t ress = FIO_createDResources(dictFileName);

    if (suffix==NULL) EXM_THROW(70, "zstd: decompression: unknown dst");   /* should never happen */

    if (!strcmp(suffix, stdoutmark) || !strcmp(suffix, nulmark)) {  /* special cases : -c or -t */
        unsigned u;
        ress.dstFile = FIO_openDstFile(suffix);
        if (ress.dstFile == 0) EXM_THROW(71, "cannot open %s", suffix);
        for (u=0; u<nbFiles; u++)
            missingFiles += FIO_decompressSrcFile(ress, suffix, srcNamesTable[u]);
        if (fclose(ress.dstFile)) EXM_THROW(72, "Write error : cannot properly close stdout");
    } else {
        size_t suffixSize;
        size_t dfnSize = FNSPACE;
        unsigned u;
        char* dstFileName = (char*)malloc(FNSPACE);
        if (dstFileName==NULL) EXM_THROW(73, "not enough memory for dstFileName");
        for (u=0; u<nbFiles; u++) {   /* create dstFileName */
            const char* const srcFileName = srcNamesTable[u];
            const char* const suffixPtr = strrchr(srcFileName, '.');
            size_t const sfnSize = strlen(srcFileName);
            if (!suffixPtr) {
                DISPLAYLEVEL(1, "zstd: %s: unknown suffix -- ignored \n", srcFileName);
                skippedFiles++;
                continue;
            }
            suffixSize = strlen(suffixPtr);
            if (dfnSize+suffixSize <= sfnSize+1) {
                free(dstFileName);
                dfnSize = sfnSize + 20;
                dstFileName = (char*)malloc(dfnSize);
                if (dstFileName==NULL) EXM_THROW(74, "not enough memory for dstFileName");
            }
            if (sfnSize <= suffixSize || (strcmp(suffixPtr, GZ_EXTENSION) && strcmp(suffixPtr, XZ_EXTENSION) && strcmp(suffixPtr, ZSTD_EXTENSION) && strcmp(suffixPtr, LZMA_EXTENSION) && strcmp(suffixPtr, LZ4_EXTENSION))) {
                DISPLAYLEVEL(1, "zstd: %s: unknown suffix (%s/%s/%s/%s expected) -- ignored \n", srcFileName, GZ_EXTENSION, XZ_EXTENSION, ZSTD_EXTENSION, LZMA_EXTENSION);
                skippedFiles++;
                continue;
            } else {
                memcpy(dstFileName, srcFileName, sfnSize - suffixSize);
                dstFileName[sfnSize-suffixSize] = '\0';
            }

            missingFiles += FIO_decompressDstFile(ress, dstFileName, srcFileName);
        }
        free(dstFileName);
    }

    FIO_freeDResources(ress);
    return missingFiles + skippedFiles;
}

#endif /* #ifndef ZSTD_NODECOMPRESS */
