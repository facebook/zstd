/* ******************************************************************
   util.h
   utility functions
   Copyright (C) 2016, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
    - FSE source repository : https://github.com/Cyan4973/FiniteStateEntropy
    - Public forum : https://groups.google.com/forum/#!forum/lz4c
****************************************************************** */
#ifndef UTIL_H_MODULE
#define UTIL_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif

/*-****************************************
*  Dependencies
******************************************/
#define _POSIX_C_SOURCE 199309L   /* before <time.h> - needed for nanosleep() */
#include <time.h>       /* clock_t, nanosleep, clock, CLOCKS_PER_SEC */
#include <sys/types.h>  /* stat */
#include <sys/stat.h>   /* stat */
#include "mem.h"        /* U32, U64 */


/*-****************************************
*  Compiler specifics
******************************************/
#if defined(__GNUC__)
#  define UTIL_STATIC static __attribute__((unused))
#elif defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#  define UTIL_STATIC static inline
#elif defined(_MSC_VER)
#  define UTIL_STATIC static __inline
#else
#  define UTIL_STATIC static  /* this version may generate warnings for unused static functions; disable the relevant warning */
#endif

/* Sleep functions: posix - windows - others */
#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
#  include <unistd.h>
#  include <sys/resource.h> /* setpriority */
#  define UTIL_sleep(s) sleep(s)
#  define UTIL_sleepMilli(milli) { struct timespec t; t.tv_sec=0; t.tv_nsec=milli*1000000ULL; nanosleep(&t, NULL); }
#  define SET_HIGH_PRIORITY setpriority(PRIO_PROCESS, 0, -20)
#elif defined(_WIN32)
#  include <windows.h>
#  define UTIL_sleep(s) Sleep(1000*s)
#  define UTIL_sleepMilli(milli) Sleep(milli)
#  define SET_HIGH_PRIORITY SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS)
#else
#  define UTIL_sleep(s)   /* disabled */
#  define UTIL_sleepMilli(milli) /* disabled */
#  define SET_HIGH_PRIORITY /* disabled */
#endif

#if !defined(_WIN32)
   typedef clock_t UTIL_time_t;
#  define UTIL_initTimer(ticksPerSecond) ticksPerSecond=0
#  define UTIL_getTime(x) x = clock()
#  define UTIL_getSpanTimeMicro(ticksPerSecond, clockStart, clockEnd) (1000000ULL * (clockEnd - clockStart) / CLOCKS_PER_SEC)
#  define UTIL_getSpanTimeNano(ticksPerSecond, clockStart, clockEnd) (1000000000ULL * (clockEnd - clockStart) / CLOCKS_PER_SEC)
#else
   typedef LARGE_INTEGER UTIL_time_t;
#  define UTIL_initTimer(x) if (!QueryPerformanceFrequency(&x)) { fprintf(stderr, "ERROR: QueryPerformance not present\n"); }
#  define UTIL_getTime(x) QueryPerformanceCounter(&x)
#  define UTIL_getSpanTimeMicro(ticksPerSecond, clockStart, clockEnd) (1000000ULL*(clockEnd.QuadPart - clockStart.QuadPart)/ticksPerSecond.QuadPart)
#  define UTIL_getSpanTimeNano(ticksPerSecond, clockStart, clockEnd) (1000000000ULL*(clockEnd.QuadPart - clockStart.QuadPart)/ticksPerSecond.QuadPart)
#endif



/*-****************************************
*  Utility functions
******************************************/
/* returns time span in microseconds */
UTIL_STATIC U64 UTIL_clockSpanMicro( UTIL_time_t clockStart, UTIL_time_t ticksPerSecond )
{
    UTIL_time_t clockEnd;
    (void)ticksPerSecond;

    UTIL_getTime(clockEnd);
    return UTIL_getSpanTimeMicro(ticksPerSecond, clockStart, clockEnd);
}


UTIL_STATIC void UTIL_waitForNextTick(UTIL_time_t ticksPerSecond)
{
    UTIL_time_t clockStart, clockEnd;
    (void)ticksPerSecond;

    UTIL_getTime(clockStart);
    do { 
        UTIL_getTime(clockEnd); 
    } while (UTIL_getSpanTimeNano(ticksPerSecond, clockStart, clockEnd) == 0);
}


UTIL_STATIC U64 UTIL_getFileSize(const char* infilename)
{
    int r;
#if defined(_MSC_VER)
    struct _stat64 statbuf;
    r = _stat64(infilename, &statbuf);
    if (r || !(statbuf.st_mode & S_IFREG)) return 0;   /* No good... */
#else
    struct stat statbuf;
    r = stat(infilename, &statbuf);
    if (r || !S_ISREG(statbuf.st_mode)) return 0;   /* No good... */
#endif
    return (U64)statbuf.st_size;
}


UTIL_STATIC U64 UTIL_getTotalFileSize(const char** fileNamesTable, unsigned nbFiles)
{
    U64 total = 0;
    unsigned n;
    for (n=0; n<nbFiles; n++)
        total += UTIL_getFileSize(fileNamesTable[n]);
    return total;
}


UTIL_STATIC U32 UTIL_isDirectory(const char* infilename)
{
    int r;
#if defined(_MSC_VER)
    struct _stat64 statbuf;
    r = _stat64(infilename, &statbuf);
    if (!r && (statbuf.st_mode & _S_IFDIR)) return 1;
#else
    struct stat statbuf;
    r = stat(infilename, &statbuf);
    if (!r && S_ISDIR(statbuf.st_mode)) return 1;
#endif
    return 0;
}


#if defined (__cplusplus)
}
#endif

#endif /* UTIL_H_MODULE */

