#ifndef LDM_H
#define LDM_H

#include <stddef.h>   /* size_t */

size_t LDM_compress(const char *source, char *dest, size_t source_size, size_t max_dest_size);

size_t LDM_decompress(const char *source, char *dest, size_t compressed_size, size_t max_decompressed_size);

#endif /* LDM_H */
