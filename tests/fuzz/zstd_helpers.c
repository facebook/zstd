/*
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 */

#define ZSTD_STATIC_LINKING_ONLY

#include "zstd_helpers.h"
#include "fuzz_helpers.h"
#include "zstd.h"

static void setRand(ZSTD_CCtx *cctx, ZSTD_cParameter param, unsigned min,
                    unsigned max, uint32_t *state) {
  unsigned const value = FUZZ_rand32(state, min, max);
  FUZZ_ZASSERT(ZSTD_CCtx_setParameter(cctx, param, value));
}

void FUZZ_setRandomParameters(ZSTD_CCtx *cctx, uint32_t *state)
{
    setRand(cctx, ZSTD_p_windowLog, ZSTD_WINDOWLOG_MIN, 23, state);
    setRand(cctx, ZSTD_p_hashLog, ZSTD_HASHLOG_MIN, 23, state);
    setRand(cctx, ZSTD_p_chainLog, ZSTD_CHAINLOG_MIN, 24, state);
    setRand(cctx, ZSTD_p_searchLog, ZSTD_SEARCHLOG_MIN, 9, state);
    setRand(cctx, ZSTD_p_minMatch, ZSTD_SEARCHLENGTH_MIN, ZSTD_SEARCHLENGTH_MAX,
            state);
    setRand(cctx, ZSTD_p_targetLength, ZSTD_TARGETLENGTH_MIN,
            ZSTD_TARGETLENGTH_MAX, state);
    setRand(cctx, ZSTD_p_compressionStrategy, ZSTD_fast, ZSTD_btultra, state);
    /* Select frame parameters */
    setRand(cctx, ZSTD_p_contentSizeFlag, 0, 1, state);
    setRand(cctx, ZSTD_p_checksumFlag, 0, 1, state);
    setRand(cctx, ZSTD_p_dictIDFlag, 0, 1, state);
    /* Select long distance matchig parameters */
    setRand(cctx, ZSTD_p_enableLongDistanceMatching, 0, 1, state);
    setRand(cctx, ZSTD_p_ldmHashLog, ZSTD_HASHLOG_MIN, 24, state);
    setRand(cctx, ZSTD_p_ldmMinMatch, ZSTD_LDM_MINMATCH_MIN,
            ZSTD_LDM_MINMATCH_MAX, state);
    setRand(cctx, ZSTD_p_ldmBucketSizeLog, 0, ZSTD_LDM_BUCKETSIZELOG_MAX,
            state);
    setRand(cctx, ZSTD_p_ldmHashEveryLog, 0,
            ZSTD_WINDOWLOG_MAX - ZSTD_HASHLOG_MIN, state);
}
