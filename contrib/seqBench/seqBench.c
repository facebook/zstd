#define ZSTD_STATIC_LINKING_ONLY
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zstd.h>

int main(int argc, char *argv[]) {
  int ret = 0;
  FILE *f = NULL;
  char *inBuf = NULL;
  ZSTD_Sequence *seqs = NULL;
  char *outBuf = NULL;
  char *validationBuf = NULL;
  ZSTD_CCtx *zc = ZSTD_createCCtx();

  if (!zc) {
    fprintf(stderr, "ERROR: ZSTD_createCCtx failed\n");
    return 1;
  }

  if (argc != 2) {
    fprintf(stderr, "Usage: seqBench <file>\n");
    ret = 1;
    goto cleanup;
  }

  f = fopen(argv[1], "rb");
  if (!f) {
    fprintf(stderr, "ERROR: Could not open %s\n", argv[1]);
    ret = 1;
    goto cleanup;
  }
  fseek(f, 0, SEEK_END);
  long inBufSize = ftell(f);
  fseek(f, 0, SEEK_SET);

  inBuf = malloc(inBufSize + 1);
  if (!inBuf) {
    fprintf(stderr, "ERROR: malloc failed for inBuf\n");
    ret = 1;
    goto cleanup;
  }

  if (fread(inBuf, 1, inBufSize, f) != (size_t)inBufSize) {
    fprintf(stderr, "ERROR: fread failed to read full file\n");
    ret = 1;
    goto cleanup;
  }
  fclose(f);
  f = NULL;

  size_t seqsSize = ZSTD_sequenceBound(inBufSize);
  seqs = (ZSTD_Sequence *)malloc(seqsSize * sizeof(ZSTD_Sequence));
  if (!seqs) {
    fprintf(stderr, "ERROR: malloc failed for seqs\n");
    ret = 1;
    goto cleanup;
  }

  outBuf = malloc(ZSTD_compressBound(inBufSize));
  if (!outBuf) {
    fprintf(stderr, "ERROR: malloc failed for outBuf\n");
    ret = 1;
    goto cleanup;
  }

  ZSTD_generateSequences(zc, seqs, seqsSize, inBuf, inBufSize);
  ZSTD_CCtx_setParameter(zc, ZSTD_c_blockDelimiters,
                         ZSTD_sf_explicitBlockDelimiters);
  size_t outBufSize = ZSTD_compressSequences(zc, outBuf, inBufSize, seqs,
                                             seqsSize, inBuf, inBufSize);
  if (ZSTD_isError(outBufSize)) {
    fprintf(stderr, "ERROR: %s\n", ZSTD_getErrorName(outBufSize));
    ret = 1;
    goto cleanup;
  }

  validationBuf = malloc(inBufSize);
  if (!validationBuf) {
    fprintf(stderr, "ERROR: malloc failed for validationBuf\n");
    ret = 1;
    goto cleanup;
  }
  ZSTD_decompress(validationBuf, inBufSize, outBuf, outBufSize);

  if (memcmp(inBuf, validationBuf, inBufSize) == 0) {
    printf("Compression and decompression were successful!\n");
    printf("Original size:   %ld bytes\n", inBufSize);
    printf("Compressed size: %zu bytes\n", outBufSize);
    if (outBufSize > 0) {
      printf("Ratio:           %.2f\n", (double)inBufSize / outBufSize);
    }
  } else {
    fprintf(stderr, "ERROR: input and validation buffers don't match!\n");
    for (int i = 0; i < inBufSize; i++) {
      if (inBuf[i] != validationBuf[i]) {
        fprintf(stderr, "First bad index: %d\n", i);
        break;
      }
    }
    ret = 1;
  }

cleanup:
  if (f)
    fclose(f);
  free(inBuf);
  free(seqs);
  free(outBuf);
  free(validationBuf);
  ZSTD_freeCCtx(zc);

  return ret;
}
