#include <stdlib.h>   /* malloc */
#include <pool.h>     /* threadpool */
#include <pthread.h>  /* mutex */
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
      DEBUGLOG(MUTEX_WAIT_TIME_DLEVEL, "Thread %li took %llu microseconds to acquire mutex %s \n", \
                (long int) pthread_self(), elapsedTime, #mutex); \
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

/* note : invocation only from main thread ! */
static buffer_t ZSTDMT_getBuffer(ZSTDMT_bufferPool* pool, size_t bSize)
{
    if (pool->nbBuffers) {   /* try to use an existing buffer */
        pool->nbBuffers--;
        buffer_t const buf = pool->bTable[pool->nbBuffers];
        size_t const availBufferSize = buf.size;
        if ((availBufferSize >= bSize) & (availBufferSize <= 10*bSize))   /* large enough, but not too much */
            return buf;
        free(buf.start);   /* size conditions not respected : create a new buffer */
    }
    /* create new buffer */
    buffer_t buf;
    buf.size = bSize;
    buf.start = malloc(bSize);
    return buf;
}

/* effectively store buffer for later re-use, up to pool capacity */
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
    size_t cSize;
    unsigned jobCompleted;
    pthread_mutex_t* jobCompleted_mutex;
} ZSTDMT_jobDescription;

/* ZSTDMT_compressFrame() : POOL_function type */
void ZSTDMT_compressFrame(void* jobDescription)
{
    DEBUGLOG(5, "Entering ZSTDMT_compressFrame() ");
    ZSTDMT_jobDescription* const job = (ZSTDMT_jobDescription*)jobDescription;
    DEBUGLOG(5, "compressing %u bytes with ZSTD_compressCCtx : ", (unsigned)job->srcSize);
    job->cSize = ZSTD_compressCCtx(job->cctx, job->dstBuff.start, job->dstBuff.size, job->srcStart, job->srcSize, job->compressionLevel);
    DEBUGLOG(5, "compressed to %u bytes  ", (unsigned)job->cSize);
    job->jobCompleted = 1;
    DEBUGLOG(5, "unlocking mutex jobCompleted_mutex");
    pthread_mutex_unlock(job->jobCompleted_mutex);
    DEBUGLOG(5, "ZSTDMT_compressFrame completed");
}


/* ===   CCtx Pool   === */

typedef struct {
    unsigned totalCCtx;
    unsigned availCCtx;
    ZSTD_CCtx* cctx[1];   /* variable size */
} ZSTDMT_CCtxPool;

/* note : CCtxPool invocation only from main thread */

static ZSTDMT_CCtxPool* ZSTDMT_createCCtxPool(unsigned nbThreads)
{
    ZSTDMT_CCtxPool* const cctxPool = (ZSTDMT_CCtxPool*) calloc(1, sizeof(ZSTDMT_CCtxPool) + nbThreads*sizeof(ZSTD_CCtx*));
    if (!cctxPool) return NULL;
    {   unsigned u;
        for (u=0; u<nbThreads; u++)
            cctxPool->cctx[u] = ZSTD_createCCtx();   /* check for NULL result ! */
    }
    cctxPool->totalCCtx = cctxPool->availCCtx = nbThreads;
    return cctxPool;
}

static ZSTD_CCtx* ZSTDMT_getCCtx(ZSTDMT_CCtxPool* pool)
{
    if (pool->availCCtx) {
        pool->availCCtx--;
        return pool->cctx[pool->availCCtx];
    }
    /* should not be possible, since totalCCtx==nbThreads */
    return ZSTD_createCCtx();
}

static void ZSTDMT_releaseCCtx(ZSTDMT_CCtxPool* pool, ZSTD_CCtx* cctx)
{
    if (pool->availCCtx < pool->totalCCtx)
        pool->cctx[pool->availCCtx++] = cctx;
    else
    /* should not be possible, since totalCCtx==nbThreads */
        ZSTD_freeCCtx(cctx);
}

static void ZSTDMT_freeCCtxPool(ZSTDMT_CCtxPool* pool)
{
    unsigned u;
    for (u=0; u<pool->totalCCtx; u++)
        ZSTD_freeCCtx(pool->cctx[u]);
    free(pool);
}


struct ZSTDMT_CCtx_s {
    POOL_ctx* factory;
    ZSTDMT_bufferPool* buffPool;
    ZSTDMT_CCtxPool* cctxPool;
    unsigned nbThreads;
    pthread_mutex_t jobCompleted_mutex;
    ZSTDMT_jobDescription jobs[1];   /* variable size */
};

ZSTDMT_CCtx *ZSTDMT_createCCtx(unsigned nbThreads)
{
    if ((nbThreads < 1) | (nbThreads > ZSTDMT_NBTHREADS_MAX)) return NULL;
    ZSTDMT_CCtx* const cctx = (ZSTDMT_CCtx*) calloc(1, sizeof(ZSTDMT_CCtx) + nbThreads*sizeof(ZSTDMT_jobDescription));
    if (!cctx) return NULL;
    cctx->nbThreads = nbThreads;
    cctx->factory = POOL_create(nbThreads, 1);
    cctx->buffPool = ZSTDMT_createBufferPool(nbThreads);
    cctx->cctxPool = ZSTDMT_createCCtxPool(nbThreads);
    pthread_mutex_init(&cctx->jobCompleted_mutex, NULL);
    return cctx;
}

size_t ZSTDMT_freeCCtx(ZSTDMT_CCtx* mtctx)  /* incompleted ! */
{
    POOL_free(mtctx->factory);
    ZSTDMT_freeBufferPool(mtctx->buffPool);
    ZSTDMT_freeCCtxPool(mtctx->cctxPool);
    pthread_mutex_destroy(&mtctx->jobCompleted_mutex);
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
            mtctx->jobs[u].compressionLevel = compressionLevel;
            mtctx->jobs[u].dstBuff = dstBuffer;
            mtctx->jobs[u].cctx = cctx;
            mtctx->jobs[u].frameID = u;
            mtctx->jobs[u].jobCompleted = 0;
            mtctx->jobs[u].jobCompleted_mutex = &mtctx->jobCompleted_mutex;

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
            while (mtctx->jobs[frameID].jobCompleted==0) {
                DEBUGLOG(4, "waiting for signal jobCompleted_mutex")
                pthread_mutex_lock(&mtctx->jobCompleted_mutex);
            }
            {   size_t const cSize = mtctx->jobs[frameID].cSize;
                if (ZSTD_isError(cSize)) return cSize;
                if (dstPos + cSize > dstCapacity) return ERROR(dstSize_tooSmall);
                if (frameID) {
                    memcpy((char*)dst + dstPos, mtctx->jobs[frameID].dstBuff.start, cSize);
                    ZSTDMT_releaseBuffer(mtctx->buffPool, mtctx->jobs[frameID].dstBuff);
                }
                dstPos += cSize ;
            }
            ZSTDMT_releaseCCtx(mtctx->cctxPool, mtctx->jobs[frameID].cctx);
        }
        DEBUGLOG(3, "compressed size : %u  ", (U32)dstPos);
        return dstPos;
    }

}
