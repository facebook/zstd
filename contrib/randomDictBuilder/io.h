#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* malloc, free, qsort */
#include <string.h>   /* strcmp, strlen */
#include <errno.h>    /* errno */
#include <ctype.h>
#include "zstd_internal.h" /* includes zstd.h */
#include "fileio.h"   /* stdinmark, stdoutmark, ZSTD_EXTENSION */
#include "platform.h"         /* Large Files support */
#include "util.h"
#include "zdict.h"


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


sampleInfo* getSampleInfo(const char** fileNamesTable, unsigned nbFiles, size_t chunkSize,
                          unsigned maxDictSize, const unsigned displayLevel);


void saveDict(const char* dictFileName, const void* buff, size_t buffSize);
