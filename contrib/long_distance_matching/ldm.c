#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ldm.h"

// Insert every (HASH_ONLY_EVERY + 1) into the hash table.
#define HASH_ONLY_EVERY 0

#define ML_BITS 4
#define ML_MASK ((1U<<ML_BITS)-1)
#define RUN_BITS (8-ML_BITS)
#define RUN_MASK ((1U<<RUN_BITS)-1)

#define COMPUTE_STATS
#define CHECKSUM_CHAR_OFFSET 10
//#define RUN_CHECKS
//#define LDM_DEBUG

struct LDM_hashEntry {
  offset_t offset;
};

// TODO: move to its own file.
struct LDM_hashTable {
  U32 size;
  LDM_hashEntry *entries;
};

LDM_hashEntry *HASH_getHash(
    const LDM_hashTable *table, const hash_t hash) {
  return &(table->entries[hash]);
}

void HASH_insert(LDM_hashTable *table,
                 const hash_t hash, const LDM_hashEntry entry) {
  *HASH_getHash(table, hash) = entry;
}


// TODO: Scanning speed
// TODO: Memory usage
struct LDM_compressStats {
  U32 windowSizeLog, hashTableSizeLog;
  U32 numMatches;
  U64 totalMatchLength;
  U64 totalLiteralLength;
  U64 totalOffset;

  U32 minOffset, maxOffset;

  U32 numCollisions;
  U32 numHashInserts;

  U32 offsetHistogram[32];
};

struct LDM_CCtx {
  U64 isize;             /* Input size */
  U64 maxOSize;          /* Maximum output size */

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

  LDM_hashTable hashTable;

//  LDM_hashEntry *hashTable;

//  LDM_hashEntry hashTable[LDM_HASHTABLESIZE_U32];

  const BYTE *lastPosHashed;          /* Last position hashed */
  hash_t lastHash;                    /* Hash corresponding to lastPosHashed */
  U32 lastSum;

  const BYTE *nextIp;                 // TODO: this is  redundant (ip + step)
  const BYTE *nextPosHashed;
  hash_t nextHash;                    /* Hash corresponding to nextPosHashed */
  U32 nextSum;

  unsigned step;                      // ip step, should be 1.

  // DEBUG
  const BYTE *DEBUG_setNextHash;
};



void LDM_outputHashTableOccupancy(const LDM_hashTable *hashTable) {
  U32 i = 0;
  U32 ctr = 0;
  for (; i < hashTable->size; i++) {
    if (HASH_getHash(hashTable, i)->offset == 0) {
      ctr++;
    }
  }
  printf("Hash table size, empty slots, %% empty: %u, %u, %.3f\n",
         hashTable->size, ctr,
         100.0 * (double)(ctr) / (double)hashTable->size);
}

// TODO: This can be done more efficiently (but it is not that important as it
// is only used for computing stats).
static int intLog2(U32 x) {
  int ret = 0;
  while (x >>= 1) {
    ret++;
  }
  return ret;
}

// TODO: Maybe we would eventually prefer to have linear rather than
// exponential buckets.
void LDM_outputHashTableOffsetHistogram(const LDM_CCtx *cctx) {
  U32 i = 0;
  int buckets[32] = { 0 };

  printf("\n");
  printf("Hash table histogram\n");
  for (; i < cctx->hashTable.size; i++) {
    int offset = (cctx->ip - cctx->ibase) -
                 HASH_getHash(&cctx->hashTable, i)->offset;
    buckets[intLog2(offset)]++;
  }

  i = 0;
  for (; i < 32; i++) {
    printf("2^%*d: %10u    %6.3f%%\n", 2, i,
           buckets[i],
           100.0 * (double) buckets[i] /
                   (double) cctx->hashTable.size);
  }
  printf("\n");
}

void LDM_printCompressStats(const LDM_compressStats *stats) {
  int i = 0;
  printf("=====================\n");
  printf("Compression statistics\n");
  //TODO: compute percentage matched?
  printf("Window size, hash table size (bytes): 2^%u, 2^%u\n",
          stats->windowSizeLog, stats->hashTableSizeLog);
  printf("num matches, total match length: %u, %llu\n",
          stats->numMatches,
          stats->totalMatchLength);
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
  printf("offset histogram: offset, num matches, %% of matches\n");

  for (; i <= intLog2(stats->maxOffset); i++) {
    printf("2^%*d: %10u    %6.3f%%\n", 2, i,
           stats->offsetHistogram[i],
           100.0 * (double) stats->offsetHistogram[i] /
                   (double) stats->numMatches);
  }
  printf("\n");


  printf("num collisions, num hash inserts, %% collisions: %u, %u, %.3f\n",
         stats->numCollisions, stats->numHashInserts,
         stats->numHashInserts == 0 ?
            1.0 : (100.0 * (double)stats->numCollisions) /
                  (double)stats->numHashInserts);
  printf("=====================\n");

}

int LDM_isValidMatch(const BYTE *pIn, const BYTE *pMatch) {
  /*
  if (memcmp(pIn, pMatch, LDM_MIN_MATCH_LENGTH) == 0) {
    return 1;
  }
  return 0;
  */

  //TODO: This seems to be faster for some reason?
  U32 lengthLeft = LDM_MIN_MATCH_LENGTH;
  const BYTE *curIn = pIn;
  const BYTE *curMatch = pMatch;

  for (; lengthLeft >= 8; lengthLeft -= 8) {
    if (MEM_read64(curIn) != MEM_read64(curMatch)) {
      return 0;
    }
    curIn += 8;
    curMatch += 8;
  }
  if (lengthLeft > 0) {
    return (MEM_read32(curIn) == MEM_read32(curMatch));
  }
  return 1;
}

/**
 * Convert a sum computed from getChecksum to a hash value in the range
 * of the hash table.
 */
static hash_t checksumToHash(U32 sum) {
  return ((sum * 2654435761U) >> (32 - LDM_HASHLOG));
}

/**
 * Computes a checksum based on rsync's checksum.
 *
 * a(k,l) = \sum_{i = k}^l x_i (mod M)
 * b(k,l) = \sum_{i = k}^l ((l - i + 1) * x_i) (mod M)
 * checksum(k,l) = a(k,l) + 2^{16} * b(k,l)
  */
static U32 getChecksum(const BYTE *buf, U32 len) {
  U32 i;
  U32 s1, s2;

  s1 = s2 = 0;
  for (i = 0; i < (len - 4); i += 4) {
    s2 += (4 * (s1 + buf[i])) + (3 * buf[i + 1]) +
          (2 * buf[i + 2]) + (buf[i + 3]) +
          (10 * CHECKSUM_CHAR_OFFSET);
    s1 += buf[i] + buf[i + 1] + buf[i + 2] + buf[i + 3] +
          + (4 * CHECKSUM_CHAR_OFFSET);

  }
  for(; i < len; i++) {
    s1 += buf[i] + CHECKSUM_CHAR_OFFSET;
    s2 += s1;
  }
  return (s1 & 0xffff) + (s2 << 16);
}

/**
 * Update a checksum computed from getChecksum(data, len).
 *
 * The checksum can be updated along its ends as follows:
 * a(k+1, l+1) = (a(k,l) - x_k + x_{l+1}) (mod M)
 * b(k+1, l+1) = (b(k,l) - (l-k+1)*x_k + (a(k+1,l+1)) (mod M)
 *
 * Thus toRemove should correspond to data[0].
 */
static U32 updateChecksum(U32 sum, U32 len,
                          BYTE toRemove, BYTE toAdd) {
  U32 s1 = (sum & 0xffff) - toRemove + toAdd;
  U32 s2 = (sum >> 16) - ((toRemove + CHECKSUM_CHAR_OFFSET) * len) + s1;

  return (s1 & 0xffff) + (s2 << 16);
}

/**
 * Update cctx->nextSum, cctx->nextHash, and cctx->nextPosHashed
 * based on cctx->lastSum and cctx->lastPosHashed.
 *
 * This uses a rolling hash and requires that the last position hashed
 * corresponds to cctx->nextIp - step.
 */
static void setNextHash(LDM_CCtx *cctx) {
#ifdef RUN_CHECKS
  U32 check;
  if ((cctx->nextIp - cctx->ibase != 1) &&
      (cctx->nextIp - cctx->DEBUG_setNextHash != 1)) {
    printf("CHECK debug fail: %zu %zu\n", cctx->nextIp - cctx->ibase,
            cctx->DEBUG_setNextHash - cctx->ibase);
  }

  cctx->DEBUG_setNextHash = cctx->nextIp;
#endif

//  cctx->nextSum = getChecksum((const char *)cctx->nextIp, LDM_HASH_LENGTH);
  cctx->nextSum = updateChecksum(
      cctx->lastSum, LDM_HASH_LENGTH,
      cctx->lastPosHashed[0],
      cctx->lastPosHashed[LDM_HASH_LENGTH]);
  cctx->nextPosHashed = cctx->nextIp;
  cctx->nextHash = checksumToHash(cctx->nextSum);

#ifdef RUN_CHECKS
  check = getChecksum(cctx->nextIp, LDM_HASH_LENGTH);

  if (check != cctx->nextSum) {
    printf("CHECK: setNextHash failed %u %u\n", check, cctx->nextSum);
  }

  if ((cctx->nextIp - cctx->lastPosHashed) != 1) {
    printf("setNextHash: nextIp != lastPosHashed + 1. %zu %zu %zu\n",
            cctx->nextIp - cctx->ibase, cctx->lastPosHashed - cctx->ibase,
            cctx->ip - cctx->ibase);
  }
#endif
}

static void putHashOfCurrentPositionFromHash(
    LDM_CCtx *cctx, hash_t hash, U32 sum) {
#ifdef COMPUTE_STATS
  if (cctx->stats.numHashInserts < LDM_HASHTABLESIZE_U32) {
    offset_t offset = HASH_getHash(&cctx->hashTable, hash)->offset;
    cctx->stats.numHashInserts++;
    if (offset != 0 && !LDM_isValidMatch(cctx->ip, offset + cctx->ibase)) {
      cctx->stats.numCollisions++;
    }
  }
#endif

  // Hash only every HASH_ONLY_EVERY times, based on cctx->ip.
  // Note: this works only when cctx->step is 1.
  if (((cctx->ip - cctx->ibase) & HASH_ONLY_EVERY) == HASH_ONLY_EVERY) {
    const LDM_hashEntry entry = { cctx->ip - cctx->ibase };
    HASH_insert(&cctx->hashTable, hash, entry);
  }

  cctx->lastPosHashed = cctx->ip;
  cctx->lastHash = hash;
  cctx->lastSum = sum;
}

/**
 * Copy over the cctx->lastHash, cctx->lastSum, and cctx->lastPosHashed
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
  putHashOfCurrentPositionFromHash(cctx, cctx->nextHash, cctx->nextSum);
}

/**
 * Insert hash of the current position into the hash table.
 */
static void LDM_putHashOfCurrentPosition(LDM_CCtx *cctx) {
  U32 sum = getChecksum(cctx->ip, LDM_HASH_LENGTH);
  hash_t hash = checksumToHash(sum);

#ifdef RUN_CHECKS
  if (cctx->nextPosHashed != cctx->ip && (cctx->ip != cctx->ibase)) {
    printf("CHECK failed: putHashOfCurrentPosition %zu\n",
           cctx->ip - cctx->ibase);
  }
#endif

  putHashOfCurrentPositionFromHash(cctx, hash, sum);
}

/**
 * Returns the position of the entry at hashTable[hash].
 */
static const BYTE *getPositionOnHash(LDM_CCtx *cctx, hash_t hash) {
  return HASH_getHash(&cctx->hashTable, hash)->offset + cctx->ibase;
}

U32 LDM_countMatchLength(const BYTE *pIn, const BYTE *pMatch,
                         const BYTE *pInLimit) {
  const BYTE * const pStart = pIn;
  while (pIn < pInLimit - 1) {
    BYTE const diff = (*pMatch) ^ *(pIn);
    if (!diff) {
      pIn++;
      pMatch++;
      continue;
    }
    return (U32)(pIn - pStart);
  }
  return (U32)(pIn - pStart);
}

void LDM_readHeader(const void *src, U64 *compressedSize,
                    U64 *decompressedSize) {
  const BYTE *ip = (const BYTE *)src;
  *compressedSize = MEM_readLE64(ip);
  ip += sizeof(U64);
  *decompressedSize = MEM_readLE64(ip);
  // ip += sizeof(U64);
}

static void LDM_initializeHashTable(LDM_hashTable *table) {
  table->size = LDM_HASHTABLESIZE_U32;
  table->entries = calloc(LDM_HASHTABLESIZE_U32, sizeof(LDM_hashEntry));
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

  LDM_initializeHashTable(&cctx->hashTable);
//    calloc(LDM_HASHTABLESIZE_U32, sizeof(LDM_hashEntry));
//  memset(cctx->hashTable, 0, sizeof(cctx->hashTable));
  cctx->stats.minOffset = UINT_MAX;
  cctx->stats.windowSizeLog = LDM_WINDOW_SIZE_LOG;
  cctx->stats.hashTableSizeLog = LDM_MEMORY_USAGE;


  cctx->lastPosHashed = NULL;

  cctx->step = 1;   // Fixed to be 1 for now. Changing may break things.
  cctx->nextIp = cctx->ip + cctx->step;
  cctx->nextPosHashed = 0;

  cctx->DEBUG_setNextHash = 0;
}

void LDM_destroyCCtx(LDM_CCtx *cctx) {
  free((cctx->hashTable).entries);
}

/**
 * Finds the "best" match.
 *
 * Returns 0 if successful and 1 otherwise (i.e. no match can be found
 * in the remaining input that is long enough).
 *
 */
static int LDM_findBestMatch(LDM_CCtx *cctx, const BYTE **match) {
  cctx->nextIp = cctx->ip + cctx->step;

  do {
    hash_t h;
    U32 sum;
    setNextHash(cctx);
    h = cctx->nextHash;
    sum = cctx->nextSum;
    cctx->ip = cctx->nextIp;
    cctx->nextIp += cctx->step;

    if (cctx->ip > cctx->imatchLimit) {
      return 1;
    }

    *match = getPositionOnHash(cctx, h);
    putHashOfCurrentPositionFromHash(cctx, h, sum);

  } while (cctx->ip - *match > LDM_WINDOW_SIZE ||
           !LDM_isValidMatch(cctx->ip, *match));
  setNextHash(cctx);
  return 0;
}

void LDM_encodeLiteralLengthAndLiterals(
    LDM_CCtx *cctx, BYTE *pToken, const U32 literalLength) {
  /* Encode the literal length. */
  if (literalLength >= RUN_MASK) {
    int len = (int)literalLength - RUN_MASK;
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
                     const U32 literalLength,
                     const U32 offset,
                     const U32 matchLength) {
  BYTE *pToken = cctx->op++;

  /* Encode the literal length and literals. */
  LDM_encodeLiteralLengthAndLiterals(cctx, pToken, literalLength);

  /* Encode the offset. */
  MEM_write32(cctx->op, offset);
  cctx->op += LDM_OFFSET_SIZE;

  /* Encode the match length. */
  if (matchLength >= ML_MASK) {
    unsigned matchLengthRemaining = matchLength;
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
  const BYTE *match;
  LDM_initializeCCtx(&cctx, src, srcSize, dst, maxDstSize);

  /* Hash the first position and put it into the hash table. */
  LDM_putHashOfCurrentPosition(&cctx);

  /**
   * Find a match.
   * If no more matches can be found (i.e. the length of the remaining input
   * is less than the minimum match length), then stop searching for matches
   * and encode the final literals.
   */
  while (LDM_findBestMatch(&cctx, &match) == 0) {
#ifdef COMPUTE_STATS
    cctx.stats.numMatches++;
#endif

    /**
     * Catch up: look back to extend the match backwards from the found match.
     */
    while (cctx.ip > cctx.anchor && match > cctx.ibase &&
           cctx.ip[-1] == match[-1]) {
      cctx.ip--;
      match--;
    }

    /**
     * Write current block (literals, literal length, match offset, match
     * length) and update pointers and hashes.
     */
    {
      const U32 literalLength = cctx.ip - cctx.anchor;
      const U32 offset = cctx.ip - match;
      const U32 matchLength = LDM_countMatchLength(
          cctx.ip + LDM_MIN_MATCH_LENGTH, match + LDM_MIN_MATCH_LENGTH,
          cctx.ihashLimit);

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

  // LDM_outputHashTableOffsetHistogram(&cctx);

  /* Encode the last literals (no more matches). */
  {
    const size_t lastRun = cctx.iend - cctx.anchor;
    BYTE *pToken = cctx.op++;
    LDM_encodeLiteralLengthAndLiterals(&cctx, pToken, lastRun);
  }

#ifdef COMPUTE_STATS
  LDM_printCompressStats(&cctx.stats);
  LDM_outputHashTableOccupancy(&cctx.hashTable);
#endif

  {
    const size_t ret = cctx.op - cctx.obase;
    LDM_destroyCCtx(&cctx);
    return ret;
  }
}

struct LDM_DCtx {
  size_t compressedSize;
  size_t maxDecompressedSize;

  const BYTE *ibase;   /* Base of input */
  const BYTE *ip;      /* Current input position */
  const BYTE *iend;    /* End of source */

  const BYTE *obase;   /* Base of output */
  BYTE *op;            /* Current output position */
  const BYTE *oend;    /* End of output */
};

void LDM_initializeDCtx(LDM_DCtx *dctx,
                        const void *src, size_t compressedSize,
                        void *dst, size_t maxDecompressedSize) {
  dctx->compressedSize = compressedSize;
  dctx->maxDecompressedSize = maxDecompressedSize;

  dctx->ibase = src;
  dctx->ip = (const BYTE *)src;
  dctx->iend = dctx->ip + dctx->compressedSize;
  dctx->op = dst;
  dctx->oend = dctx->op + dctx->maxDecompressedSize;
}

size_t LDM_decompress(const void *src, size_t compressedSize,
                      void *dst, size_t maxDecompressedSize) {
  LDM_DCtx dctx;
  LDM_initializeDCtx(&dctx, src, compressedSize, dst, maxDecompressedSize);

  while (dctx.ip < dctx.iend) {
    BYTE *cpy;
    const BYTE *match;
    size_t length, offset;

    /* Get the literal length. */
    const unsigned token = *(dctx.ip)++;
    if ((length = (token >> ML_BITS)) == RUN_MASK) {
      unsigned s;
      do {
        s = *(dctx.ip)++;
        length += s;
      } while (s == 255);
    }

    /* Copy the literals. */
    cpy = dctx.op + length;
    memcpy(dctx.op, dctx.ip, length);
    dctx.ip += length;
    dctx.op = cpy;

    //TODO : dynamic offset size
    offset = MEM_read32(dctx.ip);
    dctx.ip += LDM_OFFSET_SIZE;
    match = dctx.op - offset;

    /* Get the match length. */
    length = token & ML_MASK;
    if (length == ML_MASK) {
      unsigned s;
      do {
        s = *(dctx.ip)++;
        length += s;
      } while (s == 255);
    }
    length += LDM_MIN_MATCH_LENGTH;

    /* Copy match. */
    cpy = dctx.op + length;

    // Inefficient for now.
    while (match < cpy - offset && dctx.op < dctx.oend) {
      *(dctx.op)++ = *match++;
    }
  }
  return dctx.op - (BYTE *)dst;
}

// TODO: implement and test hash function
void LDM_test(void) {
}

/*
void LDM_test(const void *src, size_t srcSize,
              void *dst, size_t maxDstSize) {
  const BYTE *ip = (const BYTE *)src + 1125;
  U32 sum = getChecksum((const char *)ip, LDM_HASH_LENGTH);
  U32 sum2;
  ++ip;
  for (; ip < (const BYTE *)src + 1125 + 100; ip++) {
    sum2 = updateChecksum(sum, LDM_HASH_LENGTH,
                                 ip[-1], ip[LDM_HASH_LENGTH - 1]);
    sum = getChecksum((const char *)ip, LDM_HASH_LENGTH);
    printf("TEST HASH: %zu %u %u\n", ip - (const BYTE *)src, sum, sum2);
  }
}
*/


