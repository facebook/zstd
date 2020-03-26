/*
 * Copyright (c) 2016-2020, Przemyslaw Skibinski, Yann Collet, Facebook, Inc.
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
#include "util.h"       /* note : ensure that platform.h is included first ! */
#include <stdlib.h>     /* malloc, realloc, free */
#include <stdio.h>      /* fprintf */
#include <time.h>       /* clock_t, clock, CLOCKS_PER_SEC, nanosleep */
#include <errno.h>
#include <assert.h>

#if defined(_WIN32)
#  include <sys/utime.h>  /* utime */
#  include <io.h>         /* _chmod */
#else
#  include <unistd.h>     /* chown, stat */
#  if PLATFORM_POSIX_VERSION < 200809L
#    include <utime.h>    /* utime */
#  else
#    include <fcntl.h>    /* AT_FDCWD */
#    include <sys/stat.h> /* utimensat */
#  endif
#endif

#if defined(_MSC_VER) || defined(__MINGW32__) || defined (__MSVCRT__)
#include <direct.h>     /* needed for _mkdir in windows */
#endif

#if defined(__linux__) || (PLATFORM_POSIX_VERSION >= 200112L)  /* opendir, readdir require POSIX.1-2001 */
#  include <dirent.h>       /* opendir, readdir */
#  include <string.h>       /* strerror, memcpy */
#endif /* #ifdef _WIN32 */


/*-****************************************
*  Internal Macros
******************************************/

/* CONTROL is almost like an assert(), but is never disabled.
 * It's designed for failures that may happen rarely,
 * but we don't want to maintain a specific error code path for them,
 * such as a malloc() returning NULL for example.
 * Since it's always active, this macro can trigger side effects.
 */
#define CONTROL(c)  {         \
    if (!(c)) {               \
        UTIL_DISPLAYLEVEL(1, "Error : %s, %i : %s",  \
                          __FILE__, __LINE__, #c);   \
        exit(1);              \
}   }

/* console log */
#define UTIL_DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define UTIL_DISPLAYLEVEL(l, ...) { if (g_utilDisplayLevel>=l) { UTIL_DISPLAY(__VA_ARGS__); } }

/* A modified version of realloc().
 * If UTIL_realloc() fails the original block is freed.
 */
UTIL_STATIC void* UTIL_realloc(void *ptr, size_t size)
{
    void *newptr = realloc(ptr, size);
    if (newptr) return newptr;
    free(ptr);
    return NULL;
}

#if defined(_MSC_VER)
    #define chmod _chmod
#endif


/*-****************************************
*  Console log
******************************************/
int g_utilDisplayLevel;


/*-*************************************
*  Constants
***************************************/
#define LIST_SIZE_INCREASE   (8*1024)
#define MAX_FILE_OF_FILE_NAMES_SIZE (1<<20)*50


/*-*************************************
*  Functions
***************************************/

int UTIL_fileExist(const char* filename)
{
    stat_t statbuf;
#if defined(_MSC_VER)
    int const stat_error = _stat64(filename, &statbuf);
#else
    int const stat_error = stat(filename, &statbuf);
#endif
    return !stat_error;
}

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

/* like chmod, but avoid changing permission of /dev/null */
int UTIL_chmod(char const* filename, mode_t permissions)
{
    if (!strcmp(filename, "/dev/null")) return 0;   /* pretend success, but don't change anything */
    return chmod(filename, permissions);
}

int UTIL_setFileStat(const char *filename, stat_t *statbuf)
{
    int res = 0;

    if (!UTIL_isRegularFile(filename))
        return -1;

    /* set access and modification times */
    /* We check that st_mtime is a macro here in order to give us confidence
     * that struct stat has a struct timespec st_mtim member. We need this
     * check because there are some platforms that claim to be POSIX 2008
     * compliant but which do not have st_mtim... */
#if (PLATFORM_POSIX_VERSION >= 200809L) && defined(st_mtime)
    {
        /* (atime, mtime) */
        struct timespec timebuf[2] = { {0, UTIME_NOW} };
        timebuf[1] = statbuf->st_mtim;
        res += utimensat(AT_FDCWD, filename, timebuf, 0);
    }
#else
    {
        struct utimbuf timebuf;
        timebuf.actime = time(NULL);
        timebuf.modtime = statbuf->st_mtime;
        res += utime(filename, &timebuf);
    }
#endif

#if !defined(_WIN32)
    res += chown(filename, statbuf->st_uid, statbuf->st_gid);  /* Copy ownership */
#endif

    res += UTIL_chmod(filename, statbuf->st_mode & 07777);  /* Copy file permissions */

    errno = 0;
    return -res; /* number of errors is returned */
}

int UTIL_isDirectory(const char* infilename)
{
    stat_t statbuf;
#if defined(_MSC_VER)
    int const r = _stat64(infilename, &statbuf);
    if (!r && (statbuf.st_mode & _S_IFDIR)) return 1;
#else
    int const r = stat(infilename, &statbuf);
    if (!r && S_ISDIR(statbuf.st_mode)) return 1;
#endif
    return 0;
}

int UTIL_compareStr(const void *p1, const void *p2) {
    return strcmp(* (char * const *) p1, * (char * const *) p2);
}

int UTIL_isSameFile(const char* fName1, const char* fName2)
{
    assert(fName1 != NULL); assert(fName2 != NULL);
#if defined(_MSC_VER) || defined(_WIN32)
    /* note : Visual does not support file identification by inode.
     *        inode does not work on Windows, even with a posix layer, like msys2.
     *        The following work-around is limited to detecting exact name repetition only,
     *        aka `filename` is considered different from `subdir/../filename` */
    return !strcmp(fName1, fName2);
#else
    {   stat_t file1Stat;
        stat_t file2Stat;
        return UTIL_getFileStat(fName1, &file1Stat)
            && UTIL_getFileStat(fName2, &file2Stat)
            && (file1Stat.st_dev == file2Stat.st_dev)
            && (file1Stat.st_ino == file2Stat.st_ino);
    }
#endif
}

/* UTIL_isFIFO : distinguish named pipes */
int UTIL_isFIFO(const char* infilename)
{
/* macro guards, as defined in : https://linux.die.net/man/2/lstat */
#if PLATFORM_POSIX_VERSION >= 200112L
    stat_t statbuf;
    int const r = UTIL_getFileStat(infilename, &statbuf);
    if (!r && S_ISFIFO(statbuf.st_mode)) return 1;
#endif
    (void)infilename;
    return 0;
}

int UTIL_isLink(const char* infilename)
{
/* macro guards, as defined in : https://linux.die.net/man/2/lstat */
#if PLATFORM_POSIX_VERSION >= 200112L
    stat_t statbuf;
    int const r = lstat(infilename, &statbuf);
    if (!r && S_ISLNK(statbuf.st_mode)) return 1;
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


U64 UTIL_getTotalFileSize(const char* const * fileNamesTable, unsigned nbFiles)
{
    U64 total = 0;
    unsigned n;
    for (n=0; n<nbFiles; n++) {
        U64 const size = UTIL_getFileSize(fileNamesTable[n]);
        if (size == UTIL_FILESIZE_UNKNOWN) return UTIL_FILESIZE_UNKNOWN;
        total += size;
    }
    return total;
}


/* condition : @file must be valid, and not have reached its end.
 * @return : length of line written into @buf, ended with `\0` instead of '\n',
 *           or 0, if there is no new line */
static size_t readLineFromFile(char* buf, size_t len, FILE* file)
{
    assert(!feof(file));
    /* Work around Cygwin problem when len == 1 it returns NULL. */
    if (len <= 1) return 0;
    CONTROL( fgets(buf, (int) len, file) );
    {   size_t linelen = strlen(buf);
        if (strlen(buf)==0) return 0;
        if (buf[linelen-1] == '\n') linelen--;
        buf[linelen] = '\0';
        return linelen+1;
    }
}

/* Conditions :
 *   size of @inputFileName file must be < @dstCapacity
 *   @dst must be initialized
 * @return : nb of lines
 *       or -1 if there's an error
 */
static int
readLinesFromFile(void* dst, size_t dstCapacity,
            const char* inputFileName)
{
    int nbFiles = 0;
    size_t pos = 0;
    char* const buf = (char*)dst;
    FILE* const inputFile = fopen(inputFileName, "r");

    assert(dst != NULL);

    if(!inputFile) {
        if (g_utilDisplayLevel >= 1) perror("zstd:util:readLinesFromFile");
        return -1;
    }

    while ( !feof(inputFile) ) {
        size_t const lineLength = readLineFromFile(buf+pos, dstCapacity-pos, inputFile);
        if (lineLength == 0) break;
        assert(pos + lineLength < dstCapacity);
        pos += lineLength;
        ++nbFiles;
    }

    CONTROL( fclose(inputFile) == 0 );

    return nbFiles;
}

/*Note: buf is not freed in case function successfully created table because filesTable->fileNames[0] = buf*/
FileNamesTable*
UTIL_createFileNamesTable_fromFileName(const char* inputFileName)
{
    size_t nbFiles = 0;
    char* buf;
    size_t bufSize;
    size_t pos = 0;

    if (!UTIL_fileExist(inputFileName) || !UTIL_isRegularFile(inputFileName))
        return NULL;

    {   U64 const inputFileSize = UTIL_getFileSize(inputFileName);
        if(inputFileSize > MAX_FILE_OF_FILE_NAMES_SIZE)
            return NULL;
        bufSize = (size_t)(inputFileSize + 1); /* (+1) to add '\0' at the end of last filename */
    }

    buf = (char*) malloc(bufSize);
    CONTROL( buf != NULL );

    {   int const ret_nbFiles = readLinesFromFile(buf, bufSize, inputFileName);

        if (ret_nbFiles <= 0) {
          free(buf);
          return NULL;
        }
        nbFiles = (size_t)ret_nbFiles;
    }

    {   const char** filenamesTable = (const char**) malloc(nbFiles * sizeof(*filenamesTable));
        CONTROL(filenamesTable != NULL);

        {   size_t fnb;
            for (fnb = 0, pos = 0; fnb < nbFiles; fnb++) {
                filenamesTable[fnb] = buf+pos;
                pos += strlen(buf+pos)+1;  /* +1 for the finishing `\0` */
        }   }
        assert(pos <= bufSize);

        return UTIL_assembleFileNamesTable(filenamesTable, nbFiles, buf);
    }
}

static FileNamesTable*
UTIL_assembleFileNamesTable2(const char** filenames, size_t tableSize, size_t tableCapacity, char* buf)
{
    FileNamesTable* const table = (FileNamesTable*) malloc(sizeof(*table));
    CONTROL(table != NULL);
    table->fileNames = filenames;
    table->buf = buf;
    table->tableSize = tableSize;
    table->tableCapacity = tableCapacity;
    return table;
}

FileNamesTable*
UTIL_assembleFileNamesTable(const char** filenames, size_t tableSize, char* buf)
{
    return UTIL_assembleFileNamesTable2(filenames, tableSize, tableSize, buf);
}

void UTIL_freeFileNamesTable(FileNamesTable* table)
{
    if (table==NULL) return;
    free((void*)table->fileNames);
    free(table->buf);
    free(table);
}

FileNamesTable* UTIL_allocateFileNamesTable(size_t tableSize)
{
    const char** const fnTable = (const char**)malloc(tableSize * sizeof(*fnTable));
    FileNamesTable* fnt;
    if (fnTable==NULL) return NULL;
    fnt = UTIL_assembleFileNamesTable(fnTable, tableSize, NULL);
    fnt->tableSize = 0;   /* the table is empty */
    return fnt;
}

void UTIL_refFilename(FileNamesTable* fnt, const char* filename)
{
    assert(fnt->tableSize < fnt->tableCapacity);
    fnt->fileNames[fnt->tableSize] = filename;
    fnt->tableSize++;
}

static size_t getTotalTableSize(FileNamesTable* table)
{
    size_t fnb = 0, totalSize = 0;
    for(fnb = 0 ; fnb < table->tableSize && table->fileNames[fnb] ; ++fnb) {
        totalSize += strlen(table->fileNames[fnb]) + 1; /* +1 to add '\0' at the end of each fileName */
    }
    return totalSize;
}

FileNamesTable*
UTIL_mergeFileNamesTable(FileNamesTable* table1, FileNamesTable* table2)
{
    unsigned newTableIdx = 0;
    size_t pos = 0;
    size_t newTotalTableSize;
    char* buf;

    FileNamesTable* const newTable = UTIL_assembleFileNamesTable(NULL, 0, NULL);
    CONTROL( newTable != NULL );

    newTotalTableSize = getTotalTableSize(table1) + getTotalTableSize(table2);

    buf = (char*) calloc(newTotalTableSize, sizeof(*buf));
    CONTROL ( buf != NULL );

    newTable->buf = buf;
    newTable->tableSize = table1->tableSize + table2->tableSize;
    newTable->fileNames = (const char **) calloc(newTable->tableSize, sizeof(*(newTable->fileNames)));
    CONTROL ( newTable->fileNames != NULL );

    {   unsigned idx1;
        for( idx1=0 ; (idx1 < table1->tableSize) && table1->fileNames[idx1] && (pos < newTotalTableSize); ++idx1, ++newTableIdx) {
            size_t const curLen = strlen(table1->fileNames[idx1]);
            memcpy(buf+pos, table1->fileNames[idx1], curLen);
            assert(newTableIdx <= newTable->tableSize);
            newTable->fileNames[newTableIdx] = buf+pos;
            pos += curLen+1;
    }   }

    {   unsigned idx2;
        for( idx2=0 ; (idx2 < table2->tableSize) && table2->fileNames[idx2] && (pos < newTotalTableSize) ; ++idx2, ++newTableIdx) {
            size_t const curLen = strlen(table2->fileNames[idx2]);
            memcpy(buf+pos, table2->fileNames[idx2], curLen);
            assert(newTableIdx <= newTable->tableSize);
            newTable->fileNames[newTableIdx] = buf+pos;
            pos += curLen+1;
    }   }
    assert(pos <= newTotalTableSize);
    newTable->tableSize = newTableIdx;

    UTIL_freeFileNamesTable(table1);
    UTIL_freeFileNamesTable(table2);

    return newTable;
}

#ifdef _WIN32
static int UTIL_prepareFileList(const char* dirName,
                                char** bufStart, size_t* pos,
                                char** bufEnd, int followLinks)
{
    char* path;
    size_t dirLength, pathLength;
    int nbFiles = 0;
    WIN32_FIND_DATAA cFile;
    HANDLE hFile;

    dirLength = strlen(dirName);
    path = (char*) malloc(dirLength + 3);
    if (!path) return 0;

    memcpy(path, dirName, dirLength);
    path[dirLength] = '\\';
    path[dirLength+1] = '*';
    path[dirLength+2] = 0;

    hFile=FindFirstFileA(path, &cFile);
    if (hFile == INVALID_HANDLE_VALUE) {
        UTIL_DISPLAYLEVEL(1, "Cannot open directory '%s'\n", dirName);
        return 0;
    }
    free(path);

    do {
        size_t const fnameLength = strlen(cFile.cFileName);
        path = (char*) malloc(dirLength + fnameLength + 2);
        if (!path) { FindClose(hFile); return 0; }
        memcpy(path, dirName, dirLength);
        path[dirLength] = '\\';
        memcpy(path+dirLength+1, cFile.cFileName, fnameLength);
        pathLength = dirLength+1+fnameLength;
        path[pathLength] = 0;
        if (cFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if ( strcmp (cFile.cFileName, "..") == 0
              || strcmp (cFile.cFileName, ".") == 0 )
                continue;
            /* Recursively call "UTIL_prepareFileList" with the new path. */
            nbFiles += UTIL_prepareFileList(path, bufStart, pos, bufEnd, followLinks);
            if (*bufStart == NULL) { free(path); FindClose(hFile); return 0; }
        } else if ( (cFile.dwFileAttributes & FILE_ATTRIBUTE_NORMAL)
                 || (cFile.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)
                 || (cFile.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED) ) {
            if (*bufStart + *pos + pathLength >= *bufEnd) {
                ptrdiff_t const newListSize = (*bufEnd - *bufStart) + LIST_SIZE_INCREASE;
                *bufStart = (char*)UTIL_realloc(*bufStart, newListSize);
                if (*bufStart == NULL) { free(path); FindClose(hFile); return 0; }
                *bufEnd = *bufStart + newListSize;
            }
            if (*bufStart + *pos + pathLength < *bufEnd) {
                memcpy(*bufStart + *pos, path, pathLength+1 /* include final \0 */);
                *pos += pathLength + 1;
                nbFiles++;
        }   }
        free(path);
    } while (FindNextFileA(hFile, &cFile));

    FindClose(hFile);
    return nbFiles;
}

#elif defined(__linux__) || (PLATFORM_POSIX_VERSION >= 200112L)  /* opendir, readdir require POSIX.1-2001 */

static int UTIL_prepareFileList(const char *dirName,
                                char** bufStart, size_t* pos,
                                char** bufEnd, int followLinks)
{
    DIR* dir;
    struct dirent * entry;
    size_t dirLength;
    int nbFiles = 0;

    if (!(dir = opendir(dirName))) {
        UTIL_DISPLAYLEVEL(1, "Cannot open directory '%s': %s\n", dirName, strerror(errno));
        return 0;
    }

    dirLength = strlen(dirName);
    errno = 0;
    while ((entry = readdir(dir)) != NULL) {
        char* path;
        size_t fnameLength, pathLength;
        if (strcmp (entry->d_name, "..") == 0 ||
            strcmp (entry->d_name, ".") == 0) continue;
        fnameLength = strlen(entry->d_name);
        path = (char*) malloc(dirLength + fnameLength + 2);
        if (!path) { closedir(dir); return 0; }
        memcpy(path, dirName, dirLength);

        path[dirLength] = '/';
        memcpy(path+dirLength+1, entry->d_name, fnameLength);
        pathLength = dirLength+1+fnameLength;
        path[pathLength] = 0;

        if (!followLinks && UTIL_isLink(path)) {
            UTIL_DISPLAYLEVEL(2, "Warning : %s is a symbolic link, ignoring\n", path);
            free(path);
            continue;
        }

        if (UTIL_isDirectory(path)) {
            nbFiles += UTIL_prepareFileList(path, bufStart, pos, bufEnd, followLinks);  /* Recursively call "UTIL_prepareFileList" with the new path. */
            if (*bufStart == NULL) { free(path); closedir(dir); return 0; }
        } else {
            if (*bufStart + *pos + pathLength >= *bufEnd) {
                ptrdiff_t newListSize = (*bufEnd - *bufStart) + LIST_SIZE_INCREASE;
                assert(newListSize >= 0);
                *bufStart = (char*)UTIL_realloc(*bufStart, (size_t)newListSize);
                *bufEnd = *bufStart + newListSize;
                if (*bufStart == NULL) { free(path); closedir(dir); return 0; }
            }
            if (*bufStart + *pos + pathLength < *bufEnd) {
                memcpy(*bufStart + *pos, path, pathLength + 1);  /* with final \0 */
                *pos += pathLength + 1;
                nbFiles++;
        }   }
        free(path);
        errno = 0; /* clear errno after UTIL_isDirectory, UTIL_prepareFileList */
    }

    if (errno != 0) {
        UTIL_DISPLAYLEVEL(1, "readdir(%s) error: %s \n", dirName, strerror(errno));
        free(*bufStart);
        *bufStart = NULL;
    }
    closedir(dir);
    return nbFiles;
}

#else

static int UTIL_prepareFileList(const char *dirName,
                                char** bufStart, size_t* pos,
                                char** bufEnd, int followLinks)
{
    (void)bufStart; (void)bufEnd; (void)pos; (void)followLinks;
    UTIL_DISPLAYLEVEL(1, "Directory %s ignored (compiled without _WIN32 or _POSIX_C_SOURCE) \n", dirName);
    return 0;
}

#endif /* #ifdef _WIN32 */

int UTIL_isCompressedFile(const char *inputName, const char *extensionList[])
{
  const char* ext = UTIL_getFileExtension(inputName);
  while(*extensionList!=NULL)
  {
    const int isCompressedExtension = strcmp(ext,*extensionList);
    if(isCompressedExtension==0)
      return 1;
    ++extensionList;
  }
   return 0;
}

/*Utility function to get file extension from file */
const char* UTIL_getFileExtension(const char* infilename)
{
   const char* extension = strrchr(infilename, '.');
   if(!extension || extension==infilename) return "";
   return extension;
}


FileNamesTable*
UTIL_createExpandedFNT(const char** inputNames, size_t nbIfns, int followLinks)
{
    unsigned nbFiles;
    char* buf = (char*)malloc(LIST_SIZE_INCREASE);
    char* bufend = buf + LIST_SIZE_INCREASE;

    if (!buf) return NULL;

    {   size_t ifnNb, pos;
        for (ifnNb=0, pos=0, nbFiles=0; ifnNb<nbIfns; ifnNb++) {
            if (!UTIL_isDirectory(inputNames[ifnNb])) {
                size_t const len = strlen(inputNames[ifnNb]);
                if (buf + pos + len >= bufend) {
                    ptrdiff_t newListSize = (bufend - buf) + LIST_SIZE_INCREASE;
                    assert(newListSize >= 0);
                    buf = (char*)UTIL_realloc(buf, (size_t)newListSize);
                    if (!buf) return NULL;
                    bufend = buf + newListSize;
                }
                if (buf + pos + len < bufend) {
                    memcpy(buf+pos, inputNames[ifnNb], len+1);  /* including final \0 */
                    pos += len + 1;
                    nbFiles++;
                }
            } else {
                nbFiles += (unsigned)UTIL_prepareFileList(inputNames[ifnNb], &buf, &pos, &bufend, followLinks);
                if (buf == NULL) return NULL;
    }   }   }

    /* note : even if nbFiles==0, function returns a valid, though empty, FileNamesTable* object */

    {   size_t ifnNb, pos;
        size_t const fntCapacity = nbFiles + 1;  /* minimum 1, allows adding one reference, typically stdin */
        const char** const fileNamesTable = (const char**)malloc(fntCapacity * sizeof(*fileNamesTable));
        if (!fileNamesTable) { free(buf); return NULL; }

        for (ifnNb = 0, pos = 0; ifnNb < nbFiles; ifnNb++) {
            fileNamesTable[ifnNb] = buf + pos;
            if (buf + pos > bufend) { free(buf); free((void*)fileNamesTable); return NULL; }
            pos += strlen(fileNamesTable[ifnNb]) + 1;
        }
        return UTIL_assembleFileNamesTable2(fileNamesTable, nbFiles, fntCapacity, buf);
    }
}


void UTIL_expandFNT(FileNamesTable** fnt, int followLinks)
{
    FileNamesTable* const newFNT = UTIL_createExpandedFNT((*fnt)->fileNames, (*fnt)->tableSize, followLinks);
    CONTROL(newFNT != NULL);
    UTIL_freeFileNamesTable(*fnt);
    *fnt = newFNT;
}

FileNamesTable* UTIL_createFNT_fromROTable(const char** filenames, size_t nbFilenames)
{
    size_t const sizeof_FNTable = nbFilenames * sizeof(*filenames);
    const char** const newFNTable = (const char**)malloc(sizeof_FNTable);
    if (newFNTable==NULL) return NULL;
    memcpy((void*)newFNTable, filenames, sizeof_FNTable);  /* void* : mitigate a Visual compiler bug or limitation */
    return UTIL_assembleFileNamesTable(newFNTable, nbFilenames, NULL);
}


/*-****************************************
*  count the number of physical cores
******************************************/

#if defined(_WIN32) || defined(WIN32)

#include <windows.h>

typedef BOOL(WINAPI* LPFN_GLPI)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);

int UTIL_countPhysicalCores(void)
{
    static int numPhysicalCores = 0;
    if (numPhysicalCores != 0) return numPhysicalCores;

    {   LPFN_GLPI glpi;
        BOOL done = FALSE;
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
        DWORD returnLength = 0;
        size_t byteOffset = 0;

#if defined(_MSC_VER)
/* Visual Studio does not like the following cast */
#   pragma warning( disable : 4054 )  /* conversion from function ptr to data ptr */
#   pragma warning( disable : 4055 )  /* conversion from data ptr to function ptr */
#endif
        glpi = (LPFN_GLPI)(void*)GetProcAddress(GetModuleHandle(TEXT("kernel32")),
                                               "GetLogicalProcessorInformation");

        if (glpi == NULL) {
            goto failed;
        }

        while(!done) {
            DWORD rc = glpi(buffer, &returnLength);
            if (FALSE == rc) {
                if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                    if (buffer)
                        free(buffer);
                    buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(returnLength);

                    if (buffer == NULL) {
                        perror("zstd");
                        exit(1);
                    }
                } else {
                    /* some other error */
                    goto failed;
                }
            } else {
                done = TRUE;
        }   }

        ptr = buffer;

        while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength) {

            if (ptr->Relationship == RelationProcessorCore) {
                numPhysicalCores++;
            }

            ptr++;
            byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        }

        free(buffer);

        return numPhysicalCores;
    }

failed:
    /* try to fall back on GetSystemInfo */
    {   SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        numPhysicalCores = sysinfo.dwNumberOfProcessors;
        if (numPhysicalCores == 0) numPhysicalCores = 1; /* just in case */
    }
    return numPhysicalCores;
}

#elif defined(__APPLE__)

#include <sys/sysctl.h>

/* Use apple-provided syscall
 * see: man 3 sysctl */
int UTIL_countPhysicalCores(void)
{
    static S32 numPhysicalCores = 0; /* apple specifies int32_t */
    if (numPhysicalCores != 0) return numPhysicalCores;

    {   size_t size = sizeof(S32);
        int const ret = sysctlbyname("hw.physicalcpu", &numPhysicalCores, &size, NULL, 0);
        if (ret != 0) {
            if (errno == ENOENT) {
                /* entry not present, fall back on 1 */
                numPhysicalCores = 1;
            } else {
                perror("zstd: can't get number of physical cpus");
                exit(1);
            }
        }

        return numPhysicalCores;
    }
}

#elif defined(__linux__)

/* parse /proc/cpuinfo
 * siblings / cpu cores should give hyperthreading ratio
 * otherwise fall back on sysconf */
int UTIL_countPhysicalCores(void)
{
    static int numPhysicalCores = 0;

    if (numPhysicalCores != 0) return numPhysicalCores;

    numPhysicalCores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (numPhysicalCores == -1) {
        /* value not queryable, fall back on 1 */
        return numPhysicalCores = 1;
    }

    /* try to determine if there's hyperthreading */
    {   FILE* const cpuinfo = fopen("/proc/cpuinfo", "r");
#define BUF_SIZE 80
        char buff[BUF_SIZE];

        int siblings = 0;
        int cpu_cores = 0;
        int ratio = 1;

        if (cpuinfo == NULL) {
            /* fall back on the sysconf value */
            return numPhysicalCores;
        }

        /* assume the cpu cores/siblings values will be constant across all
         * present processors */
        while (!feof(cpuinfo)) {
            if (fgets(buff, BUF_SIZE, cpuinfo) != NULL) {
                if (strncmp(buff, "siblings", 8) == 0) {
                    const char* const sep = strchr(buff, ':');
                    if (sep == NULL || *sep == '\0') {
                        /* formatting was broken? */
                        goto failed;
                    }

                    siblings = atoi(sep + 1);
                }
                if (strncmp(buff, "cpu cores", 9) == 0) {
                    const char* const sep = strchr(buff, ':');
                    if (sep == NULL || *sep == '\0') {
                        /* formatting was broken? */
                        goto failed;
                    }

                    cpu_cores = atoi(sep + 1);
                }
            } else if (ferror(cpuinfo)) {
                /* fall back on the sysconf value */
                goto failed;
        }   }
        if (siblings && cpu_cores) {
            ratio = siblings / cpu_cores;
        }
failed:
        fclose(cpuinfo);
        return numPhysicalCores = numPhysicalCores / ratio;
    }
}

#elif defined(__FreeBSD__)

#include <sys/param.h>
#include <sys/sysctl.h>

/* Use physical core sysctl when available
 * see: man 4 smp, man 3 sysctl */
int UTIL_countPhysicalCores(void)
{
    static int numPhysicalCores = 0; /* freebsd sysctl is native int sized */
    if (numPhysicalCores != 0) return numPhysicalCores;

#if __FreeBSD_version >= 1300008
    {   size_t size = sizeof(numPhysicalCores);
        int ret = sysctlbyname("kern.smp.cores", &numPhysicalCores, &size, NULL, 0);
        if (ret == 0) return numPhysicalCores;
        if (errno != ENOENT) {
            perror("zstd: can't get number of physical cpus");
            exit(1);
        }
        /* sysctl not present, fall through to older sysconf method */
    }
#endif

    numPhysicalCores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (numPhysicalCores == -1) {
        /* value not queryable, fall back on 1 */
        numPhysicalCores = 1;
    }
    return numPhysicalCores;
}

#elif defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__CYGWIN__)

/* Use POSIX sysconf
 * see: man 3 sysconf */
int UTIL_countPhysicalCores(void)
{
    static int numPhysicalCores = 0;

    if (numPhysicalCores != 0) return numPhysicalCores;

    numPhysicalCores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (numPhysicalCores == -1) {
        /* value not queryable, fall back on 1 */
        return numPhysicalCores = 1;
    }
    return numPhysicalCores;
}

#else

int UTIL_countPhysicalCores(void)
{
    /* assume 1 */
    return 1;
}

#endif

#if defined (__cplusplus)
}
#endif
