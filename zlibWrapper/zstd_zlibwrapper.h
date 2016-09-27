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

/* returns a string with version of zstd library */
const char * zstdVersion(void);


/*** COMPRESSION ***/
/* enables/disables zstd compression during runtime */
void ZWRAP_useZSTDcompression(int turn_on);

/* checks if zstd compression is turned on */
int ZWRAP_isUsingZSTDcompression(void);

/* Changes a pledged source size for a given compression stream.
   It will change ZSTD compression parameters what may improve compression speed and/or ratio.
   The function should be called just after deflateInit() or deflateReset() and before deflate() or deflateSetDictionary().
   It's only helpful when data is compressed in blocks. 
   There will be no change in case of deflateInit() or deflateReset() immediately followed by deflate(strm, Z_FINISH) 
   as this case is automatically detected.  */
int ZWRAP_setPledgedSrcSize(z_streamp strm, unsigned long long pledgedSrcSize);

/* Similar to deflateReset but preserves dictionary set using deflateSetDictionary.
   It should improve compression speed because there will be less calls to deflateSetDictionary 
   When using zlib compression this method redirects to deflateReset. */
int ZWRAP_deflateReset_keepDict(z_streamp strm);



/*** DECOMPRESSION ***/
typedef enum { ZWRAP_FORCE_ZLIB, ZWRAP_AUTO } ZWRAP_decompress_type;

/* enables/disables automatic recognition of zstd/zlib compressed data during runtime */
void ZWRAP_setDecompressionType(ZWRAP_decompress_type type);

/* checks zstd decompression type */
ZWRAP_decompress_type ZWRAP_getDecompressionType(void);

/* Checks if zstd decompression is used for a given stream.
   If will return 1 only when inflate() was called and zstd header was detected. */
int ZWRAP_isUsingZSTDdecompression(z_streamp strm);

/* Similar to inflateReset but preserves dictionary set using inflateSetDictionary.
   inflate() will return Z_NEED_DICT only for the first time what will improve decompression speed.
   For zlib streams this method redirects to inflateReset. */
int ZWRAP_inflateReset_keepDict(z_streamp strm);


#if defined (__cplusplus)
}
#endif

#endif /* ZSTD_ZLIBWRAPPER_H */
