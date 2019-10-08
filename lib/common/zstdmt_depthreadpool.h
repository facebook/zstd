#ifndef ZSTDMT_DEPTHREADPOOL_H_MODULE
#define ZSTDMT_DEPTHREADPOOL_H_MODULE

#include <stddef.h>

/***************************************
* Definitions
***************************************/

typedef void (*ZSTDMT_depThreadPoolFn)(void* data);                          /* Job callback function */
typedef struct ZSTDMT_depThreadPoolJob_s ZSTDMT_depThreadPoolJob_t;          /* Job management struct */
typedef struct ZSTDMT_depThreadPoolThread_s ZSTDMT_depThreadPoolThread_t;    /* Thread managedment struct */
typedef struct ZSTDMT_depThreadPoolCtx_s ZSTDMT_depThreadPoolCtx_t;          /* Context (one per thread pool) */

/***************************************
* ZSTDMT_depThreadPool API
***************************************/

/* Basic usage:
 * ZSTDMT_depThreadPoolCtx_t* ctx = ZSTDMT_depThreadPool_createCtx(2, 2);
 * ZSTDMT_depThreadPool_addJob(ctx, job1, NULL, 0, NULL);
 * ZSTDMT_depThreadPool_addJob(ctx, job2, NULL, 1, arrayWithJob1Id);
 * ZSTDMT_depThreadPool_destroyCtx(ctx); */

/* Create a context supporting 'maxNbJobs' and 'maxNbThreads'
 * 'maxNbJobs' is the total number of jobs you expect during any usage
 * Note: the threads won't exit until 'maxNbJobs' have been completed!
 * @return: ptr to the created context */
ZSTDMT_depThreadPoolCtx_t* ZSTDMT_depThreadPool_createCtx(size_t maxNbJobs, size_t maxNbThreads);

/* Wait until all the jobs queued finish and then frees resources
 * Destroys all mutexes and cond variables used */
void ZSTDMT_depThreadPool_destroyCtx(ZSTDMT_depThreadPoolCtx_t* ctx);

/* Adds a job with potential dependencies to the thread pool
 * @fn: This is the callback function
 * @data: Any args that you want to pass to your callback function
 * @nbDeps: Number of dependencies
 * @depJobIds: The dependencies
 * Note: This only supports having jobs with one dependency.
 * TODO?: Might need to extend this to multiple dependencies but not right now
 * @return: The job id of the newly added job. */
size_t ZSTDMT_depThreadPool_addJob(ZSTDMT_depThreadPoolCtx_t* ctx, ZSTDMT_depThreadPoolFn fn, void* data, size_t nbDeps, size_t* depJobIds);

#endif /* ZSTDMT_DEPTHREADPOOL_H_MODULE */
