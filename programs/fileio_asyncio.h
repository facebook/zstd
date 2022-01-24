/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_FILEIO_ASYNCIO_H
#define ZSTD_FILEIO_ASYNCIO_H

#if defined (__cplusplus)
extern "C" {
#endif

#include "../lib/common/mem.h"     /* U32, U64 */
#include "fileio_types.h"
#include "platform.h"
#include "util.h"
#include "../lib/common/pool.h"
#include "../lib/common/threading.h"

#define MAX_IO_JOBS          (10)

typedef struct {
    /* These struct fields should be set only on creation and not changed afterwards */
    POOL_ctx* threadPool;
    int totalIoJobs;
    FIO_prefs_t* prefs;
    POOL_function poolFunction;

    /* Controls the file we currently write to, make changes only by using provided utility functions */
    FILE* file;

    /* The jobs and availableJobsCount fields are accessed by both the main and worker threads and should
     * only be mutated after locking the mutex */
    ZSTD_pthread_mutex_t ioJobsMutex;
    void* availableJobs[MAX_IO_JOBS];
    int availableJobsCount;
} IOPoolCtx_t;

typedef struct {
    IOPoolCtx_t base;
    unsigned storedSkips;
} WritePoolCtx_t;

typedef struct {
    /* These fields are automatically set and shouldn't be changed by non WritePool code. */
    void *ctx;
    FILE* file;
    void *buffer;
    size_t bufferSize;

    /* This field should be changed before a job is queued for execution and should contain the number
     * of bytes to write from the buffer. */
    size_t usedBufferSize;
    U64 offset;
} IOJob_t;

/** AIO_fwriteSparse() :
*  @return : storedSkips,
*            argument for next call to AIO_fwriteSparse() or AIO_fwriteSparseEnd() */
unsigned AIO_fwriteSparse(FILE* file,
                 const void* buffer, size_t bufferSize,
                 const FIO_prefs_t* const prefs,
                 unsigned storedSkips);

void AIO_fwriteSparseEnd(const FIO_prefs_t* const prefs, FILE* file, unsigned storedSkips);

/* AIO_WritePool_releaseIoJob:
 * Releases an acquired job back to the pool. Doesn't execute the job. */
void AIO_WritePool_releaseIoJob(IOJob_t *job);

/* AIO_WritePool_acquireJob:
 * Returns an available write job to be used for a future write. */
IOJob_t* AIO_WritePool_acquireJob(WritePoolCtx_t *ctx);

/* AIO_WritePool_enqueueAndReacquireWriteJob:
 * Enqueues a write job for execution and acquires a new one.
 * After execution `job`'s pointed value would change to the newly acquired job.
 * Make sure to set `usedBufferSize` to the wanted length before call.
 * The queued job shouldn't be used directly after queueing it. */
void AIO_WritePool_enqueueAndReacquireWriteJob(IOJob_t **job);

/* AIO_WritePool_sparseWriteEnd:
 * Ends sparse writes to the current file.
 * Blocks on completion of all current write jobs before executing. */
void AIO_WritePool_sparseWriteEnd(WritePoolCtx_t *ctx);

/* AIO_WritePool_setFile:
 * Sets the destination file for future writes in the pool.
 * Requires completion of all queues write jobs and release of all otherwise acquired jobs.
 * Also requires ending of sparse write if a previous file was used in sparse mode. */
void AIO_WritePool_setFile(WritePoolCtx_t *ctx, FILE* file);

/* AIO_WritePool_getFile:
 * Returns the file the writePool is currently set to write to. */
FILE* AIO_WritePool_getFile(WritePoolCtx_t *ctx);

/* AIO_WritePool_closeFile:
 * Ends sparse write and closes the writePool's current file and sets the file to NULL.
 * Requires completion of all queues write jobs and release of all otherwise acquired jobs.  */
int AIO_WritePool_closeFile(WritePoolCtx_t *ctx);

/* AIO_WritePool_create:
 * Allocates and sets and a new write pool including its included jobs.
 * bufferSize should be set to the maximal buffer we want to write to at a time. */
WritePoolCtx_t* AIO_WritePool_create(FIO_prefs_t* const prefs, size_t bufferSize);

/* AIO_WritePool_free:
 * Frees and releases a writePool and its resources. Closes destination file. */
void AIO_WritePool_free(WritePoolCtx_t* ctx);

#if defined (__cplusplus)
}
#endif

#endif /* ZSTD_FILEIO_ASYNCIO_H */
