/*
 * Copyright (c) 2018-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* largeNbDicts
 * This is a benchmark test tool
 * dedicated to the specific case of dictionary decompression
 * using a very large nb of dictionaries
 * thus generating many cache-misses.
 * It's created in a bid to investigate performance and find optimizations. */


/*---  Dependencies  ---*/

#include <stddef.h>   /* size_t */
#include <stdlib.h>   /* malloc, free */
#include <stdio.h>    /* printf */
#include <assert.h>   /* assert */

#include "util.h"
#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"
#include "zdict.h"


/*---  Constants  --- */

#define KB  *(1<<10)
#define MB  *(1<<20)

#define BLOCKSIZE (4 KB)
#define DICTSIZE  (4 KB)
#define COMP_LEVEL 3

#define DISPLAY_LEVEL_DEFAULT 3


/*---  Display Macros  ---*/

#define DISPLAY(...)         fprintf(stdout, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) { if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); } }
static int g_displayLevel = DISPLAY_LEVEL_DEFAULT;   /* 0 : no display,  1: errors,  2 : + result + interaction + warnings,  3 : + progression,  4 : + information */


/*---  buffer_t  ---*/

typedef struct {
    void* ptr;
    size_t size;
    size_t capacity;
} buffer_t;

static const buffer_t kBuffNull = { NULL, 0, 0 };


static buffer_t fillBuffer_fromHandle(buffer_t buff, FILE* f)
{
    size_t const readSize = fread(buff.ptr, 1, buff.capacity, f);
    buff.size = readSize;
    return buff;
}

static void freeBuffer(buffer_t buff)
{
    free(buff.ptr);
}

/* @return : kBuffNull if any error */
static buffer_t createBuffer_fromHandle(FILE* f, size_t bufferSize)
{
    void* const buffer = malloc(bufferSize);
    if (buffer==NULL) return kBuffNull;

    {   buffer_t buff = { buffer, 0, bufferSize };
        buff = fillBuffer_fromHandle(buff, f);
        if (buff.size != buff.capacity) {
            freeBuffer(buff);
            return kBuffNull;
        }
        return buff;
    }
}

/* @return : kBuffNull if any error */
static buffer_t createBuffer_fromFile(const char* fileName)
{
    U64 const fileSize = UTIL_getFileSize(fileName);
    size_t const bufferSize = (size_t) fileSize;

    if (fileSize == UTIL_FILESIZE_UNKNOWN) return kBuffNull;
    assert((U64)bufferSize == fileSize);   /* check overflow */

    {   buffer_t buff;
        FILE* const f = fopen(fileName, "rb");
        if (f == NULL) return kBuffNull;

        buff = createBuffer_fromHandle(f, bufferSize);
        fclose(f);   /* do nothing specific if fclose() fails */
        return buff;
    }
}


/*---  buffer_collection_t  ---*/

typedef struct {
    void** buffers;
    size_t* capacities;
    size_t nbBuffers;
} buffer_collection_t;

static const buffer_collection_t kNullCollection = { NULL, NULL, 0 };

static void freeCollection(buffer_collection_t collection)
{
    free(collection.buffers);
    free(collection.capacities);
}

/* returns .buffers=NULL if operation fails */
buffer_collection_t splitBuffer(buffer_t srcBuffer, size_t blockSize)
{
    size_t const nbBlocks = (srcBuffer.size + (blockSize-1)) / blockSize;

    void** const buffers = malloc(nbBlocks * sizeof(void*));
    size_t* const capacities = malloc(nbBlocks * sizeof(size_t*));
    if ((buffers==NULL) || capacities==NULL) {
        free(buffers);
        free(capacities);
        return kNullCollection;
    }

    char* newBlockPtr = (char*)srcBuffer.ptr;
    char* const srcEnd = newBlockPtr + srcBuffer.size;
    assert(nbBlocks >= 1);
    for (size_t blockNb = 0; blockNb < nbBlocks-1; blockNb++) {
        buffers[blockNb] = newBlockPtr;
        capacities[blockNb] = blockSize;
        newBlockPtr += blockSize;
    }

    /* last block */
    assert(newBlockPtr <= srcEnd);
    size_t const lastBlockSize = (srcEnd - newBlockPtr);
    buffers[nbBlocks-1] = newBlockPtr;
    capacities[nbBlocks-1] = lastBlockSize;

    buffer_collection_t result;
    result.buffers = buffers;
    result.capacities = capacities;
    result.nbBuffers = nbBlocks;
    return result;
}



/*---  ddict_collection_t  ---*/

typedef struct {
    ZSTD_DDict** ddicts;
    size_t nbDDict;
} ddict_collection_t;

static const ddict_collection_t kNullDDictCollection = { NULL, 0 };

static void freeDDictCollection(ddict_collection_t ddictc)
{
    for (size_t dictNb=0; dictNb < ddictc.nbDDict; dictNb++) {
        ZSTD_freeDDict(ddictc.ddicts[dictNb]);
    }
    free(ddictc.ddicts);
}

/* returns .buffers=NULL if operation fails */
static ddict_collection_t createDDictCollection(const void* dictBuffer, size_t dictSize, size_t nbDDict)
{
    ZSTD_DDict** const ddicts = malloc(nbDDict * sizeof(ZSTD_DDict*));
    if (ddicts==NULL) return kNullDDictCollection;
    for (size_t dictNb=0; dictNb < nbDDict; dictNb++) {
        ddicts[dictNb] = ZSTD_createDDict(dictBuffer, dictSize);
        assert(ddicts[dictNb] != NULL);
    }
    ddict_collection_t ddictc;
    ddictc.ddicts = ddicts;
    ddictc.nbDDict = nbDDict;
    return ddictc;
}



/*---  Benchmark  --- */


/* bench() :
 * @return : 0 is success, 1+ otherwise */
int bench(const char* fileName)
{
    int result = 0;

    DISPLAYLEVEL(3, "loading %s... \n", fileName);
    buffer_t const srcBuffer = createBuffer_fromFile(fileName);
    if (srcBuffer.ptr == NULL) {
        DISPLAYLEVEL(1," error reading file %s \n", fileName);
        return 1;
    }
    DISPLAYLEVEL(3, "created src buffer of size %.1f MB \n",
                    (double)(srcBuffer.size) / (1 MB));

    buffer_collection_t const srcBlockBuffers = splitBuffer(srcBuffer, BLOCKSIZE);
    assert(srcBlockBuffers.buffers != NULL);
    unsigned const nbBlocks = (unsigned)srcBlockBuffers.nbBuffers;
    DISPLAYLEVEL(3, "splitting input into %u blocks of max size %u bytes \n",
                    nbBlocks, BLOCKSIZE);

    size_t const dstBlockSize = ZSTD_compressBound(BLOCKSIZE);
    size_t const dstBufferCapacity = nbBlocks * dstBlockSize;
    void* const dstPtr = malloc(dstBufferCapacity);
    assert(dstPtr != NULL);
    buffer_t dstBuffer;
    dstBuffer.ptr = dstPtr;
    dstBuffer.capacity = dstBufferCapacity;
    dstBuffer.size = dstBufferCapacity;

    buffer_collection_t const dstBlockBuffers = splitBuffer(dstBuffer, dstBlockSize);
    assert(dstBlockBuffers.buffers != NULL);

    DISPLAYLEVEL(3, "creating dictionary, of target size %u bytes \n", DICTSIZE);
    void* const dictBuffer = malloc(DICTSIZE);
    if (dictBuffer == NULL) { result = 1; goto _cleanup; }

    size_t const dictSize = ZDICT_trainFromBuffer(dictBuffer, DICTSIZE,
                                                srcBuffer.ptr,
                                                srcBlockBuffers.capacities,
                                                nbBlocks);
    if (ZSTD_isError(dictSize)) {
        DISPLAYLEVEL(1, "error creating dictionary \n");
        result = 1;
        goto _cleanup;
    }

    size_t const dictMem = ZSTD_estimateDDictSize(dictSize, ZSTD_dlm_byCopy);
    size_t const allDictMem = dictMem * nbBlocks;
    DISPLAYLEVEL(3, "generating %u dictionaries, using %.1f MB of memory \n",
                    nbBlocks, (double)allDictMem / (1 MB));

    ZSTD_CDict* const cdict = ZSTD_createCDict(dictBuffer, dictSize, COMP_LEVEL);
    do {
        ddict_collection_t const dictionaries = createDDictCollection(dictBuffer, dictSize, nbBlocks);
        assert(dictionaries.ddicts != NULL);

        freeDDictCollection(dictionaries);
    } while(0);
    ZSTD_freeCDict(cdict);

_cleanup:
    free(dictBuffer);
    freeCollection(dstBlockBuffers);
    freeBuffer(dstBuffer);
    freeCollection(srcBlockBuffers);
    freeBuffer(srcBuffer);

    return result;
}




/*---  Command Line ---*/

int bad_usage(const char* exeName)
{
    DISPLAY (" bad usage : \n");
    DISPLAY (" %s filename \n", exeName);
    return 1;
}

int main (int argc, const char** argv)
{
    const char* const exeName = argv[0];

    if (argc != 2) return bad_usage(exeName);
    return bench(argv[1]);
}
