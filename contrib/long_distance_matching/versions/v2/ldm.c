#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "ldm.h"

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

typedef uint64_t tag;

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
    BYTE* p = (BYTE*)memPtr;
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

typedef struct compress_stats {
  U32 num_matches;
  U32 total_match_length;
  U32 total_literal_length;
  U64 total_offset;
} compress_stats;

static void LDM_printCompressStats(const compress_stats *stats) {
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

// TODO: unused.
struct hash_entry {
  U64 offset;
  tag t;
};

static U32 LDM_hash(U32 sequence) {
  return ((sequence * 2654435761U) >> ((32)-LDM_HASHLOG));
}

static U32 LDM_hash5(U64 sequence) {
  static const U64 prime5bytes = 889523592379ULL;
  static const U64 prime8bytes = 11400714785074694791ULL;
  const U32 hashLog = LDM_HASHLOG;
  if (LDM_isLittleEndian())
    return (U32)(((sequence << 24) * prime5bytes) >> (64 - hashLog));
  else
    return (U32)(((sequence >> 24) * prime8bytes) >> (64 - hashLog));
}

static U32 LDM_hash_position(const void * const p) {
  return LDM_hash(LDM_read32(p));
}

static void LDM_put_position_on_hash(const BYTE *p, U32 h, void *tableBase,
                                     const BYTE *srcBase) {
  if (((p - srcBase) & HASH_EVERY) != HASH_EVERY) {
    return;
  }

  U32 *hashTable = (U32 *) tableBase;
  hashTable[h] = (U32)(p - srcBase);
}

static void LDM_put_position(const BYTE *p, void *tableBase,
                             const BYTE *srcBase) {
  if (((p - srcBase) & HASH_EVERY) != HASH_EVERY) {
    return;
  }
  U32 const h = LDM_hash_position(p);
  LDM_put_position_on_hash(p, h, tableBase, srcBase);
}

static const BYTE *LDM_get_position_on_hash(
    U32 h, void *tableBase, const BYTE *srcBase) {
  const U32 * const hashTable = (U32*)tableBase;
  return hashTable[h] + srcBase;
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

void LDM_read_header(const void *src, size_t *compressSize,
                     size_t *decompressSize) {
  const U32 *ip = (const U32 *)src;
  *compressSize = *ip++;
  *decompressSize = *ip;
}

// TODO: maxDstSize is unused
size_t LDM_compress(const void *src, size_t srcSize,
                    void *dst, size_t maxDstSize) {
  const BYTE * const istart = (const BYTE*)src;
  const BYTE *ip = istart;
  const BYTE * const iend = istart + srcSize;
  const BYTE *ilimit = iend - HASH_SIZE;
  const BYTE * const matchlimit = iend - HASH_SIZE;
  const BYTE * const mflimit = iend - MINMATCH;
  BYTE *op = (BYTE*) dst;

  compress_stats compressStats = { 0 };

  U32 hashTable[LDM_HASHTABLESIZE_U32];
  memset(hashTable, 0, sizeof(hashTable));

  const BYTE *anchor = (const BYTE *)src;
//  struct LDM_cctx cctx;
  size_t output_size = 0;

  U32 forwardH;

  /* Hash first byte: put into hash table */

  LDM_put_position(ip, hashTable, istart);
  const BYTE *lastHash = ip;
  ip++;
  forwardH = LDM_hash_position(ip);

  //TODO Loop terminates before ip>=ilimit.
  while (ip < ilimit) {
    const BYTE *match;
    BYTE *token;

    /* Find a match */
    {
      const BYTE *forwardIp = ip;
      unsigned step = 1;

      do {
        U32 const h = forwardH;
        ip = forwardIp;
        forwardIp += step;

        if (forwardIp > mflimit) {
          goto _last_literals;
        }

        match = LDM_get_position_on_hash(h, hashTable, istart);

        forwardH = LDM_hash_position(forwardIp);
        LDM_put_position_on_hash(ip, h, hashTable, istart);
        lastHash = ip;
      } while (ip - match > WINDOW_SIZE ||
               LDM_read64(match) != LDM_read64(ip));
    }
    compressStats.num_matches++;

    /* Catchup: look back to extend match from found match */
    while (ip > anchor && match > istart && ip[-1] == match[-1]) {
      ip--;
      match--;
    }

    /* Encode literals */
    {
      unsigned const litLength = (unsigned)(ip - anchor);
      token = op++;

      compressStats.total_literal_length += litLength;

#ifdef LDM_DEBUG
      printf("Cur position: %zu\n", anchor - istart);
      printf("LitLength %zu. (Match offset). %zu\n", litLength, ip - match);
#endif

      if (litLength >= RUN_MASK) {
        int len = (int)litLength - RUN_MASK;
        *token = (RUN_MASK << ML_BITS);
        for (; len >= 255; len -= 255) {
          *op++ = 255;
        }
        *op++ = (BYTE)len;
      } else {
        *token = (BYTE)(litLength << ML_BITS);
      }
#ifdef LDM_DEBUG
      printf("Literals ");
      fwrite(anchor, litLength, 1, stdout);
      printf("\n");
#endif
      memcpy(op, anchor, litLength);
      op += litLength;
    }
_next_match:
    /* Encode offset */
    {
      /*
      LDM_writeLE16(op, ip-match);
      op += 2;
      */
      LDM_write32(op, ip - match);
      op += 4;
      compressStats.total_offset += (ip - match);
    }

    /* Encode Match Length */
    {
      unsigned matchCode;
      matchCode = LDM_count(ip + MINMATCH, match + MINMATCH,
                            matchlimit);
#ifdef LDM_DEBUG
      printf("Match length %zu\n", matchCode + MINMATCH);
      fwrite(ip, MINMATCH + matchCode, 1, stdout);
      printf("\n");
#endif
      compressStats.total_match_length += matchCode + MINMATCH;
      unsigned ctr = 1;
      ip++;
      for (; ctr < MINMATCH + matchCode; ip++, ctr++) {
        LDM_put_position(ip, hashTable, istart);
      }
//      ip += MINMATCH + matchCode;
      if (matchCode >= ML_MASK) {
        *token += ML_MASK;
        matchCode -= ML_MASK;
        LDM_write32(op, 0xFFFFFFFF);
        while (matchCode >= 4*0xFF) {
          op += 4;
          LDM_write32(op, 0xffffffff);
          matchCode -= 4*0xFF;
        }
        op += matchCode / 255;
        *op++ = (BYTE)(matchCode % 255);
      } else {
        *token += (BYTE)(matchCode);
      }
#ifdef LDM_DEBUG
      printf("\n");

#endif
    }

    anchor = ip;

    LDM_put_position(ip, hashTable, istart);
    forwardH = LDM_hash_position(++ip);
    lastHash = ip;
  }
_last_literals:
    /* Encode last literals */
  {
    size_t const lastRun = (size_t)(iend - anchor);
    if (lastRun >= RUN_MASK) {
      size_t accumulator = lastRun - RUN_MASK;
      *op++ = RUN_MASK << ML_BITS;
      for(; accumulator >= 255; accumulator -= 255) {
        *op++ = 255;
      }
      *op++ = (BYTE)accumulator;
    } else {
      *op++ = (BYTE)(lastRun << ML_BITS);
    }
    memcpy(op, anchor, lastRun);
    op += lastRun;
  }
  LDM_printCompressStats(&compressStats);
  return (op - (BYTE *)dst);
}

typedef struct LDM_DCtx {
  const BYTE * const ibase;   /* Pointer to base of input */
  const BYTE *ip;             /* Pointer to current input position */
  const BYTE *iend;           /* End of source */
  BYTE *op;                   /* Pointer to output */
  const BYTE * const oend;    /* Pointer to end of output */

} LDM_DCtx;

size_t LDM_decompress(const void *src, size_t compressed_size,
                      void *dst, size_t max_decompressed_size) {
  const BYTE *ip = (const BYTE *)src;
  const BYTE * const iend = ip + compressed_size;
  BYTE *op = (BYTE *)dst;
  BYTE * const oend = op + max_decompressed_size;
  BYTE *cpy;

  while (ip < iend) {
    size_t length;
    const BYTE *match;
    size_t offset;

    /* get literal length */
    unsigned const token = *ip++;
    if ((length=(token >> ML_BITS)) == RUN_MASK) {
      unsigned s;
      do {
        s = *ip++;
        length += s;
      } while (s == 255);
    }
#ifdef LDM_DEBUG
    printf("Literal length: %zu\n", length);
#endif

    /* copy literals */
    cpy = op + length;
#ifdef LDM_DEBUG
    printf("Literals ");
    fwrite(ip, length, 1, stdout);
    printf("\n");
#endif
    memcpy(op, ip, length);
    ip += length;
    op = cpy;

    /* get offset */
    /*
    offset = LDM_readLE16(ip);
    ip += 2;
    */
    offset = LDM_read32(ip);
    ip += 4;
#ifdef LDM_DEBUG
    printf("Offset: %zu\n", offset);
#endif
    match = op - offset;
 //   LDM_write32(op, (U32)offset);

    /* get matchlength */
    length = token & ML_MASK;
    if (length == ML_MASK) {
      unsigned s;
      do {
        s = *ip++;
        length += s;
      } while (s == 255);
    }
    length += MINMATCH;
#ifdef LDM_DEBUG
    printf("Match length: %zu\n", length);
#endif
    /* copy match */
    cpy = op + length;

    // Inefficient for now
    while (match < cpy - offset && op < oend) {
      *op++ = *match++;
    }
  }
  return op - (BYTE *)dst;
}


