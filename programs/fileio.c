/*
  fileio.c - File i/o handler
  Copyright (C) Yann Collet 2013-2015

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
  - zstd source repository : https://github.com/Cyan4973/zstd
  - Public forum : https://groups.google.com/forum/#!forum/lz4c
*/
/*
  Note : this is stand-alone program.
  It is not part of ZSTD compression library, it is a user program of ZSTD library.
  The license of ZSTD library is BSD.
  The license of this file is GPLv2.
*/

/**************************************
*  Compiler Options
**************************************/
/* Disable some Visual warning messages */
#ifdef _MSC_VER
#  define _CRT_SECURE_NO_WARNINGS
#  define _CRT_SECURE_NO_DEPRECATE     /* VS2005 */
#  pragma warning(disable : 4127)      /* disable: C4127: conditional expression is constant */
#endif

#define GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)

#define _FILE_OFFSET_BITS 64   /* Large file support on 32-bits unix */
#define _POSIX_SOURCE 1        /* enable fileno() within <stdio.h> on unix */


/**************************************
*  Includes
**************************************/
#include <stdio.h>    /* fprintf, fopen, fread, _fileno, stdin, stdout */
#include <stdlib.h>   /* malloc, free */
#include <string.h>   /* strcmp, strlen */
#include <time.h>     /* clock */
#include "fileio.h"
#include "zstd_static.h"


/**************************************
*  OS-specific Includes
**************************************/
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


/**************************************
*  Basic Types
**************************************/
#if defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   /* C99 */
# include <stdint.h>
typedef uint8_t  BYTE;
typedef uint16_t U16;
typedef uint32_t U32;
typedef  int32_t S32;
typedef uint64_t U64;
#else
typedef unsigned char       BYTE;
typedef unsigned short      U16;
typedef unsigned int        U32;
typedef   signed int        S32;
typedef unsigned long long  U64;
#endif


/**************************************
*  Constants
**************************************/
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

//static const unsigned FIO_maxBlockSizeID = 0xB;   /* => 2MB block */
static const unsigned FIO_blockHeaderSize = 3;

#define FIO_FRAMEHEADERSIZE 5        /* as a define, because needed to allocated table on stack */
#define FSE_CHECKSUM_SEED        0

#define CACHELINE 64


/**************************************
*  Complex types
**************************************/
typedef enum { bt_compressed, bt_raw, bt_rle, bt_crc } bType_t;


/**************************************
*  Macros
**************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); }
static U32 g_displayLevel = 2;   /* 0 : no display;   1: errors;   2 : + result + interaction + warnings;   3 : + progression;   4 : + information */

#define DISPLAYUPDATE(l, ...) if (g_displayLevel>=l) { \
            if ((FIO_GetMilliSpan(g_time) > refreshRate) || (g_displayLevel>=4)) \
            { g_time = clock(); DISPLAY(__VA_ARGS__); \
            if (g_displayLevel>=4) fflush(stdout); } }
static const unsigned refreshRate = 150;
static clock_t g_time = 0;


/**************************************
*  Local Parameters
**************************************/
static U32 g_overwrite = 0;

void FIO_overwriteMode(void) { g_overwrite=1; }
void FIO_setNotificationLevel(unsigned level) { g_displayLevel=level; }


/**************************************
*  Exceptions
**************************************/
#define DEBUG 0
#define DEBUGOUTPUT(...) if (DEBUG) DISPLAY(__VA_ARGS__);
#define EXM_THROW(error, ...)                                             \
{                                                                         \
    DEBUGOUTPUT("Error defined at %s, line %i : \n", __FILE__, __LINE__); \
    DISPLAYLEVEL(1, "Error %i : ", error);                                \
    DISPLAYLEVEL(1, __VA_ARGS__);                                         \
    DISPLAYLEVEL(1, "\n");                                                \
    exit(error);                                                          \
}


/**************************************
*  Functions
**************************************/
static unsigned FIO_GetMilliSpan(clock_t nPrevious)
{
    clock_t nCurrent = clock();
    unsigned nSpan = (unsigned)(((nCurrent - nPrevious) * 1000) / CLOCKS_PER_SEC);
    return nSpan;
}


static void FIO_getFileHandles(FILE** pfinput, FILE** pfoutput, const char* input_filename, const char* output_filename)
{
    if (!strcmp (input_filename, stdinmark))
    {
        DISPLAYLEVEL(4,"Using stdin for input\n");
        *pfinput = stdin;
        SET_BINARY_MODE(stdin);
    }
    else
    {
        *pfinput = fopen(input_filename, "rb");
    }

    if (!strcmp (output_filename, stdoutmark))
    {
        DISPLAYLEVEL(4,"Using stdout for output\n");
        *pfoutput = stdout;
        SET_BINARY_MODE(stdout);
    }
    else
    {
        /* Check if destination file already exists */
        *pfoutput=0;
        if (strcmp(output_filename,nulmark)) *pfoutput = fopen( output_filename, "rb" );
        if (*pfoutput!=0)
        {
            fclose(*pfoutput);
            if (!g_overwrite)
            {
                char ch;
                if (g_displayLevel <= 1)   /* No interaction possible */
                    EXM_THROW(11, "Operation aborted : %s already exists", output_filename);
                DISPLAYLEVEL(2, "Warning : %s already exists\n", output_filename);
                DISPLAYLEVEL(2, "Overwrite ? (Y/N) : ");
                ch = (char)getchar();
                if ((ch!='Y') && (ch!='y')) EXM_THROW(11, "Operation aborted : %s already exists", output_filename);
            }
        }
        *pfoutput = fopen( output_filename, "wb" );
    }

    if ( *pfinput==0 ) EXM_THROW(12, "Pb opening %s", input_filename);
    if ( *pfoutput==0) EXM_THROW(13, "Pb opening %s", output_filename);
}


unsigned long long FIO_compressFilename(const char* output_filename, const char* input_filename)
{
    U64 filesize = 0;
    U64 compressedfilesize = 0;
    BYTE* inBuff;
    BYTE* inSlot;
    BYTE* inEnd;
    BYTE* outBuff;
    size_t blockSize = 128 KB;
    size_t inBuffSize = 4 * blockSize;
    size_t outBuffSize = ZSTD_compressBound(blockSize);
    FILE* finput;
    FILE* foutput;
    size_t sizeCheck, cSize;
    ZSTD_Cctx* ctx;


    /* Init */
    FIO_getFileHandles(&finput, &foutput, input_filename, output_filename);
    ctx = ZSTD_createCCtx();

    /* Allocate Memory */
    inBuff  = (BYTE*)malloc(inBuffSize);
    outBuff = (BYTE*)malloc(outBuffSize);
    if (!inBuff || !outBuff) EXM_THROW(21, "Allocation error : not enough memory");
    inSlot = inBuff;
    inEnd = inBuff + inBuffSize;

    /* Write Frame Header */
    cSize = ZSTD_compressBegin(ctx, outBuff, outBuffSize);
    if (ZSTD_isError(cSize)) EXM_THROW(22, "Compression error : cannot create frame header");

    sizeCheck = fwrite(outBuff, 1, cSize, foutput);
    if (sizeCheck!=cSize) EXM_THROW(23, "Write error : cannot write header");
    compressedfilesize += cSize;

    /* Main compression loop */
    while (1)
    {
        size_t inSize;

        /* Fill input Buffer */
        if (inSlot + blockSize > inEnd) inSlot = inBuff;
        inSize = fread(inSlot, (size_t)1, blockSize, finput);
        if (inSize==0) break;
        filesize += inSize;
        DISPLAYUPDATE(2, "\rRead : %u MB   ", (U32)(filesize>>20));

        /* Compress Block */
        cSize = ZSTD_compressContinue(ctx, outBuff, outBuffSize, inSlot, inSize);
        if (ZSTD_isError(cSize))
            EXM_THROW(24, "Compression error : %s ", ZSTD_getErrorName(cSize));

        /* Write cBlock */
        sizeCheck = fwrite(outBuff, 1, cSize, foutput);
        if (sizeCheck!=cSize) EXM_THROW(25, "Write error : cannot write compressed block");
        compressedfilesize += cSize;
        inSlot += inSize;

        DISPLAYUPDATE(2, "\rRead : %u MB  ==> %.2f%%   ", (U32)(filesize>>20), (double)compressedfilesize/filesize*100);
    }

    /* End of Frame */
    cSize = ZSTD_compressEnd(ctx, outBuff, outBuffSize);
    if (ZSTD_isError(cSize)) EXM_THROW(26, "Compression error : cannot create frame end");

    sizeCheck = fwrite(outBuff, 1, cSize, foutput);
    if (sizeCheck!=cSize) EXM_THROW(27, "Write error : cannot write frame end");
    compressedfilesize += cSize;

    /* Status */
    DISPLAYLEVEL(2, "\r%79s\r", "");
    DISPLAYLEVEL(2,"Compressed %llu bytes into %llu bytes ==> %.2f%%\n",
        (unsigned long long) filesize, (unsigned long long) compressedfilesize, (double)compressedfilesize/filesize*100);

    /* clean */
    free(inBuff);
    free(outBuff);
    fclose(finput);
    fclose(foutput);
    ZSTD_freeCCtx(ctx);

    return compressedfilesize;
}


#define MAXHEADERSIZE FIO_FRAMEHEADERSIZE+3
unsigned long long FIO_decompressFilename(const char* output_filename, const char* input_filename)
{
    FILE* finput, *foutput;
    BYTE* inBuff;
    size_t inBuffSize;
    BYTE* outBuff, *op, *oend;
    size_t outBuffSize;
    U32   blockSize = 128 KB;
    U32   wNbBlocks = 4;
    U64   filesize = 0;
    BYTE* header[MAXHEADERSIZE];
    ZSTD_Dctx* dctx;
    size_t toRead;
    size_t sizeCheck;


    /* Init */
    FIO_getFileHandles(&finput, &foutput, input_filename, output_filename);
    dctx = ZSTD_createDCtx();

    /* check header */
    toRead = ZSTD_nextSrcSizeToDecompress(dctx);
    if (toRead > MAXHEADERSIZE) EXM_THROW(30, "Not enough memory to read header");
    sizeCheck = fread(header, (size_t)1, toRead, finput);
    if (sizeCheck != toRead) EXM_THROW(31, "Read error : cannot read header");
    sizeCheck = ZSTD_decompressContinue(dctx, NULL, 0, header, toRead);   // Decode frame header
    if (ZSTD_isError(sizeCheck)) EXM_THROW(32, "Error decoding header");

    /* Here later : blockSize determination */

    /* Allocate Memory */
    inBuffSize = blockSize + FIO_blockHeaderSize;
    inBuff  = (BYTE*)malloc(inBuffSize);
    outBuffSize = wNbBlocks * blockSize;
    outBuff = (BYTE*)malloc(outBuffSize);
    op = outBuff;
    oend = outBuff + outBuffSize;
    if (!inBuff || !outBuff) EXM_THROW(33, "Allocation error : not enough memory");

    /* Main decompression Loop */
    toRead = ZSTD_nextSrcSizeToDecompress(dctx);
    while (toRead)
    {
        size_t readSize, decodedSize;

        /* Fill input buffer */
        readSize = fread(inBuff, 1, toRead, finput);
        if (readSize != toRead)
            EXM_THROW(34, "Read error");

        /* Decode block */
        decodedSize = ZSTD_decompressContinue(dctx, op, oend-op, inBuff, readSize);
        if (ZSTD_isError(decodedSize)) EXM_THROW(35, "Decoding error : input corrupted");

        if (decodedSize)   /* not a header */
        {
            /* Write block */
            sizeCheck = fwrite(op, 1, decodedSize, foutput);
            if (sizeCheck != decodedSize) EXM_THROW(36, "Write error : unable to write data block to destination file");
            filesize += decodedSize;
            op += decodedSize;
            if (op==oend) op = outBuff;
            DISPLAYUPDATE(2, "\rDecoded : %u MB...     ", (U32)(filesize>>20) );
        }

        /* prepare for next Block */
        toRead = ZSTD_nextSrcSizeToDecompress(dctx);
    }

    DISPLAYLEVEL(2, "\r%79s\r", "");
    DISPLAYLEVEL(2,"Decoded %llu bytes   \n", (long long unsigned)filesize);

    /* clean */
    free(inBuff);
    free(outBuff);
    fclose(finput);
    fclose(foutput);
    ZSTD_freeDCtx(dctx);

    return filesize;
}

