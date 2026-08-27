/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "util.h"

#if defined(__linux__)
#  include <errno.h>
#  include <fcntl.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <unistd.h>

static int isDenied(int error)
{
    return error == EACCES || error == EPERM;
}

static void makePath(char* buffer, size_t capacity,
                     const char* directory, const char* name)
{
    int const written = snprintf(buffer, capacity, "%s/%s", directory, name);
    if (written < 0 || (size_t)written >= capacity) {
        fprintf(stderr, "test path is too long\n");
        exit(1);
    }
}

static int testSandbox(const char* allowedDir, const char* blockedFile,
                       const char* exactFile, const char* allowedFile,
                       const char* blockedCreated,
                       const char* fifoPath, const char* symlinkPath,
                       const char* subdirPath)
{
    const char* writablePaths[2];
    int status;
    int fd;

    writablePaths[0] = allowedDir;
    writablePaths[1] = exactFile;
    status = UTIL_landlockRestrict(writablePaths, 2);
    if (status == 0)
        return 77;
    if (status < 0)
        return 2;

    fd = open(blockedFile, O_RDONLY);
    if (fd < 0)
        return 3;
    close(fd);

    fd = open(allowedFile, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return 4;
    close(fd);

    fd = open(exactFile, O_WRONLY | O_TRUNC);
    if (fd < 0)
        return 5;
    close(fd);

    fd = open(blockedCreated, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd >= 0) {
        close(fd);
        return 6;
    }
    if (!isDenied(errno))
        return 7;

    fd = open(blockedFile, O_WRONLY | O_TRUNC);
    if (fd >= 0) {
        close(fd);
        return 8;
    }
    if (!isDenied(errno))
        return 9;

    if (mkfifo(fifoPath, 0600) == 0)
        return 10;
    if (!isDenied(errno))
        return 11;

    if (symlink("target", symlinkPath) == 0)
        return 12;
    if (!isDenied(errno))
        return 13;

    if (mkdir(subdirPath, 0700) < 0)
        return 14;
    if (rmdir(subdirPath) == 0)
        return 15;
    if (!isDenied(errno))
        return 16;

    if (unlink(blockedFile) == 0)
        return 17;
    if (!isDenied(errno))
        return 18;
    return 0;
}
#endif

int main(void)
{
#if defined(__linux__)
    char rootDir[] = "/tmp/zstd-landlock-test-XXXXXX";
    char allowedDir[160];
    char blockedDir[160];
    char blockedFile[192];
    char exactFile[192];
    char allowedFile[192];
    char blockedCreated[192];
    char fifoPath[192];
    char symlinkPath[192];
    char subdirPath[192];
    pid_t child;
    int childStatus = 0;
    int result = 1;
    int fd;

    if (mkdtemp(rootDir) == NULL)
        return 1;
    makePath(allowedDir, sizeof(allowedDir), rootDir, "allowed");
    makePath(blockedDir, sizeof(blockedDir), rootDir, "blocked");
    makePath(blockedFile, sizeof(blockedFile), blockedDir, "existing");
    makePath(exactFile, sizeof(exactFile), blockedDir, "exact");
    makePath(allowedFile, sizeof(allowedFile), allowedDir, "created");
    makePath(blockedCreated, sizeof(blockedCreated), blockedDir, "created");
    makePath(fifoPath, sizeof(fifoPath), allowedDir, "fifo");
    makePath(symlinkPath, sizeof(symlinkPath), allowedDir, "symlink");
    makePath(subdirPath, sizeof(subdirPath), allowedDir, "subdir");

    if (mkdir(allowedDir, 0700) < 0 || mkdir(blockedDir, 0700) < 0)
        goto cleanup;
    fd = open(blockedFile, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        goto cleanup;
    close(fd);
    fd = open(exactFile, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        goto cleanup;
    close(fd);

    child = fork();
    if (child < 0)
        goto cleanup;
    if (child == 0) {
        int const childResult = testSandbox(allowedDir, blockedFile,
                                            exactFile, allowedFile, blockedCreated,
                                            fifoPath, symlinkPath, subdirPath);
        _exit(childResult);
    }
    if (waitpid(child, &childStatus, 0) < 0)
        goto cleanup;
    if (WIFEXITED(childStatus)) {
        int const exitCode = WEXITSTATUS(childStatus);
        result = exitCode == 77 ? 0 : exitCode;
    }

cleanup:
    unlink(allowedFile);
    unlink(blockedCreated);
    unlink(fifoPath);
    unlink(symlinkPath);
    unlink(blockedFile);
    unlink(exactFile);
    rmdir(subdirPath);
    rmdir(allowedDir);
    rmdir(blockedDir);
    rmdir(rootDir);
    return result;
#else
    return 0;
#endif
}
