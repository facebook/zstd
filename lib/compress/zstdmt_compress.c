#include <stdlib.h>   /* malloc */
#include <pthread.h>
#include "zstd_internal.h"   /* MIN, ERROR */
#include "zstdmt_compress.h"

#if 0
#  include <stdio.h>
   static unsigned g_debugLevel = 4;
#  define DEBUGLOG(l, ...) if (l<=g_debugLevel) { fprintf(stderr, __VA_ARGS__); fprintf(stderr, " \n"); }
#else
#  define DEBUGLOG(l, ...)   /* disabled */
#endif

#define ZSTDMT_NBTHREADS_MAX 128
#define ZSTDMT_NBSTACKEDFRAMES_MAX (2*ZSTDMT_NBTHREADS_MAX)

typedef struct frameToWrite_s {
    const void* start;
    size_t frameSize;
    unsigned frameID;
    unsigned isLastFrame;
} frameToWrite_t;

typedef struct ZSTDMT_dstBuffer_s {
    ZSTD_outBuffer out;
    unsigned frameIDToWrite;
    pthread_mutex_t frameTable_mutex;
    pthread_mutex_t allFramesWritten_mutex;
    frameToWrite_t stackedFrame[ZSTDMT_NBSTACKEDFRAMES_MAX];
    unsigned nbStackedFrames;
} ZSTDMT_dstBufferManager;

static ZSTDMT_dstBufferManager ZSTDMT_createDstBufferManager(void* dst, size_t dstCapacity)
{
    ZSTDMT_dstBufferManager dbm;
    dbm.out.dst = dst;
    dbm.out.size = dstCapacity;
    dbm.out.pos = 0;
    dbm.frameIDToWrite = 0;
    pthread_mutex_init(&dbm.frameTable_mutex, NULL);
    pthread_mutex_init(&dbm.allFramesWritten_mutex, NULL);
    pthread_mutex_lock(&dbm.allFramesWritten_mutex);
    dbm.nbStackedFrames = 0;
    return dbm;
}

/* note : can fail if nbStackedFrames > ZSTDMT_NBSTACKEDFRAMES_MAX.
 * note2 : can only be called from a section with frameTable_mutex already locked */
static void ZSTDMT_stackFrameToWrite(ZSTDMT_dstBufferManager* dstBufferManager, frameToWrite_t frame) {
    dstBufferManager->stackedFrame[dstBufferManager->nbStackedFrames++] = frame;
}


typedef struct buffer_s {
    void* start;
    size_t bufferSize;
} buffer_t;

static buffer_t ZSTDMT_getDstBuffer(const ZSTDMT_dstBufferManager* dstBufferManager)
{
    ZSTD_outBuffer const out = dstBufferManager->out;
    buffer_t buf;
    buf.start = (char*)(out.dst) + out.pos;
    buf.bufferSize = out.size - out.pos;
    return buf;
}

/* condition : stackNumber < dstBufferManager->nbStackedFrames.
 * note : there can only be one write at a time, due to frameID condition */
static size_t ZSTDMT_writeFrame(ZSTDMT_dstBufferManager* dstBufferManager, unsigned stackNumber)
{
    ZSTD_outBuffer const out = dstBufferManager->out;
    size_t const frameSize = dstBufferManager->stackedFrame[stackNumber].frameSize;
    const void* const frameStart = dstBufferManager->stackedFrame[stackNumber].start;
    if (out.pos + frameSize > out.size)
        return ERROR(dstSize_tooSmall);
    DEBUGLOG(3, "writing frame %u (%u bytes) ", dstBufferManager->stackedFrame[stackNumber].frameID, (U32)frameSize);
    memcpy((char*)out.dst + out.pos, frameStart, frameSize);
    dstBufferManager->out.pos += frameSize;
    dstBufferManager->frameIDToWrite = dstBufferManager->stackedFrame[stackNumber].frameID + 1;
    return 0;
}

static size_t ZSTDMT_tryWriteFrame(ZSTDMT_dstBufferManager* dstBufferManager,
                                   const void* src, size_t srcSize,
                                   unsigned frameID, unsigned isLastFrame)
{
    unsigned lastFrameWritten = 0;

    /* check if correct frame ordering; stack otherwise */
    DEBUGLOG(5, "considering writing frame %u ", frameID);
    pthread_mutex_lock(&dstBufferManager->frameTable_mutex);
    if (frameID != dstBufferManager->frameIDToWrite) {
        DEBUGLOG(4, "writing frameID %u : not possible, waiting for %u  ", frameID, dstBufferManager->frameIDToWrite);
        frameToWrite_t frame = { src, srcSize, frameID, isLastFrame };
        ZSTDMT_stackFrameToWrite(dstBufferManager, frame);
        pthread_mutex_unlock(&dstBufferManager->frameTable_mutex);
        return 0;
    }
    pthread_mutex_unlock(&dstBufferManager->frameTable_mutex);

    /* write frame
     * note : only one write possible due to frameID condition */
    DEBUGLOG(3, "writing frame %u (%u bytes) ", frameID, (U32)srcSize);
    ZSTD_outBuffer const out = dstBufferManager->out;
    if (out.pos + srcSize > out.size)
        return ERROR(dstSize_tooSmall);
    if (frameID) /* frameID==0 compress directly in dst buffer */
        memcpy((char*)out.dst + out.pos, src, srcSize);
    dstBufferManager->out.pos += srcSize;
    dstBufferManager->frameIDToWrite = frameID+1;
    lastFrameWritten = isLastFrame;

    /* check if more frames are stacked */
    pthread_mutex_lock(&dstBufferManager->frameTable_mutex);
    unsigned frameWritten = dstBufferManager->nbStackedFrames>0;
    while (frameWritten) {
        unsigned u;
        frameID++;
        frameWritten = 0;
        for (u=0; u<dstBufferManager->nbStackedFrames; u++) {
            if (dstBufferManager->stackedFrame[u].frameID == frameID) {
                pthread_mutex_unlock(&dstBufferManager->frameTable_mutex);
                { size_t const writeError = ZSTDMT_writeFrame(dstBufferManager, u);
                  if (ZSTD_isError(writeError)) return writeError; }
                lastFrameWritten = dstBufferManager->stackedFrame[u].isLastFrame;
                /* remove frame from stack */
                pthread_mutex_lock(&dstBufferManager->frameTable_mutex);
                dstBufferManager->stackedFrame[u] = dstBufferManager->stackedFrame[dstBufferManager->nbStackedFrames-1];
                dstBufferManager->nbStackedFrames -= 1;
                frameWritten = dstBufferManager->nbStackedFrames>0;
                break;
    }   }   }
    pthread_mutex_unlock(&dstBufferManager->frameTable_mutex);

    /* end reached : last frame written */
    if (lastFrameWritten) pthread_mutex_unlock(&dstBufferManager->allFramesWritten_mutex);
    return 0;
}



typedef struct ZSTDMT_jobDescription_s {
    const void* src;   /* NULL means : kill thread */
    size_t srcSize;
    int compressionLevel;
    ZSTDMT_dstBufferManager* dstManager;
    unsigned frameNumber;
    unsigned isLastFrame;
} ZSTDMT_jobDescription;

typedef struct ZSTDMT_jobAgency_s {
    pthread_mutex_t jobAnnounce_mutex;
    pthread_mutex_t jobApply_mutex;
    ZSTDMT_jobDescription jobAnnounce;
} ZSTDMT_jobAgency;

/* ZSTDMT_postjob() :
 * This function is blocking as long as previous posted job is not taken.
 * It could be made non-blocking, with a storage queue.
 * But blocking has benefits : on top of memory savings,
 * the caller will be able to measure delay, allowing dynamic speed throttle (via compression level).
 */
static void ZSTDMT_postjob(ZSTDMT_jobAgency* jobAgency, ZSTDMT_jobDescription job)
{
    DEBUGLOG(5, "starting job posting  ");
    pthread_mutex_lock(&jobAgency->jobApply_mutex);   /* wait for a thread to take previous job */
    DEBUGLOG(5, "job posting mutex acquired ");
    jobAgency->jobAnnounce = job;   /* post job */
    pthread_mutex_unlock(&jobAgency->jobAnnounce_mutex);   /* announce */
    DEBUGLOG(5, "job available now ");
}

static ZSTDMT_jobDescription ZSTDMT_getjob(ZSTDMT_jobAgency* jobAgency)
{
    pthread_mutex_lock(&jobAgency->jobAnnounce_mutex);   /* should check return code */
    ZSTDMT_jobDescription const job = jobAgency->jobAnnounce;
    pthread_mutex_unlock(&jobAgency->jobApply_mutex);
    return job;
}



#define ZSTDMT_NBBUFFERSPOOLED_MAX ZSTDMT_NBTHREADS_MAX
typedef struct ZSTDMT_bufferPool_s {
    buffer_t bTable[ZSTDMT_NBBUFFERSPOOLED_MAX];
    unsigned nbBuffers;
} ZSTDMT_bufferPool;

static buffer_t ZSTDMT_getBuffer(ZSTDMT_bufferPool* pool, size_t bSize)
{
    if (pool->nbBuffers) {   /* try to use an existing buffer */
        pool->nbBuffers--;
        buffer_t const buf = pool->bTable[pool->nbBuffers];
        size_t const availBufferSize = buf.bufferSize;
        if ((availBufferSize >= bSize) & (availBufferSize <= 10*bSize))   /* large enough, but not too much */
            return buf;
        free(buf.start);   /* size conditions not respected : create a new buffer */
    }
    /* create new buffer */
    buffer_t buf;
    buf.bufferSize = bSize;
    buf.start = calloc(1, bSize);
    return buf;
}

/* effectively store buffer for later re-use, up to pool capacity */
static void ZSTDMT_releaseBuffer(ZSTDMT_bufferPool* pool, buffer_t buf)
{
    if (pool->nbBuffers >= ZSTDMT_NBBUFFERSPOOLED_MAX) {
        free(buf.start);
        return;
    }
    pool->bTable[pool->nbBuffers++] = buf;   /* store for later re-use */
}



struct ZSTDMT_CCtx_s {
    pthread_t pthread[ZSTDMT_NBTHREADS_MAX];
    unsigned nbThreads;
    ZSTDMT_jobAgency jobAgency;
    ZSTDMT_bufferPool bufferPool;
};

static void* ZSTDMT_compressionThread(void* arg)
{
    if (arg==NULL) return NULL;   /* error : should not be possible */
    ZSTDMT_CCtx* const cctx = (ZSTDMT_CCtx*) arg;
    ZSTDMT_jobAgency* const jobAgency = &cctx->jobAgency;
    ZSTDMT_bufferPool* const pool = &cctx->bufferPool;
    for (;;) {
        ZSTDMT_jobDescription const job = ZSTDMT_getjob(jobAgency);
        if (job.src == NULL) {
            DEBUGLOG(4, "thread exit  ")
            return NULL;
        }
        ZSTDMT_dstBufferManager* dstBufferManager = job.dstManager;
        size_t const dstBufferCapacity = ZSTD_compressBound(job.srcSize);
        DEBUGLOG(4, "requesting a dstBuffer for frame %u", job.frameNumber);
        buffer_t const dstBuffer = job.frameNumber ? ZSTDMT_getBuffer(pool, dstBufferCapacity) : ZSTDMT_getDstBuffer(dstBufferManager);  /* lack params */
        DEBUGLOG(4, "start compressing frame %u", job.frameNumber);
        size_t const cSize = ZSTD_compress(dstBuffer.start, dstBuffer.bufferSize, job.src, job.srcSize, job.compressionLevel);
        if (ZSTD_isError(cSize)) return (void*)(cSize);   /* error */
        size_t const writeError = ZSTDMT_tryWriteFrame(dstBufferManager, dstBuffer.start, cSize, job.frameNumber, job.isLastFrame);   /* pas clair */
        if (ZSTD_isError(writeError)) return (void*)writeError;
        if (job.frameNumber) ZSTDMT_releaseBuffer(pool, dstBuffer);
    }
}

ZSTDMT_CCtx *ZSTDMT_createCCtx(unsigned nbThreads)
{
    if ((nbThreads < 1) | (nbThreads > ZSTDMT_NBTHREADS_MAX)) return NULL;
    ZSTDMT_CCtx* const cctx = (ZSTDMT_CCtx*) calloc(1, sizeof(ZSTDMT_CCtx));
    if (!cctx) return NULL;
    pthread_mutex_init(&cctx->jobAgency.jobAnnounce_mutex, NULL);   /* check return value ? */
    pthread_mutex_init(&cctx->jobAgency.jobApply_mutex, NULL);
    pthread_mutex_lock(&cctx->jobAgency.jobAnnounce_mutex);   /* no job at beginning */
    /* start all workers */
    cctx->nbThreads = nbThreads;
    DEBUGLOG(2, "nbThreads : %u \n", nbThreads);
    unsigned t;
    for (t = 0; t < nbThreads; t++) {
        pthread_create(&cctx->pthread[t], NULL, ZSTDMT_compressionThread, cctx);  /* check return value ? */
    }
    return cctx;
}

size_t ZSTDMT_freeCCtx(ZSTDMT_CCtx* cctx)
{
    /* free threads */
    /* free mutex (if necessary) */
    /* free bufferPool */
    free(cctx);   /* incompleted ! */
    return 0;
}

size_t ZSTDMT_compressCCtx(ZSTDMT_CCtx* cctx,
                           void* dst, size_t dstCapacity,
                     const void* src, size_t srcSize,
                           int compressionLevel)
{
    ZSTDMT_jobAgency* jobAgency = &cctx->jobAgency;
    ZSTD_parameters const params = ZSTD_getParams(compressionLevel, srcSize, 0);
    size_t const frameSizeTarget = (size_t)1 << (params.cParams.windowLog + 2);
    unsigned const nbFrames = (unsigned)(srcSize / frameSizeTarget) + (srcSize < frameSizeTarget) /* min 1 */;
    size_t const avgFrameSize = (srcSize + (nbFrames-1)) / nbFrames;
    size_t remainingSrcSize = srcSize;
    const char* const srcStart = (const char*)src;
    size_t frameStartPos = 0;
    ZSTDMT_dstBufferManager dbm = ZSTDMT_createDstBufferManager(dst, dstCapacity);

    DEBUGLOG(2, "windowLog : %u   => frameSizeTarget : %u      ", params.cParams.windowLog, (U32)frameSizeTarget);
    DEBUGLOG(2, "nbFrames : %u   (size : %u bytes)   ", nbFrames, (U32)avgFrameSize);

    {   unsigned u;
        for (u=0; u<nbFrames; u++) {
            size_t const frameSize = MIN(remainingSrcSize, avgFrameSize);
            DEBUGLOG(3, "posting job %u   (%u bytes)", u, (U32)frameSize);
            ZSTDMT_jobDescription const job = { srcStart+frameStartPos, frameSize, compressionLevel,
                                                &dbm, u, u==(nbFrames-1) };
            ZSTDMT_postjob(jobAgency, job);
            frameStartPos += frameSize;
            remainingSrcSize -= frameSize;
    }   }

    pthread_mutex_lock(&dbm.allFramesWritten_mutex);
    DEBUGLOG(4, "compressed size : %u  ", (U32)dbm.out.pos);
    return dbm.out.pos;
}
