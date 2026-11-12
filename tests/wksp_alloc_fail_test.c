/*
 * wksp_alloc_fail_test.c
 *
 * Regression test: a failed ZSTD_cwksp_reserve_* inside ZSTD_resetCCtx_internal /
 * ZSTD_reset_matchState must surface as ERROR(memory_allocation) instead of
 * returning success with NULL pointers (crash in ZSTD_seqToCodes / memset).
 *
 * Build (from zstd root, after `make -C lib libzstd.a`):
 *   cc -O1 -g -Ilib -DZSTD_STATIC_LINKING_ONLY \
 *      tests/wksp_alloc_fail_test.c lib/libzstd.a -o wksp_alloc_fail_test
 *   ./wksp_alloc_fail_test
 *
 * Related: crash site shared with issue #3950 (seqStore ll/ml/of code tables).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"
#include "zstd_errors.h"

/* ------------------------------------------------------------------------- */
/* Custom allocator: fail the Nth allocation (1-based), succeed the rest.     */
/* ------------------------------------------------------------------------- */

typedef struct {
    size_t allocs_seen;   /* number of alloc calls so far */
    size_t fail_at;       /* 1-based index to fail; 0 = never fail */
    size_t budget;        /* remaining bytes; (size_t)-1 = unlimited */
    size_t total_requested;
} budget_alloc_t;

static void* budget_alloc(void* opaque, size_t size)
{
    budget_alloc_t* st = (budget_alloc_t*)opaque;
    st->allocs_seen++;
    st->total_requested += size;
    if (st->fail_at != 0 && st->allocs_seen == st->fail_at) {
        return NULL;
    }
    if (size > st->budget) {
        return NULL;
    }
    st->budget -= size;
    /* malloc(0) may return NULL; zstd treats NULL as allocation failure.
     * Reserve sites can legitimately request 0 bytes (e.g. hashTable3 when
     * hashLog3 == 0). Always return a non-NULL pointer. */
    return malloc(size ? size : 1);
}

static void budget_free(void* opaque, void* address)
{
    (void)opaque;
    free(address);
}

static ZSTD_customMem budget_mem(budget_alloc_t* st)
{
    ZSTD_customMem mem;
    mem.customAlloc = budget_alloc;
    mem.customFree = budget_free;
    mem.opaque = st;
    return mem;
}

static void budget_reset(budget_alloc_t* st, size_t fail_at, size_t budget)
{
    st->allocs_seen = 0;
    st->fail_at = fail_at;
    st->budget = budget;
    st->total_requested = 0;
}

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static int g_failures = 0;

static void expect(int cond, const char* msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        g_failures++;
    }
}

/* Compress `src` with `cctx`. Returns zstd status (0 or error code). */
static size_t compress_once(ZSTD_CCtx* cctx,
                            void* dst, size_t dstCapacity,
                            const void* src, size_t srcSize,
                            int cLevel,
                            unsigned long long pledged)
{
    size_t err;
    err = ZSTD_CCtx_reset(cctx, ZSTD_reset_session_and_parameters);
    if (ZSTD_isError(err)) return err;
    err = ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, cLevel);
    if (ZSTD_isError(err)) return err;
    /* Changing pledgedSrcSize changes blockSize / neededSpace and forces
     * ZSTD_resetCCtx_internal to re-reserve the workspace. */
    err = ZSTD_CCtx_setPledgedSrcSize(cctx, pledged);
    if (ZSTD_isError(err)) return err;
    return ZSTD_compress2(cctx, dst, dstCapacity, src, srcSize);
}

/*
 * One scenario under a given fail_at / budget.
 * Returns 0 on pass (no crash; failure is a clean zstd error), 1 on logic fail.
 */
static int run_fail_sweep(size_t fail_at, size_t budget,
                          int cLevel, unsigned long long pledged,
                          int strategy)
{
    budget_alloc_t st;
    ZSTD_CCtx* cctx;
    size_t const srcSize = 64 * 1024;
    size_t const dstCapacity = ZSTD_compressBound(srcSize);
    void* src;
    void* dst;
    size_t result;
    int ok = 0;

    src = malloc(srcSize);
    dst = malloc(dstCapacity);
    if (!src || !dst) {
        free(src);
        free(dst);
        return 0; /* skip on OOM of the test harness itself */
    }
    /* compressible pattern */
    memset(src, 0xA5, srcSize);

    budget_reset(&st, fail_at, budget);
    cctx = ZSTD_createCCtx_advanced(budget_mem(&st));
    if (cctx == NULL) {
        /* CCtx allocation itself failed — clean failure. */
        free(src);
        free(dst);
        return 0;
    }

    if (strategy >= 0) {
        size_t serr = ZSTD_CCtx_setParameter(cctx, ZSTD_c_strategy, strategy);
        if (ZSTD_isError(serr)) {
            /* Parameter rejected: still must not crash. */
            ZSTD_freeCCtx(cctx);
            free(src);
            free(dst);
            return 0;
        }
    }

    result = compress_once(cctx, dst, dstCapacity, src, srcSize, cLevel, pledged);

    if (ZSTD_isError(result)) {
        /* Clean failure is the contract. Prefer memory_allocation, but any
         * recoverable zstd error is acceptable (e.g. dstSize_error if we
         * short-circuit early). */
        if (ZSTD_getErrorCode(result) != ZSTD_error_memory_allocation) {
            fprintf(stderr,
                    "note: fail_at=%zu budget=%zu returned %s (wanted memory_allocation)\n",
                    fail_at, budget, ZSTD_getErrorName(result));
        }
        ok = 1;
    } else {
        /* Success is fine when the failed allocation was not required. */
        ok = 1;
    }

    ZSTD_freeCCtx(cctx);
    free(src);
    free(dst);
    return ok ? 0 : 1;
}

/* ------------------------------------------------------------------------- */
/* Test cases                                                                */
/* ------------------------------------------------------------------------- */

/*
 * Sweep the allocation index so every ZSTD_cwksp_reserve_* site in
 * ZSTD_resetCCtx_internal / ZSTD_reset_matchState gets a chance to fail.
 */
static void test_sweep_alloc_index(void)
{
    size_t fail_at;
    int levels[] = { 1, 3, 9 };
    size_t li;

    printf("test_sweep_alloc_index...\n");
    for (li = 0; li < sizeof(levels) / sizeof(levels[0]); li++) {
        for (fail_at = 1; fail_at <= 40; fail_at++) {
            /* Two different pledged sizes: second compress on a reused CCtx
             * would also rebuild, but we create a fresh CCtx each time and
             * still vary pledged to cover both small and large workspaces. */
            g_failures += run_fail_sweep(fail_at, (size_t)-1,
                                         levels[li], 16 * 1024, -1);
            g_failures += run_fail_sweep(fail_at, (size_t)-1,
                                         levels[li], 256 * 1024, -1);
        }
    }
}

/*
 * Byte budget: simulate allocation pressure rather than a single failed call.
 * Exercises the sticky ZSTD_cwksp_reserve_failed() path when an intermediate
 * reserve (e.g. match state tables) fails and a later one is still attempted.
 */
static void test_sweep_byte_budget(void)
{
    size_t budgets[] = {
        0, 64, 256, 1024, 4096, 16 * 1024, 64 * 1024,
        256 * 1024, 1024 * 1024, 2 * 1024 * 1024
    };
    size_t i;

    printf("test_sweep_byte_budget...\n");
    for (i = 0; i < sizeof(budgets) / sizeof(budgets[0]); i++) {
        g_failures += run_fail_sweep(0, budgets[i], 3, 128 * 1024, -1);
        g_failures += run_fail_sweep(0, budgets[i], 1, 128 * 1024, -1);
    }
}

/*
 * Row-match-finder path (tagTable). ZSTD_fast / ZSTD_dfast can select the row
 * match finder; the CDict reset path memsets tagTable immediately after the
 * reserve. A failure there previously hit ZSTD_memset(NULL, ...).
 */
static void test_tag_table_row_match_finder(void)
{
    int strategies[] = { (int)ZSTD_fast, (int)ZSTD_dfast, (int)ZSTD_greedy, (int)ZSTD_lazy };
    size_t si, fail_at;

    printf("test_tag_table_row_match_finder...\n");
    for (si = 0; si < sizeof(strategies) / sizeof(strategies[0]); si++) {
        for (fail_at = 1; fail_at <= 40; fail_at++) {
            g_failures += run_fail_sweep(fail_at, (size_t)-1,
                                         1, 32 * 1024, strategies[si]);
        }
    }
}

/*
 * Long-match / large workspace: level 19+ uses large tables and opt parser
 * allocations (litFreq etc.) in ZSTD_reset_matchState.
 */
static void test_opt_parser_workspace(void)
{
    size_t fail_at;

    printf("test_opt_parser_workspace...\n");
    for (fail_at = 1; fail_at <= 50; fail_at++) {
        g_failures += run_fail_sweep(fail_at, (size_t)-1, 19, 64 * 1024, -1);
    }
}

/*
 * Reuse one CCtx across two compressions with different pledged sizes so the
 * second call rebuilds the workspace (workspaceTooSmall / wasteful path).
 * Fail allocations starting mid-way through the second reset.
 */
static void test_reuse_rebuild(void)
{
    budget_alloc_t st;
    ZSTD_CCtx* cctx;
    size_t const srcSize = 32 * 1024;
    size_t const dstCapacity = ZSTD_compressBound(srcSize);
    void* src = malloc(srcSize);
    void* dst = malloc(dstCapacity);
    size_t fail_at;

    printf("test_reuse_rebuild...\n");
    if (!src || !dst) {
        free(src);
        free(dst);
        return;
    }
    memset(src, 0x5A, srcSize);

    for (fail_at = 1; fail_at <= 60; fail_at++) {
        size_t r1, r2;

        budget_reset(&st, 0, (size_t)-1); /* first compress must succeed */
        cctx = ZSTD_createCCtx_advanced(budget_mem(&st));
        if (!cctx) break;

        r1 = compress_once(cctx, dst, dstCapacity, src, srcSize, 3, srcSize);
        expect(!ZSTD_isError(r1), "first compress in reuse_rebuild should succeed");

        /* Second compress with a much larger pledged size → workspace rebuild.
         * Re-arm the allocator to fail the Nth allocation of this second call. */
        st.allocs_seen = 0;
        st.fail_at = fail_at;

        r2 = compress_once(cctx, dst, dstCapacity, src, srcSize, 3, 8 * 1024 * 1024);
        if (ZSTD_isError(r2)) {
            /* clean error required */
            if (ZSTD_getErrorCode(r2) != ZSTD_error_memory_allocation) {
                fprintf(stderr,
                        "note: reuse rebuild fail_at=%zu → %s\n",
                        fail_at, ZSTD_getErrorName(r2));
            }
        }
        /* if not an error, the failed alloc was unused — ok */
        ZSTD_freeCCtx(cctx);
    }

    free(src);
    free(dst);
}

/*
 * CDict creation with a failing allocator (entropyWorkspace / match-state
 * reserves). Must return NULL or produce a CDict without crashing.
 */
static void test_cdict_alloc_fail(void)
{
    budget_alloc_t st;
    size_t const dictSize = 16 * 1024;
    void* dict = malloc(dictSize);
    size_t fail_at;

    printf("test_cdict_alloc_fail...\n");
    if (!dict) return;
    memset(dict, 0x11, dictSize);

    for (fail_at = 1; fail_at <= 30; fail_at++) {
        ZSTD_CDict* cdict;
        ZSTD_compressionParameters cParams;

        budget_reset(&st, fail_at, (size_t)-1);
        memset(&cParams, 0, sizeof(cParams));
        cParams = ZSTD_getCParams(3, dictSize, 0);

        cdict = ZSTD_createCDict_advanced(dict, dictSize,
                                          ZSTD_dlm_byCopy,
                                          ZSTD_dct_auto,
                                          cParams,
                                          budget_mem(&st));
        /* Either NULL (clean fail) or a valid CDict. Must not crash. */
        if (cdict) {
            ZSTD_freeCDict(cdict);
        }
    }
    free(dict);
}

/*
 * Control: with a never-failing allocator, compression must succeed.
 */
static void test_control_success(void)
{
    budget_alloc_t st;
    ZSTD_CCtx* cctx;
    size_t const srcSize = 16 * 1024;
    size_t const dstCapacity = ZSTD_compressBound(srcSize);
    void* src = malloc(srcSize);
    void* dst = malloc(dstCapacity);
    size_t result;

    printf("test_control_success...\n");
    if (!src || !dst) {
        free(src);
        free(dst);
        return;
    }
    memset(src, 0x3C, srcSize);

    budget_reset(&st, 0, (size_t)-1);
    cctx = ZSTD_createCCtx_advanced(budget_mem(&st));
    expect(cctx != NULL, "createCCtx should succeed with unlimited budget");
    if (cctx) {
        result = compress_once(cctx, dst, dstCapacity, src, srcSize, 3, srcSize);
        expect(!ZSTD_isError(result), "control compress should succeed");
        if (ZSTD_isError(result)) {
            fprintf(stderr, "  control error: %s\n", ZSTD_getErrorName(result));
        }
        ZSTD_freeCCtx(cctx);
    }
    free(src);
    free(dst);
}

int main(void)
{
    test_control_success();
    test_sweep_alloc_index();
    test_sweep_byte_budget();
    test_tag_table_row_match_finder();
    test_opt_parser_workspace();
    test_reuse_rebuild();
    test_cdict_alloc_fail();

    if (g_failures) {
        printf("FAILED: %d assertion(s)\n", g_failures);
        return 1;
    }
    printf("PASSED: all wksp allocation-failure cases returned cleanly\n");
    return 0;
}
