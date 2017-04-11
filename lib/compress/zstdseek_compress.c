/**
 * Copyright (c) 2017-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <stdlib.h>     /* malloc, free */

#define XXH_STATIC_LINKING_ONLY
#include "xxhash.h"

#include "zstd_internal.h" /* includes zstd.h */
#include "seekable.h"

typedef struct {
    U32 cSize;
    U32 dSize;
    U32 checksum;
} chunklogEntry_t;

typedef struct {
    chunklogEntry_t* entries;
    U32 size;
    U32 capacity;
} chunklog_t;

struct ZSTD_seekable_CStream_s {
    ZSTD_CStream* cstream;
    chunklog_t chunklog;

    U32 chunkCSize;
    U32 chunkDSize;

    XXH64_state_t xxhState;

    U32 maxChunkSize;

    int checksumFlag;

    int writingSeekTable;
};

ZSTD_seekable_CStream* ZSTD_seekable_createCStream()
{
    ZSTD_seekable_CStream* zcs = malloc(sizeof(ZSTD_seekable_CStream));

    if (zcs == NULL) return NULL;

    memset(zcs, 0, sizeof(*zcs));

    zcs->cstream = ZSTD_createCStream();
    if (zcs->cstream == NULL) goto failed1;

    /* allocate some initial space */
    {   size_t const CHUNKLOG_STARTING_CAPACITY = 16;
        zcs->chunklog.entries =
                malloc(sizeof(chunklogEntry_t) * CHUNKLOG_STARTING_CAPACITY);
        if (zcs->chunklog.entries == NULL) goto failed2;
        zcs->chunklog.capacity = CHUNKLOG_STARTING_CAPACITY;
    }

    return zcs;

failed2:
    ZSTD_freeCStream(zcs->cstream);
failed1:
    free(zcs);
    return NULL;
}

size_t ZSTD_seekable_freeCStream(ZSTD_seekable_CStream* zcs)
{
    if (zcs == NULL) return 0; /* support free on null */
    ZSTD_freeCStream(zcs->cstream);
    free(zcs->chunklog.entries);
    free(zcs);

    return 0;
}

size_t ZSTD_seekable_initCStream(ZSTD_seekable_CStream* zcs,
                                 int compressionLevel,
                                 int checksumFlag,
                                 U32 maxChunkSize)
{
    zcs->chunklog.size = 0;
    zcs->chunkCSize = 0;
    zcs->chunkDSize = 0;

    /* make sure maxChunkSize has a reasonable value */
    if (maxChunkSize > ZSTD_SEEKABLE_MAX_CHUNK_DECOMPRESSED_SIZE) {
        return ERROR(compressionParameter_unsupported);
    }

    zcs->maxChunkSize = maxChunkSize
                                ? maxChunkSize
                                : ZSTD_SEEKABLE_MAX_CHUNK_DECOMPRESSED_SIZE;

    zcs->checksumFlag = checksumFlag;
    if (zcs->checksumFlag) {
        XXH64_reset(&zcs->xxhState, 0);
    }

    zcs->writingSeekTable = 0;

    return ZSTD_initCStream(zcs->cstream, compressionLevel);
}

static size_t ZSTD_seekable_logChunk(ZSTD_seekable_CStream* zcs)
{
    if (zcs->chunklog.size == ZSTD_SEEKABLE_MAXCHUNKS)
        return ERROR(chunkIndex_tooLarge);

    zcs->chunklog.entries[zcs->chunklog.size] = (chunklogEntry_t)
        {
            .cSize = zcs->chunkCSize,
            .dSize = zcs->chunkDSize,
        };
    if (zcs->checksumFlag)
        zcs->chunklog.entries[zcs->chunklog.size].checksum =
                /* take lower 32 bits of digest */
                XXH64_digest(&zcs->xxhState) & 0xFFFFFFFFU;

    zcs->chunklog.size++;
    /* grow the buffer if required */
    if (zcs->chunklog.size == zcs->chunklog.capacity) {
        /* exponential size increase for constant amortized runtime */
        size_t const newCapacity = zcs->chunklog.capacity * 2;
        chunklogEntry_t* const newEntries = realloc(zcs->chunklog.entries,
                sizeof(chunklogEntry_t) * newCapacity);

        if (newEntries == NULL) return ERROR(memory_allocation);

        zcs->chunklog.entries = newEntries;
        zcs->chunklog.capacity = newCapacity;
    }

    return 0;
}

size_t ZSTD_seekable_endChunk(ZSTD_seekable_CStream* zcs, ZSTD_outBuffer* output)
{
    size_t const prevOutPos = output->pos;
    /* end the frame */
    size_t ret = ZSTD_endStream(zcs->cstream, output);

    zcs->chunkCSize += output->pos - prevOutPos;

    /* need to flush before doing the rest */
    if (ret) return ret;

    /* frame done */

    /* store the chunk data for later */
    ret = ZSTD_seekable_logChunk(zcs);
    if (ret) return ret;

    /* reset for the next chunk */
    zcs->chunkCSize = 0;
    zcs->chunkDSize = 0;

    ZSTD_resetCStream(zcs->cstream, 0);
    if (zcs->checksumFlag)
        XXH64_reset(&zcs->xxhState, 0);

    return 0;
}

size_t ZSTD_seekable_compressStream(ZSTD_seekable_CStream* zcs, ZSTD_outBuffer* output, ZSTD_inBuffer* input)
{
    const BYTE* const inBase = (const BYTE*) input->src + input->pos;
    size_t inLen = input->size - input->pos;

    inLen = MIN(inLen, (size_t)(zcs->maxChunkSize - zcs->chunkDSize));

    /* if we haven't finished flushing the last chunk, don't start writing a new one */
    if (inLen > 0) {
        ZSTD_inBuffer inTmp = { inBase, inLen, 0 };
        size_t const prevOutPos = output->pos;

        size_t const ret = ZSTD_compressStream(zcs->cstream, output, &inTmp);

        if (zcs->checksumFlag) {
            XXH64_update(&zcs->xxhState, inBase, inTmp.pos);
        }

        zcs->chunkCSize += output->pos - prevOutPos;
        zcs->chunkDSize += inTmp.pos;

        input->pos += inTmp.pos;

        if (ZSTD_isError(ret)) return ret;
    }

    if (zcs->maxChunkSize == zcs->chunkDSize) {
        /* log the chunk and start over */
        size_t const ret = ZSTD_seekable_endChunk(zcs, output);
        if (ZSTD_isError(ret)) return ret;

        /* get the client ready for the next chunk */
        return (size_t)zcs->maxChunkSize;
    }

    return (size_t)(zcs->maxChunkSize - zcs->chunkDSize);
}

static size_t ZSTD_seekable_seekTableSize(ZSTD_seekable_CStream* zcs)
{
    size_t const sizePerChunk = 8 + (zcs->checksumFlag?4:0);
    size_t const seekTableLen = ZSTD_skippableHeaderSize +
                                sizePerChunk * zcs->chunklog.size +
                                ZSTD_seekTableFooterSize;

    return seekTableLen;
}

static size_t ZSTD_seekable_writeSeekTable(ZSTD_seekable_CStream* zcs, ZSTD_outBuffer* output)
{
    BYTE* op = (BYTE*) output->dst;
    BYTE tmp[4]; /* so that we can work with buffers too small to write a whole word to */

    /* repurpose
     * zcs->chunkDSize: the current index in the table and
     * zcs->chunkCSize: the amount of the table written so far
     *
     * This function is written this way so that if it has to return early
     * because of a small buffer, it can keep going where it left off.
     */

    size_t const sizePerChunk = 8 + (zcs->checksumFlag?4:0);
    size_t const seekTableLen = ZSTD_seekable_seekTableSize(zcs);

#define st_write32(x, o)                                                       \
    do {                                                                       \
        if (zcs->chunkCSize < (o) + 4) {                                       \
            size_t const lenWrite = MIN(output->size - output->pos,            \
                                        (o) + 4 - zcs->chunkCSize);            \
            MEM_writeLE32(tmp, (x));                                           \
            memcpy(op + output->pos, tmp + (zcs->chunkCSize - (o)), lenWrite); \
            zcs->chunkCSize += lenWrite;                                       \
            output->pos += lenWrite;                                           \
            if (lenWrite < 4) return seekTableLen - zcs->chunkCSize;           \
        }                                                                      \
    } while (0)

    st_write32(ZSTD_MAGIC_SKIPPABLE_START, 0);
    st_write32(seekTableLen - ZSTD_skippableHeaderSize, 4);

    while (zcs->chunkDSize < zcs->chunklog.size) {
        st_write32(zcs->chunklog.entries[zcs->chunkDSize].cSize,
                   ZSTD_skippableHeaderSize + sizePerChunk * zcs->chunkDSize);
        st_write32(zcs->chunklog.entries[zcs->chunkDSize].dSize,
                   ZSTD_skippableHeaderSize + sizePerChunk * zcs->chunkDSize + 4);
        if (zcs->checksumFlag) {
            st_write32(zcs->chunklog.entries[zcs->chunkDSize].checksum,
                       ZSTD_skippableHeaderSize + sizePerChunk * zcs->chunkDSize + 8);
        }

        zcs->chunkDSize++;
    }

    st_write32(zcs->chunklog.size, seekTableLen - ZSTD_seekTableFooterSize);

    if (output->size - output->pos < 1) return seekTableLen - zcs->chunkCSize;
    if (zcs->chunkCSize < seekTableLen - 4) {
        BYTE sfd = 0;
        sfd |= (zcs->checksumFlag) << 7;

        op[output->pos] = sfd;
        output->pos++;
        zcs->chunkCSize++;
    }

    st_write32(ZSTD_SEEKABLE_MAGICNUMBER, seekTableLen - 4);

    if (zcs->chunkCSize != seekTableLen) return ERROR(GENERIC);
    return 0;

#undef st_write32
}

size_t ZSTD_seekable_endStream(ZSTD_seekable_CStream* zcs, ZSTD_outBuffer* output)
{
    if (!zcs->writingSeekTable && zcs->chunkDSize) {
        const size_t endChunk = ZSTD_seekable_endChunk(zcs, output);
        if (ZSTD_isError(endChunk)) return endChunk;
        /* return an accurate size hint */
        if (endChunk) return endChunk + ZSTD_seekable_seekTableSize(zcs);
    }

    zcs->writingSeekTable = 1;

    return ZSTD_seekable_writeSeekTable(zcs, output);
}
