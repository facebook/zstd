/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
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
#include <zstd_errors.h>
#if defined(WIN32) || defined(_WIN32)
#  include <windows.h>
#  define SLEEP(x) Sleep(x)
#else
#  include <unistd.h>
#  define SLEEP(x) usleep(x * 1000)
#endif

#include "xxhash.h"

#define ZSTD_MULTITHREAD 1
#include "threading.h"
#include "pool.h"      // use zstd thread pool for demo

#include "../zstd_seekable.h"

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

struct state {
    FILE* fout;
    ZSTD_pthread_mutex_t mutex;
    size_t nextID;
    struct job* pending;
    ZSTD_frameLog* frameLog;
    const int compressionLevel;
};

struct job {
    size_t id;
    struct job* next;
    struct state* state;

    void* src;
    size_t srcSize;
    void* dst;
    size_t dstSize;

    unsigned checksum;
};

static void addPending_inmutex(struct state* state, struct job* job)
{
    struct job** p = &state->pending;
    while (*p && (*p)->id < job->id)
        p = &(*p)->next;
    job->next = *p;
    *p = job;
}

static void flushFrame(struct state* state, struct job* job)
{
    fwrite_orDie(job->dst, job->dstSize, state->fout);
    free(job->dst);

    size_t ret = ZSTD_seekable_logFrame(state->frameLog, (unsigned)job->dstSize, (unsigned)job->srcSize, job->checksum);
    if (ZSTD_isError(ret)) {
        fprintf(stderr, "ZSTD_seekable_logFrame() error : %s \n", ZSTD_getErrorName(ret));
        exit(12);
    }
}

static void flushPending_inmutex(struct state* state)
{
    while (state->pending && state->pending->id == state->nextID) {
        struct job* p = state->pending;
        state->pending = p->next;
        flushFrame(state, p);
        free(p);
        state->nextID++;
    }
}

static void finishFrame(struct job* job)
{
    struct state *state = job->state;
    ZSTD_pthread_mutex_lock(&state->mutex);
    addPending_inmutex(state, job);
    flushPending_inmutex(state);
    ZSTD_pthread_mutex_unlock(&state->mutex);
}

static void compressFrame(void* opaque)
{
    struct job* job = opaque;

    job->checksum = (unsigned)XXH64(job->src, job->srcSize, 0);

    size_t ret = ZSTD_compress(job->dst, job->dstSize, job->src, job->srcSize, job->state->compressionLevel);
    if (ZSTD_isError(ret)) {
        fprintf(stderr, "ZSTD_compress() error : %s \n", ZSTD_getErrorName(ret));
        exit(20);
    }
    job->dstSize = ret;

    // No longer need
    free(job->src);
    job->src = NULL;

    finishFrame(job);
}

static const char* createOutFilename_orDie(const char* filename)
{
    size_t const inL = strlen(filename);
    size_t const outL = inL + 5;
    void* outSpace = malloc_orDie(outL);
    memset(outSpace, 0, outL);
    strcat(outSpace, filename);
    strcat(outSpace, ".zst");
    return (const char*)outSpace;
}

static void openInOut_orDie(const char* fname, FILE** fin, FILE** fout) {
    if (strcmp(fname, "-") == 0) {
        *fin = stdin;
        *fout = stdout;
    } else {
        *fin = fopen_orDie(fname, "rb");
        const char* outName = createOutFilename_orDie(fname);
        *fout = fopen_orDie(outName, "wb");
    }
}

static void compressFile_orDie(const char* fname, int cLevel, unsigned frameSize, size_t nbThreads)
{
    struct state state = {
        .nextID = 0,
        .pending = NULL,
        .compressionLevel = cLevel,
    };
    ZSTD_pthread_mutex_init(&state.mutex, NULL);
    state.frameLog = ZSTD_seekable_createFrameLog(1);
    if (state.frameLog == NULL) { fprintf(stderr, "ZSTD_seekable_createFrameLog() failed \n"); exit(11); }

    POOL_ctx* pool = POOL_create(nbThreads, nbThreads);
    if (pool == NULL) { fprintf(stderr, "POOL_create() error \n"); exit(9); }

    FILE* fin;
    openInOut_orDie(fname, &fin, &state.fout);

    if (ZSTD_compressBound(frameSize) > 0xFFFFFFFFU) { fprintf(stderr, "Frame size too large \n"); exit(10); }
    size_t dstSize = ZSTD_compressBound(frameSize);

    for (size_t id = 0; 1; id++) {
        struct job* job = malloc_orDie(sizeof(struct job));
        job->id = id;
        job->next = NULL;
        job->state = &state;
        job->src = malloc_orDie(frameSize);
        job->dst = malloc_orDie(dstSize);
        job->srcSize = fread_orDie(job->src, frameSize, fin);
        job->dstSize = dstSize; 
        POOL_add(pool, compressFrame, job);
        if (feof(fin))
            break;
    }

    POOL_joinJobs(pool);
    POOL_free(pool);
    if (state.pending) {
        fprintf(stderr, "Unexpected leftover output blocks!\n");
        exit(13);
    }

    {   unsigned char seekTableBuff[1024];
        ZSTD_outBuffer out = {seekTableBuff, 1024, 0};
        while (ZSTD_seekable_writeSeekTable(state.frameLog, &out) != 0) {
            fwrite_orDie(seekTableBuff, out.pos, state.fout);
            out.pos = 0;
        }
        fwrite_orDie(seekTableBuff, out.pos, state.fout);
    }

    ZSTD_seekable_freeFrameLog(state.frameLog);
    fclose_orDie(state.fout);
    fclose_orDie(fin);
}

int main(int argc, const char** argv) {
    const char* const exeName = argv[0];
    if (argc!=4) {
        printf("wrong arguments\n");
        printf("usage:\n");
        printf("%s FILE FRAME_SIZE NB_THREADS\n", exeName);
        return 1;
    }

    {   const char* const inFileName = argv[1];
        unsigned const frameSize = (unsigned)atoi(argv[2]);
        size_t const nbThreads = (size_t)atoi(argv[3]);

        compressFile_orDie(inFileName, 5, frameSize, nbThreads);
    }

    return 0;
}
