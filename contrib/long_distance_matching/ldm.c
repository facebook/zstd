#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "ldm.h"
#include "util.h"

#define HASH_EVERY 7

#define LDM_MEMORY_USAGE 14
#define LDM_HASHLOG (LDM_MEMORY_USAGE-2)
#define LDM_HASHTABLESIZE (1 << (LDM_MEMORY_USAGE))
#define LDM_HASHTABLESIZE_U32 ((LDM_HASHTABLESIZE) >> 2)
#define LDM_HASH_SIZE_U32 (1 << (LDM_HASHLOG))

#define WINDOW_SIZE (1 << 20)
#define MAX_WINDOW_SIZE 31
#define HASH_SIZE 8
#define MINMATCH 8

#define ML_BITS 4
#define ML_MASK ((1U<<ML_BITS)-1)
#define RUN_BITS (8-ML_BITS)
#define RUN_MASK ((1U<<RUN_BITS)-1)

//#define LDM_DEBUG

typedef  uint8_t BYTE;
typedef uint16_t U16;
typedef uint32_t U32;
typedef  int32_t S32;
typedef uint64_t U64;

typedef uint32_t offset_t;
typedef uint32_t hash_t;

// typedef uint64_t tag;

/*
static unsigned LDM_isLittleEndian(void) {
    const union { U32 u; BYTE c[4]; } one = { 1 };
    return one.c[0];
}

static U16 LDM_read16(const void *memPtr) {
  U16 val;
  memcpy(&val, memPtr, sizeof(val));
  return val;
}

static U16 LDM_readLE16(const void *memPtr) {
  if (LDM_isLittleEndian()) {
    return LDM_read16(memPtr);
  } else {
    const BYTE *p = (const BYTE *)memPtr;
    return (U16)((U16)p[0] + (p[1] << 8));
  }
}

static void LDM_write16(void *memPtr, U16 value){
  memcpy(memPtr, &value, sizeof(value));
}

static void LDM_write32(void *memPtr, U32 value) {
  memcpy(memPtr, &value, sizeof(value));
}

static void LDM_writeLE16(void *memPtr, U16 value) {
  if (LDM_isLittleEndian()) {
    LDM_write16(memPtr, value);
  } else {
    BYTE* p = (BYTE *)memPtr;
    p[0] = (BYTE) value;
    p[1] = (BYTE)(value>>8);
  }
}

static U32 LDM_read32(const void *ptr) {
  return *(const U32 *)ptr;
}

static U64 LDM_read64(const void *ptr) {
  return *(const U64 *)ptr;
}

static void LDM_copy8(void *dst, const void *src) {
  memcpy(dst, src, 8);
}

*/
typedef struct LDM_hashEntry {
  offset_t offset;
} LDM_hashEntry;

typedef struct LDM_compressStats {
  U32 num_matches;
  U32 total_match_length;
  U32 total_literal_length;
  U64 total_offset;
} LDM_compressStats;

static void LDM_printCompressStats(const LDM_compressStats *stats) {
  printf("=====================\n");
  printf("Compression statistics\n");
  printf("Total number of matches: %u\n", stats->num_matches);
  printf("Average match length: %.1f\n", ((double)stats->total_match_length) /
                                         (double)stats->num_matches);
  printf("Average literal length: %.1f\n",
         ((double)stats->total_literal_length) / (double)stats->num_matches);
  printf("Average offset length: %.1f\n",
         ((double)stats->total_offset) / (double)stats->num_matches);
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

} LDM_CCtx;


static hash_t LDM_hash(U32 sequence) {
  return ((sequence * 2654435761U) >> ((32)-LDM_HASHLOG));
}

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

static hash_t LDM_hash_position(const void * const p) {
  return LDM_hash(LDM_read32(p));
}

static void LDM_putHashOfPosition(const BYTE *p, hash_t h,
                                  void *tableBase, const BYTE *srcBase) {
  LDM_hashEntry *hashTable;
  if (((p - srcBase) & HASH_EVERY) != HASH_EVERY) {
    return;
  }

  hashTable = (LDM_hashEntry *) tableBase;
  hashTable[h] = (LDM_hashEntry) { (hash_t)(p - srcBase) };
}

static void LDM_putPosition(const BYTE *p, void *tableBase,
                            const BYTE *srcBase) {
  hash_t hash;
  if (((p - srcBase) & HASH_EVERY) != HASH_EVERY) {
    return;
  }
  hash = LDM_hash_position(p);
  LDM_putHashOfPosition(p, hash, tableBase, srcBase);
}

static void LDM_putHashOfCurrentPosition(LDM_CCtx *const cctx) {
  hash_t hash = LDM_hash_position(cctx->ip);
  LDM_putHashOfPosition(cctx->ip, hash, cctx->hashTable, cctx->ibase);
  cctx->lastPosHashed = cctx->ip;
  cctx->lastHash = hash;
}

static const BYTE *LDM_get_position_on_hash(
    hash_t h, void *tableBase, const BYTE *srcBase) {
  const LDM_hashEntry * const hashTable = (LDM_hashEntry *)tableBase;
  return hashTable[h].offset + srcBase;
}

static BYTE LDM_read_byte(const void *memPtr) {
  BYTE val;
  memcpy(&val, memPtr, 1);
  return val;
}

static unsigned LDM_count(const BYTE *pIn, const BYTE *pMatch,
                          const BYTE *pInLimit) {
  const BYTE * const pStart = pIn;
  while (pIn < pInLimit - 1) {
    BYTE const diff = LDM_read_byte(pMatch) ^ LDM_read_byte(pIn);
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

  cctx->ihashLimit = cctx->iend - HASH_SIZE;
  cctx->imatchLimit = cctx->iend - MINMATCH;

  cctx->obase = (BYTE *)dst;
  cctx->op = (BYTE *)dst;

  cctx->anchor = cctx->ibase;

  memset(&(cctx->stats), 0, sizeof(cctx->stats));
  memset(cctx->hashTable, 0, sizeof(cctx->hashTable));

  cctx->lastPosHashed = NULL;
}

// TODO: srcSize and maxDstSize is unused
size_t LDM_compress(const void *src, size_t srcSize,
                    void *dst, size_t maxDstSize) {
  LDM_CCtx cctx;
  U32 forwardH;
  LDM_initializeCCtx(&cctx, src, srcSize, dst, maxDstSize);


  /* Hash the first position and put it into the hash table. */
  LDM_putHashOfCurrentPosition(&cctx);
  cctx.ip++;
  forwardH = LDM_hash_position(cctx.ip);

  // TODO: loop condition is not accurate.
  while (1) {
    const BYTE *match;
    BYTE *token;

    /* Find a match */
    {
      const BYTE *forwardIp = cctx.ip;
      unsigned step = 1;

      do {
        U32 const h = forwardH;
        cctx.ip = forwardIp;
        forwardIp += step;

        if (forwardIp > cctx.imatchLimit) {
          goto _last_literals;
        }

        match = LDM_get_position_on_hash(h, cctx.hashTable, cctx.ibase);

        forwardH = LDM_hash_position(forwardIp);
        LDM_putHashOfPosition(cctx.ip, h, cctx.hashTable, cctx.ibase);
      } while (cctx.ip - match > WINDOW_SIZE ||
               LDM_read64(match) != LDM_read64(cctx.ip));
    }
    cctx.stats.num_matches++;

    /* Catchup: look back to extend match from found match */
    while (cctx.ip > cctx.anchor && match > cctx.ibase && cctx.ip[-1] == match[-1]) {
      cctx.ip--;
      match--;
    }

    /* Encode literals */
    {
      unsigned const litLength = (unsigned)(cctx.ip - cctx.anchor);
      token = cctx.op++;

      cctx.stats.total_literal_length += litLength;

#ifdef LDM_DEBUG
      printf("Cur position: %zu\n", cctx.anchor - cctx.ibase);
      printf("LitLength %zu. (Match offset). %zu\n", litLength, cctx.ip - match);
#endif

      if (litLength >= RUN_MASK) {
        int len = (int)litLength - RUN_MASK;
        *token = (RUN_MASK << ML_BITS);
        for (; len >= 255; len -= 255) {
          *(cctx.op)++ = 255;
        }
        *(cctx.op)++ = (BYTE)len;
      } else {
        *token = (BYTE)(litLength << ML_BITS);
      }
#ifdef LDM_DEBUG
      printf("Literals ");
      fwrite(cctx.anchor, litLength, 1, stdout);
      printf("\n");
#endif
      memcpy(cctx.op, cctx.anchor, litLength);
      cctx.op += litLength;
    }

    /* Encode offset */
    {
      /*
      LDM_writeLE16(cctx.op, cctx.ip-match);
      cctx.op += 2;
      */
      LDM_write32(cctx.op, cctx.ip - match);
      cctx.op += 4;
      cctx.stats.total_offset += (cctx.ip - match);
    }

    /* Encode Match Length */
    {
      unsigned matchCode;
      unsigned ctr = 1;
      matchCode = LDM_count(cctx.ip + MINMATCH, match + MINMATCH,
                            cctx.ihashLimit);
#ifdef LDM_DEBUG
      printf("Match length %zu\n", matchCode + MINMATCH);
      fwrite(cctx.ip, MINMATCH + matchCode, 1, stdout);
      printf("\n");
#endif
      cctx.stats.total_match_length += matchCode + MINMATCH;
      cctx.ip++;
      for (; ctr < MINMATCH + matchCode; cctx.ip++, ctr++) {
        LDM_putHashOfCurrentPosition(&cctx);
      }
//      cctx.ip += MINMATCH + matchCode;
      if (matchCode >= ML_MASK) {
        *token += ML_MASK;
        matchCode -= ML_MASK;
        LDM_write32(cctx.op, 0xFFFFFFFF);
        while (matchCode >= 4*0xFF) {
          cctx.op += 4;
          LDM_write32(cctx.op, 0xffffffff);
          matchCode -= 4*0xFF;
        }
        cctx.op += matchCode / 255;
        *(cctx.op)++ = (BYTE)(matchCode % 255);
      } else {
        *token += (BYTE)(matchCode);
      }
#ifdef LDM_DEBUG
      printf("\n");

#endif
    }

    cctx.anchor = cctx.ip;

    LDM_putPosition(cctx.ip, cctx.hashTable, cctx.ibase);
    forwardH = LDM_hash_position(++cctx.ip);
  }
_last_literals:
    /* Encode last literals */
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
    size_t length;
    const BYTE *match;
    size_t offset;

    /* get literal length */
    unsigned const token = *(dctx.ip)++;
    if ((length = (token >> ML_BITS)) == RUN_MASK) {
      unsigned s;
      do {
        s = *(dctx.ip)++;
        length += s;
      } while (s == 255);
    }
#ifdef LDM_DEBUG
    printf("Literal length: %zu\n", length);
#endif

    /* copy literals */
    cpy = dctx.op + length;
#ifdef LDM_DEBUG
    printf("Literals ");
    fwrite(dctx.ip, length, 1, stdout);
    printf("\n");
#endif
    memcpy(dctx.op, dctx.ip, length);
    dctx.ip += length;
    dctx.op = cpy;

    /* get offset */
    /*
    offset = LDM_readLE16(dctx.ip);
    dctx.ip += 2;
    */

    //TODO : dynamic offset size
    offset = LDM_read32(dctx.ip);
    dctx.ip += 4;
#ifdef LDM_DEBUG
    printf("Offset: %zu\n", offset);
#endif
    match = dctx.op - offset;
 //   LDM_write32(op, (U32)offset);

    /* get matchlength */
    length = token & ML_MASK;
    if (length == ML_MASK) {
      unsigned s;
      do {
        s = *(dctx.ip)++;
        length += s;
      } while (s == 255);
    }
    length += MINMATCH;
#ifdef LDM_DEBUG
    printf("Match length: %zu\n", length);
#endif
    /* copy match */
    cpy = dctx.op + length;

    // Inefficient for now
    while (match < cpy - offset && dctx.op < dctx.oend) {
      *(dctx.op)++ = *match++;
    }
  }
  return dctx.op - (BYTE *)dst;
}


