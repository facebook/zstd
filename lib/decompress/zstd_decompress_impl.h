/*
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef FUNCTION
#  error "FUNCTION(name) must be defined"
#endif

#ifndef TARGET
#  error "TARGET must be defined"
#endif



static TARGET
size_t FUNCTION(ZSTD_decompressSequences)(
                               ZSTD_DCtx* dctx,
                               void* dst, size_t maxDstSize,
                         const void* seqStart, size_t seqSize, int nbSeq,
                         const ZSTD_longOffset_e isLongOffset)
{
    const BYTE* ip = (const BYTE*)seqStart;
    const BYTE* const iend = ip + seqSize;
    BYTE* const ostart = (BYTE* const)dst;
    BYTE* const oend = ostart + maxDstSize;
    BYTE* op = ostart;
    const BYTE* litPtr = dctx->litPtr;
    const BYTE* const litEnd = litPtr + dctx->litSize;
    const BYTE* const base = (const BYTE*) (dctx->base);
    const BYTE* const vBase = (const BYTE*) (dctx->vBase);
    const BYTE* const dictEnd = (const BYTE*) (dctx->dictEnd);
    DEBUGLOG(5, "ZSTD_decompressSequences");

    /* Regen sequences */
    if (nbSeq) {
        seqState_t seqState;
        dctx->fseEntropy = 1;
        { U32 i; for (i=0; i<ZSTD_REP_NUM; i++) seqState.prevOffset[i] = dctx->entropy.rep[i]; }
        CHECK_E(BIT_initDStream(&seqState.DStream, ip, iend-ip), corruption_detected);
        ZSTD_initFseState(&seqState.stateLL, &seqState.DStream, dctx->LLTptr);
        ZSTD_initFseState(&seqState.stateOffb, &seqState.DStream, dctx->OFTptr);
        ZSTD_initFseState(&seqState.stateML, &seqState.DStream, dctx->MLTptr);

        for ( ; (BIT_reloadDStream(&(seqState.DStream)) <= BIT_DStream_completed) && nbSeq ; ) {
            nbSeq--;
            {   seq_t const sequence = FUNCTION(ZSTD_decodeSequence)(&seqState, isLongOffset);
                size_t const oneSeqSize = ZSTD_execSequence(op, oend, sequence, &litPtr, litEnd, base, vBase, dictEnd);
                DEBUGLOG(6, "regenerated sequence size : %u", (U32)oneSeqSize);
                if (ZSTD_isError(oneSeqSize)) return oneSeqSize;
                op += oneSeqSize;
        }   }

        /* check if reached exact end */
        DEBUGLOG(5, "ZSTD_decompressSequences: after decode loop, remaining nbSeq : %i", nbSeq);
        if (nbSeq) return ERROR(corruption_detected);
        /* save reps for next block */
        { U32 i; for (i=0; i<ZSTD_REP_NUM; i++) dctx->entropy.rep[i] = (U32)(seqState.prevOffset[i]); }
    }

    /* last literal segment */
    {   size_t const lastLLSize = litEnd - litPtr;
        if (lastLLSize > (size_t)(oend-op)) return ERROR(dstSize_tooSmall);
        memcpy(op, litPtr, lastLLSize);
        op += lastLLSize;
    }

    return op-ostart;
}

static TARGET
size_t FUNCTION(ZSTD_decompressSequencesLong)(
                               ZSTD_DCtx* dctx,
                               void* dst, size_t maxDstSize,
                         const void* seqStart, size_t seqSize, int nbSeq,
                         const ZSTD_longOffset_e isLongOffset)
{
    const BYTE* ip = (const BYTE*)seqStart;
    const BYTE* const iend = ip + seqSize;
    BYTE* const ostart = (BYTE* const)dst;
    BYTE* const oend = ostart + maxDstSize;
    BYTE* op = ostart;
    const BYTE* litPtr = dctx->litPtr;
    const BYTE* const litEnd = litPtr + dctx->litSize;
    const BYTE* const prefixStart = (const BYTE*) (dctx->base);
    const BYTE* const dictStart = (const BYTE*) (dctx->vBase);
    const BYTE* const dictEnd = (const BYTE*) (dctx->dictEnd);

    /* Regen sequences */
    if (nbSeq) {
#define STORED_SEQS 4
#define STOSEQ_MASK (STORED_SEQS-1)
#define ADVANCED_SEQS 4
        seq_t sequences[STORED_SEQS];
        int const seqAdvance = MIN(nbSeq, ADVANCED_SEQS);
        seqState_t seqState;
        int seqNb;
        dctx->fseEntropy = 1;
        { U32 i; for (i=0; i<ZSTD_REP_NUM; i++) seqState.prevOffset[i] = dctx->entropy.rep[i]; }
        seqState.prefixStart = prefixStart;
        seqState.pos = (size_t)(op-prefixStart);
        seqState.dictEnd = dictEnd;
        CHECK_E(BIT_initDStream(&seqState.DStream, ip, iend-ip), corruption_detected);
        ZSTD_initFseState(&seqState.stateLL, &seqState.DStream, dctx->LLTptr);
        ZSTD_initFseState(&seqState.stateOffb, &seqState.DStream, dctx->OFTptr);
        ZSTD_initFseState(&seqState.stateML, &seqState.DStream, dctx->MLTptr);

        /* prepare in advance */
        for (seqNb=0; (BIT_reloadDStream(&seqState.DStream) <= BIT_DStream_completed) && (seqNb<seqAdvance); seqNb++) {
            sequences[seqNb] = FUNCTION(ZSTD_decodeSequenceLong)(&seqState, isLongOffset);
        }
        if (seqNb<seqAdvance) return ERROR(corruption_detected);

        /* decode and decompress */
        for ( ; (BIT_reloadDStream(&(seqState.DStream)) <= BIT_DStream_completed) && (seqNb<nbSeq) ; seqNb++) {
            seq_t const sequence = FUNCTION(ZSTD_decodeSequenceLong)(&seqState, isLongOffset);
            size_t const oneSeqSize = ZSTD_execSequenceLong(op, oend, sequences[(seqNb-ADVANCED_SEQS) & STOSEQ_MASK], &litPtr, litEnd, prefixStart, dictStart, dictEnd);
            if (ZSTD_isError(oneSeqSize)) return oneSeqSize;
            PREFETCH(sequence.match);  /* note : it's safe to invoke PREFETCH() on any memory address, including invalid ones */
            sequences[seqNb&STOSEQ_MASK] = sequence;
            op += oneSeqSize;
        }
        if (seqNb<nbSeq) return ERROR(corruption_detected);

        /* finish queue */
        seqNb -= seqAdvance;
        for ( ; seqNb<nbSeq ; seqNb++) {
            size_t const oneSeqSize = ZSTD_execSequenceLong(op, oend, sequences[seqNb&STOSEQ_MASK], &litPtr, litEnd, prefixStart, dictStart, dictEnd);
            if (ZSTD_isError(oneSeqSize)) return oneSeqSize;
            op += oneSeqSize;
        }

        /* save reps for next block */
        { U32 i; for (i=0; i<ZSTD_REP_NUM; i++) dctx->entropy.rep[i] = (U32)(seqState.prevOffset[i]); }
#undef STORED_SEQS
#undef STOSEQ_MASK
#undef ADVANCED_SEQS
    }

    /* last literal segment */
    {   size_t const lastLLSize = litEnd - litPtr;
        if (lastLLSize > (size_t)(oend-op)) return ERROR(dstSize_tooSmall);
        memcpy(op, litPtr, lastLLSize);
        op += lastLLSize;
    }

    return op-ostart;
}
