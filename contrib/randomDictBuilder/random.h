#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* malloc, free, qsort */
#include <string.h> /* memset */
#include <time.h>   /* clock */
#include "zstd_internal.h" /* includes zstd.h */
#ifndef ZDICT_STATIC_LINKING_ONLY
#define ZDICT_STATIC_LINKING_ONLY
#endif
#include "zdict.h"


/**************************************
* Context
***************************************/
typedef struct {
  const BYTE *samples;
  size_t *offsets;
  const size_t *samplesSizes;
  size_t nbSamples;
  U32 totalSamplesSize;
} RANDOM_ctx_t;

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


typedef struct {
    U64 totalSizeToLoad;
    unsigned oneSampleTooLarge;
    unsigned nbSamples;
} fileStats;


ZDICTLIB_API size_t ZDICT_trainFromBuffer_random(
    void *dictBuffer, size_t dictBufferCapacity,
    const void *samplesBuffer, const size_t *samplesSizes, unsigned nbSamples,
    ZDICT_random_params_t parameters);


int RANDOM_trainFromFiles(const char* dictFileName, unsigned maxDictSize,
                        const char** fileNamesTable, unsigned nbFiles,
                        size_t chunkSize, ZDICT_random_params_t *params);
