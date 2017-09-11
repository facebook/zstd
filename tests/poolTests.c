/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */


#include "pool.h"
#include "threading.h"
#include "util.h"
#include <stddef.h>
#include <stdio.h>

#define ASSERT_TRUE(p)                                                         \
  do {                                                                         \
    if (!(p)) {                                                                \
      return 1;                                                                \
    }                                                                          \
  } while (0)
#define ASSERT_FALSE(p) ASSERT_TRUE(!(p))
#define ASSERT_EQ(lhs, rhs) ASSERT_TRUE((lhs) == (rhs))

struct data {
  pthread_mutex_t mutex;
  unsigned data[16];
  size_t i;
};

void fn(void *opaque) {
  struct data *data = (struct data *)opaque;
  pthread_mutex_lock(&data->mutex);
  data->data[data->i] = data->i;
  ++data->i;
  pthread_mutex_unlock(&data->mutex);
}

int testOrder(size_t numThreads, size_t queueSize) {
  struct data data;
  POOL_ctx *ctx = POOL_create(numThreads, queueSize);
  ASSERT_TRUE(ctx);
  data.i = 0;
  pthread_mutex_init(&data.mutex, NULL);
  {
    size_t i;
    for (i = 0; i < 16; ++i) {
      POOL_add(ctx, &fn, &data);
    }
  }
  POOL_free(ctx);
  ASSERT_EQ(16, data.i);
  {
    size_t i;
    for (i = 0; i < data.i; ++i) {
      ASSERT_EQ(i, data.data[i]);
    }
  }
  pthread_mutex_destroy(&data.mutex);
  return 0;
}

void waitFn(void *opaque) {
  (void)opaque;
  UTIL_sleepMilli(1);
}

/* Tests for deadlock */
int testWait(size_t numThreads, size_t queueSize) {
  struct data data;
  POOL_ctx *ctx = POOL_create(numThreads, queueSize);
  ASSERT_TRUE(ctx);
  {
    size_t i;
    for (i = 0; i < 16; ++i) {
        POOL_add(ctx, &waitFn, &data);
    }
  }
  POOL_free(ctx);
  return 0;
}

int main(int argc, const char **argv) {
  size_t numThreads;
  for (numThreads = 1; numThreads <= 4; ++numThreads) {
    size_t queueSize;
    for (queueSize = 0; queueSize <= 2; ++queueSize) {
      if (testOrder(numThreads, queueSize)) {
        printf("FAIL: testOrder\n");
        return 1;
      }
      if (testWait(numThreads, queueSize)) {
        printf("FAIL: testWait\n");
        return 1;
      }
    }
  }
  printf("PASS: testOrder\n");
  (void)argc;
  (void)argv;
  return (POOL_create(0, 1)) ? printf("FAIL: testInvalid\n"), 1
                             : printf("PASS: testInvalid\n"), 0;
  return 0;
}
