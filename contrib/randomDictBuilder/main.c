#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* malloc, free, qsort */
#include <string.h>   /* strcmp, strlen */
#include <errno.h>    /* errno */
#include <ctype.h>
#include "fileio.h"   /* stdinmark, stdoutmark, ZSTD_EXTENSION */
#include "random.h"
#include "util.h"

#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (displayLevel>=l) { DISPLAY(__VA_ARGS__); }

static const unsigned g_defaultMaxDictSize = 110 KB;
#define DEFAULT_CLEVEL 3
#define DEFAULT_INPUTFILE ""
#define DEFAULT_k 200
#define DEFAULT_OUTPUTFILE "defaultDict"
#define DEFAULT_DICTID 0


static unsigned readU32FromChar(const char** stringPtr)
{
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
static unsigned longCommandWArg(const char** stringPtr, const char* longCommand)
{
    size_t const comSize = strlen(longCommand);
    int const result = !strncmp(*stringPtr, longCommand, comSize);
    if (result) *stringPtr += comSize;
    return result;
}


int main(int argCount, const char* argv[])
{
  int displayLevel = 2;
  const char* programName = argv[0];
  int operationResult = 0;

  /* Initialize parameters with default value */
  char* inputFile = DEFAULT_INPUTFILE;
  unsigned k = DEFAULT_k;
  char* outputFile = DEFAULT_OUTPUTFILE;
  unsigned dictID = DEFAULT_DICTID;
  unsigned maxDictSize = g_defaultMaxDictSize;

  const char** filenameTable = (const char**)malloc(argCount * sizeof(const char*));
  unsigned filenameIdx = 0;

  for (int i = 1; i < argCount; i++) {
    const char* argument = argv[i];
    if (longCommandWArg(&argument, "k=")) { k = readU32FromChar(&argument); continue; }
    if (longCommandWArg(&argument, "dictID=")) { dictID = readU32FromChar(&argument); continue; }
    if (longCommandWArg(&argument, "maxdict=")) { maxDictSize = readU32FromChar(&argument); continue; }
    if (longCommandWArg(&argument, "in=")) {
      /* Allow multiple input files */
      inputFile = malloc(strlen(argument) + 1);
      strcpy(inputFile, argument);
      filenameTable[filenameIdx] = inputFile;
      filenameIdx++;
      continue;
    }
    if (longCommandWArg(&argument, "out=")) {
      outputFile = malloc(strlen(argument) + 1);
      strcpy(outputFile, argument);
      continue;
    }
    DISPLAYLEVEL(1, "Incorrect parameters\n");
    operationResult = 1;
    return operationResult;
  }

  if (maxDictSize == 0) {
    DISPLAYLEVEL(1, "maxDictSize should not be 0.\n");
    operationResult = 1;
    return operationResult;
  }

  char* fileNamesBuf = NULL;
  unsigned fileNamesNb = filenameIdx;
  int followLinks = 0;
  const char** extendedFileList = NULL;
  extendedFileList = UTIL_createFileList(filenameTable, filenameIdx, &fileNamesBuf, &fileNamesNb, followLinks);
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

  operationResult = RANDOM_trainFromFiles(outputFile, maxDictSize, filenameTable, filenameIdx, blockSize, &params);
  return operationResult;
}
