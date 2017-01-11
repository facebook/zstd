#include <stdlib.h>   /* malloc */
#include <pool.h>     /* threadpool */
#include "threading.h"  /* mutex */
#include "zstd_internal.h"   /* MIN, ERROR */
#include "zstdmt_compress.h"

#if 0
#  include <stdio.h>
#  include <unistd.h>
#  include <sys/times.h>
   static unsigned g_debugLevel = 2;
#  define DEBUGLOG(l, ...) if (l<=g_debugLevel) { fprintf(stderr, __VA_ARGS__); fprintf(stderr, " \n"); }

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

#  define DEBUGLOG(l, ...)   /* disabled */
#  define PTHREAD_MUTEX_LOCK(m) pthread_mutex_lock(m)

#endif


#define ZSTDMT_NBTHREADS_MAX 128

/* ===   Buffer Pool   === */

typedef struct buffer_s {
    void* start;
    size_t size;
} buffer_t;

typedef struct ZSTDMT_bufferPool_s {
    unsigned totalBuffers;;
    unsigned nbBuffers;
    buffer_t bTable[1];   /* variable size */
} ZSTDMT_bufferPool;

static ZSTDMT_bufferPool* ZSTDMT_createBufferPool(unsigned nbThreads)
{
    unsigned const maxNbBuffers = 2*nbThreads + 2;
    ZSTDMT_bufferPool* const bufPool = (ZSTDMT_bufferPool*)calloc(1, sizeof(ZSTDMT_bufferPool) + maxNbBuffers * sizeof(buffer_t));
    if (bufPool==NULL) return NULL;
    bufPool->totalBuffers = maxNbBuffers;
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
        free(buf.start);   /* size conditions not respected : create a new buffer */
    }
    /* create new buffer */
    {   buffer_t buf;
        buf.size = bSize;
        buf.start = malloc(bSize);
        return buf;
    }
}

/* store buffer for later re-use, up to pool capacity */
static void ZSTDMT_releaseBuffer(ZSTDMT_bufferPool* pool, buffer_t buf)
{
    if (pool->nbBuffers < pool->totalBuffers) {
        pool->bTable[pool->nbBuffers++] = buf;   /* store for later re-use */
        return;
    }
    /* Reached bufferPool capacity (should not happen) */
    free(buf.start);
}



typedef struct {
    ZSTD_CCtx* cctx;
    const void* srcStart;
    size_t srcSize;
    buffer_t dstBuff;
    int compressionLevel;
    unsigned frameID;
    unsigned long long fullFrameSize;
    size_t cSize;
    unsigned jobCompleted;
    pthread_mutex_t* jobCompleted_mutex;
    pthread_cond_t* jobCompleted_cond;
} ZSTDMT_jobDescription;

/* ZSTDMT_compressFrame() : POOL_function type */
void ZSTDMT_compressFrame(void* jobDescription)
{
    ZSTDMT_jobDescription* const job = (ZSTDMT_jobDescription*)jobDescription;
    buffer_t dstBuff = job->dstBuff;
    ZSTD_parameters const params = ZSTD_getParams(job->compressionLevel, job->fullFrameSize, 0);
    size_t hSize = ZSTD_compressBegin_advanced(job->cctx, NULL, 0, params, job->fullFrameSize);
    if (ZSTD_isError(hSize)) { job->cSize = hSize; goto _endJob; }
    hSize = ZSTD_compressContinue(job->cctx, dstBuff.start, dstBuff.size, job->srcStart, 0);   /* flush frame header */
    if (ZSTD_isError(hSize)) { job->cSize = hSize; goto _endJob; }
    if ((job->frameID & 1) == 0) {   /* preserve frame header when it is first beginning of frame */
        dstBuff.start = (char*)dstBuff.start + hSize;
        dstBuff.size -= hSize;
    } else
        hSize = 0;

    job->cSize = (job->frameID>=2) ?   /* last chunk signal */
                 ZSTD_compressEnd(job->cctx, dstBuff.start, dstBuff.size, job->srcStart, job->srcSize) :
                 ZSTD_compressContinue(job->cctx, dstBuff.start, dstBuff.size, job->srcStart, job->srcSize);
    if (!ZSTD_isError(job->cSize)) job->cSize += hSize;
    DEBUGLOG(5, "frame %u : compressed %u bytes into %u bytes  ", (unsigned)job->frameID, (unsigned)job->srcSize, (unsigned)job->cSize);

_endJob:
    PTHREAD_MUTEX_LOCK(job->jobCompleted_mutex);
    job->jobCompleted = 1;
    pthread_cond_signal(job->jobCompleted_cond);
    pthread_mutex_unlock(job->jobCompleted_mutex);
}


/* ===   CCtx Pool   === */

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
    for (u=0; u<pool->availCCtx; u++)  /* note : availCCtx is supposed == totalCCtx; otherwise, some CCtx are still in use */
        ZSTD_freeCCtx(pool->cctx[u]);
    free(pool);
}

static ZSTDMT_CCtxPool* ZSTDMT_createCCtxPool(unsigned nbThreads)
{
    ZSTDMT_CCtxPool* const cctxPool = (ZSTDMT_CCtxPool*) calloc(1, sizeof(ZSTDMT_CCtxPool) + nbThreads*sizeof(ZSTD_CCtx*));
    if (!cctxPool) return NULL;
    {   unsigned threadNb;
        for (threadNb=0; threadNb<nbThreads; threadNb++) {
            cctxPool->cctx[threadNb] = ZSTD_createCCtx();
            if (cctxPool->cctx[threadNb]==NULL) {   /* failed cctx allocation : abort cctxPool creation */
                cctxPool->totalCCtx = cctxPool->availCCtx = threadNb;
                ZSTDMT_freeCCtxPool(cctxPool);
                return NULL;
    }   }   }
    cctxPool->totalCCtx = cctxPool->availCCtx = nbThreads;
    return cctxPool;
}

static ZSTD_CCtx* ZSTDMT_getCCtx(ZSTDMT_CCtxPool* pool)
{
    if (pool->availCCtx) {
        pool->availCCtx--;
        return pool->cctx[pool->availCCtx];
    }
    /* note : should not be possible, since totalCCtx==nbThreads */
    return ZSTD_createCCtx();
}

static void ZSTDMT_releaseCCtx(ZSTDMT_CCtxPool* pool, ZSTD_CCtx* cctx)
{
    if (pool->availCCtx < pool->totalCCtx)
        pool->cctx[pool->availCCtx++] = cctx;
    else
    /* note : should not be possible, since totalCCtx==nbThreads */
        ZSTD_freeCCtx(cctx);
}


struct ZSTDMT_CCtx_s {
    POOL_ctx* factory;
    ZSTDMT_bufferPool* buffPool;
    ZSTDMT_CCtxPool* cctxPool;
    unsigned nbThreads;
    pthread_mutex_t jobCompleted_mutex;
    pthread_cond_t jobCompleted_cond;
    ZSTDMT_jobDescription jobs[1];   /* variable size */
};

ZSTDMT_CCtx *ZSTDMT_createCCtx(unsigned nbThreads)
{
    ZSTDMT_CCtx* cctx;
    if ((nbThreads < 1) | (nbThreads > ZSTDMT_NBTHREADS_MAX)) return NULL;
    cctx = (ZSTDMT_CCtx*) calloc(1, sizeof(ZSTDMT_CCtx) + nbThreads*sizeof(ZSTDMT_jobDescription));
    if (!cctx) return NULL;
    cctx->nbThreads = nbThreads;
    cctx->factory = POOL_create(nbThreads, 1);
    cctx->buffPool = ZSTDMT_createBufferPool(nbThreads);
    cctx->cctxPool = ZSTDMT_createCCtxPool(nbThreads);
    if (!cctx->factory | !cctx->buffPool | !cctx->cctxPool) {  /* one object was not created */
        ZSTDMT_freeCCtx(cctx);
        return NULL;
    }
    pthread_mutex_init(&cctx->jobCompleted_mutex, NULL);   /* Todo : check init function return */
    pthread_cond_init(&cctx->jobCompleted_cond, NULL);
    return cctx;
}

size_t ZSTDMT_freeCCtx(ZSTDMT_CCtx* mtctx)
{
    POOL_free(mtctx->factory);
    ZSTDMT_freeBufferPool(mtctx->buffPool);
    ZSTDMT_freeCCtxPool(mtctx->cctxPool);
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
    ZSTD_parameters const params = ZSTD_getParams(compressionLevel, srcSize, 0);
    size_t const frameSizeTarget = (size_t)1 << (params.cParams.windowLog + 2);
    unsigned const nbFramesMax = (unsigned)(srcSize / frameSizeTarget) + (srcSize < frameSizeTarget) /* min 1 */;
    unsigned const nbFrames = MIN(nbFramesMax, mtctx->nbThreads);
    size_t const avgFrameSize = (srcSize + (nbFrames-1)) / nbFrames;
    size_t remainingSrcSize = srcSize;
    const char* const srcStart = (const char*)src;
    size_t frameStartPos = 0;


    DEBUGLOG(2, "windowLog : %u   => frameSizeTarget : %u      ", params.cParams.windowLog, (U32)frameSizeTarget);
    DEBUGLOG(2, "nbFrames : %u   (size : %u bytes)   ", nbFrames, (U32)avgFrameSize);

    {   unsigned u;
        for (u=0; u<nbFrames; u++) {
            size_t const frameSize = MIN(remainingSrcSize, avgFrameSize);
            size_t const dstBufferCapacity = u ? ZSTD_compressBound(frameSize) : dstCapacity;
            buffer_t const dstBuffer = u ? ZSTDMT_getBuffer(mtctx->buffPool, dstBufferCapacity) : (buffer_t){ dst, dstCapacity };
            ZSTD_CCtx* cctx = ZSTDMT_getCCtx(mtctx->cctxPool);

            mtctx->jobs[u].srcStart = srcStart + frameStartPos;
            mtctx->jobs[u].srcSize = frameSize;
            mtctx->jobs[u].fullFrameSize = srcSize;
            mtctx->jobs[u].compressionLevel = compressionLevel;
            mtctx->jobs[u].dstBuff = dstBuffer;
            mtctx->jobs[u].cctx = cctx;
            mtctx->jobs[u].frameID = (u>0) | ((u==nbFrames-1)<<1);
            mtctx->jobs[u].jobCompleted = 0;
            mtctx->jobs[u].jobCompleted_mutex = &mtctx->jobCompleted_mutex;
            mtctx->jobs[u].jobCompleted_cond = &mtctx->jobCompleted_cond;

            DEBUGLOG(3, "posting job %u   (%u bytes)", u, (U32)frameSize);
            POOL_add(mtctx->factory, ZSTDMT_compressFrame, &mtctx->jobs[u]);

            frameStartPos += frameSize;
            remainingSrcSize -= frameSize;
    }   }
    /* note : since nbFrames <= nbThreads, all jobs should be running immediately in parallel */

    {   unsigned frameID;
        size_t dstPos = 0;
        for (frameID=0; frameID<nbFrames; frameID++) {
            DEBUGLOG(3, "ready to write frame %u ", frameID);

            PTHREAD_MUTEX_LOCK(&mtctx->jobCompleted_mutex);
            while (mtctx->jobs[frameID].jobCompleted==0) {
                DEBUGLOG(4, "waiting for jobCompleted signal from frame %u", frameID);
                pthread_cond_wait(&mtctx->jobCompleted_cond, &mtctx->jobCompleted_mutex);
            }
            pthread_mutex_unlock(&mtctx->jobCompleted_mutex);

            ZSTDMT_releaseCCtx(mtctx->cctxPool, mtctx->jobs[frameID].cctx);
            {   size_t const cSize = mtctx->jobs[frameID].cSize;
                if (ZSTD_isError(cSize)) return cSize;
                if (dstPos + cSize > dstCapacity) return ERROR(dstSize_tooSmall);
                if (frameID) {   /* note : frame 0 is already written directly into dst */
                    memcpy((char*)dst + dstPos, mtctx->jobs[frameID].dstBuff.start, cSize);
                    ZSTDMT_releaseBuffer(mtctx->buffPool, mtctx->jobs[frameID].dstBuff);
                }
                dstPos += cSize ;
            }
        }
        DEBUGLOG(3, "compressed size : %u  ", (U32)dstPos);
        return dstPos;
    }

}
