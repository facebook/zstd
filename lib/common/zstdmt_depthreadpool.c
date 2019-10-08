#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "zstdmt_depthreadpool.h"

/***************************************
* Definitions
***************************************/

struct ZSTDMT_depThreadPoolJob_s {
	ZSTDMT_depThreadPoolFn fn;
	void* data;
	int independent;
	size_t depJobId;
	int started;
	int finished;
};

struct ZSTDMT_depThreadPoolThread_s {
	pthread_mutex_t mutex;
	pthread_t thread;
	ZSTDMT_depThreadPoolJob_t* job;
	ZSTDMT_depThreadPoolCtx_t* ctx;
};

struct ZSTDMT_depThreadPoolCtx_s {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	ZSTDMT_depThreadPoolThread_t** threads;
	size_t maxNbThreads;
	ZSTDMT_depThreadPoolJob_t** jobs;
	size_t nbJobs;
	size_t nbJobsRemaining;
};

/***************************************
* ZSTDMT_depThreadPool internal
***************************************/

/* This just linearly searches the array of jobs for ANY
 * job that hasn't already been started and either has
 * no dependencies or has its dependency satisfied */
static ZSTDMT_depThreadPoolJob_t* ZSTDMT_getNextJob(ZSTDMT_depThreadPoolCtx_t* ctx)
{
	ZSTDMT_depThreadPoolJob_t* job = NULL;
	size_t i;

	for (i = 0; i < ctx->nbJobs; ++i) {
		job = ctx->jobs[i];
		if (job && !job->started && (job->independent || (!job->independent && ctx->jobs[job->depJobId]->finished))) break;
		else job = NULL;
	}
	if (job) job->started = 1;

	return job;
}

/* This is the main worker routine. Basically, it waits for a job to become
 * ready for execution by calling ZSTDMT_getNextJob(...) and then runs it!
 * It should only exit when all jobs have finished running */
static void* ZSTDMT_threadRoutine(void* data)
{
	ZSTDMT_depThreadPoolThread_t* thread = (ZSTDMT_depThreadPoolThread_t*)data;
	ZSTDMT_depThreadPoolCtx_t* ctx = thread->ctx;

	pthread_mutex_lock(&thread->mutex);
	thread->job = NULL;
	while (1) {
		pthread_mutex_lock(&ctx->mutex);
		while (ctx->nbJobsRemaining && (thread->job = ZSTDMT_getNextJob(ctx)) == NULL)
			pthread_cond_wait(&ctx->cond, &ctx->mutex);
		if (!ctx->nbJobsRemaining) break;
		pthread_mutex_unlock(&ctx->mutex);

		thread->job->fn(thread->job->data);
		thread->job->finished = 1;
		free(thread->job);
		thread->job = NULL;

		pthread_mutex_lock(&ctx->mutex);
		ctx->nbJobsRemaining--;
		if (!ctx->nbJobsRemaining) break;
		pthread_cond_signal(&ctx->cond);
		pthread_mutex_unlock(&ctx->mutex);
	}
	pthread_cond_signal(&ctx->cond);
	pthread_mutex_unlock(&ctx->mutex);
	pthread_mutex_unlock(&thread->mutex);

	return NULL;
}

/***************************************
* ZSTDMT_depThreadPool API
***************************************/

ZSTDMT_depThreadPoolCtx_t* ZSTDMT_depThreadPool_createCtx(size_t maxNbJobs, size_t maxNbThreads)
{
	ZSTDMT_depThreadPoolCtx_t* ctx = malloc(sizeof(ZSTDMT_depThreadPoolCtx_t));
	size_t i;

	pthread_mutex_init(&ctx->mutex, NULL);
	pthread_cond_init(&ctx->cond, NULL);
	ctx->nbJobs = 0;
	ctx->nbJobsRemaining = maxNbJobs;
	ctx->maxNbThreads = maxNbThreads;
	ctx->threads = malloc(sizeof(ZSTDMT_depThreadPoolThread_t*) * ctx->maxNbThreads);
	ctx->jobs = malloc(sizeof(ZSTDMT_depThreadPoolJob_t*) * ctx->nbJobsRemaining);
	for (i = 0; i < ctx->maxNbThreads; ++i) {
		ctx->threads[i] = malloc(sizeof(ZSTDMT_depThreadPoolThread_t));
		ctx->threads[i]->ctx = ctx;
		ctx->threads[i]->job = NULL;
		pthread_mutex_init(&ctx->threads[i]->mutex, NULL);
		pthread_create(&ctx->threads[i]->thread, NULL, ZSTDMT_threadRoutine, ctx->threads[i]);
	}

	return ctx;
}

void ZSTDMT_depThreadPool_destroyCtx(ZSTDMT_depThreadPoolCtx_t* ctx)
{
	size_t i;
	for (i = 0; i < ctx->maxNbThreads; ++i) pthread_join(ctx->threads[i]->thread, NULL);
	for (i = 0; i < ctx->maxNbThreads; ++i) {
		pthread_mutex_destroy(&ctx->threads[i]->mutex);
		free(ctx->threads[i]);
	}
	free(ctx->threads);
	free(ctx->jobs);
	pthread_mutex_destroy(&ctx->mutex);
	pthread_cond_destroy(&ctx->cond);
	free(ctx);
}

size_t ZSTDMT_depThreadPool_addJob(ZSTDMT_depThreadPoolCtx_t* ctx, ZSTDMT_depThreadPoolFn fn,
	void* data, int independent, size_t depJobId)
{
	ZSTDMT_depThreadPoolJob_t* job = malloc(sizeof(ZSTDMT_depThreadPoolJob_t));
	job->fn = fn;
	job->data = data;
	job->started = 0;
	job->finished = 0;
	job->independent = independent;
	job->depJobId = depJobId;

	pthread_mutex_lock(&ctx->mutex);
	ctx->jobs[ctx->nbJobs++] = job;
	pthread_cond_signal(&ctx->cond);
	pthread_mutex_unlock(&ctx->mutex);
  return ctx->nbJobs - 1;
}
