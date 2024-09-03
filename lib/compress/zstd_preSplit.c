/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "../common/mem.h" /* U64 */
#include "zstd_preSplit.h"


#define BLOCKSIZE_MIN 3500
#define THRESHOLD_PENALTY_RATE 16
#define THRESHOLD_BASE (THRESHOLD_PENALTY_RATE - 2)
#define THRESHOLD_PENALTY 4

#define HASHLENGTH 2
#define HASHLOG 10
#define HASHTABLESIZE (1 << HASHLOG)
#define HASHMASK (HASHTABLESIZE - 1)
#define KNUTH 0x9e3779b9

static unsigned hash2(const void *p)
{
    return (U32)(MEM_read16(p)) * KNUTH >> (32 - HASHLOG);
}


/* ==================================== */
/* Global array -> for testing only !!! */
/* ==================================== */
typedef struct {
  int events[HASHTABLESIZE];
  S64 nbEvents;
} FingerPrint;
static FingerPrint pastEvents;
static FingerPrint newEvents;

static void initStats(void) {
  memset(&pastEvents, 0, sizeof(pastEvents));
  memset(&newEvents, 0, sizeof(newEvents));
}
/* ==================================== */

static void addToFingerprint(FingerPrint* fp, const void* src, size_t s) {
  const char* p = src;
  size_t limit = s - HASHLENGTH + 1;
  size_t n;
  assert(s >= HASHLENGTH);
  for (n = 0; n < limit; n++) {
    fp->events[hash2(p++)]++;
  }
  fp->nbEvents += limit;
}

static void recordFingerprint(FingerPrint *fp, const void *src, size_t s) {
  memset(fp, 0, sizeof(*fp));
  addToFingerprint(fp, src, s);
}

static S64 abs64(S64 i) { return (i < 0) ? -i : i; }

static S64 fpDistance(const FingerPrint *fp1, const FingerPrint *fp2) {
  S64 distance = 0;
  size_t n;
  for (n = 0; n < HASHTABLESIZE; n++) {
    distance +=
        abs64(fp1->events[n] * fp2->nbEvents - fp2->events[n] * fp1->nbEvents);
  }
  return distance;
}

// Compare newEvents with pastEvents
// return 1 when considered "too different"
static int compareFingerprints(const FingerPrint* ref,
                            const FingerPrint* newfp,
                            int penalty)
{
    if (ref->nbEvents <= BLOCKSIZE_MIN)
        return 0;
    {   S64 p50 = ref->nbEvents * newfp->nbEvents;
        S64 deviation = fpDistance(ref, newfp);
        S64 threshold = p50 * (THRESHOLD_BASE + penalty) / THRESHOLD_PENALTY_RATE;
        return deviation >= threshold;
    }
}

static void mergeEvents(FingerPrint* acc, const FingerPrint* newfp)
{
    size_t n;
    for (n = 0; n < HASHTABLESIZE; n++) {
        acc->events[n] += newfp->events[n];
    }
    acc->nbEvents += newfp->nbEvents;
}

static void flushEvents(void)
{
    size_t n;
    for (n = 0; n < HASHTABLESIZE; n++) {
        pastEvents.events[n] = newEvents.events[n];
    }
    pastEvents.nbEvents = newEvents.nbEvents;
    memset(&newEvents, 0, sizeof(newEvents));
}

static void removeEvents(FingerPrint* acc, const FingerPrint* slice)
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
size_t ZSTD_splitBlock_4k(const void* src, size_t srcSize, size_t blockSizeMax)
{
    const char* p = src;
    int penalty = THRESHOLD_PENALTY;
    size_t pos = 0;
    if (srcSize <= blockSizeMax) return srcSize;
    assert(blockSizeMax == (128 << 10));

    initStats();
    for (pos = 0; pos < blockSizeMax;) {
        assert(pos <= blockSizeMax - CHUNKSIZE);
        recordFingerprint(&newEvents, p + pos, CHUNKSIZE);
        if (compareFingerprints(&pastEvents, &newEvents, penalty)) {
            return pos;
        } else {
            mergeEvents(&pastEvents, &newEvents);
            memset(&newEvents, 0, sizeof(newEvents));
            penalty = penalty - 1 + (penalty == 0);
        }
        pos += CHUNKSIZE;
    }
    return blockSizeMax;
    (void)flushEvents; (void)removeEvents;
}
