/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* gzfinishtest : check that gzflush(Z_FINISH) does not start an empty member.
 *
 * Writes one small buffer, finishes the member with gzflush(), flushes again
 * with no data in between, then closes the file. The result must contain
 * exactly one gzip member : the file size must equal the size obtained when
 * writing the same data without the intermediate flushes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ZLIB_CONST
#define Z_PREFIX
#include "zlib.h"

static int writeMode(const char* path, int mode)
{
    gzFile file = gzopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "gzfinishtest: gzopen failed\n");
        return 1;
    }
    if (gzwrite(file, "hello", 5) != 5) {
        fprintf(stderr, "gzfinishtest: gzwrite failed\n");
        return 1;
    }
    if (mode >= 1) {
        if (gzflush(file, Z_FINISH) != Z_OK) {
            fprintf(stderr, "gzfinishtest: gzflush failed\n");
            return 1;
        }
    }
    if (mode >= 2) {
        /* second finish with no data in between : must not emit a member */
        if (gzflush(file, Z_FINISH) != Z_OK) {
            fprintf(stderr, "gzfinishtest: second gzflush failed\n");
            return 1;
        }
    }
    if (gzclose(file) != Z_OK) {
        fprintf(stderr, "gzfinishtest: gzclose failed\n");
        return 1;
    }
    return 0;
}

static long fileSize(const char* path)
{
    FILE* const f = fopen(path, "rb");
    long size;
    if (f == NULL) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    size = ftell(f);
    fclose(f);
    return size;
}

int main(int argc, char** argv)
{
    const char* name = (argc > 1) ? argv[1] : "gzfinishtest.tmp";
    long plain, flushed, twice;

    if (writeMode(name, 0)) return 1;
    plain = fileSize(name);
    if (writeMode(name, 1)) return 1;
    flushed = fileSize(name);
    if (writeMode(name, 2)) return 1;
    twice = fileSize(name);

    if (plain < 0 || flushed != plain || twice != plain) {
        fprintf(stderr, "gzfinishtest: file sizes differ : plain=%ld flushed=%ld twice=%ld "
                        "(spurious empty gzip member after Z_FINISH)\n",
                        plain, flushed, twice);
        remove(name);
        return 1;
    }

    printf("gzfinishtest OK : %ld bytes in all cases\n", plain);
    remove(name);
    return 0;
}
