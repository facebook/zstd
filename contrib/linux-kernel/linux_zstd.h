/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of https://github.com/facebook/zstd.
 * An additional grant of patent rights can be found in the PATENTS file in the
 * same directory.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation. This program is dual-licensed; you may select
 * either version 2 of the GNU General Public License ("GPL") or BSD license
 * ("BSD").
 */

#ifndef LINUX_ZSTD_H
#define LINUX_ZSTD_H

/**
 * This is a kernel-style API that wraps the upstream zstd API, which cannot be
 * used directly because the symbols aren't exported. It exposes the minimal
 * functionality which is currently required by users of zstd in the kernel.
 * Expose extra functions from lib/zstd/zstd.h as needed.
 */

/* ======   Dependency   ====== */
#include <linux/types.h>

/* ======   Helper Functions   ====== */
/**
 * zstd_compress_bound() - maximum compressed size in worst case scenario
 * @src_size: The size of the data to compress.
 *
 * Return:    The maximum compressed size in the worst case scenario.
 */
size_t zstd_compress_bound(size_t src_size);

/**
 * zstd_is_error() - tells if a size_t function result is an error code
 * @code:  The function result to check for error.
 *
 * Return: Non-zero iff the code is an error.
 */
unsigned int zstd_is_error(size_t code);

/**
 * zstd_get_error_code() - translates an error function result to an error code
 * @code:  The function result for which zstd_is_error(code) is true.
 *
 * Return: A unique error code for this error.
 */
int zstd_get_error_code(size_t code);

/**
 * zstd_get_error_name() - translates an error function result to a string
 * @code:  The function result for which zstd_is_error(code) is true.
 *
 * Return: An error string corresponding to the error code.
 */
const char *zstd_get_error_name(size_t code);

/* ======   Parameter Selection   ====== */

/**
 * enum zstd_strategy - zstd compression search strategy
 *
 * From faster to stronger.
 */
enum zstd_strategy {
	zstd_fast = 1,
	zstd_dfast = 2,
	zstd_greedy = 3,
	zstd_lazy = 4,
	zstd_lazy2 = 5,
	zstd_btlazy2 = 6,
	zstd_btopt = 7,
	zstd_btultra = 8,
	zstd_btultra2 = 9
};

/**
 * struct zstd_compression_parameters - zstd compression parameters
 * @window_log:    Log of the largest match distance. Larger means more
 *                 compression, and more memory needed during decompression.
 * @chain_log:     Fully searched segment. Larger means more compression,
 *                 slower, and more memory (useless for fast).
 * @hash_log:      Dispatch table. Larger means more compression,
 *                 slower, and more memory.
 * @search_log:    Number of searches. Larger means more compression and slower.
 * @search_length: Match length searched. Larger means faster decompression,
 *                 sometimes less compression.
 * @target_length: Acceptable match size for optimal parser (only). Larger means
 *                 more compression, and slower.
 * @strategy:      The zstd compression strategy.
 */
struct zstd_compression_parameters {
	unsigned int window_log;
	unsigned int chain_log;
	unsigned int hash_log;
	unsigned int search_log;
	unsigned int search_length;
	unsigned int target_length;
	enum zstd_strategy strategy;
};

/**
 * struct zstd_frame_parameters - zstd frame parameters
 * @content_size_flag: Controls whether content size will be present in the
 *                     frame header (when known).
 * @checksum_flag:     Controls whether a 32-bit checksum is generated at the
 *                     end of the frame for error detection.
 * @no_dict_id_flag:   Controls whether dictID will be saved into the frame
 *                     header when using dictionary compression.
 *
 * The default value is all fields set to 0.
 */
struct zstd_frame_parameters {
	unsigned int content_size_flag;
	unsigned int checksum_flag;
	unsigned int no_dict_id_flag;
};

/**
 * struct zstd_parameters - zstd parameters
 * @cparams: The compression parameters.
 * @fparams: The frame parameters.
 */
struct zstd_parameters {
	struct zstd_compression_parameters cparams;
	struct zstd_frame_parameters fparams;
};

/**
 * zstd_get_params() - returns zstd_parameters for selected level
 * @level:              The compression level
 * @estimated_src_size: The estimated source size to compress or 0
 *                      if unknown.
 *
 * Return:              The selected zstd_parameters.
 */
struct zstd_parameters zstd_get_params(int level,
	unsigned long long estimated_src_size);

/* ======   Single-pass Compression   ====== */

typedef struct ZSTD_CCtx_s zstd_cctx;

/**
 * zstd_cctx_workspace_bound() - max memory needed to initialize a zstd_cctx
 * @parameters: The compression parameters to be used.
 *
 * If multiple compression parameters might be used, the caller must call
 * zstd_cctx_workspace_bound() for each set of parameters and use the maximum
 * size.
 *
 * Return:      A lower bound on the size of the workspace that is passed to
 *              zstd_init_cctx().
 */
size_t zstd_cctx_workspace_bound(
	const struct zstd_compression_parameters *parameters);

/**
 * zstd_init_cctx() - initialize a zstd compression context
 * @workspace:      The workspace to emplace the context into. It must outlive
 *                  the returned context.
 * @workspace_size: The size of workspace. Use zstd_cctx_workspace_bound() to
 *                  determine how large the workspace must be.
 *
 * Return:          A zstd compression context or NULL on error.
 */
zstd_cctx *zstd_init_cctx(void *workspace, size_t workspace_size);

/**
 * zstd_compress_cctx() - compress src into dst with the initialized parameters
 * @cctx:         The context. Must have been initialized with zstd_init_cctx().
 * @dst:          The buffer to compress src into.
 * @dst_capacity: The size of the destination buffer. May be any size, but
 *                ZSTD_compressBound(srcSize) is guaranteed to be large enough.
 * @src:          The data to compress.
 * @src_size:     The size of the data to compress.
 * @parameters:   The compression parameters to be used.
 *
 * Return:        The compressed size or an error, which can be checked using
 *                zstd_is_error().
 */
size_t zstd_compress_cctx(zstd_cctx *cctx, void *dst, size_t dst_capacity,
	const void *src, size_t src_size, const struct zstd_parameters *parameters);

/* ======   Single-pass Decompression   ====== */

typedef struct ZSTD_DCtx_s zstd_dctx;

/**
 * zstd_dctx_workspace_bound() - max memory needed to initialize a zstd_dctx
 *
 * Return: A lower bound on the size of the workspace that is passed to
 *         zstd_init_dctx().
 */
size_t zstd_dctx_workspace_bound(void);

/**
 * zstd_init_dctx() - initialize a zstd decompression context
 * @workspace:      The workspace to emplace the context into. It must outlive
 *                  the returned context.
 * @workspace_size: The size of workspace. Use zstd_dctx_workspace_bound() to
 *                  determine how large the workspace must be.
 *
 * Return:          A zstd decompression context or NULL on error.
 */
zstd_dctx *zstd_init_dctx(void *workspace, size_t workspace_size);

/**
 * zstd_decompress_dctx() - decompress zstd compressed src into dst
 * @dctx:         The decompression context.
 * @dst:          The buffer to decompress src into.
 * @dst_capacity: The size of the destination buffer. Must be at least as large
 *                as the decompressed size. If the caller cannot upper bound the
 *                decompressed size, then it's better to use the streaming API.
 * @src:          The zstd compressed data to decompress. Multiple concatenated
 *                frames and skippable frames are allowed.
 * @src_size:     The exact size of the data to decompress.
 *
 * Return:        The decompressed size or an error, which can be checked using
 *                zstd_is_error().
 */
size_t zstd_decompress_dctx(zstd_dctx *dctx, void *dst, size_t dst_capacity,
	const void *src, size_t src_size);

/* ======   Streaming Buffers   ====== */

/**
 * struct zstd_in_buffer - input buffer for streaming
 * @src:  Start of the input buffer.
 * @size: Size of the input buffer.
 * @pos:  Position where reading stopped. Will be updated.
 *        Necessarily 0 <= pos <= size.
 */
struct zstd_in_buffer {
	const void *src;
	size_t size;
	size_t pos;
};

/**
 * struct zstd_out_buffer - output buffer for streaming
 * @dst:  Start of the output buffer.
 * @size: Size of the output buffer.
 * @pos:  Position where writing stopped. Will be updated.
 *        Necessarily 0 <= pos <= size.
 */
struct zstd_out_buffer {
	void *dst;
	size_t size;
	size_t pos;
};

/* ======   Streaming Compression   ====== */

typedef struct ZSTD_CCtx_s zstd_cstream;

/**
 * zstd_cstream_workspace_bound() - memory needed to initialize a zstd_cstream
 * @cparams: The compression parameters to be used for compression.
 *
 * Return:   A lower bound on the size of the workspace that is passed to
 *           zstd_init_cstream().
 */
size_t zstd_cstream_workspace_bound(
	const struct zstd_compression_parameters *cparams);

/**
 * zstd_init_cstream() - initialize a zstd streaming compression context
 * @parameters        The zstd parameters to use for compression.
 * @pledged_src_size: If params.fParams.contentSizeFlag == 1 then the caller
 *                    must pass the source size (zero means empty source).
 *                    Otherwise, the caller may optionally pass the source
 *                    size, or zero if unknown.
 * @workspace:        The workspace to emplace the context into. It must outlive
 *                    the returned context.
 * @workspace_size:   The size of workspace.
 *                    Use zstd_cstream_workspace_bound(params->cparams) to
 *                    determine how large the workspace must be.
 *
 * Return:            The zstd streaming compression context or NULL on error.
 */
zstd_cstream *zstd_init_cstream(const struct zstd_parameters *parameters,
	unsigned long long pledged_src_size, void *workspace, size_t workspace_size);

/**
 * zstd_reset_cstream() - reset the context using parameters from creation
 * @cstream:          The zstd streaming compression context to reset.
 * @pledged_src_size: Optionally the source size, or zero if unknown.
 *
 * Resets the context using the parameters from creation. Skips dictionary
 * loading, since it can be reused. If `pledged_src_size` is non-zero the frame
 * content size is always written into the frame header.
 *
 * Return:            Zero or an error, which can be checked using
 *                    zstd_is_error().
 */
size_t zstd_reset_cstream(zstd_cstream *cstream,
	unsigned long long pledged_src_size);

/**
 * zstd_compress_stream() - streaming compress some of input into output
 * @cstream: The zstd streaming compression context.
 * @output:  Destination buffer. `output->pos` is updated to indicate how much
 *           compressed data was written.
 * @input:   Source buffer. `input->pos` is updated to indicate how much data
 *           was read. Note that it may not consume the entire input, in which
 *           case `input->pos < input->size`, and it's up to the caller to
 *           present remaining data again.
 *
 * The `input` and `output` buffers may be any size. Guaranteed to make some
 * forward progress if `input` and `output` are not empty.
 *
 * Return:   A hint for the number of bytes to use as the input for the next
 *           function call or an error, which can be checked using
 *           zstd_is_error().
 */
size_t zstd_compress_stream(zstd_cstream *cstream,
	struct zstd_out_buffer *output, struct zstd_in_buffer *input);

/**
 * zstd_flush_stream() - flush internal buffers into output
 * @cstream: The zstd streaming compression context.
 * @output:  Destination buffer. `output->pos` is updated to indicate how much
 *           compressed data was written.
 *
 * zstd_flush_stream() must be called until it returns 0, meaning all the data
 * has been flushed. Since zstd_flush_stream() causes a block to be ended,
 * calling it too often will degrade the compression ratio.
 *
 * Return:   The number of bytes still present within internal buffers or an
 *           error, which can be checked using zstd_is_error().
 */
size_t zstd_flush_stream(zstd_cstream *cstream, struct zstd_out_buffer *output);

/**
 * zstd_end_stream() - flush internal buffers into output and end the frame
 * @cstream: The zstd streaming compression context.
 * @output:  Destination buffer. `output->pos` is updated to indicate how much
 *           compressed data was written.
 *
 * zstd_end_stream() must be called until it returns 0, meaning all the data has
 * been flushed and the frame epilogue has been written.
 *
 * Return:   The number of bytes still present within internal buffers or an
 *           error, which can be checked using zstd_is_error().
 */
size_t zstd_end_stream(zstd_cstream *cstream, struct zstd_out_buffer *output);

/* ======   Streaming Decompression   ====== */

typedef struct ZSTD_DCtx_s zstd_dstream;

/**
 * zstd_dstream_workspace_bound() - memory needed to initialize a zstd_dstream
 * @max_window_size: The maximum window size allowed for compressed frames.
 *
 * Return:           A lower bound on the size of the workspace that is passed
 *                   to zstd_init_dstream().
 */
size_t zstd_dstream_workspace_bound(size_t max_window_size);

/**
 * zstd_init_dstream() - initialize a zstd streaming decompression context
 * @max_window_size: The maximum window size allowed for compressed frames.
 * @workspace:       The workspace to emplace the context into. It must outlive
 *                   the returned context.
 * @workspaceSize:   The size of workspace.
 *                   Use zstd_dstream_workspace_bound(max_window_size) to
 *                   determine how large the workspace must be.
 *
 * Return:           The zstd streaming decompression context.
 */
zstd_dstream *zstd_init_dstream(size_t max_window_size, void *workspace,
	size_t workspace_size);

/**
 * zstd_reset_dstream() - reset the context using parameters from creation
 * @dstream: The zstd streaming decompression context to reset.
 *
 * Resets the context using the parameters from creation. Skips dictionary
 * loading, since it can be reused.
 *
 * Return:   Zero or an error, which can be checked using zstd_is_error().
 */
size_t zstd_reset_dstream(zstd_dstream *dstream);

/**
 * zstd_decompress_stream() - streaming decompress some of input into output
 * @dstream: The zstd streaming decompression context.
 * @output:  Destination buffer. `output.pos` is updated to indicate how much
 *           decompressed data was written.
 * @input:   Source buffer. `input.pos` is updated to indicate how much data was
 *           read. Note that it may not consume the entire input, in which case
 *           `input.pos < input.size`, and it's up to the caller to present
 *           remaining data again.
 *
 * The `input` and `output` buffers may be any size. Guaranteed to make some
 * forward progress if `input` and `output` are not empty.
 * zstd_decompress_stream() will not consume the last byte of the frame until
 * the entire frame is flushed.
 *
 * Return:   Returns 0 iff a frame is completely decoded and fully flushed.
 *           Otherwise returns a hint for the number of bytes to use as the
 *           input for the next function call or an error, which can be checked
 *           using zstd_is_error(). The size hint will never load more than the
 *           frame.
 */
size_t zstd_decompress_stream(zstd_dstream *dstream,
	struct zstd_out_buffer *output, struct zstd_in_buffer *input);

/* ======   Frame Inspection Functions ====== */

/**
 * zstd_find_frame_compressed_size() - returns the size of a compressed frame
 * @src:      Source buffer. It should point to the start of a zstd encoded
 *            frame or a skippable frame.
 * @src_size: The size of the source buffer. It must be at least as large as the
 *            size of the frame.
 *
 * Return:    The compressed size of the frame pointed to by `src` or an error,
 *            which can be check with zstd_is_error().
 *            Suitable to pass to ZSTD_decompress() or similar functions.
 */
size_t zstd_find_frame_compressed_size(const void *src, size_t src_size);

/**
 * struct zstd_frame_params - zstd frame parameters stored in the frame header
 * @frame_content_size: The frame content size, or 0 if not present.
 * @window_size:        The window size, or 0 if the frame is a skippable frame.
 * @dict_id:            The dictionary id, or 0 if not present.
 * @checksum_flag:      Whether a checksum was used.
 */
struct zstd_frame_params {
	unsigned long long frame_content_size;
	unsigned int window_size;
	unsigned int dict_id;
	unsigned int checksum_flag;
};

/**
 * zstd_get_frame_params() - extracts parameters from a zstd or skippable frame
 * @params:   On success the frame parameters are written here.
 * @src:      The source buffer. It must point to a zstd or skippable frame.
 * @src_size: The size of the source buffer.
 *
 * Return:    0 on success. If more data is required it returns how many bytes
 *            must be provided to make forward progress. Otherwise it returns
 *            an error, which can be checked using zstd_is_error().
 */
size_t zstd_get_frame_params(struct zstd_frame_params *params, const void *src,
	size_t src_size);

#endif  /* LINUX_ZSTD_H */
