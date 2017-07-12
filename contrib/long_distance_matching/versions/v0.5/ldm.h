#ifndef LDM_H
#define LDM_H

#include <stddef.h>   /* size_t */

#define LDM_COMPRESS_SIZE 4
#define LDM_DECOMPRESS_SIZE 4
#define LDM_HEADER_SIZE ((LDM_COMPRESS_SIZE)+(LDM_DECOMPRESS_SIZE))

size_t LDM_compress(const void *src, size_t srcSize,
                    void *dst, size_t maxDstSize);

size_t LDM_decompress(const void *src, size_t srcSize,
                      void *dst, size_t maxDstSize);

/**
 * Reads the header from src and writes the compressed size and
 * decompressed size into compressSize and decompressSize respectively.
 */
void LDM_readHeader(const void *src, size_t *compressSize,
                    size_t *decompressSize);

void LDM_test(const void *src, size_t srcSize,
              void *dst, size_t maxDstSize);

#endif /* LDM_H */
