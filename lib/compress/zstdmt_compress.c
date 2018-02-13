/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */


/* ======   Tuning parameters   ====== */
#define ZSTDMT_NBWORKERS_MAX 200
#define ZSTDMT_JOBSIZE_MAX  (MEM_32bits() ? (512 MB) : (2 GB))  /* note : limited by `jobSize` type, which is `unsigned` */
#define ZSTDMT_OVERLAPLOG_DEFAULT 6


/* ======   Compiler specifics   ====== */
#if defined(_MSC_VER)
#  pragma warning(disable : 4204)   /* disable: C4204: non-constant aggregate initializer */
#endif


/* ======   Dependencies   ====== */
#include <string.h>      /* memcpy, memset */
#include <limits.h>      /* INT_MAX */
#include "pool.h"        /* threadpool */
#include "threading.h"   /* mutex */
#include "zstd_compress_internal.h"  /* MIN, ERROR, ZSTD_*, ZSTD_highbit32 */
#include "zstdmt_compress.h"


/* ======   Debug   ====== */
#if defined(ZSTD_DEBUG) && (ZSTD_DEBUG>=2)

#  include <stdio.h>
#  include <unistd.h>
#  include <sys/times.h>
#  define DEBUGLOGRAW(l, ...) if (l<=ZSTD_DEBUG) { fprintf(stderr, __VA_ARGS__); }

#  define DEBUG_PRINTHEX(l,p,n) {            \
    unsigned debug_u;                        \
    for (debug_u=0; debug_u<(n); debug_u++)  \
        DEBUGLOGRAW(l, "%02X ", ((const unsigned char*)(p))[debug_u]); \
    DEBUGLOGRAW(l, " \n");                   \
}

static unsigned long long GetCurrentClockTimeMicroseconds(void)
{
   static clock_t _ticksPerSecond = 0;
   if (_ticksPerSecond <= 0) _ticksPerSecond = sysconf(_SC_CLK_TCK);

   { struct tms junk; clock_t newTicks = (clock_t) times(&junk);
     return ((((unsigned long long)newTicks)*(1000000))/_ticksPerSecond); }
}

#define MUTEX_WAIT_TIME_DLEVEL 6
#define ZSTD_PTHREAD_MUTEX_LOCK(mutex) {          \
    if (ZSTD_DEBUG >= MUTEX_WAIT_TIME_DLEVEL) {   \
        unsigned long long const beforeTime = GetCurrentClockTimeMicroseconds(); \
        ZSTD_pthread_mutex_lock(mutex);           \
        {   unsigned long long const afterTime = GetCurrentClockTimeMicroseconds(); \
            unsigned long long const elapsedTime = (afterTime-beforeTime); \
            if (elapsedTime > 1000) {  /* or whatever threshold you like; I'm using 1 millisecond here */ \
                DEBUGLOG(MUTEX_WAIT_TIME_DLEVEL, "Thread took %llu microseconds to acquire mutex %s \n", \
                   elapsedTime, #mutex);          \
        }   }                                     \
    } else {                                      \
        ZSTD_pthread_mutex_lock(mutex);           \
    }                                             \
}

#else

#  define ZSTD_PTHREAD_MUTEX_LOCK(m) ZSTD_pthread_mutex_lock(m)
#  define DEBUG_PRINTHEX(l,p,n) {}

#endif


/* =====   Buffer Pool   ===== */
/* a single Buffer Pool can be invoked from multiple threads in parallel */

typedef struct buffer_s {
    void* start;
    size_t capacity;
} buffer_t;

static const buffer_t g_nullBuffer = { NULL, 0 };

typedef struct ZSTDMT_bufferPool_s {
    ZSTD_pthread_mutex_t poolMutex;
    size_t bufferSize;
    unsigned totalBuffers;
    unsigned nbBuffers;
    ZSTD_customMem cMem;
    buffer_t bTable[1];   /* variable size */
} ZSTDMT_bufferPool;

static ZSTDMT_bufferPool* ZSTDMT_createBufferPool(unsigned nbWorkers, ZSTD_customMem cMem)
{
    unsigned const maxNbBuffers = 2*nbWorkers + 3;
    ZSTDMT_bufferPool* const bufPool = (ZSTDMT_bufferPool*)ZSTD_calloc(
        sizeof(ZSTDMT_bufferPool) + (maxNbBuffers-1) * sizeof(buffer_t), cMem);
    if (bufPool==NULL) return NULL;
    if (ZSTD_pthread_mutex_init(&bufPool->poolMutex, NULL)) {
        ZSTD_free(bufPool, cMem);
        return NULL;
    }
    bufPool->bufferSize = 64 KB;
    bufPool->totalBuffers = maxNbBuffers;
    bufPool->nbBuffers = 0;
    bufPool->cMem = cMem;
    return bufPool;
}

static void ZSTDMT_freeBufferPool(ZSTDMT_bufferPool* bufPool)
{
    unsigned u;
    DEBUGLOG(3, "ZSTDMT_freeBufferPool (address:%08X)", (U32)(size_t)bufPool);
    if (!bufPool) return;   /* compatibility with free on NULL */
    for (u=0; u<bufPool->totalBuffers; u++) {
        DEBUGLOG(4, "free buffer %2u (address:%08X)", u, (U32)(size_t)bufPool->bTable[u].start);
        ZSTD_free(bufPool->bTable[u].start, bufPool->cMem);
    }
    ZSTD_pthread_mutex_destroy(&bufPool->poolMutex);
    ZSTD_free(bufPool, bufPool->cMem);
}

/* only works at initialization, not during compression */
static size_t ZSTDMT_sizeof_bufferPool(ZSTDMT_bufferPool* bufPool)
{
    size_t const poolSize = sizeof(*bufPool)
                          + (bufPool->totalBuffers - 1) * sizeof(buffer_t);
    unsigned u;
    size_t totalBufferSize = 0;
    ZSTD_pthread_mutex_lock(&bufPool->poolMutex);
    for (u=0; u<bufPool->totalBuffers; u++)
        totalBufferSize += bufPool->bTable[u].capacity;
    ZSTD_pthread_mutex_unlock(&bufPool->poolMutex);

    return poolSize + totalBufferSize;
}

/* ZSTDMT_setBufferSize() :
 * all future buffers provided by this buffer pool will have _at least_ this size
 * note : it's better for all buffers to have same size,
 * as they become freely interchangeable, reducing malloc/free usages and memory fragmentation */
static void ZSTDMT_setBufferSize(ZSTDMT_bufferPool* const bufPool, size_t const bSize)
{
    ZSTD_pthread_mutex_lock(&bufPool->poolMutex);
    DEBUGLOG(4, "ZSTDMT_setBufferSize: bSize = %u", (U32)bSize);
    bufPool->bufferSize = bSize;
    ZSTD_pthread_mutex_unlock(&bufPool->poolMutex);
}

/** ZSTDMT_getBuffer() :
 *  assumption : bufPool must be valid
 * @return : a buffer, with start pointer and size
 *  note: allocation may fail, in this case, start==NULL and size==0 */
static buffer_t ZSTDMT_getBuffer(ZSTDMT_bufferPool* bufPool)
{
    size_t const bSize = bufPool->bufferSize;
    DEBUGLOG(5, "ZSTDMT_getBuffer: bSize = %u", (U32)bufPool->bufferSize);
    ZSTD_pthread_mutex_lock(&bufPool->poolMutex);
    if (bufPool->nbBuffers) {   /* try to use an existing buffer */
        buffer_t const buf = bufPool->bTable[--(bufPool->nbBuffers)];
        size_t const availBufferSize = buf.capacity;
        bufPool->bTable[bufPool->nbBuffers] = g_nullBuffer;
        if ((availBufferSize >= bSize) & ((availBufferSize>>3) <= bSize)) {
            /* large enough, but not too much */
            DEBUGLOG(5, "ZSTDMT_getBuffer: provide buffer %u of size %u",
                        bufPool->nbBuffers, (U32)buf.capacity);
            ZSTD_pthread_mutex_unlock(&bufPool->poolMutex);
            return buf;
        }
        /* size conditions not respected : scratch this buffer, create new one */
        DEBUGLOG(5, "ZSTDMT_getBuffer: existing buffer does not meet size conditions => freeing");
        ZSTD_free(buf.start, bufPool->cMem);
    }
    ZSTD_pthread_mutex_unlock(&bufPool->poolMutex);
    /* create new buffer */
    DEBUGLOG(5, "ZSTDMT_getBuffer: create a new buffer");
    {   buffer_t buffer;
        void* const start = ZSTD_malloc(bSize, bufPool->cMem);
        buffer.start = start;   /* note : start can be NULL if malloc fails ! */
        buffer.capacity = (start==NULL) ? 0 : bSize;
        if (start==NULL) {
            DEBUGLOG(5, "ZSTDMT_getBuffer: buffer allocation failure !!");
        } else {
            DEBUGLOG(5, "ZSTDMT_getBuffer: created buffer of size %u", (U32)bSize);
        }
        return buffer;
    }
}

/* store buffer for later re-use, up to pool capacity */
static void ZSTDMT_releaseBuffer(ZSTDMT_bufferPool* bufPool, buffer_t buf)
{
    if (buf.start == NULL) return;   /* compatible with release on NULL */
    DEBUGLOG(5, "ZSTDMT_releaseBuffer");
    ZSTD_pthread_mutex_lock(&bufPool->poolMutex);
    if (bufPool->nbBuffers < bufPool->totalBuffers) {
        bufPool->bTable[bufPool->nbBuffers++] = buf;  /* stored for later use */
        DEBUGLOG(5, "ZSTDMT_releaseBuffer: stored buffer of size %u in slot %u",
                    (U32)buf.capacity, (U32)(bufPool->nbBuffers-1));
        ZSTD_pthread_mutex_unlock(&bufPool->poolMutex);
        return;
    }
    ZSTD_pthread_mutex_unlock(&bufPool->poolMutex);
    /* Reached bufferPool capacity (should not happen) */
    DEBUGLOG(5, "ZSTDMT_releaseBuffer: pool capacity reached => freeing ");
    ZSTD_free(buf.start, bufPool->cMem);
}


/* =====   CCtx Pool   ===== */
/* a single CCtx Pool can be invoked from multiple threads in parallel */

typedef struct {
    ZSTD_pthread_mutex_t poolMutex;
    unsigned totalCCtx;
    unsigned availCCtx;
    ZSTD_customMem cMem;
    ZSTD_CCtx* cctx[1];   /* variable size */
} ZSTDMT_CCtxPool;

/* note : all CCtx borrowed from the pool should be released back to the pool _before_ freeing the pool */
static void ZSTDMT_freeCCtxPool(ZSTDMT_CCtxPool* pool)
{
    unsigned u;
    for (u=0; u<pool->totalCCtx; u++)
        ZSTD_freeCCtx(pool->cctx[u]);  /* note : compatible with free on NULL */
    ZSTD_pthread_mutex_destroy(&pool->poolMutex);
    ZSTD_free(pool, pool->cMem);
}

/* ZSTDMT_createCCtxPool() :
 * implies nbWorkers >= 1 , checked by caller ZSTDMT_createCCtx() */
static ZSTDMT_CCtxPool* ZSTDMT_createCCtxPool(unsigned nbWorkers,
                                              ZSTD_customMem cMem)
{
    ZSTDMT_CCtxPool* const cctxPool = (ZSTDMT_CCtxPool*) ZSTD_calloc(
        sizeof(ZSTDMT_CCtxPool) + (nbWorkers-1)*sizeof(ZSTD_CCtx*), cMem);
    assert(nbWorkers > 0);
    if (!cctxPool) return NULL;
    if (ZSTD_pthread_mutex_init(&cctxPool->poolMutex, NULL)) {
        ZSTD_free(cctxPool, cMem);
        return NULL;
    }
    cctxPool->cMem = cMem;
    cctxPool->totalCCtx = nbWorkers;
    cctxPool->availCCtx = 1;   /* at least one cctx for single-thread mode */
    cctxPool->cctx[0] = ZSTD_createCCtx_advanced(cMem);
    if (!cctxPool->cctx[0]) { ZSTDMT_freeCCtxPool(cctxPool); return NULL; }
    DEBUGLOG(3, "cctxPool created, with %u workers", nbWorkers);
    return cctxPool;
}

/* only works during initialization phase, not during compression */
static size_t ZSTDMT_sizeof_CCtxPool(ZSTDMT_CCtxPool* cctxPool)
{
    ZSTD_pthread_mutex_lock(&cctxPool->poolMutex);
    {   unsigned const nbWorkers = cctxPool->totalCCtx;
        size_t const poolSize = sizeof(*cctxPool)
                                + (nbWorkers-1) * sizeof(ZSTD_CCtx*);
        unsigned u;
        size_t totalCCtxSize = 0;
        for (u=0; u<nbWorkers; u++) {
            totalCCtxSize += ZSTD_sizeof_CCtx(cctxPool->cctx[u]);
        }
        ZSTD_pthread_mutex_unlock(&cctxPool->poolMutex);
        assert(nbWorkers > 0);
        return poolSize + totalCCtxSize;
    }
}

static ZSTD_CCtx* ZSTDMT_getCCtx(ZSTDMT_CCtxPool* cctxPool)
{
    DEBUGLOG(5, "ZSTDMT_getCCtx");
    ZSTD_pthread_mutex_lock(&cctxPool->poolMutex);
    if (cctxPool->availCCtx) {
        cctxPool->availCCtx--;
        {   ZSTD_CCtx* const cctx = cctxPool->cctx[cctxPool->availCCtx];
            ZSTD_pthread_mutex_unlock(&cctxPool->poolMutex);
            return cctx;
    }   }
    ZSTD_pthread_mutex_unlock(&cctxPool->poolMutex);
    DEBUGLOG(5, "create one more CCtx");
    return ZSTD_createCCtx_advanced(cctxPool->cMem);   /* note : can be NULL, when creation fails ! */
}

static void ZSTDMT_releaseCCtx(ZSTDMT_CCtxPool* pool, ZSTD_CCtx* cctx)
{
    if (cctx==NULL) return;   /* compatibility with release on NULL */
    ZSTD_pthread_mutex_lock(&pool->poolMutex);
    if (pool->availCCtx < pool->totalCCtx)
        pool->cctx[pool->availCCtx++] = cctx;
    else {
        /* pool overflow : should not happen, since totalCCtx==nbWorkers */
        DEBUGLOG(4, "CCtx pool overflow : free cctx");
        ZSTD_freeCCtx(cctx);
    }
    ZSTD_pthread_mutex_unlock(&pool->poolMutex);
}


/* ------------------------------------------ */
/* =====          Worker thread         ===== */
/* ------------------------------------------ */

typedef struct {
    size_t   consumed;                   /* SHARED - set0 by mtctx, then modified by worker AND read by mtctx */
    size_t   cSize;                      /* SHARED - set0 by mtctx, then modified by worker AND read by mtctx, then set0 by mtctx */
    ZSTD_pthread_mutex_t job_mutex;      /* Thread-safe - used by mtctx and worker */
    ZSTD_pthread_cond_t job_cond;        /* Thread-safe - used by mtctx and worker */
    ZSTDMT_CCtxPool* cctxPool;           /* Thread-safe - used by mtctx and (all) workers */
    ZSTDMT_bufferPool* bufPool;          /* Thread-safe - used by mtctx and (all) workers */
    buffer_t dstBuff;                    /* set by worker (or mtctx), then read by worker & mtctx, then modified by mtctx => no barrier */
    buffer_t srcBuff;                    /* set by mtctx, then released by worker => no barrier */
    const void* prefixStart;             /* set by mtctx, then read and set0 by worker => no barrier */
    size_t   prefixSize;                 /* set by mtctx, then read by worker => no barrier */
    size_t   srcSize;                    /* set by mtctx, then read by worker & mtctx => no barrier */
    unsigned firstJob;                   /* set by mtctx, then read by worker => no barrier */
    unsigned lastJob;                    /* set by mtctx, then read by worker => no barrier */
    ZSTD_CCtx_params params;             /* set by mtctx, then read by worker => no barrier */
    const ZSTD_CDict* cdict;             /* set by mtctx, then read by worker => no barrier */
    unsigned long long fullFrameSize;    /* set by mtctx, then read by worker => no barrier */
    size_t   dstFlushed;                 /* used only by mtctx */
    unsigned frameChecksumNeeded;        /* used only by mtctx */
} ZSTDMT_jobDescription;

/* ZSTDMT_compressionJob() is a POOL_function type */
void ZSTDMT_compressionJob(void* jobDescription)
{
    ZSTDMT_jobDescription* const job = (ZSTDMT_jobDescription*)jobDescription;
    ZSTD_CCtx* const cctx = ZSTDMT_getCCtx(job->cctxPool);
    const void* const src = (const char*)job->prefixStart + job->prefixSize;
    buffer_t dstBuff = job->dstBuff;

    /* ressources */
    if (cctx==NULL) {
        job->cSize = ERROR(memory_allocation);
        goto _endJob;
    }
    if (dstBuff.start == NULL) {   /* streaming job : doesn't provide a dstBuffer */
        dstBuff = ZSTDMT_getBuffer(job->bufPool);
        if (dstBuff.start==NULL) {
            job->cSize = ERROR(memory_allocation);
            goto _endJob;
        }
        job->dstBuff = dstBuff;   /* this value can be read in ZSTDMT_flush, when it copies the whole job */
    }

    /* init */
    if (job->cdict) {
        size_t const initError = ZSTD_compressBegin_advanced_internal(cctx, NULL, 0, ZSTD_dm_auto, job->cdict, job->params, job->fullFrameSize);
        assert(job->firstJob);  /* only allowed for first job */
        if (ZSTD_isError(initError)) { job->cSize = initError; goto _endJob; }
    } else {  /* srcStart points at reloaded section */
        U64 const pledgedSrcSize = job->firstJob ? job->fullFrameSize : job->srcSize;
        ZSTD_CCtx_params jobParams = job->params;   /* do not modify job->params ! copy it, modify the copy */
        {   size_t const forceWindowError = ZSTD_CCtxParam_setParameter(&jobParams, ZSTD_p_forceMaxWindow, !job->firstJob);
            if (ZSTD_isError(forceWindowError)) {
                job->cSize = forceWindowError;
                goto _endJob;
        }   }
        {   size_t const initError = ZSTD_compressBegin_advanced_internal(cctx,
                                        job->prefixStart, job->prefixSize, ZSTD_dm_rawContent, /* load dictionary in "content-only" mode (no header analysis) */
                                        NULL, /*cdict*/
                                        jobParams, pledgedSrcSize);
            if (ZSTD_isError(initError)) {
                job->cSize = initError;
                goto _endJob;
    }   }   }
    if (!job->firstJob) {  /* flush and overwrite frame header when it's not first job */
        size_t const hSize = ZSTD_compressContinue(cctx, dstBuff.start, dstBuff.capacity, src, 0);
        if (ZSTD_isError(hSize)) { job->cSize = hSize; /* save error code */ goto _endJob; }
        DEBUGLOG(5, "ZSTDMT_compressionJob: flush and overwrite %u bytes of frame header (not first job)", (U32)hSize);
        ZSTD_invalidateRepCodes(cctx);
    }

    /* compress */
    {   size_t const chunkSize = 4*ZSTD_BLOCKSIZE_MAX;
        int const nbChunks = (int)((job->srcSize + (chunkSize-1)) / chunkSize);
        const BYTE* ip = (const BYTE*) src;
        BYTE* const ostart = (BYTE*)dstBuff.start;
        BYTE* op = ostart;
        BYTE* oend = op + dstBuff.capacity;
        int chunkNb;
        if (sizeof(size_t) > sizeof(int)) assert(job->srcSize < ((size_t)INT_MAX) * chunkSize);   /* check overflow */
        DEBUGLOG(5, "ZSTDMT_compressionJob: compress %u bytes in %i blocks", (U32)job->srcSize, nbChunks);
        assert(job->cSize == 0);
        for (chunkNb = 1; chunkNb < nbChunks; chunkNb++) {
            size_t const cSize = ZSTD_compressContinue(cctx, op, oend-op, ip, chunkSize);
            if (ZSTD_isError(cSize)) { job->cSize = cSize; goto _endJob; }
            ip += chunkSize;
            op += cSize; assert(op < oend);
            /* stats */
            ZSTD_PTHREAD_MUTEX_LOCK(&job->job_mutex);
            job->cSize += cSize;
            job->consumed = chunkSize * chunkNb;
            DEBUGLOG(5, "ZSTDMT_compressionJob: compress new block : cSize==%u bytes (total: %u)",
                        (U32)cSize, (U32)job->cSize);
            ZSTD_pthread_cond_signal(&job->job_cond);   /* warns some more data is ready to be flushed */
            ZSTD_pthread_mutex_unlock(&job->job_mutex);
        }
        /* last block */
        assert(chunkSize > 0); assert((chunkSize & (chunkSize - 1)) == 0);  /* chunkSize must be power of 2 for mask==(chunkSize-1) to work */
        if ((nbChunks > 0) | job->lastJob /*must output a "last block" flag*/ ) {
            size_t const lastBlockSize1 = job->srcSize & (chunkSize-1);
            size_t const lastBlockSize = ((lastBlockSize1==0) & (job->srcSize>=chunkSize)) ? chunkSize : lastBlockSize1;
            size_t const cSize = (job->lastJob) ?
                 ZSTD_compressEnd     (cctx, op, oend-op, ip, lastBlockSize) :
                 ZSTD_compressContinue(cctx, op, oend-op, ip, lastBlockSize);
            if (ZSTD_isError(cSize)) { job->cSize = cSize; goto _endJob; }
            /* stats */
            ZSTD_PTHREAD_MUTEX_LOCK(&job->job_mutex);
            job->cSize += cSize;
            ZSTD_pthread_mutex_unlock(&job->job_mutex);
    }   }

_endJob:
    /* release resources */
    ZSTDMT_releaseCCtx(job->cctxPool, cctx);
    ZSTDMT_releaseBuffer(job->bufPool, job->srcBuff);
    job->srcBuff = g_nullBuffer; job->prefixStart = NULL;
    /* report */
    ZSTD_PTHREAD_MUTEX_LOCK(&job->job_mutex);
    job->consumed = job->srcSize;
    ZSTD_pthread_cond_signal(&job->job_cond);
    ZSTD_pthread_mutex_unlock(&job->job_mutex);
}


/* ------------------------------------------ */
/* =====   Multi-threaded compression   ===== */
/* ------------------------------------------ */

typedef struct {
    buffer_t buffer;
    size_t targetCapacity;  /* note : buffers provided by the pool may be larger than target capacity */
    size_t prefixSize;
    size_t filled;
} inBuff_t;

struct ZSTDMT_CCtx_s {
    POOL_ctx* factory;
    ZSTDMT_jobDescription* jobs;
    ZSTDMT_bufferPool* bufPool;
    ZSTDMT_CCtxPool* cctxPool;
    ZSTD_CCtx_params params;
    size_t targetSectionSize;
    size_t targetPrefixSize;
    inBuff_t inBuff;
    int jobReady;        /* 1 => one job is already prepared, but pool has shortage of workers. Don't create another one. */
    XXH64_state_t xxhState;
    unsigned singleBlockingThread;
    unsigned jobIDMask;
    unsigned doneJobID;
    unsigned nextJobID;
    unsigned frameEnded;
    unsigned allJobsCompleted;
    unsigned long long frameContentSize;
    unsigned long long consumed;
    unsigned long long produced;
    ZSTD_customMem cMem;
    ZSTD_CDict* cdictLocal;
    const ZSTD_CDict* cdict;
};

static void ZSTDMT_freeJobsTable(ZSTDMT_jobDescription* jobTable, U32 nbJobs, ZSTD_customMem cMem)
{
    U32 jobNb;
    if (jobTable == NULL) return;
    for (jobNb=0; jobNb<nbJobs; jobNb++) {
        ZSTD_pthread_mutex_destroy(&jobTable[jobNb].job_mutex);
        ZSTD_pthread_cond_destroy(&jobTable[jobNb].job_cond);
    }
    ZSTD_free(jobTable, cMem);
}

/* ZSTDMT_allocJobsTable()
 * allocate and init a job table.
 * update *nbJobsPtr to next power of 2 value, as size of table */
static ZSTDMT_jobDescription* ZSTDMT_createJobsTable(U32* nbJobsPtr, ZSTD_customMem cMem)
{
    U32 const nbJobsLog2 = ZSTD_highbit32(*nbJobsPtr) + 1;
    U32 const nbJobs = 1 << nbJobsLog2;
    U32 jobNb;
    ZSTDMT_jobDescription* const jobTable = (ZSTDMT_jobDescription*)
                ZSTD_calloc(nbJobs * sizeof(ZSTDMT_jobDescription), cMem);
    int initError = 0;
    if (jobTable==NULL) return NULL;
    *nbJobsPtr = nbJobs;
    for (jobNb=0; jobNb<nbJobs; jobNb++) {
        initError |= ZSTD_pthread_mutex_init(&jobTable[jobNb].job_mutex, NULL);
        initError |= ZSTD_pthread_cond_init(&jobTable[jobNb].job_cond, NULL);
    }
    if (initError != 0) {
        ZSTDMT_freeJobsTable(jobTable, nbJobs, cMem);
        return NULL;
    }
    return jobTable;
}

/* ZSTDMT_CCtxParam_setNbWorkers():
 * Internal use only */
size_t ZSTDMT_CCtxParam_setNbWorkers(ZSTD_CCtx_params* params, unsigned nbWorkers)
{
    if (nbWorkers > ZSTDMT_NBWORKERS_MAX) nbWorkers = ZSTDMT_NBWORKERS_MAX;
    params->nbWorkers = nbWorkers;
    params->overlapSizeLog = ZSTDMT_OVERLAPLOG_DEFAULT;
    params->jobSize = 0;
    return nbWorkers;
}

ZSTDMT_CCtx* ZSTDMT_createCCtx_advanced(unsigned nbWorkers, ZSTD_customMem cMem)
{
    ZSTDMT_CCtx* mtctx;
    U32 nbJobs = nbWorkers + 2;
    DEBUGLOG(3, "ZSTDMT_createCCtx_advanced (nbWorkers = %u)", nbWorkers);

    if (nbWorkers < 1) return NULL;
    nbWorkers = MIN(nbWorkers , ZSTDMT_NBWORKERS_MAX);
    if ((cMem.customAlloc!=NULL) ^ (cMem.customFree!=NULL))
        /* invalid custom allocator */
        return NULL;

    mtctx = (ZSTDMT_CCtx*) ZSTD_calloc(sizeof(ZSTDMT_CCtx), cMem);
    if (!mtctx) return NULL;
    ZSTDMT_CCtxParam_setNbWorkers(&mtctx->params, nbWorkers);
    mtctx->cMem = cMem;
    mtctx->allJobsCompleted = 1;
    mtctx->factory = POOL_create_advanced(nbWorkers, 0, cMem);
    mtctx->jobs = ZSTDMT_createJobsTable(&nbJobs, cMem);
    assert(nbJobs > 0); assert((nbJobs & (nbJobs - 1)) == 0);  /* ensure nbJobs is a power of 2 */
    mtctx->jobIDMask = nbJobs - 1;
    mtctx->bufPool = ZSTDMT_createBufferPool(nbWorkers, cMem);
    mtctx->cctxPool = ZSTDMT_createCCtxPool(nbWorkers, cMem);
    if (!mtctx->factory | !mtctx->jobs | !mtctx->bufPool | !mtctx->cctxPool) {
        ZSTDMT_freeCCtx(mtctx);
        return NULL;
    }
    DEBUGLOG(3, "mt_cctx created, for %u threads", nbWorkers);
    return mtctx;
}

ZSTDMT_CCtx* ZSTDMT_createCCtx(unsigned nbWorkers)
{
    return ZSTDMT_createCCtx_advanced(nbWorkers, ZSTD_defaultCMem);
}


/* ZSTDMT_releaseAllJobResources() :
 * note : ensure all workers are killed first ! */
static void ZSTDMT_releaseAllJobResources(ZSTDMT_CCtx* mtctx)
{
    unsigned jobID;
    DEBUGLOG(3, "ZSTDMT_releaseAllJobResources");
    for (jobID=0; jobID <= mtctx->jobIDMask; jobID++) {
        DEBUGLOG(4, "job%02u: release dst address %08X", jobID, (U32)(size_t)mtctx->jobs[jobID].dstBuff.start);
        ZSTDMT_releaseBuffer(mtctx->bufPool, mtctx->jobs[jobID].dstBuff);
        mtctx->jobs[jobID].dstBuff = g_nullBuffer;
        mtctx->jobs[jobID].cSize = 0;
        DEBUGLOG(4, "job%02u: release src address %08X", jobID, (U32)(size_t)mtctx->jobs[jobID].srcBuff.start);
        ZSTDMT_releaseBuffer(mtctx->bufPool, mtctx->jobs[jobID].srcBuff);
        mtctx->jobs[jobID].srcBuff = g_nullBuffer;
    }
    memset(mtctx->jobs, 0, (mtctx->jobIDMask+1)*sizeof(ZSTDMT_jobDescription));
    DEBUGLOG(4, "input: release address %08X", (U32)(size_t)mtctx->inBuff.buffer.start);
    ZSTDMT_releaseBuffer(mtctx->bufPool, mtctx->inBuff.buffer);
    mtctx->inBuff.buffer = g_nullBuffer;
    mtctx->allJobsCompleted = 1;
}

static void ZSTDMT_waitForAllJobsCompleted(ZSTDMT_CCtx* mtctx)
{
    DEBUGLOG(4, "ZSTDMT_waitForAllJobsCompleted");
    while (mtctx->doneJobID < mtctx->nextJobID) {
        unsigned const jobID = mtctx->doneJobID & mtctx->jobIDMask;
        ZSTD_PTHREAD_MUTEX_LOCK(&mtctx->jobs[jobID].job_mutex);
        while (mtctx->jobs[jobID].consumed < mtctx->jobs[jobID].srcSize) {
            DEBUGLOG(5, "waiting for jobCompleted signal from job %u", mtctx->doneJobID);   /* we want to block when waiting for data to flush */
            ZSTD_pthread_cond_wait(&mtctx->jobs[jobID].job_cond, &mtctx->jobs[jobID].job_mutex);
        }
        ZSTD_pthread_mutex_unlock(&mtctx->jobs[jobID].job_mutex);
        mtctx->doneJobID++;
    }
}

size_t ZSTDMT_freeCCtx(ZSTDMT_CCtx* mtctx)
{
    if (mtctx==NULL) return 0;   /* compatible with free on NULL */
    POOL_free(mtctx->factory);   /* stop and free worker threads */
    ZSTDMT_releaseAllJobResources(mtctx);  /* release job resources into pools first */
    ZSTDMT_freeJobsTable(mtctx->jobs, mtctx->jobIDMask+1, mtctx->cMem);
    ZSTDMT_freeBufferPool(mtctx->bufPool);
    ZSTDMT_freeCCtxPool(mtctx->cctxPool);
    ZSTD_freeCDict(mtctx->cdictLocal);
    ZSTD_free(mtctx, mtctx->cMem);
    return 0;
}

size_t ZSTDMT_sizeof_CCtx(ZSTDMT_CCtx* mtctx)
{
    if (mtctx == NULL) return 0;   /* supports sizeof NULL */
    return sizeof(*mtctx)
            + POOL_sizeof(mtctx->factory)
            + ZSTDMT_sizeof_bufferPool(mtctx->bufPool)
            + (mtctx->jobIDMask+1) * sizeof(ZSTDMT_jobDescription)
            + ZSTDMT_sizeof_CCtxPool(mtctx->cctxPool)
            + ZSTD_sizeof_CDict(mtctx->cdictLocal);
}

/* Internal only */
size_t ZSTDMT_CCtxParam_setMTCtxParameter(ZSTD_CCtx_params* params,
                                ZSTDMT_parameter parameter, unsigned value) {
    DEBUGLOG(4, "ZSTDMT_CCtxParam_setMTCtxParameter");
    switch(parameter)
    {
    case ZSTDMT_p_jobSize :
        DEBUGLOG(4, "ZSTDMT_CCtxParam_setMTCtxParameter : set jobSize to %u", value);
        if ( (value > 0)  /* value==0 => automatic job size */
           & (value < ZSTDMT_JOBSIZE_MIN) )
            value = ZSTDMT_JOBSIZE_MIN;
        params->jobSize = value;
        return value;
    case ZSTDMT_p_overlapSectionLog :
        if (value > 9) value = 9;
        DEBUGLOG(4, "ZSTDMT_p_overlapSectionLog : %u", value);
        params->overlapSizeLog = (value >= 9) ? 9 : value;
        return value;
    default :
        return ERROR(parameter_unsupported);
    }
}

size_t ZSTDMT_setMTCtxParameter(ZSTDMT_CCtx* mtctx, ZSTDMT_parameter parameter, unsigned value)
{
    DEBUGLOG(4, "ZSTDMT_setMTCtxParameter");
    switch(parameter)
    {
    case ZSTDMT_p_jobSize :
        return ZSTDMT_CCtxParam_setMTCtxParameter(&mtctx->params, parameter, value);
    case ZSTDMT_p_overlapSectionLog :
        return ZSTDMT_CCtxParam_setMTCtxParameter(&mtctx->params, parameter, value);
    default :
        return ERROR(parameter_unsupported);
    }
}

/* Sets parameters relevant to the compression job,
 * initializing others to default values. */
static ZSTD_CCtx_params ZSTDMT_initJobCCtxParams(ZSTD_CCtx_params const params)
{
    ZSTD_CCtx_params jobParams;
    memset(&jobParams, 0, sizeof(jobParams));

    jobParams.cParams = params.cParams;
    jobParams.fParams = params.fParams;
    jobParams.compressionLevel = params.compressionLevel;

    jobParams.ldmParams = params.ldmParams;
    return jobParams;
}

/*! ZSTDMT_updateCParams_whileCompressing() :
 *  Update compression level and parameters (except wlog)
 *  while compression is ongoing.
 *  New parameters will be applied to next compression job. */
void ZSTDMT_updateCParams_whileCompressing(ZSTDMT_CCtx* mtctx, int compressionLevel, ZSTD_compressionParameters cParams)
{
    U32 const saved_wlog = mtctx->params.cParams.windowLog;   /* Do not modify windowLog while compressing */
    DEBUGLOG(5, "ZSTDMT_updateCParams_whileCompressing (level:%i)",
                compressionLevel);
    mtctx->params.compressionLevel = compressionLevel;
    if (compressionLevel != ZSTD_CLEVEL_CUSTOM)
        cParams = ZSTD_getCParams(compressionLevel, mtctx->frameContentSize, 0 /* dictSize */ );
    cParams.windowLog = saved_wlog;
    mtctx->params.cParams = cParams;
}

/* ZSTDMT_getNbWorkers():
 * @return nb threads currently active in mtctx.
 * mtctx must be valid */
unsigned ZSTDMT_getNbWorkers(const ZSTDMT_CCtx* mtctx)
{
    assert(mtctx != NULL);
    return mtctx->params.nbWorkers;
}

/* ZSTDMT_getFrameProgression():
 * tells how much data has been consumed (input) and produced (output) for current frame.
 * able to count progression inside worker threads.
 * Note : mutex will be acquired during statistics collection. */
ZSTD_frameProgression ZSTDMT_getFrameProgression(ZSTDMT_CCtx* mtctx)
{
    ZSTD_frameProgression fps;
    DEBUGLOG(6, "ZSTDMT_getFrameProgression");
    fps.consumed = mtctx->consumed;
    fps.produced = mtctx->produced;
    assert(mtctx->inBuff.filled >= mtctx->inBuff.prefixSize);
    fps.ingested = mtctx->consumed + (mtctx->inBuff.filled - mtctx->inBuff.prefixSize);
    {   unsigned jobNb;
        unsigned lastJobNb = mtctx->nextJobID + mtctx->jobReady; assert(mtctx->jobReady <= 1);
        DEBUGLOG(6, "ZSTDMT_getFrameProgression: jobs: from %u to <%u (jobReady:%u)",
                    mtctx->doneJobID, lastJobNb, mtctx->jobReady)
        for (jobNb = mtctx->doneJobID ; jobNb < lastJobNb ; jobNb++) {
            unsigned const wJobID = jobNb & mtctx->jobIDMask;
            ZSTD_pthread_mutex_lock(&mtctx->jobs[wJobID].job_mutex);
            {   size_t const cResult = mtctx->jobs[wJobID].cSize;
                size_t const produced = ZSTD_isError(cResult) ? 0 : cResult;
                fps.consumed += mtctx->jobs[wJobID].consumed;
                fps.ingested += mtctx->jobs[wJobID].srcSize;
                fps.produced += produced;
            }
            ZSTD_pthread_mutex_unlock(&mtctx->jobs[wJobID].job_mutex);
        }
    }
    return fps;
}


/* ------------------------------------------ */
/* =====   Multi-threaded compression   ===== */
/* ------------------------------------------ */

static unsigned ZSTDMT_computeNbJobs(size_t srcSize, unsigned windowLog, unsigned nbWorkers) {
    assert(nbWorkers>0);
    {   size_t const jobSizeTarget = (size_t)1 << (windowLog + 2);
        size_t const jobMaxSize = jobSizeTarget << 2;
        size_t const passSizeMax = jobMaxSize * nbWorkers;
        unsigned const multiplier = (unsigned)(srcSize / passSizeMax) + 1;
        unsigned const nbJobsLarge = multiplier * nbWorkers;
        unsigned const nbJobsMax = (unsigned)(srcSize / jobSizeTarget) + 1;
        unsigned const nbJobsSmall = MIN(nbJobsMax, nbWorkers);
        return (multiplier>1) ? nbJobsLarge : nbJobsSmall;
}   }

/* ZSTDMT_compress_advanced_internal() :
 * This is a blocking function : it will only give back control to caller after finishing its compression job.
 */
static size_t ZSTDMT_compress_advanced_internal(
                ZSTDMT_CCtx* mtctx,
                void* dst, size_t dstCapacity,
          const void* src, size_t srcSize,
          const ZSTD_CDict* cdict,
                ZSTD_CCtx_params const params)
{
    ZSTD_CCtx_params const jobParams = ZSTDMT_initJobCCtxParams(params);
    unsigned const overlapRLog = (params.overlapSizeLog>9) ? 0 : 9-params.overlapSizeLog;
    size_t const overlapSize = (overlapRLog>=9) ? 0 : (size_t)1 << (params.cParams.windowLog - overlapRLog);
    unsigned const nbJobs = ZSTDMT_computeNbJobs(srcSize, params.cParams.windowLog, params.nbWorkers);
    size_t const proposedJobSize = (srcSize + (nbJobs-1)) / nbJobs;
    size_t const avgJobSize = (((proposedJobSize-1) & 0x1FFFF) < 0x7FFF) ? proposedJobSize + 0xFFFF : proposedJobSize;   /* avoid too small last block */
    const char* const srcStart = (const char*)src;
    size_t remainingSrcSize = srcSize;
    unsigned const compressWithinDst = (dstCapacity >= ZSTD_compressBound(srcSize)) ? nbJobs : (unsigned)(dstCapacity / ZSTD_compressBound(avgJobSize));  /* presumes avgJobSize >= 256 KB, which should be the case */
    size_t frameStartPos = 0, dstBufferPos = 0;
    XXH64_state_t xxh64;
    assert(jobParams.nbWorkers == 0);
    assert(mtctx->cctxPool->totalCCtx == params.nbWorkers);

    DEBUGLOG(4, "ZSTDMT_compress_advanced_internal: nbJobs=%2u (rawSize=%u bytes; fixedSize=%u) ",
                nbJobs, (U32)proposedJobSize, (U32)avgJobSize);

    if ((nbJobs==1) | (params.nbWorkers<=1)) {   /* fallback to single-thread mode : this is a blocking invocation anyway */
        ZSTD_CCtx* const cctx = mtctx->cctxPool->cctx[0];
        if (cdict) return ZSTD_compress_usingCDict_advanced(cctx, dst, dstCapacity, src, srcSize, cdict, jobParams.fParams);
        return ZSTD_compress_advanced_internal(cctx, dst, dstCapacity, src, srcSize, NULL, 0, jobParams);
    }

    assert(avgJobSize >= 256 KB);  /* condition for ZSTD_compressBound(A) + ZSTD_compressBound(B) <= ZSTD_compressBound(A+B), required to compress directly into Dst (no additional buffer) */
    ZSTDMT_setBufferSize(mtctx->bufPool, ZSTD_compressBound(avgJobSize) );
    XXH64_reset(&xxh64, 0);

    if (nbJobs > mtctx->jobIDMask+1) {  /* enlarge job table */
        U32 jobsTableSize = nbJobs;
        ZSTDMT_freeJobsTable(mtctx->jobs, mtctx->jobIDMask+1, mtctx->cMem);
        mtctx->jobIDMask = 0;
        mtctx->jobs = ZSTDMT_createJobsTable(&jobsTableSize, mtctx->cMem);
        if (mtctx->jobs==NULL) return ERROR(memory_allocation);
        assert((jobsTableSize != 0) && ((jobsTableSize & (jobsTableSize - 1)) == 0));  /* ensure jobsTableSize is a power of 2 */
        mtctx->jobIDMask = jobsTableSize - 1;
    }

    {   unsigned u;
        for (u=0; u<nbJobs; u++) {
            size_t const jobSize = MIN(remainingSrcSize, avgJobSize);
            size_t const dstBufferCapacity = ZSTD_compressBound(jobSize);
            buffer_t const dstAsBuffer = { (char*)dst + dstBufferPos, dstBufferCapacity };
            buffer_t const dstBuffer = u < compressWithinDst ? dstAsBuffer : g_nullBuffer;
            size_t dictSize = u ? overlapSize : 0;

            mtctx->jobs[u].srcBuff = g_nullBuffer;
            mtctx->jobs[u].prefixStart = srcStart + frameStartPos - dictSize;
            mtctx->jobs[u].prefixSize = dictSize;
            mtctx->jobs[u].srcSize = jobSize; assert(jobSize > 0);  /* avoid job.srcSize == 0 */
            mtctx->jobs[u].consumed = 0;
            mtctx->jobs[u].cSize = 0;
            mtctx->jobs[u].cdict = (u==0) ? cdict : NULL;
            mtctx->jobs[u].fullFrameSize = srcSize;
            mtctx->jobs[u].params = jobParams;
            /* do not calculate checksum within sections, but write it in header for first section */
            if (u!=0) mtctx->jobs[u].params.fParams.checksumFlag = 0;
            mtctx->jobs[u].dstBuff = dstBuffer;
            mtctx->jobs[u].cctxPool = mtctx->cctxPool;
            mtctx->jobs[u].bufPool = mtctx->bufPool;
            mtctx->jobs[u].firstJob = (u==0);
            mtctx->jobs[u].lastJob = (u==nbJobs-1);

            if (params.fParams.checksumFlag) {
                XXH64_update(&xxh64, srcStart + frameStartPos, jobSize);
            }

            DEBUGLOG(5, "ZSTDMT_compress_advanced_internal: posting job %u  (%u bytes)", u, (U32)jobSize);
            DEBUG_PRINTHEX(6, mtctx->jobs[u].prefixStart, 12);
            POOL_add(mtctx->factory, ZSTDMT_compressionJob, &mtctx->jobs[u]);

            frameStartPos += jobSize;
            dstBufferPos += dstBufferCapacity;
            remainingSrcSize -= jobSize;
    }   }

    /* collect result */
    {   size_t error = 0, dstPos = 0;
        unsigned jobID;
        for (jobID=0; jobID<nbJobs; jobID++) {
            DEBUGLOG(5, "waiting for job %u ", jobID);
            ZSTD_PTHREAD_MUTEX_LOCK(&mtctx->jobs[jobID].job_mutex);
            while (mtctx->jobs[jobID].consumed < mtctx->jobs[jobID].srcSize) {
                DEBUGLOG(5, "waiting for jobCompleted signal from job %u", jobID);
                ZSTD_pthread_cond_wait(&mtctx->jobs[jobID].job_cond, &mtctx->jobs[jobID].job_mutex);
            }
            ZSTD_pthread_mutex_unlock(&mtctx->jobs[jobID].job_mutex);
            DEBUGLOG(5, "ready to write job %u ", jobID);

            mtctx->jobs[jobID].prefixStart = NULL;
            {   size_t const cSize = mtctx->jobs[jobID].cSize;
                if (ZSTD_isError(cSize)) error = cSize;
                if ((!error) && (dstPos + cSize > dstCapacity)) error = ERROR(dstSize_tooSmall);
                if (jobID) {   /* note : job 0 is written directly at dst, which is correct position */
                    if (!error)
                        memmove((char*)dst + dstPos, mtctx->jobs[jobID].dstBuff.start, cSize);  /* may overlap when job compressed within dst */
                    if (jobID >= compressWithinDst) {  /* job compressed into its own buffer, which must be released */
                        DEBUGLOG(5, "releasing buffer %u>=%u", jobID, compressWithinDst);
                        ZSTDMT_releaseBuffer(mtctx->bufPool, mtctx->jobs[jobID].dstBuff);
                }   }
                mtctx->jobs[jobID].dstBuff = g_nullBuffer;
                mtctx->jobs[jobID].cSize = 0;
                dstPos += cSize ;
            }
        }  /* for (jobID=0; jobID<nbJobs; jobID++) */

        DEBUGLOG(4, "checksumFlag : %u ", params.fParams.checksumFlag);
        if (params.fParams.checksumFlag) {
            U32 const checksum = (U32)XXH64_digest(&xxh64);
            if (dstPos + 4 > dstCapacity) {
                error = ERROR(dstSize_tooSmall);
            } else {
                DEBUGLOG(4, "writing checksum : %08X \n", checksum);
                MEM_writeLE32((char*)dst + dstPos, checksum);
                dstPos += 4;
        }   }

        if (!error) DEBUGLOG(4, "compressed size : %u  ", (U32)dstPos);
        return error ? error : dstPos;
    }
}

size_t ZSTDMT_compress_advanced(ZSTDMT_CCtx* mtctx,
                               void* dst, size_t dstCapacity,
                         const void* src, size_t srcSize,
                         const ZSTD_CDict* cdict,
                               ZSTD_parameters params,
                               unsigned overlapLog)
{
    ZSTD_CCtx_params cctxParams = mtctx->params;
    cctxParams.cParams = params.cParams;
    cctxParams.fParams = params.fParams;
    cctxParams.overlapSizeLog = overlapLog;
    return ZSTDMT_compress_advanced_internal(mtctx,
                                             dst, dstCapacity,
                                             src, srcSize,
                                             cdict, cctxParams);
}


size_t ZSTDMT_compressCCtx(ZSTDMT_CCtx* mtctx,
                           void* dst, size_t dstCapacity,
                     const void* src, size_t srcSize,
                           int compressionLevel)
{
    U32 const overlapLog = (compressionLevel >= ZSTD_maxCLevel()) ? 9 : ZSTDMT_OVERLAPLOG_DEFAULT;
    ZSTD_parameters params = ZSTD_getParams(compressionLevel, srcSize, 0);
    params.fParams.contentSizeFlag = 1;
    return ZSTDMT_compress_advanced(mtctx, dst, dstCapacity, src, srcSize, NULL, params, overlapLog);
}


/* ====================================== */
/* =======      Streaming API     ======= */
/* ====================================== */

size_t ZSTDMT_initCStream_internal(
        ZSTDMT_CCtx* mtctx,
        const void* dict, size_t dictSize, ZSTD_dictMode_e dictMode,
        const ZSTD_CDict* cdict, ZSTD_CCtx_params params,
        unsigned long long pledgedSrcSize)
{
    DEBUGLOG(4, "ZSTDMT_initCStream_internal (pledgedSrcSize=%u, nbWorkers=%u, cctxPool=%u)",
                (U32)pledgedSrcSize, params.nbWorkers, mtctx->cctxPool->totalCCtx);
    /* params are supposed to be fully validated at this point */
    assert(!ZSTD_isError(ZSTD_checkCParams(params.cParams)));
    assert(!((dict) && (cdict)));  /* either dict or cdict, not both */
    assert(mtctx->cctxPool->totalCCtx == params.nbWorkers);

    /* init */
    if (params.jobSize == 0) {
        if (params.cParams.windowLog >= 29)
            params.jobSize = ZSTDMT_JOBSIZE_MAX;
        else
            params.jobSize = 1 << (params.cParams.windowLog + 2);
    }
    if (params.jobSize > ZSTDMT_JOBSIZE_MAX) params.jobSize = ZSTDMT_JOBSIZE_MAX;

    mtctx->singleBlockingThread = (pledgedSrcSize <= ZSTDMT_JOBSIZE_MIN);  /* do not trigger multi-threading when srcSize is too small */
    if (mtctx->singleBlockingThread) {
        ZSTD_CCtx_params const singleThreadParams = ZSTDMT_initJobCCtxParams(params);
        DEBUGLOG(4, "ZSTDMT_initCStream_internal: switch to single blocking thread mode");
        assert(singleThreadParams.nbWorkers == 0);
        return ZSTD_initCStream_internal(mtctx->cctxPool->cctx[0],
                                         dict, dictSize, cdict,
                                         singleThreadParams, pledgedSrcSize);
    }

    DEBUGLOG(4, "ZSTDMT_initCStream_internal: %u workers", params.nbWorkers);

    if (mtctx->allJobsCompleted == 0) {   /* previous compression not correctly finished */
        ZSTDMT_waitForAllJobsCompleted(mtctx);
        ZSTDMT_releaseAllJobResources(mtctx);
        mtctx->allJobsCompleted = 1;
    }

    mtctx->params = params;
    mtctx->frameContentSize = pledgedSrcSize;
    if (dict) {
        ZSTD_freeCDict(mtctx->cdictLocal);
        mtctx->cdictLocal = ZSTD_createCDict_advanced(dict, dictSize,
                                                    ZSTD_dlm_byCopy, dictMode, /* note : a loadPrefix becomes an internal CDict */
                                                    params.cParams, mtctx->cMem);
        mtctx->cdict = mtctx->cdictLocal;
        if (mtctx->cdictLocal == NULL) return ERROR(memory_allocation);
    } else {
        ZSTD_freeCDict(mtctx->cdictLocal);
        mtctx->cdictLocal = NULL;
        mtctx->cdict = cdict;
    }

    assert(params.overlapSizeLog <= 9);
    mtctx->targetPrefixSize = (params.overlapSizeLog==0) ? 0 : (size_t)1 << (params.cParams.windowLog - (9 - params.overlapSizeLog));
    DEBUGLOG(4, "overlapLog=%u => %u KB", params.overlapSizeLog, (U32)(mtctx->targetPrefixSize>>10));
    mtctx->targetSectionSize = params.jobSize;
    if (mtctx->targetSectionSize < ZSTDMT_JOBSIZE_MIN) mtctx->targetSectionSize = ZSTDMT_JOBSIZE_MIN;
    if (mtctx->targetSectionSize < mtctx->targetPrefixSize) mtctx->targetSectionSize = mtctx->targetPrefixSize;  /* job size must be >= overlap size */
    DEBUGLOG(4, "Job Size : %u KB (note : set to %u)", (U32)(mtctx->targetSectionSize>>10), params.jobSize);
    mtctx->inBuff.targetCapacity = mtctx->targetPrefixSize + mtctx->targetSectionSize;
    DEBUGLOG(4, "inBuff Size : %u KB", (U32)(mtctx->inBuff.targetCapacity>>10));
    ZSTDMT_setBufferSize(mtctx->bufPool, MAX(mtctx->inBuff.targetCapacity, ZSTD_compressBound(mtctx->targetSectionSize)) );
    mtctx->inBuff.buffer = g_nullBuffer;
    mtctx->inBuff.prefixSize = 0;
    mtctx->doneJobID = 0;
    mtctx->nextJobID = 0;
    mtctx->frameEnded = 0;
    mtctx->allJobsCompleted = 0;
    mtctx->consumed = 0;
    mtctx->produced = 0;
    if (params.fParams.checksumFlag) XXH64_reset(&mtctx->xxhState, 0);
    return 0;
}

size_t ZSTDMT_initCStream_advanced(ZSTDMT_CCtx* mtctx,
                             const void* dict, size_t dictSize,
                                   ZSTD_parameters params,
                                   unsigned long long pledgedSrcSize)
{
    ZSTD_CCtx_params cctxParams = mtctx->params;  /* retrieve sticky params */
    DEBUGLOG(4, "ZSTDMT_initCStream_advanced (pledgedSrcSize=%u)", (U32)pledgedSrcSize);
    cctxParams.cParams = params.cParams;
    cctxParams.fParams = params.fParams;
    return ZSTDMT_initCStream_internal(mtctx, dict, dictSize, ZSTD_dm_auto, NULL,
                                       cctxParams, pledgedSrcSize);
}

size_t ZSTDMT_initCStream_usingCDict(ZSTDMT_CCtx* mtctx,
                               const ZSTD_CDict* cdict,
                                     ZSTD_frameParameters fParams,
                                     unsigned long long pledgedSrcSize)
{
    ZSTD_CCtx_params cctxParams = mtctx->params;
    if (cdict==NULL) return ERROR(dictionary_wrong);   /* method incompatible with NULL cdict */
    cctxParams.cParams = ZSTD_getCParamsFromCDict(cdict);
    cctxParams.fParams = fParams;
    return ZSTDMT_initCStream_internal(mtctx, NULL, 0 /*dictSize*/, ZSTD_dm_auto, cdict,
                                       cctxParams, pledgedSrcSize);
}


/* ZSTDMT_resetCStream() :
 * pledgedSrcSize can be zero == unknown (for the time being)
 * prefer using ZSTD_CONTENTSIZE_UNKNOWN,
 * as `0` might mean "empty" in the future */
size_t ZSTDMT_resetCStream(ZSTDMT_CCtx* mtctx, unsigned long long pledgedSrcSize)
{
    if (!pledgedSrcSize) pledgedSrcSize = ZSTD_CONTENTSIZE_UNKNOWN;
    return ZSTDMT_initCStream_internal(mtctx, NULL, 0, ZSTD_dm_auto, 0, mtctx->params,
                                       pledgedSrcSize);
}

size_t ZSTDMT_initCStream(ZSTDMT_CCtx* mtctx, int compressionLevel) {
    ZSTD_parameters const params = ZSTD_getParams(compressionLevel, ZSTD_CONTENTSIZE_UNKNOWN, 0);
    ZSTD_CCtx_params cctxParams = mtctx->params;   /* retrieve sticky params */
    DEBUGLOG(4, "ZSTDMT_initCStream (cLevel=%i)", compressionLevel);
    cctxParams.cParams = params.cParams;
    cctxParams.fParams = params.fParams;
    return ZSTDMT_initCStream_internal(mtctx, NULL, 0, ZSTD_dm_auto, NULL, cctxParams, ZSTD_CONTENTSIZE_UNKNOWN);
}


/* ZSTDMT_writeLastEmptyBlock()
 * Write a single empty block with an end-of-frame to finish a frame.
 * Job must be created from streaming variant.
 * This function is always successfull if expected conditions are fulfilled.
 */
static void ZSTDMT_writeLastEmptyBlock(ZSTDMT_jobDescription* job)
{
    assert(job->lastJob == 1);
    assert(job->srcSize == 0);    /* last job is empty -> will be simplified into a last empty block */
    assert(job->firstJob == 0);   /* cannot be first job, as it also needs to create frame header */
    /* A job created by streaming variant starts with a src buffer, but no dst buffer.
     * It summons a dstBuffer itself, compresses into it, then releases srcBuffer, and gives result to mtctx.
     * When done, srcBuffer is empty, while dstBuffer is filled, and will be released by mtctx.
     * This shortcut will simply switch srcBuffer for dstBuffer, providing same outcome as a normal job */
    assert(job->dstBuff.start == NULL);   /* invoked from streaming variant only (otherwise, dstBuff might be user's output) */
    assert(job->srcBuff.start != NULL);   /* invoked from streaming variant only (otherwise, srcBuff might be user's input) */
    assert(job->srcBuff.capacity >= ZSTD_blockHeaderSize);   /* no buffer should ever be that small */
    job->dstBuff = job->srcBuff;
    job->srcBuff = g_nullBuffer;
    job->cSize = ZSTD_writeLastEmptyBlock(job->dstBuff.start, job->dstBuff.capacity);
    assert(!ZSTD_isError(job->cSize));
    assert(job->consumed == 0);
}

static size_t ZSTDMT_createCompressionJob(ZSTDMT_CCtx* mtctx, size_t srcSize, ZSTD_EndDirective endOp)
{
    unsigned const jobID = mtctx->nextJobID & mtctx->jobIDMask;
    int const endFrame = (endOp == ZSTD_e_end);

    if (mtctx->nextJobID > mtctx->doneJobID + mtctx->jobIDMask) {
        DEBUGLOG(5, "ZSTDMT_createCompressionJob: will not create new job : table is full");
        assert((mtctx->nextJobID & mtctx->jobIDMask) == (mtctx->doneJobID & mtctx->jobIDMask));
        return 0;
    }

    if (!mtctx->jobReady) {
        DEBUGLOG(5, "ZSTDMT_createCompressionJob: preparing job %u to compress %u bytes with %u preload ",
                    mtctx->nextJobID, (U32)srcSize, (U32)mtctx->inBuff.prefixSize);
        assert(mtctx->jobs[jobID].srcBuff.start == NULL);   /* no buffer left : supposed already released */
        mtctx->jobs[jobID].srcBuff = mtctx->inBuff.buffer;
        mtctx->jobs[jobID].prefixStart = mtctx->inBuff.buffer.start;
        mtctx->jobs[jobID].prefixSize = mtctx->inBuff.prefixSize;
        mtctx->jobs[jobID].srcSize = srcSize;
        assert(mtctx->inBuff.filled >= srcSize + mtctx->inBuff.prefixSize);
        mtctx->jobs[jobID].consumed = 0;
        mtctx->jobs[jobID].cSize = 0;
        mtctx->jobs[jobID].params = mtctx->params;
        /* do not calculate checksum within sections, but write it in header for first section */
        if (mtctx->nextJobID) mtctx->jobs[jobID].params.fParams.checksumFlag = 0;
        mtctx->jobs[jobID].cdict = mtctx->nextJobID==0 ? mtctx->cdict : NULL;
        mtctx->jobs[jobID].fullFrameSize = mtctx->frameContentSize;
        mtctx->jobs[jobID].dstBuff = g_nullBuffer;
        mtctx->jobs[jobID].cctxPool = mtctx->cctxPool;
        mtctx->jobs[jobID].bufPool = mtctx->bufPool;
        mtctx->jobs[jobID].firstJob = (mtctx->nextJobID==0);
        mtctx->jobs[jobID].lastJob = endFrame;
        mtctx->jobs[jobID].frameChecksumNeeded = endFrame && (mtctx->nextJobID>0) && mtctx->params.fParams.checksumFlag;
        mtctx->jobs[jobID].dstFlushed = 0;

        if (mtctx->params.fParams.checksumFlag)
            XXH64_update(&mtctx->xxhState, (const char*)mtctx->inBuff.buffer.start + mtctx->inBuff.prefixSize, srcSize);

        /* get a new buffer for next input */
        if (!endFrame) {
            size_t const newPrefixSize = MIN(mtctx->inBuff.filled, mtctx->targetPrefixSize);
            mtctx->inBuff.buffer = ZSTDMT_getBuffer(mtctx->bufPool);
            if (mtctx->inBuff.buffer.start == NULL) {    /* not enough memory to allocate a new input buffer */
                mtctx->jobs[jobID].srcSize = mtctx->jobs[jobID].consumed = 0;
                mtctx->nextJobID++;
                ZSTDMT_waitForAllJobsCompleted(mtctx);
                ZSTDMT_releaseAllJobResources(mtctx);
                return ERROR(memory_allocation);
            }
            mtctx->inBuff.filled -= (mtctx->inBuff.prefixSize + srcSize) - newPrefixSize;
            memmove(mtctx->inBuff.buffer.start,   /* copy end of current job into next job, as "prefix" */
                (const char*)mtctx->jobs[jobID].prefixStart + mtctx->inBuff.prefixSize + srcSize - newPrefixSize,
                mtctx->inBuff.filled);
            mtctx->inBuff.prefixSize = newPrefixSize;
        } else {   /* endFrame==1 => no need for another input buffer */
            mtctx->inBuff.buffer = g_nullBuffer;
            mtctx->inBuff.filled = 0;
            mtctx->inBuff.prefixSize = 0;
            mtctx->frameEnded = endFrame;
            if (mtctx->nextJobID == 0) {
                /* single job exception : checksum is already calculated directly within worker thread */
                mtctx->params.fParams.checksumFlag = 0;
        }   }

        if ( (srcSize == 0)
          && (mtctx->nextJobID>0)/*single job must also write frame header*/ ) {
            DEBUGLOG(5, "ZSTDMT_createCompressionJob: creating a last empty block to end frame");
            assert(endOp == ZSTD_e_end);  /* only possible case : need to end the frame with an empty last block */
            ZSTDMT_writeLastEmptyBlock(mtctx->jobs + jobID);
            mtctx->nextJobID++;
            return 0;
        }
    }

    DEBUGLOG(5, "ZSTDMT_createCompressionJob: posting job %u : %u bytes  (end:%u, jobNb == %u (mod:%u))",
                mtctx->nextJobID,
                (U32)mtctx->jobs[jobID].srcSize,
                mtctx->jobs[jobID].lastJob,
                mtctx->nextJobID,
                jobID);
    if (POOL_tryAdd(mtctx->factory, ZSTDMT_compressionJob, &mtctx->jobs[jobID])) {
        mtctx->nextJobID++;
        mtctx->jobReady = 0;
    } else {
        DEBUGLOG(5, "ZSTDMT_createCompressionJob: no worker available for job %u", mtctx->nextJobID);
        mtctx->jobReady = 1;
    }
    return 0;
}


/*! ZSTDMT_flushProduced() :
 * `output` : `pos` will be updated with amount of data flushed .
 * `blockToFlush` : if >0, the function will block and wait if there is no data available to flush .
 * @return : amount of data remaining within internal buffer, 0 if no more, 1 if unknown but > 0, or an error code */
static size_t ZSTDMT_flushProduced(ZSTDMT_CCtx* mtctx, ZSTD_outBuffer* output, unsigned blockToFlush, ZSTD_EndDirective end)
{
    unsigned const wJobID = mtctx->doneJobID & mtctx->jobIDMask;
    DEBUGLOG(5, "ZSTDMT_flushProduced (blocking:%u , job %u <= %u)",
                blockToFlush, mtctx->doneJobID, mtctx->nextJobID);
    assert(output->size >= output->pos);

    ZSTD_PTHREAD_MUTEX_LOCK(&mtctx->jobs[wJobID].job_mutex);
    if (  blockToFlush
      && (mtctx->doneJobID < mtctx->nextJobID) ) {
        assert(mtctx->jobs[wJobID].dstFlushed <= mtctx->jobs[wJobID].cSize);
        while (mtctx->jobs[wJobID].dstFlushed == mtctx->jobs[wJobID].cSize) {  /* nothing to flush */
            if (mtctx->jobs[wJobID].consumed == mtctx->jobs[wJobID].srcSize) {
                DEBUGLOG(5, "job %u is completely consumed (%u == %u) => don't wait for cond, there will be none",
                            mtctx->doneJobID, (U32)mtctx->jobs[wJobID].consumed, (U32)mtctx->jobs[wJobID].srcSize);
                break;
            }
            DEBUGLOG(5, "waiting for something to flush from job %u (currently flushed: %u bytes)",
                        mtctx->doneJobID, (U32)mtctx->jobs[wJobID].dstFlushed);
            ZSTD_pthread_cond_wait(&mtctx->jobs[wJobID].job_cond, &mtctx->jobs[wJobID].job_mutex);  /* block when nothing to flush but some to come */
    }   }

    /* try to flush something */
    {   size_t cSize = mtctx->jobs[wJobID].cSize;                  /* shared */
        size_t const srcConsumed = mtctx->jobs[wJobID].consumed;   /* shared */
        size_t const srcSize = mtctx->jobs[wJobID].srcSize;        /* read-only, could be done after mutex lock, but no-declaration-after-statement */
        ZSTD_pthread_mutex_unlock(&mtctx->jobs[wJobID].job_mutex);
        if (ZSTD_isError(cSize)) {
            DEBUGLOG(5, "ZSTDMT_flushProduced: job %u : compression error detected : %s",
                        mtctx->doneJobID, ZSTD_getErrorName(cSize));
            ZSTDMT_waitForAllJobsCompleted(mtctx);
            ZSTDMT_releaseAllJobResources(mtctx);
            return cSize;
        }
        /* add frame checksum if necessary (can only happen once) */
        assert(srcConsumed <= srcSize);
        if ( (srcConsumed == srcSize)   /* job completed -> worker no longer active */
          && mtctx->jobs[wJobID].frameChecksumNeeded ) {
            U32 const checksum = (U32)XXH64_digest(&mtctx->xxhState);
            DEBUGLOG(4, "ZSTDMT_flushProduced: writing checksum : %08X \n", checksum);
            MEM_writeLE32((char*)mtctx->jobs[wJobID].dstBuff.start + mtctx->jobs[wJobID].cSize, checksum);
            cSize += 4;
            mtctx->jobs[wJobID].cSize += 4;  /* can write this shared value, as worker is no longer active */
            mtctx->jobs[wJobID].frameChecksumNeeded = 0;
        }
        if (cSize > 0) {   /* compression is ongoing or completed */
            size_t const toFlush = MIN(cSize - mtctx->jobs[wJobID].dstFlushed, output->size - output->pos);
            DEBUGLOG(5, "ZSTDMT_flushProduced: Flushing %u bytes from job %u (completion:%u/%u, generated:%u)",
                        (U32)toFlush, mtctx->doneJobID, (U32)srcConsumed, (U32)srcSize, (U32)cSize);
            assert(mtctx->doneJobID < mtctx->nextJobID);
            assert(cSize >= mtctx->jobs[wJobID].dstFlushed);
            assert(mtctx->jobs[wJobID].dstBuff.start != NULL);
            memcpy((char*)output->dst + output->pos,
                   (const char*)mtctx->jobs[wJobID].dstBuff.start + mtctx->jobs[wJobID].dstFlushed,
                   toFlush);
            output->pos += toFlush;
            mtctx->jobs[wJobID].dstFlushed += toFlush;  /* can write : this value is only used by mtctx */

            if ( (srcConsumed == srcSize)    /* job completed */
              && (mtctx->jobs[wJobID].dstFlushed == cSize) ) {   /* output buffer fully flushed => free this job position */
                DEBUGLOG(5, "Job %u completed (%u bytes), moving to next one",
                        mtctx->doneJobID, (U32)mtctx->jobs[wJobID].dstFlushed);
                assert(mtctx->jobs[wJobID].srcBuff.start == NULL);  /* srcBuff supposed already released */
                ZSTDMT_releaseBuffer(mtctx->bufPool, mtctx->jobs[wJobID].dstBuff);
                mtctx->jobs[wJobID].dstBuff = g_nullBuffer;
                mtctx->jobs[wJobID].cSize = 0;   /* ensure this job slot is considered "not started" in future check */
                mtctx->consumed += srcSize;
                mtctx->produced += cSize;
                mtctx->doneJobID++;
        }   }

        /* return value : how many bytes left in buffer ; fake it to 1 when unknown but >0 */
        if (cSize > mtctx->jobs[wJobID].dstFlushed) return (cSize - mtctx->jobs[wJobID].dstFlushed);
        if (srcSize > srcConsumed) return 1;   /* current job not completely compressed */
    }
    if (mtctx->doneJobID < mtctx->nextJobID) return 1;   /* some more jobs ongoing */
    if (mtctx->jobReady) return 1;      /* one job is ready to push, just not yet in the list */
    if (mtctx->inBuff.filled > 0) return 1;   /* input is not empty, and still needs to be converted into a job */
    mtctx->allJobsCompleted = mtctx->frameEnded;   /* all jobs are entirely flushed => if this one is last one, frame is completed */
    if (end == ZSTD_e_end) return !mtctx->frameEnded;  /* for ZSTD_e_end, question becomes : is frame completed ? instead of : are internal buffers fully flushed ? */
    return 0;   /* internal buffers fully flushed */
}


/** ZSTDMT_compressStream_generic() :
 *  internal use only - exposed to be invoked from zstd_compress.c
 *  assumption : output and input are valid (pos <= size)
 * @return : minimum amount of data remaining to flush, 0 if none */
size_t ZSTDMT_compressStream_generic(ZSTDMT_CCtx* mtctx,
                                     ZSTD_outBuffer* output,
                                     ZSTD_inBuffer* input,
                                     ZSTD_EndDirective endOp)
{
    size_t const newJobThreshold = mtctx->inBuff.prefixSize + mtctx->targetSectionSize;
    unsigned forwardInputProgress = 0;
    DEBUGLOG(5, "ZSTDMT_compressStream_generic (endOp=%u, srcSize=%u)",
                (U32)endOp, (U32)(input->size - input->pos));
    assert(output->pos <= output->size);
    assert(input->pos  <= input->size);

    if (mtctx->singleBlockingThread) {  /* delegate to single-thread (synchronous) */
        return ZSTD_compressStream_generic(mtctx->cctxPool->cctx[0], output, input, endOp);
    }

    if ((mtctx->frameEnded) && (endOp==ZSTD_e_continue)) {
        /* current frame being ended. Only flush/end are allowed */
        return ERROR(stage_wrong);
    }

    /* single-pass shortcut (note : synchronous-mode) */
    if ( (mtctx->nextJobID == 0)      /* just started */
      && (mtctx->inBuff.filled == 0)  /* nothing buffered */
      && (!mtctx->jobReady)           /* no job already created */
      && (endOp == ZSTD_e_end)        /* end order */
      && (output->size - output->pos >= ZSTD_compressBound(input->size - input->pos)) ) { /* enough space in dst */
        size_t const cSize = ZSTDMT_compress_advanced_internal(mtctx,
                (char*)output->dst + output->pos, output->size - output->pos,
                (const char*)input->src + input->pos, input->size - input->pos,
                mtctx->cdict, mtctx->params);
        if (ZSTD_isError(cSize)) return cSize;
        input->pos = input->size;
        output->pos += cSize;
        ZSTDMT_releaseBuffer(mtctx->bufPool, mtctx->inBuff.buffer);  /* was allocated in initStream */
        mtctx->allJobsCompleted = 1;
        mtctx->frameEnded = 1;
        return 0;
    }

    /* fill input buffer */
    if ( (!mtctx->jobReady)
      && (input->size > input->pos) ) {   /* support NULL input */
        if (mtctx->inBuff.buffer.start == NULL) {
            mtctx->inBuff.buffer = ZSTDMT_getBuffer(mtctx->bufPool);  /* note : allocation can fail, in which case, buffer.start==NULL */
            mtctx->inBuff.filled = 0;
            if ( (mtctx->inBuff.buffer.start == NULL)        /* allocation failure */
              && (mtctx->doneJobID == mtctx->nextJobID) ) {  /* and nothing to flush */
                return ERROR(memory_allocation);    /* no forward progress possible => output an error */
            }
            assert(mtctx->inBuff.buffer.capacity >= mtctx->inBuff.targetCapacity);  /* pool must provide a buffer >= targetCapacity */
        }
        if (mtctx->inBuff.buffer.start != NULL) {   /* no buffer for input, but it's possible to flush, and then reclaim the buffer */
            size_t const toLoad = MIN(input->size - input->pos, mtctx->inBuff.targetCapacity - mtctx->inBuff.filled);
            DEBUGLOG(5, "ZSTDMT_compressStream_generic: adding %u bytes on top of %u to buffer of size %u",
                        (U32)toLoad, (U32)mtctx->inBuff.filled, (U32)mtctx->inBuff.targetCapacity);
            memcpy((char*)mtctx->inBuff.buffer.start + mtctx->inBuff.filled, (const char*)input->src + input->pos, toLoad);
            input->pos += toLoad;
            mtctx->inBuff.filled += toLoad;
            forwardInputProgress = toLoad>0;
        }
        if ((input->pos < input->size) && (endOp == ZSTD_e_end))
            endOp = ZSTD_e_flush;   /* can't end now : not all input consumed */
    }

    if ( (mtctx->jobReady)
      || (mtctx->inBuff.filled >= newJobThreshold)  /* filled enough : let's compress */
      || ((endOp != ZSTD_e_continue) && (mtctx->inBuff.filled > 0))  /* something to flush : let's go */
      || ((endOp == ZSTD_e_end) && (!mtctx->frameEnded)) ) {   /* must finish the frame with a zero-size block */
        size_t const jobSize = MIN(mtctx->inBuff.filled - mtctx->inBuff.prefixSize, mtctx->targetSectionSize);
        CHECK_F( ZSTDMT_createCompressionJob(mtctx, jobSize, endOp) );
    }

    /* check for potential compressed data ready to be flushed */
    {   size_t const remainingToFlush = ZSTDMT_flushProduced(mtctx, output, !forwardInputProgress, endOp); /* block if there was no forward input progress */
        if (input->pos < input->size) return MAX(remainingToFlush, 1);  /* input not consumed : do not end flush yet */
        return remainingToFlush;
    }
}


size_t ZSTDMT_compressStream(ZSTDMT_CCtx* mtctx, ZSTD_outBuffer* output, ZSTD_inBuffer* input)
{
    CHECK_F( ZSTDMT_compressStream_generic(mtctx, output, input, ZSTD_e_continue) );

    /* recommended next input size : fill current input buffer */
    return mtctx->inBuff.targetCapacity - mtctx->inBuff.filled;   /* note : could be zero when input buffer is fully filled and no more availability to create new job */
}


static size_t ZSTDMT_flushStream_internal(ZSTDMT_CCtx* mtctx, ZSTD_outBuffer* output, ZSTD_EndDirective endFrame)
{
    size_t const srcSize = mtctx->inBuff.filled - mtctx->inBuff.prefixSize;
    DEBUGLOG(5, "ZSTDMT_flushStream_internal");

    if ( mtctx->jobReady     /* one job ready for a worker to pick up */
      || (srcSize > 0)       /* still some data within input buffer */
      || ((endFrame==ZSTD_e_end) && !mtctx->frameEnded)) {  /* need a last 0-size block to end frame */
           DEBUGLOG(5, "ZSTDMT_flushStream_internal : create a new job (%u bytes, end:%u)",
                        (U32)srcSize, (U32)endFrame);
        CHECK_F( ZSTDMT_createCompressionJob(mtctx, srcSize, endFrame) );
    }

    /* check if there is any data available to flush */
    return ZSTDMT_flushProduced(mtctx, output, 1 /* blockToFlush */, endFrame);
}


size_t ZSTDMT_flushStream(ZSTDMT_CCtx* mtctx, ZSTD_outBuffer* output)
{
    DEBUGLOG(5, "ZSTDMT_flushStream");
    if (mtctx->singleBlockingThread)
        return ZSTD_flushStream(mtctx->cctxPool->cctx[0], output);
    return ZSTDMT_flushStream_internal(mtctx, output, ZSTD_e_flush);
}

size_t ZSTDMT_endStream(ZSTDMT_CCtx* mtctx, ZSTD_outBuffer* output)
{
    DEBUGLOG(4, "ZSTDMT_endStream");
    if (mtctx->singleBlockingThread)
        return ZSTD_endStream(mtctx->cctxPool->cctx[0], output);
    return ZSTDMT_flushStream_internal(mtctx, output, ZSTD_e_end);
}
