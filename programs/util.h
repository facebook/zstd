/* ******************************************************************
  util.h - utility functions
  Copyright (C) 2016, Przemyslaw Skibinski, Yann Collet.

  GPL v2 License

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

  You can contact the author at :
  - ZSTD homepage : http://www.zstd.net/
*/
#ifndef UTIL_H_MODULE
#define UTIL_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif

/* **************************************
*  Compiler Options
****************************************/
#if defined(_MSC_VER)
#  define _CRT_SECURE_NO_WARNINGS    /* Disable some Visual warning messages for fopen, strncpy */
#  define _CRT_SECURE_NO_DEPRECATE   /* VS2005 */
#  pragma warning(disable : 4127)    /* disable: C4127: conditional expression is constant */
#if _MSC_VER <= 1800                 /* (1800 = Visual Studio 2013) */
    #define snprintf sprintf_s       /* snprintf unsupported by Visual <= 2013 */
  //#define snprintf _snprintf
#endif
#endif


/* Unix Large Files support (>4GB) */
#if !defined(__LP64__)              /* No point defining Large file for 64 bit */
#   define _FILE_OFFSET_BITS 64     /* turn off_t into a 64-bit type for ftello, fseeko */
#   if defined(__sun__)             /* Sun Solaris 32-bits requires specific definitions */
#      define _LARGEFILE_SOURCE     /* fseeko, ftello */
#   else                        
#      define _LARGEFILE64_SOURCE   /* off64_t, fseeko64, ftello64 */
#   endif
#endif


/*-****************************************
*  Dependencies
******************************************/
#include <stdlib.h>     /* features.h with _POSIX_C_SOURCE, malloc */
#include <stdio.h>      /* fprintf */
#include <sys/types.h>  /* stat */
#include <sys/stat.h>   /* stat */
#include "mem.h"        /* U32, U64 */


/* *************************************
*  Constants
***************************************/
#define LIST_SIZE_INCREASE   (8*1024)


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


/*-****************************************
*  Sleep functions: Windows - Posix - others
******************************************/
#if defined(_WIN32)
#  include <windows.h>
#  define SET_HIGH_PRIORITY SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS)
#  define UTIL_sleep(s) Sleep(1000*s)
#  define UTIL_sleepMilli(milli) Sleep(milli)
#elif (defined(__unix__) || defined(__unix) || defined(__VMS) || defined(__midipix__) || (defined(__APPLE__) && defined(__MACH__))) 
#  include <unistd.h>
#  include <sys/resource.h> /* setpriority */
#  include <time.h>         /* clock_t, nanosleep, clock, CLOCKS_PER_SEC */
#  if defined(PRIO_PROCESS)
#    define SET_HIGH_PRIORITY setpriority(PRIO_PROCESS, 0, -20)
#  else
#    define SET_HIGH_PRIORITY /* disabled */
#  endif
#  define UTIL_sleep(s) sleep(s)
#  if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 199309L)
#      define UTIL_sleepMilli(milli) { struct timespec t; t.tv_sec=0; t.tv_nsec=milli*1000000ULL; nanosleep(&t, NULL); }
#  else
#      define UTIL_sleepMilli(milli) /* disabled */
#  endif
#else
#  define SET_HIGH_PRIORITY      /* disabled */
#  define UTIL_sleep(s)          /* disabled */
#  define UTIL_sleepMilli(milli) /* disabled */
#endif


/*-****************************************
*  Time functions
******************************************/
#if !defined(_WIN32)
   typedef clock_t UTIL_time_t;
   UTIL_STATIC void UTIL_initTimer(UTIL_time_t* ticksPerSecond) { *ticksPerSecond=0; }
   UTIL_STATIC void UTIL_getTime(UTIL_time_t* x) { *x = clock(); }
   UTIL_STATIC U64 UTIL_getSpanTimeMicro(UTIL_time_t ticksPerSecond, UTIL_time_t clockStart, UTIL_time_t clockEnd) { (void)ticksPerSecond; return 1000000ULL * (clockEnd - clockStart) / CLOCKS_PER_SEC; }
   UTIL_STATIC U64 UTIL_getSpanTimeNano(UTIL_time_t ticksPerSecond, UTIL_time_t clockStart, UTIL_time_t clockEnd) { (void)ticksPerSecond; return 1000000000ULL * (clockEnd - clockStart) / CLOCKS_PER_SEC; }
#else
   typedef LARGE_INTEGER UTIL_time_t;
   UTIL_STATIC void UTIL_initTimer(UTIL_time_t* ticksPerSecond) { if (!QueryPerformanceFrequency(ticksPerSecond)) fprintf(stderr, "ERROR: QueryPerformance not present\n"); }
   UTIL_STATIC void UTIL_getTime(UTIL_time_t* x) { QueryPerformanceCounter(x); }
   UTIL_STATIC U64 UTIL_getSpanTimeMicro(UTIL_time_t ticksPerSecond, UTIL_time_t clockStart, UTIL_time_t clockEnd) { return 1000000ULL*(clockEnd.QuadPart - clockStart.QuadPart)/ticksPerSecond.QuadPart; }
   UTIL_STATIC U64 UTIL_getSpanTimeNano(UTIL_time_t ticksPerSecond, UTIL_time_t clockStart, UTIL_time_t clockEnd) { return 1000000000ULL*(clockEnd.QuadPart - clockStart.QuadPart)/ticksPerSecond.QuadPart; }
#endif


/* returns time span in microseconds */
UTIL_STATIC U64 UTIL_clockSpanMicro( UTIL_time_t clockStart, UTIL_time_t ticksPerSecond )
{
    UTIL_time_t clockEnd;
    UTIL_getTime(&clockEnd);
    return UTIL_getSpanTimeMicro(ticksPerSecond, clockStart, clockEnd);
}


UTIL_STATIC void UTIL_waitForNextTick(UTIL_time_t ticksPerSecond)
{
    UTIL_time_t clockStart, clockEnd;
    UTIL_getTime(&clockStart);
    do { 
        UTIL_getTime(&clockEnd); 
    } while (UTIL_getSpanTimeNano(ticksPerSecond, clockStart, clockEnd) == 0);
}



/*-****************************************
*  File functions
******************************************/
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


UTIL_STATIC int UTIL_doesFileExists(const char* infilename)
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
    return 1;
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


#ifdef _WIN32
#  define UTIL_HAS_CREATEFILELIST

UTIL_STATIC int UTIL_prepareFileList(const char *dirName, char** bufStart, size_t* pos, char** bufEnd)
{
    char path[MAX_PATH];
    int pathLength, nbFiles = 0;
    WIN32_FIND_DATA cFile;
    HANDLE hFile;

    pathLength = snprintf(path, MAX_PATH, "%s\\*", dirName);
    if (pathLength < 0 || pathLength >= MAX_PATH) {
        fprintf(stderr, "Path length has got too long.\n");
        return 0;
    }

    hFile=FindFirstFile(path, &cFile);
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Cannot open directory '%s'\n", dirName);
        return 0;
    }

    do {
        pathLength = snprintf(path, MAX_PATH, "%s\\%s", dirName, cFile.cFileName);
        if (pathLength < 0 || pathLength >= MAX_PATH) {
            fprintf(stderr, "Path length has got too long.\n");
            continue;
        }
        if (cFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (strcmp (cFile.cFileName, "..") == 0 ||
                strcmp (cFile.cFileName, ".") == 0) continue;
         //   printf ("[%s]\n", path);
            nbFiles += UTIL_prepareFileList(path, bufStart, pos, bufEnd);  /* Recursively call "UTIL_prepareFileList" with the new path. */
            if (*bufStart == NULL) { FindClose(hFile); return 0; }
        }
        else if ((cFile.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) || (cFile.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) || (cFile.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED)) {
            if (*bufStart + *pos + pathLength >= *bufEnd) {
                ptrdiff_t newListSize = (*bufEnd - *bufStart) + LIST_SIZE_INCREASE;
                *bufStart = (char*)realloc(*bufStart, newListSize);
                *bufEnd = *bufStart + newListSize;
                if (*bufStart == NULL) { FindClose(hFile); return 0; }
            }
            if (*bufStart + *pos + pathLength < *bufEnd) {
                strncpy(*bufStart + *pos, path, *bufEnd - (*bufStart + *pos));
                *pos += pathLength + 1;
                nbFiles++;
            }
         //   printf ("%s\\%s nbFiles=%d left=%d\n", dirName, cFile.cFileName, nbFiles, (int)(bufEnd - *bufStart));
        }
    } while (FindNextFile(hFile, &cFile));

    FindClose(hFile);
    return nbFiles;
}

#elif (defined(__unix__) || defined(__unix) || defined(__midipix__) || (defined(__APPLE__) && defined(__MACH__))) && defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200112L) /* snprintf, opendir */
#  define UTIL_HAS_CREATEFILELIST
#  include <dirent.h>       /* opendir, readdir */
#  include <limits.h>       /* PATH_MAX */
#  include <errno.h>

UTIL_STATIC int UTIL_prepareFileList(const char *dirName, char** bufStart, size_t* pos, char** bufEnd)
{
    DIR *dir;
    struct dirent *entry;
    char path[PATH_MAX];
    int pathLength, nbFiles = 0;

    if (!(dir = opendir(dirName))) {
        fprintf(stderr, "Cannot open directory '%s': %s\n", dirName, strerror(errno));
        return 0;
    }
 
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp (entry->d_name, "..") == 0 ||
            strcmp (entry->d_name, ".") == 0) continue;
        pathLength = snprintf(path, PATH_MAX, "%s/%s", dirName, entry->d_name);
        if (pathLength < 0 || pathLength >= PATH_MAX) {
            fprintf(stderr, "Path length has got too long.\n");
            continue;
        }
        if (UTIL_isDirectory(path)) {
         //   printf ("[%s]\n", path);
            nbFiles += UTIL_prepareFileList(path, bufStart, pos, bufEnd);  /* Recursively call "UTIL_prepareFileList" with the new path. */
            if (*bufStart == NULL) { closedir(dir); return 0; }
        } else {
            if (*bufStart + *pos + pathLength >= *bufEnd) {
                ptrdiff_t newListSize = (*bufEnd - *bufStart) + LIST_SIZE_INCREASE;
                *bufStart = (char*)realloc(*bufStart, newListSize);
                *bufEnd = *bufStart + newListSize;
                if (*bufStart == NULL) { closedir(dir); return 0; }
            }
            if (*bufStart + *pos + pathLength < *bufEnd) {
                strncpy(*bufStart + *pos, path, *bufEnd - (*bufStart + *pos));
                *pos += pathLength + 1;
                nbFiles++;
            }
         //   printf ("%s/%s nbFiles=%d left=%d\n", dirName, entry->d_name, nbFiles, (int)(bufEnd - *bufStart));
        }
    }

    closedir(dir);
    return nbFiles;
}

#else

UTIL_STATIC int UTIL_prepareFileList(const char *dirName, char** bufStart, size_t* pos, char** bufEnd)
{
    (void)bufStart; (void)bufEnd; (void)pos;
    fprintf(stderr, "Directory %s ignored (zstd compiled without _WIN32 or _POSIX_C_SOURCE)\n", dirName);
    return 0;
}

#endif // #ifdef _WIN32

/* 
 * UTIL_createFileList - takes a list of files and directories (params: inputNames, inputNamesNb), scans directories, 
 *                       and returns a new list of files (params: return value, allocatedBuffer, allocatedNamesNb).
 * After finishing usage of the list the structures should be freed with UTIL_freeFileList(params: return value, allocatedBuffer)
 * In case of error UTIL_createFileList returns NULL and UTIL_freeFileList should not be called.
 */
UTIL_STATIC const char** UTIL_createFileList(const char **inputNames, unsigned inputNamesNb, char** allocatedBuffer, unsigned* allocatedNamesNb)
{
    size_t pos;
    unsigned i, nbFiles;
    char *bufend, *buf;
    const char** fileTable;

    buf = (char*)malloc(LIST_SIZE_INCREASE);
    if (!buf) return NULL;
    bufend = buf + LIST_SIZE_INCREASE;

    for (i=0, pos=0, nbFiles=0; i<inputNamesNb; i++) {
        if (UTIL_doesFileExists(inputNames[i])) {
       // printf ("UTIL_doesFileExists=[%s]\n", inputNames[i]);
            size_t len = strlen(inputNames[i]);
            if (buf + pos + len >= bufend) {
                ptrdiff_t newListSize = (bufend - buf) + LIST_SIZE_INCREASE;
                buf = (char*)realloc(buf, newListSize);
                bufend = buf + newListSize;
                if (!buf) return NULL;
            }
            if (buf + pos + len < bufend) {
                strncpy(buf + pos, inputNames[i], bufend - (buf + pos));
                pos += len + 1;
                nbFiles++;
            }
        }
        else
        {
            nbFiles += UTIL_prepareFileList(inputNames[i], &buf, &pos, &bufend);
            if (buf == NULL) return NULL;
        }
    }

    if (nbFiles == 0) { free(buf); return NULL; }

    fileTable = (const char**)malloc((nbFiles+1) * sizeof(const char*));
    if (!fileTable) { free(buf); return NULL; }

    for (i=0, pos=0; i<nbFiles; i++)
    {
        fileTable[i] = buf + pos;
        pos += strlen(fileTable[i]) + 1;
    }

    if (buf + pos > bufend) { free(buf); free((void*)fileTable); return NULL; }

    *allocatedBuffer = buf;
    *allocatedNamesNb = nbFiles;

    return fileTable;
}


UTIL_STATIC void UTIL_freeFileList(const char** filenameTable, char* allocatedBuffer)
{
    if (allocatedBuffer) free(allocatedBuffer);
    if (filenameTable) free((void*)filenameTable);
}


#if defined (__cplusplus)
}
#endif

#endif /* UTIL_H_MODULE */

