#ifndef LDM_HASHTABLE_H
#define LDM_HASHTABLE_H

#include "mem.h"

#define LDM_HASH_ENTRY_SIZE_LOG 3

// TODO: clean up comments

typedef U32 hash_t;

typedef struct LDM_hashEntry {
  U32 offset;   // TODO: Replace with pointer?
  U32 checksum;
} LDM_hashEntry;

typedef struct LDM_hashTable LDM_hashTable;

LDM_hashTable *HASH_createTable(U32 size, const BYTE *offsetBase,
                                U32 minMatchLength, U32 maxWindowSize);

LDM_hashEntry *HASH_getBestEntry(const LDM_hashTable *table,
                                 const hash_t hash,
                                 const U32 checksum,
                                 const BYTE *pIn,
                                 const BYTE *pEnd,
                                 const BYTE *pAnchor,
                                 U32 *matchLength,
                                 U32 *backwardsMatchLength);

hash_t HASH_hashU32(U32 value);

/**
 * Insert an LDM_hashEntry into the bucket corresponding to hash.
 */
void HASH_insert(LDM_hashTable *table, const hash_t hash,
                 const LDM_hashEntry entry);

/**
 * Return the number of distinct hash buckets.
 */
U32 HASH_getSize(const LDM_hashTable *table);

void HASH_destroyTable(LDM_hashTable *table);

/**
 * Prints the percentage of the hash table occupied (where occupied is defined
 * as the entry being non-zero).
 */
void HASH_outputTableOccupancy(const LDM_hashTable *hashTable);

#endif /* LDM_HASHTABLE_H */
