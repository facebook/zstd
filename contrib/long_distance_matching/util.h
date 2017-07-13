#ifndef LDM_UTIL_H
#define LDM_UTIL_H

unsigned LDM_isLittleEndian(void);

uint16_t LDM_read16(const void *memPtr);

uint16_t LDM_readLE16(const void *memPtr);

void LDM_write16(void *memPtr, uint16_t value);

void LDM_write32(void *memPtr, uint32_t value);

void LDM_writeLE16(void *memPtr, uint16_t value);

uint32_t LDM_read32(const void *ptr);

uint64_t LDM_read64(const void *ptr);

void LDM_copy8(void *dst, const void *src);

uint8_t LDM_readByte(const void *ptr);

void LDM_write64(void *memPtr, uint64_t value);


#endif /* LDM_UTIL_H */
