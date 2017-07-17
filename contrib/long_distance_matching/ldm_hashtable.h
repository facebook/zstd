#ifndef LDM_HASHTABLE_H
#define LDM_HASHTABLE_H

#include "mem.h"

typedef U32 hash_t;

typedef struct LDM_hashEntry {
  U32 offset;
  U32 checksum; // Not needed?
} LDM_hashEntry;

typedef struct LDM_hashTable LDM_hashTable;

// TODO: rename functions
// TODO: comments

LDM_hashTable *HASH_createTable(U32 size, const BYTE *offsetBase);

LDM_hashEntry *HASH_getEntryFromHash(const LDM_hashTable *table,
                                     const hash_t hash,
                                     const U32 checksum);

void HASH_insert(LDM_hashTable *table, const hash_t hash,
                        const LDM_hashEntry entry);

U32 HASH_getSize(const LDM_hashTable *table);

void HASH_destroyTable(LDM_hashTable *table);

/**
 * Prints the percentage of the hash table occupied (where occupied is defined
 * as the entry being non-zero).
 */
void HASH_outputTableOccupancy(const LDM_hashTable *hashTable);


#endif /* LDM_HASHTABLE_H */
