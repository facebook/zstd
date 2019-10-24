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
#include "util.h"       /* note : ensure that platform.h is included first ! */
#include <errno.h>
#include <assert.h>

#if defined(_MSC_VER) || defined(__MINGW32__) || defined (__MSVCRT__)
#include <direct.h>     /* needed for _mkdir in windows */
#endif

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

int UTIL_setFileStat(const char *filename, stat_t *statbuf)
{
    int res = 0;

    if (!UTIL_isRegularFile(filename))
        return -1;

    /* set access and modification times */
#if defined(_WIN32) || (PLATFORM_POSIX_VERSION < 200809L)
    {
        struct utimbuf timebuf;
        timebuf.actime = time(NULL);
        timebuf.modtime = statbuf->st_mtime;
        res += utime(filename, &timebuf);
    }
#else
    {
        /* (atime, mtime) */
        struct timespec timebuf[2] = { {0, UTIME_NOW} };
        timebuf[1] = statbuf->st_mtim;
        res += utimensat(AT_FDCWD, filename, timebuf, 0);
    }
#endif

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

U32 UTIL_isLink(const char* infilename)
{
/* macro guards, as defined in : https://linux.die.net/man/2/lstat */
#if PLATFORM_POSIX_VERSION >= 200112L
    int r;
    stat_t statbuf;
    r = lstat(infilename, &statbuf);
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


int UTIL_readLineFromFile(char* buf, size_t len, FILE* file) {
    char* fgetsCheck = NULL;

    if (feof(file)) {
      UTIL_DISPLAYLEVEL(1, "[ERROR] end of file reached and need to read\n");
      return -1;
    }

    fgetsCheck = fgets(buf, (int) len, file);

    if(fgetsCheck == NULL || fgetsCheck != buf) {
      UTIL_DISPLAYLEVEL(1, "[ERROR][UTIL_readLineFromFile] fgets has a problem check: %s buf: %s \n",
        fgetsCheck == NULL ? "NULL" : fgetsCheck, buf);
      return -1;
    }

    return (int) strlen(buf)-1;   /* -1 to ignore '\n' character */
}

/* Warning: inputFileSize should be less than or equal buf capacity and buf should be initialized*/
static int readFromFile(char* buf, size_t inputFileSize, const char* inputFileName) {

  FILE* inputFile = fopen(inputFileName, "r");
  int nbFiles = -1;
  unsigned pos = 0;


  if(!buf) {
    UTIL_DISPLAYLEVEL(1, "[ERROR][UTIL_readFileNamesTableFromFile] Can't create buffer.\n");
    return -1;
  }

  if(!inputFile) {
    UTIL_DISPLAYLEVEL(1, "[ERROR][UTIL_readFileNamesTableFromFile] Can't open file to read input file names.\n");
    return -1;
  }

  for(nbFiles=0; !feof(inputFile) ; ) {
    if(UTIL_readLineFromFile(buf+pos, inputFileSize, inputFile) > 0) {
      int len = (int) strlen(buf+pos);
      buf[pos+len-1] = '\0'; /* replace '\n' with '\0'*/
      pos += len;
      ++nbFiles;
    }
  }

  fclose(inputFile);

  if(pos > inputFileSize) return -1;

  return nbFiles;
}

/*Note: buf is not freed in case function successfully created table because filesTable->fileNames[0] = buf*/
FileNamesTable*
UTIL_createFileNamesTable_fromFileName(const char* inputFileName) {
    U64 inputFileSize = 0;
    unsigned nbFiles = 0;
    int ret_nbFiles = -1;
    char* buf = NULL;
    size_t i = 0, pos = 0;

    FileNamesTable* filesTable = NULL;

    if(!UTIL_fileExist(inputFileName) || !UTIL_isRegularFile(inputFileName))
      return NULL;

    inputFileSize = UTIL_getFileSize(inputFileName) + 1; /* (+1) to add '\0' at the end of last filename */

    if(inputFileSize > MAX_FILE_OF_FILE_NAMES_SIZE)
      return NULL;

    buf = (char*) malloc((size_t) inputFileSize * sizeof(char));
    if(!buf) {
      UTIL_DISPLAYLEVEL(1, "[ERROR][UTIL_readFileNamesTableFromFile] Can't create buffer.\n");
      return NULL;
    }

    ret_nbFiles = readFromFile(buf, inputFileSize, inputFileName);

    if(ret_nbFiles <= 0) {
      free(buf);
      return NULL;
    }
    nbFiles = ret_nbFiles;

    filesTable = UTIL_createFileNamesTable(NULL, NULL, 0);
    if(!filesTable) {
      free(buf);
      UTIL_DISPLAYLEVEL(1, "[ERROR][UTIL_readFileNamesTableFromFile] Can't create table for files.\n");
      return NULL;
    }

    filesTable->tableSize = nbFiles;
    filesTable->fileNames = (const char**) malloc((nbFiles+1) * sizeof(char*));



    for(i = 0, pos = 0; i < nbFiles; ++i) {
      filesTable->fileNames[i] = buf+pos;
      pos += strlen(buf+pos)+1;
    }


    if(pos > inputFileSize){
      UTIL_freeFileNamesTable(filesTable);
      if(buf) free(buf);
      return NULL;
    }

    filesTable->buf = buf;

    return filesTable;
}

FileNamesTable*
UTIL_createFileNamesTable(const char** filenames, char* buf, size_t tableSize){
  FileNamesTable* table = (FileNamesTable*) malloc(sizeof(FileNamesTable));
  if(!table) {
    return NULL;
  }
  table->fileNames = filenames;
  table->buf = buf;
  table->tableSize = tableSize;
  return table;
}

void UTIL_freeFileNamesTable(FileNamesTable* table) {
  if(table) {
    if(table->fileNames) {
      free(table->fileNames);
    }

    if(table && table->buf) {
      free(table->buf);
    }

    free(table);
  }
}

static size_t getTotalTableSize(FileNamesTable* table) {
  size_t i = 0, totalSize = 0;
  for(i = 0 ; i < table->tableSize && table->fileNames[i] ; ++i) {
    totalSize += strlen(table->fileNames[i]) + 1; /* +1 to add '\0' at the end of each fileName */
  }

  return totalSize;
}

FileNamesTable*
UTIL_concatenateTwoTables(FileNamesTable* table1, FileNamesTable* table2) {
    unsigned newTableIdx = 0, idx1 = 0, idx2 = 0;
    size_t i = 0, pos = 0;
    size_t newTotalTableSize = 0;

    FileNamesTable* newTable = NULL;

    char* buf = NULL;


    newTable = UTIL_createFileNamesTable(NULL, NULL, 0);

    if(!newTable) {
      UTIL_DISPLAYLEVEL(1, "[ERROR][UTIL_concatenateTwoTables] Can't create new table for concatenation output.\n");
      return NULL;
    }

    newTotalTableSize = getTotalTableSize(table1) + getTotalTableSize(table2);

    buf = (char*) malloc(newTotalTableSize * sizeof(char));
    if(!buf) {
      UTIL_freeFileNamesTable(newTable);
      UTIL_DISPLAYLEVEL(1, "[ERROR][UTIL_concatenateTwoTables] Can't create buf for concatenation output.\n");
      return NULL;
    }

    for(i = 0; i < newTotalTableSize ; ++i) buf[i] = '\0';

    newTable->tableSize = table1->tableSize + table2->tableSize;
    newTable->fileNames = (const char **) malloc(newTable->tableSize * sizeof(char*));

    if(!newTable->fileNames) {
      UTIL_freeFileNamesTable(newTable);
      if(buf) free(buf);
      UTIL_DISPLAYLEVEL(1, "[ERROR][UTIL_concatenateTwoTables] Can't create new table for concatenation output.\n");
      return NULL;
    }

    for (i = 0; i < newTable->tableSize; ++i)
      newTable->fileNames[i] = NULL;

    for( ; idx1 < table1->tableSize && table1->fileNames[idx1] && pos < newTotalTableSize; ++idx1, ++newTableIdx) {
      size_t curLen = strlen(table1->fileNames[idx1]);
      memcpy(buf+pos, table1->fileNames[idx1], curLen);
      newTable->fileNames[newTableIdx] = buf+pos;
      pos += curLen+1;
    }


    for( ; idx2 < table2->tableSize && table2->fileNames[idx2] && pos < newTotalTableSize ; ++idx2, ++newTableIdx) {
      size_t curLen = strlen(table2->fileNames[idx2]);
      memcpy(buf+pos, table2->fileNames[idx2], curLen);
      newTable->fileNames[newTableIdx] = buf+pos;
      pos += curLen+1;
    }

    if(pos > newTotalTableSize) {
      UTIL_freeFileNamesTable(newTable);
      if(buf) free(buf);
      return NULL;
    }

    newTable->buf = buf;

    UTIL_freeFileNamesTable(table1);
    UTIL_freeFileNamesTable(table2);

    return newTable;
}

#ifdef _WIN32
int UTIL_prepareFileList(const char *dirName, char** bufStart, size_t* pos, char** bufEnd, int followLinks)
{
    char* path;
    int dirLength, fnameLength, pathLength, nbFiles = 0;
    WIN32_FIND_DATAA cFile;
    HANDLE hFile;

    dirLength = (int)strlen(dirName);
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
        fnameLength = (int)strlen(cFile.cFileName);
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
            }
        }
        free(path);
    } while (FindNextFileA(hFile, &cFile));

    FindClose(hFile);
    return nbFiles;
}

#elif defined(__linux__) || (PLATFORM_POSIX_VERSION >= 200112L)  /* opendir, readdir require POSIX.1-2001 */

int UTIL_prepareFileList(const char *dirName, char** bufStart, size_t* pos, char** bufEnd, int followLinks)
{
    DIR *dir;
    struct dirent *entry;
    char* path;
    size_t dirLength, fnameLength, pathLength;
    int nbFiles = 0;

    if (!(dir = opendir(dirName))) {
        UTIL_DISPLAYLEVEL(1, "Cannot open directory '%s': %s\n", dirName, strerror(errno));
        return 0;
    }

    dirLength = strlen(dirName);
    errno = 0;
    while ((entry = readdir(dir)) != NULL) {
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
            }
        }
        free(path);
        errno = 0; /* clear errno after UTIL_isDirectory, UTIL_prepareFileList */
    }

    if (errno != 0) {
        UTIL_DISPLAYLEVEL(1, "readdir(%s) error: %s\n", dirName, strerror(errno));
        free(*bufStart);
        *bufStart = NULL;
    }
    closedir(dir);
    return nbFiles;
}

#else

int UTIL_prepareFileList(const char *dirName, char** bufStart, size_t* pos, char** bufEnd, int followLinks)
{
    (void)bufStart; (void)bufEnd; (void)pos; (void)followLinks;
    UTIL_DISPLAYLEVEL(1, "Directory %s ignored (compiled without _WIN32 or _POSIX_C_SOURCE)\n", dirName);
    return 0;
}

#endif /* #ifdef _WIN32 */

/*
 * UTIL_createFileList - takes a list of files and directories (params: inputNames, inputNamesNb), scans directories,
 *                       and returns a new list of files (params: return value, allocatedBuffer, allocatedNamesNb).
 * After finishing usage of the list the structures should be freed with UTIL_freeFileList(params: return value, allocatedBuffer)
 * In case of error UTIL_createFileList returns NULL and UTIL_freeFileList should not be called.
 */
const char**
UTIL_createFileList(const char **inputNames, unsigned inputNamesNb,
                    char** allocatedBuffer, unsigned* allocatedNamesNb,
                    int followLinks)
{
    size_t pos;
    unsigned i, nbFiles;
    char* buf = (char*)malloc(LIST_SIZE_INCREASE);
    char* bufend = buf + LIST_SIZE_INCREASE;

    if (!buf) return NULL;

    for (i=0, pos=0, nbFiles=0; i<inputNamesNb; i++) {
        if (!UTIL_isDirectory(inputNames[i])) {
            size_t const len = strlen(inputNames[i]);
            if (buf + pos + len >= bufend) {
                ptrdiff_t newListSize = (bufend - buf) + LIST_SIZE_INCREASE;
                assert(newListSize >= 0);
                buf = (char*)UTIL_realloc(buf, (size_t)newListSize);
                bufend = buf + newListSize;
                if (!buf) return NULL;
            }
            if (buf + pos + len < bufend) {
                memcpy(buf+pos, inputNames[i], len+1);  /* including final \0 */
                pos += len + 1;
                nbFiles++;
            }
        } else {
            nbFiles += (unsigned)UTIL_prepareFileList(inputNames[i], &buf, &pos, &bufend, followLinks);
            if (buf == NULL) return NULL;
    }   }

    if (nbFiles == 0) { free(buf); return NULL; }

    {   const char** const fileTable = (const char**)malloc((nbFiles + 1) * sizeof(*fileTable));
        if (!fileTable) { free(buf); return NULL; }

        for (i = 0, pos = 0; i < nbFiles; i++) {
            fileTable[i] = buf + pos;
            if (buf + pos > bufend) { free(buf); free((void*)fileTable); return NULL; }
            pos += strlen(fileTable[i]) + 1;
        }

        *allocatedBuffer = buf;
        *allocatedNamesNb = nbFiles;

        return fileTable;
    }
}


/*-****************************************
*  Console log
******************************************/
int g_utilDisplayLevel;



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
            }
        }

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
            }
        }
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

#elif defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)

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
