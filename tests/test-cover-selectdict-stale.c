/*
 * Reproducer for facebook/zstd issue #4750:
 * "COVER_selectDict() uses stale customDictContentEnd after
 *  ZDICT_finalizeDictionary modifies dictContentSize"
 *
 * Bug mechanism:
 *   In COVER_selectDict() (lib/dictBuilder/cover.c), `customDictContentEnd`
 *   is computed from the *input* dictContentSize (the raw content size S0).
 *   ZDICT_finalizeDictionary() then overwrites dictContentSize with the
 *   *finalized* dictionary size S1 = S0 + header (>= S0 + 8).
 *   The shrinkDict loop iterates candidate *content* sizes S = 256, 512, ...
 *   while S < largestDict (= S1, the finalized size), and passes
 *   `customDictContentEnd - S` as the candidate's source content.
 *   Whenever a tried S lands in (S0, S1), the source pointer underflows the
 *   content buffer and ZDICT_finalizeDictionary() reads out of bounds
 *   (heap-buffer-overflow under ASan).
 *
 * This test calls COVER_selectDict() directly with shrinkDict enabled and
 * sweeps the content size S0 over a range. Before the fix, at least one S0
 * makes the loop try S > S0 and ASan aborts with a heap-buffer-overflow.
 * After the fix, the loop is bounded by the content size and the sweep
 * completes cleanly.
 *
 * Build (from the repo root):
 *   make -C lib libzstd.a MOREFLAGS="-g -O1 -fsanitize=address,undefined"
 *   cc -g -O1 -fsanitize=address,undefined -Ilib tests/test-cover-selectdict-stale.c \
 *      lib/libzstd.a -o /tmp/test-cover-selectdict-stale -lpthread
 *   /tmp/test-cover-selectdict-stale
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dictBuilder/cover.h" /* COVER_selectDict */
#include "zdict.h"             /* ZDICT_cover_params_t */

#define NB_SAMPLES 8
#define SAMPLE_SIZE 1024
#define DICT_CAPACITY 8192
/* Sweep a range of raw content sizes. The shrink loop always tries S = 256
 * (ZDICT_DICTSIZE_MIN) first, while S < S0 + header. For S0 in [240, 256)
 * the tried S = 256 exceeds S0, so on unfixed code the source pointer
 * (customDictContentEnd - 256) underflows the content buffer by 1..16 bytes
 * and ASan reports a heap-buffer-overflow deterministically (the underflow
 * lands in ASan's redzone). Larger S0 values exercise the normal,
 * in-bounds shrink iterations. */
#define SWEEP_MIN 240
#define SWEEP_MAX 2000

static void fillSamples(BYTE *samples, size_t *sizes, size_t *offsets)
{
    static const char pattern[] =
        "the quick brown fox jumps over the lazy dog; "
        "pack my box with five dozen liquor jugs. ";
    size_t pLen = strlen(pattern);
    unsigned s, i;
    for (s = 0; s < NB_SAMPLES; s++) {
        offsets[s] = s * SAMPLE_SIZE;
        sizes[s] = SAMPLE_SIZE;
        for (i = 0; i < SAMPLE_SIZE; i++) {
            /* compressible with per-sample variation */
            samples[offsets[s] + i] =
                (BYTE)(pattern[(i + s * 37) % pLen] + (i % 251 == 0 ? s : 0));
        }
    }
}

static void fillContent(BYTE *content, size_t size)
{
    static const char pattern[] =
        "dictionary candidate content bytes 0123456789 abcdef; ";
    size_t pLen = strlen(pattern);
    size_t i;
    for (i = 0; i < size; i++)
        content[i] = (BYTE)pattern[i % pLen];
}

int main(void)
{
    BYTE *samples = (BYTE *)malloc(NB_SAMPLES * SAMPLE_SIZE);
    size_t sizes[NB_SAMPLES];
    size_t offsets[NB_SAMPLES];
    ZDICT_cover_params_t params;
    size_t contentSize;
    int failures = 0;

    if (!samples) { printf("malloc failed\n"); return 1; }
    fillSamples(samples, sizes, offsets);

    memset(&params, 0, sizeof(params));
    params.shrinkDict = 1;
    params.shrinkDictMaxRegression = 1;
    params.zParams.compressionLevel = 3;
    params.splitPoint = 1.0;

    for (contentSize = SWEEP_MIN; contentSize <= SWEEP_MAX; contentSize++) {
        BYTE *content = (BYTE *)malloc(contentSize);
        COVER_dictSelection_t selection;
        if (!content) { printf("malloc failed\n"); return 1; }
        fillContent(content, contentSize);
        selection = COVER_selectDict(content, DICT_CAPACITY, contentSize,
                                     samples, sizes, NB_SAMPLES,
                                     NB_SAMPLES, NB_SAMPLES,
                                     params, offsets, 0);
        if (COVER_dictSelectionIsError(selection)) {
            printf("FAIL: COVER_selectDict error at contentSize=%u\n",
                   (unsigned)contentSize);
            failures++;
        } else {
            COVER_dictSelectionFree(selection);
        }
        free(content);
        if (contentSize % 500 == 0)
            printf("... contentSize=%u ok\n", (unsigned)contentSize);
    }

    free(samples);
    if (failures) {
        printf("FAIL: %d failing content sizes\n", failures);
        return 1;
    }
    printf("PASS: COVER_selectDict shrinkDict sweep %d..%d clean\n",
           SWEEP_MIN, SWEEP_MAX);
    return 0;
}
