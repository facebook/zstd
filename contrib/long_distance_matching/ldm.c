#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "ldm.h"

#define LDM_MEMORY_USAGE 14
#define LDM_HASHLOG (LDM_MEMORY_USAGE-2)
#define LDM_HASHTABLESIZE (1 << (LDM_MEMORY_USAGE))
#define LDM_HASHTABLESIZE_U32 ((LDM_HASHTABLESIZE) >> 2)
#define LDM_HASH_SIZE_U32 (1 << (LDM_HASHLOG))

#define WINDOW_SIZE (1 << 20)
#define MAX_WINDOW_SIZE 31
#define HASH_SIZE 4
#define MINMATCH 4

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

static unsigned LDM_isLittleEndian(void)
{
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

static void LDM_write32(void *memPtr, U32 value) {
  memcpy(memPtr, &value, sizeof(value));
}

static void LDM_write16(void *memPtr, U16 value) {
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

static void LDM_wild_copy(void *dstPtr, const void *srcPtr, void *dstEnd) {
  BYTE *d = (BYTE *)dstPtr;
  const BYTE *s = (const BYTE *)srcPtr;
  BYTE * const e = (BYTE *)dstEnd;

  do {
    LDM_copy8(d, s);
    d += 8;
    s += 8;
  } while (d < e);
}

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
//  printf("Hashing: %zu\n", p - srcBase);
  U32 *hashTable = (U32 *) tableBase;
  hashTable[h] = (U32)(p - srcBase);
}

static void LDM_put_position(const BYTE *p, void *tableBase,
                             const BYTE *srcBase) {
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

void LDM_read_header(void const *source, size_t *compressed_size,
                     size_t *decompressed_size) {
  const U32 *ip = (const U32 *)source;
  *compressed_size = *ip++;
  *decompressed_size = *ip;
}

size_t LDM_compress(void const *source, void *dest, size_t source_size,
                    size_t max_dest_size) {
  const BYTE * const istart = (const BYTE*)source;
  const BYTE *ip = istart;
  const BYTE * const iend = istart + source_size;
  const BYTE *ilimit = iend - HASH_SIZE;
  const BYTE * const matchlimit = iend - HASH_SIZE;
  const BYTE * const mflimit = iend - MINMATCH;
  BYTE *op = (BYTE*) dest;
  U32 hashTable[LDM_HASHTABLESIZE_U32];
  memset(hashTable, 0, sizeof(hashTable));

  const BYTE *anchor = (const BYTE *)source;
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

    // TODO catchup
    while (ip > anchor && match > istart && ip[-1] == match[-1]) {
      ip--;
      match--;
    }

    /* Encode literals */
    {
      unsigned const litLength = (unsigned)(ip - anchor);
      token = op++;

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
      //LDM_wild_copy(op, anchor, op + litLength);
      op += litLength;
    }
_next_match:
    /* Encode offset */
    {
      LDM_write32(op, ip - match);
      op += 4;
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
  return (op - (BYTE *)dest);
}

size_t LDM_decompress(void const *source, void *dest, size_t compressed_size,
                      size_t max_decompressed_size) {
  const BYTE *ip = (const BYTE *)source;
  const BYTE * const iend = ip + compressed_size;
  BYTE *op = (BYTE *)dest;
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
//    LDM_wild_copy(op, ip, cpy);
    ip += length;
    op = cpy;

    /* get offset */
    offset = LDM_read32(ip);

#ifdef LDM_DEBUG
    printf("Offset: %zu\n", offset);
#endif
    ip += 4;
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
//  memcpy(dest, source, compressed_size);
  return op - (BYTE *)dest;
}


