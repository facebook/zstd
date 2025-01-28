/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_SHA_TEST_H
#define ZSTD_SHA_TEST_H

#include "../lib/common/sha256.h"

typedef struct {
    const void* input;
    size_t inputLen;
    const void* expected; /* len == ZSTD_SHA256_DIGEST_SIZE */
} ZSTD_SHA256_TestVector;

typedef struct {
    const ZSTD_SHA256_TestVector* vectors;
    size_t nbVectors;
} ZSTD_SHA256_TestVectorSet;

extern const ZSTD_SHA256_TestVectorSet ZSTD_SHA256_NIST_testVectorSet;

#endif /* ZSTD_SHA_TEST_H */
