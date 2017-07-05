#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "ldm.h"

typedef  uint8_t BYTE;
typedef uint16_t U16;
typedef uint32_t U32;
typedef  int32_t S32;
typedef uint64_t U64;

typedef uint64_t tag;

struct hash_entry {
  U64 offset;
  tag t;
};

size_t LDM_compress(const char *source, char *dest, size_t source_size, size_t max_dest_size) {
  // max_dest_size >= source_size


  /**
   * Loop:
   *  Find match at position k (hash next n bytes, rolling hash)
   *  Compute match length
   *  Output literal length: k (sequences of 4 + (k-4) bytes)
   *  Output match length
   *  Output literals
   *  Output offset
   */

  memcpy(dest, source, source_size);
  return source_size;
}

size_t LDM_decompress(const char *source, char *dest, size_t compressed_size, size_t max_decompressed_size) {
  memcpy(dest, source, compressed_size);
  return compressed_size;
}


