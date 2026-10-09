/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include <zstd.h>
#include <zstd_errors.h>
#include <zdict.h>
#include <string.h>

int main()
{
    const char input[] = "CMake subdirectory consumer round trip";
    char compressed[128];
    char decompressed[sizeof(input)];
    const size_t compressedSize = ZSTD_compress(compressed, sizeof(compressed), input, sizeof(input), 1);
    if (ZSTD_isError(compressedSize)) return 1;

    const size_t decompressedSize = ZSTD_decompress(decompressed, sizeof(decompressed), compressed, compressedSize);
    if (ZSTD_isError(decompressedSize) || decompressedSize != sizeof(input)) return 1;
    if (memcmp(input, decompressed, sizeof(input)) != 0) return 1;

    if (ZSTD_getErrorCode(decompressedSize) != ZSTD_error_no_error) return 1;
    if (ZDICT_getDictID(input, sizeof(input)) != 0) return 1;
    return 0;
}
