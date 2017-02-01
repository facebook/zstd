/*
 * Copyright (c) 2017-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

/// Zstandard educational decoder implementation
/// See https://github.com/facebook/zstd/blob/dev/doc/zstd_compression_format.md

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// Zstandard decompression functions.
/// `dst` must point to a space at least as large as the reconstructed output.
size_t ZSTD_decompress(void *const dst, const size_t dst_len,
                       const void *const src, const size_t src_len);
/// If `dict != NULL` and `dict_len >= 8`, does the same thing as
/// `ZSTD_decompress` but uses the provided dict
size_t ZSTD_decompress_with_dict(void *const dst, const size_t dst_len,
                                 const void *const src, const size_t src_len,
                                 const void *const dict, const size_t dict_len);

/// Get the decompressed size of an input stream so memory can be allocated in
/// advance
size_t ZSTD_get_decompressed_size(const void *const src, const size_t src_len);

/******* UTILITY MACROS AND TYPES *********************************************/
// Max block size decompressed size is 128 KB and literal blocks must be smaller
// than that
#define MAX_LITERALS_SIZE ((size_t)128 * 1024)

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define ERROR(s)                                                               \
    do {                                                                       \
        fprintf(stderr, "Error: %s\n", s);                                     \
        exit(1);                                                               \
    } while (0)
#define INP_SIZE()                                                             \
    ERROR("Input buffer smaller than it should be or input is "                \
          "corrupted")
#define OUT_SIZE() ERROR("Output buffer too small for output")
#define CORRUPTION() ERROR("Corruption detected while decompressing")
#define BAD_ALLOC() ERROR("Memory allocation error")

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
/******* END UTILITY MACROS AND TYPES *****************************************/

/******* IMPLEMENTATION PRIMITIVE PROTOTYPES **********************************/
/// The implementations for these functions can be found at the bottom of this
/// file.  They implement low-level functionality needed for the higher level
/// decompression functions.

/*** BITSTREAM OPERATIONS *************/
/// Read `num` bits (up to 64) from `src + offset`, where `offset` is in bits
static inline u64 read_bits_LE(const u8 *src, const int num,
                               const size_t offset);

/// Read bits from the end of a HUF or FSE bitstream.  `offset` is in bits, so
/// it updates `offset` to `offset - bits`, and then reads `bits` bits from
/// `src + offset`.  If the offset becomes negative, the extra bits at the
/// bottom are filled in with `0` bits instead of reading from before `src`.
static inline u64 STREAM_read_bits(const u8 *src, const int bits,
                                   i64 *const offset);
/*** END BITSTREAM OPERATIONS *********/

/*** BIT COUNTING OPERATIONS **********/
/// Returns `x`, where `2^x` is the largest power of 2 less than or equal to
/// `num`, or `-1` if `num == 0`.
static inline int log2inf(const u64 num);
/*** END BIT COUNTING OPERATIONS ******/

/*** HUFFMAN PRIMITIVES ***************/
// Table decode method uses exponential memory, so we need to limit depth
#define HUF_MAX_BITS (16)

// Limit the maximum number of symbols to 256 so we can store a symbol in a byte
#define HUF_MAX_SYMBS (256)

/// Structure containing all tables necessary for efficient Huffman decoding
typedef struct {
    u8 *symbols;
    u8 *num_bits;
    int max_bits;
} HUF_dtable;

/// Decode a single symbol and read in enough bits to refresh the state
static inline u8 HUF_decode_symbol(const HUF_dtable *const dtable,
                                   u16 *const state, const u8 *const src,
                                   i64 *const offset);
/// Read in a full state's worth of bits to initialize it
static inline void HUF_init_state(const HUF_dtable *const dtable,
                                  u16 *const state, const u8 *const src,
                                  i64 *const offset);

/// Decompresses a single Huffman stream, returns the number of bytes decoded.
/// `src_len` must be the exact length of the Huffman-coded block.
static size_t HUF_decompress_1stream(const HUF_dtable *const dtable, u8 *dst,
                                     const size_t dst_len, const u8 *src,
                                     size_t src_len);
/// Same as previous but decodes 4 streams, formatted as in the Zstandard
/// specification.
/// `src_len` must be the exact length of the Huffman-coded block.
static size_t HUF_decompress_4stream(const HUF_dtable *const dtable, u8 *dst,
                                     const size_t dst_len, const u8 *const src,
                                     const size_t src_len);

/// Initialize a Huffman decoding table using the table of bit counts provided
static void HUF_init_dtable(HUF_dtable *const table, const u8 *const bits,
                            const int num_symbs);
/// Initialize a Huffman decoding table using the table of weights provided
/// Weights follow the definition provided in the Zstandard specification
static void HUF_init_dtable_usingweights(HUF_dtable *const table,
                                         const u8 *const weights,
                                         const int num_symbs);

/// Free the malloc'ed parts of a decoding table
static void HUF_free_dtable(HUF_dtable *const dtable);

/// Deep copy a decoding table, so that it can be used and free'd without
/// impacting the source table.
static void HUF_copy_dtable(HUF_dtable *const dst, const HUF_dtable *const src);
/*** END HUFFMAN PRIMITIVES ***********/

/*** FSE PRIMITIVES *******************/
/// For more description of FSE see
/// https://github.com/Cyan4973/FiniteStateEntropy/

// FSE table decoding uses exponential memory, so limit the maximum accuracy
#define FSE_MAX_ACCURACY_LOG (15)
// Limit the maximum number of symbols so they can be stored in a single byte
#define FSE_MAX_SYMBS (256)

/// The tables needed to decode FSE encoded streams
typedef struct {
    u8 *symbols;
    u8 *num_bits;
    u16 *new_state_base;
    int accuracy_log;
} FSE_dtable;

/// Return the symbol for the current state
static inline u8 FSE_peek_symbol(const FSE_dtable *const dtable,
                                 const u16 state);
/// Read the number of bits necessary to update state, update, and shift offset
/// back to reflect the bits read
static inline void FSE_update_state(const FSE_dtable *const dtable,
                                    u16 *const state, const u8 *const src,
                                    i64 *const offset);

/// Combine peek and update: decode a symbol and update the state
static inline u8 FSE_decode_symbol(const FSE_dtable *const dtable,
                                   u16 *const state, const u8 *const src,
                                   i64 *const offset);

/// Read bits from the stream to initialize the state and shift offset back
static inline void FSE_init_state(const FSE_dtable *const dtable,
                                  u16 *const state, const u8 *const src,
                                  i64 *const offset);

/// Decompress two interleaved bitstreams (e.g. compressed Huffman weights)
/// using an FSE decoding table.  `src_len` must be the exact length of the
/// block.
static size_t FSE_decompress_interleaved2(const FSE_dtable *const dtable,
                                          u8 *dst, const size_t dst_len,
                                          const u8 *const src,
                                          const size_t src_len);

/// Initialize a decoding table using normalized frequencies.
static void FSE_init_dtable(FSE_dtable *const dtable,
                            const i16 *const norm_freqs, const int num_symbs,
                            const int accuracy_log);

/// Decode an FSE header as defined in the Zstandard format specification and
/// use the decoded frequencies to initialize a decoding table.
static size_t FSE_decode_header(FSE_dtable *const dtable, const u8 *const src,
                                const size_t src_len,
                                const int max_accuracy_log);

/// Initialize an FSE table that will always return the same symbol and consume
/// 0 bits per symbol, to be used for RLE mode in sequence commands
static void FSE_init_dtable_rle(FSE_dtable *const dtable, const u8 symb);

/// Free the malloc'ed parts of a decoding table
static void FSE_free_dtable(FSE_dtable *const dtable);

/// Deep copy a decoding table, so that it can be used and free'd without
/// impacting the source table.
static void FSE_copy_dtable(FSE_dtable *const dst, const FSE_dtable *const src);
/*** END FSE PRIMITIVES ***************/

/******* END IMPLEMENTATION PRIMITIVE PROTOTYPES ******************************/

/******* ZSTD HELPER STRUCTS AND PROTOTYPES ***********************************/

/// Input and output pointers to allow them to be advanced by
/// functions that consume input/produce output
typedef struct {
    u8 *dst;
    size_t dst_len;

    const u8 *src;
    size_t src_len;
} io_streams_t;

/// A small structure that can be reused in various places that need to access
/// frame header information
typedef struct {
    // The size of window that we need to be able to contiguously store for
    // references
    size_t window_size;
    // The total output size of this compressed frame
    size_t frame_content_size;

    // The dictionary id if this frame uses one
    u32 dictionary_id;

    // Whether or not the content of this frame has a checksum
    int content_checksum_flag;
    // Whether or not the output for this frame is in a single segment
    int single_segment_flag;

    // The size in bytes of this header
    int header_size;
} frame_header_t;

/// The context needed to decode blocks in a frame
typedef struct {
    frame_header_t header;

    // The total amount of data available for backreferences, to determine if an
    // offset too large to be correct
    size_t current_total_output;

    const u8 *dict_content;
    size_t dict_content_len;

    // Entropy encoding tables so they can be repeated by future blocks instead
    // of retransmitting
    HUF_dtable literals_dtable;
    FSE_dtable ll_dtable;
    FSE_dtable ml_dtable;
    FSE_dtable of_dtable;

    // The last 3 offsets for the special "repeat offsets".  Array size is 4 so
    // that previous_offsets[1] corresponds to the most recent offset
    u64 previous_offsets[4];
} frame_context_t;

/// The decoded contents of a dictionary so that it doesn't have to be repeated
/// for each frame that uses it
typedef struct {
    // Entropy tables
    HUF_dtable literals_dtable;
    FSE_dtable ll_dtable;
    FSE_dtable ml_dtable;
    FSE_dtable of_dtable;

    // Raw content for backreferences
    u8 *content;
    size_t content_size;

    // Offset history to prepopulate the frame's history
    u64 previous_offsets[4];

    u32 dictionary_id;
} dictionary_t;

/// A tuple containing the parts necessary to decode and execute a ZSTD sequence
/// command
typedef struct {
    u32 literal_length;
    u32 match_length;
    u32 offset;
} sequence_command_t;

/// The decoder works top-down, starting at the high level like Zstd frames, and
/// working down to lower more technical levels such as blocks, literals, and
/// sequences.  The high-level functions roughly follow the outline of the
/// format specification:
/// https://github.com/facebook/zstd/blob/dev/doc/zstd_compression_format.md

/// Before the implementation of each high-level function declared here, the
/// prototypes for their helper functions are defined and explained

/// Decode a single Zstd frame, or error if the input is not a valid frame.
/// Accepts a dict argument, which may be NULL indicating no dictionary.
/// See
/// https://github.com/facebook/zstd/blob/dev/doc/zstd_compression_format.md#frame-concatenation
static void decode_frame(io_streams_t *const streams,
                         const dictionary_t *const dict);

// Decode data in a compressed block
static void decompress_block(io_streams_t *const streams,
                             frame_context_t *const ctx,
                             const size_t block_len);

// Decode the literals section of a block
static size_t decode_literals(io_streams_t *const streams,
                              frame_context_t *const ctx, u8 **const literals);

// Decode the sequences part of a block
static size_t decode_sequences(frame_context_t *const ctx, const u8 *const src,
                               const size_t src_len,
                               sequence_command_t **const sequences);

// Execute the decoded sequences on the literals block
static void execute_sequences(io_streams_t *const streams,
                              frame_context_t *const ctx,
                              const sequence_command_t *const sequences,
                              const size_t num_sequences,
                              const u8 *literals,
                              size_t literals_len);

// Parse a provided dictionary blob for use in decompression
static void parse_dictionary(dictionary_t *const dict, const u8 *const src,
                             const size_t src_len);
static void free_dictionary(dictionary_t *const dict);
/******* END ZSTD HELPER STRUCTS AND PROTOTYPES *******************************/

size_t ZSTD_decompress(void *const dst, const size_t dst_len,
                       const void *const src, const size_t src_len) {
    return ZSTD_decompress_with_dict(dst, dst_len, src, src_len, NULL, 0);
}

size_t ZSTD_decompress_with_dict(void *const dst, const size_t dst_len,
                                 const void *const src, const size_t src_len,
                                 const void *const dict,
                                 const size_t dict_len) {
    dictionary_t parsed_dict;
    memset(&parsed_dict, 0, sizeof(dictionary_t));
    // dict_len < 8 is not a valid dictionary
    if (dict && dict_len > 8) {
        parse_dictionary(&parsed_dict, (const u8 *)dict, dict_len);
    }

    io_streams_t streams = {(u8 *)dst, dst_len, (const u8 *)src, src_len};
    while (streams.src_len > 0) {
        decode_frame(&streams, &parsed_dict);
    }

    free_dictionary(&parsed_dict);

    return streams.dst - (u8 *)dst;
}

/******* FRAME DECODING ******************************************************/

static void decode_data_frame(io_streams_t *const streams,
                              const dictionary_t *const dict);
static void init_frame_context(io_streams_t *const streams,
                               frame_context_t *const context,
                               const dictionary_t *const dict);
static void free_frame_context(frame_context_t *const context);
static void parse_frame_header(frame_header_t *const header,
                               const u8 *const src, const size_t src_len);
static void frame_context_apply_dict(frame_context_t *const ctx,
                                     const dictionary_t *const dict);

static void decompress_data(io_streams_t *const streams,
                            frame_context_t *const ctx);

static void decode_frame(io_streams_t *const streams,
                         const dictionary_t *const dict) {
    if (streams->src_len < 4) {
        INP_SIZE();
    }
    const u32 magic_number = read_bits_LE(streams->src, 32, 0);

    streams->src += 4;
    streams->src_len -= 4;
    if (magic_number >= 0x184D2A50U && magic_number <= 0x184D2A5F) {
        // skippable frame
        if (streams->src_len < 4) {
            INP_SIZE();
        }
        const size_t frame_size = read_bits_LE(streams->src, 32, 32);

        if (streams->src_len < 4 + frame_size) {
            INP_SIZE();
        }

        // skip over frame
        streams->src += 4 + frame_size;
        streams->src_len -= 4 + frame_size;
    } else if (magic_number == 0xFD2FB528U) {
        // ZSTD frame
        decode_data_frame(streams, dict);
    } else {
        // not a real frame
        ERROR("Invalid magic number");
    }
}

/// Decode a frame that contains compressed data.  Not all frames do as there
/// are skippable frames.
/// See
/// https://github.com/facebook/zstd/blob/dev/doc/zstd_compression_format.md#general-structure-of-zstandard-frame-format
static void decode_data_frame(io_streams_t *const streams,
                              const dictionary_t *const dict) {
    frame_context_t ctx;

    // Initialize the context that needs to be carried from block to block
    init_frame_context(streams, &ctx, dict);

    if (ctx.header.frame_content_size != 0 &&
        ctx.header.frame_content_size > streams->dst_len) {
        OUT_SIZE();
    }

    decompress_data(streams, &ctx);

    free_frame_context(&ctx);
}

/// Takes the information provided in the header and dictionary, and initializes
/// the context for this frame
static void init_frame_context(io_streams_t *const streams,
                               frame_context_t *const context,
                               const dictionary_t *const dict) {
    // Most fields in context are correct when initialized to 0
    memset(context, 0x00, sizeof(frame_context_t));

    // Parse data from the frame header
    parse_frame_header(&context->header, streams->src, streams->src_len);
    streams->src += context->header.header_size;
    streams->src_len -= context->header.header_size;

    // Set up the offset history for the repeat offset commands
    context->previous_offsets[1] = 1;
    context->previous_offsets[2] = 4;
    context->previous_offsets[3] = 8;

    // Apply details from the dict if it exists
    frame_context_apply_dict(context, dict);
}

static void free_frame_context(frame_context_t *const context) {
    HUF_free_dtable(&context->literals_dtable);

    FSE_free_dtable(&context->ll_dtable);
    FSE_free_dtable(&context->ml_dtable);
    FSE_free_dtable(&context->of_dtable);

    memset(context, 0, sizeof(frame_context_t));
}

static void parse_frame_header(frame_header_t *const header,
                               const u8 *const src, const size_t src_len) {
    if (src_len < 1) {
        INP_SIZE();
    }

    const u8 descriptor = read_bits_LE(src, 8, 0);

    // decode frame header descriptor into flags
    const u8 frame_content_size_flag = descriptor >> 6;
    const u8 single_segment_flag = (descriptor >> 5) & 1;
    const u8 reserved_bit = (descriptor >> 3) & 1;
    const u8 content_checksum_flag = (descriptor >> 2) & 1;
    const u8 dictionary_id_flag = descriptor & 3;

    if (reserved_bit != 0) {
        CORRUPTION();
    }

    int header_size = 1;

    header->single_segment_flag = single_segment_flag;
    header->content_checksum_flag = content_checksum_flag;

    // decode window size
    if (!single_segment_flag) {
        if (src_len < header_size + 1) {
            INP_SIZE();
        }

        // Use the algorithm from the specification to compute window size
        // https://github.com/facebook/zstd/blob/dev/doc/zstd_compression_format.md#window_descriptor
        u8 window_descriptor = src[header_size];
        u8 exponent = window_descriptor >> 3;
        u8 mantissa = window_descriptor & 7;

        size_t window_base = (size_t)1 << (10 + exponent);
        size_t window_add = (window_base / 8) * mantissa;
        header->window_size = window_base + window_add;

        header_size++;
    }

    // decode dictionary id if it exists
    if (dictionary_id_flag) {
        const int bytes_array[] = {0, 1, 2, 4};
        const int bytes = bytes_array[dictionary_id_flag];

        if (src_len < header_size + bytes) {
            INP_SIZE();
        }

        header->dictionary_id = read_bits_LE(src + header_size, bytes * 8, 0);

        header_size += bytes;
    } else {
        header->dictionary_id = 0;
    }

    // decode frame content size if it exists
    if (single_segment_flag || frame_content_size_flag) {
        // if frame_content_size_flag == 0 but single_segment_flag is set, we
        // still have a 1 byte field
        const int bytes_array[] = {1, 2, 4, 8};
        const int bytes = bytes_array[frame_content_size_flag];

        if (src_len < header_size + bytes) {
            INP_SIZE();
        }

        header->frame_content_size =
            read_bits_LE(src + header_size, bytes * 8, 0);
        if (bytes == 2) {
            header->frame_content_size += 256;
        }

        header_size += bytes;
    } else {
        header->frame_content_size = 0;
    }

    if (single_segment_flag) {
        // in this case the effective window size is frame_content_size this
        // impacts sequence decoding as we need to determine whether to fall
        // back to the dictionary or not on large offsets
        header->window_size = header->frame_content_size;
    }

    header->header_size = header_size;
}

/// A dictionary acts as initializing values for the frame context before
/// decompression, so we implement it by applying it's predetermined
/// tables and content to the context before beginning decompression
static void frame_context_apply_dict(frame_context_t *const ctx,
                                     const dictionary_t *const dict) {
    // If the content pointer is NULL then it must be an empty dict
    if (!dict || !dict->content)
        return;

    if (ctx->header.dictionary_id == 0 && dict->dictionary_id != 0) {
        // The dictionary is unneeded, and shouldn't be used as it may interfere
        // with the default offset history
        return;
    }

    // If the dictionary id is 0, it doesn't matter if we provide the wrong raw
    // content dict, it won't change anything
    if (ctx->header.dictionary_id != 0 &&
        ctx->header.dictionary_id != dict->dictionary_id) {
        ERROR("Wrong/no dictionary provided");
    }

    // Copy the pointer in so we can reference it in sequence execution
    ctx->dict_content = dict->content;
    ctx->dict_content_len = dict->content_size;

    // If it's a formatted dict copy the precomputed tables in so they can
    // be used in the table repeat modes
    if (dict->dictionary_id != 0) {
        // Deep copy the entropy tables so they can be freed independently of
        // the dictionary struct
        HUF_copy_dtable(&ctx->literals_dtable, &dict->literals_dtable);
        FSE_copy_dtable(&ctx->ll_dtable, &dict->ll_dtable);
        FSE_copy_dtable(&ctx->of_dtable, &dict->of_dtable);
        FSE_copy_dtable(&ctx->ml_dtable, &dict->ml_dtable);

        memcpy(ctx->previous_offsets, dict->previous_offsets,
               sizeof(ctx->previous_offsets));
    }
}

/// Decompress the data from a frame block by block
static void decompress_data(io_streams_t *const streams,
                            frame_context_t *const ctx) {
    int last_block = 0;
    do {
        if (streams->src_len < 3) {
            INP_SIZE();
        }
        // Parse the block header
        last_block = streams->src[0] & 1;
        const int block_type = (streams->src[0] >> 1) & 3;
        const size_t block_len = read_bits_LE(streams->src, 21, 3);

        streams->src += 3;
        streams->src_len -= 3;

        switch (block_type) {
        case 0: {
            // Raw, uncompressed block
            if (streams->src_len < block_len) {
                INP_SIZE();
            }
            if (streams->dst_len < block_len) {
                OUT_SIZE();
            }

            // Copy the raw data into the output
            memcpy(streams->dst, streams->src, block_len);

            streams->src += block_len;
            streams->src_len -= block_len;

            streams->dst += block_len;
            streams->dst_len -= block_len;

            ctx->current_total_output += block_len;
            break;
        }
        case 1: {
            // RLE block, repeat the first byte N times
            if (streams->src_len < 1) {
                INP_SIZE();
            }
            if (streams->dst_len < block_len) {
                OUT_SIZE();
            }

            // Copy `block_len` copies of `streams->src[0]` to the output
            memset(streams->dst, streams->src[0], block_len);

            streams->dst += block_len;
            streams->dst_len -= block_len;

            streams->src += 1;
            streams->src_len -= 1;

            ctx->current_total_output += block_len;
            break;
        }
        case 2:
            // Compressed block, this is mode complex
            decompress_block(streams, ctx, block_len);
            break;
        case 3:
            // Reserved block type
            CORRUPTION();
            break;
        }
    } while (!last_block);

    if (ctx->header.content_checksum_flag) {
        // This program does not support checking the checksum, so skip over it
        // if it's present
        if (streams->src_len < 4) {
            INP_SIZE();
        }
        streams->src += 4;
        streams->src_len -= 4;
    }
}
/******* END FRAME DECODING ***************************************************/

/******* BLOCK DECOMPRESSION **************************************************/
static void decompress_block(io_streams_t *const streams, frame_context_t *const ctx,
                             const size_t block_len) {
    if (streams->src_len < block_len) {
        INP_SIZE();
    }
    // We need this to determine how long the compressed literals block was
    const u8 *const end_of_block = streams->src + block_len;

    // Part 1: decode the literals block
    u8 *literals = NULL;
    const size_t literals_size = decode_literals(streams, ctx, &literals);

    // Part 2: decode the sequences block
    if (streams->src > end_of_block) {
        INP_SIZE();
    }
    const size_t sequences_size = end_of_block - streams->src;
    sequence_command_t *sequences = NULL;
    const size_t num_sequences =
        decode_sequences(ctx, streams->src, sequences_size, &sequences);

    streams->src += sequences_size;
    streams->src_len -= sequences_size;

    // Part 3: combine literals and sequence commands to generate output
    execute_sequences(streams, ctx, sequences, num_sequences, literals,
                      literals_size);
    free(literals);
    free(sequences);
}
/******* END BLOCK DECOMPRESSION **********************************************/

/******* LITERALS DECODING ****************************************************/
static size_t decode_literals_simple(io_streams_t *const streams,
                                     u8 **const literals, const int block_type,
                                     const int size_format);
static size_t decode_literals_compressed(io_streams_t *const streams,
                                         frame_context_t *const ctx,
                                         u8 **const literals,
                                         const int block_type,
                                         const int size_format);
static size_t decode_huf_table(const u8 *src, size_t src_len,
                               HUF_dtable *const dtable);
static size_t fse_decode_hufweights(const u8 *const src, const size_t src_len,
                                    u8 *const weights, int *const num_symbs,
                                    const size_t compressed_size);

static size_t decode_literals(io_streams_t *const streams,
                              frame_context_t *const ctx, u8 **const literals) {
    if (streams->src_len < 1) {
        INP_SIZE();
    }
    // Decode literals header
    int block_type = streams->src[0] & 3;
    int size_format = (streams->src[0] >> 2) & 3;

    if (block_type <= 1) {
        // Raw or RLE literals block
        return decode_literals_simple(streams, literals, block_type,
                                      size_format);
    } else {
        // Huffman compressed literals
        return decode_literals_compressed(streams, ctx, literals, block_type,
                                          size_format);
    }
}

/// Decodes literals blocks in raw or RLE form
static size_t decode_literals_simple(io_streams_t *const streams,
                                     u8 **const literals, const int block_type,
                                     const int size_format) {
    size_t size;
    switch (size_format) {
    // These cases are in the form X0
    // In this case, the X bit is actually part of the size field
    case 0:
    case 2:
        size = read_bits_LE(streams->src, 5, 3);
        streams->src += 1;
        streams->src_len -= 1;
        break;
    case 1:
        if (streams->src_len < 2) {
            INP_SIZE();
        }
        size = read_bits_LE(streams->src, 12, 4);
        streams->src += 2;
        streams->src_len -= 2;
        break;
    case 3:
        if (streams->src_len < 2) {
            INP_SIZE();
        }
        size = read_bits_LE(streams->src, 20, 4);
        streams->src += 3;
        streams->src_len -= 3;
        break;
    default:
        // Impossible
        size = -1;
    }

    if (size > MAX_LITERALS_SIZE) {
        CORRUPTION();
    }

    *literals = malloc(size);
    if (!*literals) {
        BAD_ALLOC();
    }

    switch (block_type) {
    case 0:
        // Raw data
        if (size > streams->src_len) {
            INP_SIZE();
        }
        memcpy(*literals, streams->src, size);
        streams->src += size;
        streams->src_len -= size;
        break;
    case 1:
        // Single repeated byte
        if (1 > streams->src_len) {
            INP_SIZE();
        }
        memset(*literals, streams->src[0], size);
        streams->src += 1;
        streams->src_len -= 1;
        break;
    }

    return size;
}

/// Decodes Huffman compressed literals
static size_t decode_literals_compressed(io_streams_t *const streams,
                                         frame_context_t *const ctx,
                                         u8 **const literals,
                                         const int block_type,
                                         const int size_format) {
    size_t regenerated_size, compressed_size;
    // Only size_format=0 has 1 stream, so default to 4
    int num_streams = 4;
    switch (size_format) {
    case 0:
        num_streams = 1;
    // Fall through as it has the same size format
    case 1:
        if (streams->src_len < 3) {
            INP_SIZE();
        }
        regenerated_size = read_bits_LE(streams->src, 10, 4);
        compressed_size = read_bits_LE(streams->src, 10, 14);
        streams->src += 3;
        streams->src_len -= 3;
        break;
    case 2:
        if (streams->src_len < 4) {
            INP_SIZE();
        }
        regenerated_size = read_bits_LE(streams->src, 14, 4);
        compressed_size = read_bits_LE(streams->src, 14, 18);
        streams->src += 4;
        streams->src_len -= 4;
        break;
    case 3:
        if (streams->src_len < 5) {
            INP_SIZE();
        }
        regenerated_size = read_bits_LE(streams->src, 18, 4);
        compressed_size = read_bits_LE(streams->src, 18, 22);
        streams->src += 5;
        streams->src_len -= 5;
        break;
    default:
        // Impossible
        compressed_size = regenerated_size = -1;
    }
    if (regenerated_size > MAX_LITERALS_SIZE ||
        compressed_size > regenerated_size) {
        CORRUPTION();
    }

    if (compressed_size > streams->src_len) {
        INP_SIZE();
    }

    *literals = malloc(regenerated_size);
    if (!*literals) {
        BAD_ALLOC();
    }

    if (block_type == 2) {
        // Decode provided Huffman table

        HUF_free_dtable(&ctx->literals_dtable);
        const size_t size = decode_huf_table(streams->src, compressed_size,
                                             &ctx->literals_dtable);
        streams->src += size;
        streams->src_len -= size;
        compressed_size -= size;
    } else {
        // If we're to repeat the previous Huffman table, make sure it exists
        if (!ctx->literals_dtable.symbols) {
            CORRUPTION();
        }
    }

    if (num_streams == 1) {
        HUF_decompress_1stream(&ctx->literals_dtable, *literals,
                               regenerated_size, streams->src, compressed_size);
    } else {
        HUF_decompress_4stream(&ctx->literals_dtable, *literals,
                               regenerated_size, streams->src, compressed_size);
    }
    streams->src += compressed_size;
    streams->src_len -= compressed_size;

    return regenerated_size;
}

// Decode the Huffman table description
static size_t decode_huf_table(const u8 *src, size_t src_len,
                               HUF_dtable *const dtable) {
    if (src_len < 1) {
        INP_SIZE();
    }

    const u8 *const osrc = src;

    const u8 header = src[0];
    u8 weights[HUF_MAX_SYMBS];
    memset(weights, 0, sizeof(weights));

    src++;
    src_len--;

    int num_symbs;

    if (header >= 128) {
        // Direct representation, read the weights out
        num_symbs = header - 127;
        const size_t bytes = (num_symbs + 1) / 2;

        if (bytes > src_len) {
            INP_SIZE();
        }

        for (int i = 0; i < num_symbs; i++) {
            // read_bits_LE isn't applicable here because the weights are order
            // reversed within each byte
            // https://github.com/facebook/zstd/blob/dev/doc/zstd_compression_format.md#huffman-tree-header
            if (i % 2 == 0) {
                weights[i] = src[i / 2] >> 4;
            } else {
                weights[i] = src[i / 2] & 0xf;
            }
        }

        src += bytes;
        src_len -= bytes;
    } else {
        // The weights are FSE encoded, decode them before we can construct the
        // table
        const size_t size =
            fse_decode_hufweights(src, src_len, weights, &num_symbs, header);
        src += size;
        src_len -= size;
    }

    // Construct the table using the decoded weights
    HUF_init_dtable_usingweights(dtable, weights, num_symbs);
    return src - osrc;
}

static size_t fse_decode_hufweights(const u8 *const src, const size_t src_len,
                                    u8 *const weights, int *const num_symbs,
                                    const size_t compressed_size) {
    const int MAX_ACCURACY_LOG = 7;

    FSE_dtable dtable;

    // Construct the FSE table
    const size_t read =
            FSE_decode_header(&dtable, src, src_len, MAX_ACCURACY_LOG);

    if (src_len < compressed_size) {
        INP_SIZE();
    }

    // Decode the weights
    *num_symbs = FSE_decompress_interleaved2(
        &dtable, weights, HUF_MAX_SYMBS, src + read, compressed_size - read);

    FSE_free_dtable(&dtable);

    return compressed_size;
}
/******* END LITERALS DECODING ************************************************/

/******* SEQUENCE DECODING ****************************************************/
/// The combination of FSE states needed to decode sequences
typedef struct {
    u16 ll_state, of_state, ml_state;
    FSE_dtable ll_table, of_table, ml_table;
} sequence_state_t;

/// Different modes to signal to decode_seq_tables what to do
typedef enum {
    seq_literal_length = 0,
    seq_offset = 1,
    seq_match_length = 2,
} seq_part_t;

typedef enum {
    seq_predefined = 0,
    seq_rle = 1,
    seq_fse = 2,
    seq_repeat = 3,
} seq_mode_t;

/// The predefined FSE distribution tables for `seq_predefined` mode
static const i16 SEQ_LITERAL_LENGTH_DEFAULT_DIST[36] = {
    4, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1,  1,  2,  2,
    2, 2, 2, 2, 2, 2, 2, 3, 2, 1, 1, 1, 1, 1, -1, -1, -1, -1};
static const i16 SEQ_OFFSET_DEFAULT_DIST[29] = {
    1, 1, 1, 1, 1, 1, 2, 2, 2, 1,  1,  1,  1,  1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1};
static const i16 SEQ_MATCH_LENGTH_DEFAULT_DIST[53] = {
    1, 4, 3, 2, 2, 2, 2, 2, 2, 1, 1,  1,  1,  1,  1,  1,  1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  1,  1,  1,  1,  1,  1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1};

/// The sequence decoding baseline and number of additional bits to read/add
/// https://github.com/facebook/zstd/blob/dev/doc/zstd_compression_format.md#the-codes-for-literals-lengths-match-lengths-and-offsets
static const u32 SEQ_LITERAL_LENGTH_BASELINES[36] = {
    0,  1,  2,   3,   4,   5,    6,    7,    8,    9,     10,    11,
    12, 13, 14,  15,  16,  18,   20,   22,   24,   28,    32,    40,
    48, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65538};
static const u8 SEQ_LITERAL_LENGTH_EXTRA_BITS[36] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  1,  1,
    1, 1, 2, 2, 3, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

static const u32 SEQ_MATCH_LENGTH_BASELINES[53] = {
    3,  4,   5,   6,   7,    8,    9,    10,   11,    12,    13,   14, 15, 16,
    17, 18,  19,  20,  21,   22,   23,   24,   25,    26,    27,   28, 29, 30,
    31, 32,  33,  34,  35,   37,   39,   41,   43,    47,    51,   59, 67, 83,
    99, 131, 259, 515, 1027, 2051, 4099, 8195, 16387, 32771, 65539};
static const u8 SEQ_MATCH_LENGTH_EXTRA_BITS[53] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  1,  1,  1, 1,
    2, 2, 3, 3, 4, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

/// Offset decoding is simpler so we just need a maximum code value
static const u8 SEQ_MAX_CODES[3] = {35, -1, 52};

static void decompress_sequences(frame_context_t *const ctx, const u8 *src,
                                 size_t src_len,
                                 sequence_command_t *const sequences,
                                 const size_t num_sequences);
static sequence_command_t decode_sequence(sequence_state_t *const state,
                                          const u8 *const src,
                                          i64 *const offset);
static size_t decode_seq_table(const u8 *src, size_t src_len,
                               FSE_dtable *const table, const seq_part_t type,
                               const seq_mode_t mode);

static size_t decode_sequences(frame_context_t *const ctx, const u8 *src,
                               size_t src_len,
                               sequence_command_t **const sequences) {
    size_t num_sequences;

    // Decode the sequence header and allocate space for the output
    if (src_len < 1) {
        INP_SIZE();
    }
    if (src[0] == 0) {
        *sequences = NULL;
        return 0;
    } else if (src[0] < 128) {
        num_sequences = src[0];
        src++;
        src_len--;
    } else if (src[0] < 255) {
        if (src_len < 2) {
            INP_SIZE();
        }
        num_sequences = ((src[0] - 128) << 8) + src[1];
        src += 2;
        src_len -= 2;
    } else {
        if (src_len < 3) {
            INP_SIZE();
        }
        num_sequences = src[1] + ((u64)src[2] << 8) + 0x7F00;
        src += 3;
        src_len -= 3;
    }

    *sequences = malloc(num_sequences * sizeof(sequence_command_t));
    if (!*sequences) {
        BAD_ALLOC();
    }

    decompress_sequences(ctx, src, src_len, *sequences, num_sequences);
    return num_sequences;
}

/// Decompress the FSE encoded sequence commands
static void decompress_sequences(frame_context_t *const ctx, const u8 *src,
                                 size_t src_len,
                                 sequence_command_t *const sequences,
                                 const size_t num_sequences) {
    if (src_len < 1) {
        INP_SIZE();
    }
    u8 compression_modes = src[0];
    src++;
    src_len--;

    if ((compression_modes & 3) != 0) {
        CORRUPTION();
    }

    {
        size_t read;
        // Update the tables we have stored in the context
        read = decode_seq_table(src, src_len, &ctx->ll_dtable,
                                seq_literal_length,
                                (compression_modes >> 6) & 3);
        src += read;
        src_len -= read;
    }

    {
        const size_t read =
                decode_seq_table(src, src_len, &ctx->of_dtable, seq_offset,
                                 (compression_modes >> 4) & 3);
        src += read;
        src_len -= read;
    }

    {
        const size_t read = decode_seq_table(src, src_len, &ctx->ml_dtable,
                                             seq_match_length,
                                             (compression_modes >> 2) & 3);
        src += read;
        src_len -= read;
    }

    // Check to make sure none of the tables are uninitialized
    if (!ctx->ll_dtable.symbols || !ctx->of_dtable.symbols ||
        !ctx->ml_dtable.symbols) {
        CORRUPTION();
    }

    sequence_state_t state;
    // Copy the context's tables into the local state
    memcpy(&state.ll_table, &ctx->ll_dtable, sizeof(FSE_dtable));
    memcpy(&state.of_table, &ctx->of_dtable, sizeof(FSE_dtable));
    memcpy(&state.ml_table, &ctx->ml_dtable, sizeof(FSE_dtable));

    const int padding = 8 - log2inf(src[src_len - 1]);
    i64 offset = src_len * 8 - padding;

    FSE_init_state(&state.ll_table, &state.ll_state, src, &offset);
    FSE_init_state(&state.of_table, &state.of_state, src, &offset);
    FSE_init_state(&state.ml_table, &state.ml_state, src, &offset);

    for (size_t i = 0; i < num_sequences; i++) {
        // Decode sequences one by one
        sequences[i] = decode_sequence(&state, src, &offset);
    }

    if (offset != 0) {
        CORRUPTION();
    }

    // Don't free our tables so they can be used in the next block
}

// Decode a single sequence and update the state
static sequence_command_t decode_sequence(sequence_state_t *const state,
                                          const u8 *const src,
                                          i64 *const offset) {
    // Decode symbols, but don't update states
    const u8 of_code = FSE_peek_symbol(&state->of_table, state->of_state);
    const u8 ll_code = FSE_peek_symbol(&state->ll_table, state->ll_state);
    const u8 ml_code = FSE_peek_symbol(&state->ml_table, state->ml_state);

    // Offset doesn't need a max value as it's not decoded using a table
    if (ll_code > SEQ_MAX_CODES[seq_literal_length] ||
        ml_code > SEQ_MAX_CODES[seq_match_length]) {
        CORRUPTION();
    }

    // Read the interleaved bits
    sequence_command_t seq;
    // Offset computation works differently
    seq.offset = ((u32)1 << of_code) + STREAM_read_bits(src, of_code, offset);
    seq.match_length =
        SEQ_MATCH_LENGTH_BASELINES[ml_code] +
        STREAM_read_bits(src, SEQ_MATCH_LENGTH_EXTRA_BITS[ml_code], offset);
    seq.literal_length =
        SEQ_LITERAL_LENGTH_BASELINES[ll_code] +
        STREAM_read_bits(src, SEQ_LITERAL_LENGTH_EXTRA_BITS[ll_code], offset);

    // If the stream is complete don't read bits to update state
    if (*offset != 0) {
        // Update state in the order specified in the specification
        FSE_update_state(&state->ll_table, &state->ll_state, src, offset);
        FSE_update_state(&state->ml_table, &state->ml_state, src, offset);
        FSE_update_state(&state->of_table, &state->of_state, src, offset);
    }

    return seq;
}

/// Given a sequence part and table mode, decode the FSE distribution
static size_t decode_seq_table(const u8 *src, size_t src_len,
                               FSE_dtable *const table, const seq_part_t type,
                               const seq_mode_t mode) {
    // Constant arrays indexed by seq_part_t
    const i16 *const default_distributions[] = {SEQ_LITERAL_LENGTH_DEFAULT_DIST,
                                                SEQ_OFFSET_DEFAULT_DIST,
                                                SEQ_MATCH_LENGTH_DEFAULT_DIST};
    const size_t default_distribution_lengths[] = {36, 29, 53};
    const size_t default_distribution_accuracies[] = {6, 5, 6};

    const size_t max_accuracies[] = {9, 8, 9};

    if (mode != seq_repeat) {
        // ree old one before overwriting
        FSE_free_dtable(table);
    }

    switch (mode) {
    case seq_predefined: {
        const i16 *distribution = default_distributions[type];
        const size_t symbs = default_distribution_lengths[type];
        const size_t accuracy_log = default_distribution_accuracies[type];

        FSE_init_dtable(table, distribution, symbs, accuracy_log);

        return 0;
    }
    case seq_rle: {
        if (src_len < 1) {
            INP_SIZE();
        }
        const u8 symb = src[0];
        src++;
        src_len--;
        FSE_init_dtable_rle(table, symb);

        return 1;
    }
    case seq_fse: {
        size_t read =
            FSE_decode_header(table, src, src_len, max_accuracies[type]);
        src += read;
        src_len -= read;

        return read;
    }
    case seq_repeat:
        // Don't have to do anything here as we're not changing the table
        return 0;
    default:
        // Impossible, as mode is from 0-3
        return -1;
    }
}
/******* END SEQUENCE DECODING ************************************************/

/******* SEQUENCE EXECUTION ***************************************************/
static void execute_sequences(io_streams_t *const streams,
                              frame_context_t *const ctx,
                              const sequence_command_t *const sequences,
                              const size_t num_sequences,
                              const u8 *literals,
                              size_t literals_len) {
    u64 *const offset_hist = ctx->previous_offsets;
    size_t total_output = ctx->current_total_output;

    for (size_t i = 0; i < num_sequences; i++) {
        const sequence_command_t seq = sequences[i];

        if (seq.literal_length > literals_len) {
            CORRUPTION();
        }

        if (streams->dst_len < seq.literal_length + seq.match_length) {
            OUT_SIZE();
        }
        // Copy literals to output
        memcpy(streams->dst, literals, seq.literal_length);

        literals += seq.literal_length;
        literals_len -= seq.literal_length;

        streams->dst += seq.literal_length;
        streams->dst_len -= seq.literal_length;

        total_output += seq.literal_length;

        size_t offset;

        // Offsets are special, we need to handle the repeat offsets
        if (seq.offset <= 3) {
            u32 idx = seq.offset;
            if (seq.literal_length == 0) {
                // Special case when literal length is 0
                idx++;
            }

            if (idx == 1) {
                offset = offset_hist[1];
            } else {
                // If idx == 4 then literal length was 0 and the offset was 3
                offset = idx < 4 ? offset_hist[idx] : offset_hist[1] - 1;

                // If idx == 2 we don't need to modify offset_hist[3]
                if (idx > 2) {
                    offset_hist[3] = offset_hist[2];
                }
                offset_hist[2] = offset_hist[1];
                offset_hist[1] = offset;
            }
        } else {
            offset = seq.offset - 3;

            // Shift back history
            offset_hist[3] = offset_hist[2];
            offset_hist[2] = offset_hist[1];
            offset_hist[1] = offset;
        }

        size_t match_length = seq.match_length;
        if (total_output <= ctx->header.window_size) {
            // In this case offset might go back into the dictionary
            if (offset > total_output + ctx->dict_content_len) {
                // The offset goes beyond even the dictionary
                CORRUPTION();
            }

            if (offset > total_output) {
                const size_t dict_copy =
                    MIN(offset - total_output, match_length);
                const size_t dict_offset =
                    ctx->dict_content_len - (offset - total_output);
                for (size_t i = 0; i < dict_copy; i++) {
                    *streams->dst++ = ctx->dict_content[dict_offset + i];
                }
                match_length -= dict_copy;
            }
        } else if (offset > ctx->header.window_size) {
            CORRUPTION();
        }

        // We must copy byte by byte because the match length might be larger
        // than the offset
        // ex: if the output so far was "abc", a command with offset=3 and
        // match_length=6 would produce "abcabcabc" as the new output
        for (size_t i = 0; i < match_length; i++) {
            *streams->dst = *(streams->dst - offset);
            streams->dst++;
        }

        streams->dst_len -= seq.match_length;
        total_output += seq.match_length;
    }

    if (streams->dst_len < literals_len) {
        OUT_SIZE();
    }
    // Copy any leftover literals
    memcpy(streams->dst, literals, literals_len);
    streams->dst += literals_len;
    streams->dst_len -= literals_len;

    total_output += literals_len;

    ctx->current_total_output = total_output;
}
/******* END SEQUENCE EXECUTION ***********************************************/

/******* OUTPUT SIZE COUNTING *************************************************/
size_t traverse_frame(const frame_header_t *const header, const u8 *src,
                      size_t src_len);

/// Get the decompressed size of an input stream so memory can be allocated in
/// advance.
/// This is more complex than the implementation in the reference
/// implementation, as this API allows for the decompression of multiple
/// concatenated frames.
size_t ZSTD_get_decompressed_size(const void *src, const size_t src_len) {
  const u8 *ip = (const u8 *) src;
  size_t ip_len = src_len;
  size_t dst_size = 0;

  // Each frame header only gives us the size of its frame, so iterate over all
  // frames
  while (ip_len > 0) {
    if (ip_len < 4) {
      INP_SIZE();
    }

    const u32 magic_number = read_bits_LE(ip, 32, 0);

    ip += 4;
    ip_len -= 4;
    if (magic_number >= 0x184D2A50U && magic_number <= 0x184D2A5F) {
        // skippable frame, this has no impact on output size
        if (ip_len < 4) {
            INP_SIZE();
        }
        const size_t frame_size = read_bits_LE(ip, 32, 32);

        if (ip_len < 4 + frame_size) {
            INP_SIZE();
        }

        // skip over frame
        ip += 4 + frame_size;
        ip_len -= 4 + frame_size;
    } else if (magic_number == 0xFD2FB528U) {
        // ZSTD frame
        frame_header_t header;
        parse_frame_header(&header, ip, ip_len);

        if (header.frame_content_size == 0 && !header.single_segment_flag) {
            // Content size not provided, we can't tell
            return -1;
        }

        dst_size += header.frame_content_size;

        // we need to traverse the frame to find when the next one starts
        const size_t traversed = traverse_frame(&header, ip, ip_len);
        ip += traversed;
        ip_len -= traversed;
    } else {
        // not a real frame
        ERROR("Invalid magic number");
    }
  }

  return dst_size;
}

/// Iterate over each block in a frame to find the end of it, to get to the
/// start of the next frame
size_t traverse_frame(const frame_header_t *const header, const u8 *src,
                      size_t src_len) {
    const u8 *const src_beg = src;
    const u8 *const src_end = src + src_len;
    src += header->header_size;
    src_len += header->header_size;

    int last_block = 0;

    do {
        if (src + 3 > src_end) {
            INP_SIZE();
        }
        // Parse the block header
        last_block = src[0] & 1;
        const int block_type = (src[0] >> 1) & 3;
        const size_t block_len = read_bits_LE(src, 21, 3);

        src += 3;
        switch (block_type) {
        case 0: // Raw block, block_len bytes
            if (src + block_len > src_end) {
                INP_SIZE();
            }
            src += block_len;
            break;
        case 1: // RLE block, 1 byte
            if (src + 1 > src_end) {
                INP_SIZE();
            }
            src++;
            break;
        case 2: // Compressed block, compressed size is block_len
            if (src + block_len > src_end) {
                INP_SIZE();
            }
            src += block_len;
            break;
        case 3:
            // Reserved block type
            CORRUPTION();
            break;
        }
    } while (!last_block);

    if (header->content_checksum_flag) {
        if (src + 4 > src_end) {
            INP_SIZE();
        }
        src += 4;
    }

    return src - src_beg;
}

/******* END OUTPUT SIZE COUNTING *********************************************/

/******* DICTIONARY PARSING ***************************************************/
static void init_raw_content_dict(dictionary_t *const dict, const u8 *const src,
                                  const size_t src_len);

static void parse_dictionary(dictionary_t *const dict, const u8 *src,
                             size_t src_len) {
    memset(dict, 0, sizeof(dictionary_t));
    if (src_len < 8) {
        INP_SIZE();
    }
    const u32 magic_number = read_bits_LE(src, 32, 0);
    if (magic_number != 0xEC30A437) {
        // raw content dict
        init_raw_content_dict(dict, src, src_len);
        return;
    }
    dict->dictionary_id = read_bits_LE(src, 32, 32);

    src += 8;
    src_len -= 8;

    // Parse the provided entropy tables in order
    {
        const size_t read =
                decode_huf_table(src, src_len, &dict->literals_dtable);
        src += read;
        src_len -= read;
    }
    {
        const size_t read = decode_seq_table(src, src_len, &dict->of_dtable,
                                             seq_offset, seq_fse);
        src += read;
        src_len -= read;
    }
    {
        const size_t read = decode_seq_table(src, src_len, &dict->ml_dtable,
                                             seq_match_length, seq_fse);
        src += read;
        src_len -= read;
    }
    {
        const size_t read = decode_seq_table(src, src_len, &dict->ll_dtable,
                                             seq_literal_length, seq_fse);
        src += read;
        src_len -= read;
    }

    if (src_len < 12) {
        INP_SIZE();
    }
    // Read in the previous offset history
    dict->previous_offsets[1] = read_bits_LE(src, 32, 0);
    dict->previous_offsets[2] = read_bits_LE(src, 32, 32);
    dict->previous_offsets[3] = read_bits_LE(src, 32, 64);

    src += 12;
    src_len -= 12;

    // Ensure the provided offsets aren't too large
    for (int i = 1; i <= 3; i++) {
        if (dict->previous_offsets[i] > src_len) {
            ERROR("Dictionary corrupted");
        }
    }
    // The rest is the content
    dict->content = malloc(src_len);
    if (!dict->content) {
        BAD_ALLOC();
    }

    dict->content_size = src_len;
    memcpy(dict->content, src, src_len);
}

/// If parse_dictionary is given a raw content dictionary, it delegates here
static void init_raw_content_dict(dictionary_t *const dict, const u8 *const src,
                                  const size_t src_len) {
    dict->dictionary_id = 0;
    // Copy in the content
    dict->content = malloc(src_len);
    if (!dict->content) {
        BAD_ALLOC();
    }

    dict->content_size = src_len;
    memcpy(dict->content, src, src_len);
}

/// Free an allocated dictionary
static void free_dictionary(dictionary_t *const dict) {
    HUF_free_dtable(&dict->literals_dtable);
    FSE_free_dtable(&dict->ll_dtable);
    FSE_free_dtable(&dict->of_dtable);
    FSE_free_dtable(&dict->ml_dtable);

    free(dict->content);

    memset(dict, 0, sizeof(dictionary_t));
}
/******* END DICTIONARY PARSING ***********************************************/

/******* BITSTREAM OPERATIONS *************************************************/
/// Read `num` bits (up to 64) from `src + offset`, where `offset` is in bits
static inline u64 read_bits_LE(const u8 *src, const int num,
                               const size_t offset) {
    if (num > 64) {
        return -1;
    }

    // Skip over bytes that aren't in range
    src += offset / 8;
    size_t bit_offset = offset % 8;
    u64 res = 0;

    int shift = 0;
    int left = num;
    while (left > 0) {
        u64 mask = left >= 8 ? 0xff : (((u64)1 << left) - 1);
        // Dead the next byte, shift it to account for the offset, and then mask
        // out the top part if we don't need all the bits
        res += (((u64)*src++ >> bit_offset) & mask) << shift;
        shift += 8 - bit_offset;
        left -= 8 - bit_offset;
        bit_offset = 0;
    }

    return res;
}

/// Read bits from the end of a HUF or FSE bitstream.  `offset` is in bits, so
/// it updates `offset` to `offset - bits`, and then reads `bits` bits from
/// `src + offset`.  If the offset becomes negative, the extra bits at the
/// bottom are filled in with `0` bits instead of reading from before `src`.
static inline u64 STREAM_read_bits(const u8 *const src, const int bits,
                                   i64 *const offset) {
    *offset = *offset - bits;
    size_t actual_off = *offset;
    size_t actual_bits = bits;
    // Don't actually read bits from before the start of src, so if `*offset <
    // 0` fix actual_off and actual_bits to reflect the quantity to read
    if (*offset < 0) {
        actual_bits += *offset;
        actual_off = 0;
    }
    u64 res = read_bits_LE(src, actual_bits, actual_off);

    if (*offset < 0) {
        // Fill in the bottom "overflowed" bits with 0's
        res = -*offset >= 64 ? 0 : (res << -*offset);
    }
    return res;
}
/******* END BITSTREAM OPERATIONS *********************************************/

/******* BIT COUNTING OPERATIONS **********************************************/
/// Returns `x`, where `2^x` is the largest power of 2 less than or equal to
/// `num`, or `-1` if `num == 0`.
static inline int log2inf(const u64 num) {
    for (int i = 63; i >= 0; i--) {
        if (((u64)1 << i) <= num) {
            return i;
        }
    }
    return -1;
}
/******* END BIT COUNTING OPERATIONS ******************************************/

/******* HUFFMAN PRIMITIVES ***************************************************/
static inline u8 HUF_decode_symbol(const HUF_dtable *const dtable,
                                   u16 *const state, const u8 *const src,
                                   i64 *const offset) {
    // Look up the symbol and number of bits to read
    const u8 symb = dtable->symbols[*state];
    const u8 bits = dtable->num_bits[*state];
    const u16 rest = STREAM_read_bits(src, bits, offset);
    // Shift `bits` bits out of the state, keeping the low order bits that
    // weren't necessary to determine this symbol.  Then add in the new bits
    // read from the stream.
    *state = ((*state << bits) + rest) & (((u16)1 << dtable->max_bits) - 1);

    return symb;
}

static inline void HUF_init_state(const HUF_dtable *const dtable,
                                  u16 *const state, const u8 *const src,
                                  i64 *const offset) {
    // Read in a full `dtable->max_bits` bits to initialize the state
    const u8 bits = dtable->max_bits;
    *state = STREAM_read_bits(src, bits, offset);
}

static size_t HUF_decompress_1stream(const HUF_dtable *const dtable, u8 *dst,
                                     const size_t dst_len, const u8 *src,
                                     size_t src_len) {
    const u8 *const dst_max = dst + dst_len;
    const u8 *const odst = dst;

    // To maintain similarity with FSE, start from the end
    // Find the last 1 bit
    const int padding = 8 - log2inf(src[src_len - 1]);

    i64 offset = src_len * 8 - padding;
    u16 state;

    HUF_init_state(dtable, &state, src, &offset);

    while (dst < dst_max && offset > -dtable->max_bits) {
        // Iterate over the stream, decoding one symbol at a time
        *dst++ = HUF_decode_symbol(dtable, &state, src, &offset);
    }
    // If we stopped before consuming all the input, we didn't have enough space
    if (dst == dst_max && offset > -dtable->max_bits) {
        OUT_SIZE();
    }

    // When all symbols have been decoded, the final state value shouldn't have
    // any data from the stream, so it should have "read" dtable->max_bits from
    // before the start of `src`
    // Therefore `offset`, the edge to start reading new bits at, should be
    // dtable->max_bits before the start of the stream
    if (offset != -dtable->max_bits) {
        CORRUPTION();
    }

    return dst - odst;
}

static size_t HUF_decompress_4stream(const HUF_dtable *const dtable, u8 *dst,
                                     const size_t dst_len, const u8 *const src,
                                     const size_t src_len) {
    if (src_len < 6) {
        INP_SIZE();
    }

    const u8 *const src1 = src + 6;
    const u8 *const src2 = src1 + read_bits_LE(src, 16, 0);
    const u8 *const src3 = src2 + read_bits_LE(src, 16, 16);
    const u8 *const src4 = src3 + read_bits_LE(src, 16, 32);
    const u8 *const src_end = src + src_len;

    // We can't test with all 4 sizes because the 4th size is a function of the
    // other 3 and the provided length
    if (src4 - src >= src_len) {
        INP_SIZE();
    }

    const size_t segment_size = (dst_len + 3) / 4;
    u8 *const dst1 = dst;
    u8 *const dst2 = dst1 + segment_size;
    u8 *const dst3 = dst2 + segment_size;
    u8 *const dst4 = dst3 + segment_size;
    u8 *const dst_end = dst + dst_len;

    size_t total_out = 0;

    // Decode each stream independently for simplicity
    // If we wanted to we could decode all 4 at the same time for speed,
    // utilizing more execution units
    total_out += HUF_decompress_1stream(dtable, dst1, segment_size, src1,
                                        src2 - src1);
    total_out += HUF_decompress_1stream(dtable, dst2, segment_size, src2,
                                        src3 - src2);
    total_out += HUF_decompress_1stream(dtable, dst3, segment_size, src3,
                                        src4 - src3);
    total_out += HUF_decompress_1stream(dtable, dst4, dst_end - dst4, src4,
                                        src_end - src4);

    return total_out;
}

static void HUF_init_dtable(HUF_dtable *const table, const u8 *const bits,
                            const int num_symbs) {
    memset(table, 0, sizeof(HUF_dtable));
    if (num_symbs > HUF_MAX_SYMBS) {
        ERROR("Too many symbols for Huffman");
    }

    u8 max_bits = 0;
    u16 rank_count[HUF_MAX_BITS + 1];
    memset(rank_count, 0, sizeof(rank_count));

    // Count the number of symbols for each number of bits, and determine the
    // depth of the tree
    for (int i = 0; i < num_symbs; i++) {
        if (bits[i] > HUF_MAX_BITS) {
            ERROR("Huffman table depth too large");
        }
        max_bits = MAX(max_bits, bits[i]);
        rank_count[bits[i]]++;
    }

    const size_t table_size = 1 << max_bits;
    table->max_bits = max_bits;
    table->symbols = malloc(table_size);
    table->num_bits = malloc(table_size);

    if (!table->symbols || !table->num_bits) {
        free(table->symbols);
        free(table->num_bits);
        BAD_ALLOC();
    }

    u32 rank_idx[HUF_MAX_BITS + 1];
    // Initialize the starting codes for each rank (number of bits)
    rank_idx[max_bits] = 0;
    for (int i = max_bits; i >= 1; i--) {
        rank_idx[i - 1] = rank_idx[i] + rank_count[i] * (1 << (max_bits - i));
        // The entire range takes the same number of bits so we can memset it
        memset(&table->num_bits[rank_idx[i]], i, rank_idx[i - 1] - rank_idx[i]);
    }

    if (rank_idx[0] != table_size) {
        CORRUPTION();
    }

    // Allocate codes and fill in the table
    for (int i = 0; i < num_symbs; i++) {
        if (bits[i] != 0) {
            // Allocate a code for this symbol and set its range in the table
            const u16 code = rank_idx[bits[i]];
            // Since the code doesn't care about the bottom `max_bits - bits[i]`
            // bits of state, it gets a range that spans all possible values of
            // the lower bits
            const u16 len = 1 << (max_bits - bits[i]);
            memset(&table->symbols[code], i, len);
            rank_idx[bits[i]] += len;
        }
    }
}

static void HUF_init_dtable_usingweights(HUF_dtable *const table,
                                         const u8 *const weights,
                                         const int num_symbs) {
    // +1 because the last weight is not transmitted in the header
    if (num_symbs + 1 > HUF_MAX_SYMBS) {
        ERROR("Too many symbols for Huffman");
    }

    u8 bits[HUF_MAX_SYMBS];

    u64 weight_sum = 0;
    for (int i = 0; i < num_symbs; i++) {
        weight_sum += weights[i] > 0 ? (u64)1 << (weights[i] - 1) : 0;
    }

    // Find the first power of 2 larger than the sum
    const int max_bits = log2inf(weight_sum) + 1;
    const u64 left_over = ((u64)1 << max_bits) - weight_sum;
    // If the left over isn't a power of 2, the weights are invalid
    if (left_over & (left_over - 1)) {
        CORRUPTION();
    }

    // left_over is used to find the last weight as it's not transmitted
    // by inverting 2^(weight - 1) we can determine the value of last_weight
    const int last_weight = log2inf(left_over) + 1;

    for (int i = 0; i < num_symbs; i++) {
        bits[i] = weights[i] > 0 ? (max_bits + 1 - weights[i]) : 0;
    }
    bits[num_symbs] =
        max_bits + 1 - last_weight; // Last weight is always non-zero

    HUF_init_dtable(table, bits, num_symbs + 1);
}

static void HUF_free_dtable(HUF_dtable *const dtable) {
    free(dtable->symbols);
    free(dtable->num_bits);
    memset(dtable, 0, sizeof(HUF_dtable));
}

static void HUF_copy_dtable(HUF_dtable *const dst,
                            const HUF_dtable *const src) {
    if (src->max_bits == 0) {
        memset(dst, 0, sizeof(HUF_dtable));
        return;
    }

    const size_t size = (size_t)1 << src->max_bits;
    dst->max_bits = src->max_bits;

    dst->symbols = malloc(size);
    dst->num_bits = malloc(size);
    if (!dst->symbols || !dst->num_bits) {
        BAD_ALLOC();
    }

    memcpy(dst->symbols, src->symbols, size);
    memcpy(dst->num_bits, src->num_bits, size);
}
/******* END HUFFMAN PRIMITIVES ***********************************************/

/******* FSE PRIMITIVES *******************************************************/
/// Allow a symbol to be decoded without updating state
static inline u8 FSE_peek_symbol(const FSE_dtable *const dtable,
                                 const u16 state) {
    return dtable->symbols[state];
}

/// Consumes bits from the input and uses the current state to determine the
/// next state
static inline void FSE_update_state(const FSE_dtable *const dtable,
                                    u16 *const state, const u8 *const src,
                                    i64 *const offset) {
    const u8 bits = dtable->num_bits[*state];
    const u16 rest = STREAM_read_bits(src, bits, offset);
    *state = dtable->new_state_base[*state] + rest;
}

/// Decodes a single FSE symbol and updates the offset
static inline u8 FSE_decode_symbol(const FSE_dtable *const dtable,
                                   u16 *const state, const u8 *const src,
                                   i64 *const offset) {
    const u8 symb = FSE_peek_symbol(dtable, *state);
    FSE_update_state(dtable, state, src, offset);
    return symb;
}

static inline void FSE_init_state(const FSE_dtable *const dtable,
                                  u16 *const state, const u8 *const src,
                                  i64 *const offset) {
    // Read in a full `accuracy_log` bits to initialize the state
    const u8 bits = dtable->accuracy_log;
    *state = STREAM_read_bits(src, bits, offset);
}

static size_t FSE_decompress_interleaved2(const FSE_dtable *const dtable,
                                          u8 *dst, const size_t dst_len,
                                          const u8 *const src,
                                          const size_t src_len) {
    if (src_len == 0) {
        INP_SIZE();
    }

    const u8 *const dst_max = dst + dst_len;
    const u8 *const odst = dst;

    // Find the last 1 bit
    const int padding = 8 - log2inf(src[src_len - 1]);

    i64 offset = src_len * 8 - padding;

    // The end of the stream contains the 2 states, in this order
    u16 state1, state2;
    FSE_init_state(dtable, &state1, src, &offset);
    FSE_init_state(dtable, &state2, src, &offset);

    // Decode until we overflow the stream
    // Since we decode in reverse order, overflowing the stream is offset going
    // negative
    while (1) {
        if (dst > dst_max - 2) {
            OUT_SIZE();
        }
        *dst++ = FSE_decode_symbol(dtable, &state1, src, &offset);
        if (offset < 0) {
            // There's still a symbol to decode in state2
            *dst++ = FSE_peek_symbol(dtable, state2);
            break;
        }

        if (dst > dst_max - 2) {
            OUT_SIZE();
        }
        *dst++ = FSE_decode_symbol(dtable, &state2, src, &offset);
        if (offset < 0) {
            // There's still a symbol to decode in state1
            *dst++ = FSE_peek_symbol(dtable, state1);
            break;
        }
    }

    // Number of symbols read
    return dst - odst;
}

static void FSE_init_dtable(FSE_dtable *const dtable,
                            const i16 *const norm_freqs, const int num_symbs,
                            const int accuracy_log) {
    if (accuracy_log > FSE_MAX_ACCURACY_LOG) {
        ERROR("FSE accuracy too large");
    }
    if (num_symbs > FSE_MAX_SYMBS) {
        ERROR("Too many symbols for FSE");
    }

    dtable->accuracy_log = accuracy_log;

    const size_t size = (size_t)1 << accuracy_log;
    dtable->symbols = malloc(size * sizeof(u8));
    dtable->num_bits = malloc(size * sizeof(u8));
    dtable->new_state_base = malloc(size * sizeof(u16));

    if (!dtable->symbols || !dtable->num_bits || !dtable->new_state_base) {
        BAD_ALLOC();
    }

    // Used to determine how many bits need to be read for each state,
    // and where the destination range should start
    // Needs to be u16 because max value is 2 * max number of symbols,
    // which can be larger than a byte can store
    u16 state_desc[FSE_MAX_SYMBS];

    int high_threshold = size;
    for (int s = 0; s < num_symbs; s++) {
        // Scan for low probability symbols to put at the top
        if (norm_freqs[s] == -1) {
            dtable->symbols[--high_threshold] = s;
            state_desc[s] = 1;
        }
    }

    // Place the rest in the table
    const u16 step = (size >> 1) + (size >> 3) + 3;
    const u16 mask = size - 1;
    u16 pos = 0;
    for (int s = 0; s < num_symbs; s++) {
        if (norm_freqs[s] <= 0) {
            continue;
        }

        state_desc[s] = norm_freqs[s];

        for (int i = 0; i < norm_freqs[s]; i++) {
            // Give `norm_freqs[s]` states to symbol s
            dtable->symbols[pos] = s;
            do {
                pos = (pos + step) & mask;
            } while (pos >=
                     high_threshold); // Make sure we don't occupy a spot taken
                                      // by the low prob symbols
            // Note: no other collision checking is necessary as `step` is
            // coprime to `size`, so the cycle will visit each position exactly
            // once
        }
    }
    if (pos != 0) {
        CORRUPTION();
    }

    // Now we can fill baseline and num bits
    for (int i = 0; i < size; i++) {
        u8 symbol = dtable->symbols[i];
        u16 next_state_desc = state_desc[symbol]++;
        // Fills in the table appropriately, next_state_desc increases by symbol
        // over time, decreasing number of bits
        dtable->num_bits[i] = (u8)(accuracy_log - log2inf(next_state_desc));
        // Baseline increases until the bit threshold is passed, at which point
        // it resets to 0
        dtable->new_state_base[i] =
            ((u16)next_state_desc << dtable->num_bits[i]) - size;
    }
}

/// Decode an FSE header as defined in the Zstandard format specification and
/// use the decoded frequencies to initialize a decoding table.
static size_t FSE_decode_header(FSE_dtable *const dtable, const u8 *const src,
                                const size_t src_len,
                                const int max_accuracy_log) {
    if (max_accuracy_log > FSE_MAX_ACCURACY_LOG) {
        ERROR("FSE accuracy too large");
    }
    if (src_len < 1) {
        INP_SIZE();
    }

    const int accuracy_log = 5 + read_bits_LE(src, 4, 0);
    if (accuracy_log > max_accuracy_log) {
        ERROR("FSE accuracy too large");
    }

    // The +1 facilitates the `-1` probabilities
    i32 remaining = (1 << accuracy_log) + 1;
    i16 frequencies[FSE_MAX_SYMBS];

    int symb = 0;
    // Offset of 4 because 4 bits were already read in for accuracy
    size_t offset = 4;
    while (remaining > 1 && symb < FSE_MAX_SYMBS) {
        // Log of the number of possible values we could read
        int bits = log2inf(remaining) + 1;

        u16 val = read_bits_LE(src, bits, offset);
        offset += bits;

        // Try to mask out the lower bits to see if it qualifies for the "small
        // value" threshold
        const u16 lower_mask = ((u16)1 << (bits - 1)) - 1;
        const u16 threshold = ((u16)1 << bits) - 1 - remaining;

        if ((val & lower_mask) < threshold) {
            offset--;
            val = val & lower_mask;
        } else if (val > lower_mask) {
            val = val - threshold;
        }

        const i16 proba = (i16)val - 1;
        // A value of -1 is possible, and has special meaning
        remaining -= proba < 0 ? -proba : proba;

        frequencies[symb] = proba;
        symb++;

        // Handle the special probability = 0 case
        if (proba == 0) {
            // Read the next two bits to see how many more 0s
            int repeat = read_bits_LE(src, 2, offset);
            offset += 2;

            while (1) {
                for (int i = 0; i < repeat && symb < FSE_MAX_SYMBS; i++) {
                    frequencies[symb++] = 0;
                }
                if (repeat == 3) {
                    repeat = read_bits_LE(src, 2, offset);
                    offset += 2;
                } else {
                    break;
                }
            }
        }
    }

    if (remaining != 1 || symb >= FSE_MAX_SYMBS) {
        CORRUPTION();
    }

    // Initialize the decoding table using the determined weights
    FSE_init_dtable(dtable, frequencies, symb, accuracy_log);

    return (offset + 7) / 8;
}

static void FSE_init_dtable_rle(FSE_dtable *const dtable, const u8 symb) {
    dtable->symbols = malloc(sizeof(u8));
    dtable->num_bits = malloc(sizeof(u8));
    dtable->new_state_base = malloc(sizeof(u16));

    if (!dtable->symbols || !dtable->num_bits || !dtable->new_state_base) {
        BAD_ALLOC();
    }

    // This setup will always have a state of 0, always return symbol `symb`,
    // and never consume any bits
    dtable->symbols[0] = symb;
    dtable->num_bits[0] = 0;
    dtable->new_state_base[0] = 0;
    dtable->accuracy_log = 0;
}

static void FSE_free_dtable(FSE_dtable *const dtable) {
    free(dtable->symbols);
    free(dtable->num_bits);
    free(dtable->new_state_base);
    memset(dtable, 0, sizeof(FSE_dtable));
}

static void FSE_copy_dtable(FSE_dtable *const dst, const FSE_dtable *const src) {
    if (src->accuracy_log == 0) {
        memset(dst, 0, sizeof(FSE_dtable));
        return;
    }

    size_t size = (size_t)1 << src->accuracy_log;
    dst->accuracy_log = src->accuracy_log;

    dst->symbols = malloc(size);
    dst->num_bits = malloc(size);
    dst->new_state_base = malloc(size * sizeof(u16));
    if (!dst->symbols || !dst->num_bits || !dst->new_state_base) {
        BAD_ALLOC();
    }

    memcpy(dst->symbols, src->symbols, size);
    memcpy(dst->num_bits, src->num_bits, size);
    memcpy(dst->new_state_base, src->new_state_base, size * sizeof(u16));
}
/******* END FSE PRIMITIVES ***************************************************/

