#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include "zstdmt_depthreadpool.h"

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
	pthread_mutex_t mutex;
	pthread_t thread;
	ZSTDMT_DepThreadPoolJob* job;
	ZSTDMT_DepThreadPoolCtx* ctx;
};

struct ZSTDMT_DepThreadPoolCtx_s {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
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

static void ZSTDMT_checkStatus(int status, const char* text) {if (status != 0) err_abort(status, text);}

/* This is the main worker routine. Basically, it waits for a job to become
 * ready for execution by calling ZSTDMT_getNextJob(...) and then runs it!
 * It should only exit when all jobs have finished running */
static void* ZSTDMT_threadRoutine(void* data)
{
	ZSTDMT_DepThreadPoolThread* thread = (ZSTDMT_DepThreadPoolThread*)data;
	ZSTDMT_DepThreadPoolCtx* ctx = thread->ctx;

	ZSTDMT_checkStatus(pthread_mutex_lock(&thread->mutex), "Lock thread");
	thread->job = NULL;
	while (1) {
		ZSTDMT_checkStatus(pthread_mutex_lock(&ctx->mutex), "Lock ctx");
		while (ctx->nbJobsRemaining && (thread->job = ZSTDMT_getNextJob(ctx)) == NULL)
			ZSTDMT_checkStatus(pthread_cond_wait(&ctx->cond, &ctx->mutex), "Cond wait");
		if (!ctx->nbJobsRemaining) break;
		ZSTDMT_checkStatus(pthread_mutex_unlock(&ctx->mutex), "Unlock ctx");

		thread->job->fn(thread->job->data);
		thread->job->finished = 1;
		free(thread->job->depJobIds);
		free(thread->job);
		thread->job = NULL;

		ZSTDMT_checkStatus(pthread_mutex_lock(&ctx->mutex), "Lock ctx");
		ctx->nbJobsRemaining--;
		if (!ctx->nbJobsRemaining) break;
		ZSTDMT_checkStatus(pthread_cond_signal(&ctx->cond), "Cond signal");
		ZSTDMT_checkStatus(pthread_mutex_unlock(&ctx->mutex), "Unlock ctx");
	}
	ZSTDMT_checkStatus(pthread_cond_signal(&ctx->cond), "Cond signal");
	ZSTDMT_checkStatus(pthread_mutex_unlock(&ctx->mutex), "Unlock ctx");
	ZSTDMT_checkStatus(pthread_mutex_unlock(&thread->mutex), "Unlock thread");

	return NULL;
}

/***************************************
* ZSTDMT_depThreadPool API
***************************************/

ZSTDMT_DepThreadPoolCtx* ZSTDMT_depThreadPool_createCtx(size_t maxNbJobs, size_t maxNbThreads)
{
	ZSTDMT_DepThreadPoolCtx* ctx = (ZSTDMT_DepThreadPoolCtx*)malloc(sizeof(ZSTDMT_DepThreadPoolCtx));
	size_t i;

	ZSTDMT_checkStatus(pthread_mutex_init(&ctx->mutex, NULL), "Mutex init ctx");
	ZSTDMT_checkStatus(pthread_cond_init(&ctx->cond, NULL), "Cond init ctx");
	ctx->nbJobs = 0;
	ctx->nbJobsRemaining = maxNbJobs;
	ctx->maxNbThreads = maxNbThreads;
	ctx->threads = (ZSTDMT_DepThreadPoolThread**)malloc(sizeof(ZSTDMT_DepThreadPoolThread*) * ctx->maxNbThreads);
	ctx->jobs = (ZSTDMT_DepThreadPoolJob**)malloc(sizeof(ZSTDMT_DepThreadPoolJob*) * ctx->nbJobsRemaining);
	for (i = 0; i < ctx->maxNbThreads; ++i) {
		ctx->threads[i] = (ZSTDMT_DepThreadPoolThread*)malloc(sizeof(ZSTDMT_DepThreadPoolThread));
		ctx->threads[i]->ctx = ctx;
		ctx->threads[i]->job = NULL;
		ZSTDMT_checkStatus(pthread_mutex_init(&ctx->threads[i]->mutex, NULL), "Mutex init thread");
		ZSTDMT_checkStatus(pthread_create(&ctx->threads[i]->thread, NULL, ZSTDMT_threadRoutine, ctx->threads[i]), "Thread create");
	}

	return ctx;
}

void ZSTDMT_depThreadPool_destroyCtx(ZSTDMT_DepThreadPoolCtx* ctx)
{
	size_t i;
	for (i = 0; i < ctx->maxNbThreads; ++i) pthread_join(ctx->threads[i]->thread, NULL);
	for (i = 0; i < ctx->maxNbThreads; ++i) {
		ZSTDMT_checkStatus(pthread_mutex_destroy(&ctx->threads[i]->mutex), "Mutex destroy thread");
		free(ctx->threads[i]);
	}
	free(ctx->threads);
	free(ctx->jobs);
	ZSTDMT_checkStatus(pthread_mutex_destroy(&ctx->mutex), "Mutex destroy ctx");
	ZSTDMT_checkStatus(pthread_cond_destroy(&ctx->cond), "Cond destroy ctx");
	free(ctx);
}

size_t ZSTDMT_depThreadPool_addJob(ZSTDMT_DepThreadPoolCtx* ctx, ZSTDMT_depThreadPoolFn fn,
	void* data, size_t nbDeps, size_t* depJobIds)
{
	ZSTDMT_DepThreadPoolJob* job = (ZSTDMT_DepThreadPoolJob*)malloc(sizeof(ZSTDMT_DepThreadPoolJob));
	job->fn = fn;
	job->data = data;
	job->started = 0;
	job->finished = 0;
	job->nbDeps = nbDeps;
	job->depJobIds = (size_t*)malloc(sizeof(size_t) * nbDeps);
	memcpy(job->depJobIds, depJobIds, sizeof(size_t) * nbDeps);

	ZSTDMT_checkStatus(pthread_mutex_lock(&ctx->mutex), "Lock ctx");
	ctx->jobs[ctx->nbJobs++] = job;
	ZSTDMT_checkStatus(pthread_cond_signal(&ctx->cond), "Cond signal");
	ZSTDMT_checkStatus(pthread_mutex_unlock(&ctx->mutex), "Unlock ctx");
  return ctx->nbJobs - 1;
}
