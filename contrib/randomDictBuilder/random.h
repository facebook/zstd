#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* malloc, free, qsort */
#include <string.h> /* memset */
#include <time.h>   /* clock */
#include "zstd_internal.h" /* includes zstd.h */
#ifndef ZDICT_STATIC_LINKING_ONLY
#define ZDICT_STATIC_LINKING_ONLY
#endif
#include "zdict.h"

/**
 * A segment is an inclusive range in the source.
 */
typedef struct {
  U32 begin;
  U32 end;
} RANDOM_segment_t;


typedef struct {
    unsigned k;                  /* Segment size : constraint: 0 < k : Reasonable range [16, 2048+]; Default to 200 */
    ZDICT_params_t zParams;
} ZDICT_random_params_t;



ZDICTLIB_API size_t ZDICT_trainFromBuffer_random(
    void *dictBuffer, size_t dictBufferCapacity,
    const void *samplesBuffer, const size_t *samplesSizes, unsigned nbSamples,
    ZDICT_random_params_t parameters);
