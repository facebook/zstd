#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>


#include "ldm.h"
#include "util.h"

#define HASH_EVERY 1

#define LDM_MEMORY_USAGE 22
#define LDM_HASHLOG (LDM_MEMORY_USAGE-2)
#define LDM_HASHTABLESIZE (1 << (LDM_MEMORY_USAGE))
#define LDM_HASHTABLESIZE_U32 ((LDM_HASHTABLESIZE) >> 2)
#define LDM_HASH_SIZE_U32 (1 << (LDM_HASHLOG))

#define LDM_OFFSET_SIZE 4

#define WINDOW_SIZE (1 << 23)
#define MAX_WINDOW_SIZE 31
#define HASH_SIZE 4
#define LDM_HASH_LENGTH 4

// Should be multiple of four
#define MINMATCH 4

#define ML_BITS 4
#define ML_MASK ((1U<<ML_BITS)-1)
#define RUN_BITS (8-ML_BITS)
#define RUN_MASK ((1U<<RUN_BITS)-1)

#define LDM_ROLLING_HASH
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


typedef struct LDM_hashEntry {
  offset_t offset;
} LDM_hashEntry;

typedef struct LDM_compressStats {
  U32 numMatches;
  U32 totalMatchLength;
  U32 totalLiteralLength;
  U64 totalOffset;

  U32 numCollisions;
  U32 numHashInserts;
} LDM_compressStats;

static void LDM_printCompressStats(const LDM_compressStats *stats) {
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
  printf("=====================\n");
}

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

  LDM_hashEntry hashTable[LDM_HASHTABLESIZE_U32];

  const BYTE *lastPosHashed;          /* Last position hashed */
  hash_t lastHash;                    /* Hash corresponding to lastPosHashed */
  const BYTE *nextIp;
  const BYTE *nextPosHashed;
  hash_t nextHash;                    /* Hash corresponding to nextPosHashed */

  // Members for rolling hash.
  U32 lastSum;
  U32 nextSum;

  unsigned step;

  // DEBUG
  const BYTE *DEBUG_setNextHash;
} LDM_CCtx;

static int LDM_isValidMatch(const BYTE *p, const BYTE *match) {
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
    return LDM_read32(curP) == LDM_read32(curMatch);
  }
  return 1;
}



#ifdef LDM_ROLLING_HASH
/**
 * Convert a sum computed from LDM_getRollingHash to a hash value in the range
 * of the hash table.
 */
static hash_t LDM_sumToHash(U32 sum) {
  return sum & (LDM_HASH_SIZE_U32 - 1);
}

static U32 LDM_getRollingHash(const char *data, U32 len) {
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

typedef struct LDM_sumStruct {
  U16 s1, s2;
} LDM_sumStruct;

static U32 LDM_updateRollingHash(U32 sum, U32 len,
                                 schar toRemove, schar toAdd) {
  U32 s1 = (sum & 0xffff) - toRemove + toAdd;
  U32 s2 = (sum >> 16) - (toRemove * len) + s1;

  return (s1 & 0xffff) + (s2 << 16);
}


/*
static hash_t LDM_hashPosition(const void * const p) {
  return LDM_sumToHash(LDM_getRollingHash((const char *)p, LDM_HASH_LENGTH));
}
*/

/*
static void LDM_getRollingHashParts(U32 sum, LDM_sumStruct *sumStruct) {
  sumStruct->s1 = sum & 0xffff;
  sumStruct->s2 = sum >> 16;
}
*/

static void LDM_setNextHash(LDM_CCtx *cctx) {

#ifdef RUN_CHECKS
  U32 check;
  if ((cctx->nextIp - cctx->ibase != 1) &&
      (cctx->nextIp - cctx->DEBUG_setNextHash != 1)) {
    printf("CHECK debug fail: %zu %zu\n", cctx->nextIp - cctx->ibase,
            cctx->DEBUG_setNextHash - cctx->ibase);
  }

  cctx->DEBUG_setNextHash = cctx->nextIp;
#endif

//  cctx->nextSum = LDM_getRollingHash((const char *)cctx->nextIp, LDM_HASH_LENGTH);
  cctx->nextSum = LDM_updateRollingHash(
      cctx->lastSum, LDM_HASH_LENGTH,
      (schar)((cctx->lastPosHashed)[0]),
      (schar)((cctx->lastPosHashed)[LDM_HASH_LENGTH]));

#ifdef RUN_CHECKS
  check = LDM_getRollingHash((const char *)cctx->nextIp, LDM_HASH_LENGTH);

  if (check != cctx->nextSum) {
    printf("CHECK: setNextHash failed %u %u\n", check, cctx->nextSum);
//    printf("INFO: %u %u %u\n", LDM_read32(cctx->nextIp),
  }
#endif
  cctx->nextPosHashed = cctx->nextIp;
  cctx->nextHash = LDM_sumToHash(cctx->nextSum);

#ifdef RUN_CHECKS
  if ((cctx->nextIp - cctx->lastPosHashed) != 1) {
    printf("setNextHash: nextIp != lastPosHashed + 1. %zu %zu %zu\n",
            cctx->nextIp - cctx->ibase, cctx->lastPosHashed - cctx->ibase,
            cctx->ip - cctx->ibase);
  }
#endif

}

static void LDM_putHashOfCurrentPositionFromHash(
    LDM_CCtx *cctx, hash_t hash, U32 sum) {
  /*
  if (((cctx->ip - cctx->ibase) & HASH_EVERY) != HASH_EVERY) {
    return;
  }
  */
#ifdef COMPUTE_STATS
  if (cctx->stats.numHashInserts < LDM_HASHTABLESIZE_U32) {
    offset_t offset = (cctx->hashTable)[hash].offset;
    cctx->stats.numHashInserts++;
    if (offset == 0 && !LDM_isValidMatch(cctx->ip, offset + cctx->ibase)) {
      cctx->stats.numCollisions++;
    }
  }
#endif
  (cctx->hashTable)[hash] = (LDM_hashEntry){ (hash_t)(cctx->ip - cctx->ibase) };
  cctx->lastPosHashed = cctx->ip;
  cctx->lastHash = hash;
  cctx->lastSum = sum;
}

static void LDM_updateLastHashFromNextHash(LDM_CCtx *cctx) {
#ifdef RUN_CHECKS
  if (cctx->ip != cctx->nextPosHashed) {
    printf("CHECK failed: updateLastHashFromNextHash %zu\n", cctx->ip - cctx->ibase);
  }
#endif
  LDM_putHashOfCurrentPositionFromHash(cctx, cctx->nextHash, cctx->nextSum);
}

static void LDM_putHashOfCurrentPosition(LDM_CCtx *cctx) {
  U32 sum = LDM_getRollingHash((const char *)cctx->ip, LDM_HASH_LENGTH);
  hash_t hash = LDM_sumToHash(sum);
#ifdef RUN_CHECKS
  if (cctx->nextPosHashed != cctx->ip && (cctx->ip != cctx->ibase)) {
    printf("CHECK failed: putHashOfCurrentPosition %zu\n", cctx->ip - cctx->ibase);
  }
#endif
//  hash_t hash = LDM_hashPosition(cctx->ip);
  LDM_putHashOfCurrentPositionFromHash(cctx, hash, sum);
//  printf("Offset %zu\n", cctx->ip - cctx->ibase);
}

#else
static hash_t LDM_hash(U32 sequence) {
  return ((sequence * 2654435761U) >> ((32)-LDM_HASHLOG));
}

static hash_t LDM_hashPosition(const void * const p) {
  return LDM_hash(LDM_read32(p));
}

static void LDM_putHashOfCurrentPositionFromHash(
    LDM_CCtx *cctx, hash_t hash) {
  /*
  if (((cctx->ip - cctx->ibase) & HASH_EVERY) != HASH_EVERY) {
    return;
  }
  */
#ifdef COMPUTE_STATS
  if (cctx->stats.numHashInserts < LDM_HASHTABLESIZE_U32) {
    offset_t offset = (cctx->hashTable)[hash].offset;
    cctx->stats.numHashInserts++;
    if (offset == 0 && !LDM_isValidMatch(cctx->ip, offset + cctx->ibase)) {
      cctx->stats.numCollisions++;
    }
  }
#endif

  (cctx->hashTable)[hash] = (LDM_hashEntry){ (hash_t)(cctx->ip - cctx->ibase) };
#ifdef RUN_CHECKS
  if (cctx->ip - cctx->lastPosHashed != 1) {
    printf("putHashError\n");
  }
#endif
  cctx->lastPosHashed = cctx->ip;
  cctx->lastHash = hash;
}

static void LDM_putHashOfCurrentPosition(LDM_CCtx *cctx) {
  hash_t hash = LDM_hashPosition(cctx->ip);
  LDM_putHashOfCurrentPositionFromHash(cctx, hash);
}

#endif

/*
static hash_t LDM_hash5(U64 sequence) {
  static const U64 prime5bytes = 889523592379ULL;
  static const U64 prime8bytes = 11400714785074694791ULL;
  const U32 hashLog = LDM_HASHLOG;
  if (LDM_isLittleEndian())
    return (((sequence << 24) * prime5bytes) >> (64 - hashLog));
  else
    return (((sequence >> 24) * prime8bytes) >> (64 - hashLog));
}
*/


static const BYTE *LDM_getPositionOnHash(
    hash_t h, void *tableBase, const BYTE *srcBase) {
  const LDM_hashEntry * const hashTable = (LDM_hashEntry *)tableBase;
  return hashTable[h].offset + srcBase;
}


static unsigned LDM_count(const BYTE *pIn, const BYTE *pMatch,
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

static void LDM_initializeCCtx(LDM_CCtx *cctx,
                               const void *src, size_t srcSize,
                               void *dst, size_t maxDstSize) {
  cctx->isize = srcSize;
  cctx->maxOSize = maxDstSize;

  cctx->ibase = (const BYTE *)src;
  cctx->ip = cctx->ibase;
  cctx->iend = cctx->ibase + srcSize;

#ifdef LDM_ROLLING_HASH
  cctx->ihashLimit = cctx->iend - LDM_HASH_LENGTH;
#else
  cctx->ihashLimit = cctx->iend - HASH_SIZE;
#endif
  cctx->imatchLimit = cctx->iend - MINMATCH;

  cctx->obase = (BYTE *)dst;
  cctx->op = (BYTE *)dst;

  cctx->anchor = cctx->ibase;

  memset(&(cctx->stats), 0, sizeof(cctx->stats));
  memset(cctx->hashTable, 0, sizeof(cctx->hashTable));

  cctx->lastPosHashed = NULL;

  cctx->step = 1;
  cctx->nextIp = cctx->ip + cctx->step;
  cctx->nextPosHashed = 0;

  cctx->DEBUG_setNextHash = 0;
}

#ifdef LDM_ROLLING_HASH
static int LDM_findBestMatch(LDM_CCtx *cctx, const BYTE **match) {
  cctx->nextIp = cctx->ip + cctx->step;

  do {
    hash_t h;
    U32 sum;
//    printf("Call A\n");
    LDM_setNextHash(cctx);
//    printf("End call a\n");
    h = cctx->nextHash;
    sum = cctx->nextSum;
    cctx->ip = cctx->nextIp;
    cctx->nextIp += cctx->step;

    if (cctx->ip > cctx->imatchLimit) {
      return 1;
    }

    *match = LDM_getPositionOnHash(h, cctx->hashTable, cctx->ibase);

//    // Compute cctx->nextSum and cctx->nextHash from cctx->nextIp.
//    LDM_setNextHash(cctx);
    LDM_putHashOfCurrentPositionFromHash(cctx, h, sum);

//    printf("%u %u\n", cctx->lastHash, cctx->nextHash);
  } while (cctx->ip - *match > WINDOW_SIZE ||
           !LDM_isValidMatch(cctx->ip, *match));
//           LDM_read64(*match) != LDM_read64(cctx->ip));
  LDM_setNextHash(cctx);
  return 0;
}
#else
static int LDM_findBestMatch(LDM_CCtx *cctx, const BYTE **match) {
  cctx->nextIp = cctx->ip;

  do {
    hash_t const h = cctx->nextHash;
    cctx->ip = cctx->nextIp;
    cctx->nextIp += cctx->step;

    if (cctx->ip > cctx->imatchLimit) {
      return 1;
    }

    *match = LDM_getPositionOnHash(h, cctx->hashTable, cctx->ibase);

    cctx->nextHash = LDM_hashPosition(cctx->nextIp);
    LDM_putHashOfCurrentPositionFromHash(cctx, h);

  } while (cctx->ip - *match > WINDOW_SIZE ||
           !LDM_isValidMatch(cctx->ip, *match));
  return 0;
}

#endif

/**
 * Write current block (literals, literal length, match offset,
 * match length).
 *
 * Update input pointer, inserting hashes into hash table along the
 * way.
 */
static void LDM_outputBlock(LDM_CCtx *cctx, const BYTE *match) {
  unsigned const literalLength = (unsigned)(cctx->ip - cctx->anchor);
  unsigned const offset = cctx->ip - match;
  unsigned const matchLength = LDM_count(
      cctx->ip + MINMATCH, match + MINMATCH, cctx->ihashLimit);
  BYTE *token = cctx->op++;

  cctx->stats.totalLiteralLength += literalLength;
  cctx->stats.totalOffset += offset;
  cctx->stats.totalMatchLength += matchLength + MINMATCH;

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

  /* Encode match length */
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

//  LDM_setNextHash(cctx);
//  cctx->ip = cctx->lastPosHashed + 1;
//  cctx->nextIp = cctx->ip + cctx->step;
//  printf("HERE: %zu %zu %zu\n", cctx->ip - cctx->ibase,
//         cctx->lastPosHashed - cctx->ibase, cctx->nextIp - cctx->ibase);

  cctx->nextIp = cctx->ip + cctx->step;

  while (cctx->ip < cctx->anchor + MINMATCH + matchLength + literalLength) {
//    printf("Loop\n");
    if (cctx->ip > cctx->lastPosHashed) {
      LDM_updateLastHashFromNextHash(cctx);
//      LDM_putHashOfCurrentPosition(cctx);
#ifdef LDM_ROLLING_HASH
      LDM_setNextHash(cctx);
#endif
    }
    /*
    printf("Call b %zu %zu %zu\n",
            cctx->lastPosHashed - cctx->ibase,
            cctx->nextIp - cctx->ibase,
            cctx->ip - cctx->ibase);
            */
//    printf("end call b\n");
    cctx->ip++;
    cctx->nextIp++;
  }

//  printf("There: %zu %zu\n", cctx->ip - cctx->ibase, cctx->lastPosHashed - cctx->ibase);
}

// TODO: srcSize and maxDstSize is unused
size_t LDM_compress(const void *src, size_t srcSize,
                    void *dst, size_t maxDstSize) {
  LDM_CCtx cctx;
  LDM_initializeCCtx(&cctx, src, srcSize, dst, maxDstSize);

  /* Hash the first position and put it into the hash table. */
  LDM_putHashOfCurrentPosition(&cctx);
#ifdef LDM_ROLLING_HASH
//  LDM_setNextHash(&cctx);
//  tmp_hash = LDM_updateRollingHash(cctx.lastSum, LDM_HASH_LENGTH,
//                                   cctx.ip[0], cctx.ip[LDM_HASH_LENGTH]);
//  printf("Update test: %u %u\n", tmp_hash, cctx.nextSum);
//  cctx.ip++;
#else
  cctx.ip++;
  cctx.nextHash = LDM_hashPosition(cctx.ip);
#endif

  // TODO: loop condition is not accurate.
  while (1) {
    const BYTE *match;
//    printf("Start of loop\n");

    /**
     * Find a match.
     * If no more matches can be found (i.e. the length of the remaining input
     * is less than the minimum match length), then stop searching for matches
     * and encode the final literals.
     */
    if (LDM_findBestMatch(&cctx, &match) != 0) {
      goto _last_literals;
    }
//    printf("End of match finding\n");

    cctx.stats.numMatches++;

    /**
     * Catch up: look back to extend the match backwards from the found match.
     */
    while (cctx.ip > cctx.anchor && match > cctx.ibase &&
           cctx.ip[-1] == match[-1]) {
//      printf("Catch up\n");
      cctx.ip--;
      match--;
    }

    /**
     * Write current block (literals, literal length, match offset, match
     * length) and update pointers and hashes.
     */
    LDM_outputBlock(&cctx, match);
//    printf("End of loop\n");

    // Set start of next block to current input pointer.
    cctx.anchor = cctx.ip;
    LDM_updateLastHashFromNextHash(&cctx);
//    LDM_putHashOfCurrentPosition(&cctx);
#ifndef LDM_ROLLING_HASH
    cctx.ip++;
#endif

    /*
    LDM_putHashOfCurrentPosition(&cctx);
    printf("Call c\n");
    LDM_setNextHash(&cctx);
    printf("End call c\n");
    cctx.ip++;
    cctx.nextIp++;
    */
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
  LDM_printCompressStats(&cctx.stats);
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

    /* Copy literals. */
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

void LDM_test(const void *src, size_t srcSize,
              void *dst, size_t maxDstSize) {
#ifdef LDM_ROLLING_HASH
  const BYTE *ip = (const BYTE *)src + 1125;
  U32 sum = LDM_getRollingHash((const char *)ip, LDM_HASH_LENGTH);
  U32 sum2;
  ++ip;
  for (; ip < (const BYTE *)src + 1125 + 100; ip++) {
    sum2 = LDM_updateRollingHash(sum, LDM_HASH_LENGTH,
                                 ip[-1], ip[LDM_HASH_LENGTH - 1]);
    sum = LDM_getRollingHash((const char *)ip, LDM_HASH_LENGTH);
    printf("TEST HASH: %zu %u %u\n", ip - (const BYTE *)src, sum, sum2);
  }
#endif
}


