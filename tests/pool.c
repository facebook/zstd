#include "pool.h"
#include "threading.h"
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
  unsigned data[1024];
  size_t i;
};

void fn(void *opaque) {
  struct data *data = (struct data *)opaque;
  pthread_mutex_lock(&data->mutex);
  data->data[data->i] = data->i;
  ++data->i;
  pthread_mutex_unlock(&data->mutex);
}

int testOrder(size_t numThreads, size_t queueLog) {
  struct data data;
  POOL_ctx *ctx = POOL_create(numThreads, queueLog);
  ASSERT_TRUE(ctx);
  data.i = 0;
  pthread_mutex_init(&data.mutex, NULL);
  {
    size_t i;
    for (i = 0; i < 1024; ++i) {
      POOL_add(ctx, &fn, &data);
    }
  }
  POOL_free(ctx);
  ASSERT_EQ(1024, data.i);
  {
    size_t i;
    for (i = 0; i < data.i; ++i) {
      ASSERT_EQ(i, data.data[i]);
    }
  }
  pthread_mutex_destroy(&data.mutex);
  return 0;
}

int main(int argc, const char **argv) {
  size_t numThreads;
  for (numThreads = 1; numThreads <= 8; ++numThreads) {
    size_t queueLog;
    for (queueLog = 1; queueLog <= 8; ++queueLog) {
      if (testOrder(numThreads, queueLog)) {
        printf("FAIL: testOrder\n");
        return 1;
      }
    }
  }
  printf("PASS: testOrder\n");
  (POOL_create(0, 1) || POOL_create(1, 0)) ? printf("FAIL: testInvalid\n")
                                           : printf("PASS: testInvalid\n");
  (void)argc;
  (void)argv;
  return 0;
}
