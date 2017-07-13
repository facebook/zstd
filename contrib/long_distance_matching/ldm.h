#ifndef LDM_H
#define LDM_H

#include <stddef.h>   /* size_t */

#include "mem.h"    // from /lib/common/mem.h

#define LDM_COMPRESS_SIZE 8
#define LDM_DECOMPRESS_SIZE 8
#define LDM_HEADER_SIZE ((LDM_COMPRESS_SIZE)+(LDM_DECOMPRESS_SIZE))

typedef U32 offset_t;
typedef U32 hash_t;
typedef struct LDM_hashEntry LDM_hashEntry;
typedef struct LDM_compressStats LDM_compressStats;
typedef struct LDM_CCtx LDM_CCtx;

/**
 *  Compresses src into dst.
 *
 *  NB: This currently ignores maxDstSize and assumes enough space is available.
 *
 *  Block format (see lz4 documentation for more information):
 *  github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md
 *
 *  A block is composed of sequences. Each sequence begins with a token, which
 *  is a one-byte value separated into two 4-bit fields.
 *
 *  The first field uses the four high bits of the token and encodes the literal
 *  length. If the field value is 0, there is no literal. If it is 15,
 *  additional bytes are added (each ranging from 0 to 255) to the previous
 *  value to produce a total length.
 *
 *  Following the token and optional length bytes are the literals.
 *
 *  Next are the 4 bytes representing the offset of the match (2 in lz4),
 *  representing the position to copy the literals.
 *
 *  The lower four bits of the token encode the match length. With additional
 *  bytes added similarly to the additional literal length bytes after the offset.
 *
 *  The last sequence is incomplete and stops right after the lieterals.
 *
 */
size_t LDM_compress(const void *src, size_t srcSize,
                    void *dst, size_t maxDstSize);

size_t LDM_decompress(const void *src, size_t srcSize,
                      void *dst, size_t maxDstSize);

/**
 * Reads the header from src and writes the compressed size and
 * decompressed size into compressSize and decompressSize respectively.
 *
 * NB: LDM_compress and LDM_decompress currently do not add/read headers.
 */
void LDM_readHeader(const void *src, U64 *compressSize,
                    U64 *decompressSize);

void LDM_test(void);

#endif /* LDM_H */
