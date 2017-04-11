/*
 * Copyright (c) 2017-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <stdlib.h> /* malloc, free */

#define XXH_STATIC_LINKING_ONLY
#include "xxhash.h"

#include "zstd_internal.h" /* includes zstd.h */
#include "seekable.h"

typedef struct {
    U64 cOffset;
    U64 dOffset;
    U32 checksum;
} seekEntry_t;

typedef struct {
    seekEntry_t* entries;
    size_t tableLen;

    int checksumFlag;
} seekTable_t;

/** ZSTD_seekable_offsetToChunk() :
 *  Performs a binary search to find the last chunk with a decompressed offset
 *  <= pos
 *  @return : the chunk's index */
static U32 ZSTD_seekable_offsetToChunk(const seekTable_t* table, U64 pos)
{
    U32 lo = 0;
    U32 hi = table->tableLen;

    while (lo + 1 < hi) {
        U32 mid = lo + ((hi - lo) >> 1);
        if (table->entries[mid].dOffset <= pos) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}

/* Stream decompressor state machine stages */
enum ZSTD_seekable_DStream_stage {
    zsds_init = 0,
    zsds_seek,
    zsds_decompress,
    zsds_done,
};

struct ZSTD_seekable_DStream_s {
    ZSTD_DStream* dstream;
    seekTable_t seekTable;

    U32 curChunk;
    U64 compressedOffset;
    U64 decompressedOffset;

    U64 targetStart;
    U64 targetEnd;

    U64 nextSeek;

    enum ZSTD_seekable_DStream_stage stage;

    XXH64_state_t xxhState;
};

ZSTD_seekable_DStream* ZSTD_seekable_createDStream(void)
{
    ZSTD_seekable_DStream* zds = malloc(sizeof(ZSTD_seekable_DStream));

    if (zds == NULL) return NULL;

    /* also initializes stage to zsds_init */
    memset(zds, 0, sizeof(*zds));

    zds->dstream = ZSTD_createDStream();
    if (zds->dstream == NULL) {
        free(zds);
        return NULL;
    }

    return zds;
}

size_t ZSTD_seekable_freeDStream(ZSTD_seekable_DStream* zds)
{
    if (zds == NULL) return 0; /* support free on null */
    ZSTD_freeDStream(zds->dstream);
    free(zds->seekTable.entries);
    free(zds);

    return 0;
}

size_t ZSTD_seekable_loadSeekTable(ZSTD_seekable_DStream* zds, const void* src, size_t srcSize)
{
    const BYTE* ip = (const BYTE*)src + srcSize;

    U32 numChunks;
    int checksumFlag;

    U32 sizePerEntry;

    /* footer is fixed size */
    if (srcSize < ZSTD_seekTableFooterSize)
        return ZSTD_seekTableFooterSize;

    if (MEM_readLE32(ip - 4) != ZSTD_SEEKABLE_MAGICNUMBER) {
        return ERROR(prefix_unknown);
    }

    {   BYTE const sfd = ip[-5];
        checksumFlag = sfd >> 7;
    }

    numChunks = MEM_readLE32(ip-9);
    sizePerEntry = 8 + (checksumFlag?4:0);

    {   U32 const tableSize = sizePerEntry * numChunks;
        U32 const frameSize = tableSize + ZSTD_seekTableFooterSize + ZSTD_skippableHeaderSize;

        const BYTE* base = ip - frameSize;

        if (srcSize < frameSize) return frameSize;

        if ((MEM_readLE32(base) & 0xFFFFFFF0U) != ZSTD_MAGIC_SKIPPABLE_START) {
            return ERROR(prefix_unknown);
        }
        if (MEM_readLE32(base+4) + ZSTD_skippableHeaderSize != frameSize) {
            return ERROR(prefix_unknown);
        }

        {   /* Allocate an extra entry at the end so that we can do size
             * computations on the last element without special case */
            seekEntry_t* entries = malloc(sizeof(seekEntry_t) * (numChunks + 1));
            const BYTE* tableBase = base + ZSTD_skippableHeaderSize;

            U32 idx;
            size_t pos;

            U64 cOffset = 0;
            U64 dOffset = 0;

            if (!entries) {
                free(entries);
                return ERROR(memory_allocation);
            }

            /* compute cumulative positions */
            for (idx = 0, pos = 0; idx < numChunks; idx++) {
                entries[idx].cOffset = cOffset;
                entries[idx].dOffset = dOffset;

                cOffset += MEM_readLE32(tableBase + pos); pos += 4;
                dOffset += MEM_readLE32(tableBase + pos); pos += 4;
                if (checksumFlag) {
                    entries[idx].checksum = MEM_readLE32(tableBase + pos);
                    pos += 4;
                }
            }
            entries[numChunks].cOffset = cOffset;
            entries[numChunks].dOffset = dOffset;

            zds->seekTable.entries = entries;
            zds->seekTable.tableLen = numChunks;
            zds->seekTable.checksumFlag = checksumFlag;
            return 0;
        }
    }
}

size_t ZSTD_seekable_initDStream(ZSTD_seekable_DStream* zds, U64 rangeStart, U64 rangeEnd)
{
    /* restrict range to the end of the file, of non-negative size */
    rangeStart = MIN(rangeStart, zds->seekTable.entries[zds->seekTable.tableLen].dOffset);
    rangeEnd = MIN(rangeEnd, zds->seekTable.entries[zds->seekTable.tableLen].dOffset);
    rangeEnd = MAX(rangeEnd, rangeStart);

    zds->targetStart = rangeStart;
    zds->targetEnd = rangeEnd;
    zds->stage = zsds_seek;

    /* force a seek first */
    zds->curChunk = (U32) -1;
    zds->compressedOffset = (U64) -1;
    zds->decompressedOffset = (U64) -1;

    if (zds->seekTable.checksumFlag) {
        XXH64_reset(&zds->xxhState, 0);
    }

    if (rangeStart == rangeEnd) zds->stage = zsds_done;

    {   const size_t ret = ZSTD_initDStream(zds->dstream);
        if (ZSTD_isError(ret)) return ret; }
    return 0;
}

U64 ZSTD_seekable_getSeekOffset(ZSTD_seekable_DStream* zds)
{
    return zds->nextSeek;
}

size_t ZSTD_seekable_updateOffset(ZSTD_seekable_DStream* zds, U64 offset)
{
    if (zds->stage != zsds_seek) {
        return ERROR(stage_wrong);
    }
    if (offset != zds->nextSeek) {
        return ERROR(needSeek);
    }

    zds->stage = zsds_decompress;
    zds->compressedOffset = offset;
    return 0;
}

size_t ZSTD_seekable_decompressStream(ZSTD_seekable_DStream* zds, ZSTD_outBuffer* output, ZSTD_inBuffer* input)
{
    const seekTable_t* const jt = &zds->seekTable;
    while (1) {
        switch (zds->stage) {
        case zsds_init:
            return ERROR(init_missing); /* ZSTD_seekable_initDStream should be called first */
        case zsds_decompress: {
            BYTE* const outBase = (BYTE*)output->dst + output->pos;
            size_t const outLen = output->size - output->pos;
            while (zds->decompressedOffset < zds->targetStart) {
                U64 const toDecompress =
                        zds->targetStart - zds->decompressedOffset;
                size_t const prevInputPos = input->pos;

                ZSTD_outBuffer outTmp = {
                        .dst = outBase,
                        .size = (size_t)MIN((U64)outLen, toDecompress),
                        .pos = 0};

                size_t const ret =
                        ZSTD_decompressStream(zds->dstream, &outTmp, input);

                if (ZSTD_isError(ret)) return ret;
                if (ret == 0) {
                    /* should not happen at this stage */
                    return ERROR(corruption_detected);
                }

                zds->compressedOffset += input->pos - prevInputPos;
                zds->decompressedOffset += outTmp.pos;

                if (jt->checksumFlag) {
                    XXH64_update(&zds->xxhState, outTmp.dst, outTmp.pos);
                }

                if (input->pos == input->size) {
                    /* need more input */
                    return MIN(
                            ZSTD_DStreamInSize(),
                            (size_t)(jt->entries[zds->curChunk + 1]
                                             .cOffset -
                                     zds->compressedOffset));
                }
            }

            /* do actual decompression */
            {
                U64 const toDecompress =
                        MIN(zds->targetEnd,
                            jt->entries[zds->curChunk + 1].dOffset) -
                        zds->decompressedOffset;
                size_t const prevInputPos = input->pos;

                ZSTD_outBuffer outTmp = {
                        .dst = outBase,
                        .size = (size_t)MIN((U64)outLen, toDecompress),
                        .pos = 0};

                size_t const ret =
                        ZSTD_decompressStream(zds->dstream, &outTmp, input);

                if (ZSTD_isError(ret)) return ret;

                zds->compressedOffset += input->pos - prevInputPos;
                zds->decompressedOffset += outTmp.pos;

                output->pos += outTmp.pos;

                if (jt->checksumFlag) {
                    XXH64_update(&zds->xxhState, outTmp.dst, outTmp.pos);
                    if (ret == 0) {
                        /* verify the checksum */
                        U32 const digest = XXH64_digest(&zds->xxhState) & 0xFFFFFFFFU;
                        if (digest != jt->entries[zds->curChunk].checksum) {
                            return ERROR(checksum_wrong);
                        }

                        XXH64_reset(&zds->xxhState, 0);
                    }
                }

                if (zds->decompressedOffset == zds->targetEnd) {
                    /* done */
                    zds->stage = zsds_done;
                    return 0;
                }

                if (ret == 0) {
                    /* frame is done */
                    /* make sure this lines up with the expected frame border */
                    if (zds->decompressedOffset !=
                                jt->entries[zds->curChunk + 1].dOffset ||
                        zds->compressedOffset !=
                                jt->entries[zds->curChunk + 1].cOffset)
                        return ERROR(corruption_detected);
                    ZSTD_resetDStream(zds->dstream);
                    zds->stage = zsds_seek;
                    break;
                }

                /* need more input */
                return MIN(ZSTD_DStreamInSize(), (size_t)(
                        jt->entries[zds->curChunk + 1].cOffset -
                        zds->compressedOffset));
            }
        }
        case zsds_seek: {
            U32 targetChunk;
            if (zds->decompressedOffset < zds->targetStart ||
                    zds->decompressedOffset >= zds->targetEnd) {
                /* haven't started yet */
                targetChunk = ZSTD_seekable_offsetToChunk(jt, zds->targetStart);
            } else {
                targetChunk = ZSTD_seekable_offsetToChunk(jt, zds->decompressedOffset);
            }

            zds->curChunk = targetChunk;

            if (zds->compressedOffset == jt->entries[targetChunk].cOffset) {
                zds->stage = zsds_decompress;
                break;
            }

            zds->nextSeek = jt->entries[targetChunk].cOffset;
            zds->decompressedOffset = jt->entries[targetChunk].dOffset;
            /* signal to user that a seek is required */
            return ERROR(needSeek);
        }
        case zsds_done:
            return 0;
        }
    }
}
