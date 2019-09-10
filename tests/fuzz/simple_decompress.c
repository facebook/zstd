/*
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 */

/**
 * This fuzz target attempts to decompress the fuzzed data with the simple
 * decompression function to ensure the decompressor never crashes.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "fuzz_helpers.h"
#include "zstd.h"
#include "fuzz_data_producer.h"

static ZSTD_DCtx *dctx = NULL;

int LLVMFuzzerTestOneInput(const uint8_t *src, size_t size)
{
  FUZZ_dataProducer_t *producer = FUZZ_dataProducer_create(src, size);

  if (!dctx) {
      dctx = ZSTD_createDCtx();
      FUZZ_ASSERT(dctx);
  }

  size_t const bufSize = FUZZ_dataProducer_uint32Range(producer, 0, 10 * size);
  void *rBuf = malloc(bufSize);
  FUZZ_ASSERT(rBuf);

  /* Restrict to remaining data. If we run out of data while generating params,
   we should still continue and let decompression happen on empty data. */
   size = FUZZ_dataProducer_remainingBytes(producer);

  ZSTD_decompressDCtx(dctx, rBuf, bufSize, src, size);
  free(rBuf);

  FUZZ_dataProducer_free(producer);

#ifndef STATEFUL_FUZZING
    ZSTD_freeDCtx(dctx); dctx = NULL;
#endif
    return 0;
}
