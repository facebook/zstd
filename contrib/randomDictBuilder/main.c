#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* malloc, free, qsort */
#include <string.h>   /* strcmp, strlen */
#include <errno.h>    /* errno */
#include <ctype.h>
#include "random.h"
#include "fileio.h"   /* stdinmark, stdoutmark, ZSTD_EXTENSION */
#include "platform.h"         /* Large Files support */
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
#define DEFAULT_INPUTFILE ""
#define DEFAULT_k 200
#define DEFAULT_OUTPUTFILE "defaultDict"
#define DEFAULT_DICTID 0

#define SAMPLESIZE_MAX (128 KB)
#define RANDOM_MAX_SAMPLES_SIZE (sizeof(size_t) == 8 ? ((U32)-1) : ((U32)1 GB))
#define RANDOM_MEMMULT 9
static const size_t g_maxMemory = (sizeof(size_t) == 4) ? (2 GB - 64 MB) : ((size_t)(512 MB) << sizeof(size_t));

#define NOISELENGTH 32


/*-*************************************
*  Structs
***************************************/
typedef struct {
    U64 totalSizeToLoad;
    unsigned oneSampleTooLarge;
    unsigned nbSamples;
} fileStats;

typedef struct {
  const void* srcBuffer;
  const size_t *samplesSizes;
  size_t nbSamples;
}sampleInfo;


/*-*************************************
*  Commandline related functions
***************************************/
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

int RANDOM_trainFromFiles(const char* dictFileName, sampleInfo *info, unsigned maxDictSize,
                       ZDICT_random_params_t *params){
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
            free(dictBuffer);
        }
        /* save dict */
        DISPLAYLEVEL(2, "Save dictionary of size %u into file %s \n", (U32)dictSize, dictFileName);
        saveDict(dictFileName, dictBuffer, dictSize);
    }

    /* clean up */
    free(dictBuffer);
    return result;
}

sampleInfo* getSampleInfo(const char** fileNamesTable,
                  unsigned nbFiles, size_t chunkSize, unsigned maxDictSize, const unsigned displayLevel){
    fileStats const fs = getFileStats(fileNamesTable, nbFiles, chunkSize, displayLevel);
    size_t* const sampleSizes = (size_t*)malloc(fs.nbSamples * sizeof(size_t));
    size_t const memMult = RANDOM_MEMMULT;
    size_t const maxMem =  findMaxMem(fs.totalSizeToLoad * memMult) / memMult;
    size_t loadedSize = (size_t) MIN ((unsigned long long)maxMem, fs.totalSizeToLoad);
    void* const srcBuffer = malloc(loadedSize+NOISELENGTH);

    /* Checks */
    if ((!sampleSizes) || (!srcBuffer))
        EXM_THROW(12, "not enough memory for trainFromFiles");   /* should not happen */
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

    sampleInfo *info = (sampleInfo *)malloc(sizeof(sampleInfo));

    info->nbSamples = fs.nbSamples;
    info->samplesSizes = sampleSizes;
    info->srcBuffer = srcBuffer;

    return info;
}



int main(int argCount, const char* argv[])
{
  int displayLevel = 2;
  const char* programName = argv[0];
  int operationResult = 0;

  unsigned cLevel = DEFAULT_CLEVEL;
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
    if (longCommandWArg(&argument, "c=")) { cLevel = readU32FromChar(&argument); continue; }
    if (longCommandWArg(&argument, "dictID=")) { dictID = readU32FromChar(&argument); continue; }
    if (longCommandWArg(&argument, "maxdict=")) { maxDictSize = readU32FromChar(&argument); continue; }
    if (longCommandWArg(&argument, "in=")) {
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
  zParams.compressionLevel = cLevel;
  zParams.notificationLevel = displayLevel;
  zParams.dictID = dictID;
  params.zParams = zParams;
  params.k = k;

  sampleInfo* info= getSampleInfo(filenameTable,
                    filenameIdx, blockSize, maxDictSize, zParams.notificationLevel);
  operationResult = RANDOM_trainFromFiles(outputFile, info, maxDictSize, &params);

  return operationResult;
}
