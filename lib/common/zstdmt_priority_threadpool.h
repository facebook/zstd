#ifndef ZSTDMT_PRIORITY_THREADPOOL_H_MODULE
#define ZSTDMT_PRIORITY_THREADPOOL_H_MODULE

#include <stddef.h>

/* Priority thread pool context */
typedef struct ZSTDMT_pThreadPoolCtx_s ZSTDMT_pThreadPoolCtx_t;

/* Callback function for each job */
typedef void (*ZSTDMT_pThreadPool_fn)(void* data);

/* Basic Usage:
 * ctx = ZSTDMT_pThreadPoolCtx_create(QUEUE_SIZE, NB_THREADS);
 * for (i = 0; i < nbJobs; ++i)
 *    ZSTDMT_pThreadPool_addJob(ctx, priority(i), 0, callbackFn, NULL);
 * ZSTDMT_pThreadPool_waitAllThreads(ctx);
 * ZSTDMT_pThreadPool_free(ctx); */
/***************************************
* ZSTDMT_pThreadPool API
***************************************/

/* Create a priority thread pool of 'queueSize' using 'nbThreads' worker threads
 * @return : ptr to ZSTDMT_pThreadPoolCtx_t for thread pool */
static ZSTDMT_pThreadPoolCtx_t* ZSTDMT_pThreadPoolCtx_create(size_t queueSize,
  size_t nbThreads);

/* Destroies the context */
static size_t ZSTDMT_pThradPoolCtx_free(ZSTDMT_pThreadPoolCtx_t* ctx);

/* Enqueue a job into the priority queue of the thread pool with given 'priority'
 * Note: time complexity will be log(#jobs)
 * Note: lower priorities are going to be excuted first (ie. priority 0 jobs will come first)
 * @return : ID of the job */
static size_t ZSTDMT_pThreadPool_addJob(ZSTDMT_pThreadPoolCtx_t* ctx, size_t priority,
  size_t addedTimeMs, ZSTDMT_pThreadPool_fn fn, void* data);

/* Dequeue the job from the priority queue of the thread pool if it exists in the queue
 * @return : 1 for success or some error code */
static size_t ZSTDMT_pThreadPool_removeJob(ZSTDMT_pThreadPoolCtx_t* ctx, size_t jobId);

/* @return : the current length of the queue */
static size_t ZSTDMT_pThreadPool_getQueueLength(ZSTDMT_pThreadPoolCtx_t* ctx);

/* @return : 0 if job is already queued, 1 otherwise */
static int ZSTDMT_pThreadPool_isJobQueued(ZSTDMT_pThreadPoolCtx_t* ctx, size_t jobId);

/* @return : 0 if job is running, 1 otherwise */
static int ZSTDMT_pThreadPool_isJobRunning(ZSTDMT_pThreadPoolCtx_t* ctx, size_t jobId);

/* Remove all the non-running jobs in the queue
 * @return : number of jobs removed */
static size_t ZSTDMT_pThreadPool_clearQueue(ZSTDMT_pThreadPoolCtx_t* ctx);

/* Wait for all the jobs to finish (ie. join all the threads to the main one)
 * Note: Will terminate the jobs that are not currently running
 * @return : number of jobs terminated */
static size_t ZSTDMT_pThreadPool_waitAllThreads(ZSTDMT_pThreadPoolCtx_t* ctx);

#endif /* ZSTDMT_PRIORITY_THREADPOOL_H_MODULE */
