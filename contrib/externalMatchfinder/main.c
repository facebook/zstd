/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"
#include "zstd_errors.h"
#include "matchfinder.h" // simpleExternalMatchFinder

#define CHECK(res)                                      \
do  {                                                   \
    if (ZSTD_isError(res)) {                            \
        printf("ERROR: %s\n", ZSTD_getErrorName(res));  \
        return 1;                                       \
    }                                                   \
} while (0)                                             \

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: exampleMatchfinder <file>\n");
        return 1;
    }

    ZSTD_CCtx* const zc = ZSTD_createCCtx();

    int simpleExternalMatchState = 0xdeadbeef;

    // Here is the crucial bit of code!
    ZSTD_registerExternalMatchFinder(
        zc,
        &simpleExternalMatchState,
        simpleExternalMatchFinder
    );

    {
        size_t const res = ZSTD_CCtx_setParameter(zc, ZSTD_c_enableMatchFinderFallback, 1);
        CHECK(res);
    }

    FILE *f = fopen(argv[1], "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    long const srcSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* const src = malloc(srcSize + 1);
    fread(src, srcSize, 1, f);
    fclose(f);

    size_t const dstSize = ZSTD_compressBound(srcSize);
    char* const dst = malloc(dstSize);

    size_t const cSize = ZSTD_compress2(zc, dst, dstSize, src, srcSize);
    CHECK(cSize);

    char* const val = malloc(srcSize);
    assert(val);

    {
        size_t const res = ZSTD_decompress(val, srcSize, dst, cSize);
        CHECK(res);
    }

    if (memcmp(src, val, srcSize) == 0) {
        printf("Compression and decompression were successful!\n");
        printf("Original size: %lu\n", srcSize);
        printf("Compressed size: %lu\n", cSize);
        return 0;
    } else {
        printf("ERROR: input and validation buffers don't match!\n");
        for (int i = 0; i < srcSize; i++) {
            if (src[i] != val[i]) {
                printf("First bad index: %d\n", i);
                break;
            }
        }
        return 1;
    }

    ZSTD_freeCCtx(zc);
}
