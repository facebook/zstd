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
#define XXH_NAMESPACE ZSTD_
#include "xxhash.h"

#include "zstd_internal.h" /* includes zstd.h */
#include "zstd_seekable.h"

typedef struct {
    U32 cSize;
    U32 dSize;
    U32 checksum;
} framelogEntry_t;

typedef struct {
    framelogEntry_t* entries;
    U32 size;
    U32 capacity;
} framelog_t;

struct ZSTD_seekable_CStream_s {
    ZSTD_CStream* cstream;
    framelog_t framelog;

    U32 frameCSize;
    U32 frameDSize;

    XXH64_state_t xxhState;

    U32 maxFrameSize;

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
    {   size_t const FRAMELOG_STARTING_CAPACITY = 16;
        zcs->framelog.entries =
                malloc(sizeof(framelogEntry_t) * FRAMELOG_STARTING_CAPACITY);
        if (zcs->framelog.entries == NULL) goto failed2;
        zcs->framelog.capacity = FRAMELOG_STARTING_CAPACITY;
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
    free(zcs->framelog.entries);
    free(zcs);

    return 0;
}

size_t ZSTD_seekable_initCStream(ZSTD_seekable_CStream* zcs,
                                 int compressionLevel,
                                 int checksumFlag,
                                 U32 maxFrameSize)
{
    zcs->framelog.size = 0;
    zcs->frameCSize = 0;
    zcs->frameDSize = 0;

    /* make sure maxFrameSize has a reasonable value */
    if (maxFrameSize > ZSTD_SEEKABLE_MAX_FRAME_DECOMPRESSED_SIZE) {
        return ERROR(compressionParameter_unsupported);
    }

    zcs->maxFrameSize = maxFrameSize
                                ? maxFrameSize
                                : ZSTD_SEEKABLE_MAX_FRAME_DECOMPRESSED_SIZE;

    zcs->checksumFlag = checksumFlag;
    if (zcs->checksumFlag) {
        XXH64_reset(&zcs->xxhState, 0);
    }

    zcs->writingSeekTable = 0;

    return ZSTD_initCStream(zcs->cstream, compressionLevel);
}

static size_t ZSTD_seekable_logFrame(ZSTD_seekable_CStream* zcs)
{
    if (zcs->framelog.size == ZSTD_SEEKABLE_MAXFRAMES)
        return ERROR(frameIndex_tooLarge);

    zcs->framelog.entries[zcs->framelog.size] = (framelogEntry_t)
        {
            .cSize = zcs->frameCSize,
            .dSize = zcs->frameDSize,
        };
    if (zcs->checksumFlag)
        zcs->framelog.entries[zcs->framelog.size].checksum =
                /* take lower 32 bits of digest */
                XXH64_digest(&zcs->xxhState) & 0xFFFFFFFFU;

    zcs->framelog.size++;
    /* grow the buffer if required */
    if (zcs->framelog.size == zcs->framelog.capacity) {
        /* exponential size increase for constant amortized runtime */
        size_t const newCapacity = zcs->framelog.capacity * 2;
        framelogEntry_t* const newEntries = realloc(zcs->framelog.entries,
                sizeof(framelogEntry_t) * newCapacity);

        if (newEntries == NULL) return ERROR(memory_allocation);

        zcs->framelog.entries = newEntries;
        zcs->framelog.capacity = newCapacity;
    }

    return 0;
}

size_t ZSTD_seekable_endFrame(ZSTD_seekable_CStream* zcs, ZSTD_outBuffer* output)
{
    size_t const prevOutPos = output->pos;
    /* end the frame */
    size_t ret = ZSTD_endStream(zcs->cstream, output);

    zcs->frameCSize += output->pos - prevOutPos;

    /* need to flush before doing the rest */
    if (ret) return ret;

    /* frame done */

    /* store the frame data for later */
    ret = ZSTD_seekable_logFrame(zcs);
    if (ret) return ret;

    /* reset for the next frame */
    zcs->frameCSize = 0;
    zcs->frameDSize = 0;

    ZSTD_resetCStream(zcs->cstream, 0);
    if (zcs->checksumFlag)
        XXH64_reset(&zcs->xxhState, 0);

    return 0;
}

size_t ZSTD_seekable_compressStream(ZSTD_seekable_CStream* zcs, ZSTD_outBuffer* output, ZSTD_inBuffer* input)
{
    const BYTE* const inBase = (const BYTE*) input->src + input->pos;
    size_t inLen = input->size - input->pos;

    inLen = MIN(inLen, (size_t)(zcs->maxFrameSize - zcs->frameDSize));

    /* if we haven't finished flushing the last frame, don't start writing a new one */
    if (inLen > 0) {
        ZSTD_inBuffer inTmp = { inBase, inLen, 0 };
        size_t const prevOutPos = output->pos;

        size_t const ret = ZSTD_compressStream(zcs->cstream, output, &inTmp);

        if (zcs->checksumFlag) {
            XXH64_update(&zcs->xxhState, inBase, inTmp.pos);
        }

        zcs->frameCSize += output->pos - prevOutPos;
        zcs->frameDSize += inTmp.pos;

        input->pos += inTmp.pos;

        if (ZSTD_isError(ret)) return ret;
    }

    if (zcs->maxFrameSize == zcs->frameDSize) {
        /* log the frame and start over */
        size_t const ret = ZSTD_seekable_endFrame(zcs, output);
        if (ZSTD_isError(ret)) return ret;

        /* get the client ready for the next frame */
        return (size_t)zcs->maxFrameSize;
    }

    return (size_t)(zcs->maxFrameSize - zcs->frameDSize);
}

static size_t ZSTD_seekable_seekTableSize(ZSTD_seekable_CStream* zcs)
{
    size_t const sizePerFrame = 8 + (zcs->checksumFlag?4:0);
    size_t const seekTableLen = ZSTD_skippableHeaderSize +
                                sizePerFrame * zcs->framelog.size +
                                ZSTD_seekTableFooterSize;

    return seekTableLen;
}

static size_t ZSTD_seekable_writeSeekTable(ZSTD_seekable_CStream* zcs, ZSTD_outBuffer* output)
{
    BYTE* op = (BYTE*) output->dst;
    BYTE tmp[4]; /* so that we can work with buffers too small to write a whole word to */

    /* repurpose
     * zcs->frameDSize: the current index in the table and
     * zcs->frameCSize: the amount of the table written so far
     *
     * This function is written this way so that if it has to return early
     * because of a small buffer, it can keep going where it left off.
     */

    size_t const sizePerFrame = 8 + (zcs->checksumFlag?4:0);
    size_t const seekTableLen = ZSTD_seekable_seekTableSize(zcs);

#define st_write32(x, o)                                                       \
    do {                                                                       \
        if (zcs->frameCSize < (o) + 4) {                                       \
            size_t const lenWrite = MIN(output->size - output->pos,            \
                                        (o) + 4 - zcs->frameCSize);            \
            MEM_writeLE32(tmp, (x));                                           \
            memcpy(op + output->pos, tmp + (zcs->frameCSize - (o)), lenWrite); \
            zcs->frameCSize += lenWrite;                                       \
            output->pos += lenWrite;                                           \
            if (lenWrite < 4) return seekTableLen - zcs->frameCSize;           \
        }                                                                      \
    } while (0)

    st_write32(ZSTD_MAGIC_SKIPPABLE_START | 0xE, 0);
    st_write32(seekTableLen - ZSTD_skippableHeaderSize, 4);

    while (zcs->frameDSize < zcs->framelog.size) {
        st_write32(zcs->framelog.entries[zcs->frameDSize].cSize,
                   ZSTD_skippableHeaderSize + sizePerFrame * zcs->frameDSize);
        st_write32(zcs->framelog.entries[zcs->frameDSize].dSize,
                   ZSTD_skippableHeaderSize + sizePerFrame * zcs->frameDSize + 4);
        if (zcs->checksumFlag) {
            st_write32(zcs->framelog.entries[zcs->frameDSize].checksum,
                       ZSTD_skippableHeaderSize + sizePerFrame * zcs->frameDSize + 8);
        }

        zcs->frameDSize++;
    }

    st_write32(zcs->framelog.size, seekTableLen - ZSTD_seekTableFooterSize);

    if (output->size - output->pos < 1) return seekTableLen - zcs->frameCSize;
    if (zcs->frameCSize < seekTableLen - 4) {
        BYTE sfd = 0;
        sfd |= (zcs->checksumFlag) << 7;

        op[output->pos] = sfd;
        output->pos++;
        zcs->frameCSize++;
    }

    st_write32(ZSTD_SEEKABLE_MAGICNUMBER, seekTableLen - 4);

    if (zcs->frameCSize != seekTableLen) return ERROR(GENERIC);
    return 0;

#undef st_write32
}

size_t ZSTD_seekable_endStream(ZSTD_seekable_CStream* zcs, ZSTD_outBuffer* output)
{
    if (!zcs->writingSeekTable && zcs->frameDSize) {
        const size_t endFrame = ZSTD_seekable_endFrame(zcs, output);
        if (ZSTD_isError(endFrame)) return endFrame;
        /* return an accurate size hint */
        if (endFrame) return endFrame + ZSTD_seekable_seekTableSize(zcs);
    }

    zcs->writingSeekTable = 1;

    return ZSTD_seekable_writeSeekTable(zcs, output);
}
