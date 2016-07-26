/*
  fileio_legacy.c - File i/o handler for legacy format
  Copyright (C) Yann Collet 2015-2016

  GPL v2 License

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

  You can contact the author at :
  - zstd homepage : http://www.zstd.net
  - zstd source repository : https://github.com/Cyan4973/zstd
*/
/*
  Note : this file is not part of ZSTD compression library.
  The license of ZSTD library is BSD.
  The license of this file is GPLv2.
*/

/* *************************************
*  Compiler Options
***************************************/
/* Disable some Visual warning messages */
#ifdef _MSC_VER
#  define _CRT_SECURE_NO_WARNINGS
#  define _CRT_SECURE_NO_DEPRECATE     /* VS2005 */
#  pragma warning(disable : 4127)      /* disable: C4127: conditional expression is constant */
#endif

#define _FILE_OFFSET_BITS 64   /* Large file support on 32-bits unix */
#define _POSIX_SOURCE 1        /* enable fileno() within <stdio.h> on unix */


/* *************************************
*  Includes
***************************************/
#include <stdio.h>     /* fprintf, fopen, fread, _fileno, stdin, stdout */
#include <stdlib.h>    /* malloc, free */
#include <string.h>    /* strcmp, strlen */
#include <time.h>      /* clock */
#include <errno.h>     /* errno */
#include "mem.h"
#include "fileio_legacy.h"
#include "zstd_legacy.h"  /* legacy support */


/* *************************************
*  OS-specific Includes
***************************************/
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(_WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>    /* _O_BINARY */
#  include <io.h>       /* _setmode, _isatty */
#  ifdef __MINGW32__
   /* int _fileno(FILE *stream);   // seems no longer useful // MINGW somehow forgets to include this windows declaration into <stdio.h> */
#  endif
#  define SET_BINARY_MODE(file) { int unused = _setmode(_fileno(file), _O_BINARY); (void)unused; }
#  define IS_CONSOLE(stdStream) _isatty(_fileno(stdStream))
#else
#  include <unistd.h>   /* isatty */
#  define SET_BINARY_MODE(file)
#  define IS_CONSOLE(stdStream) isatty(fileno(stdStream))
#endif


/* *************************************
*  Constants
***************************************/
#define KB *(1U<<10)
#define MB *(1U<<20)
#define GB *(1U<<30)

#define _1BIT  0x01
#define _2BITS 0x03
#define _3BITS 0x07
#define _4BITS 0x0F
#define _6BITS 0x3F
#define _8BITS 0xFF

#define BIT6  0x40
#define BIT7  0x80

#define FIO_FRAMEHEADERSIZE 5     /* as a define, because needed to allocated table on stack */
#define FSE_CHECKSUM_SEED   0

#define CACHELINE 64


/* *************************************
*  Macros
***************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); }
static U32 g_displayLevel = 1;   /* 0 : no display;   1: errors;   2 : + result + interaction + warnings;   3 : + progression;   4 : + information */

#define DISPLAYUPDATE(l, ...) if (g_displayLevel>=l) { \
            if ((FIO_GetMilliSpan(g_time) > refreshRate) || (g_displayLevel>=4)) \
            { g_time = clock(); DISPLAY(__VA_ARGS__); \
            if (g_displayLevel>=4) fflush(stdout); } }
static const unsigned refreshRate = 150;
static clock_t g_time = 0;


/* *************************************
*  Local Parameters
***************************************/
void FIO_legacy_setNotificationLevel(unsigned level) { g_displayLevel=level; }


/* *************************************
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
    DISPLAYLEVEL(1, "\n");                                                \
    exit(error);                                                          \
}


/* *************************************
*  Functions
***************************************/
static unsigned FIO_GetMilliSpan(clock_t nPrevious)
{
    clock_t nCurrent = clock();
    unsigned nSpan = (unsigned)(((nCurrent - nPrevious) * 1000) / CLOCKS_PER_SEC);
    return nSpan;
}


unsigned long long FIOv01_decompressFrame(FILE* foutput, FILE* finput)
{
    size_t const outBuffSize = 512 KB;
    BYTE* outBuff = (BYTE*)malloc(outBuffSize);
    size_t inBuffSize = 128 KB + 8;
    BYTE inBuff[128 KB + 8];
    BYTE* op = outBuff;
    BYTE* const oend = outBuff + outBuffSize;
    U64   filesize = 0;
    size_t toRead;
    size_t sizeCheck;
    ZSTDv01_Dctx* dctx = ZSTDv01_createDCtx();


    /* init */
    if (outBuff==NULL) EXM_THROW(41, "Error : not enough memory to decode legacy frame");

    /* restore header, already read from input */
    MEM_writeLE32(inBuff, ZSTDv01_magicNumberLE);
    sizeCheck = ZSTDv01_decompressContinue(dctx, NULL, 0, inBuff, sizeof(ZSTDv01_magicNumberLE));   /* Decode frame header */
    if (ZSTDv01_isError(sizeCheck)) EXM_THROW(42, "Error decoding legacy header");

    /* Main decompression Loop */
    toRead = ZSTDv01_nextSrcSizeToDecompress(dctx);
    while (toRead){
        size_t readSize, decodedSize;

        /* Fill input buffer */
        if (toRead > inBuffSize)
            EXM_THROW(43, "too large block");
        readSize = fread(inBuff, 1, toRead, finput);
        if (readSize != toRead)
            EXM_THROW(44, "Read error");

        /* Decode block */
        decodedSize = ZSTDv01_decompressContinue(dctx, op, oend-op, inBuff, readSize);
        if (ZSTDv01_isError(decodedSize)) EXM_THROW(45, "Decoding error : input corrupted");

        if (decodedSize) {  /* not a header */
            /* Write block */
            sizeCheck = fwrite(op, 1, decodedSize, foutput);
            if (sizeCheck != decodedSize) EXM_THROW(46, "Write error : unable to write data block to destination file");
            filesize += decodedSize;
            op += decodedSize;
            if (op==oend) op = outBuff;
            DISPLAYUPDATE(2, "\rDecoded : %u MB...     ", (U32)(filesize>>20) );
        }

        /* prepare for next Block */
        toRead = ZSTDv01_nextSrcSizeToDecompress(dctx);
    }

    /* release resources */
    free(outBuff);
    free(dctx);
    return filesize;
}


unsigned long long FIOv02_decompressFrame(FILE* foutput, FILE* finput)
{
    size_t const outBuffSize = 512 KB;
    BYTE* outBuff = (BYTE*)malloc(outBuffSize);
    size_t inBuffSize = 128 KB + 8;
    BYTE inBuff[128 KB + 8];
    BYTE* op = outBuff;
    BYTE* const oend = outBuff + outBuffSize;
    U64   filesize = 0;
    size_t toRead;
    size_t sizeCheck;
    ZSTDv02_Dctx* dctx = ZSTDv02_createDCtx();

    /* init */
    if (outBuff==NULL) EXM_THROW(41, "Error : not enough memory to decode legacy frame");

    /* restore header, already read from input */
    MEM_writeLE32(inBuff, ZSTDv02_magicNumber);
    sizeCheck = ZSTDv02_decompressContinue(dctx, NULL, 0, inBuff, sizeof(ZSTDv02_magicNumber));   /* Decode frame header */
    if (ZSTDv02_isError(sizeCheck)) EXM_THROW(42, "Error decoding legacy header");

    /* Main decompression Loop */
    toRead = ZSTDv02_nextSrcSizeToDecompress(dctx);
    while (toRead) {
        size_t readSize, decodedSize;

        /* Fill input buffer */
        if (toRead > inBuffSize)
            EXM_THROW(43, "too large block");
        readSize = fread(inBuff, 1, toRead, finput);
        if (readSize != toRead)
            EXM_THROW(44, "Read error");

        /* Decode block */
        decodedSize = ZSTDv02_decompressContinue(dctx, op, oend-op, inBuff, readSize);
        if (ZSTDv02_isError(decodedSize)) EXM_THROW(45, "Decoding error : input corrupted");

        if (decodedSize) {   /* not a header */
            /* Write block */
            sizeCheck = fwrite(op, 1, decodedSize, foutput);
            if (sizeCheck != decodedSize) EXM_THROW(46, "Write error : unable to write data block to destination file");
            filesize += decodedSize;
            op += decodedSize;
            if (op==oend) op = outBuff;
            DISPLAYUPDATE(2, "\rDecoded : %u MB...     ", (U32)(filesize>>20) );
        }

        /* prepare for next Block */
        toRead = ZSTDv02_nextSrcSizeToDecompress(dctx);
    }

    /* release resources */
    free(outBuff);
    free(dctx);
    return filesize;
}


unsigned long long FIOv03_decompressFrame(FILE* foutput, FILE* finput)
{
    size_t const outBuffSize = 512 KB;
    BYTE* outBuff = (BYTE*)malloc(outBuffSize);
    size_t inBuffSize = 128 KB + 8;
    BYTE inBuff[128 KB + 8];
    BYTE* op = outBuff;
    BYTE* const oend = outBuff + outBuffSize;
    U64   filesize = 0;
    size_t toRead;
    size_t sizeCheck;
    ZSTDv03_Dctx* dctx = ZSTDv03_createDCtx();

    /* init */
    if (outBuff==NULL) EXM_THROW(41, "Error : not enough memory to decode legacy frame");

    /* restore header, already read from input */
    MEM_writeLE32(inBuff, ZSTDv03_magicNumber);
    sizeCheck = ZSTDv03_decompressContinue(dctx, NULL, 0, inBuff, sizeof(ZSTDv03_magicNumber));   /* Decode frame header */
    if (ZSTDv03_isError(sizeCheck)) EXM_THROW(42, "Error decoding legacy header");

    /* Main decompression Loop */
    toRead = ZSTDv03_nextSrcSizeToDecompress(dctx);
    while (toRead) {
        size_t readSize, decodedSize;

        /* Fill input buffer */
        if (toRead > inBuffSize)
            EXM_THROW(43, "too large block");
        readSize = fread(inBuff, 1, toRead, finput);
        if (readSize != toRead)
            EXM_THROW(44, "Read error");

        /* Decode block */
        decodedSize = ZSTDv03_decompressContinue(dctx, op, oend-op, inBuff, readSize);
        if (ZSTDv03_isError(decodedSize)) EXM_THROW(45, "Decoding error : input corrupted");

        if (decodedSize) {   /* not a header */
            /* Write block */
            sizeCheck = fwrite(op, 1, decodedSize, foutput);
            if (sizeCheck != decodedSize) EXM_THROW(46, "Write error : unable to write data block to destination file");
            filesize += decodedSize;
            op += decodedSize;
            if (op==oend) op = outBuff;
            DISPLAYUPDATE(2, "\rDecoded : %u MB...     ", (U32)(filesize>>20) );
        }

        /* prepare for next Block */
        toRead = ZSTDv03_nextSrcSizeToDecompress(dctx);
    }

    /* release resources */
    free(outBuff);
    free(dctx);
    return filesize;
}


/*=====    v0.4.x    =====*/

typedef struct {
    void*  srcBuffer;
    size_t srcBufferSize;
    void*  dstBuffer;
    size_t dstBufferSize;
    void*  dictBuffer;
    size_t dictBufferSize;
    ZBUFFv04_DCtx* dctx;
} dRessv04_t;

static dRessv04_t FIOv04_createDResources(void)
{
    dRessv04_t ress;

    /* init */
    ress.dctx = ZBUFFv04_createDCtx();
    if (ress.dctx==NULL) EXM_THROW(60, "Can't create ZBUFF decompression context");
    ress.dictBuffer = NULL; ress.dictBufferSize=0;

    /* Allocate Memory */
    ress.srcBufferSize = ZBUFFv04_recommendedDInSize();
    ress.srcBuffer = malloc(ress.srcBufferSize);
    ress.dstBufferSize = ZBUFFv04_recommendedDOutSize();
    ress.dstBuffer = malloc(ress.dstBufferSize);
    if (!ress.srcBuffer || !ress.dstBuffer) EXM_THROW(61, "Allocation error : not enough memory");

    return ress;
}

static void FIOv04_freeDResources(dRessv04_t ress)
{
    size_t errorCode = ZBUFFv04_freeDCtx(ress.dctx);
    if (ZBUFFv04_isError(errorCode)) EXM_THROW(69, "Error : can't free ZBUFF context resource : %s", ZBUFFv04_getErrorName(errorCode));
    free(ress.srcBuffer);
    free(ress.dstBuffer);
    free(ress.dictBuffer);
}


unsigned long long FIOv04_decompressFrame(dRessv04_t ress,
                                          FILE* foutput, FILE* finput)
{
    U64    frameSize = 0;
    size_t readSize = 4;

    MEM_writeLE32(ress.srcBuffer, ZSTDv04_magicNumber);
    ZBUFFv04_decompressInit(ress.dctx);
    ZBUFFv04_decompressWithDictionary(ress.dctx, ress.dictBuffer, ress.dictBufferSize);

    while (1) {
        /* Decode */
        size_t sizeCheck;
        size_t inSize=readSize, decodedSize=ress.dstBufferSize;
        size_t toRead = ZBUFFv04_decompressContinue(ress.dctx, ress.dstBuffer, &decodedSize, ress.srcBuffer, &inSize);
        if (ZBUFFv04_isError(toRead)) EXM_THROW(36, "Decoding error : %s", ZBUFFv04_getErrorName(toRead));
        readSize -= inSize;

        /* Write block */
        sizeCheck = fwrite(ress.dstBuffer, 1, decodedSize, foutput);
        if (sizeCheck != decodedSize) EXM_THROW(37, "Write error : unable to write data block to destination file");
        frameSize += decodedSize;
        DISPLAYUPDATE(2, "\rDecoded : %u MB...     ", (U32)(frameSize>>20) );

        if (toRead == 0) break;
        if (readSize) EXM_THROW(38, "Decoding error : should consume entire input");

        /* Fill input buffer */
        if (toRead > ress.srcBufferSize) EXM_THROW(34, "too large block");
        readSize = fread(ress.srcBuffer, 1, toRead, finput);
        if (readSize != toRead) EXM_THROW(35, "Read error");
    }

    return frameSize;
}


/*=====    v0.5.x    =====*/

typedef struct {
    void*  srcBuffer;
    size_t srcBufferSize;
    void*  dstBuffer;
    size_t dstBufferSize;
    const void*  dictBuffer;
    size_t dictBufferSize;
    ZBUFFv05_DCtx* dctx;
} dRessv05_t;

static dRessv05_t FIOv05_createDResources(void)
{
    dRessv05_t ress;

    /* init */
    ress.dctx = ZBUFFv05_createDCtx();
    if (ress.dctx==NULL) EXM_THROW(60, "Can't create ZBUFF decompression context");
    ress.dictBuffer = NULL; ress.dictBufferSize=0;

    /* Allocate Memory */
    ress.srcBufferSize = ZBUFFv05_recommendedDInSize();
    ress.srcBuffer = malloc(ress.srcBufferSize);
    ress.dstBufferSize = ZBUFFv05_recommendedDOutSize();
    ress.dstBuffer = malloc(ress.dstBufferSize);
    if (!ress.srcBuffer || !ress.dstBuffer) EXM_THROW(61, "Allocation error : not enough memory");

    return ress;
}

static void FIOv05_freeDResources(dRessv05_t ress)
{
    size_t const errorCode = ZBUFFv05_freeDCtx(ress.dctx);
    if (ZBUFFv05_isError(errorCode)) EXM_THROW(69, "Error : can't free ZBUFF context resource : %s", ZBUFFv05_getErrorName(errorCode));
    free(ress.srcBuffer);
    free(ress.dstBuffer);
}


unsigned long long FIOv05_decompressFrame(dRessv05_t ress,
                                          FILE* foutput, FILE* finput)
{
    U64    frameSize = 0;
    size_t readSize  = 4;

    MEM_writeLE32(ress.srcBuffer, ZSTDv05_MAGICNUMBER);
    ZBUFFv05_decompressInitDictionary(ress.dctx, ress.dictBuffer, ress.dictBufferSize);

    while (1) {
        /* Decode */
        size_t inSize=readSize, decodedSize=ress.dstBufferSize;
        size_t toRead = ZBUFFv05_decompressContinue(ress.dctx, ress.dstBuffer, &decodedSize, ress.srcBuffer, &inSize);
        if (ZBUFFv05_isError(toRead)) EXM_THROW(36, "Decoding error : %s", ZBUFFv05_getErrorName(toRead));
        readSize -= inSize;

        /* Write block */
        { size_t const sizeCheck = fwrite(ress.dstBuffer, 1, decodedSize, foutput);
          if (sizeCheck != decodedSize) EXM_THROW(37, "Write error : unable to write data block to destination file"); }
        frameSize += decodedSize;
        DISPLAYUPDATE(2, "\rDecoded : %u MB...     ", (U32)(frameSize>>20) );

        if (toRead == 0) break;
        if (readSize) EXM_THROW(38, "Decoding error : should consume entire input");

        /* Fill input buffer */
        if (toRead > ress.srcBufferSize) EXM_THROW(34, "too large block");
        readSize = fread(ress.srcBuffer, 1, toRead, finput);
        if (readSize != toRead) EXM_THROW(35, "Read error");
    }

    return frameSize;
}


/*=====    v0.6.x    =====*/

typedef struct {
    void*  srcBuffer;
    size_t srcBufferSize;
    void*  dstBuffer;
    size_t dstBufferSize;
    const void*  dictBuffer;
    size_t dictBufferSize;
    ZBUFFv06_DCtx* dctx;
} dRessv06_t;

static dRessv06_t FIOv06_createDResources(void)
{
    dRessv06_t ress;

    /* init */
    ress.dctx = ZBUFFv06_createDCtx();
    if (ress.dctx==NULL) EXM_THROW(60, "Can't create ZBUFF decompression context");
    ress.dictBuffer = NULL; ress.dictBufferSize=0;

    /* Allocate Memory */
    ress.srcBufferSize = ZBUFFv06_recommendedDInSize();
    ress.srcBuffer = malloc(ress.srcBufferSize);
    ress.dstBufferSize = ZBUFFv06_recommendedDOutSize();
    ress.dstBuffer = malloc(ress.dstBufferSize);
    if (!ress.srcBuffer || !ress.dstBuffer) EXM_THROW(61, "Allocation error : not enough memory");

    return ress;
}

static void FIOv06_freeDResources(dRessv06_t ress)
{
    size_t const errorCode = ZBUFFv06_freeDCtx(ress.dctx);
    if (ZBUFFv06_isError(errorCode)) EXM_THROW(69, "Error : can't free ZBUFF context resource : %s", ZBUFFv06_getErrorName(errorCode));
    free(ress.srcBuffer);
    free(ress.dstBuffer);
}


unsigned long long FIOv06_decompressFrame(dRessv06_t ress,
                                          FILE* foutput, FILE* finput)
{
    U64    frameSize = 0;
    size_t readSize  = 4;

    MEM_writeLE32(ress.srcBuffer, ZSTDv06_MAGICNUMBER);
    ZBUFFv06_decompressInitDictionary(ress.dctx, ress.dictBuffer, ress.dictBufferSize);

    while (1) {
        /* Decode */
        size_t inSize=readSize, decodedSize=ress.dstBufferSize;
        size_t toRead = ZBUFFv06_decompressContinue(ress.dctx, ress.dstBuffer, &decodedSize, ress.srcBuffer, &inSize);
        if (ZBUFFv06_isError(toRead)) EXM_THROW(36, "Decoding error : %s", ZBUFFv06_getErrorName(toRead));
        readSize -= inSize;

        /* Write block */
        { size_t const sizeCheck = fwrite(ress.dstBuffer, 1, decodedSize, foutput);
          if (sizeCheck != decodedSize) EXM_THROW(37, "Write error : unable to write data block to destination file"); }
        frameSize += decodedSize;
        DISPLAYUPDATE(2, "\rDecoded : %u MB...     ", (U32)(frameSize>>20) );

        if (toRead == 0) break;
        if (readSize) EXM_THROW(38, "Decoding error : should consume entire input");

        /* Fill input buffer */
        if (toRead > ress.srcBufferSize) EXM_THROW(34, "too large block");
        readSize = fread(ress.srcBuffer, 1, toRead, finput);
        if (readSize != toRead) EXM_THROW(35, "Read error");
    }

    return frameSize;
}


/*=====    v0.7.x    =====*/

typedef struct {
    void*  srcBuffer;
    size_t srcBufferSize;
    void*  dstBuffer;
    size_t dstBufferSize;
    const void*  dictBuffer;
    size_t dictBufferSize;
    ZBUFFv07_DCtx* dctx;
} dRessv07_t;

static dRessv07_t FIOv07_createDResources(void)
{
    dRessv07_t ress;

    /* init */
    ress.dctx = ZBUFFv07_createDCtx();
    if (ress.dctx==NULL) EXM_THROW(60, "Can't create ZBUFF decompression context");
    ress.dictBuffer = NULL; ress.dictBufferSize=0;

    /* Allocate Memory */
    ress.srcBufferSize = ZBUFFv07_recommendedDInSize();
    ress.srcBuffer = malloc(ress.srcBufferSize);
    ress.dstBufferSize = ZBUFFv07_recommendedDOutSize();
    ress.dstBuffer = malloc(ress.dstBufferSize);
    if (!ress.srcBuffer || !ress.dstBuffer) EXM_THROW(61, "Allocation error : not enough memory");

    return ress;
}

static void FIOv07_freeDResources(dRessv07_t ress)
{
    size_t const errorCode = ZBUFFv07_freeDCtx(ress.dctx);
    if (ZBUFFv07_isError(errorCode)) EXM_THROW(69, "Error : can't free ZBUFF context resource : %s", ZBUFFv07_getErrorName(errorCode));
    free(ress.srcBuffer);
    free(ress.dstBuffer);
}


unsigned long long FIOv07_decompressFrame(dRessv07_t ress,
                                          FILE* foutput, FILE* finput)
{
    U64    frameSize = 0;
    size_t readSize  = 4;

    MEM_writeLE32(ress.srcBuffer, ZSTDv07_MAGICNUMBER);
    ZBUFFv07_decompressInitDictionary(ress.dctx, ress.dictBuffer, ress.dictBufferSize);

    while (1) {
        /* Decode */
        size_t inSize=readSize, decodedSize=ress.dstBufferSize;
        size_t toRead = ZBUFFv07_decompressContinue(ress.dctx, ress.dstBuffer, &decodedSize, ress.srcBuffer, &inSize);
        if (ZBUFFv07_isError(toRead)) EXM_THROW(36, "Decoding error : %s", ZBUFFv07_getErrorName(toRead));
        readSize -= inSize;

        /* Write block */
        { size_t const sizeCheck = fwrite(ress.dstBuffer, 1, decodedSize, foutput);
          if (sizeCheck != decodedSize) EXM_THROW(37, "Write error : unable to write data block to destination file"); }
        frameSize += decodedSize;
        DISPLAYUPDATE(2, "\rDecoded : %u MB...     ", (U32)(frameSize>>20) );

        if (toRead == 0) break;
        if (readSize) EXM_THROW(38, "Decoding error : should consume entire input");

        /* Fill input buffer */
        if (toRead > ress.srcBufferSize) EXM_THROW(34, "too large block");
        readSize = fread(ress.srcBuffer, 1, toRead, finput);
        if (readSize != toRead) EXM_THROW(35, "Read error");
    }

    return frameSize;
}


/*=====   General legacy dispatcher   =====*/

unsigned long long FIO_decompressLegacyFrame(FILE* foutput, FILE* finput,
                                             const void* dictBuffer, size_t dictSize,
                                             U32 magicNumberLE)
{
    switch(magicNumberLE)
    {
        case ZSTDv01_magicNumberLE :
            return FIOv01_decompressFrame(foutput, finput);
        case ZSTDv02_magicNumber :
            return FIOv02_decompressFrame(foutput, finput);
        case ZSTDv03_magicNumber :
            return FIOv03_decompressFrame(foutput, finput);
        case ZSTDv04_magicNumber :
            {   dRessv04_t r = FIOv04_createDResources();
                unsigned long long const s = FIOv04_decompressFrame(r, foutput, finput);
                FIOv04_freeDResources(r);
                return s;
            }
        case ZSTDv05_MAGICNUMBER :
            {   dRessv05_t r = FIOv05_createDResources();
                r.dictBuffer = dictBuffer;
                r.dictBufferSize = dictSize;
                {   unsigned long long const s = FIOv05_decompressFrame(r, foutput, finput);
                    FIOv05_freeDResources(r);
                    return s;
            }   }
        case ZSTDv06_MAGICNUMBER :
            {   dRessv06_t r = FIOv06_createDResources();
                r.dictBuffer = dictBuffer;
                r.dictBufferSize = dictSize;
                {   unsigned long long const s = FIOv06_decompressFrame(r, foutput, finput);
                    FIOv06_freeDResources(r);
                    return s;
            }   }
        case ZSTDv07_MAGICNUMBER :
            {   dRessv07_t r = FIOv07_createDResources();
                r.dictBuffer = dictBuffer;
                r.dictBufferSize = dictSize;
                {   unsigned long long const s = FIOv07_decompressFrame(r, foutput, finput);
                    FIOv07_freeDResources(r);
                    return s;
            }   }
        default :
            return ERROR(prefix_unknown);
    }
}
