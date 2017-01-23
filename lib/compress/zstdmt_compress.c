/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */


/* ======   Tuning parameters   ====== */
#ifndef ZSTDMT_SECTION_LOGSIZE_MIN
#  define ZSTDMT_SECTION_LOGSIZE_MIN 20   /* minimum size for a full compression job (20==2^20==1 MB) */
#endif

#define ZSTDMT_NBTHREADS_MAX 128


/* ======   Compiler specifics   ====== */
#if defined(_MSC_VER)
#  pragma warning(disable : 4204)        /* disable: C4204: non-constant aggregate initializer */
#endif


/* ======   Dependencies   ====== */
#include <stdlib.h>   /* malloc */
#include <string.h>   /* memcpy */
#include <pool.h>     /* threadpool */
#include "threading.h"  /* mutex */
#include "zstd_internal.h"   /* MIN, ERROR, ZSTD_*, ZSTD_highbit32 */
#include "zstdmt_compress.h"


/* ======   Debug   ====== */
#if 0

#  include <stdio.h>
#  include <unistd.h>
#  include <sys/times.h>
   static unsigned g_debugLevel = 3;
#  define DEBUGLOGRAW(l, ...) if (l<=g_debugLevel) { fprintf(stderr, __VA_ARGS__); }
#  define DEBUGLOG(l, ...) if (l<=g_debugLevel) { fprintf(stderr, __FILE__ ": "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, " \n"); }

#  define DEBUG_PRINTHEX(l,p,n) { \
    unsigned debug_u;                   \
    for (debug_u=0; debug_u<(n); debug_u++)           \
        DEBUGLOGRAW(l, "%02X ", ((const unsigned char*)(p))[debug_u]); \
    DEBUGLOGRAW(l, " \n");       \
}

static unsigned long long GetCurrentClockTimeMicroseconds()
{
   static clock_t _ticksPerSecond = 0;
   if (_ticksPerSecond <= 0) _ticksPerSecond = sysconf(_SC_CLK_TCK);

   struct tms junk; clock_t newTicks = (clock_t) times(&junk);
   return ((((unsigned long long)newTicks)*(1000000))/_ticksPerSecond);
}

#define MUTEX_WAIT_TIME_DLEVEL 5
#define PTHREAD_MUTEX_LOCK(mutex) \
if (g_debugLevel>=MUTEX_WAIT_TIME_DLEVEL) { \
   unsigned long long beforeTime = GetCurrentClockTimeMicroseconds(); \
   pthread_mutex_lock(mutex); \
   unsigned long long afterTime = GetCurrentClockTimeMicroseconds(); \
   unsigned long long elapsedTime = (afterTime-beforeTime); \
   if (elapsedTime > 1000) {  /* or whatever threshold you like; I'm using 1 millisecond here */ \
      DEBUGLOG(MUTEX_WAIT_TIME_DLEVEL, "Thread took %llu microseconds to acquire mutex %s \n", \
               elapsedTime, #mutex); \
  } \
} else pthread_mutex_lock(mutex);

#else

#  define DEBUGLOG(l, ...)      {}    /* disabled */
#  define PTHREAD_MUTEX_LOCK(m) pthread_mutex_lock(m)
#  define DEBUG_PRINTHEX(l,p,n) {}

#endif


/* =====   Buffer Pool   ===== */

typedef struct buffer_s {
    void* start;
    size_t size;
} buffer_t;

static const buffer_t g_nullBuffer = { NULL, 0 };

typedef struct ZSTDMT_bufferPool_s {
    unsigned totalBuffers;
    unsigned nbBuffers;
    buffer_t bTable[1];   /* variable size */
} ZSTDMT_bufferPool;

static ZSTDMT_bufferPool* ZSTDMT_createBufferPool(unsigned nbThreads)
{
    unsigned const maxNbBuffers = 2*nbThreads + 2;
    ZSTDMT_bufferPool* const bufPool = (ZSTDMT_bufferPool*)calloc(1, sizeof(ZSTDMT_bufferPool) + (maxNbBuffers-1) * sizeof(buffer_t));
    if (bufPool==NULL) return NULL;
    bufPool->totalBuffers = maxNbBuffers;
    bufPool->nbBuffers = 0;
    return bufPool;
}

static void ZSTDMT_freeBufferPool(ZSTDMT_bufferPool* bufPool)
{
    unsigned u;
    if (!bufPool) return;   /* compatibility with free on NULL */
    for (u=0; u<bufPool->totalBuffers; u++)
        free(bufPool->bTable[u].start);
    free(bufPool);
}

/* assumption : invocation from main thread only ! */
static buffer_t ZSTDMT_getBuffer(ZSTDMT_bufferPool* pool, size_t bSize)
{
    if (pool->nbBuffers) {   /* try to use an existing buffer */
        buffer_t const buf = pool->bTable[--(pool->nbBuffers)];
        size_t const availBufferSize = buf.size;
        if ((availBufferSize >= bSize) & (availBufferSize <= 10*bSize))   /* large enough, but not too much */
            return buf;
        free(buf.start);   /* size conditions not respected : scratch this buffer and create a new one */
    }
    /* create new buffer */
    {   buffer_t buffer;
        void* const start = malloc(bSize);
        if (start==NULL) bSize = 0;
        buffer.start = start;   /* note : start can be NULL if malloc fails ! */
        buffer.size = bSize;
        return buffer;
    }
}

/* store buffer for later re-use, up to pool capacity */
static void ZSTDMT_releaseBuffer(ZSTDMT_bufferPool* pool, buffer_t buf)
{
    if (buf.start == NULL) return;   /* release on NULL */
    if (pool->nbBuffers < pool->totalBuffers) {
        pool->bTable[pool->nbBuffers++] = buf;   /* store for later re-use */
        return;
    }
    /* Reached bufferPool capacity (should not happen) */
    free(buf.start);
}


/* =====   CCtx Pool   ===== */

typedef struct {
    unsigned totalCCtx;
    unsigned availCCtx;
    ZSTD_CCtx* cctx[1];   /* variable size */
} ZSTDMT_CCtxPool;

/* assumption : CCtxPool invocation only from main thread */

/* note : all CCtx borrowed from the pool should be released back to the pool _before_ freeing the pool */
static void ZSTDMT_freeCCtxPool(ZSTDMT_CCtxPool* pool)
{
    unsigned u;
    for (u=0; u<pool->totalCCtx; u++)
        ZSTD_freeCCtx(pool->cctx[u]);  /* note : compatible with free on NULL */
    free(pool);
}

/* ZSTDMT_createCCtxPool() :
 * implies nbThreads >= 1 , checked by caller ZSTDMT_createCCtx() */
static ZSTDMT_CCtxPool* ZSTDMT_createCCtxPool(unsigned nbThreads)
{
    ZSTDMT_CCtxPool* const cctxPool = (ZSTDMT_CCtxPool*) calloc(1, sizeof(ZSTDMT_CCtxPool) + (nbThreads-1)*sizeof(ZSTD_CCtx*));
    if (!cctxPool) return NULL;
    cctxPool->totalCCtx = nbThreads;
    cctxPool->availCCtx = 1;   /* at least one cctx for single-thread mode */
    cctxPool->cctx[0] = ZSTD_createCCtx();
    if (!cctxPool->cctx[0]) { ZSTDMT_freeCCtxPool(cctxPool); return NULL; }
    DEBUGLOG(1, "cctxPool created, with %u threads", nbThreads);
    return cctxPool;
}

static ZSTD_CCtx* ZSTDMT_getCCtx(ZSTDMT_CCtxPool* pool)
{
    if (pool->availCCtx) {
        pool->availCCtx--;
        return pool->cctx[pool->availCCtx];
    }
    return ZSTD_createCCtx();   /* note : can be NULL, when creation fails ! */
}

static void ZSTDMT_releaseCCtx(ZSTDMT_CCtxPool* pool, ZSTD_CCtx* cctx)
{
    if (cctx==NULL) return;   /* compatibility with release on NULL */
    if (pool->availCCtx < pool->totalCCtx)
        pool->cctx[pool->availCCtx++] = cctx;
    else
        /* pool overflow : should not happen, since totalCCtx==nbThreads */
        ZSTD_freeCCtx(cctx);
}


/* =====   Thread worker   ===== */

typedef struct {
    buffer_t buffer;
    size_t filled;
} inBuff_t;

typedef struct {
    ZSTD_CCtx* cctx;
    buffer_t src;
    const void* srcStart;
    size_t   srcSize;
    buffer_t dstBuff;
    size_t   cSize;
    size_t   dstFlushed;
    unsigned firstChunk;
    unsigned lastChunk;
    unsigned jobCompleted;
    pthread_mutex_t* jobCompleted_mutex;
    pthread_cond_t* jobCompleted_cond;
    ZSTD_parameters params;
    ZSTD_CDict* cdict;
    const void* dict;
    size_t dictSize;
    unsigned long long fullFrameSize;
} ZSTDMT_jobDescription;

/* ZSTDMT_compressChunk() : POOL_function type */
void ZSTDMT_compressChunk(void* jobDescription)
{
    ZSTDMT_jobDescription* const job = (ZSTDMT_jobDescription*)jobDescription;
    buffer_t const dstBuff = job->dstBuff;
    if (job->cdict) {
        size_t const initError = ZSTD_compressBegin_usingCDict(job->cctx, job->cdict, job->fullFrameSize);
        if (ZSTD_isError(initError)) { job->cSize = initError; goto _endJob; }
    } else {
        size_t const initError = ZSTD_compressBegin_advanced(job->cctx, job->dict, job->dictSize, job->params, job->fullFrameSize);
        if (ZSTD_isError(initError)) { job->cSize = initError; goto _endJob; }
    }
    if (!job->firstChunk) {  /* flush frame header */
        size_t const hSize = ZSTD_compressContinue(job->cctx, dstBuff.start, dstBuff.size, job->srcStart, 0);
        if (ZSTD_isError(hSize)) { job->cSize = hSize; goto _endJob; }
        ZSTD_invalidateRepCodes(job->cctx);
    }

    DEBUGLOG(4, "Compressing : ");
    DEBUG_PRINTHEX(4, job->srcStart, 12);
    job->cSize = (job->lastChunk) ?   /* last chunk signal */
                 ZSTD_compressEnd(job->cctx, dstBuff.start, dstBuff.size, job->srcStart, job->srcSize) :
                 ZSTD_compressContinue(job->cctx, dstBuff.start, dstBuff.size, job->srcStart, job->srcSize);
    DEBUGLOG(3, "compressed %u bytes into %u bytes   (first:%u) (last:%u)", (unsigned)job->srcSize, (unsigned)job->cSize, job->firstChunk, job->lastChunk);

_endJob:
    PTHREAD_MUTEX_LOCK(job->jobCompleted_mutex);
    job->jobCompleted = 1;
    pthread_cond_signal(job->jobCompleted_cond);
    pthread_mutex_unlock(job->jobCompleted_mutex);
}


/* ------------------------------------------ */
/* =====   Multi-threaded compression   ===== */
/* ------------------------------------------ */

struct ZSTDMT_CCtx_s {
    POOL_ctx* factory;
    ZSTDMT_bufferPool* buffPool;
    ZSTDMT_CCtxPool* cctxPool;
    pthread_mutex_t jobCompleted_mutex;
    pthread_cond_t jobCompleted_cond;
    size_t targetSectionSize;
    size_t inBuffSize;
    inBuff_t inBuff;
    ZSTD_parameters params;
    unsigned nbThreads;
    unsigned jobIDMask;
    unsigned doneJobID;
    unsigned nextJobID;
    unsigned frameEnded;
    unsigned allJobsCompleted;
    unsigned long long frameContentSize;
    ZSTD_CDict* cdict;
    ZSTD_CStream* cstream;
    ZSTDMT_jobDescription jobs[1];   /* variable size (must lies at the end) */
};

ZSTDMT_CCtx *ZSTDMT_createCCtx(unsigned nbThreads)
{
    ZSTDMT_CCtx* cctx;
    U32 const minNbJobs = nbThreads + 2;
    U32 const nbJobsLog2 = ZSTD_highbit32(minNbJobs) + 1;
    U32 const nbJobs = 1 << nbJobsLog2;
    DEBUGLOG(5, "nbThreads : %u  ; minNbJobs : %u ;  nbJobsLog2 : %u ;  nbJobs : %u  \n",
            nbThreads, minNbJobs, nbJobsLog2, nbJobs);
    if ((nbThreads < 1) | (nbThreads > ZSTDMT_NBTHREADS_MAX)) return NULL;
    cctx = (ZSTDMT_CCtx*) calloc(1, sizeof(ZSTDMT_CCtx) + nbJobs*sizeof(ZSTDMT_jobDescription));
    if (!cctx) return NULL;
    cctx->nbThreads = nbThreads;
    cctx->jobIDMask = nbJobs - 1;
    cctx->allJobsCompleted = 1;
    cctx->factory = POOL_create(nbThreads, 1);
    cctx->buffPool = ZSTDMT_createBufferPool(nbThreads);
    cctx->cctxPool = ZSTDMT_createCCtxPool(nbThreads);
    if (!cctx->factory | !cctx->buffPool | !cctx->cctxPool) {  /* one object was not created */
        ZSTDMT_freeCCtx(cctx);
        return NULL;
    }
    if (nbThreads==1) {
        cctx->cstream = ZSTD_createCStream();
        if (!cctx->cstream) {
            ZSTDMT_freeCCtx(cctx); return NULL;
    }   }
    pthread_mutex_init(&cctx->jobCompleted_mutex, NULL);   /* Todo : check init function return */
    pthread_cond_init(&cctx->jobCompleted_cond, NULL);
    DEBUGLOG(4, "mt_cctx created, for %u threads \n", nbThreads);
    return cctx;
}

/* ZSTDMT_releaseAllJobResources() :
 * Ensure all workers are killed first. */
static void ZSTDMT_releaseAllJobResources(ZSTDMT_CCtx* mtctx)
{
    unsigned jobID;
    for (jobID=0; jobID <= mtctx->jobIDMask; jobID++) {
        ZSTDMT_releaseBuffer(mtctx->buffPool, mtctx->jobs[jobID].dstBuff);
        mtctx->jobs[jobID].dstBuff = g_nullBuffer;
        ZSTDMT_releaseBuffer(mtctx->buffPool, mtctx->jobs[jobID].src);
        mtctx->jobs[jobID].src = g_nullBuffer;
        ZSTDMT_releaseCCtx(mtctx->cctxPool, mtctx->jobs[jobID].cctx);
        mtctx->jobs[jobID].cctx = NULL;
    }
    memset(mtctx->jobs, 0, (mtctx->jobIDMask+1)*sizeof(ZSTDMT_jobDescription));
    ZSTDMT_releaseBuffer(mtctx->buffPool, mtctx->inBuff.buffer);
    mtctx->inBuff.buffer = g_nullBuffer;
    mtctx->allJobsCompleted = 1;
}

size_t ZSTDMT_freeCCtx(ZSTDMT_CCtx* mtctx)
{
    if (mtctx==NULL) return 0;   /* compatible with free on NULL */
    POOL_free(mtctx->factory);
    if (!mtctx->allJobsCompleted) ZSTDMT_releaseAllJobResources(mtctx); /* stop workers first */
    ZSTDMT_freeBufferPool(mtctx->buffPool);  /* release job resources into pools first */
    ZSTDMT_freeCCtxPool(mtctx->cctxPool);
    ZSTD_freeCDict(mtctx->cdict);
    ZSTD_freeCStream(mtctx->cstream);
    pthread_mutex_destroy(&mtctx->jobCompleted_mutex);
    pthread_cond_destroy(&mtctx->jobCompleted_cond);
    free(mtctx);
    return 0;
}


size_t ZSTDMT_compressCCtx(ZSTDMT_CCtx* mtctx,
                           void* dst, size_t dstCapacity,
                     const void* src, size_t srcSize,
                           int compressionLevel)
{
    ZSTD_parameters params = ZSTD_getParams(compressionLevel, srcSize, 0);
    size_t const chunkTargetSize = (size_t)1 << (params.cParams.windowLog + 2);
    unsigned const nbChunksMax = (unsigned)(srcSize / chunkTargetSize) + (srcSize < chunkTargetSize) /* min 1 */;
    unsigned nbChunks = MIN(nbChunksMax, mtctx->nbThreads);
    size_t const proposedChunkSize = (srcSize + (nbChunks-1)) / nbChunks;
    size_t const avgChunkSize = ((proposedChunkSize & 0x1FFFF) < 0xFFFF) ? proposedChunkSize + 0xFFFF : proposedChunkSize;   /* avoid too small last block */
    size_t remainingSrcSize = srcSize;
    const char* const srcStart = (const char*)src;
    size_t frameStartPos = 0;

    DEBUGLOG(3, "windowLog : %2u => chunkTargetSize : %u bytes  ", params.cParams.windowLog, (U32)chunkTargetSize);
    DEBUGLOG(2, "nbChunks  : %2u   (chunkSize : %u bytes)   ", nbChunks, (U32)avgChunkSize);
    params.fParams.contentSizeFlag = 1;

    if (nbChunks==1) {   /* fallback to single-thread mode */
        ZSTD_CCtx* const cctx = mtctx->cctxPool->cctx[0];
        return ZSTD_compressCCtx(cctx, dst, dstCapacity, src, srcSize, compressionLevel);
    }

    {   unsigned u;
        for (u=0; u<nbChunks; u++) {
            size_t const chunkSize = MIN(remainingSrcSize, avgChunkSize);
            size_t const dstBufferCapacity = u ? ZSTD_compressBound(chunkSize) : dstCapacity;
            buffer_t const dstAsBuffer = { dst, dstCapacity };
            buffer_t const dstBuffer = u ? ZSTDMT_getBuffer(mtctx->buffPool, dstBufferCapacity) : dstAsBuffer;
            ZSTD_CCtx* const cctx = ZSTDMT_getCCtx(mtctx->cctxPool);

            if ((cctx==NULL) || (dstBuffer.start==NULL)) {
                mtctx->jobs[u].cSize = ERROR(memory_allocation);   /* job result */
                mtctx->jobs[u].jobCompleted = 1;
                nbChunks = u+1;
                break;   /* let's wait for previous jobs to complete, but don't start new ones */
            }

            mtctx->jobs[u].srcStart = srcStart + frameStartPos;
            mtctx->jobs[u].srcSize = chunkSize;
            mtctx->jobs[u].fullFrameSize = srcSize;
            mtctx->jobs[u].params = params;
            mtctx->jobs[u].dstBuff = dstBuffer;
            mtctx->jobs[u].cctx = cctx;
            mtctx->jobs[u].firstChunk = (u==0);
            mtctx->jobs[u].lastChunk = (u==nbChunks-1);
            mtctx->jobs[u].jobCompleted = 0;
            mtctx->jobs[u].jobCompleted_mutex = &mtctx->jobCompleted_mutex;
            mtctx->jobs[u].jobCompleted_cond = &mtctx->jobCompleted_cond;

            DEBUGLOG(3, "posting job %u   (%u bytes)", u, (U32)chunkSize);
            DEBUG_PRINTHEX(3, mtctx->jobs[u].srcStart, 12);
            POOL_add(mtctx->factory, ZSTDMT_compressChunk, &mtctx->jobs[u]);

            frameStartPos += chunkSize;
            remainingSrcSize -= chunkSize;
    }   }
    /* note : since nbChunks <= nbThreads, all jobs should be running immediately in parallel */

    {   unsigned chunkID;
        size_t error = 0, dstPos = 0;
        for (chunkID=0; chunkID<nbChunks; chunkID++) {
            DEBUGLOG(3, "waiting for chunk %u ", chunkID);
            PTHREAD_MUTEX_LOCK(&mtctx->jobCompleted_mutex);
            while (mtctx->jobs[chunkID].jobCompleted==0) {
                DEBUGLOG(4, "waiting for jobCompleted signal from chunk %u", chunkID);
                pthread_cond_wait(&mtctx->jobCompleted_cond, &mtctx->jobCompleted_mutex);
            }
            pthread_mutex_unlock(&mtctx->jobCompleted_mutex);
            DEBUGLOG(3, "ready to write chunk %u ", chunkID);

            ZSTDMT_releaseCCtx(mtctx->cctxPool, mtctx->jobs[chunkID].cctx);
            mtctx->jobs[chunkID].cctx = NULL;
            mtctx->jobs[chunkID].srcStart = NULL;
            {   size_t const cSize = mtctx->jobs[chunkID].cSize;
                if (ZSTD_isError(cSize)) error = cSize;
                if ((!error) && (dstPos + cSize > dstCapacity)) error = ERROR(dstSize_tooSmall);
                if (chunkID) {   /* note : chunk 0 is already written directly into dst */
                    if (!error) memcpy((char*)dst + dstPos, mtctx->jobs[chunkID].dstBuff.start, cSize);
                    ZSTDMT_releaseBuffer(mtctx->buffPool, mtctx->jobs[chunkID].dstBuff);
                    mtctx->jobs[chunkID].dstBuff = g_nullBuffer;
                }
                dstPos += cSize ;
            }
        }
        if (!error) DEBUGLOG(3, "compressed size : %u  ", (U32)dstPos);
        return error ? error : dstPos;
    }

}


/* ====================================== */
/* =======      Streaming API     ======= */
/* ====================================== */

static void ZSTDMT_waitForAllJobsCompleted(ZSTDMT_CCtx* zcs) {
    while (zcs->doneJobID < zcs->nextJobID) {
        unsigned const jobID = zcs->doneJobID & zcs->jobIDMask;
        PTHREAD_MUTEX_LOCK(&zcs->jobCompleted_mutex);
        while (zcs->jobs[jobID].jobCompleted==0) {
            DEBUGLOG(4, "waiting for jobCompleted signal from chunk %u", zcs->doneJobID);   /* we want to block when waiting for data to flush */
            pthread_cond_wait(&zcs->jobCompleted_cond, &zcs->jobCompleted_mutex);
        }
        pthread_mutex_unlock(&zcs->jobCompleted_mutex);
        zcs->doneJobID++;
    }
}


static size_t ZSTDMT_initCStream_internal(ZSTDMT_CCtx* zcs,
                                    const void* dict, size_t dictSize, unsigned updateDict,
                                    ZSTD_parameters params, unsigned long long pledgedSrcSize)
{
    ZSTD_customMem const cmem = { NULL, NULL, NULL };
    if (zcs->nbThreads==1) return ZSTD_initCStream_advanced(zcs->cstream, dict, dictSize, params, pledgedSrcSize);
    if (zcs->allJobsCompleted == 0) {   /* previous job not correctly finished */
        ZSTDMT_waitForAllJobsCompleted(zcs);
        ZSTDMT_releaseAllJobResources(zcs);
        zcs->allJobsCompleted = 1;
    }
    params.fParams.checksumFlag = 0;   /* current limitation : no checksum (to be lifted in a later version) */
    zcs->params = params;
    if (updateDict) {
        ZSTD_freeCDict(zcs->cdict); zcs->cdict = NULL;
        if (dict && dictSize) {
            zcs->cdict = ZSTD_createCDict_advanced(dict, dictSize, 0, params, cmem);
            if (zcs->cdict == NULL) return ERROR(memory_allocation);
    }   }
    zcs->frameContentSize = pledgedSrcSize;
    zcs->targetSectionSize = (size_t)1 << MAX(ZSTDMT_SECTION_LOGSIZE_MIN, (zcs->params.cParams.windowLog + 2));
    zcs->inBuffSize = zcs->targetSectionSize + ((size_t)1 << zcs->params.cParams.windowLog);
    zcs->inBuff.buffer = ZSTDMT_getBuffer(zcs->buffPool, zcs->inBuffSize);
    if (zcs->inBuff.buffer.start == NULL) return ERROR(memory_allocation);
    zcs->inBuff.filled = 0;
    zcs->doneJobID = 0;
    zcs->nextJobID = 0;
    zcs->frameEnded = 0;
    zcs->allJobsCompleted = 0;
    return 0;
}

size_t ZSTDMT_initCStream_advanced(ZSTDMT_CCtx* zcs,
                                const void* dict, size_t dictSize,
                                ZSTD_parameters params, unsigned long long pledgedSrcSize)
{
    return ZSTDMT_initCStream_internal(zcs, dict, dictSize, 1, params, pledgedSrcSize);
}

/* ZSTDMT_resetCStream() :
 * pledgedSrcSize is optional and can be zero == unknown */
size_t ZSTDMT_resetCStream(ZSTDMT_CCtx* zcs, unsigned long long pledgedSrcSize)
{
    if (zcs->nbThreads==1) return ZSTD_resetCStream(zcs->cstream, pledgedSrcSize);
    return ZSTDMT_initCStream_internal(zcs, NULL, 0, 0, zcs->params, pledgedSrcSize);
}

size_t ZSTDMT_initCStream(ZSTDMT_CCtx* zcs, int compressionLevel) {
    ZSTD_parameters const params = ZSTD_getParams(compressionLevel, 0, 0);
    return ZSTDMT_initCStream_internal(zcs, NULL, 0, 1, params, 0);
}


/* ZSTDMT_flushNextJob() :
 * output : will be updated with amount of data flushed .
 * blockToFlush : the function will block and wait if there is no data available to flush .
 * @return : amount of data remaining within internal buffer, 1 if unknown but > 0, 0 if no more */
static size_t ZSTDMT_flushNextJob(ZSTDMT_CCtx* zcs, ZSTD_outBuffer* output, unsigned blockToFlush)
{
    unsigned const wJobID = zcs->doneJobID & zcs->jobIDMask;
    if (zcs->doneJobID == zcs->nextJobID) return 0;   /* all flushed ! */
    PTHREAD_MUTEX_LOCK(&zcs->jobCompleted_mutex);
    while (zcs->jobs[wJobID].jobCompleted==0) {
        DEBUGLOG(5, "waiting for jobCompleted signal from job %u", zcs->doneJobID);    /* block when nothing available to flush */
        if (!blockToFlush) { pthread_mutex_unlock(&zcs->jobCompleted_mutex); return 0; } /* nothing ready to be flushed => skip */
        pthread_cond_wait(&zcs->jobCompleted_cond, &zcs->jobCompleted_mutex);
    }
    pthread_mutex_unlock(&zcs->jobCompleted_mutex);
    /* compression job completed : output can be flushed */
    {   ZSTDMT_jobDescription job = zcs->jobs[wJobID];
        size_t const toWrite = MIN(job.cSize - job.dstFlushed, output->size - output->pos);
        DEBUGLOG(4, "Flushing %u bytes from job %u ", (U32)toWrite, zcs->doneJobID);
        ZSTDMT_releaseCCtx(zcs->cctxPool, job.cctx);
        zcs->jobs[wJobID].cctx = NULL;
        ZSTDMT_releaseBuffer(zcs->buffPool, job.src);
        zcs->jobs[wJobID].srcStart = NULL;
        zcs->jobs[wJobID].src = g_nullBuffer;
        if (ZSTD_isError(job.cSize)) {
            ZSTDMT_waitForAllJobsCompleted(zcs);
            ZSTDMT_releaseAllJobResources(zcs);
            return job.cSize;
        }
        memcpy((char*)output->dst + output->pos, (const char*)job.dstBuff.start + job.dstFlushed, toWrite);
        output->pos += toWrite;
        job.dstFlushed += toWrite;
        if (job.dstFlushed == job.cSize) {   /* output buffer fully flushed => move to next one */
            ZSTDMT_releaseBuffer(zcs->buffPool, job.dstBuff);
            zcs->jobs[wJobID].dstBuff = g_nullBuffer;
            zcs->jobs[wJobID].jobCompleted = 0;
            zcs->doneJobID++;
        } else {
            zcs->jobs[wJobID].dstFlushed = job.dstFlushed;
        }
        /* return value : how many bytes left in buffer ; fake it to 1 if unknown but >0 */
        if (job.cSize > job.dstFlushed) return (job.cSize - job.dstFlushed);
        if (zcs->doneJobID < zcs->nextJobID) return 1;   /* still some buffer to flush */
        zcs->allJobsCompleted = zcs->frameEnded;   /* frame completed and entirely flushed */
        return 0;   /* everything flushed */
}   }


size_t ZSTDMT_compressStream(ZSTDMT_CCtx* zcs, ZSTD_outBuffer* output, ZSTD_inBuffer* input)
{
    if (zcs->frameEnded) return ERROR(stage_wrong);   /* current frame being ended. Only flush is allowed. Restart with init */
    if (zcs->nbThreads==1) return ZSTD_compressStream(zcs->cstream, output, input);

    /* fill input buffer */
    {   size_t const toLoad = MIN(input->size - input->pos, zcs->inBuffSize - zcs->inBuff.filled);
        memcpy((char*)zcs->inBuff.buffer.start + zcs->inBuff.filled, input->src, toLoad);
        input->pos += toLoad;
        zcs->inBuff.filled += toLoad;
    }

    if ( (zcs->inBuff.filled == zcs->inBuffSize)  /* filled enough : let's compress */
        && (zcs->nextJobID <= zcs->doneJobID + zcs->jobIDMask) ) {   /* avoid overwriting job round buffer */
        size_t const dstBufferCapacity = ZSTD_compressBound(zcs->targetSectionSize);
        buffer_t const dstBuffer = ZSTDMT_getBuffer(zcs->buffPool, dstBufferCapacity);
        ZSTD_CCtx* const cctx = ZSTDMT_getCCtx(zcs->cctxPool);
        unsigned const jobID = zcs->nextJobID & zcs->jobIDMask;

        if ((cctx==NULL) || (dstBuffer.start==NULL)) {
            zcs->jobs[jobID].jobCompleted = 1;
            zcs->nextJobID++;
            ZSTDMT_waitForAllJobsCompleted(zcs);
            ZSTDMT_releaseAllJobResources(zcs);
            return ERROR(memory_allocation);
        }

        DEBUGLOG(1, "preparing job %u to compress %u bytes \n", (U32)zcs->nextJobID, (U32)zcs->targetSectionSize);
        zcs->jobs[jobID].src = zcs->inBuff.buffer;
        zcs->jobs[jobID].srcStart = zcs->inBuff.buffer.start;
        zcs->jobs[jobID].srcSize = zcs->targetSectionSize;
        zcs->jobs[jobID].params = zcs->params;
        zcs->jobs[jobID].cdict = zcs->nextJobID==0 ? zcs->cdict : NULL;
        zcs->jobs[jobID].dict = NULL;
        zcs->jobs[jobID].dictSize = 0;
        zcs->jobs[jobID].fullFrameSize = zcs->frameContentSize;
        zcs->jobs[jobID].dstBuff = dstBuffer;
        zcs->jobs[jobID].cctx = cctx;
        zcs->jobs[jobID].firstChunk = (zcs->nextJobID==0);
        zcs->jobs[jobID].lastChunk = 0;
        zcs->jobs[jobID].jobCompleted = 0;
        zcs->jobs[jobID].dstFlushed = 0;
        zcs->jobs[jobID].jobCompleted_mutex = &zcs->jobCompleted_mutex;
        zcs->jobs[jobID].jobCompleted_cond = &zcs->jobCompleted_cond;

        /* get a new buffer for next input - save remaining into it */
        zcs->inBuff.buffer = ZSTDMT_getBuffer(zcs->buffPool, zcs->inBuffSize);
        if (zcs->inBuff.buffer.start == NULL) {   /* not enough memory to allocate next input buffer */
            zcs->jobs[jobID].jobCompleted = 1;
            zcs->nextJobID++;
            ZSTDMT_waitForAllJobsCompleted(zcs);
            ZSTDMT_releaseAllJobResources(zcs);
            return ERROR(memory_allocation);
        }
        zcs->inBuff.filled = (U32)(zcs->inBuffSize - zcs->targetSectionSize);
        memcpy(zcs->inBuff.buffer.start, (const char*)zcs->jobs[jobID].srcStart + zcs->targetSectionSize, zcs->inBuff.filled);

        DEBUGLOG(3, "posting job %u   (%u bytes)  (note : doneJob = %u=>%u)", zcs->nextJobID, (U32)zcs->jobs[jobID].srcSize, zcs->doneJobID, zcs->doneJobID & zcs->jobIDMask);
        POOL_add(zcs->factory, ZSTDMT_compressChunk, &zcs->jobs[jobID]);   /* This call is blocking if all workers are busy */
        zcs->nextJobID++;
    }

    /* check if there is any data available to flush */
    ZSTDMT_flushNextJob(zcs, output, (zcs->inBuff.filled == zcs->inBuffSize));  /* we'll block if it wasn't possible to create new job due to saturation */
#if 0
    {   unsigned const jobID = zcs->doneJobID & zcs->jobIDMask;
        unsigned jobCompleted;
        pthread_mutex_lock(&zcs->jobCompleted_mutex);
        while (zcs->jobs[jobID].jobCompleted == 0 && zcs->inBuff.filled == zcs->inBuffSize) {
            /* when no new job could be started, block until there is something to flush, ensuring forward progress */
            pthread_cond_wait(&zcs->jobCompleted_cond, &zcs->jobCompleted_mutex);
        }
        jobCompleted = zcs->jobs[jobID].jobCompleted;
        pthread_mutex_unlock(&zcs->jobCompleted_mutex);
        if (jobCompleted) {
            ZSTDMT_jobDescription const job = zcs->jobs[jobID];
            size_t const toWrite = MIN(job.cSize - job.dstFlushed, output->size - output->pos);
            DEBUGLOG(1, "flush %u bytes from job %u ", (U32)toWrite, zcs->doneJobID);
            ZSTDMT_releaseCCtx(zcs->cctxPool, job.cctx);
            zcs->jobs[jobID].cctx = NULL;
            ZSTDMT_releaseBuffer(zcs->buffPool, job.src);
            zcs->jobs[jobID].srcStart = NULL; zcs->jobs[jobID].src = g_nullBuffer;
            if (ZSTD_isError(job.cSize)) {
                ZSTDMT_waitForAllJobsCompleted(zcs);
                ZSTDMT_releaseAllJobResources(zcs);
                return job.cSize;
            }
            memcpy((char*)output->dst + output->pos, (const char*)job.dstBuff.start + job.dstFlushed, toWrite);
            output->pos += toWrite;
            zcs->jobs[jobID].dstFlushed += toWrite;
            DEBUGLOG(1, "remaining : %u bytes ", (U32)(job.cSize - job.dstFlushed));
            if (zcs->jobs[jobID].dstFlushed == job.cSize) {   /* output buffer fully flushed => go to next one */
                ZSTDMT_releaseBuffer(zcs->buffPool, job.dstBuff);
                zcs->jobs[jobID].dstBuff = g_nullBuffer;
                zcs->jobs[jobID].jobCompleted = 0;
                zcs->doneJobID++;
    }   }   }
#endif
    /* recommended next input size : fill current input buffer */
    return zcs->inBuffSize - zcs->inBuff.filled;
}


static size_t ZSTDMT_flushStream_internal(ZSTDMT_CCtx* zcs, ZSTD_outBuffer* output, unsigned endFrame)
{
    size_t const srcSize = zcs->inBuff.filled;

    DEBUGLOG(1, "flushing : %u bytes to compress", (U32)srcSize);
    if ( ((srcSize > 0) || (endFrame && !zcs->frameEnded))
       && (zcs->nextJobID <= zcs->doneJobID + zcs->jobIDMask) ) {
        size_t const dstBufferCapacity = ZSTD_compressBound(srcSize);
        buffer_t const dstBuffer = ZSTDMT_getBuffer(zcs->buffPool, dstBufferCapacity);
        ZSTD_CCtx* const cctx = ZSTDMT_getCCtx(zcs->cctxPool);
        unsigned const jobID = zcs->nextJobID & zcs->jobIDMask;

        if ((cctx==NULL) || (dstBuffer.start==NULL)) {
            zcs->jobs[jobID].jobCompleted = 1;
            zcs->nextJobID++;
            ZSTDMT_waitForAllJobsCompleted(zcs);
            ZSTDMT_releaseAllJobResources(zcs);
            return ERROR(memory_allocation);
        }

        zcs->jobs[jobID].src = zcs->inBuff.buffer;
        zcs->jobs[jobID].srcStart = zcs->inBuff.buffer.start;
        zcs->jobs[jobID].srcSize = srcSize;
        zcs->jobs[jobID].params = zcs->params;
        zcs->jobs[jobID].cdict = zcs->nextJobID==0 ? zcs->cdict : NULL;
        zcs->jobs[jobID].dict = NULL;
        zcs->jobs[jobID].dictSize = 0;
        zcs->jobs[jobID].fullFrameSize = zcs->frameContentSize;
        zcs->jobs[jobID].dstBuff = dstBuffer;
        zcs->jobs[jobID].cctx = cctx;
        zcs->jobs[jobID].firstChunk = (zcs->nextJobID==0);
        zcs->jobs[jobID].lastChunk = endFrame;
        zcs->jobs[jobID].jobCompleted = 0;
        zcs->jobs[jobID].dstFlushed = 0;
        zcs->jobs[jobID].jobCompleted_mutex = &zcs->jobCompleted_mutex;
        zcs->jobs[jobID].jobCompleted_cond = &zcs->jobCompleted_cond;

        /* get a new buffer for next input */
        if (!endFrame) {
            zcs->inBuff.buffer = ZSTDMT_getBuffer(zcs->buffPool, zcs->inBuffSize);
            zcs->inBuff.filled = 0;
            if (zcs->inBuff.buffer.start == NULL) {   /* not enough memory to allocate next input buffer */
                zcs->jobs[jobID].jobCompleted = 1;
                zcs->nextJobID++;
                ZSTDMT_waitForAllJobsCompleted(zcs);
                ZSTDMT_releaseAllJobResources(zcs);
                return ERROR(memory_allocation);
            }
        } else {
            zcs->inBuff.buffer = g_nullBuffer;
            zcs->inBuff.filled = 0;
            zcs->frameEnded = 1;
        }

        DEBUGLOG(3, "posting job %u : %u bytes  (end:%u) (note : doneJob = %u=>%u)", zcs->nextJobID, (U32)zcs->jobs[jobID].srcSize, zcs->jobs[jobID].lastChunk, zcs->doneJobID, zcs->doneJobID & zcs->jobIDMask);
        POOL_add(zcs->factory, ZSTDMT_compressChunk, &zcs->jobs[jobID]);   /* this call is blocking when thread worker pool is exhausted */
        zcs->nextJobID++;
    }

    /* check if there is any data available to flush */
    DEBUGLOG(5, "zcs->doneJobID : %u  ; zcs->nextJobID : %u ", zcs->doneJobID, zcs->nextJobID);
    return ZSTDMT_flushNextJob(zcs, output, 1);

#if 0
    {   unsigned const wJobID = zcs->doneJobID & zcs->jobIDMask;
        PTHREAD_MUTEX_LOCK(&zcs->jobCompleted_mutex);
        while (zcs->jobs[wJobID].jobCompleted==0) {
            DEBUGLOG(5, "waiting for jobCompleted signal from job %u", zcs->doneJobID);   /* block when nothing available to flush */
            pthread_cond_wait(&zcs->jobCompleted_cond, &zcs->jobCompleted_mutex);
        }
        pthread_mutex_unlock(&zcs->jobCompleted_mutex);
        /* compression job completed : output can be flushed */
        {   ZSTDMT_jobDescription job = zcs->jobs[wJobID];
            size_t const toWrite = MIN(job.cSize - job.dstFlushed, output->size - output->pos);
            DEBUGLOG(4, "Flushing %u bytes from job %u ", (U32)toWrite, zcs->doneJobID);
            ZSTDMT_releaseCCtx(zcs->cctxPool, job.cctx); zcs->jobs[wJobID].cctx = NULL;   /* release cctx for future task */
            ZSTDMT_releaseBuffer(zcs->buffPool, job.src); zcs->jobs[wJobID].srcStart = NULL; zcs->jobs[wJobID].src = g_nullBuffer;
            if (ZSTD_isError(job.cSize)) {
                ZSTDMT_waitForAllJobsCompleted(zcs);
                ZSTDMT_releaseAllJobResources(zcs);
                return job.cSize;
            }
            memcpy((char*)output->dst + output->pos, (const char*)job.dstBuff.start + job.dstFlushed, toWrite);
            output->pos += toWrite;
            job.dstFlushed += toWrite;
            if (job.dstFlushed == job.cSize) {   /* output buffer fully flushed => next one */
                ZSTDMT_releaseBuffer(zcs->buffPool, job.dstBuff); zcs->jobs[wJobID].dstBuff = g_nullBuffer;
                zcs->jobs[wJobID].jobCompleted = 0;
                zcs->doneJobID++;
            } else {
                zcs->jobs[wJobID].dstFlushed = job.dstFlushed;
            }
            /* return value : how many bytes left in buffer ; fake it to 1 if unknown but >0 */
            if (job.cSize > job.dstFlushed) return (job.cSize - job.dstFlushed);
            if ((zcs->doneJobID < zcs->nextJobID) || (zcs->inBuff.filled)) return 1;   /* still some buffer to flush */
            zcs->allJobsCompleted = zcs->frameEnded;
            return 0;
    }   }
#endif
}


size_t ZSTDMT_flushStream(ZSTDMT_CCtx* zcs, ZSTD_outBuffer* output)
{
    if (zcs->nbThreads==1) return ZSTD_flushStream(zcs->cstream, output);
    return ZSTDMT_flushStream_internal(zcs, output, 0);
}

size_t ZSTDMT_endStream(ZSTDMT_CCtx* zcs, ZSTD_outBuffer* output)
{
    if (zcs->nbThreads==1) return ZSTD_endStream(zcs->cstream, output);
    return ZSTDMT_flushStream_internal(zcs, output, 1);
}
