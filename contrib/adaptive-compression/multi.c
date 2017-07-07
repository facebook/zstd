#define DISPLAY(...) fprintf(stderr, __VA_ARGS__)
#define DEBUGLOG(l, ...) { if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); } }
#define FILE_CHUNK_SIZE 4 << 20
#define MAX_NUM_JOBS 2;
#define stdinmark  "/*stdin*\\"
#define stdoutmark "/*stdout*\\"
#define MAX_PATH 256
#define DEFAULT_DISPLAY_LEVEL 1
#define DEFAULT_COMPRESSION_LEVEL 6
#define DEFAULT_ADAPT_PARAM 2
typedef unsigned char BYTE;

#include <stdio.h>      /* fprintf */
#include <stdlib.h>     /* malloc, free */
#include <pthread.h>    /* pthread functions */
#include <string.h>     /* memset */
#include <time.h>       /* clock(), CLOCKS_PER_SEC */
#include "zstd.h"

static int g_displayLevel = DEFAULT_DISPLAY_LEVEL;
static unsigned g_compressionLevel = DEFAULT_COMPRESSION_LEVEL;
static unsigned g_displayStats = 0;
static clock_t g_time = 0;
static clock_t const refreshRate = CLOCKS_PER_SEC / 60; /* 60 Hz */

typedef struct {
    void* start;
    size_t size;
} buffer_t;

typedef struct {
    unsigned waitCompleted;
    unsigned waitReady;
    unsigned waitWrite;
    unsigned readyCounter;
    unsigned completedCounter;
    unsigned writeCounter;
} stat_t;

typedef struct {
    buffer_t src;
    buffer_t dst;
    unsigned compressionLevel;
    unsigned jobID;
    size_t compressedSize;
} jobDescription;

typedef struct {
    unsigned compressionLevel;
    unsigned numActiveThreads;
    unsigned numJobs;
    unsigned lastJobID;
    unsigned nextJobID;
    unsigned threadError;
    unsigned jobReadyID;
    unsigned jobCompletedID;
    unsigned jobWriteID;
    unsigned allJobsCompleted;
    unsigned adaptParam;
    pthread_mutex_t jobCompleted_mutex;
    pthread_cond_t jobCompleted_cond;
    pthread_mutex_t jobReady_mutex;
    pthread_cond_t jobReady_cond;
    pthread_mutex_t allJobsCompleted_mutex;
    pthread_cond_t allJobsCompleted_cond;
    pthread_mutex_t jobWrite_mutex;
    pthread_cond_t jobWrite_cond;
    stat_t stats;
    jobDescription* jobs;
    FILE* dstFile;
} adaptCCtx;

static void freeCompressionJobs(adaptCCtx* ctx)
{
    unsigned u;
    for (u=0; u<ctx->numJobs; u++) {
        jobDescription job = ctx->jobs[u];
        free(job.dst.start);
        free(job.src.start);
    }
}

static int freeCCtx(adaptCCtx* ctx)
{
    {
        int const completedMutexError = pthread_mutex_destroy(&ctx->jobCompleted_mutex);
        int const completedCondError = pthread_cond_destroy(&ctx->jobCompleted_cond);
        int const readyMutexError = pthread_mutex_destroy(&ctx->jobReady_mutex);
        int const readyCondError = pthread_cond_destroy(&ctx->jobReady_cond);
        int const allJobsMutexError = pthread_mutex_destroy(&ctx->allJobsCompleted_mutex);
        int const allJobsCondError = pthread_cond_destroy(&ctx->allJobsCompleted_cond);
        int const jobWriteMutexError = pthread_mutex_destroy(&ctx->jobWrite_mutex);
        int const jobWriteCondError = pthread_cond_destroy(&ctx->jobWrite_cond);
        int const fileCloseError =  ctx->dstFile != NULL ? fclose(ctx->dstFile) : 0;
        if (ctx->jobs){
            freeCompressionJobs(ctx);
            free(ctx->jobs);
        }
        return completedMutexError | completedCondError | readyMutexError | readyCondError | fileCloseError | allJobsMutexError | allJobsCondError | jobWriteMutexError | jobWriteCondError;
    }
}

static adaptCCtx* createCCtx(unsigned numJobs, const char* const outFilename)
{

    adaptCCtx* ctx = malloc(sizeof(adaptCCtx));
    if (ctx == NULL) {
        DISPLAY("Error: could not allocate space for context\n");
        return NULL;
    }
    memset(ctx, 0, sizeof(adaptCCtx));
    ctx->compressionLevel = g_compressionLevel;
    pthread_mutex_init(&ctx->jobCompleted_mutex, NULL); /* TODO: add checks for errors on each mutex */
    pthread_cond_init(&ctx->jobCompleted_cond, NULL);
    pthread_mutex_init(&ctx->jobReady_mutex, NULL);
    pthread_cond_init(&ctx->jobReady_cond, NULL);
    pthread_mutex_init(&ctx->allJobsCompleted_mutex, NULL);
    pthread_cond_init(&ctx->allJobsCompleted_cond, NULL);
    pthread_mutex_init(&ctx->jobWrite_mutex, NULL);
    pthread_cond_init(&ctx->jobWrite_cond, NULL);
    ctx->numJobs = numJobs;
    ctx->jobReadyID = 0;
    ctx->jobCompletedID = 0;
    ctx->jobWriteID = 0;
    ctx->lastJobID = -1; /* intentional underflow */
    ctx->jobs = calloc(1, numJobs*sizeof(jobDescription));
    ctx->nextJobID = 0;
    ctx->threadError = 0;
    ctx->allJobsCompleted = 0;
    ctx->adaptParam = DEFAULT_ADAPT_PARAM;
    if (!ctx->jobs) {
        DISPLAY("Error: could not allocate space for jobs during context creation\n");
        freeCCtx(ctx);
        return NULL;
    }
    {
        unsigned const stdoutUsed = !strcmp(outFilename, stdoutmark);
        FILE* dstFile = stdoutUsed ? stdout : fopen(outFilename, "wb");
        if (dstFile == NULL) {
            DISPLAY("Error: could not open output file\n");
            freeCCtx(ctx);
            return NULL;
        }
        ctx->dstFile = dstFile;
    }
    return ctx;
}



static void waitUntilAllJobsCompleted(adaptCCtx* ctx)
{
    pthread_mutex_lock(&ctx->allJobsCompleted_mutex);
    while (ctx->allJobsCompleted == 0) {
        pthread_cond_wait(&ctx->allJobsCompleted_cond, &ctx->allJobsCompleted_mutex);
    }
    pthread_mutex_unlock(&ctx->allJobsCompleted_mutex);
}

static unsigned adaptCompressionLevel(adaptCCtx* ctx)
{
    unsigned reset = 0;
    unsigned const allSlow = ctx->adaptParam < ctx->stats.completedCounter && ctx->adaptParam < ctx->stats.writeCounter && ctx->adaptParam < ctx->stats.readyCounter ? 1 : 0;
    unsigned const compressWaiting = ctx->adaptParam < ctx->stats.readyCounter ? 1 : 0;
    unsigned const writeWaiting = ctx->adaptParam < ctx->stats.completedCounter ? 1 : 0;
    unsigned const createWaiting = ctx->adaptParam < ctx->stats.writeCounter ? 1 : 0;
    unsigned const writeSlow = ((compressWaiting && createWaiting) || (createWaiting && !writeWaiting)) ? 1 : 0;
    unsigned const compressSlow = ((writeWaiting && createWaiting) || (writeWaiting && !compressWaiting)) ? 1 : 0;
    unsigned const createSlow = ((compressWaiting && writeWaiting) || (compressWaiting && !createWaiting)) ? 1 : 0;
    // unsigned const writeSlow = ((compressWaiting && createWaiting)) ? 1 : 0;
    // unsigned const compressSlow = ((writeWaiting && createWaiting)) ? 1 : 0;
    // unsigned const createSlow = ((compressWaiting && writeWaiting)) ? 1 : 0;
    DEBUGLOG(2, "ready: %u completed: %u write: %u\n", ctx->stats.readyCounter, ctx->stats.completedCounter, ctx->stats.writeCounter);
    if (allSlow) {
        reset = 1;
    }
    else if ((writeSlow || createSlow) && ctx->compressionLevel < (unsigned)ZSTD_maxCLevel()) {
        DEBUGLOG(2, "increasing compression level %u\n", ctx->compressionLevel);
        ctx->compressionLevel++;
        reset = 1;
    }
    else if (compressSlow && ctx->compressionLevel > 1) {
        DEBUGLOG(2, "decreasing compression level %u\n", ctx->compressionLevel);
        ctx->compressionLevel--;
        reset = 1;
    }
    if (reset) {
        ctx->stats.readyCounter = 0;
        ctx->stats.writeCounter = 0;
        ctx->stats.completedCounter = 0;
    }
    return ctx->compressionLevel;
}

static void* compressionThread(void* arg)
{
    adaptCCtx* ctx = (adaptCCtx*)arg;
    unsigned currJob = 0;
    for ( ; ; ) {
        unsigned const currJobIndex = currJob % ctx->numJobs;
        jobDescription* job = &ctx->jobs[currJobIndex];
        // DEBUGLOG(2, "compressionThread(): waiting on job ready\n");
        pthread_mutex_lock(&ctx->jobReady_mutex);
        while(currJob + 1 > ctx->jobReadyID) {
            ctx->stats.waitReady++;
            ctx->stats.readyCounter++;
            DEBUGLOG(2, "waiting on job ready, nextJob: %u\n", currJob);
            pthread_cond_wait(&ctx->jobReady_cond, &ctx->jobReady_mutex);
        }
        pthread_mutex_unlock(&ctx->jobReady_mutex);
        // DEBUGLOG(2, "compressionThread(): continuing after job ready\n");
        /* compress the data */
        {
            unsigned const cLevel = adaptCompressionLevel(ctx);
            // unsigned const cLevel = job->compressionLevel;
            DEBUGLOG(2, "cLevel used: %u\n", cLevel);
            size_t const compressedSize = ZSTD_compress(job->dst.start, job->dst.size, job->src.start, job->src.size, cLevel);
            if (ZSTD_isError(compressedSize)) {
                ctx->threadError = 1;
                DISPLAY("Error: something went wrong during compression: %s\n", ZSTD_getErrorName(compressedSize));
                return arg;
            }
            job->compressedSize = compressedSize;
        }
        pthread_mutex_lock(&ctx->jobCompleted_mutex);
        ctx->jobCompletedID++;
        DEBUGLOG(2, "signaling for job %u\n", currJob);
        pthread_cond_signal(&ctx->jobCompleted_cond);
        pthread_mutex_unlock(&ctx->jobCompleted_mutex);
        DEBUGLOG(2, "finished job compression %u\n", currJob);
        currJob++;
        if (currJob >= ctx->lastJobID || ctx->threadError) {
            /* finished compressing all jobs */
            DEBUGLOG(2, "all jobs finished compressing\n");
            break;
        }
    }
    return arg;
}

static void displayProgress(unsigned jobDoneID)
{
    clock_t currTime = clock();
    unsigned const refresh = currTime - g_time > refreshRate ? 1 : 0;
    if (refresh) {
        fprintf(stdout, "%u jobs completed\r", jobDoneID+1);
        fflush(stdout);
    }
}

static void* outputThread(void* arg)
{
    adaptCCtx* ctx = (adaptCCtx*)arg;

    unsigned currJob = 0;
    for ( ; ; ) {
        unsigned const currJobIndex = currJob % ctx->numJobs;
        jobDescription* job = &ctx->jobs[currJobIndex];
        // DEBUGLOG(2, "outputThread(): waiting on job completed\n");
        pthread_mutex_lock(&ctx->jobCompleted_mutex);
        while (currJob + 1 > ctx->jobCompletedID) {
            ctx->stats.waitCompleted++;
            ctx->stats.completedCounter++;
            DEBUGLOG(2, "waiting on job completed, nextJob: %u\n", currJob);
            pthread_cond_wait(&ctx->jobCompleted_cond, &ctx->jobCompleted_mutex);
        }
        pthread_mutex_unlock(&ctx->jobCompleted_mutex);
        // DEBUGLOG(2, "outputThread(): continuing after job completed\n");
        {
            size_t const compressedSize = job->compressedSize;
            if (ZSTD_isError(compressedSize)) {
                DISPLAY("Error: an error occurred during compression\n");
                return arg;
            }
            {
                size_t const writeSize = fwrite(job->dst.start, 1, compressedSize, ctx->dstFile);
                if (writeSize != compressedSize) {
                    DISPLAY("Error: an error occurred during file write operation\n");
                    return arg;
                }
            }
        }
        DEBUGLOG(2, "finished job write %u\n", currJob);
        displayProgress(currJob);
        currJob++;
        DEBUGLOG(2, "locking job write mutex\n");
        pthread_mutex_lock(&ctx->jobWrite_mutex);
        ctx->jobWriteID++;
        pthread_cond_signal(&ctx->jobWrite_cond);
        pthread_mutex_unlock(&ctx->jobWrite_mutex);
        DEBUGLOG(2, "unlocking job write mutex\n");

        DEBUGLOG(2, "checking if done: %u/%u\n", currJob, ctx->lastJobID);
        if (currJob >= ctx->lastJobID || ctx->threadError) {
            /* finished with all jobs */
            DEBUGLOG(2, "all jobs finished writing\n");
            pthread_mutex_lock(&ctx->allJobsCompleted_mutex);
            ctx->allJobsCompleted = 1;
            pthread_cond_signal(&ctx->allJobsCompleted_cond);
            pthread_mutex_unlock(&ctx->allJobsCompleted_mutex);
            break;
        }
    }
    return arg;
}

static int createCompressionJob(adaptCCtx* ctx, BYTE* data, size_t srcSize)
{
    unsigned const nextJob = ctx->nextJobID;
    unsigned const nextJobIndex = nextJob % ctx->numJobs;
    jobDescription* job = &ctx->jobs[nextJobIndex];
    // DEBUGLOG(2, "createCompressionJob(): wait for job write\n");
    pthread_mutex_lock(&ctx->jobWrite_mutex);
    // DEBUGLOG(2, "Creating new compression job -- nextJob: %u, jobCompletedID: %u, jobWriteID: %u, numJObs: %u\n", nextJob,ctx->jobCompletedID, ctx->jobWriteID, ctx->numJobs);
    while (nextJob - ctx->jobWriteID >= ctx->numJobs) {
        ctx->stats.waitWrite++;
        ctx->stats.writeCounter++;
        DEBUGLOG(2, "waiting on job Write, nextJob: %u\n", nextJob);
        pthread_cond_wait(&ctx->jobWrite_cond, &ctx->jobWrite_mutex);
    }
    pthread_mutex_unlock(&ctx->jobWrite_mutex);
    // DEBUGLOG(2, "createCompressionJob(): continuing after job write\n");


    job->compressionLevel = ctx->compressionLevel;
    job->src.start = malloc(srcSize);
    job->src.size = srcSize;
    job->dst.size = ZSTD_compressBound(srcSize);
    job->dst.start = malloc(job->dst.size);
    job->jobID = nextJob;
    if (!job->src.start || !job->dst.start) {
        /* problem occurred, free things then return */
        DISPLAY("Error: problem occurred during job creation\n");
        free(job->src.start);
        free(job->dst.start);
        return 1;
    }
    memcpy(job->src.start, data, srcSize);
    pthread_mutex_lock(&ctx->jobReady_mutex);
    ctx->jobReadyID++;
    pthread_cond_signal(&ctx->jobReady_cond);
    pthread_mutex_unlock(&ctx->jobReady_mutex);
    DEBUGLOG(2, "finished job creation %u\n", nextJob);
    ctx->nextJobID++;
    return 0;
}

static void printStats(stat_t stats)
{
    DISPLAY("========STATISTICS========\n");
    DISPLAY("# times waited on job ready: %u\n", stats.waitReady);
    DISPLAY("# times waited on job completed: %u\n", stats.waitCompleted);
    DISPLAY("# times waited on job Write: %u\n\n", stats.waitWrite);
}

static int compressFilename(const char* const srcFilename, const char* const dstFilename)
{
    BYTE* const src = malloc(FILE_CHUNK_SIZE);
    unsigned const stdinUsed = !strcmp(srcFilename, stdinmark);
    FILE* const srcFile = stdinUsed ? stdin : fopen(srcFilename, "rb");
    const char* const outFilename = (stdinUsed && !dstFilename) ? stdoutmark : dstFilename;
    size_t const numJobs = MAX_NUM_JOBS;
    int ret = 0;
    adaptCCtx* ctx = NULL;
    g_time = clock();


    /* checking for errors */
    if (!srcFilename || !outFilename || !src || !srcFile) {
        DISPLAY("Error: initial variables could not be allocated\n");
        ret = 1;
        goto cleanup;
    }

    /* creating context */
    ctx = createCCtx(numJobs, outFilename);
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
        if (feof(srcFile)) {
            DEBUGLOG(2, "THE STREAM OF DATA ENDED %u\n", ctx->nextJobID);
            ctx->lastJobID = ctx->nextJobID;
            break;
        }
    }

cleanup:
    waitUntilAllJobsCompleted(ctx);
    if (g_displayStats) printStats(ctx->stats);
    /* file compression completed */
    ret  |= (srcFile != NULL) ? fclose(srcFile) : 0;
    ret |= (ctx != NULL) ? freeCCtx(ctx) : 0;
    free(src);
    return ret;
}

static int compressFilenames(const char** filenameTable, unsigned numFiles)
{
    int ret = 0;
    unsigned fileNum;
    char outFile[MAX_PATH];
    for (fileNum=0; fileNum<numFiles; fileNum++) {
        const char* filename = filenameTable[fileNum];
        if (snprintf(outFile, MAX_PATH, "%s.zst", filename) + 1 > MAX_PATH) {
            DISPLAY("Error: output filename is too long\n");
            return 1;
        }
        ret |= compressFilename(filename, outFile);
    }
    return ret;
}

/*! readU32FromChar() :
    @return : unsigned integer value read from input in `char` format
    allows and interprets K, KB, KiB, M, MB and MiB suffix.
    Will also modify `*stringPtr`, advancing it to position where it stopped reading.
    Note : function result can overflow if digit string > MAX_UINT */
static unsigned readU32FromChar(const char** stringPtr)
{
    unsigned result = 0;
    while ((**stringPtr >='0') && (**stringPtr <='9'))
        result *= 10, result += **stringPtr - '0', (*stringPtr)++ ;
    if ((**stringPtr=='K') || (**stringPtr=='M')) {
        result <<= 10;
        if (**stringPtr=='M') result <<= 10;
        (*stringPtr)++ ;
        if (**stringPtr=='i') (*stringPtr)++;
        if (**stringPtr=='B') (*stringPtr)++;
    }
    return result;
}

/* return 0 if successful, else return error */
int main(int argCount, const char* argv[])
{
    const char* outFilename = NULL;
    const char** filenameTable = (const char**)malloc(argCount*sizeof(const char*));
    unsigned filenameIdx = 0;
    filenameTable[0] = stdinmark;
    int ret = 0;
    int argNum;

    if (filenameTable == NULL) {
        DISPLAY("Error: could not allocate sapce for filename table.\n");
        return 1;
    }

    for (argNum=1; argNum<argCount; argNum++) {
        const char* argument = argv[argNum];

        /* output filename designated with "-o" */
        if (argument[0]=='-') {
            if (strlen(argument) > 1 && argument[1] == 'o') {
                argument += 2;
                outFilename = argument;
                continue;
            }
            else if (strlen(argument) > 1 && argument[1] == 'v') {
                g_displayLevel++;
                continue;
            }
            else if (strlen(argument) > 1 && argument[1] == 'i') {
                argument += 2;
                g_compressionLevel = readU32FromChar(&argument);
                DEBUGLOG(2, "g_compressionLevel: %u\n", g_compressionLevel);
                continue;
            }
            else if (strlen(argument) > 1 && argument[1] == 's') {
                g_displayStats = 1;
                continue;
            }
            else {
                DISPLAY("Error: invalid argument provided\n");
                ret = 1;
                goto _main_exit;
            }
        }

        /* regular files to be compressed */
        filenameTable[filenameIdx++] = argument;
    }

    /* error checking with number of files */
    if (filenameIdx > 1 && outFilename != NULL) {
        DISPLAY("Error: multiple input files provided, cannot use specified output file\n");
        ret = 1;
        goto _main_exit;
    }

    /* compress files */
    if (filenameIdx <= 1) {
        ret |= compressFilename(filenameTable[0], outFilename);
    }
    else {
        ret |= compressFilenames(filenameTable, filenameIdx);
    }
_main_exit:
    free(filenameTable);
    return ret;
}
