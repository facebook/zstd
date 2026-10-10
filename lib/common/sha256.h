/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_SHA256_H
#define ZSTD_SHA256_H

#include "mem.h" /* size_t, uint8_t */

#define ZSTD_SHA256_DIGEST_SIZE 32

typedef struct {
  uint8_t digest[ZSTD_SHA256_DIGEST_SIZE];
} ZSTD_SHA256_Result;

/**
 * Returns the SHA-256 hash of the provided data.
 */
ZSTD_SHA256_Result ZSTD_SHA256_hash(const void* data, size_t len);

#endif /* ZSTD_SHA256_H */
