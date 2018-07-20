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
#define MEMMULT 11
#define NOISELENGTH 32

/*-*************************************
*  Struct
***************************************/
typedef struct {
  const void* dictBuffer;
  size_t dictSize;
} dictInfo;


/*-*************************************
*  Commandline related functions
***************************************/
static unsigned readU32FromChar(const char** stringPtr){
    const char errorMsg[] = "error: numeric value too large";
    unsigned result = 0;
    while ((**stringPtr >='0') && (**stringPtr <='9')) {
        unsigned const max = (((unsigned)(-1)) / 10) - 1;
        if (result > max) exit(1);
        result *= 10, result += **stringPtr - '0', (*stringPtr)++ ;
    }
    if ((**stringPtr=='K') || (**stringPtr=='M')) {
        unsigned const maxK = ((unsigned)(-1)) >> 10;
        if (result > maxK) exit(1);
        result <<= 10;
        if (**stringPtr=='M') {
            if (result > maxK) exit(1);
            result <<= 10;
        }
        (*stringPtr)++;  /* skip `K` or `M` */
        if (**stringPtr=='i') (*stringPtr)++;
        if (**stringPtr=='B') (*stringPtr)++;
    }
    return result;
}

/** longCommandWArg() :
 *  check if *stringPtr is the same as longCommand.
 *  If yes, @return 1 and advances *stringPtr to the position which immediately follows longCommand.
 * @return 0 and doesn't modify *stringPtr otherwise.
 */
static unsigned longCommandWArg(const char** stringPtr, const char* longCommand){
    size_t const comSize = strlen(longCommand);
    int const result = !strncmp(*stringPtr, longCommand, comSize);
    if (result) *stringPtr += comSize;
    return result;
}

static void fillNoise(void* buffer, size_t length)
{
    unsigned const prime1 = 2654435761U;
    unsigned const prime2 = 2246822519U;
    unsigned acc = prime1;
    size_t p=0;;

    for (p=0; p<length; p++) {
        acc *= prime2;
        ((unsigned char*)buffer)[p] = (unsigned char)(acc >> 21);
    }
}

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
                        0;   /* should never happen */
    void* const dictBuffer = malloc(maxDictSize);

    dictInfo* dInfo;

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
        } else {
          size_t totalSize= 0;
          for (int i = 0; i < info->nbSamples; i++) {
            totalSize += info->samplesSizes[i];
          }
          size_t const maxMem = findMaxMem(totalSize * MEMMULT) / MEMMULT;
          size_t loadedSize = (size_t) MIN ((unsigned long long)maxMem, totalSize);
          fillNoise((char*)(info->srcBuffer) + loadedSize, NOISELENGTH);
          dictSize = ZDICT_trainFromBuffer_unsafe_legacy(dictBuffer, maxDictSize, info->srcBuffer,
                                               info->samplesSizes, info->nbSamples, *legacyParams);
        }
        if (ZDICT_isError(dictSize)) {
            DISPLAYLEVEL(1, "dictionary training failed : %s \n", ZDICT_getErrorName(dictSize));   /* should not happen */
            free(dictBuffer);
            freeSampleInfo(info);
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
  double cRatio;
  size_t dstCapacity;
  int i;

  /* Pointers */
  ZSTD_CCtx* cctx;
  ZSTD_CDict *cdict;
  size_t *offsets;
  void* dst;

  /* Allocate dst with enough space to compress the maximum sized sample */
  {
    size_t maxSampleSize = 0;
    for (int i = 0; i < srcInfo->nbSamples; i++) {
      maxSampleSize = MAX(srcInfo->samplesSizes[i], maxSampleSize);
    }
    dstCapacity = ZSTD_compressBound(maxSampleSize);
    dst = malloc(dstCapacity);
  }

  /* Create the cctx and cdict */
  cctx = ZSTD_createCCtx();
  cdict = ZSTD_createCDict(dInfo->dictBuffer, dInfo->dictSize, compressionLevel);

  if(!cctx || !cdict || !dst) {
    cRatio = -1;
    goto _cleanup;
  }

  /* Calculate offset for each sample */
  offsets = (size_t *)malloc((srcInfo->nbSamples + 1) * sizeof(size_t));
  offsets[0] = 0;
  for (i = 1; i <= srcInfo->nbSamples; i++) {
    offsets[i] = offsets[i - 1] + srcInfo->samplesSizes[i - 1];
  }

  /* Compress each sample and sum their sizes*/
  const BYTE *const samples = (const BYTE *)srcInfo->srcBuffer;
  for (i = 0; i < srcInfo->nbSamples; i++) {
    const size_t compressedSize = ZSTD_compress_usingCDict(cctx, dst, dstCapacity, samples + offsets[i], srcInfo->samplesSizes[i], cdict);
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
  if(dst) {
    free(dst);
  }
  if(offsets) {
    free(offsets);
  }
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
/** benchmarkRandom() :
 *  Measure how long random dictionary builder takes and compression ratio with the random dictionary
 *  @return 0 if benchmark successfully, 1 otherwise
 */
int benchmarkRandom(sampleInfo *srcInfo, unsigned maxDictSize, ZDICT_random_params_t *randomParam) {
  const int displayLevel = randomParam->zParams.notificationLevel;
  int result = 0;
  clock_t t;
  t = clock();
  dictInfo* dInfo = createDictFromFiles(srcInfo, maxDictSize, randomParam, NULL, NULL);
  t = clock() - t;
  double time_taken = ((double)t)/CLOCKS_PER_SEC;
  if (!dInfo) {
    DISPLAYLEVEL(1, "RANDOM does not train successfully\n");
    result = 1;
    goto _cleanup;
  }
  DISPLAYLEVEL(2, "RANDOM took %f seconds to execute \n", time_taken);

  double cRatio = compressWithDict(srcInfo, dInfo, randomParam->zParams.compressionLevel, displayLevel);
  if (cRatio < 0) {
    DISPLAYLEVEL(1, "Compressing with RANDOM dictionary does not work\n");
    result = 1;
    goto _cleanup;
  }
  DISPLAYLEVEL(2, "Compression ratio with random dictionary is %f\n", cRatio);


_cleanup:
  freeDictInfo(dInfo);
  return result;
}

/** benchmarkCover() :
 *  Measure how long random dictionary builder takes and compression ratio with the cover dictionary
 *  @return 0 if benchmark successfully, 1 otherwise
 */
int benchmarkCover(sampleInfo *srcInfo, unsigned maxDictSize,
                ZDICT_cover_params_t *coverParam) {
  const int displayLevel = coverParam->zParams.notificationLevel;
  int result = 0;
  clock_t t;
  t = clock();
  dictInfo* dInfo = createDictFromFiles(srcInfo, maxDictSize, NULL, coverParam, NULL);
  t = clock() - t;
  double time_taken = ((double)t)/CLOCKS_PER_SEC;
  if (!dInfo) {
    DISPLAYLEVEL(1, "COVER does not train successfully\n");
    result = 1;
    goto _cleanup;
  }
  DISPLAYLEVEL(2, "COVER took %f seconds to execute \n", time_taken);

  double cRatio = compressWithDict(srcInfo, dInfo, coverParam->zParams.compressionLevel, displayLevel);
  if (cRatio < 0) {
    DISPLAYLEVEL(1, "Compressing with COVER dictionary does not work\n");
    result = 1;
    goto _cleanup;
  }
  DISPLAYLEVEL(2, "Compression ratio with cover dictionary is %f\n", cRatio);

_cleanup:
  freeDictInfo(dInfo);
  return result;
}



/** benchmarkLegacy() :
 *  Measure how long legacy dictionary builder takes and compression ratio with the legacy dictionary
 *  @return 0 if benchmark successfully, 1 otherwise
 */
int benchmarkLegacy(sampleInfo *srcInfo, unsigned maxDictSize, ZDICT_legacy_params_t *legacyParam) {
  const int displayLevel = legacyParam->zParams.notificationLevel;
  int result = 0;
  clock_t t;
  t = clock();
  dictInfo* dInfo = createDictFromFiles(srcInfo, maxDictSize, NULL, NULL, legacyParam);
  t = clock() - t;
  double time_taken = ((double)t)/CLOCKS_PER_SEC;
  if (!dInfo) {
    DISPLAYLEVEL(1, "LEGACY does not train successfully\n");
    result = 1;
    goto _cleanup;

  }
  DISPLAYLEVEL(2, "LEGACY took %f seconds to execute \n", time_taken);

  double cRatio = compressWithDict(srcInfo, dInfo, legacyParam->zParams.compressionLevel, displayLevel);
  if (cRatio < 0) {
    DISPLAYLEVEL(1, "Compressing with LEGACY dictionary does not work\n");
    result = 1;
    goto _cleanup;

  }
  DISPLAYLEVEL(2, "Compression ratio with legacy dictionary is %f\n", cRatio);

_cleanup:
  freeDictInfo(dInfo);
  return result;
}



int main(int argCount, const char* argv[])
{
  int displayLevel = 2;
  const char* programName = argv[0];
  int result = 0;
  /* Initialize arguments to default values */
  unsigned k = 200;
  unsigned d = 6;
  unsigned cLevel = 3;
  unsigned dictID = 0;
  unsigned maxDictSize = g_defaultMaxDictSize;

  /* Initialize table to store input files */
  const char** filenameTable = (const char**)malloc(argCount * sizeof(const char*));
  unsigned filenameIdx = 0;

  char* fileNamesBuf = NULL;
  unsigned fileNamesNb = filenameIdx;
  int followLinks = 0;
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

  size_t blockSize = 0;
  sampleInfo* srcInfo= getSampleInfo(filenameTable,
                    filenameIdx, blockSize, maxDictSize, displayLevel);

  /* set up zParams */
  ZDICT_params_t zParams;
  zParams.compressionLevel = cLevel;
  zParams.notificationLevel = displayLevel;
  zParams.dictID = dictID;

  /* for random */
  ZDICT_random_params_t randomParam;
  randomParam.zParams = zParams;
  randomParam.k = k;
  int randomResult = benchmarkRandom(srcInfo, maxDictSize, &randomParam);
  if(randomResult) {
    result = 1;
    goto _cleanup;
  }

  /* for cover */
  ZDICT_cover_params_t coverParam;
  memset(&coverParam, 0, sizeof(coverParam));
  coverParam.zParams = zParams;
  coverParam.splitPoint = 1.0;
  coverParam.d = d;
  coverParam.steps = 40;
  coverParam.nbThreads = 1;
  int coverOptResult = benchmarkCover(srcInfo, maxDictSize, &coverParam);
  if(coverOptResult) {
    result = 1;
    goto _cleanup;
  }

  /* for legacy */
  ZDICT_legacy_params_t legacyParam;
  legacyParam.zParams = zParams;
  legacyParam.selectivityLevel = 9;
  int legacyResult = benchmarkLegacy(srcInfo, maxDictSize, &legacyParam);
  if(legacyResult) {
    result = 1;
    goto _cleanup;
  }

  /* Free allocated memory */
_cleanup:
  UTIL_freeFileList(extendedFileList, fileNamesBuf);
  freeSampleInfo(srcInfo);
  return result;
}
