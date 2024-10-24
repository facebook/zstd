/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "../common/compiler.h" /* ZSTD_ALIGNOF */
#include "../common/mem.h" /* S64 */
#include "../common/zstd_deps.h" /* ZSTD_memset */
#include "../common/zstd_internal.h" /* ZSTD_STATIC_ASSERT */
#include "zstd_preSplit.h"


#define BLOCKSIZE_MIN 3500
#define THRESHOLD_PENALTY_RATE 16
#define THRESHOLD_BASE (THRESHOLD_PENALTY_RATE - 2)
#define THRESHOLD_PENALTY 3

#define HASHLENGTH 2
#define HASHLOG 10
#define HASHTABLESIZE (1 << HASHLOG)
#define HASHMASK (HASHTABLESIZE - 1)
#define KNUTH 0x9e3779b9

static unsigned hash2(const void *p)
{
    return (U32)(MEM_read16(p)) * KNUTH >> (32 - HASHLOG);
}


typedef struct {
  unsigned events[HASHTABLESIZE];
  size_t nbEvents;
} Fingerprint;
typedef struct {
    Fingerprint pastEvents;
    Fingerprint newEvents;
} FPStats;

static void initStats(FPStats* fpstats)
{
    ZSTD_memset(fpstats, 0, sizeof(FPStats));
}

FORCE_INLINE_TEMPLATE void addEvents_generic(Fingerprint* fp, const void* src, size_t srcSize, size_t samplingRate)
{
    const char* p = (const char*)src;
    size_t limit = srcSize - HASHLENGTH + 1;
    size_t n;
    assert(srcSize >= HASHLENGTH);
    for (n = 0; n < limit; n+=samplingRate) {
        fp->events[hash2(p+n)]++;
    }
    fp->nbEvents += limit/samplingRate;
}

#define ADDEVENTS_RATE(_rate) ZSTD_addEvents_##_rate

#define ZSTD_GEN_ADDEVENTS_SAMPLE(_rate)                                                \
    static void ADDEVENTS_RATE(_rate)(Fingerprint* fp, const void* src, size_t srcSize) \
    {                                                                                   \
        addEvents_generic(fp, src, srcSize, _rate);                                     \
    }

ZSTD_GEN_ADDEVENTS_SAMPLE(1)
ZSTD_GEN_ADDEVENTS_SAMPLE(5)


typedef void (*addEvents_f)(Fingerprint* fp, const void* src, size_t srcSize);

static void recordFingerprint(Fingerprint* fp, const void* src, size_t s, addEvents_f addEvents)
{
    ZSTD_memset(fp, 0, sizeof(*fp));
    addEvents(fp, src, s);
}

static U64 abs64(S64 s64) { return (U64)((s64 < 0) ? -s64 : s64); }

static U64 fpDistance(const Fingerprint* fp1, const Fingerprint* fp2)
{
    U64 distance = 0;
    size_t n;
    for (n = 0; n < HASHTABLESIZE; n++) {
        distance +=
            abs64((S64)fp1->events[n] * (S64)fp2->nbEvents - (S64)fp2->events[n] * (S64)fp1->nbEvents);
    }
    return distance;
}

/* Compare newEvents with pastEvents
 * return 1 when considered "too different"
 */
static int compareFingerprints(const Fingerprint* ref,
                            const Fingerprint* newfp,
                            int penalty)
{
    assert(ref->nbEvents > 0);
    assert(newfp->nbEvents > 0);
    {   U64 p50 = (U64)ref->nbEvents * (U64)newfp->nbEvents;
        U64 deviation = fpDistance(ref, newfp);
        U64 threshold = p50 * (U64)(THRESHOLD_BASE + penalty) / THRESHOLD_PENALTY_RATE;
        return deviation >= threshold;
    }
}

static void mergeEvents(Fingerprint* acc, const Fingerprint* newfp)
{
    size_t n;
    for (n = 0; n < HASHTABLESIZE; n++) {
        acc->events[n] += newfp->events[n];
    }
    acc->nbEvents += newfp->nbEvents;
}

static void flushEvents(FPStats* fpstats)
{
    size_t n;
    for (n = 0; n < HASHTABLESIZE; n++) {
        fpstats->pastEvents.events[n] = fpstats->newEvents.events[n];
    }
    fpstats->pastEvents.nbEvents = fpstats->newEvents.nbEvents;
    ZSTD_memset(&fpstats->newEvents, 0, sizeof(fpstats->newEvents));
}

static void removeEvents(Fingerprint* acc, const Fingerprint* slice)
{
    size_t n;
    for (n = 0; n < HASHTABLESIZE; n++) {
        assert(acc->events[n] >= slice->events[n]);
        acc->events[n] -= slice->events[n];
    }
    acc->nbEvents -= slice->nbEvents;
}

#define CHUNKSIZE (8 << 10)
/* Note: technically, we use CHUNKSIZE, so that's 8 KB */
static size_t ZSTD_splitBlock_byChunks(const void* src, size_t srcSize,
                        size_t blockSizeMax, addEvents_f f,
                        void* workspace, size_t wkspSize)
{
    FPStats* const fpstats = (FPStats*)workspace;
    const char* p = (const char*)src;
    int penalty = THRESHOLD_PENALTY;
    size_t pos = 0;
    if (srcSize <= blockSizeMax) return srcSize;
    assert(blockSizeMax == (128 << 10));
    assert(workspace != NULL);
    assert((size_t)workspace % ZSTD_ALIGNOF(FPStats) == 0);
    ZSTD_STATIC_ASSERT(ZSTD_SLIPBLOCK_WORKSPACESIZE >= sizeof(FPStats));
    assert(wkspSize >= sizeof(FPStats)); (void)wkspSize;

    initStats(fpstats);
    recordFingerprint(&fpstats->pastEvents, p, CHUNKSIZE, f);
    for (pos = CHUNKSIZE; pos <= blockSizeMax - CHUNKSIZE; pos += CHUNKSIZE) {
        recordFingerprint(&fpstats->newEvents, p + pos, CHUNKSIZE, f);
        if (compareFingerprints(&fpstats->pastEvents, &fpstats->newEvents, penalty)) {
            return pos;
        } else {
            mergeEvents(&fpstats->pastEvents, &fpstats->newEvents);
            if (penalty > 0) penalty--;
        }
    }
    assert(pos == blockSizeMax);
    return blockSizeMax;
    (void)flushEvents; (void)removeEvents;
}

size_t ZSTD_splitBlock(const void* src, size_t srcSize,
                    size_t blockSizeMax, ZSTD_SplitBlock_strategy_e splitStrat,
                    void* workspace, size_t wkspSize)
{
    if (splitStrat == split_lvl2)
        return ZSTD_splitBlock_byChunks(src, srcSize, blockSizeMax, ADDEVENTS_RATE(1), workspace, wkspSize);

    assert(splitStrat == split_lvl1);
    return ZSTD_splitBlock_byChunks(src, srcSize, blockSizeMax, ADDEVENTS_RATE(5), workspace, wkspSize);
}
