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

int UTIL_isRegularFile(const char* infilename)
{
    stat_t statbuf;
    return UTIL_getFileStat(infilename, &statbuf); /* Only need to know whether it is a regular file */
}

int UTIL_getFileStat(const char* infilename, stat_t *statbuf)
{
    int r;
#if defined(_MSC_VER)
    r = _stat64(infilename, statbuf);
    if (r || !(statbuf->st_mode & S_IFREG)) return 0;   /* No good... */
#else
    r = stat(infilename, statbuf);
    if (r || !S_ISREG(statbuf->st_mode)) return 0;   /* No good... */
#endif
    return 1;
}

int UTIL_setFileStat(const char *filename, stat_t *statbuf)
{
    int res = 0;
    struct utimbuf timebuf;

    if (!UTIL_isRegularFile(filename))
        return -1;

    timebuf.actime = time(NULL);
    timebuf.modtime = statbuf->st_mtime;
    res += utime(filename, &timebuf);  /* set access and modification times */

#if !defined(_WIN32)
    res += chown(filename, statbuf->st_uid, statbuf->st_gid);  /* Copy ownership */
#endif

    res += chmod(filename, statbuf->st_mode & 07777);  /* Copy file permissions */

    errno = 0;
    return -res; /* number of errors is returned */
}

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

/*-****************************************
*  Time functions
******************************************/
#if defined(_WIN32)   /* Windows */
    UTIL_time_t UTIL_getTime(void) { UTIL_time_t x; QueryPerformanceCounter(&x); return x; }
    U64 UTIL_getSpanTimeMicro(UTIL_time_t clockStart, UTIL_time_t clockEnd)
    {
        static LARGE_INTEGER ticksPerSecond;
        static int init = 0;
        if (!init) {
            if (!QueryPerformanceFrequency(&ticksPerSecond))
                UTIL_DISPLAYLEVEL(1, "ERROR: QueryPerformanceFrequency() failure\n");
            init = 1;
        }
        return 1000000ULL*(clockEnd.QuadPart - clockStart.QuadPart)/ticksPerSecond.QuadPart;
    }
    U64 UTIL_getSpanTimeNano(UTIL_time_t clockStart, UTIL_time_t clockEnd)
    {
        static LARGE_INTEGER ticksPerSecond;
        static int init = 0;
        if (!init) {
            if (!QueryPerformanceFrequency(&ticksPerSecond))
                UTIL_DISPLAYLEVEL(1, "ERROR: QueryPerformanceFrequency() failure\n");
            init = 1;
        }
        return 1000000000ULL*(clockEnd.QuadPart - clockStart.QuadPart)/ticksPerSecond.QuadPart;
    }

#elif defined(__APPLE__) && defined(__MACH__)
    UTIL_time_t UTIL_getTime(void) { return mach_absolute_time(); }
    U64 UTIL_getSpanTimeMicro(UTIL_time_t clockStart, UTIL_time_t clockEnd)
    {
        static mach_timebase_info_data_t rate;
        static int init = 0;
        if (!init) {
            mach_timebase_info(&rate);
            init = 1;
        }
        return (((clockEnd - clockStart) * (U64)rate.numer) / ((U64)rate.denom))/1000ULL;
    }

    U64 UTIL_getSpanTimeNano(UTIL_time_t clockStart, UTIL_time_t clockEnd)
    {
        static mach_timebase_info_data_t rate;
        static int init = 0;
        if (!init) {
            mach_timebase_info(&rate);
            init = 1;
        }
        return ((clockEnd - clockStart) * (U64)rate.numer) / ((U64)rate.denom);
    }

#elif (PLATFORM_POSIX_VERSION >= 200112L) \
   && (defined(__UCLIBC__)                \
      || (defined(__GLIBC__)              \
          && ((__GLIBC__ == 2 && __GLIBC_MINOR__ >= 17) \
             || (__GLIBC__ > 2))))

    UTIL_time_t UTIL_getTime(void)
    {
        UTIL_time_t time;
        if (clock_gettime(CLOCK_MONOTONIC, &time))
            UTIL_DISPLAYLEVEL(1, "ERROR: Failed to get time\n");   /* we could also exit() */
        return time;
    }

    UTIL_time_t UTIL_getSpanTime(UTIL_time_t begin, UTIL_time_t end)
    {
        UTIL_time_t diff;
        if (end.tv_nsec < begin.tv_nsec) {
            diff.tv_sec = (end.tv_sec - 1) - begin.tv_sec;
            diff.tv_nsec = (end.tv_nsec + 1000000000ULL) - begin.tv_nsec;
        } else {
            diff.tv_sec = end.tv_sec - begin.tv_sec;
            diff.tv_nsec = end.tv_nsec - begin.tv_nsec;
        }
        return diff;
    }

    U64 UTIL_getSpanTimeMicro(UTIL_time_t begin, UTIL_time_t end)
    {
        UTIL_time_t const diff = UTIL_getSpanTime(begin, end);
        U64 micro = 0;
        micro += 1000000ULL * diff.tv_sec;
        micro += diff.tv_nsec / 1000ULL;
        return micro;
    }

    U64 UTIL_getSpanTimeNano(UTIL_time_t begin, UTIL_time_t end)
    {
        UTIL_time_t const diff = UTIL_getSpanTime(begin, end);
        U64 nano = 0;
        nano += 1000000000ULL * diff.tv_sec;
        nano += diff.tv_nsec;
        return nano;
    }

#else   /* relies on standard C (note : clock_t measurements can be wrong when using multi-threading) */
    typedef clock_t UTIL_time_t;
    #define UTIL_TIME_INITIALIZER 0
    UTIL_time_t UTIL_getTime(void) { return clock(); }
    U64 UTIL_getSpanTimeMicro(UTIL_time_t clockStart, UTIL_time_t clockEnd) { return 1000000ULL * (clockEnd - clockStart) / CLOCKS_PER_SEC; }
    U64 UTIL_getSpanTimeNano(UTIL_time_t clockStart, UTIL_time_t clockEnd) { return 1000000000ULL * (clockEnd - clockStart) / CLOCKS_PER_SEC; }
#endif

#define SEC_TO_MICRO 1000000

/* returns time span in microseconds */
U64 UTIL_clockSpanMicro(UTIL_time_t clockStart )
{
    UTIL_time_t const clockEnd = UTIL_getTime();
    return UTIL_getSpanTimeMicro(clockStart, clockEnd);
}

/* returns time span in microseconds */
U64 UTIL_clockSpanNano(UTIL_time_t clockStart )
{
    UTIL_time_t const clockEnd = UTIL_getTime();
    return UTIL_getSpanTimeNano(clockStart, clockEnd);
}

void UTIL_waitForNextTick(void)
{
    UTIL_time_t const clockStart = UTIL_getTime();
    UTIL_time_t clockEnd;
    do {
        clockEnd = UTIL_getTime();
    } while (UTIL_getSpanTimeNano(clockStart, clockEnd) == 0);
}

#if defined (__cplusplus)
}
#endif

