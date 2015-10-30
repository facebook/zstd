/*
    bench.c - Demo module to benchmark open-source compression algorithms
    Copyright (C) Yann Collet 2012-2015

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
    - ztsd public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/* **************************************
*  Compiler Options
****************************************/
/* Disable some Visual warning messages */
#define _CRT_SECURE_NO_WARNINGS                  /* fopen */

/* Unix Large Files support (>4GB) */
#define _FILE_OFFSET_BITS 64
#if (defined(__sun__) && (!defined(__LP64__)))   /* Sun Solaris 32-bits requires specific definitions */
#  define _LARGEFILE_SOURCE
#elif ! defined(__LP64__)                        /* No point defining Large file for 64 bit */
#  define _LARGEFILE64_SOURCE
#endif

/* S_ISREG & gettimeofday() are not supported by MSVC */
#if defined(_MSC_VER) || defined(_WIN32)
#  define BMK_LEGACY_TIMER 1
#endif


/* *************************************
*  Includes
***************************************/
#include <stdlib.h>      /* malloc, free */
#include <string.h>      /* memset */
#include <stdio.h>       /* fprintf, fopen, ftello64 */
#include <sys/types.h>   /* stat64 */
#include <sys/stat.h>    /* stat64 */

/* Use ftime() if gettimeofday() is not available */
#if defined(BMK_LEGACY_TIMER)
#  include <sys/timeb.h>   /* timeb, ftime */
#else
#  include <sys/time.h>    /* gettimeofday */
#endif

#include "mem.h"
#include "zstd.h"
#include "zstdhc.h"
#include "xxhash.h"


/* *************************************
*  Compiler specifics
***************************************/
#if !defined(S_ISREG)
#  define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif


/* *************************************
*  Constants
***************************************/
#define NBLOOPS    3
#define TIMELOOP   2500

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define MAX_MEM             (2 GB - 64 MB)
#define DEFAULT_CHUNKSIZE   (4 MB)

static U32 g_compressibilityDefault = 50;
static U32 prime1 = 2654435761U;
static U32 prime2 = 2246822519U;


/* *************************************
*  Macros
***************************************/
#define DISPLAY(...) fprintf(stderr, __VA_ARGS__)


/* *************************************
*  Benchmark Parameters
***************************************/
static int nbIterations = NBLOOPS;
static size_t g_blockSize = 0;

void BMK_SetNbIterations(int nbLoops)
{
    nbIterations = nbLoops;
    DISPLAY("- %i iterations -\n", nbIterations);
}

void BMK_SetBlockSize(size_t blockSize)
{
    g_blockSize = blockSize;
    DISPLAY("using blocks of size %u KB \n", (U32)(blockSize>>10));
}


/* ********************************************************
*  Private functions
**********************************************************/

#if defined(BMK_LEGACY_TIMER)

static int BMK_GetMilliStart(void)
{
  /* Based on Legacy ftime()
  *  Rolls over every ~ 12.1 days (0x100000/24/60/60)
  *  Use GetMilliSpan to correct for rollover */
  struct timeb tb;
  int nCount;
  ftime( &tb );
  nCount = (int) (tb.millitm + (tb.time & 0xfffff) * 1000);
  return nCount;
}

#else

static int BMK_GetMilliStart(void)
{
  /* Based on newer gettimeofday()
  *  Use GetMilliSpan to correct for rollover */
  struct timeval tv;
  int nCount;
  gettimeofday(&tv, NULL);
  nCount = (int) (tv.tv_usec/1000 + (tv.tv_sec & 0xfffff) * 1000);
  return nCount;
}

#endif


static int BMK_GetMilliSpan( int nTimeStart )
{
  int nSpan = BMK_GetMilliStart() - nTimeStart;
  if ( nSpan < 0 )
    nSpan += 0x100000 * 1000;
  return nSpan;
}


/* ********************************************************
*  Data generator
**********************************************************/
/* will hopefully be converted into ROL instruction by compiler */
static U32 BMK_rotl32(unsigned val32, unsigned nbBits) { return((val32 << nbBits) | (val32 >> (32 - nbBits))); }

static U32 BMK_rand(U32* src)
{
    U32 rand32 = *src;
    rand32 *= prime1;
    rand32 += prime2;
    rand32 = BMK_rotl32(rand32, 13);
    *src = rand32;
    return rand32 >> 9;
}

#define BMK_RAND15BITS  ( BMK_rand(&seed) & 0x7FFF)
#define BMK_RANDLENGTH  ((BMK_rand(&seed) & 3) ? (BMK_rand(&seed) % 15) : (BMK_rand(&seed) % 510) + 15)
#define BMK_RANDCHAR    (BYTE)((BMK_rand(&seed) & 63) + '0')
static void BMK_datagen(void* buffer, size_t bufferSize, double proba, U32 seed)
{
    BYTE* BBuffer = (BYTE*)buffer;
    unsigned pos = 0;
    U32 P32 = (U32)(32768 * proba);

    /* First Byte */
    BBuffer[pos++] = BMK_RANDCHAR;

    while (pos < bufferSize)
    {
        /* Select : Literal (noise) or copy (within 64K) */
        if (BMK_RAND15BITS < P32)
        {
            /* Match */
            size_t match, end;
            unsigned length = BMK_RANDLENGTH + 4;
            unsigned offset = BMK_RAND15BITS + 1;
            if (offset > pos) offset = pos;
            match = pos - offset;
            end = pos + length;
            if (end > bufferSize) end = bufferSize;
            while (pos < end) BBuffer[pos++] = BBuffer[match++];
        }
        else
        {
            /* Literal */
            size_t end;
            unsigned length = BMK_RANDLENGTH;
            end = pos + length;
            if (end > bufferSize) end = bufferSize;
            while (pos < end) BBuffer[pos++] = BMK_RANDCHAR;
        }
    }
}


/* ********************************************************
*  Bench functions
**********************************************************/
typedef struct
{
    char*  srcPtr;
    size_t srcSize;
    char*  cPtr;
    size_t cRoom;
    size_t cSize;
    char*  resPtr;
    size_t resSize;
} blockParam_t;

typedef size_t (*compressor_t) (void* dst, size_t maxDstSize, const void* src, size_t srcSize, int compressionLevel);

static size_t local_compress_fast (void* dst, size_t maxDstSize, const void* src, size_t srcSize, int compressionLevel)
{
    (void)compressionLevel;
    return ZSTD_compress(dst, maxDstSize, src, srcSize);
}

#define MIN(a,b) (a<b ? a : b)

static int BMK_benchMem(void* srcBuffer, size_t srcSize, const char* fileName, int cLevel)
{
    const size_t blockSize = g_blockSize ? g_blockSize : srcSize;
    const U32 nbBlocks = (U32) ((srcSize + (blockSize-1)) / blockSize);
    blockParam_t* const blockTable = (blockParam_t*) malloc(nbBlocks * sizeof(blockParam_t));
    const size_t maxCompressedSize = (size_t)nbBlocks * ZSTD_compressBound(blockSize);
    void* const compressedBuffer = malloc(maxCompressedSize);
    void* const resultBuffer = malloc(srcSize);
    const compressor_t compressor = (cLevel <= 1) ? local_compress_fast : ZSTD_HC_compress;
    U64 crcOrig;

    /* init */
    if (strlen(fileName)>16)
        fileName += strlen(fileName)-16;

    /* Memory allocation & restrictions */
    if (!compressedBuffer || !resultBuffer || !blockTable)
    {
        DISPLAY("\nError: not enough memory!\n");
        free(compressedBuffer);
        free(resultBuffer);
        free(blockTable);
        return 12;
    }

    /* Calculating input Checksum */
    crcOrig = XXH64(srcBuffer, srcSize, 0);

    /* Init blockTable data */
    {
        U32 i;
        size_t remaining = srcSize;
        char* srcPtr = (char*)srcBuffer;
        char* cPtr = (char*)compressedBuffer;
        char* resPtr = (char*)resultBuffer;
        for (i=0; i<nbBlocks; i++)
        {
            size_t thisBlockSize = MIN(remaining, blockSize);
            blockTable[i].srcPtr = srcPtr;
            blockTable[i].cPtr = cPtr;
            blockTable[i].resPtr = resPtr;
            blockTable[i].srcSize = thisBlockSize;
            blockTable[i].cRoom = ZSTD_compressBound(thisBlockSize);
            srcPtr += thisBlockSize;
            cPtr += blockTable[i].cRoom;
            resPtr += thisBlockSize;
            remaining -= thisBlockSize;
        }
    }

    /* warmimg up memory */
    BMK_datagen(compressedBuffer, maxCompressedSize, 0.10, 1);

    /* Bench */
    {
        int loopNb;
        size_t cSize = 0;
        double fastestC = 100000000., fastestD = 100000000.;
        double ratio = 0.;
        U64 crcCheck = 0;

        DISPLAY("\r%79s\r", "");
        for (loopNb = 1; loopNb <= nbIterations; loopNb++)
        {
            int nbLoops;
            int milliTime;
            U32 blockNb;

            /* Compression */
            DISPLAY("%2i-%-17.17s :%10u ->\r", loopNb, fileName, (U32)srcSize);
            memset(compressedBuffer, 0xE5, maxCompressedSize);

            nbLoops = 0;
            milliTime = BMK_GetMilliStart();
            while (BMK_GetMilliStart() == milliTime);
            milliTime = BMK_GetMilliStart();
            while (BMK_GetMilliSpan(milliTime) < TIMELOOP)
            {
                for (blockNb=0; blockNb<nbBlocks; blockNb++)
                    blockTable[blockNb].cSize = compressor(blockTable[blockNb].cPtr,  blockTable[blockNb].cRoom, blockTable[blockNb].srcPtr,blockTable[blockNb].srcSize, cLevel);
                nbLoops++;
            }
            milliTime = BMK_GetMilliSpan(milliTime);

            cSize = 0;
            for (blockNb=0; blockNb<nbBlocks; blockNb++)
                cSize += blockTable[blockNb].cSize;
            if ((double)milliTime < fastestC*nbLoops) fastestC = (double)milliTime / nbLoops;
            ratio = (double)srcSize / (double)cSize;
            DISPLAY("%2i-%-17.17s :%10i ->%10i (%5.3f),%6.1f MB/s\r", loopNb, fileName, (int)srcSize, (int)cSize, ratio, (double)srcSize / fastestC / 1000.);

#if 1
            /* Decompression */
            memset(resultBuffer, 0xD6, srcSize);

            nbLoops = 0;
            milliTime = BMK_GetMilliStart();
            while (BMK_GetMilliStart() == milliTime);
            milliTime = BMK_GetMilliStart();
            for ( ; BMK_GetMilliSpan(milliTime) < TIMELOOP; nbLoops++)
            {
                for (blockNb=0; blockNb<nbBlocks; blockNb++)
                    blockTable[blockNb].resSize = ZSTD_decompress(blockTable[blockNb].resPtr, blockTable[blockNb].srcSize,
                                                                  blockTable[blockNb].cPtr, blockTable[blockNb].cSize);
            }
            milliTime = BMK_GetMilliSpan(milliTime);

            if ((double)milliTime < fastestD*nbLoops) fastestD = (double)milliTime / nbLoops;
            DISPLAY("%2i-%-17.17s :%10i ->%10i (%5.3f),%6.1f MB/s ,%6.1f MB/s\r", loopNb, fileName, (int)srcSize, (int)cSize, ratio, (double)srcSize / fastestC / 1000., (double)srcSize / fastestD / 1000.);

            /* CRC Checking */
            crcCheck = XXH64(resultBuffer, srcSize, 0);
            if (crcOrig!=crcCheck)
            {
                unsigned u;
                unsigned eBlockSize = (unsigned)(MIN(65536*2, blockSize));
                DISPLAY("\n!!! WARNING !!! %14s : Invalid Checksum : %x != %x\n", fileName, (unsigned)crcOrig, (unsigned)crcCheck);
                for (u=0; u<srcSize; u++)
                {
                    if (((BYTE*)srcBuffer)[u] != ((BYTE*)resultBuffer)[u])
                    {
                        printf("Decoding error at pos %u (block %u, pos %u) \n", u, u / eBlockSize, u % eBlockSize);
                        break;
                    }
                }
                break;
            }
#endif
        }

        if (crcOrig == crcCheck)
            DISPLAY("%2i-%-17.17s :%10i ->%10i (%5.3f),%6.1f MB/s ,%6.1f MB/s \n", cLevel, fileName, (int)srcSize, (int)cSize, ratio, (double)srcSize / fastestC / 1000., (double)srcSize / fastestD / 1000.);
    }

    /* End cleaning */
    free(compressedBuffer);
    free(resultBuffer);
    return 0;
}


static U64 BMK_GetFileSize(char* infilename)
{
    int r;
#if defined(_MSC_VER)
    struct _stat64 statbuf;
    r = _stat64(infilename, &statbuf);
#else
    struct stat statbuf;
    r = stat(infilename, &statbuf);
#endif
    if (r || !S_ISREG(statbuf.st_mode)) return 0;   /* No good... */
    return (U64)statbuf.st_size;
}

static size_t BMK_findMaxMem(U64 requiredMem)
{
    size_t step = 64 MB;
    BYTE* testmem = NULL;

    requiredMem = (((requiredMem >> 26) + 1) << 26);
    requiredMem += 2 * step;
    if (requiredMem > MAX_MEM) requiredMem = MAX_MEM;

    while (!testmem)
    {
        requiredMem -= step;
        testmem = (BYTE*)malloc((size_t)requiredMem);
    }

    free(testmem);
    return (size_t)(requiredMem - step);
}

static int BMK_benchOneFile(char* inFileName, int cLevel)
{
    FILE*  inFile;
    U64    inFileSize;
    size_t benchedSize, readSize;
    void* srcBuffer;
    int result=0;

    /* Check file existence */
    inFile = fopen(inFileName, "rb");
    if (inFile == NULL)
    {
        DISPLAY("Pb opening %s\n", inFileName);
        return 11;
    }

    /* Memory allocation & restrictions */
    inFileSize = BMK_GetFileSize(inFileName);
    benchedSize = BMK_findMaxMem(inFileSize * 3) / 3;
    if ((U64)benchedSize > inFileSize) benchedSize = (size_t)inFileSize;
    if (benchedSize < inFileSize)
        DISPLAY("Not enough memory for '%s' full size; testing %i MB only...\n", inFileName, (int)(benchedSize >> 20));
    srcBuffer = malloc(benchedSize);
    if (!srcBuffer)
    {
        DISPLAY("\nError: not enough memory!\n");
        fclose(inFile);
        return 12;
    }

    /* Fill input buffer */
    DISPLAY("Loading %s...       \r", inFileName);
    readSize = fread(srcBuffer, 1, benchedSize, inFile);
    fclose(inFile);

    if (readSize != benchedSize)
    {
        DISPLAY("\nError: problem reading file '%s' !!    \n", inFileName);
        free(srcBuffer);
        return 13;
    }

    /* Bench */
    if (cLevel<0)
    {
        int l;
        for (l=1; l <= -cLevel; l++)
            result = BMK_benchMem(srcBuffer, benchedSize, inFileName, l);
    }
    else
        result = BMK_benchMem(srcBuffer, benchedSize, inFileName, cLevel);

    /* clean up */
    free(srcBuffer);
    DISPLAY("\n");
    return result;
}


static int BMK_syntheticTest(int cLevel, double compressibility)
{
    size_t benchedSize = 10000000;
    void* srcBuffer = malloc(benchedSize);
    int result=0;
    char name[20] = {0};

    /* Memory allocation */
    if (!srcBuffer)
    {
        DISPLAY("\nError: not enough memory!\n");
        free(srcBuffer);
        return 12;
    }

    /* Fill input buffer */
    BMK_datagen(srcBuffer, benchedSize, compressibility, 0);

    /* Bench */
#ifdef _MSC_VER
    sprintf_s(name, 20, "Synthetic %2u%%", (unsigned)(compressibility*100));
#else
    snprintf (name, 20, "Synthetic %2u%%", (unsigned)(compressibility*100));
#endif
    /* Bench */
    if (cLevel<0)
    {
        int l;
        for (l=1; l <= -cLevel; l++)
            result = BMK_benchMem(srcBuffer, benchedSize, name, l);
    }
    else
        result = BMK_benchMem(srcBuffer, benchedSize, name, cLevel);

    /* End */
    free(srcBuffer);
    DISPLAY("\n");
    return result;
}


int BMK_benchFiles(char** fileNamesTable, unsigned nbFiles, unsigned cLevel)
{
    double compressibility = (double)g_compressibilityDefault / 100;

    if (nbFiles == 0)
    {
        BMK_syntheticTest(cLevel, compressibility);
    }
    else
    {
        /* Loop for each file */
        unsigned fileIdx = 0;
        while (fileIdx<nbFiles)
        {
            BMK_benchOneFile(fileNamesTable[fileIdx], cLevel);
            fileIdx++;
        }
    }
    return 0;
}

