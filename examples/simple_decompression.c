/**
 * Copyright 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the license found in the
 * LICENSE-examples file in the root directory of this source tree.
 */



#include <stdlib.h>    // malloc, exit
#include <stdio.h>     // printf
#include <string.h>    // strerror
#include <errno.h>     // errno
#include <sys/stat.h>  // stat
#include <zstd.h>      // presumes zstd library is installed


static off_t fsize_X(const char *filename)
{
    struct stat st;
    if (stat(filename, &st) == 0) return st.st_size;
    /* error */
    printf("stat: %s : %s \n", filename, strerror(errno));
    exit(1);
}

static FILE* fopen_X(const char *filename, const char *instruction)
{
    FILE* const inFile = fopen(filename, instruction);
    if (inFile) return inFile;
    /* error */
    printf("fopen: %s : %s \n", filename, strerror(errno));
    exit(2);
}

static void* malloc_X(size_t size)
{
    void* const buff = malloc(size);
    if (buff) return buff;
    /* error */
    printf("malloc: %s \n", strerror(errno));
    exit(3);
}

static void* loadFile_X(const char* fileName, size_t* size)
{
    off_t const buffSize = fsize_X(fileName);
    FILE* const inFile = fopen_X(fileName, "rb");
    void* const buffer = malloc_X(buffSize);
    size_t const readSize = fread(buffer, 1, buffSize, inFile);
    if (readSize != (size_t)buffSize) {
        printf("fread: %s : %s \n", fileName, strerror(errno));
        exit(4);
    }
    fclose(inFile);   /* can't fail (read only) */
    *size = buffSize;
    return buffer;
}


static void decompress(const char* fname)
{
    size_t cSize;
    void* const cBuff = loadFile_X(fname, &cSize);
    unsigned long long const rSize = ZSTD_getDecompressedSize(cBuff, cSize);
    if (rSize==0) {
        printf("%s : original size unknown. Use streaming decompression instead. \n", fname);
        exit(5);
    }
    void* const rBuff = malloc_X((size_t)rSize);

    size_t const dSize = ZSTD_decompress(rBuff, rSize, cBuff, cSize);

    if (dSize != rSize) {
        printf("error decoding %s : %s \n", fname, ZSTD_getErrorName(dSize));
        exit(7);
    }

    /* success */
    printf("%25s : %6u -> %7u \n", fname, (unsigned)cSize, (unsigned)rSize);

    free(rBuff);
    free(cBuff);
}


int main(int argc, const char** argv)
{
    const char* const exeName = argv[0];

    if (argc!=2) {
        printf("wrong arguments\n");
        printf("usage:\n");
        printf("%s FILE\n", exeName);
        return 1;
    }

    decompress(argv[1]);

    printf("%s correctly decoded (in memory). \n", argv[1]);

    return 0;
}
