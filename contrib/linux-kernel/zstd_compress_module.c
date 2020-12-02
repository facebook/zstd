// SPDX-License-Identifier: GPL-2.0

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/zstd.h>

#include "zstd.h"
#include "common/zstd_deps.h"
#include "common/zstd_internal.h"

static void zstd_check_structs(void) {
	/* Check that the structs have the same size. */
	ZSTD_STATIC_ASSERT(sizeof(ZSTD_parameters) ==
		sizeof(struct zstd_parameters));
	ZSTD_STATIC_ASSERT(sizeof(ZSTD_compressionParameters) ==
		sizeof(struct zstd_compression_parameters));
	ZSTD_STATIC_ASSERT(sizeof(ZSTD_frameParameters) ==
		sizeof(struct zstd_frame_parameters));
	/* Zstd guarantees that the layout of the structs never change. Verify it. */
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_parameters, cParams) ==
		offsetof(struct zstd_parameters, cparams));
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_parameters, fParams) ==
		offsetof(struct zstd_parameters, fparams));
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_compressionParameters, windowLog) ==
		offsetof(struct zstd_compression_parameters, window_log));
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_compressionParameters, chainLog) ==
		offsetof(struct zstd_compression_parameters, chain_log));
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_compressionParameters, hashLog) ==
		offsetof(struct zstd_compression_parameters, hash_log));
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_compressionParameters, searchLog) ==
		offsetof(struct zstd_compression_parameters, search_log));
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_compressionParameters, minMatch) ==
		offsetof(struct zstd_compression_parameters, search_length));
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_compressionParameters, targetLength) ==
		offsetof(struct zstd_compression_parameters, target_length));
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_compressionParameters, strategy) ==
		offsetof(struct zstd_compression_parameters, strategy));
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_frameParameters, contentSizeFlag) ==
		offsetof(struct zstd_frame_parameters, content_size_flag));
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_frameParameters, checksumFlag) ==
		offsetof(struct zstd_frame_parameters, checksum_flag));
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_frameParameters, noDictIDFlag) ==
		offsetof(struct zstd_frame_parameters, no_dict_id_flag));
	/* Check that the strategies are the same. This can change. */
	ZSTD_STATIC_ASSERT((int)ZSTD_fast == (int)zstd_fast);
	ZSTD_STATIC_ASSERT((int)ZSTD_dfast == (int)zstd_dfast);
	ZSTD_STATIC_ASSERT((int)ZSTD_greedy == (int)zstd_greedy);
	ZSTD_STATIC_ASSERT((int)ZSTD_lazy == (int)zstd_lazy);
	ZSTD_STATIC_ASSERT((int)ZSTD_lazy2 == (int)zstd_lazy2);
	ZSTD_STATIC_ASSERT((int)ZSTD_btlazy2 == (int)zstd_btlazy2);
	ZSTD_STATIC_ASSERT((int)ZSTD_btopt == (int)zstd_btopt);
	ZSTD_STATIC_ASSERT((int)ZSTD_btultra == (int)zstd_btultra);
	ZSTD_STATIC_ASSERT((int)ZSTD_btultra2 == (int)zstd_btultra2);
	/* Check input buffer */
	ZSTD_STATIC_ASSERT(sizeof(ZSTD_inBuffer) == sizeof(struct zstd_in_buffer));
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_inBuffer, src) ==
		offsetof(struct zstd_in_buffer, src));
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_inBuffer, size) ==
		offsetof(struct zstd_in_buffer, size));
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_inBuffer, pos) ==
		offsetof(struct zstd_in_buffer, pos));
	/* Check output buffer */
	ZSTD_STATIC_ASSERT(sizeof(ZSTD_outBuffer) ==
		sizeof(struct zstd_out_buffer));
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_outBuffer, dst) ==
		offsetof(struct zstd_out_buffer, dst));
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_outBuffer, size) ==
		offsetof(struct zstd_out_buffer, size));
	ZSTD_STATIC_ASSERT(offsetof(ZSTD_outBuffer, pos) ==
		offsetof(struct zstd_out_buffer, pos));
}

size_t zstd_compress_bound(size_t src_size)
{
	return ZSTD_compressBound(src_size);
}
EXPORT_SYMBOL(zstd_compress_bound);

struct zstd_parameters zstd_get_params(int level,
	unsigned long long estimated_src_size)
{
	const ZSTD_parameters params = ZSTD_getParams(level, estimated_src_size, 0);
	struct zstd_parameters out;

	/* no-op */
	zstd_check_structs();
	ZSTD_memcpy(&out, &params, sizeof(out));
	return out;
}
EXPORT_SYMBOL(zstd_get_params);

size_t zstd_cctx_workspace_bound(
	const struct zstd_compression_parameters *cparams)
{
	ZSTD_compressionParameters p;

	ZSTD_memcpy(&p, cparams, sizeof(p));
	return ZSTD_estimateCCtxSize_usingCParams(p);
}
EXPORT_SYMBOL(zstd_cctx_workspace_bound);

zstd_cctx *zstd_init_cctx(void *workspace, size_t workspace_size)
{
	if (workspace == NULL)
		return NULL;
	return ZSTD_initStaticCCtx(workspace, workspace_size);
}
EXPORT_SYMBOL(zstd_init_cctx);

size_t zstd_compress_cctx(zstd_cctx *cctx, void *dst, size_t dst_capacity,
	const void *src, size_t src_size, const struct zstd_parameters *parameters)
{
	ZSTD_parameters p;

	ZSTD_memcpy(&p, parameters, sizeof(p));
	return ZSTD_compress_advanced(cctx, dst, dst_capacity, src, src_size, NULL, 0, p);
}
EXPORT_SYMBOL(zstd_compress_cctx);

size_t zstd_cstream_workspace_bound(
	const struct zstd_compression_parameters *cparams)
{
	ZSTD_compressionParameters p;

	ZSTD_memcpy(&p, cparams, sizeof(p));
	return ZSTD_estimateCStreamSize_usingCParams(p);
}
EXPORT_SYMBOL(zstd_cstream_workspace_bound);

zstd_cstream *zstd_init_cstream(const struct zstd_parameters *parameters,
	unsigned long long pledged_src_size, void *workspace, size_t workspace_size)
{
	ZSTD_parameters p;
	zstd_cstream *cstream;
	size_t ret;

	if (workspace == NULL)
		return NULL;

	cstream = ZSTD_initStaticCStream(workspace, workspace_size);
	if (cstream == NULL)
		return NULL;

	/* 0 means unknown in linux zstd API but means 0 in new zstd API */
	if (pledged_src_size == 0)
		pledged_src_size = ZSTD_CONTENTSIZE_UNKNOWN;

	ZSTD_memcpy(&p, parameters, sizeof(p));
	ret = ZSTD_initCStream_advanced(cstream, NULL, 0, p, pledged_src_size);
	if (ZSTD_isError(ret))
		return NULL;

	return cstream;
}
EXPORT_SYMBOL(zstd_init_cstream);

size_t zstd_reset_cstream(zstd_cstream *cstream,
	unsigned long long pledged_src_size)
{
	return ZSTD_resetCStream(cstream, pledged_src_size);
}
EXPORT_SYMBOL(zstd_reset_cstream);

size_t zstd_compress_stream(zstd_cstream *cstream,
	struct zstd_out_buffer *output, struct zstd_in_buffer *input)
{
	ZSTD_outBuffer o;
	ZSTD_inBuffer i;
	size_t ret;

	ZSTD_memcpy(&o, output, sizeof(o));
	ZSTD_memcpy(&i, input, sizeof(i));
	ret = ZSTD_compressStream(cstream, &o, &i);
	ZSTD_memcpy(output, &o, sizeof(o));
	ZSTD_memcpy(input, &i, sizeof(i));
	return ret;
}
EXPORT_SYMBOL(zstd_compress_stream);

size_t zstd_flush_stream(zstd_cstream *cstream, struct zstd_out_buffer *output)
{
	ZSTD_outBuffer o;
	size_t ret;

	ZSTD_memcpy(&o, output, sizeof(o));
	ret = ZSTD_flushStream(cstream, &o);
	ZSTD_memcpy(output, &o, sizeof(o));
	return ret;
}
EXPORT_SYMBOL(zstd_flush_stream);

size_t zstd_end_stream(zstd_cstream *cstream, struct zstd_out_buffer *output)
{
	ZSTD_outBuffer o;
	size_t ret;

	ZSTD_memcpy(&o, output, sizeof(o));
	ret = ZSTD_endStream(cstream, &o);
	ZSTD_memcpy(output, &o, sizeof(o));
	return ret;
}
EXPORT_SYMBOL(zstd_end_stream);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Zstd Compressor");
