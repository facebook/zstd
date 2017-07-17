#include <stdlib.h>
#include <stdio.h>

#include "ldm_hashtable.h"
#include "mem.h"

struct LDM_hashTable {
  U32 size;
  LDM_hashEntry *entries;
  const BYTE *offsetBase;
};

LDM_hashTable *HASH_createTable(U32 size, const BYTE *offsetBase) {
  LDM_hashTable *table = malloc(sizeof(LDM_hashTable));
  table->size = size;
  table->entries = calloc(size, sizeof(LDM_hashEntry));
  table->offsetBase = offsetBase;
  return table;
}

void HASH_initializeTable(LDM_hashTable *table, U32 size) {
  table->size = size;
  table->entries = calloc(size, sizeof(LDM_hashEntry));
}

LDM_hashEntry *getBucket(const LDM_hashTable *table, const hash_t hash) {
  return table->entries + hash;
}

LDM_hashEntry *HASH_getEntryFromHash(
    const LDM_hashTable *table, const hash_t hash, const U32 checksum) {
  (void)checksum;
  return getBucket(table, hash);
}

void HASH_insert(LDM_hashTable *table,
                 const hash_t hash, const LDM_hashEntry entry) {
  *getBucket(table, hash) = entry;
}

U32 HASH_getSize(const LDM_hashTable *table) {
  return table->size;
}

void HASH_destroyTable(LDM_hashTable *table) {
  free(table->entries);
  free(table);
}

void HASH_outputTableOccupancy(const LDM_hashTable *hashTable) {
  U32 i = 0;
  U32 ctr = 0;
  for (; i < HASH_getSize(hashTable); i++) {
    if (getBucket(hashTable, i)->offset == 0) {
      ctr++;
    }
  }
  printf("Hash table size, empty slots, %% empty: %u, %u, %.3f\n",
         HASH_getSize(hashTable), ctr,
         100.0 * (double)(ctr) / (double)HASH_getSize(hashTable));
}
