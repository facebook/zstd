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

/* enables/disables zstd compression during runtime */
void useZSTD(int turn_on);

/* check if zstd compression is turned on */
int isUsingZSTD(void);

/* returns a string with version of zstd library */
const char * zstdVersion(void);

/* Changes a pledged source size for a given compression stream.
   The function should be called after deflateInit().
   After this function deflateReset() should be called. */
int ZSTD_setPledgedSrcSize(z_streamp strm, unsigned long long pledgedSrcSize);


#if defined (__cplusplus)
}
#endif

#endif /* ZSTD_ZLIBWRAPPER_H */
