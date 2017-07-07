#ifndef LDM_H
#define LDM_H

#include <stddef.h>   /* size_t */

#define LDM_COMPRESS_SIZE 4
#define LDM_DECOMPRESS_SIZE 4
#define LDM_HEADER_SIZE ((LDM_COMPRESS_SIZE)+(LDM_DECOMPRESS_SIZE))

size_t LDM_compress(void const *source, void *dest, size_t source_size,
                    size_t max_dest_size);

size_t LDM_decompress(void const *source, void *dest, size_t compressed_size,
                      size_t max_decompressed_size);

void LDM_read_header(void const *source, size_t *compressed_size,
                     size_t *decompressed_size);

#endif /* LDM_H */
