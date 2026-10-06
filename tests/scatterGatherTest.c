/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* scatterGatherTest.c : tests for ZSTD_decompressScatter() (issue #2345) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/zstd.h"
#include "../lib/zstd_errors.h"

#define DATA_SIZE (200000)

/* deterministic xorshift64* : compressible output (7/8 bytes from a small alphabet) */
static unsigned long long rng_state = 0x9E3779B97F4A7C15ULL;
static unsigned rng_next(void) {
    unsigned long long x = rng_state;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    rng_state = x;
    return (unsigned)(x * 0x2545F4914F6CDD1DULL >> 32);
}
static void genData(void* dst, size_t size) {
    unsigned char* p = (unsigned char*)dst;
    size_t i;
    for (i = 0; i < size; i++)
        p[i] = (rng_next() & 7) ? (unsigned char)('a' + rng_next() % 16)
                                : (unsigned char)rng_next();
}

static void* xmalloc(size_t s) {
    void* p = malloc(s ? s : 1);
    if (!p) { fprintf(stderr, "malloc(%u) failed\n", (unsigned)s); exit(1); }
    return p;
}

/* compress src[0..srcSize) with default level; returns size, stores buffer in *dstPtr */
static size_t compressBuf(const void* src, size_t srcSize, void** dstPtr) {
    size_t const cCap = ZSTD_compressBound(srcSize);
    void* dst = xmalloc(cCap);
    size_t const cSize = ZSTD_compress(dst, cCap, src, srcSize, 3);
    if (ZSTD_isError(cSize)) {
        fprintf(stderr, "ZSTD_compress error: %s\n", ZSTD_getErrorName(cSize));
        exit(1);
    }
    *dstPtr = dst;
    return cSize;
}

/* concatenate scatter output into a flat buffer for comparison */
static void flatten(ZSTD_outBuffer* dsts, size_t nbDsts, void* flat) {
    size_t i, off = 0;
    for (i = 0; i < nbDsts; i++) {
        memcpy((char*)flat + off, dsts[i].dst, dsts[i].pos);
        off += dsts[i].pos;
    }
}

static int g_dataSize = DATA_SIZE;
static unsigned char* g_data;      /* original data */
static void* g_cData;              /* compressed single frame */
static size_t g_cSize;
static void* g_ref;                /* ZSTD_decompress() reference output */

static int test_odd_sizes(void) {
    /* 3+ odd-sized buffers, byte-identical to single-buffer ZSTD_decompress */
    size_t const s0 = 1, s1 = 1000, s2 = 65537;
    size_t const s3 = (size_t)g_dataSize - s0 - s1 - s2;
    ZSTD_outBuffer dsts[4];
    unsigned char* flat;
    size_t ret;
    dsts[0].dst = xmalloc(s0); dsts[0].size = s0; dsts[0].pos = 0;
    dsts[1].dst = xmalloc(s1); dsts[1].size = s1; dsts[1].pos = 0;
    dsts[2].dst = xmalloc(s2); dsts[2].size = s2; dsts[2].pos = 0;
    dsts[3].dst = xmalloc(s3); dsts[3].size = s3; dsts[3].pos = 0;
    ret = ZSTD_decompressScatter(dsts, 4, g_cData, g_cSize);
    if (ZSTD_isError(ret) || ret != (size_t)g_dataSize) {
        printf("FAIL odd_sizes: ret=%s\n", ZSTD_isError(ret) ? ZSTD_getErrorName(ret) : "wrong size");
        return 1;
    }
    if (dsts[0].pos != s0 || dsts[1].pos != s1 || dsts[2].pos != s2 || dsts[3].pos != s3) {
        printf("FAIL odd_sizes: pos not fully advanced\n");
        return 1;
    }
    flat = xmalloc(g_dataSize);
    flatten(dsts, 4, flat);
    if (memcmp(flat, g_ref, g_dataSize) != 0) {
        printf("FAIL odd_sizes: output differs from ZSTD_decompress reference\n");
        return 1;
    }
    free(flat);
    { size_t i; for (i = 0; i < 4; i++) free(dsts[i].dst); }
    printf("PASS odd_sizes: 4 buffers (1,1000,65537,%u) byte-identical, ret=%u\n",
           (unsigned)s3, (unsigned)ret);
    return 0;
}

static int test_single_buffer(void) {
    /* nbDsts=1 must equal ZSTD_decompress() */
    ZSTD_outBuffer dst;
    size_t ret;
    dst.dst = xmalloc(g_dataSize); dst.size = g_dataSize; dst.pos = 0;
    ret = ZSTD_decompressScatter(&dst, 1, g_cData, g_cSize);
    if (ZSTD_isError(ret) || ret != (size_t)g_dataSize ||
        memcmp(dst.dst, g_ref, g_dataSize) != 0 || dst.pos != (size_t)g_dataSize) {
        printf("FAIL single_buffer\n");
        free(dst.dst);
        return 1;
    }
    free(dst.dst);
    printf("PASS single_buffer: nbDsts=1 equals ZSTD_decompress, ret=%u\n", (unsigned)ret);
    return 0;
}

static int test_zero_length_middle(void) {
    /* zero-capacity middle entry is legal and skipped */
    size_t const half = (size_t)g_dataSize / 2;
    ZSTD_outBuffer dsts[3];
    unsigned char* flat;
    size_t ret;
    dsts[0].dst = xmalloc(half); dsts[0].size = half; dsts[0].pos = 0;
    dsts[1].dst = NULL;          dsts[1].size = 0;    dsts[1].pos = 0;
    dsts[2].dst = xmalloc(g_dataSize - half);
    dsts[2].size = (size_t)g_dataSize - half; dsts[2].pos = 0;
    ret = ZSTD_decompressScatter(dsts, 3, g_cData, g_cSize);
    if (ZSTD_isError(ret) || ret != (size_t)g_dataSize || dsts[1].pos != 0) {
        printf("FAIL zero_length_middle\n");
        return 1;
    }
    flat = xmalloc(g_dataSize);
    flatten(dsts, 3, flat);
    if (memcmp(flat, g_ref, g_dataSize) != 0) {
        printf("FAIL zero_length_middle: output differs\n");
        return 1;
    }
    free(flat);
    free(dsts[0].dst); free(dsts[2].dst);
    printf("PASS zero_length_middle: NULL/0-size middle entry skipped, ret=%u\n", (unsigned)ret);
    return 0;
}

static int test_dst_too_small(void) {
    /* total capacity 1 byte short -> ZSTD_error_dstSize_tooSmall */
    size_t const half = (size_t)g_dataSize / 2;
    ZSTD_outBuffer dsts[2];
    size_t ret;
    dsts[0].dst = xmalloc(half); dsts[0].size = half; dsts[0].pos = 0;
    dsts[1].dst = xmalloc(half - 1); dsts[1].size = half - 1; dsts[1].pos = 0;
    ret = ZSTD_decompressScatter(dsts, 2, g_cData, g_cSize);
    free(dsts[0].dst); free(dsts[1].dst);
    if (!ZSTD_isError(ret) || ZSTD_getErrorCode(ret) != ZSTD_error_dstSize_tooSmall) {
        printf("FAIL dst_too_small: expected dstSize_tooSmall, got %s\n",
               ZSTD_isError(ret) ? ZSTD_getErrorName(ret) : "success");
        return 1;
    }
    printf("PASS dst_too_small: 1 byte short -> %s\n", ZSTD_getErrorName(ret));
    return 0;
}

static int test_empty_src(void) {
    /* empty src -> error */
    ZSTD_outBuffer dst;
    size_t ret;
    dst.dst = xmalloc(16); dst.size = 16; dst.pos = 0;
    ret = ZSTD_decompressScatter(&dst, 1, g_cData, 0);
    free(dst.dst);
    if (!ZSTD_isError(ret)) {
        printf("FAIL empty_src: expected error, got success\n");
        return 1;
    }
    printf("PASS empty_src: -> %s\n", ZSTD_getErrorName(ret));
    return 0;
}

static int test_multi_frame(void) {
    /* concatenated frames decompress into concatenated output */
    size_t const half = (size_t)g_dataSize / 2;
    void *c1, *c2, *cMulti;
    size_t c1Size, c2Size;
    ZSTD_outBuffer dsts[3];
    unsigned char* flat;
    size_t ret;
    c1Size = compressBuf(g_data, half, &c1);
    c2Size = compressBuf(g_data + half, g_dataSize - half, &c2);
    cMulti = xmalloc(c1Size + c2Size);
    memcpy(cMulti, c1, c1Size);
    memcpy((char*)cMulti + c1Size, c2, c2Size);
    /* odd scatter sizes across the frame boundary */
    dsts[0].dst = xmalloc(half + 7); dsts[0].size = half + 7; dsts[0].pos = 0;
    dsts[1].dst = xmalloc(999);      dsts[1].size = 999;      dsts[1].pos = 0;
    dsts[2].dst = xmalloc(g_dataSize - half - 7 - 999);
    dsts[2].size = (size_t)g_dataSize - half - 7 - 999; dsts[2].pos = 0;
    ret = ZSTD_decompressScatter(dsts, 3, cMulti, c1Size + c2Size);
    if (ZSTD_isError(ret) || ret != (size_t)g_dataSize) {
        printf("FAIL multi_frame: %s\n", ZSTD_isError(ret) ? ZSTD_getErrorName(ret) : "wrong size");
        return 1;
    }
    flat = xmalloc(g_dataSize);
    flatten(dsts, 3, flat);
    if (memcmp(flat, g_data, g_dataSize) != 0) {
        printf("FAIL multi_frame: output differs\n");
        return 1;
    }
    free(flat);
    { size_t i; for (i = 0; i < 3; i++) free(dsts[i].dst); }
    free(c1); free(c2); free(cMulti);
    printf("PASS multi_frame: 2 concatenated frames scattered across 3 buffers, ret=%u\n",
           (unsigned)ret);
    return 0;
}

static int test_truncated_src(void) {
    /* src cut 10 bytes short -> error (input ends mid-frame) */
    ZSTD_outBuffer dst;
    size_t ret;
    dst.dst = xmalloc(g_dataSize); dst.size = g_dataSize; dst.pos = 0;
    ret = ZSTD_decompressScatter(&dst, 1, g_cData, g_cSize - 10);
    free(dst.dst);
    if (!ZSTD_isError(ret)) {
        printf("FAIL truncated_src: expected error, got success\n");
        return 1;
    }
    printf("PASS truncated_src: -> %s\n", ZSTD_getErrorName(ret));
    return 0;
}

static int test_bad_params(void) {
    ZSTD_outBuffer dst;
    size_t ret;
    int fails = 0;
    /* dsts NULL with nbDsts>0 */
    dst.dst = xmalloc(16); dst.size = 16; dst.pos = 0;
    ret = ZSTD_decompressScatter(NULL, 2, g_cData, g_cSize);
    if (!ZSTD_isError(ret) || ZSTD_getErrorCode(ret) != ZSTD_error_parameter_outOfBound) {
        printf("FAIL bad_params: NULL dsts not rejected\n"); fails++;
    }
    /* pos > size */
    dst.pos = 17;
    ret = ZSTD_decompressScatter(&dst, 1, g_cData, g_cSize);
    if (!ZSTD_isError(ret) || ZSTD_getErrorCode(ret) != ZSTD_error_parameter_outOfBound) {
        printf("FAIL bad_params: pos>size not rejected\n"); fails++;
    }
    /* non-empty buffer with NULL dst */
    dst.dst = NULL; dst.size = 16; dst.pos = 0;
    ret = ZSTD_decompressScatter(&dst, 1, g_cData, g_cSize);
    if (!ZSTD_isError(ret) || ZSTD_getErrorCode(ret) != ZSTD_error_parameter_outOfBound) {
        printf("FAIL bad_params: NULL dst not rejected\n"); fails++;
    }
    free(dst.dst);
    if (fails) return 1;
    printf("PASS bad_params: NULL dsts / pos>size / NULL dst all rejected with parameter_outOfBound\n");
    return 0;
}

int main(void) {
    int fails = 0, total = 0;
    g_data = xmalloc(DATA_SIZE);
    genData(g_data, DATA_SIZE);
    g_cSize = compressBuf(g_data, DATA_SIZE, &g_cData);
    g_ref = xmalloc(DATA_SIZE);
    {   size_t const r = ZSTD_decompress(g_ref, DATA_SIZE, g_cData, g_cSize);
        if (ZSTD_isError(r) || r != DATA_SIZE) {
            fprintf(stderr, "reference ZSTD_decompress failed: %s\n",
                    ZSTD_isError(r) ? ZSTD_getErrorName(r) : "wrong size");
            return 1;
        }
    }
    printf("setup: %d bytes -> %u compressed\n", DATA_SIZE, (unsigned)g_cSize);

#define RUN(t) do { total++; fails += t(); } while (0)
    RUN(test_odd_sizes);
    RUN(test_single_buffer);
    RUN(test_zero_length_middle);
    RUN(test_dst_too_small);
    RUN(test_empty_src);
    RUN(test_multi_frame);
    RUN(test_truncated_src);
    RUN(test_bad_params);
#undef RUN

    printf("result: %d/%d tests passed\n", total - fails, total);
    free(g_data); free(g_cData); free(g_ref);
    return fails != 0;
}
