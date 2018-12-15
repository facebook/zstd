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

    size_t const cSize = ZSTD_compressCCtx(cctx, cBuff, cBuffSize, fBuff, fBuffSize, 1);
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

int main(int argc, const char** argv)
{
    const char* const exeName = argv[0];

    if (argc<2) {
        printf("wrong arguments\n");
        printf("usage:\n");
        printf("%s FILE(s)\n", exeName);
        return 1;
    }

    /* pre-calculate buffer sizes needed to handle all files */
    size_t maxFileNameLength=0;
    size_t maxFileSize = 0;
    size_t maxCBufferSize = 0;

    int argNb;
    for (argNb = 1; argNb < argc; argNb++) {
      const char* const fileName = argv[argNb];
      size_t const fileNameLength = strlen(fileName);
      size_t const fileSize = fsize_orDie(fileName);

      if (fileNameLength > maxFileNameLength) maxFileNameLength = fileNameLength;
      if (fileSize > maxFileSize) maxFileSize = fileSize;
    }
    maxCBufferSize = ZSTD_compressBound(maxFileSize);

    /* allocate memory for output file name, input/output buffers for all compression tasks */
    char* const outFilename = (char*)malloc_orDie(maxFileNameLength + 5);
    void* const fBuffer = malloc_orDie(maxFileSize);
    void* const cBuffer = malloc_orDie(maxCBufferSize);

    /* create a compression context (ZSTD_CCtx) for all compression tasks */
    ZSTD_CCtx* const cctx = ZSTD_createCCtx();
    if (cctx==NULL) { fprintf(stderr, "ZSTD_createCCtx() error \n"); exit(10); }

    /* compress files with shared context, input and output buffers */
    for (argNb = 1; argNb < argc; argNb++) {
      const char* const inFilename = argv[argNb];
      getOutFilename(inFilename, outFilename);
      compressExpress_orDie(inFilename, outFilename, cctx, cBuffer, maxCBufferSize, fBuffer, maxFileSize);
    }

    /* free momery resources */
    free(outFilename);
    free(fBuffer);
    free(cBuffer);
    ZSTD_freeCCtx(cctx);   /* never fails */

    printf("compressed %i files \n", argc-1);

    return 0;
}
