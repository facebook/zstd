/*
 * Copyright (c) 2016-2020, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */
#ifndef __x86_64__
# error This test only works on x86-64
#endif

#include "./libzstd/zstd.h"

void* ZSTD_malloc(size_t size) {
    (void)size;
    return NULL;
}
void ZSTD_free(void* ptr) {
    (void)ptr;
}
void* ZSTD_calloc(size_t num, size_t size) {
    (void)num;
    (void)size;
    return NULL;
}

/* The standard requires these three functions be present.
 * The compiler is free to insert calls to these functions.
 */
void* memmove(void* destination, const void* source, size_t num) {
    char* const d = (char*)destination;
    char const* const s = (char const*)source;
    if (s < d) {
        for (size_t i = num; i-- > 0;) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = 0; i < num; ++i) {
            d[i] = s[i];
        }
    }
    return destination;
}
void* memcpy(void* destination, const void* source, size_t num) {
    return memmove(destination, source, num);
}
void* memset(void* destination, int value, size_t num) {
    char* const d = (char*)destination;
    for (size_t i = 0; i < num; ++i) {
        d[i] = value;
    }
    return destination;
}

void* ZSTD_memmove(void* destination, const void* source, size_t num) {
    return memmove(destination, source, num);
}
void* ZSTD_memcpy(void* destination, const void* source, size_t num) {
    return memmove(destination, source, num);
}
void* ZSTD_memset(void* destination, int value, size_t num) {
    return memset(destination, value, num);
}

int main(void) {
    char dst[100];
    ZSTD_CCtx* const cctx = ZSTD_createCCtx();
    ZSTD_freeCCtx(cctx);
    if (cctx != NULL) {
        return 1;
    }
    if (!ZSTD_isError(ZSTD_compress(dst, sizeof(dst), NULL, 0, 1))) {
        return 2;
    }
    return 0;
}

static inline long syscall1(long syscall, long arg1) {
    long ret;
    __asm__ volatile
    (
        "syscall"
        : "=a" (ret)
        : "a" (syscall), "D" (arg1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline void exit(int status) {
    syscall1(60, status);
}

void _start(void) {
    int const ret = main();
    exit(ret);
}
