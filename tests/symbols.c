/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */


#include <stdio.h>
#include "zstd_errors.h"
#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"
#define ZBUFF_DISABLE_DEPRECATE_WARNINGS
#define ZBUFF_STATIC_LINKING_ONLY
#include "zbuff.h"
#define ZDICT_DISABLE_DEPRECATE_WARNINGS
#define ZDICT_STATIC_LINKING_ONLY
#include "zdict.h"

/* Symbols that aren't being tested for in:
 * ["zstreamtest.c", "fuzzer.c", "fullbench.c", "longmatch.c", "legacy.c",
 *    "poolTests.c", "invalidDictionaries.c", "decodecorpus.c", "fileio.c",
 *    "dibio.c", "benchzstd.c", "zstdcli.c"] */
static const void *symbols[] = {
  ZDICT_addEntropyTablesFromBuffer,
  NULL,
};

int main(int argc, const char** argv) {
  const void **symbol;
  (void)argc;
  (void)argv;

  for (symbol = symbols; *symbol != NULL; ++symbol) {
    printf("%p\n", *symbol);
  }
  return 0;
}
