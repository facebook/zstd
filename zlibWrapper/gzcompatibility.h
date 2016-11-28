/**
 * Copyright (c) 2016-present, Przemyslaw Skibinski, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#if ZLIB_VERNUM == 0x1260 && !defined(_LARGEFILE64_SOURCE)
  //  #define _LARGEFILE64_SOURCE 0
#endif

#if ZLIB_VERNUM <= 0x1240
ZEXTERN int ZEXPORT gzclose_r OF((gzFile file));
ZEXTERN int ZEXPORT gzclose_w OF((gzFile file));
ZEXTERN int ZEXPORT gzbuffer OF((gzFile file, unsigned size)); 
ZEXTERN z_off_t ZEXPORT gzoffset OF((gzFile file));
 
#if !defined(_WIN32) && defined(Z_LARGE64)
#  define z_off64_t off64_t
#else
#  if defined(_WIN32) && !defined(__GNUC__) && !defined(Z_SOLO)
#    define z_off64_t __int64
#  else
#    define z_off64_t z_off_t
#  endif
#endif
#endif


#if ZLIB_VERNUM <= 0x1250
struct gzFile_s {
    unsigned have;
    unsigned char *next;
    z_off64_t pos;
};
#endif


#if ZLIB_VERNUM <= 0x1270
#if defined(_WIN32) && !defined(Z_SOLO)
#    include <stddef.h>         /* for wchar_t */ 
ZEXTERN gzFile         ZEXPORT gzopen_w OF((const wchar_t *path,
                                            const char *mode));
#endif
#endif
