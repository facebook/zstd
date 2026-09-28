/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* Unit tests for ZSTD_findFrameBoundaries() (issue #4734).
 * Builds a multi-frame buffer (standard + skippable frames), verifies that
 * the scanner reports exact, contiguous boundaries, and proves the
 * concurrency use case: every reported standard frame decompresses
 * independently to its original content. */

#define ZSTD_STATIC_LINKING_ONLY   /* for ZSTD_writeSkippableFrame() */
#include "zstd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;
#define CHECK(cond, ...)                                            \
    do {                                                            \
        if (!(cond)) {                                              \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);              \
            printf(__VA_ARGS__);                                    \
            printf("\n");                                           \
            g_failures++;                                           \
        }                                                           \
    } while (0)

/* simple xorshift64 PRNG */
static unsigned long long g_rng = 0x123456789ABCDEFULL;
static unsigned rngNext(void) {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (unsigned)(g_rng >> 11);
}

/* Fill `dst` with compressible pseudo-random data (low entropy). */
static void genCompressible(void* dst, size_t size, unsigned seed) {
    unsigned char* p = (unsigned char*)dst;
    size_t i;
    g_rng = 0x123456789ABCDEFULL + seed;
    for (i = 0; i < size; i++) {
        /* mostly small alphabet => compresses well, with occasional runs */
        unsigned r = rngNext();
        p[i] = (unsigned char)((r % 7) + ((r >> 8) % 3) * 40 + (i % 251 == 0 ? 200 : 0));
    }
}

#define NB_STD_FRAMES 5

int main(void) {
    static const size_t stdSizes[NB_STD_FRAMES] = { 1024, 65536, 13, 300000, 1 };
    static const int stdLevels[NB_STD_FRAMES] = { 1, 3, 19, 5, 1 };
    void* stdSrc[NB_STD_FRAMES];
    size_t stdFrameCap = 0;
    unsigned char* multiBuf = NULL;
    size_t multiSize = 0, multiCap = 0;
    size_t i;

    /* ---- build source blobs ---- */
    for (i = 0; i < NB_STD_FRAMES; i++) {
        stdSrc[i] = malloc(stdSizes[i] ? stdSizes[i] : 1);
        CHECK(stdSrc[i] != NULL, "malloc failed");
        genCompressible(stdSrc[i], stdSizes[i], (unsigned)(100 + i));
        stdFrameCap += ZSTD_compressBound(stdSizes[i]);
    }
    /* room for frames + 2 skippable frames + slack */
    multiCap = stdFrameCap + 2 * (8 + 4096) + 1024;
    multiBuf = (unsigned char*)malloc(multiCap);
    CHECK(multiBuf != NULL, "malloc failed");

    /* ---- frame 0,1 : standard frames ---- */
    for (i = 0; i < 2; i++) {
        size_t cSize = ZSTD_compress(multiBuf + multiSize, multiCap - multiSize,
                                     stdSrc[i], stdSizes[i], stdLevels[i]);
        CHECK(!ZSTD_isError(cSize), "compress failed: %s", ZSTD_getErrorName(cSize));
        multiSize += cSize;
    }
    /* ---- skippable frame A (magic variant 0) ---- */
    {
        const char skipContent[] = "skippable-payload-A";
        size_t wSize = ZSTD_writeSkippableFrame(multiBuf + multiSize, multiCap - multiSize,
                                                skipContent, sizeof(skipContent), 0);
        CHECK(!ZSTD_isError(wSize), "writeSkippableFrame failed: %s", ZSTD_getErrorName(wSize));
        multiSize += wSize;
    }
    /* ---- frames 2,3,4 : standard frames ---- */
    for (i = 2; i < NB_STD_FRAMES; i++) {
        size_t cSize = ZSTD_compress(multiBuf + multiSize, multiCap - multiSize,
                                     stdSrc[i], stdSizes[i], stdLevels[i]);
        CHECK(!ZSTD_isError(cSize), "compress failed: %s", ZSTD_getErrorName(cSize));
        multiSize += cSize;
    }
    /* ---- skippable frame B (magic variant 15, empty content) ---- */
    {
        size_t wSize = ZSTD_writeSkippableFrame(multiBuf + multiSize, multiCap - multiSize,
                                                NULL, 0, 15);
        CHECK(!ZSTD_isError(wSize), "writeSkippableFrame failed: %s", ZSTD_getErrorName(wSize));
        multiSize += wSize;
    }

    /* total frames: 5 standard + 2 skippable = 7 */
    printf("test buffer: %zu bytes, 7 frames (5 standard + 2 skippable)\n", multiSize);

    /* ---- counting pass (two-pass idiom) ---- */
    {
        size_t const n = ZSTD_findFrameBoundaries(multiBuf, multiSize, NULL, 0);
        CHECK(!ZSTD_isError(n), "counting pass failed: %s", ZSTD_getErrorName(n));
        CHECK(n == 7, "counting pass: expected 7 frames, got %zu", n);
    }

    /* ---- fill pass ---- */
    {
        ZSTD_FrameBoundary bounds[8];
        size_t const n = ZSTD_findFrameBoundaries(multiBuf, multiSize, bounds, 8);
        size_t pos = 0, stdIdx = 0;
        CHECK(!ZSTD_isError(n), "fill pass failed: %s", ZSTD_getErrorName(n));
        CHECK(n == 7, "fill pass: expected 7 frames, got %zu", n);
        for (i = 0; i < n; i++) {
            /* contiguity */
            CHECK(bounds[i].offset == pos,
                  "frame %zu: offset %zu, expected %zu", i, bounds[i].offset, pos);
            /* size matches the single-frame primitive */
            {
                size_t const single = ZSTD_findFrameCompressedSize(multiBuf + pos, multiSize - pos);
                CHECK(!ZSTD_isError(single), "findFrameCompressedSize failed at frame %zu", i);
                CHECK(bounds[i].compressedSize == single,
                      "frame %zu: size %zu != primitive %zu", i, bounds[i].compressedSize, single);
            }
            pos += bounds[i].compressedSize;
        }
        CHECK(pos == multiSize, "frames cover %zu of %zu bytes", pos, multiSize);

        /* skippable flags: frames 2 and 6 are the skippable ones */
        for (i = 0; i < n; i++) {
            int expectSkip = (i == 2 || i == 6);
            CHECK((bounds[i].isSkippable != 0) == expectSkip,
                  "frame %zu: isSkippable=%u, expected %d", i, bounds[i].isSkippable, expectSkip);
        }

        /* concurrency proof: each standard frame decompresses independently */
        for (i = 0; i < n; i++) {
            if (bounds[i].isSkippable) continue;
            {
                void* dec = malloc(stdSizes[stdIdx] ? stdSizes[stdIdx] : 1);
                size_t const dSize = ZSTD_decompress(dec, stdSizes[stdIdx],
                                                    multiBuf + bounds[i].offset,
                                                    bounds[i].compressedSize);
                CHECK(!ZSTD_isError(dSize), "decompress of frame %zu failed: %s",
                      i, ZSTD_getErrorName(dSize));
                CHECK(dSize == stdSizes[stdIdx],
                      "frame %zu: decompressed %zu bytes, expected %zu",
                      i, dSize, stdSizes[stdIdx]);
                CHECK(memcmp(dec, stdSrc[stdIdx], stdSizes[stdIdx]) == 0,
                      "frame %zu: roundtrip mismatch", i);
                free(dec);
                stdIdx++;
            }
        }
        CHECK(stdIdx == NB_STD_FRAMES, "decompressed %zu standard frames, expected %d",
              stdIdx, NB_STD_FRAMES);
    }

    /* ---- capacity too small ---- */
    {
        ZSTD_FrameBoundary bounds[2];
        size_t const r = ZSTD_findFrameBoundaries(multiBuf, multiSize, bounds, 2);
        CHECK(ZSTD_isError(r), "small capacity: expected error, got %zu", r);
        CHECK(ZSTD_getErrorCode(r) == ZSTD_error_dstSize_tooSmall,
              "small capacity: expected dstSize_tooSmall, got %s", ZSTD_getErrorName(r));
    }

    /* ---- empty input ---- */
    {
        size_t const r = ZSTD_findFrameBoundaries(multiBuf, 0, NULL, 0);
        CHECK(!ZSTD_isError(r) && r == 0, "empty input: expected 0, got %s/%zu",
              ZSTD_isError(r) ? ZSTD_getErrorName(r) : "ok", r);
    }

    /* ---- truncated inputs ---- */
    {
        static const size_t cutoffs[] = { 1, 3, 7, 100 };
        for (i = 0; i < sizeof(cutoffs) / sizeof(cutoffs[0]); i++) {
            size_t cut = multiSize - cutoffs[i];
            size_t const r = ZSTD_findFrameBoundaries(multiBuf, cut, NULL, 0);
            CHECK(ZSTD_isError(r), "truncated at %zu: expected error, got %zu", cut, r);
        }
        /* cut exactly at a frame boundary must succeed with fewer frames */
        {
            ZSTD_FrameBoundary b0[1];
            size_t const f0 = ZSTD_findFrameCompressedSize(multiBuf, multiSize);
            size_t const r = ZSTD_findFrameBoundaries(multiBuf, f0, b0, 1);
            CHECK(!ZSTD_isError(r) && r == 1,
                  "boundary cut: expected 1 frame, got %s/%zu",
                  ZSTD_isError(r) ? ZSTD_getErrorName(r) : "ok", r);
            CHECK(b0[0].offset == 0 && b0[0].compressedSize == f0,
                  "boundary cut: wrong boundary values");
        }
    }

    /* ---- bad magic ---- */
    {
        unsigned char* bad = (unsigned char*)malloc(multiSize);
        size_t r;
        memcpy(bad, multiBuf, multiSize);
        bad[0] ^= 0xFF; bad[1] ^= 0xFF; bad[2] ^= 0xFF; bad[3] ^= 0xFF;
        r = ZSTD_findFrameBoundaries(bad, multiSize, NULL, 0);
        CHECK(ZSTD_isError(r), "bad magic: expected error, got %zu", r);
        free(bad);
    }
    /* pure garbage */
    {
        static const unsigned char garbage[64] = {
            0xDE, 0xAD, 0xBE, 0xEF, 1, 2, 3, 4, 5, 6, 7, 8
        };
        size_t const r = ZSTD_findFrameBoundaries(garbage, sizeof(garbage), NULL, 0);
        CHECK(ZSTD_isError(r), "garbage: expected error, got %zu", r);
    }

    /* ---- single skippable frame only ---- */
    {
        unsigned char skipBuf[64];
        ZSTD_FrameBoundary b[1];
        size_t wSize = ZSTD_writeSkippableFrame(skipBuf, sizeof(skipBuf), "abc", 3, 7);
        size_t r;
        CHECK(!ZSTD_isError(wSize), "writeSkippableFrame failed");
        r = ZSTD_findFrameBoundaries(skipBuf, wSize, b, 1);
        CHECK(!ZSTD_isError(r) && r == 1, "single skippable: expected 1, got %s/%zu",
              ZSTD_isError(r) ? ZSTD_getErrorName(r) : "ok", r);
        CHECK(b[0].offset == 0 && b[0].compressedSize == wSize && b[0].isSkippable,
              "single skippable: wrong boundary values");
    }

    for (i = 0; i < NB_STD_FRAMES; i++) free(stdSrc[i]);
    free(multiBuf);

    if (g_failures == 0) printf("frameScannerTest: all tests passed\n");
    else printf("frameScannerTest: %d FAILURES\n", g_failures);
    return g_failures != 0;
}
