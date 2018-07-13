/*-*************************************
*  Dependencies
***************************************/
#include <stdio.h>            /* fprintf */
#include <stdlib.h>           /* malloc, free, qsort */
#include <string.h>           /* memset */
#include <time.h>             /* clock */
#include "zstd_internal.h" /* includes zstd.h */
#ifndef ZDICT_STATIC_LINKING_ONLY
#define ZDICT_STATIC_LINKING_ONLY
#endif
#include "random.h"
#include "platform.h"         /* Large Files support */
#include "util.h"             /* UTIL_getFileSize, UTIL_getTotalFileSize */

/*-*************************************
*  Constants
***************************************/
#define SAMPLESIZE_MAX (128 KB)
#define RANDOM_MAX_SAMPLES_SIZE (sizeof(size_t) == 8 ? ((U32)-1) : ((U32)1 GB))
#define RANDOM_MEMMULT 9
static const size_t g_maxMemory = (sizeof(size_t) == 4) ? (2 GB - 64 MB) : ((size_t)(512 MB) << sizeof(size_t));

#define NOISELENGTH 32
#define DEFAULT_K 200

/*-*************************************
*  Console display
***************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (displayLevel>=l) { DISPLAY(__VA_ARGS__); }

static const U64 g_refreshRate = SEC_TO_MICRO / 6;
static UTIL_time_t g_displayClock = UTIL_TIME_INITIALIZER;

#define DISPLAYUPDATE(l, ...) { if (displayLevel>=l) { \
            if ((UTIL_clockSpanMicro(g_displayClock) > g_refreshRate) || (displayLevel>=4)) \
            { g_displayClock = UTIL_getTime(); DISPLAY(__VA_ARGS__); \
            if (displayLevel>=4) fflush(stderr); } } }


/*-*************************************
*  Exceptions
***************************************/
#ifndef DEBUG
#  define DEBUG 0
#endif
#define DEBUGOUTPUT(...) if (DEBUG) DISPLAY(__VA_ARGS__);
#define EXM_THROW(error, ...)                                             \
{                                                                         \
    DEBUGOUTPUT("Error defined at %s, line %i : \n", __FILE__, __LINE__); \
    DISPLAY("Error %i : ", error);                                        \
    DISPLAY(__VA_ARGS__);                                                 \
    DISPLAY("\n");                                                        \
    exit(error);                                                          \
}


/* ********************************************************
*  File related operations
**********************************************************/
/** loadFiles() :
 *  load samples from files listed in fileNamesTable into buffer.
 *  works even if buffer is too small to load all samples.
 *  Also provides the size of each sample into sampleSizes table
 *  which must be sized correctly, using DiB_fileStats().
 * @return : nb of samples effectively loaded into `buffer`
 * *bufferSizePtr is modified, it provides the amount data loaded within buffer.
 *  sampleSizes is filled with the size of each sample.
 */
static unsigned loadFiles(void* buffer, size_t* bufferSizePtr,
                              size_t* sampleSizes, unsigned sstSize,
                              const char** fileNamesTable, unsigned nbFiles, size_t targetChunkSize,
                              unsigned displayLevel)
{
    char* const buff = (char*)buffer;
    size_t pos = 0;
    unsigned nbLoadedChunks = 0, fileIndex;

    for (fileIndex=0; fileIndex<nbFiles; fileIndex++) {
        const char* const fileName = fileNamesTable[fileIndex];
        unsigned long long const fs64 = UTIL_getFileSize(fileName);
        unsigned long long remainingToLoad = (fs64 == UTIL_FILESIZE_UNKNOWN) ? 0 : fs64;
        U32 const nbChunks = targetChunkSize ? (U32)((fs64 + (targetChunkSize-1)) / targetChunkSize) : 1;
        U64 const chunkSize = targetChunkSize ? MIN(targetChunkSize, fs64) : fs64;
        size_t const maxChunkSize = (size_t)MIN(chunkSize, SAMPLESIZE_MAX);
        U32 cnb;
        FILE* const f = fopen(fileName, "rb");
        if (f==NULL) EXM_THROW(10, "zstd: dictBuilder: %s %s ", fileName, strerror(errno));
        DISPLAYUPDATE(2, "Loading %s...       \r", fileName);
        for (cnb=0; cnb<nbChunks; cnb++) {
            size_t const toLoad = (size_t)MIN(maxChunkSize, remainingToLoad);
            if (toLoad > *bufferSizePtr-pos) break;
            {   size_t const readSize = fread(buff+pos, 1, toLoad, f);
                if (readSize != toLoad) EXM_THROW(11, "Pb reading %s", fileName);
                pos += readSize;
                sampleSizes[nbLoadedChunks++] = toLoad;
                remainingToLoad -= targetChunkSize;
                if (nbLoadedChunks == sstSize) { /* no more space left in sampleSizes table */
                    fileIndex = nbFiles;  /* stop there */
                    break;
                }
                if (toLoad < targetChunkSize) {
                    fseek(f, (long)(targetChunkSize - toLoad), SEEK_CUR);
        }   }   }
        fclose(f);
    }
    DISPLAYLEVEL(2, "\r%79s\r", "");
    *bufferSizePtr = pos;
    DISPLAYLEVEL(4, "loaded : %u KB \n", (U32)(pos >> 10))
    return nbLoadedChunks;
}



#define rotl32(x,r) ((x << r) | (x >> (32 - r)))
static U32 getRand(U32* src)
{
    static const U32 prime1 = 2654435761U;
    static const U32 prime2 = 2246822519U;
    U32 rand32 = *src;
    rand32 *= prime1;
    rand32 ^= prime2;
    rand32  = rotl32(rand32, 13);
    *src = rand32;
    return rand32 >> 5;
}


/* shuffle() :
 * shuffle a table of file names in a semi-random way
 * It improves dictionary quality by reducing "locality" impact, so if sample set is very large,
 * it will load random elements from it, instead of just the first ones. */
static void shuffle(const char** fileNamesTable, unsigned nbFiles) {
    U32 seed = 0xFD2FB528;
    unsigned i;
    for (i = nbFiles - 1; i > 0; --i) {
        unsigned const j = getRand(&seed) % (i + 1);
        const char* const tmp = fileNamesTable[j];
        fileNamesTable[j] = fileNamesTable[i];
        fileNamesTable[i] = tmp;
    }
}



/*-********************************************************
*  Dictionary training functions
**********************************************************/
static size_t findMaxMem(unsigned long long requiredMem)
{
    size_t const step = 8 MB;
    void* testmem = NULL;

    requiredMem = (((requiredMem >> 23) + 1) << 23);
    requiredMem += step;
    if (requiredMem > g_maxMemory) requiredMem = g_maxMemory;

    while (!testmem) {
        testmem = malloc((size_t)requiredMem);
        requiredMem -= step;
    }

    free(testmem);
    return (size_t)requiredMem;
}

static void saveDict(const char* dictFileName,
                         const void* buff, size_t buffSize)
{
    FILE* const f = fopen(dictFileName, "wb");
    if (f==NULL) EXM_THROW(3, "cannot open %s ", dictFileName);

    { size_t const n = fwrite(buff, 1, buffSize, f);
      if (n!=buffSize) EXM_THROW(4, "%s : write error", dictFileName) }

    { size_t const n = (size_t)fclose(f);
      if (n!=0) EXM_THROW(5, "%s : flush error", dictFileName) }
}

/*! getFileStats() :
 *  Given a list of files, and a chunkSize (0 == no chunk, whole files)
 *  provides the amount of data to be loaded and the resulting nb of samples.
 *  This is useful primarily for allocation purpose => sample buffer, and sample sizes table.
 */
static fileStats getFileStats(const char** fileNamesTable, unsigned nbFiles, size_t chunkSize, unsigned displayLevel)
{
    fileStats fs;
    unsigned n;
    memset(&fs, 0, sizeof(fs));
    for (n=0; n<nbFiles; n++) {
        U64 const fileSize = UTIL_getFileSize(fileNamesTable[n]);
        U64 const srcSize = (fileSize == UTIL_FILESIZE_UNKNOWN) ? 0 : fileSize;
        U32 const nbSamples = (U32)(chunkSize ? (srcSize + (chunkSize-1)) / chunkSize : 1);
        U64 const chunkToLoad = chunkSize ? MIN(chunkSize, srcSize) : srcSize;
        size_t const cappedChunkSize = (size_t)MIN(chunkToLoad, SAMPLESIZE_MAX);
        fs.totalSizeToLoad += cappedChunkSize * nbSamples;
        fs.oneSampleTooLarge |= (chunkSize > 2*SAMPLESIZE_MAX);
        fs.nbSamples += nbSamples;
    }
    DISPLAYLEVEL(4, "Preparing to load : %u KB \n", (U32)(fs.totalSizeToLoad >> 10));
    return fs;
}





/* ********************************************************
*  Random Dictionary Builder
**********************************************************/
/**
 * Returns the sum of the sample sizes.
 */
static size_t RANDOM_sum(const size_t *samplesSizes, unsigned nbSamples) {
  size_t sum = 0;
  unsigned i;
  for (i = 0; i < nbSamples; ++i) {
    sum += samplesSizes[i];
  }
  return sum;
}


/**
 * Selects a random segment from totalSamplesSize - k + 1 possible segments
 */
static RANDOM_segment_t RANDOM_selectSegment(const RANDOM_ctx_t *ctx,
                                            ZDICT_random_params_t parameters) {
    const U32 k = parameters.k;
    RANDOM_segment_t segment;
    unsigned index;

    /* Seed random number generator */
    srand((unsigned)time(NULL));
    /* Randomly generate a number from 0 to sampleSizes - k */
    index = rand()%(ctx->totalSamplesSize - k + 1);

    /* inclusive */
    segment.begin = index;
    segment.end = index + k - 1;

    return segment;
}


/**
 * Check the validity of the parameters.
 * Returns non-zero if the parameters are valid and 0 otherwise.
 */
static int RANDOM_checkParameters(ZDICT_random_params_t parameters, size_t maxDictSize) {
    /* k is a required parameter */
    if (parameters.k == 0) {
      return 0;
    }
    /* k <= maxDictSize */
    if (parameters.k > maxDictSize) {
      return 0;
    }
    return 1;
}


/**
 * Clean up a context initialized with `RANDOM_ctx_init()`.
 */
static void RANDOM_ctx_destroy(RANDOM_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  if (ctx->offsets) {
    free(ctx->offsets);
    ctx->offsets = NULL;
  }
}


/**
 * Prepare a context for dictionary building.
 * Returns 1 on success or zero on error.
 * The context must be destroyed with `RANDOM_ctx_destroy()`.
 */
static int RANDOM_ctx_init(RANDOM_ctx_t *ctx, const void *samplesBuffer,
                          const size_t *samplesSizes, unsigned nbSamples) {
    const BYTE *const samples = (const BYTE *)samplesBuffer;
    const size_t totalSamplesSize = RANDOM_sum(samplesSizes, nbSamples);
    const int displayLevel = 2;
    /* Checks */
    if (totalSamplesSize >= (size_t)RANDOM_MAX_SAMPLES_SIZE) {
      DISPLAYLEVEL(1, "Total samples size is too large (%u MB), maximum size is %u MB\n",
                   (U32)(totalSamplesSize>>20), (RANDOM_MAX_SAMPLES_SIZE >> 20));
      return 0;
    }
    memset(ctx, 0, sizeof(*ctx));
    DISPLAYLEVEL(1, "Building dictionary from %u samples of total size %u\n", nbSamples,
                 (U32)totalSamplesSize);
    ctx->samples = samples;
    ctx->samplesSizes = samplesSizes;
    ctx->nbSamples = nbSamples;
    ctx->offsets = (size_t *)malloc((nbSamples + 1) * sizeof(size_t));
    ctx->totalSamplesSize = (U32)totalSamplesSize;
    if (!ctx->offsets) {
      DISPLAYLEVEL(1, "Failed to allocate buffer for offsets\n");
      RANDOM_ctx_destroy(ctx);
      return 0;
    }
    {
      U32 i;
      ctx->offsets[0] = 0;
      for (i = 1; i <= nbSamples; ++i) {
        ctx->offsets[i] = ctx->offsets[i - 1] + samplesSizes[i - 1];
      }
    }
    return 1;
}


/**
 * Given the prepared context build the dictionary.
 */
static size_t RANDOM_buildDictionary(const RANDOM_ctx_t *ctx, void *dictBuffer,
                                    size_t dictBufferCapacity,
                                    ZDICT_random_params_t parameters) {
    BYTE *const dict = (BYTE *)dictBuffer;
    size_t tail = dictBufferCapacity;
    const int displayLevel = parameters.zParams.notificationLevel;
    while (tail > 0) {

      /* Select a segment */
      RANDOM_segment_t segment = RANDOM_selectSegment(ctx, parameters);

      size_t segmentSize;
      segmentSize = MIN(segment.end - segment.begin + 1, tail);

      tail -= segmentSize;
      memcpy(dict + tail, ctx->samples + segment.begin, segmentSize);
      DISPLAYUPDATE(
          2, "\r%u%%       ",
          (U32)(((dictBufferCapacity - tail) * 100) / dictBufferCapacity));
    }

    return tail;
}

/*! ZDICT_trainFromBuffer_random():
 *  Train a dictionary from an array of samples using the RANDOM algorithm.
 *  Samples must be stored concatenated in a single flat buffer `samplesBuffer`,
 *  supplied with an array of sizes `samplesSizes`, providing the size of each sample, in order.
 *  The resulting dictionary will be saved into `dictBuffer`.
 * @return: size of dictionary stored into `dictBuffer` (<= `dictBufferCapacity`)
 *          or an error code, which can be tested with ZDICT_isError().
 */
ZDICTLIB_API size_t ZDICT_trainFromBuffer_random(
    void *dictBuffer, size_t dictBufferCapacity,
    const void *samplesBuffer, const size_t *samplesSizes, unsigned nbSamples,
    ZDICT_random_params_t parameters) {
      const int displayLevel = parameters.zParams.notificationLevel;
      BYTE* const dict = (BYTE*)dictBuffer;
      RANDOM_ctx_t ctx;
      /* Checks */
      if (!RANDOM_checkParameters(parameters, dictBufferCapacity)) {
          DISPLAYLEVEL(1, "k is incorrect\n");
          return ERROR(GENERIC);
      }
      if (nbSamples == 0) {
        DISPLAYLEVEL(1, "Random must have at least one input file\n");
        return ERROR(GENERIC);
      }
      if (dictBufferCapacity < ZDICT_DICTSIZE_MIN) {
        DISPLAYLEVEL(1, "dictBufferCapacity must be at least %u\n",
                     ZDICT_DICTSIZE_MIN);
        return ERROR(dstSize_tooSmall);
      }

      if (!RANDOM_ctx_init(&ctx, samplesBuffer, samplesSizes, nbSamples)) {
        return ERROR(GENERIC);
      }
      DISPLAYLEVEL(2, "Building dictionary\n");
      {
        const size_t tail = RANDOM_buildDictionary(&ctx, dictBuffer, dictBufferCapacity, parameters);
        const size_t dictSize = ZDICT_finalizeDictionary(
            dict, dictBufferCapacity, dict + tail, dictBufferCapacity - tail,
            samplesBuffer, samplesSizes, nbSamples, parameters.zParams);
        if (!ZSTD_isError(dictSize)) {
            DISPLAYLEVEL(2, "Constructed dictionary of size %u\n",
                          (U32)dictSize);
        }
        RANDOM_ctx_destroy(&ctx);
        return dictSize;
      }
}


int RANDOM_trainFromFiles(const char* dictFileName, unsigned maxDictSize,
                       const char** fileNamesTable, unsigned nbFiles,
                       size_t chunkSize, ZDICT_random_params_t *params){
    unsigned const displayLevel = params->zParams.notificationLevel;
    void* const dictBuffer = malloc(maxDictSize);
    fileStats const fs = getFileStats(fileNamesTable, nbFiles, chunkSize, displayLevel);
    size_t* const sampleSizes = (size_t*)malloc(fs.nbSamples * sizeof(size_t));
    size_t const memMult = RANDOM_MEMMULT;
    size_t const maxMem =  findMaxMem(fs.totalSizeToLoad * memMult) / memMult;
    size_t loadedSize = (size_t) MIN ((unsigned long long)maxMem, fs.totalSizeToLoad);
    void* const srcBuffer = malloc(loadedSize+NOISELENGTH);
    int result = 0;

    /* Checks */
    if ((!sampleSizes) || (!srcBuffer) || (!dictBuffer))
        EXM_THROW(12, "not enough memory for DiB_trainFiles");   /* should not happen */
    if (fs.oneSampleTooLarge) {
        DISPLAYLEVEL(2, "!  Warning : some sample(s) are very large \n");
        DISPLAYLEVEL(2, "!  Note that dictionary is only useful for small samples. \n");
        DISPLAYLEVEL(2, "!  As a consequence, only the first %u bytes of each sample are loaded \n", SAMPLESIZE_MAX);
    }
    if (fs.nbSamples < 5) {
        DISPLAYLEVEL(2, "!  Warning : nb of samples too low for proper processing ! \n");
        DISPLAYLEVEL(2, "!  Please provide _one file per sample_. \n");
        DISPLAYLEVEL(2, "!  Alternatively, split files into fixed-size blocks representative of samples, with -B# \n");
        EXM_THROW(14, "nb of samples too low");   /* we now clearly forbid this case */
    }
    if (fs.totalSizeToLoad < (unsigned long long)(8 * maxDictSize)) {
        DISPLAYLEVEL(2, "!  Warning : data size of samples too small for target dictionary size \n");
        DISPLAYLEVEL(2, "!  Samples should be about 100x larger than target dictionary size \n");
    }

    /* init */
    if (loadedSize < fs.totalSizeToLoad)
        DISPLAYLEVEL(1, "Not enough memory; training on %u MB only...\n", (unsigned)(loadedSize >> 20));

    /* Load input buffer */
    DISPLAYLEVEL(3, "Shuffling input files\n");
    shuffle(fileNamesTable, nbFiles);
    nbFiles = loadFiles(srcBuffer, &loadedSize, sampleSizes, fs.nbSamples, fileNamesTable, nbFiles, chunkSize, displayLevel);

    {   size_t dictSize;
        dictSize = ZDICT_trainFromBuffer_random(dictBuffer, maxDictSize, srcBuffer,
                                             sampleSizes, fs.nbSamples, *params);
        DISPLAYLEVEL(2, "k=%u\n", params->k);
        if (ZDICT_isError(dictSize)) {
            DISPLAYLEVEL(1, "dictionary training failed : %s \n", ZDICT_getErrorName(dictSize));   /* should not happen */
            result = 1;
            goto _cleanup;
        }
        /* save dict */
        DISPLAYLEVEL(2, "Save dictionary of size %u into file %s \n", (U32)dictSize, dictFileName);
        saveDict(dictFileName, dictBuffer, dictSize);
    }

    /* clean up */
_cleanup:
    free(srcBuffer);
    free(sampleSizes);
    free(dictBuffer);
    return result;
}
