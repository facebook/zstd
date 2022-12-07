#ifndef EXTERNAL_MATCHFINDER
#define EXTERNAL_MATCHFINDER

#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"

/* See external_matchfinder.c for details on each test case */
typedef enum {
    EMF_ZERO_SEQS = 0,
    EMF_ONE_BIG_SEQ = 1,
    EMF_LOTS_OF_SEQS = 2,
    EMF_BIG_ERROR = 3,
    EMF_SMALL_ERROR = 4
} EMF_testCase;

size_t zstreamExternalMatchFinder(
  void* externalMatchState,
  ZSTD_Sequence* outSeqs, size_t outSeqsCapacity,
  const void* src, size_t srcSize,
  const void* dict, size_t dictSize,
  int compressionLevel
);

#endif // EXTERNAL_MATCHFINDER
