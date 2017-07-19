#include <stdlib.h>
#include <stdio.h>

#include "ldm.h"
#include "ldm_hashtable.h"
#include "mem.h"


// Number of elements per hash bucket.
// HASH_BUCKET_SIZE_LOG defined in ldm.h
#define HASH_BUCKET_SIZE (1 << (HASH_BUCKET_SIZE_LOG))

// TODO: rename. Number of hash buckets.
#define LDM_HASHLOG ((LDM_MEMORY_USAGE)-4-HASH_BUCKET_SIZE_LOG)
//#define ZSTD_SKIP

struct LDM_hashTable {
  U32 numBuckets;
  U32 numEntries;
  LDM_hashEntry *entries;
  BYTE *bucketOffsets;     // Pointer to current insert position.

  // Position corresponding to offset=0 in LDM_hashEntry.
  const BYTE *offsetBase;
  U32 minMatchLength;
  U32 maxWindowSize;
};

LDM_hashTable *HASH_createTable(U32 size, const BYTE *offsetBase,
                                U32 minMatchLength, U32 maxWindowSize) {
  LDM_hashTable *table = malloc(sizeof(LDM_hashTable));
  table->numBuckets = size >> HASH_BUCKET_SIZE_LOG;
  table->numEntries = size;
  table->entries = calloc(size, sizeof(LDM_hashEntry));
  table->bucketOffsets = calloc(size >> HASH_BUCKET_SIZE_LOG, sizeof(BYTE));
  table->offsetBase = offsetBase;
  table->minMatchLength = minMatchLength;
  table->maxWindowSize = maxWindowSize;
  return table;
}

static LDM_hashEntry *getBucket(const LDM_hashTable *table, const hash_t hash) {
  return table->entries + (hash << HASH_BUCKET_SIZE_LOG);
}

static unsigned ZSTD_NbCommonBytes (register size_t val)
{
    if (MEM_isLittleEndian()) {
        if (MEM_64bits()) {
#       if defined(_MSC_VER) && defined(_WIN64)
            unsigned long r = 0;
            _BitScanForward64( &r, (U64)val );
            return (unsigned)(r>>3);
#       elif defined(__GNUC__) && (__GNUC__ >= 3)
            return (__builtin_ctzll((U64)val) >> 3);
#       else
            static const int DeBruijnBytePos[64] = { 0, 0, 0, 0, 0, 1, 1, 2,
                                                     0, 3, 1, 3, 1, 4, 2, 7,
                                                     0, 2, 3, 6, 1, 5, 3, 5,
                                                     1, 3, 4, 4, 2, 5, 6, 7,
                                                     7, 0, 1, 2, 3, 3, 4, 6,
                                                     2, 6, 5, 5, 3, 4, 5, 6,
                                                     7, 1, 2, 4, 6, 4, 4, 5,
                                                     7, 2, 6, 5, 7, 6, 7, 7 };
            return DeBruijnBytePos[((U64)((val & -(long long)val) * 0x0218A392CDABBD3FULL)) >> 58];
#       endif
        } else { /* 32 bits */
#       if defined(_MSC_VER)
            unsigned long r=0;
            _BitScanForward( &r, (U32)val );
            return (unsigned)(r>>3);
#       elif defined(__GNUC__) && (__GNUC__ >= 3)
            return (__builtin_ctz((U32)val) >> 3);
#       else
            static const int DeBruijnBytePos[32] = { 0, 0, 3, 0, 3, 1, 3, 0,
                                                     3, 2, 2, 1, 3, 2, 0, 1,
                                                     3, 3, 1, 2, 2, 2, 2, 0,
                                                     3, 1, 2, 0, 1, 0, 1, 1 };
            return DeBruijnBytePos[((U32)((val & -(S32)val) * 0x077CB531U)) >> 27];
#       endif
        }
    } else {  /* Big Endian CPU */
        if (MEM_64bits()) {
#       if defined(_MSC_VER) && defined(_WIN64)
            unsigned long r = 0;
            _BitScanReverse64( &r, val );
            return (unsigned)(r>>3);
#       elif defined(__GNUC__) && (__GNUC__ >= 3)
            return (__builtin_clzll(val) >> 3);
#       else
            unsigned r;
            const unsigned n32 = sizeof(size_t)*4;   /* calculate this way due to compiler complaining in 32-bits mode */
            if (!(val>>n32)) { r=4; } else { r=0; val>>=n32; }
            if (!(val>>16)) { r+=2; val>>=8; } else { val>>=24; }
            r += (!val);
            return r;
#       endif
        } else { /* 32 bits */
#       if defined(_MSC_VER)
            unsigned long r = 0;
            _BitScanReverse( &r, (unsigned long)val );
            return (unsigned)(r>>3);
#       elif defined(__GNUC__) && (__GNUC__ >= 3)
            return (__builtin_clz((U32)val) >> 3);
#       else
            unsigned r;
            if (!(val>>16)) { r=2; val>>=8; } else { r=0; val>>=24; }
            r += (!val);
            return r;
#       endif
    }   }
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

U32 countBackwardsMatch(const BYTE *pIn, const BYTE *pAnchor,
                        const BYTE *pMatch, const BYTE *pBase) {
  U32 matchLength = 0;
  while (pIn > pAnchor && pMatch > pBase && pIn[-1] == pMatch[-1]) {
    pIn--;
    pMatch--;
    matchLength++;
  }
  return matchLength;
}

LDM_hashEntry *HASH_getBestEntry(const LDM_hashTable *table,
                                 const hash_t hash,
                                 const U32 checksum,
                                 const BYTE *pIn,
                                 const BYTE *pEnd,
                                 const BYTE *pAnchor,
                                 U32 *pForwardMatchLength,
                                 U32 *pBackwardMatchLength) {
  LDM_hashEntry *bucket = getBucket(table, hash);
  LDM_hashEntry *cur = bucket;
  LDM_hashEntry *bestEntry = NULL;
  U32 bestMatchLength = 0;
  for (; cur < bucket + HASH_BUCKET_SIZE; ++cur) {
    const BYTE *pMatch = cur->offset + table->offsetBase;

    // Check checksum for faster check.
    if (cur->checksum == checksum && pIn - pMatch <= table->maxWindowSize) {
      U32 forwardMatchLength = ZSTD_count(pIn, pMatch, pEnd);
      U32 backwardMatchLength, totalMatchLength;
      if (forwardMatchLength < table->minMatchLength) {
        continue;
      }
      backwardMatchLength =
          countBackwardsMatch(pIn, pAnchor, cur->offset + table->offsetBase,
                              table->offsetBase);

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

hash_t HASH_hashU32(U32 value) {
  return ((value * 2654435761U) >> (32 - LDM_HASHLOG));
}

void HASH_insert(LDM_hashTable *table,
                 const hash_t hash, const LDM_hashEntry entry) {
  *(getBucket(table, hash) + table->bucketOffsets[hash]) = entry;
  table->bucketOffsets[hash]++;
  table->bucketOffsets[hash] &= HASH_BUCKET_SIZE - 1;
}

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

  printf("Num buckets, bucket size: %d, %d\n",
         table->numBuckets, HASH_BUCKET_SIZE);
  printf("Hash table size, empty slots, %% empty: %u, %u, %.3f\n",
         table->numEntries, ctr,
         100.0 * (double)(ctr) / table->numEntries);
}
