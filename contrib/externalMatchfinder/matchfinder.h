#ifndef MATCHFINDER_H
#define MATCHFINDER_H

#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"

size_t simpleExternalMatchFinder(
  void* externalMatchState, ZSTD_Sequence* outSeqs, size_t outSeqsCapacity,
  const void* src, size_t srcSize, const void* dict, size_t dictSize
);

#endif
