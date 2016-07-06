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

/*-*************************************
*  Includes
***************************************/
#include "util.h"           /* Compiler options, UTIL_GetFileSize, UTIL_getTotalFileSize */
#include <stdlib.h>         /* malloc, free */
#include <string.h>         /* memset */
#include <stdio.h>          /* fprintf, fopen, ftello64 */
#include <time.h>           /* clock_t, clock, CLOCKS_PER_SEC */

#include "mem.h"            /* read */
#include "error_private.h"
#include "dibio.h"


/*-*************************************
*  Constants
***************************************/
#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define MEMMULT 11
static const size_t maxMemory = (sizeof(size_t) == 4) ? (2 GB - 64 MB) : ((size_t)(512 MB) << sizeof(size_t));

#define NOISELENGTH 32


/*-*************************************
*  Console display
***************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); }
static unsigned g_displayLevel = 0;   /* 0 : no display;   1: errors;   2: default;  4: full information */

#define DISPLAYUPDATE(l, ...) if (g_displayLevel>=l) { \
            if ((DIB_clockSpan(g_time) > refreshRate) || (g_displayLevel>=4)) \
            { g_time = clock(); DISPLAY(__VA_ARGS__); \
            if (g_displayLevel>=4) fflush(stdout); } }
static const clock_t refreshRate = CLOCKS_PER_SEC * 2 / 10;
static clock_t g_time = 0;

static clock_t DIB_clockSpan(clock_t nPrevious) { return clock() - nPrevious; }


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

#define MIN(a,b)   ( (a) < (b) ? (a) : (b) )


/* ********************************************************
*  File related operations
**********************************************************/
/** DiB_loadFiles() :
*   @return : nb of files effectively loaded into `buffer` */
static unsigned DiB_loadFiles(void* buffer, size_t* bufferSizePtr,
                              size_t* fileSizes,
                              const char** fileNamesTable, unsigned nbFiles)
{
    char* const buff = (char*)buffer;
    size_t pos = 0;
    unsigned n;

    for (n=0; n<nbFiles; n++) {
        const char* const fileName = fileNamesTable[n];
        unsigned long long const fs64 = UTIL_getFileSize(fileName);
        size_t const fileSize = (size_t) MIN(fs64, 128 KB);
        if (fileSize > *bufferSizePtr-pos) break;
        {   FILE* const f = fopen(fileName, "rb");
            if (f==NULL) EXM_THROW(10, "zstd: dictBuilder: %s %s ", fileName, strerror(errno));
            DISPLAYUPDATE(2, "Loading %s...       \r", fileName);
            { size_t const readSize = fread(buff+pos, 1, fileSize, f);
              if (readSize != fileSize) EXM_THROW(11, "Pb reading %s", fileName);
              pos += readSize; }
            fileSizes[n] = fileSize;
            fclose(f);
    }   }
    *bufferSizePtr = pos;
    return n;
}


/*-********************************************************
*  Dictionary training functions
**********************************************************/
static size_t DiB_findMaxMem(unsigned long long requiredMem)
{
    size_t const step = 8 MB;
    void* testmem = NULL;

    requiredMem = (((requiredMem >> 23) + 1) << 23);
    requiredMem += step;
    if (requiredMem > maxMemory) requiredMem = maxMemory;

    while (!testmem) {
        testmem = malloc((size_t)requiredMem);
        requiredMem -= step;
    }

    free(testmem);
    return (size_t)requiredMem;
}


static void DiB_fillNoise(void* buffer, size_t length)
{
    unsigned const prime1 = 2654435761U;
    unsigned const prime2 = 2246822519U;
    unsigned acc = prime1;
    size_t p=0;;

    for (p=0; p<length; p++) {
        acc *= prime2;
        ((unsigned char*)buffer)[p] = (unsigned char)(acc >> 21);
    }
}


static void DiB_saveDict(const char* dictFileName,
                         const void* buff, size_t buffSize)
{
    FILE* const f = fopen(dictFileName, "wb");
    if (f==NULL) EXM_THROW(3, "cannot open %s ", dictFileName);

    { size_t const n = fwrite(buff, 1, buffSize, f);
      if (n!=buffSize) EXM_THROW(4, "%s : write error", dictFileName) }

    { size_t const n = (size_t)fclose(f);
      if (n!=0) EXM_THROW(5, "%s : flush error", dictFileName) }
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
    void* const dictBuffer = malloc(maxDictSize);
    size_t* const fileSizes = (size_t*)malloc(nbFiles * sizeof(size_t));
    unsigned long long const totalSizeToLoad = UTIL_getTotalFileSize(fileNamesTable, nbFiles);
    size_t const maxMem =  DiB_findMaxMem(totalSizeToLoad * MEMMULT) / MEMMULT;
    size_t benchedSize = MIN (maxMem, (size_t)totalSizeToLoad);
    void* const srcBuffer = malloc(benchedSize+NOISELENGTH);
    int result = 0;

    /* Checks */
    if ((!fileSizes) || (!srcBuffer) || (!dictBuffer)) EXM_THROW(12, "not enough memory for DiB_trainFiles");   /* should not happen */

    /* init */
    g_displayLevel = params.notificationLevel;
    if (benchedSize < totalSizeToLoad)
        DISPLAYLEVEL(1, "Not enough memory; training on %u MB only...\n", (unsigned)(benchedSize >> 20));

    /* Load input buffer */
    nbFiles = DiB_loadFiles(srcBuffer, &benchedSize, fileSizes, fileNamesTable, nbFiles);
    DiB_fillNoise((char*)srcBuffer + benchedSize, NOISELENGTH);   /* guard band, for end of buffer condition */

    {   size_t const dictSize = ZDICT_trainFromBuffer_unsafe(dictBuffer, maxDictSize,
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
    }

    /* clean up */
_cleanup:
    free(srcBuffer);
    free(dictBuffer);
    free(fileSizes);
    return result;
}
