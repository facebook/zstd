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

/* *************************************
*  Tuning options
***************************************/
#ifndef ZSTD_LEGACY_SUPPORT
/**LEGACY_SUPPORT :
*  decompressor can decode older formats (starting from Zstd 0.1+) */
#  define ZSTD_LEGACY_SUPPORT 1
#endif


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
#include <stdio.h>      /* fprintf, fopen, fread, _fileno, stdin, stdout */
#include <stdlib.h>     /* malloc, free */
#include <string.h>     /* strcmp, strlen */
#include <time.h>       /* clock */
#include <errno.h>      /* errno */
#include <sys/types.h>  /* stat64 */
#include <sys/stat.h>   /* stat64 */
#include "mem.h"
#include "fileio.h"
#include "zstd_static.h"   /* ZSTD_magicNumber */
#include "zstd_buffered_static.h"

#if defined(ZSTD_LEGACY_SUPPORT) && (ZSTD_LEGACY_SUPPORT==1)
#  include "zstd_legacy.h"    /* legacy */
#  include "fileio_legacy.h"  /* legacy */
#endif


/* *************************************
*  OS-specific Includes
***************************************/
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(_WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>    /* _O_BINARY */
#  include <io.h>       /* _setmode, _isatty */
#  ifdef __MINGW32__
   // int _fileno(FILE *stream);   /* seems no longer useful /* MINGW somehow forgets to include this windows declaration into <stdio.h> */
#  endif
#  define SET_BINARY_MODE(file) { int unused = _setmode(_fileno(file), _O_BINARY); (void)unused; }
#  define IS_CONSOLE(stdStream) _isatty(_fileno(stdStream))
#else
#  include <unistd.h>   /* isatty */
#  define SET_BINARY_MODE(file)
#  define IS_CONSOLE(stdStream) isatty(fileno(stdStream))
#endif

#if !defined(S_ISREG)
#  define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
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

#define BLOCKSIZE      (128 KB)
#define ROLLBUFFERSIZE (BLOCKSIZE*8*64)

#define FIO_FRAMEHEADERSIZE 5        /* as a define, because needed to allocated table on stack */
#define FSE_CHECKSUM_SEED        0

#define CACHELINE 64

#define MAX_DICT_SIZE (512 KB)

/* *************************************
*  Macros
***************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); }
static U32 g_displayLevel = 2;   /* 0 : no display;   1: errors;   2 : + result + interaction + warnings;   3 : + progression;   4 : + information */

#define DISPLAYUPDATE(l, ...) if (g_displayLevel>=l) { \
            if ((FIO_GetMilliSpan(g_time) > refreshRate) || (g_displayLevel>=4)) \
            { g_time = clock(); DISPLAY(__VA_ARGS__); \
            if (g_displayLevel>=4) fflush(stdout); } }
static const unsigned refreshRate = 150;
static clock_t g_time = 0;


/* *************************************
*  Local Parameters
***************************************/
static U32 g_overwrite = 0;

void FIO_overwriteMode(void) { g_overwrite=1; }
void FIO_setNotificationLevel(unsigned level) { g_displayLevel=level; }


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


static U64 FIO_getFileSize(const char* infilename)
{
    int r;
#if defined(_MSC_VER)
    struct _stat64 statbuf;
    r = _stat64(infilename, &statbuf);
#else
    struct stat statbuf;
    r = stat(infilename, &statbuf);
#endif
    if (r || !S_ISREG(statbuf.st_mode)) return 0;
    return (U64)statbuf.st_size;
}


static int FIO_getFiles(FILE** fileOutPtr, FILE** fileInPtr,
                        const char* dstFileName, const char* srcFileName)
{
    if (!strcmp (srcFileName, stdinmark))
    {
        DISPLAYLEVEL(4,"Using stdin for input\n");
        *fileInPtr = stdin;
        SET_BINARY_MODE(stdin);
    }
    else
    {
        *fileInPtr = fopen(srcFileName, "rb");
    }

    if ( *fileInPtr==0 )
    {
        DISPLAYLEVEL(1, "Unable to access file for processing: %s\n", srcFileName);
        return 1;
    }

    if (!strcmp (dstFileName, stdoutmark))
    {
        DISPLAYLEVEL(4,"Using stdout for output\n");
        *fileOutPtr = stdout;
        SET_BINARY_MODE(stdout);
    }
    else
    {
        /* Check if destination file already exists */
        if (!g_overwrite)
        {
            *fileOutPtr = fopen( dstFileName, "rb" );
            if (*fileOutPtr != 0)
            {
                /* prompt for overwrite authorization */
                fclose(*fileOutPtr);
                DISPLAY("Warning : %s already exists \n", dstFileName);
                if ((g_displayLevel <= 1) || (*fileInPtr == stdin))
                {
                    /* No interaction possible */
                    DISPLAY("Operation aborted : %s already exists \n", dstFileName);
                    return 1;
                }
                DISPLAY("Overwrite ? (y/N) : ");
                {
                    int ch = getchar();
                    if ((ch!='Y') && (ch!='y'))
                    {
                        DISPLAY("No. Operation aborted : %s already exists \n", dstFileName);
                        return 1;
                    }
                    while ((ch!=EOF) && (ch!='\n')) ch = getchar();  /* flush rest of input line */
                }
            }
        }
        *fileOutPtr = fopen( dstFileName, "wb" );
    }

    if (*fileOutPtr==0) EXM_THROW(13, "Pb opening %s", dstFileName);

    return 0;
}

/*!FIO_loadFile
*  creates a buffer, pointed by *bufferPtr,
*  loads "filename" content into it
*  up to MAX_DICT_SIZE bytes
*/
static size_t FIO_loadFile(void** bufferPtr, const char* fileName)
{
    FILE* fileHandle;
    size_t readSize;
    U64 fileSize;

    *bufferPtr = NULL;
    if (fileName == NULL)
        return 0;

    DISPLAYLEVEL(4,"Loading %s as dictionary \n", fileName);
    fileHandle = fopen(fileName, "rb");
    if (fileHandle==0) EXM_THROW(31, "Error opening file %s", fileName);
    fileSize = FIO_getFileSize(fileName);
    if (fileSize > MAX_DICT_SIZE)
    {
        int seekResult;
        if (fileSize > 1 GB) EXM_THROW(32, "Dictionary file %s is too large", fileName);   /* avoid extreme cases */
        DISPLAYLEVEL(2,"Dictionary %s is too large : using last %u bytes only \n", fileName, MAX_DICT_SIZE);
        seekResult = fseek(fileHandle, (long int)(fileSize-MAX_DICT_SIZE), SEEK_SET);   /* use end of file */
        if (seekResult != 0) EXM_THROW(33, "Error seeking into file %s", fileName);
        fileSize = MAX_DICT_SIZE;
    }
    *bufferPtr = (BYTE*)malloc((size_t)fileSize);
    if (*bufferPtr==NULL) EXM_THROW(34, "Allocation error : not enough memory for dictBuffer");
    readSize = fread(*bufferPtr, 1, (size_t)fileSize, fileHandle);
    if (readSize!=fileSize) EXM_THROW(35, "Error reading dictionary file %s", fileName);
    fclose(fileHandle);
    return (size_t)fileSize;
}


/* **********************************************************************
*  Compression
************************************************************************/
typedef struct {
    void*  srcBuffer;
    size_t srcBufferSize;
    void*  dstBuffer;
    size_t dstBufferSize;
    void*  dictBuffer;
    size_t dictBufferSize;
    ZBUFF_CCtx* ctx;
} cRess_t;

static cRess_t FIO_createCResources(const char* dictFileName)
{
    cRess_t ress;

    ress.ctx = ZBUFF_createCCtx();
    if (ress.ctx == NULL) EXM_THROW(30, "Allocation error : can't create ZBUFF context");

    /* Allocate Memory */
    ress.srcBufferSize = ZBUFF_recommendedCInSize();
    ress.srcBuffer = malloc(ress.srcBufferSize);
    ress.dstBufferSize = ZBUFF_recommendedCOutSize();
    ress.dstBuffer = malloc(ress.dstBufferSize);
    if (!ress.srcBuffer || !ress.dstBuffer) EXM_THROW(31, "Allocation error : not enough memory");

    /* dictionary */
    ress.dictBufferSize = FIO_loadFile(&(ress.dictBuffer), dictFileName);

    return ress;
}

static void FIO_freeCResources(cRess_t ress)
{
    size_t errorCode;
    free(ress.srcBuffer);
    free(ress.dstBuffer);
    free(ress.dictBuffer);
    errorCode = ZBUFF_freeCCtx(ress.ctx);
    if (ZBUFF_isError(errorCode)) EXM_THROW(38, "Error : can't release ZBUFF context resource : %s", ZBUFF_getErrorName(errorCode));
}


/*
 * FIO_compressFilename_extRess()
 * result : 0 : compression completed correctly
 *          1 : missing or pb opening srcFileName
 */
static int FIO_compressFilename_extRess(cRess_t ress,
                                        const char* dstFileName, const char* srcFileName,
                                        int cLevel)
{
    FILE* srcFile;
    FILE* dstFile;
    U64 filesize = 0;
    U64 compressedfilesize = 0;
    size_t dictSize = ress.dictBufferSize;
    size_t sizeCheck, errorCode;

    /* File check */
    if (FIO_getFiles(&dstFile, &srcFile, dstFileName, srcFileName)) return 1;

    /* init */
    filesize = FIO_getFileSize(srcFileName) + dictSize;
    errorCode = ZBUFF_compressInit_advanced(ress.ctx, ZSTD_getParams(cLevel, filesize));
    if (ZBUFF_isError(errorCode)) EXM_THROW(21, "Error initializing compression");
    errorCode = ZBUFF_compressWithDictionary(ress.ctx, ress.dictBuffer, ress.dictBufferSize);
    if (ZBUFF_isError(errorCode)) EXM_THROW(22, "Error initializing dictionary");

    /* Main compression loop */
    filesize = 0;
    while (1)
    {
        size_t inSize;

        /* Fill input Buffer */
        inSize = fread(ress.srcBuffer, (size_t)1, ress.srcBufferSize, srcFile);
        if (inSize==0) break;
        filesize += inSize;
        DISPLAYUPDATE(2, "\rRead : %u MB  ", (U32)(filesize>>20));

        {
            /* Compress (buffered streaming ensures appropriate formatting) */
            size_t usedInSize = inSize;
            size_t cSize = ress.dstBufferSize;
            size_t result = ZBUFF_compressContinue(ress.ctx, ress.dstBuffer, &cSize, ress.srcBuffer, &usedInSize);
            if (ZBUFF_isError(result))
                EXM_THROW(23, "Compression error : %s ", ZBUFF_getErrorName(result));
            if (inSize != usedInSize)
                /* inBuff should be entirely consumed since buffer sizes are recommended ones */
                EXM_THROW(24, "Compression error : input block not fully consumed");

            /* Write cBlock */
            sizeCheck = fwrite(ress.dstBuffer, 1, cSize, dstFile);
            if (sizeCheck!=cSize) EXM_THROW(25, "Write error : cannot write compressed block into %s", dstFileName);
            compressedfilesize += cSize;
        }

        DISPLAYUPDATE(2, "\rRead : %u MB  ==> %.2f%%   ", (U32)(filesize>>20), (double)compressedfilesize/filesize*100);
    }

    /* End of Frame */
    {
        size_t cSize = ress.dstBufferSize;
        size_t result = ZBUFF_compressEnd(ress.ctx, ress.dstBuffer, &cSize);
        if (result!=0) EXM_THROW(26, "Compression error : cannot create frame end");

        sizeCheck = fwrite(ress.dstBuffer, 1, cSize, dstFile);
        if (sizeCheck!=cSize) EXM_THROW(27, "Write error : cannot write frame end into %s", dstFileName);
        compressedfilesize += cSize;
    }

    /* Status */
    DISPLAYLEVEL(2, "\r%79s\r", "");
    DISPLAYLEVEL(2,"Compressed %llu bytes into %llu bytes ==> %.2f%%\n",
        (unsigned long long) filesize, (unsigned long long) compressedfilesize, (double)compressedfilesize/filesize*100);

    /* clean */
    fclose(srcFile);
    if (fclose(dstFile)) EXM_THROW(28, "Write error : cannot properly close %s", dstFileName);

    return 0;
}


int FIO_compressFilename(const char* dstFileName, const char* srcFileName,
                         const char* dictFileName, int compressionLevel)
{
    clock_t start, end;
    cRess_t ress;
    int issueWithSrcFile = 0;

    /* Init */
    start = clock();
    ress = FIO_createCResources(dictFileName);

    /* Compress File */
    issueWithSrcFile += FIO_compressFilename_extRess(ress, dstFileName, srcFileName, compressionLevel);

    /* Free resources */
    FIO_freeCResources(ress);

    /* Final Status */
    end = clock();
    {
        double seconds = (double)(end - start) / CLOCKS_PER_SEC;
        DISPLAYLEVEL(4, "Completed in %.2f sec \n", seconds);
    }

    return issueWithSrcFile;
}


#define FNSPACE 30
int FIO_compressMultipleFilenames(const char** inFileNamesTable, unsigned nbFiles,
                                  const char* suffix,
                                  const char* dictFileName, int compressionLevel)
{
    unsigned u;
    int missed_files = 0;
    char* dstFileName = (char*)malloc(FNSPACE);
    size_t dfnSize = FNSPACE;
    const size_t suffixSize = strlen(suffix);
    cRess_t ress;

    /* init */
    ress = FIO_createCResources(dictFileName);

    /* loop on each file */
    for (u=0; u<nbFiles; u++)
    {
        size_t ifnSize = strlen(inFileNamesTable[u]);
        if (dfnSize <= ifnSize+suffixSize+1) { free(dstFileName); dfnSize = ifnSize + 20; dstFileName = (char*)malloc(dfnSize); }
        strcpy(dstFileName, inFileNamesTable[u]);
        strcat(dstFileName, suffix);

        missed_files += FIO_compressFilename_extRess(ress, dstFileName, inFileNamesTable[u], compressionLevel);
    }

    /* Close & Free */
    FIO_freeCResources(ress);
    free(dstFileName);

    return missed_files;
}


/* **************************************************************************
*  Decompression
****************************************************************************/
typedef struct {
    void*  srcBuffer;
    size_t srcBufferSize;
    void*  dstBuffer;
    size_t dstBufferSize;
    void*  dictBuffer;
    size_t dictBufferSize;
    ZBUFF_DCtx* dctx;
} dRess_t;

static dRess_t FIO_createDResources(const char* dictFileName)
{
    dRess_t ress;

    /* init */
    ress.dctx = ZBUFF_createDCtx();
    if (ress.dctx==NULL) EXM_THROW(60, "Can't create ZBUFF decompression context");

    /* Allocate Memory */
    ress.srcBufferSize = ZBUFF_recommendedDInSize();
    ress.srcBuffer = malloc(ress.srcBufferSize);
    ress.dstBufferSize = ZBUFF_recommendedDOutSize();
    ress.dstBuffer = malloc(ress.dstBufferSize);
    if (!ress.srcBuffer || !ress.dstBuffer) EXM_THROW(61, "Allocation error : not enough memory");

    /* dictionary */
    ress.dictBufferSize = FIO_loadFile(&(ress.dictBuffer), dictFileName);

    return ress;
}

static void FIO_freeDResources(dRess_t ress)
{
    size_t errorCode = ZBUFF_freeDCtx(ress.dctx);
    if (ZBUFF_isError(errorCode)) EXM_THROW(69, "Error : can't free ZBUFF context resource : %s", ZBUFF_getErrorName(errorCode));
    free(ress.srcBuffer);
    free(ress.dstBuffer);
    free(ress.dictBuffer);
}


unsigned long long FIO_decompressFrame(dRess_t ress,
                                       FILE* foutput, FILE* finput, size_t alreadyLoaded)
{
    U64    frameSize = 0;
    size_t readSize=alreadyLoaded;

    /* Main decompression Loop */
    ZBUFF_decompressInitDictionary(ress.dctx, ress.dictBuffer, ress.dictBufferSize);
    while (1)
    {
        /* Decode */
        size_t sizeCheck;
        size_t inSize=readSize, decodedSize=ress.dstBufferSize;
        size_t toRead = ZBUFF_decompressContinue(ress.dctx, ress.dstBuffer, &decodedSize, ress.srcBuffer, &inSize);
        if (ZBUFF_isError(toRead)) EXM_THROW(36, "Decoding error : %s", ZBUFF_getErrorName(toRead));
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


static int FIO_decompressFile_extRess(dRess_t ress,
                                      const char* dstFileName, const char* srcFileName)
{
    unsigned long long filesize = 0;
    FILE* srcFile;
    FILE* dstFile;

    /* Init */
    if (FIO_getFiles(&dstFile, &srcFile, dstFileName, srcFileName)) return 1;

    /* for each frame */
    for ( ; ; )
    {
        size_t sizeCheck;
        /* check magic number -> version */
        size_t toRead = 4;
        sizeCheck = fread(ress.srcBuffer, (size_t)1, toRead, srcFile);
        if (sizeCheck==0) break;   /* no more input */
        if (sizeCheck != toRead) EXM_THROW(31, "Read error : cannot read header");
#if defined(ZSTD_LEGACY_SUPPORT) && (ZSTD_LEGACY_SUPPORT==1)
        if (ZSTD_isLegacy(MEM_readLE32(ress.srcBuffer)))
        {
            filesize += FIO_decompressLegacyFrame(dstFile, srcFile, MEM_readLE32(ress.srcBuffer));
            continue;
        }
#endif   /* ZSTD_LEGACY_SUPPORT */

        filesize += FIO_decompressFrame(ress, dstFile, srcFile, toRead);
    }

    /* Final Status */
    DISPLAYLEVEL(2, "\r%79s\r", "");
    DISPLAYLEVEL(2, "Successfully decoded %llu bytes \n", filesize);

    /* Close */
    fclose(srcFile);
    if (fclose(dstFile)) EXM_THROW(38, "Write error : cannot properly close %s", dstFileName);

    return 0;
}


int FIO_decompressFilename(const char* dstFileName, const char* srcFileName,
                           const char* dictFileName)
{
    int missingFiles = 0;
    dRess_t ress = FIO_createDResources(dictFileName);

    missingFiles += FIO_decompressFile_extRess(ress, dstFileName, srcFileName);

    FIO_freeDResources(ress);
    return missingFiles;
}


#define MAXSUFFIXSIZE 8
int FIO_decompressMultipleFilenames(const char** srcNamesTable, unsigned nbFiles,
                                    const char* suffix,
                                    const char* dictFileName)
{
    unsigned u;
    int skippedFiles = 0;
    int missingFiles = 0;
    char* dstFileName = (char*)malloc(FNSPACE);
    size_t dfnSize = FNSPACE;
    const size_t suffixSize = strlen(suffix);
    dRess_t ress;

	if (dstFileName==NULL) EXM_THROW(70, "not enough memory for dstFileName");
    ress = FIO_createDResources(dictFileName);

    for (u=0; u<nbFiles; u++)
    {
        const char* srcFileName = srcNamesTable[u];
        size_t sfnSize = strlen(srcFileName);
        const char* suffixPtr = srcFileName + sfnSize - suffixSize;
        if (dfnSize <= sfnSize-suffixSize+1) { free(dstFileName); dfnSize = sfnSize + 20; dstFileName = (char*)malloc(dfnSize); if (dstFileName==NULL) EXM_THROW(71, "not enough memory for dstFileName"); }
        if (sfnSize <= suffixSize  ||  strcmp(suffixPtr, suffix) != 0)
        {
            DISPLAYLEVEL(1, "File extension doesn't match expected extension (%4s); will not process file: %s\n", suffix, srcFileName);
            skippedFiles++;
            continue;
        }
        memcpy(dstFileName, srcFileName, sfnSize - suffixSize);
        dstFileName[sfnSize-suffixSize] = '\0';

        missingFiles += FIO_decompressFile_extRess(ress, dstFileName, srcFileName);
    }

    FIO_freeDResources(ress);
    free(dstFileName);
    return missingFiles + skippedFiles;
}
