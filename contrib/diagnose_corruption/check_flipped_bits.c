/*
 * Copyright (c) 2019-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "zstd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
  char *input;
  size_t input_size;

  char *perturbed; /* same size as input */

  char *output;
  size_t output_size;

  ZSTD_DCtx* dctx;
} stuff_t;

static void free_stuff(stuff_t* stuff) {
  free(stuff->input);
  free(stuff->output);
  ZSTD_freeDCtx(stuff->dctx);
}

static void usage(void) {
  fprintf(stderr, "check_flipped_bits input_filename");
  exit(1);
}

static char* readFile(const char* filename, size_t* size) {
  struct stat statbuf;
  int ret;
  FILE* f;
  char *buf;
  size_t bytes_read;

  ret = stat(filename, &statbuf);
  if (ret != 0) {
    fprintf(stderr, "stat failed: %m\n");
    return NULL;
  }
  if ((statbuf.st_mode & S_IFREG) != S_IFREG) {
    fprintf(stderr, "Input must be regular file\n");
    return NULL;
  }

  *size = statbuf.st_size;

  f = fopen(filename, "r");
  if (f == NULL) {
    fprintf(stderr, "fopen failed: %m\n");
    return NULL;
  }

  buf = malloc(*size);
  if (buf == NULL) {
    fprintf(stderr, "malloc failed\n");
  }

  bytes_read = fread(buf, 1, *size, f);
  if (bytes_read != *size) {
    fprintf(stderr, "failed to read whole file\n");
    free(buf);
    return NULL;
  }

  ret = fclose(f);
  if (ret != 0) {
    fprintf(stderr, "fclose failed: %m\n");
    free(buf);
    return NULL;
  }

  return buf;
}

static int init_stuff(stuff_t* stuff, int argc, char *argv[]) {
  const char* input_filename;

  if (argc != 2) {
    usage();
  }

  input_filename = argv[1];
  stuff->input_size = 0;
  stuff->input = readFile(input_filename, &stuff->input_size);
  if (stuff->input == NULL) {
    fprintf(stderr, "Failed to read input file.\n");
    return 0;
  }

  stuff->perturbed = malloc(stuff->input_size);
  if (stuff->perturbed == NULL) {
    fprintf(stderr, "malloc failed.\n");
    return 0;
  }
  memcpy(stuff->perturbed, stuff->input, stuff->input_size);

  stuff->output_size = ZSTD_DStreamOutSize();
  stuff->output = malloc(stuff->output_size);
  if (stuff->output == NULL) {
    fprintf(stderr, "malloc failed.\n");
    return 0;
  }

  stuff->dctx = ZSTD_createDCtx();
  if (stuff->dctx == NULL) {
    return 0;
  }

  return 1;
}

static int test_decompress(stuff_t* stuff) {
  size_t ret;
  ZSTD_inBuffer in = {stuff->perturbed, stuff->input_size, 0};
  ZSTD_outBuffer out = {stuff->output, stuff->output_size, 0};
  ZSTD_DCtx* dctx = stuff->dctx;

  ZSTD_DCtx_reset(dctx, ZSTD_reset_session_only);
  ZSTD_DCtx_refDDict(dctx, NULL);

  while (in.pos != in.size) {
    out.pos = 0;
    ret = ZSTD_decompressStream(dctx, &out, &in);

    if (ZSTD_isError(ret)) {
      /*
      fprintf(
          stderr, "Decompression failed: %s\n", ZSTD_getErrorName(ret));
      */
      return 0;
    }
  }

  return 1;
}

static int perturb_bits(stuff_t* stuff) {
  size_t pos;
  size_t bit;
  for (pos = 0; pos < stuff->input_size; pos++) {
    unsigned char old_val = stuff->input[pos];
    if (pos % 1000 == 0) {
      fprintf(stderr, "Perturbing byte %zu\n", pos);
    }
    for (bit = 0; bit < 8; bit++) {
      unsigned char new_val = old_val ^ (1 << bit);
      stuff->perturbed[pos] = new_val;
      if (test_decompress(stuff)) {
        fprintf(
            stderr,
            "Flipping byte %zu bit %zu (0x%02x -> 0x%02x) "
            "produced a successful decompression!\n",
            pos, bit, old_val, new_val);
      }
    }
    stuff->perturbed[pos] = old_val;
  }
  return 1;
}

static int perturb_bytes(stuff_t* stuff) {
  size_t pos;
  size_t new_val;
  for (pos = 0; pos < stuff->input_size; pos++) {
    unsigned char old_val = stuff->input[pos];
    if (pos % 1000 == 0) {
      fprintf(stderr, "Perturbing byte %zu\n", pos);
    }
    for (new_val = 0; new_val < 256; new_val++) {
      stuff->perturbed[pos] = new_val;
      if (test_decompress(stuff)) {
        fprintf(
            stderr,
            "Changing byte %zu (0x%02x -> 0x%02x) "
            "produced a successful decompression!\n",
            pos, old_val, (unsigned char)new_val);
      }
    }
    stuff->perturbed[pos] = old_val;
  }
  return 1;
}

int main(int argc, char* argv[]) {
  stuff_t stuff;

  if(!init_stuff(&stuff, argc, argv)) {
    fprintf(stderr, "Failed to init.\n");
    return 1;
  }

  if (test_decompress(&stuff)) {
    fprintf(stderr, "Blob already decompresses successfully!\n");
    return 1;
  }

  perturb_bits(&stuff);

  perturb_bytes(&stuff);

  free_stuff(&stuff);
}