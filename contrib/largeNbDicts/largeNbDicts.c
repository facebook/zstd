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
 * thus suffering latency from lots of cache misses.
 * It's created in a bid to investigate performance and find optimizations. */


/*---  Dependencies  ---*/

#include <stddef.h>   /* size_t */
#include <stdlib.h>   /* malloc, free */
#include <stdio.h>    /* printf */
#include <assert.h>   /* assert */

#include "util.h"
#include "bench.h"
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

/* shrinkSizes() :
 * update sizes in buffer collection */
void shrinkSizes(buffer_collection_t collection,
                 const size_t* sizes)  /* presumed same size as collection */
{
    size_t const nbBlocks = collection.nbBuffers;
    for (size_t blockNb = 0; blockNb < nbBlocks; blockNb++) {
        assert(sizes[blockNb] <= collection.capacities[blockNb]);
        collection.capacities[blockNb] = sizes[blockNb];
    }
}

/*---  dictionary creation  ---*/

buffer_t createDictionary(const char* dictionary,
                        const void* srcBuffer, size_t* srcBlockSizes, unsigned nbBlocks)
{
    if (dictionary) {
        DISPLAYLEVEL(3, "loading dictionary %s \n", dictionary);
        return createBuffer_fromFile(dictionary);
    } else {
        DISPLAYLEVEL(3, "creating dictionary, of target size %u bytes \n", DICTSIZE);
        void* const dictBuffer = malloc(DICTSIZE);
        assert(dictBuffer != NULL);

        size_t const dictSize = ZDICT_trainFromBuffer(dictBuffer, DICTSIZE,
                                                    srcBuffer,
                                                    srcBlockSizes,
                                                    nbBlocks);
        assert(!ZSTD_isError(dictSize));

        buffer_t result;
        result.ptr = dictBuffer;
        result.capacity = DICTSIZE;
        result.size = dictSize;
        return result;
    }
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
    assert(ddicts != NULL);
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


/* mess with adresses, so that linear scanning dictionaries != linear address scanning */
void shuffleDictionaries(ddict_collection_t dicts)
{
    size_t const nbDicts = dicts.nbDDict;
    for (size_t r=0; r<nbDicts; r++) {
        size_t const d1 = rand() % nbDicts;
        size_t const d2 = rand() % nbDicts;
        ZSTD_DDict* tmpd = dicts.ddicts[d1];
        dicts.ddicts[d1] = dicts.ddicts[d2];
        dicts.ddicts[d2] = tmpd;
    }
}


/* ---   Compression  --- */

/* compressBlocks() :
 * @return : total compressed size of all blocks,
 *        or 0 if error.
 */
static size_t compressBlocks(size_t* cSizes,   /* optional (can be NULL). If present, must contain at least nbBlocks fields */
                             buffer_collection_t dstBlockBuffers,
                             buffer_collection_t srcBlockBuffers,
                             ZSTD_CDict* cdict, int cLevel)
{
    size_t const nbBlocks = srcBlockBuffers.nbBuffers;
    assert(dstBlockBuffers.nbBuffers == srcBlockBuffers.nbBuffers);

    ZSTD_CCtx* const cctx = ZSTD_createCCtx();
    assert(cctx != NULL);

    size_t totalCSize = 0;
    for (size_t blockNb=0; blockNb < nbBlocks; blockNb++) {
        size_t cBlockSize;
        if (cdict == NULL) {
            cBlockSize = ZSTD_compressCCtx(cctx,
                            dstBlockBuffers.buffers[blockNb], dstBlockBuffers.capacities[blockNb],
                            srcBlockBuffers.buffers[blockNb], srcBlockBuffers.capacities[blockNb],
                            cLevel);
        } else {
            cBlockSize = ZSTD_compress_usingCDict(cctx,
                            dstBlockBuffers.buffers[blockNb], dstBlockBuffers.capacities[blockNb],
                            srcBlockBuffers.buffers[blockNb], srcBlockBuffers.capacities[blockNb],
                            cdict);
        }
        assert(!ZSTD_isError(cBlockSize));
        if (cSizes) cSizes[blockNb] = cBlockSize;
        totalCSize += cBlockSize;
    }
    return totalCSize;
}


/* ---  Benchmark  --- */

typedef size_t (*BMK_benchFn_t)(const void* src, size_t srcSize, void* dst, size_t dstCapacity, void* customPayload);
typedef size_t (*BMK_initFn_t)(void* initPayload);

typedef struct {
    ZSTD_DCtx* dctx;
    size_t nbBlocks;
    size_t blockNb;
    ddict_collection_t dictionaries;
} decompressInstructions;

decompressInstructions createDecompressInstructions(ddict_collection_t dictionaries)
{
    decompressInstructions di;
    di.dctx = ZSTD_createDCtx();
    assert(di.dctx != NULL);
    di.nbBlocks = dictionaries.nbDDict;
    di.blockNb = 0;
    di.dictionaries = dictionaries;
    return di;
}

void freeDecompressInstructions(decompressInstructions di)
{
    ZSTD_freeDCtx(di.dctx);
}

/* benched function */
size_t decompress(const void* src, size_t srcSize, void* dst, size_t dstCapacity, void* payload)
{
    decompressInstructions* const di = (decompressInstructions*) payload;

    size_t const result = ZSTD_decompress_usingDDict(di->dctx,
                                        dst, dstCapacity,
                                        src, srcSize,
                                        di->dictionaries.ddicts[di->blockNb]);

    di->blockNb = di->blockNb + 1;
    if (di->blockNb >= di->nbBlocks) di->blockNb = 0;

    return result;
}


#define BENCH_TIME_DEFAULT_MS 6000
#define RUN_TIME_DEFAULT_MS   1000

static int benchMem(buffer_collection_t dstBlocks,
                    buffer_collection_t srcBlocks,
                    ddict_collection_t dictionaries)
{
    assert(dstBlocks.nbBuffers == srcBlocks.nbBuffers);
    assert(dstBlocks.nbBuffers == dictionaries.nbDDict);

    double bestSpeed = 0.;

    BMK_timedFnState_t* const benchState =
            BMK_createTimedFnState(BENCH_TIME_DEFAULT_MS, RUN_TIME_DEFAULT_MS);
    decompressInstructions di = createDecompressInstructions(dictionaries);

    for (;;) {
        BMK_runOutcome_t const outcome = BMK_benchTimedFn(benchState,
                                decompress, &di,
                                NULL, NULL,
                                dstBlocks.nbBuffers,
                                (const void* const *)srcBlocks.buffers, srcBlocks.capacities,
                                dstBlocks.buffers, dstBlocks.capacities,
                                NULL);

        assert(BMK_isSuccessful_runOutcome(outcome));
        BMK_runTime_t const result = BMK_extract_runTime(outcome);
        U64 const dTime_ns = result.nanoSecPerRun;
        double const dTime_sec = (double)dTime_ns / 1000000000;
        size_t const srcSize = result.sumOfReturn;
        double const dSpeed_MBps = (double)srcSize / dTime_sec / (1 MB);
        if (dSpeed_MBps > bestSpeed) bestSpeed = dSpeed_MBps;
        DISPLAY("Decompression Speed : %.1f MB/s \r", bestSpeed);
        if (BMK_isCompleted_TimedFn(benchState)) break;
    }
    DISPLAY("\n");

    freeDecompressInstructions(di);
    BMK_freeTimedFnState(benchState);

    return 0;   /* success */
}


/* bench() :
 * fileName : file to load for benchmarking purpose
 * dictionary : optional (can be NULL), file to load as dictionary,
 *              if none provided : will be calculated on the fly by the program.
 * @return : 0 is success, 1+ otherwise */
int bench(const char* fileName, const char* dictionary)
{
    int result = 0;

    DISPLAYLEVEL(3, "loading %s... \n", fileName);
    buffer_t const srcBuffer = createBuffer_fromFile(fileName);
    assert(srcBuffer.ptr != NULL);
    size_t const srcSize = srcBuffer.size;
    DISPLAYLEVEL(3, "created src buffer of size %.1f MB \n",
                    (double)srcSize / (1 MB));

    buffer_collection_t const srcBlockBuffers = splitBuffer(srcBuffer, BLOCKSIZE);
    assert(srcBlockBuffers.buffers != NULL);
    unsigned const nbBlocks = (unsigned)srcBlockBuffers.nbBuffers;
    DISPLAYLEVEL(3, "split input into %u blocks of max size %u bytes \n",
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

    /* dictionary determination */
    buffer_t const dictBuffer = createDictionary(dictionary,
                                srcBuffer.ptr,
                                srcBlockBuffers.capacities, nbBlocks);
    assert(dictBuffer.ptr != NULL);

    ZSTD_CDict* const cdict = ZSTD_createCDict(dictBuffer.ptr, dictBuffer.size, COMP_LEVEL);
    assert(cdict != NULL);

    size_t const cTotalSizeNoDict = compressBlocks(NULL, dstBlockBuffers, srcBlockBuffers, NULL, COMP_LEVEL);
    assert(cTotalSizeNoDict != 0);
    DISPLAYLEVEL(3, "compressing at level %u without dictionary : Ratio=%.2f  (%u bytes) \n",
                    COMP_LEVEL,
                    (double)srcSize / cTotalSizeNoDict, (unsigned)cTotalSizeNoDict);

    size_t* const cSizes = malloc(nbBlocks * sizeof(size_t));
    assert(cSizes != NULL);

    size_t const cTotalSize = compressBlocks(cSizes, dstBlockBuffers, srcBlockBuffers, cdict, COMP_LEVEL);
    assert(cTotalSize != 0);
    DISPLAYLEVEL(3, "compressed using a %u bytes dictionary : Ratio=%.2f  (%u bytes) \n",
                    (unsigned)dictBuffer.size,
                    (double)srcSize / cTotalSize, (unsigned)cTotalSize);

    size_t const dictMem = ZSTD_estimateDDictSize(dictBuffer.size, ZSTD_dlm_byCopy);
    size_t const allDictMem = dictMem * nbBlocks;
    DISPLAYLEVEL(3, "generating %u dictionaries, using %.1f MB of memory \n",
                    nbBlocks, (double)allDictMem / (1 MB));

    ddict_collection_t const dictionaries = createDDictCollection(dictBuffer.ptr, dictBuffer.size, nbBlocks);
    assert(dictionaries.ddicts != NULL);

    shuffleDictionaries(dictionaries);
    // for (size_t u = 0; u < dictionaries.nbDDict; u++) DISPLAY("dict address : %p \n", dictionaries.ddicts[u]);   /* check dictionary addresses */

    void* const resultPtr = malloc(srcSize);
    assert(resultPtr != NULL);
    buffer_t resultBuffer;
    resultBuffer.ptr = resultPtr;
    resultBuffer.capacity = srcSize;
    resultBuffer.size = srcSize;

    buffer_collection_t const resultBlockBuffers = splitBuffer(resultBuffer, BLOCKSIZE);
    assert(resultBlockBuffers.buffers != NULL);

    shrinkSizes(dstBlockBuffers, cSizes);

    result = benchMem(resultBlockBuffers, dstBlockBuffers, dictionaries);

    /* free all heap objects in reverse order */
    freeCollection(resultBlockBuffers);
    free(resultPtr);
    freeDDictCollection(dictionaries);
    free(cSizes);
    ZSTD_freeCDict(cdict);
    freeBuffer(dictBuffer);
    freeCollection(dstBlockBuffers);
    freeBuffer(dstBuffer);
    freeCollection(srcBlockBuffers);
    freeBuffer(srcBuffer);

    return result;
}




/* ---  Command Line  --- */

int bad_usage(const char* exeName)
{
    DISPLAY (" bad usage : \n");
    DISPLAY (" %s filename [-D dictionary] \n", exeName);
    return 1;
}

int main (int argc, const char** argv)
{
    const char* const exeName = argv[0];

    if (argc < 2) return bad_usage(exeName);
    const char* const fileName = argv[1];

    const char* dictionary = NULL;
    if (argc > 2) {
        if (argc != 4) return bad_usage(exeName);
        if (strcmp(argv[2], "-D")) return bad_usage(exeName);
        dictionary = argv[3];
    }

    return bench(fileName, dictionary);
}
