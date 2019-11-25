/*
 * Copyright (c) 2016-present, Przemyslaw Skibinski, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef UTIL_H_MODULE
#define UTIL_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif


/*-****************************************
*  Dependencies
******************************************/
#include "platform.h"     /* PLATFORM_POSIX_VERSION, ZSTD_NANOSLEEP_SUPPORT, ZSTD_SETPRIORITY_SUPPORT */
#include <stdlib.h>       /* malloc, realloc, free */
#include <stddef.h>       /* size_t, ptrdiff_t */
#include <stdio.h>        /* fprintf */
#include <sys/types.h>    /* stat, utime */
#include <sys/stat.h>     /* stat, chmod */
#if defined(_WIN32)
#  include <sys/utime.h>  /* utime */
#  include <io.h>         /* _chmod */
#else
#  include <unistd.h>     /* chown, stat */
#if PLATFORM_POSIX_VERSION < 200809L
#  include <utime.h>      /* utime */
#else
#  include <fcntl.h>      /* AT_FDCWD */
#  include <sys/stat.h>   /* utimensat */
#endif
#endif
#include <time.h>         /* clock_t, clock, CLOCKS_PER_SEC, nanosleep */
#include "mem.h"          /* U32, U64 */

/*-************************************************************
* Avoid fseek()'s 2GiB barrier with MSVC, macOS, *BSD, MinGW
***************************************************************/
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#   define UTIL_fseek _fseeki64
#elif !defined(__64BIT__) && (PLATFORM_POSIX_VERSION >= 200112L) /* No point defining Large file for 64 bit */
#  define UTIL_fseek fseeko
#elif defined(__MINGW32__) && defined(__MSVCRT__) && !defined(__STRICT_ANSI__) && !defined(__NO_MINGW_LFS)
#   define UTIL_fseek fseeko64
#else
#   define UTIL_fseek fseek
#endif


/*-*************************************************
*  Sleep & priority functions: Windows - Posix - others
***************************************************/
#if defined(_WIN32)
#  include <windows.h>
#  define SET_REALTIME_PRIORITY SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS)
#  define UTIL_sleep(s) Sleep(1000*s)
#  define UTIL_sleepMilli(milli) Sleep(milli)

#elif PLATFORM_POSIX_VERSION > 0 /* Unix-like operating system */
#  include <unistd.h>   /* sleep */
#  define UTIL_sleep(s) sleep(s)
#  if ZSTD_NANOSLEEP_SUPPORT   /* necessarily defined in platform.h */
#      define UTIL_sleepMilli(milli) { struct timespec t; t.tv_sec=0; t.tv_nsec=milli*1000000ULL; nanosleep(&t, NULL); }
#  else
#      define UTIL_sleepMilli(milli) /* disabled */
#  endif
#  if ZSTD_SETPRIORITY_SUPPORT
#    include <sys/resource.h> /* setpriority */
#    define SET_REALTIME_PRIORITY setpriority(PRIO_PROCESS, 0, -20)
#  else
#    define SET_REALTIME_PRIORITY /* disabled */
#  endif

#else  /* unknown non-unix operating systen */
#  define UTIL_sleep(s)          /* disabled */
#  define UTIL_sleepMilli(milli) /* disabled */
#  define SET_REALTIME_PRIORITY  /* disabled */
#endif


/*-*************************************
*  Constants
***************************************/
#define LIST_SIZE_INCREASE   (8*1024)
#define MAX_FILE_OF_FILE_NAMES_SIZE (1<<20)*50

/*-****************************************
*  Compiler specifics
******************************************/
#if defined(__INTEL_COMPILER)
#  pragma warning(disable : 177)    /* disable: message #177: function was declared but never referenced, useful with UTIL_STATIC */
#endif
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
*  Console log
******************************************/
extern int g_utilDisplayLevel;
#define UTIL_DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define UTIL_DISPLAYLEVEL(l, ...) { if (g_utilDisplayLevel>=l) { UTIL_DISPLAY(__VA_ARGS__); } }


/*-****************************************
*  File functions
******************************************/
#if defined(_MSC_VER)
    #define chmod _chmod
    typedef struct __stat64 stat_t;
#else
    typedef struct stat stat_t;
#endif


int UTIL_fileExist(const char* filename);
int UTIL_isRegularFile(const char* infilename);
int UTIL_setFileStat(const char* filename, stat_t* statbuf);
U32 UTIL_isDirectory(const char* infilename);
int UTIL_getFileStat(const char* infilename, stat_t* statbuf);
int UTIL_isSameFile(const char* file1, const char* file2);
int UTIL_compareStr(const void *p1, const void *p2);
int UTIL_isCompressedFile(const char* infilename, const char *extensionList[]);
const char* UTIL_getFileExtension(const char* infilename);

#ifndef _MSC_VER
U32 UTIL_isFIFO(const char* infilename);
#endif
U32 UTIL_isLink(const char* infilename);
#define UTIL_FILESIZE_UNKNOWN  ((U64)(-1))
U64 UTIL_getFileSize(const char* infilename);

U64 UTIL_getTotalFileSize(const char* const * fileNamesTable, unsigned nbFiles);


/*-****************************************
 *  Lists of Filenames
 ******************************************/

#ifdef _WIN32
#  define UTIL_HAS_CREATEFILELIST
#elif defined(__linux__) || (PLATFORM_POSIX_VERSION >= 200112L)  /* opendir, readdir require POSIX.1-2001 */
#  define UTIL_HAS_CREATEFILELIST
#else
   /* do not define UTIL_HAS_CREATEFILELIST */
#endif /* #ifdef _WIN32 */

typedef struct
{
    const char** fileNames;
    char* buf;            /* fileNames are stored in this buffer (or are read-only) */
    size_t tableSize;     /* nb of fileNames */
    size_t tableCapacity;
} FileNamesTable;

/*! UTIL_createFileNamesTable_fromFileName() :
 *  read filenames from @inputFileName, and store them into returned object.
 * @return : a FileNamesTable*, or NULL in case of error (ex: @inputFileName doesn't exist).
 *  Note: inputFileSize must be less than 50MB
 */
FileNamesTable*
UTIL_createFileNamesTable_fromFileName(const char* inputFileName);

/*! UTIL_assembleFileNamesTable() :
 *  This function takes ownership of its arguments, @filenames and @buf,
 *  and store them inside the created object.
 * @return : FileNamesTable*, or NULL, if allocation fails.
 */
FileNamesTable*
UTIL_assembleFileNamesTable(const char** filenames, size_t tableSize, char* buf);

/*! UTIL_freeFileNamesTable() :
 *  This function is compatible with NULL argument and never fails.
 */
void UTIL_freeFileNamesTable(FileNamesTable* table);

/*! UTIL_mergeFileNamesTable():
 * @return : FileNamesTable*, concatenation of @table1 and @table2
 *  note: @table1 and @table2 are consumed (freed) by this operation
 */
FileNamesTable*
UTIL_mergeFileNamesTable(FileNamesTable* table1, FileNamesTable* table2);


/*! UTIL_expandFNT() :
 *  read names from @fnt, and expand those corresponding to directories
 *  update @fnt, now containing only file names,
 * @return : 0 in case of success, 1 if error
 *  note : in case of error, @fnt[0] is NULL
 */
void UTIL_expandFNT(FileNamesTable** fnt, int followLinks);

/*! UTIL_createFNT_fromROTable() :
 *  copy the @filenames pointer table inside the returned object.
 *  The names themselves are still stored in their original buffer, which must outlive the object.
 * @return : a FileNamesTable* object,
 *        or NULL in case of error
 */
FileNamesTable* UTIL_createFNT_fromROTable(const char** filenames, size_t nbFilenames);

/*! UTIL_createExpandedFNT() :
 *  read names from @filenames, and expand those corresponding to directories
 * @return : an expanded FileNamesTable*, where each name is a file
 *        or NULL in case of error
 */
FileNamesTable* UTIL_createExpandedFNT(const char** filenames, size_t nbFilenames, int followLinks);


/*! UTIL_allocateFileNamesTable() :
 *  Allocates a table of const char*, to insert read-only names later on.
 *  The created FileNamesTable* doesn't hold a buffer.
 * @return : FileNamesTable*, or NULL, if allocation fails.
 */
FileNamesTable* UTIL_allocateFileNamesTable(size_t tableSize);


/*! UTIL_refFilename() :
 *  Add a read-only name to reference into @fnt table.
 *  Since @filename is only referenced, its lifetime must outlive @fnt.
 *  This function never fails, but it can abort().
 *  Internal table must be large enough to reference a new member
 *  (capacity > size), otherwise the function will abort().
 */
void UTIL_refFilename(FileNamesTable* fnt, const char* filename);


/*-****************************************
 *  System
 ******************************************/

int UTIL_countPhysicalCores(void);


#if defined (__cplusplus)
}
#endif

#endif /* UTIL_H_MODULE */
