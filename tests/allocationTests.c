/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include "../lib/common/allocations.h"
#include "../lib/common/pool.h"
#include "../lib/common/zstd_internal.h"

#define ASSERT_TRUE(p)                                                       \
  do {                                                                       \
    if (!(p)) {                                                              \
      printf("Assertion failed at %s:%d: %s\n", __FILE__, __LINE__, #p);     \
      return 1;                                                              \
    }                                                                        \
  } while (0)

#define ASSERT_FALSE(p) ASSERT_TRUE(!(p))

static int test_customCalloc2_overflow(void) {
    size_t const nmemb = SIZE_MAX / 2 + 1;
    size_t const size = 4;
    void* const ptr = ZSTD_customCalloc2(nmemb, size, ZSTD_defaultCMem);
    ASSERT_TRUE(ptr == NULL);
    return 0;
}

static int test_pool_create_overflow(void) {
    /* numThreads * sizeof(thread) should overflow */
    size_t const numThreads = SIZE_MAX / 2; 
    POOL_ctx* const ctx = POOL_create(numThreads, 1);
    ASSERT_TRUE(ctx == NULL);
    return 0;
}

int main(void) {
    int result = 0;
    
    printf("Testing ZSTD_customCalloc2 overflow protection...\n");
    if (test_customCalloc2_overflow()) {
        printf("FAILED: ZSTD_customCalloc2 overflow test\n");
        result = 1;
    } else {
        printf("SUCCESS: ZSTD_customCalloc2 overflow test\n");
    }

    printf("Testing POOL_create overflow protection...\n");
    if (test_pool_create_overflow()) {
        printf("FAILED: POOL_create overflow test\n");
        result = 1;
    } else {
        printf("SUCCESS: POOL_create overflow test\n");
    }

    if (result == 0) {
        printf("PASS: All allocation security tests\n");
    }

    return result;
}
