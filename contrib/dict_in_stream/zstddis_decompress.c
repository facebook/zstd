/*
 * Copyright 2020 Sean Bartell.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include <stdlib.h>  // malloc, free

#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"
#include "zstd_errors.h"
#include "mem.h"
#include "zstd_dict_in_stream.h"

#undef ERROR
#define ERROR(name) ((size_t)-ZSTD_error_##name)

size_t ZSTD_dict_in_stream_getDataSize(const void* src, size_t srcSize)
{
    if (srcSize < ZSTD_DICT_IN_STREAM_HEADER_SIZE)
        return ERROR(srcSize_wrong);
    if (MEM_read32(src) != ZSTD_DICT_IN_STREAM_MAGIC)
        return ERROR(prefix_unknown);
    return MEM_read32((BYTE*)src + 4);
}

size_t ZSTD_dict_in_stream_getDictSize(const void* src, size_t srcSize)
{
    if (srcSize < 4)
        return ERROR(srcSize_wrong);
    if (MEM_read32(src) == ZSTD_MAGIC_DICTIONARY)
        return srcSize;
    unsigned long long result = ZSTD_getFrameContentSize(src, srcSize);
    if (result == ZSTD_CONTENTSIZE_UNKNOWN)
        return ERROR(dictionary_corrupted);
    if (result == ZSTD_CONTENTSIZE_ERROR)
        return ERROR(GENERIC);
    if ((size_t)result != result)
        return ERROR(frameParameter_windowTooLarge);
    return (size_t)result;
}

size_t ZSTD_dict_in_stream_getDict(void* dst, size_t dstCapacity,
                             const void* src, size_t srcSize)
{
    if (srcSize < 4)
        return ERROR(srcSize_wrong);
    if (MEM_read32(src) == ZSTD_MAGIC_DICTIONARY) {
        if (dstCapacity < srcSize)
            return ERROR(dstSize_tooSmall);
        memcpy(dst, src, srcSize);
        return srcSize;
    }
    if (MEM_read32(src) != ZSTD_MAGICNUMBER)
        return ERROR(prefix_unknown);
    if (ZSTD_findFrameCompressedSize(src, srcSize) != srcSize)
        return ERROR(srcSize_wrong);
    return ZSTD_decompress(dst, dstCapacity, src, srcSize);
}

ZSTD_CDict* ZSTD_dict_in_stream_createCDict(const void* src, size_t srcSize,
                                            int compressionLevel)
{
    size_t size = ZSTD_dict_in_stream_getDictSize(src, srcSize);
    if (ZSTD_isError(size))
        return NULL;
    void* buffer = malloc(size);
    if (!buffer)
        return NULL;
    size_t actualSize = ZSTD_dict_in_stream_getDict(buffer, size, src, srcSize);
    if (actualSize != size) {
        free(buffer);
        return NULL;
    }
    ZSTD_CDict* result = ZSTD_createCDict(buffer, actualSize, compressionLevel);
    free(buffer);
    return result;
}

ZSTD_DDict* ZSTD_dict_in_stream_createDDict(const void* src, size_t srcSize)
{
    size_t size = ZSTD_dict_in_stream_getDictSize(src, srcSize);
    if (ZSTD_isError(size))
        return NULL;
    void* buffer = malloc(size);
    if (!buffer)
        return NULL;
    size_t actualSize = ZSTD_dict_in_stream_getDict(buffer, size, src, srcSize);
    if (actualSize != size) {
        free(buffer);
        return NULL;
    }
    ZSTD_DDict* result = ZSTD_createDDict(buffer, actualSize);
    free(buffer);
    return result;
}
