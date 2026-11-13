/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* Change one source immediately before zstd's real fopen(). */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef FILE* (*fopen_fn)(const char*, const char*);

static FILE* openSource(const char* symbol, const char* path, const char* mode)
{
    static int changed = 0;
    fopen_fn const realOpen = (fopen_fn)dlsym(RTLD_NEXT, symbol);
    const char* const target = getenv("ZSTD_TEST_RACE_TARGET");
    if (realOpen == NULL) {
        fputs("source-identity: cannot find real fopen\n", stderr);
        _exit(90);
    }
    if (!changed && target != NULL && !strcmp(path, target) && !strcmp(mode, "rb")) {
        const char* const replacement = getenv("ZSTD_TEST_RACE_REPLACEMENT");
        changed = 1;
        if (replacement != NULL ? rename(replacement, path) != 0 : chmod(path, 0600) != 0) {
            perror("source-identity");
            _exit(91);
        }
        fprintf(stderr, "source-identity: changed %s\n", path);
    }
    return realOpen(path, mode);
}

FILE* fopen(const char* path, const char* mode)
{
    return openSource("fopen", path, mode);
}

#if !defined(fopen64)
FILE* fopen64(const char* path, const char* mode)
{
    return openSource("fopen64", path, mode);
}
#endif
