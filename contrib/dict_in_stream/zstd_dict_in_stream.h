/*
 * Copyright 2020 Sean Bartell.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */
#ifndef ZSTD_DICT_IN_STREAM_H
#define ZSTD_DICT_IN_STREAM_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stddef.h>   /* size_t */
#include "zstd.h"

#define ZSTD_DICT_IN_STREAM_MAGIC 0x184D2A5D
#define ZSTD_DICT_IN_STREAM_HEADER_SIZE 8

/*-****************************************************************************
*  Dictionary in Stream - Decompression HowTo
*
*  1. Read the header (ZSTD_DICT_IN_STREAM_HEADER_SIZE bytes).
*  2. Use ZSTD_dict_in_stream_getDataSize on the header,
*     to check how many data bytes to read.
*  3. Read data bytes.
*  4. Use ZSTD_dict_in_stream_getDictSize on the data,
*     to check the size of the decompressed dictionary.
*  5. Use ZSTD_dict_in_stream_getDict on the data,
*     to decompress the dictionary.
*
*  Instead of steps 4-5, you can also use ZSTD_dict_in_stream_createCDict or
*  ZSTD_dict_in_stream_createDDict.
*
* ****************************************************************************/

/*! ZSTD_dict_in_stream_getDataSize() :
 *  Given a dict_in_stream header, of size ZSTD_DICT_IN_STREAM_HEADER_SIZE,
 *  determine how many bytes of dictionary data follow the header.
 *  Returns an error code if this is an invalid header
 *  (which can be tested using ZSTD_isError()). */
ZSTDLIB_API size_t ZSTD_dict_in_stream_getDataSize(const void* src, size_t srcSize);

/*! ZSTD_dict_in_stream_getDictSize() :
 *  Given the (possibly compressed) dictionary data that follows a
 *  dict_in_stream header, determine the decompressed dictionary size.
 *  Returns an error code if this is invalid dictionary data
 *  (which can be tested using ZSTD_isError()). */
ZSTDLIB_API size_t ZSTD_dict_in_stream_getDictSize(const void* src, size_t srcSize);

/*! ZSTD_dict_in_stream_getDict() :
 *  Given the (possibly compressed) dictionary data that follows a
 *  dict_in_stream header, decompress the dictionary.
 *  Returns an error code on error (which can be tested using ZSTD_isError()). */
ZSTDLIB_API size_t ZSTD_dict_in_stream_getDict(void* dst, size_t dstCapacity,
                                         const void* src, size_t srcSize);

/*! ZSTD_dict_in_stream_createCDict() :
 *  Convenience function to load the dictionary as a CDict.
 *  Returns NULL on error or if this is not a valid dictionary. */
ZSTDLIB_API ZSTD_CDict* ZSTD_dict_in_stream_createCDict(const void* src, size_t srcSize,
                                                        int compressionLevel);

/*! ZSTD_dict_in_stream_createDDict() :
 *  Convenience function to load the dictionary as a DDict.
 *  Returns NULL on error or if this is not a valid dictionary. */
ZSTDLIB_API ZSTD_DDict* ZSTD_dict_in_stream_createDDict(const void* src, size_t srcSize);

/*-****************************************************************************
*  Dictionary in Stream - Compression HowTo
*
*  1. Use ZSTD_dict_in_stream_maxFrameSize on the dictionary,
*     to determine the maximum possible size of the dictionary frame.
*  2. Use ZSTD_dict_in_stream_createFrame to create the frame.
*  3. Write the resulting frame at the beginning of the file.
*
* ****************************************************************************/

/*! ZSTD_dict_in_stream_maxFrameSize() :
 *  Determine the maximum possible size of the dictionary frame needed to store
 *  a dictionary.
 *  Returns an error code if it fails (which can be tested using ZSTD_isError()). */
ZSTDLIB_API size_t ZSTD_dict_in_stream_maxFrameSize(const void* dict, size_t dictSize);

/*! ZSTD_dict_in_stream_createFrame() :
 *  Create a dictionary frame from a dictionary, with optional compression.
 *  compressionLevel can be 0 to disable compression.
 *  Returns the size of the frame,
 *  or an error code if it fails (which can be tested using ZSTD_isError()). */
ZSTDLIB_API size_t ZSTD_dict_in_stream_createFrame(void* dst, size_t dstCapacity,
                                             const void* dict, size_t dictSize,
                                                   int compressionLevel);

#if defined(__cplusplus)
}
#endif

#endif // ZSTD_DICT_IN_STREAM_H
