/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

/*
  This program takes a file in input,
  performs a zstd round-trip test (compression - decompress)
  compares the result with original
  and generates a crash (double free) on corruption detection.
*/

/*===========================================
*   Dependencies
*==========================================*/
#include <stddef.h>     /* size_t */
#include <stdlib.h>     /* malloc, free, exit */
#include <stdio.h>      /* fprintf */
#include <sys/types.h>  /* stat */
#include <sys/stat.h>   /* stat */
#include "xxhash.h"
#include "zstd.h"

/*===========================================
*   Macros
*==========================================*/
#define MIN(a,b)  ( (a) < (b) ? (a) : (b) )

/** roundTripTest() :
*   Compresses `srcBuff` into `compressedBuff`,
*   then decompresses `compressedBuff` into `resultBuff`.
*   Compression level used is derived from first content byte.
*   @return : result of decompression, which should be == `srcSize`
*          or an error code if either compression or decompression fails.
*   Note : `compressedBuffCapacity` should be `>= ZSTD_compressBound(srcSize)`
*          for compression to be guaranteed to work */
static size_t roundTripTest(void* resultBuff, size_t resultBuffCapacity,
                            void* compressedBuff, size_t compressedBuffCapacity,
                      const void* srcBuff, size_t srcBuffSize)
{
    static const int maxClevel = 19;
    size_t const hashLength = MIN(128, srcBuffSize);
    unsigned const h32 = XXH32(srcBuff, hashLength, 0);
    int const cLevel = h32 % maxClevel;
    size_t const cSize = ZSTD_compress(compressedBuff, compressedBuffCapacity, srcBuff, srcBuffSize, cLevel);
    if (ZSTD_isError(cSize)) {
        fprintf(stderr, "Compression error : %s \n", ZSTD_getErrorName(cSize));
        return cSize;
    }
    return ZSTD_decompress(resultBuff, resultBuffCapacity, compressedBuff, cSize);
}


static size_t checkBuffers(const void* buff1, const void* buff2, size_t buffSize)
{
    const char* ip1 = (const char*)buff1;
    const char* ip2 = (const char*)buff2;
    size_t pos;

    for (pos=0; pos<buffSize; pos++)
        if (ip1[pos]!=ip2[pos])
            break;

    return pos;
}


static void roundTripCheck(const void* srcBuff, size_t srcBuffSize)
{
    size_t const cBuffSize = ZSTD_compressBound(srcBuffSize);
    void* cBuff = malloc(cBuffSize);
    void* rBuff = malloc(cBuffSize);
    #define CRASH { free(cBuff); free(cBuff); }   /* double free, to crash program */

    if (!cBuff || !rBuff) {
        fprintf(stderr, "not enough memory ! \n");
        exit (1);
    }

    {   size_t const result = roundTripTest(rBuff, cBuffSize, cBuff, cBuffSize, srcBuff, srcBuffSize);
        if (ZSTD_isError(result)) {
            fprintf(stderr, "roundTripTest error : %s \n", ZSTD_getErrorName(result));
            CRASH;
        }
        if (result != srcBuffSize) {
            fprintf(stderr, "Incorrect regenerated size : %u != %u\n", (unsigned)result, (unsigned)srcBuffSize);
            CRASH;
        }
        if (checkBuffers(srcBuff, rBuff, srcBuffSize) != srcBuffSize) {
            fprintf(stderr, "Silent decoding corruption !!!");
            CRASH;
        }
    }

    free(cBuff);
    free(rBuff);
}


static size_t getFileSize(const char* infilename)
{
    int r;
#if defined(_MSC_VER)
    struct _stat64 statbuf;
    r = _stat64(infilename, &statbuf);
    if (r || !(statbuf.st_mode & S_IFREG)) return 0;   /* No good... */
#else
    struct stat statbuf;
    r = stat(infilename, &statbuf);
    if (r || !S_ISREG(statbuf.st_mode)) return 0;   /* No good... */
#endif
    return (size_t)statbuf.st_size;
}


static int isDirectory(const char* infilename)
{
    int r;
#if defined(_MSC_VER)
    struct _stat64 statbuf;
    r = _stat64(infilename, &statbuf);
    if (!r && (statbuf.st_mode & _S_IFDIR)) return 1;
#else
    struct stat statbuf;
    r = stat(infilename, &statbuf);
    if (!r && S_ISDIR(statbuf.st_mode)) return 1;
#endif
    return 0;
}


/** loadFile() :
*   requirement : `buffer` size >= `fileSize` */
static void loadFile(void* buffer, const char* fileName, size_t fileSize)
{
    FILE* const f = fopen(fileName, "rb");
    if (isDirectory(fileName)) {
        fprintf(stderr, "Ignoring %s directory \n", fileName);
        exit(2);
    }
    if (f==NULL) {
        fprintf(stderr, "Impossible to open %s \n", fileName);
        exit(3);
    }
    {   size_t const readSize = fread(buffer, 1, fileSize, f);
        if (readSize != fileSize) {
            fprintf(stderr, "Error reading %s \n", fileName);
            exit(5);
    }   }
    fclose(f);
}


static void fileCheck(const char* fileName)
{
    size_t const fileSize = getFileSize(fileName);
    void* buffer = malloc(fileSize);
    if (!buffer) {
        fprintf(stderr, "not enough memory \n");
        exit(4);
    }
    loadFile(buffer, fileName, fileSize);
    roundTripCheck(buffer, fileSize);
    free (buffer);
}

int main(int argCount, const char** argv) {
    if (argCount < 2) {
        fprintf(stderr, "Error : no argument : need input file \n");
        exit(9);
    }
    fileCheck(argv[1]);
    fprintf(stderr, "no pb detected\n");
    return 0;
}
