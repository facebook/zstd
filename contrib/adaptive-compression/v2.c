#define DISPLAY(...) fprintf(stderr, __VA_ARGS__)
#define FILE_CHUNK_SIZE 4 << 20
typedef unsigned char BYTE;

#include <stdio.h>      /* fprintf */
#include <stdlib.h>     /* malloc, free */
#include <pthread.h>    /* pthread functions */
#include <string.h>     /* memset */
#include "zstd.h"



typedef struct {
    void* start;
    size_t size;
} buffer_t;

typedef struct {
    buffer_t src;
    buffer_t dst;
    unsigned compressionLevel;
    unsigned jobID;
    unsigned jobCompleted;
    unsigned jobReady;
    pthread_mutex_t* jobCompleted_mutex;
    pthread_cond_t* jobCompleted_cond;
    pthread_mutex_t* jobReady_mutex;
    pthread_cond_t* jobReady_cond;
    size_t compressedSize;
} jobDescription;

typedef struct {
    unsigned compressionLevel;
    unsigned numActiveThreads;
    unsigned numJobs;
    unsigned nextJobID;
    unsigned threadError;
    pthread_mutex_t jobCompleted_mutex;
    pthread_cond_t jobCompleted_cond;
    pthread_mutex_t jobReady_mutex;
    pthread_cond_t jobReady_cond;
    jobDescription* jobs;
    FILE* dstFile;
} adaptCCtx;

static adaptCCtx* createCCtx(unsigned numJobs, const char* const outFilename)
{
    adaptCCtx* ctx = malloc(sizeof(adaptCCtx));
    memset(ctx, 0, sizeof(adaptCCtx));
    ctx->compressionLevel = 6; /* default */
    pthread_mutex_init(&ctx->jobCompleted_mutex, NULL);
    pthread_cond_init(&ctx->jobCompleted_cond, NULL);
    pthread_mutex_init(&ctx->jobReady_mutex, NULL);
    pthread_cond_init(&ctx->jobReady_cond, NULL);
    ctx->numJobs = numJobs;
    ctx->jobs = malloc(numJobs*sizeof(jobDescription));
    ctx->nextJobID = 0;
    ctx->threadError = 0;
    if (!ctx->jobs) {
        DISPLAY("Error: could not allocate space for jobs during context creation\n");
        return NULL;
    }
    {
        FILE* dstFile = fopen(outFilename, "wb");
        if (dstFile == NULL) {
            DISPLAY("Error: could not open output file\n");
            return NULL;
        }
        ctx->dstFile = dstFile;
    }
    return ctx;
}

static void freeCompressionJobs(adaptCCtx* ctx)
{
    unsigned u;
    for (u=0; u<ctx->numJobs; u++) {
        jobDescription job = ctx->jobs[u];
        if (job.dst.start) free(job.dst.start);
        if (job.src.start) free(job.src.start);
    }
}

static int freeCCtx(adaptCCtx* ctx)
{
    int const completedMutexError = pthread_mutex_destroy(&ctx->jobCompleted_mutex);
    int const completedCondError = pthread_cond_destroy(&ctx->jobCompleted_cond);
    int const readyMutexError = pthread_mutex_destroy(&ctx->jobReady_mutex);
    int const readyCondError = pthread_cond_destroy(&ctx->jobReady_cond);
    int const fileError =  fclose(ctx->dstFile);
    freeCompressionJobs(ctx);
    free(ctx->jobs);
    return completedMutexError | completedCondError | readyMutexError | readyCondError | fileError;
}

static void* compressionThread(void* arg)
{
    adaptCCtx* ctx = (adaptCCtx*)arg;
    unsigned currJob = 0;
    for ( ; ; ) {
        jobDescription* job = &ctx->jobs[currJob];
        pthread_mutex_lock(job->jobReady_mutex);
        while(job->jobReady == 0) {
            pthread_cond_wait(job->jobReady_cond, job->jobReady_mutex);
        }
        pthread_mutex_unlock(job->jobReady_mutex);

        /* compress the data */
        {
            size_t const compressedSize = ZSTD_compress(job->dst.start, job->dst.size, job->src.start, job->src.size, job->compressionLevel);
            if (ZSTD_isError(compressedSize)) {
                ctx->threadError = 1;
                DISPLAY("Error: somethign went wrong during compression\n");
                return arg;
            }
            job->compressedSize = compressedSize;
        }
        currJob++;
        if (currJob >= ctx->numJobs || ctx->threadError) {
            /* finished compressing all jobs */
            break;
        }
    }
    return arg;
}

static void* outputThread(void* arg)
{
    adaptCCtx* ctx = (adaptCCtx*)arg;
    unsigned currJob = 0;
    for ( ; ; ) {
        jobDescription* job = &ctx->jobs[currJob];
        pthread_mutex_lock(job->jobCompleted_mutex);
        while (job->jobCompleted == 0) {
            pthread_cond_wait(job->jobCompleted_cond, job->jobCompleted_mutex);
        }
        pthread_mutex_unlock(job->jobCompleted_mutex);
        {
            size_t const compressedSize = job->compressedSize;
            if (ZSTD_isError(compressedSize)) {
                DISPLAY("Error: an error occurred during compression\n");
                return arg; /* TODO: return something else if error */
            }
            {
                size_t const writeSize = fwrite(ctx->jobs[currJob].dst.start, 1, compressedSize, ctx->dstFile);
                if (writeSize != compressedSize) {
                    DISPLAY("Error: an error occurred during file write operation\n");
                    return arg; /* TODO: return something else if error */
                }
            }
        }
        currJob++;
        if (currJob >= ctx->numJobs || ctx->threadError) {
            /* finished with all jobs */
            break;
        }
    }
    return arg;
}


static size_t getFileSize(const char* const filename)
{
    FILE* fd = fopen(filename, "rb");
    if (fd == NULL) {
        DISPLAY("Error: could not open file in order to get file size\n");
        return -1; /* intentional underflow */
    }
    if (fseek(fd, 0, SEEK_END) != 0) {
        DISPLAY("Error: fseek failed during file size computation\n");
        return -1;
    }
    {
        size_t const fileSize = ftell(fd);
        if (fclose(fd) != 0) {
            DISPLAY("Error: could not close file during file size computation\n");
            return -1;
        }
        return fileSize;
    }
}

static int createCompressionJob(adaptCCtx* ctx, BYTE* data, size_t srcSize)
{
    unsigned const nextJob = ctx->nextJobID;
    jobDescription job = ctx->jobs[nextJob];
    job.compressionLevel = ctx->compressionLevel;
    job.src.start = malloc(srcSize);
    job.src.size = srcSize;
    job.dst.size = ZSTD_compressBound(srcSize);
    job.dst.start = malloc(job.dst.size);
    job.jobCompleted = 0;
    job.jobCompleted_cond = &ctx->jobCompleted_cond;
    job.jobCompleted_mutex = &ctx->jobCompleted_mutex;
    job.jobReady_cond = &ctx->jobReady_cond;
    job.jobReady_mutex = &ctx->jobReady_mutex;
    job.jobID = nextJob;
    if (!job.src.start || !job.dst.start) {
        /* problem occurred, free things then return */
        if (job.src.start) free(job.src.start);
        if (job.dst.start) free(job.dst.start);
        return 1;
    }
    memcpy(job.src.start, data, srcSize);
    ctx->nextJobID++;
    return 0;
}

/* return 0 if successful, else return error */
int main(int argCount, const char* argv[])
{
    const char* const srcFilename = argv[1];
    const char* const dstFilename = argv[2];
    BYTE* const src = malloc(FILE_CHUNK_SIZE);
    FILE* const srcFile = fopen(srcFilename, "rb");
    size_t fileSize = getFileSize(srcFilename);
    size_t const numJobsPrelim = (fileSize / FILE_CHUNK_SIZE) + 1;
    size_t const numJobs = (numJobsPrelim * FILE_CHUNK_SIZE) == fileSize ? numJobsPrelim : numJobsPrelim + 1;
    int ret = 0;
    adaptCCtx* ctx = NULL;


    /* checking for errors */
    if (fileSize == (size_t)(-1)) {
        ret = 1;
        goto cleanup;
    }
    if (!srcFilename || !dstFilename || !src) {
        DISPLAY("Error: initial variables could not be allocated\n");
        ret = 1;
         goto cleanup;
    }

    /* creating context */
    ctx = createCCtx(numJobs, dstFilename);
    if (ctx == NULL) {
        ret = 1;
        goto cleanup;
    }

    /* create output thread */
    {
        pthread_t out;
        if (pthread_create(&out, NULL, &outputThread, ctx)) {
            DISPLAY("Error: could not create output thread\n");
            ret = 1;
            goto cleanup;
        }
    }
    /* create compression thread */
    {
        pthread_t compression;
        if (pthread_create(&compression, NULL, &compressionThread, ctx)) {
            DISPLAY("Error: could not create compression thread\n");
            ret = 1;
            goto cleanup;
        }
    }

    /* creating jobs */
    for ( ; ; ) {
        size_t const readSize = fread(src, 1, FILE_CHUNK_SIZE, srcFile);
        if (readSize != FILE_CHUNK_SIZE && !feof(srcFile)) {
            DISPLAY("Error: problem occurred during read from src file\n");
            ret = 1;
            goto cleanup;
        }

        /* reading was fine, now create the compression job */
        {
            int const error = createCompressionJob(ctx, src, readSize);
            if (error != 0) {
                ret = error;
                goto cleanup;
            }
        }
    }

    /* file compression completed */
    {
        int const fileCloseError = fclose(srcFile);
        int const cctxReleaseError = freeCCtx(ctx);
        if (fileCloseError | cctxReleaseError) {
            ret = 1;
            goto cleanup;
        }
    }
cleanup:
    if (src != NULL) free(src);
    if (ctx != NULL) freeCCtx(ctx);
    return ret;
}
