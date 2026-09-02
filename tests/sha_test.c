/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

#include "sha_test.h"

static void test_tv(ZSTD_SHA256_TestVector tv) {
    const ZSTD_SHA256_Result result = ZSTD_SHA256_hash(tv.input, tv.inputLen);
    if (memcmp(result.digest, tv.expected, ZSTD_SHA256_DIGEST_SIZE)) {
        exit(1);
    }
}

int main(int argc, const char* argv[])
{
    size_t tvIdx;

    (void)argc;
    (void)argv;

    for (tvIdx = 0; tvIdx < ZSTD_SHA256_NIST_testVectorSet.nbVectors; tvIdx++) {
        ZSTD_SHA256_TestVector tv = ZSTD_SHA256_NIST_testVectorSet.vectors[tvIdx];
        test_tv(tv);
    }

    return 0;
}
