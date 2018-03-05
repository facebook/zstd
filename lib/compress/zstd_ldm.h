/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 */

#ifndef ZSTD_LDM_H
#define ZSTD_LDM_H

#if defined (__cplusplus)
extern "C" {
#endif

#include "zstd_compress_internal.h"   /* ldmParams_t, U32 */
#include "zstd.h"   /* ZSTD_CCtx, size_t */

/*-*************************************
*  Long distance matching
***************************************/

#define ZSTD_LDM_DEFAULT_WINDOW_LOG ZSTD_WINDOWLOG_DEFAULTMAX
#define ZSTD_LDM_HASHEVERYLOG_NOTSET 9999

/**
 * ZSTD_ldm_generateSequences():
 *
 * Generates the sequences using the long distance match finder.
 * The sequences completely parse a prefix of the source, but leave off the last
 * literals. Returns the number of sequences generated into `sequences`. The
 * user must have called ZSTD_window_update() for all of the input they have,
 * even if they pass it to ZSTD_ldm_generateSequences() in chunks.
 *
 * NOTE: The source may be any size, assuming it doesn't overflow the hash table
 * indices, and the output sequences table is large enough..
 */
size_t ZSTD_ldm_generateSequences(
        ldmState_t* ldms, rawSeq* sequences,
        ldmParams_t const* params, void const* src, size_t srcSize,
        int const extDict);

/**
 * ZSTD_ldm_blockCompress():
 *
 * Compresses a block using the predefined sequences, along with a secondary
 * block compressor. The literals section of every sequence is passed to the
 * secondary block compressor, and those sequences are interspersed with the
 * predefined sequences. Returns the length of the last literals.
 * `nbSeq` is the number of sequences available in `sequences`.
 *
 * NOTE: The source must be at most the maximum block size, but the predefined
 * sequences can be any size, and may be longer than the block. In the case that
 * they are longer than the block, the last sequences may need to be split into
 * two. We handle that case correctly, and update `sequences` and `nbSeq`
 * appropriately.
 */
size_t ZSTD_ldm_blockCompress(rawSeq const* sequences, size_t nbSeq,
    ZSTD_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_REP_NUM],
    ZSTD_compressionParameters const* cParams, void const* src, size_t srcSize,
    int const extDict);


/** ZSTD_ldm_initializeParameters() :
 *  Initialize the long distance matching parameters to their default values. */
size_t ZSTD_ldm_initializeParameters(ldmParams_t* params, U32 enableLdm);

/** ZSTD_ldm_getTableSize() :
 *  Estimate the space needed for long distance matching tables or 0 if LDM is
 *  disabled.
 */
size_t ZSTD_ldm_getTableSize(ldmParams_t params);

/** ZSTD_ldm_getSeqSpace() :
 *  Return an upper bound on the number of sequences that can be produced by
 *  the long distance matcher, or 0 if LDM is disabled.
 */
size_t ZSTD_ldm_getMaxNbSeq(ldmParams_t params, size_t maxChunkSize);

/** ZSTD_ldm_getTableSize() :
 *  Return prime8bytes^(minMatchLength-1) */
U64 ZSTD_ldm_getHashPower(U32 minMatchLength);

/** ZSTD_ldm_adjustParameters() :
 *  If the params->hashEveryLog is not set, set it to its default value based on
 *  windowLog and params->hashLog.
 *
 *  Ensures that params->bucketSizeLog is <= params->hashLog (setting it to
 *  params->hashLog if it is not).
 *
 *  Ensures that the minMatchLength >= targetLength during optimal parsing.
 */
void ZSTD_ldm_adjustParameters(ldmParams_t* params,
                               ZSTD_compressionParameters const* cParams);

#if defined (__cplusplus)
}
#endif

#endif /* ZSTD_FAST_H */
