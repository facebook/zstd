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

typedef struct {
    void* fBuffer;
    void* cBuffer;
    size_t fBufferSize;
    size_t cBufferSize;
    ZSTD_CCtx* cctx;
} resources;

/*
 * allocate memory for buffers big enough to compress all files
 * as well as memory for output file name (ofn)
 */
static resources createResources_orDie(int argc, const char** argv, char **ofn, int* ofnBufferLen)
{
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

    resources ress;
    ress.fBufferSize = maxFileSize;
    ress.cBufferSize = ZSTD_compressBound(maxFileSize);

    *ofnBufferLen = maxFilenameLength + 5;
    *ofn = (char*)malloc_orDie(*ofnBufferLen);
    ress.fBuffer = malloc_orDie(ress.fBufferSize);
    ress.cBuffer = malloc_orDie(ress.cBufferSize);
    ress.cctx = ZSTD_createCCtx();
    if (ress.cctx==NULL) { fprintf(stderr, "ZSTD_createCCtx() error \n"); exit(10); }
    return ress;
}

static void freeResources(resources ress, char *outFilename)
{
    free(ress.fBuffer);
    free(ress.cBuffer);
    ZSTD_freeCCtx(ress.cctx);   /* never fails */
    free(outFilename);
}

/* compress with pre-allocated context (ZSTD_CCtx) and input/output buffers*/
static void compressFile_orDie(resources ress, const char* fname, const char* oname)
{
    size_t fSize = loadFile_orDie(fname, ress.fBuffer, ress.fBufferSize);

    size_t const cSize = ZSTD_compressCCtx(ress.cctx, ress.cBuffer, ress.cBufferSize, ress.fBuffer, fSize, 1);
    if (ZSTD_isError(cSize)) {
        fprintf(stderr, "error compressing %s : %s \n", fname, ZSTD_getErrorName(cSize));
        exit(8);
    }

    saveFile_orDie(oname, ress.cBuffer, cSize);

    /* success */
    // printf("%25s : %6u -> %7u - %s \n", fname, (unsigned)fSize, (unsigned)cSize, oname);
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

    /* memory allocation for outFilename and resources */
    char* outFilename;
    int outFilenameBufferLen;
    resources const ress = createResources_orDie(argc, argv, &outFilename, &outFilenameBufferLen); 

    /* compress files with shared context, input and output buffers */
    int argNb;
    for (argNb = 1; argNb < argc; argNb++) {
        const char* const inFilename = argv[argNb];
        int inFilenameLen = strlen(inFilename);
        assert(inFilenameLen + 5 <= outFilenameBufferLen);
        memcpy(outFilename, inFilename, inFilenameLen);
        memcpy(outFilename+inFilenameLen, ".zst", 5);
        compressFile_orDie(ress, inFilename, outFilename);
    }

    /* free momery */
    freeResources(ress,outFilename);

    printf("compressed %i files \n", argc-1);

    return 0;
}
