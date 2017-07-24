#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ldm.h"

#define LDM_HASHTABLESIZE (1 << (LDM_MEMORY_USAGE))
#define LDM_HASHTABLESIZE_U32 ((LDM_HASHTABLESIZE) >> 2)
#define LDM_HASHTABLESIZE_U64 ((LDM_HASHTABLESIZE) >> 3)

#if USE_CHECKSUM
  #define LDM_HASH_ENTRY_SIZE_LOG 3
#else
  #define LDM_HASH_ENTRY_SIZE_LOG 2
#endif

//#define HASH_ONLY_EVERY_LOG 7
#define HASH_ONLY_EVERY_LOG (LDM_WINDOW_SIZE_LOG-((LDM_MEMORY_USAGE)-(LDM_HASH_ENTRY_SIZE_LOG)))

#define HASH_ONLY_EVERY ((1 << (HASH_ONLY_EVERY_LOG)) - 1)

#define HASH_BUCKET_SIZE (1 << (HASH_BUCKET_SIZE_LOG))
#define LDM_HASHLOG ((LDM_MEMORY_USAGE)-(LDM_HASH_ENTRY_SIZE_LOG)-(HASH_BUCKET_SIZE_LOG))

#define COMPUTE_STATS
#define OUTPUT_CONFIGURATION
#define HASH_CHAR_OFFSET 10

// Take first match only.
//#define ZSTD_SKIP

//#define RUN_CHECKS

static const U64 prime8bytes = 11400714785074694791ULL;

// Type of the small hash used to index into the hash table.
typedef U32 hash_t;

#if USE_CHECKSUM
typedef struct LDM_hashEntry {
  U32 offset;
  U32 checksum;
} LDM_hashEntry;
#else
typedef struct LDM_hashEntry {
  U32 offset;
} LDM_hashEntry;
#endif

struct LDM_compressStats {
  U32 windowSizeLog, hashTableSizeLog;
  U32 numMatches;
  U64 totalMatchLength;
  U64 totalLiteralLength;
  U64 totalOffset;

  U32 matchLengthHistogram[32];

  U32 minOffset, maxOffset;
  U32 offsetHistogram[32];

  U64 TMP_hashCount[1 << HASH_ONLY_EVERY_LOG];
  U64 TMP_totalHashCount;

  U64 TMP_totalInWindow;
  U64 TMP_totalInserts;

  U64 TMP_matchCount;
};

typedef struct LDM_hashTable LDM_hashTable;

struct LDM_CCtx {
  size_t isize;             /* Input size */
  size_t maxOSize;          /* Maximum output size */

  const BYTE *ibase;        /* Base of input */
  const BYTE *ip;           /* Current input position */
  const BYTE *iend;         /* End of input */

  // Maximum input position such that hashing at the position does not exceed
  // end of input.
  const BYTE *ihashLimit;

  // Maximum input position such that finding a match of at least the minimum
  // match length does not exceed end of input.
  const BYTE *imatchLimit;

  const BYTE *obase;        /* Base of output */
  BYTE *op;                 /* Output */

  const BYTE *anchor;       /* Anchor to start of current (match) block */

  LDM_compressStats stats;            /* Compression statistics */

  LDM_hashTable *hashTable;

  const BYTE *lastPosHashed;          /* Last position hashed */
  U64 lastHash;

  const BYTE *nextIp;                 // TODO: this is redundant (ip + step)
  const BYTE *nextPosHashed;
  U64 nextHash;

  unsigned step;                      // ip step, should be 1.

  const BYTE *lagIp;
  U64 lagHash;

#ifdef RUN_CHECKS
  const BYTE *DEBUG_setNextHash;
#endif
};

struct LDM_hashTable {
  U32 numBuckets;          // The number of buckets.
  U32 numEntries;          // numBuckets * HASH_BUCKET_SIZE.

  LDM_hashEntry *entries;
  BYTE *bucketOffsets;     // A pointer (per bucket) to the next insert position.
};

/**
 * Create a hash table that can contain size elements.
 * The number of buckets is determined by size >> HASH_BUCKET_SIZE_LOG.
 */
LDM_hashTable *HASH_createTable(U32 size) {
  LDM_hashTable *table = malloc(sizeof(LDM_hashTable));
  table->numBuckets = size >> HASH_BUCKET_SIZE_LOG;
  table->numEntries = size;
  table->entries = calloc(size, sizeof(LDM_hashEntry));
  table->bucketOffsets = calloc(size >> HASH_BUCKET_SIZE_LOG, sizeof(BYTE));
  return table;
}

static LDM_hashEntry *getBucket(const LDM_hashTable *table, const hash_t hash) {
  return table->entries + (hash << HASH_BUCKET_SIZE_LOG);
}

static unsigned ZSTD_NbCommonBytes (register size_t val) {
  if (MEM_isLittleEndian()) {
    if (MEM_64bits()) {
#    if defined(_MSC_VER) && defined(_WIN64)
      unsigned long r = 0;
      _BitScanForward64( &r, (U64)val );
      return (unsigned)(r>>3);
#     elif defined(__GNUC__) && (__GNUC__ >= 3)
      return (__builtin_ctzll((U64)val) >> 3);
#     else
      static const int DeBruijnBytePos[64] = { 0, 0, 0, 0, 0, 1, 1, 2,
                                               0, 3, 1, 3, 1, 4, 2, 7,
                                               0, 2, 3, 6, 1, 5, 3, 5,
                                               1, 3, 4, 4, 2, 5, 6, 7,
                                               7, 0, 1, 2, 3, 3, 4, 6,
                                               2, 6, 5, 5, 3, 4, 5, 6,
                                               7, 1, 2, 4, 6, 4, 4, 5,
                                               7, 2, 6, 5, 7, 6, 7, 7 };
      return DeBruijnBytePos[
          ((U64)((val & -(long long)val) * 0x0218A392CDABBD3FULL)) >> 58];
#     endif
  } else { /* 32 bits */
#     if defined(_MSC_VER)
      unsigned long r=0;
      _BitScanForward( &r, (U32)val );
      return (unsigned)(r>>3);
#     elif defined(__GNUC__) && (__GNUC__ >= 3)
      return (__builtin_ctz((U32)val) >> 3);
#     else
      static const int DeBruijnBytePos[32] = { 0, 0, 3, 0, 3, 1, 3, 0,
                                               3, 2, 2, 1, 3, 2, 0, 1,
                                               3, 3, 1, 2, 2, 2, 2, 0,
                                               3, 1, 2, 0, 1, 0, 1, 1 };
      return DeBruijnBytePos[
          ((U32)((val & -(S32)val) * 0x077CB531U)) >> 27];
#     endif
    }
  } else {  /* Big Endian CPU */
    if (MEM_64bits()) {
#     if defined(_MSC_VER) && defined(_WIN64)
      unsigned long r = 0;
      _BitScanReverse64( &r, val );
      return (unsigned)(r>>3);
#     elif defined(__GNUC__) && (__GNUC__ >= 3)
      return (__builtin_clzll(val) >> 3);
#     else
      unsigned r;
      /* calculate this way due to compiler complaining in 32-bits mode */
      const unsigned n32 = sizeof(size_t)*4;
      if (!(val>>n32)) { r=4; } else { r=0; val>>=n32; }
      if (!(val>>16)) { r+=2; val>>=8; } else { val>>=24; }
      r += (!val);
      return r;
#       endif
    } else { /* 32 bits */
#     if defined(_MSC_VER)
      unsigned long r = 0;
      _BitScanReverse( &r, (unsigned long)val );
      return (unsigned)(r>>3);
#     elif defined(__GNUC__) && (__GNUC__ >= 3)
      return (__builtin_clz((U32)val) >> 3);
#     else
      unsigned r;
      if (!(val>>16)) { r=2; val>>=8; } else { r=0; val>>=24; }
      r += (!val);
      return r;
#     endif
    }
  }
}

// From lib/compress/zstd_compress.c
static size_t ZSTD_count(const BYTE *pIn, const BYTE *pMatch,
                         const BYTE *const pInLimit) {
    const BYTE * const pStart = pIn;
    const BYTE * const pInLoopLimit = pInLimit - (sizeof(size_t)-1);

    while (pIn < pInLoopLimit) {
        size_t const diff = MEM_readST(pMatch) ^ MEM_readST(pIn);
        if (!diff) {
          pIn += sizeof(size_t);
          pMatch += sizeof(size_t);
          continue;
        }
        pIn += ZSTD_NbCommonBytes(diff);
        return (size_t)(pIn - pStart);
    }

    if (MEM_64bits()) {
      if ((pIn < (pInLimit - 3)) && (MEM_read32(pMatch) == MEM_read32(pIn))) {
        pIn += 4;
        pMatch += 4;
      }
    }
    if ((pIn < (pInLimit - 1)) && (MEM_read16(pMatch) == MEM_read16(pIn))) {
      pIn += 2;
      pMatch += 2;
    }
    if ((pIn < pInLimit) && (*pMatch == *pIn)) {
      pIn++;
    }
    return (size_t)(pIn - pStart);
}

/**
 * Count number of bytes that match backwards before pIn and pMatch.
 *
 * We count only bytes where pMatch > pBaes and pIn > pAnchor.
 */
static size_t countBackwardsMatch(const BYTE *pIn, const BYTE *pAnchor,
                                  const BYTE *pMatch, const BYTE *pBase) {
  size_t matchLength = 0;
  while (pIn > pAnchor && pMatch > pBase && pIn[-1] == pMatch[-1]) {
    pIn--;
    pMatch--;
    matchLength++;
  }
  return matchLength;
}

/**
 * Returns a pointer to the entry in the hash table matching the hash and
 * checksum with the "longest match length" as defined below. The forward and
 * backward match lengths are written to *pForwardMatchLength and
 * *pBackwardMatchLength.
 *
 * The match length is defined based on cctx->ip and the entry's offset.
 * The forward match is computed from cctx->ip and entry->offset + cctx->ibase.
 * The backward match is computed backwards from cctx->ip and
 * cctx->ibase only if the forward match is longer than LDM_MIN_MATCH_LENGTH.
 *
 */
LDM_hashEntry *HASH_getBestEntry(const LDM_CCtx *cctx,
                                 const hash_t hash,
                                 const U32 checksum,
                                 U64 *pForwardMatchLength,
                                 U64 *pBackwardMatchLength) {
  LDM_hashTable *table = cctx->hashTable;
  LDM_hashEntry *bucket = getBucket(table, hash);
  LDM_hashEntry *cur = bucket;
  LDM_hashEntry *bestEntry = NULL;
  U64 bestMatchLength = 0;
#if !(USE_CHECKSUM)
  (void)checksum;
#endif
  for (; cur < bucket + HASH_BUCKET_SIZE; ++cur) {
    const BYTE *pMatch = cur->offset + cctx->ibase;

    // Check checksum for faster check.
#if USE_CHECKSUM
    if (cur->checksum == checksum &&
        cctx->ip - pMatch <= LDM_WINDOW_SIZE) {
#else
    if (cctx->ip - pMatch <= LDM_WINDOW_SIZE) {
#endif
      U64 forwardMatchLength = ZSTD_count(cctx->ip, pMatch, cctx->iend);
      U64 backwardMatchLength, totalMatchLength;

      // Only take matches where the forward match length is large enough
      // for speed.
      if (forwardMatchLength < LDM_MIN_MATCH_LENGTH) {
        continue;
      }

      backwardMatchLength =
          countBackwardsMatch(cctx->ip, cctx->anchor,
                              cur->offset + cctx->ibase,
                              cctx->ibase);

      totalMatchLength = forwardMatchLength + backwardMatchLength;

      if (totalMatchLength >= bestMatchLength) {
        bestMatchLength = totalMatchLength;
        *pForwardMatchLength = forwardMatchLength;
        *pBackwardMatchLength = backwardMatchLength;

        bestEntry = cur;
#ifdef ZSTD_SKIP
        return cur;
#endif
      }
    }
  }
  if (bestEntry != NULL) {
    return bestEntry;
  }
  return NULL;
}

#ifdef TMP_EVICTION
void HASH_insert(LDM_hashTable *table,
                 const hash_t hash, const LDM_hashEntry entry,
                 LDM_CCtx *cctx) {
  // Overwrite based on part of checksum.
  /*
  LDM_hashEntry *toOverwrite =
    getBucket(table, hash) + table->bucketOffsets[hash];
  const BYTE *pMatch = toOverwrite->offset + cctx->ibase;
  if (toOverwrite->offset != 0 &&
      cctx->ip - pMatch <= LDM_WINDOW_SIZE) {
    cctx->stats.TMP_totalInWindow++;
  }

  cctx->stats.TMP_totalInserts++;
  *(toOverwrite) = entry;
  */

  /*
  int i;
  LDM_hashEntry *bucket = getBucket(table, hash);
  for (i = 0; i < HASH_BUCKET_SIZE; i++) {
    if (bucket[i].checksum == entry.checksum) {
      bucket[i] = entry;
      cctx->stats.TMP_matchCount++;
      return;
    }
  }
  */

  // Find entry beyond window size, replace. Else, random.
  int i;
  LDM_hashEntry *bucket = getBucket(table, hash);
  for (i = 0; i < HASH_BUCKET_SIZE; i++) {
    if (cctx->ip - cctx->ibase - bucket[i].offset > LDM_WINDOW_SIZE) {
      bucket[i] = entry;
      return;
    }
  }

  i = rand() & (HASH_BUCKET_SIZE - 1);
  *(bucket + i) = entry;


  /**
   * Sliding buffer style pointer
   * Keep old entry as temporary. If the old entry is outside the window,
   * overwrite and we are done.
   *
   * Backwards (insert at x):
   * x, a, b b, c c c c, d d d d d d d d
   * x, d d d d d d d d, c c c c, b b, a
   *
   * Else, find something to evict.
   * If old entry has more ones, it takes
   * the next spot. <-- reversed order?
   *
   * If window size > LDM_WINDOW_SIZE,
   * overwrite,
   *
   * Insert forwards. If > tag, keep. Else evict.
   *
   */


  /*
  *(getBucket(table, hash) + table->bucketOffsets[hash]) = entry;
  table->bucketOffsets[hash]++;
  table->bucketOffsets[hash] &= HASH_BUCKET_SIZE - 1;
  */

//  U16 mask = entry.checksum & (HASH_BUCKET_SIZE - 1);
//  *(getBucket(table, hash) + mask) = entry;
}

#else

void HASH_insert(LDM_hashTable *table,
                 const hash_t hash, const LDM_hashEntry entry) {
  *(getBucket(table, hash) + table->bucketOffsets[hash]) = entry;
  table->bucketOffsets[hash]++;
  table->bucketOffsets[hash] &= HASH_BUCKET_SIZE - 1;
}
#endif // TMP_EVICTION


U32 HASH_getSize(const LDM_hashTable *table) {
  return table->numBuckets;
}

void HASH_destroyTable(LDM_hashTable *table) {
  free(table->entries);
  free(table->bucketOffsets);
  free(table);
}

void HASH_outputTableOccupancy(const LDM_hashTable *table) {
  U32 ctr = 0;
  LDM_hashEntry *cur = table->entries;
  LDM_hashEntry *end = table->entries + (table->numBuckets * HASH_BUCKET_SIZE);
  for (; cur < end; ++cur) {
    if (cur->offset == 0) {
      ctr++;
    }
  }

  // The number of buckets is repeated as a check for now.
  printf("Num buckets, bucket size: %d (2^%d), %d\n",
         table->numBuckets, LDM_HASHLOG, HASH_BUCKET_SIZE);
  printf("Hash table size, empty slots, %% empty: %u, %u, %.3f\n",
         table->numEntries, ctr,
         100.0 * (double)(ctr) / table->numEntries);
}

// TODO: This can be done more efficiently (but it is not that important as it
// is only used for computing stats).
static int intLog2(U64 x) {
  int ret = 0;
  while (x >>= 1) {
    ret++;
  }
  return ret;
}

void LDM_printCompressStats(const LDM_compressStats *stats) {
  int i = 0;
  printf("=====================\n");
  printf("Compression statistics\n");
  printf("Window size, hash table size (bytes): 2^%u, 2^%u\n",
          stats->windowSizeLog, stats->hashTableSizeLog);
  printf("num matches, total match length, %% matched: %u, %llu, %.3f\n",
          stats->numMatches,
          stats->totalMatchLength,
          100.0 * (double)stats->totalMatchLength /
              (double)(stats->totalMatchLength + stats->totalLiteralLength));
  printf("avg match length: %.1f\n", ((double)stats->totalMatchLength) /
                                         (double)stats->numMatches);
  printf("avg literal length, total literalLength: %.1f, %llu\n",
         ((double)stats->totalLiteralLength) / (double)stats->numMatches,
         stats->totalLiteralLength);
  printf("avg offset length: %.1f\n",
         ((double)stats->totalOffset) / (double)stats->numMatches);
  printf("min offset, max offset: %u, %u\n",
         stats->minOffset, stats->maxOffset);

  printf("\n");
  printf("offset histogram | match length histogram\n");
  printf("offset/ML, num matches, %% of matches | num matches, %% of matches\n");

  for (; i <= intLog2(stats->maxOffset); i++) {
    printf("2^%*d: %10u    %6.3f%% |2^%*d:  %10u    %6.3f \n",
           2, i,
           stats->offsetHistogram[i],
           100.0 * (double) stats->offsetHistogram[i] /
                   (double) stats->numMatches,
           2, i,
           stats->matchLengthHistogram[i],
           100.0 * (double) stats->matchLengthHistogram[i] /
                   (double) stats->numMatches);
  }
  printf("\n");
#ifdef TMP_TAG_INSERT
/*
  printf("Lower bit distribution\n");
  for (i = 0; i < (1 << HASH_ONLY_EVERY_LOG); i++) {
    printf("%5d %5llu %6.3f\n", i, stats->TMP_hashCount[i],
           100.0 * (double) stats->TMP_hashCount[i] /
                   (double) stats->TMP_totalHashCount);
  }
*/
#endif

#ifdef TMP_EVICTION
  printf("Evicted something in window: %llu %6.3f\n",
         stats->TMP_totalInWindow,
         100.0 * (double)stats->TMP_totalInWindow /
                 (double)stats->TMP_totalInserts);
  printf("Match count: %llu\n", stats->TMP_matchCount);
#endif
  printf("=====================\n");
}

/**
 * Return the upper (most significant) LDM_HASHLOG bits.
 */
static hash_t getSmallHash(U64 hash) {
  return hash >> (64 - LDM_HASHLOG);
}

/**
 * Return the 32 bits after the upper LDM_HASHLOG bits.
 */
static U32 getChecksum(U64 hash) {
  return (hash >> (64 - 32 - LDM_HASHLOG)) & 0xFFFFFFFF;
}

#ifdef TMP_TAG_INSERT
static U32 lowerBitsFromHfHash(U64 hash) {
  // The number of bits used so far is LDM_HASHLOG + 32.
  // So there are 32 - LDM_HASHLOG bits left.
  // Occasional hashing requires HASH_ONLY_EVERY_LOG bits.
  // So if 32 - LDMHASHLOG < HASH_ONLY_EVERY_LOG, just return lower bits
  // allowing for reuse of bits.
  if (32 - LDM_HASHLOG < HASH_ONLY_EVERY_LOG) {
    return hash & HASH_ONLY_EVERY;
  } else {
    // Otherwise shift by (32 - LDM_HASHLOG - HASH_ONLY_EVERY_LOG) bits first.
    return (hash >> (32 - LDM_HASHLOG - HASH_ONLY_EVERY_LOG)) &
           HASH_ONLY_EVERY;
  }
}
#endif

/**
 * Get a 64-bit hash using the first len bytes from buf.
 *
 * Giving bytes s = s_1, s_2, ... s_k, the hash is defined to be
 * H(s) = s_1*(a^(k-1)) + s_2*(a^(k-2)) + ... + s_k*(a^0)
 *
 * where the constant a is defined to be prime8bytes.
 *
 * The implementation adds an offset to each byte, so
 * H(s) = (s_1 + HASH_CHAR_OFFSET)*(a^(k-1)) + ...
 */
static U64 getHash(const BYTE *buf, U32 len) {
  U64 ret = 0;
  U32 i;
  for (i = 0; i < len; i++) {
    ret *= prime8bytes;
    ret += buf[i] + HASH_CHAR_OFFSET;
  }
  return ret;

}

static U64 ipow(U64 base, U64 exp) {
  U64 ret = 1;
  while (exp) {
    if (exp & 1) {
      ret *= base;
    }
    exp >>= 1;
    base *= base;
  }
  return ret;
}

static U64 updateHash(U64 hash, U32 len,
                      BYTE toRemove, BYTE toAdd) {
  // TODO: this relies on compiler optimization.
  // The exponential can be calculated explicitly as len is constant.
  hash -= ((toRemove + HASH_CHAR_OFFSET) *
          ipow(prime8bytes, len - 1));
  hash *= prime8bytes;
  hash += toAdd + HASH_CHAR_OFFSET;
  return hash;
}

/**
 * Update cctx->nextHash and cctx->nextPosHashed
 * based on cctx->lastHash and cctx->lastPosHashed.
 *
 * This uses a rolling hash and requires that the last position hashed
 * corresponds to cctx->nextIp - step.
 */
static void setNextHash(LDM_CCtx *cctx) {
#ifdef RUN_CHECKS
  U64 check;
  if ((cctx->nextIp - cctx->ibase != 1) &&
      (cctx->nextIp - cctx->DEBUG_setNextHash != 1)) {
    printf("CHECK debug fail: %zu %zu\n", cctx->nextIp - cctx->ibase,
            cctx->DEBUG_setNextHash - cctx->ibase);
  }

  cctx->DEBUG_setNextHash = cctx->nextIp;
#endif

  cctx->nextHash = updateHash(
      cctx->lastHash, LDM_HASH_LENGTH,
      cctx->lastPosHashed[0],
      cctx->lastPosHashed[LDM_HASH_LENGTH]);
  cctx->nextPosHashed = cctx->nextIp;

#ifdef TMP_TAG_INSERT
  {
    U32 hashEveryMask = lowerBitsFromHfHash(cctx->nextHash);
    cctx->stats.TMP_totalHashCount++;
    cctx->stats.TMP_hashCount[hashEveryMask]++;
  }
#endif

#if LDM_LAG
  if (cctx->ip - cctx->ibase > LDM_LAG) {
    cctx->lagHash = updateHash(
      cctx->lagHash, LDM_HASH_LENGTH,
      cctx->lagIp[0], cctx->lagIp[LDM_HASH_LENGTH]);
    cctx->lagIp++;
  }
#endif

#ifdef RUN_CHECKS
  check = getHash(cctx->nextIp, LDM_HASH_LENGTH);

  if (check != cctx->nextHash) {
    printf("CHECK: setNextHash failed %llu %llu\n", check, cctx->nextHash);
  }

  if ((cctx->nextIp - cctx->lastPosHashed) != 1) {
    printf("setNextHash: nextIp != lastPosHashed + 1. %zu %zu %zu\n",
            cctx->nextIp - cctx->ibase, cctx->lastPosHashed - cctx->ibase,
            cctx->ip - cctx->ibase);
  }
#endif
}

static void putHashOfCurrentPositionFromHash(LDM_CCtx *cctx, U64 hash) {
  // Hash only every HASH_ONLY_EVERY times, based on cctx->ip.
  // Note: this works only when cctx->step is 1.
#if LDM_LAG
  if (((cctx->ip - cctx->ibase) & HASH_ONLY_EVERY) == HASH_ONLY_EVERY) {
    // TODO: Off by one, but not important.
    if (cctx->lagIp - cctx->ibase > 0) {
      U32 smallHash = getSmallHash(cctx->lagHash);

#   if USE_CHECKSUM
      U32 checksum = getChecksum(cctx->lagHash);
      const LDM_hashEntry entry = { cctx->lagIp - cctx->ibase, checksum };
#   else
      const LDM_hashEntry entry = { cctx->lagIp - cctx->ibase };
#   endif

#   ifdef TMP_EVICTION
      HASH_insert(cctx->hashTable, smallHash, entry, cctx);
#   else
      HASH_insert(cctx->hashTable, smallHash, entry);
#   endif
    } else {
#   if USE_CHECKSUM
      U32 checksum = getChecksum(hash);
      const LDM_hashEntry entry = { cctx->lagIp - cctx->ibase, checksum };
#   else
      const LDM_hashEntry entry = { cctx->lagIp - cctx->ibase };
#   endif

#   ifdef TMP_EVICTION
      HASH_insert(cctx->hashTable, smallHash, entry, cctx);
#   else
      HASH_insert(cctx->hashTable, smallHash, entry);
#    endif
    }
  }
#else
#ifdef TMP_TAG_INSERT
  U32 hashEveryMask = lowerBitsFromHfHash(hash);
  if (hashEveryMask == HASH_ONLY_EVERY) {
#else
  if (((cctx->ip - cctx->ibase) & HASH_ONLY_EVERY) == HASH_ONLY_EVERY) {
#endif
    U32 smallHash = getSmallHash(hash);
#if USE_CHECKSUM
    U32 checksum = getChecksum(hash);
    const LDM_hashEntry entry = { cctx->ip - cctx->ibase, checksum };
#else
    const LDM_hashEntry entry = { cctx->ip - cctx->ibase };
#endif
#ifdef TMP_EVICTION
    HASH_insert(cctx->hashTable, smallHash, entry, cctx);
#else
    HASH_insert(cctx->hashTable, smallHash, entry);
#endif
  }
#endif

  cctx->lastPosHashed = cctx->ip;
  cctx->lastHash = hash;
}

/**
 * Copy over the cctx->lastHash, and cctx->lastPosHashed
 * fields from the "next" fields.
 *
 * This requires that cctx->ip == cctx->nextPosHashed.
 */
static void LDM_updateLastHashFromNextHash(LDM_CCtx *cctx) {
#ifdef RUN_CHECKS
  if (cctx->ip != cctx->nextPosHashed) {
    printf("CHECK failed: updateLastHashFromNextHash %zu\n",
           cctx->ip - cctx->ibase);
  }
#endif
  putHashOfCurrentPositionFromHash(cctx, cctx->nextHash);
}

/**
 * Insert hash of the current position into the hash table.
 */
static void LDM_putHashOfCurrentPosition(LDM_CCtx *cctx) {
  U64 hash = getHash(cctx->ip, LDM_HASH_LENGTH);

#ifdef RUN_CHECKS
  if (cctx->nextPosHashed != cctx->ip && (cctx->ip != cctx->ibase)) {
    printf("CHECK failed: putHashOfCurrentPosition %zu\n",
           cctx->ip - cctx->ibase);
  }
#endif

  putHashOfCurrentPositionFromHash(cctx, hash);
}

void LDM_initializeCCtx(LDM_CCtx *cctx,
                        const void *src, size_t srcSize,
                        void *dst, size_t maxDstSize) {
  cctx->isize = srcSize;
  cctx->maxOSize = maxDstSize;

  cctx->ibase = (const BYTE *)src;
  cctx->ip = cctx->ibase;
  cctx->iend = cctx->ibase + srcSize;

  cctx->ihashLimit = cctx->iend - LDM_HASH_LENGTH;
  cctx->imatchLimit = cctx->iend - LDM_MIN_MATCH_LENGTH;

  cctx->obase = (BYTE *)dst;
  cctx->op = (BYTE *)dst;

  cctx->anchor = cctx->ibase;

  memset(&(cctx->stats), 0, sizeof(cctx->stats));
#if USE_CHECKSUM
  cctx->hashTable = HASH_createTable(LDM_HASHTABLESIZE_U64);
#else
  cctx->hashTable = HASH_createTable(LDM_HASHTABLESIZE_U32);
#endif
  cctx->stats.minOffset = UINT_MAX;
  cctx->stats.windowSizeLog = LDM_WINDOW_SIZE_LOG;
  cctx->stats.hashTableSizeLog = LDM_MEMORY_USAGE;


  cctx->lastPosHashed = NULL;

  cctx->step = 1;   // Fixed to be 1 for now. Changing may break things.
  cctx->nextIp = cctx->ip + cctx->step;
  cctx->nextPosHashed = 0;

#ifdef RUN_CHECKS
  cctx->DEBUG_setNextHash = 0;
#endif
}

void LDM_destroyCCtx(LDM_CCtx *cctx) {
  HASH_destroyTable(cctx->hashTable);
}

/**
 * Finds the "best" match.
 *
 * Returns 0 if successful and 1 otherwise (i.e. no match can be found
 * in the remaining input that is long enough).
 *
 * forwardMatchLength contains the forward length of the match.
 */
static int LDM_findBestMatch(LDM_CCtx *cctx, const BYTE **match,
                             U64 *forwardMatchLength, U64 *backwardMatchLength) {

  LDM_hashEntry *entry = NULL;
  cctx->nextIp = cctx->ip + cctx->step;

  while (entry == NULL) {
    U64 hash;
    hash_t smallHash;
    U32 checksum;
#ifdef TMP_TAG_INSERT
    U32 hashEveryMask;
#endif
    setNextHash(cctx);

    hash = cctx->nextHash;
    smallHash = getSmallHash(hash);
    checksum = getChecksum(hash);
#ifdef TMP_TAG_INSERT
    hashEveryMask = lowerBitsFromHfHash(hash);
#endif

    cctx->ip = cctx->nextIp;
    cctx->nextIp += cctx->step;

    if (cctx->ip > cctx->imatchLimit) {
      return 1;
    }
#ifdef TMP_TAG_INSERT
    if (hashEveryMask == HASH_ONLY_EVERY) {

      entry = HASH_getBestEntry(cctx, smallHash, checksum,
                                forwardMatchLength, backwardMatchLength);
    }
#else
    entry = HASH_getBestEntry(cctx, smallHash, checksum,
                              forwardMatchLength, backwardMatchLength);
#endif

    if (entry != NULL) {
      *match = entry->offset + cctx->ibase;
    }

    putHashOfCurrentPositionFromHash(cctx, hash);

  }
  setNextHash(cctx);
  return 0;
}

void LDM_encodeLiteralLengthAndLiterals(
    LDM_CCtx *cctx, BYTE *pToken, const U64 literalLength) {
  /* Encode the literal length. */
  if (literalLength >= RUN_MASK) {
    U64 len = (U64)literalLength - RUN_MASK;
    *pToken = (RUN_MASK << ML_BITS);
    for (; len >= 255; len -= 255) {
      *(cctx->op)++ = 255;
    }
    *(cctx->op)++ = (BYTE)len;
  } else {
    *pToken = (BYTE)(literalLength << ML_BITS);
  }

  /* Encode the literals. */
  memcpy(cctx->op, cctx->anchor, literalLength);
  cctx->op += literalLength;
}

void LDM_outputBlock(LDM_CCtx *cctx,
                     const U64 literalLength,
                     const U32 offset,
                     const U64 matchLength) {
  BYTE *pToken = cctx->op++;

  /* Encode the literal length and literals. */
  LDM_encodeLiteralLengthAndLiterals(cctx, pToken, literalLength);

  /* Encode the offset. */
  MEM_write32(cctx->op, offset);
  cctx->op += LDM_OFFSET_SIZE;

  /* Encode the match length. */
  if (matchLength >= ML_MASK) {
    U64 matchLengthRemaining = matchLength;
    *pToken += ML_MASK;
    matchLengthRemaining -= ML_MASK;
    MEM_write32(cctx->op, 0xFFFFFFFF);
    while (matchLengthRemaining >= 4*0xFF) {
      cctx->op += 4;
      MEM_write32(cctx->op, 0xffffffff);
      matchLengthRemaining -= 4*0xFF;
    }
    cctx->op += matchLengthRemaining / 255;
    *(cctx->op)++ = (BYTE)(matchLengthRemaining % 255);
  } else {
    *pToken += (BYTE)(matchLength);
  }
}

// TODO: maxDstSize is unused. This function may seg fault when writing
// beyond the size of dst, as it does not check maxDstSize. Writing to
// a buffer and performing checks is a possible solution.
//
// This is based upon lz4.
size_t LDM_compress(const void *src, size_t srcSize,
                    void *dst, size_t maxDstSize) {
  LDM_CCtx cctx;
  const BYTE *match = NULL;
  U64 forwardMatchLength = 0;
  U64 backwardsMatchLength = 0;

  LDM_initializeCCtx(&cctx, src, srcSize, dst, maxDstSize);

#ifdef OUTPUT_CONFIGURATION
  LDM_outputConfiguration();
#endif

  /* Hash the first position and put it into the hash table. */
  LDM_putHashOfCurrentPosition(&cctx);

#if LDM_LAG
  cctx.lagIp = cctx.ip;
  cctx.lagHash = cctx.lastHash;
#endif
  /**
   * Find a match.
   * If no more matches can be found (i.e. the length of the remaining input
   * is less than the minimum match length), then stop searching for matches
   * and encode the final literals.
   */
  while (LDM_findBestMatch(&cctx, &match, &forwardMatchLength,
         &backwardsMatchLength) == 0) {

#ifdef COMPUTE_STATS
    cctx.stats.numMatches++;
#endif

     cctx.ip -= backwardsMatchLength;
     match -= backwardsMatchLength;

    /**
     * Write current block (literals, literal length, match offset, match
     * length) and update pointers and hashes.
     */
    {
      const U64 literalLength = cctx.ip - cctx.anchor;
      const U32 offset = cctx.ip - match;
      const U64 matchLength = forwardMatchLength +
                              backwardsMatchLength -
                              LDM_MIN_MATCH_LENGTH;

      LDM_outputBlock(&cctx, literalLength, offset, matchLength);

#ifdef COMPUTE_STATS
      cctx.stats.totalLiteralLength += literalLength;
      cctx.stats.totalOffset += offset;
      cctx.stats.totalMatchLength += matchLength + LDM_MIN_MATCH_LENGTH;
      cctx.stats.minOffset =
          offset < cctx.stats.minOffset ? offset : cctx.stats.minOffset;
      cctx.stats.maxOffset =
          offset > cctx.stats.maxOffset ? offset : cctx.stats.maxOffset;
      cctx.stats.offsetHistogram[(U32)intLog2(offset)]++;
      cctx.stats.matchLengthHistogram[
          (U32)intLog2(matchLength + LDM_MIN_MATCH_LENGTH)]++;
#endif

      // Move ip to end of block, inserting hashes at each position.
      cctx.nextIp = cctx.ip + cctx.step;
      while (cctx.ip < cctx.anchor + LDM_MIN_MATCH_LENGTH +
                       matchLength + literalLength) {
        if (cctx.ip > cctx.lastPosHashed) {
          // TODO: Simplify.
          LDM_updateLastHashFromNextHash(&cctx);
          setNextHash(&cctx);
        }
        cctx.ip++;
        cctx.nextIp++;
      }
    }

    // Set start of next block to current input pointer.
    cctx.anchor = cctx.ip;
    LDM_updateLastHashFromNextHash(&cctx);
  }

  /* Encode the last literals (no more matches). */
  {
    const U64 lastRun = cctx.iend - cctx.anchor;
    BYTE *pToken = cctx.op++;
    LDM_encodeLiteralLengthAndLiterals(&cctx, pToken, lastRun);
  }

#ifdef COMPUTE_STATS
  LDM_printCompressStats(&cctx.stats);
  HASH_outputTableOccupancy(cctx.hashTable);
#endif

  {
    const size_t ret = cctx.op - cctx.obase;
    LDM_destroyCCtx(&cctx);
    return ret;
  }
}

void LDM_outputConfiguration(void) {
  printf("=====================\n");
  printf("Configuration\n");
  printf("LDM_WINDOW_SIZE_LOG: %d\n", LDM_WINDOW_SIZE_LOG);
  printf("LDM_MIN_MATCH_LENGTH, LDM_HASH_LENGTH: %d, %d\n",
         LDM_MIN_MATCH_LENGTH, LDM_HASH_LENGTH);
  printf("LDM_MEMORY_USAGE: %d\n", LDM_MEMORY_USAGE);
  printf("HASH_ONLY_EVERY_LOG: %d\n", HASH_ONLY_EVERY_LOG);
  printf("HASH_BUCKET_SIZE_LOG: %d\n", HASH_BUCKET_SIZE_LOG);
  printf("LDM_LAG %d\n", LDM_LAG);
  printf("USE_CHECKSUM %d\n", USE_CHECKSUM);
  printf("=====================\n");
}



// TODO: implement and test hash function
void LDM_test(const BYTE *src) {
  const U32 diff = 100;
  const BYTE *pCur = src + diff;
  U64 hash = getHash(pCur, LDM_HASH_LENGTH);

  for (; pCur < src + diff + 60; ++pCur) {
    U64 nextHash = getHash(pCur + 1, LDM_HASH_LENGTH);
    U64 updatedHash = updateHash(hash, LDM_HASH_LENGTH,
                                 pCur[0], pCur[LDM_HASH_LENGTH]);
    hash = nextHash;
    printf("%llu %llu\n", nextHash, updatedHash);
  }
}


