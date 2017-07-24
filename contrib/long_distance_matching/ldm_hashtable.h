/**
 * A "hash" table used in LDM compression.
 *
 * This is not exactly a hash table in the sense that inserted entries
 * are not guaranteed to remain in the hash table.
 */

#ifndef LDM_HASHTABLE_H
#define LDM_HASHTABLE_H

#include "mem.h"

// The log size of LDM_hashEntry in bytes.
#define LDM_HASH_ENTRY_SIZE_LOG 3

typedef U32 hash_t;

typedef struct LDM_hashEntry {
  U32 offset;       // Represents the offset of the entry from offsetBase.
  U32 checksum;     // A checksum to select entries with the same hash value.
} LDM_hashEntry;

typedef struct LDM_hashTable LDM_hashTable;

/**
 * Create a table that can contain size elements. This does not necessarily
 * correspond to the number of hash buckets. The number of hash buckets
 * is size / (1 << HASH_BUCKET_SIZE_LOG)
 *
 * minMatchLength is the minimum match length required in HASH_getBestEntry.
 *
 * maxWindowSize is the maximum distance from pIn in HASH_getBestEntry.
 * The window is defined to be (pIn - offsetBase - offset).
 */
LDM_hashTable *HASH_createTable(U32 size, const BYTE *offsetBase,
                                U32 minMatchLength, U32 maxWindowSize);

/**
 * Return the "best" entry from the table with the same hash and checksum.
 *
 * pIn: a pointer to the current input position.
 * pEnd: a pointer to the maximum input position.
 * pAnchor: a pointer to the minimum input position.
 *
 * This function computes the forward and backward match length from pIn
 * and writes it to forwardMatchLength and backwardsMatchLength.
 *
 * E.g. for the two strings "aaabbbb" "aaabbbb" with pIn and the
 * entry pointing at the first "b", the forward match length would be
 * four (representing the "b" matches) and the backward match length would
 * three (representing the "a" matches before the pointer).
 */
LDM_hashEntry *HASH_getBestEntry(const LDM_hashTable *table,
                                 const hash_t hash,
                                 const U32 checksum,
                                 const BYTE *pIn,
                                 const BYTE *pEnd,
                                 const BYTE *pAnchor,
                                 U64 *forwardMatchLength,
                                 U64 *backwardsMatchLength);

/**
 * Return a hash of the value.
 */
hash_t HASH_hashU32(U32 value);

/**
 * Insert an LDM_hashEntry into the bucket corresponding to hash.
 *
 * An entry may be evicted in the process.
 */
void HASH_insert(LDM_hashTable *table, const hash_t hash,
                 const LDM_hashEntry entry);

/**
 * Return the number of distinct hash buckets.
 */
U32 HASH_getSize(const LDM_hashTable *table);

/**
 * Destroy the table.
 */
void HASH_destroyTable(LDM_hashTable *table);

/**
 * Prints the percentage of the hash table occupied (where occupied is defined
 * as the entry being non-zero).
 */
void HASH_outputTableOccupancy(const LDM_hashTable *hashTable);

#endif /* LDM_HASHTABLE_H */
