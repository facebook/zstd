#ifndef LDM_H
#define LDM_H

#include "mem.h"    // from /lib/common/mem.h

//#include "ldm_params.h"

// =============================================================================
// Modify the parameters in ldm_params.h if "ldm_params.h" is included.
// Otherwise, modify the parameters here.
// =============================================================================

#ifndef LDM_PARAMS_H
  // Defines the size of the hash table.
  // Note that this is not the number of buckets.
  // Currently this should be less than WINDOW_SIZE_LOG + 4.
  #define LDM_MEMORY_USAGE 23

  // The number of entries in a hash bucket.
  #define HASH_BUCKET_SIZE_LOG 3 // The maximum is 4 for now.

  // Defines the lag in inserting elements into the hash table.
  #define LDM_LAG 0

  // The maximum window size when searching for matches.
  // The maximum value is 30
  #define LDM_WINDOW_SIZE_LOG 28

  // The minimum match length.
  // This should be a multiple of four.
  #define LDM_MIN_MATCH_LENGTH 64

  // If INSERT_BY_TAG, insert entries into the hash table as a function of the
  // hash. Certain hashes will not be inserted.
  //
  // Otherwise, insert as a function of the position.
  #define INSERT_BY_TAG 1

  // Store a checksum with the hash table entries for faster comparison.
  // This halves the number of entries the hash table can contain.
  #define USE_CHECKSUM 1
#endif

// Output compression statistics.
#define COMPUTE_STATS

// Output the configuration.
#define OUTPUT_CONFIGURATION

// If defined, forces the probability of insertion to be approximately
// one per (1 << HASH_ONLY_EVERY_LOG). If not defined, the probability will be
// calculated based on the memory usage and window size for "even" insertion
// throughout the window.

// #define HASH_ONLY_EVERY_LOG 8

// =============================================================================

// The number of bytes storing the compressed and decompressed size
// in the header.
#define LDM_COMPRESSED_SIZE 8
#define LDM_DECOMPRESSED_SIZE 8
#define LDM_HEADER_SIZE ((LDM_COMPRESSED_SIZE)+(LDM_DECOMPRESSED_SIZE))

#define ML_BITS 4
#define ML_MASK ((1U<<ML_BITS)-1)
#define RUN_BITS (8-ML_BITS)
#define RUN_MASK ((1U<<RUN_BITS)-1)

// The number of bytes storing the offset.
#define LDM_OFFSET_SIZE 4

#define LDM_WINDOW_SIZE (1 << (LDM_WINDOW_SIZE_LOG))

// TODO: Match lengths that are too small do not use the hash table efficiently.
// There should be a minimum hash length given the hash table size.
#define LDM_HASH_LENGTH LDM_MIN_MATCH_LENGTH

typedef struct LDM_compressStats LDM_compressStats;
typedef struct LDM_CCtx LDM_CCtx;
typedef struct LDM_DCtx LDM_DCtx;

/**
 *  Compresses src into dst.
 *  Returns the compressed size if successful, 0 otherwise.
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
 *  The last sequence is incomplete and stops right after the literals.
 */
size_t LDM_compress(const void *src, size_t srcSize,
                    void *dst, size_t maxDstSize);

/**
 * Initialize the compression context.
 *
 * Allocates memory for the hash table.
 *
 * Returns 0 if successful, 1 otherwise.
 */
size_t LDM_initializeCCtx(LDM_CCtx *cctx,
                          const void *src, size_t srcSize,
                          void *dst, size_t maxDstSize);

/**
 * Frees up memory allocated in LDM_initializeCCtx().
 */
void LDM_destroyCCtx(LDM_CCtx *cctx);

/**
 * Prints the distribution of offsets in the hash table.
 *
 * The offsets are defined as the distance of the hash table entry from the
 * current input position of the cctx.
 */
void LDM_outputHashTableOffsetHistogram(const LDM_CCtx *cctx);

/**
 * Outputs compression statistics to stdout.
 */
void LDM_printCompressStats(const LDM_compressStats *stats);

/**
 * Encode the literal length followed by the literals.
 *
 * The literal length is written to the upper four bits of pToken, with
 * additional bytes written to the output as needed (see lz4).
 *
 * This is followed by literalLength bytes corresponding to the literals.
 */
void LDM_encodeLiteralLengthAndLiterals(LDM_CCtx *cctx, BYTE *pToken,
                                        const U64 literalLength);

/**
 * Write current block (literals, literal length, match offset,
 * match length).
 */
void LDM_outputBlock(LDM_CCtx *cctx,
                     const U64 literalLength,
                     const U32 offset,
                     const U64 matchLength);

/**
 * Decompresses src into dst.
 *
 * Note: assumes src does not have a header.
 */
size_t LDM_decompress(const void *src, size_t srcSize,
                      void *dst, size_t maxDstSize);

/**
 * Initialize the decompression context.
 */
void LDM_initializeDCtx(LDM_DCtx *dctx,
                        const void *src, size_t compressedSize,
                        void *dst, size_t maxDecompressedSize);

/**
 * Reads the header from src and writes the compressed size and
 * decompressed size into compressedSize and decompressedSize respectively.
 *
 * NB: LDM_compress and LDM_decompress currently do not add/read headers.
 */
void LDM_readHeader(const void *src, U64 *compressedSize,
                    U64 *decompressedSize);

/**
 * Write the compressed and decompressed size.
 */
void LDM_writeHeader(void *memPtr, U64 compressedSize,
                     U64 decompressedSize);

/**
 * Output the configuration used.
 */
void LDM_outputConfiguration(void);

#endif /* LDM_H */
