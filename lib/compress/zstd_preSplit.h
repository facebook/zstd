/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_PRESPLIT_H
#define ZSTD_PRESPLIT_H

#include <stddef.h>  /* size_t */

#if defined (__cplusplus)
extern "C" {
#endif

typedef enum { split_lvl1, split_lvl2 } ZSTD_SplitBlock_strategy_e;

#define ZSTD_SLIPBLOCK_WORKSPACESIZE 8208

/* note:
 * @workspace must be aligned on 8-bytes boundaries
 * @wkspSize must be at least >= ZSTD_SLIPBLOCK_WORKSPACESIZE
 * note2:
 * for the time being, this function only accepts full 128 KB blocks,
 * therefore @blockSizeMax must be == 128 KB.
 * This could be extended to smaller sizes in the future.
 */
size_t ZSTD_splitBlock(const void* src, size_t srcSize,
                    size_t blockSizeMax, ZSTD_SplitBlock_strategy_e splitStrat,
                    void* workspace, size_t wkspSize);

#if defined (__cplusplus)
}
#endif

#endif /* ZSTD_PRESPLIT_H */
