/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* This example demonstrates streaming compression with a dictionary.
 * It combines the streaming approach from streaming_compression.c
 * with the dictionary approach from dictionary_compression.c.
 *
 * This is useful when compressing many small files (e.g. database records,
 * JSON objects, log entries) in a streaming fashion using a pre-trained
 * dictionary for better compression ratios on small data.
 *
 * It uses the advanced API:
 *   - ZSTD_CCtx_loadDictionary() to load the dictionary
 *   - ZSTD_compressStream2() to stream-compress the input
 *
 * Usage: streaming_dictionary_compression DICT FILES...
 */

#include "common.h" // Helper functions, CHECK(), and CHECK_ZSTD()
#include <stdio.h>  // printf
#include <stdlib.h> // free
#include <string.h> // memset, strcat, strlen
#include <zstd.h>   // presumes zstd library is installed

static void compressFile_orDie(const char *fname, const char *outName,
                               const void *dictBuffer, size_t dictSize,
                               int cLevel) {
  /* Open the input and output files. */
  FILE *const fin = fopen_orDie(fname, "rb");
  FILE *const fout = fopen_orDie(outName, "wb");

  /* Create the input and output buffers.
   * They may be any size, but we recommend using these functions to size them.
   */
  size_t const buffInSize = ZSTD_CStreamInSize();
  void *const buffIn = malloc_orDie(buffInSize);
  size_t const buffOutSize = ZSTD_CStreamOutSize();
  void *const buffOut = malloc_orDie(buffOutSize);

  /* Create the compression context. */
  ZSTD_CCtx *const cctx = ZSTD_createCCtx();
  CHECK(cctx != NULL, "ZSTD_createCCtx() failed!");

  /* Set compression parameters. */
  CHECK_ZSTD(ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, cLevel));
  CHECK_ZSTD(ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1));

  /* Load the dictionary.
   * The dictionary will be used for all subsequent compressions using this
   * context, until it is reset or a new dictionary is loaded.
   * ZSTD_CCtx_loadDictionary() makes an internal copy of the dictionary,
   * so we can free dictBuffer after this call if we wanted to.
   */
  CHECK_ZSTD(ZSTD_CCtx_loadDictionary(cctx, dictBuffer, dictSize));

  /* Stream-compress the input file. */
  size_t const toRead = buffInSize;
  for (;;) {
    size_t read = fread_orDie(buffIn, toRead, fin);
    int const lastChunk = (read < toRead);
    ZSTD_EndDirective const mode = lastChunk ? ZSTD_e_end : ZSTD_e_continue;
    ZSTD_inBuffer input = {buffIn, read, 0};
    int finished;
    do {
      ZSTD_outBuffer output = {buffOut, buffOutSize, 0};
      size_t const remaining =
          ZSTD_compressStream2(cctx, &output, &input, mode);
      CHECK_ZSTD(remaining);
      fwrite_orDie(buffOut, output.pos, fout);
      finished = lastChunk ? (remaining == 0) : (input.pos == input.size);
    } while (!finished);
    CHECK(input.pos == input.size, "Impossible: zstd only returns 0 when the "
                                   "input is completely consumed!");
    if (lastChunk) {
      break;
    }
  }

  ZSTD_freeCCtx(cctx);
  fclose_orDie(fout);
  fclose_orDie(fin);
  free(buffIn);
  free(buffOut);
}

static char *createOutFilename_orDie(const char *filename) {
  size_t const inL = strlen(filename);
  size_t const outL = inL + 5;
  void *outSpace = malloc_orDie(outL);
  memset(outSpace, 0, outL);
  strcat(outSpace, filename);
  strcat(outSpace, ".zst");
  return (char *)outSpace;
}

int main(int argc, const char **argv) {
  const char *const exeName = argv[0];
  int const cLevel = 3;

  if (argc < 3) {
    fprintf(stderr, "wrong arguments\n");
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "%s DICT [FILES...]\n", exeName);
    fprintf(stderr,
            "\nCompress FILES using streaming mode with a dictionary.\n");
    fprintf(stderr, "DICT is a dictionary file created with `zstd --train`.\n");
    return 1;
  }

  /* Load dictionary into memory.
   * The dictionary is loaded once and reused for all files. */
  const char *const dictName = argv[1];
  size_t dictSize;
  void *const dictBuffer = mallocAndLoadFile_orDie(dictName, &dictSize);
  printf("loading dictionary %s (%zu bytes)\n", dictName, dictSize);

  /* Compress each file with the dictionary. */
  int u;
  for (u = 2; u < argc; u++) {
    const char *const inFilename = argv[u];
    char *const outFilename = createOutFilename_orDie(inFilename);
    compressFile_orDie(inFilename, outFilename, dictBuffer, dictSize, cLevel);
    printf("%25s : compressed with dictionary -> %s\n", inFilename,
           outFilename);
    free(outFilename);
  }

  free(dictBuffer);
  printf("All %u files compressed with dictionary. \n", argc - 2);
  return 0;
}
