/*
 * Copyright (c) 2016-present, Przemyslaw Skibinski, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#if defined (__cplusplus)
extern "C" {
#endif


/*-****************************************
*  Dependencies
******************************************/
#include "util.h"


U32 UTIL_isDirectory(const char* infilename)
{
    int r;
    stat_t statbuf;
#if defined(_MSC_VER)
    r = _stat64(infilename, &statbuf);
    if (!r && (statbuf.st_mode & _S_IFDIR)) return 1;
#else
    r = stat(infilename, &statbuf);
    if (!r && S_ISDIR(statbuf.st_mode)) return 1;
#endif
    return 0;
}

U32 UTIL_isLink(const char* infilename)
{
/* macro guards, as defined in : https://linux.die.net/man/2/lstat */
#ifndef __STRICT_ANSI__
#if defined(_BSD_SOURCE) \
    || (defined(_XOPEN_SOURCE) && (_XOPEN_SOURCE >= 500)) \
    || (defined(_XOPEN_SOURCE) && defined(_XOPEN_SOURCE_EXTENDED)) \
    || (defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200112L)) \
    || (defined(__APPLE__) && defined(__MACH__))
    int r;
    stat_t statbuf;
    r = lstat(infilename, &statbuf);
    if (!r && S_ISLNK(statbuf.st_mode)) return 1;
#endif
#endif
    (void)infilename;
    return 0;
}

U64 UTIL_getFileSize(const char* infilename)
{
    if (!UTIL_isRegularFile(infilename)) return UTIL_FILESIZE_UNKNOWN;
    {   int r;
#if defined(_MSC_VER)
        struct __stat64 statbuf;
        r = _stat64(infilename, &statbuf);
        if (r || !(statbuf.st_mode & S_IFREG)) return UTIL_FILESIZE_UNKNOWN;
#elif defined(__MINGW32__) && defined (__MSVCRT__)
        struct _stati64 statbuf;
        r = _stati64(infilename, &statbuf);
        if (r || !(statbuf.st_mode & S_IFREG)) return UTIL_FILESIZE_UNKNOWN;
#else
        struct stat statbuf;
        r = stat(infilename, &statbuf);
        if (r || !S_ISREG(statbuf.st_mode)) return UTIL_FILESIZE_UNKNOWN;
#endif
        return (U64)statbuf.st_size;
    }
}


U64 UTIL_getTotalFileSize(const char* const * const fileNamesTable, unsigned nbFiles)
{
    U64 total = 0;
    int error = 0;
    unsigned n;
    for (n=0; n<nbFiles; n++) {
        U64 const size = UTIL_getFileSize(fileNamesTable[n]);
        error |= (size == UTIL_FILESIZE_UNKNOWN);
        total += size;
    }
    return error ? UTIL_FILESIZE_UNKNOWN : total;
}

#if defined (__cplusplus)
}
#endif
