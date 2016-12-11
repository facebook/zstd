#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

void compress(ZSTD_CStream *ctx, ZSTD_outBuffer out, const void *data, size_t size) {
  ZSTD_inBuffer in = { data, size, 0 };
  while (in.pos < in.size) {
    ZSTD_outBuffer tmp = out;
    const size_t rc = ZSTD_compressStream(ctx, &tmp, &in);
    if (ZSTD_isError(rc)) {
      exit(5);
    }
  }
  ZSTD_outBuffer tmp = out;
  const size_t rc = ZSTD_flushStream(ctx, &tmp);
  if (rc != 0) { exit(6); }
}

int main() {
  ZSTD_CStream *ctx;
  ZSTD_parameters params = {};
  size_t rc;
  unsigned windowLog;
  /* Create stream */
  ctx = ZSTD_createCStream();
  if (!ctx) { return 1; }
  /* Set parameters */
  params.cParams.windowLog = 18;
  params.cParams.chainLog = 13;
  params.cParams.hashLog = 14;
  params.cParams.searchLog = 1;
  params.cParams.searchLength = 7;
  params.cParams.targetLength = 16;
  params.cParams.strategy = ZSTD_fast;
  windowLog = params.cParams.windowLog;
  /* Initialize stream */
  rc = ZSTD_initCStream_advanced(ctx, NULL, 0, params, 0);
  if (ZSTD_isError(rc)) { return 2; }
  {
    uint64_t compressed = 0;
    const uint64_t toCompress = ((uint64_t)1) << 33;
    const size_t size = 1 << windowLog;
    size_t pos = 0;
    char *srcBuffer = (char*) malloc(1 << windowLog);
    char *dstBuffer = (char*) malloc(ZSTD_compressBound(1 << windowLog));
    ZSTD_outBuffer out = { dstBuffer, ZSTD_compressBound(1 << windowLog), 0 };
    const char match[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const size_t randomData = (1 << windowLog) - 2*sizeof(match);
    for (size_t i = 0; i < sizeof(match); ++i) {
      srcBuffer[i] = match[i];
    }
    for (size_t i = 0; i < randomData; ++i) {
      srcBuffer[sizeof(match) + i] = (char)(rand() & 0xFF);
    }
    for (size_t i = 0; i < sizeof(match); ++i) {
      srcBuffer[sizeof(match) + randomData + i] = match[i];
    }
    compress(ctx, out, srcBuffer, size);
    compressed += size;
    while (compressed < toCompress) {
      const size_t block = rand() % (size - pos + 1);
      if (pos == size) { pos = 0; }
      compress(ctx, out, srcBuffer + pos, block);
      pos += block;
      compressed += block;
    }
    free(srcBuffer);
    free(dstBuffer);
  }
}
