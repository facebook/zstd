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
#include <stdlib.h>
#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"

static const char invalidRepCode[] = {
  0x37, 0xa4, 0x30, 0xec, 0x2a, 0x00, 0x00, 0x00, 0x39, 0x10, 0xc0, 0xc2,
  0xa6, 0x00, 0x0c, 0x30, 0xc0, 0x00, 0x03, 0x0c, 0x30, 0x20, 0x72, 0xf8,
  0xb4, 0x6d, 0x4b, 0x9f, 0xfc, 0x97, 0x29, 0x49, 0xb2, 0xdf, 0x4b, 0x29,
  0x7d, 0x4a, 0xfc, 0x83, 0x18, 0x22, 0x75, 0x23, 0x24, 0x44, 0x4d, 0x02,
  0xb7, 0x97, 0x96, 0xf6, 0xcb, 0xd1, 0xcf, 0xe8, 0x22, 0xea, 0x27, 0x36,
  0xb7, 0x2c, 0x40, 0x46, 0x01, 0x08, 0x23, 0x01, 0x00, 0x00, 0x06, 0x1e,
  0x3c, 0x83, 0x81, 0xd6, 0x18, 0xd4, 0x12, 0x3a, 0x04, 0x00, 0x80, 0x03,
  0x08, 0x0e, 0x12, 0x1c, 0x12, 0x11, 0x0d, 0x0e, 0x0a, 0x0b, 0x0a, 0x09,
  0x10, 0x0c, 0x09, 0x05, 0x04, 0x03, 0x06, 0x06, 0x06, 0x02, 0x00, 0x03,
  0x00, 0x00, 0x02, 0x02, 0x00, 0x04, 0x06, 0x03, 0x06, 0x08, 0x24, 0x6b,
  0x0d, 0x01, 0x10, 0x04, 0x81, 0x07, 0x00, 0x00, 0x04, 0xb9, 0x58, 0x18,
  0x06, 0x59, 0x92, 0x43, 0xce, 0x28, 0xa5, 0x08, 0x88, 0xc0, 0x80, 0x88,
  0x8c, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
  0x08, 0x00, 0x00, 0x00
};

typedef struct dictionary_s {
  const char *data;
  size_t size;
} dictionary;

static const dictionary dictionaries[] = {
  {invalidRepCode, sizeof(invalidRepCode)},
  {NULL, 0},
};

static void* countAllocations(void* opaque, size_t size) {
  size_t* const count = (size_t*)opaque;
  ++*count;
  (void)size;
  return NULL;
}

static void ignoreFree(void* opaque, void* address) {
  (void)opaque;
  (void)address;
}

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      fprintf(stderr, "CDict size test failed at line %d: %s\n",          \
              __LINE__, #condition);                                       \
      goto cleanup;                                                        \
    }                                                                      \
  } while (0)

static int testCDictSizeOverflow(void) {
  static const char dict[] = "a small raw content dictionary";
  ZSTD_compressionParameters cParams = ZSTD_getCParams(1, 0, 0);
  ZSTD_CCtx_params* const params = ZSTD_createCCtxParams();
  ZSTD_CDict* cdict = NULL;
  void* workspace = NULL;
  size_t allocCount = 0;
  ZSTD_customMem const customMem = {countAllocations, ignoreFree, &allocCount};
  size_t baseSize;
  size_t workspaceSize;
  int result = 1;

  CHECK(params != NULL);
  CHECK(!ZSTD_isError(ZSTD_CCtxParams_init(params, 1)));
  cParams.windowLog = ZSTD_WINDOWLOG_MIN;
  cParams.hashLog = ZSTD_HASHLOG_MIN;
  cParams.chainLog = ZSTD_CHAINLOG_MIN;
  baseSize = ZSTD_estimateCDictSize_advanced(0, cParams, ZSTD_dlm_byCopy);
  CHECK(!ZSTD_isError(baseSize));
  workspaceSize = ZSTD_estimateCDictSize_advanced(sizeof(dict), cParams, ZSTD_dlm_byCopy);
  CHECK(!ZSTD_isError(workspaceSize));
  workspace = malloc(workspaceSize);
  CHECK(workspace != NULL);

  {
    size_t const maxSize = (size_t)-1;
    size_t const badSizes[] = {
      maxSize,
      maxSize - 1,
      maxSize - (sizeof(void*) - 1),
      /* The estimator includes a conservative match-state allowance, so stay
       * well inside the overflow range for both static and heap workspaces. */
      maxSize - baseSize / 2,
    };
    size_t i;
    for (i = 0; i < sizeof(badSizes) / sizeof(badSizes[0]); ++i) {
      size_t const dictSize = badSizes[i];
      /* Exercise the public reproducer as well as each workspace-sizing path.
       * The dictionary is deliberately much smaller than the claimed size:
       * overflow must be rejected before the input is read or copied. */
      cdict = ZSTD_createCDict(dict, dictSize, 1);
      CHECK(cdict == NULL);
      CHECK(ZSTD_isError(ZSTD_estimateCDictSize(dictSize, 1)));
      CHECK(ZSTD_isError(ZSTD_estimateCDictSize_advanced(dictSize, cParams, ZSTD_dlm_byCopy)));
      cdict = ZSTD_createCDict_advanced(dict, dictSize, ZSTD_dlm_byCopy,
                                      ZSTD_dct_auto, cParams, customMem);
      CHECK(cdict == NULL);
      cdict = ZSTD_createCDict_advanced2(dict, dictSize, ZSTD_dlm_byCopy,
                                       ZSTD_dct_auto, params, customMem);
      CHECK(cdict == NULL);
      CHECK(allocCount == 0);
      CHECK(ZSTD_initStaticCDict(workspace, workspaceSize, dict, dictSize,
                                ZSTD_dlm_byCopy, ZSTD_dct_auto, cParams) == NULL);
      /* By-reference estimates don't include a dictionary allocation. */
      CHECK(ZSTD_estimateCDictSize_advanced(dictSize, cParams, ZSTD_dlm_byRef) == baseSize);
    }
  }

  {
    size_t const goodSizes[] = {0, 1, sizeof(void*) - 1, sizeof(void*), sizeof(dict)};
    size_t i;
    for (i = 0; i < sizeof(goodSizes) / sizeof(goodSizes[0]); ++i) {
      size_t const dictSize = goodSizes[i];
      size_t const neededSize = ZSTD_estimateCDictSize_advanced(dictSize, cParams, ZSTD_dlm_byCopy);
      CHECK(!ZSTD_isError(neededSize));
      CHECK(neededSize <= workspaceSize);
      cdict = ZSTD_createCDict(dict, dictSize, 1);
      CHECK(cdict != NULL);
      ZSTD_freeCDict(cdict);
      cdict = NULL;
      CHECK(ZSTD_initStaticCDict(workspace, neededSize, dict, dictSize,
                                ZSTD_dlm_byCopy, ZSTD_dct_auto, cParams) != NULL);
      CHECK(ZSTD_initStaticCDict(workspace, baseSize, dict, dictSize,
                                ZSTD_dlm_byRef, ZSTD_dct_auto, cParams) != NULL);
    }
  }
  result = 0;

cleanup:
  ZSTD_freeCDict(cdict);
  ZSTD_freeCCtxParams(params);
  free(workspace);
  return result;
}

#undef CHECK

int main(int argc, const char** argv) {
  const dictionary *dict;
  if (testCDictSizeOverflow()) return 3;
  for (dict = dictionaries; dict->data != NULL; ++dict) {
    ZSTD_CDict *cdict;
    ZSTD_DDict *ddict;
    cdict = ZSTD_createCDict(dict->data, dict->size, 1);
    if (cdict) {
      ZSTD_freeCDict(cdict);
      return 1;
    }
    ddict = ZSTD_createDDict(dict->data, dict->size);
    if (ddict) {
      ZSTD_freeDDict(ddict);
      return 2;
    }
  }

  (void)argc;
  (void)argv;
  return 0;
}
