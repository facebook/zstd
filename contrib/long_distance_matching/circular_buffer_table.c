#include <stdlib.h>
#include <stdio.h>

#include "ldm_hashtable.h"
#include "mem.h"

//TODO: move def somewhere else.
//TODO: memory usage is currently no longer LDM_MEMORY_USAGE.
//      refactor code to scale the number of elements appropriately.

// Number of elements per hash bucket.
#define HASH_BUCKET_SIZE_LOG 1  // MAX is 4 for now
#define HASH_BUCKET_SIZE (1 << (HASH_BUCKET_SIZE_LOG))

struct LDM_hashTable {
  U32 size;
  LDM_hashEntry *entries;  // 1-D array for now.

  // Position corresponding to offset=0 in LDM_hashEntry.
  const BYTE *offsetBase;
  BYTE *bucketOffsets;     // Pointer to current insert position.
                           // Last insert was at bucketOffsets - 1?
};

LDM_hashTable *HASH_createTable(U32 size, const BYTE *offsetBase) {
  LDM_hashTable *table = malloc(sizeof(LDM_hashTable));
  table->size = size;
  table->entries = calloc(size * HASH_BUCKET_SIZE, sizeof(LDM_hashEntry));
  table->bucketOffsets = calloc(size, sizeof(BYTE));
  table->offsetBase = offsetBase;
  return table;
}

static LDM_hashEntry *getBucket(const LDM_hashTable *table, const hash_t hash) {
  return table->entries + (hash << HASH_BUCKET_SIZE_LOG);
}

/*
static LDM_hashEntry *getLastInsertFromHash(const LDM_hashTable *table,
                                            const hash_t hash) {
  LDM_hashEntry *bucket = getBucket(table, hash);
  BYTE offset = (table->bucketOffsets[hash] - 1) & (HASH_BUCKET_SIZE - 1);
  return bucket + offset;
}
*/

LDM_hashEntry *HASH_getValidEntry(const LDM_hashTable *table,
                                  const hash_t hash,
                                  const U32 checksum,
                                  const BYTE *pIn,
                                  int (*isValid)(const BYTE *pIn, const BYTE *pMatch)) {
  LDM_hashEntry *bucket = getBucket(table, hash);
  LDM_hashEntry *cur = bucket;
  // TODO: in order of recency?
  for (; cur < bucket + HASH_BUCKET_SIZE; ++cur) {
    // CHeck checksum for faster check.
    if (cur->checksum == checksum &&
        (*isValid)(pIn, cur->offset + table->offsetBase)) {
      return cur;
    }
  }
  return NULL;
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
  return table->size * HASH_BUCKET_SIZE;
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

  printf("Hash table size, empty slots, %% empty: %u, %u, %.3f\n",
         HASH_getSize(table), ctr,
         100.0 * (double)(ctr) / (double)HASH_getSize(table));
}
