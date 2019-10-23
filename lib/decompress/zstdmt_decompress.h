#ifndef ZSTDMT_DECOMPRESS_H
#define ZSTDMT_DECOMPRESS_H

#include "zstd.h"
#include "zstd_decompress_internal.h"
#include "zstd_internal.h"
#include <stddef.h>

size_t ZSTDMT_decompress(void *dst, size_t dstSize, const void *src,
                         size_t srcSize);

#endif /* ZSTDMT_DECOMPRESS_H */
