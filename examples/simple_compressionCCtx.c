/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include <stdlib.h>    // malloc, free, exit
#include <stdio.h>     // fprintf, perror, fopen, etc.
#include <string.h>    // strlen, strcat, memset, strerror
#include <errno.h>     // errno
#include <sys/stat.h>  // stat
#include <zstd.h>      // presumes zstd library is installed
#include "utils.h"

/* compress with pre-allocated context (ZSTD_CCtx) and input/output buffers*/
static void compressExpress_orDie(const char* fname, const char* oname,
                                  ZSTD_CCtx* cctx, void* cBuff, size_t cBuffSize, void* fBuff, size_t fBuffSize)
{
    size_t fSize;
    loadFile_orDie(fname, &fSize, fBuff, fBuffSize);

    size_t const cSize = ZSTD_compressCCtx(cctx, cBuff, cBuffSize, fBuff, fSize, 1);
    if (ZSTD_isError(cSize)) {
        fprintf(stderr, "error compressing %s : %s \n", fname, ZSTD_getErrorName(cSize));
        exit(8);
    }

    saveFile_orDie(oname, cBuff, cSize);

    /* success */
    printf("%25s : %6u -> %7u - %s \n", fname, (unsigned)fSize, (unsigned)cSize, oname);
}

static void getOutFilename(const char* const filename, char* const outFilename)
{
    memset(outFilename, 0, 1);
    strcat(outFilename, filename);
    strcat(outFilename, ".zst");
}

/* allocate memory for buffers big enough to compress all files
 * as well as memory for output file names (outFileName)
 */
void allocMemory_orDie(int argc, const char** argv, char** outFilename,
                       void** cBuffer, size_t* cBufferSize, void** fBuffer, size_t* fBufferSize) {
    size_t maxFilenameLength=0;
    size_t maxFileSize = 0;

    int argNb;
    for (argNb = 1; argNb < argc; argNb++) {
      const char* const filename = argv[argNb];
      size_t const filenameLength = strlen(filename);
      size_t const fileSize = fsize_orDie(filename);

      if (filenameLength > maxFilenameLength) maxFilenameLength = filenameLength;
      if (fileSize > maxFileSize) maxFileSize = fileSize;
    }
    *cBufferSize = ZSTD_compressBound(maxFileSize);
    *fBufferSize = maxFileSize;

    /* allocate memory for output file name, input/output buffers for all compression tasks */
    *outFilename = (char*)malloc_orDie(maxFilenameLength + 5);
    *cBuffer = malloc_orDie(*cBufferSize);
    *fBuffer = malloc_orDie(*fBufferSize);
}

int main(int argc, const char** argv)
{
    const char* const exeName = argv[0];

    if (argc<2) {
        printf("wrong arguments\n");
        printf("usage:\n");
        printf("%s FILE(s)\n", exeName);
        return 1;
    }

    /* allocate memory for buffers big enough to compress all files
     * as well as memory for output file name (outFileName)
     *    fBuffer - buffer for input file data
     *    cBuffer - buffer for compressed data 
     */
    char* outFilename;
    void* fBuffer;
    void* cBuffer;
    size_t fBufferSize;
    size_t cBufferSize;
    allocMemory_orDie(argc, argv, &outFilename, &cBuffer, &cBufferSize, &fBuffer, &fBufferSize); 

    /* create a compression context (ZSTD_CCtx) for all compression tasks */
    ZSTD_CCtx* const cctx = ZSTD_createCCtx();
    if (cctx==NULL) { fprintf(stderr, "ZSTD_createCCtx() error \n"); exit(10); }

    /* compress files with shared context, input and output buffers */
    int argNb;
    for (argNb = 1; argNb < argc; argNb++) {
      const char* const inFilename = argv[argNb];
      getOutFilename(inFilename, outFilename);
      compressExpress_orDie(inFilename, outFilename, cctx, cBuffer, cBufferSize, fBuffer, fBufferSize);
    }

    /* free momery resources */
    free(outFilename);
    free(fBuffer);
    free(cBuffer);
    ZSTD_freeCCtx(cctx);   /* never fails */

    printf("compressed %i files \n", argc-1);

    return 0;
}
