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

size_t ZSTD_splitBlock_4k(const void* src, size_t srcSize, size_t blockSizeMax);

#if defined (__cplusplus)
}
#endif

#endif /* ZSTD_PRESPLIT_H */
