/**
 * Copyright 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the license found in the
 * LICENSE-examples file in the root directory of this source tree.
 */


#include <stdlib.h>    // malloc, exit
#include <stdio.h>     // fprintf, perror, feof
#include <string.h>    // strerror
#include <errno.h>     // errno
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>      // presumes zstd library is installed
#include <zstd_errors.h>


static void* malloc_orDie(size_t size)
{
    void* const buff = malloc(size);
    if (buff) return buff;
    /* error */
    perror("malloc");
    exit(1);
}

static void* realloc_orDie(void* ptr, size_t size)
{
    ptr = realloc(ptr, size);
    if (ptr) return ptr;
    /* error */
    perror("realloc");
    exit(1);
}

static FILE* fopen_orDie(const char *filename, const char *instruction)
{
    FILE* const inFile = fopen(filename, instruction);
    if (inFile) return inFile;
    /* error */
    perror(filename);
    exit(3);
}

static size_t fread_orDie(void* buffer, size_t sizeToRead, FILE* file)
{
    size_t const readSize = fread(buffer, 1, sizeToRead, file);
    if (readSize == sizeToRead) return readSize;   /* good */
    if (feof(file)) return readSize;   /* good, reached end of file */
    /* error */
    perror("fread");
    exit(4);
}

static size_t fwrite_orDie(const void* buffer, size_t sizeToWrite, FILE* file)
{
    size_t const writtenSize = fwrite(buffer, 1, sizeToWrite, file);
    if (writtenSize == sizeToWrite) return sizeToWrite;   /* good */
    /* error */
    perror("fwrite");
    exit(5);
}

static size_t fclose_orDie(FILE* file)
{
    if (!fclose(file)) return 0;
    /* error */
    perror("fclose");
    exit(6);
}

static void fseek_orDie(FILE* file, long int offset, int origin) {
    if (!fseek(file, offset, origin)) {
        if (!fflush(file)) return;
    }
    /* error */
    perror("fseek");
    exit(7);
}


static void decompressFile_orDie(const char* fname, unsigned startOffset, unsigned endOffset)
{
    FILE* const fin  = fopen_orDie(fname, "rb");
    size_t const buffInSize = ZSTD_DStreamInSize();
    void*  const buffIn  = malloc_orDie(buffInSize);
    FILE* const fout = stdout;
    size_t const buffOutSize = ZSTD_DStreamOutSize();  /* Guarantee to successfully flush at least one complete compressed block in all circumstances. */
    void*  const buffOut = malloc_orDie(buffOutSize);

    ZSTD_seekable_DStream* const dstream = ZSTD_seekable_createDStream();
    if (dstream==NULL) { fprintf(stderr, "ZSTD_seekable_createDStream() error \n"); exit(10); }

    {   size_t sizeNeeded = 0;
        void* buffSeekTable = NULL;

        do {
            sizeNeeded = ZSTD_seekable_loadSeekTable(dstream, buffSeekTable, sizeNeeded);
            if (!sizeNeeded) break;

            if (ZSTD_isError(sizeNeeded)) {
                fprintf(stderr, "ZSTD_seekable_loadSeekTable() error : %s \n",
                        ZSTD_getErrorName(sizeNeeded));
                exit(11);
            }

            fseek_orDie(fin, -(long) sizeNeeded, SEEK_END);
            buffSeekTable = realloc_orDie(buffSeekTable, sizeNeeded);
            fread_orDie(buffSeekTable, sizeNeeded, fin);
        } while (sizeNeeded > 0);

        free(buffSeekTable);
    }

    /* In more complex scenarios, a file may consist of multiple appended frames (ex : pzstd).
    *  The following example decompresses only the first frame.
    *  It is compatible with other provided streaming examples */
    size_t const initResult = ZSTD_seekable_initDStream(dstream, startOffset, endOffset);
    if (ZSTD_isError(initResult)) { fprintf(stderr, "ZSTD_seekable_initDStream() error : %s \n", ZSTD_getErrorName(initResult)); exit(11); }

    size_t result, read, toRead = 0;

    do {
        read = fread_orDie(buffIn, toRead, fin);
        {   ZSTD_inBuffer input = { buffIn, read, 0 };
            ZSTD_outBuffer output = { buffOut, buffOutSize, 0 };
            result = ZSTD_seekable_decompressStream(dstream, &output, &input);

            if (ZSTD_isError(result)) {
                if (ZSTD_getErrorCode(result) == ZSTD_error_needSeek) {
                    unsigned long long const offset = ZSTD_seekable_getSeekOffset(dstream);
                    fseek_orDie(fin, offset, SEEK_SET);
                    ZSTD_seekable_updateOffset(dstream, offset);
                    toRead = 0;
                } else {
                    fprintf(stderr,
                            "ZSTD_seekable_decompressStream() error : %s \n",
                            ZSTD_getErrorName(result));
                    exit(12);
                }
            } else {
                toRead = result;
            }
            fwrite_orDie(buffOut, output.pos, fout);
        }
    } while (result > 0);

    ZSTD_seekable_freeDStream(dstream);
    fclose_orDie(fin);
    fclose_orDie(fout);
    free(buffIn);
    free(buffOut);
}


int main(int argc, const char** argv)
{
    const char* const exeName = argv[0];

    if (argc!=4) {
        fprintf(stderr, "wrong arguments\n");
        fprintf(stderr, "usage:\n");
        fprintf(stderr, "%s FILE\n", exeName);
        return 1;
    }

    {
        const char* const inFilename = argv[1];
        unsigned const startOffset = (unsigned) atoi(argv[2]);
        unsigned const endOffset = (unsigned) atoi(argv[3]);
        decompressFile_orDie(inFilename, startOffset, endOffset);
    }
    return 0;
}
