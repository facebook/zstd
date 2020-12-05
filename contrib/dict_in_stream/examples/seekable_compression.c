/*
 * Copyright (c) 2017-present, Facebook, Inc.
 * Copyright (c) 2020 Sean Bartell
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 */

#include <stdlib.h>    // malloc, free, exit, atoi
#include <stdio.h>     // fprintf, perror, feof, fopen, etc.
#include <string.h>    // strlen, memset, strcat
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>      // presumes zstd library is installed

#include "zstd_dict_in_stream.h"
#include "zstd_seekable.h"

static void* malloc_orDie(size_t size)
{
    void* const buff = malloc(size);
    if (buff) return buff;
    /* error */
    perror("malloc:");
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

static size_t fsize_orDie(FILE* file)
{
    if (fseek(file, 0, SEEK_END)) {
        perror("fseek");
        exit(7);
    }
    long result = ftell(file);
    if (result < 0) {
        perror("ftell");
        exit(8);
    }
    if (fseek(file, 0, SEEK_SET)) {
        perror("fseek");
        exit(9);
    }
    return result;
}

static void compressFile_orDie(const char* dictName, const char* fname, const char* outName, int cLevel, unsigned frameSize)
{
    FILE* const fdict = fopen_orDie(dictName, "rb");
    FILE* const fin   = fopen_orDie(fname, "rb");
    FILE* const fout  = fopen_orDie(outName, "wb");
    size_t const dictSize = fsize_orDie(fdict);
    void*  const dict    = malloc_orDie(dictSize);
    size_t const buffInSize = ZSTD_CStreamInSize();    /* can always read one full block */
    void*  const buffIn  = malloc_orDie(buffInSize);
    size_t const buffOutSize = ZSTD_CStreamOutSize();  /* can always flush a full block */
    void*  const buffOut = malloc_orDie(buffOutSize);

    ZSTD_seekable_CStream* const cstream = ZSTD_seekable_createCStream();
    if (cstream==NULL) { fprintf(stderr, "ZSTD_seekable_createCStream() error \n"); exit(10); }
    size_t const initResult = ZSTD_seekable_initCStream(cstream, cLevel, 1, frameSize);
    if (ZSTD_isError(initResult)) { fprintf(stderr, "ZSTD_seekable_initCStream() error : %s \n", ZSTD_getErrorName(initResult)); exit(11); }

    fread_orDie(dict, dictSize, fdict);
    ZSTD_CDict* cdict = ZSTD_createCDict(dict, dictSize, cLevel);
    ZSTD_seekable_refCDict(cstream, cdict);

    size_t dictFrameSize = ZSTD_dict_in_stream_maxFrameSize(dict, dictSize);
    if (ZSTD_isError(dictFrameSize)) { fprintf(stderr, "ZSTD_dict_in_stream_maxFrameSize() error : %s \n", ZSTD_getErrorName(dictFrameSize)); exit(14); }
    void*  const dictFrame = malloc_orDie(dictFrameSize);
    dictFrameSize = ZSTD_dict_in_stream_createFrame(dictFrame, dictFrameSize, dict, dictSize, 5);
    if (ZSTD_isError(dictFrameSize)) { fprintf(stderr, "ZSTD_dict_in_stream_createFrame() error : %s \n", ZSTD_getErrorName(dictFrameSize)); exit(15); }
    fwrite_orDie(dictFrame, dictFrameSize, fout);
    ZSTD_seekable_logFrame(ZSTD_seekable_getFrameLog(cstream), dictFrameSize, 0, 0);

    size_t read, toRead = buffInSize;
    while( (read = fread_orDie(buffIn, toRead, fin)) ) {
        ZSTD_inBuffer input = { buffIn, read, 0 };
        while (input.pos < input.size) {
            ZSTD_outBuffer output = { buffOut, buffOutSize, 0 };
            toRead = ZSTD_seekable_compressStream(cstream, &output , &input);   /* toRead is guaranteed to be <= ZSTD_CStreamInSize() */
            if (ZSTD_isError(toRead)) { fprintf(stderr, "ZSTD_seekable_compressStream() error : %s \n", ZSTD_getErrorName(toRead)); exit(12); }
            if (toRead > buffInSize) toRead = buffInSize;   /* Safely handle case when `buffInSize` is manually changed to a value < ZSTD_CStreamInSize()*/
            fwrite_orDie(buffOut, output.pos, fout);
        }
    }

    while (1) {
        ZSTD_outBuffer output = { buffOut, buffOutSize, 0 };
        size_t const remainingToFlush = ZSTD_seekable_endStream(cstream, &output);   /* close stream */
        if (ZSTD_isError(remainingToFlush)) { fprintf(stderr, "ZSTD_seekable_endStream() error : %s \n", ZSTD_getErrorName(remainingToFlush)); exit(13); }
        fwrite_orDie(buffOut, output.pos, fout);
        if (!remainingToFlush) break;
    }

    ZSTD_seekable_freeCStream(cstream);
    ZSTD_freeCDict(cdict);
    fclose_orDie(fout);
    fclose_orDie(fin);
    fclose_orDie(fdict);
    free(dict);
    free(dictFrame);
    free(buffIn);
    free(buffOut);
}

static char* createOutFilename_orDie(const char* filename)
{
    size_t const inL = strlen(filename);
    size_t const outL = inL + 5;
    void* outSpace = malloc_orDie(outL);
    memset(outSpace, 0, outL);
    strcat(outSpace, filename);
    strcat(outSpace, ".zst");
    return (char*)outSpace;
}

int main(int argc, const char** argv) {
    const char* const exeName = argv[0];
    if (argc!=4) {
        printf("wrong arguments\n");
        printf("usage:\n");
        printf("%s DICT_FILE FILE FRAME_SIZE\n", exeName);
        return 1;
    }

    {   const char* const dictFileName = argv[1];
        const char* const inFileName = argv[2];
        unsigned const frameSize = (unsigned)atoi(argv[3]);

        char* const outFileName = createOutFilename_orDie(inFileName);
        compressFile_orDie(dictFileName, inFileName, outFileName, 10, frameSize);
        free(outFileName);
    }

    return 0;
}
