#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "ldm.h"
#include "util.h"

// Insert every (HASH_ONLY_EVERY + 1) into the hash table.
#define HASH_ONLY_EVERY 0

#define LDM_MEMORY_USAGE 20
#define LDM_HASHLOG (LDM_MEMORY_USAGE-2)
#define LDM_HASHTABLESIZE (1 << (LDM_MEMORY_USAGE))
#define LDM_HASHTABLESIZE_U32 ((LDM_HASHTABLESIZE) >> 2)

#define LDM_OFFSET_SIZE 4

#define WINDOW_SIZE (1 << 20)

//These should be multiples of four.
#define LDM_HASH_LENGTH 4
#define MINMATCH 4

#define ML_BITS 4
#define ML_MASK ((1U<<ML_BITS)-1)
#define RUN_BITS (8-ML_BITS)
#define RUN_MASK ((1U<<RUN_BITS)-1)

#define COMPUTE_STATS
//#define RUN_CHECKS
//#define LDM_DEBUG

typedef  uint8_t BYTE;
typedef uint16_t U16;
typedef uint32_t U32;
typedef  int32_t S32;
typedef uint64_t U64;

typedef uint32_t offset_t;
typedef uint32_t hash_t;
typedef signed char schar;

typedef struct hashEntry {
  offset_t offset;
} hashEntry;

typedef struct LDM_compressStats {
  U32 numMatches;
  U32 totalMatchLength;
  U32 totalLiteralLength;
  U64 totalOffset;

  U32 numCollisions;
  U32 numHashInserts;
} LDM_compressStats;

typedef struct LDM_CCtx {
  size_t isize;             /* Input size */
  size_t maxOSize;          /* Maximum output size */

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

  hashEntry hashTable[LDM_HASHTABLESIZE_U32];

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
} LDM_CCtx;

#ifdef COMPUTE_STATS
/**
 * Outputs compression statistics.
 */
static void printCompressStats(const LDM_CCtx *cctx) {
  const LDM_compressStats *stats = &(cctx->stats);
  printf("=====================\n");
  printf("Compression statistics\n");
  printf("Total number of matches: %u\n", stats->numMatches);
  printf("Average match length: %.1f\n", ((double)stats->totalMatchLength) /
                                         (double)stats->numMatches);
  printf("Average literal length: %.1f\n",
         ((double)stats->totalLiteralLength) / (double)stats->numMatches);
  printf("Average offset length: %.1f\n",
         ((double)stats->totalOffset) / (double)stats->numMatches);
  printf("Num collisions, num hash inserts, %% collisions: %u, %u, %.3f\n",
         stats->numCollisions, stats->numHashInserts,
         stats->numHashInserts == 0 ?
            1.0 : (100.0 * (double)stats->numCollisions) /
                  (double)stats->numHashInserts);

  // Output occupancy of hash table.
  {
    U32 i = 0;
    U32 ctr = 0;
    for (; i < LDM_HASHTABLESIZE_U32; i++) {
      if ((cctx->hashTable)[i].offset == 0) {
        ctr++;
      }
    }
    printf("Hash table size, empty slots, %% empty: %u %u %.3f\n",
           LDM_HASHTABLESIZE_U32, ctr,
           100.0 * (double)(ctr) / (double)LDM_HASHTABLESIZE_U32);
  }

  printf("=====================\n");
}
#endif

/**
 * Checks whether the MINMATCH bytes from p are the same as the MINMATCH
 * bytes from match.
 *
 * This assumes MINMATCH is a multiple of four.
 *
 * Return 1 if valid, 0 otherwise.
 */
static int LDM_isValidMatch(const BYTE *p, const BYTE *match) {
  /*
  if (memcmp(p, match, MINMATCH) == 0) {
    return 1;
  }
  return 0;
  */

  //TODO: This seems to be faster for some reason?
  U16 lengthLeft = MINMATCH;
  const BYTE *curP = p;
  const BYTE *curMatch = match;

  for (; lengthLeft >= 8; lengthLeft -= 8) {
    if (LDM_read64(curP) != LDM_read64(curMatch)) {
      return 0;
    }
    curP += 8;
    curMatch += 8;
  }
  if (lengthLeft > 0) {
    return (LDM_read32(curP) == LDM_read32(curMatch));
  }
  return 1;
}

/**
 * Convert a sum computed from getChecksum to a hash value in the range
 * of the hash table.
 */
static hash_t checksumToHash(U32 sum) {
  return ((sum * 2654435761U) >> ((32)-LDM_HASHLOG));
}

/**
 * Computes a checksum based on rsync's checksum.
 *
 * a(k,l) = \sum_{i = k}^l x_i (mod M)
 * b(k,l) = \sum_{i = k}^l ((l - i + 1) * x_i) (mod M)
 * checksum(k,l) = a(k,l) + 2^{16} * b(k,l)
  */
static U32 getChecksum(const char *data, U32 len) {
  U32 i;
  U32 s1, s2;
  const schar *buf = (const schar *)data;

  s1 = s2 = 0;
  for (i = 0; i < (len - 4); i += 4) {
    s2 += (4 * (s1 + buf[i])) + (3 * buf[i + 1]) +
          (2 * buf[i + 2]) + (buf[i + 3]);
    s1 += buf[i] + buf[i + 1] + buf[i + 2] + buf[i + 3];
  }
  for(; i < len; i++) {
    s1 += buf[i];
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
                          schar toRemove, schar toAdd) {
  U32 s1 = (sum & 0xffff) - toRemove + toAdd;
  U32 s2 = (sum >> 16) - (toRemove * len) + s1;

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
      (schar)((cctx->lastPosHashed)[0]),
      (schar)((cctx->lastPosHashed)[LDM_HASH_LENGTH]));
  cctx->nextPosHashed = cctx->nextIp;
  cctx->nextHash = checksumToHash(cctx->nextSum);

#ifdef RUN_CHECKS
  check = getChecksum((const char *)cctx->nextIp, LDM_HASH_LENGTH);

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
    offset_t offset = (cctx->hashTable)[hash].offset;
    cctx->stats.numHashInserts++;
    if (offset != 0 && !LDM_isValidMatch(cctx->ip, offset + cctx->ibase)) {
      cctx->stats.numCollisions++;
    }
  }
#endif

  // Hash only every HASH_ONLY_EVERY times, based on cctx->ip.
  // Note: this works only when cctx->step is 1.
  if (((cctx->ip - cctx->ibase) & HASH_ONLY_EVERY) == HASH_ONLY_EVERY) {
    (cctx->hashTable)[hash] = (hashEntry){ (offset_t)(cctx->ip - cctx->ibase) };
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
  U32 sum = getChecksum((const char *)cctx->ip, LDM_HASH_LENGTH);
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
  return cctx->hashTable[hash].offset + cctx->ibase;
}

/**
 *  Counts the number of bytes that match from pIn and pMatch,
 *  up to pInLimit.
 *
 *  TODO: make more efficient.
 */
static unsigned countMatchLength(const BYTE *pIn, const BYTE *pMatch,
                                 const BYTE *pInLimit) {
  const BYTE * const pStart = pIn;
  while (pIn < pInLimit - 1) {
    BYTE const diff = LDM_readByte(pMatch) ^ LDM_readByte(pIn);
    if (!diff) {
      pIn++;
      pMatch++;
      continue;
    }
    return (unsigned)(pIn - pStart);
  }
  return (unsigned)(pIn - pStart);
}

void LDM_readHeader(const void *src, size_t *compressSize,
                    size_t *decompressSize) {
  const U32 *ip = (const U32 *)src;
  *compressSize = *ip++;
  *decompressSize = *ip;
}

/**
 * Initialize a compression context.
 */
static void initializeCCtx(LDM_CCtx *cctx,
                           const void *src, size_t srcSize,
                           void *dst, size_t maxDstSize) {
  cctx->isize = srcSize;
  cctx->maxOSize = maxDstSize;

  cctx->ibase = (const BYTE *)src;
  cctx->ip = cctx->ibase;
  cctx->iend = cctx->ibase + srcSize;

  cctx->ihashLimit = cctx->iend - LDM_HASH_LENGTH;
  cctx->imatchLimit = cctx->iend - MINMATCH;

  cctx->obase = (BYTE *)dst;
  cctx->op = (BYTE *)dst;

  cctx->anchor = cctx->ibase;

  memset(&(cctx->stats), 0, sizeof(cctx->stats));
  memset(cctx->hashTable, 0, sizeof(cctx->hashTable));

  cctx->lastPosHashed = NULL;

  cctx->step = 1;   // Fixed to be 1 for now. Changing may break things.
  cctx->nextIp = cctx->ip + cctx->step;
  cctx->nextPosHashed = 0;

  cctx->DEBUG_setNextHash = 0;
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

  } while (cctx->ip - *match > WINDOW_SIZE ||
           !LDM_isValidMatch(cctx->ip, *match));
  setNextHash(cctx);
  return 0;
}

/**
 * Write current block (literals, literal length, match offset,
 * match length).
 *
 * Update input pointer, inserting hashes into hash table along the way.
 */
static void outputBlock(LDM_CCtx *cctx,
                        unsigned const literalLength,
                        unsigned const offset,
                        unsigned const matchLength) {
  BYTE *token = cctx->op++;

  /* Encode the literal length. */
  if (literalLength >= RUN_MASK) {
    int len = (int)literalLength - RUN_MASK;
    *token = (RUN_MASK << ML_BITS);
    for (; len >= 255; len -= 255) {
      *(cctx->op)++ = 255;
    }
    *(cctx->op)++ = (BYTE)len;
  } else {
    *token = (BYTE)(literalLength << ML_BITS);
  }

  /* Encode the literals. */
  memcpy(cctx->op, cctx->anchor, literalLength);
  cctx->op += literalLength;

  /* Encode the offset. */
  LDM_write32(cctx->op, offset);
  cctx->op += LDM_OFFSET_SIZE;

  /* Encode the match length. */
  if (matchLength >= ML_MASK) {
    unsigned matchLengthRemaining = matchLength;
    *token += ML_MASK;
    matchLengthRemaining -= ML_MASK;
    LDM_write32(cctx->op, 0xFFFFFFFF);
    while (matchLengthRemaining >= 4*0xFF) {
      cctx->op += 4;
      LDM_write32(cctx->op, 0xffffffff);
      matchLengthRemaining -= 4*0xFF;
    }
    cctx->op += matchLengthRemaining / 255;
    *(cctx->op)++ = (BYTE)(matchLengthRemaining % 255);
  } else {
    *token += (BYTE)(matchLength);
  }
}

// TODO: srcSize and maxDstSize is unused
size_t LDM_compress(const void *src, size_t srcSize,
                    void *dst, size_t maxDstSize) {
  LDM_CCtx cctx;
  initializeCCtx(&cctx, src, srcSize, dst, maxDstSize);

  /* Hash the first position and put it into the hash table. */
  LDM_putHashOfCurrentPosition(&cctx);

  // TODO: loop condition is not accurate.
  while (1) {
    const BYTE *match;

    /**
     * Find a match.
     * If no more matches can be found (i.e. the length of the remaining input
     * is less than the minimum match length), then stop searching for matches
     * and encode the final literals.
     */
    if (LDM_findBestMatch(&cctx, &match) != 0) {
      goto _last_literals;
    }
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
      unsigned const literalLength = (unsigned)(cctx.ip - cctx.anchor);
      unsigned const offset = cctx.ip - match;
      unsigned const matchLength = countMatchLength(
          cctx.ip + MINMATCH, match + MINMATCH, cctx.ihashLimit);

#ifdef COMPUTE_STATS
      cctx.stats.totalLiteralLength += literalLength;
      cctx.stats.totalOffset += offset;
      cctx.stats.totalMatchLength += matchLength + MINMATCH;
#endif
      outputBlock(&cctx, literalLength, offset, matchLength);

      // Move ip to end of block, inserting hashes at each position.
      cctx.nextIp = cctx.ip + cctx.step;
      while (cctx.ip < cctx.anchor + MINMATCH + matchLength + literalLength) {
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
_last_literals:
  /* Encode the last literals (no more matches). */
  {
    size_t const lastRun = (size_t)(cctx.iend - cctx.anchor);
    if (lastRun >= RUN_MASK) {
      size_t accumulator = lastRun - RUN_MASK;
      *(cctx.op)++ = RUN_MASK << ML_BITS;
      for(; accumulator >= 255; accumulator -= 255) {
        *(cctx.op)++ = 255;
      }
      *(cctx.op)++ = (BYTE)accumulator;
    } else {
      *(cctx.op)++ = (BYTE)(lastRun << ML_BITS);
    }
    memcpy(cctx.op, cctx.anchor, lastRun);
    cctx.op += lastRun;
  }

#ifdef COMPUTE_STATS
  printCompressStats(&cctx);
#endif

  return (cctx.op - (const BYTE *)cctx.obase);
}

typedef struct LDM_DCtx {
  size_t compressSize;
  size_t maxDecompressSize;

  const BYTE *ibase;   /* Base of input */
  const BYTE *ip;      /* Current input position */
  const BYTE *iend;    /* End of source */

  const BYTE *obase;   /* Base of output */
  BYTE *op;            /* Current output position */
  const BYTE *oend;    /* End of output */
} LDM_DCtx;

static void LDM_initializeDCtx(LDM_DCtx *dctx,
                               const void *src, size_t compressSize,
                               void *dst, size_t maxDecompressSize) {
  dctx->compressSize = compressSize;
  dctx->maxDecompressSize = maxDecompressSize;

  dctx->ibase = src;
  dctx->ip = (const BYTE *)src;
  dctx->iend = dctx->ip + dctx->compressSize;
  dctx->op = dst;
  dctx->oend = dctx->op + dctx->maxDecompressSize;

}

size_t LDM_decompress(const void *src, size_t compressSize,
                      void *dst, size_t maxDecompressSize) {
  LDM_DCtx dctx;
  LDM_initializeDCtx(&dctx, src, compressSize, dst, maxDecompressSize);

  while (dctx.ip < dctx.iend) {
    BYTE *cpy;
    const BYTE *match;
    size_t length, offset;

    /* Get the literal length. */
    unsigned const token = *(dctx.ip)++;
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
    offset = LDM_read32(dctx.ip);
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
    length += MINMATCH;

    /* Copy match. */
    cpy = dctx.op + length;

    // Inefficient for now.
    while (match < cpy - offset && dctx.op < dctx.oend) {
      *(dctx.op)++ = *match++;
    }
  }
  return dctx.op - (BYTE *)dst;
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


