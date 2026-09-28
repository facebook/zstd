/* ******************************************************************
 * fileApiTest - tests for the ZSTD_fopen() file convenience API
 * (lib/compress/zstd_file.c, declared in lib/zstd.h)
 *
 * Coverage:
 *  - write-then-read roundtrips, byte-identical, on text + binary data
 *    including >1MB multi-chunk transfers with mismatched chunk sizes
 *  - interop with the repo's own zstd CLI (both directions)
 *  - error paths (missing file, bad mode, wrong-direction I/O, EOF)
 *  - fclose flush correctness (non-empty file, valid magic, complete frame)
 *  - compression levels via ZSTD_fopenLevel(), incl. clamping
 *
 * Usage: ./fileApiTest [path-to-zstd-cli]
 * CLI interop tests are skipped when no CLI path is given.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 ****************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "zstd.h"

static int g_tests = 0;
static int g_failures = 0;

#define CHECK(cond) do { \
    g_tests++; \
    if (!(cond)) { \
        g_failures++; \
        printf("FAIL line %d: %s\n", __LINE__, #cond); \
    } \
} while (0)

#define DISPLAY(...) do { printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)


/* ---------- deterministic test data ---------- */

static unsigned g_randState;

static unsigned xrand(void)
{
    g_randState ^= g_randState << 13;
    g_randState ^= g_randState >> 17;
    g_randState ^= g_randState << 5;
    return g_randState;
}

/* mixed data: compressible runs + text-ish + incompressible noise */
static void genData(unsigned char* buf, size_t size, unsigned seed)
{
    static const char words[] = "the quick brown fox jumps over the lazy dog 0123456789\n";
    size_t wlen = sizeof(words) - 1;
    size_t i = 0;
    size_t k;
    g_randState = seed ? seed : 0x12345678u;
    while (i < size) {
        unsigned r = xrand() % 100;
        size_t run;
        if (r < 40) {                       /* compressible run */
            unsigned char c = (unsigned char)(xrand() % 256);
            run = 1 + xrand() % 5000;
            if (run > size - i) run = size - i;
            memset(buf + i, c, run);
        } else if (r < 70) {                /* text-ish */
            run = 1 + xrand() % 2000;
            if (run > size - i) run = size - i;
            for (k = 0; k < run; k++) buf[i + k] = (unsigned char)words[xrand() % wlen];
        } else {                            /* incompressible */
            run = 1 + xrand() % 3000;
            if (run > size - i) run = size - i;
            for (k = 0; k < run; k++) buf[i + k] = (unsigned char)(xrand() >> 8);
        }
        i += run;
    }
}

static int writeRawFile(const char* path, const void* data, size_t size)
{
    FILE* f = fopen(path, "wb");
    size_t w;
    if (f == NULL) return 0;
    w = fwrite(data, 1, size, f);
    fclose(f);
    return w == size;
}


/* ---------- roundtrip helper ---------- */

/* Write `data` with ZSTD_f* in writeChunk pieces, read back in readChunk
 * pieces, compare byte-identical. Returns 1 on full success. */
static int roundtrip(const unsigned char* data, size_t dataSize,
                     const char* fname, int level /* 0 == ZSTD_fopen */,
                     size_t writeChunk, size_t readChunk)
{
    ZSTD_FCtx* f;
    size_t pos = 0;
    unsigned char* out;
    size_t got = 0;
    int ok;

    f = (level == 0) ? ZSTD_fopen(fname, "wb") : ZSTD_fopenLevel(fname, "wb", level);
    if (f == NULL) return 0;
    while (pos < dataSize) {
        size_t chunk = writeChunk;
        if (chunk > dataSize - pos) chunk = dataSize - pos;
        if (ZSTD_fwrite(data + pos, chunk, f) != chunk) { ZSTD_fclose(f); return 0; }
        pos += chunk;
    }
    if (ZSTD_fclose(f) != 0) return 0;

    f = ZSTD_fopen(fname, "rb");
    if (f == NULL) return 0;
    out = (unsigned char*)malloc(dataSize ? dataSize : 1);
    if (out == NULL) { ZSTD_fclose(f); return 0; }
    while (got < dataSize) {
        size_t chunk = readChunk;
        size_t r;
        if (chunk > dataSize - got) chunk = dataSize - got;
        r = ZSTD_fread(out + got, chunk, f);
        if (r == 0) break;  /* EOF or error */
        got += r;
    }
    ok = (got == dataSize)
      && (ZSTD_ferror(f) == 0)
      && (dataSize == 0 || memcmp(out, data, dataSize) == 0);
    if (ok) {  /* read at EOF must return 0 with no error */
        unsigned char tmp[16];
        size_t r = ZSTD_fread(tmp, sizeof(tmp), f);
        ok = (r == 0) && (ZSTD_ferror(f) == 0);
    }
    free(out);
    if (ZSTD_fclose(f) != 0) ok = 0;
    return ok;
}


/* ---------- tests ---------- */

static void test_roundtrips(void)
{
    unsigned char* text;
    unsigned char* big;
    unsigned char* tiny;
    DISPLAY("roundtrips...");
    text = (unsigned char*)malloc(200000);
    big  = (unsigned char*)malloc(3000000);
    tiny = (unsigned char*)malloc(13);
    CHECK(text != NULL && big != NULL && tiny != NULL);
    if (text == NULL || big == NULL || tiny == NULL) return;
    genData(text, 200000, 1);
    genData(big, 3000000, 2);
    genData(tiny, 13, 3);

    /* text, small mismatched chunks */
    CHECK(roundtrip(text, 200000, "tmp_fileapi_text.zst", 0, 997, 65536));
    /* binary >1MB, multi-chunk both directions, odd sizes */
    CHECK(roundtrip(big, 3000000, "tmp_fileapi_big.zst", 3, 100000, 65537));
    /* single-byte writes and reads (staging stress) */
    CHECK(roundtrip(text, 50000, "tmp_fileapi_1b.zst", 0, 1, 1));
    /* tiny payload */
    CHECK(roundtrip(tiny, 13, "tmp_fileapi_tiny.zst", 0, 13, 13));
    /* empty payload: valid empty frame, reads back as EOF */
    CHECK(roundtrip(tiny, 0, "tmp_fileapi_empty.zst", 0, 13, 13));

    free(text); free(big); free(tiny);
}

static void test_fclose_flush(void)
{
    unsigned char* data;
    ZSTD_FCtx* f;
    FILE* raw;
    long fsize;
    unsigned char magic[4];
    unsigned char* cbuf;
    size_t csize;
    void* dbuf;
    size_t const dcap = 200000;
    size_t dr;
    DISPLAY("fclose flush...");
    data = (unsigned char*)malloc(dcap);
    CHECK(data != NULL);
    if (data == NULL) return;
    genData(data, dcap, 7);

    f = ZSTD_fopen("tmp_fileapi_flush.zst", "wb");
    CHECK(f != NULL);
    if (f == NULL) { free(data); return; }
    CHECK(ZSTD_fwrite(data, dcap, f) == dcap);
    CHECK(ZSTD_fclose(f) == 0);   /* must flush + complete the frame */

    raw = fopen("tmp_fileapi_flush.zst", "rb");
    CHECK(raw != NULL);
    if (raw == NULL) { free(data); return; }
    fseek(raw, 0, SEEK_END);
    fsize = ftell(raw);
    CHECK(fsize > 0);            /* file is non-empty after close */
    fseek(raw, 0, SEEK_SET);
    CHECK(fread(magic, 1, 4, raw) == 4);
    /* zstd frame magic 0xFD2FB528, little-endian */
    CHECK(magic[0] == 0x28 && magic[1] == 0xB5 && magic[2] == 0x2F && magic[3] == 0xFD);
    csize = (size_t)fsize;
    cbuf = (unsigned char*)malloc(csize);
    CHECK(cbuf != NULL);
    fseek(raw, 0, SEEK_SET);   /* rewind past the magic-byte peek */
    if (cbuf != NULL) {
        CHECK(fread(cbuf, 1, csize, raw) == csize);
        /* one-shot decode of the whole file: proves trailing frame complete */
        dbuf = malloc(dcap);
        CHECK(dbuf != NULL);
        if (dbuf != NULL) {
            dr = ZSTD_decompress(dbuf, dcap, cbuf, csize);
            CHECK(!ZSTD_isError(dr));
            CHECK(dr == dcap);
            CHECK(memcmp(dbuf, data, dcap) == 0);
            free(dbuf);
        }
        free(cbuf);
    }
    fclose(raw);
    free(data);
}

/* both directions against the repo's own zstd CLI */
static void test_cli_interop(const char* zstdBin)
{
    unsigned char* data;
    size_t const dataSize = 1000000;
    char cmd[1024];
    int rc;
    ZSTD_FCtx* f;
    unsigned char* out;
    size_t got;
    DISPLAY("CLI interop (%s)...", zstdBin);
    data = (unsigned char*)malloc(dataSize);
    CHECK(data != NULL);
    if (data == NULL) return;
    genData(data, dataSize, 11);
    CHECK(writeRawFile("tmp_fileapi_orig.bin", data, dataSize));

    /* API -> CLI: our file must decompress with the zstd CLI */
    CHECK(roundtrip(data, dataSize, "tmp_fileapi_tocli.zst", 0, 8192, 8192));
    rc = (int)snprintf(cmd, sizeof(cmd), "%s -d -c tmp_fileapi_tocli.zst | cmp - tmp_fileapi_orig.bin", zstdBin);
    CHECK(rc > 0 && rc < (int)sizeof(cmd));
    CHECK(system(cmd) == 0);

    /* CLI -> API: CLI-produced .zst must read via ZSTD_fopen */
    rc = (int)snprintf(cmd, sizeof(cmd), "%s -c tmp_fileapi_orig.bin > tmp_fileapi_fromcli.zst", zstdBin);
    CHECK(rc > 0 && rc < (int)sizeof(cmd));
    CHECK(system(cmd) == 0);
    f = ZSTD_fopen("tmp_fileapi_fromcli.zst", "rb");
    CHECK(f != NULL);
    out = (unsigned char*)malloc(dataSize);
    CHECK(out != NULL);
    got = 0;
    if (f != NULL && out != NULL) {
        size_t r;
        while (got < dataSize && (r = ZSTD_fread(out + got, dataSize - got, f)) != 0) got += r;
        CHECK(got == dataSize);
        CHECK(ZSTD_ferror(f) == 0);
        CHECK(memcmp(out, data, dataSize) == 0);
        CHECK(ZSTD_fclose(f) == 0);
    }
    free(out);
    free(data);
}

static void test_error_paths(void)
{
    ZSTD_FCtx* f;
    unsigned char tmp[16];
    DISPLAY("error paths...");
    /* missing file for read */
    errno = 0;
    f = ZSTD_fopen("tmp_fileapi_does_not_exist_12345.zst", "rb");
    CHECK(f == NULL);
    CHECK(errno == ENOENT);
    /* malformed modes */
    CHECK(ZSTD_fopen("tmp_fileapi_x.zst", "xx") == NULL);
    CHECK(ZSTD_fopen("tmp_fileapi_x.zst", "") == NULL);
    CHECK(ZSTD_fopen("tmp_fileapi_x.zst", "a") == NULL);
    CHECK(ZSTD_fopen("tmp_fileapi_x.zst", "rw") == NULL);
    CHECK(ZSTD_fopen("tmp_fileapi_x.zst", NULL) == NULL);
    CHECK(ZSTD_fopen(NULL, "wb") == NULL);
    CHECK(errno == EINVAL);

    /* wrong-direction I/O */
    f = ZSTD_fopen("tmp_fileapi_wrongdir.zst", "wb");
    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(ZSTD_fread(tmp, sizeof(tmp), f) == 0);   /* read on write handle */
        CHECK(ZSTD_ferror(f) != 0);
        /* fclose surfaces the sticky error, like zlib's gzclose */
        CHECK(ZSTD_fclose(f) != 0);
    }
    f = ZSTD_fopen("tmp_fileapi_wrongdir.zst", "rb");
    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(ZSTD_fwrite(tmp, sizeof(tmp), f) == 0);   /* write on read handle */
        CHECK(ZSTD_ferror(f) != 0);
        CHECK(ZSTD_fclose(f) != 0);
    }
    /* NULL handle behavior */
    CHECK(ZSTD_fclose(NULL) == 0);
    CHECK(ZSTD_ferror(NULL) != 0);
    CHECK(ZSTD_fread(tmp, sizeof(tmp), NULL) == 0);
    CHECK(ZSTD_fwrite(tmp, sizeof(tmp), NULL) == 0);
}

static void test_levels(void)
{
    unsigned char* data;
    size_t const dataSize = 300000;
    DISPLAY("levels...");
    data = (unsigned char*)malloc(dataSize);
    CHECK(data != NULL);
    if (data == NULL) return;
    genData(data, dataSize, 21);
    CHECK(roundtrip(data, dataSize, "tmp_fileapi_l1.zst", 1, 4096, 4096));
    CHECK(roundtrip(data, dataSize, "tmp_fileapi_l19.zst", 19, 4096, 4096));
    CHECK(roundtrip(data, dataSize, "tmp_fileapi_lneg.zst", -5, 4096, 4096));
    CHECK(roundtrip(data, dataSize, "tmp_fileapi_lclamp.zst", 9999, 4096, 4096));  /* clamped */
    free(data);
}

static void cleanup(void)
{
    remove("tmp_fileapi_text.zst");
    remove("tmp_fileapi_big.zst");
    remove("tmp_fileapi_1b.zst");
    remove("tmp_fileapi_tiny.zst");
    remove("tmp_fileapi_empty.zst");
    remove("tmp_fileapi_flush.zst");
    remove("tmp_fileapi_orig.bin");
    remove("tmp_fileapi_tocli.zst");
    remove("tmp_fileapi_fromcli.zst");
    remove("tmp_fileapi_wrongdir.zst");
    remove("tmp_fileapi_l1.zst");
    remove("tmp_fileapi_l19.zst");
    remove("tmp_fileapi_lneg.zst");
    remove("tmp_fileapi_lclamp.zst");
}

int main(int argc, char** argv)
{
    const char* zstdBin = (argc > 1) ? argv[1] : NULL;

    DISPLAY("fileApiTest: ZSTD_fopen() file convenience API");
    test_roundtrips();
    test_fclose_flush();
    if (zstdBin != NULL) test_cli_interop(zstdBin);
    else DISPLAY("(skipping CLI interop: no zstd binary path given)");
    test_error_paths();
    test_levels();
    cleanup();

    DISPLAY("fileApiTest: %d tests, %d failures", g_tests, g_failures);
    return g_failures != 0;
}
