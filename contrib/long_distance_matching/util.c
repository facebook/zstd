#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "util.h"

typedef  uint8_t BYTE;
typedef uint16_t U16;
typedef uint32_t U32;
typedef  int32_t S32;
typedef uint64_t U64;

unsigned LDM_isLittleEndian(void) {
    const union { U32 u; BYTE c[4]; } one = { 1 };
    return one.c[0];
}

U16 LDM_read16(const void *memPtr) {
  U16 val;
  memcpy(&val, memPtr, sizeof(val));
  return val;
}

U16 LDM_readLE16(const void *memPtr) {
  if (LDM_isLittleEndian()) {
    return LDM_read16(memPtr);
  } else {
    const BYTE *p = (const BYTE *)memPtr;
    return (U16)((U16)p[0] + (p[1] << 8));
  }
}

void LDM_write16(void *memPtr, U16 value){
  memcpy(memPtr, &value, sizeof(value));
}

void LDM_write32(void *memPtr, U32 value) {
  memcpy(memPtr, &value, sizeof(value));
}

void LDM_writeLE16(void *memPtr, U16 value) {
  if (LDM_isLittleEndian()) {
    LDM_write16(memPtr, value);
  } else {
    BYTE* p = (BYTE *)memPtr;
    p[0] = (BYTE) value;
    p[1] = (BYTE)(value>>8);
  }
}

U32 LDM_read32(const void *ptr) {
  return *(const U32 *)ptr;
}

U64 LDM_read64(const void *ptr) {
  return *(const U64 *)ptr;
}

void LDM_copy8(void *dst, const void *src) {
  memcpy(dst, src, 8);
}


