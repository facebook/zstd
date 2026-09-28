/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#define ZSTD_STATIC_LINKING_ONLY

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "fuzz_data_producer.h"
#include "fuzz_helpers.h"
#include "zstd_helpers.h"

/**
 * This fuzz target ensures that ZSTD_generateSequences() does not crash and
 * if it succeeds that ZSTD_compressSequences() round trips.
 */

static void testRoundTrip(ZSTD_CCtx* cctx, ZSTD_Sequence const* seqs, size_t nbSeqs, const void* src, size_t srcSize) {
  /* Compress the sequences with block delimiters */
  const size_t compressBound = ZSTD_compressBound(srcSize);
  void* dst = FUZZ_malloc(compressBound);
  FUZZ_ASSERT(dst);

  size_t compressedSize = ZSTD_compressSequences(cctx, dst, compressBound, seqs, nbSeqs, src, srcSize);
  FUZZ_ZASSERT(compressedSize);

  void* decompressed = FUZZ_malloc(srcSize);
  FUZZ_ASSERT(srcSize == 0 || decompressed);
  size_t decompressedSize = ZSTD_decompress(decompressed, srcSize, dst, compressedSize);
  FUZZ_ZASSERT(decompressedSize);
  FUZZ_ASSERT(decompressedSize == srcSize);
  if (srcSize != 0) {
    FUZZ_ASSERT(!memcmp(src, decompressed, srcSize));
  }

  free(decompressed);
  free(dst);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {

  FUZZ_dataProducer_t *producer = FUZZ_dataProducer_create(data, size);
  size = FUZZ_dataProducer_reserveDataPrefix(producer);

  ZSTD_CCtx* cctx = ZSTD_createCCtx();
  FUZZ_ASSERT(cctx);

  const size_t seqsCapacity = FUZZ_dataProducer_uint32Range(producer, 0, 2 * ZSTD_sequenceBound(size));
  ZSTD_Sequence* seqs = (ZSTD_Sequence*)FUZZ_malloc(sizeof(ZSTD_Sequence) * seqsCapacity);
  FUZZ_ASSERT(seqsCapacity == 0 || seqs);

  FUZZ_setRandomParameters(cctx, size, producer);
  FUZZ_ZASSERT(ZSTD_CCtx_setParameter(cctx, ZSTD_c_targetCBlockSize, 0));
  FUZZ_ZASSERT(ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, 0));

  const size_t nbSeqs = ZSTD_generateSequences(cctx, seqs, seqsCapacity, data, size);
  if (ZSTD_isError(nbSeqs)) {
    /* Allowed to error if the destination is too small */
    if (ZSTD_getErrorCode(nbSeqs) == ZSTD_error_dstSize_tooSmall) {
        FUZZ_ASSERT(seqsCapacity < ZSTD_sequenceBound(size));
    }
  } else {
    /* Ensure we round trip with and without block delimiters*/

    FUZZ_ZASSERT(ZSTD_CCtx_setParameter(cctx, ZSTD_c_blockDelimiters, ZSTD_sf_explicitBlockDelimiters));
    testRoundTrip(cctx, seqs, nbSeqs, data, size);

    const size_t nbMergedSeqs = ZSTD_mergeBlockDelimiters(seqs, nbSeqs);
    FUZZ_ASSERT(nbMergedSeqs <= nbSeqs);
    FUZZ_ZASSERT(ZSTD_CCtx_reset(cctx, ZSTD_reset_session_only));
    FUZZ_ZASSERT(ZSTD_CCtx_setParameter(cctx, ZSTD_c_blockDelimiters, ZSTD_sf_noBlockDelimiters));
    testRoundTrip(cctx, seqs, nbMergedSeqs, data, size);
  }

  free(seqs);
  ZSTD_freeCCtx(cctx);
  FUZZ_dataProducer_free(producer);
  return 0;
}
