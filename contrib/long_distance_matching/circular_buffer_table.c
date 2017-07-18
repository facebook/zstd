#include <stdlib.h>
#include <stdio.h>

#include "ldm.h"
#include "ldm_hashtable.h"
#include "mem.h"

//TODO: move def somewhere else.

// Number of elements per hash bucket.
// HASH_BUCKET_SIZE_LOG defined in ldm.h
#define HASH_BUCKET_SIZE_LOG 0 // MAX is 4 for now
#define HASH_BUCKET_SIZE (1 << (HASH_BUCKET_SIZE_LOG))

#define LDM_HASHLOG ((LDM_MEMORY_USAGE)-4-HASH_BUCKET_SIZE_LOG)

struct LDM_hashTable {
  U32 size;  // Number of buckets
  U32 maxEntries;  // Rename...
  LDM_hashEntry *entries;  // 1-D array for now.

  // Position corresponding to offset=0 in LDM_hashEntry.
  const BYTE *offsetBase;
  BYTE *bucketOffsets;     // Pointer to current insert position.
                           // Last insert was at bucketOffsets - 1?
};

LDM_hashTable *HASH_createTable(U32 size, const BYTE *offsetBase) {
  LDM_hashTable *table = malloc(sizeof(LDM_hashTable));
  table->size = size >> HASH_BUCKET_SIZE_LOG;
  table->maxEntries = size;
  table->entries = calloc(size, sizeof(LDM_hashEntry));
  table->bucketOffsets = calloc(size >> HASH_BUCKET_SIZE_LOG, sizeof(BYTE));
  table->offsetBase = offsetBase;
  return table;
}

static LDM_hashEntry *getBucket(const LDM_hashTable *table, const hash_t hash) {
  return table->entries + (hash << HASH_BUCKET_SIZE_LOG);
}

LDM_hashEntry *HASH_getValidEntry(const LDM_hashTable *table,
                                  const hash_t hash,
                                  const U32 checksum,
                                  const BYTE *pIn,
                                  int (*isValid)(const BYTE *pIn, const BYTE *pMatch)) {
  LDM_hashEntry *bucket = getBucket(table, hash);
  LDM_hashEntry *cur = bucket;
  // TODO: in order of recency?
  for (; cur < bucket + HASH_BUCKET_SIZE; ++cur) {
    // Check checksum for faster check.
    if (cur->checksum == checksum &&
        (*isValid)(pIn, cur->offset + table->offsetBase)) {
      return cur;
    }
  }
  return NULL;
}

hash_t HASH_hashU32(U32 value) {
  return ((value * 2654435761U) >> (32 - LDM_HASHLOG));
}


LDM_hashEntry *HASH_getEntryFromHash(const LDM_hashTable *table,
                                     const hash_t hash,
                                     const U32 checksum) {
  // Loop through bucket.
  // TODO: in order of recency???
  LDM_hashEntry *bucket = getBucket(table, hash);
  LDM_hashEntry *cur = bucket;
  for(; cur < bucket + HASH_BUCKET_SIZE; ++cur) {
    if (cur->checksum == checksum) {
      return cur;
    }
  }
  return NULL;
}

void HASH_insert(LDM_hashTable *table,
                 const hash_t hash, const LDM_hashEntry entry) {
  *(getBucket(table, hash) + table->bucketOffsets[hash]) = entry;
  table->bucketOffsets[hash]++;
  table->bucketOffsets[hash] &= HASH_BUCKET_SIZE - 1;
}

U32 HASH_getSize(const LDM_hashTable *table) {
  return table->size;
}

void HASH_destroyTable(LDM_hashTable *table) {
  free(table->entries);
  free(table->bucketOffsets);
  free(table);
}

void HASH_outputTableOccupancy(const LDM_hashTable *table) {
  U32 ctr = 0;
  LDM_hashEntry *cur = table->entries;
  LDM_hashEntry *end = table->entries + (table->size * HASH_BUCKET_SIZE);
  for (; cur < end; ++cur) {
    if (cur->offset == 0) {
      ctr++;
    }
  }

  printf("Num buckets, bucket size: %d, %d\n", table->size, HASH_BUCKET_SIZE);
  printf("Hash table size, empty slots, %% empty: %u, %u, %.3f\n",
         table->maxEntries, ctr,
         100.0 * (double)(ctr) / table->maxEntries);
}
