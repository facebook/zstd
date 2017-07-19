#include <stdlib.h>
#include <stdio.h>

#include "ldm.h"
#include "ldm_hashtable.h"
#include "mem.h"

#define LDM_HASHLOG ((LDM_MEMORY_USAGE) - 4)

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

static int isValidMatch(const BYTE *pIn, const BYTE *pMatch,
                            U32 minMatchLength, U32 maxWindowSize) {
  U32 lengthLeft = minMatchLength;
  const BYTE *curIn = pIn;
  const BYTE *curMatch = pMatch;

  if (pIn - pMatch > maxWindowSize) {
    return 0;
  }

  for (; lengthLeft >= 4; lengthLeft -= 4) {
    if (MEM_read32(curIn) != MEM_read32(curMatch)) {
      return 0;
    }
    curIn += 4;
    curMatch += 4;
  }
  return 1;
}

LDM_hashEntry *HASH_getValidEntry(const LDM_hashTable *table,
                                  const hash_t hash,
                                  const U32 checksum,
                                  const BYTE *pIn,
                                  const BYTE *pEnd,
                                  U32 minMatchLength,
                                  U32 maxWindowSize) {
  LDM_hashEntry *entry = getBucket(table, hash);
  (void)checksum;
  (void)pEnd;
  if (isValidMatch(pIn, entry->offset + table->offsetBase,
                   minMatchLength, maxWindowSize)) {
    return entry;
  }
  return NULL;
}

hash_t HASH_hashU32(U32 value) {
  return ((value * 2654435761U) >> (32 - LDM_HASHLOG));
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
