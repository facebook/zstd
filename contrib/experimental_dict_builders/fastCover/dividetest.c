#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* malloc, free, qsort */
#include <string.h>   /* strcmp, strlen */
#include <errno.h>    /* errno */
#include <ctype.h>
#include <float.h>
#include "fastCover.h"
#include "io.h"
#include "util.h"
#include <time.h>   /* clock */
#include "mem.h" /* read */
#include "pool.h"
#include "threading.h"
#include "zstd_internal.h" /* includes zstd.h */
#include "zdict.h"

/*-*************************************
*  Constants
***************************************/
#define FASTCOVER_MAX_SAMPLES_SIZE (sizeof(size_t) == 8 ? ((U32)-1) : ((U32)1 GB))
#define FASTCOVER_MAX_F 32
#define DEFAULT_SPLITPOINT 1.0
static const unsigned g_defaultMaxDictSize = 110 KB;

/*-*************************************
*  Console display
***************************************/
static int g_displayLevel = 2;
#define DISPLAY(...)                                                           \
  {                                                                            \
    fprintf(stderr, __VA_ARGS__);                                              \
    fflush(stderr);                                                            \
  }
#define LOCALDISPLAYLEVEL(displayLevel, l, ...)                                \
  if (displayLevel >= l) {                                                     \
    DISPLAY(__VA_ARGS__);                                                      \
  } /* 0 : no display;   1: errors;   2: default;  3: details;  4: debug */
#define DISPLAYLEVEL(l, ...) LOCALDISPLAYLEVEL(g_displayLevel, l, __VA_ARGS__)

#define LOCALDISPLAYUPDATE(displayLevel, l, ...)                               \
  if (displayLevel >= l) {                                                     \
    if ((clock() - g_time > refreshRate) || (displayLevel >= 4)) {             \
      g_time = clock();                                                        \
      DISPLAY(__VA_ARGS__);                                                    \
    }                                                                          \
  }
#define DISPLAYUPDATE(l, ...) LOCALDISPLAYUPDATE(g_displayLevel, l, __VA_ARGS__)
static const clock_t refreshRate = CLOCKS_PER_SEC * 15 / 100;
static clock_t g_time = 0;


/*-*************************************
* Hash Functions
***************************************/
static const U64 prime6bytes = 227718039650203ULL;
static size_t ZSTD_hash6(U64 u, U32 h) { return (size_t)(((u  << (64-48)) * prime6bytes) >> (64-h)) ; }
static size_t ZSTD_hash6Ptr(const void* p, U32 h) { return ZSTD_hash6(MEM_readLE64(p), h); }

static const U64 prime8bytes = 0xCF1BBCDCB7A56463ULL;
static size_t ZSTD_hash8(U64 u, U32 h) { return (size_t)(((u) * prime8bytes) >> (64-h)) ; }
static size_t ZSTD_hash8Ptr(const void* p, U32 h) { return ZSTD_hash8(MEM_readLE64(p), h); }


/**
 * Hash the d-byte value pointed to by p and mod 2^f
 */
static size_t FASTCOVER_hashPtrToIndex(const void* p, U32 h, unsigned d) {
  if (d == 6) {
    return ZSTD_hash6Ptr(p, h) & ((1 << h) - 1);
  }
  return ZSTD_hash8Ptr(p, h) & ((1 << h) - 1);
}


/*-*************************************
* Context
***************************************/
typedef struct {
  const BYTE *samples;
  size_t *offsets;
  const size_t *samplesSizes;
  size_t nbSamples;
  size_t nbTrainSamples;
  size_t nbTestSamples;
  size_t nbDmers;
  U32 *freqs;
  U16 *segmentFreqs;
  unsigned d;
} FASTCOVER_ctx_t;


/*-*************************************
*  Helper functions
***************************************/
/**
 * Returns the sum of the sample sizes.
 */
static size_t FASTCOVER_sum(const size_t *samplesSizes, unsigned nbSamples) {
  size_t sum = 0;
  unsigned i;
  for (i = 0; i < nbSamples; ++i) {
    sum += samplesSizes[i];
  }
  return sum;
}

static void FASTCOVER_ctx_destroy(FASTCOVER_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  if (ctx->segmentFreqs) {
    free(ctx->segmentFreqs);
    ctx->segmentFreqs = NULL;
  }
  if (ctx->freqs) {
    free(ctx->freqs);
    ctx->freqs = NULL;
  }
  if (ctx->offsets) {
    free(ctx->offsets);
    ctx->offsets = NULL;
  }
}

static void FASTCOVER_computeFrequency(U32 *freqs, unsigned f, FASTCOVER_ctx_t *ctx){
  size_t start = 0; /* start of current dmer */
  const size_t end = ctx->offsets[ctx->nbTrainSamples];
  while (start + ctx->d <= end) {
    const size_t dmerIndex = FASTCOVER_hashPtrToIndex(ctx->samples + start, f, ctx->d);
    freqs[dmerIndex]++;
    start++;
  }
}

static int FASTCOVER_ctx_init(FASTCOVER_ctx_t *ctx, const void *samplesBuffer,
                          const size_t *samplesSizes, unsigned nbSamples,
                          unsigned d, double splitPoint, unsigned f) {
  const BYTE *const samples = (const BYTE *)samplesBuffer;
  const size_t totalSamplesSize = FASTCOVER_sum(samplesSizes, nbSamples);
  /* Split samples into testing and training sets */
  const unsigned nbTrainSamples = splitPoint < 1.0 ? (unsigned)((double)nbSamples * splitPoint) : nbSamples;
  const unsigned nbTestSamples = splitPoint < 1.0 ? nbSamples - nbTrainSamples : nbSamples;
  const size_t trainingSamplesSize = splitPoint < 1.0 ? FASTCOVER_sum(samplesSizes, nbTrainSamples) : totalSamplesSize;
  const size_t testSamplesSize = splitPoint < 1.0 ? FASTCOVER_sum(samplesSizes + nbTrainSamples, nbTestSamples) : totalSamplesSize;
  /* Checks */
  if (totalSamplesSize < MAX(d, sizeof(U64)) ||
      totalSamplesSize >= (size_t)FASTCOVER_MAX_SAMPLES_SIZE) {
    DISPLAYLEVEL(1, "Total samples size is too large (%u MB), maximum size is %u MB\n",
                 (U32)(totalSamplesSize>>20), (FASTCOVER_MAX_SAMPLES_SIZE >> 20));
    return 0;
  }
  /* Check if there are at least 5 training samples */
  if (nbTrainSamples < 5) {
    DISPLAYLEVEL(1, "Total number of training samples is %u and is invalid.", nbTrainSamples);
    return 0;
  }
  /* Check if there's testing sample */
  if (nbTestSamples < 1) {
    DISPLAYLEVEL(1, "Total number of testing samples is %u and is invalid.", nbTestSamples);
    return 0;
  }
  /* Zero the context */
  memset(ctx, 0, sizeof(*ctx));
  DISPLAYLEVEL(2, "Training on %u samples of total size %u\n", nbTrainSamples,
               (U32)trainingSamplesSize);
  DISPLAYLEVEL(2, "Testing on %u samples of total size %u\n", nbTestSamples,
               (U32)testSamplesSize);

  ctx->samples = samples;
  ctx->samplesSizes = samplesSizes;
  ctx->nbSamples = nbSamples;
  ctx->nbTrainSamples = nbTrainSamples;
  ctx->nbTestSamples = nbTestSamples;
  ctx->nbDmers = trainingSamplesSize - d + 1;
  ctx->d = d;

  /* The offsets of each file */
  ctx->offsets = (size_t *)malloc((nbSamples + 1) * sizeof(size_t));
  if (!ctx->offsets) {
    DISPLAYLEVEL(1, "Failed to allocate scratch buffers\n");
    FASTCOVER_ctx_destroy(ctx);
    return 0;
  }

  /* Fill offsets from the samplesSizes */
  {
    U32 i;
    ctx->offsets[0] = 0;
    for (i = 1; i <= nbSamples; ++i) {
      ctx->offsets[i] = ctx->offsets[i - 1] + samplesSizes[i - 1];
    }
  }

  /* Initialize frequency array of size 2^f */
  ctx->freqs = (U32 *)calloc((1 << f), sizeof(U32));
  ctx->segmentFreqs = (U16 *)calloc((1 << f), sizeof(U16));


  return 1;
}





int main(int argCount, const char* argv[])
{
  int displayLevel = 2;
  const char* programName = argv[0];
  int operationResult = 0;

  /* Initialize arguments to default values */
  unsigned k = 0;
  unsigned d = 0;
  unsigned f = 23;
  unsigned steps = 32;
  unsigned nbThreads = 1;
  unsigned split = 100;
  const char* outputFile = "fastCoverDict";
  unsigned dictID = 0;
  unsigned maxDictSize = g_defaultMaxDictSize;

  /* Initialize table to store input files */
  const char** filenameTable = (const char**)malloc(argCount * sizeof(const char*));
  unsigned filenameIdx = 0;

  char* fileNamesBuf = NULL;
  unsigned fileNamesNb = filenameIdx;
  int followLinks = 0; /* follow directory recursively */
  const char** extendedFileList = NULL;

  /* Parse arguments */
  for (int i = 1; i < argCount; i++) {
    const char* argument = argv[i];
    if (longCommandWArg(&argument, "k=")) { k = readU32FromChar(&argument); continue; }
    if (longCommandWArg(&argument, "d=")) { d = readU32FromChar(&argument); continue; }
    if (longCommandWArg(&argument, "f=")) { f = readU32FromChar(&argument); continue; }
    if (longCommandWArg(&argument, "steps=")) { steps = readU32FromChar(&argument); continue; }
    if (longCommandWArg(&argument, "split=")) { split = readU32FromChar(&argument); continue; }
    if (longCommandWArg(&argument, "dictID=")) { dictID = readU32FromChar(&argument); continue; }
    if (longCommandWArg(&argument, "maxdict=")) { maxDictSize = readU32FromChar(&argument); continue; }
    if (longCommandWArg(&argument, "in=")) {
      filenameTable[filenameIdx] = argument;
      filenameIdx++;
      continue;
    }
    if (longCommandWArg(&argument, "out=")) {
      outputFile = argument;
      continue;
    }
    printf("Incorrect parameters\n");
    operationResult = 1;
    return operationResult;
  }

  /* Get the list of all files recursively (because followLinks==0)*/
  extendedFileList = UTIL_createFileList(filenameTable, filenameIdx, &fileNamesBuf,
                                        &fileNamesNb, followLinks);
  if (extendedFileList) {
      unsigned u;
      for (u=0; u<fileNamesNb; u++) DISPLAYLEVEL(4, "%u %s\n", u, extendedFileList[u]);
      free((void*)filenameTable);
      filenameTable = extendedFileList;
      filenameIdx = fileNamesNb;
  }

  size_t blockSize = 0;

  /* Set up zParams */
  ZDICT_params_t zParams;
  zParams.compressionLevel = 3;
  zParams.notificationLevel = displayLevel;
  zParams.dictID = dictID;

  /* Set up fastCover params */
  ZDICT_fastCover_params_t params;
  params.zParams = zParams;
  params.k = k;
  params.d = d;
  params.f = f;
  params.steps = steps;
  params.nbThreads = nbThreads;
  params.splitPoint = (double)split/100;

  /* Build dictionary */
  sampleInfo* info = getSampleInfo(filenameTable,
                    filenameIdx, blockSize, maxDictSize, zParams.notificationLevel);

  FASTCOVER_ctx_t ctx;
  if (!FASTCOVER_ctx_init(&ctx, info->srcBuffer, info->samplesSizes, info->nbSamples,
                          params.d, params.splitPoint, params.f)) {
    printf("Failed to initialize context\n");
    return 1;
  }
  const size_t kRunsPerTrial = 100;
  const size_t kTrials = 200;
  double min = DBL_MAX;
  double max = 0;
  for (size_t i = 0; i < kTrials; ++i) {
    memset(ctx.freqs, 0, (1 << f) * sizeof(U32));
    const UTIL_time_t begin = UTIL_getTime();
    for (size_t j = 0; j < kRunsPerTrial; ++j) {
      FASTCOVER_computeFrequency(ctx.freqs, params.f, &ctx);
    }
    const U64 timeMicro = UTIL_clockSpanMicro(begin);
    const double timeSec = timeMicro / (double)SEC_TO_MICRO;
    DISPLAYLEVEL(1, "computeFrequency took %f seconds to execute \n", timeSec);
    if (timeSec < min) min = timeSec;
    if (timeSec > max) max = timeSec;
  }
  printf("min is %f\n", min);
  printf("max is %f\n", max);


  FASTCOVER_ctx_destroy(&ctx);

  /* Free allocated memory */
  UTIL_freeFileList(extendedFileList, fileNamesBuf);
  freeSampleInfo(info);

  return operationResult;
}
