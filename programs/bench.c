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

/***************************************
*  Compiler Options
***************************************/
/* Disable some Visual warning messages */
#define _CRT_SECURE_NO_WARNINGS                  /* fopen */

// Unix Large Files support (>4GB)
#define _FILE_OFFSET_BITS 64
#if (defined(__sun__) && (!defined(__LP64__)))   // Sun Solaris 32-bits requires specific definitions
#  define _LARGEFILE_SOURCE
#elif ! defined(__LP64__)                        // No point defining Large file for 64 bit
#  define _LARGEFILE64_SOURCE
#endif

// S_ISREG & gettimeofday() are not supported by MSVC
#if defined(_MSC_VER) || defined(_WIN32)
#  define BMK_LEGACY_TIMER 1
#endif


/**************************************
*  Includes
**************************************/
#include <stdlib.h>      /* malloc, free */
#include <string.h>      /* memset */
#include <stdio.h>       // fprintf, fopen, ftello64
#include <sys/types.h>   // stat64
#include <sys/stat.h>    // stat64

// Use ftime() if gettimeofday() is not available on your target
#if defined(BMK_LEGACY_TIMER)
#  include <sys/timeb.h>   // timeb, ftime
#else
#  include <sys/time.h>    // gettimeofday
#endif

#include "zstd.h"
#include "xxhash.h"


/**************************************
*  Compiler specifics
**************************************/
#if !defined(S_ISREG)
#  define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif


/**************************************
* Basic Types
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


/**************************************
*  Macros
**************************************/
#define DISPLAY(...) fprintf(stderr, __VA_ARGS__)


/**************************************
*  Benchmark Parameters
**************************************/
static int nbIterations = NBLOOPS;

void BMK_SetNbIterations(int nbLoops)
{
    nbIterations = nbLoops;
    DISPLAY("- %i iterations -\n", nbIterations);
}


/*********************************************************
*  Private functions
*********************************************************/

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



/*********************************************************
*  Data generator
*********************************************************/
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


/*********************************************************
*  Bench functions
*********************************************************/

static int BMK_benchMem(void* srcBuffer, size_t srcSize, char* fileName, int cLevel)
{
    size_t maxCompressedSize = ZSTD_compressBound(srcSize);
    void* compressedBuffer = malloc(maxCompressedSize);
    void* resultBuffer = malloc(srcSize);
    U64 crcOrig;

    /* Init */
    (void)cLevel;

    /* Memory allocation & restrictions */
    if (!compressedBuffer || !resultBuffer)
    {
        DISPLAY("\nError: not enough memory!\n");
        free(compressedBuffer);
        free(resultBuffer);
        return 12;
    }

    /* Calculating input Checksum */
    crcOrig = XXH64(srcBuffer, srcSize, 0);

    /* warmimg up memory */
    BMK_datagen(compressedBuffer, maxCompressedSize, 0.10, 1);   /* warmimg up memory */

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

            /* Compression */
            DISPLAY("%1i-%-14.14s : %9u ->\r", loopNb, fileName, (U32)srcSize);
            memset(compressedBuffer, 0xE5, maxCompressedSize);

            nbLoops = 0;
            milliTime = BMK_GetMilliStart();
            while (BMK_GetMilliStart() == milliTime);
            milliTime = BMK_GetMilliStart();
            while (BMK_GetMilliSpan(milliTime) < TIMELOOP)
            {
                cSize = ZSTD_compress(compressedBuffer, maxCompressedSize, srcBuffer, srcSize);
                nbLoops++;
            }
            milliTime = BMK_GetMilliSpan(milliTime);

            if ((double)milliTime < fastestC*nbLoops) fastestC = (double)milliTime / nbLoops;
            ratio = (double)cSize / (double)srcSize*100.;
            DISPLAY("%1i-%-14.14s : %9i -> %9i (%5.2f%%),%7.1f MB/s\r", loopNb, fileName, (int)srcSize, (int)cSize, ratio, (double)srcSize / fastestC / 1000.);

#if 1
            /* Decompression */
            memset(resultBuffer, 0xD6, srcSize);

            nbLoops = 0;
            milliTime = BMK_GetMilliStart();
            while (BMK_GetMilliStart() == milliTime);
            milliTime = BMK_GetMilliStart();
            while (BMK_GetMilliSpan(milliTime) < TIMELOOP)
            {
                ZSTD_decompress(resultBuffer, srcSize, compressedBuffer, cSize);
                nbLoops++;
            }
            milliTime = BMK_GetMilliSpan(milliTime);

            if ((double)milliTime < fastestD*nbLoops) fastestD = (double)milliTime / nbLoops;
            DISPLAY("%1i-%-14.14s : %9i -> %9i (%5.2f%%),%7.1f MB/s ,%7.1f MB/s\r", loopNb, fileName, (int)srcSize, (int)cSize, ratio, (double)srcSize / fastestC / 1000., (double)srcSize / fastestD / 1000.);
#endif

            /* CRC Checking */
            crcCheck = XXH64(resultBuffer, srcSize, 0);
            if (crcOrig!=crcCheck)
            {
                unsigned i = 0;
                DISPLAY("\n!!! WARNING !!! %14s : Invalid Checksum : %x != %x\n", fileName, (unsigned)crcOrig, (unsigned)crcCheck);
                while (i<srcSize)
                {
                    if (((BYTE*)srcBuffer)[i] != ((BYTE*)resultBuffer)[i])
                    {
                        printf("\nDecoding error at pos %u   \n", i);
                        break;
                    }
                    i++;
                }
                break;
            }
        }

        if (crcOrig == crcCheck)
        {
            if (ratio<100.)
                DISPLAY("%-16.16s : %9i -> %9i (%5.2f%%),%7.1f MB/s ,%7.1f MB/s\n", fileName, (int)srcSize, (int)cSize, ratio, (double)srcSize / fastestC / 1000., (double)srcSize / fastestD / 1000.);
            else
                DISPLAY("%-16.16s : %9i -> %9i (%5.1f%%),%7.1f MB/s ,%7.1f MB/s \n", fileName, (int)srcSize, (int)cSize, ratio, (double)srcSize / fastestC / 1000., (double)srcSize / fastestD / 1000.);
        }
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
    int result;

    /* Init */
    (void)cLevel;

    // Check file existence
    inFile = fopen(inFileName, "rb");
    if (inFile == NULL)
    {
        DISPLAY("Pb opening %s\n", inFileName);
        return 11;
    }

    // Memory allocation & restrictions
    inFileSize = BMK_GetFileSize(inFileName);
    benchedSize = BMK_findMaxMem(inFileSize * 3) / 3;
    if ((U64)benchedSize > inFileSize) benchedSize = (size_t)inFileSize;
    if (benchedSize < inFileSize)
        DISPLAY("Not enough memory for '%s' full size; testing %i MB only...\n", inFileName, (int)(benchedSize >> 20));

    // Alloc
    srcBuffer = malloc(benchedSize);

    if (!srcBuffer)
    {
        DISPLAY("\nError: not enough memory!\n");
        free(srcBuffer);
        fclose(inFile);
        return 12;
    }

    // Fill input buffer
    DISPLAY("Loading %s...       \r", inFileName);
    readSize = fread(srcBuffer, 1, benchedSize, inFile);
    fclose(inFile);

    if (readSize != benchedSize)
    {
        DISPLAY("\nError: problem reading file '%s' !!    \n", inFileName);
        free(srcBuffer);
        return 13;
    }

    // Bench
    result = BMK_benchMem(srcBuffer, benchedSize, inFileName, cLevel);

    // End
    free(srcBuffer);
    DISPLAY("\n");
    return result;
}


static int BMK_syntheticTest(int cLevel, double compressibility)
{
    size_t benchedSize = 10000000;
    void* srcBuffer = malloc(benchedSize);
    int result;
    char name[20] = {0};

    /* Init */
    (void)cLevel;

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
    result = BMK_benchMem(srcBuffer, benchedSize, name, cLevel);

    /* End */
    free(srcBuffer);
    DISPLAY("\n");
    return result;
}


int BMK_bench(char** fileNamesTable, unsigned nbFiles, unsigned cLevel)
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

