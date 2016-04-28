/*
    dibio - I/O API for dictionary builder
    Copyright (C) Yann Collet 2016

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
    - zstd homepage : http://www.zstd.net/
*/

/*-**************************************
*  Compiler Options
****************************************/
/* Disable some Visual warning messages */
#ifdef _MSC_VER
#  define _CRT_SECURE_NO_WARNINGS                /* fopen */
#  pragma warning(disable : 4127)                /* disable: C4127: conditional expression is constant */
#endif

/* Unix Large Files support (>4GB) */
#define _FILE_OFFSET_BITS 64
#if (defined(__sun__) && (!defined(__LP64__)))   /* Sun Solaris 32-bits requires specific definitions */
#  define _LARGEFILE_SOURCE
#elif ! defined(__LP64__)                        /* No point defining Large file for 64 bit */
#  define _LARGEFILE64_SOURCE
#endif


/*-*************************************
*  Includes
***************************************/
#include <stdlib.h>         /* malloc, free */
#include <string.h>         /* memset */
#include <stdio.h>          /* fprintf, fopen, ftello64 */

#include "util.h"           /* UTIL_GetFileSize */
#include "mem.h"            /* read */
#include "error_private.h"
#include "dibio.h"


/*-*************************************
*  Constants
***************************************/
#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define DICTLISTSIZE 10000
#define MEMMULT 11
static const size_t maxMemory = (sizeof(size_t) == 4) ? (2 GB - 64 MB) : ((size_t)(512 MB) << sizeof(size_t));

#define NOISELENGTH 32
#define PRIME1   2654435761U
#define PRIME2   2246822519U


/*-*************************************
*  Console display
***************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); }
static unsigned g_displayLevel = 0;   /* 0 : no display;   1: errors;   2: default;  4: full information */


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
    DISPLAYLEVEL(1, "\n");                                                \
    exit(error);                                                          \
}


/* ********************************************************
*  Helper functions
**********************************************************/
unsigned DiB_isError(size_t errorCode) { return ERR_isError(errorCode); }

const char* DiB_getErrorName(size_t errorCode) { return ERR_getErrorName(errorCode); }


/* ********************************************************
*  File related operations
**********************************************************/
static unsigned long long DiB_getTotalFileSize(const char** fileNamesTable, unsigned nbFiles)
{
    unsigned long long total = 0;
    unsigned n;
    for (n=0; n<nbFiles; n++)
        total += UTIL_getFileSize(fileNamesTable[n]);
    return total;
}


static void DiB_loadFiles(void* buffer, size_t bufferSize,
                          size_t* fileSizes,
                          const char** fileNamesTable, unsigned nbFiles)
{
    char* buff = (char*)buffer;
    size_t pos = 0;
    unsigned n;

    for (n=0; n<nbFiles; n++) {
        size_t readSize;
        unsigned long long fileSize = UTIL_getFileSize(fileNamesTable[n]);
        FILE* f = fopen(fileNamesTable[n], "rb");
        if (f==NULL) EXM_THROW(10, "impossible to open file %s", fileNamesTable[n]);
        DISPLAYLEVEL(2, "Loading %s...       \r", fileNamesTable[n]);
        if (fileSize > bufferSize-pos) fileSize = 0;  /* stop there, not enough memory to load all files */
        readSize = fread(buff+pos, 1, (size_t)fileSize, f);
        if (readSize != (size_t)fileSize) EXM_THROW(11, "could not read %s", fileNamesTable[n]);
        pos += readSize;
        fileSizes[n] = (size_t)fileSize;
        fclose(f);
    }
}


/*-********************************************************
*  Dictionary training functions
**********************************************************/
static size_t DiB_findMaxMem(unsigned long long requiredMem)
{
    size_t step = 8 MB;
    void* testmem = NULL;

    requiredMem = (((requiredMem >> 23) + 1) << 23);
    requiredMem += 2 * step;
    if (requiredMem > maxMemory) requiredMem = maxMemory;

    while (!testmem) {
        requiredMem -= step;
        testmem = malloc((size_t)requiredMem);
    }

    free(testmem);
    return (size_t)(requiredMem - step);
}


static void DiB_fillNoise(void* buffer, size_t length)
{
    unsigned acc = PRIME1;
    size_t p=0;;

    for (p=0; p<length; p++) {
        acc *= PRIME2;
        ((unsigned char*)buffer)[p] = (unsigned char)(acc >> 21);
    }
}


static void DiB_saveDict(const char* dictFileName,
                         const void* buff, size_t buffSize)
{
    FILE* f;
    size_t n;

    f = fopen(dictFileName, "wb");
    if (f==NULL) EXM_THROW(3, "cannot open %s ", dictFileName);

    n = fwrite(buff, 1, buffSize, f);
    if (n!=buffSize) EXM_THROW(4, "%s : write error", dictFileName)

    n = (size_t)fclose(f);
    if (n!=0) EXM_THROW(5, "%s : flush error", dictFileName)
}


/*! ZDICT_trainFromBuffer_unsafe() :
    Strictly Internal use only !!
    Same as ZDICT_trainFromBuffer_advanced(), but does not control `samplesBuffer`.
    `samplesBuffer` must be followed by noisy guard band to avoid out-of-buffer reads.
    @return : size of dictionary stored into `dictBuffer` (<= `dictBufferCapacity`)
              or an error code.
*/
size_t ZDICT_trainFromBuffer_unsafe(void* dictBuffer, size_t dictBufferCapacity,
                              const void* samplesBuffer, const size_t* samplesSizes, unsigned nbSamples,
                              ZDICT_params_t parameters);


int DiB_trainFromFiles(const char* dictFileName, unsigned maxDictSize,
                       const char** fileNamesTable, unsigned nbFiles,
                       ZDICT_params_t params)
{
    void* srcBuffer;
    size_t benchedSize;
    size_t* fileSizes = (size_t*)malloc(nbFiles * sizeof(size_t));
    unsigned long long totalSizeToLoad = DiB_getTotalFileSize(fileNamesTable, nbFiles);
    void* dictBuffer = malloc(maxDictSize);
    size_t dictSize;
    int result = 0;

    /* init */
    g_displayLevel = params.notificationLevel;
    benchedSize = DiB_findMaxMem(totalSizeToLoad * MEMMULT) / MEMMULT;
    if ((unsigned long long)benchedSize > totalSizeToLoad) benchedSize = (size_t)totalSizeToLoad;
    if (benchedSize < totalSizeToLoad)
        DISPLAYLEVEL(1, "Not enough memory; training on %u MB only...\n", (unsigned)(benchedSize >> 20));

    /* Memory allocation & restrictions */
    srcBuffer = malloc(benchedSize+NOISELENGTH);     /* + noise */
    if ((!fileSizes) || (!srcBuffer) || (!dictBuffer)) EXM_THROW(12, "not enough memory for DiB_trainFiles");  /* should not happen */

    /* Load input buffer */
    DiB_loadFiles(srcBuffer, benchedSize, fileSizes, fileNamesTable, nbFiles);
    DiB_fillNoise((char*)srcBuffer + benchedSize, NOISELENGTH);   /* guard band, for end of buffer condition */

    /* call buffer version */
    dictSize = ZDICT_trainFromBuffer_unsafe(dictBuffer, maxDictSize,
                        srcBuffer, fileSizes, nbFiles,
                        params);
    if (ZDICT_isError(dictSize)) {
        DISPLAYLEVEL(1, "dictionary training failed : %s \n", ZDICT_getErrorName(dictSize));   /* should not happen */
        result = 1;
        goto _cleanup;
    }

    /* save dict */
    DISPLAYLEVEL(2, "Save dictionary of size %u into file %s \n", (U32)dictSize, dictFileName);
    DiB_saveDict(dictFileName, dictBuffer, dictSize);

    /* clean up */
_cleanup:
    free(srcBuffer);
    free(dictBuffer);
    free(fileSizes);
    return result;
}
