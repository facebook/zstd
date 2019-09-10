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

char* UTIL_lastStrstr(char* haystack, char* needle) {
    char *result, *ptr, *h;
    
    h = haystack;
    result = NULL;
    if (*needle == '\0') {
        return h;
    }

    while (1) {
        ptr = strstr(h, needle); /* this will return NULL once we hit the last 'needle' */
        if (ptr == NULL)
            break;
        result = ptr;
        h = ptr + 1;
    }
    return result;
}

char* UTIL_getCwd(char* buf) {
    char* r;
#if defined(_MSC_VER) || defined(__MINGW32__) || defined (__MSVCRT__)
    r = _getcwd(buf, LIST_SIZE_INCREASE);
    if (errno != 0) {
        perror("UTIL_getCwd: ");
    }
#else
    r = getcwd(buf, LIST_SIZE_INCREASE);
    if (errno != 0) {
        perror("UTIL_getCwd: ");
    }
#endif
    errno = 0;
    return r;
}

int UTIL_createDir(const char* outDirName)
{
    int r;
    if (UTIL_isDirectory(outDirName))
        return 0;   /* no need to create if directory already exists */
#if defined(_MSC_VER) || defined(__MINGW32__) || defined (__MSVCRT__)
    r = _mkdir(outDirName);
#else
    r = mkdir(outDirName, S_IRWXU | S_IRWXG | S_IRWXO); /* dir has all permissions */
#endif
    if (r || !UTIL_isDirectory(outDirName)) {
        if (errno != 0) {
            perror("UTIL_createDir: ");
            errno = 0;
        }
        return 1;
    }
    errno = 0;
    return 0;   /* success */
}

#define MAXSYMLINKS 32
/* based off of realpath() from OpenBSD*/
char* UTIL_getRealPathPosixImpl(const char *path, char* resolved) {
#if PLATFORM_POSIX_VERSION >= 200112L
    char *p, *q, *s;
    size_t leftLen, resolvedLen;
    unsigned symlinks;
    int slen;
    char left[LIST_SIZE_INCREASE],
         next_token[LIST_SIZE_INCREASE],
         symlink[LIST_SIZE_INCREASE];

    symlinks = 0;
    if (path[0] == '/') {
        resolved[0] = '/';
        resolved[1] = '\0';
        if (path[1] == '\0')
            return (resolved);
        resolvedLen = 1;
        strcpy(left, path + 1);
        leftLen = strlen(left);
    } else {
        if (getcwd(resolved, LIST_SIZE_INCREASE) == NULL) {
            strcpy(resolved, ".");
            return (NULL);
        }
        resolvedLen = strlen(resolved);
        strcpy(left, path);
        leftLen = strlen(left);
    }
    if (leftLen >= sizeof(left) || resolvedLen >= LIST_SIZE_INCREASE) {
        errno = ENAMETOOLONG;
        return (NULL);
    }
    /* Iterate over path components in `left'. */
    while (leftLen != 0) {
        /* Extract the next path component and adjust `left' and its length. */
        p = strchr(left, '/');
        s = p ? p : left + leftLen;
        if ((unsigned long)(s - left) >= sizeof(next_token)) {
            errno = ENAMETOOLONG;
            return (NULL);
        }
        memcpy(next_token, left, s - left);
        next_token[s - left] = '\0';
        leftLen -= s - left;
        if (p != NULL)
            memmove(left, s + 1, leftLen + 1);
        if (resolved[resolvedLen - 1] != '/') {
            if (resolvedLen + 1 >= LIST_SIZE_INCREASE) {
                errno = ENAMETOOLONG;
                return (NULL);
            }
            resolved[resolvedLen++] = '/';
            resolved[resolvedLen] = '\0';
        }
        if (next_token[0] == '\0')
            continue;
        else if (strcmp(next_token, ".") == 0)
            continue;
        else if (strcmp(next_token, "..") == 0) {
            /*
             * Strip the last path component except when we have
             * single "/"
             */
            if (resolvedLen > 1) {
                resolved[resolvedLen - 1] = '\0';
                q = strrchr(resolved, '/') + 1;
                *q = '\0';
                resolvedLen = q - resolved;
            }
            continue;
        }
        strcat(resolved, next_token);
        resolvedLen = strlen(resolved);
        if (resolvedLen >= LIST_SIZE_INCREASE) {
            errno = ENAMETOOLONG;
            return (NULL);
        }
        if (UTIL_isLink(resolved)) {
            if (symlinks++ > MAXSYMLINKS /* 32 is too many levels of symlink */) {
                errno = ELOOP;
                return (NULL);
            }
            slen = readlink(resolved, symlink, sizeof(symlink) - 1);
            if (slen < 0)
                return (NULL);
            symlink[slen] = '\0';
            if (symlink[0] == '/') {
                resolved[1] = 0;
                resolvedLen = 1;
            } else if (resolvedLen > 1) {
                /* Strip the last path component. */
                resolved[resolvedLen - 1] = '\0';
                q = strrchr(resolved, '/') + 1;
                *q = '\0';
                resolvedLen = q - resolved;
            }
            /*
             * If there are any path components left, then
             * append them to symlink. The result is placed
             * in `left'.
             */
            if (p != NULL) {
                if (symlink[slen - 1] != '/') {
                    if ((unsigned long)slen + 1 >= sizeof(symlink)) {
                        errno = ENAMETOOLONG;
                        return (NULL);
                    }
                    symlink[slen] = '/';
                    symlink[slen + 1] = 0;
                }
                strcat(symlink, left);
                leftLen = strlen(symlink);
                if (leftLen >= sizeof(left)) {
                    errno = ENAMETOOLONG;
                    return (NULL);
                }
            }
            strcpy(left, symlink);
            leftLen = strlen(left);
        }
    }
    /*
     * Remove trailing slash except when the resolved pathname
     * is a single "/".
     */
    if (resolvedLen > 1 && resolved[resolvedLen - 1] == '/')
        resolved[resolvedLen - 1] = '\0';
    return (resolved);
#else
    UTIL_DISPLAYLEVEL(1, "Platform not compatible, not processing %s or %s", path, resolved);
    return NULL;
#endif
}

int UTIL_getRealPath(const char* relativePath, char* absolutePath) {
#if defined(_MSC_VER) || defined (__MINGW32__) || defined (__MSVCRT__)
    char *deepestAbsolutePathFolder, *pathExtension, *relativePathTemp, c, *r;
    c = '\\';
    r = _fullpath(absolutePath, relativePath, LIST_SIZE_INCREASE);
    if ((errno == ENOENT) && (absolutePath != NULL)) {
        /* 
         * directory doesn't already exist, so realpath will be too short,
         * but will contain a correct prefix until the unrecognized file = we must extend 
         */
        deepestAbsolutePathFolder = strrchr(absolutePath, c);   /* last folder/file currently in absolutePath */
        deepestAbsolutePathFolder++;    /* get rid of '/' */
        relativePathTemp = (char*)malloc((strlen(relativePath)+1)*sizeof(char));
        if (relativePathTemp) {
            memcpy(relativePathTemp, relativePath, strlen(relativePath));
        } else {
            UTIL_DISPLAYLEVEL(1, "Error allocating memory for relative path\n")
            return 1;
        }
        pathExtension = UTIL_lastStrstr(relativePathTemp, deepestAbsolutePathFolder);    /* ptr to last occurrence of last folder/file in relativePath */
        free(relativePathTemp);
        *deepestAbsolutePathFolder = '\0';
        strcat(absolutePath, pathExtension);    /* merge the correct prefix (absolutePath) with extension to desired target (pathExtension) */
        return 0;
    }
#elif PLATFORM_POSIX_VERSION >= 200112L
    char *r;
    r = UTIL_getRealPathPosixImpl(relativePath, absolutePath);
#else
    UTIL_DISPLAYLEVEL(1, "System doesn't support output dir functionality\n");
    return -1;   
#endif
    if (errno == 0 && r != NULL) {
        return 0;
    } else {
        perror("UTIL_getRealPath: ");
        errno = 0;
        return 1;
    }
}

int UTIL_createPath(const char* inputPath, int dirMode)
{
    char path[LIST_SIZE_INCREASE], pathDelim[2];
    char* ptr;
    char c;
    int result;

    result = UTIL_getRealPath(inputPath, path);
    if (result == -1) {
        UTIL_DISPLAYLEVEL(1, "--output-dir* commands not available on this system\n");
        exit(1);
    }
    c = '/';
    #if defined(_MSC_VER) || defined(__MINGW32__) || defined (__MSVCRT__)   /* windows support */
    c = '\\';
    #endif

    /* appending a '/' means our path construction function includes the last element, otherwise not */
    if (dirMode) {
        /* to satisfy msan */
        pathDelim[0] = c;
        pathDelim[1] = '\0';
        strcat(path, pathDelim);
    }

    /* approach is to start from the first folder in path, and iteratively try to construct a dir at each '/' until the file */
    for (ptr = strchr(path+1, c); ptr; ptr = strchr(ptr+1, c)) {
        *ptr = '\0';
        result = UTIL_createDir((const char*) path);
        if (result) {
            UTIL_DISPLAYLEVEL(1, "Unsuccessful directory creation at %s\n", path);
        }
        *ptr = c;  
    }
    return result;
}

int UTIL_createDirMirrored(char** dstFilenameTable, unsigned nbFiles) {
    int result;
    unsigned u;

    for (u = 0; u < nbFiles; ++u) {
        if (dstFilenameTable[u] != NULL) {
            result = UTIL_createPath(dstFilenameTable[u], 0);
            if (result) {
                UTIL_DISPLAYLEVEL(8, "Directory creation was unsuccessful\n");
            }
        }
    }
    return 0;
}

/* aux function for use with qsort() */
int UTIL_compareStr(const void *p1, const void *p2) {
    return strcmp(* (char * const *) p1, * (char * const *) p2);
}

int UTIL_checkFilenameCollisions(char** dstFilenameTable, unsigned nbFiles) {
    char** dstFilenameTableSorted;
    char* prevElem;
    unsigned u;

    dstFilenameTableSorted = (char**) malloc(sizeof(char*) * nbFiles);
    if (!dstFilenameTableSorted) {
        UTIL_DISPLAYLEVEL(1, "Unable to malloc new str array, not checking for name collisions\n");
        return 1;
    }
    /* approach: sort the table and check for consecutive strings that match */
    for (u = 0; u < nbFiles; ++u) {
        dstFilenameTableSorted[u] = dstFilenameTable[u];
    }
    qsort(dstFilenameTableSorted, nbFiles, sizeof(char*), UTIL_compareStr);
    prevElem = dstFilenameTableSorted[0];
    for (u = 1; u < nbFiles; ++u) {
        if (strcmp(prevElem, dstFilenameTableSorted[u]) == 0) {
            UTIL_DISPLAYLEVEL(1, "WARNING: Two files have same target path + filename : %s\n", prevElem);
        }
        prevElem = dstFilenameTableSorted[u];
    }

    free(dstFilenameTableSorted);
    return 0;
}

void UTIL_createDestinationDirTable(const char** filenameTable, unsigned nbFiles,
                                   const char* outDirName, char** dstFilenameTable)
{
    unsigned u;
    char c;
    int err;
    char outDirNameAbsolute[LIST_SIZE_INCREASE];

    c = '/';
    #if defined(_MSC_VER) || defined(__MINGW32__) || defined (__MSVCRT__) /* windows support */
    c = '\\';
    #endif
    err = UTIL_getRealPath(outDirName, outDirNameAbsolute); /* abs path is stored in outDirNameAbsolute, sans the final "/" */
    if (err == 1) {
        UTIL_DISPLAYLEVEL(8, "Was unable to get absolute directory path of target output directory\n");
    } else if (err == -1) {
        UTIL_DISPLAYLEVEL(1, "--output-dir* commands not available on this system\n");
        exit(1);
    }

    /* generate the appropriate destination filename table: absolute output dir + filename */
    for (u = 0; u < nbFiles; ++u) {
        const char* filename;
        size_t finalPathLen, outDirNameAbsoluteLen;
        outDirNameAbsoluteLen = strlen(outDirNameAbsolute);
        filename = strrchr(filenameTable[u], c);
        if (filename == NULL) {
            filename = filenameTable[u];  /* no 'c' found, use the basic name given */
        } else {
            filename += 1;  /* strrchr includes the first occurrence of 'c', which we need to get rid of */
        }
        finalPathLen = outDirNameAbsoluteLen + strlen(filename);
        dstFilenameTable[u] = (char*) malloc((finalPathLen+6) * sizeof(char)); /* 
                                                                                * extra 1 bit for \0,
                                                                                * extra 1 bit for directory delim,
                                                                                * extra 4 for .zst if compressing */
        if (!dstFilenameTable) {
            UTIL_DISPLAYLEVEL(1, "Unable to allocate space for file destination\n"); /* NULL entries are fine */
            continue;
        }
        strcpy(dstFilenameTable[u], outDirNameAbsolute);
        dstFilenameTable[u][outDirNameAbsoluteLen] = c;
        dstFilenameTable[u][outDirNameAbsoluteLen+1] = '\0';
        strcat(dstFilenameTable[u], filename);
        UTIL_DISPLAYLEVEL(8, "Final output file path: %s", dstFilenameTable[u]);
    }

    /* check for name collisions and warn if they exist*/
    if (UTIL_checkFilenameCollisions(dstFilenameTable, nbFiles))
        UTIL_DISPLAYLEVEL(1, "Checking name collisions failed\n");
}

void UTIL_createDestinationDirTableMirrored(const char** filenameTable, unsigned nbFiles,
                                   const char* outDirName, char** dstFilenameTable)
{
    unsigned u;
    size_t cwdLength;
    const char* filePath;
    int err;
    char outDirNameAbsolute[LIST_SIZE_INCREASE],
         cwd[LIST_SIZE_INCREASE],   /* same limit used by filenameTable */
         c;

    c = '/';
    #if defined(_MSC_VER) || defined(__MINGW32__) || defined (__MSVCRT__) /* windows support */
    c = '\\';
    #endif
    /* store current dir in cwd via _getcwd or getcwd */ 
    if (UTIL_getCwd(cwd) == NULL) {
        UTIL_DISPLAYLEVEL(1, "Unable to fetch current directory\n");
    }
    cwdLength = strlen(cwd);
    err = UTIL_getRealPath(outDirName, outDirNameAbsolute); /* abs path is stored in outDirNameAbsolute, sans the final "/" */
    if (err == 1) {
        UTIL_DISPLAYLEVEL(8, "Was unable to get absolute directory path of target output directory\n");
    } else if (err == -1) {
        UTIL_DISPLAYLEVEL(1, "--output-dir* commands not available on this system\n");
        exit(1);
    }
    
    /* generate destination files table: output dir (absolute) + (srcFilename absolute - cwd) */
    for (u = 0; u < nbFiles; ++u) {
        char srcFileNameAbsolute[LIST_SIZE_INCREASE];
        err = UTIL_getRealPath(filenameTable[u], srcFileNameAbsolute); /* abs path is stored in outDirNameAbsolute, sans the final "/" */
        if (err == 1) {
            UTIL_DISPLAYLEVEL(8, "Was unable to get absolute path of source file %s\n", filenameTable[u]);
        }

        /* require cwd to be prefix of filenametable[u] */
        if (strncmp(cwd, srcFileNameAbsolute, cwdLength) == 0) {
            size_t finalPathLen, outDirNameAbsoluteLen;
            outDirNameAbsoluteLen = strlen(outDirNameAbsolute);
            filePath = srcFileNameAbsolute + cwdLength + 1;
            finalPathLen = outDirNameAbsoluteLen + strlen(filePath);
            dstFilenameTable[u] = (char*) malloc((finalPathLen+6) * sizeof(char));
            if (!dstFilenameTable) {
                UTIL_DISPLAYLEVEL(1, "Unable to allocate space for file destination\n");
                continue;
            }
            strcpy(dstFilenameTable[u], outDirNameAbsolute);    /* final path is: output dir (absolute) + '/' + relative path to file from cwd */
            dstFilenameTable[u][outDirNameAbsoluteLen] = c;
            dstFilenameTable[u][outDirNameAbsoluteLen+1] = '\0';
            strcat(dstFilenameTable[u], filePath);
            UTIL_DISPLAYLEVEL(8, "Final output file path: %s\n", dstFilenameTable[u]);
        } else {
            dstFilenameTable[u] = NULL;  /* this file exists in directory upstream of cwd, we do NOT process */
        }
    } 
}

void UTIL_processMultipleFilenameDestinationDir(char** dstFilenameTable, unsigned mirrored,
                                                const char** filenameTable, unsigned nbFiles,
                                                const char* outDirName) {
    
    int dirResult;

    if (mirrored) {
        UTIL_createDestinationDirTableMirrored(filenameTable, nbFiles, outDirName, dstFilenameTable);
        dirResult = UTIL_createDirMirrored(dstFilenameTable, nbFiles);
    } else {
        UTIL_createDestinationDirTable(filenameTable, nbFiles, outDirName, dstFilenameTable);
        dirResult = UTIL_createPath(outDirName, 1);
    }

    if (dirResult)
        UTIL_DISPLAYLEVEL(1, "Target directory creation unsuccessful\n");
}

void UTIL_freeDestinationFilenameTable(char** dstDirTable, unsigned nbFiles) {
    unsigned u;

    for (u = 0; u < nbFiles; ++u) {
        if (dstDirTable[u] != NULL)
            free(dstDirTable[u]);
    }
    if (dstDirTable != NULL) free((void*)dstDirTable);
}

int UTIL_isSameFile(const char* file1, const char* file2)
{
#if defined(_MSC_VER)
    /* note : Visual does not support file identification by inode.
     *        The following work-around is limited to detecting exact name repetition only,
     *        aka `filename` is considered different from `subdir/../filename` */
    return !strcmp(file1, file2);
#else
    stat_t file1Stat;
    stat_t file2Stat;
    return UTIL_getFileStat(file1, &file1Stat)
        && UTIL_getFileStat(file2, &file2Stat)
        && (file1Stat.st_dev == file2Stat.st_dev)
        && (file1Stat.st_ino == file2Stat.st_ino);
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
    int dirLength, fnameLength, pathLength, nbFiles = 0;

    if (!(dir = opendir(dirName))) {
        UTIL_DISPLAYLEVEL(1, "Cannot open directory '%s': %s\n", dirName, strerror(errno));
        return 0;
    }

    dirLength = (int)strlen(dirName);
    errno = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp (entry->d_name, "..") == 0 ||
            strcmp (entry->d_name, ".") == 0) continue;
        fnameLength = (int)strlen(entry->d_name);
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
                *bufStart = (char*)UTIL_realloc(*bufStart, newListSize);
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
    const char** fileTable;

    if (!buf) return NULL;

    for (i=0, pos=0, nbFiles=0; i<inputNamesNb; i++) {
        if (!UTIL_isDirectory(inputNames[i])) {
            size_t const len = strlen(inputNames[i]);
            if (buf + pos + len >= bufend) {
                ptrdiff_t newListSize = (bufend - buf) + LIST_SIZE_INCREASE;
                buf = (char*)UTIL_realloc(buf, newListSize);
                bufend = buf + newListSize;
                if (!buf) return NULL;
            }
            if (buf + pos + len < bufend) {
                memcpy(buf+pos, inputNames[i], len+1);  /* with final \0 */
                pos += len + 1;
                nbFiles++;
            }
        } else {
            nbFiles += UTIL_prepareFileList(inputNames[i], &buf, &pos, &bufend, followLinks);
            if (buf == NULL) return NULL;
    }   }

    if (nbFiles == 0) { free(buf); return NULL; }

    fileTable = (const char**)malloc((nbFiles+1) * sizeof(const char*));
    if (!fileTable) { free(buf); return NULL; }

    for (i=0, pos=0; i<nbFiles; i++) {
        fileTable[i] = buf + pos;
        pos += strlen(fileTable[i]) + 1;
    }

    if (buf + pos > bufend) { free(buf); free((void*)fileTable); return NULL; }

    *allocatedBuffer = buf;
    *allocatedNamesNb = nbFiles;

    return fileTable;
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

        glpi = (LPFN_GLPI)GetProcAddress(GetModuleHandle(TEXT("kernel32")),
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
