// SPDX-License-Identifier: GPL-2.0

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/zstd.h>

#include "common/zstd_deps.h"
#include "common/zstd_internal.h"

size_t zstd_compress_bound(size_t src_size)
{
	return ZSTD_compressBound(src_size);
}
EXPORT_SYMBOL(zstd_compress_bound);

zstd_parameters zstd_get_params(int level,
	unsigned long long estimated_src_size)
{
	return ZSTD_getParams(level, estimated_src_size, 0);
}
EXPORT_SYMBOL(zstd_get_params);

size_t zstd_cctx_workspace_bound(const zstd_compression_parameters *cparams)
{
	return ZSTD_estimateCCtxSize_usingCParams(*cparams);
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
	const void *src, size_t src_size, const zstd_parameters *parameters)
{
	return ZSTD_compress_advanced(cctx, dst, dst_capacity, src, src_size, NULL, 0, *parameters);
}
EXPORT_SYMBOL(zstd_compress_cctx);

size_t zstd_cstream_workspace_bound(const zstd_compression_parameters *cparams)
{
	return ZSTD_estimateCStreamSize_usingCParams(*cparams);
}
EXPORT_SYMBOL(zstd_cstream_workspace_bound);

zstd_cstream *zstd_init_cstream(const zstd_parameters *parameters,
	unsigned long long pledged_src_size, void *workspace, size_t workspace_size)
{
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

	ret = ZSTD_initCStream_advanced(cstream, NULL, 0, *parameters, pledged_src_size);
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

size_t zstd_compress_stream(zstd_cstream *cstream, zstd_out_buffer *output,
	zstd_in_buffer *input)
{
	return ZSTD_compressStream(cstream, output, input);
}
EXPORT_SYMBOL(zstd_compress_stream);

size_t zstd_flush_stream(zstd_cstream *cstream, zstd_out_buffer *output)
{
	return ZSTD_flushStream(cstream, output);
}
EXPORT_SYMBOL(zstd_flush_stream);

size_t zstd_end_stream(zstd_cstream *cstream, zstd_out_buffer *output)
{
	return ZSTD_endStream(cstream, output);
}
EXPORT_SYMBOL(zstd_end_stream);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Zstd Compressor");
