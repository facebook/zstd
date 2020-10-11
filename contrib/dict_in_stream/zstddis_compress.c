/*
 * Copyright 2020 Sean Bartell.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"
#include "zstd_errors.h"
#include "mem.h"
#include "zstd_dict_in_stream.h"

#undef ERROR
#define ERROR(name) ((size_t)-ZSTD_error_##name)

size_t ZSTD_dict_in_stream_maxFrameSize(const void* dict, size_t dictSize)
{
    size_t result = dictSize + ZSTD_DICT_IN_STREAM_HEADER_SIZE;
    if (result < dictSize) // overflow
        return ERROR(parameter_outOfBound);
    return result;
}

size_t ZSTD_dict_in_stream_createFrame(void* dst, size_t dstCapacity,
                                 const void* dict, size_t dictSize,
                                       int compressionLevel)
{
    if (dstCapacity < ZSTD_DICT_IN_STREAM_HEADER_SIZE)
        return ERROR(dstSize_tooSmall);
    BYTE *dataDst = (BYTE*)dst + ZSTD_DICT_IN_STREAM_HEADER_SIZE;
    size_t dataCapacity = dstCapacity - ZSTD_DICT_IN_STREAM_HEADER_SIZE;
    size_t dataSize = ERROR(dstSize_tooSmall);
    if (compressionLevel != 0)
        dataSize = ZSTD_compress(dataDst, dataCapacity, dict, dictSize, compressionLevel);
    if (ZSTD_isError(dataSize) && dataCapacity >= dictSize) {
        memcpy(dataDst, dict, dictSize);
        dataCapacity = dictSize;
    }
    if (ZSTD_isError(dataSize))
        return dataSize;
    MEM_writeLE32(dst, ZSTD_DICT_IN_STREAM_MAGIC);
    MEM_writeLE32((BYTE*)dst + 4, dataSize);
    return dataSize + ZSTD_DICT_IN_STREAM_HEADER_SIZE;
}
