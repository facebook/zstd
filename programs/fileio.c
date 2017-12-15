/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
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
#include "util.h"       /* UTIL_getFileSize, UTIL_isRegularFile */
#include <stdio.h>      /* fprintf, fopen, fread, _fileno, stdin, stdout */
#include <stdlib.h>     /* malloc, free */
#include <string.h>     /* strcmp, strlen */
#include <errno.h>      /* errno */

#if defined (_MSC_VER)
#  include <sys/stat.h>
#  include <io.h>
#endif

#include "bitstream.h"
#include "mem.h"
#include "fileio.h"
#include "util.h"
#define ZSTD_STATIC_LINKING_ONLY   /* ZSTD_magicNumber, ZSTD_frameHeaderSize_max */
#include "zstd.h"
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
#  define LZ4F_ENABLE_OBSOLETE_ENUMS
#  include <lz4frame.h>
#  include <lz4.h>
#endif


/*-*************************************
*  Constants
***************************************/
#define KB *(1<<10)
#define MB *(1<<20)
#define GB *(1U<<30)

#define DICTSIZE_MAX (32 MB)   /* protection against large input (attack scenario) */

#define FNSPACE 30


/*-*************************************
*  Macros
***************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYOUT(...)      fprintf(stdout, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) { if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); } }
static int g_displayLevel = 2;   /* 0 : no display;  1: errors;  2: + result + interaction + warnings;  3: + progression;  4: + information */
void FIO_setNotificationLevel(unsigned level) { g_displayLevel=level; }

static const U64 g_refreshRate = SEC_TO_MICRO / 6;
static UTIL_time_t g_displayClock = UTIL_TIME_INITIALIZER;

#define DISPLAYUPDATE(l, ...) { if (g_displayLevel>=l) { \
            if ((UTIL_clockSpanMicro(g_displayClock) > g_refreshRate) || (g_displayLevel>=4)) \
            { g_displayClock = UTIL_getTime(); DISPLAY(__VA_ARGS__); \
            if (g_displayLevel>=4) fflush(stderr); } } }

#undef MIN  /* in case it would be already defined */
#define MIN(a,b)    ((a) < (b) ? (a) : (b))


/*-*************************************
*  Debug
***************************************/
#if defined(ZSTD_DEBUG) && (ZSTD_DEBUG>=1)
#  include <assert.h>
#else
#  ifndef assert
#    define assert(condition) ((void)0)
#  endif
#endif

#ifndef ZSTD_DEBUG
#  define ZSTD_DEBUG 0
#endif
#define DEBUGLOG(l,...) if (l<=ZSTD_DEBUG) DISPLAY(__VA_ARGS__);
#define EXM_THROW(error, ...)                                             \
{                                                                         \
    DISPLAYLEVEL(1, "zstd: ");                                            \
    DEBUGLOG(1, "Error defined at %s, line %i : \n", __FILE__, __LINE__); \
    DISPLAYLEVEL(1, "error %i : ", error);                                \
    DISPLAYLEVEL(1, __VA_ARGS__);                                         \
    DISPLAYLEVEL(1, " \n");                                               \
    exit(error);                                                          \
}

#define CHECK_V(v, f)                                \
    v = f;                                           \
    if (ZSTD_isError(v)) {                           \
        DEBUGLOG(1, "%s \n", #f);                    \
        EXM_THROW(11, "%s", ZSTD_getErrorName(v));   \
    }
#define CHECK(f) { size_t err; CHECK_V(err, f); }


/*-************************************
*  Signal (Ctrl-C trapping)
**************************************/
#include  <signal.h>

static const char* g_artefact = NULL;
static void INThandler(int sig)
{
    assert(sig==SIGINT); (void)sig;
#if !defined(_MSC_VER)
    signal(sig, SIG_IGN);  /* this invocation generates a buggy warning in Visual Studio */
#endif
    if (g_artefact) remove(g_artefact);
    DISPLAY("\n");
    exit(2);
}
static void addHandler(char const* dstFileName)
{
    if (UTIL_isRegularFile(dstFileName)) {
        g_artefact = dstFileName;
        signal(SIGINT, INThandler);
    } else {
        g_artefact = NULL;
    }
}
/* Idempotent */
static void clearHandler(void)
{
    if (g_artefact) signal(SIGINT, SIG_DFL);
    g_artefact = NULL;
}


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
static U32 g_sparseFileSupport = 1;   /* 0: no sparse allowed; 1: auto (file yes, stdout no); 2: force sparse */
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
    g_blockSize = blockSize;
}
#define FIO_OVERLAP_LOG_NOTSET 9999
static U32 g_overlapLog = FIO_OVERLAP_LOG_NOTSET;
void FIO_setOverlapLog(unsigned overlapLog){
    if (overlapLog && g_nbThreads==1)
        DISPLAYLEVEL(2, "Setting overlapLog is useless in single-thread mode \n");
    g_overlapLog = overlapLog;
}
static U32 g_ldmFlag = 0;
void FIO_setLdmFlag(unsigned ldmFlag) {
    g_ldmFlag = (ldmFlag>0);
}
static U32 g_ldmHashLog = 0;
void FIO_setLdmHashLog(unsigned ldmHashLog) {
    g_ldmHashLog = ldmHashLog;
}
static U32 g_ldmMinMatch = 0;
void FIO_setLdmMinMatch(unsigned ldmMinMatch) {
    g_ldmMinMatch = ldmMinMatch;
}

#define FIO_LDM_PARAM_NOTSET 9999
static U32 g_ldmBucketSizeLog = FIO_LDM_PARAM_NOTSET;
void FIO_setLdmBucketSizeLog(unsigned ldmBucketSizeLog) {
    g_ldmBucketSizeLog = ldmBucketSizeLog;
}

static U32 g_ldmHashEveryLog = FIO_LDM_PARAM_NOTSET;
void FIO_setLdmHashEveryLog(unsigned ldmHashEveryLog) {
    g_ldmHashEveryLog = ldmHashEveryLog;
}



/*-*************************************
*  Functions
***************************************/
/** FIO_remove() :
 * @result : Unlink `fileName`, even if it's read-only */
static int FIO_remove(const char* path)
{
    if (!UTIL_isRegularFile(path)) {
        DISPLAYLEVEL(2, "zstd: Refusing to remove non-regular file %s\n", path);
        return 0;
    }
#if defined(_WIN32) || defined(WIN32)
    /* windows doesn't allow remove read-only files,
     * so try to make it writable first */
    chmod(path, _S_IWRITE);
#endif
    return remove(path);
}

/** FIO_openSrcFile() :
 *  condition : `srcFileName` must be non-NULL.
 * @result : FILE* to `srcFileName`, or NULL if it fails */
static FILE* FIO_openSrcFile(const char* srcFileName)
{
    assert(srcFileName != NULL);
    if (!strcmp (srcFileName, stdinmark)) {
        DISPLAYLEVEL(4,"Using stdin for input\n");
        SET_BINARY_MODE(stdin);
        return stdin;
    }

    if (!UTIL_isRegularFile(srcFileName)) {
        DISPLAYLEVEL(1, "zstd: %s is not a regular file -- ignored \n",
                        srcFileName);
        return NULL;
    }

    {   FILE* const f = fopen(srcFileName, "rb");
        if (f == NULL)
            DISPLAYLEVEL(1, "zstd: %s: %s \n", srcFileName, strerror(errno));
        return f;
    }
}

/** FIO_openDstFile() :
 *  condition : `dstFileName` must be non-NULL.
 * @result : FILE* to `dstFileName`, or NULL if it fails */
static FILE* FIO_openDstFile(const char* dstFileName)
{
    assert(dstFileName != NULL);
    if (!strcmp (dstFileName, stdoutmark)) {
        DISPLAYLEVEL(4,"Using stdout for output\n");
        SET_BINARY_MODE(stdout);
        if (g_sparseFileSupport==1) {
            g_sparseFileSupport = 0;
            DISPLAYLEVEL(4, "Sparse File Support is automatically disabled on stdout ; try --sparse \n");
        }
        return stdout;
    }

    if (g_sparseFileSupport == 1) {
        g_sparseFileSupport = ZSTD_SPARSE_DEFAULT;
    }

    if (UTIL_isRegularFile(dstFileName)) {
        FILE* fCheck;
        if (!strcmp(dstFileName, nulmark)) {
            EXM_THROW(40, "%s is unexpectedly a regular file", dstFileName);
        }
        /* Check if destination file already exists */
        fCheck = fopen( dstFileName, "rb" );
        if (fCheck != NULL) {  /* dst file exists, authorization prompt */
            fclose(fCheck);
            if (!g_overwrite) {
                if (g_displayLevel <= 1) {
                    /* No interaction possible */
                    DISPLAY("zstd: %s already exists; not overwritten  \n",
                            dstFileName);
                    return NULL;
                }
                DISPLAY("zstd: %s already exists; overwrite (y/N) ? ",
                        dstFileName);
                {   int ch = getchar();
                    if ((ch!='Y') && (ch!='y')) {
                        DISPLAY("    not overwritten  \n");
                        return NULL;
                    }
                    /* flush rest of input line */
                    while ((ch!=EOF) && (ch!='\n')) ch = getchar();
            }   }
            /* need to unlink */
            FIO_remove(dstFileName);
    }   }

    {   FILE* const f = fopen( dstFileName, "wb" );
        if (f == NULL)
            DISPLAYLEVEL(1, "zstd: %s: %s\n", dstFileName, strerror(errno));
        return f;
    }
}


/*! FIO_createDictBuffer() :
 *  creates a buffer, pointed by `*bufferPtr`,
 *  loads `filename` content into it, up to DICTSIZE_MAX bytes.
 * @return : loaded size
 *  if fileName==NULL, returns 0 and a NULL pointer
 */
static size_t FIO_createDictBuffer(void** bufferPtr, const char* fileName)
{
    FILE* fileHandle;
    U64 fileSize;

    assert(bufferPtr != NULL);
    *bufferPtr = NULL;
    if (fileName == NULL) return 0;

    DISPLAYLEVEL(4,"Loading %s as dictionary \n", fileName);
    fileHandle = fopen(fileName, "rb");
    if (fileHandle==NULL) EXM_THROW(31, "%s: %s", fileName, strerror(errno));
    fileSize = UTIL_getFileSize(fileName);
    if (fileSize > DICTSIZE_MAX) {
        EXM_THROW(32, "Dictionary file %s is too large (> %u MB)",
                        fileName, DICTSIZE_MAX >> 20);   /* avoid extreme cases */
    }
    *bufferPtr = malloc((size_t)fileSize);
    if (*bufferPtr==NULL) EXM_THROW(34, "%s", strerror(errno));
    {   size_t const readSize = fread(*bufferPtr, 1, (size_t)fileSize, fileHandle);
        if (readSize!=fileSize)
            EXM_THROW(35, "Error reading dictionary file %s", fileName);
    }
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
    ZSTD_CStream* cctx;
} cRess_t;

static cRess_t FIO_createCResources(const char* dictFileName, int cLevel,
                                    U64 srcSize,
                                    ZSTD_compressionParameters* comprParams) {
    cRess_t ress;
    memset(&ress, 0, sizeof(ress));

    ress.cctx = ZSTD_createCCtx();
    if (ress.cctx == NULL)
        EXM_THROW(30, "allocation error : can't create ZSTD_CCtx");
    ress.srcBufferSize = ZSTD_CStreamInSize();
    ress.srcBuffer = malloc(ress.srcBufferSize);
    ress.dstBufferSize = ZSTD_CStreamOutSize();
    ress.dstBuffer = malloc(ress.dstBufferSize);
    if (!ress.srcBuffer || !ress.dstBuffer)
        EXM_THROW(31, "allocation error : not enough memory");

    /* Advances parameters, including dictionary */
    {   void* dictBuffer;
        size_t const dictBuffSize = FIO_createDictBuffer(&dictBuffer, dictFileName);   /* works with dictFileName==NULL */
        if (dictFileName && (dictBuffer==NULL))
            EXM_THROW(32, "allocation error : can't create dictBuffer");

        CHECK( ZSTD_CCtx_setParameter(ress.cctx, ZSTD_p_contentSizeFlag, 1) );  /* always enable content size when available (note: supposed to be default) */
        CHECK( ZSTD_CCtx_setParameter(ress.cctx, ZSTD_p_dictIDFlag, g_dictIDFlag) );
        CHECK( ZSTD_CCtx_setParameter(ress.cctx, ZSTD_p_checksumFlag, g_checksumFlag) );
        /* compression level */
        CHECK( ZSTD_CCtx_setParameter(ress.cctx, ZSTD_p_compressionLevel, cLevel) );
        /* long distance matching */
        CHECK( ZSTD_CCtx_setParameter(
                      ress.cctx, ZSTD_p_enableLongDistanceMatching, g_ldmFlag) );
        CHECK( ZSTD_CCtx_setParameter(ress.cctx, ZSTD_p_ldmHashLog, g_ldmHashLog) );
        CHECK( ZSTD_CCtx_setParameter(ress.cctx, ZSTD_p_ldmMinMatch, g_ldmMinMatch) );
        if (g_ldmBucketSizeLog != FIO_LDM_PARAM_NOTSET) {
            CHECK( ZSTD_CCtx_setParameter(ress.cctx, ZSTD_p_ldmBucketSizeLog, g_ldmBucketSizeLog) );
        }
        if (g_ldmHashEveryLog != FIO_LDM_PARAM_NOTSET) {
            CHECK( ZSTD_CCtx_setParameter(ress.cctx, ZSTD_p_ldmHashEveryLog, g_ldmHashEveryLog) );
        }
        /* compression parameters */
        CHECK( ZSTD_CCtx_setParameter(ress.cctx, ZSTD_p_windowLog, comprParams->windowLog) );
        CHECK( ZSTD_CCtx_setParameter(ress.cctx, ZSTD_p_chainLog, comprParams->chainLog) );
        CHECK( ZSTD_CCtx_setParameter(ress.cctx, ZSTD_p_hashLog, comprParams->hashLog) );
        CHECK( ZSTD_CCtx_setParameter(ress.cctx, ZSTD_p_searchLog, comprParams->searchLog) );
        CHECK( ZSTD_CCtx_setParameter(ress.cctx, ZSTD_p_minMatch, comprParams->searchLength) );
        CHECK( ZSTD_CCtx_setParameter(ress.cctx, ZSTD_p_targetLength, comprParams->targetLength) );
        CHECK( ZSTD_CCtx_setParameter(ress.cctx, ZSTD_p_compressionStrategy, (U32)comprParams->strategy) );
        /* multi-threading */
        DISPLAYLEVEL(5,"set nb threads = %u \n", g_nbThreads);
        CHECK( ZSTD_CCtx_setParameter(ress.cctx, ZSTD_p_nbThreads, g_nbThreads) );
        /* dictionary */
        CHECK( ZSTD_CCtx_setPledgedSrcSize(ress.cctx, srcSize) );  /* just for dictionary loading, for compression parameters adaptation */
        CHECK( ZSTD_CCtx_loadDictionary(ress.cctx, dictBuffer, dictBuffSize) );
        CHECK( ZSTD_CCtx_setPledgedSrcSize(ress.cctx, ZSTD_CONTENTSIZE_UNKNOWN) );  /* reset */

        free(dictBuffer);
    }

    return ress;
}

static void FIO_freeCResources(cRess_t ress)
{
    free(ress.srcBuffer);
    free(ress.dstBuffer);
    ZSTD_freeCStream(ress.cctx);   /* never fails */
}


#ifdef ZSTD_GZCOMPRESS
static unsigned long long FIO_compressGzFrame(cRess_t* ress,
                    const char* srcFileName, U64 const srcFileSize,
                    int compressionLevel, U64* readsize)
{
    unsigned long long inFileSize = 0, outFileSize = 0;
    z_stream strm;
    int ret;

    if (compressionLevel > Z_BEST_COMPRESSION)
        compressionLevel = Z_BEST_COMPRESSION;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    ret = deflateInit2(&strm, compressionLevel, Z_DEFLATED,
                        15 /* maxWindowLogSize */ + 16 /* gzip only */,
                        8, Z_DEFAULT_STRATEGY); /* see http://www.zlib.net/manual.html */
    if (ret != Z_OK)
        EXM_THROW(71, "zstd: %s: deflateInit2 error %d \n", srcFileName, ret);

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
        if (ret != Z_OK)
            EXM_THROW(72, "zstd: %s: deflate error %d \n", srcFileName, ret);
        {   size_t const decompBytes = ress->dstBufferSize - strm.avail_out;
            if (decompBytes) {
                if (fwrite(ress->dstBuffer, 1, decompBytes, ress->dstFile) != decompBytes)
                    EXM_THROW(73, "Write error : cannot write to output file");
                outFileSize += decompBytes;
                strm.next_out = (Bytef*)ress->dstBuffer;
                strm.avail_out = (uInt)ress->dstBufferSize;
            }
        }
        if (srcFileSize == UTIL_FILESIZE_UNKNOWN)
            DISPLAYUPDATE(2, "\rRead : %u MB ==> %.2f%%",
                            (U32)(inFileSize>>20),
                            (double)outFileSize/inFileSize*100)
        else
            DISPLAYUPDATE(2, "\rRead : %u / %u MB ==> %.2f%%",
                            (U32)(inFileSize>>20), (U32)(srcFileSize>>20),
                            (double)outFileSize/inFileSize*100);
    }

    while (1) {
        ret = deflate(&strm, Z_FINISH);
        {   size_t const decompBytes = ress->dstBufferSize - strm.avail_out;
            if (decompBytes) {
                if (fwrite(ress->dstBuffer, 1, decompBytes, ress->dstFile) != decompBytes)
                    EXM_THROW(75, "Write error : cannot write to output file");
                outFileSize += decompBytes;
                strm.next_out = (Bytef*)ress->dstBuffer;
                strm.avail_out = (uInt)ress->dstBufferSize;
        }   }
        if (ret == Z_STREAM_END) break;
        if (ret != Z_BUF_ERROR)
            EXM_THROW(77, "zstd: %s: deflate error %d \n", srcFileName, ret);
    }

    ret = deflateEnd(&strm);
    if (ret != Z_OK)
        EXM_THROW(79, "zstd: %s: deflateEnd error %d \n", srcFileName, ret);
    *readsize = inFileSize;

    return outFileSize;
}
#endif


#ifdef ZSTD_LZMACOMPRESS
static unsigned long long FIO_compressLzmaFrame(cRess_t* ress,
                            const char* srcFileName, U64 const srcFileSize,
                            int compressionLevel, U64* readsize, int plain_lzma)
{
    unsigned long long inFileSize = 0, outFileSize = 0;
    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_action action = LZMA_RUN;
    lzma_ret ret;

    if (compressionLevel < 0) compressionLevel = 0;
    if (compressionLevel > 9) compressionLevel = 9;

    if (plain_lzma) {
        lzma_options_lzma opt_lzma;
        if (lzma_lzma_preset(&opt_lzma, compressionLevel))
            EXM_THROW(71, "zstd: %s: lzma_lzma_preset error", srcFileName);
        ret = lzma_alone_encoder(&strm, &opt_lzma); /* LZMA */
        if (ret != LZMA_OK)
            EXM_THROW(71, "zstd: %s: lzma_alone_encoder error %d", srcFileName, ret);
    } else {
        ret = lzma_easy_encoder(&strm, compressionLevel, LZMA_CHECK_CRC64); /* XZ */
        if (ret != LZMA_OK)
            EXM_THROW(71, "zstd: %s: lzma_easy_encoder error %d", srcFileName, ret);
    }

    strm.next_in = 0;
    strm.avail_in = 0;
    strm.next_out = (BYTE*)ress->dstBuffer;
    strm.avail_out = ress->dstBufferSize;

    while (1) {
        if (strm.avail_in == 0) {
            size_t const inSize = fread(ress->srcBuffer, 1, ress->srcBufferSize, ress->srcFile);
            if (inSize == 0) action = LZMA_FINISH;
            inFileSize += inSize;
            strm.next_in = (BYTE const*)ress->srcBuffer;
            strm.avail_in = inSize;
        }

        ret = lzma_code(&strm, action);

        if (ret != LZMA_OK && ret != LZMA_STREAM_END)
            EXM_THROW(72, "zstd: %s: lzma_code encoding error %d", srcFileName, ret);
        {   size_t const compBytes = ress->dstBufferSize - strm.avail_out;
            if (compBytes) {
                if (fwrite(ress->dstBuffer, 1, compBytes, ress->dstFile) != compBytes)
                    EXM_THROW(73, "Write error : cannot write to output file");
                outFileSize += compBytes;
                strm.next_out = (BYTE*)ress->dstBuffer;
                strm.avail_out = ress->dstBufferSize;
        }   }
        if (srcFileSize == UTIL_FILESIZE_UNKNOWN)
            DISPLAYUPDATE(2, "\rRead : %u MB ==> %.2f%%",
                            (U32)(inFileSize>>20),
                            (double)outFileSize/inFileSize*100)
        else
            DISPLAYUPDATE(2, "\rRead : %u / %u MB ==> %.2f%%",
                            (U32)(inFileSize>>20), (U32)(srcFileSize>>20),
                            (double)outFileSize/inFileSize*100);
        if (ret == LZMA_STREAM_END) break;
    }

    lzma_end(&strm);
    *readsize = inFileSize;

    return outFileSize;
}
#endif

#ifdef ZSTD_LZ4COMPRESS
#if LZ4_VERSION_NUMBER <= 10600
#define LZ4F_blockLinked blockLinked
#define LZ4F_max64KB max64KB
#endif
static int FIO_LZ4_GetBlockSize_FromBlockId (int id) { return (1 << (8 + (2 * id))); }
static unsigned long long FIO_compressLz4Frame(cRess_t* ress,
                            const char* srcFileName, U64 const srcFileSize,
                            int compressionLevel, U64* readsize)
{
    const size_t blockSize = FIO_LZ4_GetBlockSize_FromBlockId(LZ4F_max64KB);
    unsigned long long inFileSize = 0, outFileSize = 0;

    LZ4F_preferences_t prefs;
    LZ4F_compressionContext_t ctx;

    LZ4F_errorCode_t const errorCode = LZ4F_createCompressionContext(&ctx, LZ4F_VERSION);
    if (LZ4F_isError(errorCode))
        EXM_THROW(31, "zstd: failed to create lz4 compression context");

    memset(&prefs, 0, sizeof(prefs));

    assert(blockSize <= ress->srcBufferSize);

    prefs.autoFlush = 1;
    prefs.compressionLevel = compressionLevel;
    prefs.frameInfo.blockMode = LZ4F_blockLinked;
    prefs.frameInfo.blockSizeID = LZ4F_max64KB;
    prefs.frameInfo.contentChecksumFlag = (contentChecksum_t)g_checksumFlag;
#if LZ4_VERSION_NUMBER >= 10600
    prefs.frameInfo.contentSize = (srcFileSize==UTIL_FILESIZE_UNKNOWN) ? 0 : srcFileSize;
#endif
    assert(LZ4F_compressBound(blockSize, &prefs) <= ress->dstBufferSize);

    {
        size_t readSize;
        size_t headerSize = LZ4F_compressBegin(ctx, ress->dstBuffer, ress->dstBufferSize, &prefs);
        if (LZ4F_isError(headerSize))
            EXM_THROW(33, "File header generation failed : %s",
                            LZ4F_getErrorName(headerSize));
        if (fwrite(ress->dstBuffer, 1, headerSize, ress->dstFile) != headerSize)
            EXM_THROW(34, "Write error : cannot write header");
        outFileSize += headerSize;

        /* Read first block */
        readSize  = fread(ress->srcBuffer, (size_t)1, (size_t)blockSize, ress->srcFile);
        inFileSize += readSize;

        /* Main Loop */
        while (readSize>0) {
            size_t outSize;

            /* Compress Block */
            outSize = LZ4F_compressUpdate(ctx, ress->dstBuffer, ress->dstBufferSize, ress->srcBuffer, readSize, NULL);
            if (LZ4F_isError(outSize))
                EXM_THROW(35, "zstd: %s: lz4 compression failed : %s",
                            srcFileName, LZ4F_getErrorName(outSize));
            outFileSize += outSize;
            if (srcFileSize == UTIL_FILESIZE_UNKNOWN)
                DISPLAYUPDATE(2, "\rRead : %u MB ==> %.2f%%",
                                (U32)(inFileSize>>20),
                                (double)outFileSize/inFileSize*100)
            else
                DISPLAYUPDATE(2, "\rRead : %u / %u MB ==> %.2f%%",
                                (U32)(inFileSize>>20), (U32)(srcFileSize>>20),
                                (double)outFileSize/inFileSize*100);

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
        if (LZ4F_isError(headerSize))
            EXM_THROW(38, "zstd: %s: lz4 end of file generation failed : %s",
                        srcFileName, LZ4F_getErrorName(headerSize));

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
    ZSTD_EndDirective directive = ZSTD_e_continue;
    DISPLAYLEVEL(5, "%s: %u bytes \n", srcFileName, (U32)fileSize);

    switch (g_compressionType) {
        case FIO_zstdCompression:
            break;

        case FIO_gzipCompression:
#ifdef ZSTD_GZCOMPRESS
            compressedfilesize = FIO_compressGzFrame(&ress, srcFileName, fileSize, compressionLevel, &readsize);
#else
            (void)compressionLevel;
            EXM_THROW(20, "zstd: %s: file cannot be compressed as gzip (zstd compiled without ZSTD_GZCOMPRESS) -- ignored \n",
                            srcFileName);
#endif
            goto finish;

        case FIO_xzCompression:
        case FIO_lzmaCompression:
#ifdef ZSTD_LZMACOMPRESS
            compressedfilesize = FIO_compressLzmaFrame(&ress, srcFileName, fileSize, compressionLevel, &readsize, g_compressionType==FIO_lzmaCompression);
#else
            (void)compressionLevel;
            EXM_THROW(20, "zstd: %s: file cannot be compressed as xz/lzma (zstd compiled without ZSTD_LZMACOMPRESS) -- ignored \n",
                            srcFileName);
#endif
            goto finish;

        case FIO_lz4Compression:
#ifdef ZSTD_LZ4COMPRESS
            compressedfilesize = FIO_compressLz4Frame(&ress, srcFileName, fileSize, compressionLevel, &readsize);
#else
            (void)compressionLevel;
            EXM_THROW(20, "zstd: %s: file cannot be compressed as lz4 (zstd compiled without ZSTD_LZ4COMPRESS) -- ignored \n",
                            srcFileName);
#endif
            goto finish;
    }

    /* init */
    if (fileSize != UTIL_FILESIZE_UNKNOWN)
        ZSTD_CCtx_setPledgedSrcSize(ress.cctx, fileSize);

    /* Main compression loop */
    do {
        size_t result;
        /* Fill input Buffer */
        size_t const inSize = fread(ress.srcBuffer, (size_t)1, ress.srcBufferSize, srcFile);
        ZSTD_inBuffer inBuff = { ress.srcBuffer, inSize, 0 };
        readsize += inSize;

        if (inSize == 0 || (fileSize != UTIL_FILESIZE_UNKNOWN && readsize == fileSize))
            directive = ZSTD_e_end;

        result = 1;
        while (inBuff.pos != inBuff.size || (directive == ZSTD_e_end && result != 0)) {
            ZSTD_outBuffer outBuff = { ress.dstBuffer, ress.dstBufferSize, 0 };
            CHECK_V(result, ZSTD_compress_generic(ress.cctx, &outBuff, &inBuff, directive));

            /* Write compressed stream */
            DISPLAYLEVEL(6, "ZSTD_compress_generic,ZSTD_e_continue: generated %u bytes \n",
                            (U32)outBuff.pos);
            if (outBuff.pos) {
                size_t const sizeCheck = fwrite(ress.dstBuffer, 1, outBuff.pos, dstFile);
                if (sizeCheck!=outBuff.pos)
                    EXM_THROW(25, "Write error : cannot write compressed block into %s", dstFileName);
                compressedfilesize += outBuff.pos;
            }
        }
        if (g_nbThreads > 1) {
            if (fileSize == UTIL_FILESIZE_UNKNOWN)
                DISPLAYUPDATE(2, "\rRead : %u MB", (U32)(readsize>>20))
            else
                DISPLAYUPDATE(2, "\rRead : %u / %u MB",
                                    (U32)(readsize>>20), (U32)(fileSize>>20));
        } else {
            if (fileSize == UTIL_FILESIZE_UNKNOWN)
                DISPLAYUPDATE(2, "\rRead : %u MB ==> %.2f%%",
                                (U32)(readsize>>20),
                                (double)compressedfilesize/readsize*100)
            else
                DISPLAYUPDATE(2, "\rRead : %u / %u MB ==> %.2f%%",
                                (U32)(readsize>>20), (U32)(fileSize>>20),
                                (double)compressedfilesize/readsize*100);
        }
    } while (directive != ZSTD_e_end);

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
                            const char* dstFileName, const char* srcFileName,
                            int compressionLevel)
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
        /* We must clear the handler, since after this point calling it would
         * delete both the source and destination files.
         */
        clearHandler();
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
                                        const char* dstFileName,
                                        const char* srcFileName,
                                        int compressionLevel)
{
    int result;
    stat_t statbuf;
    int stat_result = 0;

    ress.dstFile = FIO_openDstFile(dstFileName);
    if (ress.dstFile==NULL) return 1;  /* could not open dstFileName */
    /* Must ony be added after FIO_openDstFile() succeeds.
     * Otherwise we may delete the destination file if at already exists, and
     * the user presses Ctrl-C when asked if they wish to overwrite.
     */
    addHandler(dstFileName);

    if (strcmp (srcFileName, stdinmark) && UTIL_getFileStat(srcFileName, &statbuf))
        stat_result = 1;
    result = FIO_compressFilename_srcFile(ress, dstFileName, srcFileName, compressionLevel);
    clearHandler();

    if (fclose(ress.dstFile)) { /* error closing dstFile */
        DISPLAYLEVEL(1, "zstd: %s: %s \n", dstFileName, strerror(errno));
        result=1;
    }
    if (result!=0) {  /* remove operation artefact */
        if (remove(dstFileName))
            EXM_THROW(1, "zstd: %s: %s", dstFileName, strerror(errno));
    }
    else if (strcmp (dstFileName, stdoutmark) && stat_result)
        UTIL_setFileStat(dstFileName, &statbuf);

    return result;
}


int FIO_compressFilename(const char* dstFileName, const char* srcFileName,
                         const char* dictFileName, int compressionLevel, ZSTD_compressionParameters* comprParams)
{
    clock_t const start = clock();
    U64 const fileSize = UTIL_getFileSize(srcFileName);
    U64 const srcSize = (fileSize == UTIL_FILESIZE_UNKNOWN) ? ZSTD_CONTENTSIZE_UNKNOWN : fileSize;

    cRess_t const ress = FIO_createCResources(dictFileName, compressionLevel, srcSize, comprParams);
    int const result = FIO_compressFilename_dstFile(ress, dstFileName, srcFileName, compressionLevel);

    double const seconds = (double)(clock() - start) / CLOCKS_PER_SEC;
    DISPLAYLEVEL(4, "Completed in %.2f sec \n", seconds);

    FIO_freeCResources(ress);
    return result;
}


int FIO_compressMultipleFilenames(const char** inFileNamesTable, unsigned nbFiles,
                                  const char* outFileName, const char* suffix,
                                  const char* dictFileName, int compressionLevel,
                                  ZSTD_compressionParameters* comprParams)
{
    int missed_files = 0;
    size_t dfnSize = FNSPACE;
    char*  dstFileName = (char*)malloc(FNSPACE);
    size_t const suffixSize = suffix ? strlen(suffix) : 0;
    U64 const firstFileSize = UTIL_getFileSize(inFileNamesTable[0]);
    U64 const firstSrcSize = (firstFileSize == UTIL_FILESIZE_UNKNOWN) ? ZSTD_CONTENTSIZE_UNKNOWN : firstFileSize;
    U64 const srcSize = (nbFiles != 1) ? ZSTD_CONTENTSIZE_UNKNOWN : firstSrcSize ;
    cRess_t ress = FIO_createCResources(dictFileName, compressionLevel, srcSize, comprParams);

    /* init */
    if (dstFileName==NULL)
        EXM_THROW(27, "FIO_compressMultipleFilenames : allocation error for dstFileName");
    if (outFileName == NULL && suffix == NULL)
        EXM_THROW(28, "FIO_compressMultipleFilenames : dst unknown");  /* should never happen */

    /* loop on each file */
    if (outFileName != NULL) {
        unsigned u;
        ress.dstFile = FIO_openDstFile(outFileName);
        for (u=0; u<nbFiles; u++)
            missed_files += FIO_compressFilename_srcFile(ress, outFileName, inFileNamesTable[u], compressionLevel);
        if (fclose(ress.dstFile))
            EXM_THROW(29, "Write error : cannot properly close stdout");
    } else {
        unsigned u;
        for (u=0; u<nbFiles; u++) {
            size_t const ifnSize = strlen(inFileNamesTable[u]);
            if (dfnSize <= ifnSize+suffixSize+1) {  /* resize name buffer */
                free(dstFileName);
                dfnSize = ifnSize + 20;
                dstFileName = (char*)malloc(dfnSize);
                if (!dstFileName) {
                    EXM_THROW(30, "zstd: %s", strerror(errno));
            }   }
            strcpy(dstFileName, inFileNamesTable[u]);
            strcat(dstFileName, suffix);
            missed_files += FIO_compressFilename_dstFile(ress, dstFileName, inFileNamesTable[u], compressionLevel);
    }   }

    FIO_freeCResources(ress);
    free(dstFileName);
    return missed_files;
}

#endif /* #ifndef ZSTD_NOCOMPRESS */



#ifndef ZSTD_NODECOMPRESS

/* **************************************************************************
 *  Decompression
 ***************************************************************************/
typedef struct {
    void*  srcBuffer;
    size_t srcBufferSize;
    size_t srcBufferLoaded;
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
    CHECK( ZSTD_setDStreamParameter(ress.dctx, DStream_p_maxWindowSize, g_memLimit) );
    ress.srcBufferSize = ZSTD_DStreamInSize();
    ress.srcBuffer = malloc(ress.srcBufferSize);
    ress.dstBufferSize = ZSTD_DStreamOutSize();
    ress.dstBuffer = malloc(ress.dstBufferSize);
    if (!ress.srcBuffer || !ress.dstBuffer)
        EXM_THROW(61, "Allocation error : not enough memory");

    /* dictionary */
    {   void* dictBuffer;
        size_t const dictBufferSize = FIO_createDictBuffer(&dictBuffer, dictFileName);
        CHECK( ZSTD_initDStream_usingDict(ress.dctx, dictBuffer, dictBufferSize) );
        free(dictBuffer);
    }

    return ress;
}

static void FIO_freeDResources(dRess_t ress)
{
    CHECK( ZSTD_freeDStream(ress.dctx) );
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
                if (sizeCheck != seg0SizeT)
                    EXM_THROW(73, "Write error : cannot write decoded block");
        }   }
        ptrT += seg0SizeT;
    }

    {   static size_t const maskT = sizeof(size_t)-1;
        if (bufferSize & maskT) {
            /* size not multiple of sizeof(size_t) : implies end of block */
            const char* const restStart = (const char*)bufferTEnd;
            const char* restPtr = restStart;
            size_t restSize =  bufferSize & maskT;
            const char* const restEnd = restStart + restSize;
            for ( ; (restPtr < restEnd) && (*restPtr == 0); restPtr++) ;
            storedSkips += (unsigned) (restPtr - restStart);
            if (restPtr != restEnd) {
                int seekResult = LONG_SEEK(file, storedSkips, SEEK_CUR);
                if (seekResult)
                    EXM_THROW(74, "Sparse skip error ; try --no-sparse");
                storedSkips = 0;
                {   size_t const sizeCheck = fwrite(restPtr, 1, restEnd - restPtr, file);
                    if (sizeCheck != (size_t)(restEnd - restPtr))
                        EXM_THROW(75, "Write error : cannot write decoded end of block");
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
            if (sizeCheck != 1)
                EXM_THROW(69, "Write error : cannot write last zero");
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
    {   size_t const sizeCheck = fwrite(buffer, 1, alreadyLoaded, foutput);
        if (sizeCheck != alreadyLoaded) {
            DISPLAYLEVEL(1, "Pass-through write error \n");
            return 1;
    }   }

    while (readFromInput) {
        readFromInput = fread(buffer, 1, blockSize, finput);
        storedSkips = FIO_fwriteSparse(foutput, buffer, readFromInput, storedSkips);
    }

    FIO_fwriteSparseEnd(foutput, storedSkips);
    return 0;
}

static void FIO_zstdErrorHelp(dRess_t* ress, size_t ret, char const* srcFileName)
{
    ZSTD_frameHeader header;
    /* No special help for these errors */
    if (ZSTD_getErrorCode(ret) != ZSTD_error_frameParameter_windowTooLarge)
        return;
    /* Try to decode the frame header */
    ret = ZSTD_getFrameHeader(&header, ress->srcBuffer, ress->srcBufferLoaded);
    if (ret == 0) {
        U32 const windowSize = (U32)header.windowSize;
        U32 const windowLog = BIT_highbit32(windowSize) + ((windowSize & (windowSize - 1)) != 0);
        U32 const windowMB = (windowSize >> 20) + ((windowSize & ((1 MB) - 1)) != 0);
        assert(header.windowSize <= (U64)((U32)-1));
        assert(g_memLimit > 0);
        DISPLAYLEVEL(1, "%s : Window size larger than maximum : %llu > %u\n",
                        srcFileName, header.windowSize, g_memLimit);
        if (windowLog <= ZSTD_WINDOWLOG_MAX) {
            DISPLAYLEVEL(1, "%s : Use --long=%u or --memory=%uMB\n",
                            srcFileName, windowLog, windowMB);
            return;
        }
    } else if (ZSTD_getErrorCode(ret) != ZSTD_error_frameParameter_windowTooLarge) {
        DISPLAYLEVEL(1, "%s : Error decoding frame header to read window size : %s\n",
                        srcFileName, ZSTD_getErrorName(ret));
        return;
    }
    DISPLAYLEVEL(1, "%s : Window log larger than ZSTD_WINDOWLOG_MAX=%u not supported\n",
                    srcFileName, ZSTD_WINDOWLOG_MAX);
}

/** FIO_decompressFrame() :
 *  @return : size of decoded zstd frame, or an error code
*/
#define FIO_ERROR_FRAME_DECODING   ((unsigned long long)(-2))
unsigned long long FIO_decompressZstdFrame(dRess_t* ress,
                                       FILE* finput,
                                       const char* srcFileName,
                                       U64 alreadyDecoded)
{
    U64 frameSize = 0;
    U32 storedSkips = 0;

    size_t const srcFileLength = strlen(srcFileName);
    if (srcFileLength>20) srcFileName += srcFileLength-20;  /* display last 20 characters only */

    ZSTD_resetDStream(ress->dctx);

    /* Header loading : ensures ZSTD_getFrameHeader() will succeed */
    {   size_t const toDecode = ZSTD_FRAMEHEADERSIZE_MAX;
        if (ress->srcBufferLoaded < toDecode) {
            size_t const toRead = toDecode - ress->srcBufferLoaded;
            void* const startPosition = (char*)ress->srcBuffer + ress->srcBufferLoaded;
            ress->srcBufferLoaded += fread(startPosition, 1, toRead, finput);
    }   }

    /* Main decompression Loop */
    while (1) {
        ZSTD_inBuffer  inBuff = { ress->srcBuffer, ress->srcBufferLoaded, 0 };
        ZSTD_outBuffer outBuff= { ress->dstBuffer, ress->dstBufferSize, 0 };
        size_t const readSizeHint = ZSTD_decompressStream(ress->dctx, &outBuff, &inBuff);
        if (ZSTD_isError(readSizeHint)) {
            DISPLAYLEVEL(1, "%s : Decoding error (36) : %s \n",
                            srcFileName, ZSTD_getErrorName(readSizeHint));
            FIO_zstdErrorHelp(ress, readSizeHint, srcFileName);
            return FIO_ERROR_FRAME_DECODING;
        }

        /* Write block */
        storedSkips = FIO_fwriteSparse(ress->dstFile, ress->dstBuffer, outBuff.pos, storedSkips);
        frameSize += outBuff.pos;
        DISPLAYUPDATE(2, "\r%-20.20s : %u MB...     ",
                         srcFileName, (U32)((alreadyDecoded+frameSize)>>20) );

        if (inBuff.pos > 0) {
            memmove(ress->srcBuffer, (char*)ress->srcBuffer + inBuff.pos, inBuff.size - inBuff.pos);
            ress->srcBufferLoaded -= inBuff.pos;
        }

        if (readSizeHint == 0) break;   /* end of frame */
        if (inBuff.size != inBuff.pos) {
            DISPLAYLEVEL(1, "%s : Decoding error (37) : should consume entire input \n",
                            srcFileName);
            return FIO_ERROR_FRAME_DECODING;
        }

        /* Fill input buffer */
        {   size_t const toDecode = MIN(readSizeHint, ress->srcBufferSize);  /* support large skippable frames */
            if (ress->srcBufferLoaded < toDecode) {
                size_t const toRead = toDecode - ress->srcBufferLoaded;   /* > 0 */
                void* const startPosition = (char*)ress->srcBuffer + ress->srcBufferLoaded;
                size_t const readSize = fread(startPosition, 1, toRead, finput);
                if (readSize==0) {
                    DISPLAYLEVEL(1, "%s : Read error (39) : premature end \n",
                                    srcFileName);
                    return FIO_ERROR_FRAME_DECODING;
                }
                ress->srcBufferLoaded += readSize;
    }   }   }

    FIO_fwriteSparseEnd(ress->dstFile, storedSkips);

    return frameSize;
}


#ifdef ZSTD_GZDECOMPRESS
static unsigned long long FIO_decompressGzFrame(dRess_t* ress,
                                    FILE* srcFile, const char* srcFileName)
{
    unsigned long long outFileSize = 0;
    z_stream strm;
    int flush = Z_NO_FLUSH;
    int decodingError = 0;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in = 0;
    strm.avail_in = 0;
    /* see http://www.zlib.net/manual.html */
    if (inflateInit2(&strm, 15 /* maxWindowLogSize */ + 16 /* gzip only */) != Z_OK)
        return FIO_ERROR_FRAME_DECODING;

    strm.next_out = (Bytef*)ress->dstBuffer;
    strm.avail_out = (uInt)ress->dstBufferSize;
    strm.avail_in = (uInt)ress->srcBufferLoaded;
    strm.next_in = (z_const unsigned char*)ress->srcBuffer;

    for ( ; ; ) {
        int ret;
        if (strm.avail_in == 0) {
            ress->srcBufferLoaded = fread(ress->srcBuffer, 1, ress->srcBufferSize, srcFile);
            if (ress->srcBufferLoaded == 0) flush = Z_FINISH;
            strm.next_in = (z_const unsigned char*)ress->srcBuffer;
            strm.avail_in = (uInt)ress->srcBufferLoaded;
        }
        ret = inflate(&strm, flush);
        if (ret == Z_BUF_ERROR) {
            DISPLAYLEVEL(1, "zstd: %s: premature gz end \n", srcFileName);
            decodingError = 1; break;
        }
        if (ret != Z_OK && ret != Z_STREAM_END) {
            DISPLAYLEVEL(1, "zstd: %s: inflate error %d \n", srcFileName, ret);
            decodingError = 1; break;
        }
        {   size_t const decompBytes = ress->dstBufferSize - strm.avail_out;
            if (decompBytes) {
                if (fwrite(ress->dstBuffer, 1, decompBytes, ress->dstFile) != decompBytes) {
                    DISPLAYLEVEL(1, "zstd: %s \n", strerror(errno));
                    decodingError = 1; break;
                }
                outFileSize += decompBytes;
                strm.next_out = (Bytef*)ress->dstBuffer;
                strm.avail_out = (uInt)ress->dstBufferSize;
            }
        }
        if (ret == Z_STREAM_END) break;
    }

    if (strm.avail_in > 0)
        memmove(ress->srcBuffer, strm.next_in, strm.avail_in);
    ress->srcBufferLoaded = strm.avail_in;
    if ( (inflateEnd(&strm) != Z_OK)  /* release resources ; error detected */
      && (decodingError==0) ) {
        DISPLAYLEVEL(1, "zstd: %s: inflateEnd error \n", srcFileName);
        decodingError = 1;
    }
    return decodingError ? FIO_ERROR_FRAME_DECODING : outFileSize;
}
#endif


#ifdef ZSTD_LZMADECOMPRESS
static unsigned long long FIO_decompressLzmaFrame(dRess_t* ress, FILE* srcFile, const char* srcFileName, int plain_lzma)
{
    unsigned long long outFileSize = 0;
    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_action action = LZMA_RUN;
    lzma_ret initRet;
    int decodingError = 0;

    strm.next_in = 0;
    strm.avail_in = 0;
    if (plain_lzma) {
        initRet = lzma_alone_decoder(&strm, UINT64_MAX); /* LZMA */
    } else {
        initRet = lzma_stream_decoder(&strm, UINT64_MAX, 0); /* XZ */
    }

    if (initRet != LZMA_OK) {
        DISPLAYLEVEL(1, "zstd: %s: %s error %d \n",
                        plain_lzma ? "lzma_alone_decoder" : "lzma_stream_decoder",
                        srcFileName, initRet);
        return FIO_ERROR_FRAME_DECODING;
    }

    strm.next_out = (BYTE*)ress->dstBuffer;
    strm.avail_out = ress->dstBufferSize;
    strm.next_in = (BYTE const*)ress->srcBuffer;
    strm.avail_in = ress->srcBufferLoaded;

    for ( ; ; ) {
        lzma_ret ret;
        if (strm.avail_in == 0) {
            ress->srcBufferLoaded = fread(ress->srcBuffer, 1, ress->srcBufferSize, srcFile);
            if (ress->srcBufferLoaded == 0) action = LZMA_FINISH;
            strm.next_in = (BYTE const*)ress->srcBuffer;
            strm.avail_in = ress->srcBufferLoaded;
        }
        ret = lzma_code(&strm, action);

        if (ret == LZMA_BUF_ERROR) {
            DISPLAYLEVEL(1, "zstd: %s: premature lzma end \n", srcFileName);
            decodingError = 1; break;
        }
        if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
            DISPLAYLEVEL(1, "zstd: %s: lzma_code decoding error %d \n",
                            srcFileName, ret);
            decodingError = 1; break;
        }
        {   size_t const decompBytes = ress->dstBufferSize - strm.avail_out;
            if (decompBytes) {
                if (fwrite(ress->dstBuffer, 1, decompBytes, ress->dstFile) != decompBytes) {
                    DISPLAYLEVEL(1, "zstd: %s \n", strerror(errno));
                    decodingError = 1; break;
                }
                outFileSize += decompBytes;
                strm.next_out = (BYTE*)ress->dstBuffer;
                strm.avail_out = ress->dstBufferSize;
        }   }
        if (ret == LZMA_STREAM_END) break;
    }

    if (strm.avail_in > 0)
        memmove(ress->srcBuffer, strm.next_in, strm.avail_in);
    ress->srcBufferLoaded = strm.avail_in;
    lzma_end(&strm);
    return decodingError ? FIO_ERROR_FRAME_DECODING : outFileSize;
}
#endif

#ifdef ZSTD_LZ4DECOMPRESS
static unsigned long long FIO_decompressLz4Frame(dRess_t* ress,
                                    FILE* srcFile, const char* srcFileName)
{
    unsigned long long filesize = 0;
    LZ4F_errorCode_t nextToLoad;
    LZ4F_decompressionContext_t dCtx;
    LZ4F_errorCode_t const errorCode = LZ4F_createDecompressionContext(&dCtx, LZ4F_VERSION);
    int decodingError = 0;

    if (LZ4F_isError(errorCode)) {
        DISPLAYLEVEL(1, "zstd: failed to create lz4 decompression context \n");
        return FIO_ERROR_FRAME_DECODING;
    }

    /* Init feed with magic number (already consumed from FILE* sFile) */
    {   size_t inSize = 4;
        size_t outSize= 0;
        MEM_writeLE32(ress->srcBuffer, LZ4_MAGICNUMBER);
        nextToLoad = LZ4F_decompress(dCtx, ress->dstBuffer, &outSize, ress->srcBuffer, &inSize, NULL);
        if (LZ4F_isError(nextToLoad)) {
            DISPLAYLEVEL(1, "zstd: %s: lz4 header error : %s \n",
                            srcFileName, LZ4F_getErrorName(nextToLoad));
            LZ4F_freeDecompressionContext(dCtx);
            return FIO_ERROR_FRAME_DECODING;
    }   }

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
            if (LZ4F_isError(nextToLoad)) {
                DISPLAYLEVEL(1, "zstd: %s: lz4 decompression error : %s \n",
                                srcFileName, LZ4F_getErrorName(nextToLoad));
                decodingError = 1; break;
            }
            pos += remaining;

            /* Write Block */
            if (decodedBytes) {
                if (fwrite(ress->dstBuffer, 1, decodedBytes, ress->dstFile) != decodedBytes) {
                    DISPLAYLEVEL(1, "zstd: %s \n", strerror(errno));
                    decodingError = 1; break;
                }
                filesize += decodedBytes;
                DISPLAYUPDATE(2, "\rDecompressed : %u MB  ", (unsigned)(filesize>>20));
            }

            if (!nextToLoad) break;
        }
    }
    /* can be out because readSize == 0, which could be an fread() error */
    if (ferror(srcFile)) {
        DISPLAYLEVEL(1, "zstd: %s: read error \n", srcFileName);
        decodingError=1;
    }

    if (nextToLoad!=0) {
        DISPLAYLEVEL(1, "zstd: %s: unfinished lz4 stream \n", srcFileName);
        decodingError=1;
    }

    LZ4F_freeDecompressionContext(dCtx);
    ress->srcBufferLoaded = 0; /* LZ4F will reach exact frame boundary */

    return decodingError ? FIO_ERROR_FRAME_DECODING : filesize;
}
#endif



/** FIO_decompressFrames() :
 *  Find and decode frames inside srcFile
 *  srcFile presumed opened and valid
 * @return : 0 : OK
 *           1 : error
 */
static int FIO_decompressFrames(dRess_t ress, FILE* srcFile,
                        const char* dstFileName, const char* srcFileName)
{
    unsigned readSomething = 0;
    unsigned long long filesize = 0;
    assert(srcFile != NULL);

    /* for each frame */
    for ( ; ; ) {
        /* check magic number -> version */
        size_t const toRead = 4;
        const BYTE* const buf = (const BYTE*)ress.srcBuffer;
        if (ress.srcBufferLoaded < toRead)  /* load up to 4 bytes for header */
            ress.srcBufferLoaded += fread((char*)ress.srcBuffer + ress.srcBufferLoaded,
                                          (size_t)1, toRead - ress.srcBufferLoaded, srcFile);
        if (ress.srcBufferLoaded==0) {
            if (readSomething==0) {  /* srcFile is empty (which is invalid) */
                DISPLAYLEVEL(1, "zstd: %s: unexpected end of file \n", srcFileName);
                return 1;
            }  /* else, just reached frame boundary */
            break;   /* no more input */
        }
        readSomething = 1;   /* there is at least 1 byte in srcFile */
        if (ress.srcBufferLoaded < toRead) {
            DISPLAYLEVEL(1, "zstd: %s: unknown header \n", srcFileName);
            return 1;
        }
        if (ZSTD_isFrame(buf, ress.srcBufferLoaded)) {
            unsigned long long const frameSize = FIO_decompressZstdFrame(&ress, srcFile, srcFileName, filesize);
            if (frameSize == FIO_ERROR_FRAME_DECODING) return 1;
            filesize += frameSize;
        } else if (buf[0] == 31 && buf[1] == 139) { /* gz magic number */
#ifdef ZSTD_GZDECOMPRESS
            unsigned long long const frameSize = FIO_decompressGzFrame(&ress, srcFile, srcFileName);
            if (frameSize == FIO_ERROR_FRAME_DECODING) return 1;
            filesize += frameSize;
#else
            DISPLAYLEVEL(1, "zstd: %s: gzip file cannot be uncompressed (zstd compiled without HAVE_ZLIB) -- ignored \n", srcFileName);
            return 1;
#endif
        } else if ((buf[0] == 0xFD && buf[1] == 0x37)  /* xz magic number */
                || (buf[0] == 0x5D && buf[1] == 0x00)) { /* lzma header (no magic number) */
#ifdef ZSTD_LZMADECOMPRESS
            unsigned long long const frameSize = FIO_decompressLzmaFrame(&ress, srcFile, srcFileName, buf[0] != 0xFD);
            if (frameSize == FIO_ERROR_FRAME_DECODING) return 1;
            filesize += frameSize;
#else
            DISPLAYLEVEL(1, "zstd: %s: xz/lzma file cannot be uncompressed (zstd compiled without HAVE_LZMA) -- ignored \n", srcFileName);
            return 1;
#endif
        } else if (MEM_readLE32(buf) == LZ4_MAGICNUMBER) {
#ifdef ZSTD_LZ4DECOMPRESS
            unsigned long long const frameSize = FIO_decompressLz4Frame(&ress, srcFile, srcFileName);
            if (frameSize == FIO_ERROR_FRAME_DECODING) return 1;
            filesize += frameSize;
#else
            DISPLAYLEVEL(1, "zstd: %s: lz4 file cannot be uncompressed (zstd compiled without HAVE_LZ4) -- ignored \n", srcFileName);
            return 1;
#endif
        } else if ((g_overwrite) && !strcmp (dstFileName, stdoutmark)) {  /* pass-through mode */
            return FIO_passThrough(ress.dstFile, srcFile,
                                   ress.srcBuffer, ress.srcBufferSize, ress.srcBufferLoaded);
        } else {
            DISPLAYLEVEL(1, "zstd: %s: unsupported format \n", srcFileName);
            return 1;
    }   }  /* for each frame */

    /* Final Status */
    DISPLAYLEVEL(2, "\r%79s\r", "");
    DISPLAYLEVEL(2, "%-20s: %llu bytes \n", srcFileName, filesize);

    return 0;
}


/** FIO_decompressSrcFile() :
    Decompression `srcFileName` into `ress.dstFile`
    @return : 0 : OK
              1 : operation not started
*/
static int FIO_decompressSrcFile(dRess_t ress, const char* dstFileName, const char* srcFileName)
{
    FILE* srcFile;
    int result;

    if (UTIL_isDirectory(srcFileName)) {
        DISPLAYLEVEL(1, "zstd: %s is a directory -- ignored \n", srcFileName);
        return 1;
    }

    srcFile = FIO_openSrcFile(srcFileName);
    if (srcFile==NULL) return 1;

    result = FIO_decompressFrames(ress, srcFile, dstFileName, srcFileName);

    /* Close file */
    if (fclose(srcFile)) {
        DISPLAYLEVEL(1, "zstd: %s: %s \n", srcFileName, strerror(errno));  /* error should not happen */
        return 1;
    }
    if ( g_removeSrcFile /* --rm */
      && (result==0)     /* decompression successful */
      && strcmp(srcFileName, stdinmark) ) /* not stdin */ {
        /* We must clear the handler, since after this point calling it would
         * delete both the source and destination files.
         */
        clearHandler();
        if (remove(srcFileName)) {
            /* failed to remove src file */
            DISPLAYLEVEL(1, "zstd: %s: %s \n", srcFileName, strerror(errno));
            return 1;
    }   }
    return result;
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
    /* Must ony be added after FIO_openDstFile() succeeds.
     * Otherwise we may delete the destination file if at already exists, and
     * the user presses Ctrl-C when asked if they wish to overwrite.
     */
    addHandler(dstFileName);

    if ( strcmp(srcFileName, stdinmark)
      && UTIL_getFileStat(srcFileName, &statbuf) )
        stat_result = 1;
    result = FIO_decompressSrcFile(ress, dstFileName, srcFileName);
    clearHandler();

    if (fclose(ress.dstFile)) {
        DISPLAYLEVEL(1, "zstd: %s: %s \n", dstFileName, strerror(errno));
        result = 1;
    }

    if ( (result != 0)  /* operation failure */
      && strcmp(dstFileName, nulmark)      /* special case : don't remove() /dev/null (#316) */
      && strcmp(dstFileName, stdoutmark) ) /* special case : don't remove() stdout */
        remove(dstFileName);  /* remove decompression artefact; note don't do anything special if remove() fails */
    else {  /* operation success */
        if ( strcmp(dstFileName, stdoutmark) /* special case : don't chmod stdout */
          && strcmp(dstFileName, nulmark)    /* special case : don't chmod /dev/null */
          && stat_result )                   /* file permissions correctly extracted from src */
            UTIL_setFileStat(dstFileName, &statbuf);  /* transfer file permissions from src into dst */
    }

    signal(SIGINT, SIG_DFL);

    return result;
}


int FIO_decompressFilename(const char* dstFileName, const char* srcFileName,
                           const char* dictFileName)
{
    dRess_t const ress = FIO_createDResources(dictFileName);

    int const decodingError = FIO_decompressDstFile(ress, dstFileName, srcFileName);

    FIO_freeDResources(ress);
    return decodingError;
}


#define MAXSUFFIXSIZE 8
int FIO_decompressMultipleFilenames(const char** srcNamesTable, unsigned nbFiles,
                                    const char* outFileName,
                                    const char* dictFileName)
{
    int skippedFiles = 0;
    int missingFiles = 0;
    dRess_t ress = FIO_createDResources(dictFileName);

    if (outFileName) {
        unsigned u;
        ress.dstFile = FIO_openDstFile(outFileName);
        if (ress.dstFile == 0) EXM_THROW(71, "cannot open %s", outFileName);
        for (u=0; u<nbFiles; u++)
            missingFiles += FIO_decompressSrcFile(ress, outFileName, srcNamesTable[u]);
        if (fclose(ress.dstFile))
            EXM_THROW(72, "Write error : cannot properly close output file");
    } else {
        size_t suffixSize;
        size_t dfnSize = FNSPACE;
        unsigned u;
        char* dstFileName = (char*)malloc(FNSPACE);
        if (dstFileName==NULL)
            EXM_THROW(73, "not enough memory for dstFileName");
        for (u=0; u<nbFiles; u++) {   /* create dstFileName */
            const char* const srcFileName = srcNamesTable[u];
            const char* const suffixPtr = strrchr(srcFileName, '.');
            size_t const sfnSize = strlen(srcFileName);
            if (!suffixPtr) {
                DISPLAYLEVEL(1, "zstd: %s: unknown suffix -- ignored \n",
                                srcFileName);
                skippedFiles++;
                continue;
            }
            suffixSize = strlen(suffixPtr);
            if (dfnSize+suffixSize <= sfnSize+1) {
                free(dstFileName);
                dfnSize = sfnSize + 20;
                dstFileName = (char*)malloc(dfnSize);
                if (dstFileName==NULL)
                    EXM_THROW(74, "not enough memory for dstFileName");
            }
            if (sfnSize <= suffixSize
                || (strcmp(suffixPtr, GZ_EXTENSION)
                    && strcmp(suffixPtr, XZ_EXTENSION)
                    && strcmp(suffixPtr, ZSTD_EXTENSION)
                    && strcmp(suffixPtr, LZMA_EXTENSION)
                    && strcmp(suffixPtr, LZ4_EXTENSION)) ) {
                DISPLAYLEVEL(1, "zstd: %s: unknown suffix (%s/%s/%s/%s/%s expected) -- ignored \n",
                             srcFileName, GZ_EXTENSION, XZ_EXTENSION, ZSTD_EXTENSION, LZMA_EXTENSION, LZ4_EXTENSION);
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



/* **************************************************************************
 *  .zst file info (--list command)
 ***************************************************************************/

typedef struct {
    U64 decompressedSize;
    U64 compressedSize;
    U64 windowSize;
    int numActualFrames;
    int numSkippableFrames;
    int decompUnavailable;
    int usesCheck;
    U32 nbFiles;
} fileInfo_t;

/** getFileInfo() :
 *  Reads information from file, stores in *info
 * @return : 0 if successful
 *           1 for frame analysis error
 *           2 for file not compressed with zstd
 *           3 for cases in which file could not be opened.
 */
static int getFileInfo_fileConfirmed(fileInfo_t* info, const char* inFileName){
    int detectError = 0;
    FILE* const srcFile = FIO_openSrcFile(inFileName);
    if (srcFile == NULL) {
        DISPLAY("Error: could not open source file %s\n", inFileName);
        return 3;
    }
    info->compressedSize = UTIL_getFileSize(inFileName);

    /* begin analyzing frame */
    for ( ; ; ) {
        BYTE headerBuffer[ZSTD_FRAMEHEADERSIZE_MAX];
        size_t const numBytesRead = fread(headerBuffer, 1, sizeof(headerBuffer), srcFile);
        if (numBytesRead < ZSTD_frameHeaderSize_min) {
            if ( feof(srcFile)
              && (numBytesRead == 0)
              && (info->compressedSize > 0)
              && (info->compressedSize != UTIL_FILESIZE_UNKNOWN) ) {
                break;
            }
            else if (feof(srcFile)) {
                DISPLAY("Error: reached end of file with incomplete frame\n");
                detectError = 2;
                break;
            }
            else {
                DISPLAY("Error: did not reach end of file but ran out of frames\n");
                detectError = 1;
                break;
            }
        }
        {   U32 const magicNumber = MEM_readLE32(headerBuffer);
            /* Zstandard frame */
            if (magicNumber == ZSTD_MAGICNUMBER) {
                ZSTD_frameHeader header;
                U64 const frameContentSize = ZSTD_getFrameContentSize(headerBuffer, numBytesRead);
                if (frameContentSize == ZSTD_CONTENTSIZE_ERROR || frameContentSize == ZSTD_CONTENTSIZE_UNKNOWN) {
                    info->decompUnavailable = 1;
                } else {
                    info->decompressedSize += frameContentSize;
                }
                if (ZSTD_getFrameHeader(&header, headerBuffer, numBytesRead) != 0) {
                    DISPLAY("Error: could not decode frame header\n");
                    detectError = 1;
                    break;
                }
                info->windowSize = header.windowSize;
                /* move to the end of the frame header */
                {   size_t const headerSize = ZSTD_frameHeaderSize(headerBuffer, numBytesRead);
                    if (ZSTD_isError(headerSize)) {
                        DISPLAY("Error: could not determine frame header size\n");
                        detectError = 1;
                        break;
                    }
                    {   int const ret = fseek(srcFile, ((long)headerSize)-((long)numBytesRead), SEEK_CUR);
                        if (ret != 0) {
                            DISPLAY("Error: could not move to end of frame header\n");
                            detectError = 1;
                            break;
                }   }   }

                /* skip the rest of the blocks in the frame */
                {   int lastBlock = 0;
                    do {
                        BYTE blockHeaderBuffer[3];
                        size_t const readBytes = fread(blockHeaderBuffer, 1, 3, srcFile);
                        if (readBytes != 3) {
                            DISPLAY("There was a problem reading the block header\n");
                            detectError = 1;
                            break;
                        }
                        {   U32 const blockHeader = MEM_readLE24(blockHeaderBuffer);
                            U32 const blockTypeID = (blockHeader >> 1) & 3;
                            U32 const isRLE = (blockTypeID == 1);
                            U32 const isWrongBlock = (blockTypeID == 3);
                            long const blockSize = isRLE ? 1 : (long)(blockHeader >> 3);
                            if (isWrongBlock) {
                                DISPLAY("Error: unsupported block type \n");
                                detectError = 1;
                                break;
                            }
                            lastBlock = blockHeader & 1;
                            {   int const ret = fseek(srcFile, blockSize, SEEK_CUR);
                                if (ret != 0) {
                                    DISPLAY("Error: could not skip to end of block\n");
                                    detectError = 1;
                                    break;
                        }   }   }
                    } while (lastBlock != 1);

                    if (detectError) break;
                }

                /* check if checksum is used */
                {   BYTE const frameHeaderDescriptor = headerBuffer[4];
                    int const contentChecksumFlag = (frameHeaderDescriptor & (1 << 2)) >> 2;
                    if (contentChecksumFlag) {
                        int const ret = fseek(srcFile, 4, SEEK_CUR);
                        info->usesCheck = 1;
                        if (ret != 0) {
                            DISPLAY("Error: could not skip past checksum\n");
                            detectError = 1;
                            break;
                }   }   }
                info->numActualFrames++;
            }
            /* Skippable frame */
            else if ((magicNumber & 0xFFFFFFF0U) == ZSTD_MAGIC_SKIPPABLE_START) {
                U32 const frameSize = MEM_readLE32(headerBuffer + 4);
                long const seek = (long)(8 + frameSize - numBytesRead);
                int const ret = LONG_SEEK(srcFile, seek, SEEK_CUR);
                if (ret != 0) {
                    DISPLAY("Error: could not find end of skippable frame\n");
                    detectError = 1;
                    break;
                }
                info->numSkippableFrames++;
            }
            /* unknown content */
            else {
                detectError = 2;
                break;
            }
        }
    }  /* end analyzing frame */
    fclose(srcFile);
    info->nbFiles = 1;
    return detectError;
}

static int getFileInfo(fileInfo_t* info, const char* srcFileName)
{
    int const isAFile = UTIL_isRegularFile(srcFileName);
    if (!isAFile) {
        DISPLAY("Error : %s is not a file", srcFileName);
        return 3;
    }
    return getFileInfo_fileConfirmed(info, srcFileName);
}


static void displayInfo(const char* inFileName, const fileInfo_t* info, int displayLevel){
    unsigned const unit = info->compressedSize < (1 MB) ? (1 KB) : (1 MB);
    const char* const unitStr = info->compressedSize < (1 MB) ? "KB" : "MB";
    double const windowSizeUnit = (double)info->windowSize / unit;
    double const compressedSizeUnit = (double)info->compressedSize / unit;
    double const decompressedSizeUnit = (double)info->decompressedSize / unit;
    double const ratio = (info->compressedSize == 0) ? 0 : ((double)info->decompressedSize)/info->compressedSize;
    const char* const checkString = (info->usesCheck ? "XXH64" : "None");
    if (displayLevel <= 2) {
        if (!info->decompUnavailable) {
            DISPLAYOUT("%6d  %5d  %7.2f %2s  %9.2f %2s  %5.3f  %5s  %s\n",
                    info->numSkippableFrames + info->numActualFrames,
                    info->numSkippableFrames,
                    compressedSizeUnit, unitStr, decompressedSizeUnit, unitStr,
                    ratio, checkString, inFileName);
        } else {
            DISPLAYOUT("%6d  %5d  %7.2f %2s                       %5s  %s\n",
                    info->numSkippableFrames + info->numActualFrames,
                    info->numSkippableFrames,
                    compressedSizeUnit, unitStr,
                    checkString, inFileName);
        }
    } else {
        DISPLAYOUT("%s \n", inFileName);
        DISPLAYOUT("# Zstandard Frames: %d\n", info->numActualFrames);
        if (info->numSkippableFrames)
            DISPLAYOUT("# Skippable Frames: %d\n", info->numSkippableFrames);
        DISPLAYOUT("Window Size: %.2f %2s (%llu B)\n",
                   windowSizeUnit, unitStr,
                   (unsigned long long)info->windowSize);
        DISPLAYOUT("Compressed Size: %.2f %2s (%llu B)\n",
                    compressedSizeUnit, unitStr,
                    (unsigned long long)info->compressedSize);
        if (!info->decompUnavailable) {
            DISPLAYOUT("Decompressed Size: %.2f %2s (%llu B)\n",
                    decompressedSizeUnit, unitStr,
                    (unsigned long long)info->decompressedSize);
            DISPLAYOUT("Ratio: %.4f\n", ratio);
        }
        DISPLAYOUT("Check: %s\n", checkString);
        DISPLAYOUT("\n");
    }
}

static fileInfo_t FIO_addFInfo(fileInfo_t fi1, fileInfo_t fi2)
{
    fileInfo_t total;
    total.numActualFrames = fi1.numActualFrames + fi2.numActualFrames;
    total.numSkippableFrames = fi1.numSkippableFrames + fi2.numSkippableFrames;
    total.compressedSize = fi1.compressedSize + fi2.compressedSize;
    total.decompressedSize = fi1.decompressedSize + fi2.decompressedSize;
    total.decompUnavailable = fi1.decompUnavailable | fi2.decompUnavailable;
    total.usesCheck = fi1.usesCheck & fi2.usesCheck;
    total.nbFiles = fi1.nbFiles + fi2.nbFiles;
    return total;
}

static int FIO_listFile(fileInfo_t* total, const char* inFileName, int displayLevel){
    fileInfo_t info;
    memset(&info, 0, sizeof(info));
    {   int const error = getFileInfo(&info, inFileName);
        if (error == 1) {
            /* display error, but provide output */
            DISPLAY("An error occurred while getting file info \n");
        }
        else if (error == 2) {
            DISPLAYOUT("File %s not compressed by zstd \n", inFileName);
            if (displayLevel > 2) DISPLAYOUT("\n");
            return 1;
        }
        else if (error == 3) {
            /* error occurred while opening the file */
            if (displayLevel > 2) DISPLAYOUT("\n");
            return 1;
        }
        displayInfo(inFileName, &info, displayLevel);
        *total = FIO_addFInfo(*total, info);
        return error;
    }
}

int FIO_listMultipleFiles(unsigned numFiles, const char** filenameTable, int displayLevel){
    if (numFiles == 0) {
        DISPLAYOUT("No files given\n");
        return 0;
    }
    if (displayLevel <= 2) {
        DISPLAYOUT("Frames  Skips  Compressed  Uncompressed  Ratio  Check  Filename\n");
    }
    {   int error = 0;
        unsigned u;
        fileInfo_t total;
        memset(&total, 0, sizeof(total));
        total.usesCheck = 1;
        for (u=0; u<numFiles;u++) {
            error |= FIO_listFile(&total, filenameTable[u], displayLevel);
        }
        if (numFiles > 1 && displayLevel <= 2) {   /* display total */
            unsigned const unit = total.compressedSize < (1 MB) ? (1 KB) : (1 MB);
            const char* const unitStr = total.compressedSize < (1 MB) ? "KB" : "MB";
            double const compressedSizeUnit = (double)total.compressedSize / unit;
            double const decompressedSizeUnit = (double)total.decompressedSize / unit;
            double const ratio = (total.compressedSize == 0) ? 0 : ((double)total.decompressedSize)/total.compressedSize;
            const char* const checkString = (total.usesCheck ? "XXH64" : "");
            DISPLAYOUT("----------------------------------------------------------------- \n");
            if (total.decompUnavailable) {
                DISPLAYOUT("%6d  %5d  %7.2f %2s                       %5s  %u files\n",
                        total.numSkippableFrames + total.numActualFrames,
                        total.numSkippableFrames,
                        compressedSizeUnit, unitStr,
                        checkString, total.nbFiles);
            } else {
                DISPLAYOUT("%6d  %5d  %7.2f %2s  %9.2f %2s  %5.3f  %5s  %u files\n",
                        total.numSkippableFrames + total.numActualFrames,
                        total.numSkippableFrames,
                        compressedSizeUnit, unitStr, decompressedSizeUnit, unitStr,
                        ratio, checkString, total.nbFiles);
        }   }
        return error;
    }
}


#endif /* #ifndef ZSTD_NODECOMPRESS */
