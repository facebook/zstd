/**
 * Copyright (c) 2016-present, Przemyslaw Skibinski, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#ifndef ZSTD_ZLIBWRAPPER_H
#define ZSTD_ZLIBWRAPPER_H

#if defined (__cplusplus)
extern "C" {
#endif


#define Z_PREFIX
#include <zlib.h>

#if !defined(z_const)
#if ZLIB_VERNUM >= 0x1260
    #define z_const const
#else
    #define z_const
#endif
#endif

void useZSTD(int turn_on);
int isUsingZSTD(void);
const char * zstdVersion(void);


#if defined (__cplusplus)
}
#endif

#endif /* ZSTD_ZLIBWRAPPER_H */
