/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */


#ifndef BENCH_H_121279284357
#define BENCH_H_121279284357

#include <stddef.h>

int BMK_benchFiles(const char** fileNamesTable, unsigned nbFiles,
                   const char* dictFileName, int cLevel, int cLevelLast);

/* Set Parameters */
void BMK_SetNbIterations(unsigned nbLoops);
void BMK_SetBlockSize(size_t blockSize);
void BMK_setAdditionalParam(int additionalParam);
void BMK_setNotificationLevel(unsigned level);

 #endif   /* BENCH_H_121279284357 */
