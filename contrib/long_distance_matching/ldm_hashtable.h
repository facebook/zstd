#ifndef LDM_HASHTABLE_H
#define LDM_HASHTABLE_H

#include "mem.h"

// TODO: clean up comments

typedef U32 hash_t;

typedef struct LDM_hashEntry {
  U32 offset;   // TODO: Replace with pointer?
  U32 checksum;
} LDM_hashEntry;

typedef struct LDM_hashTable LDM_hashTable;

/**
 * Create a hash table with size hash buckets.
 * LDM_hashEntry.offset is added to offsetBase to calculate pMatch in
 * HASH_getValidEntry.
 */
LDM_hashTable *HASH_createTable(U32 size, const BYTE *offsetBase);

/**
 * Returns an LDM_hashEntry from the table that matches the checksum.
 * Returns NULL if one does not exist.
 */
LDM_hashEntry *HASH_getEntryFromHash(const LDM_hashTable *table,
                                     const hash_t hash,
                                     const U32 checksum);

/**
 *  Gets a valid entry that matches the checksum. A valid entry is defined by
 *  *isValid.
 *
 *  The function finds an entry matching the checksum, computes pMatch as
 *  offset + table.offsetBase, and calls isValid.
 */
LDM_hashEntry *HASH_getValidEntry(const LDM_hashTable *table,
                                  const hash_t hash,
                                  const U32 checksum,
                                  const BYTE *pIn,
                                  const BYTE *pEnd,
                                  U32 minMatchLength,
                                  U32 maxWindowSize);

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
