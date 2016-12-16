/**
 * Copyright (c) 2016-present, Przemyslaw Skibinski, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#ifndef PLATFORM_H_MODULE
#define PLATFORM_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif

/* **************************************
*  Compiler Options
****************************************/
#if defined(__INTEL_COMPILER)
#  pragma warning(disable : 177)    /* disable: message #177: function was declared but never referenced */
#endif
#if defined(_MSC_VER)
#  define _CRT_SECURE_NO_WARNINGS   /* Disable some Visual warning messages for fopen, strncpy */
#  define _CRT_SECURE_NO_DEPRECATE  /* VS2005 */
#  pragma warning(disable : 4127)   /* disable: C4127: conditional expression is constant */
#  if _MSC_VER <= 1800              /* (1800 = Visual Studio 2013) */
#    define snprintf sprintf_s      /* snprintf unsupported by Visual <= 2013 */
#  endif
#endif


/* **************************************
*  Unix Large Files support (>4GB)
****************************************/
#if !defined(__LP64__)                                /* No point defining Large file for 64 bit */
#  define _FILE_OFFSET_BITS 64                        /* turn off_t into a 64-bit type for ftello, fseeko */
#  if defined(__sun__) && !defined(_LARGEFILE_SOURCE) /* Sun Solaris 32-bits requires specific definitions */
#    define _LARGEFILE_SOURCE                         /* fseeko, ftello */
#  elif !defined(_LARGEFILE64_SOURCE)
#    define _LARGEFILE64_SOURCE                       /* off64_t, fseeko64, ftello64 */
#  endif
#endif


/* ************************************************************
*  Detect POSIX version
*  PLATFORM_POSIX_VERSION = -1 for non-Unix e.g. Windows
*  PLATFORM_POSIX_VERSION = 0 for Unix-like non-POSIX
*  PLATFORM_POSIX_VERSION >= 1 is equal to found _POSIX_VERSION
***************************************************************/
#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)) || defined(__midipix__) || defined(__VMS))
	/* UNIX-style OS. ------------------------------------------- */
#  if (defined(__APPLE__) && defined(__MACH__)) || defined(__SVR4) || defined(_AIX) || defined(__hpux) \
     || defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)  /* POSIX.1–2001 (SUSv3) conformant */
#    define PLATFORM_POSIX_VERSION 200112L
#  else
#    if defined(__linux__) || defined(__linux)
#      define _POSIX_C_SOURCE 200112L  /* use feature test macro */
#    endif
#    include <unistd.h>  /* declares _POSIX_VERSION */
#    if defined(_POSIX_VERSION)  /* POSIX compliant */
#      define PLATFORM_POSIX_VERSION _POSIX_VERSION
#    else
#      define PLATFORM_POSIX_VERSION 0
#    endif
#  endif
#endif

#if !defined(PLATFORM_POSIX_VERSION)
#  define PLATFORM_POSIX_VERSION -1
#endif



#if defined (__cplusplus)
}
#endif

#endif /* PLATFORM_H_MODULE */
