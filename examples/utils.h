/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/*
 * This header file has common utility functions used in examples.
 */
#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>    // malloc, free, exit
#include <stdio.h>     // fprintf, perror, fopen, etc.
#include <string.h>    // strlen, strcat, memset, strerror
#include <errno.h>     // errno
#include <sys/stat.h>  // stat

/*
 * Define the returned error code from utility functions.
 */
typedef enum {
    ERROR_fsize = 1,
    ERROR_fopen = 2,
    ERROR_fclose = 3,
    ERROR_fread = 4,
    ERROR_fwrite = 5,
    ERROR_loadFile = 6,
    ERROR_saveFile = 7,
    ERROR_malloc = 8,
    ERROR_largeFile = 9,
} UTILS_ErrorCode;

/*! fsize_orDie() :
 * Get the size of a given file path.
 *
 * @return The size of a given file path.
 */
static size_t fsize_orDie(const char *filename)
{
    struct stat st;
    if (stat(filename, &st) != 0) {
        /* error */
        perror(filename);
        exit(ERROR_fsize);
    }

    off_t const fileSize = st.st_size;
    size_t const size = (size_t)fileSize;
    /* if off_t -> size_t conversion causes discrepancy, the file size is
     * too big for at least 1 type to handle
     */
    if (size != fileSize) {   /* narrowcast overflow */
        fprintf(stderr, "%s : filesize too large \n", filename);
        exit(ERROR_largeFile);
    }
    return size;
}

/*! fopen_orDie() :
 * Open a file using given file path and open option.
 *
 * @return If successful this function will return a FILE pointer to an
 * opened file otherwise it sends an error to stderr and exits.
 */
static FILE* fopen_orDie(const char *filename, const char *instruction)
{
    FILE* const inFile = fopen(filename, instruction);
    if (inFile) return inFile;
    /* error */
    perror(filename);
    exit(ERROR_fopen);
}

/*! fclose_orDie() :
 * Close an opened file using given FILE pointer.
 */
static void fclose_orDie(FILE* file)
{
    if (!fclose(file)) { return; };
    /* error */
    perror("fclose");
    exit(ERROR_fclose);
}

/*! fread_orDie() :
 *
 * Read sizeToRead bytes from a given file, storing them at the
 * location given by buffer.
 *
 * @return The number of bytes read.
 */
static size_t fread_orDie(void* buffer, size_t sizeToRead, FILE* file)
{
    size_t const readSize = fread(buffer, 1, sizeToRead, file);
    if (readSize == sizeToRead) return readSize;   /* good */
    if (feof(file)) return readSize;   /* good, reached end of file */
    /* error */
    perror("fread");
    exit(ERROR_fread);
}

/*! fwrite_orDie() :
 *
 * Write sizeToWrite bytes to a file pointed to by file, obtaining
 * them from a location given by buffer.
 *
 * Note: This function will send an error to stderr and exit if it
 * cannot write data to the given file pointer.
 *
 * @return The number of bytes written.
 */
static size_t fwrite_orDie(const void* buffer, size_t sizeToWrite, FILE* file)
{
    size_t const writtenSize = fwrite(buffer, 1, sizeToWrite, file);
    if (writtenSize == sizeToWrite) return sizeToWrite;   /* good */
    /* error */
    perror("fwrite");
    exit(ERROR_fwrite);
}

/*! malloc_orDie() :
 * Allocate memory.
 *
 * @return If successful this function returns a pointer to allo-
 * cated memory.  If there is an error, this function will send that
 * error to stderr and exit.
 */
static void* malloc_orDie(size_t size)
{
    void* const buff = malloc(size);
    if (buff) return buff;
    /* error */
    perror("malloc");
    exit(ERROR_malloc);
}

/*! loadFile_orDie() :
 * Read size bytes from a file. If buffer is not provided (i.e., buffer == null),
 * malloc will be called to allocate one.
 *
 * Note: This function will send an error to stderr and exit if it
 * cannot read data from the given file path.
 *
 * @return If successful this function will return a pointer to read
 * data otherwise it will printout an error to stderr and exit.
 */
static void* loadFile_orDie(const char* fileName, size_t* size, void* buffer, int bufferSize)
{
    size_t const fileSize = fsize_orDie(fileName);
    FILE* const inFile = fopen_orDie(fileName, "rb");
    if (!buffer) {
        buffer = malloc_orDie(fileSize);
    }
    else if (bufferSize < fileSize) {
        fprintf(stderr, "%s : filesize bigger than provided buffer.\n", fileName);

    }
    size_t const readSize = fread(buffer, 1, fileSize, inFile);
    if (readSize != (size_t)fileSize) {
        fprintf(stderr, "fread: %s : %s \n", fileName, strerror(errno));
        exit(ERROR_fread);
    }
    fclose(inFile);  /* can't fail, read only */
    *size = fileSize;
    return buffer;
}

/*! saveFile_orDie() :
 *
 * Save buffSize bytes to a given file path, obtaining them from a location pointed
 * to by buff.
 *
 * Note: This function will send an error to stderr and exit if it
 * cannot write to a given file.
 */
static void saveFile_orDie(const char* fileName, const void* buff, size_t buffSize)
{
    FILE* const oFile = fopen_orDie(fileName, "wb");
    size_t const wSize = fwrite(buff, 1, buffSize, oFile);
    if (wSize != (size_t)buffSize) {
        fprintf(stderr, "fwrite: %s : %s \n", fileName, strerror(errno));
        exit(ERROR_fwrite);
    }
    if (fclose(oFile)) {
        perror(fileName);
        exit(ERROR_fclose);
    }
}

#endif
