#include <stdlib.h>
#include <stdio.h>
#include "zstd.h"
#include "dictBuilder/zdict.h"

int main(int argc, const char** argv) {
  ZSTD_CDict *cdict;
  ZSTD_DDict *ddict;
  size_t dict_size, i;
  unsigned char *samples = malloc(64*1024);
  size_t sample_sizes[1] =  { 64*1024 };
  void *dict = malloc(32*1024);
  srand(0);
  for (i=0; i < 64*1024; i++) {
      samples[i] = rand() % 32;
  }

  dict_size = ZDICT_trainFromBuffer(dict, 32*1024, samples, sample_sizes, 1);

  if (ZDICT_isError(dict_size)) {
      puts(ZDICT_getErrorName(dict_size));
      return 1;
  }

  printf("Created dictionary of size %zu\n", dict_size);

  cdict = ZSTD_createCDict(dict, dict_size, 1);
  if (cdict) {
    puts("Created CDict");
  } else {
    puts("Failed to create CDict");
    return (2);
  }
  ddict = ZSTD_createDDict(dict, dict_size);
  if (ddict) {
    puts("Created DDict");
  } else {
    puts("Failed to create DDict");
    return (3);
  }

  (void) argc;
  (void) argv;
  return 0;
}
