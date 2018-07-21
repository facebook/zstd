#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* malloc, free, qsort */
#include <string.h>   /* strcmp, strlen */
#include <errno.h>    /* errno */
#include <ctype.h>
#include <time.h>
#include "random.h"
#include "dictBuilder.h"
#include "zstd_internal.h" /* includes zstd.h */
#include "io.h"
#include "util.h"
#include "zdict.h"



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


/*-*************************************
*  Constants
***************************************/
static const unsigned g_defaultMaxDictSize = 110 KB;
#define DEFAULT_CLEVEL 3
#define DEFAULT_DISPLAYLEVEL 2


/*-*************************************
*  Struct
***************************************/
typedef struct {
  const void* dictBuffer;
  size_t dictSize;
} dictInfo;


/*-*************************************
* Dictionary related operations
***************************************/
/** createDictFromFiles() :
 *  Based on type of param given, train dictionary using the corresponding algorithm
 *  @return dictInfo containing dictionary buffer and dictionary size
 */
dictInfo* createDictFromFiles(sampleInfo *info, unsigned maxDictSize,
                  ZDICT_random_params_t *randomParams, ZDICT_cover_params_t *coverParams,
                  ZDICT_legacy_params_t *legacyParams) {
    unsigned const displayLevel = randomParams ? randomParams->zParams.notificationLevel :
                                  coverParams ? coverParams->zParams.notificationLevel :
                                  legacyParams ? legacyParams->zParams.notificationLevel :
                                  DEFAULT_DISPLAYLEVEL;   /* no dict */
    void* const dictBuffer = malloc(maxDictSize);

    dictInfo* dInfo = NULL;

    /* Checks */
    if (!dictBuffer)
        EXM_THROW(12, "not enough memory for trainFromFiles");   /* should not happen */

    {   size_t dictSize;
        if(randomParams) {
          dictSize = ZDICT_trainFromBuffer_random(dictBuffer, maxDictSize, info->srcBuffer,
                                               info->samplesSizes, info->nbSamples, *randomParams);
        }else if(coverParams) {
          dictSize = ZDICT_optimizeTrainFromBuffer_cover(dictBuffer, maxDictSize, info->srcBuffer,
                                                info->samplesSizes, info->nbSamples, coverParams);
        } else if(legacyParams) {
          dictSize = ZDICT_trainFromBuffer_legacy(dictBuffer, maxDictSize, info->srcBuffer,
                                               info->samplesSizes, info->nbSamples, *legacyParams);
        } else {
          dictSize = 0;
        }
        if (ZDICT_isError(dictSize)) {
            DISPLAYLEVEL(1, "dictionary training failed : %s \n", ZDICT_getErrorName(dictSize));   /* should not happen */
            free(dictBuffer);
            return dInfo;
        }
        dInfo = (dictInfo *)malloc(sizeof(dictInfo));
        dInfo->dictBuffer = dictBuffer;
        dInfo->dictSize = dictSize;
    }
    return dInfo;
}


/** compressWithDict() :
 *  Compress samples from sample buffer given dicionary stored on dictionary buffer and compression level
 *  @return compression ratio
 */
double compressWithDict(sampleInfo *srcInfo, dictInfo* dInfo, int compressionLevel, int displayLevel) {
  /* Local variables */
  size_t totalCompressedSize = 0;
  size_t totalOriginalSize = 0;
  const unsigned hasDict = dInfo->dictSize > 0 ? 1 : 0;
  double cRatio;
  size_t dstCapacity;
  int i;

  /* Pointers */
  ZSTD_CDict *cdict = NULL;
  ZSTD_CCtx* cctx = NULL;
  size_t *offsets = NULL;
  void* dst = NULL;

  /* Allocate dst with enough space to compress the maximum sized sample */
  {
    size_t maxSampleSize = 0;
    for (int i = 0; i < srcInfo->nbSamples; i++) {
      maxSampleSize = MAX(srcInfo->samplesSizes[i], maxSampleSize);
    }
    dstCapacity = ZSTD_compressBound(maxSampleSize);
    dst = malloc(dstCapacity);
  }

  /* Calculate offset for each sample */
  offsets = (size_t *)malloc((srcInfo->nbSamples + 1) * sizeof(size_t));
  offsets[0] = 0;
  for (i = 1; i <= srcInfo->nbSamples; i++) {
    offsets[i] = offsets[i - 1] + srcInfo->samplesSizes[i - 1];
  }

  /* Create the cctx */
  cctx = ZSTD_createCCtx();
  if(!cctx || !dst) {
    cRatio = -1;
    goto _cleanup;
  }

  /* Create CDict if there's a dictionary stored on buffer */
  if (hasDict) {
    cdict = ZSTD_createCDict(dInfo->dictBuffer, dInfo->dictSize, compressionLevel);
    if(!cdict) {
      cRatio = -1;
      goto _cleanup;
    }
  }

  /* Compress each sample and sum their sizes*/
  const BYTE *const samples = (const BYTE *)srcInfo->srcBuffer;
  for (i = 0; i < srcInfo->nbSamples; i++) {
    size_t compressedSize;
    if(hasDict) {
      compressedSize = ZSTD_compress_usingCDict(cctx, dst, dstCapacity, samples + offsets[i], srcInfo->samplesSizes[i], cdict);
    } else {
      compressedSize = ZSTD_compressCCtx(cctx, dst, dstCapacity,samples + offsets[i], srcInfo->samplesSizes[i], compressionLevel);
    }
    if (ZSTD_isError(compressedSize)) {
      cRatio = -1;
      goto _cleanup;
    }
    totalCompressedSize += compressedSize;
  }

  /* Sum orignal sizes */
  for (i = 0; i<srcInfo->nbSamples; i++) {
    totalOriginalSize += srcInfo->samplesSizes[i];
  }

  /* Calculate compression ratio */
  DISPLAYLEVEL(2, "original size is %lu\n", totalOriginalSize);
  DISPLAYLEVEL(2, "compressed size is %lu\n", totalCompressedSize);
  cRatio = (double)totalOriginalSize/(double)totalCompressedSize;

_cleanup:
  free(dst);
  free(offsets);
  ZSTD_freeCCtx(cctx);
  ZSTD_freeCDict(cdict);
  return cRatio;
}


/** FreeDictInfo() :
 *  Free memory allocated for dictInfo
 */
void freeDictInfo(dictInfo* info) {
  if (!info) return;
  if (info->dictBuffer) free((void*)(info->dictBuffer));
  free(info);
}



/*-********************************************************
  *  Benchmarking functions
**********************************************************/
/** benchmarkDictBuilder() :
 *  Measure how long a dictionary builder takes and compression ratio with the dictionary built
 *  @return 0 if benchmark successfully, 1 otherwise
 */
int benchmarkDictBuilder(sampleInfo *srcInfo, unsigned maxDictSize, ZDICT_random_params_t *randomParam,
                        ZDICT_cover_params_t *coverParam, ZDICT_legacy_params_t *legacyParam) {
  /* Local variables */
  const unsigned displayLevel = randomParam ? randomParam->zParams.notificationLevel :
                                coverParam ? coverParam->zParams.notificationLevel :
                                legacyParam ? legacyParam->zParams.notificationLevel :
                                DEFAULT_DISPLAYLEVEL;   /* no dict */
  const char* name = randomParam ? "RANDOM" :
                    coverParam ? "COVER" :
                    legacyParam ? "LEGACY" :
                    "NODICT";    /* no dict */
  const unsigned cLevel = randomParam ? randomParam->zParams.compressionLevel :
                          coverParam ? coverParam->zParams.compressionLevel :
                          legacyParam ? legacyParam->zParams.compressionLevel :
                          DEFAULT_CLEVEL;   /* no dict */
  int result = 0;

  /* Calculate speed */
  const UTIL_time_t begin = UTIL_getTime();
  dictInfo* dInfo = createDictFromFiles(srcInfo, maxDictSize, randomParam, coverParam, legacyParam);
  const U64 timeMicro = UTIL_clockSpanMicro(begin);
  const double timeSec = timeMicro / (double)SEC_TO_MICRO;
  if (!dInfo) {
    DISPLAYLEVEL(1, "%s does not train successfully\n", name);
    result = 1;
    goto _cleanup;
  }
  DISPLAYLEVEL(2, "%s took %f seconds to execute \n", name, timeSec);

  /* Calculate compression ratio */
  const double cRatio = compressWithDict(srcInfo, dInfo, cLevel, displayLevel);
  if (cRatio < 0) {
    DISPLAYLEVEL(1, "Compressing with %s dictionary does not work\n", name);
    result = 1;
    goto _cleanup;

  }
  DISPLAYLEVEL(2, "Compression ratio with %s dictionary is %f\n", name, cRatio);

_cleanup:
  freeDictInfo(dInfo);
  return result;
}



int main(int argCount, const char* argv[])
{
  const int displayLevel = DEFAULT_DISPLAYLEVEL;
  const char* programName = argv[0];
  int result = 0;

  /* Initialize arguments to default values */
  const unsigned k = 200;
  const unsigned d = 6;
  const unsigned cLevel = DEFAULT_CLEVEL;
  const unsigned dictID = 0;
  const unsigned maxDictSize = g_defaultMaxDictSize;

  /* Initialize table to store input files */
  const char** filenameTable = (const char**)malloc(argCount * sizeof(const char*));
  unsigned filenameIdx = 0;

  char* fileNamesBuf = NULL;
  unsigned fileNamesNb = filenameIdx;
  const int followLinks = 0;
  const char** extendedFileList = NULL;

  /* Parse arguments */
  for (int i = 1; i < argCount; i++) {
    const char* argument = argv[i];
    if (longCommandWArg(&argument, "in=")) {
      filenameTable[filenameIdx] = argument;
      filenameIdx++;
      continue;
    }
    DISPLAYLEVEL(1, "benchmark: Incorrect parameters\n");
    return 1;
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

  /* get sampleInfo */
  size_t blockSize = 0;
  sampleInfo* srcInfo= getSampleInfo(filenameTable,
                    filenameIdx, blockSize, maxDictSize, displayLevel);

  /* set up zParams */
  ZDICT_params_t zParams;
  zParams.compressionLevel = cLevel;
  zParams.notificationLevel = displayLevel;
  zParams.dictID = dictID;

  /* with no dict */
  {
    const int noDictResult = benchmarkDictBuilder(srcInfo, maxDictSize, NULL, NULL, NULL);
    if(noDictResult) {
      result = 1;
      goto _cleanup;
    }
  }

  /* for random */
  {
    ZDICT_random_params_t randomParam;
    randomParam.zParams = zParams;
    randomParam.k = k;
    const int randomResult = benchmarkDictBuilder(srcInfo, maxDictSize, &randomParam, NULL, NULL);
    if(randomResult) {
      result = 1;
      goto _cleanup;
    }
  }

  /* for cover */
  {
    ZDICT_cover_params_t coverParam;
    memset(&coverParam, 0, sizeof(coverParam));
    coverParam.zParams = zParams;
    coverParam.splitPoint = 1.0;
    coverParam.d = d;
    coverParam.steps = 40;
    coverParam.nbThreads = 1;
    const int coverOptResult = benchmarkDictBuilder(srcInfo, maxDictSize, NULL, &coverParam, NULL);
    if(coverOptResult) {
      result = 1;
      goto _cleanup;
    }
  }

  /* for legacy */
  {
    ZDICT_legacy_params_t legacyParam;
    legacyParam.zParams = zParams;
    legacyParam.selectivityLevel = 9;
    const int legacyResult = benchmarkDictBuilder(srcInfo, maxDictSize, NULL, NULL, &legacyParam);
    if(legacyResult) {
      result = 1;
      goto _cleanup;
    }
  }

  /* Free allocated memory */
_cleanup:
  UTIL_freeFileList(extendedFileList, fileNamesBuf);
  freeSampleInfo(srcInfo);
  return result;
}
