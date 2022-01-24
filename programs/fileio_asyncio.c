/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "platform.h"
#include <stdio.h>      /* fprintf, open, fdopen, fread, _fileno, stdin, stdout */
#include <stdlib.h>     /* malloc, free */
#include <assert.h>
#include <errno.h>      /* errno */

#if defined (_MSC_VER)
#  include <sys/stat.h>
#  include <io.h>
#endif

#include "fileio_asyncio.h"
#include "fileio_common.h"

/* **********************************************************************
 *  Sparse write
 ************************************************************************/

/** AIO_fwriteSparse() :
*  @return : storedSkips,
*            argument for next call to AIO_fwriteSparse() or AIO_fwriteSparseEnd() */
unsigned AIO_fwriteSparse(FILE* file,
                 const void* buffer, size_t bufferSize,
                 const FIO_prefs_t* const prefs,
                 unsigned storedSkips)
{
    const size_t* const bufferT = (const size_t*)buffer;   /* Buffer is supposed malloc'ed, hence aligned on size_t */
    size_t bufferSizeT = bufferSize / sizeof(size_t);
    const size_t* const bufferTEnd = bufferT + bufferSizeT;
    const size_t* ptrT = bufferT;
    static const size_t segmentSizeT = (32 KB) / sizeof(size_t);   /* check every 32 KB */

    if (prefs->testMode) return 0;  /* do not output anything in test mode */

    if (!prefs->sparseFileSupport) {  /* normal write */
        size_t const sizeCheck = fwrite(buffer, 1, bufferSize, file);
        if (sizeCheck != bufferSize)
            EXM_THROW(70, "Write error : cannot write decoded block : %s",
                      strerror(errno));
        return 0;
    }

    /* avoid int overflow */
    if (storedSkips > 1 GB) {
        if (LONG_SEEK(file, 1 GB, SEEK_CUR) != 0)
        EXM_THROW(91, "1 GB skip error (sparse file support)");
        storedSkips -= 1 GB;
    }

    while (ptrT < bufferTEnd) {
        size_t nb0T;

        /* adjust last segment if < 32 KB */
        size_t seg0SizeT = segmentSizeT;
        if (seg0SizeT > bufferSizeT) seg0SizeT = bufferSizeT;
        bufferSizeT -= seg0SizeT;

        /* count leading zeroes */
        for (nb0T=0; (nb0T < seg0SizeT) && (ptrT[nb0T] == 0); nb0T++) ;
        storedSkips += (unsigned)(nb0T * sizeof(size_t));

        if (nb0T != seg0SizeT) {   /* not all 0s */
            size_t const nbNon0ST = seg0SizeT - nb0T;
            /* skip leading zeros */
            if (LONG_SEEK(file, storedSkips, SEEK_CUR) != 0)
                EXM_THROW(92, "Sparse skip error ; try --no-sparse");
            storedSkips = 0;
            /* write the rest */
            if (fwrite(ptrT + nb0T, sizeof(size_t), nbNon0ST, file) != nbNon0ST)
                EXM_THROW(93, "Write error : cannot write decoded block : %s",
                          strerror(errno));
        }
        ptrT += seg0SizeT;
    }

    {   static size_t const maskT = sizeof(size_t)-1;
        if (bufferSize & maskT) {
            /* size not multiple of sizeof(size_t) : implies end of block */
            const char* const restStart = (const char*)bufferTEnd;
            const char* restPtr = restStart;
            const char* const restEnd = (const char*)buffer + bufferSize;
            assert(restEnd > restStart && restEnd < restStart + sizeof(size_t));
            for ( ; (restPtr < restEnd) && (*restPtr == 0); restPtr++) ;
            storedSkips += (unsigned) (restPtr - restStart);
            if (restPtr != restEnd) {
                /* not all remaining bytes are 0 */
                size_t const restSize = (size_t)(restEnd - restPtr);
                if (LONG_SEEK(file, storedSkips, SEEK_CUR) != 0)
                    EXM_THROW(92, "Sparse skip error ; try --no-sparse");
                if (fwrite(restPtr, 1, restSize, file) != restSize)
                    EXM_THROW(95, "Write error : cannot write end of decoded block : %s",
                              strerror(errno));
                storedSkips = 0;
            }   }   }

    return storedSkips;
}

void AIO_fwriteSparseEnd(const FIO_prefs_t* const prefs, FILE* file, unsigned storedSkips)
{
    if (prefs->testMode) assert(storedSkips == 0);
    if (storedSkips>0) {
        assert(prefs->sparseFileSupport > 0);  /* storedSkips>0 implies sparse support is enabled */
        (void)prefs;   /* assert can be disabled, in which case prefs becomes unused */
        if (LONG_SEEK(file, storedSkips-1, SEEK_CUR) != 0)
            EXM_THROW(69, "Final skip error (sparse file support)");
        /* last zero must be explicitly written,
         * so that skipped ones get implicitly translated as zero by FS */
        {   const char lastZeroByte[1] = { 0 };
            if (fwrite(lastZeroByte, 1, 1, file) != 1)
                EXM_THROW(69, "Write error : cannot write last zero : %s", strerror(errno));
        }   }
}


/* **********************************************************************
 *  AsyncIO functionality
 ************************************************************************/

/* ***********************************
 *  General IoPool implementation
 *************************************/

static IOJob_t *AIO_IOPool_createIoJob(IOPoolCtx_t *ctx, size_t bufferSize) {
    void *buffer;
    IOJob_t *job;
    job = (IOJob_t*) malloc(sizeof(IOJob_t));
    buffer = malloc(bufferSize);
    if(!job || !buffer)
    EXM_THROW(101, "Allocation error : not enough memory");
    job->buffer = buffer;
    job->bufferSize = bufferSize;
    job->usedBufferSize = 0;
    job->file = NULL;
    job->ctx = ctx;
    job->offset = 0;
    return job;
}


/* AIO_IOPool_createThreadPool:
 * Creates a thread pool and a mutex for threaded IO pool.
 * Displays warning if asyncio is requested but MT isn't available. */
static void AIO_IOPool_createThreadPool(IOPoolCtx_t *ctx, const FIO_prefs_t *prefs) {
    ctx->threadPool = NULL;
    if(prefs->asyncIO) {
        if (ZSTD_pthread_mutex_init(&ctx->ioJobsMutex, NULL))
        EXM_THROW(102,"Failed creating write availableJobs mutex");
        /* We want MAX_IO_JOBS-2 queue items because we need to always have 1 free buffer to
         * decompress into and 1 buffer that's actively written to disk and owned by the writing thread. */
        assert(MAX_IO_JOBS >= 2);
        ctx->threadPool = POOL_create(1, MAX_IO_JOBS - 2);
        if (!ctx->threadPool)
        EXM_THROW(104, "Failed creating writer thread pool");
    }
}

/* AIO_IOPool_init:
 * Allocates and sets and a new write pool including its included availableJobs. */
static void AIO_IOPool_init(IOPoolCtx_t *ctx, FIO_prefs_t* const prefs, POOL_function poolFunction, size_t bufferSize) {
    int i;
    AIO_IOPool_createThreadPool(ctx, prefs);
    ctx->prefs = prefs;
    ctx->poolFunction = poolFunction;
    ctx->totalIoJobs = ctx->threadPool ? MAX_IO_JOBS : 1;
    ctx->availableJobsCount = ctx->totalIoJobs;
    for(i=0; i < ctx->availableJobsCount; i++) {
        ctx->availableJobs[i] = AIO_IOPool_createIoJob(ctx, bufferSize);
    }
    ctx->file = NULL;
}


/* AIO_IOPool_releaseIoJob:
 * Releases an acquired job back to the pool. Doesn't execute the job. */
static void AIO_IOPool_releaseIoJob(IOJob_t *job) {
    IOPoolCtx_t *ctx = (IOPoolCtx_t *) job->ctx;
    if(ctx->threadPool) {
        ZSTD_pthread_mutex_lock(&ctx->ioJobsMutex);
        assert(ctx->availableJobsCount < MAX_IO_JOBS);
        ctx->availableJobs[ctx->availableJobsCount++] = job;
        ZSTD_pthread_mutex_unlock(&ctx->ioJobsMutex);
    } else {
        assert(ctx->availableJobsCount == 0);
        ctx->availableJobsCount++;
    }
}

/* AIO_IOPool_join:
 * Waits for all tasks in the pool to finish executing. */
static void AIO_IOPool_join(IOPoolCtx_t* ctx) {
    if(ctx->threadPool)
        POOL_joinJobs(ctx->threadPool);
}

/* AIO_IOPool_free:
 * Release a previously allocated write thread pool. Makes sure all takss are done and released. */
static void AIO_IOPool_destroy(IOPoolCtx_t* ctx) {
    int i;
    if(ctx->threadPool) {
        /* Make sure we finish all tasks and then free the resources */
        AIO_IOPool_join(ctx);
        /* Make sure we are not leaking availableJobs */
        assert(ctx->availableJobsCount == ctx->totalIoJobs);
        POOL_free(ctx->threadPool);
        ZSTD_pthread_mutex_destroy(&ctx->ioJobsMutex);
    }
    assert(ctx->file == NULL);
    for(i=0; i<ctx->availableJobsCount; i++) {
        IOJob_t* job = (IOJob_t*) ctx->availableJobs[i];
        free(job->buffer);
        free(job);
    }
}

/* AIO_IOPool_acquireJob:
 * Returns an available io job to be used for a future io. */
static IOJob_t* AIO_IOPool_acquireJob(IOPoolCtx_t *ctx) {
    IOJob_t *job;
    assert(ctx->file != NULL || ctx->prefs->testMode);
    if(ctx->threadPool) {
        ZSTD_pthread_mutex_lock(&ctx->ioJobsMutex);
        assert(ctx->availableJobsCount > 0);
        job = (IOJob_t*) ctx->availableJobs[--ctx->availableJobsCount];
        ZSTD_pthread_mutex_unlock(&ctx->ioJobsMutex);
    } else {
        assert(ctx->availableJobsCount == 1);
        ctx->availableJobsCount--;
        job = (IOJob_t*)ctx->availableJobs[0];
    }
    job->usedBufferSize = 0;
    job->file = ctx->file;
    job->offset = 0;
    return job;
}


/* AIO_IOPool_setFile:
 * Sets the destination file for future files in the pool.
 * Requires completion of all queues write jobs and release of all otherwise acquired jobs.
 * Also requires ending of sparse write if a previous file was used in sparse mode. */
static void AIO_IOPool_setFile(IOPoolCtx_t *ctx, FILE* file) {
    assert(ctx!=NULL);
    AIO_IOPool_join(ctx);
    assert(ctx->availableJobsCount == ctx->totalIoJobs);
    ctx->file = file;
}

static FILE* AIO_IOPool_getFile(IOPoolCtx_t *ctx) {
    return ctx->file;
}

/* AIO_IOPool_enqueueJob:
 * Enqueues an io job for execution.
 * The queued job shouldn't be used directly after queueing it. */
static void AIO_IOPool_enqueueJob(IOJob_t *job) {
    IOPoolCtx_t* ctx = (IOPoolCtx_t *)job->ctx;
    if(ctx->threadPool)
        POOL_add(ctx->threadPool, ctx->poolFunction, job);
    else
        ctx->poolFunction(job);
}

/* ***********************************
 *  WritePool implementation
 *************************************/

/* AIO_WritePool_acquireJob:
 * Returns an available write job to be used for a future write. */
IOJob_t* AIO_WritePool_acquireJob(WritePoolCtx_t *ctx) {
    return AIO_IOPool_acquireJob(&ctx->base);
}

/* AIO_WritePool_enqueueAndReacquireWriteJob:
 * Queues a write job for execution and acquires a new one.
 * After execution `job`'s pointed value would change to the newly acquired job.
 * Make sure to set `usedBufferSize` to the wanted length before call.
 * The queued job shouldn't be used directly after queueing it. */
void AIO_WritePool_enqueueAndReacquireWriteJob(IOJob_t **job) {
    AIO_IOPool_enqueueJob(*job);
    *job = AIO_IOPool_acquireJob((IOPoolCtx_t *)(*job)->ctx);
}

/* AIO_WritePool_sparseWriteEnd:
 * Ends sparse writes to the current file.
 * Blocks on completion of all current write jobs before executing. */
void AIO_WritePool_sparseWriteEnd(WritePoolCtx_t *ctx) {
    assert(ctx != NULL);
    if(ctx->base.threadPool)
        POOL_joinJobs(ctx->base.threadPool);
    AIO_fwriteSparseEnd(ctx->base.prefs, ctx->base.file, ctx->storedSkips);
    ctx->storedSkips = 0;
}

/* AIO_WritePool_setFile:
 * Sets the destination file for future writes in the pool.
 * Requires completion of all queues write jobs and release of all otherwise acquired jobs.
 * Also requires ending of sparse write if a previous file was used in sparse mode. */
void AIO_WritePool_setFile(WritePoolCtx_t *ctx, FILE* file) {
    AIO_IOPool_setFile(&ctx->base, file);
    assert(ctx->storedSkips == 0);
}

/* AIO_WritePool_getFile:
 * Returns the file the writePool is currently set to write to. */
FILE* AIO_WritePool_getFile(WritePoolCtx_t *ctx) {
    return AIO_IOPool_getFile(&ctx->base);
}

/* AIO_WritePool_releaseIoJob:
 * Releases an acquired job back to the pool. Doesn't execute the job. */
void AIO_WritePool_releaseIoJob(IOJob_t *job) {
    AIO_IOPool_releaseIoJob(job);
}

/* AIO_WritePool_closeFile:
 * Ends sparse write and closes the writePool's current file and sets the file to NULL.
 * Requires completion of all queues write jobs and release of all otherwise acquired jobs.  */
int AIO_WritePool_closeFile(WritePoolCtx_t *ctx) {
    FILE *dstFile = ctx->base.file;
    assert(dstFile!=NULL || ctx->base.prefs->testMode!=0);
    AIO_WritePool_sparseWriteEnd(ctx);
    AIO_IOPool_setFile(&ctx->base, NULL);
    return fclose(dstFile);
}

/* AIO_WritePool_executeWriteJob:
 * Executes a write job synchronously. Can be used as a function for a thread pool. */
static void AIO_WritePool_executeWriteJob(void* opaque){
    IOJob_t* job = (IOJob_t*) opaque;
    WritePoolCtx_t* ctx = (WritePoolCtx_t*) job->ctx;
    ctx->storedSkips = AIO_fwriteSparse(job->file, job->buffer, job->usedBufferSize, ctx->base.prefs, ctx->storedSkips);
    AIO_IOPool_releaseIoJob(job);
}

/* AIO_WritePool_create:
 * Allocates and sets and a new write pool including its included jobs. */
WritePoolCtx_t* AIO_WritePool_create(FIO_prefs_t* const prefs, size_t bufferSize) {
    WritePoolCtx_t* ctx = (WritePoolCtx_t*) malloc(sizeof(WritePoolCtx_t));
    if(!ctx) EXM_THROW(100, "Allocation error : not enough memory");
    AIO_IOPool_init(&ctx->base, prefs, AIO_WritePool_executeWriteJob, bufferSize);
    ctx->storedSkips = 0;
    return ctx;
}

/* AIO_WritePool_free:
 * Frees and releases a writePool and its resources. Closes destination file if needs to. */
void AIO_WritePool_free(WritePoolCtx_t* ctx) {
    /* Make sure we finish all tasks and then free the resources */
    if(AIO_WritePool_getFile(ctx))
        AIO_WritePool_closeFile(ctx);
    AIO_IOPool_destroy(&ctx->base);
    assert(ctx->storedSkips==0);
    free(ctx);
}
