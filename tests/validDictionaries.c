#include <stdlib.h>
#include <stdio.h>
#include "zstd.h"
#include "dictBuilder/zdict.h"

#define SIZE 64*1024
#define DICT_SIZE 32*1024

int main(int argc, const char** argv) {
  ZSTD_CDict *cdict;
  ZSTD_DDict *ddict;
  size_t size, dict_size, i;
  unsigned char *samples = malloc(SIZE);
  size_t sample_sizes[1] =  { SIZE };
  void *dict = malloc(DICT_SIZE);
  size_t compress_bound = ZSTD_compressBound(SIZE);
  void *compressed = malloc(compress_bound);
  void *decompressed = malloc(SIZE);
  ZSTD_CCtx* cctx = ZSTD_createCCtx();
  ZSTD_DCtx* dctx = ZSTD_createDCtx();

  srand(0);
  for (i=0; i < SIZE; i++) {
      samples[i] = rand() % 32;
  }

  dict_size = ZDICT_trainFromBuffer(dict, DICT_SIZE, samples, sample_sizes, 1);

  if (ZDICT_isError(dict_size)) {
      puts(ZDICT_getErrorName(dict_size));
      return 1;
  } else {
    printf("Created dictionary of size %zu\n", dict_size);
  }

  size = ZSTD_compress_usingDict(cctx, compressed,  compress_bound, samples, SIZE, dict, dict_size, 1);
  if (ZSTD_isError(size)) {
      puts(ZSTD_getErrorName(size));
      return 2;
  } else {
      puts("Compressed with dict");
  }


  size = ZSTD_decompress_usingDict(dctx, decompressed, SIZE, compressed, size, dict, dict_size);
  if (ZSTD_isError(size)) {
      puts(ZSTD_getErrorName(size));
      return 3;
  } else {
      puts("Decompressed with dict");
  }

  if (size != SIZE) {
      puts("Corrupted with dict");
      return 4;
  }

  cdict = ZSTD_createCDict(dict, dict_size, 1);
  if (cdict) {
    puts("Created CDict");
  } else {
    puts("Failed to create CDict");
    return 5;
  }
  ddict = ZSTD_createDDict(dict, dict_size);
  if (ddict) {
    puts("Created DDict");
  } else {
    puts("Failed to create DDict");
    return 6;
  }


  size = ZSTD_compress_usingCDict(cctx, compressed,  compress_bound, samples, SIZE, cdict);
  if (ZSTD_isError(size)) {
      puts(ZSTD_getErrorName(size));
      return 7;
  } else {
      puts("Compressed with CDict");
  }


  size = ZSTD_decompress_usingDDict(dctx, decompressed, SIZE, compressed, size, ddict);
  if (ZSTD_isError(size)) {
      puts(ZSTD_getErrorName(size));
      return 8;
  } else {
      puts("Decompressed with DDict");
  }

  if (size != SIZE) {
      puts("Corrupted with CDict / DDict");
      return 9;
  }

  (void) argc;
  (void) argv;
  return 0;
}
