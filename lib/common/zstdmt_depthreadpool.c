#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "threading.h"
#include "zstdmt_depthreadpool.h"

#ifdef ZSTD_MULTITHREAD

#define err_abort(code,text) do { \
	fprintf (stderr, "%s at \"%s\":%d: %s\n", \
		text, __FILE__, __LINE__, strerror (code)); \
	abort (); \
} while (0)

/***************************************
* Definitions
***************************************/

struct ZSTDMT_DepThreadPoolJob_s {
	ZSTDMT_depThreadPoolFn fn;
	void* data;
	size_t nbDeps;
	size_t* depJobIds;
	int started;
	int finished;
};

struct ZSTDMT_DepThreadPoolThread_s {
	ZSTD_pthread_mutex_t mutex;
	ZSTD_pthread_t thread;
	ZSTDMT_DepThreadPoolJob* job;
	ZSTDMT_DepThreadPoolCtx* ctx;
};

struct ZSTDMT_DepThreadPoolCtx_s {
	ZSTD_pthread_mutex_t mutex;
	ZSTD_pthread_cond_t cond;
	ZSTDMT_DepThreadPoolThread** threads;
	size_t maxNbThreads;
	ZSTDMT_DepThreadPoolJob** jobs;
	size_t nbJobs;
	size_t nbJobsRemaining;
};

/***************************************
* ZSTDMT_depThreadPool internal
***************************************/

/* This just linearly searches the array of jobs for ANY
 * job that hasn't already been started and either has
 * no dependencies or has its dependency satisfied */
static ZSTDMT_DepThreadPoolJob* ZSTDMT_getNextJob(ZSTDMT_DepThreadPoolCtx* ctx)
{
	ZSTDMT_DepThreadPoolJob* job = NULL;
	size_t i;
	size_t depsSatisfied;
	size_t j;

	for (i = 0; i < ctx->nbJobs; ++i) {
		job = ctx->jobs[i];
		if (job && !job->started){
			if (job->nbDeps == 0) break;
			else {
				for (j = 0, depsSatisfied = 1; j < job->nbDeps; ++j) {depsSatisfied &= ctx->jobs[job->depJobIds[j]]->finished;}
				if (depsSatisfied) break;
			}
		}
		job = NULL;
	}
	if (job) job->started = 1;

	return job;
}

/* This is the main worker routine. Basically, it waits for a job to become
 * ready for execution by calling ZSTDMT_getNextJob(...) and then runs it!
 * It should only exit when all jobs have finished running */
static void* ZSTDMT_threadRoutine(void* data)
{
	ZSTDMT_DepThreadPoolThread* thread = (ZSTDMT_DepThreadPoolThread*)data;
	ZSTDMT_DepThreadPoolCtx* ctx = thread->ctx;

	ZSTD_pthread_mutex_lock(&thread->mutex);
	thread->job = NULL;
	while (1) {
		ZSTD_pthread_mutex_lock(&ctx->mutex);
		while (ctx->nbJobsRemaining && (thread->job = ZSTDMT_getNextJob(ctx)) == NULL)
			ZSTD_pthread_cond_wait(&ctx->cond, &ctx->mutex);
		if (!ctx->nbJobsRemaining) break;
		ZSTD_pthread_mutex_unlock(&ctx->mutex);

		thread->job->fn(thread->job->data);
		thread->job->finished = 1;
		free(thread->job->depJobIds);
		free(thread->job);
		thread->job = NULL;

		ZSTD_pthread_mutex_lock(&ctx->mutex);
		ctx->nbJobsRemaining--;
		if (!ctx->nbJobsRemaining) break;
		ZSTD_pthread_cond_signal(&ctx->cond);
		ZSTD_pthread_mutex_unlock(&ctx->mutex);
	}
	ZSTD_pthread_cond_signal(&ctx->cond);
	ZSTD_pthread_mutex_unlock(&ctx->mutex);
	ZSTD_pthread_mutex_unlock(&thread->mutex);

	return NULL;
}

/***************************************
* ZSTDMT_depThreadPool API
***************************************/

ZSTDMT_DepThreadPoolCtx* ZSTDMT_depThreadPool_createCtx(size_t maxNbJobs, size_t maxNbThreads)
{
	ZSTDMT_DepThreadPoolCtx* ctx = (ZSTDMT_DepThreadPoolCtx*)malloc(sizeof(ZSTDMT_DepThreadPoolCtx));
	size_t i;

	if (ctx == NULL) err_abort(-1, "Malloc ctx");

	if (ZSTD_pthread_mutex_init(&ctx->mutex, NULL)) err_abort(-1, "Ctx mutex init");
	if (ZSTD_pthread_cond_init(&ctx->cond, NULL)) err_abort(-1, "Cond init");

	ctx->nbJobs = 0;
	ctx->nbJobsRemaining = maxNbJobs;
	ctx->maxNbThreads = maxNbThreads;
	ctx->threads = (ZSTDMT_DepThreadPoolThread**)malloc(sizeof(ZSTDMT_DepThreadPoolThread*) * ctx->maxNbThreads);
	if (ctx->threads == NULL) err_abort(-1, "Malloc threads");
	ctx->jobs = (ZSTDMT_DepThreadPoolJob**)malloc(sizeof(ZSTDMT_DepThreadPoolJob*) * ctx->nbJobsRemaining);
	if (ctx->jobs == NULL) err_abort(-1, "Malloc jobs");
	for (i = 0; i < ctx->maxNbThreads; ++i) {
		ctx->threads[i] = (ZSTDMT_DepThreadPoolThread*)malloc(sizeof(ZSTDMT_DepThreadPoolThread));
		if (ctx->threads[i] == NULL) err_abort(-1, "Malloc thread");
		ctx->threads[i]->ctx = ctx;
		ctx->threads[i]->job = NULL;

		if (ZSTD_pthread_mutex_init(&ctx->threads[i]->mutex, NULL)) err_abort(-1, "Thread mutex init");

		ZSTD_pthread_create(&ctx->threads[i]->thread, NULL, ZSTDMT_threadRoutine, ctx->threads[i]);
	}

	return ctx;
}

void ZSTDMT_depThreadPool_destroyCtx(ZSTDMT_DepThreadPoolCtx* ctx)
{
	size_t i;
	for (i = 0; i < ctx->maxNbThreads; ++i) ZSTD_pthread_join(ctx->threads[i]->thread, NULL);
	for (i = 0; i < ctx->maxNbThreads; ++i) {
		ZSTD_pthread_mutex_destroy(&ctx->threads[i]->mutex);
		free(ctx->threads[i]);
	}
	free(ctx->threads);
	free(ctx->jobs);
	ZSTD_pthread_mutex_destroy(&ctx->mutex);
	ZSTD_pthread_cond_destroy(&ctx->cond);
	free(ctx);
}

size_t ZSTDMT_depThreadPool_addJob(ZSTDMT_DepThreadPoolCtx* ctx, ZSTDMT_depThreadPoolFn fn,
	void* data, size_t nbDeps, size_t* depJobIds)
{
	ZSTDMT_DepThreadPoolJob* job = (ZSTDMT_DepThreadPoolJob*)malloc(sizeof(ZSTDMT_DepThreadPoolJob));
	if (job == NULL) err_abort(-1, "Malloc job");
	job->fn = fn;
	job->data = data;
	job->started = 0;
	job->finished = 0;
	job->nbDeps = nbDeps;
	job->depJobIds = (size_t*)malloc(sizeof(size_t) * nbDeps);
	if (job->depJobIds == NULL) err_abort(-1, "Malloc depJobIds");
	memcpy(job->depJobIds, depJobIds, sizeof(size_t) * nbDeps);

	ZSTD_pthread_mutex_lock(&ctx->mutex);
	ctx->jobs[ctx->nbJobs++] = job;
	ZSTD_pthread_cond_signal(&ctx->cond);
	ZSTD_pthread_mutex_unlock(&ctx->mutex);
  return ctx->nbJobs - 1;
}

#endif /* ZSTD_MULTITHREAD */
