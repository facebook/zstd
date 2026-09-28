/*
 * mtDecompressTest.c — test driver for ZSTD_decompressMultiThreaded()
 *
 * Builds multi-frame buffers (with and without stored content sizes,
 * mixed with skippable frames) and checks that multi-threaded
 * decompression is byte-identical to serial ZSTD_decompress(),
 * plus a battery of edge cases.
 *
 * Returns 0 on success, 1 on any failure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZSTD_STATIC_LINKING_ONLY
#include "../lib/zstd.h"

static int failures = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

/* simple deterministic PRNG (xorshift64) */
static unsigned long long rng_state = 0x123456789abcdefULL;
static unsigned char rng_next(void) {
    unsigned long long x = rng_state;
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    rng_state = x;
    return (unsigned char)(x >> 33);
}

/* compressible-ish data: mostly repeating patterns with noise */
static void gen_data(unsigned char* buf, size_t size, int seed) {
    size_t i;
    rng_state = 0x9e3779b97f4a7c15ULL + (unsigned long long)seed;
    for (i = 0; i < size; i++) {
        unsigned char r = rng_next();
        buf[i] = (r < 200) ? (unsigned char)(i & 0xFF) : r;  /* ~78% pattern */
    }
}

/* one compressed frame; contentSizeFlag 1 = store content size, 0 = streaming-like */
static size_t make_frame(unsigned char* dst, size_t dstCap,
                         const unsigned char* src, size_t srcSize,
                         int contentSizeFlag, int level) {
    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    size_t r;
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_contentSizeFlag, contentSizeFlag);
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, level);
    r = ZSTD_compress2(cctx, dst, dstCap, src, srcSize);
    ZSTD_freeCCtx(cctx);
    return r;
}

static size_t make_skippable(unsigned char* dst, size_t dstCap,
                             const unsigned char* payload, size_t payloadSize) {
    return ZSTD_writeSkippableFrame(dst, dstCap, payload, payloadSize, 7);
}

int main(void) {
    /* ---- build an 8-frame buffer: varied sizes 1KB..1MB, 2 skippable mixed in ---- */
    static const size_t frameSizes[8] = {
        1024, 65536, 300000, 1048576, 4096, 262144, 77777, 1048576
    };
    unsigned char* originals[8];
    unsigned char* frames[8];
    size_t frameCSizes[8];
    int isSkippable[8] = { 0, 0, 1, 0, 0, 1, 0, 0 };
    unsigned char* multi = NULL;
    size_t multiCap = 0, multiSize = 0;
    unsigned char* serialOut = NULL;
    size_t totalOrig = 0;
    int i, t;
    static const int threadCounts[] = { 1, 2, 4, 8, 16 };
    unsigned char skipPayload[64];

    printf("ZSTD version %s — mtDecompressTest\n", ZSTD_versionString());

    for (i = 0; i < 64; i++) skipPayload[i] = (unsigned char)i;
    for (i = 0; i < 8; i++) {
        size_t bound;
        originals[i] = (unsigned char*)malloc(frameSizes[i]);
        gen_data(originals[i], frameSizes[i], i);
        bound = ZSTD_compressBound(frameSizes[i]) + 64;
        frames[i] = (unsigned char*)malloc(bound);
        if (isSkippable[i]) {
            frameCSizes[i] = make_skippable(frames[i], bound, skipPayload, sizeof(skipPayload));
        } else {
            frameCSizes[i] = make_frame(frames[i], bound, originals[i], frameSizes[i], 1, 3);
        }
        CHECK(!ZSTD_isError(frameCSizes[i]), "frame %d compress failed: %s", i, ZSTD_getErrorName(frameCSizes[i]));
        multiCap += frameCSizes[i];
        totalOrig += isSkippable[i] ? 0 : frameSizes[i];
    }
    multi = (unsigned char*)malloc(multiCap);
    for (i = 0; i < 8; i++) {
        memcpy(multi + multiSize, frames[i], frameCSizes[i]);
        multiSize += frameCSizes[i];
    }
    serialOut = (unsigned char*)malloc(totalOrig ? totalOrig : 1);

    /* serial reference */
    {
        size_t r = ZSTD_decompress(serialOut, totalOrig, multi, multiSize);
        CHECK(!ZSTD_isError(r), "serial reference failed: %s", ZSTD_getErrorName(r));
        CHECK(r == totalOrig, "serial size %u != %u", (unsigned)r, (unsigned)totalOrig);
    }
    /* concatenated originals reference (skip skippable frames) */
    {
        unsigned char* cat = (unsigned char*)malloc(totalOrig ? totalOrig : 1);
        size_t pos = 0;
        for (i = 0; i < 8; i++) {
            if (!isSkippable[i]) { memcpy(cat + pos, originals[i], frameSizes[i]); pos += frameSizes[i]; }
        }
        CHECK(memcmp(serialOut, cat, totalOrig) == 0, "serial != concatenated originals");
        free(cat);
    }

    /* ---- main test: thread counts 1,2,4,8,16 ---- */
    for (t = 0; t < 5; t++) {
        int nbT = threadCounts[t];
        unsigned char* out = (unsigned char*)malloc(totalOrig ? totalOrig : 1);
        size_t r = ZSTD_decompressMultiThreaded(out, totalOrig, multi, multiSize, nbT);
        CHECK(!ZSTD_isError(r), "nbThreads=%d failed: %s", nbT, ZSTD_getErrorName(r));
        CHECK(r == totalOrig, "nbThreads=%d size %u != %u", nbT, (unsigned)r, (unsigned)totalOrig);
        CHECK(memcmp(out, serialOut, totalOrig) == 0, "nbThreads=%d output != serial", nbT);
        free(out);
        printf("  threads=%2d  OK (%u bytes)\n", nbT, (unsigned)totalOrig);
    }

    /* ---- unknown content-size path (contentSizeFlag=0): temp-buffer fallback ---- */
    {
        unsigned char* multi2 = (unsigned char*)malloc(multiCap);
        size_t multi2Size = 0, total2 = 0;
        for (i = 0; i < 8; i++) {
            size_t bound = ZSTD_compressBound(frameSizes[i]) + 64;
            unsigned char* f = (unsigned char*)malloc(bound);
            size_t cs;
            if (isSkippable[i]) {
                cs = make_skippable(f, bound, skipPayload, sizeof(skipPayload));
            } else {
                cs = make_frame(f, bound, originals[i], frameSizes[i], 0, 3);
                /* confirm the flag took effect */
                CHECK(ZSTD_getFrameContentSize(f, cs) == ZSTD_CONTENTSIZE_UNKNOWN,
                      "frame %d unexpectedly stores content size", i);
                total2 += frameSizes[i];
            }
            CHECK(!ZSTD_isError(cs), "frame %d (noSize) compress failed", i);
            memcpy(multi2 + multi2Size, f, cs);
            multi2Size += cs;
            free(f);
        }
        for (t = 0; t < 5; t++) {
            int nbT = threadCounts[t];
            unsigned char* out = (unsigned char*)malloc(total2 ? total2 : 1);
            size_t r = ZSTD_decompressMultiThreaded(out, total2, multi2, multi2Size, nbT);
            CHECK(!ZSTD_isError(r), "noSize nbThreads=%d failed: %s", nbT, ZSTD_getErrorName(r));
            CHECK(r == total2, "noSize nbThreads=%d size %u != %u", nbT, (unsigned)r, (unsigned)total2);
            CHECK(memcmp(out, serialOut, total2) == 0, "noSize nbThreads=%d output != serial", nbT);
            free(out);
        }
        printf("  unknown-content-size path  OK (threads 1,2,4,8,16)\n");
        free(multi2);
    }

    /* ---- edge cases ---- */
    {
        unsigned char* out = (unsigned char*)malloc(totalOrig ? totalOrig : 1);
        unsigned char* bad = (unsigned char*)malloc(multiSize);
        size_t r;

        /* empty input -> error */
        r = ZSTD_decompressMultiThreaded(out, totalOrig, multi, 0, 4);
        CHECK(ZSTD_isError(r), "empty input should error");
        CHECK(ZSTD_getErrorCode(r) == ZSTD_error_srcSize_wrong,
              "empty input: expected srcSize_wrong, got %s", ZSTD_getErrorName(r));

        /* truncated final frame -> error */
        memcpy(bad, multi, multiSize);
        r = ZSTD_decompressMultiThreaded(out, totalOrig, bad, multiSize - 10, 4);
        CHECK(ZSTD_isError(r), "truncated input should error (got %u)", (unsigned)r);
        CHECK(ZSTD_getErrorCode(r) == ZSTD_error_srcSize_wrong,
              "truncated input: expected srcSize_wrong, got %s", ZSTD_getErrorName(r));

        /* corrupt magic -> error */
        memcpy(bad, multi, multiSize);
        bad[0] ^= 0xFF;
        r = ZSTD_decompressMultiThreaded(out, totalOrig, bad, multiSize, 4);
        CHECK(ZSTD_isError(r), "corrupt magic should error");
        CHECK(ZSTD_getErrorCode(r) == ZSTD_error_prefix_unknown,
              "corrupt magic: expected prefix_unknown, got %s", ZSTD_getErrorName(r));

        /* dst one byte short -> dstSize_tooSmall */
        r = ZSTD_decompressMultiThreaded(out, totalOrig - 1, multi, multiSize, 4);
        CHECK(ZSTD_getErrorCode(r) == ZSTD_error_dstSize_tooSmall,
              "short dst: expected dstSize_tooSmall, got %s", ZSTD_getErrorName(r));

        /* nbThreads=0 and negative -> serial behavior */
        r = ZSTD_decompressMultiThreaded(out, totalOrig, multi, multiSize, 0);
        CHECK(!ZSTD_isError(r) && r == totalOrig && memcmp(out, serialOut, totalOrig) == 0,
              "nbThreads=0 should behave serial");
        r = ZSTD_decompressMultiThreaded(out, totalOrig, multi, multiSize, -3);
        CHECK(!ZSTD_isError(r) && r == totalOrig && memcmp(out, serialOut, totalOrig) == 0,
              "nbThreads=-3 should behave serial");

        /* nbThreads >> nframes */
        r = ZSTD_decompressMultiThreaded(out, totalOrig, multi, multiSize, 64);
        CHECK(!ZSTD_isError(r) && r == totalOrig && memcmp(out, serialOut, totalOrig) == 0,
              "nbThreads=64 should work");

        /* single frame -> serial path, identical result */
        r = ZSTD_decompressMultiThreaded(out, frameSizes[3], frames[3], frameCSizes[3], 8);
        {
            size_t rs = ZSTD_decompress(serialOut, frameSizes[3], frames[3], frameCSizes[3]);
            CHECK(!ZSTD_isError(r) && r == rs && memcmp(out, serialOut, r) == 0,
                  "single frame should equal serial");
        }

        /* skippable-only input -> 0 bytes, no error */
        {
            unsigned char sk[128];
            size_t sks = make_skippable(sk, sizeof(sk), skipPayload, sizeof(skipPayload));
            r = ZSTD_decompressMultiThreaded(out, 16, sk, sks, 4);
            CHECK(!ZSTD_isError(r) && r == 0, "skippable-only should return 0 (got %s)",
                  ZSTD_isError(r) ? ZSTD_getErrorName(r) : "ok");
        }

        /* garbage input -> error */
        for (i = 0; i < 64; i++) bad[i] = rng_next();
        r = ZSTD_decompressMultiThreaded(out, totalOrig, bad, 64, 4);
        CHECK(ZSTD_isError(r), "garbage input should error");
        CHECK(ZSTD_getErrorCode(r) == ZSTD_error_prefix_unknown,
              "garbage input: expected prefix_unknown, got %s", ZSTD_getErrorName(r));

        free(bad);
        free(out);
        printf("  edge cases  OK\n");
    }

    for (i = 0; i < 8; i++) { free(originals[i]); free(frames[i]); }
    free(multi);
    free(serialOut);

    if (failures == 0) printf("PASS: all mtDecompressTest checks passed\n");
    else printf("FAIL: %d check(s) failed\n", failures);
    return failures != 0;
}
