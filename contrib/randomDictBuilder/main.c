#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* malloc, free, qsort */
#include <string.h>   /* strcmp, strlen */
#include <errno.h>    /* errno */
#include <ctype.h>
#include "random.h"
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
#define DEFAULT_k 200
#define DEFAULT_OUTPUTFILE "defaultDict"
#define DEFAULT_DICTID 0



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



/*-*************************************
*  RANDOM
***************************************/
int RANDOM_trainFromFiles(const char* dictFileName, sampleInfo *info,
                          unsigned maxDictSize,
                          ZDICT_random_params_t *params) {
    unsigned const displayLevel = params->zParams.notificationLevel;
    void* const dictBuffer = malloc(maxDictSize);

    int result = 0;

    /* Checks */
    if (!dictBuffer)
        EXM_THROW(12, "not enough memory for trainFromFiles");   /* should not happen */

    {   size_t dictSize;
        dictSize = ZDICT_trainFromBuffer_random(dictBuffer, maxDictSize, info->srcBuffer,
                                             info->samplesSizes, info->nbSamples, *params);
        DISPLAYLEVEL(2, "k=%u\n", params->k);
        if (ZDICT_isError(dictSize)) {
            DISPLAYLEVEL(1, "dictionary training failed : %s \n", ZDICT_getErrorName(dictSize));   /* should not happen */
            result = 1;
            goto _done;
        }
        /* save dict */
        DISPLAYLEVEL(2, "Save dictionary of size %u into file %s \n", (U32)dictSize, dictFileName);
        saveDict(dictFileName, dictBuffer, dictSize);
    }

    /* clean up */
_done:
    free(dictBuffer);
    return result;
}



int main(int argCount, const char* argv[])
{
  int displayLevel = 2;
  const char* programName = argv[0];
  int operationResult = 0;

  /* Initialize arguments to default values */
  unsigned k = DEFAULT_k;
  const char* outputFile = DEFAULT_OUTPUTFILE;
  unsigned dictID = DEFAULT_DICTID;
  unsigned maxDictSize = g_defaultMaxDictSize;

  /* Initialize table to store input files */
  const char** filenameTable = (const char**)malloc(argCount * sizeof(const char*));
  unsigned filenameIdx = 0;

  /* Parse arguments */
  for (int i = 1; i < argCount; i++) {
    const char* argument = argv[i];
    if (longCommandWArg(&argument, "k=")) { k = readU32FromChar(&argument); continue; }
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
    DISPLAYLEVEL(1, "Incorrect parameters\n");
    operationResult = 1;
    return operationResult;
  }

  char* fileNamesBuf = NULL;
  unsigned fileNamesNb = filenameIdx;
  int followLinks = 0; /* follow directory recursively */
  const char** extendedFileList = NULL;
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

  ZDICT_random_params_t params;
  ZDICT_params_t zParams;
  zParams.compressionLevel = DEFAULT_CLEVEL;
  zParams.notificationLevel = displayLevel;
  zParams.dictID = dictID;
  params.zParams = zParams;
  params.k = k;

  sampleInfo* info= getSampleInfo(filenameTable,
                    filenameIdx, blockSize, maxDictSize, zParams.notificationLevel);
  operationResult = RANDOM_trainFromFiles(outputFile, info, maxDictSize, &params);

  /* Free allocated memory */
  UTIL_freeFileList(extendedFileList, fileNamesBuf);
  freeSampleInfo(info);

  return operationResult;
}
