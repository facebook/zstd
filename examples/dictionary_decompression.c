/*
  Dictionary decompression
  Educational program using zstd library
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
    fclose(inFile);
    *size = buffSize;
    return buffer;
}

/* createDict() :
   `dictFileName` is supposed to have been created using `zstd --train` */
static const ZSTD_DDict* createDict(const char* dictFileName)
{
    size_t dictSize;
    void* const dictBuffer = loadFile_X(dictFileName, &dictSize);
    const ZSTD_DDict* const ddict = ZSTD_createDDict(dictBuffer, dictSize);
    free(dictBuffer);
    return ddict;
}


static void decompress(const char* fname, const ZSTD_DDict* ddict)
{
    size_t cSize;
    void* const cBuff = loadFile_X(fname, &cSize);
    unsigned long long const rSize = ZSTD_getDecompressedSize(cBuff, cSize);
    if (rSize==0) {
        printf("%s : original size unknown \n", fname);
        exit(5);
    }
    void* const rBuff = malloc_X(rSize);

    ZSTD_DCtx* const dctx = ZSTD_createDCtx();
    size_t const dSize = ZSTD_decompress_usingDDict(dctx, rBuff, rSize, cBuff, cSize, ddict);

    if (dSize != rSize) {
        printf("error decoding %s : %s \n", fname, ZSTD_getErrorName(dSize));
        exit(7);
    }

    /* success */
    printf("%25s : %6u -> %7u \n", fname, (unsigned)cSize, (unsigned)rSize);

    ZSTD_freeDCtx(dctx);
    free(rBuff);
    free(cBuff);
}


int main(int argc, const char** argv)
{
    const char* const exeName = argv[0];

    if (argc<3) {
        printf("wrong arguments\n");
        printf("usage:\n");
        printf("%s [FILES] dictionary\n", exeName);
        return 1;
    }

    /* load dictionary only once */
    const char* const dictName = argv[argc-1];
    const ZSTD_DDict* const dictPtr = createDict(dictName);

    int u;
    for (u=1; u<argc-1; u++) decompress(argv[u], dictPtr);

    printf("All %u files decoded. \n", argc-2);
}
